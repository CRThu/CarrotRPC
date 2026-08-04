/****************************
 * RPC_LOG - 分级日志库
 * CarrotRPC
 *
 * 设计目标：
 * 1. 取代 printf，零 stdio 依赖
 * 2. 两级级别：用户级别（可开关）+ 协议级别（始终输出）
 * 3. 自实现格式化，类型安全，防注入
 * 4. 对齐 CARROT_RPC 协议：[LEVEL]: message
 *
 * 编译宏：
 *   RPC_LOG_OUTPUT_BUF    - 启用缓冲区输出模式（适合 UART DMA）
 *   RPC_LOG_ENABLE_DEBUG  - 设为 0 编译期消除 DEBUG 日志
 *   RPC_LOG_ENABLE_INFO   - 设为 0 编译期消除 INFO 日志
 *   RPC_LOG_ENABLE_WARN   - 设为 0 编译期消除 WARN 日志
 *   RPC_LOG_ENABLE_ERROR  - 设为 0 编译期消除 ERROR 日志
 *****************************/
#pragma once
#ifndef _RPCLOG_H_
#define _RPCLOG_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <inttypes.h>
#include "rpc_cfg.h"

/*=============================================================
 * 编译期配置（保留默认值，允许 rpc_config.h 覆盖）
 *=============================================================*/

#ifndef RPC_LOG_ENABLE_DEBUG
#define RPC_LOG_ENABLE_DEBUG    1
#endif

#ifndef RPC_LOG_ENABLE_INFO
#define RPC_LOG_ENABLE_INFO     1
#endif

#ifndef RPC_LOG_ENABLE_WARN
#define RPC_LOG_ENABLE_WARN     1
#endif

#ifndef RPC_LOG_ENABLE_ERROR
#define RPC_LOG_ENABLE_ERROR    1
#endif

/*=============================================================
 * 日志级别
 *=============================================================*/

/* 用户级别（可通过 rpc_log_set_level 开关） */
#define RPC_LOG_DEBUG   0
#define RPC_LOG_INFO    1
#define RPC_LOG_WARN    2
#define RPC_LOG_ERROR   3

/* 协议级别（始终输出，不受 set_level 影响） */
#define RPC_LOG_RETURN  4
#define RPC_LOG_DATA    5
#define RPC_LOG_REG     6

/*=============================================================
 * 输出回调（编译期二选一）
 *=============================================================*/

#ifdef RPC_LOG_OUTPUT_BUF
typedef void (*rpc_log_out_fn)(const char* buf, uint16_t len);
#else
typedef void (*rpc_log_out_fn)(char c);
#endif

/**
 * @brief 设置日志输出目标
 */
void rpc_log_set_output(rpc_log_out_fn fn);

/**
 * @brief 设置全局最低用户级别（协议级别不受影响）
 */
void rpc_log_set_level(uint8_t level);

/*=============================================================
 * 核心日志函数
 *=============================================================*/

/**
 * @brief 格式化日志（安全子集，防注入）
 * 支持: %d %u %x %X %s %c %%
 * 无参数时直接传字符串即可：rpc_log(RPC_LOG_INFO, "hello")
 * 有参数时：rpc_log(RPC_LOG_ERROR, "expected %d, got %d", a, b)
 * 输出格式: [LEVEL]: message\r\n
 */
void rpc_log(uint8_t level, const char* fmt, ...);

/**
 * @brief int64 类型输出
 * 输出格式: [LEVEL]: tag=value\r\n
 */
void rpc_log_i64(uint8_t level, const char* tag, int64_t val);

/**
 * @brief uint64 类型输出
 * 输出格式: [LEVEL]: tag=value\r\n
 */
void rpc_log_u64(uint8_t level, const char* tag, uint64_t val);

/**
 * @brief 十六进制输出
 * 输出格式: [LEVEL]: tag=0xHEX\r\n
 */
void rpc_log_hex(uint8_t level, const char* tag, uint64_t val);

/**
 * @brief 浮点数输出
 * 输出格式: [LEVEL]: tag=value\r\n
 */
void rpc_log_f64(uint8_t level, const char* tag, double val, uint8_t prec);

/**
 * @brief 字符串输出
 * 输出格式: [LEVEL]: tag=str\r\n (tag 为 NULL 时只输出 [LEVEL]: str\r\n)
 */
void rpc_log_str(uint8_t level, const char* tag, const char* str);

/*=============================================================
 * 便捷宏
 *=============================================================*/

#if RPC_LOG_ENABLE_DEBUG
#define rpc_debug(...)         rpc_log(RPC_LOG_DEBUG, __VA_ARGS__)
#else
#define rpc_debug(...)         ((void)0)
#endif

#if RPC_LOG_ENABLE_INFO
#define rpc_info(...)          rpc_log(RPC_LOG_INFO, __VA_ARGS__)
#else
#define rpc_info(...)          ((void)0)
#endif

