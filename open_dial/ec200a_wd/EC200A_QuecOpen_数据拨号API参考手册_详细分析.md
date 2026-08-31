# EC200A-CN(TA) QuecOpen 数据拨号 API 参考手册 — 详细分析

> **文档来源**：Quectel EC200A-CN(TA) QuecOpen 数据拨号 API 参考手册 V1.0.0 Preliminary（2023-04-11）
> **适用模块**：移远通信 LTE Standard EC200A-CN(TA) 系列模块
> **文档状态**：临时文件（Preliminary Document, Not Checked）
> **作者**：Ryder WANG / Allen FENG
> **分析整理**：基于原始 PDF 全文逐章逐节归纳，无内容遗漏

---

## 目录

1. [文档基本信息](#1-文档基本信息)
2. [引言](#2-引言)
3. [数据拨号原理](#3-数据拨号原理)
   - 3.1 [拨号流程](#31-拨号流程)
   - 3.2 [重连机制](#32-重连机制)
   - 3.3 [拨号状态](#33-拨号状态)
   - 3.4 [异常处理](#34-异常处理)
4. [数据拨号 API](#4-数据拨号-api)
   - 4.1 [头文件](#41-头文件)
   - 4.2 [函数概览](#42-函数概览)
   - 4.3 [函数详解](#43-函数详解)
5. [完整使用示例](#5-完整使用示例)
6. [注意事项](#6-注意事项)
7. [附录：参考文档与术语缩写](#7-附录参考文档与术语缩写)

---

## 1. 文档基本信息

| 项目 | 内容 |
|------|------|
| 产品系列 | LTE Standard 模块系列 |
| 模块型号 | EC200A-CN(TA) |
| 开发平台 | QuecOpen®（基于 Linux 的嵌入式开发平台） |
| 文档版本 | V1.0.0 |
| 文档日期 | 2023-04-11 |
| 文档状态 | 临时文件（Preliminary Document, Not Checked） |
| 厂商 | 上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.） |
| 联系方式 | 上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼，邮编 200233 |
| 电话 | +86 21 5108 6236 |
| 邮箱 | info@quectel.com |
| 技术支持 | support@quectel.com |

### 文档历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|---------|
| - | 2021-12-10 | Ryder WANG | 文档创建 |
| 1.0.0 | 2023-04-11 | Ryder WANG / Allen FENG | 临时版本 |

### 免责与授权声明摘要

- 文档内容仅供客户产品设计参考，设计时需结合自身独立分析判断。
- 未经移远通信书面同意，不得复制、转载、翻译或创建衍生作品。
- 移远通信不承担因文档信息不准确或遗漏所产生的任何责任。
- 部分设备数据可能上传至移远通信或第三方服务器（运营商、芯片供应商等），仅用于实现产品功能。
- 版权所有 © 上海移远通信技术股份有限公司 2023，保留一切权利。

---

## 2. 引言

移远通信 LTE Standard EC200A-CN(TA) 模块支持 **QuecOpen®** 方案。QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。

**数据拨号（Data Call）** 是指无线数据业务的建立过程：

- 设备携带 APN 信息向运营商网络侧发起数据拨号请求；
- 运营商网络侧根据 APN 信息建立数据链路到相应网络；
- 拨号成功后设备获得 IP 地址及相关网络参数（网关、DNS 等）。

本文档介绍数据拨号的完整过程，以及 SDK 中用于拨号的 API。

---

## 3. 数据拨号原理

### 3.1 拨号流程

运营商在签约时为 (U)SIM 卡分配一个或多个 APN 信息，常见的是一路公网 APN 以及一路或多路私网 APN。模块携带 APN 信息进行数据拨号时，运营商网络侧根据 APN 信息分配 IP 地址以及网络。

```
                ┌──────────┐   ┌──────────┐   ┌──────────┐
                │  ISP网络1 │   │  ISP网络2 │   │  ISP网络3 │
                └────┬─────┘   └────┬─────┘   └────┬─────┘
                     │              │              │
                ┌────┴──────────────┴──────────────┴────┐
                │              移动网络基站               │
                └────────────────────────────────────────┘
                     ↑              ↑              ↑
          ┌──────────┴──────────────┴──────────────┴───────────┐
          │  APN信息1        APN信息2        APN信息3           │
          │  (APN名称        (APN名称        (APN名称           │
          │   IP版本          IP版本          IP版本            │
          │   认证类型        认证类型        认证类型           │
          │   用户名          用户名          用户名             │
          │   密码)           密码)           密码)             │
          │                                                     │
          │  数据拨号1       数据拨号2       数据拨号3          │
          └─────────────────────────────────────────────────────┘
                              EC200A 模块
```

**数据拨号成功的前提**：模块已注册到运营商网络。

> **注意**：LTE 网络注册过程中需要建立默认承载。建立默认承载需要传入 APN 信息，若 APN 信息错误，可能导致网络注册失败；运营商分配的 APN 信息中，有一路 APN 可用于建立默认承载，详情需咨询运营商。

模块首次检测到 (U)SIM 卡时，会根据 (U)SIM 卡的 PLMN 信息默认加载一组常用的 APN 信息，可通过 `AT+CGDCONT?` 查看。

**模块最多支持 4 路 APN**：
- **APN1**：用于建立默认承载
- **APN2 ~ APN4**：用于数据拨号（推荐使用此范围）
- 每路数据拨号对应一路 APN

**推荐的拨号流程**：

| 步骤 | 操作 |
|------|------|
| 步骤 1 | 设置默认承载 APN 信息（APN1） |
| 步骤 2 | 设置拨号使用的 APN 信息，配置 APN 名称、IP 类型等 |
| 步骤 3 | 使用步骤 2 配置的 APN 进行数据拨号 |

> **注意**：在发起数据拨号时，模块不会判断是否已注册网络，所以在网络未注册时发起数据拨号会失败，这是正常现象。需先注册网络，再进行数据拨号。

### 3.2 重连机制

拨号失败或拨号断开后，可通过以下两个函数配置自动重连：

- `ql_data_call_param_set_reconnect_mode()` — 设置自动重连模式
- `ql_data_call_param_set_reconnect_interval()` — 设置重试时间间隔

共支持 **4 种重连模式**：

#### DISABLE（禁用）

不自动重连，需手动重新拨号。

#### NORMAL（普通定时重连）

根据设置的时间间隔进行重连。

> **建议设置 20 秒以上**，频繁拨号可能会被运营商加入黑名单。

#### MODE1（时间退避重连）

- 时间间隔列表为 T1、T2、T3……Tn
- 重连时间间隔依次为：T1、T2、T3……Tn、Tn、Tn……Tn，直至拨号成功
- 若需要再次重连，则间隔时间从 T1 重新开始

#### MODE2（循环时间退避重连）

- 时间间隔列表为 T1、T2、T3……Tn
- 重连时间间隔依次为：T1、T2、T3……Tn、T1、T2、T3……Tn……，直至拨号成功
- 若需要再次重连，则间隔时间从 T1 重新开始

### 3.3 拨号状态

数据拨号过程包含以下多种状态，整个生命周期如下：

| 状态 | 说明 |
|------|------|
| **NONE** | 数据拨号实例不存在 |
| **CREATE** | 数据拨号实例被创建 |
| **IDLE** | 数据拨号已经被配置（空闲，等待拨号） |
| **CONNECTING** | 正在进行拨号 |
| **CONNECTED** | 数据拨号成功，已获取到 IP 地址等信息 |
| **DISCONNECTED** | 数据拨号失败或者断开 |
| **DELETE** | 数据拨号实例被删除 |

**状态转换图（Data Call State Transition）**：

```
NONE ──CREATE_REQ──► CREATE ──CONFIG_REQ──► IDLE ──START_REQ──► CONNECTING
                                                                    │
                                               ◄──Data call succeed─┤ → CONNECTED
                                               ◄──Data call failed──┤ → DISCONNECTED
                                               ◄──Network disconnect┤ → DISCONNECTED
                    ◄──────────────────STOP_REQ──────────────────────┤
                    ◄──────────────────STOP_REQ────────────────────────
                    ◄──DELETE_REQ─────────────────────────────────────
                                        ◄──DELETE_REQ─────────────────
                                                      ◄──DELETE_REQ───
                                                                ◄──DELETE_REQ
                                        [Reconnect enable 时自动返回 CONNECTING]
```

> 其中，CONNECTED 为拨号成功状态，在此状态下可获取 IP 地址等信息；DISCONNECTED 为拨号失败或拨号断开，在此状态下可获取拨号错误码。拨号错误码定义在头文件 `ql-sysroots/usr/include/qmi/wireless_data_service_v01.h` 下。

### 3.4 异常处理

数据拨号 API 接口使用 **C/S（Client/Server）方式**实现：

- API 接口在初始化时，创建与服务程序的通信连接；
- 进行 API 接口调用时，API 接口发送请求给服务程序，服务程序处理完成后返回结果给 API 接口；
- **如果服务程序出现异常退出**，调用 API 接口时会返回 `QL_ERR_ABORTED`；
- 用户可以调用 `ql_data_call_set_service_error_cb()` 注册服务异常的回调函数，用以监控服务程序的异常退出；
- 在检测到服务异常退出后，用户可以先调用 `ql_data_call_deinit()` 注销接口，然后再调用 `ql_data_call_init()` 重启数据拨号服务，进行业务恢复。

---

## 4. 数据拨号 API

### 4.1 头文件

API 头文件位于 `ql-sysroots/usr/include/ql-sdk` 目录下：

| 头文件 | 说明 |
|--------|------|
| `ql_data_call.h` | 数据拨号 API 主头文件 |
| `ql_net_common.h` | 部分公用结构体定义（如 `ql_net_addr_t`、`ql_net_addr6_t` 等） |

### 4.2 函数概览

共 **33 个 API 函数**，分类如下：

| 函数名 | 功能描述 |
|--------|---------|
| `ql_data_call_init()` | 初始化数据拨号服务 |
| `ql_data_call_create()` | 创建数据拨号实例 |
| `ql_data_call_param_alloc()` | 申请数据拨号配置实例 |
| `ql_data_call_param_init()` | 初始化数据拨号配置实例 |
| `ql_data_call_param_free()` | 释放数据拨号配置实例 |
| `ql_data_call_param_set_apn_id()` | 设置数据拨号配置实例的 APN ID |
| `ql_data_call_param_get_apn_id()` | 获取数据拨号配置实例的 APN ID |
| `ql_data_call_param_set_apn_name()` | 设置数据拨号配置实例的 APN 名称 |
| `ql_data_call_param_get_apn_name()` | 获取数据拨号配置实例的 APN 名称 |
| `ql_data_call_param_set_user_name()` | 设置数据拨号配置实例的用户名 |
| `ql_data_call_param_get_user_name()` | 获取数据拨号配置实例的用户名 |
| `ql_data_call_param_set_user_password()` | 设置数据拨号配置实例的用户密码 |
| `ql_data_call_param_get_user_password()` | 获取数据拨号配置实例的用户密码 |
| `ql_data_call_param_set_auth_pref()` | 设置数据拨号配置实例的认证类型 |
| `ql_data_call_param_get_auth_pref()` | 获取数据拨号配置实例的认证类型 |
| `ql_data_call_param_set_ip_version()` | 设置数据拨号配置实例的 IP 类型 |
| `ql_data_call_param_get_ip_version()` | 获取数据拨号配置实例的 IP 类型 |
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
| `ql_data_call_deinit()` | 去初始化数据拨号服务 |

---

### 4.3 函数详解

---

#### 3.3.1 `ql_data_call_init`

**功能**：初始化数据拨号服务。

> **重要备注**：使用其他数据拨号 API 前，**必须先调用该函数**初始化数据拨号服务。

**函数原型**：
```c
int ql_data_call_init(void)
```

**参数**：无

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_SERVICE_NOT_READY` | 数据拨号服务未准备就绪，建议重试 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 3.3.2 `ql_data_call_create`

**功能**：创建数据拨号实例。

**函数原型**：
```c
int ql_data_call_create(int call_id, const char *call_name, int is_background)
```

**参数**：

| 参数 | 方向 | 类型 | 说明 |
|------|------|------|------|
| `call_id` | [In] | int | 数据拨号实例的唯一标识，范围：1 ~ INT_MAX，需自定义 |
| `call_name` | [In] | const char* | 数据拨号实例名称，需自定义 |
| `is_background` | [In] | int | 设置数据拨号实例是否由后台服务管理 |

`is_background` 取值说明：

| 值 | 说明 |
|----|------|
| `1` | 数据拨号由后台服务管理，即使拨号程序退出，拨号实例仍然存在 |
| `0` | 数据拨号由拨号程序管理，若拨号程序退出，拨号实例也被相应删除 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_SERVICE_NOT_READY` | 数据拨号服务未准备就绪，建议重试 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 3.3.3 `ql_data_call_param_alloc`

**功能**：申请数据拨号配置实例。此配置实例使用完成后，应调用 `ql_data_call_param_free()` 释放配置实例。

**函数原型**：
```c
ql_data_call_param_t *ql_data_call_param_alloc(void)
```

**参数**：无

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `NULL` | 内存不足，函数执行失败 |
| 配置实例的地址 | 函数执行成功 |

##### 3.3.3.1 `ql_data_call_param_t` 类型定义

数据拨号配置实例的类型定义如下（用户无需关注其格式及内容，使用 API 操作即可）：

```c
typedef void ql_data_call_param_t
```

---

#### 3.3.4 `ql_data_call_param_init`

**功能**：初始化数据拨号配置实例（将配置实例恢复到初始状态）。

**函数原型**：
```c
int ql_data_call_param_init(ql_data_call_param_t *param)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 需要初始化的数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.5 `ql_data_call_param_free`

**功能**：释放数据拨号配置实例（释放由 `ql_data_call_param_alloc()` 申请的内存）。

**函数原型**：
```c
int ql_data_call_param_free(ql_data_call_param_t *param)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 需要释放的数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.6 `ql_data_call_param_set_apn_id`

**功能**：设置数据拨号配置实例的 APN ID。

**函数原型**：
```c
int ql_data_call_param_set_apn_id(ql_data_call_param_t *param, int apn_id)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `apn_id` | [In] | 为数据拨号配置实例设置的 APN ID，范围：1~4，**数据拨号建议使用 2~4** |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.7 `ql_data_call_param_get_apn_id`

**功能**：获取数据拨号配置实例的 APN ID。

**函数原型**：
```c
int ql_data_call_param_get_apn_id(ql_data_call_param_t *param, int *apn_id)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `apn_id` | [Out] | 指定数据拨号配置实例的 APN ID（输出） |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.8 `ql_data_call_param_set_apn_name`

**功能**：设置数据拨号配置实例的 APN 名称。

**函数原型**：
```c
int ql_data_call_param_set_apn_name(ql_data_call_param_t *param, const char *apn_name)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `apn_name` | [In] | 为数据拨号配置实例设置的 APN 名称 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.9 `ql_data_call_param_get_apn_name`

**功能**：获取数据拨号配置实例的 APN 名称。

**函数原型**：
```c
int ql_data_call_param_get_apn_name(ql_data_call_param_t *param, char *buf, int buf_len)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `buf` | [Out] | APN 名称缓存（输出） |
| `buf_len` | [In] | APN 名称缓存长度 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.10 `ql_data_call_param_set_user_name`

**功能**：设置数据拨号配置实例的用户名。

**函数原型**：
```c
int ql_data_call_param_set_user_name(ql_data_call_param_t *param, const char *user_name)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `user_name` | [In] | 为数据拨号配置实例设置的用户名 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.11 `ql_data_call_param_get_user_name`

**功能**：获取数据拨号配置实例的用户名。

**函数原型**：
```c
int ql_data_call_param_get_user_name(ql_data_call_param_t *param, char *buf, int buf_len)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `buf` | [Out] | APN 用户名缓存（输出） |
| `buf_len` | [In] | APN 用户名缓存长度 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.12 `ql_data_call_param_set_user_password`

**功能**：设置数据拨号配置实例的用户密码。

**函数原型**：
```c
int ql_data_call_param_set_user_password(ql_data_call_param_t *param, const char *user_password)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `user_password` | [In] | 指定数据拨号配置实例的 APN 用户密码 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.13 `ql_data_call_param_get_user_password`

**功能**：获取数据拨号配置实例的用户密码。

**函数原型**：
```c
int ql_data_call_param_get_user_password(ql_data_call_param_t *param, char *buf, int buf_len)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `buf` | [Out] | APN 用户密码缓存（输出） |
| `buf_len` | [In] | APN 用户密码缓存长度 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.14 `ql_data_call_param_set_auth_pref`

**功能**：设置数据拨号配置实例的认证类型。

**函数原型**：
```c
int ql_data_call_param_set_auth_pref(ql_data_call_param_t *param, int auth_pref)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `auth_pref` | [In] | 为数据拨号配置实例设置的认证类型，枚举类型见 `QL_NET_AUTH_PREF_E`（§3.3.30.2） |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.15 `ql_data_call_param_get_auth_pref`

**功能**：获取数据拨号配置实例的认证类型。

**函数原型**：
```c
int ql_data_call_param_get_auth_pref(ql_data_call_param_t *param, int *p_data)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `p_data` | [Out] | 认证类型（输出），枚举类型见 §3.3.30.2 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.16 `ql_data_call_param_set_ip_version`

**功能**：设置数据拨号配置实例的 IP 类型。

**函数原型**：
```c
int ql_data_call_param_set_ip_version(ql_data_call_param_t *param, int ip_ver)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `ip_ver` | [In] | IP 类型，`QL_NET_IP_VER_E` 枚举类型详见 §3.3.16.1 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

##### 3.3.16.1 `QL_NET_IP_VER_E` — IP 版本枚举

```c
typedef enum {
    QL_NET_IP_VER_MIN  = -0,
    QL_NET_IP_VER_V4   = 0x1,   // IPv4
    QL_NET_IP_VER_V6   = 0x2,   // IPv6
    QL_NET_IP_VER_V4V6 = 0x3,   // IPv4v6（暂不支持）
    QL_NET_IP_VER_MAX
} QL_NET_IP_VER_E
```

| 成员 | 值 | 描述 |
|------|----|------|
| `QL_NET_IP_VER_V4` | 0x1 | IPv4 |
| `QL_NET_IP_VER_V6` | 0x2 | IPv6 |
| `QL_NET_IP_VER_V4V6` | 0x3 | IPv4v6（**暂不支持**） |

---

#### 3.3.17 `ql_data_call_param_get_ip_version`

**功能**：获取数据拨号配置实例的 IP 类型。

**函数原型**：
```c
int ql_data_call_param_get_ip_version(ql_data_call_param_t *param, int *ip_ver)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `ip_ver` | [Out] | 指定数据拨号配置实例的 IP 类型（输出），枚举类型详见 §3.3.16.1 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.18 `ql_data_call_param_set_reconnect_mode`

**功能**：设置自动重连模式，与函数 `ql_data_call_param_set_reconnect_interval()` 配合使用。

**函数原型**：
```c
int ql_data_call_param_set_reconnect_mode(ql_data_call_param_t *param, int mode)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `mode` | [In] | 重连模式，请参考 §2.2，枚举类型详见 §3.3.18.1 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

##### 3.3.18.1 `QL_NET_DATA_CALL_RECONNECT_MODE_E` — 重连模式枚举

```c
typedef enum {
    QL_NET_DATA_CALL_RECONNECT_MODE_MIN    = -1,
    QL_NET_DATA_CALL_RECONNECT_DISABLE     = 0x0,  // 禁用自动重连
    QL_NET_DATA_CALL_RECONNECT_NORMAL      = 0x1,  // 普通定时重连
    QL_NET_DATA_CALL_RECONNECT_MODE_1      = 0x2,  // 时间退避重连
    QL_NET_DATA_CALL_RECONNECT_MODE_2      = 0x3,  // 循环时间退避重连
    QL_NET_DATA_CALL_RECONONECT_MODE_MAX
} QL_NET_DATA_CALL_RECONNECT_MODE_E
```

| 成员 | 描述 |
|------|------|
| `QL_NET_DATA_CALL_RECONNECT_DISABLE` | DISABLE — 不自动重连 |
| `QL_NET_DATA_CALL_RECONNECT_NORMAL` | NORMAL — 按固定时间间隔重连 |
| `QL_NET_DATA_CALL_RECONNECT_MODE_1` | MODE1 — 时间退避重连（到 Tn 后保持） |
| `QL_NET_DATA_CALL_RECONNECT_MODE_2` | MODE2 — 循环时间退避重连 |

---

#### 3.3.19 `ql_data_call_param_get_reconnect_mode`

**功能**：获取为指定数据拨号配置实例设置的重连模式。

**函数原型**：
```c
int ql_data_call_param_get_reconnect_mode(ql_data_call_param_t *param, int *p_mode)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `p_mode` | [Out] | 为指定配置实例设置的重连模式（输出），枚举类型详见 §3.3.18.1 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.20 `ql_data_call_param_set_reconnect_interval`

**功能**：设置自动重连间隔，与函数 `ql_data_call_param_set_reconnect_mode()` 配合使用。

**函数原型**：
```c
int ql_data_call_param_set_reconnect_interval(ql_data_call_param_t *param, int *time_list, int num)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `time_list` | [In] | 时间间隔列表，整型数组，单位：**秒**，最大支持数组长度为 **20** |
| `num` | [In] | `time_list` 的数组长度 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.21 `ql_data_call_param_get_reconnect_interval`

**功能**：获取为指定数据拨号配置实例设置的重连间隔。

**函数原型**：
```c
int ql_data_call_param_get_reconnect_interval(ql_data_call_param_t *param, int *time_list, int *p_num)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |
| `time_list` | [Out] | 时间间隔列表，整型数组，单位：秒，最大支持数组长度为 20 |
| `p_num` | [In/Out] | 整型指针；传入时指针内容为 `time_list` 的数组长度；返回时指针内容为数组中的有效成员个数 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.22 `ql_data_call_config`

**功能**：使用数据拨号配置实例来配置数据拨号实例（将参数配置写入到拨号实例）。

**函数原型**：
```c
int ql_data_call_config(int call_id, ql_data_call_param_t *param)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `call_id` | [In] | 数据拨号实例的唯一标识，范围：1 ~ INT_MAX，需自定义 |
| `param` | [In] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 3.3.23 `ql_data_call_get_config`

**功能**：获取数据拨号实例的配置实例（从拨号实例中读回已保存的配置参数）。

**函数原型**：
```c
int ql_data_call_get_config(int call_id, ql_data_call_param_t *param)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `call_id` | [In] | 数据拨号实例的唯一标识，范围：1 ~ INT_MAX，需自定义 |
| `param` | [Out] | 数据拨号配置实例，由 `ql_data_call_param_alloc()` 申请，配置读出到此参数中 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 3.3.24 `ql_data_call_start`

**功能**：开始数据拨号。

**函数原型**：
```c
int ql_data_call_start(int call_id)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `call_id` | [In] | 数据拨号实例的唯一标识，范围：1 ~ INT_MAX，需自定义 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功（拨号请求已提交，不代表拨号已成功） |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 3.3.25 `ql_data_call_stop`

**功能**：停止数据拨号。

**函数原型**：
```c
int ql_data_call_stop(int call_id)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `call_id` | [In] | 数据拨号实例的唯一标识，范围：1 ~ INT_MAX，需自定义 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 3.3.26 `ql_data_call_delete`

**功能**：删除数据拨号实例。

**函数原型**：
```c
int ql_data_call_delete(int call_id)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `call_id` | [In] | 数据拨号实例的唯一标识，范围：1 ~ INT_MAX，需自定义 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 3.3.27 `ql_data_call_get_list`

**功能**：获取当前存在的数据拨号实例列表。

**函数原型**：
```c
int ql_data_call_get_list(ql_data_call_item_t *list, int *list_len)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `list` | [Out] | 数据拨号实例列表的数组，结构体定义见 §3.3.27.1 |
| `list_len` | [In/Out] | 传入数据拨号实例列表数据长度，返回标识数组中有效的个数 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

##### 3.3.27.1 `ql_data_call_item_t` — 拨号实例信息结构体

当前存在的数据拨号实例信息结构体定义如下：

```c
typedef struct
{
    int  call_id;
    char call_name[QL_NET_MAX_NAME_LEN];
} ql_data_call_item_t
```

| 类型 | 字段 | 说明 |
|------|------|------|
| `int` | `call_id` | 数据拨号的 ID |
| `char` | `call_name` | 数据拨号的名称 |

---

#### 3.3.28 `ql_data_call_get_status`

**功能**：获取数据拨号实例的状态（包含连接状态、IP 地址等详细信息）。

**函数原型**：
```c
int ql_data_call_get_status(int call_id, ql_data_call_status_t *p_sta)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `call_id` | [In] | 数据拨号实例的唯一标识，范围：1 ~ INT_MAX，需自定义 |
| `p_sta` | [Out] | 返回数据拨号实例的状态以及 IP 地址等信息，结构体定义详见 §3.3.28.1 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

##### 3.3.28.1 `ql_data_call_status_t` — 数据拨号状态信息结构体

```c
typedef struct
{
    int                         call_id;
    char                        call_name[QL_NET_MAX_NAME_LEN];
    QL_NET_IP_VER_E             ip_ver;
    QL_NET_DATA_CALL_STATUS_E   call_status;
    char                        device[QL_NET_MAX_NAME_LEN];
    uint8_t                     has_addr;
    ql_net_addr_t               addr;
    uint8_t                     has_addr6;
    ql_net_addr6_t              addr6;
    int                         call_end_reason_type;
    int                         call_end_reason_code;
} ql_data_call_status_t
```

| 类型 | 字段 | 说明 |
|------|------|------|
| `int` | `call_id` | 数据拨号 ID |
| `char` | `call_name` | 数据拨号的名称 |
| `QL_NET_IP_VER_E` | `ip_ver` | IP 类型（详见 §3.3.16.1） |
| `QL_NET_DATA_CALL_STATUS_E` | `call_status` | 数据拨号的状态（详见 §3.3.28.2） |
| `char` | `device` | 数据拨号对应的网卡名 |
| `uint8_t` | `has_addr` | 是否获取到 IPv4 地址（1=有，0=无） |
| `ql_net_addr_t` | `addr` | IPv4 地址信息（详见 §3.3.28.3） |
| `uint8_t` | `has_addr6` | 是否获取到 IPv6 地址（1=有，0=无） |
| `ql_net_addr6_t` | `addr6` | IPv6 地址信息（详见 §3.3.28.4） |
| `int` | `call_end_reason_type` | 上一次数据拨号断开原因类型 |
| `int` | `call_end_reason_code` | 上一次数据拨号断开的原因码 |

##### 3.3.28.2 `QL_NET_DATA_CALL_STATUS_E` — 数据拨号状态枚举

```c
typedef enum {
    QL_NET_DATA_CALL_STATUS_MIN              = -1,
    QL_NET_DATA_CALL_STATUS_NONE             = 0x0,  // 置空状态
    QL_NET_DATA_CALL_STATUS_CREATED          = 0x1,  // 创建状态
    QL_NET_DATA_CALL_STATUS_IDLE             = 0x2,  // 空闲状态
    QL_NET_DATA_CALL_STATUS_CONNECTING       = 0x3,  // 连接中状态
    QL_NET_DATA_CALL_STATUS_PARTIAL_V4_CONNECTED = 0x4,  // IPv4连接成功，IPv6未连接
    QL_NET_DATA_CALL_STATUS_PARTIAL_V6_CONNECTED = 0x5,  // IPv6连接成功，IPv4未连接
    QL_NET_DATA_CALL_STATUS_CONNECTED        = 0x6,  // 连接成功
    QL_NET_DATA_CALL_STATUS_DISCONNECTED     = 0x7,  // 连接断开
    QL_NET_DATA_CALL_STATUS_ERROR            = 0x8,  // 数据拨号失败
    QL_NET_DATA_CALL_STATUS_DELETED          = 0x9,  // 删除状态
    QL_NET_DATA_CALL_STATUS_MAX
} QL_NET_DATA_CALL_STATUS_E
```

| 成员 | 值 | 描述 |
|------|----|------|
| `QL_NET_DATA_CALL_STATUS_NONE` | 0x0 | 置空状态 |
| `QL_NET_DATA_CALL_STATUS_CREATED` | 0x1 | 创建状态 |
| `QL_NET_DATA_CALL_STATUS_IDLE` | 0x2 | 空闲状态 |
| `QL_NET_DATA_CALL_STATUS_CONNECTING` | 0x3 | 连接中状态 |
| `QL_NET_DATA_CALL_STATUS_PARTIAL_V4_CONNECTED` | 0x4 | IPv4 连接成功，IPv6 未连接 |
| `QL_NET_DATA_CALL_STATUS_PARTIAL_V6_CONNECTED` | 0x5 | IPv6 连接成功，IPv4 未连接 |
| `QL_NET_DATA_CALL_STATUS_CONNECTED` | 0x6 | 连接成功（完全连接） |
| `QL_NET_DATA_CALL_STATUS_DISCONNECTED` | 0x7 | 连接断开 |
| `QL_NET_DATA_CALL_STATUS_ERROR` | 0x8 | 数据拨号失败 |
| `QL_NET_DATA_CALL_STATUS_DELETED` | 0x9 | 删除状态 |

##### 3.3.28.3 `ql_net_addr_t` — IPv4 地址信息结构体

IPv4 类型数据拨号的地址信息结构体定义如下：

```c
typedef struct {
    char    addr[QL_NET_MAX_ADDR_LEN];      // IPv4 地址
    char    netmask[QL_NET_MAX_ADDR_LEN];   // 子网掩码
    uint8_t subnet_bits;                     // 子网的网络位的长度
    char    gateway[QL_NET_MAX_ADDR_LEN];   // 网关地址
    char    dnsp[QL_NET_MAX_ADDR_LEN];      // 首选 DNS
    char    dnss[QL_NET_MAX_ADDR_LEN];      // 备选 DNS
} ql_net_addr_t
```

| 类型 | 字段 | 说明 |
|------|------|------|
| `char` | `addr` | IPv4 地址 |
| `char` | `netmask` | 子网掩码 |
| `uint8_t` | `subnet_bits` | 子网的网络位的长度 |
| `char` | `gateway` | 网关地址 |
| `char` | `dnsp` | 首选 DNS |
| `char` | `dnss` | 备选 DNS |

##### 3.3.28.4 `ql_net_addr6_t` — IPv6 地址信息结构体

IPv6 类型数据拨号的地址信息结构体定义如下：

```c
typedef struct {
    char     addr[QL_NET_MAX_ADDR_LEN];     // IPv6 地址
    char     prefix[QL_NET_MAX_ADDR_LEN];   // 子网前缀
    int32_t  prefix_bits;                    // 子网前缀的长度
    char     gateway[QL_NET_MAX_ADDR_LEN];  // 网关地址
    char     dnsp[QL_NET_MAX_ADDR_LEN];     // 首选 DNS
    char     dnss[QL_NET_MAX_ADDR_LEN];     // 备选 DNS
} ql_net_addr6_t
```

| 类型 | 字段 | 说明 |
|------|------|------|
| `char` | `addr` | IPv6 地址 |
| `char` | `prefix` | 子网前缀 |
| `int32_t` | `prefix_bits` | 子网前缀的长度 |
| `char` | `gateway` | 网关地址 |
| `char` | `dnsp` | 首选 DNS |
| `char` | `dnss` | 备选 DNS |

---

#### 3.3.29 `ql_data_call_set_status_ind_cb`

**功能**：设置数据拨号实例状态变化上报的回调函数。当数据拨号的状态发生变化，会调用传入的回调函数。

**函数原型**：
```c
int ql_data_call_set_status_ind_cb(ql_data_call_status_ind_cb_f cb)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `cb` | [In] | 传入的回调函数，数据拨号实例的状态发生变化时会调用该回调函数，回调函数详见 §3.3.29.1 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

> **重要备注**：
> - 回调函数中**不可调用任何数据拨号接口**；
> - 在回调函数中**禁止处理耗时或者阻塞的任务**。

##### 3.3.29.1 `ql_data_call_status_ind_cb_f` — 状态变化回调函数类型

数据拨号实例状态变化的回调函数定义如下：

```c
typedef void (*ql_data_call_status_ind_cb_f)(
    int                       call_id,
    QL_NET_DATA_CALL_STATUS_E pre_call_status,
    ql_data_call_status_t    *p_msg
)
```

| 参数 | 方向 | 说明 |
|------|------|------|
| `call_id` | [In] | 数据拨号 ID |
| `pre_call_status` | [In] | **之前**数据拨号的状态，枚举类型详见 §3.3.28.2 |
| `p_msg` | [In] | 数据拨号的**当前**状态信息（含 IP 地址等） |

**返回值**：无

---

#### 3.3.30 `ql_data_call_set_apn_config`

**功能**：设置 APN 信息。若指定的 APN ID 不存在，则自动创建新 APN，然后再进行 APN 信息配置。

**函数原型**：
```c
int ql_data_call_set_apn_config(int apn_id, ql_data_call_apn_config_t *p_info)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `apn_id` | [In] | APN ID，范围：1~4。APN1 用于建立默认承载，**建议使用 APN2~APN4 进行数据拨号** |
| `p_info` | [In] | APN 配置信息，结构体定义详见 §3.3.30.1 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

##### 3.3.30.1 `ql_data_call_apn_config_t` — APN 配置信息结构体

```c
typedef struct
{
    QL_NET_IP_VER_E   ip_ver;
    QL_NET_AUTH_PREF_E auth_pref;
    char apn_name[QL_NET_MAX_APN_NAME_LEN];
    char username[QL_NET_MAX_APN_USERNAME_LEN];
    char password[QL_NET_MAX_APN_PASSWORD_LEN];
} ql_data_call_apn_config_t
```

| 类型 | 字段 | 说明 |
|------|------|------|
| `QL_NET_IP_VER_E` | `ip_ver` | IP 类型，枚举详见 §3.3.16.1 |
| `QL_NET_AUTH_PREF_E` | `auth_pref` | 认证类型，枚举详见 §3.3.30.2 |
| `char` | `apn_name` | APN 名称 |
| `char` | `username` | 用户名 |
| `char` | `password` | 密码 |

##### 3.3.30.2 `QL_NET_AUTH_PREF_E` — 认证类型枚举

```c
typedef enum {
    QL_NET_AUTH_PREF_MIN                  = -1,
    QL_NET_AUTH_PREF_PAP_CHAP_NOT_ALLOWED = 0,  // 不使用 PAP 和 CHAP 认证
    QL_NET_AUTH_PREF_PAP_ONLY_ALLOWED     = 1,  // 只使用 PAP 认证
    QL_NET_AUTH_PREF_CHAP_ONLY_ALLOWED    = 2,  // 只使用 CHAP 认证
    QL_NET_AUTH_PREF_PAP_CHAP_BOTH_ALLOWED = 3, // 使用 PAP 或 CHAP 认证
    QL_NET_AUTH_PREF_MAX
} QL_NET_AUTH_PREF_E
```

| 成员 | 值 | 描述 |
|------|----|------|
| `QL_NET_AUTH_PREF_PAP_CHAP_NOT_ALLOWED` | 0 | 不使用 PAP 和 CHAP 认证 |
| `QL_NET_AUTH_PREF_PAP_ONLY_ALLOWED` | 1 | 只使用 PAP 认证 |
| `QL_NET_AUTH_PREF_CHAP_ONLY_ALLOWED` | 2 | 只使用 CHAP 认证 |
| `QL_NET_AUTH_PREF_PAP_CHAP_BOTH_ALLOWED` | 3 | 使用 PAP 或 CHAP 认证 |

---

#### 3.3.31 `ql_data_call_get_apn_config`

**功能**：获取 APN 信息。

**函数原型**：
```c
int ql_data_call_get_apn_config(int apn_id, ql_data_call_apn_config_t *p_info)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `apn_id` | [In] | APN ID，范围：1~4。APN1 用于建立默认承载，建议使用 APN2~APN4 进行数据拨号 |
| `p_info` | [Out] | APN 配置信息（输出） |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 非法参数，函数执行失败 |

---

#### 3.3.32 `ql_data_call_set_service_error_cb`

**功能**：设置数据拨号服务程序异常退出上报的回调函数。

**函数原型**：
```c
int ql_data_call_set_service_error_cb(ql_data_call_service_error_cb_f cb)
```

**参数**：

| 参数 | 方向 | 说明 |
|------|------|------|
| `cb` | [In] | 回调函数，如果服务程序发生异常退出，该回调函数会被执行，回调函数详见 §3.3.32.1 |

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

> **重要备注**：
> - 回调函数中**不可调用任何数据拨号接口**；
> - 在回调函数中**禁止处理耗时或者阻塞的任务**。

##### 3.3.32.1 `ql_data_call_service_error_cb_f` — 服务异常回调函数类型

数据拨号服务异常上报的回调函数定义如下：

```c
typedef void (*ql_data_call_service_error_cb_f)(int error)
```

| 参数 | 方向 | 说明 |
|------|------|------|
| `error` | [In] | 错误码，**当前仅返回 `QL_ERR_ABORTED`** |

**返回值**：无

---

#### 3.3.33 `ql_data_call_deinit`

**功能**：去初始化数据拨号服务（与 `ql_data_call_init()` 相对，用于注销数据拨号服务连接）。

**函数原型**：
```c
int ql_data_call_deinit(void)
```

**参数**：无

**返回值**：

| 返回值 | 说明 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

## 5. 完整使用示例

本节介绍如何使用数据拨号接口**同时拨两路数据拨号**（一路公网 + 一路私网）。

调用数据拨号函数需了解如何进行 Linux 系统的网络配置以及 DNS 处理。

**示例前提（SIM 卡与运营商签约的 APN 信息）**：

| APN 类型 | APN 名称 | 用途 |
|----------|---------|------|
| 私网 APN | `apnprivate` | 用于建立默认承载（APN1） |
| 公网 APN | `apnpublic` | 用于公网数据拨号（APN2） |
| 私网 APN | `apnprivate` | 用于私网数据拨号（APN3） |

> 示例代码文件路径：`sample/data_call/sample_multi_data_call.c`

```c
/* ============================================================
 * 数据拨号状态回调处理函数
 * ============================================================ */
data_call_cb()
{
    /* 拨号状态处理 */
}

/* ============================================================
 * 主函数：同时拨公网和私网两路数据拨号
 * ============================================================ */
main()
{
    /* 1. 初始化拨号服务（必须第一步调用） */
    ql_data_call_init;

    /* 2. 设置拨号状态变化上报回调函数 */
    ql_data_call_set_status_ind_cb data_call_cb;

    /* 3. 设置默认承载 APN 的信息（APN1 = 私网 APN） */
    ql_data_call_set_apn_config apn_id=1, apn_name=apnprivate, ip_ver=QL_NET_IP_VER_V4;

    /* 4. 公网拨号使用 APN2，设置 APN2 信息 */
    ql_data_call_set_apn_config apn_id=2, apn_name=apnpublic, ip_ver=ipv4;

    /* 5. 私网拨号使用 APN3，设置 APN3 信息 */
    ql_data_call_set_apn_config apn_id=3, apn_name=apnprivate, ip_ver=ipv4;

    /* ---------- 公网拨号（call_id=1） ---------- */

    /* 6. 创建公网数据拨号实例 */
    ql_data_call_create call_id=1, call_name=public, is_background=0;

    /* 7. 配置公网拨号的配置实例 */
    ql_data_call_param_set_apn_id apn_id=2;         // 使用 APN2（公网）
    ql_data_call_param_set_ip_version ip_ver=ipv4;  // IPv4
    ql_data_call_param_set_reconnect_mode;           // 设置重连模式
    ql_data_call_param_set_reconnect_interval;       // 设置重连间隔

    /* 8. 将配置实例应用到公网拨号实例 */
    ql_data_call_config call_id=1;

    /* 9. 发起公网拨号 */
    ql_data_call_start call_id=1;

    /* ---------- 私网拨号（call_id=2） ---------- */

    /* 10. 创建私网数据拨号实例 */
    ql_data_call_create call_id=2, call_name=private, is_background=0;

    /* 11. 配置私网拨号的配置实例 */
    ql_data_call_param_set_apn_id apn_id=3;         // 使用 APN3（私网）
    ql_data_call_param_set_ip_version ip_ver=ipv4;  // IPv4
    ql_data_call_param_set_reconnect_mode;           // 设置重连模式
    ql_data_call_param_set_reconnect_interval;       // 设置重连间隔

    /* 12. 将配置实例应用到私网拨号实例 */
    ql_data_call_config call_id=2;

    /* 13. 发起私网拨号 */
    ql_data_call_start call_id=2;
}
```

### 典型拨号流程总结（标准步骤）

```
ql_data_call_init()
    ↓
ql_data_call_set_status_ind_cb()  ← 注册状态回调
    ↓
ql_data_call_set_apn_config()     ← 设置 APN1（默认承载）
    ↓
ql_data_call_set_apn_config()     ← 设置 APN2/3（业务拨号）
    ↓
ql_data_call_create()             ← 创建拨号实例
    ↓
ql_data_call_param_alloc()        ← 申请配置实例
    ↓
ql_data_call_param_init()         ← 初始化配置实例
    ↓
ql_data_call_param_set_apn_id()   ← 设置 APN ID
ql_data_call_param_set_ip_version() ← 设置 IP 版本
ql_data_call_param_set_reconnect_mode() ← 设置重连模式
ql_data_call_param_set_reconnect_interval() ← 设置重连间隔
    ↓
ql_data_call_config()             ← 应用配置到拨号实例
    ↓
ql_data_call_param_free()         ← 释放配置实例内存
    ↓
ql_data_call_start()              ← 发起拨号
    ↓
[等待回调 data_call_cb 通知 CONNECTED 状态]
    ↓
ql_data_call_get_status()         ← 获取 IP 地址等信息（可选）
    ↓
... 业务使用中 ...
    ↓
ql_data_call_stop()               ← 停止拨号
    ↓
ql_data_call_delete()             ← 删除拨号实例
    ↓
ql_data_call_deinit()             ← 去初始化服务（可选）
```

---

## 6. 注意事项

原文 §3.5 注意事项（共 3 条，均为强制约束）：

| 编号 | 注意事项 |
|------|---------|
| 1 | **不可**在数据拨号状态回调函数中调用其他 API 接口 |
| 2 | **不可**在数据拨号状态回调函数中处理阻塞事务 |
| 3 | 所有数据拨号函数都**不支持多线程调用** |

> **补充说明**：以上约束同样适用于 `ql_data_call_set_service_error_cb()` 注册的服务异常回调函数（见 §3.3.32 备注）。

---

## 7. 附录：参考文档与术语缩写

### 参考文档

| 序号 | 文档名称 |
|------|---------|
| [1] | Quectel EC200A-CN(TA) QuecOpen 快速开发指导（QuecOpen 平台详细介绍） |
| [2] | Quectel EC200A-CN(TA) QuecOpen 网络注册相关文档（网络注册 API） |
| [3] | EC200A-CN(TA) AT 命令手册（`AT+CGDCONT?` 查看 APN 信息） |

### 术语和缩写

| 术语/缩写 | 全称 | 说明 |
|-----------|------|------|
| APN | Access Point Name | 接入点名称，标识运营商的数据网络 |
| API | Application Programming Interface | 应用程序编程接口 |
| C/S | Client/Server | 客户端/服务端架构 |
| CHAP | Challenge Handshake Authentication Protocol | 握手认证协议 |
| DNS | Domain Name System | 域名系统 |
| IoT | Internet of Things | 物联网 |
| IP | Internet Protocol | 互联网协议 |
| IPv4 | Internet Protocol version 4 | 第四版互联网协议 |
| IPv6 | Internet Protocol version 6 | 第六版互联网协议 |
| ISP | Internet Service Provider | 互联网服务提供商 |
| LTE | Long Term Evolution | 长期演进（4G 标准） |
| PAP | Password Authentication Protocol | 密码认证协议 |
| PLMN | Public Land Mobile Network | 公共陆地移动网络 |
| QMI | Qualcomm MSM Interface | 高通 MSM 接口 |
| SDK | Software Development Kit | 软件开发工具包 |
| SIM | Subscriber Identity Module | 用户身份模块 |
| USIM | Universal Subscriber Identity Module | 通用用户身份模块 |

---

## 附：关键常量与宏说明

以下常量在相关结构体和 API 中频繁出现，来自 `ql_net_common.h`：

| 常量名 | 用途 |
|--------|------|
| `QL_NET_MAX_NAME_LEN` | 数据拨号实例名称 / 网卡名的最大长度 |
| `QL_NET_MAX_ADDR_LEN` | IP 地址字符串的最大长度（含 IPv4/IPv6） |
| `QL_NET_MAX_APN_NAME_LEN` | APN 名称的最大长度 |
| `QL_NET_MAX_APN_USERNAME_LEN` | APN 用户名的最大长度 |
| `QL_NET_MAX_APN_PASSWORD_LEN` | APN 密码的最大长度 |

---

## 附：错误码说明

主要错误码来源于 `ql_type.h`：

| 错误码 | 说明 |
|--------|------|
| `QL_ERR_OK` | 成功（= 0） |
| `QL_ERR_INVALID_ARG` | 非法参数 |
| `QL_ERR_SERVICE_NOT_READY` | 数据拨号服务未就绪，建议重试 |
| `QL_ERR_ABORTED` | 操作被中止（通常为服务端异常退出导致） |
| 其他值 | 详见 `ql_type.h` 中的完整错误码定义 |

拨号错误码（`call_end_reason_code`）定义在：
```
ql-sysroots/usr/include/qmi/wireless_data_service_v01.h
```

---

*文档分析完毕。原文共 40 页，本分析文档涵盖全部章节内容，包含：文档基本信息、引言、数据拨号原理（拨号流程/重连机制/拨号状态/异常处理）、全部 33 个 API 函数详解（函数原型/参数/返回值）、所有数据结构与枚举类型定义、完整使用示例、注意事项及附录。*
