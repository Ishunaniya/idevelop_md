# Quectel AG35-CET QuecOpen(SDK) Wi-Fi 开发指导 — 全文详尽分析

> **文档全名**：AG35-CET QuecOpen(SDK) Wi-Fi 开发指导
> **适用产品系列**：LTE Standard 模块系列（AG35-CET）
> **版本**：V1.0.0（临时文件 / Preliminary）
> **日期**：2023-11-13
> **作者**：Sunshine HUANG
> **总页数**：81 页（文档页脚标注 “x / 80”，即正文 80 页 + 封面页）
> **本分析覆盖范围**：全文第 1–81 页（逐章逐节、逐 API、逐枚举、逐示例展开）

---

## 文档定位与全局结论（导读）

这份文档是移远 AG35-CET QuecOpen(SDK) 平台下 **Wi-Fi 功能的 C 语言 API 参考手册 + 开发指导**。核心事实：

- AG35-CET 模块本身**不带 Wi-Fi 射频**，Wi-Fi 能力是通过**外挂移远 Wi-Fi 模块 FC30R** 实现的。AG35 作为主控（QuecOpen Linux 应用侧）通过 `ql_wifi_*` 系列 SDK API 控制 FC30R。
- FC30R 仅支持 **2.4 GHz** 频段，支持 **AP / STA 两种工作模式**，外加 **Wi-Fi P2P（Wi-Fi Direct 直连）**。
- 所有 API 以 `ql_wifi_` 为前缀，头文件为 `ql_wifi.h`（API 声明）与 `ql_wifi_common.h`（数据结构/枚举定义），位于 SDK 包 `ql-sysroots/usr/include/ql-sdk/` 目录。
- API 分四大族：**全局控制族**（init/deinit/enable/disable/work_mode）、**AP 模式族**（`ql_wifi_ap_*`）、**STA 模式族**（`ql_wifi_sta_*`）、**P2P 族**（`ql_wifi_p2p_*`）。
- 编程范式是**配置 → 注册回调 → 启动**，且**严禁在回调函数内部再调用任何 Wi-Fi API**（会破坏后续消息处理）。

> 与本仓库（modem_mng）的关联提示：modem_mng 目前是 4G 拨号管理守护进程，未涉及 Wi-Fi；本文档属于 AG35 平台 QuecOpen 能力调研资料，若后续要在 AG35 上扩展 Wi-Fi 热点/STA 中继能力，本文是直接依据。

---

## 第 1 章 引言（P8）

- AG35-CET 模块支持 **QuecOpen®** 方案；QuecOpen 是基于 **Linux** 的嵌入式开发平台，用于简化 IoV（车联网）应用的软件设计与开发。详细信息见参考文档 [1]（QuecOpen 总体文档）。
- 本文档适用于 **SDK 构建环境** 的 QuecOpen 方案，专门介绍 AG35-CET QuecOpen 模块 **Wi-Fi 功能相关的 API 及其示例**。

---

## 第 2 章 Wi-Fi 功能实现（P9–P10）

AG35-CET 通过搭配移远 Wi-Fi 模块 **FC30R** 实现 Wi-Fi 功能。本章介绍 FC30R 支持的工作模式以及 Wi-Fi P2P 功能。

### 2.1 Wi-Fi 工作模式（P9）

FC30R 支持两种工作模式：

| 模式 | 说明 |
|---|---|
| **AP 模式** | 模块仅开放**一个热点**，支持 **2.4 GHz** 频段。即作为无线接入点供他设备连接。 |
| **STA 模式** | 模块作为一个**无线网卡**使用，能够连接 **2.4 GHz** 频段的热点。 |

> 解读：AP 模式明确说明"仅开放一个热点"，对应后文 `QL_WIFI_AP_INDEX_E` 枚举虽预留索引，但实际单 AP；工作模式枚举里也只有 `STA` 和 `AP0`（单 AP），没有多 AP。

### 2.2 Wi-Fi P2P 简介（P9–P10）

FC30R 还支持 **Wi-Fi 直连（Wi-Fi Direct）**，即 **Wi-Fi P2P** 功能。

- **Wi-Fi Direct** 是 Wi-Fi 联盟推出的重要协议规范；**Wi-Fi P2P** 是其中一种连接方式。它支持多个 Wi-Fi 设备在**无 AP** 的场景下相互连接，是 **Wi-Fi Display（无线显示）** 功能的基础。
- 应用场景：**Miracast**（Wi-Fi 联盟对支持 Wi-Fi Display 设备的认证名称）。支持 P2P 的智能手机可直接连接支持 P2P 的 IVI（车载信息娱乐）系统，把本地视频/媒体资源传送到 IVI 显示或播放。

**Wi-Fi P2P 三个组件 / 一个设备两个角色：**

- **P2P Device（P2P 设备）**：
  - 支持 **P2P Group Owner** 和 **P2P Client** 两个角色；
  - 通过协商成为 **P2P Group Owner（GO）** 或 **P2P Client** 角色；
  - 支持 **WSC**（Wi-Fi Simple Configuration）和 **P2P Discovery** 机制；
  - 可支持 **WLAN 和 P2P 并发操作**。

- **P2P Group Owner（GO）角色**：
  - 类似接入点（AP）的实体，为相互关联的 Clients（P2P Clients 或 Legacy Clients）提供基础服务集（BSS）功能和服务；
  - 提供 **WSC Internal Registrar** 功能；
  - 可为相互关联的 Clients 彼此之间建立通信；
  - 可将与其关联的 Clients 同时接入不同 WLAN。

- **P2P Client 角色**：
  - 执行 **non-AP STA** 功能；
  - 提供 **WSC Enrollee** 功能。

**组网逻辑（图 1：Wi-Fi P2P 组织结构图）**：

- 组建 P2P Group（即 P2P Network）之前，终端均为独立的 P2P Device。P2P Device 之间完成 P2P 协商后，**有且只有一个 Device 作为 GO 角色**，其他 P2P Device 扮演 Client 角色。
- 一个 P2P Group 中只有一个 GO，一个 GO 支持一个或多个 Client 连接。
- 由于 GO 功能类似 AP，所以周围**不支持 P2P 功能的 STA 也能发现并关联到 GO**，这些 STA 称为 **Legacy Client**。
- 图示为 `1:n P2P Group`：P2P Group Owner ↔ P2P Client、P2P Group Owner ↔ Legacy Client。

---

## 第 3 章 Wi-Fi API

### 3.1 头文件（P11）

- Wi-Fi API 头文件为 **`ql_wifi.h`** 和 **`ql_wifi_common.h`**，位于 SDK 包的 **`ql-sysroots/usr/include/ql-sdk/`** 目录下。
  - `ql_wifi.h`：定义 Wi-Fi 相关的 **API**；
  - `ql_wifi_common.h`：定义 Wi-Fi 相关的**数据结构**。
- 若无特别说明，本文档所涉及头文件均在该目录下。

### 3.2 函数概览（表 1，P11–P14）

完整函数清单（按文档表 1 顺序，分族整理）：

**全局控制族：**

| 函数 | 说明 |
|---|---|
| `ql_wifi_init()` | 初始化 Wi-Fi 服务 |
| `ql_wifi_deinit()` | 去初始化 Wi-Fi 服务 |
| `ql_wifi_enable()` | 开启 Wi-Fi 功能 |
| `ql_wifi_disable()` | 关闭 Wi-Fi 功能 |
| `ql_wifi_set_enable_status_ind_cb()` | 注册 Wi-Fi 状态回调函数 |
| `ql_wifi_work_mode_set()` | 设置 Wi-Fi 工作模式 |
| `ql_wifi_set_bridge()` | 设置当前 Wi-Fi 加入的网桥 |
| `ql_wifi_work_mode_get()` | 获取当前 Wi-Fi 工作模式 |

**AP 模式族：**

| 函数 | 说明 |
|---|---|
| `ql_wifi_ap_ssid_set()` | 设置 AP 模式的 SSID |
| `ql_wifi_ap_ssid_get()` | 获取 AP 模式的 SSID |
| `ql_wifi_ap_ssid_hidden_set()` | 设置 AP 模式的 SSID 隐藏状态 |
| `ql_wifi_ap_ssid_hidden_get()` | 获取 AP 模式的 SSID 隐藏状态 |
| `ql_wifi_ap_mode_set()` | 设置 AP 模式的工作协议模式 |
| `ql_wifi_ap_mode_get()` | 获取 AP 模式的工作协议模式 |
| `ql_wifi_ap_bandwidth_set()` | 设置 AP 模式的带宽 |
| `ql_wifi_ap_bandwidth_get()` | 获取 AP 模式的带宽 |
| `ql_wifi_ap_channel_set()` | 设置 AP 模式的信道 |
| `ql_wifi_ap_channel_get()` | 获取 AP 模式的信道 |
| `ql_wifi_ap_max_sta_num_set()` | 设置 AP 模式终端最大连接数量 |
| `ql_wifi_ap_max_sta_num_get()` | 获取 AP 模式终端最大连接数量 |
| `ql_wifi_ap_country_code_set()` | 设置 AP 模式的国家码 |
| `ql_wifi_ap_country_code_get()` | 获取 AP 模式的国家码 |
| `ql_wifi_ap_isolate_set()` | 设置 AP 隔离状态 |
| `ql_wifi_ap_isolate_get()` | 获取 AP 隔离状态 |
| `ql_wifi_ap_mac_acl_rule_set()` | 设置 AP 模式的 MAC 地址访问规则 |
| `ql_wifi_ap_mac_acl_rule_get()` | 获取 AP 模式的 MAC 地址访问规则 |
| `ql_wifi_ap_acl_mac_set()` | 为当前 MAC 地址访问规则添加或删除指定 MAC 地址 |
| `ql_wifi_ap_acl_mac_get()` | 获取当前 MAC 地址访问规则中所有的 MAC 地址 |
| `ql_wifi_ap_auth_set()` | 设置 AP 模式的安全认证 |
| `ql_wifi_ap_auth_get()` | 获取 AP 模式的安全认证信息 |
| `ql_wifi_ap_set_status_ind_cb()` | 注册 AP 模式状态回调函数 |
| `ql_wifi_ap_start()` | 开启 AP 功能 |
| `ql_wifi_ap_stop()` | 关闭 AP 功能 |
| `ql_wifi_ap_get_status()` | 获取 AP 模式的状态信息 |

**STA 模式族：**

| 函数 | 说明 |
|---|---|
| `ql_wifi_sta_ssid_set()` | 设置 STA 模式的 SSID |
| `ql_wifi_sta_ssid_get()` | 获取 STA 模式的 SSID |
| `ql_wifi_sta_auth_set()` | 设置 STA 模式的安全认证 |
| `ql_wifi_sta_auth_get()` | 获取 STA 模式的安全认证信息 |
| `ql_wifi_sta_set_status_ind_cb()` | 注册 STA 模式状态回调函数 |
| `ql_wifi_sta_start()` | 开启 STA 功能 |
| `ql_wifi_sta_stop()` | 关闭 STA 功能 |
| `ql_wifi_sta_get_status()` | 获取 STA 模式的状态信息 |
| `ql_wifi_sta_start_scan()` | STA 模式扫描外部热点 |
| `ql_wifi_sta_set_scan_result_ind_cb()` | 注册扫描结果回调函数 |

**通用回调 / 国家码族：**

| 函数 | 说明 |
|---|---|
| `ql_wifi_set_service_error_cb()` | 注册 Wi-Fi 服务异常回调函数 |
| `ql_wifi_set_ap_sta_connect_ind_cb()` | 注册 AP 模式和 STA 设备连接状态回调函数 |
| `ql_wifi_country_code_set()` | 设置 Wi-Fi 国家码 |
| `ql_wifi_country_code_get()` | 获取 Wi-Fi 国家码 |

**P2P 族：**

