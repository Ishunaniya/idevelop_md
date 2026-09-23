# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) Log 抓取指导 — 全量分析

> 本文是对原始 PDF《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_Log抓取指导_V1.0.0_Preliminary_20240509.pdf》的逐章全量精读还原。
> 讲的是**如何抓取模块 AP 侧 / CP 侧的 log 与 dump 文件**——Windows 上位机环境（CATStudio/TFTPD64）和已装车场景（Linux/Android 车机 + diag_tranfer 工具）两条路径，以及配套 4 条 AT 命令。
> **本文档与当前 `fix_cp_dump` 分支、`diag/` 模块直接相关，是 6 份 PDF 中相关度最高的一份，重点细读。**

---

## 0. 文档元信息

| 项 | 内容 |
|---|---|
| 文档名 | AG35-CET&AG35-EUT QuecOpen(SDK) Log 抓取指导 |
| 适用模块系列 | LTE Standard 模块系列：**AG35-CET、AG35-EUT** |
| 版本 | 1.0.0（**临时版本 Preliminary**） |
| 日期 | 2024-05-09 |
| 状态 | 临时文件 |
| 总页数 | 40 页（正文 1~39） |
| 作者 | Gabriel LI |
| 厂商 | 上海移远通信技术股份有限公司（Quectel） |

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| -（创建） | 2024-05-09 | Gabriel LI | 文档创建 |
| 1.0.0 | 2024-05-09 | Gabriel LI | 临时版本 |

---

## 1. 整书目录树

```
1  引言
2  Windows 环境下 Log 和 Dump 文件抓取
   2.1  Log 抓取
        2.1.1  通过串口工具抓取 AP 侧 Log
        2.1.2  通过 CATStudio 工具抓取 CP 侧 Log
        2.1.3  通过 CATStudio 工具抓取射频 Log（Sulog）
   2.2  Dump 文件抓取
        2.2.1  抓取 AP 侧 Dump 文件
               2.2.1.1  抓取 Dump 文件（TFTPD64 + rdp_transfer）
               2.2.1.2  配置模块中内核 Log 存储（last_kmsg）
        2.2.2  抓取 CP 侧 Dump 文件
               2.2.2.1  抓取 Dump 文件
               2.2.2.2  配置模块中 Dump 文件存储（/sdcard/ 自动保存）
        2.2.3  手动触发 Dump
3  Linux 和 Android 环境下 Log 抓取
   3.1  抓取 Log 步骤
        3.1.1  集成 diag_tranfer 至车机应用（diagsulogger 编译）
        3.1.2  通过 USB 连接模块场景下抓取 Log
        3.1.3  通过局域网连接模块场景下抓取 Log
   3.2  配置模块中 Diag Log 存储
        3.2.1  自动保存 Diag Log 至模块内存（mrvl_tel_diag.cfg）
        3.2.2  开启/关闭自动保存 Diag Log 功能（aplog/cplog）
4  AT 命令（apdump / cpdump / aplog / cplog）
5  附录 参考文档及术语缩写
```

> 文档含大量截图（图 1~25，CATStudio/TFTPD64/设备管理器界面），但**操作命令和参数文本可 100% 抽取**；下文已把所有命令与参数表完整还原。

---

## 2. 逐章全量内容

### 第 1 章 引言

- AG35-CET / AG35-EUT 支持 **QuecOpen® 方案**（基于 Linux 的嵌入式开发平台，用于 IoV）。
- 本文档介绍用 **QLog、rdp_transfer、CATStudio、TFTPD64** 等工具抓取 AP 侧/CP 侧 log 与 dump。
- **异常排查首步**：模块出异常时，先用 `AT+CFUN?` 和 `AT+CPIN?` 查当前功能模式与 (U)SIM PIN 状态，初判问题原因；配置有误则用对应 AT 命令重配；无法判断/解决时收集 log 提交移远技术支持。
- 工具获取渠道：
  - **QLog**、**rdp_transfer**、**CATStudio**：联系移远技术支持获取/协助安装。
  - **TFTPD64**：官网自取 `https://bitbucket.org/phjounin/tftpd64/downloads`。

---

### 第 2 章 Windows 环境下 Log 和 Dump 文件抓取

调试阶段把模块用 USB/串口连到 Windows 上位机，按需抓 log。

#### 2.1.1 通过串口工具抓取 **AP 侧 Log**

