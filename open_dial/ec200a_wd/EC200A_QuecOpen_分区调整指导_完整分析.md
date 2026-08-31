# EC200A-CN(TA) QuecOpen 分区调整指导（完整分析）

> **标题**：EC200A-CN(TA) QuecOpen 分区调整指导
> **适用模块系列**：LTE Standard 模块系列（EC200A-CN(TA)）
> **版本**：1.0.0
> **日期**：2023-03-20
> **状态**：临时文件（Preliminary / Not Checked）
> **发布方**：上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.）

> 本文件是对原 PDF《Quectel_EC200A-CN(TA)_QuecOpen_分区调整指导_V1.0.0_Preliminary_20230320.pdf》（共 27 页正文）的逐页、逐表、逐图、逐代码完整还原与整理，不省略任何技术内容。

---

## 联系信息

**上海移远通信技术股份有限公司**
上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼　邮编：200233
电话：+86 21 5108 6236　邮箱：info@quectel.com
销售支持：http://www.quectel.com/cn/support/sales.htm
技术支持：http://www.quectel.com/cn/support/technical.htm　邮箱：support@quectel.com

---

## 前言 / 法律声明（摘要）

移远通信提供该文档内容以支持客户的产品设计。客户须按照文档中提供的规范、参数来设计产品。移远通信提供的参考设计仅作为示例，本文档及其所涉及服务在"可用"基础上提供。移远通信可在未事先通知的情况下随时增加、修改或重述本文档。

（含使用和披露限制、许可协议、版权声明、商标、第三方权利、隐私声明、免责声明等标准条款；与移远其他 QuecOpen 文档一致。）

版权所有 © 上海移远通信技术股份有限公司 2023，保留一切权利。
*Copyright © Quectel Wireless Solutions Co., Ltd. 2023.*

---

## 文档历史 / 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2022-04-15 | Jessie LEI | 文档创建 |
| 1.0.0 | 2023-03-20 | Nick LIN | 临时版本 |

---

## 目录（原文结构）

1. 引言
2. 分区调整说明
   - 2.1 分区介绍
   - 2.2 分区调整注意事项
3. 分区调整方法
   - 3.1 调整 Flash Layout（3.1.1 涉及文件 / 3.1.2 调整方法）
   - 3.2 适配 uboot 配置文件（3.2.1 涉及文件 / 3.2.2 适配方法）
   - 3.3 配置 blf 文件（3.3.1 涉及文件 / 3.3.2 配置方法）
   - 3.4 烧录固件
   - 3.5 添加挂载逻辑到新增分区
4. UBI 文件系统制作和加载
   - 4.1 制作 squashfs 文件系统镜像 / 加载 squashfs 文件系统
   - 4.2 新增分区 backup_data.ubi 固件制作
5. 附录 参考文档及术语缩写

**表格索引**：表 1 可调整的 Linux 分区概览、表 2 update.blf 配置文件标签描述、表 3 blf 文件列表、表 4 参考文档、表 5 术语缩写。

---

## 1 引言

移远通信 EC200A-CN(TA) 模块支持 QuecOpen® 方案。QuecOpen® 是开源的、基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。有关 QuecOpen® 的详细信息，请参考**文档 [1]**（《Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导》）。

本文档主要介绍在 QuecOpen® 方案下，移远通信 EC200A-CN(TA) 模块的分区调整方法以及相关注意事项。

---

## 2 分区调整说明

### 2.1 分区介绍

QuecOpen® 方案下，Linux 系统中用于存放和读写用户应用程序以及配置参数数据的相关分区如下表所示。

**表 1：可调整的 Linux 分区概览**

| 用户分区 | 默认空间 | 可用空间 | 分区格式 | 挂载点 | 分区用途 |
|---|---|---|---|---|---|
| oem_data | 20 MB | 约 20 MB | squashfs 文件系统 | `/NVM/oem_data` | 带有 A/B 分区互为备份，支持 OTA 升级，用于存放客户的应用程序和配置参数数据 |
| rootfs_data | 112 MB | 约 112 MB | ubifs 文件系统 | `/data`、`/mnt`、`/log`、`/overlay/etc`、`/overlay/nvm` | 系统可读写分区 |
| rootfs_data（续） | — | — | overlayfs 文件系统 | `/etc`、`/NVM` | 系统可读写分区 |

> **备注**：
> 1. `oem_data-a` 和 `oem_data-b` 分区用于存放用户程序及配置文件，挂载为只读。oem_data 分区用作 A/B 分区，有备份还原功能，某个分区损坏后会切换系统进行还原恢复；也有 OTA 升级功能。因此建议使用该分区存放重要应用程序及配置文件。
> 2. `rootfs_data` 分区以可读写形式挂载，分为三个 UBI 卷，其中 0 号卷（ubi0_0）分别挂载到 `/data`、`/mnt` 和 `/log` 目录，另两个卷分别挂载到 `/etc` 和 `/NVM` 目录。ubi0_0 卷裸分区大小约 100 MB，可用大小约 81 MB，其中移远通信会存放少量配置数据（≤ 6 MB）。由于分区挂载失败时会擦除该分区 `/etc` 和 `/NVM`，所以不要直接使用该分区，需新增 `backup_data` 分区。新增分区的裸区大小不大于 89 MB，可用大小不大于 70 MB。若需新增可读写的 `backup_data` 分区，请参考**第 3 章**。

### 2.2 分区调整注意事项

