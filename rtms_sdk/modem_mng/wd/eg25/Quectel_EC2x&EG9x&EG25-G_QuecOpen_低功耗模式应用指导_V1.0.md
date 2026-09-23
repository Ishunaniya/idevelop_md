# EC2x&EG9x&EG25-G 系列 QuecOpen 低功耗模式应用指导

---

## 文档信息

| 项目 | 内容 |
|------|------|
| 文档标题 | EC2x&EG9x&EG25-G 系列 QuecOpen 低功耗模式应用指导 |
| 模块系列 | LTE Standard 模块系列 |
| 版本 | 1.0 |
| 日期 | 2021-05-08 |
| 状态 | 受控文件 |
| 版权 | Copyright © Quectel Wireless Solutions Co., Ltd. 2021 |

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|------|------|------|----------|
| - | 2020-08-17 | Zoffy YU | 文档创建 |
| 1.0 | 2021-05-08 | Zoffy YU | 受控版本 |

---

## 目录

1. 引言
   - 1.1 适用模块
2. 低功耗模式
   - 2.1 低功耗模式状态图
   - 2.2 低功耗休眠和唤醒方案
3. 低功耗模式相关 API
   - 3.1 头文件
   - 3.2 函数详解
     - 3.2.1 QL_Lpm_Init
       - 3.2.1.1 QL_Lpm_Handler_T
     - 3.2.2 QL_Lpm_Deinit
     - 3.2.3 Ql_Autosleep_Enable
     - 3.2.4 Ql_SLP_WakeLock_Create
     - 3.2.5 Ql_SLP_WakeLock_Lock
     - 3.2.6 Ql_SLP_WakeLock_Unlock
     - 3.2.7 Ql_SLP_WakeLock_Destroy
4. 耗流
5. 常见问题和注意事项
   - 5.1 模块无法进入休眠模式
   - 5.2 模块休眠时非预期唤醒
   - 5.3 休眠耗流偏高
6. 附录：参考文档和术语缩写

---

## 图片索引

| 图片编号 | 图片名称 | 页码 |
|----------|----------|------|
| 图 1 | 低功耗模式状态图 | 7 |
| 图 2 | 低功耗休眠和唤醒方案 | 8 |
| 图 3 | KEYSIGHT 耗流图 | 18 |

---

## 表格索引

| 表格编号 | 表格名称 | 页码 |
|----------|----------|------|
| 表 1 | 适用模块 | 6 |
| 表 2 | 低功耗休眠和唤醒方案事件介绍 | 8 |
| 表 3 | 常见 NAS 唤醒消息 | 17 |
| 表 4 | 参考文档 | 19 |
| 表 5 | 术语缩写 | 19 |

---

## 1 引言

移远通信 LTE Standard EC2x 系列、EG9x 系列和 EG25-G 模块支持 QuecOpen® 方案。QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。有关 QuecOpen® 的详细信息，请参考文档 [1]。

移远通信基于 Linux 系统的 AutoSleep 和 WakeLock 技术开发了一套 QuecOpen® 低功耗休眠唤醒方案，可分别实现模块的休眠与唤醒功能。

- **AutoSleep**：使能 AutoSleep 后，模块始终有冻结进程、挂起外围设备以及使 CPU 进入休眠的趋势。
- **WakeLock**：如果 Kernel 或者任意 APP 持有一个或多个 WakeLock，将会抑制模块进入休眠。

### 1.1 适用模块

**表 1：适用模块**

| 模块系列 | 模块 |
|----------|------|
| EC2x | EC25 系列 |
| EC2x | EC21 系列 |
| EC2x | EC20 R2.1 |
| EC2x | EC20-CN |
| EG9x | EG95 系列 |
| EG9x | EG91 系列 |
| EG25-G | EG25-G |

---

## 2 低功耗模式

### 2.1 低功耗模式状态图

低功耗模式状态图描述了 QuecOpen 模块与客户 MCU 之间的休眠/唤醒状态交互关系：

```
模块状态：模块被唤醒 <---> 模块挂起
MCU 状态：MCU 被唤醒 <---> MCU 挂起

交互引脚：
  - wakeup_out pin：QuecOpen 模块 → 客户 MCU（rising 沿唤醒 MCU）
  - wakeup_in pin：客户 MCU → QuecOpen 模块（falling 沿进入休眠，rising 沿唤醒模块）

触发模块唤醒的事件：voice（语音）、sms（短信）、data（数据）
```

