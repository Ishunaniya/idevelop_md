# EC200A-CN(TA) QuecOpen Wi-Fi API 参考手册（完整分析）

> **标题**：EC200A-CN(TA) QuecOpen Wi-Fi API 参考手册
> **适用模块系列**：LTE Standard 模块系列（EC200A-CN(TA)）
> **版本**：1.0.0
> **日期**：2023-01-10
> **状态**：临时文件（Preliminary / Not Checked）
> **发布方**：上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.）

> 本文件是对原 PDF《Quectel_EC200A-CN(TA)_QuecOpen_Wi-Fi_API_参考手册_V1.0.0_Preliminary_20230110.pdf》（共 81 页正文）的逐页、逐函数、逐结构体、逐枚举完整还原与整理，覆盖全部 67 个 API、所有数据结构与示例，不省略任何技术内容。

---

## 联系信息

**上海移远通信技术股份有限公司**
上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼　邮编：200233
电话：+86 21 5108 6236　邮箱：info@quectel.com
销售支持：http://www.quectel.com/cn/support/sales.htm
技术支持：http://www.quectel.com/cn/support/technical.htm　邮箱：support@quectel.com

---

## 前言 / 法律声明（摘要）

移远通信提供该文档内容以支持客户的产品设计。客户须按照文档中提供的规范、参数来设计产品。移远通信提供的参考设计仅作为示例，本文档及其所涉及服务在"可用"基础上提供。移远通信可在未事先通知的情况下随时增加、修改或重述本文档。

（含使用和披露限制、许可协议、版权声明、商标、第三方权利、隐私声明、免责声明等标准条款；与移远其他 QuecOpen 文档一致。）

版权所有 © 上海移远通信技术股份有限公司 2023，保留一切权利。
*Copyright © Quectel Wireless Solutions Co., Ltd. 2023.*

---

## 文档历史 / 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2023-01-10 | Sunshine HUANG | 文档创建 |
| 1.0.0 | 2023-01-10 | Sunshine HUANG | 临时版本 |

---

## 目录（原文结构）

1. 引言
2. Wi-Fi 模块功能（2.1 Wi-Fi 工作模式 / 2.2 Wi-Fi P2P 简介）
3. Wi-Fi API（3.1 头文件 / 3.2 函数概览 / 3.3 函数详解 共 67 个 API）
4. 示例（4.1 设置 STA 模式 / 4.2 设置单 AP 模式 / 4.3 Wi-Fi P2P 模式）
5. 附录 参考文档及术语缩写

---

## 1 引言

移远通信 EC200A-CN(TA) 模块支持 QuecOpen® 方案。QuecOpen® 是开源的、基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。有关 QuecOpen® 的详细信息，请参考**文档 [1]**（《Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导》）。

本文档主要介绍移远通信 EC200A-CN(TA) QuecOpen® 模块 Wi-Fi 功能相关的 API 及相关实例。

---

## 2 Wi-Fi 模块功能

EC200A-CN(TA) 模块需搭配移远通信 Wi-Fi 模块 **FC30R** 以实现 Wi-Fi 功能。有关该 Wi-Fi 模块的详细信息，请参考其对应的硬件设计手册。

### 2.1 Wi-Fi 工作模式

移远通信 Wi-Fi 模块 FC30R 支持两种工作模式：

1. **AP 模式**：AP 模式下，模块仅开放一个热点，支持 2.4 GHz。
2. **STA 模式**：STA 模式下，模块作为一个无线网卡使用，能够连接 2.4 GHz 频段的热点。

### 2.2 Wi-Fi P2P 简介

Wi-Fi 模块还支持 Wi-Fi 直连功能，即 Wi-Fi P2P 功能。

Wi-Fi P2P 是 Wi-Fi 联盟推出的一项重要技术规范。该规范也被称为 Wi-Fi Direct（Wi-Fi 直连），支持多个 Wi-Fi 设备在无 AP 的场景下相互连接，是 Wi-Fi Display（无线显示）功能的基础。在 Miracast（Wi-Fi 联盟对支持 Wi-Fi Display 功能的设备的认证名称）应用场景中，支持 P2P 的智能手机可直接连接支持 P2P 的智能电视，该智能手机可将本地视频或媒体资源传送到该智能电视进行显示或播放。

Wi-Fi P2P 结构中定义了三个组件，即一个设备、两个角色：

- **P2P Device**：P2P 架构中角色的实体，可作为一个 Wi-Fi 设备；
- **P2P Group Owner**：角色之一，类似于基础结构型基本服务集中 Infrastructure BSS 的 AP；
- **P2P Client**：角色之一，类似于基础结构型基本服务集中 Infrastructure BSS 的 STA。

组建 P2P Group（即 P2P Network）之前，终端均为独立的 P2P Device。所需 P2P Device 之间完成 P2P 协商后，有且只有一个 P2P Device 将作为 GO 角色，其他 P2P Device 作为 Client 的角色。

如下图 P2P 网络组织结构所示，一个 P2P Group 中只有一个 GO，一个 GO 支持一个或者多个 Client 连接。由于 GO 的功能类似于 AP，所以周围不支持 P2P 功能的 STA 也能发现并关联到 GO，这些 STA 称为 Legacy Client。

> **图 1：Wi-Fi P2P 组织结构图** —— `1:n P2P Group`：中心为 **P2P Group Owner**，与 **P2P Client** 和 **Legacy Client** 双向连接。

---

## 3 Wi-Fi API

### 3.1 头文件

Wi-Fi API 头文件为 **`ql_wifi.h`** 和 **`ql_wifi_common.h`**，位于 SDK 包的 `ql-sysroots/usr/include/ql-sdk/` 目录下。其中 `ql_wifi.h` 定义了 Wi-Fi 相关的 API，`ql_wifi_common.h` 定义了 Wi-Fi 相关的数据结构。若无特别说明，本文档所涉及头文件均在该目录下。

### 3.2 函数概览（表 1）

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
| `ql_wifi_ap_acl_mac_set()` | 为当前 MAC 地址访问规则添加或者删除指定的 MAC 地址 |
| `ql_wifi_ap_acl_mac_get()` | 获取当前 MAC 地址访问规则中所有的 MAC 地址 |
| `ql_wifi_ap_auth_set()` | 设置 AP 模式的安全认证 |
| `ql_wifi_ap_auth_get()` | 获取 AP 模式的安全认证信息 |
| `ql_wifi_ap_set_status_ind_cb()` | 注册 AP 模式状态回调函数 |
| `ql_wifi_ap_start()` | 开启 AP 功能 |
| `ql_wifi_ap_stop()` | 关闭 AP 功能 |
| `ql_wifi_ap_get_status()` | 获取 AP 模式的状态信息 |
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
| `ql_wifi_set_service_error_cb()` | 注册 Wi-Fi 服务异常回调函数 |
| `ql_wifi_set_ap_sta_connect_ind_cb()` | 注册 AP 模式和 STA 设备连接状态回调函数 |
| `ql_wifi_country_code_set()` | 设置 Wi-Fi 国家码 |
| `ql_wifi_country_code_get()` | 获取 Wi-Fi 国家码 |
| `ql_wifi_p2p_dev_name_set()` | 设置 Wi-Fi P2P 设备名称 |
| `ql_wifi_p2p_dev_name_get()` | 获取 Wi-Fi P2P 设备名称 |
| `ql_wifi_p2p_dev_type_set()` | 设置 Wi-Fi P2P 设备类型 |
| `ql_wifi_p2p_dev_type_get()` | 获取 Wi-Fi P2P 设备类型 |
| `ql_wifi_p2p_oper_class_channel_set()` | 设置 Wi-Fi P2P 设备的工作频率 |
| `ql_wifi_p2p_oper_class_channel_get()` | 获取 Wi-Fi P2P 设备的工作频率 |
| `ql_wifi_p2p_ssid_postfix_set()` | 设置 Wi-Fi P2P 模式的 SSID 后缀名 |
| `ql_wifi_p2p_ssid_postfix_get()` | 获取 Wi-Fi P2P 模式的 SSID 后缀名 |
| `ql_wifi_p2p_set_enable_status_ind_cb()` | 注册 Wi-Fi P2P 模式使能状态回调函数 |
| `ql_wifi_p2p_enable()` | 使能 Wi-Fi P2P 功能 |
| `ql_wifi_p2p_disable()` | 去使能 Wi-Fi P2P 功能 |
| `ql_wifi_p2p_set_dev_found_ind_cb()` | 注册处理扫描到的 P2P 设备信息的回调函数 |
| `ql_wifi_p2p_dev_find_start()` | 开始扫描周围的 P2P 设备 |
| `ql_wifi_p2p_dev_find_stop()` | 停止扫描周围的 P2P 设备 |
| `ql_wifi_p2p_get_status()` | 获取 Wi-Fi P2P 模式的连接状态信息 |
| `ql_wifi_p2p_set_status_ind_cb()` | 注册 Wi-Fi P2P 模式连接状态回调函数 |
| `ql_wifi_p2p_set_peer_dev_req_ind_cb()` | 注册对端 P2P 设备连接请求回调函数 |
| `ql_wifi_p2p_connect()` | 连接对端 P2P 设备 |
| `ql_wifi_p2p_disconnect()` | 断开 P2P 设备的连接 |

