# Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_固件下载指导_V1.0 分析报告

## 1. 文档基本信息

| 项目 | 内容 |
|------|------|
| 文档标题 | AG35-CET&AG35-EUT QuecOpen(SDK)固件下载指导 |
| 适用产品系列 | LTE Standard 模块系列（AG35-CET、AG35-EUT） |
| 版本 | 1.0 |
| 发布日期 | 2024-11-06 |
| 文档状态 | 受控文件 |
| 发布单位 | 上海移远通信技术股份有限公司（Quectel） |
| 总页数 | 12（PDF 实际 13 页，含封面） |
| 文档历史 | 初稿创建于 2023-06-16（作者 William LIU，无版本号）；1.0 版（2024-11-06，William LIU）为受控版本，变更点：①增加适用模块 AG35-EUT；②基于 QuecOpen 方案统一命名，更新文档名称 |
| 保密声明 | 文档加盖 "Quectel Confidential" 水印，受使用和披露限制（许可协议/版权/商标/第三方权利等标准法务声明） |

## 2. 目录结构概览

```
文档历史 ........................................ 3
目录 ............................................ 4
表格索引 ........................................ 5
图片索引 ........................................ 6
1 引言 .......................................... 7
2 固件下载步骤 ................................... 8
  2.1 安装 USB 驱动 ................................ 8
  2.2 下载固件 ..................................... 9
3 附录 参考文档及术语缩写 ......................... 12
  表1：参考文档 ................................. 12
  表2：术语缩写 ................................. 12
```

图片索引（共5张截图）：
- 图1：USB 驱动安装完成界面（p8）
- 图2：选择 update.blf 文件（p9）
- 图3：待下载文件（p9）
- 图4：待下载状态（p10）
- 图5：固件下载完成（p11）

## 3. 逐章节详细摘要

### 第1章 引言（p7）

- **背景**：AG35-CET 和 AG35-EUT 模块支持 QuecOpen® 方案。QuecOpen® 是移远基于 Linux 的嵌入式开发平台，用于简化 IoV（车联网）应用的软件设计和开发过程。QuecOpen® 详细信息参见参考文档 [1]《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导》。
- **本文档适用范围**：适用于 SDK 构建环境下的 QuecOpen® 方案，介绍如何使用专用的 **SWDownloader.exe** 工具，将指定版本的固件下载至 AG35-CET 和 AG35-EUT 模块。
- **重要备注（原文强调框）**：
  > 请联系移远通信技术支持获取最新版本的 SWDownloader.exe 工具，并确保其版本为 **4.9.2.3 或更高**。随着软件基线升级，SWDownloader.exe 工具也需要相应升级，请确保工具版本的同步更新。
- 工具获取渠道：需联系 Quectel 技术支持获取，非公开下载。

### 第2章 固件下载步骤（p8–p11）

#### 2.1 安装 USB 驱动（p8）

**前提条件**：使用下载工具之前，需在上位机（Windows PC）安装模块的 USB 驱动 —— `Quectel_Windows_USB_Driver(A)_Customer`。

**操作步骤**：
1. **步骤1**：联系移远通信技术支持获取 USB 驱动安装包，并解压。
2. **步骤2**：双击 `setup.exe` 文件，按照提示完成安装。

驱动安装完成后界面如图1所示：InstallShield Wizard 弹出 "Maintenance Complete" 提示，文字为 "InstallShield Wizard has finished performing maintenance operations on Quectel_Windows_USB_Driver(A)_Customer."，点击 "Finish" 即可。

#### 2.2 下载固件（p9–p11）

**前提条件**：USB 驱动已安装完成。

**操作步骤**：
1. **步骤1**：打开 `SWDownloader.exe` 工具（界面标题显示版本号示例为 4.9.2.3）。
2. **步骤2**：选择菜单 "File" → "Open"，在弹出框中选择待下载固件版本包中 `update/` 目录下的 `update.blf` 文件。
   - 文档截图示例路径：`F:\ASR\AG35CETCAR01A01M2G_OCPU\update\update.blf`
   - `update.blf` 文件大小示例：70 KB，文件类型为 "BLF 文件"