1. 串口线连模块调试串口 ↔ 上位机；串口工具选 COM 口，**115200 bps、关闭流控**，上电即显示开机 log。
2. 回车，在 `OpenWrt login:` 输 **用户名 `root` / 密码 `oelinux123`** 登录 Linux。
3. AP 侧 log = **kernel log + QLog 接口 API log**，抓取方式：
   - `echo 9 > /proc/sys/kernel/printk` —— 打印 kernel log
   - `echo N > /sys/module/printk/parameters/console_suspend` —— 打印休眠过程的内核全部日志
   - `logcat` —— 打印 QLog 接口的 API log
   - `logcat -s ql_audio` —— 抓指定服务（如音频）的 API log

**开机卡死定位**（SDK 默认 Debug 串口日志权限不高以加快开机）：
- `make kernel_menuconfig` → `Kernel hacking` → `printk and dmesg options`
- 把 `Default console loglevel`、`quiet console loglevel`、`Default message log level` 设为最大：**15、15、7**。

#### 2.1.2 通过 CATStudio 抓取 **CP 侧 Log**（Windows，USB Diag 端口）

- 前提：装移远 USB 驱动（见文档 [2]）。
- 流程：CATStudio → "在线解析日志" → 右下角 `Device Communication` → `Device0` → `Settings` → 选 **`Quectel USB DIAG Port`，115200 bps**。
- `Logger` 选项卡 → `CpLogStart`（变 `CpLogStop`）→ `Database` → `Update`。
- **Database 文件配对**（位于**固件包 `/dbg/` 目录下**）：
  - `Communication` → `TEXT` → `Text file` 选 **`LWG_MDB.txt`**
  - `Application` → `TEXT` → `Text file` 选 **`Boerne_DIAG.mdb.txt`**
  - `UpdateAll`，配对成功亮两个绿灯。
- `Modules` → `LogViewer` 打开抓取界面；复现问题后 `Log` → `Export Log-File…` → `Output` 导出。

#### 2.1.3 通过 CATStudio 抓取 **射频 Log（Sulog）**

- 用途：**极端掉网场景**需分析射频相关 log。
- 移远默认 USB 驱动**未集成**抓 Sulog 的原始驱动，需联系技术支持获取。
- 流程：设备管理器选 **`Marvell DEBUG` 端口** → 装原始驱动 → 建 `sulog` 文件夹 → 配 CATStudio → 抓取（示例开了 `Auto Start`）。
- **坑**：Sulog 有概率卡住，等 CATStudio 恢复正常后再稍等片刻点 `Stop`。

#### 2.2 Dump 文件抓取

- 模块**默认自动保存 dump 文件**。用 `AT+QCFG="apdump"` / `AT+QCFG="cpdump"` 配置 dump 开关及是否保存 CP dump（详见第 4 章）。
- 省略参数即查询当前配置。

##### 2.2.1.1 抓取 **AP 侧 Dump**（TFTPD64 + rdp_transfer）

- **步骤1 配为 dump 模式**（二选一）：
  - 方式一：`AT+QCFG="apdump",1` 打开 AP dump（**立即生效**）；模块 dump 重启后进入 dump 模式；**抓完务必 `AT+QCFG="apdump",0` 关闭**。
  - 方式二：`echo 1 > /proc/sys/kernel/rdup` 打开 AP dump；再次重启即关闭。
  - dump 后自动重启进入 dump 模式（抓取模式），并**自动切到 RNDIS 网卡模式**。
- **步骤2**：关上位机防火墙（模块 DHCP 获 IP，防火墙可能误判不安全局域网删掉 dump 文件）。
- **步骤3**：打开 TFTPD64 选上位机分配的 IP；模块 dump 重启后在 debug 口执行
  `rdp_transfer 192.168.225.159`（IP 换成实际地址），传输完成后 dump 文件存在工具同级目录。
- **重要坑**：**模块进入 dump 模式后无法软件重启，需硬件重置或断电重启。**

##### 2.2.1.2 内核 Log 存储（last_kmsg）

- 模块支持 **`last_dmesg` 机制**：自动把**重启前**的内核日志存在 DDR（仅限**未断电重启**，如 `reboot`、dump 后重启）。
- 查看：`cat /proc/last_kmsg` —— 看上一次重启时的内核日志。

##### 2.2.2.1 抓取 **CP 侧 Dump**

