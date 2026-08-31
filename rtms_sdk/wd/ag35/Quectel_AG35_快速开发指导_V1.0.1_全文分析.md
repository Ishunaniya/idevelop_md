# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) 快速开发指导 —— 全文分析

> **文档全名**：AG35-CET&AG35-EUT QuecOpen(SDK) QuecOpen 快速开发指导
> **适用模块**：AG35-CET、AG35-EUT（LTE Standard 模块系列）
> **版本**：1.0.1
> **日期**：2024-08-09
> **状态**：临时文件（Preliminary / Quectel Confidential）
> **作者**：William LIU
> **总页数**：27 个物理页（封面页不编号，正文页脚标记为 1/26 ~ 26/26）
> **本分析覆盖**：全部 27 页（封面 + 正文第 1~26 页）

---

## 文档定位与核心结论（速读）

本手册是 AG35-CET / AG35-EUT 两款模块基于 **QuecOpen® 方案（SDK 构建环境）** 的「快速上手」类文档。它**不是 API 参考手册**，而是把"拿到模块到能跑出第一个程序"的完整链路串起来：固件升级 → 开发环境搭建（USB 驱动 / ADB / 交叉编译工具链 / SDK） → SDK 目录结构与编译命令 → 文件系统/内核镜像制作 → 日志系统 → API/外设接口文档索引 → 应用启动管理 → 静态代码扫描 → 开机 AT 自检。真正每个功能（蜂窝网络、(U)SIM、数据拨号、SMS、GNSS、Wi-Fi、GPIO/I2C/SPI/USB/UART 等）的细节都被**外链到各自的专题开发指导文档**，本文只给索引。

几个对工程落地最关键的硬事实：
- **QuecOpen 是基于 Linux（OpenWrt rootfs）的嵌入式平台**，shell 提示符为 `root@OpenWrt`，应用启动用 **SystemV** 风格（`/etc/rc.d/`）。
- **交叉编译工具链固定**：`arm-openwrt-linux-gcc`，GCC 8.4.0 + glibc，默认装到 `/opt/ql_crosstools`。
- **内核版本 5.4.195**（由 `make kernel_module` 生成路径 `ql-ol-rootfs/lib/modules/5.4.195/` 推断）。
- **USB VID = 0x2c7c**（Quectel 厂商号），ADB 枚举必须先写入这个 VID。
- **日志系统沿用 Android Logcat**（`log/log.h`，链接 `-llog`），不是标准 syslog。
- 固件包 / SDK / 工具链三件套，前两者版本号必须匹配，工具链一般不变只装一次。

---

## 封面 + 法律声明页（物理第 1~3 页 / 正文页脚前 2 页）

- **封面**：标题 "AG35-CET&AG35-EUT QuecOpen(SDK) 快速开发指导"，LTE Standard 模块系列，版本 1.0.1，日期 2024-08-09，状态"临时文件"。
- **联系方式**：上海移远通信技术股份有限公司，地址上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼，邮编 200233，电话 +86 21 5108 6236，邮箱 info@quectel.com；技术支持 support@quectel.com。
- **前言/使用和披露限制**：标准 Quectel 法务样板——许可协议（保密、限本项目使用）、版权声明、商标、第三方权利、隐私声明（"特定设备数据将会上传至移远或第三方服务器，包括运营商、芯片供应商"——对车联网数据合规有提示意义）、免责声明 4 条。版权 © 2024 上海移远通信 / Quectel Wireless Solutions Co., Ltd.
- **解读**：隐私声明里"数据上传运营商/芯片供应商"这一句，对做出口/海外项目时的数据合规评估值得留意。

---

## 文档历史（修订记录）

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2023-06-10 | William LIU | 文档创建 |
| 1.0.0 | 2023-06-10 | William LIU | 临时版本 |
| 1.0.1 | 2024-08-09 | William LIU | 临时版本：1) 基于 QuecOpen 方案统一命名，更新文档名称；2) 更新交叉编译工具链（第 3.3 章、第 5.4 章）；3) 更新 API 接口参考文档（第 6.5 章）；4) 新增外设接口参考文档（第 6.6 章） |

**解读**：从 1.0.0 到 1.0.1 的关键变化是**工具链版本被更新过**（即手册里写的 `ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain.tar.gz` 是较新的一版，老固件可能配的是别的 GCC 版本，移植/对齐时要注意工具链版本与固件基线匹配）。第 6.6"外设接口"一章是 1.0.1 才新增的。

---

## 目录 / 表格索引 / 图片索引（正文第 4~7 页）