1. 除调整 `oem_data` 和 `rootfs_data` 分区的大小及新增分区（如可读写的 `backup_data` 分区）外，**请勿修改其他分区**。
2. 修改 `oem_data` 或 `rootfs_data` 分区大小：
   - `oem_data` 分区挂载在 `/NVM/oemdata` 目录下，建议用户将应用程序存放在该分区以便管理。同时，`oem_data` 分区分为 A 和 B 分区，修改 `oem_data-a` 分区大小时，需同步修改 `oem_data-b` 分区。
   - `rootfs_data` 分区位于 flash layout 的最后位置（`QUEC_BBM` 分区为系统预留的分区，不参与分区 flash layout 的划分），去除其他分区的占用空间之后，flash 的剩余空间都将分给 `rootfs_data`，因此修改 `oem_data-a`/`oem_data-b` 分区大小后，无需修改 `rootfs_data` 分区大小，只需调整 `rootfs_data` 分区偏移地址即可，但需保证 `rootfs_data` 分区能够正常挂载。
3. 新增分区：若新增一个可读写的 `backup_data` 分区，建议添加在 `rootfs_data` 分区之前。

---

## 3 分区调整方法

配置文件 `update.blf` 中定义了镜像列表，和 SWD Downloader 烧录界面中的镜像列表相对应。每个镜像包含了相应的镜像名称、ID 等属性。相关数据段如下所示（节选）：

```ini
[Image_List]
1_Image_Enable = 1
1_Image_Tim_Included = 1
1_Image_Image_ID = 0x54494D48
1_Image_Next_Image_ID = 0x4F424D49
1_Image_Path = tim_falcon_qspinand.bin
......
12_Image_Flash_Entry_Address = 0x04180000
12_Image_Load_Address = 0xFFFFFFFF
12_Image_Type = RAW
12_Image_ID_Name = GRBI
12_Image_Erase_Size =
12_Image_Partition_Number = 0
12_Image_Hash_Algorithm_ID = SHA-256
12_Image_Image_Size_To_Hash_in_bytes = 0xFFFFFFFF
13_Image_Enable = 1
13_Image_Tim_Included = 3
13_Image_Image_ID = 0x52464249
13_Image_Next_Image_ID = 0x4F534C4F
13_Image_Path = RFPLUGIN.bin
13_Image_Flash_Entry_Address = 0x04140000
13_Image_Load_Address = 0xFFFFFFFF
13_Image_Type = RAW
13_Image_ID_Name = RFBI
13_Image_Erase_Size =
13_Image_Partition_Number = 0
13_Image_Hash_Algorithm_ID = SHA-256
13_Image_Image_Size_To_Hash_in_bytes = 0xFFFFFFFF
```

**表 2：update.blf 配置文件标签描述**

| 标签名称 | 描述 |
|---|---|
| TIM Included | 若设置为 Tim Included，指定镜像将被 Bootloader 自动加载至内存 |
| Flash Entry Address | 指定镜像烧录到 Flash 中的物理地址 |
| Load Address | 指定镜像烧录到 DDR Memory 中对应的物理地址 |

### 3.1 调整 Flash Layout

#### 3.1.1 涉及文件

```
linux/arch/arm/boot/dts/asr1803_ab_flash_layout.dtsi
```

#### 3.1.2 调整方法

在相应文件中调整分区设备树 flash layout，原始（默认）分区布局如下：

```dts
partition@0 {
    label = "bootloader";
    reg = <0x0 0x40000>;
    read-only;
};
partition@40000 {
    label = "cp_reliabledata";
    reg = <0x40000 0x20000>;
};
        . . . . . . . .
partition@100000 {
    label = "dtim-a";
    reg = <0x100000 0x40000>;
};
partition@140000 {
    label = "cpimage-a";
    reg = <0x140000 0xf00000>;
};
partition@1040000 {
    label = "u-boot-a";
    reg = <0x1040000 0xc0000>;
};
partition@1100000 {
    label = "kernel-a";
    reg = <0x1100000 0x800000>;
};
partition@1900000 {
    label = "rootfs-a";
    reg = <0x1900000 0x1380000>;
};
partition@2c80000 {
    label = "tos-a";
    reg = <0x2c80000 0x80000>;
};
partition@2d00000 {
    label = "oem_data-a";
    reg = <0x2d00000 0x1400000>;
};
partition@4100000 {
    label = "dtim-b";
    reg = <0x4100000 0x40000>;
};
partition@4140000 {
    label = "cpimage-b";
    reg = <0x4140000 0xf00000>;
};
partition@5040000 {
    label = "u-boot-b";
    reg = <0x5040000 0xc0000>;
};
partition@5100000 {
    label = "kernel-b";
    reg = <0x5100000 0x800000>;
};
partition@5900000 {
    label = "rootfs-b";
    reg = <0x5900000 0x1380000>;
};
partition@6c80000 {
    label = "tos-b";
    reg = <0x6c80000 0x80000>;
};
partition@6d00000 {
    label = "oem_data-b";
    reg = <0x6d00000 0x1400000>;
};
partition@8100000 {
    label = "asr_flag";
    reg = <0x8100000 0x100000>;
};
partition@8200000 {
    label = "misc";
    reg = <0x8200000 0x40000>;
};
partition@8240000 {
    label = "cust_info";
    reg = <0x8240000 0x40000>;
};   // 若需新增分区，推荐在此处添加，并相应调整 rootfs_data 分区
partition@8280000 {
    label = "rootfs_data";
    reg = <0x8280000 0>;
};
```

**● 新增分区**

