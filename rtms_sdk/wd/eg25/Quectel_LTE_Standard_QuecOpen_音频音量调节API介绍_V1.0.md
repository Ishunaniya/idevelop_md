# LTE Standard QuecOpen 音频音量调节 API 介绍

---

## 文档信息

| 项目 | 内容 |
|------|------|
| 文档标题 | LTE Standard QuecOpen 音频音量调节 API 介绍 |
| 模块系列 | LTE Standard 模块系列 |
| 版本 | LTE_Standard_QuecOpen_音频音量调节_API_介绍_V1.0 |
| 日期 | 2020-04-15 |
| 状态 | 受控文件 |
| 版权 | Copyright © Quectel Wireless Solutions Co., Ltd. 2020 |

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|------|------|------|----------|
| 1.0 | 2020-04-15 | 高飞虎 | 初始版本 |

---

## 目录

1. 引言
   - 1.1 适用模块
2. 音频音量调节 API 介绍
   - 2.1 调节模块播音音量
     - 2.1.1 函数
     - 2.1.2 例程
   - 2.2 调节模块录音音量
     - 2.2.1 函数
     - 2.2.2 例程
   - 2.3 调节模块通话音量等级
     - 2.3.1 函数
     - 2.3.2 例程
   - 2.4 调节模块通话上行音量
     - 2.4.1 函数
     - 2.4.2 例程
   - 2.5 调节模块通话下行音量
     - 2.5.1 函数
     - 2.5.2 例程
   - 2.6 调节侧音音量
     - 2.6.1 函数
     - 2.6.2 例程
3. 附录 A 术语缩写

---

## 表格索引

| 表格编号 | 表格名称 | 页码 |
|----------|----------|------|
| 表 1 | 适用模块 | 5 |
| 表 2 | 术语缩写 | 9 |

---

## 1 引言

移远通信 LTE Standard 模块支持 QuecOpen® 方案，本文档主要介绍了 LTE Standard QuecOpen 模块的音频音量调节相关的 API。通过此文档的帮助，客户可以快速应用相关功能。

### 1.1 适用模块

**表 1：适用模块**

| 模块系列 | 模块 |
|----------|------|
| EC2x 系列 QuecOpen | EC25 系列 QuecOpen |
| EC2x 系列 QuecOpen | EC21 系列 QuecOpen |
| EC2x 系列 QuecOpen | EC20 R2.1 QuecOpen |
| EC2x 系列 QuecOpen | EC20-CN QuecOpen |
| EG9x 系列 QuecOpen | EG95 系列 QuecOpen |
| EG9x 系列 QuecOpen | EG91 系列 QuecOpen |
| EG25-G QuecOpen | EG25-G QuecOpen |

---

## 2 音频音量调节 API 介绍

本章介绍 LTE Standard QuecOpen 模块音频音量调节的 6 类 API，每类 API 均包含一个读取函数（用于获取当前音量值）和一个写入函数（用于设置音量值），并配有对应的例程文件。

### 2.1 调节模块播音音量

#### 2.1.1 函数

| 类型 | 函数名 | 功能说明 |
|------|--------|----------|
| 读函数 | `ql_audplay_gain_read` | 读取模块播音音量 |
| 写函数 | `ql_audplay_gain_write` | 设置模块播音音量 |

**函数说明：**

- **`ql_audplay_gain_read`**：读取当前模块的播音（播放）音量增益值。
- **`ql_audplay_gain_write`**：写入并设置模块的播音（播放）音量增益值。

#### 2.1.2 例程

| 项目 | 内容 |
|------|------|
| 例程文件 | `example_audplay_gain.c` |

**例程说明：**

`example_audplay_gain.c` 演示了如何调用 `ql_audplay_gain_read` 读取当前播音音量，以及如何调用 `ql_audplay_gain_write` 设置新的播音音量。

---

### 2.2 调节模块录音音量

#### 2.2.1 函数

| 类型 | 函数名 | 功能说明 |
|------|--------|----------|
| 读函数 | `ql_audrd_gain_read` | 读取模块录音音量 |
| 写函数 | `ql_audrd_gain_write` | 设置模块录音音量 |

**函数说明：**

- **`ql_audrd_gain_read`**：读取当前模块的录音（录制）音量增益值。
- **`ql_audrd_gain_write`**：写入并设置模块的录音（录制）音量增益值。

#### 2.2.2 例程

| 项目 | 内容 |
|------|------|
| 例程文件 | `example_audrd_gain.c` |

**例程说明：**

`example_audrd_gain.c` 演示了如何调用 `ql_audrd_gain_read` 读取当前录音音量，以及如何调用 `ql_audrd_gain_write` 设置新的录音音量。

---

### 2.3 调节模块通话音量等级

#### 2.3.1 函数

| 类型 | 函数名 | 功能说明 |
|------|--------|----------|
| 读函数 | `ql_clvl_read` | 读取模块音量等级 |
| 写函数 | `ql_clvl_write` | 设置模块音量等级 |

**函数说明：**

