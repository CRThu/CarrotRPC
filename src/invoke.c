/****************************
 * INVOKE v2 - 调度执行引擎
 * CarrotRPC
 *
 * cmd_scan → queue → cmd_parse → dispatch_find → invoke_call
 *****************************/
#include "invoke.h"
#include "rpclog.h"
#include <string.h>

/* 库内置全局字符串返回共享缓冲区 */
char g_rpc_str_ret_buf[INVOKE_STR_MAX_SIZE];

/*=============================================================
 * 内部：将 cmd_arg_t 转为类型化值，存入 staging buffer
 * p[i] 直接指向数据 (一次解引用读取值)
 *=============================================================*/
/*=============================================================
 * 内部辅助函数
 *=============================================================*/

/**
 * @brief 从 entry 中提取 Token 文本存入本地 buffer (以 \0 结尾，支持 ringbuf 物理回绕)
 * 使用分段 memcpy 实现极致性能：未跨界时 1 次 memcpy，跨界时 2 次 memcpy
 */
static uint16_t copy_entry_token(const cmd_entry_t* entry, uint16_t offset, uint16_t len, char* dst, uint16_t max_dst)
{
    if (dst == NULL || max_dst == 0 || entry == NULL || entry->buf == NULL) return 0;
    if (len >= max_dst) len = max_dst - 1;

    if (entry->buf_len > 0)
    {
        uint16_t start_phys = (uint16_t)((entry->cmd_start + offset) % entry->buf_len);
        uint16_t first_part = entry->buf_len - start_phys;

        if (len <= first_part)
        {
            memcpy(dst, &entry->buf[start_phys], len);
        }
        else
        {
            memcpy(dst, &entry->buf[start_phys], first_part);
            memcpy(dst + first_part, entry->buf, len - first_part);
        }
    }
    else
    {
        memcpy(dst, &entry->buf[entry->cmd_start + offset], len);
    }

    dst[len] = '\0';
    return len;
}

/**
 * @brief 将 cmd_arg_t 转为类型化值，存入 staging buffer
 */
static void stage_arg(const cmd_entry_t* entry, const cmd_arg_t* arg, uint8_t type,
                      uint64_t* val_raw, char (*str_buf)[INVOKE_STR_MAX_SIZE],
                      void** p, uint8_t i)
{
    uint16_t arg_offset = 0;
    if (entry->buf_len > 0)
    {
        uint16_t phys = (uint16_t)(arg->ptr - (const char*)entry->buf);
        arg_offset = (uint16_t)((phys + entry->buf_len - (entry->cmd_start % entry->buf_len)) % entry->buf_len);
    }
    else
    {
        arg_offset = (uint16_t)(arg->ptr - (const char*)&entry->buf[entry->cmd_start]);
    }

    if (type == DS)
    {
        copy_entry_token(entry, arg_offset, arg->len, str_buf[i], sizeof(str_buf[i]));
        p[i] = str_buf[i];
    }
    else if (type == DI || type == DU || type == DF)
    {
        /* 判断数字 Token 是否在物理内存中连续（未跨越末尾） */
        uint16_t phys = (uint16_t)(arg->ptr - (const char*)entry->buf);
        int is_contiguous = (entry->buf_len == 0) || (phys + arg->len <= entry->buf_len);

        if (is_contiguous)
        {
            /* 未跨界：零拷贝直接解析数字，消除 memcpy 开销 */
            if (type == DI)
            {
                val_raw[i] = (uint64_t)typeconv_to_i64(arg->ptr, arg->len);
            }
            else if (type == DU)
            {
                val_raw[i] = typeconv_to_u64(arg->ptr, arg->len);
            }
            else if (type == DF)
            {
                double d = typeconv_to_f64(arg->ptr, arg->len);
                memcpy(&val_raw[i], &d, sizeof(double));
            }
        }
        else
        {
            /* 跨界：退化为拷贝到临时栈 buffer 后解析 */
            char num_str[32];
            copy_entry_token(entry, arg_offset, arg->len, num_str, sizeof(num_str));
            if (type == DI)
            {
                val_raw[i] = (uint64_t)typeconv_to_i64(num_str, arg->len);
            }
            else if (type == DU)
            {
                val_raw[i] = typeconv_to_u64(num_str, arg->len);
            }
            else if (type == DF)
            {
                double d = typeconv_to_f64(num_str, arg->len);
                memcpy(&val_raw[i], &d, sizeof(double));
            }
        }
        p[i] = &val_raw[i];
    }
}

