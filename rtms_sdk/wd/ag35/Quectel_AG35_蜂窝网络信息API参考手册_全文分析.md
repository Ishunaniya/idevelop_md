# Quectel AG35-CET QuecOpen 蜂窝网络信息 API 参考手册 — 全文分析

> **文档全名**：AG35-CET QuecOpen 蜂窝网络信息 API 参考手册
> **适用模块**：LTE Standard 模块系列（AG35-CET，QuecOpen 方案）
> **版本**：V1.0.0（状态：Preliminary 临时文件 / Not Checked）
> **日期**：2023-06-19　**作者**：Keroro TAN
> **源 PDF**：`Quectel_AG35-CET_QuecOpen_蜂窝网络信息API_参考手册_V1.0.0_Preliminary_20230619.pdf`
> **总页数**：58 页（PDF 物理页），正文页脚标注 1/57 起
> **本分析覆盖**：全部 58 页（分段 1-20 / 21-40 / 41-58）

---

## 0. 文档定位与总览

本手册是移远（Quectel）AG35-CET 模块在 **QuecOpen（基于 Linux 的嵌入式开发平台）** 方案下的「蜂窝网络信息 API」编程参考。QuecOpen 允许把客户应用直接跑在模块内部的 Linux 上，本文档专讲一组 `ql_nw_*` 网络信息接口：扫描网络、设置/查询首选制式与漫游、查询运营商、小区信息、注网（语音/数据）状态、信号强度、小区访问状态、网络时间（NITZ），以及一整套主动上报事件回调注册接口。

**核心头文件**：`ql_nw.h`，位于 SDK 包的 `ql-sysroots/usr/include/ql-sdk/` 目录下。文中提到的所有头文件若无特别说明均在此目录。错误码定义在 `ql_type.h`。

**关键全局约束（备注里反复强调）**：
- 所有 `ql_nw_*` API **均不支持并发调用**（除非特别说明）。多线程使用需自行串行化。
- 使用任何其他蜂窝网络信息 API 前，**必须先调用 `ql_nw_init()`** 初始化网络服务。
- 所有函数返回值统一约定：`0` = 成功；其他值 = 失败，错误码见 `ql_type.h`。

**文档结构**（来自目录）：
- 第 1 章 引言
- 第 2 章 蜂窝网络信息 API（2.1 头文件 / 2.2 函数概览 / 2.3 函数详解 共 19 个函数 / 2.4 错误码及恢复措施）
- 第 3 章 示例（18 个示例，3.1~3.18，对应各 API 的实际用法）
- 第 4 章 附录（参考文档及术语缩写）
- 表格索引：表1 函数概览(p8)、表2 错误码及恢复措施(p38)、表3 参考文档(p55)、表4 术语缩写(p55)

---

## 1. 引言（第 7 页）

AG35-CET 模块支持 **QuecOpen®** 方案。QuecOpen 是基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。QuecOpen 的详细信息见参考文档 [1]。本文档主要介绍 QuecOpen 方案下 AG35-CET 模块的蜂窝网络信息 API 和相关示例。

---

## 2. 蜂窝网络信息 API（第 8 页起）

### 2.1 头文件
蜂窝网络信息 API 头文件为 `ql_nw.h`，位于 `ql-sysroots/usr/include/ql-sdk/`。

### 2.2 函数概览（表 1，第 8-9 页）

| 函数 | 说明 |
|---|---|
| `ql_nw_init()` | 初始化网络服务 |
| `ql_nw_network_scan()` | 扫描当前网络 |
| `ql_nw_set_power_mode()` | 设置功耗模式 |
| `ql_nw_set_pref_nwmode_roaming()` | 设置首选网络制式和漫游通知开启状态 |
| `ql_nw_get_pref_nwmode_roaming()` | 获取首选网络制式和漫游通知开启状态 |
| `ql_nw_get_mobile_operator_name()` | 获取运营商信息 |
| `ql_nw_get_cell_info()` | 获取当前小区和邻区信息 |
| `ql_nw_get_voice_reg_status()` | 获取语音拨号注网状态信息 |
| `ql_nw_get_data_reg_status()` | 获取数据拨号注网状态信息 |
| `ql_nw_get_signal_strength()` | 获取信号强度信息及信号强度等级 |
| `ql_nw_get_cell_access_status()` | 获取小区的访问状态 |
| `ql_nw_get_nitz_time_info()` | 获取网络时间信息 |
| `ql_nw_set_voice_reg_ind_cb()` | 注册语音拨号注网事件回调函数 |
| `ql_nw_set_data_reg_ind_cb()` | 注册数据拨号注网事件回调函数 |
| `ql_nw_set_signal_strength_ind_cb()` | 注册信号强度事件回调函数 |
| `ql_nw_set_cell_access_status_ind_cb()` | 注册小区访问状态事件回调函数 |
| `ql_nw_set_nitz_time_update_ind_cb()` | 注册网络时间事件回调函数 |
| `ql_nw_set_service_error_cb()` | 设置服务异常事件回调函数 |
| `ql_nw_deinit` | 去初始化网络服务 |

> **备注**：若无特别说明，所有蜂窝网络信息 API 均不支持并发调用。

---

### 2.3 函数详解

#### 2.3.1 `ql_nw_init`（第 9 页）
初始化网络服务。

```c
int ql_nw_init(void);
```

- **参数**：无
- **返回值**：`0` 成功；其他值失败，错误码见 `ql_type.h`。
- **备注**：使用其他任何蜂窝网络信息 API 前，必须先调用本函数初始化网络服务。

---

#### 2.3.2 `ql_nw_network_scan`（第 10 页）
扫描当前网络。该操作**耗时较久**，为不影响其他功能正常使用，采用**异步调用**方式：扫描结果在第二个参数（回调函数指针 `ql_nw_network_scan_async_cb`）中处理。

```c
int ql_nw_network_scan(int *async_index, ql_nw_network_scan_async_cb async_cb);
```

- **参数**：
  - `async_index` **[Out]**：异步操作索引值。多个异步操作同时执行时，用于区别当前获取的数据源自哪次异步调用的结果。
  - `async_cb` **[In]**：异步扫描网络回调函数，详见 2.3.2.1。
- **返回值**：`0` 成功；其他值失败。

##### 2.3.2.1 `ql_nw_network_scan_async_cb`（第 10 页）
异步扫描网络的回调函数原型：

```c
typedef void (*ql_nw_network_scan_async_cb)(int async_index,
                                            ql_nw_scan_result_list_info_t *p_info);
```

- **参数**：
  - `async_index` **[In]**：异步操作索引值（与发起扫描时返回的索引对应）。
  - `p_info` **[In]**：网络扫描结果，详见 2.3.2.2。
- **返回值**：无。

##### 2.3.2.2 `ql_nw_scan_result_list_info_t`（第 11 页）
网络扫描结果列表结构体：

```c
typedef struct
{
    uint32_t                  entry_len;
    ql_nw_scan_entry_info_t   entry[QL_NW_SCAN_MAX_LIST_NUM];
} ql_nw_scan_result_list_info_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `uint32_t` | `entry_len` | `entry` 数组的有效长度（扫描到的网络条目数） |
| `ql_nw_scan_entry_info_t` | `entry` | 扫描到的网络信息数组（上限 `QL_NW_SCAN_MAX_LIST_NUM`），详见 2.3.2.3 |

##### 2.3.2.3 `ql_nw_scan_entry_info_t`（第 11 页）
单条扫描网络信息结构体：

```c
typedef struct
{
    ql_nw_mobile_operator_name_info_t   operator_name;
    QL_NW_NETWORK_STATUS_TYPE_E         network_status;
    QL_NW_RADIO_TECH_TYPE_E             rat;
} ql_nw_scan_entry_info_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `ql_nw_mobile_operator_name_info_t` | `operator_name` | 运营商信息，详见 2.3.2.4 |
| `QL_NW_NETWORK_STATUS_TYPE_E` | `network_status` | 网络状态，详见 2.3.2.5 |
| `QL_NW_RADIO_TECH_TYPE_E` | `rat` | 网络制式（Radio Access Tech），详见 2.3.2.6 |

##### 2.3.2.4 `ql_nw_mobile_operator_name_info_t`（第 11-12 页）
运营商信息结构体：

