# Quectel AG35-CET & AG35-EUT QuecOpen(SDK) eCall 开发指导 — 全文分析

> **文档全名**：AG35-CET&AG35-EUT QuecOpen(SDK) eCall 开发指导
> **适用模块**：AG35-CET、AG35-EUT（LTE Standard 模块系列）
> **版本**：V1.0.1（Preliminary / 临时文件）
> **日期**：2025-03-06
> **作者**：Lyndsay XIE
> **总页数**：47 页（PDF 内页脚标注为 46 页正文 + 封面页）
> **本分析覆盖页范围**：全文（第 1–47 页），分段读取分段写入
>
> 本分析为对原始 PDF 的逐章逐节、逐函数、逐枚举、逐示例的详尽中文解读，保留原文全部技术细节（函数原型、参数表、枚举值、AT/示例），并补充工程解读与边界条件说明。

---

## 0. 文档背景与定位（封面 / 前言 / 文档历史，第 1–3 页）

### 0.1 文档属性
- **状态：临时文件（Preliminary）**。原文明确警示：本文档涉及的模块引脚/接口定义、频段、功能、特性及设计等发生变动的可能性较大，部分参数仅经初步验证或尚未验证。临时版本仅供临时参考，若与正式版本存在差异，以最新版本为准。
- 版权所有 © 上海移远通信技术股份有限公司 2025。文档受保密与版权约束（许可协议、版权声明、商标、第三方权利、隐私声明、免责声明俱全，属 Quectel 标准法律页，无技术内容）。

### 0.2 修订记录（关键——反映 V1.0.1 相对 V1.0.0 的实质变更）
| 版本 | 日期 | 作者 | 变更要点 |
|---|---|---|---|
| - | 2023-09-27 | Lyndsay XIE | 文档创建 |
| 1.0.0 | 2023-09-27 | Lyndsay XIE | 临时版本 |
| 1.0.1 | 2025-03-06 | Lyndsay XIE | 见下方 10 项变更 |

**V1.0.1 的 10 项变更（逐条，对工程对齐很重要）：**
1. 新增适用模块 **AG35-EUT**（此前仅 AG35-CET）。
2. 更新 eCall 重拨机制为**欧洲标准**（第 2.4 章）。
3. 更新 eCall 状态事件列表（第 2.6 章）。
4. **移除结构体 `msd_t`**（第 3.3.4 章）。
5. 更新结构体 `ql_ecall_config_t`（第 3.3.11.1 章）。
6. 更新结构体 `msd_Vin_t`（第 3.3.16.2 章）。
7. 更新车辆类型（第 3.3.25、3.3.26 章）。
8. 新增函数返回值（第 3.3.27~3.3.31 章）。
9. 更新推荐的函数使用流程（第 3.4.1 章）。
10. 更新设置回调函数上报 eCall 状态的示例（第 3.4.2.2 章）。

> **解读**：第 4 项「移除 `msd_t`」+ 第 6 项「更新 `msd_Vin_t`」说明早期版本曾用一个完整 `msd_t` 大结构一次性填 MSD，新版改为以一组 `ql_ecall_set_msd_*` 离散 setter + `msd_Vin_t`（仅 VIN 车辆识别码）来组装 MSD。移植/对接旧代码时需注意这一 API 形态变化。

---

## 1. 引言（第 7 页）

- AG35-CET 与 AG35-EUT 支持 **QuecOpen®** 方案：基于 **Linux** 的嵌入式开发平台，用于简化 IoV（车联网）应用的软件设计开发。
- 本文档适用于 **SDK 构建环境**的 QuecOpen 方案。两模块在 **GSM 或 UMTS 网络**下可实现 eCall 功能（注意：eCall 语音承载走 2G/3G CS 域，不是 LTE VoLTE）。
- 文档主旨：介绍 eCall 工作流程、配置参数，以及如何通过 API 函数实现 eCall 功能。

---

## 2. eCall 综述（第 8–11 页）

### 2.1 eCall 工作流程

#### 2.1.1 流程概述
- **eCall 是一项欧洲倡议**，目标：为欧盟境内任何地方发生碰撞的驾车者提供快速援助。计划在所有车辆部署一台设备，发生严重交通事故时**自动拨打国际求救电话 112**，通过无线通信发送乘车人/车辆信息及 **GNSS 定位坐标**到当地应急机构；并支持手动拨打求救电话。
- 事故发生时：车辆 eCall 系统经卫星定位获取车辆位置，自动/手动建立紧急语音呼叫，并由 **IVS**（含 AG35 模块的车载设备）把含位置、乘客数量、车辆识别码等的 **MSD（Minimum Set of Data，最小数据集）**通过移动蜂窝网络发送至当地公共安全应急机构（如 **PSAP**），短时间内完成事故信息采集，开展紧急救援。

- **两大组成实体：**
  - **IVS（In-Vehicle System，车载系统）**：发起/接听 eCall 并发送 MSD；车祸时自动拨 112，同时通过语音通道把 MSD 发给 PSAP。MSD 内容可含：位置信息、时间、乘客数量、车牌号及其他紧急救援所需信息。
  - **PSAP（Public Safety Answering Point，公共安全应答中心）**：接听/回拨 eCall，请求 MSD 数据，发起公共救援。

- **备注（重要测试约束）：**
  1. 本文档中 AG35-CET/AG35-EUT QuecOpen 模块作为 **IVS** 使用。
  2. **中国目前还未建立标准 PSAP**，无法在实网下测试；但可用实验室标准测试仪器 **CMW500** 及其配套软件模拟 PSAP。本文档即用 CMW500 模拟 PSAP 测试。

#### 2.1.2 流程示意图（图 1）
图 1 描绘完整链路：①车辆发生事故 → ②呼叫 112（语音，红色虚线）→ ③经基站 → ④传输车辆位置信息和最小数据集（数据，黄色实线）→ ⑤到 **1st Level PSAP（一级 PSAP）**。同时卫星向车辆提供 GNSS 定位。图例：Data=数据通道，Voice=语音通道。即 eCall 同一呼叫内既走语音又走带内（in-band）数据（MSD 通过语音通道调制传输）。

### 2.2 eCall 触发模式
- eCall 支持**手动**和**自动**两种触发方式，通过 `ql_ecall_dial()` 函数配置（见 `ql_voice_ecall_info_t` 的 `auto_trigger` 参数）。

### 2.3 MO eCall 与 MT eCall
- **MO eCall（Mobile Originated，主叫）**：IVS 主动发起 eCall 通话；PSAP 接听后，IVS 通过 **PUSH 模式**自动发送 MSD，整个流程无需 PSAP 发送请求。
- **MT eCall（Mobile Terminated，被叫）**：指前一次 eCall 通话由 PSAP 主动挂断，此时只要 **T2 定时器还没到期**，IVS 就会自动接听 PSAP 主动发起的 eCall 通话；且本次 eCall 使用 **PULL 模式**传输 MSD，即需要 PSAP 主动发送 MSD 请求。

> **解读**：PUSH 与 PULL 是 MSD 传输的两种触发方向——MO 时 IVS 主动推（PUSH），MT 回拨时由 PSAP 拉（PULL）。T2 定时器（默认 3600 秒）是「eCall 会话保持期」，期内 IVS 保持可被回拨接听状态。

### 2.4 eCall 重拨机制（V1.0.1 更新为欧洲标准）
eCall 拨打失败或通话异常断开后，IVS 按如下机制自动重拨：
- **欧洲标准**：间隔 **60 秒**重拨一次，共重拨 **10 次**；通话中异常断连会**立即重拨**，共 **2 次**。
- **GLONASS 标准**：间隔 **30 秒**重拨一次，共重拨 **10 次**。
- **备注**：用户可按需修改重拨次数和时间。

### 2.5 eCall 标准定时器（表 1，遵循《BS EN 16454-2015》）
| 定时器 | 功能说明 | 默认值 |
|---|---|---|
| **T2** | IVS 的 eCall 通话超时时间（通话持续时间超过此定时器时间时，IVS 将主动挂断通话） | **3600 秒** |
| **T3** | IVS 初始化信号持续时间 | **2 秒** |
| **T5** | IVS 等待发送 MSD 的时间 | **5 秒** |
| **T6** | IVS 等待接收 AL-ACK 的时间 | **5 秒** |
| **T7** | IVS 传输 MSD 的最大时间 | **20 秒** |
| **T9** | IVS 在模式下最小的驻网持续时间 | **3600 秒** |
| **T10** | IVS 在 eCall only 模式下最大的驻网持续时间 | **12 小时** |

> **解读**：T5/T6/T7 与下文状态事件 `*_T5/T6/T7_TIMEOUT` 一一对应，是 MSD 带内传输各阶段的看门狗。T9/T10 约束 eCall-only 模式下的驻网时长。

