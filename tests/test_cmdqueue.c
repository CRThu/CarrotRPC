/**
 * test_cmdqueue.c — Unit tests for cmdqueue module
 */
#include "unity.h"
#include "test_helpers.h"
#include "cmdqueue.h"
#include "cmdscan.h"
#include <string.h>

static cmd_queue_t queue;

/* ===== init ===== */
void test_cmdqueue_init(void)
{
    cmd_queue_init(&queue);
    TEST_ASSERT_TRUE(cmd_queue_is_empty(&queue));
    TEST_ASSERT_EQUAL_UINT8(0, cmd_queue_count(&queue));
}

/* ===== push / pop ===== */
void test_cmdqueue_push_pop_simple(void)
{
    cmd_queue_init(&queue);

    cmd_scanner_t scanner;
    cmd_init(&scanner, (const uint8_t*)"print(1,2)", 10);

    cmd_entry_t entry;
    cmd_scan(&scanner, &entry);

    cmd_queue_status_t s = cmd_queue_push(&queue, &entry);
    TEST_ASSERT_EQUAL_INT(CMDQUEUE_OK, s);
    TEST_ASSERT_FALSE(cmd_queue_is_empty(&queue));
    TEST_ASSERT_EQUAL_UINT8(1, cmd_queue_count(&queue));

    cmd_entry_t entry_out;
    s = cmd_queue_pop(&queue, &entry_out);
    TEST_ASSERT_EQUAL_INT(CMDQUEUE_OK, s);
    TEST_ASSERT_EQUAL_UINT16(10, entry_out.cmd_len);
    TEST_ASSERT_EQUAL_UINT8(5, entry_out.func_len);

    const char* cmd = entry_out.buf + entry_out.cmd_start;
    TEST_ASSERT_EQUAL_MEMORY("print(1,2)", cmd, 10);

    TEST_ASSERT_TRUE(cmd_queue_is_empty(&queue));
}

void test_cmdqueue_push_pop_fifo(void)
{
    cmd_queue_init(&queue);

    cmd_scanner_t scanner;
    cmd_entry_t entry1, entry2;

    /* cmd_a\n cmd_b\n - 两条命令，每条 5 字符 + \n */
    cmd_init(&scanner, (const uint8_t*)"cmd_a\ncmd_b\n", 12);
    cmd_scan(&scanner, &entry1);
    cmd_scan(&scanner, &entry2);

    cmd_queue_push(&queue, &entry1);
    cmd_queue_push(&queue, &entry2);

    TEST_ASSERT_EQUAL_UINT8(2, cmd_queue_count(&queue));

    cmd_entry_t entry_out;
    cmd_queue_pop(&queue, &entry_out);
    const char* cmd = entry_out.buf + entry_out.cmd_start;
    TEST_ASSERT_EQUAL_MEMORY("cmd_a", cmd, 5);

    cmd_queue_pop(&queue, &entry_out);
    cmd = entry_out.buf + entry_out.cmd_start;
    TEST_ASSERT_EQUAL_MEMORY("cmd_b", cmd, 5);

    TEST_ASSERT_TRUE(cmd_queue_is_empty(&queue));
}

/* ===== full ===== */
void test_cmdqueue_push_full(void)
{
    cmd_queue_init(&queue);

    cmd_scanner_t scanner;
    cmd_init(&scanner, (const uint8_t*)"x\n", 2);

    cmd_entry_t entry;
    cmd_scan(&scanner, &entry);

    for (uint8_t i = 0; i < CMD_QUEUE_SIZE; i++)
    {
        cmd_queue_status_t s = cmd_queue_push(&queue, &entry);
        TEST_ASSERT_EQUAL_INT(CMDQUEUE_OK, s);
    }

    TEST_ASSERT_TRUE(cmd_queue_is_full(&queue));

    cmd_queue_status_t s = cmd_queue_push(&queue, &entry);
    TEST_ASSERT_EQUAL_INT(CMDQUEUE_ERR_FULL, s);
}

/* ===== pop empty ===== */
void test_cmdqueue_pop_empty(void)
{
    cmd_queue_init(&queue);

    cmd_entry_t entry_out;
    cmd_queue_status_t s = cmd_queue_pop(&queue, &entry_out);
    TEST_ASSERT_EQUAL_INT(CMDQUEUE_ERR_FULL, s);
}

/* ===== flush ===== */
void test_cmdqueue_flush(void)
{
    cmd_queue_init(&queue);

    cmd_scanner_t scanner;
    cmd_init(&scanner, (const uint8_t*)"test\n", 5);

    cmd_entry_t entry;
    cmd_scan(&scanner, &entry);
    cmd_queue_push(&queue, &entry);
    cmd_queue_push(&queue, &entry);

    TEST_ASSERT_EQUAL_UINT8(2, cmd_queue_count(&queue));

    cmd_queue_flush(&queue);

    TEST_ASSERT_TRUE(cmd_queue_is_empty(&queue));
    TEST_ASSERT_EQUAL_UINT8(0, cmd_queue_count(&queue));
}

/* ===== check ===== */
void test_cmdqueue_check_found(void)
{
    cmd_queue_init(&queue);

    cmd_scanner_t scanner;
    cmd_entry_t entry1, entry2;

    cmd_init(&scanner, (const uint8_t*)"start\nstop\n", 11);
    cmd_scan(&scanner, &entry1);
    cmd_scan(&scanner, &entry2);

    cmd_queue_push(&queue, &entry1);
    cmd_queue_push(&queue, &entry2);

    TEST_ASSERT_EQUAL_UINT8(1, cmd_queue_check(&queue, "stop"));
    TEST_ASSERT_EQUAL_UINT8(1, cmd_queue_check(&queue, "start"));
    TEST_ASSERT_EQUAL_UINT8(0, cmd_queue_check(&queue, "pause"));
}