**图例说明：**
- 蓝色方框：QuecOpen 模块和 MCU 状态
- 粉色方框：唤醒 QuecOpen 模块的事件
- 橙色方框：挂起 QuecOpen 模块的事件

**状态转换说明：**
- 当模块收到 voice/sms/data 事件时，模块从挂起状态被唤醒
- 模块唤醒后，通过 wakeup_out 引脚的上升沿通知 MCU
- MCU 通过 wakeup_in 引脚的下降沿告知模块可以进入休眠
- MCU 通过 wakeup_in 引脚的上升沿告知模块保持唤醒

### 2.2 低功耗休眠和唤醒方案

本节介绍 QuecOpen 低功耗模式方案，包含 wakeup_in/wakeup_out/voice/sms/data 事件的 ql_lpm 功能总结。

**方案架构：**

```
QuecOpen 模块（事件 A/B/C/D/E） <---wakeup_out---> 客户 MCU（事件 F/G）
                                 <---wakeup_in---
```

**表 2：低功耗休眠和唤醒方案事件介绍**

| 状态 | 事件 | 描述 |
|------|------|------|
| 初始状态 | D | wakeup_out 引脚输出高电平，使 MCU 对应引脚的初始状态为高电平。 |
| 初始状态 | G | MCU 引脚输出高电平，使 wakeup_in 引脚的初始状态为高电平。 |
| 模块休眠 | E | 当 wakeup_in 引脚收到下降沿时，APP 将释放 WakeLock。当模块进入休眠模式，wakeup_out 引脚将自动输出低电平反馈给 MCU。 |
| 模块唤醒 | A/B/C/E | 通过这四个唤醒事件唤醒模块时，APP 需要锁定 WakeLock。 |
| 模块唤醒 | D | 当 A、B、C、E 事件发生时，控制 wakeup_out 引脚输出高电平，唤醒/反馈给 MCU。 |

**备注：**
1. D 事件的示例请参考：`ql-ol-extsdk/example/low_power_consume_app/example_lpm.c`
2. A、B、C、E 事件的示例请参考：`ql-ol-extsdk/example/low_power_consume_app/example_lpm_all.c`

---

## 3 低功耗模式相关 API

### 3.1 头文件

API 函数头文件 `ql_lpm.h` 位于 `ql-sdk/ql-ol-extsdk/example/wakelock/main.c` 目录下。若无特别说明，本文档所提到的头文件均在该目录下。

### 3.2 函数详解

#### 3.2.1 QL_Lpm_Init

调用该函数初始化低功耗休眠功能，模块将自动加载 ql_lpm 驱动，同时监听 wakeup_in 引脚状态变化并通知给 `ql_lpm_handler`。

**函数原型**

```c
int QL_Lpm_Init(QL_Lpm_Handler_T ql_lpm_handler, QL_Lpm_Cfg_T *ql_lpm_cfg)
```

**参数**

| 参数名 | 方向 | 描述 |
|--------|------|------|
| `ql_lpm_handler` | [In] | 用户回调。当 wakeup_in 引脚电平发生变化时将触发该回调函数。 |
| `ql_lpm_cfg` | [In] | 用户参数数据结构。支持客户配置引脚以及触发方式；建议传入 NULL（使用默认的引脚和触发方式）。 |

**返回值**

| 返回值 | 描述 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

---

##### 3.2.1.1 QL_Lpm_Handler_T

当 wakeup_in 引脚的电平发生变化时将触发该回调函数（`ql_lpm_handler`），可通过 `QL_Lpm_Init` 注册回调函数。

**函数原型**

```c
typedef void (*QL_Lpm_Handler_T)(ql_lpm_edge_t lpm_edge)
```

**参数**

| 参数名 | 方向 | 描述 |
|--------|------|------|
| `lpm_edge` | [In] | 底层上报的 wakeup_in 电平边沿变化 |

**lpm_edge 枚举值：**

| 枚举值 | 含义 |
|--------|------|
| `E_QL_LPM_FALLING` | 下降沿 |
| `E_QL_LPM_RISING` | 上升沿 |

**返回值**

无

---

#### 3.2.2 QL_Lpm_Deinit

调用该函数注销低功耗休眠功能，卸载 ql_lpm 内核模块，并注销句柄。