### 2.6 eCall 状态事件（表 2）
eCall 状态事件指从 eCall 发起到结束期间，模块上报到 AP（应用处理器）的 eCall 状态信息。**定义在头文件 `/ql-sysroots/usr/include/ql-sdk/ql_ecall.h` 中。**

| eCall 状态事件 | 说明 |
|---|---|
| `QL_ECALL_EVENT_T2_TIMEOUT` | T2 超时 |
| `QL_ECALL_EVENT_T5_TIMEOUT` | T5 超时 |
| `QL_ECALL_EVENT_T6_TIMEOUT` | T6 超时 |
| `QL_ECALL_EVENT_T7_TIMEOUT` | T7 超时 |
| `QL_ECALL_EVENT_SENDING_START` | IVS 开始发送 MSD |
| `QL_ECALL_EVENT_SENDING_MSD` | IVS 正在发送 MSD |
| `QL_ECALL_EVENT_LLACK_RECEIVED` | IVS 收到 LL-ACK 消息 |
| `QL_ECALL_EVENT_ALLACK_POSITIVE_RECEIVED` | IVS 收到 AL-ACK 消息 |
| `QL_ECALL_EVENT_ALLACK_CLEARDOWN_RECEIVED` | IVS 收到 PSAP CLEARDOWN 消息 |
| `QL_ECALL_EVENT_MSD_TRANS_STATUS_SUCCESS` | MSD 发送成功 |
| `QL_ECALL_EVENT_MSD_TRANS_STATUS_FAILURE` | MSD 发送失败 |
| `QL_ECALL_EVENT_ECALL_STARTED` | 发起 eCall |
| `QL_ECALL_EVENT_MSDUPDATE_PSAP_REQURE_UPDATE_MSD` | PSAP 请求更新 MSD |
| `QL_ECALL_EVENT_MSDUPDATE_PSAP_REQURE_TIMEOUT` | MSD 更新超时 |
| `QL_ECALL_EVENT_ACTIVE` | eCall 通话连接成功 |
| `QL_ECALL_EVENT_DISCONNECTED` | eCall 通话正常断开连接 |
| `QL_ECALL_EVENT_ABNORMAL_HANGUP` | IVS 异常挂断 |
| `QL_ECALL_EVENT_PSAP_CALLBACK_START` | 开启 PSAP 回拨自动接听 |
| `QL_ECALL_EVENT_INCOMING_CALL` | IVS 接收到 PSAP 回拨 |
| `QL_ECALL_EVENT_DIAL_DURATION_TIMEOUT` | eCall 重拨超时 |
| `QL_ECALL_EVENT_INTERVAL_TIMEOUT` | IVS 重拨间隔时间超时 |
| `QL_ECALL_EVENT_AUTO_ANSWER_TIMEOUT` | 自动接听超时 |

> **解读（带内传输握手序列）**：典型成功流程的事件序列为 `ECALL_STARTED` → `ACTIVE`（PSAP 接听）→ `SENDING_START` → `SENDING_MSD` → `LLACK_RECEIVED`（链路层确认）→ `ALLACK_POSITIVE_RECEIVED`（应用层确认）→ `MSD_TRANS_STATUS_SUCCESS`。`ALLACK_CLEARDOWN_RECEIVED` 表示 PSAP 通知清场结束。`PSAP_CALLBACK_START`/`INCOMING_CALL` 对应 MT eCall 回拨场景。回调函数中 `ind` 参数即取这些枚举值。

---

## 3. eCall API（第 12–19 页，本段覆盖到 3.3.8）

### 3.1 头文件
- eCall API 头文件为 **`ql_ecall.h`**，位于 SDK 包的 **`ql-sysroots/usr/include/ql-sdk/`** 路径下。若无特别说明，本文档所涉头文件均在该目录下。

### 3.2 函数概览（表 3，全部 31 个 API）
| 函数 | 说明 |
|---|---|
| `ql_ecall_init()` | 初始化 eCall |
| `ql_ecall_deinit()` | 注销 eCall |
| `ql_ecall_set_user_ind_cb()` | 注册 eCall 状态上报回调函数 |
| `ql_ecall_dial()` | 发起 eCall 通话 |
| `ql_ecall_update_msd_raw()` | 使用原始数据更新 MSD |
| `ql_ecall_update_msd()` | 更新 MSD |
| `ql_ecall_hangup()` | 挂断 eCall 通话 |
| `ql_ecall_start_test()` | 发起 eCall 测试 |
| `ql_ecall_start_manual()` | 发起手动拨打 eCall |
| `ql_ecall_start_automatic()` | 发起自动拨打 eCall |
| `ql_ecall_set_config_info()` | 配置 eCall |
| `ql_ecall_get_config_info()` | 获取 eCall 配置 |
| `ql_ecall_set_test_number()` | 设置 eCall 测试号码 |
| `ql_ecall_set_system_std()` | 设置 eCall 系统标准类型 |
| `ql_ecall_get_system_std()` | 获取 eCall 系统标准类型 |
| `ql_ecall_set_msd_vin()` | 设置车辆识别码 |
| `ql_ecall_get_msd_vin()` | 获取车辆识别码 |
| `ql_ecall_set_msd_version()` | 设置 MSD 版本号 |
| `ql_ecall_get_msd_version()` | 获取 MSD 版本号 |
| `ql_ecall_set_msd_tx_mode()` | 设置 MSD 发送模式 |
| `ql_ecall_get_msd_tx_mode()` | 获取 MSD 发送模式 |
| `ql_ecall_set_msd_position()` | 设置车辆位置信息 |
| `ql_ecall_set_msd_position_n1()` | 设置车辆经纬度的增量 N1 |
| `ql_ecall_set_msd_position2_n2()` | 设置车辆经纬度的增量 N2 |
| `ql_ecall_set_msd_vehicle_type()` | 设置车辆类型 |
| `ql_ecall_get_msd_vehicle_type()` | 获取车辆类型 |
| `ql_ecall_set_msd_passengers_count()` | 设置乘客数量 |
| `ql_ecall_set_msd_propulsion_type()` | 设置车辆推动类型 |
| `ql_ecall_get_msd_propulsion_type()` | 获取车辆推动类型 |
| `ql_ecall_set_msd_call_type()` | 设置 eCall 类型 |
| `ql_ecall_get_msd_call_type()` | 获取 eCall 类型 |

> **备注（关键限制）**：若无特别说明，**所有 eCall API 均不支持并发调用，并且不能在相关回调函数中调用以上 API**。即回调线程内禁止反向调 API（典型如在 `user_ind_cb` 里调 `ql_ecall_hangup` 是不允许的），需通过事件投递到主线程处理。

### 3.3 函数详解

#### 3.3.1 `ql_ecall_init` — 初始化 eCall
```c
int ql_ecall_init(void);
```
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功；其他值失败（错误码见 `ql_type.h`）。
- **备注**：使用其他 eCall API 前，**必须先调用本函数初始化 eCall 服务**。

#### 3.3.2 `ql_ecall_deinit` — 注销 eCall
```c
int ql_ecall_deinit(void);
```
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 3.3.3 `ql_ecall_set_user_ind_cb` — 注册 eCall 状态上报回调
```c
void ql_ecall_set_user_ind_cb(ql_ecall_user_ind_f cb, void *userdata);
```
- **参数：**
  - `cb` [In]：eCall 状态回调函数，详见 3.3.3.1。
  - `userdata` [In]：用户数据，通常为 `NULL`。
- **返回值**：无（void）。

##### 3.3.3.1 `ql_ecall_user_ind_f` cb — 回调函数原型
```c
typedef void (*ql_ecall_user_ind_f)(int ind, void *userdata);
```
- **参数：**
  - `ind` [In]：eCall 状态，取值见表 2（2.6 节状态事件枚举）。
  - `userdata` [In]：用户数据。
- **返回值**：无。
- 该回调用于处理 eCall 状态变化。

#### 3.3.4 `ql_ecall_dial` — 发起 eCall 通话
```c
int ql_ecall_dial(int sim_id, const ql_voice_ecall_info_t* const p_info, uint32_t* const p_id);
```
- **参数：**
  - `sim_id` [In]：(U)SIM 卡 ID。`0` = 无效卡槽；`1` = (U)SIM1；`2` = (U)SIM2。
  - `p_info` [In]：eCall 输入信息，详见 3.3.4.1。**暂不支持 `msd` 参数输入。**
  - `p_id` [Out]：返回的 eCall ID。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