```c
typedef struct
{
    char long_eons[512 + 1];
    char short_eons[512 + 1];
    char mcc[3 + 1];
    char mnc[3 + 1];
} ql_nw_mobile_operator_name_info_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `char[513]` | `long_eons` | 运营商名称全称（EONS = Enhanced Operator Name String） |
| `char[513]` | `short_eons` | 运营商名称简称 |
| `char[4]` | `mcc` | 移动设备国家代码（Mobile Country Code，3 位数字 + NUL） |
| `char[4]` | `mnc` | 移动设备网络代码（Mobile Network Code，2~3 位数字 + NUL） |

> **解读**：`mcc`/`mnc` 拼起来即 PLMN。中国移动 46000、中国联通 46001、中国电信 46011 等。`long_eons`/`short_eons` 缓冲特意留到 513 字节（512+1 NUL），调用方读取时要注意它们是定长数组而非指针。

##### 2.3.2.5 `QL_NW_NETWORK_STATUS_TYPE_E`（第 12-13 页）
网络状态枚举：

```c
typedef enum QL_NW_NETWORK_STATUS_TYPE_ENUM
{
    QL_NW_NETWORK_STATUS_NONE            = 0,
    QL_NW_NETWORK_STATUS_CURRENT_SERVING = 1,
    QL_NW_NETWORK_STATUS_PREFERRED       = 2,
    QL_NW_NETWORK_STATUS_NOT_PREFERRED   = 3,
    QL_NW_NETWORK_STATUS_AVAILABLE       = 4,
    QL_NW_NETWORK_STATUS_FORBIDDEN       = 5
} QL_NW_NETWORK_STATUS_TYPE_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_NW_NETWORK_STATUS_NONE` | 0 | 未知网络 |
| `QL_NW_NETWORK_STATUS_CURRENT_SERVING` | 1 | 当前注册网络 |
| `QL_NW_NETWORK_STATUS_PREFERRED` | 2 | 首选网络 |
| `QL_NW_NETWORK_STATUS_NOT_PREFERRED` | 3 | 非首选网络 |
| `QL_NW_NETWORK_STATUS_AVAILABLE` | 4 | 可用网络 |
| `QL_NW_NETWORK_STATUS_FORBIDDEN` | 5 | 禁用网络 |

##### 2.3.2.6 `QL_NW_RADIO_TECH_TYPE_E`（第 13-14 页）
网络制式枚举（注意：**AG35-CET 仅支持 LTE / WCDMA / HSPA 系，其余多数标注「不支持」**）：

```c
typedef enum QL_NW_RADIO_TECH_TYPE_ENUM
{
    QL_NW_RADIO_TECH_TD_SCDMA = 1,
    QL_NW_RADIO_TECH_GSM      = 2,
    QL_NW_RADIO_TECH_HSPAP    = 3,
    QL_NW_RADIO_TECH_LTE      = 4,
    QL_NW_RADIO_TECH_EHRPD    = 5,
    QL_NW_RADIO_TECH_EVDO_B   = 6,
    QL_NW_RADIO_TECH_HSPA     = 7,
    QL_NW_RADIO_TECH_HSUPA    = 8,
    QL_NW_RADIO_TECH_HSDPA    = 9,
    QL_NW_RADIO_TECH_EVDO_A   = 10,
    QL_NW_RADIO_TECH_EVDO_0   = 11,
    QL_NW_RADIO_TECH_1xRTT    = 12,
    QL_NW_RADIO_TECH_IS95B    = 13,
    QL_NW_RADIO_TECH_IS95A    = 14,
    QL_NW_RADIO_TECH_UMTS     = 15,
    QL_NW_RADIO_TECH_EDGE     = 16,
    QL_NW_RADIO_TECH_GPRS     = 17,
    QL_NW_RADIO_TECH_NR5G     = 18,
    QL_NW_RADIO_TECH_NONE     = 19
} QL_NW_RADIO_TECH_TYPE_E;
```

| 成员 | 值 | 描述 | 支持 |
|---|---|---|---|
| `TD_SCDMA` | 1 | TDSCDMA 网络 | 不支持 |
| `GSM` | 2 | GSM 网络 | 不支持 |
| `HSPAP` | 3 | HSPAP 网络 | 支持 |
| `LTE` | 4 | LTE 网络 | 支持 |
| `EHRPD` | 5 | eHRPD 网络 | 不支持 |
| `EVDO_B` | 6 | EVDO_B 网络 | 不支持 |
| `HSPA` | 7 | HSPA 网络 | 支持 |
| `HSUPA` | 8 | HSUPA 网络 | 支持 |
| `HSDPA` | 9 | HSDPA 网络 | 支持 |
| `EVDO_A` | 10 | EVDO_A 网络 | 不支持 |
| `EVDO_0` | 11 | EVDO_0 网络 | 不支持 |
| `1xRTT` | 12 | 1xRTT 网络 | 不支持 |
| `IS95B` | 13 | IS-95B 网络 | 不支持 |
| `IS95A` | 14 | IS-95A 网络 | 不支持 |
| `UMTS` | 15 | UMTS 网络 | 支持 |
| `EDGE` | 16 | EDGE 网络 | 不支持 |
| `GPRS` | 17 | GPRS 网络 | 不支持 |
| `NR5G` | 18 | NR5G 网络 | 不支持 |
| `NONE` | 19 | 无网络 | — |

> **解读**：枚举值不连续从 1 起（无 0），`NONE=19` 表示无网络。AG35-CET 是 LTE Standard + UMTS/HSPA 回落模块，无 2G(GSM)/5G(NR)/CDMA。代码里按 `serving_rat` 分支时只需处理 LTE 与 UMTS/HSPA 族。

---

#### 2.3.3 `ql_nw_set_power_mode`（第 14-15 页）
设置功耗模式。若设置为低功耗模式，**可能影响语音、数据、信号强度等事件的主动上报**，因此应在 `ql_nw_init()` 初始化后调用本函数设置为**非低功耗模式**。

```c
int ql_nw_set_power_mode(uint8_t lower_mode);
```

- **参数**：`lower_mode` **[In]** 功耗模式（位掩码），详见 2.3.3.1。
- **返回值**：`0` 成功；其他值失败。

##### 2.3.3.1 功耗模式定义（第 15 页）
功耗模式是一组按位掩码（bitmask），可按需屏蔽不同子系统的 RIL 消息上报：

```c
#define QL_NW_LOWER_POWER_MASK_DISABLE  0x00
#define QL_NW_LOWER_POWER_MASK_NORMAL   0x01
#define QL_NW_LOWER_POWER_MASK_NETWORK  0x02
#define QL_NW_LOWER_POWER_MASK_SIM      0x04
#define QL_NW_LOWER_POWER_MASK_SMS      0x08
#define QL_NW_LOWER_POWER_MASK_VOICE    0x10
```

| 宏名 | 值 | 描述 |
|---|---|---|
| `QL_NW_LOWER_POWER_MASK_DISABLE` | 0x00 | 不屏蔽任何 RIL 消息上报 |
| `QL_NW_LOWER_POWER_MASK_NORMAL` | 0x01 | 屏蔽网络、(U)SIM、SMS、语音消息上报 |
| `QL_NW_LOWER_POWER_MASK_NETWORK` | 0x02 | 屏蔽 NAS 和 DSD 服务的信息上报 |
| `QL_NW_LOWER_POWER_MASK_SIM` | 0x04 | 屏蔽 (U)SIM 卡服务信息上报 |
| `QL_NW_LOWER_POWER_MASK_SMS` | 0x08 | 屏蔽 SMS 服务信息上报 |
| `QL_NW_LOWER_POWER_MASK_VOICE` | 0x10 | 屏蔽语音服务信息上报 |

> **解读**：要让本文档里的各类 `*_ind_cb` 事件（注网/信号/小区访问/NITZ）都能正常主动上报，必须传 `QL_NW_LOWER_POWER_MASK_DISABLE`(0x00)。`NORMAL`(0x01) 会一次性屏蔽掉网络/SIM/SMS/语音四类。NAS=非接入层，DSD=Data Services Dispatcher，被 `_NETWORK` 位屏蔽会导致注网/数据事件收不到。对应到 modem_mng 工程：若要可靠收到信号/注网回调，初始化后应显式调用 `ql_nw_set_power_mode(0x00)`。

---

#### 2.3.4 `ql_nw_set_pref_nwmode_roaming`（第 15-16 页）
设置首选网络制式和漫游通知开启状态。

```c
int ql_nw_set_pref_nwmode_roaming(ql_nw_pref_nwmode_roming_info_t *p_info);
```

- **参数**：`p_info` **[In]** 首选网络制式和漫游通知开启状态，详见 2.3.4.1。
- **返回值**：`0` 成功；其他值失败。
- **注意**：原型里结构体类型名拼写为 `ql_nw_pref_nwmode_roming_info_t`（文档此处少一个 'a'，与 2.3.4.1 标题 `ql_nw_pref_nwmode_roaming_info_t` 不一致 —— 实际以头文件为准，疑似文档笔误）。

##### 2.3.4.1 `ql_nw_pref_nwmode_roaming_info_t`（第 16 页）
首选网络制式和漫游通知开启状态结构体：

```c
typedef struct
{
    uint64_t                 preferred_nw_mode;
    QL_NW_ROAM_STATE_TYPE_E  preferred_roaming;
} ql_nw_pref_nwmode_roming_info_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `uint64_t` | `preferred_nw_mode` | 首选网络制式（位掩码），详见 2.3.4.2 |
| `QL_NW_ROAM_STATE_TYPE_E` | `preferred_roaming` | 漫游通知开启状态，详见 2.3.4.3（**不支持**） |

> **解读**：`preferred_roaming` 字段标注「不支持」，即 AG35-CET 上漫游通知开关无效，只有 `preferred_nw_mode` 真正起作用。

##### 2.3.4.2 首选网络制式定义（第 16-17 页）
首选网络制式为位掩码，可「或」组合：

```c
#define QL_NW_MODE_NONE     0x00
#define QL_NW_MODE_GSM      0x01
#define QL_NW_MODE_WCDMA    0x02
#define QL_NW_MODE_CDMA     0x04
#define QL_NW_MODE_EVDO     0x08
#define QL_NW_MODE_LTE      0x10
#define QL_NW_MODE_TDSCDMA  0x20
#define QL_NW_MODE_NR5G     0x40
#define QL_NW_MODE_PRL      0x10000
```

| 宏名 | 值 | 描述 |
|---|---|---|
| `QL_NW_MODE_NONE` | 0x00 | 不设置首选网络制式 |
| `QL_NW_MODE_GSM` | 0x01 | 设置首选网络制式为 GSM（不支持） |
| `QL_NW_MODE_WCDMA` | 0x02 | 设置首选网络制式为 WCDMA |
| `QL_NW_MODE_CDMA` | 0x04 | 预留 |
| `QL_NW_MODE_EVDO` | 0x08 | 预留 |
| `QL_NW_MODE_LTE` | 0x10 | 设置首选网络制式为 LTE |
| `QL_NW_MODE_TDSCDMA` | 0x20 | 预留 |
| `QL_NW_MODE_NR5G` | 0x40 | 预留 |
| `QL_NW_MODE_PRL` | 0x10000 | 设置首选网络制式为 (U)SIM 卡 PRL 文件中存储的网络制式 |

> **解读**：AG35-CET 实际可用的只有 `WCDMA`(0x02) 与 `LTE`(0x10)（以及 PRL）。常见用法：仅 LTE = `0x10`；LTE+WCDMA 双模 = `0x12`。GSM 标「不支持」，CDMA/EVDO/TDSCDMA/NR5G 全部「预留」。`uint64_t` 宽度是为了容纳 `PRL=0x10000` 这种高位标志。

##### 2.3.4.3 `QL_NW_ROAM_STATE_TYPE_E`（第 17 页）
漫游通知开启状态枚举（**不支持**）：

```c
typedef enum QL_NW_ROAM_STATE_TYPE_ENUM
{
    QL_NW_ROAM_STATE_OFF = 0,
    QL_NW_ROAM_STATE_ON  = 1
} QL_NW_ROAM_STATE_TYPE_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_NW_ROAM_STATE_OFF` | 0 | 关闭漫游 |
| `QL_NW_ROAM_STATE_ON` | 1 | 开启漫游 |

---

#### 2.3.5 `ql_nw_get_pref_nwmode_roaming`（第 17 页）
获取首选网络制式和漫游通知开启状态。

```c
int ql_nw_get_pref_nwmode_roaming(ql_nw_pref_nwmode_roming_info_t *p_info);
```

