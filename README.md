## CarrotRPC - Dynamic Function Invocation Framework

🌐 Language: [English](README.md) | [中文](README.zh-CN.md)

---

![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)

CarrotRPC is a lightweight C dynamic function invocation framework for embedded RPC scenarios. It parses ASCII commands from DMA buffers and invokes pre-registered functions with zero-copy parameter passing.

### Features

- **Zero-Copy Parsing**: Command scanning and argument splitting return pointers into the original buffer — no memory allocation
- **DMA-Friendly Pipeline**: `cmd_scan` extracts command boundaries, `cmd_queue` buffers for deferred execution, `cmd_parse` splits args at call time
- **Type-Safe Dispatch**: Runtime string signature registration with automatic type conversion (int64, uint64, string, float64)
- **Return Value Capture**: Supports void, int64, and char* return types
- **Embedded-Optimized**: No stdlib dependencies in core modules, tail-priority byte comparison for fast name matching
- **Unified Logging**: `rpc_log` replaces printf with level-based logging (DEBUG/INFO/WARN/ERROR + protocol levels), zero stdio dependency

### Configuration

Edit `inc/rpc_cfg.h` to enable/disable features:

```c
/* ringbuf */
#define RINGBUF_DMA              // Enable DMA hardware sync

/* rpclog */
#define RPC_LOG_ENABLE_DEBUG  1  // Enable DEBUG log
#define RPC_LOG_ENABLE_INFO   1  // Enable INFO log
#define RPC_LOG_ENABLE_WARN   1  // Enable WARN log
#define RPC_LOG_ENABLE_ERROR  1  // Enable ERROR log
#define RPC_LOG_OUTPUT_BUF       // Enable buffer output mode

/* cmdqueue */
#define CMD_QUEUE_SIZE      128  // Queue capacity
#define CMD_QUEUE_BUF_SIZE  2048 // Queue buffer size

/* dispatch */
#define DISPATCH_MAX_FUNC_CNT  64  // Max registered functions
#define DISPATCH_ARGS_MAX_CNT  9   // Max args per function

/* invoke */
#define INVOKE_STR_MAX_SIZE    64  // Max string return length
```

Or via CMake: `-DRPC_LOG_ENABLE_DEBUG=0`

### Directory Structure

```
CarrotRPC/
├── inc/                 # Public headers
│   ├── rpc.h            # Unified entry (recommended)
│   ├── rpc_cfg.h        # Global configuration (compile switches)
│   ├── cmdscan.h        # Zero-copy command scanner + arg splitter
│   ├── cmdqueue.h       # Command queue (ring buffer)
│   ├── dispatch.h       # Function registration + lookup
│   ├── invoke.h         # Dispatch execution engine
│   ├── typeconv.h       # String <-> typed value conversion
│   ├── rpclog.h        # Unified logging (replaces printf)
│   └── ringbuf.h        # Generic ring buffer (optional DMA sync)
├── src/                 # Implementation
├── examples/            # Demo programs
├── tests/               # Unit tests (Unity + FFF)
├── vendor/              # Third-party libraries
├── docs/                # Documentation
│   └── rpc_usage.md     # Usage guide (STM32 HAL style)
└── CMakeLists.txt       # CMake build config
```

### Quick Start

```c
#include "rpc.h"

/* 1. Define handler functions */
void LED_On(void* channel) {
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
}

int64_t add(void* a, void* b) {
    return *(int64_t*)a + *(int64_t*)b;
}

/* 2. Register with type signatures */
static dispatch_registry_t dispatcher;

dispatch_init(&dispatcher);
dispatch_reg(&dispatcher, LED_On, "LED_On(i)");
dispatch_reg(&dispatcher, add,    "add(i, i) -> i");

/* 3. Use rpc_log instead of printf */
rpc_info("system ready");
rpc_error("arg mismatch: expected %d, got %d", 3, 2);

/* 4. Parse and invoke */
cmd_args_t args;
cmd_parse("add(10, 20)", 12, &args);

invoke_ret_t ret;
dispatch_status_t s = invoke_call(&dispatcher, &args, &ret);
// ret.i64 == 30
```

### Pipeline

```
DMA buffer → cmd_scan → cmd_queue → cmd_parse → dispatch + invoke → function call
```

### Building

Use `build.bat` (auto-detects compiler, uses Ninja):

```bat
build.bat          # Build
build.bat run      # Build and run tests
build.bat demo     # Build and run demo
```

Or manually with CMake:

```bash
cmake -B build
cmake --build build
./build/carrot_tests
```

### Testing

222 tests across 8 suites: e2e, cmdscan, cmdqueue, ringbuf, typeconv, dispatch, invoke, rpc_log.

### License

Apache License 2.0
