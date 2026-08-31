# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) 数据拨号开发指导 — 全文分析

> **文档全名**：AG35-CET&AG35-EUT QuecOpen(SDK) 数据拨号开发指导
> **适用模块**：AG35-CET、AG35-EUT（LTE Standard 模块系列）
> **版本**：V1.0.1 Preliminary（临时文件）
> **日期**：2024-12-06　**作者**：Keroro TAN
> **总页数**：44 页（PDF），正文页脚标注 1/43 ~ 43/43
> **本分析覆盖范围**：全部 44 页（封面 + 前言 + 文档历史 + 目录/索引 + 第1~4章 + 附录）

---

## 文档元信息与修订历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| -（创建） | 2023-06-19 | Keroro TAN | 文档创建 |
| 1.0.0 | 2023-06-19 | Keroro TAN | 临时版本 |
| 1.0.1 | 2024-12-06 | Keroro TAN | 临时版本：①基于 QuecOpen 方案统一命名，更新文档名称；②新增适用模块 AG35-EUT；③新增 API `ql_data_call_get_pkt_stats()`（第 3.3.33 章） |

**解读**：本文档为 **Preliminary（临时）** 状态，原文明确声明引脚/接口定义、频段、功能、特性及设计可能发生较大变动，部分参数仅经初步验证或未经验证，临时版本与正式版本如有差异应以最新版为准。从 1.0.0 到 1.0.1 的核心技术增量是新增了**数据包统计 API** `ql_data_call_get_pkt_stats()`，并把适用范围从单 CET 扩展到 CET + EUT 两个型号。

### 文档结构总览（基于目录页）

- **第 1 章 引言**（p.8）
- **第 2 章 数据拨号**（p.9）：2.1 拨号流程 / 2.2 重连机制 / 2.3 拨号状态 / 2.4 异常处理
- **第 3 章 数据拨号 API**（p.13）：3.1 头文件 / 3.2 函数概览 / 3.3 函数详解（34 个 API + 多个数据结构/枚举）/ 3.4 错误码及恢复措施 / 3.5 函数使用示例 / 3.6 注意事项
- **第 4 章 附录 参考文档及术语缩写**（p.43）

**表格索引**：表1 函数概览（p.13）、表2 错误码及恢复措施（p.39）、表3 参考文档（p.43）、表4 术语和缩写（p.43）
**图片索引**：图1 拨号示意图（p.9）、图2 数据拨号状态转换（p.11）

---

## 第 1 章　引言（p.8）

- AG35-CET / AG35-EUT 模块支持 **QuecOpen®** 方案：基于 **Linux** 的嵌入式开发平台，用于简化 **IoV（车联网）** 应用的软件设计与开发，详细信息见参考文档 [1]。
- **数据拨号（Data Call）定义**：无线数据业务的建立过程。设备携带 **APN 信息**向运营商网络侧发起数据拨号，运营商网络侧根据 APN 信息建立数据链路到相应网络。
- 本文档适用于 **SDK 构建环境**的 QuecOpen 方案，主要介绍数据拨号过程以及 SDK 中用于拨号的 API。

---

## 第 2 章　数据拨号

### 2.1 拨号流程（p.9，含图1 拨号示意图）

- **APN 来源**：运营商在签约时为 (U)SIM 卡分配一个或多个 APN 信息，常见的是 **一路公网 APN + 一路或多路私网 APN**。模块携带 APN 信息发起数据拨号，运营商网络侧根据 APN 分配 **IP 地址**及网络。
- **图1（拨号示意图）解读**：图中展示三朵 ISP 网络云（ISP网络1/2/3），模块（天线）通过三组 **APN 信息**分别建立三路**数据拨号**（数据拨号1/2/3），每组 APN 信息包含五个字段：
  - APN 名称
  - IP 版本
  - 认证类型
  - 用户名
  - 密码
  - 即一路数据拨号 ↔ 一路 APN ↔ 一个 ISP 网络，多路拨号并行对应多个网络。

- **拨号前提**：数据拨号成功的前提是**模块已注册到运营商网络**。
  - LTE 网络注册过程中需建立**默认承载（default bearer）**。建立默认承载同样需要传入 APN 信息；若传入的 APN 信息错误，可能导致网络注册失败。
  - 运营商分配的 APN 信息中，**有一路 APN 可用来建立默认承载**，详情需咨询运营商。
- **注册与拨号的关系**：模块在发起数据拨号时**不会判断是否已注册网络**，所以在**网络未注册时发起数据拨号会失败，这是正常现象**。应先参考文档 [2] 完成网络注册，再进行数据拨号。

- **APN 槽位机制（重要约束）**：
  - 模块首次检测到 (U)SIM 卡时，会根据 (U)SIM 卡的 **PLMN 信息**默认加载一组常用 APN 信息，可通过 **`AT+CGDCONT?`**（遵循 3GPP 协议规范）查看。
  - 模块**最多支持 4 路 APN**，其中 **APN1 用于建立默认承载**；
  - **数据拨号推荐使用 APN2~APN4**，每路数据拨号对应一路 APN。

- **推荐拨号流程（三步）**：
  1. **步骤 1**：设置默认承载 APN 信息（APN1）；
  2. **步骤 2**：设置拨号使用的 APN 信息，配置 APN 名称、IP 类型等；
  3. **步骤 3**：使用步骤 2 配置的 APN，进行数据拨号。

- **备注**：有关 `AT+CGDCONT?` 命令详情，需联系移远通信技术支持（文档未展开）。

### 2.2 重连机制（p.10）

拨号失败或拨号断开后，可通过 **`ql_data_call_param_set_reconnect_mode()`** 设置自动重连模式，并配合 **`ql_data_call_param_set_reconnect_interval()`** 设置重试间隔时间（参考第 3.3.18 章和第 3.3.20 章）。四种模式如下：

| 模式 | 行为 | 关键说明 |
|---|---|---|
| **DISABLE** | 不自动重连 | 可按需进行手动重新拨号 |
| **NORMAL** | 按设置的固定时间间隔重连 | **建议间隔 ≥ 20 秒**，频繁拨号可能被运营商加入黑名单 |
| **MODE1** | 时间退避（backoff）模式 | 间隔序列 T1、T2、T3……Tn，重连间隔为 T1、T2、T3……Tn、Tn、Tn……Tn（**到达 Tn 后保持 Tn**）直至拨号成功；若需再次重连，则间隔时间从 T1 重新开始 |
| **MODE2** | 循环时间退避模式 | 间隔序列 T1、T2、T3……Tn，重连间隔为 T1、T2、T3…Tn、T1、T2、T3…Tn、T1、T2、T3…Tn…（**循环回到 T1**）直至拨号成功；若需再次重连，则间隔时间从 T1 重新开始 |

**解读**：
- MODE1 与 MODE2 的差异在于**触顶后的行为**——MODE1 触顶 Tn 后**恒定保持 Tn**（指数退避后稳定），MODE2 触顶后**循环回 T1 重新退避**（锯齿状循环）。
- 二者均在拨号成功后复位，再次需要重连时都从 T1 起算。
- NORMAL 的 20 秒下限和"黑名单"提示是重要的工程约束：自动重连间隔不可设置过小。

### 2.3 拨号状态（p.11，含图2 状态转换）

- 数据拨号过程包含多种状态。其中 **CONNECTED** 为拨号成功状态，此状态下可获取 **IP 地址**等信息；**DISCONNECTED** 为拨号失败或断开状态，此状态下可获取**拨号错误码**。拨号错误码定义在头文件 **`ql-sysroots/usr/include/ql-sdk/ql_net_common.h`** 下。

