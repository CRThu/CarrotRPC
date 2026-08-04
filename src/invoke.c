/****************************
 * INVOKE v2 - 调度执行引擎
 * CarrotRPC
 *
 * cmd_scan → queue → cmd_parse → dispatch_find → invoke_call
 *****************************/
#include "invoke.h"
#include "rpclog.h"
#include <string.h>

/*=============================================================
 * 内部：将 cmd_arg_t 转为类型化值，存入 staging buffer
 * p[i] 直接指向数据 (一次解引用读取值)
 *=============================================================*/
static void stage_arg(const cmd_arg_t* arg, uint8_t type,
                      int64_t* val_i64, uint64_t* val_u64,
                      char (*str_buf)[INVOKE_STR_MAX_SIZE],
                      void** p, uint8_t i)
{
    switch (type)
    {
    case DI:
        val_i64[i] = typeconv_to_i64(arg->ptr, arg->len);
        p[i] = &val_i64[i];
        break;
    case DU:
        val_u64[i] = typeconv_to_u64(arg->ptr, arg->len);
        p[i] = &val_u64[i];
        break;
    case DS:
    {
        uint16_t copy_len = arg->len < (INVOKE_STR_MAX_SIZE - 1)
                              ? arg->len : (INVOKE_STR_MAX_SIZE - 1);
        memcpy(str_buf[i], arg->ptr, copy_len);
        str_buf[i][copy_len] = '\0';
        p[i] = str_buf[i];
        break;
    }
    case DV:
    default:
        p[i] = NULL;
        break;
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
                              cmd_args_t* result, invoke_ret_t* ret)
{
    if (result == NULL || result->func_name == NULL || result->func_name_len == 0)
    {
        rpc_error("Invalid result.");
        return DISPATCH_ERR_NULL;
    }

    /* 1. 查找函数 */
    dispatch_func_t* f = dispatch_find(reg, result->func_name, result->func_name_len);
    if (f == NULL)
    {
        rpc_error("Function not found.");
        return DISPATCH_ERR_NOT_FOUND;
    }

    uint8_t expected_args = f->args_count;

    /* 2. 验证参数数量 */
    if (result->args_count != expected_args)
    {
        rpc_error("Arg count mismatch: expected %d, got %d.",
                  expected_args, result->args_count);
        return DISPATCH_ERR_SIG;
    }

    /* 3. staging buffer — 全部在栈上，outlives handler call */
    int64_t  val_i64[DISPATCH_ARGS_MAX_CNT];
    uint64_t val_u64[DISPATCH_ARGS_MAX_CNT];
    char     str_buf[DISPATCH_ARGS_MAX_CNT][INVOKE_STR_MAX_SIZE];
    void*    p[DISPATCH_ARGS_MAX_CNT];

    memset(val_i64, 0, sizeof(val_i64));
    memset(val_u64, 0, sizeof(val_u64));
    memset(str_buf, 0, sizeof(str_buf));
    memset(p, 0, sizeof(p));

    for (uint8_t i = 0; i < expected_args; i++)
    {
        if (f->args_type[i] == DV)
        {
            p[i] = NULL;
            continue;
        }
        stage_arg(&result->args[i], f->args_type[i],
                  val_i64, val_u64, str_buf, p, i);
    }

    /* 4. 根据返回值类型选择 delegate 族，按参数数量分发 */
    invoke_ret_type_t ret_kind = resolve_ret_type(f->ret_type);

    switch (ret_kind)
    {
    /* ---- void 返回 ---- */
    case INVOKERET_NONE:
    {
        switch (expected_args)
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
        switch (expected_args)
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
        switch (expected_args)
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
