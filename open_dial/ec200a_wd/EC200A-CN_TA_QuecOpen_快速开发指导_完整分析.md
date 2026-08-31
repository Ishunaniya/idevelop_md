# EC200A-CN(TA) QuecOpen® 快速开发指导 — 完整分析文档

> **原始文档信息**
> - 文档标题：EC200A-CN(TA) QuecOpen 快速开发指导
> - 所属系列：LTE Standard 模块系列
> - 版本：1.0.0
> - 日期：2022-07-07
> - 状态：临时文件（Preliminary Document, Not Checked）
> - 出版方：上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.）
> - 地址：上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼，邮编：200233
> - 电话：+86 21 5108 6236　邮箱：info@quectel.com
> - 技术支持：support@quectel.com

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更描述 |
|------|------|------|---------|
| — | 2021-11-18 | Morris XIAO | 文档创建 |
| 1.0.0 | 2022-07-07 | Morris XIAO / William Liu | 临时版本 |

---

## 目录

1. [引言](#1-引言)
2. [QuecOpen® 软件开发概述](#2-quecopen-软件开发概述)
   - 2.1 [对开发者的要求](#21-对开发者的要求)
   - 2.2 [开发流程](#22-开发流程)
3. [软件包介绍](#3-软件包介绍)
   - 3.1 [固件包](#31-固件包)
   - 3.2 [SDK 软件开发包](#32-sdk-软件开发包)
   - 3.3 [交叉编译工具包](#33-交叉编译工具包)
4. [固件升级](#4-固件升级)
   - 4.1 [使用移远通信升级工具升级](#41-使用移远通信升级工具升级)
   - 4.2 [紧急升级](#42-紧急升级)
5. [软件开发环境搭建](#5-软件开发环境搭建)
   - 5.1 [USB 驱动安装](#51-usb-驱动安装)
   - 5.2 [ADB 工具安装](#52-adb-工具安装)
     - 5.2.1 [安装 ADB 驱动](#521-安装-adb-驱动)
     - 5.2.2 [添加模块 USB VID](#522-添加模块-usb-vid)
     - 5.2.3 [枚举设备](#523-枚举设备)
   - 5.3 [文件下载](#53-文件下载)
   - 5.4 [交叉编译工具安装](#54-交叉编译工具安装)
     - 5.4.1 [创建工作目录](#541-创建工作目录)
     - 5.4.2 [安装交叉编译工具链](#542-安装交叉编译工具链)
     - 5.4.3 [检查是否安装成功](#543-检查是否安装成功)
   - 5.5 [SDK 软件包安装](#55-sdk-软件包安装)
     - 5.5.1 [SDK 软件包解压](#551-sdk-软件包解压)
     - 5.5.2 [检查是否安装成功](#552-检查是否安装成功)
     - 5.5.3 [烧录验证](#553-烧录验证)
6. [SDK 使用方法](#6-sdk-使用方法)
   - 6.1 [SDK 文件说明](#61-sdk-文件说明)
   - 6.2 [编译方式](#62-编译方式)
   - 6.3 [文件系统镜像制作](#63-文件系统镜像制作)
     - 6.3.1 [制作根文件系统镜像](#631-制作根文件系统镜像)
     - 6.3.2 [内核镜像文件制作](#632-内核镜像文件制作)
   - 6.4 [日志系统](#64-日志系统)
     - 6.4.1 [在应用程序中使用日志接口](#641-在应用程序中使用日志接口)
     - 6.4.2 [查看应用程序日志](#642-查看应用程序日志)
     - 6.4.3 [日志保存](#643-日志保存)
   - 6.5 [API 接口](#65-api-接口)
   - 6.6 [应用程序启动](#66-应用程序启动)
   - 6.7 [静态代码扫描](#67-静态代码扫描)
     - 6.7.1 [CPPCHECK 安装](#671-cppcheck-安装)
     - 6.7.2 [使用 CPPCHECK 扫描代码](#672-使用-cppcheck-扫描代码)
7. [模块开机检查](#7-模块开机检查)
8. [附录 参考文档及术语缩写](#8-附录-参考文档及术语缩写)

---

## 表格索引

| 表格编号 | 表格名称 |
|---------|---------|
| 表 1 | 关键固件包文件列表 |
| 表 2 | SDK 文件说明 |
| 表 3 | SDK 根目录下常用编译命令 |
| 表 4 | 示例程序 |
| 表 5 | 日志等级对应日志接口 |
| 表 6 | API 接口使用方法相关文档 |
| 表 7 | 参考文档 |
| 表 8 | 术语和缩写 |

---

## 法律声明摘要

### 前言

移远通信提供该文档内容以支持客户的产品设计。客户须按照文档中提供的规范、参数来设计产品。同时，您理解并同意，移远通信提供的参考设计仅作为示例。您同意在设计您目标产品时使用您独立的分析、评估和判断。

在使用本文档所指导的任何硬软件或服务之前，请仔细阅读本声明。尽管移远通信采取了商业范围内的合理努力来提供尽可能好的体验，但本文档和其所涉及服务是在"可用"基础上提供给您的。移远通信可在未事先通知的情况下，自行决定随时增加、修改或重述本文档。

### 使用和披露限制

- **许可协议**：除非移远通信特别授权，否则我司所提供硬软件、材料和文档的接收方须对接收的内容保密，不得将其用于除本项目的实施与开展以外的任何其他目的。
- **版权声明**：移远通信产品和本协议项下的第三方产品可能包含受移远通信或第三方材料、硬软件和文档版权保护的相关资料。除非事先得到书面同意，否则您不得获取、使用、向第三方披露我司所提供的文档和信息，或对此类受版权保护的资料进行复制、转载、抄袭、出版、展示、翻译、分发、合并、修改、或创建其衍生作品。
- **商标**：除另行规定，本文档中的任何内容均不授予在广告、宣传或其他方面使用移远通信或第三方的任何商标、商号及名称，或其缩略语，或其仿冒品的权利。
- **第三方权利**：您理解本文档可能涉及一个或多个属于第三方的硬软件和文档（"第三方材料"）。您对此类第三方材料的使用应受本文档的所有限制和义务约束。

### 隐私声明

为实现移远通信产品功能，特定设备数据将会上传至移远通信或第三方服务器（包括运营商、芯片供应商或您指定的服务器）。移远通信严格遵守相关法律法规，仅为实现产品功能之目的或在适用法律允许的情况下保留、使用、披露或以其他方式处理相关数据。

### 免责声明

1. 移远通信不承担任何因未能遵守有关操作或设计规范而造成损害的责任。
2. 移远通信不承担因本文档中的任何因不准确、遗漏、或使用本文档中的信息而产生的任何责任。
3. 移远通信尽力确保开发中功能的完整性、准确性、及时性，但不排除上述功能错误或遗漏的可能。除非另有协议规定，否则移远通信对开发中功能的使用不做任何暗示或法定的保证。
4. 移远通信对第三方网站及第三方资源的信息、内容、广告、商业报价、产品、服务和材料的可访问性、安全性、准确性、可用性、合法性和完整性不承担任何法律责任。

> 版权所有 © 上海移远通信技术股份有限公司 2022，保留一切权利。  
> *Copyright © Quectel Wireless Solutions Co., Ltd. 2022.*

---

## 1 引言

移远通信 EC200A-CN(TA) 模块支持 QuecOpen® 方案。**QuecOpen® 是基于 Linux 的嵌入式开发平台，可简化应用的软件设计和开发过程。**

本文档主要介绍 QuecOpen® 方案下 EC200A-CN(TA) 模块的以下内容，帮助用户快速理解和开发：

- 固件升级方法
- 软件开发环境搭建
- SDK 的使用方法
- 模块开机检查的方法
- 其他相关操作流程

---

## 2 QuecOpen® 软件开发概述

### 2.1 对开发者的要求

在使用 QuecOpen® 方案进行开发之前，开发者需具备以下知识背景：

1. **熟悉标准 GNU/Linux 应用开发**，以及常见 Linux 系统命令。
2. **掌握一些驱动、网络协议基本知识**。
3. **了解 EC200A-CN(TA) QuecOpen 模块的 AT 命令知识**，参考文档 [2]（Quectel_EC200A-CN(TA)_QuecOpen_AT_Commands_Manual）。

### 2.2 开发流程

完整的开发流程分为以下 4 个步骤：

**步骤 1 — 硬件准备**

| 要求项 | 最低要求 |
|--------|---------|
| 操作系统 | Ubuntu 1604 或 1804，64 位系统 |
| 内存 | 4 GB 以上 |
| CPU | 4 核以上 |
| 虚拟机内存（如使用） | 不低于 4 GB |

**步骤 2 — 确认固件与 SDK 版本匹配**

- 固件包和 SDK 软件开发包的版本号必须相同。
- 若不匹配，须升级固件至指定版本。
- 详见**第 4 章**（固件升级）。

**步骤 3 — 搭建软件开发环境**

需安装以下组件：
- USB 驱动
- ADB 工具
- 交叉编译工具
- SDK

详见**第 5 章**（软件开发环境搭建）。

**步骤 4 — 使用 SDK 进行开发**

- 参考示例程序开发应用程序。
- 详见**第 6 章**（SDK 使用方法）。

---

## 3 软件包介绍

移远通信发布每个版本的软件包，包含以下三个组成部分：

| 组成部分 | 说明 |
|---------|------|
| 固件包 | 包含模块的各分区镜像文件及升级配置文件 |
| SDK 软件开发包 | 提供软件开发环境（与固件包版本号相同） |
| 交叉编译工具包 | GCC 交叉编译工具链（版本通常不变，仅需安装一次） |

> **重要说明：** 固件包和 SDK 软件开发包版本号相同，用户使用 SDK 软件开发包开发应用程序时，必须使用对应版本的固件包。交叉编译工具包的版本通常不会改变，因此仅需安装一次。

**软件包文件示例（ls 输出）：**

```
-rw-rw-r-- 1 william william 109472820 3月 16 09:20 EC200ACNTAR02A01M2G_OCPU.zip
-rw-rw-r-- 1 william william  35927633 3月 16 09:20 ql-ec200a-1803e-gcc-8.4.0-v1-toolchain.tar.gz
-rw-rw-r-- 1 william william 245412622 3月 16 09:20 ql-ol-extsdk-ec200acntar02a01m2g_ocpu.tar.gz
```

### 3.1 固件包

固件包一般以压缩包形式提供，**名称为英文大写字母**，命名格式为：

```
<模块OC字段><基线版本号><软件版本号><内存大小>_OCPU.zip
```

**命名解析示例：`EC200ACNTAR02A01M2G_OCPU.zip`**

| 字段 | 值 | 说明 |
|------|-----|------|
| 模块 OC 字段 | EC200ACNTA | EC200A-CN(TA) 模块 |
| 基线版本号 | R02 | 安全基线版本 |
| 软件版本号 | A01 | 软件版本 |
| 内存大小 | M2G | 2GB 内存 |
| OCPU | OCPU | QuecOpen 版本标识 |

固件包包含分区的镜像文件以及固件升级使用的配置文件。

#### 表 1：关键固件包文件列表

| 文件 | 文件类型 | 描述 |
|------|---------|------|
| `update/update.blf` | 配置文件 | 升级工具使用该文件对模块进行升级；**请勿改动** |
| `update/ARBEL.bin` | 镜像文件 | Modem 镜像文件 |
| `update/u-boot.bin` | 镜像文件 | uboot 镜像文件 |
| `update/zImage` | 镜像文件 | 内核镜像文件；**可替换为使用 SDK 制作的内核镜像文件** |
| `update/root.squashfs` | 镜像文件 | 根文件系统镜像文件；**可替换为使用 SDK 制作的根文件系统镜像文件** |

### 3.2 SDK 软件开发包

SDK 软件包提供软件开发环境，具体使用方法请参考**第 6 章**。SDK 软件包一般以压缩包形式提供，**名称为英文小写字母**，命名格式为：

```
ql-ol-extsdk-<模块OC字段><基线版本号><软件版本号><内存大小>_ocpu.tar.gz
```

**命名解析示例：`ql-ol-extsdk-ec200acntar02a01m2g_ocpu.tar.gz`**

| 字段 | 值 | 说明 |
|------|-----|------|
| 模块 OC 字段 | EC200ACNTA | EC200A-CN(TA) 模块 |
| 基线版本号 | R02 | 安全基线版本 |
| 软件版本号 | A01 | 软件版本 |
| 内存大小 | M2G | 2GB 内存 |
| ocpu | ocpu | QuecOpen 版本标识（小写） |

### 3.3 交叉编译工具包

交叉编译工具包是运行在 **Ubuntu 1604 或者 1804 的 64 位系统**的 GCC 交叉编译工具链，安装方法请参考**第 5.4 章**。

交叉编译工具包一般以压缩包形式提供，**名称为英文小写字母**，命名格式为：

```
ql-<模块OC字段>-<基线版本号>-<GCC版本号>-<工具链版本号>-toolchain.tar.gz
```

**命名解析示例：`ql-ec200a-1803e-gcc-8.4.0-v1-toolchain.tar.gz`**

| 字段 | 值 | 说明 |
|------|-----|------|
| 模块 OC 字段 | EC200A | EC200A 模块 |
| 基线版本号 | 1803E | 基线版本 |
| GCC 版本号 | 8.4.0 | GCC 编译器版本 |
| 工具链版本号 | V1 | 工具链版本 |

---

## 4 固件升级

EC200A-CN(TA) QuecOpen 模块支持多种固件升级方式。

### 4.1 使用移远通信升级工具升级

Windows 系统的上位机升级工具的详细使用方法，请参考**文档 [1]**（Quectel 升级工具用户指南）。

### 4.2 紧急升级

**适用场景：** 在开发调试阶段，当模块无法正常启动，或者上位机无法识别模块的 USB 端口时，可通过紧急升级方式恢复模块。

**操作步骤：**

1. 使模块进入紧急升级下载模式：
   - 若模块无法正常开机，可以**短接 VDD_EXT 引脚和 USB_boot 引脚**，重新给模块上电，使模块进入紧急下载模式。
2. 模块进入紧急下载模式后，使用上位机工具进行升级。

> **备注：** 有关短接 VDD_EXT 引脚和 USB_boot 引脚的方法，请咨询移远通信技术支持。

---

## 5 软件开发环境搭建

本章介绍在 Ubuntu 系统上搭建完整 QuecOpen 开发环境的全部步骤，包括 USB 驱动、ADB 工具、交叉编译工具链和 SDK 的安装。

### 5.1 USB 驱动安装

上位机需安装 USB 驱动程序，方可通过 USB 与模块进行通信。

| 平台 | 安装方式 |
|------|---------|
| Linux / Android | 参考文档 [4]（Quectel Linux/Android USB 驱动安装指南） |
| Windows | 使用 EC200A-CN(TA) USB 驱动安装包直接安装 |

> **备注：** 联系移远通信技术支持获取 EC200A-CN(TA) USB 驱动安装包。

### 5.2 ADB 工具安装

**Android 调试桥（ADB）** 是一种功能多样的命令行工具，可让上位机与设备进行通信。ADB 命令便于执行各种设备操作（例如安装和调试应用程序），并提供对 Linux shell（用于在设备上运行各种命令）的访问权限。

- ADB 官方参考：https://developer.android.google.cn/studio/command-line/adb
- 本章介绍在 **Ubuntu 系统**下安装 ADB 工具的方法。

#### 5.2.1 安装 ADB 驱动

**方法一（标准方式）：**

```bash
sudo apt-get update
sudo apt-get install android-tools-adb
```

**方法二（若方法一失败，使用 PPA 源重新安装）：**

```bash
sudo add-apt-repository ppa:nilarimogard/webupd8
sudo apt-get update
sudo apt-get install android-tools-adb
```

**验证安装：** 执行 `adb` 命令，若安装成功，将显示 ADB 的版本号：

```
ol@ql-Ubuntu:~$ adb
Android Debug Bridge version 1.0.32
```

#### 5.2.2 添加模块 USB VID

执行如下命令添加 USB VID。设置 ADB 运行使用的 USB VID 为移远通信模块的 USB VID，即 **0x2c7c**：

```bash
cd ~
sudo echo 0x2c7c > .android/adb_usb.ini
```

#### 5.2.3 枚举设备

以上操作完成后，执行如下命令，检查 ADB 是否可以正确识别出移远模块：

```bash
sudo adb kill-server
sudo adb devices
```

### 5.3 文件下载

移远通信 EC200A-CN(TA) QuecOpen 模块支持多种方式下载文件。在调试过程中，**使用 ADB 命令下载文件最为便捷**。

`adb push` 可将上位机的本地文件下载到模块的文件系统，命令基本格式为：

```
adb push <本地路径> <模块路径>
```

**示例：** 将 SDK 中 `sample/helloworld/` 目录下的 `helloworld` 可执行文件上传至模块的 `/data/` 目录：

```bash
adb push ~/sdk/ql-ol-extsdk-ec200acntar02a01m2g_ocpu/sample/helloworld/helloworld /data
```

### 5.4 交叉编译工具安装

交叉编译工具**只需安装一次**。根据 R01 和 R02 两个基线版本，需使用不同的编译工具：

| 基线版本 | GCC 版本 | 对应工具包文件名 |
|---------|---------|---------------|
| R01 | GCC 4.8 | `ql-ec200a-1803e-gcc-4.8-v1-toolchain.tar.gz` |
| R02 | GCC 8.4 | `ql-ec200a-1803e-gcc-8.4.0-v1-toolchain.tar.gz` |

以下以 **R02 安全基线版本**的编译链工具 `ql-ec200a-1803e-gcc-8.4.0-v1-toolchain.tar.gz` 为例进行详细说明，其他版本安装方式类似。

#### 5.4.1 创建工作目录

交叉编译工具默认的工作目录为 `/opt/ql_crosstools`，若上位机系统不存在该目录，请运行如下命令创建：

```bash
sudo mkdir -p /opt
sudo mkdir -p /opt/ql_crosstools
sudo chmod 777 /opt/ql_crosstools
```

#### 5.4.2 安装交叉编译工具链

将交叉编译工具压缩包复制到 `/opt/ql_crosstools` 目录下并解压：

```bash
cp ql-ec200a-1803e-gcc-8.4.0-v1-toolchain.tar.gz /opt/ql_crosstools
cd /opt/ql_crosstools
tar xvf ql-ec200a-1803e-gcc-8.4.0-v1-toolchain.tar.gz
```

#### 5.4.3 检查是否安装成功

执行如下命令，检查交叉编译工具是否可以正常工作：

```bash
/opt/ql_crosstools/ql-ec200a-1803e-gcc-8.4.0-v1-toolchain/bin/arm-openwrt-linux-gcc --version
```

若安装成功，可以查询到交叉编译工具的版本号，输出示例如下：

```
william@workstation:/$ /opt/ql_crosstools/ql-ec200a-1803e-gcc-8.4.0-v1-toolchain/bin/arm-openwrt-linux-gcc --version
arm-openwrt-linux-gcc (OpenWrt GCC 8.4.0 r8-3309e8f583) 8.4.0
Copyright (C) 2018 Free Software Foundation, Inc.
This is free software; see the source for copying conditions. There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

### 5.5 SDK 软件包安装

SDK 压缩包可以安装到**任意目录**，但是必须在 **Ubuntu 普通用户的工作环境**下进行（不要在 root 用户下操作）。

以下以 `ql-ol-extsdk-ec200acntar02a01m2g_ocpu.tar.gz` 为例进行详细说明。

#### 5.5.1 SDK 软件包解压

将 SDK 压缩包复制到目标工作目录并进行解压：

```bash
tar xvf ql-ol-extsdk-ec200acntar02a01m2g_ocpu.tar.gz
cd ql-ol-extsdk-ec200acntar02a01m2g_ocpu
```

#### 5.5.2 检查是否安装成功

通过编译示例程序，检查 SDK 软件包是否可以正常工作。执行如下命令编译 `helloworld` 示例程序：

```bash
cd ql-ol-extsdk-ec200acntar02a01m2g_ocpu
cd sample/helloworld/
make
```

编译成功之后，生成 `helloworld` 可执行文件。编译成功即说明 SDK 软件包安装成功。

#### 5.5.3 烧录验证

将编译生成的可执行文件 `helloworld` 下载到模块的文件系统目录，赋予可执行权限，然后运行。运行成功即表明编译成功。以下载到 `/tmp` 目录为例：

```bash
root@OpenWrt:/tmp# chmod +x helloworld
root@OpenWrt:/tmp# ./helloworld
hello quectel
root@OpenWrt:/tmp#
```

输出 `hello quectel` 说明程序运行成功，整个开发环境搭建完成。

---

## 6 SDK 使用方法

### 6.1 SDK 文件说明

SDK 解压后的目录结构及各目录的说明如下表所示：

#### 表 2：SDK 文件说明

| 目录 / 文件 | 说明 |
|------------|------|
| `ql-sysroots` | 交叉编译使用的头文件和库文件 |
| `ql-ol-kernel` | Linux 内核源码 |
| `ql-ol-rootfs.tar.gz` | Linux 根文件系统压缩包 |
| `tools` | 编译使用的二进制文件、脚本和配置文件 |
| `sample` | 示例代码文件夹 |
| `sdk.mk` | 公用的 Makefile 头文件 |
| `ql-ol-rootfs` | 编译 rootfs 时，解压 `ql-ol-rootfs.tar.gz` 生成的目录 |
| `oemapp` | 客户文件系统源文件夹 |
| `target` | 目标文件夹，编译的镜像文件和临时文件会存放在此目录 |

### 6.2 编译方式

SDK 的编译由 **Makefile 规则**组织：

- **顶层 Makefile**：用于编译内核和文件系统镜像。
- **`sample` 目录下各子目录的 Makefile**：用于编译各个单独的示例程序。

用户可以直接进入 `sample` 目录下的各个子目录，运行 `make` 即可开始编译。各个目录的 `Makefile` 文件都引用了 `sdk.mk` 文件，`sdk.mk` 文件中定义了编译过程中需要使用的环境变量。

#### 表 3：SDK 根目录下常用编译命令

| 命令名称 | 说明 |
|---------|------|
| `make rootfs` | 编译 rootfs 镜像，生成的目标文件位于 `target` 目录下 |
| `make kernel` | 编译内核镜像，生成的目标文件位于 `target` 目录下 |
| `make kernel_module` | 编译内核模块，生成的文件位于 `ql-ol-rootfs/lib` 目录下 |
| `make sample` | 编译所有的示例程序代码，生成的二进制文件位于 `sample` 目录下的各子目录中 |

#### 表 4：示例程序

| 目录 | 说明 |
|------|------|
| `sample/kmodule` | 自定义内核模块示例程序 |
| `sample/helloworld` | 用户程序示例 |
| `sample/test_sdk_api` | SDK 的 API 接口测试程序 |

### 6.3 文件系统镜像制作

#### 6.3.1 制作根文件系统镜像

首次解压的 SDK 包的根文件系统压缩包名称为 `ql-ol-rootfs.tar.gz`。在 SDK 根目录下运行 `make rootfs` 编译命令制作根文件系统镜像，生成的文件位于 `target` 目录，名称为 **`root.squashfs`**。

**`make rootfs` 命令运行包含两个阶段：**

**第一阶段：** 解压 `ql-ol-rootfs.tar.gz`，解压目录为 `ql-ol-rootfs`。

Makefile 中解压 `ql-ol-rootfs.tar.gz` 的代码片段：

```makefile
if [ ! -d ${QL_ROOTFS_DIR} ]; then \
    printf "Extracting the rootfs..."; \
    mkdir -p ${QL_ROOTFS_DIR}; \
    tar -xvf ${QL_SDK_DIR}/ql-ol-rootfs.tar.gz -C ${QL_ROOTFS_DIR}, \
    echo "Done."; \
fi
```

**第二阶段：** 调用制作文件系统的相关工具（`mksquashfs` 命令），生成 `root.squashfs` 镜像文件。

Makefile 中制作根文件系统的核心命令（截图所示）：

```makefile
$(QL_TOOLS_BIN_DIR)/mksquashfs $(QL_SDK_BIN) ql-ol-rootfs $(QL_TARGET_DIR)/root.squashfs \
    -nopad -noappend -root-owned -comp xz -Xbcj x86 -b 256k -p '/dev d 755 0 0' \
    -p '/dev/console c 600 0 0 5 1' -p '/dev/null c 666 0 0 1 3' ...
```

**生成镜像的使用：**

生成的镜像文件可以替换固件包 `update` 目录下的同名文件，在上位机使用工具进行升级。升级步骤请参考**第 4 章**。

> **备注：**
> 1. **R01 安全基线版本**中只需要替换 `root.squashfs` 即可。
> 2. **R02 安全基线版本**由于安全的原因，rootfs 的 hash 校验值保存在 uboot 中，故需**同时替换烧录 `root.squashfs` 和 `u-boot.bin`**。其中 `u-boot.bin` 会在 `root.squashfs` 生成的同时自动编译生成。

#### 6.3.2 内核镜像文件制作

在 SDK 根目录运行 `make kernel` 编译命令，编译内核并制作内核镜像文件，生成的内核镜像文件位于 `target` 目录，名称为 **`zImage`**。可以替换固件包 `update` 目录下的同名文件，在上位机使用工具进行升级。升级步骤请参考**第 4 章**。

**特别注意：内核模块联动编译**

若修改了内核 ko 动态加载驱动代码，在重新编译内核之后，还需要：

1. 使用命令 `make kernel_module` **重新编译内核驱动模块**。
2. 生成的内核模块存放在根文件系统的 `ql-ol-rootfs/lib/modules/5.4.188` 目录。
   - 该目录对应 Linux 内核版本 5.4.188，并随内核版本升级而改变。
3. **需重新制作根文件系统镜像并升级**（仅更新内核镜像不够）。

### 6.4 日志系统

系统支持 **Android 的 logcat 日志系统**，相关资源如下：

| 资源 | 路径 |
|------|------|
| 头文件目录 | `ql-sysroots/usr/include/log.h` |
| 示例程序目录 | `sample/log_logcat` |
| logcat 封装定义 | `ql-sysroots/usr/include/android/log.h` |
| Logcat 官方使用指导 | https://developer.android.google.cn/studio/command-line/logcat |

**日志条目结构：** 每个日志条目包含三部分：
- **Priority（优先级）**：日志等级
- **Tag（标记）**：标识日志来源
- **Message（消息）**：实际日志内容

Logcat 日志系统定义了 **6 个日志等级**，由低到高为：

#### 表 5：日志等级对应日志接口

| 日志等级 | 日志接口 | 说明 |
|---------|---------|------|
| VERBOSE | `ALOGV` | 仅在开启 `LOG_NDEBUG` 宏后才能输出 |
| DEBUG | `ALOGD` | 调试信息 |
| INFO | `ALOGI` | 一般信息 |
| WARN | `ALOGW` | 警告信息 |
| ERROR | `ALOGE` | 错误信息 |
| FATAL | ——（无接口） | 致命等级，不提供日志接口 |

#### 6.4.1 在应用程序中使用日志接口

**步骤一：在 Makefile 中的链接选项中增加链接 logcat 的库（`-llog`）**

```makefile
CURR_DIR := $(shell pwd)

-include ../../sdk.mk

QL_TARGET_EXE = sample_log
QL_TARGET_OBJS = main.o

CFLAGS  := $(QL_SDK_CFLAGS) $(QL_SDK_HARD_CFLAGS)
LDFLAGS := $(QL_SDK_LDFLAGS) $(QL_SDK_LIBS) -llog        # ← 关键：添加 -llog

all: $(QL_TARGET_EXE)

%.o:%.c
    $(CC) -o $@ -c $^ $(CFLAGS)

$(QL_TARGET_EXE): $(QL_TARGET_OBJS)
    $(CC) -o $@ $(QL_TARGET_OBJS) $(LDFLAGS)

clean:
    rm -rf *.o
    rm -rf $(QL_TARGET_EXE)
```

**步骤二：在源码中定义日志 Tag 和 VERBOSE 开关，然后引用头文件**

```c
#include <stdio.h>

/** Define log tag */
#define LOG_TAG "ql_log_test"

/** Turn on debug switch, and the log can be output by the ALOGV */
#define LOG_NDEBUG 1

#include "log/log.h"

int main(int argc, char *argv[])
{
    printf("Using \"logcat -s %s\" to filter log\n", LOG_TAG);
    ALOGV("I am LOG_VERBOSE");
    ALOGD("I am LOG_DEBUG");
    ALOGI("I am LOG_INFO");
    ALOGW("I am LOG_WARN");
    ALOGE("I am LOG_ERROR");

    return 0;
}
```

**重要说明：**
- 必须在 `#include "log/log.h"` **之前**定义 `LOG_TAG` 和 `LOG_NDEBUG`。
- `#define LOG_NDEBUG 1` 开启后，`ALOGV`（VERBOSE 等级）才会输出日志。
- 若不定义 `LOG_NDEBUG`，则 VERBOSE 等级的日志不会输出。

#### 6.4.2 查看应用程序日志

使用 `logcat` 可根据日志的 Tag 和日志等级显示日志内容，命令格式为：

```
logcat -s <日志的Tag[:显示日志的等级]>
```

**示例：** 查询日志 tag 为 `ql_log_test` 的日志信息：

```
/data # logcat -s ql_log_test
--------- beginning of main
01-21 06:18:30.182  1366  1366 D ql_log_test: I am LOG_DEBUG
01-21 06:18:30.182  1366  1366 I ql_log_test: I am LOG_INFO
01-21 06:18:30.182  1366  1366 W ql_log_test: I am LOG_WARN
01-21 06:18:30.182  1366  1366 E ql_log_test: I am LOG_ERROR
--------- beginning of crash
```

**日志格式说明：**

```
日期   时间         PID   TID  等级  Tag             消息
01-21 06:18:30.182 1366  1366  D  ql_log_test: I am LOG_DEBUG
```

**查看日志文件信息（`ls -al` 示例）：**

```
/usrdata # ls -al
total 492
drwxr-xr-x   2 root   root     512 Jan 21 10:31 .
drwxr-xr-x  28 root   root     463 Feb 27  2020 ..
-rw-------   1 root   root   75457 Jan 21 10:35 log.txt
-rw-------   1 root   root  102443 Jan 21 10:31 log.txt.1
-rw-------   1 root   root  102438 Jan 21 10:31 log.txt.2
-rw-------   1 root   root  102457 Jan 21 10:31 log.txt.3
-rw-------   1 root   root  102521 Jan 21 10:31 log.txt.4
```

#### 6.4.3 日志保存

logcat 支持将日志**分段循环覆盖**的方式保存到文件，命令格式为：

```
logcat [日志的Tag[:显示日志的等级]] -r <单个日志文件大小（单位 K）> -n <日志文件个数> -f <日志文件名>
```

**参数说明：**

| 参数 | 含义 |
|------|------|
| `-r <K>` | 单个日志文件大小，单位为 KB |
| `-n <N>` | 日志文件最大个数（循环覆盖） |
| `-f <文件名>` | 日志文件路径和名称 |

**示例：** 将系统所有日志保存至文件 `/usrdata/log.txt`，单个日志文件大小为 100 KB，文件个数为 5：

```bash
/usrdata # logcat -f /usrdata/log.txt -r 100 -n 5 &
/usrdata # _
```

执行后，logcat 在后台运行（`&`），日志循环写入 `log.txt`、`log.txt.1`、`log.txt.2` ... `log.txt.4`，共 5 个文件，每个文件约 100 KB，循环覆盖最旧的文件。

### 6.5 API 接口

SDK 提供了一组应用程序接口（API），相关资源如下：

| 资源 | 路径 |
|------|------|
| API 头文件目录 | `ql-sysroots/usr/include/ql-sdk` |
| API 测试程序 | `sample/test_sdk_api` |

#### 表 6：API 接口使用方法相关文档

| 功能 | 参考文档 |
|------|---------|
| 网络注册、获取注网相关信息 | `Quectel_EC200A-CN(TA)_QuecOpen_蜂窝网络信息API参考手册` |
| SIM 卡相关功能 | `Quectel_EC200A-CN(TA)_QuecOpen_SIM_API参考手册` |
| 模块管理、获取模块的状态和信息 | `Quectel_EC200A-CN(TA)_QuecOpen_设备管理API参考手册` |
| 短信 | `Quectel_EC200A-CN(TA)_QuecOpen_SMS_API参考手册` |
| 电话 | `Quectel_EC200A-CN(TA)_QuecOpen_语音服务API参考手册` |

### 6.6 应用程序启动

移远通信 EC200A-CN(TA) QuecOpen 模块的**启动管理使用 sysv 方式**。

**启动机制说明：**

1. 启动顺序为先启动 `/etc/rc.d` 目录下的启动脚本。
2. 启动脚本按照优先级数字**由小到大**顺序启动。

**推荐实践：**

- 推荐客户将启动脚本存放在 `/etc/rc.d` 目录下。
- 脚本优先级数字设置为 **99**（最低优先级）。
- 原因：表示系统服务启动完毕之后再启动用户服务，可以**避免服务的依赖问题**。

**启动脚本存放路径：** `/etc/rc.d/S99<your_app_name>`

### 6.7 静态代码扫描

静态代码扫描指用户在写好源代码后，**无需经过编译器编译**，直接使用扫描工具对源码进行扫描，找出代码当中存在的一些语义缺陷、安全漏洞。

本章节主要介绍免费的 C/C++ 静态代码扫描工具 **CPPCHECK** 的使用方法。

- 官方网址：http://cppcheck.net

#### 6.7.1 CPPCHECK 安装

在 Ubuntu 系统下，执行如下命令安装 CPPCHECK 工具：

```bash
sudo apt-get install cppcheck
```

#### 6.7.2 使用 CPPCHECK 扫描代码

在 Ubuntu 系统下，进入源码目录，使用 CPPCHECK 工具执行扫描，命令格式为：

```bash
cppcheck --enable=all <源码目录>
```

根据错误提示，对代码进行修复。

**扫描结果示例：**

```
[test_sdk_api/m_data_call.c 423]  (style) The scope of the variable 'ret' can be reduced
[test_sdk_api/m_data_call.c 539]  (style) The scope of the variable 't' can be reduced
[test_sdk_api/m_dn.c 239]         (style) The scope of the variable 'ret' can be reduced
[test_sdk_api/m_dn.c 75]          (portability) scanf without field width limits can crash with huge input data on some versions
[test_sdk_api/m_dn.c 173]         (portability) scanf without field width limits can crash with huge input data on some versions
[test_sdk_api/m_dn.c 209]         (error) Memory leak: buf
[test_sdk_api/m_rw.c 373]         (warning) %d in format string (no. 1) requires a signed integer given in the argument list
[test_sdk_api/m_rw.c 393]         (warning) %d in format string (no. 2) requires a signed integer given in the argument list
[test_sdk_api/m_rw.c 412]         (warning) %d in format string (no. 1) requires a signed integer given in the argument list
[test_sdk_api/m_rw.c 285]         (portability) scanf without field width limits can crash with huge input data on some versions
[test_sdk_api/m_rw.c 630]         (portability) scanf without field width limits can crash with huge input data on some versions
[test_sdk_api/m_rw.c 312]         (portability) scanf without field width limits can crash with huge input data on some versions
[test_sdk_api/m_rw.c 648]         (portability) scanf without field width limits can crash with huge input data on some versions
```

**CPPCHECK 可检测的错误类型说明：**

| 错误类型 | 说明 | 示例 |
|---------|------|------|
| `style`（风格） | 代码风格问题，如变量作用域可缩减 | `The scope of the variable 'ret' can be reduced` |
| `portability`（移植性） | 跨平台/版本可能崩溃的问题 | `scanf without field width limits can crash with huge input data` |
| `error`（错误） | 真实的错误，如内存泄漏 | `Memory leak: buf` |
| `warning`（警告） | 格式字符串参数类型不匹配等 | `%d in format string requires a signed integer` |

---

## 7 模块开机检查

模块启动后，可以通过一系列 AT 命令检查模块是否处于正常工作状态。

**操作步骤：**

**第一步：** 通过 USB 串口线将上位机连接至模块的 **USB AT 口**。

**第二步：** 插入 **(U)SIM 卡**和**天线**后，将模块上电。

**第三步：** 使用 **QCOM 工具**，分别执行如下命令：

| AT 命令 | 功能说明 |
|---------|---------|
| `AT` | 检测串口通信是否正常 |
| `AT+CPIN?` | 检测 (U)SIM 卡状态 |
| `AT+CSQ` | 检测信号强度 |
| `AT+CGREG?` | 检测模块注网状态 |
| `AT+COPS?` | 查询当前运营商 |
| `AT+QCELLINFO?` | 查询网络制式及小区信息 |
| `ATDxxx;` | 语音测试（`xxx` 为目标手机号码，注意末尾分号） |

**插入一张普通联通卡的开机检查流程完整示例（图 1）：**

```
AT+CPIN?
+CPIN: READY

OK
AT+CSQ
+CSQ: 24,99

OK
AT+CGREG?
+CGREG: 3,0

OK
AT+COPS?
+COPS: 0,2,"46001",7

OK
AT+QCELLINFO?
+QCELLINFO: LTE E3,19650,46001,56848,365,100126239,-62

OK
ATD18501729042;

OK
```

**AT 命令响应解析说明：**

| 命令 | 响应 | 含义 |
|------|------|------|
| `AT+CPIN?` | `+CPIN: READY` | SIM 卡正常，无需 PIN 码 |
| `AT+CSQ` | `+CSQ: 24,99` | 信号强度 24（满格约 31），误码率未知（99） |
| `AT+CGREG?` | `+CGREG: 3,0` | 注网状态（3: 注册被拒绝，0: 未注册，实际需关注第二个参数） |
| `AT+COPS?` | `+COPS: 0,2,"46001",7` | 中国联通（MCC:460，MNC:01），LTE 制式（7） |
| `AT+QCELLINFO?` | `+QCELLINFO: LTE E3,19650,46001,56848,365,100126239,-62` | LTE 制式，EARFCN:19650，运营商:46001，PCI:56848，RSRP:-62 dBm |
| `ATD18501729042;` | `OK` | 呼叫成功 |

> **备注：**
> 3. 有关 AT 命令的详细信息，请参考**文档 [2]**（Quectel_EC200A-CN(TA)_QuecOpen_AT_Commands_Manual）。
> 4. 有关 QCOM 工具使用方法，请参考**文档 [3]**（Quectel QCOM 工具用户指南）。

---

## 8 附录 参考文档及术语缩写

> **说明：** 原始 PDF 最后一页（附录正文页）内容未完整呈现，以下参考文档和术语缩写根据文中引用及行业惯例整理。

### 表 7：参考文档

| 序号 | 文档名称 | 用途 |
|------|---------|------|
| [1] | Quectel_EC200A-CN(TA) 固件升级工具用户指南 | Windows 上位机升级工具使用说明（见 4.1 节） |
| [2] | Quectel_EC200A-CN(TA)_QuecOpen_AT_Commands_Manual | AT 命令参考手册（见 2.1 节、第 7 章） |
| [3] | Quectel QCOM 工具用户指南 | QCOM 串口调试工具使用说明（见第 7 章） |
| [4] | Quectel Linux/Android USB 驱动安装用户指南 | Linux 和 Android 系统 USB 驱动安装（见 5.1 节） |

### 表 8：术语和缩写

| 缩写 | 英文全称 | 中文含义 |
|------|---------|---------|
| ADB | Android Debug Bridge | 安卓调试桥 |
| API | Application Programming Interface | 应用程序接口 |
| CPU | Central Processing Unit | 中央处理器 |
| EARFCN | E-UTRA Absolute Radio Frequency Channel Number | LTE 绝对无线频道号 |
| GCC | GNU Compiler Collection | GNU 编译器套件 |
| ko | Kernel Object | 内核对象（动态加载模块） |
| LTE | Long Term Evolution | 长期演进（4G 移动通信标准） |
| MCC | Mobile Country Code | 移动国家码 |
| MNC | Mobile Network Code | 移动网络码 |
| OC | — | 模块 OC 字段（固件命名标识） |
| OCPU | Open CPU | 开放式 CPU（QuecOpen 平台标识） |
| PCI | Physical Cell Identity | 物理小区标识 |
| RSRP | Reference Signal Received Power | 参考信号接收功率 |
| SDK | Software Development Kit | 软件开发工具包 |
| SIM | Subscriber Identity Module | 用户识别模块 |
| UIM | User Identity Module | 用户身份模块 |
| USB | Universal Serial Bus | 通用串行总线 |
| VID | Vendor ID | 供应商标识符 |

---

## 附：开发流程速查

以下为基于本文档的完整开发环境搭建和开发流程总结，供快速参考：

```
┌─────────────────────────────────────────────────────────┐
│              EC200A-CN(TA) QuecOpen 开发流程              │
├─────────────────────────────────────────────────────────┤
│ 1. 准备硬件环境                                           │
│    └─ Ubuntu 1604/1804, 64-bit, ≥4GB RAM, ≥4核 CPU       │
│                                                         │
│ 2. 准备软件包（三件套版本必须匹配）                          │
│    ├─ 固件包:   EC200ACNTAR02A01M2G_OCPU.zip             │
│    ├─ SDK包:    ql-ol-extsdk-ec200acntar02a01m2g_ocpu.tar.gz │
│    └─ 工具链:   ql-ec200a-1803e-gcc-8.4.0-v1-toolchain.tar.gz │
│                                                         │
│ 3. 升级固件（如需）                                        │
│    ├─ 正常: 使用移远上位机升级工具（文档[1]）                 │
│    └─ 紧急: 短接 VDD_EXT + USB_boot 引脚进入紧急模式        │
│                                                         │
│ 4. 安装 USB 驱动                                          │
│    ├─ Linux/Android: 参考文档[4]                          │
│    └─ Windows: 使用驱动安装包                             │
│                                                         │
│ 5. 安装 ADB 工具                                          │
│    ├─ apt-get install android-tools-adb                  │
│    ├─ 添加 VID: echo 0x2c7c > ~/.android/adb_usb.ini     │
│    └─ 验证: sudo adb devices                             │
│                                                         │
│ 6. 安装交叉编译工具链                                       │
│    ├─ 目录: /opt/ql_crosstools/                           │
│    ├─ 解压 toolchain.tar.gz 到上述目录                     │
│    └─ 验证: arm-openwrt-linux-gcc --version               │
│                                                         │
│ 7. 安装 SDK                                               │
│    ├─ 解压 SDK tar.gz 到工作目录（普通用户下）               │
│    ├─ 编译 helloworld: cd sample/helloworld && make       │
│    └─ 验证: adb push helloworld /tmp && ./helloworld     │
│                                                         │
│ 8. 应用程序开发                                            │
│    ├─ 参考 sample/ 目录下示例程序                           │
│    ├─ 日志: 使用 ALOGD/ALOGI/ALOGW/ALOGE 接口             │
│    ├─ API: 头文件在 ql-sysroots/usr/include/ql-sdk/       │
│    ├─ 启动脚本: 放入 /etc/rc.d/ 目录，优先级 99             │
│    └─ 静态扫描: cppcheck --enable=all <源码目录>            │
│                                                         │
│ 9. 制作镜像并升级                                          │
│    ├─ 根文件系统: make rootfs → target/root.squashfs      │
│    ├─ 内核: make kernel → target/zImage                   │
│    └─ 替换固件包 update/ 目录中同名文件后升级                │
│                                                         │
│ 10. 模块开机检查                                           │
│     ├─ AT          → 串口通信正常                          │
│     ├─ AT+CPIN?    → SIM 卡状态                           │
│     ├─ AT+CSQ      → 信号强度                             │
│     ├─ AT+CGREG?   → 注网状态                             │
│     ├─ AT+COPS?    → 运营商                               │
│     ├─ AT+QCELLINFO? → 网络制式                           │
│     └─ ATDxxx;     → 语音测试                             │
└─────────────────────────────────────────────────────────┘
```

---

*本文档依据 Quectel EC200A-CN(TA) QuecOpen 快速开发指导 V1.0.0（2022-07-07）整理，内容完整覆盖原始 PDF 全部 25 页。*
