/**
 * test_typeconv.c — Unit tests for typeconv module
 */
#include "unity.h"
#include "typeconv.h"
#include <string.h>
#include <math.h>

/* ===== to_i64 ===== */

void test_typeconv_i64_positive(void)
{
    int64_t val = typeconv_to_i64("42", 2);
    TEST_ASSERT_EQUAL_INT64(42, val);
}

void test_typeconv_i64_negative(void)
{
    int64_t val = typeconv_to_i64("-100", 4);
    TEST_ASSERT_EQUAL_INT64(-100, val);
}

void test_typeconv_i64_zero(void)
{
    int64_t val = typeconv_to_i64("0", 1);
    TEST_ASSERT_EQUAL_INT64(0, val);
}

void test_typeconv_i64_large(void)
{
    int64_t val = typeconv_to_i64("9999999999", 10);
    TEST_ASSERT_EQUAL_INT64(9999999999LL, val);
}

void test_typeconv_i64_non_digit_ignored(void)
{
    int64_t val = typeconv_to_i64("12abc34", 7);
    TEST_ASSERT_EQUAL_INT64(1234, val);
}

void test_typeconv_i64_single_char(void)
{
    int64_t val = typeconv_to_i64("7", 1);
    TEST_ASSERT_EQUAL_INT64(7, val);
}

/* ===== to_u64 ===== */

void test_typeconv_u64_basic(void)
{
    uint64_t val = typeconv_to_u64("FF", 2);
    TEST_ASSERT_EQUAL_HEX64(0xFF, val);
}

void test_typeconv_u64_with_prefix(void)
{
    uint64_t val = typeconv_to_u64("0xDEAD", 6);
    TEST_ASSERT_EQUAL_HEX64(0xDEAD, val);
}

void test_typeconv_u64_zero(void)
{
    uint64_t val = typeconv_to_u64("0", 1);
    TEST_ASSERT_EQUAL_HEX64(0, val);
}

void test_typeconv_u64_uppercase_prefix(void)
{
    uint64_t val = typeconv_to_u64("0xAB", 4);
    TEST_ASSERT_EQUAL_HEX64(0xAB, val);
}

void test_typeconv_u64_long_hex(void)
{
    uint64_t val = typeconv_to_u64("FFFFFFFFFFFFFFFF", 16);
    TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFFFULL, val);
}

void test_typeconv_u64_max_decimal(void)
{
    uint64_t val = typeconv_to_u64("18446744073709551615", 20);
    TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFFFULL, val);
}

/* ===== from_i64 ===== */

void test_typeconv_from_i64_positive(void)
{
    char buf[32];
    uint16_t len = typeconv_from_i64(42, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT16(2, len);
    TEST_ASSERT_EQUAL_STRING("42", buf);
}

void test_typeconv_from_i64_negative(void)
{
    char buf[32];
    uint16_t len = typeconv_from_i64(-100, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT16(4, len);
    TEST_ASSERT_EQUAL_STRING("-100", buf);
}

void test_typeconv_from_i64_zero(void)
{
    char buf[32];
    uint16_t len = typeconv_from_i64(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT16(1, len);
    TEST_ASSERT_EQUAL_STRING("0", buf);
}

/* ===== from_u64 ===== */

void test_typeconv_from_u64_basic(void)
{
    char buf[32];
    uint16_t len = typeconv_from_u64(0xFF, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT16(4, len);
    TEST_ASSERT_EQUAL_STRING("0xFF", buf);
}

void test_typeconv_from_u64_zero(void)
{
    char buf[32];
    uint16_t len = typeconv_from_u64(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT16(3, len);
    TEST_ASSERT_EQUAL_STRING("0x0", buf);
}

/* ===== to_f64 ===== */

void test_typeconv_f64_basic(void)
{
    double val = typeconv_to_f64("3.14", 4);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 3.14, val);
}

void test_typeconv_f64_negative(void)
{
    double val = typeconv_to_f64("-1.5", 4);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, -1.5, val);
}

void test_typeconv_f64_integer(void)
{
    double val = typeconv_to_f64("100", 3);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 100.0, val);
}

void test_typeconv_f64_exponent(void)
{
    double val = typeconv_to_f64("1.5e2", 5);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, 150.0, val);
}

void test_typeconv_f64_negative_exponent(void)
{
    double val = typeconv_to_f64("1.23E-2", 7);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0123, val);
}

