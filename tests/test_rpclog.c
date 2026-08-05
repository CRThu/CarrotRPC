/**
 * test_rpclog.c — Unit tests for rpc_log module
 */
#include "unity.h"
#include "rpclog.h"
#include <string.h>

/* ============================================================
 * Test output capture
 * ============================================================ */

#define CAP_BUF_SIZE 512
static char cap_buf[CAP_BUF_SIZE];
static uint16_t cap_pos = 0;

static void cap_reset(void)
{
    memset(cap_buf, 0, CAP_BUF_SIZE);
    cap_pos = 0;
}

static void cap_putc(char c)
{
    if (cap_pos < CAP_BUF_SIZE - 1)
        cap_buf[cap_pos++] = c;
}

#if RPC_LOG_OUTPUT_BUF
static void cap_buf_out(const char* buf, uint16_t len)
{
    uint16_t to_write = len;
    if (cap_pos + to_write > CAP_BUF_SIZE - 1)
        to_write = CAP_BUF_SIZE - 1 - cap_pos;
    memcpy(cap_buf + cap_pos, buf, to_write);
    cap_pos += to_write;
}
#endif

static void rpc_log_setUp(void)
{
    cap_reset();
#if RPC_LOG_OUTPUT_BUF
    rpc_log_set_output(cap_buf_out);
#else
    rpc_log_set_output(cap_putc);
#endif
    rpc_log_set_level(RPC_LOG_DEBUG);
}

/* ============================================================
 * GROUP 1: Basic output format
 * ============================================================ */

void test_rpc_log_debug_format(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_DEBUG, "hello");
    TEST_ASSERT_EQUAL_STRING("[DEBUG]: hello\r\n", cap_buf);
}

void test_rpc_log_info_format(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_INFO, "started");
    TEST_ASSERT_EQUAL_STRING("[INFO]: started\r\n", cap_buf);
}

void test_rpc_log_warn_format(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_WARN, "low");
    TEST_ASSERT_EQUAL_STRING("[WARN]: low\r\n", cap_buf);
}

void test_rpc_log_error_format(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_ERROR, "fail");
    TEST_ASSERT_EQUAL_STRING("[ERROR]: fail\r\n", cap_buf);
}

void test_rpc_log_return_format(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_RETURN, "result=42");
    TEST_ASSERT_EQUAL_STRING("[RETURN]: result=42\r\n", cap_buf);
}

void test_rpc_log_data_format(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_DATA, "key=val");
    TEST_ASSERT_EQUAL_STRING("[DATA]: key=val\r\n", cap_buf);
}

void test_rpc_log_reg_format(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_REG, "add registered");
    TEST_ASSERT_EQUAL_STRING("[REG]: add registered\r\n", cap_buf);
}

/* ============================================================
 * GROUP 2: Convenience macros (text)
 * ============================================================ */

void test_rpc_debug_macro(void)
{
    rpc_log_setUp();
    rpc_debug("msg");
    TEST_ASSERT_EQUAL_STRING("[DEBUG]: msg\r\n", cap_buf);
}

void test_rpc_info_macro(void)
{
    rpc_log_setUp();
    rpc_info("msg");
    TEST_ASSERT_EQUAL_STRING("[INFO]: msg\r\n", cap_buf);
}

void test_rpc_warning_macro(void)
{
    rpc_log_setUp();
    rpc_warning("msg");
    TEST_ASSERT_EQUAL_STRING("[WARN]: msg\r\n", cap_buf);
}

void test_rpc_error_macro(void)
{
    rpc_log_setUp();
    rpc_error("msg");
    TEST_ASSERT_EQUAL_STRING("[ERROR]: msg\r\n", cap_buf);
}

void test_rpc_return_macro(void)
{
    rpc_log_setUp();
    rpc_return("msg");
    TEST_ASSERT_EQUAL_STRING("[RETURN]: msg\r\n", cap_buf);
}