以新增一个大小为 16 MB 的可读写分区 `backup_data` 为例：

```dts
partition@8280000 {
    label = "backup_data";
    reg = <0x8280000 0X1000000>;   // 起止地址
};
partition@9280000 {
    label = "rootfs_data";
    reg = <0x9280000 0>;
}
```

**● oem_data 分区大小调整**

以将 `oem_data-a` 分区的大小由 20 MB 增加到 30 MB 为例：

入口地址不变，分区大小由 `0x1400000` 增加到 `0x1e00000`，同时相应调整 `oem_data-b`；由于 `oem_data-a` 及 `oem_data-b` 分区的调整会导致后面其他分区的入口地址的变化，用户需进行相应调整。

调整前：

```dts
partition@2d00000 {
    label = "oem_data-a";
    reg = <0x2d00000 0x1400000>;
};
partition@4100000 {
    label = "dtim-b";
    reg = <0x4100000 0x40000>;
}
```

调整后：

```dts
partition@2d00000 {
    label = "oem_data-a";
    reg = <0x2d00000 0x1e00000>;
};
partition@4b00000 {
    label = "dtim-b";
    reg = <0x4b00000 0x40000>;
}
```

> **备注**：调整完成后，需重新编译生成 `zImage` 镜像并替换到固件包中；编译方法参考**文档 [1]**。

### 3.2 适配 uboot 配置文件

#### 3.2.1 涉及文件

```
uboot/include/configs/falcon_p401.h
```

#### 3.2.2 适配方法

在 `uboot/include/configs/falcon_p401.h` 文件中适配分区表地址参数及 MTD 设备号，如下所示（关键宏定义）：

```c
#define NVM_MTD_PART        "rootfs_data"
#define NVM_UBI_VOLUME      "nvm"
#define NVM_UBIFS_DIR       "/root"
//#define CONFIG_USE_MEP_IN_CODE
#define MEP_OTA_FLASH_ADDRESS   0x000C0000
#define MEP_OTA_FLASH_LEN       (0x20000*2)

#define OEM_NVM_MTD_PART    "oem_data"
#define OEM_NVM_MTD_PART_A  "oem_data-a"
#define OEM_NVM_MTD_PART_B  "oem_data-b"

#define OEM_NVM_UBI_VOLUME  "oem_data"
#define OEM_NVM_UBIFS_DIR   "/config"

#define UBOOT_MTD_PART_A    "u-boot-a"
#define UBOOT_MTD_PART_B    "u-boot-b"

#define KERNEL_MTD_PART_A   "kernel-a"
#define KERNEL_MTD_PART_B   "kernel-b"

#define ROOTFS_MTD_PART_A   "rootfs-a"
#define ROOTFS_MTD_PART_B   "rootfs-b"

#define TOS_MTD_PART_A      "tos-a"
#define TOS_MTD_PART_B      "tos-b"

#ifdef CONFIG_CMD_FASTBOOT
#define CONFIG_MTDPARTS \
    "mtdparts=nand_mtd:0x00C0000@0x1040000(\"UBOOT_MTD_PART_A\"),"\
    "0x00C0000@0x5040000(\"UBOOT_MTD_PART_B\"),"\
    "0x0800000@0x1100000(\"KERNEL_MTD_PART_A\"),"\
    "0x0800000@0x5100000(\"KERNEL_MTD_PART_B\"),"\
    "0x0138000@0x1900000(\"ROOTFS_MTD_PART_A\"),"\
    "0x0138000@0x5900000(\"ROOTFS_MTD_PART_B\"),"\
    "0x0008000@0x2c80000(\"TOS_MTD_PART_A\"),"\
    "0x0008000@0x6c80000(\"TOS_MTD_PART_B\"),"\
    "0x1400000@0x2d00000(\"OEM_NVM_MTD_PART_A\"),"\   /* 大小发生改变，以改变后的地址大小填入 */
    "0x1400000@0x6d00000(\"OEM_NVM_MTD_PART_B\"),"\
    "0x7060000@0x8280000(\"NVM_MTD_PART\"),"          /* 调整后 rootfs_data 分区的起始地址发生改变 */
#else
```

原 `rootfs_data` 分区大小为 `0x7060000`，起始地址为 `0x8280000`。由于添加了 `backup_data` 分区后，`rootfs_data` 分区的大小减去了 `backup_data` 分区的大小，故 `rootfs_data` 分区大小变为 `0x7060000 - 0x1000000 = 0x6060000`，起始地址由 `0x8280000` 变为 `0x9280000`。

此外，还需同步更新如下参数（注意 BBM 大小变化也要改）：

```c
/* rootfs_data */
#define NVM_MTD_PART_SIZE   0x7060000   /* NOTE: if you change BBM size, you must modify this also */
#define NVM_MTD_PART_ADDR   0x8280000
```

`#else` 分支（非 FASTBOOT，修改为调整后的地址）：

```c
#else
#define CONFIG_MTDPARTS \
    "mtdparts=nand_mtd:0x7060000@0x8280000(\"NVM_MTD_PART\"),"\
    "0x01400000@0x2d00000(\"OEM_NVM_UBI_VOLUME\")"
#endif
```

`#ifdef CONFIG_CMD_FASTBOOT` 完整 mtdparts（调整为修改后的地址）：

