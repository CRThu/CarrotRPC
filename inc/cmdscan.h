/****************************
* CMD - 零拷贝命令解析器
* CRTHu
* 2025.07.15
*
* 设计目标：
* 1. 无内存复制 - 直接返回缓冲区指针
* 2. 扫描提取 + 零拷贝参数切分
* 3. 无 std 库依赖 - 纯嵌入式友好
* 4. 提取函数名长度 - 供 cmd_queue 直接使用
*****************************/
#pragma once
#ifndef _CMD_SCAN_H_
#define _CMD_SCAN_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <inttypes.h>
#include "ringbuf.h"

#define CMD_MAX_ARGS                    10

/* 参数指针结构 - 零拷贝，直接指向原始缓冲区 */
typedef struct
{
    const char* ptr;        /* 参数起始地址 */
    uint16_t len;           /* 参数长度（不含终止符） */
} cmd_arg_t;

/* 扫描/解析状态 */
typedef enum
{
    CMD_INCOMPLETE = 0,     /* 扫描未完成，等待更多数据 */
    CMD_COMPLETE = 1,       /* 扫描到完整命令（遇到 \n 或 \0） */
    CMD_ERROR = -2          /* 格式错误 */
} cmd_status_t;

/* 增量解析状态 */
typedef enum
{
    CMD_STATE_IDLE = 0,        /* 等待新命令起始（跳过前导空白/换行） */
    CMD_STATE_FUNC_NAME,       /* 正在扫描函数名 */
    CMD_STATE_PAREN_ARGS,      /* 正在扫描括号内参数 func(...) */
    CMD_STATE_SPACE_ARGS       /* 正在扫描无括号参数 func arg1 arg2 */
} cmd_scan_state_t;

/* 扫描器上下文 - 适配 DMA 增量接收 */
typedef struct
{
    const uint8_t* buf;       /* 缓冲区指针 */
    uint16_t buf_size;        /* 缓冲区总大小 */
    uint16_t scan_pos;        /* 当前增量扫描指针 (只进不退) */
    uint16_t cmd_start;       /* 当前正在扫描命令的起点 */
    uint8_t  func_len;        /* 已锁定的函数名长度 */
    cmd_scan_state_t state;   /* 增量解析状态机状态 */
    ringbuf_t* ring;          /* 可选绑定的环形缓冲区指针 (NULL 表示线性 buf) */
} cmd_scanner_t;

/* 命令条目 - 扫描提取结果，用于入队 */
typedef struct
{
    const uint8_t* buf;     /* DMA 缓冲区指针 */
    uint16_t buf_len;       /* DMA 缓冲区总长度（环形缓冲区需要） */
    uint16_t cmd_start;     /* 命令在 buf 中的起始位置 */
    uint16_t cmd_len;       /* 命令总长度 */
    uint8_t  func_len;      /* 函数名长度 */
} cmd_entry_t;

/* 参数切分结果 - 解析输出 */
typedef struct
{
    const char* func_name;          /* 函数名指针 */
    uint16_t func_name_len;         /* 函数名长度 */

    cmd_arg_t args[CMD_MAX_ARGS];   /* 参数指针数组 */
    uint8_t args_count;             /* 实际参数个数 */
} cmd_args_t;

/*=============================================================
 * API 函数
 *=============================================================*/

/**
 * @brief 初始化线性缓冲区扫描器
 * @param scanner 扫描器上下文
 * @param buf 缓冲区指针
 * @param buf_size 缓冲区大小
 */
void cmd_init(cmd_scanner_t* scanner, const uint8_t* buf, uint16_t buf_size);

/**
 * @brief 初始化环形缓冲区扫描器 (无缝支持 ringbuf 自动消费)
 * @param scanner 扫描器上下文
 * @param ring    绑定的环形缓冲区指针
 */
void cmd_init_ringbuf(cmd_scanner_t* scanner, ringbuf_t* ring);

/**
 * @brief 重置扫描器到初始状态
 * @param scanner 扫描器上下文
 */
void cmd_reset(cmd_scanner_t* scanner);

/**
 * @brief 扫描提取命令条目
 *
 * 从 scanner 当前扫描位置提取单条命令的边界信息
 * 可连续扫描多条命令，scanner 自动记录位置
 * 若绑定了 ringbuf，扫描到完整命令后会自动从 ringbuf 消费已被成帧的字节
 *
 * @param scanner 扫描器上下文
 * @param entry 输出：命令条目
 * @return cmd_status_t 扫描结果
 */
cmd_status_t cmd_scan(cmd_scanner_t* scanner, cmd_entry_t* entry);

/**
 * @brief 将完整命令解析为参数指针数组
 *
 * 无内存复制 - 所有指针直接指向原始 cmd 缓冲区
 *
 * @param cmd 命令字符串指针
 * @param len 命令长度
 * @param args 输出：参数切分结果
 * @return uint8_t 解析出的参数个数，0 表示无参数，错误时返回 0xFF
 */
uint8_t cmd_parse(const char* cmd, uint16_t len, cmd_args_t* args);

/**
 * @brief 快速字节比较 (尾部优先)
 *
 * 策略: 长度不等 → 尾字节 → 首字节 → 中间
 * 适配嵌入式命名习惯: 前缀相同后缀区分 (如 stm32_iic_write vs stm32_iic_read)
 *
 * @param a     比较源
 * @param a_len 源长度
 * @param b     比较目标
 * @param b_len 目标长度
 * @return 0 相等, 非0 不相等
 */
static inline int cmd_compare(const void* a, uint16_t a_len,
                              const void* b, uint16_t b_len)
{
    /* 长度不等直接退出 */
    if (a_len != b_len)
        return (int)a_len - (int)b_len;

    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* pb = (const uint8_t*)b;
    uint16_t len = a_len;

    if (len == 0) return 0;

    /* 尾字节快速退出 — 嵌入式命名后缀差异大 */
    if (pa[len - 1] != pb[len - 1])
        return (int)pa[len - 1] - (int)pb[len - 1];

    /* 首字节快速退出 */
    if (*pa != *pb)
        return (int)*pa - (int)*pb;

    /* 中间逐字节 (短字符串, 编译器易优化) */
    for (uint16_t i = 1; i < len - 1; i++)
    {
        if (pa[i] != pb[i])
            return (int)pa[i] - (int)pb[i];
    }

    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _CMD_SCAN_H_ */