| 函数 | 说明 |
|---|---|
| `ql_wifi_p2p_dev_name_set()` | 设置 Wi-Fi P2P 设备名称 |
| `ql_wifi_p2p_dev_name_get()` | 获取 Wi-Fi P2P 设备名称 |
| `ql_wifi_p2p_dev_type_set()` | 设置 Wi-Fi P2P 设备类型 |
| `ql_wifi_p2p_dev_type_get()` | 获取 Wi-Fi P2P 设备类型 |
| `ql_wifi_p2p_oper_class_channel_set()` | 设置 Wi-Fi P2P 设备的工作频率 |
| `ql_wifi_p2p_oper_class_channel_get()` | 获取 Wi-Fi P2P 设备的工作频率 |
| `ql_wifi_p2p_ssid_postfix_set()` | 设置 Wi-Fi P2P 模式的 SSID 后缀名 |
| `ql_wifi_p2p_ssid_postfix_get()` | 获取 Wi-Fi P2P 模式的 SSID 后缀名 |
| `ql_wifi_p2p_set_enable_status_ind_cb()` | 注册 Wi-Fi P2P 模式的启用状态回调函数 |
| `ql_wifi_p2p_enable()` | 启用 Wi-Fi P2P 功能 |
| `ql_wifi_p2p_disable()` | 禁用 Wi-Fi P2P 功能 |
| `ql_wifi_p2p_set_dev_found_ind_cb()` | 注册 P2P 设备扫描信息回调函数 |
| `ql_wifi_p2p_dev_find_start()` | 开始扫描周围的 P2P 设备 |
| `ql_wifi_p2p_dev_find_stop()` | 停止扫描周围的 P2P 设备 |
| `ql_wifi_p2p_get_status()` | 获取 Wi-Fi P2P 模式的连接状态信息 |
| `ql_wifi_p2p_set_status_ind_cb()` | 注册 Wi-Fi P2P 模式连接状态回调函数 |
| `ql_wifi_p2p_set_peer_dev_req_ind_cb()` | 注册对端 P2P 设备连接请求回调函数 |
| `ql_wifi_p2p_connect()` | 连接对端 P2P 设备 |
| `ql_wifi_p2p_disconnect()` | 断开对端 P2P 设备 |

**表 1 备注（关键全局约束，务必牢记）：**

1. 若无特别说明，**不可在任何回调函数中调用上述 Wi-Fi API**，否则会对后续消息的处理造成影响。
2. 调用 `ql_wifi_ap_start()` 或 `ql_wifi_sta_start()` **之前**，须调用 `ql_wifi_work_mode_set()` 设置正确的 Wi-Fi 工作模式；若没有预先设置 Wi-Fi 工作模式，则**默认工作模式为单 AP 模式**。
3. 调用 `ql_wifi_ap_start()` 或 `ql_wifi_sta_start()` 之前，须调用 `ql_wifi_enable()` 启用 Wi-Fi。
4. 调用 `ql_wifi_ap_start()` 或 `ql_wifi_sta_start()` 之前，用户可根据自身需求，调用其它配置接口进行相关配置。

> 解读：标准调用序列为 `ql_wifi_init()` → 各类 `set` 配置 → 注册回调 → `ql_wifi_work_mode_set()` → `ql_wifi_enable()` → `ql_wifi_ap_start()/sta_start()`。

### 3.3 函数详解

#### 3.3.1 ql_wifi_init（P14）

初始化 Wi-Fi 服务。**使用其它任何 Wi-Fi API 之前，必须先调用本函数。**

- **原型**：`int ql_wifi_init(void);`
- **参数**：无
- **返回值**：
  - `QL_ERR_OK`：执行成功
  - `QL_ERR_SERVICE_NOT_READY`：执行失败，Wi-Fi 服务未准备就绪，**建议重试**
  - 其他值：执行失败，错误码详见 `ql_type.h`

#### 3.3.2 ql_wifi_deinit（P15）

去初始化 Wi-Fi 服务。

- **原型**：`int ql_wifi_deinit(void);`
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功；其他值失败（错误码见 `ql_type.h`）

#### 3.3.3 ql_wifi_enable（P15）

开启 Wi-Fi 功能。

- **原型**：`int ql_wifi_enable(void);`
- **参数**：无
- **返回值**：
  - `QL_ERR_OK`：成功
  - `QL_ERR_NOT_INIT`：失败，未初始化 Wi-Fi 服务
  - `QL_ERR_SERVICE_ABORT`：失败，Wi-Fi 服务出错
  - 其他值：错误码见 `ql_type.h`

#### 3.3.4 ql_wifi_disable（P15–P16）

关闭 Wi-Fi 功能。

- **原型**：`int ql_wifi_disable(void);`
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功；`QL_ERR_NOT_INIT`（未初始化）；`QL_ERR_SERVICE_ABORT`（服务出错）；其他错误码见 `ql_type.h`

#### 3.3.5 ql_wifi_set_enable_status_ind_cb（P16）

注册 Wi-Fi 状态回调函数。

- **原型**：`int ql_wifi_set_enable_status_ind_cb(ql_wifi_enable_status_ind_cb_f cb);`
- **参数**：`cb` [In] Wi-Fi 状态回调函数（见 3.3.5.1）
- **返回值**：`QL_ERR_OK`；`QL_ERR_NOT_INIT`；`QL_ERR_SERVICE_ABORT`；其他见 `ql_type.h`

##### 3.3.5.1 ql_wifi_enable_status_ind_cb_f（P16–P17）

回调函数类型，用于处理 Wi-Fi 状态。

- **原型**：
```c
typedef void (*ql_wifi_enable_status_ind_cb_f)(QL_WIFI_ENABLE_STATUS_E pre_status,
                                               QL_WIFI_ENABLE_STATUS_E status);
```
- **参数**：
  - `pre_status` [In]：先前的 Wi-Fi 状态（见 3.3.5.2）
  - `status` [In]：当前的 Wi-Fi 状态（见 3.3.5.2）
- **返回值**：无

##### 3.3.5.2 QL_WIFI_ENABLE_STATUS_E（P17）

Wi-Fi 状态枚举：

```c
typedef enum QL_WIFI_STATUS_ENUM
{
    QL_WIFI_STATUS_DISABLED = 0,
    QL_WIFI_STATUS_ENABLED,
    QL_WIFI_STATUS_ERROR
} QL_WIFI_ENABLE_STATUS_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_WIFI_STATUS_DISABLED` | 0 | 已禁用 Wi-Fi |
| `QL_WIFI_STATUS_ENABLED` | 1 | 已启用 Wi-Fi |
| `QL_WIFI_STATUS_ERROR` | 2 | 启用 Wi-Fi 出错 |

#### 3.3.6 ql_wifi_work_mode_set（P17–P18）

设置 Wi-Fi 工作模式。

- **原型**：`int ql_wifi_work_mode_set(QL_WIFI_WORK_MODE_E mode);`
- **参数**：`mode` [In] Wi-Fi 工作模式（见 3.3.6.1）
- **返回值**：`QL_ERR_OK`；`QL_ERR_NOT_INIT`；`QL_ERR_SERVICE_ABORT`；`QL_ERR_INVALID_ARG`（无效参数）；其他见 `ql_type.h`

##### 3.3.6.1 QL_WIFI_WORK_MODE_E（P18）

Wi-Fi 工作模式枚举：

```c
typedef enum QL_WIFI_WORK_MODE_ENUM
{
    QL_WIFI_WORK_MODE_MIN = -1,
    QL_WIFI_WORK_MODE_STA = 0,
    QL_WIFI_WORK_MODE_AP0,
    QL_WIFI_WORK_MODE_MAX
} QL_WIFI_WORK_MODE_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_WIFI_WORK_MODE_STA` | 0 | STA 模式 |
| `QL_WIFI_WORK_MODE_AP0` | 1 | 单 AP 模式 |

> `MIN=-1` / `MAX` 为边界哨兵值，不作为有效模式。仅 STA 与单 AP 两种工作模式 —— 印证 FC30R 单 AP 限制。

#### 3.3.7 ql_wifi_set_bridge（P18）

设置当前 Wi-Fi 加入的网桥。

- **原型**：`int ql_wifi_set_bridge(char *bridge);`
- **参数**：`bridge` [In] 需加入的网桥名称，**若不设置则默认使用网桥 bridge0**。
- **返回值**：（见下页）`QL_ERR_OK`；`QL_ERR_NOT_INIT`；`QL_ERR_SERVICE_ABORT`；`QL_ERR_INVALID_ARG`；其他见 `ql_type.h`

#### 3.3.8 ql_wifi_work_mode_get（P19）

获取当前 Wi-Fi 工作模式。

- **原型**：`int ql_wifi_work_mode_get(QL_WIFI_WORK_MODE_E *p_mode);`
- **参数**：`p_mode` [Out] 当前的 Wi-Fi 工作模式（见 3.3.6.1）
- **返回值**：`QL_ERR_OK`；`QL_ERR_NOT_INIT`；`QL_ERR_SERVICE_ABORT`；`QL_ERR_INVALID_ARG`；其他见 `ql_type.h`

#### 3.3.9 ql_wifi_ap_ssid_set（P19–P20）

设置 AP 模式的 SSID。

- **原型**：`int ql_wifi_ap_ssid_set(QL_WIFI_AP_INDEX_E idx, const char *ssid);`
- **参数**：
  - `idx` [In] AP 模式索引（见 3.3.9.1）
  - `ssid` [In] SSID。**SSID 不可超过 32 字节，且不为空。**
- **返回值**：`QL_ERR_OK`；`QL_ERR_NOT_INIT`；`QL_ERR_SERVICE_ABORT`；`QL_ERR_INVALID_ARG`；其他见 `ql_type.h`

##### 3.3.9.1 QL_WIFI_AP_INDEX_E（P20）

AP 模式索引枚举：

```c
typedef enum QL_WIFI_INDEX_ENUM
{
    QL_WIFI_AP_INDEX_MIN = -1,
    QL_WIFI_AP_INDEX_AP0 = 0,
    QL_WIFI_AP_INDEX_MAX
} QL_WIFI_AP_INDEX_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_WIFI_AP_INDEX_AP0` | 0 | 热点 0 的索引 |

> 解读：仅一个有效索引 AP0，对应"仅开放一个热点"。后续所有 `ql_wifi_ap_*` 接口的 `idx` 参数都填 `QL_WIFI_AP_INDEX_AP0`。

#### 3.3.10 ql_wifi_ap_ssid_get（P20）

获取 AP 模式的 SSID。

- **原型**：`int ql_wifi_ap_ssid_get(QL_WIFI_AP_INDEX_E idx, char *ssid_buf, int buf_len);`
- **参数**：
  - `idx` [In] AP 模式索引
  - `ssid_buf` [Out] 指针，指向存放当前 SSID 的缓存
  - `buf_len` [In] 缓存大小。**建议设置为 33 字节**（32 字节 SSID + 1 字节结束符）
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / `QL_ERR_INVALID_ARG` / 其他

#### 3.3.11 ql_wifi_ap_ssid_hidden_set（P21）

设置 AP 模式的 SSID 隐藏状态。

- **原型**：`int ql_wifi_ap_ssid_hidden_set(QL_WIFI_AP_INDEX_E idx, int ssid_hidden);`
- **参数**：
  - `idx` [In] AP 模式索引
  - `ssid_hidden` [In] SSID 隐藏状态：`0`=不隐藏 SSID，`1`=隐藏 SSID
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / `QL_ERR_INVALID_ARG` / 其他

#### 3.3.12 ql_wifi_ap_ssid_hidden_get（P21–P22）

获取 AP 模式的 SSID 隐藏状态。

- **原型**：`int ql_wifi_ap_ssid_hidden_get(QL_WIFI_AP_INDEX_E idx, int *p_ssid_hidden);`
- **参数**：`idx` [In] AP 模式索引；`p_ssid_hidden` [Out] SSID 隐藏状态（`0`=不隐藏，`1`=隐藏）
- **返回值**：同上四类错误码

#### 3.3.13 ql_wifi_ap_mode_set（P22）

设置 AP 模式的工作协议模式（802.11 协议类型）。

- **原型**：`int ql_wifi_ap_mode_set(QL_WIFI_AP_INDEX_E idx, QL_WIFI_AP_MODE_TYPE_E mode);`
- **参数**：`idx` [In] AP 模式索引；`mode` [In] 工作协议模式（见 3.3.13.1）
- **返回值**：同上四类错误码

