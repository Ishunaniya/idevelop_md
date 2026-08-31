# Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_USB_开发指导_V1.0 分析报告

## 1. 文档基本信息

- **标题**：AG35-CET&AG35-EUT QuecOpen(SDK) USB 开发指导
- **适用模块**：LTE Standard 模块系列 AG35-CET、AG35-EUT
- **版本**：1.0（受控文件）
- **日期**：2025-05-20
- **作者（最终版本）**：Gabriel LI
- **总页数**：23 页（封面 + 正文22页）
- **发布单位**：上海移远通信技术股份有限公司（Quectel）

### 文档历史（修订记录）

| 版本 | 日期 | 作者 | 变更描述 |
|------|------|------|----------|
| - | 2023-11-13 | Aurora JIANG | 文档创建 |
| 1.0.0 | 2023-11-13 | Aurora JIANG | 临时版本 |
| 1.0.1 | 2024-09-29 | Gabriel LI | 临时版本：1. 新增适用模块 AG35-EUT；2. 新增 USB 主从模式切换的说明（第4章）；3. 新增 USB 端口默认状态的说明（第5章）；4. 新增进入 DCP 充电模式导致 USB 关闭的说明（第6.5章，对应最终版第6.2.2节）|
| 1.0 | 2025-05-20 | Gabriel LI | 受控版本（正式发布） |

可以看出文档曾经历多次结构调整，最终定稿版本中 USB 主从模式切换、端口默认配置、DCP 充电模式说明被整合进了最终目录的第4、5、6章。

### 前言/免责声明要点（非技术性，但需知悉）
- 本文档内容仅供产品设计参考，客户需独立分析评估判断。
- 移远通信对文档内容的完整性/准确性/及时性不承担超出商业合理努力范围的责任。
- 涉及第三方材料版权与保密义务（许可协议）。
- 隐私声明：特定设备数据可能上传至移远通信或第三方服务器（运营商/芯片供应商/客户指定服务器）。

---

## 2. 目录结构概览

```
文档历史 .......................................................... 3
目录 .............................................................. 4
表格索引 .......................................................... 5
图片索引 .......................................................... 6
1  引言 ........................................................... 7
2  USB 网卡类型配置 ................................................ 8
   2.1 永久修改（2.1.1 通过配置文件 / 2.1.2 通过 SDK）
   2.2 临时修改
3  ADB 功能配置 .................................................... 10
   3.1 永久配置（3.1.1 通过配置文件 / 3.1.2 通过 SDK）
   3.2 临时配置
4  USB 主从模式切换 ................................................ 12
   4.1 OTG 模式开启
   4.2 U 盘测试
   4.3 ECM 识别测试
   4.4 设备节点配置
5  USB 端口默认配置 ................................................ 16
   5.1 查看默认配置
   5.2 关闭指定端口
6  USB 常规调试 .................................................... 18
   6.1 USB 传输速率
   6.2 USB 枚举（6.2.1 枚举状态 / 6.2.2 DCP 充电模式）
   6.3 ADB 功能
   6.4 USB 端口
7  附录 参考文档及术语缩写 .......................................... 22
```

表格索引：表1：参考文档（p.22）；表2：术语缩写（p.22）
图片索引：图1：ECM 识别测试硬件连接示意图（p.14）；图2：在 Windows 设备管理器查看端口（p.17）

---

## 3. 逐章节详细摘要

### 第1章 引言（p.7）

移远通信 AG35-CET 和 AG35-EUT 模块支持 **QuecOpen® 方案**——基于 Linux 的嵌入式开发平台，用于简化 IoV（车联网）应用的软件设计和开发过程。QuecOpen® 详细信息参考"文档[1]"（即《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导》）。

本文档适用于 **SDK 构建环境** 的 QuecOpen® 方案，主要介绍 AG35-CET 和 AG35-EUT 模块 USB 相关的配置说明，涵盖：
- USB 网卡类型配置
- ADB 功能配置
- USB 主从模式切换
- USB 端口默认配置
- USB 常规调试

### 第2章 USB 网卡类型配置（p.8-9）

**术语定义**：模块支持的 USB 网卡类型为 **ECM**（Ethernet Control Model，以太网控制模型）和 **RNDIS**（Remote Network Driver Interface Specification，远程网络驱动接口规范），**默认为 ECM**。

#### 2.1 永久修改
配置网卡类型有两种方式：
1. **修改配置文件**——主要应用于模块验证或调试。
2. **修改 SDK 包文件**——配置 USB 网卡类型后需重新编译 rootfs 镜像并进行烧录。

