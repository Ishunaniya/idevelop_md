# Quectel EC2x&EG2x-G&EG9x&EM05 系列 QuecOpen 设备管理 API 参考手册分析

> 原始文档：Quectel_EC2x&EG2x-G&EG9x&EM05系列_QuecOpen_设备管理API_参考手册_V1.0.pdf  
> 文档版本：V1.0  
> 文档日期：2023-03-09  
> 分析日期：2026-05-28

---

## 1. 文档概述

### 1.1 手册目的

本手册介绍移远通信（Quectel）LTE Standard 模块系列在 QuecOpen 平台下的**设备管理（Device Management, DM）API** 的使用方法和功能。DM API 用于：
- 模块管理
- 获取模块的状态与信息
- 设置模块参数（如飞行模式）
- 获取设备标识信息（IMEI/MEID）
- 获取固件版本信息

### 1.2 适用模块型号

| 模块系列 | 具体模块 |
|---------|---------|
| EC2x   | EC21 系列、EC25 系列、EC20-CE |
| EG2x-G | EG21-G、EG25-G |
| EG9x   | EG91 系列、EG95 系列 |
| EM05   | EM05 系列 |

### 1.3 QuecOpen 版本说明

QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。所有 DM API 均在 QuecOpen SDK 中提供。

### 1.4 章节结构总览

| 章节 | 内容 |
|-----|------|
| 第1章 引言 | 适用模块列表 |
| 第2章 设备管理 API | 头文件、函数概览、函数详解（原型、参数、返回值） |
| 第3章 DM 后台程序使用示例 | 初始化、飞行模式控制、IMEI/MEID 获取、固件版本获取 |
| 第4章 附录 术语缩写 | API、DM、IMEI、MEID、SDK 等缩写说明 |

---

## 2. 设备管理 API 详解

### 2.1 头文件

```c
#include "ql_mcm_dm.h"
```

头文件路径：`ql-ol-sdk/ql-ol-extsdk/include/ql_mcm_dm.h`

> **注意**：若无特别说明，本文档所提及的头文件均位于该目录下。

### 2.2 函数概览

| 函数名 | 功能说明 |
|-------|---------|
| `QL_MCM_DM_Client_Init()` | 初始化设备管理服务 |
| `QL_MCM_DM_AddRxIndMsgHandler()` | 设置飞行模式变化后触发的回调函数 |
| `QL_MCM_DM_SetAirplaneModeChgInd()` | 设置飞行模式发生变化后触发回调函数执行的开关 |
| `QL_MCM_DM_GetAirplaneMode()` | 获取模块的飞行模式 |
| `QL_MCM_DM_SetAirplaneMode()` | 设置模块的飞行模式 |
| `QL_MCM_DM_Client_Deinit()` | 注销设备管理服务 |
| `QL_MCM_DM_GetSerialNumbers()` | 获取模块的 IMEI 和 MEID 信息 |
| `QL_MCM_DM_GetFirmware()` | 获取模块的固件版本信息 |

> **重要限制**：若无特殊说明，所有设备管理 API 函数均**不支持并发调用**。

### 2.3 函数详解

---

### `QL_MCM_DM_Client_Init`

#### 功能描述

初始化设备管理（DM）服务，获取服务句柄，为后续所有 DM API 调用做准备。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_DM_Client_Init(dm_client_handle_type *ph_dm)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `ph_dm` | `dm_client_handle_type *` | 输出（Out） | 服务句柄指针，初始化成功后存储有效句柄值，后续 API 调用需传入此句柄 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_mcm.h` |

#### 使用注意事项

- 程序启动后必须首先调用此函数，才能使用其他 DM API。
- 退出程序时需调用 `QL_MCM_DM_Client_Deinit()` 注销服务。

---

### `QL_MCM_DM_AddRxIndMsgHandler`

#### 功能描述

设置因飞行模式切换到其他模式后而触发的回调函数（Callback Function）。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_DM_AddRxIndMsgHandler(
    QL_MCM_DM_RxIndMsgHandlerFunc_t handlerPtr,
    void* contextPtr
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `handlerPtr` | `QL_MCM_DM_RxIndMsgHandlerFunc_t` | 输入（In） | 飞行模式切换到其他模式后触发的回调函数指针，详见 `QL_MCM_DM_RxIndMsgHandlerFunc_t` 类型定义 |
| `contextPtr` | `void*` | 输入（In） | 服务句柄 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_mcm.h` |