##### 3.3.13.1 QL_WIFI_AP_MODE_TYPE_E（P23）

AP 工作协议模式枚举：

```c
typedef enum QL_WIFI_AP_MODE_TYPE_ENUM
{
    QL_WIFI_AP_MODE_MIN = -1,
    QL_WIFI_AP_MODE_80211B = 0,
    QL_WIFI_AP_MODE_80211BG,
    QL_WIFI_AP_MODE_80211BGN,
    QL_WIFI_AP_MODE_80211AX_2G,
    QL_WIFI_AP_MODE_MAX
} QL_WIFI_AP_MODE_TYPE_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_WIFI_AP_MODE_80211B` | 0 | IEEE 802.11b（2.4 GHz） |
| `QL_WIFI_AP_MODE_80211BG` | 1 | IEEE 802.11b/g（2.4 GHz） |
| `QL_WIFI_AP_MODE_80211BGN` | 2 | IEEE 802.11b/g/n（2.4 GHz） |
| `QL_WIFI_AP_MODE_80211AX_2G` | 3 | IEEE 802.11ax（2.4 GHz，即 Wi-Fi 6） |

> 解读：所有协议模式均限于 2.4 GHz，与 FC30R 单频段一致。常规选 `80211BGN` 兼容性最佳；`80211AX_2G` 为 Wi-Fi 6。

#### 3.3.14 ql_wifi_ap_mode_get（P23–P24）

获取 AP 模式的工作协议模式。

- **原型**：`int ql_wifi_ap_mode_get(QL_WIFI_AP_INDEX_E idx, QL_WIFI_AP_MODE_TYPE_E *p_mode);`
- **参数**：`idx` [In]；`p_mode` [Out] 当前的协议模式
- **返回值**：四类错误码

#### 3.3.15 ql_wifi_ap_bandwidth_set（P24）

设置 AP 模式的带宽。

- **原型**：
```c
int ql_wifi_ap_bandwidth_set(QL_WIFI_AP_INDEX_E idx,
                             QL_WIFI_BANDWIDTH_E bandwidth);
```
- **参数**：`idx` [In]；`bandwidth` [In] AP 模式的带宽（见 3.3.15.1）
- **返回值**：四类错误码

##### 3.3.15.1 QL_WIFI_BANDWIDTH_E（P24–P25）

带宽类型枚举：

```c
typedef enum QL_WIFI_BANDWIDTH_ENUM
{
    QL_WIFI_BANDWIDTH_MIN = -1,
    QL_WIFI_BANDWIDTH_20MHZ = 0,
    QL_WIFI_BANDWIDTH_40MHZ,
    QL_WIFI_BANDWIDTH_80MHZ,
    QL_WIFI_BANDWIDTH_160MHZ,
    QL_WIFI_BANDWIDTH_MAX
} QL_WIFI_BANDWIDTH_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_WIFI_BANDWIDTH_20MHZ` | 0 | 20 MHz |
| `QL_WIFI_BANDWIDTH_40MHZ` | 1 | 40 MHz |
| `QL_WIFI_BANDWIDTH_80MHZ` | 2 | 80 MHz |
| `QL_WIFI_BANDWIDTH_160MHZ` | 3 | 160 MHz |

> 解读：枚举虽列出 80/160 MHz，但 2.4 GHz 实际仅常用 20/40 MHz；80/160 MHz 通常属 5 GHz，对 FC30R（仅 2.4 GHz）的可用性需以实测为准。

#### 3.3.16 ql_wifi_ap_bandwidth_get（P25）

获取 AP 模式的带宽。

- **原型**：
```c
int ql_wifi_ap_bandwidth_get(QL_WIFI_AP_INDEX_E idx,
                             QL_WIFI_BANDWIDTH_E *p_bandwidth);
```
- **参数**：`idx` [In]；`p_bandwidth` [Out] 当前 AP 带宽
- **返回值**：四类错误码

#### 3.3.17 ql_wifi_ap_channel_set（P25–P26）

设置 AP 模式的信道。

- **原型**：`int ql_wifi_ap_channel_set(QL_WIFI_AP_INDEX_E idx, int channel);`
- **参数**：
  - `idx` [In] AP 模式索引
  - `channel` [In] 信道（2.4 GHz）：
    - `0` = **自适应**（自动选信道）
    - `1/2/3/4/5/6/7/8/9/10/11/12/13/14` = 对应 2.4 GHz 信道
- **返回值**：四类错误码

#### 3.3.18 ql_wifi_ap_channel_get（P26–P27）

获取 AP 模式的信道。

- **原型**：`int ql_wifi_ap_channel_get(QL_WIFI_AP_INDEX_E idx, int *p_channel);`
- **参数**：`idx` [In]；`p_channel` [Out] AP 当前使用的信道（`0`=自适应，`1~14`=2.4 GHz 信道）
- **返回值**：四类错误码

#### 3.3.19 ql_wifi_ap_max_sta_num_set（P27）

设置 AP 模式终端最大连接数量。

- **原型**：`int ql_wifi_ap_max_sta_num_set(QL_WIFI_AP_INDEX_E idx, int max_sta_num);`
- **参数**：`idx` [In]；`max_sta_num` [In] AP 模式终端最大连接数量，**取值范围 1~32**
- **返回值**：四类错误码

#### 3.3.20 ql_wifi_ap_max_sta_num_get（P27–P28）

获取 AP 模式终端最大连接数量。

- **原型**：`int ql_wifi_ap_max_sta_num_get(QL_WIFI_AP_INDEX_E idx, int *p_max_sta_num);`
- **参数**：`idx` [In]；`p_max_sta_num` [Out] 当前 AP 终端最大连接数量
- **返回值**：四类错误码

#### 3.3.21 ql_wifi_ap_country_code_set（P28）

设置 AP 模式的国家码。

- **原型**：`int ql_wifi_ap_country_code_set(QL_WIFI_AP_INDEX_E idx, const char *country_code);`
- **参数**：
  - `idx` [In] AP 模式索引
  - `country_code` [In] 国家码。**默认值 CN（表示中国）；最大长度 2 个字节；不可为空。**
- **返回值**：四类错误码

> 解读：国家码决定可用信道与发射功率法规约束（例如 CN 仅 1~13 信道、JP 含 14 信道）。

#### 3.3.22 ql_wifi_ap_country_code_get（P28–P29）

获取 AP 模式的国家码。

- **原型**：
```c
int ql_wifi_ap_country_code_get(QL_WIFI_AP_INDEX_E idx,
                                char *country_code_buf,
                                int buf_len);
```
- **参数**：`idx` [In]；`country_code_buf` [Out] 指向存放当前国家码的缓存；`buf_len` [In] 缓存大小，**建议设置为 3 字节**（2 字节国家码 + 1 字节结束符）
- **返回值**：四类错误码

#### 3.3.23 ql_wifi_ap_isolate_set（P29）

设置 AP 隔离状态（AP isolation，控制接入同一 AP 的终端之间能否互访）。

- **原型**：`int ql_wifi_ap_isolate_set(QL_WIFI_AP_INDEX_E idx, int isolate);`
- **参数**：`idx` [In]；`isolate` [In] AP 隔离状态：`0`=AP 不隔离，`1`=AP 隔离
- **返回值**：四类错误码

> 解读：开启隔离后，连接到同一热点的客户端之间二层互通被禁止（仅能访问网关/外网），常用于公共热点安全场景。

#### 3.3.24 ql_wifi_ap_isolate_get（P30）

获取 AP 隔离状态。

- **原型**：`int ql_wifi_ap_isolate_get(QL_WIFI_AP_INDEX_E idx, int *p_isolate);`
- **参数**：`idx` [In]；`p_isolate` [Out] AP 当前的隔离状态（`0`=不隔离，`1`=隔离）
- **返回值**：四类错误码

#### 3.3.25 ql_wifi_ap_mac_acl_rule_set（P30–P31）

设置 AP 模式的 MAC 地址访问规则（ACL）。

- **原型**：
```c
int ql_wifi_ap_mac_acl_rule_set(QL_WIFI_AP_INDEX_E idx,
                                QL_WIFI_MAC_ACL_RULE_E acl_rule);
```
- **参数**：`idx` [In]；`acl_rule` [In] MAC 地址访问规则（见 3.3.25.1）
- **返回值**：四类错误码

##### 3.3.25.1 QL_WIFI_MAC_ACL_RULE_E（P31）

MAC 地址访问规则枚举：

```c
typedef enum QL_WIFI_MAC_RULE_ENUM
{
    QL_WIFI_MAC_ACL_RULE_MIN = -1,
    QL_WIFI_MAC_ACL_RULE_NONE = 0,
    QL_WIFI_MAC_ACL_RULE_BLACK,
    QL_WIFI_MAC_ACL_RULE_WHITE,
    QL_WIFI_MAC_ACL_RULE_MAX
} QL_WIFI_MAC_ACL_RULE_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_WIFI_MAC_ACL_RULE_NONE` | 0 | 无规则（不做 MAC 过滤） |
| `QL_WIFI_MAC_ACL_RULE_BLACK` | 1 | 黑名单（名单内 MAC 禁止接入） |
| `QL_WIFI_MAC_ACL_RULE_WHITE` | 2 | 白名单（仅名单内 MAC 允许接入） |

#### 3.3.26 ql_wifi_ap_mac_acl_rule_get（P31–P32）

获取 AP 模式的 MAC 地址访问规则。

- **原型**：
```c
int ql_wifi_ap_mac_acl_rule_get(QL_WIFI_AP_INDEX_E idx,
                                QL_WIFI_MAC_ACL_RULE_E *p_acl_rule);
```
- **参数**：`idx` [In]；`p_acl_rule` [Out] 当前的 MAC 地址访问规则
- **返回值**：四类错误码

#### 3.3.27 ql_wifi_ap_acl_mac_set（P32–P33）

为当前 MAC 地址访问规则**添加或删除**指定的 MAC 地址。

- **原型**：
```c
int ql_wifi_ap_acl_mac_set(QL_WIFI_AP_INDEX_E idx,
                           QL_WIFI_ACL_MAC_CMD_E cmd,
                           const char *macaddr);
```
- **参数**：
  - `idx` [In] AP 模式索引
  - `cmd` [In] MAC 地址操作类型（见 3.3.27.1）
  - `macaddr` [In] MAC 地址
- **返回值**：四类错误码

##### 3.3.27.1 QL_WIFI_ACL_MAC_CMD_E（P32–P33）

MAC 地址操作类型枚举：

```c
typedef enum
{
    QL_WIFI_ACL_MAC_CMD_DEL = 0,
    QL_WIFI_ACL_MAC_CMD_ADD
} QL_WIFI_ACL_MAC_CMD_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_WIFI_ACL_MAC_CMD_DEL` | 0 | 删除 MAC 地址 |
| `QL_WIFI_ACL_MAC_CMD_ADD` | 1 | 添加 MAC 地址 |

#### 3.3.28 ql_wifi_ap_acl_mac_get（P33）

获取当前 MAC 地址访问规则中所有的 MAC 地址。

- **原型**：`int ql_wifi_ap_acl_mac_get(QL_WIFI_AP_INDEX_E idx, ql_wifi_acl_mac_list_t *p_mac_list);`
- **参数**：`idx` [In]；`p_mac_list` [Out] MAC 地址列表（见 3.3.28.1）
- **返回值**：四类错误码

##### 3.3.28.1 ql_wifi_acl_mac_list_t（P33–P34）

MAC 地址访问规则中的 MAC 地址列表结构体：

```c
typedef struct
{
    int cnt;
    ql_wifi_mac_addr_t addr[QL_WIFI_MAX_ACL_MAC_CNT];
} ql_wifi_acl_mac_list_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `int` | `cnt` | MAC 地址个数，**最大不超过 32** |
| `ql_wifi_mac_addr_t` | `addr` | MAC 地址数组（见 3.3.28.2）。`QL_WIFI_MAX_ACL_MAC_CNT` 定义在 `ql_wifi_common.h` 中，**值为 32** |

##### 3.3.28.2 ql_wifi_mac_addr_t（P34）

MAC 地址结构体：

```c
typedef struct
{
    char macaddr[18];
} ql_wifi_mac_addr_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `char` | `macaddr` | MAC 地址，格式 `%02X:%02X:%02X:%02X:%02X:%02X`，例如 `2F:3F:4F:5F:6F:7F`（17 字符 + 结束符 = 18 字节）|