- **参数**：`p_info` **[Out]** 获取到的首选网络制式和漫游通知开启状态，详见 2.3.4.1。
- **返回值**：`0` 成功；其他值失败。

---

#### 2.3.6 `ql_nw_get_mobile_operator_name`（第 18 页）
获取运营商信息。

```c
int ql_nw_get_mobile_operator_name(ql_nw_mobile_operator_name_info_t *p_info);
```

- **参数**：`p_info` **[Out]** 运营商信息（`long_eons`/`short_eons`/`mcc`/`mnc`），详见 2.3.2.4。
- **返回值**：`0` 成功；其他值失败。

> **解读**：这是获取当前注册运营商 PLMN 与名称的标准接口。比 modem_mng 工程里用 AT+COPS 解析、或硬编码 46000 判断要更可靠（参见工程记忆 `ec200a-operator-name-deferred`：建议用 SDK 方案而非 COPS 解析）。

---

#### 2.3.7 `ql_nw_get_cell_info`（第 18-20 页）
获取当前小区和邻区信息。

```c
int ql_nw_get_cell_info(ql_nw_cell_info_t *p_info);
```

- **参数**：`p_info` **[Out]** 获取到的小区信息，详见 2.3.7.1。
- **返回值**：`0` 成功；其他值失败。

##### 2.3.7.1 `ql_nw_cell_info_t`（第 18-19 页）
小区信息总结构体（一个聚合体，按 RAT 分别承载 GSM/UMTS/LTE/NR5G/CDMA 的小区数组，每类配 `*_valid` 有效标志 + `*_len` 有效长度）：

```c
typedef struct
{
    QL_NW_RADIO_TECH_TYPE_E serving_rat;
    uint8_t                 gsm_info_valid;
    uint8_t                 gsm_info_len;
    ql_nw_gsm_info_t        gsm_info[QL_NW_MAX_GSM_CELL_INFO_NUM];
    uint8_t                 umts_info_valid;
    uint8_t                 umts_info_len;
    ql_nw_umts_info_t       umts_info[QL_NW_MAX_UMTS_CELL_INFO_NUM];
    uint8_t                 lte_info_valid;
    uint8_t                 lte_info_len;
    ql_nw_lte_info_t        lte_info[QL_NW_MAX_LTE_CELL_INFO_NUM];
    uint8_t                 nr5g_info_valid;
    ql_nw_nr5g_info_t       nr5g_info;
    uint8_t                 cdma_info_valid;
    ql_nw_cdma_info_t       cdma_info;
} ql_nw_cell_info_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_NW_RADIO_TECH_TYPE_E` | `serving_rat` | 当前服务网络制式，详见 2.3.2.6 |
| `uint8_t` | `gsm_info_valid` | `gsm_info` 是否有效（0=无效） |
| `uint8_t` | `gsm_info_len` | `gsm_info` 的有效长度 |
| `ql_nw_gsm_info_t` | `gsm_info[]` | GSM 小区信息（**预留**） |
| `uint8_t` | `umts_info_valid` | `umts_info` 是否有效（0=无效，其他值=有效） |
| `uint8_t` | `umts_info_len` | `umts_info` 的有效长度 |
| `ql_nw_umts_info_t` | `umts_info[]` | UMTS 小区信息，详见 2.3.7.2 |
| `uint8_t` | `lte_info_valid` | `lte_info` 是否有效（0=无效，其他值=有效） |
| `uint8_t` | `lte_info_len` | `lte_info` 的有效长度 |
| `ql_nw_lte_info_t` | `lte_info[]` | LTE 小区信息，详见 2.3.7.3 |
| `uint8_t` | `nr5g_info_valid` | **预留** |
| `ql_nw_nr5g_info_t` | `nr5g_info` | **预留** |
| `uint8_t` | `cdma_info_valid` | **预留** |
| `ql_nw_cdma_info_t` | `cdma_info` | **预留** |

> **解读**：使用流程 = 先看 `serving_rat` 判断当前制式，再读对应 `*_valid`/`*_len`，遍历 `*_info[0..len-1]`。GSM/NR5G/CDMA 三类对 AG35-CET 是预留，实际只关心 `umts_info` 与 `lte_info`。数组上限由宏 `QL_NW_MAX_GSM/UMTS/LTE_CELL_INFO_NUM` 决定（含当前小区 + 邻区）。UMTS/LTE 子结构定义在第 20 页（见下一段分析）。

##### 2.3.7.2 `ql_nw_umts_info_t`（第 20 页）
UMTS 小区信息结构体：

```c
typedef struct
{
    uint32_t cid;
    uint32_t lcid;
    char     plmn[3];
    uint16_t lac;
    uint16_t uarfcn;
    uint16_t psc;
    int16_t  rssi;
} ql_nw_umts_info_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `uint32_t` | `cid` | 小区 ID 号 |
| `uint32_t` | `lcid` | UTRAN 小区 ID |
| `char[3]` | `plmn` | PLMN，可由 MCC/MNC 得出 PLMN |
| `uint16_t` | `lac` | 位置区域码（Location Area Code） |
| `uint16_t` | `uarfcn` | RF 信道号（UTRA Absolute RF Channel Number） |
| `uint16_t` | `psc` | 主扰码（Primary Scrambling Code） |
| `int16_t` | `rssi` | 接收信号强度 |

##### 2.3.7.3 `ql_nw_lte_info_t`（第 20-21 页）
LTE 小区信息结构体：

```c
typedef struct
{
    uint32_t cid;
    char     plmn[3];
    uint16_t tac;
    uint16_t pci;
    uint16_t earfcn;
    int16_t  rssi;
} ql_nw_lte_info_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `uint32_t` | `cid` | 小区 ID 号 |
| `char[3]` | `plmn` | PLMN，可由 MCC/MNC 得出 PLMN |
| `uint16_t` | `tac` | 追踪区域码（Tracking Area Code） |
| `uint16_t` | `pci` | 物理小区 ID（Physical Cell ID） |
| `uint16_t` | `earfcn` | RF 信道号，范围 0~65535 |
| `int16_t` | `rssi` | 接收信号强度 |

> **解读**：UMTS 用 `lac`/`psc`/`uarfcn`，LTE 用 `tac`/`pci`/`earfcn`，这是两种制式的关键区分字段。`plmn[3]` 为 3 字节（无 NUL，需自行截取拼接成 MCC+MNC）。`cid` 在 LTE 下是 ECI（含 eNB ID + 扇区），在 UMTS 下是小区标识。

---

#### 2.3.8 `ql_nw_get_voice_reg_status`（第 21 页）
获取**语音拨号注网状态**信息（CS 域注册状态）。

```c
int ql_nw_get_voice_reg_status(ql_nw_reg_status_info_t *p_info);
```

- **参数**：`p_info` **[Out]** 语音拨号注网状态信息，详见 2.3.8.1。
- **返回值**：`0` 成功；其他值失败。

##### 2.3.8.1 `ql_nw_reg_status_info_t`（第 21-23 页）
语音或数据拨号注网状态信息结构体（语音/数据共用同一结构）：

```c
typedef struct
{
    QL_NW_TECH_DOMAIN_TYPE_E  tech_domain;
    QL_NW_RADIO_TECH_TYPE_E   radio_tech;
    QL_NW_ROAM_STATE_TYPE_E   roaming;
    QL_NW_DENY_REASON_TYPE_E  deny_reason;
    QL_NW_SERVICE_TYPE_E      reg_state;
    char     mcc[3+1];
    char     mnc[3+1];
    uint8_t  forbidden;
    uint32_t cid;
    uint16_t lac;
    uint16_t psc;
    uint16_t tac;
    uint8_t  inPRL;
    uint8_t  css;
    uint16_t sid;
    uint16_t nid;
    uint16_t bsid;
    uint16_t nr5g_svc_opt;
    uint16_t nr5g_pci;
    uint64_t nr5g_cid;
    uint32_t nr5g_tac;
    uint8_t  endc_available;
} ql_nw_reg_status_info_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_NW_TECH_DOMAIN_TYPE_E` | `tech_domain` | 协议类型（3GPP/3GPP2），详见 2.3.8.2 |
| `QL_NW_RADIO_TECH_TYPE_E` | `radio_tech` | 网络制式，详见 2.3.2.6 |
| `QL_NW_ROAM_STATE_TYPE_E` | `roaming` | 漫游通知开启状态，详见 2.3.4.3 |
| `QL_NW_DENY_REASON_TYPE_E` | `deny_reason` | 注网被拒原因，详见 2.3.8.3 |
| `QL_NW_SERVICE_TYPE_E` | `reg_state` | 网络服务类型（无/受限/正常），详见 2.3.8.4 |
| `char[4]` | `mcc` | 移动设备国家代码 |
| `char[4]` | `mnc` | 移动设备网络代码 |
| `uint8_t` | `forbidden` | 是否禁用注网功能：0=启用，1=禁用 |
| `uint32_t` | `cid` | 小区 ID 号 |
| `uint16_t` | `lac` | 位置区域码 |
| `uint16_t` | `psc` | 主扰码，**仅在 WCDMA 系统中使用** |
| `uint16_t` | `tac` | 追踪区域码，**LTE 系统中使用** |
| `uint8_t` | `inPRL` | 是否在优先漫游列表中：0=不在，1=在 |
| `uint8_t` | `css` | 是否支持并发：0=不支持，1=支持 |
| `uint16_t` | `sid` | 预留 |
| `uint16_t` | `nid` | 预留 |
| `uint16_t` | `bsid` | 基站 ID |
| `uint16_t` | `nr5g_svc_opt` | NR5G 服务选项或类型（不支持） |
| `uint16_t` | `nr5g_pci` | NR5G 物理小区 ID（不支持） |
| `uint64_t` | `nr5g_cid` | NR5G SA 小区 ID（不支持） |
| `uint32_t` | `nr5g_tac` | NR5G 追踪区域码（不支持） |
| `uint8_t` | `endc_available` | EUTRA-NR 双连接（不支持） |

> **解读**：这是本手册信息量最大的结构。判断是否注网成功 = 看 `reg_state`（=`QL_NW_SERVICE_FULL` 正常服务）；判断为什么注不上 = 看 `deny_reason`；判断当前制式 = `radio_tech`；当前小区 = `cid`+`lac`(UMTS)/`tac`(LTE)。NR5G 相关 5 个字段全部不支持。**对 modem_mng 拨号守护进程而言，这个结构可替代 AT+CEREG/AT+CREG 解析**：`reg_state==FULL` 即注网成功，`deny_reason` 可直接定位 SIM/账号/网络侧问题，比解析 AT 应答稳健。

