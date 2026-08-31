# EC200A-CN(TA) QuecOpen GNSS API 参考手册

> **来源文档**：Quectel_EC200A-CN(TA)_QuecOpen_GNSS_API_参考手册_V1.0.0_Preliminary_20220811.pdf
> **适用模块**：LTE Standard 模块系列 — EC200A-CN(TA)
> **版本**：1.0.0　**日期**：2022-08-11　**状态**：临时文档（Preliminary）
>
> 本 md 为该 PDF 全文逐项整理，不省略技术内容。法律/版权/免责声明仅摘要点。

---

## 文档历史（修订记录）

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| -（创建） | 2022-03-31 | Zac ZHANG | 文档创建 |
| 1.0.0 | 2022-08-11 | Sunshine HUANG | 临时版本 |

---

## 1 引言

移远通信 EC200A-CN(TA) 模块支持 **QuecOpen®** 方案（开源、基于 Linux 的嵌入式开发平台）。本文档主要介绍如何通过 EC200A-CN(TA) QuecOpen 模块 SDK 中提供的 **GNSS API** 实现模块的 GNSS 相关功能。

**GNSS** 即全球导航卫星系统，泛指所有卫星导航系统，包括：
- **全球卫星导航系统**：GPS、GLONASS、BDS、Galileo 等
- **区域导航系统**：日本的 QZSS、印度的 IRNSS
- **增强型卫星导航系统**：WAAS、SDCM、EGNOS、MSAS、GAGAN

---

## 2 GNSS 功能特性介绍

EC200A-CN(TA) QuecOpen 模块采用 **UC6228CI** 芯片，支持多个系统的 GNSS，可**并行接收和处理** GPS L1、BDS B1、GLONASS L1 和 Galileo E1；同时支持扩展系统 QZSS 和 SBAS；支持 AGNSS 快速定位技术，使定位更快速、更准确。

### 2.1 功能特性

- 支持多星座定位功能，包括 GPS、GLONASS、BDS、Galileo 和 QZSS 等；
- 支持 AGNSS 快速定位技术；
- 支持星基增强系统（SBAS），包括 WAAS、EGNOS、MSAS 和 GAGAN 等；
- 支持精密单点定位和差分定位（DGPS）。

### 2.2 性能指标

**表 1：性能指标**

| 指标 | 性能 | 备注 |
|---|---|---|
| 水平定位精度（CEP50） | < 2 米 | 开阔地带；定位模式：GPS + BDS 模式 |
| 高程定位精度（CEP50） | < 3 米 | 开阔地带；定位模式：GPS + BDS 模式 |
| 速度精度（CEP50） | 0.01 米/秒 | 开阔地带；定位模式：GPS + BDS 模式 |
| 同时跟踪通道数 | 64 | - |
| 冷启动 TTFF | < 28 秒 | 开阔地带；定位模式：GPS + BDS 模式 |
| 热启动 TTFF | ≤ 1 秒 | 开阔地带；定位模式：GPS + BDS 模式 |
| 重捕获时间 | ≤ 1 秒 | 定位模式：GPS + BDS 模式 |
| 捕获灵敏度（冷启动） | -147 dBm | 定位模式：GPS + BDS 模式 |
| 跟踪灵敏度 | -160 dBm | 定位模式：GPS + BDS 模式 |
| 捕获灵敏度（热启动） | -155 dBm | 定位模式：GPS + BDS 模式 |
| 重捕灵敏度 | -158 dBm | 定位模式：GPS + BDS 模式 |

---

## 3 调用流程

GNSS API 调用的基本流程（图 1：GNSS API 基本调用流程）：

```
初始化 GNSS 接口        ql_gnss_init()
        ↓
注册 GNSS 回调函数      ql_gnss_set_ind_cb()
        ↓
注册 GNSS 回调函数      ql_gnss_set_service_error_cb()
        ↓
开启 GNSS              ql_gnss_start()
        ↓
按需调用函数设置参数      ← While(1) 循环
        ↓
关闭 GNSS              ql_gnss_stop()
```

