# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) 快速开发指导 V1.0 — 全文详尽分析

> **源文档全名**：AG35-CET&AG35-EUT QuecOpen(SDK) 快速开发指导
> **文档版本**：V1.0（受控版本）
> **发布日期**：2025-05-29
> **作者**：William LIU
> **总页数**：28 页（PDF 页脚标注 1/27 ~ 26/27，即正文 27 编号页 + 封面共 28 物理页）
> **本分析覆盖范围**：全部 28 页（第 1~8 章 + 文档历史 + 索引 + 附录）
> **适用产品**：移远通信 AG35-CET、AG35-EUT 模块（LTE Standard 模块系列），QuecOpen® 方案（基于 Linux 的嵌入式开发平台）

---

## 文档定位与背景

本文档是 AG35-CET / AG35-EUT 模块基于 **QuecOpen® (SDK)** 方案的"快速开发入门"指导，目标是带开发者走通从环境搭建到应用编译、烧录、开机检查的完整最小闭环。它不是 API 手册，而是把固件升级、开发环境准备、SDK 使用、开机检查这四大流程串起来的"路线图"，具体的 API / AT / 外设细节都通过参考文档（`文档[1]`~`文档[N]`）外链。

QuecOpen® 是移远的开放式 CPU（OpenCPU）方案：应用程序与 modem 协议栈跑在同一颗 SoC 的 Linux 上，简化 IoV（车联网）应用的软件设计开发。这与本仓库 modem_mng 中 AG35 走 EC200A 源码路径、同属 OpenCPU 的事实一致（CLAUDE.md 中提到 EC200A 是 OpenCPU，`cfun=1,1` 会复位整个 SoC 含 Linux）。

### 文档历史（修订记录）

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| -（草稿） | 2023-06-10 | William LIU | 文档创建 |
| 1.0.0 | 2023-06-10 | William LIU | 临时版本 |
| 1.0.1 | 2024-08-09 | William LIU | 临时版本：①基于 QuecOpen 方案统一命名，更新文档名称；②更新交叉编译工具链（第 3.3 章、第 5.4 章）；③更新 API 接口参考文档（第 6.5 章）；④新增外设接口参考文档（第 6.6 章） |
| 1.0 | 2025-05-29 | William LIU | 受控版本 |

> **解读**：本仓库 md/pdf/ag35/ 下同时存有 V1.0 与 V1.0.1（_Preliminary_20240809）两份"快速开发指导"。V1.0（本文档，2025-05-29 受控）实际上是在 V1.0.1（2024-08-09 临时）之后转正的"受控版本"——版本号回到 1.0 是 Quectel 受控文档的命名习惯（临时版用三段号 x.y.z，转正用两段号 x.y）。两份内容主体一致，本文档为正式受控基线。

---

## 第 1 章 引言

- AG35-CET 和 AG35-EUT 模块支持 **QuecOpen®** 方案。QuecOpen® 是基于 Linux 的嵌入式开发平台，可简化 IoV 应用的软件设计和开发过程。
- 本文档适用于 **SDK 构建环境** 的 QuecOpen® 方案（区别于其它构建方式），主要介绍快速开发流程，涵盖四大块：
  1. 固件升级
  2. 软件开发环境准备
  3. SDK 的使用
  4. 模块开机检查

> **解读**："SDK 构建环境的 QuecOpen 方案"措辞暗示 AG35 可能还有其它（非 SDK）构建方式；本文档只覆盖 SDK 方式。AG35-CET 与 AG35-EUT 在本文档中始终成对出现、共用一套流程，差异仅在型号字段（见第 3 章命名规则），开发流程完全相同。

---

## 第 2 章 QuecOpen® 软件开发概述

### 2.1 对开发者的要求

1. 熟悉标准 **GNU/Linux 应用开发**，以及常见 Linux 系统命令。
2. 掌握一些 **驱动、网络协议** 基本知识。
3. 了解模块的 **AT 命令** 知识（所需 AT 命令详情需咨询移远技术支持——意味着 AT 手册不随本文档附带）。

### 2.2 开发流程（四步）

1. **准备硬件开发环境**，对上位机（开发主机）的硬性要求：
   - 系统需为 **Ubuntu 18.04 的 64 位系统**（强约束，下文工具链均针对此环境）
   - 内存 **4 GiB 以上**
   - CPU **4 核以上**
   - 若使用虚拟机，分配给虚拟机的内存 **不低于 4 GiB**
2. **确认固件版本和 SDK 版本匹配**，若不匹配，升级固件至指定版本（详见第 4 章）。
3. **准备软件开发环境**，需安装：USB 驱动、ADB 工具、交叉编译工具及 SDK（详见第 5 章）。
4. **使用 SDK 进行开发**（参考示例程序开发应用程序，详见第 6 章）。

> **关键约束**：开发主机锁定 Ubuntu 18.04 x64。交叉工具链是 gcc-8.4.0 / glibc，为该环境编译。固件与 SDK 版本必须严格匹配（同一版本号），这是 QuecOpen 的硬规则——见第 3 章"固件包和 SDK 版本号相同"。

---

## 第 3 章 软件包介绍

每个发布版本的软件包均包含三件套：**固件包、SDK、交叉编译工具包**。其中：
- **固件包和 SDK 版本号相同**——用 SDK 开发应用时，必须使用对应版本的固件包（版本绑定）。
- **交叉编译工具包版本通常不变**，因此 **仅需安装一次**。

图 1（AG35-CET 软件包）展示三个文件的实际样例（含大小，反映典型体量）：

| 文件名 | 类型 | 大小（约） | 角色 |
|---|---|---|---|
| `AG35CETCAR01A07M2G_OCPU.zip` | ZIP | 126,345 KB（≈123 MB） | 固件包 |
| `ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain.tar.gz` | GZ | 362,231 KB（≈354 MB） | 交叉编译工具包 |
| `ql-ol-extsdk-ag35cetcar01a07m2g_ocpu.tar.gz` | GZ | 300,214 KB（≈293 MB） | SDK 软件开发包 |

### 3.1 固件包

固件包以压缩包形式提供，名称为英文大写字母，命名格式：