#### 3.3.29 ql_wifi_ap_auth_set（P34–P35）

设置 AP 模式的安全认证。

- **原型**：`int ql_wifi_ap_auth_set(QL_WIFI_AP_INDEX_E idx, ql_wifi_ap_auth_t *p_auth);`
- **参数**：`idx` [In]；`p_auth` [In] 安全认证信息（见 3.3.29.1）
- **返回值**：四类错误码

##### 3.3.29.1 ql_wifi_ap_auth_t（P35–P36）

AP 模式的安全认证结构体（含 WEP 与 WPA-PSK 两种 union 分支）：

```c
typedef struct
{
    QL_WIFI_AUTH_E auth;
    union
    {
        struct
        {
            int default_index;
            char passwd[4][64];
        } wep;
        struct
        {
            QL_WIFI_AUTH_WPA_PSK_E pairwise;
            char passwd[64];
            int group_rekey;
        } wpa_psk;
    } info;
} ql_wifi_ap_auth_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_WIFI_AUTH_E` | `auth` | 安全认证类型（见 3.3.29.2） |
| `int` | `default_index` | WEP 的配置索引（WEP 支持 4 组密钥，指定默认使用第几组） |
| `char` | `passwd[4][64]` | WEP 的密码（4 组，每组最长 64 字节） |
| `QL_WIFI_AUTH_WPA_PSK_E` | `pairwise` | 加密方式（见 3.3.29.3） |
| `char` | `passwd[64]` | WPA_PSK 的密码 |
| `int` | `group_rekey` | 组密钥更新周期 |

> 解读：`auth` 选 `OPEN` 时 union 不使用；选 `WEP` 用 `info.wep`；选 WPA/WPA2/WPA3 系列用 `info.wpa_psk`。WPA-PSK 密码长度通常 8~63 字符。

##### 3.3.29.2 QL_WIFI_AUTH_E（P36）

安全认证类型枚举：

```c
typedef enum QL_WIFI_AUTH_ENUM
{
    QL_WIFI_AUTH_MIN = -1,
    QL_WIFI_AUTH_OPEN = 0,
    QL_WIFI_AUTH_WEP,
    QL_WIFI_AUTH_WPA_PSK,
    QL_WIFI_AUTH_WPA2_PSK,
    QL_WIFI_AUTH_WPA3_PSK,
    QL_WIFI_AUTH_WPA_WPA2_PSK_BOTH,
    QL_WIFI_AUTH_WPA2_WPA3_PSK_BOTH,
    QL_WIFI_AUTH_WPA_WPA2_WPA3_PSK_ALL,
    QL_WIFI_AUTH_MAX
} QL_WIFI_AUTH_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_WIFI_AUTH_OPEN` | 0 | OPEN（开放，无加密） |
| `QL_WIFI_AUTH_WEP` | 1 | WEP |
| `QL_WIFI_AUTH_WPA_PSK` | 2 | WPA 个人版 |
| `QL_WIFI_AUTH_WPA2_PSK` | 3 | WPA2 个人版 |
| `QL_WIFI_AUTH_WPA3_PSK` | 4 | WPA3 个人版 |
| `QL_WIFI_AUTH_WPA_WPA2_PSK_BOTH` | 5 | WPA&WPA2 个人版（混合） |
| `QL_WIFI_AUTH_WPA2_WPA3_PSK_BOTH` | 6 | WPA2&WPA3 个人版（混合） |
| `QL_WIFI_AUTH_WPA_WPA2_WPA3_PSK_ALL` | 7 | WPA&WPA2&WPA3 个人版（全兼容） |

##### 3.3.29.3 QL_WIFI_AUTH_WPA_PSK_E（P37）

加密方式（pairwise cipher）枚举：

```c
typedef enum QL_WIFI_AUTH_WPA_PSK_ENUM
{
    QL_WIFI_AUTH_WPA_PAIRWISE_MIN = -1,
    QL_WIFI_AUTH_WPA_PAIRWISE_AUTO = 0,
    QL_WIFI_AUTH_WPA_PAIRWISE_TKIP,
    QL_WIFI_AUTH_WPA_PAIRWISE_AES,
    QL_WIFI_AUTH_WPA_PAIRWISE_MAX
} QL_WIFI_AUTH_WPA_PSK_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_WIFI_AUTH_WPA_PAIRWISE_AUTO` | 0 | 自动模式（包含 TKIP 和 AES） |
| `QL_WIFI_AUTH_WPA_PAIRWISE_TKIP` | 1 | TKIP |
| `QL_WIFI_AUTH_WPA_PAIRWISE_AES` | 2 | AES |

**备注（重要兼容性建议，P37）**：要使 Wi-Fi 设备连接时达到最大兼容性，建议在 **AP 工作模式下配置加密方式为 AES**；在 **STA 工作模式下配置加密方式为自动模式**。

#### 3.3.30 ql_wifi_ap_auth_get（P37–P38）

获取 AP 模式的安全认证信息。

- **原型**：`int ql_wifi_ap_auth_get(QL_WIFI_AP_INDEX_E idx, ql_wifi_ap_auth_t *p_auth);`
- **参数**：`idx` [In]；`p_auth` [Out] 安全认证信息（见 3.3.29.1）
- **返回值**：四类错误码

#### 3.3.31 ql_wifi_ap_set_status_ind_cb（P38）

注册 AP 模式状态回调函数。

- **原型**：`int ql_wifi_ap_set_status_ind_cb(ql_wifi_ap_status_ind_cb_f cb);`
- **参数**：`cb` [In] AP 模式状态回调函数（见 3.3.31.1）
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

##### 3.3.31.1 ql_wifi_ap_status_ind_cb_f（P38–P39）

AP 模式状态回调函数类型：

```c
typedef void (*ql_wifi_ap_status_ind_cb_f)(QL_WIFI_AP_INDEX_E index,
                                           QL_WIFI_AP_STATUS_E pre_status,
                                           ql_wifi_ap_status_t *p_msg);
```
- **参数**：
  - `index` [In] AP 模式索引
  - `pre_status` [In] 先前的 AP 模式状态（见 3.3.31.2）
  - `p_msg` [In] 当前的 AP 模式状态等信息（见 3.3.31.3）
- **返回值**：无

##### 3.3.31.2 QL_WIFI_AP_STATUS_E（P39）

AP 模式状态枚举：

```c
typedef enum QL_WIFI_AP_STATUS_ENUM
{
    QL_WIFI_AP_STATUS_NONE,
    QL_WIFI_AP_STATUS_IDLE,
    QL_WIFI_AP_STATUS_ENABLING,
    QL_WIFI_AP_STATUS_ENABLED,
    QL_WIFI_AP_STATUS_DISABLING,
    QL_WIFI_AP_STATUS_ERROR
} QL_WIFI_AP_STATUS_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_WIFI_AP_STATUS_NONE` | 0 | 未设置相应的 Wi-Fi 工作模式 |
| `QL_WIFI_AP_STATUS_IDLE` | 1 | 设置了相应的 Wi-Fi 工作模式，但未开启 AP 功能 |
| `QL_WIFI_AP_STATUS_ENABLING` | 2 | AP 模式启用中（**暂不支持**） |
| `QL_WIFI_AP_STATUS_ENABLED` | 3 | AP 模式已启用 |
| `QL_WIFI_AP_STATUS_DISABLING` | 4 | AP 模式禁用中（**暂不支持**） |
| `QL_WIFI_AP_STATUS_ERROR` | 5 | 出错状态 |

> 解读：ENABLING/DISABLING 标注"暂不支持"，意味着状态机实际只会上报 NONE/IDLE/ENABLED/ERROR 这几个稳态。

##### 3.3.31.3 ql_wifi_ap_status_t（P40）

AP 模式的状态信息结构体：

```c
typedef struct ql_wifi_ind_ap_status_struct
{
    QL_WIFI_AP_STATUS_E status;
    char ifname[32];
    char bssid[18];
} ql_wifi_ap_status_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_WIFI_AP_STATUS_E` | `status` | AP 模式的状态（见 3.3.31.2） |
| `char` | `ifname` | 接口名，例如 `wlan0`、`wlan1` 等 |
| `char` | `bssid` | BSSID，基本服务集标识符（AP 自身 MAC）|

#### 3.3.32 ql_wifi_ap_start（P40）

开启 AP 功能。

- **原型**：`int ql_wifi_ap_start(QL_WIFI_AP_INDEX_E index);`
- **参数**：`index` [In] AP 模式索引
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / `QL_ERR_INVALID_ARG` / 其他

#### 3.3.33 ql_wifi_ap_stop（P41）

关闭 AP 功能。

- **原型**：`int ql_wifi_ap_stop(QL_WIFI_AP_INDEX_E index);`
- **参数**：`index` [In] AP 模式索引
- **返回值**：四类错误码

#### 3.3.34 ql_wifi_ap_get_status（P41）

获取 AP 模式的状态信息。

- **原型**：`int ql_wifi_ap_get_status(QL_WIFI_AP_INDEX_E index, ql_wifi_ap_status_t *p_sta);`
- **参数**：`index` [In]；`p_sta` [Out] AP 模式的状态信息（见 3.3.31.3）
- **返回值**：四类错误码

#### 3.3.35 ql_wifi_sta_ssid_set（P42）

设置 STA 模式的 SSID（要连接的目标热点 SSID）。

- **原型**：`int ql_wifi_sta_ssid_set(const char *ssid);`
- **参数**：`ssid` [In] SSID。**不可超过 32 字节，且不为空。**
- **返回值**：四类错误码

> 注意：STA 接口**不带 `idx` 参数**（STA 只有一个实例），区别于 AP 族。

#### 3.3.36 ql_wifi_sta_ssid_get（P42）

获取 STA 模式的 SSID。

- **原型**：`int ql_wifi_sta_ssid_get(char *ssid_buf, int buf_len);`
- **参数**：`ssid_buf` [Out] 指向存放当前 SSID 的缓存；`buf_len` [In] 缓存大小
- **返回值**：四类错误码

#### 3.3.37 ql_wifi_sta_auth_set（P43）

设置 STA 模式的安全认证（连接目标热点所用的认证/密码）。

- **原型**：`int ql_wifi_sta_auth_set(ql_wifi_sta_auth_t *p_auth);`
- **参数**：`p_auth` [In] 安全认证信息（见 3.3.37.1）
- **返回值**：四类错误码

##### 3.3.37.1 ql_wifi_sta_auth_t（P43–P44）

STA 模式的安全认证结构体（相比 AP 版，WEP 分支无 `default_index`，且密码为单组）：

```c
typedef struct
{
    QL_WIFI_AUTH_E auth;
    union
    {
        struct
        {
            char passwd[64];
        } wep;
        struct
        {
            QL_WIFI_AUTH_WPA_PSK_E pairwise;
            char passwd[64];
        } wpa_psk;
    } info;
} ql_wifi_sta_auth_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_WIFI_AUTH_E` | `auth` | 安全认证类型（见 3.3.29.2） |
| `char` | `passwd`（wep） | WEP 的密码 |
| `QL_WIFI_AUTH_WPA_PSK_E` | `pairwise` | 加密方式（见 3.3.29.3） |
| `char` | `passwd`（wpa_psk） | WPA_PSK 的密码 |

> 解读：与 AP 版（3.3.29.1）相比，STA 版 WEP 不含 `default_index` 与 4 组密钥、wpa_psk 不含 `group_rekey`。结合 3.3.29.3 备注：STA 侧 `pairwise` 建议设为 AUTO 以最大兼容。

#### 3.3.38 ql_wifi_sta_auth_get（P44）

获取 STA 模式的安全认证信息。

- **原型**：`int ql_wifi_sta_auth_get(ql_wifi_sta_auth_t *p_auth);`
- **参数**：`p_auth` [Out] 当前的安全认证信息
- **返回值**：四类错误码

