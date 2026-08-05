/****************************
 * TYPECONV - 纯值转换模块
 * CarrotRPC
 *
 * 纯函数，无状态，无外部依赖
 *****************************/
#include "typeconv.h"
#include <string.h>

/*=============================================================
 * 字符串 -> 类型化值
 *=============================================================*/

static uint64_t parse_raw64(const char* str, uint16_t len, int is_signed)
{
    if (str == NULL || len == 0)
        return 0;

    uint16_t i = 0;
    int negative = 0;

    if (is_signed && i < len && str[i] == '-')
    {
        negative = 1;
        i++;
    }

    int is_hex = 0;
    if (len - i >= 2 && str[i] == '0' && (str[i + 1] == 'x' || str[i + 1] == 'X'))
    {
        is_hex = 1;
        i += 2;
    }
    else if (!is_signed)
    {
        for (uint16_t j = i; j < len; j++)
        {
            char c = str[j];
            if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
            {
                is_hex = 1;
                break;
            }
        }
    }

    uint64_t result = 0;

    if (is_hex)
    {
        for (; i < len; i++)
        {
            char c = str[i];
            if (c >= '0' && c <= '9')
            {
                result = (result << 4) | (c - '0');
            }
            else if (c >= 'a' && c <= 'f')
            {
                result = (result << 4) | (c - 'a' + 10);
            }
            else if (c >= 'A' && c <= 'F')
            {
                result = (result << 4) | (c - 'A' + 10);
            }
            else
            {
                continue;
            }
        }
    }
    else
    {
        for (; i < len; i++)
        {
            char c = str[i];
            if (c >= '0' && c <= '9')
            {
                result = result * 10 + (c - '0');
            }
            else
            {
                continue;
            }
        }
    }

    return negative ? (0ULL - result) : result;
}

int64_t typeconv_to_i64(const char* str, uint16_t len)
{
    return (int64_t)parse_raw64(str, len, 1);
}

uint64_t typeconv_to_u64(const char* str, uint16_t len)
{
    return parse_raw64(str, len, 0);
}

/*=============================================================
 * 类型化值 -> 字符串
 *=============================================================*/

uint16_t typeconv_from_i64(int64_t val, char* buf, uint16_t buf_size)
{
    if (buf == NULL || buf_size == 0)
        return 0;

    char tmp[24];
    uint16_t pos = 0;
    int negative = 0;

    if (val < 0)
    {
        negative = 1;
        val = -val;
    }

    if (val == 0)
    {
        tmp[pos++] = '0';
    }
    else
    {
        while (val > 0 && pos < sizeof(tmp))
        {
            tmp[pos++] = '0' + (val % 10);
            val /= 10;
        }
    }

    uint16_t total = pos + negative;
    if (total >= buf_size)
        total = buf_size - 1;

    uint16_t write_pos = 0;
    if (negative && write_pos < total)
        buf[write_pos++] = '-';

    for (int16_t j = pos - 1; j >= 0 && write_pos < total; j--)
        buf[write_pos++] = tmp[j];

    buf[write_pos] = '\0';
    return write_pos;
}

uint16_t typeconv_from_u64(uint64_t val, char* buf, uint16_t buf_size)
{
    if (buf == NULL || buf_size == 0)
        return 0;

    char tmp[20];
    uint16_t pos = 0;

    if (val == 0)
    {
        tmp[pos++] = '0';
    }
    else
    {
        const char* hex = "0123456789ABCDEF";
        while (val > 0 && pos < sizeof(tmp))
        {
            tmp[pos++] = hex[val & 0x0F];
            val >>= 4;
        }
    }

    uint16_t total = 2 + pos;
    if (total >= buf_size)
        total = buf_size - 1;

    uint16_t write_pos = 0;
    if (write_pos < total) buf[write_pos++] = '0';
    if (write_pos < total) buf[write_pos++] = 'x';

    for (int16_t j = pos - 1; j >= 0 && write_pos < total; j--)
        buf[write_pos++] = tmp[j];

    buf[write_pos] = '\0';
    return write_pos;
}

/*=============================================================
 * double 转换
 *=============================================================*/

