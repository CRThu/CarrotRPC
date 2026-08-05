/**
 * test_invoke.c — Unit tests + E2E pipeline tests for invoke module
 *
 * Tests the complete new pipeline:
 *   DMA buffer → cmd_scan → cmd_queue_push → cmd_queue_pop
 *   → cmd_parse → dispatch_find → invoke_call
 */
#include "unity.h"
#include "test_invoke_helpers.h"
#include "invoke.h"
#include "cmdscan.h"
#include "cmdqueue.h"
#include <string.h>

/* ===== Test setup ===== */

static void invoke_setUp(void)
{
    invoke_test_helpers_reset();
}

/* ===== Helper: run full pipeline ===== */

static dispatch_status_t run_pipeline(const char* buf, uint16_t len)
{
    static uint8_t inv_q_buf[2048];
    cmd_scanner_t scanner;
    cmd_init(&scanner, (const uint8_t*)buf, len);

    cmd_queue_t queue;
    cmd_queue_init(&queue, inv_q_buf, sizeof(inv_q_buf));

    cmd_entry_t entry;
    while (cmd_scan(&scanner, &entry) == CMD_COMPLETE)
    {
        cmd_queue_push(&queue, &entry);
    }

    dispatch_status_t last_status = DISPATCH_OK;
    while (!cmd_queue_is_empty(&queue))
    {
        cmd_queue_pop(&queue, &entry);

        cmd_args_t result;
        cmd_parse((const char*)entry.buf + entry.cmd_start, entry.cmd_len, &result);

        last_status = invoke_call(&invoke_dispatcher, &result, NULL);
    }

    return last_status;
}

/* =================================================================
 * GROUP 1: NULL / edge cases
 * ================================================================= */

void test_icall_null_result(void)
{
    invoke_setUp();
    dispatch_status_t s = invoke_call(&invoke_dispatcher, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_NULL, s);
}

void test_icall_null_func_name(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, NULL);
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_NULL, s);
}

void test_icall_func_not_found(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "nonexistent";
    result.func_name_len = strlen("nonexistent");
    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, NULL);
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_NOT_FOUND, s);
}

/* =================================================================
 * GROUP 2: 0-arg function
 * ================================================================= */

void test_icall_hello(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "hello";
    result.func_name_len = 5;
    result.args_count = 0;

    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, NULL);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

/* =================================================================
 * GROUP 3: 1-arg DEC64
 * ================================================================= */

void test_icall_dec_positive(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "dec";
    result.func_name_len = 3;
    result.args[0].ptr = "42";
    result.args[0].len = 2;
    result.args_count = 1;

    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, NULL);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_dec_fake.call_count);

    /* p[i] points directly to int64_t — single dereference */
    int64_t val = *(int64_t*)invoke_mock_dec_fake.arg0_val;
    TEST_ASSERT_EQUAL_INT64(42, val);
}

void test_icall_dec_negative(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "dec";
    result.func_name_len = 3;
    result.args[0].ptr = "-100";
    result.args[0].len = 4;
    result.args_count = 1;

    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, NULL);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    int64_t val = *(int64_t*)invoke_mock_dec_fake.arg0_val;
    TEST_ASSERT_EQUAL_INT64(-100, val);
}

void test_icall_dec_zero(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "dec";
    result.func_name_len = 3;
    result.args[0].ptr = "0";
    result.args[0].len = 1;
    result.args_count = 1;

    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, NULL);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    int64_t val = *(int64_t*)invoke_mock_dec_fake.arg0_val;
    TEST_ASSERT_EQUAL_INT64(0, val);
}

/* =================================================================
 * GROUP 4: 1-arg HEX64
 * ================================================================= */

void test_icall_hex_basic(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "hex";
    result.func_name_len = 3;
    result.args[0].ptr = "FF";
    result.args[0].len = 2;
    result.args_count = 1;

    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, NULL);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    uint64_t val = *(uint64_t*)invoke_mock_hex_fake.arg0_val;
    TEST_ASSERT_EQUAL_HEX64(0xFF, val);
}

void test_icall_hex_with_prefix(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "hex";
    result.func_name_len = 3;
    result.args[0].ptr = "0xDEAD";
    result.args[0].len = 6;
    result.args_count = 1;

    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, NULL);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    uint64_t val = *(uint64_t*)invoke_mock_hex_fake.arg0_val;
    TEST_ASSERT_EQUAL_HEX64(0xDEAD, val);
}

