# Quectel AG35-CET & AG35-EUT QuecOpen(SDK) 网络管理指导 — 全文详尽分析

> **文档全名**：AG35-CET&AG35-EUT QuecOpen(SDK) 网络管理指导
> **适用模块**：移远通信 AG35-CET / AG35-EUT（LTE Standard 模块系列）
> **版本**：V1.0.1（临时文件 / Preliminary Confidential）
> **日期**：2025-03-28
> **总页数**：73 页（PDF），文档内页码标注 1/72 ~ 71/72（封面不计入页脚）
> **本分析覆盖页范围**：全 73 页（分段读取并逐段写入）
>
> 本分析对原文逐章逐节展开，保留所有 API 原型、参数表、枚举值、AT/shell 命令、示例代码与表格，并补充技术解读。

---

## 文档元信息与修订历史

**修订记录（第 3 页）**：

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| -    | 2024-01-26 | Yumn HUANG | 文档创建 |
| 1.0.0 | 2024-01-26 | Yumn HUANG | 临时版本 |
| 1.0.1 | 2025-03-28 | Allen FENG | 临时版本；**新增 ACL 转发功能（第 3.9.2.1 章）** |

> 解读：V1.0.1 相对 V1.0.0 的唯一实质改动是新增了 ACL 转发功能（见后文 3.9.2.1）。文档全程标注 "Preliminary / Quectel Confidential"，属临时保密文件。

**文档结构（目录，第 4–5 页）**：
- 第 1 章 引言
- 第 2 章 网络管理概念解释
- 第 3 章 QL_NF 介绍（核心章节）：3.1 规则配置 / 3.2 NF_INTF / 3.3 VLAN 管理 / 3.4 桥接管理 / 3.5 Wi-Fi 管理 / 3.6 LAN 管理（DHCP、DNS、LAN 主机）/ 3.7 静态 ARP 规则管理 / 3.8 数据拨号 / 3.9 转发策略（含 ACL）/ 3.10 QL_NF API / 3.11 注意事项
- 第 4 章 场景案例（场景一~四）
- 第 5 章 附录：参考文档及术语缩写

**表格索引（24 个表）/ 图片索引（16 张图）**，关键的有：图 2 QL_NF 整体框图、图 3 NF_INTF 状态变迁图、图 5 数据拨号状态变迁图、图 6 ACL 工作示意图。

---

## 1 引言（第 8 页）

- AG35-CET 和 AG35-EUT 模块支持 **QuecOpen®** 方案——基于 **Linux** 的嵌入式开发平台，用于简化 IoV（车联网）应用的软件设计与开发。详细信息参考文档 [1]。
- 本文档适用于 **SDK 构建环境**的 QuecOpen 方案，主要介绍 AG35-CET/AG35-EUT 模块的**网络管理框架（QL_NF）**，涉及：数据拨号、LAN 管理、网络接口管理、数据转发策略管理等。

---

## 2 网络管理概念解释（第 9–11 页）

本章以一个常用路由器为例解释网络管理相关概念。

**表 1：路由器相关配置**

| 配置项 | 值 |
|---|---|
| WAN 口数量 | 1 个，以太网口 |
| LAN 口数量 | 2 个，以太网口 |
| WIFI | 支持 |
| 防火墙 | 支持 |
| 管理方式 | HTTP 服务器 |

**图 1 路由器功能图**所示的拓扑（自上而下）：
- 外部「网络」云 —— 经 RJ45 接入 **eth1（WAN 接口）**。
- WAN 接口下方有「防火墙」，再到中部的「本地服务（HTTP Server / DHCP Server / DNS Server）」「本地程序（APP1/APP2…）」与左侧「转发策略」模块。
- 下方再经一层「防火墙」到 **LAN 接口**：`wlan0`（Wi-Fi）、`eth0.1`、`eth0.2`（以太网），分别接手机、PC、打印机。

**核心概念定义**：

- **网络接口（Network Interface）**：Linux 系统的网卡名。以太网对应 `eth1`、`eth0.1`、`eth0.2`；Wi-Fi 对应 `wlan0`。
  1. **WAN 接口（WAN Interface）**：连接外部网络的 Linux 网络接口。可同时存在多个 WAN 接口。在移远模块上，WAN 接口还可以是**拨号网络接口**、**USB 设备网络接口（ECM、RNDIS、NCM）**和 **WIFI Station 模式网络接口**等。
  2. **LAN 接口（LAN Interface）**：接入 LAN 设备的网络接口。手机经 Wi-Fi 接入 → `wlan0`；PC 和打印机经 ETH 接入 → `eth0.1`/`eth0.2`。在移远模块上，LAN 接口还可以是 USB 网络接口（ECM、RNDIS、NCM）等。
- **LAN 设备（LAN Device）**：通过 LAN 口接入局域网的设备（手机、PC、打印机等）。
- **本地服务（Local Service）**：在路由器运行的网络服务程序，如 DHCP Server、DNS Server、HTTP Server。
- **本地程序（Local Application）**：在路由器运行且会访问网络的程序，如 APP1/APP2。
- **DHCP Server**：为 LAN 设备提供 IP 地址管理服务。
- **DNS Server**：为 LAN 设备提供域名解析服务。
- **防火墙（Firewall）**：阻止或允许外部设备通过 WAN 口或 LAN 口访问本地服务。例：允许 LAN 设备访问 DHCP/DNS/HTTP Server，禁止外部网络设备访问 DHCP/DNS Server，出于安全也禁止外部访问 HTTP Server。
- **转发策略（Forwarding Policy）**：LAN 设备通过路由器访问外部网络时，路由器内的网络配置策略，包括路由处理和转发处理等。

> 解读：本章建立的术语（WAN/LAN 接口、本地服务/程序、防火墙、转发策略）是后文 QL_NF 框架的概念基础。AG35 作为路由器形态时具备 1 WAN + 2 LAN + WiFi 的能力。

---

## 3 QL_NF 介绍（第 12 页起）

### 设计动机

传统网络管理基于 Linux 网络接口实现路由和转发策略。网络接口状态变化以及各种网络工具的配合使用，导致网络处理逻辑复杂且难以维护。对于复杂应用场景，开发者需要了解各种网络协议、Linux 网络技术（`route`、`bridge`、`netfilter` 等）和网络工具（`IP`、`iptables`、`ebtables`、`brctl` 等）的使用方式，这不但加长开发周期，而且一些需依赖经验解决的问题无法得到解决。

为解决该问题，移远将网络设备管理、转发策略等封装为一系列**可描述的命令**，这套网络管理框架称为 **QL_NF（Quectel Network Framework）**。基于 QL_NF，开发者无需关注技术细节或路由/转发处理，只需了解第 2 章概念，结合应用场景设置相应规则即可完成配置。

在 QL_NF 中，基于 Linux 网络设备接口，移远封装了一层网络接口 **NF_INTF（Quectel Network Framework Interface）**。后文 **LAN 管理、WAN 管理、转发策略等均基于 NF_INTF**。

**图 2 QL_NF 整体框图**（三层结构）：
- **顶层业务模块**：左侧 WAN1/WAN2/WAN3… 接口；中部「本地服务」「ACL / 转发策略 / URL Binder」「QOS」；右侧防火墙、LAN1/LAN2/LAN3… 接口、本地程序。WAN 与本地服务、本地服务与 LAN 之间各有防火墙。
- **中间层**：QUECTEL 网络框架接口层。
- **底层**：Linux 网络框架接口层——数据拨号、USB 网络设备、VLAN 设备、ETH、VPN、PPP、WIFI AP 模式、WIFI Station 模式、Linux 桥接。

> 解读：QL_NF 是对 Linux 网络栈（route/iptables/ebtables/brctl 等）的抽象封装。它把 WAN/LAN/防火墙/转发/QoS/URL Binder/ACL 等都构建在统一的 NF_INTF 抽象之上。QOS 与 firewall 在本版本中尚 "暂不支持"（见 3.1 nf_target 表）。

---

### 3.1 QL_NF 规则配置（第 13–14 页）

QL_NF 规则通过**可描述的命令**配置，当前支持普通格式配置。

**命令格式**：
```
<action> <nf_target> <target_name> <param_list>
```

**参数解释**：

- **`<action>`** — QL_NF 目标规则操作：
  | action | 含义 |
  |---|---|
  | `add` | 添加规则 |
  | `del` | 删除规则 |
  | `set` | 配置规则 |
  | `get` | 获取规则信息或配置 |
  | `start` | 使能规则 |
  | `stop` | 去使能规则 |

- **`<nf_target>`** — QL_NF 目标，支持如下目标：
  | nf_target | 含义 |
  |---|---|
  | `interface` | NF_INTF 管理 |
  | `acl` | 转发策略管理、URL 绑定管理 |
  | `data_call` | 数据拨号管理 |
  | `dns_srv` | DNS 服务管理 |
  | `dhcp_srv` | DHCP 服务管理 |
  | `networks` | 无线网络管理，包括无线网络注册、无线网络异常处理等 |
  | `arp` | ARP 服务管理 |
  | `lanhost` | LAN 主机管理 |
  | `qos` | QoS 管理（**暂不支持**） |
  | `firewall` | 防火墙管理（**暂不支持**） |

- **`<target_name>`** — 由客户根据 `<nf_target>` 定义，通常为 QL_NF 目标实例的唯一标识；`all` 是特殊关键字，表示所有 QL_NF 目标。
- **`<param_list>`** — 参数列表，支持同时获取或配置多个参数，参数之间以空格分隔。设置参数时，参数名和参数值以 `=` 连接，例如：`param1=value1 param2=value2`。

**三种配置方式**：
- **方式一（推荐）**：预加载方式。系统启动后，QL_NF 服务加载 `/etc/ql_nf_preload.conf` 文件中的配置规则。若需添加配置规则，可写入 SDK 的 `ql-ol-rootfs/etc/ql_nf_preload.conf` 文件，执行 `make rootfs` 编译生成镜像烧录到模块中使用。
- **方式二**：API 接口。头文件为 `ql_nf.h`，示例代码参见 `sample/nf`。
- **方式三**：shell 命令 `ql_cmd nf xxxx`。建议调试时使用此方式，调试完毕后采用方式一固化配置。

**特殊命令**：
```
get all：获取当前支持的所有 QL_NF 目标。
del all：删除所有规则。
```

> 解读：这是 QL_NF 的核心命令语法。所有后续功能（NF_INTF、DHCP、DNS、ARP、拨号、ACL）都遵循 `<action> <nf_target> <target_name> <param_list>` 这个统一语法。三种配置方式中，shell 命令用于调试，preload.conf 用于固化，C API 用于程序集成。

---

### 3.2 NF_INTF

#### 3.2.1 NF_INTF 规则配置（第 14–15 页）

**命令格式**：
```
<action> interface <interface_name> <param_list>
```

**参数解释**：
- **`<action>`** — QL_NF 操作：
  | action | 含义 |
  |---|---|
  | `add` | 创建 NF_INTF 实例 |
  | `del` | 删除 NF_INTF 实例 |
  | `set` | 设置 NF_INTF 参数 |
  | `get` | 获取 NF_INTF 状态或配置，返回配置参数（详见表 2）或状态参数（详见表 3） |
  | `start` | 开始 NF_INTF 寻址（寻址方式参见 3.2.2） |
  | `stop` | 停止 NF_INTF 寻址 |
- **`<interface_name>`** — NF_INTF 名称，NF_INTF 的唯一标识；**最大长度 32 字节**。其他 QL_NF 目标规则都使用 `<interface_name>` 引用对应的 NF_INTF。
- **`<param_list>`** — 参数列表。

**示例**：
```
add interface <interface_name> <key_params>     # 添加 NF_INTF 规则（详见 3.2.4）
del interface <interface_name>                   # 删除 NF_INTF 规则
set interface <interface_name> <param_list>      # 配置 NF_INTF 规则（param_list 详见表 2）
get interface <interface_name> class=info        # 获取 NF_INTF 状态参数（详见表 3；name=all 获取所有）
get interface <interface_name> class=config      # 获取 NF_INTF 配置参数（详见表 2；name=all 获取所有）
```

**表 2：NF_INTF 配置参数**（`class=config` 返回 / `set` 设置）

| 参数 | 类型 | 描述 |
|---|---|---|
| `proto` | 字符串 | 寻址方式。`none`=未设置寻址方式；`dhcp`=使用 DHCP 协议寻址；`static`=静态 IP 地址；`inherit`=继承网络接口的寻址方式，当前只有 `data_call` 类型会自动设置为该方式。 |
| `mtu` | 整型 | MTU 值。范围：1~1518。 |
| `mac` | MAC 地址格式字符串 | MAC 地址 |
| `ip` | IP 地址格式字符串 | 设置 IP 地址（`static` 寻址方式使用） |
| `netmask` | IP 地址格式字符串 | 设置子网掩码（`static` 寻址方式使用） |
| `gateway` | IP 地址格式字符串 | 设置网关（`static` 寻址方式使用） |
| `dnsp` | IP 地址格式字符串 | 首选 DNS。注意：设置的首选 DNS 会覆盖 NF_INTF 使用其他寻址方式获取的首选 DNS。 |
| `dnss` | IP 地址格式字符串 | 备选 DNS。注意：设置的备选 DNS 会覆盖 NF_INTF 使用其他寻址方式获取的备选 DNS。 |
| `conn` | 字符串 | 自动重连类型（DHCP 寻址方式使用，当寻址失败后是否进行重试）。`once`=不重试，NF_INTF 进入 `disconnected` 状态；`auto`=自动重试。 |
| `vendor` | 字符串 | 制造商名称（DHCP 寻址方式使用，DHCP OPTION 60 字段）。长度范围：1~32 字节。 |
| `hostname` | 字符串 | 主机名称（DHCP 寻址方式使用，DHCP OPTION 50 字段）。长度范围：1~32 字节。 |
| `addif` | 字符串 | 把传入的 NF_INTF 加入当前 `bridge` 类型的 NF_INTF 中。长度范围：1~32 字节。 |
| `delif` | 字符串 | 把传入的 NF_INTF 从当前 `bridge` 类型的 NF_INTF 中删除。长度范围：1~32 字节。 |