> **函数概览备注（重要使用前提）**：
> 1. 若无特别说明，本文档所述函数均**不支持并发调用**，并且**不能在相关回调函数中调用以上函数**，否则会对后续消息的处理造成影响。
> 2. 在调用 `ql_wifi_ap_start()` 或 `ql_wifi_sta_start()` 之前，须调用 `ql_wifi_work_mode_set()` 设置正确的 Wi-Fi 工作模式；若没有预先设置 Wi-Fi 工作模式，则默认的工作模式为**单 AP 模式**。
> 3. 在调用 `ql_wifi_ap_start()` 或 `ql_wifi_sta_start()` 之前，须调用 `ql_wifi_enable()` 使能 Wi-Fi。
> 4. 在调用 `ql_wifi_ap_start()` 或 `ql_wifi_sta_start()` 之前，用户可根据自身需求，调用其它配置接口进行相关的配置。

> **通用返回值约定**：`0` 成功；`-1034` 未初始化 Wi-Fi 服务；`-1099` Wi-Fi 服务出错；`-1010` 非法参数；其他值见 `ql_type.h`。`ql_wifi_init()` 特有 `-1067`（服务未准备就绪，建议重试）。下文各函数仅在与此不同处单独标注。

---

### 3.3 函数详解

#### 3.3.1 ql_wifi_init —— 初始化 Wi-Fi 服务
```c
int ql_wifi_init(void);
```
- **参数**：无
- **返回值**：`0` 成功；`-1067` Wi-Fi 服务未准备就绪，建议重试；其他值见 `ql_type.h`。
- **备注**：使用其它 Wi-Fi API 之前，必须先调用该函数初始化 Wi-Fi 服务。

#### 3.3.2 ql_wifi_deinit —— 去初始化 Wi-Fi 服务
```c
int ql_wifi_deinit(void);
```
- 参数：无。返回值：`0` 成功；其他值见 `ql_type.h`。

#### 3.3.3 ql_wifi_enable —— 开启 Wi-Fi 功能
```c
int ql_wifi_enable(void);
```
- 参数：无。返回值：`0`/`-1034`/`-1099`/其他。

#### 3.3.4 ql_wifi_disable —— 关闭 Wi-Fi 功能
```c
int ql_wifi_disable(void);
```
- 参数：无。返回值：`0`/`-1034`/`-1099`/其他。

#### 3.3.5 ql_wifi_set_enable_status_ind_cb —— 注册 Wi-Fi 状态回调
```c
int ql_wifi_set_enable_status_ind_cb(ql_wifi_enable_status_ind_cb_f cb);
```
- 参数：`cb` [In] Wi-Fi 状态回调函数。返回值：`0`/`-1034`/`-1099`/其他。

##### 3.3.5.1 ql_wifi_enable_status_ind_cb_f（回调原型）
```c
typedef void (*ql_wifi_enable_status_ind_cb_f)(QL_WIFI_ENABLE_STATUS_E pre_status,
                                               QL_WIFI_ENABLE_STATUS_E status);
```
- 参数：`pre_status` [In] 先前的 Wi-Fi 状态；`status` [In] 当前的 Wi-Fi 状态。返回值：无。

##### 3.3.5.2 QL_WIFI_ENABLE_STATUS_E（Wi-Fi 状态枚举）
```c
typedef enum QL_WIFI_STATUS_ENUM
{
    QL_WIFI_STATUS_DISABLED = 0,
    QL_WIFI_STATUS_ENABLED,
    QL_WIFI_STATUS_ERROR
} QL_WIFI_ENABLE_STATUS_E;
```
| 成员 | 描述 |
|---|---|
| `QL_WIFI_STATUS_DISABLED` | Wi-Fi 未使能 |
| `QL_WIFI_STATUS_ENABLED` | Wi-Fi 已使能 |
| `QL_WIFI_STATUS_ERROR` | Wi-Fi 使能出错 |

#### 3.3.6 ql_wifi_work_mode_set —— 设置 Wi-Fi 工作模式
```c
int ql_wifi_work_mode_set(QL_WIFI_WORK_MODE_E mode);
```
- 参数：`mode` [In] Wi-Fi 工作模式。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

##### 3.3.6.1 QL_WIFI_WORK_MODE_E（工作模式枚举）
```c
typedef enum QL_WIFI_WORK_MODE_ENUM
{
    QL_WIFI_WORK_MODE_MIN = -1,
    QL_WIFI_WORK_MODE_STA = 0,
    QL_WIFI_WORK_MODE_AP0,
    QL_WIFI_WORK_MODE_MAX
} QL_WIFI_WORK_MODE_E;
```
| 成员 | 描述 |
|---|---|
| `QL_WIFI_WORK_MODE_STA` | STA 模式 |
| `QL_WIFI_WORK_MODE_AP0` | 单 AP 模式 |

#### 3.3.7 ql_wifi_set_bridge —— 设置当前 Wi-Fi 加入的网桥
```c
int ql_wifi_set_bridge(char *bridge);
```
- 参数：`bridge` [In] 需加入的网桥名称，若不设置则默认使用网桥 `bridge0`。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.8 ql_wifi_work_mode_get —— 获取当前 Wi-Fi 工作模式
```c
int ql_wifi_work_mode_get(QL_WIFI_WORK_MODE_E *p_mode);
```
- 参数：`p_mode` [Out] 当前 Wi-Fi 工作模式。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.9 ql_wifi_ap_ssid_set —— 设置 AP 模式 SSID
```c
int ql_wifi_ap_ssid_set(QL_WIFI_AP_INDEX_E idx, const char *ssid);
```
- 参数：`idx` [In] AP 模式索引；`ssid` [In] SSID，**不可超过 32 字节，且不为空**。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

##### 3.3.9.1 QL_WIFI_AP_INDEX_E（AP 模式索引枚举）
```c
typedef enum QL_WIFI_INDEX_ENUM
{
    QL_WIFI_AP_INDEX_MIN = -1,
    QL_WIFI_AP_INDEX_AP0 = 0,
    QL_WIFI_AP_INDEX_MAX
} QL_WIFI_AP_INDEX_E;
```
| 成员 | 描述 |
|---|---|
| `QL_WIFI_AP_INDEX_AP0` | 热点 0 的索引 |

#### 3.3.10 ql_wifi_ap_ssid_get —— 获取 AP 模式 SSID
```c
int ql_wifi_ap_ssid_get(QL_WIFI_AP_INDEX_E idx, char *ssid_buf, int buf_len);
```
- 参数：`idx` [In] AP 索引；`ssid_buf` [Out] 指向存放当前 SSID 的缓存，**缓存大小建议设置为 33 字节**；`buf_len` [In] 缓存长度。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.11 ql_wifi_ap_ssid_hidden_set —— 设置 SSID 隐藏状态
```c
int ql_wifi_ap_ssid_hidden_set(QL_WIFI_AP_INDEX_E idx, int ssid_hidden);
```
- 参数：`idx` AP 索引；`ssid_hidden` [In] SSID 隐藏状态：`0` 不隐藏 SSID，`1` 隐藏 SSID。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.12 ql_wifi_ap_ssid_hidden_get —— 获取 SSID 隐藏状态
```c
int ql_wifi_ap_ssid_hidden_get(QL_WIFI_AP_INDEX_E idx, int *p_ssid_hidden);
```
- 参数：`idx` AP 索引；`p_ssid_hidden` [Out] `0` 不隐藏 / `1` 隐藏。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.13 ql_wifi_ap_mode_set —— 设置 AP 工作协议模式
```c
int ql_wifi_ap_mode_set(QL_WIFI_AP_INDEX_E idx, QL_WIFI_AP_MODE_TYPE_E mode);
```
- 参数：`idx` AP 索引；`mode` [In] 工作协议模式。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

##### 3.3.13.1 QL_WIFI_AP_MODE_TYPE_E（AP 工作协议模式枚举）
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
| 成员 | 描述 |
|---|---|
| `QL_WIFI_AP_MODE_80211B` | IEEE 802.11b（2.4 GHz） |
| `QL_WIFI_AP_MODE_80211BG` | IEEE 802.11b/g（2.4 GHz） |
| `QL_WIFI_AP_MODE_80211BGN` | IEEE 802.11b/g/n（2.4 GHz） |
| `QL_WIFI_AP_MODE_80211AX_2G` | IEEE 802.11ax（2.4 GHz） |