**2.1.1 通过配置文件**
- 步骤1：通过 Debug 串口或 ADB 连接模块至主机。
- 步骤2：执行 `cat /data/usb/usb_net_type` 查询当前 USB 网卡类型（示例返回 `rndis`）。
- 步骤3：执行对应命令切换网卡类型：
  - 切换为 ECM：`echo ecm > /data/usb/usb_net_type`
  - 切换为 RNDIS：`echo rndis > /data/usb/usb_net_type`
- 步骤4：执行 `sync` 并重启模块使配置生效（也可通过执行 `/sbin/usb_init` 脚本使配置生效）。

**2.1.2 通过 SDK**
- 在 SDK 包的 `ql-ol-rootfs/sbin/` 目录下找到 `usb_init` 文件。
- 文件内容示例：
  ```sh
  #!/bin/sh
  # usb_init script
  #eyelyn  add 20220621 usb configured node
  #The default nic mode is ECM and ADB is enabled by default
  QUEC_USB_NET_TYPE="ecm"
  QUEC_USB_ADB_ENABLE="on"
  ```
- 默认 `QUEC_USB_NET_TYPE="ecm"` 表示当前模块 USB 网卡类型为 ECM；如需修改为 RNDIS 网卡类型，仅需将 `"ecm"` 字样替换为 `"rndis"`。

**备注（重要限制）**：网卡类型修改完成后需要**重新编译 rootfs 镜像**，然后**烧录新的镜像**到模块中，烧录成功后配置方可生效。有关编译和固件烧录的详细信息，请参考文档[1]和[2]。

#### 2.2 临时修改（p.9）
- 步骤1：通过 Debug 串口或 ADB 连接模块至主机。
- 步骤2：执行 `cat /sys/devices/virtual/android_usb/android0/functions` 查询当前 USB 网卡类型，如下图所示：示例返回 `ecm,marvell_diag,acm,marvell_modem,adb`。
- 步骤3：分别执行命令切换网卡类型。

  执行如下命令将 USB 网卡类型切换为 RNDIS：
  ```
  echo 0 > /sys/devices/virtual/android_usb/android0/enable
  echo rndis,marvell_diag,acm,marvell_modem,adb > /sys/devices/virtual/android_usb/android0/functions
  echo 1 > /sys/devices/virtual/android_usb/android0/enable
  ```
  执行如下命令切换回 ECM：
  ```
  echo 0 > /sys/devices/virtual/android_usb/android0/enable
  echo ecm,marvell_diag,acm,marvell_modem,adb > /sys/devices/virtual/android_usb/android0/functions
  echo 1 > /sys/devices/virtual/android_usb/android0/enable
  ```
- 步骤4：执行 `sync`，配置立即生效。**模块重启后则恢复为默认配置**（即临时修改不持久化）。

### 第3章 ADB 功能配置（p.10-11）

模块**默认开启 ADB 功能**。本章介绍如何打开或关闭 ADB 功能。

#### 3.1 永久配置
可通过修改配置文件或 SDK 包中的 `usb_init` 文件修改 ADB 功能状态。

**3.1.1 通过配置文件**
- 步骤1：执行 `cat /data/usb/usb_adb_enable` 查询当前 ADB 功能是否已打开。返回 `on` 表示当前 ADB 功能已打开。
- 步骤2：执行命令切换 ADB 状态：
  - 关闭 ADB 功能：`echo off > /data/usb/usb_adb_enable`
  - 开启 ADB 功能：`echo on > /data/usb/usb_adb_enable`
- 步骤3：重启模块或执行 `/sbin/usb_init` 脚本使配置生效。

**3.1.2 通过 SDK**
- 打开 SDK 包 `ql-ol-rootfs/sbin/` 目录下的 `usb_init` 文件，找到 ADB 功能配置所在的位置（同 2.1.2 节展示的脚本内容）。
- 默认状态下，`QUEC_USB_ADB_ENABLE="on"` 表示 ADB 功能已打开。如需关闭该功能，仅需将 `"on"` 更改为 `"off"`，即 `QUEC_USB_ADB_ENABLE="off"`。

**备注**：关闭 ADB 功能后需要**重新编译 rootfs 镜像**，然后烧录新的镜像到模块中，烧录成功后配置方可生效。有关编译和固件烧录的详细信息，请参考文档[1]和[2]。

