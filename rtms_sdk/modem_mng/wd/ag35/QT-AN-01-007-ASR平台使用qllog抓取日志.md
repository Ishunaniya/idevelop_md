# QT-AN-01-007《AN007：ASR 平台使用 qllog 抓取日志》分析

> 本分析基于对源 PDF 全部 8 页内容的逐页实读，未做任何臆测性补充。文档实际内部编号显示为 `QT-AN-01-001`（封面/书脊编号为 `QT-AN-01-007`，但每页页眉编号均印为 `QT-AN-01-001`——这是文档自身存在的编号不一致，详见后文"局限性"部分）。

## 1. 文档基本信息

| 项目 | 内容 |
|------|------|
| 标题 | AN007：ASR 平台使用 qllog 抓取日志 |
| 页眉文档编号 | QT-AN-01-001（注：与封面/文件名中的 QT-AN-01-007 不一致，见下文说明） |
| 版本号 | V1.0 |
| 初版制定时间 | 2024-11-18 |
| 最新更新时间 | 2024-11-18（页眉标注），文件变更记录表中记载的修订日期为 2025-02-20，版本仍为 V1.0 |
| 编制人 | William liu |
| 文件保密级别 | 公开（封面勾选框为"公开"） |
| 出品单位 | 移远通信（QUECTEL），上海市闵行区田林路1016号科技绿洲3期（B区）5号楼 200233 |
| 适用平台/产品 | **仅适用于 ASR1806 平台**（文档第1节明确指出"这个服务当前只有在 ASR1806 平台提供"） |
| 前提条件（SDK版本） | 需要将 SDK 更新到 **2024年12月以后**的版本，否则不支持该功能 |
| 适用范围/目的 | 面向产品化之后无预留 USB 口、需要远程抓取并分析日志的客户/前线 FAE 场景，介绍移远开发的 `qllogd` 日志抓取服务及其两种使用方式（SDK 接口 / qllog 命令行工具） |

## 2. 目录结构概览

文档正文共 7 个章节（无独立目录页，根据章节标题归纳）：

1. 目的
2. 基于 SDK 接口的方式
   - 2.1 抓 LOG 的源
   - 2.2 LOG 等级
   - 2.3 初始化和去初始化
   - 2.4 启动和停止抓取 log 服务
   - 2.5 配置和获取配置
3. 基于 qllog 命令的方式
4. 抓取内核 Dump
   - 4.1 抓 AP dump
   - 4.2 抓 CP dump
5. 配置文件
6. 建议使用场景
7. 日志数量级评估

封面页之后另有一页"文件管控表"（文件变更记录），属于公司文档模板的固定结构，非正文内容。

## 3. 逐章节详细摘要

### 第1节：目的（page 3）

- **背景动机**：产品化后的设备通常不预留 USB 口，给售后人员现场抓取 LOG 带来困难；同时越来越多车厂要求集成"抓 LOG"相关服务以支持远程抓 LOG 分析。
- **解决方案**：移远基于上述需求开发了 `qllogd` 服务，提供给客户或前线 FAE 使用，目的是简化抓取 LOG 的操作。
- **平台限制（重要前提）**：该服务**目前只在 ASR1806 平台**提供，且需要将 SDK 版本更新到 **2024年12月以后**才可用。

### 第2节：基于 SDK 接口的方式（page 3-4）

提供给希望将抓 LOG 功能集成到自身应用中的客户使用，调用方式与移远其他 SDK 接口一致。

- 头文件路径：`ql-sysroots/usr/include/ql-sdk/ql_log.h`
- 示例代码路径：`sample/test_sdk_api/m_log.c`

**2.1 抓 LOG 的源**（宏定义及含义）：

