# Quectel EC2x&EG9x&EG25-G 系列 QuecOpen 注网信息 API 参考手册分析

> 原始文档：Quectel_EC2x&EG9x&EG25-G系列_QuecOpen_注网信息API_参考手册_V1.0.pdf  
> 文档版本：V1.0  
> 文档日期：2021-02-19  
> 分析日期：2026-05-28

---

## 1. 文档概述

### 1.1 手册目的

本手册介绍移远通信（Quectel）EC2x 系列、EG9x 系列和 EG25-G 模块在 QuecOpen® 方案下进行**网络注册（注网）**和**获取注网相关信息**时使用的一系列 API 函数。

通过注网信息 API，用户可实现以下功能：
1. 初始化或注销注网服务
2. 设置或获取注网相关信息（首选网络制式、漫游通知开关、注网状态信息等）
3. 获取网络时间（NITZ）
4. 设置功耗模式（低功耗/正常模式）
5. 获取信号强度（各网络制式）
6. 触发网络扫描
7. 注册网络事件并设置事件回调函数
8. 获取小区访问状态

### 1.2 适用模块型号

| 模块系列 | 具体模块 |
|---------|---------|
| EC2x 系列 | EC25 系列、EC21 系列、EC20 R2.1、EC20-CN |
| EG9x 系列 | EG95 系列、EG91 系列 |
| EG25-G | EG25-G |

### 1.3 章节结构总览

| 章节 | 内容 |
|-----|------|
| 第1章 引言 | 功能说明、适用模块 |
| 第2章 注网相关功能介绍 | 功耗模式配置说明、信号强度转换规则（各网络制式） |
| 第3章 注网信息 API | 头文件、参考示例、函数概览、13 个函数详解（含所有数据结构、枚举） |
| 第4章 示例 | 各接口函数完整代码片段（13 个示例） |
| 第5章 附录 | 参考文档、术语缩写表 |

---

## 2. 注网相关功能介绍

### 2.1 功耗模式配置

模块具有休眠唤醒功能：
- **正常模式**：模块每隔约 2~3 秒自动上报一次信号强度事件，收到上报后模块被唤醒
- **低功耗模式**：关闭信号强度事件上报，模块不会因信号强度上报而被频繁唤醒

> **注意**：信号强度事件即便未通过 `QL_MCM_NW_EventRegister()` 注册，也会自动上报并唤醒模块。若需禁止，必须通过 `QL_MCM_NW_SetLowPowerMode()` 关闭。

### 2.2 信号格数显示规则

信号强度由强到弱定义为 5 个等级：
- **GREAT**（5格）、**GOOD**（4格）、**MODERATE**（3格）、**POOR**（2格）、**NONE**（1格）

#### 2.2.1 CDMA 网络制式信号强度转换

获取 rssi 和 ecio 两个参数，分别转换为 rssi_level 和 ecio_level（1~5），取较小者。

**rssi → 信号格数**：
| rssi 范围 | 信号格数 |
|---------|--------|
| rssi < -100 | 1 |
| -100 ≤ rssi < -95 | 2 |
| -95 ≤ rssi < -85 | 3 |
| -85 ≤ rssi < -75 | 4 |
| rssi ≥ -75 | 5 |

**ecio → 信号格数**：
| ecio 范围 | 信号格数 |
|---------|--------|
| ecio < -150 | 1 |
| -150 ≤ ecio < -130 | 2 |
| -130 ≤ ecio < -110 | 3 |
| -110 ≤ ecio < -90 | 4 |
| ecio ≥ -90 | 5 |

**最终值**：取 min(rssi_level, ecio_level)

#### 2.2.2 HDR 网络制式信号强度转换

获取 rssi 和 sinr 两个参数，分别转换，取较小者。

**rssi → 信号格数**：
| rssi 范围 | 信号格数 |
|---------|--------|
| rssi < -105 | 1 |
| -105 ≤ rssi < -90 | 2 |
| -90 ≤ rssi < -75 | 3 |
| -75 ≤ rssi < -65 | 4 |
| rssi ≥ -65 | 5 |

**sinr → 信号格数**：
| sinr 范围 | 信号格数 |
|---------|--------|
| sinr < 1 | 1 |
| 1 ≤ sinr < 3 | 2 |
| 3 ≤ sinr < 5 | 3 |
| 5 ≤ sinr < 7 | 4 |
| sinr ≥ 7 | 5 |

**最终值**：取 min(rssi_level, sinr_level)

#### 2.2.3 LTE 网络制式信号强度转换

优先使用 rsrp 转换（若结果合法 1~5），否则用 rssi 转 ASU 再转信号格数。

**rsrp → 信号格数**：
| rsrp 范围 | 信号格数 |
|---------|--------|
| rsrp < -115 | 1 |
| -115 ≤ rsrp < -105 | 2 |
| -105 ≤ rsrp < -95 | 3 |
| -95 ≤ rsrp < -85 | 4 |
| -85 ≤ rsrp < -44 | 5 |
| rsrp ≥ -44 | 0（无效，使用 ASU） |

**rssi → ASU → 信号格数**（ASU = (rssi + 113) / 2）：
| ASU 范围 | 信号格数 |
|---------|--------|
| 0 ≤ ASU < 5 | 2 |
| 5 ≤ ASU < 8 | 3 |
| 8 ≤ ASU < 12 | 4 |
| 12 ≤ ASU ≤ 63 | 5 |
| ASU > 63 | 1 |

#### 2.2.4 其他网络制式（CDMA2000/WCDMA/TD-SCDMA/GSM）

统一使用 rssi → ASU → 信号格数（ASU = (rssi + 113) / 2）：

| ASU 范围 | 信号格数 |
|---------|--------|
| ASU ≤ 2 或 ASU == 99 | 1 |
| 2 < ASU < 5 | 2 |
| 5 ≤ ASU < 8 | 3 |
| 8 ≤ ASU < 12 | 4 |
| ASU ≥ 12（且不等于99） | 5 |

---

## 3. 注网信息 API 详解

### 3.1 头文件

```c
#include "ql_mcm_nw.h"
```

头文件路径：`ql-ol-sdk/ql-ol-crosstool/sysroots/armv7a-vfp-neon-oe-linux-gnueabi/usr/include/quectel-openlinux-sdk/ql_mcm_nw.h`

### 3.2 参考示例

示例代码文件：`test_nw.c`  
路径：`ql-ol-sdk/ql-ol-extsdk/example/test_mcm_api/test_nw.c`

### 3.3 函数概览

| 函数名 | 功能说明 |
|-------|---------|
| `QL_MCM_NW_Client_Init()` | 初始化注网服务 |
| `QL_MCM_NW_Client_Deinit()` | 注销注网服务 |
| `QL_MCM_NW_SetConfig()` | 设置首选网络制式和漫游通知 |
| `QL_MCM_NW_GetConfig()` | 获取首选网络制式和漫游通知状态 |
| `QL_MCM_NW_GetNitzTimeInfo()` | 获取网络时间 |
| `QL_MCM_NW_EventRegister()` | 注册网络事件 |
| `QL_MCM_NW_GetOperatorName()` | 获取运营商信息 |
| `QL_MCM_NW_PerformScan()` | 触发网络扫描 |
| `QL_MCM_NW_GetRegStatus()` | 获取注网状态信息 |
| `QL_MCM_NW_SetLowPowerMode()` | 设置功耗模式 |
| `QL_MCM_NW_GetSignalStrength()` | 获取信号强度 |
| `QL_MCM_NW_GetCellAccessState()` | 获取小区访问状态 |
| `QL_MCM_NW_AddRxMsgHandler()` | 设置网络事件的回调函数 |