#### 3.2 临时配置（p.11）
- 步骤1：执行 `cat /data/usb/usb_adb_enable` 查询当前 ADB 功能是否已打开（截图中实际通过查询 functions 验证当前已含 `adb`）。
- 步骤2：执行对应命令切换 ADB 功能状态：

  执行如下命令关闭 ADB 功能：
  ```
  echo 0 > /sys/devices/virtual/android_usb/android0/enable
  echo ecm,marvell_diag,acm,marvell_modem > /sys/devices/virtual/android_usb/android0/functions
  echo 1 > /sys/devices/virtual/android_usb/android0/enable
  ```
  执行如下命令重新打开 ADB 功能：
  ```
  echo 0 > /sys/devices/virtual/android_usb/android0/enable
  echo ecm,marvell_diag,acm,marvell_modem,adb > /sys/devices/virtual/android_usb/android0/functions
  echo 1 > /sys/devices/virtual/android_usb/android0/enable
  ```
- 步骤3：配置立即生效。模块重启后则恢复为默认配置。

### 第4章 USB 主从模式切换（p.12-15）

模块支持 USB 主从模式，并且可通过 **NET_STATUS 引脚复用为 USB_ID 引脚**或**系统命令**实现主从切换。打开 OTG 模式后，模块支持动态切换主从模式。**当前，在主模式下，仅支持 U 盘存储设备识别和 ECM 网卡枚举功能**（限制）。

#### 4.1 OTG 模式开启（p.12-13）

修改设备树文件 `/ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts`：

```c
/* Config USB OTG */
#define CONFIG_USB_OTG_ENABLE       0
#define CONFIG_VBUS_POWER_ENABLE 0
……
&usb {
#if (CONFIG_USB_OTG_ENABLE)
#if (CONFIG_VBUS_POWER_ENABLE)
    pinctrl-0 = <&usb_id_pinmux &usb_host_pinmux>;
    pinctrl-1 = <&usb_id_pinmux_slp &usb_host_pinmux>;
    otg,use-gpio-vbus;
    gpio-num = <122>;
#endif
    usbid_gpio = <99>;
    edge_detect_gpio = <99>;
#else
    otg-force-dev-mode = <1>;
#endif
};
```

- 打开 OTG 模式需修改 `CONFIG_USB_OTG_ENABLE` 为 `1`：`#define CONFIG_USB_OTG_ENABLE  1`
- 在主模式下，**不提供 5V VBUS 电源输出**，需外接电源为从设备供电。模块需设计**外部电源控制电路**，此电源控制引脚默认采用 **144 号引脚**（对应基带芯片 **GPIO_122**）。如需更换其他引脚，需修改 `gpio-num = <122>`，并修改 `CONFIG_VBUS_POWER_ENABLE` 为 `1`：`#define CONFIG_VBUS_POWER_ENABLE  1`

#### 4.2 U 盘测试（p.13）

- 拉低 **USB_ID**，若打印如下 log（关键行高亮：`dwc2 c0000000.usb: start host`）则表示模块已切换到主机模式（日志中可见 `disable vbus irq`、`USB2 PHY set_suspend: 0...`、`Forcing mode to host`、`Host mode set`、`restore host cfgs`、`DWC OTG Controller`、`new USB bus registered, assigned bus number 1`、`USB OTG HCD Has Root Hub`等）。
- 插入 U 盘，若打印如下 log 则表示识别成功：`New USB device found, idVendor=0781, idProduct=5597, bcdDevice=1.00`、`Product: Cruzer Glide 3.0`、`Manufacturer: SanDisk`、`USB Mass Storage device detected`、`scsi host0: usb-storage`、`[sda] Attached SCSI removable disk`，可在命令行执行 `ls /dev/sda*` 验证，返回 `/dev/sda /dev/sda1`。

#### 4.3 ECM 识别测试（p.14）

将两个模块连接，分别作为主机和从机：
- **主机模块**：USB_ID 接 GND，并拉低 USB_ID 以进入主机模式；
- **从机模块**：拉高 USB_ID 维持从机模式，且从机模块需有 **5V VBUS 电源输入**。

硬件连接示意图如图1所示：主机与从机之间通过 USB_VBUS、USB_DM、USB_DP、GND 四条线互连；主机侧 USB_ID 接 GND；从机侧 USB_ID 接 1.8V；从机侧 USB_VBUS 由外部 5V 电源供电。详情请参考"文档[3]"（《Quectel_AG35-CET&AG35-EUT_QuecOpen_硬件设计手册》）。

