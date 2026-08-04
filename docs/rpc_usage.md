# CarrotRPC 使用指南

## 1. 即时执行（单条命令）

> **自动 DMA 同步原则**：初始化时将 DMA 读写位置绑定至 `ringbuf`，调用 `ringbuf_readable()` 时内部将**自动同步 DMA 硬件当前写位置**，主循环无需任何手动同步操作与串口回调。

```c
#include "rpc.h"

#define DMA_BUF_SIZE  2048
uint8_t dma_buf[DMA_BUF_SIZE];
ringbuf_t dma_ring;
cmd_queue_t cmd_queue;
dispatch_registry_t dispatcher;

/* 从 CNDTR 寄存器读取 DMA 当前写指针位置 */
uint16_t get_dma_head(void) {
    return DMA_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx);
}

/* RPC 函数定义 */
void LED_On(void* channel) {
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
}

void LED_Off(void* channel) {
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
}

int main(void) {
    /* 模块初始化 */
    dispatch_init(&dispatcher);
    cmd_queue_init(&cmd_queue);
    ringbuf_init(&dma_ring, dma_buf, DMA_BUF_SIZE);

    /* 绑定 DMA 硬件读取函数：之后 ringbuf 内部会自动同步 DMA 位置 */
    ringbuf_set_head_reader(&dma_ring, get_dma_head);

    /* 注册函数 */
    dispatch_reg(&dispatcher, LED_On,  "LED_On(i)");
    dispatch_reg(&dispatcher, LED_Off, "LED_Off(i)");

    /* 开启 UART DMA Circular 循环接收 */
    HAL_UART_Receive_DMA(&huart1, dma_buf, DMA_BUF_SIZE);

    while (1) {
        /* 1. 查询可读字节 (内部自动向硬件同步 DMA head) */
        uint16_t readable = ringbuf_readable(&dma_ring);
        if (readable > 0) {
            cmd_scanner_t scanner;
            cmd_init(&scanner, dma_buf + dma_ring.tail, readable);

            cmd_entry_t entry;
            while (cmd_scan(&scanner, &entry) == CMD_COMPLETE) {
                cmd_queue_push(&cmd_queue, &entry);
            }
            ringbuf_skip(&dma_ring, scanner.scan_pos);
        }

        /* 2. 出队解析并调度执行 */
        cmd_entry_t entry;
        while (!cmd_queue_is_empty(&cmd_queue)) {
            cmd_queue_pop(&cmd_queue, &entry);
            cmd_args_t args;
            cmd_parse((const char*)entry.buf + entry.cmd_start, entry.cmd_len, &args);

            invoke_ret_t ret;
            invoke_call(&dispatcher, &args, &ret);
        }
    }
}
```

**命令格式：**
```
LED_On(1)
LED_Off(1)
```

---

## 2. 队列执行（多条命令）

```c
#include "rpc.h"

int main(void) {
    dispatch_init(&dispatcher);
    cmd_queue_init(&cmd_queue);
    ringbuf_init(&dma_ring, dma_buf, DMA_BUF_SIZE);
    ringbuf_set_head_reader(&dma_ring, get_dma_head);

    dispatch_reg(&dispatcher, Motor_SetSpeed, "Motor_SetSpeed(i, i)");
    dispatch_reg(&dispatcher, Motor_Run,      "Motor_Run(i)");

    /* 开启 DMA Circular 循环接收 */
    HAL_UART_Receive_DMA(&huart1, dma_buf, DMA_BUF_SIZE);

    while (1) {
        /* 1. 自动同步并扫描 */
        uint16_t readable = ringbuf_readable(&dma_ring);
        if (readable > 0) {
            cmd_scanner_t scanner;
            cmd_init(&scanner, dma_buf + dma_ring.tail, readable);

            cmd_entry_t entry;
            while (cmd_scan(&scanner, &entry) == CMD_COMPLETE) {
                cmd_queue_push(&cmd_queue, &entry);
            }
            ringbuf_skip(&dma_ring, scanner.scan_pos);
        }

        /* 2. 消费队列 */
        cmd_entry_t entry;
        while (!cmd_queue_is_empty(&cmd_queue)) {
            cmd_queue_pop(&cmd_queue, &entry);
            cmd_args_t args;
            cmd_parse((const char*)entry.buf + entry.cmd_start, entry.cmd_len, &args);

            invoke_call(&dispatcher, &args, NULL);
        }
    }
}
```