#### 3.3.39 ql_wifi_sta_set_status_ind_cb（P44–P45）

注册 STA 模式状态回调函数。

- **原型**：`int ql_wifi_sta_set_status_ind_cb(ql_wifi_sta_status_ind_cb_f cb);`
- **参数**：`cb` [In] STA 模式状态回调函数（见 3.3.39.1）
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

##### 3.3.39.1 ql_wifi_sta_status_ind_cb_f（P45）

STA 模式状态回调函数类型：

```c
typedef void (*ql_wifi_sta_status_ind_cb_f)(QL_WIFI_STA_STATUS_E pre_status,
                                             ql_wifi_sta_status_t *p_msg);
```
- **参数**：`pre_status` [In] 先前的 STA 模式状态（见 3.3.39.2）；`p_msg` [In] 当前的 STA 模式状态等信息（见 3.3.39.3）
- **返回值**：无

##### 3.3.39.2 QL_WIFI_STA_STATUS_E（P45–P46）

STA 模式状态枚举：

```c
typedef enum QL_WIFI_STA_STATUS_ENUM
{
    QL_WIFI_STA_STATUS_NONE,
    QL_WIFI_STA_STATUS_IDLE,
    QL_WIFI_STA_STATUS_CONNECTING,
    QL_WIFI_STA_STATUS_ASSOCIATED,
    QL_WIFI_STA_STATUS_CONNECTED,
    QL_WIFI_STA_STATUS_DISCONNECTED,
    QL_WIFI_STA_STATUS_ERROR
} QL_WIFI_STA_STATUS_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_WIFI_STA_STATUS_NONE` | 0 | 未设置相应的 Wi-Fi 工作模式 |
| `QL_WIFI_STA_STATUS_IDLE` | 1 | 设置了相应的 Wi-Fi 工作模式，但未开启 STA 功能 |
| `QL_WIFI_STA_STATUS_CONNECTING` | 2 | 正在连接 AP |
| `QL_WIFI_STA_STATUS_ASSOCIATED` | 3 | 已经连上 AP，但未分配 IP 地址 |
| `QL_WIFI_STA_STATUS_CONNECTED` | 4 | 已经连上 AP，并且已分配 IP 地址 |
| `QL_WIFI_STA_STATUS_DISCONNECTED` | 5 | 和 AP 断开连接 |
| `QL_WIFI_STA_STATUS_ERROR` | 6 | 出错状态 |

> 解读：关键区分 `ASSOCIATED`（二层关联成功但无 IP）与 `CONNECTED`（已拿到 DHCP IP，可上网）。业务判断"联网可用"应以 `CONNECTED` 为准。

##### 3.3.39.3 ql_wifi_sta_status_t（P46–P47）

STA 模式的状态信息结构体（信息量最丰富的结构体，含 IPv4/IPv6 与原因码）：

```c
typedef struct ql_wifi_sta_status_struct
{
    QL_WIFI_STA_STATUS_E status;
    char ifname[32];
    char ap_bssid[18];
    int rssi;
    uint8_t has_addr;
    ql_net_addr_t addr;
    uint8_t has_addr6;
    ql_net_addr6_t addr6;
    QL_WIFI_REASON_CODE_E reason_code;
    QL_WIFI_SUB_REASON_CODE_E sub_reason_code;
} ql_wifi_sta_status_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_WIFI_STA_STATUS_E` | `status` | STA 模式状态（见 3.3.39.2） |
| `char` | `ifname` | 网络接口名 |
| `char` | `ap_bssid` | BSSID，基本服务集标识符（所连 AP 的 MAC）|
| `int` | `rssi` | STA 设备连接热点时的信号强度，**单位 dBm；`-9999` 表示无效值** |
| `uint8_t` | `has_addr` | 是否为 IPv4 地址：`1`=是，`0`=否 |
| `ql_net_addr_t` | `addr` | IPv4 地址（`ql_net_addr_t` 定义在 `ql_net_common.h`）|
| `uint8_t` | `has_addr6` | 是否为 IPv6 地址：`1`=是，`0`=否 |
| `ql_net_addr6_t` | `addr6` | IPv6 地址（`ql_net_addr6_t` 定义在 `ql_net_common.h`）|
| `QL_WIFI_REASON_CODE_E` | `reason_code` | STA 错误原因码（定义在 `ql_wifi_common.h`）|
| `QL_WIFI_SUB_REASON_CODE_E` | `sub_reason_code` | STA 错误次要原因码（定义在 `ql_wifi_common.h`）|

> 解读：`reason_code`/`sub_reason_code` 的具体取值本文档未展开（仅指向 `ql_wifi_common.h`），用于诊断断连原因（如认证失败、信号丢失等）。

#### 3.3.40 ql_wifi_sta_start（P47）

开启 STA 功能。

- **原型**：`int ql_wifi_sta_start(void);`
- **参数**：无
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他
- **备注（重要）**：调用该函数前，须**开启 Wi-Fi 功能**（`ql_wifi_enable()`），且将 Wi-Fi 工作模式设置为 **STA 模式**（`ql_wifi_work_mode_set(QL_WIFI_WORK_MODE_STA)`）。

#### 3.3.41 ql_wifi_sta_stop（P48）

关闭 STA 功能。

- **原型**：`int ql_wifi_sta_stop(void);`
- **参数**：无
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

#### 3.3.42 ql_wifi_sta_get_status（P48）

获取 STA 模式的状态信息。

- **原型**：`int ql_wifi_sta_get_status(ql_wifi_sta_status_t *p_sta);`
- **参数**：`p_sta` [Out] STA 模式的状态信息（见 3.3.39.3）
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / `QL_ERR_INVALID_ARG` / 其他

#### 3.3.43 ql_wifi_sta_start_scan（P49）

STA 模式扫描外部热点。**异步**——扫描结果通过 3.3.44 注册的回调返回。

- **原型**：`int ql_wifi_sta_start_scan(void);`
- **参数**：无
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

#### 3.3.44 ql_wifi_sta_set_scan_result_ind_cb（P49）

注册扫描结果回调函数。

- **原型**：`int ql_wifi_sta_set_scan_result_ind_cb(ql_wifi_sta_scan_result_ind_cb_f cb);`
- **参数**：`cb` [In] 扫描结果回调函数（见 3.3.44.1）
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

##### 3.3.44.1 ql_wifi_sta_scan_result_ind_cb_f（P50）

扫描结果回调函数类型，用于处理 STA 模式下的扫描结果。

- **原型**：`typedef void (*ql_wifi_sta_scan_result_ind_cb_f)(ql_wifi_sta_scan_result_t *p_msg);`
- **参数**：`p_msg` [In] 外部热点信息扫描结果（见 3.3.44.2）
- **返回值**：无

##### 3.3.44.2 ql_wifi_sta_scan_result_t（P50）

扫描结果结构体：

```c
typedef struct ql_wifi_sta_scan_result_struct
{
    QL_WIFI_SCAN_REASON_CODE_E reason_code;
    ql_wifi_sta_scan_list_t scan_list;
} ql_wifi_sta_scan_result_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_WIFI_SCAN_REASON_CODE_E` | `reason_code` | 扫描结果码（见 3.3.44.5） |
| `ql_wifi_sta_scan_list_t` | `scan_list` | 热点信息扫描列表（见 3.3.44.3）。**仅当 `reason_code` 为 `QL_WIFI_SCAN_REASON_CODE_SUCCESS` 时该字段有效** |

##### 3.3.44.3 ql_wifi_sta_scan_list_t（P50–P51）

扫描热点列表结构体：

```c
typedef struct ql_wifi_sta_scan_list_struct
{
    int cnt;
    ql_wifi_sta_scan_info_t info[QL_WIFI_MAX_SCAN_INFO_CNT];
} ql_wifi_sta_scan_list_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `int` | `cnt` | 扫描到的外部热点个数 |
| `ql_wifi_sta_scan_info_t` | `info` | 扫描的外部热点信息数组。`QL_WIFI_MAX_SCAN_INFO_CNT` 定义在 `ql_wifi_common.h`，**值为 60** |

##### 3.3.44.4 ql_wifi_sta_scan_info_t（P51）

单个热点信息结构体：

```c
typedef struct ql_wifi_sta_scan_info_struct
{
    char bssid[18];
    char essid[33];
    int signal;
    int frequency;
    QL_WIFI_AUTH_E auth;
} ql_wifi_sta_scan_info_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `char` | `bssid` | BSSID，基本服务集标识符（热点 MAC）|
| `char` | `essid` | 扩展服务集标识符（即热点 SSID，最长 32+1=33 字节）|
| `int` | `signal` | 信号强度，**单位 dBm** |
| `int` | `frequency` | 当前工作频率，**单位 Hz** |
| `QL_WIFI_AUTH_E` | `auth` | 认证方式（见 3.3.29.2） |

##### 3.3.44.5 QL_WIFI_SCAN_REASON_CODE_E（P52）

扫描结果码枚举：

```c
typedef enum QL_WIFI_SCAN_REASON_CODE
{
    QL_WIFI_SCAN_REASON_CODE_SUCCESS = 0,
    QL_WIFI_SCAN_REASON_CODE_BUSY = 1,
    QL_WIFI_SCAN_REASON_CODE_BREAK_START = 2,
    QL_WIFI_SCAN_REASON_CODE_BREAK_STOP = 3,
    QL_WIFI_SCAN_REASON_CODE_UNKNOW = 4
} QL_WIFI_SCAN_REASON_CODE_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_WIFI_SCAN_REASON_CODE_SUCCESS` | 0 | 扫描成功 |
| `QL_WIFI_SCAN_REASON_CODE_BUSY` | 1 | 扫描繁忙，建议重试 |
| `QL_WIFI_SCAN_REASON_CODE_BREAK_START` | 2 | 扫描被 `ql_wifi_sta_start()` 打断 |
| `QL_WIFI_SCAN_REASON_CODE_BREAK_STOP` | 3 | 扫描被 `ql_wifi_sta_stop()` 打断 |
| `QL_WIFI_SCAN_REASON_CODE_UNKNOW` | 4 | 扫描失败，原因未知 |

#### 3.3.45 ql_wifi_set_service_error_cb（P52）

注册 Wi-Fi 服务异常回调函数。

- **原型**：`int ql_wifi_set_service_error_cb(ql_wifi_service_error_cb_f cb);`
- **参数**：`cb` [In] Wi-Fi 服务异常回调函数。**只有当 Wi-Fi 服务异常退出才会执行回调函数**（见 3.3.45.1）
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

##### 3.3.45.1 ql_wifi_service_error_cb_f（P53）

服务异常回调函数类型：

- **原型**：`typedef void (*ql_wifi_service_error_cb_f)(int error);`
- **参数**：`error` [In] 错误码
- **返回值**：无

**备注（C/S 架构与恢复机制，P53，关键）**：
Wi-Fi API 使用 **C/S（Client/Server）架构**。用户调用 `ql_wifi_init()` 时会创建与服务程序（**`ql_wifid`**）的通信连接；调用其它 Wi-Fi API 时发送请求给服务程序，服务程序处理完请求后返回处理结果给 Wi-Fi API。若服务程序异常退出，用户调用 Wi-Fi API 时会返回 `QL_ERR_SERVICE_ABORT` 错误码。用户可调用 `ql_wifi_set_service_error_cb()` 注册服务程序异常的回调函数，用于监控服务程序的异常退出。**检测到服务程序异常退出后，客户端程序可调用 `ql_wifi_deinit()` 注销 Wi-Fi 服务，再调用 `ql_wifi_init()` 重建与服务程序的通信连接，恢复 Wi-Fi 服务。**

> 解读：这是 Wi-Fi 服务崩溃后的标准自愈套路 —— `service_error_cb` → `deinit` → `init` → 重新配置启动。与 modem_mng 中守护进程对 SDK 服务异常的处理思路一致。

#### 3.3.46 ql_wifi_set_ap_sta_connect_ind_cb（P53–P54）

注册 AP 模式和 STA 设备连接状态回调函数（**AP 侧**用于感知有终端 STA 连入/断开）。

