/**
 * test_ringbuf.c — Unit tests for ringbuf module
 */
#include "unity.h"
#include "test_helpers.h"
#include "ringbuf.h"
#include <string.h>

static ringbuf_t ring;
static uint8_t ring_buf[64];

/* ===== init ===== */
void test_ringbuf_init(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));
    TEST_ASSERT_EQUAL_UINT16(0, ring.head);
    TEST_ASSERT_EQUAL_UINT16(0, ring.tail);
    TEST_ASSERT_EQUAL_UINT16(0, ringbuf_readable(&ring));
    TEST_ASSERT_EQUAL_UINT16(63, ringbuf_writable(&ring));
#if RINGBUF_DMA
    TEST_ASSERT_NULL(ring.head_reader);
    TEST_ASSERT_NULL(ring.tail_reader);
#endif
}

/* ===== set head/tail ===== */
void test_ringbuf_set_head_tail(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    ringbuf_set_head(&ring, 10);
    TEST_ASSERT_EQUAL_UINT16(10, ring.head);
    TEST_ASSERT_EQUAL_UINT16(10, ringbuf_readable(&ring));

    ringbuf_set_tail(&ring, 5);
    TEST_ASSERT_EQUAL_UINT16(5, ring.tail);
    TEST_ASSERT_EQUAL_UINT16(5, ringbuf_readable(&ring));
}

/* ===== readable ===== */
void test_ringbuf_readable(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    ringbuf_set_head(&ring, 10);
    TEST_ASSERT_EQUAL_UINT16(10, ringbuf_readable(&ring));

    ring.tail = 5;
    TEST_ASSERT_EQUAL_UINT16(5, ringbuf_readable(&ring));

    ring.tail = 10;
    TEST_ASSERT_EQUAL_UINT16(0, ringbuf_readable(&ring));
}

void test_ringbuf_readable_wrap(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    ring.tail = 50;
    ringbuf_set_head(&ring, 5);
    /* head(5) < tail(50), wrap: size - tail + head = 64 - 50 + 5 = 19 */
    TEST_ASSERT_EQUAL_UINT16(19, ringbuf_readable(&ring));
}

/* ===== writable ===== */
void test_ringbuf_writable(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    TEST_ASSERT_EQUAL_UINT16(63, ringbuf_writable(&ring));

    ringbuf_set_head(&ring, 10);
    TEST_ASSERT_EQUAL_UINT16(53, ringbuf_writable(&ring));

    /* 尾部预留 1 字节，防止 head == tail 时无法区分空/满 */
    ringbuf_set_head(&ring, 63);
    TEST_ASSERT_EQUAL_UINT16(0, ringbuf_writable(&ring));
}

/* ===== write ===== */
void test_ringbuf_write(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    uint16_t n = ringbuf_write(&ring, data, 3);

    TEST_ASSERT_EQUAL_UINT16(3, n);
    TEST_ASSERT_EQUAL_UINT16(3, ringbuf_readable(&ring));
    TEST_ASSERT_EQUAL_UINT8(0xAA, ring_buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, ring_buf[1]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, ring_buf[2]);
}

void test_ringbuf_write_wrap(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    /* tail=10, head=60: 可写 = 64-1-(60-10) = 13 */
    ring.tail = 10;
    ring.head = 60;
    uint8_t data[] = {0xDD, 0xDD, 0xDD, 0xDD, 0xDD};
    uint16_t n = ringbuf_write(&ring, data, 5);

    TEST_ASSERT_EQUAL_UINT16(5, n);
    /* 前 4 字节到末尾, 后 1 字节回绕到开头 */
    TEST_ASSERT_EQUAL_UINT8(0xDD, ring_buf[60]);
    TEST_ASSERT_EQUAL_UINT8(0xDD, ring_buf[61]);
    TEST_ASSERT_EQUAL_UINT8(0xDD, ring_buf[62]);
    TEST_ASSERT_EQUAL_UINT8(0xDD, ring_buf[63]);
    TEST_ASSERT_EQUAL_UINT8(0xDD, ring_buf[0]);
}

void test_ringbuf_write_full(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    ring.head = 63;
    ring.tail = 0;
    /* 可写 = 0 */

    uint8_t data[] = {0xAA};
    uint16_t n = ringbuf_write(&ring, data, 1);
    TEST_ASSERT_EQUAL_UINT16(0, n);
}

/* ===== peek ===== */
void test_ringbuf_peek(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    memset(ring_buf, 0xAA, 10);
    ringbuf_set_head(&ring, 10);

    uint8_t dst[16];
    uint16_t n = ringbuf_peek(&ring, dst, 10);
    TEST_ASSERT_EQUAL_UINT16(10, n);
    TEST_ASSERT_EQUAL_MEMORY(ring_buf, dst, 10);
}

