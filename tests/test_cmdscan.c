/**
 * test_cmdscan.c — Unit tests for cmdscan module
 */
#include "unity.h"
#include "test_helpers.h"
#include "cmdscan.h"
#include "ringbuf.h"
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
    cmd_entry_t e = { (const uint8_t*)"print", 5, 0, 5, 0 };
    uint8_t count = cmd_parse(&e, &result);

    TEST_ASSERT_EQUAL_UINT8(0, count);
    TEST_ASSERT_EQUAL_UINT16(5, result.func_name_len);
    TEST_ASSERT_EQUAL_MEMORY("print", result.func_name, 5);
}

void test_cmd_parse_with_parens(void)
{
    cmd_args_t result;
    cmd_entry_t e = { (const uint8_t*)"print(1,2)", 10, 0, 10, 0 };
    uint8_t count = cmd_parse(&e, &result);

    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_MEMORY("print", result.func_name, 5);
    TEST_ASSERT_EQUAL_MEMORY("1", result.args[0].ptr, result.args[0].len);
    TEST_ASSERT_EQUAL_MEMORY("2", result.args[1].ptr, result.args[1].len);
}

void test_cmd_parse_space_separated(void)
{
    cmd_args_t result;
    cmd_entry_t e = { (const uint8_t*)"print 1 2", 9, 0, 9, 0 };
    uint8_t count = cmd_parse(&e, &result);

    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_MEMORY("print", result.func_name, 5);
    TEST_ASSERT_EQUAL_MEMORY("1", result.args[0].ptr, result.args[0].len);
    TEST_ASSERT_EQUAL_MEMORY("2", result.args[1].ptr, result.args[1].len);
}

void test_cmd_parse_semicolon_separated(void)
{
    cmd_args_t result;
    cmd_entry_t e = { (const uint8_t*)"print;1;2", 9, 0, 9, 0 };
    uint8_t count = cmd_parse(&e, &result);

    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_MEMORY("print", result.func_name, 5);
    TEST_ASSERT_EQUAL_MEMORY("1", result.args[0].ptr, result.args[0].len);
    TEST_ASSERT_EQUAL_MEMORY("2", result.args[1].ptr, result.args[1].len);
}

void test_cmd_parse_with_spaces(void)
{
    cmd_args_t result;
    cmd_entry_t e = { (const uint8_t*)"print( 1 , 2 )", 14, 0, 14, 0 };
    uint8_t count = cmd_parse(&e, &result);

    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_MEMORY("1", result.args[0].ptr, result.args[0].len);
    TEST_ASSERT_EQUAL_MEMORY("2", result.args[1].ptr, result.args[1].len);
}

void test_cmd_parse_empty(void)
{
    cmd_args_t result;
    cmd_entry_t e = { (const uint8_t*)"", 0, 0, 0, 0 };
    uint8_t count = cmd_parse(&e, &result);
    TEST_ASSERT_EQUAL_UINT8(0xFF, count);
}

void test_cmd_parse_null(void)
{
    uint8_t count = cmd_parse(NULL, NULL);
    TEST_ASSERT_EQUAL_UINT8(0xFF, count);
}

void test_cmd_parse_func_with_space(void)
{
    cmd_args_t result;
    cmd_entry_t e = { (const uint8_t*)"print 123", 9, 0, 9, 0 };
    uint8_t count = cmd_parse(&e, &result);

    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_MEMORY("print", result.func_name, 5);
    TEST_ASSERT_EQUAL_MEMORY("123", result.args[0].ptr, result.args[0].len);
}

void test_cmd_parse_entry_with_offset(void)
{
    /* 缓冲区前面有 10 字节垃圾数据，cmd_start = 10 */
    uint8_t buf[64] = "0123456789sub(50, 20)";
    cmd_entry_t e = { buf, 64, 10, 11, 3 };
    cmd_args_t result;
    uint8_t count = cmd_parse(&e, &result);

    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_UINT16(3, result.func_name_len);
    TEST_ASSERT_EQUAL_MEMORY("sub", result.func_name, 3);
    TEST_ASSERT_EQUAL_MEMORY("50", result.args[0].ptr, result.args[0].len);
    TEST_ASSERT_EQUAL_MEMORY("20", result.args[1].ptr, result.args[1].len);
}