- **原型**：`int ql_wifi_set_ap_sta_connect_ind_cb(ql_wifi_ap_sta_connect_ind_cb_f cb);`
  - （文档原文印刷为 `ql_wifi_set_ap_sta_connect ind_cb`，应为 `_connect_ind_cb`，疑似排版空格错误）
- **参数**：`cb` [In] AP 模式和 STA 设备连接状态回调函数（见 3.3.46.1）
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

##### 3.3.46.1 ql_wifi_ap_sta_connect_ind_cb_f（P54）

回调函数类型，用于处理 AP 模式下 STA 设备的连接状态：

```c
typedef void (*ql_wifi_ap_sta_connect_ind_cb_f)(QL_WIFI_AP_INDEX_E index,
                                                ql_wifi_sta_connect_status_t *p_msg);
```
- **参数**：`index` [In] AP 模式索引；`p_msg` [In] AP 模式和 STA 设备的连接状态（见 3.3.46.2）
- **返回值**：无

##### 3.3.46.2 ql_wifi_sta_connect_status_t（P54）

连接状态结构体：

```c
typedef struct
{
    int is_connected;
    char macaddr[18];
} ql_wifi_sta_connect_status_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `int` | `is_connected` | 连接状态：`1`=已连接，`0`=已断开 |
| `char` | `macaddr` | STA 设备的 MAC 地址 |

> 解读：AP 模式下每当有客户端连入或离开，回调上报该客户端 MAC 与连接/断开状态，可用于在线终端统计。

#### 3.3.47 ql_wifi_country_code_set（P55）

设置 Wi-Fi 国家码（全局，区别于 3.3.21 的 AP 专用版）。

- **原型**：`int ql_wifi_country_code_set(const char *country_code);`
- **参数**：`country_code` [In] 国家码。**默认值 CN（中国）；最大长度 2 字节；不可为空。**
- **返回值**：四类错误码
- **备注**：Wi-Fi 国家码默认为 CN，如需修改国家码，则需在调用 `ql_wifi_enable()` 开启 Wi-Fi 功能**之前**修改。

#### 3.3.48 ql_wifi_country_code_get（P55）

获取 Wi-Fi 国家码。

- **原型**：`int ql_wifi_country_code_get(char *country_code_buf, int buf_len);`
- **参数**：`country_code_buf` [Out] 指向存储 Wi-Fi 国家码配置的缓存；`buf_len` [In] 缓存大小（单位字节），**建议设置为 3 字节**
- **返回值**：`QL_ERR_OK` 等四类错误码

#### 3.3.49 ql_wifi_p2p_dev_name_set（P56）

设置 Wi-Fi P2P 设备名称（P2P 发现时对外显示的名字）。

- **原型**：`int ql_wifi_p2p_dev_name_set(const char *dev_name);`
- **参数**：`dev_name` [In] P2P 设备名称
- **返回值**：四类错误码

#### 3.3.50 ql_wifi_p2p_dev_name_get（P56）

获取 Wi-Fi P2P 设备名称。

- **原型**：`int ql_wifi_p2p_dev_name_get(char *dev_name_buf, int buf_len);`
- **参数**：`dev_name_buf` [Out] 指向存储 P2P 设备名称配置的缓存；`buf_len` [In] 缓存大小（字节）
- **返回值**：四类错误码

#### 3.3.51 ql_wifi_p2p_dev_type_set（P57）

设置 Wi-Fi P2P 设备类型。

- **原型**：`int ql_wifi_p2p_dev_type_set(const char *dev_type);`
- **参数**：`dev_type` [In] P2P 设备类型
- **返回值**：四类错误码
- **备注（设备类型格式，P57）**：
  - 格式：`<categ>-<OUI>-<subcateg>`
  - `<categ>`：类别，为整数值
  - `<OUI>`：组织唯一标识符。四个八位字节的 16 进制编码数值；**WPS OUI 默认为 `0050F204`**
  - `<subcateg>`：OUI 特定的整型类型数值子类别
  - 举例：
    - `1-0050F204-1`（电脑/个人电脑）
    - `1-0050F204-2`（计算机/服务器）
    - `5-0050F204-1`（存储/NAS）
    - `6-0050F204-1`（网络基础设施/AP）

#### 3.3.52 ql_wifi_p2p_dev_type_get（P57–P58）

获取 Wi-Fi P2P 设备类型。

- **原型**：`int ql_wifi_p2p_dev_type_get(char *dev_type_buf, int buf_len);`
- **参数**：`dev_type_buf` [Out] 指向存储 P2P 设备类型配置的缓存；`buf_len` [In] 缓存大小（字节）
- **返回值**：四类错误码

#### 3.3.53 ql_wifi_p2p_oper_class_channel_set（P58–P59）

设置 Wi-Fi P2P 设备的工作频率（通过 operating class + operating channel 共同确定频率）。

- **原型**：`int ql_wifi_p2p_oper_class_channel_set(int oper_class, int oper_channel);`
- **参数**：`oper_class` [In] 待设置的 operating class；`oper_channel` [In] 待设置的 operating channel
- **返回值**：四类错误码
- **备注（暂支持取值表，P59）**：`oper_class` 与 `oper_channel` 共同限定 P2P 设备工作频率：

| oper_class | oper_channel | frequency | channel spacing |
|---|---|---|---|
| 81 | 1–13 | 2407 + 5 × oper_channel | 25 MHz |
| 82 | 14 | 2414 + 5 × oper_channel | 25 MHz |
| 115 | 36,40,44,48 | 5000 + 5 × oper_channel | 20 MHz |
| 116 | 36,44 | 5000 + 5 × oper_channel | 40 MHz |
| 117 | 40,48 | 5000 + 5 × oper_channel | 40 MHz |
| 124 | 149,153,157,161 | 5000 + 5 × oper_channel | 20 MHz |
| 125 | 149,153,157,161,165 | 5000 + 5 × oper_channel | 20 MHz |
| 126 | 149,157 | 5000 + 5 × oper_channel | 40 MHz |
| 127 | 153,161 | 5000 + 5 × oper_channel | 40 MHz |

> 解读：oper_class 81/82 对应 2.4 GHz（FC30R 适用频段）；115~127 对应 5 GHz 频段（计算公式 `5000 + 5×channel`）。虽然表中列出 5 GHz 项，但 FC30R 主体为 2.4 GHz，5 GHz 可用性需以实测/模块能力为准。P2P 频率受双方协商约束。

#### 3.3.54 ql_wifi_p2p_oper_class_channel_get（P59）

获取 Wi-Fi P2P 设备的工作频率。

- **原型**：`int ql_wifi_p2p_oper_class_channel_get(int *p_oper_class, int *p_oper_channel);`
- **参数**：`p_oper_class` [Out] 存放当前 P2P 设备 operating class 的缓存；`p_oper_channel` [Out] 存放当前 P2P 设备 operating channel 的缓存
- **返回值**：四类错误码

#### 3.3.55 ql_wifi_p2p_ssid_postfix_set（P59–续）

设置 Wi-Fi P2P 模式的 SSID 后缀名。

- **原型**：`int ql_wifi_p2p_ssid_postfix_set(const char *ssid_postfix);`
- **参数**：`ssid_postfix` [In] SSID 后缀名
- **返回值**：四类错误码（`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / `QL_ERR_INVALID_ARG` / 其他）
- **备注（P60，关键）**：当 Wi-Fi P2P 作为 **GO 角色**时，具有和 Wi-Fi AP 模式类似的功能，也存在 SSID，其格式为：**`DIRECT-xx-ssid_postfix`**，其中 `xx` 为**随机产生的两个字母或数字**，`ssid_postfix` 为 SSID 的后缀名；其他 Wi-Fi STA 模式的设备可以扫描到该 SSID 并进行连接。

> 解读：P2P GO 对外广播的热点名遵循 Wi-Fi Direct 规范的 `DIRECT-` 前缀约定，`xx` 由协议栈随机生成，开发者只能定制后缀部分。这意味着 Legacy STA 看到的热点名是“半随机”的，应用层做自动连接时不能硬编码完整 SSID。

#### 3.3.56 ql_wifi_p2p_ssid_postfix_get（P60）

获取 Wi-Fi P2P 模式的 SSID 后缀名。

- **原型**：`int ql_wifi_p2p_ssid_postfix_get(char *ssid_postfix_buf, int buf_len);`
- **参数**：`ssid_postfix_buf` [Out] 指向存放当前 SSID 后缀名的缓存；`buf_len` [In] 缓存大小（字节）
- **返回值**：四类错误码

#### 3.3.57 ql_wifi_p2p_set_enable_status_ind_cb（P61）

注册 Wi-Fi P2P 模式启用状态回调函数。

- **原型**：`int ql_wifi_p2p_set_enable_status_ind_cb(ql_wifi_p2p_enable_status_ind_cb_f cb);`
- **参数**：`cb` [In] P2P 模式启用状态回调函数（见 3.3.57.1）
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT`（未初始化）/ `QL_ERR_SERVICE_ABORT`（服务出错）/ 其他

##### 3.3.57.1 ql_wifi_p2p_enable_status_ind_cb_f（P61）

处理 Wi-Fi P2P 模式启用状态的回调原型：

```c
typedef void (*ql_wifi_p2p_enable_status_ind_cb_f)
    (QL_WIFI_P2P_ENABLE_STATUS_E pre_status, QL_WIFI_P2P_ENABLE_STATUS_E status);
```

- `pre_status` [In] P2P 模式之前的启用状态；`status` [In] P2P 模式当前的启用状态；无返回值。

##### 3.3.57.2 QL_WIFI_P2P_ENABLE_STATUS_E（P61–P62）

```c
typedef enum
{
    QL_WIFI_P2P_EN_STATUS_DISABLED = 0,
    QL_WIFI_P2P_EN_STATUS_ENABLED,
    QL_WIFI_P2P_EN_STATUS_ERROR,
} QL_WIFI_P2P_ENABLE_STATUS_E;
```

| 成员 | 值 | 描述 |
|---|---|---|
| `QL_WIFI_P2P_EN_STATUS_DISABLED` | 0 | Wi-Fi P2P 模式已禁用 |
| `QL_WIFI_P2P_EN_STATUS_ENABLED` | 1 | Wi-Fi P2P 模式已启用 |
| `QL_WIFI_P2P_EN_STATUS_ERROR` | 2 | Wi-Fi P2P 模式启用出错 |

#### 3.3.58 ql_wifi_p2p_enable（P62）

启用 Wi-Fi P2P 功能。

- **原型**：`int ql_wifi_p2p_enable(void);`
- **参数**：无
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

#### 3.3.59 ql_wifi_p2p_disable（P62–P63）

禁用 Wi-Fi P2P 功能。

- **原型**：`int ql_wifi_p2p_disable(void);`
- **参数**：无
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

#### 3.3.60 ql_wifi_p2p_set_dev_found_ind_cb（P63）

注册 P2P 设备扫描信息回调函数。

- **原型**：`int ql_wifi_p2p_set_dev_found_ind_cb(ql_wifi_p2p_dev_found_ind_cb_f cb);`
- **参数**：`cb` [In] P2P 设备扫描信息回调函数（见 3.3.60.1）
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

##### 3.3.60.1 ql_wifi_p2p_dev_found_ind_cb_f（P63–P64）

处理 P2P 设备扫描信息的回调原型：

```c
typedef void (*ql_wifi_p2p_dev_found_ind_cb_f)(ql_wifi_p2p_found_dev_info_t *p_msg);
```

- `p_msg` [In] P2P 设备扫描信息（见 3.3.60.2）；无返回值。

##### 3.3.60.2 ql_wifi_p2p_found_dev_info_t（P64）

P2P 设备扫描信息结构体：

```c
typedef struct
{
    int  is_found;
    char macaddr[18];
    char dev_type[32];
    char dev_name[32];
} ql_wifi_p2p_found_dev_info_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| int | `is_found` | P2P 设备扫描结果。`1`=该 P2P 设备已扫描到，可连接；`0`=该 P2P 设备已丢失，不可连接，需要重新扫描 |
| char[18] | `macaddr` | P2P 设备 MAC 地址 |
| char[32] | `dev_type` | P2P 设备类型 |
| char[32] | `dev_name` | P2P 设备名称 |

