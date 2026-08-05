/****************************
 * DISPATCH v2 - 函数注册与查找
 * CarrotRPC
 *
 * 完全自包含，不依赖旧代码
 * 运行时字符串签名注册
 *
 * 用法：
 *   static dispatch_registry_t dispatcher;
 *
 *   dispatch_init(&dispatcher);
 *   dispatch_reg(&dispatcher, add, "add(i, i) -> i");
 *   dispatch_reg(&dispatcher, hello, "hello()");
 *   dispatch_find(&dispatcher, "add", 3);
 *****************************/
#pragma once
#ifndef _DISPATCH_H_
#define _DISPATCH_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <inttypes.h>
#include "rpc_cfg.h"

/*=============================================================
 * 状态码
 *=============================================================*/
typedef enum {
    DISPATCH_OK = 0,
    DISPATCH_ERR_NULL,          // 空指针
    DISPATCH_ERR_FULL,          // 注册表已满
    DISPATCH_ERR_SIG,           // 签名解析失败
    DISPATCH_ERR_NOT_FOUND,     // 函数未找到
} dispatch_status_t;

/*=============================================================
 * 类型系统 (独立于旧 dynpool)
 *=============================================================*/
typedef enum {
    DV = 0,     // void
    DI,         // int64
    DU,         // uint64
    DS,         // string
    DF,         // float64
} dispatch_type_t;

/*=============================================================
 * 函数信息
 *=============================================================*/
typedef void (*dispatch_handler_fn)(void);

typedef struct {
    char         name[DISPATCH_FUNC_NAME_MAX];
    uint16_t     name_len;
    dispatch_handler_fn handler;
    uint8_t      ret_type;                   // dispatch_type_t
    uint8_t      args_type[DISPATCH_ARGS_MAX_CNT];
    uint8_t      args_count;
} dispatch_func_t;

/*=============================================================
 * 注册表 (编译期固定容量)
 *=============================================================*/
typedef struct {
    dispatch_func_t  funcs[DISPATCH_MAX_FUNC_CNT];
    uint16_t         count;                 // 当前数量
} dispatch_registry_t;

/*=============================================================
 * 注册宏
 *=============================================================*/
#define dispatch_reg(REG, HANDLER, SIG) \
    _dispatch_add(REG, #HANDLER, (dispatch_handler_fn)(HANDLER), SIG)

/*=============================================================
 * 公开 API
 *=============================================================*/

/**
 * @brief 初始化注册表
 *
 * @param dispatcher  注册表指针
 */
void dispatch_init(dispatch_registry_t* dispatcher);

/**
 * @brief 注册函数 (内部由宏调用)
 *
 * @param dispatcher  注册表指针
 * @param name        函数名 (字符串)
 * @param handler     函数指针
 * @param sig         签名字符串, 如 "(i, i) -> i" 或 "hello()"
 * @return dispatch_status_t
 */
dispatch_status_t _dispatch_add(dispatch_registry_t* dispatcher,
                                const char* name, dispatch_handler_fn handler, const char* sig);

/**
 * @brief 查找函数
 *
 * @param dispatcher  注册表指针
 * @param name        函数名指针 (不需要 null-terminate)
 * @param len         函数名长度
 * @return dispatch_func_t* 找到的函数信息, NULL=未找到
 */
dispatch_func_t* dispatch_find(dispatch_registry_t* dispatcher,
                               const char* name, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* _DISPATCH_H_ */