##### 2.3.8.2 `QL_NW_TECH_DOMAIN_TYPE_E`（第 23 页）
协议类型枚举：

```c
typedef enum QL_NW_TECH_DOMAIN_TYPE_ENUM
{
    QL_NW_TECH_DOMAIN_NONE  = 0,
    QL_NW_TECH_DOMAIN_3GPP  = 1,
    QL_NW_TECH_DOMAIN_3GPP2 = 2,
} QL_NW_TECH_DOMAIN_TYPE_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_NW_TECH_DOMAIN_NONE` | 0 | 无效协议 |
| `QL_NW_TECH_DOMAIN_3GPP` | 1 | 3GPP 协议（GSM/UMTS/LTE） |
| `QL_NW_TECH_DOMAIN_3GPP2` | 2 | 3GPP2 协议（CDMA 系） |

##### 2.3.8.3 `QL_NW_DENY_REASON_TYPE_E`（第 24-26 页）
注网被拒原因枚举（对应 3GPP EMM/MM/GMM 拒绝原因码，**值不连续**）：

```c
typedef enum QL_NW_DENY_REASON_TYPE_ENUM
{
    QL_NW_IMSI_UNKNOWN_IN_HSS_DENY_REASON                          = 2,
    QL_NW_ILLEGAL_UE_DENY_REASON                                  = 3,
    QL_NW_IMEI_NOT_ACCEPTED_DENY_REASON                          = 5,
    QL_NW_ILLEGAL_ME_DENY_REASON                                 = 6,
    QL_NW_EPS_SERVICES_NOT_ALLOWED_DENY_REASON                  = 7,
    QL_NW_EPS_SERVICES_AND_NON_EPS_SERVICES_NOT_ALLOWED_DENY_REASON = 8,
    QL_NW_UE_ID_CANNOT_BE_DERIVED_BY_THE_NETWORK_DENY_REASON     = 9,
    QL_NW_IMPLICITY_DETACHED_DENY_REASON                         = 10,
    QL_NW_PLMN_NOT_ALLOWED_DENY_REASON                          = 11,
    QL_NW_TRACKING_AREA_NOT_ALLOWED_DENY_REASON                = 12,
    QL_NW_ROAMING_NOT_ALLOWED_IN_THIS_TRACNING_AREA_DENY_REASON = 13,
    QL_NW_EPS_SERVICES_NOT_ALLOWED_IN_THIS_PLMN_DENY_REASON     = 14,
    QL_NW_NO_SUITALBE_CELLS_IN_TRACKING_AREA_DENY_REASON        = 15,
    QL_NW_MSC_TEMPORARILY_NOT_REACHABLE_DENY_REASON            = 16,
    QL_NW_NETWORK_FAILURE_DENY_REASON                          = 17,
    QL_NW_CS_DOMAIN_NOT_AVAILABLE_DENY_REASON                  = 18,
    QL_NW_EMS_FAILURE_DENY_REASON                              = 19,
    QL_NW_MAC_FAILURE_DENY_REASON                              = 20,
    QL_NW_SYNC_FAILURE_DENY_REASON                             = 21,
    QL_NW_CONGESTION_DENY_REASON                               = 22,
    QL_NW_UE_SECURITY_CAPABILITIES_MISMATCH_DENY_REASON       = 23,
    QL_NW_SECURITY_MODE_REJECTED_DENY_REASON                  = 24,
    QL_NW_NOT_AUTHORIZED_FOR_THIS_CSG_DENY_REASON             = 25,
    QL_NW_NON_EPS_AUTHENTICATION_UNCACCEPTABLE_DENY_REASON    = 26,
    QL_NW_REQUESTED_SERVICE_OPTION_NOT_AUTHORIZED_IN_THIS_PLMN_DENY_REASON = 35,
    QL_NW_CS_SERVICE_TEMPORARILY_NOT_AVAILABLE_DENY_REASON    = 39,
    QL_NW_NO_ESP_BEARER_CONTEXT_ACTIVATED_DENY_REASON         = 40,
    QL_NW_SEMAMTICALLY_INCORRECT_MESSAGE_DENY_REASON          = 95,
    QL_NW_INVALID_MANDATORY_INFORMATION_DENY_REASON           = 96,
    QL_NW_MESSAGE_TYPE_NON_EXISTENT_OR_NOT_IMPLEMENTED_DENY_REASON = 97,
    QL_NW_MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCAL_STATE_DENY_REASON = 98,
    QL_NW_INFORMATION_ELEMENT_NON_EXISTENT_OR_NOT_IMPLEMENTED_DENY_REASON = 99,
    QL_NW_CONDITIONAL_IE_ERROR_DENY_REASON                    = 100,
    QL_NW_MESSAGE_NOT_COMPATIBLE_WITH_PROTOCAL_STATE_DENY_REASON = 101,
} QL_NW_DENY_REASON_TYPE_E;
```

| 成员（值） | 描述 |
|---|---|
| `IMSI_UNKNOWN_IN_HSS`(2) | HSS 中未知 IMSI |
| `ILLEGAL_UE`(3) | 非法移动用户 |
| `IMEI_NOT_ACCEPTED`(5) | 非法 IMEI |
| `ILLEGAL_ME`(6) | 非法移动设备 |
| `EPS_SERVICES_NOT_ALLOWED`(7) | EPS 服务被拒绝 |
| `EPS_SERVICES_AND_NON_EPS_SERVICES_NOT_ALLOWED`(8) | EPS 服务和非 EPS 服务被拒绝 |
| `UE_ID_CANNOT_BE_DERIVED_BY_THE_NETWORK`(9) | 用户 ID 不可派生 |
| `IMPLICITY_DETACHED`(10) | 隐式去附着 |
| `PLMN_NOT_ALLOWED`(11) | PLMN 被拒绝 |
| `TRACKING_AREA_NOT_ALLOWED`(12) | 跟踪区域被拒绝 |
| `ROAMING_NOT_ALLOWED_IN_THIS_TRACNING_AREA`(13) | 跟踪区域不允许漫游 |
| `EPS_SERVICES_NOT_ALLOWED_IN_THIS_PLMN`(14) | EPS 服务在此 PLMN 不可用 |
| `NO_SUITALBE_CELLS_IN_TRACKING_AREA`(15) | 无可用小区 |
| `MSC_TEMPORARILY_NOT_REACHABLE`(16) | MSC 暂时不可达 |
| `NETWORK_FAILURE`(17) | 网络故障 |
| `CS_DOMAIN_NOT_AVAILABLE`(18) | CS 域不可用 |
| `EMS_FAILURE`(19) | EMS 故障 |
| `MAC_FAILURE`(20) | MAC 故障 |
| `SYNC_FAILURE`(21) | 同步故障 |
| `CONGESTION`(22) | 网络拥塞 |
| `UE_SECURITY_CAPABILITIES_MISMATCH`(23) | 用户安全能力不匹配 |
| `SECURITY_MODE_REJECTED`(24) | 安全模式拒绝或未指定 |
| `NOT_AUTHORIZED_FOR_THIS_CSG`(25) | CSG 未认证 |
| `NON_EPS_AUTHENTICATION_UNCACCEPTABLE`(26) | 非 EPS 服务认证失败 |
| `REQUESTED_SERVICE_OPTION_NOT_AUTHORIZED_IN_THIS_PLMN`(35) | 请求选项未被认证 |
| `CS_SERVICE_TEMPORARILY_NOT_AVAILABLE`(39) | CS 服务暂时不可用 |
| `NO_ESP_BEARER_CONTEXT_ACTIVATED`(40) | ESP 上下文未被激活 |
| `SEMAMTICALLY_INCORRECT_MESSAGE`(95) | 语法错误 |
| `INVALID_MANDATORY_INFORMATION`(96) | （强制信息无效） |
| `MESSAGE_TYPE_NON_EXISTENT_OR_NOT_IMPLEMENTED`(97) | 消息类型不存在或未实现 |
| `MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCAL_STATE`(98) | 消息类型与协议不兼容 |
| `INFORMATION_ELEMENT_NON_EXISTENT_OR_NOT_IMPLEMENTED`(99) | 消息不存在或未实现 |
| `CONDITIONAL_IE_ERROR`(100) | IE 条件错误 |
| `MESSAGE_NOT_COMPATIBLE_WITH_PROTOCAL_STATE`(101) | 消息与协议不兼容 |

> **解读**：这些就是 3GPP TS 24.008/24.301 的 EMM/MM cause 值。运维定位时最有用的几个：**11(PLMN 被拒)/13(漫游受限)/2/3/6(SIM 非法)/22(拥塞)**。其中 11/13/2/3/6 多为「重置也救不回来」类（账号/卡/漫游协议问题）—— 这正印证了 modem_mng 工程中「从未连接成功则门控掉 L1/L2/L3 恢复」的设计：若 `deny_reason` 是这类，盲目 CFUN 重置无意义。注意宏名中的拼写错误（`TRACNING`、`SEMAMTICALLY`、`UNCACCEPTABLE`、`PROTOCAL`、`SUITALBE`、`IMPLICITY`）是 SDK 原样，引用时勿"纠正"。

##### 2.3.8.4 `QL_NW_SERVICE_TYPE_E`（第 26-27 页）
网络服务类型枚举：

```c
typedef enum QL_NW_SERVICE_TYPE_ENUM
{
    QL_NW_SERVICE_NONE    = 0,
    QL_NW_SERVICE_LIMITED = 1,
    QL_NW_SERVICE_FULL    = 2,
} QL_NW_SERVICE_TYPE_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_NW_SERVICE_NONE` | 0 | 无服务 |
| `QL_NW_SERVICE_LIMITED` | 1 | 受限服务（仅紧急呼叫等） |
| `QL_NW_SERVICE_FULL` | 2 | 正常服务（已注网，可正常通信） |

---

#### 2.3.9 `ql_nw_get_data_reg_status`（第 27 页）
获取**数据拨号注网状态**信息（PS 域注册状态）。与 2.3.8 共用 `ql_nw_reg_status_info_t` 输出结构。

```c
int ql_nw_get_data_reg_status(ql_nw_reg_status_info_t *p_info);
```