#### 3.3.14 ql_wifi_ap_mode_get —— 获取 AP 工作协议模式
```c
int ql_wifi_ap_mode_get(QL_WIFI_AP_INDEX_E idx, QL_WIFI_AP_MODE_TYPE_E *p_mode);
```
- 参数：`idx` AP 索引；`p_mode` [Out] 工作协议模式。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.15 ql_wifi_ap_bandwidth_set —— 设置 AP 带宽
```c
int ql_wifi_ap_bandwidth_set(QL_WIFI_AP_INDEX_E idx, QL_WIFI_BANDWIDTH_E bandwidth);
```
- 参数：`idx` AP 索引；`bandwidth` [In] AP 带宽。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

##### 3.3.15.1 QL_WIFI_BANDWIDTH_E（带宽类型枚举）
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
| 成员 | 描述 |
|---|---|
| `QL_WIFI_BANDWIDTH_20MHZ` | 20 MHz |
| `QL_WIFI_BANDWIDTH_40MHZ` | 40 MHz |
| `QL_WIFI_BANDWIDTH_80MHZ` | 80 MHz |
| `QL_WIFI_BANDWIDTH_160MHZ` | 160 MHz |

#### 3.3.16 ql_wifi_ap_bandwidth_get —— 获取 AP 带宽
```c
int ql_wifi_ap_bandwidth_get(QL_WIFI_AP_INDEX_E idx, QL_WIFI_BANDWIDTH_E *p_bandwidth);
```
- 参数：`idx` AP 索引；`p_bandwidth` [Out] AP 带宽。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.17 ql_wifi_ap_channel_set —— 设置 AP 信道
```c
int ql_wifi_ap_channel_set(QL_WIFI_AP_INDEX_E idx, int channel);
```
- 参数：`idx` AP 索引；`channel` [In] 待设置的信道（2.4 GHz）：`0` 自适应；`1/2/3/4/5/6/7/8/9/10/11/12/13/14` 2.4 GHz 信道。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.18 ql_wifi_ap_channel_get —— 获取 AP 信道
```c
int ql_wifi_ap_channel_get(QL_WIFI_AP_INDEX_E idx, int *p_channel);
```
- 参数：`idx` AP 索引；`p_channel` [Out] AP 当前使用的信道（`0` 自适应；`1~14` 2.4 GHz 信道）。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.19 ql_wifi_ap_max_sta_num_set —— 设置 AP 最大连接数
```c
int ql_wifi_ap_max_sta_num_set(QL_WIFI_AP_INDEX_E idx, int max_sta_num);
```
- 参数：`idx` AP 索引；`max_sta_num` [In] 终端最大连接数量，**取值范围 1~32**。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.20 ql_wifi_ap_max_sta_num_get —— 获取 AP 最大连接数
```c
int ql_wifi_ap_max_sta_num_get(QL_WIFI_AP_INDEX_E idx, int *p_max_sta_num);
```
- 参数：`idx` AP 索引；`p_max_sta_num` [Out] 当前最大连接数量。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.21 ql_wifi_ap_country_code_set —— 设置 AP 国家码
```c
int ql_wifi_ap_country_code_set(QL_WIFI_AP_INDEX_E idx, const char *country_code);
```
- 参数：`idx` AP 索引；`country_code` [In] 国家码，**默认值 CN（中国），最大长度 2 字节，不可为空**。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.22 ql_wifi_ap_country_code_get —— 获取 AP 国家码
```c
int ql_wifi_ap_country_code_get(QL_WIFI_AP_INDEX_E idx, char *country_code_buf, int buf_len);
```
- 参数：`idx` AP 索引；`country_code_buf` [Out] 指向存放国家码的缓存，**建议大小 3 字节**；`buf_len` [In] 缓存长度（字节）。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.23 ql_wifi_ap_isolate_set —— 设置 AP 隔离状态
```c
int ql_wifi_ap_isolate_set(QL_WIFI_AP_INDEX_E idx, int isolate);
```
- 参数：`idx` AP 索引；`isolate` [In] `0` AP 不隔离 / `1` AP 隔离。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.24 ql_wifi_ap_isolate_get —— 获取 AP 隔离状态
```c
int ql_wifi_ap_isolate_get(QL_WIFI_AP_INDEX_E idx, int *p_isolate);
```
- 参数：`idx` AP 索引；`p_isolate` [Out] `0` 不隔离 / `1` 隔离。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.25 ql_wifi_ap_mac_acl_rule_set —— 设置 MAC 地址访问规则
```c
int ql_wifi_ap_mac_acl_rule_set(QL_WIFI_AP_INDEX_E idx, QL_WIFI_MAC_ACL_RULE_E acl_rule);
```
- 参数：`idx` AP 索引；`acl_rule` [In] MAC 地址访问规则。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

##### 3.3.25.1 QL_WIFI_MAC_ACL_RULE_E（MAC 访问规则枚举）
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
| 成员 | 描述 |
|---|---|
| `QL_WIFI_MAC_ACL_RULE_NONE` | 无规则 |
| `QL_WIFI_MAC_ACL_RULE_BLACK` | 黑名单 |
| `QL_WIFI_MAC_ACL_RULE_WHITE` | 白名单 |

#### 3.3.26 ql_wifi_ap_mac_acl_rule_get —— 获取 MAC 访问规则
```c
int ql_wifi_ap_mac_acl_rule_get(QL_WIFI_AP_INDEX_E idx, QL_WIFI_MAC_ACL_RULE_E *p_acl_rule);
```
- 参数：`idx` AP 索引；`p_acl_rule` [Out] 当前 MAC 访问规则。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.27 ql_wifi_ap_acl_mac_set —— 为访问规则增删指定 MAC
```c
int ql_wifi_ap_acl_mac_set(QL_WIFI_AP_INDEX_E idx, QL_WIFI_ACL_MAC_CMD_E cmd, const char *macaddr);
```
- 参数：`idx` AP 索引；`cmd` [In] MAC 操作类型；`macaddr` [In] MAC 地址。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

##### 3.3.27.1 QL_WIFI_ACL_MAC_CMD_E（MAC 操作类型枚举）
```c
typedef enum
{
    QL_WIFI_ACL_MAC_CMD_DEL = 0,
    QL_WIFI_ACL_MAC_CMD_ADD
} QL_WIFI_ACL_MAC_CMD_E;
```
| 成员 | 描述 |
|---|---|
| `QL_WIFI_ACL_MAC_CMD_DEL` | 删除 MAC 地址 |
| `QL_WIFI_ACL_MAC_CMD_ADD` | 添加 MAC 地址 |

#### 3.3.28 ql_wifi_ap_acl_mac_get —— 获取访问规则中所有 MAC
```c
int ql_wifi_ap_acl_mac_get(QL_WIFI_AP_INDEX_E idx, ql_wifi_acl_mac_list_t *p_mac_list);
```
- 参数：`idx` AP 索引；`p_mac_list` [Out] MAC 地址列表。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

##### 3.3.28.1 ql_wifi_acl_mac_list_t（MAC 地址列表结构体）
```c
typedef struct
{
    int cnt;
    ql_wifi_mac_addr_t addr[QL_WIFI_MAX_ACL_MAC_CNT];
} ql_wifi_acl_mac_list_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| int | `cnt` | MAC 地址个数，最大不超过 32 |
| `ql_wifi_mac_addr_t` | `addr` | MAC 地址。`QL_WIFI_MAX_ACL_MAC_CNT` 定义在 `ql_wifi_common.h` 中，值为 **32** |

##### 3.3.28.2 ql_wifi_mac_addr_t（MAC 地址结构体）
```c
typedef struct
{
    char macaddr[18];
} ql_wifi_mac_addr_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| char | `macaddr` | MAC 地址，格式 `%02X:%02X:%02X:%02X:%02X:%02X`，例如 `2F:3F:4F:5F:6F:7F` |

#### 3.3.29 ql_wifi_ap_auth_set —— 设置 AP 安全认证
```c
int ql_wifi_ap_auth_set(QL_WIFI_AP_INDEX_E idx, ql_wifi_ap_auth_t *p_auth);
```
- 参数：`idx` AP 索引；`p_auth` [In] 安全认证信息。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

