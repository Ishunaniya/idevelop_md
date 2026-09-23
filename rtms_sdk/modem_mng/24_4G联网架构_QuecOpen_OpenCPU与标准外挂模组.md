# 4G 联网架构知识点：QuecOpen/OpenCPU 与标准外挂模组

> 文档状态：**经静态资料与代码审核，真机验证待补**  
> 版本：1.2（2026-09-13）  
> 目标读者：个人知识积累与项目实施备忘  
> 适用范围：以移远 EC/EG/AG 系列和外部 Linux 主控为主；MCU 部分仅说明概念和责任边界，不是 STM32 等平台的可直接照抄实施手册。
>
> 目标：弄清“应用跑在哪里”“主控和模组通过什么通信”“拨号和 IP 数据走什么通道”，避免将 OpenCPU、AT 串口、`ttyUSB*` 和 ECM 网卡混为一谈。
>
> 使用限制：本文不承诺“覆盖全部市售方案”或“绝对无错”。带型号的实施结论必须有对应 SKU、固件版本、硬件拓扑和真机记录。

## 0. 覆盖边界：什么叫“完整”，什么不应假装穷尽

本知识点按**技术架构类别**覆盖当前常见的蜂窝联网路径，而不是声称列完市场上每一个模组型号、每份私有 SDK 或每块载板的变体。不同厂商、基带平台、固件 USB composition、运营商认证和客户定制都会改变实际接口。

文档分为两部分：

* **第 0～10 节：通用架构知识**，只讲可跨项目复用的概念。
* **第 11～12 节：项目实施附录**，区分已确认代码事实、当前未提交工作区实现、设计建议和待真机验证项。

本文已覆盖的主流类别是：

```text
应用位置：模组内应用  |  外部 Linux/MCU 主控  |  两者并存
物理互连：UART/USB/PCIe/以太网等
功能与协议：USB serial、ECM/NCM/RNDIS、QMI/MBIM、MHI、PPP
数据所有权：主控 IP 栈  |  模组 socket 卸载  |  模组内客户应用
低功耗唤醒：主机 suspend remote wake  |  常供电 MCU + RI/GPIO  |  模组自主联网
```

本文可用于建立选型框架和判断 RK3506 方案边界，不能单独作为量产依据。真正落地前，仍必须以**选定模组的硬件设计手册、AT 手册、USB/PCIe composition 手册和目标固件实测**为准；不能把“某一类别支持”推导为“任意 EC/EG/AG 型号、任意固件都支持”。

## 1. 一句话结论

4G 模组内有蜂窝协议栈。按**产品主应用位置**可以看成两个主要端点：

```text
模组内应用：主业务运行在模组的 Linux/RTOS/脚本环境
外部主控：  主业务运行在 RK/MCU 等外部处理器
```

实际产品还可以是混合形态，例如模组/常供电 MCU 只保持唤醒 socket，RK 上电后再承担视频和复杂业务。区分架构的核心是：谁发起数据业务、谁持有 TCP/MQTT socket、谁在外部主控掉电后仍可能存活。文中“PDP”是沿用 AT 手册的通用叫法；LTE 场景更严谨地还会涉及 PDN connection/EPS bearer。

## 2. 先区分几个经常混淆的概念

| 概念 | 是什么 | 不是什么 |
| --- | --- | --- |
| 模组 | 包含基带、射频、SIM 接口和蜂窝协议栈的通信设备 | 不必然只是“串口外设” |
| OpenCPU | 模组内运行客户应用的广义开发模式，底层可为 RTOS 或 Linux | 不是统一的 OS、API 或 AT 开关 |
| QuecOpen | 移远的模组内客户应用方案；本文所引 EC2x/EG9x/EG25 资料定义为 Linux 平台 | 不应与所有厂商、所有 RTOS OpenCPU 等同 |
| AT 通道 | 主要用于主控配置/查询模组；模组 socket 卸载和 PPP 场景也可承载业务数据 | 不是 Linux 的 IP 网卡 |
| ECM/NCM/RNDIS | USB 网络功能/封装，向主机提供 Ethernet-like netdev | 不负责蜂窝注册本身 |
| QMI/MBIM | 用于管理 modem/bearer 的协议，并配有相应数据通道约定 | 不是物理总线，`cdc-wdm`/WWAN 节点也不是 AT 口 |
| PPP | 在串行字节流上承载 IP 数据的链路层协议 | 不是模组的 USB 复合功能 |

是否使用 OpenCPU 不能只看模块外壳型号。它取决于模块 SKU、所刷固件、SDK、授权和产品软件部署位置；同一系列模块可能既能作为外接 modem，也可能具备模组侧应用开发能力。

## 3. 两个主要端点的总览

| 维度 | QuecOpen/OpenCPU | 标准外挂模组 |
| --- | --- | --- |
| 产品业务程序 | 在模组内部运行 | 在 RK、MCU、i.MX 等主控运行 |
| 网络控制 API | 平台专用 SDK；本仓库 EC200A/AG35 用 `ql_nw_*`/`ql_data_call_*`，EG25G 用 MCM/QMI/DSI | AT、QMI、MBIM、PPP 拨号或厂商连接管理器，按模式选一个 data-call owner |
| 数据网卡位置 | 模组内部；本仓库 EC200A/AG35 为 `ccinet0`，EG25G 可见 `rmnet_data*` | 主控 Linux，常见 `eth1`、`usb0`、`wwan0`、`ppp0` |
| 云端 TCP/MQTT socket | 模组内部应用持有 | 主控应用持有；也可额外让模组内部 socket 持有 |
| 主控掉电的影响 | 若模组继续供电，模组内应用和 socket 可能仍在 | 主控进程/socket 必然消失；模组的驻网/PDP 可能保留，若另有模组侧 socket 卸载，该 socket 是否保留取决于固件和接口断开策略 |
| 合适产品 | DTU、低功耗终端、定位器、协议较简单的设备 | 视频、远控、AI、复杂外设和大存储产品 |
| 主要风险 | 模组复位会连同客户应用一起重启；SDK 与固件 ABI 绑定 | USB 重新枚举、端口变化、Linux 网络栈重建、主控与 MCU 抢控制权 |

## 4. QuecOpen/OpenCPU：应用跑在模组内

### 4.1 软件和数据路径

```text
云平台
  ⇅ LTE 空口 / 运营商核心网
模组 CP：基带、SIM 管理、注册、PDP/PDN
  ⇅ 平台专用内部数据服务（部分 Qualcomm 平台使用 QMI/MCM/DSI）
模组 AP Linux/RTOS
  ├─ 客户应用
  ├─ 平台 SDK：SIM、注册、信号、运营商、data call
  ├─ TCP / MQTT / TLS / HTTP 客户端
  └─ 内部 netdev：如 ccinet0 或 rmnet_data*，以当前 SDK/固件为准
```

客户应用与蜂窝协议栈都位于模组内，不需要通过另一颗 RK 的 USB 或 UART 来完成拨号。不要泛化为“必然是同一颗 SoC”：AP/CP 划分和内部 IPC 由基带平台决定。本仓库的 SDK 回调可返回 IP、网关、DNS 和内部网卡名；其他 SDK 的回调字段和系统自动配置行为必须另行确认。

### 4.2 典型实现步骤

1. 确认模块支持 QuecOpen/OpenCPU，并刷入与 SDK 匹配的固件。
2. 初始化 SIM、网络和数据服务；注册注网和 data-call 事件回调。
3. 查询 SIM、PIN、数据域注册、信号和运营商，准备 APN/PDP 类型/认证。
4. 创建数据呼叫，等待异步 `CONNECTED` 回调；不要只根据 API 同步返回判断拨号成功。
5. 按 SDK 约定处理回调提供的网卡、地址、网关和 DNS；若 SDK/系统已自动配置，不应重复写路由。本仓库 EC200A/AG35 实现会显式配置默认路由和 DNS。
6. 在模组内运行 MQTT/TCP/TLS/HTTP 业务，完成鉴权、订阅和心跳。
7. 定义低功耗、PDP 保活、PSM/eDRX、掉线重连及模组复位策略。

### 4.3 UART 在 OpenCPU 架构中的位置

模组内部应用不需要用 UART 控制“自己”。UART 通常用于：

* 与外接低功耗 MCU 交换业务数据、唤醒请求或诊断信息；
* 连接传感器、GNSS、调试器；
* 由 MCU 在模组常供电时接收 URC、短信或下行唤醒事件。

如果模组复位或执行会导致模组 AP 重启的操作，模组内客户应用也会一起停止。因此不能把 `CFUN=1,1`、PWRKEY 或 VBAT 断电当作普通的“网络重拨”手段。

## 5. 标准外挂模组：应用跑在外部主控

### 5.1 控制面和数据面必须分开理解

外接模式中，模组处理 SIM、无线注册和 PDP；主控处理产品业务。主控需要两种能力：

```text
控制面：查询和命令模组
  AT / QMI / MBIM

数据面：让主控 Linux/RTOS 收发 IP 包
  ECM/NCM/RNDIS netdev、QMI/MBIM 配套的 WWAN 数据通道，或串行 PPP
```

控制面与数据面可能使用同一 USB 物理线上的不同 interface，也可能分别走 UART 和 USB。