- **七种状态定义**：

| 状态 | 含义 |
|---|---|
| **NONE** | 数据拨号实例不存在 |
| **CREATED** | 数据拨号实例被创建 |
| **IDLE** | 数据拨号实例已经被配置 |
| **CONNECTING** | 正在进行拨号 |
| **CONNECTED** | 数据拨号成功，获取到 IP 地址等信息 |
| **DISCONNECTED** | 数据拨号失败或者断开 |
| **DELETED** | 数据拨号实例被删除 |

- **图2（数据拨号状态转换 / Data Call State Transition）解读**——状态机迁移（横轴七列：NONE → CREATED → IDLE → CONNECTING → CONNECTED → DISCONNECTED → DELETED）：
  - `NONE --CREATE_REQ--> CREATED`：创建实例
  - `CREATED --CONFIG_REQ--> IDLE`：配置实例
  - `IDLE --START_REQ--> CONNECTING`：发起拨号
  - `CONNECTING --Data call succeeded--> CONNECTED`：拨号成功（虚线/异步事件）
  - `CONNECTING --Data call failed--> DISCONNECTED`：拨号失败（虚线/异步事件）
  - `CONNECTED --Network disconnected--> DISCONNECTED`：网络断开（虚线/异步事件）
  - `DISCONNECTED --STOP_REQ--> IDLE`：停止后回到 IDLE
  - `CONNECTED --STOP_REQ--> IDLE`：连接态停止回到 IDLE
  - `DISCONNECTED --Reconnect enabled--> CONNECTING`（虚线）：启用重连则自动重新拨号
  - `CONNECTING --STOP_REQ--> IDLE`：拨号中停止回到 IDLE
  - **DELETE_REQ**：CREATED / IDLE / CONNECTING / CONNECTED / DISCONNECTED 任一状态均可通过 `DELETE_REQ` 迁移到 **DELETED**
  - `DELETED --Immediate transition--> NONE`：删除后立即回到 NONE

  **解读**：实线箭头为同步请求（REQ 由 API 主动触发：CREATE/CONFIG/START/STOP/DELETE），虚线箭头为异步事件（由网络/SDK 回调驱动：成功/失败/断开/重连）。这张图是后续 API 调用顺序（init→create→param_alloc/config→start→…→stop→delete→deinit）的状态依据。

### 2.4 异常处理（p.12）

- 数据拨号 API 采用 **C/S（Client/Server）** 架构实现。API 接口在初始化时创建与服务程序的通信连接；调用 API 时，API 发送请求给服务程序，服务程序处理完成后返回结果给 API。
- **服务程序异常退出的两种感知方式**：
  1. 调用 API 时返回 **`QL_ERR_ABORTED`**；
  2. 用户调用 **`ql_data_call_set_service_error_cb()`** 注册服务异常回调函数，监控服务程序异常退出。
- **恢复流程**：检测到服务异常退出后，用户应**先调用 `ql_data_call_deinit()`** 注销数据拨号服务，**然后再调用 `ql_data_call_init()`** 重新启动数据拨号服务，进行业务恢复。

---

## 第 3 章　数据拨号 API

### 3.1 头文件（p.13）

- 数据拨号 API 头文件为 **`ql_data_call.h`**，位于 **`ql-sysroots/usr/include/ql-sdk/`** 目录下。
- 部分公用结构体定义在头文件 **`ql_net_common.h`** 中。
- 若无特别说明，本文档涉及的头文件均在该目录下。

### 3.2 函数概览（表1，p.13~14）

完整 34 个 API（按文档顺序）：

| 函数 | 描述 |
|---|---|
| `ql_data_call_init()` | 初始化数据拨号服务 |
| `ql_data_call_create()` | 创建拨号实例 |
| `ql_data_call_param_alloc()` | 申请数据拨号配置实例 |
| `ql_data_call_param_init()` | 初始化数据拨号配置实例 |
| `ql_data_call_param_free()` | 释放数据拨号配置实例 |
| `ql_data_call_param_set_apn_id()` | 设置配置实例的 APN ID |
| `ql_data_call_param_get_apn_id()` | 获取配置实例的 APN ID |
| `ql_data_call_param_set_apn_name()` | 设置配置实例的 APN 名称 |
| `ql_data_call_param_get_apn_name()` | 获取配置实例的 APN 名称 |
| `ql_data_call_param_set_user_name()` | 设置配置实例的用户名 |
| `ql_data_call_param_get_user_name()` | 获取配置实例的用户名 |
| `ql_data_call_param_set_user_password()` | 设置配置实例的用户密码 |
| `ql_data_call_param_get_user_password()` | 获取配置实例的用户密码 |
| `ql_data_call_param_set_auth_pref()` | 设置配置实例的认证类型 |
| `ql_data_call_param_get_auth_pref()` | 获取配置实例的认证类型 |
| `ql_data_call_param_set_ip_version()` | 设置配置实例的 IP 类型 |
| `ql_data_call_param_get_ip_version()` | 获取配置实例的 IP 类型 |
| `ql_data_call_param_set_reconnect_mode()` | 设置自动重连模式 |
| `ql_data_call_param_get_reconnect_mode()` | 获取自动重连模式配置 |
| `ql_data_call_param_set_reconnect_interval()` | 设置自动重连间隔 |
| `ql_data_call_param_get_reconnect_interval()` | 获取自动重连间隔配置 |
| `ql_data_call_config()` | 设置配置实例到数据拨号实例 |
| `ql_data_call_get_config()` | 获取数据拨号实例的配置实例 |
| `ql_data_call_start()` | 开始数据拨号 |
| `ql_data_call_stop()` | 停止数据拨号 |
| `ql_data_call_delete()` | 删除数据拨号实例 |
| `ql_data_call_get_list()` | 获取数据拨号实例列表 |
| `ql_data_call_get_status()` | 获取数据拨号状态 |
| `ql_data_call_set_status_ind_cb()` | 设置数据拨号状态变化上报回调函数 |
| `ql_data_call_set_apn_config()` | 设置 APN 配置信息 |
| `ql_data_call_get_apn_config()` | 获取 APN 配置信息 |
| `ql_data_call_set_service_error_cb()` | 设置服务程序异常退出上报的回调函数 |
| `ql_data_call_get_pkt_stats()` | 获取数据拨号实例数据包的统计信息（1.0.1 新增） |
| `ql_data_call_deinit()` | 去初始化数据拨号服务 |

**API 调用生命周期分组解读**：
- **服务级**：`init` / `deinit`（成对，全局一次）
- **实例级**：`create` / `delete`，`start` / `stop`，`get_list` / `get_status`
- **配置实例级（param_*）**：`alloc` / `init` / `free`（生命周期管理）+ 一组 `set_*/get_*` 属性读写器（apn_id、apn_name、user_name、user_password、auth_pref、ip_version、reconnect_mode、reconnect_interval）
- **配置绑定**：`config` / `get_config`（把 param 实例绑定到 data call 实例）
- **回调注册**：`set_status_ind_cb`（状态变化）、`set_service_error_cb`（服务异常）
- **APN 配置**：`set_apn_config` / `get_apn_config`
- **统计**：`get_pkt_stats`

### 3.3 函数详解

#### 3.3.1 `ql_data_call_init`（p.14）— 初始化数据拨号服务

```c
int ql_data_call_init(void)
```
- **参数**：无
- **返回值**：
  - `QL_ERR_OK`：函数执行成功
  - `QL_ERR_INVALID_ARG`：非法参数，执行失败
  - `QL_ERR_UNKNOWN`：未知错误，无法连接到服务
  - `QL_ERR_SERVICE_NOT_READY`：数据拨号服务未准备就绪，建议重试
  - 其他值：执行失败，错误码详见 `ql_type.h`