在主机上执行如下命令修改配置，然后执行 ping 测试。**默认生成的网卡名为 USB0**（文档原文，截图中命令实际使用小写 `usb0`）：
```
ifconfig usb0 up
brctl addif bridge0 usb0
ifconfig bridge0 192.168.225.100
```
随后执行 `ping 192.168.225.1` 测试，文档展示的双侧终端截图显示 ping 成功（3 packets transmitted, 3 packets received, 0% packet loss，往返时延约 1.037~2.267ms），并通过 `brctl show` 确认 bridge0 已桥接 usb0 和 eth0 接口。

#### 4.4 设备节点配置（p.15）

模块支持采用**设备节点方式**切换主从模式，**不依赖 USB_ID 引脚**。进行切换前，**需先将 USB 恢复到枚举状态，而不是在 USB 完成枚举并进入工作状态后才进行切换操作**（重要前提条件/注意事项）。

```
/sys/devices/platform/soc/d4200000.axi/c0000000.usb/usb_mode
#切换从机
echo peripheral > /sys/devices/platform/soc/d4200000.axi/c0000000.usb/usb_mode
#切换主机
echo host > /sys/devices/platform/soc/d4200000.axi/c0000000.usb/usb_mode
```

文档附有切换至从机和切换至主机的内核日志截图：
- 切换至从机日志关键行：`force usb to peripheral mode by user`、`=>old_otg_state: 9, usbid: 1 vbus: 0`、`dwc2 c0000000.usb: stop host`、`USB disconnect, device number 1`、`USB2 PHY set_suspend: 1...`、`cur_otg_state: [9->1], usbid: 1 vbus: 0`
- 切换至主机日志关键行：`force usb to host mode by user`、`=>old_otg_state: 1, usbid: 0 vbus: 0`、`disable vbus irq`、`dwc2 c0000000.usb: start host`、`Forcing mode to host`、`Host mode set`、`USB OTG HCD Has Root Hub`、`cur_otg_state: [1->9], usbid: 0 vbus: 0`

### 第5章 USB 端口默认配置（p.16-17）

#### 5.1 查看默认配置（p.16）

模块支持如下两种方式查看默认开启的端口。

**方法1**：执行如下命令查看默认开启的端口：
```
cat /sys/devices/virtual/android_usb/android0/functions
```
返回如下：
```
ecm,marvell_diag,acm,marvell_modem,marvell_debug,adb
```

**方法2**：在上位机设备管理器查看端口：
- `ecm` 对应 CDC ECM
- `marvell_diag` 对应 Quectel USB DIAG
- `acm` 对应 Quectel USB AT Port 和 Quectel NMEA Port
- `marvell_modem` 对应 Quectel USB Modem
- `marvell_debug` 对应 Marvell DEBUG

图2（p.17）展示 Windows 设备管理器中实际识别出的端口列表：
- 调制解调器：Quectel USB Modem
- 端口（COM 和 LPT）：Quectel USB AT Port (COM20)、Quectel USB DIAG Port (COM16)、Quectel USB NMEA Port (COM18)、USB-SERIAL CH340 (COM13)、通信端口 (COM1)
- 其他设备：CDC ECM、Marvell DEBUG

#### 5.2 关闭指定端口（p.17）

模块支持通过修改 `ql-ol-rootfs/sbin/usb_init` 文件关闭指定端口：
```
QUEC_USB_FUN="ecm,marvell_diag,acm,marvell_modem,marvell_debug,adb"
```

各端口的关闭规则及功能说明：
- **ECM 网卡**默认开启，如需关闭可删除 `ecm`。
- **marvell_modem** 默认开启但不使用，将其删除即可关闭。
- **AT 命令端口和 NMEA 端口**默认开启，删除 `acm` 即可关闭。**因 `acm` 同时绑定 AT 命令端口和 NMEA 端口，删除 `acm` 会同时关闭 AT 命令端口和 NMEA 端口**（重要关联关系，不可单独关闭其中一个）。
- **ADB 功能**默认开启，如需关闭可删除 `adb`。
- **DIAG 口**默认开启，如需关闭日志功能，可删除 `marvell_diag`。
- **marvell_debug** 默认开启，若无需 debug 日志可删除。

**备注（重要限制）**：移远通信提供的 Windows 驱动包会按特定顺序识别各功能端口，若单独关闭某端口，**可能使识别顺序混乱，导致默认 Windows 驱动无法正常工作**。此时，可连接其他设备（如 Ubuntu 系统设备），依据新生成的端口号重新进行识别和配置。

### 第6章 USB 常规调试（p.18-21）

#### 6.1 USB 传输速率（p.18）

