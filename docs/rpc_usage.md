# CarrotRPC 使用指南

## 1. 即时执行（单条命令）

```c
#include "dispatch.h"
#include "invoke.h"
#include "cmdscan.h"
#include "cmdqueue.h"

/* RPC 函数定义 */
void LED_On(void* channel) {
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
}

void LED_Off(void* channel) {
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
}

/* DMA 回调：预解析后推入队列 */
cmd_queue_t cmd_queue;
cmd_scanner_t cmd_scanner;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    cmd_entry_t entry;
    cmd_init(&cmd_scanner, rx_buffer, rx_len);
    if (cmd_scan(&cmd_scanner, &entry) == CMD_COMPLETE) {
        cmd_queue_push(&cmd_queue, &entry);
    }
    HAL_UART_Receive_DMA(&huart1, rx_buffer, RX_SIZE);
}

/* 主程序 */
static dispatch_registry_t dispatcher;

int main(void) {
    dispatch_init(&dispatcher);
    dispatch_reg(&dispatcher, LED_On,  "LED_On(i)");
    dispatch_reg(&dispatcher, LED_Off, "LED_Off(i)");
    cmd_queue_init(&cmd_queue);

    while (1) {
        cmd_entry_t entry;
        while (!cmd_queue_is_empty(&cmd_queue)) {
        cmd_queue_pop(&cmd_queue, &entry);
        cmd_args_t result;
        cmd_parse((const char*)entry.buf + entry.cmd_start, entry.cmd_len, &result);

        invoke_ret_t ret;
        invoke_call(&dispatcher, &result, &ret);
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
/* 命令队列 */
cmd_queue_t cmd_queue;
cmd_scanner_t cmd_scanner;

/* DMA 回调：预解析后推入队列 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    cmd_entry_t entry;
    cmd_init(&cmd_scanner, rx_buffer, rx_len);
    if (cmd_scan(&cmd_scanner, &entry) == CMD_COMPLETE) {
        cmd_queue_push(&cmd_queue, &entry);
    }
    HAL_UART_Receive_DMA(&huart1, rx_buffer, RX_SIZE);
}

/* 主循环：逐条执行 */
int main(void) {
    dispatch_init(&dispatcher);
    dispatch_reg(&dispatcher, Motor_SetSpeed, "Motor_SetSpeed(i, i)");
    dispatch_reg(&dispatcher, Motor_Run,     "Motor_Run(i)");
    cmd_queue_init(&cmd_queue);

    while (1) {
        cmd_entry_t entry;
        while (!cmd_queue_is_empty(&cmd_queue)) {
        cmd_queue_pop(&cmd_queue, &entry);
        cmd_args_t result;
        cmd_parse((const char*)entry.buf + entry.cmd_start, entry.cmd_len, &result);

        invoke_call(&dispatcher, &result, NULL);
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

## 3. 中断任务

使用 `cmd_queue_check()` 在长任务中检查是否有中断命令：

```c
/* 电机运行（可被中断） */
void Motor_Run(void* speed) {
    for (int i = 0; i < 1000; i++) {
        /* 检查是否有 "Motor_Stop" 命令 */
        if (cmd_queue_check(&cmd_queue, "Motor_Stop")) {
            return;  /* 被中断，提前退出 */
        }

        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, *(int64_t*)speed);
        HAL_Delay(10);
    }
}

/* 命令队列 */
cmd_queue_t cmd_queue;