| 宏 | 值 | 含义 |
|---|---|---|
| `QL_LOG_SOURCE_MODEM` | 0x01 | 抓 CP（modem）LOG，等同于 CatStudio 抓取保存的日志，用于分析网络问题 |
| `QL_LOG_SOURCE_APP` | 0x02 | 抓应用 LOG，相当于 Logcat 中筛选了 system 和 main 两个 buffer 的应用日志 |
| `QL_LOG_SOURCE_KERNEL` | 0x03 | 抓内核 dmesg 日志 |
| `QL_LOG_SOURCE_DUMP` | 0x04 | 收集系统 dump（需特殊操作，见第4节） |
| `QL_LOG_SOURCE_SYSINFO` | 0x05 | 定时收集系统信息 |
| `QL_LOG_SOURCE_GNSS` | 0x06 | 单独收集 GNSS 的 NMEA 报文（**注意**：该命令本身不会主动打开 GNSS，需先用 `ql_gnss` 相关 API 打开 GNSS） |

**2.2 LOG 等级**（宏定义）：

| 宏 | 值 | 输出内容 |
|---|---|---|
| `QL_LOG_LEVEL_ERROR` | 1 | 只输出 ERROR 级别日志（输出最少） |
| `QL_LOG_LEVEL_WARNING` | 2 | 输出 WARNING + ERROR |
| `QL_LOG_LEVEL_INFO` | 3 | 输出 INFO + WARNING + ERROR |
| `QL_LOG_LEVEL_DEBUG` | 4 | 输出 DEBUG + INFO + WARNING + ERROR（输出最多） |

**2.3 初始化和去初始化**：
```c
int ql_log_init(void);
int ql_log_deinit(void);
```
与其他 SDK 服务一致：使用前需先 `ql_log_init()` 连接 `ql_logd` 服务，使用完毕需调用 `ql_log_deinit()` 断开连接。

**2.4 启动和停止抓取 log 服务**：
```c
int ql_log_start(int source, int flag);
int ql_log_stop(int source);
```
- `source`：参考 2.1 中定义的源宏。
- `flag`：预留参数，当前未使用，默认传 0。
- **注意1**：`ql_log_start` 是**异步**操作——调用后立即返回，实际由 `ql_logd` 服务在后台开线程收集，直到用户调用 `ql_log_stop` 关闭为止。
- **注意2**：可以同时抓取多个不同的源，只需多次调用 `ql_log_start`。

**2.5 配置和获取配置**：
```c
int ql_log_get_config(int source, ql_log_config_t *p_config);
int ql_log_set_config(int source, ql_log_config_t *p_config);

typedef struct {
    char     log_dir[QL_LOG_DIR_NAME_MAX];
    uint32_t log_level;
    uint32_t log_file_size;
    uint32_t log_file_num;
} ql_log_config_t;
```

字段含义：
- `log_dir`：保存 log 的路径
- `log_level`：抓取 log 的等级
- `log_file_size`：单个 log 文件最大大小，单位 MB
- `log_file_num`：最多保存的日志文件数量

**注意事项（文档原文）**：
1. 对于 modem 日志，由于 ASR 底层限制，**目前只能支持 ERROR 和 DEBUG 两个等级**（不支持 WARNING/INFO）。
2. 对于 sysinfo 日志，只有 `log_file_num` 和 `log_dir` 有效，其余参数无意义。
3. 该配置**只保存在内存里**，重启后恢复默认值；如需固化默认配置需通过配置文件修改（参考第5章）。

**特别注意（原文高亮提示）**：
`/system/etc/mrvl_tel_diag.cfg` 文件中存在配置项：
```
#Default Media: 0-off, 1-usb (default), 2-sd, 3-tcp
default_media=3
```
该配置决定 modem log 走 TCP 模式还是 USB 模式。**若发现 USB 连接不上 CatStudio，应先检查此配置**，将 `default_media=1` 并保存、重启生效，用于临时连接 USB 抓取 log 的场景。

### 第3节：基于 qllog 命令的方式（page 5-6）

面向不愿/无需自行开发应用程序的用户，提供命令行工具 `qllog`。

文档贴有命令行帮助截图（终端黑底白字），内容摘录如下：
```
qllog software v1.0 (2024-08-02) - a tool to capture and store logs for ASR platform

Usage:
  qllog -s source [-p path] [-l level] [-z file-size] [-n file-num] [-t timeout]
```

**参数解析表**：

