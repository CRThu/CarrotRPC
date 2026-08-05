/****************************
* CMD QUEUE - 命令队列
* CRTHu
* 2025.07.15
*
* 设计目标：
* 1. 环形缓冲区，FIFO 顺序执行
* 2. 存储原始命令 + 紧凑元数据
* 3. 支持协作式中断检查
* 4. 兼容环形/非环形 DMA 缓冲区
*****************************/
#pragma once
#ifndef _CMD_QUEUE_H_
#define _CMD_QUEUE_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <inttypes.h>
#include "rpc_cfg.h"
#include "cmdscan.h"
#include "ringbuf.h"

#if RPC_USE_CMD_QUEUE

/* 队列状态码 */
typedef enum {
    CMDQUEUE_OK = 0,
    CMDQUEUE_ERR_NULL,      /* 空指针 */
    CMDQUEUE_ERR_FULL,      /* 队列满 / 缓冲区不足 */
} cmd_queue_status_t;

/* 命令队列 */
typedef struct
{
    cmd_entry_t items[CMD_QUEUE_SIZE];
    ringbuf_t ring;             /* 环形缓冲区（管理命令数据） */
    uint8_t head;               /* 队头索引 */
    uint8_t tail;               /* 队尾索引 */
    uint8_t count;              /* 当前队列长度 */
} cmd_queue_t;

/**
 * @brief 初始化命令队列 (缓冲区由外部传入，消除硬编码静态 RAM 占用)
 * @param queue 队列指针
 * @param buf 外部缓冲区指针
 * @param buf_size 外部缓冲区字节数
 */
void cmd_queue_init(cmd_queue_t* queue, uint8_t* buf, uint16_t buf_size);

/**
 * @brief 命令入队（复制原始命令到 buf）
 * @param queue 队列指针
 * @param entry 命令条目（来自 cmd_scan）
 * @return cmd_queue_status_t 成功或其他错误码
 */
cmd_queue_status_t cmd_queue_push(cmd_queue_t* queue, cmd_entry_t* entry);

/**
 * @brief 命令出队
 * @param queue 队列指针
 * @param entry 输出：命令条目（buf 指向 queue 内部 buf）
 * @return cmd_queue_status_t 成功，CMDQUEUE_ERR_FULL 队列空
 */
cmd_queue_status_t cmd_queue_pop(cmd_queue_t* queue, cmd_entry_t* entry);

/**
 * @brief 检查队列是否为空
 */
uint8_t cmd_queue_is_empty(cmd_queue_t* queue);

/**
 * @brief 检查队列是否已满
 */
uint8_t cmd_queue_is_full(cmd_queue_t* queue);

/**
 * @brief 获取队列中命令数量
 */
uint8_t cmd_queue_count(cmd_queue_t* queue);

/**
 * @brief 清空队列
 */
void cmd_queue_flush(cmd_queue_t* queue);

/**
 * @brief 扫描队列是否有指定函数名的命令
 * @param queue 队列指针
 * @param func_name 要查找的函数名
 * @return 1 找到，0 未找到
 */
uint8_t cmd_queue_check(cmd_queue_t* queue, const char* func_name);

#endif /* RPC_USE_CMD_QUEUE */

#ifdef __cplusplus
}
#endif

#endif /* _CMD_QUEUE_H_ */