> 解读：DHCP OPTION 50 是请求的 IP 地址（这里用作 hostname 字段——原文标注），OPTION 60 是 Vendor Class Identifier。`addif`/`delif` 是桥接端口的增删操作。`dnsp`/`dnss` 的覆盖语义需注意：手动设置会覆盖 DHCP/拨号自动下发的 DNS。

**表 3：NF_INTF 状态参数**（`class=info` 返回）

| 参数 | 类型 | 描述 |
|---|---|---|
| `name` | 字符串 | NF_INTF 名称。长度范围：1~32 字节。 |
| `status` | 字符串 | NF_INTF 状态；取值详见 3.2.3。 |
| `ifname` | 字符串 | 对应 Linux 网络接口名称。长度范围：1~32 字节。 |
| `ip` | IP 地址格式字符串 | IP 地址 |
| `netmask` | IP 地址格式字符串 | 子网掩码 |
| `gateway` | IP 地址格式字符串 | 网关 |
| `dnsp` | IP 地址格式字符串 | 首选 DNS |
| `dnss` | IP 地址格式字符串 | 备选 DNS |

#### 3.2.2 NF_INTF 寻址方式（第 16 页）

NF_INTF 获取 IP 地址后，其他 QL_NF 规则才能生效。NF_INTF 获取 IP 地址的过程称为**寻址**。

**表 4：NF_INTF 寻址方式**

| 寻址方式 | 描述 |
|---|---|
| `dhcp` | 使用 DHCP 协议获取 IP 地址。 |
| `static` | 使用静态 IP 地址（手动 IP 地址）；如果 `conn` 参数设置为 `auto`，并且已经配置静态 IP 地址，NF_INTF 将会立即进入 `connected` 状态。 |
| `inherit` | 继承 Linux 网络接口的寻址方式，目前只有 `data_call` 使用该寻址方式。 |

#### 3.2.3 NF_INTF 状态（第 16–17 页）

NF_INTF 支持 **8 种状态**：

**表 5：NF_INTF 状态**

| 状态 | 描述 |
|---|---|
| `none` | 对应的 Linux 网络接口不存在。 |
| `unconfig` | 对应的 Linux 网络接口存在，但未配置寻址方式。 |
| `ready` | 已经配置寻址方式，可以使能寻址。 |
| `binder` | NF_INTF 绑定在桥接 NF_INTF 下，作为一个桥接端口。 |
| `connecting` | 正在寻址。 |
| `connected` | 寻址成功，获取到 IP 地址。 |
| `disconnected` | 寻址失败或者连接断开。 |

> 注：原文称 "8 种状态" 但表 5 列出 7 项（none/unconfig/ready/binder/connecting/connected/disconnected）。第 8 种可能是初始/无效态未单独列出，属原文表述与列举数量的轻微不一致。

**图 3 NF_INTF 状态变迁图**（时序）：
- `none` → `unconfig`：检测到 Linux 网络接口（"Linux network interface detected"）。
- `unconfig` → `ready`：寻址方式已配置（"Addressing mode has been configured"）。
- `ready` → `binder`：把 NF_INTF 加入桥接 NF_INTF（"Add NF_INTF to bridge NF_INTF"）。
- `ready` → `connecting`：Start CMD（执行 `start`）。
- `connecting` → `connected`：寻址成功（"Addressing success"）。
- `connecting`/`connected` → `disconnected`：寻址失败（"Addressing failure"）/ 链路断开 + Stop CMD（"Link disconnection, Stop CMD"）。
- `disconnected` → `connecting`：Start CMD 或重连（"Start CMD or Reconnect"）。

SDK 中提供了 NF_INTF 状态变化上报的接口函数，参见 SDK 示例程序 `sample/nf`。若用户应用程序需要监控网络状态变化，请参考 3.10 章。

#### 3.2.4 创建 NF_INTF（第 17–18 页）

**命令格式**：
```
add interface <interface_name> type=<interface_type> <key_params>
```

**参数解释**：
- `<interface_name>` — NF_INTF 名称，唯一标识；最大长度 32 字节。
- `<interface_type>` — NF_INTF 类型；详见表 6。
- `<key_params>` — 关键参数：NF_INTF 通过关键参数建立和 Linux 网络接口的映射。详见表 6 和表 7。

创建 NF_INTF 时，根据 NF_INTF 类型传入对应的关键参数，NF 建立 NF_INTF 和 Linux 网络接口的映射关系。

**表 6：NF_INTF 类型（`<interface_type>`）和关键参数（`<key_params>`）**

| interface_type | 描述 | key_params |
|---|---|---|
| `data_call` | 数据拨号接口；参考详见 3.8 章。 | `call_id` |
| `usb_dev` | USB 网络接口，自动识别 USB 网络接口名称（ecm0、usbnet0 等） | / |
| `bridge` | 桥接设备；需要传入网络接口名称（`ifname`），NF_INTF 自动创建桥接的网络接口。 | `ifname` |
| `vdev` | VLAN 接口；传入主 NF_INTF 名称和 VLAN ID。在主 NF_INTF 基础上创建一个 VLAN NF_INTF。 | `vlan`、`master` |
| `com_dev` | 通用接口；根据传入网络接口名称（`ifname`），自动识别网络接口。 | `ifname` |
| `local` | 本地接口；特殊接口，表示所有本地服务和程序，系统自动创建，用户没有权限操作该接口。 | / |
| `wifi_ap` | Wi-Fi AP 模式对应接口。 | `index` |
| `wifi_sta` | Wi-Fi STA 模式对应的接口。 | / |

**表 7：NF_INTF 关键参数（`<key_params>`）描述**

| key_params | 类型 | 描述 |
|---|---|---|
| `call_id` | 整型 | 数据拨号的唯一标识。范围：1~INT MAX。 |
| `ifname` | 字符串 | Linux 网络接口名。范围：1~32 字节。 |
| `vlan` | 整型 | VLAN ID。范围：1~4096。 |
| `master` | 字符串 | 主 NF_INTF 名称。范围：1~32 字节。 |

**创建各类型 NF_INTF 示例**：
```
# data_call 类型：
add interface intf_public type=data_call call_id=1

# usb_dev 类型：
add interface intf_usb type=usb_dev

# bridge 类型：
add interface intf_lan type=bridge ifname=br0

# vdev 类型：
add interface intf_usb_300 type=vdev master=intf_usb vlan=300

# com_dev 类型：
add interface intf_eth type=com_dev ifname=eth0
```

#### 3.2.5 NF_INTF 配置示例（第 19 页）

创建 `usb_dev` 类型的 NF_INTF，使用 DHCP 方式寻址，步骤如下：
```
# 步骤一：添加 usb_dev 类型的 NF_INTF
add interface intf_usb type=usb_dev

# 步骤二：设置 NF_INTF 寻址方式和自动重连类型
set interface intf_usb proto=dhcp conn=auto

# 步骤三：使能 NF_INTF，开始寻址
start interface intf_usb

# 步骤四：查看 NF_INTF 状态
get interface intf_usb class=info
```

> 解读：这是 NF_INTF 完整生命周期的典型流程：add（创建映射）→ set（配置寻址方式 proto + 重连策略 conn）→ start（触发寻址）→ get class=info（查询状态/IP）。

---

### 3.3 VLAN 管理（第 19 页起）

QL_NF 支持基于 LAN 接口创建 VLAN 接口，**只支持创建单层 VLAN 标签的 VLAN 接口**。在实际运用中，通常使用 USB 网络接口和以太网接口创建对应的 VLAN 接口。可以使用 QL_NF 创建**不多于 64 个** VLAN NF_INTF。

**VLAN 创建示例（第 20 页）**：
```
# 首先创建 USB 网络接口及以太网接口对应的 NF_INTF：
add interface intf_usb type=usb_dev
add interface intf_eth type=com_dev ifname=eth0

# 创建 VLAN ID 为 100、基于 USB 网络接口的 VLAN NF_INTF：
add interface intf_vdev1 type=vdev master=intf_usb vlan=100

# 创建 VLAN ID 为 200、基于 USB 网络接口的 VLAN NF_INTF：
add interface intf_vdev2 type=vdev master=intf_usb vlan=200

# 创建 VLAN ID 为 10、基于以太网接口的 VLAN NF_INTF：
add interface intf_vdev3 type=vdev master=intf_eth vlan=10

# 创建 VLAN ID 为 20、基于以太网接口的 VLAN NF_INTF：
add interface intf_vdev4 type=vdev master=intf_eth vlan=20
```

**VLAN 优先级（PCP）**：QL_NF 也支持设置 VLAN 接口发出数据报文中的 **VLAN 优先级字段（PCP，Priority Code Point）**。在创建 VLAN NF_INTF 时，使用 `vlan_pri` 参数传入 VLAN 优先级值：
```
# 创建 VLAN ID 为 300、输出数据报文的 VLAN 优先级为 4、基于 USB 网络接口的 VLAN NF_INTF：
add interface intf_vdev5 type=vdev master=intf_usb vlan=300 vlan_pri=4

# 创建 VLAN ID 为 30、输出数据报文的 VLAN 优先级为 5、基于以太网接口的 VLAN NF_INTF：
add interface intf_vdev6 type=vdev master=intf_eth vlan=30 vlan_pri=5
```

> 解读：VLAN 接口必须基于一个已存在的主 NF_INTF（`master`），仅支持单层 VLAN 标签（不支持 QinQ），上限 64 个 VLAN NF_INTF。`vlan_pri` 对应 802.1p 的 PCP 字段（0~7），用于 QoS 标记。

---

### 3.4 桥接管理（第 20–21 页）

桥接是一种采用不同接入方式接入模块的设备接入同一个局域网的方法。在 QL_NF 中，**任意类型的 NF_INTF 都可以加入桥接 NF_INTF**，常用的是把 LAN NF_INTF 加入桥接 NF_INTF。常用的 LAN 接口有：USB 网络接口、以太网接口、Wi-Fi 接口、基于 USB 网络接口以及以太网接口的 VLAN 接口。

**桥接 NF_INTF 的使用方法**：创建 LAN NF_INTF；创建桥接 NF_INTF；把 LAN NF_INTF 加入桥接 NF_INTF。

**示例**（将 USB 网络接口、以太网接口和基于以太网接口的 VLAN ID 为 200 的 VLAN 接口加入一个桥接接口）：
```
# 步骤一：创建 USB 网络接口、以太网接口、以及 VLAN 接口对应的 NF_INTF
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

**表 8：扩展配置参数**（桥接接口额外支持的配置参数）

| 参数 | 类型 | 描述 |
|---|---|---|
| `noarp` | 整型 | 打开或关闭桥接接口的 ARP 功能。若关闭 ARP 功能，则使用静态 ARP。`0`=打开（默认值）；`1`=禁止。 |
| `isolate` | 整型 | 打开或关闭桥接接口下 LAN 接口之间的数据隔离。`0`=关闭（默认值）；`1`=打开。 |

> 解读：`isolate=1` 实现 AP 隔离（同一桥下的 LAN 设备互相不能通信，只能上网），常用于公共网络安全场景。`noarp=1` 关闭动态 ARP 学习，配合静态 ARP 规则（3.7）使用。

---

### 3.5 Wi-Fi 管理（第 21–22 页）

QL_NF 支持管理通过 **QuecOpen Wi-Fi API** 创建的 Wi-Fi 接口。QuecOpen Wi-Fi API 支持创建 **1~2 个 AP 模式**的网络接口以及 **1 个 STA 模式**的网络接口，详见文档 [2]。

创建 Wi-Fi 相关的 NF_INTF 以及相应的转发规则**不需要 Wi-Fi 网络接口已经存在**，QL_NF 可以自动识别是否创建或删除了 Wi-Fi 网络接口及其状态变化，并动态更改相应 NF_INTF 的状态。在开发过程中，可以将 Wi-Fi 相关的规则预设在 `/etc/ql_nf_preload.conf` 文件中，然后在应用程序中使用 Wi-Fi API 管理 Wi-Fi。

**创建 Wi-Fi 的 NF_INTF 示例**：
```
# 创建 Wi-Fi AP0 的 NF_INTF：
add interface intf_wifi_ap0 type=wifi_ap index=0

# 创建 Wi-Fi AP1 的 NF_INTF：
add interface intf_wifi_ap1 type=wifi_ap index=1

