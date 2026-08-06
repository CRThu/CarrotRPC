/****************************
* CMD 示例 - 模拟 DMA 场景
* 演示如何使用命令解析器
*****************************/
#include "cmdscan.h"
#include <stdio.h>
#include <string.h>

/*=============================================================
 * 模拟 DMA 缓冲区
 *=============================================================*/

/* 模拟 DMA 接收缓冲区 */
static uint8_t dma_buffer[512];
static uint16_t dma_write_pos = 0;

/* 模拟 DMA 接收数据 */
void simulate_dma_receive(const uint8_t* data, uint16_t len)
{
    /* 模拟数据写入 DMA 缓冲区 */
    for (uint16_t i = 0; i < len && dma_write_pos < sizeof(dma_buffer); i++)
    {
        dma_buffer[dma_write_pos++] = data[i];
    }
}

/* 重置 DMA 缓冲区 */
void reset_dma_buffer(void)
{
    dma_write_pos = 0;
}

/*=============================================================
 * 示例函数（RPC 处理函数）
 *=============================================================*/

void cmd_on(int32_t* channel)
{
    printf("[RPC] cmd_on(channel=%d)\r\n", *channel);
}

void cmd_off(int32_t* channel)
{
    printf("[RPC] cmd_off(channel=%d)\r\n", *channel);
}

void cmd_set_speed(int32_t* motor_id, int32_t* speed)
{
    printf("[RPC] cmd_set_speed(motor=%d, speed=%d)\r\n", *motor_id, *speed);
}

void cmd_get_status(void)
{
    printf("[RPC] cmd_get_status()\r\n");
}

/*=============================================================
 * 主程序 - 演示命令解析流程
 *=============================================================*/

void demo_single_command(void)
{
    printf("=== 单条命令解析示例 ===\r\n");

    /* 模拟接收一条完整命令 */
    const char* cmd_str = "cmd_on(1)\r\n";
    reset_dma_buffer();
    simulate_dma_receive((const uint8_t*)cmd_str, strlen(cmd_str));

    /* 初始化扫描器 */
    cmd_scanner_t scanner;
    cmd_init(&scanner, dma_buffer, dma_write_pos);

    /* 扫描提取命令 */
    cmd_entry_t entry;
    cmd_status_t status = cmd_scan(&scanner, &entry);

    if (status == CMD_COMPLETE)
    {
        printf("[SCAN] 找到完整命令，长度=%d\r\n", entry.cmd_len);

        /* 解析参数 */
        cmd_args_t result;
        uint8_t args_count = cmd_parse(&entry, &result);

        printf("[PARSE] 函数名: %.*s\r\n", result.func_name_len, result.func_name);
        printf("[PARSE] 参数个数: %d\r\n", args_count);

        for (uint8_t i = 0; i < args_count; i++)
        {
            printf("[PARSE]   arg[%d]: %.*s\r\n",
                   i, result.args[i].len, result.args[i].ptr);
        }
    }

    printf("\r\n");
}

void demo_multi_command(void)
{
    printf("=== 多命令解析示例（模拟 DMA 连续接收）===\r\n");

    /* 模拟接收多条命令 */
    const char* cmds = "cmd_on(1)\r\ncmd_set_speed(0, 100)\r\ncmd_get_status()\r\n";
    reset_dma_buffer();
    simulate_dma_receive((const uint8_t*)cmds, strlen(cmds));

    /* 初始化扫描器 */
    cmd_scanner_t scanner;
    cmd_init(&scanner, dma_buffer, dma_write_pos);

    /* 循环扫描提取所有命令 */
    uint8_t cmd_count = 0;
    cmd_entry_t entry;

    while (cmd_scan(&scanner, &entry) == CMD_COMPLETE)
    {
        cmd_count++;
        printf("[SCAN] 命令 %d: ", cmd_count);

        /* 输出命令内容 */
        for (uint16_t i = 0; i < entry.cmd_len; i++)
        {
            putchar(dma_buffer[entry.cmd_start + i]);
        }
        printf("\r\n");

        /* 解析参数 */
        cmd_args_t result;
        uint8_t args_count = cmd_parse(&entry, &result);

        printf("[PARSE]   函数: %.*s, 参数: %d\r\n",
               result.func_name_len, result.func_name, args_count);
    }

    printf("[SCAN] 扫描结束，共处理 %d 条命令\r\n\r\n", cmd_count);
}

