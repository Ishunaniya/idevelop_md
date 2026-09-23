# EC2x&EG9x&EG25-G 系列 QuecOpen Linux 分区调整指导

> **模块系列**：LTE Standard 模块系列
> **版本**：1.0
> **日期**：2020-11-13
> **状态**：受控文件
> **版权**：版权所有 © 上海移远通信技术股份有限公司 2020，保留一切权利。Copyright © Quectel Wireless Solutions Co., Ltd. 2020.

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2020-06-13 | 肖召猛 | 文档创建 |
| 1.0 | 2020-11-13 | 肖召猛 | 初始版本 |

---

## 目录

1. [引言](#1-引言)
   - 1.1 [适用模块](#11-适用模块)
2. [QuecOpen® Linux 分区介绍](#2-quecopen-linux-分区介绍)
3. [分区调整](#3-分区调整)
   - 3.1 [添加分区](#31-添加分区)
   - 3.2 [删除分区](#32-删除分区)
   - 3.3 [注意事项](#33-注意事项)
4. [UBI 文件系统制作和加载](#4-ubi-文件系统制作和加载)
   - 4.1 [制作 UBI 文件系统镜像](#41-制作-ubi-文件系统镜像)
   - 4.2 [下载 UBI 文件系统至相应分区](#42-下载-ubi-文件系统至相应分区)
   - 4.3 [加载 UBI 文件系统](#43-加载-ubi-文件系统)
5. [附录 参考文档和术语缩写](#5-附录-参考文档和术语缩写)

### 表格索引

- 表 1：适用模块
- 表 2：可调整的 Linux 分区概览
- 表 3：partition_nand.xml 配置文件标签描述
- 表 4：参考文档
- 表 5：术语缩写

---

## 1 引言

移远通信 LTE Standard EC2x 系列、EG9x 系列和 EG25-G 模块支持 QuecOpen® 方案；QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。有关 QuecOpen® 的详细信息，请参考文档 [1]。

本文档主要介绍在 QuecOpen® 方案下，如何调整用于存放和读写固件包中用户应用程序及配置参数数据的 Linux 操作系统分区，以及如何制作和加载分区 UBI 文件系统。

### 1.1. 适用模块

**表 1：适用模块**

| 模块系列 | 模块 |
|---|---|
| EC2x 系列 | EC25 系列 |
| | EC21 系列 |
| | EC20 R2.1 |
| | EC20-CN |
| EG9x 系列 | EG95 系列 |
| | EG91 系列 |
| EG25-G | EG25-G |

> **备注**：本文档仅适用于 **512 MB RAM + 256 MB ROM** QuecOpen® 模块。

---

## 2 QuecOpen® Linux 分区介绍

如下表格所述为 QuecOpen® 方案下，Linux 系统中用于存放和读写用户应用程序以及配置参数数据的相关分区。**除下表所述分区外，其他分区均不可做任何修改，并且不可用于存放或读写用户应用程序和配置参数数据。**

**表 2：可调整的 Linux 分区概览**

| 用户分区 | 默认空间 | 可用空间 | 分区格式 | 挂载点 | 分区用途 |
|---|---|---|---|---|---|
| `usr_data` | 90.5 MB | 约 88.5 MB | UBI 文件系统 | `\usrdata` | • 存放和读写用户应用程序和配置参数数据。 |
| `sys_back` | 58 MB | 禁用 | 镜像 | 不挂载 | • rootfs 卷备份。<br>• 此分区不建议使用。 |
| `system` | 284.5 MB | 约 230.9 MB | UBI 文件系统 | `\`（根目录） | • rootfs 卷。<br>• 存放和读写用户应用程序和配置参数数据。 |

有关上述分区使用和调整的相关注意事项，详见第 [3.3 章](#33-注意事项)。

> **备注**：有关用户应用程序及重要参数备份还原方案，请参考文档 [2]。

---

## 3 分区调整

`partition.mbn` 文件为烧录至模块的分区表，位于固件包里 `\update` 目录下。`partition_nand.xml` 配置文件用于制作 `partition.mbn`，其内容如下图所示。

> **图 1：partition_nand.xml 配置文件**（原文档为截图，内容形如下文 [4.2 章](#42-下载-ubi-文件系统至相应分区) 所示的 `<partition>` 标签结构）

**表 3：partition_nand.xml 配置文件标签描述**

| 标签名称 | 描述 | 补充说明 |
|---|---|---|
| `<partition>` 和 `</partition>` | 这两个标签之间为一个分区的配置信息 | - |
| `<name>` | 该分区的名称 | - |
| `<size_kb>` | 该分区所占 NAND 的大小 | 单位：KB；大小需为 **128 KB 的整数倍** |
| `<pad_kb>` | 该分区用于冗余的大小 | 大小：一般为 **128~512 KB** |
| `<which_flash>` 和 `<attr>` | - | 上图所示中 `<which_flash>` 和 4 个 `<attr>` 的配置按照 `system` 分区属性配置。 |
| `<img_name>` | UBI 文件系统镜像 | - |

### 3.1. 添加分区

添加分区的步骤如下：

1. 参考其他分区的配置，在 `partition_nand.xml` 配置文件里添加一个 `<partition>` 标签的内容；

2. 将 `partition_nand.xml` 拷贝到 `partition.mbn` 分区表的环境里，制作出新的 `partition.mbn` 文件；`partition.mbn` 分区表的制作步骤如下：
   1) 将模块 SDK 包复制到 Linux 系统环境（需提前安装相关开发环境）里；
   2) 选择 `\partition_make` 目录，将修改后的 `partition_nand.xml` 复制到 `\partition_make\common\config` 目录里；
   3) 在 `\partition_make\common\build` 目录下，执行 `partition_gen.sh` 脚本即可生成 `partition.mbn` 文件；

3. 完成 `partition.mbn` 制作后，在 `partition_make\common\build` 目录下，执行脚本 `build.py`，在 SDK 包中重新生成 `\firehose` 目录下的三个文件（`prog_nand_firehose_9x07.mbn` 除外）；用这三个文件替换原始固件包里 `\update\firehose` 目录下对应的文件（对应的文件名相同，无需对 `prog_nand_firehose_9x07.mbn` 做任何操作）。

4. 用新的 `partition.mbn` 文件替换固件包里旧的 `partition.mbn` 文件；

5. 通过 QFlash 工具在 Firehose 模式下载固件。

6. 进行固件版本烧录，分区添加生效。

> **备注**
> 1. 执行 `partition_gen.sh` 脚本时，若有权限限制问题上报，需先在 `\partition_make` 目录下执行 `sudo chmod –R 777 ./*` 命令来修改相关权限。
> 2. 需使用移远通信在 **2018 年 03 月 15 号之后**提供的 `\partition_make` 目录。
> 3. 通过 QFlash 工具在 Firehose 模式进行固件下载，和具体模块使用的 NAND flash 的 totalsize、pagesize、totalpage、blocksize 等属性有关。`\partition_make` 目录提供的是默认适用于 **totalsize 为 4 Gbit、pagesize 为 4 KB** 的 NAND flash。
>    - 若用户使用的模块中 NAND flash 的 totalsize 为 **4 Gbit、pagesize 为 2 KB**，则需使用 `\partition\make\common` 路径下的 `nand_mbn_generator-4Gbit-2kPagesize.py` 文件替换 `nand_mbn_generator.py` 文件；
>    - 若用户使用的模块中 NAND flash 的 totalsize 为 **4 Gbit、pagesize 为 4 KB**，则需使用 `nand_mbn_generator-4Gbit-4kPagesize.py` 替换 `nand_mbn_generator.py` 文件。

### 3.2. 删除分区

只需将 `partition_nand.xml` 配置文件里对应 `<partition>` 标签包含的内容删除，即可删除对应分区。

### 3.3. 注意事项

- **请勿随意调整分区顺序**，尤其是所有分区中的前 3 个分区。

- 若事先未和移远通信研发工程师确认，**请勿删除移远通信原始固件包的已有分区**。

- 用户可根据实际需要调整 `usr_data` 分区大小，但**请勿改变分区名称和分区挂载点**；且如需进行 DFOTA 升级，则 `usr_data` 分区总空间至少要预留为 **60 MB**；若用户应用程序和模块固件需一起升级，则预留空间还需加上用户应用程序大小，但总空间不可超过 `usr_data` 分区的总可用空间（约 **88.5 MB**）。

- `usr_data` 分区默认在位于 `\etc\init.d` 下的 `find_partitions.sh` 脚本里加载完成。**若该分区挂载失败，将自动重新格式化再使用。因此，存放在该分区的应用程序或者数据皆有丢失的风险。** 存放在该分区的文件数据是开放的，可根据自身编程需求调整，具体实施方法可以参考 `find_partitions.sh` 里的代码。

- `sys_back` 分区用于备份 `system` 分区。若 `system` 分区存放数据过大（为 `system` 分区可用大小使用率的 **85% 或以上**），`sys_back` 分区大小需做相应调整，但请注意分区大小**最少要达到 `system.ubi` 文件大小加 6 MB**。

- `system` 分区要放在**最后**，添加的分区可以放在其前面。更改所述三分区任一分区后，注意计算最终预留给 `system` 分区空间是否足够；若 `system` 分区空间不够，可能导致其在模块开机时无法加载，从而造成 `system` 分区反复还原。用户在 `system.ubi` 文件系统里添加自己的应用程序后，重新制作出来的 `system.ubi` 文件大小**不能超过 50 MB，或者要比 `sys_back` 分区小 6 MB**。

> **备注**：`system` 分区在使用时不建议增加大数据（大小为 `system` 分区可用大小使用率的 85% 或以上）。

---

## 4 UBI 文件系统制作和加载

如用户增加了新的分区，则需制作该分区对应的 UBI 文件系统，具体方法如本章节所述。

### 4.1. 制作 UBI 文件系统镜像

分区添加方法，请参考第 [3.1 章](#31-添加分区)。分区添加完成后，需制作 UBI 文件系统镜像，具体方法请参考文档 [2]。

### 4.2. 下载 UBI 文件系统至相应分区

用户可在 `partition_nand.xml` 配置是否下载 UBI 文件系统。若要下载，在 `<partition>` 标签的最后一行添加要下载至该分区的 UBI 文件系统镜像名称，之后将制作的 UBI 文件系统镜像文件放在固件包里。固件下载时会将该文件下载到 NAND 的对应分区里。

```xml
<partition>
    <name length="16" type="string">0:usr_data</name>
    <size_kb length="4">126564</size_kb>
    <pad_kb length="4">512</pad_kb>
    <which_flash>0</which_flash>
    <attr>0xFF</attr>
    <attr>0x01</attr>
    <attr>0x00</attr>
    <attr>0xFF</attr>
    <img_name type="string">usrdata.ubi</img_name>
</partition>
```

> **备注**：通过 QFlash 工具在 Firehose 模式进行固件下载时，请参考第 [3.1 章](#31-添加分区)更换 `\firehose` 目录下对应的配置文件。

### 4.3. 加载 UBI 文件系统

系统重启时执行 `EC20FXX_OCPU_SDK\ql-ol-sdk\ql-ol-rootfs\etc\init.d\find_partitions.sh` 脚本加载 UBI 文件系统，具体可以参考该脚本里的代码实现，如下所示。UBI 文件系统加载分为两步：第一步是 UBI 文件系统添加，第二步是 UBI 卷的挂载。

```sh
eval FindAndMountVolume${fstype} usrfs /data
eval FindAndMount${fstype} modem /firmware
#quectel add for usr_data partition mount
eval FindAndMountUsrdata${fstype} usr_data /usrdata
```

> **备注**：新添加的 UBI 文件系统需挂载在 `usr_data` 分区的 UBI 文件系统**后面**，请勿随意变动顺序。

---

## 5 附录 参考文档和术语缩写

**表 4：参考文档**

| 序号 | 文档名称 | 描述 |
|---|---|---|
| [1] | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_快速开发指导 | 适用于 EC2x 系列、EG9x 系列和 EG25-G QuecOpen 模块的快速开发指导 |
| [2] | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_用户应用程序及重要配置参数备份还原参考方案 | 适用于 EC2x 系列、EG9x 系列和 EG25-G QuecOpen 模块的用户应用程序及重要配置参数备份还原参考方案 |

**表 5：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| DFOTA | Delta Firmware Over-The-Air | 固件空中差分升级 |
| UBI | Unsorted Block Image | 无排序区块图像 |
| SDK | Software Development Kit | 软件开发工具包 |