正文共 8 章。表格 9 个、插图 2 个：
- **表 1**：关键固件包文件列表（p10）
- **表 2**：SDK 文件说明（p17）
- **表 3**：SDK 根目录下常用的编译命令（p18）
- **表 4**：示例程序（p18）
- **表 5**：日志等级对应日志接口（p20）
- **表 6**：API 接口使用方法相关文档（p22）
- **表 7**：外设接口相关文档列表（p23）
- **表 8**：参考文档（p26）
- **表 9**：术语和缩写（p26）
- **图 1**：AG35-CET 软件包（p10）
- **图 2**：模块开机检查流程命令（p25）

---

## 第 1 章 引言（正文 p8）

- AG35-CET 和 AG35-EUT 模块支持 **QuecOpen® 方案**。QuecOpen 是**基于 Linux 的嵌入式开发平台**，用于简化 **IoV（车联网）应用**的软件设计和开发过程。
- 本文档适用于 **SDK 构建环境的 QuecOpen 方案**（区别于其它非 SDK 的方案），主要介绍快速开发流程：固件升级、软件开发环境准备、SDK 的使用、模块开机检查等操作流程。

**解读**：明确"IoV"定位，与本仓库 modem_mng 做 4G 拨号守护进程的车载场景吻合。"SDK 构建环境"是一个限定词——AG35 还可能有其它开发形态（如纯 AT 模式），本文只谈 SDK。

---

## 第 2 章 QuecOpen® 软件开发概述（正文 p9）

### 2.1 对开发者的要求
1. 熟悉标准 GNU/Linux 应用开发，以及常见 Linux 系统命令；
2. 掌握一些驱动、网络协议基本知识；
3. 了解 AG35-CET 和 AG35-EUT 模块的 **AT 命令**知识。

### 2.2 开发流程（4 步）
1. **准备硬件开发环境**：系统需为 **Ubuntu 18.04 64 位**，内存 **4 GB 以上**，CPU **4 核以上**；若用虚拟机，分配给虚拟机的内存 **≥ 4 GB**。
2. **确认固件版本和 SDK 版本匹配**，若不匹配，升级固件至指定版本（→ 第 4 章）。
3. **准备软件开发环境**：安装 USB 驱动、ADB 工具、交叉编译工具、SDK（→ 第 5 章）。
4. **使用 SDK 进行开发**（参考示例程序）（→ 第 6 章）。

**解读**：Ubuntu 18.04 是硬约束（工具链是为该发行版构建的 glibc 版本）。在更新的 Ubuntu 上跑这套 GCC 8.4.0 工具链可能遇到 glibc 不兼容问题——本仓库交叉编译若用更新主机，需注意。

---

## 第 3 章 软件包介绍（正文 p10~11）

每个发布版本含三件套：**固件包 + SDK 软件开发包 + 交叉编译工具包**。其中固件包与 SDK 软件开发包**版本号相同**（用 SDK 开发时必须用对应版本固件）；交叉编译工具包版本通常不变，**仅需安装一次**。

**图 1：AG35-CET 软件包**示例（三个压缩包及大小）：
| 文件名 | 类型 | 大小 |
|---|---|---|
| `AG35CETCAR01A07M2G_OCPU.zip` | ZIP | 126,345 KB（≈123 MB，固件包） |
| `ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain.tar.gz` | GZ | 362,231 KB（≈354 MB，工具链） |
| `ql-ol-extsdk-ag35cetcar01a07m2g_ocpu.tar.gz` | GZ | 300,214 KB（≈293 MB，SDK） |

### 3.1 固件包
- 命名格式：`<模块OC字段><基线版本号><软件版本号><内存大小>_OCPU.zip`。
- 以 `AG35CETCAR01A01M2G_OCPU.zip` 为例：
  - 模块 OC 字段：`AG35CET`（AG35-CET 模块）
  - 基线版本号：`R01`
  - 软件版本号：`A01`
  - 内存大小：`M2G`
  - `OCPU`：QuecOpen 版本

**表 1：关键固件包文件列表**
| 文件 | 文件类型 | 描述 |
|---|---|---|
| `updata/update.blf` | 配置文件 | 移远升级工具用它对模块升级，**请勿改动** |
| `update/ARBEL.bin` | 镜像文件 | **Modem 镜像**文件 |
| `update/u-boot.bin` | 镜像文件 | **uboot 镜像**；可替换为用 SDK 制作的 uboot 镜像 |
| `update/zImage` | 镜像文件 | **内核镜像**；可替换为用 SDK 制作的内核镜像（`make kernel` 产物） |
| `update/root.squashfs` | 镜像文件 | **根文件系统镜像**；可替换为用 SDK 制作的 rootfs（`make rootfs` 产物） |
| `update/oemapp.squashfs` | 镜像文件 | **客户应用 app 镜像**；可替换为客户自己的应用镜像 |

