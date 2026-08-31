# EC200A-CN(TA) QuecOpen 低功耗模式应用指导

> **文档版本**：V1.0.0 Preliminary  
> **发布日期**：2022-07-19  
> **适用平台**：EC200A-CN(TA)（QuecOpen 模式）  
> **关键字**：AutoSleep、WakeLock、LPM、休眠唤醒、低功耗

---

## 目录

- [1. 引言](#1-引言)
- [2. 低功耗模式](#2-低功耗模式)
  - [2.1 低功耗模式介绍](#21-低功耗模式介绍)
  - [2.2 GPIO 休眠唤醒方案](#22-gpio-休眠唤醒方案)
    - [2.2.1 sleep_sys_ind](#221-sleep_sys_ind)
    - [2.2.2 wakeup_out](#222-wakeup_out)
    - [2.2.3 wakeup_in](#223-wakeup_in)
  - [2.3 低功耗模式 GPIO 配置](#23-低功耗模式-gpio-配置)
- [3. 低功耗模式 API](#3-低功耗模式-api)
  - [3.1 头文件](#31-头文件)
  - [3.2 函数概览](#32-函数概览)
  - [3.3 函数详解](#33-函数详解)
    - [3.3.1 WakeLock 相关 API](#331-wakelock-相关-api)
- [4. 常见问题处理](#4-常见问题处理)
  - [4.1 模块无法进入休眠](#41-模块无法进入休眠)
  - [4.2 唤醒原因排查](#42-唤醒原因排查)
- [附录 参考文档及术语缩写](#附录-参考文档及术语缩写)

---

## 1. 引言

移远通信 EC200A-CN(TA) 模块支持 QuecOpen® 方案。QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。

本文档主要介绍基于 Linux 的 **AutoSleep** 和 **WakeLock** 技术开发的一套 QuecOpen® 休眠唤醒框架。

- **AutoSleep**：AutoSleep 从 Android wakelocks 集中演化而来，基于唤醒源实现，达到 Opportunistic Suspend 的效果，意思是在"系统没有事情在做"时，将自动切换到低功耗状态。

- **WakeLock**：唤醒锁机制。"系统没有事情在做"的判断依赖于 wakeup events framework。AutoSleep 开启后，只要系统没有正在处理和新增的唤醒源，就尝试将系统挂起；若挂起的过程中有事件产生，则会进入休眠唤醒流程。

### 休眠唤醒流程

```
开机
  │
  ▼
使能 AutoSleep
  │
  ▼
系统是否有 WakeLock？
  │ 是
  │──────────────────────→ (继续持有，不挂起)
  │ 否
  ▼
进入挂机（Suspend）
  │
  ▼ 唤醒
进入 Resume
  │
  └─────────────────────→ 回到"是否有 WakeLock"判断
```

**图 1：休眠唤醒流程**

---

## 2. 低功耗模式

### 2.1 低功耗模式介绍

低功耗模式（即 LPM）的状态图如下所示。当 MCU/Android 检测车辆 CAN 信号发现熄火（即 ACC OFF）时，将通过 SPI、UART 和 USB 等接口通知模块停止用户应用程序的各项业务、停止外设工作、解锁相应的唤醒锁。当模块内部所有唤醒锁均被释放且所有外设都未工作时，模块将进入休眠，即低功耗模式。

当电话、短信、数据、定时器及特定 GPIO 中断等 5 种中断事件出现时，模块会从休眠状态中唤醒。相应的回调函数或者处理函数锁定相应的唤醒锁后，模块进入正常工作状态。

### 低功耗模式状态图

```
MCU/Android ←──SPI/UART/USB──→ QuecOpen模块
     ↑                               │
  车辆CAN信号                        │
                              ┌──────┴──────┐
                              │    暂停     │◄──────────────┐
                              └──────┬──────┘               │
                              │语音│数据│短信│定时器│GPIO│VBUS│
                              └──────┬──────┘
                                     │
                              ┌──────┴──────┐
                              │    恢复     │
                              └──────┬──────┘
                                     │
                              相应的回调或者处理函数
                                     │ 调用API加锁
                              ┌──────┴──────┐
                              │ 业务处理完成 │
                              └──────┬──────┘
                                     │ 调用API解锁
                                     └───────────────────────┘
```

**图 2：低功耗模式状态图**

> **备注**：建议模块控制的外设在休眠唤醒时正常供电。若外设必须要在休眠时去电、唤醒时上电，则需要保证外设供电的控制在模块侧，避免外部 MCU 等控制器上电和模块内驱动进入休眠的时间不同步，造成驱动休眠失败。

---

### 2.2 GPIO 休眠唤醒方案

GPIO 休眠唤醒方案中，MCU/Android 与 QuecOpen 模块之间通过以下三个引脚交互休眠唤醒状态：

```
MCU/Android ←── sleep_sys_ind ──  QuecOpen module
             ←── wakeup_out   ──
             ──── wakeup_in   ──→
                    ↑
              车辆CAN信号
```

**图 3：GPIO 休眠唤醒方案**

---

#### 2.2.1 sleep_sys_ind

`sleep_sys_ind`，相当于 `wakeup_sys_ind`。该引脚由**模块驱动**注册管理，表征系统进入或退出休眠。

**表 1：sleep_sys_ind 电平**

| 电平 | 表征状态 |
|------|---------|
| 0 | 默认表示模块系统进入休眠 |
| 1 | 默认表示模块系统退出休眠 |

---

#### 2.2.2 wakeup_out

该引脚由**用户层**注册管理，表征用户应用程序进入或退出休眠（具体使用的 GPIO 管脚由用户自行决定）。

**表 2：wakeup_out 电平**

| 电平 | 表征状态 |
|------|---------|
| 0 | 默认表示应用程序进入休眠 |
| 1 | 默认表示应用程序退出休眠 |

---

#### 2.2.3 wakeup_in

该引脚由**模块驱动**注册管理，表征外接 MCU 控制的模块休眠唤醒状态。

**表 3：wakeup_in 电平**

| 电平 | 表征状态 |
|------|---------|
| 0 | 默认表示 MCU 控制模块休眠 |
| 1 | 默认表示 MCU 控制模块唤醒 |

---

### 2.3 低功耗模式 GPIO 配置

DTS 配置在文件 `ql-ol-kernel/arch/arm/boot/dts/ql-asr1803-overlay.dtsi` 中。其具体配置示例如下：

```dts
quec,gpio_lpm{
    compatible = "quec,ql_lpm";
    pinctrl-names = "default","sleep";
    pinctrl-0 = <&wakeup_in_pin &sleep_sys_ind>;
    pinctrl-1 = <&wakeup_in_pin_sleep>;
    gpios = <&gpio 117 0>,
            <&gpio 120 0>;
    ql,gpio-names = "wakeup_in","sleep_sys_ind";
    ql,sleep-sys-ind-enable;
    ql,sleep-sys-ind-state = <0>;
    status = "ok";
};
```

**图 4：DTSI 说明**

**表 4：GPIO 配置说明**

| 参数 | 含义 | 默认状态 |
|------|------|---------|
| `gpios` | `wakeup_in` 和 `sleep_sys_ind` 的 QuecOpen 引脚配置 | wakeup_in: *117*；sleep_sys_ind: *120* |
| `ql,sleep-sys-ind-enable` | 是否开启休眠指示引脚 | 开启 |
| `ql,sleep-sys-ind-state` | **0**：模块休眠时指示低电平，唤醒时指示高电平；**1**：模块休眠时指示高电平，唤醒时指示低电平 | *0* |
| `status` | `ok`：表示开启低功耗模式；`disabled`：表示关闭低功耗模式 | *ok* |

---

## 3. 低功耗模式 API

### 3.1 头文件

低功耗模式 API 的头文件为 `ql_lpm.h` 和 `ql_sleep_wakelock.h`，位于 `ql-sysroots/usr/include/ql-sdk` 目录下，若无特别说明，本文档所提到的头文件均在该目录下。

低功耗模式 API 的使用可以参考 SDK 示例程序，SDK 示例程序为 `example_lpm.c`，位于 `sample/low_power_consume_app` 目录下。

---

### 3.2 函数概览

**表 5：函数概览**

| 函数 | 说明 |
|------|------|
| `ql_lpm_init()` | 初始化低功耗模式 |
| `ql_autosleep_enable()` | 使能 AutoSleep |
| `ql_slp_wakelock_create()` | 创建唤醒锁 |
| `ql_slp_wakelock_lock()` | 锁定已创建的唤醒锁 |
| `ql_slp_wakelock_unlock()` | 解锁指定的唤醒锁 |
| `ql_slp_wakelock_destroy()` | 注销唤醒锁 |

---

### 3.3 函数详解

#### 3.3.1 WakeLock 相关 API

##### 3.3.1.1 ql_lpm_init

该函数用于初始化低功耗模式。

**函数原型**

```c
int ql_lpm_init(ql_lpm_handler_t ql_lpm_handler)
```

**参数**

- `ql_lpm_handler`：
  - [In] 回调函数，当 `wakeup_in` 引脚电平发生变化时将触发该回调函数。详见 [3.3.1.1.1 ql_lpm_handler_t](#33111-ql_lpm_handler_t)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

> **备注**：用户重复调用该接口时，只有最后一次调用传入的回调函数会执行，因此不建议重复调用该接口。

---

##### 3.3.1.1.1 ql_lpm_handler_t

当 `wakeup_in` 引脚的电平发生变化时将触发该回调函数，可通过 `ql_lpm_init()` 注册该回调函数。

**函数原型**

```c
typedef void (*ql_lpm_handler_t)(ql_lpm_edge_t lpm_edge)
```

**参数**

- `lpm_edge`：
  - [In] 底层上报的 `wakeup_in` 电平边沿变化。详见 [3.3.1.1.2 ql_lpm_edge_t](#33112-ql_lpm_edge_t)。

**返回值**：无

---

##### 3.3.1.1.2 ql_lpm_edge_t

`wakeup_in` 电平边沿变化枚举定义如下：

```c
typedef enum
{
    E_QL_LPM_FALLING = 0,
    E_QL_LPM_RISING  = 1,
} ql_lpm_edge_t
```

**成员**

| 成员 | 描述 |
|------|------|
| `E_QL_LPM_FALLING` | 下降沿（`wakeup_in` 电平下降唤醒模块或 `wakeup_out` 电平下降唤醒 MCU） |
| `E_QL_LPM_RISING` | 上升沿（`wakeup_in` 电平上升唤醒模块或 `wakeup_out` 电平上升唤醒 MCU） |

---

##### 3.3.1.2 ql_autosleep_enable

该函数用于使能 AutoSleep，使能后系统会在满足条件后自动进入休眠。

**函数原型**

```c
int ql_autosleep_enable(int enable)
```

**参数**

- `enable`：
  - [In] 是否使能 AutoSleep。
    - `0`：关闭 AutoSleep
    - `1`：使能 AutoSleep

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

> **备注**：`ql_autosleep_enable(1)` 使能 AutoSleep 后，一般不建议使用 `ql_autosleep_enable(0)` 来保持唤醒，而应该调用 `ql_slp_wakelock_lock()` 使系统保持唤醒，调用 `ql_slp_wakelock_unlock()` 可以释放放唤醒锁，放弃休眠锁定。详情请参考 **3.3.1.4** 和 **3.3.1.5** 章。系统休眠后进程会被冻结，唤醒后进程会继续运行。

---

##### 3.3.1.3 ql_slp_wakelock_create

该函数用于创建唤醒锁。此接口创建的唤醒锁所有者为当前进程；最多可以创建 **512** 个休眠唤醒锁。

**函数原型**

```c
int ql_slp_wakelock_create(const char *name, size_t len)
```

**参数**

- `name`：
  - [In] 唤醒锁名称（在 `/sys/kernel/debug/wakeup_sources` 文件内体现）
- `len`：
  - [In] 唤醒锁名称的长度，最大长度为 **28** 个字符。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 唤醒锁描述符（非负整数） | 函数执行成功 |
| 其他值（负数） | 函数执行失败。错误码详见 `errno.h`。 |

---

##### 3.3.1.4 ql_slp_wakelock_lock

该函数用于锁定已创建的唤醒锁，锁定后模块无法进入休眠模式。

**函数原型**

```c
int ql_slp_wakelock_lock(int fd)
```

**参数**

- `fd`：
  - [In] 唤醒锁描述符

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

---

##### 3.3.1.5 ql_slp_wakelock_unlock

该函数用于解锁指定的唤醒锁。若 Linux 系统内没有其他程序持有唤醒锁，且程序已使能 AutoSleep，则模块会自动进入休眠。

**函数原型**

```c
int ql_slp_wakelock_unlock(int fd)
```

**参数**

- `fd`：
  - [In] 唤醒锁描述符

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

---

##### 3.3.1.6 ql_slp_wakelock_destroy

该函数用于注销唤醒锁。

**函数原型**

```c
int ql_slp_wakelock_destroy(int fd)
```

**参数**

- `fd`：
  - [In] 唤醒锁描述符

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

---

## 4. 常见问题处理

### 4.1 模块无法进入休眠

系统进入休眠时采用持锁机制，当使能 AutoSleep 后，若某个应用还持有唤醒锁，整个系统都无法进入休眠；系统内所有唤醒锁都释放后，系统方可进入休眠。

若用户在应用程序中已经释放了唤醒锁，但模块仍未进入休眠模式，可执行以下命令查看当前系统持有的唤醒锁：

```bash
awk '$7!= 0 {print $1" "$7}' /sys/kernel/debug/wakeup_sources
```

**排查步骤：**

**① 若输出 `mv-udc` 和 `mv-otg`**，则表示 USB_VBUS 引脚处于高电平状态，需拉低 USB_VBUS 引脚电平使模块休眠。

```
root@OpenWrt:/# awk '$7!= 0 {print $1" "$7}' /sys/kernel/debug/wakeup_sources
name active_since
mv-udc 5102
mv-otg 5100
root@OpenWrt:/#
```

**② 若输出 `ql_lpm`**，则表示 lpm 程序持有唤醒锁，需释放唤醒锁使模块休眠。

```
root@OpenWrt:/data# awk '$7 !=0 {print $1" "$7}'  /sys/kernel/debug/wakeup_sources
name active_since
3063.ql_lpm 4719
root@OpenWrt:/data#
```

**③ 若输出如下信息**，则表示系统未持有唤醒锁，模块可以休眠。

```
root@OpenWrt:/data# awk '$7 !=0 {print $1" "$7}'  /sys/kernel/debug/wakeup_sources
name active_since
root@OpenWrt:/data#
```

> **备注**：若输出信息为其他唤醒锁标识符，用户可自行排查对应的应用程序内是否存在唤醒锁。只有将应用程序内所有的唤醒锁都释放了，模块才能正常进入休眠。

---

### 4.2 唤醒原因排查

可使用模块的调试串口捕捉日志信息，通过日志信息对模块唤醒原因进行分析，确认是否为非预期唤醒，并针对性地处理。

如需排查唤醒原因，首先执行如下命令，打开调试串口 log，即可查看唤醒中断信息。

```bash
echo 9 > /proc/sys/kernel/printk
echo 16371 > /dev/msocket_dump
```

然后，根据以下各节排查唤醒原因。如需移远通信技术支持分析唤醒原因，请提供休眠唤醒时的 Kernel Log；若为网络消息唤醒，请提供 CP Log。

---

#### 4.2.1 VBUS 中断唤醒

VBUS 作为唤醒源唤醒的日志如下图所示，关键字为 `vbus`：

```
[ 156.724616] NSOCK: msocket_read: portc_recv returns ffffffe00
[ 156.730960] NSOCK: msocket_read: portc_recv returns ffffffe00
[ 156.737578] NSOCK: msocket_read: portc_recv returns ffffffe00
[ 156.842071] bridge0: port 2(eth0) entered disabled state
[ 156.851435] mv-udc mv-udc: enable irq wakeup
[ 156.851618] ============quectel_wakeup:============
[ 156.854546] ============vbus=======================
[ 156.854607] vbuson_work
[ 156.854637] no wakelock for suspend state
[ 156.854668] ============vbus=======================
[ 156.854698] vbuson_work
[ 156.854729] no wakelock for suspend state
```

---

#### 4.2.2 RTC 中断唤醒

RTC 作为唤醒源唤醒的日志如下图所示。模块有两个 RTC 设备，分别为 **RTC0** 和 **RTC1**：

```
[ 792.227193] printk: Suspending console(s) (use no_console_suspend to debug)
[ 792.322170] bridge0: port 2(eth0) entered disabled state
[ 792.324610] Change ddr freq to lowest value. (Cur: 266000Khz)
[ 792.331472] mv-udc mv-udc: enable irq wakeup
[ 792.331625] ==========wake up events status ===========
[ 792.331625] BEFORE SUSPEND AWUCRS:0x90080000
[ 792.331625] Power Mode Status :0xdf
[ 792.331625] ============quectel_wakeup:============
[ 792.331686] PM: pm_system_irq_wakeup: 22 triggered rtc Alarm
[ 792.334278] ============rtc0==========================
[ 792.334370] ============rtc0==========================
```

---

#### 4.2.3 Wakeup_in 中断唤醒

`Wakeup_in` 是模块的唤醒引脚，默认为 **GPIO117**，唤醒日志如下图所示：

```
[ 971.494616] (elapsed 0.001 seconds)
[ 971.499588] done.
[ 971.505413] printk: Suspending console(s) (use no_console_suspend to debug)
[ 971.611461] bridge0: port 2(eth0) entered disabled state
[ 971.614084] Change ddr freq to lowest value. (Cur: 266000Khz)
[ 971.620947] mv-udc mv-udc: enable irq wakeup
[ 971.621099] ==========wake up events status ===========
[ 971.621099] BEFORE SUSPEND AWUCRS:0x90080000
[ 971.621099] Power Mode Status :0xdf
[ 971.621099] ============quectel_wakeup:============
[ 971.624058] ============wakeup_in================
```

---

#### 4.2.4 电话/短信/网络消息等中断唤醒

常见电话、短信、网络消息等中断唤醒的日志为在 `quectel_wakeup` 关键词之后打印 16 进制数据，如下图所示：

```
[ 982.226736] printk: Suspending console(s) (use no_console_suspend to debug)
[ 982.331228] bridge0: port 2(eth0) entered disabled state
[ 982.333821] Change ddr freq to lowest value. (Cur: 266000Khz)
[ 982.340683] mv-udc mv-udc: enable irq wakeup
[ 982.340836] ==========wake up events status ===========
[ 982.340836] BEFORE SUSPEND AWUCRS:0x900B0000
[ 982.340836] Power Mode Status :0xc3
[ 982.340836] ============quectel_wakeup:============
[ 982.341049] portq RX: DUMP BEGIN port :1, Length:308 --
[ 982.341080] 00 00 00 00 00 00 00 00  00 24 01 00 00 00 00 00
[ 982.341110] 01 00 00 00 05 00 00 00  18 01 00 00 2d 00 00 00
[ 982.341141] 05 00 00 00 01 00 00 d0  4e 35 07 8d 00 00 00 81
[ 982.341171] 8a 81 03 01 25 00 82 02  81 82 85 13 80 00 28 00
[ 982.341171] 35 00 00 09 50 00 53 61  5e 94 75 28 8f
```

具体的网络端唤醒场景需要通过 CP Log 排查，可联系移远通信技术支持协助排查具体原因。

---

## 附录 参考文档及术语缩写

**表 6：参考文档**

| 文档名称 |
|---------|
| [1] Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

**表 7：术语缩写**

| 缩写 | 英文描述 | 中文描述 |
|------|---------|---------|
| AutoSleep | Auto Sleep | 自动休眠 |
| LPM | Low Power Mode | 低功耗模式 |
| MCU | Micro Controller Unit | 微控制单元 |
| VBUS | USB Bus Voltage | USB 总线电压 |
| WakeLock | Wake Lock | 唤醒锁 |