void test_typeconv_f64_zero(void)
{
    double val = typeconv_to_f64("0.0", 3);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, val);
}

/* ===== from_f64 ===== */

void test_typeconv_from_f64_basic(void)
{
    char buf[32];
    uint16_t len = typeconv_from_f64(3.14, buf, sizeof(buf), 6);
    TEST_ASSERT_EQUAL_UINT16(4, len);
    TEST_ASSERT_EQUAL_STRING("3.14", buf);
}

void test_typeconv_from_f64_negative(void)
{
    char buf[32];
    uint16_t len = typeconv_from_f64(-1.5, buf, sizeof(buf), 6);
    TEST_ASSERT_EQUAL_UINT16(4, len);
    TEST_ASSERT_EQUAL_STRING("-1.5", buf);
}

void test_typeconv_from_f64_integer(void)
{
    char buf[32];
    uint16_t len = typeconv_from_f64(100.0, buf, sizeof(buf), 6);
    TEST_ASSERT_EQUAL_UINT16(3, len);
    TEST_ASSERT_EQUAL_STRING("100", buf);
}

void test_typeconv_from_f64_zero(void)
{
    char buf[32];
    uint16_t len = typeconv_from_f64(0.0, buf, sizeof(buf), 6);
    TEST_ASSERT_EQUAL_UINT16(1, len);
    TEST_ASSERT_EQUAL_STRING("0", buf);
}

void test_typeconv_from_f64_small(void)
{
    char buf[32];
    typeconv_from_f64(0.005, buf, sizeof(buf), 6);
    TEST_ASSERT_EQUAL_STRING("0.005", buf);
}

/* 不同精度输出 */
void test_typeconv_from_f64_precision_1(void)
{
    char buf[32];
    typeconv_from_f64(3.14159, buf, sizeof(buf), 1);
    TEST_ASSERT_EQUAL_STRING("3.1", buf);
}

void test_typeconv_from_f64_precision_3(void)
{
    char buf[32];
    typeconv_from_f64(3.14159, buf, sizeof(buf), 3);
    TEST_ASSERT_EQUAL_STRING("3.142", buf);
}

void test_typeconv_from_f64_precision_8(void)
{
    char buf[32];
    typeconv_from_f64(3.14159, buf, sizeof(buf), 8);
    TEST_ASSERT_EQUAL_STRING("3.14159", buf);
}

/* 整数输出 (precision=0) */
void test_typeconv_from_f64_precision_zero(void)
{
    char buf[32];
    uint16_t len = typeconv_from_f64(3.14159, buf, sizeof(buf), 0);
    TEST_ASSERT_EQUAL_UINT16(1, len);
    TEST_ASSERT_EQUAL_STRING("3", buf);
}

/* 浮点表示误差：3.1399999 → 3.14 */
void test_typeconv_from_f64_fp_imprecision(void)
{
    char buf[32];
    typeconv_from_f64(3.1399999, buf, sizeof(buf), 2);
    TEST_ASSERT_EQUAL_STRING("3.14", buf);
}

void test_typeconv_from_f64_fp_imprecision_2(void)
{
    char buf[32];
    typeconv_from_f64(2.9999999, buf, sizeof(buf), 1);
    TEST_ASSERT_EQUAL_STRING("3", buf);
}

/* ===== round-trip ===== */

void test_typeconv_i64_roundtrip(void)
{
    char buf[32];
    typeconv_from_i64(12345, buf, sizeof(buf));
    int64_t val = typeconv_to_i64(buf, strlen(buf));
    TEST_ASSERT_EQUAL_INT64(12345, val);
}

