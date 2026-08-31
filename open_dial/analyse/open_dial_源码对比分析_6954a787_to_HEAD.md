# open_dial 源码对比分析报告

**基准提交**：`6954a787b7b9259141a1431c5682edbf4713acc3`  
**目标**：HEAD（`259b37b84b3e5a6afba7760cc9ca8c8a9d8e1b21`）  
**分析时间**：2026-05-09  
**分析对象**：dial.c / data_call/data_call.c / misc.c / nw/nw.c / sim/sim.c / apn/apn.c 等全部变更文件

---

## 目录

- [一、全局对比概览](#一全局对比概览)
- [二、dial.c 详细分析](#二dialc-详细分析)
- [三、CPIN 查询逻辑专项审查（紧急）](#三cpin-查询逻辑专项审查紧急)
  - [3.7 SDK 自动重连能否替代应用层恢复](#37-sdk-自动重连能否替代应用层恢复断卡场景)
  - [3.8 轮询 CPIN 的根本性设计缺陷与改进方向](#38-轮询-cpin-的根本性设计缺陷与改进方向)
- [四、L1/L2/L3 分级恢复逻辑完整分析](#四l1l2l3-分级恢复逻辑完整分析)
- [五、其他模块变更分析](#五其他模块变更分析)
- [六、总结与建议](#六总结与建议)

---

## 一、全局对比概览

### 1.1 提交历史时间线

| Hash（前8位）| 日期 | 描述 |
|---|---|---|
| `df06b4d` | 2026-04-09 13:53 | [BUGFIX] 漏洞修复合集（APN、DNS、信号、抖动、L1/L3、路由等12个文件） |
| `ffea5dd` | 2026-04-09 14:47 | [BUGFIX] 修复断网重连后因内核路由残留导致必须重启系统才能联网的问题 |
| `72888c3` | 2026-04-20 13:53 | [BUGFIX] 修复路由清理中未按接口精确操作、误清空防火墙规则及缺失 IPv6 路由清理 |
| `6c02b01` | 2026-04-20 14:40 | [BUGFIX] 修复 Fast Retry 超时误判、call_id 过滤丢事件等拨号稳定性问题 |
| `6f7c1be` | 2026-04-20 16:41 | [BUGFIX] 修复 persistent 模式下成功重连后 retry_count 未清零的问题 |
| `8c331aa` | 2026-04-20 16:43 | [CHGLOG] 漏提交 dial 二进制文件 |
| `7e0648d` | 2026-04-21 14:08 | [BUGFIX] 修复 SIM UNKNOWN 状态下 L2/L3 恢复永不触发的问题 |
| `53c9853` | 2026-04-29 14:35 | [CHGLOG] 在 SIM 瞬断错误日志中附加 AT+CPIN? 实际状态 |
| `26342e6` | 2026-04-30 15:58 | [CHGLOG] 新增三层诊断日志、去除 L1 恢复 REG 前置校验 |
| `94d0fa3` | 2026-05-04 13:23 | [CHGLOG] SIM 断恢复后立即触发 L1 重拨，断联时长由约 70s 缩短至约 30s |
| `ab20799` | 2026-05-05 13:59 | [CHGLOG] SIM 瞬断时同步抓取 AT+QSIMSTAT/CEREG 快照及 dmesg 内核日志 |
| `d51f798` | 2026-05-05 14:35 | [CHGLOG] 新增启动时固件版本日志；QSIMSTAT 和 dmesg 诊断从断卡时刻移至 SIM 恢复时刻 |
| `8e6c343` | 2026-05-06 10:43 | [DEL] 删除无关文件 |
| `f93a4ee` | 2026-05-06 13:56 | [ADD] 新增 .gitignore，忽略 md 文档目录 |
| `bc6b14c` | 2026-05-06 17:11 | [BUGFIX] 修复固件版本日志：AT+CGMR→AT+QGMR，去除 SUB 前缀冗余文本 |
| `3ff9f9e` | 2026-05-07 15:49 | [CHGLOG] REG=0 时跳过 L1、提前触发 L2（90s），缩短注册丢失场景恢复时长 |
| `259b37b` | 2026-05-08 09:37 | [CHGLOG] 心跳日志新增 TEMP 字段，每 30s 记录设备温度 |

共 **17 个提交**，跨度约 29 天（2026-04-09 至 2026-05-08）。

### 1.2 变更文件列表

| 文件 | 新增行 | 删除行 | 说明 |
|---|---|---|---|
| `dial.c` | +908 | -160 | **核心变更**，新增大量诊断函数和逻辑重构 |
| `apn/apn.c` | +64 | -30 | 内存安全、精确匹配、正确释放 |
| `data_call/data_call.c` | +80 | -27 | 路由/NAT 清理、call_name 过滤、DNS 修复 |
| `data_call/data_call.h` | +7 | 0 | 新增 DIAL_CALL_NAME 宏和 g_pdp_connected 外部声明 |
| `dial.h` | +2 | -4 | 移除冗余头文件包含 |
| `misc.c` | +52 | -13 | 僵尸进程修复、realloc 安全性、pgrep -x |
| `nw/nw.c` | +10 | -1 | 文件大小边界检查、O_TRUNC 修复 |
| `nw/nw.h` | +1 | 0 | 新增 stdbool.h 包含 |
| `logger_sd.c` | +1 | 0 | 微调 |
| `reboot_conf/dial_reboot_conf.c` | +1 | -1 | 微调 |
| `Makefile` | +3 | -3 | 编译参数调整 |
| `.gitignore` | +2 | 0 | 新增忽略规则 |
| `_public.h` | 0 | -1 | 移除冗余包含 |

**总计**：978 行新增，160 行删除，净增 818 行。

### 1.3 总体改动规模评估

本次迭代属于**中大型重构+功能增强**，核心改动集中在两个方向：

1. **稳定性修复**（前 5 个 commit）：消除路由残留、Fast Retry 误判、call_id 丢事件、内存泄漏等 bug。
2. **可观测性增强**（后 12 个 commit）：针对 eSIM 瞬断问题增加诊断日志（DIAG-SIM、DIAG-DMESG）、RSRP/RSRQ 扩展心跳、固件版本初始化日志、设备温度监测。

---

### 1.4 重拨机制历史演变溯源（`e92733c` 之前的断裂期）

> **背景问题**：`e92733c`（minglei.li，2025-12-11）加入了"拨号 3 次快速恢复机制"，在此之前断网后为什么无法重新联网？是清除路由表的问题吗？

#### 结论先行

不是路由表问题，路由残留确实存在但那是"联网后马上又断"，不是"完全无法重拨"的根因。**真正的根因是 `7910fee`（2024-10-17）将拨号机制从 `ql_data_call` SDK 切换成了 `ql_netcall`，并在恢复路径中 `killall -9 ql_rild`，彻底打断了 SDK 层，导致后续所有重拨调用均无效。**

---

#### 阶段一：`b3d8422` 及更早（2023-03）— 有完整重拨机制

早期版本使用 `ql_data_call` SDK，同时配置了 SDK 自动重连（20s 间隔）和应用层 CFUN 恢复链：

```c
// SDK 层：PDP 断开后 20s 自动重连
ql_data_call_param_set_reconnect_mode(p_param, QL_NET_DATA_CALL_RECONNECT_NORMAL);
time_interval_list[0] = 20;
ql_data_call_param_set_reconnect_interval(p_param, time_interval_list, 1);

// 应用层：Ping 超时 → CFUN 射频重置 → 重新发起拨号
dial_stat_stop          → ql_data_call_stop()
  → dial_stat_stop_cfun  → Ql_SendAT("AT+CFUN=0")   // 真发，有效
  → dial_stat_start_cfun → Ql_SendAT("AT+CFUN=1")
  → dial_stat_none       → ql_data_call_init/create/start  // 重新拨号
```

两层保障（SDK 自动重连 + 应用层 CFUN 链）均有效，断网后可以重拨。

---

#### 阶段二：`7bf8205`（2023-07-06）— 部分降级，SDK 重连仍在

提交信息：`把AT+CFUN注释掉，防止频繁开关射频`

```c
// 应用层 CFUN 注释掉了：
// Ql_SendAT("AT+CFUN=0");
// Ql_SendAT("AT+CFUN=1");
```

但 `ql_data_call_param_set_reconnect_mode(RECONNECT_NORMAL)` 配置仍在，SDK 底层每 20s 还会自动重拨 PDP。应用层少了 CFUN 这层兜底，但整体仍可用。

---

#### 阶段三：`7910fee`（2024-10-17）到 `07a2ab3`（2025-12-11）— **重拨机制彻底断裂，约 14 个月**

提交信息：`循环内采用移远的ql_netcall来拨号`

这一版本引入了三处致命问题，共同导致"断网后无法重拨"：

**① 状态机入口处 kill 掉 ql_netcall（每次重进入都无连接）**

```c
// 状态机开始前先清掉所有 ql_netcall 进程
system("killall -9 ql_netcall");
```

**② Ping 失败时 `killall -9 ql_rild`，打断 SDK 层**

```c
// check_exit_count 分支：
system("killall -9 ql_rild");   // 直接杀掉 SDK 的 Radio Interface Layer Daemon
```

`ql_rild` 是 `ql_data_call_init()` 的服务后端。杀掉它之后，即使状态机走回 `dial_stat_none` 重新调用 `ql_data_call_init()`，SDK 底层已死，初始化会持续失败或挂起。

**③ 应用层 CFUN 恢复链已是空操作（承袭 `7bf8205` 的注释）**

```c
case dial_stat_stop_cfun:
    ALOGI("stop cfun");
    // Ql_SendAT("AT+CFUN=0");   ← 注释掉，什么也不做
    p_dial_mng->dial_st = dial_stat_start_cfun;
    break;
case dial_stat_start_cfun:
    p_dial_mng->dial_st = dial_stat_none;   // 直接回到 none，无任何 RF 操作
    break;
```

三个问题叠加：**ql_netcall 被 kill → ql_rild 被 kill → CFUN 无效 → SDK 无法初始化**，整条恢复链断裂。状态机虽然还在跑，但没有任何机制能真正发起重拨。

`922ce40`（2025-11-26）的提交信息 `阶段备份，使用自动重连前备份` 也印证了团队此时已知晓问题，在准备切回 SDK 方式前做了保存。

`07a2ab3`（2025-12-11，同一天）只加了 SD 卡日志模块，`dial.c` 原样未动，仍处于断裂状态。

---

#### 阶段四：`e92733c`（2025-12-11）— 恢复重拨 + 新增 Fast Retry

同一天晚些时候，完整切回 `ql_data_call` SDK 方式，并新增了"3 次快速重试后进入持久循环"的 Fast Retry 机制（即本次分析基线 `6954a787` 的直接上游）。

---

#### 阶段对比总表

| 提交 | 时间 | 断网能否重拨 | 原因 |
|------|------|------------|------|
| `b3d8422` 及更早 | 2023-03 | ✅ 能 | SDK 20s 自动重连 + CFUN 应用层恢复链完整 |
| `7bf8205` | 2023-07 | ✅ 基本能 | CFUN 注释掉，但 SDK 自动重连仍在 |
| `570b50f` ~ `f580980` | 2023-09 ~ 2024-04 | ✅ 同上 | 未改核心重拨逻辑 |
| **`7910fee`** | **2024-10** | **❌ 不能** | **切 ql_netcall + killall ql_rild 打断 SDK** |
| `89f1866` ~ `07a2ab3` | 2024-10 ~ 2025-12 | **❌ 不能** | dial.c 未恢复，问题延续 |
| **`e92733c`**（及之后）| **2025-12** | **✅ 恢复** | 切回 ql_data_call SDK，加 Fast Retry |

**结论：不是"07a2ab3 之前每一条提交都没有重拨机制"。2023 年的代码是好的，是 2024-10 的 `7910fee` 一刀切掉了重拨，整整 14 个月后才由 `e92733c` 修回来。断网后无法联网的根本原因是 SDK 层被 `killall -9 ql_rild` 打断，而非路由表残留。**

---

## 二、dial.c 详细分析

### 2.1 头文件调整（`#include <sys/wait.h>` 移动）

**修改前**（位于 dial.c 顶部）：
```c
#include <sys/wait.h> // 必须引入这个头文件
```

**修改后**：该行从 dial.c 删除，移入 misc.c。

**代码层分析**：`sys/wait.h` 提供 `waitpid()`，该函数在 `misc.c::restartNetworkServices()` 中使用（双重 fork 方案的一部分）。基准版本中头文件放错了位置。

**功能/行为层分析**：不影响功能，只是头文件归位。

**修改动机推断**：重构 `restartNetworkServices()` 时将 `waitpid()` 的使用移到 misc.c，头文件随之一并移走，dial.c 自身无需此头文件。

---

### 2.2 `read_cfun_count()` 未初始化变量修复

**修改前**：
```c
int count;
fscanf(file, "%d", &count);
```

**修改后**：
```c
int count = 0;
fscanf(file, "%d", &count);
```

**代码层分析**：添加 `= 0` 初始值。

**功能/行为层分析**：若 `fscanf()` 因文件内容格式异常而失败（返回 0 或 EOF），旧版本 `count` 为未初始化的栈内随机值，`read_cfun_count()` 会返回随机数。此返回值会与 `MAX_CALLS(10)` 比较，随机值若大于 10 则 `restart_cfun()` 误判次数已满，跳过关键的 CFUN 重置。

**修改动机推断**：防御性编程，消除 fscanf 失败时的 UB（未定义行为）。

---

### 2.3 `dial_mng_new()` 空指针防护

**修改前**：
```c
dial_mng_t *p_dial_mng = (dial_mng_t *)calloc(1, sizeof(dial_mng_t));
memset((char *)p_dial_mng, 0, sizeof(dial_mng_t));
```

**修改后**：
```c
dial_mng_t *p_dial_mng = (dial_mng_t *)calloc(1, sizeof(dial_mng_t));
if (p_dial_mng == NULL) {
    perror("calloc dial_mng_t failed");
    return NULL;
}
```

**代码层分析**：
1. 移除了 `calloc` 之后多余的 `memset`（`calloc` 本身保证返回的内存已清零）。
2. 添加 NULL 判断，`calloc` 失败时打印错误并提前返回。

**功能/行为层分析**：若 `calloc` 在极端内存不足时返回 NULL，旧版本直接对 NULL 指针执行 `memset`，导致 SIGSEGV 崩溃。新版本提前返回 NULL，由调用方 `main()` 捕获并打印错误退出。

**修改动机推断**：消除隐患，也与 main() 中对返回值的 NULL 检查配套（见 2.4）。

---

### 2.4 `main()` 中对 `dial_mng_new()` 返回值增加检查

**修改后**：
```c
p_dial_mng = dial_mng_new();
if (p_dial_mng == NULL) {
    fprintf(stderr, "Failed to allocate dial_mng_t\n");
    return -1;
}
```

**代码层分析**：在 main() 中对 `dial_mng_new()` 返回值进行 NULL 判断，内存分配失败时提前退出。

**功能/行为层分析**：与 2.3 配套，完整地处理了内存分配失败的异常路径。

---

### 2.5 `sig_handler()` 信号安全修复

**修改前**：
```c
static dial_mng_t *p_dial_mng = NULL;
void sig_handler(int signum)
{
    if ((signum == SIGKILL))
    {
        if (p_dial_mng != NULL)
        {
            free(p_dial_mng);
            p_dial_mng = NULL;
        }
    }
}
```

**修改后**：
```c
static dial_mng_t *p_dial_mng = NULL;
static volatile sig_atomic_t g_sigint_received = 0;

void sig_handler(int signum)
{
    if (signum == SIGINT)
    {
        // 信号处理函数中只能使用 async-signal-safe 操作
        // free() 不是 async-signal-safe，不能在此调用
        g_sigint_received = 1;
    }
}
```

**代码层分析**：
1. 信号从 `SIGKILL` 改为 `SIGINT`：`SIGKILL` 是不可捕获信号，`signal(SIGKILL, ...)` 实际上无效（在 Linux 上 `signal()` 对 SIGKILL 调用会被忽略），旧版注册是无意义的。
2. 移除信号处理函数中的 `free()` 调用：POSIX 规定信号处理函数中只能调用 async-signal-safe 函数，`free()` 不在此列，在信号处理函数中调用 `free()` 属于未定义行为，可能导致堆损坏。
3. 改为设置 `volatile sig_atomic_t g_sigint_received = 1`，在主循环中安全处理清理逻辑。

**功能/行为层分析**：
- 旧版：`SIGKILL` 信号处理形同虚设；按 `Ctrl+C`（SIGINT）时因未注册 SIGINT 处理器，进程直接被默认行为终止（不会调用 `log_close()`，日志可能丢失最后一条记录）。
- 新版：`SIGINT` 时设标志位，主循环（`while(1)` 开头）检测到后执行 `free(p_dial_mng) + log_close() + exit(0)`，实现了干净关闭。

**修改动机推断**：修复信号安全性 bug（见 CLAUDE.md C.1 #13），同时保证进程响应 Ctrl+C 时日志能正常落盘。

---

### 2.6 主循环内 `g_sigint_received` 检查

**修改后**（`dial_loop()` 的 `while(1)` 开头）：
```c
if (g_sigint_received) {
    dial_log("[EVENT] SIGINT received. Cleaning up and exiting.\n");
    if (p_dial_mng != NULL) {
        free(p_dial_mng);
        p_dial_mng = NULL;
    }
    log_close();
    exit(0);
}
```

**代码层分析**：与 2.5 的信号标志配套，主循环每 50ms 轮询一次信号标志，安全执行清理。

**功能/行为层分析**：日志打印 `[EVENT] SIGINT received`，可在日志文件中追溯到进程是被主动 kill 还是异常崩溃。

---

### 2.7 `get_cpu_temp()` 新增温度读取函数

**修改后**（新增）：
```c
static int get_cpu_temp(void) {
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!fp) return -1;
    int raw = -1;
    fscanf(fp, "%d", &raw);
    fclose(fp);
    return (raw >= 0) ? raw / 1000 : -1;
}
```

**代码层分析**：从 Linux thermal sysfs 读取原始温度值（单位毫摄氏度），转换为摄氏度整数，失败返回 -1。

**功能/行为层分析**：每 30s 心跳日志中增加 `TEMP:xx` 字段（见 2.11），用于排查高温导致的模组异常（EC200A 正常工作上限 +75°C，扩展上限 +85°C）。

**修改动机推断**：客户设备频繁掉线，温度可能是诱因之一，此功能为远程问题排查提供数据支撑。

---

### 2.8 `clean_str()` 添加 unused 属性

**修改前**：
```c
static void clean_str(char *str) {
```

**修改后**：
```c
__attribute__((unused))
static void clean_str(char *str) {
```

**代码层分析**：添加 GCC 的 `__attribute__((unused))` 属性，告知编译器此函数即使未被调用也不报 `-Wunused-function` 警告。

**修改动机推断**：`clean_str()` 在当前主流程中已无调用点（旧调用已被移除或改用 `strpbrk` 替代），但保留了函数体以备将来使用。

---

### 2.9 大规模 AT 诊断工具函数新增（14个）

本次新增了以下一组 `static` 诊断查询函数，全部采用相同的安全模式：`popen → 逐行 fgets → 提取匹配行 → strncpy+null终止 → pclose`：

| 函数名 | AT 命令 | 用途 |
|---|---|---|
| `get_imei_safe()` | `AT+CGSN` | 获取 IMEI（14-16位纯数字行） |
| `get_imsi_at_safe()` | `AT+CIMI` | 获取 IMSI（14-16位纯数字行） |
| `get_operator_safe()` | `AT+COPS?` | 获取运营商名称及接入技术 |
| `get_cesq_safe()` | `AT+CESQ` | 获取 3GPP 标准信号质量（RSRP/RSRQ） |
| `get_creg_safe()` | `AT+CREG?` | 获取注册状态及服务小区 CID |
| `get_cfun_safe()` | `AT+CFUN?` | 获取当前模块功能模式 |
| `get_cgmr_safe()` | `AT+QGMR` | 获取固件版本（Quectel 私有命令） |
| `get_csub_safe()` | `AT+CSUB` | 获取固件子版本号 |
| `get_cgdcont_safe()` | `AT+CGDCONT?` | 获取 PDP 上下文配置（验证 APN） |
| `get_nw_mode_pref_safe()` | `AT+QNWPREFCFG="mode_pref"` | 获取网络制式偏好 |
| `get_cgpaddr_safe()` | `AT+CGPADDR` | 获取 PDP 已分配 IP 地址 |
| `get_qtemp_safe()` | `AT+QTEMP` | 获取模块温度（排查热关机） |
| `get_ceer_safe()` | `AT+CEER` | 获取最近一次错误原因 |
| `get_cgact_safe()` | `AT+CGACT?` | 获取 PDP 上下文激活状态 |

**代码层分析**：所有函数均接受 `char *out, int len` 参数，使用调用方的栈缓冲区，无动态分配，线程安全。

**功能/行为层分析**：
- 启动时（`dial_loop()` 初始化段）调用 IMEI、IMSI、FW、SUB、CFUN、CGDCONT、NW_MODE，打印完整设备档案，便于在日志中追溯具体设备和固件版本。
- 故障时（首次 Ping 失败、SIM 断卡、ST_RECOVERY 前）调用 CESQ、CREG、CGPADDR、QTEMP、CEER、CGACT，提供详细断线诊断。
- 5分钟扩展心跳中调用 CESQ、CREG、CGPADDR，监控信号质量和 IP 漂移。

**修改动机推断**：eSIM 瞬断事件难以复现，增加现场诊断信息是解决问题的关键路径。

---

### 2.10 `build_ext_line()` 诊断行格式化函数新增

```c
static int build_ext_line(char *buf, int len,
                           const char *qcsq_str,
                           const char *cell_str,
                           const char *ip_str,
                           const char *temp_str,
                           const char *ceer_str,
                           const char *cgact_str)
```

**代码层分析**：将 CESQ/CREG/CGPADDR/QTEMP/CEER/CGACT 的原始 AT 响应解析并格式化为 `key:val | key:val` 形式的单行字符串：
- `RSRP` 由 `rsrp_raw - 141` 换算（dBm）
- `RSRQ` 由 `rsrq_raw / 2 - 19` 换算（dB）
- `CID` 从 `+CREG: n,stat,"lac","ci"` 的第3-4对引号提取
- `IP` 从 `+CGPADDR: id,"x.x.x.x"` 提取，兼容有无引号和逗号后空格

**功能/行为层分析**：让日志输出结构化，便于 grep 解析；传入 NULL 的字段跳过，5分钟心跳和 DIAG 快照复用同一格式化逻辑。

---

### 2.11 心跳日志增强：新增 `TEMP` 字段

**修改前**：
```c
dial_log("[HEARTBEAT] SIM:%s | REG:%d | CSQ:%d | DownTime:%lds\n",
       sim_str, reg_stat, rssi, downtime_sec);
```

**修改后**：
```c
int cpu_temp = get_cpu_temp();
dial_log("[HEARTBEAT] SIM:%s | REG:%d | CSQ:%d | TEMP:%d | DownTime:%lds\n",
       sim_str, reg_stat, rssi, cpu_temp, downtime_sec);
```

**功能/行为层分析**：每 30s 心跳附带设备温度，可通过趋势分析判断高温是否与掉线时间相关联。

**修改动机推断**：commit `259b37b`，明确注明 EC200A 工作温度规格（-30~+75°C 标准，最高扩展 +85°C），说明曾遇到疑似热相关问题。

---

### 2.12 心跳日志内 `CSQ` 获取简化

**修改前**：
```c
int rssi = -1;
if (exec_cmd_safe("serial_atcmd at+csq", cmd_buf, sizeof(cmd_buf)) == 0) {
    rssi = get_csq_value_safe();                
}
```

**修改后**：
```c
int rssi = get_csq_value_safe();
```

**代码层分析**：`get_csq_value_safe()` 自身已通过 `popen` 执行 `serial_atcmd at+csq`，无需先用 `exec_cmd_safe()` 重复执行。旧版执行了两次 AT 命令。同时心跳内 `g_last_reg_stat = reg_stat` 缓存新增，供恢复逻辑使用。

---

### 2.13 新增 5 分钟扩展心跳（`next_ext_heartbeat_ts`）

**修改后**（新增代码块）：
```c
if (has_notified_connect && tnow >= next_ext_heartbeat_ts) {
    next_ext_heartbeat_ts = tnow + 5 * 60 * 1000ULL;

    char qcsq_str[64]  = {0};
    char cell_str[256] = {0};
    char ip_str[128]   = {0};

    get_cesq_safe(qcsq_str, sizeof(qcsq_str));
    get_creg_safe(cell_str, sizeof(cell_str));
    get_cgpaddr_safe(ip_str, sizeof(ip_str));

    char ext_line[256] = {0};
    int n = build_ext_line(ext_line, sizeof(ext_line),
                           qcsq_str, cell_str, ip_str,
                           NULL, NULL, NULL);
    if (n > 0 && ext_line[0]) {
        dial_log("[HEARTBEAT] %s\n", ext_line);
    }
}
```

**功能/行为层分析**：
- 仅在首次网络连接成功后（`has_notified_connect=1`）开始计时，避免无网时频繁 AT 调用。
- 首次连接成功时 `next_ext_heartbeat_ts = 0` 触发立即执行，记录初始连接状态（小区 CID、RSRP、分配 IP）。
- 从故障中恢复（Ping 再次成功）时同样设 `next_ext_heartbeat_ts = 0`，立即打印恢复后的信号状态。
- 每 5 分钟一次（3 次 popen），AT 负担远低于 30s 基本心跳，不影响正常检测节奏。

**修改动机推断**：基本心跳（CPIN/CEREG/CSQ）无法捕获信号质量趋势，RSRP/CID 字段可追踪小区切换，IP 字段可检测地址漂移，这些都是 eSIM 设备远程诊断的关键数据。

---

### 2.14 Fast Fail 机制完整重构

**修改前**（旧逻辑）：
```c
if (is_fast_fail_mode && !has_notified_connect) {
    if (tnow - start_loop_ts > FAST_FAIL_TIMEOUT_MS) {
        dial_log("[INIT] Fast Fail: Timeout %dms without connection...\n", FAST_FAIL_TIMEOUT_MS);
        exit(1); 
    }
}
```
- `FAST_FAIL_TIMEOUT_MS = 10000ms`（10s）
- 以 `dial_loop()` 入口为计时起点

**修改后**（新逻辑）：
```c
// 新增常量
#define FAST_FAIL_TIMEOUT_MS 10000 // PDP 建立后留给 Ping 成功的时间窗口
#define PDP_WAIT_TIMEOUT_MS  60000 // PDP 建立本身允许的最长等待

if (is_fast_fail_mode && !has_notified_connect) {
    if (g_pdp_connected) {
        if (pdp_ready_ts == 0) {
            pdp_ready_ts = tnow;
            dial_log("[INIT] PDP connected, start Ping window (%dms).\n", FAST_FAIL_TIMEOUT_MS);
        }
        if (tnow - pdp_ready_ts > FAST_FAIL_TIMEOUT_MS) {
            dial_log("[INIT] Fast Fail: PDP up but no Ping success in %dms...\n", ...);
            log_close();
            exit(1);
        }
    } else {
        pdp_ready_ts = 0;
        if (tnow - start_loop_ts > PDP_WAIT_TIMEOUT_MS) {
            dial_log("[INIT] Fast Fail: PDP not established in %ds...\n", ...);
            log_close();
            exit(1);
        }
    }
}
```

**代码层分析**：引入 `pdp_ready_ts` 变量和 `g_pdp_connected` 外部标志，将 Fast Fail 的单一超时拆分为两阶段：
1. **PDP 等待阶段**：从 `dial_loop()` 入口起最多等 60s，PDP 不建立则退出。
2. **Ping 验证阶段**：从 `g_pdp_connected=1` 时刻起最多 10s，Ping 未成功则退出。

**功能/行为层分析**：旧版的核心 Bug——`ql_data_call_init()` 最多耗时 20s，加上 SIM 初始化、APN 设置、`ql_data_call_start()` 等步骤，PDP 建立前就已超过 10s 窗口，导致合法的拨号过程被误判为失败退出。新版将 10s 窗口改为仅对 **PDP 已建立后的 Ping 超时** 生效，避免误判。

**修改动机推断**：commit `6c02b01` 明确标注修复 "Fast Retry 超时误判"，这是该版本中对用户影响最大的 bug 之一。

---

### 2.15 Ping 抖动过滤器（`PING_FAIL_THRESHOLD`）

**修改后**（ST_PING 失败处理中）：
```c
#define PING_FAIL_THRESHOLD 3

int ping_fail_count = 0; // 连续 ping 失败计数

// Ping 失败处理：
ping_fail_count++;
if (ping_fail_count < PING_FAIL_THRESHOLD) {
    // 还未达到阈值，跳过本次，不更新 start_fail_ts
    stage = ST_STATUS;
    break;
}
// 达到阈值后才启动故障计时 ...
```

**功能/行为层分析**：单次或两次偶发 Ping 失败不触发故障计时，避免网络抖动（路由器短暂重启、基站切换）导致误触发 L1 软重拨。必须连续失败 3 次（约 4.5s）才认为是真实故障。

Ping 成功后：`ping_fail_count = 0`（立即清零，保证抖动过后下次故障从头计数）。

SIM 断卡检测到时（`has_notified_connect` 状态切换）：也清零 `ping_fail_count`，防止 SIM 断卡前积累的计数影响断卡后的恢复逻辑判断（这是 commit `7e0648d` 修复的 L2/L3 永不触发 bug 的一部分）。

---

### 2.16 首次故障 DIAG 快照（`diag_snap_done`）

**修改后**（ST_PING 达到阈值、`start_fail_ts` 首次记录时）：
```c
if (start_fail_ts == 0) {
    start_fail_ts = tnow;
    if (!diag_snap_done) {
        diag_snap_done = 1;
        // 6 次 AT 查询：CESQ/CREG/CGPADDR/QTEMP/CEER/CGACT
        char d_qcsq[64], d_cell[256], d_ip[128], d_temp[64], d_ceer[128], d_cgact[128];
        get_cesq_safe(d_qcsq, ...);
        get_creg_safe(d_cell, ...);
        get_cgpaddr_safe(d_ip, ...);
        get_qtemp_safe(d_temp, ...);
        get_ceer_safe(d_ceer, ...);
        get_cgact_safe(d_cgact, ...);
        char diag[512] = {0};
        build_ext_line(diag, ...);
        if (diag[0]) dial_log("[DIAG] %s\n", diag);
    }
}
```

**功能/行为层分析**：每次故障周期只打印一次 DIAG 快照（`diag_snap_done` 标志保护），记录故障发生瞬间的 RSRP/RSRQ、小区 CID、分配 IP、模块温度、错误原因和 PDP 激活状态，这些是排查断线根因最有价值的快照数据。

恢复成功（Ping 成功）时：`diag_snap_done = 0`，允许下次故障重新触发快照。

---

### 2.17 ST_RECOVERY 前置诊断快照

**修改后**（进入 ST_RECOVERY 的 `switch` 语句之前）：
```c
case ST_RECOVERY:
    {
        char ceer_buf[128] = {0};
        get_ceer_safe(ceer_buf, sizeof(ceer_buf));
        if (ceer_buf[0]) {
            dial_log("[RECOVERY] LastErr: %s\n", ceer_buf);
        }
        char cgact_buf[128] = {0};
        get_cgact_safe(cgact_buf, sizeof(cgact_buf));
        if (cgact_buf[0]) {
            dial_log("[RECOVERY] PDP state: %s\n", cgact_buf);
        }
    }
    switch (recovery_level) { ... }
```

**功能/行为层分析**：每次执行恢复操作前，先记录最近错误原因（CEER）和 PDP 激活状态（CGACT），帮助在日志中定位是哪一层 PDP 故障触发了恢复，区分 "网络层故障" 和 "运营商拒绝" 等不同根因。

---

### 2.18 `on_network_connected()` 变量名修复

**修改前**：
```c
printf(">>>>> Callback: Network is fully ready! Interface: %s <<<<<\n", g_dev_name);
system("echo 1 > /tmp/network_status");
update_network_status(1);
```

**修改后**：
```c
printf(">>>>> Callback: Network is fully ready! Interface: %s <<<<<\n", g_if_name);
update_network_status(1);
```

**代码层分析**：`g_dev_name` 是一个旧的静态字符串 `"ccinet0"`（硬编码，在 `dial.c` 中定义），而实际网卡名由 SDK 回调写入 `g_if_name`（`data_call.c`）。修复后打印的是实际接口名。同时去除了 `system("echo 1 > /tmp/network_status")` 的重复调用（`update_network_status(1)` 已完成该操作）。

---

### 2.19 `dial_loop()` 初始化段日志全面增强

新增以下日志输出：
```c
// 启动时设备档案
dial_log("[INIT] ICCID: %s\n", ...);
dial_log("[INIT] IMEI:  %s\n", ...);
dial_log("[INIT] FW:    %s\n", ...);  // AT+QGMR 固件版本
dial_log("[INIT] SUB:   %s\n", ...);  // AT+CSUB 子版本
dial_log("[INIT] IMSI:  %s\n", ...);
dial_log("[INIT] Operator: %s\n", ...);
dial_log("[INIT] CFUN: %s\n", ...);
dial_log("[INIT] PDP cfg: %s\n", ...);
dial_log("[INIT] NW mode: %s\n", ...);

// data_call_init 进度日志
dial_log("[INIT] Waiting for ql_data_call_init...\n");
if (retry_cnt % 20 == 0) {
    dial_log("[INIT] data_call_init retrying, remaining=%d\n", retry_cnt);
}
dial_log("[INIT] data_call_init OK.\n");  // 或失败原因

// 拨号启动结果
dial_log("[INIT] Starting Data Call...\n");
if (ret != 0) {
    dial_log("[INIT] ql_data_call_start failed: %d\n", ret);
}
```

**功能/行为层分析**：彻底消除之前最长 20s 的静默窗口。任何启动失败都有明确的日志定位点。设备档案在每次重启时记录，可通过日志追溯具体硬件、固件、运营商组合。

---

### 2.20 固件版本命令修复：`AT+CGMR` → `AT+QGMR`

**修改前**（基准版本中）：`get_cgmr_safe()` 使用 `AT+CGMR`

**修改后**：
```c
FILE *fp = popen("serial_atcmd at+qgmr", "r");
```

同时 `get_csub_safe()` 改为跳过 `"SubEdition:"` 前缀，只保留版本号。

**功能/行为层分析**：`AT+CGMR`（标准命令）在部分 Quectel 固件中返回 `"XX-YY-ZZ"` 占位符，`AT+QGMR`（Quectel 私有扩展）返回真实版本号如 `EC200ACNTAR02A04M2G_OCPU`。`AT+CSUB` 原来包含 `"SubEdition:V06"` 前缀，现在裁剪为只留 `V06`，减少日志冗余。

---

### 2.21 `persistent` 模式下 `retry_count` 清零修复

**修改前**：
```c
if (is_fast_fail_mode) {
    clear_retry_count();
    is_fast_fail_mode = 0;
}
```

**修改后**：
```c
clear_retry_count();  // 无条件清零（无论 is_fast_fail_mode）
if (is_fast_fail_mode) {
    is_fast_fail_mode = 0;
}
```

**代码层分析**：`clear_retry_count()` 从 `if (is_fast_fail_mode)` 块内移到块外，改为无条件调用。

**`is_fast_fail_mode` 的判定依据**（`check_and_update_retry_count()` 行为）：

| 启动次数 | 文件内容 | 函数行为 | 返回值 | `is_fast_fail_mode` |
|---------|---------|---------|-------|-------------------|
| 第 1 次 | 无文件 | count=0 → count++=1 → 写入 "1" | 1 | `1`（Fast Retry 模式）|
| 第 2 次 | "1" | count=1 → count++=2 → 写入 "2" | 2 | `1` |
| 第 3 次 | "2" | count=2 → count++=3 → 写入 "3" | 3 | `1` |
| **第 4 次** | **"3"** | **count=3 >= 3 → 不更新文件，直接返回** | **-1** | **`0`（persistent 模式）** |

**旧逻辑 Bug 完整复现路径**：
```
第 1 次启动 → Fast Retry 失败 → exit(1) → 文件="1"
第 2 次启动 → Fast Retry 失败 → exit(1) → 文件="2"
第 3 次启动 → Fast Retry 失败 → exit(1) → 文件="3"
第 4 次启动 → count=3 >= 3 → is_fast_fail_mode=0 → persistent 模式
  → 拨号成功，Ping 通
  → 旧逻辑：if(is_fast_fail_mode) 为 false → clear_retry_count() 不执行
  → /tmp/dial_retry_count 永久残留，内容仍为 "3"

设备运行一段时间后因任意原因重启（L3/掉电/看门狗）：
  → 读文件 count=3 → 返回 -1 → 直接进入 persistent 模式
  → ❌ 永久跳过 Fast Retry 的 3 次快速恢复机会
  → 即使这是全新独立故障，Fast Retry 机制永远失效
```

**新逻辑正确性验证（边界情况）**：

`clear_retry_count()` 内有 `access()` 前置守护：
```c
static void clear_retry_count() {
    if (access(RETRY_COUNT_FILE, F_OK) == 0) {  // 文件不存在则直接返回
        unlink(RETRY_COUNT_FILE);
        dial_log("[INIT] Dial success. Retry count cleared.\n");
    }
}
```
- **persistent 模式首次 Ping 成功**：文件存在（内容"3"），删除，日志打印一次。
- **后续每次 Ping 成功**（1500ms 一次）：`access()` 返回 -1，跳过，无日志洪水，无副作用。
- **persistent 模式从未 Ping 成功**：文件一直是"3"，每次重启仍进入 persistent 模式——合理，网络根本不通不该快速重启。

**修改动机**：Fast Retry 的设计意图是"启动失败时快速重试3次，成功后下次断网重启重新获得3次机会"。旧逻辑破坏了"成功后重置"这一前提，导致 persistent 模式成功后 Fast Retry 永久失效。

---

### 2.22 SIM 断卡诊断与恢复逻辑重构

这是本版本改动最多、最复杂的部分，涉及多个 commit（`53c9853`、`26342e6`、`94d0fa3`、`ab20799`、`d51f798`）。

#### 2.22.1 新增 `sim_error_active` 状态变量

```c
int sim_error_active = 0;    // SIM 曾发生断开，READY 恢复时打印日志用
```

此变量用于区分 "SIM 首次断卡" 和 "SIM 断卡期间持续轮询" 两种状态，避免每轮 ST_SIM 都重复打印断卡日志。

#### 2.22.2 SIM 断卡时打印完整诊断快照

**修改后**（SIM 非 READY 且 `has_notified_connect=1` 时）：
```c
dial_log("[ERROR] SIM Card disconnected during runtime, CPIN=%s.\n", sim_str);

// AT 层快照
{
    char sa[64] = {0}, sb[96] = {0};
    FILE *fa = popen("serial_atcmd at+qsimstat? 2>/dev/null | grep '^+' | head -1 | tr -d '\\r\\n'", "r");
    if (fa) { fgets(sa, sizeof(sa), fa); pclose(fa); }
    FILE *fb = popen("serial_atcmd at+cereg? 2>/dev/null | grep '^+' | head -1 | tr -d '\\r\\n'", "r");
    if (fb) { fgets(sb, sizeof(sb), fb); pclose(fb); }
    dial_log("[DIAG-SIM] %s | %s\n", sa[0] ? sa : "TIMEOUT", sb[0] ? sb : "TIMEOUT");
}

// dmesg 内核日志
{
    char dm[512] = {0};
    FILE *fd = popen("dmesg 2>/dev/null | grep -iE 'sim|uicc|usim' | tail -1 | tr -d '\\r\\n'", "r");
    if (fd) { fgets(dm, sizeof(dm), fd); pclose(fd); }
    dial_log("[DIAG-DMESG] %s\n", dm[0] ? dm : "(none)");
}

has_notified_connect = 0;
ping_fail_count = 0;       // 清零 jitter 计数
sim_error_active = 1;      // 进入 SIM 故障状态
```

**注意**：commit `d51f798` 将 QSIMSTAT 快照**从断卡时刻移至恢复时刻**。断卡时 AT 端口可能阻塞，此时执行 QSIMSTAT 必然超时。改在恢复时执行，才能获取有效数据。日志前缀因此分为 `[DIAG-SIM]`（断卡时，QSIMSTAT 可能显示 TIMEOUT）和 `[DIAG-SIM-REC]`（恢复时，QSIMSTAT 有效）。

#### 2.22.3 SIM 恢复时立即强制触发 L1

**修改后**（SIM 从非 READY 恢复为 READY 时）：
```c
} else if (sim_error_active) {
    /* 打印恢复日志和 QSIMSTAT/dmesg 快照 ... */
    sim_error_active = 0;

    if (start_fail_ts != 0 && enable_policy_recovery) {
        dial_log("[INFO] SIM recovered, forcing immediate L1 recovery (skip 60s wait).\n");
        recovery_level = 1;
        stage = ST_RECOVERY;
        break;  /* 直接进 ST_RECOVERY，跳过下方 stage = ST_SIGNAL */
    }
}
```

**功能/行为层分析**：eSIM 瞬断场景下，SIM 重新 READY 后 CP 内部数据链路已中断（现场日志实证）。若走正常流程，需等待 Ping 连续失败 3 次（约 4.5s）触发计时，再等 60s 才能触发 L1，合计约需 64.5s + SIM 下线时长。优化后 SIM 恢复即刻强制 L1，总断联时长约为：SIM 下线时长 + L1 耗时（约 5s）+ Ping 验证（约 1.5s）≈ 30s，比原来减少约 57%。

---

## 三、CPIN 查询逻辑专项审查（紧急）

> 背景：客户设备报告 SIM 卡突然断连，导致设备频繁断线，需要完整梳理 CPIN 相关逻辑。

### 3.1 AT+CPIN? 在代码中的所有调用点

| 调用位置 | 文件:行号 | 调用时机 | 实现方式 |
|---|---|---|---|
| `get_cpin_status_str()` 函数体 | `dial.c:1108` | 被下方两处调用 | `popen("serial_atcmd at+cpin?", "r")` |
| 心跳日志（Part A） | `dial.c:1448` | 每 30s，无条件 | 调用 `get_cpin_status_str()` |
| ST_SIM 状态机 | `dial.c:1505` | 每 800ms（受 `next_ts` 节流） | 调用 `get_cpin_status_str()` |

**调用实现**（`get_cpin_status_str()`，dial.c:1107-1145）：
```c
void get_cpin_status_str(char *out_buf, int len) {
    FILE *fp = popen("serial_atcmd at+cpin?", "r");
    if (!fp) {
        dial_log("[WARN] AT+CPIN? popen failed (%s), treating as ERROR\n", strerror(errno));
        strncpy(out_buf, "ERROR", len - 1);
        return;
    }
    strncpy(out_buf, "UNKNOWN", len - 1);
    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        char *p = strstr(line, "+CPIN: ");
        if (p) {
            p += 7;
            // 截断换行，复制到 out_buf
            break;
        }
    }
    pclose(fp);
}
```

### 3.2 查询频率

| 场景 | 标称值 | 实际频率 | 说明 |
|---|---|---|---|
| 30s 基本心跳 | 30s | 每 30s 一次 | 独立计时器 `next_heartbeat_ts` |
| ST_SIM 状态机 | `SIM_INTERVAL_MS=800ms` | **实际约 3100ms** | `next_ts` 全状态机共享，见下方详细分析 |

#### ⚠️ 关键纠正：CPIN 实际检测间隔是 ~3100ms，不是 800ms

`next_ts` 是**全状态机唯一共享变量**，追踪一个完整循环的执行节奏：

```
t=T       ST_SIM 执行，设 next_ts = T + 800ms
            ↓ 等 800ms
t=T+800ms ST_SIGNAL 执行，设 next_ts = T + 800ms + 800ms = T+1600ms
            ↓ 等 800ms
t=T+1600ms ST_PING 执行（Ping），设 next_ts = T + 1600ms + 1500ms = T+3100ms
            ↓ 等 1500ms
t=T+3100ms ST_STATUS → ST_SIM：检查 next_ts，T+3100ms 到期，执行下一次 CPIN 查询
```

**实际 CPIN 检测间隔 = 800 + 800 + 1500 = 3100ms ≈ 3 秒一次**。

`SIM_INTERVAL_MS = 800` 这个值的真实含义是"ST_SIM 执行完毕到 ST_SIGNAL 执行之间的等待时长"，并非 CPIN 的轮询周期。将其改为任何 ≤ 1500ms 的值，CPIN 的真实检测频率都不会有任何变化，因为整个循环被 `PING_INTERVAL_MS=1500ms` 所支配。这个命名具有误导性，也说明**此参数值没有经过明确的设计推导**，属于历史遗留的随意填值。

**直接影响**：SIM 掉卡后，主循环最多需要等待一个完整周期（~3100ms）才能进入 ST_SIM 检测 CPIN，再加上 `serial_atcmd at+cpin?` 命令本身的执行时间（正常 <1s，阻塞时不可知），最短感知延迟约为 **3~4 秒**。

### 3.3 异常响应流程

```
AT+CPIN? 返回非 READY（如 "SIM FAILURE", "NOT INSERTED", "ERROR", "UNKNOWN"）
    │
    ├─ 如果 has_notified_connect == 1（当前会话已经连接成功过）：
    │   ├─ 打印 [ERROR] SIM Card disconnected during runtime, CPIN=xxx.
    │   ├─ popen(AT+QSIMSTAT?) → [DIAG-SIM] ...（断卡时可能超时）
    │   ├─ popen(dmesg | grep sim) → [DIAG-DMESG] ...（断卡时可能显示上次事件）
    │   ├─ has_notified_connect = 0
    │   ├─ ping_fail_count = 0（清零抖动计数）
    │   └─ sim_error_active = 1
    │
    ├─ 如果 has_notified_connect == 0（未连接/已断开）：
    │   └─ （静默，不重复打印）
    │
    ├─ 设置 start_fail_ts = tnow（如果尚未设置）
    │
    └─ stage = ST_SIGNAL（继续往下走，Ping 必然失败，由 fail_duration 触发恢复）

AT+CPIN? 返回 READY 且 sim_error_active == 1（SIM 从故障状态恢复）：
    ├─ 计算 sim_down_secs = (tnow - start_fail_ts) / 1000
    ├─ 打印 [INFO] SIM recovered: CPIN=READY (down ~Xs)
    ├─ popen(AT+QSIMSTAT?) → [DIAG-SIM-REC] ...（此时 AT 端口可用，有效）
    ├─ popen(dmesg | grep sim) → [DIAG-DMESG-REC] ...（此时内核日志已写入）
    ├─ sim_error_active = 0
    └─ 如果 start_fail_ts != 0 && enable_policy_recovery：
        ├─ 打印 [INFO] SIM recovered, forcing immediate L1 recovery (skip 60s wait).
        ├─ recovery_level = 1
        └─ stage = ST_RECOVERY（跳过 ST_SIGNAL，直接执行 L1）
```

### 3.4 掉卡感知能力分析

#### 最快检测时间

**结论：实际检测延迟为 ~3100ms（一个完整状态机周期），而非 800ms。**

如 3.2 节所述，`next_ts` 被全状态机共享，ST_SIM 实际每 **~3100ms** 才执行一次 CPIN 查询。掉卡事件发生时，主循环可能刚完成 ST_SIM 进入 ST_SIGNAL 等待，最坏情况需要等整整 3100ms 才能再次执行 ST_SIM 检测到掉卡。

**关键风险点：AT 端口阻塞**

代码注释明确提到（commit `d51f798`）：
> "断卡时 AT 端口阻塞，QSIMSTAT 必然超时"

这意味着 eSIM 掉卡时 `serial_atcmd at+cpin?` 本身可能阻塞较长时间（取决于 `serial_atcmd` 工具的超时设置）。若每次 CPIN? 查询阻塞 5-10s，则整个状态机循环会卡住，掉卡感知时间会显著增加（可能达到数十秒）。

#### 检测到后触发什么动作

1. 记录 `start_fail_ts`（故障计时开始）
2. 后续 Ping 因网络断开而失败
3. 新版本：**SIM 恢复后立即强制 L1**（stop→start），断联恢复时间约 30s
4. 旧版本：等 Ping 连续失败 3 次 + 60s 才触发 L1，约需 65s

#### 检测盲区分析

| 场景 | 是否存在盲区 | 说明 |
|---|---|---|
| SIM 掉卡后状态机在 ST_PING | 存在短暂盲区 | 最多等 1500ms Ping 间隔到期后，回到 ST_STATUS→ST_SIM |
| AT 端口阻塞期间 | **存在较大盲区** | 每次 CPIN? 查询可能阻塞数秒甚至更长，整个主循环挂起 |
| SIM 掉卡后 `start_fail_ts` 为 0 | 不存在盲区 | ST_SIM 中直接设置，无需等到 Ping 失败 |
| SIM 掉卡后 `ping_fail_count` 清零 | 已修复 | 旧版会导致 L2/L3 永不触发；新版 `ping_fail_count=0` 配合 `sim_error_active=1` 正确处理 |
| SIM 掉卡但 `has_notified_connect=0` | 可能漏日志 | 若设备刚启动尚未 Ping 通就掉卡，不会打印 [ERROR] 日志，但 `start_fail_ts` 会设置 |

#### 能恢复吗？

- **能恢复**（SIM 硬件短暂断开后自动恢复）：新版 SIM 恢复即触发 L1，约 30s 内重新联网。
- **不能自动恢复**（SIM 永久失效/物理拔出）：`start_fail_ts` 持续计时，经 60s→5min→30min 依次触发 L1/L2/L3，L3 发送 `AT+CFUN=1,1` 重启整颗 SoC，然后由保活脚本重拉 dial 重新初始化。

### 3.5 与 L1/L2/L3 恢复的关联

```
SIM 掉卡场景下的恢复路径：

  掉卡时刻 (t=0):
    ├─ start_fail_ts = t
    ├─ ping_fail_count = 0
    └─ sim_error_active = 1

  掉卡期间 (SIM 持续非 READY):
    ├─ Ping 必然失败
    ├─ ping_fail_count++ → 达到 3 次后 start_fail_ts 确认（已在 ST_SIM 设置）
    ├─ t+60s：L1 触发（REG 状态判断：SIM 掉卡时 REG 可能为 0，当 REG=0 时 L1 被跳过！）
    ├─ t+90s：若 REG=0，L2 提前触发（新策略）
    └─ t+30min：L3 重启

  SIM 恢复时刻 (t=k):
    ├─ sim_error_active=0
    ├─ 强制 recovery_level=1，stage=ST_RECOVERY
    └─ L1 立即执行（stop→sleep(2)→start），约 5s 后重新联网

注意：SIM 掉卡时 REG 状态通常也丢失（REG=0），
     当 REG=0 时 L1 被跳过（新逻辑），需等 L2（90s）。
     但 SIM 恢复路径通过强制设置 recovery_level=1 绕过 REG 检查，
     不受 g_last_reg_stat 条件影响。
```

### 3.6 改进建议

1. **AT 端口超时保护（最高优先级）**：`serial_atcmd` 工具的超时时间未知。建议在调用链中增加超时控制：
   ```c
   // 建议方案：给 popen 的命令增加 timeout 包装
   FILE *fp = popen("timeout 3 serial_atcmd at+cpin? 2>/dev/null", "r");
   ```
   这样即使 AT 端口阻塞，3s 后强制超时，`get_cpin_status_str()` 返回 "UNKNOWN"，主循环不会挂起。

2. **SIM 长期掉卡时抑制 L1/L2 反复无效重试**：当 SIM 持续非 READY 超过 5 分钟时，L2（飞行模式）无法解决硬件问题，但仍会反复执行，徒增 AT 命令负担。建议：若 CPIN 持续非 READY 超过 L2 冷却期，跳过 L2，直接等 L3。

3. **掉卡后通知外部进程**：当前掉卡事件仅写日志，无 `/tmp` 状态文件反映。建议写 `/tmp/sim_status`（类似 `/tmp/network_status`），供外部监控程序感知。

4. **`sim_error_active` 标志初始化**：当前 `sim_error_active = 0` 只在 `dial_loop()` 局部变量中初始化。若 SIM 在 `dial_loop()` 入口前就已非 READY（如上电时 SIM 未稳定），`sim_error_active` 从未置 1，SIM 恢复时也不会打印恢复日志。建议在 `dial_loop()` 入口处查询一次 CPIN 状态，根据结果预设 `sim_error_active`。

### 3.7 SDK 自动重连能否替代应用层恢复（断卡场景）

> 背景：有观点认为检测到断卡后不做任何应用层处理，仅依赖 SDK 的 25s 自动重连即可恢复。本节基于代码事实进行推演。

#### SDK 重连的工作层次

```c
const int reconnect_interval = 25; // SDK 底层自动重连间隔
int time_list[2] = { reconnect_interval, 0 };
ql_data_call_param_set_reconnect_interval(p_cfg, time_list, 2);
```

SDK 自动重连工作在 **PDP/数据链路层**：感知 `DISCONNECTED` 事件，每 25 秒尝试重新建立 PDP 上下文。它不感知 SIM 层状态，不知道断连原因是 SIM 掉卡还是信号丢失。

#### SIM 断卡时的实际分层故障链

```
SIM 断卡（硬件/软件层）
  └─ SIM 接口下线 → 模组失去 IMSI/ICCID（身份丢失）
       └─ 网络注册丢失（REG=0）
            └─ PDP 上下文被强制拆除
                 └─ SDK 收到 DISCONNECTED → 启动 25s 重连计时
```

SDK 看到的只是最底层的 `DISCONNECTED`，完全不知道原因是 SIM 层故障。

#### 纯 SDK 重连的时序推演

```
t=0       SIM 断卡
t=~3s     CPIN? 检测到非 READY（假设 AT 端口未阻塞）
t=~3s     SDK 收到 DISCONNECTED，启动 25s 计时
t=~28s    SDK 第 1 次重连尝试
            ├─ SIM 还未恢复 → ql_data_call_start 返回 -1001 → 失败
            └─ REG=0 → 即使 SIM 恢复也可能尚未完成网络注册 → 失败
t=30s     SIM 硬件恢复
t=30s     模组重读 SIM，搜网注册（需 10~30s 不确定）
t=~53s    SDK 第 2 次重连尝试（28s + 25s）
            ├─ 若网络注册已完成 → 成功，共断联 ~53s
            └─ 若注册仍未完成 → 失败
t=~78s    SDK 第 3 次重连尝试 → 成功，共断联 ~78s
```

**核心问题**：SDK 是**固定节拍的盲重连**，完全不知道 SIM 什么时候恢复、网络什么时候注册完成，每次失败都要再白等 25 秒。

#### 与应用层 L1 对比

```
t=0       SIM 断卡
t=~3s     ST_SIM 检测到，sim_error_active=1，start_fail_ts 设置
t=30s     SIM 恢复，ST_SIM 检测到 CPIN=READY（下一个 ~3s 轮次）
t=~33s    强制触发 L1（stop→sleep(2)→start），不等 60s
t=~38s    重连成功（若网络注册已完成），共断联 ~38s
```

#### 结论

| 对比项 | 纯 SDK 重连 | 应用层 L1（当前方案）|
|---|---|---|
| 感知 SIM 恢复时机 | 不感知，固定 25s 节拍 | ST_SIM 检测 CPIN=READY 后立即触发 |
| REG=0 时重连效果 | 失败（-1001），浪费 25s | L1 跳过，90s 触发 L2（飞行模式） |
| 断联时长（SIM 掉 30s）| **50~100s，不确定** | **~38s，确定性** |
| 是否需要应用层介入 | 否 | 是 |

**结论**：纯 SDK 重连对 SIM 断卡场景效果差且不确定。当前应用层"SIM 恢复后立即强制 L1"是必要且正确的，SDK 重连只能作为兜底保险，不能替代主动恢复逻辑。

---

### 3.8 轮询 CPIN 的根本性设计缺陷与改进方向

#### 轮询 vs 事件驱动

当前 CPIN 检测方式是**主动轮询**（每 ~3100ms 问一次），而 SIM 掉卡是**瞬时事件**，二者存在天然的感知延迟。

EC200A/EG25 模组支持 `AT+QSIMSTAT=1` 开启 SIM 状态 URC（Unsolicited Result Code）主动上报：

```
# 启用 URC 上报（一次性配置）
AT+QSIMSTAT=1

# SIM 脱离时模组自动推送（无需轮询，零延迟）：
+QSIMSTAT: 0,0

# SIM 恢复时模组自动推送：
+QSIMSTAT: 0,1
```

当前代码中 `AT+QSIMSTAT` **仅用于断卡/恢复时的诊断快照**，从未用于实时检测。整个 SIM 状态变化感知完全依赖轮询。

#### popen 无超时导致的主循环阻塞风险

```c
void get_cpin_status_str(char *out_buf, int len) {
    FILE *fp = popen("serial_atcmd at+cpin?", "r");  // ← 无超时保护
    while (fgets(line, sizeof(line), fp)) { ... }    // ← 死等返回
    pclose(fp);                                        // ← serial_atcmd 不返回则永远阻塞
}
```

代码注释已明确指出"断卡时 AT 端口阻塞"。这意味着 SIM 掉卡的那一刻，AT 端口可能就卡住了。若状态机恰好轮到 ST_SIM 去查 CPIN，`popen` 阻塞，**主循环整个冻结**：

- Fast Fail 检测暂停
- 心跳日志静默
- L1/L2/L3 计时冻结
- 实际感知延迟 = `serial_atcmd` 工具自身的超时时间（可能是几秒到几十秒）

因此真实的断卡感知延迟**不是 3 秒，而是不可预测的**。

#### 理想改进方向

| 方案 | 实现复杂度 | 效果 |
|---|---|---|
| **方案 A（最小改动）**：给所有 `serial_atcmd` 调用加 `timeout 3` 包装 | 低 | 防止阻塞，感知延迟仍是 ~3s |
| **方案 B（推荐）**：使用 `AT+QSIMSTAT=1` URC，监听 `/dev/ttyUSB*` 串口推送 | 中 | 零延迟感知，从根本上解决问题 |
| **方案 C（理想）**：使用 Quectel SDK 的 SIM 事件回调（若 SDK 提供）| 中 | 与方案 B 等效，更贴近 SDK 架构 |

方案 A 的最小改动（立即可做）：
```c
// 将 get_cpin_status_str() 中的调用改为：
FILE *fp = popen("timeout 3 serial_atcmd at+cpin? 2>/dev/null", "r");
// 3s 内无响应返回 "UNKNOWN"，主循环不阻塞
```

---

## 四、L1/L2/L3 分级恢复逻辑完整分析

### 4.1 时间参数汇总表

| 常量/变量 | 值 | 说明 |
|---|---|---|
| `PING_INTERVAL_MS` | 1500 ms | Ping 检测间隔 |
| `SIM_INTERVAL_MS` | 800 ms | SIM 状态检测间隔 |
| `SIG_INTERVAL_MS` | 800 ms | 信号检测间隔（透传） |
| `HEARTBEAT_INTERVAL_MS` | 30000 ms | 基本心跳间隔 |
| `next_ext_heartbeat_ts` 间隔 | 300000 ms | 扩展心跳间隔（5分钟） |
| `LEVEL1_TIMEOUT` | 60,000 ms | 触发 L1 阈值（1分钟） |
| `LEVEL2_TIMEOUT` | 300,000 ms | 触发 L2 阈值（5分钟） |
| `LEVEL3_TIMEOUT` | 1,800,000 ms | 触发 L3 阈值（30分钟） |
| `FAST_FAIL_TIMEOUT_MS` | 10,000 ms | Fast Fail：PDP 后 Ping 窗口 |
| `PDP_WAIT_TIMEOUT_MS` | 60,000 ms | Fast Fail：PDP 建立最大等待 |
| `PING_FAIL_THRESHOLD` | 3 次 | 连续失败触发计时的阈值 |
| SDK `reconnect_interval` | 25 s | SDK 底层自动重连间隔 |
| L1 冷却期（`last_l1_ts`） | 60,000 ms | 两次 L1 最短间隔 |
| L2 冷却期（正常） | 300,000 ms（5分钟） | `last_recovery_ts` 检查 |
| L2 冷却期（REG=0 快速路径） | 90,000 ms（90s） | `g_last_reg_stat == 0` 时 |
| L2 触发时间（REG=0 快速路径） | 90 s | `fail_duration > 90 * 1000` |
| `MAX_CALLS`（CFUN 保护） | 10 次 | `restart_cfun()` 上限 |
| `MIN_INTERVAL`（CFUN 保护） | 600 s | 两次 CFUN 最短间隔 |

### 4.2 L1 软重拨逻辑

#### 触发条件（ST_PING 失败路径）

```c
else if (fail_duration > LEVEL1_TIMEOUT && g_last_reg_stat != 0) {
     if (tnow - last_l1_ts > (60 * 1000)) {
         recovery_level = 1;
     }
}
```

必须同时满足：
1. `fail_duration > 60,000ms`（连续失败超过 1 分钟）
2. `g_last_reg_stat != 0`（网络注册状态不是 "注册丢失"）
3. 距上次 L1 执行超过 60s（`last_l1_ts` 节流）

#### SIM 恢复强制路径

```c
// ST_SIM 中，SIM 从非 READY 恢复为 READY 时
recovery_level = 1;
stage = ST_RECOVERY;
break;
```

此路径**不受 `fail_duration`、`g_last_reg_stat`、`last_l1_ts` 等条件限制**，直接触发 L1。

#### L1 执行动作序列

```c
case 1:
{
    dial_log("[RECOVERY L1] Stopping Data Call & Cleaning PDP... REG=%d\n", g_last_reg_stat);
    ql_data_call_stop(g_call_id);
    sleep(2);
    dial_log("[RECOVERY L1] Restarting Data Call...\n");
    ret = ql_data_call_start(g_call_id);
    if (ret != 0) {
        // Start 失败时：只更新 last_l1_ts，不更新 last_recovery_ts
        // 保证 L2 冷却期检查能正常触发
        last_l1_ts = tnow;
        break;
    }
    // Start 成功：两个时间戳都更新
    last_recovery_ts = tnow;
    last_l1_ts = tnow;
    sleep(3);
    break;
}
```

#### L1 失败时的精细处理（新版 vs 旧版）

**旧版**：
```c
last_recovery_ts = tnow; // 统一在 ST_RECOVERY 入口更新
```
问题：L1 失败也会更新 `last_recovery_ts`，导致 L2 的冷却期重置，L2 无法按时触发。

**新版**：
- L1 **成功**：`last_recovery_ts = tnow`（L2/L3 冷却期重置，因为 L1 有效）
- L1 **失败**：`last_recovery_ts` **不更新**，只更新 `last_l1_ts`（只节流 L1 本身，不影响 L2 计时）

#### 跳过条件

当 `g_last_reg_stat == 0`（网络注册丢失）时：L1 被完全跳过，因为实测 REG=0 期间 `ql_data_call_start()` 返回 -1001（未就绪），软重拨无效，应直接等 L2 的飞行模式。

#### 潜在问题

- `g_last_reg_stat` 由 30s 心跳中的 `get_cereg_status_safe()` 更新，实时性有限（最坏情况 30s 延迟）。若注册状态在 30s 心跳间隙变化，L1 判断可能基于过时数据。
- `sleep(2)` 和 `sleep(3)` 阻塞主线程，期间心跳日志无法输出，造成 5s 静默。

### 4.3 L2 射频重置逻辑

#### 触发条件

```c
else if (fail_duration > LEVEL2_TIMEOUT ||
         (g_last_reg_stat == 0 && fail_duration > 90 * 1000)) {
     uint64_t l2_cooldown = (g_last_reg_stat == 0)
                            ? (90 * 1000)
                            : (5 * 60 * 1000);
     if (tnow - last_recovery_ts > l2_cooldown) {
         recovery_level = 2;
     }
}
```

两种触发路径：
1. **正常路径**：`fail_duration > 5min`，冷却期 5min
2. **REG=0 快速路径**：`g_last_reg_stat == 0` 且 `fail_duration > 90s`，冷却期 90s

**REG=0 快速路径的动机**（commit `3ff9f9e` 注释）：
> "实测两次 REG=0 事件（10:05 和 16:31），L1 连续 4 次均失败（-1001），L2 飞行模式切换（AT+CFUN=0/1）是使 CP 重新扫频注册的有效手段。"

#### L2 执行动作序列

```c
case 2:
    last_recovery_ts = tnow; // L2 无论成败都更新（防止频繁重置）
    last_l1_ts = tnow;       // L2 后 60s 内抑制 L1
    dial_log("[RECOVERY L2] Toggling RF (Airplane Mode)... REG=%d\n", g_last_reg_stat);
    system("serial_atcmd at+cfun=0"); // 关射频
    sleep(3);
    system("serial_atcmd at+cfun=1"); // 开射频
    dial_log("[RECOVERY L2] RF ON. Waiting 10s for registration...\n");
    sleep(10);
    dial_log("[RECOVERY L2] Restarting Data Call...\n");
    ret = ql_data_call_start(g_call_id);
    if (ret != 0) {
        dial_log("[RECOVERY L2] Start failed: %d\n", ret);
    }
    break;
```

**总耗时**：`sleep(3) + sleep(10) = 13s` 阻塞主线程，期间无心跳日志。

**L2 执行上限**：无单独计数器，但受 `last_recovery_ts` 冷却期隐性限制（正常路径每 5min 一次，L2 执行时间 ~13s，实际约每 5min13s 一次）。

#### 潜在问题

- `AT+CFUN=0/1` 是射频重置，不是模块重启，CP 协议栈重新初始化需要时间。10s 等待可能对某些信号质量差的场景不足（需要更长的搜网时间）。
- L2 期间 `sleep(13s)` 完全阻塞，无法响应 SIGINT 信号（`g_sigint_received` 检查在循环顶部，13s 内无法触达）。

### 4.4 L3 模块重启逻辑

**修改前**（基准版本）：
```c
case 3:
    dial_log("[RECOVERY L3] FATAL: Network down for 30mins. But Not REBOOTING MODULE...\n");
    //log_close();
    sync();
    //system("serial_atcmd at+cfun=1,1"); // ← 关键代码被注释
    sleep(5);
    //system("reboot");
    //exit(1);
    break;
```

**修改后**：
```c
case 3:
    dial_log("[RECOVERY L3] FATAL: Network down 30mins. Sending AT+CFUN=1,1 then exit.\n");
    sync();
    system("serial_atcmd at+cfun=1,1"); // ← 取消注释，实际执行
    sleep(20);
    dial_log("[RECOVERY L3] Exiting. start_prog will reinitialize.\n");
    log_close();
    exit(1);
    break;
```

**功能/行为层分析**：
- 旧版 L3 完全是空洞代码，30分钟断网后无任何恢复动作，进程继续运行但不做任何事，设备永久断线。
- 新版 L3 执行 `AT+CFUN=1,1`：在 EC200A（ASR1803 SoC）上，此命令触发整颗 SoC（AP+CP）重启，Linux 系统重启，保活脚本重新拉起 dial。这是最彻底的恢复手段。
- `sleep(20)` 等待 SoC 重启完成（AT+CFUN=1,1 后无 OK 响应，SoC 直接重启），然后 `exit(1)` 确保进程退出，让保活脚本检测到并重拉。

**EC200A 平台特殊性**：
- `AT+CFUN=1,1`：整颗 ASR1803 SoC 硬重置，AP（跑 Linux）和 CP（跑蜂窝协议栈）全部重启，bootloader 重走。
- `AT+CFUN=0/1`：仅 CP 射频关闭/开启，AP 和 Linux 不受影响。
- 两者不能混淆，L2 用 CFUN=0/1，L3 用 CFUN=1,1。

### 4.5 完整状态机流程图（文字版）

```
每轮循环（50ms usleep）：
│
├── [检查 SIGINT 标志] → 若收到 SIGINT：clean exit
│
├── [Fast Fail 检测] （仅前 3 次启动且未成功连接时）：
│    ├─ PDP 已建立：从 pdp_ready_ts 计 10s Ping 窗口 → 超时 exit(1)
│    └─ PDP 未建立：从 start_loop_ts 计 60s → 超时 exit(1)
│
├── [30s 心跳] （CPIN / CEREG / CSQ / TEMP / DownTime）→ 缓存 g_last_reg_stat
│
├── [5min 扩展心跳] （仅已连接后）（CESQ / CREG / CGPADDR）
│
└── [状态机 switch(stage)]：
     │
     ├─ ST_STATUS：立即 → ST_SIM
     │
     ├─ ST_SIM（800ms 间隔）：
     │    └─ get_cpin_status_str()
     │         ├─ == "READY" 且 sim_error_active：
     │         │    ├─ 打印恢复日志 + QSIMSTAT + dmesg
     │         │    ├─ sim_error_active = 0
     │         │    └─ 若 start_fail_ts != 0：
     │         │         recovery_level=1, stage=ST_RECOVERY → break
     │         │
     │         ├─ == "READY"（正常）：→ ST_SIGNAL
     │         │
     │         └─ != "READY"：
     │              ├─ 若 has_notified_connect：
     │              │    打印 [ERROR] + QSIMSTAT + dmesg + 清零计数
     │              │    sim_error_active = 1
     │              ├─ start_fail_ts = tnow（若为0）
     │              └─ → ST_SIGNAL
     │
     ├─ ST_SIGNAL（800ms 间隔）：透传 → ST_PING
     │
     ├─ ST_PING（1500ms 间隔）：
     │    └─ test_can_ping_google()
     │         │
     │         ├─ Ping 成功：
     │         │    ├─ ping_fail_count = 0
     │         │    ├─ start_fail_ts = 0, recovery_level = 0
     │         │    ├─ last_l1_ts = 0, diag_snap_done = 0
     │         │    ├─ clear_retry_count()
     │         │    ├─ is_fast_fail_mode = 0
     │         │    ├─ 若 !has_notified_connect：触发回调，has_notified_connect=1
     │         │    └─ → ST_STATUS
     │         │
     │         └─ Ping 失败：
     │              ├─ ping_fail_count++
     │              │
     │              ├─ 若 ping_fail_count < 3：→ ST_STATUS（抖动过滤）
     │              │
     │              └─ 若 ping_fail_count >= 3：
     │                   ├─ start_fail_ts = tnow（若为0，同时打 DIAG 快照）
     │                   ├─ fail_duration = tnow - start_fail_ts
     │                   │
     │                   └─ [分级判断]（若 enable_policy_recovery=1）：
     │                        ├─ fail_duration > 30min → recovery_level=3
     │                        │
     │                        ├─ fail_duration > 5min
     │                        │   OR (REG=0 AND fail_duration > 90s)
     │                        │   AND last_recovery_ts 冷却：→ recovery_level=2
     │                        │
     │                        ├─ fail_duration > 60s
     │                        │   AND REG != 0
     │                        │   AND last_l1_ts 冷却 60s：→ recovery_level=1
     │                        │
     │                        └─ 若 recovery_level > 0：
     │                             打印 [ALARM]，→ ST_RECOVERY
     │
     └─ ST_RECOVERY：
          ├─ 打印 [RECOVERY] LastErr (CEER) + PDP state (CGACT)
          ├─ switch(recovery_level)：
          │    ├─ 1：stop → sleep(2) → start
          │    │       start 成功：更新 last_recovery_ts, last_l1_ts
          │    │       start 失败：只更新 last_l1_ts
          │    ├─ 2：cfun=0 → sleep(3) → cfun=1 → sleep(10) → start
          │    │       更新 last_recovery_ts, last_l1_ts
          │    └─ 3：sync → cfun=1,1 → sleep(20) → log_close → exit(1)
          │
          ├─ has_notified_connect = 0
          └─ → ST_STATUS
```

### 4.6 分级恢复逻辑缺陷分析

| 场景 | 缺陷描述 | 严重程度 |
|---|---|---|
| L2 期间 sleep(13s) | 主线程阻塞，SIGINT 无响应，心跳 13s 静默 | 低（可接受） |
| L1 期间 sleep(5s) | 同上，约 5s 静默 | 低 |
| REG 状态延迟 30s | `g_last_reg_stat` 仅在心跳时更新，可能基于 30s 前的状态决策 | 中 |
| SIM 掉卡时 AT 端口阻塞 | `get_cpin_status_str()` 可能长时间阻塞，整个主循环挂起 | **高** |
| SIM 持续掉卡时 L2 无效 | L2 飞行模式无法解决硬件 SIM 故障，但仍会按冷却期重复执行 | 低 |
| CFUN=1,1 后 sleep(20) | SoC 重启完成时间不确定，可能 20s 不够，也可能远超 20s | 低（exit 兜底） |
| Ping 目标硬编码 8.8.8.8 | 若 Google DNS 不可达（如国内某些网络环境），Ping 始终失败 | 中 |

**SIM 掉卡场景下分级恢复的有效性**：
- L1（软重拨）：SIM 掉卡时 ql_data_call_start 返回 -1001，L1 无效。
- L2（飞行模式）：若 SIM 已恢复，L2 可帮助 CP 重新注册；若 SIM 仍断开，无效。
- L3（重启）：强制 SoC 重启，若 SIM 已恢复（瞬断类型），重启后可正常工作；若 SIM 永久失效，仍无效。
- **最优路径**：SIM 恢复后立即强制 L1（新版已实现），约 30s 恢复联网，无需等待 L1/L2/L3 常规计时。

---

## 五、其他模块变更分析

### 5.1 `data_call/data_call.c` 变更

#### 5.1.1 新增 `g_pdp_connected` 全局变量

**修改前**：无此变量，PDP 连接状态无法从回调线程传递到主循环。

**修改后**：
```c
// data_call.c
volatile int g_pdp_connected = 0;  /* 由 CONNECTED/DISCONNECTED 回调维护 */

// data_call.h
extern volatile int g_pdp_connected;

// CONNECTED 时：
g_pdp_connected = 1;

// DISCONNECTED 时：
g_pdp_connected = 0;
```

**功能/行为层分析**：dial.c 的 Fast Fail 机制和状态机均依赖此变量判断 PDP 是否建立。`volatile` 保证多线程（SDK 回调线程 + 主循环）间的可见性，在单核 ARM 平台上足够。

#### 5.1.2 `call_name` 过滤替代 `call_id` 过滤

**修改前**（注释掉的 call_id 过滤）：
```c
// if (call_id != DATA_CALL_ID_PUBLIC) { return; }
```

**修改后**：
```c
if (p_msg == NULL || strcmp(p_msg->call_name, DIAL_CALL_NAME) != 0) {
    return;
}
```

其中 `DIAL_CALL_NAME = "auto_network"` 定义在 `data_call.h`。

**功能/行为层分析**：SDK 重拨后 `call_id` 可能改变，旧的 call_id 过滤（被注释）会丢失事件。`call_name` 是创建时传入的字符串，进程生命周期内稳定，是正确的过滤依据。同时增加了 `p_msg == NULL` 的空指针保护。

#### 5.1.3 CONNECTED 时防御性预清理

**修改后**（在写路由之前）：
```c
// 防御性预清理：应对程序崩溃/重启后的路由残留
snprintf(cmd_buf, sizeof(cmd_buf), "ip route del default dev %s 2>/dev/null", p_msg->device);
system(cmd_buf);
snprintf(cmd_buf, sizeof(cmd_buf), "ip -6 route del default dev %s 2>/dev/null", p_msg->device);
system(cmd_buf);
snprintf(cmd_buf, sizeof(cmd_buf), "iptables -t nat -D POSTROUTING -o %s -j MASQUERADE 2>/dev/null", p_msg->device);
{
    int _k;
    for (_k = 0; _k < 10 && system(cmd_buf) == 0; _k++) { /* 清到干净 */ }
}
```

**功能/行为层分析**：每次 CONNECTED 时先清理可能残留的旧路由和 NAT 规则，解决了程序异常退出后重启，旧路由与新路由并存导致路由混乱的问题（commit `ffea5dd`：断网重连后必须重启系统才能联网的根因）。

#### 5.1.4 `ip ro` → `ip route`（标准化命令）

**修改前**：`ip ro add/del`

**修改后**：`ip route add/del`（IPv4）、`ip -6 route add/del`（IPv6）

纯格式规范化，功能相同。

#### 5.1.5 `iptables -t filter -F` 删除

**修改前**：
```c
system("iptables -t filter -F"); // CONNECTED 时清空 filter 表所有规则
```

**修改后**：此行删除，改为循环删除特定 MASQUERADE 规则。

**功能/行为层分析**：`-F` 清空所有 filter 表规则，包括用户自定义的防火墙规则（INPUT/FORWARD 等），破坏了系统安全策略（commit `72888c3` 修复）。新版只操作 nat 表的 MASQUERADE 规则，不触碰 filter 表。

#### 5.1.6 DNS 换行符修复（`\r\n` → `\n`）

**修改前**：`fprintf(fp, "nameserver %s\r\n", ...)` （CRLF）

**修改后**：`fprintf(fp, "nameserver %s\n", ...)` （LF）

**功能/行为层分析**：`resolv.conf` 是 Unix 格式文件，CRLF 行尾会被 `nameserver` 解析器误读为 `nameserver x.x.x.x\r`，`\r` 成为地址的一部分，导致 DNS 解析失败。

#### 5.1.7 DISCONNECTED 时路由/NAT 主动清理

**修改后**（DISCONNECTED 新增）：
```c
g_pdp_connected = 0;
// ...
if (g_if_name[0]) {
    char del_cmd[256];
    snprintf(del_cmd, ..., "ip route del default dev %s 2>/dev/null", g_if_name);
    system(del_cmd);
    snprintf(del_cmd, ..., "ip -6 route del default dev %s 2>/dev/null", g_if_name);
    system(del_cmd);
    snprintf(del_cmd, ..., "iptables -t nat -D POSTROUTING -o %s -j MASQUERADE 2>/dev/null", g_if_name);
    { int _k; for (_k = 0; _k < 10 && system(del_cmd) == 0; _k++) {} }
    g_if_name[0] = '\0'; // 清空接口名
}
unlink("/tmp/resolv_v4.conf");
unlink("/tmp/resolv_v6.conf");
```

**功能/行为层分析**：解决了断线后路由残留的问题。精确按 `g_if_name` 清理，不影响其他网卡（eth0/wlan0）的路由。DNS 临时文件也一并清理，防止下次仅有 IPv4 连接时，旧 IPv6 DNS 被错误合并入 `/etc/resolv.conf`。

#### 5.1.8 `flow_monitor_task` 首次调用修复

**修改后**：
```c
static int first_call = 1;
// ...
if (first_call) {
    first_call = 0;
    *dial_timer = cur_timer; // 首次只记录基准值
} else if ((if_rx_packets != u64_if_rx_packets) && ...) {
    *dial_timer = cur_timer;
}
```

**功能/行为层分析**：旧版首次调用时 `u64_if_rx_packets = 0`，实际 RX 数据包数量非 0，`if_rx_packets > u64_if_rx_packets` 为真，会误判为 "有流量更新"，重置计时器，可能影响断流检测准确性。新版首次调用只记录基准值，不做比较。

---

### 5.2 `misc.c` 变更

#### 5.2.1 `executeATCommand` 内存泄漏修复

**修改前**：
```c
char *result = malloc(1);
// ...
if (!pipe) {
    fprintf(stderr, "Couldn't start command.\n");
    return NULL; // ← malloc(1) 的内存泄漏！
}
```

**修改后**：
```c
if (!pipe) {
    fprintf(stderr, "Couldn't start command.\n");
    free(result); // ← 修复泄漏
    return NULL;
}
```

#### 5.2.2 `realloc` 安全性修复

**修改前**：
```c
result = realloc(result, new_length + 1);
strcat(result, buffer);
```

**修改后**：
```c
char *tmp = realloc(result, new_length + 1);
if (!tmp) {
    free(result);
    pclose(pipe);
    return NULL;
}
result = tmp;
strcat(result, buffer);
```

**功能/行为层分析**：旧版直接 `result = realloc(result, ...)`，若 `realloc` 返回 NULL（内存不足），旧的 `result` 指针被覆盖为 NULL，原内存泄漏。新版使用临时指针 `tmp`，失败时正确释放。

#### 5.2.3 `restartNetworkServices()` 双重 fork 防僵尸进程

**修改前**（单层 fork）：
```c
pid1 = fork();
if (pid1 == 0) {
    execl("/usr/bin/ql_rild", "ql_rild", NULL);
    _exit(1);
}
// ← 父进程没有 waitpid，子进程成为僵尸
```

**修改后**（双重 fork）：
```c
pid1 = fork();
if (pid1 == 0) { // 中间进程
    if (fork() == 0) { // 孙子进程，被 init(PID=1) 收养
        execl("/usr/bin/ql_rild", "ql_rild", NULL);
        _exit(1);
    }
    _exit(0); // 中间进程立即退出
}
if (pid1 > 0) {
    waitpid(pid1, NULL, 0); // 回收中间进程，无僵尸
}
```

**功能/行为层分析**：中间进程立即 `_exit(0)`，孙子进程因父进程退出而被 init(PID=1) 收养。主进程 `waitpid(pid1)` 回收中间进程，不会产生僵尸。孙子进程继续运行 `ql_rild`，init 负责回收其资源。此方案无需修改全局 SIGCHLD 处理器，不干扰其他地方的 `waitpid` 调用。

#### 5.2.4 `check_process()` 参数校验和精确匹配

**修改前**：
```c
int check_process(const char *process_name) {
    char command[256];
    snprintf(command, sizeof(command), "pgrep %s | wc -l", process_name);
    // ...
}
```

**修改后**：
```c
int check_process(const char *process_name) {
    if (process_name == NULL || strlen(process_name) == 0 || strlen(process_name) > 64) {
        fprintf(stderr, "check_process: invalid process_name\n");
        return -1;
    }
    char command[256];
    snprintf(command, sizeof(command), "pgrep -x %s | wc -l", process_name);
    // ...
}
```

**功能/行为层分析**：
1. 参数校验防止 NULL 解引用和超长字符串溢出 command 缓冲区。
2. `pgrep -x`（精确匹配进程名）：旧版 `pgrep dial` 会匹配 `dialog`、`tmdial` 等含 "dial" 子串的进程，导致单实例检查误判。`-x` 标志要求进程名完全等于参数。

---

### 5.3 `nw/nw.c` 变更

#### 5.3.1 `nw_get_if_statistics_rx_packets()` 越界防护

**修改后**：
```c
file_size = lseek(if_statistics_rx_packets_fd, 0L, SEEK_END);
lseek(if_statistics_rx_packets_fd, 0L, SEEK_SET);
if (file_size <= 0 || file_size >= (int)sizeof(val_bytes)) {
    close(if_statistics_rx_packets_fd);
    return ret;
}
read(if_statistics_rx_packets_fd, val_bytes, file_size);
```

**功能/行为层分析**：若 sysfs 文件大小异常（0 字节或超过 `val_bytes` 缓冲区），旧版 `read()` 会溢出或读到空数据。新版增加边界检查，安全返回失败。

#### 5.3.2 `nw_mark_network_status()` O_TRUNC 和错误处理

**修改前**：
```c
fd = open(NW_STATUS_PATH, O_WRONLY | O_CREAT);
// ← 无 O_TRUNC，重写只覆盖部分内容（如从"0"写"1"，文件变为"1"正常；但从"10"写"0"，文件变为"00"）
```

**修改后**：
```c
fd = open(NW_STATUS_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
if (fd < 0) {
    perror("open NW_STATUS_PATH failed");
    return;
}
```

**功能/行为层分析**：`O_TRUNC` 保证每次写入前清空文件，防止旧内容残留。同时指定文件权限 `0644`（旧版 `O_CREAT` 没有权限参数，权限未定义），增加 `fd < 0` 的错误处理。

---

### 5.4 `apn/apn.c` 变更

#### 5.4.1 内存安全全面加固

| 原代码 | 修改后 | 问题 |
|---|---|---|
| `str = (char *)malloc(len)` | `str = (char *)malloc(len + 1)` | 未留 `\0` 的空间 |
| `fread(str, 1, len, fp) < 0` | `fread(str, 1, len, fp) != (size_t)len` | fread 返回 size_t，不能与 0 比负数 |
| `str` 无 null 终止 | `str[len] = '\0'` | json_tokener_parse 需要 null 终止字符串 |
| `p_apn_obj = malloc(...)` 无检查 | 添加 NULL 检查 + 资源清理 | malloc 失败不报错 |
| `memcpy(dst, src, strlen(src))` | `strncpy(dst, src, sizeof(dst)-1); dst[...]='\0'` | 若 src 超过 dst 则溢出 |
| `json_object_put(json_temp)` | 删除（borrowed ref 不需要 put） | 使用 borrowed reference 后 put 导致 use-after-free |
| 根对象不 put | `json_object_put(obj)` | 内存泄漏（之前因误用 put 而注释掉）|

#### 5.4.2 APN 匹配从前缀改为精确匹配

**修改前**：
```c
if (0 == strncmp(p_apn_obj[i].iccid, iccid, strlen(p_apn_obj[i].iccid)))
```

**修改后**：
```c
if (0 == strcmp(p_apn_obj[i].iccid, iccid))
```

**功能/行为层分析**：旧版使用 `strncmp(a, b, len_a)` 是前缀匹配，若 JSON 中 ICCID 为 "8986" 而实际 ICCID 为 "898600..."，会错误匹配。新版精确比较，消除 ICCID 前缀重叠时的误匹配风险。

#### 5.4.3 `free(p_apn_obj)` 确保释放

所有可能的 `return -1` 路径均添加了 `free(p_apn_obj)` 以防止内存泄漏，函数末尾也无条件释放。

---

### 5.5 `dial.h` 变更

**修改前**：
```c
#include "sim.h"
#include "_public.h"
```

**修改后**（删除上述两行，新增）：
```c
#include "ql-sdk/ql_nw.h"
```

**功能/行为层分析**：`dial.h` 中删除了对 `sim.h` 和 `_public.h` 的包含（它们通过其他路径已被引入，避免重复包含），直接引入 `ql_nw.h`（为 `dial.h` 中使用的 SDK 类型提供声明）。

---

## 六、总结与建议

### 6.1 本次迭代改动的核心目标

本次 17 个提交可归纳为三个层次的目标：

1. **消除已知稳定性 Bug**（前 7 个 commit，2026-04-09 至 04-21）：
   - 路由残留导致必须重启才能联网（根本原因：DISCONNECTED 未清路由）
   - Fast Retry 误判（PDP 建立时间被算入 10s 窗口）
   - call_id 过滤丢事件（SDK 重拨后 call_id 变化）
   - L3 恢复代码被注释（30分钟后无任何动作）
   - Ping 抖动误触发（单次失败即启动故障计时）
   - persistent 模式下 retry_count 不清零

2. **缩短 eSIM 瞬断恢复时长**（中期 commit，2026-04-29 至 05-05）：
   - SIM 恢复后立即强制 L1，断联时长从 ~70s → ~30s
   - REG=0 时 L2 在 90s 提前触发，跳过无效的 L1 重试

3. **增强可观测性，支持远程诊断**（最近 commit，2026-05-05 至 05-08）：
   - 启动时打印完整设备档案（IMEI/ICCID/FW/IMSI/运营商）
   - 故障时打印 DIAG 快照（RSRP/RSRQ/CID/IP/温度/CEER/CGACT）
   - SIM 断卡时抓取 QSIMSTAT/CEREG/dmesg 快照（在恢复时刻获取有效数据）
   - 5分钟扩展心跳（信号质量趋势 + 小区变化 + IP 漂移）
   - 每 30s 心跳新增温度字段

### 6.2 改动后稳定性/可靠性的提升点

| 改进项 | 改进前 | 改进后 |
|---|---|---|
| 路由管理 | 断网重连后路由残留，必须手动重启 | DISCONNECTED 时精确清理，无残留 |
| Fast Retry | PDP 建立时间耗尽 10s 窗口，正常拨号被误判退出 | 60s PDP 等待 + 10s Ping 窗口，分阶段超时 |
| L3 恢复 | 代码注释，30分钟后无任何动作 | 实际执行 AT+CFUN=1,1 + exit，触发 SoC 重启 |
| eSIM 瞬断恢复 | 需等 Ping 超时（3次）+ 60s L1 ≈ 70s | SIM 恢复立即 L1，约 30s 恢复联网 |
| REG=0 场景 | L1 无效（-1001）但仍重复执行 | 跳过 L1，90s 快速触发 L2 |
| DNS 格式 | CRLF 导致 DNS 解析失败 | 改为 LF，DNS 正常工作 |
| Ping 抖动 | 单次抖动误触发 L1 | 连续 3 次失败才触发，抗抖动 |
| 防火墙 | iptables -F 清空所有规则 | 仅循环清除特定 MASQUERADE 规则 |
| 僵尸进程 | fork 后子进程成僵尸 | 双重 fork，init 收养孙子进程 |
| 信号处理 | free() 在信号处理函数内（UB） | 仅设标志位，主循环安全清理 |

### 6.3 仍存在的潜在问题

| 问题 | 严重程度 | 说明 |
|---|---|---|
| **AT 端口阻塞导致主循环挂起** | **高** | eSIM 掉卡时 `serial_atcmd at+cpin?` 无超时保护，可能永久阻塞，整个主循环冻结（Fast Fail/心跳/L1/L2/L3 全部暂停） |
| **CPIN 实际检测间隔被误解** | **中** | `SIM_INTERVAL_MS=800` 具有误导性，真实检测周期约 3100ms（受 `next_ts` 共享机制支配），比预期慢 3.8 倍 |
| **轮询方式感知延迟不确定** | **中** | AT 端口阻塞时实际感知时间远超 3s，不适合需要快速响应的 SIM 掉卡场景，应改用 URC 推送（`AT+QSIMSTAT=1`） |
| **SDK 重连不能替代应用层恢复** | **中** | SDK 25s 固定节拍盲重连不感知 SIM 恢复时机，断联时长 50~100s 不确定；应用层 L1 可将其压缩到 ~38s 确定性恢复 |
| L1/L2 期间 sleep 阻塞 | 中 | L1 sleep 5s、L2 sleep 13s 内无法响应 SIGINT，心跳静默 |
| `g_last_reg_stat` 最大 30s 过期 | 中 | REG 状态由心跳缓存，最坏情况 30s 后才更新，可能基于过期数据决策 |
| Ping 目标硬编码 8.8.8.8 | 中 | Google DNS 不可达时（部分专网环境），Ping 始终失败，触发虚假故障恢复 |
| 断开后 `/etc/resolv.conf` 不清空 | 低 | 断网后 `/etc/resolv.conf` 仍保留旧 DNS，只删了临时文件 |
| `flow_monitor_task` 未集成主流程 | 低 | RX 流量监测函数存在但从未被调用 |
| SIM 掉卡后 has_notified_connect=0 时不打 [ERROR] 日志 | 低 | 启动后未曾联网的掉卡事件无错误日志 |

### 6.4 优先级最高的改进建议（TOP 3）

#### TOP 1：AT 命令执行超时保护（最高优先级，立即可做）

**问题**：eSIM 掉卡时 AT 端口阻塞，`popen("serial_atcmd at+cpin?")` 无超时保护，可能永久阻塞，主循环整体冻结（Fast Fail/心跳/分级恢复全部暂停），实际感知延迟不可控。

**建议方案**：在 `get_cpin_status_str()` 及所有 AT 诊断函数的 `popen` 调用中增加 `timeout` 包装：

```c
// 修改前：
FILE *fp = popen("serial_atcmd at+cpin?", "r");

// 修改后：
FILE *fp = popen("timeout 3 serial_atcmd at+cpin? 2>/dev/null", "r");
```

3s 超时足够 AT 命令正常响应（通常 <1s），同时防止阻塞。也可以在 `serial_atcmd` 工具层面统一添加超时参数，覆盖所有调用点。

#### TOP 1.5（并列）：用 URC 替代轮询 CPIN（根本性改进）

**问题**：轮询方式存在三重缺陷：①实际检测间隔 ~3100ms 不是预期的 800ms；②AT 端口阻塞时感知时间不可控；③感知本质上是被动的，无法做到零延迟。

**建议方案**：启用 `AT+QSIMSTAT=1` URC，在串口接收线程（或独立线程）监听主动推送：

```
AT+QSIMSTAT=1              # 启动时发送一次，开启 URC 功能

# 模组自动推送（无需轮询）：
+QSIMSTAT: 0,0             # SIM 脱离
+QSIMSTAT: 0,1             # SIM 插入/恢复
```

实现方式：
1. 在 `dial_loop()` 初始化阶段发送 `AT+QSIMSTAT=1`
2. 在 SDK 或串口读取线程中解析 URC，设置 `volatile int g_sim_removed = 1/0`
3. 主循环中读取此标志，替代 ST_SIM 的 CPIN 轮询

此方案可将 SIM 掉卡感知延迟从 ~3100ms 缩短至 **<100ms**，且完全不受 AT 端口阻塞影响（URC 是模组主动推送）。

#### TOP 2：Ping 多目标支持，消除 8.8.8.8 单点依赖

**问题**：硬编码 `8.8.8.8`，在 Google DNS 不可达的场景（国内专网、企业内网、某些运营商策略屏蔽）中永远 Ping 失败，触发虚假的 L1→L2→L3 恢复循环，最终无限重启。

**建议方案**：

```c
// 增加备选 Ping 目标，任一成功即为网络正常
static const char *PING_TARGETS[] = {
    "8.8.8.8",       // Google DNS（国际）
    "114.114.114.114", // 移动/国内通用
    "223.5.5.5",     // 阿里 DNS
    NULL
};

bool test_can_ping_any(const char *interface_name) {
    for (int i = 0; PING_TARGETS[i]; i++) {
        if (test_can_ping_target(PING_TARGETS[i], interface_name)) return true;
    }
    return false;
}
```

理想情况下从配置文件读取 Ping 目标列表。

#### TOP 3：L2 冷却期与触发逻辑的 `g_last_reg_stat` 实时化

**问题**：`g_last_reg_stat` 在 30s 心跳中更新，最坏情况下 L1/L2 触发时使用的是 30s 前的注册状态，导致 REG=0 的 L2 快速路径可能基于过时数据触发（或错过触发）。

**建议方案**：在 ST_PING 失败后进入分级判断前，实时查询一次 CEREG：

```c
// 在 ping_fail_count >= PING_FAIL_THRESHOLD 后、分级判断前：
int fresh_reg = get_cereg_status_safe(); // 最新注册状态
if (fresh_reg >= 0) g_last_reg_stat = fresh_reg;
// 然后再用 g_last_reg_stat 做分级判断
```

此调用仅在故障路径中执行（正常 Ping 成功时不触发），AT 命令负担可接受。

---

*本报告基于 `git diff 6954a787..HEAD` 全量代码对比及 HEAD 版本 dial.c 完整阅读生成，所有结论有代码行号或 diff 片段为依据。*

*分析范围：dial.c（~1800行）/ data_call/data_call.c / misc.c / nw/nw.c / nw/nw.h / apn/apn.c / data_call/data_call.h / dial.h*