#if RPC_LOG_ENABLE_WARN
#define rpc_warning(...)       rpc_log(RPC_LOG_WARN, __VA_ARGS__)
#else
#define rpc_warning(...)       ((void)0)
#endif

#if RPC_LOG_ENABLE_ERROR
#define rpc_error(...)         rpc_log(RPC_LOG_ERROR, __VA_ARGS__)
#else
#define rpc_error(...)         ((void)0)
#endif

/* 协议级别宏（始终输出，无法编译期关闭） */
#define rpc_return(...)        rpc_log(RPC_LOG_RETURN, __VA_ARGS__)
#define rpc_data(...)          rpc_log(RPC_LOG_DATA,   __VA_ARGS__)
#define rpc_reg(...)           rpc_log(RPC_LOG_REG,    __VA_ARGS__)

/*=============================================================
 * 便捷宏 - 类型输出（组合 level + type）
 *=============================================================*/

#if RPC_LOG_ENABLE_DEBUG
#define rpc_debug_i64(tag, val)   rpc_log_i64(RPC_LOG_DEBUG, tag, val)
#define rpc_debug_u64(tag, val)   rpc_log_u64(RPC_LOG_DEBUG, tag, val)
#define rpc_debug_hex(tag, val)   rpc_log_hex(RPC_LOG_DEBUG, tag, val)
#define rpc_debug_f64(tag, v, p)  rpc_log_f64(RPC_LOG_DEBUG, tag, v, p)
#define rpc_debug_str(tag, str)   rpc_log_str(RPC_LOG_DEBUG, tag, str)
#else
#define rpc_debug_i64(tag, val)   ((void)0)
#define rpc_debug_u64(tag, val)   ((void)0)
#define rpc_debug_hex(tag, val)   ((void)0)
#define rpc_debug_f64(tag, v, p)  ((void)0)
#define rpc_debug_str(tag, str)   ((void)0)
#endif

#if RPC_LOG_ENABLE_INFO
#define rpc_info_i64(tag, val)    rpc_log_i64(RPC_LOG_INFO, tag, val)
#define rpc_info_u64(tag, val)    rpc_log_u64(RPC_LOG_INFO, tag, val)
#define rpc_info_hex(tag, val)    rpc_log_hex(RPC_LOG_INFO, tag, val)
#define rpc_info_f64(tag, v, p)   rpc_log_f64(RPC_LOG_INFO, tag, v, p)
#define rpc_info_str(tag, str)    rpc_log_str(RPC_LOG_INFO, tag, str)
#else
#define rpc_info_i64(tag, val)    ((void)0)
#define rpc_info_u64(tag, val)    ((void)0)
#define rpc_info_hex(tag, val)    ((void)0)
#define rpc_info_f64(tag, v, p)   ((void)0)
#define rpc_info_str(tag, str)    ((void)0)
#endif

#if RPC_LOG_ENABLE_WARN
#define rpc_warn_i64(tag, val)    rpc_log_i64(RPC_LOG_WARN, tag, val)
#define rpc_warn_u64(tag, val)    rpc_log_u64(RPC_LOG_WARN, tag, val)
#define rpc_warn_hex(tag, val)    rpc_log_hex(RPC_LOG_WARN, tag, val)
#define rpc_warn_f64(tag, v, p)   rpc_log_f64(RPC_LOG_WARN, tag, v, p)
#define rpc_warn_str(tag, str)    rpc_log_str(RPC_LOG_WARN, tag, str)
#else
#define rpc_warn_i64(tag, val)    ((void)0)
#define rpc_warn_u64(tag, val)    ((void)0)
#define rpc_warn_hex(tag, val)    ((void)0)
#define rpc_warn_f64(tag, v, p)   ((void)0)
#define rpc_warn_str(tag, str)    ((void)0)
#endif

#if RPC_LOG_ENABLE_ERROR
#define rpc_error_i64(tag, val)   rpc_log_i64(RPC_LOG_ERROR, tag, val)
#define rpc_error_u64(tag, val)   rpc_log_u64(RPC_LOG_ERROR, tag, val)
#define rpc_error_hex(tag, val)   rpc_log_hex(RPC_LOG_ERROR, tag, val)
#define rpc_error_f64(tag, v, p)  rpc_log_f64(RPC_LOG_ERROR, tag, v, p)
#define rpc_error_str(tag, str)   rpc_log_str(RPC_LOG_ERROR, tag, str)
#else
#define rpc_error_i64(tag, val)   ((void)0)
#define rpc_error_u64(tag, val)   ((void)0)
#define rpc_error_hex(tag, val)   ((void)0)
#define rpc_error_f64(tag, v, p)  ((void)0)
#define rpc_error_str(tag, str)   ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* _RPCLOG_H_ */