> **重要限制**：若无特别说明，所有注网信息 API 函数均**不支持并发调用**，并且**不可在任何回调函数中调用**所述函数，否则会对后续消息的处理造成影响。

### 3.4 函数详解

---

### `QL_MCM_NW_Client_Init`

#### 功能描述

初始化注网服务，获取服务句柄。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_NW_Client_Init(nw_client_handle_type *ph_nw);
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `ph_nw` | `nw_client_handle_type *` | 输出（Out） | 注网服务句柄指针，成功后存储有效句柄 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 初始化注网服务成功 |
| 其他值 | 初始化注网服务失败，错误码详见 `ql_mcm.h` |

#### 注意事项

- 使用其他注网信息接口函数前，**必须**先调用本函数以初始化注网服务。

---

### `QL_MCM_NW_Client_Deinit`

#### 功能描述

注销注网服务，释放资源。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_NW_Client_Deinit(nw_client_handle_type ph_nw);
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `ph_nw` | `nw_client_handle_type` | 输入（In） | 注网服务句柄（由 `QL_MCM_NW_Client_Init()` 返回） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 注销注网服务成功 |
| 其他值 | 注销注网服务失败，错误码详见 `ql_mcm.h` |

---

### `QL_MCM_NW_SetConfig`

#### 功能描述

设置首选网络制式，以及是否开启漫游通知。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_NW_SetConfig(
    nw_client_handle_type     h_nw,
    QL_MCM_NW_CONFIG_INFO_T   *pt_info
);
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_nw` | `nw_client_handle_type` | 输入（In） | 注网服务句柄 |
| `pt_info` | `QL_MCM_NW_CONFIG_INFO_T *` | 输入（In） | 首选网络制式和漫游通知配置 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 设置成功 |
| 其他值 | 设置失败，错误码详见 `ql_mcm.h` |

#### 注意事项

1. 该函数设置的参数会**断电保存**（掉电后仍有效）。
2. 若模块未搜到设置的首选网络制式，则会选择其他网络尝试注册。
3. 若已设置漫游开启，则模块注网时首选支持漫游功能的网络。

---

### `QL_MCM_NW_CONFIG_INFO_T`（数据结构）

首选网络制式和漫游通知配置结构体。

```c
typedef struct
{
    uint64_t                      preferred_nw_mode;
    E_QL_MCM_NW_ROAM_STATE_TYPE_T roaming_pref;
} QL_MCM_NW_CONFIG_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `uint64_t` | `preferred_nw_mode` | 首选网络制式，使用宏定义的位掩码（可按位OR组合多个） |
| `E_QL_MCM_NW_ROAM_STATE_TYPE_T` | `roaming_pref` | 漫游通知状态 |

---

### 首选网络制式宏定义

```c
#define QL_MCM_NW_MODE_NONE    0x00   // 不设置首选网络制式
#define QL_MCM_NW_MODE_GSM     0x01   // GSM
#define QL_MCM_NW_MODE_WCDMA   0x02   // WCDMA
#define QL_MCM_NW_MODE_CDMA    0x04   // CDMA
#define QL_MCM_NW_MODE_EVDO    0x08   // EVDO
#define QL_MCM_NW_MODE_LTE     0x10   // LTE
#define QL_MCM_NW_MODE_TDSCDMA 0x20   // TD-SCDMA
#define QL_MCM_NW_MODE_PRL     0x10000 // (U)SIM卡PRL文件中存储的网络制式
```

| 宏定义 | 值 | 描述 |
|-------|---|------|
| `QL_MCM_NW_MODE_NONE` | 0x00 | 不设置首选网络制式 |
| `QL_MCM_NW_MODE_GSM` | 0x01 | 设置首选网络制式为 GSM |
| `QL_MCM_NW_MODE_WCDMA` | 0x02 | 设置首选网络制式为 WCDMA |
| `QL_MCM_NW_MODE_CDMA` | 0x04 | 设置首选网络制式为 CDMA |
| `QL_MCM_NW_MODE_EVDO` | 0x08 | 设置首选网络制式为 EVDO |
| `QL_MCM_NW_MODE_LTE` | 0x10 | 设置首选网络制式为 LTE |
| `QL_MCM_NW_MODE_TDSCDMA` | 0x20 | 设置首选网络制式为 TD-SCDMA |
| `QL_MCM_NW_MODE_PRL` | 0x10000 | 设置首选网络制式为 (U)SIM 卡 PRL 文件中存储的网络制式 |

> **备注**：
> - 用户可以同时选择多个首选网络制式（按位OR组合）。
> - PRL 文件用于存放已与当前 (U)SIM 卡运营商签约的其他运营商信息，用于国际漫游场景。

---

### `E_QL_MCM_NW_ROAM_STATE_TYPE_T`（枚举）

漫游和漫游通知状态枚举。

```c
typedef enum
{
    E_QL_MCM_NW_ROAM_STATE_OFF = 0,  // 关闭
    E_QL_MCM_NW_ROAM_STATE_ON  = 1,  // 开启
} E_QL_MCM_NW_ROAM_STATE_TYPE_T;
```

---

### `QL_MCM_NW_GetConfig`

#### 功能描述

获取首选网络制式和漫游通知状态。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_NW_GetConfig(
    nw_client_handle_type     h_nw,
    QL_MCM_NW_CONFIG_INFO_T   *pt_info
);
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_nw` | `nw_client_handle_type` | 输入（In） | 注网服务句柄 |
| `pt_info` | `QL_MCM_NW_CONFIG_INFO_T *` | 输出（Out） | 首选网络制式和漫游通知状态 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 获取成功 |
| 其他值 | 获取失败，错误码详见 `ql_mcm.h` |

---

### `QL_MCM_NW_GetNitzTimeInfo`

#### 功能描述

获取网络时间（NITZ - Network Identity and Time Zone）。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_NW_GetNitzTimeInfo(
    nw_client_handle_type       h_nw,
    QL_MCM_NW_NITZ_TIME_INFO_T  *pt_info
);
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_nw` | `nw_client_handle_type` | 输入（In） | 注网服务句柄 |
| `pt_info` | `QL_MCM_NW_NITZ_TIME_INFO_T *` | 输出（Out） | 网络时间信息 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 获取成功 |
| 其他值 | 获取失败，错误码详见 `ql_mcm.h` |

---

### `QL_MCM_NW_NITZ_TIME_INFO_T`（数据结构）

网络时间信息结构体。

```c
typedef struct
{
    char     nitz_time[QL_MCM_NW_NITZ_BUF_LEN + 1];
    uint64_t abs_time;
    int8_t   leap_sec;
} QL_MCM_NW_NITZ_TIME_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `char[]` | `nitz_time` | UTC 时间字符串，格式：`YY/MM/DD,HH:MM:SS+/-TZ` |
| `uint64_t` | `abs_time` | 绝对时间，相对于 1970-01-01 00:00:00（UTC）的毫秒数 |
| `int8_t` | `leap_sec` | 闰秒（时间误差调整阈值） |

---

### `QL_MCM_NW_EventRegister`

#### 功能描述

注册各类网络事件（语音注册、数据注册、信号强度、小区访问状态变更、网络时间更新）。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_NW_EventRegister(
    nw_client_handle_type h_nw,
    uint32_t              bit_mask
);
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_nw` | `nw_client_handle_type` | 输入（In） | 注网服务句柄 |
| `bit_mask` | `uint32_t` | 输入（In） | 待注册的网络事件位掩码（可按位OR组合多个事件） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 注册成功 |
| 其他值 | 注册失败，错误码详见 `ql_mcm.h` |