```c
#ifdef CONFIG_CMD_FASTBOOT
#define CONFIG_MTDPARTS \
    "mtdparts=nand_mtd:0x00C0000@0x1040000(\"UBOOT_MTD_PART_A\"),"\
    "0x00C0000@0x5040000(\"UBOOT_MTD_PART_B\"),"\
    "0x0800000@0x1100000(\"KERNEL_MTD_PART_A\"),"\
    "0x0800000@0x5100000(\"KERNEL_MTD_PART_B\"),"\
    "0x0138000@0x1900000(\"ROOTFS_MTD_PART_A\"),"\
    "0x0138000@0x5900000(\"ROOTFS_MTD_PART_B\"),"\
    "0x0008000@0x2c80000(\"TOS_MTD_PART_A\"),"\
    "0x0008000@0x6c80000(\"TOS_MTD_PART_B\"),"\
    "0x1400000@0x2d00000(\"OEM_NVM_MTD_PART_A\"),"\
    "0x1400000@0x6d00000(\"OEM_NVM_MTD_PART_B\"),"\   /* 调整为修改后的地址 */
    "0x7060000@0x8280000(\"NVM_MTD_PART\"),"
#else
```

> **备注**：
> 1. 适配完成后，需重新编译生成 `u-boot.bin` 镜像并替换到固件包中；编译方法参考**文档 [1]**。
> 2. 若 `oem_data` 分区大小发生调整，上述文件中涉及的分区入口地址及大小需根据 flash layout 调整进行相应调整。

### 3.3 配置 blf 文件

#### 3.3.1 涉及文件

**表 3：blf 文件列表**

| 文件名称 | 描述 |
|---|---|
| `update.blf` | SWD Downloader 配置文件 |
| `quectel_AB_OTA.blf` | OTA 配置 blf 文件 |
| `quectel_skylark_pm802_standard_AB.blf` | 非工厂配置 blf 文件 |

blf 文件是与下载工具 SWD Downloader 配套使用的配置文件。其主要功能如下：
- 配置 DDR 参数
- 配置镜像列表
- 定义其他可配置参数

从使用角度，用户只需关注镜像列表的配置。

#### 3.3.2 配置方法

以调整 `oem_data` 分区及新增 `backup_data` 分区为例，修改 `update.blf` 文件。

**1. 直接修改 update.blf**

**● 新增 backup_data 分区供挂载使用**

若新增分区不烧录镜像，则无需新增 image_list，调整 `backup_data` 分区后的 `rootfs_data` 分区大小及入口地址即可：

```ini
End Reserved Data
[EraseOnly_Option]
Total_Eraseonly_Areas = 1
1_Eraseonly_Area_Size = 0x07060000              ; rootfs_data 分区大小
1_Eraseonly_Area_FlashStartAddress = 0x08280000 ; rootfs_data 入口地址
1_Eraseonly_Area_Partition = 0
1_Eraseonly_Area_Enable = 1
[Extended_Reserved_Data]
Consumer_ID
CID = TBRI
```

若新增分区需烧录镜像，则需新增 image_list。示例如下：

**a. 修改镜像数**

```
Number_of_Images = 18   改成   Number_of_Images = 19
```

**b. 修改 image_list**

```ini
18_Image_Enable = 1
18_Image_Tim_Included = 3
18_Image_Image_ID = 0x4F454D44
18_Image_Next_Image_ID = 0x44415441   ; 由 0xFFFFFFFF 改为 0x44415441
18_Image_Path = oem_data.ubi
18_Image_Flash_Entry_Address = 0x06D00000
18_Image_Load_Address = 0xFFFFFFFF
18_Image_Type = RAW
18_Image_ID_Name = OEMD
18_Image_Erase_Size = 0x01400000
18_Image_Partition_Number = 0
18_Image_Hash_Algorithm_ID = SHA-256
18_Image_Image_Size_To_Hash_in_bytes = 0xFFFFFFFF
////////// 新增下面这个 image list
19_Image_Enable = 1
19_Image_Tim_Included =
19_Image_Image_ID = 0x44415441
19_Image_Next_Image_ID = 0xFFFFFFFF
19_Image_Path = backup_data.ubi
19_Image_Flash_Entry_Address = ////  backup_data 分区的入口地址
19_Image_Load_Address = 0xFFFFFFFF
19_Image_Type = RAW
19_Image_ID_Name = DATA
19_Image_Erase_Size = ////  backup_data 分区的大小
19_Image_Partition_Number = 0
19_Image_Hash_Algorithm_ID = SHA-256
19_Image_Image_Size_To_Hash_in_bytes = 0xFFFFFFFF
```

**● 调整 oem_data 分区**

若调整了 `oem_data-a`、`oem_data-b` 分区的大小，需修改 `update.blf` 文件相关分区配置。由于 flash layout 中 `asr_flag` 分区位于 `oem_data` 分区之后，首先需调整非烧录分区 `asr_flag` 分区，如下：

```ini
OTAI
Enabled = 0x00000001
Flash_Address = 0x08100000   ; asr_flag 分区入口地址调整
Magic = 0x464F5441
End_OTAI
BBMT
```

其次，`dtim-b` 分区的入口地址发生相应偏移，也需修改：

```ini
3_Image_Map_Info
3_Image_ID = 0x54494D32
3_Image_Type = RECOVERYIMAGE       ; dtim-b 分区镜像类型
3_Flash_Address_Lo = 0x04100000    ; dtim-b 分区入口地址
3_Flash_Address_Hi = 0x00000000
3_Partition = 0x00000000
3_Enable = 1
3_End_Image_Map_Info
End_Image_Maps
Vendor_DDR_Initialization
```