void test_rpc_data_macro(void)
{
    rpc_log_setUp();
    rpc_data("msg");
    TEST_ASSERT_EQUAL_STRING("[DATA]: msg\r\n", cap_buf);
}

void test_rpc_reg_macro(void)
{
    rpc_log_setUp();
    rpc_reg("msg");
    TEST_ASSERT_EQUAL_STRING("[REG]: msg\r\n", cap_buf);
}

/* ============================================================
 * GROUP 3: Level filtering
 * ============================================================ */

void test_level_filter_debug_blocked(void)
{
    rpc_log_setUp();
    rpc_log_set_level(RPC_LOG_INFO);
    rpc_log(RPC_LOG_DEBUG, "should not appear");
    TEST_ASSERT_EQUAL_STRING("", cap_buf);
}

void test_level_filter_info_allowed(void)
{
    rpc_log_setUp();
    rpc_log_set_level(RPC_LOG_INFO);
    rpc_log(RPC_LOG_INFO, "visible");
    TEST_ASSERT_EQUAL_STRING("[INFO]: visible\r\n", cap_buf);
}

void test_level_filter_warn_allowed(void)
{
    rpc_log_setUp();
    rpc_log_set_level(RPC_LOG_WARN);
    rpc_log(RPC_LOG_WARN, "visible");
    TEST_ASSERT_EQUAL_STRING("[WARN]: visible\r\n", cap_buf);
}

void test_level_filter_error_allowed(void)
{
    rpc_log_setUp();
    rpc_log_set_level(RPC_LOG_ERROR);
    rpc_log(RPC_LOG_ERROR, "visible");
    TEST_ASSERT_EQUAL_STRING("[ERROR]: visible\r\n", cap_buf);
}

void test_level_filter_below_blocked(void)
{
    rpc_log_setUp();
    rpc_log_set_level(RPC_LOG_ERROR);
    rpc_log(RPC_LOG_DEBUG, "no");
    rpc_log(RPC_LOG_INFO, "no");
    rpc_log(RPC_LOG_WARN, "no");
    TEST_ASSERT_EQUAL_STRING("", cap_buf);
}

/* ============================================================
 * GROUP 4: Protocol levels always output
 * ============================================================ */

void test_protocol_return_always_output(void)
{
    rpc_log_setUp();
    rpc_log_set_level(RPC_LOG_ERROR);
    rpc_log(RPC_LOG_RETURN, "always");
    TEST_ASSERT_EQUAL_STRING("[RETURN]: always\r\n", cap_buf);
}

void test_protocol_data_always_output(void)
{
    rpc_log_setUp();
    rpc_log_set_level(RPC_LOG_ERROR);
    rpc_log(RPC_LOG_DATA, "always");
    TEST_ASSERT_EQUAL_STRING("[DATA]: always\r\n", cap_buf);
}

void test_protocol_reg_always_output(void)
{
    rpc_log_setUp();
    rpc_log_set_level(RPC_LOG_ERROR);
    rpc_log(RPC_LOG_REG, "always");
    TEST_ASSERT_EQUAL_STRING("[REG]: always\r\n", cap_buf);
}

/* ============================================================
 * GROUP 5: Type output — i64
 * ============================================================ */

void test_rpc_log_i64_positive(void)
{
    rpc_log_setUp();
    rpc_log_i64(RPC_LOG_INFO, "val", 42);
    TEST_ASSERT_EQUAL_STRING("[INFO]: val=42\r\n", cap_buf);
}

void test_rpc_log_i64_negative(void)
{
    rpc_log_setUp();
    rpc_log_i64(RPC_LOG_INFO, "val", -100);
    TEST_ASSERT_EQUAL_STRING("[INFO]: val=-100\r\n", cap_buf);
}

void test_rpc_log_i64_zero(void)
{
    rpc_log_setUp();
    rpc_log_i64(RPC_LOG_INFO, "val", 0);
    TEST_ASSERT_EQUAL_STRING("[INFO]: val=0\r\n", cap_buf);
}