> **备注**
> 1. 不可在回调函数中调用任何其他 GNSS API 函数。
> 2. 不可在回调函数中处理阻塞任务。
> 3. 需使能配置文件 `/etc/ql_locationd.conf` 中的 `dynamic_set=1` 方可调用函数设置参数。

---

## 4 GNSS API

### 4.1 头文件

- GNSS API 头文件为 **`ql_gnss.h`**，位于 SDK 包的 `ql-sysroots/usr/include/ql-sdk/ql_gnss.h`。
- 错误码定义在 **`ql_type.h`**，位于 `ql-sysroots/usr/include/ql-sdk/ql_type.h`。

### 4.2 函数概览

**表 2：函数概览**

| 函数 | 说明 |
|---|---|
| `ql_gnss_init()` | 初始化 GNSS 服务 |
| `ql_gnss_deinit()` | 去初始化 GNSS 服务 |
| `ql_gnss_set_ind_cb()` | 注册 GNSS 数据上报回调函数 |
| `ql_gnss_start()` | 开启 GNSS |
| `ql_gnss_stop()` | 关闭 GNSS |
| `ql_gnss_set_start_mode()` | 设置 GNSS 启动模式 |
| `ql_gnss_get_engine_state()` | 获取当前 GNSS 引擎状态 |
| `ql_gnss_set_constellation()` | 设置星系组合配置 |
| `ql_gnss_get_constellation()` | 获取当前的星系组合配置 |
| `ql_gnss_set_nmea_type()` | 设置上报 NMEA 语句类型 |
| `ql_gnss_get_nmea_version()` | 获取当前 NMEA 版本配置 |
| `ql_gnss_set_nmea_version()` | 设置 NMEA 版本配置 |
| `ql_gnss_set_agnss_mode()` | 设置 AGNSS 启动模式 |
| `ql_gnss_inject_agnss_data()` | 注入 AGNSS 数据 |
| `ql_gnss_inject_utc_time()` | 注入当前 UTC 时间 |
| `ql_gnss_suspend()` | 休眠 GNSS 引擎 |
| `ql_gnss_resume()` | 唤醒 GNSS 引擎 |
| `ql_gnss_set_service_error_cb()` | 注册 GNSS 服务异常回调函数 |

> 通用返回值约定：`QL_ERR_OK` 表示函数执行成功；其他值表示失败，详情请参考 `ql_type.h`。

### 4.3 函数详解

#### 4.3.1 `ql_gnss_init`
初始化 GNSS 服务。
```c
int ql_gnss_init(void)
```
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败
- **备注**：调用其他 GNSS API 之前，必须先调用此函数以初始化 GNSS 服务。

#### 4.3.2 `ql_gnss_deinit`
去初始化 GNSS 服务。
```c
int ql_gnss_deinit(void)
```
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败
- **备注**：完成其他 GNSS API 的调用之后，需调用此函数以释放 GNSS 相关资源。

#### 4.3.3 `ql_gnss_set_ind_cb`
注册 GNSS 数据上报回调函数。
```c
int ql_gnss_set_ind_cb(ql_gnss_ind_cb_f cb)
```
- **参数**：`cb` [In] GNSS 数据上报回调函数
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

#### 4.3.4 `ql_gnss_set_service_error_cb`
注册 GNSS 服务异常上报回调函数。
```c
int ql_gnss_set_service_error_cb(ql_gnss_error_cb_f cb)
```
- **参数**：`cb` [In] GNSS 服务异常回调函数
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

#### 4.3.5 `ql_gnss_start`
开启 GNSS。
```c
int ql_gnss_start(void)
```
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败
- **备注**：
  1. 默认使用配置文件 `/etc/ql_locationd.conf` 配置 GNSS 参数设置。如需调用 API 设置 GNSS 参数，需要更改使能配置文件 `/etc/ql_locationd.conf` 中的 `dynamic_set=1`，并须先调用此函数开启 GNSS。
  2. 用户可根据 IMEI 号查询模块对应的固件版本号。若查询到的固件版本号为 `EC200ACNTAR01A01M2G_OCPU` 或 `EC200ACNTAR01A02M2G_OCPU`，则可能会出现执行 `ql_gnss_start()` 函数失败的情况。此时可修改 `ql_locationd.conf` 中的波特率为 **9600**。若不是上述两个版本号，则不会出现执行失败的情况。