- **备注**：使用其他数据拨号 API 前，**必须**先调用该函数初始化数据拨号服务。

#### 3.3.2 `ql_data_call_create`（p.15）— 创建数据拨号实例

```c
int ql_data_call_create(int call_id, const char *call_name, int is_background)
```
- **参数**：
  - `call_id` [In]：数据拨号实例的唯一标识，范围 **1 ~ INT_MAX**，需自定义。
  - `call_name` [In]：数据拨号实例名称，需自定义。
  - `is_background` [In]：设置数据拨号实例是否由后台服务管理：
    - `1` = 由**后台服务**管理，即使拨号程序退出，拨号实例仍然存在；
    - `0` = 由**拨号程序**管理，若拨号程序退出，拨号实例也被相应删除。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG` / `QL_ERR_UNKNOWN`（无法连接到服务）/ `QL_ERR_SERVICE_NOT_READY`（建议重试）/ 其他值（详见 `ql_type.h`）。

**解读**：`is_background=1` 提供了拨号守护进程崩溃后链路保持的能力（适合后台常驻拨号），`is_background=0` 则随进程生命周期清理实例。

#### 3.3.3 `ql_data_call_param_alloc`（p.16）— 申请配置实例

```c
ql_data_call_param_t *ql_data_call_param_alloc(void)
```
- **参数**：无
- **返回值**：
  - 配置实例的地址：执行成功
  - `NULL`：内存不足，执行失败
- **备注**：配置实例使用完成后，应调用 `ql_data_call_param_free()` 释放。

##### 3.3.3.1 `ql_data_call_param_t`（p.16）
```c
typedef void ql_data_call_param_t
```
数据拨号配置实例的类型定义为 **`void`**（不透明句柄），用户无需关注其格式及内容。

#### 3.3.4 `ql_data_call_param_init`（p.16）— 初始化配置实例

```c
int ql_data_call_param_init(ql_data_call_param_t *param)
```
- **参数**：`param` [In]：需要初始化的配置实例，由 `ql_data_call_param_alloc()` 申请。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

#### 3.3.5 `ql_data_call_param_free`（p.17）— 释放配置实例

```c
int ql_data_call_param_free(ql_data_call_param_t *param)
```
- **参数**：`param` [In]：需要释放的配置实例，由 `ql_data_call_param_alloc()` 申请。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

#### 3.3.6 `ql_data_call_param_set_apn_id`（p.17）— 设置 APN ID

```c
int ql_data_call_param_set_apn_id(ql_data_call_param_t *param, int apn_id)
```
- **参数**：
  - `param` [In]：配置实例。
  - `apn_id` [In]：为配置实例设置的 APN ID，**范围 1~4，数据拨号建议使用 2~4**。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

**解读**：呼应 2.1 节——APN1 留给默认承载，拨号用 2~4。

#### 3.3.7 `ql_data_call_param_get_apn_id`（p.17~18）— 获取 APN ID

```c
int ql_data_call_param_get_apn_id(ql_data_call_param_t *param, int *apn_id)
```
- **参数**：`param` [In]：配置实例；`apn_id` [Out]：指定配置实例的 APN ID。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

#### 3.3.8 `ql_data_call_param_set_apn_name`（p.18）— 设置 APN 名称

```c
int ql_data_call_param_set_apn_name(ql_data_call_param_t *param, const char *apn_name)
```
- **参数**：`param` [In]：配置实例；`apn_name` [In]：为配置实例设置的 APN 名称。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

#### 3.3.9 `ql_data_call_param_get_apn_name`（p.18~19）— 获取 APN 名称

```c
int ql_data_call_param_get_apn_name(ql_data_call_param_t *param, char *buf, int buf_len)
```
- **参数**：
  - `param` [In]：配置实例；
  - `buf` [Out]：APN 名称缓存；
  - `buf_len` [In]：APN 名称缓存长度。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

**解读**：get 类带缓冲区接口需传入 `buf_len` 防止溢出（与本仓库 EG25 `Ql_SendAT` 缓冲契约相同的工程关切）。

#### 3.3.10 `ql_data_call_param_set_user_name`（p.19）— 设置用户名

```c
int ql_data_call_param_set_user_name(ql_data_call_param_t *param, const char *user_name)
```
- **参数**：`param` [In]：配置实例；`user_name` [In]：为配置实例设置的用户名。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

#### 3.3.11 `ql_data_call_param_get_user_name`（p.19）— 获取用户名

```c
int ql_data_call_param_get_user_name(ql_data_call_param_t *param, char *buf, int buf_len)
```
- **参数**：`param` [In]：配置实例；`buf` [Out]：用户名缓存；`buf_len` [In]：缓存长度（本页签名可见，参数表续在下一页）。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`（按同类 get 接口推断，下一段读取确认）。

