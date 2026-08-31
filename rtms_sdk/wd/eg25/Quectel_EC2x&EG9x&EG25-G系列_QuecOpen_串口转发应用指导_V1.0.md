# EC2x&EG9x&EG25-G 系列 QuecOpen — 串口转发应用指导

> **模块系列：** LTE Standard 系列
> **版本：** EC2x&EG9x&EG25-G 系列 QuecOpen_串口转发应用指导_V1.0
> **日期：** 2020-07-15
> **状态：** 受控文件
> **版权：** Copyright © Quectel Wireless Solutions Co., Ltd. 2020. 保留一切权利。

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| 1.0 | 2020-07-15 | 孙保 | 初始版本 |

---

## 目录

- [1 引言](#1-引言)
  - [1.1 适用模块](#11-适用模块)
- [2 串口转发端口](#2-串口转发端口)
  - [2.1 USB 端口](#21-usb-端口)
  - [2.2 SMD 虚拟端口](#22-smd-虚拟端口)
- [3 串口转发功能介绍](#3-串口转发功能介绍)
  - [3.1 通过 UART 收发 AT 命令](#31-通过-uart-收发-at-命令)
    - [3.1.1 通过主串口收发 AT 命令](#311-通过主串口收发-at-命令)
    - [3.1.2 通过调试串口收发 AT 命令](#312-通过调试串口收发-at-命令)
  - [3.2 通过 UART 或 USB NMEA 端口输出 NMEA 数据](#32-通过-uart-或-usb-nmea-端口输出-nmea-数据)
    - [3.2.1 通过主串口输出 NMEA 数据](#321-通过主串口输出-nmea-数据)
    - [3.2.2 通过调试串口输出 NMEA 数据](#322-通过调试串口输出-nmea-数据)
    - [3.2.3 通过 USB NMEA 端口输出 NMEA 数据](#323-通过-usb-nmea-端口输出-nmea-数据)
- [4 附录 A 参考文档和术语缩写](#4-附录-a-参考文档和术语缩写)

### 图片索引

- 图 1：Windows 通过设备管理器查看 USB 端口
- 图 2：通过 Windows 端的 QCOM 工具打开 USB AT 端口
- 图 3：实现主串口与 SMD8 间数据交互转发
- 图 4：实现调试串口与 SMD8 间数据交互转发
- 图 5：实现主串口与 SMD7 间数据交互转发
- 图 6：实现调试串口与 SMD7 间数据交互转发
- 图 7：实现 USB NMEA 口与 SMD7 间数据交互转发

### 表格索引

- 表 1：适用模块
- 表 2：参考文档
- 表 3：术语缩写

---

## 1 引言

移远通信 LTE Standard EC2x&EG9x&EG25-G 系列模块支持 QuecOpen® 方案。本文档主要介绍了**串口转发功能**，即通过移远通信模块在指定的 UART 端口和虚拟端口之间进行数据转发，包括**收发 AT 命令**和**输出 NMEA 数据**。

### 1.1 适用模块

**表 1：适用模块**

| 模块系列 | 模块 |
|---|---|
| EC2x 系列 | EC25 系列 |
| EC2x 系列 | EC21 系列 |
| EC2x 系列 | EC20 R2.1 |
| EC2x 系列 | EC20-CN |
| EG9x 系列 | EG95 系列 |
| EG9x 系列 | EG91 系列 |
| EG25-G | EG25-G |

---

## 2 串口转发端口

EC2x&EG9x&EG25-G 系列 QuecOpen 模块提供两个用于收发 AT 命令的端口：**USB AT 端口**和 **SMD 虚拟端口**；以及两个用于传输 NMEA 数据的端口：**USB NMEA 端口**和 **SMD 虚拟端口**。详细信息请参考第 3 章。

### 2.1 USB 端口

当 USB 连接到 PC 时，USB AT 端口将在设备管理器中列出，如下图的 COM27：

> 图 1：Windows 通过设备管理器查看 USB 端口 —— Windows 设备管理器中列出 USB AT 端口（示例为 COM27）的截图。

Windows 端的 QCOM 工具可以打开 USB AT 端口，并收发 AT 命令，如下图所示：

> 图 2：通过 Windows 端的 QCOM 工具打开 USB AT 端口 —— QCOM 工具打开 USB AT 端口并收发 AT 命令的截图。

> **备注**
> COM13 需连接到主串口（Main UART），COM12 需连接到调试串口（Debug UART）。

### 2.2 SMD 虚拟端口

SMD 虚拟端口包含如 **SMD7、SMD8、SMD9** 等。

以 SMD8 虚拟端口为例，则其设备文件名为 `/dev/smd8`，如下图所示。其他虚拟端口设备文件名以此类推。

> 图（原文配图）：SMD8 虚拟端口对应设备文件 `/dev/smd8` 的截图。

SMD 虚拟端口可用于在用户内部编码中发送 AT 命令，下图以 SMD8 为例。

> 图（原文配图）：用户内部编码中通过 SMD8 虚拟端口发送 AT 命令的截图。

> **备注**
> 一个虚拟端口不可同时进行 AT 命令收发和 NMEA 数据输出，故为避免用户同时进行 AT 命令收发和 NMEA 数据输出，第 3.1 章和第 3.2 章分别以 **SMD8** 和 **SMD7** 为例对串口转发功能进行介绍。

---

## 3 串口转发功能介绍

在终端产品中，通常不使用 USB AT 端口，可使用**主串口**或**调试串口**来收发 AT 命令；以及通过**主串口、调试串口或 USB NMEA 端口**来输出 NMEA 数据。

### 3.1 通过 UART 收发 AT 命令

执行如下命令查看 `quectel-uart-ddp` 的帮助信息：

```bash
quectel-uart-ddp -help
```

#### 3.1.1 通过主串口收发 AT 命令

执行以下命令实现**主串口**与 **SMD8** 虚拟端口间数据交互转发：

```bash
quectel-uart-ddp –uart /dev/ttyHS0 -smd /dev/smd8
```

> 图 3：实现主串口与 SMD8 间数据交互转发 —— 主串口（`/dev/ttyHS0`）与 SMD8 虚拟端口间 AT 命令交互转发的截图。

#### 3.1.2 通过调试串口收发 AT 命令

执行以下命令实现**调试串口**与 **SMD8** 虚拟端口间数据交互转发：

```bash
quectel-uart-ddp –uart /dev/ttyHSL0 -smd /dev/smd8
```

> 图 4：实现调试串口与 SMD8 间数据交互转发 —— 调试串口（`/dev/ttyHSL0`）与 SMD8 虚拟端口间 AT 命令交互转发的截图。

### 3.2 通过 UART 或 USB NMEA 端口输出 NMEA 数据

#### 3.2.1 通过主串口输出 NMEA 数据

使用主串口输出 NMEA 数据，即通过 `quectel-uart-ddp` 应用程序设置 NMEA 数据输出功能。

执行以下命令实现**主串口**与 **SMD7** 虚拟端口间数据交互转发：

```bash
quectel-uart-ddp –uart /dev/ttyHS0 -smd /dev/smd7
```

然后在发送 `AT+QGPS=1` 启用 GNSS 功能后，即可通过主串口 **COM13** 输出 NMEA 数据，如下图所示。有关该 AT 命令的详细信息，可参考文档 [1]。

> 图 5：实现主串口与 SMD7 间数据交互转发 —— 主串口（`/dev/ttyHS0`，COM13）输出 NMEA 数据的截图。

#### 3.2.2 通过调试串口输出 NMEA 数据

执行以下命令实现**调试串口**与 **SMD7** 虚拟端口间数据交互转发：

```bash
quectel-uart-ddp –uart /dev/ttyHSL0 -smd /dev/smd7
```

然后在发送 `AT+QGPS=1` 启用 GNSS 功能后，即可通过调试串口 **COM12** 输出 NMEA 数据，如下图所示：

> 图 6：实现调试串口与 SMD7 间数据交互转发 —— 调试串口（`/dev/ttyHSL0`，COM12）输出 NMEA 数据的截图。

#### 3.2.3 通过 USB NMEA 端口输出 NMEA 数据

执行以下命令实现 **USB NMEA 口 COM26** 与 SMD 虚拟端口（以 SMD7 为例）间数据交互转发：

```bash
quectel-uart-ddp –uart /dev/ttyGS0 -smd /dev/smd7
```

然后在发送 `AT+QGPS=1` 启用 GNSS 功能后，即可通过 USB NMEA 端口 **COM26** 输出 NMEA 数据，如下图所示：

> 图 7：实现 USB NMEA 口与 SMD7 间数据交互转发 —— USB NMEA 端口（`/dev/ttyGS0`，COM26）输出 NMEA 数据的截图。

---

## 4 附录 A 参考文档和术语缩写

**表 2：参考文档**

| 缩写 | 文件名 | 备注 |
|---|---|---|
| [1] | Quectel_LTE_Standard_GNSS_应用指导 | 适用于 EC2x 系列、EG9x 系列和 EG25-G 模块的 GNSS 应用指导 |

**表 3：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| GNSS | Global Navigation Satellite System | 全球定位系统 |
| NMEA | NMEA (National Marine Electronics Association) 0183 Interface Standard | NMEA（美国国家海洋电子协会）0183 接口标准 |
| PC | Personal Computer | 个人计算机 |
| SMD | Surface Mounted Devices | 表面贴装器件 |
| UART | Universal Asynchronous Receiver/Transmitter | 通用异步收发传输器 |
| USB | Universal Serial Bus | 通用串行总线 |