void test_rpc_info_i64_macro(void)
{
    rpc_log_setUp();
    rpc_info_i64("speed", 100);
    TEST_ASSERT_EQUAL_STRING("[INFO]: speed=100\r\n", cap_buf);
}

/* ============================================================
 * GROUP 6: Type output — u64
 * ============================================================ */

void test_rpc_log_u64_value(void)
{
    rpc_log_setUp();
    rpc_log_u64(RPC_LOG_INFO, "flags", 0xFF);
    TEST_ASSERT_EQUAL_STRING("[INFO]: flags=0xFF\r\n", cap_buf);
}

void test_rpc_log_u64_zero(void)
{
    rpc_log_setUp();
    rpc_log_u64(RPC_LOG_INFO, "flags", 0);
    TEST_ASSERT_EQUAL_STRING("[INFO]: flags=0x0\r\n", cap_buf);
}

/* ============================================================
 * GROUP 7: Type output — hex
 * ============================================================ */

void test_rpc_log_hex_deadbeef(void)
{
    rpc_log_setUp();
    rpc_log_hex(RPC_LOG_INFO, "addr", 0xDEADBEEF);
    TEST_ASSERT_EQUAL_STRING("[INFO]: addr=0xDEADBEEF\r\n", cap_buf);
}

void test_rpc_log_hex_zero(void)
{
    rpc_log_setUp();
    rpc_log_hex(RPC_LOG_INFO, "addr", 0);
    TEST_ASSERT_EQUAL_STRING("[INFO]: addr=0x0\r\n", cap_buf);
}

void test_rpc_info_hex_macro(void)
{
    rpc_log_setUp();
    rpc_info_hex("addr", 0xCAFE);
    TEST_ASSERT_EQUAL_STRING("[INFO]: addr=0xCAFE\r\n", cap_buf);
}

/* ============================================================
 * GROUP 8: Type output — f64
 * ============================================================ */

void test_rpc_log_f64_positive(void)
{
    rpc_log_setUp();
    rpc_log_f64(RPC_LOG_INFO, "ratio", 3.14, 2);
    TEST_ASSERT_EQUAL_STRING("[INFO]: ratio=3.14\r\n", cap_buf);
}

void test_rpc_log_f64_negative(void)
{
    rpc_log_setUp();
    rpc_log_f64(RPC_LOG_INFO, "temp", -20.5, 1);
    TEST_ASSERT_EQUAL_STRING("[INFO]: temp=-20.5\r\n", cap_buf);
}

void test_rpc_log_f64_integer(void)
{
    rpc_log_setUp();
    rpc_log_f64(RPC_LOG_INFO, "count", 100.0, 0);
    TEST_ASSERT_EQUAL_STRING("[INFO]: count=100\r\n", cap_buf);
}

void test_rpc_info_f64_macro(void)
{
    rpc_log_setUp();
    rpc_info_f64("ratio", 1.5, 2);
    TEST_ASSERT_EQUAL_STRING("[INFO]: ratio=1.5\r\n", cap_buf);
}

/* ============================================================
 * GROUP 9: fmt formatting (via unified rpc_log)
 * ============================================================ */

void test_rpc_log_string(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_INFO, "hello world");
    TEST_ASSERT_EQUAL_STRING("[INFO]: hello world\r\n", cap_buf);
}

void test_rpc_log_int(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_INFO, "val=%d", 42);
    TEST_ASSERT_EQUAL_STRING("[INFO]: val=42\r\n", cap_buf);
}

void test_rpc_log_uint(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_INFO, "val=%u", 100u);
    TEST_ASSERT_EQUAL_STRING("[INFO]: val=100\r\n", cap_buf);
}

void test_rpc_log_hex_fmt(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_INFO, "val=%x", 0xABu);
    TEST_ASSERT_EQUAL_STRING("[INFO]: val=0xAB\r\n", cap_buf);
}

