/****************************
 * TYPECONV - 纯值转换模块 (string <-> typed values)
 * CarrotRPC
 *
 * 设计目标：
 * 1. 纯函数，无状态，无外部依赖
 * 2. 双向转换：字符串参数 -> 类型化值 / 类型化值 -> 字符串
 * 3. 零拷贝友好：直接操作字符串指针
 *****************************/
#pragma once
#ifndef _TYPECONV_H_
#define _TYPECONV_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <inttypes.h>

/**
 * @brief 字符串 (decimal) -> int64_t
 *
 * 支持 '-' 前缀，十进制解析
 *
 * @param str  字符串指针
 * @param len  字符串长度
 * @return int64_t 解析结果
 */
int64_t typeconv_to_i64(const char* str, uint16_t len);

/**
 * @brief 字符串 (hex) -> uint64_t
 *
 * 支持 0x/0X 前缀，十六进制解析
 *
 * @param str  字符串指针
 * @param len  字符串长度
 * @return uint64_t 解析结果
 */
uint64_t typeconv_to_u64(const char* str, uint16_t len);

/**
 * @brief int64_t -> 字符串 (decimal)
 *
 * @param val       要转换的值
 * @param buf       输出缓冲区
 * @param buf_size  输出缓冲区大小
 * @return uint16_t 写入的字节数 (不含 '\0')
 */
uint16_t typeconv_from_i64(int64_t val, char* buf, uint16_t buf_size);

/**
 * @brief uint64_t -> 字符串 (hex "0x" 前缀)
 *
 * @param val       要转换的值
 * @param buf       输出缓冲区
 * @param buf_size  输出缓冲区大小
 * @return uint16_t 写入的字节数 (不含 '\0')
 */
uint16_t typeconv_from_u64(uint64_t val, char* buf, uint16_t buf_size);

/**
 * @brief 字符串 (decimal/科学计数法) -> double
 *
 * 支持格式: "3.14", "-1.5e10", "1.23E-4", "100"
 *
 * @param str  字符串指针
 * @param len  字符串长度
 * @return double 解析结果
 */
double typeconv_to_f64(const char* str, uint16_t len);

/**
 * @brief double -> 字符串 (可调精度，去除末尾零)
 *
 * 整数/小数分离转换，精度可调 (1-15位)，自动去除末尾零
 * 嵌入式友好：无 stdio，纯整数+浮点运算
 *
 * @param val       要转换的值
 * @param buf       输出缓冲区
 * @param buf_size  输出缓冲区大小
 * @param precision 小数位数 (1-15，建议匹配应用场景)
 * @return uint16_t 写入的字节数 (不含 '\0')
 */
uint16_t typeconv_from_f64(double val, char* buf, uint16_t buf_size, uint8_t precision);

#ifdef __cplusplus
}
#endif

#endif /* _TYPECONV_H_ */
