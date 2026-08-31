# Quectel AG35 QuecOpen SDK Log抓取指导 —— 文档分析笔记

> 原文档：Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_Log抓取指导_V1.0.0_Preliminary_20240509.pdf
> 文档版本：V1.0.0（临时版本）  日期：2024-05-09

---

## 一、文档结构概览

| 章节 | 内容 |
|---|---|
| 第1章 | 引言：工具介绍与使用前提 |
| 第2章 | Windows 环境下 Log 和 Dump 文件抓取 |
| 第3章 | Linux/Android 环境下 Log 抓取（车机场景）|
| 第4章 | AT 命令详解（apdump / cpdump / aplog / cplog）|
| 附录 | 参考文档与术语缩写 |

---

## 二、引言（§1）

### 涉及工具

| 工具 | 获取方式 | 用途 |
|---|---|---|
| **QLog** | 联系移远技术支持 | Log 抓取 |
| **rdp_transfer / CATStudio** | 联系移远技术支持协助安装 | CP 侧 Log / Dump 抓取 |
| **TFTPD64** | 官方网站自行下载 | AP/CP dump 文件传输 |
| **diagsulogger** | 联系移远获取源码编译 | Linux/Android 环境 Diag Log 抓取 |

### 异常排查首要步骤
模块出现异常时，首先执行：
```
AT+CFUN?    # 查看当前功能模式
AT+CPIN?    # 查看 (U)SIM PIN 状态
```
根据结果初步判断问题原因，再决定是否需要抓取 Log。

---

## 三、Windows 环境下 Log 抓取（§2.1）

### 3.1 通过串口工具抓取 AP 侧 Log（§2.1.1）

**连接方式：** 串口线连接模块调试串口，波特率 115200，关闭流控。

**常用命令：**

```bash
# 打印 kernel log
echo 9 > /proc/sys/kernel/printk

# 打印调试休眠过程的全部内核日志（调试卡死问题时使用）
echo N > /sys/module/printk/parameters/console_suspend

# 打印 QLog 接口的 API log
logcat

# 打印指定服务的 API log（如音频）
logcat -s ql_audio
```

**提高 Debug 日志级别（解决开机卡死问题）：**

在 SDK 根目录执行 `make kernel_menuconfig`，进入：
`Kernel hacking` → `printk and dmesg options`

设置以下三项为最大值：
- Default console loglevel：**15**
- Quiet console loglevel：**15**
- Default message log level：**7**

### 3.2 通过 CATStudio 工具抓取 CP 侧 Log（§2.1.2）

**前提：** 上位机需安装移远提供的 USB 驱动。

**步骤：**
1. 打开 CATStudio，选择"在线解析日志（Generic Target Online）"
2. 在主界面右下角选择 `Device Communication` → `Device 0` → `Settings`
3. 选择 `Quectel USB DIAG Port`，波特率 115200，点击 OK
4. 点击 `Logger` 选项卡 → `CpLogStart`
5. 点击 `Database` → `Update`，选择固件包 `/dbg/` 目录下的 `LWG_MDB.txt`（Communication）和 `Boerne_DIAG.mdb.txt`（Application），点击 `UpdateAll`
6. 数据库匹配成功后 Logger 界面亮起两个绿灯，开始录制
7. 复现问题后，`Modules` → `LogViewer` 查看日志
8. `Log` → `Export Log-File...` 导出 `.sdl` 文件

> **注意：** `LWG_MDB.txt` 与 `Boerne_DIAG.mdb.txt` 位于固件包 `/dbg/` 目录下。

### 3.3 通过 CATStudio 工具抓取射频 Log（Sulog）（§2.1.3）

Sulog 用于分析极端掉网场景中的射频相关问题，需要安装特殊驱动（ASR Sulog Device）。

**步骤：**
1. 在设备管理器中找到 `Marvell DEBUG` 端口，手动安装 ASR 驱动（`windows10_sulog_install_driver/sulogusb.inf`）
2. 安装成功后设备管理器出现 `ASR Sulog Device`
3. 在上位机新建 `sulog` 文件夹
4. 打开 CATStudio，切换到 `Sulog` 标签页：
   - `Insert input file`：填写 `.bin` 文件保存路径
   - `Recording Method`：Stop record after，建议改为 100MB
   - `Record By`：USB Streaming
   - `Additional Options`：勾选 `Auto start after reset`
   - `UE Communications`：选择 `HWSulog`
