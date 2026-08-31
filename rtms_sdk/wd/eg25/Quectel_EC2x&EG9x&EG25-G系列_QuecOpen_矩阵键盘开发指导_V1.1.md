# EC2x&EG9x&EG25-G QuecOpen — 矩阵键盘开发指导

> **模块系列：** LTE Standard 模块系列
> **版本：** EC2x&EG9x&EG25-G_QuecOpen_矩阵键盘开发指导_V1.1
> **日期：** 2020-05-22
> **状态：** 受控文件
> **版权：** Copyright © Quectel Wireless Solutions Co., Ltd. 2020. 保留一切权利。

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| 1.0 | 2020-05-12 | 高飞虎 / 张洋洋 | 初始版本 |
| 1.1 | 2020-05-22 | 高飞虎 / 张洋洋 | 在 PDF 版侧边栏中添加了 EC2x&EG9x&EG25-G QuecOpen 矩阵键盘附件附件以及更新了编译路径信息（第四章）。 |

---

## 目录

- [1 引言](#1-引言)
  - [1.1 适用模块](#11-适用模块)
- [2 矩阵键盘硬件设计](#2-矩阵键盘硬件设计)
- [3 设备树以及矩阵键盘](#3-设备树以及矩阵键盘)
  - [3.1 设备树增加矩阵键盘配置](#31-设备树增加矩阵键盘配置)
    - [3.1.1 属性说明](#311-属性说明)
    - [3.1.2 功能说明](#312-功能说明)
    - [3.1.3 配置参考](#313-配置参考)
    - [3.1.4 驱动源码和文件路径](#314-驱动源码和文件路径)
  - [3.2 使能矩阵键盘内核选项](#32-使能矩阵键盘内核选项)
- [4 应用程序示例](#4-应用程序示例)
- [5 附录 A 术语缩写](#5-附录-a-术语缩写)

### 表格索引

- 表 1：适用模块
- 表 2：术语缩写

### 图片索引

- 图 1：4 × 4 矩阵键盘
- 图 2：设备树配置
- 图 3：Make Kernel_Menuconfig 配置系统界面

---

## 1 引言

移远通信 LTE Standard 模块支持 QuecOpen® 方案。**独立键盘**指的是一个按键占用单独的一个 I/O 口，其优点是编程简单，缺点是浪费 I/O 口；而**矩阵键盘**通常将按键排列成矩阵形式，每条水平线和垂直线在交叉处不直接连通，而是通过一个按键加以连接，其优点是节省 I/O 口，缺点是编程复杂，因此**超过 6 个以上按键推荐使用矩阵键盘方式**。

为了满足客户对矩阵键盘功能的需要，本文档主要介绍了 EC2x&EG9x&EG25-G QuecOpen 模块的矩阵键盘硬件设计、设备树、矩阵键盘内核选项以及应用程序示例。通过此文档的帮助，客户可以快速应用相关功能。

### 1.1 适用模块

**表 1：适用模块**

| 模块系列 | 模块 |
|---|---|
| EC2x 系列 QuecOpen | EC25 系列 QuecOpen |
| EC2x 系列 QuecOpen | EC21 系列 QuecOpen |
| EC2x 系列 QuecOpen | EC20 R2.1 QuecOpen |
| EC2x 系列 QuecOpen | EC20-CN QuecOpen |
| EG9x 系列 QuecOpen | EG95 系列 QuecOpen |
| EG9x 系列 QuecOpen | EG91 系列 QuecOpen |
| EG25-G QuecOpen | EG25-G QuecOpen |

---

## 2 矩阵键盘硬件设计

常见的矩阵键盘参考设计为 **4 × 4 矩阵键盘**，如下图所示：

> 图 1：4 × 4 矩阵键盘 —— 4 行 × 4 列共 16 个按键组成的矩阵键盘原理图，行线与列线在交叉处通过按键连接。

---

## 3 设备树以及矩阵键盘

### 3.1 设备树增加矩阵键盘配置

#### 3.1.1 属性说明

| 属性 | 说明 |
|---|---|
| `debounce-delay-ms` | 按键防抖时间 |
| `col-scan-delay-us` | 触发按键后列扫描延时 |
| `linux,wakeup` | 支持按键唤醒模块 |
| `row-gpios` | 行使用的 GPIO 编号 |
| `col-gpios` | 列使用的 GPIO 编号 |
| `linux-keymap` | 行列以及上报到 APP 的键码 |

#### 3.1.2 功能说明

| 项目 | 说明 |
|---|---|
| 行作为输入引脚 | 选择支持中断唤醒的 I/O，保持默认内部下拉 |
| 列作为输出引脚 | 驱动在默认配置下输出高电平 |
| 边沿触发 | 上升沿和下降沿都会上报 |

#### 3.1.3 配置参考

| 项目 | 路径 |
|---|---|
| 配置参考 | `ql-ol-kernel/msm-3.18/Documentation/devicetree/bindings/input/gpio-matrix-keypad.txt` |

#### 3.1.4 驱动源码和文件路径

| 项目 | 路径 |
|---|---|
| 驱动源码 | `ql-ol-kernel/msm-3.18/drivers/input/keypad/matrix_keypad.c` |
| 文件路径 | `Ql-ol-kernel/arch/arm/boot/dts/qcom/mdm9607.dtsi` |

请按照下图中的操作把代码加到对应路径。

> 图 2：设备树配置 —— 在 `mdm9607.dtsi` 设备树文件中加入矩阵键盘配置（含 `row-gpios`、`col-gpios`、`linux-keymap`、`debounce-delay-ms`、`col-scan-delay-us`、`linux,wakeup` 等属性）的代码截图。

有关应用程序请参考第 4 章。

### 3.2 使能矩阵键盘内核选项

需要执行以下命令来使能矩阵键盘内核选项：

```bash
ql-ol-sdk$ make kernel_menuconfig
ql-ol-sdk$ make kernel
```

运行 `make kernel_menuconfig` 按照下图提示打开配置系统界面，退出并保存。

> 图 3：Make Kernel_Menuconfig 配置系统界面 —— 内核 menuconfig 配置界面中使能矩阵键盘（Matrix Keypad）相关选项的截图。

---

## 4 应用程序示例

执行 `make kernel` 编译内核，将附于文档侧边栏的附件「EC2x&EG9x&EG25-G QuecOpen 矩阵键盘」里的 `matrix_keyboard` 目录放到路径 `ql-ol-extsdk/example` 下进行编译，然后将可执行程序放到模块里面进行测试；触发按键时可以看到键码上报，休眠情况下也可以上报。

---

## 5 附录 A 术语缩写

**表 2：术语缩写**

| 术语 | 英文全称 | 中文全称 |
|---|---|---|
| APP | Application | 应用程序 |
| GPIO | General-purpose input/output | 通用型输入/输出 |
| I/O | Input/Output | 输入/输出 |
