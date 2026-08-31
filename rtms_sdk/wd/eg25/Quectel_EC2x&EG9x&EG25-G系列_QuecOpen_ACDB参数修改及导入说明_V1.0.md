# EC2x&EG9x&EG25-G 系列 QuecOpen ACDB 参数修改及导入说明

**LTE Standard 模块系列**

- 版本：EC2x&EG9x&EG25-G 系列_QuecOpen_ACDB 参数修改及导入说明_V1.0
- 日期：2020-07-30
- 状态：受控文件

---

## 公司及联系方式

上海移远通信技术股份有限公司始终以为客户提供最及时、最全面的服务为宗旨。如需任何帮助，请随时联系我司上海总部，联系方式如下：

**上海移远通信技术股份有限公司**
上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼　　邮编：200233
电话：+86 21 51086236　邮箱：info@quectel.com

或联系我司当地办事处，详情请登录：http://www.quectel.com/cn/support/sales.htm 。

如需技术支持或反馈我司技术文档中的问题，可随时登陆如下网址：
http://www.quectel.com/cn/support/technical.htm 或发送邮件至：support@quectel.com 。

### 前言

上海移远通信技术股份有限公司提供该文档内容用以支持其客户的产品设计。客户须按照文档中提供的规范、参数来设计其产品。因未能遵守有关操作或设计规范而造成的损害，上海移远通信技术股份有限公司不承担任何责任。在未声明前，上海移远通信技术股份有限公司有权对该文档进行更新。

### 免责声明

上海移远通信技术股份有限公司尽力确保开发中功能的完整性、准确性、及时性或效用，但不排除上述功能错误或遗漏的可能。除非其他有效协议另有规定，否则上海移远通信技术股份有限公司对开发中功能的使用不做任何暗示或明示的保证。在适用法律允许的最大范围内，上海移远通信技术股份有限公司不对任何因使用开发中功能而遭受的损失或损害承担责任，无论此类损失或损害是否可以预见。

### 版权申明

本文档版权属于上海移远通信技术股份有限公司，任何人未经我司允许而复制转载该文档将承担法律责任。

版权所有 ©上海移远通信技术股份有限公司 2020，保留一切权利。
Copyright © Quectel Wireless Solutions Co., Ltd. 2020.

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| 1.0 | 2020-07-30 | 权良民 / 崔德兵 | 初始版本 |

---

## 目录

- 文档历史
- 目录
- 表格索引
- 图片索引
- 1 引言
  - 1.1. 适用模块
- 2 ACDB 及 QACT 概览
  - 2.1. ACDB
  - 2.2. QACT
- 3 修改并保存 ACDB 文件
  - 3.1. 离线校准模式
    - 3.1.1. 导出 ACDB 文件至本地
    - 3.1.2. 修改并保存本地 ACDB 文件
  - 3.2. 在线校准模式
    - 3.2.1. Linux 端内存中修改 ACDB 参数
    - 3.2.2. DSP 模式校准模块
- 4 导入 ACDB 文件
  - 4.1. 解压 SDK 并配置编译环境
  - 4.2. 拷贝 ACDB 文件至文件系统
  - 4.3. 编译文件系统
- 5 附录 A 参考文档及术语缩写

### 表格索引

- 表 1：适用模块
- 表 2：参考文档
- 表 3：术语缩写

### 图片索引

- 图 1：ACDB 文件
- 图 2：QACT 在线连接
- 图 3：保存 ACDB 文件
- 图 4：离线打开 ACDB 文件
- 图 5：Audio Use Case
- 图 6：Device Use Case
- 图 7：HANDSET_SPKR
- 图 8：设置 CODEC_GAIN
- 图 9：设置 CODEC_GAIN
- 图 10：保存 ACDB 文件
- 图 11：进入 DSP 校准模式
- 图 12：调整 ACDB 参数位置
- 图 13：解压 SDK
- 图 14：配置编译环境
- 图 15：ACDB 文件
- 图 16：修改权限
- 图 17：编译文件系统
- 图 18：文件系统文件

---

## 1 引言

移远通信 LTE Standard EC2x&EG9x&EG25-G 系列模块支持 QuecOpen® 方案。本文档主要介绍在 QuecOpen® 方案下，如何修改、保存 ACDB（Audio Calibration Database，即音频校准数据库）参数，并通过 SDK 包将修改后的 ACDB 文件导入至模块文件系统。

### 1.1. 适用模块

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

## 2 ACDB 及 QACT 概览

### 2.1. ACDB

ACDB 用于调整 ADSP 音频参数。目前可供使用的 ACDB 文件共 7 个，如下图所示；图中的 `workspaceFile.qwsp` 是工程文件，供 QACT 工具打开 ACDB 文件使用。

