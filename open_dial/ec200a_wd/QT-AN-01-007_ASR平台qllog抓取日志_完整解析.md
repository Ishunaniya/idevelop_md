# QT-AN-01-007：ASR 平台使用 qllog 抓取日志 —— 完整解析

> **来源文件**：`QT-AN-01-007-ASR平台使用qllog抓取日志.pdf`（8 页，移远 Quectel 官方应用笔记）
> **文档编号**：QT-AN-01-001（页眉标注）　**版本**：V1.0　**初版制定**：2024-11-18　**首次发行**：2025-02-20（编制：William liu）
> **保密级别**：公开
> **本解析约定**：第 1~7 章为 PDF 原文的完整忠实转录与整理（不增不减不臆测）；
> 第 8 章为针对本项目设备（EC200A/ASR1803）的适配性核验，**全部基于实测事实**，与原文严格区分。

**文档元信息（第 1~2 页封面与文件管控表，全部信息）**：

| 项 | 值 |
|---|---|
| 封面标题 | AN007：ASR 平台使用 qllog 抓取日志 |
| 页眉编号 | QT-AN-01-001（注意：文件名为 QT-AN-01-007，页眉编号与文件名不一致，PDF 原样如此） |
| 版本 | V1.0 |
| 初版制定时间 | 2024-11-18　最新更新时间：2024-11-18 |
| 文件变更记录 | 仅一条：2025-02-20 / V1.0 / 首次发行 / 编制 William liu（其余行为空白） |
| 保密级别 | 公开 ☑（绝密 ☐ 保密 ☐） |
| PDF 属性 | 作者 kelly，WPS 文字生成，创建于 2025-02-24 |
| 页脚联系方式 | 上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼 200233；邮箱 info@quectel.com；网址 www.quectel.com |

---

## 1. 目的（PDF 第 1 章，第 3 页）

原文要点（完整）：

- 产品化之后通常**没有预留 USB 口**，这对售后抓取 LOG 带来了困难；
- 同时越来越多的**车厂要求集成抓 LOG 相关的服务**，可以实现**远程抓 LOG 分析**；
- 基于这些需求，移远开发了 **qllogd 服务**，提供给客户或者前线 FAE，用于**简化抓 LOG**；
- ⚠️ **关键适用范围声明（原文）**：
  > "这个服务当前**只有在 ASR1806 平台提供**，需要更新 SDK 版本到 **2024 年 12 月以后**的版本。"

架构关系：`qllog`（命令行工具）和 SDK API 都是前端，真正干活的是后台服务 **`ql_logd`**——没有 `ql_logd`，qllog 命令无法工作。

---

## 2. 基于 SDK 接口的方式（PDF 第 2 章，第 3~4 页）

移远提供了一套 SDK 接口方便客户集成到应用中，使用方法和其他移远 SDK 接口一致。

- **头文件位置**：`ql-sysroots/usr/include/ql-sdk/ql_log.h`
- **例子代码**：`sample/test_sdk_api/m_log.c`

### 2.1 抓 LOG 的源

```c
#define QL_LOG_SOURCE_MODEM     0x01
#define QL_LOG_SOURCE_APP       0x02
#define QL_LOG_SOURCE_KERNEL    0x03
#define QL_LOG_SOURCE_DUMP      0x04
#define QL_LOG_SOURCE_SYSINFO   0x05
#define QL_LOG_SOURCE_GNSS      0x06
```

各源的含义（原文完整转录）：

| 源 | 含义 |
|---|---|
| **Modem** | 抓 CP LOG，**和 CatStudio 抓取保存的日志相同**，用于分析网络问题 |
| **App** | 抓应用的 LOG，**相当于 Logcat 的应用日志筛选了 system 和 main 这两个 buffer** |
| **Kernel** | 抓内核 **dmesg** 日志 |
| **Dump** | 收集系统 dump。但抓 dump 需要特殊操作（详见第 4 章） |
| **Sysinfo** | 指定时收集系统信息 |
| **Gnss** | 单收集 GNSS 的 **NMEA 报文**。注意：此命令**不会主动打开 GNSS**，需要先用 `ql_gnss` 相关 API 打开 GNSS |

### 2.2 LOG 等级

```c
#define QL_LOG_LEVEL_ERROR      1
#define QL_LOG_LEVEL_WARNING    2
#define QL_LOG_LEVEL_INFO       3
#define QL_LOG_LEVEL_DEBUG      4
```

- ERROR 等级只输出错误级别的日志
- WARNING 等级输出警告和错误级别的日志
- INFO 等级输出 INFO/WARNING/ERROR 等级的日志
- DEBUG 等级输出 DEBUG/INFO/WARNING/ERROR 等级的日志
- 即：**ERROR 输出最少，DEBUG 输出最多**（等级值越大越详细）