此外，还需修改 `oem_data` 分区后有镜像的分区的入口地址（这些镜像随 oem_data 分区偏移进行偏移），节选 image_list：

```ini
10_Image_Tim_Included = 2
10_Image_Image_ID = 0x4F454D44
10_Image_Next_Image_ID = 0x41524249
10_Image_Path = oem_data.ubi          ; oem_data 分区固件
10_Image_Flash_Entry_Address = 0x02D00000   ; oem_data 分区入口地址
10_Image_ID_Name = OEMD
10_Image_Erase_Size = 0x01400000
11_Image_Path = ARBEL.bin
11_Image_Flash_Entry_Address = 0x06380000    ; 随 oem_data 分区偏移进行偏移
11_Image_ID_Name = ARBI
12_Image_Path = MSA.bin
12_Image_Flash_Entry_Address = 0x06190000
12_Image_ID_Name = GRBI
13_Image_Path = RFPLUGIN.bin
13_Image_Flash_Entry_Address = 0x06140000
13_Image_ID_Name = RFBI
14_Image_Path = u-boot.bin
14_Image_Flash_Entry_Address = 0x06040000
14_Image_ID_Name = OSLO
15_Image_Path = zImage
15_Image_Flash_Entry_Address = 0x06100000
15_Image_ID_Name = ZIMG
16_Image_Path = root.squashfs
16_Image_Flash_Entry_Address = 0x06900000
16_Image_ID_Name = SYSJ
16_Image_Erase_Size = 0x01380000
17_Image_Path = tos.bin
17_Image_Flash_Entry_Address = 0x06C80000
17_Image_ID_Name = TZSI
18_Image_Path = oem_data.ubi
18_Image_Flash_Entry_Address = 0x06D00000
18_Image_ID_Name = OEMD
18_Image_Erase_Size = 0x01400000
```

**2. 使用 part_resizing.sh 脚本辅助修改 blf 文件**

修改后的 blf 文件位于 `tools/fota/target/` 路径：

- 新增 `backup_data` 分区大小为 16 MB：

```bash
./part_resizing.sh backup_data 16
```

  脚本输出示例：
  ```
  NEW_DATA_NAME: backup_data
  NEW_DATA_SIZE: 16 M
  rootfs_data size:0x06060000      ; 调整后的 rootfs_data 分区地址大小
  rootfs_data address:0x09280000
  ```
  生成修改后的 blf 文件（`quectel_AB_OTA.blf`、`quectel_skylark_pm802_standard_AB.blf`、`update.blf`）于 `target/` 目录。

- 新增 `backup_data` 分区大小为 16 MB，并调整 `oem_data` 分区大小为 30 MB：

```bash
./part_resizing.sh backup_data 16 30
```

  脚本输出示例（调整后各分区地址和大小）：
  ```
  NEW_DATA_NAME: backup_data
  NEW_DATA_SIZE: 16 M
  NEW_OEM_DATA_SIZE: 30 M
  rootfs_data size:0x04c60000
  rootfs_data address:0x0A480000
  dtim-b address:0x04B00000
  RFPLUGIN address:0x04B40000
  MSA address:0x04B80000
  ARBEL address:0x04F80000
  uboot-b address:0x05A40000
  kernel-b address:0x05B00000
  rootfs-b address:0x06300000
  tos-b address:0x07680000
  oem_data-b address:0x07700000
  asr_flag address:0x09500000
  ```

> **备注**：如需 OTA 升级，需修改 `quectel_skylark_pm802_standard_AB.blf` 和 `quectel_AB_OTA.blf` 制作 OTA 升级包，修改方式可参考 `update.blf` 文件的修改，也可使用 `part_resizing.sh` 脚本辅助修改。

### 3.4 烧录固件

将 `zImage`（详见**第 3.1.2 章**）、`u-boot.bin`（详见**第 3.2.2 章**）和 blf 文件（详见**第 3.3.2 章**）替换到固件包内并烧录到模块后，分区修改生效。执行 `cat /proc/mtd` 显示 MTD 分区信息，如下所示（调整后实例：新增 16 MB backup_data，oem_data-a/b 各扩到 30 MB）：

```text
root@OpenWrt:~# cat /proc/mtd
dev:    size      erasesize  name
mtd0:  00040000  00020000  "bootloader"
mtd1:  00020000  00020000  "cp_reliabledata"
mtd2:  00020000  00020000  "ap_reliabledata"
mtd3:  00020000  00020000  "mep-ota"
mtd4:  00020000  00020000  "cp_reliabledata_backup"
mtd5:  00020000  00020000  "ap_reliabledata_backup"
mtd6:  00020000  00020000  "mep-ota_backup"
mtd7:  00040000  00020000  "dtim-a"
mtd8:  00f00000  00020000  "cpimage-a"
mtd9:  000c0000  00020000  "u-boot-a"
mtd10: 00800000  00020000  "kernel-a"
mtd11: 01380000  00020000  "rootfs-a"
mtd12: 00080000  00020000  "tos-a"
mtd13: 01e00000  00020000  "oem_data-a"    // 由 20 MB(0x01400000) 增加到 30 MB(0x01e00000)
mtd14: 00040000  00020000  "dtim-b"
mtd15: 00f00000  00020000  "cpimage-b"
mtd16: 000c0000  00020000  "u-boot-b"
mtd17: 00800000  00020000  "kernel-b"
mtd18: 01380000  00020000  "rootfs-b"
mtd19: 00080000  00020000  "tos-b"
mtd20: 01e00000  00020000  "oem_data-b"    // 由 20 MB(0x01400000) 增加到 30 MB(0x01e00000)
mtd21: 00100000  00020000  "asr_flag"
mtd22: 00040000  00020000  "misc"
mtd23: 00040000  00020000  "cust_info"
mtd24: 01000000  00020000  "backup_data"   // 新增 backup_data 分区 16 MB(0x01000000)
mtd25: 04c60000  00020000  "rootfs_data"   // 由 112 MB(0x07060000) 减少到 76 MB(0x04c60000)
                                            // 减少的 36 MB：16 MB 给新增 backup_data，
                                            // 另 20 MB 给 oem_data-a/-b 各扩 10 MB
mtd26: 00d20000  00020000  "QUEC_BBM"
```