| 参数 | 长选项 | 说明 |
|---|---|---|
| `-s` | `--log-source` | 设置源 source（参考 2.1） |
| `-p` | `--log-path` | 设置对应源的 LOG 保存路径 |
| `-l` | `--log-level` | 设置 LOG 等级（参考 2.2） |
| `-z` | `--log-file-size` | 设置单个 LOG 文件最大大小 |
| `-n` | `--log-file-num` | 设置最多保存 LOG 文件数量 |
| `-c` | `--get-config` | 获取对应源的配置信息 |
| `-m` | `--multi-source` | 设置多个源同时收集 LOG |
| `-t` | `--timeout` | 设置采集 LOG 的时间，默认不退出 |
| `-h` | `--help` | 帮助 |

**参考例子（原文）**：

1. 使用默认配置采集 10 秒 LOG 然后退出：
   ```
   qllog -s modem -t 10
   qllog -s app -t 10
   qllog -s kernel -t 10
   ......
   ```
2. 采集 10 秒应用 logcat 日志并保存到 `/oemdata` 目录：
   ```
   qllog -s app -p /oemdata -t 10
   ```
3. 后台采集应用日志并循环保存到 5 个文件，每个文件最大 20MB，保存到 emmc 分区：
   ```
   qllog -s app -p /mnt/mmcblk0p1/log/app -z 20 -n 5 &
   ```
4. 同时收集 modem、app、kernel 的 log 10 秒：
   ```
   qllog -m 0x0E -t 10
   ```

**`-m` 多源模式说明**：
- `-m` 模式下**只允许同时使用 `-t` 参数**，不允许其他配置项。
- `-m` 后跟的是每个源的 bitmask（位图），多个源通过"或"运算组合传入（文档原文写作"与"的方式传递，但从示例 `0x02|0x04|0x08=0x0E` 计算逻辑看，实际是位或 OR 运算，疑似文档表述笔误，详见局限性章节）。

**Bitmask 定义表**：

| Bitmask 宏 | 值 |
|---|---|
| `BITMASK_MODEM` | 0x02 |
| `BITMASK_APP` | 0x04 |
| `BITMASK_KERNEL` | 0x08 |
| `BITMASK_DUMP` | 0x10 |
| `BITMASK_SYSINFO` | 0x20 |
| `BITMASK_GNSS` | 0x40 |

示例：同时收集 modem/app/kernel 时，`BITMASK_MODEM | BITMASK_APP | BITMASK_KERNEL = 0x02 | 0x04 | 0x08 = 0x0E`。

> 注：终端帮助截图中的 bitmask 取值与正文略有差异：截图显示 "modem:0x02, app:0x04, kernel:0x08, dump:0x10, sysinfo:0x20, gnss:0x40"，与正文表格一致，可互相印证。

### 第4节：抓取内核 Dump（page 6）

抓 dump 是特殊场景，因为内核 dump 之后系统会重启，需要特殊处理流程。`qllog` 的处理逻辑：检测到出现 dump 后系统重启 → `qllog` 判断当前为 dump 触发状态 → 采集 dump 信息并保存到默认配置路径 → 完成后重启 → 重新进入正常工作状态。

**4.1 抓 AP dump（应用处理器侧 dump）**：

操作步骤：
1. 输入命令（配置后立即生效）：
   ```
   qllog -s dump -t 1
   ```
2. 等待内核 dump 发生即可；测试时可手动触发：
   ```
   echo p >/dev/ramdump_ctl
   ```
3. dump 发生后系统重启进入 dump 模式，`qllog` 自动收集 DUMP 到指定目录，完成后重启回正常工作模式；可在对应目录查看已保存的 Dump log。

**注意**：Dump 保存路径只能在配置文件里设定（参考第5章），无法通过命令行参数指定。

**4.2 抓 CP dump（基带/modem 侧 dump）**：

操作步骤（顺序不可跳步）：
1. 首先输入 AT 命令设置系统 dump 模式（**该命令重启才生效**）：
   ```
   at+aconfig="RDUP=1"
   at+aconfig="EEHP=2"
   ```
   原因：modem 默认 dump 后是静默重启的，不会让系统进入 dump 状态，需要先设置标志位。
