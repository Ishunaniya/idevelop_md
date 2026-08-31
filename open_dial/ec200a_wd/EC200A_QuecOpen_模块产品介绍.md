# EC200A-CN(TA) QuecOpen LTE Standard 模块 — 产品介绍

> **原文档**：`Quectel_EC200A-CN(TA)_LTE_Standard_QuecOpen_模块产品介绍_V1.0.0_Preliminary_20210810.pdf`
> **版本**：1.0.0 ｜ **状态**：受控文件 ｜ **制作时间**：2021 年 3 月 ｜ **文件日期**：2021-08-10
> **形式**：23 页横版幻灯片（产品介绍 PPT）
> **说明**：本 md 按原 PPT **逐页（Page 1/23 ~ 23/23）** 完整转录，包含全部文字、表格、图示要点与脚注，不删减。图形示意（架构图/时间轴/框图）以结构化文字等价呈现。

---

## 目录（按幻灯片）

| 页 | 标题 |
|---|---|
| 1 | 封面 |
| 2 | 保密义务 |
| 3 | 工规车用模块 Roadmap |
| 4 | 章节目录 |
| 5 | 尺寸参数 |
| 6 | 模块技术参数 |
| 7 | 主要优势 |
| 8 | 章节目录（开发计划） |
| 9 | 时间轴 / 强制认证计划 |
| 10 | 章节目录（技术详解） |
| 11 | 系列应用架构 |
| 12 | 软件优势 |
| 13 | FOTA |
| 14 | 安全方案 |
| 15 | QuecOpen® 软件框架 |
| 16 | 多 APN 解决方案 |
| 17 | LTE 模块技术支持工具包 |
| 18 | 技术支持（分隔页） |
| 19 | 章节目录（应用领域） |
| 20 | 车载行业应用 |
| 21 | T-BOX |
| 22 | 定位器 / 车载 OBD / UBI |
| 23 | 移联万物，志高行远 + 联系方式 |

---

## Page 1 / 23 — 封面

- **Quectel**
- **EC200A-CN(TA) QuecOpen**
- **LTE Standard 模块**
- **产品介绍**
- 2021 年 3 月

---

## Page 2 / 23 — 保密义务

除非上海移远通信技术股份有限公司特别授权，否则我司所提供文档和信息的接收方须对接收的文档和信息保密，不得将其用于除本项目的实施与开展以外的任何其他目的。未经上海移远通信技术股份有限公司书面同意，不得获取、使用或向第三方泄露我司所提供的文档和信息。对于任何违反保密义务、未经授权使用或以其他非法形式恶意使用所述文档和信息的违法侵权行为，上海移远通信技术股份有限公司有权追究法律责任。

> 页脚：版本 1.0.0 ｜ 状态：受控文件（此页脚出现在全部正文页，后续不再重复标注）。

---

## Page 3 / 23 — 工规车用模块 Roadmap

工业级/车用模块路线图（按 Cat 等级与预估工程样品时间排布）：

| 模块 / 系列 | 制式 | 速率 | 预估工程样品时间 |
|---|---|---|---|
| EC200U 系列 | LTE Cat 1 | 10M DL / 5M UL | Q9/21 |
| **EC200A-CN(TA) QuecOpen** | **LTE Cat 4** | **150M DL / 50M UL** | Q1/22（图中标注 Cat 4） |
| AG35-CEN | LTE Cat 4 | — | Cat 4 |

> 图例：`Qx/YY` = Estimated Engineering Sample Time（预估工程样品时间）。

---

## Page 4 / 23 — 章节目录

- **产品优势和规格**
- 开发计划
- 技术详解
- 应用领域

---

## Page 5 / 23 — EC200A-CN(TA) QuecOpen® 尺寸参数（LTE Cat 4 模块）

| 维度 | 数值 |
|---|---|
| 长度 | 29.0 mm（±0.15 mm） |
| 宽度 | 32.0 mm（±0.15 mm） |
| 高度 | 2.65 mm（±0.2 mm） |
| 重量 | 4.6 g |

---

## Page 6 / 23 — EC200A-CN(TA) QuecOpen® 模块技术参数

页眉标注：LTE Cat 4 模块 ｜ 29.0 mm × 32.0 mm × 2.65 mm ｜ 10 Mbps DL / 5 Mbps UL[^rate]