### 2.3 初始化和去初始化

```c
int ql_log_init(void);
int ql_log_deinit(void);
```

和其他 SDK 服务一样：使用之前需要初始化**连接 ql_logd 服务**，使用完毕之后需要断开服务。

### 2.4 启动和停止抓取 log 服务

```c
int ql_log_start(int source, int flag);
int ql_log_stop(int source);
```

- `source`：2.1 中描述的源；`flag`：**预留参数，当前没有使用，默认传 0 即可**
- 注意事项（原文）：
  1. `ql_log_start` 是**异步操作**——开启之后函数会立刻返回，由 `ql_logd` 服务在后台开对应的线程来收集，直到用户调用 `ql_log_stop` 关闭为止；
  2. **可以同时抓取多个不同的源**，只需要多次调用 `ql_log_start` 即可。

### 2.5 配置和获取配置

```c
int ql_log_get_config(int source, ql_log_config_t *p_config);
int ql_log_set_config(int source, ql_log_config_t *p_config);

typedef struct {
    char     log_dir[QL_LOG_DIR_NAME_MAX];  // 保存 log 的路径
    uint32_t log_level;                     // 抓取 log 的等级
    uint32_t log_file_size;                 // 单个 log 文件最大大小，单位 MB
    uint32_t log_file_num;                  // 最多能保存多少个日志文件
} ql_log_config_t;
```

注意事项（原文）：

1. 对于 **modem 日志**：由于 ASR 底层的限制，目前**只能支持 ERROR 和 DEBUG 两个等级**；
2. 对于 **sysinfo 日志**：只有 `log_file_num` 和 `log_dir` 是有效的，其他参数无意义；
3. **该配置只保存在内存里**，重启后会恢复默认值；要修改默认配置需通过配置文件（见第 5 章）。

### ＜特别注意＞（原文红字）

`/system/etc/mrvl_tel_diag.cfg` 文件里有个配置项：

```
#Default Media: 0-off, 1-usb (default), 2-sd, 3-tcp
default_media=3
```

- 这个配置项决定 **modem log 使用 tcp 模式还是 usb 模式**；
- 如果发现 **USB 连接不上 CatStudio**，先检查这个配置，把 `default_media=1` 然后保存、**重启生效**；
- 用于**临时连接 USB 抓 log** 的场景。

---

## 3. 基于 qllog 命令的方式（PDF 第 3 章，第 4~6 页）

第 2 章是 SDK 方式（需要用户开发应用程序）；qllog 是现成的命令行工具（截图显示版本 `qllog software v1.0 (2024-08-02)`）。

### 帮助截图全文转录（PDF 第 5 页黑底截图）

```
qllog software v1.0 (2024-08-02) - a tool to capture and store logs for ASR platform

Usage:
  qllog -s source [-p path] [-l level] [-z file-size] [-n file-num] [-t timeout]

Options:
  -s, --log-source     set log source to capture (avaliable source: modem, app, kernel, dump, sysinfo, gnss)
  -p, --log-path       set log store path
  -l, --log-level      set log level (1:error, 2:warning, 3:info, 4:debug)
  -z, --log-file-size  set log file max size in MB
  -n, --log-file-num   set log file max number
  -c, --get-config     get log config
  -m, --multi-source   set multi log source (bitmask: modem:0x02, app:0x04, kernel:0x08, dump:0x10, sysinfo:0x20, gnss:0x40)
  -t, --timeout        set log capture timeout in second
  -h, --help           print help message

Example:
  qllog -s app -t 5
  qllog -s kernel -t 5
  qllog -s modem -p /mnt/mmcblk0p1/log/modem
  qllog -m 0xE0 -t 5 (capture modem & app & kernel logs)
```

> ⚠️ **截图内自带笔误**：帮助信息最后一行示例写 `-m 0xE0` 并注释"capture modem & app & kernel logs"，
> 但按其自己给出的 bitmask（modem 0x02 | app 0x04 | kernel 0x08）应为 **`0x0E`**；
> PDF 正文（第 5~6 页）例 4 用的正是 `0x0E` 并给出计算过程。**以正文 `0x0E` 为准**，`0xE0` 实为 sysinfo|gnss。

### 参数完整解析（原文）