##### 3.3.4.1 `ql_voice_ecall_info_t` — eCall 输入信息结构体
```c
typedef struct
{
    uint32_t              msd_len;
    uint8_t               msd[QL_VOICE_MAX_ECALL_MSD];
    QL_VOICE_ECALL_TYPE_E type;
    int                   auto_trigger;
    char                  test_number[QL_VOICE_MAX_PHONE_NUMBER];
} ql_voice_ecall_info_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `uint32_t` | `msd_len` | MSD 数据长度 |
| `uint8_t` | `msd[QL_VOICE_MAX_ECALL_MSD]` | MSD 数据 |
| `QL_VOICE_ECALL_TYPE_E` | `type` | eCall 类型；见 3.3.4.2 |
| `int` | `auto_trigger` | eCall 触发方式：`0`=手动，`1`=自动 |
| `char` | `test_number[QL_VOICE_MAX_PHONE_NUMBER]` | 测试号码 |

> **解读**：虽然结构体里有 `msd`/`msd_len` 字段，但 `ql_ecall_dial` 文档明确「暂不支持 msd 参数输入」——MSD 需用专门的 `ql_ecall_update_msd_raw` / 一组 `set_msd_*` setter 配置。`test_number` 仅在 `type=TEST` 时配合 PSAP 模拟号使用。

##### 3.3.4.2 `QL_VOICE_ECALL_TYPE_E` — eCall 类型枚举
```c
typedef enum
{
    QL_VOICE_ECALL_TYPE_TEST      = 1,
    QL_VOICE_ECALL_TYPE_EMERGENCY = 2,
    QL_VOICE_ECALL_TYPE_RECONFIG  = 3,
} QL_VOICE_ECALL_TYPE_E;
```
| 成员 | 描述 |
|---|---|
| `QL_VOICE_ECALL_TYPE_TEST` | 测试模式（可指定 PSAP 号码进行拨打） |
| `QL_VOICE_ECALL_TYPE_EMERGENCY` | 正常模式（默认拨打紧急救助电话 112） |
| `QL_VOICE_ECALL_TYPE_RECONFIG` | 重新配置模式 |

##### 3.3.4.3 `ECALL_SYSTEM_STD_E` — eCall 系统标准类型枚举
```c
typedef enum
{
    ECALL_SYSTEM_STD_PAN_EUROPEAN,
    ECALL_SYSTEM_STD_ERA_GLONASS
} ECALL_SYSTEM_STD_E;
```
| 成员 | 描述 |
|---|---|
| `ECALL_SYSTEM_STD_PAN_EUROPEAN` | 欧洲标准 |
| `ECALL_SYSTEM_STD_ERA_GLONASS` | GLONASS 标准（**暂不支持**） |

> **解读**：枚举未显式赋值，故 `PAN_EUROPEAN=0`、`ERA_GLONASS=1`。当前仅支持欧洲标准（Pan-European eCall），俄标 ERA-GLONASS 暂不支持。这与 2.4 节重拨机制对欧洲/GLONASS 两套参数的描述一致——参数虽列出但 GLONASS 路径未启用。

#### 3.3.5 `ql_ecall_update_msd_raw` — 使用原始数据更新 MSD
```c
int ql_ecall_update_msd_raw(uint8_t* msd, uint32_t msd_len);
```
- **参数：**
  - `msd` [In]：待更新的 MSD。
  - `msd_len` [In]：MSD 长度。**单位：hex，最大长度 140 hex。**
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

> **解读**：MSD 标准最大长度即 140 字节（hex 字节）。该接口用于已有完整 MSD 字节流（例如外部按 EN 15722 ASN.1 编码好）时直接灌入，绕过离散 setter。

#### 3.3.6 `ql_ecall_update_msd` — 更新 MSD（配合 setter 使用）
```c
int ql_ecall_update_msd(void);
```
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功；其他值失败。
- 该函数用于更新 MSD，**需要配套其他 MSD 参数设置接口使用**（即先用 `set_msd_*` 系列设置各字段，再调用本函数提交生效）。

#### 3.3.7 `ql_ecall_hangup` — 挂断 eCall 通话
```c
int ql_ecall_hangup(void);
```
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 3.3.8 `ql_ecall_start_test` — 发起 eCall 测试
```c
int ql_ecall_start_test(int sim_id);
```
- **参数：**
  - `sim_id` [In]：(U)SIM 卡 ID。`0`=无效卡槽；`1`=(U)SIM1；`2`=(U)SIM2。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。
- **备注（重要前置条件）**：调用该函数之前，需调用函数完成**全部 MSD 信息配置，其中车辆识别码（VIN）必须设置**。

#### 3.3.9 `ql_ecall_start_manual` — 发起手动拨打 eCall
```c
int ql_ecall_start_manual(int sim_id);
```
- **参数**：`sim_id` [In] (U)SIM 卡 ID（`0`=无效卡槽；`1`=(U)SIM1；`2`=(U)SIM2）。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。
- **备注**：调用前需完成全部 MSD 信息配置，其中**车辆识别码必须设置**。

#### 3.3.10 `ql_ecall_start_automatic` — 发起自动拨打 eCall
```c
int ql_ecall_start_automatic(int sim_id);
```
- **参数**：`sim_id` [In] (U)SIM 卡 ID（`0`=无效卡槽；`1`=(U)SIM1；`2`=(U)SIM2）。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。
- **备注**：调用前需完成全部 MSD 信息配置，其中**车辆识别码必须设置**。

> **解读（manual vs automatic）**：手动 eCall 通常由车内乘员按 SOS 键触发，自动 eCall 由碰撞传感器触发。二者在 MSD 中体现为 `call_type`（见 `set_msd_call_type`）及触发标志位不同，PSAP 据此区分优先级；两者均要求 VIN 已设置。

#### 3.3.11 `ql_ecall_set_config_info` — 配置 eCall
```c
void ql_ecall_set_config_info(ql_ecall_config_t ecall_context_info);
```
- **参数**：`ecall_context_info` [In] eCall 配置信息，详见 3.3.11.1。
- **返回值**：无（void）。注意是**值传递整个结构体**。

##### 3.3.11.1 `ql_ecall_config_t` — eCall 配置信息结构体（V1.0.1 更新）
```c
typedef struct
{
    uint8_t   ecallonly_valid;
    uint8_t   ecallonly;
    uint8_t   cleardownTimer_timeout_ms_valid;
    uint32_t  cleardownTimer_timeout_ms;
    uint8_t   t5_timeout_ms_valid;
    uint16_t  t5_timeout_ms;
    uint8_t   t6_timeout_ms_valid;
    uint16_t  t6_timeout_ms;
    uint8_t   t7_timeout_ms_valid;
    uint16_t  t7_timeout_ms;
    uint8_t   autoAnswer_timeout_ms_valid;
    uint32_t  autoAnswer_timeout_ms;
    uint8_t   deregister_network_timeout_ms_valid;  // T10
    uint32_t  deregister_network_timeout_ms;
    uint8_t   dialDurationTimer_timout_ms_valid;
    uint32_t  dialDurationTimer_timout_ms;
    uint8_t   maxDropDialAttempts_valid;
    uint32_t  maxDropDialAttempts;
    uint8_t   maxDialAttempts_valid;
    uint32_t  maxDialAttempts;
    uint8_t   intervalBetweenAttempts_valid;
    uint16_t  intervalBetweenAttempts;
    uint8_t   resetEcallSessionMode_valid;
    uint8_t   resetEcallSessionMode;
} ql_ecall_config_t;
```

**参数表（每个字段都遵循「`*_valid` 使能开关 + 实际值」配对模式：valid=1 使能设置/1 该项生效，valid=0 关闭设置/不修改）：**
| 类型 | 参数 | 描述 |
|---|---|---|
| `uint8_t` | `ecallonly_valid` | 设置 eCall only 模式使能状态：1=使能，0=关闭 |
| `uint8_t` | `ecallonly` | eCall only 模式（开/关本身） |
| `uint8_t` | `cleardownTimer_timeout_ms_valid` | cleardown 定时器使能（原文参数表未单列描述，结构体中存在） |
| `uint32_t` | `cleardownTimer_timeout_ms` | cleardown 定时器时间（ms） |
| `uint8_t` | `t5_timeout_ms_valid` | 设置 T5 定时器使能状态：1=使能，0=关闭 |
| `uint16_t` | `t5_timeout_ms` | T5 定时器时间 |
| `uint8_t` | `t6_timeout_ms_valid` | 设置 T6 定时器使能状态：1=使能设置，0=关闭设置 |
| `uint16_t` | `t6_timeout_ms` | T6 定时器时间 |
| `uint8_t` | `t7_timeout_ms_valid` | 设置 T7 定时器使能状态：1=使能设置，0=关闭设置 |
| `uint16_t` | `t7_timeout_ms` | T7 定时器时间 |
| `uint8_t` | `autoAnswer_timeout_ms_valid`（原文写 `antoAnswer_timeout_ms_valid`） | 设置自动接听超时时间使能状态：1=使能设置，0=关闭设置 |
| `uint32_t` | `autoAnswer_timeout_ms`（原文写 `antoAnswer_timeout_ms`） | 自动接听超时时间 |
| `uint8_t` | `deregister_network_timeout_ms_valid` | 设置 **T10** 定时器使能状态：1=使能设置，0=关闭设置 |
| `uint32_t` | `deregister_network_timeout_ms` | **T10** 定时器时间 |
| `uint8_t` | `dialDurationTimer_timout_ms_valid` | 设置重拨总时间定时器使能状态：1=使能设置，0=关闭设置 |
| `uint32_t` | `dialDurationTimer_timout_ms` | 重拨总时间 |
| `uint8_t` | `maxDropDialAttempts_valid` | 设置通话异常挂断重拨次数使能状态：1=使能设置，0=关闭设置 |
| `uint32_t` | `maxDropDialAttempts` | 通话异常挂断重拨次数（对应 2.4 节欧标"连续立即重拨 2 次"） |
| `uint8_t` | `maxDialAttempts_valid` | 设置最大拨号失败重拨次数使能状态：1=使能设置，0=关闭设置 |
| `uint32_t` | `maxDialAttempts` | 最大拨号失败重拨次数（对应 2.4 节"共重拨 10 次"） |
| `uint8_t` | `intervalBetweenAttempts_valid` | 设置拨号失败重拨间隔时间使能状态：1=使能设置，0=关闭设置 |
| `uint16_t` | `intervalBetweenAttempts` | 拨号失败重拨间隔时间（对应 2.4 节"间隔 60 秒/30 秒"） |
| `uint8_t` | `resetEcallSessionMode_valid` | 设置自动恢复功能使能状态：1=使能设置，0=关闭设置 |
| `uint8_t` | `resetEcallSessionMode` | 自动恢复功能（开/关本身） |

> **解读**：该结构体把 2.4 节重拨机制与 2.5 节标准定时器全部参数化，是用户「按需修改重拨次数和时间」的落地接口。`*_valid` 配对设计允许只覆写部分字段而保留其余默认值。`ecallonly` 对应车规「eCall only 注册」模式（SIM 仅用于 eCall，平时不驻网）。注意原文 `autoAnswer` 字段在结构体定义里拼作 `autoAnswer`，参数表里却印作 `antoAnswer`（疑似笔误），实际以头文件为准。

#### 3.3.12 `ql_ecall_get_config_info` — 获取 eCall 配置
```c
void ql_ecall_get_config_info(ql_ecall_config_t* ecall_context_info);
```
- **参数**：`ecall_context_info` [Out] eCall 配置信息，详见 3.3.11.1。
- **返回值**：无（void）。

#### 3.3.13 `ql_ecall_set_test_number` — 设置 eCall 测试号码
```c
int ql_ecall_set_test_number(int sim_id, char* number);
```
- **参数：**
  - `sim_id` [In] (U)SIM 卡 ID（`0`=无效卡槽；`1`=(U)SIM1；`2`=(U)SIM2）。
  - `number` [In] 测试号码。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。
- **解读**：测试模式（`TEST` 类型）下，eCall 不拨 112 而拨此处设置的 PSAP 模拟号码（如连 CMW500 时的被叫号）。

#### 3.3.14 `ql_ecall_set_system_std` — 设置 eCall 系统标准类型
```c
int ql_ecall_set_system_std(ECALL_SYSTEM_STD_E system_std);
```
- **参数**：`system_std` [In] eCall 系统标准类型，详见 3.3.4.3（`PAN_EUROPEAN` / `ERA_GLONASS`，后者暂不支持）。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 3.3.15 `ql_ecall_get_system_std` — 获取 eCall 系统标准类型
```c
int ql_ecall_get_system_std(ECALL_SYSTEM_STD_E* system_std);
```
- **参数**：`system_std` [Out] eCall 系统标准类型，详见 3.3.4.3。
- **返回值**：`QL_ERR_OK` 成功；其他值失败。

#### 3.3.16 `ql_ecall_set_msd_vin` — 设置车辆识别码（VIN）
```c
QL_ERR_MSD_RESULT_E ql_ecall_set_msd_vin(msd_Vin_t vin);
```
- **参数**：`vin` [In] 车辆识别码，详见 3.3.16.2（**值传递结构体**）。
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败（错误码见 3.3.16.1）。
- **注意**：从本函数起，MSD 相关 setter/getter 改用专用返回类型 `QL_ERR_MSD_RESULT_E`（而非通用 `int`/`QL_ERR_OK`）。

##### 3.3.16.1 `QL_ERR_MSD_RESULT_E` — MSD 发送结果枚举
```c
typedef enum
{
    QL_ERR_MSD_NONE              = 0,
    QL_ERR_MSD_SUCCESS           = QL_ERR_MSD_NONE,            // = 0
    QL_ERR_MSD_GENERIC           = -(QL_ERR_MSD_NONE+1),       // = -1
    QL_ERR_MSD_BADPARM           = -(QL_ERR_MSD_NONE+2),       // = -2
    QL_ERR_MSD_VERSION           = -(QL_ERR_MSD_NONE+3),       // = -3
    QL_ERR_MSD_ISOWMI            = -(QL_ERR_MSD_NONE+2),       // 原文如此（= -2，与 BADPARM 重号）
    QL_ERR_MSD_ISOVDS            = -(QL_ERR_MSD_NONE+4),       // = -4
    QL_ERR_MSD_ISOVIS_MODELYEAR  = -(QL_ERR_MSD_NONE+5),       // = -5
    QL_ERR_MSD_ISOVIS_SEQPLAN    = -(QL_ERR_MSD_NONE+6),       // = -6
    QL_ERR_MSD_LATITUDE          = -(QL_ERR_MSD_NONE+7),       // = -7
    QL_ERR_MSD_LONGITUDE         = -(QL_ERR_MSD_NONE+8),       // = -8
    QL_ERR_MSD_VEH_DIRECTION     = -(QL_ERR_MSD_NONE+9),       // = -9
    QL_ERR_MSD_VEH_LAT_LOCATION1 = -(QL_ERR_MSD_NONE+10),      // = -10
    QL_ERR_MSD_VEH_LON_LOCATION1 = -(QL_ERR_MSD_NONE+11),      // = -11
    QL_ERR_MSD_VEH_LAT_LOCATION2 = -(QL_ERR_MSD_NONE+12),      // = -12
    QL_ERR_MSD_VEH_LON_LOCATION2 = -(QL_ERR_MSD_NONE+13),      // = -13
    QL_ERR_MSD_VEH_IDENTIFI_NUMBER     = -(QL_ERR_MSD_NONE+14),// = -14
    QL_ERR_MSD_OPTINAL_DATA_GLONASS    = -(QL_ERR_MSD_NONE+15),// = -15
} QL_ERR_MSD_RESULT_E;
```
| 成员 | 描述 |
|---|---|
| `QL_ERR_MSD_SUCCESS` | 操作成功 |
| `QL_ERR_MSD_GENERIC` | MSD 类错误 |
| `QL_ERR_MSD_BADPARM` | MSD 参数错误 |
| `QL_ERR_MSD_VERSION` | MSD 版本错误 |
| `QL_ERR_MSD_ISOWMI` | WMI 识别码错误 |
| `QL_ERR_MSD_ISOVDS` | VDS 错误 |
| `QL_ERR_MSD_ISOVIS_MODELYEAR` | 车型年份错误 |
| `QL_ERR_MSD_ISOVIS_SEQPLAN` | 装配厂和出厂顺序号错误 |
| `QL_ERR_MSD_LATITUDE` | 纬度错误 |
| `QL_ERR_MSD_LONGITUDE` | 经度错误 |
| `QL_ERR_MSD_VEH_DIRECTION` | 车辆方位错误 |
| `QL_ERR_MSD_VEH_LAT_LOCATION1` | 经度增量 N1 位置错误 |
| `QL_ERR_MSD_VEH_LON_LOCATION1` | 纬度增量 N1 位置错误 |
| `QL_ERR_MSD_VEH_LAT_LOCATION2` | 经度增量 N2 位置错误 |
| `QL_ERR_MSD_VEH_LON_LOCATION2` | 纬度增量 N2 位置错误 |
| `QL_ERR_MSD_VEH_IDENTIFI_NUMBER` | 车辆识别码错误 |
| `QL_ERR_MSD_OPTINAL_DATA_GLONASS` | GLONASS 可选数据错误 |

> **注意（原文枚举瑕疵）**：`QL_ERR_MSD_ISOWMI` 被赋值为 `-(QL_ERR_MSD_NONE+2)`，与 `QL_ERR_MSD_BADPARM` 的 `-2` **数值重复**。这是原文档明确印刷的内容，使用时应以宏名判断而非裸数值，避免歧义。每个错误码精确对应一个 MSD 字段设置失败，便于定位是哪一项参数非法。

##### 3.3.16.2 `msd_Vin_t` — 车辆识别码结构体（V1.0.1 更新，紧凑打包）
```c
typedef struct __attribute__((__packed__)) {
    char isowmi[4];
    char isovds[7];
    char isovisModelyear[2];
    char isovisSeqPlant[8];
} msd_Vin_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `char` | `isowmi` | 世界制造商识别代码（WMI） |
| `char` | `isovds` | 车辆说明部分（VDS） |
| `char` | `isovisModelyear` | 车型年份 |
| `char` | `isovisSeqPlant` | 装配厂和出厂顺序号 |