> 解读：`is_found` 是“出现/消失”的双向事件标志——扫描既上报新发现设备（1），也上报已离开设备（0）。应用维护对端列表时需据此增删。

#### 3.3.61 ql_wifi_p2p_dev_find_start（P64–P65）

开始扫描周围的 P2P 设备。

- **原型**：`int ql_wifi_p2p_dev_find_start(void);`
- **参数**：无
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

#### 3.3.62 ql_wifi_p2p_dev_find_stop（P65）

停止扫描周围的 P2P 设备。

- **原型**：`int ql_wifi_p2p_dev_find_stop(void);`
- **参数**：无
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

#### 3.3.63 ql_wifi_p2p_get_status（P65）

获取 Wi-Fi P2P 模式的连接状态信息。

- **原型**：`int ql_wifi_p2p_get_status(ql_wifi_p2p_status_t *p_status);`
- **参数**：`p_status` [Out] Wi-Fi P2P 模式的连接状态信息（见 3.3.63.1）
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / `QL_ERR_INVALID_ARG` / 其他

##### 3.3.63.1 ql_wifi_p2p_status_t（P66–P67）

Wi-Fi P2P 模式的连接状态信息结构体：

```c
typedef struct
{
    QL_WIFI_P2P_STATUS_E       status;
    QL_WIFI_P2P_ROLE_E         role;
    int                        freq;
    char                       ssid[33];
    char                       passwd[64];
    char                       pin_code[9];
    char                       ifname[32];
    uint8_t                    has_addr;
    ql_net_addr_t              addr;
    uint8_t                    has_addr6;
    ql_net_addr_t              addr6;
    QL_WIFI_REASON_CODE_E      reason_code;
    QL_WIFI_SUB_REASON_CODE_E  sub_reason_code;
} ql_wifi_p2p_status_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_WIFI_P2P_STATUS_E` | `status` | Wi-Fi P2P 模式连接状态（见 3.3.63.2） |
| `QL_WIFI_P2P_ROLE_E` | `role` | Wi-Fi P2P 模式的角色（见 3.3.63.3） |
| int | `freq` | Wi-Fi P2P 模式的工作频率 |
| char[33] | `ssid` | Wi-Fi P2P 模式的 SSID（针对 GO 角色） |
| char[64] | `passwd` | Wi-Fi P2P 模式的 SSID 密码（针对 GO 角色） |
| char[9] | `pin_code` | 使用 PIN 方式主动连接对端 P2P 设备时，对端设备需要输入的 `pin_code` 进行验证 |
| char[32] | `ifname` | Wi-Fi P2P 模式 Linux 网络接口名 |
| uint8_t | `has_addr` | 是否为 IPv4 地址（针对 Client 角色）。`1`=是，`0`=否 |
| `ql_net_addr_t` | `addr` | IPv4 地址，`ql_net_addr_t` 定义在 API 头文件 `ql_net_common.h` 中（针对 Client 角色） |
| uint8_t | `has_addr6` | 是否为 IPv6 地址（针对 Client 角色）。`1`=是，`0`=否 |
| `ql_net_addr_t` | `addr6` | IPv6 地址，`ql_net_addr_t` 定义在 `ql_net_common.h`（针对 Client 角色） |
| `QL_WIFI_REASON_CODE_E` | `reason_code` | Wi-Fi P2P 模式连接失败原因码，定义在 `ql_wifi_common.h` |
| `QL_WIFI_SUB_REASON_CODE_E` | `sub_reason_code` | Wi-Fi P2P 模式连接失败次要原因码，定义在 `ql_wifi_common.h` |

> 解读：GO 角色提供 `ssid`/`passwd`（供他人接入自己），Client 角色提供 `addr`/`addr6`（自己从 GO 拿到的 IP）。`pin_code` 用于 WPS-PIN 配对。结构体与前述 STA/AP 的 status_t 复用同一套 reason_code 体系。

##### 3.3.63.2 QL_WIFI_P2P_STATUS_E（P67–P68）

```c
typedef enum
{
    QL_WIFI_P2P_STATUS_IDLE = 0,
    QL_WIFI_P2P_STATUS_CONNECTING,
    QL_WIFI_P2P_STATUS_ASSOCIATED,
    QL_WIFI_P2P_STATUS_CONNECTED,
    QL_WIFI_P2P_STATUS_DISCONNECTED,
    QL_WIFI_P2P_STATUS_ERROR
} QL_WIFI_P2P_STATUS_E;
```

| 成员 | 描述 |
|---|---|
| `QL_WIFI_P2P_STATUS_IDLE` | 还未开始和对端 P2P 设备进行连接 |
| `QL_WIFI_P2P_STATUS_CONNECTING` | 正在连接对端 P2P 设备，包括 P2P 角色的协商 |
| `QL_WIFI_P2P_STATUS_ASSOCIATED` | 如果 CONNECTING 过程中协商的是 **Client 角色**，则该状态表示已经连上对端 P2P 设备，但未分配 IP 地址；如果是 **GO 角色**，则没有该状态 |
| `QL_WIFI_P2P_STATUS_CONNECTED` | 已经连上对端 P2P 设备，且 IP 地址已分配 |
| `QL_WIFI_P2P_STATUS_DISCONNECTED` | P2P 连接断开 |
| `QL_WIFI_P2P_STATUS_ERROR` | P2P 连接出错 |

> 解读：状态机 IDLE → CONNECTING →（仅 Client）ASSOCIATED → CONNECTED → DISCONNECTED。ASSOCIATED 是“链路已连但 DHCP 未完成”的中间态，仅 Client 经历；GO 自身就是地址分配方，直接到 CONNECTED。

##### 3.3.63.3 QL_WIFI_P2P_ROLE_E（P68）

```c
typedef enum
{
    QL_WIFI_P2P_ROLE_NONE = 0,
    QL_WIFI_P2P_ROLE_CLI,
    QL_WIFI_P2P_ROLE_GO
} QL_WIFI_P2P_ROLE_E;
```

| 成员 | 描述 |
|---|---|
| `QL_WIFI_P2P_ROLE_NONE` | 还未开始 P2P 角色协商或者协商失败 |
| `QL_WIFI_P2P_ROLE_CLI` | 协商成功，且为 P2P Client 角色 |
| `QL_WIFI_P2P_ROLE_GO` | 协商成功，且为 P2P GO 角色 |

#### 3.3.64 ql_wifi_p2p_set_status_ind_cb（P68）

注册 Wi-Fi P2P 模式连接状态回调函数。

- **原型**：`int ql_wifi_p2p_set_status_ind_cb(ql_wifi_p2p_status_ind_cb_f cb);`
- **参数**：`cb` [In] P2P 模式连接状态回调函数（见 3.3.64.1）
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

##### 3.3.64.1 ql_wifi_p2p_status_ind_cb_f（P69）

处理 Wi-Fi P2P 模式连接状态的回调原型：

```c
typedef void (*ql_wifi_p2p_status_ind_cb_f)(QL_WIFI_P2P_STATUS_E pre_status,
                                            ql_wifi_p2p_status_t *p_msg);
```

- `pre_status` [In] P2P 模式之前的连接状态（见 3.3.63.2）；`p_msg` [In] P2P 模式当前的连接状态信息（见 3.3.63.1）；无返回值。

#### 3.3.65 ql_wifi_p2p_set_peer_dev_req_ind_cb（P69）

注册对端 P2P 设备连接请求回调函数（即被动接收他端发起的连接请求时触发）。

- **原型**：`int ql_wifi_p2p_set_peer_dev_req_ind_cb(ql_wifi_p2p_peer_dev_req_ind_cb_f cb);`
- **参数**：`cb` [In] 对端 P2P 设备连接请求回调函数（见 3.3.65.1）
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

##### 3.3.65.1 ql_wifi_p2p_peer_dev_req_ind_cb_f（P70）

处理对端 P2P 设备连接请求的回调原型：

```c
typedef void (*ql_wifi_p2p_peer_dev_req_ind_cb_f)(ql_wifi_p2p_req_peer_dev_info_t *pmsg);
```

- `pmsg` [In] 对端 P2P 设备信息（见 3.3.65.2）；无返回值。

##### 3.3.65.2 ql_wifi_p2p_req_peer_dev_info_t（P70）

对端 P2P 设备信息结构体：

```c
typedef struct
{
    char macaddr[18];
} ql_wifi_p2p_req_peer_dev_info_t;
```

| 类型 | 参数 | 描述 |
|---|---|---|
| char[18] | `macaddr` | 对端 P2P 设备 MAC 地址 |

#### 3.3.66 ql_wifi_p2p_connect（P70–P71）

连接对端 P2P 设备。

- **原型**：`int ql_wifi_p2p_connect(const char *macaddr, const char *method, int go_intent);`
- **参数**：
  - `macaddr` [In] 对端 P2P 设备的 MAC 地址
  - `method` [In] P2P 连接方式，取值 **`pbc`** 或 **`pin`**：
    - 当本地 P2P 设备主动使用 **`pin`** 方式发起连接，本地设备会产生一个 PIN 码，对端设备连接时要输入这个 PIN 码；
    - 本地设备使用 **`pbc`** 方式发起连接时，对端设备可以直接连接；
    - 当对端设备使用 `pin` 方式主动连接本地 P2P 设备时，则该参数为对端 P2P 设备产生的 PIN 码。
  - `go_intent` [In] 成为 **GO 角色的渴望程度**。类型整数，取值范围 **0~15**；值越大表示越渴望成为 GO 角色。一般情况下，要想成为 Client 角色则设为 `0`，要想成为 GO 角色则设为 `15`。
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

> 解读：`go_intent` 是 Wi-Fi Direct GO Negotiation 的标准参数，双方各报 0~15，值高者当 GO；两端都报 15 会冲突（协商失败）。`pbc`=Push Button Configuration（一键直连），`pin`=PIN 码配对，二者对应 WPS 的两种入网方式。

#### 3.3.67 ql_wifi_p2p_disconnect（P71）

断开对端 P2P 设备。

- **原型**：`int ql_wifi_p2p_disconnect(void);`
- **参数**：无
- **返回值**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / 其他

---

## 第 4 章 示例（P72–P78）

Wi-Fi API 示例代码请参考 SDK 中的 **`sample/test_sdk_api/m_wifi.c`**。本章演示三种典型用法：STA 模式、单 AP 模式、P2P 模式。三段示例共享同一套“初始化重试”范式。

### 通用初始化范式（三段示例共用）

```c
/** 定义 Wi-Fi 服务最大初始化次数 */
#define WIFI_INIT_MAX_TRY_CNT 50

/** Wi-Fi 服务初始化 */
int wifi_is_init = 0, wifi_init_try_cnt = 0, ret = QL_ERR_OK;
while (WIFI_INIT_MAX_TRY_CNT > wifi_init_try_cnt)
{
    ret = ql_wifi_init();
    if (QL_ERR_SERVICE_NOT_READY == ret)          /* 服务未就绪：等 200ms 重试 */
    {
        printf("wifi service is not ready, try again, try count = %d\n", (wifi_init_try_cnt + 1));
        usleep(200 * 1000);
        wifi_init_try_cnt++;
    }
    else if (QL_ERR_OK == ret)                     /* 初始化成功：置标志并跳出 */
    {
        printf("Succeeded to initialize wifi service\n");
        wifi_is_init = 1;
        break;
    }
    else                                           /* 其他错误：直接跳出（不再重试） */
    {
        printf("Failed to initialize wifi service, ret = %d\n", ret);
        break;
    }
}
if (1 != wifi_is_init) { printf("Failed to initialize wifi service\n"); return -1; }
```

> 关键点：`ql_wifi_init()` 返回 `QL_ERR_SERVICE_NOT_READY` 时必须轮询重试（最多 50 次 × 200ms = 10s），因为 Wi-Fi 服务（FC30R 侧）启动需要时间；其他错误码则视为不可恢复，立即放弃。这与第 3.3.1 节备注“建议重试”相互印证。

### 4.1 设置 STA 模式（P72–P74）