2. 重启设备，待系统正常启动后，输入抓 log 命令：
   ```
   qllog -s dump -t 1
   ```
3. 此时才可以触发 dump。测试时可手动输入命令模拟 CP dump：
   ```
   echo a > /dev/acipc
   ```
4. 触发后系统重启进入 dump 模式，`ql_logd` 收集 log 并保存到指定路径，收集完毕后重启进入正常模式。
5. **务必手动恢复**：因为 `RDUP` 和 `EEHP` 标志位会被持久保存、不会自动恢复，使用完毕需手动设置回正常模式：
   ```
   at+aconfig="RDUP=0"
   at+aconfig="EEHP=1"
   ```

**注意（原文）**：抓系统 Dump 建议仅用于临时分析问题，**不建议作为常态固化进系统量产版本**。

### 第5节：配置文件（page 7-8）

`ql_logd` 服务启动时会读取 `/etc/ql_logd.conf` 文件作为默认配置。用户可在 SDK 里修改该默认配置文件参数，建议产品化之后通过该配置文件固化默认配置。

**默认配置内容（原文完整摘录）**：

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

**配置项说明（原文）**：
1. `PATH`/`LEVEL`/`SIZE`/`COUNT` 的定义参考 2.5 章描述，对应每个 LOG 源单独的配置。
2. `SYSINFO_LOG_INTERVAL=600` 表示每 600 秒收集一次 sysinfo；sysinfo 没有等级概念，单个文件大小固定。
3. `APP_DUMP_ENABLE` 指移远 qlsdk 服务发生 dump 后，可收集对应的应用 dump log 用于分析问题；其中：
   - `APP_CORE_LOG_TEMP_FILE_DIR=/tmp`：dump 发生后临时保存在 `/tmp` 目录；`ql_logd` 检测到产生 dump 文件后会收集并压缩打包放到 `DUMP_LOG_PATH` 指定路径，并从 `/tmp` 删除原文件。
   - `APP_CORE_FORMAT`：指定 dump 日志生成的文件名格式（如 `coredump-%e-%p-%s-%t`）。
   - 原文注释强调："log_temp_file_dir should not be the same as log_file_dir"（临时目录不能与最终日志目录相同）。
4. modem 的 `log level` 只能设置为 1 或 4（对应 Error 和 Debug 等级），与 2.5 节注意事项一致。
5. 内核 dmesg 日志不能设置等级。
6. 所有 log 输出格式均以 **logcat 格式**输出。

### 第6节：建议使用场景（page 8）

文档对各日志源给出了使用建议：

1. **App 日志**：用于分析应用问题、QLSDK 问题、网络问题、ECALL 问题，是**必须**的，建议在后台一直抓。
2. **Kernel 日志**：用于分析内核问题、驱动问题，是**必须**的，建议在后台一直抓。
3. **Modem 日志**：用于分析 modem 问题，一般是驻网、网络问题等，在 APP 日志已分析不出问题的情况下才需要。**注意：数据量非常大**，建议不能一直抓，需预留接口，在需要复现问题时再触发抓取。
4. **sysinfo 日志**：抓系统基本信息，间隔采集，反映系统配置，一般用于辅助查看拨号问题、确认系统版本信息、常用 AT 命令查看 modem 状态等；文件不大，可以一直抓。
5. **dump**：用来抓内核 dump，正常系统不能开，需要复现问题时手动触发抓取。
6. **gnss 日志**：专门用来抓 NMEA 报文，用于分析 GNSS 定位、偏移等芯片问题；注意只包含 NMEA 报文，GNSS 服务本身的日志包含在 APP 类日志里面。

**原文总结**：app、kernel、sysinfo 是必须提供的，可覆盖大部分问题分析场景；遇到较困难的 modem 类问题（如和基站交互、断网等）才需要提供 modem 日志；遇到系统 dump 问题分析才需要抓 dump；分析 gnss 定位相关问题才需要提供 gnss nmea 的 log。

### 第7节：日志数量级评估（page 8）

