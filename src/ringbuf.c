/****************************
* RINGBUF - 通用环形缓冲区
* CRTHu
* 2025.07.16
*****************************/
#include "ringbuf.h"
#include <string.h>

/*=============================================================
 * 内部游标获取抽象 (优先使用 DMA 硬件位置读取函数)
 *=============================================================*/
static inline uint16_t get_head(const ringbuf_t* ring)
{
#ifdef RINGBUF_DMA
    if (ring->head_reader) {
        return ring->head_reader();
    }
#endif
    return ring->head;
}

static inline uint16_t get_tail(const ringbuf_t* ring)
{
#ifdef RINGBUF_DMA
    if (ring->tail_reader) {
        return ring->tail_reader();
    }
#endif
    return ring->tail;
}

void ringbuf_init(ringbuf_t* ring, uint8_t* buf, uint16_t size)
{
    if (ring == NULL || buf == NULL || size == 0) return;

    ring->buf = buf;
    ring->size = size;
    ring->head = 0;
    ring->tail = 0;
#ifdef RINGBUF_DMA
    ring->head_reader = NULL;
    ring->tail_reader = NULL;
#endif
}

#ifdef RINGBUF_DMA
void ringbuf_set_head_reader(ringbuf_t* ring, uint16_t (*func)(void))
{
    if (ring == NULL) return;
    ring->head_reader = func;
}

void ringbuf_set_tail_reader(ringbuf_t* ring, uint16_t (*func)(void))
{
    if (ring == NULL) return;
    ring->tail_reader = func;
}

void ringbuf_sync_head(ringbuf_t* ring)
{
    if (ring == NULL || ring->head_reader == NULL) return;
    ring->head = ring->head_reader();
}

void ringbuf_sync_tail(ringbuf_t* ring)
{
    if (ring == NULL || ring->tail_reader == NULL) return;
    ring->tail = ring->tail_reader();
}
#endif

void ringbuf_set_head(ringbuf_t* ring, uint16_t head)
{
    if (ring == NULL) return;
    ring->head = head % ring->size;
}

void ringbuf_set_tail(ringbuf_t* ring, uint16_t tail)
{
    if (ring == NULL) return;
    ring->tail = tail % ring->size;
}

uint16_t ringbuf_readable(ringbuf_t* ring)
{
    if (ring == NULL) return 0;

    uint16_t head = get_head(ring);
    uint16_t tail = get_tail(ring);

    if (head >= tail)
        return head - tail;
    else
        return ring->size - tail + head;
}

uint16_t ringbuf_writable(ringbuf_t* ring)
{
    if (ring == NULL) return 0;
    return ring->size - 1 - ringbuf_readable(ring);
}

uint16_t ringbuf_peek(ringbuf_t* ring, uint8_t* dst, uint16_t len)
{
    if (ring == NULL || dst == NULL) return 0;

    uint16_t avail = ringbuf_readable(ring);
    if (len > avail)
        len = avail;

    uint16_t tail = get_tail(ring);
    uint16_t first = ring->size - tail;

    if (first >= len)
    {
        memcpy(dst, &ring->buf[tail], len);
    }
    else
    {
        memcpy(dst, &ring->buf[tail], first);
        memcpy(dst + first, &ring->buf[0], len - first);
    }

    return len;
}

void ringbuf_skip(ringbuf_t* ring, uint16_t len)
{
    if (ring == NULL) return;

    uint16_t avail = ringbuf_readable(ring);
    if (len > avail)
        len = avail;

    uint16_t tail = get_tail(ring);

#ifdef RINGBUF_DMA
    if (!ring->tail_reader)
#endif
    {
        ring->tail = (tail + len) % ring->size;
    }
}

uint16_t ringbuf_read(ringbuf_t* ring, uint8_t* dst, uint16_t len)
{
    if (ring == NULL || dst == NULL) return 0;

    uint16_t n = ringbuf_peek(ring, dst, len);
    if (n > 0)
    {
        ringbuf_skip(ring, n);
    }
    return n;
}

uint16_t ringbuf_write(ringbuf_t* ring, const uint8_t* src, uint16_t len)
{
    if (ring == NULL || src == NULL) return 0;

    uint16_t space = ringbuf_writable(ring);
    if (len > space)
        len = space;

    uint16_t head = get_head(ring);
    uint16_t first = ring->size - head;

    if (first >= len)
    {
        memcpy(&ring->buf[head], src, len);
    }
    else
    {
        memcpy(&ring->buf[head], src, first);
        memcpy(&ring->buf[0], src + first, len - first);
    }

#ifdef RINGBUF_DMA
    if (!ring->head_reader)
#endif
    {
        ring->head = (head + len) % ring->size;
    }

    return len;
}

void ringbuf_flush(ringbuf_t* ring)
{
    if (ring == NULL) return;

#ifdef RINGBUF_DMA
    if (!ring->tail_reader)
#endif
    {
        ring->tail = get_head(ring);
    }
}