**函数原型**

```c
int QL_Lpm_Deinit()
```

**参数**

无

**返回值**

| 返回值 | 描述 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

---

#### 3.2.3 Ql_Autosleep_Enable

调用该函数使能 AutoSleep，使能后系统会在满足条件后自动进入休眠。

**函数原型**

```c
int Ql_Autosleep_Enable(char enable)
```

**参数**

| 参数名 | 方向 | 描述 |
|--------|------|------|
| `enable` | [In] | 是否使能 AutoSleep 功能 |

**enable 参数值：**

| 值 | 描述 |
|----|------|
| 1 | 使能 AutoSleep |
| 0 | 禁用 AutoSleep（一般不传入 0；如果需要保持唤醒，请调用 `Ql_SLP_WakeLock_Lock` 进行唤醒后的锁定。） |

**返回值**

| 返回值 | 描述 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

**备注：**

> `Ql_Autosleep_Enable` 使能 AutoSleep 后，不建议使用 `Ql_Autosleep_Enable(0)` 来保持模块唤醒，而应该调用 `Ql_SLP_WakeLock_Lock` 使模块保持唤醒，`Ql_SLP_WakeLock_Unlock` 可以释放 WakeLock，放弃休眠锁定。详情请参考第 **3.2.5 章** 和 **3.2.6 章**。系统休眠后进程会被冻结，唤醒后进程会继续运行。

---

#### 3.2.4 Ql_SLP_WakeLock_Create

当前进程可调用该函数创建 WakeLock。最多可以创建 512 个 WakeLock。

**函数原型**

```c
int Ql_SLP_WakeLock_Create(const char *name, size_t len)
```

**参数**

| 参数名 | 方向 | 描述 |
|--------|------|------|
| `name` | [In] | WakeLock 名称。可在 `/sys/kernel/debug/wakeup_sources` 中体现。 |
| `len` | [In] | WakeLock 名称的长度。最大长度为 28 个字符。 |

**返回值**

| 返回值 | 描述 |
|--------|------|
| WakeLock 描述符 | 函数执行成功 |
| -1 | 函数执行失败；请通过 errno 查询错误码信息 |

---

#### 3.2.5 Ql_SLP_WakeLock_Lock

调用该函数锁定已创建的 WakeLock，锁定后模块无法进入休眠模式。

**函数原型**

```c
int Ql_SLP_WakeLock_Lock(int fd)
```

**参数**

| 参数名 | 方向 | 描述 |
|--------|------|------|
| `fd` | [In] | WakeLock 描述符。 |

**返回值**

| 返回值 | 描述 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

---

#### 3.2.6 Ql_SLP_WakeLock_Unlock

调用该函数解锁指定的 WakeLock。如果系统内其他 WakeLock 都已被释放，且 APP 已使能 AutoSleep 功能，那么模块将会进入低功耗休眠模式。

**函数原型**

```c
int Ql_SLP_WakeLock_Unlock(int fd)
```

**参数**

| 参数名 | 方向 | 描述 |
|--------|------|------|
| `fd` | [In] | WakeLock 描述符。 |

**返回值**

| 返回值 | 描述 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

---

#### 3.2.7 Ql_SLP_WakeLock_Destroy

调用该函数注销 WakeLock。

**函数原型**

```c
int Ql_SLP_WakeLock_Destroy(int fd)
```

**参数**

| 参数名 | 方向 | 描述 |
|--------|------|------|
| `fd` | [In] | WakeLock 描述符。 |

**返回值**

| 返回值 | 描述 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

---

## 4 耗流

EC2x 系列、EG9x 系列和 EG25-G QuecOpen 模块在不同网络制式下的耗流情况详见各模块的硬件设计手册文档（文档 [2]、[3]、[4]、[5]、[6]、[7] 和 [8]）。

---

## 5 常见问题和注意事项

### 5.1 模块无法进入休眠模式

如果客户在 APP 中已经释放了 WakeLock，但是模块仍然没有进入休眠模式，请使用如下命令查看当前系统持有的 WakeLock：

```bash
awk '$6 != 0 {print $1" "$6}' /sys/kernel/debug/wakeup_sources
```

**情况一：输出 `msm_otg`**

说明 USB_VBUS 引脚处于高电平状态，需要拉低才能休眠。

```
~ # awk '$6 != 0 {print $1" "$6}' /sys/kernel/debug/wakeup_sources
name  active_since
msm_otg  77464
~ #
```

