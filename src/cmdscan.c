/****************************
* CMD - 零拷贝命令解析器
* CRTHu
* 2025.07.15
*****************************/
#include "cmdscan.h"
#include <stddef.h>

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
}

void cmd_reset(cmd_scanner_t* scanner)
{
    if (scanner == NULL) return;

    scanner->scan_pos = 0;
}

cmd_status_t cmd_scan(cmd_scanner_t* scanner, cmd_entry_t* entry)
{
    if (scanner == NULL || entry == NULL || scanner->buf == NULL)
        return CMD_ERROR;

    entry->buf = scanner->buf;
    entry->buf_len = scanner->buf_size;
    entry->cmd_start = 0;
    entry->cmd_len = 0;
    entry->func_len = 0;

    /* 跳过前导空白和终止符 */
    while (scanner->scan_pos < scanner->buf_size)
    {
        char c = (char)scanner->buf[scanner->scan_pos];

        if (is_space(c) || is_terminator(c))
        {
            scanner->scan_pos++;
            continue;
        }

        break;
    }

    if (scanner->scan_pos >= scanner->buf_size)
    {
        return CMD_INCOMPLETE;
    }

    uint16_t cmd_start = scanner->scan_pos;

    /* 扫描函数名 */
    while (scanner->scan_pos < scanner->buf_size)
    {
        char c = (char)scanner->buf[scanner->scan_pos];

        if (is_terminator(c) || is_separator(c))
        {
            /* 遇到终止符或分隔符，命令结束 */
            entry->cmd_start = cmd_start;
            entry->cmd_len = scanner->scan_pos - cmd_start;
            entry->func_len = entry->cmd_len;
            scanner->scan_pos++; /* 跳过终止符/分隔符 */
            return CMD_COMPLETE;
        }

        if (is_left_bracket(c))
        {
            /* 遇到左括号，函数名结束 */
            entry->cmd_start = cmd_start;
            entry->func_len = scanner->scan_pos - cmd_start;

            /* 扫描到右括号或终止符 */
            scanner->scan_pos++; /* 跳过左括号 */
            while (scanner->scan_pos < scanner->buf_size)
            {
                c = (char)scanner->buf[scanner->scan_pos];
                if (is_terminator(c))
                {
                    entry->cmd_len = scanner->scan_pos - cmd_start;
                    scanner->scan_pos++; /* 跳过终止符 */
                    return CMD_COMPLETE;
                }
                if (is_right_bracket(c))
                {
                    scanner->scan_pos++;
                    /* 继续扫描到终止符或分隔符 */
                    while (scanner->scan_pos < scanner->buf_size &&
                           !is_terminator((char)scanner->buf[scanner->scan_pos]) &&
                           !is_separator((char)scanner->buf[scanner->scan_pos]))
                        scanner->scan_pos++;
                    entry->cmd_len = scanner->scan_pos - cmd_start;
                    if (scanner->scan_pos < scanner->buf_size)
                        scanner->scan_pos++; /* 跳过终止符/分隔符 */
                    return CMD_COMPLETE;
                }
                scanner->scan_pos++;
            }
            /* 缓冲区结束，未找到终止符 */
            entry->cmd_len = scanner->scan_pos - cmd_start;
            return CMD_INCOMPLETE;
        }

        if (is_space(c))
        {
            /* 遇到空白，函数名结束，无括号形式 */
            entry->cmd_start = cmd_start;
            entry->func_len = scanner->scan_pos - cmd_start;

            /* 继续扫描到终止符、分隔符或缓冲区结束 */
            while (scanner->scan_pos < scanner->buf_size &&
                   !is_terminator((char)scanner->buf[scanner->scan_pos]) &&
                   !is_separator((char)scanner->buf[scanner->scan_pos]))
                scanner->scan_pos++;

            entry->cmd_len = scanner->scan_pos - cmd_start;
            if (scanner->scan_pos < scanner->buf_size)
                scanner->scan_pos++; /* 跳过终止符/分隔符 */
            return CMD_COMPLETE;
        }

        scanner->scan_pos++;
    }

    /* 缓冲区结束，未找到终止符 - 命令本身完整 */
    entry->cmd_start = cmd_start;
    entry->cmd_len = scanner->scan_pos - cmd_start;
    if (entry->func_len == 0)
        entry->func_len = entry->cmd_len;
    return CMD_COMPLETE;
}

/*=============================================================
 * 参数解析 API（零拷贝）
 *=============================================================*/