- 步骤1：`AT+QCFG="cpdump",1` 打开 CP dump，抓完 `AT+QCFG="cpdump",0` 关闭。**该命令重启后生效**（注意与 apdump 立即生效不同）。
- dump 后自动重启进入 dump 模式，自动切 RNDIS。
- 步骤2/3 同 AP：关防火墙 + TFTPD64 + `rdp_transfer <IP>`。
- 同样的坑：**dump 模式无法软件重启，需硬件复位/断电。**

##### 2.2.2.2 配置模块中 Dump 文件存储（**与 open_dial 强相关**）

- 默认情况下，CP dump 后会在根文件系统 **`/sdcard/` 目录自动保存 CP dump 文件，前提是 `/sdcard/` 已挂载存储设备**。
- **`/sdcard/` 默认未挂载**，客户需自行挂载内存才能保存 CP dump。dump 时系统自动检测：挂载则保存，未挂载则不保存。
- dump 文件**以生成日期命名**，单个**通常 ≤ 100 MB**；分区剩余空间不足时**自动删最早的 dump** 腾位置。
- 不需要自动保存时：`AT+QCFG="cpdump",2`，返回 OK 后**重启生效**。

##### 2.2.3 手动触发 Dump（调试用）

- 手动触发 **AP 侧 dump**：`echo p > /dev/ramdump_ctl`
- 手动触发 **CP 侧 dump**：`echo a > /dev/acipc`

---

### 第 3 章 Linux / Android 环境下 Log 抓取（**已装车场景**）

模块装车后无法用 USB 直连上位机；车机（Linux/Android）集成移远 **`diag_tranfer`** 工具即可远程抓 Diag log，再上传服务器分析。车机可通过 **USB 或局域网** 连模块。

#### 3.1.1 集成 diag_tranfer（编译 diagsulogger）

- 向技术支持要 `diag_tranfer` 源码，编译后集成进车机应用。
- 改 Makefile 的三个编译链（Ubuntu 自动用系统链可跳过）→ `make` → `./diagsulogger -h`。

**`diagsulogger` 工具帮助（ASR DIAG / SULOG Logger v1.0.0）完整参数**：

```
Usage: diagsulogger [-m max_size] [-n num_logs] [-s save_path] [-t type]
                    [-d device] [-l listen_if] [-p port] [-b baudrate] [-v] [-w] [-T timeout]
```

| 参数 | 含义 | 默认 |
|---|---|---|
| `-m` | 单个日志文件最大 MB | 128 |
| `-n` | 轮转日志文件个数 | 0（不轮转） |
| `-s` | 日志保存目录（空=仅 TCP） | `.` |
| `-t` | log 类型：**0 USB Diag / 1 TTY Diag / 2 TCP Diag / 3 SW Sulog / 4 HW Sulog / 5 HW Sulog 大缓冲 / 6 TCP Sulog** | 0 |
| `-d` | TTY 设备路径(`/dev/ttyUSB0`) 或 `IP:port`(TCP) 或 USB 序列号 | 空(自动) |
| `-l` | TCP server 监听网卡(如 eth0) | 空(不监听) |
| `-p` | TCP server 监听端口（需配合 -l） | 32768 |
| `-b` | 串口波特率（仅 -t 1 有效） | 115200 |
| `-v` | 详细输出读到数据的十六进制 dump | - |
| `-w` | USB 无设备时等待而非退出 | - |
| `-T` | USB ready 后开始抓取的超时 ms | 300 |

#### 3.1.2 USB 连接抓取

- `./diagsulogger -t 0` —— 抓 Diag log，生成以日期命名的 **`.sdl`** 文件，CATStudio 可打开分析网络问题。
- `./diagsulogger -t 3` —— 抓 Sulog（极端掉网/射频分析），生成以日期命名的 **`.bin`** 文件。
- 备注：Ubuntu 上仅管理员权限可操作 USB；**常见网络问题只需抓 Diag log**。

#### 3.1.3 局域网（以太网）连接抓取

- 改配置文件 **`/system/etc/mrvl_tel_diag.cfg`**：
  ```
  #Default Media: 0-off, 1-usb (default), 2-sd, 3-tcp
  default_media=3
  #DIAG IP address
  diag_ip=192.168.225.1   # 改成模块在局域网的实际 IP
  ```
  改完**重启生效**。
- 抓取命令（ECM 网卡为例）：
  - Diag log：`./diagsulogger -t 2 -d 192.168.225.1:12345`
  - Sulog：`./diagsulogger -t 6 -d 192.168.225.1:5109`
- 3.1.3.2 给出两个 AG35-CET 经以太网互抓的组网测试示例。

#### 3.2 配置模块中 Diag Log 自动存储

