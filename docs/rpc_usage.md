# CarrotRPC 极简使用指南 (v2.1)

CarrotRPC 是专为嵌入式 MCU 设计的轻量级 ASCII 动态函数调用框架，原生集成 **UART Circular DMA 接收** 与 **DMA TX 切片发送**。

---

## 1. 极致精简：STM32 DMA 接收 + 零拷贝 RPC 调度 (推荐范例)

无需任何串口接收中断回调，在 `main` 循环中自动探查 DMA 硬件缓冲区并执行预注册函数：

```c
#include "rpc.h"

#define DMA_BUF_SIZE  1024
uint8_t dma_buf[DMA_BUF_SIZE];
ringbuf_t dma_ring;
dispatch_registry_t dispatcher;
cmd_scanner_t scanner;

/* 1. 从 CNDTR 寄存器获取 DMA 当前写游标 */
uint16_t get_dma_head(void) {
    return DMA_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx);
}

/* 2. RPC 被调函数 */
void LED_On(void* arg)  { HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET); }
void LED_Off(void* arg) { HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET); }
int64_t add(void* a, void* b) { return *(int64_t*)a + *(int64_t*)b; }

int main(void) {
    /* 初始化模块与 DMA 绑定 */
    dispatch_init(&dispatcher);
    ringbuf_init(&dma_ring, dma_buf, sizeof(dma_buf));
    ringbuf_set_head_reader(&dma_ring, get_dma_head);
    cmd_init(&scanner, NULL, 0);

    /* 动态签名注册 */
    dispatch_reg(&dispatcher, LED_On,  "LED_On(i)");
    dispatch_reg(&dispatcher, LED_Off, "LED_Off(i)");
    dispatch_reg(&dispatcher, add,     "add(i, i) -> i");

    /* 启动 UART Circular DMA 循环接收 (零 CPU 中断开销) */
    HAL_UART_Receive_DMA(&huart1, dma_buf, sizeof(dma_buf));

    while (1) {
        /* 原生环形零拷贝探查 + 自动解析 + 动态调度 */
        cmd_entry_t entry;
        while (cmd_scan_ringbuf(&scanner, &dma_ring, &entry) == CMD_COMPLETE) {
            cmd_args_t args;
            cmd_parse_ringbuf(&dma_ring, &entry, &args);

            invoke_ret_t ret;
            invoke_call(&dispatcher, &args, &ret);
            /* 若开启 RPC_INVOKE_AUTO_RETURN，有返回值时将自动通过 rpc_log 打印 [RETURN]: 30 */
        }
    }
}
```

**串口输入示例：**
```text
LED_On(1)
add(10, 20)
```

---

## 2. DMA TX 切片发送 (零拷贝链式续发)

驱动端无需任何 busy 标志位计算，轮询与 TC 中断配合实现零拷贝连续输出：

```c
uint8_t tx_buf[512];
ringbuf_t tx_ring;

/* 轮询 / 主线程：fetch 发送切片 */
void USART_Tx_Poll(void) {
    const uint8_t* ptr;
    uint16_t len;
    if (ringbuf_dma_tx_fetch(&tx_ring, &ptr, &len)) {
        HAL_UART_Transmit_DMA(&huart1, (uint8_t*)ptr, len);
    }
}

/* DMA 发送完成 TC 中断回调：自动探查回绕链式续发 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    const uint8_t* next_ptr;
    uint16_t next_len;
    if (ringbuf_dma_tx_complete(&tx_ring, &next_ptr, &next_len)) {
        HAL_UART_Transmit_DMA(&huart1, (uint8_t*)next_ptr, next_len);
    }
}
```

---

## 3. 命令队列与长任务中断 (可选)

如需对命令进行缓解答起排队，或在耗时长任务中协作式响应停止指令：

```c
uint8_t queue_buf[CMD_QUEUE_BUF_SIZE];
cmd_queue_t cmd_queue;

/* 初始化 */
cmd_queue_init(&cmd_queue, queue_buf, sizeof(queue_buf));

/* 长时间循环中快速检查是否有 Motor_Stop */
void LongTask_Run(void) {
    for (int i = 0; i < 1000; i++) {
        cmd_entry_t entry;
        while (cmd_scan_ringbuf(&scanner, &dma_ring, &entry) == CMD_COMPLETE) {
            cmd_queue_push(&cmd_queue, &entry);
        }
        if (cmd_queue_check(&cmd_queue, "Motor_Stop")) {
            return; /* 抢占中断退出 */
        }
        HAL_Delay(10);
    }
}
```

---

## 4. 签名与类型速查

签名格式：`"函数名(参数类型...) -> 返回类型"`

| 字符 | 对应 C 类型 | 签名示例 | 说明 |
|:---:|:---:|:---:|:---|
| `v` | `void` | `"hello()"` | 无参数，void 返回 |
| `i` / `i64` | `int64_t` | `"add(i, i) -> i"` | 64 位有符号整数 (解析 '-' 及十进制) |
| `u` / `u64` | `uint64_t` | `"hex(u) -> u"` | 64 位无符号整数 (支持 0x 及纯 Hex) |
| `s` | `char*` | `"echo(s) -> s"` | 字符串指针 |
| `f` / `f64` | `double` | `"pi() -> f"` | 双精度浮点数 |

* **全局字符串返回**：处理函数若返回 char*，可直接写往 `g_rpc_str_ret_buf` / `RPC_STR_RET_BUF` 共享缓冲区，避免野指针。��片状态机，驱动端无需手动计算边界或声明 busy 标志位：

```c
#include "rpc.h"

uint8_t tx_buf[512];
ringbuf_t tx_ring;

void USART_Tx_Init(void) {
    ringbuf_init(&tx_ring, tx_buf, sizeof(tx_buf));
}

/* 1. 主线程 / 轮询发送：尝试 fetch 可发送切片 */
void USART_Tx_Poll(void) {
    const uint8_t* ptr;
    uint16_t len;
    /* 自动检查 busy、计算连续切片与边界截断 */
    if (ringbuf_dma_tx_fetch(&tx_ring, &ptr, &len)) {
        HAL_UART_Transmit_DMA(&huart1, (uint8_t*)ptr, len);
    }
}

/* 2. DMA 发送完成中断 (TC 中断) */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    const uint8_t* next_ptr;
    uint16_t next_len;
    /* 自动提交移走已发字节，若跨越回绕边界自动传出下一段链式续发 */
    if (ringbuf_dma_tx_complete(&tx_ring, &next_ptr, &next_len)) {
        HAL_UART_Transmit_DMA(&huart1, (uint8_t*)next_ptr, next_len);
    }
}
```

---

## 注意事项

1. **零串口中断 RX**：开启 DMA Circular 模式后，无需编写任何串口接收中断回调，全靠 `cmd_scan_ringbuf` 直接扫描环形缓冲区。
2. **零拷贝 DMA TX**：使用 `ringbuf_dma_tx_fetch` 和 `ringbuf_dma_tx_complete` 能够免去应用层边界计算与 busy 状态跟踪。
3. **参数解引用**：回调函数签名统一为 `void func(void* arg)`，内部根据类型使用 `*(int64_t*)arg` 读取参数值。
4. **大小写敏感**：函数名注册与命令解析均严格区分大小写。
5. **返回值捕获**：使用 `invoke_call` 传入 `invoke_ret_t*` 指针即可捕获函数返回值；字符串型返回可直接填充到全局 `g_rpc_str_ret_buf` / `RPC_STR_RET_BUF`。