```
<模块OC字段><基线版本号><软件版本号><内存大小>_OCPU.zip
```

以 `AG35CETCAR01A01M2G_OCPU.zip` 为例，版本信息分解：

| 字段 | 取值 | 含义 |
|---|---|---|
| 模块 OC 字段 | `AG35CETCA` | AG35-CET 模块 |
| 基线版本号 | `R01` | 基线 R01 |
| 软件版本号 | `A01` | 软件版本 A01 |
| 内存大小 | `M2G` | 2 GB 内存 |
| OCPU | QuecOpen 版本标识 | — |

固件包包含**分区的镜像文件**以及**固件升级使用的配置文件**，关键文件列表（表 1）：

| 文件 | 文件类型 | 描述 |
|---|---|---|
| `update.blf` | 配置文件 | 移远升级工具使用该文件对模块进行升级，**请勿改动** |
| `ARBEL.bin` | 镜像文件 | **modem 镜像文件**（协议栈固件） |
| `u-boot.bin` | 镜像文件 | **uboot 镜像**；可替换为使用 SDK 制作的 uboot 镜像 |
| `zImage` | 镜像文件 | **内核镜像**；可替换为 SDK 制作的内核镜像 |
| `root.squashfs` | 镜像文件 | **根文件系统镜像**（squashfs 格式）；可替换为 SDK 制作的 rootfs 镜像 |
| `oemapp.squashfs` | 镜像文件 | **客户应用镜像**（oemapp 分区）；可替换为 SDK 制作的客户自己的应用镜像 |

> **解读（重要）**：这张表是整个开发闭环的核心——SDK 制作出的 4 类镜像（`zImage`、`root.squashfs`、`oemapp.squashfs`、`u-boot.bin`）正是用来**替换固件包 update 目录下同名文件**，再用升级工具刷机。`ARBEL.bin`（modem 协议栈）和 `update.blf`（升级配置）不可由 SDK 替换/改动。`oemapp` 分区是放客户应用的专属分区，对应第 6.3.3 节的 `make oemapp`。这与 modem_mng 把 modem_mng 守护进程部署到目标机的实践相印证：客户程序最终进 oemapp 分区。

### 3.2 SDK

SDK 提供软件开发环境（用法见第 6 章）。以压缩包形式提供，名称为英文小写字母，命名格式：

```
ql-ol-extsdk-<模块OC字段><基线版本号><软件版本号><内存大小>_ocpu.tar.gz
```

以 `ql-ol-extsdk-ag35cetcar01a01m2g_ocpu.tar.gz` 为例：

| 字段 | 取值 | 含义 |
|---|---|---|
| 模块 OC 字段 | `AG35CETCA` | AG35-CET 模块 |
| 基线版本号 | `R01` | — |
| 软件版本号 | `A01` | — |
| 内存大小 | `M2G` | 2 GB |
| OCPU | QuecOpen 版本 | — |

> **解读**：SDK 命名与固件包**字段一一对应、仅大小写不同**（固件大写、SDK 小写，前缀 `ql-ol-extsdk-`，后缀 `_ocpu.tar.gz`）。这就是"版本号相同、必须配套"的具体体现——肉眼即可核对固件与 SDK 是否同版本。

### 3.3 交叉编译工具包

- 是运行在 **Ubuntu 18.04 64 位系统** 的 **GCC 交叉编译工具链**（安装方法见第 5.3 章）。
- 以压缩包形式提供，名称为英文小写字母，命名格式：

```
ql-<模块型号部分字段>-<基线版本号>-<GCC版本号>-<工具链版本号>-toolchain.tar.gz
```

以 `ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain.tar.gz` 为例：

| 字段 | 取值 | 含义 |
|---|---|---|
| 模块型号部分字段 | `AG35` | AG35-CET 模块 |
| 基线版本号 | `1806E` | — |
| GCC 版本号 | `gcc-8.4.0` | GCC 8.4.0 |
| 工具链版本号 | `v1` | — |

> **解读**：工具链名里还隐含 `glibc`（C 库为 glibc，非 musl）。这一点与本仓库记忆 [[ag35-ledcontrol-sigill-workaround]] 直接相关——AG35 调 libledcontrol 触发 SIGILL，曾排查是否 musl/glibc 不匹配，本文档确认 AG35 工具链就是 **gcc-8.4.0 + glibc**，印证了"非 musl/glibc 问题"的排除结论。工具链版本通常不变，故全生命周期只装一次。

---

## 第 4 章 固件升级

AG35-CET / AG35-EUT 支持多种固件升级方式，本章介绍三种。

### 4.1 使用移远通信升级工具升级

- **Windows 系统** 的上位机升级工具详细用法 → 参考 **文档[1]**。
- **Linux 和 Android 系统** 的上位机升级工具详细用法 → 参考 **文档[2]**。

### 4.2 A/B 系统升级

- 模块 **A/B 系统升级**方法 → 参考 **文档[3]**。

> **解读**：A/B 双系统（无缝 OTA）是该模块的能力——一个分区运行、另一个分区接收更新，升级失败可回滚。这是车规/IoV 场景对升级可靠性的常见要求。

### 4.3 紧急升级

开发调试阶段，**模块无法正常启动**时的救砖手段：

1. 将 **`USB_BOOT` 上拉至 `VDD_EXT` 引脚**，重新给模块上电，使模块进入**强制下载模式**。
2. 使用上位机工具进行升级。

> **备注**：将 `USB_BOOT` 上拉至 `VDD_EXT` 的方法 → 参考 **文档[4]** 或咨询移远技术支持。

> **解读**：这是硬件强制下载（EDL/forced download）模式，类似高通 9008。当 rootfs/内核被刷坏导致无法启动时，靠拉高 USB_BOOT 引脚进入 bootrom 下载模式刷机。对照 modem_mng 的 L3 恢复策略（纯 `exit(1)` 依赖 watchdog 重启，避免 `cfun=1,1` 复位整个 SoC）——本文档的紧急升级是更底层的"砖了之后"救援，而 L3 是"运行期网络挂了"的软恢复，两者层次不同。

---

## 第 5 章 软件开发环境准备