- **`ql_clvl_read`**：读取当前模块的通话音量等级（对应 AT 命令 `AT+CLVL` 的功能）。
- **`ql_clvl_write`**：写入并设置模块的通话音量等级。

#### 2.3.2 例程

| 项目 | 内容 |
|------|------|
| 例程文件 | `example_clvl.c` |

**例程说明：**

`example_clvl.c` 演示了如何调用 `ql_clvl_read` 读取当前通话音量等级，以及如何调用 `ql_clvl_write` 设置新的通话音量等级。

---

### 2.4 调节模块通话上行音量

#### 2.4.1 函数

| 类型 | 函数名 | 功能说明 |
|------|--------|----------|
| 读函数 | `ql_mic_gain_read` | 读取模块通话上行音量 |
| 写函数 | `ql_mic_gain_write` | 设置模块通话上行音量 |

**函数说明：**

- **`ql_mic_gain_read`**：读取当前模块的通话上行音量增益（麦克风增益），即从本端麦克风采集后发送给对端的音频音量。
- **`ql_mic_gain_write`**：写入并设置模块的通话上行音量增益。

#### 2.4.2 例程

| 项目 | 内容 |
|------|------|
| 例程文件 | `example_qmic.c` |

**例程说明：**

`example_qmic.c` 演示了如何调用 `ql_mic_gain_read` 读取当前通话上行（麦克风）音量，以及如何调用 `ql_mic_gain_write` 设置新的通话上行音量。

---

### 2.5 调节模块通话下行音量

#### 2.5.1 函数

| 类型 | 函数名 | 功能说明 |
|------|--------|----------|
| 读函数 | `ql_spk_gain_read` | 读取模块通话下行音量 |
| 写函数 | `ql_spk_gain_write` | 设置模块通话下行音量 |

**函数说明：**

- **`ql_spk_gain_read`**：读取当前模块的通话下行音量增益（扬声器增益），即从对端接收并播放给本端用户的音频音量。
- **`ql_spk_gain_write`**：写入并设置模块的通话下行音量增益。

#### 2.5.2 例程

| 项目 | 内容 |
|------|------|
| 例程文件 | `example_qspk.c` |

**例程说明：**

`example_qspk.c` 演示了如何调用 `ql_spk_gain_read` 读取当前通话下行（扬声器）音量，以及如何调用 `ql_spk_gain_write` 设置新的通话下行音量。

---

### 2.6 调节侧音音量

#### 2.6.1 函数

| 类型 | 函数名 | 功能说明 |
|------|--------|----------|
| 读函数 | `ql_sidet_read` | 读取模块侧音音量 |
| 写函数 | `ql_sidet_write` | 设置模块侧音音量 |

**函数说明：**

- **侧音（Sidetone）**：通话过程中，本端说话时能听到自己声音的一种反馈效果。适当的侧音可提升通话自然感，过强则会产生回声干扰。
- **`ql_sidet_read`**：读取当前模块的侧音音量增益值。
- **`ql_sidet_write`**：写入并设置模块的侧音音量增益值。

#### 2.6.2 例程

| 项目 | 内容 |
|------|------|
| 例程文件 | `example_sidet.c` |

**例程说明：**

`example_sidet.c` 演示了如何调用 `ql_sidet_read` 读取当前侧音音量，以及如何调用 `ql_sidet_write` 设置新的侧音音量。

---

## API 汇总表

| 功能类别 | 读函数 | 写函数 | 例程文件 |
|----------|--------|--------|----------|
| 播音音量 | `ql_audplay_gain_read` | `ql_audplay_gain_write` | `example_audplay_gain.c` |
| 录音音量 | `ql_audrd_gain_read` | `ql_audrd_gain_write` | `example_audrd_gain.c` |
| 通话音量等级 | `ql_clvl_read` | `ql_clvl_write` | `example_clvl.c` |
| 通话上行音量 | `ql_mic_gain_read` | `ql_mic_gain_write` | `example_qmic.c` |
| 通话下行音量 | `ql_spk_gain_read` | `ql_spk_gain_write` | `example_qspk.c` |
| 侧音音量 | `ql_sidet_read` | `ql_sidet_write` | `example_sidet.c` |

---

## 3 附录 A 术语缩写

**表 2：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|------|----------|----------|
| API | Application Program Interface | 应用程序接口 |
| AT | Attention | AT 命令（调制解调器控制命令） |
| CPU | Central Processing Unit | 中央处理器 |
| IoT | Internet of Things | 物联网 |
| LTE | Long Term Evolution | 长期演进 |
| MIC | Microphone | 麦克风 |
| PCM | Pulse Code Modulation | 脉冲编码调制 |
| QuecOpen | Quectel Open Platform | 移远通信开放平台 |
| SDK | Software Development Kit | 软件开发工具包 |
| SPK | Speaker | 扬声器 |
| UAC | USB Audio Class | USB 音频类 |

---

*本文档版权属于上海移远通信技术股份有限公司，任何人未经我司允许而复制转载该文档将承担法律责任。*