void demo_incremental_receive(void)
{
    printf("=== 增量接收示例（模拟 DMA 分片接收，缓冲后批量解析）===\r\n");

    /* 模拟分片接收的命令 */
    const char* part1 = "cmd_set_";
    const char* part2 = "speed(2,";
    const char* part3 = " 50)\r\n";

    reset_dma_buffer();

    /* 初始化扫描器 */
    cmd_scanner_t scanner;
    cmd_init(&scanner, dma_buffer, sizeof(dma_buffer));

    /* 第一次接收 */
    printf("[DMA] 接收第1片: %s\r\n", part1);
    simulate_dma_receive((const uint8_t*)part1, strlen(part1));
    scanner.buf_size = dma_write_pos;

    /* 第二次接收 */
    printf("[DMA] 接收第2片: %s\r\n", part2);
    simulate_dma_receive((const uint8_t*)part2, strlen(part2));
    scanner.buf_size = dma_write_pos;

    /* 第三次接收 */
    printf("[DMA] 接收第3片: %s\r\n", part3);
    simulate_dma_receive((const uint8_t*)part3, strlen(part3));
    scanner.buf_size = dma_write_pos;

    /* 缓冲完成后，使用 entry 解析 */
    cmd_entry_t entry;
    cmd_status_t status = cmd_scan(&scanner, &entry);

    if (status == CMD_COMPLETE)
    {
        printf("[SCAN] 找到完整命令，长度=%d\r\n", entry.cmd_len);

        /* 解析参数 */
        cmd_args_t result;
        uint8_t args_count = cmd_parse(&entry, &result);

        printf("[PARSE] 函数名: %.*s\r\n", result.func_name_len, result.func_name);
        printf("[PARSE] 参数个数: %d\r\n", args_count);

        for (uint8_t i = 0; i < args_count; i++)
        {
            printf("[PARSE]   arg[%d]: %.*s\r\n",
                   i, result.args[i].len, result.args[i].ptr);
        }
    }

    printf("\r\n");
}

void demo_zero_copy(void)
{
    printf("=== 零拷贝验证 ===\r\n");
    
    /* 模拟一条命令 */
    char cmd_buf[] = "cmd_set_speed(3, 200)";
    uint16_t cmd_len = sizeof(cmd_buf) - 1; /* 不含 \0 */
    
    /* 解析参数 */
    cmd_args_t result;
    cmd_entry_t e = { (const uint8_t*)cmd_buf, cmd_len, 0, cmd_len, 0 };
    uint8_t args_count = cmd_parse(&e, &result);
    
    printf("[VERIFY] 原始缓冲区地址: %p\r\n", (void*)cmd_buf);
    printf("[VERIFY] 函数名指针: %p\r\n", (void*)result.func_name);
    printf("[VERIFY] arg[0] 指针: %p\r\n", (void*)result.args[0].ptr);
    printf("[VERIFY] arg[1] 指针: %p\r\n", (void*)result.args[1].ptr);
    
    /* 验证指针指向原始缓冲区 */
    if (result.func_name >= cmd_buf && result.func_name < cmd_buf + cmd_len)
    {
        printf("[VERIFY] 函数名指针指向原始缓冲区 ✓\r\n");
    }
    else
    {
        printf("[VERIFY] 函数名指针不指向原始缓冲区 ✗\r\n");
    }
    
    if (result.args[0].ptr >= cmd_buf && result.args[0].ptr < cmd_buf + cmd_len)
    {
        printf("[VERIFY] arg[0] 指针指向原始缓冲区 ✓\r\n");
    }
    else
    {
        printf("[VERIFY] arg[0] 指针不指向原始缓冲区 ✗\r\n");
    }
    
    printf("\r\n");
}