---

### 网络事件宏定义

```c
#define NW_IND_VOICE_REG_EVENT_IND_FLAG          (1 << 0)  // 语音拨号注册事件
#define NW_IND_DATA_REG_EVENT_IND_FLAG           (1 << 1)  // 数据拨号注册事件
#define NW_IND_SIGNAL_STRENGTH_EVENT_IND_FLAG    (1 << 2)  // 信号强度事件
#define NW_IND_CELL_ACCESS_STATE_CHG_EVENT_IND_FLAG (1 << 3)  // 小区访问状态变更事件
#define NW_IND_NITZ_TIME_UPDATE_EVENT_IND_FLAG   (1 << 4)  // 网络时间更新事件
```

| 宏定义 | 值 | 描述 |
|-------|---|------|
| `NW_IND_VOICE_REG_EVENT_IND_FLAG` | bit0 | 语音拨号注册事件 |
| `NW_IND_DATA_REG_EVENT_IND_FLAG` | bit1 | 数据拨号注册事件 |
| `NW_IND_SIGNAL_STRENGTH_EVENT_IND_FLAG` | bit2 | 信号强度事件 |
| `NW_IND_CELL_ACCESS_STATE_CHG_EVENT_IND_FLAG` | bit3 | 小区访问状态变更事件 |
| `NW_IND_NITZ_TIME_UPDATE_EVENT_IND_FLAG` | bit4 | 网络时间更新事件 |

> **备注**：用户可以选择注册多个网络事件（按位OR组合）。

---

### `QL_MCM_NW_GetOperatorName`

#### 功能描述

获取当前注册的运营商信息（名称全称、简称、MCC、MNC）。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_NW_GetOperatorName(
    nw_client_handle_type            h_nw,
    QL_MCM_NW_OPERATOR_NAME_INFO_T   *pt_info
);
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_nw` | `nw_client_handle_type` | 输入（In） | 注网服务句柄 |
| `pt_info` | `QL_MCM_NW_OPERATOR_NAME_INFO_T *` | 输出（Out） | 运营商信息 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 获取成功 |
| 其他值 | 获取失败，错误码详见 `ql_mcm.h` |

---

### `QL_MCM_NW_OPERATOR_NAME_INFO_T`（数据结构）

运营商信息结构体。

```c
typedef struct
{
    char long_eons[512 + 1];  // 运营商名称全称
    char short_eons[512 + 1]; // 运营商名称简称
    char mcc[3 + 1];          // 移动设备国家代码
    char mnc[3 + 1];          // 移动设备网络代码
} QL_MCM_NW_OPERATOR_NAME_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `char[513]` | `long_eons` | 运营商名称全称（Long Enhanced Operator Name String） |
| `char[513]` | `short_eons` | 运营商名称简称（Short Enhanced Operator Name String） |
| `char[4]` | `mcc` | 移动设备国家代码（3位） |
| `char[4]` | `mnc` | 移动设备网络代码（2或3位） |

---

### `QL_MCM_NW_PerformScan`

#### 功能描述

触发网络扫描，扫描当前可用的所有网络。注意：网络扫描一般耗时较久，需等待扫描完成后方有结果返回。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_NW_PerformScan(
    nw_client_handle_type              h_nw,
    QL_MCM_NW_SCAN_RESULT_LIST_INFO_T  *pt_info
);
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_nw` | `nw_client_handle_type` | 输入（In） | 注网服务句柄 |
| `pt_info` | `QL_MCM_NW_SCAN_RESULT_LIST_INFO_T *` | 输出（Out） | 网络扫描结果列表（建议用 malloc 动态分配内存） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 扫描成功 |
| 其他值 | 扫描失败，错误码详见 `ql_mcm.h` |

---

### `QL_MCM_NW_SCAN_RESULT_LIST_INFO_T`（数据结构）

网络扫描结果列表结构体。

```c
typedef struct
{
    uint32_t                     entry_len;
    QL_MCM_NW_SCAN_ENTRY_INFO_T  entry[QL_MCM_NW_SCAN_LIST_MAX];
} QL_MCM_NW_SCAN_RESULT_LIST_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `uint32_t` | `entry_len` | 扫描到的网络数量 |
| `QL_MCM_NW_SCAN_ENTRY_INFO_T[]` | `entry` | 每个扫描到的网络信息 |

---

### `QL_MCM_NW_SCAN_ENTRY_INFO_T`（数据结构）

单个扫描网络的信息结构体。

```c
typedef struct
{
    QL_MCM_NW_OPERATOR_NAME_INFO_T    operator_name;
    E_QL_MCM_NW_NETWORK_STATUS_TYPE_T network_status;
    E_QL_MCM_NW_RADIO_TECH_TYPE_T     rat;
} QL_MCM_NW_SCAN_ENTRY_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `QL_MCM_NW_OPERATOR_NAME_INFO_T` | `operator_name` | 运营商信息（名称、MCC、MNC） |
| `E_QL_MCM_NW_NETWORK_STATUS_TYPE_T` | `network_status` | 网络状态（当前服务/首选/可用/禁用等） |
| `E_QL_MCM_NW_RADIO_TECH_TYPE_T` | `rat` | 无线接入技术（RAT）类型 |

---

### `E_QL_MCM_NW_NETWORK_STATUS_TYPE_T`（枚举）

网络状态枚举。

```c
typedef enum
{
    E_QL_MCM_NW_NETWORK_STATUS_NONE            = 0,
    E_QL_MCM_NW_NETWORK_STATUS_CURRENT_SERVING = 1,
    E_QL_MCM_NW_NETWORK_STATUS_PREFERRED       = 2,
    E_QL_MCM_NW_NETWORK_STATUS_NOT_PREFERRED   = 3,
    E_QL_MCM_NW_NETWORK_STATUS_AVAILABLE       = 4,
    E_QL_MCM_NW_NETWORK_STATUS_FORBIDDEN       = 5,
} E_QL_MCM_NW_NETWORK_STATUS_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `NONE` | 0 | 未知网络状态 |
| `CURRENT_SERVING` | 1 | 当前所注册的网络 |
| `PREFERRED` | 2 | 首选网络 |
| `NOT_PREFERRED` | 3 | 非首选网络 |
| `AVAILABLE` | 4 | 可用网络 |
| `FORBIDDEN` | 5 | 禁用网络 |

---

### `E_QL_MCM_NW_RADIO_TECH_TYPE_T`（枚举）

网络无线接入技术（RAT）类型枚举。