void test_cmd_parse_entry_max_args(void)
{
    /* 10 个参数极限值测试 */
    const char* str = "fn(1,2,3,4,5,6,7,8,9,10)";
    cmd_entry_t e = { (const uint8_t*)str, (uint16_t)strlen(str), 0, (uint16_t)strlen(str), 2 };
    cmd_args_t result;
    uint8_t count = cmd_parse(&e, &result);

    TEST_ASSERT_EQUAL_UINT8(10, count);
    TEST_ASSERT_EQUAL_MEMORY("1", result.args[0].ptr, result.args[0].len);
    TEST_ASSERT_EQUAL_MEMORY("10", result.args[9].ptr, result.args[9].len);
}

void test_cmd_parse_entry_invalid_buf_or_len(void)
{
    cmd_args_t result;

    /* buf 为 NULL */
    cmd_entry_t e1 = { NULL, 10, 0, 5, 0 };
    TEST_ASSERT_EQUAL_UINT8(0xFF, cmd_parse(&e1, &result));

    /* cmd_len 为 0 */
    uint8_t buf[10] = "test";
    cmd_entry_t e2 = { buf, 10, 0, 0, 0 };
    TEST_ASSERT_EQUAL_UINT8(0xFF, cmd_parse(&e2, &result));
}

/* ===== 零拷贝验证 ===== */
void test_cmd_zero_copy(void)
{
    char buf[] = "print(1,2)";
    cmd_args_t result;
    cmd_entry_t e = { (const uint8_t*)buf, (uint16_t)strlen(buf), 0, (uint16_t)strlen(buf), 0 };
    cmd_parse(&e, &result);

    /* 指针应指向原始缓冲区 */
    TEST_ASSERT_TRUE(result.func_name >= buf && result.func_name < buf + strlen(buf));
    TEST_ASSERT_TRUE(result.args[0].ptr >= buf && result.args[0].ptr < buf + strlen(buf));
    TEST_ASSERT_TRUE(result.args[1].ptr >= buf && result.args[1].ptr < buf + strlen(buf));
}

/* ===== cmd_scan: 流式增量切片与粘包测试 ===== */

