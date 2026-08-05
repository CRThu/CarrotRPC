/**
 * main_test.c — Unity test runner entry point
 *
 * Global setUp/tearDown + runs all test suites.
 */
#include "unity.h"
#include "test_helpers.h"

/* Global setUp: reset all mocks + register mock function group */
void setUp(void)
{
    test_helpers_reset();
}

void tearDown(void) {}

/* External test suite entry points */
extern int run_cmdscan_tests(void);
extern int run_cmdqueue_tests(void);
extern int run_ringbuf_tests(void);
extern int run_typeconv_tests(void);
extern int run_dispatch_tests(void);
extern int run_invoke_tests(void);
extern int run_e2e_tests(void);
extern int run_rpc_log_tests(void);

#include "rpc.h"

int main(void)
{
    int failures = 0;

    printf("\n========================================\n");
    printf("  CarrotRPC v%s Test Suite Started\n", CARROT_RPC_VERSION_STR);
    printf("========================================\n");

    printf("\n========== E2E Tests ==========\n");
    failures += run_e2e_tests();

    printf("\n========== CmdScan Tests ==========\n");
    failures += run_cmdscan_tests();

    printf("\n========== CmdQueue Tests ==========\n");
    failures += run_cmdqueue_tests();

    printf("\n========== RingBuf Tests ==========\n");
    failures += run_ringbuf_tests();

    printf("\n========== TypeConv Tests ==========\n");
    failures += run_typeconv_tests();

    printf("\n========== Dispatch Tests ==========\n");
    failures += run_dispatch_tests();

    printf("\n========== Invoke Tests ==========\n");
    failures += run_invoke_tests();

    printf("\n========== RpcLog Tests ==========\n");
    failures += run_rpc_log_tests();

    printf("\n========== SUMMARY ==========\n");
    if (failures == 0)
        printf("All test suites PASSED.\n");
    else
        printf("Some test suites FAILED.\n");

    return failures;
}
