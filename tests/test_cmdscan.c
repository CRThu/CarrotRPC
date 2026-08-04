/**
 * test_cmdscan.c — Unit tests for cmdscan module
 */
#include "unity.h"
#include "test_helpers.h"
#include "cmdscan.h"
#include <string.h>

/* ===== cmd_init ===== */
void test_cmd_init(void)
{
    cmd_scanner_t scanner;
    uint8_t buf[] = "hello";
    cmd_init(&scanner, buf, sizeof(buf) - 1);

    TEST_ASSERT_EQUAL_UINT16(0, scanner.scan_pos);
}

void test_cmd_init_null(void)
{
    cmd_init(NULL, NULL, 0);  /* 不应崩溃 */
}

/* ===== cmd_reset ===== */
void test_cmd_reset(void)
{
    cmd_scanner_t scanner;
    uint8_t buf[] = "test";
    cmd_init(&scanner, buf, sizeof(buf) - 1);
    scanner.scan_pos = 5;

    cmd_reset(&scanner);

    TEST_ASSERT_EQUAL_UINT16(0, scanner.scan_pos);
}

/* ===== cmd_scan: 四种命令格式 ===== */
void test_cmd_scan_parens(void)
{
    /* print(1)\n */
    cmd_scanner_t scanner;
    uint8_t buf[] = "print(1)\n";
    cmd_init(&scanner, buf, sizeof(buf) - 1);

    cmd_entry_t entry;
    cmd_status_t s = cmd_scan(&scanner, &entry);
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, s);
    TEST_ASSERT_EQUAL_UINT16(0, entry.cmd_start);
    TEST_ASSERT_EQUAL_UINT16(8, entry.cmd_len);      /* "print(1)" */
    TEST_ASSERT_EQUAL_UINT8(5, entry.func_len);      /* "print" */
    TEST_ASSERT_EQUAL_MEMORY("print", entry.buf + entry.cmd_start, entry.func_len);
}

void test_cmd_scan_parens_with_semicolon(void)
{
    /* print(1);\n */
    cmd_scanner_t scanner;
    uint8_t buf[] = "print(1);\n";
    cmd_init(&scanner, buf, sizeof(buf) - 1);

    cmd_entry_t entry;
    cmd_status_t s = cmd_scan(&scanner, &entry);
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, s);
    TEST_ASSERT_EQUAL_UINT16(8, entry.cmd_len);      /* "print(1)" 不含分号 */
    TEST_ASSERT_EQUAL_UINT8(5, entry.func_len);
}

void test_cmd_scan_space_separated(void)
{
    /* print 1\n */
    cmd_scanner_t scanner;
    uint8_t buf[] = "print 1\n";
    cmd_init(&scanner, buf, sizeof(buf) - 1);

    cmd_entry_t entry;
    cmd_status_t s = cmd_scan(&scanner, &entry);
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, s);
    TEST_ASSERT_EQUAL_UINT16(7, entry.cmd_len);      /* "print 1" */
    TEST_ASSERT_EQUAL_UINT8(5, entry.func_len);
}

void test_cmd_scan_semicolon_separated(void)
{
    /* print;1;\n */
    cmd_scanner_t scanner;
    uint8_t buf[] = "print;1;\n";
    cmd_init(&scanner, buf, sizeof(buf) - 1);

    cmd_entry_t entry;
    cmd_status_t s = cmd_scan(&scanner, &entry);
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, s);
    TEST_ASSERT_EQUAL_UINT16(5, entry.cmd_len);      /* "print" 不含分号 */
    TEST_ASSERT_EQUAL_UINT8(5, entry.func_len);
}

void test_cmd_scan_multi_args_parens(void)
{
    /* print(1,2,3)\n */
    cmd_scanner_t scanner;
    uint8_t buf[] = "print(1,2,3)\n";
    cmd_init(&scanner, buf, sizeof(buf) - 1);

    cmd_entry_t entry;
    cmd_status_t s = cmd_scan(&scanner, &entry);
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, s);
    TEST_ASSERT_EQUAL_UINT16(12, entry.cmd_len);     /* "print(1,2,3)" */
    TEST_ASSERT_EQUAL_UINT8(5, entry.func_len);
}

void test_cmd_scan_multi_args_parens_semicolon(void)
{
    /* print(1,2,3);\n */
    cmd_scanner_t scanner;
    uint8_t buf[] = "print(1,2,3);\n";
    cmd_init(&scanner, buf, sizeof(buf) - 1);

    cmd_entry_t entry;
    cmd_status_t s = cmd_scan(&scanner, &entry);
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, s);
    TEST_ASSERT_EQUAL_UINT16(12, entry.cmd_len);     /* "print(1,2,3)" 不含分号 */
    TEST_ASSERT_EQUAL_UINT8(5, entry.func_len);
}