3. **步骤3**：点击 `update.blf` 文件后，工具会列出待下载的镜像文件清单（图3），表格列包括：
   - `S`（勾选/Selected）
   - `P`（Partition?，示例值 0）
   - `T`（Type，示例值 1/2/3/5/6/7）
   - `ImgID`（镜像标识，如 TIMH、OBMI、RFBI、GRBI、ARBI、TZSI、OSLO、ZIMG、SYSJ、OEMD、TIM1、TIM4、TIM2、TIM5 等）
   - `Erase...`（擦除范围）
   - `Flash Address`（闪存地址，如 0x0000_0000、0x0002_0000、0x0004_0000、0x003A_0000、0x003E_0000、0x007E_0000、0x0108_0000、0x0118_0000、0x0124_0000、0x01A4_0000、0x02E4_0000、0x0428_0000、0x042C_0000、0x046C_0000、0x04F6_0000、0x0506_0000、0x0512_0000、0x0592_0000、0x06D2_0000、0x0036_0000、0x0038_0000、0x0424_0000、0x0426_0000 等）
   - `Load Addr.`（加载地址，如 0xD100_00.. 、0x004E_80.. 、0xFFFF_FFFF、0x0040_80.. 、0x0200_00.. 等）
   - `HashAlgm`（哈希算法，全部为 **SHA-256**）
   - `Size to Ha...`（待哈希大小，多数为 `0xFFFF_FFFF`）
   - `ImgType`（镜像类型，含 **RAW** 和 **DTIM** 两类）
   - `Img File Path`（镜像文件路径，分两类来源：①工具临时目录 `C:\Users\william.liu\AppData\Local\Temp\SWD14B4.tmp\AbsBlfTemp\...`；②固件包目录 `F:\ASR\AG35CETCAR01A01M2G_OCPU\update\*.bin/.squashfs`，包含 `TLoader_QSPINAND.bin`、`RFPLUGIN.bin`、`MSA.bin`、`ARBEL.bin`、`tos.bin`、`u-boot.bin`、`zImage`、`root.squashfs`、`oemapp.squashfs` 等）
4. **步骤4**：点击工具左上角的**绿灯按钮**，进入待下载模式：
   - 进入后左边灯变灰色，右边灯变红色。
   - 如需切换下载模式，点击红灯，工具恢复默认状态。
   - 如需重新进入下载模式，再次点击左上角绿灯。
   - 图4所示状态下，工具下方日志区显示：
     ```
     [2023/08/06 14:03:30.935] - /Start to prepare images for download/
     [2023/08/06 14:03:30.938] - /SWDownloader 4.9.2.3 - [update.blf]/
     [2023/08/06 14:03:32.39] - /Complete images preparation for download successfully /
     ```
5. **步骤5**：使用 **Micro-USB 线**连接模块与上位机，将模块**重新上电**或**重启**（即按下 EVB 板 **RESET** 或 **PWEKRY** 按键）即可下载固件。
   - **备选方式**：也可通过短接模块 **USB_BOOT** 引脚至 **1.8 V**，使模块进入**强制下载模式**，再将模块重新上电；此时模块与 SWDownloader.exe 建立数据连接，即可下载固件。
6. **步骤6**：镜像文件下载结束后弹出提示信息（图5示例为弹窗 "Elapsed time: 00:00:25"），点击 "确定" 后，左上角的**红灯将变灰**，表示固件下载完成。
   - 图5下方 Debugging 日志区显示了详细的下载过程日志（节选）：
     ```
     [...] Device: 1, Target Debug Message: \Downloading file: ...\
     [...] Device: 1, Target Debug Message: \FastDownload\
     [...] Device: 1, Target Debug Message: \Download data finished\
     [...] Device: 1, Target Debug Message: \Completed Download file: ...\0x0510..._FSP_h.bin\
     [...] Device: 1, Target Debug Message: \Platform is busy \
     [...] Device: 1, Target Debug Message: \Platform is Ready \
     [...] Device: 1, Target Debug Message: \Warning Flash Successfully \
     [...] Device: 1, Target Debug Message: \Begin to disconnect \
     [...] Device: 1, Target Debug Message: \Finish disconnect \
     [...] Device: 1, Logged processing: Elapsed time: 00:00:25.179, Status: PASS
     ```

