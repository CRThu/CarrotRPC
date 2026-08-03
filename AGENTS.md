# CarrotRPC - 项目架构指南

## 项目概述

CarrotRPC 是一个轻量级的 C 动态函数调用框架，用于通过字符串命令或参数池调用预注册的函数。常用于嵌入式 RPC 场景。

## 项目规则

### 代码编辑规则
- **禁止使用 `write` 工具修改已有文件**：必须使用 `edit` 工具进行精确替换，防止注释和格式丢失
- `write` 仅用于创建新文件

### 重构规则
- **每次代码重构后必须更新本文档**（AGENTS.md）和相关文档
- 更新内容包括：目录结构、模块说明、API 列表、使用流程示例

## 目录结构

```
├── inc/                 # 公开头文件
│   ├── rpc.h            # 统一入口头文件 (推荐使用)
│   ├── rpc_cfg.h        # 全局配置 (编译开关集中管理)
│   ├── dispatch.h      # 函数注册与查找 (v2, 自包含)
│   ├── invoke.h        # 调度执行引擎 (v2)
│   ├── typeconv.h      # 纯值转换模块 (string <-> typed values)
│   ├── rpclog.h       # 分级日志库 (取代 printf)
│   ├── cmdscan.h       # 零拷贝命令扫描 + 参数切分
│   ├── cmdqueue.h      # 命令队列 (环形缓冲区)
│   └── ringbuf.h       # 通用环形缓冲区 (支持 DMA 硬件同步)
├── src/                 # 实现
│   ├── dispatch.c
│   ├── invoke.c
│   ├── typeconv.c
│   ├── rpclog.c
│   ├── cmdscan.c
│   ├── cmdqueue.c
│   └── ringbuf.c
├── examples/            # 示例代码
├── tests/               # 单元测试 (使用 Unity + FFF)
├── vendor/              # 第三方库 (unity, fff)
├── docs/                # 文档
│   └── rpc_usage.md     # 使用指南 (STM32 HAL 风格)
├── build/               # CMake 构建输出
└── CMakeLists.txt       # CMake 配置
```

## 核心模块

### 0. rpc_cfg (全局配置)
- **职责**: 集中管理所有模块的编译开关
- **使用方式**:
  - 修改 `inc/rpc_cfg.h` 中的宏定义
  - 或通过 CMake 参数覆盖：`-DRPC_xxx=1`
- **配置项**:
  ```c
  /* ringbuf */
  #define RINGBUF_DMA              // 启用 DMA 硬件同步

  /* rpclog */
  #define RPC_LOG_ENABLE_DEBUG  1  // 启用 DEBUG 日志
  #define RPC_LOG_ENABLE_INFO   1  // 启用 INFO 日志
  #define RPC_LOG_ENABLE_WARN   1  // 启用 WARN 日志
  #define RPC_LOG_ENABLE_ERROR  1  // 启用 ERROR 日志
  #define RPC_LOG_OUTPUT_BUF       // 启用缓冲区输出模式

  /* cmdqueue */
  #define CMD_QUEUE_SIZE      128  // 队列容量
  #define CMD_QUEUE_BUF_SIZE  2048 // 队列缓冲区大小

  /* dispatch */
  #define DISPATCH_MAX_FUNC_CNT  64  // 最大注册函数数
  #define DISPATCH_ARGS_MAX_CNT  9   // 单函数最大参数数

  /* invoke */
  #define INVOKE_STR_MAX_SIZE    64  // 字符串返回值最大长度
  ```
- **优先级**: CMake 参数 > rpc_cfg.h > 模块默认值

### 1. cmdscan (零拷贝命令解析器)
- **职责**: 零拷贝命令扫描 + 参数切分，适配 DMA 场景
- **设计目标**:
  - 无内存复制 - 直接返回原始缓冲区指针
  - 扫描提取 + 零拷贝参数切分
  - 无 std 库依赖 - 纯嵌入式友好
- **关键结构**:
  - `cmd_scanner_t`: 扫描器上下文 (buf, buf_size, scan_pos)
  - `cmd_entry_t`: 命令条目（扫描提取结果，含 buf 指针、cmd_start、cmd_len、func_len）
  - `cmd_args_t`: 参数切分结果（函数名指针 + 参数指针数组）
  - `cmd_arg_t`: 参数指针 (ptr + len)
  - `cmd_status_t`: 扫描状态 (CMD_INCOMPLETE, CMD_COMPLETE, CMD_ERROR)
