# EC200A-CN(TA) QuecOpen Linux 系统时间同步

> **文档版本**：V1.0.2 Preliminary  
> **发布日期**：2023-08-11  
> **适用平台**：EC200A-CN(TA)（QuecOpen 模式，车规级模块系列）  
> **关键字**：ql_time_daemon、NITZ、GNSS、NTP、USER、时间同步、时区、RTC、ql_time_conf.json

---

## 文档历史

| 版本 | 日期 | 作者 | 变更描述 |
|------|------|------|---------|
| — | 2022-07-18 | Allen FENG | 文档创建 |
| 1.0.0 | 2022-07-18 | Allen FENG | 临时版本 |
| 1.0.1 | 2023-05-30 | Allen FENG | 修改了查看当前系统时间的时间源的文件（第 3.2 章） |
| 1.0.2 | 2023-08-11 | Allen FENG | 增加配置项 `rtc_sync_enable` 和 `ntp_server_list`（第 3.2 章） |

---

## 目录

- [1. 引言](#1-引言)
- [2. 时间源](#2-时间源)
- [3. 时间同步机制](#3-时间同步机制)
  - [3.1 时间同步机制框架](#31-时间同步机制框架)
  - [3.2 自动同步系统时间](#32-自动同步系统时间)
  - [3.3 手动同步系统时间](#33-手动同步系统时间)
  - [3.4 获取时间同步信息](#34-获取时间同步信息)
- [4. 设置系统时区](#4-设置系统时区)
- [5. 附录 参考文档及术语缩写](#5-附录-参考文档及术语缩写)

---

## 1. 引言

移远通信 EC200A-CN(TA) 模块支持 QuecOpen® 方案；QuecOpen® 是基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。有关 QuecOpen® 的详细信息，请参考文档 [1]。

本文档主要介绍 QuecOpen® 方案下，移远通信 EC200A-CN(TA) 模块的**时间源**、**时间同步机制**和**系统时区设置**。

---

## 2. 时间源

QuecOpen 方案下，EC200A-CN(TA) 支持自动同步系统时间，可支持 **NITZ**、**GNSS** 和 **NTP** 时间源以及**用户自定义时间源**。

**表 1：模块支持的时间源类型**

| 时间源 | 描述 |
|--------|------|
| **NITZ** | NITZ（网络标识和时区）是一种通过无线网络向移动设备提供本地日期和时间、时区、夏时制偏差，以及网络提供商身份信息的机制。NITZ 是运营商的可选服务，目前尚不能确认所有运营商支持 NITZ 授时服务。**获取 NITZ 时间的前提**：运营商基站支持且设备能正常注网。 |
| **GNSS** | GNSS 是可提供三维坐标、速度以及时间信息的空基无线电导航定位系统。GNSS 定位成功之后，可从 GGA 数据中获取 UTC 时间中的时分秒毫秒。**获取 UTC 时间的前提**：模块通过调用 GNSS 相关 API 定位并且成功。 |
| **NTP** | NTP 是一种用于互联网上计算机时钟同步的网络协议，可以使设备与 NTP 服务器进行时间同步。**获取 NTP 时间的前提**：设备可以访问互联网。NTP 时间同步访问互联网，产生少量的流量。 |
| **USER** | 用户自定义时间。通过执行 `echo "user: <时间戳>" > /tmp/ql_time_set_pipe` 将自定义时间通过管道传递给 `ql_time_daemon` 程序，以进行时间同步；详见 **第 3.3 章**。 |

EC200A-CN(TA) QuecOpen 模块包含一个**硬件 RTC**（设备文件 `/dev/rtc1`）；系统断电，RTC 将自动复位；系统软重启或者硬重启时，RTC 可继续工作。

---

## 3. 时间同步机制

### 3.1 时间同步机制框架

时间同步机制框架如下图所示：

```
┌─────────────────────────────────────────────────────────┐
│                      System Time                         │
│   ↑ Sync Time                        ↑ Sync Time        │
│                                                          │
│  Chronyd ←── Boot ──→  ql_time_daemon ←── NMEA ── ql_location │
│                              ↑    ↑    ↑                 │
│                         NTP  │  RTC│  NITZ               │
│                         Time │Time │  Time               │
│                              ↓    ↓    ↓                 │
│                         Network  RTC  Modem              │
└─────────────────────────────────────────────────────────┘
```

**图 1：时间同步机制框架**

- **GNSS 定位有效时**：GNSS 时间源优先级最高。
- **GNSS 定位无效时**：由模块后台进程 `ql_time_daemon` 使用 NTP、NITZ 和 USER 时间源同步系统时间；可通过配置文件 `/etc/ql_time_conf.json` 中的配置项 `time_source_priority` 配置 NTP、NITZ 和 USER 时间源的优先级顺序，详见 **第 3.2 章**。

---

### 3.2 自动同步系统时间

模块后台进程 **`ql_time_daemon`** 会自动同步系统时间，并维护模块的硬件 RTC。

**时间同步状态查看：**

通过标志文件 `/tmp/ql_time_set_flag` 可查看系统是否已经进行时间同步：
- 若标志文件**存在**，则系统已经完成时间同步；
- 若**不存在**，则还未进行时间同步。

同时可以通过文件 `/tmp/ql_time_set_flag`，查看当前系统时间的**时间源**，以及时间同步的**初始时间**。

---

**时间同步配置文件为 `/etc/ql_time_conf.json`**，包含下表所列配置项。

**表 2：时间同步配置文件包含的配置项**

| 配置项 | 可选性 | 描述 |
|--------|--------|------|
| `sync_accuracy_ms` | 必选 | 仅当通过时间源获取到的时间和系统当前时间之差大于该值时，才会自动同步系统时间，以此避免频繁同步。**默认值：2000**。单位：毫秒。若该值为 `0`，表示一旦从时间源获取到时间，就立即自动同步系统时间。 |
| `modem_sync_enable` | 必选 | `1`（默认值）使能通过 NITZ 时间源同步系统时间；`0` 禁止通过 NITZ 时间源同步系统时间。**注**：模块正常注网才能从 NITZ 获取准确的时间。 |
| `gnss_sync_enable` | 必选 | `1`（默认值）使能从定位信息中获取 UTC 时间并同步系统时间；`0` 禁止从定位信息中获取 UTC 时间或同步系统时间。**注**：GNSS 成功定位后才能从定位信息中获取 UTC 时间并同步系统时间。且 GNSS 默认只能同步时分秒，故需要 GNSS 同步时，必须同时开启 `modem_sync_enable`，通过 Modem 获取时区等信息。 |
| `ntp_sync_enable` | 必选 | `1`（默认值）使能通过 NTP 时间源同步系统时间；`0` 禁止通过 NTP 时间源同步系统时间。**注**：模块正常访问 NTP 服务器才能从 NTP 获取准确的时间。 |
| `time_source_priority` | 必选 | 不同时间源的优先级顺序（由低到高排序）。**默认排序：NITZ < NTP < USER < GNSS**。**注**：GNSS 时间源的优先级最高，不可修改。如需禁止通过 GNSS 时间同步系统时间，可设置 `gnss_sync_enable` 为 `0`。 |
| `ntp_sync_policy` | 必选 | 当前支持两种通过 NTP 时间源同步系统时间的模式：<br>**Assist 模式（默认模式）**：在可以访问互联网的前提下，若当前系统时间仍未同步，则自动启动 NTP 进行时间同步；获取时间后，停止 NTP 服务。<br>**Normal 模式**：在可以访问互联网的前提下，模块开机后自动启动 NTP 进行时间同步；时间同步成功后，模块根据 `ntp_resync_interval_s` 的配置，将退出 NTP 或根据配置的时间间隔再次进行时间同步。 |
| `ntp_server_probe_interval_ms` | 必选 | 向 NTP 服务器请求时间信息失败后的重试间隔。**默认值：2000**。单位：毫秒。取值必须为大于等于 1 的整数。 |
| `ntp_server_probe_retry_count` | 必选 | 向 NTP 服务器请求时间信息失败后的重试次数。取值必须为大于等于 1 的整数。**默认值：2**。 |
| `ntp_failed_retry_interval_s` | 必选 | 向所有 NTP 服务器请求时间信息均失败后，重新发起时间同步请求的时间间隔。**默认值：300**。单位：秒。若设置为 `0`，表示不进行重试。 |
| `ntp_resync_interval_s` | 必选 | NTP 时间同步成功之后，下一次进行同步的时间间隔。**默认值：86400**。单位：秒。仅在 Normal 模式下有效。若设置为 `0`，表示时间同步成功之后退出 NTP。建议将该值设置为较大值，例如 86400 秒（1 天）。 |
| `timing_synchronization_enable` | 必选 | 使能从 NITZ 时间源同步系统时间（`modem_sync_enable` 设置为 1）时，是否循环从 Modem 获取时间并同步 Linux 系统时间。`1`（默认值）循环获取；`0` 不循环获取。**注**：循环获取 Modem 时间并进行同步，可减小 Linux 系统在长时间运行后因设备特性导致的 Linux 系统时间和 Modem 时间的误差。 |
| `timing_interval_s` | 必选 | 循环从 Modem 处获取时间并进行系统时间同步的时间间隔。**默认值：3600**。单位：秒。建议将该值设置为较大值，例如 3600 秒。 |
| `chronyd_sync_enable` | 必选 | `1` 系统检查并主动开启 chronyd 服务；`0`（默认值）系统不检查、不控制 chronyd 服务。 |
| `sync_system_time_enable` | 必选 | `1`（默认值）使能系统时间同步；`0` 禁止系统时间同步。 |
| `rtc_sync_enable` | 必选 | `1`（默认值）使能 RTC 时间同步；`0` 禁止 RTC 时间同步。 |
| `ntp_server_list` | 可选 | NTP 服务器地址。**默认值**为 ntp.org 系列的时间同步服务器。 |
| `sync_system_time_to_pipe` | 可选 | 定义 `ql_time_daemon` 中时间同步有关数据的管道路径；详见 **第 3.4 章**。`ql_time_daemon` 进程获取到新时间时向指定管道写入时间数据；数据格式：`time_source=%s time_msec=%lld time_monotonic=%lld`。**默认为空**。 |

---

### 3.3 手动同步系统时间

模块也支持手动同步系统时间：将时间信息写入文件 `/tmp/ql_time_set_pipe`，即可更新系统时间。

**文件写入格式**：`user: <时间戳>`（单位：毫秒）

例如，如需将系统时间设置为 `2019-12-31 17:01:01`，则应写入如下信息：

```bash
echo "user: 1577811661000" > /tmp/ql_time_set_pipe
```

> **备注**：可设置的最早系统时间为 **2019-12-31 17:01:01**，在此之前的时间视为无效时间。

---

### 3.4 获取时间同步信息

模块支持将上述时间同步机制获取的时间传递至用户应用程序。配置 `/etc/ql_time_conf.json` 的 `sync_system_time_to_pipe` 配置项，以定义管道路径；配置完成后，`ql_time_daemon` 程序将会在获取到新时间时，将获取到的**时间戳**、**时间源**以及**当前模块开机时间**写入到管道中以便用户应用程序读取。

**管道数据格式：**

```
time_source=%s time_msec=%lld time_monotonic=%lld
```

**获取时间同步信息的步骤：**

**步骤一**：修改 `/etc/ql_time_conf.json`，配置：

```json
"sync_system_time_to_pipe": "/tmp/ql_time_pipe"
```

**步骤二**：重启模块。

**步骤三**：执行 `cat /tmp/ql_time_pipe` 即可读取时间。

> **备注**：建议将管道路径定义到 `/tmp` 目录下。

---

## 4. 设置系统时区

模块修改时区需要修改 `ql-ol-extsdk-xxx/ql-ol-rootfs/etc/config/system` 进行系统时区设置。

例如，将系统时区设置为 **Asia/Shanghai**，可按照如下步骤设置：

**步骤一**：编辑 `ql-ol-extsdk-xxx/ql-ol-rootfs/etc/config/system` 文件内容如下：

```
config system
    option hostname 'OpenWrt'
    option timezone 'CST-7'
    option zonename 'Asia/Shanghai'
    option ttylogin '0'
    option log_size '64'
    option urandom_seed '0'

config timeserver 'ntp'
    option enabled '1'
    option enable_server '0'
    list server '0.openwrt.pool.ntp.org'
    list server '1.openwrt.pool.ntp.org'
    list server '2.openwrt.pool.ntp.org'
    list server '3.openwrt.pool.ntp.org'
```

若没有上述文件，则需要手动创建一个添加如上格式的内容。在需要修改时区时，直接修改文件中的 `timezone` 字段和 `zonename` 字段即可。

**步骤二**：在 SDK 中编译 rootfs。

**步骤三**：将编译好的 rootfs 烧录到模块中。

**步骤四**：运行 `date` 命令查看当前的系统时区是否已经更新成功。

---

## 5. 附录 参考文档及术语缩写

**表 3：参考文档**

| 文档名称 |
|---------|
| [1] Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

**表 4：术语和缩写**

| 缩写 | 英文全称 | 中文全称 |
|------|---------|---------|
| API | Application Programming Interface | 应用程序编程接口 |
| GNSS | Global Navigation Satellite System | 全球导航卫星系统 |
| IoT | Internet of Things | 物联网 |
| NMEA | NMEA (National Marine Electronics Association) 0183 Interface Standard | NMEA（美国国家海洋电子协会）0183 接口标准 |
| NITZ | Network Identity and Time Zone | 网络标识和时区 |
| NTP | Network Time Protocol | 网络时间协议 |
| PPE | Precise Positioning Engine | 精确定位引擎 |
| RTC | Real-Time Clock | 实时时钟 |
| SDK | Software Development Kit | 软件开发工具包 |
| UTC | Universal Time Coordinated | 通用协调时 |