| 项目 | 参数 |
|---|---|
| 型号 | EC200A-CN(TA) QuecOpen |
| LTE-FDD | B1 / B3 / B5 / B8 |
| LTE-TDD | B34 / B38 / B39 / B40 / B41 |
| WCDMA | B1 / B5 / B8 |
| GSM | B3 / B8 |
| 处理器 | **Cortex-A7 1.2 GHz** |
| 存储空间 | **1 Gb + 2 Gb**，用户可用 RAM < 100 M，ROM > 100 M |
| VoLTE | 支持 |
| Wi-Fi / BT | 不支持，可外挂 |
| RGMII | 支持 |
| (U)SIM | 支持 |
| FOTA | 支持 |
| 供电电压 | 3.4 ~ 4.3 V，典型值 3.8 V |
| 一般特性 | 工作温度范围 -35° ~ 75°；扩展温度范围 -40° ~ 85° |
| 区域 | 中国 / 印度 |
| 认证 | 强制认证：SRRC\* / NAL\* / CCC\* |

> 脚注：① 表示蓝牙与 Wi-Fi 功能只能任选其一。"\*" 表示正在进行中。

[^rate]: 本页页眉印有"10 Mbps DL / 5 Mbps UL"，与第 3 页 Roadmap 及产品规格书所标的 **150M DL / 50M UL**（LTE Cat 4）不一致，疑为幻灯片沿用 Cat 1 模板的笔误，此处按原文如实记录。

---

## Page 7 / 23 — EC200A-CN(TA) QuecOpen® 主要优势

| 优势 | 描述 |
|---|---|
| **硬件接口** | USB 2.0 / PCM / 模拟音频 / (U)SIM / UART / ADC / I2C / SPI / I2S / SSP / LCD / SDIO / 矩阵键盘 / RESET / PWRKEY / 天线接口（主天线、GNSS 天线、分集天线） |
| USB 转串口驱动 | Windows 7/8/8.1/10，Linux 2.6~5.12，Android 4.x~11.x |
| GNSS 驱动 | Android 4.x~11.x |
| USB RNDIS 驱动 | Windows 7/8/8.1/10，Linux 2.6~5.12 |
| USB ECM 驱动 | Linux 2.6~5.12 |
| USB NCM 驱动 | Linux 2.6~5.12 |
| 丰富的软件协议栈 | TCP / UDP / PPP / NTP / NITZ / FTP / HTTP / PING / CMUX / HTTPS / FTPS / SSL / FILE / MQTT / MMS / SMTP / SMTPS |
| **特殊功能** | FOTA（空中下载固件升级）；(U)SIM 卡检测；QuecOpen(Open Linux) / 多 APN；安全启动（Secure Boot），支撑代码 / 用户数据备份；内置 Codec；通过专用电路及组件实现 ESD/EMI 防护 |
| **短消息** | 文本和 PDU 模式；点对点短信收发；短消息小区广播；短消息存储：ME & SM |
| QuecOpen® | 支持 |
| **音频** | 支持 1 路数字音频 PCM 接口；GSM：HR/FR/EFR/AMR/AMR-WB；支持回音消除和噪声抑制 |

> 脚注：① 表示蓝牙与 Wi-Fi 功能只能任选其一。"\*" 表示正在开发中。

---

## Page 8 / 23 — 章节目录

- 产品优势和规格
- **开发计划**
- 技术详解
- 应用领域

---

## Page 9 / 23 — EC200A-CN(TA) QuecOpen® 时间轴

**项目进度**（2021 年 5 月 ~ 2022 年 2 月）阶段节点：**Pre-ES → ES → CS → MP**

| 阶段 | 含义 |
|---|---|
| **Pre-ES** | 工程样品阶段；基本功能完善，可供客户进行简单 Demo 演示 |
| **ES** | 工程样品阶段；基本功能完善，可供客户进行简单 Demo 演示 |
| **CS** | 商业样品阶段；稳定的硬件设计和相对稳定的软件设计，根据需求可增加软件特性 |
| **MP** | 软硬件设计已达量产阶段，认证进度请见"认证计划" |

**强制认证计划**：SRRC / NAL / CCC —— 列"开始（预估）"与"完成（预估）"两栏（PPT 中为时间条，无具体日期文字）。

---

## Page 10 / 23 — 章节目录

- 产品优势和规格
- 开发计划
- **技术详解**
- 应用领域

---

