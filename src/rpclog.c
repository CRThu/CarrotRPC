/****************************
 * RPC_LOG - 分级日志库
 * CarrotRPC
 *
 * 自实现格式化，零 printf 依赖
 * 复用 typeconv 做数字转换
 *****************************/
#include "rpclog.h"
#include "typeconv.h"
#include <string.h>
#include <stdarg.h>

/*=============================================================
 * 内部状态
 *=============================================================*/

static rpc_log_out_fn s_out_fn = NULL;
static uint8_t s_min_level = RPC_LOG_DEBUG;

/* 缓冲区模式内部缓冲 */
#if RPC_LOG_OUTPUT_BUF
#define RPC_LOG_BUF_SIZE 128
static char s_buf[RPC_LOG_BUF_SIZE];
static uint16_t s_buf_pos = 0;
#endif

/*=============================================================
 * 内部：底层输出
 *=============================================================*/

static void rpc_log_putc(char c)
{
    if (s_out_fn == NULL) return;

#if RPC_LOG_OUTPUT_BUF
    if (s_buf_pos < RPC_LOG_BUF_SIZE)
    {
        s_buf[s_buf_pos++] = c;
    }
    /* 缓冲区满或遇到换行，刷新 */
    if (s_buf_pos >= RPC_LOG_BUF_SIZE || c == '\n')
    {
        s_out_fn(s_buf, s_buf_pos);
        s_buf_pos = 0;
    }
#else
    s_out_fn(c);
#endif
}

static void rpc_log_puts(const char* s)
{
    if (s == NULL) return;
    while (*s)
    {
        rpc_log_putc(*s++);
    }
}

static void rpc_log_flush(void)
{
#if RPC_LOG_OUTPUT_BUF
    if (s_buf_pos > 0 && s_out_fn != NULL)
    {
        s_out_fn(s_buf, s_buf_pos);
        s_buf_pos = 0;
    }
#endif
}

/*=============================================================
 * 内部：级别前缀输出
 *=============================================================*/

static void rpc_log_put_level(uint8_t level)
{
    rpc_log_putc('[');
    switch (level)
    {
    case RPC_LOG_DEBUG:   rpc_log_puts("DEBUG");   break;
    case RPC_LOG_INFO:    rpc_log_puts("INFO");    break;
    case RPC_LOG_WARN:    rpc_log_puts("WARN");    break;
    case RPC_LOG_ERROR:   rpc_log_puts("ERROR");   break;
    case RPC_LOG_RETURN:  rpc_log_puts("RETURN");  break;
    case RPC_LOG_DATA:    rpc_log_puts("DATA");    break;
    case RPC_LOG_REG:     rpc_log_puts("REG");     break;
    default:              rpc_log_puts("???");     break;
    }
    rpc_log_puts("]: ");
}

/*=============================================================
 * 公开 API：设置
 *=============================================================*/

void rpc_log_set_output(rpc_log_out_fn fn)
{
    s_out_fn = fn;
}

void rpc_log_set_level(uint8_t level)
{
    s_min_level = level;
}

/*=============================================================
 * 公开 API：核心日志 (varargs)
 *=============================================================*/

void rpc_log(uint8_t level, const char* fmt, ...)
{
    if (level <= RPC_LOG_ERROR && level < s_min_level) return;
    if (fmt == NULL) return;

    rpc_log_put_level(level);

    va_list ap;
    va_start(ap, fmt);

    while (*fmt)
    {
        if (*fmt != '%')
        {
            rpc_log_putc(*fmt++);
            continue;
        }

        fmt++; /* skip '%' */

        switch (*fmt)
        {
        case 'd':
        {
            int64_t val = (int64_t)va_arg(ap, int);
            char buf[24];
            typeconv_from_i64(val, buf, sizeof(buf));
            rpc_log_puts(buf);
            break;
        }
        case 'u':
        {
            uint64_t val = (uint64_t)va_arg(ap, unsigned int);
            char buf[24];
            typeconv_from_i64((int64_t)val, buf, sizeof(buf));
            rpc_log_puts(buf);
            break;
        }
        case 'x':
        case 'X':
        {
            uint64_t val = (uint64_t)va_arg(ap, unsigned int);
            char buf[24];
            typeconv_from_u64(val, buf, sizeof(buf));
            rpc_log_puts(buf);
            break;
        }
        case 's':
        {
            const char* s = va_arg(ap, const char*);
            rpc_log_puts(s ? s : "(null)");
            break;
        }
        case 'c':
        {
            char c = (char)va_arg(ap, int);
            rpc_log_putc(c);
            break;
        }
        case '%':
            rpc_log_putc('%');
            break;
        default:
            rpc_log_putc('%');
            rpc_log_putc(*fmt);
            break;
        }

        fmt++;
    }

    va_end(ap);

    rpc_log_puts("\r\n");
    rpc_log_flush();
}

/*=============================================================
 * 公开 API：类型输出
 *=============================================================*/

void rpc_log_i64(uint8_t level, const char* tag, int64_t val)
{
    if (level <= RPC_LOG_ERROR && level < s_min_level) return;

    char buf[24];
    rpc_log_put_level(level);
    if (tag != NULL && *tag != '\0')
    {
        rpc_log_puts(tag);
        rpc_log_putc('=');
    }
    typeconv_from_i64(val, buf, sizeof(buf));
    rpc_log_puts(buf);
    rpc_log_puts("\r\n");
    rpc_log_flush();
}

void rpc_log_u64(uint8_t level, const char* tag, uint64_t val)
{
    if (level <= RPC_LOG_ERROR && level < s_min_level) return;

    char buf[24];
    rpc_log_put_level(level);
    if (tag != NULL && *tag != '\0')
    {
        rpc_log_puts(tag);
        rpc_log_putc('=');
    }
    typeconv_from_u64(val, buf, sizeof(buf));
    rpc_log_puts(buf);
    rpc_log_puts("\r\n");
    rpc_log_flush();
}

void rpc_log_hex(uint8_t level, const char* tag, uint64_t val)
{
    if (level <= RPC_LOG_ERROR && level < s_min_level) return;

    char buf[24];
    rpc_log_put_level(level);
    if (tag != NULL && *tag != '\0')
    {
        rpc_log_puts(tag);
        rpc_log_putc('=');
    }
    typeconv_from_u64(val, buf, sizeof(buf));
    rpc_log_puts(buf);
    rpc_log_puts("\r\n");
    rpc_log_flush();
}

void rpc_log_f64(uint8_t level, const char* tag, double val, uint8_t prec)
{
    if (level <= RPC_LOG_ERROR && level < s_min_level) return;

    char buf[32];
    rpc_log_put_level(level);
    if (tag != NULL && *tag != '\0')
    {
        rpc_log_puts(tag);
        rpc_log_putc('=');
    }
    typeconv_from_f64(val, buf, sizeof(buf), prec);
    rpc_log_puts(buf);
    rpc_log_puts("\r\n");
    rpc_log_flush();
}

void rpc_log_str(uint8_t level, const char* tag, const char* str)
{
    if (level <= RPC_LOG_ERROR && level < s_min_level) return;

    rpc_log_put_level(level);
    if (tag != NULL && *tag != '\0')
    {
        rpc_log_puts(tag);
        rpc_log_putc('=');
    }
    rpc_log_puts(str ? str : "(null)");
    rpc_log_puts("\r\n");
    rpc_log_flush();
}