---

### `QL_MCM_DM_RxIndMsgHandlerFunc_t`（回调函数类型）

#### 功能描述

该函数类型定义为因飞行模式切换到其他模式后触发的回调函数签名。

#### 函数原型（typedef 定义）

```c
typedef void (*QL_MCM_DM_RxIndMsgHandlerFunc_t)(
    dm_client_handle_type   h_dm,
    E_QL_MCM_DM_NFY_MSG_ID_T e_msg_id,
    void                    *pv_data,
    void                    *contextPtr
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_dm` | `dm_client_handle_type` | 输入（In） | 服务句柄 |
| `e_msg_id` | `E_QL_MCM_DM_NFY_MSG_ID_T` | 输入（In） | 触发回调函数的事件 ID，详见 `E_QL_MCM_DM_NFY_MSG_ID_T` 枚举 |
| `pv_data` | `void *` | 输入（In） | 触发回调函数的事件内容数据指针 |
| `contextPtr` | `void *` | 输入（In） | 保留，暂未使用 |

#### 返回值

无（`void`）

---

### `E_QL_MCM_DM_NFY_MSG_ID_T`（枚举类型）

#### 功能描述

触发回调函数的事件 ID 枚举定义。

#### 枚举定义

```c
typedef enum
{
    E_QL_MCM_DM_RADIO_MODE_CHANGED_EVENT = 0,
} E_QL_MCM_DM_NFY_MSG_ID_T;
```

#### 枚举成员说明

| 成员 | 值 | 描述 |
|-----|---|------|
| `E_QL_MCM_DM_RADIO_MODE_CHANGED_EVENT` | 0 | 飞行模式变化触发的事件 ID |

---

### `QL_MCM_DM_SetAirplaneModeChgInd`

#### 功能描述