**确认（p.20）**：`ql_data_call_param_get_user_name` 参数表：`param`[In]、`buf`[Out]（APN 用户名缓存）、`buf_len`[In]（APN 用户名缓存长度）；返回值 `QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

#### 3.3.12 `ql_data_call_param_set_user_password`（p.20）— 设置用户密码

```c
int ql_data_call_param_set_user_password(ql_data_call_param_t *param, const char *user_password)
```
- **参数**：`param` [In]：配置实例；`user_password` [In]：指定配置实例的 APN 用户密码。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

#### 3.3.13 `ql_data_call_param_get_user_password`（p.20~21）— 获取用户密码

```c
int ql_data_call_param_get_user_password(ql_data_call_param_t *param, char *buf, int buf_len)
```
- **参数**：`param` [In]：配置实例；`buf` [Out]：APN 用户密码缓存；`buf_len` [In]：APN 用户密码缓存长度。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

#### 3.3.14 `ql_data_call_param_set_auth_pref`（p.21）— 设置认证类型

```c
int ql_data_call_param_set_auth_pref(ql_data_call_param_t *param, int auth_pref)
```
- **参数**：`param` [In]：配置实例；`auth_pref` [In]：认证类型，枚举详见 3.3.14.1（`QL_NET_AUTH_PREF_E`）。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

##### 3.3.14.1 `QL_NET_AUTH_PREF_E`（p.21~22）— 认证类型枚举

```c
typedef enum {
    QL_NET_AUTH_PREF_MIN = -1,
    QL_NET_AUTH_PREF_PAP_CHAP_NOT_ALLOWED = 0,
    QL_NET_AUTH_PREF_PAP_ONLY_ALLOWED = 1,
    QL_NET_AUTH_PREF_CHAP_ONLY_ALLOWED = 2,
    QL_NET_AUTH_PREF_PAP_CHAP_BOTH_ALLOWED = 3,
    QL_NET_AUTH_PREF_MAX
} QL_NET_AUTH_PREF_E
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_NET_AUTH_PREF_PAP_CHAP_NOT_ALLOWED` | 0 | 不使用 PAP 和 CHAP 认证 |
| `QL_NET_AUTH_PREF_PAP_ONLY_ALLOWED` | 1 | 只使用 PAP 认证 |
| `QL_NET_AUTH_PREF_CHAP_ONLY_ALLOWED` | 2 | 只使用 CHAP 认证 |
| `QL_NET_AUTH_PREF_PAP_CHAP_BOTH_ALLOWED` | 3 | 使用 PAP 或 CHAP 认证 |

**解读**：`MIN=-1`/`MAX` 为边界哨兵。认证类型须与运营商/APN 签约一致；公网 APN 通常无需认证（值 0），私网 APN 常需 PAP 或 CHAP（配合用户名/密码）。

#### 3.3.15 `ql_data_call_param_get_auth_pref`（p.22）— 获取认证类型

```c
int ql_data_call_param_get_auth_pref(ql_data_call_param_t *param, int *p_data)
```
- **参数**：`param` [In]：配置实例；`p_data` [Out]：认证类型，枚举详见 3.3.14.1。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

#### 3.3.16 `ql_data_call_param_set_ip_version`（p.22~23）— 设置 IP 类型

```c
int ql_data_call_param_set_ip_version(ql_data_call_param_t *param, int ip_ver)
```
- **参数**：`param` [In]：配置实例；`ip_ver` [In]：IP 类型，枚举 `QL_NET_IP_VER_E` 详见 3.3.16.1。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

##### 3.3.16.1 `QL_NET_IP_VER_E`（p.23）— IP 类型枚举

```c
typedef enum {
    QL_NET_IP_VER_MIN = -0,
    QL_NET_IP_VER_V4 = 0x1,
    QL_NET_IP_VER_V6 = 0x2,
    QL_NET_IP_VER_V4V6 = 0x3,
    QL_NET_IP_VER_MAX
} QL_NET_IP_VER_E
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_NET_IP_VER_V4` | 0x1 | IPv4 |
| `QL_NET_IP_VER_V6` | 0x2 | IPv6 |
| `QL_NET_IP_VER_V4V6` | 0x3 | IPv4v6（**不支持**） |

**重要约束**：`QL_NET_IP_VER_V4V6`（双栈）在本平台**不支持**！实际拨号只能选 IPv4 或 IPv6 之一。这与下文 `call_status` 中 `PARTIAL_V4_CONNECTED`/`PARTIAL_V6_CONNECTED` 的"部分连接"语义形成呼应——状态枚举保留了双栈语义，但 IP 版本配置侧禁用了 V4V6。

#### 3.3.17 `ql_data_call_param_get_ip_version`（p.23~24）— 获取 IP 类型

```c
int ql_data_call_param_get_ip_version(ql_data_call_param_t *param, int *ip_ver)
```
- **参数**：`param` [In]：配置实例；`ip_ver` [Out]：指定配置实例的 IP 类型，枚举详见 3.3.16.1。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

#### 3.3.18 `ql_data_call_param_set_reconnect_mode`（p.24）— 设置自动重连模式

```c
int ql_data_call_param_set_reconnect_mode(ql_data_call_param_t *param, int mode)
```
- **参数**：`param` [In]：配置实例；`mode` [In]：重连模式（参考第 2.2 章），枚举 `QL_NET_DATA_CALL_RECONNECT_MODE_E` 详见 3.3.18.1。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

##### 3.3.18.1 `QL_NET_DATA_CALL_RECONNECT_MODE_E`（p.24~25）— 重连模式枚举

```c
typedef enum {
    QL_NET_DATA_CALL_RECONNECT_MODE_MIN = -1,
    QL_NET_DATA_CALL_RECONNECT_DISABLE = 0x0,
    QL_NET_DATA_CALL_RECONNECT_NORMAL = 0x1,
    QL_NET_DATA_CALL_RECONNECT_MODE_1 = 0x2,
    QL_NET_DATA_CALL_RECONNECT_MODE_2 = 0x3,
    QL_NET_DATA_CALL_RECONONECT_MODE_MAX   /* 注：原文拼写为 RECONONECT */
} QL_NET_DATA_CALL_RECONNECT_MODE_E
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_NET_DATA_CALL_RECONNECT_DISABLE` | 0x0 | DISABLE（不自动重连） |
| `QL_NET_DATA_CALL_RECONNECT_NORMAL` | 0x1 | NORMAL（固定间隔，建议≥20s） |
| `QL_NET_DATA_CALL_RECONNECT_MODE_1` | 0x2 | MODE1（退避，触顶保持 Tn） |
| `QL_NET_DATA_CALL_RECONNECT_MODE_2` | 0x3 | MODE2（循环退避，触顶回 T1） |

> 注：枚举 MAX 哨兵原文拼写为 `QL_NET_DATA_CALL_RECONONECT_MODE_MAX`（多一个 ON），属文档笔误，照录。

#### 3.3.19 `ql_data_call_param_get_reconnect_mode`（p.25）— 获取重连模式

```c
int ql_data_call_param_get_reconnect_mode(ql_data_call_param_t *param, int *p_mode)
```
- **参数**：`param` [In]：配置实例；`p_mode` [Out]：重连模式（参考第 2.2 章），枚举详见 3.3.18.1。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

#### 3.3.20 `ql_data_call_param_set_reconnect_interval`（p.25~26）— 设置自动重连间隔

```c
int ql_data_call_param_set_reconnect_interval(ql_data_call_param_t *param, int *time_list, int num)
```
- **参数**：
  - `param` [In]：配置实例；
  - `time_list` [In]：时间间隔列表，整型数组，**数组成员单位：秒**，**最大支持数组长度 20**；
  - `num` [In]：`time_list` 的数组长度。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

**解读**：这就是 2.2 节 MODE1/MODE2 退避序列 {T1…Tn} 的载体，n ≤ 20。NORMAL 模式则相当于序列只有一个固定值。

#### 3.3.21 `ql_data_call_param_get_reconnect_interval`（p.26）— 获取自动重连间隔

```c
int ql_data_call_param_get_reconnect_interval(ql_data_call_param_t *param, int *time_list, int *p_num)
```
- **参数**：
  - `param` [In]：配置实例；
  - `time_list` [Out]：时间间隔列表，整型数组，单位秒，最大长度 20；
  - `p_num` [In/Out]：整型指针，**传入时**指针内容为 `time_list` 的数组长度（容量），**返回时**指针内容为数组中有效成员个数。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

**解读**：`p_num` 是典型的 in/out 容量-计数模式，调用前置入缓冲容量，调用后取回实际写入数。

#### 3.3.22 `ql_data_call_config`（p.26~27）— 绑定配置实例到拨号实例

```c
int ql_data_call_config(int call_id, ql_data_call_param_t *param)
```
- **参数**：`call_id` [In]：拨号实例唯一标识（1~INT_MAX，需自定义）；`param` [In]：配置实例，由 `ql_data_call_param_alloc()` 申请。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG` / 其他值（详见 `ql_type.h`）。

**解读**：把一堆 `param_set_*` 累积好的配置一次性套用到 `create` 出来的 call 实例上（对应状态机 CREATED→IDLE 的 CONFIG_REQ）。

#### 3.3.23 `ql_data_call_get_config`（p.27）— 获取拨号实例的配置实例

```c
int ql_data_call_get_config(int call_id, ql_data_call_param_t *param)
```
- **参数**：`call_id` [In]：拨号实例唯一标识；`param` [Out]：配置实例，由 `ql_data_call_param_alloc()` 申请（作为输出容器）。
- **返回值**：`QL_ERR_OK` / `QL_ERR_INVALID_ARG`。

#### 3.3.24 `ql_data_call_start`（p.27~28）— 开始数据拨号

```c
int ql_data_call_start(int call_id)
```
- **参数**：`call_id` [In]：拨号实例唯一标识（1~INT_MAX）。
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT`（服务未初始化）/ `QL_ERR_SERVICE_NOT_READY`（建议重试）/ `QL_ERR_INVALID_ARG` / 其他值（详见 `ql_type.h`）。

**解读**：对应状态机 IDLE→CONNECTING 的 START_REQ。注意：调用前必须先 `config`，否则会返回 `QL_ERR_INVALID_STATE`（见 3.4 错误码表）。

#### 3.3.25 `ql_data_call_stop`（p.28）— 停止数据拨号

```c
int ql_data_call_stop(int call_id)
```
- **参数**：`call_id` [In]：拨号实例唯一标识（1~INT_MAX）。
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_NOT_READY` / `QL_ERR_INVALID_ARG` / 其他值。