> **备注**：由于调整分区对其后分区会产生影响并需相应修改，非必要不建议调整分区表。

### 3.5 添加挂载逻辑到新增分区

需要挂载使用的分区，可以使用 `ql-ol-rootfs/lib/preinit/81_mount_ubifs_overlay` 脚本对其进行调整。以将新增 `backup_data` 分区挂载为 ubifs 文件系统为例，添加挂载逻辑如下：

**1. 获取新增分区的 MTD 号**（`+` 为新增行）：

```sh
ubifs_volume_support()
    mtdpart_idx="$(find_mtd_index rootfs_data)"
    [ -z "$mtdpart_idx" ] && return 1
+   mtdpart_idx_backup_data="$(find_mtd_index backup_data)"
+   [ -z "$mtdpart_idx_backup_data" ] && return 1
    mtdpart_idx_oem="$(find_mtd_index oem_data$SLOT)"
    if [ -z "$mtdpart_idx_oem" ]
    then
        # oem_data may has only one partition
        mtdpart_idx_oem="$(find_mtd_index oem_data)"
        [ -z "$mtdpart_idx_oem" ] && return 1
    fi
    grep -qs ubifs /proc/filesystems || return 1
    echo "found rootfs_data partition and ubifs support"
    return 0
}
```

**2. 添加挂载 ubifs 的可读写挂载目录 /mnt/backup_data**：

```sh
ubifs_backup_data_mount() {
    recover_ubifs=0   /// recover_ubifs 为 0，挂载正常；为 1，检查挂载所需设备节点异常，格式化重新创建节点
    [ ! -e /dev/ubi2 ] && ubiattach /dev/ubi_ctrl -m $mtdpart_idx_backup_data -d 2 || recover_ubifs=1   // ubi 设备异常
    if [ $recover_ubifs -eq 0 ]
    then
        ubi2_nod_id=`cat /sys/class/ubi/ubi2/dev | tr -s ":" " "`
        [ ! -e /dev/ubi2 ] && mknod /dev/ubi2 c ${ubi2_nod_id}
        if [ ! -e /sys/class/ubi/ubi2_0/dev ]
        then
            # no volume
            recover_ubifs=1   // 卷设备异常
        else
            # check for "backup_data" volume
            ubi2_0_nod_id=`cat /sys/class/ubi/ubi2_0/dev | tr -s ":" " "`
            [ ! -e /dev/ubi2_0 ] && mknod /dev/ubi2_0 c ${ubi2_0_nod_id}
            { ubinfo /dev/ubi2_0 | grep Name | grep -qs "backup_data" ; } || \
                recover_ubifs=1   // 卷设备节点异常
        fi
    fi
    if [ $recover_ubifs -eq 1 ]   /// 检查设备及节点异常后进行分区格式化重新创建修复
    then
        echo "ubifs data partition is damaged"
        echo "try to recover by formatting $mtdpart..."
        [ -e /dev/ubi2 ] && ubidetach -m $mtdpart_idx_backup_data
        ubiformat -y -q /dev/mtd$mtdpart_idx_backup_data   // 格式化 mtd
        ubiattach -m $mtdpart_idx_backup_data /dev/ubi_ctrl   // 做 ubi 设备
        ubi2_nod_id=`cat /sys/class/ubi/ubi2/dev | tr -s ":" " "`
        [ ! -e /dev/ubi2 ] && mknod /dev/ubi2 c ${ubi2_nod_id}
        ubimkvol /dev/ubi2 -n 0 -N backup_data -t dynamic --maxavsize   // 做卷设备
    fi
    mount -t ubifs -o rw,noatime,bulk_read ubi2:backup_data /mnt/backup_data   // 挂载
    return 0
}
```

**3. 调用 ubifs_data_mount**（`+` 为新增行）：

```sh
ubifs_syscfg_rootfs_pivot() {
    echo "switching to ubifs sysfs overlay"
    # PIPE mode need to recover this file in /etc/init.d/network
    cp -rf /etc/config/network /tmp/network_pipe
    # Dir /etc and /NVM can be written after creating overlay
    create_overlay /etc $overlay_mountpoint/etc /etc
    create_overlay /NVM $overlay_mountpoint/nvm /NVM
    # jessie.lei-2022/6/7 During the ota upgrade process, the rootfs_data partition will be erased,
    # and the ota upgrade state will be erased, so the erased NEEDSYNC state is written here during the ota upgrade.
    update_flag=`hexdump -n 8 -s 176 /dev/mtdblock21 | awk '{print$2}'`
    if [ $update_flag -eq 1 ];
    then
        touch /NVM/.fota_state.txt
        echo 5 > /NVM/.fota_state.txt
    # Prevent the status query from failing after the first burning flash
    elif [ ! -e "/NVM/.fota_state.txt" ];
    then
        touch /NVM/.fota_state.txt
        echo 0 > /NVM/.fota_state.txt
    fi
    ubifs_oem_data_mount
+   mkdir -p /mnt/backup_data        // 创建挂载点
+   ubifs_backup_data_mount          // 调用挂载功能
}
```