```c
typedef enum
{
    E_QL_MCM_NW_RADIO_TECH_TD_SCDMA = 1,
    E_QL_MCM_NW_RADIO_TECH_GSM      = 2,
    E_QL_MCM_NW_RADIO_TECH_HSPAP    = 3,
    E_QL_MCM_NW_RADIO_TECH_LTE      = 4,
    E_QL_MCM_NW_RADIO_TECH_EHRPD    = 5,
    E_QL_MCM_NW_RADIO_TECH_EVDO_B   = 6,
    E_QL_MCM_NW_RADIO_TECH_HSPA     = 7,
    E_QL_MCM_NW_RADIO_TECH_HSUPA    = 8,
    E_QL_MCM_NW_RADIO_TECH_HSDPA    = 9,
    E_QL_MCM_NW_RADIO_TECH_EVDO_A   = 10,
    E_QL_MCM_NW_RADIO_TECH_EVDO_0   = 11,
    E_QL_MCM_NW_RADIO_TECH_1xRTT    = 12,
    E_QL_MCM_NW_RADIO_TECH_IS95B    = 13,
    E_QL_MCM_NW_RADIO_TECH_IS95A    = 14,
    E_QL_MCM_NW_RADIO_TECH_UMTS     = 15,
    E_QL_MCM_NW_RADIO_TECH_EDGE     = 16,
    E_QL_MCM_NW_RADIO_TECH_GPRS     = 17,
    E_QL_MCM_NW_RADIO_TECH_NONE     = 18,
} E_QL_MCM_NW_RADIO_TECH_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `TD_SCDMA` | 1 | TD-SCDMA 网络 |
| `GSM` | 2 | GSM 网络 |
| `HSPAP` | 3 | HSPA+ 网络 |
| `LTE` | 4 | LTE 网络 |
| `EHRPD` | 5 | EHRPD 网络 |
| `EVDO_B` | 6 | EVDO_B 网络 |
| `HSPA` | 7 | HSPA 网络 |
| `HSUPA` | 8 | HSUPA 网络 |
| `HSDPA` | 9 | HSDPA 网络 |
| `EVDO_A` | 10 | EVDO_A 网络 |
| `EVDO_0` | 11 | EVDO_0 网络 |
| `1xRTT` | 12 | 1xRTT 网络 |
| `IS95B` | 13 | IS-95B 网络 |
| `IS95A` | 14 | IS-95A 网络 |
| `UMTS` | 15 | UMTS 网络 |
| `EDGE` | 16 | EDGE 网络 |
| `GPRS` | 17 | GPRS 网络 |
| `NONE` | 18 | 未知网络 |

---

### `QL_MCM_NW_GetRegStatus`

#### 功能描述

获取模块的注网状态信息，包括语音拨号和数据拨号时的注网状态（3GPP 和 3GPP2 协议分别报告）。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_NW_GetRegStatus(
    nw_client_handle_type        h_nw,
    QL_MCM_NW_REG_STATUS_INFO_T  *pt_info
);
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_nw` | `nw_client_handle_type` | 输入（In） | 注网服务句柄 |
| `pt_info` | `QL_MCM_NW_REG_STATUS_INFO_T *` | 输出（Out） | 注网状态信息（语音/数据 × 3GPP/3GPP2） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 获取成功 |
| 其他值 | 获取失败，错误码详见 `ql_mcm.h` |

---

### `QL_MCM_NW_REG_STATUS_INFO_T`（数据结构）

注网状态信息结构体（包含语音/数据的 3GPP 和 3GPP2 详细信息）。

```c
typedef struct
{
    uint8_t                     voice_registration_valid;
    QL_MCM_NW_COMMON_REG_INFO_T voice_registration;
    uint8_t                     data_registration_valid;
    QL_MCM_NW_COMMON_REG_INFO_T data_registration;
    uint8_t                     voice_registration_details_3gpp_valid;
    QL_MCM_NW_3GPP_REG_INFO_T   voice_registration_details_3gpp;
    uint8_t                     data_registration_details_3gpp_valid;
    QL_MCM_NW_3GPP_REG_INFO_T   data_registration_details_3gpp;
    uint8_t                     voice_registration_details_3gpp2_valid;
    QL_MCM_NW_3GPP2_REG_INFO_T  voice_registration_details_3gpp2;
    uint8_t                     data_registration_details_3gpp2_valid;
    QL_MCM_NW_3GPP2_REG_INFO_T  data_registration_details_3gpp2;
} QL_MCM_NW_REG_STATUS_INFO_T;
```

| 字段 | 说明 |
|-----|------|
| `voice_registration_valid` | 语音注册公共信息是否有效 |
| `voice_registration` | 语音拨号注网状态（公共信息） |
| `data_registration_valid` | 数据注册公共信息是否有效 |
| `data_registration` | 数据拨号注网状态（公共信息） |
| `voice_registration_details_3gpp_valid` | 3GPP 语音详细信息是否有效 |
| `voice_registration_details_3gpp` | 3GPP 语音注网详细信息 |
| `data_registration_details_3gpp_valid` | 3GPP 数据详细信息是否有效 |
| `data_registration_details_3gpp` | 3GPP 数据注网详细信息 |
| `voice_registration_details_3gpp2_valid` | 3GPP2 语音详细信息是否有效 |
| `voice_registration_details_3gpp2` | 3GPP2 语音注网详细信息 |
| `data_registration_details_3gpp2_valid` | 3GPP2 数据详细信息是否有效 |
| `data_registration_details_3gpp2` | 3GPP2 数据注网详细信息 |

---

### `QL_MCM_NW_COMMON_REG_INFO_T`（数据结构）

语音或数据拨号的注网状态公共信息结构体。

```c
typedef struct
{
    E_QL_MCM_NW_TECH_DOMAIN_TYPE_T  tech_domain;
    E_QL_MCM_NW_RADIO_TECH_TYPE_T   radio_tech;
    E_QL_MCM_NW_ROAM_STATE_TYPE_T   roaming;
    E_QL_MCM_NW_DENY_REASON_TYPE_T  deny_reason;
    E_QL_MCM_NW_SERVICE_TYPE_T      registration_state;
} QL_MCM_NW_COMMON_REG_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `E_QL_MCM_NW_TECH_DOMAIN_TYPE_T` | `tech_domain` | 协议类型（3GPP/3GPP2/未知） |
| `E_QL_MCM_NW_RADIO_TECH_TYPE_T` | `radio_tech` | 无线接入技术类型 |
| `E_QL_MCM_NW_ROAM_STATE_TYPE_T` | `roaming` | 漫游状态 |
| `E_QL_MCM_NW_DENY_REASON_TYPE_T` | `deny_reason` | 注网被拒原因 |
| `E_QL_MCM_NW_SERVICE_TYPE_T` | `registration_state` | 网络服务类型（无服务/受限/正常） |

---

### `QL_MCM_NW_3GPP_REG_INFO_T`（数据结构）

3GPP 协议标准的注网状态详细信息结构体。

```c
typedef struct
{
    E_QL_MCM_NW_TECH_DOMAIN_TYPE_T  tech_domain;
    E_QL_MCM_NW_RADIO_TECH_TYPE_T   radio_tech;
    char                            mcc[3+1];
    char                            mnc[3+1];
    E_QL_MCM_NW_ROAM_STATE_TYPE_T   roaming;
    uint8_t                         forbidden;
    uint32_t                        cid;
    uint16_t                        lac;
    uint16_t                        psc;
    uint16_t                        tac;
} QL_MCM_NW_3GPP_REG_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `E_QL_MCM_NW_TECH_DOMAIN_TYPE_T` | `tech_domain` | 协议类型 |
| `E_QL_MCM_NW_RADIO_TECH_TYPE_T` | `radio_tech` | 无线接入技术类型 |
| `char[4]` | `mcc` | 移动设备国家代码 |
| `char[4]` | `mnc` | 移动设备网络代码 |
| `E_QL_MCM_NW_ROAM_STATE_TYPE_T` | `roaming` | 漫游状态 |
| `uint8_t` | `forbidden` | 网络禁用标志 |
| `uint32_t` | `cid` | 小区 ID 码 |
| `uint16_t` | `lac` | 位置区码（Location Area Code） |
| `uint16_t` | `psc` | 主扰码（Primary Scrambling Code） |
| `uint16_t` | `tac` | 追踪区域码（Tracking Area Code） |