**解读**：对应状态机 CONNECTING/CONNECTED/DISCONNECTED → IDLE 的 STOP_REQ。重连失败需重新配置时，先 stop 再改 config。

#### 3.3.26 `ql_data_call_delete`（p.28~29）— 删除数据拨号实例

```c
int ql_data_call_delete(int call_id)
```
- **参数**：`call_id` [In]：拨号实例唯一标识（1~INT_MAX）。
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_NOT_READY` / `QL_ERR_INVALID_ARG` / 其他值。

**解读**：对应状态机任意态 → DELETED 的 DELETE_REQ，随后立即回到 NONE。

#### 3.3.27 `ql_data_call_get_list`（p.29）— 获取拨号实例列表

```c
int ql_data_call_get_list(ql_data_call_item_t *list, int *list_len)
```
- **参数**：
  - `list` [Out]：拨号实例列表数组，结构体定义详见 3.3.27.1；
  - `list_len` [In/Out]：传入数组容量，返回数组中有效个数。
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_NOT_READY` / `QL_ERR_INVALID_ARG` / 其他值。

##### 3.3.27.1 `ql_data_call_item_t`（p.29~30）— 拨号实例列表项

```c
typedef struct {
    int call_id;
    char call_name[QL_NET_MAX_NAME_LEN];
} ql_data_call_item_t
```

| 类型 | 参数 | 描述 |
|---|---|---|
| int | `call_id` | 数据拨号的 ID |
| char | `call_name` | 数据拨号的名称 |

#### 3.3.28 `ql_data_call_get_status`（p.30）— 获取拨号状态

```c
int ql_data_call_get_status(int call_id, ql_data_call_status_t *p_sta)
```
- **参数**：`call_id` [In]：拨号实例唯一标识；`p_sta` [Out]：返回拨号实例的状态以及 IP 地址等信息，结构体定义详见 3.3.28.1。
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_NOT_READY` / `QL_ERR_INVALID_ARG` / 其他值。

##### 3.3.28.1 `ql_data_call_status_t`（p.30~31）— 拨号状态信息结构体

```c
typedef struct {
    int call_id;
    char call_name[QL_NET_MAX_NAME_LEN];
    QL_NET_IP_VER_E ip_ver;
    QL_NET_DATA_CALL_STATUS_E call_status;
    char device[QL_NET_MAX_NAME_LEN];
    uint8_t has_addr;
    ql_net_addr_t addr;
    uint8_t has_addr6;
    ql_net_addr6_t addr6;
    int call_end_reason_type;
    int call_end_reason_code;
} ql_data_call_status_t
```

| 类型 | 参数 | 描述 |
|---|---|---|
| int | `call_id` | 数据拨号 ID |
| char | `call_name` | 数据拨号名称 |
| `QL_NET_IP_VER_E` | `ip_ver` | IP 类型（枚举详见 3.3.16.1） |
| `QL_NET_DATA_CALL_STATUS_E` | `call_status` | 数据拨号状态（枚举详见 3.3.28.2） |
| char | `device` | 数据拨号对应的网卡名 |
| uint8_t | `has_addr` | 是否获取到 IPv4 地址 |
| `ql_net_addr_t` | `addr` | IPv4 地址信息（结构体详见 3.3.28.3） |
| uint8_t | `has_addr6` | 是否获取到 IPv6 地址 |
| `ql_net_addr6_t` | `addr6` | IPv6 地址信息（结构体详见 3.3.28.4） |
| int | `call_end_reason_type` | 上一次数据拨号断开原因类型 |
| int | `call_end_reason_code` | 上一次数据拨号断开的原因码 |

**解读**：`device` 字段（网卡名，如 rmnet/usb 接口）是拨号成功后应用层配置路由、做 ping 探测的关键。`call_end_reason_type/code` 用于诊断断链原因（对应状态机里的 Network disconnected 事件）。`has_addr`/`has_addr6` 是布尔标志位，配合 IP 版本判断双栈下哪一栈起来了。

##### 3.3.28.2 `QL_NET_DATA_CALL_STATUS_E`（p.31~32）— 拨号状态枚举

```c
typedef enum {
    QL_NET_DATA_CALL_STATUS_MIN = -1,
    QL_NET_DATA_CALL_STATUS_NONE = 0x0,
    QL_NET_DATA_CALL_STATUS_CREATED = 0x1,
    QL_NET_DATA_CALL_STATUS_IDLE = 0x2,
    QL_NET_DATA_CALL_STATUS_CONNECTING = 0x3,
    QL_NET_DATA_CALL_STATUS_PARTIAL_V4_CONNECTED = 0x4,
    QL_NET_DATA_CALL_STATUS_PARTIAL_V6_CONNECTED = 0x5,
    QL_NET_DATA_CALL_STATUS_CONNECTED = 0x6,
    QL_NET_DATA_CALL_STATUS_DISCONNECTED = 0x7,
    QL_NET_DATA_CALL_STATUS_ERROR = 0x8,
    QL_NET_DATA_CALL_STATUS_DELETED = 0x9,
    QL_NET_DATA_CALL_STATUS_MAX
} QL_NET_DATA_CALL_STATUS_E
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_NET_DATA_CALL_STATUS_NONE` | 0x0 | 置空状态 |
| `QL_NET_DATA_CALL_STATUS_CREATED` | 0x1 | 创建状态 |
| `QL_NET_DATA_CALL_STATUS_IDLE` | 0x2 | 空闲状态 |
| `QL_NET_DATA_CALL_STATUS_CONNECTING` | 0x3 | 连接中状态 |
| `QL_NET_DATA_CALL_STATUS_PARTIAL_V4_CONNECTED` | 0x4 | IPv4 连接成功，IPv6 未连接 |
| `QL_NET_DATA_CALL_STATUS_PARTIAL_V6_CONNECTED` | 0x5 | IPv6 连接成功，IPv4 未连接 |
| `QL_NET_DATA_CALL_STATUS_CONNECTED` | 0x6 | 连接成功 |
| `QL_NET_DATA_CALL_STATUS_DISCONNECTED` | 0x7 | 连接断开 |
| `QL_NET_DATA_CALL_STATUS_ERROR` | 0x8 | 数据拨号失败 |
| `QL_NET_DATA_CALL_STATUS_DELETED` | 0x9 | 删除状态 |

**解读**：相比 2.3 节文字版的 7 个状态，枚举多出 3 个细分态：`PARTIAL_V4_CONNECTED`（0x4）、`PARTIAL_V6_CONNECTED`（0x5）和 `ERROR`（0x8）。partial 态用于双栈拨号中只起来一栈的过渡状态；`ERROR` 与 `DISCONNECTED` 区分了"拨号失败"与"曾连接后断开"。应用层判断"在线"时，应把 0x4/0x5/0x6 视情况处理（纯 IPv4 业务可接受 0x4）。

##### 3.3.28.3 `ql_net_addr_t`（p.32~33）— IPv4 地址信息结构体

```c
typedef struct {
    char addr[QL_NET_MAX_ADDR_LEN];
    char netmask[QL_NET_MAX_ADDR_LEN];
    uint8_t subnet_bits;
    char gateway[QL_NET_MAX_ADDR_LEN];
    char dnsp[QL_NET_MAX_ADDR_LEN];
    char dnss[QL_NET_MAX_ADDR_LEN];
} ql_net_addr_t
```

| 类型 | 参数 | 描述 |
|---|---|---|
| char | `addr` | IPv4 地址 |
| char | `netmask` | 子网掩码 |
| uint8_t | `subnet_bits` | 子网的网络位的长度（CIDR 前缀位数） |
| char | `gateway` | 网关地址 |
| char | `dnsp` | 首选 DNS |
| char | `dnss` | 备选 DNS |

##### 3.3.28.4 `ql_net_addr6_t`（p.33）— IPv6 地址信息结构体

```c
typedef struct {
    char addr[QL_NET_MAX_ADDR_LEN];
    char prefix[QL_NET_MAX_ADDR_LEN];
    int32_t prefix_bits;
    char gateway[QL_NET_MAX_ADDR_LEN];
    char dnsp[QL_NET_MAX_ADDR_LEN];
    char dnss[QL_NET_MAX_ADDR_LEN];
} ql_net_addr6_t
```

| 类型 | 参数 | 描述 |
|---|---|---|
| char | `addr` | IPv6 地址 |
| char | `prefix` | 子网前缀 |
| int32_t | `prefix_bits` | 子网前缀的长度 |
| char | `gateway` | 网关地址 |
| char | `dnsp` | 首选 DNS |
| char | `dnss` | 备选 DNS |

**解读**：IPv4 与 IPv6 结构体对称，区别在 v4 用 `netmask`+`subnet_bits`(uint8)，v6 用 `prefix`+`prefix_bits`(int32)。所有地址字段均为字符串（点分/冒分十六进制表示），缓冲长度统一为 `QL_NET_MAX_ADDR_LEN`。

#### 3.3.29 `ql_data_call_set_status_ind_cb`（p.34）— 设置状态变化回调

```c
int ql_data_call_set_status_ind_cb(ql_data_call_status_ind_cb_f cb)
```
- **说明**：当数据拨号状态发生变化时，会调用传入的回调函数。
- **参数**：`cb` [In]：传入的回调函数，状态变化时被调用，回调函数原型详见 3.3.29.1。
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_NOT_READY` / `QL_ERR_INVALID_ARG` / 其他值。
- **⚠️ 备注（重要）**：**回调函数中不可调用任何数据拨号接口，且回调函数中禁止处理耗时或者阻塞的任务。**