##### 3.2.1 自动保存到模块内存（mrvl_tel_diag.cfg）

- 建议用 **EMMC/SD 卡**单独分区存 log，**不建议用 flash**（读写寿命有限）。
- 改 `mrvl_tel_diag.cfg`：`default_media=2`（存本地内存）+ `[SDSettings]`：

```
[SDSettings]
max_log_num=15            # 可保存文件数上限
folder_num=0              # 当前文件夹编号
auto_inc=1                # 开机自增文件夹编号
log_size=10               # 单文件大小（MB）
mount_path=/mnt/mmcblk0   # SD 挂载点
log_path=/mnt/mmcblk0p2   # 日志目录
folder_prefix=Log         # 文件夹名前缀
```

##### 3.2.2 开启/关闭自动保存

- AP log：`AT+QCFG="aplog",<level>`
  - 0 = 按 telinit 脚本默认等级；15 = 不输出；31 = 全部等级；1~8 = 指定等级。
- CP log：`AT+QCFG="cplog",<level>`（0 不输出 / 1 输出）。
- **同时 `AT+QCFG="aplog",15` + `AT+QCFG="cplog",0` = 关闭自动保存 Diag log。**
- 模块启动默认一直输出保存日志，不需要可在**开机脚本**部署上述两命令关闭。

---

### 第 4 章 AT 命令详解

> AT 类型：测试 `AT+<cmd>=?` / 查询 `AT+<cmd>?` / 设置 `AT+<cmd>=<p1>...` / 执行 `AT+<cmd>`。下表最大响应时间均 **300 ms**。

#### 4.2.1 `AT+QCFG="apdump"[,<level>]` — AP Dump 等级

- 特性：**立即生效**；参数配置**自动保存**。
- `<level>`：**0** 关闭（dump 时仅模块重启）；**1** 打开（dump 时重启并进入 dump 模式）。

#### 4.2.2 `AT+QCFG="cpdump"[,<level>]` — CP Dump 等级（**open_dial 核心**）

- 特性：**重启生效**；参数配置**自动保存**。
- `<level>`：
  - **0** 关闭 CP dump：CP 侧 dump 时**仅 CP 重启，并保存 CP dump 日志到 `/sdcard/` 目录**
  - **1** 打开 CP dump：dump 时**模块重启并进入 dump 模式**
  - **2** 关闭 CP dump：dump 时**仅 CP 重启**（不保存日志）

> ⚠️ **辨析**：level 0 与 2 都"关闭 dump 模式"（dump 时只重启不进抓取模式），区别在 **0 仍落盘 `/sdcard/` 日志，2 完全不保存**。要采集 CP dump 日志而又不想进入"卡死等抓取"的 dump 模式，应选 **0**。

#### 4.2.3 `AT+QCFG="aplog"[,<level>]` — AP Log 等级

- 特性：**立即生效**；参数配置**不自动保存**。
- `<level>`：0 默认等级 / 1~8 指定等级 / 15 关闭 / 31 全部等级（输出到 diag 口）。

#### 4.2.4 `AT+QCFG="cplog",<level>` — CP Log 等级

- 特性：**立即生效**；参数配置**不自动保存**。
- `<level>`：0 关闭 / 1 打开（输出 CP log 到 diag 口）。
- **备注**：本章 AT 命令适用于 **`AG35CETCAR01A06M2G_OCPU` 及之后版本**。

---

## 3. 关键警告与坑（手册"备注"汇总）

1. **dump 模式无法软件重启** —— 模块进入 dump 模式后只能硬件复位/断电重启。对 open_dial 的含义：靠 `AT+CFUN=1,1` 或 exit 重拉都救不了已进入 dump 模式的模块。
2. **CP dump 落盘依赖 `/sdcard/` 已挂载** —— 默认未挂载就不保存；要留证据必须先挂存储。文件以日期命名、≤100MB、空间不足自动删最旧。
3. **apdump 立即生效 / cpdump 重启生效** —— 改 CP dump 配置后不重启不生效，易误判"没改成功"。
4. **`cpdump` level 0 vs 2** —— 0 落盘 `/sdcard/`，2 不保存；别配反导致丢失关键 dump。
5. **aplog/cplog 不自动保存配置** —— 掉电/重启后失效，需写进开机脚本才持久。
6. **抓 CP log 需 Database 文件配对** —— `LWG_MDB.txt` + `Boerne_DIAG.mdb.txt`（固件包 `/dbg/`），版本不匹配解析会乱。
7. **Sulog 抓取会卡** —— 等工具恢复再点 Stop，否则文件不完整。
8. **last_kmsg 只在未断电重启时有效** —— 断电后 DDR 内容丢失，抓不到上次内核日志。
9. **Linux 登录口令固定** —— `root` / `oelinux123`（出厂默认，量产需关注安全）。
10. **dump/Diag 抓取会切 RNDIS 网卡** —— 会打断正常 ECM 数据业务。