- **参数**：`p_info` **[Out]** 数据拨号注网状态信息，详见 2.3.8.1。
- **返回值**：`0` 成功；其他值失败。

> **解读**：拨号上网真正关心的是这个（PS/数据域），`reg_state==FULL` 才能拨号成功。语音域(2.3.8)对纯数据应用可忽略。

---

#### 2.3.10 `ql_nw_get_signal_strength`（第 27-28 页）
获取当前信号强度信息以及信号强度等级。

```c
int ql_nw_get_signal_strength(ql_nw_signal_strength_info_t *p_info,
                              QL_NW_SIGNAL_STRENGTH_LEVEL_E *p_level);
```

- **参数**：
  - `p_info` **[Out]** 信号强度信息，详见 2.3.10.1。**注册多个网络时（如电信卡可能注册到 2 个网络），则会有两种信号强度信息**。
  - `level`（`p_level`）**[Out]** 信号强度等级（归一化的 0~4 档），详见 2.3.10.4。
- **返回值**：`0` 成功；其他值失败。

##### 2.3.10.1 `ql_nw_signal_strength_info_t`（第 28-29 页）
信号强度信息总结构体（按制式分别承载，各配 `has_*` 有效标志）：

```c
typedef struct
{
    uint8_t                       has_gsm;
    ql_nw_gsm_signal_info_t       gsm;
    uint8_t                       has_wcdma;
    ql_nw_wcdma_signal_info_t     wcdma;
    uint8_t                       has_tdscdma;
    ql_nw_tdscdma_signal_info_t   tdscdma;
    uint8_t                       has_lte;
    ql_nw_lte_signal_info_t       lte;
    uint8_t                       has_nr5g;
    ql_nw_nr5g_signal_info_t      nr5g;
    uint8_t                       has_cdma;
    ql_nw_cdma_signal_info_t      cdma;
    uint8_t                       has_hdr;
    ql_nw_hdr_signal_info_t       hdr;
} ql_nw_signal_strength_info_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `uint8_t` | `has_gsm` | `gsm` 是否有效（0=无效） |
| `ql_nw_gsm_signal_info_t` | `gsm` | **预留** |
| `uint8_t` | `has_wcdma` | `wcdma` 是否有效（0=无效，其他值=有效） |
| `ql_nw_wcdma_signal_info_t` | `wcdma` | WCDMA 信号强度信息，详见 2.3.10.2 |
| `uint8_t` | `has_tdscdma` | `tdscdma` 是否有效（0=无效） |
| `ql_nw_tdscdma_signal_info_t` | `tdscdma` | **预留** |
| `uint8_t` | `has_lte` | `lte` 是否有效（0=无效，其他值=有效） |
| `ql_nw_lte_signal_info_t` | `lte` | LTE 信号强度信息，详见 2.3.10.3 |
| `uint8_t` | `has_nr5g` | `nr5g` 是否有效（0=无效） |
| `ql_nw_nr5g_signal_info_t` | `nr5g` | **预留** |
| `uint8_t` | `has_cdma` | `cdma` 是否有效（0=无效） |
| `ql_nw_cdma_signal_info_t` | `cdma` | **预留** |
| `uint8_t` | `has_hdr` | `hdr` 是否有效（0=无效） |
| `ql_nw_hdr_signal_info_t` | `hdr` | **预留** |

> **解读**：AG35-CET 实际只填 `wcdma` 与 `lte`（GSM/TDSCDMA/NR5G/CDMA/HDR 全预留）。用法：先判 `has_lte`/`has_wcdma` 再读对应子结构。一卡可能同时上报两套（电信 VoLTE + 数据），所以需都检查。

##### 2.3.10.2 `ql_nw_wcdma_signal_info_t`（第 29 页）
WCDMA 信号强度结构体：

```c
typedef struct
{
    int8_t  rssi;
    int16_t ecio;
} ql_nw_wcdma_signal_info_t;
```

| 类型 | 参数 | 描述 | 单位 |
|---|---|---|---|
| `int8_t` | `rssi` | 接收信号强度指示 | dBm |
| `int16_t` | `ecio` | Ec/Io（载干比） | **-0.5 dB**（即数值×(-0.5)=实际 dB） |

##### 2.3.10.3 `ql_nw_lte_signal_info_t`（第 29-30 页）
LTE 信号强度结构体：

```c
typedef struct
{
    int8_t  rssi;
    int8_t  rsrq;
    int16_t rsrp;
    int16_t snr;
} ql_nw_lte_signal_info_t;
```

| 类型 | 参数 | 描述 | 单位 |
|---|---|---|---|
| `int8_t` | `rssi` | 接收信号强度指示 | dBm |
| `int8_t` | `rsrq` | 参考信号接收质量 | dB |
| `int16_t` | `rsrp` | 参考信号接收功率 | dBm |
| `int16_t` | `snr` | 信噪比 | **0.1 dB**（数值×0.1=实际 dB） |

> **解读**：注意单位换算 —— LTE 的 `snr` 是 0.1 dB 步进（如 `snr=125` → 12.5 dB），WCDMA 的 `ecio` 是 -0.5 dB 步进。`rsrp`/`rssi`/`rsrq` 是直接 dBm/dB。这是评估 LTE 链路质量的标准四元组：RSRP（覆盖）、RSRQ（质量）、SINR/SNR（干扰）、RSSI（总功率）。

##### 2.3.10.4 `QL_NW_SIGNAL_STRENGTH_LEVEL_E`（第 30 页）
信号强度等级枚举（归一化 5 档 + MIN/MAX 边界哨兵）：

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
} QL_NW_SIGNAL_STRENGTH_LEVEL_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `..._MIN` | -1 | 边界哨兵（下界） |
| `..._NONE` | 0 | 无信号 |
| `..._POOR` | 1 | 信号差 |
| `..._MODERATE` | 2 | 信号中等 |
| `..._GOOD` | 3 | 信号良 |
| `..._GREAT` | 4 | 信号优 |
| `..._MAX` | 5 | 边界哨兵（上界） |

> **解读**：`p_level` 给出归一化的 0~4 档，应用层做信号图标可直接用，无需自己把 RSRP 映射成格数。`MIN(-1)`/`MAX(5)` 用于范围校验。

---

#### 2.3.11 `ql_nw_get_cell_access_status`（第 31 页）
获取小区的访问状态（小区当前允许哪类呼叫接入）。

```c
int ql_nw_get_cell_access_status(QL_NW_CELL_ACCESS_STATE_TYPE_E *p_info);
```

- **参数**：`p_info` **[Out]** 小区访问状态，详见 2.3.11.1。
- **返回值**：`0` 成功；其他值失败。

##### 2.3.11.1 `QL_NW_CELL_ACCESS_STATE_TYPE_E`（第 31 页）
小区访问状态枚举：

```c
typedef enum QL_NW_CELL_ACCESS_STATE_TYPE_ENUM
{
    QL_NW_CELL_ACCESS_NONE           = 0,
    QL_NW_CELL_ACCESS_NORMAL_ONLY    = 1,
    QL_NW_CELL_ACCESS_EMERGENCY_ONLY = 2,
    QL_NW_CELL_ACCESS_NO_CALLS       = 3,
    QL_NW_CELL_ACCESS_ALL_CALLS      = 4,
} QL_NW_CELL_ACCESS_STATE_TYPE_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_NW_CELL_ACCESS_NONE` | 0 | 未知访问状态 |
| `QL_NW_CELL_ACCESS_NORMAL_ONLY` | 1 | 正常访问状态（仅普通呼叫） |
| `QL_NW_CELL_ACCESS_EMERGENCY_ONLY` | 2 | 紧急访问状态（仅紧急呼叫） |
| `QL_NW_CELL_ACCESS_NO_CALLS` | 3 | 无法访问状态 |
| `QL_NW_CELL_ACCESS_ALL_CALLS` | 4 | 全访问状态 |

---

#### 2.3.12 `ql_nw_get_nitz_time_info`（第 32 页）
获取网络时间信息（NITZ = Network Identity and Time Zone，运营商下发的时间）。

```c
int ql_nw_get_nitz_time_info(ql_nw_nitz_time_info_t *p_info);
```

- **参数**：`p_info` **[Out]** 网络时间信息，详见 2.3.12.1。
- **返回值**：`0` 成功；其他值失败。

##### 2.3.12.1 `ql_nw_nitz_time_info_t`（第 32 页）
网络时间信息结构体：

```c
typedef struct
{
    char     nitz_time[QL_NW_NITZ_BUF_LEN + 1];
    uint64_t abs_time;
    int8_t   leap_sec;
} ql_nw_nitz_time_info_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `char[QL_NW_NITZ_BUF_LEN+1]` | `nitz_time` | UTC 时间字符串。格式：`YY/MM/DD,HH:MM:SS+/-TZ`。`+`代表东区、`-`代表西区，**TZ 以 15 分钟为单位**。如 `+32` 代表东 8 区（32×15min=480min=8h），`-24` 代表西 7 区 |
| `uint64_t` | `abs_time` | 绝对时间，相对于 1970-01-01 00:00:00 UTC（Unix 时间戳） |
| `int8_t` | `leap_sec` | 闰秒（时间误差调整阈值） |

> **解读**：与本工程高度相关 —— 工程记忆 `rk3576-iomng-systime-architecture` 指出系统时间靠 NTP（4G+chronyd）维持、无硬件 RTC。NITZ 是另一条获取网络时间的路径：运营商在注网时下发，无需联网 NTP 即可拿到。`abs_time` 可直接 `clock_settime` 校时。TZ 是 15 分钟单位的有符号整数（注意东 8 区是 +32 而非 +8）。`logger_sd.c` 的「pre-time-sync 日志路由到 unsynced/」逻辑若改用 NITZ，可更早完成校时。

---

#### 2.3.13 `ql_nw_set_voice_reg_ind_cb`（第 33 页）
注册**语音拨号注网事件**回调函数（注网状态变化时主动上报）。

```c
int ql_nw_set_voice_reg_ind_cb(ql_nw_voice_reg_ind_cb cb_func);
```

- **参数**：`cb_func` **[In]** 语音拨号注网事件回调函数，详见 2.3.13.1。
- **返回值**：`0` 成功；其他值失败。

##### 2.3.13.1 `ql_nw_voice_reg_ind_cb`（第 33 页）
```c
typedef void (*ql_nw_voice_reg_ind_cb)(ql_nw_reg_status_info_t *p_info);
```
- `p_info` **[In]**：语音拨号注网状态信息，详见 2.3.8.1。返回值：无。

