# Quectel EC2x&EG9x&EG25-G 系列 QuecOpen (U)SIM API 参考手册分析

> 原始文档：Quectel_EC2x&EG9x&EG25-G系列_QuecOpen_(U)SIM_API_参考手册_V1.0.pdf  
> 文档版本：V1.0  
> 文档日期：2023-06-30  
> 分析日期：2026-05-28

---

## 1. 文档概述

### 1.1 手册目的

本手册介绍移远通信（Quectel）EC2x 系列、EG9x 系列和 EG25-G 模块在 QuecOpen 平台下的 **(U)SIM API**（通用用户识别模块 API）的使用方法和功能。(U)SIM API 主要用于：
- 初始化/注销 SIM 卡服务
- 获取 SIM 卡信息（IMSI、ICCID、手机号码、运营商列表）
- PIN 码管理（使能、禁用、验证、更改、解锁）
- 获取 SIM 卡状态
- SIM 卡个性化（Personalization）设置与解锁
- 读写 SIM 卡文件
- 读写 SIM 卡电话簿

### 1.2 适用模块型号

| 模块系列 | 具体模块 |
|---------|---------|
| EC2x   | EC20-CE、EC20-CN、EC21 系列、EC25 系列 |
| EG9x   | EG91 系列、EG95 系列 |
| EG25-G | EG25-G |

### 1.3 QuecOpen 版本说明

QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。更多 QuecOpen 信息参见文档 《Quectel_EC2x&EG2x-G&EG9x 系列_QuecOpen_快速开发指导》。

### 1.4 章节结构总览

| 章节 | 内容 |
|-----|------|
| 第1章 引言 | 适用模块列表 |
| 第2章 (U)SIM API | 头文件、函数概览、函数详解（含所有数据结构、枚举） |
| 第3章 示例 | 示例代码文件位置说明 |
| 第4章 附录 | 参考文档、术语缩写表 |

---

## 2. (U)SIM API 详解

### 2.1 头文件

```c
#include "ql_mcm_sim.h"
```

头文件路径：`ql-ol-sdk/ql-ol-extsdk/include/ql_mcm_sim.h`

> **注意**：若无特别说明，本文档所提及的头文件均位于该目录下。

### 2.2 函数概览

| 函数名 | 功能说明 |
|-------|---------|
| `QL_MCM_SIM_Client_Init()` | 初始化(U)SIM 卡服务并获取句柄 |
| `QL_MCM_SIM_Client_Deinit()` | 注销(U)SIM 卡服务 |
| `QL_MCM_SIM_GetIMSI()` | 获取(U)SIM 卡的 IMSI |
| `QL_MCM_SIM_GetICCID()` | 获取(U)SIM 卡的 ICCID |
| `QL_MCM_SIM_GetPhoneNumber()` | 获取(U)SIM 卡本机号码 |
| `QL_MCM_SIM_GetOperatorPlmnList()` | 获取运营商列表 |
| `QL_MCM_SIM_EnablePIN()` | 使能 PIN 码验证 |
| `QL_MCM_SIM_DisablePIN()` | 禁用 PIN 码验证 |
| `QL_MCM_SIM_VerifyPIN()` | 验证 PIN 码 |
| `QL_MCM_SIM_ChangePin()` | 更改 PIN 码 |
| `QL_MCM_SIM_UnblockPIN()` | 3 次错误输入 PIN 码后，使用 PUK 码和新 PIN 码解锁 |
| `QL_MCM_SIM_GetCardStatus()` | 获取(U)SIM 卡状态 |
| `QL_MCM_SIM_Personalization()` | 激活和设置个性化数据 |
| `QL_MCM_SIM_Depersonalization()` | 关闭或解除个性化数据 |
| `QL_MCM_SIM_WriteFile()` | 将数据写入(U)SIM 卡文件 |
| `QL_MCM_SIM_ReadFile()` | 从(U)SIM 卡文件读取数据 |
| `QL_MCM_SIM_GetFileSize()` | 获取(U)SIM 卡文件大小 |
| `QL_MCM_SIM_ReadPhoneBookRecord()` | 读取(U)SIM 卡电话簿中的电话号码记录 |
| `QL_MCM_SIM_WritePhoneBookRecord()` | 将电话号码记录写入(U)SIM 卡电话簿 |

> **重要限制**：若无特别说明，本文档所述函数均**不支持并发调用**，并且**不能在相关回调函数中调用**以上函数。

### 2.3 函数详解

---

### `QL_MCM_SIM_Client_Init`

#### 功能描述

初始化(U)SIM 卡服务，获取服务句柄，为后续所有 SIM API 调用做准备。

#### 函数原型

```c
int QL_MCM_SIM_Client_Init(sim_client_handle_type *ph_sim)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `ph_sim` | `sim_client_handle_type *` | 输出（Out） | 指向(U)SIM 卡服务句柄的指针，初始化成功后存储有效句柄 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

#### 注意事项

- 使用其他(U)SIM 卡接口函数前，**必须**调用本函数初始化(U)SIM 卡服务获取句柄。

---

### `sim_client_handle_type`（句柄类型）

```c
typedef uint32 sim_client_handle_type;
```

| 类型 | 参数 | 描述 |
|-----|-----|------|
| `uint32` | `sim_client_handle_type` | (U)SIM 卡服务句柄 |

---

### `QL_MCM_SIM_Client_Deinit`

#### 功能描述

注销(U)SIM 卡服务，释放资源。

#### 函数原型

```c
int QL_MCM_SIM_Client_Deinit(sim_client_handle_type h_sim)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄（由 `QL_MCM_SIM_Client_Init` 返回） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

#### 注意事项

- 若不再使用(U)SIM 卡服务，**必须**调用本函数注销(U)SIM 卡服务以释放资源。

---

### `QL_MCM_SIM_GetIMSI`

#### 功能描述