**补充备注（p11）**：
> 模块需通过 **LTE OPEN EVB** 实现与上位机的连接，有关 EVB 详情可参考文档 [2]《Quectel_LTE_OPEN_EVB_User_Guide》。

### 第3章 附录：参考文档及术语缩写（p12）

**表1：参考文档**
| 编号 | 文档名称 |
|------|----------|
| [1] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |
| [2] | Quectel_LTE_OPEN_EVB_User_Guide |

**表2：术语缩写**
| 缩写 | 英文全称 | 中文全称 |
|------|----------|----------|
| IoV | Internet of Vehicles | 车联网 |
| EVB | Evaluation Board | 评估板 |
| USB | Universal Serial Bus | 通用串行总线 |

## 4. 关键命令/参数/接口汇总表

> 本文档为固件下载（GUI工具操作）指导，**未涉及任何 AT 命令**，全部操作均通过 SWDownloader.exe 图形界面完成。以下为文档中出现的关键工具参数/标识汇总：

| 类别 | 名称/值 | 说明 |
|------|---------|------|
| 工具名称 | SWDownloader.exe | 固件下载专用工具，版本要求 ≥ 4.9.2.3 |
| USB 驱动包名 | Quectel_Windows_USB_Driver(A)_Customer | 上位机需安装的模块 USB 驱动 |
| 索引文件 | update.blf | 固件包 `update/` 目录下的索引/工程文件，记录待下载镜像清单 |
| 镜像类型(ImgType) | RAW / DTIM | RAW=原始分区镜像；DTIM=设备树/Trim类镜像（文档未展开解释） |
| 哈希算法 | SHA-256 | 镜像校验算法（表格列固定值） |
| 关键镜像文件名 | TLoader_QSPINAND.bin, RFPLUGIN.bin, MSA.bin, ARBEL.bin, tos.bin, u-boot.bin, zImage, root.squashfs, oemapp.squashfs | 固件包内具体分区镜像文件（ASR平台典型分区） |
| 硬件接口 | USB_BOOT 引脚 | 短接至 1.8V 可强制进入下载模式 |
| 硬件接口 | Micro-USB | 模块与上位机的物理连接接口 |
| EVB 按键 | RESET / PWEKRY | 用于触发模块重启进入下载流程 |
| 控制端口 | 无（非网络/TCP端口，为本地 USB 枚举设备） | — |

## 5. 完整操作流程还原

```
准备阶段：
  1. 联系 Quectel 技术支持获取：
     a. SWDownloader.exe 工具（版本 ≥ 4.9.2.3）
     b. Quectel_Windows_USB_Driver(A)_Customer 驱动安装包
     c. 目标固件版本包（含 update/update.blf 及各分区镜像文件）

第一步：安装 USB 驱动
  解压驱动包 → 双击 setup.exe → 按提示完成安装 → 确认 "Maintenance Complete" 提示

第二步：下载固件
  1. 打开 SWDownloader.exe
  2. File → Open → 选择固件包 update/ 目录下的 update.blf
  3. 工具自动加载并列出待下载镜像清单（校验 SHA-256、Flash Address 等）
  4. 点击左上角绿灯，进入待下载模式（左灯变灰，右灯变红）
  5. Micro-USB 连接模块与PC →
     方式A：模块重新上电 / 按 EVB RESET 或 PWEKRY 重启
     方式B：短接 USB_BOOT 引脚至 1.8V → 强制下载模式 → 重新上电
  6. 模块与工具建立连接，自动开始下载（日志区可见 FastDownload/Download data finished等过程）
  7. 下载完成后弹出耗时提示（如 00:00:25）→ 点击"确定"
  8. 红灯变灰 → 表示固件下载完成（日志显示 Status: PASS）
```

## 6. 与本项目（open_dial）的关联点

本项目 `open_dial` 是运行在 **Quectel EC2x/EG2x** 模组（OpenNPC, ARMv7 平台）上的拨号管理用户态程序，通过 AT 命令与 QMI/MCM API 与模块交互，管理 SIM/Roamlink 双通道数据连接。本 PDF 是 **AG35-CET/AG35-EUT**（不同模组型号，ASR 平台）的 **生产/开发阶段固件刷写工具指导**，两者关联性分析如下：