在用 AG35-CET / AG35-EUT 做软件开发前，需完成四项准备：①安装 USB 驱动 ②安装 ADB 工具 ③安装交叉编译工具 ④安装 SDK。

### 5.1 USB 驱动安装

上位机需安装 USB 驱动，方可通过 USB 与模块通信。
- **Linux 和 Android** 系统安装 USB 驱动详细步骤 → 参考 **文档[5]**。
- **Windows** 系统可通过 USB 驱动安装包直接安装；如需安装包，联系移远技术支持获取。

### 5.2 ADB 工具安装

ADB（Android Debug Bridge，Android 调试桥）是功能多样的命令行工具，用于上位机与设备间通信。ADB 命令便于执行各种设备操作（如安装和调试应用），并提供对 Linux shell（在设备上运行命令）的访问权限。本章介绍 Ubuntu 系统下安装 ADB。

> **备注**：ADB 官方用户指南 → `https://developer.android.google.cn/studio/command-line/adb`

#### 5.2.1 安装 ADB 驱动

1) 在上位机 shell 执行：
```bash
sudo apt-get update
sudo apt-get install android-tools-adb
```

2) 若上述执行失败，改用如下命令重装 ADB 驱动；若上面成功则跳过此步：
```bash
sudo add-apt-repository ppa:nilarimogard/webupd8
sudo apt-get update
sudo apt-get install android-tools-adb
```

3) 执行 `adb` 查看是否安装成功，成功则显示版本号，示例：
```
ol@ql-Ubuntu:~$ adb
Android Debug Bridge version 1.0.32
```

#### 5.2.2 添加模块 USB VID

执行如下命令添加 USB VID，将 ADB 运行使用的 USB VID 设为移远模块的 USB VID，即 **`0x2c7c`**：
```bash
cd ~
sudo echo 0x2c7c > .android/adb_usb.ini
```

> **解读**：`0x2c7c` 是 Quectel 的 USB Vendor ID（全系移远模块通用）。ADB 默认只认 Google 等已知 VID，必须把 Quectel VID 写进 `~/.android/adb_usb.ini` 才能枚举到 AG35。注意原文 `sudo echo ... > 文件` 这种写法实际上重定向由当前 shell（非 root）完成，`sudo` 只作用于 echo——这是文档里常见的一个不严谨写法，真要写 root 文件应 `sudo sh -c 'echo ... > 文件'`，但此处目标文件在用户家目录，无所谓。

#### 5.2.3 枚举设备

以上完成后，执行如下命令检查 ADB 是否能正确识别出移远模块：
```bash
sudo adb kill-server
sudo adb devices
```

#### 5.2.4 文件下载

AG35-CET / AG35-EUT 支持多种方式下载文件。调试中用 ADB 命令下载文件最便捷。执行 `adb push` 将上位机本地文件下载到模块文件系统，命令基本格式：
```
adb push <本地路径> <模块路径>
```

示例：把 `~/sdk/ql-ol-extsdk-ag35cetcar01a01m2g_ocpu/sample/helloworld/` 下的 `helloworld` 上传到模块 `/data/` 目录：
```bash
adb push ~/sdk/ql-ol-extsdk-ag35cetcar01a01m2g_ocpu/sample/helloworld/helloworld /data
```

### 5.3 交叉编译工具安装

交叉编译工具只需安装一次。本节以 `ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain.tar.gz` 版本为例。

#### 5.3.1 创建工作目录

交叉编译工具默认工作目录为 **`/opt/ql_crosstools`**，若不存在则创建：
```bash
sudo mkdir -p /opt/ql_crosstools
```

#### 5.3.2 安装交叉编译工具链

将工具链压缩包复制到 `/opt/ql_crosstools` 下并解压：
```bash
tar xvf ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain.tar.gz -C /opt/ql_crosstools
```

#### 5.3.3 检查是否安装成功

执行如下命令查询工具链版本号：
```bash
/opt/ql_crosstools/ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain/bin/arm-openwrt-linux-gcc --version
```
成功输出示例：
```
arm-openwrt-linux-gcc (OpenWrt GCC 8.4.0 (r9-552b312bd0)) 8.4.0
Copyright (C) 2018 Free Software Foundation, Inc.
```

> **解读（关键）**：交叉编译器三元组是 **`arm-openwrt-linux-gcc`**——底层系统是 **OpenWrt**（从 5.4.3 节目标机提示符 `root@OpenWrt:/tmp#` 也能确认）。GCC 为 OpenWrt 定制的 8.4.0。这就是 AG35 QuecOpen 的真实运行环境：OpenWrt + Linux 5.4.195 内核（见 6.3.2）+ glibc。本仓库交叉编译 AG35 时需用此前缀工具链，路径硬编码在 `/opt/ql_crosstools/...`。

### 5.4 SDK 安装

SDK 压缩包可安装到上位机任意本地目录，但**必须在 Ubuntu 普通用户的工作环境下进行**（非 root）。本节以 `ql-ol-extsdk-ag35cetcar01a01m2g_ocpu.tar.gz` 为例。

#### 5.4.1 SDK 解压

```bash
tar xvf ql-ol-extsdk-ag35cetcar01a01m2g_ocpu.tar.gz
cd ql-ol-extsdk-ag35cetcar01a01m2g_ocpu
```

#### 5.4.2 检查是否安装成功

通过编译示例程序检查 SDK。编译 helloworld 示例，成功生成可执行文件即说明 SDK 安装成功：
```bash
cd ql-ol-extsdk-ag35cetcar01a01m2g_ocpu
cd sample/helloworld/
make
```

#### 5.4.3 烧录验证

将编译生成的 `helloworld` 下载到模块文件系统目录（如 `/tmp`），赋予可执行权限后运行，成功输出 `hello quectel` 即表明编译成功：
```
root@OpenWrt:/tmp# chmod +x helloworld
root@OpenWrt:/tmp# ./helloworld
hello quectel
root@OpenWrt:/tmp#
```

> **解读**：注意 SDK 必须以**普通用户**身份解压/编译（root 编译会引入权限/路径问题，OpenWrt buildroot 的通病）。目标机 shell 提示符 `root@OpenWrt` 确认设备端是 root 运行的 OpenWrt。`/tmp` 是 tmpfs，掉电丢失——仅供调试；正式部署进 `oemapp` 分区（见 6.3.3）。

