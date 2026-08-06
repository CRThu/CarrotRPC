/****************************
* CMD - 零拷贝命令解析器
* CRTHu
* 2025.07.15
*****************************/
#include "cmdscan.h"
#include <stddef.h>
#include <string.h>

/*=============================================================
 * 内部辅助函数（无 std 库依赖）
 *=============================================================*/

/**
 * @brief 检查字符是否为空白字符
 */
static inline int8_t is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/**
 * @brief 检查字符是否为终止符
 */
static inline int8_t is_terminator(char c)
{
    return c == '\n' || c == '\0';
}

/**
 * @brief 检查字符是否为左括号
 */
static inline int8_t is_left_bracket(char c)
{
    return c == '(';
}

/**
 * @brief 检查字符是否为右括号
 */
static inline int8_t is_right_bracket(char c)
{
    return c == ')';
}

/**
 * @brief 检查字符是否为分隔符
 */
static inline int8_t is_separator(char c)
{
    return c == ',' || c == ';';
}

/*=============================================================
 * 扫描器 API
 *=============================================================*/

void cmd_init(cmd_scanner_t* scanner, const uint8_t* buf, uint16_t buf_size)
{
    if (scanner == NULL) return;

    scanner->buf = buf;
    scanner->buf_size = buf_size;
    scanner->scan_pos = 0;
    scanner->cmd_start = 0;
    scanner->func_len = 0;
    scanner->state = CMD_STATE_IDLE;
    scanner->ring = NULL;
}

void cmd_init_ringbuf(cmd_scanner_t* scanner, ringbuf_t* ring)
{
    if (scanner == NULL) return;

    scanner->ring = ring;
    scanner->buf = (ring != NULL) ? ring->buf : NULL;
    scanner->buf_size = (ring != NULL) ? ring->size : 0;
    scanner->scan_pos = (ring != NULL) ? ring->tail : 0;
    scanner->cmd_start = scanner->scan_pos;
    scanner->func_len = 0;
    scanner->state = CMD_STATE_IDLE;
}

void cmd_reset(cmd_scanner_t* scanner)
{
    if (scanner == NULL) return;

    scanner->scan_pos = (scanner->ring != NULL) ? scanner->ring->tail : 0;
    scanner->cmd_start = scanner->scan_pos;
    scanner->func_len = 0;
    scanner->state = CMD_STATE_IDLE;
}