> **解读**：`__packed__` 确保按 ISO 3779 VIN 字段紧凑布局，无填充字节。注意各数组**不含 NUL 终止符的额外空间**（isowmi 正好 4 字节 = WMI 3 位 + 1？实际 WMI 标准为 3 字符，此处 4 字节需以头文件实际长度为准）。标准 VIN 共 17 位 = WMI(3) + VDS(6) + VIS(8)，此处字段长度（4+7+2+8）总和与编码方式相关，对接时务必按 SDK 头文件实际长度填充，不要自行补 `\0`。错误填充会返回对应的 `QL_ERR_MSD_ISO*` 错误码。

#### 3.3.17 `ql_ecall_get_msd_vin` — 获取车辆识别码
```c
QL_ERR_MSD_RESULT_E ql_ecall_get_msd_vin(msd_Vin_t* vin);
```
- **参数**：`vin` [Out] 车辆识别码，详见 3.3.16.2。
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败（见 3.3.16.1）。

#### 3.3.18 `ql_ecall_set_msd_version` — 设置 MSD 版本号
```c
QL_ERR_MSD_RESULT_E ql_ecall_set_msd_version(uint8_t msd_version);
```
- **参数**：`msd_version` [In] MSD 版本号。
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败。
- **解读**：MSD 版本号对应 EN 15722 标准的格式版本（常见值为 1 或 2）。

