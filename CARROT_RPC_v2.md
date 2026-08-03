# CARROT RPC 协议通信手册 (v2.0)

本手册定义了 CarrotRPC 下位机与上位机之间的远程过程调用（RPC）报文规范 v2.0，包含 **ASCII 可视化 RPC** 与 **Binary 高速流 RPC (扩展)**。

CarrotRPC C 语言库原生完整实现了 ASCII RPC 命令的零拷贝解析、动态函数分发与分级日志/协议数据上报。

---

## 1. ASCII RPC 接口规范

所有 ASCII 报文均以 `\r\n` (0x0D, 0x0A) 或 `\n` (0x0A) 结尾。

### 1.1 命令请求规范（上位机 -> 下位机）

下位机接收到的命令由 **函数名** 与 **参数列表** 组成，支持以下格式：

*   **语法**: 
    *   带括号格式: `func_name(arg1, arg2, ...)`
    *   无括号空格分隔格式: `func_name arg1 arg2 ...`
*   **支持的数据类型**:
    *   `int64`: 十进制带符号整数（例: `100`, `-456`）
    *   `uint64`: 十六进制无符号整数（例: `0x12AF`, `0XFF`）
    *   `float64`: 十进制浮点数 / 科学计数法（例: `3.14`, `-0.5`, `1.2e-3`）
    *   `string`: 字符串文本（例: `"hello"`, `world`）
*   **示例**:
    *   `LED_On(1)`
    *   `Motor_SetSpeed(1, 100)`
    *   `add 10 20`

---

### 1.2 系统消息与响应规范（下位机 -> 上位机）

所有下位机上报报文统一采用 **纯固定前缀 Tag** 格式：`[<TAG>]: <PAYLOAD>`。前缀永远只有固定的 7 种系统 Tag。

#### 1.2.1 系统日志接口 (`MSG`)
用于推送带状态前缀的日志信息。
*   **语法**: `[<LEVEL>]: <MESSAGE>`
*   **级别**: `DEBUG`, `INFO`, `WARN`, `ERROR`
*   **示例**: `[INFO]: System Boot Complete`

#### 1.2.2 结构化数据接口 (`DATA`)
用于上报实时数值、多通道数据及波形数据块。
*   **语法**: `[DATA]: <PAYLOAD>`
*   **正交分隔符规范**:
    *   **分号 `;` (通道分隔符)**: 用于区分不同的逻辑通道或特征维度（如 `CH1; CH2`）。
    *   **逗号 `,` (数据追加分隔符)**: 用于在同一通道内追加连续时间采样点/波形点（如 `v1, v2, v3`）。
    *   **等号 `=` (命名关联符)**: 用于可选的通道或变量命名（如 `CH1=v1`）。
*   **常见表达模式**:
    *   **多通道单点模式** (`;` 隔通道): `[DATA]: 1.2; 3.4; 5.6` (CH1=1.2, CH2=3.4, CH3=5.6)
    *   **单通道波形追加模式** (`,` 隔时间点): `[DATA]: 10,12,15,18` (单通道连续 4 个采样点)
    *   **多通道命名模式** (`;` 隔通道名): `[DATA]: CH1=1.2; CH2=3.4` 或 `[DATA]: IMU.x=1.2; IMU.y=0.5`
    *   **多通道波形矩阵模式** (`;` 隔通道，`,` 隔时间点): `[DATA]: CH1=1,2,3; CH2=10,20,30`

#### 1.2.3 寄存器访问接口 (`REG`)
用于返回寄存器或特定位域当前值。
*   **语法**: `[REG]: <PAYLOAD>`
*   **支持格式**:
    *   **单/多寄存器** (显式地址键值对): `[REG]: {<FILE>.}0x<ADDR1>=0x<HEX_VAL1>,{<FILE>.}0x<ADDR2>=0x<HEX_VAL2>...`
    *   **位域访问**: `[REG]: {<FILE>.}0x<ADDR>.b<END>_<START>=0x<HEX_VAL>`
*   **示例**: 
    *   `[REG]: 0x4001=0x00FF` (单寄存器)
    *   `[REG]: file0.0xAA=0x0012` (带寄存器组名)
    *   `[REG]: 0x4000=0x0010,0x4001=0x0020` (多寄存器显式地址)
    *   `[REG]: 0x10.b7_4=0xA` (0x10 寄存器的 Bit 7~4 位域)

#### 1.2.4 函数返回值接口 (`RETURN`)
用于返回动态调用的函数执行结果。
*   **语法**: `[RETURN]: <VALUE>`
*   **示例**: `[RETURN]: 30`

---

## 2. Binary RPC 接口规范 (TODO / 暂不支持)

> [!NOTE]
> **暂不支持**：当前 CarrotRPC 框架核心专注于 ASCII RPC 命令解析与统一固定 Tag 报文（`[BIN]:` 标记的 Hex/RAW 扩展与参数类型正在评估演进中）。以下原始裸数据包结构仅作为远期设计参考草案。

### 2.1 裸包结构草案 (DATA_266)
| 偏移 | 字段 | 类型 | 说明 |
| :--- | :--- | :--- | :--- |
| 0 | `START` | uint8 | 固定为 `0x3C` (`<`) |
| 1 | `ID` | uint8 | `0x42` (DATA_266) |
| 2 | `FLAGS` | uint16 | 数据元信息 (端序、位宽等) |
| 4 | `STREAM_ID` | uint8 | 逻辑通道 ID |
| 5 | `LEN` | uint16 | Payload 有效载荷长度 |
| 7 | `PAYLOAD` | uint8[256] | 原始二进制数据 |
| 263 | `CRC` | uint16 | 校验码 |
| 265 | `END` | uint8 | 固定为 `0x3E` (`>`) |

---

## 3. 开发者调用示例 (CarrotRPC C SDK)

### 3.1 接收并解析 ASCII 命令

```c
#include "rpc.h"

// 注册处理函数
void Motor_SetSpeed(void* channel, void* speed) {
    int64_t ch = *(int64_t*)channel;
    int64_t spd = *(int64_t*)speed;
    // ... 执行硬件控制
}

// 在 RPC 调度初始化时注册
dispatch_reg(&dispatcher, Motor_SetSpeed, "Motor_SetSpeed(i, i)");

// 零拷贝解析并调用（收到 ASCII 命令 "Motor_SetSpeed(1, 100)\r\n"）
cmd_args_t args;
cmd_parse(rx_cmd_str, rx_len, &args);
invoke_call(&dispatcher, &args, NULL);
```

### 3.2 下位机日志与协议数据上报

```c
#include "rpc.h"

// 1. 推送带级别的日志 -> 输出: [INFO]: Battery Level 85%
rpc_info("Battery Level %d%%", 85);

// 2. 推送结构化数据 -> 输出: [DATA]: CH1=1; CH2=2; CH3=3
rpc_data("CH1=%d; CH2=%d; CH3=%d", 1, 2, 3);
// 或推送多通道波形矩阵 -> 输出: [DATA]: CH1=1,2,3; CH2=10,20,30
rpc_data("CH1=1,2,3; CH2=10,20,30");
// 或带模块名字段 -> 输出: [DATA]: IMU.x=1.20; IMU.y=0.50
rpc_data("IMU.x=%.2f; IMU.y=%.2f", 1.2, 0.5);

// 3. 返回 16 进制寄存器值 -> 输出: [REG]: 0x4001=0x00FF
rpc_reg("0x4001=0x00FF");
// 或返回多寄存器显式值 -> 输出: [REG]: 0x4000=0x0010,0x4001=0x0020
rpc_reg("0x4000=0x0010,0x4001=0x0020");
```