### 5.2 UART 连接：控制面为主，可选 PPP 或模组 socket

硬件上通常至少连接：UART TX/RX、GND；按需要增加 RTS/CTS、DTR、RI、PWRKEY、RESET、STATUS。必须确认 IO 电平，蜂窝模组常见 1.8 V，不能假设可直接连接 3.3 V 主控。

#### AT 控制

主控通过 UART 发 `AT`、SIM/注册/APN/PDP 命令，读 `OK`、`ERROR` 和 URC。UART 设备在 Linux 通常是 SoC 的 `/dev/ttyS*`，不是 `/dev/ttyUSB*`。

#### 模组内部 socket

主控发 AT 命令让模组建立 TCP/MQTT/HTTP，业务负载也经 UART 收发。优点是主控不必拥有完整 IP 数据面，常供电 MCU 可在 Linux 主机掉电后继续接收小量唤醒命令；限制是吞吐、并发、TLS 证书维护和厂商 AT 差异。

#### PPP

主控完成 AT 拨号后把 UART 切为 PPP 字节流，Linux `pppd` 创建 `ppp0`、获取 IP 和默认路由。它只需要 UART，但吞吐和调试体验通常不如 USB 网卡，不适合高码率视频/复杂远控业务。

### 5.3 USB 复合设备：控制面 + 高吞吐数据面

模组通过 USB 接到 Linux 主控时，通常枚举为复合设备：不同 USB interface 被不同内核驱动绑定。

| 功能 | 主控 Linux 典型结果 | 职责 |
| --- | --- | --- |
| USB AT 串口 | `option`/`usbserial` 创建 `/dev/ttyUSB0...N` | AT、URC、短信、PDP 控制 |
| CDC ECM | `cdc_ether` 创建 `eth1`/`usb0` 等网卡 | 以太网式 IP 数据面 |
| CDC NCM | `cdc_ncm` 创建网卡 | ECM 类似，常有更好的聚合能力 |
| RNDIS | `rndis_host` 创建网卡 | 另一类 USB 网络封装 |
| QMI | 常见 `qmi_wwan` netdev + `/dev/cdc-wdmX` | 主机用 QMI 管理 data call/bearer；数据通道常为 raw-IP，也存在其他实现 |
| MBIM | 常见 `cdc_mbim` netdev + `/dev/cdc-wdmX` | 主机用 MBIM 管理 IP session；控制节点和数据 netdev 是两个逻辑接口 |

`/dev/ttyUSB*` 是内核 USB serial 驱动创建的**控制节点**，不是 `modem_mng` 创建的，也不是 IP 数据面。ECM/NCM/RNDIS 的 netdev 才承载主控应用的 TCP/UDP 流量。

### 5.4 外接 USB ECM 的典型链路

```text
RK Linux
  ├─ USB serial 驱动 → /dev/ttyUSB* → AT 控制
  ├─ CDC 网络驱动 → eth1 / usb0 → IP 数据面
  └─ 远控应用 → TCP/MQTT/TLS
        ⇅
模组：SIM、注册、PDP、ECM 数据功能
        ⇅
运营商网络 / 云平台
```

典型步骤：

1. 给模组稳定供电，按硬件时序执行 PWRKEY；等待 USB 枚举。
2. 内核绑定 USB serial 和 ECM/NCM/RNDIS/QMI/MBIM 驱动，出现 AT 节点和网卡。
3. 基于 USB 父级拓扑、VID:PID、`ATI`/IMEI 找到正确 AT 端口与对应网卡；不要硬编码枚举号。
4. 通过 AT 或 QMI/MBIM 检查 SIM、注册状态和 APN；冷启动时建立 PDP。
5. 在 ECM 模式下，等待网卡出现，优先按模组手册运行 DHCP；只有硬件/固件手册明确给出主机侧静态参数时才可静态配置。不得默认把 `AT+CGCONTRDP` 的 PDP/MT 侧地址写到主机 ECM 网卡。
6. 检查默认路由、DNS 和指定云端连通性，再启动或通知远控业务。
7. 远控业务完成 TLS 鉴权、订阅/注册和心跳 ACK 后，才可宣布 `cloud_ready`（本文建议的逻辑状态名）。

## 6. 与 RK3506 远控需求的关系

代码可确认 `rk3506j/` 定位为**外接 USB AT + USB 网络**：AT 用于查询/控制模组，主机 netdev 供 RK 上的业务访问网络。配置和拨号代码称其为 ECM，但最终 USB composition 仍应在目标固件上用 `lsusb -t`、sysfs 驱动绑定和真机日志确认。

> **实现状态（2026-09-14）**：RK3506 工作区已将旧的全局 `keep_modem_session` POC 改为 AOV `event_code=13` 驱动的 `REMOTE_WAKE` 接管；无事件则走冷启动，接管失败按“软拨后硬恢复”升级。它仍是未发布、未真机验收的工作区能力，且依赖 AOV/MCU 可靠重发本次启动事件。

当 MCU 关闭 RK 电源、但不关闭模组 VBAT 时，必须分开看：

| 状态 | 可能结果 |
| --- | --- |
| 模组驻网/PDP | 可能保留，取决于模块固件、供电和运营商 |
| USB 物理连线 | 可能仍连接，但 RK 重新上电后仍需 USB 重新枚举 |
| `/dev/ttyUSB*`、ECM 网卡 | 上一次 Linux 的节点已经消失，新内核重新创建，编号不可假定 |
| Linux IP/路由/DNS | 已随 RAM 状态消失，必须恢复 |
| RK 远控进程的 socket | 必然消失，必须重新连接云平台 |

该远控目标架构应区分三条路径：

```text
cold_boot
  → 按电源 owner 协议允许 PWRKEY，完成注册/PDP/USB 数据功能/Linux 网络/业务建连

remote_wake
  → MCU 已确认模组常供电
  → 不碰 PWRKEY、CFUN、PDP deactivate
  → 等 USB、查询 PDP、恢复 Linux 网络、远控业务重连

modem_fault_recovery
  → MCU 复位模组
  → 再执行完整 cold_boot
```

若要求 RK 断电时仍可由云端下发唤醒，必须让模组内应用或常供电 MCU 持有一条适合的 MQTT/socket/短信/来电接收通道；其收到并校验合法事件后通过 RI、GPIO 或 UART 通知 MCU 给 RK 上电。这些通道在主机掉电时是否保持，取决于固件、DTR/USB 断开策略和运营商网络，必须实测。RK 上的 `modem_mng` 只能负责上电之后的网络接管或完整拨号，无法在 RK 已掉电时维持 Linux socket。

## 7. 选型检查清单

- [ ] 产品业务是否必须跑在 RK？若不是，OpenCPU 能否覆盖协议、存储、安全和 OTA 需求？
- [ ] 模块实际固件是否支持 QuecOpen/OpenCPU，SDK 是否与固件匹配？
- [ ] 外接模式的数据面选择 ECM/NCM、QMI、MBIM 还是 PPP？吞吐和驱动是否满足？
- [ ] UART、USB、RI、DTR、PWRKEY、RESET、STATUS、VBAT/USB VBUS 的电平和 owner 是否已确认？
- [ ] 谁在 RK 掉电时保持接收云端唤醒指令？
- [ ] 一个 AT 控制端口是否保证只有一个 owner，避免 RK、MCU、脚本并发抢占？
- [ ] 是否区分 `host_network_ready` 与 `cloud_ready`？
- [ ] 是否有 cold boot、remote wake、PDP 丢失、AT 无响应、USB 延迟枚举、端口重编号的真机测试？

## 8. 本仓库对应代码位置

| 架构 | 目录 | 识别依据 |
| --- | --- | --- |
| 模组内应用 | `ec200a/`、`ag35/` | 使用 `ql_nw_*`、`ql_data_call_*` SDK；当前入口明确等待 `ccinet0` |
| 模组内应用 | `eg25/` | 使用 MCM/QMI/DSI SDK 和内部 `/dev/smd8`；代码监测 `rmnet_data*` |
| 外接 Linux 模组 | `rk3506j/` | USB `ttyUSB*`/USB netdev 发现、AT `QNETDEVCTL`、DHCP，以及当前存在风险的 `CGCONTRDP` fallback |

这些目录的代码不应互相照搬：OpenCPU/QuecOpen 的 SDK 数据呼叫 API 不能直接在 RK 上调用；RK 的 `ttyUSB`/ECM 拨号流程也不应用于已经运行在模组内的应用。表中 RK3506J 的 `CGCONTRDP` 只证明当前代码存在该路径，**不代表该路径已被本文认定为正确**；见 12.3.1 的风险说明。

## 9. 主流蜂窝联网方案全景分类

### 9.1 先按“IP 和业务 socket 属于谁”分类

这是比“接 UART 还是 USB”更根本的第一层选择。