---

#### 2.3.14 `ql_nw_set_data_reg_ind_cb`（第 33-34 页）
注册**数据拨号注网事件**回调函数。

```c
int ql_nw_set_data_reg_ind_cb(ql_nw_data_reg_ind_cb cb_func);
```

- **参数**：`cb_func` **[In]** 数据拨号注网事件回调函数，详见 2.3.14.1。
- **返回值**：`0` 成功；其他值失败。

##### 2.3.14.1 `ql_nw_data_reg_ind_cb`（第 34 页）
```c
typedef void (*ql_nw_data_reg_ind_cb)(ql_nw_reg_status_info_t *p_info);
```
（文档原文把 `typedef` 误印成 `type def`，是排版笔误。）
- `p_info` **[In]**：数据拨号注网状态信息，详见 2.3.8.1。返回值：无。

> **解读**：对拨号守护进程，**注册 `data_reg_ind_cb` 是替代轮询 AT+CEREG 的事件驱动方案**：PS 域注网状态一变（掉网/恢复/被拒）立即回调，可比 modem_mng 当前 50ms 轮询更及时地触发恢复逻辑。注意回调在 SDK 的 indication 线程执行，与主拨号循环要做好同步（参考工程里 logger 用递归 mutex 的做法）。

---

#### 2.3.15 `ql_nw_set_signal_strength_ind_cb`（第 34-35 页）
注册**信号强度事件**回调函数。

```c
int ql_nw_set_signal_strength_ind_cb(ql_nw_signal_strength_ind_cb cb_func);
```

- **参数**：`cb_func` **[In]** 信号强度事件回调函数，详见 2.3.15.1。
- **返回值**：`0` 成功；其他值失败。

##### 2.3.15.1 `ql_nw_signal_strength_ind_cb`（第 35 页）
```c
typedef void (*ql_nw_signal_strength_ind_cb)(ql_nw_signal_strength_info_t *p_info,
                                             QL_NW_SIGNAL_STRENGTH_LEVEL_E level);
```
- `p_info` **[In]**：信号强度信息（2.3.10.1）；`level` **[In]**：信号强度等级（2.3.10.4）。返回值：无。

---

#### 2.3.16 `ql_nw_set_cell_access_status_ind_cb`（第 35-36 页）
注册**小区访问状态事件**回调函数。

```c
int ql_nw_set_cell_access_status_ind_cb(ql_nw_cell_access_status_ind_cb cb_func);
```

- **参数**：`cb_func` **[In]** 小区访问状态事件回调函数，详见 2.3.16.1。
- **返回值**：`0` 成功；其他值失败。

##### 2.3.16.1 `ql_nw_cell_access_status_ind_cb`（第 36 页）
```c
typedef void (*ql_nw_cell_access_status_ind_cb)(QL_NW_CELL_ACCESS_STATE_TYPE_E status);
```
- `status` **[In]**：小区当前的访问状态（2.3.11.1）。返回值：无。

---

#### 2.3.17 `ql_nw_set_nitz_time_update_ind_cb`（第 36-37 页）
注册**网络时间事件**回调函数（NITZ 时间更新时主动上报）。

```c
int ql_nw_set_nitz_time_update_ind_cb(ql_nw_nitz_time_update_ind_cb cb_func);
```

- **参数**：`cb_func` **[In]** 网络时间事件回调函数，详见 2.3.17.1。
- **返回值**：`0` 成功；其他值失败。

##### 2.3.17.1 `ql_nw_nitz_time_update_ind_cb`（第 36 页）
```c
typedef void (*ql_nw_nitz_time_update_ind_cb)(ql_nw_nitz_time_info_t *p_info);
```
- `p_info` **[Out]**：当前网络时间（2.3.12.1）。返回值：无。

> **解读**：这是上文「用 NITZ 校时」的事件驱动版本 —— 注册后，一旦运营商下发时间即回调，应用可在回调里直接 `clock_settime(abs_time)`，免去主动轮询。对无 RTC 的设备（如 RK3576）是冷启动快速校时的可靠手段。

---

#### 2.3.18 `ql_nw_set_service_error_cb`（第 37 页）
设置**服务异常事件**回调函数（网络服务底层异常时通知）。

```c
int ql_nw_set_service_error_cb(ql_nw_service_error_cb_f cb);
```

- **参数**：`cb` **[In]** 服务异常回调函数，详见 2.3.18.1。
- **返回值**：`0` 成功；其他值失败。

##### 2.3.18.1 `ql_nw_service_error_cb_f`（第 37 页）
```c
typedef void (*ql_nw_service_error_cb_f)(int error);
```
- `error` **[Out]**：错误码。目前定义：`QL_ERR_SERVICE_ABORT` = 服务终止。返回值：无。

> **解读**：收到 `QL_ERR_SERVICE_ABORT` 表示网络服务整体异常退出，按 2.4 节恢复措施应 `ql_nw_deinit()` → `ql_nw_init()` 重新初始化。这是整套 API 的「看门狗」入口，守护进程应注册它来感知 SDK 网络服务崩溃。

---

#### 2.3.19 `ql_nw_deinit`（第 38 页）
去初始化网络服务。

```c
int ql_nw_deinit(void);
```

- **参数**：无
- **返回值**：`0` 成功；其他值失败。

---

### 2.4 错误码及恢复措施（表 2，第 38-39 页）

| 返回值 | 错误码 | 描述 | 恢复措施 |
|---|---|---|---|
| -1010 | `QL_ERR_INVALID_ARG` | API 参数无效 | 检查 API 参数是否合法，确保合法后再次尝试 |
| -1067 | `QL_ERR_SERVICE_NOT_READY` | 服务未就绪 | 重复调用 `ql_nw_init()`，尝试一段时间（建议 30 秒）；若仍失败，抓取调用 API 全过程 log 提交移远技术支持 |
| -1099 | `QL_ERR_SERVICE_ABORT` | 服务异常退出 | 先 `ql_nw_deinit()` 再 `ql_nw_init()`，待初始化成功后再尝试 |
| -1034 | `QL_ERR_NOT_INIT` | 未初始化 | 调用 `ql_nw_init()` 初始化蜂窝网络信息服务 |
| -1002 | `QL_ERR_INTERNAL` | 内部错误 | 先 `ql_nw_deinit()` 再 `ql_nw_init()`，若仍报错则抓 log 提交支持 |
| -1003 | `QL_ERR_UNKNOWN` | 未知错误 | 同上（deinit→init，仍失败则抓 log） |
| -1004 | `QL_ERR_GENERIC` | 一般错误 | 同上 |
| -1030 | `QL_ERR_NO_MEM` | 内存不足 | 查询模块内存使用率是否过高，释放内存后重试；排查内存泄漏 |
| -1042 | `QL_ERR_NO_NETWORK_FOUND` | 无网络 | 检查注网状态，成功注网后重试；若持续无网络，可尝试切换飞行模式后再试；仍失败抓 log |
| -1033 | `QL_ERR_TIMEOUT` | 操作超时 | 尝试重新调用；仍失败抓 log 提交支持 |

> **解读**：通用恢复套路有三档：①参数类(-1010)自查参数；②未初始化/未就绪类(-1034/-1067)调 init 并等待；③异常/内部类(-1099/-1002/-1003/-1004)走 deinit→init 重建。`-1042 无网络`建议「切飞行模式再切回」——等价于 AT+CFUN=0/1 的 RF 复位，与 modem_mng 的 L2 恢复档同理。这张表可直接指导守护进程对 `ql_nw_*` 返回值做分级处理。

## 3. 示例（第 40-54 页）

> 本章所有代码示例均摘自 `ql-sdk/sample/test_sdk_api/m_nw.c`，该文件可查看接口函数的完整示例。**程序启动后必须先调用 `ql_nw_init()` 初始化网络服务**。示例程序是一个交互式菜单（输入 0~15 选择测试项，-1 退出），用到若干内部辅助函数：`t_get_hex()`/`t_get_int()`（读取用户十六进制/十进制输入）、`internal_nw_get_mcc_mnc_value()`（把 `plmn[3]` 转成 MCC/MNC）、`internal_nw_get_tech_domain()`/`internal_nw_get_radio_tech()`/`internal_nw_get_service_option()`/`internal_nw_get_signal_strength_level()`（把枚举值转成可读字符串）。

### 3.1 网络扫描（第 40 页）
```c
void item_ql_nw_network_scan(void)
{
    int ret;
    int async_id = 0;

    ret = ql_nw_network_scan(&async_id, nw_network_scan_async_cb);
    if(ret == QL_ERR_OK)
        printf("async network scan succeed, token id is %d\n", async_id);
    else
        printf("async network scan failed, token id is %d, ret=%d", async_id, ret);
}
```
**实测输出**（节选）：
```
network scan async callback, async id is 2, list_len-1, detail info:
    [0]: long_eons=CMCC, short_eons=CMCC, mcc=460, mnc=00,
         network_status = CURRENT_SERVING, radio_tech = UMTS
```
> 解读：扫描是异步的，发起时拿到 `async_id`（token），真正结果在回调里按 token 区分。示例中扫到中国移动（mcc=460,mnc=00），状态 CURRENT_SERVING、制式 UMTS。`QL_ERR_OK` 即 0。

### 3.2 设置功耗模式（第 40-41 页）
```c
void item_ql_nw_set_power_mode(void)
{
    int ret;
    uint32_t mode = 0;
    printf("please input power mode mask hex(VOICE | SMS | SIM | NETWORK | NORMAL): ");
    ret = t_get_hex(&mode);
    if(ret != 0) { printf("Invalid input\n"); return; }
    ret = ql_nw_set_power_mode(mode);
    printf("ql_nw_set_lower_power_mode ret = %d\n", ret);
}
```
> 解读：交互式输入位掩码（VOICE|SMS|SIM|NETWORK|NORMAL 对应 2.3.3.1 的宏）。要全开事件上报应输入 0。