##### 3.3.29.1 ql_wifi_ap_auth_t（AP 安全认证结构体）
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
    };
} ql_wifi_ap_auth_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_WIFI_AUTH_E` | `auth` | 安全认证类型 |
| int | `default_index` | WEP 的配置索引 |
| char | `passwd`（wep） | WEP 的密码 |
| `QL_WIFI_AUTH_WPA_PSK_E` | `pairwise` | 加密方式 |
| char | `passwd`（wpa_psk） | WPA_PSK 的密码 |
| int | `group_rekey` | 组密钥更新周期 |

##### 3.3.29.2 QL_WIFI_AUTH_E（安全认证类型枚举）
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
| 成员 | 描述 |
|---|---|
| `QL_WIFI_AUTH_OPEN` | OPEN |
| `QL_WIFI_AUTH_WEP` | WEP |
| `QL_WIFI_AUTH_WPA_PSK` | WPA 个人版 |
| `QL_WIFI_AUTH_WPA2_PSK` | WPA2 个人版 |
| `QL_WIFI_AUTH_WPA3_PSK` | WPA3 个人版 |
| `QL_WIFI_AUTH_WPA_WPA2_PSK_BOTH` | WPA&WPA2 个人版 |
| `QL_WIFI_AUTH_WPA2_WPA3_PSK_BOTH` | WPA2&WPA3 个人版 |
| `QL_WIFI_AUTH_WPA_WPA2_WPA3_PSK_ALL` | WPA&WPA2&WPA3 个人版 |

##### 3.3.29.3 QL_WIFI_AUTH_WPA_PSK_E（加密方式枚举）
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
| 成员 | 描述 |
|---|---|
| `QL_WIFI_AUTH_WPA_PAIRWISE_AUTO` | 自动模式（包含 TKIP 和 AES） |
| `QL_WIFI_AUTH_WPA_PAIRWISE_TKIP` | TKIP |
| `QL_WIFI_AUTH_WPA_PAIRWISE_AES` | AES |

> **备注**：要使 Wi-Fi 设备连接时达到最大兼容性，建议在 **AP 工作模式下配置加密方式为 AES**；在 **STA 工作模式下配置加密模式为自动模式**。

#### 3.3.30 ql_wifi_ap_auth_get —— 获取 AP 安全认证
```c
int ql_wifi_ap_auth_get(QL_WIFI_AP_INDEX_E idx, ql_wifi_ap_auth_t *p_auth);
```
- 参数：`idx` AP 索引；`p_auth` [Out] 安全认证信息。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.31 ql_wifi_ap_set_status_ind_cb —— 注册 AP 状态回调
```c
int ql_wifi_ap_set_status_ind_cb(ql_wifi_ap_status_ind_cb_f cb);
```
- 参数：`cb` [In] AP 模式状态回调函数。返回值：`0`/`-1034`/`-1099`/其他。

##### 3.3.31.1 ql_wifi_ap_status_ind_cb_f（回调原型）
```c
typedef void (*ql_wifi_ap_status_ind_cb_f)(QL_WIFI_AP_INDEX_E index,
                                            QL_WIFI_AP_STATUS_E status,
                                            ql_wifi_ap_status_t *p_msg);
```
- 参数：`index` [In] AP 索引；`status` [In] 先前的 AP 模式状态；`p_msg` [In] 当前的 AP 模式状态等信息。返回值：无。

##### 3.3.31.2 QL_WIFI_AP_STATUS_E（AP 状态枚举）
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
| 成员 | 描述 |
|---|---|
| `QL_WIFI_AP_STATUS_NONE` | 未设置相应的 Wi-Fi 工作模式 |
| `QL_WIFI_AP_STATUS_IDLE` | 设置了相应的 Wi-Fi 工作模式，但未开启 AP 功能 |
| `QL_WIFI_AP_STATUS_ENABLING` | AP 模式启用中（暂不支持） |
| `QL_WIFI_AP_STATUS_ENABLED` | AP 模式已启用 |
| `QL_WIFI_AP_STATUS_DISABLING` | AP 模式禁用中（暂不支持） |
| `QL_WIFI_AP_STATUS_ERROR` | 出错状态 |

##### 3.3.31.3 ql_wifi_ap_status_t（AP 状态信息结构体）
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
| `QL_WIFI_AP_STATUS_E` | `status` | AP 模式的状态 |
| char | `ifname` | 接口名，例如 wlan0、wlan1 等 |
| char | `bssid` | BSSID，基本服务集标识符 |

#### 3.3.32 ql_wifi_ap_start —— 开启 AP 功能
```c
int ql_wifi_ap_start(QL_WIFI_AP_INDEX_E index);
```
- 参数：`index` [In] AP 索引。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.33 ql_wifi_ap_stop —— 关闭 AP 功能
```c
int ql_wifi_ap_stop(QL_WIFI_AP_INDEX_E index);
```
- 参数：`index` [In] AP 索引。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.34 ql_wifi_ap_get_status —— 获取 AP 状态信息
```c
int ql_wifi_ap_get_status(QL_WIFI_AP_INDEX_E index, ql_wifi_ap_status_t *p_sta);
```
- 参数：`index` [In] AP 索引；`p_sta` [Out] AP 状态信息。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.35 ql_wifi_sta_ssid_set —— 设置 STA SSID
```c
int ql_wifi_sta_ssid_set(const char *ssid);
```
- 参数：`ssid` [In] SSID，**不可超过 32 字节，且不为空**。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.36 ql_wifi_sta_ssid_get —— 获取 STA SSID
```c
int ql_wifi_sta_ssid_get(char *ssid_buf, int buf_len);
```
- 参数：`ssid_buf` [Out] 指向存放当前 SSID 的缓存；`buf_len` [In] 缓存长度。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.37 ql_wifi_sta_auth_set —— 设置 STA 安全认证
```c
int ql_wifi_sta_auth_set(ql_wifi_sta_auth_t *p_auth);
```
- 参数：`p_auth` [In] 安全认证信息。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

##### 3.3.37.1 ql_wifi_sta_auth_t（STA 安全认证结构体）
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
    };
} ql_wifi_sta_auth_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_WIFI_AUTH_E` | `auth` | 安全认证类型（见 3.3.29.2） |
| char | `passwd`（wep） | WEP 的密码 |
| `QL_WIFI_AUTH_WPA_PSK_E` | `pairwise` | 加密方式（见 3.3.29.3） |
| char | `passwd`（wpa_psk） | WPA_PSK 的密码 |

#### 3.3.38 ql_wifi_sta_auth_get —— 获取 STA 安全认证
```c
int ql_wifi_sta_auth_get(ql_wifi_sta_auth_t *p_auth);
```
- 参数：`p_auth` [Out] 安全认证信息。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.39 ql_wifi_sta_set_status_ind_cb —— 注册 STA 状态回调
```c
int ql_wifi_sta_set_status_ind_cb(ql_wifi_sta_status_ind_cb_f cb);
```
- 参数：`cb` [In] STA 模式状态回调函数。返回值：`0`/`-1034`/`-1099`/其他。

##### 3.3.39.1 ql_wifi_sta_status_ind_cb_f（回调原型）
```c
typedef void (*ql_wifi_sta_status_ind_cb_f)(QL_WIFI_STA_STATUS_E pre_status,
                                             ql_wifi_sta_status_t *p_msg);
```
- 参数：`pre_status` [In] 先前的 STA 模式状态；`p_msg` [In] 当前 STA 模式状态等信息。返回值：无。

##### 3.3.39.2 QL_WIFI_STA_STATUS_E（STA 状态枚举）
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
| 成员 | 描述 |
|---|---|
| `QL_WIFI_STA_STATUS_NONE` | 未设置相应的 Wi-Fi 工作模式 |
| `QL_WIFI_STA_STATUS_IDLE` | 设置了相应的 Wi-Fi 工作模式，但未开启 STA 功能 |
| `QL_WIFI_STA_STATUS_CONNECTING` | 正在连接 AP |
| `QL_WIFI_STA_STATUS_ASSOCIATED` | 已经连上 AP，但未分配 IP 地址 |
| `QL_WIFI_STA_STATUS_CONNECTED` | 已经连上 AP，并且已分配 IP 地址 |
| `QL_WIFI_STA_STATUS_DISCONNECTED` | 和 AP 断开连接 |
| `QL_WIFI_STA_STATUS_ERROR` | 出错状态 |