**解读**：升级时**逐个分区镜像**替换是这套量产/OTA 流程的核心——自研代码最终打进 `oemapp.squashfs`；改内核驱动改 `zImage`；改根文件系统改 `root.squashfs`。`ARBEL.bin`（Modem 基带固件）和 `update.blf`（升级描述）不可动。注意第一处目录拼写为 `updata/`、其余为 `update/`，应为原文笔误，实际目录应统一为 `update/`。

### 3.2 SDK 软件开发包
- 命名格式：`ql-ol-extsdk-<模块OC字段><基线版本号><软件版本号><内存大小>_ocpu.tar.gz`。
- 以 `ql-ol-extsdk-ag35cetcar01a01m2g_ocpu.tar.gz` 为例：OC 字段 AG35CET / 基线 R01 / 软件版本 A01 / 内存 M2G / ocpu=QuecOpen 版本。
- 用法详见第 6 章。

### 3.3 交叉编译工具包
- 运行在 Ubuntu 18.04 64 位上的 **GCC 交叉编译工具链**，安装方法见第 5.4 章。
- 命名格式：`ql-<模块OC字段>-<基线版本号>-<GCC版本号>-<工具链版本号>-toolchain.tar.gz`。
- 以 `ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain.tar.gz` 为例：
  - 模块 OC 字段：`AG35`（AG35 模块）
  - 基线版本号：`1806E`
  - GCC 版本号：`8.4.0-glibc`
  - 工具链版本号：`V1`

**解读**：工具链基线 `1806E` 与固件基线 `R01` 命名体系不同（工具链跨固件版本通用，所以工具链名里用的是 SoC/平台级基线）。本仓库 cross-profiles 里的 AG35 toolchain cmake 文件应指向这个 `arm-openwrt-linux-gcc`。

---

## 第 4 章 固件升级（正文 p12）

AG35-CET / AG35-EUT 支持多种固件升级方式，本章介绍两种。

### 4.1 使用移远通信升级工具升级
- Windows 上位机升级工具的详细方法 → **文档 [1]**（`Quectel_Customer_FW_Download_Tool_User_Guide`）。

### 4.2 紧急升级
- 适用场景：开发调试阶段，**模块无法正常启动**，或上位机**无法识别模块 USB 端口**时。
- 操作：**短接 `VDD_EXT` 引脚和 `USB_BOOT` 引脚，重新给模块上电**，使模块进入**紧急下载模式**，再用上位机工具升级。
- **备注**：短接 VDD_EXT 和 USB_BOOT 的具体方法，需咨询移远技术支持。

**解读**：这是"砖了也能救回来"的兜底通道（强制 boot ROM 下载模式）。对应本仓库 modem_mng 里 L3 恢复"纯 exit 救不了挂死的 CP 固件"的那种极端情形——真正硬恢复要靠这种引脚强制下载或整模块重刷。

---

## 第 5 章 软件开发环境准备（正文 p13~16）

准备 5 步：1) 装 USB 驱动；2) 装 ADB 工具；3) 下载文件到模块；4) 装交叉编译工具；5) 装 SDK。

### 5.1 USB 驱动安装
- 上位机需装 USB 驱动才能通过 USB 与模块通信。
- Linux / Android 系统装 USB 驱动详细步骤 → **文档 [3]**（`Quectel_WCDMA&LTE_Linux_USB_Driver_User_Guide`）。
- Windows 通过 USB 驱动安装包直接安装；安装包需联系移远技术支持获取。

### 5.2 ADB 工具安装
- **ADB（Android Debug Bridge）**：命令行工具，实现上位机与设备通信，可执行设备操作（安装/调试应用），并提供 Linux shell 访问权限。本章讲 Ubuntu 下安装。
- 备注：ADB 官方指南 https://developer.android.google.cn/studio/command-line/adb

#### 5.2.1 安装 ADB 驱动
1) 在上位机 shell 执行：
```bash
sudo apt-get update
sudo apt-get install android-tools-adb
```
2) 若上面失败，执行如下重装（成功则跳过）：
```bash
sudo add-apt-repository ppa:nilarimogard/webupd8
sudo apt-get update
sudo apt-get install android-tools-adb
```
3) 执行 `adb` 查看是否安装成功，成功显示版本号，如：`Android Debug Bridge version 1.0.32`。

#### 5.2.2 添加模块 USB VID
设置 ADB 使用的 USB VID 为移远模块的 VID，即 **`0x2c7c`**：
```bash
cd ~
sudo echo 0x2c7c > .android/adb_usb.ini
```
> **注意**：`sudo echo ... > file` 这种写法因重定向由当前 shell（非 root）执行，遇到 `.android/` 权限/不存在时可能不生效；实际操作中通常要先 `mkdir -p ~/.android` 并用 `tee`。这是手册里一个易踩的坑。

#### 5.2.3 枚举设备
```bash
sudo adb kill-server
sudo adb devices
```
检查 ADB 是否能正确识别移远模块。

