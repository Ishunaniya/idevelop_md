# EC200A-CN(TA) QuecOpen 蜂窝网络信息 API 参考手册

> **文档版本**：V1.0.0 Preliminary  
> **发布日期**：2023-03-20  
> **适用平台**：EC200A-CN(TA)（QuecOpen 模式）  
> **关键字**：ql_nw、蜂窝网络、信号强度、注册状态、小区信息、NITZ、网络扫描、功耗模式

---

## 目录

- [1. 引言](#1-引言)
- [2. API 参考](#2-api-参考)
  - [2.1 头文件](#21-头文件)
  - [2.2 函数概览](#22-函数概览)
  - [2.3 函数详解](#23-函数详解)
    - [2.3.1 ql_nw_init](#231-ql_nw_init)
    - [2.3.2 ql_nw_network_scan](#232-ql_nw_network_scan)
    - [2.3.3 ql_nw_set_power_mode](#233-ql_nw_set_power_mode)
    - [2.3.4 ql_nw_set_pref_nwmode_roaming](#234-ql_nw_set_pref_nwmode_roaming)
    - [2.3.5 ql_nw_get_pref_nwmode_roaming](#235-ql_nw_get_pref_nwmode_roaming)
    - [2.3.6 ql_nw_get_mobile_operator_name](#236-ql_nw_get_mobile_operator_name)
    - [2.3.7 ql_nw_get_cell_info](#237-ql_nw_get_cell_info)
    - [2.3.8 ql_nw_get_voice_reg_status](#238-ql_nw_get_voice_reg_status)
    - [2.3.9 ql_nw_get_data_reg_status](#239-ql_nw_get_data_reg_status)
    - [2.3.10 ql_nw_get_signal_strength](#2310-ql_nw_get_signal_strength)
    - [2.3.11 ql_nw_get_cell_access_status](#2311-ql_nw_get_cell_access_status)
    - [2.3.12 ql_nw_get_nitz_time_info](#2312-ql_nw_get_nitz_time_info)
    - [2.3.13 ql_nw_set_voice_reg_ind_cb](#2313-ql_nw_set_voice_reg_ind_cb)
    - [2.3.14 ql_nw_set_data_reg_ind_cb](#2314-ql_nw_set_data_reg_ind_cb)
    - [2.3.15 ql_nw_set_signal_strength_ind_cb](#2315-ql_nw_set_signal_strength_ind_cb)
    - [2.3.16 ql_nw_set_cell_access_status_ind_cb](#2316-ql_nw_set_cell_access_status_ind_cb)
    - [2.3.17 ql_nw_set_nitz_time_update_ind_cb](#2317-ql_nw_set_nitz_time_update_ind_cb)
    - [2.3.18 ql_nw_set_service_error_cb](#2318-ql_nw_set_service_error_cb)
    - [2.3.19 ql_nw_deinit](#2319-ql_nw_deinit)
- [3. 示例](#3-示例)
  - [3.1 网络扫描](#31-网络扫描)
  - [3.2 设置功耗模式](#32-设置功耗模式)
  - [3.3 设置首选网络制式和漫游通知开启状态](#33-设置首选网络制式和漫游通知开启状态)
  - [3.4 获取首选网络制式和漫游通知开启状态](#34-获取首选网络制式和漫游通知开启状态)
  - [3.5 获取运营商信息](#35-获取运营商信息)
  - [3.6 获取小区信息](#36-获取小区信息)
  - [3.7 获取语音拨号注网状态](#37-获取语音拨号注网状态)
  - [3.8 获取数据拨号注网状态](#38-获取数据拨号注网状态)
  - [3.9 获取信号强度信息](#39-获取信号强度信息)
  - [3.10 获取小区访问状态信息](#310-获取小区访问状态信息)
  - [3.11 获取网络时间信息](#311-获取网络时间信息)
  - [3.12 注册语音拨号注网事件](#312-注册语音拨号注网事件)
  - [3.13 注册数据拨号注网事件](#313-注册数据拨号注网事件)
  - [3.14 注册信号强度事件](#314-注册信号强度事件)
  - [3.15 注册小区状态事件](#315-注册小区状态事件)
  - [3.16 注册网络时间事件](#316-注册网络时间事件)
  - [3.17 注册服务异常函数回调](#317-注册服务异常函数回调)
  - [3.18 去初始化网络服务](#318-去初始化网络服务)
- [4. 附录 参考文档及术语缩写](#4-附录-参考文档及术语缩写)

---

## 1. 引言

移远通信 EC200A-CN(TA) 模块支持 QuecOpen® 方案。QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。

本文档为 EC200A-CN(TA) QuecOpen 蜂窝网络信息 API 参考手册，介绍以下功能的 C API：

- 网络扫描（可用网络搜索）
- 功耗模式设置（RF 功能启用/禁用）
- 首选网络制式和漫游配置
- 运营商名称查询（PLMN 信息）
- 小区信息查询（GSM/UMTS/LTE/NR5G/CDMA）
- 语音/数据拨号注网状态查询及事件回调
- 信号强度查询及事件回调（RSSI/RSRP/RSRQ/SNR/SINR）
- 小区访问状态查询及事件回调
- 网络时间（NITZ）查询及事件回调
- 服务异常回调

---

## 2. API 参考

### 2.1 头文件

蜂窝网络信息 API 的头文件为 **`ql_nw.h`**，位于 `ql-sysroots/usr/include/ql-sdk` 目录下。若无特别说明，本文档所提到的头文件均在该目录下。

代码示例均来自 `ql-sdk/sample/test_sdk_api/m_nw.c`，此文件下可查看接口函数的完整示例。程序启动后，必须调用 **`ql_nw_init()`** 初始化网络服务。

---

### 2.2 函数概览

**表 1：函数概览**

| 函数 | 说明 |
|------|------|
| `ql_nw_init()` | 初始化网络服务 |
| `ql_nw_network_scan()` | 扫描可用网络（异步） |
| `ql_nw_set_power_mode()` | 设置功耗模式 |
| `ql_nw_set_pref_nwmode_roaming()` | 设置首选网络制式和漫游通知开启状态 |
| `ql_nw_get_pref_nwmode_roaming()` | 获取首选网络制式和漫游通知开启状态 |
| `ql_nw_get_mobile_operator_name()` | 获取运营商信息 |
| `ql_nw_get_cell_info()` | 获取小区信息 |
| `ql_nw_get_voice_reg_status()` | 获取语音拨号注网状态信息 |
| `ql_nw_get_data_reg_status()` | 获取数据拨号注网状态信息 |
| `ql_nw_get_signal_strength()` | 获取当前信号强度信息以及信号强度等级 |
| `ql_nw_get_cell_access_status()` | 获取当前小区访问状态 |
| `ql_nw_get_nitz_time_info()` | 获取网络时间信息 |
| `ql_nw_set_voice_reg_ind_cb()` | 注册语音拨号注网事件回调函数 |
| `ql_nw_set_data_reg_ind_cb()` | 注册数据拨号注网事件回调函数 |
| `ql_nw_set_signal_strength_ind_cb()` | 注册信号强度事件回调函数 |
| `ql_nw_set_cell_access_status_ind_cb()` | 注册小区访问状态事件回调函数 |
| `ql_nw_set_nitz_time_update_ind_cb()` | 注册网络时间事件回调函数 |
| `ql_nw_set_service_error_cb()` | 注册服务异常事件回调函数 |
| `ql_nw_deinit()` | 去初始化网络服务 |

---

### 2.3 函数详解

#### 2.3.1 ql_nw_init

该函数用于初始化网络服务。使用其他 `ql_nw` API 前，必须先调用此函数。

**函数原型**

```c
int ql_nw_init(void)
```

**参数**：无

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 2.3.2 ql_nw_network_scan

该函数用于扫描当前可用的网络（异步操作）。

**函数原型**

```c
int ql_nw_network_scan(int *p_async_id, ql_nw_network_scan_async_cb cb_func)
```

**参数**

- `p_async_id`：
  - [Out] 异步操作 ID，用于匹配回调
- `cb_func`：
  - [In] 扫描完成后的回调函数

**返回值**

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK`（0） | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

**QL_NW_RADIO_TECH_TYPE_E（网络制式枚举）**

```c
typedef enum QL_NW_RADIO_TECH_TYPE_ENUM
{
    QL_NW_RADIO_TECH_NO_SRV    = 0,
    QL_NW_RADIO_TECH_GSM       = 1,
    QL_NW_RADIO_TECH_GPRS      = 2,
    QL_NW_RADIO_TECH_EDGE      = 3,
    QL_NW_RADIO_TECH_WCDMA     = 4,
    QL_NW_RADIO_TECH_HSDPA     = 5,
    QL_NW_RADIO_TECH_HSUPA     = 6,
    QL_NW_RADIO_TECH_HSPA      = 7,
    QL_NW_RADIO_TECH_TDSCDMA   = 8,
    QL_NW_RADIO_TECH_EHRPD     = 9,
    QL_NW_RADIO_TECH_EVDO_REV_0 = 10,
    QL_NW_RADIO_TECH_EVDO_REV_A = 11,
    QL_NW_RADIO_TECH_EVDO_REV_B = 12,
    QL_NW_RADIO_TECH_1xRTT     = 13,
    QL_NW_RADIO_TECH_IS95A     = 14,
    QL_NW_RADIO_TECH_IS95B     = 15,
    QL_NW_RADIO_TECH_LTE       = 14,   /* 注：与 IS95A 值重叠 */
    QL_NW_RADIO_TECH_UMTS      = 17,
    QL_NW_RADIO_TECH_TD_HSDPA  = 18,
    QL_NW_RADIO_TECH_TD_HSUPA  = 19,
    QL_NW_RADIO_TECH_TD_HSPA   = 20,
    QL_NW_RADIO_TECH_TD_HSPAP  = 21,
    QL_NW_RADIO_TECH_LTE_CA    = 22,
    QL_NW_RADIO_TECH_NR5G      = 23,
} QL_NW_RADIO_TECH_TYPE_E
```

**QL_NW_NETWORK_STATUS_E（网络状态枚举）**

```c
typedef enum
{
    QL_NW_NETWORK_STATUS_UNKNOWN         = 0,
    QL_NW_NETWORK_STATUS_CURRENT_SERVING = 1,
    QL_NW_NETWORK_STATUS_AVAILABLE       = 2,
    QL_NW_NETWORK_STATUS_FORBIDDEN       = 3,
} QL_NW_NETWORK_STATUS_E
```

**ql_nw_scan_info_t（网络扫描结果结构体）**

```c
typedef struct
{
    QL_NW_NETWORK_STATUS_E  network_status;
    QL_NW_RADIO_TECH_TYPE_E radio_tech;
    char                    mcc[3+1];
    char                    mnc[3+1];
    char                    long_eons[...];    /* 运营商名称全称 */
    char                    short_eons[...];   /* 运营商名称简称 */
} ql_nw_scan_info_t
```

---

#### 2.3.3 ql_nw_set_power_mode

该函数用于设置网络功耗模式（控制 RF 功能的启用/禁用）。

**函数原型**

```c
int ql_nw_set_power_mode(uint32_t mode)
```

**参数**

- `mode`：
  - [In] 功耗模式掩码，为以下标志位的按位或组合：

| 标志位 | 说明 |
|--------|------|
| `QL_NW_POWER_MODE_VOICE` | 语音功能 |
| `QL_NW_POWER_MODE_SMS` | 短信功能 |
| `QL_NW_POWER_MODE_SIM` | SIM 卡功能 |
| `QL_NW_POWER_MODE_NETWORK` | 网络功能 |
| `QL_NW_POWER_MODE_NORMAL` | 正常模式（全部功能） |

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 2.3.4 ql_nw_set_pref_nwmode_roaming

该函数用于设置首选网络制式和漫游通知开启状态。

**函数原型**

```c
int ql_nw_set_pref_nwmode_roaming(ql_nw_pref_nwmode_roaming_info_t *p_info)
```

**参数**

- `p_info`：
  - [In] 首选网络制式和漫游配置信息

**ql_nw_pref_nwmode_roaming_info_t 结构体**

```c
typedef struct
{
    uint64_t preferred_nw_mode;    /* 首选网络制式掩码（TDSCDMA|LTE|EVDO|CDMA|WCDMA|GSM） */
    uint32_t preferred_roaming;    /* 漫游通知开启状态 */
} ql_nw_pref_nwmode_roaming_info_t
```

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 2.3.5 ql_nw_get_pref_nwmode_roaming

该函数用于获取首选网络制式和漫游通知开启状态。

**函数原型**

```c
int ql_nw_get_pref_nwmode_roaming(ql_nw_pref_nwmode_roaming_info_t *p_info)
```

**参数**

- `p_info`：
  - [Out] 首选网络制式和漫游配置信息，结构同 [2.3.4](#234-ql_nw_set_pref_nwmode_roaming)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 2.3.6 ql_nw_get_mobile_operator_name

该函数用于获取当前注册网络的运营商信息。

**函数原型**

```c
int ql_nw_get_mobile_operator_name(ql_nw_mobile_operator_name_info_t *p_info)
```

**参数**

- `p_info`：
  - [Out] 运营商信息

**ql_nw_mobile_operator_name_info_t 结构体**

```c
typedef struct
{
    char long_eons[...];    /* 运营商名称全称（Long Enhanced Operator Name String） */
    char short_eons[...];   /* 运营商名称简称（Short Enhanced Operator Name String） */
    char mcc[3+1];          /* 移动设备国家代码（Mobile Country Code） */
    char mnc[3+1];          /* 移动设备网络代码（Mobile Network Code） */
} ql_nw_mobile_operator_name_info_t
```

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 2.3.7 ql_nw_get_cell_info

该函数用于获取当前小区信息（支持 GSM/UMTS/LTE/NR5G/CDMA 多制式）。

**函数原型**

```c
int ql_nw_get_cell_info(ql_nw_cell_info_t *p_info)
```

**参数**

- `p_info`：
  - [Out] 小区信息

**ql_nw_cell_info_t 结构体**

```c
typedef struct
{
    uint8_t             gsm_info_valid;
    ql_nw_gsm_info_t    gsm_info[QL_NW_MAX_GSM_CELL_INFO_CNT];
    uint8_t             gsm_info_len;
    uint8_t             umts_info_valid;
    ql_nw_umts_info_t   umts_info[QL_NW_MAX_UMTS_CELL_INFO_CNT];
    uint8_t             umts_info_len;
    uint8_t             lte_info_valid;
    ql_nw_lte_info_t    lte_info[QL_NW_MAX_LTE_CELL_INFO_CNT];
    uint8_t             lte_info_len;
    uint8_t             nr5g_info_valid;
    ql_nw_nr5g_info_t   nr5g_info;             /* NR5G 单小区 */
    uint8_t             cdma_info_valid;
    ql_nw_cdma_info_t   cdma_info;             /* CDMA 单小区 */
} ql_nw_cell_info_t
```

**ql_nw_gsm_info_t（GSM 小区信息）**

```c
typedef struct
{
    uint32_t    cid;        /* 小区 ID 号 */
    char        plmn[3];    /* PLMN（可由 MCC/MNC 得出） */
    uint16_t    lac;        /* 位置区域码 */
    uint16_t    arfcn;      /* RF 信道号 */
    uint8_t     bsic;       /* 基站识别码 */
    int8_t      rssi;       /* 接收信号强度 */
} ql_nw_gsm_info_t
```

**ql_nw_umts_info_t（UMTS/WCDMA 小区信息）**

```c
typedef struct
{
    uint32_t    cid;        /* 小区 ID 号 */
    uint32_t    lcid;       /* 本地小区 ID */
    char        plmn[3];    /* PLMN */
    uint16_t    lac;        /* 位置区域码 */
    uint16_t    uarfcn;     /* UTRA 绝对射频信道号 */
    uint16_t    psc;        /* 主扰码（WCDMA 系统中使用） */
    int8_t      rssi;       /* 接收信号强度 */
} ql_nw_umts_info_t
```

**ql_nw_cdma_info_t（CDMA 小区信息）**

```c
typedef struct
{
    uint16_t    sid;        /* 系统 ID */
    uint16_t    nid;        /* 网络 ID */
    uint16_t    bid;        /* 基站 ID */
    uint16_t    refpn;      /* 参考 PN 偏移 */
    int32_t     base_lat;   /* 基站纬度 */
    int32_t     base_long;  /* 基站经度 */
    int8_t      rssi;       /* 接收信号强度 */
} ql_nw_cdma_info_t
```

**ql_nw_nr5g_info_t（NR5G 小区信息）**

```c
typedef struct
{
    uint64_t    cid;        /* 小区 ID 号（NR 为 64 位） */
    char        plmn[3];    /* PLMN */
    uint32_t    tac;        /* 跟踪区域码 */
    uint16_t    pci;        /* 物理小区 ID */
    uint32_t    arfcn;      /* RF 信道号 */
    int16_t     rsrp;       /* 参考信号接收功率（dBm） */
    int16_t     rsrq;       /* 参考信号接收质量（dB） */
    int16_t     snr;        /* 信噪比（0.1 dB） */
} ql_nw_nr5g_info_t
```

**ql_nw_lte_info_t（LTE 小区信息）**

```c
typedef struct
{
    uint32_t    cid;        /* 小区 ID 号 */
    char        plmn[3];    /* PLMN（可由 MCC/MNC 得出） */
    uint16_t    tac;        /* 追踪区域码 */
    uint16_t    pci;        /* 物理小区 ID */
    uint16_t    earfcn;     /* RF 信道号 */
    int16_t     rssi;       /* 接收信号强度 */
} ql_nw_lte_info_t
```

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 2.3.8 ql_nw_get_voice_reg_status

该函数用于获取语音拨号注网状态信息。

**函数原型**

```c
int ql_nw_get_voice_reg_status(ql_nw_reg_status_info_t *p_info)
```

**参数**

- `p_info`：
  - [Out] 获取到的语音拨号注网状态信息；详见 [2.3.8.1 ql_nw_reg_status_info_t](#2381-ql_nw_reg_status_info_t)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.8.1 ql_nw_reg_status_info_t

语音或数据拨号注网状态信息结构体定义如下：

```c
typedef struct
{
    QL_NW_TECH_DOMAIN_TYPE_E    tech_domain;
    QL_NW_RADIO_TECH_TYPE_E     radio_tech;
    QL_NW_ROAM_STATE_TYPE_E     roaming;
    QL_NW_DENY_REASON_TYPE_E    deny_reason;
    QL_NW_SERVICE_TYPE_E        reg_state;
    char                        mcc[3+1];
    char                        mnc[3+1];
    uint8_t                     forbidden;
    uint32_t                    cid;
    uint16_t                    lac;
    uint16_t                    psc;
    uint16_t                    tac;
    uint8_t                     inPRL;
    uint8_t                     css;
    uint16_t                    sid;
    uint16_t                    nid;
    uint16_t                    bsid;
} ql_nw_reg_status_info_t
```

**参数**

| 类型 | 参数 | 描述 |
|------|------|------|
| `QL_NW_TECH_DOMAIN_TYPE_E` | `tech_domain` | 协议类型；详见 [2.3.8.2](#2382-ql_nw_tech_domain_type_e) |
| `QL_NW_RADIO_TECH_TYPE_E` | `radio_tech` | 网络制式；详见 [2.3.2](#232-ql_nw_network_scan) |
| `QL_NW_ROAM_STATE_TYPE_E` | `roaming` | 漫游通知开启状态；详见 [2.3.8.5](#2385-ql_nw_roam_state_type_e) |
| `QL_NW_DENY_REASON_TYPE_E` | `deny_reason` | 注网被拒原因；详见 [2.3.8.3](#2383-ql_nw_deny_reason_type_e) |
| `QL_NW_SERVICE_TYPE_E` | `reg_state` | 网络服务类型；详见 [2.3.8.4](#2384-ql_nw_service_type_e) |
| `char` | `mcc` | 移动设备国家代码 |
| `char` | `mnc` | 移动设备网络代码 |
| `uint8_t` | `forbidden` | 是否禁用注网功能。0：启用；1：禁用 |
| `uint32_t` | `cid` | 小区 ID 号 |
| `uint16_t` | `lac` | 位置区域码 |
| `uint16_t` | `psc` | 主扰码。仅在 WCDMA 系统中使用。 |
| `uint16_t` | `tac` | 追踪区域码。LTE 系统中使用。 |
| `uint8_t` | `inPRL` | 是否在优先漫游列表中。0：不在；1：在 |
| `uint8_t` | `css` | 是否支持并发。0：不支持；1：支持 |
| `uint16_t` | `sid` | 预留 |
| `uint16_t` | `nid` | 预留 |
| `uint16_t` | `bsid` | 基站 ID |

---

##### 2.3.8.2 QL_NW_TECH_DOMAIN_TYPE_E

协议类型枚举定义如下：

```c
typedef enum QL_NW_TECH_DOMAIN_TYPE_ENUM
{
    QL_NW_TECH_DOMAIN_NONE  = 0,
    QL_NW_TECH_DOMAIN_3GPP  = 1,
    QL_NW_TECH_DOMAIN_3GPP2 = 2,
} QL_NW_TECH_DOMAIN_TYPE_E
```

| 成员 | 描述 |
|------|------|
| `QL_NW_TECH_DOMAIN_NONE` | 无效协议 |
| `QL_NW_TECH_DOMAIN_3GPP` | 3GPP 协议 |
| `QL_NW_TECH_DOMAIN_3GPP2` | 3GPP2 协议 |

---

##### 2.3.8.3 QL_NW_DENY_REASON_TYPE_E

注网被拒原因枚举定义如下：

```c
typedef enum QL_NW_DENY_REASON_TYPE_ENUM
{
    QL_NW_IMSI_UNKNOWN_IN_HSS_DENY_REASON                            = 2,
    QL_NW_ILLEGAL_UE_DENY_REASON                                     = 3,
    QL_NW_IMEI_NOT_ACCEPTED_DENY_REASON                              = 5,
    QL_NW_ILLEGAL_ME_DENY_REASON                                     = 6,
    QL_NW_EPS_SERVICES_NOT_ALLOWED_DENY_REASON                       = 7,
    QL_NW_EPS_SERVICES_AND_NON_EPS_SERVICES_NOT_ALLOWED_DENY_REASON  = 8,
    QL_NW_UE_ID_CANNOT_BE_DERIVED_BY_THE_NETWORK_DENY_REASON         = 9,
    QL_NW_IMPLICITY_DETACHED_DENY_REASON                             = 10,
    QL_NW_PLMN_NOT_ALLOWED_DENY_REASON                               = 11,
    QL_NW_TRACKING_AREA_NOT_ALLOWED_DENY_REASON                      = 12,
    QL_NW_ROAMING_NOT_ALLOWED_IN_THIS_TRACNING_AREA_DENY_REASON      = 13,
    QL_NW_EPS_SERVICES_NOT_ALLOWED_IN_THIS_PLMN_DENY_REASON          = 14,
    QL_NW_NO_SUITALBE_CELLS_IN_TRACKING_AREA_DENY_REASON             = 15,
    QL_NW_MSC_TEMPORARILY_NOT_REACHABLE_DENY_REASON                  = 16,
    QL_NW_NETWORK_FAILURE_DENY_REASON                                = 17,
    QL_NW_CS_DOMAIN_NOT_AVAILABLE_DENY_REASON                        = 18,
    QL_NW_EMS_FAILURE_DENY_REASON                                    = 19,
    QL_NW_MAC_FAILURE_DENY_REASON                                    = 20,
    QL_NW_SYNC_FAILURE_DENY_REASON                                   = 21,
    QL_NW_CONGESTION_DENY_REASON                                     = 22,
    QL_NW_UE_SECURITY_CAPABILITIES_MISMATCH_DENY_REASON              = 23,
    QL_NW_SECURITY_MODE_REJECTED_DENY_REASON                         = 24,
    QL_NW_NOT_AUTHORIZED_FOR_THIS_CSG_DENY_REASON                    = 25,
    QL_NW_NON_EPS_AUTHENTICATION_UNCACCEPTABLE_DENY_REASON           = 26,
    QL_NW_REQUESTED_SERVICE_OPTION_NOT_AUTHORIZED_IN_THIS_PLMN_DENY_REASON = 35,
    QL_NW_CS_SERVICE_TEMPORARILY_NOT_AVAILABLE_DENY_REASON           = 39,
    QL_NW_NO_ESP_BEARER_CONTEXT_ACTIVATED_DENY_REASON                = 40,
    QL_NW_SEMAMTICALLY_INCORRECT_MESSAGE_DENY_REASON                 = 95,
    QL_NW_INVALID_MANDATORY_INFORMATION_DENY_REASON                  = 96,
    QL_NW_MESSAGE_TYPE_NON_EXISTENT_OR_NOT_IMPLEMENTED_DENY_REASON   = 97,
    QL_NW_MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCAL_STATE_DENY_REASON = 98,
    QL_NW_INFORMATION_ELEMENT_NON_EXISTENT_OR_NOT_IMPLEMENTED_DENY_REASON = 99,
    QL_NW_CONDITIONAL_IE_ERROR_DENY_REASON                           = 100,
    QL_NW_MESSAGE_NOT_COMPATIBLE_WITH_PROTOCAL_STATE_DENY_REASON     = 101,
} QL_NW_DENY_REASON_TYPE_E
```

**主要成员说明**

| 成员 | 描述 |
|------|------|
| `QL_NW_IMSI_UNKNOWN_IN_HSS_DENY_REASON` (2) | HSS 中未知 IMSI |
| `QL_NW_ILLEGAL_UE_DENY_REASON` (3) | 非法移动用户 |
| `QL_NW_IMEI_NOT_ACCEPTED_DENY_REASON` (5) | 非法 IMEI |
| `QL_NW_ILLEGAL_ME_DENY_REASON` (6) | 非法移动设备 |
| `QL_NW_EPS_SERVICES_NOT_ALLOWED_DENY_REASON` (7) | EPS 服务拒绝 |
| `QL_NW_EPS_SERVICES_AND_NON_EPS_SERVICES_NOT_ALLOWED_DENY_REASON` (8) | EPS 服务和非 EPS 服务拒绝 |
| `QL_NW_UE_ID_CANNOT_BE_DERIVED_BY_THE_NETWORK_DENY_REASON` (9) | 用户 ID 不可派生 |
| `QL_NW_IMPLICITY_DETACHED_DENY_REASON` (10) | 隐式去附着 |
| `QL_NW_PLMN_NOT_ALLOWED_DENY_REASON` (11) | PLMN 拒绝 |
| `QL_NW_TRACKING_AREA_NOT_ALLOWED_DENY_REASON` (12) | 跟踪区域拒绝 |
| `QL_NW_ROAMING_NOT_ALLOWED_IN_THIS_TRACNING_AREA_DENY_REASON` (13) | 跟踪区域不允许漫游 |
| `QL_NW_EPS_SERVICES_NOT_ALLOWED_IN_THIS_PLMN_DENY_REASON` (14) | EPS 服务在此 PLMN 不可用 |
| `QL_NW_NO_SUITALBE_CELLS_IN_TRACKING_AREA_DENY_REASON` (15) | 没有可用的小区 |
| `QL_NW_MSC_TEMPORARILY_NOT_REACHABLE_DENY_REASON` (16) | MSC 未达标 |
| `QL_NW_NETWORK_FAILURE_DENY_REASON` (17) | 网络故障 |
| `QL_NW_CS_DOMAIN_NOT_AVAILABLE_DENY_REASON` (18) | CS 域不可用 |
| `QL_NW_EMS_FAILURE_DENY_REASON` (19) | EMS 故障 |
| `QL_NW_MAC_FAILURE_DENY_REASON` (20) | MAC 故障 |
| `QL_NW_SYNC_FAILURE_DENY_REASON` (21) | 同步故障 |
| `QL_NW_CONGESTION_DENY_REASON` (22) | 网络拥塞 |
| `QL_NW_UE_SECURITY_CAPABILITIES_MISMATCH_DENY_REASON` (23) | 用户安全能力不匹配 |
| `QL_NW_SECURITY_MODE_REJECTED_DENY_REASON` (24) | 安全模式拒绝或未指定 |
| `QL_NW_NOT_AUTHORIZED_FOR_THIS_CSG_DENY_REASON` (25) | CSG 未认证 |
| `QL_NW_NON_EPS_AUTHENTICATION_UNCACCEPTABLE_DENY_REASON` (26) | 非 EPS 服务认证失败 |
| `QL_NW_CS_SERVICE_TEMPORARILY_NOT_AVAILABLE_DENY_REASON` (39) | CS 服务暂时不可用 |
| `QL_NW_NO_ESP_BEARER_CONTEXT_ACTIVATED_DENY_REASON` (40) | ESP 上下文未被激活 |
| `QL_NW_SEMAMTICALLY_INCORRECT_MESSAGE_DENY_REASON` (95) | 语法错误 |
| `QL_NW_CONDITIONAL_IE_ERROR_DENY_REASON` (100) | IE 条件错误 |
| `QL_NW_MESSAGE_NOT_COMPATIBLE_WITH_PROTOCAL_STATE_DENY_REASON` (101) | 消息与协议不兼容 |

---

##### 2.3.8.4 QL_NW_SERVICE_TYPE_E

网络服务类型枚举定义如下：

```c
typedef enum QL_NW_SERVICE_TYPE_ENUM
{
    QL_NW_SERVICE_NONE    = 0,
    QL_NW_SERVICE_LIMITED = 1,
    QL_NW_SERVICE_FULL    = 2,
} QL_NW_SERVICE_TYPE_E
```

| 成员 | 描述 |
|------|------|
| `QL_NW_SERVICE_NONE` | 无服务 |
| `QL_NW_SERVICE_LIMITED` | 受限服务 |
| `QL_NW_SERVICE_FULL` | 正常服务 |

---

##### 2.3.8.5 QL_NW_ROAM_STATE_TYPE_E

漫游通知开启状态枚举定义如下：

```c
typedef enum QL_NW_ROAM_STATE_TYPE_ENUM
{
    QL_NW_ROAM_STATE_OFF = 0,
    QL_NW_ROAM_STATE_ON  = 1
} QL_NW_ROAM_STATE_TYPE_E
```

| 成员 | 描述 |
|------|------|
| `QL_NW_ROAM_STATE_OFF` | 关闭漫游 |
| `QL_NW_ROAM_STATE_ON` | 开启漫游 |

---

#### 2.3.9 ql_nw_get_data_reg_status

该函数用于获取数据拨号注网状态信息。

**函数原型**

```c
int ql_nw_get_data_reg_status(ql_nw_reg_status_info_t *p_info)
```

**参数**

- `p_info`：
  - [Out] 获取到的数据拨号注网状态信息；详见 [2.3.8.1 ql_nw_reg_status_info_t](#2381-ql_nw_reg_status_info_t)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 2.3.10 ql_nw_get_signal_strength

该函数用于获取当前信号强度信息以及信号强度等级。

**函数原型**

```c
int ql_nw_get_signal_strength(ql_nw_signal_strength_info_t *p_info,
                               QL_NW_SIGNAL_STRENGTH_LEVEL_E* p_level)
```

**参数**

- `p_info`：
  - [Out] 获取到的信号强度信息。当注册多个网络时，如电信卡可能注册到 2 个网络，则会有两种信号强度信息；详见 [2.3.10.1 ql_nw_signal_strength_info_t](#23101-ql_nw_signal_strength_info_t)。
- `p_level`：
  - [Out] 信号强度等级；详见 [2.3.10.4 QL_NW_SIGNAL_STRENGTH_LEVEL_E](#23104-ql_nw_signal_strength_level_e)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.10.1 ql_nw_signal_strength_info_t

信号强度信息结构体定义如下：

```c
typedef struct
{
    uint8_t                     has_gsm;
    ql_nw_gsm_signal_info_t     gsm;        /* 预留 */
    uint8_t                     has_wcdma;
    ql_nw_wcdma_signal_info_t   wcdma;
    uint8_t                     has_tdscdma;
    ql_nw_tdscdma_signal_info_t tdscdma;    /* 预留 */
    uint8_t                     has_lte;
    ql_nw_lte_signal_info_t     lte;
    uint8_t                     has_cdma;
    ql_nw_cdma_signal_info_t    cdma;       /* 预留 */
    uint8_t                     has_hdr;
    ql_nw_hdr_signal_info_t     hdr;        /* 预留 */
} ql_nw_signal_strength_info_t
```

| 类型 | 参数 | 描述 |
|------|------|------|
| `uint8_t` | `has_gsm` | 参数 gsm 是否有效。0：无效 |
| `ql_nw_gsm_signal_info_t` | `gsm` | 预留 |
| `uint8_t` | `has_wcdma` | 参数 wcdma 是否有效。0：无效；其他值：有效 |
| `ql_nw_wcdma_signal_info_t` | `wcdma` | WCDMA 信号强度信息；详见 [2.3.10.2](#23102-ql_nw_wcdma_signal_info_t) |
| `uint8_t` | `has_tdscdma` | 参数 tdscdma 是否有效。0：无效 |
| `ql_nw_tdscdma_signal_info_t` | `tdscdma` | 预留 |
| `uint8_t` | `has_lte` | 参数 lte 是否有效。0：无效；其他值：有效 |
| `ql_nw_lte_signal_info_t` | `lte` | LTE 信号强度信息；详见 [2.3.10.3](#23103-ql_nw_lte_signal_info_t) |
| `uint8_t` | `has_cdma` | 参数 cdma 是否有效。0：无效 |
| `ql_nw_cdma_signal_info_t` | `cdma` | 预留 |
| `uint8_t` | `has_hdr` | 参数 hdr 是否有效。0：无效 |
| `ql_nw_hdr_signal_info_t` | `hdr` | 预留 |

---

##### 2.3.10.2 ql_nw_wcdma_signal_info_t

WCDMA 信号强度结构体定义如下：

```c
typedef struct
{
    int8_t  rssi;
    int16_t ecio;
} ql_nw_wcdma_signal_info_t
```

| 类型 | 参数 | 描述 |
|------|------|------|
| `int8_t` | `rssi` | 接收信号强度指示。单位：dBm。 |
| `int16_t` | `ecio` | Ec/Io（截干比）。单位：-0.5 dB。 |

---

##### 2.3.10.3 ql_nw_lte_signal_info_t

LTE 信号强度结构体定义如下：

```c
typedef struct
{
    int8_t  rssi;
    int8_t  rsrq;
    int16_t rsrp;
    int16_t snr;
} ql_nw_lte_signal_info_t
```

| 类型 | 参数 | 描述 |
|------|------|------|
| `int8_t` | `rssi` | 接收信号强度指示。单位：dBm。 |
| `int8_t` | `rsrq` | 参考信号接收质量。单位：dB。 |
| `int16_t` | `rsrp` | 参考信号接收功率。单位：dBm。 |
| `int16_t` | `snr` | 信噪比。单位：0.1 dB。 |

---

##### 2.3.10.4 QL_NW_SIGNAL_STRENGTH_LEVEL_E

信号强度等级枚举定义如下：

```c
typedef enum QL_NW_SIGNAL_STRENGTH_LEVEL_ENUM
{
    QL_NW_SIGNAL_STRENGTH_LEVEL_MIN      = -1,
    QL_NW_SIGNAL_STRENGTH_LEVEL_NONE     = 0,
    QL_NW_SIGNAL_STRENGTH_LEVEL_POOR     = 1,
    QL_NW_SIGNAL_STRENGTH_LEVEL_MODERATE = 2,
    QL_NW_SIGNAL_STRENGTH_LEVEL_GOOD     = 3,
    QL_NW_SIGNAL_STRENGTH_LEVEL_GREAT    = 4,
    QL_NW_SIGNAL_STRENGTH_LEVEL_MAX
} QL_NW_SIGNAL_STRENGTH_LEVEL_E
```

| 成员 | 描述 |
|------|------|
| `QL_NW_SIGNAL_STRENGTH_LEVEL_NONE` | 无信号 |
| `QL_NW_SIGNAL_STRENGTH_LEVEL_POOR` | 信号差 |
| `QL_NW_SIGNAL_STRENGTH_LEVEL_MODERATE` | 信号中等 |
| `QL_NW_SIGNAL_STRENGTH_LEVEL_GOOD` | 信号良 |
| `QL_NW_SIGNAL_STRENGTH_LEVEL_GREAT` | 信号优 |

---

#### 2.3.11 ql_nw_get_cell_access_status

该函数用于获取当前小区访问状态。

**函数原型**

```c
int ql_nw_get_cell_access_status(QL_NW_CELL_ACCESS_STATE_TYPE_E *p_info)
```

**参数**

- `p_info`：
  - [Out] 获取到的小区访问状态；详见 [2.3.11.1](#23111-ql_nw_cell_access_state_type_e)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.11.1 QL_NW_CELL_ACCESS_STATE_TYPE_E

小区访问状态枚举定义如下：

```c
typedef enum QL_NW_CELL_ACCESS_STATE_TYPE_ENUM
{
    QL_NW_CELL_ACCESS_NONE           = 0,
    QL_NW_CELL_ACCESS_NORMAL_ONLY    = 1,
    QL_NW_CELL_ACCESS_EMERGENCY_ONLY = 2,
    QL_NW_CELL_ACCESS_NO_CALLS       = 3,
    QL_NW_CELL_ACCESS_ALL_CALLS      = 4,
} QL_NW_CELL_ACCESS_STATE_TYPE_E
```

| 成员 | 描述 |
|------|------|
| `QL_NW_CELL_ACCESS_NONE` | 未知访问状态 |
| `QL_NW_CELL_ACCESS_NORMAL_ONLY` | 正常访问状态 |
| `QL_NW_CELL_ACCESS_EMERGENCY_ONLY` | 紧急访问状态 |
| `QL_NW_CELL_ACCESS_NO_CALLS` | 无法访问状态 |
| `QL_NW_CELL_ACCESS_ALL_CALLS` | 全访问状态 |

---

#### 2.3.12 ql_nw_get_nitz_time_info

该函数用于获取网络时间信息。

**函数原型**

```c
int ql_nw_get_nitz_time_info(ql_nw_nitz_time_info_t *p_info)
```

**参数**

- `p_info`：
  - [Out] 网络时间信息；详见 [2.3.12.1 ql_nw_nitz_time_info_t](#23121-ql_nw_nitz_time_info_t)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.12.1 ql_nw_nitz_time_info_t

网络时间信息结构体定义如下：

```c
typedef struct
{
    char        nitz_time[QL_NW_NITZ_BUF_LEN + 1];
    uint64_t    abs_time;
    int8_t      leap_sec;
} ql_nw_nitz_time_info_t
```

| 类型 | 参数 | 描述 |
|------|------|------|
| `char` | `nitz_time` | UTC 时间。格式为：`YY/MM/DD,HH:MM:SS +/- TZ`。`+` 代表东区，`-` 代表西区，TZ 以 15 分钟为单位。如 `+32` 代表东 8 区，`-24` 代表西 7 区。 |
| `uint64_t` | `abs_time` | 绝对时间。相对于 1970 年 1 月 1 日 0 点 0 分 0 秒（UTC）。 |
| `int8_t` | `leap_sec` | 闰秒（时间误差调整阈值）。 |

---

#### 2.3.13 ql_nw_set_voice_reg_ind_cb

该函数用于注册语音拨号注网事件回调函数。

**函数原型**

```c
int ql_nw_set_voice_reg_ind_cb(ql_nw_voice_reg_ind_cb cb_func)
```

**参数**

- `cb_func`：
  - [In] 语音拨号注网事件回调函数；详见 [2.3.13.1 ql_nw_voice_reg_ind_cb](#23131-ql_nw_voice_reg_ind_cb)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.13.1 ql_nw_voice_reg_ind_cb

该回调函数处理语音拨号注网事件。

**函数原型**

```c
typedef void (*ql_nw_voice_reg_ind_cb)(ql_nw_reg_status_info_t *p_info)
```

| 参数 | 方向 | 描述 |
|------|------|------|
| `p_info` | [In] | 语音拨号注网状态；详见 [2.3.8.1](#2381-ql_nw_reg_status_info_t) |

**返回值**：无

---

#### 2.3.14 ql_nw_set_data_reg_ind_cb

该函数用于注册数据拨号注网事件回调函数。

**函数原型**

```c
int ql_nw_set_data_reg_ind_cb(ql_nw_data_reg_ind_cb cb_func)
```

**参数**

- `cb_func`：
  - [In] 数据拨号注网事件回调函数；详见 [2.3.14.1 ql_nw_data_reg_ind_cb](#23141-ql_nw_data_reg_ind_cb)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.14.1 ql_nw_data_reg_ind_cb

该回调函数处理数据拨号注网事件。

**函数原型**

```c
typedef void (*ql_nw_data_reg_ind_cb)(ql_nw_reg_status_info_t *p_info)
```

| 参数 | 方向 | 描述 |
|------|------|------|
| `p_info` | [In] | 数据拨号注网状态；详见 [2.3.8.1](#2381-ql_nw_reg_status_info_t) |

**返回值**：无

---

#### 2.3.15 ql_nw_set_signal_strength_ind_cb

该函数用于注册信号强度事件回调函数。

**函数原型**

```c
int ql_nw_set_signal_strength_ind_cb(ql_nw_signal_strength_ind_cb cb_func)
```

**参数**

- `cb_func`：
  - [In] 信号强度事件回调函数；详见 [2.3.15.1 ql_nw_signal_strength_ind_cb](#23151-ql_nw_signal_strength_ind_cb)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.15.1 ql_nw_signal_strength_ind_cb

该回调函数处理信号强度事件。

**函数原型**

```c
typedef void (*ql_nw_signal_strength_ind_cb)(ql_nw_signal_strength_info_t *p_info,
                                              QL_NW_SIGNAL_STRENGTH_LEVEL_E level)
```

| 参数 | 方向 | 描述 |
|------|------|------|
| `p_info` | [In] | 信号强度信息；详见 [2.3.10.1](#23101-ql_nw_signal_strength_info_t) |
| `level` | [In] | 信号强度等级；详见 [2.3.10.4](#23104-ql_nw_signal_strength_level_e) |

**返回值**：无

---

#### 2.3.16 ql_nw_set_cell_access_status_ind_cb

该函数用于注册小区访问状态事件回调函数。

**函数原型**

```c
int ql_nw_set_cell_access_status_ind_cb(ql_nw_cell_access_status_ind_cb cb_func)
```

**参数**

- `cb_func`：
  - [In] 小区访问状态事件回调函数；详见 [2.3.16.1 ql_nw_cell_access_status_ind_cb](#23161-ql_nw_cell_access_status_ind_cb)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.16.1 ql_nw_cell_access_status_ind_cb

该回调函数处理小区访问状态事件。

**函数原型**

```c
typedef void (*ql_nw_cell_access_status_ind_cb)(QL_NW_CELL_ACCESS_STATE_TYPE_E status)
```

| 参数 | 方向 | 描述 |
|------|------|------|
| `status` | [In] | 小区访问状态；详见 [2.3.11.1](#23111-ql_nw_cell_access_state_type_e) |

**返回值**：无

---

#### 2.3.17 ql_nw_set_nitz_time_update_ind_cb

该函数用于注册网络时间事件回调函数。

**函数原型**

```c
int ql_nw_set_nitz_time_update_ind_cb(ql_nw_nitz_time_update_ind_cb cb_func)
```

**参数**

- `cb_func`：
  - [In] 网络时间事件回调函数；详见 [2.3.17.1 ql_nw_nitz_time_update_ind_cb](#23171-ql_nw_nitz_time_update_ind_cb)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.17.1 ql_nw_nitz_time_update_ind_cb

该回调函数处理网络时间事件。

**函数原型**

```c
typedef void (*ql_nw_nitz_time_update_ind_cb)(ql_nw_nitz_time_info_t *p_info)
```

| 参数 | 方向 | 描述 |
|------|------|------|
| `p_info` | [Out] | 网络时间信息；详见 [2.3.12.1](#23121-ql_nw_nitz_time_info_t) |

**返回值**：无

---

#### 2.3.18 ql_nw_set_service_error_cb

该函数用于注册服务异常事件回调函数。

**函数原型**

```c
int ql_nw_set_service_error_cb(ql_nw_service_error_cb_f cb)
```

**参数**

- `cb`：
  - [In] 服务异常回调函数；详见 [2.3.18.1 ql_nw_service_error_cb_f](#23181-ql_nw_service_error_cb_f)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 2.3.18.1 ql_nw_service_error_cb_f

该回调函数处理服务异常事件。

**函数原型**

```c
typedef void (*ql_nw_service_error_cb_f)(int error)
```

| 参数 | 方向 | 描述 |
|------|------|------|
| `error` | [Out] | 错误码。`QL_ERR_SERVICE_ABORT`：服务终止 |

**返回值**：无

---

#### 2.3.19 ql_nw_deinit

该函数用于去初始化网络服务。

**函数原型**

```c
int ql_nw_deinit(void)
```

**参数**：无

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

## 3. 示例

本章所述代码示例均摘自 `ql-sdk/sample/test_sdk_api/m_nw.c`，此文件下可查看接口函数的完整示例。程序启动后，必须调用 `ql_nw_init()` 初始化网络服务。

测试菜单示例（运行 test_sdk_api 后可见）：

```
0:  test ql_nw_network_scan
1:  test ql_nw_set_power_mode
2:  test ql_nw_set_pref_nwmode_roaming
3:  test ql_nw_get_pref_nwmode_roaming
4:  test ql_nw_get_mobile_operator_name
5:  test ql_nw_get_cell_info
6:  test ql_nw_get_voice_reg_status
7:  test ql_nw_get_data_reg_status
8:  test ql_nw_get_signal_strength
9:  test ql_nw_get_cell_access_status
10: test ql_nw_get_nitz_time_info
11: test ql_nw_set_voice_reg_ind_cb
12: test ql_nw_set_data_reg_ind_cb
13: test ql_nw_set_signal_strength_ind_cb
14: test ql_nw_set_cell_access_status_ind_cb
15: test ql_nw_set_nitz_time_ind_cb
-1: exit
```

---

### 3.1 网络扫描

```c
void item_ql_nw_network_scan(void)
{
    int ret;
    int async_id = 0;

    ret = ql_nw_network_scan(&async_id, nw_network_scan_async_cb);
    if (ret == QL_ERR_OK)
    {
        printf("async network scan succeed, token id is %d\n", async_id);
    }
    else
    {
        printf("async network scan failed, token id is %d, ret=%d", async_id, ret);
    }
}
```

网络扫描结果示例：

```
network scan async callback, async id is 2, list_len=1, detail info:
[0]: long_eons=CMCC, short_eons=CMCC, mcc=460, mnc=00, network_status = CURRENT_SERVING, radio_tech = UMTS
```

---

### 3.2 设置功耗模式

```c
void item_ql_nw_set_power_mode(void)
{
    int ret;
    uint32_t mode = 0;

    printf("please input power mode mask hex(VOICE | SMS | SIM | NETWORK | NORMAL): ");
    ret = t_get_hex(&mode);
    if (ret != 0)
    {
        printf("Invalid input\n");
        return;
    }
    ret = ql_nw_set_power_mode(mode);
    printf("ql_nw_set_lower_power_mode ret = %d\n", ret);
}
```

---

### 3.3 设置首选网络制式和漫游通知开启状态

```c
void item_ql_nw_set_pref_nwmode_roaming(void)
{
    int ret;
    uint32_t mask = 0;
    ql_nw_pref_nwmode_roaming_info_t t_info;

    memset(&t_info, 0, sizeof(t_info));
    printf("please input config mask hex(TDSCDMA | LTE | EVDO | CDMA | WCDMA | GSM): ");
    ret = t_get_hex(&mask);
    if (ret != 0)
    {
        printf("Invalid input\n");
        return;
    }
    t_info.preferred_nw_mode = mask;

#if 0
    printf("please input roaming pref(0:off 1:on): ");
    ret = t_get_int((int *)&mask);
    if (ret != 0)
    {
        printf("Invalid input\n");
        return;
    }
    t_info.preferred_roaming = mask;
#endif

    ret = ql_nw_set_pref_nwmode_roaming(&t_info);
    printf("ql_nw_set_config ret = %d\n", ret);
}
```

---

### 3.4 获取首选网络制式和漫游通知开启状态

```c
void item_ql_nw_get_pref_nwmode_roaming(void)
{
    int ret;
    ql_nw_pref_nwmode_roaming_info_t t_info;

    memset(&t_info, 0, sizeof(t_info));
    ret = ql_nw_get_pref_nwmode_roaming(&t_info);
#if 0
    printf("ql_nw_get_config ret = %d\npreferred_nw_mode=%#llx, preferred_roaming=%d\n",
            ret, t_info.preferred_nw_mode, t_info.preferred_roaming);
#else
    printf("ql_nw_get_config ret = %d\npreferred_nw_mode=%#llx\n",
            ret, t_info.preferred_nw_mode);
#endif
}
```

---

### 3.5 获取运营商信息

```c
void item_ql_nw_get_mobile_operator_name(void)
{
    int ret;
    ql_nw_mobile_operator_name_info_t t_info;

    memset(&t_info, 0, sizeof(t_info));
    ret = ql_nw_get_mobile_operator_name(&t_info);
    printf("ql_nw_get_operator_name ret = %d, long_eons=%s, short_eons=%s, mcc=%s, mnc=%s\n",
            ret,
            t_info.long_eons,
            t_info.short_eons,
            t_info.mcc,
            t_info.mnc);
}
```

---

### 3.6 获取小区信息

```c
void item_ql_nw_get_cell_info(void)
{
    int i;
    int ret;
    unsigned short mcc;
    unsigned short mnc;
    ql_nw_cell_info_t t_info;

    memset(&t_info, 0, sizeof(t_info));
    ret = ql_nw_get_cell_info(&t_info);
    printf("ql_nw_get_cell_info ret = %d, detail info:\n", ret);

    if (t_info.gsm_info_valid)
    {
        printf("gsm cell information:\n");
        for (i = 0; i < t_info.gsm_info_len; i++)
        {
            printf("\tcid=%d,plmn=0x%02x 0x%02x 0x%02x,lac=%d,arfcn=%d,bsic=%d,rssi=%d,",
                    t_info.gsm_info[i].cid,
                    t_info.gsm_info[i].plmn[0], t_info.gsm_info[i].plmn[1], t_info.gsm_info[i].plmn[2],
                    t_info.gsm_info[i].lac,
                    t_info.gsm_info[i].arfcn,
                    t_info.gsm_info[i].bsic,
                    t_info.gsm_info[i].rssi);
            internal_nw_get_mcc_mnc_value(t_info.gsm_info[i].plmn, 3, &mcc, &mnc);
            printf("convert plmn to mcc=%d,mnc=%02d\n", mcc, mnc);
        }
    }

    if (t_info.umts_info_valid)
    {
        printf("umts cell information:\n");
        for (i = 0; i < t_info.umts_info_len; i++)
        {
            printf("\tcid=%d,lcid=%d,plmn=0x%02x 0x%02x 0x%02x,lac=%d,uarfcn=%d,psc=%d,rssi=%d,",
                    t_info.umts_info[i].cid, t_info.umts_info[i].lcid,
                    t_info.umts_info[i].plmn[0], t_info.umts_info[i].plmn[1], t_info.umts_info[i].plmn[2],
                    t_info.umts_info[i].lac,
                    t_info.umts_info[i].uarfcn,
                    t_info.umts_info[i].psc,
                    t_info.umts_info[i].rssi);
            internal_nw_get_mcc_mnc_value(t_info.umts_info[i].plmn, 3, &mcc, &mnc);
            printf("convert plmn to mcc=%d,mnc=%02d\n", mcc, mnc);
        }
    }

    if (t_info.lte_info_valid)
    {
        printf("lte cell information:\n");
        for (i = 0; i < t_info.lte_info_len; i++)
        {
            printf("\tcid=%d,plmn=0x%02x 0x%02x 0x%02x,tac=%d,pci=%d,earfcn=%d,rssi=%d,",
                    t_info.lte_info[i].cid,
                    t_info.lte_info[i].plmn[0], t_info.lte_info[i].plmn[1], t_info.lte_info[i].plmn[2],
                    t_info.lte_info[i].tac,
                    t_info.lte_info[i].pci,
                    t_info.lte_info[i].earfcn,
                    t_info.lte_info[i].rssi);
            internal_nw_get_mcc_mnc_value(t_info.lte_info[i].plmn, 3, &mcc, &mnc);
            printf("convert plmn to mcc=%d,mnc=%02d\n", mcc, mnc);
        }
    }

    if (t_info.nr5g_info_valid)
    {
        printf("nr5g cell information:\n");
        printf("\tcid=%lld,plmn=0x%02x 0x%02x 0x%02x,tac=%d,pci=%d,arfcn=%d,rsrp=%d,rsrq=%d,snr=%d,",
                t_info.nr5g_info.cid,
                t_info.nr5g_info.plmn[0], t_info.nr5g_info.plmn[1], t_info.nr5g_info.plmn[2],
                t_info.nr5g_info.tac,
                t_info.nr5g_info.pci,
                t_info.nr5g_info.arfcn,
                t_info.nr5g_info.rsrp,
                t_info.nr5g_info.rsrq,
                t_info.nr5g_info.snr);
        internal_nw_get_mcc_mnc_value(t_info.nr5g_info.plmn, 3, &mcc, &mnc);
        printf("convert plmn to mcc=%d,mnc=%02d\n", mcc, mnc);
    }

    if (t_info.cdma_info_valid)
    {
        printf("cdma cell information:\n");
        printf("\tsid=%d,nid=%d,bid=%d,refpn=%d,base_lat=%d,base_long=%d,rssi=%d\n",
                t_info.cdma_info.sid,
                t_info.cdma_info.nid,
                t_info.cdma_info.bid,
                t_info.cdma_info.refpn,
                t_info.cdma_info.base_lat,
                t_info.cdma_info.base_long,
                t_info.cdma_info.rssi);
    }
}
```

---

### 3.7 获取语音拨号注网状态

获取语音拨号注网状态输出示例：

```
ql_nw_get_voice_reg_status ret = 0, detail info:
    tech_domain = 3GPP2, radio_tech = LTE, roaming = 0, reg_status = 2, deny_reason = 0
    mcc=460, mnc=00, forbidden=0, cid=0xF4c2521, lac=0, psc=0, tac=22115
    inPRL=0, css=0, sid=0, nid=0, bsid=0
```

---

### 3.8 获取数据拨号注网状态

方法同获取语音拨号注网状态类似，请参考 **第 3.7 章**。

---

### 3.9 获取信号强度信息

```c
void item_ql_nw_get_signal_strength(void)
{
    int ret;
    char level_info[16] = {0};
    ql_nw_signal_strength_info_t info;
    QL_NW_SIGNAL_STRENGTH_LEVEL_E level = QL_NW_SIGNAL_STRENGTH_LEVEL_NONE;

    memset(&info, 0, sizeof(info));
    ret = ql_nw_get_signal_strength(&info, &level);
    if (QL_ERR_OK != ret)
    {
        printf("failed, ret = %d\n", ret);
        return;
    }

    if (info.has_gsm)
        printf("gsm_sig_info: rssi=%hhd\n", info.gsm.rssi);
    if (info.has_wcdma)
        printf("wcdma_sig_info: rssi=%hhd, ecio=%hd\n", info.wcdma.rssi, info.wcdma.ecio);
    if (info.has_tdscdma)
        printf("tdscdma_sig_info: rssi=%hhd, rscp=%hhd, ecio=%hd, sinr=%hhd\n",
               info.tdscdma.rssi, info.tdscdma.rscp, info.tdscdma.ecio, info.tdscdma.sinr);
    if (info.has_lte)
        printf("lte_sig_info: rssi=%hhd, rsrq=%hhd, rsrp=%hd, snr=%hd\n",
               info.lte.rssi, info.lte.rsrq, info.lte.rsrp, info.lte.snr);
    if (info.has_nr5g)
        printf("nr5g_sig_info: rsrp=%hd, rsrq=%hd, snr=%hd\n",
               info.nr5g.rsrp, info.nr5g.rsrq, info.nr5g.snr);
    if (info.has_cdma)
        printf("cdma_sig_info: rssi=%hhd, ecio=%hd\n", info.cdma.rssi, info.cdma.ecio);
    if (info.has_hdr)
        printf("hdr_sig_info: rssi=%hhd, ecio=%hd, sinr=%hd, io=%d\n",
               info.hdr.rssi, info.hdr.ecio, info.hdr.sinr, info.hdr.io);

    if (internal_nw_get_signal_strength_level(level, level_info, sizeof(level_info)) == 0)
        printf("signal strength level is %d, unrecognized\n", level);
    else
        printf("signal strength level is %s\n", level_info);
}
```

信号强度信息获取示例输出（选项 8）：

```
please enter your choice: 8
lte_sig_info: rssi=-74, rsrq=-4, rsrp=-98, snr=224, signal strength level is MODERATE
```

---

### 3.10 获取小区访问状态信息

```c
void item_ql_nw_get_cell_access_status(void)
{
    int ret;
    QL_NW_CELL_ACCESS_STATE_TYPE_E e_state;

    ret = ql_nw_get_cell_access_status(&e_state);
    printf("ql_nw_get_cell_access_state ret = %d, e_state=%d\n", ret, e_state);
}
```

---

### 3.11 获取网络时间信息

```c
void item_ql_nw_get_nitz_time_info(void)
{
    int ret;
    ql_nw_nitz_time_info_t t_info;

    memset(&t_info, 0, sizeof(t_info));
    ret = ql_nw_get_nitz_time_info(&t_info);
    printf("ql_nw_get_nitz_time_info ret = %d\n nitz_time=%s, abs_time=%lld, leap_sec=%hhd\n",
            ret,
            t_info.nitz_time,
            t_info.abs_time,
            t_info.leap_sec);
}
```

---

### 3.12 注册语音拨号注网事件

```c
void item_ql_nw_set_voice_reg_ind_cb(void)
{
    int ret = 0;
    int reg_flag = 0;

    printf("please input voice reg option: (0: unreg, other: reg): ");
    ret = t_get_int(&reg_flag);
    if (ret != 0)
    {
        printf("Invalid input\n");
        return;
    }
    if (reg_flag)
        ret = ql_nw_set_voice_reg_ind_cb(nw_voice_reg_event_ind_cb);
    else
        ret = ql_nw_set_voice_reg_ind_cb(NULL);
    printf("ql_nw_reg_voice_reg_event ret = %d\n", ret);
}
```

---

### 3.13 注册数据拨号注网事件

```c
void item_ql_nw_set_data_reg_ind_cb(void)
{
    int ret = 0;
    int reg_flag = 0;

    printf("please input data reg option: (0: unreg, other: reg): ");
    ret = t_get_int(&reg_flag);
    if (ret != 0)
    {
        printf("Invalid input\n");
        return;
    }
    if (reg_flag)
        ret = ql_nw_set_data_reg_ind_cb(nw_data_reg_event_ind_cb);
    else
        ret = ql_nw_set_data_reg_ind_cb(NULL);
    printf("ql_nw_reg_data_reg_event ret = %d\n", ret);
}
```

---

### 3.14 注册信号强度事件

```c
void item_ql_nw_set_signal_strength_chg_ind_cb(void)
{
    int ret = 0;
    int reg_flag = 0;

    printf("please input signal strength change reg option: (0: unreg, other: reg): ");
    ret = t_get_int(&reg_flag);
    if (ret != 0)
    {
        printf("Invalid input\n");
        return;
    }
    if (reg_flag)
        ret = ql_nw_set_signal_strength_ind_cb(nw_signal_strength_event_ind_cb);
    else
        ret = ql_nw_set_signal_strength_ind_cb(NULL);
    printf("ql_nw_reg_signal_strength_chg_event ret = %d\n", ret);
}
```

---

### 3.15 注册小区状态事件

```c
void item_ql_nw_set_cell_access_status_chg_ind_cb(void)
{
    int ret = 0;
    int reg_flag = 0;

    printf("please input cell access status change reg option: (0: unreg, other: reg): ");
    ret = t_get_int(&reg_flag);
    if (ret != 0)
    {
        printf("Invalid input\n");
        return;
    }
    if (reg_flag)
        ret = ql_nw_set_cell_access_status_ind_cb(nw_cell_access_status_event_ind_cb);
    else
        ret = ql_nw_set_cell_access_status_ind_cb(NULL);
    printf("ql_nw_reg_cell_access_status_event ret = %d\n", ret);
}
```

---

### 3.16 注册网络时间事件

```c
void item_ql_nw_set_nitz_time_update_ind_cb(void)
{
    int ret = 0;
    int reg_flag = 0;

    printf("please input nitz time update reg option: (0: unreg, other: reg): ");
    ret = t_get_int(&reg_flag);
    if (ret != 0)
    {
        printf("Invalid input\n");
        return;
    }
    if (reg_flag)
        ret = ql_nw_set_nitz_time_update_ind_cb(nw_nitz_time_update_event_ind_cb);
    else
        ret = ql_nw_set_nitz_time_update_ind_cb(NULL);
    printf("ql_nw_reg_nitz_time_update_event ret = %d\n", ret);
}
```

---

### 3.17 注册服务异常函数回调

```c
void item_ql_nw_set_service_error_cb(void)
{
    int ret = 0;

    ret = ql_nw_set_service_error_cb(nw_service_error_cb);
    if (ret != QL_ERR_OK)
    {
        printf("Failed to ql_nw_set_service_error_cb, ret=%d\n", ret);
    }
    else
    {
        printf("Sucessful\n");
    }
}
```

---

### 3.18 去初始化网络服务

```c
void item_ql_nw_deinit(void)
{
    int ret = 0;

    printf("Start to ql_nw_deinit: ");
    ret = ql_nw_deinit();
    if (ret == QL_ERR_OK)
    {
        printf("nw deinit ok\n");
    }
    else
    {
        printf("failed, ret=%d\n", ret);
    }
}
```

---

## 4. 附录 参考文档及术语缩写

**表 2：参考文档**

| 文档名称 |
|---------|
| [1] Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

**表 3：术语缩写**

| 缩写 | 英文描述 | 中文描述 |
|------|---------|---------|
| API | Application Programming Interface | 应用编程接口 |
| ARFCN | Absolute Radio Frequency Channel Number | RF 信道号 |
| CSG | Closed Subscriber Group | 闭合用户组 |
| CDMA | Code Division Multiple Access | 码分多址 |
| EDGE | Enhanced Data Rate for GSM Evolution | 增强型数据速率 GSM 演进技术 |
| EHRPD | Evolved High Rate Package Data | 演进的高速分组网络 |
| EVDO | Evolution-Data Optimized | 演进数据优化 |
| GPRS | General Packet Radio Service | 通用无线分组业务 |
| GSM | Global System for Mobile Communications | 全球移动通讯系统 |
| HDR | High Data Rate | 高速数据速率 |
| HLR | Home Location Register | 归属位置寄存器 |
| HSPA | High-Speed Packet Access | 高速分组接入 |
| ID | Identifier | 标识符 |
| IMSI | International Mobile Subscriber Identity | 国际移动用户识别码 |
| IoT | Internet of Things | 物联网 |
| LAC | Location Area Code | 位置区码 |
| Long_eons | Long Enhanced Operator Name String | 运营商名称全称 |
| LTE | Long Time Evolution | 长期演进技术 |
| MAC | Media Access Control | 介质访问控制 |
| MCC | Mobile Country Code | 移动设备国家代码 |
| MNC | Mobile Network Code | 移动设备网络代码 |
| NW | Network registration | 网络注册 |
| PLMN | Public Land Mobile Network | 公共陆地移动网 |
| PRL | Preferred Roaming List | 优选漫游列表 |
| PSC | Primary Scrambling Code | 主扰码 |
| RF | Radio Frequency | 射频 |
| RSCP | Receive Signal Channel Power | 接收信号频道功率 |
| RSRP | Reference Signal Receiving Power | 参考信号接收功率 |
| RSRQ | Reference Signal Receiving Quality | 参考信号接收质量 |
| RSSI | Received Signal Strength Indicates | 接收信号强度指示 |
| SDK | Software Development Kit | 软件开发工具包 |
| Short_eons | Short Enhanced Operator Name String | 运营商名称简称 |
| SINR | Signal to Interference plus Noise Ratio | 信干噪比 |
| SIM | Subscriber Identity Module | 用户身份识别模块 |
| SNR | Signal Noise Ratio | 信噪比 |
| TAC | Tracing Area Code | 跟踪区编码 |
| TD-SCDMA | Time Division-Synchronous Code Division Multiple Access | 时分-同步码分多址 |
| UMTS | Universal Mobile Telecommunications System | 通用移动通信系统 |
| (U)SIM | (Universal) Subscriber Identity Module | (通用)用户身份识别模块 |