##### 3.3.39.3 ql_wifi_sta_status_t（STA 状态信息结构体）
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
    QL_WIFI_REASON_CODE_E sub_reason_code;
} ql_wifi_sta_status_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_WIFI_STA_STATUS_E` | `status` | STA 模式状态 |
| char | `ifname` | 网络接口名 |
| char | `ap_bssid` | BSSID，基本服务集标识符 |
| int | `rssi` | STA 设备连接热点时的信号强度；单位 dBm；`-9999` 表示无效值 |
| uint8_t | `has_addr` | 是否有 IPv4 地址 |
| `ql_net_addr_t` | `addr` | IPv4 地址，定义在 `ql_net_common.h` |
| uint8_t | `has_addr6` | 是否有 IPv6 地址 |
| `ql_net_addr6_t` | `addr6` | IPv6 地址，定义在 `ql_net_common.h` |
| `QL_WIFI_REASON_CODE_E` | `reason_code` | STA 错误原因码，定义在 `ql_wifi_common.h` |
| `QL_WIFI_REASON_CODE_E` | `sub_reason_code` | STA 错误子原因码，`QL_WIFI_SUB_REASON_CODE_E` 定义在 `ql_wifi_common.h` |

#### 3.3.40 ql_wifi_sta_start —— 开启 STA 功能
```c
int ql_wifi_sta_start(void);
```
- 参数：无。返回值：`0`/`-1034`/`-1099`/其他。
- **备注**：调用该函数前，须开启 Wi-Fi 功能，且将 Wi-Fi 工作模式设置为 STA 模式。

#### 3.3.41 ql_wifi_sta_stop —— 关闭 STA 功能
```c
int ql_wifi_sta_stop(void);
```
- 参数：无。返回值：`0`/`-1034`/`-1099`/其他。

#### 3.3.42 ql_wifi_sta_get_status —— 获取 STA 状态信息
```c
int ql_wifi_sta_get_status(ql_wifi_sta_status_t *p_sta);
```
- 参数：`p_sta` [Out] STA 模式的状态信息。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.43 ql_wifi_sta_start_scan —— STA 模式扫描外部热点
```c
int ql_wifi_sta_start_scan(void);
```
- 参数：无。返回值：`0`/`-1034`/`-1099`/其他。

#### 3.3.44 ql_wifi_sta_set_scan_result_ind_cb —— 注册扫描结果回调
```c
int ql_wifi_sta_set_scan_result_ind_cb(ql_wifi_sta_scan_result_ind_cb_f cb);
```
- 参数：`cb` [In] 扫描结果回调函数。返回值：`0`/`-1034`/`-1099`/其他。

##### 3.3.44.1 ql_wifi_sta_scan_result_ind_cb_f（回调原型）
```c
typedef void (*ql_wifi_sta_scan_result_ind_cb_f)(ql_wifi_sta_scan_result_t *pmsg);
```
- 参数：`pmsg` [In] 外部热点的信息列表。返回值：无。

##### 3.3.44.2 ql_wifi_sta_scan_result_t（扫描结果结构体）
```c
typedef struct ql_wifi_sta_scan_result_struct
{
    QL_WIFI_SCAN_REASON_CODE_E reason_code;
    ql_wifi_sta_scan_list_t scan_list;
} ql_wifi_sta_scan_result_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_WIFI_SCAN_REASON_CODE_E` | `reason_code` | 扫描结果原因码 |
| `ql_wifi_sta_scan_list_t` | `scan_list` | 扫描到的热点信息列表。当 `reason_code` 为 `QL_WIFI_SCAN_REASON_CODE_SUCCESS` 时该字段有效 |

##### 3.3.44.3 ql_wifi_sta_scan_list_t（热点信息列表结构体）
```c
typedef struct ql_wifi_sta_scan_list_struct
{
    int cnt;
    ql_wifi_sta_scan_info_t info[QL_WIFI_MAX_SCAN_INFO_CNT];
} ql_wifi_sta_scan_list_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| int | `cnt` | 扫描到的外部热点个数 |
| `ql_wifi_sta_scan_info_t` | `info` | 扫描的外部热点信息。`QL_WIFI_MAX_SCAN_INFO_CNT` 定义在 `ql_wifi_common.h`，值为 **60** |

##### 3.3.44.4 ql_wifi_sta_scan_info_t（外部热点信息结构体）
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
| char | `bssid` | BSSID，基本服务集标识符 |
| char | `essid` | 扩展服务集标识符 |
| int | `signal` | 信号强度。单位 dBm |
| int | `frequency` | 当前工作频率。单位 Hz |
| `QL_WIFI_AUTH_E` | `auth` | 认证方式（见 3.3.29.2） |

##### 3.3.44.5 QL_WIFI_SCAN_REASON_CODE_E（扫描结果原因码枚举）
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
| 成员 | 描述 |
|---|---|
| `QL_WIFI_SCAN_REASON_CODE_SUCCESS` | 扫描成功 |
| `QL_WIFI_SCAN_REASON_CODE_BUSY` | 扫描繁忙，建议重试 |
| `QL_WIFI_SCAN_REASON_CODE_BREAK_START` | 扫描被 `ql_wifi_sta_start()` 打断 |
| `QL_WIFI_SCAN_REASON_CODE_BREAK_STOP` | 扫描被 `ql_wifi_sta_stop()` 打断 |
| `QL_WIFI_SCAN_REASON_CODE_UNKNOW` | 扫描失败，原因未知 |

#### 3.3.45 ql_wifi_set_service_error_cb —— 注册服务异常回调
```c
int ql_wifi_set_service_error_cb(ql_wifi_service_error_cb_f cb);
```
- 参数：`cb` [In] Wi-Fi 服务异常回调函数。只有当 Wi-Fi 服务异常退出时，才会执行回调函数。返回值：`0`/`-1034`/`-1099`/其他。

##### 3.3.45.1 ql_wifi_service_error_cb_f（回调原型）
```c
typedef void (*ql_wifi_service_error_cb_f)(int error);
```
- 参数：`error` [In] 错误码。返回值：无。
- **备注**：Wi-Fi API 使用 **C/S（Client/Server）架构**。用户在调用 `ql_wifi_init()` 时，会创建与服务程序（`ql_wifid`）的通信连接；在调用其它 Wi-Fi API 时，则会发送请求给服务程序，服务程序处理完请求后返回处理结果给 Wi-Fi API。若服务程序出现异常退出，用户在调用 Wi-Fi API 时会返回 `-1099` 错误码（详见 `ql_type.h`）；用户也可调用 `ql_wifi_set_service_error_cb()` 注册服务程序异常的回调函数，用于监控服务程序的异常退出。在检测到服务程序异常退出后，用户的客户端程序可以先调用 `ql_wifi_deinit()` 注销 Wi-Fi 服务，再调用 `ql_wifi_init()` 创建和服务程序的通信连接，恢复 Wi-Fi 服务。

#### 3.3.46 ql_wifi_set_ap_sta_connect_ind_cb —— 注册 AP/STA 连接状态回调
```c
int ql_wifi_set_ap_sta_connect_ind_cb(ql_wifi_ap_sta_connect_ind_cb_f cb);
```
- 参数：`cb` [In] AP 模式和终端设备连接状态回调函数。返回值：`0`/`-1034`/`-1099`/其他。

##### 3.3.46.1 ql_wifi_ap_sta_connect_ind_cb_f（回调原型）
```c
typedef void (*ql_wifi_ap_sta_connect_ind_cb_f)(QL_WIFI_AP_INDEX_E index,
                                                 ql_wifi_sta_connect_status_t *pmsg);
```
- 参数：`index` [In] AP 索引；`pmsg` [In] AP 模式和终端设备的连接状态。返回值：无。