### 5.3 文件下载
- AG35-CET / AG35-EUT 支持多种下载方式；调试中用 **ADB 命令最便捷**。
- `adb push` 把上位机本地文件下到模块文件系统，格式：`adb push <本地路径> <模块路径>`。
- 示例（把 SDK 里 helloworld 上传到模块 `/data/`）：
```bash
adb push ~/sdk/ql-ol-extsdk-ag35cetcar01a01m2g_ocpu/sample/helloworld/helloworld /data
```

### 5.4 交叉编译工具安装
工具链只需装一次，以 `ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain.tar.gz` 为例。

#### 5.4.1 创建工作目录
默认工作目录 `/opt/ql_crosstools`，不存在则：
```bash
sudo mkdir –p /opt/ql_crosstools
```

#### 5.4.2 安装交叉编译工具链
把压缩包复制到 `/opt/ql_crosstools` 下解压：
```bash
tar xvf ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain.tar.gz -C /opt/ql_crosstools
```

#### 5.4.3 检查是否安装成功
```bash
/opt/ql_crosstools/ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain/bin/arm-openwrt-linux-gcc --version
```
成功输出示例：`arm-openwrt-linux-gcc (Openwrt GCC 8.4.0 r0-552b312bd0) 8.4.0`，Copyright (C) 2018 Free Software Foundation。

**解读**：
- 编译器前缀 **`arm-openwrt-linux-gcc`**、安装根 `/opt/ql_crosstools/ql-ag35-1806e-gcc-8.4.0-glibc-v1-toolchain/`——这是写 CMake toolchain file / Makefile 时的关键路径与三元组。
- GCC 8.4.0 来自 OpenWrt 工具链（解释了为什么 rootfs 是 OpenWrt、提示符是 `root@OpenWrt`）。

### 5.5 SDK 安装
SDK 压缩包可装到上位机任意本地目录，但**必须在 Ubuntu 普通用户**环境下进行（非 root）。以 `ql-ol-extsdk-ag35cetcar01a01m2g_ocpu.tar.gz` 为例。

#### 5.5.1 SDK 软件开发包解压
```bash
tar xvf ql-ol-extsdk-ag35cetcar01a01m2g_ocpu.tar.gz
cd ql-ol-extsdk-ag35cetcar01a01m2g_ocpu
```

#### 5.5.2 检查是否安装成功（编译 helloworld）
```bash
cd ql-ol-extsdk-ag35cetcar01a01m2g_ocpu
cd sample/helloworld/
make
```
编译成功生成 `helloworld` 可执行文件，即说明 SDK 安装成功。

#### 5.5.3 烧录验证
把 `helloworld` 下载到模块（如 `/tmp`），加可执行权限运行：
```bash
root@OpenWrt:/tmp# chmod +x helloworld
root@OpenWrt:/tmp# ./helloworld
hello quectel
```
运行成功即表明编译成功。

**解读**：SDK 必须以普通用户解压/编译（root 会破坏文件属主与 OpenWrt 构建假设）；模块端提示符 `root@OpenWrt` 印证 rootfs 是 OpenWrt。

---

## 第 6 章 SDK 使用方法（正文 p17~24）

### 6.1 SDK 文件说明（表 2）
| 目录 | 说明 |
|---|---|
| `ql-sysroots` | 交叉编译使用的**头文件和库文件**（API 头文件在此，见 6.5） |
| `ql-ol-kernel` | Linux **内核源码** |
| `ql-ol-rootfs.tar.gz` | Linux **根文件系统压缩包** |
| `tools` | 编译使用的二进制文件、脚本和配置文件 |
| `sample` | **示例代码**文件夹 |
| `sdk.mk` | **公用的 Makefile 头文件**（定义编译环境变量，被各子目录 Makefile include） |
| `ql-ol-rootfs` | 编译 rootfs 时，解压 `ql-ol-rootfs.tar.gz` 生成的目录 |
| `oemapp` | **客户文件系统源文件夹** |
| `target` | 目标文件夹，编译的镜像文件和临时文件存放在此 |

### 6.2 编译方式
- SDK 编译由 **Makefile 规则**组织：顶层 Makefile 编译内核和文件系统镜像；`sample/` 下各子目录的 Makefile 编译各示例程序。可直接进入 `sample/` 子目录运行 `make` 编译。
- 各目录 Makefile 都 include 了 `sdk.mk`，`sdk.mk` 定义编译用环境变量。

**表 3：SDK 根目录下常用的编译命令**
| 命令 | 说明 |
|---|---|
| `make rootfs` | 编译 rootfs 镜像，产物在 `target/`（`root.squashfs`） |
| `make kernel` | 编译内核镜像，产物在 `target/`（`zImage`） |
| `make kernel_module` | 编译内核模块，产物在 `ql-ol-rootfs/lib/modules/` 下 |
| `make sample` | 编译所有示例程序，二进制在 `sample/` 各子目录 |