/* =================================================================
 * GROUP 5: 1-arg STRING
 * ================================================================= */

void test_icall_string_basic(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "str";
    result.func_name_len = 3;
    result.args[0].ptr = "hello";
    result.args[0].len = 5;
    result.args_count = 1;

    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, NULL);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);

    /* p[i] points to null-terminated str_buf — single dereference */
    const char* val = (const char*)invoke_mock_string_fake.arg0_val;
    TEST_ASSERT_EQUAL_STRING("hello", val);
}

/* =================================================================
 * GROUP 6: 2-arg DEC64 (int64 return)
 * ================================================================= */

void test_icall_add(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "add";
    result.func_name_len = 3;
    result.args[0].ptr = "10";
    result.args[0].len = 2;
    result.args[1].ptr = "20";
    result.args[1].len = 2;
    result.args_count = 2;

    invoke_ret_t ret;
    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, &ret);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_add_fake.call_count);

    int64_t a = *(int64_t*)invoke_mock_add_fake.arg0_val;
    int64_t b = *(int64_t*)invoke_mock_add_fake.arg1_val;
    TEST_ASSERT_EQUAL_INT64(10, a);
    TEST_ASSERT_EQUAL_INT64(20, b);
}

/* =================================================================
 * GROUP 7: 3-arg STRING
 * ================================================================= */

void test_icall_3strings(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "args";
    result.func_name_len = 4;
    result.args[0].ptr = "alpha";
    result.args[0].len = 5;
    result.args[1].ptr = "beta";
    result.args[1].len = 4;
    result.args[2].ptr = "gamma";
    result.args[2].len = 5;
    result.args_count = 3;

    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, NULL);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_args_fake.call_count);

    const char* a0 = (const char*)invoke_mock_args_fake.arg0_val;
    const char* a1 = (const char*)invoke_mock_args_fake.arg1_val;
    const char* a2 = (const char*)invoke_mock_args_fake.arg2_val;
    TEST_ASSERT_EQUAL_STRING("alpha", a0);
    TEST_ASSERT_EQUAL_STRING("beta", a1);
    TEST_ASSERT_EQUAL_STRING("gamma", a2);
}

/* =================================================================
 * GROUP 8: arg count mismatch
 * ================================================================= */

void test_icall_arg_mismatch_too_many(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "hello";  /* 0 args expected */
    result.func_name_len = 5;
    result.args[0].ptr = "extra";
    result.args[0].len = 5;
    result.args_count = 1;

    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, NULL);
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_SIG, s);
    TEST_ASSERT_EQUAL_INT(0, invoke_mock_hello_fake.call_count);
}

void test_icall_arg_mismatch_too_few(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "add";  /* 2 args expected */
    result.func_name_len = 3;
    result.args[0].ptr = "10";
    result.args[0].len = 2;
    result.args_count = 1;

    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, NULL);
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_SIG, s);
    TEST_ASSERT_EQUAL_INT(0, invoke_mock_add_fake.call_count);
}

/* =================================================================
 * GROUP 9: Return value capture
 * ================================================================= */

void test_icall_ret_none(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "hello";
    result.func_name_len = 5;
    result.args_count = 0;

    invoke_ret_t ret;
    ret.type = INVOKERET_STR;  /* sentinel */
    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, &ret);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(INVOKERET_NONE, ret.type);
}

void test_icall_ret_i64(void)
{
    invoke_setUp();
    cmd_args_t result;
    memset(&result, 0, sizeof(result));
    result.func_name = "add";
    result.func_name_len = 3;
    result.args[0].ptr = "3";
    result.args[0].len = 1;
    result.args[1].ptr = "7";
    result.args[1].len = 1;
    result.args_count = 2;

    invoke_ret_t ret;
    dispatch_status_t s = invoke_call(&invoke_dispatcher, &result, &ret);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(INVOKERET_I64, ret.type);
    /* mock_add returns 0 by default (fff init), but ret type is correct */
}

/* =================================================================
 * GROUP 10: Full E2E pipeline (prefetch → queue → parse → invoke)
 * ================================================================= */