void test_cmd_scan_multi_args_space(void)
{
    /* print 1 2 3\n */
    cmd_scanner_t scanner;
    uint8_t buf[] = "print 1 2 3\n";
    cmd_init(&scanner, buf, sizeof(buf) - 1);

    cmd_entry_t entry;
    cmd_status_t s = cmd_scan(&scanner, &entry);
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, s);
    TEST_ASSERT_EQUAL_UINT16(11, entry.cmd_len);     /* "print 1 2 3" */
    TEST_ASSERT_EQUAL_UINT8(5, entry.func_len);
}

void test_cmd_scan_multi_args_semicolon(void)
{
    /* print;1;2;3;\n */
    cmd_scanner_t scanner;
    uint8_t buf[] = "print;1;2;3;\n";
    cmd_init(&scanner, buf, sizeof(buf) - 1);

    cmd_entry_t entry;
    cmd_status_t s = cmd_scan(&scanner, &entry);
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, s);
    TEST_ASSERT_EQUAL_UINT16(5, entry.cmd_len);      /* "print" 不含分号 */
    TEST_ASSERT_EQUAL_UINT8(5, entry.func_len);
}

/* ===== cmd_parse: 基本 ===== */
void test_cmd_parse_no_args(void)
{
    cmd_args_t result;
    uint8_t count = cmd_parse("print", 5, &result);

    TEST_ASSERT_EQUAL_UINT8(0, count);
    TEST_ASSERT_EQUAL_UINT16(5, result.func_name_len);
    TEST_ASSERT_EQUAL_MEMORY("print", result.func_name, 5);
}

void test_cmd_parse_with_parens(void)
{
    cmd_args_t result;
    uint8_t count = cmd_parse("print(1,2)", 10, &result);

    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_MEMORY("print", result.func_name, 5);
    TEST_ASSERT_EQUAL_MEMORY("1", result.args[0].ptr, result.args[0].len);
    TEST_ASSERT_EQUAL_MEMORY("2", result.args[1].ptr, result.args[1].len);
}

void test_cmd_parse_space_separated(void)
{
    cmd_args_t result;
    uint8_t count = cmd_parse("print 1 2", 9, &result);

    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_MEMORY("print", result.func_name, 5);
    TEST_ASSERT_EQUAL_MEMORY("1", result.args[0].ptr, result.args[0].len);
    TEST_ASSERT_EQUAL_MEMORY("2", result.args[1].ptr, result.args[1].len);
}

void test_cmd_parse_semicolon_separated(void)
{
    cmd_args_t result;
    uint8_t count = cmd_parse("print;1;2", 9, &result);

    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_MEMORY("print", result.func_name, 5);
    TEST_ASSERT_EQUAL_MEMORY("1", result.args[0].ptr, result.args[0].len);
    TEST_ASSERT_EQUAL_MEMORY("2", result.args[1].ptr, result.args[1].len);
}

void test_cmd_parse_with_spaces(void)
{
    cmd_args_t result;
    uint8_t count = cmd_parse("print( 1 , 2 )", 14, &result);

    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_MEMORY("1", result.args[0].ptr, result.args[0].len);
    TEST_ASSERT_EQUAL_MEMORY("2", result.args[1].ptr, result.args[1].len);
}

void test_cmd_parse_empty(void)
{
    cmd_args_t result;
    uint8_t count = cmd_parse("", 0, &result);
    TEST_ASSERT_EQUAL_UINT8(0xFF, count);
}

void test_cmd_parse_null(void)
{
    uint8_t count = cmd_parse(NULL, 0, NULL);
    TEST_ASSERT_EQUAL_UINT8(0xFF, count);
}

void test_cmd_parse_func_with_space(void)
{
    cmd_args_t result;
    uint8_t count = cmd_parse("print 123", 9, &result);

    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_MEMORY("print", result.func_name, 5);
    TEST_ASSERT_EQUAL_MEMORY("123", result.args[0].ptr, result.args[0].len);
}

/* ===== 零拷贝验证 ===== */
void test_cmd_zero_copy(void)
{
    char buf[] = "print(1,2)";
    cmd_args_t result;
    cmd_parse(buf, strlen(buf), &result);

    /* 指针应指向原始缓冲区 */
    TEST_ASSERT_TRUE(result.func_name >= buf && result.func_name < buf + strlen(buf));
    TEST_ASSERT_TRUE(result.args[0].ptr >= buf && result.args[0].ptr < buf + strlen(buf));
    TEST_ASSERT_TRUE(result.args[1].ptr >= buf && result.args[1].ptr < buf + strlen(buf));
}