5. 在 `Device Communication` 中选择 `Device 1`（`Quectel USB DIAG Port`），点击 `Start`
6. 点击 `Stop` 结束抓取（Sulog 有概率卡住，需等 CATStudio 恢复正常后再点 Stop）

---

## 四、Windows 环境下 Dump 文件抓取（§2.2）

### 4.1 AP 侧 Dump 抓取（§2.2.1）

#### 开启 AP Dump 的两种方式

**方式一（推荐，重启后需手动关闭）：**
```
AT+QCFG="apdump",1    # 开启 AP dump
AT+QCFG="apdump",0    # 抓取完成后关闭
```

**方式二（重启后自动关闭）：**
```bash
echo 1 > /proc/sys/kernel/rdup
```

#### 抓取步骤
1. 开启 AP dump 模式后，等待模块发生 dump
2. 模块自动重启进入 dump 模式，自动枚举为 RNDIS 网卡
3. **关闭上位机防火墙**（否则 dump 文件可能被删除）
4. 打开 TFTPD64，选择上位机分配的 IP（如 `192.168.225.159`）
5. 在模块 debug 口执行：`rdp_transfer 192.168.225.159`
6. 传输完成后在 TFTPD64 同级目录找到 dump 文件

**AP 侧 dump 文件示例：**
```
RAMDUMP_700101-0035.gz
RAMDUMP_700101-0035.txt
rdp_ok.txt
```

> ⚠️ 模块进入 dump 模式后**无法软件重启**，只能硬件断电重启。

#### last_dmesg 机制（§2.2.1.2）

模块支持 `last_dmesg` 机制，自动将**重启前的内核日志**保存在 DDR 中（仅限软件重启，如执行 `reboot` 或发生 dump 重启，断电重启无效）。

```bash
cat /proc/last_kmsg    # 查看上次重启前的内核日志
```

> 这对于分析开机 crash、重启原因非常有用。

### 4.2 CP 侧 Dump 抓取（§2.2.2）

#### 开启 CP Dump

```
AT+QCFG="cpdump",1    # 开启 CP dump 模式（需重启生效）
AT+QCFG="cpdump",0    # 抓取完成后关闭（需重启生效）
```

#### 抓取步骤
1. 执行 `AT+QCFG="cpdump",1`，重启模块生效
2. 等待 CP 发生 dump，模块自动重启进入 dump 模式，枚举为 RNDIS 网卡
3. **关闭上位机防火墙**
4. 打开 TFTPD64，选择上位机分配的 IP
5. 在模块 debug 口执行：`rdp_transfer 192.168.225.159`
6. 传输完成后在 TFTPD64 同级目录找到 dump 文件

**CP 侧 dump 文件示例：**
```
COM_DDR_700101-0045.bin
DSP_DDR_700101-0045.bin
rdp_ok.txt
```

> ⚠️ 模块进入 dump 模式后**无法软件重启**，只能硬件断电重启。

#### 自动保存 CP Dump 到 /sdcard（§2.2.2.2）

> 默认情况下，模块发生 CP dump 后，在根文件系统 `/sdcard/` 目录下会**自动保存 CP dump 文件**，前提是 `/sdcard/` 目录已挂载存储设备。`/sdcard/` 目录默认未挂载，客户可按需自行挂载内存。

- dump 文件以生成日期命名，大小通常不超过 100M
- 挂载分区剩余空间不足时，系统自动删除最早的 dump 文件
- **我们的 bind mount 方案（`/media/sdcard` → `/sdcard`）完全符合官方指导**

若不需要自动保存，执行 `AT+QCFG="cpdump",2` 关闭该功能。

### 4.3 完整 CP Dump 文件列表（§2.2.3 截图确认）

正常的 CP dump 目录（如 `1_cp_Assert_1970-1-1-0-4-47/`）包含以下文件：