##### 3.3.46.2 ql_wifi_sta_connect_status_t（连接状态结构体）
```c
typedef struct ql_wifi_sta_connect_status_struct
{
    int is_connected;
    char macaddr[18];
} ql_wifi_sta_connect_status_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| int | `is_connected` | 连接状态：`1` 已连接；`0` 连接已断开 |
| char | `macaddr` | STA 设备的 MAC 地址 |

#### 3.3.47 ql_wifi_country_code_set —— 设置 Wi-Fi 国家码
```c
int ql_wifi_country_code_set(const char *country_code);
```
- 参数：`country_code` [In] 国家码，**默认值 CN（中国），最大长度 2 字节，不可为空**。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。
- **备注**：Wi-Fi 国家码默认为 CN，如需修改国家码，则需在调用 `ql_wifi_enable()` 开启 Wi-Fi 功能之前修改。

#### 3.3.48 ql_wifi_country_code_get —— 获取 Wi-Fi 国家码
```c
int ql_wifi_country_code_get(char *country_code_buf, int buf_len);
```
- 参数：`country_code_buf` [Out] 指向存储国家码的缓存，**建议大小 3 字节**；`buf_len` [In] 缓存长度（字节）。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.49 ql_wifi_p2p_dev_name_set —— 设置 P2P 设备名称
```c
int ql_wifi_p2p_dev_name_set(const char *dev_name);
```
- 参数：`dev_name` [In] P2P 设备名称。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.50 ql_wifi_p2p_dev_name_get —— 获取 P2P 设备名称
```c
int ql_wifi_p2p_dev_name_get(char *dev_name_buf, int buf_len);
```
- 参数：`dev_name_buf` [Out] 指向存储 P2P 设备名称的缓存；`buf_len` [In] 缓存长度（字节）。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.51 ql_wifi_p2p_dev_type_set —— 设置 P2P 设备类型
```c
int ql_wifi_p2p_dev_type_set(const char *dev_type);
```
- 参数：`dev_type` [In] P2P 设备类型。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。
- **备注（设备类型格式）**：主要设备类型格式 `<categ>-<OUI>-<subcateg>`：
  - `<categ>` 类别为整数值；
  - `<OUI>` 组织唯一标识符，四个八位字节的 16 进制编码数值；WPS OUI 默认为 `0050F204`；
  - `<subcateg>` OUI 特定的整型类型数值子类别。
  - 举例：`1-0050F204-1`（电脑/电脑）、`1-0050F204-2`（计算机/服务器）、`5-0050F204-1`（存储/NAS）、`6-0050F204-1`（网络基础设施/AP）。

#### 3.3.52 ql_wifi_p2p_dev_type_get —— 获取 P2P 设备类型
```c
int ql_wifi_p2p_dev_type_get(char *dev_type_buf, int buf_len);
```
- 参数：`dev_type_buf` [Out] 指向存储 P2P 设备类型的缓存；`buf_len` [In] 缓存长度（字节）。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.53 ql_wifi_p2p_oper_class_channel_set —— 设置 P2P 工作频率
```c
int ql_wifi_p2p_oper_class_channel_set(int oper_class, int oper_channel);
```
- 参数：`oper_class` [In] 待设置的 operating class；`oper_channel` [In] 待设置的 operating channel。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。
- **备注**：`oper_class` 和 `oper_channel` 共同限定 P2P 设备的工作频率，暂支持如下取值：

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

#### 3.3.54 ql_wifi_p2p_oper_class_channel_get —— 获取 P2P 工作频率
```c
int ql_wifi_p2p_oper_class_channel_get(int *p_oper_class, int *p_oper_channel);
```
- 参数：`p_oper_class` [Out] 存放当前 operating class 的缓存；`p_oper_channel` [Out] 存放当前 operating channel 的缓存。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.55 ql_wifi_p2p_ssid_postfix_set —— 设置 P2P SSID 后缀名
```c
int ql_wifi_p2p_ssid_postfix_set(const char *ssid_postfix);
```
- 参数：`ssid_postfix` [In] SSID 后缀名。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。
- **备注**：当 Wi-Fi P2P 为 GO 角色时，具有和 Wi-Fi AP 模式类似的功能，也存在 SSID，其格式为：`DIRECT-xx-ssid_postfix`，其中 `xx` 为随机产生的两个字母或者数字，`ssid_postfix` 为 SSID 的后缀名；其他 Wi-Fi STA 模式的设备可以扫描到该 SSID 并进行连接。

#### 3.3.56 ql_wifi_p2p_ssid_postfix_get —— 获取 P2P SSID 后缀名
```c
int ql_wifi_p2p_ssid_postfix_get(char *ssid_postfix_buf, int buf_len);
```
- 参数：`ssid_postfix_buf` [Out] 指向存放当前 SSID 后缀名的缓存；`buf_len` [In] 缓存长度（字节）。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

#### 3.3.57 ql_wifi_p2p_set_enable_status_ind_cb —— 注册 P2P 使能状态回调
```c
int ql_wifi_p2p_set_enable_status_ind_cb(ql_wifi_p2p_enable_status_ind_cb_f cb);
```
- 参数：`cb` [In] Wi-Fi P2P 模式使能状态回调函数。返回值：`0`/`-1034`/`-1099`/其他。

##### 3.3.57.1 ql_wifi_p2p_enable_status_ind_cb_f（回调原型）
```c
typedef void (*ql_wifi_p2p_enable_status_ind_cb_f)(QL_WIFI_P2P_ENABLE_STATUS_E pre_status,
                                                    QL_WIFI_P2P_ENABLE_STATUS_E status);
```
- 参数：`pre_status` [In] P2P 模式之前的使能状态；`status` [In] P2P 模式当前的使能状态。返回值：无。

##### 3.3.57.2 QL_WIFI_P2P_ENABLE_STATUS_E（P2P 使能状态枚举）
```c
typedef enum
{
    QL_WIFI_P2P_EN_STATUS_DISABLED = 0,
    QL_WIFI_P2P_EN_STATUS_ENABLED,
    QL_WIFI_P2P_EN_STATUS_ERROR,
} QL_WIFI_P2P_ENABLE_STATUS_E;
```
| 成员 | 描述 |
|---|---|
| `QL_WIFI_P2P_EN_STATUS_DISABLED` | Wi-Fi P2P 模式未使能 |
| `QL_WIFI_P2P_EN_STATUS_ENABLED` | Wi-Fi P2P 模式已使能 |
| `QL_WIFI_P2P_EN_STATUS_ERROR` | Wi-Fi P2P 模式使能出错 |

#### 3.3.58 ql_wifi_p2p_enable —— 使能 Wi-Fi P2P
```c
int ql_wifi_p2p_enable(void);
```
- 参数：无。返回值：`0`/`-1034`/`-1099`/其他。

#### 3.3.59 ql_wifi_p2p_disable —— 去使能 Wi-Fi P2P
```c
int ql_wifi_p2p_disable(void);
```
- 参数：无。返回值：`0`/`-1034`/`-1099`/其他。

#### 3.3.60 ql_wifi_p2p_set_dev_found_ind_cb —— 注册扫描到 P2P 设备的回调
```c
int ql_wifi_p2p_set_dev_found_ind_cb(ql_wifi_p2p_dev_found_ind_cb_f cb);
```
- 参数：`cb` [In] P2P 设备信息回调函数。返回值：`0`/`-1034`/`-1099`/其他。

##### 3.3.60.1 ql_wifi_p2p_dev_found_ind_cb_f（回调原型）
```c
typedef void (*ql_wifi_p2p_dev_found_ind_cb_f)(ql_wifi_p2p_found_dev_info_t *p_msg);
```
- 参数：`p_msg` [In] P2P 设备信息。返回值：无。

##### 3.3.60.2 ql_wifi_p2p_found_dev_info_t（P2P 设备信息结构体）
```c
typedef struct
{
    int is_found;
    char macaddr[18];
    char dev_type[32];
    char dev_name[32];
} ql_wifi_p2p_found_dev_info_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| int | `is_found` | P2P 设备信息：`1` 该 P2P 设备已扫描到，可连接该 P2P 设备；`0` 该 P2P 设备已丢失，不可连接该 P2P 设备，需重新扫描 |
| char | `macaddr` | P2P 设备 MAC 地址 |
| char | `dev_type` | P2P 设备类型 |
| char | `dev_name` | P2P 设备名称 |

#### 3.3.61 ql_wifi_p2p_dev_find_start —— 开始扫描周围 P2P 设备
```c
int ql_wifi_p2p_dev_find_start(void);
```
- 参数：无。返回值：`0`/`-1034`/`-1099`/其他。

#### 3.3.62 ql_wifi_p2p_dev_find_stop —— 停止扫描周围 P2P 设备
```c
int ql_wifi_p2p_dev_find_stop(void);
```
- 参数：无。返回值：`0`/`-1034`/`-1099`/其他。

#### 3.3.63 ql_wifi_p2p_get_status —— 获取 P2P 连接状态信息
```c
int ql_wifi_p2p_get_status(ql_wifi_p2p_status_t *p_status);
```
- 参数：`p_status` [Out] Wi-Fi P2P 模式的连接状态信息。返回值：`0`/`-1034`/`-1099`/`-1010`/其他。