**命令格式（多条）：**
```
Motor_SetSpeed(1, 100)
Motor_SetSpeed(2, 200)
Motor_Run(1)
```

---

## 3. 中断任务（抢占/协作式检查）

在执行耗时长任务期间，可以通过 `cmd_queue_check()` 检查是否有高优先级/停止指令传入：

```c
/* 电机运行（可被中断） */
void Motor_Run(void* speed) {
    for (int i = 0; i < 1000; i++) {
        /* 在耗时任务中扫描 ringbuf (内部自动同步 DMA) */
        uint16_t readable = ringbuf_readable(&dma_ring);
        if (readable > 0) {
            cmd_scanner_t scanner;
            cmd_init(&scanner, dma_buf + dma_ring.tail, readable);
            cmd_entry_t entry;
            while (cmd_scan(&scanner, &entry) == CMD_COMPLETE) {
                cmd_queue_push(&cmd_queue, &entry);
            }
            ringbuf_skip(&dma_ring, scanner.scan_pos);
        }

        /* 检查队列中是否有 "Motor_Stop" 命令 */
        if (cmd_queue_check(&cmd_queue, "Motor_Stop")) {
            return;  /* 被中断，提前退出 */
        }

        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, *(int64_t*)speed);
        HAL_Delay(10);
    }
}
```

int main(void) {
    dispatch_init(&dispatcher);
    cmd_queue_init(&cmd_queue);

    dispatch_reg(&dispatcher, Motor_Run,  "Motor_Run(i)");
    dispatch_reg(&dispatcher, Motor_Stop, "Motor_Stop()");

    while (1) {
        cmd_entry_t entry;
        while (!cmd_queue_is_empty(&cmd_queue)) {
            cmd_queue_pop(&cmd_queue, &entry);
            cmd_args_t args;
            cmd_parse((const char*)entry.buf + entry.cmd_start, entry.cmd_len, &args);

            invoke_call(&dispatcher, &args, NULL);
        }
    }
}
```

**使用流程：**
1. 发送 `Motor_Run(100)` 启动电机
2. 运行期间发送 `Motor_Stop()` 中断电机运行

---

## 4. 返回值捕获

```c
int64_t add(void* a, void* b) {
    return *(int64_t*)a + *(int64_t*)b;
}

/* 注册 */
dispatch_reg(&dispatcher, add, "add(i, i) -> i");

/* 调用并捕获返回值 */
cmd_args_t result;
cmd_parse("add(10, 20)", 12, &result);

invoke_ret_t ret;
dispatch_status_t s = invoke_call(&dispatcher, &result, &ret);
if (s == DISPATCH_OK && ret.type == INVOKERET_I64) {
    printf("result: %ld\n", (long)ret.i64);  // result: 30
}

/* 开启 RPC_INVOKE_AUTO_RETURN 后，invoke_call 会自动通过 rpc_log 上报 [RETURN]: 30\r\n */
```

---

## 5. 签名格式参考

```
"函数名(参数类型...) -> 返回类型"
```

| 字符 | 类型 | 示例 |
|------|------|------|
| `v` | void | `"hello()"` |
| `i` / `i64` | int64 | `"add(i, i) -> i"` |
| `u` / `u64` | uint64 | `"hex(u) -> u"` |
| `s` | string | `"echo(s) -> s"` |
| `f` / `f64` | float64 | `"pi() -> f"` |

- 无 `->` 则 void 返回: `"proc(s, i, u)"` 等价于 `"proc(s, i, u) -> v"`
- 不带括号也支持: `"i, i -> i"`

---

## 6. DMA 环形缓冲区集成 (Circular DMA)

> **硬件环形缓冲区原理**: 串口开启 DMA Circular 模式后，硬件 DMA 在后台自动将接收到的字节循环写入 `dma_buf`，**CPU 在接收阶段零中断开销**。数据的提取与扫描通过主循环 (`main`) 或定时器 (`Timer/SysTick`) 完成。

---

### 方式 A：主循环 (`main`) 扫描 (推荐，最简零中断)

无需任何串口中断，在 `main` 循环中定期同步 DMA 指针并提取指令：

```c
#include "rpc.h"

