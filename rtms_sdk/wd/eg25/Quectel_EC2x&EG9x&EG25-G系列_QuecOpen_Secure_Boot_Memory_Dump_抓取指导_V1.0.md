# EC2x&EG9x&EG2x-G 系列 QuecOpen — Secure Boot Memory Dump 抓取指导

> **模块系列：** LTE Standard 模块系列
> **版本：** 1.0
> **日期：** 2020-08-24
> **状态：** 受控文件
> **版权：** Copyright © Quectel Wireless Solutions Co., Ltd. 2020. 保留一切权利。

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| 1.0 | 2020-08-24 | 李循威 | 初始版本 |

---

## 目录

- [1 引言](#1-引言)
  - [1.1 适用模块](#11-适用模块)
- [2 Memory Dump 抓取步骤](#2-memory-dump-抓取步骤)
  - [2.1 修改签名工具的配置文件](#21-修改签名工具的配置文件)
    - [2.1.1 读取芯片 SN](#211-读取芯片-sn)
    - [2.1.2 修改签名工具的配置文件](#212-修改签名工具的配置文件)
  - [2.2 重新签名固件](#22-重新签名固件)
  - [2.3 下载固件](#23-下载固件)
  - [2.4 测试和抓取方法](#24-测试和抓取方法)
- [3 附录 A 术语缩写](#3-附录-a-术语缩写)

---

## 1 引言

移远通信 LTE Standard EC2x 系列、EG9x 系列和 EG25-G 模块支持 QuecOpen® 方案。模块中的 Memory Dump 文件可用于确定其 Linux 系统中的问题或错误。Memory Dump 通常提供系统终止或崩溃之前的最后状态信息，如存储器位置、程序状态和其他相关详细信息。

QuecOpen® 方案下，移远通信 EC2x 系列、EG9x 系列和 EG25-G 模块支持 Secure Boot 机制；出于安全考虑，该机制使能后会默认阻止 Memory Dump 信息。为满足客户的不同需求，模块在 QuecOpen® 方案下亦允许客户在 Secure Boot 使能后抓取 Memory Dump 信息。

本文档主要介绍使用移远通信 EC2x 系列、EG9x 系列和 EG25-G QuecOpen® 模块时，如何在使能 Secure Boot 的情况下抓取 Memory Dump 信息，包括设置步骤和测试方法。

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

## 2 Memory Dump 抓取步骤

若模块已使能 Secure Boot，但仍希望能够抓取 Memory Dump 信息，则可通过**修改签名工具的配置文件、重新签名固件并下载运行新固件**实现 Memory Dump 抓取。以下章节为具体设置步骤。

### 2.1 修改签名工具的配置文件

#### 2.1.1 读取芯片 SN

在模块文件系统的 `cat /sys/devices/soc0/serial_number` 路径下获取芯片的 SN 号，并转换为**十六进制**数据。

#### 2.1.2 修改签名工具的配置文件

修改签名工具中的配置文件，即将芯片 SN 转换成的十六进制数据写入到该配置文件中。

所述配置文件位于签名工具包的如下路径：

```
common\sectools\config\9x07\9x07_secimage.xml
```

以十六进制格式的芯片 SN `0x11223344` 为例，请根据如下步骤和示例进行修改：

1. **修改 `<debug>` 参数：**
   将该参数的高字节设置为十六进制格式的芯片 SN，低字节设置为 `3`；若十六进制格式的 SN 为 `0x11223344`，则应修改 `<debug>` 为 `0x1122334400000003`，如下图所示。

   > 图（原文配图）：`9x07_secimage.xml` 配置文件中 `<debug>` 参数被修改为 `0x1122334400000003` 的截图。

2. **修改 `<crash_dump>` 参数：**
   将该参数的高字节设置为十六进制格式的 SN，低字节设置为 `1`；若十六进制格式的 SN 为 `0x11223344`，则应修改 `<crash_dump>` 为 `0x1122334400000001`，如下图所示。

   > 图（原文配图）：`9x07_secimage.xml` 配置文件中 `<crash_dump>` 参数被修改为 `0x1122334400000001` 的截图。

### 2.2 重新签名固件

配置文件修改完成后，重新签名固件。

请使用移远通信释放的签名工具对固件重新签名；详细步骤请参考签名工具中的使用文档。请联系移远通信技术支持（support@quectel.com）获取签名工具。

### 2.3 下载固件

将重新签名后的固件重新下载至模块并覆盖上一次签名的固件。请联系移远通信技术支持（support@quectel.com）获取固件下载工具。

### 2.4 测试和抓取方法

完成下载重新签名的固件后，可在模块 Linux 系统中执行如下命令，从而测试是否配置正确（即是否可进入 Memory Dump 抓取模式）：

```bash
echo 0 > /sys/bus/msm_subsys/devices/subsys1/system_reset_mode
echo system > /sys/bus/msm_subsys/devices/subsys1/restart_level
echo c > /proc/sysrq-trigger
```

上述**第一条和第二条命令**用于配置系统在遇到异常时进入 Memory Dump 抓取模式，**第三条命令**用于触发 Panic。

若前面章节所述配置正确且成功下载了重新签名的固件，模块将仅保留一个 **DM 的 USB 虚拟串口**。使用 **QPST** 工具连接模块后，即可通过这个 USB 虚拟串口抓取 Memory Dump 信息。

> **备注**
> 如需使用 QPST 工具，需高通授权。

---

## 3 附录 A 术语缩写

**表 2：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| DM | Device Manager | 设备管理器 |
| QPST | Qualcomm Product Support Tool | 高通产品支持工具 |
| SN | Serial Number | 序列号 |
| USB | Universal Serial Bus | 通用串行总线 |