/**
 * @brief 跳过空白字符
 */
static const char* skip_space(const char* p, uint16_t len, uint16_t* pos)
{
    while (*pos < len && is_space(p[*pos]))
    {
        (*pos)++;
    }
    return p;
}

/**
 * @brief 计算 token 长度（直到遇到分隔符或空白）
 */
static uint16_t token_len(const char* p, uint16_t len, uint16_t start)
{
    uint16_t i = start;

    while (i < len)
    {
        char c = p[i];

        if (is_separator(c) || is_space(c) || is_right_bracket(c))
        {
            break;
        }

        i++;
    }

    return i - start;
}

/**
 * @brief 去除首尾空白
 */
static void trim_token(const char* p, uint16_t len,
                       const char** start, uint16_t* trim_len)
{
    uint16_t i = 0;
    uint16_t j = len;

    /* 去除前导空白 */
    while (i < j && is_space(p[i]))
    {
        i++;
    }

    /* 去除尾部空白 */
    while (j > i && is_space(p[j - 1]))
    {
        j--;
    }

    *start = p + i;
    *trim_len = j - i;
}

uint8_t cmd_parse(const char* cmd, uint16_t len, cmd_args_t* args)
{
    if (cmd == NULL || args == NULL || len == 0)
        return 0xFF; /* 错误 */

    /* 清空结果 */
    args->func_name = NULL;
    args->func_name_len = 0;
    args->args_count = 0;

    uint16_t pos = 0;

    /* 1. 跳过前导空白 */
    skip_space(cmd, len, &pos);

    /* 2. 解析函数名（遇到括号、空白、分隔符停止） */
    uint16_t name_start = pos;
    while (pos < len && !is_left_bracket(cmd[pos]) && !is_space(cmd[pos]) && !is_separator(cmd[pos]))
    {
        pos++;
    }

    if (pos == name_start)
        return 0xFF; /* 没有函数名 */

    args->func_name = cmd + name_start;
    args->func_name_len = pos - name_start;

    /* 3. 跳过函数名后的空白 */
    skip_space(cmd, len, &pos);

    /* 4. 检查是否有左括号 */
    if (pos >= len || !is_left_bracket(cmd[pos]))
    {
        /* 无括号形式：检查是否有参数（空格分隔） */
        if (pos < len && !is_terminator(cmd[pos]))
        {
            /* 有参数，解析空格分隔的参数 */
            uint8_t arg_idx = 0;

            while (pos < len && arg_idx < CMD_MAX_ARGS)
            {
                /* 跳过参数前空白 */
                skip_space(cmd, len, &pos);

                /* 检查是否到达终止符 */
                if (pos >= len || is_terminator(cmd[pos]))
                {
                    break;
                }

                /* 计算当前 token 长度 */
                uint16_t t_len = token_len(cmd, len, pos);

                if (t_len > 0)
                {
                    /* 去除首尾空白 */
                    const char* token_start;
                    uint16_t token_length;
                    trim_token(cmd + pos, t_len, &token_start, &token_length);

                    if (token_length > 0)
                    {
                        args->args[arg_idx].ptr = token_start;
                        args->args[arg_idx].len = token_length;
                        arg_idx++;
                    }
                }

                pos += t_len;

                /* 跳过分隔符（逗号/分号） */
                if (pos < len && is_separator(cmd[pos]))
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

    /* 5. 解析参数 */
    uint8_t arg_idx = 0;

    while (pos < len && arg_idx < CMD_MAX_ARGS)
    {
        /* 跳过参数前空白 */
        skip_space(cmd, len, &pos);

        /* 检查是否到达右括号 */
        if (pos < len && is_right_bracket(cmd[pos]))
        {
            break;
        }

        /* 检查是否到达终止符 */
        if (pos < len && is_terminator(cmd[pos]))
        {
            break;
        }

        /* 计算当前 token 长度 */
        uint16_t t_len = token_len(cmd, len, pos);

        if (t_len > 0)
        {
            /* 去除首尾空白 */
            const char* token_start;
            uint16_t token_length;
            trim_token(cmd + pos, t_len, &token_start, &token_length);

            if (token_length > 0)
            {
                args->args[arg_idx].ptr = token_start;
                args->args[arg_idx].len = token_length;
                arg_idx++;
            }
        }

        pos += t_len;

        /* 跳过分隔符 */
        if (pos < len && is_separator(cmd[pos]))
        {
            pos++;
        }
    }

    args->args_count = arg_idx;
    return arg_idx;
}