**表 4：示例程序**
| 目录 | 说明 |
|---|---|
| `sample/kmodule` | 自定义内核模块示例程序 |
| `sample/helloworld` | 用户程序示例 |
| `sample/test_sdk_api` | **SDK 的 API 接口测试程序**（最有参考价值——各 API 用法范例在此） |

### 6.3 文件系统镜像制作

#### 6.3.1 制作根文件系统镜像
- rootfs 压缩包名 `ql-ol-rootfs.tar.gz`。SDK 根目录运行 `make rootfs` 制作，产物 `target/root.squashfs`。
- `make rootfs` 两阶段：
  - **第一阶段**：解压 `ql-ol-rootfs.tar.gz`，解压出目录 `ql-ol-rootfs/`。
  - **第二阶段**：调用制作文件系统的相关工具。
- Makefile 中解压 `ql-ol-rootfs.tar.gz` 的代码片段（截图）：
```makefile
if [ ! -d ${QL_ROOTFS_DIR} ]; then \
        printf "Extracting the rootfs..."; \
        mkdir -p ${QL_ROOTFS_DIR}; \
        tar -xvf ${QL_SDK_DIR}/ql-ol-rootfs.tar.gz -C ${QL_ROOTFS_DIR}; \
        echo "Done."; \
fi
```
- 制作镜像片段（截图，关键工具 `mksquashfs`）：使用 `${QL_TOOLS_BIN_DIR}/mksquashfs ${QL_ROOTFS_DIR}/... root.squashfs -noappend -comp xz ...`（生成 squashfs，xz 压缩）。
- 生成的 `root.squashfs` 可替换固件包 `update/` 下同名文件，再用移远升级工具升级（步骤见第 4 章）。

**解读**：`make rootfs` 第一阶段**只在 `ql-ol-rootfs/` 不存在时解压**（`if [ ! -d ... ]` 守卫）。这意味着：要让对 rootfs 内容的修改重新打入镜像，光改 tar 包不行，要么删掉 `ql-ol-rootfs/` 目录重解压，要么直接改 `ql-ol-rootfs/` 里的文件后再 `make rootfs`。

#### 6.3.2 制作内核镜像文件
- `make kernel` 编译内核并制作镜像，产物 `target/zImage`，可替换固件包 `update/zImage` 升级。
- 若改了**内核 ko 动态加载驱动代码**，重编内核后还要 `make kernel_module` 重编内核驱动模块，产物放在 rootfs 的 **`ql-ol-rootfs/lib/modules/5.4.195/`**（该目录对应 **Linux 内核版本 5.4.195**，随内核版本升级而改变），且需重新制作 rootfs 镜像用于升级。

**解读**：明确 **Linux 内核 5.4.195**。改驱动是"改 ko → make kernel → make kernel_module → make rootfs"的三连，缺一不可（ko 装进 rootfs，不进 zImage）。

### 6.4 日志系统
- 系统支持 **Android 的 Logcat 日志系统**。头文件路径 `ql-sysroots/usr/include/log.h`，示例程序 `sample/log_logcat`。
- 应用调用 `log.h` 中定义的宏（日志接口），把日志写入 **logcat 日志缓冲区**，用 `logcat` 查看，或保存到存储空间。官方指南 https://developer.android.google.cn/studio/command-line/logcat
- 每条日志含：**优先级 Priority + 标识来源的 Tag + 实际消息**。
- Logcat 定义 **6 个日志等级**，优先级由低到高：**VERBOSE、DEBUG、INFO、WARN、ERROR、FATAL**。
  - **VERBOSE** 仅在开启 `LOG_NDEBUG` 宏后才输出；
  - **FATAL** 标识严重错误，**不提供日志接口**（不可由应用主动打）。
- 重新封装日志接口可参考 `ql-sysroots/usr/include/android/log.h`。

**表 5：日志等级对应日志接口**
| 日志等级 | 日志接口 |
|---|---|
| VERBOSE | `ALOGV` |
| DEBUG | `ALOGD` |
| INFO | `ALOGI` |
| WARN | `ALOGW` |
| ERROR | `ALOGE` |
（FATAL 无对应应用接口）