### 3.3 设置首选网络制式和漫游通知开启状态（第 41-42 页）
```c
void item_ql_nw_set_pref_nwmode_roaming(void)
{
    int ret;
    uint32_t mask = 0;
    ql_nw_pref_nwmode_roaming_info_t t_info;

    memset(&t_info, 0, sizeof(t_info));
    printf("please input config mask hex(TDSCDMA | LTE | EVDO | CDMA | WCDMA | GSM): ");
    ret = t_get_hex(&mask);
    if(ret != 0) { printf("Invalid input\n"); return; }
    t_info.preferred_nw_mode = mask;
#if 0
    printf("please input roaming pref(0:off 1:on): ");
    ret = t_get_int((int *)&mask);
    if(ret != 0) { printf("Invalid input\n"); return; }
    t_info.preferred_roaming = mask;
#endif
    ret = ql_nw_set_pref_nwmode_roaming(&t_info);
    printf("ql_nw_set_config ret = %d\n", ret);
}
```
> 解读：漫游设置段用 `#if 0` 整段注释掉 —— 印证 2.3.4 中 `preferred_roaming`「不支持」。实际只设 `preferred_nw_mode`。例如仅 LTE 输入 `10`，LTE+WCDMA 输入 `12`。

### 3.4 获取首选网络制式和漫游通知开启状态（第 42 页）
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
> 解读：同样把 roaming 字段打印用 `#if 0/#else` 屏蔽，只打印 `preferred_nw_mode`（`%#llx` 十六进制，因为是 uint64_t）。

### 3.5 获取运营商信息（第 42 页）
```c
void item_ql_nw_get_mobile_operator_name(void)
{
    int ret;
    ql_nw_mobile_operator_name_info_t t_info;
    memset(&t_info, 0, sizeof(t_info));
    ret = ql_nw_get_mobile_operator_name(&t_info);
    printf("ql_nw_get_operator_name ret = %d, long_eons=%s, short_eons=%s, mcc=%s, mnc=%s\n",
           ret, t_info.long_eons, t_info.short_eons, t_info.mcc, t_info.mnc);
}
```

### 3.6 获取小区信息（第 43-45 页）
完整示例遍历 GSM/UMTS/LTE/NR5G/CDMA 五类，按各自 `*_valid`/`*_len` 打印，并用 `internal_nw_get_mcc_mnc_value(plmn, 3, &mcc, &mnc)` 把 `plmn[3]` 转 MCC/MNC：
```c
void item_ql_nw_get_cell_info(void)
{
    int i, ret;
    unsigned short mcc, mnc;
    ql_nw_cell_info_t t_info;
    memset(&t_info, 0, sizeof(t_info));
    ret = ql_nw_get_cell_info(&t_info);
    printf("ql_nw_get_cell_info ret = %d, detail info:\n", ret);

    if(t_info.gsm_info_valid) {              /* GSM：cid/plmn/lac/arfcn/bsic/rssi */
        for(i = 0; i < t_info.gsm_info_len; i++) {
            printf("\tcid=%d,plmn=0x%02x 0x%02x 0x%02x,lac=%d,arfcn=%d,bsic=%d,rssi=%d,", ...);
            internal_nw_get_mcc_mnc_value(t_info.gsm_info[i].plmn, 3, &mcc, &mnc);
            printf("convert plmn to mcc=%d,mnc=%02d\n", mcc, mnc);
        }
    }
    if(t_info.umts_info_valid) {             /* UMTS：cid/lcid/plmn/lac/uarfcn/psc/rssi */ ... }
    if(t_info.lte_info_valid)  {             /* LTE：cid/plmn/tac/pci/earfcn/rssi */ ... }
    if(t_info.nr5g_info_valid) {             /* NR5G：cid/plmn/tac/pci/arfcn/rsrp/rsrq/snr */ ... }
    if(t_info.cdma_info_valid) {             /* CDMA：sid/nid/bid/refpn/base_lat/base_long/rssi */ ... }
}
```
> 解读：示例代码虽然把 GSM/NR5G/CDMA 分支都写了（便于跨平台复用），但 AG35-CET 实际只会进 UMTS/LTE 分支（其余 `*_valid` 恒为 0）。注意 `plmn` 是 3 字节裸数组、用 `0x%02x` 逐字节打印，再调内部函数转 MCC/MNC。NR5G 示例分支多出 `arfcn/rsrp/rsrq/snr` 字段、CDMA 多出 `refpn/base_lat/base_long`（这些子结构本手册正文未展开，属预留）。

### 3.7 获取语音拨号注网状态（第 45-47 页）
```c
void item_ql_nw_get_voice_reg_status(void)
{
    int ret;
    char domain_info[16] = {0};
    char radio_info[16] = {0};
    char svc_opt[128] = {0};
    ql_nw_reg_status_info_t t_info;
    memset(&t_info, 0, sizeof(t_info));
    ret = ql_nw_get_voice_reg_status(&t_info);
    printf("ql_nw_get_voice_reg_status ret = %d, detail info:\n", ret);

    /* tech_domain / radio_tech 枚举转字符串，转不出则打印原始数值 */
    if(internal_nw_get_tech_domain(t_info.tech_domain, domain_info, sizeof(domain_info)) == 0)
        printf("\ttech_domain is unrecognized:%d, ", t_info.tech_domain);
    else
        printf("\ttech_domain = %s, ", domain_info);
    if(internal_nw_get_radio_tech(t_info.radio_tech, radio_info, sizeof(radio_info)) == 0)
        printf("radio_tech is unrecognized:%d, ", t_info.radio_tech);
    else
        printf("radio_tech = %s, ", radio_info);

    printf("roaming = %d, reg_status = %d, deny_reason = %d\n",
           t_info.roaming, t_info.reg_state, t_info.deny_reason);

    if(QL_NW_RADIO_TECH_NR5G != t_info.radio_tech) {   /* 非 NR5G：打印 3GPP 字段 */
        printf("\tmcc=%s, mnc=%s, forbidden=%d, cid=0x%X, lac=%d, psc=%d, tac=%d, endc_available=%d\n", ...);
        printf("\tinPRL=%d, css=%d, sid=%d, nid=%d, bsid=%d\n", ...);
    } else {                                            /* NR5G 分支：nr5g_cid/pci/tac/svc_opt */ ... }
}
```
**实测输出**（菜单选 6）：
```
ql_nw_get_voice_reg_status ret = 0, detail info:
    tech_domain = 3GPP2, radio_tech = LTE, roaming = 0, reg_status = 2, deny_reason = 0
    mcc=460, mnc=00, forbidden=0, cid=0xF4c2521, lac=0, psc=0, tac=22115
    inPRL=0, css=0, sid=0, nid=0, bsid=0
```
> 解读：`reg_status=2` 即 `QL_NW_SERVICE_FULL`（正常服务），`deny_reason=0`（无拒绝）。LTE 下 `tac=22115` 有效、`lac/psc=0`（属 UMTS 字段）。注意 `radio_tech` 用 `!= QL_NW_RADIO_TECH_NR5G` 判断是否走 3GPP 分支。

### 3.8 获取数据拨号注网状态（第 47 页）
> 方法同获取语音拨号注网状态类似，请参考第 3.7 章。（即把 `ql_nw_get_voice_reg_status` 换成 `ql_nw_get_data_reg_status`，结构体相同。）

### 3.9 获取信号强度信息（第 47-49 页）
```c
void item_ql_nw_get_signal_strength(void)
{
    int ret;
    char level_info[16] = {0};
    ql_nw_signal_strength_info_t info;
    QL_NW_SIGNAL_STRENGTH_LEVEL_E level = QL_NW_SIGNAL_STRENGTH_LEVEL_NONE;
    memset(&info, 0, sizeof(info));
    ret = ql_nw_get_signal_strength(&info, &level);
    if (QL_ERR_OK != ret) { printf("failed, ret = %d\n", ret); return; }

    if(info.has_gsm)     printf("gsm_sig_info: rssi=%hhd\n", info.gsm.rssi);
    if(info.has_wcdma)   printf("wcdma_sig_info: rssi=%hhd, ecio=%hd\n", info.wcdma.rssi, info.wcdma.ecio);
    if(info.has_tdscdma) printf("tdscdma_sig_info: rssi=%hhd, rscp=%hhd, ecio=%hd, sinr=%hhd\n", ...);
    if(info.has_lte)     printf("lte_sig_info: rssi=%hhd, rsrq=%hhd, rsrp=%hd, snr=%hd\n",
                                info.lte.rssi, info.lte.rsrq, info.lte.rsrp, info.lte.snr);
    if(info.has_nr5g)    printf("nr5g_sig_info: rsrp=%hd, rsrq=%hd, snr=%hd\n", ...);
    if(info.has_cdma)    printf("cdma_sig_info: rssi=%hhd, ecio=%hd\n", ...);
    if(info.has_hdr)     printf("hdr_sig_info: rssi=%hhd, ecio=%hd, sinr=%hd, io=%d\n", ...);

    if(internal_nw_get_signal_strength_level(level, level_info, sizeof(level_info)) == 0)
        printf("signal strength level is %d, unrecognized\n", level);
    else
        printf("signal strength level is %s\n", level_info);
}
```
**实测输出**（菜单选 8）：
```
lte_sig_info: rssi=-74, rsrq=-4, rsrp=-98, snr=224, signal strength level is MODERATE
```
> 解读：`rssi=-74 dBm`、`rsrq=-4 dB`、`rsrp=-98 dBm`、`snr=224`（×0.1=22.4 dB，链路很好但归一化等级只给到 MODERATE，说明等级主要看 RSRP 覆盖：-98 dBm 属中等）。打印用 `%hhd`(int8_t) / `%hd`(int16_t) 精确匹配字段宽度，移植时勿用错格式符。

### 3.10 获取小区访问状态信息（第 50 页）
```c
void item_ql_nw_get_cell_access_status(void)
{
    int ret;
    QL_NW_CELL_ACCESS_STATE_TYPE_E e_state;
    ret = ql_nw_get_cell_access_status(&e_state);
    printf("ql_nw_get_cell_access_state ret = %d, e_state=%d\n", ret, e_state);
}
```

### 3.11 获取网络时间信息（第 50 页）
```c
void item_ql_nw_get_nitz_time_info(void)
{
    int ret;
    ql_nw_nitz_time_info_t t_info;
    memset(&t_info, 0, sizeof(t_info));
    ret = ql_nw_get_nitz_time_info(&t_info);
    printf("ql_nw_get_nitz_time_info ret = %d\n nitz_time=%s, abs_time=%lld, leap_sec=%hhd\n",
           ret, t_info.nitz_time, t_info.abs_time, t_info.leap_sec);
}
```
> 解读：`abs_time` 用 `%lld`(uint64_t)、`leap_sec` 用 `%hhd`(int8_t)。