完整流程：`init`（带重试）→ `ql_wifi_enable()` → `ql_wifi_work_mode_set(QL_WIFI_WORK_MODE_STA)` → `ql_wifi_sta_ssid_set(...)` → `ql_wifi_sta_auth_set(...)` → `ql_wifi_sta_start()`。

```c
/** 启用 Wi-Fi */
ret = ql_wifi_enable();
if (QL_ERR_OK != ret) { printf("Failed to enable wifi, ret = %d", ret); return -1; }

/** 设置 Wi-Fi 工作模式为 STA 模式 */
ret = ql_wifi_work_mode_set(QL_WIFI_WORK_MODE_STA);
if (QL_ERR_OK != ret) { printf("Failed to set wifi work mode, ret = %d", ret); return -1; }

/** 设置 SSID（要连接的目标热点名） */
ret = ql_wifi_sta_ssid_set("Quectel-Hf");
if (QL_ERR_OK != ret) { printf("Failed to set ssid, ret = %d", ret); return -1; }

/** 设置 STA 模式安全认证 */
ql_wifi_sta_auth_t auth;
memset(&auth, 0, sizeof(ql_wifi_sta_auth_t));
auth.auth = QL_WIFI_AUTH_WPA_WPA2_PSK_BOTH;                 /* 认证方式 WPA/WPA2-PSK 兼容 */
auth.wpa_psk.pairwise = QL_WIFI_AUTH_WPA_PAIRWISE_AUTO;     /* 加密算法自动 */
strncpy(auth.wpa_psk.passwd, "quectel-i-hf", sizeof(auth.wpa_psk.passwd) - 1);  /* 密码 */
ret = ql_wifi_sta_auth_set(&auth);
if (QL_ERR_OK != ret) { printf("Failed to set wifi auth, ret = %d", ret); return -1; }

/** 开启 STA 模式 */
ret = ql_wifi_sta_start();
if (QL_ERR_OK != ret) { printf("Failed to start sta, ret = %d", ret); ql_wifi_disable(); return -1; }

return 0;
```

> 注意：`strncpy(..., sizeof(...) - 1)` 是为了保证 NUL 结尾、防止密码字段溢出。STA 失败时调用 `ql_wifi_disable()` 回滚。`ql_wifi_sta_auth_t` 的结构在 3.3.37.1（STA auth）中定义，使用 `wpa_psk.pairwise` + `wpa_psk.passwd` 子字段。

### 4.2 设置单 AP 模式（P74–P76）

完整流程：`init`（带重试）→ `ql_wifi_enable()` → `ql_wifi_work_mode_set(QL_WIFI_WORK_MODE_AP0)` →（信道/带宽等配置，示例省略）→ `ql_wifi_ap_ssid_set(...)` → `ql_wifi_ap_auth_set(QL_WIFI_AP_INDEX_AP0, &auth)` → `ql_wifi_ap_start(QL_WIFI_AP_INDEX_AP0)`。

```c
/** 设置 Wi-Fi 工作模式为单 AP 模式 */
ret = ql_wifi_work_mode_set(QL_WIFI_WORK_MODE_AP0);
if (QL_ERR_OK != ret) { printf("Failed to set wifi work mode, ret = %d", ret); return -1; }

/** 设置信道，带宽等配置此处省略 */
//……..

/** 设置 SSID（对外广播的热点名） */
ret = ql_wifi_ap_ssid_set("Quectel-AP0");
if (QL_ERR_OK != ret) { printf("Failed to set ssid, ret = %d", ret); return -1; }

/** 设置 AP 模式安全认证 */
ql_wifi_ap_auth_t auth;
memset(&auth, 0, sizeof(ql_wifi_ap_auth_t));
auth.auth = QL_WIFI_AUTH_WPA_WPA2_PSK_BOTH;
auth.wpa_psk.pairwise = QL_WIFI_AUTH_WPA_PAIRWISE_AUTO;
strncpy(auth.wpa_psk.passwd, "123456789", sizeof(auth.wpa_psk.passwd) - 1);
ret = ql_wifi_ap_auth_set(QL_WIFI_AP_INDEX_AP0, &auth);
if (QL_ERR_OK != ret) { printf("Failed to set wifi auth, ret = %d", ret); return -1; }

/** 开启 AP 模式 */
ret = ql_wifi_ap_start(QL_WIFI_AP_INDEX_AP0);
if (QL_ERR_OK != ret) {
    printf("Failed to start ap[%d], ret = %d", QL_WIFI_AP_INDEX_AP0, ret);
    ql_wifi_disable();
    return -1;
}

return 0;
```

> 对比 STA：AP 模式的 `ssid_set`/`auth_set`/`start` 都带 `QL_WIFI_AP_INDEX_AP0` 索引参数（STA 无索引）；`ql_wifi_ap_auth_t` 与 STA 的 auth 结构字段名一致但是独立类型（3.3.29.1）。注释 `ql_wifi_ap_ssid_set("Quectel-AP0")` 在示例中未带 index——以正文 3.3.9 原型为准应为 `ql_wifi_ap_ssid_set(QL_WIFI_AP_INDEX_AP0, "Quectel-AP0")`，示例代码此处省略了 index 实参（文档排版简写，开发时需补全）。

### 4.3 Wi-Fi P2P 模式（P76–P78）

P2P 示例额外定义了一个设备发现回调，并演示主动连接流程：`init` → `enable` → `ql_wifi_p2p_dev_name_set(...)` →（其他配置省略）→ `ql_wifi_p2p_enable()` → `ql_wifi_p2p_set_dev_found_ind_cb(...)` → `ql_wifi_p2p_dev_find_start()` →（等回调上报对端）→ `ql_wifi_p2p_connect(...)`。

```c
/** 设备发现回调：仅把扫描到的 P2P 设备信息转交主进程 */
void wifi_p2p_dev_found_ind_cb(ql_wifi_p2p_found_dev_info_t *p_msg)
{
    /** 将 p2p 设备信息发给主进程，可以使用 socketpair */
}

int main()
{
    /* …… init 带重试范式（同上） + ql_wifi_enable() …… */

    /** 设置 P2P 设备名称 */
    ret = ql_wifi_p2p_dev_name_set("Quectel-P2P");
    if (QL_ERR_OK != ret) { printf("Failed to set ssid, ret = %d", ret); return -1; }

    //其他配置接口省略….

    ret = ql_wifi_p2p_enable();
    if (QL_ERR_OK != ret) { printf("Failed to enable p2p, ret = %d", ret); ql_wifi_disable(); return -1; }

    /** 注册设备发现回调 */
    ret = ql_wifi_p2p_set_dev_found_ind_cb(p2p_dev_found_ind_cb);
    if (QL_ERR_OK != ret) {
        printf("Failed to set dev found ind cb, ret = %d", ret);
        ql_wifi_p2p_disable(); ql_wifi_disable(); return -1;
    }

    /** 开始扫描周围 P2P 设备 */
    ret = ql_wifi_p2p_dev_find_start();
    if (QL_ERR_OK != ret) {
        printf("Failed to start finding p2p dev, ret = %d", ret);
        ql_wifi_p2p_disable(); ql_wifi_disable(); return -1;
    }

    /** 等待 wifi_p2p_dev_found_ind_cb 上报对端 P2P 设备信息，可以使用 select 机制 */
    /** 并连接对端 P2P 设备（pbc 方式，go_intent=15 渴望成为 GO） */
    ret = ql_wifi_p2p_connect("对端设备 MAC 地址", "pbc", 15);
    if (QL_ERR_OK != ret) {
        printf("Failed to connect p2p, ret = %d", ret);
        ql_wifi_p2p_disable(); ql_wifi_disable(); return -1;
    }

    return 0;
}
```

> 关键工程实践（文档显式给出）：
> 1. **回调里不能调用 Wi-Fi API**（全文备注第 1 条），所以 `wifi_p2p_dev_found_ind_cb` 只把数据通过 **socketpair** 转交主进程，由主进程在外部调用 `ql_wifi_p2p_connect`。
> 2. 主进程用 **select** 机制等待回调上报的对端设备信息，是典型的“回调线程收事件 → 管道/socketpair 通知 → 主循环处理”解耦模式。
> 3. 错误回滚遵循“后开先关”：连接失败先 `ql_wifi_p2p_disable()` 再 `ql_wifi_disable()`。
> 4. 示例中 `ql_wifi_p2p_set_dev_found_ind_cb` 实参写 `p2p_dev_found_ind_cb`，与定义的 `wifi_p2p_dev_found_ind_cb` 函数名不完全一致（文档笔误，实际应一致）。

---

## 第 5 章 附录 参考文档及术语缩写（P79–P80）

### 表 2：参考文档

| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_AG35-CET_QuecOpen(SDK)_快速开发指导 |

### 表 3：术语缩写

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| AES | Advanced Encryption Standard | 高级加密标准 |
| AP | Access Point | 接入点 |
| API | Application Programming Interface | 应用程序编程接口 |
| BSSID | Basic Service Set Identifier | 基本服务标识符 |
| ESSID | Extended Service Set Identifier | 扩展服务集标识符 |
| GO | Group Owner | 组所有者 |
| IoV | Internet of Vehicles | 车联网 |
| IPv4 | Internet Protocol version 4 | 第 4 版互联网协议 |
| IPv6 | Internet Protocol version 6 | 第 6 版互联网协议 |
| MAC | Media Access Control (Address) | 媒体访问控制（位址） |
| PSK | Pre-Shared Key | 预共享的密钥 |
| P2P | Peer to Peer | 点对点 |
| SDK | Software Development Kit | 软件开发工具包 |
| SSID | Service Set Identifier | 服务集标识符 |
| STA | Station | 站点 |
| TKIP | Temporal Key Integrity Protocol | 临时密钥完整性协议 |
| WEP | Wired Equivalent Privacy | 有线等效保密 |
| WLAN | Wireless Local Area Networks | 无线局域网 |
| WPA | Wi-Fi Protected Access | Wi-Fi 网络安全接入 |
| WPS | Wi-Fi Protected Setup | Wi-Fi 保护设置 |
| WSC | Wi-Fi Simple Configuration | Wi-Fi 简化配置 |

---

## 全文要点总结（工程视角）

1. **硬件前提**：AG35-CET 无内置 Wi-Fi，须外挂 FC30R（2.4 GHz）；所有能力经 `ql_wifi_*` SDK API 暴露，头文件 `ql_wifi.h` + `ql_wifi_common.h`。
2. **三种模式互斥（工作模式枚举只有 STA / AP0）**，P2P 是独立开关（`ql_wifi_p2p_enable`），可与 WLAN 并发。
3. **标准启动序列**：`ql_wifi_init`（须轮询重试 SERVICE_NOT_READY）→ `ql_wifi_enable` → `ql_wifi_work_mode_set` →（各项配置）→ `ql_wifi_xxx_start`。备注强调：`*_start` 前必须先 `work_mode_set` 和 `enable`，否则默认单 AP。
4. **回调铁律**：任何回调函数内部都不得再调用 Wi-Fi API；正确做法是 socketpair/管道 + 主循环 select 解耦。
5. **错误码体系统一**：`QL_ERR_OK` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_ABORT` / `QL_ERR_INVALID_ARG` / `QL_ERR_SERVICE_NOT_READY`，详细错误码见 `ql_type.h`。
6. **安全认证**：AP 与 STA 各有独立的 `ql_wifi_ap_auth_t` / `ql_wifi_sta_auth_t`，示例统一用 `QL_WIFI_AUTH_WPA_WPA2_PSK_BOTH` + `PAIRWISE_AUTO`。
7. **P2P 协商**：`go_intent` 0~15 决定 GO/Client；`pbc`/`pin` 两种 WPS 入网方式；GO 的 SSID 形如 `DIRECT-xx-<postfix>`。
8. **与 modem_mng 的关系**：本仓库当前 modem_mng 为 4G 拨号守护进程，未使用 Wi-Fi；本文档是 AG35 平台 Wi-Fi 能力扩展（热点/STA 中继/P2P 投屏）的一手 API 依据。

<!-- GENERATION_COMPLETE -->