# 创建 Wi-Fi STA 模式的 NF_INTF：
add interface intf_wifi_sta type=wifi_sta
```

> 解读：`wifi_ap` 用 `index`（0/1）区分两个 AP 实例，`wifi_sta` 无需 key_params。NF_INTF 可先于实际 Wi-Fi 接口创建——这是一种解耦设计，便于预加载配置。

---

### 3.6 LAN 管理（第 22 页起）

LAN 管理包括 **DHCP 服务、DNS 服务和 LAN 设备**的管理。DHCP 服务为 LAN 设备分配 IP 地址。DNS 服务为 LAN 设备提供域名解析服务。

LAN 设备可以通过 **ETH、USB 和 Wi-Fi** 等方式接入同一个局域网。接入局域网过程中需要使用桥接方式，将 ETH、USB 和 Wi-Fi 对应的 NF_INTF 加到桥接 NF_INTF 下，然后在桥接 NF_INTF 上管理 DHCP 服务。`/etc/ql_nf_preload.conf` 文件中的预置规则即是创建 USB、ETH 和桥接类型的 NF_INTF，并将 USB 和 ETH 对应的 NF_INTF 加入到桥接 NF_INTF：
```
ql_cmd add interface intf_usb type=usb_dev
ql_cmd add interface intf_lan type=bridge ifname=br0 proto=static conn=auto
ql_cmd set interface intf_lan ip=192.168.225.1 netmask=255.255.255.0
ql_cmd set interface intf_lan addif=intf_usb
```

#### 3.6.1 DHCP 服务管理（第 22–24 页）

QL_NF 使用 `dhcp_srv` 管理 DHCP 服务，**一个 DHCP 服务和一个 LAN NF_INTF 绑定**，支持同时启动多个 DHCP 服务。

**命令格式**：
```
<action> dhcp_srv <dhcp_srv_name> <param_list>
```

**参数解释**：
- `<action>`：`add`（添加 DHCP 服务实例）、`del`（删除）、`set`（配置）、`get`（获取状态或配置，返回配置参数表 9 或状态参数表 10）、`start`（开启）、`stop`（停止）。
- `<dhcp_srv_name>`：DHCP 服务名称，唯一标识；最大长度 32 字节。
- `<param_list>`：参数列表。

**示例**：
```
add dhcp_srv <dhcp_srv_name>                       # 添加
del dhcp_srv <dhcp_srv_name>                       # 删除
set dhcp_srv <dhcp_srv_name> <param_list>          # 配置（param_list 见表 9）
get dhcp_srv <dhcp_srv_name> class=info            # 获取状态参数（见表 10）
get dhcp_srv <dhcp_srv_name> class=config          # 获取配置参数（见表 9）
start dhcp_srv <dhcp_srv_name>                     # 开启
stop dhcp_srv <dhcp_srv_name>                      # 停止
```

**表 9：DHCP 服务配置参数**

| 参数 | 类型 | 描述 |
|---|---|---|
| `intf` | 字符串 | 绑定的 LAN NF_INTF。长度范围：1~32 字节。 |
| `ip_start` | IP 地址格式字符串 | DHCP 地址池起始地址 |
| `ip_end` | IP 地址格式字符串 | DHCP 地址池结束地址 |
| `netmask` | IP 地址格式字符串 | 子网掩码 |
| `gateway` | IP 地址格式字符串 | 网关地址。注意：网关地址必须和绑定的 NF_INTF 的 IP 地址相同。 |
| `dnsp` | IP 地址格式字符串 | 首选 DNS。常设置为网关地址。 |
| `dnss` | IP 地址格式字符串 | 备选 DNS。常设置为空。 |
| `lease_time` | 整数 | DHCP 地址租期。单位：小时，范围：1~INT MAX。建议设置为 12 小时以上。 |

> 注意：表 9 中 `lease_time` 单位描述为"小时"，但下文示例中 `lease_time=86400`（=86400 秒=24 小时），说明实际命令传入的是**秒**。原文存在单位描述（小时）与示例取值（秒）的不一致，实际应以秒为单位（86400 秒 = 1 天）。

**表 10：DHCP 服务状态参数**

| 参数 | 类型 | 描述 |
|---|---|---|
| `name` | 字符串 | DHCP 服务名称。长度范围：1~32 字节。 |
| `start` | 整型 | 服务是否开启。`0`=未开启；`1`=已开启。 |
| `status` | 字符串 | DHCP 服务状态。`none`=服务未启动；`active`=服务正在运行中；`error`=发生内部错误。 |

**3.6.1.1 DHCP 配置示例（第 24–25 页）**：通过 `/etc/ql_nf_preload.conf` 中的预置命令创建桥接 NF_INTF，并启动 192.168.225.0 网段的 DHCP 服务：
```
add interface intf_usb type=usb_dev
add interface intf_lan type=bridge ifname=br0
set interface intf_lan proto=static conn=auto ip=192.168.225.1 netmask=255.255.255.0
set interface intf_lan addif=intf_usb
add dhcp_srv dhcp_lan intf=intf_lan
set dhcp_srv dhcp_lan ip_start=192.168.225.100 ip_end=192.168.225.200 netmask=255.255.255.0 gateway=192.168.225.1 dnsp=192.168.225.1 lease_time=86400
start dhcp_srv dhcp_lan
```

#### 3.6.2 DNS 服务管理（第 25–26 页）

QL_NF 使用 `dns_srv` 管理 DNS 服务，**系统只能启动一个 DNS 服务**。

**命令格式**：
```
<action> dns_srv <dns_srv_name>
```

**参数解释**：
- `<action>`：`add`（添加）、`del`（删除）、`get`（获取状态，返回状态参数表 11）、`start`（开启）、`stop`（停止）。
- `<dns_srv_name>`：DNS 服务名称，最大长度 32 字节，唯一标识。

**示例**：
```
ql_cmd nf add dns_srv <dns_srv_name>      # 添加
ql_cmd nf del dns_srv <dns_srv_name>      # 删除
ql_cmd nf start dns_srv <dns_srv_name>    # 开启
ql_cmd nf stop dns_srv <dns_srv_name>     # 停止
ql_cmd nf get dns_srv <dns_srv_name>      # 获取状态（见表 11）
```

**表 11：DNS 服务状态参数**

| 参数 | 类型 | 描述 |
|---|---|---|
| `name` | 字符串 | DNS 服务名称。长度范围：1~32 字节。 |
| `start` | 整型 | 服务是否开启。`0`=未开启；`1`=已开启。 |
| `status` | 字符串 | DNS 服务状态。`none`=服务未启动；`active`=服务正在运行；`error`=发生内部错误。 |

#### 3.6.3 LAN 管理示例（第 26 页）

LAN 设备通过 USB 和 ETH 接入到模块创建的局域网，局域网网段为 192.168.225.0/24，LAN 设备可通过 DHCP 方式获取 IP 地址，并能 PING 通网关。完整 QL_NF 命令示例：
```
add interface intf_usb type=usb_dev
add interface intf_lan type=bridge ifname=br0
set interface intf_lan proto=static conn=auto ip=192.168.225.1 netmask=255.255.255.0
set interface intf_lan addif=intf_usb
add dhcp_srv dhcp_lan intf=intf_lan
set dhcp_srv dhcp_lan ip_start=192.168.225.100 ip_end=192.168.225.200 netmask=255.255.255.0 gateway=192.168.225.1 dnsp=192.168.225.1 lease_time=86400
start dhcp_srv dhcp_lan
add dns_srv dns_lan
start dns_srv dns_lan
```

> 解读：这是一个完整的 LAN 配网序列——桥接接口 + 静态网关 IP + DHCP 服务（分配 .100~.200）+ DNS 服务。DHCP 的 dnsp 指向网关 192.168.225.1，由模块上的 DNS 服务（dns_lan）负责实际解析。

#### 3.6.4 LAN 主机管理（第 27 页）

QL_NF 支持查询接入 QL_NF 管理的局域网的主机。

**命令格式**：
```
get lanhost all
```

**表 12：LAN 主机信息**

| 参数 | 类型 | 描述 |
|---|---|---|
| `ip` | IP 地址格式字符串 | LAN 主机的 IP 地址 |
| `mac` | MAC 地址格式字符串 | LAN 主机的 MAC 地址 |
| `name` | 字符串 | LAN 主机名称。根据 DHCP 请求报文中的字段获取，若主机使用静态 IP 地址方式或者 DHCP 请求报文中没有包含名称信息，则该字段为空。 |
| `intf` | 字符串 | LAN 主机接入的局域网对应的 NF_INTF |
| `ifname` | 字符串 | LAN 主机接入的 Linux 接口名称 |
| `uptime` | 整型 | 在线时间。单位：秒。 |

SDK 中同时提供 API 获取 LAN 主机信息，详情参考头文件 `ql-sysroots/usr/include/ql-sdk/ql_lanhost.h`。

---

### 3.7 静态 ARP 规则管理（第 27–28 页）

QL_NF 支持创建、删除和查询静态 ARP 规则。

**命令格式**：
```
<action> interface <arp_rule_name> <param_list>
```
> 注：原文命令格式写的是 `<action> interface <arp_rule_name>`，但根据 nf_target 表，ARP 管理的 target 应为 `arp`。此处疑为原文笔误（应为 `<action> arp <arp_rule_name> <param_list>`）。

**参数解释**：
- `<action>`：`add`（添加静态 ARP 规则）、`del`（删除静态 ARP 规则）、`get`（获取所有静态 ARP 规则）。
- `<arp_rule_name>`：ARP 规则名称，最大长度 32 字节，唯一标识。
- `<param_list>`：参数列表，详见表 13。

**表 13：ARP 规则参数列表**

| 参数 | 类型 | 描述 |
|---|---|---|
| `intf` | 字符串 | ARP 规则基于的 NF_INTF |
| `ip` | IP 地址格式字符串 | IP 地址 |
| `mac` | MAC 地址格式字符串 | MAC 地址 |

> 解读：静态 ARP 把指定 IP 绑定到指定 MAC，配合桥接的 `noarp=1` 使用，可防 ARP 欺骗、固定 LAN 设备的 IP-MAC 映射。

---

### 3.8 数据拨号（第 28 页起）

#### 3.8.1 拨号介绍

无线数据业务的建立过程，即称为**数据拨号（Data Call）**。设备携带 APN 信息向运营商网络侧发起数据拨号，运营商网络侧根据 APN 信息分配 IP 地址，并建立数据链路到相应网络。

**图 4 拨号示意图**：3 路数据拨号（数据拨号 1/2/3）各自携带一份 APN Info（APN 名称、IP Version、Auth Pref、用户名、密码），分别连到 ISP 网络 1/2/3。

运营商会在签约时为 (U)SIM 卡分配一个或多个 APN 信息，常见的是一路公网 APN 信息以及一路或者多路私网 APN 信息。模块携带 APN 信息进行数据拨号，运营商网络侧根据 APN 信息分配公网或者私网的 IP 地址。

**默认承载**：数据拨号成功的前提是模块已注册到运营商网络。LTE 网络注册过程中，需建立**默认承载（default bearer）**。和数据拨号类似，建立默认承载需要传入 APN 信息，若传入的 APN 信息错误，可能导致网络注册失败。运营商分配的 APN 信息中，有一路 APN 信息可用来建立默认承载，详情请咨询运营商。

#### 3.8.2 重连机制（第 29 页）

拨号失败或者拨号断开时，可通过配置数据拨号命令中的 `reconn_mode` 及 `reconn_interval` 参数选择是否进入自动重连模式（详见 3.8.5）。模块支持以下四种自动重连模式：

- **`disable`**：不自动重连，可按需手动重新拨号。
- **`normal`**：根据设置的时间间隔进行重连，建议设置 20 秒以上。频繁拨号可能会被运营商加入黑名单。
- **`mode1`（时间退避模式）**：以时间退避模式重连。例如时间间隔列表为 T1、T2、T3…Tn，重连的时间间隔为 T1、T2、T3…Tn、Tn、Tn…Tn 直至拨号成功。即到达最后一个间隔 Tn 后保持 Tn 不变。若需要再次重连，则间隔时间从 T1 开始。
- **`mode2`（循环时间退避模式）**：以循环时间退避模式重连。例如时间间隔列表为 T1、T2、T3…Tn，重连的时间间隔为 T1、T2、T3…Tn、T1、T2、T3…Tn、T1、T2、T3…Tn…直至拨号成功。即循环使用整个间隔列表。若需要再次重连，则间隔时间从 T1 开始。

> 解读：`mode1` 退避到最大值后保持（适合长时间稳态重试），`mode2` 循环整个列表（适合周期性重试）。两者都用 `reconn_interval` 数组定义间隔序列（最多 20 个，见表 16）。

#### 3.8.3 设置默认承载 APN（第 29–30 页）

**命令格式**：
```
set networks attach_apn <param_list>
```

**参数解释**：`<param_list>` 为 APN 参数列表，详见表 14。

**表 14：APN 参数列表**

| 参数 | 类型 | 描述 |
|---|---|---|
| `apn_name` | 字符串 | APN 名称。运营商根据此参数分配 IP 地址信息；长度范围：1~150 字节。 |
| `ip_ver` | 整型 | IP 类型：`4`=IPv4；`6`=IPv6；`46`=IPv4v6。 |
| `auth_pref` | 字符串 | 认证方式。部分运营商网络会开启认证，需要传入认证方式，如无特殊指明，可不设置该参数或者设置为 `none`。`none`=无认证（默认值）；`pap`=PAP 认证；`chap`=CHAP 认证；`pap_chap`=PAP 和 CHAP 认证。 |
| `username` | 字符串 | 用户名。长度范围：1~150 字节。 |
| `password` | 字符串 | 密码。长度范围：1~150 字节。 |
| `sim_id` | 整型 | 若模块不支持 DSDA，无需设置该参数；若模块支持 DSDA，则使用该参数指定 (U)SIM 卡槽。`1`=卡槽 1；`2`=卡槽 2。 |

#### 3.8.4 数据拨号状态（第 30 页）

**表 15：数据拨号状态列表**

| 数据拨号状态 | 描述 |
|---|---|
| `none` | 数据拨号实例不存在。 |
| `created` | 数据拨号实例被创建。 |
| `idle` | 数据拨号实例已经被配置。 |
| `connecting` | 正在进行拨号。 |
| `connected` | 数据拨号成功，获取到 IP 地址等信息。 |
| `disconnected` | 数据拨号失败或者断开。 |
| `deleted` | 数据拨号实例被删除。 |

**图 5 数据拨号状态变迁图（Data Call State Transition）**：
- `none` → `created`：CREATE_REQ
- `created` → `idle`：CONFIG_REQ
- `idle` → `connecting`：START_REQ
- `connecting` → `connected`：Data call succeed
- `connecting` → `disconnected`：Data call failed
- `connected` → `disconnected`：Network disconnect
- `connecting`/`connected` → `idle`：STOP_REQ
- `idle` → `created`：（DELETE_REQ 路径相关）
- `disconnected` → `connecting`：Reconnect enable（重连使能）
- `connected` → `idle`：STOP_REQ
- 各状态（created/idle/connecting/connected/disconnected）→ `deleted`：DELETE_REQ
- `deleted` → `none`：Immediately transition（立即转换）

> 解读：拨号实例生命周期为 none→created→idle→connecting→connected，停止回到 idle，删除进入 deleted 后立即回 none。`disconnected` 在重连使能时可自动回到 connecting。

#### 3.8.5 数据拨号命令（第 31–33 页）

**命令格式**：
```
<action> data_call <call_id> <param_list>
```

**参数解释**：
- `<action>`：`add`（添加一路数据拨号实例）、`del`（删除）、`set`（配置参数）、`get`（获取状态参数表 17 或配置参数表 16）、`start`（开始拨号）、`stop`（停止拨号）。
- `<call_id>`：数据拨号的唯一标识；整型，取值范围 1~INT_MAX。
- `<param_list>`：配置参数列表。

**示例**：
```
ql_cmd nf add data_call <call_id>                        # 创建
ql_cmd nf del data_call <call_id>                        # 删除
ql_cmd nf set data_call <call_id> <param_list>           # 设置配置参数（见表 16）
ql_cmd nf start data_call <call_id>                      # 开始拨号
ql_cmd nf stop data_call <call_id>                       # 停止拨号
ql_cmd nf get data_call <call_id> class=info             # 获取状态（见表 17；call_id=all 获取所有）
ql_cmd nf get data_call <call_id> class=config           # 获取配置（见表 16；call_id=all 获取所有）
```

**表 16：数据拨号配置参数**

| 参数 | 类型 | 描述 |
|---|---|---|
| `call_id` | 整型 | 用户指定，唯一标识数据拨号实例。范围：1~INT MAX。 |
| `apn_name` | 字符串 | APN 名称；运营商网络根据此参数分配 IP 地址信息，长度范围：1~50 字节。 |
| `ip_ver` | 整数 | IP 类型：`4`=IPv4；`6`=IPv6；`46`=IPv4v6。 |
| `auth_pref` | 字符串 | 认证方式。`none`=无认证（默认值）；`pap`=PAP 认证；`chap`=CHAP 认证；`pap_chap`=PAP 和 CHAP 认证。 |
| `username` | 字符串 | 用户名。长度范围：1~150 字节。 |
| `password` | 字符串 | 密码。长度范围：1~150 字节。 |
| `reconn_mode` | 字符串 | 自动重连模式（详见 3.8.2）：`disable`、`normal`、`mode1`、`mode2`。 |
| `reconn_interval` | 整型数组 | 自动重连时间间隔列表。单位：秒，数组最大长度为 20。 |
| `sim_id` | 整型 | 若模块不支持 DSDA，无需设置；若支持 DSDA，则指定 (U)SIM 卡槽。`1`=卡槽 1；`2`=卡槽 2。 |

> 注：`apn_name` 在表 14（默认承载）中长度范围为 1~150 字节，在表 16（数据拨号）中为 1~50 字节。两表对同名参数给出的长度上限不同，使用时以各自场景的限制为准。

**表 17：数据拨号状态参数**

| 参数 | 类型 | 描述 |
|---|---|---|
| `status` | 字符串 | 拨号状态（详见 3.8.4）：`none`/`create`/`idle`/`connecting`/`connected`/`disconnected`/`delete`。 |
| `ipaddr` | IP 地址格式字符串 | 拨号成功，获取的 IP 地址 |
| `netmask` | IP 地址格式字符串 | 拨号成功，获取的子网掩码 |
| `gateway` | IP 地址格式字符串 | 拨号成功，获取的网关 |
| `dnsp` | IP 地址格式字符串 | 拨号成功，获取的首选 DNS |
| `dnss` | IP 地址格式字符串 | 拨号成功，获取的备选 DNS |
| `last_ecode`（暂不支持） | 整型 | 上次拨号失败或者断开的内部错误码。错误码详见头文件 `ql-sysroots/usr/include/ril.h` 中的 `RIL_LastCallFailCause` 结构体。 |

#### 3.8.6 拨号示例（第 34 页）

若用户的 (U)SIM 卡和运营商签约了两路 APN，公网 APN 名称为 `apnpublic`，私网 APN 名称为 `apnprivate`，其中公网 APN 可以建立默认承载：
```
ql_cmd nf set networks attach_apn apn_name=apnpublic
ql_cmd nf add data_call 1
ql_cmd nf set data_call 1 apn_name=apnpublic
ql_cmd nf start data_call 1
ql_cmd nf add data_call 2
ql_cmd nf set data_call 2 apn_name=apnprivate
ql_cmd nf start data_call 2
```

> 解读：先用公网 APN 设置默认承载（保证网络注册成功），再分别拉起公网（call 1）和私网（call 2）两路数据拨号。每路拨号对应一个 call_id，后续可被 `data_call` 类型的 NF_INTF 引用。

#### 3.8.7 注意事项（第 34 页）

- 用户可通过 API 函数管理数据拨号，使用 QL_NF 管理转发规则。有关 API 函数的详情请参考文档 [3]。
- 数据拨号 API 函数使用 `call_id` 唯一标识数据拨号实例。在 QL_NF 中创建数据拨号 NF_INTF，也是使用 `call_id` 关联数据拨号的实例。创建 NF_INTF 时，无需数据拨号实例存在；创建数据拨号实例后，会自动同步数据拨号 NF_INTF 的状态。
- 在开发过程中，可以将数据拨号的 NF_INTF 以及相关的转发规则预设在 `/etc/ql_nf_preload.conf` 配置文件中，然后在应用程序中使用数据拨号的 API 函数管理数据拨号。

---

### 3.9 转发策略（第 34 页起）

#### 3.9.1 转发策略介绍

转发策略决定 **LAN 接口和 WAN 接口之间的选路、本地程序和 WAN 接口之间的选路**。QL_NF 使用 **ACL** 管理转发策略。

**ACL（Access Control Lists，访问控制列表）**用于设置 LAN 接口和 WAN 接口之间的选路、本地程序和 WAN 接口之间的选路。用户只需指定**数据源（LAN NF_INTF 或本地程序）、数据类型以及数据目的（WAN NF_INTF）**，即可完成规则的配置，无需对 Linux 网络规则进行配置。

为了管理本地程序的网络访问，QL_NF 使用 `intf_local` NF_INTF 表示所有模块上运行的本地程序。

**图 6 ACL 工作示意图**（第 35 页）：
- 上方有两台服务器：服务器 1（URL: testserve.com）、服务器 1（IP:2.2.2.2 PORT:9090），分别经 ISP NET1 / ISP NET2。
- 模块内有两路 WAN NF_INTF：`intf_net1`（Data Call 1）、`intf_net2`（Data Call 2）。
- 四条 ACL 规则：
  - **ACL1**：`sintf=intf_eth url=testsever.com dintf=intf_net1`（来自 eth、访问 testserve.com 的流量走 net1）
  - **ACL2**：`sintf=intf_eth dintf=intf_net2`（来自 eth 的其余流量走 net2）
  - **ACL3**：`sintf=intf_usb dintf=intf_net2`（来自 usb 的流量走 net2）
  - **ACL4**：`sintf=intf_usb dip=2.2.2.2 dport=9090 dintf=intf_net1`（来自 usb、目的 2.2.2.2:9090 的流量走 net1）
- LAN NF_INTF：`intf_eth`（eth0）接 LAN 设备 1（APP1 连 testserver.com、APP2 连其他服务器）；`intf_usb`（ecm0）接 LAN 设备 2（APP1 连其他服务器、APP2 连 2.2.2.2:9090）。

> 解读：ACL 实现策略路由——按源接口 + 数据特征（URL/目的 IP/端口）选择不同的 WAN 出口（多 WAN 分流）。这是双卡/双拨号场景下的核心分流机制。

#### 3.9.2 ACL 命令（第 35–36 页）

**命令格式**：
```
<action> acl <acl_name> sintf=<source_interface_name> <data_flow_type_params> dintf=<destination_interface_name>
```

**参数解释**：
- `<action>`：`add`（添加 ACL 规则）、`del`（删除）、`get`（获取 ACL 规则配置）。
- `<acl_name>`：ACL 规则名称，唯一标识；字符串类型；1~32 字节长度。
- `<source_interface_name>`：LAN NF_INTF 名称；可设置为 `intf_local`，表示所有本地服务和本地程序。
- `<data_flow_type_params>`：数据流标识参数列表，详见表 18。
- `<destination_interface_name>`：WAN NF_INTF 名称，数据转发的出口。

**示例**：
```
# 添加 ACL 规则：
add acl <acl_name> sintf=<source_interface_name> <data_flow_type_params> dintf=<destination_interface_name>