void test_cmd_scan_streaming_chunked(void)
{
    /* 模拟 DMA 串口 2Mbps 高波特率下，add(10, 20)\r\n 分 3 批到达 (同一缓冲区 buf_size 递增) */
    cmd_scanner_t scanner;
    cmd_entry_t entry;

    uint8_t buf[32] = "add(10, 20)\r\n";

    /* 批次 1: 仅首字节 "a" (buf_size = 1) */
    cmd_init(&scanner, buf, 1);
    TEST_ASSERT_EQUAL_INT(CMD_INCOMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_EQUAL_UINT16(1, scanner.scan_pos); /* 不回退，推进到 1 */

    /* 批次 2: 接收增量追加至 "add(10" (buf_size = 6) */
    scanner.buf_size = 6;
    TEST_ASSERT_EQUAL_INT(CMD_INCOMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_EQUAL_UINT16(6, scanner.scan_pos); /* 不回退，推进到 6 */

    /* 批次 3: 完整帧到达 "add(10, 20)\r\n" (buf_size = 13) */
    scanner.buf_size = 13;
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_EQUAL_UINT16(0, entry.cmd_start);
    TEST_ASSERT_EQUAL_UINT16(11, entry.cmd_len); /* "add(10, 20)" 精准长度 11 */
    TEST_ASSERT_EQUAL_UINT8(3, entry.func_len);   /* "add" */
}

void test_cmd_scan_byte_by_byte(void)
{
    /* 逐字节增量扫描测试 O(N) */
    cmd_scanner_t scanner;
    cmd_entry_t entry;

    uint8_t buf[] = "set_led(1, 2)\n";
    uint16_t total_len = sizeof(buf) - 1; /* 14 */

    cmd_init(&scanner, buf, 0);

    /* 逐字节增加 buf_size 并扫描，直到遇到右括号 ')' 前均为 CMD_INCOMPLETE */
    for (uint16_t size = 1; size < 13; size++)
    {
        scanner.buf_size = size;
        cmd_status_t s = cmd_scan(&scanner, &entry);
        TEST_ASSERT_EQUAL_INT(CMD_INCOMPLETE, s);
        TEST_ASSERT_EQUAL_UINT16(size, scanner.scan_pos); /* 单向递增 */
    }

    /* 当 buf_size 抵达包含 ')' 的 13 字节时，命令成帧成 COMPLETE */
    scanner.buf_size = total_len;
    cmd_status_t s = cmd_scan(&scanner, &entry);
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, s);
    TEST_ASSERT_EQUAL_UINT16(0, entry.cmd_start);
    TEST_ASSERT_EQUAL_UINT16(13, entry.cmd_len); /* "set_led(1, 2)" */
    TEST_ASSERT_EQUAL_UINT8(7, entry.func_len);  /* "set_led" */
}

void test_cmd_scan_sticky_and_partial_packets(void)
{
    /* 粘包 + 未完成半包: "ping()\r\nadd(10, 20)\r\nhello" */
    cmd_scanner_t scanner;
    cmd_entry_t entry;

    uint8_t buf[64] = "ping()\r\nadd(10, 20)\r\nhello";
    cmd_init(&scanner, buf, 26);

    /* 第 1 条完整包: ping() */
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_EQUAL_UINT16(0, entry.cmd_start);
    TEST_ASSERT_EQUAL_UINT16(6, entry.cmd_len);  /* "ping()" 精准长度 6 */
    TEST_ASSERT_EQUAL_UINT8(4, entry.func_len);

    /* 第 2 条完整包: add(10, 20) */
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_EQUAL_UINT16(8, entry.cmd_start);
    TEST_ASSERT_EQUAL_UINT16(11, entry.cmd_len); /* "add(10, 20)" 精准长度 11 */
    TEST_ASSERT_EQUAL_UINT8(3, entry.func_len);

    /* 第 3 条未完整包: hello (尚无 \n) */
    TEST_ASSERT_EQUAL_INT(CMD_INCOMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_EQUAL_UINT16(26, scanner.scan_pos); /* 不回退 */

    /* 缓冲区中追加 \n 并递增 buf_size 到 27 */
    buf[26] = '\n';
    scanner.buf_size = 27;

    /* 再次调用 cmd_scan，成功解析出第 3 条完整包 */
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, cmd_scan(&scanner, &entry));
    TEST_ASSERT_EQUAL_UINT16(21, entry.cmd_start);
    TEST_ASSERT_EQUAL_UINT16(5, entry.cmd_len);  /* "hello" */
    TEST_ASSERT_EQUAL_UINT8(5, entry.func_len);  /* "hello" */
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
    RUN_TEST(test_cmd_scan_byte_by_byte);
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

    /* cmd_entry 专属边界测试 */
    void test_cmd_parse_entry_with_offset(void);
    void test_cmd_parse_entry_max_args(void);
    void test_cmd_parse_entry_invalid_buf_or_len(void);
    RUN_TEST(test_cmd_parse_entry_with_offset);
    RUN_TEST(test_cmd_parse_entry_max_args);
    RUN_TEST(test_cmd_parse_entry_invalid_buf_or_len);

    /* 零拷贝 */
    RUN_TEST(test_cmd_zero_copy);

    /* ringbuf 扫描与回绕 */
    void test_cmdscan_ringbuf_basic(void);
    void test_cmdscan_ringbuf_empty_buffer_no_runaway(void);
    void test_cmdscan_ringbuf_wrap_around_parse(void);
    RUN_TEST(test_cmdscan_ringbuf_basic);
    RUN_TEST(test_cmdscan_ringbuf_empty_buffer_no_runaway);
    RUN_TEST(test_cmdscan_ringbuf_wrap_around_parse);

    return UnityEnd();
}

/* ===== ringbuf 原生扫描用例 ===== */
static ringbuf_t s_ring;
static uint8_t s_ring_buf[64];
static cmd_scanner_t s_ring_scanner;

void test_cmdscan_ringbuf_basic(void)
{
    ringbuf_init(&s_ring, s_ring_buf, sizeof(s_ring_buf));
    cmd_init_ringbuf(&s_ring_scanner, &s_ring);

    const char* str = "hello(123)\n";
    ringbuf_write(&s_ring, (const uint8_t*)str, strlen(str));

    cmd_entry_t entry;
    cmd_status_t st = cmd_scan(&s_ring_scanner, &entry);
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, st);
    TEST_ASSERT_EQUAL_UINT16(10, entry.cmd_len);

    cmd_args_t args;
    uint8_t count = cmd_parse(&entry, &args);
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(5, args.func_name_len);
    TEST_ASSERT_EQUAL_UINT16(3, args.args[0].len);
}