##### 3.3.29.1 `ql_data_call_status_ind_cb_f`（p.34~35）— 状态变化回调原型

```c
typedef void (*ql_data_call_status_ind_cb_f)(int call_id,
        QL_NET_DATA_CALL_STATUS_E pre_call_status,
        ql_data_call_status_t *p_msg)
```
- **参数**：
  - `call_id` [In]：拨号实例唯一标识（1~INT_MAX）；
  - `pre_call_status` [In]：之前数据拨号的状态（枚举详见 3.3.28.2）；
  - `p_msg` [In]：数据拨号的状态信息（结构体详见 3.3.28.1，含当前状态与地址）。
- **返回值**：无。

**解读**：回调同时给出"前一状态"和"当前完整状态信息"，便于做边沿检测（如 CONNECTED→DISCONNECTED 触发恢复）。回调线程内禁止再调拨号 API、禁止阻塞——这与本仓库 EC200A 实践中 SDK indication 回调线程的约束一致（参见 [[ec200a-dial-callback-shadowing]]：回调遮蔽导致 `g_last_call_status` 永不更新的教训，说明正确注册此回调至关重要）。

#### 3.3.30 `ql_data_call_set_apn_config`（p.35）— 设置 APN 配置信息

```c
int ql_data_call_set_apn_config(int apn_id, ql_data_call_apn_config_t *p_info)
```
- **说明**：设置 APN 信息。**若指定的 APN ID 不存在，则自动创建新 APN，然后再配置 APN 信息。**
- **参数**：
  - `apn_id` [In]：APN ID，范围 1~4。APN1 用于建立默认承载，建议使用 APN2~APN4 进行数据拨号；
  - `p_info` [In]：APN 配置信息，结构体定义详见 3.3.30.1。
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_NOT_READY` / `QL_ERR_INVALID_ARG` / 其他值。

##### 3.3.30.1 `ql_data_call_apn_config_t`（p.35~36）— APN 配置信息结构体

```c
typedef struct {
    QL_NET_IP_VER_E ip_ver;
    QL_NET_AUTH_PREF_E auth_pref;
    char apn_name[QL_NET_MAX_APN_NAME_LEN];
    char username[QL_NET_MAX_APN_USERNAME_LEN];
    char password[QL_NET_MAX_APN_PASSWORD_LEN];
} ql_data_call_apn_config_t
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_NET_IP_VER_E` | `ip_ver` | IP 类型（枚举详见 3.3.16.1） |
| `QL_NET_AUTH_PREF_E` | `auth_pref` | 认证类型（枚举详见 3.3.14.1） |
| char | `apn_name` | APN 名称 |
| char | `username` | 用户名 |
| char | `password` | 密码 |

**解读**：`set_apn_config` 是直接写 APN 槽位（含可写 APN1 默认承载）的底层接口；与逐字段 `param_set_*` + `config` 的"配置实例→拨号实例"路径不同——前者面向 APN 表（`AT+CGDCONT` 视角），后者面向拨号实例。注意各字段缓冲长度有专属宏：`QL_NET_MAX_APN_NAME_LEN` / `QL_NET_MAX_APN_USERNAME_LEN` / `QL_NET_MAX_APN_PASSWORD_LEN`。**呼应 2.1 节推荐流程步骤1（用本接口写 APN1 默认承载）和步骤2（写 APN2~4 拨号 APN）。** ⚠️ 与本仓库 [[ec200a-apn-id-aligned]] 的教训相关：apn 配置接口必须在运行期真正被调用，否则 apn.json 中的 APN 不生效。

#### 3.3.31 `ql_data_call_get_apn_config`（p.36）— 获取 APN 配置信息

```c
int ql_data_call_get_apn_config(int apn_id, ql_data_call_apn_config_t *p_info)
```
- **参数**：`apn_id` [In]：APN ID（1~4，APN1 默认承载，建议拨号用 2~4）；`p_info` [Out]：APN 配置信息（详见 3.3.30.1）。
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_NOT_READY` / `QL_ERR_INVALID_ARG` / 其他值。

#### 3.3.32 `ql_data_call_set_service_error_cb`（p.36~37）— 设置服务异常回调

```c
int ql_data_call_set_service_error_cb(ql_data_call_service_error_cb_f cb)
```
- **说明**：设置数据拨号服务程序异常退出上报的回调函数。
- **参数**：`cb` [In]：回调函数，服务程序异常退出时被执行（详见 3.3.32.1）。
- **返回值**：`QL_ERR_OK` / 其他值（详见 `ql_type.h`）。
- **⚠️ 备注**：**回调函数中不可调用任何数据拨号接口，且禁止处理耗时或阻塞的任务。**

##### 3.3.32.1 `ql_data_call_service_error_cb_f`（p.37）— 服务异常回调原型

```c
typedef void (*ql_data_call_service_error_cb_f)(int error)
```
- **参数**：`error` [In]：错误码，**当前仅返回 `QL_ERR_ABORTED`**。
- **返回值**：无。

**解读**：呼应 2.4 节异常处理——回调触发后，恢复流程为 `deinit()` → `init()` 重建服务连接。由于回调内禁止调拨号接口，实际恢复动作须投递到主循环/工作线程执行（典型做法：回调里置标志位，主循环检测后做 deinit/init）。

#### 3.3.33 `ql_data_call_get_pkt_stats`（p.37~38，1.0.1 新增）— 获取数据包统计