| 类别 | IP 栈和业务 socket 所在位置 | 主控掉电后 | 典型用途 | 远控唤醒适配度 |
| --- | --- | --- | --- | --- |
| 模组内应用 | 模组 AP Linux/RTOS | 模组常供电时可能继续存在 | DTU、追踪器、低功耗 IoT | 高：模组可直接接收下行唤醒 |
| 模组 socket 卸载 | 模组协议栈；外部 MCU/主控经 AT 收发 payload | 模组常供电时 socket 可能继续维持，外部主机接口状态仍需恢复 | 小流量 MCU、短信/告警、备用唤醒通道 | 高：适合常供电 MCU |
| 主控 IP 栈 | 主控 Linux/RTOS 的网卡和业务进程 | IP、路由、socket 均消失；PDP 可能保留 | 视频、网关、AI、复杂远控 | 中：需 MCU/模组额外唤醒通道 |

“PDP 仍在”与“主控业务 socket 已断”可以同时成立。PDP 是模组/运营商侧承载；主控 socket 是进程和 Linux 内核对象。主控彻底掉电时，后者必然消失。

### 9.2 用五层模型描述外部主控链路

不要把总线、USB function、控制协议和 netdev 当成同一层。一个完整方案应按下表描述：

| 层级 | 回答的问题 | 常见选项 |
| --- | --- | --- |
| 1. 物理互连 | 主机与模组如何传输比特 | UART、USB 2.0/3.x、PCIe、RGMII/RMII/SGMII、SDIO/HSIC/私有总线 |
| 2. 设备功能/通道 | 总线上暴露哪些功能 | USB serial、ECM/NCM/RNDIS、MBIM function、MHI channel |
| 3. 控制协议 | 谁管理 SIM、注册和 data call | AT、QMI、MBIM、厂商 SDK；同一 data call 要有明确 owner |
| 4. 数据封装与主机接口 | IP 包如何进入主机栈 | Ethernet-like netdev、raw-IP/WWAN netdev、PPP `ppp0`、多路复用子接口 |
| 5. 业务所有者 | 谁持有 TLS/MQTT/TCP socket | 外部主控、模组内应用、模组 AT socket 卸载 |

常见组合示例：

| 物理链路 | 控制面 | 数据面 | 工程边界 |
| --- | --- | --- | --- |
| UART | AT | 模组 socket 或 PPP | 硬件简单；吞吐、流控和 AT owner 需评估 |
| USB 复合设备 | USB AT 或 QMI/MBIM | ECM/NCM/RNDIS netdev 或 WWAN 数据通道 | 常用于 Linux 主控；必须核对固件 composition 和驱动 |
| PCIe + MHI | MHI 控制通道上的平台协议 | MHI/WWAN 数据通道 | 常见于高带宽模组；吞吐、CPU 占用和功耗必须以具体实现测量 |
| RGMII/RMII/SGMII | AT/UART/USB 或模组本地管理 | Ethernet 帧 | 只有模组硬件与固件明确支持时才可选 |
| SDIO/HSIC/私有总线 | 厂商 SDK/私有协议 | 私有或 IP 数据通道 | 只能按特定平台资料实施 |

这些方案与本仓库 RK3506J 的 USB AT + USB 网络不是同一实现。更换它们通常意味着重新设计设备树/总线初始化、内核驱动、用户态管理器、生产诊断和恢复逻辑。

### 9.3 USB/WWAN 方案：ECM、NCM、RNDIS、QMI、MBIM 的区别

| 方案 | 主机看到什么 | PDP/bearer 谁控制 | 主机 IP 配置 | 优点 | 注意事项 |
| --- | --- | --- | --- | --- | --- |
| ECM | 以太网式 netdev | 常由 AT/模组自动策略控制 | 通常为 DHCP；静态方案须有型号手册依据 | 简洁、兼容性好 | 主机 LAN 地址与模组 PDP 地址可不同；不可盲用 `CGCONTRDP` |
| NCM | 以太网式 netdev | 依模组实现 | 通常为 DHCP 或控制协议返回的参数 | 支持 USB 数据报聚合 | 模组 USB composition、会话管理方式和内核驱动必须匹配 |
| RNDIS | 以太网式 netdev | 同 ECM 类 | 常为 DHCP | Windows 兼容场景常见 | Linux/模块兼容性需实测 |
| QMI 管理 + WWAN 数据 | `cdc-wdm` 等控制节点 + WWAN 数据接口 | 主机 QMI 客户端 | QMI 返回 IP/DNS/MTU，数据面常为 raw-IP | 可提供多 PDN、复用和结构化状态 | 不是所有移远模组都支持 QMI；要处理 client 生命周期 |
| MBIM | `cdc-wdm` 控制 + MBIM netdev | 主机 MBIM 客户端 | MBIM 返回 bearer IP 配置 | 标准化、工具生态较好 | 模组对 MBIM/扩展能力的支持差异大 |

不要把 ECM 的 `AT+QNETDEVCTL` 流程搬到 QMI/MBIM 模式，也不要假定 QMI/MBIM 的 `/dev/cdc-wdmX` 是 AT 口。控制协议、网卡类型、IP 获取方式和故障恢复都不同。

### 9.4 模组内应用也不只有一种：OpenCPU/OpenLinux/脚本

| 模式 | 应用形态 | 特点 | 适合的唤醒方式 |
| --- | --- | --- | --- |
| OpenCPU/RTOS | C 应用跑在模块 RTOS/轻量运行时 | 资源和功耗低；厂商 SDK 强绑定 | 模组 MQTT/socket/短信 → RI/GPIO → MCU |
| OpenLinux/QuecOpen Linux | C/C++/脚本应用跑在模块 Linux AP | 功能强，有本地文件/进程/网卡 | 模组业务常驻，主控作为被唤醒协处理器 |
| 模组脚本运行时 | QuecPython、Lua 等厂商脚本环境 | 开发快，适合控制和轻量协议 | 适合原型或低复杂度常驻唤醒代理 |
| 仅 AT socket | 模组不运行客户完整应用，主控经 AT 操作 socket | 最小实现，协议和安全能力受 AT 固件限制 | MCU 常供电、仅收小量命令 |

它们的共同点是：云连接能被放在模组一侧；不同点是可维护性、资源、SDK 绑定、OTA、安全和复杂业务能力。对于视频远控，常见折中是“模组/MCU只保留轻量唤醒通道，RK 起来后承担视频和完整业务”。

### 9.5 低功耗/远程唤醒也有三种不同等级

| 等级 | 主机状态 | 模组与 USB 状态 | 常见唤醒路径 | Linux 侧是否可直接恢复旧状态 |
| --- | --- | --- | --- | --- |
| S0 runtime PM | Linux 仍运行，部分设备 runtime suspend | USB/总线保持可管理 | USB remote wake、IRQ | 取决于 host controller、模组和驱动是否支持 runtime resume |
| suspend-to-RAM | Linux 内存与内核上下文保留 | 取决于 USB Host 是否保持电源/会话 | USB remote wake、RI、GPIO | 可能；需处理 reset/resume |
| 主机完整掉电/重启 | Linux 进程、RAM、网卡状态消失 | 模组可常供电，但 Host 会重新枚举 | 模组 socket/短信/RI → 常供电 MCU/PMIC → 主机上电 | 不可以；只能重新枚举并接管模组侧 PDP |

RK3506 远控项目属于第三种，不应套用“系统 suspend 后 quick resume”的时间和状态假设。

### 9.6 面向产品的实现选择流程

```text
是否要求主控掉电期间仍接收云端命令？
  ├─ 否
  │   └─ 选择主控 IP 栈；分别决定物理总线、控制协议和数据通道
  └─ 是
      ├─ 模组是否可运行可靠的客户应用？
      │   ├─ 是 → 模组内 MQTT/socket + RI/GPIO/MCU 唤醒主控
      │   └─ 否 → 常供电 MCU 经 UART/AT socket 或短信/来电接收唤醒
      └─ 主控重新上电后
          ├─ 模组 PDP 已在 → 只恢复主机侧网卡/IP/路由/DNS/业务
          └─ PDP 不在或模组异常 → 受控冷拨号/硬件恢复
```

选择后的必做验证项：

1. 模块当前固件到底暴露哪种 USB/PCIe composition；
2. 模块在主机掉电期间是否真的保持 VBAT、注册、PDP 和 socket；
3. RI/STATUS/DTR/PWRKEY/RESET 信号由谁供电、谁驱动、是否跨电压域；
4. 运营商 NAT、长时间空闲、PSM/eDRX 是否会关闭承载或 socket；
5. 新 Linux 启动后 USB/PCIe 枚举、IP 配置和业务重新鉴权的 P95 时间；
6. PDP 还在但主机 DHCP/DNS 不在、AT 不通、USB 延迟、端口重编号等异常路径。

## 10. 对“是否囊括所有市面方案”的准确回答

本文已经覆盖用于产品决策的主流技术类别，但不声称列尽全部市售模块和私有方案。正确的工程方法是：先从第 9 节确定当前产品属于哪个架构，再锁定模块型号、固件版本、载板原理图和运营商条件，最后针对该组合做接口和低功耗实测。任何不带“模块型号 + 固件版本 + 实物拓扑”的所谓全方案，都不能直接变成量产实现。

## 11. 项目实施附录：本仓库与 MPC 平台归类

本节是项目备忘，不是厂商中立的通用知识。“外接模组”仅说明应用不运行在蜂窝模组内，不自动证明某块板子的模组型号、PWRKEY 极性、VBAT 电源域或 AOV 断电策略。那些结论仍必须回到原理图和真机日志。