获取(U)SIM 卡的 IMSI（国际移动用户识别码）。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_GetIMSI(
    sim_client_handle_type h_sim,
    QL_SIM_APP_ID_INFO_T   *pt_info,
    char                   *imsi,
    size_t                 imsiLen
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_SIM_APP_ID_INFO_T *` | 输入（In） | (U)SIM 卡应用程序标识信息，指定卡槽和应用类型 |
| `imsi` | `char *` | 输出（Out） | IMSI 缓冲区，用于存放获取的 IMSI |
| `imsiLen` | `size_t` | 输入（In） | IMSI 缓冲区长度（字节） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_SIM_APP_ID_INFO_T`（数据结构）

(U)SIM 卡应用程序标识信息结构体，用于指定卡槽和应用类型。

```c
typedef struct
{
    E_QL_MCM_SIM_SLOT_ID_TYPE_T e_slot_id;
    E_QL_MCM_SIM_APP_TYPE_T     e_app;
} QL_SIM_APP_ID_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `E_QL_MCM_SIM_SLOT_ID_TYPE_T` | `e_slot_id` | (U)SIM 卡卡槽编号 |
| `E_QL_MCM_SIM_APP_TYPE_T` | `e_app` | (U)SIM 卡应用程序类型 |

---

### `E_QL_MCM_SIM_SLOT_ID_TYPE_T`（枚举）

(U)SIM 卡卡槽编号枚举。

```c
typedef enum
{
    E_QL_MCM_SIM_SLOT_ID_1 = 0xB01,
    E_QL_MCM_SIM_SLOT_ID_2 = 0xB02,
} E_QL_MCM_SIM_SLOT_ID_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `E_QL_MCM_SIM_SLOT_ID_1` | 0xB01 | 卡槽 1 |
| `E_QL_MCM_SIM_SLOT_ID_2` | 0xB02 | 卡槽 2 |

---

### `E_QL_MCM_SIM_APP_TYPE_T`（枚举）

(U)SIM 卡应用程序类型枚举。

```c
typedef enum
{
    E_QL_MCM_SIM_APP_TYPE_UNKNOWN = 0xB00,
    E_QL_MCM_SIM_APP_TYPE_3GPP    = 0xB01,
    E_QL_MCM_SIM_APP_TYPE_3GPP2   = 0xB02,
    E_QL_MCM_SIM_APP_TYPE_ISIM    = 0xB03,
} E_QL_MCM_SIM_APP_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `E_QL_MCM_SIM_APP_TYPE_UNKNOWN` | 0xB00 | 未知应用类型 |
| `E_QL_MCM_SIM_APP_TYPE_3GPP` | 0xB01 | 3GPP 类型（SIM/USIM） |
| `E_QL_MCM_SIM_APP_TYPE_3GPP2` | 0xB02 | 3GPP2 类型（RUIM/CSIM） |
| `E_QL_MCM_SIM_APP_TYPE_ISIM` | 0xB03 | ISIM 专用应用程序 |

---

### `QL_MCM_SIM_GetICCID`

#### 功能描述

获取(U)SIM 卡的 ICCID（集成电路卡识别码）。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_GetICCID(
    sim_client_handle_type      h_sim,
    E_QL_MCM_SIM_SLOT_ID_TYPE_T simId,
    char                        *iccid,
    size_t                      iccidLen
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `simId` | `E_QL_MCM_SIM_SLOT_ID_TYPE_T` | 输入（In） | (U)SIM 卡卡槽编号 |
| `iccid` | `char *` | 输出（Out） | ICCID 缓冲区，用于存放获取的 ICCID |
| `iccidLen` | `size_t` | 输入（In） | ICCID 缓冲区长度（字节） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_MCM_SIM_GetPhoneNumber`

#### 功能描述

获取(U)SIM 卡本机号码（MDN/MSISDN）。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_GetPhoneNumber(
    sim_client_handle_type h_sim,
    QL_SIM_APP_ID_INFO_T   *pt_info,
    char                   *phone_num,
    size_t                 phoneLen
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_SIM_APP_ID_INFO_T *` | 输入（In） | (U)SIM 卡应用程序标识信息 |
| `phone_num` | `char *` | 输出（Out） | 缓冲区，用于存放获取的电话号码 |
| `phoneLen` | `size_t` | 输入（In） | `phone_num` 缓冲区长度（字节） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_MCM_SIM_GetOperatorPlmnList`

#### 功能描述

获取(U)SIM 卡中存储的首选运营商列表（PLMN List）。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_GetOperatorPlmnList(
    sim_client_handle_type            h_sim,
    E_QL_MCM_SIM_SLOT_ID_TYPE_T       simId,
    QL_SIM_PREFERRED_OPERATOR_LIST_T  *pt_info
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `simId` | `E_QL_MCM_SIM_SLOT_ID_TYPE_T` | 输入（In） | (U)SIM 卡卡槽编号 |
| `pt_info` | `QL_SIM_PREFERRED_OPERATOR_LIST_T *` | 输出（Out） | 首选运营商信息列表 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_SIM_PREFERRED_OPERATOR_LIST_T`（数据结构）

首选运营商信息列表结构体。

```c
typedef struct
{
    uint32_t           preferred_operator_list_len;
    QL_SIM_PLMN_INFO_T preferred_operator_list[QL_SIM_PLMN_NUM_MAX];
} QL_SIM_PREFERRED_OPERATOR_LIST_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `uint32_t` | `preferred_operator_list_len` | 首选运营商列表长度（字节） |
| `QL_SIM_PLMN_INFO_T[]` | `preferred_operator_list` | 首选运营商列表，最多可容纳 24 个首选运营商 |

---

### `QL_SIM_PLMN_INFO_T`（数据结构）

单个首选运营商信息（PLMN）结构体。

```c
typedef struct
{
    char     mcc[QL_SIM_MCC_LEN];
    uint32_t mnc_len;
    char     mnc[QL_SIM_MNC_MAX];
} QL_SIM_PLMN_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `char[]` | `mcc` | 移动设备国家代码（MCC），3 位 |
| `uint32_t` | `mnc_len` | 移动设备网络代码的长度，2 位或 3 位 |
| `char[]` | `mnc` | 移动设备网络代码（MNC） |

---

### `QL_MCM_SIM_EnablePIN`

#### 功能描述

使能 PIN 码验证功能。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_EnablePIN(
    sim_client_handle_type   h_sim,
    QL_SIM_ENABLE_PIN_INFO_T *pt_info
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_SIM_ENABLE_PIN_INFO_T *` | 输入（In） | PIN 码验证开启信息（同 `QL_SIM_VERIFY_PIN_INFO_T`） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

#### 注意事项

- 该函数在**模块重启后生效**。
- 打开(U)SIM 卡 PIN 码验证功能后，**将无法拨打电话**。

---

### `QL_SIM_VERIFY_PIN_INFO_T`（数据结构）

PIN 码验证信息（也用于 EnablePIN/DisablePIN）。

```c
typedef struct
{
    QL_SIM_APP_ID_INFO_T       app_info;
    E_QL_MCM_SIM_PIN_ID_TYPE_T pin_id;
    uint32_t                   pin_value_len;
    char                       pin_value[QL_MCM_SIM_PIN_LEN_MAX];
} QL_SIM_VERIFY_PIN_INFO_T;
```

类型别名：
- `typedef QL_SIM_VERIFY_PIN_INFO_T QL_SIM_ENABLE_PIN_INFO_T;`
- `typedef QL_SIM_VERIFY_PIN_INFO_T QL_SIM_DISABLE_PIN_INFO_T;`

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `QL_SIM_APP_ID_INFO_T` | `app_info` | (U)SIM 卡应用程序标识信息（卡槽+应用类型） |
| `E_QL_MCM_SIM_PIN_ID_TYPE_T` | `pin_id` | PIN 码编号（PIN1 或 PIN2） |
| `uint32_t` | `pin_value_len` | PIN 码长度（字节），最大 8 字节 |
| `char[]` | `pin_value` | PIN 码字符串 |

---

### `E_QL_MCM_SIM_PIN_ID_TYPE_T`（枚举）

PIN 码编号枚举。

```c
typedef enum
{
    E_QL_MCM_SIM_PIN_ID_1 = 0xB01,
    E_QL_MCM_SIM_PIN_ID_2 = 0xB02,
} E_QL_MCM_SIM_PIN_ID_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `E_QL_MCM_SIM_PIN_ID_1` | 0xB01 | PIN 1 |
| `E_QL_MCM_SIM_PIN_ID_2` | 0xB02 | PIN 2 |

---

### `QL_MCM_SIM_DisablePIN`

#### 功能描述

禁用 PIN 码验证功能。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_DisablePIN(
    sim_client_handle_type    h_sim,
    QL_SIM_DISABLE_PIN_INFO_T *pt_info
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_SIM_DISABLE_PIN_INFO_T *` | 输入（In） | PIN 码验证关闭信息（同 `QL_SIM_VERIFY_PIN_INFO_T`） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_MCM_SIM_VerifyPIN`

#### 功能描述

验证 PIN 码。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_VerifyPIN(
    sim_client_handle_type   h_sim,
    QL_SIM_VERIFY_PIN_INFO_T *pt_info
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_SIM_VERIFY_PIN_INFO_T *` | 输入（In） | PIN 码验证信息 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

#### 注意事项

- 调用该函数前，需先调用 `QL_MCM_SIM_EnablePIN()` 开启(U)SIM 卡 PIN 码验证功能，否则将上报错误。

---

### `QL_MCM_SIM_ChangePin`

#### 功能描述

更改 PIN 码。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_ChangePin(
    sim_client_handle_type   h_sim,
    QL_SIM_CHANGE_PIN_INFO_T *pt_info
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_SIM_CHANGE_PIN_INFO_T *` | 输入（In） | PIN 码更改信息（含旧 PIN 和新 PIN） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

#### 注意事项

- 调用该函数前，需先调用 `QL_MCM_SIM_EnablePIN()` 开启(U)SIM 卡 PIN 码验证，否则将上报错误。

---

### `QL_SIM_CHANGE_PIN_INFO_T`（数据结构）

PIN 码更改信息结构体。

```c
typedef struct
{
    QL_SIM_APP_ID_INFO_T       app_info;
    E_QL_MCM_SIM_PIN_ID_TYPE_T pin_id;
    uint32_t                   old_pin_value_len;
    char                       old_pin_value[QL_MCM_SIM_PIN_LEN_MAX];
    uint32_t                   new_pin_value_len;
    char                       new_pin_value[QL_MCM_SIM_PIN_LEN_MAX];
} QL_SIM_CHANGE_PIN_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `QL_SIM_APP_ID_INFO_T` | `app_info` | (U)SIM 卡应用程序标识信息 |
| `E_QL_MCM_SIM_PIN_ID_TYPE_T` | `pin_id` | PIN 码编号 |
| `uint32_t` | `old_pin_value_len` | 旧的 PIN 码长度（字节），最大 8 字节 |
| `char[]` | `old_pin_value` | 旧的 PIN 码 |
| `uint32_t` | `new_pin_value_len` | 新的 PIN 码长度（字节），最大 8 字节 |
| `char[]` | `new_pin_value` | 新的 PIN 码 |

---

### `QL_MCM_SIM_UnblockPIN`

#### 功能描述

当 3 次错误输入 PIN 码且(U)SIM 卡状态为请求 PUK 时，输入 PUK 码和新的 PIN 码进行解锁。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_UnblockPIN(
    sim_client_handle_type    h_sim,
    QL_SIM_UNBLOCK_PIN_INFO_T *pt_info
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_SIM_UNBLOCK_PIN_INFO_T *` | 输入（In） | (U)SIM 卡解锁信息（含 PUK 码和新 PIN 码） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_SIM_UNBLOCK_PIN_INFO_T`（数据结构）

(U)SIM 卡解锁信息结构体。

```c
typedef struct
{
    QL_SIM_APP_ID_INFO_T       app_info;
    E_QL_MCM_SIM_PIN_ID_TYPE_T pin_id;
    uint32_t                   puk_value_len;
    char                       puk_value[QL_MCM_SIM_PIN_LEN_MAX];
    uint32_t                   new_pin_value_len;
    char                       new_pin_value[QL_MCM_SIM_PIN_LEN_MAX];
} QL_SIM_UNBLOCK_PIN_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `QL_SIM_APP_ID_INFO_T` | `app_info` | (U)SIM 卡应用程序标识信息 |
| `E_QL_MCM_SIM_PIN_ID_TYPE_T` | `pin_id` | PIN 码编号 |
| `uint32_t` | `puk_value_len` | PUK 码长度（字节），最大 8 字节 |
| `char[]` | `puk_value` | PUK 码 |
| `uint32_t` | `new_pin_value_len` | 新的 PIN 码长度（字节），最大 8 字节 |
| `char[]` | `new_pin_value` | 新的 PIN 码 |

---

### `QL_MCM_SIM_GetCardStatus`

#### 功能描述

获取(U)SIM 卡当前状态。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_GetCardStatus(
    sim_client_handle_type         h_sim,
    E_QL_MCM_SIM_SLOT_ID_TYPE_T    simId,
    QL_MCM_SIM_CARD_STATUS_INFO_T  *pt_info
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `simId` | `E_QL_MCM_SIM_SLOT_ID_TYPE_T` | 输入（In） | (U)SIM 卡卡槽编号 |
| `pt_info` | `QL_MCM_SIM_CARD_STATUS_INFO_T *` | 输出（Out） | (U)SIM 卡状态信息 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_MCM_SIM_CARD_STATUS_INFO_T`（数据结构）

(U)SIM 卡状态信息结构体。

```c
typedef struct
{
    E_QL_MCM_SIM_CARD_STATE_TYPE_T e_card_state;
    E_QL_MCM_SIM_CARD_TYPE_T       e_card_type;
    QL_MCM_SIM_CARD_ALL_APP_INFO_T card_app_info;
} QL_MCM_SIM_CARD_STATUS_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `E_QL_MCM_SIM_CARD_STATE_TYPE_T` | `e_card_state` | (U)SIM 卡状态 |
| `E_QL_MCM_SIM_CARD_TYPE_T` | `e_card_type` | (U)SIM 卡类型 |
| `QL_MCM_SIM_CARD_ALL_APP_INFO_T` | `card_app_info` | (U)SIM 卡应用程序信息 |

---

### `E_QL_MCM_SIM_CARD_STATE_TYPE_T`（枚举）

(U)SIM 卡状态枚举。

```c
typedef enum
{
    E_QL_MCM_SIM_CARD_STATE_UNKNOWN                    = 0xB01,
    E_QL_MCM_SIM_CARD_STATE_ABSENT                     = 0xB02,
    E_QL_MCM_SIM_CARD_STATE_PRESENT                    = 0xB03,
    E_QL_MCM_SIM_CARD_STATE_ERROR_UNKNOWN              = 0xB04,
    E_QL_MCM_SIM_CARD_STATE_ERROR_POWER_DOWN           = 0xB05,
    E_QL_MCM_SIM_CARD_STATE_ERROR_POLL_ERROR           = 0xB06,
    E_QL_MCM_SIM_CARD_STATE_ERROR_NO_ATR_RECEIVED      = 0xB07,
    E_QL_MCM_SIM_CARD_STATE_ERROR_VOLT_MISMATCH        = 0xB08,
    E_QL_MCM_SIM_CARD_STATE_ERROR_PARITY_ERROR         = 0xB09,
    E_QL_MCM_SIM_CARD_STATE_ERROR_SIM_TECHNICAL_PROBLEMS = 0xB0A,
} E_QL_MCM_SIM_CARD_STATE_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `E_QL_MCM_SIM_CARD_STATE_UNKNOWN` | 0xB01 | 未知状态 |
| `E_QL_MCM_SIM_CARD_STATE_ABSENT` | 0xB02 | 无卡状态 |
| `E_QL_MCM_SIM_CARD_STATE_PRESENT` | 0xB03 | 检测到(U)SIM 卡 |
| `E_QL_MCM_SIM_CARD_STATE_ERROR_UNKNOWN` | 0xB04 | 未知错误 |
| `E_QL_MCM_SIM_CARD_STATE_ERROR_POWER_DOWN` | 0xB05 | (U)SIM 卡未上电 |
| `E_QL_MCM_SIM_CARD_STATE_ERROR_POLL_ERROR` | 0xB06 | 轮询错误 |
| `E_QL_MCM_SIM_CARD_STATE_ERROR_NO_ATR_RECEIVED` | 0xB07 | 未收到复位应答 |
| `E_QL_MCM_SIM_CARD_STATE_ERROR_VOLT_MISMATCH` | 0xB08 | 电压失配 |
| `E_QL_MCM_SIM_CARD_STATE_ERROR_PARITY_ERROR` | 0xB09 | 奇偶校验错误 |
| `E_QL_MCM_SIM_CARD_STATE_ERROR_SIM_TECHNICAL_PROBLEMS` | 0xB0A | (U)SIM 卡技术问题 |

---

### `E_QL_MCM_SIM_CARD_TYPE_T`（枚举）

(U)SIM 卡类型枚举。

```c
typedef enum
{
    E_QL_MCM_SIM_CARD_TYPE_UNKNOWN = 0xB00,
    E_QL_MCM_SIM_CARD_TYPE_ICC     = 0xB01,
    E_QL_MCM_SIM_CARD_TYPE_UICC    = 0xB02,
} E_QL_MCM_SIM_CARD_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `E_QL_MCM_SIM_CARD_TYPE_UNKNOWN` | 0xB00 | 未知类型 |
| `E_QL_MCM_SIM_CARD_TYPE_ICC` | 0xB01 | SIM 卡或 RUIM 卡（ICC 格式） |
| `E_QL_MCM_SIM_CARD_TYPE_UICC` | 0xB02 | USIM 卡或 CSIM 卡（UICC 格式） |

---

### `QL_MCM_SIM_CARD_ALL_APP_INFO_T`（数据结构）

(U)SIM 卡所有应用程序信息结构体。

```c
typedef struct
{
    QL_MCM_SIM_CARD_APP_INFO_T app_3gpp;
    QL_MCM_SIM_CARD_APP_INFO_T app_3gpp2;
    QL_MCM_SIM_CARD_APP_INFO_T app_isim;
} QL_MCM_SIM_CARD_ALL_APP_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `QL_MCM_SIM_CARD_APP_INFO_T` | `app_3gpp` | 3GPP 应用程序信息 |
| `QL_MCM_SIM_CARD_APP_INFO_T` | `app_3gpp2` | 3GPP2 应用程序信息 |
| `QL_MCM_SIM_CARD_APP_INFO_T` | `app_isim` | ISIM 应用程序信息 |

---

### `QL_MCM_SIM_CARD_APP_INFO_T`（数据结构）

单个应用程序信息结构体。

```c
typedef struct
{
    QL_MCM_SIM_SUBSCRIPTION_TYPE_T    subscription;
    QL_MCM_SIM_APP_STATE_TYPE_T       app_state;
    E_QL_MCM_SIM_PERSO_FEATURE_TYPE_T perso_feature;
    uint8_t                           perso_retries;
    uint8_t                           perso_unblock_retries;
    QL_MCM_SIM_PIN_STATE_TYPE_T       pin1_state;
    uint8_t                           pin1_num_retries;
    uint8_t                           puk1_num_retries;
    QL_MCM_SIM_PIN_STATE_TYPE_T       pin2_state;
    uint8_t                           pin2_num_retries;
    uint8_t                           puk2_num_retries;
} QL_MCM_SIM_CARD_APP_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `QL_MCM_SIM_SUBSCRIPTION_TYPE_T` | `subscription` | 订阅类型（主卡/副卡/未订阅） |
| `QL_MCM_SIM_APP_STATE_TYPE_T` | `app_state` | 应用程序状态 |
| `E_QL_MCM_SIM_PERSO_FEATURE_TYPE_T` | `perso_feature` | 个性化功能类型 |
| `uint8_t` | `perso_retries` | 设置个性化功能剩余尝试次数 |
| `uint8_t` | `perso_unblock_retries` | 解锁个性化功能剩余尝试次数 |
| `QL_MCM_SIM_PIN_STATE_TYPE_T` | `pin1_state` | PIN1 码状态 |
| `uint8_t` | `pin1_num_retries` | 输入 PIN1 码剩余尝试次数 |
| `uint8_t` | `puk1_num_retries` | 输入 PUK1 码剩余尝试次数 |
| `QL_MCM_SIM_PIN_STATE_TYPE_T` | `pin2_state` | PIN2 码状态 |
| `uint8_t` | `pin2_num_retries` | 输入 PIN2 码剩余尝试次数 |
| `uint8_t` | `puk2_num_retries` | 输入 PUK2 码剩余尝试次数 |

---

### `QL_MCM_SIM_SUBSCRIPTION_TYPE_T`（枚举）

订阅类型枚举。

```c
typedef enum
{
    E_QL_MCM_SIM_PROV_STATE_NONE = 0xB00,
    E_QL_MCM_SIM_PROV_STATE_PRI  = 0xB01,
    E_QL_MCM_SIM_PROV_STATE_SEC  = 0xB02,
} QL_MCM_SIM_SUBSCRIPTION_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `E_QL_MCM_SIM_PROV_STATE_NONE` | 0xB00 | 未订阅 |
| `E_QL_MCM_SIM_PROV_STATE_PRI` | 0xB01 | 主卡 |
| `E_QL_MCM_SIM_PROV_STATE_SEC` | 0xB02 | 副卡 |

---

### `QL_MCM_SIM_APP_STATE_TYPE_T`（枚举）

应用程序状态枚举。

```c
typedef enum
{
    E_QL_MCM_SIM_APP_STATE_UNKNOWN                 = 0xB00,
    E_QL_MCM_SIM_APP_STATE_DETECTED                = 0xB01,
    E_QL_MCM_SIM_APP_STATE_PIN1_REQ                = 0xB02,
    E_QL_MCM_SIM_APP_STATE_PUK1_REQ               = 0xB03,
    E_QL_MCM_SIM_APP_STATE_INITALIZATING           = 0xB04,
    E_QL_MCM_SIM_APP_STATE_PERSO_CK_REQ           = 0xB05,
    E_QL_MCM_SIM_APP_STATE_PERSO_PUK_REQ          = 0xB06,
    E_QL_MCM_SIM_APP_STATE_PERSO_PERMANENTLY_BLOCKED = 0xB07,
    E_QL_MCM_SIM_APP_STATE_PIN1_PERM_BLOCKED      = 0xB08,
    E_QL_MCM_SIM_APP_STATE_ILLEGAL                 = 0xB09,
    E_QL_MCM_SIM_APP_STATE_READY                   = 0xB0A,
} QL_MCM_SIM_APP_STATE_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `E_QL_MCM_SIM_APP_STATE_UNKNOWN` | 0xB00 | 未知状态 |
| `E_QL_MCM_SIM_APP_STATE_DETECTED` | 0xB01 | 检测到(U)SIM 卡 |
| `E_QL_MCM_SIM_APP_STATE_PIN1_REQ` | 0xB02 | 等待输入 PIN1 码 |
| `E_QL_MCM_SIM_APP_STATE_PUK1_REQ` | 0xB03 | 等待输入 PUK1 码 |
| `E_QL_MCM_SIM_APP_STATE_INITALIZATING` | 0xB04 | 初始化中 |
| `E_QL_MCM_SIM_APP_STATE_PERSO_CK_REQ` | 0xB05 | 等待输入 PCK 码 |
| `E_QL_MCM_SIM_APP_STATE_PERSO_PUK_REQ` | 0xB06 | 等待输入解锁个性化设置的密码 |
| `E_QL_MCM_SIM_APP_STATE_PERSO_PERMANENTLY_BLOCKED` | 0xB07 | 个性化设置被永久锁定 |
| `E_QL_MCM_SIM_APP_STATE_PIN1_PERM_BLOCKED` | 0xB08 | PIN1 码被永久锁定 |
| `E_QL_MCM_SIM_APP_STATE_ILLEGAL` | 0xB09 | 非法状态 |
| `E_QL_MCM_SIM_APP_STATE_READY` | 0xB0A | 初始化完成，就绪 |

---

### `QL_MCM_SIM_PIN_STATE_TYPE_T`（枚举）

PIN 码状态枚举。

```c
typedef enum
{
    E_QL_MCM_SIM_PIN_STATE_UNKNOWN             = 0xB01,
    E_QL_MCM_SIM_PIN_STATE_ENABLED_NOT_VERIFIED = 0xB02,
    E_QL_MCM_SIM_PIN_STATE_ENABLED_VERIFIED    = 0xB03,
    E_QL_MCM_SIM_PIN_STATE_DISABLED            = 0xB04,
    E_QL_MCM_SIM_PIN_STATE_BLOCKED             = 0xB05,
    E_QL_MCM_SIM_PIN_STATE_PERMANENTLY_BLOCKED = 0xB06,
} QL_MCM_SIM_PIN_STATE_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `E_QL_MCM_SIM_PIN_STATE_UNKNOWN` | 0xB01 | 未知状态 |
| `E_QL_MCM_SIM_PIN_STATE_ENABLED_NOT_VERIFIED` | 0xB02 | PIN 码验证功能已使能，尚未验证 |
| `E_QL_MCM_SIM_PIN_STATE_ENABLED_VERIFIED` | 0xB03 | PIN 码验证功能已使能，通过验证 |
| `E_QL_MCM_SIM_PIN_STATE_DISABLED` | 0xB04 | PIN 码验证功能未使能 |
| `E_QL_MCM_SIM_PIN_STATE_BLOCKED` | 0xB05 | PIN 码输入错误次数超过限制，需要 PUK 码解锁 |
| `E_QL_MCM_SIM_PIN_STATE_PERMANENTLY_BLOCKED` | 0xB06 | PUK 码输入错误次数超过限制，(U)SIM 卡永久锁定，不可恢复 |

---

### `QL_MCM_SIM_Personalization`

#### 功能描述

激活和设置个性化数据（SIM 卡锁定功能）。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_Personalization(
    sim_client_handle_type                 h_sim,
    QL_MCM_SIM_PERSONALIZE_FEATURE_INFO_T  *pt_info
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_MCM_SIM_PERSONALIZE_FEATURE_INFO_T *` | 输入（In） | 个性化数据激活信息 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_MCM_SIM_PERSONALIZE_FEATURE_INFO_T`（数据结构）

个性化数据激活信息结构体。

```c
typedef struct
{
    uint32_t                               ctrl_key_value_len;
    char                                   ctrl_key_value[MCM_SIM_CK_MAX_V01];
    E_QL_MCM_SIM_PERSO_FEATURE_TYPE_T      e_feature_type;
    uint32_t                               feature_data_len;
    QL_MCM_SIM_PERSONAL_FEATURE_INFO_DATA_T t_feature_data;
} QL_MCM_SIM_PERSONALIZE_FEATURE_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `uint32_t` | `ctrl_key_value_len` | CK 码长度（字节），最大 16 字节 |
| `char[]` | `ctrl_key_value` | CK 码（Control Key，控制密码） |
| `E_QL_MCM_SIM_PERSO_FEATURE_TYPE_T` | `e_feature_type` | 个性化功能类型 |
| `uint32_t` | `feature_data_len` | `t_feature_data` 有效数组的数目 |
| `QL_MCM_SIM_PERSONAL_FEATURE_INFO_DATA_T` | `t_feature_data` | 个性化数据（联合体） |

---

### `E_QL_MCM_SIM_PERSO_FEATURE_TYPE_T`（枚举）

个性化功能类型枚举。

```c
typedef enum
{
    E_QL_MCM_SIM_PERSO_FEATURE_TYPE_UNKNOWN              = 0xB00,
    E_QL_MCM_SIM_PERSO_FEATURE_TYPE_3GPP_NETWORK         = 0xB01,
    E_QL_MCM_SIM_PERSO_FEATURE_TYPE_3GPP_NETWORK_SUBSET  = 0xB02,
    E_QL_MCM_SIM_PERSO_FEATURE_TYPE_3GPP_SERVICE_PROVIDER= 0xB03,
    E_QL_MCM_SIM_PERSO_FEATURE_TYPE_3GPP_CORPORATE       = 0xB04,
    E_QL_MCM_SIM_PERSO_FEATURE_TYPE_3GPP_SIM             = 0xB05,
    E_QL_MCM_SIM_PERSO_FEATURE_TYPE_3GPP2_NETWORK_TYPE_1 = 0xB06,
    E_QL_MCM_SIM_PERSO_FEATURE_TYPE_3GPP2_NETWORK_TYPE_2 = 0xB07,
    E_QL_MCM_SIM_PERSO_FEATURE_TYPE_3GPP2_RUIM           = 0xB08,
} E_QL_MCM_SIM_PERSO_FEATURE_TYPE_T;
```

| 成员 | 描述 |
|-----|------|
| `UNKNOWN` | 未知特性 |
| `3GPP_NETWORK` | 基于 3GPP MCC 和 MNC 的特性 |
| `3GPP_NETWORK_SUBSET` | 基于 3GPP MCC、MNC 和 IMSI 第 6、7 位的特性 |
| `3GPP_SERVICE_PROVIDER` | 基于 3GPP MCC、MNC 和 GID1 的特性 |
| `3GPP_CORPORATE` | 基于 3GPP MCC、MNC、GID1 和 GID2 的特性 |
| `3GPP_SIM` | 基于 3GPP IMSI 的特性 |
| `3GPP2_NETWORK_TYPE_1` | 基于 3GPP2 MCC 和 MNC 的特性 |
| `3GPP2_NETWORK_TYPE_2` | 基于 3GPP2 IRM 的特性 |
| `3GPP2_RUIM` | 基于 3GPP2 IMSI_M 的特性 |

---

### `QL_MCM_SIM_PERSONAL_FEATURE_INFO_DATA_T`（联合体）

个性化数据联合体定义，根据 `e_feature_type` 选择使用对应的成员。

```c
typedef union
{
    QL_MCM_SIM_NW_PERSONAL_INFO_T           feature_gw_network_perso[QL_MCM_SIM_PERSO_NUM_NW_MAX];
    QL_MCM_SIM_NW_SUBSET_PERSONAL_INFO_T    feature_gw_network_subset_perso[QL_MCM_SIM_PERSO_NUM_NS_MAX];
    QL_MCM_SIM_NW_SP_PERSONAL_INFO_T        feature_gw_sp_perso[QL_MCM_SIM_PERSO_NUM_GW_SP_MAX];
    QL_MCM_SIM_GW_CORPORATE_PERSONAL_INFO_T feature_gw_corporate_perso[QL_MCM_SIM_PERSO_NUM_GW_CP_MAX];
    QL_MCM_SIM_SIM_PERSONAL_INFO_T          feature_gw_sim_perso[QL_MCM_SIM_PERSO_NUM_SIM_MAX];
    QL_MCM_SIM_NW_PERSONAL_INFO_T           feature_1x_network1_perso[QL_MCM_SIM_PERSO_NUM_NW_MAX];
    QL_MCM_SIM_1X_NW_TYPE2_PERSONAL_INFO_T  feature_1x_network2_perso[QL_MCM_SIM_PERSO_NUM_1X_NW2_MAX];
    QL_MCM_SIM_SIM_PERSONAL_INFO_T          feature_1x_ruim_perso[QL_MCM_SIM_PERSO_NUM_SIM_MAX];
} QL_MCM_SIM_PERSONAL_FEATURE_INFO_DATA_T;
```

| 字段 | 说明 |
|-----|------|
| `feature_gw_network_perso` | GW 网络个性化 |
| `feature_gw_network_subset_perso` | GW 网络子集个性化 |
| `feature_gw_sp_perso` | GW 服务提供商个性化 |
| `feature_gw_corporate_perso` | GW 企业个性化 |
| `feature_gw_sim_perso` | GW (U)SIM 卡个性化 |
| `feature_1x_network1_perso` | 网络类型 1 个性化（3GPP2） |
| `feature_1x_network2_perso` | 网络类型 2 个性化（3GPP2 IRM） |
| `feature_1x_ruim_perso` | RUIM 个性化 |

#### 个性化子结构体

**`QL_MCM_SIM_NW_PERSONAL_INFO_T`**（GW 网络 / 网络类型1）：
```c
typedef struct {
    char     mcc[MCM_SIM_MCC_LEN_V01];  // 移动设备国家代码
    uint32_t mnc_len;                    // MNC 长度（2 或 3 位）
    char     mnc[MCM_SIM_MNC_MAX_V01];  // 移动设备网络代码
} QL_MCM_SIM_NW_PERSONAL_INFO_T;
```

**`QL_MCM_SIM_NW_SUBSET_PERSONAL_INFO_T`**（GW 网络子集）：
```c
typedef struct {
    QL_MCM_SIM_NW_PERSONAL_INFO_T network;  // MCC+MNC
    char digit6;                             // IMSI 第 6 位
    char digit7;                             // IMSI 第 7 位
} QL_MCM_SIM_NW_SUBSET_PERSONAL_INFO_T;
```

**`QL_MCM_SIM_NW_SP_PERSONAL_INFO_T`**（GW 服务提供商）：
```c
typedef struct {
    QL_MCM_SIM_NW_PERSONAL_INFO_T network;  // MCC+MNC
    uint8_t gid1;                            // GID1 中的服务提供商代码
} QL_MCM_SIM_NW_SP_PERSONAL_INFO_T;
```

**`QL_MCM_SIM_GW_CORPORATE_PERSONAL_INFO_T`**（GW 企业）：
```c
typedef struct {
    QL_MCM_SIM_NW_PERSONAL_INFO_T network;  // MCC+MNC
    uint8_t gid1;                            // GID1 中的服务提供商代码
    uint8_t gid2;                            // GID2 中的企业客户代码
} QL_MCM_SIM_GW_CORPORATE_PERSONAL_INFO_T;
```

**`QL_MCM_SIM_SIM_PERSONAL_INFO_T`**（GW SIM / RUIM）：
```c
typedef struct {
    QL_MCM_SIM_NW_PERSONAL_INFO_T network;    // MCC+MNC
    uint32_t msin_len;                          // MSIN 长度（最大 10 字节）
    char     msin[QL_MCM_SIM_MSIN_LEN_MAX];   // MSIN
} QL_MCM_SIM_SIM_PERSONAL_INFO_T;
```

**`QL_MCM_SIM_1X_NW_TYPE2_PERSONAL_INFO_T`**（网络类型2）：
```c
typedef struct {
    char irm_code[4];   // 以 IRM 为基础的 IMSI_M MIN 的前 4 位数
} QL_MCM_SIM_1X_NW_TYPE2_PERSONAL_INFO_T;
```

---

### `QL_MCM_SIM_Depersonalization`

#### 功能描述

关闭或解锁(U)SIM 卡个性化数据功能。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_Depersonalization(
    sim_client_handle_type           h_sim,
    QL_MCM_SIM_DEPERSONALIZE_INFO_T  *pt_info
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_MCM_SIM_DEPERSONALIZE_INFO_T *` | 输入（In） | 个性化数据解锁信息 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_MCM_SIM_DEPERSONALIZE_INFO_T`（数据结构）

个性化数据解锁信息结构体。

```c
typedef struct
{
    E_QL_MCM_SIM_PERSO_FEATURE_TYPE_T    e_feature_type;
    E_QL_MCM_SIM_PERSO_OPERATION_TYPE_T  e_operation;
    uint32_t                             ctrl_key_value_len;
    char                                 ctrl_key_value[QL_MCM_SIM_CONTROL_KEY_LEN_MAX];
} QL_MCM_SIM_DEPERSONALIZE_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `E_QL_MCM_SIM_PERSO_FEATURE_TYPE_T` | `e_feature_type` | 待关闭或解锁的个性化功能 |
| `E_QL_MCM_SIM_PERSO_OPERATION_TYPE_T` | `e_operation` | 操作类型（关闭或解锁） |
| `uint32_t` | `ctrl_key_value_len` | CK 码长度（字节），最大 16 字节 |
| `char[]` | `ctrl_key_value` | CK 码 |

---

### `E_QL_MCM_SIM_PERSO_OPERATION_TYPE_T`（枚举）

操作类型枚举。

```c
typedef enum
{
    E_QL_MCM_SIM_PERSO_OPERATION_DEACTIVATE = 0xB00,
    E_QL_MCM_SIM_PERSO_OPERATION_UNBLOCK    = 0xB01,
} E_QL_MCM_SIM_PERSO_OPERATION_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `E_QL_MCM_SIM_PERSO_OPERATION_DEACTIVATE` | 0xB00 | 关闭个性化功能 |
| `E_QL_MCM_SIM_PERSO_OPERATION_UNBLOCK` | 0xB01 | 解锁个性化功能 |

---

### `QL_MCM_SIM_WriteFile`

#### 功能描述

将数据写入(U)SIM 卡文件。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_WriteFile(
    sim_client_handle_type             h_sim,
    QL_MCM_SIM_CARD_WRITE_FILE_INFO_T  *pt_info
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_MCM_SIM_CARD_WRITE_FILE_INFO_T *` | 输入（In） | 待写入的数据和文件信息 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_MCM_SIM_CARD_WRITE_FILE_INFO_T`（数据结构）

写文件信息结构体。

```c
typedef struct
{
    QL_SIM_APP_ID_INFO_T               app_info;
    QL_MCM_SIM_CARD_FILE_ACCESS_INFO_T file_access;
    uint32_t                           data_len;
    uint8_t                            data[QL_MCM_SIM_DATA_LEN_MAX];
} QL_MCM_SIM_CARD_WRITE_FILE_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `QL_SIM_APP_ID_INFO_T` | `app_info` | (U)SIM 卡应用程序标识信息 |
| `QL_MCM_SIM_CARD_FILE_ACCESS_INFO_T` | `file_access` | (U)SIM 卡文件访问信息 |
| `uint32_t` | `data_len` | 数据长度（字节），最大 4096 字节 |
| `uint8_t[]` | `data` | 待写入的数据 |

---

### `QL_MCM_SIM_CARD_FILE_ACCESS_INFO_T`（数据结构）

(U)SIM 卡文件访问信息结构体（读写共用）。

```c
typedef struct
{
    uint16_t offset;
    uint8_t  record_num;
    uint32_t path_len;
    char     path[QL_MCM_SIM_PATH_LEN_MAX];
} QL_MCM_SIM_CARD_FILE_ACCESS_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `uint16_t` | `offset` | 文件中数据起始位置。仅对透明文件有效；读取时取值为 0 |
| `uint8_t` | `record_num` | 记录索引。0=透明文件，大于0=循环或线性固定文件 |
| `uint32_t` | `path_len` | 文件路径长度 |
| `char[]` | `path` | 文件路径，最大 20 字节 |

---

### `QL_MCM_SIM_ReadFile`

#### 功能描述

从(U)SIM 卡文件读取数据。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_ReadFile(
    sim_client_handle_type               h_sim,
    QL_MCM_SIM_CARD_FILE_INFO_TO_READ_T  *pt_info,
    QL_MCM_SIM_CARD_FILE_DATA_T          *pt_out
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_MCM_SIM_CARD_FILE_INFO_TO_READ_T *` | 输入（In） | 应用程序标识信息和文件信息 |
| `pt_out` | `QL_MCM_SIM_CARD_FILE_DATA_T *` | 输出（Out） | 从(U)SIM 卡文件读取的数据信息 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_MCM_SIM_CARD_FILE_INFO_TO_READ_T`（数据结构）

读文件输入信息结构体。

```c
typedef struct
{
    QL_SIM_APP_ID_INFO_T               app_info;
    QL_MCM_SIM_CARD_FILE_ACCESS_INFO_T file_access;
} QL_MCM_SIM_CARD_FILE_INFO_TO_READ_T;
```

### `QL_MCM_SIM_CARD_FILE_DATA_T`（数据结构）

读文件输出数据结构体。

```c
typedef struct
{
    uint32_t data_len;
    uint8_t  data[QL_MCM_SIM_DATA_LEN_MAX];
} QL_MCM_SIM_CARD_FILE_DATA_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `uint32_t` | `data_len` | 数据长度，最大 4096 字节 |
| `uint8_t[]` | `data` | 从(U)SIM 卡文件读取的数据 |

---

### `QL_MCM_SIM_GetFileSize`

#### 功能描述

获取(U)SIM 卡文件大小。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_GetFileSize(
    sim_client_handle_type                      h_sim,
    QL_MCM_SIM_CARD_GET_FILE_SIZE_INPUT_INFO_T  *pt_info,
    QL_MCM_SIM_FILE_SIZE_INFO_T                 *pt_out
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_MCM_SIM_CARD_GET_FILE_SIZE_INPUT_INFO_T *` | 输入（In） | 应用程序标识信息和文件路径 |
| `pt_out` | `QL_MCM_SIM_FILE_SIZE_INFO_T *` | 输出（Out） | 文件大小信息 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_MCM_SIM_CARD_GET_FILE_SIZE_INPUT_INFO_T`（数据结构）

```c
typedef struct
{
    QL_SIM_APP_ID_INFO_T app_info;
    uint32_t             path_len;
    char                 path[QL_MCM_SIM_PATH_LEN_MAX];
} QL_MCM_SIM_CARD_GET_FILE_SIZE_INPUT_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `QL_SIM_APP_ID_INFO_T` | `app_info` | (U)SIM 卡应用程序标识信息 |
| `uint32_t` | `path_len` | 文件路径长度，最大 20 字节 |
| `char[]` | `path` | 文件路径 |

---

### `QL_MCM_SIM_FILE_SIZE_INFO_T`（数据结构）

文件大小信息结构体。

```c
typedef struct
{
    E_QL_MCM_SIM_FILE_TYPE_T e_file_type;
    uint16_t                 file_size;
    uint16_t                 record_size;
    uint16_t                 record_count;
} QL_MCM_SIM_FILE_SIZE_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `E_QL_MCM_SIM_FILE_TYPE_T` | `e_file_type` | (U)SIM 卡文件类型 |
| `uint16_t` | `file_size` | 透明文件的大小 |
| `uint16_t` | `record_size` | 循环或线性固定文件中每个记录的大小 |
| `uint16_t` | `record_count` | 循环或线性固定文件中记录个数 |

---

### `E_QL_MCM_SIM_FILE_TYPE_T`（枚举）

(U)SIM 卡文件类型枚举。

```c
typedef enum
{
    E_QL_MCM_SIM_FILE_TYPE_UNKNOWN     = 0xB00,
    E_QL_MCM_SIM_FILE_TYPE_TRANSPARENT = 0xB01,
    E_QL_MCM_SIM_FILE_TYPE_CYCLIC      = 0xB02,
    MCM_SIM_FILE_TYPE_LINEAR_FIXED     = 0xB03,
} E_QL_MCM_SIM_FILE_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `E_QL_MCM_SIM_FILE_TYPE_UNKNOWN` | 0xB00 | 未知类型 |
| `E_QL_MCM_SIM_FILE_TYPE_TRANSPARENT` | 0xB01 | 透明文件 |
| `E_QL_MCM_SIM_FILE_TYPE_CYCLIC` | 0xB02 | 循环文件 |
| `MCM_SIM_FILE_TYPE_LINEAR_FIXED` | 0xB03 | 线性固定文件 |

---

### `QL_MCM_SIM_ReadPhoneBookRecord`

#### 功能描述

读取(U)SIM 卡电话簿中的电话号码记录。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_ReadPhoneBookRecord(
    sim_client_handle_type               h_sim,
    QL_MCM_SIM_PHONE_BOOK_RECORD_INFO_T  *pt_info
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_MCM_SIM_PHONE_BOOK_RECORD_INFO_T *` | 输出（Out） | 读取的电话号码记录（含文件信息和记录内容） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_MCM_SIM_WritePhoneBookRecord`

#### 功能描述

将电话号码记录写入(U)SIM 卡电话簿。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_SIM_WritePhoneBookRecord(
    sim_client_handle_type               h_sim,
    QL_MCM_SIM_PHONE_BOOK_RECORD_INFO_T  *pt_info
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_sim` | `sim_client_handle_type` | 输入（In） | (U)SIM 卡服务句柄 |
| `pt_info` | `QL_MCM_SIM_PHONE_BOOK_RECORD_INFO_T *` | 输入（In） | 待写入的电话号码记录 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 函数执行成功 |
| 其他值 | 函数执行失败，详见 `ql_mcm.h` |

---

### `QL_MCM_SIM_PHONE_BOOK_RECORD_INFO_T`（数据结构）

电话号码记录结构体。

```c
typedef struct
{
    QL_MCM_SIM_CARD_FILE_INFO_TO_READ_T file_info;
    QL_MCM_SIM_PHONE_RECORD_INFO_T      t_record;
} QL_MCM_SIM_PHONE_BOOK_RECORD_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `QL_MCM_SIM_CARD_FILE_INFO_TO_READ_T` | `file_info` | 文件信息（读写通用） |
| `QL_MCM_SIM_PHONE_RECORD_INFO_T` | `t_record` | 电话号码记录内容 |

---

### `QL_MCM_SIM_PHONE_RECORD_INFO_T`（数据结构）

电话号码记录内容结构体。

```c
typedef struct
{
    char username[14];
    char phonenum[24];
} QL_MCM_SIM_PHONE_RECORD_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `char[14]` | `username` | 电话号码记录中的用户名 |
| `char[24]` | `phonenum` | 电话号码（号码前加 "+" 号，例如：+13812345678） |

---

## 3. 使用流程与示例代码

### 3.1 (U)SIM API 总体使用流程

```
程序启动
    │
    ▼
QL_MCM_SIM_Client_Init()         ← 初始化 SIM 服务，获取句柄
    │
    ▼
QL_MCM_SIM_GetCardStatus()       ← 检查 SIM 卡状态
    │
    ├─ 卡存在且就绪 ──────────────────────────┐
    │                                        ▼
    │                        QL_MCM_SIM_GetIMSI()  / GetICCID() / GetPhoneNumber()
    │                        QL_MCM_SIM_EnablePIN() / VerifyPIN() / ChangePin()
    │                        QL_MCM_SIM_ReadFile() / WriteFile() / GetFileSize()
    │                        QL_MCM_SIM_ReadPhoneBookRecord() / WritePhoneBookRecord()
    │
    ▼
QL_MCM_SIM_Client_Deinit()       ← 注销 SIM 服务
    │
    ▼
程序退出
```

### 3.2 获取 IMSI 示例

```c
sim_client_handle_type h_sim;
char imsi[20] = {0};
QL_SIM_APP_ID_INFO_T app_info;

// 初始化
int ret = QL_MCM_SIM_Client_Init(&h_sim);
if (ret != 0) {
    printf("Init failed: %d\n", ret);
    return;
}

// 设置卡槽和应用类型
app_info.e_slot_id = E_QL_MCM_SIM_SLOT_ID_1;
app_info.e_app = E_QL_MCM_SIM_APP_TYPE_3GPP;

// 获取 IMSI
ret = QL_MCM_SIM_GetIMSI(h_sim, &app_info, imsi, sizeof(imsi));
if (ret == 0) {
    printf("IMSI: %s\n", imsi);
}

// 注销
QL_MCM_SIM_Client_Deinit(h_sim);
```

### 3.3 PIN 码管理流程

```c
QL_SIM_VERIFY_PIN_INFO_T pin_info;
pin_info.app_info.e_slot_id = E_QL_MCM_SIM_SLOT_ID_1;
pin_info.app_info.e_app = E_QL_MCM_SIM_APP_TYPE_3GPP;
pin_info.pin_id = E_QL_MCM_SIM_PIN_ID_1;
pin_info.pin_value_len = 4;
strncpy(pin_info.pin_value, "1234", QL_MCM_SIM_PIN_LEN_MAX);

// 使能 PIN 验证
QL_MCM_SIM_EnablePIN(h_sim, &pin_info);

// 验证 PIN 码
QL_MCM_SIM_VerifyPIN(h_sim, &pin_info);
```

### 3.4 示例代码位置

示例代码文件：`test_sim.c`  
路径：`/ql-ol-sdk/ql-ol-extsdk/example/test_mcm_api/test_sim.c`

---

## 4. 错误码说明

所有 API 函数的返回值类型为 `int` 或 `E_QL_ERROR_CODE_T`，具体错误码定义在 `ql_mcm.h` 中。

| 返回值 | 含义 |
|-------|------|
| `0` | 执行成功 |
| 其他值 | 执行失败，需参考 `ql_mcm.h` 中的错误码定义 |

---

## 5. 注意事项与限制

1. **并发调用限制**：所有(U)SIM API 函数均**不支持并发调用**。

2. **回调函数内禁止调用**：**不能在相关回调函数中调用**以上函数。

3. **生命周期管理**：
   - 使用任何(U)SIM 卡接口函数前，必须先调用 `QL_MCM_SIM_Client_Init()` 初始化服务
   - 不再使用时必须调用 `QL_MCM_SIM_Client_Deinit()` 注销服务

4. **PIN 码使能要求**：
   - 调用 `QL_MCM_SIM_VerifyPIN()` 前，需先调用 `QL_MCM_SIM_EnablePIN()` 开启 PIN 验证
   - 调用 `QL_MCM_SIM_ChangePin()` 前，需先开启 PIN 验证
   - 开启 PIN 验证功能在**模块重启后生效**
   - 开启 PIN 验证后**将无法拨打电话**

5. **PUK 解锁条件**：
   - `QL_MCM_SIM_UnblockPIN()` 仅在 PIN 码输入 3 次错误后 SIM 卡进入 PUK 请求状态时有效
   - 若 PUK 码也超过错误次数限制，SIM 卡将**永久锁定且不可恢复**

6. **文件操作**：
   - `offset` 参数仅对透明文件有效，读取时固定为 0
   - `record_num` 为 0 表示透明文件，大于 0 表示循环或线性固定文件
   - 文件路径最大 20 字节，数据最大 4096 字节

7. **PLMN 列表**：首选运营商列表最多可容纳 24 个运营商。

---

## 6. 版本与兼容性信息

| 项目 | 信息 |
|-----|------|
| 文档版本 | V1.0 |
| 文档日期 | 2023-06-30 |
| 文档状态 | 受控文件 |
| 作者 | Colin CUI |
| 适用模块系列 | EC2x（EC20-CE/EC20-CN/EC21/EC25）、EG9x（EG91/EG95）、EG25-G |
| 头文件 | `ql_mcm_sim.h` |
| 头文件路径 | `ql-ol-sdk/ql-ol-extsdk/include/` |
| 示例代码路径 | `ql-ol-sdk/ql-ol-extsdk/example/test_mcm_api/test_sim.c` |
| 参考文档 | Quectel_EC2x&EG2x-G&EG9x 系列_QuecOpen_快速开发指导 |

---

## 7. 附录：术语缩写

| 缩写 | 英文全称 | 中文全称 |
|-----|---------|---------|
| 3GPP | 3rd Generation Partnership Project | 第三代合作伙伴计划 |
| 3GPP2 | 3rd Generation Partnership Project 2 | 第三代合作伙伴计划 2 |
| API | Application Programming Interface | 应用程序接口 |
| CK | Control Key | 控制密码 |
| CSIM | CDMA2000 Subscriber Identity Module | CDMA2000 用户识别模块 |
| GID | Group Identifier | 组标识符 |
| ICCID | Integrated Circuit Card ID | 集成电路卡识别码 |
| IMSI | International Mobile Subscriber Identity | 国际移动用户识别码 |
| IoT | Internet of Things | 物联网 |
| IRM | International Roaming MIN | 国际漫游移动识别码 |
| ISIM | IP Multimedia Service Identity Module | IP 多媒体服务身份模块 |
| MCC | Mobile Country Code | 移动设备国家代码 |
| MIN | Mobile Identification Number | 手机识别码 |
| MNC | Mobile Network Code | 移动设备网络代码 |
| MSIN | Mobile Subscriber Identification Number | 移动用户识别码 |
| PCK | Personalization Control Key | 个性化控制密钥 |
| PIN | Personal Identification Number | 个人识别密码 |
| PUK | PIN Unblocking Key | PIN 码解锁密码 |
| RUIM | Removable User Identity Module | 可移动用户识别模块 |
| SDK | Software Development Kit | 软件开发工具包 |
| SIM | Subscriber Identification Module | 用户识别模块 |
| (U)SIM | (Universal) Subscriber Identity Module | （通用）用户识别模块 |

<!-- GENERATION_COMPLETE: 2026-05-28 -->
