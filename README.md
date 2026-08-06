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
#define RINGBUF_DMA              // Enable DMA hardware sync & DMA TX status machine

/* rpclog */
#define RPC_LOG_ENABLE_DEBUG  1  // Enable DEBUG log
#define RPC_LOG_ENABLE_INFO   1  // Enable INFO log
#define RPC_LOG_ENABLE_WARN   1  // Enable WARN log
#define RPC_LOG_ENABLE_ERROR  1  // Enable ERROR log
#define RPC_LOG_OUTPUT_BUF       // Enable buffer output mode

/* cmdqueue */
#define RPC_USE_CMD_QUEUE   1    // Enable cmdqueue (1=enabled, 0=disabled with 0 RAM overhead)
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

/* 4. Scan, parse and invoke (Standard Pipeline) */
uint8_t rx_buf[] = "add(10, 20)\n";
cmd_scanner_t scanner;
cmd_init(&scanner, rx_buf, sizeof(rx_buf) - 1);

cmd_entry_t entry;
if (cmd_scan(&scanner, &entry) == CMD_COMPLETE) {
    cmd_args_t args;
    cmd_parse(&entry, &args);

    invoke_ret_t ret;
    dispatch_status_t s = invoke_call(&dispatcher, &args, &ret);
    // s == DISPATCH_OK, ret.i64 == 30
}
```

### Signature & Type Registration

Functions are registered dynamically using human-readable signatures: `"name(args...) -> ret_type"`.

| Type Code | C Type Equivalent | Signature Example | Description |
|-----------|-------------------|-------------------|-------------|
| `v` | `void` | `"hello()"` | No arguments, void return |
| `i` / `i64` | `int64_t` | `"add(i, i) -> i"` | 64-bit signed integer |
| `u` / `u64` | `uint64_t` | `"hex(u) -> u"` | 64-bit unsigned integer |
| `s` | `char*` | `"echo(s) -> s"` | String pointer |
| `f` / `f64` | `double` | `"pi() -> f"` | 64-bit floating point |

* Omitting `->` implies void return: `"proc(s, i)"` is equivalent to `"proc(s, i) -> v"`.

### Core API Reference

#### `cmdscan` (Zero-Copy Command Parser)
* `cmd_init(scanner, buf, size)` / `cmd_init_ringbuf(scanner, ring)`: Initialize linear/ringbuf scanner context.
* `cmd_scan(scanner, &entry)`: Scan for command boundary (supports linear/ringbuf, auto-consumes framed data from ringbuf).
* `cmd_parse(&entry, &args)`: Parse command entry into argument pointers array (zero-copy + wrap-around safe).

#### `cmdqueue` (Command Queue)
* `cmd_queue_init(queue, buf, buf_size)`: Initialize queue with external memory buffer.
* `cmd_queue_push(queue, &entry)` / `cmd_queue_pop(queue, &entry)`: Enqueue and dequeue command entries.
* `cmd_queue_check(queue, func_name)`: Cooperative interrupt scan for high-priority command.

#### `ringbuf` (Ring Buffer & DMA TX State Machine)
* `ringbuf_init(ring, buf, size)`: Initialize ring buffer.
* `ringbuf_set_head_reader(ring, func)` / `ringbuf_set_tail_reader(ring, func)`: Bind hardware DMA RX/TX position getters.
* `ringbuf_dma_tx_fetch(ring, &ptr, &len)`: Poll & fetch contiguous DMA TX sending slice.
* `ringbuf_dma_tx_complete(ring, &next_ptr, &next_len)`: TC interrupt commit & probe for chained wrap-around TX.

#### `dispatch` & `invoke` (Invocation Engine)
* `dispatch_init(reg)`: Reset registry.
* `dispatch_reg(reg, handler, "sig")`: Register function with type signature.
* `dispatch_find(reg, name, len)`: Length-bounded function lookup.
* `invoke_call(reg, &args, &ret)`: Zero-copy execution engine (consolidated 64-bit staging).

#### `typeconv` (Bidirectional Value Conversion)
* `typeconv_to_i64(str, len)` / `typeconv_to_u64(str, len)` / `typeconv_to_f64(str, len)`: String to numeric value conversion.
* `typeconv_from_i64(val, buf, size)` / `typeconv_from_u64(val, buf, size)` / `typeconv_from_f64(val, buf, size, prec)`: Numeric value to string conversion.

#### `rpclog` (Unified Logging)
* `rpc_info(...)` / `rpc_error(...)` / `rpc_debug(...)` / `rpc_warning(...)`: User-level log macros.
* `rpc_return(...)` / `rpc_data(...)` / `rpc_reg(...)`: Protocol-level log macros.

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

### License

Apache License 2.0