文档声明：以下数据基于 qlsdk 原始版本、**未跑复杂网络服务**场景下的参考数据；用户实际跑复杂业务场景下日志量级差异较大，建议客户实际测试为准。

| 日志源 | 数据量级 |
|---|---|
| APP | 100MB / 7小时 |
| kernel | 1MB / 24小时 |
| modem | 15MB / 10分钟（Error 精简等级）；85MB / 10分钟（Debug 全量 Log 等级） |
| sysinfo | 320KB/1个文件，每10分钟产生1个 |
| GNSS NMEA | 100MB / 24小时 |

## 4. 关键命令/接口/参数汇总表

### 4.1 SDK 接口函数汇总

| 函数签名 | 用途 |
|---|---|
| `int ql_log_init(void)` | 初始化并连接 ql_logd 服务 |
| `int ql_log_deinit(void)` | 断开 ql_logd 服务连接 |
| `int ql_log_start(int source, int flag)` | 异步启动指定源的日志采集（flag 预留，传0） |
| `int ql_log_stop(int source)` | 停止指定源的日志采集 |
| `int ql_log_get_config(int source, ql_log_config_t *p_config)` | 获取指定源的配置 |
| `int ql_log_set_config(int source, ql_log_config_t *p_config)` | 设置指定源的配置（仅内存生效，重启失效） |

### 4.2 qllog 命令行参数汇总

| 参数 | 全称 | 作用 |
|---|---|---|
| `-s` | `--log-source` | 指定日志源 |
| `-p` | `--log-path` | 指定保存路径 |
| `-l` | `--log-level` | 指定日志等级 |
| `-z` | `--log-file-size` | 单文件最大大小(MB) |
| `-n` | `--log-file-num` | 最多保留文件数 |
| `-c` | `--get-config` | 查询配置 |
| `-m` | `--multi-source` | 多源 bitmask 同时采集（仅可配合 `-t`） |
| `-t` | `--timeout` | 采集时长(秒)，默认不超时 |
| `-h` | `--help` | 帮助信息 |

### 4.3 AT 命令（仅用于 CP dump 流程）

| 命令 | 作用 |
|---|---|
| `at+aconfig="RDUP=1"` | 启用 RDUP，使 modem dump 不再静默重启（需重启生效） |
| `at+aconfig="EEHP=2"` | 设置 EEHP 标志为 2（配合 RDUP 使能进入 dump 模式，需重启生效） |
| `at+aconfig="RDUP=0"` | 恢复正常模式（手动复位，不会自动恢复） |
| `at+aconfig="EEHP=1"` | 恢复正常模式（手动复位，不会自动恢复） |

### 4.4 关键文件/路径汇总

| 路径 | 用途 |
|---|---|
| `ql-sysroots/usr/include/ql-sdk/ql_log.h` | SDK 头文件 |
| `sample/test_sdk_api/m_log.c` | SDK 示例代码 |
| `/etc/ql_logd.conf` | ql_logd 服务默认配置文件 |
| `/system/etc/mrvl_tel_diag.cfg` | modem log 传输模式配置（`default_media`：0关/1usb/2sd/3tcp） |
| `/dev/ramdump_ctl` | 手动触发 AP dump 测试入口（`echo p >`） |
| `/dev/acipc` | 手动触发 CP dump 测试入口（`echo a >`） |
| `/mnt/mmcblk0p1/log/{modem,app,kernel,dump,sysinfo,gnss}` | 各日志源默认保存路径 |
| `/tmp`（`APP_CORE_LOG_TEMP_FILE_DIR`） | dump 临时文件目录（不可与最终日志目录相同） |

## 5. 完整操作流程还原

### 5.1 SDK 集成方式使用流程

1. 包含头文件 `ql_log.h`，参考示例 `m_log.c`。
2. 调用 `ql_log_init()` 连接 `ql_logd` 服务。
3. （可选）调用 `ql_log_set_config()` 修改某个 source 的 `log_dir`/`log_level`/`log_file_size`/`log_file_num`（注意仅内存生效）。
4. 调用 `ql_log_start(source, 0)` 启动指定源的采集（可多次调用以启动多个不同源，异步、立即返回）。
5. 需要时调用 `ql_log_stop(source)` 停止采集。
6. 使用完毕调用 `ql_log_deinit()` 断开服务连接。