证据状态定义：

* **已确认（代码）**：当前环境中可从构建规则和源码复核，但仍不等于真机验收。
* **工作区方案**：仅存在于未提交修改，不是已发布基线。
* **未验证**：当前环境缺少所述仓库、原理图、固件或日志，不得用作实施依据。

### 11.1 总览对照

| 仓库/构建平台 | 产品应用位置 | 模组访问和数据面 | 架构归类 | 证据状态 |
| --- | --- | --- | --- | --- |
| 本仓库 `EC200A` | EC200A 模组内 Linux 应用 | `ql_nw_*`/`ql_data_call_*`，内部 `ccinet0` | 模组内应用 | **已确认（代码）** |
| 本仓库 `AG35` | AG35 模组内 Linux 应用 | `ql_nw_*`/`ql_data_call_*`，当前入口等待 `ccinet0` | 模组内应用 | **已确认（代码）**；接口名待真机复核 |
| 本仓库 `EG25G` | EG25 模组内 Linux 应用 | MCM/QMI/DSI、`/dev/smd8`、`rmnet_data*` | 模组内应用 | **已确认（代码）** |
| 本仓库 `RK3506J` | RK3506 Linux | USB `ttyUSB*` + 主机 USB netdev，AT `QNETDEVCTL` + DHCP | 标准外挂 | **已确认（代码）**；保留会话恢复为**工作区方案** |
| 本仓库 `MCIMX6Y2CVM08AB`（i.MX6ULL） | i.MX6ULL Linux | USB AT + USB netdev，AT `QNETDEVCTL` + DHCP | 标准外挂 | **已确认（代码）** |
| `mpc-base/mod` | 据原文称为 MPC 主机 Linux/AOV | 据原文称为 `/dev/ttyS2` AT + `usb0` | 暂归为标准外挂 | **未验证**：当前环境没有 MPC 仓库、原理图或日志 |

### 11.2 当前仓库各平台的细节

#### EC200A、AG35、EG25：同为“应用在模组内”，但 SDK/底层不同

三者不能因为都叫 OpenCPU/QuecOpen 就互相复制接口：

```text
EC200A / AG35
  → 当前构建使用 ql-sdk：ql_nw、ql_data_call、ql_sim 等
  → data-call 回调把内部网卡、IP、网关、DNS 回给模组内应用
  → 当前入口显式等待 ccinet0；AG35 仍应真机确认

EG25G
  → 当前构建使用 EG25 Open SDK：MCM/QMI/DSI 等库
  → 内部 AT 通道为 /dev/smd8，数据接口为 rmnet_data* 等
```

因此它们是“模组本身既是蜂窝终端、又承载客户应用”的架构；`CFUN=1,1` 或硬复位的影响范围通常不仅是无线注册，还可能使同机客户 Linux 应用一起停止。EC200A、AG35、EG25 的具体 API、接口名、复位语义和固件升级方式仍要按各自 SDK 验证。

#### RK3506J：不是 OpenCPU 路径，即使外挂的是 EC200A

RK3506J 代码运行在 RK 的 Buildroot/Linux 上。外挂 EC200A/EG912 作为 USB 复合设备时：

```text
RK Linux
  ├─ USB serial 驱动创建 /dev/ttyUSB*：AT 控制面
  ├─ CDC ECM/NCM/RNDIS 驱动创建 eth1/其它 netdev：IP 数据面
  └─ rk3506j/modem_mng：AT 查询/拨号 + DHCP + 主机路由
```

同一“EC200A”在这里仅是外接 modem；与上表 `QL_MODULE_PLATFORM=EC200A` 的模组内应用构建不是同一种部署。远控项目讨论的主机掉电、USB 重新枚举、Linux socket 重连，针对的正是这一列。

> **已知实现风险**：当前 RK3506J 代码在 DHCP 失败时会尝试把 `CGCONTRDP` 返回的地址/网关配置到主机 netdev。3GPP 定义该命令返回 MT 的 PDP 动态参数；ECM 路由/NAT 模式下的主机 LAN 地址通常由 DHCP 另行分配。在 EC200A/EG912 当前固件与真机拓扑证明两者可等同前，该 fallback 不得作为知识文档推荐流程。

#### i.MX6ULL：同属主机侧外挂模组，不应误判为模组内实现

`MCIMX6Y2CVM08AB` 的 CMake 分支把 `imx6ull/main.cpp`、`imx6ull/dial/imx6ull_dialer.cpp`、`imx6ull/device/modem_device.cpp` 和 `imx6ull/nw/network_configurator.cpp` 编入主程序。它通过 AT 查询/控制模组，并在 i.MX6ULL Linux 上配置数据网卡和路由。

这与 RK3506 的区别主要是主控芯片、板级驱动和具体端口/网卡命名，不是“OpenCPU 对外挂”的架构区别；两者都应使用主机重启后的枚举、IP、路由和业务重连模型。

### 11.3 MPC：未验证的 AOV/MCU 协同记录

> **未验证，不得作为实施依据**：当前工作区中没有 MPC 仓库、对应原理图、MCU 固件或真机日志。以下只保留原文的待核线索，不再声称为“当前源码可确认”。

原文待核的主链路是：

```text
MPC 主机 Linux/AOV 服务
  ├─ /dev/ttyS2：主机 UART AT 控制通道
  ├─ QNETDEVCTL/AT：查询并控制模组数据业务
  ├─ usb0：主机 USB 数据网卡，按不同模组型号配置主机 IP/网关/DNS
  ├─ HAL/MCU：DTR、4GWAKEUP 引脚引用计数、4G VBUS 开关
  └─ net_wireless：4G/Wi-Fi 统一管理、心跳、平台唤醒 ACK
        ⇅
外挂 4G 模组
```

待获得 MPC 资料后，应逐项核对：实际构建目录、UART AT + USB `usb0`、DTR/4GWAKEUP/VBUS owner、AOV 任务协同、PDP/socket 保留、RI 至常供电 MCU 的连线，以及主机电源是否真正切断。验证前不能断言其已实现“主机完全断电、模组自主 socket 收到云命令后重新给主机上电”。

### 11.4 审核结论和使用限制

本文对**当前可访问仓库**的平台归类为：

```text
模组内 OpenCPU/OpenLinux：EC200A、AG35、EG25G 构建目标
外挂模组主机侧：         RK3506J、i.MX6ULL
待验证记录：             MPC
```

这条分类说明“应用在哪里、IP 接口在哪里、谁需要处理主机重启”；它不取代硬件确认。特别是远控项目还必须取得以下事实，才可把文档架构转为量产设计：模块型号和固件、USB composition、UART/USB/RI/DTR/PWRKEY/STATUS 连线、VBAT/Host VBUS 电源树、MCU 唤醒协议、以及运营商下 PDP/socket 的实际保活时间。

## 12. 外挂模组技术点详细展开

本节面向“产品应用运行在 RK/MCU/Linux 主控，4G 模组作为外设”的常见主流实现。理解顺序应固定为：**先电源和硬件信号，再控制面，再数据面，最后才是拨号状态机和业务**。跳过前两层直接写 AT 脚本，往往会得到“偶尔可用、休眠后不可恢复”的系统。

### 12.1 先画对电源域：VBAT、主机电源、USB VBUS 不是同一个东西

外接模组板级设计至少要区分下列供电对象：

| 对象 | 作用 | RK/主机掉电时的典型选择 | 常见误区 |
| --- | --- | --- | --- |
| 模组 VBAT/VDD | 供给基带、射频、SIM 和模组内部 AP | 远控保活时保持供电 | 误以为 USB 仍插着就等于模组仍有 VBAT |
| 主控主电源 | RK、DDR、eMMC、主机 Linux | 休眠方案中可被 MCU 关闭 | 主控掉电后任何 Linux 状态都不存在 |
| USB Host VBUS | 主机端口 5 V/Hub 电源 | 可随主机电源消失或被独立控制 | 把它当作模组 VBAT，或忽略它会导致 USB 重枚举 |
| 模组 I/O 电源域 | UART、RI、DTR、STATUS 等逻辑电平 | 常需要由常供电 MCU 保持可读 | 直接把 1.8 V 模组 IO 接到 3.3 V 主控 |
| 常供电 MCU/PMIC 域 | 唤醒决策、保持状态、控制电源 | 远控掉电设计中必须持续供电 | 让已断电的 RK 自己承担唤醒决策 |

远控产品推荐先确定唯一电源 owner：通常是常供电 MCU/PMIC。`modem_mng` 只能提出“需要冷启动/需要复位/禁止触碰会话”的请求，不能和 MCU 同时直接驱动 PWRKEY、RESET 或 VBAT。

#### 12.1.1 常见控制和状态信号