void test_typeconv_u64_roundtrip(void)
{
    char buf[32];
    typeconv_from_u64(0xABCDEF, buf, sizeof(buf));
    uint64_t val = typeconv_to_u64(buf, strlen(buf));
    TEST_ASSERT_EQUAL_HEX64(0xABCDEF, val);
}

void test_typeconv_f64_roundtrip(void)
{
    char buf[32];
    typeconv_from_f64(3.14, buf, sizeof(buf), 6);
    double val = typeconv_to_f64(buf, strlen(buf));
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 3.14, val);
}

void test_typeconv_f64_roundtrip_pi(void)
{
    char buf[32];
    typeconv_from_f64(3.14159, buf, sizeof(buf), 5);
    double val = typeconv_to_f64(buf, strlen(buf));
    TEST_ASSERT_DOUBLE_WITHIN(0.00001, 3.14159, val);
}

void test_typeconv_f64_roundtrip_negative(void)
{
    char buf[32];
    typeconv_from_f64(-0.0075, buf, sizeof(buf), 4);
    double val = typeconv_to_f64(buf, strlen(buf));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, -0.0075, val);
}

int run_typeconv_tests(void)
{
    UNITY_BEGIN();

    /* to_i64 */
    RUN_TEST(test_typeconv_i64_positive);
    RUN_TEST(test_typeconv_i64_negative);
    RUN_TEST(test_typeconv_i64_zero);
    RUN_TEST(test_typeconv_i64_large);
    RUN_TEST(test_typeconv_i64_non_digit_ignored);
    RUN_TEST(test_typeconv_i64_single_char);

    /* to_u64 */
    RUN_TEST(test_typeconv_u64_basic);
    RUN_TEST(test_typeconv_u64_with_prefix);
    RUN_TEST(test_typeconv_u64_zero);
    RUN_TEST(test_typeconv_u64_uppercase_prefix);
    RUN_TEST(test_typeconv_u64_long_hex);
    RUN_TEST(test_typeconv_u64_max_decimal);

    /* from_i64 */
    RUN_TEST(test_typeconv_from_i64_positive);
    RUN_TEST(test_typeconv_from_i64_negative);
    RUN_TEST(test_typeconv_from_i64_zero);

    /* from_u64 */
    RUN_TEST(test_typeconv_from_u64_basic);
    RUN_TEST(test_typeconv_from_u64_zero);

    /* to_f64 */
    RUN_TEST(test_typeconv_f64_basic);
    RUN_TEST(test_typeconv_f64_negative);
    RUN_TEST(test_typeconv_f64_integer);
    RUN_TEST(test_typeconv_f64_exponent);
    RUN_TEST(test_typeconv_f64_negative_exponent);
    RUN_TEST(test_typeconv_f64_zero);

    /* from_f64 */
    RUN_TEST(test_typeconv_from_f64_basic);
    RUN_TEST(test_typeconv_from_f64_negative);
    RUN_TEST(test_typeconv_from_f64_integer);
    RUN_TEST(test_typeconv_from_f64_zero);
    RUN_TEST(test_typeconv_from_f64_small);
    RUN_TEST(test_typeconv_from_f64_precision_1);
    RUN_TEST(test_typeconv_from_f64_precision_3);
    RUN_TEST(test_typeconv_from_f64_precision_8);
    RUN_TEST(test_typeconv_from_f64_precision_zero);
    RUN_TEST(test_typeconv_from_f64_fp_imprecision);
    RUN_TEST(test_typeconv_from_f64_fp_imprecision_2);

    /* round-trip */
    RUN_TEST(test_typeconv_i64_roundtrip);
    RUN_TEST(test_typeconv_u64_roundtrip);
    RUN_TEST(test_typeconv_f64_roundtrip);
    RUN_TEST(test_typeconv_f64_roundtrip_pi);
    RUN_TEST(test_typeconv_f64_roundtrip_negative);

    return UNITY_END();
}