```c
int ql_data_call_get_pkt_stats(int call_id, ql_data_call_pkt_stats_t *p_sta)
```
- **参数**：`call_id` [In]：拨号实例唯一标识（1~INT_MAX）；`p_sta` [Out]：数据包统计信息（详见 3.3.33.1）。
- **返回值**：`QL_ERR_OK` / 其他值（详见 `ql_type.h`）。

##### 3.3.33.1 `ql_data_call_pkt_stats_t`（p.38）— 数据包统计结构体

```c
typedef struct {
    uint64_t tx_pkts;
    uint64_t tx_bytes;
    uint64_t tx_dropped_pkts;
    uint64_t rx_pkts;
    uint64_t rx_bytes;
    uint64_t rx_dropped_pkts;
} ql_data_call_pkt_stats_t
```

| 类型 | 参数 | 描述 |
|---|---|---|
| uint64_t | `tx_pkts` | 发送的包数 |
| uint64_t | `tx_bytes` | 发送的字节数 |
| uint64_t | `tx_dropped_pkts` | 发送过程中丢弃的包数 |
| uint64_t | `rx_pkts` | 接收的包数 |
| uint64_t | `rx_bytes` | 接收的字节数 |
| uint64_t | `rx_dropped_pkts` | 接收过程中丢弃的包数 |

**解读**：全部 `uint64_t`，避免大流量计数溢出（呼应本仓库 [[imx6-bug-audit-cron-20260611]] 中"流量 2GiB stoi 崩溃"的教训——计数应用 64 位，且解析侧也要防溢出）。该统计可用于流量监控与链路健康判断（tx/rx_bytes 长期不增长 + dropped 攀升可作为掉线旁证）。

#### 3.3.34 `ql_data_call_deinit`（p.38~39）— 去初始化数据拨号服务

```c
int ql_data_call_deinit(void)
```
- **参数**：无
- **返回值**：`QL_ERR_OK` / 其他值（详见 `ql_type.h`）。
- **解读**：与 `init` 成对，服务异常恢复时先 `deinit` 再 `init`。

### 3.4 错误码及恢复措施（表2，p.39~40）

| 值 | 错误码 | 描述 | 恢复措施 |
|---|---|---|---|
| **-1010** | `QL_ERR_INVALID_ARG` | 无效参数 | 确保 API 参数有效后重试 |
| **-1067** | `QL_ERR_SERVICE_NOT_READY` | 服务未就绪 | 仅调用 `ql_data_call_init()` 时返回。可 `usleep()` 休眠 100 毫秒后再重新 `ql_data_call_init()`，可重复直至成功。建议设置超时时间；若超时，请抓取调用 API 全过程的 log 提交至移远技术支持 |
| **-1034** | `QL_ERR_NOT_INIT` | 未进行初始化操作 | 调用 `ql_data_call_init()` 初始化数据拨号服务后重试 |
| **-1099** | `QL_ERR_SERVICE_ABORT` | 服务端异常 | 先调用 `ql_data_call_deinit()`，然后 `ql_data_call_init()` 重新初始化后重试 |
| **-1013** | `QL_ERR_INVALID_STATE` | 无效状态 | ① 若 `ql_data_call_start()` 后又调用 `start()`/`create()`/`config()` 返回该码，需先 `ql_data_call_stop()` 停止拨号后重试；② 若未调用 `config()` 就调用 `start()`，也可能返回该码，需先 `config()` 再 `start()` |

**解读**：
- 关键错误码数值固定：`-1010`/`-1013`/`-1034`/`-1067`/`-1099`，可直接用于代码比对。
- `-1067` 的 100ms 轮询重试是 init 的标准容错套路（呼应 3.3.1/表1 描述"建议重试"）。
- `-1013`（无效状态）本质是状态机违例——必须遵守 create→config→start 的顺序，重连/重配前必须 stop。⚠️ 注意 3.3.32.1 服务异常回调返回的是 `QL_ERR_ABORTED`，而本表服务端异常错误码写作 `QL_ERR_SERVICE_ABORT`(-1099)，二者命名在文档中并存，应用判断时以实际头文件 `ql_type.h` 定义为准。

### 3.4（续）错误码及恢复措施补充（p.40）

表2 延续到第 40 页，补充以下错误码：

| 值 | 错误码 | 描述 | 恢复措施 |
|---|---|---|---|
| **-1062** | `QL_ERR_INTERFACE_NOT_FOUND` | 未创建数据拨号实例 | 调用 `ql_data_call_create()` 创建数据拨号实例后重试 |
| **-1098** | `QL_ERR_MODEM_OFFLINE` | Modem 未就绪 | 若调用 `ql_data_call_set_apn_config()`、`ql_data_call_get_apn_config()` 或 `ql_data_call_bind_subscription()` 时返回该码，`usleep()` 休眠 100ms 后循环重试报错的函数，可重复直至成功。建议设置超时时间；超时则抓全过程 log 提交移远技术支持 |
| **-1019** | `QL_ERR_INVALID_CALL_ID` | call_id 无效 | 该 call_id 未创建数据拨号。检查 call_id 有效性后重试 |
| **-1030** | `QL_ERR_NO_MEM` | 内存不足 | 确保内存充足后重试 |
| **-1004** | `QL_ERR_GENERIC` | 通用错误 | 抓取调用 API 全过程的 log 并提交至移远技术支持 |
| **-1002** | `QL_ERR_INTERNAL` | 内部错误 | 抓取调用 API 全过程的 log 并提交至移远技术支持 |

**解读与补充**：
- 完整错误码数值清单（便于代码比对）：`-1002`(INTERNAL)、`-1004`(GENERIC)、`-1010`(INVALID_ARG)、`-1013`(INVALID_STATE)、`-1019`(INVALID_CALL_ID)、`-1030`(NO_MEM)、`-1034`(NOT_INIT)、`-1062`(INTERFACE_NOT_FOUND)、`-1067`(SERVICE_NOT_READY)、`-1098`(MODEM_OFFLINE)、`-1099`(SERVICE_ABORT)。
- `-1098`（Modem 未就绪）出现了一个新接口 **`ql_data_call_bind_subscription()`**——该接口未在第3.3章函数详解中展开（疑为多卡/多 subscription 绑定接口），仅在此处错误码恢复措施里被提及，属本文档的"隐藏 API"线索，使用前应查头文件 `ql_data_call.h` 确认。
- 恢复措施套路归纳：**参数类**（-1010/-1019）→ 校验参数；**状态类**（-1013/-1034/-1062）→ 补齐前置调用顺序（init→create→config→start）；**就绪类**（-1067/-1098）→ 100ms 轮询重试 + 超时上报；**致命类**（-1099 service abort）→ deinit+init 重建；**未知类**（-1002/-1004/-1030）→ 抓 log/查内存。

### 3.5 函数使用示例（p.40~41）

- **前置知识**：调用数据拨号函数需了解 Linux 系统的网络配置以及 DNS 处理（即拨号成功拿到 `ql_net_addr_t` 后，需自行 ifconfig/route/写 resolv.conf——这与本仓库 [[eg25-license-pending-rbmaster-dns-stuck]] 中 resolv.conf=0.0.0.0 的 DNS 处理坑高度相关）。
- **示例场景**：(U)SIM 卡与运营商签约的 APN：
  - 公网 APN：`apnpublic`
  - 私网 APN：`apnprivate`（私网 APN 用于建立默认承载）
- **目标**：同时拨两路数据拨号。示例代码参考 **`sample/data_call/sample_multi_data_call.c`**。

**伪代码流程（原文，含 APN 槽位分配）**：