void test_cmdscan_ringbuf_empty_buffer_no_runaway(void)
{
    memset(s_ring_buf, 0, sizeof(s_ring_buf));
    ringbuf_init(&s_ring, s_ring_buf, sizeof(s_ring_buf));
    cmd_init_ringbuf(&s_ring_scanner, &s_ring);

    cmd_entry_t entry;
    /* 空缓冲区下多次扫描，探查游标不应跑飞 */
    for (int i = 0; i < 5; i++)
    {
        cmd_status_t st = cmd_scan(&s_ring_scanner, &entry);
        TEST_ASSERT_EQUAL_INT(CMD_INCOMPLETE, st);
        TEST_ASSERT_EQUAL_UINT16(0, s_ring_scanner.scan_pos);
    }

    /* 随后写入数据，应正常成帧 */
    const char* str = "ping()\n";
    ringbuf_write(&s_ring, (const uint8_t*)str, strlen(str));

    cmd_status_t st = cmd_scan(&s_ring_scanner, &entry);
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, st);
    TEST_ASSERT_EQUAL_UINT16(6, entry.cmd_len);
}

void test_cmdscan_ringbuf_wrap_around_parse(void)
{
    /* 1. 构造一个 16 字节的 ringbuf，先把 tail 设为 12 (接近末尾) */
    uint8_t ring_mem[16];
    ringbuf_t ring;
    ringbuf_init(&ring, ring_mem, 16);
    ring.head = 12;
    ring.tail = 12;

    /* 2. 写入 "add(10, 20)\n" (12 字节)，将跨越 16 字节边界回绕到 0..7 */
    const char* str = "add(10, 20)\n";
    ringbuf_write(&ring, (const uint8_t*)str, strlen(str));

    cmd_scanner_t scanner;
    cmd_init_ringbuf(&scanner, &ring);

    cmd_entry_t entry;
    cmd_status_t st = cmd_scan(&scanner, &entry);
    TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, st);
    TEST_ASSERT_EQUAL_UINT16(12, entry.cmd_start); /* 起始位于 12 */
    TEST_ASSERT_EQUAL_UINT16(11, entry.cmd_len);   /* "add(10, 20)" 长度 11 */

    /* 3. 使用重构后的 cmd_parse 验证跨越回绕边界的安全切分 */
    cmd_args_t args;
    uint8_t count = cmd_parse(&entry, &args);
    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_UINT16(3, args.func_name_len);
    TEST_ASSERT_EQUAL_UINT16(2, args.args[0].len);
    TEST_ASSERT_EQUAL_UINT16(2, args.args[1].len);
}