| 文件名 | 大小 |
|---|---|
| cmdline.txt | 326 B |
| com_DDR_RW.bin | 9,437,188 B（~9MB）|
| com_DSP_DDR.bin | 2,621,440 B |
| com_DTCM.bin | 65,536 B |
| com_EE_Hbuf.bin | 1,036 B |
| com_GBDump.bin | 1,048,576 B |
| com_GplcSpy.bin | 26,532 B |
| com_IPCDump.bin | 4,104 B |
| com_ITCM.bin | 65,536 B |
| com_L1aSpy.bin | 6,000 B |
| com_MtpsRam.bin | 65,536 B |
| com_RECORD.bin | 58,931 B |
| com_WDT_END.bin | 65,536 B |
| com_aplpSpy.bin | 8,184 B |
| com_dtcSpy.bin | 1,496 B |
| com_rti_tsk.bin | 2,132 B |
| com_wdtKiCK.bin | 9,437,188 B |
| diag.sdl | 124,889 B（~122KB）|
| dmesg.txt | 114 B |
| eeh_journal.txt | 4,114 B |
| ubootlog.txt | 4,198 B |

> 与客户反馈"正常应有的文件"（图2）完全一致，证实客户现场 dump 确实被截断。

### 4.4 手动触发 Dump（§2.2.3）

```bash
# 手动触发 AP 侧 dump
echo p > /dev/ramdump_ctl

# 手动触发 CP 侧 dump
echo a > /dev/acipc
```

---

## 五、Linux/车机环境下 Log 抓取（§3）

### 5.1 整体架构

车机模块安装后无法通过 USB 直连上位机抓 Log，通过以下两种方式连接：
- **USB/ECM 网卡**：模块与车机通过 USB 相连
- **局域网（RGMII）**：模块与车机通过以太网相连

集成移远提供的 `diag_tranfer`（`diagsulogger`）工具后，可实现车机远程抓取 Diag Log。

### 5.2 diagsulogger 工具参数说明（§3.1.1）

```
Usage: diagsulogger [-m max_size] [-n num_logs] [-s save_path] [-t type]
                    [-d device] [-l listen_if] [-p port] [-b baudrate]
                    [-v] [-w] [-T timeout]

-m  保存日志文件的最大大小（MB），默认 128
-n  日志文件轮转数量，默认 0（不轮转）
-s  日志保存目录，默认当前目录
-t  日志类型：0=USB Diag, 1=TTY Diag, 2=TCP Diag, 3=SW Sulog,
              4=HW Sulog, 5=HW Sulog大缓冲, 6=TCP Sulog
-d  设备路径（如 /dev/ttyUSB0）或 IP:port（如 192.168.1.1:32122）
-l  TCP 服务端监听的网卡接口（如 eth0）
-p  TCP 服务端端口，默认 32768
-b  串口波特率，默认 115200
-v  打印十六进制 dump
-w  等待 USB 就绪再启动，默认不等待
-T  USB 就绪后启动抓取的超时时间（ms），默认 300
```

### 5.3 通过 USB 抓取 Log（§3.1.2）

```bash
# 抓取 Diag log（生成 .sdl 文件，可用 CATStudio 分析）
./diagsulogger -t 0

# 抓取 Sulog（射频相关，生成 .bin 文件）
./diagsulogger -t 3
```

> 抓取完成后当前目录生成以日期命名的 `.sdl` 或 `.bin` 文件。

### 5.4 通过局域网抓取 Log（§3.1.3）

**步骤1：修改模块配置文件**
```bash
vi /system/etc/mrvl_tel_diag.cfg    # 文档示例路径
# 本设备实际路径为 /etc/mrvl_tel_diag.cfg
```

修改以下两项，重启生效：
```ini
default_media=3          # 改为 TCP 网络模式
diag_ip=192.168.225.1    # 改为模块在局域网中的实际 IP
```

**步骤2：在车机/上位机执行抓取命令**
```bash
# 抓取 Diag log
./diagsulogger -t 2 -d 192.168.225.1:12345

# 抓取 Sulog
./diagsulogger -t 6 -d 192.168.225.1:5109
```

---

## 六、配置模块中 Diag Log 存储（§3.2）

### 6.1 两套机制的区别（重要）

| 机制 | 配置方式 | 作用 | 触发时机 |
|---|---|---|---|
| **CP dump** | 自动（`/sdcard` 挂载即生效）| 崩溃时抓取内存快照 | CP 发生 dump 时 |
| **Diag Log 持久化** | `mrvl_tel_diag.cfg` | 实时滚动录制 `.sdl` 日志 | 正常运行期间持续录制 |