---

## 第 6 章 SDK 使用方法（上：6.1~6.4 开头）

### 6.1 SDK 文件说明（表 2：SDK 顶层目录）

| 目录 / 文件 | 说明 |
|---|---|
| `ql-sysroots` | 交叉编译使用的**头文件和库文件**（即 sysroot，API 头文件如 `log.h` 在此） |
| `ql-ol-kernel` | **Linux 内核源码** |
| `ql-ol-rootfs.tar.gz` | **Linux 根文件系统压缩包**（编译 rootfs 时解压它） |
| `tools` | 编译使用的**二进制文件、脚本和配置文件** |
| `sample` | **示例代码**文件夹 |
| `sdk.mk` | **公用的 Makefile 头文件**（定义编译用环境变量，被各目录 Makefile include） |
| `ql-ol-rootfs` | 编译 rootfs 时解压 `ql-ol-rootfs.tar.gz` 生成的目录 |
| `oemapp` | **客户文件系统源文件夹**（客户应用源码放这里，对应 oemapp 分区） |
| `target` | 目标文件夹，编译产出的**镜像文件和临时文件**存放于此 |
| `Makefile` | 根目录 Makefile，用于编译内核和制作文件系统镜像 |

> **解读**：与本仓库环境变量呼应——CLAUDE.md 提到 EC200A 用 `QL_SYSROOT_DIR` 指向 sysroot，此处 `ql-sysroots` 正是被指向的目录。`oemapp/`（源）→ `make oemapp` → `oemapp.squashfs`（镜像）→ 替换固件包 → 刷机，是客户应用的完整路径。

### 6.2 编译方式

- SDK 编译由 **Makefile 规则**组织：
  - **根目录 Makefile** → 编译内核和文件系统镜像；
  - **`sample/` 各子目录 Makefile** → 编译各单独示例程序。
- 用户可直接进入 `sample/` 各子目录，运行 `make` 即开始编译。
- 各目录 Makefile 都 **引用 `sdk.mk`**，`sdk.mk` 中定义了编译过程需要的环境变量。

**表 3：SDK 根目录下常用编译命令**

| 命令 | 说明 |
|---|---|
| `make rootfs` | 编译 rootfs 镜像，产物位于 `target/` 下（`root.squashfs`） |
| `make kernel` | 编译内核镜像，产物位于 `target/` 下（`zImage`） |
| `make kernel_module` | 编译内核模块，产物位于 `ql-ol-rootfs/lib/` 下 |
| `make sample` | 编译所有示例程序代码，产物位于 `sample/` 各子目录中 |

**表 4：示例程序（`sample/` 子目录）**

| 目录 | 说明 |
|---|---|
| `sample/kmodule` | 自定义**内核模块**示例程序 |
| `sample/helloworld` | **用户程序**示例 |
| `sample/test_sdk_api` | **SDK 的 API 接口测试程序**（学习 API 用法的入口） |

> **解读**：`sample/test_sdk_api` 是学习 QuecOpen API 的最重要参考——它演示了第 6.5 节 API 接口的实际调用方式。`make kernel_module` 用于编译 ko 动态驱动（如自定义内核模块），改动后还需重制 rootfs 镜像（见 6.3.2）。

### 6.3 文件系统镜像制作

#### 6.3.1 制作根文件系统镜像

- 解压后 SDK 中根文件系统压缩包名为 `ql-ol-rootfs.tar.gz`。在 SDK 根目录运行 `make rootfs`，产物位于 `target/` 下，名为 **`root.squashfs`**。
- `make rootfs` 分两阶段：
  - **第一阶段**：解压 `ql-ol-rootfs.tar.gz`，解压出目录 `ql-ol-rootfs/`。
  - **第二阶段**：调用制作文件系统的相关工具。

Makefile 中解压 `ql-ol-rootfs.tar.gz` 的代码片段（原文截图）：
```makefile
if [ ! -d ${QL_ROOTFS_DIR} ]; then \
    printf "Extracting the rootfs..."; \
    mkdir -p ${QL_ROOTFS_DIR}; \
    tar -xvf ${QL_SDK_DIR}/ql-ol-rootfs.tar.gz -C ${QL_ROOTFS_DIR}; \
    echo "Done."; \
fi
```
> 仅当 `QL_ROOTFS_DIR` 目录不存在时才解压——即首次 `make rootfs` 解压，之后复用。若想重新基于干净 rootfs 构建，需手动删除该目录。

制作根文件系统镜像的代码片段（原文截图，命令行较长，核心是调用 `mksquashfs` 工具，将 `ql-ol-rootfs` 目录打成 `root.squashfs`，参数含 `-noappend -all-root -comp xz` 等，块大小与压缩算法在此设定）。

- 生成的镜像可替换固件包 `update/` 目录下的同名文件，在上位机用移远升级工具升级（步骤见第 4 章）。

#### 6.3.2 制作内核镜像文件

- 在 SDK 根目录运行 `make kernel`，编译内核并制作内核镜像，产物位于 `target/` 下，名为 **`zImage`**。可替换固件包 `update/` 下同名文件后升级。
- 若**修改了内核 ko 动态加载驱动代码**，重新编译内核后，还需用 `make kernel_module` 重新编译内核驱动模块，产物存放在根文件系统的 **`ql-ol-rootfs/lib/modules/5.4.195/`** 目录（该目录对应 **Linux 内核版本 5.4.195**，随内核版本升级而改变），且需**重新制作根文件系统镜像**用于模块升级。

> **解读（关键事实）**：AG35 的 **Linux 内核版本是 5.4.195**。改 ko 后的完整链条是：`make kernel` → `make kernel_module`（产物进 `lib/modules/5.4.195/`）→ 重新 `make rootfs`（把新 ko 打进 rootfs）→ 刷 `zImage` + `root.squashfs`。单独刷 zImage 不会带上新 ko。

#### 6.3.3 制作用户分区镜像文件