---

### `QL_MCM_NW_3GPP2_REG_INFO_T`（数据结构）

3GPP2 协议标准的注网状态详细信息结构体。

```c
typedef struct
{
    E_QL_MCM_NW_TECH_DOMAIN_TYPE_T  tech_domain;
    E_QL_MCM_NW_RADIO_TECH_TYPE_T   radio_tech;
    char                            mcc[3+1];
    char                            mnc[3+1];
    E_QL_MCM_NW_ROAM_STATE_TYPE_T   roaming;
    uint8_t                         forbidden;
    uint8_t                         inPRL;
    uint8_t                         css;
    uint16_t                        sid;
    uint16_t                        nid;
    uint16_t                        bsid;
} QL_MCM_NW_3GPP2_REG_INFO_T;
```

| 类型 | 字段 | 说明 |
|-----|-----|------|
| `E_QL_MCM_NW_TECH_DOMAIN_TYPE_T` | `tech_domain` | 协议类型 |
| `E_QL_MCM_NW_RADIO_TECH_TYPE_T` | `radio_tech` | 无线接入技术类型 |
| `char[4]` | `mcc` | 移动设备国家代码 |
| `char[4]` | `mnc` | 移动设备网络代码 |
| `E_QL_MCM_NW_ROAM_STATE_TYPE_T` | `roaming` | 漫游状态 |
| `uint8_t` | `forbidden` | 网络禁用标志 |
| `uint8_t` | `inPRL` | 优先漫游标志 |
| `uint8_t` | `css` | 并发支持标志 |
| `uint16_t` | `sid` | 系统 ID 码 |
| `uint16_t` | `nid` | 网络 ID 码 |
| `uint16_t` | `bsid` | 基站 ID 码 |

---

### `E_QL_MCM_NW_TECH_DOMAIN_TYPE_T`（枚举）

协议类型枚举。

```c
typedef enum
{
    E_QL_MCM_NW_TECH_DOMAIN_NONE  = 0,
    E_QL_MCM_NW_TECH_DOMAIN_3GPP  = 1,
    E_QL_MCM_NW_TECH_DOMAIN_3GPP2 = 2,
} E_QL_MCM_NW_TECH_DOMAIN_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `NONE` | 0 | 协议类型不可知 |
| `3GPP` | 1 | 3GPP 协议 |
| `3GPP2` | 2 | 3GPP2 协议 |

---

### `E_QL_MCM_NW_SERVICE_TYPE_T`（枚举）

网络服务类型枚举。

```c
typedef enum
{
    E_QL_MCM_NW_SERVICE_NONE    = 0x0000,  // 无服务
    E_QL_MCM_NW_SERVICE_LIMITED = 0x0001,  // 受限服务
    E_QL_MCM_NW_SERVICE_FULL    = 0x0002,  // 正常服务
} E_QL_MCM_NW_SERVICE_TYPE_T;
```

---

### `E_QL_MCM_NW_DENY_REASON_TYPE_T`（枚举）

注网被拒原因枚举（共 25 个值，1~25）。

```c
typedef enum
{
    E_QL_MCM_NW_IMSI_UNKNOWN_HLR_DENY_REASON                 = 1,
    E_QL_MCM_NW_ILLEGAL_MS_DENY_REASON                        = 2,
    E_QL_MCM_NW_IMSI_UNKNOWN_VLR_DENY_REASON                 = 3,
    E_QL_MCM_NW_IMEI_NOT_ACCEPTED_DENY_REASON                = 4,
    E_QL_MCM_NW_ILLEGAL_ME_DENY_REASON                       = 5,
    E_QL_MCM_NW_PLMN_NOT_ALLOWED_DENY_REASON                 = 6,
    E_QL_MCM_NW_LA_NOT_ALLOWED_DENY_REASON                   = 7,
    E_QL_MCM_NW_ROAMING_NOT_ALLOWED_LA_DENY_REASON           = 8,
    E_QL_MCM_NW_NO_SUITABLE_CELLS_LA_DENY_REASON             = 9,
    E_QL_MCM_NW_NETWORK_FAILURE_DENY_REASON                  = 10,
    E_QL_MCM_NW_MAC_FAILURE_DENY_REASON                      = 11,
    E_QL_MCM_NW_SYNCH_FAILURE_DENY_REASON                    = 12,
    E_QL_MCM_NW_CONGESTION_DENY_REASON                       = 13,
    E_QL_MCM_NW_GSM_AUTHENTICATION_UNACCEPTABLE_DENY_REASON  = 14,
    E_QL_MCM_NW_NOT_AUTHORIZED_CSG_DENY_REASON               = 15,
    E_QL_MCM_NW_SERVICE_OPTION_NOT_SUPPORTED_DENY_REASON     = 16,
    E_QL_MCM_NW_REQ_SERVICE_OPTION_NOT_SUBSCRIBED_DENY_REASON= 17,
    E_QL_MCM_NW_CALL_CANNOT_BE_IDENTIFIED_DENY_REASON        = 18,
    E_QL_MCM_NW_SEMANTICALLY_INCORRECT_MSG_DENY_REASON       = 19,
    E_QL_MCM_NW_INVALID_MANDATORY_INFO_DENY_REASON           = 20,
    E_QL_MCM_NW_MSG_TYPE_NON_EXISTENT_DENY_REASON            = 21,
    E_QL_MCM_NW_INFO_ELEMENT_NON_EXISTENT_DENY_REASON        = 22,
    E_QL_MCM_NW_CONDITIONAL_IE_ERR_DENY_REASON               = 23,
    E_QL_MCM_NW_MSG_INCOMPATIBLE_PROTOCOL_STATE_DENY_REASON  = 24,
    E_QL_MCM_NW_PROTOCOL_ERROR_DENY_REASON                   = 25,
} E_QL_MCM_NW_DENY_REASON_TYPE_T;
```

| 值 | 成员（简写） | 描述 |
|---|-----------|------|
| 1 | IMSI_UNKNOWN_HLR | HLR 中未知 IMSI |
| 2 | ILLEGAL_MS | 非法移动站点 |
| 3 | IMSI_UNKNOWN_VLR | VLR 中未知 IMSI |
| 4 | IMEI_NOT_ACCEPTED | IMEI 不合法 |
| 5 | ILLEGAL_ME | 非法移动设备 |
| 6 | PLMN_NOT_ALLOWED | PLMN 不合法 |
| 7 | LA_NOT_ALLOWED | 位置不合法 |
| 8 | ROAMING_NOT_ALLOWED_LA | 不接受漫游 |
| 9 | NO_SUITABLE_CELLS_LA | 找不到合适的小区 |
| 10 | NETWORK_FAILURE | 网络失败 |
| 11 | MAC_FAILURE | MAC 地址失败 |
| 12 | SYNCH_FAILURE | 同步失败 |
| 13 | CONGESTION | 拥堵 |
| 14 | GSM_AUTHENTICATION_UNACCEPTABLE | GSM 认证失败 |
| 15 | NOT_AUTHORIZED_CSG | 未认证的 CSG |
| 16 | SERVICE_OPTION_NOT_SUPPORTED | 不支持的服务选项 |
| 17 | REQ_SERVICE_OPTION_NOT_SUBSCRIBED | 未被订阅的服务选项 |
| 18 | CALL_CANNOT_BE_IDENTIFIED | 拨号无法识别 |
| 19 | SEMANTICALLY_INCORRECT_MSG | 语义错误消息 |
| 20 | INVALID_MANDATORY_INFO | 无效强制信息 |
| 21 | MSG_TYPE_NON_EXISTENT | 消息类型不存在 |
| 22 | INFO_ELEMENT_NON_EXISTENT | 信息元素不存在 |
| 23 | CONDITIONAL_IE_ERR | IE 错误 |
| 24 | MSG_INCOMPATIBLE_PROTOCOL_STATE | 消息与呼叫状态不兼容 |
| 25 | PROTOCOL_ERROR | 协议错误 |

---

### `QL_MCM_NW_SetLowPowerMode`

#### 功能描述

设置模块功耗模式，即控制是否关闭信号强度事件上报。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_NW_SetLowPowerMode(
    nw_client_handle_type h_nw,
    uint32_t              low_power_mode_on
);
```