double typeconv_to_f64(const char* str, uint16_t len)
{
    double result = 0.0;
    double sign = 1.0;
    uint16_t i = 0;

    if (i < len && str[i] == '-')
    {
        sign = -1.0;
        i++;
    }
    else if (i < len && str[i] == '+')
    {
        i++;
    }

    /* 跳过前导符号和空白后检查特殊值 */
    uint16_t start = i;

    /* inf/nan (大小写不敏感) */
    if (len - start >= 3)
    {
        char a = str[start] | 0x20;
        char b = str[start + 1] | 0x20;
        char c = str[start + 2] | 0x20;
        if (a == 'i' && b == 'n' && c == 'f')
            return sign * (1.0 / 0.0);
        if (a == 'n' && b == 'a' && c == 'n')
            return (0.0 / 0.0);
    }

    /* 整数部分 */
    for (; i < len; i++)
    {
        char c = str[i];
        if (c >= '0' && c <= '9')
            result = result * 10.0 + (c - '0');
        else
            break;
    }

    /* 小数部分 */
    if (i < len && str[i] == '.')
    {
        i++;
        double frac = 0.0;
        double base = 0.1;
        for (; i < len; i++)
        {
            char c = str[i];
            if (c >= '0' && c <= '9')
            {
                frac += (c - '0') * base;
                base *= 0.1;
            }
            else
                break;
        }
        result += frac;
    }

    /* 指数部分 */
    if (i < len && (str[i] == 'e' || str[i] == 'E'))
    {
        i++;
        int exp_sign = 1;
        int exp = 0;

        if (i < len && str[i] == '-')
        {
            exp_sign = -1;
            i++;
        }
        else if (i < len && str[i] == '+')
        {
            i++;
        }

        for (; i < len; i++)
        {
            if (str[i] >= '0' && str[i] <= '9')
                exp = exp * 10 + (str[i] - '0');
            else
                break;
        }

        exp *= exp_sign;

        /* 按 10^3 分组处理，减少浮点乘法次数 */
        while (exp >= 3)
        {
            result *= 1000.0;
            exp -= 3;
        }
        while (exp <= -3)
        {
            result *= 0.001;
            exp += 3;
        }

        /* 剩余 0~2 位 */
        while (exp > 0)
        {
            result *= 10.0;
            exp--;
        }
        while (exp < 0)
        {
            result *= 0.1;
            exp++;
        }
    }

    return result * sign;
}

uint16_t typeconv_from_f64(double val, char* buf, uint16_t buf_size, uint8_t precision)
{
    if (buf == NULL || buf_size == 0)
        return 0;

    if (precision > 15)
        precision = 15;

    uint16_t pos = 0;

    /* 特殊值 */
    if (val != val)
    {
        buf[0] = 'n'; buf[1] = 'a'; buf[2] = 'n'; buf[3] = '\0';
        return 3;
    }
    if (val > 1e30)
    {
        buf[0] = 'i'; buf[1] = 'n'; buf[2] = 'f'; buf[3] = '\0';
        return 3;
    }
    if (val < -1e30)
    {
        buf[0] = '-'; buf[1] = 'i'; buf[2] = 'n'; buf[3] = 'f'; buf[4] = '\0';
        return 4;
    }

    /* 符号 */
    if (val < 0.0)
    {
        buf[pos++] = '-';
        val = -val;
    }

    /* 整数/小数分离，直接运算，无 pow10 数组 */
    uint64_t ipart = (uint64_t)val;
    double frac = val - (double)ipart;

    /* 四舍五入：scale = 10^precision, + 0.5 四舍五入 */
    double scale = 1.0;
    for (uint8_t i = 0; i < precision; i++)
        scale *= 10.0;
    uint64_t fpart = (uint64_t)(frac * scale + 0.5);

    /* 溢出处理：fpart == 10^precision 意味着小数进位到整数 */
    uint64_t max_frac = (uint64_t)scale - 1;
    if (fpart > max_frac)
    {
        ipart++;
        fpart = 0;
    }

    /* 整数部分：逐位提取到 tmp[]，逆序写入 buf */
    char tmp[20];
    uint8_t tpos = 0;

    if (ipart == 0)
    {
        tmp[tpos++] = '0';
    }
    else
    {
        while (ipart > 0)
        {
            tmp[tpos++] = '0' + (ipart % 10);
            ipart /= 10;
        }
    }

    for (int8_t j = tpos - 1; j >= 0 && pos < buf_size - 1; j--)
        buf[pos++] = tmp[j];

    /* 小数部分：填充 precision 位，去除末尾零，紧凑输出 */
    if (precision > 0)
    {
        /* 提取 precision 位小数到 frac_digits[]，正序填充 */
        char frac_digits[16];
        uint64_t ftmp = fpart;
        for (int8_t i = precision - 1; i >= 0; i--)
        {
            frac_digits[i] = '0' + (ftmp % 10);
            ftmp /= 10;
        }

        /* 去除末尾零 */
        int8_t last = precision - 1;
        while (last >= 0 && frac_digits[last] == '0')
            last--;

        if (last >= 0)
        {
            buf[pos++] = '.';
            for (uint8_t i = 0; i <= (uint8_t)last && pos < buf_size - 1; i++)
                buf[pos++] = frac_digits[i];
        }
    }

    buf[pos] = '\0';
    return pos;
}