**情况二：输出 `DATA1`**

说明 GNSS 数据在抑制休眠，需要调用 `QL_LOC_Stop_Navigation()` 禁用 GNSS 功能后方可进入休眠模式。

```
~ # awk '$6 != 0 {print $1" "$6}' /sys/kernel/debug/wakeup_sources
name  active_since
DATA1  448
msm_otg  1105442
~ #
```

**情况三：输出 `bam_dmux_wakelock`**

则说明在 rmnet_data 网口上有数据交互，需要停止数据的交互后方可进入休眠模式。

```
~ # awk '$6 != 0 {print $1" "$6}' /sys/kernel/debug/wakeup_sources
name  active_since
bam_dmux_wakelock  1714
msm_otg  226735
~ #
```

**情况四：**  
如果客户正在使用 Wi-Fi 功能，请在进入休眠模式调用 `ql_wifi_disable()` 禁用 Wi-Fi 功能。

**情况五：**  
如果客户正在使用以太网，请在进入休眠模式前调用 `ql_sgmii_disable()` 禁用以太网。

> **备注：** 仅 EC21 系列、EC25 系列、EC20 R2.1 和 EG25-G QuecOpen 模块支持 Wi-Fi 和以太网功能。

### 5.2 模块休眠时非预期唤醒

检查模块是否出现休眠时非预期唤醒，可以使用功耗测试仪来捕提耗流图；也可以使用模块的调试串口捕提日志信息，通过日志信息对模块唤醒原因进行分析，确认是否为非预期唤醒，并针对性地处理。

**诊断步骤：**

首先，请依次执行如下命令：

```bash
~ # echo 1 > /sys/module/printk/parameters/perf_mode_console
~ # echo 1 > /sys/module/msm_show_resume_irq/parameters/debug_mask
~ # echo 0x2 > /sys/module/ipc_router_core/parameters/debug_mask
```

然后，调用相关 API 使模块进入休眠模式并观察日志信息。若打印如下日志，表示模块可能出现了非预期唤醒：

```
[  113.386694] gic_show_resume_irq: 57 triggered qcom,smd-modem
[  113.386694] gic_show_resume_irq: 200 triggered qcom,smd-rpm
[  113.386694] resume cycles:       2542257600
[  113.388512] [IPCRTR]  CLI RX Len:0xd T:0x1 CF:0x0 SVC:<0x3:0x1> SRC:<0x3:0x11> DST:<0x1:0x43> DATA: 51000b04 13000600
[  113.388520] PM: noirq resume of devices complete after 0.975 msecs
[  113.389994] PM: early resume of devices complete after 1.088 msecs
```

**情况一：打印 `gic_show_resume_irq: 57 triggered qcom,smd-modem`**
- **57** 表示 Modem 通过 SMD 向 AP 侧发送 QMI 消息；
- 若为 **58**，则表示有 IP 报文发送到 AP 侧。

**情况二：打印 `[IPCRTR]  CLI RX Len:0xd T:0x1 CF:0x0 SVC:<0x3:0x1> SRC:<0x3:0x11> DST:<0x1:0x43> DATA: 51000b04 13000600`**

- **CLI RX** 表示 QMI 客户端接收到消息，可能为响应，也可能为指示，此时需要通过 **DATA** 字段进行区分。
- **SVC:<0x3:0x1>**：0x3 代表 QMI MSG ID：NAS。
- **DATA: 51000b04 13000600**：这个字段逆序看 13000600 51000b04，其中，04 字段表示指示，51 字段表示 `MI_NAS_SIG_INFO_IND`，即因为信号强度的改变而上报状态变化的 QMI MSG。

**表 3：常见 NAS 唤醒消息**

| NAS 唤醒消息 | 字段 | 描述 |
|-------------|------|------|
| QMI_NAS_ERR_RATE_IND | 0x0053 | 提供具体的 RAT 错误率信息 |
| QMI_NAS_SIG_INFO_IND | 0x0051 | 提供信号强度状态变化信息 |
| QMI_NAS_RF_BAND_INFO_IND | 0x0066 | 上报当前射频频段信息 |
| QMI_NAS_SYS_INFO_IND | 0x004E | 指示系统信息变化 |
| QMI_NAS_SERVING_SYSTEN_IND | 0x0024 | 指示当前服务系统注册状态和/或无线电技术的变化（不推荐使用） |