#### 6.4.1 在应用程序中使用日志接口
- **Makefile 链接选项**要加 **`-llog`**（链接 Logcat 的库）。Makefile 示例（截图）：
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
- **源码**：引用 Logcat 头文件前，先定义日志 Tag 及是否开启 VERBOSE 输出：
```c
#include <stdio.h>

/** 定义 log tag */
#define LOG_TAG "ql_log_test"

/** 打开调试开关，ALOGV 会输出 log */
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

**解读关键坑**：注释写"打开调试开关 `#define LOG_NDEBUG 1`，ALOGV 会输出 log"。但按 Android 惯例 **`LOG_NDEBUG 1` 是关闭 VERBOSE，`LOG_NDEBUG 0` 才输出 VERBOSE**——这与正文 6.4 的"VERBOSE 仅在开启 LOG_NDEBUG 后输出"一致但与 Android 标准语义相反，**也与下方实测输出对不上**（见 6.4.2，实测日志里没有 VERBOSE 行）。这说明手册此处注释/正文表述与实际行为存在矛盾，移植时**以实测为准**：要想看到 ALOGV，应设 `LOG_NDEBUG 0`。这是一个值得记入经验的文档级 bug。

#### 6.4.2 查看应用程序日志
- 用 `logcat` 按 Tag 和等级显示日志，格式：`logcat -s <日志的Tag[:显示日志的等级]>`。
- 以查询 Tag=`ql_log_test` 为例：
```text
/data # logcat -s ql_log_test
--------- beginning of main
01-21 06:18:30.182  1366  1366 D ql_log_test: I am LOG_DEBUG
01-21 06:18:30.182  1366  1366 I ql_log_test: I am LOG_INFO
01-21 06:18:30.182  1366  1366 W ql_log_test: I am LOG_WARN
01-21 06:18:30.182  1366  1366 E ql_log_test: I am LOG_ERROR
        beginning of system
```
- **实测只有 D/I/W/E 四行，没有 V（VERBOSE）行** —— 印证上面对 `LOG_NDEBUG` 的判断：默认配置下 VERBOSE 未输出。
- 日志行格式：`日期 时间 PID TID 等级字母 Tag: 消息`。

#### 6.4.3 日志保存
- Logcat 支持把日志**分段、循环覆盖**保存到文件，格式：
  `<logcat 日志的Tag[:显示日志的等级]> -r <单个日志文件大小（单位 K）> -n <日志文件个数> -f <日志文件名>`
- 示例（把所有日志保存到 `/usrdata/log.txt`，单文件 100 K，文件数 5，后台运行）：
```bash
/usrdata # logcat -f /usrdata/log.txt -r 100 -n 5 &
```
- 查看产生的文件（`ls -al`）：会生成 `log.txt`、`log.txt.1`、`log.txt.2`、`log.txt.3`、`log.txt.4` 共 5 个，每个约 100 KB（102443/102438/... 字节），循环覆盖。

**解读**：`-r 100 -n 5` 即 100KB×5≈500KB 的环形日志，写满后从 `.4` 回卷。这是模块自带的日志落盘能力。对照本仓库 `logger_sd.c` 自己实现的 SD 卡分天/40 天保留日志——AG35 平台其实可直接复用 logcat 落盘，但自研 `logger_sd.c` 给的是更可控的按天目录+保留策略，二者并存不冲突。

### 6.5 API 接口
- SDK 提供一组 API，头文件位于 **`ql-sysroots/usr/include/ql-sdk/`**。部分 API 有测试程序，路径 `sample/test_sdk_api/`。

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

**解读**：API 头文件在 `ql-sysroots/usr/include/ql-sdk/`，这是本仓库 AG35 移植里调用 SDK API（如数据拨号 `ql_data_call_*`）时 include 路径的根。`sample/test_sdk_api/` 是各 API 的活范例，移植时优先参考。每个功能都有独立专题文档（本目录下其它 md 正是对应这些文档的全文分析）。

### 6.6 外设接口（1.0.1 新增）
- AG35-CET / AG35-EUT 提供多种外设接口：**GPIO、I2C、SPI、USB、UART** 等。

**表 7：外设接口相关文档列表**
| 功能 | 参考文档 |
|---|---|
| GPIO | `..._GPIO_开发指导` |
| I2C | `..._I2C_开发指导` |
| SPI | `..._SPI_开发指导` |
| USB | `..._USB_开发指导` |
| UART | `..._UART_开发指导` |

### 6.7 应用程序启动
- AG35-CET / AG35-EUT 的启动管理用 **SystemV 方式**。启动顺序：先启动 `/etc/rc.d/` 目录下的启动脚本，脚本按**优先级数字由小到大**顺序启动。
- **推荐**：客户把启动脚本放在 `/etc/rc.d/` 下，**脚本优先级数字设为 99（最低优先级）**，表示系统服务启动完毕后再启动用户服务，避免服务依赖问题。

**解读**：这是自研 daemon（如 modem_mng）在 AG35 上**开机自启的官方做法**——放 `/etc/rc.d/S99xxx`。优先级 99 确保 4G/SDK 底层服务都起来后再起拨号守护，避免拨号进程比 modem 服务先跑而失败。

### 6.8 静态代码扫描
- 用免费 C/C++ 静态扫描工具 **CPPCHECK**（无需编译，直接扫源码找语法缺陷、安全漏洞）。官网 http://cppcheck.net

