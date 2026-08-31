# EC200A-CN(TA) QuecOpen eMMC 用户指导 — 完整分析文档

> **文档来源**：Quectel_EC200A-CN(TA)_QuecOpen_eMMC用户指导_V1.0.0_Preliminary_20220712.pdf  
> **版本**：1.0.0  
> **日期**：2022-07-12  
> **状态**：临时文件（Preliminary Document, Not Checked）  
> **产品系列**：LTE Standard 模块系列  
> **厂商**：上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.）

---

## 文档历史

| 版本 | 日期 | 作者 | 变更表述 |
|------|------|------|----------|
| - | 2022-01-08 | Larry ZHANG | 文档创建 |
| 1.0.0 | 2022-07-12 | Larry ZHANG / Eyelyn TANG | 临时版本 |

---

## 目录

1. [引言](#1-引言)
2. [eMMC 硬件接口及引脚定义](#2-emmc-硬件接口及引脚定义)
3. [eMMC 设备树配置及内核配置](#3-emmc-设备树配置及内核配置)
   - 3.1 [设备树文件配置](#31-设备树文件配置)
   - 3.2 [内核配置](#32-内核配置)
     - 3.2.1 [eMMC 驱动配置](#321-emmc-驱动配置)
     - 3.2.2 [Ext4 文件系统](#322-ext4-文件系统)
4. [eMMC 文件系统制作和功能测试验证](#4-emmc-文件系统制作和功能测试验证)
   - 4.1 [搭建 LTE OPEN EVB 硬件测试环境](#41-搭建-lte-open-evb-硬件测试环境)
   - 4.2 [查看 eMMC 设备是否正确识别](#42-查看-emmc-设备是否正确识别)
   - 4.3 [分区管理](#43-分区管理)
   - 4.4 [导入 Ext4 文件系统制作工具](#44-导入-ext4-文件系统制作工具)
   - 4.5 [格式化分区为 Ext4 文件系统](#45-格式化分区为-ext4-文件系统)
   - 4.6 [读写测试](#46-读写测试)
5. [eMMC 常用调试方法](#5-emmc-常用调试方法)
   - 5.1 [简单读写测试](#51-简单读写测试)
   - 5.2 [查看当前系统支持的文件系统](#52-查看当前系统支持的文件系统)
   - 5.3 [获取 eMMC 基本信息](#53-获取-emmc-基本信息)
   - 5.4 [Linux MMC 驱动子系统简述](#54-linux-mmc-驱动子系统简述)
   - 5.5 [打开 Debug 信息](#55-打开-debug-信息)
6. [Mdev 自动挂载](#6-mdev-自动挂载)
7. [附录：参考文档及术语缩写](#7-附录参考文档及术语缩写)

---

## 1 引言

移远通信 EC200A-CN(TA) 模块支持 **QuecOpen®** 方案。QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。

### 基本概念说明

| 名称 | 说明 |
|------|------|
| **SD 卡** | Secure Digital Memory Card，安全数据存储卡，广泛应用于便携式设备（数码相机、个人数码助理、多媒体播放器等） |
| **MMC** | Multi Media Card，多媒体存储卡，是一种闪存（Flash Memory Card）标准，定义了 MMC 的架构以及访问 Flash Memory 的接口和协议 |
| **eMMC** | Embedded Multi Media Card，嵌入式多媒体存储卡，是对 MMC 的拓展，满足更高标准的性能、成本、体积、稳定、易用等需求 |
| **SDIO** | 从 SD 演化出来的安全数字输入输出接口 |

### 关键架构说明

- **Linux Kernel** 使用 MMC 子系统统一管理 eMMC、SD 卡、SDIO Wi-Fi 设备
- eMMC 强调多媒体存储；SD 强调安全和数据保护；SDIO 是安全数字输入输出接口
- 移远通信 EC200A-CN(TA) QuecOpen 模块预留**一路 SDIO 接口**用于连接 eMMC
- 该 SDIO 接口符合 **SD 3.0 协议**

### 本文档内容范围

本文档介绍在 QuecOpen® 方案下，EC200A-CN(TA) 模块 eMMC 开发流程，包括：
- 接口定义
- 电路设计参考
- 设备树设置
- 分区管理
- 调试方法

---

## 2 eMMC 硬件接口及引脚定义

EC200A-CN(TA) 模块提供了支持 **SD 3.0 协议**的 SDIO 接口，用于外接 eMMC 设备，且支持 **eMMC 4.5.1 协议**。

### 表 1：SDIO 接口引脚定义

| 引脚名称 | 引脚号 | I/O | 描述 |
|----------|--------|-----|------|
| VDD_SDIO | 34 | PO | SD 卡 SDIO 总线上拉电源 |
| SD_SDIO_DATA3 | 28 | DO | SDIO 总线 DATA3 |
| SD_SDIO_DATA2 | 29 | DIO | SDIO 总线 DATA2 |
| SD_SDIO_DATA1 | 30 | DIO | SDIO 总线 DATA1 |
| SD_SDIO_DATA0 | 31 | DIO | SDIO 总线 DATA0 |
| SD_SDIO_CMD | 33 | DIO | SDIO 总线命令 |
| SD_SDIO_CLK | 32 | DIO | SD 卡 SDIO 总线时钟 |
| SD_DET | 23 | DI | SD 卡插拔检测 |

> **说明**：有关详细信息，请参考**文档 [2]**（Quectel_EC200A-CN(TA)_QuecOpen_硬件设计手册）。

---

## 3 eMMC 设备树配置及内核配置

### 3.1 设备树文件配置

设备树文件 `ql-asr1803-overlay.dtsi` 位于 `kernel/arch/arm/boot/dts/` 路径下。

检查设备树节点 `sdh1` 的配置信息是否已如下所示增加设备信息。如有差异，请修改设备文件如下：

```dts
&sdh0 {

/* if 1 enable emmc    if 0 enable sd nand */
#if 1
        pinctrl-0 = <&sdh0_pmx_func1 &sdh0_pmx_func2>;
        pinctrl-1 = <&sdh0_pmx_func1_slow &sdh0_pmx_func2_slow>;
        pinctrl-2 = <&sdh0_pmx_func1_fast &sdh0_pmx_func2_fast>;
        /delete-property/ no-mmc;
        no-sd;
        asr,sdh-host-caps2 = <(
                MMC_CAP2_ONLY_1_8V
        )>;

/* prop "sdh-dtr-data":<timing preset_rate src_rate tx_delay rx_delay tx_dline_reg rx_dline_reg> */
        asr,sdh-dtr-data =
<PXA_MMC_TIMING_LEGACY PXA_SDH_DTR_26M PXA_SDH_DTR_104M 0 0 0 0>,
<PXA_MMC_TIMING_MMC_HS PXA_SDH_DTR_52M PXA_SDH_DTR_104M 0 0 0 0>,
<PXA_MMC_TIMING_UHS_DDR50 PXA_SDH_DTR_52M PXA_SDH_DTR_104M 0 0 0 3>,
<PXA_MMC_TIMING_MMC_HS200 PXA_SDH_DTR_208M PXA_SDH_DTR_208M 0 0 0 0>,
<PXA_MMC_TIMING_MAX PXA_SDH_DTR_PS_NONE PXA_SDH_DTR_104M 0 0 0 0>;
#endif
        /delete-property/ vmmc-supply;
        /delete-property/ vmmc2-supply;
        /delete-property/ vqmmc2-supply;

};
```

#### 关键配置项说明

| 配置项 | 说明 |
|--------|------|
| `#if 1` | 条件编译，1 为启用 eMMC，0 为启用 SD NAND |
| `/delete-property/ no-mmc;` | 删除禁用 MMC 的属性（即启用 MMC） |
| `no-sd;` | 禁用 SD 卡（因为此接口用于 eMMC） |
| `MMC_CAP2_ONLY_1_8V` | 仅支持 1.8V 电压 |
| `asr,sdh-dtr-data` | DTR（Data Transfer Rate）时序配置 |
| `/delete-property/ vmmc-supply` | 删除 VMMC 供电属性 |
| `/delete-property/ vmmc2-supply` | 删除 VMMC2 供电属性 |
| `/delete-property/ vqmmc2-supply` | 删除 VQMMC2 供电属性 |

#### DTR 时序配置详解

| 时序模式 | 源速率 | 预设速率 | tx_delay | rx_delay | tx_dline_reg | rx_dline_reg |
|----------|--------|----------|----------|----------|--------------|--------------|
| LEGACY（传统模式） | PXA_SDH_DTR_26M | PXA_SDH_DTR_104M | 0 | 0 | 0 | 0 |
| MMC_HS（高速模式） | PXA_SDH_DTR_52M | PXA_SDH_DTR_104M | 0 | 0 | 0 | 0 |
| UHS_DDR50（DDR50 模式） | PXA_SDH_DTR_52M | PXA_SDH_DTR_104M | 0 | 0 | 0 | 3 |
| MMC_HS200（HS200 模式） | PXA_SDH_DTR_208M | PXA_SDH_DTR_208M | 0 | 0 | 0 | 0 |
| MAX（最大速率） | PXA_SDH_DTR_PS_NONE | PXA_SDH_DTR_104M | 0 | 0 | 0 | 0 |

---

### 3.2 内核配置

#### 3.2.1 eMMC 驱动配置

内核配置文件中**默认已打开 eMMC 配置**，用户无需进行额外操作。

#### 3.2.2 Ext4 文件系统

**步骤 1**：在代码目录下，执行以下命令：

```bash
make kernel_menuconfig
```

**步骤 2**：选择 `File systems`，查看配置信息显示已默认支持 ext4 文件系统。

在 menuconfig 界面中，`File systems` 子菜单下可以看到：

```
[*] The Extended 4 (ext4) filesystem    ← 已选中（built-in）
[*]   Use ext4 for ext2/ext3 file systems (NEW)
[ ]   Ext4 POSIX Access Control Lists (NEW)
[ ]   Ext4 Security Labels (NEW)
[ ]   Ext4 Encryption (NEW)
[ ]   EXT4 debugging support (NEW)
[ ]   JBD2 (ext4) debugging support (NEW)
```

> **备注**：
> 1. 建议 eMMC 用户使用 **Ext4 文件系统**。
> 2. 默认支持 Ext4 文件系统，可以通过查看 `/proc/filesystems` 属性文件确认是否支持 Ext4。

---

## 4 eMMC 文件系统制作和功能测试验证

### 4.1 搭建 LTE OPEN EVB 硬件测试环境

默认情况下，LTE OPEN EVB 支持 eMMC，需要在 EVB 上**切换 eMMC 开关**。

### 4.2 查看 eMMC 设备是否正确识别

在命令行输入如下命令查看 eMMC 设备是否成功识别：

```bash
ls /dev/mmc*
```

**示例输出**：

```
root@OpenWrt:/# ls /dev/mmc*
/dev/mmcblk0    /dev/mmcblk0boot0    /dev/mmcblk0boot1    /dev/mmcblk0rpmb
```

`/dev` 目录下成功识别到 eMMC 存储件 `mmcblk0`。

在命令行输入如下命令查看 eMMC 的块大小：

```bash
cat /proc/partitions
```

**示例输出**：

```
root@OpenWrt:/# cat /proc/partitions | grep mmc
 179    0   3760128  mmcblk0
 179   72       512  mmcblk0rpmb
 179   48     16384  mmcblk0boot1
 179   24     16384  mmcblk0boot0
```

#### 设备节点说明

| 设备节点 | 主设备号 | 次设备号 | 块数（512B/块） | 说明 |
|----------|----------|----------|-----------------|------|
| mmcblk0 | 179 | 0 | 3760128（≈3.6 GiB） | eMMC 主存储区 |
| mmcblk0rpmb | 179 | 72 | 512 | Replay Protected Memory Block（防回放保护区） |
| mmcblk0boot1 | 179 | 48 | 16384 | Boot 分区 1 |
| mmcblk0boot0 | 179 | 24 | 16384 | Boot 分区 0 |

---

### 4.3 分区管理

#### 4.3.1 对 eMMC 进行分区

进入 Linux 系统后，在 shell 终端输入以下命令，对 eMMC 进行分区：

```bash
# fdisk /dev/mmcblk0
```

**示例输出**：

```
root@OpenWrt:/# fdisk /dev/mmcblk0

Welcome to fdisk (util-linux 2.24.1).
Changes will remain in memory only, until you decide to write them.
Be careful before using the write command.

Command (m for help):
```

> **说明**：fdisk 中的所有操作均在内存中进行，直到执行 `w` 命令才会写入磁盘。

#### 4.3.2 查看 eMMC 已有分区信息

输入命令 `p`，查看 eMMC 已有分区信息：

```
Command (m for help): p

Disk /dev/mmcblk0: 3.6 GiB, 3850371072 bytes, 7520256 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x00000000

Device          Boot  Start     End    Blocks  Id  System
/dev/mmcblk0p1         2048  7520255  3759104  83  Linux
```

由上图可知，当前使用的 eMMC 存在一个分区（`/dev/mmcblk0p1`，大小约 3.6 GiB）。

#### 4.3.3 删除已有分区

若 eMMC 当前无分区，可忽略该操作。若 eMMC 已有分区，输入命令 `d`，并依次输入要删除的分区号。由于当前 eMMC 只有一个分区，输入 `d` 默认删除仅有的分区：

```
Command (m for help): d

Selected partition 1
Partition 1 has been deleted.

Command (m for help): p
Disk /dev/mmcblk0: 3.6 GiB, 3850371072 bytes, 7520256 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x00000000
```

删除后，`p` 命令显示无任何分区。

#### 4.3.4 创建新分区

以创建两个分区为例。**第一个分区大小是 50 M，剩余的存储空间分配给第二个分区**。

**创建第一个分区（大小 50 M）**：

输入命令 `n`，接着输入命令 `p`，建立第一个分区（大小 50M），按 ENTER 键执行当前默认配置：

```
Command (m for help): n

Partition type:
   p   primary (0 primary, 0 extended, 4 free)
   e   extended
Select (default p): p
Partition number (1-4, default 1):
First sector (2048-7520255, default 2048):
Last sector, +sectors or +size{K,M,G,T,P} (2048-7520255, default 7520255):

Created a new partition 1 of type 'Linux' and of size 3.6 GiB.

Command (m for help):
```

> **注意**：若要创建 50 M 大小的第一个分区，在 `Last sector` 时应输入 `+50M` 而非直接回车（直接回车使用默认值会占用全部空间）。

**创建第二个分区（剩余空间）**：

输入命令 `n`，建立第二个分区（大小为 eMMC 剩余空间大小），按 ENTER 键执行当前默认配置。输入 `p`，查看分区信息。

#### 4.3.5 写分区

输入命令 `w`，将会写入并保存到 eMMC 分区信息：

```
Command (m for help): w
```

写入后，`p` 命令确认分区信息：

```
Disk /dev/mmcblk0: 3.6 GiB, 3850371072 bytes, 7520256 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x00000000

Device          Boot  Start     End    Blocks  Id  System
/dev/mmcblk0p1         2048  7520255  3759104  83  Linux
```

---

### 4.4 导入 Ext4 文件系统制作工具

默认情况下，Ext4 工具（如：`mke2fs`、`mkfs.ext4` 等）**已经集成在固件中**。若未集成，请联系移远通信技术支持。

---

### 4.5 格式化分区为 Ext4 文件系统

此处以分区 1 为例，演示使用制作的 mke2fs 工具将分区 1 格式化为 Ext4 文件系统：

```bash
# mke2fs -t ext4 /dev/mmcblk0p1
```

**示例输出**：

```
root@OpenWrt:/# mke2fs -t ext4 /dev/mmcblk0p1
mke2fs 1.42.4 (12-June-2012)
Discarding device blocks: done
Filesystem label=
OS type: Linux
Block size=4096 (log=2)
Fragment size=4096 (log=2)
Stride=0 blocks, Stripe width=0 blocks
235248 inodes, 939776 blocks
46988 blocks (5.00%) reserved for the super user
First data block=0
Maximum filesystem blocks=964689920
29 block groups
32768 blocks per group, 32768 fragments per group
8112 inodes per group
Superblock backups stored on blocks:
        32768, 98304, 163840, 229376, 294912, 819200, 884736

Allocating group tables: done
Writing inode tables: done
Creating journal (16384 blocks): done
Writing superblocks and filesystem accounting information: done

root@OpenWrt:/#
```

#### 格式化结果说明

| 参数 | 值 | 说明 |
|------|----|------|
| 工具版本 | mke2fs 1.42.4 (12-June-2012) | ext2/3/4 文件系统创建工具 |
| 块大小 | 4096 bytes (log=2) | 每个文件系统块 4KB |
| Fragment 大小 | 4096 bytes (log=2) | 与块大小相同 |
| inode 数量 | 235248 | 文件系统中最大文件/目录数 |
| 总块数 | 939776 | 文件系统总块数 |
| 超级用户保留块 | 46988 (5.00%) | 为系统管理保留的块 |
| 块组数 | 29 | 文件系统块组总数 |
| 每组块数 | 32768 | 每个块组的块数 |
| 每组 inode 数 | 8112 | 每个块组的 inode 数 |
| Journal 大小 | 16384 blocks | 日志大小 |
| 超级块备份位置 | 32768, 98304, 163840, 229376, 294912, 819200, 884736 | 备份超级块所在块号 |

---

### 4.6 读写测试

**步骤 1**：在 `/mnt/` 目录下创建文件夹 `sdcard`。将 eMMC 分区 1 挂载到 `/mnt/mmcblk0p1` 目录下：

```bash
root@OpenWrt:/# mount -t ext4 /dev/mmcblk0p1 /mnt/mmcblk0p1/
[14465.565338] EXT4-fs (mmcblk0p1): mounted filesystem with ordered data mode. Opts: (null)
```

**步骤 2**：使用 `df -h` 查看 eMMC 分区 1 是否挂载成功，以及分区格式是否正确：

```bash
root@OpenWrt:/# df -h | grep mmc
/dev/mmcblk0p1    3.5G    7.2M    3.3G    0%    /mnt/mmcblk0p1
```

**步骤 3**：执行如下命令，生成大小为 3G 的文件并存储至 `/mnt/mmcblk0p1`（即 eMMC 分区 1）：

```bash
dd if=/dev/zero of=/mnt/mmcblk0p1/test1.img bs=1M count=3072
```

**示例输出**：

```
root@OpenWrt:/# dd if=/dev/zero of=/mnt/mmcblk0p1/test1.img bs=1M count=3072
3072+0 records in
3072+0 records out
root@OpenWrt:#
root@OpenWrt:/# df -h
Filesystem      Size    Used  Available  Use%  Mounted on
rootfs         128.8M  112.0K  124.0M    0%    /
/dev/root        7.5M    7.5M      0    100%   /rom
tmpfs           47.9M   80.0K   47.8M    0%    /tmp
ubi0_0         128.8M  112.0K  124.0M    0%    /overlay
ubil:oem_data    3.1M   20.0K    3.1M    1%    /NVM/oem_data
overlayfs:/overlay/root           128.8M  112.0K  124.0M    0%    /
tmpfs           512.0K      0   512.0K    0%    /dev
/dev/mmcblk0p1   3.5G    3.0G   270.5M   92%   /mnt/mmcblk0p1
```

**判断标准**：查看新生成的文件大小，如果大小正确，可以判定 eMMC 读写成功。从 `df -h` 输出可知，`/dev/mmcblk0p1` 已写入 3.0G 数据，空间使用率 92%，说明 eMMC 读写功能正常。

---

## 5 eMMC 常用调试方法

### 5.1 简单读写测试

eMMC 设备被识别后，会在 `/dev` 目录下生成名为 `mmcblk0` 的设备。可通过执行 `echo`、`hexdump` 等命令对 eMMC 设备进行简单的读写测试。

#### 5.1.1 写入数据

执行如下命令向识别到的 eMMC 设备（`/dev/mmcblk0`）写入数据：

```bash
echo HelloWorld! > /dev/mmcblk0
```

**示例**：

```
root@OpenWrt:/# ls /dev/mmc*
/dev/mmcblk0    /dev/mmcblk0boot0    /dev/mmcblk0boot1    /dev/mmcblk0rpmb
root@OpenWrt:/#
root@OpenWrt:/# echo HelloWorld! > /dev/mmcblk0
root@OpenWrt:/#
```

#### 5.1.2 读出数据

执行如下命令从识别到的 eMMC 设备（`/dev/mmcblk0`）读出写入的数据：

```bash
hexdump -C -n 15 /dev/mmcblk0
```

**示例输出**：

```
root@OpenWrt:/#
root@OpenWrt:/# hexdump -C -n 15 /dev/mmcblk0
00000000  48 65 6c 6c 6f 57 6f 72  6c 64 21 0a 00 00 00  |HelloWorld!....|
0000000f
root@OpenWrt:/# _
```

**分析**：

| 偏移地址 | 十六进制数据 | ASCII 解释 |
|----------|--------------|------------|
| 00000000 | 48 65 6c 6c 6f 57 6f 72 6c 64 21 0a 00 00 00 | `HelloWorld!\n...` |

成功从 eMMC 读取之前写入的 "HelloWorld!" 字符串，验证读写功能正常。

---

### 5.2 查看当前系统支持的文件系统

执行如下命令查看当前 Linux 系统支持的文件系统类型：

```bash
cat /proc/filesystems
```

**示例输出**：

```
root@OpenWrt:/#
root@OpenWrt:/# cat /proc/filesystems
nodev    sysfs
nodev    rootfs
nodev    bdev
nodev    proc
nodev    tmpfs
nodev    debugfs
nodev    sockfs
nodev    pipefs
nodev    anon_inodefs
nodev    configfs
nodev    devpts
         ext3
         ext2
         ext4
         squashfs
nodev    ramfs
         vfat
         fuseblk
nodev    fuse
nodev    fusectl
nodev    overlayfs
nodev    mqueue
nodev    mtd_inodefs
nodev    ubifs
root@OpenWrt:/#
```

当前系统支持 `vfat`、`Ext2`、`Ext3`、`Ext4` 等文件系统。

> **说明**：`nodev` 表示该文件系统不需要关联块设备（无设备文件系统）；没有 `nodev` 标记的表示基于块设备的文件系统。

---

### 5.3 获取 eMMC 基本信息

识别 eMMC 时，eMMC 驱动代码已将一些常用信息通过文件的形式创建在 `sys` 目录下。用户可以通过 `cat` 等命令查看设备信息。常用命令如下：

```bash
# 列出 mmcblk0 的详细信息（符号链接等）
ls /sys/block/mmcblk0 -l

# 查看 eMMC 容量（单位：512 字节扇区数）
cat /sys/block/mmcblk0/size

# 查看当前挂载信息
cat /proc/self/mounts

# 查看 eMMC 设备详细信息（逐一查询）
cat /sys/block/mmcblk0/device/cid
cat /sys/block/mmcblk0/device/csd
cat /sys/block/mmcblk0/device/date
cat /sys/block/mmcblk0/device/fwrev
cat /sys/block/mmcblk0/device/hwrev
cat /sys/block/mmcblk0/device/manfid
cat /sys/block/mmcblk0/device/name
cat /sys/block/mmcblk0/device/oemid
cat /sys/block/mmcblk0/device/serial
cat /sys/block/mmcblk0/device/type
cat /sys/block/mmcblk0/device/uevent
```

#### sys 文件节点说明

| 文件节点 | 说明 |
|----------|------|
| `/sys/block/mmcblk0/size` | eMMC 总容量（扇区数，每扇区 512 字节） |
| `/sys/block/mmcblk0/device/cid` | Card Identification Register，卡身份识别寄存器 |
| `/sys/block/mmcblk0/device/csd` | Card Specific Data Register，卡特定数据寄存器 |
| `/sys/block/mmcblk0/device/date` | 制造日期 |
| `/sys/block/mmcblk0/device/fwrev` | 固件版本号 |
| `/sys/block/mmcblk0/device/hwrev` | 硬件版本号 |
| `/sys/block/mmcblk0/device/manfid` | 制造商 ID |
| `/sys/block/mmcblk0/device/name` | 产品名称 |
| `/sys/block/mmcblk0/device/oemid` | OEM ID |
| `/sys/block/mmcblk0/device/serial` | 序列号 |
| `/sys/block/mmcblk0/device/type` | 设备类型（MMC/SD等） |
| `/sys/block/mmcblk0/device/uevent` | 设备事件信息 |

---

### 5.4 Linux MMC 驱动子系统简述

#### 5.4.1 Linux MMC 子系统介绍

MMC、SD、SDIO 三种技术均起源于 MMC 技术，有很多共性，因此 **Linux Kernel 统一使用 MMC framework 管理所有和这三种技术有关的设备**。

Linux 的 MMC 子系统在内核 `drivers/mmc` 目录中实现，该目录下有 3 个子目录：

| 子目录 | 作用 |
|--------|------|
| `card` | 存放 MMC 卡的块设备相关代码 |
| `core` | 核心层代码 |
| `host` | 主控制器的相关代码 |

这三个子目录反映了该子系统的层次架构。

#### MMC 子系统三层架构

MMC 子系统从上到下可分为 **3 个层次**：**Host 层**、**Core 层**和 **Card 层**。

---

**Host 层（主控制器驱动层）**

- 也叫主控制器驱动层
- 主控制器通常是 SOC 上的一个外设，通过它，SOC 能够按照 MMC 协议规定的方式与卡进行数据传输
- 在 `drivers/mmc/host` 子目录下有很多主控制器驱动，其中包括模块的 SDIO 驱动 **`sdhci-msm.c`**

---

**Core 层（MMC 子系统核心层）**

- 即 MMC 子系统核心层，实现代码位于 `drivers/mmc/core` 子目录中
- 核心层（主控制器驱动程序）除为 Host 层驱动提供诸如：
  - `mmc_alloc_host()` — 分配 MMC 主控制器对象
  - `mmc_add_host()` — 添加 MMC 主控制器对象
  - `mmc_remove_host()` — 删除 MMC 主控制器对象
  等接口外，还分别就 MMC 和 SDIO 两种规范实现了相应的协议代码

---

**Card 层（块设备驱动层）**

- 因为 MMC 卡属于块设备，所以需要为这些设备提供块设备驱动
- 在 `drivers/mmc/core` 中，`block.c` 主要实现了 MMC 卡的通用块设备驱动
- 在 `queue.c` 中则实现了该块设备的请求队列及处理
- **MMC 卡块设备的主设备号是 179**

#### Linux MMC 子系统架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        Kernel Space                                     │
│                                                                         │
│              ┌──────────────────────────────────┐                      │
│              │    Virtual File System Layer       │                      │
│              └──────────────┬───────────────────┘                      │
│                             │                                           │
│              ┌──────────────▼───────────────────┐                      │
│              │      Generic Block Layer           │                      │
│              └──────────────┬───────────────────┘                      │
│                             │                                           │
│  mmc        ┌───────────────▼──────────────────────────────────────┐   │
│  device  ──►│         mmc Block/Queue driver                        │◄── mmc/card/block.c
│  driver     │                                                        │   mmc/card/queue.c
│             ├────────────────────────────────────────────────────────┤   │
│             │              mmc core driver                           │◄── mmc/core/core.c
│             │                                                        │   mmc/core/mmc.c
│             │                                                        │   mmc/core/sd.c
│             ├────────────────────────────────────────────────────────┤   │
│             │             mmc host driver                            │   │
│             ├────────────────────────────────────────────────────────┤   │
│             │            SDHCI Host driver                           │◄── mmc/host/sdhci.c
│             └─────────────────────┬──────────────────────────────────┘   │
└───────────────────────────────────┼─────────────────────────────────────┘
                                    │
                    ┌───────────────┴────────────────┐
                    │         Hardware Devices        │
                    │   eMMC device  │  SD memory card│
                    └───────────────────────────────-─┘
```

（对应文档图 1：Linux MMC 子系统架构图）

---

#### 5.4.2 Linux MMC 子系统驱动初始化流程

通常，大多数问题出现在驱动初始化过程中。整理 MMC 驱动框架的初始化过程如下：

```
SD HCI Driver              MMC Core                    MMC Card
─────────────           ─────────────               ─────────────
sdhci_probe
     │
sdhci_alloc_host ──────► mmc_alloc_host
                              │
                         INIT_DELAYED_WORK
                          (mmc_rescan)
     │
sdhci_add_host ────────► mmc_add_host
                              │
                         mmc_start_host
                              │
                         mmc_schedule_delayed_work
                              │
                         mmc_rescan
                              │
                         mmc_rescan_try_freq
                              │
                         mmc_attach_mmc
                              │
                         mmc_init_card
                              │
                         mmc_add_card ──────────────► mmc_blk_probe
                                                           │
                                                      mmc_blk_alloc
                                                           │
                                                      mmc_add_disk
```

（对应文档图 2：Linux MMC 驱动初始化流程图）

#### 初始化流程各函数说明

| 函数名 | 所属层 | 说明 |
|--------|--------|------|
| `sdhci_probe` | SD HCI Driver | SDHCI 驱动探测入口 |
| `sdhci_alloc_host` | SD HCI Driver | 分配 Host 结构体 |
| `mmc_alloc_host` | MMC Core | 分配 MMC Host 对象 |
| `INIT_DELAYED_WORK(mmc_rescan)` | MMC Core | 初始化延迟工作队列（用于设备扫描） |
| `sdhci_add_host` | SD HCI Driver | 注册 Host 到 MMC Core |
| `mmc_add_host` | MMC Core | 添加 Host 到系统 |
| `mmc_start_host` | MMC Core | 启动 Host |
| `mmc_schedule_delayed_work` | MMC Core | 调度延迟工作（触发 mmc_rescan） |
| `mmc_rescan` | MMC Core | 扫描总线上的卡 |
| `mmc_rescan_try_freq` | MMC Core | 尝试不同频率扫描 |
| `mmc_attach_mmc` | MMC Core | 绑定 MMC 设备 |
| `mmc_init_card` | MMC Core | 初始化 MMC 卡 |
| `mmc_add_card` | MMC Core | 将卡添加到系统 |
| `mmc_blk_probe` | MMC Card | 块设备驱动探测 |
| `mmc_blk_alloc` | MMC Card | 分配块设备结构 |
| `mmc_add_disk` | MMC Card | 注册块设备到内核 |

---

### 5.5 打开 Debug 信息

#### 5.5.1 打开 MMC 子系统调试信息

在 `drivers/mmc/core/core.c` 文件中添加如下宏定义，可以将 MMC 子系统的调试信息（`pr_debug`）重定向到内核错误输出（`pr_err`）：

```c
#undef pr_debug
#define pr_debug pr_err
```

添加位置示例（在 core.c 头部 include 之后）：

```c
#include "mmc_ops.h"
#include "sd_ops.h"
#include "sdio_ops.h"

#undef pr_debug          /* ← 添加这两行 */
#define pr_debug pr_err  /* ← 启用调试输出 */

/* The max erase timeout, used when host->max_busy_timeout isn't specified */
#define MMC_ERASE_TIMEOUT_MS    (60 * 1000) /* 60 s */
#define SD_DISCARD_TIMEOUT_MS   (250)
```

> **原理**：Linux 内核中 `pr_debug` 在非调试构建时默认不输出。将其重定义为 `pr_err` 后，所有原本是 `pr_debug` 的调试信息都会以错误级别输出到内核日志，可通过 `dmesg` 查看。

---

## 6 Mdev 自动挂载

### 6.1 mdev 简介

**mdev** 是 busybox 提供的一个工具，用在嵌入式系统中，相当于简化版的 **udev**，用于在系统启动和热插拔或动态加载驱动程序时，**自动创建设备节点**。文件系统中 `/dev` 目录下的设备节点都是由 mdev 创建的。

| 特性 | 说明 |
|------|------|
| 来源 | busybox 内置工具 |
| 对标工具 | udev（简化版） |
| 适用场景 | 嵌入式 Linux 系统 |
| 主要功能 | 系统启动时创建 /dev 节点；热插拔时动态创建/删除节点 |
| 配置文件 | `/etc/mdev.conf` |

---

### 6.2 eMMC 自动挂载机制

eMMC 自动挂载机制采用的是 mdev 的机制。

**mdev 配置文件路径**：`ql-ol-rootfs/etc/mdev.conf`

相关 eMMC 配置如下所示；**当识别到了 eMMC 设备并且存在 eMMC 分区，则会运行 `/etc/mdev/automountsdcard.sh` 脚本**：

```
tun[0-9]*   0:0   0660

mmcblk[0-9]*          0:6   660
mmcblk[0-9]*p[0-9]*   0:6   660   */etc/mdev/automountsdcard.sh ${MDEV}
[hs]d[a-z][0-9]*      0:0   660   */etc/mdev/automountsdcard.sh ${MDEV}
```

#### mdev.conf 配置语法说明

| 配置行 | 格式 | 说明 |
|--------|------|------|
| `mmcblk[0-9]*` | 设备名匹配 用户:组 权限 | 匹配 eMMC 主设备（无分区），设置所有者和权限 |
| `mmcblk[0-9]*p[0-9]*` | 设备名匹配 用户:组 权限 脚本 | 匹配 eMMC 分区设备，检测到时执行自动挂载脚本 |
| `[hs]d[a-z][0-9]*` | 设备名匹配 用户:组 权限 脚本 | 匹配 SATA/USB 存储设备分区，同样执行自动挂载脚本 |
| `${MDEV}` | 环境变量 | 由 mdev 传入，表示当前检测到的设备名 |
| `*` | 命令前缀 | 表示无论设备插入还是拔出都执行该脚本 |

**使用 `df -Th` 命令可以查看到分区 `/dev/mmcblk0p1` 和 `/dev/mmcblk0p2` 挂载的目录分别为 `/mnt/mmcblk0p1` 和 `/mnt/mmcblk0p2`**：

```
root@OpenWrt:/# df -Th
Filesystem           Type    Size    Used  Available  Use%  Mounted on
/dev/cm-0            squashfs  13.3M   13.3M         0  100%  /
tmpfs                tmpfs   45.5M  116.0K    45.4M    0%  /tmp
ubi0:data            ubifs   90.7M   28.0K    86.0M    0%  /data
ubi0:data            ubifs   90.7M   28.0K    86.0M    0%  /log
ubi0:etc             ubifs    2.9M    1.9M   856.0K   69%  /overlay/etc
ubi0:nvm             ubifs    2.0M   56.0K     1.8M    3%  /overlay/nvm
overlayfs:/overlay/etc  overlay   2.9M    1.9M   856.0K   69%  /etc
overlayfs:/overlay/nvm  overlay   2.0M   56.0K     1.8M    3%  /NVM
/dev/ubiblock1_0     squashfs  128.0K  128.0K         0  100%  /NVM/oem_data
tmpfs                tmpfs  512.0K        0   512.0K    0%  /dev
/dev/mmcblk0p2       ext4     3.4G   32.0K     3.2G    0%  /mnt/mmcblk0p2   ← eMMC 分区2
/dev/mmcblk0p1       ext4    43.6M   16.0K    40.1M    0%  /mnt/mmcblk0p1   ← eMMC 分区1
root@OpenWrt:/#
```

#### 禁用 eMMC 自动挂载

若不需要使用 mdev 机制挂载 eMMC，可删除 mdev 自动挂载 eMMC 机制，**注释** `ql-ol-rootfs/etc/mdev.conf` 配置文件的 eMMC 自动挂载配置：

```
tun[0-9]*   0:0   0660

mmcblk[0-9]*          0:6   660
#mmcblk[0-9]*p[0-9]*  0:6   660   */etc/mdev/automountsdcard.sh ${MDEV}    ← 注释此行
[hs]d[a-z][0-9]*      0:0   660   */etc/mdev/automountsdcard.sh ${MDEV}
```

#### 启用/修改 eMMC 自动挂载行为

若需要使用 mdev 机制自动挂载 eMMC，则可直接修改 `/etc/mdev/automountsdcard.sh` 自动挂载脚本。

---

## 7 附录：参考文档及术语缩写

### 表 2：参考文档

| 编号 | 文档名称 |
|------|----------|
| [1] | Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |
| [2] | Quectel_EC200A-CN(TA)_QuecOpen_硬件设计手册 |

### 表 3：术语缩写

| 缩写 | 英文全称 | 中文全称 |
|------|----------|----------|
| eMMC | Embedded Multi Media Card | 嵌入式多媒体控制器 |
| Ext4 | Fourth extended filesystem | 第四代扩展文件系统 |
| FAT | File Allocation Table | 文件配置表 |
| IoT | Internet of Things | 物联网 |
| SD Card | Secure Digital Memory Card | 安全数码卡 |
| SOC | System On Chip | 片上系统 |

---

## 补充整理：关键操作速查表

### eMMC 开发完整操作流程

```
1. 硬件连接
   └─ EVB 板切换 eMMC 开关

2. 验证设备识别
   ├─ ls /dev/mmc*              → 确认 /dev/mmcblk0 存在
   └─ cat /proc/partitions      → 查看分区和容量

3. 分区操作（fdisk /dev/mmcblk0）
   ├─ p  → 查看现有分区
   ├─ d  → 删除分区（按分区号）
   ├─ n  → 新建分区（p=主分区, e=扩展分区）
   └─ w  → 写入分区表

4. 格式化
   └─ mke2fs -t ext4 /dev/mmcblk0p1

5. 挂载
   └─ mount -t ext4 /dev/mmcblk0p1 /mnt/mmcblk0p1/

6. 读写验证
   ├─ dd if=/dev/zero of=/mnt/mmcblk0p1/test.img bs=1M count=100
   └─ df -h  → 验证使用量

7. 卸载（可选）
   └─ umount /mnt/mmcblk0p1
```

### 快速诊断命令汇总

| 目的 | 命令 |
|------|------|
| 检查 eMMC 是否识别 | `ls /dev/mmc*` |
| 查看分区和块大小 | `cat /proc/partitions` |
| 查看当前挂载 | `df -Th` 或 `cat /proc/self/mounts` |
| 查看支持的文件系统 | `cat /proc/filesystems` |
| 获取 eMMC 容量 | `cat /sys/block/mmcblk0/size` |
| 获取 eMMC 制造商信息 | `cat /sys/block/mmcblk0/device/manfid` |
| 获取 eMMC 产品名 | `cat /sys/block/mmcblk0/device/name` |
| 获取 eMMC 序列号 | `cat /sys/block/mmcblk0/device/serial` |
| 简单写测试 | `echo HelloWorld! > /dev/mmcblk0` |
| 简单读验证 | `hexdump -C -n 15 /dev/mmcblk0` |
| 进入分区工具 | `fdisk /dev/mmcblk0` |
| 格式化为 Ext4 | `mke2fs -t ext4 /dev/mmcblk0p1` |
| 挂载 Ext4 分区 | `mount -t ext4 /dev/mmcblk0p1 /mnt/mmcblk0p1/` |

### 关键路径汇总

| 路径 | 说明 |
|------|------|
| `kernel/arch/arm/boot/dts/ql-asr1803-overlay.dtsi` | eMMC 设备树配置文件 |
| `drivers/mmc/` | Linux MMC 子系统源码根目录 |
| `drivers/mmc/card/block.c` | MMC 卡块设备驱动 |
| `drivers/mmc/card/queue.c` | MMC 块设备请求队列 |
| `drivers/mmc/core/core.c` | MMC Core 核心代码（调试宏修改此处） |
| `drivers/mmc/core/mmc.c` | MMC 协议实现 |
| `drivers/mmc/core/sd.c` | SD 协议实现 |
| `drivers/mmc/host/sdhci-msm.c` | 模块 SDIO 主控制器驱动 |
| `/dev/mmcblk0` | eMMC 主存储设备节点 |
| `/dev/mmcblk0p1` | eMMC 第一分区设备节点 |
| `/sys/block/mmcblk0/device/` | eMMC 设备信息 sysfs 目录 |
| `/proc/partitions` | 系统分区信息 |
| `/proc/filesystems` | 系统支持的文件系统列表 |
| `ql-ol-rootfs/etc/mdev.conf` | mdev 设备管理配置文件 |
| `/etc/mdev/automountsdcard.sh` | eMMC 自动挂载脚本 |

---

*文档整理完毕。版权所有 © 上海移远通信技术股份有限公司 2022，保留一切权利。*  
*Copyright © Quectel Wireless Solutions Co., Ltd. 2022.*