#### 4.3.6 `ql_gnss_stop`
关闭 GNSS。
```c
int ql_gnss_stop(void)
```
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

#### 4.3.7 `ql_gnss_set_start_mode`
设置 GNSS 引擎启动模式。
```c
int ql_gnss_set_start_mode(QL_GNSS_START_MODE_E mask)
```
- **参数**：`mask` [In] GNSS 启动模式；详见 4.3.7.1
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

##### 4.3.7.1 `QL_GNSS_START_MODE_E`
```c
typedef enum {
    /** Cold Start Gnss*/
    QL_GNSS_COLD_START = 0,
    /** Warm Start Gnss */
    QL_GNSS_WARM_START,
    /** Hot Start Gnss*/
    QL_GNSS_HOT_START,
} QL_GNSS_START_MODE_E
```

| 成员 | 描述 |
|---|---|
| `QL_GNSS_COLD_START` | 冷启动 |
| `QL_GNSS_WARM_START` | 温启动 |
| `QL_GNSS_HOT_START` | 热启动 |

#### 4.3.8 `ql_gnss_get_engine_state`
获取 GNSS 引擎状态。
```c
int ql_gnss_get_engine_state(QL_GNSS_ENGINE_STATE_E *state)
```
- **参数**：`state` [Out] GNSS 引擎状态；详见 4.3.8.1
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

##### 4.3.8.1 `QL_GNSS_ENGINE_STATE_E`
```c
typedef enum
{
    /** gnss engine is off */
    QL_GNSS_ENGINE_STATE_OFF = 0,
    /** gnss engine is on */
    QL_GNSS_ENGINE_STATE_ON = 1,
} QL_GNSS_ENGINE_STATE_E
```

| 成员 | 描述 |
|---|---|
| `QL_GNSS_ENGINE_STATE_ON` | 开启 |
| `QL_GNSS_ENGINE_STATE_OFF` | 关闭 |

#### 4.3.9 `ql_gnss_set_constellation`
设置星系组合配置，即参与定位的星座。
```c
int ql_gnss_set_constellation(QL_GNSS_CONSTELLATION_MASK_E mask)
```
- **参数**：`mask` [In] 星系组合配置；详见 4.3.9.1
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

##### 4.3.9.1 `QL_GNSS_CONSTELLATION_MASK_E`
```c
typedef enum
{
    GPS_SBAS_QZSS               = 0x08,
    BDS_ONLY                    = 0x02,
    GPS_BDS_GALILEO_SBAS_QZSS   = 0x11,
} QL_GNSS_CONSTELLATION_MASK_E
```

| 成员 | 描述 |
|---|---|
| `GPS_SBAS_QZSS` | GPS + SBAS + QZSS |
| `BDS_ONLY` | 仅 BDS |
| `GPS_BDS_GALILEO_SBAS_QZSS` | GPS + BDS + GAL + SBAS + QZSS |

#### 4.3.10 `ql_gnss_get_constellation`
获取当前的星系组合配置。
```c
int ql_gnss_get_constellation(QL_GNSS_CONSTELLATION_MASK_E *mask)
```
- **参数**：`mask` [Out] 获取的星系组合配置；详见 4.3.9.1
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

#### 4.3.11 `ql_gnss_set_nmea_version`
设置 NMEA 语句版本。
```c
int ql_gnss_set_nmea_version(QL_GNSS_NMEA_VERSION_ID_E version)
```
- **参数**：`version` [In] NMEA 语句版本；详见 4.3.11.1
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

##### 4.3.11.1 `QL_GNSS_NMEA_VERSION_ID_E`
```c
typedef enum {
    QL_GNSS_NMEA_VERSION_V30,    /* NMEA 3.0 */
    QL_GNSS_NMEA_VERSION_V41     /* NMEA 4.1 */
} QL_GNSS_NMEA_VERSION_ID_E
```