/* DMA 回调 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    cmd_entry_t entry;
    cmd_init(&cmd_scanner, rx_buffer, rx_len);
    if (cmd_scan(&cmd_scanner, &entry) == CMD_COMPLETE) {
        cmd_queue_push(&cmd_queue, &entry);
    }
    HAL_UART_Receive_DMA(&huart1, rx_buffer, RX_SIZE);
}

/* 主程序 */
int main(void) {
    dispatch_init(&dispatcher);
    dispatch_reg(&dispatcher, Motor_Run,  "Motor_Run(i)");
    dispatch_reg(&dispatcher, Motor_Stop, "Motor_Stop()");
    cmd_queue_init(&cmd_queue);

    while (1) {
        cmd_entry_t entry;
        while (!cmd_queue_is_empty(&cmd_queue)) {
        cmd_queue_pop(&cmd_queue, &entry);
        cmd_args_t result;
        cmd_parse((const char*)entry.buf + entry.cmd_start, entry.cmd_len, &result);

        invoke_call(&dispatcher, &result, NULL);
    }
    }
}
```

**使用流程：**
1. 发送 `Motor_Run(100)` 启动电机
2. 发送 `Motor_Stop` 中断电机运行

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
    printf("result: %ld\n", ret.i64);  // result: 30
}
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

## 6. DMA 环形缓冲区集成

> **启用 DMA 模式**: 在 `ringbuf.h` 中取消注释 `#define RINGBUF_DMA`，启用硬件同步功能。

```c
#include "ringbuf.h"
#include "cmdscan.h"
#include "cmdqueue.h"
#include "dispatch.h"
#include "invoke.h"

/* STM32 DMA 环形缓冲区 */
#define DMA_BUF_SIZE  2048
uint8_t dma_buf[DMA_BUF_SIZE];
ringbuf_t dma_ring;

/* STM32: 从 NDTR 寄存器读取 DMA 写位置 */
uint16_t get_dma_head(void) {
    return DMA_BUF_SIZE - DMA1_SNDTR(DMA_BUF_SIZE);
}

/* 初始化 */
void DMA_Init(void) {
    ringbuf_init(&dma_ring, dma_buf, DMA_BUF_SIZE);
    ringbuf_set_head_reader(&dma_ring, get_dma_head);
    cmd_queue_init(&cmd_queue);
}

/* DMA 回调：更新 head，扫描命令，推入队列 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    cmd_scanner_t scanner;
    cmd_init(&scanner, dma_ring.buf, dma_ring.size);

    cmd_entry_t entry;
    while (cmd_scan(&scanner, &entry) == CMD_COMPLETE) {
        cmd_queue_push(&cmd_queue, &entry);
    }
}

/* 主循环 */
int main(void) {
    DMA_Init();
    dispatch_init(&dispatcher);
    dispatch_reg(&dispatcher, LED_On,  "LED_On(i)");
    dispatch_reg(&dispatcher, LED_Off, "LED_Off(i)");

    while (1) {
        cmd_entry_t entry;
        while (!cmd_queue_is_empty(&cmd_queue)) {
        cmd_queue_pop(&cmd_queue, &entry);
        cmd_args_t result;
        cmd_parse((const char*)entry.buf + entry.cmd_start, entry.cmd_len, &result);

        invoke_call(&dispatcher, &result, NULL);
    }
    }
}
```

**软件模式（调试用，无需 `RINGBUF_DMA`）：**

```c
ringbuf_init(&dma_ring, dma_buf, DMA_BUF_SIZE);

/* 在轮询中手动更新 head */
ringbuf_set_head(&dma_ring, DMA1_SNDTR(DMA_BUF_SIZE));

/* 可读数据量 */
uint16_t len = ringbuf_readable(&dma_ring);

/* 读取数据（处理 wrap-around） */
uint8_t tmp[256];
ringbuf_peek(&dma_ring, tmp, len);

/* 跳过已处理数据 */
ringbuf_skip(&dma_ring, len);
```

---

## 注意事项

1. **回调中不要执行命令**：DMA 回调只扫描并推入队列，在主循环中执行
2. **参数是指针**：函数签名 `void func(void* arg)`，内部 `*(int64_t*)arg` 读取值
3. **函数名区分大小写**
4. **DMA 缓冲区**：`cmd_scan` 支持环形/非环形缓冲区，传入实际数据长度即可
5. **签名字符串**：类型写错不会编译报错，运行时解析失败返回 `DISPATCH_ERR_SIG`
