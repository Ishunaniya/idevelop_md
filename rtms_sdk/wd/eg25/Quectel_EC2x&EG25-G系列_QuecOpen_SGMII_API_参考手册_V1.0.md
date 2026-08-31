# EC2x&EG25-G 系列 QuecOpen — SGMII API 参考手册

> **模块系列：** LTE Standard 模块系列
> **版本：** 1.0
> **日期：** 2021-04-30
> **状态：** 受控文件
> **版权：** Copyright © Quectel Wireless Solutions Co., Ltd. 2021. 保留一切权利。

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2021-03-31 | Yang LU | 文档创建 |
| 1.0 | 2021-04-30 | Yang LU | 受控版本 |

---

## 目录

- [1 引言](#1-引言)
  - [1.1 适用模块](#11-适用模块)
- [2 SGMII 介绍](#2-sgmii-介绍)
- [3 SGMII API 说明](#3-sgmii-api-说明)
  - [3.1 头文件](#31-头文件)
  - [3.2 枚举](#32-枚举)
  - [3.3 结构体](#33-结构体)
  - [3.4 API 详解](#34-api-详解)
- [4 示例](#4-示例)
- [5 附录 参考文档和术语缩写](#5-附录-参考文档和术语缩写)

### 表格索引

- 表 1：适用模块
- 表 2：参考文档
- 表 3：术语缩写

---

## 1 引言

移远通信 LTE Standard EC2x 系列和 EG25-G 模块支持 QuecOpen® 方案；QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。有关 QuecOpen® 的详细信息，请参考文档 [1]。

本文档主要介绍在 QuecOpen® 方案下 EC2x 系列和 EG25-G 模块的 SGMII 功能相关 API 及其详解。

### 1.1 适用模块

**表 1：适用模块**

| 模块系列 | 模块 |
|---|---|
| EC2x | EC25 系列 |
| EC2x | EC21 系列 |
| EC2x | EC20 R2.1 |
| EC2x | EC20-CN |
| EG25-G | EG25-G |

---

## 2 SGMII 介绍

SGMII 是 PHY 与 MAC 之间的接口，与 GMII 和 RGMII 类似。区别在于 GMII 和 RGMII 均为并行接口，且需要随路时钟；PCB 布线相对复杂，且不适应背板应用。而 SGMII 为串行接口，无需提供额外时钟，MAC 和 PHY 均需 CDR 恢复时钟。SGMII 支持 8b/10b 编码方式，理论上支持的最大速率为 **1.25 Gbps**。

移远通信 LTE Standard EC2x 系列和 EG25-G 模块的 SGMII 接口目前仅支持 **AR8033 PHY 芯片**。该接口符合 **IEEE802.3** 标准，且支持 **10 Mbps / 100 Mbps / 1000 Mbps** 工作模式。

---

## 3 SGMII API 说明

本章节将介绍 SGMII API 的枚举、结构体及详解。

> **备注**
> Linux 开启时默认不启用 SGMII 功能。如有需要，可通过调用 `ql_sgmii_enable` 启用该功能。

### 3.1 头文件

`QL_Ethernet_Mode_Set`、`QL_Ethernet_Mode_Get`、`QL_LAN_DHCP_Config_Set` 和 `QL_LAN_DHCP_Config_Get` 的 SGMII API 接口文件为 **`ql_network.h`**；本文档中列举的其他 API 的 SGMII API 接口文件为 **`ql_sgmii.h`**。

`ql_network.h` 和 `ql_sgmii.h` 均位于 SDK 包中 `ql-ol-sdk/ql-ol-extsdk/include` 路径下。

### 3.2 枚举

#### 3.2.1 ql_sgmii_speed_e

网口速率枚举信息定义如下：

```c
typedef enum {
    QL_SGMII_SPEED_10MHZ,      /* 10 MHz */
    QL_SGMII_SPEED_100MHZ,     /* 100 MHz */
    QL_SGMII_SPEED_1000MHZ     /* 1000 MHz */
} ql_sgmii_speed_e;
```

**参数**

| 参数 | 描述 |
|---|---|
| `QL_SGMII_SPEED_10MHZ` | 10 MHz |
| `QL_SGMII_SPEED_100MHZ` | 100 MHz |
| `QL_SGMII_SPEED_1000MHZ` | 1000 MHz |

#### 3.2.2 ql_sgmii_duplex_e

网口双工模式枚举信息定义如下：

```c
typedef enum {
    QL_SGMII_DUPLEX_FULL = 0,  /* 全双工模式 */
    QL_SGMII_DUPLEX_HALF       /* 半双工模式 */
} ql_sgmii_duplex_e;
```

**参数**

| 参数 | 描述 |
|---|---|
| `QL_SGMII_DUPLEX_FULL` | 全双工模式 |
| `QL_SGMII_DUPLEX_HALF` | 半双工模式 |

#### 3.2.3 ql_sgmii_autoneg_e

以太网卡自协商模式开关枚举信息定义如下：

```c
typedef enum {
    QL_SGMII_AUTONEG_OFF = 0,
    QL_SGMII_AUTONEG_ON
} ql_sgmii_autoneg_e;
```

**参数**

| 参数 | 描述 |
|---|---|
| `QL_SGMII_AUTONEG_OFF` | 关闭自协商模式 |
| `QL_SGMII_AUTONEG_ON` | 开启自协商模式 |

#### 3.2.4 ql_ethernet_mode_e

以太网连接模式枚举信息定义如下：

```c
typedef enum {
    QL_ETHERNET_MODE_LAN_ROUTE = 0,
    QL_ETHERNET_MODE_WAN_ROUTE
} ql_ethernet_mode_e;
```

**参数**

| 参数 | 描述 |
|---|---|
| `QL_ETHERNET_MODE_LAN_ROUTE` | LAN 路由模式。以太网卡作为局域网为对端设备（如：PC）提供网络服务。 |
| `QL_ETHERNET_MODE_WAN_ROUTE` | WAN 路由模式。以太网卡作为上网口，通过对端设备（如：交换机）为模块提供网络服务。该模式下，eth0 端口不加入 bridge0 桥接下，通过 udhcpc 服务获取 IP 地址。 |

### 3.3 结构体

#### 3.3.1 ql_sgmii_info

SGMII 以太网卡信息结构体定义如下：

```c
struct ql_sgmii_info {
    ql_sgmii_autoneg_e autoneg;
    ql_sgmii_speed_e   speed;
    ql_sgmii_duplex_e  duplex;
    unsigned long      tx_bytes;
    unsigned long      rx_bytes;
    unsigned long      tx_pkts;
    unsigned long      rx_pkts;
};
```

**参数**

| 类型 | 参数 | 描述 |
|---|---|---|
| `ql_sgmii_autoneg_e` | `autoneg` | 是否开启自协商模式。 |
| `ql_sgmii_speed_e` | `speed` | 网口速率 |
| `ql_sgmii_duplex_e` | `duplex` | 网口双工模式 |
| `unsigned long` | `tx_bytes` | 发送数据大小 |
| `unsigned long` | `rx_bytes` | 接收数据大小 |
| `unsigned long` | `tx_pkts` | 发送数据包数 |
| `unsigned long` | `rx_pkts` | 接收数据包数 |

#### 3.3.2 ql_lan_dhcp_config_s

DHCP 配置信息结构体定义如下：

```c
typedef struct {
    char          gw_ip[20];         /* 网关地址 */
    char          netmask[20];       /* 子网掩码 */
    unsigned char enable_dhcp;       /* 是否启用 DHCP。布尔类型。 */
    char          dhcp_start_ip[20]; /* DHCP 起始 IP 地址 */
    char          dhcp_end_ip[20];   /* DHCP 结束 IP 地址 */
    unsigned int  lease_time;        /* DHCP 租约时间。单位：秒。 */
} ql_lan_dhcp_config_s;
```

**参数**

| 类型 | 参数 | 描述 |
|---|---|---|
| `char` | `gw_ip` | 网关地址 |
| `char` | `netmask` | 子网掩码 |
| `unsigned char` | `enable_dhcp` | 是否启用 DHCP。布尔类型。 |
| `char` | `dhcp_start_ip` | DHCP 起始 IP 地址 |
| `char` | `dhcp_end_ip` | DHCP 结束 IP 地址 |
| `unsigned int` | `lease_time` | DHCP 租约时间。单位：秒。 |

### 3.4 API 详解

#### 3.4.1 ql_sgmii_enable

该函数用于启用 SGMII 功能。调用该函数将加载 SGMII 驱动，驱动加载成功后，在控制台下可看到 eth0 网口。

> 图（原文配图）：控制台下 SGMII 驱动加载成功后出现 eth0 网口的截图。

eth0 网口启动成功后，qti 程序捕获启动事件并通知 QCMAP_ConnectionManager 进程将 eth0 设备加载到 bridge0 桥上。

> 图（原文配图）：qti 捕获 eth0 启动事件、QCMAP_ConnectionManager 将 eth0 加入 bridge0 的截图。

**函数原型**

```c
int ql_sgmii_enable(void);
```

**参数**

无

**返回值**

| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| `1` | 函数执行失败 |

> **备注**
> 1. eth0 网口默认 MAC 地址为 `00:80:48:BA:D1:30`。可通过命令行修改 MAC 地址，例如：`ifconfig eth0 hw ether 00:80:48:BA:d1:30`；也可直接修改 SGMII 启动脚本 `/etc/init.d/start_emac_le` 中 MAC 地址。
> 2. 调用本文档中其他 API 前，需保证已通过调用该函数启用了 SGMII 功能。

#### 3.4.2 ql_sgmii_disable

该函数用于禁用 SGMII 功能。调用该函数将 eth0 网口从 bridge0 桥下移除，并卸载启动时加载的驱动。

**函数原型**

```c
int ql_sgmii_disable(void);
```

**参数**

无

**返回值**

| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| `1` | 函数执行失败 |

#### 3.4.3 ql_sgmii_speed_set

该函数用于设置网口速率。

**函数原型**

```c
int ql_sgmii_speed_set(ql_sgmii_speed_e speed);
```

**参数**

- `speed`：[In] 网口速率。取值为枚举 `ql_sgmii_speed_e` 中的值，请参考第 3.2.1 章。

**返回值**

| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| `1` | 函数执行失败 |

#### 3.4.4 ql_sgmii_speed_get

该函数用于获取当前设置的网口速率。

**函数原型**

```c
int ql_sgmii_speed_get(ql_sgmii_speed_e *speed);
```

**参数**

- `speed`：[Out] 当前设置的网口速率。取值为枚举 `ql_sgmii_speed_e` 中的值，请参考第 3.2.1 章。

**返回值**

| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| `1` | 函数执行失败 |

#### 3.4.5 ql_sgmii_duplex_set

该函数用于设置网口双工模式。

**函数原型**

```c
int ql_sgmii_duplex_set(ql_sgmii_duplex_e duplex);
```

**参数**

- `duplex`：[In] 网口双工模式。取值为枚举 `ql_sgmii_duplex_e` 中的值，请参考第 3.2.2 章。

**返回值**

| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| `1` | 函数执行失败 |

> **备注**
> 1. 该函数仅在设置了网口速率时（即以太网卡自协商模式为关闭状态，详情请参考第 3.4.7 章）可用。有关如何设置网口速率，请参考第 3.4.3 章。
> 2. 千兆速率 AR8033 芯片仅支持全双工模式。

#### 3.4.6 ql_sgmii_duplex_get

该函数用于获取网口当前设置的双工模式。

**函数原型**

```c
int ql_sgmii_duplex_get(ql_sgmii_duplex_e *duplex);
```

**参数**

- `duplex`：[Out] 网口当前设置的双工模式。取值为枚举 `ql_sgmii_duplex_e` 中的值，请参考第 3.2.2 章。

**返回值**

| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| `1` | 函数执行失败 |

#### 3.4.7 ql_sgmii_autoneg_set

该函数用于设置是否打开以太网卡自协商模式。

**函数原型**

```c
int ql_sgmii_autoneg_set(ql_sgmii_autoneg_e autoneg);
```

**参数**

- `autoneg`：[In] 是否开启以太网卡自协商模式。取值为枚举 `ql_sgmii_autoneg_e` 中的值，请参考第 3.2.3 章。

**返回值**

| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| `1` | 函数执行失败 |

#### 3.4.8 ql_sgmii_autoneg_get

该函数用于获取以太网卡自协商模式是否已经开启。

**函数原型**

```c
int ql_sgmii_autoneg_get(ql_sgmii_autoneg_e *autoneg);
```

**参数**

- `autoneg`：[Out] 是否打开以太网卡自协商模式。取值为枚举 `ql_sgmii_autoneg_e` 中的值，请参考第 3.2.3 章。

**返回值**

| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| `1` | 函数执行失败 |

#### 3.4.9 ql_smgii_info_get

该函数用于获取 SGMII 以太网卡信息，包括传输数据大小、数据包个数、当前运行速率和设置的双工模式。

**函数原型**

```c
int ql_smgii_info_get(struct ql_sgmii_info *info);
```

**参数**

- `info`：[Out] SGMII 以太网卡信息。信息包含在结构体 `ql_sgmii_info` 中，请参考第 3.3.1 章。

**返回值**

| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| `1` | 函数执行失败 |

#### 3.4.10 QL_Ethernet_Mode_Set

该函数用于设置以太网连接模式。

**函数原型**

```c
int QL_Ethernet_Mode_Set(ql_ethernet_mode_e mode);
```

**参数**

- `mode`：[In] 以太网连接模式。取值为枚举 `ql_ethernet_mode_e` 中的值，请参考第 3.2.4 章。默认值：**LAN 路由模式**。

**返回值**

| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| `1` | 函数执行失败 |

#### 3.4.11 QL_Ethernet_Mode_Get

该函数用于获取以太网连接模式。

**函数原型**

```c
int QL_Ethernet_Mode_Get(ql_ethernet_mode_e *mode);
```

**参数**

- `mode`：[Out] 以太网连接模式。取值为枚举 `ql_ethernet_mode_e` 中的值，请参考第 3.2.4 章。

**返回值**

| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| `1` | 函数执行失败 |

#### 3.4.12 QL_LAN_DHCP_Config_Set

该函数用于在 LAN 路由模式下设置网关地址、分配 DHCP 地址及设置租约时间。

**函数原型**

```c
int QL_LAN_DHCP_Config_Set(ql_lan_dhcp_config_s lan_dhcp_config);
```

**参数**

- `lan_dhcp_config`：[In] 网关地址、DHCP 地址及租约时间。信息包含在结构体 `ql_lan_dhcp_config_s` 中，请参考第 3.3.2 章。

**返回值**

| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| `1` | 函数执行失败 |

#### 3.4.13 QL_LAN_DHCP_Config_Get

该函数用于获取当前以太网 LAN 路由模式下设置的网关地址、分配的 DHCP 地址和租约时间。

**函数原型**

```c
int QL_LAN_DHCP_Config_Get(ql_lan_dhcp_config_s *lan_dhcp_config);
```

**参数**

- `lan_dhcp_config`：[Out] 已设置的网关地址、分配的 DHCP 地址及已设置的租约时间。信息包含在结构体 `ql_lan_dhcp_config_s` 中，请参考第 3.3.2 章。

**返回值**

| 返回值 | 含义 |
|---|---|
| `0` | 函数执行成功 |
| `1` | 函数执行失败 |

> **备注**
> 供电中断时，通过该章节（第 3.4 章）所述函数进行的配置不保存，需在模块重启后重新设置。

---

## 4 示例

移远通信提供的 SDK 中含示例文件 `example/sgmii/example_sgmii.c` 和 `example/lan/example_lan.c`。

---

## 5 附录 参考文档和术语缩写

**表 2：参考文档**

| 序号 | 文档名称 | 描述 |
|---|---|---|
| [1] | Quectel_EC2x&EG9x&EG25-G系列_QuecOpen_快速开发指导 | EC2x&EG9x&EG25-G系列 QuecOpen 快速开发指导 |

**表 3：术语缩写**

| 术语 | 英文全称 | 中文全称 |
|---|---|---|
| API | Application Programming Interface | 应用程序编程接口 |
| CDR | Clock Data Recovery | 时钟数据恢复 |
| DHCP | Dynamic Host Configuration Protocol | 动态主机设置协议 |
| GMII | Gigabit Media Independent Interface | 千兆位媒体独立接口 |
| LAN | Local Area Network | 局域网 |
| LTE | Long-Term Evolution | 长期演进 |
| MAC | Medium Access Control | 媒体访问控制 |
| PCB | Printed Circuit Board | 印刷电路板 |
| PHY | Physical | 端口物理层 |
| RGMII | Reduced Gigabit Media Independent Interface | 千兆位介质独立接口 |
| SGMII | Serial Gigabit Media Independent Interface | 串行千兆位媒体独立接口 |
| WAN | Wide Area Network | 广域网 |