| 成员 | 描述 |
|---|---|
| `QL_GNSS_NMEA_VERSION_V30` | 3.0 版本 |
| `QL_GNSS_NMEA_VERSION_V41` | 4.1 版本 |

#### 4.3.12 `ql_gnss_get_nmea_version`
获取当前 NMEA 语句版本。
```c
int ql_gnss_get_nmea_version(QL_GNSS_NMEA_VERSION_ID_E *version)
```
- **参数**：`version` [Out] 获取的当前 NMEA 语句版本
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

#### 4.3.13 `ql_gnss_set_nmea_type`
设置 NMEA 语句类型配置。
```c
int ql_gnss_set_nmea_type(uint32_t mask)
```
- **参数**：`mask` [In] NMEA 类型，用户可自由组合；详见 4.3.13.1
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

##### 4.3.13.1 `QL_GNSS_NMEA_TYPE_ID_E`
```c
typedef enum {
    QL_NMEA_TYPE_BITMASK_GGA  = (1<<0),
    QL_NMEA_TYPE_BITMASK_GLL  = (1<<1),
    QL_NMEA_TYPE_BITMASK_GSA  = (1<<2),
    QL_NMEA_TYPE_BITMASK_GSV  = (1<<3),
    QL_NMEA_TYPE_BITMASK_RMC  = (1<<4),
    QL_NMEA_TYPE_BITMASK_VTG  = (1<<5),
    QL_NMEA_TYPE_BITMASK_ZDA  = (1<<6),
    QL_NMEA_TYPE_BITMASK_GST  = (1<<7)
} QL_GNSS_NMEA_TYPE_ID_E
```

| 成员 | 描述 |
|---|---|
| `QL_NMEA_TYPE_BITMASK_GGA` | 输出 GGA 语句 |
| `QL_NMEA_TYPE_BITMASK_GLL` | 输出 GLL 语句 |
| `QL_NMEA_TYPE_BITMASK_GSA` | 输出 GSA 语句 |
| `QL_NMEA_TYPE_BITMASK_GSV` | 输出 GSV 语句 |
| `QL_NMEA_TYPE_BITMASK_RMC` | 输出 RMC 语句 |
| `QL_NMEA_TYPE_BITMASK_VTG` | 输出 VTG 语句 |
| `QL_NMEA_TYPE_BITMASK_ZDA` | 输出 ZDA 语句 |
| `QL_NMEA_TYPE_BITMASK_GST` | 输出 GST 语句 |

> **备注**：若设置相应 bit 为 1，则表示设置输出对应的 NMEA 语句；若设置为 0 则不输出。

#### 4.3.14 `ql_gnss_set_agnss_mode`
设置 AGNSS 的注入方式。
```c
int ql_gnss_set_agnss_mode(uint32_t mask)
```
- **参数**：`mask` [In] AGNSS 的星系组合模式；详见 4.3.14.1
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

##### 4.3.14.1 `QL_GNSS_AGNSS_MODE_E`
```c
typedef enum {
    QL_AGNSS_BITMASK_GPS      = (1<<0),
    QL_AGNSS_BITMASK_BD       = (1<<1),
    QL_AGNSS_BITMASK_GLONASS  = (1<<2),
    QL_AGNSS_BITMASK_GALILEO  = (1<<3),
    QL_AGNSS_BITMASK_QZSS     = (1<<4)
} QL_GNSS_AGNSS_MODE_E
```

| 成员 | 描述 |
|---|---|
| `QL_AGNSS_BITMASK_GPS` | 注入 GPS 星历数据 |
| `QL_AGNSS_BITMASK_BD` | 注入 BDS 星历数据 |
| `QL_AGNSS_BITMASK_GLONASS` | 注入 GLONASS 星历数据 |
| `QL_AGNSS_BITMASK_GALILEO` | 注入 GALILEO 星历数据 |
| `QL_AGNSS_BITMASK_QZSS` | 注入 QZSS 星历数据 |

> **备注**：若设置相应 bit 为 1，则表示注入对应的星历数据；若设置为 0 不注入。