模块**仅支持 USB 2.0 协议传输速率**。可通过执行如下命令查询当前模块传输速率：
```
cat /sys/devices/platform/soc/d4200000.axi/c0000000.usb/udc/c0000000.usb/current_speed
```
若返回值为 `high-speed`，则表示当前为 USB 2.0 协议传输速率。

**备注**：下载模式下，支持使用 USB 2.0 协议传输速率进行数据传输。

#### 6.2 USB 枚举（p.18-19）

**6.2.1 枚举状态**

执行 `cat /sys/devices/virtual/android_usb/android0/state` 查看枚举结果。若返回值为 `CONFIGURED`，则表示 USB 枚举成功。

**6.2.2 DCP 充电模式**

以下场景中，USB 将被枚举为 **DCP（Dedicated Charging Port，专用充电端口）充电模式**：
- USB 连接的设备处于不稳定状态；
- 模块正处于枚举状态中，USB 数据线处于恶劣环境。

**影响**：该模式下 **USB 功能关闭，对端无法枚举出端口**，可通过**重新插拔 USB 恢复**。

文档展示了内核源码片段（`dwc2_charger_type_confirm` 函数，约第316-358行），关键代码逻辑：函数中有 `timeout = 95`（注释 "950 ms, should get charger type in 1s"）的等待循环，检测 `hsotg->bus_reset_received`/`suspend_received` 状态及 vbus 状态；若最终判定为充电器，则设置 `hsotg->charger_type = DCP_CHARGER`，打印 `pr_err("%s: suspend usb phy\n", __func__);`，并调用 `usb_phy_set_suspend(hsotg->uphy, 1);` 挂起 USB PHY。内核日志中该过程亦有打印。

若实际应用中**无充电场景**，可通过修改内核代码以**删除 DCP 充电模式逻辑**，即修改文件 `/ql-ol-kernel/drivers/usb/dwc2/gadget.c`，在 `dwc2_charger_type_confirm` 函数中添加 `return;`（文档第19/21页截图显示在函数第322行、`unsigned int vbus = 0;` 声明之后插入 `return;`，使函数直接返回，跳过后续整个 charger 类型确认及 PHY 挂起逻辑），从而禁用 DCP 充电模式检测，避免 USB 因被误判为充电模式而关闭功能。

#### 6.3 ADB 功能（调试/故障排除，p.20）

- 步骤1：若模块的 ADB 功能无法使用，请首先确认 ADB 功能已打开（详见第3章）。若 ADB 功能已打开，请执行 `ps | grep adb` 检查 ADB 进程是否存在，若返回值包含如下红框中的值则表示 ADB 进程已存在：`844 root  0:00 /usr/bin/adbd -D`。
- 步骤2：若 ADB 进程不存在，执行 `/etc/init.d/adbd start` 手动启动该进程。
- 若 ADB 功能已打开且 ADB 进程已开启时，仍无法使用 ADB 功能，请联系移远通信技术支持。

#### 6.4 USB 端口（调试/故障排除，p.20-21）

若出现 USB 端口未被识别的情况，请依次进行以下操作排除失败原因。若以下情况均无异常，请联系移远通信技术支持：
- 步骤1：请确保 **USB 数据线无松动**。USB 数据线松动可能会导致模块与主机连接失败。
- 步骤2：请确认 **USB 枚举成功**（详见第6.2章）：
  - 若显示 USB 枚举成功，可能是由于**主机端驱动未安装或安装失败**造成的，请卸载驱动并重新安装驱动；
  - 若显示 USB 枚举失败，请联系移远通信技术支持。

### 第7章 附录 参考文档及术语缩写（p.22）

**表1：参考文档**

| 编号 | 文档名称 |
|------|----------|
| [1] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |
| [2] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_固件下载指导 |
| [3] | Quectel_AG35-CET&AG35-EUT_QuecOpen_硬件设计手册 |

**表2：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|------|----------|----------|
| ADB | Android Debug Bridge | 安卓调试桥 |
| DCP | Dedicated Charging Port | 专用充电端口 |
| ECM | Ethernet Control Model | 以太网控制模型 |
| IoV | Internet of Vehicles | 车联网 |
| NMEA | NMEA (National Marine Electronics Association) 0183 Interface Standard | NMEA（美国国家海洋电子协会）0183 接口标准 |
| RNDIS | Remote Network Driver Interface Specification | 远程网络驱动接口规范 |
| SDK | Software Development Kit | 软件开发工具包 |
| USB | Universal Serial Bus | 通用串行总线 |

---

## 4. 关键命令/接口/参数汇总表