### 5.2 命令行方式使用流程

- 单源采集：`qllog -s <source> [-p path] [-l level] [-z size] [-n num] -t <timeout>`
- 多源同时采集：`qllog -m <bitmask> -t <timeout>`（`-m` 模式下不可附加其他配置参数）
- 后台持续采集：在命令末尾加 `&`

### 5.3 AP dump 抓取完整流程

1. 执行 `qllog -s dump -t 1`（立即生效，进入等待状态）。
2. 等待真实内核 dump 发生，或手动执行 `echo p >/dev/ramdump_ctl` 触发测试。
3. 系统检测到 dump → 重启 → 进入 dump 模式。
4. `qllog` 自动采集 dump 信息，保存至配置文件中指定的 `DUMP_LOG_PATH` 路径。
5. 采集完成后系统自动重启，回到正常工作模式。
6. 到指定目录核实 Dump log 已生成。

### 5.4 CP dump 抓取完整流程（注意涉及两次重启）

1. 执行 `at+aconfig="RDUP=1"` 和 `at+aconfig="EEHP=2"`。
2. **重启设备**（该 AT 配置需要重启才能生效）。
3. 系统正常启动后，执行 `qllog -s dump -t 1`。
4. 触发真实 CP dump，或手动执行 `echo a > /dev/acipc` 模拟触发。
5. 系统重启，进入 dump 模式；`ql_logd` 收集日志并保存到指定路径。
6. 收集完毕，系统再次重启，回到正常模式。
7. **必须手动**执行 `at+aconfig="RDUP=0"` 和 `at+aconfig="EEHP=1"` 恢复正常配置（标志位持久化保存，不会自动复位）。

## 6. 与本项目（open_dial）的潜在关联点

本项目 `open_dial` 是运行在 **Quectel EC2x/EG2x（以及 AG35）模组**上的拨号管理程序（基于 OpenNPC、ARMv7 平台），通过 MCM/QMI/DSI API 及 AT 命令管理蜂窝数据连接和 Roamlink 双通道切换。结合 CLAUDE.md 中记录的项目背景，本文档与本项目的关联性分析如下：

1. **平台不匹配，关联非常有限**：文档第1节明确说明 `qllogd`/`qllog` 服务**当前只在 ASR1806 平台提供**。本项目运行的 EC2x/EG2x/AG35 系列模组使用的是 Quectel QuecOpen SDK（基于高通/其他基带平台），与 ASR1806 平台是不同的硬件/SDK 体系，文档中描述的 `ql_log.h`、`ql_logd`、`qllog` 命令、`/etc/ql_logd.conf`、`mrvl_tel_diag.cfg`（`mrvl` 前缀暗示 Marvell/ASR 芯片相关）等接口和路径**均不适用于本项目当前部署的模组**。
2. **思路层面可参考之处（仅作为理念借鉴，非可直接复用的代码/接口）**：
   - 本项目当前依赖 `seas_log` 模块做本地滚动日志（`src/seas_log/`，60MB 滚动、SD 卡检测），以及 `/tmp/dial_status` 状态文件输出，属于"本地日志+本地状态"模式，没有类似 `qllogd` 这种"远程/统一日志采集服务"的设计。文档中"日志源分类（modem/app/kernel/dump/sysinfo/gnss）+ 等级 + 大小/数量轮转 + 配置文件固化默认值"的设计模式，可作为未来若需要扩展开放式调试日志采集能力时的参考范式，但需要在 Quectel EC2x/EG2x/AG35 对应 SDK 文档中查找等价接口（如有）。
   - 文档第6节"建议使用场景"中关于"网络/驻网问题难以复现，应预留接口按需触发抓取 modem 日志，而非长期常态抓取"的运维思路，与本项目 CLAUDE.md 中对 `NW_TCP_TEST_HOST` 单点故障、网络反复探测等问题的关注点存在相似的工程考量（按需触发、避免长期高开销采集），可作为运维侧的经验参考，但不涉及具体代码复用。
