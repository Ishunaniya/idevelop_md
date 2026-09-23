# EC2x&EG2x-G&EG9x 系列 QuecOpen 快速开发指导 V1.0

> **文档信息**
> - 适用平台：EC2x 系列、EG2x-G 系列、EG9x 系列
> - 发布方：上海移远通信技术股份有限公司
> - 总页数：30 页

---

## 目录

1. [引言](#1-引言)
2. [QuecOpen® 软件开发概述](#2-quecopen-软件开发概述)
3. [软件包介绍](#3-软件包介绍)
4. [固件升级](#4-固件升级)
5. [软件开发环境准备](#5-软件开发环境准备)
6. [SDK 使用](#6-sdk-使用)
7. [文件下载](#7-文件下载)
8. [模块开机检查](#8-模块开机检查)
9. [附录：参考文档及术语缩写](#9-附录参考文档及术语缩写)

---

## 表格索引

| 表格 | 页码 |
|---|---|
| 表 1：适用模块 | 7 |
| 表 2：固件包关键文件列表 | 9 |
| 表 3：SDK 文件说明 | 18 |
| 表 4：SDK 根目录下常用的编译命令 | 19 |
| 表 5：API 接口相关文档列表 | 21 |
| 表 6：外设接口相关文档列表 | 22 |
| 表 7：参考文档 | 29 |
| 表 8：术语缩写 | 29 |

---

## 1 引言

移远通信 EC2x、EG2x-G 和 EG9x 系列模块支持 QuecOpen® 方案。QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。

本文档主要介绍移远通信 EC2x、EG2x-G 和 EG9x 系列模块 QuecOpen® 方案的快速开发流程，包括固件升级、软件开发环境准备、SDK 的使用、文件下载以及模块开机检查等操作流程。

### 1.1 适用模块

**表 1：适用模块**

| 模块系列 | 模块 |
|---|---|
| EC2x | EC20-CE |
| EC2x | EC20-CN |
| EC2x | EC21 系列 |
| EC2x | EC25 系列 |
| EG2x-G | EG21-G |
| EG2x-G | EG25-G |
| EG9x | EG91 系列 |
| EG9x | EG95 系列 |

---

## 2 QuecOpen® 软件开发概述

### 2.1 对开发者的要求

1. 熟悉标准 GNU/Linux 应用开发，以及常见 Linux 系统命令。
2. 掌握一些驱动、网络协议基本知识。
3. 了解移远通信 EC2x、EG2x-G 和 EG9x 系列模块的 AT 命令（参考文档 [1]）。

### 2.2 开发流程

1. **准备硬件开发环境**：系统需为 Ubuntu 1404 或者 1604 的 64 位系统，内存 4 GB 以上，CPU 4 核以上；若使用虚拟机，则分配给虚拟机的内存应不低于 4 GB。
2. **确认固件版本和 SDK 版本匹配**：若不匹配，升级固件至指定版本。有关详细信息，参考**第 4 章**。
3. **准备软件开发环境**：安装 USB 驱动、ADB 工具及 SDK。有关详细信息，参考**第 5 章**。
4. **使用 SDK 进行开发**：有关详细信息，参考**第 6 章**。

---

## 3 软件包介绍

移远通信发布的版本软件包包含固件包和 SDK 软件开发包。固件包和 SDK 软件开发包版本号相同，使用 SDK 软件开发包开发应用程序时，需使用对应的固件包。

> **示例对应关系：**
> - SDK 软件开发包：`EC20CEFARGR06A01M4G_OCPU_SDK.tar.bz2`
> - 对应固件包：`EC20CEFARGR06A01M4G_OCPU.zip`

### 3.1 固件包

固件包包含分区的镜像文件以及固件升级所需的配置文件。固件包一般以压缩包形式提供，名称为英文大写字母，命名格式为：

```
<模块 OC 字段><基线版本号><软件版本号><Flash 信息>_OCPU
```

以 `EC20CEFARGR06A01M4G_OCPU.zip` 为例，该版本固件包名称中包含的版本信息有：

- **模块 OC 字段**：EC20CEFARG（属于 EC2x 系列模块）
- **基线版本号**：R06
- **软件版本号**：A01
- **Flash 信息**：M4G（Flash 512 M；RAM 256 M）
- **OCPU**：QuecOpen 版本

**表 2：固件包关键文件列表**

| 文件 | 文件类型 | 描述 |
|---|---|---|
| `contents.xml` | 配置文件 | 移远通信升级工具使用该文件对模块进行升级，请勿改动。 |
| `update/firehose` | 配置文件 | 移远通信升级工具使用该文件夹下的配置文件对模块进行升级，请勿改动。 |
| `update/NON-HLOS.ubi` | 镜像文件 | Modem 镜像文件。 |
| `update/partition.mbn` | 镜像文件 | 分区信息镜像文件。 |
| `update/partition_nand.xml` | 配置文件 | 分区信息配置文件；移远通信升级工具使用，请勿改动。 |
| `update/mdm9607-boot.img` | 镜像文件 | 内核镜像文件，可替换为使用 SDK 制作的内核镜像文件。 |
| `update/mdm9607-sysfs.ubi` | 镜像文件 | 根文件系统镜像文件，可替换为使用 SDK 制作的根文件系统镜像文件。 |

### 3.2 SDK 软件开发包

SDK 软件开发包提供软件开发环境和交叉编译工具链。SDK 软件开发包一般以压缩包形式提供，命名格式为：

```
<模块 OC 字段><基线版本号><软件版本号><Flash 信息>_OCPU_SDK
```

以 `EC20CEFARGR06A01M4G_OCPU_SDK.tar.bz2` 为例，软件开发包名称中包含的信息有：

- **模块 OC 字段**：EC20CEFARG（属于 EC2x 系列模块）
- **基线版本号**：R06
- **软件版本号**：A01
- **Flash 信息**：M4G（Flash 512 M；RAM 256 M）
- **OCPU**：QuecOpen 版本

SDK 软件开发包的具体使用方法请参考**第 6 章**。

---

## 4 固件升级

模块支持多种固件升级方式，本章介绍了四种固件升级方式。

### 4.1 使用移远通信升级工具升级

- Windows 系统的上位机升级工具的详细使用方法，请参考**文档 [2]**。
- Linux 和 Android 系统的上位机升级工具的详细使用方法，请参考**文档 [3]** 和**文档 [4]**。

### 4.2 使用 fastboot 工具升级

fastboot 是 Android 工具包中的一个命令行工具，支持 Windows、Linux 和 Android 系统。

**步骤 1**：通过如下任一方式使模块进入 fastboot 模式：

1. 在上位机执行如下 ADB 命令：
   ```
   adb reboot bootloader
   ```

2. 在模块 shell 命令行中执行如下命令：
   ```
   sys_reboot bootloader
   ```

**步骤 2**：在上位机使用 fastboot 命令升级分区，命令格式为：

```
fastboot flash <模块分区名> <镜像文件在上位机文件系统的路径>
```

常用的 fastboot 升级命令有：

1. 升级内核镜像：
   ```
   fastboot flash boot mdm9607-boot.img
   ```
   示例输出：
   ```
   sending 'boot' (12550 KB)...
   OKAY [  0.399s]
   writing 'boot'...
   OKAY [  3.028s]
   finished. total time: 3.428s
   ```

2. 升级 rootfs 镜像：
   ```
   fastboot flash system mdm9607-sysfs.ubi
   ```
   示例输出：
   ```
   sending 'system' (48896 KB)...
   OKAY [  1.546s]
   writing 'system'...
   OKAY [ 11.654s]
   finished. total time: 13.200s
   ```

3. 升级 oemapp 文件系统镜像：
   ```
   fastboot flash oemapp oemapp.ubi
   ```
   示例输出：
   ```
   sending 'oemapp' (768 KB)...
   OKAY [  0.025s]
   writing 'oemapp'...
   OKAY [  0.466s]
   finished. total time: 0.493s
   ```

**步骤 3**：在上位机中使用如下命令保存配置：
```
fastboot reboot
```

### 4.3 DFOTA 升级

差分包制作以及 DFOTA 升级方法，请参考**文档 [5]**。

### 4.4 紧急升级

在开发调试阶段，若模块无法正常启动或上位机无法识别模块的 USB 端口，可通过如下方式使模块进入紧急升级下载模式，然后使用上位机工具进行升级。

- 若模块无法正常开机，将 `USB_BOOT` 上拉至 `VDD_EXT`，并重新给模块上电，使模块进入紧急下载模式。
- 若上位机无法正常识别模块的 USB 接口，打开模块 shell 命令行执行 `sys_reboot edl`，模块将自动重启并进入紧急下载模式。

> **备注**：有关将 `USB_BOOT` 上拉到 `VDD_EXT` 的方法，请参考相应模块的硬件设计手册或咨询移远通信技术支持。

---

## 5 软件开发环境准备

在应用 QuecOpen 方案进行软件开发前，需做好以下准备工作：

1. 安装 USB 驱动。
2. 安装 ADB 工具。
3. 安装 SDK。

### 5.1 USB 驱动安装

上位机需要安装 USB 驱动程序才能通过 USB 与模块进行通信。

- **Linux 和 Android 系统**：详细步骤请参考**文档 [6]**。
- **Windows 系统**：可以通过安装包 `Quectel_LTE_Windows_USB_Driver_For_ECM` 直接安装 USB 驱动程序。如需此安装包，请联系移远通信技术支持获取。

### 5.2 ADB 工具安装

Android 调试桥（ADB）是一种功能多样的命令行工具，用于实现上位机与设备之间的通信。ADB 命令便于执行各种设备操作（例如安装和调试应用），并提供对 Linux shell（用于在设备上运行各种命令）的访问权限。

> **备注**：ADB 用户指南官方网址：https://developer.android.google.cn/studio/command-line/adb

#### 5.2.1 安装 ADB 驱动

1. 在上位机 shell 命令行中执行如下命令安装 ADB 驱动：
   ```bash
   sudo apt-get update
   sudo apt-get install android-tools-adb
   ```

2. 若上述命令执行失败，请执行如下命令重新安装 ADB 驱动；若上述命令执行成功，则跳过此步骤：
   ```bash
   sudo add-apt-repository ppa:nilarimogard/webupd8
   sudo apt-get update
   sudo apt-get install android-tools-adb
   ```

3. 执行 `adb` 查看 ADB 驱动是否安装成功。若安装成功，则显示 ADB 的版本号，如下所示：
   ```
   Android Debug Bridge version 1.0.31
   ```

#### 5.2.2 添加模块 USB VID

执行如下命令添加 USB VID，设置 ADB 运行使用的 USB VID 为移远通信模块的 USB VID，即 `0x2c7c`：

```bash
cd ~
sudo echo 0x2c7c > .android/adb_usb.ini
```

#### 5.2.3 枚举设备

以上操作完成后，执行如下命令检查 ADB 是否可以准确识别出模块：

```bash
sudo adb kill-server
sudo adb devices
```

### 5.3 SDK 安装

需在 Ubuntu 普通用户的工作环境下将 SDK 软件开发包安装至上位机的任意本地目录。以 `EC20CEFARGR06A01M4G_OCPU_SDK.tar.bz2` 为例演示如何在 Ubuntu 普通用户的工作环境下安装 SDK 软件开发包。

#### 5.3.1 解压 SDK 软件开发包

将 SDK 压缩包复制到上位机的本地目录下并解压，解压后的目录为 `ql-ol-sdk`：

```bash
tar xvf EC20CEFARGR06A01M4G_OCPU_SDK.tar.bz2
cd ql-ol-sdk
```

#### 5.3.2 检查 SDK 安装结果

通过编译示例程序检查 SDK 软件包是否可以正常工作。执行如下命令编译 `helloworld` 示例程序，编译成功后生成 `helloworld` 可执行文件：

```bash
cd ql-ol-sdk
source ql-ol-crosstool/ql-ol-crosstool-env-init
cd ql-ol-extsdk/example/hello_world
make
```

编译成功即说明 SDK 软件开发包安装成功。编译完成后终端输出示例：

```
QUECTEL_PROJECT_NAME    =EC21E
QUECTEL_PROJECT_REV     =EC21EFAR06A05M4G_OCPU_20.001
arm-oe-linux-gnueabi-gcc -march=armv7-a -mfloat-abi=softfp -mfpu=neon ...
```

#### 5.3.3 检查交叉编译工具链

执行如下命令，检查交叉编译工具是否可以正常工作：

```bash
arm-oe-linux-gnueabi-gcc -v
```

如果安装成功，可以查询到交叉编译工具的版本号，示例如下：

```
Thread model: posix
gcc version 4.9.2 (GCC)
```

完整输出：
```
arm-oe-linux-gnueabi-gcc (GCC) 4.9.2
Copyright (C) 2014 Free Software Foundation, Inc.
This is free software; see the source for copying conditions. There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

#### 5.3.4 烧录验证

将编译生成的可执行文件 `helloworld` 下载至模块的文件系统目录，例如下载到 `/data` 目录，赋予可执行权限，然后运行。运行成功即表明编译成功。

```bash
adb push helloworld /data
adb shell
# cd /data
# chmod +x helloworld
# ./helloworld
```

成功输出：
```
<Hello OpenLinux !>
atoi("19.7")=19
```

---

## 6 SDK 使用

### 6.1 SDK 文件说明

**表 3：SDK 文件说明**

| 目录 | 说明 |
|---|---|
| `ql-ol-crosstool` | 交叉编译使用的头文件、库文件和交叉编译工具链 |
| `ql-ol-crosstool/sysroots/x86_64-oesdk-linux` | 交叉编译工具链 |
| `ql-ol-crosstool/sysroots/armv7ahf-neon-oe-linux-gnueabi` | 编译引用的头文件和库，包括 C 库、第三方软件的头文件和库、SDK 的头文件和库 |
| `ql-ol-extsdk` | SDK 相关库、头文件以及一些工具 |
| `ql-ol-extsdk/include` | SDK 的头文件 |
| `ql-ol-extsdk/lib` | SDK 的库文件 |
| `ql-ol-extsdk/example` | 示例代码 |
| `ql-ol-kernel` | Linux 内核源码 |
| `ql-ol-rootfs.tar.gz` | Linux 根文件系统压缩包 |
| `Makefile` | Makefile 文件，用于编译内核和制作文件系统镜像 |
| `ql-ol-rootfs` | 编译 rootfs 时，解压 `ql-ol-rootfs.tar.gz` 生成的目录 |
| `ql-ol-usrdata` | usrdata 目录，用于制作 `usrdata.ubi` 镜像 |
| `target` | 目标文件夹，一般用于存放编译的镜像文件和临时文件 |

### 6.2 编译方式

SDK 的编译由 Makefile 规则组织，根目录下的 `Makefile` 用于编译内核和文件系统镜像，`ql-ol-extsdk/example` 目录下各子目录中的 `Makefile` 则用于编译各单独的示例程序。

编译前需要加载编译的环境变量。在根目录下执行如下命令加载环境变量：

```bash
source ql-ol-crosstool/ql-ol-crosstool-env-init
```

环境变量加载成功后，可以在命令行中进行交叉编译。加载成功后终端示例：

```
QUECTEL_PROJECT_NAME    =EC21E
QUECTEL_PROJECT_REV     =EC21EFAR06A05M4G_OCPU_20.001
```

SDK 根目录下的 `Makefile` 已实现编译内核和制作文件系统镜像的命令，详细命令如下表。

**表 4：SDK 根目录下常用的编译命令**

| 命令名称 | 说明 |
|---|---|
| `make rootfs` | 编译 rootfs 镜像，生成的目标文件存位于 `target` 目录下 |
| `make kernel_menuconfig` | 生成内核配置文件 |
| `make kernel` | 编译内核镜像，生成的目标文件存位于 `target` 目录下 |
| `make kernel_module` | 编译内核模块，生成的目标文件位于 `ql-ol-rootfs/lib` 目录下 |

### 6.3 文件权限

因文件系统镜像在 Ubuntu 非 root 用户环境下制作，故无 root 用户权限；但在模块中运行需要 root 用户权限，所以移远通信 SDK 使用 **Pseudo** 机制以实现在非 root 用户环境下制作具有 root 用户权限的文件系统。

Pseudo 通过 Linux `LD_PRELOAD` 机制覆盖 Linux 上位机的系统调用，赋予在 Pseudo 环境下创建的文件以伪 root 权限，并将文件的伪 root 权限信息存放在数据库中，可持久保存文件的伪 root 权限信息。

SDK 中使用的 Pseudo 命令为 **`ql_fakeroot`**，在加载编译环境变量之后（参考 **6.2 章节**），可在命令行中直接使用该命令。Pseudo 的工作目录为 `ql-ol-crosstool/sysroots/x86_64-oesdk-linux/pseudo`，其中存放文件的伪 root 权限数据库以及一些工作日志。**Pseudo 的数据库使用的是文件的绝对路径，不可随意移动 SDK 目录**。在创建和修改文件系统中的文件时，需要使用命令 `ql_fakeroot`。

示例——在根文件系统目录下创建一个 `test_dir2` 文件夹，并在 `test_dir2` 文件夹下创建一个文件 `test_file2.txt`，写入内容 `"test"`：

```bash
cd ql-ol-rootfs
ql_fakeroot mkdir test_dir2
ql_fakeroot touch test_dir2/test_file2.txt
ql_fakeroot echo test > test_dir2/test_file2.txt
```

查看 `test_file2.txt` 文件的权限是否为 root（若用户组 ID 和用户 ID 均为 0 则表明文件具有 root 权限）：

```
ql_fakeroot ls -al test_dir2/
total 12
drwxrwxr-x  2 0 0 4096  8月 13 15:29 .
drwxr-xr-x 27 0 0 4096  8月 13 15:29 ..
-rw-rw-r--  1 0 0    5  8月 13 15:29 test_file2.txt
```

UID/GID 均为 0，说明权限正确。

### 6.4 文件系统镜像制作

#### 6.4.1 制作根文件系统镜像文件

解压的 SDK 中根文件系统压缩包的名称为 `ql-ol-rootfs.tar.gz`。初次使用 `source` 加载编译环境时，会自动解压此根文件系统压缩包，解压目录为 `ql-ol-rootfs`，并创建 `ql-ol-crosstool/sysroots/x86_64-oesdk-linux/pseudo` 文件夹。根文件系统的所有文件的伪 root 权限信息会被写入数据库 `ql-ol-crosstool/sysroots/x86_64-oesdk-linux/pseudo/files.db`。

随后可使用 `make rootfs` 制作根文件系统镜像文件，目标文件为 `target/mdm9607-sysfs.ubi`。

生成的镜像文件可通过 fastboot 命令行工具升级到模块，也可替换固件包 `update` 目录下的同名文件，在上位机使用移远通信升级工具进行升级。升级步骤参考**第 4 章**。

#### 6.4.2 制作内核镜像文件

在 SDK 根目录执行 `make kernel` 编译内核并制作内核镜像文件，生成的内核镜像文件位于 `target` 目录，名称为 `mdm9607-boot.img`。

生成的镜像文件可通过 fastboot 命令行工具升级到模块，也可替换固件包 `update` 目录下的同名文件，在上位机使用移远通信升级工具进行升级。升级步骤参考**第 4 章**。

移远通信 QuecOpen 软件包中默认包含已编译好的内核模块，若重新编译了内核，需使用 `make kernel_module` 重新编译内核模块（生成的内核模块存放在根文件系统的 `ql-ol-rootfs/usr/lib/modules` 目录），且需要重新制作根文件系统镜像用于模块升级。

> **备注**：首次编译内核时，需先执行 `make kernel_menuconfig` 生成内核配置文件。

### 6.5 API 接口

移远通信 SDK 中提供了一组应用程序接口（API）。头文件位于 `ql-ol-extsdk/include` 和 `ql-ol-crosstool/sysroots/armv7ahf-neon-oe-linux-gnueabi/usr/include/quectel-openlinux-sdk`，库文件位于 `ql-ol-extsdk/lib` 和 `ql-ol-crosstool/sysroots/armv7ahf-neon-oe-linux-gnueabi/usr/lib`。其中部分 API 有相应测试程序，存放路径为 `ql-ol-extsdk/example/test_mcm_api`。

API 接口的使用方法，请参阅相关文档，对应的文档信息如下表所示：

**表 5：API 接口相关文档列表**

| 功能 | 参考文档 |
|---|---|
| 网络注册、获取注网相关信息 | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_注网信息 API_参考手册 |
| SIM 卡相关功能 | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_SIM_API_参考手册 |
| 模块管理，获取模块的状态和信息 | Quectel_EC2x&EG2x-G&EG9x 系列_QuecOpen_设备管理 API_参考手册 |
| 短信 | Quectel_EC2x&EG2x-G&EG9x 系列_QuecOpen_SMS_API_参考手册 |
| 电话 | Quectel_EC2x&EG9x&EG2x-G&EM05 系列_QuecOpen_语音通话_API 参考手册 |
| 数据拨号 | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_数据拨号用户指导 |
| GNSS | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_GNSS_API_参考手册 |
| Wi-Fi | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_Wi-Fi_API_参考手册 |
| 蓝牙 | Quectel_EC2x 系列&EG25-G_QuecOpen_蓝牙 API_参考手册 |
| 音频 | Quectel_EC2x&EG2x-G&EG9x 系列_QuecOpen_音频服务_API_参考手册 |

### 6.6 外设接口

模块提供多种外设接口，包括 GPIO、I2S、I2C、SPI、USB 和 UART 等。

外设接口的使用方法，请参阅相关文档，对应的文档信息如下表所示：

**表 6：外设接口相关文档列表**

| 功能 | 参考文档 |
|---|---|
| GPIO | Quectel_EC2x&EG2x-G&EG9x 系列_QuecOpen_GPIO_API_参考手册 |
| I2S | Quectel_EC2x&EG2x-G&EG9x 系列_QuecOpen_I2S_开发指导 |
| I2C | Quectel_EC2x&EG2x-G&EG9x 系列_QuecOpen_I2C_开发指导 |
| SPI | Quectel_EC2x&EG2x-G&EG9x 系列_QuecOpen_SPI_开发指导 |
| USB | Quectel_EC2x&EG2x-G&EG9x 系列_QuecOpen_USB_使用指导 |
| UART | Quectel_EC2x&EG2x-G&EG9x 系列_QuecOpen_UART_开发指导 |

### 6.7 Linux C 库接口

模块使用的 C 库是 **Glibc**，版本为 **2.26**。可查看官方文档或执行 `man` 获取 C 库接口使用说明。

示例——查询 `strncpy` 接口的使用说明：
```bash
man strncpy
```

输出示例：
```
STRNCPY(3)

NAME
       strcpy, strncpy - copy a string

SYNOPSIS
       #include <string.h>
       char *strcpy(char *dest, const char *src);
       char *strncpy(char *dest, const char *src, size_t n);

DESCRIPTION
       The strcpy() function copies the string pointed to by src, including
       the terminating null byte ('\0'), to the buffer pointed to by dest...
       The strncpy() function is similar, except that at most n bytes of src
       are copied. Warning: If there is no null byte among the first n bytes
       of src, the string placed in dest will not be null-terminated.
```

### 6.8 应用程序启动

模块的启动管理使用 **sysv** 方式。启动顺序为先启动 `/etc/rcS.d` 目录下的启动脚本，然后再启动 `/etc/rc5.d` 目录下的启动脚本。启动脚本按照数字由小到大的优先级顺序启动。推荐客户将启动脚本存放在 `/etc/rc5.d` 目录下，脚本优先级使用大于等于 **99** 的数字标记。详情请参考**文档 [7]**。

### 6.9 日志系统

考虑到系统安全，用户在进行应用软件开发的时候，不应该把应用程序日志输出到串口，而是应该使用日志系统对日志进行管理。推荐使用系统自带的 **Logcat** 日志系统进行日志管理，详情请参考**文档 [8]**。

Logcat 日志系统除了保存客户应用程序的日志以外，还保存内核日志和执行 `logcat -b events` 后输出的日志，便于以后排查问题。

### 6.10 静态代码扫描

静态代码扫描指用户写好源代码后，无需经过编译器编译，直接使用扫描工具对源码进行扫描，从而找出代码中存在的语义缺陷或安全漏洞。本章节主要介绍 C/C++ 静态代码扫描工具 **CPPCHECK** 及其使用方法。详情请参阅 CPPCHECK 的网址：http://cppcheck.net。

#### 6.10.1 CPPCHECK 安装

在 Ubuntu 系统下，执行如下命令安装 CPPCHECK 工具：

```bash
sudo apt-get install cppcheck
```

#### 6.10.2 CPPCHECK 使用

在 Ubuntu 系统下，进入源码目录，使用 CPPCHECK 工具执行相应的命令对源码目录进行扫描，并根据错误提示，对代码进行修复。命令格式为：

```bash
cppcheck --enable=all <源码目录>
```

扫描结果示例如下：

```
[test_sdk_api/m_data_call.c:423] (style) The scope of the variable 'ret' can be reduced
[test_sdk_api/m_data_call.c:539] (style) The scope of the variable 'i' can be reduced
[test_sdk_api/m_dn.c:239] (style) The scope of the variable 'ret' can be reduced
[test_sdk_api/m_dn.c:75] (portability) scanf without field width limits can crash with huge input data on some versions
[test_sdk_api/m_dn.c:173] (portability) scanf without field width limits can crash with huge input data on some versions
[test_sdk_api/m_dn.c:209] (error) Memory leak: buf
[test_sdk_api/m_nw.c:373] (warning) %d in format string (no. 1) requires a signed integer given in the argument list
...
```

### 6.11 开源软件

模块默认集成了一些需要使用的开源软件。对于未集成的开源软件，需要客户自行集成。本章节列出模块已经集成的开源软件以及移植开源软件的参考手册。

#### 6.11.1 模块已集成的开源软件

联系移远通信技术支持获取模块已集成的开源软件信息。

#### 6.11.2 开源软件移植

在 SDK 中移植开源软件和在 Linux 系统下移植开源软件类似。移远通信已经移植了一些开源软件，如需移植其他开源软件，可参考**文档 [9]** 所介绍的移植方法。

---

## 7 文件下载

模块支持多种方式下载文件。在调试过程中，使用 ADB 命令进行下载最为便捷。执行 `adb push` 可将上位机的本地文件下载到模块的文件系统，命令常用格式为：

```bash
adb push <本地路径> <模块路径>
```

例如，将 `~/sdk/sample/ql-ol-sdk/ql-ol-extsdk/example/hello_world/` 目录下的 `helloworld` 文件下载至模块的 `/data/` 目录下，需执行如下命令：

```bash
adb push ~/sdk/sample/ql-ol-sdk/ql-ol-extsdk/example/hello_world/helloworld /data
```

---

## 8 模块开机检查

模块启动后，可以通过一系列 AT 命令检查模块是否处于正常工作状态，具体步骤如下：

**第一步**：通过 USB 串口线将 PC 连接至模块的 USB AT 口。
**第二步**：插入 (U)SIM 卡和天线后，将模块上电。
**第三步**：使用 **QCOM** 工具，依次执行如下命令：

| AT 命令 | 功能 |
|---|---|
| `AT` | 检测串口是否正常 |
| `AT+CPIN?` | 检测 (U)SIM 卡 |
| `AT+CSQ` | 检测信号强度 |
| `AT+CGREG?` | 检测模块注网状态 |
| `AT+COPS?` | 查询运营商 |
| `AT+QNWINFO` | 查询网络制式 |
| `ATDxxx;` | 语音测试（xxx 为手机号码） |

下图为插入一张普通中国电信卡的开机检查流程示例：

```
AT
OK
AT+CPIN?
+CPIN: READY

OK
AT+CSQ
+CSQ: 31,99

OK
AT+CGREG?
+CGREG: 0,1

OK
AT+COPS?
+COPS: 0,0,"CHN-CT",7

OK
AT+QNWINFO
+QNWINFO: "FDD","46011","LTE BAND 1",100

OK
ATD18501729042;
OK
```

> **备注**：
> 1. 有关 AT 命令详情，请参考**文档 [1]**。
> 2. 有关 QCOM 工具的使用方法，请参考**文档 [10]**。

---

## 9 附录：参考文档及术语缩写

### 参考文档（表 7）

| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_EC2x&EG9x&EG2x-G&EM05_Series_AT_Commands_Manual |
| [2] | Quectel_Customer_FW_Download_Tool_用户指导 |
| [3] | Quectel_EC25&EC21&EC20 R2.0_QFlash_Linux&Android_用户指导 |
| [4] | Quectel_LTE&5G_QFirehose_Linux&Android_用户指导 |
| [5] | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_DFOTA_应用指导 |
| [6] | Quectel_WCDMA&LTE_Linux_USB_Driver_用户指导 |
| [7] | Quectel_EC2x 系列_QuecOpen_开机自启动服务指导 |
| [8] | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_日志系统应用指导 |
| [9] | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_Linux_VPN 移植指导 |
| [10] | Quectel_QCOM_User_Guide |

### 术语缩写（表 8）

| 缩写 | 英文描述 | 中文描述 |
|---|---|---|
| ADB | Android Debug Bridge | Android 调试桥 |
| API | Application Programming Interface | 应用程序编程接口 |
| DM | Device Management | 设备管理 |
| FOTA | Firmware Over-the-Air | 空中固件升级 |
| GNSS | Global Navigation Satellite System | 全球导航卫星系统 |
| GPIO | General-purpose input/output | 通用型之输入输出 |
| I2C | Inter-Integrated Circuit | 内置集成电路 |
| I2S | Inter-IC Sound | 集成电路内置音频 |
| ID | Identification | 身份 |
| LTE | Long Term Evolution | 长期演进 |
| PC | Personal Computer | 个人电脑 |
| SDK | Software Development Kit | 软件开发工具包 |
| SMS | Short Messaging Service | 短消息业务 |
| SPI | Serial Peripheral Interface | 串行外设接口 |
| (U)SIM | (Universal) Subscriber Identity Module | （全球）用户识别卡 |
| UART | Universal Asynchronous Receiver/Transmitter | 通用异步收发传输器 |
| USB | Universal Serial Bus | 通用串行总线 |
| VID | Vender ID | 生产商编号 |