##### 3.3.63.1 ql_wifi_p2p_status_t（P2P 连接状态信息结构体）
```c
typedef struct
{
    QL_WIFI_P2P_STATUS_E status;
    QL_WIFI_P2P_ROLE_E role;
    int freq;
    char ssid[33];
    char passwd[64];
    char pin_code[9];
    char ifname[32];
    uint8_t has_addr;
    ql_net_addr_t addr;
    uint8_t has_addr6;
    ql_net_addr6_t addr6;
    QL_WIFI_REASON_CODE_E reason_code;
    QL_WIFI_REASON_CODE_E sub_reason_code;
} ql_wifi_p2p_status_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| `QL_WIFI_P2P_STATUS_E` | `status` | Wi-Fi P2P 模式的连接状态 |
| `QL_WIFI_P2P_ROLE_E` | `role` | Wi-Fi P2P 模式的角色 |
| int | `freq` | Wi-Fi P2P 模式的工作频率 |
| char | `ssid` | Wi-Fi P2P 模式的 SSID（针对 GO 角色） |
| char | `passwd` | Wi-Fi P2P 模式的 SSID 密码（针对 GO 角色） |
| char | `pin_code` | 使用 PIN 方式主动连接对端 P2P 设备时，对端设备需要输入 `pin_code` 进行验证 |
| char | `ifname` | Wi-Fi P2P 模式 linux 网络接口名 |
| uint8_t | `has_addr` | 是否有 IPv4 地址（针对 client 角色） |
| `ql_net_addr_t` | `addr` | IPv4 地址，定义在 `ql_net_common.h`（针对 client 角色） |
| uint8_t | `has_addr6` | 是否有 IPv6 地址（针对 client 角色） |
| `ql_net_addr6_t` | `addr6` | IPv6 地址，定义在 `ql_net_common.h`（针对 client 角色） |
| `QL_WIFI_REASON_CODE_E` | `reason_code` | P2P 模式连接失败原因码，定义在 `ql_wifi_common.h` |
| `QL_WIFI_REASON_CODE_E` | `sub_reason_code` | P2P 模式连接失败子原因码，`QL_WIFI_SUB_REASON_CODE_E` 定义在 `ql_wifi_common.h` |

##### 3.3.63.2 QL_WIFI_P2P_STATUS_E（P2P 连接状态枚举）
```c
typedef enum
{
    QL_WIFI_P2P_STATUS_IDLE,
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
| `QL_WIFI_P2P_STATUS_ASSOCIATED` | 如果 CONNECTING 过程中协商的是 client 角色，则该状态表示已经连上对端 P2P 设备，但未分配 IP 地址；如果为 GO 角色，则没有该状态 |
| `QL_WIFI_P2P_STATUS_CONNECTED` | 已经连上对端 P2P 设备，且 IP 地址已分配 |
| `QL_WIFI_P2P_STATUS_DISCONNECTED` | 断开 P2P 连接 |
| `QL_WIFI_P2P_STATUS_ERROR` | P2P 连接出错 |

##### 3.3.63.3 QL_WIFI_P2P_ROLE_E（P2P 角色枚举）
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

#### 3.3.64 ql_wifi_p2p_set_status_ind_cb —— 注册 P2P 连接状态回调
```c
int ql_wifi_p2p_set_status_ind_cb(ql_wifi_p2p_status_ind_cb_f cb);
```
- 参数：`cb` [In] Wi-Fi P2P 模式连接状态回调函数。返回值：`0`/`-1034`/`-1099`/其他。

##### 3.3.64.1 ql_wifi_p2p_status_ind_cb_f（回调原型）
```c
typedef void (*ql_wifi_p2p_status_ind_cb_f)(QL_WIFI_P2P_STATUS_E pre_status,
                                             ql_wifi_p2p_status_t *p_msg);
```
- 参数：`pre_status` [In] Wi-Fi P2P 模式之前的连接状态；`p_msg` [In] Wi-Fi P2P 模式当前的连接状态信息。返回值：无。

#### 3.3.65 ql_wifi_p2p_set_peer_dev_req_ind_cb —— 注册对端 P2P 连接请求回调
```c
int ql_wifi_p2p_set_peer_dev_req_ind_cb(ql_wifi_p2p_peer_dev_req_ind_cb_f cb);
```
- 参数：`cb` [In] 对端 P2P 设备连接请求回调函数。返回值：`0`/`-1034`/`-1099`/其他。

##### 3.3.65.1 ql_wifi_p2p_peer_dev_req_ind_cb_f（回调原型）
```c
typedef void (*ql_wifi_p2p_peer_dev_req_ind_cb_f)(ql_wifi_p2p_req_peer_dev_info_t *pmsg);
```
- 参数：`pmsg` [In] 对端 P2P 设备信息。返回值：无。

##### 3.3.65.2 ql_wifi_p2p_req_peer_dev_info_t（对端 P2P 设备信息结构体）
```c
typedef struct
{
    char macaddr[18];
} ql_wifi_p2p_req_peer_dev_info_t;
```
| 类型 | 参数 | 描述 |
|---|---|---|
| char | `macaddr` | 对端 P2P 设备 MAC 地址 |

#### 3.3.66 ql_wifi_p2p_connect —— 连接对端 P2P 设备
```c
int ql_wifi_p2p_connect(const char *macaddr, const char *method, int go_intent);
```
- 参数：
  - `macaddr` [In] 对端 P2P 设备的 MAC 地址。
  - `method` [In] P2P 连接方式，取值 `pbc` 或 `pin`：当本地 P2P 设备主动使用 pin 方式发起连接，本地设备会产生一个 pin 码，对端设备连接时，要输入这个 pin 码；本地设备使用 pbc 方式发起连接时，对端设备可以直接连。当对端设备使用 pin 方式主动连接本地 P2P 设备时，则该参数为对端 P2P 设备产生的 pin 码。
  - `go_intent` [In] 成为 GO 角色的渴望程度，整数，取值范围 0~15，值越大越渴望成为 GO 角色。一般情况下，要成为 client 角色则设为 `0`，要成为 GO 角色则设为 `15`。
- 返回值：`0`/`-1034`/`-1099`/其他。

#### 3.3.67 ql_wifi_p2p_disconnect —— 断开 P2P 连接
```c
int ql_wifi_p2p_disconnect(void);
```
- 参数：无。返回值：`0`/`-1034`/`-1099`/其他。

---

## 4 示例

Wi-Fi API 示例代码请参考 `sample/test_sdk_api/m_wifi.c`。本章主要介绍如何使用 Wi-Fi API 设置 STA 模式、单 AP 模式以及 P2P 模式。

### 4.1 设置 STA 模式

```c
/** 定义 Wi-Fi 服务最大初始化次数 */
#define WIFI_INIT_MAX_TRY_CNT 50

int main()
{
    int ret = QL_ERR_OK;
    int wifi_is_init = 0;
    int wifi_init_try_cnt = 0;
    ql_wifi_sta_auth_t auth;

    /** Wi-Fi 服务初始化 */
    while (WIFI_INIT_MAX_TRY_CNT > wifi_init_try_cnt)
    {
        ret = ql_wifi_init();
        if (QL_ERR_SERVICE_NOT_READY == ret)
        {
            printf("wifi service is not ready, try again, try count = %d\n", (wifi_init_try_cnt + 1));
            usleep(200 * 1000);
            wifi_init_try_cnt++;
        }
        else if (QL_ERR_OK == ret)
        {
            printf("Succeeded to initialize wifi service\n");
            wifi_is_init = 1;
            break;
        }
        else
        {
            printf("Failed to initialize wifi service, ret = %d\n", ret);
            break;
        }
    }

    if (1 != wifi_is_init)
    {
        printf("Failed to initialize wifi service\n");
        return -1;
    }

    /** 使能 Wi-Fi */
    ret = ql_wifi_enable();
    if (QL_ERR_OK != ret)
    {
        printf("Failed to enable wifi, ret = %d", ret);
        return -1;
    }

    /** 设置 Wi-Fi 工作模式为 STA 模式 */
    ret = ql_wifi_work_mode_set(QL_WIFI_WORK_MODE_STA);
    if (QL_ERR_OK != ret)
    {
        printf("Failed to set wifi work mode, ret = %d", ret);
        return -1;
    }

    /** 设置 SSID */
    ret = ql_wifi_sta_ssid_set("Quectel-Hf");
    if (QL_ERR_OK != ret)
    {
        printf("Failed to set ssid, ret = %d", ret);
        return -1;
    }

    /** 设置 STA 模式安全认证 */
    memset(&auth, 0, sizeof(ql_wifi_sta_auth_t));
    auth.auth = QL_WIFI_AUTH_WPA_WPA2_PSK_BOTH;
    auth.wpa_psk.pairwise = QL_WIFI_AUTH_WPA_PAIRWISE_AUTO;
    strncpy(auth.wpa_psk.passwd, "quectel-i-hf", sizeof(auth.wpa_psk.passwd) - 1);

    ret = ql_wifi_sta_auth_set(&auth);
    if (QL_ERR_OK != ret)
    {
        printf("Failed to set wifi auth, ret = %d", ret);
        return -1;
    }

    /** 开启 STA 模式 */
    ret = ql_wifi_sta_start();
    if (QL_ERR_OK != ret)
    {
        printf("Failed to start sta, ret = %d", ret);
        ql_wifi_disable();
        return -1;
    }

    return 0;
}
```

### 4.2 设置单 AP 模式

```c
/** 定义 Wi-Fi 服务最大初始化次数 */
#define WIFI_INIT_MAX_TRY_CNT 50

int main()
{
    int ret = QL_ERR_OK;
    int wifi_is_init = 0;
    int wifi_init_try_cnt = 0;
    ql_wifi_ap_auth_t auth;

    /** Wi-Fi 服务初始化（同 4.1，循环 ql_wifi_init 重试） */
    while (WIFI_INIT_MAX_TRY_CNT > wifi_init_try_cnt)
    {
        ret = ql_wifi_init();
        if (QL_ERR_SERVICE_NOT_READY == ret)
        {
            printf("wifi service is not ready, try again, try count = %d\n", (wifi_init_try_cnt + 1));
            usleep(200 * 1000);
            wifi_init_try_cnt++;
        }
        else if (QL_ERR_OK == ret)
        {
            printf("Succeeded to initialize wifi service\n");
            wifi_is_init = 1;
            break;
        }
        else
        {
            printf("Failed to initialize wifi service, ret = %d\n", ret);
            break;
        }
    }

    if (1 != wifi_is_init)
    {
        printf("Failed to initialize wifi service\n");
        return -1;
    }

    /** 使能 Wi-Fi */
    ret = ql_wifi_enable();
    if (QL_ERR_OK != ret)
    {
        printf("Failed to enable wifi, ret = %d", ret);
        return -1;
    }

    /** 设置 Wi-Fi 工作模式为单 AP 模式 */
    ret = ql_wifi_work_mode_set(QL_WIFI_WORK_MODE_AP0);
    if (QL_ERR_OK != ret)
    {
        printf("Failed to set wifi work mode, ret = %d", ret);
        return -1;
    }

    /** 设置信道，带宽等配置此处省略 */
    // ……

    /** 设置 SSID */
    ret = ql_wifi_ap_ssid_set("Quectel-AP0");
    if (QL_ERR_OK != ret)
    {
        printf("Failed to set ssid, ret = %d", ret);
        return -1;
    }

    /** 设置 AP 模式安全认证 */
    memset(&auth, 0, sizeof(ql_wifi_ap_auth_t));
    auth.auth = QL_WIFI_AUTH_WPA_WPA2_PSK_BOTH;
    auth.wpa_psk.pairwise = QL_WIFI_AUTH_WPA_PAIRWISE_AUTO;
    strncpy(auth.wpa_psk.passwd, "123456789", sizeof(auth.wpa_psk.passwd) - 1);

    ret = ql_wifi_ap_auth_set(QL_WIFI_AP_INDEX_AP0, &auth);
    if (QL_ERR_OK != ret)
    {
        printf("Failed to set wifi auth, ret = %d", ret);
        return -1;
    }

    /** 开启 AP 模式 */
    ret = ql_wifi_ap_start(QL_WIFI_AP_INDEX_AP0);
    if (QL_ERR_OK != ret)
    {
        printf("Failed to start ap[%d], ret = %d", QL_WIFI_AP_INDEX_AP0, ret);
        ql_wifi_disable();
        return -1;
    }

    return 0;
}
```

### 4.3 Wi-Fi P2P 模式

```c
/** 定义 Wi-Fi 服务最大初始化次数 */
#define WIFI_INIT_MAX_TRY_CNT 50

void wifi_p2p_dev_found_ind_cb(ql_wifi_p2p_found_dev_info_t *p_msg)
{
    /** 将 p2p 设备信息发给主进程，可以使用 socketpair */
}

int main()
{
    int ret = QL_ERR_OK;
    int wifi_is_init = 0;
    int wifi_init_try_cnt = 0;
    ql_wifi_sta_auth_t auth;

    /** Wi-Fi 服务初始化（同 4.1，循环 ql_wifi_init 重试） */
    while (WIFI_INIT_MAX_TRY_CNT > wifi_init_try_cnt)
    {
        ret = ql_wifi_init();
        if (QL_ERR_SERVICE_NOT_READY == ret)
        {
            printf("wifi service is not ready, try again, try count = %d\n", (wifi_init_try_cnt + 1));
            usleep(200 * 1000);
            wifi_init_try_cnt++;
        }
        else if (QL_ERR_OK == ret)
        {
            printf("Succeeded to initialize wifi service\n");
            wifi_is_init = 1;
            break;
        }
        else
        {
            printf("Failed to initialize wifi service, ret = %d\n", ret);
            break;
        }
    }

    if (1 != wifi_is_init)
    {
        printf("Failed to initialize wifi service\n");
        return -1;
    }

    /** 使能 Wi-Fi */
    ret = ql_wifi_enable();
    if (QL_ERR_OK != ret)
    {
        printf("Failed to enable wifi, ret = %d", ret);
        return -1;
    }

    /** 设置 P2P 设备名称 */
    ret = ql_wifi_p2p_dev_name_set("Quectel-P2P");
    if (QL_ERR_OK != ret)
    {
        printf("Failed to set ssid, ret = %d", ret);
        return -1;
    }

    /** 其他配置接口省略…… */

    ret = ql_wifi_p2p_enable();
    if (QL_ERR_OK != ret)
    {
        printf("Failed to enable p2p, ret = %d", ret);
        ql_wifi_disable();
        return -1;
    }

    ret = ql_wifi_p2p_set_dev_found_ind_cb(p2p_dev_found_ind_cb);
    if (QL_ERR_OK != ret)
    {
        printf("Failed to set dev found ind cb, ret = %d", ret);
        ql_wifi_p2p_disable();
        ql_wifi_disable();
        return -1;
    }

    ret = ql_wifi_p2p_dev_find_start();
    if (QL_ERR_OK != ret)
    {
        printf("Failed to start finding p2p dev, ret = %d", ret);
        ql_wifi_p2p_disable();
        ql_wifi_disable();
        return -1;
    }

    /** 等待 wifi_p2p_dev_found_ind_cb 上报对端 P2P 设备信息，可以使用 select 机制 */

    /** 并连接对端 P2P 设备 */
    ret = ql_wifi_p2p_connect("对端设备 MAC 地址", "pbc", 15);
    if (QL_ERR_OK != ret)
    {
        printf("Failed to connect p2p, ret = %d", ret);
        ql_wifi_p2p_disable();
        ql_wifi_disable();
        return -1;
    }

    return 0;
}
```

---

## 5 附录 参考文档及术语缩写

**表 2：参考文档**

| 文档名称 |
|---|
| [1] Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

**表 3：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| AES | Advanced Encryption Standard | 高级加密标准 |
| AP | Access Point | 接入点 |
| API | Application Programming Interface | 应用程序编程接口 |
| BSS | Base Station Subsystem | 基站子系统 |
| BSSID | Basic Service Set Identifier | 基本服务标识符 |
| IoT | Internet of Things | 物联网 |
| IPv4 | Internet Protocol version 4 | 第 4 版互联网协议 |
| IPv6 | Internet Protocol version 6 | 第 6 版互联网协议 |
| MAC | Media Access Control (Address) | 媒体访问控制（地址） |
| PSK | Pre-Shared Key | 预共享的密钥 |
| P2P | Peer to Peer | 点对点 |
| SDK | Software Development Kit | 软件开发工具包 |
| SSID | Service Set Identifier | 服务集标识符 |
| STA | Station | 站点 |
| TKIP | Temporal Key Integrity Protocol | 临时密钥完整性协议 |
| WEP | Wired Equivalent Privacy | 有线等效保密 |
| Wi-Fi | Wireless Fidelity | 无线网络 |
| WPA | Wi-Fi Protected Access | Wi-Fi 网络安全接入 |

---

## 关键要点速记（Wi-Fi 开发备忘）

- **硬件依赖**：EC200A-CN(TA) 需外挂移远 **FC30R** Wi-Fi 模块；支持 **AP / STA / P2P** 三种用法（AP/STA 均 2.4 GHz）。
- **头文件**：`ql_wifi.h`（API）+ `ql_wifi_common.h`（数据结构），位于 `ql-sysroots/usr/include/ql-sdk/`。
- **架构**：C/S 架构，服务程序 `ql_wifid`；`ql_wifi_init` 建连，异常时 API 返回 `-1099`，可注册 `ql_wifi_set_service_error_cb` 监控，恢复用 `deinit→init`。
- **调用顺序（关键）**：`ql_wifi_init`（可能返回 `-1067` 需重试，示例重试 50 次/200ms）→ `ql_wifi_enable` → `ql_wifi_work_mode_set`（不设则默认单 AP）→ 各项配置 → `ql_wifi_ap_start` / `ql_wifi_sta_start`。**不可并发调用、不可在回调里调 API**。
- **通用返回值**：`0` 成功 / `-1034` 未初始化 / `-1099` 服务出错 / `-1010` 非法参数。
- **SSID**：≤ 32 字节非空；获取缓存建议 33 字节。**国家码**默认 CN（≤2 字节），改需在 enable 前；缓存建议 3 字节。
- **最大连接数** 1~32；**信道** 0 自适应或 1~14；**带宽** 20/40/80/160 MHz；**AP 协议** 802.11b/bg/bgn/ax_2g。
- **加密兼容建议**：AP 配 AES，STA 配 AUTO。认证支持 OPEN/WEP/WPA/WPA2/WPA3 及组合 PSK。
- **MAC ACL**：规则 NONE/黑名单/白名单；`ql_wifi_ap_acl_mac_set` 增删（ADD/DEL），最多 32 条；MAC 格式 `XX:XX:XX:XX:XX:XX`。
- **P2P**：角色 GO/Client；`go_intent` 0~15（15=最想当 GO，0=当 client）；连接方式 `pbc`/`pin`；GO 的 SSID 形如 `DIRECT-xx-<postfix>`；设备类型格式 `<categ>-<OUI>-<subcateg>`（WPS OUI 默认 0050F204）。
- **状态机回调**：Wi-Fi/AP/STA/P2P 各有 enable_status 与 status 回调、扫描结果回调、AP-STA 连接回调、对端请求回调。
- **示例位置**：`sample/test_sdk_api/m_wifi.c`。