#### 3.3.19 `ql_ecall_get_msd_version` — 获取 MSD 版本号
```c
QL_ERR_MSD_RESULT_E ql_ecall_get_msd_version(uint8_t* msdVersionPtr);
```
- **参数**：`msdVersionPtr` [Out] MSD 版本号。
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败。

#### 3.3.20 `ql_ecall_set_msd_tx_mode` — 设置 MSD 发送模式（仅 GLONASS 标准）
```c
QL_ERR_MSD_RESULT_E ql_ecall_set_msd_tx_mode(QL_ECALL_MSD_TX_MODE_E tx_mode);
```
- **参数**：`tx_mode` [In] MSD 发送模式，详见 3.3.20.1。
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败。
- **注意**：**仅适用于 GLONASS 标准**。欧标下 MSD 传输模式由 MO/MT 自动决定（MO=PUSH、MT=PULL），无需手动设置。

##### 3.3.20.1 `QL_ECALL_MSD_TX_MODE_E` — MSD 发送模式枚举
```c
typedef enum
{
    QL_ECALL_TX_MODE_PULL = 0,
    QL_ECALL_TX_MODE_PUSH = 1
} QL_ECALL_MSD_TX_MODE_E;
```
| 成员 | 描述 |
|---|---|
| `QL_ECALL_TX_MODE_PULL` | PULL 模式（PSAP 主动请求 MSD） |
| `QL_ECALL_TX_MODE_PUSH` | PUSH 模式（IVS 主动推送 MSD） |

#### 3.3.21 `ql_ecall_get_msd_tx_mode` — 获取 MSD 发送模式
```c
void ql_ecall_get_msd_tx_mode(QL_ECALL_MSD_TX_MODE_E* tx_mode);
```
- **参数**：`tx_mode` [Out] MSD 发送模式，详见 3.3.20.1。
- **返回值**：无（void）。

#### 3.3.22 `ql_ecall_set_msd_position` — 设置车辆位置信息
```c
QL_ERR_MSD_RESULT_E ql_ecall_set_msd_position(bool isTrusted, int32_t latitude, int32_t longitude, int32_t direction);
```
- **参数：**
  - `isTrusted` [In] 位置是否可信：`1`=可信，`0`=不可信。
  - `latitude` [In] 纬度。
  - `longitude` [In] 经度。
  - `direction` [In] 车辆行驶方向。**取值范围 0~179，单位：2 度**（即实际方向 = 取值 × 2 度，覆盖 0~358°）。
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败。
- **解读**：`isTrusted` 标记 GNSS 定位置信度（如卫星定位有效/无效），PSAP 据此判断坐标可靠性。纬度/经度为 MSD 主坐标（通常以毫角秒为单位的整数编码）。

#### 3.3.23 `ql_ecall_set_msd_position_n1` — 设置车辆经纬度增量 N1
```c
QL_ERR_MSD_RESULT_E ql_ecall_set_msd_position_n1(int32_t latitudeDeltaN1, int32_t longitudeDeltaN1);
```
- **参数：**
  - `latitudeDeltaN1` [In] 纬度增量 N1。**取值范围 -512~511。**
  - `longitudeDeltaN1` [In] 经度增量 N1。**取值范围 -512~511。**
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败。

#### 3.3.24 `ql_ecall_set_msd_position_n2` — 设置车辆经纬度增量 N2
```c
QL_ERR_MSD_RESULT_E ql_ecall_set_msd_position_n2(int32_t latitudeDeltaN2, int32_t longitudeDeltaN2);
```
- **参数：**
  - `latitudeDeltaN2` [In] 纬度增量 N2。**取值范围 -512~511。**
  - `longitudeDeltaN2` [In] 经度增量 N2。**取值范围 -512~511。**
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败。

> **解读（N1/N2 增量坐标）**：EN 15722 MSD 中除主坐标外，还携带两个最近的历史位置点（recentVehicleLocationN1/N2），以相对主坐标的增量表示，用于 PSAP 推断车辆行驶轨迹/朝向。增量范围 ±511 限制了相对偏移幅度（约 ±511 个最小角度单位）。

#### 3.3.25 `ql_ecall_set_msd_vehicle_type` — 设置车辆类型（V1.0.1 更新）
```c
QL_ERR_MSD_RESULT_E ql_ecall_set_msd_vehicle_type(uint8_t vehicleType);
```
- **参数**：`vehicleType` [In] 车辆类型，取值如下表（共 23 种，对应欧盟车辆分类）：

| 值 | 车辆类型 | 值 | 车辆类型 |
|---|---|---|---|
| 1 | M1 级轿车 | 13 | L7e 摩托车 |
| 2 | M2 级客车与旅游客车 | 14 | O 拖车 |
| 3 | M3 级客车与旅游客车 | 15 | R 农用车辆 |
| 4 | N1 级轻型商用车 | 16 | S 农用车辆 |
| 5 | N2 级重型汽车 | 17 | T 农用车辆 |
| 6 | N3 级重型汽车 | 18 | G 越野车 |
| 7 | L1e 摩托车 | 19 | SA 房车 |
| 8 | L2e 摩托车 | 20 | SB 装甲车 |
| 9 | L3e 摩托车 | 21 | SC 救护车 |
| 10 | L4e 摩托车 | 22 | SD 灵车 |
| 11 | L5e 摩托车 | 23 | 其他 |
| 12 | L6e 摩托车 | | |
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败。

#### 3.3.26 `ql_ecall_get_msd_vehicle_type` — 获取车辆类型
```c
QL_ERR_MSD_RESULT_E ql_ecall_get_msd_vehicle_type(uint8_t* vehicleType);
```
- **参数**：`vehicleType` [Out] 车辆类型，取值同 3.3.25 的 1~23 表。
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败。

#### 3.3.27 `ql_ecall_set_msd_passengers_count` — 设置乘客数量（V1.0.1 新增返回值）
```c
void ql_ecall_set_msd_passengers_count(uint8_t numberOfPassengers);
```
- **参数**：`numberOfPassengers` [In] 乘客数量。
- **返回值**：原型为 `void`，但文档返回值小节标注 `QL_ERR_MSD_SUCCESS` 成功 / 其他值失败（见 3.3.16.1）。

> **注意（原型与返回值描述不一致）**：原文函数原型印为 `void` 返回，而「返回值」小节又描述 `QL_ERR_MSD_SUCCESS`/其他值——这是 V1.0.1「新增函数返回值」（变更第 8 项）过程中原型未同步更新的遗留矛盾。实际以 SDK 头文件签名为准。`set_msd_call_type` 也存在同样情况。

#### 3.3.28 `ql_ecall_set_msd_propulsion_type` — 设置车辆推动类型（位掩码）
```c
void ql_ecall_set_msd_propulsion_type(uint8_t PropulsionType);
```
- **参数**：`PropulsionType` [In] 车辆推动类型（**位掩码，可按位或组合多种能源**）：

| 值 | 推动类型 |
|---|---|
| `0x1` | 汽油 |
| `0x2` | 柴油 |
| `0x4` | 天然气 |
| `0x8` | 丙烷气 |
| `0x10` | 电力 |
| `0x20` | 氢气 |
| `0x40` | 其他 |
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败。
- **解读**：位掩码设计支持混合动力（如汽油+电力 = `0x1|0x10 = 0x11`）。该信息对救援很关键——电动车/氢能车事故有触电/爆燃风险，PSAP 据此调度专业处置。

