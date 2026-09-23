# EC2x&EG9x&EG2x-G&EM05 系列 QuecOpen 语音服务 API 参考手册 V1.0

> **文档信息**
> - 适用产品：LTE Standard 模块系列
> - 版本：1.0
> - 日期：2022-01-16
> - 状态：受控文件
> - 作者：Colin CUI（创建：2021-12-07，受控版本：2022-01-16）
> - 发布方：上海移远通信技术股份有限公司
> - 总页数：27 页

---

## 联系信息

上海移远通信技术股份有限公司
上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼　邮编：200233
电话：+86 21 5108 6236　邮箱：info@quectel.com
技术支持：http://www.quectel.com/cn/support/technical.htm 或 support@quectel.com

---

## 文档历史

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2021-12-07 | Colin CUI | 文档创建 |
| 1.0 | 2022-01-16 | Colin CUI | 受控版本 |

---

## 目录

1. [引言](#1-引言)
   - 1.1 适用模块
2. [语音服务相关 API](#2-语音服务相关-api)
   - 2.1 头文件
   - 2.2 函数概览
   - 2.3 函数描述
3. [语音服务使用步骤示例](#3-语音服务使用步骤示例)
4. [编译说明](#4-编译说明)
5. [语音服务演示步骤](#5-语音服务演示步骤)
   - 5.1 拨打电话
   - 5.2 接听并挂断电话
   - 5.3 保持呼叫
6. [附录 参考文档及术语缩写](#6-附录-参考文档及术语缩写)

---

## 表格索引

| 表格 | 页码 |
|---|---|
| 表 1：适用模块 | 7 |
| 表 2：函数概览 | 8 |
| 表 3：参考文档 | 27 |
| 表 4：术语缩写 | 27 |

---

## 1 引言

移远通信 EC2x 系列、EG9x 系列、EG2x-G 和 EM05 系列模块支持 QuecOpen® 方案；QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。有关 QuecOpen® 的详细信息，请参考**文档 [1]**。

本文档主要介绍移远通信 EC2x 系列、EG9x 系列、EG2x-G 和 EM05 系列 QuecOpen® 模块的**语音服务相关 API** 及语音服务示例。

### 1.1 适用模块

**表 1：适用模块**

| 模块系列 | 模块 |
|---|---|
| EC2x | EC25 系列 |
| EC2x | EC21 系列 |
| EC2x | EC20-CE |
| EC2x | EC20-CN |
| EG9x | EG95 系列 |
| EG9x | EG91 系列 |
| EG2x-G | EG25-G |
| EG2x-G | EG21-G |
| EM05 | EM05 系列 |

---

## 2 语音服务相关 API

### 2.1 头文件

语音服务接口头文件位于 SDK 中的 `ql-ol-sdk\qo-ol-extsdk\include` 目录下，**`ql_mcm_voice.h`** 文件即语音服务 API 的头文件。若无特别说明，本文档所提到的头文件均在该目录下。

### 2.2 函数概览

**表 2：函数概览**

| 函数名 | 说明 |
|---|---|
| `QL_Voice_Call_Client_Init()` | 初始化语音通话功能并获取使用句柄 |
| `QL_Voice_Call_Client_Deinit()` | 销毁语音通话功能资源 |
| `QL_Voice_Call_AddStateHandler()` | 注册上报语音通话状态的回调函数 |
| `QL_Voice_Call_RemoveStateHandler()` | 销毁注册上报电话状态的回调函数 |
| `QL_Voice_Call_Start()` | 主叫端拨打电话 |
| `QL_Voice_Call_End()` | 挂断或者拒接电话 |
| `QL_Voice_Call_Answer()` | 接听电话 |
| `QL_Voice_Call_GetWaitingStatus()` | 设置呼叫等待状态 |
| `QL_Voice_Call_SetWaiting()` | 获取模块呼叫等待状态 |
| `QL_Voice_Call_Hold()` | 将来电设置为呼叫保持状态 |
| `QL_Voice_Call_UnHold()` | 取消呼叫保持 |
| `QL_Voice_Call_CancelDial()` | 结束所有会话 |
| `QL_Voice_Call_Dtmf()` | 播放 DTMF 音 |
| `QL_Voice_Call_SetForwarding()` | 设置呼叫转移模式 |
| `QL_Voice_Call_GetForwardingStatus()` | 获取所有号码的呼叫转移模式状态 |

### 2.3 函数描述

---

#### 2.3.1 QL_Voice_Call_Client_Init

该函数用于初始化语音通话功能并获取一个使用句柄。

**函数原型**
```c
int QL_Voice_Call_Client_Init(voice_client_handle_type *ph_voice);
```

**参数**

*ph_voice*：
[Out] 语音通话句柄指针。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

##### 2.3.1.1 voice_client_handle_type

语音通话句柄定义如下：

```c
typedef uint32 voice_client_handle_type;
```

---

#### 2.3.2 QL_Voice_Call_Client_Deinit

该函数用于销毁语音通话功能资源。

**函数原型**
```c
int QL_Voice_Call_Client_Deinit(voice_client_handle_type h_voice);
```

**参数**

*h_voice*：
[In] 语音通话句柄。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

---

#### 2.3.3 QL_Voice_Call_AddStateHandler

该函数用于注册上报语音通话状态的回调函数。

**函数原型**
```c
int QL_Voice_Call_AddStateHandler(voice_client_handle_type    h_voice,
                                  QL_VoiceCall_StateHandlerFunc_t handlerPtr,
                                  void*                       contextPtr);
```

**参数**

*h_voice*：
[In] 语音通话句柄。

*handlerPtr*：
[In] 指向注册语音通话状态回调函数的指针，回调函数详见**第 2.3.3.1 章**。

*contextPtr*：
[In] 获取来电时的通话 ID。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

##### 2.3.3.1 QL_VoiceCall_StateHandlerFunc_t

该回调函数用于指示语音通话状态改变。

**函数原型**
```c
typedef void (*QL_VoiceCall_StateHandlerFunc_t)
(
    int                      call_id,
    char*                    phone_num,
    E_QL_VOICE_CALL_STATE_T  state,
    void*                    *contextPtr
);
```

**参数**

*call_id*：
[In] 通话 ID。

*phone_num*：
[In] 被叫号码。

*state*：
[In] 语音通话状态。详见**第 2.3.3.2 章**枚举 `ql_mcm_voice_call_state_t`。

*contextPtr*：
[In] 该参数无意义，**暂不支持**。

**返回值**：无

##### 2.3.3.2 ql_mcm_voice_call_state_t

电话状态枚举定义如下：

```c
typedef enum {
    E_QL_MCM_VOICE_CALL_STATE_INCOMING = 0x0000,
    E_QL_MCM_VOICE_CALL_STATE_DIALING  = 0x0001,
    E_QL_MCM_VOICE_CALL_STATE_ALERTING = 0x0002,
    E_QL_MCM_VOICE_CALL_STATE_ACTIVE   = 0x0003,
    E_QL_MCM_VOICE_CALL_STATE_HOLDING  = 0x0004,
    E_QL_MCM_VOICE_CALL_STATE_END      = 0x0005,
    E_QL_MCM_VOICE_CALL_STATE_WAITING
} ql_mcm_voice_call_state_t;
```

**成员**

| 成员 | 描述 |
|---|---|
| `E_QL_MCM_VOICE_CALL_STATE_INCOMING` (0x0000) | 有电话呼入 |
| `E_QL_MCM_VOICE_CALL_STATE_DIALING` (0x0001) | 正在拨号 |
| `E_QL_MCM_VOICE_CALL_STATE_ALERTING` (0x0002) | 被叫呼叫等待，主叫振铃中 |
| `E_QL_MCM_VOICE_CALL_STATE_ACTIVE` (0x0003) | 正在通话 |
| `E_QL_MCM_VOICE_CALL_STATE_HOLDING` (0x0004) | 呼叫保持 |
| `E_QL_MCM_VOICE_CALL_STATE_END` (0x0005) | 断开通话连接 |
| `E_QL_MCM_VOICE_CALL_STATE_WAITING` | 呼叫等待 |

---

#### 2.3.4 QL_Voice_Call_RemoveStateHandler

该函数用于销毁注册上报电话状态的回调函数。

**函数原型**
```c
ql_vc_errcode_e ql_voice_call_end(void);
```

**参数**

*h_voice*：
[In] 语音通话句柄。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

---

#### 2.3.5 QL_Voice_Call_Start

该函数用于主叫端拨打电话。

**函数原型**
```c
int QL_Voice_Call_Start(voice_client_handle_type h_voice,
                        E_QL_VCALL_ID_T          simId,
                        char*                    phone_number,
                        int                      *call_id);
```

**参数**

*h_voice*：
[In] 语音通话句柄。

*simId*：
[In] (U)SIM 卡槽 ID。**此参数当前不生效。**

*phone_number*：
[In] 被叫方手机号码。

*call_id*：
[Out] 本次通话的 ID。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

---

#### 2.3.6 QL_Voice_Call_End

该函数用于挂断或者拒接电话。

**函数原型**
```c
int QL_Voice_Call_End(voice_client_handle_type h_voice, int call_id);
```

**参数**

*h_voice*：
[In] 语音通话句柄。

*call_id*：
[In] 本次通话的 ID。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

---

#### 2.3.7 QL_Voice_Call_Answer

该函数用于接听电话。

**函数原型**
```c
int QL_Voice_Call_Answer(voice_client_handle_type h_voice, int call_id);
```

**参数**

*h_voice*：
[In] 语音通话句柄。

*call_id*：
[In] 本次通话的 ID。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

---

#### 2.3.8 QL_Voice_Call_SetWaiting

该函数用于设置呼叫等待状态。

**函数原型**
```c
int QL_Voice_Call_SetWaiting(int h_voice,
                             ql_mcm_voice_call_waiting_service_t *pe_service);
```

**参数**

*h_voice*：
[In] 语音通话句柄。

*pe_service*：
[In] 呼叫等待状态。详见**第 2.3.8.1 章**枚举 `E_QL_MCM_VOICE_CALL_WAITING_SERVICE_T`。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

##### 2.3.8.1 E_QL_MCM_VOICE_CALL_WAITING_SERVICE_T

呼叫等待状态枚举定义如下：

```c
typedef enum {
    E_QL_MCM_VOICE_CALL_WAITING_VOICE_ENABLED = 0,
    E_QL_MCM_VOICE_CALL_WAITING_DISABLED      = 1,
} E_QL_MCM_VOICE_CALL_WAITING_SERVICE_T;
```

**成员**

| 成员 | 描述 |
|---|---|
| `E_QL_MCM_VOICE_CALL_WAITING_VOICE_ENABLED` | 呼叫等待已启用 |
| `E_QL_MCM_VOICE_CALL_WAITING_DISABLED` | 呼叫等待已禁用 |

---

#### 2.3.9 QL_Voice_Call_GetWaitingStatus

该函数用于获取模块呼叫等待状态。

**函数原型**
```c
int QL_Voice_Call_GetWaitingStatus(int h_voice,
                                   ql_mcm_voice_call_waiting_service_t *pe_service);
```

**参数**

*h_voice*：
[In] 语音通话句柄。

*pe_service*：
[Out] 当前模块的语音呼叫等待状态。详见**第 2.3.8.1 章**枚举 `E_QL_MCM_VOICE_CALL_WAITING_SERVICE_T`。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

---

#### 2.3.10 QL_Voice_Call_Hold

该函数用于将来电设置为呼叫保持状态。

**函数原型**
```c
int QL_Voice_Call_Hold(voice_client_handle_type h_voice);
```

**参数**

*h_voice*：
[In] 语音通话句柄。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

---

#### 2.3.11 QL_Voice_Call_UnHold

该函数用于取消呼叫保持。

**函数原型**
```c
int QL_Voice_Call_UnHold(voice_client_handle_type h_voice);
```

**参数**

*h_voice*：
[In] 语音通话句柄。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

---

#### 2.3.12 QL_Voice_Call_CancelDial

该函数用于**结束所有会话**。

**函数原型**
```c
int QL_Voice_Call_CancelDial(voice_client_handle_type h_voice);
```

**参数**

*h_voice*：
[In] 语音通话句柄。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

---

#### 2.3.13 QL_Voice_Call_Dtmf

该函数用于播放 DTMF 音。

**函数原型**
```c
int QL_Voice_Call_Dtmf(voice_client_handle_type h_voice,
                       uint8_t digit,
                       int     call_id);
```

**参数**

*h_voice*：
[In] 语音通话句柄。

*digit*：
[In] DTMF 音对应的数字类型的 ASCII 码。

*call_id*：
[In] 本次通话的 ID。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

---

#### 2.3.14 QL_Voice_Call_SetForwarding

该函数用于设置呼叫转移模式。

**函数原型**
```c
int QL_Voice_Call_SetForwarding(int                                  h_voice,
                                E_QL_MCM_VOICE_CALL_SERVICE_T        service,
                                E_QL_MCM_VOICE_CALL_FORWARDING_REASON_T reason,
                                char                                 *number);
```

**参数**

*h_voice*：
[In] 语音通话句柄。

*service*：
[In] 呼叫转移配置。详见**第 2.3.14.1 章**枚举 `E_QL_MCM_VOICE_CALL_SERVICE_T`。

*reason*：
[In] 呼叫转移类型。详见**第 2.3.14.2 章** `E_QL_MCM_VOICE_CALL_FORWARDING_REASON_T`。

*number*：
[In] 被叫号码。

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败 |

##### 2.3.14.1 E_QL_MCM_VOICE_CALL_SERVICE_T

呼叫转移配置枚举类型定义如下：

```c
typedef enum {
    E_QL_MCM_VOICE_SERVICE_UNKOWN   = 0,
    E_QL_MCM_VOICE_SERVICE_REGISTER = 1,
    E_QL_MCM_VOICE_SERVICE_ERASE    = 2,
} E_QL_MCM_VOICE_CALL_SERVICE_T;
```

**成员**

| 成员 | 描述 |
|---|---|
| `E_QL_MCM_VOICE_SERVICE_UNKOWN` | 未知操作 |
| `E_QL_MCM_VOICE_SERVICE_REGISTER` | 注册呼叫转移 |
| `E_QL_MCM_VOICE_SERVICE_ERASE` | 删除呼叫转移号码 |

##### 2.3.14.2 E_QL_MCM_VOICE_CALL_FORWARDING_REASON_T

呼叫转移类型枚举定义如下：

```c
typedef enum {
    E_QL_MCM_VOICE_CALL_FORWARD_UNCONDITIONALLY = 0,
    E_QL_MCM_VOICE_CALL_FORWARD_MOBILEBUSY     = 1,
    E_QL_MCM_VOICE_CALL_FORWARD_NOREPLY        = 2,
    E_QL_MCM_VOICE_CALL_FORWARD_UNREACHABLE    = 3,
    E_QL_MCM_VOICE_CALL_FORWARD_ALLFORWARDING  = 4,
    E_QL_MCM_VOICE_CALL_FORWARD_ALLCONDITIONAL = 5,
} E_QL_MCM_VOICE_CALL_FORWARDING_REASON_T;
```

**成员**

| 成员 | 描述 |
|---|---|
| `E_QL_MCM_VOICE_CALL_FORWARD_UNCONDITIONALLY` | 无条件呼叫转移 |
| `E_QL_MCM_VOICE_CALL_FORWARD_MOBILEBUSY` | 被叫忙 |
| `E_QL_MCM_VOICE_CALL_FORWARD_NOREPLY` | 无响应 |
| `E_QL_MCM_VOICE_CALL_FORWARD_UNREACHABLE` | 无法到达 |
| `E_QL_MCM_VOICE_CALL_FORWARD_ALLFORWARDING` | 所有呼叫转移 |
| `E_QL_MCM_VOICE_CALL_FORWARD_ALLCONDITIONAL` | 所有条件呼叫转移 |

---

#### 2.3.15 QL_Voice_Call_GetForwardingStatus

该函数用于获取所有号码的呼叫转移模式状态。

**参数**

*status*（`QL_MCM_VOICE_CALL_FORWARDING_STATUS_T`）：
呼叫转移状态。枚举详见**第 2.3.15.2 章**。

*info_len*（`uint32_t`）：
`info` 数组的有效个数。

*info*（`QL_MCM_VOICE_CALL_FORWARDING_INFO_T`）：
呼叫转移信息。枚举详见**第 2.3.15.3 章**。

##### 2.3.15.2 QL_MCM_VOICE_CALL_FORWARDING_STATUS_T

呼叫转移状态枚举定义如下：

```c
typedef enum {
    E_QL_MCM_VOICE_CALL_FORWARDING_DISABLED = 0,
    E_QL_MCM_VOICE_CALL_FORWARDING_ENABLED  = 1,
} QL_MCM_VOICE_CALL_FORWARDING_STATUS_T;
```

**成员**

| 成员 | 描述 |
|---|---|
| `E_QL_MCM_VOICE_CALL_FORWARDING_DISABLED` | 呼叫转移已禁用 |
| `E_QL_MCM_VOICE_CALL_FORWARDING_ENABLED` | 呼叫转移已启用 |

##### 2.3.15.3 QL_MCM_VOICE_CALL_FORWARDING_INFO_T

呼叫转移信息结构体定义如下：

```c
typedef struct {
    QL_MCM_VOICE_CALL_FORWARDING_TYPE_T  type;
    char number[QL_MCM_MAX_PHONE_NUMBER + 1];
} QL_MCM_VOICE_CALL_FORWARDING_INFO_T;
```

**成员**

| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_MCM_VOICE_CALL_FORWARDING_TYPE_T` | `type` | 呼叫转移类型。详见**第 2.3.15.4 章** |
| `char` | `number` | 呼叫转移号码 |

##### 2.3.15.4 QL_MCM_VOICE_CALL_FORWARDING_TYPE_T

呼叫转移类型枚举定义如下：

```c
typedef enum {
    E_QL_MCM_VOICE_CALL_FORWARDING_TYPE_VOICE      = 0,
    E_QL_MCM_VOICE_CALL_FORWARDING_TYPE_DATA       = 1,
    E_QL_MCM_VOICE_CALL_FORWARDING_TYPE_VOICE_DATA = 2,
} QL_MCM_VOICE_CALL_FORWARDING_TYPE_T;
```

**成员**

| 成员 | 描述 |
|---|---|
| `E_QL_MCM_VOICE_CALL_FORWARDING_TYPE_VOICE` | 语音类型 |
| `E_QL_MCM_VOICE_CALL_FORWARDING_TYPE_DATA` | 数据类型 |
| `E_QL_MCM_VOICE_CALL_FORWARDING_TYPE_VOICE_DATA` | 语音和数据类型 |

---

## 3 语音服务使用步骤示例

语音通话的详细使用步骤请参考 `example/voice/example_voice.c`。

**步骤1**：首先，调用 `QL_Voice_Call_Client_Init()` 完成初始化。**必须首先执行初始化步骤方可进行下一步**。

**步骤2**：调用 `QL_Voice_Call_AddStateHandler()` 注册上报语音通话状态的回调函数。

**步骤3**：通信双方语音通话，完成通话（可以由任何一方挂断）。

**步骤4**：调用 `QL_Voice_Call_RemoveStateHandler()` 销毁注册上报电话状态的回调函数。

**步骤5**：调用 `QL_Voice_Call_Client_Deinit()` 销毁语音通话功能资源。

> **注意**：上述步骤中，**步骤 4 和步骤 5 在退出程序前执行**。

---

## 4 编译说明

编译单个 `example_voice.c` 步骤如下：

**步骤1**：执行命令 `tar -jxvf ql-ol-sdk.tar.bz2` 解压 `ql-ol-sdk.tar.bz2`。

**步骤2**：执行命令 `cd ql-ol-sdk` 进入 `ql-ol-sdk` 目录。

**步骤3**：执行命令 `source ql-ol-crosstool/ql-ol-crosstool-env-init` 初始化编译环境。此步骤是为了确保 SDK 版本与模块固件版本一致，否则可能出现错误。

**步骤4**：执行命令 `cd ql-ol-extsdk/example/voice`。

**步骤5**：执行 `make clean;make` 清除编译结果并重新编译。

```bash
tar -jxvf ql-ol-sdk.tar.bz2
cd ql-ol-sdk
source ql-ol-crosstool/ql-ol-crosstool-env-init
cd ql-ol-extsdk/example/voice
make clean;make
```

---

## 5 语音服务演示步骤

执行如下命令，可以开始进行语音通话演示：

```bash
root@mdm9607-perf:~# ./example_voice
```

示例程序支持的测试用例（命令索引）：

| 索引 | 操作 |
|---|---|
| 0 | print_help（打印帮助信息） |
| 1 | QL_Voice_Call_Start |
| 2 | QL_Voice_Call_End |
| 3 | QL_Voice_Call_Answer |
| 4 | QL_Voice_Call_CancelDial |
| 5 | QL_Voice_Call_Dtmf |
| 6 | QL_Voice_Call_SetForwarding |
| 7 | QL_Voice_Call_GetForwardingStatus |
| 8 | QL_Voice_Call_SetWaiting |
| 9 | QL_Voice_Call_GetWaitingStatus |
| 10 | QL_Voice_Call_Hold |
| 11 | QL_Voice_Call_UnHold |

### 5.1 拨打电话

```
please input cmd index(-1 exit): 1
please input dest phone number:
12100000000
QL_Voice_Call_Start ret = 0, with voice_call_id=1
// 下面此类 log 为电话状态变化
please input cmd index(-1 exit): ######### Call id=1, PhoneNum:12100000000, event=DIALING!   ###### //发起
######### Call id=1, PhoneNum:12100000000, event=ALERTING!   ###### //对方振铃
######### Call id=1, PhoneNum:12100000000, event=ACTIVE!     ######//电话建立连接
######### Call id=1, PhoneNum:12100000000, event=END!        ######//电话结束
0
```

### 5.2 接听并挂断电话

```
please input cmd index(-1 exit): ######### Call id=1, PhoneNum:12100000000, event=INCOMING! ######
3
 please input answer call id
1
######### Call id=1, PhoneNum:12100000000, event=ACTIVE!   ######
 ret = 0

please input cmd index(-1 exit): ######### Call id=1, PhoneNum:12100000000, event=INCOMING! ######
3
 please input answer call id
1
######### Call id=1, PhoneNum:12100000000, event=ACTIVE!   ######
 ret = 0

//挂断电话
please input cmd index(-1 exit): 2
please input end call id:
1
######### Call id=1, PhoneNum:12100000000, event=END!   ######
 ret = 0
```

### 5.3 保持呼叫

主叫 A（开通了呼叫等待业务）拨打 B（12100000000），并且 B 接听了电话，此时调用 `QL_Voice_Call_Hold` 可以让 B 处于呼叫保持状态，再次调用 `QL_Voice_Call_Hold` 可以恢复与 B 的通话。

```
//呼叫保持，挂起当前通话
please input cmd index(-1 exit): 10
######### Call id=1, PhoneNum:12100000000, event=HOLDING!   ######
 QL_Voice_Call_Hold ret = 0

//恢复保持
please input cmd index(-1 exit): 11
######### Call id=1, PhoneNum:12100000000, event=ACTIVE!   ######
 QL_Voice_Call_UnHold ret = 0

please input cmd index(-1 exit): ######### Call id=1, PhoneNum:12100000000, event=END! ######
```

---

## 6 附录 参考文档及术语缩写

### 参考文档（表 3）

| 编号 | 文档名称 |
|---|---|
| [1] | EC2x&EG9x&EG2x-G&EM05 系列_QuecOpen_快速开发指导 |

### 术语缩写（表 4）

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| API | Application Programming Interface | 应用程序编程接口 |
| ASCII | American Standard Code for Information Interchange | 美国信息交换标准代码 |
| DTMF | Dual-Tone Multifrequency | 双音多频 |
| ID | Mostly refers to Identifier in terms of software | 软件中多数指"标识符" |
| IoT | Internet of Things | 物联网 |
| SDK | Software Development Kit | 软件开发工具包 |
