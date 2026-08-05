/****************************
 * RPC - CarrotRPC 统一入口
 *
 * 使用方式：
 *   #include "rpc.h"
 *
 * 然后在 rpc_cfg.h 中配置各模块开关。
 *****************************/
#ifndef _RPC_H_
#define _RPC_H_

/* 版本信息 */
#define CARROT_RPC_VERSION_MAJOR  2
#define CARROT_RPC_VERSION_MINOR  1
#define CARROT_RPC_VERSION_PATCH  0
#define CARROT_RPC_VERSION_STR    "2.1.0"

#include "rpc_cfg.h"
#include "ringbuf.h"
#include "cmdscan.h"
#include "cmdqueue.h"
#include "rpclog.h"
#include "typeconv.h"
#include "dispatch.h"
#include "invoke.h"

#endif /* _RPC_H_ */