> **图 1：ACDB 文件** —— 截图展示目录中共 7 个 `.acdb` 文件，以及一个 `workspaceFile.qwsp` 工程文件。

### 2.2. QACT

QACT 是由 Qualcomm 提供的一款音频参数调整工具，用于校准 ACDB 参数。

> **备注**
>
> ACDB 文件在 `alsaucm_test` 进程中加载到内存。若未启用该进程，则 QACT 工具无法支持在线校准模式。

---

## 3 修改并保存 ACDB 文件

QACT 支持离线校准模式和在线校准模式。离线校准模式，即导出 ACDB 文件至本地后，使用 QACT 打开文件，离线修改并保存 ACDB 参数。在线校准模式支持两种方式调整 ACDB 参数：一种是在 DSP 模式下校准模块，可实时调整 ACDB 参数（目前仅在语音通话下可用）；另一种是修改 Linux 端内存中 ACDB 参数，调整参数后需使用 `AT+QAUDMOD` 命令切换至相应的音频模式，参数修改才会生效。在线校准模式下，由于模块重启后，参数配置不生效，因此，修改的 ACDB 参数需要保存。本章节主要介绍在离线校准模式和在线校准模式下如何修改并保存 ACDB 参数。有关 `AT+QAUDMOD` 命令详细信息，请参考文档 [1]。

### 3.1. 离线校准模式

#### 3.1.1. 导出 ACDB 文件至本地

离线校准模式下，ACDB 文件在模块内，需将 ACDB 文件从模块导出并保存至本地。步骤如下：

1. 启动设备，打开 QPST，加载 DM 口，再打开 QACT 工具，点击“Connect To Device”连接到设备，如下图所示。

   > **图 2：QACT 在线连接** —— QACT 工具界面，点击“Connect To Device”按钮连接设备。

2. 连接到设备后，点击左上角的“另存为”按钮，将出现保存 ACDB 文件的界面，如下图所示。选择保存路径后，点击“OK”即可将 ACDB 文件及工程文件保存到本地。

   > **图 3：保存 ACDB 文件** —— 弹出保存 ACDB 文件路径选择界面，选择路径后点击“OK”。

#### 3.1.2. 修改并保存本地 ACDB 文件

ACDB 文件从模块导出并保存至本地后，需修改并保存本地的 ACDB 文件。具体步骤如下：

1. 重新打开 QACT 工具，选择离线校准模式，并选择本地的 `workspaceFile.qwsp` 文件，如下图所示。

   > **图 4：离线打开 ACDB 文件** —— QACT 选择离线校准模式并打开本地 `workspaceFile.qwsp` 工程文件。

2. QACT 工具左上角的“Audio use case”下有 3 个选项：Audio Recording、Audio Playback 和 Voice，如下图 5 所示。例如，对讲机主要使用的是 Audio Recording 和 Audio Playback。下图 6 为与“Audio use case”对应的“Device use case”下的选项，根据当前的音频模式（即通过 `AT+QAUDMOD` 设置）选择相应的“Device use case”。以 Audio Playback 为例，若 `AT+QAUDMOD` 设置为 0，“Device use case”则应选择“HANDSET_SPKR”，如下图 7 所示。

   > **图 5：Audio Use Case** —— “Audio use case”下拉列表，含 Audio Recording、Audio Playback、Voice 三个选项。
   >
   > **图 6：Device Use Case** —— 与所选 Audio use case 对应的“Device use case”选项列表。
   >
   > **图 7：HANDSET_SPKR** —— 当 `AT+QAUDMOD=0` 时在 Device use case 中选择 HANDSET_SPKR。

3. 修改相应模式下各个模块参数。以 HANDSET_SPKR 下的 CODEC_GAIN 为例，可通过两种方式修改该模块的 ACDB 参数值：一是在下图 8 所示的下拉框中直接修改 CODEC_GAIN 值；二是点击“Connect To Device”连接到设备，进入界面，然后双击对应的模块，在下图 9 所示红框中修改 CODEC_GAIN 值，然后点击“Set to ACDB”。修改完成后如下图 10 所示点击左上角的“保存”或“另存为”按钮保存 ACDB 文件。

   > **图 8：设置 CODEC_GAIN** —— 方式一：在下拉框中直接修改 CODEC_GAIN 值。
   >
   > **图 9：设置 CODEC_GAIN** —— 方式二：连接设备后双击模块，在红框中修改 CODEC_GAIN 值，再点击“Set to ACDB”。
   >
   > **图 10：保存 ACDB 文件** —— 点击左上角“保存”或“另存为”按钮保存 ACDB 文件。

### 3.2. 在线校准模式