# 删除 ACL 规则（acl_name=all 表示删除所有 ACL 规则）：
del acl <acl_name>

# 查看所有 ACL 规则：
get acl all
```

**表 18：数据流标识参数**

| 参数 | 类型 | 描述 |
|---|---|---|
| `proto` | 字符串 | 配置数据流类型。**本地程序不支持该参数**。`all`=所有类型（包括 TCP 和 UDP）；`tcp`=TCP 流；`udp`=UDP 流。 |
| `url` | 字符串 | URL 绑定。绑定 URL 到对应的 WAN 接口，长度范围 1~128 字节。**此参数不能同 `proto`、`sport`、`dip` 或 `dport` 共用**。 |
| `smac` | MAC 地址格式字符串 | 数据流源 MAC 地址，即 LAN 设备 MAC 地址。**本地程序不支持该参数**。 |
| `sip` | IP 地址格式字符串 | 数据流源 IP 地址，即 LAN 设备 IP 地址。**本地程序不支持该参数**。 |
| `sport` | 整型 | 数据流源端口。范围：1~65535。**本地程序不支持该参数**。 |
| `dip` | IP 地址格式字符串 | 数据流目的 IP 地址。 |
| `dport` | 整型 | 数据流目的端口。范围：1~65535。**本地程序不支持该参数**。 |

> 解读：`url` 与五元组参数（proto/sport/dip/dport）互斥——URL 绑定是基于域名的策略，不能再叠加端口/IP 过滤。`intf_local`（本地程序）只能用 dip/url，不能用源相关（smac/sip/sport）和 proto/dport 参数。

**3.9.2.1 ACL 转发功能（V1.0.1 新增，第 37 页）**：

默认情况下，ACL **只支持 IPv4 转发，不支持 IPv6 转发**。可通过如下命令开启或关闭 IPv4 或 IPv6 的转发功能：
```
set acl enable_ipv4      # 开启 IPv4 转发
set acl disable_ipv4     # 关闭 IPv4 转发
set acl enable_ipv6      # 开启 IPv6 转发
set acl disable_ipv6     # 关闭 IPv6 转发
```

**备注**：
1. **必须在添加 ACL 规则前**开启或关闭 IPv4 或 IPv6 的转发功能，否则会报错。
2. 如需同时开启 IPv4 和 IPv6 转发功能，则添加 ACL 规则时，**无需指定 `sip` 和 `dip` 参数**。

> 解读：这是 V1.0.1 版本相对 V1.0.0 的唯一功能新增。开关 IPv4/IPv6 转发必须先于 ACL 规则添加，是初始化顺序约束。双栈同时开启时不能用 IP 维度过滤（sip/dip）。

---

### 3.10 QL_NF API（第 37 页起）

#### 3.10.1 头文件

QL_NF API 头文件为 `ql_nf.h`，部分公用结构体定义在头文件 `ql_net_common.h`，所述头文件均位于 `ql-sysroots/usr/include/ql-sdk/` 目录下。若无特别说明，本章所涉及头文件均在该目录下。

#### 3.10.2 函数概览（第 38 页）

**表 19：函数概览**

| 函数 | 说明 |
|---|---|
| `ql_nf_init()` | 初始化 QL_NF 服务 |
| `ql_nf_cmd()` | 发送普通格式的 QL_NF 命令，并等待响应 |
| `ql_nf_set_interface_status_ind_cb()` | 设置监控 NF_INTF 状态变化回调函数 |
| `ql_nf_set_service_error_cb()` | 设置服务程序异常通知的回调函数 |
| `ql_nf_deinit()` | 注销 QL_NF 服务 |

**备注**：若无特别说明，本文档所述 QL_NF API 均**不支持并发调用**，并且**不能在相关回调函数中调用以上函数**。

#### 3.10.3 函数详解

**3.10.3.1 `ql_nf_init`（第 38–39 页）** — 初始化 QL_NF 服务。

函数原型：
```c
int ql_nf_init(void);
```
- 参数：无。
- 返回值：
  | 返回值 | 含义 |
  |---|---|
  | `QL_ERR_OK` | 函数执行成功 |
  | `QL_ERR_INVALID_ARG` | 无效参数 |
  | `QL_ERR_UNKNOWN` | 未知错误 |
  | `QL_ERR_SERVICE_NOT_READY` | QL_NF 服务（未）准备就绪，返回此错误码时建议重试。 |
  | 其他值 | 函数执行失败，错误码详见 `ql_type.h`。 |
- 备注：使用其他 QL_NF API 前，必须调用本函数初始化 QL_NF 服务。

**3.10.3.2 `ql_nf_cmd`（第 39 页）** — 发送普通格式的 QL_NF 命令，并等待响应。

函数原型：
```c
int ql_nf_cmd(const char *cmd, char *resp_buf, int resp_len);
```
- 参数：
  - `cmd` [In]：QL_NF 普通格式命令；详见 3.1 章。
  - `resp_buf` [Out]：存放响应的数据缓冲区。QL_NF 命令的响应为字符串格式。
  - `resp_len` [In]：`resp_buf` 缓冲区大小，单位字节。用户根据命令需求设置：获取单个 NF_INTF 信息时缓冲区可设为 256 字节；获取所有 NF_INTF 信息时建议 2048 字节以上。
- 返回值：同 `ql_nf_init`（`QL_ERR_OK`/`QL_ERR_INVALID_ARG`/`QL_ERR_UNKNOWN`/`QL_ERR_SERVICE_NOT_READY`/其他，错误码见 `ql_type.h`）。

**3.10.3.3 `ql_nf_set_interface_status_ind_cb`（第 39 页起）** — 设置 NF_INTF 状态变化监控的回调函数。

函数原型：
```c
int ql_nf_set_interface_status_ind_cb(ql_nf_interface_status_ind_cb_f cb);
```
- 参数：`cb` [In] — 监控 NF_INTF 状态变化的回调函数；详见 3.10.3.3.1。
- 返回值：
  | 返回值 | 含义 |
  |---|---|
  | `QL_ERR_OK` | 函数执行成功 |
  | `QL_ERR_NOT_INIT` | 未初始化 |
  | `QL_ERR_SERVICE_NOT_READY` | QL_NF 服务（未）准备就绪，返回此错误码时建议重试。 |
  | `QL_ERR_INVALID_ARG` | 无效参数 |
  | 其他值 | 函数执行失败，错误码详见 `ql_type.h`。 |

**3.10.3.3.1 `ql_nf_interface_status_ind_cb_f`（第 40 页）** — 监控 NF_INTF 状态变化的回调函数类型。

函数原型：
```c
typedef void (*ql_nf_interface_status_ind_cb_f)(ql_nf_interface_status_t *p_msg);
```
- 参数：`p_msg` [Out] — NF_INTF 的状态信息；详见 3.10.3.3.2。
- 返回值：无。

**3.10.3.3.2 `ql_nf_interface_status_t`（第 40–41 页）** — NF_INTF 状态结构体。

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

| 类型 | 参数 | 描述 |
|---|---|---|
| `char` | `name` | NF_INTF 名称 |
| `char` | `ifname` | Linux 网络接口名称 |
| `char` | `pre_status` | 前次状态；详见表 20。 |
| `char` | `status` | 当前状态；详见表 20。 |
| `uint8_t` | `has_addr` | 有无 IPv4 地址。`0`=无 IPv4 地址；非 0=有 IPv4 地址。 |
| `ql_net_addr_t` | `addr` | IPv4 地址信息；详见 3.10.3.3.3。 |
| `uint8_t` | `has_addr6` | 有无 IPv6 地址。`0`=无 IPv6 地址；非 0=有 IPv6 地址。 |
| `ql_net_addr6_t` | `addr6` | IPv6 地址信息；详见 3.10.3.3.4。 |

**表 20：接口状态**（回调与结构体中 pre_status/status 的枚举值）

| 接口状态 | 描述 |
|---|---|
| `NF_INTF_STATUS_NONE` | 初始状态或空闲状态 |
| `NF_INTF_STATUS_UNCONFIG` | 未配置 |
| `NF_INTF_STATUS_READY` | 就绪状态 |
| `NF_INTF_STATUS_CONNECTING` | 连接中 |
| `NF_INTF_STATUS_CONNECTED` | 已连接 |
| `NF_INTF_STATUS_DISCONNECTED` | 断开连接 |
| `NF_INTF_STATUS_ERROR` | 出错 |
| `NF_INTF_STATUS_BINDER` | 绑定到其他接口 |

> 解读：这里给出了 8 种接口状态枚举（与 3.2.3 表 5 对应，且补齐了 `ERROR` 态），印证了前文"8 种状态"的说法——表 5 漏列的正是 `ERROR`。

**3.10.3.3.3 `ql_net_addr_t`（第 41–42 页）** — IPv4 地址结构体。

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

| 类型 | 参数 | 描述 |
|---|---|---|
| `char` | `addr` | IPv4 地址。类型：字符串。 |
| `char` | `netmask` | 子网掩码 |
| `uint8_t` | `subnet_bits` | 子网的网络位的长度 |
| `char` | `gateway` | IPv4 网关地址 |
| `char` | `dnsp` | 主 DNS 服务器地址 |
| `char` | `dnss` | 备用 DNS 服务器地址 |

**3.10.3.3.4 `ql_net_addr6_t`（第 42–43 页）** — IPv6 地址结构体。

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

| 类型 | 参数 | 描述 |
|---|---|---|
| `char` | `addr` | IPv6 地址。类型：字符串。 |
| `char` | `prefix` | IPv6 地址前缀 |
| `uint8_t`（原文标注，结构体定义为 `int32_t`） | `prefix_bits` | IPv6 地址前缀长度 |
| `char` | `gateway` | IPv6 网关地址 |
| `char` | `dnsp` | 主 DNS 服务器地址 |
| `char` | `dnss` | 备用 DNS 服务器地址 |

> 注：`prefix_bits` 在结构体定义中是 `int32_t`，参数表中标为 `uint8_t`，存在类型描述不一致，以结构体定义 `int32_t` 为准。

**3.10.3.4 `ql_nf_set_service_error_cb`（第 43 页）** — 设置 QL_NF 服务程序异常上报的回调函数。

函数原型：
```c
int ql_nf_set_service_error_cb(ql_nf_service_error_cb_f cb);
```
- 参数：`cb` [In] — 上报 QL_NF 服务程序异常的回调函数；详见 3.10.3.4.1。
- 返回值：`QL_ERR_OK`=成功；其他值=失败（错误码见 `ql_type.h`）。

**3.10.3.4.1 `ql_nf_service_error_cb_f`（第 43 页）** — 上报 QL_NF 服务程序异常的回调函数类型。

函数原型：
```c
typedef void (*ql_nf_service_error_cb_f)(int error);
```
- 参数：`error` [Out] — 错误码。整型，用户自定义。
- 返回值：无。

**3.10.3.5 `ql_nf_deinit`（第 44 页）** — 注销 QL_NF 服务。

函数原型：
```c
int ql_nf_deinit(void);
```
- 参数：无。
- 返回值：`QL_ERR_OK`=成功；其他值=失败（错误码见 `ql_type.h`）。

---

### 3.11 注意事项（第 44–45 页）

#### 3.11.1 MAC 地址综述

- 每个 Linux 网络接口都有一个 MAC 地址，MAC 地址长度为 **48 位（6 个字节）**，通常表示为 12 个 16 进制数，每 2 个 16 进制数之间用冒号隔开，如 `08:22:22:22:22:22`。
- MAC 地址最高字节（MSB）的第二位（原文括注"LSB"）表示该地址为**全局地址还是本地地址**：如果为 1，表示是本地 MAC 地址；如果为 0，表示是全局 MAC 地址。比如 `08:22:22:22:22:22` 是全局地址，`02:22:22:22:22:22` 是本地 MAC 地址。
- 对于全局地址，其前 3 个字节表示 **OUI（Organizationally Unique Identifier）**，是 IEEE 的注册管理机构分配给不同厂家的代码，用以区分不同的厂家；后 3 个字节由厂家自行分配。
- **全局地址**是全球唯一地址，以保证设备的 MAC 地址不会产生冲突，**WAN 接口的 MAC 地址应该为全局地址**。由于本地 MAC 地址不能保证 MAC 地址的唯一性，所以只用于 LAN 接口。局域网环境下，都是 LAN 设备的 WAN 接口以及接入网关设备的 LAN 接口，可保证局域网 MAC 地址不会发生冲突。
- **模块的 Wi-Fi 网络接口**，烧录了移远通信 OUI 的全局 MAC 地址。其他网络接口，包括 ETH 网络接口和 USB 网络接口，均使用 Linux 随机生成的本地 MAC 地址；作为 LAN 接口时，不会出现 MAC 地址冲突，但作为 WAN 接口时，无法避免 MAC 地址冲突（随机 MAC 地址冲突的概率极小）。在无法确定是否会产生 MAC 地址冲突的场景下，**推荐用户在 ETH 和 USB 网络接口作为 WAN 接口时，使用用户自行向 IEEE 购买的 MAC 地址，并配置 QL_INTF 的 `mac` 参数**。

> 解读：核心结论——Wi-Fi 接口有移远 OUI 的全局 MAC（可放心做 WAN），ETH/USB 接口默认是 Linux 随机本地 MAC（做 WAN 有冲突风险，应通过 NF_INTF 的 `mac` 配置参数手动设置购买的全局 MAC）。

#### 3.11.2 禁用 QL_NF（第 44–45 页）

由于 QL_NF 框架无法满足某些特殊需求，当用户有特殊需求时可禁用 QL_NF。QuecOpen SDK 中提供了较为底层的接口（比如数据拨号接口，详见文档 [3]），结合 Linux 的网络管理，并**通过清空 `/etc/ql_nf_preload.conf` 文件禁用 QL_NF**，用户可开发所需要的网络管理程序。

> 解读：禁用方式就是清空预加载配置文件 `/etc/ql_nf_preload.conf`，然后直接用 SDK 底层数据拨号 API + 原生 Linux 网络工具自行管理网络。

---

## 4 场景案例（第 46 页起）

本章介绍如何在各个典型应用场景下配置 QL_NF 规则。示例中的域名、IP 地址或者 MAC 地址等信息并非真实存在，只为举例说明。

**推荐的 QL_NF 规则配置步骤**：
- **步骤一**：若使用数据拨号，请设置默认承载的 APN 信息（3.8.3），也可在应用程序中使用数据拨号 API 设置（文档 [3]）。
- **步骤二**：配置 LAN 相关的 NF_INTF（3.2）。
- **步骤三**：启动 DHCP 和 DNS 服务（3.6.1 和 3.6.2）。
- **步骤四**：配置 WAN 相关的 NF_INTF（3.2）。如果 WAN 接口是数据拨号的接口，也可只创建数据拨号的 NF_INTF，由应用程序调用数据拨号的 API 管理数据拨号。
- **步骤五**：配置 ACL 规则（3.9.2）。

完成调试后，再将规则固化到 `/etc/ql_nf_preload.conf` 文件。各个场景的数据转发测试，可以在模块上使用 **`tcpdump`** 工具，通过抓取各个网络接口收到的数据包，验证分析转发规则是否生效。

**备注**：在开发过程中，可能需要做前期验证工作，在场景三（4.3）中举例说明了如何使用装有 Ubuntu 系统的 PC 进行模拟测试。

---

### 4.1 场景一（第 46–50 页）：单 APN、本地程序与 LAN 设备共用数据拨号

#### 4.1.1 场景需求

**配置环境**：
1. (U)SIM 卡：签约一路 APN，名称为 `apn_name`，该路 APN 作为默认承载。
2. WAN 接口：使用签约的 APN 进行数据拨号。
3. LAN 接口：Wi-Fi AP 模式对应的接口、ETH 对应的接口和 USB ECM 对应的接口。
4. 局域网：使用 Wi-Fi、ECM 和 ETH 接入网络的设备，位于一个局域网内，网段为 `192.168.225.0/24`，网关地址为 `192.168.225.1`。

**转发策略**：
1. 模块本地程序使用数据拨号访问网络。
2. LAN 设备使用数据拨号访问网络。

**图 7 Linux 网络拓扑图**：APN Net ← APN 数据拨号 ← 应用程序 / 桥接0（wlan0、eth0、ecm0）← 各 LAN 设备。

#### 4.1.2 QL_NF 组网（第 47–48 页）

NF_INTF 定义：

| 名称 | NF_INTF 类型 | Linux 接口名 | 描述 | 地址 | WAN/LAN | DHCP? |
|---|---|---|---|---|---|---|
| `intf_lan` | bridge | bridge0 | 管理局域网 192.168.255.0/24（原文如此，实际网段应为 192.168.225.0/24） | 192.168.225.1/24 | LAN | 是 |
| `intf_eth` | com_dev | eth0 | ETH 接口对应的 NF_INTF | / | LAN | / |
| `intf_wifi_ap0` | wifi_ap | wlan0 | Wi-Fi AP 模式对应的 NF_INTF | / | LAN | / |
| `intf_usb` | usb_dev | ecm0 | USB ECM 接口对应的 NF_INTF | / | LAN | / |
| `intf_data` | data_call | 动态信息，根据拨号结果返回 | 基于 APN 的数据拨号的 NF_INTF | 数据拨号成功后的信息 | WAN | / |

转发规则（ACL）：

| ACL 规则名称 | 数据流来源 NF_INTF | 数据流匹配规则 | 数据流目的 NF_INTF | 描述 |
|---|---|---|---|---|
| `acl_app_apn` | `intf_local` | 所有数据流 | `intf_data` | 运行在模块上的应用程序，使用基于 APN 的数据拨号访问网络 |
| `acl_lan_apn` | `intf_lan` | 所有数据流 | `intf_data` | 接入局域网的 LAN 设备，通过基于 APN 的数据拨号访问网络 |

**图 8 QL_NF 网络拓扑图（场景一）**：两条数据路径（acl_app_apn、acl_lan_apn）都汇聚到 `intf_data` 出口。

#### 4.1.3 规则配置（第 49 页）

```
# 步骤一：设置默认承载 APN 信息为 (U)SIM 卡签约 APN 信息
set networks attach_apn apn_name=apn_name ip_ver=4