#### 参数说明

| 参数名 | 类型 | 方向 | 取值 | 说明 |
|-------|------|------|-----|------|
| `h_nw` | `nw_client_handle_type` | 输入（In） | — | 注网服务句柄 |
| `low_power_mode_on` | `uint32_t` | 输入（In） | 0 或 1 | 0=正常模式（打开信号强度上报），1=低功耗模式（关闭信号强度上报） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 设置成功 |
| 其他值 | 设置失败，错误码详见 `ql_mcm.h` |

#### 注意事项

1. 该函数设置的参数，**断电后不保存**（掉电后恢复默认正常模式）。
2. 该函数设置的参数，**对所有客户端均有效**。

---

### `QL_MCM_NW_GetSignalStrength`

#### 功能描述

获取信号强度信息。该函数仅返回当前 (U)SIM 卡注册到的网络信号强度信息。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_NW_GetSignalStrength(
    nw_client_handle_type            h_nw,
    QL_MCM_NW_SIGNAL_STRENGTH_INFO_T *pt_info
);
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_nw` | `nw_client_handle_type` | 输入（In） | 注网服务句柄 |
| `pt_info` | `QL_MCM_NW_SIGNAL_STRENGTH_INFO_T *` | 输出（Out） | 信号强度信息（按网络制式分类） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 获取成功 |
| 其他值 | 获取失败，错误码详见 `ql_mcm.h` |

#### 注意事项

- 移动、联通卡仅注册到一个网络，该接口仅返回一种信号强度。
- 电信卡可能注册到 2 个网络（LTE + CDMA），会返回两种信号强度。

---

### `QL_MCM_NW_SIGNAL_STRENGTH_INFO_T`（数据结构）

信号强度信息结构体（包含各网络制式的信号强度，通过 valid 字段判断是否有效）。

```c
typedef struct
{
    uint8_t                       gsm_sig_info_valid;
    QL_MCM_NW_GSM_SIGNAL_INFO_T   gsm_sig_info;
    uint8_t                       wcdma_sig_info_valid;
    QL_MCM_NW_WCDMA_SIGNAL_INFO_T wcdma_sig_info;
    uint8_t                       tdscdma_sig_info_valid;
    QL_MCM_NW_TDSCDMA_SIGNAL_INFO_T tdscdma_sig_info;
    uint8_t                       lte_sig_info_valid;
    QL_MCM_NW_LTE_SIGNAL_INFO_T   lte_sig_info;
    uint8_t                       cdma_sig_info_valid;
    QL_MCM_NW_CDMA_SIGNAL_INFO_T  cdma_sig_info;
    uint8_t                       hdr_sig_info_valid;
    QL_MCM_NW_HDR_SIGNAL_INFO_T   hdr_sig_info;
} QL_MCM_NW_SIGNAL_STRENGTH_INFO_T;
```

---

### 各网络制式信号强度子结构体

**`QL_MCM_NW_GSM_SIGNAL_INFO_T`**（GSM）：
```c
typedef struct {
    int8_t rssi;  // RSSI，单位：dBm
} QL_MCM_NW_GSM_SIGNAL_INFO_T;
```

**`QL_MCM_NW_WCDMA_SIGNAL_INFO_T`**（WCDMA）：
```c
typedef struct {
    int8_t  rssi;  // RSSI，单位：dBm
    int16_t ecio;  // Ec/Io（载干比），单位：-0.5 dB
} QL_MCM_NW_WCDMA_SIGNAL_INFO_T;
```

**`QL_MCM_NW_TDSCDMA_SIGNAL_INFO_T`**（TD-SCDMA）：
```c
typedef struct {
    int8_t  rssi;  // RSSI，单位：dBm
    int8_t  rscp;  // RSCP（接收信号码功率），单位：dBm
    int16_t ecio;  // Ec/Io，单位：dB
    int8_t  sinr;  // SINR（信干噪比），单位：dB
} QL_MCM_NW_TDSCDMA_SIGNAL_INFO_T;
```

**`QL_MCM_NW_LTE_SIGNAL_INFO_T`**（LTE）：
```c
typedef struct {
    int8_t  rssi;  // RSSI，单位：dBm
    int8_t  rsrq;  // RSRQ（参考信号接收质量），单位：dB
    int16_t rsrp;  // RSRP（参考信号接收功率），单位：dBm
    int16_t snr;   // SNR（信噪比），单位：0.1 dB
} QL_MCM_NW_LTE_SIGNAL_INFO_T;
```

**`QL_MCM_NW_CDMA_SIGNAL_INFO_T`**（CDMA）：
```c
typedef struct {
    int8_t  rssi;  // RSSI，单位：dBm
    int16_t ecio;  // Ec/Io，单位：-0.5 dB
} QL_MCM_NW_CDMA_SIGNAL_INFO_T;
```

**`QL_MCM_NW_HDR_SIGNAL_INFO_T`**（HDR）：
```c
typedef struct {
    int8_t  rssi;  // RSSI，单位：dBm
    int16_t ecio;  // Ec/Io，单位：-0.5 dB
    int8_t  sinr;  // SINR 等级，取值范围 0~8（各等级对应最大 SINR：0=-9dB ... 8=9dB）
    int32_t io;    // 其他小区干扰能量，单位：dBm（仅适用于 1x EVDO）
} QL_MCM_NW_HDR_SIGNAL_INFO_T;
```

**HDR SINR 等级对应表**：

| SINR 等级 | 最大 SINR |
|---------|---------|
| 0 | -9 dB |
| 1 | -6 dB |
| 2 | -4.5 dB |
| 3 | -3 dB |
| 4 | -2 dB |
| 5 | 1 dB |
| 6 | 3 dB |
| 7 | 6 dB |
| 8 | 9 dB |

---

### `QL_MCM_NW_GetCellAccessState`

#### 功能描述

获取当前小区的访问状态。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_NW_GetCellAccessState(
    nw_client_handle_type              h_nw,
    E_QL_MCM_NW_CELL_ACCESS_STATE_TYPE_T *pe_state
);
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_nw` | `nw_client_handle_type` | 输入（In） | 注网服务句柄 |
| `pe_state` | `E_QL_MCM_NW_CELL_ACCESS_STATE_TYPE_T *` | 输出（Out） | 小区访问状态 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 获取成功 |
| 其他值 | 获取失败，错误码详见 `ql_mcm.h` |

---

### `E_QL_MCM_NW_CELL_ACCESS_STATE_TYPE_T`（枚举）

小区访问状态枚举。