#### 3.2.1. Linux 端内存中修改 ACDB 参数

在线校准模式下，直接修改模块内的 ACDB 参数，然后保存到本地。步骤如下：

1. 启动设备，打开 QPST，加载 DM 口，再打开 QACT 工具，点击“Connect To Device”连接设备。
2. 重复第 3.1.2 章的第 2 步和第 3 步。
3. 修改完成后如上图 10 所示点击 QACT 左上角的“另存为”按钮，保存 ACDB 文件到本地。

#### 3.2.2. DSP 模式校准模块

点击下图中“DSP Calibration”，进入 DSP 校准模式，即可实时调整 ACDB 参数。

> **图 11：进入 DSP 校准模式** —— 点击“DSP Calibration”进入 DSP 校准模式。

使用 DSP 校准模式时，需要注意如下几点：

1. 在 DPS 模式下调整 ACDB 参数，会使当前的通话效果立即发生相应变化，调试到一个合适的值之后，记录下该参数值。
2. 由于在 DPS 模式下调试的参数不会保存，通话挂断后参数立即丢失，再次拨通电话时将恢复默认值，因此需要使用“Batch Copy To ACDB”功能将调试的参数复制到对应的 ACDB 文件，如下图所示。

   > **图 12：调整 ACDB 参数位置** —— 使用“Batch Copy To ACDB”功能将调试参数复制到对应 ACDB 文件的位置。

3. 只有在通话状态下，进入 DSP 模式才能看到加载的 ACDB 信息，在非通话状态下则无法查看任何相关信息。

---

## 4 导入 ACDB 文件

本章节介绍了如何将 ACDB 文件导入至模块的文件系统。

### 4.1. 解压 SDK 并配置编译环境

1. 将 SDK 包拷贝到 Linux 端执行如下命令进行解压：

   ```bash
   sudo tar -jxvf EC20CETFDKR05A03V01M2G_OCPU_DJJ_SDK.tar.bz2
   ```

   如下图所示：

   > **图 13：解压 SDK** —— 执行解压命令后的终端输出。

2. 解压后进入 `/ql-ol-sdk` 目录，执行如下命令配置编译环境：

   ```bash
   source ql-ol-crosstool/ql-ol-crosstool-env-init
   ```

   如下图所示：

   > **图 14：配置编译环境** —— 执行 source 命令配置编译环境后的终端输出。

### 4.2. 拷贝 ACDB 文件至文件系统

ACDB 文件在模块文件系统的 `/etc` 目录下，如下图所示：

> **图 15：ACDB 文件** —— `/etc` 目录下的 ACDB 文件列表。

将 `/sdk/ql-ol-sdk/ql-ol-rootfs/etc` 路径下的 ACDB 文件全部删除，将修改完成的 ACDB 文件拷贝到此路径下（不拷贝 `workspaceFile.qwsp` 文件，该文件为工程文件，模块无需使用），执行如下命令修改权限：

```bash
sudo chown -R xxx:xxx *.acdb
sudo chmod 777 ./*.acdb
```

如下图所示：

> **图 16：修改权限** —— 执行 chown / chmod 命令修改 ACDB 文件权限后的终端输出。

### 4.3. 编译文件系统

在 `/ql-ol-sdk` 目录下执行如下命令编译文件系统：

```bash
make rootfs
```

如下图所示：

> **图 17：编译文件系统** —— 执行 `make rootfs` 编译文件系统的终端输出。

编译完成后将生成 target 文件夹，新的文件系统文件位于该文件夹下，如下图所示：

> **图 18：文件系统文件** —— target 文件夹下生成的新文件系统文件（含 `mdm9607-perf-sysfs.ubi`）。

将 `mdm9607-perf-sysfs.ubi` 文件烧录到 system 分区，烧录完成后重启模块，新的 ACDB 文件将被导入至模块的文件系统。

---

## 5 附录 A 参考文档及术语缩写

**表 2：参考文档**

| 序号 | 文件名称 | 备注 |
|---|---|---|
| [1] | Quectel_EC2x&EG9x&EG2xG&EM05_Series_AT_Commands_Manual | EC2x&EG9x&EG2xG&EM05 AT 命令手册 |

**表 3：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| ACDB | Audio Calibration Database | 音频校准数据库 |
| ADSP | Advanced Digital Signal Processor | 高级数字信号处理 |
| DM | Device Manager | 设备管理 |
| DSP | Digital Signal Processor | 数字信号处理 |
| RTC | Real Time Clock | 实时时钟 |
| SDK | Software Development Kit | 软件开发工具包 |
| QACT | Qualcomm Audio Calibration Tool | 高通音频校准工具 |
| QPST | Qualcomm Product Support Tool | 高通产品支持工具 |