### 5.3 休眠耗流偏高

如果模块休眠后平均耗流依然偏高，可能有三类原因：非预期唤醒、底电流、RF 因素（网络制式、频段等）。排查步骤如下：

**步骤一：** 使用模块的调试串口捕提日志信息，首先检查模块是否出现非预期的唤醒。详情请参见第 **5.2 章**。

**步骤二：** 若步骤一未发现异常的唤醒，则需要检查底电流是否偏高。执行 `AT+CFUN=0` 关闭射频干扰，观察底电流是否偏高（正常为 1.5 mA 以下）；若底电流偏高，需要检查模块休眠时引脚是否与外部电路形成漏电回路导致底电流偏高。关于该 AT 命令的详细信息，可参考文档 [9]。

**步骤三：** 若步骤一和步骤二检查均为正常，但是休眠耗流仍然偏高，请使用功耗测试仪捕提精确的耗流图查看休眠耗流偏高的原因（以 KEYSIGHT 为例）：

KEYSIGHT 耗流图可通过功耗测试仪工具软件的 Data Logger 功能捕提，主要测量参数说明：
- **A-V1**：电压测量通道
- **A-I1**：电流测量通道
- **Measurements Between Markers**：两个 Marker 之间的统计数据，包含 Avg（平均值）、Min（最小值）、Max（最大值）、Peak to Peak（峰峰值）、Charge/Energy（电荷量/能量）等

---

## 6 附录：参考文档和术语缩写

### 参考文档

**表 4：参考文档**

| 序号 | 文档名称 | 描述 |
|------|----------|------|
| [1] | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_快速开发指导 | 适用于 EC2x 系列、EG9x 系列和 EG25-G QuecOpen 的快速开发指导 |
| [2] | Quectel_EC21 系列_QuecOpen_硬件设计手册 | EC21 系列 QuecOpen 硬件设计手册 |
| [3] | Quectel_EC25 系列_QuecOpen_硬件设计手册 | EC25 系列 QuecOpen 硬件设计手册 |
| [4] | Quectel_EC20_R2.1_QuecOpen_硬件设计手册 | EC20 R2.1 QuecOpen 硬件设计手册 |
| [5] | Quectel_EC20-CN_QuecOpen_硬件设计手册 | EC20-CN QuecOpen 硬件设计手册 |
| [6] | Quectel_EG91 系列_QuecOpen_硬件设计手册 | EG91 系列 QuecOpen 硬件设计手册 |
| [7] | Quectel_EG95 系列_QuecOpen_硬件设计手册 | EG95 系列 QuecOpen 硬件设计手册 |
| [8] | Quectel_EG25-G_QuecOpen_硬件设计手册 | EG25-G QuecOpen 硬件设计手册 |
| [9] | Quectel_EC2x&EG9x&EG2x-G&EM05_Series_AT_Commands_Manual | 适用于 EC2x 系列、EG9x 系列、EG2x-G 和 EM05 系列的 AT 命令手册 |

### 术语缩写

**表 5：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|------|----------|----------|
| AP | Access Point | 无线接入点 |
| API | Application Program Interface | 应用程序接口 |
| APN | Access Point Name | 接入点 |
| CPU | Central Processing Unit | 中央处理器 |
| DRX | Discontinuous Reception | 不连续接收 |
| GNSS | Global Navigation Satellite System | 全球导航卫星系统 |
| IoT | Internet of Things | 物联网 |
| LPM | Low Power Mode | 低功耗模式 |
| LTE | Long Term Evolution | 长期演进 |
| MCU | Micro Controller Unit | 微控制单元 |
| NAS | Non-Access Stratum | 非接入层 |
| OTA | Over The Air | 空中下载 |
| PSM | Power Saving Mode | 省电模式 |
| QMI | Qualcomm MSM Interface | 高通 MSM 接口 |
| RF | Radio Frequency | 射频 |
| SMD | Shared Memory Driver | 共享内存驱动 |
| UART | Universal Asynchronous Receiver/Transmitter | 通用异步收发传输器 |
| USB | Universal Serial Bus | 通用串行总线 |
| VBUS | Voltage Bus | 总线电压 |

---

*本文档版权属于上海移远通信技术股份有限公司，任何人未经我司允许而复制转载该文档将承担法律责任。*