```c
typedef enum
{
    E_QL_MCM_NW_CELL_ACCESS_NONE           = 0x00,
    E_QL_MCM_NW_CELL_ACCESS_NORMAL_ONLY    = 0x01,
    E_QL_MCM_NW_CELL_ACCESS_EMERGENCY_ONLY = 0x02,
    E_QL_MCM_NW_CELL_ACCESS_NO_CALLS       = 0x03,
    E_QL_MCM_NW_CELL_ACCESS_ALL_CALLS      = 0x04,
} E_QL_MCM_NW_CELL_ACCESS_STATE_TYPE_T;
```

| 成员 | 值 | 描述 |
|-----|---|------|
| `NONE` | 0x00 | 未知访问状态 |
| `NORMAL_ONLY` | 0x01 | 正常访问状态 |
| `EMERGENCY_ONLY` | 0x02 | 紧急访问状态（仅允许紧急呼叫） |
| `NO_CALLS` | 0x03 | 无法访问状态（不允许任何呼叫） |
| `ALL_CALLS` | 0x04 | 全访问状态（允许所有呼叫） |

---

### `QL_MCM_NW_AddRxMsgHandler`

#### 功能描述

设置网络事件的回调函数。当通过 `QL_MCM_NW_EventRegister()` 注册的事件发生时，自动调用此回调函数。

#### 函数原型

```c
E_QL_ERROR_CODE_T QL_MCM_NW_AddRxMsgHandler(
    nw_client_handle_type         h_nw,
    QL_MCM_NW_RxMsgHandlerFunc_t  handlerPtr,
    void*                         contextPtr
);
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `h_nw` | `nw_client_handle_type` | 输入（In） | 注网服务句柄 |
| `handlerPtr` | `QL_MCM_NW_RxMsgHandlerFunc_t` | 输入（In） | 事件回调函数指针 |
| `contextPtr` | `void*` | 输入（In） | void 型指针，预留，暂未使用 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 设置成功 |
| 其他值 | 设置失败，错误码详见 `ql_mcm.h` |

---

## 4. 使用流程与示例代码

### 4.1 总体使用流程

```
程序启动
    │
    ▼
QL_MCM_NW_Client_Init(&h_nw)       ← 初始化注网服务
    │
    ▼
QL_MCM_NW_AddRxMsgHandler(h_nw, callback, NULL)  ← 设置事件回调
    │
    ▼
QL_MCM_NW_EventRegister(h_nw, mask)  ← 注册关注的网络事件
    │
    ▼
业务操作：
    ├─ QL_MCM_NW_GetRegStatus()      ← 查询注网状态
    ├─ QL_MCM_NW_GetSignalStrength() ← 查询信号强度
    ├─ QL_MCM_NW_GetOperatorName()   ← 查询运营商信息
    ├─ QL_MCM_NW_GetNitzTimeInfo()   ← 获取网络时间
    ├─ QL_MCM_NW_SetConfig()         ← 设置首选网络制式
    ├─ QL_MCM_NW_SetLowPowerMode()   ← 设置功耗模式
    └─ QL_MCM_NW_PerformScan()       ← 触发网络扫描
    │
    ▼
QL_MCM_NW_Client_Deinit(h_nw)      ← 注销注网服务
    │
    ▼
程序退出
```

### 4.2 初始化注网服务

```c
nw_client_handle_type h_nw;
E_QL_ERROR_CODE_T ret;

ret = QL_MCM_NW_Client_Init(&h_nw);
printf("QL_MCM_NW_Client_Init ret = %d\n", ret);
```

### 4.3 设置首选网络制式和漫游通知

```c
QL_MCM_NW_CONFIG_INFO_T t_info = {0};
// 设置首选 LTE + WCDMA 网络（按位OR）
t_info.preferred_nw_mode = QL_MCM_NW_MODE_LTE | QL_MCM_NW_MODE_WCDMA;
// 开启漫游
t_info.roaming_pref = E_QL_MCM_NW_ROAM_STATE_ON;

ret = QL_MCM_NW_SetConfig(h_nw, &t_info);
printf("QL_MCM_NW_SetConfig ret = %d\n", ret);
```

### 4.4 获取首选网络制式和漫游通知状态

```c
QL_MCM_NW_CONFIG_INFO_T t_info = {0};
ret = QL_MCM_NW_GetConfig(h_nw, &t_info);
printf("preferred_nw_mode=0x%X, roaming=%d\n", t_info.preferred_nw_mode, t_info.roaming_pref);
```

### 4.5 获取网络时间

```c
QL_MCM_NW_NITZ_TIME_INFO_T t_info;
ret = QL_MCM_NW_GetNitzTimeInfo(h_nw, &t_info);
printf("nitz_time=%s, abs_time=%lld, leap_sec=%d\n",
       t_info.nitz_time, t_info.abs_time, t_info.leap_sec);
```

### 4.6 注册网络事件

```c
// 注册语音注册事件 + 数据注册事件 + 信号强度事件
uint32_t mask = NW_IND_VOICE_REG_EVENT_IND_FLAG
              | NW_IND_DATA_REG_EVENT_IND_FLAG
              | NW_IND_SIGNAL_STRENGTH_EVENT_IND_FLAG;
ret = QL_MCM_NW_EventRegister(h_nw, mask);
printf("QL_MCM_NW_EventRegister ret = %d\n", ret);
```

### 4.7 获取运营商信息

```c
QL_MCM_NW_OPERATOR_NAME_INFO_T t_info;
ret = QL_MCM_NW_GetOperatorName(h_nw, &t_info);
printf("long_eons=%s, short_eons=%s, mcc=%s, mnc=%s\n",
       t_info.long_eons, t_info.short_eons, t_info.mcc, t_info.mnc);
```

### 4.8 触发网络扫描

```c
QL_MCM_NW_SCAN_RESULT_LIST_INFO_T *pt_info = NULL;
pt_info = (QL_MCM_NW_SCAN_RESULT_LIST_INFO_T *)malloc(sizeof(QL_MCM_NW_SCAN_RESULT_LIST_INFO_T));
memset(pt_info, 0, sizeof(QL_MCM_NW_SCAN_RESULT_LIST_INFO_T));

ret = QL_MCM_NW_PerformScan(h_nw, pt_info);
printf("PerformScan ret=%d, list_len=%d\n", ret, pt_info->entry_len);
for (int i = 0; i < pt_info->entry_len; i++) {
    printf("[%d]: %s, mcc=%s, mnc=%s, status=%d, rat=%d\n",
           i,
           pt_info->entry[i].operator_name.long_eons,
           pt_info->entry[i].operator_name.mcc,
           pt_info->entry[i].operator_name.mnc,
           pt_info->entry[i].network_status,
           pt_info->entry[i].rat);
}
free(pt_info);
```

### 4.9 获取注网状态信息

```c
QL_MCM_NW_REG_STATUS_INFO_T t_info;
memset(&t_info, 0, sizeof(QL_MCM_NW_REG_STATUS_INFO_T));
ret = QL_MCM_NW_GetRegStatus(h_nw, &t_info);