#### 4.3.15 `ql_gnss_inject_agnss_data`
注入 AGNSS 数据。
```c
int ql_gnss_inject_agnss_data(curl_rsp_msg_t data)
```
- **参数**：`data` [In] AGNSS 数据
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

#### 4.3.16 `ql_gnss_inject_utc_time`
注入 UTC 时间戳。
```c
int ql_gnss_inject_utc_time(uint64_t timestamp)
```
- **参数**：`timestamp` [In] 注入的 UTC 时间戳
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

#### 4.3.17 `ql_gnss_suspend`
休眠 GNSS 引擎。
```c
int ql_gnss_suspend(void)
```
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

#### 4.3.18 `ql_gnss_resume`
唤醒 GNSS 引擎。
```c
int ql_gnss_resume(void)
```
- **参数**：无
- **返回值**：`QL_ERR_OK` 成功 / 其他值失败

### 4.4 示例代码

本章所述代码示例均摘自 `ql-ol-sdk/sample/gnss`。

```c
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "test_utils.h"
#include "ql-sdk/ql_type.h"
#include "ql-sdk/ql_gnss.h"

static void gnss_ind_cb(void *msg)
{
    uint8_t *msg_id = NULL;
    msg_id = msg;
    if(*msg_id == QL_GNSS_NMEA_MSG)
    {
        nmea_srv_ind_msg *data = (nmea_srv_ind_msg *)msg;
        printf("time=%d %s\n",data->time,data->nmea_sentence);
    }
    else if (*msg_id == QL_GNSS_STATUS_MSG)
    {
        gnss_status_ind_msg *data = (gnss_status_ind_msg *)msg;
        switch(data->g_status)
        {
            case QL_GNSS_STATUS_FOTA_START:
                printf("gnss engine fota req\n");
                break;
            case QL_GNSS_STATUS_FOTA_BOOT:
                printf("gnss engine fota bootloader\n");
                break;
            case QL_GNSS_STATUS_FOTA_FIRM:
                printf("gnss engine fota firmware\n");
                break;
            case QL_GNSS_STATUS_WORKING:
                printf("gnss engine working\n");
                break;
            default:
                break;
        };
    }
    else
    {
        printf("Invalue msg !!!");
    }
}

static void gnss_service_error_cb(int error)
{
    printf("===== GNSS Service Abort =====\n");
}

int main(int argc, char *argv[])
{
    int err = QL_ERR_OK;
    uint32_t mask = 0;
    int input = 0;

    err = ql_gnss_init();
    if(err == QL_ERR_OK)
    {
        printf("ql_gnss_init Successful\n");
    }
    else
    {
        printf("Failed to ql_gnss_init, err=%d\n", err);
    }
    err = ql_gnss_set_ind_cb(gnss_ind_cb);
    if(err != QL_ERR_OK)
    {
        printf("Failed to ql_gnss_set_ind_cb, err=%d\n", err);
    }
    else
    {
        printf("Successful\n");
    }
    err = ql_gnss_set_service_error_cb(gnss_service_error_cb);
    if(err == QL_ERR_OK)
    {
        printf("Successful\n");
    }
    else
    {
        printf("Failed to item_ql_gnss_set_service_error_cb, err=%d\n", err);
    }
    err = ql_gnss_start();
    if(err != QL_ERR_OK)
    {
        printf("Failed to start gnss , err=%d\n", err);
    }
    else
    {
        printf("ql_gnss_start Successful\n");
    }
    sleep(2);

    err = ql_gnss_get_engine_state(&mask);
    if(err != QL_ERR_OK)
    {
        printf("Get engine state faild %d\n",err);
    }
    else
    {
        if(mask)
        {
            printf("Engine state is on\n");
        }
        else
        {
            printf("Engine state is off\n");
        }
    }
    err = ql_gnss_get_nmea_version(&mask);
    if(err != QL_ERR_OK)
    {
        printf("Failed to get nmea_version with version:%d, err=%d\n",mask, err);
    }
    else
    {
        printf("Successful\n");
        if(QL_GNSS_NMEA_VERSION_V30 == mask)
        {
            printf("Nmea_version is 3.0\n");
        }
        else if(QL_GNSS_NMEA_VERSION_V41 == mask)
        {
            printf("Nmea_version is 4.0\n");
        }
    }
    while(1)
    {
        scanf("%d",&input);
        if(input == -1)
        {
            err = ql_gnss_stop();
            if(err != QL_ERR_OK)
            {
                printf("Failed to stop gnss, err=%d\n", err);
            }
            else
            {
                printf(" ql_gnss_stop Successful\n");
            }
            err = ql_gnss_deinit();
            if(err != QL_ERR_OK)
            {
                printf("Failed to ql_gnss_deinit, err=%d\n", err);
            }
            else
            {
                printf(" ql_gnss_deinit Successful\n");
            }
            break;
        }
    }
    return 0;
}
```