```c
data_call_cb()
{
    /* 拨号状态处理 */
}

main()
{
    /* 初始化拨号服务 */
    ql_data_call_init

    /* 设置拨号状态变化上报回调函数 */
    ql_data_call_set_status_ind_cb  data_call_cb

    /* 设置默认承载 APN 的信息 —— APN1 = 私网 apnprivate, IPv4 */
    ql_data_call_set_apn_config  apn_id=1, apn_name=apnprivate, ip_ver=QL_NET_IP_VER_V4

    /* 公网拨号使用 APN2，设置 APN2 信息 */
    ql_data_call_set_apn_config  apn_id=2, apn_name=apnpublic, ip_ver=ipv4

    /* 私网拨号使用 APN3，设置 APN3 信息 */
    ql_data_call_set_apn_config  apn_id=3, apn_name=apnprivate, ip_ver=ipv4

    /* ===== 第一路：公网数据拨号实例，call_id=1 ===== */
    ql_data_call_create  call_id=1, call_name=public, is_background=0
    /* 配置公网拨号的配置实例 */
    ql_data_call_param_set_apn_id           apn_id=2
    ql_data_call_param_set_ip_version       ip_ver=ipv4
    ql_data_call_param_set_reconnect_mode
    ql_data_call_param_set_reconnect_interval
    /* 设置配置实例到数据拨号实例 */
    ql_data_call_config  call_id=1
    /* 开始拨号 */
    ql_data_call_start   call_id=1

    /* ===== 第二路：私网数据拨号实例，call_id=2 ===== */
    ql_data_call_create  call_id=2, call_name=private, is_background=0
    /* 配置私网拨号的配置实例 */
    ql_data_call_param_set_apn_id           apn_id=3
    ql_data_call_param_set_ip_version       ip_ver=ipv4
    ql_data_call_param_set_reconnect_mode
    ql_data_call_param_set_reconnect_interval
    /* 设置配置实例到数据拨号实例 */
    ql_data_call_config  call_id=2
    /* 开始拨号 */
    ql_data_call_start   call_id=2
}
```

**示例关键解读**：
1. **APN 槽位映射**：APN1=私网（默认承载）、APN2=公网（apnpublic）、APN3=私网（apnprivate）。拨号实例只用 APN2/APN3（符合"拨号用 2~4"规则），APN1 仅作默认承载。
2. **两路拨号对应关系**：call_id=1（public 实例）绑定 apn_id=2；call_id=2（private 实例）绑定 apn_id=3。即"拨号实例 ↔ APN 槽位"通过 `param_set_apn_id` 关联，呼应图1的多路并行模型。
3. **调用顺序铁律**：init → set_status_ind_cb → set_apn_config(写 APN 表) → [create → param_set_* → config → start]（每路重复）。注意 `set_apn_config` 写的是全局 APN 表，`param_set_apn_id` 只是把实例指向某个槽位。
4. **`is_background=0`**：示例两路都用前台管理（进程退出则实例删除）。
5. 注意伪代码省略了 `param_alloc`/`param_init`/`param_free` 的显式申请释放（实际编码必须配对，否则内存泄漏）。

### 3.6 注意事项（p.42）

原文三条（与前述回调约束、线程安全约束呼应，集中强调）：

1. **不可在数据拨号状态回调函数中调用其他 API 接口。**
2. **不可在数据拨号状态回调函数中处理阻塞事务。**
3. **所有数据拨号函数都不支持多线程调用。**

**解读**：第 3 条是全局性硬约束——整个 `ql_data_call_*` API 族**非线程安全**，必须在单线程（通常是拨号主循环/dial_task）内串行调用。若 IPC/nanomsg 等其他线程需要拨号状态，应通过共享变量+锁（或消息队列）从主循环取，而非在其它线程直接调 API。这与本仓库 EC200A 的实践教训直接相关：[[ec200a-nanomsg-exception-crash]]（nanomsg 线程裸调状态查询接口引发崩溃）、[[ec200a-dial-callback-shadowing]]（回调注册错误导致状态不更新）——本节正是这些坑的官方依据。

### 第 4 章　附录 参考文档及术语缩写（p.43）

#### 表3 参考文档

| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |
| [2] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_蜂窝网络信息_开发指导 |

> 引言中"详细信息见文档[1]"指快速开发指导；"先注册网络见文档[2]"指蜂窝网络信息开发指导（即先用 [2] 完成网络注册，再用本文档拨号）。

#### 表4 术语和缩写

| 缩写 | 英文描述 | 中文描述 |
|---|---|---|
| API | Application Programming Interface | 应用程序接口 |
| APN | Access Point Name | 接入点名称 |
| DNS | Domain Name System | 域名系统 |
| ID | Identifier | 标识符 |
| IMS | IP Multimedia Subsystem | IP 多媒体系统 |
| IoV | Internet of Vehicles | 车联网 |
| IP | Internet Protocol | 网络互连协议 |
| IPv4 | Internet Protocol version 4 | 第 4 版互联网协议 |
| IPv6 | Internet Protocol version 6 | 第 6 版互联网协议 |
| LTE | Long Term Evolution | 长期演进 |
| PLMN | Public Land Mobile Network | 公共陆地移动（通信）网络 |
| SDK | Software Development Kit | 软件开发工具包 |
| (U)SIM | (Universal) Subscriber Identity Module | （通用）用户身份识别模块 |

---

## 全文要点速查（工程落地视角）

1. **API 调用生命周期**：`init` →（`set_status_ind_cb`/`set_service_error_cb`）→ `set_apn_config`(写 APN 表) → 每路实例：`param_alloc`/`param_init` → `create` → `param_set_*`(apn_id/ip_version/auth/reconnect…) → `config` → `start` → … → `stop` → `delete` → `param_free` → `deinit`。
2. **APN 规则**：最多 4 路，APN1=默认承载，拨号用 APN2~4；首检 SIM 按 PLMN 预载 APN，`AT+CGDCONT?` 可查；网络未注册时拨号失败属正常，需先按文档[2]注册。
3. **IP 版本**：仅支持 IPv4 / IPv6，**V4V6 双栈不支持**；状态枚举有 PARTIAL_V4/V6_CONNECTED 过渡态。
4. **重连**：DISABLE/NORMAL（≥20s，防黑名单）/MODE1（退避触顶保持）/MODE2（循环退避）；间隔列表整型数组，单位秒，最多 20 个。
5. **回调铁律**：状态回调与服务异常回调内**禁止调任何拨号 API、禁止阻塞**；整套 API **非线程安全（单线程串行）**。
6. **异常恢复**：C/S 架构，服务异常→`QL_ERR_ABORTED`/回调→`deinit`+`init` 重建；`-1067/-1098` 未就绪→100ms 轮询重试。
7. **统计**：`get_pkt_stats` 全 uint64（tx/rx pkts/bytes/dropped），1.0.1 新增，可用于流量与健康监测。
8. **拿到 IP 后**：需自行做 Linux 网络配置（ifconfig/route）与 DNS 写入（resolv.conf），SDK 只返回 `ql_net_addr_t`/`ql_net_addr6_t` 字段。

> 本分析已逐页覆盖 PDF 全部 44 页（封面/前言/文档历史/目录与索引/第1~4章/附录），含全部 34 个 API 函数原型与参数、6 个枚举（认证/IP版本/重连模式/拨号状态等）、7 个数据结构（param_t/item_t/status_t/net_addr_t/net_addr6_t/apn_config_t/pkt_stats_t）、2 张图（拨号示意图/状态转换图）、2 张表（函数概览/错误码11条）及完整双路拨号示例伪代码。

<!-- GENERATION_COMPLETE -->
