# EC2x&EG9x&EG25-G 系列 QuecOpen — RTC 闹钟应用指导

> **模块系列：** LTE Standard 模块系列
> **版本：** EC2x&EG9x&EG25-G 系列_QuecOpen_RTC 闹钟应用指导_V1.0
> **日期：** 2020-07-24
> **状态：** 受控文件
> **版权：** Copyright © Quectel Wireless Solutions Co., Ltd. 2020. 保留一切权利。

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| 1.0 | 2020-07-24 | 高飞虎 / 徐西洋 | 初始版本 |

---

## 目录

- [1 引言](#1-引言)
  - [1.1 适用模块](#11-适用模块)
- [2 RTC 硬件电路设计推荐](#2-rtc-硬件电路设计推荐)
  - [2.1 RTC 硬件电路参考设计](#21-rtc-硬件电路参考设计)
- [3 设置系统时间](#3-设置系统时间)
- [4 RTC 闹钟功能](#4-rtc-闹钟功能)
  - [4.1 RTC 闹钟功能使用说明](#41-rtc-闹钟功能使用说明)
  - [4.2 功能验证](#42-功能验证)
- [5 RTC 闹钟唤醒功能](#5-rtc-闹钟唤醒功能)
  - [5.1 RTC 闹钟唤醒使用说明](#51-rtc-闹钟唤醒使用说明)
  - [5.2 功能验证](#52-功能验证)
- [6 附录 A 参考文档及术语缩写](#6-附录-a-参考文档及术语缩写)

### 表格索引

- 表 1：适用模块
- 表 2：参考文档
- 表 3：术语缩写

---

## 1 引言

移远通信 LTE Standard EC2x&EG9x&EG25-G 系列模块支持 QuecOpen® 方案。模块内置 RTC（Real Time Clock）设备，客户可基于该 RTC 设备实现闹钟功能、闹钟唤醒功能，同时也可以将其作为普通定时器使用。本文档主要用于指导客户如何快速使用这些模块的 RTC 闹钟及唤醒功能。

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

## 2 RTC 硬件电路设计推荐

### 2.1 RTC 硬件电路参考设计

> 图（原文配图，标题为「表 1：RTC 硬件电路参考设计（以 EC21/EC25 系列模块为例）」）：RTC 硬件电路参考设计原理图，展示 VBAT_BB 引脚为内置硬件 RTC 供电的连接方式。

移远通信 EC2x&EG9x&EG25-G 系列 QuecOpen 模块的 PMIC 内置了硬件 RTC，但是没有专用的供电控制引脚。该硬件 RTC 是由上图中的 **VBAT_BB** 引脚供电，因此务必保证 **VBAT_BB 在关机状态下可正常供电**。

---

## 3 设置系统时间

EC2x&EG9x&EG25-G 系列 QuecOpen 模块中后台进程 `ql_time_daemon` 会自动同步系统时间，并维护模块的硬件 RTC。

若客户需要手动修改系统时间，可通过将时间信息写入文件 `/tmp/ql_time_set_pipe` 来实现，文件写入格式为 `user: UTC 时间（单位：毫秒）`。

例如，将系统时间设置为 `20180101 01:01:10`，可通过如下命令将时间信息写入上述文件：

```bash
echo "user: 1514768470000" > /tmp/ql_time_set_pipe
```

详细信息可参考文档 [8]。

---

## 4 RTC 闹钟功能

### 4.1 RTC 闹钟功能使用说明

参考 SDK 包中路径 `ql-ol-sdk/ql-ol-extsdk/example/posix_timer/` 下例程 `example_timer.c` 对时间进行修改，以实现客户具体需求。

### 4.2 功能验证

1. 解压 SDK 包，进入 `ql-ol-sdk` 文件夹下，执行如下命令编译例程：

   ```bash
   $ source ql-ol-crosstool/ql-ol-crosstool-env-init
   $ cd ql-ol-extsdk/example/posix_timer/
   $ make clean
   $ make
   ```

2. 在 SDK 包中路径 `ql-ol-sdk/ql-ol-extsdk/example/posix_timer` 下生成名为 `example_timer` 的可执行程序。生成的可执行程序可通过如下 adb 命令推送到模块文件系统：

   ```bash
   $ adb push example_timer /data/
   $ adb shell chmod a+x /data/example_timer
   ```

3. 进入 Linux shell 终端，执行如下命令执行例程：

   ```bash
   $ cd /data/
   $ ./example_timer &
   ```

4. 大概 **100 秒**后，模块就会定时打印 log 到 Linux 终端。

---

## 5 RTC 闹钟唤醒功能

### 5.1 RTC 闹钟唤醒使用说明

参考路径 `ql-ol-sdk/ql-ol-extsdk/example/posix_timer/` 下例程 `example_suspend_alarm.c` 对时间进行修改，以实现客户具体需求。

### 5.2 功能验证

1. 解压 SDK 包，进入 `ql-ol-sdk` 文件夹下，执行如下命令编译例程：

   ```bash
   $ source ql-ol-crosstool/ql-ol-crosstool-env-init
   $ cd ql-ol-extsdk/example/posix_timer/
   $ make clean
   $ make
   ```

2. 在 SDK 中路径 `ql-ol-sdk/ql-ol-extsdk/example/posix_timer` 下生成名为 `example_suspend_alarm` 的可执行程序。生成的可执行程序可通过如下 adb 命令推送到模块文件系统：

   ```bash
   $ adb push example_suspend_alarm /data/
   $ adb shell chmod a+x /data/example_suspend_alarm
   ```

3. 进入 Linux shell 终端，执行如下命令执行例程：

   ```bash
   $ cd /data/
   $ ./example_suspend_alarm &
   ```

4. 在 Linux 终端，使用 `echo mem > /sys/power/autosleep` 命令配置模块进入自动休眠，此时 Linux 终端无法交互。之后每隔 **5 秒**，系统会被唤醒，此时 Linux 终端可以实现交互。

---

## 6 附录 A 参考文档及术语缩写

**表 2：参考文档**

| 序号 | 文档名称 | 备注 |
|---|---|---|
| [1] | Quectel_EC21_QuecOpen_Hardware_Design | EC21 QuecOpen 硬件设计手册 |
| [2] | Quectel_EC25_QuecOpen_Hardware_Design | EC25 QuecOpen 硬件设计手册 |
| [3] | Quectel_EC20_R2.1_QuecOpen_硬件设计手册 | EC20 R2.1 QuecOpen 硬件设计手册 |
| [4] | Quectel_EC20-CN_QuecOpen_硬件设计手册 | EC20-CN QuecOpen 硬件设计手册 |
| [5] | Quectel_EG91_QuecOpen_Hardware_Design | EG91 QuecOpen 硬件设计手册 |
| [6] | Quectel_EG95_QuecOpen_Hardware_Design | EG95 QuecOpen 硬件设计手册 |
| [7] | Quectel_EG25-G_QuecOpen_Hardware_Design | EG25-G QuecOpen 硬件设计手册 |
| [8] | Quectel_EC2x_QuecOpen_Linux 系统时间说明 | EC2x QuecOpen Linux 系统时间说明 |

**表 3：术语缩写**

| 术语 | 英文全称 | 中文全称 |
|---|---|---|
| PMIC | Power Management IC | 电源管理集成电路 |
| RTC | Real Time Clock | 实时时钟 |
| SDK | Software Development Kit | 软件开发工具包 |