cmd_status_t cmd_scan(cmd_scanner_t* scanner, cmd_entry_t* entry)
{
    if (scanner == NULL || entry == NULL || scanner->buf == NULL || scanner->buf_size == 0)
        return CMD_ERROR;

    entry->buf = scanner->buf;
    entry->buf_len = scanner->buf_size;
    entry->cmd_start = 0;
    entry->cmd_len = 0;
    entry->func_len = 0;

    uint16_t max_scan_bytes = 0;

    if (scanner->ring != NULL)
    {
        /* 环形缓冲区模式：实时查询可读字节数（若开启 DMA，内部自动拉取 DMA RX 位置） */
        uint16_t readable = ringbuf_readable(scanner->ring);

        /* 若外部消费了 ringbuf (tail 推进)，同步重置 scan_pos */
        if (scanner->scan_pos < scanner->ring->tail)
        {
            scanner->scan_pos = scanner->ring->tail;
            scanner->cmd_start = scanner->scan_pos;
        }

        uint16_t inspected = scanner->scan_pos - scanner->ring->tail;
        if (inspected >= readable)
        {
            return CMD_INCOMPLETE;
        }
        max_scan_bytes = readable - inspected;
    }
    else
    {
        /* 线性缓冲区模式：可读范围到 buf_size 截止 */
        if (scanner->scan_pos >= scanner->buf_size)
        {
            return CMD_INCOMPLETE;
        }
        max_scan_bytes = scanner->buf_size - scanner->scan_pos;
    }

    uint16_t scanned_cnt = 0;
    while (scanned_cnt < max_scan_bytes)
    {
        uint16_t phys_pos = (scanner->ring != NULL) ? (scanner->scan_pos % scanner->buf_size) : scanner->scan_pos;
        char c = (char)scanner->buf[phys_pos];

        switch (scanner->state)
        {
        case CMD_STATE_IDLE:
            /* 跳过前导空白、终止符与分隔符 */
            if (is_space(c) || is_terminator(c) || is_separator(c))
            {
                scanner->scan_pos++;
                scanned_cnt++;
                break;
            }
            /* 遇到有效命令首字符 */
            scanner->cmd_start = scanner->scan_pos;
            scanner->func_len = 0;
            scanner->state = CMD_STATE_FUNC_NAME;
            break;

        case CMD_STATE_FUNC_NAME:
            if (is_terminator(c) || is_separator(c))
            {
                /* 无参无括号命令结束，例如 "print\n" 或 "print;" */
                entry->cmd_start = scanner->cmd_start;
                entry->cmd_len = scanner->scan_pos - scanner->cmd_start;
                entry->func_len = (uint8_t)entry->cmd_len;
                scanner->scan_pos++; /* 跳过终止符/分隔符 */
                scanner->state = CMD_STATE_IDLE;
                return CMD_COMPLETE;
            }
            else if (is_left_bracket(c))
            {
                /* 遇到 '('，锁定函数名长度并进入括号参数状态 */
                scanner->func_len = (uint8_t)(scanner->scan_pos - scanner->cmd_start);
                scanner->state = CMD_STATE_PAREN_ARGS;
                scanner->scan_pos++;
                scanned_cnt++;
            }
            else if (is_space(c))
            {
                /* 遇到空格，锁定函数名长度并进入无括号参数状态 */
                scanner->func_len = (uint8_t)(scanner->scan_pos - scanner->cmd_start);
                scanner->state = CMD_STATE_SPACE_ARGS;
                scanner->scan_pos++;
                scanned_cnt++;
            }
            else
            {
                scanner->scan_pos++;
                scanned_cnt++;
            }
            break;

        case CMD_STATE_PAREN_ARGS:
            if (is_terminator(c))
            {
                /* 未遇到右括号直接终止 */
                entry->cmd_start = scanner->cmd_start;
                entry->cmd_len = scanner->scan_pos - scanner->cmd_start;
                entry->func_len = scanner->func_len;
                scanner->scan_pos++; /* 跳过终止符 */
                scanner->state = CMD_STATE_IDLE;
                return CMD_COMPLETE;
            }
            else if (is_right_bracket(c))
            {
                /* 遇到右括号 ')'，带括号命令成帧完成 */
                scanner->scan_pos++; /* 跳过 ')' */
                scanned_cnt++;
                entry->cmd_start = scanner->cmd_start;
                entry->cmd_len = scanner->scan_pos - scanner->cmd_start;
                entry->func_len = scanner->func_len;

                /* 跳过尾随的分隔符、空白或终止符 */
                while (scanned_cnt < max_scan_bytes)
                {
                    uint16_t next_phys = (scanner->ring != NULL) ? (scanner->scan_pos % scanner->buf_size) : scanner->scan_pos;
                    char c2 = (char)scanner->buf[next_phys];
                    if (is_terminator(c2) || is_separator(c2))
                    {
                        scanner->scan_pos++; /* 跳过终止符/分隔符 */
                        scanned_cnt++;
                        break;
                    }
                    if (is_space(c2))
                    {
                        scanner->scan_pos++;
                        scanned_cnt++;
                    }
                    else
                    {
                        break;
                    }
                }

                scanner->state = CMD_STATE_IDLE;
                return CMD_COMPLETE;
            }
            else
            {
                scanner->scan_pos++;
                scanned_cnt++;
            }
            break;

        case CMD_STATE_SPACE_ARGS:
            if (is_terminator(c) || is_separator(c))
            {
                entry->cmd_start = scanner->cmd_start;
                entry->cmd_len = scanner->scan_pos - scanner->cmd_start;
                entry->func_len = scanner->func_len;
                scanner->scan_pos++; /* 跳过终止符/分隔符 */
                scanner->state = CMD_STATE_IDLE;
                return CMD_COMPLETE;
            }
            else
            {
                scanner->scan_pos++;
                scanned_cnt++;
            }
            break;
        }
    }

    /* 缓冲区扫描完但数据未成帧，维持当前状态与位置 */
    return CMD_INCOMPLETE;
}

/*=============================================================
 * 参数解析 API（零拷贝 + 环形回绕安全）
 *=============================================================*/

/**
 * @brief 安全获取 entry 中相较 cmd_start 的第 offset 个字符（考虑 ringbuf 取模回绕）
 */
static inline char get_entry_char(const cmd_entry_t* entry, uint16_t offset)
{
    if (entry->buf_len > 0)
    {
        uint16_t pos = (uint16_t)((entry->cmd_start + offset) % entry->buf_len);
        return (char)entry->buf[pos];
    }
    return (char)entry->buf[entry->cmd_start + offset];
}

/**
 * @brief 获取 entry 中相较 cmd_start 的第 offset 个字符在 buf 中的绝对指针
 */
static inline const char* get_entry_ptr(const cmd_entry_t* entry, uint16_t offset)
{
    if (entry->buf_len > 0)
    {
        uint16_t pos = (uint16_t)((entry->cmd_start + offset) % entry->buf_len);
        return (const char*)&entry->buf[pos];
    }
    return (const char*)&entry->buf[entry->cmd_start + offset];
}