- **模组型号不同**：文档面向 AG35-CET/AG35-EUT（ASR 平台，文件路径含 `ASR\AG35CETCAR01A01M2G_OCPU`），而本项目 CLAUDE.md 描述的运行平台是 EC2x/EG2x（高通/Qualcomm 平台，QMI/MCM API，`profile_idx=1` 用于"Qualcomm 自动路由"）。两者属于移远不同芯片平台的模块系列，**固件下载工具及流程不可直接复用**。
- **无代码层面关联**：本文档全篇为 Windows 上位机 GUI 工具操作指导，不涉及 AT 命令、QMI/MCM API、Linux 用户态程序逻辑，因此与 `open_dial` 的 `src/at/`、`src/nw/`、`src/dial/` 等模块的实现代码**没有直接交集**。
- **间接潜在关联（仅供参考）**：
  - 若后续 `open_dial` 项目需要支持或迁移到 ASR 平台模块（如 AG35 系列），则本文档的固件刷写流程（USB驱动安装 + SWDownloader.exe + update.blf）可作为产线/工厂固件刷写参考，但与 `dial` 程序本身运行时逻辑无关。
  - 文档中提到的 QuecOpen® SDK 概念与本项目"OpenNPC SDK"的开发模式类似（均为移远基于 Linux 的嵌入式开发框架），但具体 SDK 版本/工具链不同，CLAUDE.md 中 `open_dial` 的构建依赖是 OpenNPC SDK 的 `environment-setup-*`，与本文档无引用关系。
- **结论**：本文档与 `open_dial` 项目当前代码、架构、运行环境**均无直接技术关联**；仅在"移远模块固件管理"这一宽泛主题上同属同一厂商体系。

## 7. 文档自身局限性

1. **篇幅极简**：正文仅 2 章（引言 + 操作步骤）+ 1 个附录，核心内容约 4 页，是一份高度聚焦的"快速操作指导"，不是完整的工具手册或技术参考。
2. **未解释的技术细节**：
   - 未说明 ImgID 各值（TIMH、OBMI、RFBI、GRBI、ARBI、TZSI、OSLO、ZIMG、SYSJ、OEMD、TIM1/2/4/5）的具体含义。
   - 未说明 `T`（Type）列数值（1/2/3/5/6/7）的分类逻辑。
   - 未说明 `ImgType` 中 RAW 与 DTIM 的区别及适用场景。
   - 未说明 Flash Address / Load Addr. 与具体分区表的对应关系。
3. **工具获取渠道封闭**：SWDownloader.exe 与 USB 驱动均需"联系移远通信技术支持获取"，文档本身不提供下载链接或获取方式细节，亦未说明工具的具体使用环境（操作系统版本、依赖库等），只能假定为 Windows 平台。
4. **未涉及故障排查**：文档未提供任何故障排除（Troubleshooting）章节，例如下载失败、驱动识别失败、连接超时等异常情况的处理方法均未提及。
5. **未涉及命令行/脚本化下载**：仅描述了 GUI 交互式下载流程，未提及是否存在命令行版本或可自动化的批量下载方案。
6. **未说明固件包的获取渠道及版本选择依据**：文档未说明固件版本包（如 `AG35CETCAR01A01M2G_OCPU`）从何处获取，也未说明不同固件版本的差异或选择标准。
7. **截图均为早期日期**：文档截图时间戳显示为 2023年6-8月（如 "2023/08/06"），但文档版本日期为 2024-11-06，说明截图未随版本更新而重新采集，截图中的版本路径/示例数据可能与当前实际有差异。
8. **参考文档为外部受限文档**：附录引用的 [1]、[2] 两份参考文档均未在本 PDF 中提供，需另行获取，本分析未能展开核实其内容。
9. **未提及安全性/权限要求**：未说明刷写固件操作是否需要特殊权限、是否有防误刷保护机制、刷写失败后的恢复方案等。

---

文档内容已全部基于实际读取的 13 页 PDF（含封面）逐页核实完成，未提及的内容已在第7节中明确列出，不做任何臆测或编造。

<!-- GENERATION_COMPLETE: 2026-06-24 -->