if (t_info.voice_registration_valid) {
    printf("语音注册: tech_domain=%d, radio_tech=%d, roaming=%d, state=%d\n",
           t_info.voice_registration.tech_domain,
           t_info.voice_registration.radio_tech,
           t_info.voice_registration.roaming,
           t_info.voice_registration.registration_state);
}
if (t_info.data_registration_details_3gpp_valid) {
    printf("数据注册(3GPP): mcc=%s, mnc=%s, cid=0x%X, lac=%d, tac=%d\n",
           t_info.data_registration_details_3gpp.mcc,
           t_info.data_registration_details_3gpp.mnc,
           t_info.data_registration_details_3gpp.cid,
           t_info.data_registration_details_3gpp.lac,
           t_info.data_registration_details_3gpp.tac);
}
```

### 4.10 设置功耗模式

```c
// 设置低功耗模式（关闭信号强度上报）
ret = QL_MCM_NW_SetLowPowerMode(h_nw, 1);
printf("QL_MCM_NW_SetLowPowerMode ret = %d\n", ret);

// 恢复正常模式（打开信号强度上报）
ret = QL_MCM_NW_SetLowPowerMode(h_nw, 0);
```

### 4.11 获取信号强度

```c
QL_MCM_NW_SIGNAL_STRENGTH_INFO_T t_info;
memset(&t_info, 0, sizeof(t_info));
ret = QL_MCM_NW_GetSignalStrength(h_nw, &t_info);

if (t_info.lte_sig_info_valid) {
    printf("LTE: rssi=%d, rsrq=%d, rsrp=%d, snr=%d\n",
           t_info.lte_sig_info.rssi,
           t_info.lte_sig_info.rsrq,
           t_info.lte_sig_info.rsrp,
           t_info.lte_sig_info.snr);
}
if (t_info.gsm_sig_info_valid) {
    printf("GSM: rssi=%d\n", t_info.gsm_sig_info.rssi);
}
```

### 4.12 获取小区访问状态

```c
E_QL_MCM_NW_CELL_ACCESS_STATE_TYPE_T e_state;
ret = QL_MCM_NW_GetCellAccessState(h_nw, &e_state);
printf("QL_MCM_NW_GetCellAccessState ret=%d, e_state=%d\n", ret, e_state);
```

### 4.13 设置网络事件回调函数

```c
// 回调函数定义（用户自行实现）
void nw_event_ind_handler(nw_client_handle_type h_nw, uint32_t msg_id,
                           void *ind_data, uint32_t ind_data_len, void *resp_cb_data)
{
    // 根据 msg_id 处理不同事件
}

// 注册回调
ret = QL_MCM_NW_AddRxMsgHandler(h_nw, nw_event_ind_handler, NULL);
printf("QL_MCM_NW_AddRxMsgHandler ret=%d\n", ret);
```

---

## 5. 错误码说明

| 返回值 | 含义 |
|-------|------|
| `E_QL_SUCCESS` | 执行成功 |
| 其他值 | 执行失败，需参考 `ql_mcm.h` 中的错误码定义 |

---

## 6. 注意事项与限制

1. **并发调用限制**：所有注网信息 API 函数均**不支持并发调用**。

2. **回调函数内禁止调用**：**不可在任何回调函数中调用**所述函数，否则会对后续消息的处理造成影响。

3. **生命周期管理**：
   - 使用任何注网信息接口函数前，必须先调用 `QL_MCM_NW_Client_Init()` 初始化服务
   - 程序退出前必须调用 `QL_MCM_NW_Client_Deinit()` 注销服务

4. **SetConfig 的断电保存特性**：`QL_MCM_NW_SetConfig()` 设置的参数**断电保存**，重启后仍有效。

5. **SetLowPowerMode 的非持久性**：`QL_MCM_NW_SetLowPowerMode()` 设置的参数**断电后不保存**，且对所有客户端均有效。

6. **信号强度上报的自动行为**：信号强度事件即便未通过 `QL_MCM_NW_EventRegister()` 注册，也会自动上报并唤醒模块，需通过 `QL_MCM_NW_SetLowPowerMode()` 关闭。

7. **网络扫描耗时**：`QL_MCM_NW_PerformScan()` 是阻塞操作，耗时较长，调用时需有足够的等待时间。

8. **首选网络制式的组合使用**：`preferred_nw_mode` 字段支持多个模式按位 OR 组合。

9. **注网状态结构体的 valid 字段**：使用 `QL_MCM_NW_REG_STATUS_INFO_T` 中的字段前，必须先检查对应的 `_valid` 字段是否非零。

---

## 7. 版本与兼容性信息

| 项目 | 信息 |
|-----|------|
| 文档版本 | V1.0 |
| 文档日期 | 2021-02-19 |
| 文档状态 | 受控文件 |
| 作者 | Tinker SUN |
| 适用模块 | EC2x（EC25/EC21/EC20 R2.1/EC20-CN）、EG9x（EG95/EG91）、EG25-G |
| 头文件 | `ql_mcm_nw.h` |
| 头文件路径 | `ql-ol-sdk/ql-ol-crosstool/sysroots/armv7a-vfp-neon-oe-linux-gnueabi/usr/include/quectel-openlinux-sdk/` |
| 示例代码路径 | `ql-ol-sdk/ql-ol-extsdk/example/test_mcm_api/test_nw.c` |
| 参考文档 | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_快速开发指导 |

---

## 8. 附录：术语缩写

| 缩写 | 英文全称 | 中文全称 |
|-----|---------|---------|
| 1xRTT | Single-Carrier Radio Transmission Technology | 单载波无线传输技术 |
| API | Application Programming Interface | 应用程序编程接口 |
| ARFCN | Absolute Radio-Frequency Channel Number | 绝对射频信道号 |
| ASU | Arbitrary Strength Unit | 主观强度单位 |
| CDMA | Code-Division Multiple Access | 码分多址 |
| CSG | Closed Subscriber Group | 闭合用户群 |
| EDGE | Enhanced Data Rates for GSM Evolution | 增强型数据速率 GSM 演进技术 |
| eHRPD | evolved High Rate Packet Data | 演进的高速分组网络 |
| GPRS | General Packet Radio Service | 通用无线分组业务 |
| GSM | Global System for Mobile Communications | 全球移动通信系统 |
| HDR | High Data Rate | 高速数据速率 |
| HLR | Home Location Register | 归属位置寄存器 |
| HSPA | High Speed Packet Access | 高速分组接入 |
| IMSI | International Mobile Subscriber Identity | 国际移动用户识别码 |
| LAC | Location Area Code | 位置区码 |
| LTE | Long Time Evolution | 长期演进 |
| MAC | Medium Access Control | 媒体访问控制 |
| MCC | Mobile Country Code | 移动设备国家代码 |
| MNC | Mobile Network Code | 移动设备网络代码 |
| PLMN | Public Land Mobile Network | 公共陆地移动网络 |
| PRL | Preferred Roaming List | 优选漫游列表 |
| PSC | Primary Scrambling Code | 主扰码 |
| RSCP | Received Signal Code Power | 接收信号码功率 |
| RSRP | Reference Signal Received Power | 参考信号接收功率 |
| RSRQ | Reference Signal Received Quality | 参考信号接收质量 |
| RSSI | Received Signal Strength Indicator | 接收信号强度指示 |
| SDK | Software Development Kit | 软件开发工具包 |
| SINR | Signal-to-Interference-plus-Noise Ratio | 信干噪比 |
| SNR | Signal-to-Noise Ratio | 信噪比 |
| TAC | Tracking Area Code | 跟踪区域码 |
| TD-SCDMA | Time Division-Synchronous Code Division Multiple Access | 时分-同步码分多址 |
| UMTS | Universal Mobile Telecommunications System | 通用移动通信系统 |
| VLR | Visitor Location Register | 访问位置寄存器 |

<!-- GENERATION_COMPLETE: 2026-05-28 -->