#### 3.3.29 `ql_ecall_get_msd_propulsion_type` — 获取车辆推动类型
```c
void ql_ecall_get_msd_propulsion_type(uint8_t* PropulsionType);
```
- **参数**：`PropulsionType` [Out] 车辆推动类型，取值同 3.3.28 位掩码表。
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败。

#### 3.3.30 `ql_ecall_set_msd_call_type` — 设置 eCall 类型
```c
void ql_ecall_set_msd_call_type(bool testCall);
```
- **参数**：`testCall` [In] eCall 类型：`1`=测试模式，`0`=正常模式。
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败。
- **解读**：此处写入 MSD 内的「测试呼叫标志位」（testCall bit），区别于 `ql_ecall_dial` 的 `type` 字段——前者是 MSD 数据内容，后者是拨号行为类型。测试时两者应一致设置。

#### 3.3.31 `ql_ecall_get_msd_call_type` — 获取 eCall 类型
```c
void ql_ecall_get_msd_call_type(bool* testCall);
```
- **参数**：`testCall` [Out] eCall 类型：`1`=测试模式，`0`=正常模式。
- **返回值**：`QL_ERR_MSD_SUCCESS` 成功；其他值失败。

### 3.4 函数使用

#### 3.4.1 推荐的函数使用流程（V1.0.1 更新，三种方式）

**方式一：使用移远通信 MSD 编码接口（推荐，用离散 setter 组装 MSD）**
- **步骤 1**：调用 `ql_ecall_init()` 初始化 eCall。
- **步骤 2**：调用 `ql_ecall_set_user_ind_cb()` 注册上报 eCall 状态的回调函数，以便接收 IVS 上报的 eCall 状态信息。
- **步骤 3**：依次调用以下接口设置 MSD 参数：
  1. `ql_ecall_set_msd_version()`
  2. `ql_ecall_set_msd_call_type()`
  3. `ql_ecall_set_msd_vehicle_type()`
  4. `ql_ecall_set_msd_vin()`
  5. `ql_ecall_set_msd_propulsion_type()`
  6. `ql_ecall_set_msd_passengers_count()`
  7. `ql_ecall_set_msd_position()`
  8. `ql_ecall_set_msd_position_n1()`
  9. `ql_ecall_set_msd_position_n2()`
- **步骤 4**：使用 `ql_ecall_update_msd()` 接口生成 MSD 后，可调用 `ql_ecall_start_test()`、`ql_ecall_start_manual()`、`ql_ecall_start_automatic()` 接口拨打不同类型的 eCall；测试类型需要拨打前用 `ql_ecall_set_test_number()` 接口设置测试号码。
- **步骤 5**：在特定场景下（当 eCall 为手动触发，且没有与 PSAP 连接成功且不在发送 MSD 且不在语音通话时），IVS 可以调用 `ql_ecall_hangup()` 挂断 eCall 通话；一般情况为 PSAP 端挂断 eCall。

**方式二：用户自行完成 MSD 编码并调 `start_test/start_manual/start_automatic` 拨号**
- **步骤 1**：`ql_ecall_init()` 初始化。
- **步骤 2**：`ql_ecall_set_user_ind_cb()` 注册回调。
- **步骤 3**：调用 `ql_ecall_update_msd_raw()` 完成 MSD 的更新（自行编码好的原始字节）。
- **步骤 4**：MSD 更新完成后，可调用 `ql_ecall_start_test()`、`ql_ecall_start_manual()`、`ql_ecall_start_automatic()` 拨打不同类型 eCall；测试类型需拨打前用 `ql_ecall_set_test_number()` 设置测试号码。
- **步骤 5**：同方式一步骤 5（特定场景下 IVS 可 `ql_ecall_hangup()`）。

**方式三：用户自行完成 MSD 编码并调 `ql_ecall_dial()` 更新 MSD 和拨号**
- **步骤 1**：`ql_ecall_init()` 初始化。
- **步骤 2**：`ql_ecall_set_user_ind_cb()` 注册回调。
- **步骤 3**：调用 `ql_ecall_dial()`，在 `p_info` 参数中完成更新 MSD、拨号类型设置。示例：
  ```c
  ql_ecall_dial(sim_id, p_info, &id);
  ```
- **步骤 4**：在后续 PSAP 需要请求更新时，使用 `ql_ecall_update_msd_raw()` 更新 MSD。
- **步骤 5**：同前（特定场景下 IVS 可 `ql_ecall_hangup()`）。

> **解读**：三种方式区别在于 MSD 来源——方式一用 SDK 离散 setter（最易用，SDK 负责 ASN.1 编码），方式二/三用户自行编码原始 MSD（灵活但需懂 EN 15722）。方式三把 MSD 更新与拨号合并在一次 `dial` 调用里。注意：尽管前述 `ql_ecall_dial` 文档说「暂不支持 msd 参数输入」，方式三却用 `p_info` 传 MSD——以 SDK 实际实现/最新头文件为准（属临时文档的不一致点之一）。

#### 3.4.2 函数使用示例

##### 3.4.2.1 初始化 eCall
```c
void item_ql_ecall_init(void)
{
    int ret = 0;

    printf("test ql_ecall_init: ");
    ret = ql_ecall_init();
    if(ret == QL_ERR_OK)
    {
        printf("ok\n");
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```

##### 3.4.2.2 设置回调函数上报 eCall 状态（V1.0.1 更新）
```c
static void ecall_user_ind_callback(int ind, void *userdata)
{
    printf("\n****** eCall indication Received ******\n");
    printf("ecall_indication: %d - ", ind);

    switch(ind){
    case QL_ECALL_EVENT_SENDING_START:
        printf("QL_ECALL_EVENT_SENDING_START\n");
        break;
    case QL_ECALL_EVENT_SENDING_MSD:
        printf("QL_ECALL_EVENT_SENDING_MSD\n");
        break;
    case QL_ECALL_EVENT_LLACK_RECEIVED:
        printf("QL_ECALL_EVENT_LLACK_RECEIVED\n");
        break;
    case QL_ECALL_EVENT_ALLACK_POSITIVE_RECEIVED:
        printf("QL_ECALL_EVENT_ALLACK_POSITIVE_RECEIVED\n");
        break;
    case QL_ECALL_EVENT_ALLACK_CLEARDOWN_RECEIVED:
        printf("QL_ECALL_EVENT_ALLACK_CLEARDOWN_RECEIVED\n");
        break;
    case QL_ECALL_EVENT_MSDUPDATE_PSAP_REQURE_UPDATE_MSD:
        printf("QL_ECALL_EVENT_MSDUPDATE_PSAP_REQURE_UPDATE_MSD\n");
        break;
    case QL_ECALL_EVENT_MSDUPDATE_PSAP_REQURE_TIMEOUT:
        printf("QL_ECALL_EVENT_MSDUPDATE_PSAP_REQURE_TIMEOUT\n");
        break;
    case QL_ECALL_EVENT_ACTIVE:
        printf("QL_ECALL_EVENT_ACTIVE\n");
        break;
    case QL_ECALL_EVENT_DISCONNECTED:
        printf("QL_ECALL_EVENT_DISCONNECTED\n");
        break;
    case QL_ECALL_EVENT_ABNORMAL_HANGUP:
        printf("QL_ECALL_EVENT_ABNORMAL_HANGUP\n");
        break;
    case QL_ECALL_EVENT_ONLY_DEREGISTRATION:
        printf("QL_ECALL_EVENT_ONLY_DEREGISTRATION\n");
        break;
    case QL_ECALL_EVENT_MAY_DEREGISTRATION:
        printf("QL_ECALL_EVENT_MAY_DEREGISTRATION\n");
        break;
    case QL_ECALL_EVENT_PSAP_CALLBACK_START:
        printf("QL_ECALL_EVENT_MAY_DEREGISTRATION\n");   // 原文如此（打印串疑似复制粘贴笔误）
        break;
    case QL_ECALL_EVENT_T2_TIMEOUT:
        printf("QL_ECALL_EVENT_T2_TIMEOUT\n");
        break;
    case QL_ECALL_EVENT_T5_TIMEOUT:
        printf("QL_ECALL_EVENT_T5_TIMEOUT\n");
        break;
    case QL_ECALL_EVENT_T6_TIMEOUT:
        printf("QL_ECALL_EVENT_T6_TIMEOUT\n");
        break;
    case QL_ECALL_EVENT_T7_TIMEOUT:
        printf("QL_ECALL_EVENT_T7_TIMEOUT\n");
        break;
    case QL_ECALL_EVENT_ECALL_STARTED:
        printf("QL_ECALL_EVENT_ECALL_STARTED\n");
        break;
    case QL_ECALL_EVENT_INCOMING_CALL:
        printf("QL_ECALL_EVENT_INCOMING_CALL\n");
        break;
    case QL_ECALL_EVENT_DIAL_DURATION_TIMEOUT:
        printf("QL_ECALL_EVENT_DIAL_DURATION_TIMEOUT\n");
        break;
    case QL_ECALL_EVENT_ORI_REDIAL_TIME:
        p_ind_data = (ql_ecall_ind_event_data *)ind_data;
        printf("QL_ECALL_EVENT_ORI_REDIAL_TIME : %d\n", p_ind_data->ori_redial_time);
        break;
    case QL_ECALL_EVENT_DROP_REDIAL_TIME:
        p_ind_data = (ql_ecall_ind_event_data *)ind_data;
        printf("QL_ECALL_EVENT_DROP_REDIAL_TIME : %d\n", p_ind_data->drop_redial_time);
        break;
    case QL_ECALL_EVENT_INTERVAL_TIMEOUT:
        printf("QL_ECALL_EVENT_INTERVAL_TIMEOUT\n");
        break;
    case QL_ECALL_EVENT_AUTO_ANSWER_TIMEOUT:
        printf("QL_ECALL_EVENT_AUTO_ANSWER_TIMEOUT\n");
        break;
    ...
    }
}
```