- **核心 API**:
  - `cmd_init(scanner, buf, size)` - 初始化扫描器
  - `cmd_reset(scanner)` - 重置扫描器
  - `cmd_scan(scanner, &entry)` - 扫描提取单条命令边界
  - `cmd_parse(cmd, len, &args)` - 将完整命令解析为参数指针数组（零拷贝）
  - `cmd_compare(a, a_len, b, b_len)` - 快速字节比较（尾部优先）
- **使用流程**:
  ```c
  cmd_scanner_t scanner;
  cmd_init(&scanner, dma_buf, dma_len);
  
  cmd_entry_t entry;
  while (cmd_scan(&scanner, &entry) == CMD_COMPLETE) {
      cmd_queue_push(&queue, &entry);
  }
  
  // 出队时再解析
  while (!cmd_queue_is_empty(&queue)) {
      cmd_queue_pop(&queue, &entry);
      cmd_args_t args;
      cmd_parse((const char*)entry.buf + entry.cmd_start, entry.cmd_len, &args);
      // args.func_name, args.func_name_len, args.args[], args.args_count
  }
  ```

### 2. cmdqueue (命令队列)
- **职责**: 命令排队，支持协作式中断检查
- **设计目标**:
  - 内部 buf 存储原始命令（独立于 DMA 缓冲区）
  - 支持环形/非环形 DMA 缓冲区
- **关键结构**:
  - `cmd_queue_t`: 命令队列（内部 items[] 使用 `cmd_entry_t`）
- **核心 API**:
  - `cmd_queue_init(queue)` - 初始化队列
  - `cmd_queue_push(queue, &entry)` - 入队（从 entry 复制）
  - `cmd_queue_pop(queue, &entry)` - 出队（返回 cmd_entry_t）
  - `cmd_queue_check(queue, func_name)` - 扫描指定函数名
  - `cmd_queue_flush(queue)` - 清空队列

### 3. ringbuf (通用环形缓冲区)
- **职责**: 通用环形缓冲区，支持 DMA 接收/发送、命令队列等场景
- **设计目标**:
  - 支持读写操作，wrap-aware
  - 可选硬件同步：DMA 接收(读 head) / DMA 发送(读 tail)
  - 适用于嵌入式 MCU (STM32 / ESP32 等)
- **编译宏**:
  - `RINGBUF_DMA` - 在 `ringbuf.h` 中取消注释启用，开启后支持 DMA 硬件同步（head_reader / tail_reader），关闭后退化为纯软件环形缓冲区
- **关键结构**:
  - `ringbuf_t`: 环形缓冲区管理结构
- **核心 API**:
  - `ringbuf_init(ring, buf, size)` - 初始化
  - `ringbuf_set_head_reader(ring, func)` - 设置 DMA RX 硬件位置读取函数 (需 `RINGBUF_DMA`)
  - `ringbuf_set_tail_reader(ring, func)` - 设置 DMA TX 硬件位置读取函数 (需 `RINGBUF_DMA`)
  - `ringbuf_sync_head(ring)` / `ringbuf_sync_tail(ring)` - 从硬件同步位置 (需 `RINGBUF_DMA`)
  - `ringbuf_set_head(ring, head)` / `ringbuf_set_tail(ring, tail)` - 手动设置
  - `ringbuf_readable(ring)` / `ringbuf_writable(ring)` - 查询空间
  - `ringbuf_write(ring, src, len)` - 写入（处理 wrap-around）
  - `ringbuf_peek(ring, dst, len)` - 读取（不消费）
  - `ringbuf_skip(ring, len)` - 跳过已处理数据
  - `ringbuf_flush(ring)` - 清空缓冲区

### 4. dispatch (函数注册与查找)
- **职责**: 完全自包含的函数注册、查找模块，运行时字符串签名注册
- **设计目标**:
  - 无依赖：自包含模块
  - 自定义类型系统：DV/DI/DU/DS/DF
  - 字符串签名注册：`dispatch_reg(&dispatcher, handler, "name(args...) -> ret")`
  - 支持长度限定查找 (适配 cmd_parse 非 null-terminate 输出)
- **类型字符**:
  - `v` = void (DV), `i`/`i64` = int64 (DI), `u`/`u64` = uint64 (DU)
  - `s` = string (DS), `f`/`f64` = float64 (DF)
