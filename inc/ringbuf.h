/****************************
* RINGBUF - 通用环形缓冲区
* CRTHu
* 2025.07.16
*
* 设计目标：
* 1. 通用环形缓冲区，支持读写
* 2. 可选硬件同步：DMA 接收(读 head) / DMA 发送(读 tail)
* 3. wrap-aware 读写操作
* 4. 适用于：DMA 接收、DMA 发送、命令队列、日志缓冲区等
*
* 编译宏：
*   RINGBUF_DMA - 启用 DMA 硬件同步功能（head_reader / tail_reader）
*                  关闭后退化为纯软件环形缓冲区，struct 更小
*****************************/
#pragma once
#ifndef _RINGBUF_H_
#define _RINGBUF_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <inttypes.h>
#include "rpc_cfg.h"

typedef struct
{
    uint8_t* buf;
    uint16_t size;
    uint16_t head;              /* 写游标 */
    uint16_t tail;              /* 读游标 */

#if RINGBUF_DMA
    /* 可选: 硬件位置读取函数 */
    uint16_t (*head_reader)(void);  /* DMA RX: 读硬件写位置 */
    uint16_t (*tail_reader)(void);  /* DMA TX: 读硬件读位置 */

    /* 极简 DMA TX 状态机字段 */
    uint8_t  tx_busy;               /* 1: DMA 传输中, 0: 空闲 */
    uint16_t tx_last_len;           /* 上次提交传输的字节数 */
#endif
} ringbuf_t;

/**
 * @brief 初始化环形缓冲区
 */
void ringbuf_init(ringbuf_t* ring, uint8_t* buf, uint16_t size);

#if RINGBUF_DMA
/**
 * @brief 设置硬件位置读取函数
 *
 * 绑定后，ringbuf_readable() / peek() / write() 等 API 将自动向硬件拉取最新游标，
 * 无需在外部手动更新 head/tail 变量。
 *
 * @param head_reader DMA RX: 返回硬件当前写位置
 * @param tail_reader DMA TX: 返回硬件当前读位置
 */
void ringbuf_set_head_reader(ringbuf_t* ring, uint16_t (*head_reader)(void));
void ringbuf_set_tail_reader(ringbuf_t* ring, uint16_t (*tail_reader)(void));

/**
 * @brief 手动将硬件当前游标快照同步到结构体 head/tail 变量
 * (当开启 RINGBUF_DMA 并绑定 reader 后，API 已支持全自动求值，此 API 用于手动抓取快照)
 */
void ringbuf_sync_head(ringbuf_t* ring);
void ringbuf_sync_tail(ringbuf_t* ring);

/**
 * @brief 极简 DMA TX 状态机: 尝试提取一段可直接传给 DMA 的连续内存切片
 *
 * 自动处理回绕截断与繁忙锁控
 *
 * @param ring 环形缓冲区指针
 * @param ptr [OUT] 传出连续内存切片的起始指针
 * @param len [OUT] 传出连续切片的字节数
 * @return 1 成功提取待发送切片, 0 繁忙或无数据
 */
uint8_t ringbuf_dma_tx_fetch(ringbuf_t* ring, const uint8_t** ptr, uint16_t* len);

/**
 * @brief 极简 DMA TX 状态机: 在 DMA 完成 TC 中断里提交上次发送，并探查是否链式续发
 *
 * @param ring 环形缓冲区指针
 * @param next_ptr [OUT] 传出下一段连续内存切片指针
 * @param next_len [OUT] 传出下一段连续切片字节数
 * @return 1 有后续切片需要链式续发，0 无后续切片已释放 busy 状态
 */
uint8_t ringbuf_dma_tx_complete(ringbuf_t* ring, const uint8_t** next_ptr, uint16_t* next_len);
#endif

/**
 * @brief 手动设置 head/tail
 */
void ringbuf_set_head(ringbuf_t* ring, uint16_t head);
void ringbuf_set_tail(ringbuf_t* ring, uint16_t tail);

/**
 * @brief 可读/可写字节数
 */
uint16_t ringbuf_readable(ringbuf_t* ring);
uint16_t ringbuf_writable(ringbuf_t* ring);

/**
 * @brief 探查读取数据（处理 wrap-around），不消费数据 (不移动 tail)
 */
uint16_t ringbuf_peek(ringbuf_t* ring, uint8_t* dst, uint16_t len);

/**
 * @brief 跳过已处理数据 (移动 tail)
 */
void ringbuf_skip(ringbuf_t* ring, uint16_t len);

/**
 * @brief 读取并消费数据（处理 wrap-around，相当于 peek + skip 组合）
 * @param ring 缓冲区指针
 * @param dst 目标缓冲区
 * @param len 期待读取的字节数
 * @return uint16_t 实际读取并消费的字节数
 */
uint16_t ringbuf_read(ringbuf_t* ring, uint8_t* dst, uint16_t len);

/**
 * @brief 写入数据（处理 wrap-around）
 */
uint16_t ringbuf_write(ringbuf_t* ring, const uint8_t* src, uint16_t len);

/**
 * @brief 清空缓冲区
 */
void ringbuf_flush(ringbuf_t* ring);

#ifdef __cplusplus
}
#endif

#endif /* _RINGBUF_H_ */