/*=============================================================
 * 内部：根据 ret_type 判断返回值类型
 *=============================================================*/
static invoke_ret_type_t resolve_ret_type(uint8_t ret_type)
{
    switch (ret_type)
    {
    case DI:
    case DU:
        return INVOKERET_I64;
    case DS:
        return INVOKERET_STR;
    case DV:
    default:
        return INVOKERET_NONE;
    }
}

/*=============================================================
 * 公开 API
 *=============================================================*/
dispatch_status_t invoke_call(dispatch_registry_t* reg,
                              const cmd_entry_t* entry, invoke_ret_t* ret)
{
    if (reg == NULL || entry == NULL || entry->buf == NULL || entry->cmd_len == 0)
    {
        rpc_error("Invalid entry.");
        return DISPATCH_ERR_NULL;
    }

    /* 1. 复用 cmd_parse 获取零拷贝切分结构 */
    cmd_args_t args;
    uint8_t parse_res = cmd_parse(entry, &args);
    if (parse_res == 0xFF || args.func_name == NULL || args.func_name_len == 0)
    {
        rpc_error("Invalid function name.");
        return DISPATCH_ERR_NULL;
    }

    /* 2. 查找注册函数（跨界时透明接合函数名，复用 dispatch_find） */
    char name_buf[DISPATCH_FUNC_NAME_MAX];
    const char* func_name = args.func_name;

    if (entry->buf_len > 0)
    {
        uint16_t phys = (uint16_t)(args.func_name - (const char*)entry->buf);
        if (phys + args.func_name_len > entry->buf_len)
        {
            uint16_t func_offset = (uint16_t)((phys + entry->buf_len - (entry->cmd_start % entry->buf_len)) % entry->buf_len);
            copy_entry_token(entry, func_offset, args.func_name_len, name_buf, sizeof(name_buf));
            func_name = name_buf;
        }
    }

    dispatch_func_t* f = dispatch_find(reg, func_name, args.func_name_len);
    if (f == NULL)
    {
        rpc_error("Function not found.");
        return DISPATCH_ERR_NOT_FOUND;
    }

    /* 3. 验证参数数量 */
    if (args.args_count != f->args_count)
    {
        rpc_error("Arg count mismatch: expected %d, got %d.", f->args_count, args.args_count);
        return DISPATCH_ERR_SIG;
    }

    /* 4. staging buffer — 全部在栈上 */
    uint64_t val_raw[DISPATCH_ARGS_MAX_CNT];
    char     str_buf[DISPATCH_ARGS_MAX_CNT][INVOKE_STR_MAX_SIZE];
    void*    p[DISPATCH_ARGS_MAX_CNT];

    memset(val_raw, 0, sizeof(val_raw));
    memset(str_buf, 0, sizeof(str_buf));
    memset(p, 0, sizeof(p));

    for (uint8_t i = 0; i < f->args_count; i++)
    {
        stage_arg(entry, &args.args[i], f->args_type[i], val_raw, str_buf, p, i);
    }

    /* 4. 根据返回值类型选择 delegate 族，按参数数量分发 */
    invoke_ret_type_t ret_kind = resolve_ret_type(f->ret_type);

    switch (ret_kind)
    {
    /* ---- void 返回 ---- */
    case INVOKERET_NONE:
    {
        switch (f->args_count)
        {
        case 0: ((invoke_delegate_a0r0)f->handler)(); break;
        case 1: ((invoke_delegate_a1r0)f->handler)(p[0]); break;
        case 2: ((invoke_delegate_a2r0)f->handler)(p[0], p[1]); break;
        case 3: ((invoke_delegate_a3r0)f->handler)(p[0], p[1], p[2]); break;
        case 4: ((invoke_delegate_a4r0)f->handler)(p[0], p[1], p[2], p[3]); break;
        case 5: ((invoke_delegate_a5r0)f->handler)(p[0], p[1], p[2], p[3], p[4]); break;
        case 6: ((invoke_delegate_a6r0)f->handler)(p[0], p[1], p[2], p[3], p[4], p[5]); break;
        case 7: ((invoke_delegate_a7r0)f->handler)(p[0], p[1], p[2], p[3], p[4], p[5], p[6]); break;
        case 8: ((invoke_delegate_a8r0)f->handler)(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]); break;
        case 9: ((invoke_delegate_a9r0)f->handler)(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]); break;
        default:
            return DISPATCH_ERR_SIG;
        }
        if (ret != NULL) ret->type = INVOKERET_NONE;
        return DISPATCH_OK;
    }

    /* ---- int64 返回 ---- */
    case INVOKERET_I64:
    {
        int64_t r = 0;
        switch (f->args_count)
        {
        case 0: r = ((invoke_delegate_a0r1)f->handler)(); break;
        case 1: r = ((invoke_delegate_a1r1)f->handler)(p[0]); break;
        case 2: r = ((invoke_delegate_a2r1)f->handler)(p[0], p[1]); break;
        case 3: r = ((invoke_delegate_a3r1)f->handler)(p[0], p[1], p[2]); break;
        case 4: r = ((invoke_delegate_a4r1)f->handler)(p[0], p[1], p[2], p[3]); break;
        case 5: r = ((invoke_delegate_a5r1)f->handler)(p[0], p[1], p[2], p[3], p[4]); break;
        case 6: r = ((invoke_delegate_a6r1)f->handler)(p[0], p[1], p[2], p[3], p[4], p[5]); break;
        case 7: r = ((invoke_delegate_a7r1)f->handler)(p[0], p[1], p[2], p[3], p[4], p[5], p[6]); break;
        case 8: r = ((invoke_delegate_a8r1)f->handler)(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]); break;
        case 9: r = ((invoke_delegate_a9r1)f->handler)(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]); break;
        default:
            return DISPATCH_ERR_SIG;
        }