## Page 11 / 23 — EC200A-CN(TA) QuecOpen® 系列应用架构

以 EC200A-CN(TA) QuecOpen 模块为核心的系统连接框图，外围接口与器件：

- **射频**：2×2 MIMO（LTE）；GNSS Antenna。
- **存储/Wi-Fi**：eMMC 经 SDIO；外挂 **FC20**（WLAN）经 SDIO；FC20 经 PCM 接 SPK / MIC。
- **供电**：3.8 V 经 DC-DC / LDO。
- **卡**：1×(U)SIM。
- **主机侧**：MCU 经 SPI / UART / GPIOs / USB 与模块互联；MCU 另接 CAN；USB 2.0 OTG。
- **网络/扩展**：RGMII / RMII 经 PHY；I2C 接 Sensor；ADC。

---

## Page 12 / 23 — EC200A-CN(TA) QuecOpen® 软件优势

- **USB 转串口驱动**：Windows 7/8/8.1/10；Linux 2.6~5.12；Android 4.x~11.x
- **GNSS & RNDIS & ECM & NCM 驱动**：
  - GNSS 驱动：Android 4.x~11.x；Linux 3.18–5.12
  - USB RNDIS 驱动：Windows 7/8/8.1/10，Linux 2.6~5.12
  - USB ECM 驱动：Linux 2.6~5.12
  - USB NCM 驱动：Linux 2.6~5.12
- **品质保证**：可靠的网络传输协议；稳定的闪存保护机制；先进的音频处理算法
- **特殊功能**：QuecOpen® ①；QuecLocator®
- **灵活功能应用**：(U)SIM 卡检测；DTMF；TTS\*
- **增强型 AT 命令集**：3GPP TS 27.007 (GSM 07.07)；3GPP TS 27.005 (GSM 07.05 SMS)；Quectel 增强型 AT 命令集

> 脚注：① 待定；② 表示蓝牙与 Wi-Fi 功能只能任选其一；"\*" 表示正在开发中。

---

## Page 13 / 23 — FOTA

**FOTA（Firmware Upgrade Over-The-Air）** 固件空中升级功能，有助于设备通过 LTE/GSM 网络进行快速升级，也可实现备份恢复。

**FOTA 升级固件步骤：**

1. 第一步：从移远通信获取目标固件包。
2. 第二步：将目标固件包上传服务器。
3. 第三步：执行 `AT+QFOTADL` 命令。模块自动完成固件下载和升级。

---

## Page 14 / 23 — 安全方案

- **Secure Boot（安全启动）链**：SBL1 → APPSBL → Linux → Modem / File System → Secure Storage（安全存储）。
- **Cyber Security（网络安全）**：Security Policy（安全策略）、Account Management（账户管理）、Internet。
- **四大安全维度**：安全启动 / 安全策略 / 网络安全 / 存储安全 / 账户管理。

---

## Page 15 / 23 — QuecOpen® 软件框架

**开发环境（上层）**

- 命令行 SDK、下载工具、工具包
- 中间件
- 多语言支持（C、C++、Shell 脚本）
- C 运行时库（GCC 编译器）
- 自动编译环境

**Linux 分布（下层）**

- 文件系统、工具包、免费开源包
- Linux 内核
- 小内核加载程序（LK）
- 驱动、BSP
- 模块硬件

> 图：QuecOpen® 框架图。

---

## Page 16 / 23 — 多 APN 解决方案

以车载 T-BOX 为例的多 APN 网络连接方案：

- **Quectel LTE QuecOpen Module** 提供 Network Connectivity，同时承载 **Public APN（公网）** 与 **Private APN（私网）**。
- 外接 **FC20 / FC41D（Wi-Fi/BT）** 经 SDIO / PCIe；MCU 经 UART / SPI 等。
- 链路：T-BOX Data Service / TSP ↔ T-BOX ↔ Central Control Unit (in car) ↔ Internet。

**三点说明：**

- 用户通过**公网 APN** 实现车内娱乐及 Wi-Fi 热点上网功能。
- 车厂通过**私网 APN** 实现 TSP 与汽车的通讯，确保数据通信更安全、更可靠。
- **目前可支持 8 个 APN。**

---

## Page 17 / 23 — LTE 模块技术支持工具包

**技术支持工具包**

- 软件硬件说明文档
- 评估板及其辅件
- 认证、测试报告
- QNavigator