# 步骤二：创建 LAN NF_INTF，创建桥接类型的 NF_INTF 用于管理局域网，并将 LAN NF_INTF 加入桥接 NF_INTF
add interface intf_usb type=usb_dev
add interface intf_eth type=com_dev ifname=eth0
add interface intf_wifi_ap0 type=wifi_ap index=0
add interface intf_lan type=bridge ifname=br0
set interface intf_lan proto=static conn=auto ip=192.168.225.1 netmask=255.255.255.0
set interface intf_lan addif=intf_usb
set interface intf_lan addif=intf_eth
set interface intf_lan addif=intf_wifi_ap0

# 步骤三：启动 DNS 代理服务和局域网的 DHCP 服务，DHCP 服务需要绑定对应的桥接 NF_INTF
add dhcp_srv dhcp_lan intf=intf_lan
set dhcp_srv dhcp_lan ip_start=192.168.225.100 ip_end=192.168.225.200 netmask=255.255.255.0 gateway=192.168.225.1 dnsp=192.168.225.1 lease_time=86400
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

# 步骤六：Wi-Fi 相关的配置，详见文档 [2]。
```

> 解读：`reconn_mode=mode1 reconn_interval=10,20,40,80,120` 是退避重连——失败间隔依次 10s/20s/40s/80s/120s，之后保持 120s 重试。这是本文档反复使用的标准重连配置。

#### 4.1.4 场景测试（第 50 页）

将上述所有命令拷贝到 SDK 的 `ql-ol-rootfs/etc/ql_nf_preload.conf` 文件，执行 **make rootfs** 编译生成镜像烧录到模块。等待模块网络注册成功，运行 **`ql_cmd nf get interface all`**，桥接 NF_INTF 应为 `connected` 状态，等待 WAN NF_INTF 状态变为 `connected`。随后验证：
1. 模块上的应用程序，可以访问网络。
2. LAN 设备可以获取 IP 地址并访问网络。

---

### 4.2 场景二（第 50–55 页）：双 APN + 基于 URL/IP 的本地程序策略分流

#### 4.2.1 场景需求

**配置环境**：
1. (U)SIM 卡：签约两路 APN，APN1 名称 `apn_name1`、APN2 名称 `apn_name2`，其中 APN1 作为默认承载 APN。
2. WAN 接口：基于 APN1 和 APN2 的数据拨号接口。
3. LAN 接口：Wi-Fi AP 模式接口、ETH 接口和 USB ECM 接口。
4. 局域网：使用 Wi-Fi、USB ECM 和 ETH 接入网络的设备位于一个局域网内，网段 192.168.225.0/24，网关 192.168.225.1。

**转发策略**：
1. 模块本地程序 APP1 通过基于 APN2 的数据拨号以域名 `www.test.com` 方式访问网络服务。
2. 模块本地程序 APP2 通过基于 APN2 的数据拨号以 IP 地址 `2.2.2.2` 方式访问网络服务。
3. 模块其他本地程序通过基于 APN1 的数据拨号访问网络。
4. LAN 设备通过基于 APN1 的数据拨号访问网络。

**图 9 Linux 网络拓扑图（场景二）**：APN1 Net / APN2 Net 两条数据拨号；APP1 连服务器1（URL: www.test.com）走 APN2，APP2 连服务器2（IP: 2.2.2.2）走 APN2，其他程序和 LAN 设备走 APN1。

#### 4.2.2 QL_NF 组网（第 51–52 页）

NF_INTF 定义：

| NF_INTF 名称 | 类型 | Linux 接口名 | 描述 | 地址 | WAN/LAN | DHCP? |
|---|---|---|---|---|---|---|
| `intf_lan` | bridge | bridge0 | 管理局域网 192.168.255.0/24（原文如此，实际为 192.168.225.0/24） | 192.168.225.1/24 | LAN | 是 |
| `intf_eth` | com_dev | eth0 | ETH 接口的 NF_INTF | - | LAN | - |
| `intf_wifi_ap0` | wifi_ap | wlan0 | Wi-Fi AP 模式的 NF_INTF | - | LAN | - |
| `intf_usb` | usb_dev | ecm0 | USB ECM 的 NF_INTF | - | LAN | - |
| `intf_data_apn1` | data_call | 数据拨号成功后的信息 | 基于 APN1 的数据拨号的 NF_INTF | 数据拨号成功后的信息 | WAN | - |
| `intf_data_apn2` | data_call | 数据拨号成功后的信息 | 基于 APN2 的数据拨号的 NF_INTF | 数据拨号成功后的信息 | WAN | - |

转发规则（ACL）：

| ACL 规则名称 | 数据流来源 NF_INTF | 数据流匹配规则 | 数据流目的 NF_INTF | 描述 |
|---|---|---|---|---|
| `acl_app_server1` | `intf_local` | 域名为 www.test.com | `intf_data_apn2` | 模块上应用程序，通过基于 APN2 的数据拨号以域名访问网络服务 |
| `acl_app_server2` | `intf_local` | 目的 IP 地址为 2.2.2.2 | `intf_data_apn2` | 模块上应用程序，通过基于 APN2 的数据拨号以 IP 访问网络服务 |
| `acl_app_apn1` | `intf_local` | 所有数据流 | `intf_data_apn1` | 模块上应用程序，通过基于 APN1 的数据拨号访问网络 |
| `acl_lan_apn1` | `intf_lan` | 所有数据流 | `intf_data_apn1` | LAN 设备通过基于 APN1 的数据拨号访问网络 |

> 解读：ACL 规则**有优先级/顺序匹配**——更具体的规则（URL=www.test.com、dip=2.2.2.2）必须排在 `所有数据流`（acl_app_apn1）之前，否则会被通配规则先匹配走。本地程序的细分流量先匹配 server1/server2，剩余走 apn1。

**图 10 QL_NF 网络拓扑图（场景二）**：四条数据路径分别对应四条 ACL。

#### 4.2.3 规则配置（第 53–54 页）

```
# 步骤一：设置默认承载 APN 信息为 (U)SIM 卡签约 APN1 信息
set networks attach_apn apn_name=testapn1 ip_ver=4