| 参数 | 长参数 | 含义 |
|---|---|---|
| `-s` | `--log-source` | 设置源 source（modem, app, kernel, dump, sysinfo, gnss），参考 2.1 |
| `-p` | `--log-path` | 设置对应源的 **LOG 保存路径** |
| `-l` | `--log-level` | 设置 LOG 等级（1:error, 2:warning, 3:info, 4:debug），参考 2.2 |
| `-z` | `--log-file-size` | 设置单个 LOG 文件最大大小（MB） |
| `-n` | `--log-file-num` | 设置最多保存 LOG 文件数量 |
| `-c` | `--get-config` | 获取对应源的配置信息 |
| `-m` | `--multi-source` | 设置多个源同时收集 LOG（bitmask 方式） |
| `-t` | `--timeout` | 设置采集 LOG 的时间（秒），**默认不退出** |
| `-h` | `--help` | 帮助 |

### 参考例子（原文完整）

```bash
# 1. 使用默认配置采集 10 秒 LOG 然后退出
qllog -s modem -t 10
qllog -s app -t 10
qllog -s kernel -t 10

# 2. 采集 10 秒应用 logcat 日志并且保存到 /oemdata 目录
qllog -s app -p /oemdata -t 10

# 3. 后台采集应用日志并且循环保存到 5 个文件，每个文件最大 20MB，保存到 emmc 分区
qllog -s app -p /mnt/mmcblk0p1/log/app -z 20 -n 5 &

# 4. 同时收集 modem, app, kernel 的 log 10 秒
qllog -m 0x0E -t 10
```

> **注**：例 2 中的 `/oemdata` 只是该示例选用的保存路径参数，并非必须预先存在的特殊目录。

### `-m` 多源模式的 bitmask（原文）

- `-m` 模式下**只允许 `-t` 参数**，不允许其他配置；
- `-m` 后面跟的是每个源的 bitmask（位图），多个源通过"或"的方式传递进来：

```
BITMASK_MODEM   = 0x02
BITMASK_APP     = 0x04
BITMASK_KERNEL  = 0x08
BITMASK_DUMP    = 0x10
BITMASK_SYSINFO = 0x20
BITMASK_GNSS    = 0x40
```

例：modem/app/kernel 同时收集 = `0x02 | 0x04 | 0x08 = 0x0E`。

> **注意**：bitmask 值与 2.1 的 source 编号（0x01~0x06）**不是一套值**，勿混用。

---

## 4. 抓取内核 Dump（PDF 第 4 章，第 6 页）

抓 dump 比较特殊，因为内核 dump 之后系统**会重启**。qllog 的做法：出现 dump 之后重启，qllog 判断当前为 dump 触发状态，把 dump 信息采集并保存到**默认配置路径**，做完之后再重启、重新进入正常工作状态。

### 4.1 抓 AP dump

```bash
qllog -s dump -t 1        # 配置之后立刻生效，然后只需等待内核 dump 即可
```

测试时可手动触发内核 dump：

```bash
echo p > /dev/ramdump_ctl
```

流程：dump 发生 → 系统重启进入 dump 模式 → qllog 自动收集 DUMP 到指定目录 → 重启回正常工作模式 → 对应目录下可见 Dump log。

> **注意（原文）**：Dump 保存路径**只能在配置文件里设定**（第 5 章），命令行 `-p` 对 dump 无效。

### 4.2 抓 CP dump

CP dump 需要先用 AT 命令设置，因为 **modem 默认 dump 之后是静默重启的**，不会让系统 dump。注意该命令**重启才生效**：

```
at+aconfig="RDUP=1"
at+aconfig="EEHP=2"
```

重启到系统正常后输入抓 log 命令：

```bash
qllog -s dump -t 1
```

此时才可以触发 dump。模拟 CP dump 的测试命令：

```bash
echo a > /dev/acipc
```

触发后系统重启进入 dump 模式，`ql_logd` 收集 log 保存到指定路径，收集完毕后重启回正常模式。

**用完必须手动恢复**（RDUP/EEHP 标志位会保存，不会自动恢复）：

```
at+aconfig="RDUP=0"
at+aconfig="EEHP=1"
```

> **注意（原文）**：抓系统 Dump 建议用于**临时分析问题**使用，**不建议做进系统版本**。

---

## 5. 配置文件（PDF 第 5 章，第 7~8 页）

`ql_logd` 服务启动时读取 **`/etc/ql_logd.conf`** 作为默认配置。用户可在 SDK 里修改该默认配置文件的参数；通常建议产品化之后**通过默认配置文件固化下来**。默认配置全文：