| 信号 | 通常方向/作用 | 在远控方案中的意义 | 必须确认的事项 |
| --- | --- | --- | --- |
| PWRKEY | 主控/MCU → 模组，按规定脉宽开/关机 | 仅冷启动或明确硬恢复时使用 | 有效电平、最小脉宽、VBAT 稳定后的等待时间、是否与关机复用 |
| RESET_N | 主控/MCU → 模组 | 强制复位，破坏性大 | 有效电平、最小复位时间、是否会丢失 USB composition/会话 |
| STATUS | 模组 → MCU/主控 | 判断模组是否已真正开机 | 是高电平有效还是开漏、与“已注册”不是同一状态 |
| RI | 模组 → MCU/主控 | 短信、来电、URC 或厂商事件可作为唤醒提示 | 事件映射、脉宽、是否在 PSM 下仍有效、是否接到 MCU wake pin |
| DTR | 主控/MCU → 模组 | 唤醒模组、睡眠控制或串口控制，语义因固件而异 | 极性、是否会触发睡眠、是否需要保持电平 |
| RTS/CTS | UART 双向 | 硬件流控，防止高波特率/URC 时丢数据 | 是否启用、交叉连接、电平转换 |
| USB D+/D- | 主机 ↔ 模组 | USB 枚举、AT 功能和网络功能 | Host/Device 角色、ESD、Hub/USB VBUS 与主机掉电关系 |

不要从名称推断电平或功能。例如 RI 不一定代表“任何网络包都触发”，DTR 不一定只是串口流控；必须以当前模块固件的硬件/AT 手册为准。

#### 12.1.2 UART、USB、PCIe 是物理/总线选择，不是拨号协议

三者首先回答“主机与模组如何交换字节或数据包”，并不直接回答“用什么命令拨号”或“主机怎样取得 IP”。一种物理互连可以承载多个功能，同一种控制协议也可能出现在不同物理互连上。

| 互连 | 主机/模组角色 | 上层常见功能 | 主机侧集成重点 | 适用倾向 |
| --- | --- | --- | --- | --- |
| UART | 双方串行收发；没有设备枚举 | AT、URC、模组 socket payload、PPP | 波特率、IO 电平、RTS/CTS、DTR/RI、串口 owner、数据模式切换 | MCU、小流量、备用控制和唤醒通道 |
| USB | 主控通常为 Host，模组为复合 Device | USB serial、ECM/NCM/RNDIS、QMI/MBIM、诊断/NMEA 等 | USB composition、VID:PID、interface/驱动绑定、VBUS、热插拔和重新枚举 | Linux 主控的常用控制与数据链路 |
| PCIe | 主控通常为 Root Complex，模组为 Endpoint | MHI 或厂商通道、WWAN、诊断和控制服务 | PERST#/REFCLK/WAKE#（若采用）、链路训练、枚举、DMA/IOMMU、MSI、中断和低功耗恢复 | 明确支持 PCIe 的高带宽模组和主控 |

必须避免以下等号：

```text
USB  ≠ ECM                   PCIe ≠ MHI ≠ QMI
UART ≠ AT                    /dev/ttyUSB* ≠ SoC UART
总线存在 ≠ 数据会话已建立     netdev 存在 ≠ 已取得可用 IP
```

USB 的一次成功启动通常经历“模组上电 → USB Device 枚举 → 各 interface 匹配驱动 → tty/control/netdev 节点出现”。任何一级重置都可能导致节点消失或编号变化。

PCIe/MHI 的典型启动则是“模组上电 → PCIe link training → Endpoint 被枚举 → MHI 控制器进入可用状态 → 控制/数据 channel 建立 → WWAN 等上层接口出现”。MHI 是在 PCIe 上管理设备状态和逻辑通道的传输框架，不等于蜂窝拨号协议；具体 channel 上可能承载 QMI、QRTR、IP 数据或厂商协议。恢复时要区分 PCIe 链路失败、MHI 状态失败、控制服务失败和 bearer 失败，不能统一归因于“拨号失败”。

### 12.2 控制面：谁命令模组，如何避免 AT 并发混乱

控制面的职责包括：检测模组、查询 SIM 和注册、配置 APN、建立/查询/释放 PDP、读取信号和小区、执行恢复，以及订阅 URC。常见承载有 UART AT、USB AT 串口、QMI、MBIM 和厂商 SDK。

#### 12.2.1 UART AT

```text
主控 UART TX ─────► 模组 UART RX
主控 UART RX ◄───── 模组 UART TX
主控 RTS ─────────► 模组 CTS       （若启用硬件流控）
主控 CTS ◄───────── 模组 RTS
主控 DTR ─────────► 模组 DTR       （若使用）
MCU wake pin ◄───── 模组 RI/STATUS  （若使用）
```

UART AT 的优点是硬件简单、调试直观、主机可以是 Linux 或 MCU。限制是同一字节流同时承载命令响应和 URC：

1. 必须只有一个 AT owner。RK 服务、MCU、调试 shell 和第三方守护进程不可并发写同一个 AT 口。
2. AT 层必须有串行事务锁、超时、终止响应识别和 URC 分发器；不能用简单 `echo > /dev/tty*` 作为常态实现。
3. 多行响应、回显、URC 与命令响应可能交错，解析器要按行和命令上下文归属，不能只查字符串是否含 `OK`。
4. 高速日志、GNSS NMEA、socket payload 或大量 URC 时要启用 RTS/CTS，并给接收缓冲和解析线程留足余量。
5. 若 RK 断电但 MCU 继续监听 UART，RK 上电后必须完成 AT owner 交接；否则 MCU 和 RK 会互相截获响应。

UART 仅作 AT 控制时，IP 数据仍可走 USB/PCIe 网卡；UART 本身不产生 `/dev/ttyUSB*`。SoC UART 在 Linux 上多为 `/dev/ttyS*`。

#### 12.2.2 USB AT 串口

模组作为 USB Device 接入 Linux Host 时，USB core 识别 VID:PID，串口类驱动（常见 `option`）绑定多个 interface，`tty` 层再创建 `/dev/ttyUSB0...N`。这些端口可能分别承载 AT、诊断、NMEA、Modem、ADB 等功能。

实现时应：

1. 以 USB 物理父路径和 VID:PID 找到候选复合设备；
2. 在同一父路径下收集 `ttyUSB*`/`ttyACM*` 与 netdev；
3. 对候选端口执行无副作用 AT 探测（如 `AT`、`ATI`），再读取 IMEI/固件确认身份；
4. 缓存“本次枚举”的 AT 口和网卡配对，但在 USB disconnect 后使缓存失效；
5. 绝不假设 `ttyUSB1`、`ttyUSB5` 在所有固件、所有开机时序中恒定。

`option/new_id` 仅是让内核尝试把某个 VID:PID 绑定到 option 驱动；它不表示 USB 枚举完成、不表示目标 `ttyUSB` 已出现，也不表示 AT 已能通信。

#### 12.2.3 QMI 和 MBIM 控制

QMI 和 MBIM 把“创建数据会话、拿 IP/DNS/MTU、查询 bearer 状态”等能力以二进制控制协议提供给主机，常使用 `/dev/cdc-wdmX`。它们与 AT 可以共存，但必须定义哪个服务拥有 data call：

* **QMI**：通常需要分配并释放 client ID、设置 data format、启动/停止 WDS data call、订阅状态；异常退出时 client 未释放可能耗尽模组资源。
* **MBIM**：主机打开 MBIM control channel，创建/查询 bearer；SIM、PIN、注册和 IP 配置通过 MBIM 消息读取。
* **AT 与 QMI/MBIM 协作原则**：AT 可以做模块特有配置和诊断，但不要同时用 AT `QNETDEVCTL` 与 QMI/MBIM 对同一 PDP/bearer 进行启停，避免所有权冲突。

QMI/MBIM 的优点是结构化状态、IPv4v6 和多 PDN 支持较好；代价是 Buildroot 内核、用户态库、设备权限、client 生命周期和异常恢复都要完整实现。

#### 12.2.4 AT 命令体系：命令响应、URC 与数据模式

AT 是文本式 modem 控制接口。它可以运行在 SoC UART、USB serial 或其他厂商通道上；“AT”本身不代表物理串口，也不自动创建 Linux IP 网卡。命令大致分为三类：

| 类别 | 作用 | 示例 | 使用边界 |
| --- | --- | --- | --- |
| 基础/识别 | 探测通道、关闭回显、读取型号和固件 | `AT`、`ATE0`、`ATI` | 只证明命令通道可交互，不证明 SIM/网络可用 |
| 3GPP 标准命令 | SIM、注册、PDP context 和动态参数 | `CPIN?`、`CEREG?`、`CGDCONT`、`CGACT?`、`CGCONTRDP` | 实际字段、制式和固件支持仍需核对对应 AT 手册 |
| 厂商扩展命令 | USB composition、网卡拨号、低功耗或专有功能 | 如移远部分固件的 `QNETDEVCTL` | 不能跨厂商、跨型号或跨固件照搬 |

一条 AT 事务不是简单的“一发一收”。解析器至少要同时处理：命令回显（若未关闭）、零到多行中间响应、最终结果码 `OK`/`ERROR`/`+CME ERROR`，以及任何时刻插入的 URC。可靠实现应做到：

1. 每个物理 AT 口只有一个事务调度 owner，其他组件通过 IPC 请求它执行命令；
2. 按命令定义识别前缀、字段和最终结果，而不是用“输出包含 `OK`”判断整个流程成功；
3. 将注册变化、来电、短信、掉线等 URC 独立分发，并允许 URC 与当前响应交错；
4. 为每条命令设置合适超时和可取消策略，超时后先恢复解析边界，再决定是否重试；
5. 把“AT 可用、SIM ready、已注册、bearer active”保存成不同状态，不用最后一次命令成功覆盖整个 modem 状态。

