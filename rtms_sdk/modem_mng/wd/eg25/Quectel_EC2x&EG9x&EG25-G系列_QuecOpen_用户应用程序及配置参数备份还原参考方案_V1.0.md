# EC2x&EG9x&EG25-G 系列 QuecOpen — 用户应用程序及配置参数备份还原参考方案

> **模块系列：** LTE Standard 模块系列
> **版本：** 1.0
> **日期：** 2021-01-07
> **状态：** 受控文件
> **版权：** Copyright © Quectel Wireless Solutions Co., Ltd. 2021. 保留一切权利。

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2020-06-13 | Tinker SUN | 文档创建 |
| 1.0 | 2021-01-07 | Tinker SUN | 受控版本 |

---

## 目录

- [1 引言](#1-引言)
  - [1.1 适用模块](#11-适用模块)
- [2 相关分区](#2-相关分区)
- [3 备份还原参考方案](#3-备份还原参考方案)
  - [3.1 参考方案](#31-参考方案)
  - [3.2 可能出现的场景](#32-可能出现的场景)
  - [3.3 测试方式](#33-测试方式)
- [4 参考脚本](#4-参考脚本)
- [5 附录 参考文档和术语缩写](#5-附录-参考文档和术语缩写)

### 表格索引

- 表 1：适用模块
- 表 2：可用于用户应用程序存放和备份还原的分区概览
- 表 3：参考文档
- 表 4：术语缩写

### 图片索引

- 图 1：检测脚本文件
- 图 2：备份还原参考方案流程图

---

## 1 引言

移远通信 LTE Standard EC2x 系列、EG9x 系列和 EG25-G 模块支持 QuecOpen® 方案；QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。有关 QuecOpen® 的详细信息，请参考文档 [1]。

本文档主要介绍了在 QuecOpen® 方案下备份和还原用户应用程序及配置参数的参考方案。Linux 操作系统分区用于存放和读写这些应用程序及配置参数。

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

## 2 相关分区

如下表格所述为 QuecOpen® 方案下，Linux 系统中用于存放和读写用户应用程序以及配置参数的相关分区。可用于用户应用程序存放和备份还原的分区为 **system 分区**和 **usr_data 分区**。

**表 2：可用于用户应用程序存放和备份还原的分区概览**

| 用户分区 | 分区格式 | 挂载点 | 分区用途 |
|---|---|---|---|
| system | UBI 文件系统 | `/`（根目录） | • rootfs 卷。<br>• 存放和读写用户应用程序和配置参数。 |
| usr_data | UBI 文件系统 | `/usrdata` | • 存放和读写用户应用程序和配置参数。 |

有关用于存放和读写这些应用程序及配置参数的 Linux 操作系统分区的详细信息，请参考文档 [2]。

---

## 3 备份还原参考方案

### 3.1 参考方案

用户应用程序及配置参数分别在 **system 分区**和 **usr_data 分区**各存放一份。模块开机时，在运行 `/etc/init.d` 路径下的 `find_partitions.sh` 脚本后，模块运行用于自动备份和还原的检测脚本 `app_auto_backup_restore.sh`（如下图 1 所示，用户可自行选择存放路径，并已附于文档侧边栏），以检测两个分区中用户应用程序是否存在：

- 若其中一个分区的用户应用程序不存在，则从对方分区将用户应用程序及配置参数复制到该分区。
- 若两个分区的用户应用程序都存在，则检查两个分区里的应用程序版本是否一致。
  - 若版本号不一致，则从对方分区将版本号高的用户应用程序及配置参数复制至该分区并覆盖原有用户应用程序及配置参数。

具体流程如下图 2 所示。

由于 system 分区和 usr_data 分区不会同时损坏，即使用户应用程序已升级，该备份还原机制可以保证用户应用程序及配置参数不丢失；若文件系统升级，还能保证用户应用程序为最新版本。

> 图 1：检测脚本文件 —— 检测脚本文件 `app_auto_backup_restore.sh`（文档侧边栏附件）。

> **图 2：备份还原参考方案流程图（文字化还原）**
>
> ```
>                          模块开始运行
>                              │
>                              ▼
>   在运行 /etc/init.d 路径下的 find_partitions.sh 脚本后，
>   运行 app_auto_backup_restore.sh 脚本，判断工作区
>   和备份区的用户应用程序是否存在
>                              │
>          ┌───────────────────┼───────────────────────────┐
>          ▼                   ▼                           ▼
>  usr_data 分区的用户     system 分区的用户        usr_data 和 system 分区
>  应用程序不存在          应用程序不存在            的用户应用程序都存在
>          │                   │                           │
>          └─────────┬─────────┘                           ▼
>                    ▼                          检查两个分区里的应用程序版本
>       从对方分区将用户应用程序及                是否一致。若不一致，则从对方分区
>       配置参数复制到该分区                      将版本号高的用户应用程序及配置参数
>                                               复制至该分区并覆盖原有用户应用程序
>                                               及配置参数。
>          └───────────────────┬───────────────────────────┘
>                              ▼
>                       系统依序继续正常运行
> ```

> **备注**
> 1. 由于 Linux 文件系统的缓存原理，如果在备份时突然断电可能会导致备份的应用程序不完整，而代码却没有“察觉”到，下次还原时系统无法运行。解决方法是先复制用户应用程序及配置参数文件，之后执行同步操作，最后复制用户应用程序版本号标记文件 `app_ver.txt`。
> 2. 如果用户应用程序及配置参数在 system 和 usr_data 之外的分区存放，可以通过修改脚本 `app_auto_backup_restore.sh`：将 system 或 usr_data 分区的挂载目录名称改为该分区的挂载目录以进行备份还原。
> 3. 备份后的两个文件夹要分别放在 system 和 usr_data 分区，因为系统内核检测到文件系统损坏后会格式化单独的整个分区。

### 3.2 可能出现的场景

本章节主要分析了用户应用程序及配置参数备份还原实际运行中可能出现的场景及特点。例如若用户应用程序及配置参数存放在 system 分区（存放路径为 `/usrapp/apprun/apps`），则备份区为 usr_data（备份路径为 `/usrdata/appbackup/apps`），实际运行中可能会出现如下四种场景：

- **场景一（首次烧录固件）：** 模块第一次烧录固件，`/usrdata/appbackup/app` 备份区是空的，该路径下的用户应用程序版本号标记文件 `app_ver.txt` 也不存在，`app_auto_backup_restore.sh` 脚本将会把 `/usrapp/apprun/apps` 路径下的文件夹和标记文件 `app_ver.txt` 复制至 `/usrdata/appbackup/app` 备份区完成备份。

- **场景二（usr_data 分区损坏）：** 模块在后期运行时，如果 usr_data 分区损坏，在加载 `find_partitions.sh` 脚本时会格式化 usr_data 分区，此时备份数据和备份标记文件丢失。`app_auto_backup_restore.sh` 脚本将会把 `/usrapp/apprun/apps` 路径下文件夹和标记文件 `app_ver.txt` 复制到 `/usrdata/appbackup` 路径下完成备份。

- **场景三（用户应用程序升级）：** 用户应用程序升级成功后，`/usrapp/apprun/apps` 路径下的 `app_ver.txt` 文件中的版本号将同时递增。即使不对备份区进行更新，下次开机时，`app_auto_backup_restore.sh` 脚本检测到版本号变更后，将会把 `/usrapp/apprun/apps` 路径下文件夹和标记文件 `app_ver.txt` 复制到 `/usrdata/appbackup` 路径下完成备份。

- **场景四（system 分区损坏、发生还原）：** 用户应用程序进行了升级，并更新了备份区，但在后期的使用中模块的 system 分区损坏，发生了还原，此时 `/usrapp/apprun/apps` 路径下的文件夹内容会被还原为出厂状态，`app_auto_backup_restore.sh` 脚本会判断出备份区的应用程序版本号较高，会将备份区 `/usrdata/appbackup/apps` 路径下文件夹和标记文件 `app_ver.txt` 复制到 `/usrapp/apprun/apps` 路径下完成备份。

### 3.3 测试方式

可以选用如下任意一种方法来测试用户应用程序及配置参数是否备份还原成功。

1. **删除 system 或 usr_data 分区的用户应用程序及版本号标记文件。**
   - 删除 system 分区的用户应用程序及版本号标记文件，重启模块（或直接运行 `app_auto_backup_restore.sh` 脚本），检查 usr_data 分区的应用程序及版本号标记文件是否同步至 system 分区。
   - 删除 usr_data 分区的用户应用程序及版本号标记文件，重启模块（或直接运行 `app_auto_backup_restore.sh` 脚本），检查 system 分区的应用程序及版本号标记文件是否同步至 usr_data 分区。

2. **更新 system 或 usr_data 分区的用户应用程序及版本号标记文件。**
   - 在 system 分区的应用程序里添加文件，同时将版本号标记文件内的版本增加 1，重启模块（或者直接运行 `app_auto_backup_restore.sh` 脚本），检查 system 分区的应用程序和版本号标记文件是否同步至 usr_data 分区。
   - 同理更新 usr_data 分区的应用程序及版本号标记文件，重启模块（或者直接运行 `app_auto_backup_restore.sh` 脚本），检查 usr_data 分区的应用程序及版本号标记文件是否同步至 system 分区。

3. **系统正常运行后，使用 fastboot 模式擦除 system 或 usr_data 分区。**
   - 系统正常运行后，使用 fastboot 模式擦除 system 分区，重启模块后等待系统正常运行，检查 system 分区里应用程序是否存在，且和 usr_data 分区里的一致。
   - 系统正常运行后，使用 fastboot 模式擦除 usr_data 分区，重启模块后等待系统正常运行，检查 usr_data 分区里应用程序是否存在，且和 system 分区里的一致。

---

## 4 参考脚本

用户应用程序及配置参数备份还原参考方案的参考脚本如下。

```sh
#!/bin/sh
# Copyright (c) 2014, The Linux Foundation. All rights reserved.
#
# example app_auto_backup_restore.sh
#
# if app.bin stored in system partition (/usrapp/app.bin, /usrapp/app_Ver)
# app backup partition is usr_data in (/usrdata/appbackup/app.bin, /usrdata/appbackup/app_Ver)
#
# /usrapp/apprun/apps
# /usrapp/apprun/app_ver.txt
# /usrdata/appbackup/apps
# /usrdata/appbackup/app_ver.txt

Ver1=0
Ver2=0
app1="/usrapp/apprun/apps"
app1_Ver="/usrapp/apprun/app_ver.txt"
#just support 1,2,3,4,5,6,7 ......
app2="/usrdata/appbackup/apps"
app2_Ver="/usrdata/appbackup/app_ver.txt"
# you'd better make sure the usr_data partition is mount ok in here
if [ ! -f $app1_Ver ];then
       Ver1=-1
else
       Ver1=`cat $app1_Ver`
fi
if [ ! -f $app2_Ver ];then
       Ver2=-1
else
       Ver2=`cat $app2_Ver`
fi

if [ "$Ver1" -eq "$Ver2" ];then
       echo "app verion same , exit !!!"
       exit 0
fi

echo -n " app version update now!!!!"
if [ "$Ver1" -gt "$Ver2" ];then
       echo -n " app update to usr_data partition /usrdata/appbackup/"
       rm -rf $app2
       rm -rf $app2_Ver
       mkdir -p $app2
       cp -rf $app1 $app2
       sync
       cp -rf $app1_Ver $app2_Ver
       sync
fi
if [ "$Ver1" -lt "$Ver2" ];then
       echo -n " app update to system partition /usrapp/apprun/"
       rm -rf $app1
       rm -rf $app1_Ver
       mkdir -p $app1
       cp -r $app2 $app1
       sync
       cp -rf $app2_Ver $app1_Ver
       sync
fi
```

---

## 5 附录 参考文档和术语缩写

**表 3：参考文档**

| 序号 | 文档名称 | 描述 |
|---|---|---|
| [1] | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_快速开发指导 | 适用于 EC2x 系列、EG9x 系列和 EG25-G QuecOpen 模块的快速开发指导 |
| [2] | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_Linux 分区调整指导 | 适用于 EC2x 系列、EG9x 系列和 EG25-G QuecOpen 模块的 Linux 分区调整指导 |

**表 4：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| UBI | Unsorted Block Image | 无排序区块图像 |
