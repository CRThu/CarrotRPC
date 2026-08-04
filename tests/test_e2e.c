/**
 * test_e2e.c — End-to-end tests for ASCII command -> function call pipeline
 *
 * Tests the complete new pipeline:
 *   DMA buffer → cmd_scan → cmd_queue_push → cmd_queue_pop
 *   → cmd_parse → dispatch_find → invoke_call
 *
 * Reuses test_invoke_helpers.h for mock functions and dispatch registry.
 */
#include "unity.h"
#include "test_invoke_helpers.h"
#include "cmdscan.h"
#include "cmdqueue.h"
#include "invoke.h"
#include "rpclog.h"
#include <string.h>

/* ===== Helper: run full new pipeline ===== */

static dispatch_status_t parse_and_invoke(const char* cmd)
{
    if (cmd == NULL) return DISPATCH_ERR_NULL;

    char buf[128];
    uint16_t len = (uint16_t)strlen(cmd);
    if (len > 0 && cmd[len - 1] != '\n' && cmd[len - 1] != ';' && cmd[len - 1] != '(')
    {
        if (len < sizeof(buf) - 1)
        {
            memcpy(buf, cmd, len);
            buf[len] = '\n';
            buf[len + 1] = '\0';
            len++;
            cmd = buf;
        }
    }

    cmd_scanner_t scanner;
    cmd_init(&scanner, (const uint8_t*)cmd, len);

    cmd_queue_t queue;
    cmd_queue_init(&queue);

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
 * GROUP 1: hello() — void function, zero args
 * ================================================================= */

/* --- Command format variants --- */
void test_e2e_hello_parens(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hello()");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

void test_e2e_hello_no_parens(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hello");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

void test_e2e_hello_crlf(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hello\r\n");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

void test_e2e_hello_parens_crlf(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hello()\r\n");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

void test_e2e_hello_lf(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hello\n");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

void test_e2e_hello_cr(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hello\r");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

/* --- Whitespace variants --- */
void test_e2e_hello_space_inside_parens(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hello( )");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

void test_e2e_hello_leading_space(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke(" hello()");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

void test_e2e_hello_trailing_space(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hello() ");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

void test_e2e_hello_tabs(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("\thello\t");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

void test_e2e_hello_mixed_whitespace(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("  \t hello() \t\r\n");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

/* =================================================================
 * GROUP 2: dec(int64_t) — signed decimal, 1 arg
 * ================================================================= */

void test_e2e_dec_positive(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("dec(42)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_dec_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(42, invoke_captured.dec_val);
}

void test_e2e_dec_negative(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("dec(-100)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_dec_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(-100, invoke_captured.dec_val);
}

void test_e2e_dec_zero(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("dec(0)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_dec_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(0, invoke_captured.dec_val);
}

void test_e2e_dec_large(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("dec(9223372036854775807)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_dec_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(9223372036854775807LL, invoke_captured.dec_val);
}

void test_e2e_dec_negative_large(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("dec(-9223372036854775807)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_dec_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(-9223372036854775807LL, invoke_captured.dec_val);
}

/* --- Space-separated style --- */
void test_e2e_dec_space_separated(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("dec 12345");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_dec_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(12345, invoke_captured.dec_val);
}

/* --- Whitespace around arg --- */
void test_e2e_dec_spaces_around_arg(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("dec( 123 )");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_dec_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(123, invoke_captured.dec_val);
}

void test_e2e_dec_tab_around_arg(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("dec(\t42\t)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_dec_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(42, invoke_captured.dec_val);
}

void test_e2e_dec_with_crlf(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("dec(42)\r\n");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_dec_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(42, invoke_captured.dec_val);
}

void test_e2e_dec_space_with_crlf(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("dec 42\r\n");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_dec_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(42, invoke_captured.dec_val);
}

/* =================================================================
 * GROUP 3: hex(uint64_t) — unsigned hex, 1 arg
 * ================================================================= */

void test_e2e_hex_basic(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hex(FF)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hex_fake.call_count);
    TEST_ASSERT_EQUAL_HEX64(0xFF, invoke_captured.hex_val);
}

void test_e2e_hex_with_prefix(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hex(0xABCDEF)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hex_fake.call_count);
    TEST_ASSERT_EQUAL_HEX64(0xABCDEF, invoke_captured.hex_val);
}

void test_e2e_hex_space_separated(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hex DEADBEEF");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hex_fake.call_count);
    TEST_ASSERT_EQUAL_HEX64(0xDEADBEEF, invoke_captured.hex_val);
}

void test_e2e_hex_space_separated_with_0x(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hex 0x1234");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hex_fake.call_count);
    TEST_ASSERT_EQUAL_HEX64(0x1234, invoke_captured.hex_val);
}

void test_e2e_hex_zero(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hex(0)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hex_fake.call_count);
    TEST_ASSERT_EQUAL_HEX64(0, invoke_captured.hex_val);
}

void test_e2e_hex_with_crlf(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hex(FF)\r\n");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hex_fake.call_count);
    TEST_ASSERT_EQUAL_HEX64(0xFF, invoke_captured.hex_val);
}

void test_e2e_hex_spaces_around_arg(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hex( FF )");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hex_fake.call_count);
    TEST_ASSERT_EQUAL_HEX64(0xFF, invoke_captured.hex_val);
}

/* =================================================================
 * GROUP 4: str(char*) — string, 1 arg
 * ================================================================= */

void test_e2e_string_basic(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("str(hello)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_string_fake.call_count);
    TEST_ASSERT_EQUAL_STRING("hello", invoke_captured.str_val);
}

void test_e2e_string_with_spaces(void)
{
    /* Parser treats spaces as separators, so "hello world" becomes 2 tokens.
       str expects 1 arg → arg count mismatch */
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("str(  hello world  )");
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_SIG, s);
    TEST_ASSERT_EQUAL_INT(0, invoke_mock_string_fake.call_count);
}

void test_e2e_string_space_separated(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("str hello");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_string_fake.call_count);
    TEST_ASSERT_EQUAL_STRING("hello", invoke_captured.str_val);
}

void test_e2e_string_with_crlf(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("str(hello)\r\n");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_string_fake.call_count);
    TEST_ASSERT_EQUAL_STRING("hello", invoke_captured.str_val);
}

void test_e2e_string_empty_parens(void)
{
    /* Empty string in parens — str expects 1 arg, got 0 → arg count mismatch */
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("str()");
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_SIG, s);
    TEST_ASSERT_EQUAL_INT(0, invoke_mock_string_fake.call_count);
}

/* =================================================================
 * GROUP 5: add(int64_t, int64_t) — 2 args, both DEC64
 * ================================================================= */

void test_e2e_add_basic(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("add(10,20)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_add_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(10, invoke_captured.add_a);
    TEST_ASSERT_EQUAL_INT64(20, invoke_captured.add_b);
}

void test_e2e_add_negative(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("add(-5,3)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_add_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(-5, invoke_captured.add_a);
    TEST_ASSERT_EQUAL_INT64(3, invoke_captured.add_b);
}

void test_e2e_add_space_separated(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("add 100 200");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_add_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(100, invoke_captured.add_a);
    TEST_ASSERT_EQUAL_INT64(200, invoke_captured.add_b);
}

void test_e2e_add_comma_spaces(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("add( 1 , 2 )");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_add_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(1, invoke_captured.add_a);
    TEST_ASSERT_EQUAL_INT64(2, invoke_captured.add_b);
}

void test_e2e_add_semicolon_separator(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("add(1;2)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_add_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(1, invoke_captured.add_a);
    TEST_ASSERT_EQUAL_INT64(2, invoke_captured.add_b);
}

void test_e2e_add_mixed_separators(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("add(1, 2 )");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_add_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(1, invoke_captured.add_a);
    TEST_ASSERT_EQUAL_INT64(2, invoke_captured.add_b);
}

void test_e2e_add_with_crlf(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("add(10,20)\r\n");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_add_fake.call_count);
}

void test_e2e_add_with_lf(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("add(10,20)\n");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_add_fake.call_count);
}

void test_e2e_add_zeroes(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("add(0,0)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_add_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(0, invoke_captured.add_a);
    TEST_ASSERT_EQUAL_INT64(0, invoke_captured.add_b);
}

/* =================================================================
 * GROUP 6: args(char*, char*, char*) — 3 string args
 * ================================================================= */

void test_e2e_args_basic(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("args(a,b,c)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_args_fake.call_count);
    TEST_ASSERT_EQUAL_STRING("a", invoke_captured.arg0);
    TEST_ASSERT_EQUAL_STRING("b", invoke_captured.arg1);
    TEST_ASSERT_EQUAL_STRING("c", invoke_captured.arg2);
}

void test_e2e_args_space_separated(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("args hello world test");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_args_fake.call_count);
    TEST_ASSERT_EQUAL_STRING("hello", invoke_captured.arg0);
    TEST_ASSERT_EQUAL_STRING("world", invoke_captured.arg1);
    TEST_ASSERT_EQUAL_STRING("test", invoke_captured.arg2);
}

void test_e2e_args_with_spaces_in_values(void)
{
    /* Parser treats spaces as separators: "hello world , foo bar , baz"
       parses as 5 tokens: hello, world, foo, bar, baz.
       args expects 3 → arg count mismatch */
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("args( hello world , foo bar , baz )");
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_SIG, s);
    TEST_ASSERT_EQUAL_INT(0, invoke_mock_args_fake.call_count);
}

void test_e2e_args_with_crlf(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("args(a,b,c)\r\n");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_args_fake.call_count);
}

void test_e2e_args_semicolon_sep(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("args(a;b;c)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_args_fake.call_count);
    TEST_ASSERT_EQUAL_STRING("a", invoke_captured.arg0);
    TEST_ASSERT_EQUAL_STRING("b", invoke_captured.arg1);
    TEST_ASSERT_EQUAL_STRING("c", invoke_captured.arg2);
}

void test_e2e_args_mixed_brackets(void)
{
    /* cmd_scan only handles (), not {} or [].
       "args{a}{b}{c}" — cmd_scan scans past {} treating them as regular chars,
       extracting the whole thing as function name "args{a}{b}{c}" which is not found */
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("args{a}{b}{c}");
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_NOT_FOUND, s);
    TEST_ASSERT_EQUAL_INT(0, invoke_mock_args_fake.call_count);
}

/* =================================================================
 * GROUP 7: Error paths
 * ================================================================= */

void test_e2e_error_nonexistent_function(void)
{
    dispatch_status_t s = parse_and_invoke("nonexistent()");
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_NOT_FOUND, s);
    TEST_ASSERT_EQUAL_INT(0, invoke_mock_hello_fake.call_count);
}

void test_e2e_error_nonexistent_space(void)
{
    dispatch_status_t s = parse_and_invoke("nonexistent arg1");
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_NOT_FOUND, s);
    TEST_ASSERT_EQUAL_INT(0, invoke_mock_hello_fake.call_count);
}

void test_e2e_error_empty_string(void)
{
    /* Empty buffer — cmd_scan finds no command, pipeline returns OK with no calls */
    dispatch_status_t s = parse_and_invoke("");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(0, invoke_mock_hello_fake.call_count);
}

void test_e2e_error_whitespace_only(void)
{
    /* Whitespace only — cmd_scan skips all, returns INCOMPLETE */
    dispatch_status_t s = parse_and_invoke("   ");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(0, invoke_mock_hello_fake.call_count);
}

void test_e2e_error_unclosed_paren(void)
{
    /* "hello(" — cmd_scan returns CMD_INCOMPLETE (no terminator found inside parens),
       so no command is extracted and no function is called */
    dispatch_status_t s = parse_and_invoke("hello(");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(0, invoke_mock_hello_fake.call_count);
}

void test_e2e_error_unmatched_close_paren(void)
{
    /* "hello)" — cmd_scan extracts "hello)" as complete command (6 chars).
       cmd_parse extracts function name "hello)" (length 6, not terminated at ')').
       dispatch_find("hello)", 6) → not found */
    dispatch_status_t s = parse_and_invoke("hello)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_NOT_FOUND, s);
}

void test_e2e_error_partial_match(void)
{
    dispatch_status_t s = parse_and_invoke("hell");
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_NOT_FOUND, s);
}

/* =================================================================
 * GROUP 8: No function name — just arguments
 * ================================================================= */

void test_e2e_args_only_1(void)
{
    /* "1" — cmd_scan extracts "1" as command, cmd_parse extracts function name "1",
       dispatch_find("1", 1) → not found */
    dispatch_status_t s = parse_and_invoke("1");
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_NOT_FOUND, s);
}

void test_e2e_args_only_2(void)
{
    /* "1 2 3" — cmd_scan extracts "1 2 3", cmd_parse sees "1" as function name
       with "2" and "3" as space-separated args. dispatch_find("1", 1) → not found */
    dispatch_status_t s = parse_and_invoke("1 2 3");
    TEST_ASSERT_EQUAL_INT(DISPATCH_ERR_NOT_FOUND, s);
}

/* =================================================================
 * GROUP 9: Two commands separated by \n
 * ================================================================= */

void test_e2e_two_commands_lf(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hello()\nhello()");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(2, invoke_mock_hello_fake.call_count);
}

void test_e2e_two_commands_crlf(void)
{
    invoke_test_helpers_reset();
    dispatch_status_t s = parse_and_invoke("hello()\r\nhello()\r\n");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_INT(2, invoke_mock_hello_fake.call_count);
}

void test_e2e_streaming_chunked_pipeline(void)
{
    /* 测试 DMA 流式切片追加完整管线: DMA -> cmdscan(增量) -> cmdqueue -> parse -> invoke */
    invoke_test_helpers_reset();

    cmd_scanner_t scanner;
    cmd_queue_t queue;
    cmd_queue_init(&queue);

    uint8_t dma_buf[64] = {0};
    cmd_init(&scanner, dma_buf, 0);

    /* 批次 1: 追加 "add(" (4 字节) */
    memcpy(dma_buf, "add(", 4);
    scanner.buf_size = 4;

    cmd_entry_t entry;
    TEST_ASSERT_EQUAL_INT(CMD_INCOMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_TRUE(cmd_queue_is_empty(&queue));
    TEST_ASSERT_EQUAL_INT(0, invoke_mock_add_fake.call_count);

    /* 批次 2: 增量追加至 "add(10, " (8 字节) */
    memcpy(dma_buf, "add(10, ", 8);
    scanner.buf_size = 8;
    TEST_ASSERT_EQUAL_INT(CMD_INCOMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_TRUE(cmd_queue_is_empty(&queue));
    TEST_ASSERT_EQUAL_INT(0, invoke_mock_add_fake.call_count);

    /* 批次 3: 补全完整命令 "add(10, 20)\r\n" (14 字节) */
    memcpy(dma_buf, "add(10, 20)\r\n", 14);
    scanner.buf_size = 14;
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, cmd_scan(&scanner, &entry));

    /* 入队 -> 出队 -> 解析 -> 调用 */
    cmd_queue_push(&queue, &entry);
    TEST_ASSERT_FALSE(cmd_queue_is_empty(&queue));

    cmd_entry_t pop_entry;
    cmd_queue_pop(&queue, &pop_entry);
    cmd_args_t args;
    cmd_parse((const char*)pop_entry.buf + pop_entry.cmd_start, pop_entry.cmd_len, &args);

    invoke_ret_t ret;
    dispatch_status_t status = invoke_call(&invoke_dispatcher, &args, &ret);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, status);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_add_fake.call_count);
    TEST_ASSERT_EQUAL_INT64(10, invoke_captured.add_a);
    TEST_ASSERT_EQUAL_INT64(20, invoke_captured.add_b);

    /* 批次 4: 紧接着在同一 buffer 末尾追加 "hello()\r\n" (23 字节) */
    memcpy(dma_buf + 14, "hello()\r\n", 9);
    scanner.buf_size = 23;
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, cmd_scan(&scanner, &entry));

    cmd_queue_push(&queue, &entry);
    cmd_queue_pop(&queue, &pop_entry);
    cmd_parse((const char*)pop_entry.buf + pop_entry.cmd_start, pop_entry.cmd_len, &args);

    status = invoke_call(&invoke_dispatcher, &args, &ret);
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, status);
    TEST_ASSERT_EQUAL_INT(1, invoke_mock_hello_fake.call_count);
}

/* =================================================================
 * GROUP 10: dispatch_init test
 * ================================================================= */

void test_e2e_reset_clears_registry(void)
{
    /* After setUp, hello is registered */
    dispatch_func_t* f = dispatch_find(&invoke_dispatcher, "hello", 5);
    TEST_ASSERT_NOT_NULL(f);

    dispatch_init(&invoke_dispatcher);

    f = dispatch_find(&invoke_dispatcher, "hello", 5);
    TEST_ASSERT_NULL(f);
}

void test_e2e_reset_allows_re_register(void)
{
    dispatch_init(&invoke_dispatcher);
    dispatch_reg(&invoke_dispatcher, invoke_mock_hello, "hello()");

    dispatch_func_t* f = dispatch_find(&invoke_dispatcher, "hello", 5);
    TEST_ASSERT_NOT_NULL(f);
}

/* =================================================================
 * GROUP 11: Auto Return [RETURN] Log Output
 * ================================================================= */

static char g_log_buf[256];
static uint16_t g_log_pos = 0;

static void log_capture_char(char c)
{
    if (g_log_pos < sizeof(g_log_buf) - 1)
    {
        g_log_buf[g_log_pos++] = c;
        g_log_buf[g_log_pos] = '\0';
    }
}

static void reset_log_capture(void)
{
    memset(g_log_buf, 0, sizeof(g_log_buf));
    g_log_pos = 0;
    rpc_log_set_output(log_capture_char);
}

void test_e2e_auto_return_i64(void)
{
    invoke_test_helpers_reset();
    reset_log_capture();

    dispatch_status_t s = parse_and_invoke("add(10,20)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_STRING("[RETURN]: 30\r\n", g_log_buf);
}

static char* mock_echo(void* a)
{
    return (char*)a;
}

void test_e2e_auto_return_str(void)
{
    invoke_test_helpers_reset();
    dispatch_reg(&invoke_dispatcher, mock_echo, "echo(s) -> s");
    reset_log_capture();

    dispatch_status_t s = parse_and_invoke("echo(world)");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_STRING("[RETURN]: world\r\n", g_log_buf);
}

void test_e2e_auto_return_none(void)
{
    invoke_test_helpers_reset();
    reset_log_capture();

    dispatch_status_t s = parse_and_invoke("hello()");
    TEST_ASSERT_EQUAL_INT(DISPATCH_OK, s);
    TEST_ASSERT_EQUAL_STRING("", g_log_buf);
}

/* =================================================================
 * Unity test runner
 * ================================================================= */

int run_e2e_tests(void)
{
    UNITY_BEGIN();

    /* Group 1: hello */
    RUN_TEST(test_e2e_hello_parens);
    RUN_TEST(test_e2e_hello_no_parens);
    RUN_TEST(test_e2e_hello_crlf);
    RUN_TEST(test_e2e_hello_parens_crlf);
    RUN_TEST(test_e2e_hello_lf);
    RUN_TEST(test_e2e_hello_cr);
    RUN_TEST(test_e2e_hello_space_inside_parens);
    RUN_TEST(test_e2e_hello_leading_space);
    RUN_TEST(test_e2e_hello_trailing_space);
    RUN_TEST(test_e2e_hello_tabs);
    RUN_TEST(test_e2e_hello_mixed_whitespace);

    /* Group 2: dec */
    RUN_TEST(test_e2e_dec_positive);
    RUN_TEST(test_e2e_dec_negative);
    RUN_TEST(test_e2e_dec_zero);
    RUN_TEST(test_e2e_dec_large);
    RUN_TEST(test_e2e_dec_negative_large);
    RUN_TEST(test_e2e_dec_space_separated);
    RUN_TEST(test_e2e_dec_spaces_around_arg);
    RUN_TEST(test_e2e_dec_tab_around_arg);
    RUN_TEST(test_e2e_dec_with_crlf);
    RUN_TEST(test_e2e_dec_space_with_crlf);

    /* Group 3: hex */
    RUN_TEST(test_e2e_hex_basic);
    RUN_TEST(test_e2e_hex_with_prefix);
    RUN_TEST(test_e2e_hex_space_separated);
    RUN_TEST(test_e2e_hex_space_separated_with_0x);
    RUN_TEST(test_e2e_hex_zero);
    RUN_TEST(test_e2e_hex_with_crlf);
    RUN_TEST(test_e2e_hex_spaces_around_arg);

    /* Group 4: string */
    RUN_TEST(test_e2e_string_basic);
    RUN_TEST(test_e2e_string_with_spaces);
    RUN_TEST(test_e2e_string_space_separated);
    RUN_TEST(test_e2e_string_with_crlf);
    RUN_TEST(test_e2e_string_empty_parens);

    /* Group 5: add (2 args) */
    RUN_TEST(test_e2e_add_basic);
    RUN_TEST(test_e2e_add_negative);
    RUN_TEST(test_e2e_add_space_separated);
    RUN_TEST(test_e2e_add_comma_spaces);
    RUN_TEST(test_e2e_add_semicolon_separator);
    RUN_TEST(test_e2e_add_mixed_separators);
    RUN_TEST(test_e2e_add_with_crlf);
    RUN_TEST(test_e2e_add_with_lf);
    RUN_TEST(test_e2e_add_zeroes);

    /* Group 6: args (3 string args) */
    RUN_TEST(test_e2e_args_basic);
    RUN_TEST(test_e2e_args_space_separated);
    RUN_TEST(test_e2e_args_with_spaces_in_values);
    RUN_TEST(test_e2e_args_with_crlf);
    RUN_TEST(test_e2e_args_semicolon_sep);
    RUN_TEST(test_e2e_args_mixed_brackets);

    /* Group 7: Error paths */
    RUN_TEST(test_e2e_error_nonexistent_function);
    RUN_TEST(test_e2e_error_nonexistent_space);
    RUN_TEST(test_e2e_error_empty_string);
    RUN_TEST(test_e2e_error_whitespace_only);
    RUN_TEST(test_e2e_error_unclosed_paren);
    RUN_TEST(test_e2e_error_unmatched_close_paren);
    RUN_TEST(test_e2e_error_partial_match);

    /* Group 8: Args only */
    RUN_TEST(test_e2e_args_only_1);
    RUN_TEST(test_e2e_args_only_2);

    /* Group 9: Multi-command */
    RUN_TEST(test_e2e_two_commands_lf);
    RUN_TEST(test_e2e_two_commands_crlf);
    RUN_TEST(test_e2e_streaming_chunked_pipeline);

    /* Group 10: Reset */
    RUN_TEST(test_e2e_reset_clears_registry);
    RUN_TEST(test_e2e_reset_allows_re_register);

    /* Group 11: Auto Return */
    RUN_TEST(test_e2e_auto_return_i64);
    RUN_TEST(test_e2e_auto_return_str);
    RUN_TEST(test_e2e_auto_return_none);

    return UNITY_END();
}