某些拨号或 socket 命令会让当前串口从 AT command mode 进入 online data mode。此时原始字节是 PPP 帧或业务 payload，不能继续当作 AT 行解析；返回 command mode 的 escape sequence、保护时间、DTR 行为和挂断方式必须依型号手册实现。若模组提供独立 AT 端口和数据端口，优先分开使用以降低状态冲突。

### 12.3 数据面：主控的 IP 包到底如何到达运营商

控制面回答“模组是否建立数据会话”，数据面回答“主控应用的 TCP/UDP 包如何出去”。两者必须分别验收。

#### 12.3.1 USB ECM/NCM/RNDIS

这些模式把模组向主机暴露为一个 Ethernet-like 网卡：

```text
RK 应用 socket
  → Linux TCP/IP + 路由
  → eth1 / usb0 / 其它 USB netdev
  → USB ECM/NCM/RNDIS 封装
  → 模组 PDP
  → 运营商网络
```

主机 IP 配置必须以型号手册和当前固件为准：

| 主机配置方式 | 过程 | 风险点 |
| --- | --- | --- |
| DHCP（ECM/NCM/RNDIS 常见） | 主机 `udhcpc`/网络管理器请求租约，获得主机侧地址、网关和 DNS | DHCP 服务可能慢、未启动；旧 DHCP 进程/持久化租约可能残留，重启后仍需验证 |
| 型号手册明确的主机侧静态配置 | 使用模组主机/LAN 侧规则指定的地址、前缀、网关和 DNS | 不能将 PDP/MT 地址与 ECM 主机地址默认视为同一地址；还要处理 IPv6、MTU 和路由 metric |

三者都能向 Linux 提供 Ethernet-like netdev，但 USB 数据封装和生态不同：

| 模式 | 核心特点 | Linux 常见驱动 | 工程判断 |
| --- | --- | --- | --- |
| ECM | CDC Ethernet 模型，主机像连接一块 USB 以太网卡 | `cdc_ether` | 实现直观；是否由模组做 NAT/路由、怎样启停 PDP 和怎样取 IP 仍由固件决定 |
| NCM | CDC 网络模型，可把多个网络数据报聚合进 USB transfer | `cdc_ncm` | 聚合可改善 USB 传输效率；主机与固件必须对 NCM 参数、MTU 和会话方式兼容 |
| RNDIS | 基于 USB 消息和数据封装的远程网络设备模型 | `rndis_host` | 常见于需要 Windows 生态兼容的产品；Linux 驱动、固件稳定性和安全维护状态要按目标平台评估 |

`eth1`、`usb0`、`enx...` 只是系统命名结果，不能用名称反推实际模式。应通过 sysfs 父设备、绑定驱动、USB interface class/subclass/protocol 和 VID:PID 确认它到底是 ECM、NCM 还是 RNDIS。固件切换 USB composition 后，AT 口数量、interface 编号、驱动和网卡名都可能变化。

**`AT+CGCONTRDP` 不是通用的 ECM 主机静态配置接口。** 3GPP TS 27.007 将其 `<local_addr and subnet_mask>` 和 `<gw_addr>` 定义为 MT 的 PDP 动态参数。在常见的 ECM 路由/NAT 形态中，模组可能持有运营商侧地址，主机则经 DHCP 获得另一个 LAN 地址。`CGCONTRDP` 可用于确认 PDP 和诊断 DNS/MTU，除非具体模组手册明确规定，不得把它的地址/网关写到 ECM 主机网卡。

ECM/NCM/RNDIS 接管已有 PDP 时，Linux 内核中的旧地址、路由和 socket 已随主机重启消失；持久化文件中的 DHCP lease 或 DNS 配置则可能仍存在，但不能盲信。正确动作是重新设置 link up，重新申请/校验主机侧地址，只清理属于该接口的陈旧路由，恢复 DNS，再验证到指定业务端点。

#### 12.3.2 PPP

PPP 的数据面通过串行字节流承载：

```text
主控 AT：配置并进入数据拨号模式
  → pppd 在串口上 LCP/PAP/CHAP/IPCP
  → Linux 创建 ppp0
  → IP/默认路由/DNS
```

PPP 建链并不是收到 `CONNECT` 就完成，主要阶段是：

1. AT 侧选定 APN/profile/CID 并进入数据模式；
2. LCP 协商链路参数；
3. 若网络要求，执行 PAP/CHAP 鉴权；
4. IPCP 和/或 IPv6CP 协商网络层参数；
5. `pppd` 创建并配置 `ppp0`，再由脚本或网络管理器处理路由与 DNS；
6. 链路退出时清理属于本会话的地址、路由、DNS 和进程状态。

实现必须确认模块规定的数据拨号命令、CID、认证方式、DTR/DCD 行为和转义/逃逸机制；不能把别的模块的拨号字符串照搬。`defaultroute`、`usepeerdns` 等行为要与系统现有网络管理策略协调，避免 PPP 脚本覆盖 Wi-Fi/以太网的默认路由或全局 DNS。PPP 更适合低带宽、低成本、USB 不可用的产品。它不适合以视频、远程桌面或大文件为主的 RK 远控上行。

PPP 故障应按阶段定位：没有 `CONNECT` 属于 AT/注册/data-call 问题；LCP 反复重试属于串口、流控或对端 PPP 问题；鉴权失败检查 APN/PAP/CHAP；IPCP 成功但公网不通才进入地址、路由、DNS 和运营商数据面排查。把所有退出都当作“信号差”会掩盖真实故障。

#### 12.3.3 QMI/MBIM 数据面

QMI/MBIM 管理的方案中，`/dev/cdc-wdmX` 一般是控制消息节点，`wwan0` 或驱动创建的其他 netdev 才是 IP 数据接口。二者来自同一个模组不代表任意 `cdc-wdmX` 都与任意 `wwanX` 配对；必须按 sysfs 父拓扑和驱动关系匹配。

典型生命周期是：

```text
发现并配对 control node + netdev
  → 打开 QMI client / MBIM control session
  → 查询 SIM、注册和 packet service
  → 设置 APN、IP family、认证和 profile/session
  → 启动 bearer，取得 IP/前缀/网关/DNS/MTU
  → 按驱动数据格式配置 netdev 和路由
  → 监听断链/注册/地址变化
  → 停止 bearer，释放 client/session 和主机网络状态
```

| 项目 | QMI | MBIM |
| --- | --- | --- |
| 控制模型 | 服务/client 模型；蜂窝数据常使用 WDS 等服务 | 标准化 command/response/indication 和 session 模型 |
| Linux 常见组合 | `qmi_wwan` + `/dev/cdc-wdmX` + WWAN netdev | `cdc_mbim` + `/dev/cdc-wdmX` + MBIM/WWAN netdev |
| 数据格式 | 常见 raw-IP；是否使用 802.3、QMAP/多路复用必须协商并匹配驱动 | 数据承载与 NCM 机制相关；多 session 映射按驱动/工具约定处理 |
| IP 来源 | bearer/current-settings 等控制结果 | IP configuration 消息/indication |
| 主要资源风险 | client ID、data format、mux 与 bearer 生命周期泄漏或不一致 | control session、transaction、session ID 和多路映射不一致 |

它们都不是 DHCP 的另一种名字。主机应根据控制协议返回的 IPv4/IPv6 参数和驱动 ABI 配置接口；只有具体实现明确要求 DHCP 时才运行 DHCP 客户端。QMI/MBIM 能与 AT 共存做诊断或专有配置，但同一 PDP/bearer 只能有一个连接管理 owner。

#### 12.3.4 PCIe MHI 数据面

PCIe MHI 常见于高带宽蜂窝模组。PCIe 提供总线、枚举和 DMA 能力，MHI 在其上管理设备状态、事件环和逻辑 channel，WWAN/QMI/QRTR 等再使用相应 channel。它们是上下层关系，不是三个同义名称。

主机集成需要同时处理：

* 板级 PCIe 供电、复位、参考时钟和唤醒信号时序；
* Root Complex、Endpoint、BAR、MSI/MSI-X、DMA mask 和 IOMMU 配置；
* PCIe link down/retrain、AER 错误和模组突然复位；
* MHI 状态机、channel 创建/销毁和上层控制服务重连；
* WWAN netdev 的地址、MTU、多路复用和路由生命周期；
* runtime PM、ASPM/L1 子状态与恢复时延的真机权衡。

PCIe/MHI 出现 netdev 后，主机 IP 的来源仍由其上层 modem 协议决定，不能照搬 ECM DHCP 或 PPP 流程。也不能仅凭“PCIe”宣称吞吐更高、CPU 更低或功耗更优；这些结论取决于链路代际、通道实现、拷贝/DMA 路径、包大小、驱动和电源策略，必须测量。

#### 12.3.5 以太网 MAC：RGMII/RMII/SGMII