---

## 5 AGNSS 辅助定位技术

### 5.1 功能简介

EC200A-CN(TA) 模块支持 AGNSS 辅助定位技术。该服务可为全球提供 **GPS、BDS、Galileo、GLONASS、QZSS** 五个系统的实时星历，通过 **HTTP 协议**传输星历信息。

### 5.2 功能使用

AGNSS 功能需通过互联网访问服务器，所以设备需能访问互联网。访问 AGNSS 服务器获取星历数据需要使用 AGNSS 账号。

#### 5.2.1 服务器地址解析

每个使用 AGNSS 功能的用户会被分配一个二级域名（URL），例如：`unicore-api1.rx-networks.cn`。

为保证全球范围内的服务质量，AGNSS 服务器使用**动态负载平衡**技术。域名指向的 IP 是不固定的，所以不可直接通过 IP 地址访问服务器。需先通过 DNS 解析到 IP 地址，再使用 IP 地址访问服务器。

#### 5.2.2 账号认证

AGNSS 服务通过 HTTP 协议提供星历数据，在 HTTP 请求中必须包含账号信息。HTTP 请求中认证字段的格式如下：

```
Authorization: RXN - BASIC cId = ,mId = ,dId = ,pw =
```

**表 3：账号认证参数描述**

| 参数名 | 账号示例 | 描述 | 备注 |
|---|---|---|---|
| `cId` | ID | 用户名 | - |
| `mId` | mId | 用户标识 | - |
| `dId` | - | 设备 ID | 每个设备 ID 唯一，长度 50 字节以内，为保证服务质量，每个 ID 每天访问 12 次以后可能会被拒绝 |
| `pw` | Base64 Password | 密码 | - |

#### 5.2.3 数据请求

数据请求的格式需符合 HTTP 协议。一个完整的请求包含 HTTP 头和请求内容（Content）两部分。按照 HTTP 协议规定，在两部分之间必须有一个空行。请求内容部分符合 JSON 数据格式。以向服务器请求 GPS 和 BDS 的星历数据为例：

```http
POST /rxn-api/locationApi/rtcm HTTP/1.1
Host: XXXXXXX
Authorization: RXN-BASIC cId=XXXXXX,mId=XXXXX,dId=XXXXXXXXXX,pw=XXXXXX
Content-type: application/json
Accept: application/octet-stream
Content-length: XX

[{"rtAssistance":{"format":"rtcm","msgs":["GPS:1NAF","BDS:2NAF"]}}]
```

HTTP 协议头中的 `Host` 字段为服务器的域名（URL）。请求内容的长度必须与 HTTP 头中的 `Content-length` 字段相符。

**表 4：请求内容字段描述**

| 字段 | 描述 |
|---|---|
| `rtAssistance` | 表示请求的实时星历数据。此字段为固定值，不可更改。 |
| `"format":"rtcm"` | 表示请求的星历数据的格式为 RTCM 格式。此字段为固定值，不可更改。 |
| `"msgs":["GPS:1NAF","BDS:2NAF"]` | 表示请求的是 GPS 和 BDS 的星历数据。此字段可由用户根据需要自由组合，服务器支持的各星座标识如下：<br>GPS: `1NAF` 代表 GPS 星历<br>BDS: `2NAF` 代表 BDS 星历<br>GLO: `2NAF` 代表 GLONASS 星历<br>GAL: `2NAF` 代表 Galileo 星历<br>QZS: `2NAF` 代表 QZSS 星历<br>例如需要请求 GPS+GAL+BDS 的星座星历，则应为：`"msgs":["GPS:1NAF","GAL:2NAF","BDS:2NAF"]` |

