# EC200A-CN(TA) QuecOpen 固件烧录指导（完整分析）

> **标题**：EC200A-CN(TA) QuecOpen 固件烧录指导
> **适用模块系列**：LTE Standard 模块系列（EC200A-CN(TA)）
> **版本**：1.0.0
> **日期**：2022-07-08
> **状态**：临时文件（Preliminary / Not Checked）
> **发布方**：上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.）

> 本文件是对原 PDF《Quectel_EC200A-CN(TA)_QuecOpen_固件烧录指导_V1.0.0_Preliminary_20220708.pdf》（共 9 页正文）的逐页、逐表、逐图完整还原，不省略任何技术内容。

---

## 联系信息

**上海移远通信技术股份有限公司**
上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼　邮编：200233
电话：+86 21 5108 6236　邮箱：info@quectel.com
销售支持：http://www.quectel.com/cn/support/sales.htm
技术支持：http://www.quectel.com/cn/support/technical.htm　邮箱：support@quectel.com

---

## 前言 / 法律声明（摘要）

移远通信提供该文档内容以支持客户的产品设计。客户须按照文档中提供的规范、参数来设计产品。移远通信提供的参考设计仅作为示例，本文档和其所涉及服务在"可用"基础上提供。移远通信可在未事先通知的情况下随时增加、修改或重述本文档。

（含使用和披露限制、许可协议、版权声明、商标、第三方权利、隐私声明、免责声明等标准条款；与移远其他 QuecOpen 文档一致。）

版权所有 © 上海移远通信技术股份有限公司 2022，保留一切权利。
*Copyright © Quectel Wireless Solutions Co., Ltd. 2022.*

---

## 文档历史 / 修订记录

| 版本 | 日期 | 作者 | 变更描述 |
|---|---|---|---|
| - | 2021-11-18 | Morris XIAO | 文档创建 |
| 1.0.0 | 2022-07-08 | William Liu | 临时版本 |

---

## 目录（原文结构）

1. 引言
2. 固件烧录步骤
   - 2.1 USB 驱动安装
   - 2.2 固件烧录
3. 附录 参考文档及术语缩写

**表格索引**：表 1 参考文档、表 2 术语缩写

**图片索引**：
- 图 1：USB 驱动安装完成界面
- 图 2：选择烧录文件
- 图 3：待烧录文件显示
- 图 4：待烧录状态
- 图 5：固件烧录完成

---

## 1 引言

移远通信 EC200A-CN(TA) 模块支持 QuecOpen® 方案。QuecOpen® 是基于 Linux 的嵌入式开发平台，可简化应用的软件设计和开发过程。有关 QuecOpen 的详细信息，请参考**文档 [1]**（《Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导》）。

移远通信 EC200A-CN(TA) 模块固件版本需要使用专用下载工具 **SWDownloader.exe** 烧录到模块中。本文档介绍如何使用 SWDownloader.exe 工具烧录 EC200A-CN(TA) 模块对应的固件版本。

> **备注**：联系移远通信技术支持获取 SWDownloader.exe 工具。

---

## 2 固件烧录步骤

### 2.1 USB 驱动安装

在使用烧录工具之前，首先需要在 PC 上安装 EC200A-CN(TA) USB 驱动（**Quectel_Windows_USB_Driver(A)_Customer**），请向移远通信技术支持获取驱动安装包。之后，解压驱动安装包，双击 **`setup.exe`** 文件，按照提示完成安装即可。驱动安装完成后，界面如下图所示。

> **图 1：USB 驱动安装完成界面** —— `Quectel_Windows_USB_Driver(A)_Customer - InstallShield Wizard`，提示 *Maintenance Complete*（InstallShield Wizard has finished performing maintenance operations on Quectel_Windows_USB_Driver(A)_Customer），点击 **Finish** 完成。

### 2.2 固件烧录

**步骤 1**：打开 **SWDownloader.exe** 工具，选择菜单 **"File" -> "Open"**，然后选择待烧录固件版本包中 **`update` 目录**中的 **`update.blf`** 文件，如图 2 所示。

> **图 2：选择烧录文件** —— 工具版本 `SWDownloader 4.9.1.3`，从固件包路径（示例 `EC200ACNTAR02A01M2G_OCPU\update`）选取 `update.blf`（示例大小 47 KB，BLF 文件）。

**步骤 2**：打开对应的 `.blf` 文件之后，会出现如图 3 所示界面（待烧录镜像列表）。