部分具备 AP/Linux/网关能力的模块可通过 RGMII/RMII/SGMII 对外接 PHY 或主控 MAC 提供以太网式数据路径。这是板级 MAC/PHY/设备树问题，不能因为模块有 USB ECM 就假设它也支持 RGMII。

实现重点是：接口双方角色、参考时钟、IO 电压、PHY 供电、时序延迟、MAC 地址、设备树和驱动。它适合需要模块作为网关/路由器侧的设计，但对当前 RK3506 USB ECM 项目属于重新选硬件架构，不是软件改一个接口名即可替换。

#### 12.3.6 Linux 网卡、地址、路由和 DNS：必须逐层验证

Linux 出现一个 netdev，只表示内核注册了网络接口；它不等于链路、地址、路由或 DNS 已就绪。应分层观察：

| 状态层 | 要回答的问题 | 典型观察项 | 失败含义 |
| --- | --- | --- | --- |
| 设备存在 | 驱动是否创建接口，接口属于哪个模组 | sysfs 父拓扑、driver、ifindex、当前 netdev 名 | 枚举、驱动、composition 或硬件问题 |
| Link | 接口是否 administratively up，驱动是否报告 carrier | flags、operstate、carrier | 接口未启用或模组数据 function 未就绪；某些 WWAN 驱动没有传统以太网 carrier 语义 |
| 地址 | 主机是否有正确 IPv4/IPv6 地址和前缀 | address、scope、preferred/valid lifetime | DHCP、SLAAC、控制协议回填或静态配置失败 |
| 路由 | 到业务目标应从哪个接口、源地址和网关出去 | main/policy table、default route、metric、`ip route get` 结果 | 无路由、抢默认路由、源地址或策略规则错误 |
| DNS | 域名由谁解析，server 和 search domain 是否有效 | resolver owner、nameserver、接口级 DNS | 只能通 IP 不能通域名，或多网络互相覆盖配置 |

**DHCP 的职责**是在已经可传输二层/网络封装的接口上，为主机取得租约参数。典型 IPv4 流程是 Discover、Offer、Request、ACK；它不会替模组完成 SIM 检测、运营商注册或 PDP 建立。`udhcpc` 等客户端收到 ACK 后，通常由 hook 脚本真正写入地址、默认路由和 DNS，因此“DHCP 进程退出码成功”仍要检查脚本结果与最终内核状态。

DHCP 实现应保证同一接口只有一个 client owner，并区分初次申请、续租、NAK、lease 失效和接口重枚举。网卡消失后旧进程必须退出；新 ifindex 即使沿用旧名称，也应视为新设备重新验证。不要为修复单个 4G 接口而清空系统所有地址或所有默认路由。

**路由的职责**是决定目标流量的下一跳、出口接口和源地址。Linux 会先按 policy rule 选择查询哪些路由表，再在表内优先匹配最长前缀；相同前缀的候选路由还会受 metric 等属性影响。有多个 default route 不代表会自动按业务期望切换。4G 与以太网/Wi-Fi 并存时，应明确：

1. 哪个连接管理器拥有每条路由和规则；
2. 默认出口优先级、故障切换与恢复回切策略；
3. 是否需要独立 routing table、source-based policy routing 或业务绑定接口；
4. `rp_filter`、NAT/VPN 和防火墙是否允许非对称或多出口流量；
5. 删除会话时只删除带有本 owner 标识或精确匹配的配置。

**DNS 不在 Linux 路由表里。** DNS server 可能来自 DHCP、QMI/MBIM bearer 参数、PPP peer、IPv6 RA/DHCPv6 或静态配置。系统必须指定唯一 resolver 管理方式，例如由 `udhcpc` 脚本管理文件，或交给 resolvconf/systemd-resolved；多个脚本直接覆盖 `/etc/resolv.conf` 会造成“最后拨号的接口污染所有网络”。有条件时采用按接口/域的 DNS；嵌入式单文件方案至少要记录 owner，并在接口退出时只撤销自己的配置。

推荐按下面顺序验收，而不是只执行一次公网 `ping`：

```text
确认 netdev 与目标模组的父拓扑
  → 确认 link/driver/data function 状态
  → 确认 IPv4/IPv6 地址来源、前缀和 MTU
  → 对业务目标执行路由查询，核对出口/源地址/网关
  → 分别验证网关、指定公网 IP、DNS 查询
  → 验证 TLS 时间与证书、云端鉴权、订阅/心跳 ACK
```

这个顺序能把“驱动没起来”“DHCP 没地址”“默认路由走错”“只有 DNS 坏了”和“云端业务失败”分开，避免一律用模组复位掩盖主机网络配置错误。

#### 12.3.7 标准外挂方案的选型落点

| 产品约束 | 常见候选 | 选择前必须验证 |
| --- | --- | --- |
| MCU、低吞吐、引脚少 | UART AT socket 卸载或 PPP | 峰值负载、流控、证书/协议能力、串口 owner 和远程升级 |
| Linux、希望快速形成普通网卡 | USB ECM；按生态也可能选 NCM/RNDIS | 固件 composition、DHCP/NAT/IP passthrough、驱动和重枚举恢复 |
| Linux、需要结构化会话、多 PDN 或双栈管理 | USB QMI/MBIM + WWAN | 模组是否支持、data format、用户态工具、mux/session 和生命周期 |
| 高带宽且模组明确支持 | PCIe + MHI/配套上层协议 | 板级时序、内核驱动、DMA/IOMMU、低功耗和错误恢复 |
| 主机掉电仍需接收云端唤醒 | 模组内 socket/应用或常供电 MCU 通道，与上述数据面组合 | 运营商保活、功耗、安全认证、RI/GPIO 和电源 owner |

选型不是简单比较总线峰值带宽。还应把驱动成熟度、目标内核版本、模组固件认证、掉电恢复、运营商行为、CPU/内存占用、功耗、量产诊断和维护团队能力一并计入。

### 12.4 APN、注册、PDP、IP、业务连接：五层状态不能混为一谈

```text
SIM ready
  → 注册到运营商（CEREG/CGREG 等）
  → PDP/bearer 已建立
  → 主机 IP/路由/DNS 已配置
  → 云端 TLS/MQTT/TCP 业务已鉴权和收到心跳 ACK
```

每一步成功不代表下一步必然成功：

* SIM READY 不代表网络已注册；
* 已注册不代表 APN/PDP 成功；
* PDP 成功不代表主机网卡已有 IP；
* 有 IP、能 ping 公网 IP 不代表 DNS 正确、TLS 可用或远控服务已登录；
* TCP 已连不代表云端已认证或订阅控制主题成功。

项目设计上建议区分两个逻辑事件：`host_network_ready` 和 `cloud_ready`。这两个名称是**建议的跨组件契约**，不是行业标准；当前工作区只可见 `host_network_ready` 启动日志点，不能据此声称已实现完整事件接口或 `cloud_ready`。上层应以业务鉴权/订阅/心跳 ACK 完成作为“可控”条件。

### 12.5 冷启动、普通休眠恢复、主机掉电唤醒的实现差异

| 情形 | 可以保留什么 | 主控应该做什么 | 禁止的误判 |
| --- | --- | --- | --- |
| 冷启动 | 通常什么都不能假定 | PWRKEY/AT/注册/PDP/主机网络完整流程 | “没有 ttyUSB 就肯定只是慢枚举” |
| Linux suspend/resume | 内存和部分内核状态可能保留 | 处理 USB resume、同步模组状态、恢复必要驱动 | “必然等同一次冷启动” |
| 主机彻底掉电、模组常供电 | 模组注册/PDP/模组 socket **可能**保留 | MCU 给主机上电后重新枚举、检查 PDP、重建主机网络和业务 | “USB 仍物理连接，所以 IP/socket 还在” |
| 模组故障/断电 | 模组状态不可信 | MCU 受控复位模组，再完整拨号 | “接管失败就永远不能动模组” |

目标远控方案应从 MCU 获取启动原因、模组供电状态和唤醒来源；`boot_reason`、`modem_vbat` 和 `wake_source` 是本文**建议的契约字段**，尚未在当前代码中形成已验证的公共 API。Linux 仅凭 `ttyUSB`、网卡是否出现无法可靠推断启动情形。

### 12.6 远控掉电场景的建议职责划分

> 下图是目标设计，不是当前仓库已验收现状。落地前需定义 MCU↔RK 协议的枚举值、持久化时机、超时和兼容版本。

```text
常供电模组/MCU
  ├─ 模组保持注册/PDP，或按低功耗策略驻网
  ├─ 保持最小唤醒通道：模组 MQTT/socket、短信或来电
  ├─ 收到并校验合法远端唤醒消息
  ├─ RI/GPIO/UART 通知 MCU
  └─ MCU 置 boot_reason=REMOTE_WAKE，恢复 RK 电源

RK/modem_mng
  ├─ 读 MCU 状态，选择 cold_boot / remote_wake / modem_fault_recovery
  ├─ remote_wake：不做 PWRKEY/CFUN/PDP deactivate
  ├─ 重新枚举 USB/PCIe，严格检查模组和 PDP
  ├─ 恢复 Linux IP、路由、DNS
  └─ 远控应用重建 TLS/MQTT/TCP，报告 cloud_ready
```