#define DMA_BUF_SIZE  2048
uint8_t dma_buf[DMA_BUF_SIZE];
ringbuf_t dma_ring;
cmd_queue_t cmd_queue;
dispatch_registry_t dispatcher;

/* STM32: 从 CNDTR/NDTR 寄存器读取 DMA 当前写指针位置 */
uint16_t get_dma_head(void) {
    return DMA_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx);
}

void DMA_Init(void) {
    ringbuf_init(&dma_ring, dma_buf, DMA_BUF_SIZE);
#ifdef RINGBUF_DMA
    ringbuf_set_head_reader(&dma_ring, get_dma_head);
#endif
    cmd_queue_init(&cmd_queue);

    /* 启动 UART DMA Circular 模式，后台持续接收 */
    HAL_UART_Receive_DMA(&huart1, dma_buf, DMA_BUF_SIZE);
}

int main(void) {
    DMA_Init();
    dispatch_init(&dispatcher);
    dispatch_reg(&dispatcher, LED_On,  "LED_On(i)");
    dispatch_reg(&dispatcher, LED_Off, "LED_Off(i)");

    while (1) {
        /* 1. 同步 DMA 写指针 (head) */
        ringbuf_set_head(&dma_ring, get_dma_head());

        /* 2. 扫描 ringbuf 中到达的新字节 */
        uint16_t readable = ringbuf_readable(&dma_ring);
        if (readable > 0) {
            cmd_scanner_t scanner;
            cmd_init(&scanner, dma_buf + dma_ring.tail, readable);

            cmd_entry_t entry;
            while (cmd_scan(&scanner, &entry) == CMD_COMPLETE) {
                cmd_queue_push(&cmd_queue, &entry);
            }
            /* 消费已扫描字节 */
            ringbuf_skip(&dma_ring, scanner.scan_pos);
        }

        /* 3. 出队并调度执行 RPC 命令 */
        cmd_entry_t entry;
        while (!cmd_queue_is_empty(&cmd_queue)) {
            cmd_queue_pop(&cmd_queue, &entry);
            cmd_args_t args;
            cmd_parse((const char*)entry.buf + entry.cmd_start, entry.cmd_len, &args);

            invoke_ret_t ret;
            invoke_call(&dispatcher, &args, &ret);
        }
    }
}
```

---

### 方式 B：定时器扫描 (Timer / SysTick Scanning)

在 1ms~10ms 滴答定时器中断或 RTOS 任务中做轻量扫描入队，实现“提取”与“执行”解耦：

```c
/* 定时器中断 callback / SysTick / 1ms RTOS Task */
void System_Timer_10ms_Tick(void) {
    /* 同步 DMA 写位置 */
    ringbuf_set_head(&dma_ring, get_dma_head());

    uint16_t readable = ringbuf_readable(&dma_ring);
    if (readable > 0) {
        cmd_scanner_t scanner;
        cmd_init(&scanner, dma_buf + dma_ring.tail, readable);

        cmd_entry_t entry;
        while (cmd_scan(&scanner, &entry) == CMD_COMPLETE) {
            cmd_queue_push(&cmd_queue, &entry);  /* 仅推送至队列，极轻量 */
        }
        ringbuf_skip(&dma_ring, scanner.scan_pos);
    }
}

/* 主循环仅负责执行 RPC 命令 */
int main(void) {
    DMA_Init();
    dispatch_init(&dispatcher);
    dispatch_reg(&dispatcher, Motor_Run, "Motor_Run(i)");

    while (1) {
        cmd_entry_t entry;
        while (!cmd_queue_is_empty(&cmd_queue)) {
            cmd_queue_pop(&cmd_queue, &entry);
            cmd_args_t args;
            cmd_parse((const char*)entry.buf + entry.cmd_start, entry.cmd_len, &args);

            invoke_call(&dispatcher, &args, NULL);
        }
    }
}
```

---

## 注意事项

1. **零串口中断**：开启 DMA Circular 模式后，无需编写任何串口中断回调，全靠 `main` 循环或定时器同步硬件写指针并扫描。
2. **参数解引用**：回调函数签名统一为 `void func(void* arg)`，内部根据类型使用 `*(int64_t*)arg` 读取参数值。
3. **大小写敏感**：函数名注册与命令解析均严格区分大小写。
4. **返回值捕获**：使用 `invoke_call` 传入 `invoke_ret_t*` 指针即可捕获函数返回值。

