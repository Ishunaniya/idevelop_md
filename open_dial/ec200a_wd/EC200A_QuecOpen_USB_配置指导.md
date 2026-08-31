# EC200A-CN(TA) QuecOpen USB 配置指导

> **文档编号**：无  
> **版本**：1.0.0  
> **日期**：2022-07-07  
> **状态**：临时文件  
> **适用模块**：LTE Standard 模块系列（EC200A-CN(TA)）  
> **发布方**：上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.）

---

## 联系信息

**上海移远通信技术股份有限公司**  
上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼　邮编：200233  
电话：+86 21 5108 6236　邮箱：info@quectel.com  
销售支持：http://www.quectel.com/cn/support/sales.htm  
技术支持：http://www.quectel.com/cn/support/technical.htm　邮件：support@quectel.com

---

## 法律声明

### 使用和披露限制

**许可协议**  
除非移远通信特别授权，否则我司所提供硬软件、材料和文档的接收方须对接收的内容保密，不得将其用于除本项目的实施与开展以外的任何其他目的。

**版权声明**  
移远通信产品和本协议项下的第三方产品可能包含受移远通信或第三方材料、硬软件和文档版权保护的相关资料。除非事先得到书面同意，否则你不得获取、使用、向第三方披露我司所提供的文档和信息，或对此类受版权保护的资料进行复制、转载、抄袭、出版、展示、翻译、分发、合并、修改，或创建其衍生作品。移远通信或第三方对受版权保护的资料拥有专有权，不授予或转让任何专利、版权、商标或服务商标的许可。对于任何违反保密义务、未经授权使用或以其他非法形式恶意使用所述文档和信息的违法侵权行为，移远通信有权追究法律责任。

**商标**  
除另行规定，本文档中的任何内容均不授予在广告、宣传或其他方面使用移远通信或第三方的任何商标、商号及名称，或其缩略语，或其仿冒品的权利。

**第三方权利**  
您理解本文档可能涉及一个或多个属于第三方的硬软件和文档（"第三方材料"）。您对此类第三方材料的使用应受本文档的所有限制和义务约束。

移远通信针对第三方材料不做任何明示或暗示的保证或陈述，包括但不限于任何暗示或法定的适销性或特定用途的适用性、平静受益权、系统集成、信息准确性以及与许可技术或被许可人使用许可技术相关的不侵犯任何第三方知识产权的保证。本协议中的任何内容都不构成移远通信对任何移远通信产品或任何其他硬软件、设备、工具、信息或产品的开发、增强、修改、分销、营销、销售、提供销售或以其他方式维持生产的陈述或保证。此外，移远通信免因交易过程、使用或贸易而产生的任何和所有保证。

### 隐私声明

为实现移远通信产品功能，特定设备数据将会上传至移远通信或第三方服务器（包括运营商、芯片供应商或您指定的服务器）。移远通信严格遵守相关法律法规，仅为实现产品功能之目的或在适用法律允许的情况下保留、使用、披露或以其他方式处理相关数据。当您与第三方进行数据交互时，请自行了解其隐私保护和数据安全政策。

### 免责声明

1. 移远通信不承担任何因未能遵守有关操作或设计规范而造成损害的责任。
2. 移远通信不承担因本文档中的任何因不准确、遗漏、或使用本文档中的信息而产生的任何责任。
3. 移远通信尽力确保开发中功能的完整性、准确性、及时性，但不排除上述功能错误或遗漏的可能。除非另有协议规定，否则移远通信对开发中功能的使用不做任何明示或法定的保证。在适用法律允许的最大范围内，移远通信不对任何因使用开发中功能而遭受的损害承担责任，无论此类损害是否可以预见。
4. 移远通信对第三方网站及第三方资源的信息、内容、广告、商业报价、产品、服务和材料的可访问性、安全性、准确性、可用性、合法性和完整性不承担任何法律责任。

**版权所有 © 上海移远通信技术股份有限公司 2022，保留一切权利。**  
*Copyright © Quectel Wireless Solutions Co., Ltd. 2022.*

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|------|------|------|---------|
| - | 2021-11-26 | Larry ZHANG | 文档创建 |
| 1.0.0 | 2022-07-07 | Eyelyn TANG | 临时版本 |