```ini
# Modem default log config
MODEM_LOG_PATH=/mnt/mmcblk0p1/log/modem
MODEM_LOG_LEVEL=4
MODEM_LOG_SIZE=100
MODEM_LOG_COUNT=4

# App default log config
APP_LOG_PATH=/mnt/mmcblk0p1/log/app
APP_LOG_LEVEL=4
APP_LOG_SIZE=100
APP_LOG_COUNT=4

# Kernel default log config
KERNEL_LOG_PATH=/mnt/mmcblk0p1/log/kernel
KERNEL_LOG_SIZE=10
KERNEL_LOG_COUNT=2

# Dump default log config
DUMP_LOG_PATH=/mnt/mmcblk0p1/log/dump
DUMP_LOG_COUNT=4
APP_DUMP_ENABLE=1
#log_temp_file_dir should not be the same as log_file_dir
APP_CORE_LOG_TEMP_FILE_DIR=/tmp
APP_CORE_FORMAT=coredump-%e-%p-%s-%t

# Sysinfo default log config
SYSINFO_LOG_PATH=/mnt/mmcblk0p1/log/sysinfo
SYSINFO_LOG_COUNT=10
SYSINFO_LOG_INTERVAL=600

# Gnss default log config
GNSS_LOG_PATH=/mnt/mmcblk0p1/log/gnss
GNSS_LOG_SIZE=100
GNSS_LOG_COUNT=4
```

配置说明（原文完整）：

1. PATH/LEVEL/SIZE/COUNT 的定义参考 2.5 章，对应每个 LOG 源单独的配置；
2. `SYSINFO_LOG_INTERVAL=600`：每 600 秒收集一次 sysinfo；sysinfo **没有等级概念**，单个文件大小固定；
3. `APP_DUMP_ENABLE`：指移远 qlsdk 服务发生 dump 之后收集对应的应用 dump log 用于分析问题。`APP_CORE_LOG_TEMP_FILE_DIR=/tmp` 表示 dump 发生后先临时保存在 `/tmp`，qllogd 检测到产生了 dump 文件后收集并**压缩打包**放到 `DUMP_LOG_PATH`，并从 `/tmp` 删除；`APP_CORE_FORMAT` 指定 dump 日志生成的格式（`%e`=可执行名, `%p`=PID, `%s`=信号, `%t`=时间戳，标准 core_pattern 格式）；
4. **modem 的 log level 只能设置 1 或者 4**，对应 Error 和 Debug 等级；
5. **内核 dmesg 日志不能设置等级**；
6. 所有 log 输出格式以 **logcat 格式**输出。

---

## 6. 建议使用场景（PDF 第 6 章，第 8 页，原文完整）

| 源 | 用途 | 建议 |
|---|---|---|
| **App** | 分析应用问题、QLSDK 问题、网络问题、ECALL 问题 | **必须的，建议在后台一直抓** |
| **Kernel** | 分析内核问题、驱动问题 | **必须的，建议在后台一直抓** |
| **Modem** | 分析 modem 问题（一般是驻网、网络问题等），在 APP 日志已经分析看不出问题的情况下才需要 | **数据量非常大，建议不能一直抓**；需要预留接口，在需要复现问题的时候再触发来抓 |
| **Sysinfo** | 反映系统的基本信息（间隔采集），系统配置，一般用于辅助看拨号问题、确认系统版本信息，常用 AT 命令查看 modem 状态等 | 文件不大，**可以一直抓** |
| **Dump** | 抓内核 dump，正常系统不产生 | 需要抓 dump 复现问题的时候**手动触发抓取** |
| **Gnss** | 专门抓 NMEA 报文，分析 GNSS 定位、偏移等芯片等问题时使用 | 注意这里只包含 NMEA；GNSS 服务本身的日志包含在 APP 类日志里面 |

**原文总结**：

> app 和 kernel 和 sysinfo 是必须提供的，可以覆盖大部分的问题分析场景。遇到比较困难的 modem 类问题（例如和基站交互、断网等），需要提供 modem 日志；遇到系统 dump 的问题分析才需要抓 dump；分析 gnss 定位相关问题，才需要提供 gnss nmea 的 log。

---

## 7. 日志数量级评估（PDF 第 7 章，第 8 页，原文完整）

> 前提声明（原文）：以下数据是指 **qlsdk 原始版本、没有跑复杂网络服务**的参考数据。用户实际跑复杂业务的场景下，日志的量级差异较大，**建议客户实际测试为准**。

| 源 | 量级 |
|---|---|
| APP | 100MB / 7 小时 |
| kernel | 1MB / 24 小时 |
| modem | 15MB / 10 分钟（Error 精简等级）；**85MB / 10 分钟**（debug 全量 Log 等级） |
| sysinfo | 320KB / 1 个文件，每 10 分钟 1 个 |
| GNSS NMEA | 100MB / 24 小时 |

---
---

## 8. 与本设备（EC200A / ASR1803）的适配性核验

