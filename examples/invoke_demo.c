/****************************
 * INVOKE Demo - 调度执行引擎演示
 *
 * 演示 invoke 模块功能：
 * 1. 完整管线：cmd_parse + invoke_call
 * 2. 各种参数类型
 * 3. 返回值处理
 *****************************/
#include "invoke.h"
#include <stdio.h>
#include <string.h>

/*=============================================================
 * 示例 RPC 处理函数
 *=============================================================*/
static void hello(void)
{
    printf("[RPC] hello()\n");
}

static int64_t add(void* a, void* b)
{
    return *(int64_t*)a + *(int64_t*)b;
}

static void led_on(void* channel)
{
    printf("[RPC] LED_On(channel=%d)\n", *(int64_t*)channel);
}

static void motor_set(void* id, void* speed)
{
    printf("[RPC] motor_set(id=%d, speed=%d)\n", *(int64_t*)id, *(int64_t*)speed);
}

/*=============================================================
 * 主函数
 *=============================================================*/
int main(void)
{
    static dispatch_registry_t dispatcher;
    invoke_ret_t ret;
    dispatch_status_t s;

    printf("=== Invoke Demo ===\n\n");

    /* 1. 注册函数 */
    dispatch_init(&dispatcher);
    dispatch_reg(&dispatcher, hello, "hello()");
    dispatch_reg(&dispatcher, add, "add(i, i) -> i");
    dispatch_reg(&dispatcher, led_on, "LED_On(i)");
    dispatch_reg(&dispatcher, motor_set, "motor_set(i, i)");
    printf("Registered 4 functions\n\n");

    /* 2. 调用无参无返回值函数 */
    printf("--- Call hello() ---\n");
    cmd_args_t args;
    cmd_entry_t e;
    e = cmd_entry_from_str("hello()", 7);
    cmd_parse(&e, &args);
    s = invoke_call(&dispatcher, &args, &ret);
    printf("status=%d, ret_type=%d\n\n", s, ret.type);

    /* 3. 调用有返回值函数 */
    printf("--- Call add(10, 20) ---\n");
    e = cmd_entry_from_str("add(10, 20)", 11);
    cmd_parse(&e, &args);
    s = invoke_call(&dispatcher, &args, &ret);
    printf("status=%d, ret.i64=%lld\n\n", s, ret.i64);

    /* 4. 调用负数参数 */
    printf("--- Call add(-100, 50) ---\n");
    e = cmd_entry_from_str("add(-100, 50)", 13);
    cmd_parse(&e, &args);
    s = invoke_call(&dispatcher, &args, &ret);
    printf("status=%d, ret.i64=%lld\n\n", s, ret.i64);

    /* 5. 调用单参数函数 */
    printf("--- Call LED_On(3) ---\n");
    e = cmd_entry_from_str("LED_On(3)", 9);
    cmd_parse(&e, &args);
    s = invoke_call(&dispatcher, &args, &ret);
    printf("status=%d\n\n", s);

    /* 6. 调用双参数函数 */
    printf("--- Call motor_set(1, 1000) ---\n");
    e = cmd_entry_from_str("motor_set(1, 1000)", 18);
    cmd_parse(&e, &args);
    s = invoke_call(&dispatcher, &args, &ret);
    printf("status=%d\n\n", s);

    /* 7. 调用不存在的函数 */
    printf("--- Call nonexistent() ---\n");
    e = cmd_entry_from_str("nonexistent()", 13);
    cmd_parse(&e, &args);
    s = invoke_call(&dispatcher, &args, &ret);
    printf("status=%d (expected DISPATCH_ERR_NOT_FOUND)\n\n", s);

    /* 8. 完整管线演示（模拟 DMA 输入） */
    printf("--- Full pipeline ---\n");
    const char* cmd = "add(100, 200)";
    printf("Input: '%s'\n", cmd);
    e = cmd_entry_from_str(cmd, strlen(cmd));
    cmd_parse(&e, &args);
    printf("Parsed: func='%.*s', args_count=%d\n", args.func_name_len, args.func_name, args.args_count);
    s = invoke_call(&dispatcher, &args, &ret);
    printf("Result: status=%d, value=%lld\n", s, ret.i64);

    printf("\n=== Demo Complete ===\n");
    return 0;
}
