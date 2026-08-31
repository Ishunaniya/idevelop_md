# EC200A-CN(TA) QuecOpen 网络管理应用指导 — 完整分析

> **文档信息**
> - 原文档：Quectel_EC200A-CN(TA)_QuecOpen_网络管理使用指导_V1.0.0_Preliminary_20220715.pdf
> - 版本：1.0.0 | 日期：2022-07-15 | 状态：临时文件（Preliminary）
> - 作者：Allen FENG（移远通信）| 系列：车规级模块系列

---

## 目录

1. [引言](#1-引言)
2. [网络管理概念解释](#2-网络管理概念解释)
3. [QL_NF 介绍](#3-ql_nf-介绍)
   - 3.1 [QL_NF 规则配置](#31-ql_nf-规则配置)
   - 3.2 [NF_INTF](#32-nf_intf)
   - 3.3 [VLAN 管理](#33-vlan-管理)
   - 3.4 [桥接管理](#34-桥接管理)
   - 3.5 [Wi-Fi 管理](#35-wi-fi-管理)
   - 3.6 [LAN 管理](#36-lan-管理)
   - 3.7 [静态 ARP 规则管理](#37-静态-arp-规则管理)
   - 3.8 [数据拨号](#38-数据拨号)
   - 3.9 [转发策略](#39-转发策略)
   - 3.10 [QL_NF API](#310-ql_nf-api)
   - 3.11 [注意事项](#311-注意事项)
4. [场景案例](#4-场景案例)
   - 4.1 [场景一：单路 APN + Wi-Fi/ETH/USB LAN](#41-场景一)
   - 4.2 [场景二：双路 APN + URL/IP 分流](#42-场景二)
   - 4.3 [场景三：VLAN 隔离 + 双路 APN 分流（含真实测试）](#43-场景三)
   - 4.4 [场景四：多 VLAN + 静态 IP + 静态 ARP + 三路 APN](#44-场景四)
5. [附录：参考文档及术语缩写](#5-附录参考文档及术语缩写)

---

## 1 引言

移远通信 EC200A-CN(TA) 模块支持 **QuecOpen®** 方案。QuecOpen® 是基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。

本文档主要介绍 EC200A-CN(TA) 模块的**网络管理框架（QL_NF）**，涉及：

- 数据拨号
- LAN 管理
- 网络接口管理
- 数据转发策略管理

---

## 2 网络管理概念解释

本章引用常用路由器的功能图，解释网络管理相关概念。

### 路由器配置参考（表 1）

| 项目 | 说明 |
|------|------|
| WAN 口数量 | 1 个，以太网口 |
| LAN 口数量 | 2 个，以太网口 |
| WIFI | 支持 |
| 防火墙 | 支持 |
| 管理方式 | HTTP Server |

### 核心概念说明

**路由器功能图描述（图 1）：**

```
互联网（云端）
      │ RJ45
   eth1（WAN 接口）
      │
  ┌───┴──────────────────────────┐
  │  防火墙                       │
  │  ┌──────────────────────┐    │
  │  │ 本地程序 APP1 APP2 … │    │
  │  │ 本地服务 HTTP/DHCP/DNS│    │
  │  │ 转发策略              │    │
  │  └──────────────────────┘    │
  │  防火墙                       │
  │  wlan0  eth0.1  eth0.2       │
  └──┬──────┬────────┬────────────┘
     │      │        │
   手机     PC     打印机（LAN 设备）
```

- **网络接口（Network Interface）**：Linux 系统的网卡名。以太网对应 eth1、eth0.1、eth0.2；WIFI 对应 wlan0。
  - **WAN 接口**：连接外部网络的 Linux 网络接口。以太网接入 Internet 时对应 eth1，可同时存在多个 WAN 口。在移远通信模块上，WAN 接口还可以为拨号网络接口、USB 设备网络接口（ECM、RNDIS、NCM）和 WIFI Station 模式网络接口等。
  - **LAN 接口**：接入 LAN 设备的网络接口。手机通过 Wi-Fi 接入路由器，PC 和打印机通过 ETH 接入路由器。对应 LAN 口分别为 wlan0、eth0.1、eth0.2。在移远通信模块上，LAN 接口还可以为 USB 网络接口（ECM、RNDIS 和 NCM）等。

- **LAN 设备（LAN Device）**：通过 LAN 口接入局域网的设备，如手机、PC 和打印机等。

- **本地服务（Local Service）**：在路由器运行的网络服务程序，如 DHCP Server、DNS Server 和 HTTP Server。

- **本地程序（Local Application）**：在路由器运行且会访问网络的程序，如 APP1 和 APP2。

- **DHCP Server**：为 LAN 设备提供 IP 地址管理服务。

- **DNS Server**：为 LAN 设备提供域名解析服务。

- **防火墙（Firewall）**：阻止或者允许外部设备通过 WAN 口或 LAN 口访问本地服务。防火墙允许 LAN 设备访问 DHCP Server、DNS Server 和 HTTP Server，禁止外部网络设备访问 DHCP Server 和 DNS Server，同时出于安全考虑，也禁止其 HTTP Server。

- **转发策略（Forwarding Policy）**：LAN 设备通过路由器访问外部网络，路由器内的一些网络配置策略，包括路由处理和转发处理等。

---

## 3 QL_NF 介绍

传统的网络管理基于 Linux 的网络接口，实现路由和转发等策略。网络接口状态变化以及各种网络工具的配合使用，导致网络处理逻辑复杂而且难以维护。对于一些复杂应用场景，开发者需要了解各种网络协议、Linux 的网络技术（route、bridge 和 netfilter 等）和网络工具的使用（IP、iptables、ebtables 和 brctl 等）方可进行开发，这不但加长了开发周期，而且一些需要依靠经验解决的问题无法得到解决。

为解决这个问题，移远通信根据实际应用场景，**把网络设备管理、转发策略等封装为一系列可描述的命令**，开发者无需管理技术细节和了解路由和转发处理，只需了解第 2 章的相关概念，设置相应的规则，结合实际应用场景，即可完成规则的配置。

### QL_NF 整体框架（图 2）

```
┌──────────────────────────────────────────────────────────────────────┐
│  WAN1/WAN2/WAN3/…接口  ←→  防火墙 ←→  本地服务  ←→  防火墙  ←→  LAN1/2/3…接口  │
│                                                                       │
│                    ACL（转发策略） / QOS / URL Binder                  │
│                                         ←→  本地程序                  │
├──────────────────────────────────────────────────────────────────────┤
│                    QUECTEL 网络框架接口层（QL_NF）                     │
├──────────────────────────────────────────────────────────────────────┤
│                    Linux 网络框架接口层                                │
│  数据拨号 | USB 网络设备 | VLAN 设备 | ETH | VPN | PPP               │
│  WIFI AP 模式 | WIFI Station 模式 | Linux 桥接                        │
└──────────────────────────────────────────────────────────────────────┘
```

基于 Linux 网络设备接口，移远通信封装了一层网络接口（**Quectel Network Interface**），后面简称 **NF_INTF**，LAN 管理、WAN 管理、转发策略等均基于 NF_INTF。

---

## 3.1 QL_NF 规则配置

QL_NF 规则以可描述命令方式配置，当前支持**普通格式**配置。

### 命令格式

```
<action> <nf_target> <target_name> <param_list>
```

### 参数说明

| 参数 | 说明 |
|------|------|
| `<action>` | QL_NF 目标规则操作。支持：`add`（添加）、`del`（删除）、`set`（配置）、`get`（获取信息或配置）、`start`（使能规则）、`stop`（去使能规则） |
| `<nf_target>` | QL_NF 目标。支持：`interface`（NF_INTF 管理）、`acl`（转发策略/URL 绑定管理）、`data_call`（数据拨号管理）、`dns_srv`（DNS 服务管理）、`dhcp_srv`（DHCP 服务管理）、`networks`（无线网络管理，含注册/异常处理等）、`qos`（QOS 管理，待实现）、`firewall`（防火墙管理，待实现） |
| `<target_name>` | 由 `<nf_target>` 定义，通常为 QL_NF 目标实例的唯一标识。`all` 是特殊关键字，表示所有 QL_NF 目标 |
| `<param_list>` | 参数列表。支持同时获取或配置多个参数，参数之间以空格分隔。设置参数时，参数名和参数值以 `=` 号连接，例如：`param1 = value1` 和 `param2 = value2` |

### 三种配置方式

1. **预加载方式（推荐）**：系统启动后，QL_NF 服务加载 `/etc/ql_nf_preload.conf` 文件中的配置规则。
2. **API 接口**：头文件为 `ql_nf.h`，示例代码参见 `sample/nf`。
3. **shell 命令**：`ql_cmd nf xxxx`。建议在调试时使用此方式，调试完毕后采用**方式 1** 固化配置。

### 特殊命令

```bash
get all          # 获取当前支持的所有 QL_NF 目标
del all          # 删除所有规则
```

---

## 3.2 NF_INTF

### 3.2.1 NF_INTF 规则配置

#### 命令格式

```
<action> interface <interface_name> <param_list>
```

#### 参数说明

| 参数 | 说明 |
|------|------|
| `<action>` | NF_INTF 操作。支持：`add`（添加实例）、`del`（删除实例）、`set`（设置参数）、`get`（获取状态或配置，返回配置参数或状态参数）、`start`（开始 NF_INTF 寻址，见 3.2.2）、`stop`（停止 NF_INTF 寻址） |
| `<interface_name>` | NF_INTF 名称。单位：字节，最大长度 32 字节。NF_INTF 的唯一标识，其他 QL_NF 目标规则都使用 `<interface_name>` 引用对应的 NF_INTF |
| `<param_list>` | 参数列表 |

#### 操作示例

```bash
# 添加 NF_INTF 规则（详见 3.2.4）
add interface <interface_name> <key_params>

# 删除 NF_INTF 规则
del interface <interface_name>

# 配置 NF_INTF 规则（<param_list> 为配置参数列表，见表 2）
set interface <interface_name> <param_list>

# 获取 NF_INTF 状态参数（状态参数见表 3；若 interface_name 为 all，获取所有 NF_INTF 状态参数）
get interface <interface_name> class=info

# 获取 NF_INTF 配置参数（配置参数见表 2；若 interface_name 为 all，获取所有 NF_INTF 配置参数）
get interface <interface_name> class=config
```

---

### NF_INTF 配置参数（表 2）

| 参数 | 类型 | 描述 |
|------|------|------|
| `proto` | 字符串 | 寻址方式。支持：`none`（未设置）、`dhcp`（DHCP 协议寻址）、`static`（静态 IP 地址）、`inherit`（继承网络接口寻址方式，当前只有 `data_call` 类型会自动设置为该方式） |
| `mtu` | 整数 | MTU 值。范围：1~1518 |
| `mac` | MAC 地址格式字符串 | MAC 地址 |
| `ip` | IP 地址格式字符串 | 设置 IP 地址（`static` 寻址方式使用） |
| `netmask` | IP 地址格式字符串 | 设置子网掩码（`static` 寻址方式使用） |
| `gateway` | IP 地址格式字符串 | 设置网关（`static` 寻址方式使用） |
| `dnsp` | IP 地址格式字符串 | 首选 DNS。注意：设置后会覆盖 NF_INTF 使用其他寻址方式获取的 DNS |
| `dnss` | IP 地址格式字符串 | 备选 DNS。注意：设置后会覆盖 NF_INTF 使用其他寻址方式获取的 DNS |
| `conn` | 字符串 | 自动重连类型（DHCP 寻址方式使用，当寻址失败后是否进行重试）。`once`：不重试，NF_INTF 进入 `disconnected` 状态；`auto`：自动重试 |
| `vendor` | 字符串 | 制造商名称（DHCP 寻址方式使用，DHCP OPTION 60 字段）。长度范围：1~32 字节 |
| `hostname` | 字符串 | 主机名称（DHCP 寻址方式使用，DHCP OPTION 50 字段）。长度范围：1~32 字节 |
| `addif` | 字符串 | 把传入的 NF_INTF 加入当前 `bridge` 类型的 NF_INTF 中。长度范围：1~32 字节 |
| `delif` | 字符串 | 把传入的 NF_INTF 从当前 `bridge` 类型的 NF_INTF 中删除。长度范围：1~32 字节 |

---

### NF_INTF 状态参数（表 3）

| 参数 | 类型 | 描述 |
|------|------|------|
| `name` | 字符串 | NF_INTF 名称。长度范围：1~32 字节 |
| `status` | 字符串 | NF_INTF 状态。取值见第 3.2.3 章 |
| `ifname` | 字符串 | 对应 Linux 网络接口名称。长度范围：1~32 字节 |
| `ip` | IP 地址格式字符串 | IP 地址 |
| `netmask` | IP 地址格式字符串 | 子网掩码 |
| `gateway` | IP 地址格式字符串 | 网关 |
| `dnsp` | IP 地址格式字符串 | 首选 DNS |
| `dnss` | IP 地址格式字符串 | 备选 DNS |

---

### 3.2.2 NF_INTF 寻址方式

NF_INTF 获取 IP 地址后，其他的 QL_NF 规则才能生效。NF_INTF 获取 IP 地址的过程，称为**寻址**。

当前支持寻址方式如下：

- **dhcp**：使用 DHCP 协议获取 IP 地址。
- **static**：静态 IP 地址（或称为手动 IP 地址）。若 `conn` 参数设置为 `auto` 模式，并且已经配置静态 IP 地址，NF_INTF 会立即进入 `connected` 状态。
- **inherit**：继承 Linux 网络接口的寻址方式，目前只有 `data_call` 使用该寻址方式。

---

### 3.2.3 NF_INTF 状态

NF_INTF 支持 8 种状态：

| 状态 | 说明 |
|------|------|
| `none` | 对应的 Linux 网络接口不存在 |
| `unconfig` | 对应的 Linux 网络接口存在，但未配置寻址方式 |
| `ready` | 已经配置寻址方式，可以使能寻址 |
| `binder` | NF_INTF 绑定在桥接 NF_INTF 下，作为一个桥接端口 |
| `connecting` | 正在寻址 |
| `connected` | 寻址成功，获取到 IP 地址 |
| `disconnected` | 寻址失败或者断开连接 |

#### NF_INTF 状态变迁图（图 3）描述

```
none ──(Linux network interface detected)──► unconfig
unconfig ──(Addressing mode has been configured)──► ready
unconfig ──(Add NF_INTF to bridge NF_INTF)──► binder
ready ──(Start CMD)──► connecting
connecting ──(Addressing success)──► connected
connecting ──(Addressing failure)──► disconnected
connected ──(Link disconnection/Stop CMD)──► disconnected
disconnected ──(Start CMD or Reconnect)──► connecting
```

> SDK 包中提供了 NF_INTF 状态变化上报的接口函数，参见 SDK 的 sample 程序 `sample/nf`。若用户应用程序需要监控网络状态变化，请参考第 3.10 章。

---

### 3.2.4 创建 NF_INTF

#### 命令格式

```
add interface <interface_name> type=<interface_type> <key_params>
```

#### 参数说明

| 参数 | 说明 |
|------|------|
| `<interface_name>` | NF_INTF 名称，唯一标识 NF_INTF |
| `<interface_type>` | NF_INTF 类型，详见表 4 |
| `<key_params>` | 关键参数。NF_INTF 通过关键参数建立和 Linux 网络接口的映射。具体关键参数描述见表 4 和表 5 |

#### NF_INTF 类型（表 4）

| NF_INTF 类型 | 关键参数 | 描述 |
|-------------|---------|------|
| `data_call` | `call_id` | 数据拨号接口，参考第 3.8 章 |
| `usb_dev` | / | USB 网络接口，自动识别 USB 网络接口名称（ecm0、usbnet0 等） |
| `bridge` | `ifname` | 桥接设备。需要传入网络接口名称（`ifname`），NF_INTF 自动创建桥接的网络接口 |
| `vdev` | `vlan`、`master` | VLAN 接口。传入主 NF_INTF 名称（`master`）和 VLAN ID（`vlan`），在主 NF_INTF 基础上创建一个 VLAN NF_INTF |
| `com_dev` | `ifname` | 通用接口。根据传入的网络接口名称（`ifname`），自动识别网络接口 |
| `local` | / | 本地接口。特殊接口，表示所有本地服务和程序，系统自动创建，用户没有权限操作该接口 |
| `wifi_ap` | `index` | Wi-Fi AP 模式对应接口 |
| `wifi_sta` | / | Wi-Fi STA 模式对应的接口 |

#### NF_INTF 关键参数（表 5）

| 关键参数 | 类型 | 描述 |
|---------|------|------|
| `call_id` | 整数 | 数据拨号的唯一标识。范围：1~INT MAX |
| `ifname` | 字符串 | Linux 网络接口名。范围：1~32 字节 |
| `vlan` | 整数 | VLAN ID。范围：1~4096 |
| `master` | 字符串 | 主 NF_INTF 名称。范围：1~32 字节 |

#### 创建各类 NF_INTF 示例

```bash
# data_call 类型
add interface intf_public type=data_call call_id=1

# usb_dev 类型
add interface intf_usb type=usb_dev

# bridge 类型
add interface intf_lan type=bridge ifname=br0

# vdev 类型
add interface intf_usb_300 type=vdev master=intf_usb vlan=300

# com_dev 类型
add interface intf_eth type=com_dev ifname=eth0
```

---

### 3.2.5 NF_INTF 配置示例

创建 `usb_dev` 类型的 NF_INTF，使用 DHCP 方式寻址：

```bash
# 步骤一：添加 usb_dev 类型的 NF_INTF
add interface intf_usb type=usb_dev

# 步骤二：设置 NF_INTF 寻址方式和自动重连类型
set interface intf_usb proto=dhcp conn=auto

# 步骤三：使能 NF_INTF，开始寻址
start interface intf_usb

# 步骤四：查看 NF_INTF 状态
get interface intf_usb class=info
```

---

## 3.3 VLAN 管理

QL_NF 支持基于 LAN 接口创建 VLAN 接口，**只支持创建单层 VLAN 标签的 VLAN 接口**。在实际运用中，通常使用 USB 网络接口和以太网接口创建对应的 VLAN 接口。可以使用 QL_NF 创建**不多于 64 个** VLAN NF_INTF。

### 示例

```bash
# 首先创建 USB 网络接口以及以太网接口对应的 NF_INTF
add interface intf_usb type=usb_dev
add interface intf_eth type=com_dev ifname=eth0

# 创建 VLAN ID 为 100，基于 USB 网络接口的 VLAN NF_INTF
add interface intf_vdev1 type=vdev master=intf_usb vlan=100

# 创建 VLAN ID 为 200，基于 USB 网络接口的 VLAN NF_INTF
add interface intf_vdev2 type=vdev master=intf_usb vlan=200

# 创建 VLAN ID 为 10，基于以太网接口的 VLAN NF_INTF
add interface intf_vdev3 type=vdev master=intf_eth vlan=10

# 创建 VLAN ID 为 20，基于以太网接口的 VLAN NF_INTF
add interface intf_vdev4 type=vdev master=intf_eth vlan=20
```

### VLAN 优先级（PCP）

QL_NF 支持设置 VLAN 接口发出数据报文中的 VLAN 优先级字段（PCP）。在创建 VLAN NF_INTF 时，使用 `vlan_pri` 参数传入 VLAN 优先级值：

```bash
# 创建 VLAN ID 为 300，输出数据报文的 VLAN 优先级为 4，基于 USB 网络接口的 VLAN NF_INTF
add interface intf_vdev5 type=vdev master=intf_usb vlan=300 vlan_pri=4

# 创建 VLAN ID 为 30，输出数据报文的 VLAN 优先级为 5，基于以太网接口的 VLAN NF_INTF
add interface intf_vdev6 type=vdev master=intf_eth vlan=30 vlan_pri=5
```

---

## 3.4 桥接管理

桥接是一种将采用不同接入方式接入模块的设备接入同一个局域网的方法。在 QL_NF 中，**任意类型的 NF_INTF 都可以加入桥接 NF_INTF**，常用的是把 LAN NF_INTF 加入桥接 NF_INTF。常用的 LAN 接口有：USB 网络接口、以太网接口、Wi-Fi 接口、基于 USB 网络接口以及以太网接口的 VLAN 接口。

桥接 NF_INTF 的使用方法：**创建 LAN NF_INTF → 创建桥接 NF_INTF → 把 LAN NF_INTF 加入桥接 NF_INTF**。

### 示例

把 USB 网络接口、以太网接口和基于以太网接口的 VLAN ID 为 200 的 VLAN 接口，加入一个桥接接口：

```bash
# 步骤一：创建 USB 网络接口、以太网接口以及 VLAN 接口对应的 NF_INTF
add interface intf_usb type=usb_dev
add interface intf_eth type=com_dev ifname=eth0
add interface intf_eth_v200 type=vdev master=intf_eth vlan=200

# 步骤二：创建桥接 NF_INTF
add interface intf_br type=bridge ifname=bridge1

# 步骤三：把相应的 LAN 接口加入桥接接口
set interface intf_br addif=intf_usb
set interface intf_br addif=intf_eth
set interface intf_br addif=intf_eth_v200
```

### 桥接接口扩展配置参数（表 6）

| 参数 | 类型 | 描述 |
|------|------|------|
| `noarp` | 整数 | 打开或禁止桥接接口的 ARP 功能。若禁止 ARP 功能，则使用静态 ARP。`0`：打开（默认值）；`1`：禁止 |
| `isolate` | 整数 | 打开或关闭桥接接口下 LAN 接口之间的数据隔离。`0`：关闭（默认值）；`1`：打开 |

---

## 3.5 Wi-Fi 管理

QL_NF 支持管理使用 QuecOpen Wi-Fi API 创建的 Wi-Fi 接口。QuecOpen Wi-Fi API 支持创建 1~2 个 AP 模式的网络接口以及一个 STA 模式的网络接口，详情请参考**文档 [2]**。

创建 Wi-Fi 相关的 NF_INTF 以及相应的转发规则不需要 Wi-Fi 网络接口已经存在，QL_NF 可以自动识别是否创建和删除了 Wi-Fi 网络接口及其状态变化，并动态更改相应 NF_INTF 的状态。在开发过程中，可以将 Wi-Fi 相关的规则预设置在 `/etc/ql_nf_preload.conf` 文件中，然后在应用程序中，使用 Wi-Fi API 管理 Wi-Fi。

### 示例

```bash
# 创建 Wi-Fi AP0 的 NF_INTF
add interface intf_wifi_ap0 type=wifi_ap index=0

# 创建 Wi-Fi AP1 的 NF_INTF
add interface intf_wifi_ap1 type=wifi_ap index=1

# 创建 Wi-Fi STA 模式的 NF_INTF
add interface intf_wifi_sta type=wifi_sta
```

---

## 3.6 LAN 管理

LAN 管理包括 **DHCP 服务**、**DNS 服务**和 **LAN 设备**的管理。DHCP 服务为 LAN 设备分配 IP 地址。DNS 服务为 LAN 设备提供域名解析服务。

LAN 设备可以通过 ETH、USB 和 Wi-Fi 等方式接入同一个局域网。接入局域网的过程中需要使用桥接方式，把 ETH、USB 和 Wi-Fi 对应的 NF_INTF 加入桥接 NF_INTF 下，然后在桥接 NF_INTF 上管理 DHCP 服务。

`/etc/ql_nf_preload.conf` 文件中的预置规则即是创建 USB、ETH 和桥接类型的 NF_INTF，并把 USB 和 ETH 对应的 NF_INTF 加入到桥接 NF_INTF：

```bash
ql_cmd add interface intf_usb type=usb_dev
ql_cmd add interface intf_lan type=bridge ifname=br0 proto=static conn=auto
ql_cmd set interface intf_lan ip=192.168.225.1 netmask=255.255.255.0
ql_cmd set interface intf_lan addif=intf_usb
```

---

### 3.6.1 DHCP 服务管理

QL_NF 使用 `dhcp_srv` 管理 DHCP 服务，**一个 DHCP 服务和一个 LAN NF_INTF 绑定**，支持同时启动多个 DHCP 服务。

#### 命令格式

```
<action> dhcp_srv <dhcp_srv_name> <param_list>
```

#### 参数解释

| 参数 | 说明 |
|------|------|
| `<action>` | DHCP 服务操作。支持：`add`（添加实例）、`del`（删除实例）、`set`（配置）、`get`（获取状态或配置，返回配置参数见表 7 或状态参数见表 8）、`start`（开启服务）、`stop`（停止服务） |
| `<dhcp_srv_name>` | DHCP 服务名称，最大长度 32 字节，DHCP 服务的唯一标识 |
| `<param_list>` | 参数列表 |

#### 操作示例

```bash
# 添加 DHCP 服务
add dhcp_srv <dhcp_srv_name>

# 配置 DHCP 服务参数（<param_list> 为配置参数列表，见表 7）
set dhcp_srv <dhcp_srv_name> <param_list>

# 开启 DHCP 服务
start dhcp_srv <dhcp_srv_name>

# 停止 DHCP 服务
stop dhcp_srv <dhcp_srv_name>

# 获取 DHCP 服务状态参数（返回状态参数列表，见表 8）
get dhcp_srv <dhcp_srv_name> class=info

# 获取 DHCP 服务配置参数（返回配置参数列表，见表 7）
get dhcp_srv <dhcp_srv_name> class=config
```

#### DHCP 服务配置参数（表 7）

| 参数 | 类型 | 描述 |
|------|------|------|
| `intf` | 字符串 | 绑定的 LAN NF_INTF。长度范围：1~32 字节 |
| `ip_start` | IP 地址格式字符串 | DHCP 地址池起始地址 |
| `ip_end` | IP 地址格式字符串 | DHCP 地址池结束地址 |
| `netmask` | IP 地址格式字符串 | 子网掩码 |
| `gateway` | IP 地址格式字符串 | 网关地址。注意：网关地址必须和绑定的 NF_INTF 的 IP 地址相同 |
| `dnsp` | IP 地址格式字符串 | 首选 DNS，常设置为网关地址 |
| `dnss` | IP 地址格式字符串 | 备选 DNS，常设置为空 |
| `lease_time` | 整数 | DHCP 地址租期。单位：小时，范围：1~INT MAX。**建议设置为 12 小时以上** |

#### DHCP 服务状态参数（表 8）

| 参数 | 类型 | 描述 |
|------|------|------|
| `name` | 字符串 | DHCP 服务名称。长度范围：1~32 字节 |
| `start` | 整数 | 服务开启状态。`0`：未开启；`1`：已开启 |
| `status` | 字符串 | DHCP 服务状态。`none`：未启动；`active`：正在运行；`error`：发生内部错误 |

---

#### 3.6.1.1 DHCP 配置示例

通过 `/etc/ql_nf_preload.conf` 文件中的预置命令创建桥接 NF_INTF，并启动 192.168.225.0 网段的 DHCP 服务：

```bash
add interface intf_usb type=usb_dev
add interface intf_lan type=bridge ifname=br0
set interface intf_lan proto=static conn=auto ip=192.168.225.1 netmask=255.255.255.0
set interface intf_lan addif=intf_usb
add dhcp_srv dhcp_lan intf=intf_lan
set dhcp_srv dhcp_lan ip_start=192.168.225.100 ip_end=192.168.225.200 \
    netmask=255.255.255.0 gateway=192.168.225.1 dnsp=192.168.225.1 lease_time=86400
start dhcp_srv dhcp_lan
```

---

### 3.6.2 DNS 服务管理

QL_NF 使用 `dns_srv` 管理 DNS 服务，**系统只能启动一个 DNS 服务**。

#### 命令格式

```
<action> dns_srv <dns_srv_name>
```

#### 参数说明

| 参数 | 说明 |
|------|------|
| `<action>` | DNS 服务操作。支持：`add`（添加实例）、`del`（删除实例）、`get`（获取状态，返回状态参数见表 9）、`start`（开启服务）、`stop`（停止服务） |
| `<dns_srv_name>` | DNS 服务名称。最大长度 32 字节，DNS 服务的唯一标识 |

#### 操作示例

```bash
# 添加 DNS 服务实例
ql_cmd nf add dns_srv <dns_srv_name>

# 删除 DNS 服务实例
ql_cmd nf del dns_srv <dns_srv_name>

# 开启 DNS 服务
ql_cmd nf start dns_srv <dns_srv_name>

# 停止 DNS 服务
ql_cmd nf stop dns_srv <dns_srv_name>

# 获取 DNS 服务状态（返回 DNS 状态参数，见表 9）
ql_cmd nf get dns_srv <dns_srv_name>
```

#### DNS 服务状态参数（表 9）

| 参数 | 类型 | 描述 |
|------|------|------|
| `name` | 字符串 | DNS 服务名称。长度范围：1~32 字节 |
| `start` | 整数 | 服务是否开启。`0`：未开启；`1`：已开启 |
| `status` | 字符串 | DNS 服务状态。`none`：服务未启动；`active`：服务正在运行；`error`：发生内部错误 |

---

### 3.6.3 LAN 管理示例

LAN 设备通过 USB 和 ETH 接入到模块创建的局域网，局域网网段为 192.168.225.0/24。LAN 设备可以通过 DHCP 方式获取 IP 地址，并能 PING 通网关。对应的 QL_NF 命令示例如下：

```bash
add interface intf_usb type=usb_dev
add interface intf_lan type=bridge ifname=br0
set interface intf_lan proto=static conn=auto ip=192.168.225.1 netmask=255.255.255.0
set interface intf_lan addif=intf_usb
add dhcp_srv dhcp_lan intf=intf_lan
set dhcp_srv dhcp_lan ip_start=192.168.225.100 ip_end=192.168.225.200 \
    netmask=255.255.255.0 gateway=192.168.225.1 dnsp=192.168.225.1 lease_time=86400
start dhcp_srv dhcp_lan
add dns_srv dns_lan
start dns_srv dns_lan
```

---

### 3.6.4 LAN 主机管理

QL_NF 支持查询接入 QL_NF 管理的局域网的主机。

#### 命令格式

```bash
get lanhost all
```

#### LAN 主机信息（表 10）

| 参数 | 类型 | 描述 |
|------|------|------|
| `ip` | IP 地址格式字符串 | LAN 主机的 IP 地址 |
| `mac` | MAC 地址格式字符串 | LAN 主机的 MAC 地址 |
| `name` | 字符串 | LAN 主机名称。根据 DHCP 请求报文中的字段获取；若主机使用静态 IP 地址方式或者 DHCP 请求报文中没有包含名称信息，则该字段为空 |
| `intf` | 字符串 | LAN 主机接入的局域网对应的 NF_INTF |
| `ifname` | 字符串 | LAN 主机接入的 Linux 接口名称 |
| `uptime` | 整数 | 在线时间，单位秒 |

> SDK 中同时提供 API 获取 LAN 主机信息，详情请参考头文件 `ql-sysroots/usr/include/ql-sdk/ql_lanhost.h`。

---

## 3.7 静态 ARP 规则管理

QL_NF 支持创建、删除和查询静态 ARP 规则。

#### 命令格式

```
<action> interface <arp_rule_name> <param_list>
```

#### 参数说明

| 参数 | 说明 |
|------|------|
| `<action>` | ARP 规则操作。支持：`add`（添加静态 ARP 规则）、`del`（删除静态 ARP 规则）、`get`（获取所有静态 ARP 规则） |
| `<arp_rule_name>` | ARP 规则名称。最大长度 32 字节，ARP 规则的唯一标识 |
| `<param_list>` | 参数列表，见表 11 |

#### ARP 规则参数列表（表 11）

| 参数 | 类型 | 描述 |
|------|------|------|
| `intf` | 字符串 | ARP 规则基于的 NF_INTF |
| `ip` | IP 地址格式字符串 | IP 地址 |
| `mac` | MAC 地址格式字符串 | MAC 地址 |

---

## 3.8 数据拨号

### 3.8.1 拨号介绍

无线数据业务的建立过程，即称为**数据拨号（Data Call）**。设备携带 APN 信息向运营商网络侧发起数据拨号，运营商网络侧根据 APN 信息分配 IP 地址，并建立数据链路到相应网络。

**拨号示意图（图 4）描述：**

```
ISP网络1     ISP网络2     ISP网络3
   ↑            ↑            ↑
   |            |            |
┌──┴────────────┴────────────┴──┐
│  APN Info   APN Info   APN Info │
│  APN名称    APN名称    APN名称   │
│  IP Version IP Version IP Ver  │
│  Auth Pref  Auth Pref  Auth P  │
│  用户名     用户名     用户名    │
│  密码       密码       密码      │
│ 数据拨号1  数据拨号2  数据拨号3  │
└──────────────────────────────────┘
```

运营商会在签约时为 SIM 卡分配一个或多个 APN 信息，常见的是一路公网 APN 信息以及一路或者多路私网 APN 信息。

**数据拨号成功的前提**是模块已注册到运营商网络。LTE 网络注册过程中，需要建立**默认承载**。和数据拨号类似，建立默认承载也需要传入 APN 信息，若传入的 APN 信息错误，可能导致网络注册失败。运营商分配的 APN 信息中，有一路 APN 信息可以用来建立默认承载，详情请咨询运营商。

---

### 3.8.2 重连机制

拨号失败或者拨号断开时，可选择是否进入**自动重连模式**。模块支持以下四种自动重连模式：

#### disable
不自动重连，用户可以自己实现自动重连机制。

#### normal
根据**固定时间间隔**进行重连，建议设置时间间隔 20 秒以上。频繁拨号可能会被运营商加入黑名单。

#### mode1
以**时间退避模式**进行重连。例如时间间隔列表为 T1、T2、T3...Tn，重连的时间间隔为 T1、T2、T3....Tn、Tn、Tn...Tn 直至拨号成功。若需再次重连，则间隔时间从 T1 开始。

#### mode2
**循环时间退避模式**进行重连。例如时间间隔列表为 T1、T2、T3...Tn，重连的时间间隔为 T1、T2、T3...Tn、T1、T2、T3...Tn...直至拨号成功。若需再次重连，则间隔时间从 T1 开始。

---

### 3.8.3 设置默认承载 APN

#### 命令格式

```
set networks attach_apn <param_list>
```

#### APN 参数列表（表 12）

| 参数 | 类型 | 描述 |
|------|------|------|
| `apn_name` | 字符串 | APN 名称。运营商根据此参数分配 IP 地址信息。长度范围：1~150 字节 |
| `ip_ver` | 整数 | IP 类型。`4`：IPv4；`6`：IPv6；`46`：IPv4v6 |
| `auth_pref` | 字符串 | 认证方式。部分运营商网络会开启认证，需要传入认证方式，如无特殊指明，可不设置该参数或者设置为 `none`。取值：`none`（无认证，默认值）、`pap`（PAP 认证）、`chap`（CHAP 认证）、`pap_chap`（PAP 和 CHAP 认证） |
| `username` | 字符串 | 用户名。长度范围：1~150 字节 |
| `password` | 字符串 | 密码。长度范围：1~150 字节 |
| `sim_id` | 整数 | 若模块支持 DSDA，则使用该参数指定（U）SIM 卡槽（`1`：卡槽 1；`2`：卡槽 2）；若模块不支持 DSDA，无需设置该参数 |

---

### 3.8.4 数据拨号状态

支持的数据拨号状态如下：

| 状态 | 描述 |
|------|------|
| `none` | 数据拨号实例不存在 |
| `created` | 数据拨号实例被创建 |
| `idle` | 数据拨号实例已经被配置 |
| `connecting` | 正在进行拨号 |
| `connected` | 数据拨号成功，获取到 IP 地址等信息 |
| `disconnected` | 数据拨号失败或者断开 |
| `deleted` | 数据拨号实例被删除 |

#### 数据拨号状态变迁图（图 5）描述

```
none ──(CREATE REQ)──► created ──(CONFIG REQ)──► idle ──(START REQ)──► connecting
                                                                              │
                                              Data call succeed ◄────────────┘
                                              Data call failed ──► disconnected
                                              Network disconnect ──► disconnected
idle ◄──(STOP REQ)──── connecting
disconnected ──(Reconnect enable)──► connecting
connected ──(STOP REQ)──► idle
connected ──(DELETE REQ)──► disconnected ──► deleted
```

---

### 3.8.5 数据拨号命令

#### 命令格式

```
<action> data_call <call_id> <param_list>
```

#### 参数说明

| 参数 | 说明 |
|------|------|
| `<action>` | 数据拨号实例操作。支持：`add`（添加一路数据拨号实例）、`del`（删除数据拨号实例）、`set`（配置数据拨号参数）、`get`（获取数据拨号状态参数或者配置参数）、`start`（开始数据拨号）、`stop`（停止数据拨号） |
| `<call_id>` | 数据拨号的唯一标识。整型，取值范围为 1~INT MAX |
| `<param_list>` | 配置参数列表 |

#### 操作示例

```bash
# 创建数据拨号实例
ql_cmd nf add data_call <call_id>

# 删除数据拨号实例
ql_cmd nf del data_call <call_id>

# 设置数据拨号配置参数
ql_cmd nf set data_call <call_id> <param_list>

# 开始数据拨号
ql_cmd nf start data_call <call_id>

# 停止数据拨号
ql_cmd nf stop data_call <call_id>

# 获取数据拨号状态（返回状态参数列表见表 14；若 call_id 为 all，表示获取所有数据拨号状态）
ql_cmd nf get data_call <call_id> class=info

# 获取拨号配置参数（返回配置参数列表见表 13；若 call_id 为 all，表示获取所有数据拨号配置参数）
ql_cmd nf get data_call <call_id> class=config
```

#### 数据拨号配置参数（表 13）

| 参数 | 类型 | 描述 |
|------|------|------|
| `call_id` | 整数 | 用户指定，唯一标识数据拨号实例。范围：1~INT MAX |
| `apn_name` | 字符串 | APN 名称。运营商根据此参数分配 IP 地址信息，长度范围：1~50 字节 |
| `ip_ver` | 整数 | IP 类型。`4`：IPv4；`6`：IPv6；`46`：IPv4v6 |
| `auth_pref` | 字符串 | 认证方式。`none`（无认证，默认值）、`pap`（PAP 认证）、`chap`（CHAP 认证）、`pap_chap`（PAP 和 CHAP 认证） |
| `username` | 字符串 | 用户名。长度范围：1~150 字节 |
| `password` | 字符串 | 密码。长度范围：1~150 字节 |
| `reconn_mode` | 字符串 | 自动重连模式，见第 3.8.2 章 |
| `reconn_interval` | 整数数组 | 自动重连时间间隔列表。单位：秒，数组最大个数为 20 |
| `sim_id` | 整数 | 若模块支持 DSDA，则使用该参数指定（U）SIM 卡槽（`1`：卡槽 1；`2`：卡槽 2）；若模块不支持 DSDA，无需设置该参数 |

#### 数据拨号状态参数（表 14）

| 参数 | 类型 | 描述 |
|------|------|------|
| `status` | 字符串 | 拨号状态，见第 3.8.4 章 |
| `ipaddr` | IP 地址格式字符串 | IP 地址 |
| `netmask` | IP 地址格式字符串 | 子网掩码 |
| `gateway` | IP 地址格式字符串 | 网关 |
| `dnsp` | IP 地址格式字符串 | 首选 DNS |
| `dnss` | IP 地址格式字符串 | 备选 DNS |
| `last_ecode`（暂不支持） | 整数 | 上次拨号失败或者断开的内部错误码。错误码详见头文件 `ql-sysroots/usr/include/ril.h` 中的 `RIL_LastCallFailCause` 结构体 |

---

### 3.8.6 拨号示例

若用户的（U）SIM 卡和运营商签约了两路 APN，公网 APN 名称为 `apnpublic`，私网 APN 名称为 `apnprivate`，其中公网 APN 可以建立默认承载：

```bash
ql_cmd nf set networks attach_apn apn_name=apnpublic
ql_cmd nf add data_call 1
ql_cmd nf set data_call 1 apn_name=apnpublic
ql_cmd nf start data_call 1
ql_cmd nf add data_call 2
ql_cmd nf set data_call 2 apn_name=apnprivate
ql_cmd nf start data_call 2
```

---

### 3.8.7 注意事项

用户可通过 API 函数管理数据拨号、使用 QL_NF 管理转发规则。有关 API 函数的详情请参考**文档 [3]**。

数据拨号 API 函数使用 `call_id` 唯一标识数据拨号实例。在 QL_NF 中创建数据拨号 NF_INTF，也是使用 `call_id` 关联数据拨号的实例。**创建 NF_INTF 时，不需要数据拨号实例存在；创建数据拨号实例后，会自动同步数据拨号 NF_INTF 的状态**。

在开发过程中，可以将数据拨号的 NF_INTF 以及相关的转发规则预设置在 `/etc/ql_nf_preload.conf` 配置文件中，然后在应用程序中使用数据拨号的 API 函数管理数据拨号。

---

## 3.9 转发策略

### 3.9.1 转发策略介绍

转发策略决定 **LAN 接口和 WAN 接口之间的选路**、**本地程序和 WAN 接口之间的选路**。QL_NF 使用 **ACL** 管理转发策略。

ACL 表示**访问控制列表（Access Control Lists）**，用于设置 LAN 接口和 WAN 接口之间的选路、本地程序和 WAN 接口之间的选路。用户只需指定数据源（LAN NF_INTF 或者本地程序）、数据类型以及数据目的（WAN NF_INTF），即可完成规则的配置，无需对 Linux 网络规则进行配置。

为了管理本地程序的网络访问，QL_NF 使用 `intf_local` NF_INTF 表示所有模块上运行的本地程序。

#### ACL 工作示意图（图 6）描述

```
服务器1(URL:testserve.com)      服务器1(IP:2.2.2.2 PORT:9090)
         ↑                              ↑
      ISP NET1                       ISP NET2
         ↑                              ↑
   Data Call 1 (intf_net1)    Data Call 2 (intf_net2)
         ↑                              ↑
┌────────────────────────────────────────────────────────┐
│  ACL1                  ACL2          ACL3          ACL4  │
│  sintf=intf_eth        sintf=intf_eth  sintf=intf_usb  sintf=intf_usb  │
│  url=testsever.com     dintf=intf_net2  dintf=intf_net2  dip=2.2.2.2   │
│  dintf=intf_net1                                  dport=9090           │
│                                                   dintf=intf_net1      │
│          intf_eth (LAN NF_INTF)    intf_usb (LAN NF_INTF)              │
│          eth0                      ecm0                                 │
└────────────────────────────────────────────────────────┘
LAN设备1 (APP1连接testserver.com / APP2连接其他服务器)
LAN设备2 (APP1连接其他服务器 / APP2连接服务器:2.2.2.2:9090)
```

---

### 3.9.2 ACL 命令

#### 命令格式

```
<action> acl <acl_name> sintf=<source_interface_name> <data_flow_type_params> dintf=<destination_interface_name>
```

#### 参数说明

| 参数 | 说明 |
|------|------|
| `<action>` | ACL 规则操作。支持：`add`（添加 ACL 规则）、`del`（删除 ACL 规则）、`get`（获取 ACL 配置） |
| `<acl_name>` | ACL 规则名称。ACL 规则的唯一标识，字符串类型，1~32 字节长度 |
| `<source_interface_name>` | LAN NF_INTF 名称。可设置为 `intf_local`，表示所有本地服务和本地程序 |
| `<data_flow_type_params>` | 数据流标识参数列表，见表 15 |
| `<destination_interface_name>` | WAN NF_INTF 名称，数据转发的出口 |

#### 操作示例

```bash
# 添加 ACL 规则
add acl <acl_name> sintf=<source_interface_name> <data_flow_type_params> dintf=<destination_interface_name>

# 删除 ACL 规则
del acl <acl_name>

# 查看所有 ACL 规则
get acl all

# 删除所有 ACL 规则
del acl all
```

#### 数据流标识参数（表 15）

| 参数 | 类型 | 描述 |
|------|------|------|
| `proto` | 字符串 | 数据流类型。本地程序不支持该参数。`all`：所有类型，包括 TCP 和 UDP；`tcp`：TCP 流；`udp`：UDP 流 |
| `url` | 字符串 | URL 绑定。绑定 URL 到对应的 WAN 接口，长度范围：1~128 字节。**此参数不能同 `<proto>`、`<sport>`、`<dip>` 或 `<dport>` 共用** |
| `smac` | MAC 地址格式字符串 | 数据流源 MAC 地址（即 LAN 设备 MAC 地址），本地程序不支持该参数 |
| `sip` | IP 地址格式字符串 | 数据流源 IP 地址（即 LAN 设备 IP 地址），本地程序不支持该参数 |
| `sport` | 整数 | 数据流源端口。本地程序不支持该参数。范围：1~65535 |
| `dip` | IP 地址格式字符串 | 数据流目的 IP 地址 |
| `dport` | 整数 | 数据流目的端口。本地程序不支持该参数。范围：1~65535 |

---

## 3.10 QL_NF API

### 3.10.1 头文件

QL_NF API 头文件为 `ql_nf.h`，部分公用结构体定义在头文件 `ql_net_common.h`，位于 SDK 包的 `ql-sysroots/usr/include/ql-sdk` 目录下。若无特别说明，本章所涉及头文件均在该目录下。

---

### 3.10.2 函数概览（表 16）

| 函数 | 说明 |
|------|------|
| `ql_nf_init()` | 初始化 QL_NF 服务 |
| `ql_nf_cmd()` | 发送普通格式的 QL_NF 命令，并等待响应 |
| `ql_nf_set_interface_status_ind_cb()` | 设置监控 NF_INTF 状态变化回调函数 |
| `ql_nf_set_service_error_cb()` | 设置服务程序异常通知的回调函数 |
| `ql_nf_deinit()` | 注销 QL_NF 服务 |

> **备注**：若无特别说明，所有 QL_NF API 均不支持并发调用。

---

### 3.10.3 函数详解

#### 3.10.3.1 `ql_nf_init`

初始化 QL_NF 服务。

**函数原型**

```c
int ql_nf_init(void);
```

**参数**：无

**返回值**

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 无效参数 |
| `QL_ERR_UNKNOWN` | 未知错误 |
| `QL_ERR_SERVICE_NOT_READY` | QL_NF 服务准备就绪，返回此错误码时，建议重试 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

> **备注**：使用其他 QL_NF API 前，**必须调用本函数初始化 QL_NF 服务**。

---

#### 3.10.3.2 `ql_nf_cmd`

发送普通格式的 QL_NF 命令，并等待响应。

**函数原型**

```c
int ql_nf_cmd(const char *cmd, char *resp_buf, int resp_len);
```

**参数**

| 参数 | 方向 | 描述 |
|------|------|------|
| `cmd` | [In] | QL_NF 普通格式命令，见第 3.1 章 |
| `resp_buf` | [Out] | 存放响应的数据缓冲区，QL_NF 命令的响应为字符串格式 |
| `resp_len` | [In] | `resp_buf` 缓冲区大小，单位：字节。用户根据自己的命令需求设置响应缓冲区大小。例如发送获取单个 NF_INTF 信息的命令，缓冲区可设置为 256 字节大小；若获取所有 NF_INTF 信息的命令，建议缓冲区大小为 2048 字节以上 |

**返回值**

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_INVALID_ARG` | 无效参数 |
| `QL_ERR_UNKNOWN` | 未知错误 |
| `QL_ERR_SERVICE_NOT_READY` | QL_NF 服务准备就绪，返回此错误码时，建议重试 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

#### 3.10.3.3 `ql_nf_set_interface_status_ind_cb`

设置监控 NF_INTF 状态变化的回调函数。

**函数原型**

```c
int ql_nf_set_interface_status_ind_cb(ql_nf_interface_status_ind_cb_f cb);
```

**参数**

| 参数 | 方向 | 描述 |
|------|------|------|
| `cb` | [In] | 监控 NF_INTF 状态变化的回调函数，见第 3.10.3.3.1 章 |

**返回值**

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| `QL_ERR_NOT_INIT` | 未初始化 |
| `QL_ERR_SERVICE_NOT_READY` | QL_NF 服务准备就绪，返回此错误码时，建议重试 |
| `QL_ERR_INVALID_ARG` | 无效参数 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 3.10.3.3.1 `ql_nf_interface_status_ind_cb_f`

该回调函数数监控 NF_INTF 状态变化。

**函数原型**

```c
typedef void (*ql_nf_interface_status_ind_cb_f)(ql_nf_interface_status_t *p_msg);
```

**参数**

| 参数 | 方向 | 描述 |
|------|------|------|
| `p_msg` | [Out] | NF_INTF 状态结构体，见第 3.10.3.3.2 章 |

---

##### 3.10.3.3.2 `ql_nf_interface_status_t`

NF_INTF 状态结构体定义如下：

```c
typedef struct {
    char name[QL_NET_MAX_NAME_LEN];
    char ifname[QL_NET_MAX_NAME_LEN];
    char pre_status[16];
    char status[16];
    uint8_t has_addr;
    ql_net_addr_t addr;
    uint8_t has_addr6;
    ql_net_addr6_t addr6;
} ql_nf_interface_status_t;
```

**成员说明**

| 类型 | 参数 | 描述 |
|------|------|------|
| `char` | `name` | NF 中的接口名称 |
| `char` | `ifname` | 网口名称 |
| `char` | `pre_status` | 接口先前状态，见表 17 |
| `char` | `status` | 接口当前状态，见表 17 |
| `uint8_t` | `has_addr` | 有无 IPv4 地址。`0`：无 IPv4 地址；非 `0`：有 IPv4 地址 |
| `ql_net_addr_t` | `addr` | IPv4 地址，见第 3.10.3.3.3 章 |
| `uint8_t` | `has_addr6` | 有无 IPv6 地址。`0`：无 IPv4 地址；非 `0`：有 IPv4 地址 |
| `ql_net_addr6_t` | `addr6` | IPv6 地址，见第 3.10.3.3.4 章 |

---

#### 接口状态枚举（表 17）

| 接口状态 | 描述 |
|---------|------|
| `NF_INTF_STATUS_NONE` | 初始状态或空闲状态 |
| `NF_INTF_STATUS_UNCONFIG` | 未配置 |
| `NF_INTF_STATUS_READY` | 就绪状态 |
| `NF_INTF_STATUS_CONNECTING` | 连接中 |
| `NF_INTF_STATUS_CONNECTED` | 已连接 |
| `NF_INTF_STATUS_DISCONNECTED` | 断开连接 |
| `NF_INTF_STATUS_ERROR` | 出错 |
| `NF_INTF_STATUS_BINDER` | 绑定到其他接口 |

---

##### 3.10.3.3.3 `ql_net_addr_t`

IPv4 地址结构体定义如下：

```c
typedef struct {
    char addr[QL_NET_MAX_ADDR_LEN];
    char netmask[QL_NET_MAX_ADDR_LEN];
    uint8_t subnet_bits;
    char gateway[QL_NET_MAX_ADDR_LEN];
    char dnsp[QL_NET_MAX_ADDR_LEN];
    char dnss[QL_NET_MAX_ADDR_LEN];
} ql_net_addr_t;
```

**成员说明**

| 类型 | 参数 | 描述 |
|------|------|------|
| `char` | `addr` | IPv4 地址，类型：字符串 |
| `char` | `netmask` | 子网掩码 |
| `uint8_t` | `subnet_bits` | 网络号位数 |
| `char` | `gateway` | 网关 |
| `char` | `dnsp` | 首 DNS |
| `char` | `dnss` | 副 DNS |

---

##### 3.10.3.3.4 `ql_net_addr6_t`

IPv6 地址结构体定义如下：

```c
typedef struct {
    char addr[QL_NET_MAX_ADDR_LEN];
    char prefix[QL_NET_MAX_ADDR_LEN];
    int32_t prefix_bits;
    char gateway[QL_NET_MAX_ADDR_LEN];
    char dnsp[QL_NET_MAX_ADDR_LEN];
    char dnss[QL_NET_MAX_ADDR_LEN];
} ql_net_addr6_t;
```

**成员说明**

| 类型 | 参数 | 描述 |
|------|------|------|
| `char` | `addr` | IPv6 地址，类型：字符串 |
| `char` | `prefix` | IPv6 前缀 |
| `uint8_t` | `prefix_bits` | 前缀位数 |
| `char` | `gateway` | 网关 |
| `char` | `dnsp` | 首 DNS |
| `char` | `dnss` | 副 DNS |

---

#### 3.10.3.4 `ql_nf_set_service_error_cb`

设置通知服务程序异常的回调函数。

**函数原型**

```c
int ql_nf_set_service_error_cb(ql_nf_service_error_cb_f cb);
```

**参数**

| 参数 | 方向 | 描述 |
|------|------|------|
| `cb` | [In] | 服务程序异常通知回调函数，见第 3.10.3.4.1 章 |

**返回值**

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

##### 3.10.3.4.1 `ql_nf_service_error_cb_f`

该回调函数数通知服务程序异常。

**函数原型**

```c
typedef void (*ql_nf_service_error_cb_f)(int error);
```

**参数**

| 参数 | 方向 | 描述 |
|------|------|------|
| `error` | [Out] | 错误原因，类型：整数，用户自定义 |

---

#### 3.10.3.5 `ql_nf_deinit`

注销 QL_NF 服务。

**函数原型**

```c
int ql_nf_deinit(void);
```

**参数**：无

**返回值**

| 返回值 | 描述 |
|--------|------|
| `QL_ERR_OK` | 函数执行成功 |
| 其他值 | 函数执行失败，错误码详见 `ql_type.h` |

---

## 3.11 注意事项

### 3.11.1 MAC 地址分配

每个 Linux 网络接口都有一个 MAC 地址，MAC 地址长度为 48 位（6 个字节），通常表示为 12 个 16 进制数，每 2 个 16 进制数之间用冒号隔开，如 `08:22:22:22:22:22`。

MAC 地址最高字节的第二个比特位表示该地址为**全局地址**还是**本地地址**：
- 为 `1`，表示是本地 MAC 地址
- 为 `0`，表示是全局 MAC 地址

全局地址是全球唯一地址，保证设备的 MAC 地址不会产生冲突，**WAN 接口的 MAC 地址应该为全局地址**。由于本地 MAC 地址不能保证 MAC 地址的唯一性，所以只用于 LAN 接口。局域网环境下，都是 LAN 设备的 WAN 接口以及接入网关设备的 LAN 接口，可保证局域网 MAC 地址不会发生冲突。

模块的 **Wi-Fi 网络接口，烧录了移远通信 OUI 的全局 MAC 地址**。其他的网络接口，包括 ETH 网络接口和 USB 网络接口，均使用 Linux 随机生成的本地 MAC 地址。作为 LAN 接口时，不会出现 MAC 地址冲突；但作为 WAN 接口时无法避免 MAC 地址冲突（随机 MAC 地址碰撞的概率极小）。

在无法确定是否会产生 MAC 地址冲突的场景下，推荐用户在 ETH 和 USB 网络接口作为 WAN 接口时，使用用户自行向 IEEE 购买的 MAC 地址，并配置 `QL_INTF` 的 `MAC` 参数。

---

### 3.11.2 禁用 QL_NF

由于 QL_NF 框架无法满足某些特殊需求，当用户有特殊需求时可禁用 QL_NF。

SDK 中提供了较为底层的接口（如拨号接口，详情见**文档 [3]**），结合 Linux 的网络管理，并通过清空 `/etc/ql_nf_preload.conf` 文件禁用 QL_NF，开发所需要的网络管理程序。

---

## 4 场景案例

本章介绍如何在各个典型应用场景下配置 QL_NF 规则，示例中的域名、IP 地址或者 MAC 地址等信息并非真实存在，只为举例说明，用户可以根据实际信息进行设置。

### 推荐的 QL_NF 配置规则步骤

1. **步骤一**：若使用数据拨号，请设置默认承载的 APN 信息（见第 3.8.3 章），用户也可以在应用程序中使用数据拨号 API 设置（见文档 [3]）。
2. **步骤二**：配置 LAN 相关的 NF_INTF（见第 3.6 章）。
3. **步骤三**：启动 DHCP 和 DNS 服务（见第 3.6.1 章和第 3.6.2 章）。
4. **步骤四**：配置 WAN 相关的 NF_INTF（见第 3.2 章）。如果 WAN 接口是数据拨号的接口，用户也可以只创建数据拨号的 NF_INTF，由应用程序调用数据拨号的 API 去管理数据拨号。
5. **步骤五**：配置 ACL 规则（见第 3.9 章）。

> 在开发过程中，可能需要前期的验证工作，在场景三（第 4.3 章）中，举例说明了如何使用装有 Ubuntu 系统的 PC 进行模拟测试。

完成调试后，再将规则固化到 `/etc/ql_nf_preload.conf` 文件。各个场景的数据转发测试，可以在模块上使用 `tcpdump` 工具，通过抓取各个网络接口收到的数据包，验证分析转发规则是否生效。

---

## 4.1 场景一

### 4.1.1 场景需求

#### 配置环境

1. （U）SIM 卡：（U）SIM 卡签约一路 APN，名称为 `apn_name`，该路 APN 作为默认承载。
2. WAN 接口：使用签约的 APN 进行数据拨号。
3. LAN 接口：Wi-Fi AP 模式对应的接口、ETH 对应的接口和 USB ECM 对应的接口。
4. 局域网：使用 Wi-Fi、ECM 和 ETH 接入的设备，位于一个局域网，网段为 192.168.225.0/24，网关地址为 192.168.225.1。

#### 转发策略

1. 模块本地程序使用数据拨号访问网络。
2. LAN 设备使用数据拨号访问网络。

#### Linux 网络拓扑（图 7）

```
                APN Net (互联网)
                      │
               APN 数据拨号
                      │
               ┌──────┴──────┐
               │   应用程序   │
               │   桥接0      │
               └──┬────┬───┬─┘
               wlan0  eth0  ecm0
               │      │      │
             手机     PC    PC
```

### 4.1.2 QL_NF 组网

| NF_INTF | 类型 | Linux 接口名 | 描述 | 地址 | WAN/LAN | 开启 DHCP |
|---------|------|------------|------|------|---------|----------|
| `intf_lan` | `bridge` | `bridge0` | 管理局域网 192.168.225.1/24 | 192.168.225.1/24 | LAN | 是 |
| `intf_eth` | `com_dev` | `eth0` | 以太网接口 | / | LAN | / |
| `intf_wifi_ap0` | `wifi_ap` | `wlan0` | Wi-Fi AP 模式对应的 NF_INTF | / | LAN | / |
| `intf_usb` | `usb_dev` | `ecm0` | USB ECM 接口对应的 NF_INTF | / | LAN | / |
| `intf_data` | `data_call` | 动态（根据拨号结果返回） | 基于 APN 的数据拨号的 NF_INTF | 拨号成功后的信息 | WAN | / |

#### 转发规则

| ACL 名称 | 数据流来源 NF_INTF | 数据流匹配规则 | 数据流目的 NF_INTF | 描述 |
|---------|----------------|-------------|-----------------|------|
| `acl_app_apn` | `intf_local` | 所有数据流 | `intf_data` | 运行在模块上的应用程序，使用基于 APN 的数据拨号访问网络 |
| `acl_lan_apn` | `intf_lan` | 所有数据流 | `intf_data` | 接入局域网的 LAN 设备，通过基于 APN 的数据拨号访问网络 |

### 4.1.3 规则配置

```bash
# 步骤一：设置默认承载 APN 信息为 SIM 卡签约 APN 信息
set networks attach_apn apn_name=apn_name ip_ver=4

# 步骤二：创建 LAN NF_INTF，创建桥接类型的 NF_INTF 用于管理局域网，并把 LAN NF_INTF 加入桥接 NF_INTF
add interface intf_usb type=usb_dev
add interface intf_eth type=com_dev ifname=eth0
add interface intf_wifi_ap0 type=wifi_ap index=0
add interface intf_lan type=bridge ifname=br0
set interface intf_lan proto=static conn=auto ip=192.168.225.1 netmask=255.255.255.0
set interface intf_lan addif=intf_usb
set interface intf_lan addif=intf_eth
set interface intf_lan addif=intf_wifi_ap0

# 步骤三：启动 DNS 代理服务和局域网的 DHCP 服务（DHCP 服务需要绑定对应的桥接 NF_INTF）
add dhcp_srv dhcp_lan intf=intf_lan
set dhcp_srv dhcp_lan ip_start=192.168.225.100 ip_end=192.168.225.200 \
    netmask=255.255.255.0 gateway=192.168.225.1 dnsp=192.168.225.1 lease_time=86400
start dhcp_srv dhcp_lan
add dns_srv dns_lan
start dns_srv dns_lan

# 步骤四：创建 WAN NF_INTF
add data_call 1
set data_call 1 apn_name=apn_name ip_ver=4 reconn_mode=mode1 reconn_interval=10,20,40,80,120
start data_call 1
add interface intf_data type=data_call call_id=1

# 步骤五：配置转发规则
add acl acl_app_apn sintf=intf_local dintf=intf_data
add acl acl_lan_apn sintf=intf_lan dintf=intf_data

# 步骤六：Wi-Fi 相关的配置，详见文档 [2]
```

### 4.1.4 场景测试

将上述章节的所有命令拷贝到 `/etc/ql_nf_preload.conf` 文件，重启设备后等待模块网络注册成功，运行 `ql_cmd nf get interface all` 命令，桥接 NF_INTF 应为 `connected` 状态。等待 WAN NF_INTF 状态变为 `connected` 状态，验证以下内容：

1. 模块上的应用程序，可以访问网络。
2. LAN 设备可以获取 IP 地址并访问网络。

---

## 4.2 场景二

### 4.2.1 场景需求

#### 配置环境

1. SIM 卡：签约两路 APN 信息，APN1 名称为 `apn_name1`，APN2 名称为 `apn_name2`，其中 APN1 为作为默认承载 APN。
2. WAN 接口：基于 APN1 和 APN2 的数据拨号接口。
3. LAN 接口：Wi-Fi AP 模式对应的接口、ETH 对应的接口和 USB ECM 对应的接口。
4. 局域网：使用 Wi-Fi、USB ECM 和 ETH 接入的设备位于一个局域网内，网段为 192.168.225.0/24，网关地址为 192.168.225.1。

#### 转发策略

1. 模块本地程序 APP1 通过 APN2 的数据拨号以域名 `www.test.com` 方式访问网络服务。
2. 模块本地程序 APP2 通过 APN2 的数据拨号以 IP 地址 `2.2.2.2` 方式访问网络服务。
3. 模块其他本地程序通过 APN1 的数据拨号访问网络。
4. LAN 设备通过 APN1 的数据拨号访问网络。

### 4.2.2 QL_NF 组网

| NF_INTF | 类型 | Linux 接口名 | 描述 | 地址 | WAN/LAN | 开启 DHCP |
|---------|------|------------|------|------|---------|----------|
| `intf_lan` | `bridge` | `bridge0` | 管理局域网 192.168.255.0/24 | 192.168.225.1/24 | LAN | 是 |
| `intf_eth` | `com_dev` | `eth0` | 以太网接口的 NF_INTF | / | LAN | / |
| `intf_wifi_ap0` | `wifi_ap` | `wlan0` | Wi-Fi AP 模式对应的 NF_INTF | / | LAN | / |
| `intf_usb` | `usb_dev` | `ecm0` | USB ECM 对应的 NF_INTF | / | LAN | / |
| `intf_data_apn1` | `data_call` | 动态 | APN1 的数据拨号的 NF_INTF | 拨号成功后的信息 | WAN | / |
| `intf_data_apn2` | `data_call` | 动态 | APN2 的数据拨号的 NF_INTF | 拨号成功后的信息 | WAN | / |

#### 转发规则

| ACL 名称 | 数据流来源 NF_INTF | 数据流匹配规则 | 数据流目的 NF_INTF | 描述 |
|---------|----------------|-------------|-----------------|------|
| `acl_app_server1` | `intf_local` | 域名为 `www.test.com` | `intf_data_apn2` | 运行在模块上的应用程序，通过 APN2 的数据拨号以域名访问方式访问网络服务 |
| `acl_app_server2` | `intf_local` | 目的 IP 地址为 `2.2.2.2` | `intf_data_apn2` | 运行在模块上的应用程序，通过 APN2 的数据拨号以 IP 地址方式访问网络服务 |
| `acl_app_apn1` | `intf_local` | 所有数据流 | `intf_data_apn1` | 运行在模块上的应用程序，通过 APN1 的数据拨号以 IP 地址 2.2.2.2 方式访问网络服务 |
| `acl_lan_apn1` | `intf_lan` | 所有数据流 | `intf_data_apn1` | LAN 设备通过 APN1 的数据拨号访问网络 |

### 4.2.3 规则配置

```bash
# 步骤一：设置默认承载 APN 信息为 SIM 卡签约 APN1 信息
set networks attach_apn apn_name=testapn1 ip_ver=4

# 步骤二：创建 LAN NF_INTF
add interface intf_usb type=usb_dev
add interface intf_eth type=com_dev ifname=eth0
add interface intf_wifi_ap0 type=wifi_ap index=0
add interface intf_lan type=bridge ifname=br0
set interface intf_lan proto=static conn=auto ip=192.168.225.1 netmask=255.255.255.0
set interface intf_lan addif=intf_usb
set interface intf_lan addif=intf_eth
set interface intf_lan addif=intf_wifi_ap0

# 步骤三：启动 DNS 代理服务和局域网的 DHCP 服务
add dhcp_srv dhcp_lan intf=intf_lan
set dhcp_srv dhcp_lan ip_start=192.168.225.100 ip_end=192.168.225.200 \
    netmask=255.255.255.0 gateway=192.168.225.1 dnsp=192.168.225.1 lease_time=86400
start dhcp_srv dhcp_lan
add dns_srv dns_lan
start dns_srv dns_lan

# 步骤四：创建 WAN NF_INTF
add data_call 1
set data_call 1 apn_name=testapn1 ip_ver=4 reconn_mode=mode1 reconn_interval=10,20,40,80,120
start data_call 1
add data_call 2
set data_call 2 apn_name=testapn2 ip_ver=4 reconn_mode=mode1 reconn_interval=10,20,40,80,120
start data_call 2
add interface intf_data_apn1 type=data_call call_id=1
add interface intf_data_apn2 type=data_call call_id=2

# 步骤五：配置转发规则
add acl acl_app_server1 sintf=intf_local url=test.com dintf=intf_data_apn2
add acl acl_app_server2 sintf=intf_local dip=2.2.2.2 dintf=intf_data_apn2
add acl acl_app_apn1 sintf=intf_local dintf=intf_data_apn1
add acl acl_lan_apn1 sintf=intf_lan dintf=intf_data_apn1
```

### 4.2.4 场景测试

将上述命令拷贝到 `/etc/ql_nf_preload.conf` 文件，重启设备后等待模块网络注册成功。运行 `ql_cmd nf get interface all` 命令，桥接 NF 接口应为 `connected` 状态。等待 WAN NF 接口状态变为 `connected` 状态，可以做以下验证：

1. 在模块上，访问 `test.com`，通过 `intf_data_apn2` 收发数据。
2. 在模块上，访问 `2.2.2.2`，通过 `intf_data_apn2` 收发数据。
3. 在模块上，通过 `intf_data_apn1` 访问网络。
4. LAN 设备，通过 `intf_data_apn1` 访问网络。

---

## 4.3 场景三

### 4.3.1 场景需求

#### 配置环境

1. （U）SIM 卡：签约两路 APN 信息，APN1 名称为 `testapn1`；APN2 名称为 `testapn2`，其中 APN1 为作为默认承载 APN。
2. WAN 接口：基于 APN1 和 APN2 的数据拨号接口。
3. LAN 接口：Wi-Fi AP 对应的接口，基于 ETH 且 VLAN ID 分别为 100 和 200 的接口。
4. 局域网：
   - 使用 ETH VID 100 接入的设备位于一个局域网，网段为 192.168.100.0/24。
   - 使用 ETH VID 200 接入的设备位于一个局域网，网段为 192.168.200.0/24。

#### 转发策略

1. 模块本地程序使用 APN2 的数据拨号访问域名为 `www.test.com` 的服务器。
2. 模块本地程序使用 APN1 的数据拨号访问其他网络服务。
3. 通过 ETH VID 100 接入的设备使用基于 APN1 的数据拨号访问网络。
4. 通过 ETH VID 200 接入的设备使用基于 APN2 的数据拨号访问网络。
5. 通过 Wi-Fi 接入的设备，通过基于 APN1 的数据拨号访问网络。

### 4.3.2 QL_NF 组网

| NF_INTF | 类型 | Linux 接口名 | 描述 | 地址 | WAN/LAN | 开启 DHCP |
|---------|------|------------|------|------|---------|----------|
| `intf_lan100` | `bridge` | `bridge100` | 管理局域网 192.168.100.0/24 | 192.168.100.1/24 | LAN | 是 |
| `intf_lan200` | `bridge` | `bridge200` | 管理局域网 192.168.200.0/24 | 192.168.200.1/24 | LAN | 是 |
| `intf_eth` | `com_dev` | `eth0` | 以太网接口的 NF_INTF | / | LAN | / |
| `intf_100` | `vdev` | `eth0.100` | 基于以太网接口的 VLAN ID 为 100 的 NF_INTF | / | LAN | / |
| `intf_200` | `vdev` | `eth0.200` | 基于以太网接口的 VLAN ID 为 200 的 NF_INTF | / | LAN | / |
| `intf_wifi_ap0` | `wifi_ap` | `wlan0` | Wi-Fi AP 模式对应的 NF_INTF | / | LAN | / |
| `intf_data_apn1` | `data_call` | 动态 | APN1 的数据拨号的 NF_INTF | 成功后的信息 | WAN | / |
| `intf_data_apn2` | `data_call` | 动态 | APN2 的数据拨号的 NF_INTF | 成功后的信息 | WAN | / |

#### 转发规则

| ACL 名称 | 数据流来源 NF_INTF | 数据流匹配规则 | 数据流目的 NF_INTF | 描述 |
|---------|----------------|-------------|-----------------|------|
| `acl_lan100_apn1` | `intf_lan100` | 所有数据流 | `intf_data_apn1` | 192.168.100.0/24 网段的局域网，通过 APN1 的数据拨号访问网络 |
| `acl_lan200_apn2` | `intf_lan200` | 所有数据流 | `intf_data_apn2` | 192.168.200.0/24 网段的局域网，通过 APN1（注：此处为 APN2）的数据拨号访问网络 |
| `acl_app_server` | `intf_local` | `url=test.com` | `intf_data_apn2` | 运行在模块的应用程序，通过 APN2 的数据拨号访问域名为 www.test.com 的网络服务 |
| `acl_app_apn1` | `intf_lan` | 所有数据流 | `intf_data_apn1` | 运行在模块的应用程序，通过 APN1 的数据拨号访问网络 |

### 4.3.3 规则配置

```bash
# 步骤一：设置默认承载 APN 信息为 SIM 卡签约 APN1 信息
set networks attach_apn apn_name=testapn1 ip_ver=4

# 步骤二：创建 LAN NF_INTF
add interface intf_eth type=com_dev ifname=eth0
add interface intf_eth100 type=vdev master=intf_eth vlan=100
add interface intf_eth200 type=vdev master=intf_eth vlan=200
add interface intf_wifi_ap0 type=wifi_ap index=0
add interface intf_lan100 type=bridge ifname=bridge100
set interface intf_lan100 proto=static conn=auto ip=192.168.100.1 netmask=255.255.255.0
set interface intf_lan100 addif=intf_eth100
set interface intf_lan100 addif=intf_wifi_ap0

add interface intf_lan200 type=bridge ifname=bridge200
set interface intf_lan200 proto=static conn=auto ip=192.168.200.1 netmask=255.255.255.0
set interface intf_lan200 addif=intf_eth200

# 步骤三：启动 DNS 代理服务和局域网的 DHCP 服务
add dhcp_srv dhcp_lan100 intf=intf_lan100
set dhcp_srv dhcp_lan100 ip_start=192.168.100.100 ip_end=192.168.100.200 \
    netmask=255.255.255.0 gateway=192.168.100.1 dnsp=192.168.100.1 lease_time=86400
start dhcp_srv dhcp_lan100
add dhcp_srv dhcp_lan200 intf=intf_lan200
set dhcp_srv dhcp_lan200 ip_start=192.168.200.100 ip_end=192.168.200.200 \
    netmask=255.255.255.0 gateway=192.168.200.1 dnsp=192.168.200.1 lease_time=86400
start dhcp_srv dhcp_lan200
add dns_srv dns_lan
start dns_srv dns_lan

# 步骤四：创建 WAN NF_INTF
add data_call 1
set data_call 1 apn_name=testapn1 ip_ver=4 reconn_mode=mode1 reconn_interval=10,20,40,80,120
start data_call 1
add data_call 2
set data_call 2 apn_name=testapn2 ip_ver=4 reconn_mode=mode1 reconn_interval=10,20,40,80,120
start data_call 2
add interface intf_data_apn1 type=data_call call_id=1
add interface intf_data_apn2 type=data_call call_id=2

# 步骤五：配置转发规则
add acl acl_lan100_apn1 sintf=intf_lan100 dintf=intf_data_apn1
add acl acl_lan200_apn2 sintf=intf_lan200 dintf=intf_data_apn2
add acl acl_app_server sintf=intf_local url=test.com dintf=intf_data_apn2
add acl acl_app_apn1 sintf=intf_local dintf=intf_data_apn1
```

### 4.3.4 场景测试

#### 测试设备信息（表 18）

| 设备 | 介绍 |
|------|------|
| LTE OPEN-EVB | 开发板 |
| （U）SIM 卡 | 普通中国联通（U）SIM 卡，支持多路拨号 |
| 模块 | 支持中国联通频段的模块，安装到 EVB |
| YT8521 模块 | PHY 模块，安装到 EVB |
| FC30R 模组 | Wi-Fi 模块，安装到 EVB |
| 千兆以太网交换机 | 通过以太网连接 PC1、PC2 和 PHY 模块 |
| PC1 | 运行 ubuntu1804 版本的笔记本电脑，接入 VID 100 的 VLAN |
| PC2 | 运行 ubuntu1804 版本的笔记本电脑，接入 VID 200 的 VLAN |
| 手机 | 支持 Wi-Fi 功能的安卓手机 |

**测试组网（图 14）：** SIM卡 → EC200A → EVB（含 FC30R Wi-Fi 模块、YT8521 PHY 模块）→ 千兆以太网交换机 → PC1、PC2；手机通过 Wi-Fi 连接 FC30R。

#### 4.3.4.1 模块配置

根据测试环境的情况，修改 APN1 和 APN2 的名称为 `3GNET` 和 `testapn`（测试使用的是普通（U）SIM 卡，支持使用任意 APN 名称进行数据拨号，实际使用时，请替换为（U）SIM 卡签约的 APN 名称）。修改域名 `www.test.com` 为可访问的域名 `www.quectel.com`。修改后的 NF 规则如下：

```bash
set networks attach_apn apn_name=3GNET ip_ver=4
add interface intf_eth type=com_dev ifname=eth0
add interface intf_eth100 type=vdev master=intf_eth vlan=100
add interface intf_eth200 type=vdev master=intf_eth vlan=200
add interface intf_wifi_ap0 type=wifi_ap index=0
add interface intf_lan100 type=bridge ifname=bridge100
set interface intf_lan100 proto=static conn=auto ip=192.168.100.1 netmask=255.255.255.0
set interface intf_lan100 addif=intf_eth100
set interface intf_lan100 addif=intf_wifi_ap0
add interface intf_lan200 type=bridge ifname=bridge200
set interface intf_lan200 proto=static conn=auto ip=192.168.200.1 netmask=255.255.255.0
set interface intf_lan200 addif=intf_eth200
add dhcp_srv dhcp_lan100 intf=intf_lan100
set dhcp_srv dhcp_lan100 ip_start=192.168.100.100 ip_end=192.168.100.200 \
    netmask=255.255.255.0 gateway=192.168.100.1 dnsp=192.168.100.1 lease_time=86400
start dhcp_srv dhcp_lan100
add dhcp_srv dhcp_lan200 intf=intf_lan200
set dhcp_srv dhcp_lan200 ip_start=192.168.200.100 ip_end=192.168.200.200 \
    netmask=255.255.255.0 gateway=192.168.200.1 dnsp=192.168.200.1 lease_time=86400
start dhcp_srv dhcp_lan200
add dns_srv dns_lan
start dns_srv dns_lan
add data_call 1
set data_call 1 apn_name=3GNET ip_ver=4 reconn_mode=mode1 reconn_interval=10,20,40,80,120
start data_call 1
add data_call 2
set data_call 2 apn_name=testapn ip_ver=4 reconn_mode=mode1 reconn_interval=10,20,40,80,120
start data_call 2
add interface intf_data_apn1 type=data_call call_id=1
add interface intf_data_apn2 type=data_call call_id=2
add acl acl_lan100_apn1 sintf=intf_lan100 dintf=intf_data_apn1
add acl acl_lan200_apn2 sintf=intf_lan200 dintf=intf_data_apn2
add acl acl_app_server sintf=intf_local url=quectel.com dintf=intf_data_apn2
add acl acl_app_apn1 sintf=intf_local dintf=intf_data_apn1
```

将 NF 规则写到文件 `/etc/ql_nf_preload.conf` 中，并重启设备。模块启动完成，等待注网成功后，检查数据拨号的 NF_INTF 是否都为 `connected` 状态。

**状态检查示例输出：**
```
~ # ql_cmd nf get interface all
name=intf_local status=connected id=1 type=local ifname=lo ip=127.0.0.1 netmask=255.255.255.0 subnet=24
name=intf_eth status=unconfig id=2 type=com_dev ifname=eth0
name=intf_eth100 status=binder id=3 type=vdev ifname=eth0.100
name=intf_eth200 status=binder id=4 type=vdev ifname=eth0.200
name=intf_wifi_ap0 status=none id=5 type=wifi_ap
name=intf_lan100 status=connected id=6 type=bridge ifname=bridge100 ip=192.168.100.1 netmask=255.255.255.255 subnet=24
name=intf_lan200 status=connected id=7 type=bridge ifname=bridge200 ip=192.168.200.1 netmask=255.255.255.255 subnet=24
name=intf_data_apn1 status=connected id=8 type=data_call ifname=rmnet_data0 ip=10.239.80.105 netmask=255.255.255.252 subnet=30 gat
name=intf_data_apn2 status=connected id=9 type=data_call ifname=rmnet_data1 ip=10.238.27.155 netmask=255.255.255.248 subnet=29 gat
```

#### 4.3.4.2 Wi-Fi 配置（wifi 接口部分目前接口暂时未开发）

在模块上运行 API 测试程序 `ql_sdk_api_test`，配置 Wi-Fi。设置步骤如下：

1. 运行 `ql_sdk_api_test`，选择 `Wi-Fi`，进入 Wi-Fi API 选项。
2. 选择 `ql_wifi_init` 进行初始化。
3. 选择 `ql_wifi_enable` 使用 Wi-Fi。
4. 选择 `ql_wifi_work_mode_set`，设置模式为"1-AP0"模式。
5. 选择 `ql_wifi_ap_ssid_set`，设置 AP0 的 SSID 为"quectel_test"。
6. 选择 `ql_wifi_ap_auth_set`，设置 AP0 的加密参数：WPA_WPA2_PSK_BOTH, 0-AUTO, 11111111。
7. 选择 `ql_wifi_ap_start`，打开 AP0。

手机选择 SSID `quectel_test`，并连接 Wi-Fi，连接后能正常访问网络。

#### 4.3.4.3 PC1 配置

断开 PC1 上其他的网络连接，并输入以下指令停止系统的网络管理功能：

```bash
sudo /etc/init.d/networking stop
```

然后输入以下命令，配置基于以太网的、VLAN ID 为 100 的网络连接：

```bash
sudo vconfig add eth0 100
sudo ifconfig eth0.100 up
sudo dhclient eth0.100
```

上述命令运行成功后，`eth0.100` 网卡可以获取到 IP 地址，PC1 可以访问网络。

#### 4.3.4.4 PC2 配置

断开 PC2 上其他的网络连接，并输入以下指令停止系统的网络管理功能：

```bash
sudo /etc/init.d/networking stop
```

然后输入以下命令，配置基于以太网的、VID 为 200 的网络连接：

```bash
sudo vconfig add eth0 200
sudo ifconfig eth0.200 up
sudo dhclient eth0.200
```

上述命令运行成功后，`eth0.100`（注：即 eth0.200）网卡可以获取到 IP 地址，PC2 可以访问网络。

#### 4.3.4.5 数据转发测试

通过 `ql_cmd nf get interface all` 命令可以看到，`intf_data_apn1` 对应的 Linux 网络接口是 `ccinet0`；`intf_data_apn2` 对应的 Linux 网络接口是 `ccinet1`。

在模块中，通过 tcpdump 在 `ccinet0` 和 `ccinet1` 上抓取数据包：

```bash
tcpdump -i ccinet0 -w /tmp/ccinet0 &
tcpdump -i ccinet1 -w /tmp/ccinet1 &
```

然后在模块、PC1、PC2 和手机上分别访问网络，查看网络访问路径是否按照 ACL 指定的路径传输。

**验证方法：**
- 通过 `ccinet0` 抓取包含 PC1 访问网络的报文；通过 `ccinet1` 抓取不包含 PC1 访问网络的报文。
- 通过 `ccinet1` 抓取包含 PC2 访问网络的报文；通过 `ccinet0` 抓取不包含 PC2 访问网络的报文。
- 通过 `ccinet1` 抓取包含模块访问 `www.quectel.com` 的报文；通过 `ccinet0` 抓取不包含模块访问 `www.quectel.com` 的报文。
- 通过 `ccinet0` 抓取包含模块访问其他网络的报文；通过 `ccinet1` 抓取不包含模块访问其他网络的报文。
- 通过 `ccinet0` 抓取到包含手机访问网络的报文；通过 `ccinet1` 抓取不包含手机访问网络的报文。

上述 3、4 场景的验证方法如下：在模块上使用 tcpdump 开启抓包后，使用 ping 命令分别访问 `www.quectel.com` 和 `www.github.com`。然后终止 tcpdump 进程，上传对应的报文文件 `/tmp/ccinet0.pcap` 和 `/tmp/ccinet1.pcap` 到 PC，并使用 wireshark 工具分析网络报文是否是在相应的 Linux 网络接口上传输。

---

## 4.4 场景四

### 4.4.1 场景需求

在一个网络场景中，LAN 侧根据不同的业务类型划分了不同类型的 VLAN，每个 VLAN 都是一个单独的局域网。**接入 VLAN 的设备使用静态 IP 地址和静态 ARP 的方式配置网络**。同时存在三路数据拨号，不同的数据拨号用于访问不同的业务。

#### 组网信息（表 19）

| VLAN ID | 用途 | 访问网络 | 设备信息 | 模块信息 |
|---------|------|---------|---------|---------|
| 4 | 内部通信 | 无 | IP: 192.168.4.100/24, MAC: 00:22:22:22:04:01 | IP: 192.168.4.101/24, MAC: 00:22:22:22:04:02 |
| 5 | 内部通信 | 无 | IP: 192.168.5.100/24, MAC: 00:22:22:22:05:01 | IP: 192.168.5.101/24, MAC: 00:22:22:22:05:02 |
| 41 | 访问网络 | APN1 对应的网络 | IP: 192.168.41.100/24, MAC: 00:22:22:22:41:01 | IP: 192.168.41.101/24, MAC: 00:22:22:22:41:02 |
| 42 | 访问网络 | APN2 对应的网络 | IP: 192.168.42.100/24, MAC: 00:22:22:22:42:01 | IP: 192.168.42.101/24, MAC: 00:22:22:22:42:02 |
| 43 | 访问网络 | APN3 对应的网络 | IP: 192.168.43.100/24, MAC: 00:22:22:22:43:01 | IP: 192.168.43.101/24, MAC: 00:22:22:22:43:02 |

#### 配置环境

1. （U）SIM 卡：签约三路 APN 信息。APN1 名称为 `testapn1`；APN2 名称为 `testapn2`；APN3 名称为 `testapn3`。其中 APN1 作为默认承载 APN。
2. WAN 接口：三路 APN 对应的数据拨号。
3. LAN 接口：基于 ETH VID 为 4、5、41、42、43 的 VLAN 接口。
4. 局域网：见上方表格。

#### 转发策略

1. VLAN ID 为 4 的 VLAN 和 VLAN ID 为 5 的 VLAN 用于局域网内部通信。
2. 使用 VLAN ID 为 41 接入局域网的设备，通过 APN1 访问网络。
3. 使用 VLAN ID 为 42 接入局域网的设备，通过 APN2 访问网络。
4. 使用 VLAN ID 为 43 接入局域网的设备，通过 APN3 访问网络。
5. `www.test.com` 对应的网络服务位于 APN3 对应的网络。模块上运行的程序和通过 VLAN ID 为 42 接入的 LAN 设备需要通过 APN3 的数据拨号访问该网络服务。
6. 模块上运行的程序使用 APN1 访问其他网络。

### 4.4.2 QL_NF 组网

| NF_INTF | 类型 | Linux 接口名 | 描述 | 地址 | WAN/LAN | 开启 DHCP |
|---------|------|------------|------|------|---------|----------|
| `intf_lan4` | `bridge` | `bridge4` | 管理 192.168.4.0/24 网段的局域网 | 192.168.4.101/24 | LAN | 否 |
| `intf_lan5` | `bridge` | `bridge5` | 管理 192.168.5.0/24 网段的局域网 | 192.168.5.101/24 | LAN | 否 |
| `intf_lan41` | `bridge` | `bridge41` | 管理 192.168.41.0/24 网段的局域网 | 192.168.41.101/24 | LAN | 否 |
| `intf_lan42` | `bridge` | `bridge42` | 管理 192.168.42.0/24 网段的局域网 | 192.168.42.101/24 | LAN | 否 |
| `intf_lan43` | `bridge` | `bridge43` | 管理 192.168.43.0/24 网段的局域网 | 192.168.43.101/24 | LAN | 否 |
| `intf_eth` | `com_dev` | `eth0` | 以太网接口的 NF_INTF | / | LAN | / |
| `intf_eth4` | `vdev` | `eth0.4` | 基于以太网接口创建的 VID 为 4 的 NF_INTF | / | LAN | / |
| `intf_eth5` | `vdev` | `eth0.5` | 基于以太网接口创建的 VID 为 5 的 NF_INTF | / | LAN | / |
| `intf_eth41` | `vdev` | `eth0.41` | 基于以太网接口创建的 VID 为 41 的 NF_INTF | / | LAN | / |
| `intf_eth42` | `vdev` | `eth0.42` | 基于以太网接口创建的 VID 为 42 的 NF_INTF | / | LAN | / |
| `intf_eth43` | `vdev` | `eth0.43` | 基于以太网接口创建的 VID 为 43 的 NF_INTF | / | LAN | / |
| `intf_data_apn1` | `data_call` | 动态 | APN1 的数据拨号的接口 | 成功后的信息 | WAN | / |
| `intf_data_apn2` | `data_call` | 动态 | APN2 的数据拨号的接口 | 成功后的信息 | WAN | / |
| `intf_data_apn3` | `data_call` | 动态 | APN3 的数据拨号的接口 | 成功后的信息 | WAN | / |

#### 转发规则

| ACL 名称 | 数据流来源 NF_INTF | 数据流匹配规则 | 数据流目的 NF_INTF | 描述 |
|---------|----------------|-------------|-----------------|------|
| `acl_app_server` | `intf_local` | `url=test.com` | `intf_data_apn3` | 运行在模块的应用程序，通过 APN3 的数据拨号访问域名为 www.test.com 的网络服务 |
| `acl_app_apn1` | `intf_local` | 所有数据流 | `intf_data_apn1` | 运行在模块的应用程序，通过 APN1 的数据拨号访问网络 |
| `acl_lan42_server` | `intf_lan42` | `url=test.com` | `intf_data_apn3` | 通过 VID 42 的设备，通过 APN3 的数据拨号访问域名为 www.test.com 的网络服务 |
| `acl_lan41_apn1` | `intf_lan41` | 所有数据流 | `intf_data_apn1` | 通过 VID 41 接入的设备，通过 APN1 的数据拨号访问网络 |
| `acl_lan42_apn2` | `intf_lan42` | 所有数据流 | `intf_data_apn2` | 通过 VID 42 接入的设备，通过 APN2 的数据拨号访问网络 |
| `acl_lan43_apn3` | `intf_lan43` | 所有数据流 | `intf_data_apn3` | 通过 VID 43 接入的设备，通过 APN3 的数据拨号访问网络 |

### 4.4.3 规则配置

```bash
# 步骤一：设置默认承载 APN 信息为(U)SIM 卡签约 APN1 信息
set networks attach_apn apn_name=testapn1 ip_ver=4

# 步骤二：创建 LAN NF_INTF，创建桥接类型的 NF_INTF 用于管理局域网，并把 LAN NF_INTF 加入桥接 NF_INTF
add interface intf_eth type=com_dev ifname=eth0
add interface intf_eth4 type=vdev master=intf_eth vlan=4
add interface intf_eth5 type=vdev master=intf_eth vlan=5
add interface intf_eth41 type=vdev master=intf_eth vlan=41
add interface intf_eth42 type=vdev master=intf_eth vlan=42
add interface intf_eth43 type=vdev master=intf_eth vlan=43

add interface intf_lan4 type=bridge ifname=bridge4
set interface intf_lan4 proto=static conn=auto ip=192.168.4.101 netmask=255.255.255.0 \
    mac=00:22:22:22:04:02 noarp=1
set interface intf_lan4 addif=intf_eth4

add interface intf_lan5 type=bridge ifname=bridge5
set interface intf_lan5 proto=static conn=auto ip=192.168.5.101 netmask=255.255.255.0 \
    mac=00:22:22:22:05:02 noarp=1
set interface intf_lan5 addif=intf_eth5

add interface intf_lan41 type=bridge ifname=bridge41
set interface intf_lan41 proto=static conn=auto ip=192.168.41.101 netmask=255.255.255.0 \
    mac=00:22:22:22:41:02 noarp=1
set interface intf_lan41 addif=intf_eth41

add interface intf_lan42 type=bridge ifname=bridge42
set interface intf_lan42 proto=static conn=auto ip=192.168.42.101 netmask=255.255.255.0 \
    mac=00:22:22:22:42:02 noarp=1
set interface intf_lan42 addif=intf_eth42

add interface intf_lan43 type=bridge ifname=bridge43
set interface intf_lan43 proto=static conn=auto ip=192.168.43.101 netmask=255.255.255.0 \
    mac=00:22:22:22:43:02 noarp=1
set interface intf_lan43 addif=intf_eth43

# 步骤三：配置静态 ARP 规则
add arp arp4  intf=intf_lan4  ip=192.168.4.102  mac=00:22:22:22:04:02
add arp arp5  intf=intf_lan5  ip=192.168.5.102  mac=00:22:22:22:05:02
add arp arp41 intf=intf_lan41 ip=192.168.41.102 mac=00:22:22:22:41:02
add arp arp42 intf=intf_lan42 ip=192.168.42.102 mac=00:22:22:22:42:02
add arp arp43 intf=intf_lan43 ip=192.168.43.102 mac=00:22:22:22:43:02

# 步骤四：启用 DNS 代理服务
add dns_srv dns_lan
start dns_srv dns_lan

# 步骤五：创建 WAN NF_INTF
add data_call 1
set data_call 1 apn_name=testapn1 ip_ver=4 reconn_mode=mode1 reconn_interval=10,20,40,80,120
start data_call 1
add data_call 2
set data_call 2 apn_name=testapn2 ip_ver=4 reconn_mode=mode1 reconn_interval=10,20,40,80,120
start data_call 2
add data_call 3
set data_call 3 apn_name=testapn3 ip_ver=4 reconn_mode=mode1 reconn_interval=10,20,40,80,120
start data_call 3
add interface intf_data_apn1 type=data_call call_id=1
add interface intf_data_apn2 type=data_call call_id=2
add interface intf_data_apn3 type=data_call call_id=3

# 步骤六：配置转发规则
add acl acl_app_server  sintf=intf_local  url=test.com   dintf=intf_data_apn3
add acl acl_app_apn1    sintf=intf_local                 dintf=intf_data_apn1
add acl acl_lan42_server sintf=intf_lan42 url=test.com   dintf=intf_data_apn3
add acl acl_lan41_apn1  sintf=intf_lan41                 dintf=intf_data_apn1
add acl acl_lan42_apn2  sintf=intf_lan42                 dintf=intf_data_apn2
add acl acl_lan43_apn3  sintf=intf_lan43                 dintf=intf_data_apn3
```

---

## 5 附录：参考文档及术语缩写

### 参考文档（表 20）

| 编号 | 文档名称 |
|------|---------|
| [1] | Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |
| [2] | Quectel_EC200A-CN(TA)_QuecOpen_Wi-Fi_API_参考手册 |
| [3] | Quectel_EC200A-CN(TA)_QuecOpen_数据拨号 API_参考手册 |

### 术语缩写（表 21）

| 缩写 | 英文描述 | 中文描述 |
|------|---------|---------|
| ACL | Access Control Lists | 访问控制列表 |
| AP | Access Point | 无线接入点 |
| APP | Application | 应用程序 |
| APN | Access Point Name | 接入点名称 |
| CHAP | Challenge Handshake Authentication Protocol | 挑战握手认证协议 |
| DHCP | Dynamic Host Configuration Protocol | 动态主机配置协议 |
| DNS | Domain Name System | 域名系统 |
| DSDA | Dual SIM Dual Active | 双卡双通 |
| ECL | Emitter Coupled Logic | 发射极耦合逻辑电路 |
| ETH | Ethernet | 以太网 |
| LAN | Local Area Network | 局域网 |
| LTE | Long Term Evolution | 长期演进 |

---

## 附：文档版权信息

- **发布单位**：上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.）
- **地址**：上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼，邮编：200233
- **电话**：+86 21 5108 6236
- **邮箱**：info@quectel.com
- **技术支持**：support@quectel.com
- **版权所有**：© 上海移远通信技术股份有限公司 2022，保留一切权利

> **免责声明**：本文档为临时（Preliminary）版本，移远通信可在未事先通知的情况下，自行决定随时增加、修改或重述本文档。文档中所提供的参考设计仅作为示例，用户需基于独立的分析、评估和判断使用本文档信息。

---

*本 Markdown 文档由自动化工具基于原始 PDF（共 70 页）完整提取生成，涵盖文档历史、所有章节正文、全部表格（21 张）、所有代码示例及图示描述，无内容遗漏。*