### 3.12 注册语音拨号注网事件（第 50-51 页）
```c
void item_ql_nw_set_voice_reg_ind_cb(void)
{
    int ret = 0, reg_flag = 0;
    printf("please input voice reg option: (0: unreg, other: reg): ");
    ret = t_get_int(&reg_flag);
    if(ret != 0) { printf("Invalid input\n"); return; }
    if(reg_flag)
        ret = ql_nw_set_voice_reg_ind_cb(nw_voice_reg_event_ind_cb);
    else
        ret = ql_nw_set_voice_reg_ind_cb(NULL);          /* 传 NULL 即注销回调 */
    printf("ql_nw_reg_voice_reg_event ret = %d\n", ret);
}
```
> 解读：**传 `NULL` 即注销该事件回调** —— 这是所有 `*_ind_cb` 注册接口的通用约定（3.13~3.16 同此模式）。

### 3.13 注册数据拨号注网事件（第 51 页）
同 3.12，回调函数为 `nw_data_reg_event_ind_cb`，注册接口 `ql_nw_set_data_reg_ind_cb`，传 NULL 注销。

### 3.14 注册信号强度事件（第 52 页）
```c
void item_ql_nw_set_signal_strength_chg_ind_cb(void)
{
    ...
    if(reg_flag) ret = ql_nw_set_signal_strength_ind_cb(nw_signal_strength_event_ind_cb);
    else         ret = ql_nw_set_signal_strength_ind_cb(NULL);
    printf("ql_nw_reg_signal_strength_chg_event ret = %d\n", ret);
}
```

### 3.15 注册小区访问状态事件（第 52-53 页）
同上模式，回调 `nw_cell_access_status_event_ind_cb`，接口 `ql_nw_set_cell_access_status_ind_cb`。

### 3.16 注册网络时间事件（第 53 页）
同上模式，回调 `nw_nitz_time_update_event_ind_cb`，接口 `ql_nw_set_nitz_time_update_ind_cb`。

### 3.17 注册服务异常事件回调函数（第 54 页）
```c
void item_ql_nw_set_service_error_cb(void)
{
    int ret = 0;
    ret = ql_nw_set_service_error_cb(nw_service_error_cb);
    if(ret != QL_ERR_OK)
        printf("Failed to ql_nw_set_service_error_cb, ret=%d\n", ret);
    else
        printf("Sucessful\n");
}
```
> 解读：服务异常回调直接注册（无 reg/unreg 选项），区别于上面几个事件。

### 3.18 去初始化网络服务（第 54 页）
```c
void item_ql_nw_deinit(void)
{
    int ret = 0;
    printf("Start to ql_nw_deinit: ");
    ret = ql_nw_deinit();
    if(ret == QL_ERR_OK) printf("nw deinit ok\n");
    else                 printf("failed, ret=%d\n", ret);
}
```

> **示例菜单总览**（程序运行时的选项，0~15 对应上述各示例）：
> 0 network_scan / 1 set_power_mode / 2 set_pref_nwmode_roaming / 3 get_pref_nwmode_roaming / 4 get_mobile_operator_name / 5 get_cell_info / 6 get_voice_reg_status / 7 get_data_reg_status / 8 get_signal_strength / 9 get_cell_access_status / 10 get_nitz_time_info / 11 set_voice_reg_ind_cb / 12 set_data_reg_ind_cb / 13 set_signal_strength_ind_cb / 14 set_cell_access_status_ind_cb / 15 set_nitz_time_ind_cb / -1 exit。

---

## 4. 附录 — 参考文档及术语缩写（第 55-57 页）

### 表 3：参考文档
| 编号 | 文档名称 |
|---|---|
| [1] | `Quectel_AG35-CET_QuecOpen_快速开发指导` |

### 表 4：术语缩写（全表）
| 缩写 | 英文 | 中文 |
|---|---|---|
| API | Application Programming Interface | 应用编程接口 |
| ARFCN | Absolute Radio Frequency Channel Number | RF 信道号 |
| CSG | Closed Subscriber Group | 闭合用户组 |
| CDMA | Code Division Multiple Access | 码分多址 |
| DSD | Direct Stream Digital | 直接比特流数字 |
| EDGE | Enhanced Data Rate for GSM Evolution | 增强型数据速率 GSM 演进技术 |
| EHRPD | Evolved High Rate Package Data | 演进的高速分组网络 |
| EMS | Element Management System | 网元管理系统 |
| EPS | Evolved Packet System | 演进分组系统 |
| EVDO | Evolution-Data Optimized | 演进数据优化 |
| GPRS | General Packet Radio Service | 通用无线分组业务 |
| GSM | Global System for Mobile Communications | 全球移动通讯系统 |
| HDR | High Data Rate | 高速数据速率 |
| HLR | Home Location Register | 归属位置寄存器 |
| HSS | Home Subscriber Server | 归属签约用户服务器 |
| HSPA | High-Speed Packet Access | 高速分组接入 |
| ID | Identifier | 标识符 |
| IE | Information Element | 信息元素 |
| IMEI | International Mobile Equipment Identity | 国际移动设备识别码 |
| IMSI | International Mobile Subscriber Identity | 国际移动用户识别码 |
| IoT | Internet of Things | 物联网 |
| LAC | Location Area Code | 位置区码 |
| Long_eons | Long Enhanced Operator Name String | 运营商名称全称 |
| LTE | Long Time Evolution | 长期演进技术 |
| MAC | Media Access Control | 介质访问控制 |
| MCC | Mobile Country Code | 移动设备国家代码 |
| MNC | Mobile Network Code | 移动设备网络代码 |
| MSC | Mobile Switching Center | 移动交换中心 |
| NAS | Network Attached Storage | 网络附属存储（注：此处定义有误，蜂窝语境下 NAS 应为 Non-Access Stratum 非接入层） |
| NW | Network registration | 网络注册 |
| PLMN | Public Land Mobile Network | 公共陆地移动网 |
| RIL | Radio Interface Layer | 无线界面层 |
| PRL | Preferred Roaming List | 优选漫游列表 |
| PSC | Primary Scrambling Code | 主扰码 |
| RSCP | Receive Signal Channel Power | 接收信号频道功率 |
| RSRP | Reference Signal Receiving Power | 参考信号接收功率 |
| RSRQ | Reference Signal Receiving Quality | 参考信号接收质量 |
| RSSI | Received Signal Strength Indicates | 接收信号强度指示 |
| SDK | Software Development Kit | 软件开发工具包 |
| Short_eons | Short Enhanced Operator Name String | 运营商名称简称 |
| SINR | Signal to Interference plus Noise Ratio | 信干噪比 |
| SNR | Signal Noise Ratio | 信噪比 |
| TAC | Tracing Area Code | 跟踪区码 |
| TD-SCDMA | Time Division-Synchronous Code Division Multiple Access | 时分-同步码分多址 |
| UMTS | Universal Mobile Telecommunications System | 通用移动通信系统 |
| (U)SIM | (Universal) Subscriber Identity Module | （通用）用户识别模块 |
| UTC | Coordinated Universal Time | 协调世界时 |
| UTRAN | UMTS Terrestrial Radio Access Network | UMTS 陆地无线接入网 |
| VLR | Visitor Location Register | 访问位置寄存器 |
| WCDMA | Wideband Code Division Multiple Access | 宽带码分多址 |
| 3GPP | 3rd Generation Partnership Project | 第三代合作伙伴计划 |

> **注**：术语表把 NAS 解释为「Network Attached Storage（网络附属存储）」属文档笔误 —— 在 2.3.3.1 功耗模式语境（"屏蔽 NAS 和 DSD 服务的信息上报"）中，NAS 实为 3GPP 的 **Non-Access Stratum（非接入层）**，DSD 为 **Data Services Dispatcher**，而非术语表写的 Direct Stream Digital。引用时以蜂窝网络语境为准。

---

## 5. 面向 modem_mng 工程的应用要点小结

1. **整套 `ql_nw_*` 可作为 AT 命令解析的替代方案**：注网状态（`reg_state==FULL`）、信号（RSRP/RSRQ/SNR 直读）、运营商（MCC/MNC/EONS）、小区（CID/TAC/PCI）、网络时间（NITZ），都有结构化 API，比解析 AG35 的 AT 应答更稳健（规避了 EG25 那种 `Ql_SendAT` 缓冲砸栈的风险，参见 `eg25-qlsendat-rsp-buffer-overflow`）。
2. **事件驱动 vs 轮询**：注册 `data_reg_ind_cb`/`signal_strength_ind_cb` 可让掉网/信号变化即时上报，比 50ms 轮询更省、更及时；但回调跑在 SDK indication 线程，与主拨号循环共享状态时需加锁（参考 logger 的递归 mutex 做法）。
3. **低功耗陷阱**：`ql_nw_set_power_mode` 非 0 会屏蔽事件上报。若注册了回调却收不到，先确认调了 `ql_nw_set_power_mode(QL_NW_LOWER_POWER_MASK_DISABLE)`。
4. **首选制式**：AG35-CET 仅 LTE(0x10)/WCDMA(0x02) 有效，可用 `set_pref_nwmode_roaming` 锁 LTE-only 或 LTE+WCDMA；漫游字段无效。
5. **恢复策略对齐**：`deny_reason` 给出 3GPP cause，能区分「重置可救」（22 拥塞/17 网络故障）与「重置无用」（11 PLMN 拒/2/3/6 SIM 非法/13 漫游受限），正好支撑工程中"从未连接成功则门控 L1/L2/L3"的设计；`service_error_cb` 收到 `QL_ERR_SERVICE_ABORT` 应走 deinit→init 重建。
6. **NITZ 校时**：RK3576/无 RTC 设备可用 `get_nitz_time_info`/`nitz_time_update_ind_cb` 的 `abs_time` 直接 `clock_settime`，作为 NTP 之外的冷启动快速校时路径（参见 `rk3576-iomng-systime-architecture`）。
7. **单位换算坑**：LTE `snr` ×0.1 dB、WCDMA `ecio` ×(-0.5) dB；TZ 以 15 分钟为单位（东 8 区=+32）；打印用 `%hhd`/`%hd`/`%lld` 精确匹配 int8/int16/uint64。
8. **并发限制**：所有 `ql_nw_*` 不支持并发，多线程访问必须串行化。

<!-- GENERATION_COMPLETE -->