#### 5.2.4 数据回复

AGNSS 服务器收到一个合法的请求后会返回符合 HTTP 协议格式的数据，内容包含 HTTP 协议头和星历数据两部分。两部分之间必须有一个空行。示例：

```http
HTTP/1.1 200 OK
Content-Type: application/octet-stream
Date: Tue, 06 Nov 2018 08:49:10 GMT
Content-Length: XXXX
Connection: keep-alive

data…
```

HTTP 协议头的第一行中的 “200” 是 HTTP 的错误码。

**表 5：HTTP 错误码定义**

| HTTP 错误码 | 含义 | 解释 |
|---|---|---|
| 200 | 正常 | 未出现任何异常时返回此值 |
| 204 | 没有数据 | 服务器接收到合法请求，但无合法星历数据时返回此值 |
| 400 | 非法请求 | 请求不合法时返回此值 |
| 401 | 认证不通过 | 当服务器检测到用户名密码等异常时返回此值 |
| 500 | 服务器异常 | 当服务器发生异常时返回此值 |
| 501 | 暂停服务 | 当 HTTP 服务暂时不可用时返回此值 |

#### 5.2.5 数据注入

接收到服务器返回的数据后，只需依据 HTTP 头和数据之间的空行将 HTTP 头去掉，将其余辅助数据直接通过串口发送给定位芯片或模块即可。

> **备注**
> 1. 使用 AGNSS 前，请确保已正常拨号，并能正常连接网络。
> 2. 移远通信仅提供 `ql_gnss_inject_agnss_data()` 用于开启 AGPS，AGNSS 的账号和密码已经包含在 SDK 中，无需额外输入。

---

## 6 附录 参考文档及术语缩写

**表 6：参考文档**

| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

**表 7：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| AGNSS | Assisted GNSS | 辅助 GNSS 技术 |
| API | Application Programming Interface | 应用程序接口 |
| BDS | BeiDou Navigation Satellite System | 北斗导航系统 |
| DNS | Domain Name Server | 域名系统（服务）协议 |
| EGNOS | European Geostationary Navigation Overlay Service | 欧洲地球静止导航重叠服务 |
| GAGAN | GPS Aided Geo Augmented Navigation | GPS 辅助型静地轨道增强导航 |
| Galileo | Galileo satellite navigation system | 伽利略导航系统 |
| GLONASS | GLObal NAvigation Satellite System (Russia) | 格洛纳斯导航定位系统 |
| GNSS | Global Navigation Satellite Systems | 全球导航卫星系统 |
| GPS | Global Positioning System | 全球定位系统 |
| HTTP | Hypertext Transfer Protocol | 超文本传输协议 |
| IoT | Internet of Things | 物联网 |
| IRNSS | Indian Regional Navigation Satellite System | 印度区域导航卫星系统 |
| MSAS | Multi-functional Satellite Augmentation System (Japan) | 多功能卫星增强系统（日本） |
| NMEA | NMEA (National Marine Electronics Association) 0183 Interface Standard | NMEA（美国国家海洋电子协会）0183 接口标准 |
| QZSS | Quasi-Zenith Satellite System | 准天顶卫星系统 |
| SBAS | Satellite-Based Augmentation System | 星基增强系统 |
| SDCM | System of Differential Correction and Monitoring | 差分校正和监测系统 |
| SDK | Software Development Kit | 软件开发工具包 |
| TTFF | Time To First Fix | 首次定位时间 |
| URL | Uniform Resource Locator | 统一资源定位符 |
| UTC | Universal Time Coordinated | 通用协调时 |
| WAAS | Wide Area Augmentation System | 广域增强系统 |

---

*版权所有 © 上海移远通信技术股份有限公司 2022，保留一切权利。*