**开发板（UMTS & LTE EVB Kit）**

- 接口：
  - a) 供电接口
  - b) USB 接口
  - c) UART 接口
  - d) (U)SIM 接口
  - e) SD 卡接口
  - f) 音频接口
- 辅件：
  - a) 网络 LED 指示灯
  - b) 开/关机开关
  - c) 复位键
  - d) 测试点

---

## Page 18 / 23 — 技术支持（分隔页）

仅含标题"技术支持"。

---

## Page 19 / 23 — 章节目录

- 产品优势和规格
- 开发计划
- 技术详解
- **应用领域**

---

## Page 20 / 23 — 车载行业应用

**车载和运输：**

- 车辆跟踪
- 货物跟踪
- 船舶跟踪
- 车队管理
- OBD
- DVR
- 两客一危
- UBI 车险

---

## Page 21 / 23 — T-BOX

车联网标准终端 T-Box 利用 **4G/5G 无线通信、GNSS 卫星定位、加速度传感和 CAN 通讯**功能，来实现车辆远程监控、远程控制、安全监测和警报、远程诊断等在线应用。

T-BOX 通过内置的模块将车辆信息和位置信息发送至 TSP 中心。TSP 中心可跟踪车辆状态并据此提供相应服务。

**典型功能：**

- 车窗和空调控制
- 车况警报
- 不安全驾驶警告和报告
- 车辆远程自助诊断

> 末端：TSP 中心（车载服务提供者）。

---

## Page 22 / 23 — 定位器 / 车载 OBD / UBI

**应用场景：**

- 车辆追踪
- 货物追踪
- 船舶追踪
- 车队管理
- 物品追踪

**工作流程：** 发现警情时，车辆司机或乘客通过报警按钮报警 → 内置 LTE Cat.1 模组的无线车载设备实时监控并上传报警信号至管理中心 → 经 Internet 发送告警短信 / 彩信到用户手机、发送报警信号到监控中心 / 呼叫中心。示例短信：`At HH:MM:SS, the No. xx bus with plate number xx sent an alarm for xxx.`（本地回路录像）。

**核心能力：**

- **实时跟踪**：物品位置和状态，车辆运行状态
- **多重定位**：GPS / 北斗 + LBS 等
- **轨迹查询**：随时随地查询行驶轨迹
- **电子围栏**：超出预设路线，自动报警
- **SOS 紧急呼叫**

> 注：本页示意图标注"内置 LTE Cat.1 模组"，为通用定位器场景示意（与本手册主角 EC200A LTE Cat 4 的具体型号不同），按原文如实记录。

---

## Page 23 / 23 — 移联万物，志高行远

- 拥有行业最丰富、完整的产品线，一站式满足各种场景、各个地区的需求
- 采用全自动生产线、测试线，保障产品质量始终如一，同时具有超高性价比
- 建立了业内规模最大的研发团队，为客户提供及时、专业、贴心的技术支持服务
- 持续研发新技术、新产品，率先发布 5G、NB-IoT、C-V2X 等产品

**联系方式：**

- 全国热线：400 960 7678
- 地址：上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼，200233
- 联系电话：+86 21 5108 6236
- 电子邮件：info@quectel.com
- 技术支持：support@quectel.com
- 移远微信公众号

---

## 关键信息速览（编者汇总，便于检索）

- **定位**：LTE Cat 4 工规/车用 QuecOpen® 模块，处理器 Cortex-A7 1.2 GHz，存储 1 Gb + 2 Gb。
- **速率**：150M DL / 50M UL（Cat 4；见 P3 Roadmap 与规格书，P6 页眉的 10M/5M 为模板笔误）。
- **多 APN**：最多 **8 个 APN**，公网/私网并存（与本项目 `open_dial` 的 APN 绑定逻辑相关）。
- **温度**：工作 -35~+75 °C，扩展 -40~+85 °C；供电 3.4~4.3 V（典型 3.8 V）。
- **FOTA**：`AT+QFOTADL` 触发；安全启动链 SBL1→APPSBL→Linux→Modem。
- **认证**：SRRC / NAL / CCC（进行中）；区域中国/印度。

---

*本 md 由原 PPT（23 页）逐页转录整理，保留全部文字、表格、脚注与图示要点；个别幻灯片中的纯图形示意已转为结构化文字等价描述，疑似笔误处已加脚注标明并保留原文。*