# 步骤二：创建 LAN NF_INTF + 桥接
add interface intf_usb type=usb_dev
add interface intf_eth type=com_dev ifname=eth0
add interface intf_wifi_ap0 type=wifi_ap index=0
add interface intf_lan type=bridge ifname=br0
set interface intf_lan proto=static conn=auto ip=192.168.225.1 netmask=255.255.255.0
set interface intf_lan addif=intf_usb
set interface intf_lan addif=intf_eth
set interface intf_lan addif=intf_wifi_ap0

# 步骤三：启动 DNS 代理服务和局域网 DHCP 服务
add dhcp_srv dhcp_lan intf=intf_lan
set dhcp_srv dhcp_lan ip_start=192.168.225.100 ip_end=192.168.225.200 netmask=255.255.255.0 gateway=192.168.225.1 dnsp=192.168.225.1 lease_time=86400
start dhcp_srv dhcp_lan
add dns_srv dns_lan
start dns_srv dns_lan

# 步骤四：创建 WAN NF_INTF（两路数据拨号）
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

#### 4.2.4 场景测试（第 54 页）

固化到 `ql-ol-rootfs/etc/ql_nf_preload.conf`，make rootfs 烧录。等待网络注册，`ql_cmd nf get interface all` 桥接 NF_INTF 应为 connected，等 WAN 接口 connected。验证：
1. 在模块上访问 www.test.com，通过 `intf_data_apn2` 收发数据。
2. 在模块上访问 2.2.2.2，通过 `intf_data_apn2` 收发数据。
3. 在模块上，通过 `intf_data_apn1` 访问网络。
4. LAN 设备上，通过 `intf_data_apn1` 访问网络。

---

### 4.3 场景三（第 55 页起）：VLAN 隔离 + 双 APN 分流（含真机/Ubuntu PC 测试）

#### 4.3.1 场景需求（第 55 页）

**配置环境**：
1. (U)SIM 卡：签约两路 APN，APN1 名称 `testapn1`（默认承载）、APN2 名称 `testapn2`。
2. WAN 接口：基于 APN1 和 APN2 的数据拨号接口。
3. LAN 接口：Wi-Fi AP 接口、基于 ETH 且 VLAN ID 分别为 100 和 200 的接口。
4. 局域网：
   - 使用 ETH VLAN ID 100 和 Wi-Fi 接入的设备位于一个局域网，网段 `192.168.100.0/24`。
   - 使用 ETH VLAN ID 200 接入的设备位于一个局域网，网段 `192.168.200.0/24`。

**转发策略**：
1. 模块本地程序使用基于 APN2 的数据拨号访问域名 www.test.com 的服务器。
2. 模块本地程序使用基于 APN1 的数据拨号访问其他网络服务。
3. 通过 ETH VLAN ID 100 接入的设备使用基于 APN1 的数据拨号访问网络。
4. 通过 ETH VLAN ID 200 接入的设备使用基于 APN2 的数据拨号访问网络。
5. 通过 Wi-Fi 接入的设备，通过基于 APN1 的数据拨号访问网络。

**图 11 Linux 网络拓扑图（场景三）**：bridge100（wlan0、eth0.100）走 APN1；bridge200（eth0.200）走 APN2；本地程序访问 www.test.com 走 APN2。

#### 4.3.2 QL_NF 组网（第 56–58 页）

NF_INTF 定义：

| NF_INTF 名称 | 类型 | Linux 接口名 | 描述 | 地址 | WAN/LAN | DHCP? |
|---|---|---|---|---|---|---|
| `intf_lan100` | bridge | bridge100 | 管理局域网 192.168.100.0/24 | 192.168.100.1/24 | LAN | 是 |
| `intf_lan100`（原文重名，实际应为 intf_lan200） | bridge | bridge200 | 管理局域网 192.168.200.0/24 | 192.168.200.1/24 | LAN | 是 |
| `intf_eth` | com_dev | eth0 | ETH 接口对应的 NF_INTF | - | LAN | - |
| `intf_100` | vdev | eth0.100 | 基于 ETH 接口的 VLAN ID 为 100 的 NF_INTF | - | LAN | - |
| `intf_200` | vdev | eth0.200 | 基于 ETH 接口的 VLAN ID 为 200 的 NF_INTF | - | LAN | - |
| `intf_wifi_ap0` | wifi_ap | wlan0 | Wi-Fi AP 模式对应的 NF_INTF | - | LAN | - |
| `intf_data_apn1` | data_call | 数据拨号成功后的信息 | 基于 APN1 的数据拨号的 NF_INTF | 数据拨号成功后的信息 | WAN | - |
| `intf_data_apn2` | data_call | 数据拨号成功后的信息 | 基于 APN2 的数据拨号的 NF_INTF | 数据拨号成功后的信息 | WAN | - |

> 注：表中两个 bridge 的"名称"列都写成 `intf_lan100`，但 Linux 接口名分别为 bridge100/bridge200、地址不同（.100/.200），且后续规则配置中使用的是 `intf_lan100` 和 `intf_lan200`。第二行名称应为 `intf_lan200`，属原文表格笔误。

转发规则（ACL）：

| ACL 规则名称 | 数据流来源 NF_INTF | 数据流匹配规则 | 数据流目的 NF_INTF | 描述 |
|---|---|---|---|---|
| `acl_lan100_apn1` | `intf_lan100` | 所有数据流 | `intf_data_apn1` | 192.168.100.0/24 网段局域网，通过基于 APN1 的数据拨号访问网络 |
| `acl_lan200_apn2` | `intf_lan200` | 所有数据流 | `intf_data_apn2` | 192.168.200.0/24 网段局域网，通过基于 APN1（原文如此，按拓扑应为 APN2）的数据拨号访问网络 |
| `acl_app_server` | `intf_local` | 域名为 www.test.com | `intf_data_apn2` | 模块上应用程序，通过基于 APN2 的数据拨号访问 www.test.com |
| `acl_app_apn1` | `intf_lan`（原文如此，应为 intf_local） | 所有数据流 | `intf_data_apn1` | 模块上应用程序，通过基于 APN1 的数据拨号访问网络 |

> 注：`acl_lan200_apn2` 描述中写"通过基于 APN1"与拓扑/出口 intf_data_apn2 矛盾，应为 APN2；`acl_app_apn1` 来源列写 `intf_lan` 与"应用程序"描述及实际命令 `sintf=intf_local` 矛盾，应为 `intf_local`。均为原文表格笔误，以规则配置代码为准。

**图 12 QL_NF 网络拓扑图（场景三）**：四条 ACL 数据路径，intf_lan100/intf_wifi_ap0 走 apn1，intf_lan200 走 apn2，本地程序访问 test.com 走 apn2、其余走 apn1。

#### 4.3.3 规则配置（第 58–59 页）

```
# 步骤一：设置默认承载 APN 为 APN1
set networks attach_apn apn_name=testapn1 ip_ver=4

# 步骤二：创建 LAN NF_INTF（含 VLAN）+ 两个桥接
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

# 步骤三：启动 DNS 代理服务和两个局域网的 DHCP 服务
add dhcp_srv dhcp_lan100 intf=intf_lan100
set dhcp_srv dhcp_lan100 ip_start=192.168.100.100 ip_end=192.168.100.200 netmask=255.255.255.0 gateway=192.168.100.1 dnsp=192.168.100.1 lease_time=86400
start dhcp_srv dhcp_lan100
add dhcp_srv dhcp_lan200 intf=intf_lan200
set dhcp_srv dhcp_lan200 ip_start=192.168.200.100 ip_end=192.168.200.200 netmask=255.255.255.0 gateway=192.168.200.1 dnsp=192.168.200.1 lease_time=86400
start dhcp_srv dhcp_lan200
add dns_srv dns_lan
start dns_srv dns_lan

# 步骤四：创建 WAN NF_INTF（两路数据拨号）
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

> 解读：场景三展示了 VLAN 划分多个隔离局域网（VLAN100/200 各自独立网段 + 独立 DHCP），并把不同 VLAN 绑定到不同 APN 出口（VLAN100→APN1、VLAN200→APN2），Wi-Fi 与 VLAN100 共桥共享 APN1。这是车载多业务隔离的典型组网。

#### 4.3.4 场景测试（第 59–63 页）

本章节使用真实环境对场景三的 QL_NF 规则进行实测验证。以下是测试所用设备清单。

**表 21：设备信息介绍**

| 设备 | 描述 |
|---|---|
| LTE OPEN-EVB | 开发板 |
| (U)SIM 卡 | 普通中国联通 (U)SIM 卡，支持多路拨号 |
| 模块 | 支持中国联通频段的模块，安装到 EVB |
| 88EA1512 | PHY 芯片，安装到 EVB |
| AF50T | Wi-Fi 芯片，安装到 EVB |
| 千兆以太网交换机 | 通过以太网连接 PC1、PC2 和 PHY 芯片 |
| PC1 | 运行 ubuntu1804 版本的笔记本电脑，接入 VLAN ID 100 的 VLAN |
| PC2 | 运行 ubuntu1804 版本的笔记本电脑，接入 VLAN ID 200 的 VLAN |
| 手机 | 支持 Wi-Fi 功能的安卓手机 |

> 解读：测试要点——(U)SIM 卡必须**支持多路拨号**（同一张卡同时建立 APN1/APN2 两路 PDN），这是场景三/四多 APN 组网能跑通的前提；PC1/PC2 分别用网线接到千兆交换机并人为划入 VLAN100/VLAN200，以验证 ACL 按来源 VLAN 分流；手机走 Wi-Fi（wlan0，与 VLAN100 同桥）验证无线设备共享 APN1 出口。

**图 13：EVB 环境**：实物板照片，标注了 4G 模块（AG35-CET）、Wi-Fi 模块、PHY 三个安装位置。

**图 14：测试组网**：EVB 上承载 (U)SIM 卡 + AG35-CET/AG35-EUT 模块，FC30R 侧经无线连接安卓手机，YT8521 侧经千兆以太网交换机连接 PC1、PC2。

##### 4.3.4.1 模块配置（第 61–62 页）

根据测试环境，将场景三里用到的 APN1、APN2 名称改为实际签约的 **3GNET** 和 **testapn**（测试用的是普通联通卡，支持任意 APN 名称拨号；实际使用时替换为 (U)SIM 卡签约的 APN 名）。同时将示例域名 `www.test.com` 改为可访问的真实域名 `www.quectel.com`。修改后完整的 QL_NF 规则如下：

```
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
set dhcp_srv dhcp_lan100 ip_start=192.168.100.100 ip_end=192.168.100.200 netmask=255.255.255.0 gateway=192.168.100.1 dnsp=192.168.100.1 lease_time=86400
start dhcp_srv dhcp_lan100
add dhcp_srv dhcp_lan200 intf=intf_lan200
set dhcp_srv dhcp_lan200 ip_start=192.168.200.100 ip_end=192.168.200.200 netmask=255.255.255.0 gateway=192.168.200.1 dnsp=192.168.200.1 lease_time=86400
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

