/****************************
* CMD QUEUE 示例 - 命令队列演示
* 演示排队执行和协作式中断
*****************************/
#include "cmdqueue.h"
#include "cmdscan.h"
#include <stdio.h>
#include <string.h>

/*=============================================================
 * 模拟耗时任务
 *=============================================================*/

void long_running_task(cmd_queue_t* queue)
{
    printf("[TASK] 开始耗时任务...\r\n");

    for (int i = 0; i < 10; i++)
    {
        printf("[TASK] 处理中... %d/10\r\n", i + 1);

        if (cmd_queue_check(queue, "stop"))
        {
            printf("[TASK] 检测到 stop 命令，主动退出！\r\n");
            return;
        }
    }

    printf("[TASK] 任务完成\r\n");
}

/*=============================================================
 * 主程序
 *=============================================================*/

static uint8_t queue_buf_storage[2048];

int main(void)
{
    printf("CMD QUEUE 命令队列示例\r\n");
    printf("========================\r\n\r\n");

    cmd_queue_t queue;
    cmd_queue_init(&queue, queue_buf_storage, sizeof(queue_buf_storage));

    /* --- 示例1: cmd_scan + cmd_queue 流程 --- */
    printf("=== 示例1: scan + queue 流程 ===\r\n");

    /* 模拟 DMA 接收的命令 */
    const char* dma_data = "led_on(1)\nled_off(2)\nstop\n";
    uint16_t dma_len = strlen(dma_data);

    /* 使用 scanner 逐条扫描提取并推入队列 */
    cmd_scanner_t scanner;
    cmd_init(&scanner, (const uint8_t*)dma_data, dma_len);

    cmd_entry_t entry;
    while (cmd_scan(&scanner, &entry) == CMD_COMPLETE)
    {
        cmd_queue_push(&queue, &entry);
    }

    printf("[QUEUE] 队列中有 %d 条命令\r\n", cmd_queue_count(&queue));

    /* 出队执行 */
    cmd_entry_t out;
    while (cmd_queue_pop(&queue, &out) == CMDQUEUE_OK)
    {
        const char* cmd = (const char*)out.buf + out.cmd_start;

        /* 执行时用 cmd_parse 解析 */
        char cmd_buf[64];
        memcpy(cmd_buf, cmd, out.cmd_len);
        cmd_buf[out.cmd_len] = '\0';

        cmd_args_t result;
        cmd_parse(cmd_buf, out.cmd_len, &result);

        printf("[EXEC] %.*s (func: %.*s, args: %d)\r\n",
               out.cmd_len, cmd,
               result.func_name_len, result.func_name,
               result.args_count);
    }
    printf("\r\n");

    cmd_queue_flush(&queue);

    /* --- 示例2: 协作式中断 --- */
    printf("=== 示例2: 协作式中断 ===\r\n");

    cmd_scanner_t scanner2;
    cmd_init(&scanner2, (const uint8_t*)"stop", 4);
    cmd_entry_t entry2;
    cmd_scan(&scanner2, &entry2);
    cmd_queue_push(&queue, &entry2);

    printf("[CHECK] 检查队列: %s\r\n",
           cmd_queue_check(&queue, "stop") ? "找到 stop" : "未找到");

    long_running_task(&queue);
    printf("\r\n");

    cmd_queue_flush(&queue);

    /* --- 示例3: 非环形缓冲区（分号分隔） --- */
    printf("=== 示例3: 非环形缓冲区 ===\r\n");

    const char* linear_buf = "cmd_a;cmd_b;cmd_c";
    uint16_t linear_len = strlen(linear_buf);

    cmd_scanner_t scanner3;
    cmd_init(&scanner3, (const uint8_t*)linear_buf, linear_len);

    cmd_entry_t entry3;
    while (cmd_scan(&scanner3, &entry3) == CMD_COMPLETE)
    {
        cmd_queue_push(&queue, &entry3);
    }

    printf("[QUEUE] 入队 %d 条命令\r\n", cmd_queue_count(&queue));

    while (cmd_queue_pop(&queue, &out) == CMDQUEUE_OK)
    {
        printf("[EXEC] %.*s\r\n", out.cmd_len, out.buf + out.cmd_start);
    }

    return 0;
}