安全上，唤醒请求不能仅靠任意短信或明文 socket payload。至少要有设备身份、消息认证、防重放计数/时间窗、频率限制和审计日志；否则“远程唤醒”会成为攻击者持续耗电的入口。

### 12.7 故障恢复：按影响从小到大升级

建议恢复阶梯如下，任一级成功就停止升级，并记录原因、次数和冷却时间：

```text
L0  重新检查 link/IP/route/DNS/业务心跳
L1  重启主机侧 DHCP 或重新回填 IP、路由、DNS
L2  只读 AT/QMI/MBIM 复核注册与 PDP；必要时按策略重建数据会话
L3  MCU 请求模组软件复位或 PWRKEY/RESET（确认会破坏业务）
L4  MCU 按电源树设计对模组 VBAT 执行受控断电恢复；USB VBUS 只用于 USB 侧恢复，不得等同于模组 VBAT 断电
L5  仅在系统其余功能也异常时重启主机
```

远控 `REMOTE_WAKE` 的起点应是 L0/L1/L2；不可一失败就 L3。相反，连续失败且 AT 无响应/PDP 已确认丢失时，也不能永远停在“保护会话”的无效循环，应让 MCU 执行受控恢复。

### 12.8 量产排障与验收清单

#### 每次启动的结构化日志

```text
boot_reason / modem_vbat / wake_source
USB(or PCIe) topology / VID:PID / AT port / netdev
SIM / registration / PDP-bearer parsed state
host IP / gateway / DNS / route metric / IPv4v6
cloud authentication / heartbeat ACK
each stage duration / recovery level / failure reason
```

#### 必测场景

1. 模组和主机同时冷启动；
2. 主机掉电、模组常供电后远程唤醒；
3. USB 枚举慢、Hub reset、`ttyUSB` 编号变化；
4. PDP 保留但 DHCP/IP/DNS 丢失；
5. PDP 已丢失、SIM 未就绪、运营商注册被拒绝；
6. AT 无响应、USB 网卡消失、模组软件卡死；
7. 连续多次唤醒、云端重复命令、网络抖动；
8. IPv4、IPv6、IPv4v6，私有 APN 和漫游；
9. 远控业务已连接但业务心跳/订阅失败；
10. P50/P95 的 T0（MCU 上电）到 `cloud_ready` 时延。

只有这些场景通过，才能把“某次日志显示拨号成功”提升为“外挂模组远控链路可量产”。

### 12.9 从架构知识到实施指南还需补齐的专项

本文可以解释架构，但要对某个 SKU/固件变成可执行的实施指南，还必须建立下列项目专项记录：

| 专项 | 必须决定/验证的内容 |
| --- | --- |
| 地址模型 | 模组是路由/NAT、IP passthrough 还是 raw-IP；主机地址的唯一权威来源 |
| IPv6 | SLAAC、DHCPv6 或控制协议下发；前缀、默认路由、DNS 和 IPv4v6 降级策略 |
| DNS 所有权 | 是由 `udhcpc` 脚本、resolvconf/systemd-resolved、应用或固定配置管理；禁止多个 owner 互相覆盖 |
| MTU/MSS | 从 bearer 取得 MTU，验证 PMTUD、VPN/TLS 和必要时的 TCP MSS 处理，避免“小包通、大包卡死” |
| 多 PDN/多路复用 | profile/CID、QMAP/MBIM session、子接口、路由表和生命周期映射 |
| 多网并存 | 4G/Wi-Fi/以太网的 route metric、policy routing、`rp_filter`、DNS 切换和会话迁移 |
| SIM/eSIM/漫游 | SIM 选择、PIN、APN/认证、PLMN 策略、漫游和定向卡限制 |
| 时间与 TLS | 无 RTC/冷启动时的可信时间来源、证书链、私钥保护和证书轮换 |
| 固件与 OTA | 固件/SDK/USB composition 兼容矩阵、可回滚升级、配置保留和失败恢复 |
| 安全和滥用防护 | 远程唤醒消息认证、防重放、限频、调试口权限、日志脱敏和审计 |
| 运营商/量产 | NAT 穿透、空闲超时、PSM/eDRX、频段/认证、流量与功耗，以及批量设备的 P50/P95/P99 |

## 13. 证据状态与参考资料

### 13.1 当前验证矩阵

| 结论 | 当前证据 | 状态 | 发布时的允许表述 |
| --- | --- | --- | --- |
| OpenCPU/QuecOpen 与外部主控的主要区别 | 移远 QuecOpen 资料 + 本仓库构建分支 | 已静态核对 | 可作为架构知识，不推广到任意厂商的具体 API |
| EC200A/AG35 当前代码等待 `ccinet0` | `ec200a/main.cpp`、`ag35/main.cpp` | 已确认（代码） | 只可说明当前代码预期；AG35 真机接口名待复核 |
| EG25G 使用 MCM/QMI/DSI 且监测 `rmnet_data*` | `CMakeLists.txt`、`eg25/nw/nw.c` | 已确认（代码） | 可用于本仓库 EG25G 附录 |
| RK3506J 是 USB AT + 主机 USB netdev | `rk3506j/device/`、`rk3506j/dial/` | 已确认（代码） | 具体 ECM/RNDIS composition、VID:PID 和端口顺序必须按固件实测 |
| RK3506J 在主机重启后保护已有模组会话 | 当前未提交的 `event_code=13` 接管、软拨与 PWRKEY 恢复代码 | 工作区方案 | 只可写“待真机验收” |
| ECM 主机可直接使用 `CGCONTRDP` 地址 | 与 3GPP 定义和常见 ECM DHCP/NAT 拓扑冲突 | 不得通用化 | 只能写“用于 PDP 诊断；主机静态配置须有型号专用证据” |
| MPC AOV/MCU 拓扑 | 当前环境无仓库/原理图/日志 | 未验证 | 仅作待核线索，不得作为结论 |

已有内部记录曾出现 EC200A 固件 `EC200ACNTAR02A04M2G_OCPU` 和 EG25G 固件 `EG25GGBR07A08M2G`，但这不能自动代表当前 RK3506J/AG35 目标机或所有 SKU。引用真机结论时应在表格中另行记录测试日期、IMEI 脱敏标识、SKU、固件、运营商、USB composition 和原始日志路径。

### 13.2 规范和官方资料

1. [3GPP TS 27.007 / ETSI TS 127 007：UE AT 命令集](https://www.etsi.org/deliver/etsi_ts/127000_127099/127007/18.08.00_60/ts_127007v180800p.pdf)：`CGDCONT`、`CGCONTRDP`、注册和 PDP/PDN 相关术语的首要标准依据。
2. [USB-IF Communications Device Class 文档库](https://www.usb.org/documents?category%5B0%5D=49&items_per_page=All&order=name&search=&sort=asc)：CDC、NCM、MBIM 等 USB 功能的规范入口。
3. [Linux Kernel CDC MBIM 文档](https://docs.kernel.org/networking/cdc_mbim.html)：MBIM 控制/数据通道、WWAN netdev 和多 session 映射。
4. [Linux Kernel MHI 文档](https://docs.kernel.org/mhi/mhi.html)：MHI 在高速外设总线上的状态、逻辑 channel、传输环和驱动模型。
5. [Quectel UMTS/LTE/5G Linux USB Driver User Guide V3.1](https://www.quectel.com/content/uploads/2024/02/Quectel_UMTS_LTE_5G_Linux_USB_Driver_User_Guide_V3.1.pdf)：USB 复合设备、serial/network 驱动的厂商资料。
6. [Quectel LTE Cat 1 白皮书](https://quectel.com/content/uploads/2024/03/Quectel_LTE_Cat_1_white_paper.pdf)：QuecOpen Linux 平台及标准模组/模组内应用模式的概念资料。
7. [Quectel EC200A Series QuecOpen Hardware Design V1.1](https://developer.quectel.com/en/wp-content/uploads/sites/2/2024/11/Quectel_EC200A_Series_QuecOpen_Hardware_Design_V1.1.pdf)：EC200A QuecOpen 的供电、PWRKEY、USB 和硬件接口依据。
8. [Quectel EC200A 产品页](https://www.quectel.com/product/lte-ec200a-series/)：当前可用的型号资料和文档版本入口。
9. [IETF RFC 1661：Point-to-Point Protocol](https://www.rfc-editor.org/info/rfc1661/)：PPP 封装、LCP、鉴权阶段和网络控制协议阶段的标准依据。
10. [IETF RFC 2131：Dynamic Host Configuration Protocol](https://www.rfc-editor.org/info/rfc2131/)：DHCP 客户端状态、Discover/Offer/Request/ACK、租约与续租行为的标准依据。

### 13.3 辅助案例（不代替型号手册）

* [Quectel 社区：EC200A 外接 Linux 时的 ECM/`QNETDEVCTL`/DHCP 示例](https://forumschinese.quectel.com/t/topic/12152)。
* [Quectel 社区：EG25 ECM 的 PDP 地址与主机 DHCP 步骤](https://forums.quectel.com/t/eg25-in-ecm-mode-under-linux/28551)。

社区帖子只用于展示真实现象和反例，不能超过当前 SKU/固件手册和实测结果的证据等级。