#### 6.8.1 安装 CPPCHECK
```bash
sudo apt-get install cppcheck
```

#### 6.8.2 使用 CPPCHECK 扫描代码
- 进入源码目录，格式：`cppcheck --enable=all <源码目录>`。
- 扫描结果示例（截图，扫的是 `test_sdk_api/` 下源码，含 `m_data_call.c`、`m_dm.c`、`m_nw.c` 等）：
```text
[test_sdk_api/m_data_call.c:423]: (style) The scope of the variable 'ret' can be reduced.
[test_sdk_api/m_data_call.c:539]: (style) The scope of the variable 'i' can be reduced.
[test_sdk_api/m_dm.c:239]: (style) The scope of the variable 'ret' can be reduced.
[test_sdk_api/m_dm.c:75]:  (portability) scanf without field width limits can crash with huge input data ...
[test_sdk_api/m_dm.c:173]: (portability) scanf without field width limits ...
[test_sdk_api/m_dm.c:209]: (error) Memory leak: buf      ← 唯一一条 error 级（红框标注）
[test_sdk_api/m_nw.c:373]: (warning) %d in format string (no.1) requires a signed integer ...
[test_sdk_api/m_nw.c:393/412]: (warning) %d in format string ...
[test_sdk_api/m_nw.c:295/308/312/630/648]: (portability) scanf without field width limits ...
```
- 根据错误提示对代码修复。

**解读**：连**官方 `test_sdk_api` 示例代码**都被 cppcheck 扫出一条 `(error) Memory leak: buf`（`m_dm.c:209`）和大量 `scanf` 无字段宽度限制（可被超长输入打崩）、`%d` 接无符号实参等问题。这与本仓库 EG25/EC200A 审计中发现的"`Ql_SendAT` 缓冲溢出""`scanf`/`stoi` 解析 AT 应答崩溃"是同源风险——**移远示例代码本身不可盲信**，移植时必须加固缓冲边界和解析容错。建议把 cppcheck 纳入 AG35 移植的 CI。

---

## 第 7 章 模块开机检查（正文 p25）

模块启动后，用一系列 AT 命令检查是否正常工作：
- **步骤 1**：用 USB 串口线把上位机连到模块的 **USB AT 口**。
- **步骤 2**：插入 (U)SIM 卡和天线后，模块上电。
- **步骤 3**：用 **QCOM 工具**（用法见 **文档 [2]** `Quectel_QCOM_User_Guide`）逐条执行 AT 命令。

**开机检查 AT 命令清单**：
| 目的 | AT 命令 |
|---|---|
| 检测串口 | `AT` |
| 检测 (U)SIM 卡 | `AT+CPIN?` |
| 检测信号强度 | `AT+CSQ` |
| 检测模块注网状态 | `AT+CGREG?` |
| 查询运营商 | `AT+COPS?` |
| 查询网络制式 | `AT+QCELLINFO?` |
| 语音测试 | `ATDxxx`（xxx 为手机号码） |

**图 2：插入中国联通 SIM 卡的开机检查流程实例**：
```text
AT+CPIN?
+CPIN: READY              ← SIM 就绪

OK
AT+CSQ
+CSQ: 24,99               ← RSSI=24（约 -65dBm，信号好），误码率 99(未知)

OK
AT+CGREG?
+CGREG: 3,0               ← <n>=3 启用网络注册+位置URC, <stat>=0... (见解读)

OK
AT+COPS?
+COPS: 0,2,"46001",7      ← 自动模式, 数字格式PLMN, 46001(中国联通), 7=E-UTRAN(LTE)

OK
AT+QCELLINFO?
+QCELLINFO: LTE B3,19650,46001,56848,365,100126239,-62
                          ← LTE Band3, EARFCN 19650, PLMN 46001, ...,RSRP/电平 -62

OK
ATD18501729042;
                          ← 拨打语音电话(带分号=语音呼叫)
OK
```

**AT 命令逐条解读**：
- `AT` → 回 `OK` 即串口通。
- `AT+CPIN?` → `+CPIN: READY` 表示 SIM 已就绪、无需 PIN。
- `AT+CSQ` → `+CSQ: <rssi>,<ber>`。`24` 对应约 -65dBm（rssi 范围 0~31，越大越好，99=未知）；`99` ber 未知。
- `AT+CGREG?` → `+CGREG: <n>,<stat>`。例中 `3,0`：`<n>=3`（开启网络注册+位置信息 URC）。注意 `<stat>=0` 一般指"未注册且未搜网"——与下面 COPS 已注册 LTE 看似矛盾，可能是 GPRS 注册态(`CGREG`)与 EPS 注册态不同步的瞬态，实际 LTE 数据注册应看 `AT+CEREG?`。这是开机检查时要留意的细节。
- `AT+COPS?` → `+COPS: <mode>,<format>,<oper>,<AcT>`。例 `0,2,"46001",7`：mode=0 自动选网；format=2 数字 PLMN；oper="46001"=中国联通；AcT=7=E-UTRAN（LTE）。
- `AT+QCELLINFO?` → 移远私有命令，回服务小区信息。例 `LTE B3,19650,46001,56848,365,100126239,-62`：制式 LTE、Band 3、EARFCN 19650、PLMN 46001、TAC/CellID 等、末位 -62 为电平（dBm）。
- `ATDxxx;`（**带分号**）→ 发起**语音呼叫**（不带分号是数据呼叫），回 `OK` 表示拨号动作被接受。