- **签名格式**:
  - `"hello()"` — 无参数, void 返回
  - `"add(i, i) -> i"` — 两个 int64 参数, 返回 int64
  - `"echo(s) -> s"` — 一个 string 参数, 返回 string
  - `"proc(s, i, u)"` — 无 `->` 则 void 返回
  - `"i, i -> i"` — 不带括号也支持
- **关键结构**:
  - `dispatch_func_t`: 函数信息 (name, handler, ret_type, args_type[], args_count)
  - `dispatch_type_t`: 类型枚举 (DV, DI, DU, DS, DF)
  - `dispatch_status_t`: 状态码 (DISPATCH_OK, DISPATCH_ERR_NULL, etc.)
- **核心 API**:
  - `dispatch_reg(reg, handler, sig)` — 注册函数 (宏, 自动提取函数名)
  - `dispatch_find(reg, name, len)` — 查找函数 (长度限定)
  - `dispatch_init(reg)` — 重置注册表
  - `_dispatch_add(reg, name, handler, sig)` — 内部注册函数
- **用法示例**:
  ```c
  #include "dispatch.h"
  
  static dispatch_registry_t dispatcher;
  
  void hello(void) { printf("hello\n"); }
  int64_t add(void* a, void* b) { return *(int64_t*)a + *(int64_t*)b; }
  
  dispatch_init(&dispatcher);
  dispatch_reg(&dispatcher, hello, "hello()");           // name="hello"
  dispatch_reg(&dispatcher, add,   "add(i, i) -> i");   // name="add"
  
  dispatch_func_t* f = dispatch_find(&dispatcher, "add", 3);
  // f->handler, f->ret_type, f->args_type, f->args_count
  ```

### 5. invoke (调度执行引擎)
- **职责**: 通过零拷贝解析结果调用函数，依赖 dispatch 和 typeconv
- **设计目标**:
  - 栈上 staging buffer: val_i64[9], val_u64[9], str_buf[9][64], void* p[9]
  - p[i] 直接指向数据 (一次解引用读取值)
  - 支持三种返回值族: void / int64 / char*
- **关键结构**:
  - `invoke_ret_t`: 返回值 (type + union of i64/str)
  - `invoke_ret_type_t`: 返回值类型 (INVOKERET_NONE/I64/STR)
- **核心 API**:
  - `invoke_call(reg, result, ret)` — 通过零拷贝解析结果调用函数
- **用法示例**:
  ```c
  #include "invoke.h"
  
  cmd_args_t args;
  cmd_parse(cmd, len, &args);
  
  invoke_ret_t ret;
  dispatch_status_t s = invoke_call(&dispatcher, &args, &ret);
  if (s == DISPATCH_OK && ret.type == INVOKERET_I64) {
      printf("result: %ld\n", ret.i64);
  }
  ```

### 6. typeconv (纯值转换模块)
- **职责**: 字符串与类型化值之间的双向转换
- **设计目标**:
  - 纯函数，无状态，无外部依赖
  - 嵌入式友好：无 stdio，纯整数+浮点运算
- **核心 API**:
  - `typeconv_to_i64(str, len)` — 字符串 (decimal) -> int64_t
  - `typeconv_to_u64(str, len)` — 字符串 (hex) -> uint64_t
  - `typeconv_to_f64(str, len)` — 字符串 (decimal/科学计数法) -> double
  - `typeconv_from_i64(val, buf, size)` — int64_t -> 字符串
  - `typeconv_from_u64(val, buf, size)` — uint64_t -> 字符串 (hex)
  - `typeconv_from_f64(val, buf, size, precision)` — double -> 字符串

### 7. rpc_log (分级日志库)
- **职责**: 取代 printf，提供分级日志输出，对齐 CARROT_RPC 协议格式
- **设计目标**:
  - 零 stdio 依赖 — 纯嵌入式友好
  - 两级级别：用户级别（可开关）+ 协议级别（始终输出）
  - 自实现格式化 (varargs)，支持 `%d %u %x %X %s %c %%`
  - 类型安全输出：`rpc_log_i64` / `rpc_log_u64` / `rpc_log_hex` / `rpc_log_f64`
  - 编译期二选一：字节模式 / 缓冲区模式 (`RPC_LOG_OUTPUT_BUF`)