void demo_format_comparison(void)
{
    printf("=== 命令格式对比 ===\r\n");
    
    /* 测试有括号形式 */
    char cmd1[] = "print(123)";
    cmd_args_t result1;
    cmd_entry_t e1 = { (const uint8_t*)cmd1, (uint16_t)strlen(cmd1), 0, (uint16_t)strlen(cmd1), 0 };
    uint8_t args1 = cmd_parse(&e1, &result1);
    printf("[FORMAT] print(123): 函数=%.*s, 参数=%d\r\n", 
           result1.func_name_len, result1.func_name, args1);
    if (args1 > 0) {
        printf("[FORMAT]   arg[0]: %.*s\r\n", result1.args[0].len, result1.args[0].ptr);
    }
    
    /* 测试无括号形式 */
    char cmd2[] = "print 456";
    cmd_args_t result2;
    cmd_entry_t e2 = { (const uint8_t*)cmd2, (uint16_t)strlen(cmd2), 0, (uint16_t)strlen(cmd2), 0 };
    uint8_t args2 = cmd_parse(&e2, &result2);
    printf("[FORMAT] print 456: 函数=%.*s, 参数=%d\r\n",
           result2.func_name_len, result2.func_name, args2);
    if (args2 > 0) {
        printf("[FORMAT]   arg[0]: %.*s\r\n", result2.args[0].len, result2.args[0].ptr);
    }
    
    /* 测试多参数无括号形式 */
    char cmd3[] = "set 100 200 300";
    cmd_args_t result3;
    cmd_entry_t e3 = { (const uint8_t*)cmd3, (uint16_t)strlen(cmd3), 0, (uint16_t)strlen(cmd3), 0 };
    uint8_t args3 = cmd_parse(&e3, &result3);
    printf("[FORMAT] set 100 200 300: 函数=%.*s, 参数=%d\r\n",
           result3.func_name_len, result3.func_name, args3);
    for (uint8_t i = 0; i < args3; i++) {
        printf("[FORMAT]   arg[%d]: %.*s\r\n", i, result3.args[i].len, result3.args[i].ptr);
    }
    
    printf("\r\n");
}

void demo_entry(void)
{
    printf("=== 扫描提取示例（cmd_scan）===\r\n");

    /* 模拟包含多条命令的缓冲区 */
    const char* buf = "led_on(1)\nstop\nprint 1 2\n";
    uint16_t buf_len = strlen(buf);

    cmd_scanner_t scanner;
    cmd_init(&scanner, (const uint8_t*)buf, buf_len);

    uint8_t cmd_count = 0;
    cmd_entry_t entry;

    while (cmd_scan(&scanner, &entry) == CMD_COMPLETE)
    {
        cmd_count++;
        printf("[SCAN] 命令 %d: start=%d, len=%d, func_len=%d -> ",
               cmd_count, entry.cmd_start, entry.cmd_len, entry.func_len);

        /* 输出函数名 */
        for (uint8_t i = 0; i < entry.func_len; i++)
            putchar(entry.buf[entry.cmd_start + i]);
        printf("\r\n");
    }

    printf("[SCAN] 共预解析 %d 条命令\r\n\r\n", cmd_count);
}

int main(void)
{
    printf("CMD 零拷贝命令解析器示例\r\n");
    printf("================================\r\n\r\n");
    
    /* 运行示例 */
    demo_single_command();
    demo_multi_command();
    demo_incremental_receive();
    demo_entry();
    demo_zero_copy();
    demo_format_comparison();
    
    return 0;
}