> **图 3：待烧录文件显示** —— `SWDownloader 4.9.1.3 - [update.blf]`，列表列含：S.(勾选)、P.、T.、ImgID、Erase、Flash Address、Load Addr、HashAlgm、Size to Ha...、ImgType、Img File Path。
>
> 镜像清单（ImgID 一览，来自固件包 `update` 目录，HashAlgm 均为 SHA-256）：
>
> | ImgID | ImgType | 对应镜像文件（Img File Path 末段） |
> |---|---|---|
> | TIMH | RAW | `sim_falcon_qspina...` |
> | OBMI | RAW | `TLoader_QSPINA...` |
> | ARBI | RAW | `ARBEL.bin` |
> | GRBI | RAW | `MSA.bin` |
> | RFBI | RAW | `RPPLUGIN.bin` |
> | OSLO | RAW | `u-boot.bin` |
> | ZIMG | RAW | `zImage` |
> | SYSJ | RAW | `root.squashfs` |
> | TZSI | RAW | `tos.bin` |
> | OEMD | RAW | `oem_data.ubi` |
> | TIM1 | DTIM | `DTim.Primary` |
> | TIM4 | DTIM | `DTim.PPsetting` |
> | TIM2 | DTIM | `DTim.Recovery` |
> | ERAS | ERASO... | `NULL`（擦除项） |
>
> （上述镜像在 P. 列分属不同分区组 0/2、0/3 等，对应主分区与备份分区；`Flash Address`/`Load Addr` 列给出各镜像烧录地址。）

**步骤 3**：点击 SWDownloader.exe 工具界面左上角的**绿灯按钮**，即可进入待烧录模式。此时，工具左上角的左边灯变为灰色，右边灯变为红色，准备烧录镜像文件。如需切换烧录模式，只需点击红灯，工具将恢复到默认状态。重新进入烧录模式，需再次点击右上角左边的绿灯，进入烧录模式。

**步骤 4**：SWDownloader.exe 工具系统进入待烧录状态后，可使用 USB 线连接模块与 PC，并**重新上电或者重启模块**（即按下 EVB 板 **reset** 或者 **start** 按键）即可进行固件烧录。也可将 **USB BOOT 短接到 1.8 V** 使模块进入**强制下载模式**，再将模块重新上电，此时模块与 SWDownloader.exe 工具建立数据连接，即可进行固件烧录。

> **图 4：待烧录状态** —— Debug Output 显示 `Start to prepare images for download` → `Complete target preparation for download successfully` → 提示 **`Perform Target reset to start processing...`**（执行模块复位以开始烧录）。

**步骤 5**：镜像文件烧录结束后，将弹出提示信息，左上角的红灯将变灰，表示固件烧录完成，如图 5 所示。

> **图 5：固件烧录完成** —— 弹窗 `SWDownloader：Elapsed time: 00:00:51`（耗时示例约 51 秒），Debug 日志显示 `Download data finished` / `Completed Download file` / `Begin Burning flash` / `Warning flash Successfully` / `Finish disconnect` / `Status: PASS`，提示 *Setup settings and press Start button*。点击"确定"完成。

---

## 3 附录 参考文档及术语缩写

**表 1：参考文档**

| 文档名称 |
|---|
| [1] Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

**表 2：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| EVB | Evaluation Board | 评估板 |
| IoT | Internet of Things | 物联网 |
| PC | Personal Computer | 个人电脑 |
| USB | Universal Serial Bus | 通用串行总线 |

---

## 关键要点速记（烧录备忘）

- **工具**：`SWDownloader.exe`（示例版本 4.9.1.3），需向移远技术支持获取。
- **驱动**：先装 `Quectel_Windows_USB_Driver(A)_Customer`（双击 `setup.exe`）。
- **烧录文件**：固件包 `update/` 目录下的 **`update.blf`**（描述文件，关联一组镜像：u-boot、zImage、root.squashfs、oem_data.ubi、各 DTim 等，HashAlgm 为 SHA-256）。
- **流程**：File→Open 选 `update.blf` → 点左上角**绿灯**进入待烧录（左灯灰、右灯红）→ USB 连接并 **reset/start 重启模块**（或 USB BOOT 接 1.8V 进强制下载模式后上电）→ 自动烧录 → 红灯变灰 + 弹窗 `Elapsed time` + `Status: PASS` 即完成。
- **强制下载模式**：USB BOOT 短接 1.8V 后重新上电。
- 完成标志：弹窗提示 + 左上角红灯变灰 + 日志 `Finish disconnect` / `PASS`。