> **解读（示例揭示的额外事件）**：示例代码中出现了表 2 未列出的事件，说明头文件实际枚举更全：
> - `QL_ECALL_EVENT_ONLY_DEREGISTRATION` / `QL_ECALL_EVENT_MAY_DEREGISTRATION`：eCall-only 模式下的强制/可选去注册事件（与 T9/T10 驻网定时器相关）。
> - `QL_ECALL_EVENT_ORI_REDIAL_TIME` / `QL_ECALL_EVENT_DROP_REDIAL_TIME`：携带数据的事件，需把 `ind_data` 强转为 `ql_ecall_ind_event_data*` 后取 `ori_redial_time`（原始重拨剩余次数/时间）、`drop_redial_time`（异常挂断重拨剩余）。
>
> 注意 `case QL_ECALL_EVENT_PSAP_CALLBACK_START` 分支里 printf 误打成 `MAY_DEREGISTRATION`——原文档示例的复制粘贴笔误，移植时应修正。另外该回调原型 `(int ind, void *userdata)` 与示例中使用的 `ind_data` 变量名不一致，实际需以 SDK 回调签名（含携带数据的 data 指针）为准。

示例 switch 末尾的 `default` 分支打印 `UNKNOWN`，并给出回调注册函数：
```c
    default:
        printf("UNKNOWN\n");
        break;
    }
}

void item_ql_ecall_set_user_ind_cb(void)
{
    ql_ecall_set_user_ind_cb(ecall_user_ind_callback, NULL);
}
```

##### 3.4.2.3 拨打 eCall（方式三：`ql_ecall_dial` 一次性更新 MSD + 拨号，第 39–40 页）
```c
void fast_ecall_dial(void){
    int ret = 0, v = 0;
    ql_voice_ecall_info_t *p_info = NULL;
    char *find = NULL;
    uint32_t id;
    int sim_id;

    p_info = (ql_voice_ecall_info_t *)calloc(1, sizeof(*p_info));
    if (NULL == p_info)
    {
        printf("run out of memory\n");
        return;
    }

    printf("please enter the sim_id: ");
    scanf("%d", &sim_id);
    getchar();

    if(!QL_IS_SIM_VALID(sim_id))
    {
        printf("invalid sim_id\n");
        free(p_info);
        return;
    }

    printf("sim_id is %d\n", sim_id);

    printf("example MSD: 01 04 a9 81 d5 49 70 d6 5c 35 97 ca 04 20 c4 14 60 "
        "0b be 5f 7e b1 4b a6 ee 10 4f c5 27 03 c1 80 q\n");
    printf("please enter MSD(at most 140 hex), end with 'q': ");
    while (1 == scanf("%x", &v))
    {
        p_info->msd[p_info->msd_len++] = v;
    }
    getchar();  // read `q'
    getchar();  // read '\n'

    printf("MSD =======[");
    for(v = 0; v < p_info->msd_len; v ++)
    {
        printf("%02x ", p_info->msd[v]);
    }
    printf("]\n");

    if (p_info->msd_len > QL_VOICE_MAX_ECALL_MSD)
    {
        printf("MSD too long\n");
        free(p_info);
        return;
    }

    printf("please enter eCall type(1 - test, 2 - emergency,  3 - reconfig): ");
    scanf("%d", (int *)&p_info->type);
    getchar();

    printf("please enter test number(emergency ecall should be empty): ");
    find = NULL;
    fgets(p_info->test_number, sizeof(p_info->test_number)-1, stdin);
    find = strchr(p_info->test_number, '\n');
    if(find)
    {
        *find = '\0';
    }

    printf("how to trigger eCall(0 - manual, 1 - auto): ");
    scanf("%d", &p_info->auto_trigger);
    getchar();

    ret = ql_ecall_dial(sim_id, p_info, &id);
    if (ret == QL_ERR_OK)
    {
        printf("ok, call_id is %u\n", id);
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }

    free(p_info);
}