void test_rpc_log_string_arg(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_INFO, "name=%s", "test");
    TEST_ASSERT_EQUAL_STRING("[INFO]: name=test\r\n", cap_buf);
}

void test_rpc_log_char(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_INFO, "c=%c", 'A');
    TEST_ASSERT_EQUAL_STRING("[INFO]: c=A\r\n", cap_buf);
}

void test_rpc_log_percent(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_INFO, "100%%");
    TEST_ASSERT_EQUAL_STRING("[INFO]: 100%\r\n", cap_buf);
}

void test_rpc_log_mixed(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_ERROR, "arg %d and %s", 3, "test");
    TEST_ASSERT_EQUAL_STRING("[ERROR]: arg 3 and test\r\n", cap_buf);
}

void test_rpc_info_fmt_macro(void)
{
    rpc_log_setUp();
    rpc_info("val=%d", 99);
    TEST_ASSERT_EQUAL_STRING("[INFO]: val=99\r\n", cap_buf);
}

void test_rpc_error_fmt_macro(void)
{
    rpc_log_setUp();
    rpc_error("err=%s code=%d", "timeout", 504);
    TEST_ASSERT_EQUAL_STRING("[ERROR]: err=timeout code=504\r\n", cap_buf);
}

/* ============================================================
 * GROUP 10: NULL / edge cases
 * ============================================================ */

void test_rpc_log_null_msg(void)
{
    rpc_log_setUp();
    rpc_log(RPC_LOG_INFO, NULL);
    TEST_ASSERT_EQUAL_STRING("", cap_buf);
}

void test_rpc_log_no_output_fn(void)
{
    rpc_log_set_output(NULL);
    /* Should not crash */
    rpc_log(RPC_LOG_INFO, "test");
    rpc_log_i64(RPC_LOG_INFO, "v", 1);
    rpc_log_u64(RPC_LOG_INFO, "v", 1);
    rpc_log_hex(RPC_LOG_INFO, "v", 1);
    rpc_log_f64(RPC_LOG_INFO, "v", 1.0, 1);
    rpc_log(RPC_LOG_INFO, "v=%d", 1);
}

/* ============================================================
 * GROUP 11: Protocol level macros with fmt
 * ============================================================ */

void test_rpc_return_fmt_macro(void)
{
    rpc_log_setUp();
    rpc_return("ret=%d", 42);
    TEST_ASSERT_EQUAL_STRING("[RETURN]: ret=42\r\n", cap_buf);
}

void test_rpc_data_fmt_macro(void)
{
    rpc_log_setUp();
    rpc_data("data=%s", "ok");
    TEST_ASSERT_EQUAL_STRING("[DATA]: data=ok\r\n", cap_buf);
}

void test_rpc_reg_fmt_macro(void)
{
    rpc_log_setUp();
    rpc_reg("reg=%s", "add");
    TEST_ASSERT_EQUAL_STRING("[REG]: reg=add\r\n", cap_buf);
}

void test_rpc_log_str_with_tag(void)
{
    rpc_log_setUp();
    rpc_log_str(RPC_LOG_INFO, "status", "ok");
    TEST_ASSERT_EQUAL_STRING("[INFO]: status=ok\r\n", cap_buf);
}

void test_rpc_log_str_null_tag(void)
{
    rpc_log_setUp();
    rpc_log_str(RPC_LOG_RETURN, NULL, "hello");
    TEST_ASSERT_EQUAL_STRING("[RETURN]: hello\r\n", cap_buf);
}

void test_rpc_info_str_macro(void)
{
    rpc_log_setUp();
    rpc_info_str("msg", "running");
    TEST_ASSERT_EQUAL_STRING("[INFO]: msg=running\r\n", cap_buf);
}

/* ============================================================
 * GROUP 12: Level filtering (unified rpc_log)
 * ============================================================ */