设置飞行模式发生变化后触发回调函数执行的开关（使能/禁止回调通知）。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_DM_SetAirplaneModeChgInd(
    dm_client_handle_type h_dm,
    uint32_t              ind_onoff
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 取值范围 | 说明 |
|-------|------|------|---------|------|
| `h_dm` | `dm_client_handle_type` | 输入（In） | — | 服务句柄 |
| `ind_onoff` | `uint32_t` | 输入（In） | 0 或 1 | 触发回调函数执行的开关：0 表示关闭，1 表示开启 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_mcm.h` |

---

### `QL_MCM_DM_GetAirplaneMode`

#### 功能描述

获取模块当前的飞行模式（Airplane Mode）状态。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_DM_GetAirplaneMode(
    dm_client_handle_type          h_dm,
    E_QL_MCM_DM_AIRPLANE_MODE_TYPE_T *pe_airplane_mode
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_dm` | `dm_client_handle_type` | 输入（In） | 服务句柄 |
| `pe_airplane_mode` | `E_QL_MCM_DM_AIRPLANE_MODE_TYPE_T *` | 输出（Out） | 飞行模式信息指针，详见 `E_QL_MCM_DM_AIRPLANE_MODE_TYPE_T` 枚举 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_mcm.h` |

---

### `E_QL_MCM_DM_AIRPLANE_MODE_TYPE_T`（枚举类型）

#### 功能描述

飞行模式信息枚举定义。

#### 枚举定义

```c
typedef enum
{
    E_QL_MCM_DM_AIRPLANE_MODE_UNKNOWN = 0,  /**< Radio online. */
    E_QL_MCM_DM_AIRPLANE_MODE_ON      = 1,  /**< Radio power off or unknown. Airplane ON. */
    E_QL_MCM_DM_AIRPLANE_MODE_OFF     = 2,  /**< Radio online. Airplane OFF. */
    E_QL_MCM_DM_AIRPLANE_MODE_NA      = 3   /**< Radio Unvailable. */
} E_QL_MCM_DM_AIRPLANE_MODE_TYPE_T;
```

#### 枚举成员说明

| 成员 | 值 | 描述 |
|-----|---|------|
| `E_QL_MCM_DM_AIRPLANE_MODE_UNKNOWN` | 0 | 未知的飞行模式（Radio 在线） |
| `E_QL_MCM_DM_AIRPLANE_MODE_ON` | 1 | 飞行模式开启（Radio 断电或状态未知，飞行模式 ON） |
| `E_QL_MCM_DM_AIRPLANE_MODE_OFF` | 2 | 飞行模式关闭（Radio 在线，飞行模式 OFF） |
| `E_QL_MCM_DM_AIRPLANE_MODE_NA` | 3 | 飞行模式不可用（Radio 不可用） |

---

### `QL_MCM_DM_SetAirplaneMode`

#### 功能描述

设置模块的飞行模式（开启或关闭飞行模式）。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_DM_SetAirplaneMode(
    dm_client_handle_type          h_dm,
    E_QL_MCM_DM_AIRPLANE_MODE_TYPE_T e_airplane_mode
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_dm` | `dm_client_handle_type` | 输入（In） | 服务句柄 |
| `e_airplane_mode` | `E_QL_MCM_DM_AIRPLANE_MODE_TYPE_T` | 输入（In） | 要设置的飞行模式，详见 `E_QL_MCM_DM_AIRPLANE_MODE_TYPE_T` 枚举；传入 `E_QL_MCM_DM_AIRPLANE_MODE_ON`(1) 开启，`E_QL_MCM_DM_AIRPLANE_MODE_OFF`(2) 关闭 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_mcm.h` |

---

### `QL_MCM_DM_Client_Deinit`

#### 功能描述

注销设备管理（DM）服务，释放服务句柄资源。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_DM_Client_Deinit(dm_client_handle_type h_dm)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_dm` | `dm_client_handle_type` | 输入（In） | 服务句柄（由 `QL_MCM_DM_Client_Init()` 返回） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_mcm.h` |

#### 使用注意事项

- 退出程序时必须调用此函数，与 `QL_MCM_DM_Client_Init()` 成对使用。

---

### `QL_MCM_DM_GetSerialNumbers`

#### 功能描述

获取模块的 IMEI（国际移动设备识别码）和 MEID（移动设备识别码）信息。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_DM_GetSerialNumbers(
    dm_client_handle_type          h_dm,
    ql_dm_device_serial_numbers_t *serial_numbers
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_dm` | `dm_client_handle_type` | 输入（In） | 服务句柄 |
| `serial_numbers` | `ql_dm_device_serial_numbers_t *` | 输出（Out） | 模块 IMEI 和 MEID 信息结构体指针，详见 `ql_dm_device_serial_numbers_t` 结构体定义 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_mcm.h` |

---

### `ql_dm_device_serial_numbers_t`（数据结构）

#### 功能描述

模块 IMEI 和 MEID 信息结构体定义。

#### 结构体定义

```c
typedef struct
{
    uint8_t imei_valid;
    char    imei[QL_MAX_DMS_IMEI_LEN + 1];
    uint8_t meid_valid;
    char    meid[QL_MAX_DMS_MEID_LEN + 1];
} ql_dm_device_serial_numbers_t;
```

#### 字段说明

| 类型 | 字段名 | 说明 |
|-----|-------|------|
| `uint8_t` | `imei_valid` | 标记返回的 IMEI 是否有效（非零表示有效） |
| `char[]` | `imei` | IMEI 字符串，长度为 `QL_MAX_DMS_IMEI_LEN + 1`（含结尾 `\0`） |
| `uint8_t` | `meid_valid` | 标记返回的 MEID 是否有效（非零表示有效） |
| `char[]` | `meid` | MEID 字符串，长度为 `QL_MAX_DMS_MEID_LEN + 1`（含结尾 `\0`） |

#### 注意事项

- 若 MEID 为空，请联系移远通信技术支持。

---

### `QL_MCM_DM_GetFirmware`

#### 功能描述

获取模块的固件（Firmware）版本信息。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_DM_GetFirmware(
    dm_client_handle_type h_dm,
    ql_dm_device_rev_id_t *hardware_rev
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_dm` | `dm_client_handle_type` | 输入（In） | 服务句柄 |
| `hardware_rev` | `ql_dm_device_rev_id_t *` | 输出（Out） | 固件版本信息结构体指针，详见 `ql_dm_device_rev_id_t` 结构体定义 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_mcm.h` |

---

### `ql_dm_device_rev_id_t`（数据结构）

#### 功能描述

固件版本信息结构体定义。

#### 结构体定义

```c
typedef struct
{
    char device_rev_id;
} ql_dm_device_rev_id_t;
```

#### 字段说明

| 类型 | 字段名 | 说明 |
|-----|-------|------|
| `char` | `device_rev_id` | 模块固件版本信息字符串（如 `EG25GGBR07A07M2G`） |

---

## 3. 数据类型汇总

### 3.1 枚举类型

| 枚举类型名 | 用途 |
|-----------|------|
| `E_QL_MCM_DM_NFY_MSG_ID_T` | 触发回调的事件 ID（当前仅有飞行模式变化事件） |
| `E_QL_MCM_DM_AIRPLANE_MODE_TYPE_T` | 飞行模式状态（UNKNOWN/ON/OFF/NA） |

### 3.2 结构体类型

| 结构体类型名 | 用途 |
|-----------|------|
| `ql_dm_device_serial_numbers_t` | 存储 IMEI 和 MEID 信息 |
| `ql_dm_device_rev_id_t` | 存储固件版本信息 |

### 3.3 回调函数类型

| 类型名 | 用途 |
|-------|------|
| `QL_MCM_DM_RxIndMsgHandlerFunc_t` | 飞行模式变化事件回调函数签名 |

---

## 4. 使用流程与示例代码

### 4.1 DM API 总体使用流程

```
程序启动
    │
    ▼
QL_MCM_DM_Client_Init()       ← 初始化 DM 服务，获取句柄
    │
    ▼
QL_MCM_DM_AddRxIndMsgHandler() ← （可选）注册飞行模式变化回调
    │
    ▼
QL_MCM_DM_SetAirplaneModeChgInd() ← （可选）开启飞行模式变化通知
    │
    ▼
业务操作（获取/设置飞行模式、IMEI/MEID、固件版本等）
    │
    ▼
QL_MCM_DM_Client_Deinit()     ← 注销 DM 服务
    │
    ▼
程序退出
```

### 4.2 示例程序执行环境

示例代码文件：`test_dm.c`  
示例文件路径：`ql-ol-sdk/ql-ol-extsdk/example/test_mcm_api/`  
编译后可执行文件：`test_api_all`  
运行命令：`/usrdata # ./test_api_all`

运行后选择测试组：
```
Test groups:
0:  mcm_atcop
1:  mcm_dm       ← 选择 1 进入 DM 测试
2:  mcm_gps
3:  mcm_nw
4:  mcm_sim
5:  mcm_sms
6:  mcm_voice
please input command index(-1 exit): 1
```

DM 组支持的测试用例：
```
Group Name:mcm_dm, Supported test cases:
0:  QL_MCM_DM_Client_Init
1:  QL_MCM_DM_AddRxIndMsgHandler
2:  QL_MCM_DM_SetAirplaneModeChgInd
3:  QL_MCM_DM_GetAirplaneMode
4:  QL_MCM_DM_SetAirplaneMode
5:  QL_MCM_DM_Client_Deinit
6:  QL_MCM_DM_GetSerialNumbers
7:  QL_MCM_DM_GetFirmware
```

### 4.3 初始化设备管理服务

```c
dm_client_handle_type h_dm;

// 初始化 DM 服务
E_QL_ERROR_CODE_T ret = QL_MCM_DM_Client_Init(&h_dm);
// 执行成功返回：QL_MCM_DM_Client_Init ret = 0 with h_dm = 1
if (ret != E_QL_OK) {
    // 初始化失败处理
}
```

**运行结果示例：**
```
please input cmd index(-1 exit): 0
1033
QL MCM DM Client Init ret = 0 with h_dm= 1
```

### 4.4 开启飞行模式

```c
// 设置飞行模式为 ON（1）
E_QL_ERROR_CODE_T ret = QL_MCM_DM_SetAirplaneMode(h_dm, E_QL_MCM_DM_AIRPLANE_MODE_ON);
// QL_MCM_DM_SetAirplaneMode ret = 0

// 验证：获取当前飞行模式
E_QL_MCM_DM_AIRPLANE_MODE_TYPE_T mode;
ret = QL_MCM_DM_GetAirplaneMode(h_dm, &mode);
// QL_MCM_DM_GetAirplaneMode ret = 0, e_airplane_mode= 1  (ON)
```

**运行结果示例：**
```
please input cmd index(-1 exit): 4
please input airplane mode(1: ON, 2: OFF): 1
QL_MCM_DM_SetAirplaneMode ret = 0
please input cmd index(-1 exit): 3
QL_MCM_DM_GetAirplaneMode ret = 0, e_airplane_mode= 1
```

### 4.5 关闭飞行模式

```c
// 设置飞行模式为 OFF（2）
E_QL_ERROR_CODE_T ret = QL_MCM_DM_SetAirplaneMode(h_dm, E_QL_MCM_DM_AIRPLANE_MODE_OFF);
// QL_MCM_DM_SetAirplaneMode ret = 0

// 验证：获取当前飞行模式
E_QL_MCM_DM_AIRPLANE_MODE_TYPE_T mode;
ret = QL_MCM_DM_GetAirplaneMode(h_dm, &mode);
// QL_MCM_DM_GetAirplaneMode ret = 0, e_airplane_mode= 2  (OFF)
```

**运行结果示例：**
```
please input cmd index(-1 exit): 4
please input airplane mode(1: ON, 2: OFF): 2
QL_MCM_DM_SetAirplaneMode ret = 0
please input cmd index(-1 exit): 3
QL_MCM_DM_GetAirplaneMode ret = 0, e_airplane_mode= 2
```

### 4.6 飞行模式变化通知（回调机制）

```c
// 步骤1：注册回调函数
void my_dm_callback(
    dm_client_handle_type    h_dm,
    E_QL_MCM_DM_NFY_MSG_ID_T e_msg_id,
    void                     *pv_data,
    void                     *contextPtr)
{
    if (e_msg_id == E_QL_MCM_DM_RADIO_MODE_CHANGED_EVENT) {
        printf("### airplane mode changed ###\n");
        // 处理飞行模式变化事件
    }
}

// 步骤2：注册回调
QL_MCM_DM_AddRxIndMsgHandler(my_dm_callback, (void*)h_dm);

// 步骤3：开启通知（ind_onoff = 1 表示开启）
QL_MCM_DM_SetAirplaneModeChgInd(h_dm, 1);

// 之后当飞行模式改变时，回调自动触发：
// ### airplane mode changed to ON ###
```

**运行结果示例：**
```
please input cmd index(-1 exit): 1   ← 设置回调函数
QL_MCM_DM_AddRxIndMsgHandler ret = 0
please input cmd index(-1 exit): 2   ← 打开飞行模式变化通知，触发回调
please input airplane mode change indication ON/OFF(0: OFF, 1: ON): 1
QL_MCM_DM_SetAirplaneModeChgInd ret = 0
...
QL_MCM_DM_SetAirplaneMode ret = 0
### airplane mode changed to ON ###   ← 触发回调函数执行
```

### 4.7 获取 IMEI 和 MEID

```c
ql_dm_device_serial_numbers_t serial_numbers;
E_QL_ERROR_CODE_T ret = QL_MCM_DM_GetSerialNumbers(h_dm, &serial_numbers);

if (ret == E_QL_OK) {
    if (serial_numbers.imei_valid) {
        printf("IMEI: %s\n", serial_numbers.imei);
    }
    if (serial_numbers.meid_valid) {
        printf("MEID: %s\n", serial_numbers.meid);
    }
}
```

**运行结果示例：**
```
please input cmd index(-1 exit): 6
QL_MCM_DM_GetSerialNumbers ret = 0
SerialNumbers  imei:864430010001091    meid:00001234567890
```

### 4.8 获取固件版本号

```c
ql_dm_device_rev_id_t hardware_rev;
E_QL_ERROR_CODE_T ret = QL_MCM_DM_GetFirmware(h_dm, &hardware_rev);

if (ret == E_QL_OK) {
    printf("Firmware: %s\n", &hardware_rev.device_rev_id);
}
```

**运行结果示例：**
```
please input cmd index(-1 exit): 7
QL_MCM_DM_GetFirmware ret = 0
hardware revision identification :EG25GGBR07A07M2G
```

---

## 5. 错误码说明

本文档中所有 API 函数的返回值类型均为 `E_QL_ERROR_CODE_T`，其具体错误码定义在头文件 `ql_mcm.h` 中。

| 返回值 | 含义 |
|-------|------|
| `E_QL_OK` | 执行成功（通常为 0） |
| 其他值 | 执行失败，需参考 `ql_mcm.h` 中的错误码定义 |

---

## 6. 注意事项与限制

1. **并发调用限制**：若无特殊说明，所有设备管理 API 函数均**不支持并发调用**，需在单线程环境中依次调用。

2. **生命周期管理**：
   - 必须首先调用 `QL_MCM_DM_Client_Init()` 初始化服务
   - 程序退出前必须调用 `QL_MCM_DM_Client_Deinit()` 注销服务
   - 两者必须成对使用

3. **MEID 为空的情况**：若调用 `QL_MCM_DM_GetSerialNumbers()` 后 MEID 为空，请联系移远通信技术支持。

4. **飞行模式设置**：
   - 设置飞行模式时传入枚举值 `E_QL_MCM_DM_AIRPLANE_MODE_ON`(1) 开启
   - 传入 `E_QL_MCM_DM_AIRPLANE_MODE_OFF`(2) 关闭
   - 不建议传入 UNKNOWN(0) 或 NA(3)

5. **回调机制**：若需要监听飞行模式变化事件，需依次执行：
   - 注册回调函数（`QL_MCM_DM_AddRxIndMsgHandler`）
   - 开启通知开关（`QL_MCM_DM_SetAirplaneModeChgInd`，传入 1）

---

## 7. 版本与兼容性信息

| 项目 | 信息 |
|-----|------|
| 文档版本 | V1.0 |
| 文档日期 | 2023-03-09 |
| 文档状态 | 受控文件 |
| 作者 | Colin CUI |
| 适用模块系列 | EC2x（EC21/EC25/EC20-CE）、EG2x-G（EG21-G/EG25-G）、EG9x（EG91/EG95）、EM05 |
| 头文件 | `ql_mcm_dm.h` |
| 头文件路径 | `ql-ol-sdk/ql-ol-extsdk/include/` |
| 示例代码路径 | `ql-ol-sdk/ql-ol-extsdk/example/test_mcm_api/test_dm.c` |

---

## 8. 附录：术语缩写

| 缩写 | 英文全称 | 中文全称 |
|-----|---------|---------|
| API | Application Programming Interface | 应用程序接口 |
| CPU | Central Processing Unit | 中央处理器 |
| DM | Device Management | 设备管理 |
| IMEI | International Mobile Equipment Identifier | 国际移动设备识别码 |
| MEID | Mobile Equipment Identifier | 移动设备识别码 |
| SDK | Software Development Kit | 软件开发包 |

<!-- GENERATION_COMPLETE: 2026-05-28 -->
