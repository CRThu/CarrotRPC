/****************************
 * DISPATCH v2 - 函数注册与查找
 * CarrotRPC
 *
 * 完全自包含，运行时字符串签名注册
 *****************************/
#include "dispatch.h"
#include "cmdscan.h"
#include <string.h>

/*=============================================================
 * 签名解析
 *=============================================================*/

/**
 * @brief 单字符转类型
 */
static int8_t _parse_type_char(char c)
{
    switch (c)
    {
    case 'v': return DV;
    case 'i': return DI;
    case 'u': return DU;
    case 's': return DS;
    case 'f': return DF;
    default:  return -1;
    }
}

/**
 * @brief 解析类型字符串 "i64" / "u64" / "i" / "u" / "s" / "v" / "f"
 */
static int8_t _parse_type(const char* start, const char* end)
{
    uint16_t len = (uint16_t)(end - start);
    if (len == 0) return -1;

    // 单字符: v, i, u, s, f
    if (len == 1) return _parse_type_char(*start);

    // 双字符: i64, u64
    if (len == 2 && start[0] == 'i' && start[1] == '6' && end[-1] == '4')
        return DI;
    if (len == 2 && start[0] == 'u' && start[1] == '6' && end[-1] == '4')
        return DU;

    // 三字符: i64, u64, f64
    if (len == 3 && start[0] == 'i' && start[1] == '6' && start[2] == '4')
        return DI;
    if (len == 3 && start[0] == 'u' && start[1] == '6' && start[2] == '4')
        return DU;
    if (len == 3 && start[0] == 'f' && start[1] == '6' && start[2] == '4')
        return DF;

    return -1;
}

/**
 * @brief 解析签名字符串
 *
 * 支持格式:
 *   "(args...) -> ret"   带括号
 *   "args -> ret"        不带括号
 *   "()"                 无参数
 *   ""                   空, 无参数void
 *
 * @param sig      签名字符串
 * @param out_ret  输出返回类型
 * @param out_args 输出参数类型数组
 * @param out_argc 输出参数数量
 * @return dispatch_status_t
 */
static dispatch_status_t _parse_sig(const char* sig,
                       uint8_t* out_ret,
                       uint8_t* out_args,
                       uint8_t* out_argc)
{
    *out_ret = DV;
    *out_argc = 0;

    if (sig == NULL) return DISPATCH_ERR_SIG;

    const char* p = sig;

    // 跳过前导空格
    while (*p == ' ') p++;

    // 空字符串: 无参数, void
    if (*p == '\0') return DISPATCH_OK;

    // 判断是否有 '('
    const char* paren = strchr(p, '(');

    if (paren != NULL)
    {
        // 解析括号内的参数
        p = paren + 1;
        while (*p == ' ') p++;

        if (*p == ')')
        {
            // 空参数列表
            p++;
        }
        else
        {
            // 解析参数类型
            while (*p != ')' && *p != '\0' && *out_argc < DISPATCH_ARGS_MAX_CNT)
            {
                while (*p == ' ') p++;
                const char* type_start = p;
                while (*p != ',' && *p != ')' && *p != ' ' && *p != '\0') p++;
                const char* type_end = p;

                int8_t t = _parse_type(type_start, type_end);
                if (t < 0) return DISPATCH_ERR_SIG;
                out_args[(*out_argc)++] = (uint8_t)t;

                while (*p == ' ') p++;
                if (*p == ',') p++;
            }
            if (*p == ')') p++;
        }
    }
    else
    {
        // 无 '(' — 纯参数列表 "i64, i64 -> i64"
        // 但如果有 -> 且前面没有 (, 说明是 "args -> ret" 格式
        // 先检查是否有 -> (不在括号内的情况)
        const char* arrow = strstr(p, "->");
        if (arrow != NULL)
        {
            // 有 "->", 但没有 '(', 说明整个字符串是参数+返回值
            // 但这种情况很少见, 我们要求有 -> 前必须有内容
            // 解析 -> 之前的参数
            while (*p != '\0' && p < arrow)
            {
                while (*p == ' ') p++;
                if (p >= arrow) break;
                const char* type_start = p;
                while (*p != ',' && *p != ' ' && *p != '\0' && p < arrow) p++;
                const char* type_end = p;

                int8_t t = _parse_type(type_start, type_end);
                if (t < 0) return DISPATCH_ERR_SIG;
                out_args[(*out_argc)++] = (uint8_t)t;

                while (*p == ' ') p++;
                if (*p == ',') p++;
            }

            // 解析 -> 之后的返回类型
            p = arrow + 2;
            while (*p == ' ') p++;
            if (*p != '\0')
            {
                const char* type_start = p;
                while (*p != '\0' && *p != ' ' && *p != ',' && *p != ')') p++;
                int8_t t = _parse_type(type_start, p);
                if (t < 0) return DISPATCH_ERR_SIG;
                *out_ret = (uint8_t)t;
            }
        }
        else
        {
            // 无 '(', 无 '->', 但有内容 — 这种格式不合法
            // 除非是 "args" 格式 (全部是参数, void返回)
            // 支持: "i64, i64" = 两个int64参数, void返回
            while (*p != '\0')
            {
                while (*p == ' ') p++;
                if (*p == '\0') break;
                const char* type_start = p;
                while (*p != ',' && *p != ' ' && *p != '\0') p++;
                const char* type_end = p;

                int8_t t = _parse_type(type_start, type_end);
                if (t < 0) return DISPATCH_ERR_SIG;
                out_args[(*out_argc)++] = (uint8_t)t;

                while (*p == ' ') p++;
                if (*p == ',') p++;
            }
        }
    }

    // 解析 -> 返回类型 (如果还没解析)
    // 重新扫描, 找 -> (可能在括号之后)
    if (*out_ret == DV) // 还没设置返回类型
    {
        const char* arrow = strstr(sig, "->");
        if (arrow != NULL)
        {
            p = arrow + 2;
            while (*p == ' ') p++;
            if (*p != '\0')
            {
                const char* type_start = p;
                while (*p != '\0' && *p != ' ' && *p != ',' && *p != ')') p++;
                int8_t t = _parse_type(type_start, p);
                if (t >= 0) *out_ret = (uint8_t)t;
            }
        }
    }

    return DISPATCH_OK;
}