- 模块的 **`oemapp` 分区**用于存放用户的应用程序。
- 在 SDK 根目录运行 `make oemapp`，编译用户应用镜像，产物位于 `target/` 下，名为 **`oemapp.squashfs`**。可替换固件包 `update/` 下同名文件后升级。

> **解读**：本仓库的 modem_mng 守护进程，最终就是进 `oemapp` 分区（通过 `make oemapp` 打包后刷机），与 `/data`（adb push 临时调试）区分。三类自制镜像：`root.squashfs`（系统根）、`zImage`（内核）、`oemapp.squashfs`（客户应用）。

### 6.4 日志系统（开头，详见下一段分析）

- 系统支持 **Android 的 Logcat 日志系统**。头文件路径：`ql-sysroots/usr/include/log.h`，示例程序路径：`sample/log_logcat`。
- 应用程序调用 `ql-sysroots/usr/include/log.h` 中定义的宏或日志接口，将日志写入 **Logcat 日志缓冲区**。可用 `logcat` 查看日志，或将日志保存至存储空间。Logcat 官方指导：`https://developer.android.google.cn/studio/command-line/logcat`。
- 每个日志条目包含：一个**优先级（Priority）**、一个**标识来源的标签（Tag）**、实际日志消息。Logcat 定义 **6 个日志等级**，优先级从低到高依次为 **VERBOSE、DEBUG、INFO、WARN、ERROR** 和（见下一段第 6 个等级）……

### 6.4 日志系统（续）

承上：Logcat 6 个日志等级，优先级从低到高完整为 **VERBOSE < DEBUG < INFO < WARN < ERROR < FATAL**。两条特殊规则：
- **VERBOSE** 等级仅在开启 `LOG_NDEBUG` 宏后才会输出日志（默认不输出 VERBOSE，节省开销）。
- **FATAL** 等级标识严重错误，**不提供日志接口**（即没有对应的 ALOGF 宏；FATAL 是系统内部用，应用层不可主动打 FATAL）。

如需重新封装日志接口，可参考头文件 **`ql-sysroots/usr/include/android/log.h`** 中的定义。

**表 5：日志等级对应日志接口（宏）**

| 日志等级 | 日志接口（宏） |
|---|---|
| VERBOSE | `ALOGV` |
| DEBUG | `ALOGD` |
| INFO | `ALOGI` |
| WARN | `ALOGW` |
| ERROR | `ALOGE` |
| FATAL | （无接口） |

> **解读**：这是标准 Android liblog 的 `ALOGx` 系列宏（A=Android）。每条日志带 Priority + Tag + Message。VERBOSE 受 `LOG_NDEBUG` 门控、FATAL 无应用接口，是 Android 日志的惯例。

#### 6.4.1 在应用程序中使用日志接口

**第一步：Makefile 链接 Logcat 库**——在链接选项里加 `-llog`。原文 Makefile 截图：
```makefile
CURR_DIR := $(shell pwd)

-include ../../sdk.mk

QL_TARGET_EXE = sample_log
QL_TARGET_OBJS = main.o

CFLAGS  := $(QL_SDK_CFLAGS) $(QL_SDK_HARD_CFLAGS)
LDFLAGS := $(QL_SDK_LDFLAGS) $(QL_SDK_LIBS) -llog

all: $(QL_TARGET_EXE)

%.o:%.c
	$(CC) -o $@ -c $^ $(CFLAGS)

$(QL_TARGET_EXE): $(QL_TARGET_OBJS)
	$(CC) -o $@ $(QL_TARGET_OBJS) $(LDFLAGS)

clean:
	rm -rf *.o
	rm -rf $(QL_TARGET_EXE)
```
> 关键点：`-include ../../sdk.mk` 引入公共变量（`QL_SDK_CFLAGS`/`QL_SDK_HARD_CFLAGS`/`QL_SDK_LDFLAGS`/`QL_SDK_LIBS`/`CC` 等都来自 sdk.mk）；`-llog` 链接 liblog（红框强调，必加，否则 ALOGx 未定义引用）。`QL_SDK_HARD_CFLAGS` 暗示硬浮点 ABI。

**第二步：源码中，引用 Logcat 头文件之前，定义日志 Tag 及是否开启 VERBOSE 输出**。完整示例代码：
```c
#include <stdio.h>

/** 定义日志 Tag。 */
#define LOG_TAG "ql_log_test"

/** 打开调试开关，ALOGV 会输出日志。 */
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
> **关键顺序（易错点）**：`#define LOG_TAG` 和 `#define LOG_NDEBUG` 必须写在 `#include "log/log.h"` **之前**——因为 log.h 内部用这两个宏决定 Tag 和是否编出 ALOGV。顺序颠倒则 Tag 失效、VERBOSE 被裁掉。`#include "log/log.h"`（而非 `<log/log.h>`），头文件在 sysroot 的 `usr/include/log/log.h`。

#### 6.4.2 查看应用程序日志

用 `logcat` 按 Tag 和等级显示日志，命令格式（方括号内为可选字段，不填默认查询所有等级）：
```
logcat -s <日志的Tag[:显示日志的等级]>
```
以查询 Tag 为 `ql_log_test` 为例，输出：
```
root@OpenWrt:/oemdata# logcat -s ql_log_test
--------- beginning of /dev/log_main
--------- beginning of /dev/log_system
01-01 12:13:11.105 D/ql_log_test( 4597): I am LOG_DEBUG
01-01 12:13:11.105 I/ql_log_test( 4597): I am LOG_INFO
01-01 12:13:11.105 W/ql_log_test( 4597): I am LOG_WARN
01-01 12:13:11.105 E/ql_log_test( 4597): I am LOG_ERROR
```
> **解读**：注意输出里**没有 VERBOSE 那条**——尽管源码 `#define LOG_NDEBUG 1` 并调用了 `ALOGV`。这印证了 VERBOSE 的门控行为（且本例 logcat 默认显示所有等级仍未见 V，说明 V 在编译期/运行期被进一步过滤）。日志格式：`月-日 时:分:秒.毫秒 等级首字母/Tag( PID): 消息`。时间显示 `01-01 12:13`（未同步 NTP 时的默认时间，与 modem_mng logger 把未同步日志归入 `unsynced/` 的设计动机相同）。日志来自两个设备节点 `/dev/log_main`、`/dev/log_system`。

