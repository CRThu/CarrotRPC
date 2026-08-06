#include "cmdqueue.h"
#include <string.h>

#if RPC_USE_CMD_QUEUE

void cmd_queue_init(cmd_queue_t* queue, uint8_t* buf, uint16_t buf_size)
{
    if (queue == NULL || buf == NULL || buf_size == 0) return;

    ringbuf_init(&queue->ring, buf, buf_size);
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
}

cmd_queue_status_t cmd_queue_push(cmd_queue_t* queue, cmd_entry_t* entry)
{
    if (queue == NULL || entry == NULL)
        return CMDQUEUE_ERR_NULL;

    if (entry->cmd_len == 0)
        return CMDQUEUE_ERR_NULL;

    if (queue->count >= CMD_QUEUE_SIZE)
        return CMDQUEUE_ERR_FULL;

    if (ringbuf_writable(&queue->ring) < entry->cmd_len)
        return CMDQUEUE_ERR_FULL;

    uint16_t written = 0;
    if (entry->buf_len > 0 && (entry->cmd_start + entry->cmd_len) > entry->buf_len)
    {
        /* 跨越回绕边界：分两截复制写入 queue->ring */
        uint16_t part1_len = entry->buf_len - entry->cmd_start;
        uint16_t part2_len = entry->cmd_len - part1_len;

        uint16_t w1 = ringbuf_write(&queue->ring, &entry->buf[entry->cmd_start], part1_len);
        uint16_t w2 = ringbuf_write(&queue->ring, &entry->buf[0], part2_len);
        written = w1 + w2;
    }
    else
    {
        const uint8_t* src = (const uint8_t*)&entry->buf[entry->cmd_start];
        written = ringbuf_write(&queue->ring, src, entry->cmd_len);
    }

    if (written != entry->cmd_len)
        return CMDQUEUE_ERR_FULL;

    cmd_entry_t* item = &queue->items[queue->tail];
    item->buf = queue->ring.buf;
    item->buf_len = queue->ring.size;
    item->cmd_start = (queue->ring.head - entry->cmd_len + queue->ring.size) % queue->ring.size;
    item->cmd_len = entry->cmd_len;
    item->func_len = entry->func_len;

    queue->tail = (queue->tail + 1) % CMD_QUEUE_SIZE;
    queue->count++;

    return CMDQUEUE_OK;
}

cmd_queue_status_t cmd_queue_pop(cmd_queue_t* queue, cmd_entry_t* entry)
{
    if (queue == NULL || entry == NULL)
        return CMDQUEUE_ERR_NULL;

    if (queue->count == 0)
        return CMDQUEUE_ERR_FULL;

    cmd_entry_t* item = &queue->items[queue->head];
    *entry = *item;

    ringbuf_skip(&queue->ring, item->cmd_len);

    queue->head = (queue->head + 1) % CMD_QUEUE_SIZE;
    queue->count--;

    return CMDQUEUE_OK;
}

uint8_t cmd_queue_is_empty(cmd_queue_t* queue)
{
    if (queue == NULL) return 1;
    return queue->count == 0;
}

uint8_t cmd_queue_is_full(cmd_queue_t* queue)
{
    if (queue == NULL) return 1;
    return queue->count >= CMD_QUEUE_SIZE;
}

uint8_t cmd_queue_count(cmd_queue_t* queue)
{
    if (queue == NULL) return 0;
    return queue->count;
}

void cmd_queue_flush(cmd_queue_t* queue)
{
    if (queue == NULL) return;

    ringbuf_flush(&queue->ring);
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
}

uint8_t cmd_queue_check(cmd_queue_t* queue, const char* func_name)
{
    if (queue == NULL || func_name == NULL) return 0;

    uint8_t idx = queue->head;

    for (uint8_t i = 0; i < queue->count; i++)
    {
        cmd_entry_t* item = &queue->items[idx];

        if (item->func_len > 0)
        {
            uint8_t fn_len = 0;
            while (func_name[fn_len] != '\0' && fn_len < 255)
                fn_len++;

            if (item->func_len == fn_len)
            {
                uint8_t match = 1;
                for (uint8_t j = 0; j < fn_len; j++)
                {
                    uint16_t pos = (item->cmd_start + j) % queue->ring.size;
                    if (item->buf[pos] != func_name[j])
                    {
                        match = 0;
                        break;
                    }
                }

                if (match)
                    return 1;
            }
        }

        idx = (idx + 1) % CMD_QUEUE_SIZE;
    }

    return 0;
}

#endif /* RPC_USE_CMD_QUEUE */
