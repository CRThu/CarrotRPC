/****************************
 * RPC_LOG Demo - 分级日志演示
 * CarrotRPC
 *
 * 演示 rpc_log 库的全部功能：
 * 1. 字节模式输出
 * 2. 各级别输出
 * 3. 类型输出 (i64/u64/hex/f64)
 * 4. 格式化输出
 * 5. 级别过滤
 * 6. 协议级别
 *****************************/
#include "rpclog.h"
#include <stdio.h>

/*=============================================================
 * 输出回调：逐字节输出到 stdout
 *=============================================================*/
static void my_putc(char c)
{
    putchar(c);
}

/*=============================================================
 * 输出回调：缓冲区模式 (演示用)
 *=============================================================*/
#if RPC_LOG_OUTPUT_BUF
static void my_buf_out(const char* buf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
        putchar(buf[i]);
}
#endif

/*=============================================================
 * 示例函数
 *=============================================================*/
static int64_t add(int64_t a, int64_t b)
{
    return a + b;
}

/*=============================================================
 * 主函数
 *=============================================================*/
int main(void)
{
    /* 1. 设置输出回调 */
#if RPC_LOG_OUTPUT_BUF
    rpc_log_set_output(my_buf_out);
#else
    rpc_log_set_output(my_putc);
#endif

    printf("=== RpcLog Demo ===\n\n");

    /* 2. 各级别输出 */
    printf("--- Basic levels ---\n");
    rpc_debug("system starting");
    rpc_info("system ready");
    rpc_warning("low memory");
    rpc_error("critical failure");

    /* 3. 类型输出 */
    printf("\n--- Type output ---\n");
    rpc_info_i64("speed", 100);
    rpc_info_u64("flags", 0xFF);
    rpc_info_hex("addr", 0xDEADBEEF);
    rpc_info_f64("ratio", 3.14, 2);

    /* 4. 格式化输出 */
    printf("\n--- Formatted output ---\n");
    rpc_info("version=%d.%d", 2, 1);
    rpc_error("arg mismatch: expected %d, got %d", 3, 2);
    rpc_info("name=%s", "CarrotRPC");

    /* 5. 级别过滤 */
    printf("\n--- Level filtering (set to WARN) ---\n");
    rpc_log_set_level(RPC_LOG_WARN);
    rpc_debug("this should NOT appear");
    rpc_info("this should NOT appear");
    rpc_warning("this SHOULD appear");
    rpc_error("this SHOULD appear");

    /* 6. 协议级别（始终输出） */
    printf("\n--- Protocol levels (always output) ---\n");
    rpc_return("add(10,20) = 30");
    rpc_data("key=value");
    rpc_reg("add(i,i)->i registered");

    /* 7. 恢复级别，展示类型宏 */
    printf("\n--- Type macros at WARN level ---\n");
    rpc_log_set_level(RPC_LOG_DEBUG);
    rpc_warn_i64("val", -42);
    rpc_warn_hex("mask", 0xABCD);
    rpc_warn_f64("temp", -20.5, 1);

    printf("\n=== Demo Complete ===\n");
    return 0;
}