> 解读：与 4.3.3 的模板相比，仅有三处替换——`attach_apn`/`data_call 1` 的 `3GNET`、`data_call 2` 的 `testapn`、`acl_app_server` 的 `url=quectel.com`。其余 NF_INTF/桥接/DHCP/DNS/ACL 结构完全照搬场景三，说明规则与具体 APN 名解耦，移植到现网只需改 APN 名和目标域名。

**烧录与验证**：将上述 QL_NF 规则写入 SDK 的 `ql-ol-rootfs/etc/ql_nf_preload.conf` 文件，执行 **make rootfs** 编译生成镜像并烧录到模块。等待模块网络注册成功后，用以下命令检查两路数据拨号的 NF_INTF 是否为 **connected** 状态：

```
~ # ql_cmd nf get interface all
name=intf_local      status=connected id=1 type=local   ifname=lo        ip=127.0.0.1     netmask=255.255.255.0 subnet=24
name=intf_eth        status=unconfig  id=2 type=com_dev  ifname=eth0
name=intf_eth100     status=binder    id=3 type=vdev     ifname=eth0.100
name=intf_eth200     status=binder    id=4 type=vdev     ifname=eth0.200
name=intf_wifi_ap0   status=none      id=5 type=wifi_ap
name=intf_lan100     status=connected id=6 type=bridge   ifname=bridge100 ip=192.168.100.1 netmask=255.255.255.0 subnet=24
name=intf_lan200     status=connected id=7 type=bridge   ifname=bridge200 ip=192.168.200.1 netmask=255.255.255.0 subnet=24
name=intf_data_apn1  status=connected id=8 type=data_call ifname=rmnet_data0 ip=10.239.80.103 netmask=255.255.255.252 subnet=30 gat...
name=intf_data_apn2  status=connected id=9 type=data_call ifname=rmnet_data1 ip=10.238.27.155 netmask=255.255.255.248 subnet=29 gat...
```

> 解读：关键看 `intf_data_apn1` / `intf_data_apn2` 两行均为 `status=connected`，且对应不同的 Linux 接口 `rmnet_data0` / `rmnet_data1`，各自拿到不同网段的 WAN IP（10.239.x / 10.238.x）——证明同一张 SIM 卡确实建立了两路独立 PDN。其余状态值含义：`unconfig`（com_dev 未配置 IP）、`binder`（vdev 已绑定到 master）、`none`（wifi_ap 未启用，与 4.3.4.2 "wifi 接口暂未开发" 呼应）、`connected`（bridge/data_call 已就绪）。`ql_cmd nf get interface all` 是排障第一命令，用于确认所有 NF_INTF 的运行态。

##### 4.3.4.2 Wi-Fi 配置（wifi 接口部分目前接口暂未开发）（第 62 页）

在模块上运行 API 测试程序 `ql_sdk_api_test` 配置 Wi-Fi，设置步骤如下：

1. 运行 `ql_sdk_api_test`，选择 Wi-Fi，进入 Wi-Fi API 选项。
2. 选择 `ql_wifi_init` 进行初始化。
3. 选择 `ql_wifi_enable` 使用 Wi-Fi。
4. 选择 `ql_wifi_work_mode_set`，设置模式为 "1-AP0" 模式。
5. 选择 `ql_wifi_ap_ssid_set`，设置 AP0 的 SSID 为 "quectel_test"。
6. 选择 `ql_wifi_ap_auth_set`，设置 AP0 的加密参数：`WPA_WPA2_PSK_BOTH, 0-AUTO, 11111111`。
7. 选择 `ql_wifi_ap_start`，打开 AP0。

手机选择 SSID `quectel_test` 并连接 Wi-Fi，连接后应能正常访问网络。

> 解读：标题括注"wifi 接口部分目前接口暂未开发"——即此处通过 `ql_sdk_api_test` 交互式调用 API 临时拉起 AP0，而非由 QL_NF 配置自动接管 wifi_ap（呼应 4.3.4.1 中 `intf_wifi_ap0 status=none`）。加密参数三元组含义：`WPA_WPA2_PSK_BOTH`（兼容 WPA/WPA2-PSK）、`0-AUTO`（加密算法自动协商）、`11111111`（8 位 PSK 密码）。手机连上 quectel_test 后即归入 bridge100，走 APN1 出口。详细 Wi-Fi 开发见参考文档 [2]。

##### 4.3.4.3 PC1 配置（第 63 页）

断开 PC1 上其他网络连接，先停止系统自带的网络管理功能：

```
sudo /etc/init.d/networking stop
```

然后配置基于以太网的、VLAN ID 为 100 的网络连接：

```
sudo vconfig add eth0 100
sudo ifconfig eth0.100 up
sudo dhclient eth0.100
```

命令运行成功后，`eth0.100` 网卡可从模块的 `dhcp_lan100` 获取到 192.168.100.x 的 IP 地址，PC1 即可访问网络（出口为 APN1）。

> 解读：PC 侧必须先 `networking stop` 关掉 NetworkManager，否则它会与手工 VLAN 配置冲突。`vconfig add eth0 100` 在 eth0 上创建 802.1Q VLAN 子接口 eth0.100，使 PC1 发出的帧带 VLAN 100 标签——与模块侧 `intf_eth100`(eth0.100) 对应，从而被桥接进 bridge100、命中 `acl_lan100_apn1`。

##### 4.3.4.4 PC2 配置（第 63 页）

断开 PC2 上其他网络连接，先停止系统网络管理功能：

```
sudo /etc/init.d/networking stop
```

然后配置基于以太网的、VLAN ID 为 200 的网络连接：

```
sudo vconfig add eth0 200
sudo ifconfig eth0.200 up
sudo dhclient eth0.200
```

命令运行成功后，`eth0.200` 网卡可获取到 192.168.200.x 的 IP 地址，PC2 即可访问网络（出口为 APN2）。

> 解读：与 PC1 配置对称，区别仅是 VLAN ID 改为 200，对应模块侧 bridge200/`acl_lan200_apn2`，出口走 APN2。这样 PC1、PC2 物理上接同一台交换机，却因 VLAN 标签不同被强制分流到不同 APN。

##### 4.3.4.5 数据转发测试（第 63–64 页）

通过 `ql_cmd nf get interface all` 命令可查看到：`intf_data_apn1` 对应的 Linux 网络接口是 `ccinet0`；`intf_data_apn2` 对应的 Linux 网络接口是 `ccinet1`。

> 注：此处 Linux 接口名为 `ccinet0/ccinet1`，与 4.3.4.1 烧录验证回显里的 `rmnet_data0/rmnet_data1` 是同一平台不同命名/不同抓包视角的体现——抓包以本节给出的 `ccinet0/ccinet1` 为准。

在模块中通过 `tcpdump` 在 `ccinet0` 和 `ccinet1` 上抓取数据包，然后在模块、PC1、PC2 和手机上分别访问网络，查看网络访问路径是否为 ACL 指定的路径。验证方法如下：

在模块上运行以下命令分别抓取 `ccinet0` 和 `ccinet1` 上传输的报文，并保存为文件；之后将文件上传到 PC，用 **wireshark** 工具分析报文是否在相应的 Linux 网络接口上传输。

```
tcpdump -i ccinet0 -w /tmp/ccinet0 &
tcpdump -i ccinet1 -w /tmp/ccinet1 &
```

**可验证以下场景：**

1. 通过 `ccinet0` 抓取到包含 PC1 访问网络的报文；通过 `ccinet1` 抓取不到包含 PC1 访问网络的报文。
2. 通过 `ccinet1` 抓取到包含 PC2 访问网络的报文；通过 `ccinet0` 抓取不到包含 PC2 访问网络的报文。
3. 通过 `ccinet1` 抓取到包含模块访问 `www.quectel.com` 的报文（比如用 `ping` 命令访问 www.quectel.com）；通过 `ccinet0` 抓取不到包含模块访问 www.quectel.com 的报文。
4. 通过 `ccinet0` 抓取到包含模块访问其他网络的报文（比如用 `ping` 命令访问 www.github.com）；通过 `ccinet1` 抓取不到包含模块访问其他网络的报文。
5. 通过 `ccinet0` 抓取到包含手机访问网络的报文；通过 `ccinet1` 抓取不到包含手机访问网络的报文。

> 解读：五条验证逐一对应五条 ACL/转发预期：场景 1=PC1(VLAN100)→APN1(ccinet0)；场景 2=PC2(VLAN200)→APN2(ccinet1)；场景 3=模块本地程序访问 quectel.com→`acl_app_server`→APN2(ccinet1)；场景 4=模块本地程序访问其他域名(github.com)→`acl_app_apn1`→APN1(ccinet0)；场景 5=手机(Wi-Fi，共 bridge100)→APN1(ccinet0)。"在 A 抓得到、在 B 抓不到" 的对照法可证明 ACL 精确分流、没有串流。

**上述场景 3 和场景 4 的具体验证方法：**

在模块上用 `tcpdump` 开启抓包后，用 `ping` 命令分别访问 `www.quectel.com` 和 `www.github.com`；然后终止 `tcpdump` 进程，上传对应的报文文件 `/tmp/ccinet0.pcap` 和 `/tmp/ccinet1.pcap` 到 PC。

- 在 PC 上用 wireshark 打开 `ccinet0.pcap`，可看到访问 **www.github.com** 的报文（DNS 查询 github.com → CNAME github.com.s3... → ICMP Echo request/reply 到 13.229.188.59）。
- 在 PC 上用 wireshark 打开 `ccinet1.pcap`，可看到访问 **www.quectel.com** 的报文（DNS 查询 quectel.com → A 47.101.71.206 → ICMP Echo request/reply 到 47.101.71.206）。

> 解读：抓包截图佐证——github 流量（默认/其他域名）只出现在 ccinet0(APN1)，quectel 流量（`acl_app_server` 指定域名）只出现在 ccinet1(APN2)，两者互不串扰，证明基于 URL 的 ACL 分流（`url=quectel.com → intf_data_apn2`）生效。报文中先有 DNS 解析（端口 53）再有 ICMP，说明 DNS 代理也按所属 ACL 路径走对了出口。

### 4.4 场景四（第 64–70 页）

#### 4.4.1 场景需求（第 64 页）

在一个网络场景中，LAN 侧根据不同的业务类型划分了不同类型的 VLAN，每个 VLAN 都是一个单独的局域网。接入 VLAN 的设备使用**静态 IP 地址和静态 ARP** 的方式配置网络。同时存在**三路数据拨号**，不同的数据拨号用于访问不同的业务。

**表 22：组网信息**

| VLAN ID | 用途 | 访问网络 | 设备信息 | 模块信息 |
|---|---|---|---|---|
| 4 | 内部通信 | 无 | IP:192.168.4.100/24，MAC:00:22:22:22:04:01 | IP:192.168.4.101/24，MAC:00:22:22:22:04:02 |
| 5 | 内部通信 | 无 | IP:192.168.5.100/24，MAC:00:22:22:22:05:01 | IP:192.168.5.101/24，MAC:00:22:22:22:05:02 |
| 41 | 访问网络 | APN1 对应的网络 | IP:192.168.41.100/24，MAC:00:22:22:22:41:01 | IP:192.168.41.101/24，MAC:00:22:22:22:41:02 |
| 42 | 访问网络 | APN2 对应的网络 | IP:192.168.42.100/24，MAC:00:22:22:22:42:01 | IP:192.168.42.101/24，MAC:00:22:22:22:42:02 |
| 43 | 访问网络 | APN3 对应的网络 | IP:192.168.43.100/24，MAC:00:22:22:22:43:01 | IP:192.168.43.101/24，MAC:00:22:22:22:43:02 |

**配置环境：**

1. (U)SIM 卡：签约三路 APN 信息，APN1 名称为 `testapn1`、APN2 名称为 `testapn2`、APN3 名称为 `testapn3`。其中 **APN1 作为默认承载 APN**。
2. WAN 接口：三路 APN 对应的数据拨号。
3. LAN 接口：基于 ETH VLAN ID 为 4、5、41、42、43 的 VLAN 接口。
4. 局域网：见上方表格。

**转发策略：**

1. VLAN ID 为 4 的 VLAN 和 VLAN ID 为 5 的 VLAN 用于局域网**内部通信**（无外网出口）。
2. 使用 VLAN ID 为 41 接入局域网的设备，通过 APN1 访问网络。
3. 使用 VLAN ID 为 42 接入局域网的设备，通过 APN2 访问网络。
4. 使用 VLAN ID 为 43 接入局域网的设备，通过 APN3 访问网络。
5. `www.test.com` 对应的网络服务位于 APN3 对应的网络，模块上运行的程序和通过 VLAN ID 为 42 的 VLAN 接口接入 LAN 的设备，基于 APN3 的数据拨号访问 `www.test.com`。
6. 模块上运行的程序使用 APN1 访问其他网络。