挂载完成后，新增 data 分区的状态如下所示（`df -h` 与 `mount`）：

```text
root@openwrt:~# df -h
Filesystem              Size  Used  Available  Use%  Mounted on
/dev/de-0              14.3M 14.3M     0       100%  /
tmpfs                  45.5M  112K  45.4M       0%   /tmp
ubi0:data              53.2M 32.0K  50.4M       0%   /data
ubi0:data              53.2M 32.0K  50.4M       0%   /mnt
ubi0:data              53.2M 32.0K  50.4M       0%   /log
ubi0:etc               2.9M   1.8M  964.0K     65%   /overlay/etc
ubi0:nvm               2.0M   56.0K  1.8M       3%   /overlay/nvm
overlayfs:overlay/etc  2.9M   1.8M  964.0K     65%   /etc
/dev/ubiblock1_0       120.0K 120.0K   0       100%  /NVM/oem_data
ubi2:backup_data       7.5M   20.0K   7.5M      0%   /mnt/backup_data    ← 新增挂载
/dev/mmcblk0p2         1.5G  28.0K   1.4G       0%   /mnt/mmcblk0p2
/dev/mmcblk0p1         1.9G  30.3M   1.8G       2%   /mnt/mmcblk0p1

root@openwrt:~# mount
... ubi2:backup_data on /mnt/backup_data type ubifs (rw,seclabel,noatime,bulk_read,assert=read-only,ubi=2,vol=0)
```

---

## 4 UBI 文件系统制作和加载

本章以 `EC200ACNTAR02A02M2G_OCPU` 版本为例介绍 UBI 固件的制作和加载方法。

### 4.1 oem_data.ubi 制作和加载

用户使用 `oem_data` 分区将应用程序及配置文件打包进 `oem_data.ubi` 固件并烧录至模块。squashfs 文件系统的镜像制作和加载方法如下所示。

#### 4.1.1 制作 squashfs 文件系统镜像

解压 SDK 后进入 `/ql-ol-extsdk-ec200acntar02a02m2g_ocpu/` 目录下，执行 **`make oemapp`** 创建 `/oemapp/` 目录。该目录下有 `/config/` 目录用于存放 GKI 相关配置，用户无需关注。将用户数据存入 `/oemapp/` 目录中，然后执行 **`make oemapp`** 在 `/target/` 目录下生成 `oem_data.ubi`。该 UBI 固件打包了 `oem_data.squashfs`。最后将生成的 `oem_data.ubi` 新固件替换到固件包内进行烧录，或通过 FOTA 方式进行烧录。

> 制作过程命令示意：`mkdir cust_app`（在 oemapp 下建用户目录）→ `make oemapp` → 工具链调用 `mksquashfs4` 生成 `oem_data.squashfs`，再用 `ubinize -o ... oem_data.ubi -m 2048 -p 128KiB -s 2048 ... ubinize-oem.cfg` 打包成 `oem_data.ubi`。
> 生成结果（target 目录）：`oem_data.ubi`、`oem_data.squashfs`、`ubinize-oem.cfg` 等。

#### 4.1.2 加载 squashfs 文件系统

squashfs 文件系统的加载，第一步是创建设备节点，第二步是 squashfs 文件系统的挂载。在系统重启时执行 `ql-ol-extsdk-ec200acntar02a02m2g_ocpu/ql-ol-rootfs/lib/preinit/81_mount_ubifs_overlay` 脚本即可实现此过程。

### 4.2 backup_data.ubi 制作

**1. 修改 Makefile，新增 backup_data 分区**，如下所示：

```makefile
export QL_TARGET_DIR=$(QL_SDK_DIR)/target
export IMAGE_SYSFS_UBI_NAME=root.squashfs
export QL_OEMAPP_DIR=$(QL_SDK_DIR)/oemapp
export QL_BACKUP_DIR=$(QL_SDK_DIR)/backup_data
$(shell mkdir -p target)

.PHONY:all

oemapp:
	mkdir -p $(QL_TARGET_DIR); \
	mkdir -p $(QL_OEMAPP_DIR); \
	mkdir -p $(QL_OEMAPP_DIR)/config; \
	fakeroot $(QL_TOOLS_BIN_DIR)/mksquashfs4 $(QL_OEMAPP_DIR) $(QL_TARGET_DIR)/oem_data.squashfs -nopad -noappend -root-owned -comp gzip -b 128k -processors 1; \
	cp $(QL_TOOLS_CONF_DIR)/ubinize-oem.cfg $(QL_TARGET_DIR); \
	cd $(QL_TARGET_DIR); \
	fakeroot $(QL_TOOLS_BIN_DIR)/ubinize -o $(QL_TARGET_DIR)/oem_data.ubi -m 2048 -p 128KiB -s 2048 $(QL_TARGET_DIR)/ubinize-oem.cfg

backup_data:
	mkdir -p $(QL_TARGET_DIR); \
	mkdir -p $(QL_BACKUP_DIR); \
	mkdir -p $(QL_BACKUP_DIR)/config; \
	fakeroot $(QL_TOOLS_BIN_DIR)/mksquashfs4 $(QL_BACKUP_DIR) $(QL_TARGET_DIR)/backup_data.squashfs -nopad -noappend -root-owned -comp gzip -b 128k -processors 1; \
	cp $(QL_TOOLS_CONF_DIR)/ubinize-backup.cfg $(QL_TARGET_DIR); \
	cd $(QL_TARGET_DIR); \
	fakeroot $(QL_TOOLS_BIN_DIR)/ubinize -o $(QL_TARGET_DIR)/backup_data.ubi -m 2048 -p 128KiB -s 2048 $(QL_TARGET_DIR)/ubinize-backup.cfg

.PHONY:sample
```