> ⚠️ **本章不是 PDF 内容**，是针对本项目目标设备的实测核验记录（2026-07-23）。每一条均有出处，无推测成分。

### 8.1 结论：该文档不适用于本设备

| # | 事实 | 证据来源 |
|---|---|---|
| 1 | qllogd 服务**仅在 ASR1806 平台提供**，且需 2024-12 之后的 SDK | PDF 第 1 章原文（第 3 页） |
| 2 | 本设备为 **EC200A，芯片 ASR1803**，固件 1.057.030 | 项目既有事实（CLAUDE.md / ATI 可复核） |
| 3 | 设备内无 `qllog` 二进制、无 `/etc/ql_logd.conf`、无 `ql_logd` 进程；`find /usr /bin /sbin /opt /etc` 搜索为空 | 设备实测（2026-07-23，`which`/`ls`/`pgrep`/`find` 四路验证全部为空） |
| 4 | 本地全部三份 EC200A SDK（r02a02 / r02a04 / r03a02）的 `ql-sysroots/usr/include/ql-sdk/` 均**无 `ql_log.h`**；SDK 目录树内无任何 qllog/ql_logd 文件 | 开发机实测（`~/lyp/SDK/EC200A/`） |
| 5 | `ql_log.h` 在本地仅存在于 **AG35 SDK**（另一平台），与 EC200A 无关 | 开发机全盘 `find` 实测 |

综合 1~5：设备上没有 qllog 是**平台代差**（qllog 为 2024 年底 ASR1806 新增功能，未下放 ASR1803），不是文件缺失或安装问题。**不存在"补装 qllog"的可行路径**——前端命令即使拷入，其依赖的 ql_logd 服务在固件中不存在。

### 8.2 `/oemdata` 说明

- PDF 中 `-p /oemdata` 只是**示例保存路径参数**，非前提条件（见第 3 章例 2 注）；
- 本设备挂载表中对应的 OEM 分区是 `/NVM/oem_data`（ubiblock1_0，2.9M，100% 满，只读块设备），不可写；
- 本设备适合存日志的是 `/media/sdcard`（57G），即 open_dial 现有日志目录所在。

### 8.3 各 qllog 源在本设备上的等价手段

| qllog 源 | PDF 给出的本质 | 本设备等价物 | 状态 |
|---|---|---|---|
| `-s app` | "相当于 Logcat 的应用日志筛选了 system 和 main 这两个 buffer"（2.1 原文） | `logcat` | ✅ 实测可用（`diag/diag.c:54` 已在用 `logcat -d -v time`） |
| `-s kernel` | 抓内核 dmesg | `dmesg` | ✅ 实测可用（diag 模块已在用） |
| `-s modem` | "和 CatStudio 抓取保存的日志相同"（2.1 原文） | USB + CatStudio | 需 USB 口 + Quectel CatStudio；`default_media` 配置项（2.5 特别注意）在本固件是否存在**未验证** |
| `-s dump` | 内核/CP dump 收集 | 4.2 节 `at+aconfig="RDUP/EEHP"` 在 ASR1803 固件 1.057.030 上是否有效**未验证**，勿直接照搬 | ❓ 未知，需问 FAE |
| `-s sysinfo` | 定时系统信息 | 无对应服务（可用脚本定时采集替代） | ❌ |
| `-s gnss` | NMEA 报文 | 无对应服务 | ❌ |

**app 日志实际抓取命令**（替代 `qllog -s app -p /oemdata`）：

```bash
# 一次性导出当前缓冲区
logcat -d -v time > /media/sdcard/applog_$(date +%Y%m%d_%H%M%S).log

# 持续后台抓（等问题复现时用；注意 APP 日志量级参考约 100MB/7h，自行轮转清理）
logcat -v time > /media/sdcard/applog_live.log 2>&1 &
```

注：设备 logcat 支持哪些 `-b`/`-f`/`-r` 选项未逐一实测，使用前以设备上 `logcat -h` 输出为准；不带 `-b` 的默认用法已被 diag 模块验证可出内容。

### 8.4 对移远的回复要点

1. 本方模组为 EC200A（ASR1803），固件 1.057.030；设备内已确认无 qllog/ql_logd，三版 EC200A SDK 均无 `ql_log.h`；
2. 贵方文档 QT-AN-01-007 第 1 章写明 qllogd 仅 ASR1806 平台提供，与本机型不符；
3. 请确认：① EC200A/ASR1803 是否有计划提供 qllog、需何固件版本；② 若无，EC200A 上抓 app 日志的官方替代方法是否即为直接 logcat（如是，本方直接提供 logcat 导出文件）。