#if RPC_INVOKE_AUTO_RETURN
        rpc_log_i64(RPC_LOG_RETURN, NULL, r);
#endif
        if (ret != NULL)
        {
            ret->type = INVOKERET_I64;
            ret->i64 = r;
        }
        return DISPATCH_OK;
    }

    /* ---- char* 返回 ---- */
    case INVOKERET_STR:
    {
        const char* r = NULL;
        switch (f->args_count)
        {
        case 0: r = ((invoke_delegate_a0rs)f->handler)(); break;
        case 1: r = ((invoke_delegate_a1rs)f->handler)(p[0]); break;
        case 2: r = ((invoke_delegate_a2rs)f->handler)(p[0], p[1]); break;
        case 3: r = ((invoke_delegate_a3rs)f->handler)(p[0], p[1], p[2]); break;
        case 4: r = ((invoke_delegate_a4rs)f->handler)(p[0], p[1], p[2], p[3]); break;
        case 5: r = ((invoke_delegate_a5rs)f->handler)(p[0], p[1], p[2], p[3], p[4]); break;
        case 6: r = ((invoke_delegate_a6rs)f->handler)(p[0], p[1], p[2], p[3], p[4], p[5]); break;
        case 7: r = ((invoke_delegate_a7rs)f->handler)(p[0], p[1], p[2], p[3], p[4], p[5], p[6]); break;
        case 8: r = ((invoke_delegate_a8rs)f->handler)(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]); break;
        case 9: r = ((invoke_delegate_a9rs)f->handler)(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]); break;
        default:
            return DISPATCH_ERR_SIG;
        }
#if RPC_INVOKE_AUTO_RETURN
        rpc_log_str(RPC_LOG_RETURN, NULL, r ? r : "");
#endif
        if (ret != NULL)
        {
            ret->type = INVOKERET_STR;
            if (r != NULL)
            {
                uint16_t copy_len = strlen(r);
                if (copy_len >= INVOKE_STR_MAX_SIZE)
                    copy_len = INVOKE_STR_MAX_SIZE - 1;
                memcpy(ret->str, r, copy_len);
                ret->str[copy_len] = '\0';
            }
            else
            {
                ret->str[0] = '\0';
            }
        }
        return DISPATCH_OK;
    }

    default:
        return DISPATCH_ERR_SIG;
    }
}