---

## 4. 对 open_dial 项目的适用性批注（重点）

> 当前分支正是 **`fix_cp_dump`**，且 CLAUDE.md 记录 v1.28.4 升级 SDK R02A04→R03A02 就是"解决 CP dump 问题"。本文档把 CP dump 的产生/落盘/抓取机制讲清楚了，**直接服务于本分支的根因排查与现场取证流程**。注意本文档面向 **AG35**，open_dial 跑 **EC200A（OpenCPU/ASR1803）**，但二者同属移远 ASR 平台 + ql-sdk，**AT 命令（apdump/cpdump/aplog/cplog 走的是通用 `AT+QCFG`）大概率通用**，落地前用 `serial_atcmd 'AT+QCFG="cpdump"'` 在 EC200A 上实测确认即可。 |

| 文档内容 | 与 open_dial 的关系 / 可借鉴点 |
|---|---|
| **`/sdcard/` 自动保存 CP dump** | open_dial 用 `/media/sdcard/dial_log` 存日志，模组的 CP dump 默认落 `/sdcard/`。**应确认 EC200A 上 `/sdcard/` 是否挂载**——没挂就采不到 CP dump，与"CP dump 问题查不到根因"直接相关。`diag/` 模块可考虑在故障快照里一并打包 `/sdcard/` 下的 CP dump 文件。 |
| **`AT+QCFG="cpdump"` 等级语义** | 排查阶段建议设 **`cpdump`=0**（仅 CP 重启 + 落盘日志，不进卡死的 dump 模式），既能拿证据又不中断 AP 业务；**生产**则要权衡：若设 1，CP dump 时整模块进 dump 模式且只能硬复位 —— 与 open_dial "OpenCPU 下不可整机复位影响同机业务"的约束冲突。`diag/` 采集前可先 `serial_atcmd` 查/设该等级。 |
| **手动触发 `echo a > /dev/acipc`（CP dump）** | 可用于**复现/验证**本分支的 CP dump 处理路径与恢复逻辑，无需等真实 hang。AP 侧用 `echo p > /dev/ramdump_ctl`。 |
| **`/proc/last_kmsg`（last_dmesg）** | `diag/` 快照目前抓 dmesg/logcat；**建议补抓 `/proc/last_kmsg`** —— 它保留了上次（dump/reboot 前）的内核日志，正是分析"CP dump 后重启"根因的关键，且未断电重启时有效，与 open_dial 的 L3 exit 重拉/CFUN 重启场景吻合。 |
| **`diagsulogger` 工具（TCP/USB 抓 Diag/Sulog）** | 现场远程取证方案。open_dial 装车后无法 USB 直连，可参考 `-t 2`（TCP Diag）/`-t 6`（TCP Sulog）经局域网抓 CP/射频 log，定位"35min+ CP 固件 hang 死循环"这类靠 AP 日志看不出的问题。 |
| **`mrvl_tel_diag.cfg` 自动存 Diag log** | 与 open_dial 自研的 `logger_sd.c`（按天分文件夹 + 40 天保留）是**两套并行日志**：前者是模组底层 Diag/CP log，后者是应用层拨号日志。排查 CP dump 需要前者，应评估是否开启模组侧 Diag 自动落盘（注意 flash 寿命，建议落 SD）。 |
| **异常排查首步 `AT+CFUN?` / `AT+CPIN?`** | 与 open_dial 状态机/恢复逻辑一致；`diag/` 故障快照可固化这两条查询作为 dump 现场的标配采集项。 |

**结论**：此文档对本项目（尤其 `fix_cp_dump` 分支）**相关度最高、可直接落地**。三条立即可做的事：① 确认 EC200A 上 `/sdcard/` 是否挂载、CP dump 是否真有落盘；② `diag/` 快照补抓 `/proc/last_kmsg` 与 `/sdcard/` 下 CP dump；③ 用 `echo a > /dev/acipc` 手动触发以验证 CP dump 处理与恢复路径。**注意所有 AT/路径需在 EC200A 实测核对**（本文档为 AG35）。