| 用途 | 命令/文件/参数 | 说明 |
|------|---------------|------|
| 查询 USB 网卡类型（永久） | `cat /data/usb/usb_net_type` | 返回 `ecm` 或 `rndis` |
| 设置网卡类型为 ECM（永久） | `echo ecm > /data/usb/usb_net_type` | 需 `sync` + 重启或 `/sbin/usb_init` 生效 |
| 设置网卡类型为 RNDIS（永久） | `echo rndis > /data/usb/usb_net_type` | 同上 |
| SDK 中网卡类型配置项 | `ql-ol-rootfs/sbin/usb_init` 中 `QUEC_USB_NET_TYPE="ecm"` | 改 `"rndis"` 切换；需重编译 rootfs + 烧录 |
| 查询当前 USB functions（临时） | `cat /sys/devices/virtual/android_usb/android0/functions` | 示例：`ecm,marvell_diag,acm,marvell_modem,adb` |
| 临时切换网卡类型 | `echo 0/1 > .../android0/enable` + `echo <list> > .../android0/functions` | 重启后恢复默认 |
| 查询 ADB 状态（永久） | `cat /data/usb/usb_adb_enable` | 返回 `on`/`off` |
| 设置 ADB 状态（永久） | `echo off/on > /data/usb/usb_adb_enable` | 需重启或 `/sbin/usb_init` 生效 |
| SDK 中 ADB 配置项 | `QUEC_USB_ADB_ENABLE="on"` | 改 `"off"` 关闭；需重编译 rootfs + 烧录 |
| 临时开关 ADB | 修改 `.../android0/functions` 中是否含 `adb`（配合 enable 0/1） | 重启后恢复默认 |
| OTG 模式开关（设备树） | `CONFIG_USB_OTG_ENABLE`（0/1） | 文件：`ql-ol-kernel/arch/arm/boot/dts/ql-1806e-std.dts` |
| VBUS 电源控制开关 | `CONFIG_VBUS_POWER_ENABLE`（0/1），`gpio-num = <122>` | 主模式下需外接电源给从设备供电，默认引脚144（GPIO_122） |
| 设备节点方式切主从模式 | `echo peripheral/host > /sys/devices/platform/soc/d4200000.axi/c0000000.usb/usb_mode` | 切换前需先恢复枚举状态 |
| 查看 USB 端口默认配置 | `cat /sys/devices/virtual/android_usb/android0/functions` | 返回 `ecm,marvell_diag,acm,marvell_modem,marvell_debug,adb` |
| 关闭指定端口（SDK） | `ql-ol-rootfs/sbin/usb_init` 中 `QUEC_USB_FUN="..."` | 删除对应项；`acm` 同时控制 AT 口和 NMEA 口 |
| 查看 USB 传输速率 | `cat /sys/devices/platform/soc/d4200000.axi/c0000000.usb/udc/c0000000.usb/current_speed` | 返回 `high-speed` = USB 2.0 |
| 查看 USB 枚举状态 | `cat /sys/devices/virtual/android_usb/android0/state` | 返回 `CONFIGURED` = 枚举成功 |
| 删除 DCP 充电模式逻辑 | 修改 `/ql-ol-kernel/drivers/usb/dwc2/gadget.c` 中 `dwc2_charger_type_confirm` 函数，函数体开头加 `return;` | 仅适用于无充电场景，需重新编译内核 |
| 检查 ADB 进程 | `ps \| grep adb` | 存在则显示 `/usr/bin/adbd -D` |
| 手动启动 ADB 进程 | `/etc/init.d/adbd start` | — |

---

## 5. 完整操作流程还原

### 流程A：永久修改 USB 网卡类型（ECM ↔ RNDIS）
1. 通过 Debug 串口或 ADB 连接模块至主机。
2. 查询当前类型：`cat /data/usb/usb_net_type`。
3. 写入新类型：`echo ecm > /data/usb/usb_net_type` 或 `echo rndis > /data/usb/usb_net_type`。
4. `sync` 并重启模块（或执行 `/sbin/usb_init`）。
   —— 或者 —— 修改 SDK 包 `ql-ol-rootfs/sbin/usb_init` 中 `QUEC_USB_NET_TYPE` 值 → 重新编译 rootfs 镜像 → 烧录新镜像到模块 → 烧录成功后生效。

### 流程B：临时修改 USB 网卡类型
1. 连接模块。
2. 查询 `cat /sys/devices/virtual/android_usb/android0/functions`。
3. 依序执行 disable → 写入新 functions 列表 → enable 三条命令。
4. `sync` 立即生效；模块重启后恢复默认。