#### 6.4.3 日志保存

Logcat 支持日志分段，采用**循环覆盖**方式保存到日志文件，命令格式（方括号内可选）：
```
logcat [-s <Tag[:等级]>] -f <日志文件名> -r <单个日志文件大小,单位KiB> -n <日志文件个数> &
```
以将系统所有日志保存至 `/oemdata/log.txt`、单个文件 100 KiB、文件个数 5 为例：
```
root@OpenWrt:/oemdata# logcat -f /oemdata/log.txt -r 100 -n 5 &
```
查看日志文件，生成滚动文件 `log.txt`、`log.txt.1`、`log.txt.2`、`log.txt.3`、`log.txt.4`、`log.txt.5`：
```
root@OpenWrt:/oemdata# ls -l
total 108
-rw------- 1 root root  3904 Jan 1 12:15 log.txt
-rw------- 1 root root 16482 Jan 1 12:15 log.txt.1
-rw------- 1 root root 16456 Jan 1 12:15 log.txt.2
-rw------- 1 root root 16470 Jan 1 12:15 log.txt.3
-rw------- 1 root root 16403 Jan 1 12:15 log.txt.4
-rw------- 1 root root 16411 Jan 1 12:15 log.txt.5
```
> **参数详解**：`-f` 指定输出文件；`-r N` 每个文件 N KiB 后滚动；`-n M` 保留 M 个轮转文件（`log.txt` 为当前，`.1`~`.M` 为历史）；末尾 `&` 后台运行。这是设备端持久化日志的标准做法。`/oemdata` 是可读写持久分区（区别于 tmpfs 的 `/tmp`）。
>
> **与 modem_mng 对照**：本仓库 `logger_sd.c` 自己实现了一套日志落 SD 卡 + 按天分目录 + 40 天保留 + 空间下限的方案，而非用 Logcat 的 `-r/-n` 轮转。两者目标一致（设备端持久日志 + 滚动），但 modem_mng 的方案更贴合 SD 卡场景（500MB 阈值、unsynced 目录、跨午夜切换）。AG35 平台理论上也可直接用 Logcat `-f -r -n` 简化，但 modem_mng 选择跨平台统一用自有 logger。

### 6.5 API 接口

SDK 提供一组应用程序接口（API），头文件位于 **`ql-sysroots/usr/include/ql-sdk/`**，部分 API 有相应测试程序，存放路径 **`sample/test_sdk_api/`**。各功能 API 使用方法的参考文档（表 6 + 续表）：

**表 6：API 接口使用方法相关文档**

| 功能 | 参考文档 |
|---|---|
| 网络注册、获取注网相关信息 | `Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_蜂窝网络信息_开发指导` |
| (U)SIM 卡相关功能 | `..._(U)SIM_开发指导` |
| 模块管理，获取模块的状态和信息 | `..._设备管理指导` |
| 短信 | `..._SMS_开发指导` |
| 语音服务 | `..._语音服务_开发指导` |
| 网络管理，防火墙 | `..._网络管理指导` |
| 数据拨号 | `..._数据拨号_开发指导` |
| GNSS | `..._GNSS_开发指导` |
| Wi-Fi | `..._Wi-Fi_开发指导` |
| 音频服务 | `..._音频服务_开发指导` |

> **解读（对本仓库的意义）**：这张表是 AG35 全部上层 API 的文档地图。本仓库 md/pdf/ag35/ 已收录其中若干（USIM、SMS、SPI、WiFi、网络管理、数据拨号、蜂窝网络信息 API、GPIO 等），正是本批 PDF 分析任务的对象。**数据拨号开发指导** 与 modem_mng 的 AG35 拨号逻辑直接相关；**蜂窝网络信息**、**设备管理** 对应 nanomsg 暴露的 IMEI/IMSI/ICCID/CSQ 等状态查询。API 头文件统一在 `ql-sysroots/usr/include/ql-sdk/`，`sample/test_sdk_api/` 是调用范例（也是 6.8 静态扫描的扫描对象，见下）。

### 6.6 外设接口

AG35-CET / AG35-EUT 提供多种外设接口：**GPIO、I2C、SPI、USB、UART** 等。各外设使用方法参考文档（表 7）：

**表 7：外设接口相关文档**

| 功能 | 参考文档 |
|---|---|
| GPIO | `..._GPIO_开发指导` |
| I2C | `..._I2C_开发指导` |
| SPI | `..._SPI_开发指导` |
| USB | `..._USB_开发指导` |
| UART | `..._UART_开发指导` |

> **解读**：外设接口（GPIO/I2C/SPI/USB/UART）与上层 API（6.5 的网络/SIM/短信等）分属两类文档。本仓库与 GPIO 强相关——记忆 [[ag35-ledcontrol-sigill-workaround]] 中 AG35 控灯走 libledcontrol（底层是 GPIO）触发 SIGILL，对应的 GPIO 开发指导是排查方向之一。

### 6.7 应用程序启动

- AG35-CET / AG35-EUT 的启动管理使用 **SystemV 方式**（非 systemd）。
- 启动顺序：先启动 **`/etc/rc.d/`** 目录下的启动脚本，启动脚本按照**优先级数字由小到大**顺序启动。
- **推荐**：客户将启动脚本存放在 `/etc/rc.d/` 下，脚本优先级数字设为 **99**（最低优先级），表示系统服务启动完毕之后再启动用户服务，可避免服务的依赖问题。

> **解读（对本仓库重要）**：modem_mng 作为开机自启守护进程，应放 `/etc/rc.d/` 并用优先级 99（如 `S99modem_mng`），确保 4G/网络/SD 等系统服务先起来再启动它，避免依赖未就绪。这与 CLAUDE.md 中"watchdog/init relaunches"的恢复模型一致——L3 `exit(1)` 后由 init（这里是 SystemV `/etc/rc.d/` 脚本或 watchdog）重新拉起。SystemV `rc.d` 脚本优先级即 `SNN` 前缀的 NN 数字。

### 6.8 静态代码扫描