**2. 参考 `ubinize-oem.cfg`** 在 `ql-ol-extsdk-ec200acntar02a02m2g_ocpu/tools/conf` 下新建 `ubinize-backup.cfg`：

```ini
[backup_data]
# Volume mode (other option is static)
mode=ubi
# Source image
image=backup_data.squashfs
# Volume ID in UBI image
vol_id=0
# Allow for dynamic resize
vol_type=dynamic
#vol_type=static
# Volume name
vol_name=backup_data
# Total amount of logical eraseblocks minus 4(two for internal Volume,
# one for Wear-leveling, one for Scrubbing), then multiplied by 124KB
vol_size=9424KiB
#vol_flags=autoresize
```

其中 `vol_size` 计算方法如下：

> 逻辑擦除块总数 = backup_data 分区大小 / 物理块大小；
> vol_size = (逻辑擦除块总数 − 4) × 逻辑块大小。
> 其中，减去的 4 个块：2 个用于内部卷，1 个用于磨损平衡，1 个用于擦洗。
>
> 例如，若 backup_data 分区大小为 10 MiB，物理块大小为 128 KiB，逻辑块大小为 124 KiB，则
> **vol_size = [(10 × 1024)/128 − 4] × 124 = 9424 KiB**。

**3. 在 `ql-ol-extsdk-ec200acntar02a02m2g_ocpu/` 根目录下执行 `make backup_data`** 生成 `backup_data.ubi`：

> 过程：`mksquashfs4` 生成 `backup_data.squashfs` → `ubinize -o ... backup_data.ubi -m 2048 -p 128KiB -s 2048 ... ubinize-backup.cfg`。
> 生成的 UBI 固件位于 `ql-ol-extsdk-ec200acntar02a02m2g_ocpu/target/` 目录下：`backup_data.squashfs`、`backup_data.ubi`、`root.squashfs`、`root.squashfs.fakeroot-script`、`ubinize-backup.cfg`。

---

## 5 附录 参考文档及术语缩写

**表 4：参考文档**

| 文档名称 |
|---|
| [1] Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

**表 5：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| DDR | Double Data Rate | 双倍数据速率 |
| FOTA | Firmware Over-The-Air | 固件空中升级 |
| GKI | General Kernel Image | 通用内核镜像 |
| ID | Identifier | 标识符 |
| IoT | Internet of Things | 物联网 |
| MTD | Memory Technology Device | 存储器技术设备 |
| SDK | Software Development Kit | 软件开发工具包 |
| UBI | Unsorted Block Image | 无排序区块图像 |

---

## 关键要点速记（分区调整备忘）

- **可调分区**：`oem_data`（A/B 互备、squashfs 只读、挂 `/NVM/oem_data`、20 MB）与 `rootfs_data`（ubifs 可读写、112 MB）；其余分区**勿改**。
- **rootfs_data 风险**：挂载失败会被擦除（`/etc`、`/NVM`），不要直接使用，应新增 `backup_data` 分区。新增分区建议加在 `rootfs_data` 之前，裸区 ≤ 89 MB、可用 ≤ 70 MB。
- **调整四步走**：① 改 flash layout（`asr1803_ab_flash_layout.dtsi`，重编 `zImage`）→ ② 适配 uboot（`uboot/include/configs/falcon_p401.h`：mtdparts、`NVM_MTD_PART_SIZE/ADDR`，重编 `u-boot.bin`）→ ③ 配 blf（`update.blf`/OTA blf，可用 `part_resizing.sh` 辅助）→ ④ 烧录后 `cat /proc/mtd` 验证。
- **rootfs_data 在最后**：oem_data 扩容后只需移动 rootfs_data 偏移、其大小自动占满剩余空间；`QUEC_BBM` 为系统预留、不参与划分。
- **新增 backup_data 例**：16 MB（`0x1000000`），起始 `0x8280000`；rootfs_data 大小 `0x7060000 - 0x1000000 = 0x6060000`，起始挪到 `0x9280000`。
- **挂载逻辑**：改 `81_mount_ubifs_overlay`，加 `ubifs_backup_data_mount`（ubiattach/ubiformat/ubimkvol/mount，含损坏自修复），挂到 `/mnt/backup_data`。
- **UBI 制作**：`make oemapp` → `oem_data.ubi`；`make backup_data` → `backup_data.ubi`（需 Makefile 加规则 + `ubinize-backup.cfg`，`vol_size = (分区/物理块 − 4) × 逻辑块`，10 MiB → 9424 KiB）。
- **blf 标签**：`TIM Included`（自动加载到内存）、`Flash Entry Address`（Flash 物理地址）、`Load Address`（DDR 物理地址）。
- **OTA 升级**：改 `quectel_skylark_pm802_standard_AB.blf` 与 `quectel_AB_OTA.blf`。
- **总告诫**：调整分区会影响其后所有分区入口地址，非必要不建议调整。