---

## 目录

- [1 引言](#1-引言)
- [2 配置 USB 网卡类型](#2-配置-usb-网卡类型)
  - [2.1 通过 SDK](#21-通过-sdk)
  - [2.2 临时修改网卡模式](#22-临时修改网卡模式)
  - [2.3 通过配置文件](#23-通过配置文件)
- [3 配置 ADB](#3-配置-adb)
  - [3.1 通过 SDK](#31-通过-sdk)
  - [3.2 临时打开 ADB 功能](#32-临时打开-adb-功能)
  - [3.3 通过配置文件](#33-通过配置文件)
- [4 USB 常规检查](#4-usb-常规检查)
  - [4.1 USB 传输速率检查](#41-usb-传输速率检查)
  - [4.2 USB 枚举](#42-usb-枚举)
  - [4.3 ADB 功能](#43-adb-功能)
  - [4.4 USB 端口](#44-usb-端口)
- [5 附录 参考文档及术语缩写](#5-附录-参考文档及术语缩写)

---

## 表格索引

| 表号 | 标题 | 页码 |
|------|------|------|
| 表 1 | 参考文档 | 11 |
| 表 2 | 术语缩写 | 11 |

---

## 1 引言

移远通信 EC200A-CN(TA) 模块支持 QuecOpen® 方案。QuecOpen® 是基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。有关 QuecOpen 的详细信息，请参考**文档 [1]**。

本文研究对象为 EC200A-CN(TA) 模块 USB 相关的配置，目前模块 USB 相关的配置主要包括 USB 网卡类型的配置、ADB 配置以及 USB 的常规检查等三部分内容。

---

## 2 配置 USB 网卡类型

EC200A-CN(TA) QuecOpen 模块支持的 USB 网卡类型为 **ECM** 和 **RNDIS**，默认为 **ECM**。本章介绍如何修改 USB 网卡类型。

配置网卡类型有如下两种方式：

- **修改配置文件**：该方法主要应用于模块验证或调试。
- **修改 SDK 包文件**：配置 USB 网卡类型后需重新编译 rootfs 镜像并进行烧录。

---

### 2.1 通过 SDK

在 SDK 包的 `ql-ol-rootfs/sbin/` 目录下找到 `usb_init` 文件。如下图所示，`QUEC_USB_NET_TYPE="ecm"` 表示当前模块的 USB 网卡类型为 ECM。如需修改为 RNDIS 网卡类型，仅需将 `"ecm"` 字样替换为 `"rndis"`。

**`usb_init` 文件内容示例：**

```sh
#!/bin/sh
# usb_init script

#eyelyn add 20220621 usb configured node
#The default nic mode is ECM and ADB is enabled by default
QUEC_USB_NET_TYPE="ecm"
QUEC_USB_ADB_ENABLE="on"
```

> **备注**  
> 修改网卡类型后需要重新编译 rootfs 镜像，然后烧录新的镜像到模块中，烧录成功后配置方可生效。有关编译和固件烧录的详细信息，请参考**文档 [1]** 和 **[2]**。

---

### 2.2 临时修改网卡模式

**步骤 1：** 通过 Debug 串口或者 ADB 连接模块至主机。

**步骤 2：** 执行 `cat /sys/devices/virtual/android_usb/android0/functions` 命令查询当前 USB 网卡类型，如下图所示：

```bash
root@OpenWrt:/# cat /sys/devices/virtual/android_usb/android0/functions
ecm,marvell_diag,acm,marvell_modem,adb
root@OpenWrt:/#
```

**步骤 3：** 分别执行命令切换网卡类型。

**执行如下命令将 USB 网卡类型切换为 RNDIS：**

```bash
echo 0 > /sys/devices/virtual/android_usb/android0/enable
echo rndis,marvell_diag,acm,marvell_modem,adb > /sys/devices/virtual/android_usb/android0/functions
echo 1 > /sys/devices/virtual/android_usb/android0/enable
```

**执行如下命令切换回 ECM：**

```bash
echo 0 > /sys/devices/virtual/android_usb/android0/enable
echo ecm,marvell_diag,acm,marvell_modem,adb > /sys/devices/virtual/android_usb/android0/functions
echo 1 > /sys/devices/virtual/android_usb/android0/enable
```

> **备注**  
> 设置完成后执行 `sync`，配置立即生效，重启模块后需要重新配置。

---

### 2.3 通过配置文件

**步骤 1：** 通过 Debug 串口或者 ADB 连接模块至主机。

**步骤 2：** 执行 `cat /data/usb/usb_net_type` 查询当前 USB 网卡类型。如下图所示，`/data/usb/usb_net_type` 节点的值为 `rndis`，即为 RNDIS 网卡模式：

```bash
root@OpenWrt:/#
root@OpenWrt:/# cat /data/usb/usb_net_type
rndis
root@OpenWrt:/#
```

**步骤 3：** 执行对应命令切换网卡类型：

- 执行命令配置网卡模式为 ECM：`echo ecm > /data/usb/usb_net_type`
- 执行命令配置网卡模式为 RNDIS：`echo rndis > /data/usb/usb_net_type`

**步骤 4：** 重启模块或执行 `/sbin/usb_init` 脚本使配置生效。

---

## 3 配置 ADB

EC200A-CN(TA) QuecOpen 模块默认打开 ADB 功能。可通过配置文件或修改 SDK 包中的文件修改 ADB 功能状态。

---

### 3.1 通过 SDK

打开 SDK 包 `ql-ol-rootfs/sbin/init.d/` 目录下的 `usb_init` 文件，找到 ADB 功能配置所在的位置，如下图所示：

```sh
#!/bin/sh
# usb_init script

#eyelyn add 20220621 usb configured node
#The default nic mode is ECM and ADB is enabled by default
QUEC_USB_NET_TYPE="ecm"
QUEC_USB_ADB_ENABLE="on"
```

如上图所示，默认状态下，`QUEC_USB_ADB_ENABLE="on"` 表示 ADB 功能已打开。如需关闭该功能，仅需将 `"on"` 更改为 `"off"`，即 `QUEC_USB_ADB_ENABLE="off"`。

> **备注**  
> 关闭 ADB 功能后需要重新编译 rootfs 镜像，然后烧录新的镜像到模块中，烧录成功后配置方可生效。有关编译和固件烧录的详细信息，请参考**文档 [1]** 和 **[2]**。

---

### 3.2 临时打开 ADB 功能

**步骤 1：** 执行 `cat /data/usb/usb_adb_enable` 查询当前 ADB 功能是否已打开。如下图所示，查询当前 ADB 功能已打开：

```bash
root@OpenWrt:/# cat /sys/devices/virtual/android_usb/android0/functions
ecm,marvell_diag,acm,marvell_modem,adb
root@OpenWrt:/#
root@OpenWrt:/#
```

**步骤 2：** 执行对应命令切换 ADB 功能状态：

**执行如下命令关闭 ADB 功能：**

```bash
echo 0 > /sys/devices/virtual/android_usb/android0/enable
echo ecm,marvell_diag,acm,marvell_modem > /sys/devices/virtual/android_usb/android0/functions
echo 1 > /sys/devices/virtual/android_usb/android0/enable
```

**执行如下命令重新打开 ADB 功能：**

```bash
echo 0 > /sys/devices/virtual/android_usb/android0/enable
echo ecm,marvell_diag,acm,marvell_modem,adb > /sys/devices/virtual/android_usb/android0/functions
echo 1 > /sys/devices/virtual/android_usb/android0/enable
```

> **备注**  
> 执行上述命令后，配置立即生效，重启后将恢复为默认配置。

---

### 3.3 通过配置文件

**步骤 1：** 执行 `cat /data/usb/usb_adb_enable` 查询当前 ADB 功能是否已打开。如下图所示，返回 `on` 表示当前 ADB 功能已打开：

```bash
root@OpenWrt:/#
root@OpenWrt:/# cat /data/usb/usb_adb_enable
on
root@OpenWrt:/#
root@OpenWrt:/#
root@OpenWrt:/#
```

**步骤 2：** 执行如下命令切换 ADB 状态：

- 执行命令关闭 ADB 功能：`echo off > /data/usb/usb_adb_enable`
- 执行命令开启 ADB 功能：`echo on > /data/usb/usb_adb_enable`

**步骤 3：** 重启模块或执行 `/sbin/usb_init` 脚本使配置生效。

---

## 4 USB 常规检查

### 4.1 USB 传输速率检查

EC200A-CN(TA) 模块仅支持 USB 2.0 协议传输速率。可通过执行以下命令查询当前模块传输速率：

```bash
cat /sys/devices/soc.0/d4200000.axi/mv-udc/udc/mv-udc/curremt_sp
```

或使用简化路径：

```bash
root@OpenWrt:/# cat /sys/class/udc/mv-udc/current_speed
high-speed
root@OpenWrt:/#
```

若返回值为 `high-speed`，则表示当前为 USB 2.0 协议传输速率。

> **说明**：除正常的工作模式外，下载模式也使用 USB 2.0 协议传输速率进行传输。

---

### 4.2 USB 枚举

执行 `cat /sys/devices/virtual/android_usb/android0/state` 查看枚举结果，若返回值为 `CONFIGURED`，则表示 USB 枚举成功。

```bash
root@OpenWrt:/#
root@OpenWrt:/# cat /sys/devices/virtual/android_usb/android0/state
CONFIGURED
root@OpenWrt:/#
```

---

### 4.3 ADB 功能

若模块的 ADB 功能无法使用，请按以下步骤排查：

**步骤 1：** 首先确认 ADB 功能已打开（详见**第 3 章**）。若 ADB 功能已打开，请执行 `ps | grep adb` 检查 ADB 进程是否存在，若返回值包含如下内容则表示 ADB 进程已存在：

```bash
root@OpenWrt:/#
root@OpenWrt:/# ps | grep adb
844 root     0:00 /usr/bin/adbd -D    ←（红框标示行，表示 ADB 进程运行中）
971 root     0:00 grep adb
root@OpenWrt:/#
```

**步骤 2：** 若 ADB 进程不存在，执行 `/etc/init.d/adbd start` 手动启动该进程。

**步骤 3：** 若 ADB 功能已打开且 ADB 进程已启动，仍无法使用 ADB 功能，请联系移远通信技术支持。

---

### 4.4 USB 端口

若出现 USB 端口未被识别的情况，请依次进行以下操作排除失败原因。若以下情况均无异常，请联系移远通信技术支持。

**步骤 1：** 请确保 USB 数据线无松动。USB 数据线松动可能会导致模块与主机连接失败。

**步骤 2：** 请确认 USB 枚举成功（详见**第 4.2 章**）。

- 若显示 USB 枚举成功，可能是由于主机端驱动未安装或安装失败造成的，请卸载驱动并重新安装驱动。
- 若显示 USB 枚举失败，请联系移远通信技术支持。

---

## 5 附录 参考文档及术语缩写

### 表 1：参考文档

| 序号 | 文档名称 |
|------|---------|
| [1] | Quectel_EC200A-CN(TA)_QuecOpen_快速开发指南 |
| [2] | Quectel_EC200A-CN(TA)_QuecOpen_固件烧录指南 |

### 表 2：术语缩写

| 缩写 | 英文解释 | 中文解释 |
|------|---------|---------|
| ADB | Android Debug Bridge | 安卓调试桥 |
| ECM | Ethernet Control Model | 以太网控制模型 |
| IoT | Internet of Things | 物联网 |
| RNDIS | Remote Network Driver Interface Specification | 远程网络驱动接口规范 |
| SDK | Software Development Kit | 软件开发工具包 |
| USB | Universal Serial Bus | 通用串行总线 |

---

*本文档内容基于 Quectel EC200A-CN(TA) QuecOpen USB 配置指导 V1.0.0（2022-07-07）完整整理，覆盖全部 13 页原文内容。*