静态代码扫描：用户写好源代码后，**无需经过编译器编译**，直接用扫描工具对源码扫描，找出语义缺陷或安全漏洞。本节介绍免费的 C/C++ 静态扫描工具 **CPPCHECK**（官网 `http://cppcheck.net`）。

#### 6.8.1 安装 CPPCHECK

Ubuntu 下：
```bash
sudo apt-get install cppcheck
```

#### 6.8.2 使用 CPPCHECK 扫描代码

进入源码目录，命令格式：
```
cppcheck --enable=all <源码目录>
```
扫描结果示例（针对 `test_sdk_api` 的真实输出节选）：
```
[test_sdk_api/m_data_call.c:423] (style) The scope of the variable 'ret' can be reduced.
[test_sdk_api/m_data_call.c:539] (style) The scope of the variable 'i' can be reduced.
[test_sdk_api/m_dm.c:239]  (style) The scope of the variable 'ret' can be reduced.
[test_sdk_api/m_dm.c:75]   (portability) scanf without field width limits can crash with huge input data ...
[test_sdk_api/m_dm.c:173]  (portability) scanf without field width limits can crash with huge input data ...
[test_sdk_api/m_dm.c:209]  (error) Memory leak: buf
[test_sdk_api/m_nw.c:373]  (warning) %d in format string (no. 1) requires a signed integer given in the argument list ...
[test_sdk_api/m_nw.c:393]  (warning) ...
[test_sdk_api/m_nw.c:412]  (warning) ...
[test_sdk_api/m_nw.c:295]  (portability) scanf without field width limits ...
[test_sdk_api/m_nw.c:308]  (portability) ...
[test_sdk_api/m_nw.c:312]  (portability) ...
[test_sdk_api/m_nw.c:630]  (portability) ...
[test_sdk_api/m_nw.c:648]  (portability) ...
```
扫描问题分级：`(style)` 风格（变量作用域可缩小）、`(portability)` 可移植性（`scanf` 无字段宽度限制，超长输入可能崩溃）、`(warning)` 警告（`%d` 格式串要求有符号整型实参）、`(error)` 错误（**`m_dm.c:209` Memory leak: buf** 内存泄漏，原文红框标注，最严重）。

> **解读**：`--enable=all` 开启全部检查类别。优先级排序应为 **error > warning > portability > style**——`Memory leak` 必须修，`scanf` 无宽限和格式串不匹配是潜在崩溃/类型错误。值得注意的是：这些缺陷出现在 **移远自己的 `test_sdk_api` 示例代码**里——说明示例代码并非生产级，开发者参考时需自行加固。这与本仓库一贯的审计态度（记忆 [[eg25-qlsendat-rsp-buffer-overflow]]、[[ec200a-nanomsg-exception-crash]] 等）一脉相承：厂商示例/SDK 的缓冲与异常处理常有坑，移植时要逐条复核。`scanf without field width limits` 正是缓冲溢出类隐患，与 EG25 `Ql_SendAT` 缓冲砸栈是同类问题。

---

## 第 7 章 模块开机检查

模块启动后，可通过一系列 AT 命令检查模块是否处于正常工作状态。步骤：

- **步骤 1**：通过 USB 串口线将上位机连接至模块开发板（**LTE OPEN EVB**）的 **USB AT 口**。
- **步骤 2**：插入 (U)SIM 卡、连接天线后，将模块上电。
- **步骤 3**：使用 **QCOM 工具**（移远串口调试工具，用法见 **文档[6]**）分别执行如下命令：

| 检查项 | AT 命令 | 说明 |
|---|---|---|
| 检测串口 | `AT` | 串口通信是否正常，返回 `OK` |
| 检测 (U)SIM 卡 | `AT+CPIN?` | 返回 `+CPIN: READY` 表示卡就绪 |
| 检测信号强度 | `AT+CSQ` | 返回 `+CSQ: <rssi>,<ber>` |
| 检测模块注网状态 | `AT+CGREG?` | 返回 `+CGREG: <n>,<stat>` |
| 查询运营商 | `AT+COPS?` | 返回当前注册运营商 |
| 查询网络制式 | `AT+QCELLINFO?` | 移远私有命令，返回小区/制式信息 |
| 语音测试 | `ATDxxx;` | xxx 为手机号码，拨打电话（注意结尾分号表示语音呼叫） |

**图 2：插入一张中国联通 (U)SIM 卡的开机检查流程**（真实应答）：
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
+QCELLINFO: LTE,B3,19650,46001,56848,365,100126239,-62

OK
ATD1xxxxxxxxxx;