void item_ql_ecall_dial(void)
{
    printf("test ql_voice_ecall_dial: \n");
    printf("Dialling fast ecall\n");
    fast_ecall_dial();
}
```
> **解读**：示例完整演示了 `ql_voice_ecall_info_t` 的填充——交互式从 stdin 读 hex MSD（以 `'q'` 结束）、eCall 类型、测试号（emergency 应留空）、触发方式（0 手动/1 自动），再调 `ql_ecall_dial`。`QL_IS_SIM_VALID(sim_id)` 是 SDK 提供的卡槽校验宏。注意此处确实通过 `p_info->msd` 传入了 MSD，与 3.3.4 节"暂不支持 msd 参数输入"的说明相矛盾——印证前述"以实际 SDK 实现为准"的提醒。示例 MSD 长度正好 32 字节，远小于 140 上限。

##### 3.4.2.4 使用原始数据更新 MSD（第 40–41 页）
```c
void item_ql_ecall_update_msd_raw(void)
{
    int ret = 0, v = 0;
    uint32_t msd_len = 0;
    char msd[QL_VOICE_MAX_ECALL_MSD] = {0};

    printf("test ql_voice_ecall_update_msd: \n");
    printf("example MSD: 01 04 a9 81 d5 49 70 d6 5c 35 97 ca 04 20 c4 14 60 "
        "0b be 5f 7e b1 4b a6 ee 10 4f c5 27 03 c1 80 q\n");
    printf("please enter MSD(at most 140 hex), end with 'q': ");
    while (1 == scanf("%x", &v))
    {
        if(msd_len >= QL_VOICE_MAX_ECALL_MSD)
        {
            printf("MSD too long\n");
            int c;
            while ((c = getchar()) != '\n' && c != EOF) { }
            return;
        }
        else
        {
            msd[msd_len++] = v;
        }
    }
    getchar();  // read `q'
    getchar();  // read '\n'

    ret = ql_ecall_update_msd_raw((uint8_t *)msd, msd_len);
    if (ret == QL_ERR_OK)
    {
        printf("ok\n");
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```
> **解读**：相比 3.4.2.3，这里在读取循环内部就做了 `msd_len >= QL_VOICE_MAX_ECALL_MSD` 的越界保护（超长时清空输入缓冲再返回），是更稳健的写法。对应方式二的步骤 3。

##### 3.4.2.5 更新 MSD（配合离散 setter 的方式一，第 41 页）
```c
void item_ql_ecall_update_msd (void)
{
    int ret = 0;
    ret = ql_ecall_update_msd();
    if (ret == QL_ERR_OK)
    {
        printf("ok\n");
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```

##### 3.4.2.6 挂断 eCall（第 42 页）
```c
void item_ql_ecall_hangup(void)
{
    int ret = 0;
    int sim_id;

    printf("test ql_voice_ecall_hangup: \n");
    ret = ql_ecall_hangup();
    if (ret == QL_ERR_OK)
    {
        printf("ok\n");
    }
    else
    {
        printf("failed, ret = %d\n", ret);
    }
}
```

##### 3.4.2.7 设置车辆识别码（VIN 解析示例，第 42–43 页）
```c
void item_ql_ecall_set_msd_vin(void)
{
    int ret = 0;
    uint8_t index = 0;
    msd_Vin_t vin;
    char* find = NULL;
    char vin_str[QL_ECALL_MAX_VIN];

    printf("Input: default(0) other(1)\n");
    scanf("%hhd", &index);
    getchar();
    if(index)
    {
        printf("Please insert VIN:\n");
        fgets(vin_str, QL_ECALL_MAX_VIN-1, stdin);
        find = strchr(vin_str, '\n');
        if(find)
        {
            *find = '\0';
        }
        // example 1: WM9VDSVDSYA123456
        // example 2: 4Y1SL65848Z411439
        // example 3: VF37BRFVE12345678
        memcpy(&vin.isowmi,            vin_str,      3);
        memcpy(&vin.isovds,            (vin_str+3),  6);
        memcpy(&vin.isovisModelyear,   (vin_str+9),  1);
        memcpy(&vin.isovisSeqPlant,    (vin_str+10), 7);
    }
    else
    {
        memcpy(&vin.isowmi,          "WM9",      3);
        memcpy(&vin.isovds,          "VDSVDS",   6);
        memcpy(&vin.isovisModelyear, "Y",        1);
        memcpy(&vin.isovisSeqPlant,  "A123456",  7);
    }

    ret = ql_ecall_set_msd_vin(vin);
    printf(" ql_ecall_set_msd_vin ret = %d\n", ret);
}
```
> **关键解读（VIN 字段实际拷贝长度）**：示例揭示了 17 位 VIN 字符串到 `msd_Vin_t` 的真实切分方式，**与结构体声明的数组长度不同**：
> - `isowmi` ← VIN[0..2]，拷贝 **3** 字节（声明 `char[4]`）— WMI 世界制造商代码 3 位。
> - `isovds` ← VIN[3..8]，拷贝 **6** 字节（声明 `char[7]`）— VDS 车辆说明 6 位。
> - `isovisModelyear` ← VIN[9]，拷贝 **1** 字节（声明 `char[2]`）— 车型年份 1 位。
> - `isovisSeqPlant` ← VIN[10..16]，拷贝 **7** 字节（声明 `char[8]`）— 装配厂+顺序号 7 位。
>
> 合计 3+6+1+7 = **17 位**，正好是标准 VIN 长度。每个数组都比实际拷贝多 1 字节（留 NUL 余量），但示例只用 `memcpy` 填有效位、不补 `\0`。`QL_ECALL_MAX_VIN` 是 VIN 字符串缓冲宏。提供 `default(0)` 内置示例 VIN 与 `other(1)` 手动输入两条路径，便于快速测试。

##### 3.4.2.8 发起 eCall 测试（第 43 页）
```c
void item_ql_ecall_start_test(void)
{
    int ret = 0;
    int sim_id;

    printf("please enter the sim_id: ");
    ret = scanf("%d", &sim_id);
    getchar();

    if(!QL_IS_SIM_VALID(sim_id))
    {
        printf("invalid sim_id\n");
    }

    ret = ql_ecall_start_test(sim_id);
    printf(" ql_ecall_start_test ret = %d\n", ret);
}
```
> **注意**：示例在 `sim_id` 非法时仅打印 `invalid sim_id` 但**未 return**，仍继续调用 `ql_ecall_start_test`——这是原文示例的健壮性缺陷，移植时应在判错后 `return`。3.4.2.9/3.4.2.10 同样存在此问题。

##### 3.4.2.9 发起手动拨打 eCall（第 43 页）
```c
void item_ql_ecall_start_manual(void)
{
    int ret = 0;
    int sim_id;

    printf("please enter the sim_id: ");
    ret = scanf("%d", &sim_id);
    getchar();

    if(!QL_IS_SIM_VALID(sim_id))
    {
        printf("invalid sim_id\n");
    }

    ret = ql_ecall_start_manual(sim_id);
    printf(" ql_ecall_start_manual ret = %d\n", ret);
}
```

##### 3.4.2.10 发起自动拨打 eCall（第 44 页）
```c
void item_ql_ecall_start_automatic(void)
{
    int ret = 0;
    int sim_id;

    printf("please enter the sim_id: ");
    ret = scanf("%d", &sim_id);
    getchar();

    if(!QL_IS_SIM_VALID(sim_id))
    {
        printf("invalid sim_id\n");
    }

    ret = ql_ecall_start_automatic(sim_id);
    printf(" ql_ecall_start_automatic ret = %d\n", ret);
}
```
> **解读（三个 start 函数结构一致）**：`start_test`/`start_manual`/`start_automatic` 示例结构完全相同，仅最终调用的 API 不同。前置条件均为：已 `init`、已 `set_user_ind_cb`、已完成 MSD 配置（VIN 必填）。test 型还需先 `set_test_number`。

---

## 4. 附录：参考文档及术语缩写（第 45–46 页）

### 4.1 参考文档（表 4）
| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |
| [2] | BS EN 16454-2015（eCall 标准定时器依据，见 2.5 节） |

### 4.2 术语缩写（表 5）
| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| ACK | Acknowledgment | 确认消息 |
| AL-ACK | Application Layer Acknowledgement | 应用层确认消息 |
| API | Application Programming Interface | 应用程序编程接口 |
| eCall | emergency Call | 紧急电话 |
| GLONASS | Global Navigation Satellite System (Russia) | 格洛纳斯导航卫星系统（俄罗斯） |
| GNSS | Global Navigation Satellite System | 全球导航卫星系统 |
| GSM | Global System for Mobile Communication | 全球移动通信系统 |
| HLACK | High Layer Acknowledgement | 高级应用层确认消息 |
| IoV | Internet of Vehicles | 车联网 |
| IVS | In-vehicle System | 车载系统 |
| LLACK | Link Layer Acknowledgement | 链路层确认消息 |
| MO | Mobile Originated | 终端发起 |
| MSD | Minimum Set of Data | 最小数据集 |
| MT | Mobile Terminated | 终端终止 |
| NACK | Negative Acknowledgement | 否定消息 |
| PSAP | Public Safety Answering Point | 公共安全应答中心 |
| SDK | Software Development Kit | 软件开发工具包 |
| UMTS | Universal Mobile Telecommunications System | 通用移动通信系统 |
| (U)SIM | (Universal) Subscriber Identity Module | (通用)用户身份识别模块 |
| VDS | Vehicle-Descriptive-Section | 车辆说明部分 |
| VIN | Vehicle Identification Number | 车辆识别码 |
| WMI | World Manufacturer Identifier | 世界制造厂识别代号 |

---

## 5. 全文要点总结与工程对接提示

### 5.1 核心结论
1. **承载网络**：eCall 语音+带内 MSD 走 **GSM/UMTS**（2G/3G CS 域），不是 LTE。AG35-CET/EUT 作 **IVS**，对端为 **PSAP**（中国无实网 PSAP，需 CMW500 模拟）。
2. **唯一支持标准**：当前仅 `ECALL_SYSTEM_STD_PAN_EUROPEAN`（欧洲标准）。`ERA_GLONASS` 及 `ql_ecall_set_msd_tx_mode`（PULL/PUSH 手动设置）暂不支持。
3. **MSD 三条组装路径**：①离散 setter + `update_msd`；②`update_msd_raw` 原始字节；③`ql_ecall_dial` 的 `p_info` 携带。MSD 最大 **140 hex 字节**。
4. **拨号入口**：`start_test`/`start_manual`/`start_automatic`/`dial`，均需先 `init`+`set_user_ind_cb`+MSD 配置（**VIN 必填**）。
5. **重拨（欧标）**：失败间隔 60s 重拨 10 次；通话中异常断连立即连拨 2 次。GLONASS 为 30s × 10 次。可通过 `ql_ecall_config_t` 改。
6. **定时器**：T2=3600s（会话超时）、T3=2s、T5=5s、T6=5s、T7=20s、T9=3600s、T10=12h，均可经 `ql_ecall_config_t` 调整。

### 5.2 文档内部需注意的不一致/瑕疵（移植时以 SDK 头文件为准）
- `ql_ecall_dial` 说"暂不支持 msd 参数输入"，但 3.4.1 方式三与 3.4.2.3 示例均经 `p_info->msd` 传入 MSD。
- `QL_ERR_MSD_ISOWMI` 与 `QL_ERR_MSD_BADPARM` 枚举值同为 `-2`（重号）。
- `set_msd_passengers_count`/`set_msd_call_type`/`set_msd_propulsion_type` 等原型印为 `void`，返回值小节却描述 `QL_ERR_MSD_SUCCESS`（V1.0.1 新增返回值未同步原型）。
- 回调示例中 `PSAP_CALLBACK_START` 分支误打印 `MAY_DEREGISTRATION`；回调原型 `(int ind, void*)` 与示例用的携带数据指针 `ind_data`/`ql_ecall_ind_event_data` 不符。
- `ql_ecall_config_t` 中 `autoAnswer_*` 字段在参数表里印作 `antoAnswer_*`（笔误）。
- 三个 `start_*` 示例在 `sim_id` 非法时未 `return` 即继续调用 API（健壮性缺陷）。

### 5.3 与本仓库（modem_mng）的关联
- 本仓库 modem_mng 的 EC200A/EG25 平台主要面向 **4G 数据拨号**，与本 eCall 文档（GSM/UMTS 语音+带内 MSD）属不同业务域。AG35 平台若需 eCall 功能，需引入本文档 `ql_ecall.h` 系列 API（头文件路径 `ql-sysroots/usr/include/ql-sdk/`），且 eCall API **不可并发、不可在回调内反调**——这与现有拨号守护进程的回调线程模型需隔离设计（事件投递到主线程处理），与 CLAUDE.md 中记述的 EC200A 回调遮蔽/nanomsg 裸奔教训一脉相承，务必加 try/catch 与线程边界保护。

<!-- GENERATION_COMPLETE -->