**结论：修改 `mrvl_tel_diag.cfg` 对 CP dump 没有任何影响。**（已实验验证）

### 6.2 自动保存 Diag Log 至 SD 卡（§3.2.1）

修改 `/etc/mrvl_tel_diag.cfg`（本设备实际路径，文档示例为 `/system/etc/mrvl_tel_diag.cfg`）：

```ini
[SystemSettings]
default_media=2          # 改为 2（SD 卡模式）

[SDSettings]
max_log_num=15           # 最多保存文件数
log_size=10              # 每个文件大小上限（MB）
log_path=/media/sdcard   # SD 卡实际挂载路径（文档示例为 /mnt/mmcblk0p2，需按设备修改）
folder_prefix=Log        # 日志文件夹前缀
```

重启后，Diag Log 自动保存到 `<log_path>/Log001/sdlog000.sdl` 等滚动文件中。

### 6.3 开启/关闭自动保存 Diag Log（§3.2.2）

Diag log 由 **AP log** 和 **CP log** 两部分组成，可分别控制：

```bash
# 开启/关闭 AP log 自动保存
AT+QCFG="aplog",<level>
# level=0：按 telinit 脚本默认等级输出
# level=1~8：指定输出等级
# level=15：关闭 AP log 输出
# level=31：输出所有等级

# 开启/关闭 CP log 自动保存
AT+QCFG="cplog",<level>
# level=0：不输出 CP log
# level=1：输出 CP log
```

同时执行以下两条命令即可**关闭全部 Diag log 自动保存**：
```
AT+QCFG="aplog",15
AT+QCFG="cplog",0
```

> 模块启动默认会一直输出保存日志，如不需要可在开机脚本中执行上述命令关闭。

---

## 七、AT 命令详解（§4）

### 7.1 AT+QCFG="apdump" —— 设置 AP Dump 等级（§4.2.1）

```
AT+QCFG="apdump"[,<level>]
```

| level | 行为 |
|---|---|
| 0 | 关闭 AP dump；AP 侧发生 dump 时**仅模块重启** |
| 1 | 打开 AP dump；AP 侧发生 dump 时**模块重启并进入 dump 模式** |

- 该命令**立即生效**，参数自动保存

### 7.2 AT+QCFG="cpdump" —— 设置 CP Dump 等级（§4.2.2）

```
AT+QCFG="cpdump"[,<level>]
```

| level | 行为 |
|---|---|
| **0（默认）** | 关闭 CP dump 模式；CP 发生 dump 时**仅 CP 重启**，并**自动保存 dump 到 `/sdcard/`** |
| 1 | 打开 CP dump 模式；CP 发生 dump 时**整模块重启并进入 dump 模式**（需 TFTPD64 抓取）|
| 2 | 关闭 CP dump；CP 发生 dump 时仅 CP 重启，**不保存 dump 文件** |

- 该命令**重启后生效**，参数自动保存
- `cpdump=1` 进入 dump 模式后无法软件重启，只能硬件断电

> 查询当前值：`serial_atcmd AT+QCFG="cpdump"`

### 7.3 AT+QCFG="aplog" —— 设置 AP Log 等级（§4.2.3）

```
AT+QCFG="aplog",<level>
```

| level | 行为 |
|---|---|
| 0 | 按 telinit 脚本默认等级输出 AP log 到 diag 口 |
| 1~8 | 按指定等级输出 AP log 到 diag 口 |
| 15 | 关闭 AP log 输出 |
| 31 | 输出所有等级 AP log 到 diag 口 |

- 该命令**立即生效**，参数**不自动保存**（重启后恢复默认）

### 7.4 AT+QCFG="cplog" —— 设置 CP Log 等级（§4.2.4）

```
AT+QCFG="cplog",<level>
```

| level | 行为 |
|---|---|
| 0 | 关闭 CP log 输出，CP 侧不输出 CP log 到 diag 口 |
| 1（默认）| 打开 CP log 输出，CP 侧输出 CP log 到 diag 口 |

- 该命令**立即生效**，参数**不自动保存**（重启后恢复默认）

