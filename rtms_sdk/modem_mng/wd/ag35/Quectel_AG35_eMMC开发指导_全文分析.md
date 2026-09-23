# AG35-CET QuecOpen(SDK) eMMC 开发指导

> **文档版本**：V1.0.0  
> **日期**：2023-11-20  
> **状态**：临时文件  
> **适用平台**：AG35-CET（ASR1806e，LTE Standard 模块系列）  
> ⚠️ **平台说明**：本文档面向 AG35-CET/EUT，本项目实际运行于 EC200A/EG25，硬件接口细节（引脚号、DTS 路径）请以实际平台手册为准，功能原理和调试方法具有参考价值。

---

## 目录

1. [引言](#1-引言)
2. [eMMC 硬件接口及引脚定义](#2-emmc-硬件接口及引脚定义)
3. [eMMC 设备树配置及内核配置](#3-emmc-设备树配置及内核配置)
4. [eMMC 文件系统制作和功能测试验证](#4-emmc-文件系统制作和功能测试验证)
5. [eMMC 常用调试方法](#5-emmc-常用调试方法)
6. [Mdev 自动挂载](#6-mdev-自动挂载)
7. [附录 参考文档及术语缩写](#7-附录-参考文档及术语缩写)

---

## 1 引言

移远通信 AG35-CET 模块支持 QuecOpen® 方案。QuecOpen® 是基于 Linux 的嵌入式开发平台，可简化 IoV 应用的软件设计和开发过程。

**SD 卡**即安全数据存储卡，广泛应用于便携式设备上，例如数码相机、个人数码助理和多媒体播放器等。

**eMMC** 为嵌入式多媒体存储卡。MMC 表示多媒体存储卡，是一种闪存卡（Flash Memory Card）标准，它定义了 MMC 的架构以及访问 Flash Memory 的接口和协议。而 eMMC 则是对 MMC 的一个拓展，以满足更高标准的性能、成本、体积、稳定、易用等需求。

Linux Kernel 使用 MMC 子系统统一管理 eMMC、SD 卡、SDIO Wi-Fi 设备：
- **eMMC** 强调的是多媒体存储
- **SD** 强调的是安全和数据保护
- **SDIO** 是从 SD 演化出来的安全数字输入输出接口

为了满足用户多元化的需求，移远通信 AG35-CET QuecOpen 模块预留一路 SDIO 接口用于连接 eMMC。该 SDIO 接口符合 SD 3.0 协议。

本文档适用于 SDK 构建环境的 QuecOpen® 方案，主要介绍在 QuecOpen® 方案下，移远通信 AG35-CET 模块 eMMC 开发流程，包括接口引脚定义、设备树设置、分区管理以及调试方法，以帮助用户简易且快速地进行开发。

---

## 2 eMMC 硬件接口及引脚定义

AG35-CET 模块提供了一路支持 SD 3.0 协议的 SDIO 接口，可用于外接 eMMC 设备。

### 表 1：SDIO 接口引脚定义

| 引脚名 | 引脚号 | I/O | 描述 |
|--------|--------|-----|------|
| SDIO2_VDD | 46 | PO | SDIO 电源 |
| SDIO2_DATA0 | 49 | DIO | SDIO 数据位 0 |
| SDIO2_DATA1 | 50 | DIO | SDIO 数据位 1 |
| SDIO2_DATA2 | 47 | DIO | SDIO 数据位 2 |
| SDIO2_DATA3 | 48 | DIO | SDIO 数据位 3 |
| SDIO2_CMD | 51 | DIO | SDIO 命令 |
| SDIO2_CLK | 53 | DO | SDIO 时钟 |
| EMMC_RST_N | 52 | DI | eMMC 复位 |

> 有关详细信息，请参考 Quectel_AG35-CET_QuecOpen_硬件设计手册。

---

## 3 eMMC 设备树配置及内核配置

### 3.1 设备树文件配置

修改设备树文件 `ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts` 的设备树节点 `sdh0` 的配置信息。

```c
/* For 1 to support emmc(default), for 0 to support SD card */
#define CONFIG_USE_EMMC                 1
#define CONFIG_USE_EMMC_SD_ADAPTIVE     0

#if (CONFIG_USE_EMMC)
&sdh0 {
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

        /* Enable the emmc and SD adaptive functions */
        #if (CONFIG_USE_EMMC_SD_ADAPTIVE)
        /delete-property/ no-sd;
        quec-emmc-sd-gpio =   <&gpio 43 0>;
        /*GPIO43 is used as the eth power enable pin by default. Customers need to select a proper pin*/
        asr,sdh-dtr-data-sd=
                <PXA_MMC_TIMING_LEGACY PXA_SDH_DTR_26M PXA_SDH_DTR_104M 0 0 0 0>,
                <PXA_MMC_TIMING_SD_HS PXA_SDH_DTR_52M PXA_SDH_DTR_104M 0 0 0 0>,
                <PXA_MMC_TIMING_UHS_DDR50 PXA_SDH_DTR_52M PXA_SDH_DTR_104M 0 0 0 3>,
                <PXA_MMC_TIMING_UHS_SDR50 PXA_SDH_DTR_104M PXA_SDH_DTR_208M 0 0 0 0>,
                <PXA_MMC_TIMING_UHS_SDR104 PXA_SDH_DTR_208M PXA_SDH_DTR_208M 0 0 0>,
                <PXA_MMC_TIMING_MAX PXA_SDH_DTR_PS_NONE PXA_SDH_DTR_104M 0 0 0 0>;
        #endif
};
#endif
```

**重要宏定义说明：**

- `CONFIG_USE_EMMC`：用于配置要识别的设备
  - `1`：识别 eMMC 设备
  - `0`：识别 SD 卡

- `CONFIG_USE_EMMC_SD_ADAPTIVE`：用于配置通过 GPIO 识别安装的存储设备
  - `1`：使能通过 GPIO 识别安装的存储设备
  - `0`：关闭通过 GPIO 识别安装的存储设备

若客户设备同时安装了 SD 卡和 eMMC 设备，需要通过 GPIO 识别相应的设备，则需先定义 `CONFIG_USE_EMMC` 宏为 1，然后定义宏 `CONFIG_USE_EMMC_SD_ADAPTIVE` 为 1 以使能模块通过 GPIO 识别存储设备。用于识别的引脚客户可自定义。

### 3.2 内核配置

#### 3.2.1 eMMC 驱动配置

内核配置文件中默认已打开 eMMC 配置，用户无需进行额外操作。

#### 3.2.2 ext4 文件系统

1. 在代码目录下，执行 `make kernel_menuconfig`：

   ```bash
   make kernel_menuconfig
   ```

2. 选择 "File systems"，查看配置信息显示已默认支持 ext4 文件系统：

   在 menuconfig 界面中选择：
   ```
   File systems
     --> The Extended 4 (ext4) filesystem  <*>  （已选中，内建）
   ```

   同时可看到以下文件系统均已支持：
   - ext2 / ext3 / ext4（含 journalling）
   - squashfs
   - UBIFS
   - overlayfs
   - YAFFS2
   - vfat / FAT

**备注：**
1. 建议 eMMC 用户使用 Ext4 文件系统。
2. 模块当前默认支持 Ext4 文件系统，可以通过查看 `/proc/filesystems` 属性文件确认是否支持 ext4。

---

## 4 eMMC 文件系统制作和功能测试验证

### 4.1 LTE OPEN EVB 硬件测试环境

默认情况下，LTE OPEN EVB 支持 eMMC，需要在 EVB 上切换 eMMC 开关。

### 4.2 eMMC 设备识别

在命令行输入如下命令查看 eMMC 设备是否成功识别：

```bash
ls /dev/mmc*
```

示例输出：

```
/dev/mmcblk0    /dev/mmcblk0boot0    /dev/mmcblk0boot1    /dev/mmcblk0rpmb
```

如上图所示，`/dev/` 目录下成功识别到 eMMC 存储器件 `mmcblk0`。

### 4.3 eMMC 设备块大小

在命令行输入如下命令查看 eMMC 的块大小：

```bash
cat /proc/partitions
```

示例输出（过滤 mmc）：

```
179   0   3760128 mmcblk0
179  72      512  mmcblk0rpmb
179  24    16384  mmcblk0boot1
179  24    16384  mmcblk0boot0
```

- `mmcblk0`：eMMC 主存储，约 3.6 GiB（3760128 × 1 KiB = 3.59 GiB）
- `mmcblk0rpmb`：重放保护内存块（Replay Protected Memory Block），512 KiB
- `mmcblk0boot0/1`：两个引导分区，各 16 MiB

### 4.4 分区管理

进入 Linux 系统后，在 shell 终端输入以下命令，对 eMMC 进行分区：

```bash
fdisk /dev/mmcblk0
```

示例：

```
Welcome to fdisk (util-linux 2.24.1).
Changes will remain in memory only, until you decide to write them.
Be careful before using the write command.

Command (m for help):
```

#### 4.4.1 查看 eMMC 已有分区

输入命令 `p`，查看 eMMC 已有分区信息。示例如下：

```
Command (m for help): p

Disk /dev/mmcblk0: 3.6 GiB, 3850371072 bytes, 7520256 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x00000000

Device          Boot Start      End  Blocks  Id System
/dev/mmcblk0p1       2048  7520255 3759104  83 Linux
```

由上图可知，当前使用的 eMMC 存在一个分区 `mmcblk0p1`。

#### 4.4.2 删除分区

进行删除操作前，需保证 eMMC 设备已存在分区。若 eMMC 已有分区，输入命令 `d`，并依次输入要删除的分区号。若 eMMC 设备只有一个分区，输入 `d` 默认删除仅有的分区。示例如下：

```
Command (m for help): d

Selected partition 1
Partition 1 has been deleted.

Command (m for help): p
Disk /dev/mmcblk0: 3.6 GiB, 3850371072 bytes, 7520256 sectors
...（无分区记录）
```

#### 4.4.3 创建分区

以创建两个分区为例。第一个分区大小是 50 MB，剩余的存储空间分配给第二个分区。

输入命令 `n`，接着输入命令 `p`，建立第一个分区（大小 50 MB），按 ENTER 键执行当前默认配置：

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
```

> 创建第二个分区：再次输入 `n` → `p` → `2`，按 ENTER 使用剩余空间默认配置。

#### 4.4.4 写入新分区信息

输入命令 `w`，把新建分区信息写入并保存至 eMMC 设备：

```
Command (m for help): p
Disk /dev/mmcblk0: 3.6 GiB, 3850371072 bytes, 7520256 sectors
Units: sectors of 1 * 512 = 512 bytes
...
Device          Boot  Start      End  Blocks  Id System
/dev/mmcblk0p1        2048  7520255 3759104  83 Linux
```

输入 `w` 后分区表写入磁盘。

#### 4.4.5 格式化分区为 ext4 文件系统

以分区 1（`mmcblk0p1`）为例，使用 `mke2fs` 工具将分区 1 格式化为 ext4 文件系统：

```bash
mke2fs -t ext4 /dev/mmcblk0p1
```

示例输出：

```
mke2fs 1.42.4 (12-June-2012)
Discarding device blocks: done
Filesystem label=
OS type: Linux
Block size=4096 (log=2)
Fragment size=4096 (log=2)
Stride=0 blocks, stripe width=0 blocks
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
```

**备注：** ext4 文件系统制作工具 `mke2fs` 默认已集成于模块固件中。若未集成，请联系移远通信技术支持。

### 4.5 分区读写测试

将 eMMC 分区 1 挂载到 `/mnt/mmcblk0p1/` 目录下：

```bash
mount -t ext4 /dev/mmcblk0p1 /mnt/mmcblk0p1/
```

输出示例：

```
[14465.563338] EXT4-fs (mmcblk0p1): mounted filesystem with ordered data mode. Opts: (null)
```

使用 `df -h | grep mmc` 查看 eMMC 分区 1 是否挂载成功，以及分区格式是否正确：

```
/dev/mmcblk0p1    3.5G    7.2M    3.3G    0%    /mnt/mmcblk0p1
```

执行如下命令，生成大小为 3 GB 的文件并存储至 `/mnt/mmcblk0p1/`，即 eMMC 分区 1：

```bash
dd if=/dev/zero of=/mnt/mmcblk0p1/test1.img bs=1M count=3072
```

示例输出：

```
3072+0 records in
3072+0 records out
```

再次执行 `df -h`，确认文件写入成功（`/dev/mmcblk0p1` 使用量显示 3.0G）：

```
/dev/mmcblk0p1    3.5G    3.0G   270.5M   92%   /mnt/mmcblk0p1
```

查看新生成的文件大小，如果大小正确，可以判定 eMMC 读写成功。

---

## 5 eMMC 常用调试方法

### 5.1 eMMC 设备读写测试

eMMC 设备被识别后，会在 `/dev/` 目录下生成名为 `mmcblk0` 的设备。可通过执行 `echo`、`hexdump` 等命令对 eMMC 设备进行简单的读写测试。

#### 5.1.1 写入数据

执行如下命令向识别到的 eMMC 设备（`/dev/mmcblk0`）写入数据：

```bash
echo HelloWorld! > /dev/mmcblk0
```

#### 5.1.2 读取数据

执行如下命令从识别到的 eMMC 设备（`/dev/mmcblk0`）读取数据：

```bash
hexdump -C -n 15 /dev/mmcblk0
```

示例输出：

```
00000000  48 65 6c 6c 6f 57 6f 72  6c 64 21 0a 00 00 00   |HelloWorld!....|
0000000f
```

### 5.2 查看当前系统支持的文件系统

执行如下命令查看当前 Linux 系统支持的文件系统类型：

```bash
cat /proc/filesystems
```

示例输出（当前系统支持的文件系统）：

```
nodev   sysfs
nodev   rootfs
nodev   bdev
nodev   proc
nodev   tmpfs
nodev   debugfs
nodev   sockfs
nodev   pipefs
nodev   anon_inodefs
nodev   configfs
nodev   devpts
        ext3
        ext2
        ext4
        squashfs
nodev   ramfs
        vfat
        fuseblk
nodev   fuse
nodev   fusectl
nodev   overlayfs
nodev   mqueue
nodev   mtd_inodefs
nodev   ubifs
```

如上可知，当前系统支持 vfat、ext2、ext3、ext4 等文件系统。

### 5.3 获取 eMMC 基本信息

识别 eMMC 时，eMMC 驱动代码已将一些常用信息通过文件的形式创建在 `/sys/` 目录下。用户可以通过 `cat` 等命令查看设备信息。常用命令如下：

```bash
# 列出 mmcblk0 块设备的所有 sysfs 属性文件
ls /sys/block/mmcblk0 -l

# 查看 eMMC 总容量（单位：512 字节扇区数）
cat /sys/block/mmcblk0/size

# 查看当前挂载情况
cat /proc/self/mounts

# 查看 eMMC 设备各类信息文件
cat /sys/block/mmcblk0/device/cid      # CID 寄存器（Card Identification）
cat /sys/block/mmcblk0/device/csd      # CSD 寄存器（Card Specific Data）
cat /sys/block/mmcblk0/device/date     # 制造日期
cat /sys/block/mmcblk0/device/fwrev    # 固件版本
cat /sys/block/mmcblk0/device/hwrev    # 硬件版本
cat /sys/block/mmcblk0/device/manfid   # 制造商 ID
cat /sys/block/mmcblk0/device/name     # 产品名称
cat /sys/block/mmcblk0/device/oemid    # OEM ID
cat /sys/block/mmcblk0/device/serial   # 序列号
cat /sys/block/mmcblk0/device/type     # 设备类型（MMC/SD）
cat /sys/block/mmcblk0/device/uevent   # 设备 uevent 信息
```

### 5.4 Linux MMC 驱动子系统简述

#### 5.4.1 Linux MMC 子系统介绍

MMC、SD、SDIO 三种技术均起源于 MMC 技术，有很多共性，因此 Linux Kernel 统一使用 MMC framework 管理所有和这三种技术有关的设备。

Linux 的 MMC 子系统在内核 `drivers/mmc/` 目录中实现，该目录下有 3 个子目录：`card`、`core` 和 `host`，分别用于存放 MMC 卡的块设备、核心层和主控制器的相关代码，并反映该子系统的层次架构。

MMC 子系统从上到下可分为 3 个层次：**Host 层**、**Core 层**和 **Card 层**。

**Host 层（主控制器驱动层）：**
- 也叫主控制器驱动层。主控制器通常是 SoC 上的一个外设，通过它，SoC 能够按照 MMC 协议规定的方式与卡进行数据传输。
- 在 `drivers/mmc/host/` 子目录下有很多主控制器驱动，其中包括模块的 SDIO 驱动 `sdhci-mmc.c`。

**Core 层（MMC 子系统核心层）：**
- 实现代码位于 `drivers/mmc/core/` 子目录中。
- 核心层（主控制器驱动程序）除为 Host 层驱动提供诸如 `mmc_alloc_host()`/`mmc_add_host()`/`mmc_remove_host()` 等分配、添加、删除 MMC 主控制器对象的接口外，还分别就 MMC 和 SDIO 两种规范实现了相应的协议代码。

**Card 层：**
- 因为 MMC 卡属于块设备，所以需要为这些设备提供块设备驱动。
- 在 `drivers/mmc/core/` 中，`block.c` 主要实现了 MMC 卡的通用块设备驱动，在 `queue.c` 中则实现了该块设备的请求队列及处理。
- MMC 卡块设备的主设备号是 **179**。

**Linux MMC 子系统架构图（层次结构）：**

```
┌────────────────────────────────────────────────────────────┐
│                 Virtual File System Layer                   │  Kernel Space
│                 Generic Block Layer                         │
│   ┌──────────────────────────────────────────────────────┐ │
│   │  mmc Block/Queue driver  (/mmc/card/block.c, queue.c)│ │
│   ├──────────────────────────────────────────────────────┤ │
│   │  mmc core driver  (/mmc/core/core.c, mmc.c, sd.c)   │ │
│   ├──────────────────────────────────────────────────────┤ │
│   │  mmc host driver  (/mmc/host/sdhci.c)                │ │
│   │  SDHCI Host driver                                   │ │
│   └──────────────────────────────────────────────────────┘ │
│         ↓                           ↓                       │  Hardware
│   eMMC device                 SD memory card                │
└────────────────────────────────────────────────────────────┘
```

#### 5.4.2 Linux MMC 子系统驱动初始化流程

通常，大多数问题出现在驱动初始化过程中，整理 MMC 驱动框架的初始化过程如下：

```
SDHCI Driver              MMC Core              MMC Card
─────────────────────────────────────────────────────────
sdhci_probe                                   mmc_blk_probe
    │                                               │
sdhci_alloc_host ──→ mmc_alloc_host           mmc_blk_alloc
                     INIT_DELAYED_WORK              │
                     (mmc_rescan)             mmc_add_disk
    │
sdhci_add_host ───→ mmc_add_host
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
                    mmc_add_card ────────────────→（触发 mmc_blk_probe）
```

### 5.5 打开调试信息

#### 5.5.1 打开 MMC 子系统调试信息

在 `drivers/mmc/core/core.c` 文件中添加如下宏定义，可打开 MMC 子系统的 pr_debug 调试日志：

```c
#undef pr_debug
#define pr_debug pr_err
```

修改后重新编译内核，MMC 子系统的所有调试信息将以 `pr_err` 级别输出，在 dmesg 中可见。

#### 5.5.2 打印 MMC 命令行 CMD log 信息

在 `/ql-ol-kernel/drivers/mmc/Makefile` 中增加如下行：

```makefile
subdir-ccflags-y = -DDEBUG
```

重新编译内核后，MMC 驱动将输出命令行级别（CMD level）的详细 log 信息，用于排查 MMC 命令交互问题。

---

## 6 Mdev 自动挂载

### 6.1 mdev 简介

`mdev` 是 busybox 提供的一个用在嵌入式系统中的工具，相当于简化版的 `udev`，用于在系统启动和热插拔或动态加载驱动程序时，自动创建设备节点。文件系统中 `/dev/` 目录下的设备节点都是由 `mdev` 创建的。

### 6.2 eMMC 自动挂载机制

eMMC 采用 `mdev` 的机制进行自动挂载。`mdev` 配置文件路径为 `ql-ol-rootfs/etc/mdev.conf`，相关 eMMC 配置如下所示；当识别到了 eMMC 设备且存在 eMMC 分区，就会运行 `/etc/mdev/automountsdcard.sh` 脚本。

**`/etc/mdev.conf` 中的 eMMC 相关配置：**

```
tun[0-9]* 0:0 0660

mmcblk[0-9]*         0:6    660
mmcblk[0-9]*p[0-9]* 0:6    660 */etc/mdev/automountsdcard.sh ${MDEV}
[hs]d[a-z][0-9]*    0:0    660 */etc/mdev/automountsdcard.sh ${MDEV}
```

- 当识别到 eMMC 分区设备（如 `mmcblk0p1`），自动触发 `/etc/mdev/automountsdcard.sh` 脚本，执行挂载操作。

使用 `df -Th` 命令查看分区挂载情况：

```
Filesystem          Type     Size    Used Available Use% Mounted on
/dev/Om-0           squashfs 13.3M   13.3M     0    100% /
tmpfs               tmpfs    45.5M  116.0K   45.4M    0% /tmp
ubi0:data           ubifs    90.7M   28.0K   86.0M    0% /data
ubi0:data           ubifs    90.7M   28.0K   86.0M    0% /mnt
ubi0:etc            ubifs     2.9M    1.9M  856.0K   69% /overlay/etc
ubi0:nvm            ubifs     2.0M   56.0K    1.8M    3% /NVM/overlay/nvm
overlayfs:/overlay/etc  overlay  2.9M  1.9M  856.0K  69% /etc
overlayfs:/overlay/nvm  overlay  2.0M  56.0K   1.8M   3% /NVM
/dev/ubiblock1_0    squashfs 128.0K 128.0K     0    100% /NVM/oem_data
tmpfs               tmpfs   512.0K    0   512.0K    0% /dev
/dev/mmcblk0p2      ext4      3.4G   32.0K   3.2G    0% /mnt/mmcblk0p2
/dev/mmcblk0p1      ext4     43.8M   16.0K  40.1M    0% /mnt/mmcblk0p1
```

分区 `/dev/mmcblk0p1` 挂载到 `/mnt/mmcblk0p1`，`/dev/mmcblk0p2` 挂载到 `/mnt/mmcblk0p2`。

**关闭自动挂载：**

若不使用 `mdev` 的机制挂载 eMMC，可删除 `mdev` 自动挂载 eMMC 机制，注释掉 `ql-ol-rootfs/etc/mdev.conf` 配置文件的 eMMC 自动挂载配置，如下所示（注释掉 `mmcblk[0-9]*p[0-9]*` 行）：

```
mmcblk[0-9]*               0:6    660
#mmcblk[0-9]*p[0-9]*  0:6    660 */etc/mdev/automountsdcard.sh ${MDEV}
[hs]d[a-z][0-9]*    0:0    660 */etc/mdev/automountsdcard.sh ${MDEV}
```

如需使用 mdev 机制自动挂载 eMMC，则可直接修改 `/etc/mdev/automountsdcard.sh` 自动挂载脚本。

---

## 7 附录 参考文档及术语缩写

### 表 2：参考文档

| 编号 | 文档名称 |
|------|---------|
| [1] | Quectel_AG35-CET_QuecOpen_快速开发指导 |
| [2] | Quectel_AG35-CET_QuecOpen_硬件设计手册（引脚定义） |

### 表 3：术语缩写

| 缩写 | 英文描述 | 中文描述 |
|------|---------|---------|
| API | Application Programming Interface | 应用程序编程接口 |
| CPU | Central Processing Unit | 中央处理器 |
| eMMC | Embedded Multi Media Card | 嵌入式多媒体存储卡 |
| EVB | Evaluation Board | 评估板 |
| GPIO | General-Purpose Input/Output | 通用型输入/输出 |
| IoV | Internet of Vehicles | 车联网 |
| I/O | Input/Output | 输入/输出 |
| MMC | Multi Media Card | 多媒体存储卡 |
| SD | Secure Digital | 安全数字（存储卡） |
| SDIO | Secure Digital Input Output | 安全数字输入输出 |
| SDK | Software Development Kit | 软件开发工具包 |
| SoC | System on Chip | 片上系统 |