void test_rpc_log_level_filter(void)
{
    rpc_log_setUp();
    rpc_log_set_level(RPC_LOG_WARN);
    rpc_log(RPC_LOG_DEBUG, "no");
    rpc_log(RPC_LOG_INFO, "no");
    rpc_log(RPC_LOG_WARN, "yes");
    TEST_ASSERT_EQUAL_STRING("[WARN]: yes\r\n", cap_buf);
}

/* ============================================================
 * Test runner
 * ============================================================ */

int run_rpc_log_tests(void)
{
    UNITY_BEGIN();

    /* Basic format */
    RUN_TEST(test_rpc_log_debug_format);
    RUN_TEST(test_rpc_log_info_format);
    RUN_TEST(test_rpc_log_warn_format);
    RUN_TEST(test_rpc_log_error_format);
    RUN_TEST(test_rpc_log_return_format);
    RUN_TEST(test_rpc_log_data_format);
    RUN_TEST(test_rpc_log_reg_format);

    /* Convenience macros */
    RUN_TEST(test_rpc_debug_macro);
    RUN_TEST(test_rpc_info_macro);
    RUN_TEST(test_rpc_warning_macro);
    RUN_TEST(test_rpc_error_macro);
    RUN_TEST(test_rpc_return_macro);
    RUN_TEST(test_rpc_data_macro);
    RUN_TEST(test_rpc_reg_macro);

    /* Level filtering */
    RUN_TEST(test_level_filter_debug_blocked);
    RUN_TEST(test_level_filter_info_allowed);
    RUN_TEST(test_level_filter_warn_allowed);
    RUN_TEST(test_level_filter_error_allowed);
    RUN_TEST(test_level_filter_below_blocked);

    /* Protocol always output */
    RUN_TEST(test_protocol_return_always_output);
    RUN_TEST(test_protocol_data_always_output);
    RUN_TEST(test_protocol_reg_always_output);

    /* Type output */
    RUN_TEST(test_rpc_log_i64_positive);
    RUN_TEST(test_rpc_log_i64_negative);
    RUN_TEST(test_rpc_log_i64_zero);
    RUN_TEST(test_rpc_info_i64_macro);
    RUN_TEST(test_rpc_log_u64_value);
    RUN_TEST(test_rpc_log_u64_zero);
    RUN_TEST(test_rpc_log_hex_deadbeef);
    RUN_TEST(test_rpc_log_hex_zero);
    RUN_TEST(test_rpc_info_hex_macro);
    RUN_TEST(test_rpc_log_f64_positive);
    RUN_TEST(test_rpc_log_f64_negative);
    RUN_TEST(test_rpc_log_f64_integer);
    RUN_TEST(test_rpc_info_f64_macro);
    RUN_TEST(test_rpc_log_str_with_tag);
    RUN_TEST(test_rpc_log_str_null_tag);
    RUN_TEST(test_rpc_info_str_macro);

    /* fmt formatting */
    RUN_TEST(test_rpc_log_string);
    RUN_TEST(test_rpc_log_int);
    RUN_TEST(test_rpc_log_uint);
    RUN_TEST(test_rpc_log_hex_fmt);
    RUN_TEST(test_rpc_log_string_arg);
    RUN_TEST(test_rpc_log_char);
    RUN_TEST(test_rpc_log_percent);
    RUN_TEST(test_rpc_log_mixed);
    RUN_TEST(test_rpc_info_fmt_macro);
    RUN_TEST(test_rpc_error_fmt_macro);

    /* Edge cases */
    RUN_TEST(test_rpc_log_null_msg);
    RUN_TEST(test_rpc_log_no_output_fn);

    /* Protocol type macros */
    RUN_TEST(test_rpc_return_fmt_macro);
    RUN_TEST(test_rpc_data_fmt_macro);
    RUN_TEST(test_rpc_reg_fmt_macro);

    /* Level filtering */
    RUN_TEST(test_rpc_log_level_filter);

    return UNITY_END();
}