void test_icall_pipeline_hello(void)
{
    invoke_setUp();
    dispatch_status_t s = run_pipeline("hello()\n", 8);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

void test_icall_pipeline_hello_no_parens(void)
{
    invoke_setUp();
    dispatch_status_t s = run_pipeline("hello\n", 6);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

void test_icall_pipeline_dec(void)
{
    invoke_setUp();
    dispatch_status_t s = run_pipeline("dec(42)\n", 8);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT64(42, invoke_captured.dec_val);
}

void test_icall_pipeline_dec_negative(void)
{
    invoke_setUp();
    dispatch_status_t s = run_pipeline("dec(-100)\n", 10);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT64(-100, invoke_captured.dec_val);
}

void test_icall_pipeline_hex(void)
{
    invoke_setUp();
    dispatch_status_t s = run_pipeline("hex(0xFF)\n", 10);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_HEX64(0xFF, invoke_captured.hex_val);
}

void test_icall_pipeline_string(void)
{
    invoke_setUp();
    dispatch_status_t s = run_pipeline("str(hello)\n", 11);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_STRING("hello", invoke_captured.str_val);
}

void test_icall_pipeline_add(void)
{
    invoke_setUp();
    dispatch_status_t s = run_pipeline("add(10,20)\n", 11);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT64(10, invoke_captured.add_a);
    TEST_ASSERT_EQUAL_INT64(20, invoke_captured.add_b);
}

void test_icall_pipeline_args(void)
{
    invoke_setUp();
    dispatch_status_t s = run_pipeline("args(a,b,c)\n", 12);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_STRING("a", invoke_captured.arg0);
    TEST_ASSERT_EQUAL_STRING("b", invoke_captured.arg1);
    TEST_ASSERT_EQUAL_STRING("c", invoke_captured.arg2);
}

void test_icall_pipeline_two_commands(void)
{
    invoke_setUp();
    const char* buf = "hello()\nhello()\n";
    dispatch_status_t s = run_pipeline(buf, (uint16_t)strlen(buf));
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(2, invoke_mock_hello_fake.call_count);
}

void test_icall_pipeline_mixed_commands(void)
{
    invoke_setUp();
    const char* buf = "hello()\ndec(42)\nhex(FF)\n";
    dispatch_status_t s = run_pipeline(buf, (uint16_t)strlen(buf));
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);

    TEST_ASSERT_EQUAL_INT64(42, invoke_captured.dec_val);
    TEST_ASSERT_EQUAL_HEX64(0xFF, invoke_captured.hex_val);
}

void test_icall_pipeline_not_found(void)
{
    invoke_setUp();
    const char* buf = "nonexistent()\n";
    dispatch_status_t s = run_pipeline(buf, (uint16_t)strlen(buf));
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_NOT_FOUND, s);
}

int run_invoke_tests(void)
{
    UNITY_BEGIN();

    /* NULL / edge */
    RUN_TEST(test_icall_null_result);
    RUN_TEST(test_icall_null_func_name);
    RUN_TEST(test_icall_func_not_found);

    /* 0-arg */
    RUN_TEST(test_icall_hello);

    /* 1-arg DEC64 */
    RUN_TEST(test_icall_dec_positive);
    RUN_TEST(test_icall_dec_negative);
    RUN_TEST(test_icall_dec_zero);

    /* 1-arg HEX64 */
    RUN_TEST(test_icall_hex_basic);
    RUN_TEST(test_icall_hex_with_prefix);

    /* 1-arg STRING */
    RUN_TEST(test_icall_string_basic);

    /* 2-arg DEC64 */
    RUN_TEST(test_icall_add);

    /* 3-arg STRING */
    RUN_TEST(test_icall_3strings);

    /* arg count mismatch */
    RUN_TEST(test_icall_arg_mismatch_too_many);
    RUN_TEST(test_icall_arg_mismatch_too_few);

    /* return value */
    RUN_TEST(test_icall_ret_none);
    RUN_TEST(test_icall_ret_i64);

    /* E2E pipeline */
    RUN_TEST(test_icall_pipeline_hello);
    RUN_TEST(test_icall_pipeline_hello_no_parens);
    RUN_TEST(test_icall_pipeline_dec);
    RUN_TEST(test_icall_pipeline_dec_negative);
    RUN_TEST(test_icall_pipeline_hex);
    RUN_TEST(test_icall_pipeline_string);
    RUN_TEST(test_icall_pipeline_add);
    RUN_TEST(test_icall_pipeline_args);
    RUN_TEST(test_icall_pipeline_two_commands);
    RUN_TEST(test_icall_pipeline_mixed_commands);
    RUN_TEST(test_icall_pipeline_not_found);

    return UNITY_END();
}