- **级别系统**:
  - 用户级别：DEBUG(0) / INFO(1) / WARN(2) / ERROR(3) — 可通过 `rpc_log_set_level` 过滤
  - 协议级别：RETURN(4) / DATA(5) / REG(6) — 始终输出，不受过滤影响
- **核心 API**:
  - `rpc_log(level, fmt, ...)` — 格式化日志，支持 `%d %u %x %X %s %c %%`，无参数时直接传字符串
  - `rpc_log_i64(level, tag, val)` — int64 输出 `[LEVEL]: tag=val\r\n`
  - `rpc_log_u64(level, tag, val)` — uint64 输出
  - `rpc_log_hex(level, tag, val)` — 十六进制输出 `[LEVEL]: tag=0xHEX\r\n`
  - `rpc_log_f64(level, tag, val, prec)` — 浮点数输出
  - `rpc_log_set_output(fn)` — 设置输出回调
  - `rpc_log_set_level(level)` — 设置全局最低用户级别
- **便捷宏**:
  - `rpc_debug(...)` / `rpc_info(...)` / `rpc_warning(...)` / `rpc_error(...)`
  - `rpc_info_i64(tag, val)` / `rpc_info_hex(tag, val)` 等类型宏
  - `rpc_return(...)` / `rpc_data(...)` / `rpc_reg(...)` — 协议级别
- **编译宏**:
  - `RPC_LOG_OUTPUT_BUF` — 启用缓冲区模式
  - `RPC_LOG_ENABLE_DEBUG/INFO/WARN/ERROR` — 设为 0 编译期消除对应级别

## 调用流程

### 流程 1: 零拷贝方式 (推荐用于嵌入式)
```
DMA 缓冲区 → cmd_scan → 参数指针数组 → dispatch + invoke → 函数执行
```

1. DMA 接收数据到缓冲区
2. `cmd_scan()` 扫描找到完整命令
3. `cmd_parse()` 解析为参数指针数组（零拷贝）
4. `invoke_call()` 查找函数、转换参数、调用

### 流程 2: 预解析 + 队列方式 (推荐用于需要排队的场景)
```
DMA 缓冲区 → cmd_scan → cmd_queue → 出队 → cmd_parse → dispatch + invoke → 函数执行
```

1. DMA 接收数据到缓冲区
2. `cmd_scan()` 扫描命令边界
3. `cmd_queue_push()` 复制命令到队列内部缓冲区
4. `cmd_queue_pop()` 出队获取 cmd_entry_t
5. `cmd_parse()` 执行时解析参数
6. `invoke_call()` 查找函数、转换参数、调用

### 流程 3: 完整管线 (v2, 推荐)
```
dispatch_reg() 注册 → DMA → cmd_scan → cmd_queue → cmd_parse → invoke_call → 函数执行
```

1. 启动时注册函数: `dispatch_reg(&dispatcher, handler, "name(args...) -> ret")`
2. DMA 接收数据到缓冲区
3. `cmd_scan()` + `cmd_queue_push()` 扫描入队
4. `cmd_queue_pop()` 出队
5. `cmd_parse()` 解析为参数指针数组
6. `invoke_call(&dispatcher, &result, &ret)` 查找函数、转换参数、调用、捕获返回值

## 限制

- 最多 64 个注册函数 (dispatch)
- 单个函数最多 9 个参数
- 队列最多 128 条命令
- 队列内部缓冲区 2048 字节 (环形)
- DMA 环形缓冲区大小由用户定义

## 构建

**必须使用 `build.bat` 构建**，不要直接调用 cmake。build.bat 自动检测编译器 (GCC/MSVC) 并使用 Ninja。

```bat
build.bat          # 构建
build.bat run      # 构建并运行测试
build.bat demo     # 构建并运行 demo
```

## 测试

测试文件: `tests/test_*.c`，使用 Unity 框架，构建后运行 `carrot_tests`。

## 文档

- 详细使用指南请参考: [docs/rpc_usage.md](docs/rpc_usage.md)（含 API 参考、使用场景与 DMA 集成指南）
- 最新协议通信手册 (v2.0) 请参考: [CARROT_RPC_v2.md](CARROT_RPC_v2.md)（纯固定 Tag 前缀 + 正交二维波形矩阵）
- 传统协议通信手册 (v1.2.3) 请参考: [CARROT_RPC.md](CARROT_RPC.md)