3. **结论**：本文档**与本项目当前代码（`src/dial/`、`src/roamlink/`、`src/nw/`、`src/at/` 等）没有直接的接口、命令或文件路径层面的关联**。本项目模组并非 ASR1806 平台，无法直接使用文档中的 `ql_log` SDK 接口或 `qllog` 命令行工具。如果项目今后涉及切换到 ASR1806 平台模组的衍生型号或需要远程日志采集能力调研，本文档可作为该平台日志采集机制的设计参考。

## 7. 文档局限性、未说明清楚之处及自身提及的已知问题

1. **文档编号不一致**：封面/文件名标注为 `QT-AN-01-007`，但页眉每一页打印的编号均为 `QT-AN-01-001`，文档自身未对此进行说明，存在编号管理上的疏漏。
2. **"与"/"或"运算表述歧义**：第3节描述 `-m` 多源 bitmask 组合方式时原文写"多个源通过'与'的方式传递进来"，但紧接给出的示例 `BITMASK_MODEM | BITMASK_APP | BITMASK_KERNEL = 0x02 | 0x04 | 0x08 = 0x0E` 实际是按位**或**（OR）运算。文档表述（"与"）与示例公式（`|`，或运算）存在矛盾，未给出更正说明，可能是用词不严谨导致的歧义，需要读者以代码示例为准。
3. **`flag` 参数未展开说明**：`ql_log_start(int source, int flag)` 中的 `flag` 参数文档仅说明"预留，当前没有使用，默认传入 0 即可"，未说明未来可能的用途或取值范围。
4. **未说明的接口细节**：
   - 文档未说明 `ql_log_init()`/`ql_log_deinit()`/`ql_log_start()`/`ql_log_stop()` 等函数的返回值含义（仅声明返回 `int`，未列出错误码或成功值定义）。
   - 未说明 `QL_LOG_DIR_NAME_MAX` 的具体数值（仅在结构体定义中出现该宏，未给出其值）。
   - 未说明多个进程/客户端同时调用 SDK 接口时是否存在并发限制或互斥机制。
5. **dump 路径限制未充分解释原因**：文档仅说明"Dump 保存路径只能在配置文件里面设定"，未说明为什么命令行/SDK 接口不支持动态指定 dump 路径（其他源如 app/kernel/modem 均可通过 `-p`/`log_dir` 动态指定）。
6. **CP dump 流程中的风险提示不充分**：文档提醒"需要手动设置回正常模式，因为 RDUP 和 EEHP 标志位是会保存的，并不会自动恢复"，但未说明如果忘记恢复、长期处于 `RDUP=1`/`EEHP=2` 状态对设备稳定性、功耗或后续真实故障时行为的具体影响，仅笼统建议"建议用于临时分析问题使用，不建议做进去系统版本"。
7. **日志量级数据的适用范围声明保守但未细化**：第7节明确声明该数据"没有跑复杂网络服务"，"用户实际跑复杂业务的场景下，日志的量级差异较大，建议客户实际测试为准"，但未给出任何影响因素的具体说明（例如：哪些业务类型对 app/modem 日志量级影响最大），对于希望提前规划存储空间的读者参考价值有限。
8. **版本变更记录信息不完整**：文件管控表中"修订日期"列为 2025-02-20，而页眉标注的"最新更新时间"为 2024-11-18，两者不一致，文档未说明该差异原因（可能是模板生成日期与实际发行日期不同步）。
9. **无目录页/页码索引**：PDF 全文 8 页中没有独立的目录（TOC）页，所有章节标题需通过通读全文获取，不便于快速检索定位。
10. **截图中的命令行版本号信息**：第3节贴出的终端截图显示 `qllog software v1.0 (2024-08-02)`，该日期早于文档"需要 SDK 更新到 2024年12月以后版本"的要求，文档未解释 qllog 工具本身的版本演进历史或该截图版本是否为最终随 SDK 发布的版本，存在版本对应关系不明确的问题。

<!-- GENERATION_COMPLETE: 2026-06-24 -->