/* ===== cmd_scan: 流式切片与粘包测试 ===== */

void test_cmd_scan_streaming_chunked(void)
{
    /* 模拟 DMA 串口 2Mbps 高波特率下，add(10, 20)\r\n 分 3 批到达 */
    cmd_scanner_t scanner;
    cmd_entry_t entry;

    /* 批次 1: 仅首字节 "a" */
    uint8_t chunk1[] = "a";
    cmd_init(&scanner, chunk1, 1);
    TEST_ASSERT_EQUAL_INT(CMD_INCOMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_EQUAL_UINT16(0, scanner.scan_pos); /* 回退位置 */

    /* 批次 2: 接收到前缀 "add(10" (尚无右括号及换行) */
    uint8_t chunk2[] = "add(10";
    cmd_init(&scanner, chunk2, 6);
    TEST_ASSERT_EQUAL_INT(CMD_INCOMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_EQUAL_UINT16(0, scanner.scan_pos);

    /* 批次 3: 完整帧到达 "add(10, 20)\r\n" */
    uint8_t chunk3[] = "add(10, 20)\r\n";
    cmd_init(&scanner, chunk3, sizeof(chunk3) - 1);
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_EQUAL_UINT16(0, entry.cmd_start);
    TEST_ASSERT_EQUAL_UINT16(12, entry.cmd_len); /* "add(10, 20)\r" */
    TEST_ASSERT_EQUAL_UINT8(3, entry.func_len);   /* "add" */
}

void test_cmd_scan_sticky_and_partial_packets(void)
{
    /* 粘包 + 未完成半包: "ping()\r\nadd(10, 20)\r\nhello" */
    cmd_scanner_t scanner;
    cmd_entry_t entry;

    uint8_t buf[] = "ping()\r\nadd(10, 20)\r\nhello";
    cmd_init(&scanner, buf, sizeof(buf) - 1);

    /* 第 1 条完整包: ping() */
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_EQUAL_UINT16(0, entry.cmd_start);
    TEST_ASSERT_EQUAL_UINT16(7, entry.cmd_len);
    TEST_ASSERT_EQUAL_UINT8(4, entry.func_len);

    /* 第 2 条完整包: add(10, 20) */
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_EQUAL_UINT16(8, entry.cmd_start);
    TEST_ASSERT_EQUAL_UINT16(12, entry.cmd_len);
    TEST_ASSERT_EQUAL_UINT8(3, entry.func_len);

    /* 第 3 条未完整包: hello (缺少 \n) */
    uint16_t pos_before = scanner.scan_pos;
    TEST_ASSERT_EQUAL_INT(CMD_INCOMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_EQUAL_UINT16(pos_before, scanner.scan_pos); /* 位置重置回 hello 开头 */
}

/* ===== runner ===== */
int run_cmdscan_tests(void)
{
    UnityBegin("test_cmdscan.c");

    /* cmd_init */
    RUN_TEST(test_cmd_init);
    RUN_TEST(test_cmd_init_null);

    /* cmd_reset */
    RUN_TEST(test_cmd_reset);

    /* cmd_scan */
    RUN_TEST(test_cmd_scan_parens);
    RUN_TEST(test_cmd_scan_parens_with_semicolon);
    RUN_TEST(test_cmd_scan_space_separated);
    RUN_TEST(test_cmd_scan_semicolon_separated);
    RUN_TEST(test_cmd_scan_multi_args_parens);
    RUN_TEST(test_cmd_scan_multi_args_parens_semicolon);
    RUN_TEST(test_cmd_scan_multi_args_space);
    RUN_TEST(test_cmd_scan_multi_args_semicolon);
    RUN_TEST(test_cmd_scan_streaming_chunked);
    RUN_TEST(test_cmd_scan_sticky_and_partial_packets);

    /* cmd_parse */
    RUN_TEST(test_cmd_parse_no_args);
    RUN_TEST(test_cmd_parse_with_parens);
    RUN_TEST(test_cmd_parse_space_separated);
    RUN_TEST(test_cmd_parse_semicolon_separated);
    RUN_TEST(test_cmd_parse_with_spaces);
    RUN_TEST(test_cmd_parse_empty);
    RUN_TEST(test_cmd_parse_null);
    RUN_TEST(test_cmd_parse_func_with_space);

    /* 零拷贝 */
    RUN_TEST(test_cmd_zero_copy);

    return UnityEnd();
}