> 解读：场景四是场景三的扩展，新增两个特性：(a) **静态 IP + 静态 ARP**——LAN 设备不走 DHCP，IP/MAC 在表 22 中固定，模块侧用 `add arp` 写死 ARP 表项（防 IP 冲突/防 ARP 欺骗，常见于工业/车载固定拓扑）；(b) **三路 APN + 纯内部通信 VLAN**——VLAN4/5 只内通不出网，VLAN41/42/43 分别绑 APN1/2/3；并叠加基于 URL 的细粒度分流（VLAN42 访问 test.com 走 APN3、访问其他走 APN2）。

**图 15：Linux 网络拓扑图（场景四）**：五条数据路径——至服务器(test.com)、APN1 Net、APN2 Net、APN3 Net 各自路径，下挂 bridge4/bridge5/bridge41/bridge42/bridge43（分别对应 eth0.4 / eth0.5 / eth0.41 / eth0.42 / eth0.43），每个桥接一台 PC（静态 IP/MAC 见表 22）。

#### 4.4.2 QL_NF 组网（第 66–68 页）

根据场景需求，NF 接口定义如下：

| NF_INTF 名称 | 类型 | Linux 接口名 | 描述 | 地址 | WAN/LAN | 开启 DHCP? |
|---|---|---|---|---|---|---|
| `intf_lan4` | bridge | bridge4 | 管理 192.168.4.0/24 网段的局域网 | 192.168.4.101/24 | LAN | 否 |
| `intf_lan5` | bridge | bridge5 | 管理 192.168.5.0/24 网段的局域网 | 192.168.5.101/24 | LAN | 否 |
| `intf_lan41` | bridge | bridge41 | 管理 192.168.41.0/24 网段的局域网 | 192.168.41.101/24 | LAN | 否 |
| `intf_lan42` | bridge | bridge42 | 管理 192.168.42.0/24 网段的局域网 | 192.168.42.101/24 | LAN | 否 |
| `intf_lan43` | bridge | bridge43 | 管理 192.168.43.0/24 网段的局域网 | 192.168.43.101/24 | LAN | 否 |
| `intf_eth` | com_dev | eth0 | ETH 接口的 NF_INTF | - | LAN | - |
| `intf_eth4` | vdev | eth0.4 | 基于 ETH 接口创建的 VLAN ID 为 4 的 NF_INTF | - | LAN | - |
| `intf_eth5` | vdev | eth0.5 | 基于 ETH 接口创建的 VLAN ID 为 5 的 NF_INTF | - | LAN | - |
| `intf_eth41` | vdev | eth0.41 | 基于 ETH 接口创建的 VLAN ID 为 41 的 NF_INTF | - | LAN | - |
| `intf_eth42` | vdev | eth0.42 | 基于 ETH 接口创建的 VLAN ID 为 42 的 NF_INTF | - | LAN | - |
| `intf_eth43` | vdev | eth0.43 | 基于 ETH 接口创建的 VLAN ID 为 43 的 NF_INTF | - | LAN | - |
| `intf_data_apn1` | data_call | 数据拨号成功后的信息 | 基于 APN1 的数据拨号的接口 | 数据拨号成功后的信息 | WAN | - |
| `intf_data_apn2` | data_call | 数据拨号成功后的信息 | 基于 APN2 的数据拨号的接口 | 数据拨号成功后的信息 | WAN | - |
| `intf_data_apn3` | data_call | 数据拨号成功后的信息 | 基于 APN3 的数据拨号的接口 | 数据拨号成功后的信息 | WAN | - |

> 解读：与场景三相比——bridge 数量增到 5 个、data_call 增到 3 路，且所有 LAN bridge 的 "开启 DHCP?" 列均为 **否**（场景四用静态 IP，不起 DHCP 服务）。bridge 地址用 `.101`（模块侧），与表 22 "模块信息" 列一致；设备侧用 `.100`。

转发规则（ACL）：

| ACL 规则名称 | 数据流来源 NF_INTF | 数据流匹配规则 | 数据流目的 NF_INTF | 描述 |
|---|---|---|---|---|
| `acl_app_server` | `intf_local` | 域名为 www.test.com | `intf_data_apn3` | 运行在模块的应用程序，通过基于 APN3 的数据拨号访问域名为 www.test.com 的网络服务 |
| `acl_app_apn1` | `intf_local` | 所有数据流 | `intf_data_apn1` | 运行在模块的应用程序，通过基于 APN1 的数据拨号访问网络 |
| `acl_lan42_server` | `intf_lan42` | url=test.com | `intf_data_apn3` | 通过 VLAN ID 42 的设备，通过基于 APN3 的数据拨号访问域名为 www.test.com 的网络服务 |
| `acl_lan41_apn1` | `intf_lan41` | 所有数据流 | `intf_data_apn1` | 通过 VLAN ID 41 接入的设备，通过基于 APN1 的数据拨号访问网络 |
| `acl_lan42_apn2` | `intf_lan42` | 所有数据流 | `intf_data_apn2` | 通过 VLAN ID 42 接入的设备，通过基于 APN2 的数据拨号访问网络 |
| `acl_lan43_apn3` | `intf_lan43` | 所有数据流 | `intf_data_apn3` | 通过 VLAN ID 43 接入的设备，通过基于 APN3 的数据拨号访问网络 |

> 解读：注意 ACL **顺序即优先级**——更具体的规则必须排在更宽泛的规则前面。`acl_app_server`(域名 test.com→APN3) 在 `acl_app_apn1`(所有→APN1) 之前，`acl_lan42_server`(VLAN42 访问 test.com→APN3) 在 `acl_lan42_apn2`(VLAN42 所有→APN2) 之前；否则 "所有数据流" 会先命中、URL 规则永不生效。VLAN4/VLAN5 没有任何 ACL 出口规则，因此其流量无法转发到 WAN，天然实现 "仅内部通信"。

**图 16：QL_NF 网络拓扑图（场景四）**：六条 ACL 数据路径（acl_app_apn1、acl_app_server、acl_lan41_apn1、acl_lan42_server、acl_lan42_apn2、acl_lan43_apn3）连接 APN1/2/3 Net 与服务器(test.com)，下挂 intf_lan4/lan5/lan41/lan42/lan43 各对应 intf_eth4/eth5/eth41/eth42/eth43，每桥一台 PC（静态 IP/MAC）。

#### 4.4.3 规则配置（第 68–70 页）

```
# 步骤一：设置默认承载 APN 为 APN1
set networks attach_apn apn_name=testapn1 ip_ver=4

# 步骤二：创建 LAN NF_INTF（5 个 VLAN + 5 个桥接，静态 IP、静态 MAC、关 ARP）
add interface intf_eth type=com_dev ifname=eth0
add interface intf_eth4 type=vdev master=intf_eth vlan=4
add interface intf_eth5 type=vdev master=intf_eth vlan=5
add interface intf_eth41 type=vdev master=intf_eth vlan=41
add interface intf_eth42 type=vdev master=intf_eth vlan=42
add interface intf_eth43 type=vdev master=intf_eth vlan=43

add interface intf_lan4 type=bridge ifname=bridge4
set interface intf_lan4 proto=static conn=auto ip=192.168.4.101 netmask=255.255.255.0 mac=00:22:22:22:04:02 noarp=1
set interface intf_lan4 addif=intf_eth4

add interface intf_lan5 type=bridge ifname=bridge5
set interface intf_lan5 proto=static conn=auto ip=192.168.5.101 netmask=255.255.255.0 mac=00:22:22:22:05:02 noarp=1
set interface intf_lan5 addif=intf_eth5

add interface intf_lan41 type=bridge ifname=bridge41
set interface intf_lan41 proto=static conn=auto ip=192.168.41.101 netmask=255.255.255.0 mac=00:22:22:22:41:02 noarp=1
set interface intf_lan41 addif=intf_eth41

add interface intf_lan42 type=bridge ifname=bridge42
set interface intf_lan42 proto=static conn=auto ip=192.168.42.101 netmask=255.255.255.0 mac=00:22:22:22:42:02 noarp=1
set interface intf_lan42 addif=intf_eth42

add interface intf_lan43 type=bridge ifname=bridge43
set interface intf_lan43 proto=static conn=auto ip=192.168.43.101 netmask=255.255.255.0 mac=00:22:22:22:43:02 noarp=1
set interface intf_lan43 addif=intf_eth43

# 步骤三：配置静态 ARP 规则
add arp arp4 intf=intf_lan4 ip=192.168.4.102 mac=00:22:22:22:04:02
add arp arp5 intf=intf_lan5 ip=192.168.5.102 mac=00:22:22:22:05:02
add arp arp41 intf=intf_lan41 ip=192.168.41.102 mac=00:22:22:22:41:02
add arp arp42 intf=intf_lan42 ip=192.168.42.102 mac=00:22:22:22:42:02
add arp arp43 intf=intf_lan43 ip=192.168.43.102 mac=00:22:22:22:43:02

# 步骤四：启用 DNS 代理服务
add dns_srv dns_lan
start dns_srv dns_lan

# 步骤五：创建 WAN NF_INTF（三路数据拨号）
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

# 步骤六：配置转发规则（顺序即优先级，具体规则在前）
add acl acl_app_server sintf=intf_local url=test.com dintf=intf_data_apn3
add acl acl_app_apn1 sintf=intf_local dintf=intf_data_apn1
add acl acl_lan42_server sintf=intf_lan42 url=test.com dintf=intf_data_apn3
add acl acl_lan41_apn1 sintf=intf_lan41 dintf=intf_data_apn1
add acl acl_lan42_apn2 sintf=intf_lan42 dintf=intf_data_apn2
add acl acl_lan43_apn3 sintf=intf_lan43 dintf=intf_data_apn3
```

> 解读：场景四相对场景三的配置差异，逐一对照——
> - **步骤二**：每条 `set interface` 新增 `mac=...`（强制桥接 MAC 为表 22 模块侧值）和 `noarp=1`（关闭该桥接的动态 ARP，配合步骤三的静态 ARP 表）；不再有 `add dhcp_srv`/`start dhcp_srv`（静态 IP 组网无需 DHCP）。
> - **步骤三**（场景三所无）：`add arp` 为每个 LAN 写死一条静态 ARP 表项，`ip=.102`/`mac=...:.02`——即模块预先知道对端设备的 IP↔MAC 绑定，跳过 ARP 学习。
> - **步骤五**：data_call 由 2 路扩到 3 路（testapn1/2/3 → call_id 1/2/3 → intf_data_apn1/2/3）。
> - **步骤六**：6 条 ACL，关键是把两条 URL 规则（`acl_app_server`、`acl_lan42_server`）分别排在对应宽泛规则（`acl_app_apn1`、`acl_lan42_apn2`）之前，保证 test.com 流量优先命中 APN3。VLAN4/5 无 ACL 出口，仅靠桥接互通实现内部通信。
>
> 总结：场景四完整演示了 AG35 QL_NF 框架的全部核心能力——多 VLAN 隔离、静态 IP/MAC/ARP、多路 APN 并发、基于来源接口 + 基于 URL 的多维 ACL 分流、以及 ACL 优先级排序。

## 5 附录 参考文档及术语缩写（第 71–72 页）

### 5.1 参考文档

**表 23：参考文档**

| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |
| [2] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_Wi-Fi_开发指导 |
| [3] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_数据拨号_开发指导 |

> 解读：本文（网络管理指导）聚焦 QL_NF 组网与转发规则；具体的环境搭建/编译/烧录见 [1]，Wi-Fi AP 的完整 API 见 [2]（呼应 4.3.4.2 "wifi 接口暂未开发" 的交互式临时配置），单路数据拨号的底层流程见 [3]。三者构成 AG35 QuecOpen SDK 的网络相关文档集。

### 5.2 术语缩写

**表 24：术语缩写**

| 缩写 | 英文描述 | 中文描述 |
|---|---|---|
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
| ID | Identification | 标识 |
| IoV | Internet of Vehicles | 车联网 |
| IP | Internet Protocol | 互联网协议 |
| ISP | Internet Service Provider | 互联网服务提供商 |
| MAC | Media Access Control | 媒体存取控制位 |
| MTU | Maximum Transmission Unit | 最大传输单元 |
| NCM | Network Control Model | 网络控制模型 |
| OUI | Organizationally Unique Identifier | 组织唯一标识符 |
| PAP | Password Authentication Protocol | 密码认证协议 |
| PC | Personal Computer | 个人计算机 |
| PPP | Point to Point Protocol | 点对点协议 |
| QOS | Quality of Service | 服务质量 |
| QL_NF | QUECTEL Network Framework | 移远通信网络管理框架 |
| RNDIS | Remote Network Driver Interface Specification | 远程网络驱动程序接口规范 |
| SDK | Software Development Kit | 软件开发工具包 |
| (U)SIM | (Universal) Subscriber Identity Module | （通用）用户识别卡 |
| TCP | Transmission Control Protocol | 传输控制协议 |
| UDP | User Datagram Protocol | 用户数据报协议 |
| URL | Uniform Resource Locator | 统一资源定位符 |
| USB | Universal Serial Bus | 通用串行总线 |
| VLAN | Virtual Local Area Network | 虚拟局域网 |
| VPN | Virtual Private Network | 虚拟专用网络 |
| WAN | Wide Area Network | 广域网 |
| WLAN | Wireless Local Area Network | 无线局域网 |
| WIFI | Wireless Fidelity | 无线通信技术 |

> 解读：术语表中与本文 QL_NF 实战强相关的是 **QL_NF**(移远网络管理框架)、**ACL**(访问控制列表，转发规则核心)、**APN/VLAN/DHCP/DNS/ARP**(组网四件套)；**DSDA**(双卡双通) 虽列入缩写表，但本文场景一~四均为单卡多 APN，未涉及双卡——与 AG35 双卡单待(DSSS) 的论证 [[ag35-dual-sim-dsss-design]] 属不同议题，注意区分。

---

> **全文分析完成**：本文档已逐页覆盖《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_网络管理指导_V1.0.1》全部 72 页正文（封面/版本历史/目录/第 1 章概述 → 第 2 章 NF 框架 → 第 3 章 NF 命令 → 第 4 章四个组网场景实战 → 第 5 章附录），含全部表格（表 1–表 24）、图（图 1–图 16）、QL_NF/NF/ACL 命令、参数与注意事项。

<!-- GENERATION_COMPLETE -->