**解读**：这套 7 条 AT 是排障"黄金序列"，与本仓库 modem_mng 拨号前自检/恢复路径里用的 `AT+CSQ`/`AT+CPIN`/注网查询完全同源。`AT+QCELLINFO?` 是移远私有、给现场定位信号/频段问题用。COPS 的 `46001`/AcT 解析逻辑也对应仓库里 `getPlmn`/海外选网 `[OPER]` 功能里对 COPS 的处理（注意之前的经验：COPS 解析与 getPlmn 字段会冲突，运营商名显示别用 COPS 解析）。

---

## 第 8 章 附录 参考文档及术语缩写（正文 p26）

**表 8：参考文档**
| 编号 | 文档名称 |
|---|---|
| [1] | `Quectel_Customer_FW_Download_Tool_User_Guide`（固件升级工具，第 4.1 章引用） |
| [2] | `Quectel_QCOM_User_Guide`（QCOM 串口工具，第 7 章引用） |
| [3] | `Quectel_WCDMA&LTE_Linux_USB_Driver_User_Guide`（USB 驱动，第 5.1 章引用） |

**表 9：术语和缩写**
| 缩写 | 英文 | 中文 |
|---|---|---|
| ADB | Android Debug Bridge | Android 调试桥 |
| API | Application Programming Interface | 应用程序编程接口 |
| DM | Device Management | 设备管理 |
| GCC | GNU Compiler Collection | GNU 编译器套装 |
| ID | Identification | 身份 |
| IoV | Internet of Vehicles | 车联网 |
| LTE | Long Term Evolution | 3GPP 长期演进 |
| PC | Personal Computer | 个人电脑 |
| SDK | Software Development Kit | 软件开发工具包 |
| SMS | Short Messaging Service | 短消息业务 |
| (U)SIM | (Universal) Subscriber Identity Module | （通用）用户识别模块 |
| USB | Universal Serial Bus | 通用串行总线 |
| VID | Vender ID | 生产商编号 |

---

## 全文要点汇总（对 AG35 移植的工程意义）

1. **构建环境定型**：Ubuntu 18.04 64 位 + GCC 8.4.0 OpenWrt 工具链（`/opt/ql_crosstools/.../bin/arm-openwrt-linux-gcc`）+ 内核 5.4.195 + OpenWrt rootfs。本仓库 AG35 的 cmake toolchain / 交叉编译 profile 必须与此对齐。
2. **三件套版本耦合**：固件包 ↔ SDK 必须同版本；工具链独立、只装一次。
3. **量产/升级路径**：自研 app → `oemapp.squashfs`；改内核 → `make kernel`(zImage) + `make kernel_module` + `make rootfs`；用移远升级工具按分区替换 `update/` 下镜像。砖机用 VDD_EXT+USB_BOOT 短接进紧急下载。
4. **API 在 `ql-sysroots/usr/include/ql-sdk/`**，活范例在 `sample/test_sdk_api/`，每功能有独立专题文档（本目录其它 md）。
5. **日志两条路**：官方 Logcat（`-llog`/`ALOGx`/`logcat -f -r -n` 落盘）与自研 `logger_sd.c`。注意手册 `LOG_NDEBUG` 注释与实测矛盾——要看 VERBOSE 应设 `LOG_NDEBUG 0`。
6. **开机自启**：`/etc/rc.d/S99xxx`（SystemV，优先级 99 最后启动）。
7. **代码质量**：官方 `test_sdk_api` 示例本身就有 memory leak / scanf 无界 / %d 接无符号等 cppcheck 报错——示例代码不可盲信，移植要加固缓冲与解析容错（与本仓库 Ql_SendAT 溢出、stoi/scanf 崩溃的教训一致）。建议把 `cppcheck --enable=all` 纳入流程。
8. **开机自检黄金 AT 序列**：`AT / AT+CPIN? / AT+CSQ / AT+CGREG? / AT+COPS? / AT+QCELLINFO? / ATDxxx;`，与拨号守护进程的自检/恢复逻辑同源。

<!-- GENERATION_COMPLETE -->
