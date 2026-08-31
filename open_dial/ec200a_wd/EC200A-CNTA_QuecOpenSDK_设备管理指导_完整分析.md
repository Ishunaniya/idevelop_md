# EC200A-CN(TA) QuecOpen(SDK) 设备管理指导 完整分析

> **文档版本**：V1.0.1 Preliminary  
> **日期**：2024-03-12  
> **适用模块**：EC200A-CN(TA) LTE Standard 模块系列  
> **文档状态**：临时文件（Preliminary Confidential）  
> **原文页数**：31 页  

---

## 目录

1. [文档概述](#1-文档概述)
2. [法律声明](#2-法律声明)
3. [文档历史](#3-文档历史)
4. [引言](#4-引言)
5. [设备管理飞行模式功能介绍](#5-设备管理飞行模式功能介绍)
6. [设备管理 API](#6-设备管理-api)
   - [6.1 头文件](#61-头文件)
   - [6.2 函数概览](#62-函数概览)
   - [6.3 函数详解](#63-函数详解)
7. [示例代码](#7-示例代码)
8. [附录：参考文档与术语缩写](#8-附录参考文档与术语缩写)
9. [重要注意事项汇总](#9-重要注意事项汇总)

---

## 1. 文档概述

本文档为 **EC200A-CN(TA) QuecOpen(SDK) 设备管理指导**，适用于基于 SDK 构建环境的 QuecOpen® 方案，主要介绍 EC200A-CN(TA) 模块 SDK 中提供的**设备管理 API** 功能和使用方法。

设备管理是**终端管理业务**，用于**模块状态信息的查询及参数设定**。

| 属性 | 内容 |
|------|------|
| 适用平台 | QuecOpen®（基于 Linux 的嵌入式开发平台） |
| 模块系列 | LTE Standard 模块（EC200A-CN(TA)） |
| 文档版本 | V1.0.1 |
| 文档状态 | Preliminary（临时文件） |
| 发布日期 | 2024-03-12 |

---

## 2. 法律声明

### 2.1 前言

移远通信提供该文档内容以支持客户的产品设计。客户须按照文档中提供的规范、参数来设计产品。同时，移远通信提供的参考设计仅作为示例。移远通信可在未事先通知的情况下，自行决定随时增加、修改或重述本文档。

**联系方式：**
- 公司：上海移远通信技术股份有限公司
- 地址：上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼，邮编：200233
- 电话：+86 21 5108 6236
- 邮箱：info@quectel.com
- 技术支持邮箱：support@quectel.com

### 2.2 使用和披露限制

- **许可协议**：除非移远通信特别授权，否则所提供硬件、材料和文档的接收方须对接收的内容保密，不得将其用于本项目的实施与开展以外的任何其他目的。
- **版权声明**：版权所有 © 上海移远通信技术股份有限公司 2024，保留一切权利。*Copyright © Quectel Wireless Solutions Co., Ltd. 2024.*
- **商标**：本文档中的任何内容均不授予在广告、宣传或其他方面使用移远通信或第三方的任何商标、商号及名称，或其缩略语，或其仿冒品的权利。
- **第三方权利**：本文档可能涉及一个或多个属于第三方的硬软件和文档（"第三方材料"）。对此类第三方材料的使用应受本文档的所有限制和义务约束。

### 2.3 隐私声明

为实现移远通信产品功能，特定设备数据将会上传至移远通信或第三方服务器（包括运营商、芯片供应商或您指定的服务器）。移远通信严格遵守相关法律法规，仅为实现产品功能之目的或在适用法律允许的情况下保留、使用、披露或以其他方式处理相关数据。

### 2.4 免责声明

1. 移远通信不承担任何因未能遵守有关操作或设计规范而造成损害的责任。
2. 移远通信不承担因本文档中的任何因不准确、遗漏、或使用本文档中的信息而产生的任何责任。
3. 移远通信尽力确保开发中功能的完整性、准确性、及时性，但不排除上述功能错误或遗漏的可能。除非另有协议规定，否则移远通信对开发中功能的使用不做任何明示或法定的保证。
4. 移远通信对第三方网站及第三方资源的信息、内容、广告、商业报价、产品、服务和材料的可访问性、安全性、准确性、可用性、合法性和完整性不承担任何法律责任。

---

## 3. 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更描述 |
|------|------|------|----------|
| — | 2022-04-15 | Robert ZHOU | 文档创建 |
| 1.0.0 | 2023-03-10 | Sunshine HUANG | 临时版本 |
| 1.0.1 | 2024-03-12 | Jensen ZHANG | 临时版本：<br>1. 基于 QuecOpen 方案统一命名，更新文档名称。<br>2. 新增如下函数（第 3.3 章）：`ql_dm_set_radio_on()`、`ql_dm_set_radio_off()`、`ql_dm_set_service_error_cb()`、`ql_dm_set_qoos_config()`、`ql_dm_get_qoos_config()`、`ql_dm_set_qoos_enable()`、`ql_dm_get_qoos_enable()`<br>3. 新增部分示例（第 4.12 章～第 4.18 章）。 |

---

## 4. 引言

移远通信 EC200A-CN(TA) 模块支持 QuecOpen® 方案。**QuecOpen® 是基于 Linux 的嵌入式开发平台**，可简化 IoV 应用的软件设计和开发过程。有关 QuecOpen® 的详细信息，请参考文档 [1]（《EC200A-CN(TA) QuecOpen 快速开发指导》）。

本文档适用于 **SDK 构建环境的 QuecOpen® 方案**，主要介绍 EC200A-CN(TA) 模块 SDK 中提供的设备管理 API 功能和使用方法。

- **设备管理**是终端管理业务，用于模块状态信息的查询及参数设定。

---

## 5. 设备管理飞行模式功能介绍

### 5.1 飞行模式行为说明

模块进入飞行模式时，会将**射频电路断电**，此时：
- 模块**不会进行任何无线信号的收发**
- **甚至无法拨打紧急电话**

模块退出飞行模式后，会恢复对射频电路的供电，模块可进行注网、接打电话、收发短信、拨号等业务。

> **重要说明**：无论飞行模式启用或禁用，模块在**下一次断电重启后均会退出飞行模式并恢复默认配置**，即 `QL_DM_AIR_PLANE_MODE_OFF`（AT+CFUN 的值为 1，模块为全功能模式），同时打开射频功能。

### 5.2 飞行模式枚举类型：`QL_DM_AIR_PLANE_MODE_TYPE_E`

```c
typedef enum QL_DW_AIR_PLANE_MODE_TYPE_ENUM
{
    QL_DM_AIR_PLANE_MODE_UNKNOWN = 0,   /* 飞行模式状态未知 */
    QL_DM_AIR_PLANE_MODE_ON     = 1,    /* 飞行模式开启     */
    QL_DM_AIR_PLANE_MODE_OFF    = 2,    /* 飞行模式关闭     */
    QL_DM_AIR_PLANE_MODE_NA     = 3     /* 飞行模式不可用   */
} QL_DM_AIR_PLANE_MODE_TYPE_E;
```

| 成员 | 值 | 描述 |
|------|----|------|
| `QL_DM_AIR_PLANE_MODE_UNKNOWN` | 0 | 飞行模式状态未知 |
| `QL_DM_AIR_PLANE_MODE_ON` | 1 | 飞行模式开启 |
| `QL_DM_AIR_PLANE_MODE_OFF` | 2 | 飞行模式关闭（默认值） |
| `QL_DM_AIR_PLANE_MODE_NA` | 3 | 飞行模式不可用 |

---

## 6. 设备管理 API

### 6.1 头文件

- **头文件**：`ql_dm.h`
- **路径**：SDK 的 `ql-sysroots/usr/include/ql-sdk/` 目录下

> **备注**：设备管理 API 用于管理和控制模块设备，**某些 API 会改变模块的行为**，故用户需谨慎调用相关 API。

---

### 6.2 函数概览

> **重要备注**：若无特别说明，本文档所述函数均**不支持并发调用**，并且**不能在相关回调函数中调用以上函数**。

| 函数 | 说明 |
|------|------|
| `ql_dm_init()` | 初始化设备管理服务 |
| `ql_dm_deinit()` | 去初始化设备管理服务 |
| `ql_dm_get_software_version()` | 获取 AP 侧软件版本号 |
| `ql_dm_get_device_firmware_rev_id()` | 获取 modem 侧固件版本号 |
| `ql_dm_get_modem_state()` | 获取 modem 状态 |
| `ql_dm_get_temperature()` | 获取模块晶振温度 |
| `ql_dm_get_device_serial_numbers()` | 获取模块序列号（IMEI/MEID） |
| `ql_dm_get_air_plane_mode()` | 获取模块飞行模式 |
| `ql_dm_set_air_plane_mode()` | 设置模块飞行模式 |
| `ql_dm_set_air_plane_mode_ind_cb()` | 设置飞行模式事件回调函数 |
| `ql_dm_get_cpu_occupancy()` | 获取模块 AP 侧 CPU 占有率 |
| `ql_dm_get_mem_usage()` | 获取模块 AP 侧内存使用率 |
| `ql_ms_dm_set_modem_state_change_ind_cb()` | 设置 modem 状态改变回调函数 |
| `ql_dm_set_radio_on()` | 开启射频（等效 AT+CFUN=1） |
| `ql_dm_set_radio_off()` | 关闭射频（等效 AT+CFUN=0） |
| `ql_dm_set_service_error_cb()` | 设置服务异常事件回调函数 |
| `ql_dm_get_qoos_enable()` | 获取 QooS 的使能状态（兼容接口） |
| `ql_dm_set_qoos_enable()` | 设置 QooS 的使能状态（兼容接口） |
| `ql_dm_set_qoos_config()` | 设置 QooS 的配置参数（搜网间隔） |
| `ql_dm_get_qoos_config()` | 获取 QooS 的配置参数（搜网间隔） |

---

### 6.3 函数详解

#### 6.3.1 `ql_dm_init`

**功能**：初始化设备管理服务。

**函数原型**：
```c
int ql_dm_init(void)
```

**参数**：无

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

> **备注**：程序启动后，**必须调用** `ql_dm_init()` 初始化设备管理服务，才能使用其他需要初始化的 API。

---

#### 6.3.2 `ql_dm_deinit`

**功能**：去初始化设备管理服务。

**函数原型**：
```c
int ql_dm_deinit(void)
```

**参数**：无

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 6.3.3 `ql_dm_get_software_version`

**功能**：获取 AP 侧软件版本号。该函数**不要求设备管理服务必须初始化**。

**函数原型**：
```c
int ql_dm_get_software_version(char *soft_ver, int soft_ver_len)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `soft_ver` | [Out] | `char *` | AP 侧软件版本号字符串（输出缓冲区） |
| `soft_ver_len` | [In] | `int` | `soft_ver` 缓冲区的长度 |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 6.3.4 `ql_dm_get_device_firmware_rev_id`

**功能**：获取 modem 侧固件版本号。该函数在**设备管理服务初始化成功后方可使用**。

**函数原型**：
```c
int ql_dm_get_device_firmware_rev_id(char *firmware_rev_id, int firmware_rev_id_len)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `firmware_rev_id` | [Out] | `char *` | modem 侧固件版本号字符串（输出缓冲区） |
| `firmware_rev_id_len` | [In] | `int` | `firmware_rev_id` 缓冲区的长度 |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 6.3.5 `ql_dm_get_modem_state`

**功能**：获取 modem 状态。该函数**不要求设备管理服务必须初始化**。

**函数原型**：
```c
int ql_dm_get_modem_state(QL_DM_MODEM_STATE_TYPE_E *modem_state)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `modem_state` | [Out] | `QL_DM_MODEM_STATE_TYPE_E *` | modem 状态，详见 6.3.5.1 |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type` |

##### 6.3.5.1 `QL_DM_MODEM_STATE_TYPE_E`

modem 状态枚举定义如下：

```c
typedef enum QL_DM_MODEM_STATE_TYPE_ENUM
{
    QL_DM_MODEM_STATE_UNKNOWN = 2,   /* modem 状态未知 */
    QL_DM_MODEM_STATE_ONLINE  = 1,   /* modem 在线     */
    QL_DM_MODEM_STATE_OFFLINE = 0    /* modem 掉线     */
} QL_DM_MODEM_STATE_TYPE_E;
```

| 成员 | 值 | 描述 |
|------|----|------|
| `QL_DM_MODEM_STATE_UNKNOWN` | 2 | modem 状态未知 |
| `QL_DM_MODEM_STATE_ONLINE` | 1 | modem 在线 |
| `QL_DM_MODEM_STATE_OFFLINE` | 0 | modem 掉线 |

---

#### 6.3.6 `ql_dm_get_temperature`

**功能**：获取模块晶振温度。该函数**不要求设备管理服务必须初始化**。

> **说明**：模块中还有其他温度值，如芯片温度、功率放大器温度等。用户可直接访问 `/sys/class/thermal/thermal_zoneX/type` 文件查看温度类型，相同目录下的 `temp` 文件为各温度类型对应的温度值。

**函数原型**：
```c
int ql_dm_get_temperature(float *temperature)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `temperature` | [Out] | `float *` | 模块晶振温度（原始值，需除以 1000.0 得到摄氏度） |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type` |

---

#### 6.3.7 `ql_dm_get_device_serial_numbers`

**功能**：获取模块的序列号（IMEI / MEID）。该函数在**设备管理服务初始化成功后方可使用**。

**函数原型**：
```c
int ql_dm_get_device_serial_numbers(ql_dm_device_serial_numbers_info_t *p_info)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `p_info` | [Out] | `ql_dm_device_serial_numbers_info_t *` | 模块序列号信息，详见 6.3.7.1 |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type` |

##### 6.3.7.1 `ql_dm_device_serial_numbers_info_t`

模块序列号信息结构体定义如下（文档定义）：

```c
typedef struct
{
    uint8_t imei_valid;
    char    imei[QL_DM_IMEI_MAX_LEN + 1];
    uint8_t meid_valid;
    char    meid[QL_DM_MEID_MAX_LEN + 1];
} ql_dm_device_serial_numbers_info_t;
```

> **注意**：从第 4.5 节示例代码中可以看到，实际结构体还包含 `imei2_valid` 和 `imei2` 字段，用于支持**双 SIM 卡 / 双 IMEI** 场景。

| 类型 | 字段 | 描述 |
|------|------|------|
| `uint8_t` | `imei_valid` | IMEI 是否有效：`0` = 无效，非 `0` = 有效 |
| `char[]` | `imei` | 设备 IMEI 号，长度为 `QL_DM_IMEI_MAX_LEN + 1`，适用于 GSM/WCDMA 网络制式 |
| `uint8_t` | `imei2_valid` | IMEI2 是否有效（双卡场景）：`0` = 无效，非 `0` = 有效 |
| `char[]` | `imei2` | 设备 IMEI2 号（双卡场景） |
| `uint8_t` | `meid_valid` | MEID 是否有效：`0` = 无效，非 `0` = 有效 |
| `char[]` | `meid` | 设备 MEID 号，长度为 `QL_DM_MEID_MAX_LEN + 1`，适用于 CDMA 网络制式 |

---

#### 6.3.8 `ql_dm_get_air_plane_mode`

**功能**：获取模块飞行模式。该函数在**设备管理服务初始化成功后方可使用**。

**函数原型**：
```c
int ql_dm_get_air_plane_mode(QL_DM_AIR_PLANE_MODE_TYPE_E *p_info)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `p_info` | [Out] | `QL_DM_AIR_PLANE_MODE_TYPE_E *` | 飞行模式信息，详见第 5.2 节 |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type` |

---

#### 6.3.9 `ql_dm_set_air_plane_mode`

**功能**：设置模块飞行模式。飞行模式**默认为禁用状态**，**配置断电后不保存**。该函数在**设备管理服务初始化成功后方可使用**。

**函数原型**：
```c
int ql_dm_set_air_plane_mode(QL_DM_AIR_PLANE_MODE_TYPE_E air_plane_mode)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `air_plane_mode` | [In] | `QL_DM_AIR_PLANE_MODE_TYPE_E` | 要设置的飞行模式，定义见第 5.2 节 |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type` |

---

#### 6.3.10 `ql_dm_set_air_plane_mode_ind_cb`

**功能**：设置飞行模式事件回调函数。该函数所做设置在**程序退出后失效**，如有需要，请重新调用该函数进行设置。设置成功后，若飞行模式状态发生变化，则会调用飞行模式事件回调函数。该函数在**设备管理服务初始化成功后方可使用**。

**函数原型**：
```c
int ql_dm_set_air_plane_mode_ind_cb(ql_dm_air_plane_mode_ind_cb cb_func)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `cb_func` | [In] | `ql_dm_air_plane_mode_ind_cb` | 飞行模式事件回调函数；传入 `NULL` 可注销回调，详见 6.3.10.1 |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type` |

##### 6.3.10.1 `ql_dm_air_plane_mode_ind_cb` 回调类型

该回调函数处理飞行模式状态变化事件。

**函数原型**：
```c
typedef void (*ql_dm_air_plane_mode_ind_cb)(QL_DM_AIR_PLANE_MODE_TYPE_E air_plane_mode)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `air_plane_mode` | [In] | `QL_DM_AIR_PLANE_MODE_TYPE_E` | 当前模块飞行模式，详见第 5.2 节 |

**返回值**：无

---

#### 6.3.11 `ql_dm_get_cpu_occupancy`

**功能**：获取 AP 侧 CPU 占用率。该函数**不要求设备管理服务必须初始化**。

**函数原型**：
```c
int ql_dm_get_cpu_occupancy(float *cpu_occupancy)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `cpu_occupancy` | [Out] | `float *` | 当前 AP 侧 CPU 占有率（百分比，例：45.23 表示 45.23%） |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type` |

---

#### 6.3.12 `ql_dm_get_mem_usage`

**功能**：获取 AP 侧内存使用率。该函数**不要求设备管理服务必须初始化**。

**函数原型**：
```c
int ql_dm_get_mem_usage(float *mem_use)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `mem_use` | [Out] | `float *` | 当前 AP 侧内存使用率（百分比） |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type` |

---

#### 6.3.13 `ql_ms_dm_set_modem_state_change_ind_cb`

**功能**：设置 modem 状态改变回调函数。**设置断电不保存**。设置成功后，若 modem 状态改变，则会调用该函数设置的回调函数。该函数在**设备管理服务初始化成功后方可使用**。

> **注意**：函数名前缀为 `ql_ms_dm_`（而非 `ql_dm_`），与其他函数命名稍有差异。

**函数原型**：
```c
int ql_ms_dm_set_modem_state_change_ind_cb(ql_dm_modem_state_ind_cb cb_func)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `cb_func` | [In] | `ql_dm_modem_state_ind_cb` | modem 状态改变回调函数；传入 `NULL` 可注销，详见 6.3.13.1 |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type` |

##### 6.3.13.1 `ql_dm_modem_state_ind_cb` 回调类型

该回调函数在 modem 状态发生变化时被调用，传入最新的 modem 状态。

**函数原型**：
```c
typedef void (*ql_dm_modem_state_ind_cb)(int modem_state)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `modem_state` | [In] | `int` | modem 状态值，参考 `QL_DM_MODEM_STATE_TYPE_E`（见 6.3.5.1） |

**返回值**：无

---

#### 6.3.14 `ql_dm_set_radio_on`

**功能**：开启射频。其作用与 **`AT+CFUN=1`** 相同（全功能模式）。该函数在**设备管理服务初始化成功后方可使用**。

**函数原型**：
```c
int ql_dm_set_radio_on(void)
```

**参数**：无

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 6.3.15 `ql_dm_set_radio_off`

**功能**：关闭射频。其作用与 **`AT+CFUN=0`** 相同（最小功能模式，射频关闭）。该函数在**设备管理服务初始化成功后方可使用**。

**函数原型**：
```c
int ql_dm_set_radio_off(void)
```

**参数**：无

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 6.3.16 `ql_dm_set_service_error_cb`

**功能**：设置服务异常事件回调函数。该函数在**设备管理服务初始化成功后方可使用**。

**函数原型**：
```c
int ql_dm_set_service_error_cb(ql_dm_service_error_cb_f cb)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `cb` | [In] | `ql_dm_service_error_cb_f` | 服务异常事件回调函数，详见 6.3.16.1 |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

##### 6.3.16.1 `ql_dm_service_error_cb_f` 回调类型

该回调函数在设备管理服务发生异常时被调用。

**函数原型**：
```c
typedef void (*ql_dm_service_error_cb_f)(int error)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `error` | [In] | `int` | 异常错误码，已知取值：`-1099` = Service Abort（服务中止） |

**返回值**：无

---

#### 6.3.17 `ql_dm_get_qoos_enable`

**功能**：获取 QooS 使能状态。

**函数原型**：
```c
int ql_dm_get_qoos_enable(char *enable);
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `enable` | [Out] | `char *` | QooS 使能状态（输出） |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

> **重要备注**：模块中 QooS 机制为**开启状态，不可设置**。该函数为**兼容 QooS 机制而实现，无实际功能**。

---

#### 6.3.18 `ql_dm_set_qoos_enable`

**功能**：设置 QooS 使能状态。

**函数原型**：
```c
int ql_dm_set_qoos_enable(char enable);
```

**参数**：

| 参数 | 方向 | 类型 | 描述 |
|------|------|------|------|
| `enable` | [In] | `char` | QooS 使能状态（输入） |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

> **重要备注**：模块中 QooS 机制为**开启状态，不可设置**。该函数为**兼容 QooS 机制而实现，无实际功能**。

---

#### 6.3.19 `ql_dm_set_qoos_config`

**功能**：设置 QooS 配置参数（三轮搜网间隔时间）。该函数在**设备管理服务初始化成功后方可使用**。

**函数原型**：
```c
int ql_dm_set_qoos_config(int p1, int p2, int p3)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 | 范围 | 单位 |
|------|------|------|------|------|------|
| `p1` | [In] | `int` | 第一轮搜网间隔时间 | 1～225 | 秒 |
| `p2` | [In] | `int` | 第二轮搜网间隔时间 | 1～225 | 秒 |
| `p3` | [In] | `int` | 第三轮搜网间隔时间 | 1～225 | 秒 |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 6.3.20 `ql_dm_get_qoos_config`

**功能**：获取 QooS 配置参数（三轮搜网间隔时间）。该函数在**设备管理服务初始化成功后方可使用**。

**函数原型**：
```c
int ql_dm_get_qoos_config(int *p1, int *p2, int *p3)
```

**参数**：

| 参数 | 方向 | 类型 | 描述 | 范围 | 单位 |
|------|------|------|------|------|------|
| `p1` | [Out] | `int *` | 第一轮搜网间隔时间 | 1～225 | 秒 |
| `p2` | [Out] | `int *` | 第二轮搜网间隔时间 | 1～225 | 秒 |
| `p3` | [Out] | `int *` | 第三轮搜网间隔时间 | 1～225 | 秒 |

**返回值**：

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

## 7. 示例代码

> **说明**：完整示例代码路径为 `/ql-sdk/sample/test_sdk_api/m_dm.c`。本章为部分示例。**程序启动后必须先调用 `ql_dm_init()` 初始化设备管理服务。**

### 7.1 获取 AP 侧软件版本号

```c
void item_ql_dm_get_software_version(void)
{
    int ret;
    char soft_ver[128] = {0};

    ret = ql_dm_get_software_version(soft_ver, sizeof(soft_ver));

    printf("ql_dm_get_software_version ret = %d, software version is %s\n", ret, soft_ver);
}
```

### 7.2 获取 modem 侧固件版本号

```c
void item_ql_dm_get_device_firmware_rev_id(void)
{
    int ret;
    char firmware_rev_id[QL_DM_FIRMWARE_REV_MAX_LEN + 1] = {0};

    ret = ql_dm_get_device_firmware_rev_id(firmware_rev_id, sizeof(firmware_rev_id));
    printf("ql_dm_get_device_firmware_rev_id ret = %d, device revision id is %s\n",
           ret, firmware_rev_id);
}
```

### 7.3 获取 modem 状态

```c
void item_ql_dm_get_modem_state(void)
{
    int ret;
    QL_DM_MODEM_STATE_TYPE_E modem_state = QL_DM_MODEM_STATE_UNKNOWN;

    ret = ql_dm_get_modem_state(&modem_state);
    if (QL_DM_MODEM_STATE_ONLINE == modem_state)
    {
        printf("ql_dm_get_modem_state ret = %d, modem state is ONLINE\n", ret);
    }
    else if (QL_DM_MODEM_STATE_OFFLINE == modem_state)
    {
        printf("ql_dm_get_modem_state ret = %d, modem state is OFFLINE\n", ret);
    }
    else
    {
        printf("ql_dm_get_modem_state ret = %d, modem state UNKNOWN\n", ret);
    }
}
```

### 7.4 获取模块晶振温度

```c
void item_ql_dm_get_temperature(void)
{
    int ret;
    float temperature = 0.0;

    ret = ql_dm_get_temperature(&temperature);

    /* 原始值除以 1000.0 得到实际摄氏度 */
    printf("ql_dm_get_temperature ret = %d, temperature=%.2f\n", ret, temperature/1000.0);
}
```

### 7.5 获取模块序列号（IMEI / IMEI2 / MEID）

```c
void item_ql_dm_get_device_serial_numbers(void)
{
    int ret;
    ql_dm_device_serial_numbers_info_t t_info;
    memset(&t_info, 0, sizeof(ql_dm_device_serial_numbers_info_t));

    ret = ql_dm_get_device_serial_numbers(&t_info);
    printf("ql_dm_get_device_serial_number ret = %d", ret);
    if (t_info.imei_valid)
    {
        printf(", imei is %s", t_info.imei);
    }
    if (t_info.imei2_valid)
    {
        printf(", imei2 is %s", t_info.imei2);
    }
    if (t_info.meid_valid)
    {
        printf(", meid is %s ", t_info.meid);
    }
    printf("\n");
}
```

### 7.6 获取模块飞行模式

```c
void item_ql_dm_get_air_plane_mode(void)
{
    int ret;
    char mode_info[16] = {0};
    QL_DM_AIR_PLANE_MODE_TYPE_E air_plane_mode;

    ret = ql_dm_get_air_plane_mode(&air_plane_mode);

    printf("ql_dm_get_air_plane_mode ret = %d, ", ret);
    if (internal_dm_get_air_plane_mode(air_plane_mode, mode_info, sizeof(mode_info)) == 0)
    {
        printf("unrecognized air plane mode:%d\n", air_plane_mode);
    }
    else
    {
        printf("current air plane mode is %s\n", mode_info);
    }
}
```

### 7.7 设置模块飞行模式

```c
void item_ql_dm_set_air_plane_mode(void)
{
    int ret;
    int mode;
    QL_DM_AIR_PLANE_MODE_TYPE_E air_plane_mode;

    printf("please input air plane mode(1: ON, 2: OFF): ");
    ret = t_get_int(&mode);
    if (ret != 0)
    {
        printf("Invalid input\n");
        return;
    }
    air_plane_mode = mode;
    if (air_plane_mode != QL_DM_AIR_PLANE_MODE_ON &&
        air_plane_mode != QL_DM_AIR_PLANE_MODE_OFF)
    {
        printf("please input 1 or 2\n");
        return;
    }

    ret = ql_dm_set_air_plane_mode(air_plane_mode);
    printf("ql_dm_set_air_plane_mode ret = %d\n", ret);
}
```

### 7.8 设置飞行模式事件回调函数

```c
void item_ql_dm_set_air_plane_mode_ind_cb(void)
{
    int ret = 0;
    int reg_flag = 0;

    printf("please input air plane mode reg option: (0: unreg, other: reg): ");
    ret = t_get_int(&reg_flag);
    if (ret != 0)
    {
        printf("Invalid input\n");
        return;
    }

    if (reg_flag)
    {
        /* 注册回调 */
        ret = ql_dm_set_air_plane_mode_ind_cb(dm_air_plane_mode_event_ind_cb);
    }
    else
    {
        /* 注销回调（传 NULL） */
        ret = ql_dm_set_air_plane_mode_ind_cb(NULL);
    }
    printf("ql_dm_set_air_plane_mode_ind_cb ret = %d\n", ret);
}
```

### 7.9 获取模块 AP 侧 CPU 占用率

```c
void item_ql_dm_get_cpu_occupancy(void)
{
    int ret;
    float cpu_use = 0.0;

    ret = ql_dm_get_cpu_occupancy(&cpu_use);

    printf("ql_dm_get_cpu_occupancy ret = %d, cpu_occupancy = %.2f%%\n", ret, cpu_use);
}
```

### 7.10 获取模块 AP 侧内存使用率

```c
void item_ql_dm_get_mem_usage(void)
{
    int ret;
    float mem_use = 0.0;

    ret = ql_dm_get_mem_usage(&mem_use);

    printf("ql_dm_get_mem_usage ret = %d, mem_usage = %.2f%%\n", ret, mem_use);
}
```

### 7.11 设置 modem 状态改变回调函数

```c
void item_ql_dm_set_modem_state_change_ind_cb(void)
{
    int ret = 0;
    int reg_flag = 0;

    printf("please input modem state reg option: (0: unreg, other: reg): ");
    ret = t_get_int(&reg_flag);
    if (ret != 0)
    {
        printf("Invalid input\n");
        return;
    }

    if (reg_flag)
    {
        /* 注册回调 */
        ret = ql_dm_set_modem_state_change_ind_cb(dm_modem_state_change_ind_cb);
    }
    else
    {
        /* 注销回调（传 NULL） */
        ret = ql_dm_set_modem_state_change_ind_cb(NULL);
    }
    printf("ql_dm_set_modem_state_change_ind_cb ret = %d\n", ret);
}
```

### 7.12 开启射频

```c
void item_ql_dm_set_radio_on(void)
{
    int ret;

    ret = ql_dm_set_radio_on();
    printf("ql_dm_set_radio_on ret = %d\n", ret);
}
```

### 7.13 关闭射频

```c
void item_ql_dm_set_radio_off(void)
{
    int ret;

    ret = ql_dm_set_radio_off();
    printf("ql_dm_set_radio_off ret = %d\n", ret);
}
```

### 7.14 设置服务异常事件回调函数

```c
void item_ql_dm_set_service_error_cb(void)
{
    int ret = 0;

    printf("Start to item_ql_dm_set_service_error_cb : ");
    ret = ql_dm_set_service_error_cb(dm_service_error_cb);
    if (ret != QL_ERR_OK)
    {
        printf("failed, ret=%d\n", ret);
    }
    else
    {
        printf("successful\n");
    }
}
```

### 7.15 获取 QooS 使能状态

```c
void item_ql_dm_get_qoos_enable(void)
{
    int ret = 0;
    int enable = 0;

    ret = ql_dm_get_qoos_enable(&enable);
    if (ret != 0)
    {
        printf("error occur\n");
        return;
    }
    printf("enable is %d\n", enable);
    if (enable == 1)
    {
        printf("qoos customized is enable\n");
    }
    printf("ql_dm_set_qoos_config ret = %d\n", ret);
}
```

### 7.16 设置 QooS 使能状态

```c
void item_ql_dm_set_qoos_enable(void)
{
    int ret;
    int enable;

    printf("input 0/1\n");
    t_get_int(&enable);
    ret = ql_dm_set_qoos_enable(enable);
    if (ret != 0)
    {
        printf("error occur\n");
        return;
    }
    printf("ql_dm_set_qoos_config ret = %d\n", ret);
}
```

### 7.17 设置 QooS 配置参数

```c
void item_ql_dm_set_qoos_config(void)
{
    int ret;
    int phase1, phase2, phase3;

    printf("please input qoos customized param(3) three time");
    t_get_int(&phase1);
    t_get_int(&phase2);
    t_get_int(&phase3);
    ret = ql_dm_set_qoos_config(phase1, phase2, phase3);
    printf("ql_dm_set_qoos_config ret = %d\n", ret);
}
```

### 7.18 获取 QooS 配置参数

```c
void item_ql_dm_get_qoos_config(void)
{
    int ret;
    int phase1, phase2, phase3;

    ret = ql_dm_get_qoos_config(&phase1, &phase2, &phase3);
    printf("ql_dm_get_qoos_config ret = %d\n", ret);
    printf("qoos param: %d, %d, %d\n", phase1, phase2, phase3);
}
```

---

## 8. 附录：参考文档与术语缩写

### 8.1 参考文档（表 2）

| 序号 | 文档名称 |
|------|----------|
| [1] | EC200A-CN(TA) QuecOpen(SDK) 快速开发指导 |

### 8.2 术语缩写（表 3）

| 缩写 | 英文全称 | 中文说明 |
|------|----------|----------|
| AP | Application Processor | 应用处理器（运行 Linux 用户态程序的侧） |
| CP / Modem | Communication Processor | 通信处理器（射频基带处理器） |
| IMEI | International Mobile Equipment Identity | 国际移动设备识别码，适用于 GSM/WCDMA |
| MEID | Mobile Equipment Identifier | 移动设备标识符，适用于 CDMA 网络 |
| QooS | Quality of Service (Quectel) | 移远通信 QoS 机制，控制搜网间隔策略 |
| SDK | Software Development Kit | 软件开发工具包 |
| IoV | Internet of Vehicles | 车联网 |
| GSM | Global System for Mobile Communications | 全球移动通信系统（2G） |
| WCDMA | Wideband Code Division Multiple Access | 宽带码分多址（3G） |
| CDMA | Code Division Multiple Access | 码分多址 |
| LTE | Long Term Evolution | 长期演进（4G） |
| AT | Attention | AT 指令前缀 |
| API | Application Programming Interface | 应用程序编程接口 |
| DM | Device Management | 设备管理 |

---

## 9. 重要注意事项汇总

### 9.1 初始化要求一览

| 函数 | 需要先调用 `ql_dm_init()` |
|------|--------------------------|
| `ql_dm_init()` | — （本身就是初始化函数） |
| `ql_dm_deinit()` | 是 |
| `ql_dm_get_software_version()` | **否**（可直接调用） |
| `ql_dm_get_device_firmware_rev_id()` | **是** |
| `ql_dm_get_modem_state()` | **否** |
| `ql_dm_get_temperature()` | **否** |
| `ql_dm_get_device_serial_numbers()` | **是** |
| `ql_dm_get_air_plane_mode()` | **是** |
| `ql_dm_set_air_plane_mode()` | **是** |
| `ql_dm_set_air_plane_mode_ind_cb()` | **是** |
| `ql_dm_get_cpu_occupancy()` | **否** |
| `ql_dm_get_mem_usage()` | **否** |
| `ql_ms_dm_set_modem_state_change_ind_cb()` | **是** |
| `ql_dm_set_radio_on()` | **是** |
| `ql_dm_set_radio_off()` | **是** |
| `ql_dm_set_service_error_cb()` | **是** |
| `ql_dm_get_qoos_enable()` | 未明确要求 |
| `ql_dm_set_qoos_enable()` | 未明确要求 |
| `ql_dm_set_qoos_config()` | **是** |
| `ql_dm_get_qoos_config()` | **是** |

### 9.2 配置持久化说明

| 设置项 | 断电后是否保存 |
|--------|---------------|
| 飞行模式（`ql_dm_set_air_plane_mode`） | **否**，断电重启后恢复为 `QL_DM_AIR_PLANE_MODE_OFF`（全功能模式） |
| modem 状态回调（`ql_ms_dm_set_modem_state_change_ind_cb`） | **否**，断电后失效 |
| 飞行模式回调（`ql_dm_set_air_plane_mode_ind_cb`） | **否**，程序退出后即失效 |

### 9.3 并发与回调调用限制

- 若无特别说明，所述函数均**不支持并发调用**。
- **不能在相关回调函数中调用这些 API 函数**（禁止在回调上下文中重入）。

### 9.4 QooS 机制说明

- 模块中 QooS 机制默认为**开启状态，不可更改**。
- `ql_dm_get_qoos_enable()` 和 `ql_dm_set_qoos_enable()` 均为**兼容接口，无实际效果**。
- `ql_dm_set_qoos_config()` / `ql_dm_get_qoos_config()` 的三个参数分别代表**三轮搜网间隔时间**，范围均为 1～225 秒。

### 9.5 温度读取说明

- `ql_dm_get_temperature()` 返回的是模块**晶振温度**的原始值。
- 实际使用时需将原始值**除以 1000.0** 才能得到摄氏度（如示例：`temperature/1000.0`）。
- 其他温度（芯片温度、PA 温度等）可通过 Linux sysfs 文件系统读取：
  - 温度类型：`/sys/class/thermal/thermal_zoneX/type`
  - 温度数值：`/sys/class/thermal/thermal_zoneX/temp`

### 9.6 射频控制与 AT 指令对应关系

| API 函数 | 等效 AT 指令 | 含义 |
|----------|-------------|------|
| `ql_dm_set_radio_on()` | `AT+CFUN=1` | 全功能模式，开启射频 |
| `ql_dm_set_radio_off()` | `AT+CFUN=0` | 最小功能模式，关闭射频 |

### 9.7 服务异常回调说明

- 当 DM 服务发生异常时（如 modem 崩溃导致 Service Abort），会触发通过 `ql_dm_set_service_error_cb()` 注册的回调函数。
- 已知异常码：`error = -1099` 表示 **Service Abort**（服务中止）。
- 此回调可用于诊断 SDK 服务崩溃（如 CP dump 场景下检测 modem 侧异常）。

### 9.8 函数名前缀异常提醒

文档中 `ql_ms_dm_set_modem_state_change_ind_cb()` 的函数名前缀为 `ql_ms_dm_`，与其他函数的 `ql_dm_` 前缀**不一致**，使用时注意不要写错。

---

*本文档分析基于：Quectel EC200A-CN(TA) QuecOpen(SDK) 设备管理指导 V1.0.1 Preliminary，2024-03-12*