### 流程C：ADB 功能开关（永久/临时）
流程结构与A/B类似，分别作用于 `/data/usb/usb_adb_enable` 或 `usb_init` 中 `QUEC_USB_ADB_ENABLE`（永久，需重编译+烧录），或直接操作 `android0/functions` 中是否含 `adb`（临时，重启后恢复默认）。

### 流程D：开启 OTG 主从切换能力并验证
1. 修改设备树 `ql-1806e-std.dts`，设置 `CONFIG_USB_OTG_ENABLE=1`。
2. 若需要主模式下给从设备供电，设计外部电源控制电路，设置 `CONFIG_VBUS_POWER_ENABLE=1` 并配置 `gpio-num`（默认 `<122>`，对应物理引脚144/基带 GPIO_122）。
3. 重新编译并烧录固件。
4. **U盘测试**：拉低 USB_ID 进入主机模式 → 观察内核日志确认 `start host` → 插入U盘 → 观察识别日志 → `ls /dev/sda*` 验证。
5. **ECM互联测试**：两模块对接（主机 USB_ID 接GND；从机 USB_ID 接1.8V且需5V VBUS供电）→ 主机端 `ifconfig usb0 up` + `brctl addif bridge0 usb0` + `ifconfig bridge0 192.168.225.100` → ping 从机IP验证连通性。
6. **设备节点方式切换**（不依赖USB_ID引脚）：先确保USB处于枚举状态 → `echo peripheral/host > .../usb_mode` 完成切换。

### 流程E：查看与裁剪 USB 端口
1. `cat /sys/devices/virtual/android_usb/android0/functions` 或 Windows 设备管理器查看当前开启端口。
2. 按需修改 SDK 中 `usb_init` 文件的 `QUEC_USB_FUN` 字符串，删除不需要的功能项（注意 `acm` 同时控制 AT 与 NMEA 两个端口，不能单独关闭其一）。
3. 重新编译 rootfs 镜像并烧录。
4. 若驱动识别顺序混乱，换接其他系统设备（如 Ubuntu）按新端口号重新识别配置。

### 流程F：USB 常规调试与故障排除
1. 查传输速率：`cat .../current_speed`，确认 `high-speed`。
2. 查枚举状态：`cat .../android0/state`，确认 `CONFIGURED`。
3. 若怀疑被误判为DCP充电模式（USB功能失效）：重新插拔USB；若是设计上无需充电支持，修改内核 `dwc2_charger_type_confirm` 函数直接加 `return;` 屏蔽该逻辑（需重新编译内核）。
4. ADB 故障排查：确认 ADB 已开启 → `ps | grep adb` 检查 adbd 进程 → 若无则 `/etc/init.d/adbd start` → 仍无效联系厂商。
5. USB 端口未识别故障排查：检查数据线是否松动 → 检查枚举是否成功（成功则重装驱动；失败则联系厂商）。

---

## 6. 与本项目（open_dial）的关联点

本项目 `open_dial` 是运行在 Quectel EC2x/EG2x（及相关 AG35 系列）模组上的拨号管理程序，核心功能为蜂窝数据连接的建立与维护（物理SIM/虚拟SIM双通道）。经核对本文档内容与项目 CLAUDE.md 中描述的架构：

1. **本文档主题与 open_dial 业务逻辑基本不重叠**。本文档聚焦于模块侧 **USB 接口本身**的配置（网卡类型ECM/RNDIS切换、ADB调试桥开关、USB主从（OTG Host/Device）模式切换、USB端口枚举裁剪、USB传输速率/枚举状态/DCP充电模式调试），属于**模块固件层/USB子系统**配置范畴；而 open_dial 关注的是**AT 串口（/dev/smd8）+ MCM API + 网络数据呼叫（Data Call）+ Roamlink虚拟SIM**的拨号状态机，二者工作在不同子系统。