> ⚠️ 以上 AT 命令适用于固件版本 `AG35CETCAR01A06M2G_OCPU` 及之后的版本。

---

## 八、mrvl_tel_diag.cfg 完整参数说明

```ini
[SystemSettings]
diag_port=/dev/diagdev        # diag 端口设备
diag_usb=/dev/ttydiag0        # diag USB TTY 设备
diag_usb_key=diag             # diag USB key
logcat_enable=1               # 是否输出 logcat 消息
default_media=1               # 日志传输方式：0=关闭, 1=USB(默认), 2=SD卡, 3=TCP
silent_type=1                 # Diag Silent 类型：0=关闭, 1=开启(默认)
cache_size=1024               # 写缓存大小（KB）
mrvl=1                        # Marvell 平台标识

[SDSettings]
max_log_num=30                # 最多日志文件数
folder_num=2                  # 当前文件夹编号
auto_inc=1                    # 启动时自动递增文件夹编号
log_size=10                   # 日志文件大小上限（MB）
mount_path=/mnt/mmcblk0       # SD 卡挂载点（需按实际设备修改）
log_path=/sdcard              # 日志保存目录（需按实际设备修改）
folder_prefix=Log             # 日志文件夹前缀

[BootLogSettings]
enable=0                      # 是否开启 Boot Log SD 模式
max_log_num=5                 # Boot Log 最多文件数
log_size=10                   # Boot Log 文件大小上限（MB）
log_path=/log/Fs              # Boot Log 保存目录

[NetworkSettings]
diag_net_dev=rndis0           # 网络接口名
acat_ip=192.168.1.100         # ACAT IP 地址
diag_ip=127.0.0.1             # DIAG IP 地址（TCP 模式时改为模块实际 IP）
diag_netmask=255.255.255.0    # 子网掩码
port=12345                    # 网络端口
```

**本设备有效路径：** `/etc/mrvl_tel_diag.cfg`（overlayfs 合并视图）
**持久化修改路径：** 直接修改 `/etc/mrvl_tel_diag.cfg`，overlayfs 自动写入 `/overlay/etc/mrvl_tel_diag.cfg`，重启后生效。

---

## 九、当前问题定位总结

### CP dump 不完整问题

| 项目 | 状态 | 说明 |
|---|---|---|
| `/sdcard` bind mount 机制 | ✅ 正确 | 已验证，符合官方指导 |
| SD 卡空间 | ✅ 充足 | 53.4G 可用 |
| `AT+QCFG="cpdump"` 配置 | ⚠️ 待确认 | 建议执行查询命令确认 |
| `mrvl_tel_diag.cfg` 配置 | ✅ 与 dump 无关 | 已实验验证，不影响 CP dump |
| 手动触发 dump（Assert）| ✅ 完整 | 得到完整 `.tar.gz`（2.3M）|
| 客户现场 dump（DSP_COM_ERR）| ❌ 截断 | 仅 3 个文件，diag.sdl 只有 1KB |

### 根本原因
`DSP_COM_ERR_0x8164` 错误触发了比 Assert 更快的硬件复位，EEH 驱动没有足够时间完成全部 dump 文件的写盘。

### 待移远回答
> `DSP_COM_ERR_0x8164` 错误的复位流程中，是否有等待 EEH dump 写盘完成的保护机制？手动触发（`echo a > /dev/acipc`）的 Assert 类型 dump 完整，说明机制本身正常，该错误类型的复位时序与 Assert 不同，导致 EEH 无法完成写盘。

---

## 十、附录

### 参考文档
| 文档名 | 说明 |
|---|---|
| 文档[1] | QuecOpen 详细信息参考文档 |
| 文档[2] | Quectel USB 驱动安装文档 |

### 术语缩写
| 缩写 | 全称 |
|---|---|
| AP | Application Processor（应用处理器）|
| CP | Communication Processor（通信处理器/Modem）|
| EEH | Error Event Handler（错误事件处理器，负责 CP 崩溃时写 dump）|
| SDK | Software Development Kit |
| SDL | Spiral Diag Log（CATStudio 使用的日志格式）|
| Sulog | Sulog（射频相关日志）|
| TFTPD | Trivial File Transfer Protocol Daemon |
