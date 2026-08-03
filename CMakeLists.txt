cmake_minimum_required(VERSION 3.14)
project(CarrotRPC C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---- Static library ----
add_library(carrot
    src/cmdscan.c
    src/cmdqueue.c
    src/ringbuf.c
    src/typeconv.c
    src/dispatch.c
    src/invoke.c
    src/rpclog.c
)
target_include_directories(carrot PUBLIC inc)

# ---- CMD SCAN Demo executable ----
add_executable(cmdscan_demo
    examples/cmdscan_demo.c
)
target_link_libraries(cmdscan_demo PRIVATE carrot)
target_include_directories(cmdscan_demo PRIVATE examples)

# ---- CMD QUEUE Demo executable ----
add_executable(cmdqueue_demo
    examples/cmdqueue_demo.c
)
target_link_libraries(cmdqueue_demo PRIVATE carrot)
target_include_directories(cmdqueue_demo PRIVATE examples)

# ---- RPC LOG Demo executable ----
add_executable(rpclog_demo
    examples/rpclog_demo.c
)
target_link_libraries(rpclog_demo PRIVATE carrot)
target_include_directories(rpclog_demo PRIVATE examples)

# ---- DISPATCH Demo executable ----
add_executable(dispatch_demo
    examples/dispatch_demo.c
)
target_link_libraries(dispatch_demo PRIVATE carrot)
target_include_directories(dispatch_demo PRIVATE examples)

# ---- INVOKE Demo executable ----
add_executable(invoke_demo
    examples/invoke_demo.c
)
target_link_libraries(invoke_demo PRIVATE carrot)
target_include_directories(invoke_demo PRIVATE examples)

# ---- RINGBUF Demo executable ----
add_executable(ringbuf_demo
    examples/ringbuf_demo.c
)
target_link_libraries(ringbuf_demo PRIVATE carrot)
target_include_directories(ringbuf_demo PRIVATE examples)

# ---- Test executable ----
add_executable(carrot_tests
    vendor/unity/unity.c
    tests/test_helpers.c
    tests/test_cmdscan.c
    tests/test_cmdqueue.c
    tests/test_ringbuf.c
    tests/test_typeconv.c
    tests/test_dispatch.c
    tests/test_invoke_helpers.c
    tests/test_invoke.c
    tests/test_e2e.c
    tests/test_rpclog.c
    tests/main_test.c
)
target_link_libraries(carrot_tests PRIVATE carrot)
target_include_directories(carrot_tests PRIVATE
    tests
    vendor/unity
    vendor/fff
)
target_compile_definitions(carrot_tests PRIVATE UNITY_INCLUDE_DOUBLE)