2. **潜在的弱关联点**：
   - **USB网卡类型（ECM/RNDIS）**：open_dial 项目文档中提到网络接口为 `rmnet*`（如 `rmnet_data0`），这是 Qualcomm QMI/DSI 体系下的数据接口命名，与本PDF描述的 USB CDC ECM/RNDIS（dwc2 USB控制器、`asr1806` 等关键字所指代的平台体系）明显不属于同一芯片平台。本PDF面向的是该 AG35 模块所用平台的 USB 子系统，而项目 CLAUDE.md 描述的 open_dial 基于 EC2x/EG2x Qualcomm 模组的 QL_MCM_NW API 体系。因此本文档中的 `usb_init`、`/sys/devices/virtual/android_usb/android0/*`、`dwc2`、`asr1806`、`ql-ol-kernel` 等路径和接口**在 open_dial 当前代码（基于EC2x/EG2x Qualcomm平台）中均未出现，不能直接套用**。
   - **ADB 调试桥**：若未来项目涉及 AG35 模块的调试接入，本文档中 ADB 开关方法（`/data/usb/usb_adb_enable`、`usb_init` 中 `QUEC_USB_ADB_ENABLE`）可作为参考，但与 open_dial 拨号逻辑无直接代码交互。
   - **AT 命令端口（acm，对应 Quectel USB AT Port）**：open_dial 通过 `/dev/smd8` 发送 AT 指令（`Ql_SendAT`），这与本文档中 USB 复合设备枚举出的 "Quectel USB AT Port (COMxx)" 概念上类似（均是AT命令通道），但 open_dial 项目使用的是**串口设备节点 smd8**，并非 USB ACM 端口，故 AG35 USB AT Port 与 open_dial 的 AT 通道**不是同一物理/逻辑通道**，除非未来 AG35 平台的 open_dial 移植版本改为通过 USB ACM 收发 AT 命令。

3. **结论**：本文档与当前 `open_dial`（运行在 EC2x/EG2x Qualcomm 平台）项目**没有直接的代码级或运行时关联**；若公司未来计划将 open_dial 移植到 AG35-CET/AG35-EUT 模组，则本文档中 USB 网卡类型配置、ADB开关、主从模式、端口裁剪、DCP充电模式规避方法将成为**该移植工作的前置/配套知识**，但目前的源码仓库未发现任何文件引用本文档中的路径、变量名或命令。

---

## 7. 文档自身局限性

1. **平台耦合度高**：文档内容（设备树路径、`dwc2`/`asr1806` 关键字、`android_usb` sysfs 接口）表明底层 USB 控制器实现细节高度依赖特定芯片/固件版本，文档未说明该实现细节是否适用于所有 AG35 衍生型号或仅限于特定固件版本。
2. **主模式功能受限且未来计划未说明**：文档明确指出"当前在主模式下，仅支持 U 盘存储设备识别和 ECM 网卡枚举功能"，未说明是否计划支持更多设备类型（如 RNDIS从设备、其他存储设备类型），也未提及该限制的技术原因。
3. **电源设计细节不完整**：主模式下外部VBUS供电电路的具体设计（电源控制芯片型号、电气参数）文档中未给出，仅提及默认GPIO引脚（144号/GPIO_122），具体硬件设计需查阅"文档[3]"（硬件设计手册），但本PDF未包含该文档内容。
4. **DCP充电模式触发条件描述笼统**："USB连接的设备处于不稳定状态"和"USB数据线处于恶劣环境"这两个触发条件缺乏具体的量化标准（如电压抖动范围、连接抖动次数阈值等），开发者难以预先评估硬件设计是否会触发该问题。
5. **关闭指定端口对Windows驱动的影响说明不充分**：文档提示删除/关闭某端口"可能使识别顺序混乱，导致默认Windows驱动无法正常工作"，但未给出具体哪些端口组合是安全可关闭的、哪些会导致驱动失效，也未提供官方驱动包的端口识别顺序规则。
6. **调试故障排除章节（6.3、6.4）较为简略**：仅给出基础排查步骤（检查ADB进程、检查数据线、检查枚举状态），遇到非典型问题时均指向"联系移远通信技术支持"，对深层故障（如USB控制器寄存器异常、PHY硬件故障等）未提供进一步诊断方法。
7. **缺少版本/固件兼容性说明**：文档未注明本文中所有操作和sysfs接口路径在哪些SDK/固件版本范围内有效，若后续SDK升级，相关路径或脚本（如`usb_init`文件结构）可能发生变化，文档未提供版本对照表。
8. **参考文档[1][2][3]均未随附**：文档多次引用"文档[1]"（快速开发指导）、"文档[2]"（固件下载指导）、"文档[3]"（硬件设计手册）获取编译、烧录及硬件设计详情，但这三份文档本身并未包含在本次分析范围内，相关操作细节（如如何重新编译rootfs镜像的具体命令）文档自身未提及。
9. **DCP充电模式代码修改方案的副作用未评估**：文档给出在`dwc2_charger_type_confirm`函数开头加`return;`以禁用DCP充电检测的方法，但未说明该修改是否会影响其它正常充电场景下的电流协商行为或导致其他副作用，仅说明"若实际应用中无充电场景"方可采用，属于不完整的风险提示。

---

<!-- GENERATION_COMPLETE: 2026-06-24 -->