void test_ringbuf_peek_wrap(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    ring.tail = 60;
    memset(&ring_buf[60], 0xBB, 4);
    memset(&ring_buf[0], 0xCC, 5);
    ringbuf_set_head(&ring, 5);

    uint8_t dst[16];
    uint16_t n = ringbuf_peek(&ring, dst, 9);
    TEST_ASSERT_EQUAL_UINT16(9, n);

    uint8_t expected[9];
    memset(expected, 0xBB, 4);
    memset(expected + 4, 0xCC, 5);
    TEST_ASSERT_EQUAL_MEMORY(expected, dst, 9);
}

/* ===== skip ===== */
void test_ringbuf_skip(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    ringbuf_set_head(&ring, 20);
    ringbuf_skip(&ring, 5);
    TEST_ASSERT_EQUAL_UINT16(5, ring.tail);
    TEST_ASSERT_EQUAL_UINT16(15, ringbuf_readable(&ring));

    ringbuf_skip(&ring, 20);
    TEST_ASSERT_EQUAL_UINT16(20, ring.tail);
}

void test_ringbuf_skip_wrap(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    ring.tail = 60;
    ringbuf_set_head(&ring, 5);

    ringbuf_skip(&ring, 9);
    TEST_ASSERT_EQUAL_UINT16(5, ring.tail);
    TEST_ASSERT_EQUAL_UINT16(0, ringbuf_readable(&ring));
}

/* ===== read ===== */
void test_ringbuf_read(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    uint8_t src[] = {1, 2, 3, 4, 5};
    ringbuf_write(&ring, src, 5);

    uint8_t dst[10] = {0};
    uint16_t n = ringbuf_read(&ring, dst, 3);
    TEST_ASSERT_EQUAL_UINT16(3, n);
    TEST_ASSERT_EQUAL_UINT16(2, ringbuf_readable(&ring));
    TEST_ASSERT_EQUAL_UINT8(1, dst[0]);
    TEST_ASSERT_EQUAL_UINT8(2, dst[1]);
    TEST_ASSERT_EQUAL_UINT8(3, dst[2]);
}

void test_ringbuf_read_wrap(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    ring.tail = 60;
    memset(&ring_buf[60], 0xAA, 4);
    memset(&ring_buf[0], 0xBB, 4);
    ringbuf_set_head(&ring, 4);  /* 60->64 回绕 0->4, 共 8 字节 */

    uint8_t dst[10] = {0};
    uint16_t n = ringbuf_read(&ring, dst, 8);
    TEST_ASSERT_EQUAL_UINT16(8, n);
    TEST_ASSERT_EQUAL_UINT16(0, ringbuf_readable(&ring));
    TEST_ASSERT_EQUAL_UINT8(0xAA, dst[0]);
    TEST_ASSERT_EQUAL_UINT8(0xAA, dst[3]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, dst[4]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, dst[7]);
}

/* ===== empty and overflow bounds ===== */
void test_ringbuf_empty_overflow(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    /* 空状态测试 */
    uint8_t dst[5];
    TEST_ASSERT_EQUAL_UINT16(0, ringbuf_read(&ring, dst, 5));
    TEST_ASSERT_EQUAL_UINT16(0, ringbuf_peek(&ring, dst, 5));
    ringbuf_skip(&ring, 5);
    TEST_ASSERT_EQUAL_UINT16(0, ring.tail);

    /* 满状态溢出写入测试 */
    ring.head = 63;
    ring.tail = 0;
    TEST_ASSERT_EQUAL_UINT16(0, ringbuf_writable(&ring));
    uint8_t src[] = {1, 2, 3};
    TEST_ASSERT_EQUAL_UINT16(0, ringbuf_write(&ring, src, 3));
}

/* ===== flush ===== */
void test_ringbuf_flush(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));

    ringbuf_set_head(&ring, 30);
    TEST_ASSERT_EQUAL_UINT16(30, ringbuf_readable(&ring));

    ringbuf_flush(&ring);
    TEST_ASSERT_EQUAL_UINT16(0, ringbuf_readable(&ring));
    TEST_ASSERT_EQUAL_UINT16(30, ring.tail);
}

/* ===== hw func ===== */
#if RINGBUF_DMA
static volatile uint16_t test_rx_dma_ndtr;
static volatile uint16_t test_tx_dma_ndtr;

static uint16_t test_get_rx_head(void) { return 64 - test_rx_dma_ndtr; }
static uint16_t test_get_tx_tail(void) { return 64 - test_tx_dma_ndtr; }