void test_cmdqueue_check_empty(void)
{
    cmd_queue_init(&queue);
    TEST_ASSERT_EQUAL_UINT8(0, cmd_queue_check(&queue, "stop"));
}

void test_cmdqueue_check_partial_name(void)
{
    cmd_queue_init(&queue);

    cmd_scanner_t scanner;
    cmd_init(&scanner, (const uint8_t*)"stop_all\n", 9);

    cmd_entry_t entry;
    cmd_scan(&scanner, &entry);
    cmd_queue_push(&queue, &entry);

    TEST_ASSERT_EQUAL_UINT8(0, cmd_queue_check(&queue, "stop"));
    TEST_ASSERT_EQUAL_UINT8(1, cmd_queue_check(&queue, "stop_all"));
}

/* ===== func_len 一致性 ===== */
void test_cmdqueue_func_len_match_cmdscan(void)
{
    const char* test_cmds[] = {
        "led_on(1)\n",
        "stop\n",
        "print 1 2\n",
        "a;b;c\n",
        "cmd_with_long_name(arg1, arg2)\n",
    };
    uint8_t expected_func_len[] = {6, 4, 5, 1, 18};

    for (int i = 0; i < 5; i++)
    {
        cmd_scanner_t scanner;
        cmd_init(&scanner, (const uint8_t*)test_cmds[i], strlen(test_cmds[i]));

        cmd_entry_t entry;
        cmd_status_t status = cmd_scan(&scanner, &entry);
        TEST_ASSERT_EQUAL_INT(CMD_COMPLETE, status);
        TEST_ASSERT_EQUAL_UINT8(expected_func_len[i], entry.func_len);

        cmd_queue_init(&queue);
        cmd_queue_status_t s = cmd_queue_push(&queue, &entry);
        TEST_ASSERT_EQUAL_INT(CMDQUEUE_OK, s);

        /* 验证 check 能匹配 */
        char func_name[32];
        memcpy(func_name, test_cmds[i], entry.func_len);
        func_name[entry.func_len] = '\0';
        TEST_ASSERT_EQUAL_UINT8(1, cmd_queue_check(&queue, func_name));
    }
}

/* ===== buf 满 ===== */
void test_cmdqueue_buf_overflow(void)
{
    cmd_queue_init(&queue);

    char big[128];
    memset(big, 'A', sizeof(big));

    cmd_scanner_t scanner;
    uint8_t pushed = 0;
    for (uint8_t i = 0; i < CMD_QUEUE_SIZE; i++)
    {
        cmd_init(&scanner, (const uint8_t*)big, sizeof(big));
        cmd_entry_t entry;
        cmd_scan(&scanner, &entry);
        cmd_queue_status_t s = cmd_queue_push(&queue, &entry);
        if (s != CMDQUEUE_OK) break;
        pushed++;
    }

    TEST_ASSERT_TRUE(pushed < CMD_QUEUE_SIZE);

    cmd_entry_t entry_out;
    for (uint8_t i = 0; i < pushed; i++)
    {
        cmd_queue_pop(&queue, &entry_out);
        const char* cmd = entry_out.buf + entry_out.cmd_start;
        TEST_ASSERT_EQUAL_MEMORY(big, cmd, sizeof(big));
    }
}

/* ===== push NULL ===== */
void test_cmdqueue_push_invalid(void)
{
    cmd_queue_init(&queue);

    cmd_queue_status_t s = cmd_queue_push(&queue, NULL);
    TEST_ASSERT_EQUAL_INT(CMDQUEUE_ERR_NULL, s);
}

/* ===== cmd_parse 保留可用 ===== */
void test_cmd_parse_still_works(void)
{
    char cmd[] = "print(1,2)";
    cmd_args_t result;
    uint8_t count = cmd_parse(cmd, strlen(cmd), &result);

    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_UINT8(5, result.func_name_len);
    TEST_ASSERT_EQUAL_MEMORY("print", result.func_name, 5);
    TEST_ASSERT_EQUAL_MEMORY("1", result.args[0].ptr, result.args[0].len);
    TEST_ASSERT_EQUAL_MEMORY("2", result.args[1].ptr, result.args[1].len);
}

/* ===== runner ===== */
int run_cmdqueue_tests(void)
{
    UnityBegin("test_cmdqueue.c");

    RUN_TEST(test_cmdqueue_init);
    RUN_TEST(test_cmdqueue_push_pop_simple);
    RUN_TEST(test_cmdqueue_push_pop_fifo);
    RUN_TEST(test_cmdqueue_push_full);
    RUN_TEST(test_cmdqueue_pop_empty);
    RUN_TEST(test_cmdqueue_flush);
    RUN_TEST(test_cmdqueue_check_found);
    RUN_TEST(test_cmdqueue_check_empty);
    RUN_TEST(test_cmdqueue_check_partial_name);
    RUN_TEST(test_cmdqueue_func_len_match_cmdscan);
    RUN_TEST(test_cmdqueue_buf_overflow);
    RUN_TEST(test_cmdqueue_push_invalid);
    RUN_TEST(test_cmd_parse_still_works);

    return UnityEnd();
}