uint8_t cmd_parse(const cmd_entry_t* entry, cmd_args_t* args)
{
    if (entry == NULL || args == NULL || entry->buf == NULL || entry->cmd_len == 0)
        return 0xFF; /* 错误 */

    /* 清空结果 */
    args->func_name = NULL;
    args->func_name_len = 0;
    args->args_count = 0;

    uint16_t len = entry->cmd_len;
    uint16_t pos = 0;

    /* 1. 跳过前导空白 */
    while (pos < len && is_space(get_entry_char(entry, pos)))
    {
        pos++;
    }

    /* 2. 解析函数名（遇到括号、空白、分隔符停止） */
    uint16_t name_start = pos;
    while (pos < len && !is_left_bracket(get_entry_char(entry, pos)) &&
           !is_space(get_entry_char(entry, pos)) && !is_separator(get_entry_char(entry, pos)))
    {
        pos++;
    }

    if (pos == name_start)
        return 0xFF; /* 没有函数名 */

    args->func_name = get_entry_ptr(entry, name_start);
    args->func_name_len = pos - name_start;

    /* 3. 跳过函数名后的空白 */
    while (pos < len && is_space(get_entry_char(entry, pos)))
    {
        pos++;
    }

    /* 4. 检查是否有左括号 */
    if (pos >= len || !is_left_bracket(get_entry_char(entry, pos)))
    {
        /* 无括号形式：检查是否有参数（空格分隔） */
        if (pos < len && !is_terminator(get_entry_char(entry, pos)))
        {
            uint8_t arg_idx = 0;

            while (pos < len && arg_idx < CMD_MAX_ARGS)
            {
                /* 跳过参数前空白 */
                while (pos < len && is_space(get_entry_char(entry, pos)))
                {
                    pos++;
                }

                if (pos >= len || is_terminator(get_entry_char(entry, pos)))
                {
                    break;
                }

                /* 计算当前 token 长度 */
                uint16_t t_start = pos;
                while (pos < len)
                {
                    char c = get_entry_char(entry, pos);
                    if (is_separator(c) || is_space(c) || is_right_bracket(c))
                    {
                        break;
                    }
                    pos++;
                }
                uint16_t t_len = pos - t_start;

                if (t_len > 0)
                {
                    /* 去除首尾空白 */
                    uint16_t i = 0, j = t_len;
                    while (i < j && is_space(get_entry_char(entry, t_start + i))) i++;
                    while (j > i && is_space(get_entry_char(entry, t_start + j - 1))) j--;
                    uint16_t trim_len = j - i;

                    if (trim_len > 0)
                    {
                        args->args[arg_idx].ptr = get_entry_ptr(entry, t_start + i);
                        args->args[arg_idx].len = trim_len;
                        arg_idx++;
                    }
                }

                /* 跳过分隔符（逗号/分号） */
                if (pos < len && is_separator(get_entry_char(entry, pos)))
                {
                    pos++;
                }
            }

            args->args_count = arg_idx;
            return arg_idx;
        }

        /* 无参函数 */
        return 0;
    }

    pos++; /* 跳过左括号 */

    /* 5. 解析参数（带括号形式） */
    uint8_t arg_idx = 0;

    while (pos < len && arg_idx < CMD_MAX_ARGS)
    {
        /* 跳过参数前空白 */
        while (pos < len && is_space(get_entry_char(entry, pos)))
        {
            pos++;
        }

        /* 检查是否到达右括号或终止符 */
        if (pos < len && (is_right_bracket(get_entry_char(entry, pos)) || is_terminator(get_entry_char(entry, pos))))
        {
            break;
        }

        /* 计算当前 token 长度 */
        uint16_t t_start = pos;
        while (pos < len)
        {
            char c = get_entry_char(entry, pos);
            if (is_separator(c) || is_space(c) || is_right_bracket(c))
            {
                break;
            }
            pos++;
        }
        uint16_t t_len = pos - t_start;

        if (t_len > 0)
        {
            /* 去除首尾空白 */
            uint16_t i = 0, j = t_len;
            while (i < j && is_space(get_entry_char(entry, t_start + i))) i++;
            while (j > i && is_space(get_entry_char(entry, t_start + j - 1))) j--;
            uint16_t trim_len = j - i;

            if (trim_len > 0)
            {
                args->args[arg_idx].ptr = get_entry_ptr(entry, t_start + i);
                args->args[arg_idx].len = trim_len;
                arg_idx++;
            }
        }

        /* 跳过分隔符 */
        if (pos < len && is_separator(get_entry_char(entry, pos)))
        {
            pos++;
        }
    }

    args->args_count = arg_idx;
    return arg_idx;
}