void test_ringbuf_dma_rx_auto(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));
    ringbuf_set_head_reader(&ring, test_get_rx_head);

    test_rx_dma_ndtr = 54;  /* RX DMA 写了 10 字节 */
    TEST_ASSERT_EQUAL_UINT16(10, ringbuf_readable(&ring));

    test_rx_dma_ndtr = 40;  /* RX DMA 又写了 14 字节 (共 24) */
    TEST_ASSERT_EQUAL_UINT16(24, ringbuf_readable(&ring));

    ringbuf_skip(&ring, 10);
    TEST_ASSERT_EQUAL_UINT16(14, ringbuf_readable(&ring));
}

void test_ringbuf_dma_tx_auto(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));
    ringbuf_set_tail_reader(&ring, test_get_tx_tail);

    uint8_t data[20] = {0};
    ringbuf_write(&ring, data, 20);
    TEST_ASSERT_EQUAL_UINT16(20, ring.head);

    test_tx_dma_ndtr = 64;  /* TX DMA 从 0 刚开始发 */
    TEST_ASSERT_EQUAL_UINT16(20, ringbuf_readable(&ring));

    test_tx_dma_ndtr = 54;  /* TX DMA 已由硬件发走 10 字节 */
    TEST_ASSERT_EQUAL_UINT16(10, ringbuf_readable(&ring));
    TEST_ASSERT_EQUAL_UINT16(53, ringbuf_writable(&ring));
}

void test_ringbuf_dma_rxtx_both(void)
{
    /* 双 DMA 硬件中转模式：同时挂载 head_reader (RX) 和 tail_reader (TX) */
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));
    ringbuf_set_head_reader(&ring, test_get_rx_head);
    ringbuf_set_tail_reader(&ring, test_get_tx_tail);

    test_rx_dma_ndtr = 44;  /* RX DMA 写入 20 字节 (head = 20) */
    test_tx_dma_ndtr = 59;  /* TX DMA 消费发走 5 字节 (tail = 5) */

    TEST_ASSERT_EQUAL_UINT16(15, ringbuf_readable(&ring));
    TEST_ASSERT_EQUAL_UINT16(48, ringbuf_writable(&ring));
}

void test_ringbuf_dma_tx_fetch_complete(void)
{
    ringbuf_init(&ring, ring_buf, sizeof(ring_buf));
    uint8_t data[] = "Hello DMA World!";
    ringbuf_write(&ring, data, 16);

    const uint8_t* ptr = NULL;
    uint16_t len = 0;

    uint8_t res = ringbuf_dma_tx_fetch(&ring, &ptr, &len);
    TEST_ASSERT_EQUAL_UINT8(1, res);
    TEST_ASSERT_EQUAL_PTR(&ring_buf[0], ptr);
    TEST_ASSERT_EQUAL_UINT16(16, len);

    const uint8_t* ptr2 = NULL;
    uint16_t len2 = 0;
    TEST_ASSERT_EQUAL_UINT8(0, ringbuf_dma_tx_fetch(&ring, &ptr2, &len2));

    const uint8_t* next_ptr = NULL;
    uint16_t next_len = 0;
    uint8_t has_next = ringbuf_dma_tx_complete(&ring, &next_ptr, &next_len);
    TEST_ASSERT_EQUAL_UINT8(0, has_next);
    TEST_ASSERT_EQUAL_UINT16(0, ringbuf_readable(&ring));
}
#endif

/* ===== runner ===== */
int run_ringbuf_tests(void)
{
    UnityBegin("test_ringbuf_buf.c");

    RUN_TEST(test_ringbuf_init);
    RUN_TEST(test_ringbuf_set_head_tail);
    RUN_TEST(test_ringbuf_readable);
    RUN_TEST(test_ringbuf_readable_wrap);
    RUN_TEST(test_ringbuf_writable);
    RUN_TEST(test_ringbuf_write);
    RUN_TEST(test_ringbuf_write_wrap);
    RUN_TEST(test_ringbuf_write_full);
    RUN_TEST(test_ringbuf_peek);
    RUN_TEST(test_ringbuf_peek_wrap);
    RUN_TEST(test_ringbuf_skip);
    RUN_TEST(test_ringbuf_skip_wrap);
    RUN_TEST(test_ringbuf_read);
    RUN_TEST(test_ringbuf_read_wrap);
    RUN_TEST(test_ringbuf_empty_overflow);
    RUN_TEST(test_ringbuf_flush);
#if RINGBUF_DMA
    RUN_TEST(test_ringbuf_dma_rx_auto);
    RUN_TEST(test_ringbuf_dma_tx_auto);
    RUN_TEST(test_ringbuf_dma_rxtx_both);
    RUN_TEST(test_ringbuf_dma_tx_fetch_complete);
#endif

    return UnityEnd();
}
