/****************************
 * INVOKE v2 - 调度执行引擎
 * CarrotRPC
 *
 * 设计目标：
 * 1. 栈上 staging buffer: val_i64[9], val_u64[9], str_buf[9][64], void* p[9]
 * 2. p[i] 直接指向数据 (一次解引用读取值)
 * 3. 依赖 typeconv (纯转换) 和 dispatch (函数查找)
 * 4. 支持三种返回值族: void / int64 / char*
 *****************************/
#pragma once
#ifndef _INVOKE_H_
#define _INVOKE_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "dispatch.h"
#include "typeconv.h"
#include "cmdscan.h"
#include "rpc_cfg.h"

/**
 * @brief 库内置统一字符串返回共享缓冲区 (避免 handler 内返回局部栈指针引发悬空指针)
 */
extern char g_rpc_str_ret_buf[INVOKE_STR_MAX_SIZE];

#define RPC_STR_RET_BUF g_rpc_str_ret_buf

/*=============================================================
 * 返回值类型
 *=============================================================*/

typedef enum
{
    INVOKERET_NONE = 1,
    INVOKERET_I64,
    INVOKERET_STR
} invoke_ret_type_t;

typedef struct
{
    invoke_ret_type_t type;
    union
    {
        int64_t i64;
        char str[INVOKE_STR_MAX_SIZE];
    };
} invoke_ret_t;

/*=============================================================
 * delegate 类型族 (void 返回 / int64 返回 / char* 返回)
 *=============================================================*/

/* --- void 返回 --- */
typedef void (*invoke_delegate_a0r0)(void);
typedef void (*invoke_delegate_a1r0)(void* arg1);
typedef void (*invoke_delegate_a2r0)(void* arg1, void* arg2);
typedef void (*invoke_delegate_a3r0)(void* arg1, void* arg2, void* arg3);
typedef void (*invoke_delegate_a4r0)(void* arg1, void* arg2, void* arg3, void* arg4);
typedef void (*invoke_delegate_a5r0)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5);
typedef void (*invoke_delegate_a6r0)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6);
typedef void (*invoke_delegate_a7r0)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6, void* arg7);
typedef void (*invoke_delegate_a8r0)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6, void* arg7, void* arg8);
typedef void (*invoke_delegate_a9r0)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6, void* arg7, void* arg8, void* arg9);

/* --- int64 返回 --- */
typedef int64_t (*invoke_delegate_a0r1)(void);
typedef int64_t (*invoke_delegate_a1r1)(void* arg1);
typedef int64_t (*invoke_delegate_a2r1)(void* arg1, void* arg2);
typedef int64_t (*invoke_delegate_a3r1)(void* arg1, void* arg2, void* arg3);
typedef int64_t (*invoke_delegate_a4r1)(void* arg1, void* arg2, void* arg3, void* arg4);
typedef int64_t (*invoke_delegate_a5r1)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5);
typedef int64_t (*invoke_delegate_a6r1)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6);
typedef int64_t (*invoke_delegate_a7r1)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6, void* arg7);
typedef int64_t (*invoke_delegate_a8r1)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6, void* arg7, void* arg8);
typedef int64_t (*invoke_delegate_a9r1)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6, void* arg7, void* arg8, void* arg9);

/* --- char* 返回 --- */
typedef char* (*invoke_delegate_a0rs)(void);
typedef char* (*invoke_delegate_a1rs)(void* arg1);
typedef char* (*invoke_delegate_a2rs)(void* arg1, void* arg2);
typedef char* (*invoke_delegate_a3rs)(void* arg1, void* arg2, void* arg3);
typedef char* (*invoke_delegate_a4rs)(void* arg1, void* arg2, void* arg3, void* arg4);
typedef char* (*invoke_delegate_a5rs)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5);
typedef char* (*invoke_delegate_a6rs)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6);
typedef char* (*invoke_delegate_a7rs)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6, void* arg7);
typedef char* (*invoke_delegate_a8rs)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6, void* arg7, void* arg8);
typedef char* (*invoke_delegate_a9rs)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6, void* arg7, void* arg8, void* arg9);

/*=============================================================
 * 公开 API
 *=============================================================*/

/**
 * @brief 直接通过 cmd_entry_t 命令条目调用函数（内置 100% 物理回绕兼容）
 *
 * @param reg     注册表指针
 * @param entry   扫描或队列弹出的命令条目（cmd_entry_t）
 * @param ret     可选的返回值输出 (NULL = 忽略返回值)
 * @return dispatch_status_t 调用状态
 */
dispatch_status_t invoke_call(dispatch_registry_t* reg,
                              const cmd_entry_t* entry, invoke_ret_t* ret);

#ifdef __cplusplus
}
#endif

#endif /* _INVOKE_H_ */