/*=============================================================
 * 公开 API
 *=============================================================*/

void dispatch_init(dispatch_registry_t* dispatcher)
{
    if (dispatcher == NULL) return;
    dispatcher->count = 0;
    memset(dispatcher->funcs, 0, sizeof(dispatcher->funcs));
}

dispatch_status_t _dispatch_add(dispatch_registry_t* dispatcher,
                                const char* name, dispatch_handler_fn handler, const char* sig)
{
    if (dispatcher == NULL || handler == NULL || sig == NULL)
        return DISPATCH_ERR_NULL;

    if (dispatcher->count >= DISPATCH_MAX_FUNC_CNT)
        return DISPATCH_ERR_FULL;

    dispatch_func_t* f = &dispatcher->funcs[dispatcher->count];

    // 解析签名
    uint8_t ret_type = DV;
    uint8_t args_type[DISPATCH_ARGS_MAX_CNT] = {0};
    uint8_t args_count = 0;

    if (_parse_sig(sig, &ret_type, args_type, &args_count) != DISPATCH_OK)
        return DISPATCH_ERR_SIG;

    // 函数名: 优先从签名字符串提取 (如有 '(')
    // 否则用外部传入的 name
    uint16_t name_len;
    const char* paren = strchr(sig, '(');
    if (paren != NULL && paren > sig)
    {
        // 签名中有函数名部分, 如 "hello()" → "hello"
        name_len = (uint16_t)(paren - sig);
        while (name_len > 0 && sig[name_len - 1] == ' ') name_len--;
        if (name_len >= DISPATCH_FUNC_NAME_MAX) name_len = DISPATCH_FUNC_NAME_MAX - 1;
        memcpy(f->name, sig, name_len);
    }
    else
    {
        name_len = (uint16_t)strlen(name);
        if (name_len >= DISPATCH_FUNC_NAME_MAX) name_len = DISPATCH_FUNC_NAME_MAX - 1;
        memcpy(f->name, name, name_len);
    }
    f->name[name_len] = '\0';
    f->name_len = name_len;

    f->handler = handler;
    f->ret_type = ret_type;
    memcpy(f->args_type, args_type, args_count);
    f->args_count = args_count;

    dispatcher->count++;
    return DISPATCH_OK;
}

dispatch_func_t* dispatch_find(dispatch_registry_t* dispatcher,
                               const char* name, uint16_t len)
{
    if (dispatcher == NULL || name == NULL || len == 0) return NULL;

    for (uint16_t i = 0; i < dispatcher->count; i++)
    {
        if (cmd_compare(dispatcher->funcs[i].name, dispatcher->funcs[i].name_len, name, len) == 0)
            return &dispatcher->funcs[i];
    }
    return NULL;
}
