/****************************
 * RPC_CFG - CarrotRPC 全局配置
 *
 * 统一管理所有模块的编译开关与参数。
 * 优先级: 命令行参数 (-DXXX=val) > 1. 用户配置区 > 2. default设置区
 *****************************/
#ifndef _RPC_CFG_H_
#define _RPC_CFG_H_

/*=============================================================
 * 1. 用户配置区 (User Configuration Zone)
 *
 * 提示: 用户在此处手动启用/禁用功能或修改参数值。
 * 所有开关宏均为 0/1 标志位 (1=启用, 0=禁用)。
 * 注释某个宏时，将使用下方 [default设置区] 的默认值。
 *=============================================================*/

/* --- 1.1 ringbuf 模块配置 --- */
#define RINGBUF_DMA             1   /* 1: 启用 DMA 硬件同步与切片状态机, 0: 禁用 */

/* --- 1.2 rpclog 模块配置 --- */
#define RPC_LOG_OUTPUT_BUF      1   /* 1: 启用日志缓冲区模式 (推荐, 适合 UART DMA), 0: 单字节输出 */
#define RPC_LOG_ENABLE_DEBUG    1   /* 1: 启用 DEBUG 日志, 0: 禁用 */
#define RPC_LOG_ENABLE_INFO     1   /* 1: 启用 INFO 日志,  0: 禁用 */
#define RPC_LOG_ENABLE_WARN     1   /* 1: 启用 WARN 日志,  0: 禁用 */
#define RPC_LOG_ENABLE_ERROR    1   /* 1: 启用 ERROR 日志, 0: 禁用 */

/* --- 1.3 cmdqueue 模块配置 --- */
#define RPC_USE_CMD_QUEUE       1   /* 1: 启用命令队列, 0: 禁用(0 RAM/Flash 占用) */
#define CMD_QUEUE_SIZE          128 /* 命令队列容量 (最大可排队命令条数) */

/* --- 1.4 dispatch 模块配置 --- */
#define DISPATCH_MAX_FUNC_CNT   64  /* 最大注册函数数 */
#define DISPATCH_ARGS_MAX_CNT   9   /* 单个函数最大参数数 */
#define DISPATCH_FUNC_NAME_MAX  32  /* 单个函数名最大字符长度 */

/* --- 1.5 invoke 模块配置 --- */
#define INVOKE_STR_MAX_SIZE     64  /* 字符串返回值最大长度 */
#define RPC_INVOKE_AUTO_RETURN  1   /* 1: 自动输出函数返回值 [RETURN]: <VALUE>, 0: 禁用 */


/*=============================================================
 * 2. default设置区 (Default Settings Zone)
 *
 * 仅在命令行 (-D) 和上方用户配置区均未定义时生效。
 *=============================================================*/

/* --- ringbuf default设置 --- */
#ifndef RINGBUF_DMA
#define RINGBUF_DMA             1
#endif

/* --- rpclog default设置 --- */
#ifndef RPC_LOG_OUTPUT_BUF
#define RPC_LOG_OUTPUT_BUF      1
#endif

#ifndef RPC_LOG_ENABLE_DEBUG
#define RPC_LOG_ENABLE_DEBUG    1
#endif

#ifndef RPC_LOG_ENABLE_INFO
#define RPC_LOG_ENABLE_INFO     1
#endif

#ifndef RPC_LOG_ENABLE_WARN
#define RPC_LOG_ENABLE_WARN     1
#endif

#ifndef RPC_LOG_ENABLE_ERROR
#define RPC_LOG_ENABLE_ERROR    1
#endif

/* --- cmdqueue default设置 --- */
#ifndef RPC_USE_CMD_QUEUE
#define RPC_USE_CMD_QUEUE       1
#endif

#ifndef CMD_QUEUE_SIZE
#define CMD_QUEUE_SIZE          128
#endif

/* --- dispatch default设置 --- */
#ifndef DISPATCH_MAX_FUNC_CNT
#define DISPATCH_MAX_FUNC_CNT   64
#endif

#ifndef DISPATCH_ARGS_MAX_CNT
#define DISPATCH_ARGS_MAX_CNT   9
#endif

#ifndef DISPATCH_FUNC_NAME_MAX
#define DISPATCH_FUNC_NAME_MAX  32
#endif

/* --- invoke default设置 --- */
#ifndef INVOKE_STR_MAX_SIZE
#define INVOKE_STR_MAX_SIZE     64
#endif

#ifndef RPC_INVOKE_AUTO_RETURN
#define RPC_INVOKE_AUTO_RETURN  1
#endif

#endif /* _RPC_CFG_H_ */