OK
```

**逐条应答解读**：

- `+CPIN: READY` — SIM 卡 PIN 就绪，无需解锁，卡正常。
- `+CSQ: 24,99` — RSSI 等级 **24**（范围 0~31，越大越好；24 ≈ -67 dBm，信号良好）；误码率 **99**（表示未知/不可测，正常）。
  - 换算：RSSI=24 → -113 + 2×24 = **-65 dBm**，强信号。modem_mng 的 ST_SIGNAL 阶段正是用 `AT+CSQ` 取 RSSI。
- `+CGREG: 3,0` — `<n>=3`（启用网络注册 URC 并带位置信息），`<stat>=0`……注意此处 `<stat>=0` 通常表示"未注册且未搜索"，但与 `+CPIN:READY`、`+COPS` 已注册矛盾——可能是采集瞬间的快照，或 `CGREG` 的 GPRS/PS 域注册状态与 `COPS` 的 CS 域不同步（开机瞬间 PS 域尚未附着）。**实际应以 `+CGREG: x,1`（已注册本网）或 `x,5`（漫游注册）为最终正常态**。
  - **对照 modem_mng**：CLAUDE.md 中 ST_SIM 阶段检查"SIM 卡和网络注册"，恢复策略以 `REG≠0` / `REG=0` 区分。本图 `CGREG:3,0` 的 stat=0 正是 modem_mng 视为"未注册"的状态，对应 L2 路径（REG=0 且 >5min → RF reset）。
- `+COPS: 0,2,"46001",7` — `<mode>=0`（自动选网）、`<format>=2`（数字格式 PLMN）、`<oper>="46001"`（**中国联通** PLMN）、`<AcT>=7`（接入技术 7 = **E-UTRAN/LTE**）。
  - **对照本仓库海外选网功能**（记忆 [[ec200a-ag35-oper-select-pending]]）：`AT+COPS` 的 `<mode>` 字段正是海外运营商切换的核心——`COPS=0` 是自动选网（恢复路径），`COPS=1` 是手动选网（切到指定海外运营商）。本文档确认 AG35 用 `AT+COPS?` 查询当前选网，与 modem_mng 的 `[OPER]` 选网逻辑一致。`<format>=2` 数字 PLMN 也印证记忆中"勿用 COPS 解析运营商名、会与 getPlmn 字段冲突"的判断（COPS format=2 返回的是 46001 数字而非名称）。
- `+QCELLINFO: LTE,B3,19650,46001,56848,365,100126239,-62` — 移远私有小区信息：制式 **LTE**、频段 **B3**（1800MHz）、EARFCN **19650**、PLMN **46001**、TAC/小区相关 `56848`、PCI **365**、Cell ID `100126239`、RSRP **-62 dBm**（强）。
- `ATD1xxxxxxxxxx;` → `OK` — 拨打电话（号码打码），结尾分号 `;` 表示**语音呼叫**（无分号则为数据呼叫），返回 OK 表示拨号发起成功，验证语音功能。

> **解读**：这套 7 条 AT 命令是模块产线/开机自检的标准清单，覆盖：串口→SIM→信号→注网→运营商→制式→语音的完整链路。modem_mng 的 dial_loop 五阶段状态机（ST_STATUS/ST_SIM/ST_SIGNAL/ST_PING/ST_RECOVERY）本质上把其中 SIM/信号/注网检查做成了运行期持续轮询。开发板型号 **LTE OPEN EVB**，调试用 **QCOM 工具**（移远 Windows 串口工具）。

---

## 第 8 章 附录：参考文档及术语缩写

### 表 8：参考文档

| 编号 | 文档名称 | 在本文中的引用位置 |
|---|---|---|
| [1] | `Quectel_Customer_FW_Download_Tool_用户指导` | 4.1 Windows 升级工具 |
| [2] | `Quectel_LTE&LTE-A_QFlash_Linux&Android_User_Guide` | 4.1 Linux/Android 升级工具 |
| [3] | `Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_AB系统升级指导` | 4.2 A/B 系统升级 |
| [4] | `Quectel_AG35-CET&AG35-EUT_QuecOpen_硬件设计手册` | 4.3 USB_BOOT 引脚上拉方法 |
| [5] | `Quectel_UMTS_LTE_5G_Linux_USB_Driver_用户指导` | 5.1 Linux/Android USB 驱动 |
| [6] | `Quectel_QCOM_用户指导` | 第 7 章 QCOM 工具用法 |

### 表 9：术语缩写

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| ADB | Android Debug Bridge | 安卓调试桥 |
| API | Application Programming Interface | 应用程序编程接口 |
| GCC | GNU Compiler Collection | GNU 编译器套件 |
| GNSS | Global Navigation Satellite System | 全球导航卫星系统 |
| GPIO | General-purpose Input/Output | 通用型输入/输出 |
| I2C | Inter-Integrated Circuit | 集成电路总线 |
| ID | Identifier | 标识符 |
| IoV | Internet of Vehicles | 车联网 |
| LTE | Long Term Evolution | 3GPP 长期演进 |
| OC | Ordering Code | 采购编码 |
| OS | Operating System | 操作系统 |
| SDK | Software Development Kit | 软件开发工具包 |
| SMS | Short Message Service | 短消息业务 |
| SPI | Serial Peripheral Interface | 串行外设接口 |
| (U)SIM | (Universal) Subscriber Identity Module | （通用）用户识别模块 |
| UART | Universal Asynchronous Receiver/Transmitter | 通用异步收发传输器 |
| USB | Universal Serial Bus | 通用串行总线 |
| VID | Vendor ID | 运营商代码（应为"厂商代码"，原文译为"运营商代码"系笔误） |

> **勘误注**：表 9 中 `VID` 译为"运营商代码"是原文翻译错误——VID = Vendor ID 应为"**厂商代码/供应商 ID**"（5.2.1 节里的 `0x2c7c` 就是 Quectel 的厂商 ID）。运营商代码应是 PLMN（如 46001）。读者引用时需注意。

---

## 全文要点速查（面向 modem_mng 移植/维护）

| 主题 | 关键事实 | 出处 |
|---|---|---|
| 开发主机 | Ubuntu 18.04 x64，≥4GiB 内存、≥4 核 | §2.2 |
| 工具链 | `arm-openwrt-linux-gcc` 8.4.0 + **glibc**（非 musl），装 `/opt/ql_crosstools` | §3.3 / §5.3 |
| 设备 OS | **OpenWrt**，root 运行 | §5.4.3 |
| 内核版本 | **Linux 5.4.195** | §6.3.2 |
| USB VID | Quectel = **0x2c7c** | §5.2.2 |
| 三类自制镜像 | `root.squashfs`(make rootfs) / `zImage`(make kernel) / `oemapp.squashfs`(make oemapp) | §6.3 |
| 客户应用分区 | **oemapp**（源在 SDK `oemapp/`，部署进 oemapp 分区） | §3.1 / §6.3.3 |
| 日志方案 | Android Logcat（`-llog`、`ALOGx`、`LOG_TAG`/`LOG_NDEBUG` 须在 include 前）；可 `logcat -f -r -n` 轮转落盘 | §6.4 |
| 开机自启 | **SystemV** `/etc/rc.d/`，优先级 99（系统服务后启动用户服务） | §6.7 |
| 救砖 | USB_BOOT 上拉 VDD_EXT → 强制下载模式 | §4.3 |
| 静态扫描 | `cppcheck --enable=all`；示例代码本身有 memory leak / scanf 隐患 | §6.8 |
| 开机自检 AT | AT / CPIN? / CSQ / CGREG? / COPS? / QCELLINFO? / ATDxxx; | 第 7 章 |
| 选网 | `AT+COPS?` 查选网（mode=0 自动 / 数字 PLMN 46001 联通），关联海外选网功能 | 第 7 章 |

---

<!-- GENERATION_COMPLETE -->

