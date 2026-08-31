# 方案A v2 源码逐行分析（综合版）

> **版本**：dial v1.29.07  
> **综合来源**：方案A_v2_源码逐行分析B + open_dial_roamlink_analysis_v2 + SIM_Roamlink_切换逻辑深度分析  
> **优先级**：源码实际行为 > 补充文档v2 > 切换逻辑深度分析 > 基础逐行分析  
> 本文档对每一行（或每一段）代码的功能、原因、注意事项做详细说明，代码能力不强的读者也能完全看懂。

---

# 第一部分：main.c（程序入口）

main.c 是整个程序的启动入口，负责三件事：初始化日志、启动状态机线程、在主线程循环查询信号强度。

---

## 头部：版本号定义

```c
#define MAIN_VERSION 1
#define SUB_VERSION 29
#define TEST_VERSION 7
```

**作用**：定义三段版本号。程序启动时会拼成 `1.29.07` 打印到日志，也写入 `/tmp/dial_status` 的 `version` 字段。  
**为什么分三段**：主版本号很少变；子版本号每次发布新功能时递增；测试版本号用于小修小改。目前是 1.29.07。

---

## 全局互斥锁

```c
pthread_mutex_t g_at_port_mutex = PTHREAD_MUTEX_INITIALIZER;
```

**作用**：AT 串口的全局互斥锁。  
**原因**：程序有两个线程——主线程每5秒查信号强度（发 `AT+CSQ`），dial_task 线程发各种 AT 命令（`AT+COPS=0`、`AT+CEREG?` 等）。如果两个线程同时往串口写，指令会混在一起，模组会返回乱码。这把锁确保同一时刻只有一个线程在用串口。  
**用法**：每次发 AT 命令前 `pthread_mutex_lock`，发完后 `pthread_mutex_unlock`。

> **补充分析（来自v2分析文档）**：
> 
> 主线程对 `p_dial_mng->csq` 的写入与 dial_task 线程对其的读取之间存在数据竞争（无锁保护）。不过 `uint8_t` 的读写在 ARM32 上是原子操作，实际不会导致问题，但从代码规范来说应加锁或改为原子变量。
> 
> AT 串口路径：`/dev/smd8`（SMD = Shared Memory Device，高通平台共享内存设备），通过 `open()/read()/write()` 像普通串口一样操作。`O_NONBLOCK | O_NOCTTY` 标志分别用于非阻塞读写和防止将该设备设为控制终端。

---

## 日志初始化函数

```c
void seas_log_config_init()
{
    seas_log_config_t log_config;
    log_config.level = SEAS_LEVEL_INFO;
    log_config.file = "/media/sdcard/seas_log_dial.log";
```

**作用**：设置日志级别和日志文件路径。  
`SEAS_LEVEL_INFO` 表示只记录 INFO 及以上级别的日志（DEBUG 级别的不记录）。  
日志文件路径是 SD 卡上的 `seas_log_dial.log`。

```c
    int has_sdcard_and_free_1GB = 0, i;
    unsigned long long free_bytes = 0;
    for (i = 0; i < 10; ++i) {
```

**作用**：最多等10秒（循环10次，每次sleep 1秒），等 SD 卡挂载完成。  
**原因**：程序开机启动很早，SD 卡可能还没挂载好，需要轮询等待。

```c
        if (stat("/media/sdcard", &sdstat) == 0 && S_ISDIR(sdstat.st_mode)) {
```

**作用**：检查 `/media/sdcard` 路径存在且是目录（即 SD 卡已挂载）。  
`stat()` 获取文件/目录信息；`S_ISDIR` 判断是否为目录类型。

```c
            struct statvfs vfs_chk;
            if (statvfs("/media/sdcard", &vfs_chk) == 0) {
                free_bytes = (unsigned long long)vfs_chk.f_bavail * vfs_chk.f_frsize;
```

**作用**：计算 SD 卡剩余可用空间（字节数）。  
`f_bavail` 是非特权用户可用的块数，`f_frsize` 是每块大小，两者相乘得到字节数。

```c
                if (free_bytes >= (unsigned long long)1024 * 1024 * 1024) {
                    has_sdcard_and_free_1GB = 1;
                    break;
                }
```

**作用**：只有剩余空间 >= 1GB 才启用文件日志。  
**原因**：日志文件最大50MB，如果 SD 卡快满了写日志会失败，提前检查避免问题。

```c
    if(has_sdcard_and_free_1GB){
        log_config.size = 50 * 1024 * 1024;   // 50MB
    } else {
        log_config.size = 0;                   // 不写文件
    }
    seas_log_config(&log_config);
```

**作用**：有 SD 卡且空间足够就写文件日志（最大50MB），否则 size=0 表示不写文件（日志只输出到控制台）。

> **补充分析（来自v2分析文档）**：
> 
> `statvfs.f_bavail` 是对**非root用户**可用的块数（`f_bfree` 包含内核保留块），因此计算的是普通进程实际可写入的剩余空间，更保守也更准确。
> 
> 日志轮转机制：`seas_log.c` 中当文件大小 ≥ `max_file_size` 时以 `"w+"` 模式重新打开（截断后从头写），属于**简单覆写**而非真正的循环轮转（不保留 `.log.1` 等旧文件），最多保存最新 50MB 的日志记录。

---

## main 函数

```c
int main(int argc, char **argv)
{
    pthread_t newthread;
    int csq;
    seas_log_config_init();
```

**作用**：先初始化日志，确保后续日志能正常输出。

```c
    SEAS_LOG_INFO("DIAL Version: %d.%02d.%02d\r\n", MAIN_VERSION, SUB_VERSION, TEST_VERSION);
```

**作用**：在日志里打印版本号，便于排查问题时确认当前跑的是哪个版本。  
`%02d` 表示至少两位数字，不足补零，所以是 `1.29.07` 而不是 `1.29.7`。

```c
    clock_gettime(CLOCK_MONOTONIC, &g_dial_start_time);
```

**作用**：记录程序启动的精确时刻（单调时钟）。  
**为什么用 CLOCK_MONOTONIC**：系统时间（`CLOCK_REALTIME`）可能被 NTP 校准而跳变，`CLOCK_MONOTONIC` 只会单调递增，不受系统时间调整影响，适合计算"运行了多少秒"。  
这个时间点后来用来计算 `/tmp/dial_status` 里的 `uptime` 字段。

```c
    snprintf(g_dial_version_str, sizeof(g_dial_version_str),
             "%d.%02d.%02d", MAIN_VERSION, SUB_VERSION, TEST_VERSION);
```

**作用**：把版本号格式化成字符串（如 `"1.29.07"`）存到全局变量，供 `dial_status_update()` 写入状态文件用。

```c
    dial_mng_t *p_dial_mng = dial_mng_new();
```

**作用**：创建并初始化状态机管理结构体。这一步会读配置、探测 Roamlink 状态、初始化所有字段。详见 dial.c 分析。

```c
    pthread_create(&newthread, NULL, dial_task, (void *)p_dial_mng);
```

**作用**：创建一个新线程，专门跑状态机（`dial_task` 函数）。  
- `newthread`：线程句柄，后面没用到（没有 join）
- `NULL`：使用默认线程属性
- `dial_task`：线程入口函数
- `(void *)p_dial_mng`：传给 dial_task 的参数（状态机结构体指针）

**为什么单独开线程**：dial_task 是一个无限循环，如果放在主线程里就无法同时做其他事。单独开线程后，主线程可以继续循环查信号强度。

```c
    while (true)
    {
        csq = nw_at_get_csq(p_dial_mng->smd_fd);
        if (csq != -1)
        {
            SEAS_LOG_INFO("CSQ: %d\r\n", p_dial_mng->csq);
            p_dial_mng->csq = (csq == DIAL_CSQ_INVALID) ? 0 : (uint8_t)csq;
        }
        sleep(5);
    }
```

**作用**：主线程的工作——每5秒查一次信号强度，更新到 `p_dial_mng->csq`。  
- `nw_at_get_csq()` 发 `AT+CSQ` 命令，返回 0~31 的信号值，或 99（无效），或 -1（AT 命令失败）
- `csq == DIAL_CSQ_INVALID`（即99）时存 0，表示信号无效
- 这个 csq 值会被 `dial_status_update()` 读取并写入 `/tmp/dial_status` 的 `[sim] csq` 字段

> **补充分析（来自v2分析文档）**：
> 
> 两线程职责总结：
> - **主线程**：仅执行 `nw_at_get_csq()` 轮询，每5秒更新 `p_dial_mng->csq` 和 `/tmp/network_csq`
> - **dial_task 线程**：承担完整的连接生命周期——状态机、TCP 测试、通道切换、状态输出
> - 两线程共享 AT 串口 fd（`smd_fd`），以 `g_at_port_mutex` 互斥保护
> 
> AT+COPS=0 超时5秒，AT+CSQ 超时1秒：切换时若 dial_task 持有锁发 COPS=0，主线程 CSQ 最多等待5秒；反之最多等待1秒。这是已知设计权衡，只影响 CSQ 更新延迟，不影响功能。

---

# 第二部分：dial.h（核心数据结构定义）

dial.h 定义了状态机用到的所有"蓝图"——常量、枚举、结构体。读懂它就能理解状态机的所有参数和状态。

---

## 超时常量

```c
#define SIM_CHECK_TIMEOUT_SECONDS (3600)
```
**作用**：SIM 卡检测超时1小时。如果1小时内都没检测到 SIM 卡，就重置模组重试。这个值很大，实际上 SIM 卡检测失败几秒内就会重试，只是用1小时作为最终保底。

```c
#define DIAL_TIMEOUT_SECONDS (60)
```
**作用**：`dial_stat_net_connected` 状态下，rx_packets（收包数）连续60秒无增长，判定连接僵死，跳回 `reg_check` 重新拨号。

```c
#define DIAL_CSQ_THREADHOLD (20)
#define DIAL_CSQ_INVALID (99)
```
**作用**：信号强度相关常量。`99` 是模组返回"无信号/未知"的特殊值；`20` 是一个信号强度阈值（20对应约-73dBm，4G能用的最低线），目前代码里没有强制用这个阈值触发任何动作，只是定义备用。

---

## TCP 测试参数

```c
#define TCP_TEST_INTERVAL_SECONDS (60)
```
**作用**：SIM 通道处于 `net_connected` 状态时，每60秒做一次 TCP 连通性测试。

```c
#define TCP_FAIL_THRESHOLD (3)
```
**作用**：连续3次 TCP 测试失败才触发通道切换。  
**原因**：网络偶尔抖动会导致单次测试失败，设3次阈值避免误切换。3次 × 60秒间隔 = 最快180秒才会切换。

---

## Roamlink 通道参数

```c
#define ROAMLINK_CONNECT_WAIT_SEC (300)
```
**作用**：发出 `RBstartServiceMaster` 后，等待 Roamlink 连通的最长时间。300秒内 TCP 测试一直不通则判定失败。

```c
#define ROAMLINK_CHECK_INTERVAL_SEC (30)
```
**作用**：Roamlink 处于 `roamlink_active` 状态时，每30秒做一次 TCP 测试（比 SIM 通道的60秒更频繁）。

```c
#define ROAMLINK_FAIL_THRESHOLD (3)
```
**作用**：Roamlink 通道连续3次 TCP 失败才触发切换。

```c
#define ROAMLINK_NO_DATA_TIMEOUT_SEC (120)
```
**作用**：业务层检测超时。Roamlink TCP 测试通过，但所有 `rmnet_data*` 接口的收包数连续120秒不增长，判定业务层死亡。

---

## 回切等待时间

```c
#define SIM_FALLBACK_RETRY_SEC      (300)
#define ROAMLINK_FALLBACK_RETRY_SEC (300)
```
**作用**：  
- `SIM_FALLBACK_RETRY_SEC`：策略1下，从 Roamlink 切到 SIM 备用后，等300秒稳定运行，再尝试回切 Roamlink。  
- `ROAMLINK_FALLBACK_RETRY_SEC`：策略2下，从 SIM 切到 Roamlink 备用后，等300秒稳定运行，再尝试回切 SIM。  
**原因**：如果刚切过来就立刻回切，网络还没稳定，容易造成频繁振荡。等300秒确保当前通道稳定后再回切。

> **补充分析（来自v2分析文档）**：
> 
> 其他关键常量（roamlink.h / dial.h）：
> 
> | 常量 | 值 | 说明 |
> |---|---|---|
> | `LICENSE_WAIT_TIMEOUT_SEC` | **300s ⚠️** | License 等待超时——**生产前必须改为 7200（2小时）** |
> | `LICENSE_CHECK_INTERVAL_SEC` | 60s | License 文件轮询间隔 |
> | `ROAMLINK_BIZ_LOG_INTERVAL_SEC` | 3600s | "Roamlink biz OK" 日志节流（每小时最多一条，防止日志刷屏） |
> | `AT_RETRY_NB` | 2 | AT 命令最大重试次数（失败最多重试2次） |
> | `NET_POLICY_DEFAULT` | 4 | 默认策略：FORCE_SIM |
> | `NW_TCP_TEST_HOST` | `"18.196.0.17"` | TCP 测试目标 IP——**单点故障风险**：此 IP 不可达时两通道都误判失败并反复切换 |
> | `NW_TCP_TEST_PORT` | 22 | TCP 测试目标端口（SSH 端口，只用来建连，不实际登录） |
> | `ROAMLINK_CTRL_PORT` | 5568 | RBMaster 控制 TCP 端口 |
> 
> `AUTO_REDIAL` 宏（定义于 dial.h）：定义后 `QL_Data_Call_Start` 的 `reconnect=true`，让模组在断线时自动重连（底层自动重拨），减少上层重试开销。

---

## 状态枚举

```c
typedef enum
{
    dial_stat_none,               // 0: 刚启动，决定走哪条路
    dial_stat_init,               // 1: 初始化 QL_MCM_NW 客户端
    dial_stat_sim_init,           // 2: 初始化 SIM 操作句柄
    dial_stat_sim_check,          // 3: 检查 SIM 卡是否在位、就绪
    dial_stat_sim_op,             // 4: 读取 SIM 卡 ICCID
    dial_stat_reg_check,          // 5: 检查是否注册到数据网络
    dial_stat_cereg_check,        // 6: 检查 LTE 注册状态（AT+CEREG?）
    dial_stat_precondition_check, // 7: 检查数据呼叫前置条件
    dial_stat_pre_start_call,     // 8: 根据 ICCID 查询 APN，设置拨号参数
    dial_stat_start_call,         // 9: 发起数据呼叫（QL_Data_Call_Start）
    dial_stat_stop_call,          // 10: 停止数据呼叫（未在主流程使用）
    dial_stat_stop_cfun,          // 11: 发 AT+CFUN=0 关闭无线功能（运营商切换时用）
    dial_stat_start_cfun,         // 12: 发 AT+CFUN=1 恢复无线功能
    dial_stat_list_oper,          // 13: 查询可用运营商列表（AT+COPS=?）
    dial_stat_select_oper,        // 14: 手动选择运营商（AT+COPS=1,2,xxx）
    dial_stat_wait_for_connect,   // 15: 等待数据连接建立（IP 分配）
    dial_stat_net_connected,      // 16: SIM 通道已联网，周期性 TCP 测试
    dial_stat_roamlink_starting,  // 17: Roamlink 服务已启动，等待连通
    dial_stat_roamlink_active,    // 18: Roamlink 通道联网，周期性监控
} dial_stat_enu;
```

**状态流转说明**：  
- 正常 SIM 拨号路径：`none → init → sim_init → sim_check → sim_op → reg_check → cereg_check → precondition_check → pre_start_call → start_call → wait_for_connect → net_connected`
- 正常 Roamlink 路径：`none → roamlink_starting → roamlink_active`
- `stop_cfun / start_cfun / list_oper / select_oper` 是特殊运营商手动选择流程，只在 APN 配置需要手动选运营商时触发。

> **补充分析（来自切换逻辑深度分析）**：
> 
> 三个核心监控状态的对比：
> 
> | 状态 | TCP 测试间隔 | 失败3次切换耗时 | 额外保活机制 |
> |---|---|---|---|
> | `net_connected`（SIM监控） | 60s | 最快180s | rx_packets 60s无增长→重拨 |
> | `roamlink_starting`（等待连通） | 10s | N/A（300s超时则切换/重启） | 无 |
> | `roamlink_active`（Roamlink监控） | 30s | 最快90s | rx_packets 120s无增长→切换/重启 |
> 
> Roamlink 通道的 TCP 测试间隔比 SIM 短（30s vs 60s），因为虚拟 SIM 稳定性相对弱，需要更敏感的检测响应。注意 `roamlink_active` 状态**没有** rx_packets 60s 重拨机制，仅有业务层 120s 检测。

---

## 核心结构体 dial_mng_t

这是状态机的"大脑"，所有状态和参数都存在这里。下面逐字段解释：

```c
dial_stat_enu dial_st;
```
**当前状态**。状态机每次循环都判断这个值，决定执行哪段逻辑。这是整个结构体最重要的字段。

```c
nw_client_handle_type h_nw_client;
```
**网络客户端句柄**。Qualcomm MCM API 的 handle，初始化后用于查询 SIM 状态、网络注册状态等。只初始化一次（`sim_initialized` 标志保护）。

```c
ql_data_call_info_s *p_ipv4_data_call_info;
```
**IPv4 数据呼叫信息指针**。联网成功后存储当前连接的 IP、网关、DNS 等信息。用于获取当前 IP 地址显示在状态文件里。

```c
struct ifaddrs *p_ifaddrs;
```
**网络接口信息指针**。联网成功后通过 IP 地址反查接口名（如 `rmnet_data0`），用于读取该接口的 rx_packets 统计。

```c
uint64_t u64_if_rx_packets;
```
**SIM 通道接口收包数缓存**。上一次读到的 rx_packets 值，用于检测收包是否在增长（判断连接是否活跃）。

```c
uint8_t csq;
```
**当前信号强度**（0~31，99=无效）。由主线程每5秒更新，dial_task 线程读取后写入状态文件。

```c
bool is_func_called;
```
**拨号函数调用标志**。`true` 表示需要（重新）初始化数据呼叫框架，`false` 表示已初始化直接拨号。切换通道后重拨时会置 `true`。

```c
int profile_idx;
```
**拨号 profile 编号**。Quectel 模组用 profile 编号区分不同的 APN 配置，通常是 1。

```c
sim_mng_t *p_sim_mng;
```
**SIM 卡管理结构体指针**。里面存 ICCID（SIM 卡唯一识别码）等信息，读 ICCID 后用来查 APN 数据库。

```c
struct timespec dial_timer;
```
**拨号超时计时器**（双用途）：
1. 在 `wait_for_connect` 状态：记录开始等待的时刻，超时则重新拨号
2. 在 `net_connected` 状态：记录上次 rx_packets 有增长的时刻，超过60秒无增长则重拨

```c
int smd_fd;
```
**AT 串口文件描述符**。用于发送 AT 命令给模组。所有 `Ql_SendAT()` 调用都需要这个。

```c
CC_Deque *deque_oper;
```
**运营商列表双端队列**。存储 `AT+COPS=?` 查询到的可用运营商编号，手动选运营商时使用。

```c
apn_obj_t *p_apn_obj;
```
**APN 对象指针**。根据 ICCID 从 APN 数据库查到的 APN 信息（APN 名称、用户名、密码等）。切换通道时会置 `NULL` 强制重新查询。

```c
int network_select;
```
**策略值（1~4）**。从 `/usrdata/network.ini` 读取，决定通道选择和切换逻辑。

```c
struct timespec tcp_test_timer;
```
**SIM 通道 TCP 测试计时器**。记录上次做 TCP 测试的时刻，每60秒触发一次测试。

```c
int tcp_fail_count;
```
**SIM 通道连续 TCP 失败计数**。每次失败 +1，成功清零，达到3次触发切换。切换后也会清零。

```c
struct timespec roamlink_timer;
```
**Roamlink 计时器**（双用途）：
1. 在 `roamlink_starting` 状态：记录启动时刻，超300秒判定超时
2. 在 `roamlink_active` 状态：记录上次 TCP 测试时刻，每30秒触发一次

```c
int roamlink_fail_count;
```
**Roamlink 通道连续 TCP 失败计数**。每次失败 +1，成功清零，达到3次触发切换。

```c
bool sim_initialized;
```
**NW 客户端初始化标志**。`QL_MCM_NW_Client_Init()` 只能调用一次，此标志确保 Roamlink→SIM 切换时跳过重复初始化，直接从 `sim_init` 状态重入。

```c
bool roamlink_available;
```
**Roamlink 可用标志**。`dial_mng_new()` 时通过 `roamlink_probe()` 确定，如果设备没有安装 Roamlink 包或 license 缺失且恢复失败，则为 `false`，策略会被强制降级为 FORCE_SIM。

```c
bool license_pending;
```
**License 等待中标志**。`true` 表示正处于等待 license 云端下载的状态。此时强制走 SIM 上网，同时 RBMaster 在后台尝试下载 license。

```c
struct timespec license_wait_start;
```
**License 等待开始时刻**。进入 `license_pending` 模式时记录，用于计算总等待时间，超过 `LICENSE_WAIT_TIMEOUT_SEC`（生产值7200秒）则放弃。

```c
struct timespec license_check_timer;
```
**License 轮询计时器**。每60秒检测一次 license 文件是否出现，这个计时器记录上次检测时刻。

```c
uint64_t roamlink_rx_packets;
```
**Roamlink 通道收包基准值**。进入 `roamlink_active` 时记录所有 `rmnet_data*` 接口收包总和，之后每30秒对比是否增长，用于业务层存活检测。

```c
struct timespec roamlink_no_data_timer;
```
**Roamlink 无数据计时器**。记录上次检测到 rx_packets 有增长的时刻，超过120秒无增长判定业务层死亡。

```c
bool rbmaster_started;
```
**RBMaster 已启动标志**（仅在 `license_pending` 模式使用）。SIM 联网后启动 RBMaster，此标志确保只启动一次，后续 SIM 断线重连不会重复启动。

```c
struct timespec sim_fallback_timer;
```
**SIM 备用通道计时器**（策略1专用）。每次从 Roamlink 切换到 SIM 备用通道时记录当前时刻。TCP 测试通过且距此时刻超300秒，触发回切 Roamlink。  
零值（`{0,0}`）表示还没进入过 SIM 备用通道，此时不触发回切。

```c
struct timespec roamlink_fallback_timer;
```
**Roamlink 备用通道计时器**（策略2专用）。每次从 SIM 切换到 Roamlink 备用通道时记录当前时刻。TCP 测试通过且距此时刻超300秒，触发回切 SIM。  
零值表示还没进入过 Roamlink 备用通道。

```c
bool net_was_down;
```
**断网状态标志**（断网统计用）。记录上一次调用 `dial_status_update()` 时网络是否处于断开状态，用于检测"联网→断网"和"断网→联网"的边沿。

```c
bool ever_connected;
```
**曾经联网标志**（断网统计用）。防止程序开机后还没联过网就把"未联网"误计入断网次数。只有 `ever_connected=true` 时才开始统计断网。

```c
struct timespec net_down_since;
```
**断网开始时刻**。检测到"联网→断网"边沿时记录，用于计算本次断网持续时间。

```c
int outage_count;
```
**断网次数**。本次 dial 运行期间的断网总次数（重启后清零）。

```c
long last_outage_sec;
```
**上次断网时长（秒）**。每次断网恢复后更新，记录刚结束的那次断网持续了多少秒。

```c
long total_outage_sec;
```
**累计断网时长（秒）**。本次 dial 运行期间所有断网时间之和（重启后清零）。

> **补充分析（来自切换逻辑深度分析）**：
> 
> 切换操作时各计时器的重置规律总结：
> 
> **SIM → Roamlink 切换时**：
> 
> | 字段 | 操作 | 原因 |
> |---|---|---|
> | `roamlink_timer` | = 当前时间 | Roamlink 连通等待计时从0开始 |
> | `roamlink_fail_count` | 清零 | 新一轮失败计数 |
> | `roamlink_fallback_timer` | = 当前时间 | 记录进入 Roamlink 备用时刻（策略2回切用） |
> | `p_apn_obj` | = NULL | 强制下次 SIM 拨号重新查 APN |
> 
> **Roamlink → SIM 切换时**：
> 
> | 字段 | 操作 | 原因 |
> |---|---|---|
> | `tcp_test_timer` | = 当前时间 | SIM 刚联网，不立刻触发 TCP 测试 |
> | `tcp_fail_count` | 清零 | 新一轮失败计数 |
> | `sim_fallback_timer` | = 当前时间 | 记录进入 SIM 备用时刻（策略1回切用） |
> | `p_apn_obj` | = NULL | 强制重新查 APN |

---

# 第三部分：dial.c（状态机核心，最重要的文件）

dial.c 共约1600行，是整个系统的心脏。包含三个大块：
1. `dial_mng_new()` — 初始化函数（约185行）
2. 辅助函数群（约470行）
3. `dial_task()` — 状态机主循环（约900行）

---

## 一、辅助函数：diff()

```c
struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0)
    {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    }
    else
    {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}
```

**作用**：计算两个 `timespec` 时间点的差值（结束时间 - 开始时间）。  
**为什么要处理纳秒借位**：`timespec` 的 `tv_nsec` 是0~999999999的纳秒值。如果结束时间的纳秒比开始时间的纳秒小（例如 end.tv_nsec=100, start.tv_nsec=900），直接相减会得到负数，需要向秒借1位（即1000000000纳秒），这就是"借位处理"。  
**整个程序里大量使用**：每次要判断"距上次操作过了多少秒"，都调用这个函数，然后检查返回值的 `tv_sec` 字段。

---

## 二、dial_mng_new() — 初始化函数

**作用**：程序启动时调用一次，创建并初始化整个状态机结构体。

### 内存分配

```c
dial_mng_t *p_dial_mng = (dial_mng_t *)calloc(1, sizeof(dial_mng_t));
sim_mng_t *p_sim_mng;
memset((char *)p_dial_mng, 0, sizeof(dial_mng_t));
```

`calloc` 分配内存并清零（calloc 本身就会清零，但后面又 `memset` 了一遍，双保险确保全部为0）。  
`sim_mng_t` 是 SIM 卡管理结构体，后面分配。

```c
p_dial_mng->p_ipv4_data_call_info = NULL;
p_dial_mng->dial_st = dial_stat_none;
```

明确置 NULL 和初始状态（虽然 memset 已清零，但显式赋值让代码更清晰）。  
`dial_stat_none` 是状态机的起点。

```c
p_sim_mng = (sim_mng_t *)calloc(1, sizeof(sim_mng_t));
p_dial_mng->p_sim_mng = p_sim_mng;
p_dial_mng->smd_fd = at_init();
cc_deque_new(&p_dial_mng->deque_oper);
p_dial_mng->p_apn_obj = NULL;
p_dial_mng->is_func_called = true;
```

- `at_init()`：打开 AT 串口，返回文件描述符
- `cc_deque_new()`：初始化运营商列表双端队列（手动选运营商时用）
- `is_func_called = true`：标记需要初始化数据呼叫框架

> **补充分析（来自v2分析文档）**：
> 
> `calloc(1, sizeof(dial_mng_t))` 本身就会清零分配的内存（C 标准保证），紧接着的 `memset(..., 0, sizeof(dial_mng_t))` 是冗余操作。这是代码历史遗留的防御性写法，无害但浪费几微秒。

### Bug1修复：策略为SIM类时先停Roamlink

```c
p_dial_mng->network_select = roamlink_read_policy();

if (p_dial_mng->network_select == NET_POLICY_PREFER_SIM ||
    p_dial_mng->network_select == NET_POLICY_FORCE_SIM)
{
    if (roamlink_is_master_running())
    {
        SEAS_LOG_INFO("dial_mng_new: policy=%d (SIM), stopping Roamlink service first",
                      p_dial_mng->network_select);
        roamlink_stop_service();
        sleep(2);
    }
}
```

**背景**：前一个 dial 实例可能在 Roamlink 通道运行，被 `killall dial` 时模组底层仍处于虚拟 SIM 状态。  
**问题**：新 dial 启动后读 ICCID，读到的是 Roamlink 虚拟 SIM 的 ICCID（如 `8985203...F`），APN 数据库里找不到这张"卡"，导致无法拨号，永远卡在 `start_call`。  
**修复**：如果策略是 SIM 类（2或4），且 RBMaster 还在运行，先发 `RBstopServiceMaster` 停掉 Roamlink 服务，等2秒让模组切回物理 SIM，再继续初始化。  
`sleep(2)` 是给模组的切换时间，太短模组还没切完。

### 初始化计时器

```c
clock_gettime(CLOCK_MONOTONIC, &p_dial_mng->tcp_test_timer);
clock_gettime(CLOCK_MONOTONIC, &p_dial_mng->roamlink_timer);
```

TCP 测试计时器和 Roamlink 计时器初始化为当前时间，这样进入状态机后马上就能做第一次测试（因为距"上次测试"已经过了0秒，立刻触发）。

```c
p_dial_mng->tcp_fail_count      = 0;
p_dial_mng->roamlink_fail_count = 0;
p_dial_mng->sim_initialized     = false;
p_dial_mng->license_pending     = false;
p_dial_mng->roamlink_rx_packets = 0;
clock_gettime(CLOCK_MONOTONIC, &p_dial_mng->roamlink_no_data_timer);
p_dial_mng->rbmaster_started    = false;
```

各计数器和标志清零。`roamlink_no_data_timer` 设为当前时间，这样刚进入 `roamlink_active` 时，业务层检测从当前时刻开始计时，不会立刻误判。

```c
memset(&p_dial_mng->sim_fallback_timer,      0, sizeof(...));
memset(&p_dial_mng->roamlink_fallback_timer, 0, sizeof(...));
```

回切计时器用零值初始化，而不是 `clock_gettime`。  
**原因**：零值代表"从未进入过备用通道"，之后的回切判断会检查 `tv_sec > 0`，零值时跳过回切逻辑，防止还没进入备用通道就触发回切。

```c
p_dial_mng->net_was_down      = false;
p_dial_mng->ever_connected    = false;
p_dial_mng->outage_count      = 0;
p_dial_mng->last_outage_sec   = 0;
p_dial_mng->total_outage_sec  = 0;
clock_gettime(CLOCK_MONOTONIC, &p_dial_mng->net_down_since);
```

断网统计初始化。`net_down_since` 设为当前时间（防御性初始化，避免零值被 diff() 计算出极大数值）。

### roamlink_probe() 三态处理

```c
roamlink_probe_result_e probe = roamlink_probe();
```

探测结果有三种，分别处理：

**情况1：ROAMLINK_PROBE_OK**
```c
if (probe == ROAMLINK_PROBE_OK)
{
    p_dial_mng->roamlink_available = true;
}
```
RBMaster 存在且 license 有效，正常可用，`roamlink_available = true`。

**情况2：ROAMLINK_PROBE_NO_PACKAGE**
```c
else if (probe == ROAMLINK_PROBE_NO_PACKAGE)
{
    p_dial_mng->roamlink_available = false;
    if (p_dial_mng->network_select != NET_POLICY_FORCE_SIM)
    {
        SEAS_LOG_ERROR("roamlink: NO PACKAGE ...");
        p_dial_mng->network_select = NET_POLICY_FORCE_SIM;
    }
}
```
RBMaster 根本不存在（设备没装 Roamlink 包），永久降级为 FORCE_SIM。  
`if (network_select != FORCE_SIM)` 判断避免重复打日志（如果原来就是 FORCE_SIM 就不用改了）。

**情况3：ROAMLINK_PROBE_LICENSE_MISSING（含 CONF_MISSING）**
```c
else
{
    p_dial_mng->roamlink_available = false;

    if (roamlink_license_restore_from_backup())
    {
        if (roamlink_probe() == ROAMLINK_PROBE_OK)
        {
            p_dial_mng->roamlink_available = true;
        }
        else
        {
            p_dial_mng->network_select = NET_POLICY_FORCE_SIM;
        }
    }
    else
    {
        p_dial_mng->license_pending = true;
        clock_gettime(CLOCK_MONOTONIC, &p_dial_mng->license_wait_start);
        clock_gettime(CLOCK_MONOTONIC, &p_dial_mng->license_check_timer);
        p_dial_mng->network_select = NET_POLICY_FORCE_SIM;
    }
}
```

RBMaster 存在但 license 缺失（可能是固件重刷导致）：
1. 先尝试从备份路径（`/data/ufs/license.cer`）恢复
2. 恢复成功 → 重新 probe，OK 则正常用，还是失败则降级 FORCE_SIM
3. 备份也没有 → 进入 `license_pending` 模式：  
   - 临时改为 FORCE_SIM（保证网络不中断）  
   - 记录等待开始时刻（用于超时判断）  
   - 记录上次检测时刻（用于60秒轮询）  
   - SIM 联网后启动 RBMaster，等它从云端下载 license

> **补充分析（来自v2分析文档）**：
> 
> `roamlink_probe()` 实际上有 **四种**返回状态，基础文档的 `else` 分支合并处理了两种情况：
> 
> | 返回值 | 条件 | dial_mng_new 中的处理 |
> |---|---|---|
> | `ROAMLINK_PROBE_OK` | RBMaster + conf.ini + license 全部就绪 | `roamlink_available = true` |
> | `ROAMLINK_PROBE_NO_PACKAGE` | RBMaster 二进制不存在 | 永久降级 FORCE_SIM |
> | `ROAMLINK_PROBE_CONF_MISSING` | RBMaster 存在，但 `/opt/conf.ini` 不存在 | 永久降级 FORCE_SIM（factoryApp 未运行，设备未完成出厂配置） |
> | `ROAMLINK_PROBE_LICENSE_MISSING` | RBMaster + conf.ini 都在，但 license 缺失/为空 | 尝试从备份恢复；失败则进入 license_pending 临时 SIM 模式 |
> 
> `CONF_MISSING` 和 `LICENSE_MISSING` 在 `dial_mng_new()` 的 `else` 分支里被合并处理了（对上层逻辑而言，两者都进入 else 分支，触发备份恢复或 license_pending 逻辑）。区别在于 `CONF_MISSING` 时即使备份恢复成功，第二次 probe 也会因为 conf.ini 不存在而失败，最终仍然降级 FORCE_SIM。

### 写入 network_type 文件

```c
if (network_select == PREFER_ROAMLINK || FORCE_ROAMLINK)
    roamlink_mark_network_type(NETWORK_TYPE_ROAMLINK);  // 写 2
else
    roamlink_mark_network_type(NETWORK_TYPE_SIM);        // 写 1
```

开机时就把当前通道类型写入 `/tmp/network_type`，供其他程序读取。

---

## 三、辅助函数群

### dial_init()

```c
bool dial_init(dial_mng_t *p_dial_mng)
{
    if (0 != QL_MCM_NW_Client_Init(&p_dial_mng->h_nw_client))
    {
        return false;
    }
    return true;
}
```

**作用**：初始化 Qualcomm MCM 网络客户端（`h_nw_client`），后续查询 SIM 状态、网络注册状态都需要这个 handle。  
**只调用一次**：`sim_initialized` 标志保证整个程序生命期内只调用一次，重新拨号时跳过这个状态。

### dial_data_call_state_callback()

```c
void dial_data_call_state_callback(ql_data_call_state_s *state)
{
    SEAS_LOG_INFO("profile id %d ", state->profile_idx);
    SEAS_LOG_INFO("IP family %s ", ...);
    if (QL_DATA_CALL_CONNECTED == state->state)
    {
        nw_mark_network_status(1);
    }
    else
    {
        nw_mark_network_status(0);
    }
}
```

**作用**：数据呼叫状态变化的回调函数。当底层连接状态改变（断开/连上）时，Qualcomm SDK 会调用这个函数。  
连接时写 `/tmp/network_status = 1`，断开时写 0。  
**注意**：这个回调是异步的，实际的状态机切换还是由 `dial_task` 的轮询控制，不依赖这个回调。

### dail_start_data_call()

```c
bool dail_start_data_call(dial_mng_t *p_dial_mng)
{
    bool ret = false;
    ql_data_call_s data_call = {0};
    ql_data_call_error_e err = QL_DATA_CALL_ERROR_NONE;
    if (p_dial_mng->is_func_called)
    {
        if (QL_Data_Call_Init(dial_data_call_state_callback))
        {
            SEAS_LOG_INFO("Initialization data call failure");
            exit(0);
        }
    }
```

**作用**：发起数据呼叫（拨号）。  
`is_func_called = true` 时，先调用 `QL_Data_Call_Init` 注册回调，再拨号。  
注意：`QL_Data_Call_Init` 失败时直接 `exit(0)` 退出，这是 Qualcomm SDK 的要求，初始化失败无法继续。

```c
    if (p_dial_mng->is_func_called)
    {
        data_call.profile_idx = p_dial_mng->profile_idx;
        data_call.ip_family = QL_DATA_CALL_TYPE_IPV4;
#ifdef AUTO_REDIAL
        data_call.reconnect = true;
#else
        data_call.reconnect = false;
#endif
```

`AUTO_REDIAL` 宏定义在 dial.h 中，因此 `reconnect = true`，表示底层 SDK 会在连接断开时自动重连（不需要上层再发起新的拨号）。  
`profile_idx` 是之前根据 ICCID 查到的 APN profile 编号。

```c
        if (0 == QL_Data_Call_Start(&data_call, &err))
        {
            p_dial_mng->is_func_called = false;
            ret = true;
        }
    }
    p_dial_mng->is_func_called = false;
    return ret;
}
```

`QL_Data_Call_Start` 发起数据呼叫，成功返回0。  
成功后把 `is_func_called` 置 `false`，表示不需要重新初始化了。  
最后无论成功失败都把 `is_func_called = false`（防止下次进来重复初始化）。

> **补充分析（来自v2分析文档）**：
> 
> `QL_Data_Call_Init` 失败时 `exit(0)` 会直接终止整个 `dial` 进程。`check_network.sh` 看门狗会检测到进程消失并重启，所以这不是死路——相当于触发一次自动重启。但应注意退出码用的是 `0`（表示成功），更严格的做法应该用 `exit(1)` 表示异常退出，以便外部监控区分正常退出和异常退出。
> 
> `is_func_called` 的语义：
> - `true` = 需要重新调用 `QL_Data_Call_Init` 注册回调（每次 SIM 拨号前必须重注册）
> - `false` = 已注册，本轮无需重注册
> 
> 每次 Roamlink→SIM 切换时，`is_func_called` 在切换代码里被设为 `true`，确保新一轮 SIM 拨号前正确重注册回调函数。

### dail_stop_data_call()

```c
bool dail_stop_data_call(int profile_idx)
{
    ql_data_call_error_e err = QL_DATA_CALL_ERROR_NONE;
    if (0 == QL_Data_Call_Stop(profile_idx, QL_DATA_CALL_TYPE_IPV4, &err))
    {
        ret = true;
    }
    return ret;
}
```

**作用**：停止 SIM 数据呼叫。切换到 Roamlink 通道时调用，停止 SIM 的数据连接。  
**已知问题**：是否真正停止 rmnet_data0 上的流量从未实际验证，生产环境需抓包确认。

### get_tz_after_connected()

```c
bool get_tz_after_connected(dial_mng_t *p_dial_mng)
{
    pthread_mutex_lock(&g_at_port_mutex);
    if (0 != Ql_SendAT(p_dial_mng->smd_fd, "AT+QLTS=1", "OK", 1000, rsp_msg))
    { ... }
    else
    {
        if (parse_tz_info(rsp_msg, &tz))
            save_tz_info(tz);
    }
    pthread_mutex_unlock(&g_at_port_mutex);
}
```

**作用**：SIM 联网成功后，发 `AT+QLTS=1` 查询网络时间，解析并保存时区信息。  
这样设备的系统时间能跟运营商网络时间同步。

### ipv4_data_call_info_init()

```c
void ipv4_data_call_info_init(dial_mng_t *p_dial_mng)
{
    if (p_dial_mng->p_ipv4_data_call_info != NULL)
        free(p_dial_mng->p_ipv4_data_call_info);

    p_dial_mng->p_ipv4_data_call_info = nw_get_connect_ipv4_data_call_info(p_dial_mng->profile_idx);

    if (p_dial_mng->p_ipv4_data_call_info != NULL)
    {
        if (p_dial_mng->p_ifaddrs != NULL)
            free(p_dial_mng->p_ifaddrs);

        p_dial_mng->p_ifaddrs = nw_get_ifaddrs(
            inet_ntoa(p_dial_mng->p_ipv4_data_call_info->v4.addr.ip));

        if (p_dial_mng->p_ifaddrs != NULL)
            p_dial_mng->u64_if_rx_packets = 0;
    }
}
```

**作用**：SIM 联网成功后，获取连接信息和接口信息。  
步骤：
1. 释放旧的 IP 信息（重新拨号时要更新）
2. 查询当前 IP、网关、DNS 等（存到 `p_ipv4_data_call_info`）
3. 根据 IP 地址找到对应的接口名（如 `rmnet_data0`），存到 `p_ifaddrs`
4. 把 rx_packets 计数清零（重新开始统计）

---

## 四、dial_status_update() — 状态文件更新

**作用**：把当前状态机所有关键字段写入 `/tmp/dial_status`。每30秒周期调用一次，状态切换时也即时调用。

### 断网统计边沿检测（新增）

```c
bool net_up = (p_dial_mng->dial_st == dial_stat_net_connected ||
               p_dial_mng->dial_st == dial_stat_roamlink_active);
```

判断当前是否联网：只有这两个状态才算"联网中"，其他所有状态（包括 `roamlink_starting`）都算断网。

```c
if (net_up)
    p_dial_mng->ever_connected = true;
```

一旦联网过，就标记 `ever_connected = true`，之后才开始统计断网。

```c
if (!net_up && !p_dial_mng->net_was_down && p_dial_mng->ever_connected)
{
    // 联网→断网的边沿
    p_dial_mng->net_down_since = *cur_timer;
    p_dial_mng->outage_count++;
    p_dial_mng->net_was_down = true;
}
```

三个条件同时满足才计入断网：
1. `!net_up`：当前不在联网状态
2. `!net_was_down`：上一次是联网状态（边沿检测）
3. `ever_connected`：曾经联网过（防止开机未联网就计数）

```c
else if (net_up && p_dial_mng->net_was_down)
{
    // 断网→联网的边沿
    struct timespec od = diff(p_dial_mng->net_down_since, *cur_timer);
    p_dial_mng->last_outage_sec   = od.tv_sec;
    p_dial_mng->total_outage_sec += od.tv_sec;
    p_dial_mng->net_was_down = false;
}
```

断网结束时，计算本次断网时长，累加到总计，更新上次断网时长。

```c
if (p_dial_mng->net_was_down)
{
    struct timespec od = diff(p_dial_mng->net_down_since, *cur_timer);
    st.current_outage_sec = od.tv_sec;
}
```

如果当前正在断网，计算"已经断了多少秒"写入状态文件，实时可见。

> **补充分析（来自切换逻辑深度分析）**：
> 
> 三层监控方式对比：
> 
> | 监控层 | 检测函数 | 触发时机 | 可检测的故障 |
> |---|---|---|---|
> | 底层连接 | `nw_get_connect_state()` | 每次 `net_connected` 循环 | PDN 连接断开（运营商拒绝或 keepalive 超时） |
> | TCP 可达性 | `nw_tcp_connectivity_test()` | SIM 每60s、Roamlink 每30s | 网络通路问题、IP 路由故障、服务不可达 |
> | 业务层收包 | `nw_get_rmnet_rx_packets_sum()` | Roamlink 通道 TCP 测试通过后 | 虚拟SIM"空转"（TCP测试通但应用层无数据） |
> 
> 注意：`dial_stat_roamlink_starting` 状态不算联网，`dial_status_update` 会把它计入断网统计时间。

---

## 五、dial_task() — 状态机主循环

### 主循环骨架

```c
void *dial_task(void *arg)
{
    dial_mng_t *p_dial_mng = (dial_mng_t *)arg;
    sim_mng_t *p_sim_mng = p_dial_mng->p_sim_mng;
    struct timespec cur_timer, dif_timer;
    char rsp_msg[1024];
    int retry_nb = 0;

    while (true)
    {
        clock_gettime(CLOCK_MONOTONIC, &cur_timer);
        // [周期任务：license轮询]
        // [周期任务：状态文件刷新]
        switch (p_dial_mng->dial_st)
        {
            case dial_stat_none: ...
            case dial_stat_init: ...
            ...
        }
        sleep(1);
    }
}
```

每次循环：
1. 获取当前时间 `cur_timer`（所有计时计算用这个，保证一次循环内时间一致）
2. 检查 license_pending 轮询
3. 检查是否需要刷新状态文件
4. 执行当前状态的逻辑
5. `sleep(1)` 等1秒（注意：某些状态内部有额外的 `sleep`，如 `roamlink_starting` 里 sleep 10）

### license_pending 轮询

```c
if (p_dial_mng->license_pending)
{
    struct timespec lc_dif = diff(p_dial_mng->license_check_timer, cur_timer);
    struct timespec lt_dif = diff(p_dial_mng->license_wait_start, cur_timer);

    if (lc_dif.tv_sec >= LICENSE_CHECK_INTERVAL_SEC)  // 每60秒
    {
        p_dial_mng->license_check_timer = cur_timer;   // 重置计时器
        if (roamlink_license_appeared())               // 检测文件是否出现
        {
            roamlink_license_backup_and_reboot();      // 备份并重启（不返回）
        }
    }

    if (lt_dif.tv_sec >= LICENSE_WAIT_TIMEOUT_SEC)    // 超时放弃
    {
        p_dial_mng->license_pending = false;
        // network_select 保持 FORCE_SIM 不变
    }
}
```

**关键点**：这段代码在主循环的 `switch` 语句之前，无论当前处于哪个状态都会执行。这样即使 SIM 正在拨号，也能同时检测 license 是否下载完成。

### 状态文件周期刷新

```c
{
    static struct timespec s_status_timer;
    static bool s_status_initialized = false;
    if (!s_status_initialized)
    {
        s_status_timer      = cur_timer;
        s_status_initialized = true;
        dial_status_update(...);  // 首次立即写一次
    }
    else
    {
        struct timespec st_dif = diff(s_status_timer, cur_timer);
        if (st_dif.tv_sec >= 30)
        {
            s_status_timer = cur_timer;
            dial_status_update(...);
        }
    }
}
```

`static` 变量在函数调用间保持值（只初始化一次）。首次进入立刻写一次状态文件，之后每30秒写一次。  
状态切换时会额外调用 `dial_status_update`，所以实际刷新频率可能比30秒更快。

---

### 状态：dial_stat_none（起点）

```c
case dial_stat_none:
    if (p_dial_mng->license_pending)
    {
        p_dial_mng->dial_st = dial_stat_init;
        break;
    }
```

**license_pending 模式**：直接走 SIM（`dial_stat_init`），因为此时强制 FORCE_SIM，不管策略。

```c
    if (network_select == PREFER_ROAMLINK || FORCE_ROAMLINK)
    {
        if (!p_dial_mng->roamlink_available)
        {
            SEAS_LOG_ERROR("guard triggered! roamlink not available, forcing SIM");
            p_dial_mng->network_select = NET_POLICY_FORCE_SIM;
            p_dial_mng->dial_st = dial_stat_init;
            break;
        }
        roamlink_start_master();
        p_dial_mng->roamlink_timer = cur_timer;
        roamlink_start_service(p_dial_mng->smd_fd);
        p_dial_mng->dial_st = dial_stat_roamlink_starting;
    }
    else
    {
        p_dial_mng->dial_st = dial_stat_init;
    }
```

**Roamlink 策略（1/3）**：先检查防呆（roamlink_available 为 false 则强制 SIM），然后启动 RBMaster、启动 Roamlink 服务，进入 `roamlink_starting`。  
**SIM 策略（2/4）**：直接进入 `dial_stat_init`。

---

### 状态：dial_stat_init → dial_stat_wait_for_connect（SIM 拨号流程）

```c
case dial_stat_init:
    if (dial_init(p_dial_mng))
    {
        p_dial_mng->sim_initialized = true;
        p_dial_mng->dial_st = dial_stat_sim_init;
    }
    break;
```

调用 `QL_MCM_NW_Client_Init`，设置 `sim_initialized = true`（后续重拨跳过）。

```c
case dial_stat_sim_init:
    if (E_QL_OK == sim_op(SIM_OP_INIT, NULL))
    {
        p_dial_mng->dial_st = dial_stat_sim_check;
        p_dial_mng->dial_timer = cur_timer;
    }
    else
        sleep(2);  // 失败等2秒重试
    break;
```

初始化 SIM 操作，设置超时计时器起点。

```c
case dial_stat_sim_check:
    if (nw_get_sim_card_status(p_dial_mng->h_nw_client))
    {
        p_dial_mng->dial_st = dial_stat_sim_op;
    }
    else
    {
        sleep(2);
    }
    dif_timer = diff(p_dial_mng->dial_timer, cur_timer);
    if (dif_timer.tv_sec > SIM_CHECK_TIMEOUT_SECONDS)  // 超1小时
        p_dial_mng->dial_st = dial_stat_stop_cfun;  // 重置模组
    break;
```

检查 SIM 卡就绪，失败每2秒重试。超1小时说明 SIM 卡可能有问题，走 `stop_cfun` 重置模组。

```c
case dial_stat_sim_op:
    if (sim_op_handler(p_sim_mng, sim_op_stat_get_iccid))
    {
        p_dial_mng->dial_st = dial_stat_reg_check;
    }
    break;
```

读取 SIM 卡 ICCID，存入 `p_sim_mng->sim_iccid`，用于查 APN 数据库。

```c
case dial_stat_reg_check:
    if (nw_reg_status_check(p_dial_mng->h_nw_client))
    {
        p_dial_mng->dial_st = dial_stat_cereg_check;
    }
    else
    {
        sleep(2);
    }
    break;
```

调用 `QL_MCM_NW_GetRegStatus` 检查数据网络注册状态（`data_registration_valid = true` 才通过）。

```c
case dial_stat_cereg_check:
    if (nw_at_get_cereg_stat(p_dial_mng->smd_fd))
    {
        p_dial_mng->dial_st = dial_stat_precondition_check;
    }
    else
    {
        sleep(10);  // 失败等10秒（比其他步骤等更久，因为LTE注册可能需要时间）
    }
    break;
```

发 `AT+CEREG?` 检查 LTE 注册状态。返回 1（国内注册）或 5（漫游注册）才通过。失败等10秒重试（LTE 注册比 2G/3G 慢）。

```c
case dial_stat_precondition_check:
    if (0 == QL_Data_Call_Init_Precondition())
    {
        p_dial_mng->profile_idx = -1;
        p_dial_mng->dial_st = dial_stat_pre_start_call;
    }
    break;
```

检查 Quectel SDK 的数据呼叫前置条件（内部检查，不涉及 AT 命令）。  
`profile_idx = -1` 表示需要重新查询 APN。

```c
case dial_stat_pre_start_call:
    if (p_dial_mng->profile_idx == -1)
    {
        if (p_dial_mng->p_apn_obj == NULL)
        {
            p_dial_mng->p_apn_obj = apn_get_apn_obj(p_sim_mng->sim_iccid);
        }
        if (p_dial_mng->p_apn_obj != NULL)
        {
            p_dial_mng->profile_idx = apn_scan_idx((apn_obj_t *)p_dial_mng->p_apn_obj);
        }
    }
    else
    {
        ql_data_call_default_profile_s profile;
        profile.profile_idx = p_dial_mng->profile_idx;
        QL_Data_Call_Set_Default_Profile(&profile);
        p_dial_mng->dial_st = dial_stat_start_call;
    }
    break;
```

两步走：
1. `profile_idx == -1` 时，用 ICCID 查 APN 数据库（`apn_get_apn_obj`），得到 profile 编号
2. `profile_idx` 已有值时，设置默认 profile，进入 `start_call`

**注意**：切换通道时会把 `p_apn_obj = NULL`，下次进来会重新查（Bug4修复）。

```c
case dial_stat_start_call:
    if (dail_start_data_call(p_dial_mng))
    {
        p_dial_mng->dial_st = dial_stat_wait_for_connect;
        p_dial_mng->dial_timer = cur_timer;  // 记录开始等待的时刻
    }
    break;
```

发起数据呼叫，进入等待状态，记录超时计时器起点。

```c
case dial_stat_wait_for_connect:
    if (nw_get_connect_state(p_dial_mng->profile_idx) == 1)
    {
        SEAS_LOG_INFO("net connected");
        get_tz_after_connected(p_dial_mng);     // 同步时区
        ipv4_data_call_info_init(p_dial_mng);   // 获取IP和接口信息
        nw_mark_network_status(1);
        p_dial_mng->dial_st = dial_stat_net_connected;

        if (p_dial_mng->license_pending && !p_dial_mng->rbmaster_started)
        {
            roamlink_start_master();             // license_pending模式下SIM联网才启动RBMaster
            p_dial_mng->rbmaster_started = true;
        }
        dial_status_update(...);
    }
    else
    {
        dif_timer = diff(p_dial_mng->dial_timer, cur_timer);
        if (dif_timer.tv_sec > DIAL_TIMEOUT_SECONDS)  // 超60秒
        {
            p_dial_mng->dial_st = ... ;  // 超时回 reg_check 重拨
        }
    }
    break;
```

**联网成功时**：
- 同步时区
- 获取当前 IP 和接口名
- 标记网络状态为1
- 进入 `net_connected`
- **license_pending 关键逻辑**：此时才启动 RBMaster，因为 RBMaster 需要用 SIM 网络连云端下载 license，必须等 SIM 先联网稳定才启动（否则 RBMaster 会抢占模组通道踢断 SIM）

---

### 状态：dial_stat_net_connected（SIM通道监控核心）

```c
case dial_stat_net_connected:
    if (nw_get_connect_state(p_dial_mng->profile_idx) <= 0)
    {
        SEAS_LOG_ERROR("connection lost");
        nw_mark_network_status(0);
        p_dial_mng->dial_st = dial_stat_reg_check;
        p_dial_mng->tcp_fail_count = 0;  // 重置失败计数
    }
```

首先检查底层连接是否还在（调 Quectel SDK 查状态）。如果掉线，直接跳回 `reg_check` 重新拨号。  
`tcp_fail_count = 0` 是清理工作，防止旧的失败计数影响下次。

```c
    else
    {
        uint64_t if_rx_packets;
        if (nw_get_if_statistics_rx_packets(&if_rx_packets, p_dial_mng->p_ifaddrs->ifa_name))
        {
            if (if_rx_packets > p_dial_mng->u64_if_rx_packets)
            {
                p_dial_mng->dial_timer = cur_timer;  // 有新数据，重置超时计时器
            }
            p_dial_mng->u64_if_rx_packets = if_rx_packets;  // 更新缓存值
        }
```

读取 SIM 通道接口（如 `rmnet_data0`）的收包数。如果收包数有增长，说明有数据流量，重置 `dial_timer`（防止误判为超时）。  
这里是 SIM 通道的保活检测——如果60秒内 rx_packets 都不增长，判定连接僵死，后面的代码会跳回 `reg_check`。

```c
        dif_timer = diff(p_dial_mng->tcp_test_timer, cur_timer);
        if (dif_timer.tv_sec >= TCP_TEST_INTERVAL_SECONDS)  // 每60秒
        {
            p_dial_mng->tcp_test_timer = cur_timer;  // 重置测试计时器
```

距上次 TCP 测试超过60秒，触发新一轮测试。先重置计时器，即使测试失败下次还是60秒后再测。

```c
            if (nw_tcp_connectivity_test(NW_TCP_TEST_HOST, NW_TCP_TEST_PORT, NW_TCP_TEST_TIMEOUT_SEC))
            {
                if (p_dial_mng->tcp_fail_count > 0)
                    SEAS_LOG_INFO("tcp test recovered after %d failures", ...);
                p_dial_mng->tcp_fail_count = 0;   // 通过，清零失败计数
                p_dial_mng->dial_timer = cur_timer; // 刷新保活计时器
```

TCP 测试通过：
- 清零失败计数
- 刷新保活计时器（防止误触发 rx_packets 超时重拨）

```c
                // Bug2修复：策略1的SIM备用通道主动回切
                if (network_select == PREFER_ROAMLINK
                    && roamlink_available
                    && sim_fallback_timer.tv_sec > 0)  // 曾经进入过SIM备用
                {
                    dif = diff(sim_fallback_timer, cur_timer);
                    if (dif.tv_sec >= SIM_FALLBACK_RETRY_SEC)  // 等够300秒
                    {
                        dail_stop_data_call(profile_idx);
                        p_apn_obj = NULL;               // Bug4：清空APN缓存
                        roamlink_start_master();         // Bug5：确保RBMaster运行
                        roamlink_mark_network_type(NETWORK_TYPE_ROAMLINK);
                        roamlink_start_service(smd_fd);
                        roamlink_fail_count = 0;
                        dial_st = dial_stat_roamlink_starting;
                        dial_status_update(...);
                    }
                }
```

**Bug2修复逻辑**：
- 策略1（优先Roamlink）且 Roamlink 可用
- `sim_fallback_timer.tv_sec > 0` 确保真的进入过 SIM 备用（而不是刚启动）
- 距进入 SIM 备用超过300秒，尝试回切 Roamlink
- 清空 APN 缓存（Bug4修复），确保回切后重新查询
- 先确保 RBMaster 运行（Bug5修复），再启动服务

```c
            }
            else  // TCP测试失败
            {
                p_dial_mng->tcp_fail_count++;
                SEAS_LOG_ERROR("tcp test failed, count: %d / %d", tcp_fail_count, TCP_FAIL_THRESHOLD);
                p_dial_mng->dial_timer = cur_timer;  // 重置保活计时器
```

TCP 失败：计数+1，同时重置 `dial_timer`。  
**为什么重置 dial_timer**：防止 rx_packets 超时（60s）先于 TCP 3次失败（180s）触发，抢先把状态跳回 `reg_check`，导致切换逻辑永远无法执行。

```c
                if (tcp_fail_count >= TCP_FAIL_THRESHOLD)  // 连续3次
                {
                    tcp_fail_count = 0;
                    nw_mark_network_status(0);

                    if (license_pending)  // license等待模式
                    {
                        dial_st = dial_stat_reg_check;  // 只重拨SIM，不切换
                    }
                    else if (network_select == PREFER_SIM || PREFER_ROAMLINK)
                    {
                        // 切换到Roamlink
                        dail_stop_data_call(profile_idx);
                        p_apn_obj = NULL;
                        roamlink_start_master();  // Bug5
                        roamlink_fallback_timer = cur_timer;  // 记录进入Roamlink备用时刻
                        roamlink_start_service(smd_fd);
                        roamlink_fail_count = 0;
                        dial_st = dial_stat_roamlink_starting;
                    }
                    else  // FORCE_SIM（策略4）
                    {
                        dial_st = dial_stat_reg_check;  // 只重拨，永不切换
                    }
                }
```

连续3次失败的处理：
- `license_pending` 模式：只重拨 SIM（RBMaster 正在下载 license，切换会中断下载）
- 策略1/2：切换到 Roamlink，记录 `roamlink_fallback_timer`（供策略2后续回切用）
- 策略4（FORCE_SIM）：只重拨，永远不切 Roamlink

```c
    dif_timer = diff(p_dial_mng->dial_timer, cur_timer);
    if (dif_timer.tv_sec > DIAL_TIMEOUT_SECONDS)  // 60秒无数据
    {
        SEAS_LOG_ERROR("no data rx on net if, redial");
        nw_mark_network_status(0);
        dial_st = dial_stat_reg_check;
        tcp_fail_count = 0;
    }
    sleep(5);
```

rx_packets 60秒无增长，判定连接僵死，重拨。  
`sleep(5)` 控制循环频率，避免 CPU 空转。

> **补充分析（来自切换逻辑深度分析）**：
> 
> **dial_stat_net_connected 中的所有切换触发点一览：**
> 
> **触发点A — SIM TCP 失败3次 → 切换到Roamlink**
> ```
> 时序：T+60s 第1次FAIL(count=1,重置dial_timer) → T+120s 第2次FAIL → T+180s 第3次FAIL → 切换
> 关键：每次TCP失败都重置dial_timer，防止rx_packets 60s超时在TCP失败3次(180s)之前抢先触发
> 分策略：license_pending/FORCE_SIM → 只重拨SIM；PREFER_SIM/PREFER_ROAMLINK → 切到Roamlink
> ```
> 
> **触发点B — TCP 通过且策略1且已在SIM备用300s → 主动回切Roamlink**
> ```
> 条件：TCP通过 + policy==PREFER_ROAMLINK + roamlink_available + sim_fallback_timer>0 + 距进入备用≥300s
> 保护：sim_fallback_timer.tv_sec > 0 防止程序刚启动时误触发（零值代表"从未进入SIM备用"）
> ```
> 
> **触发点C — rx_packets 60s无增长 → 重拨SIM（不切换通道）**
> ```
> 触发条件：diff(dial_timer, now) > 60s
> 注意：这里只重拨，不做通道切换。60s无收包可能只是业务空闲，不代表网络故障
> ```
> 
> **`did_switch` 标志的含义（旁注）**：在 `roamlink_active` 状态中，策略2回切和业务层检测各自都可能调用 `roamlink_stop_service()`。`did_switch` 标志确保同一次30s检测循环只执行一种切换动作，防止 `roamlink_stop_service()` 被调用两次。

---

### 状态：dial_stat_roamlink_starting（等待Roamlink连通）

```c
case dial_stat_roamlink_starting:
    dif_timer = diff(p_dial_mng->roamlink_timer, cur_timer);

    if (nw_tcp_connectivity_test(...))
    {
        // TCP通过，Roamlink已连通
        nw_mark_network_status(1);
        p_dial_mng->roamlink_timer      = cur_timer;
        p_dial_mng->roamlink_fail_count = 0;
        nw_get_rmnet_rx_packets_sum(&p_dial_mng->roamlink_rx_packets);  // 重置业务层基准
        p_dial_mng->roamlink_no_data_timer = cur_timer;
        p_dial_mng->dial_st = dial_stat_roamlink_active;
        dial_status_update(...);
    }
```

TCP 通过表示 Roamlink 已连通：
- 重置 `roamlink_timer` 为当前时间（后续用作30秒测试间隔计时起点）
- 读取当前 rx_packets 总和作为基准值（之后对比增长量）
- 进入 `roamlink_active`

```c
    else if (dif_timer.tv_sec > ROAMLINK_CONNECT_WAIT_SEC)  // 超300秒
    {
        if (network_select == PREFER_ROAMLINK || PREFER_SIM)
        {
            // 超时，切回SIM
            roamlink_stop_service();
            p_apn_obj = NULL;           // Bug4
            sim_fallback_timer = cur_timer;  // 记录进入SIM备用时刻
            tcp_test_timer = cur_timer; // 重置TCP测试计时器

            if (sim_initialized)
            {
                is_func_called = true;
                dial_st = dial_stat_sim_init;  // NW客户端已初始化，跳过init
            }
            else
            {
                dial_st = dial_stat_init;  // 首次进SIM，完整初始化
            }
        }
        else  // FORCE_ROAMLINK（策略3）
        {
            // 超时，重启Roamlink
            roamlink_stop_service();
            sleep(5);
            roamlink_timer = cur_timer;  // 重置等待计时器
            roamlink_start_master();     // Bug5：防御性确保RBMaster活着
            roamlink_start_service(smd_fd);
            // 留在 roamlink_starting
        }
    }
    else
    {
        SEAS_LOG_INFO("Roamlink not yet connected, waited %lds / %ds", ...);
    }
    sleep(10);  // 每10秒检测一次（比net_connected的5秒慢，减少不必要的TCP测试）
```

**sim_initialized 判断**是关键优化：  
- `false`：还没初始化过 NW 客户端，需要从 `dial_stat_init` 完整走一遍  
- `true`：NW 客户端已初始化，只需要重新做 SIM 操作，从 `dial_stat_sim_init` 重入，节省时间

> **补充分析（来自切换逻辑深度分析）**：
> 
> **触发点D — TCP通过进入roamlink_active（正向跳转）：**  
> 进入 `roamlink_active` 时必须记录当前 rx_packets 作为基准。原因：如果用0作为基准，而此时 rmnet_data0 已经积累了之前 SIM 阶段产生的大量收包数，第一次检查就会误判为"有增长"，掩盖了 Roamlink 通道实际上还没有数据流量的情况。读取当前实际值，后续才能正确检测增量。
> 
> **触发点E — 300s超时切回SIM：**  
> `sleep(10)` 让此状态每10秒做一次TCP测试，最多等30次（300秒/10秒）。比 SIM 通道的60秒间隔更频繁，目的是一旦 Roamlink 连通就能快速检测到，减少空等时间。

---

### 状态：dial_stat_roamlink_active（Roamlink通道监控核心）

```c
case dial_stat_roamlink_active:
    dif_timer = diff(p_dial_mng->roamlink_timer, cur_timer);

    if (dif_timer.tv_sec >= ROAMLINK_CHECK_INTERVAL_SEC)  // 每30秒
    {
        p_dial_mng->roamlink_timer = cur_timer;

        if (nw_tcp_connectivity_test(...))
        {
            roamlink_fail_count = 0;
            nw_mark_network_status(1);

            // did_switch防止两个分支同时触发
            bool did_switch = false;

            // Bug3修复：策略2的Roamlink备用通道主动回切SIM
            if (!did_switch
                && network_select == PREFER_SIM
                && roamlink_fallback_timer.tv_sec > 0)
            {
                dif = diff(roamlink_fallback_timer, cur_timer);
                if (dif.tv_sec >= ROAMLINK_FALLBACK_RETRY_SEC)  // 等够300秒
                {
                    roamlink_stop_service();
                    p_apn_obj = NULL;           // Bug4
                    sim_fallback_timer = cur_timer;  // 记录进入SIM的时刻
                    tcp_test_timer = cur_timer;
                    dial_st = sim_initialized ? dial_stat_sim_init : dial_stat_init;
                    dial_status_update(...);
                    did_switch = true;
                }
            }
```

`did_switch` 标志（Bug8修复）：防止"回切SIM"和"业务层检测切SIM"在同一次循环里都触发，导致 `roamlink_stop_service()` 被调用两次。

```c
            // 业务层存活检测
            {
                uint64_t cur_rx = 0;
                if (!did_switch && nw_get_rmnet_rx_packets_sum(&cur_rx))
                {
                    if (cur_rx > roamlink_rx_packets)
                    {
                        // 有收包增长，业务正常
                        roamlink_rx_packets    = cur_rx;
                        roamlink_no_data_timer = cur_timer;
                    }
                    else
                    {
                        // 无增长，检查持续时间
                        no_data_dif = diff(roamlink_no_data_timer, cur_timer);
                        if (no_data_dif.tv_sec >= ROAMLINK_NO_DATA_TIMEOUT_SEC)  // 超120秒
                        {
                            // 业务层死亡处理
                            if (PREFER_ROAMLINK || PREFER_SIM)
                            {
                                roamlink_stop_service();
                                p_apn_obj = NULL;
                                sim_fallback_timer = cur_timer;
                                dial_st = sim_initialized ? dial_stat_sim_init : dial_stat_init;
                            }
                            else  // FORCE_ROAMLINK（策略3）
                            {
                                roamlink_stop_service();
                                sleep(5);
                                roamlink_start_master();  // Bug5
                                roamlink_start_service(smd_fd);
                                dial_st = dial_stat_roamlink_starting;
                            }
                        }
                    }
                }
            }
        }
        else  // TCP失败
        {
            roamlink_fail_count++;
            if (roamlink_fail_count >= ROAMLINK_FAIL_THRESHOLD)  // 连续3次
            {
                roamlink_fail_count = 0;
                nw_mark_network_status(0);
                if (PREFER_ROAMLINK || PREFER_SIM)
                {
                    roamlink_stop_service();
                    p_apn_obj = NULL;       // Bug4
                    sim_fallback_timer = cur_timer;  // 记录进入SIM备用时刻
                    dial_st = sim_initialized ? dial_stat_sim_init : dial_stat_init;
                }
                else  // FORCE_ROAMLINK（策略3）
                {
                    roamlink_stop_service();
                    sleep(5);
                    roamlink_timer = cur_timer;
                    roamlink_start_master();  // Bug5
                    roamlink_start_service(smd_fd);
                    dial_st = dial_stat_roamlink_starting;
                }
            }
        }
    }
    sleep(5);
```

**策略3（FORCE_ROAMLINK）的所有失败处理都是重启 Roamlink，永远不切 SIM。**  
策略1/2 失败则切到 SIM，并记录 `sim_fallback_timer`（供策略1后续回切 Roamlink 用）。

> **补充分析（来自切换逻辑深度分析）**：
> 
> **触发点F — Roamlink TCP失败3次：**
> ```
> 时序：T+30s 第1次FAIL(count=1) → T+60s 第2次FAIL → T+90s 第3次FAIL → 切换
> 对比SIM通道：Roamlink检测间隔30s（vs SIM的60s），从进入active到切换最快90s（vs SIM的180s）
> 原因：Roamlink作为虚拟SIM稳定性相对弱，设计上需要更快的故障响应
> ```
> 
> **触发点G — TCP通过且策略2且已在Roamlink备用300s → 主动回切SIM：**
> ```
> 对称于触发点B。策略2语义"优先SIM"，Roamlink备用稳定300s后尝试回到首选SIM通道
> did_switch保护：此分支若触发，后面的业务层检测(触发点H)跳过，防止重复调用stop_service
> ```
> 
> **触发点H — TCP通过但rx_packets 120s无增长：**
> ```
> 特殊条件：TCP通过（！）但业务层无数据——说明TCP测试的流量本身没有被路由到Roamlink通道
> 实际上：TCP测试产生的SYN-ACK包就会让rx_packets增长，所以触发点H在实践中很少独立发挥作用
> 理论价值：捕获"Roamlink注册成功但数据通路断裂"的极端情况
> ```
> 
> **roamlink_active 没有 rx_packets 60s 重拨保护：**  
> SIM 通道的 `dial_timer` 超时重拨逻辑在 `roamlink_active` 里不存在。Roamlink 通道的存活完全依赖 TCP 测试（30s间隔）和业务层检测（rx_packets 120s阈值）。

---

### 运营商切换相关状态（dial_stat_stop_cfun / start_cfun / list_oper / select_oper）

这四个状态是特殊流程，只在 APN 配置需要手动选运营商（`is_oper_select = true`）时触发：

```
dial_stat_stop_cfun:
    发 AT+CFUN=0  → 关闭无线功能（让模组重新搜网）
    → dial_stat_start_cfun

dial_stat_start_cfun:
    发 AT+CFUN=1  → 恢复无线功能
    → dial_stat_sim_init

dial_stat_list_oper:
    发 AT+COPS=?  → 查询可用运营商列表
    → dial_stat_select_oper

dial_stat_select_oper:
    发 AT+COPS=1,2,{mcc_mnc} → 手动锁定运营商
    → dial_stat_stop_cfun（重新开关无线）
```

这套流程在国内普通场景很少触发，只有特殊 SIM 卡（如某些物联网卡）需要手动指定运营商时才走到这里。

---

# 第四部分：roamlink.c（Roamlink控制层）

roamlink.c 封装所有与 Roamlink 相关的底层操作，让 dial.c 只需调用高层函数，不需关心细节。

---

## roamlink_probe() — 三态探测

```c
roamlink_probe_result_e roamlink_probe(void)
{
    struct stat st;

    if (access(ROAMLINK_MASTER_PATH, X_OK) != 0)
    {
        SEAS_LOG_ERROR("roamlink: RBMaster not found → NO_PACKAGE");
        return ROAMLINK_PROBE_NO_PACKAGE;
    }
```

`access(path, X_OK)` 检查文件是否存在且可执行。  
`ROAMLINK_MASTER_PATH = "/usrdata/roamlink/RBMaster"`  
找不到 RBMaster 说明设备根本没有安装 Roamlink 包，直接返回 `NO_PACKAGE`。

```c
    if (stat(ROAMLINK_LICENSE_PATH, &st) != 0 || st.st_size == 0)
    {
        SEAS_LOG_ERROR("roamlink: license missing or empty → LICENSE_MISSING");
        return ROAMLINK_PROBE_LICENSE_MISSING;
    }

    SEAS_LOG_INFO("roamlink: probe OK (size=%lld bytes)", (long long)st.st_size);
    return ROAMLINK_PROBE_OK;
}
```

`ROAMLINK_LICENSE_PATH = "/usrdata/roamlink/etc/.pconfig/license.cer"`  
`stat()` 获取文件信息，`st.st_size == 0` 说明文件存在但是空的（写入失败的情况）。  
两个检查都通过才返回 OK，并且日志里打印文件大小便于确认 license 完整性。

> **补充分析（来自v2分析文档）**：
> 
> 完整探测状态表（包含第4种状态 CONF_MISSING）：
> 
> | 返回值 | 条件 | dial_mng_new 中的行为 |
> |---|---|---|
> | `ROAMLINK_PROBE_OK` | RBMaster 可执行 + license 存在且非空 | `roamlink_available = true`，正常使用 |
> | `ROAMLINK_PROBE_NO_PACKAGE` | RBMaster 不存在或不可执行 | 永久降级 FORCE_SIM，本次运行不使用 Roamlink |
> | `ROAMLINK_PROBE_CONF_MISSING` | RBMaster 存在，但 `/opt/conf.ini` 不存在 | 永久降级 FORCE_SIM（出厂配置程序未运行） |
> | `ROAMLINK_PROBE_LICENSE_MISSING` | RBMaster + conf.ini 存在，license 缺失或为空 | 尝试从 `/data/ufs/` 备份恢复；失败则进入 license_pending OTA 下载模式 |
> 
> 注意：`CONF_MISSING` 和 `LICENSE_MISSING` 在 `dial_mng_new()` 的 `else` 分支中被统一处理，但在 `roamlink_probe()` 函数中有各自独立的日志和返回值。`CONF_MISSING` 表示 factoryApp（出厂应用）尚未完成设备配置，此时设备不具备 Roamlink 运行的基础条件。

---

## roamlink_license_restore_from_backup() — 从备份恢复

```c
bool roamlink_license_restore_from_backup(void)
{
    struct stat st;
    if (stat(ROAMLINK_LICENSE_BACKUP_PATH, &st) != 0 || st.st_size == 0)
    {
        SEAS_LOG_INFO("roamlink: no valid backup license at %s", BACKUP_PATH);
        return false;
    }
```

备份路径：`/data/ufs/license.cer`（`/data/ufs/` 是持久化分区，重刷固件不清空）。  
备份文件不存在或为空，直接返回 false（没有可恢复的）。

```c
    src_fd = open(ROAMLINK_LICENSE_BACKUP_PATH, O_RDONLY);
    dst_fd = open(ROAMLINK_LICENSE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
```

打开备份文件（只读）和目标文件（写入，如果不存在就创建，`O_TRUNC` 先清空）。

```c
    while ((n = read(src_fd, buf, sizeof(buf))) > 0)
    {
        if (write(dst_fd, buf, n) != n)
        {
            goto done;  // 写入失败，跳到清理
        }
    }
    if (fsync(dst_fd) == 0)
    {
        ok = true;
    }
```

4KB 一块地读写（`buf[4096]`），写完后 `fsync()` 确保数据刷到闪存（不只在内存缓存里）。  
**为什么要 fsync**：嵌入式 Linux 可能在写完后立刻断电（如重启），不 fsync 数据可能丢失。

---

## roamlink_license_appeared() — 检测license下载完成

```c
bool roamlink_license_appeared(void)
{
    struct stat st1, st2;

    if (stat(ROAMLINK_LICENSE_PATH, &st1) != 0 || st1.st_size == 0)
        return false;
```

第一次检查：文件不存在或为空，返回 false。

```c
    sleep(2);  // 等2秒

    if (stat(ROAMLINK_LICENSE_PATH, &st2) != 0 || st2.st_size == 0)
        return false;

    if (st2.st_size != st1.st_size)
    {
        SEAS_LOG_INFO("license file still being written (%lld → %lld bytes), waiting...",
                      st1.st_size, st2.st_size);
        return false;
    }

    return true;
}
```

等2秒后第二次检查。如果文件大小变了，说明 RBMaster 还在写，返回 false 继续等待。  
两次大小一致才认为写入完成。  
**原因**：如果 RBMaster 写文件到一半时检测到 size > 0，会误判为下载完成，但文件内容不完整，RBMaster 读到损坏的 license 会失败。两次对比是防止这种竞争条件。

---

## roamlink_license_backup_and_reboot() — 备份并重启（不返回）

```c
void roamlink_license_backup_and_reboot(void)
{
    // 打开 license 主路径（源）和备份路径（目标）
    src_fd = open(ROAMLINK_LICENSE_PATH, O_RDONLY);
    dst_fd = open(ROAMLINK_LICENSE_BACKUP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    // 逐块复制
    while ((n = read(src_fd, buf, sizeof(buf))) > 0)
        write(dst_fd, buf, n);

    fsync(dst_fd);  // 确保备份落盘
    close(dst_fd);
    close(src_fd);

do_reboot:
    sync();    // 全局同步所有缓存到磁盘
    sleep(2);  // 等待I/O完成
    reboot(RB_AUTOBOOT);  // 触发系统重启

    while (1) sleep(1);  // 不应到达这里，防御性死循环
}
```

**为什么 open 失败要 goto do_reboot 而不是 return**：license 已经下载成功了，无论备份是否成功都应该重启（重启后 probe 会找到 license 正常使用）。备份只是防止下次重刷固件后丢失，不影响本次使用。  
**`reboot(RB_AUTOBOOT)`**：Linux 系统调用，等价于执行 `reboot` 命令。需要包含 `<sys/reboot.h>`。

---

## roamlink_read_policy() — 读取策略配置

```c
int roamlink_read_policy(void)
{
    dictionary *ini;
    int policy;

    if (access(NET_POLICY_CONF_PATH, F_OK) != 0)
    {
        return NET_POLICY_DEFAULT;  // 文件不存在，返回默认值4
    }

    ini = iniparser_load(NET_POLICY_CONF_PATH);
    if (ini == NULL)
    {
        return NET_POLICY_DEFAULT;
    }

    policy = iniparser_getint(ini, "network:network_select", NET_POLICY_DEFAULT);
    iniparser_freedict(ini);  // 释放解析的字典内存
```

使用项目内置的 iniparser 库解析 `/usrdata/network.ini`。  
`iniparser_getint(ini, "network:network_select", NET_POLICY_DEFAULT)` 读取 `[network]` 段的 `network_select` 字段，格式是 `section:key`。  
读完后立刻 `iniparser_freedict` 释放内存，避免泄漏。

```c
    if (policy < NET_POLICY_PREFER_ROAMLINK || policy > NET_POLICY_FORCE_SIM)
    {
        SEAS_LOG_ERROR("invalid policy %d, use default %d", policy, NET_POLICY_DEFAULT);
        policy = NET_POLICY_DEFAULT;
    }
    return policy;
}
```

合法性检查：只接受 1~4，其他值（如0、5、-1）都重置为默认值4（FORCE_SIM）。  
**默认4的原因**：安全第一，没有配置文件时走 SIM，不强制开 Roamlink。

---

## roamlink_is_master_running() — 双重存活检测

### 第1步：/proc 扫描

```c
proc_dir = opendir("/proc");
while ((entry = readdir(proc_dir)) != NULL)
{
    if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
        continue;  // 跳过非数字目录（如 /proc/sys、/proc/net 等）

    snprintf(cmdline_path, sizeof(cmdline_path),
             "/proc/%s/cmdline", entry->d_name);
    fd = open(cmdline_path, O_RDONLY);
    if (fd < 0) continue;

    n = read(fd, cmdline_buf, sizeof(cmdline_buf) - 1);
    close(fd);

    if (n > 0 && strstr(cmdline_buf, "RBMaster"))
    {
        proc_found = true;
        break;
    }
}
closedir(proc_dir);

if (!proc_found)
{
    return false;  // 进程根本不存在
}
```

`/proc/<pid>/cmdline` 包含进程的命令行（以 `\0` 分隔的参数），读到包含 `"RBMaster"` 的进程说明它存在。  
**为什么还不够**：僵尸进程（zombie）在 `/proc` 里仍然可见，`cmdline` 也能读，但它已经不工作了。所以还需要第2步。

### 第2步：端口连通性检测

```c
sockfd = socket(AF_INET, SOCK_STREAM, 0);
flags = fcntl(sockfd, F_GETFL, 0);
fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);  // 设置非阻塞

addr.sin_port = htons(ROAMLINK_CTRL_PORT);  // 5568
inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

ret = connect(sockfd, &addr, sizeof(addr));
if (ret == 0)
{
    port_ok = true;  // 立即连上
}
else if (errno == EINPROGRESS)
{
    // 非阻塞connect，用select等最多1秒
    FD_SET(sockfd, &wfds);
    tv.tv_sec = 1;
    ret = select(sockfd + 1, NULL, &wfds, NULL, &tv);
    if (ret > 0)
    {
        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &sock_err, &sock_err_len);
        if (sock_err == 0) port_ok = true;
    }
}
close(sockfd);

if (!port_ok)
{
    SEAS_LOG_ERROR("RBMaster in /proc but port 5568 not reachable → likely zombie");
    return false;
}
```

**为什么超时设1秒**：RBMaster 监听的是本地回环地址（127.0.0.1），响应应该极快。1秒超时已经很宽松，如果1秒都连不上，说明进程已经不能正常工作了。

> **补充分析（来自切换逻辑深度分析）**：
> 
> 双重验证的必要性：
> 
> ```
> 仅/proc扫描的缺陷：
> RBMaster 崩溃（段错误）→ 变为僵尸进程
> → 父进程(init)未 waitpid 前 /proc/<pid> 仍存在
> → cmdline 仍可读，包含 "RBMaster"
> → 只做/proc扫描误判为"正在运行"
> → 不重启 RBMaster
> → dial 发 RBstartServiceMaster 到5568端口
> → 5568无监听者，命令失败
> → Roamlink 永远无法启动
> 
> 加上端口检测：
> 僵尸进程不持有端口（进程已退出，socket已关闭）
> → 连接 127.0.0.1:5568 失败（1秒超时）
> → 判断"RBMaster未正常运行"
> → 触发重启（fork+exec）
> → 新RBMaster绑定5568，恢复正常
> ```
> 
> 1秒超时选择的依据：127.0.0.1 回环地址延迟理论上是微秒级。1秒还连不上说明没有进程在监听。相比5秒的 TCP 测试超时，1秒已经相当宽松。

---

## roamlink_start_master() — 启动RBMaster进程

```c
void roamlink_start_master(void)
{
    if (roamlink_is_master_running())
    {
        SEAS_LOG_INFO("RBMaster already running, skip");
        return;  // 幂等：已在运行则跳过
    }

    pid = fork();
    if (pid == 0)
    {
        // 子进程
        if (chdir(ROAMLINK_WORKDIR) != 0)  // 切换到 /usrdata/roamlink/
            _exit(1);

        // 重定向stdout/stderr到/dev/null，不污染父进程日志
        int devnull = open("/dev/null", O_WRONLY);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        close(devnull);

        execl(ROAMLINK_MASTER_PATH, "RBMaster", NULL);  // exec替换进程映像
        _exit(1);  // 只有exec失败才到这里
    }

    // 父进程
    SEAS_LOG_INFO("RBMaster forked (pid=%d), waiting 3s", pid);
    sleep(3);  // 等RBMaster完成初始化，5568端口就绪
}
```

**为什么用 `_exit(1)` 而不是 `exit(1)`**：`exit()` 会执行 atexit 钩子和 stdio 缓冲区刷新，在 fork 出的子进程里调用 `exit()` 可能和父进程产生干扰（如双重关闭文件）。`_exit()` 直接进行系统调用退出，更安全。

**为什么不 waitpid**：RBMaster 是长驻进程，不会自然退出。如果调用 `waitpid` 会阻塞父进程。不调用 `waitpid` 时，RBMaster 退出后会变成僵尸进程等待 init（PID=1）收割——在嵌入式 Linux 里 init 会自动收割孤儿进程的子进程，没有问题。

**`sleep(3)` 的必要性**：RBMaster 启动后需要时间完成初始化（加载 .so 库、绑定5568端口）。如果马上发 `RBstartServiceMaster`，RBMaster 可能还没准备好，命令会失败。

---

## roamlink_send_cmd() — 向RBMaster发控制命令

```c
bool roamlink_send_cmd(const char *cmd)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);  // 非阻塞

    addr.sin_port = htons(ROAMLINK_CTRL_PORT);   // 5568
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    ret = connect(sockfd, &addr, sizeof(addr));
    if (ret == 0)
    {
        ok = true;
        goto send;  // 立即连上（本地回环几乎都是这种情况）
    }
    if (errno != EINPROGRESS)
    {
        goto done;  // 连接立即失败
    }

    // 用 select 等待，超时 ROAMLINK_CTRL_TIMEOUT_SEC（5秒）
    FD_SET(sockfd, &wfds);
    tv.tv_sec = ROAMLINK_CTRL_TIMEOUT_SEC;
    ret = select(sockfd + 1, NULL, &wfds, NULL, &tv);
    if (ret <= 0) goto done;  // 超时

    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &sock_err, &sock_err_len);
    if (sock_err != 0) goto done;  // 连接失败
    ok = true;

send:
    if (ok)
    {
        fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);  // 恢复阻塞模式
        if (send(sockfd, cmd, strlen(cmd), 0) < 0)
        {
            ok = false;
        }
    }

done:
    close(sockfd);
    return ok;
}
```

**为什么 send 前恢复阻塞模式**：非阻塞模式下 `send()` 可能返回 `EAGAIN`（发送缓冲区满），需要重试逻辑。改回阻塞模式后 `send()` 会等到数据发出去才返回，简化代码。

> **补充分析（来自v2分析文档）**：
> 
> `roamlink_send_cmd()` 替代了旧版 `roamlink_deploy` 中的 Shell 管道写法：
> 
> ```sh
> # 旧版（check_network.sh 中）：
> echo "RBstartServiceMaster" | nc 127.0.0.1 5568
> ```
> 
> 对比：
> 
> | 维度 | 旧版 echo\|nc | 新版 roamlink_send_cmd() |
> |---|---|---|
> | 进程开销 | 每次 fork() + exec() 两个进程 | 在当前进程内直接 socket 操作 |
> | 错误处理 | Shell 管道无法区分连接失败和命令失败 | 明确区分连接失败/超时/发送失败 |
> | 超时控制 | nc 默认无超时，可能永久阻塞 | select() 5秒超时，不会卡住状态机 |
> | 日志 | 无 | 详细记录每次命令发送结果 |
> 
> 新版在所有维度均优于旧版。

---

## roamlink_stop_service() — 停止虚拟SIM服务

```c
bool roamlink_stop_service(void)
{
    bool ok = roamlink_send_cmd(ROAMLINK_CMD_STOP);  // "RBstopServiceMaster"
    roamlink_mark_network_type(0);  // 写 /tmp/network_type = 0
    sleep(3);  // 给模组时间完成注销
    return ok;
}
```

**`sleep(3)` 的作用**：发出停止命令后，模组需要时间完成虚拟 SIM 注销流程。如果立刻启动物理 SIM 拨号，模组内部状态可能还没切换完，会导致拨号失败或状态混乱。

> **补充分析（来自切换逻辑深度分析）**：
> 
> **完整的 Roamlink→SIM 切换执行序列（6步）：**
> 
> ```
> 1. roamlink_stop_service()               [roamlink.c]
>    步骤1a: roamlink_send_cmd("RBstopServiceMaster")
>             RBMaster 释放虚拟SIM控制权，模组恢复物理SIM
>    步骤1b: roamlink_mark_network_type(0)
>             写 /tmp/network_type = "0"（切换中，未确定通道）
>    步骤1c: sleep(3)
>             ⚠️ 关键等待：给模组时间完成虚拟SIM注销
>             不等直接拨号，模组内部状态混乱会导致失败
> 
> 2. p_apn_obj = NULL                       [dial.c]
>    清空 APN 缓存，强制重新查询
> 
> 3. sim_fallback_timer = cur_timer         [dial.c]
>    记录进入SIM备用的时刻，用于策略1后续主动回切计时
> 
> 4. tcp_test_timer = cur_timer             [dial.c]
>    重置SIM通道TCP测试计时器（SIM刚联网不立刻测试）
> 
> 5. 可选：roamlink_mark_network_type(1)    [dial.c]
>    写 /tmp/network_type = "1"（物理SIM通道）
> 
> 6. 判断 sim_initialized                   [dial.c]
>    false → dial_stat_init（完整初始化，含QL_MCM_NW_Client_Init）
>    true  → is_func_called=true + dial_stat_sim_init（快速重入，跳过MCM初始化）
> ```
> 
> **完整的 SIM→Roamlink 切换执行序列（8步）：**
> 
> ```
> 1. dail_stop_data_call(profile_idx)       [dial.c]
>    调用 QL_Data_Call_Stop()
>    停止物理SIM的数据呼叫（PDN连接断开）
>    ⚠️ 实际停止效果待生产验证
> 
> 2. p_apn_obj = NULL                       [dial.c]
>    清空APN缓存
> 
> 3. roamlink_start_master()                [roamlink.c]
>    双重检测RBMaster存活（/proc + 5568端口）
>    已在运行 → 直接返回（幂等）
>    未运行 → fork()+exec()，sleep(3)等初始化完成
> 
> 4. roamlink_mark_network_type(2)          [roamlink.c内部]
>    写 /tmp/network_type = "2"（Roamlink通道）
> 
> 5. roamlink_start_service(smd_fd)         [roamlink.c]
>    5a: AT+COPS=0（清除手动运营商锁定，需持有g_at_port_mutex）
>    5b: roamlink_send_cmd("RBstartServiceMaster")
>        RBMaster接管模组，建立虚拟SIM连接
>    5c: roamlink_mark_network_type(2)（内部再写一次，确保）
> 
> 6. roamlink_timer = cur_timer             [dial.c]
>    记录启动时刻，用于300s超时计时
> 
> 7. roamlink_fail_count = 0               [dial.c]
>    清零Roamlink失败计数
> 
> 8. dial_st = dial_stat_roamlink_starting  [dial.c]
>    进入等待连通状态
> ```
> 
> **AT+COPS=0 的作用**：清除可能存在的手动运营商锁定（如运营商选择流程里设置的 `AT+COPS=1,2,xxxxx`）。不发这条命令，模组可能锁定在上次的物理 SIM 运营商上，Roamlink 无法自由注册到最优运营商。

---

# 第五部分：nw.c（网络操作层）

---

## nw_get_sim_card_status() — 检查SIM卡就绪

```c
bool nw_get_sim_card_status(nw_client_handle_type nw_client)
{
    QL_MCM_SIM_CARD_STATUS_INFO_T t_info = {0};
    int sim_card_app_status;
    if (0 == QL_MCM_SIM_GetCardStatus(nw_client, E_QL_MCM_SIM_SLOT_ID_1, &t_info))
    {
        sim_card_app_status = t_info.card_app_info.app_3gpp.app_state - 0xB00;
        if (sim_card_app_status == SIM_CARD_READY)  // SIM_CARD_READY = 10
        {
            return true;
        }
    }
    return false;
}
```

`E_QL_MCM_SIM_SLOT_ID_1` 表示第一个 SIM 卡槽。  
`app_state - 0xB00`：Qualcomm MCM 的状态码有一个基地址偏移 `0xB00`，减掉后得到实际状态值。`10` 表示 SIM 卡 App 就绪（可以使用）。

---

## nw_reg_status_check() — 检查数据网络注册

```c
bool nw_reg_status_check(nw_client_handle_type nw_client)
{
    QL_MCM_NW_REG_STATUS_INFO_T t_info = {0};
    if (0 == QL_MCM_NW_GetRegStatus(nw_client, &t_info))
    {
        if (t_info.data_registration_valid)
        {
            ret = true;
        }
    }
    return ret;
}
```

`data_registration_valid` 表示数据网络（4G/3G）注册成功。注意这里只检查数据注册，不检查语音注册（因为只关心能不能上网）。

---

## nw_at_get_cereg_stat() — 检查LTE注册状态

```c
bool nw_at_get_cereg_stat(int smd_fd)
{
    pthread_mutex_lock(&g_at_port_mutex);
    Ql_SendAT(smd_fd, "AT+CEREG?    ", "OK", 1000, rsp_msg);  // 注意命令后有空格
    pthread_mutex_unlock(&g_at_port_mutex);

    p_cereg = strstr(rsp_msg, "+CEREG: ");
    if (p_cereg != NULL)
    {
        if (2 == sscanf(p_cereg, "+CEREG: %d,%d", &cereg_n, &stat_cereg))
        {
            if ((stat_cereg == 1) || (stat_cereg == 5))
                return true;
        }
    }
    return false;
}
```

AT+CEREG 返回格式：`+CEREG: <n>,<stat>`  
- `stat=1`：注册成功（本地网络）  
- `stat=5`：注册成功（漫游网络）  
- 其他值：未注册  

**注意命令 `"AT+CEREG?    "` 后面有多个空格**：这是历史遗留，对功能没影响，串口发送时多余空格会被忽略。

---

## nw_at_get_csq() — 查询信号强度

```c
int nw_at_get_csq(int smd_fd)
{
    static int pre_csq = -1;  // 上次的CSQ值（static保持跨调用持久）

    Ql_SendAT(smd_fd, "AT+CSQ", "OK", 1000, rsp_msg);
    p_csq = strstr(rsp_msg, "+CSQ: ");
    if (p_csq != NULL)
    {
        if (2 == sscanf(p_csq, "+CSQ: %d,%d", &csq, &ber))
        {
            if (pre_csq != csq)  // 只有值变化时才更新文件
            {
                sprintf(echo_cmd, "echo %d > %s", csq, NW_CSQ_PATH);
                system(echo_cmd);  // 写 /tmp/network_csq
                pre_csq = csq;
            }
            return csq;
        }
    }
    return -1;
}
```

`pre_csq` 是 `static` 变量，只有信号强度变化时才更新文件（减少不必要的文件写操作）。  
**AT+CSQ 返回格式**：`+CSQ: <rssi>,<ber>`，`rssi` 是信号强度（0~31，99=未知），`ber` 是误码率（一般不用）。

---

## nw_mark_network_status() — 写网络状态文件

```c
void nw_mark_network_status(int net_status)
{
    char br_str[16] = {0};
    int fd;
    fd = open(NW_STATUS_PATH, O_WRONLY | O_CREAT);
    sprintf(br_str, "%d", net_status);
    write(fd, br_str, strlen(br_str));
    close(fd);
}
```

直接写 `/tmp/network_status` 文件，写入 `"1"` 或 `"0"`。  
**注意**：没有 `O_TRUNC` 标志，如果文件已存在内容比新内容长，旧内容末尾会残留。例如原来是 `"1"`，写 `"0"` 后文件内容是 `"0"`（刚好），但如果原来是 `"10"`，写 `"0"` 后是 `"00"`。实际上 status 只写 `"0"` 或 `"1"` 单字符，长度相同，不会有问题。

> **补充分析（来自v2分析文档）**：
> 
> `open()` 调用缺少 `O_TRUNC` 标志，同时也没有提供 mode 参数（应为 `O_CREAT | O_WRONLY | O_TRUNC, 0644`）。
> 
> 虽然对于单字符 `"0"` / `"1"` 写入实际无害，但这是一个低级缺陷：
> - 若未来写入的内容长度可能变化（如写入多字节），就会出现残留字符问题
> - 缺少 mode 参数（`0644`）意味着 `O_CREAT` 新建文件时权限不确定（依赖 umask），不够明确
> 
> 修复方式：`open(NW_STATUS_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644)`

---

## nw_get_connect_state() — 查询数据连接状态

```c
int nw_get_connect_state(int profile_idx)
{
    ql_data_call_info_s data_call_info;
    ql_data_call_error_e err = QL_DATA_CALL_ERROR_NONE;
    memset(&data_call_info, 0, sizeof(data_call_info));
    if (0 == QL_Data_Call_Info_Get(profile_idx, QL_DATA_CALL_TYPE_IPV4, &data_call_info, &err))
    {
        ret = QL_DATA_CALL_CONNECTED == data_call_info.v4.state ? 1 : 0;
        nw_mark_network_status(ret);  // 同时更新文件
    }
    return ret;
}
```

查询指定 profile 的数据连接是否建立（1=已连，0=未连，-1=查询失败）。每次查询都同步更新 `/tmp/network_status` 文件。

> **补充分析（来自切换逻辑深度分析）**：
> 
> `nw_get_connect_state()` 与 `nw_tcp_connectivity_test()` 的区别：
> 
> | 检测函数 | 检测的是什么 | 触发时机 | 能检测到什么故障 |
> |---|---|---|---|
> | `nw_get_connect_state()` | 数据呼叫是否建立（PDP/PDN连接） | 每次 `net_connected` 循环都检查 | 运营商侧PDN连接断开 |
> | `nw_tcp_connectivity_test()` | 端到端 TCP 能否连到18.196.0.17:22 | SIM每60s，Roamlink每30s | 网络通路问题、路由故障、IP可达性 |
> 
> 前者是底层"有没有连接"，后者是上层"连接能不能用"。底层连接"建立"了但不可用的情况（如分配了IP但路由失效）只有 TCP 测试才能发现。

---

## nw_get_ifaddrs() — 根据IP找接口名

```c
struct ifaddrs *nw_get_ifaddrs(char *p_ip_addr)
{
    getifaddrs(&ifaddr);  // 获取所有网络接口信息

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL) continue;
        family = ifa->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6)
        {
            getnameinfo(ifa->ifa_addr, ..., host, NI_MAXHOST, ..., NI_NUMERICHOST);
            // 把ifa_addr转成字符串形式的IP
            if (0 == strncmp(p_ip_addr, host, strlen(p_ip_addr)))
            {
                // 找到IP匹配的接口
                p_ifaddrs = calloc(1, sizeof(struct ifaddrs));
                memcpy(p_ifaddrs, ifa, sizeof(struct ifaddrs));
                break;
            }
        }
    }
    freeifaddrs(ifaddr);
    return p_ifaddrs;
}
```

**作用**：已知 IP 地址，找对应的网络接口名（如 `rmnet_data0`）。  
`getifaddrs` 获取所有接口，`getnameinfo` 把 sockaddr 转成字符串 IP，找到匹配的就复制出来返回。  
**注意**：返回的 `p_ifaddrs` 是 malloc 分配的，调用方负责 free。

> **补充分析（来自v2分析文档）**：
> 
> **严重缺陷：悬空指针（dangling pointer）**
> 
> ```c
> p_ifaddrs = calloc(1, sizeof(struct ifaddrs));
> memcpy(p_ifaddrs, ifa, sizeof(struct ifaddrs));  // 浅拷贝！
> // ...
> freeifaddrs(ifaddr);  // 释放原始链表，包括 ifa_name 指向的内存
> return p_ifaddrs;    // 返回的结构体里 ifa_name 已成悬空指针！
> ```
> 
> `struct ifaddrs` 包含 `char *ifa_name`（接口名字符串指针）。`memcpy` 只复制了指针值，没有复制它指向的字符串内容。调用 `freeifaddrs(ifaddr)` 后，`ifa_name` 指向的内存被释放，但返回的 `p_ifaddrs->ifa_name` 仍然指向那块已释放的地址。
> 
> 调用方（`ipv4_data_call_info_init`）随后用 `p_dial_mng->p_ifaddrs->ifa_name` 读取接口名，这是未定义行为（UB）——在实际运行中可能偶然正常（内存未被覆盖），也可能读到乱码或崩溃。
> 
> **修复方式**：
> ```c
> // 深拷贝 ifa_name
> p_ifaddrs->ifa_name = strdup(ifa->ifa_name);
> // 调用方 free 时需要同时 free(p_ifaddrs->ifa_name) 再 free(p_ifaddrs)
> ```

---

## nw_get_if_statistics_rx_packets() — 读指定接口收包数

```c
bool nw_get_if_statistics_rx_packets(uint64_t *p_rx_packets, char *if_name)
{
    char path[NAME_MAX];
    sprintf(path, NW_IF_STATISTICS_RX_PACKETS_PATH, if_name);
    // 展开为 /sys/devices/virtual/net/{if_name}/statistics/rx_packets

    fd = open(path, O_RDONLY);
    file_size = lseek(fd, 0L, SEEK_END);  // 获取文件大小
    lseek(fd, 0L, SEEK_SET);              // 回到文件头
    read(fd, val_bytes, file_size);
    close(fd);

    *p_rx_packets = strtoull(val_bytes, NULL, 0);  // 字符串转uint64
    return true;
}
```

`/sys/devices/virtual/net/` 是 Linux 内核暴露的接口统计信息，直接读文件即可，不需要 ioctl。  
`strtoull` 把字符串数字转成 `uint64_t`（无符号64位），支持很大的收包数。

---

## nw_tcp_connectivity_test() — TCP连通性测试（最关键的函数）

```c
bool nw_tcp_connectivity_test(const char *host, int port, int timeout_sec)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // 设置非阻塞
    flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    // 填充目标地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);        // 22
    inet_pton(AF_INET, host, &server_addr.sin_addr);  // 18.196.0.17

    // 发起非阻塞连接
    ret = connect(sockfd, &server_addr, sizeof(server_addr));
    if (ret == 0)
    {
        connected = true;  // 立即成功（极少见）
        goto done;
    }
    if (errno != EINPROGRESS)
    {
        // 立即失败（连接被拒绝或网络不可达）
        SEAS_LOG_ERROR("tcp_test: connect() failed immediately: %s", strerror(errno));
        goto done;
    }
```

**EINPROGRESS**：非阻塞 `connect()` 的正常情况——连接正在进行中，需要用 select 等待结果。

```c
    // 用 select 等待socket可写（连接完成或失败都会触发）
    FD_ZERO(&wfds);
    FD_SET(sockfd, &wfds);
    tv.tv_sec  = timeout_sec;  // 5秒超时
    tv.tv_usec = 0;

    ret = select(sockfd + 1, NULL, &wfds, NULL, &tv);
    if (ret <= 0)
    {
        // ret=0：超时（5秒内服务器没有响应）
        // ret<0：select内部错误
        SEAS_LOG_ERROR("tcp_test: select() timeout or error (ret=%d)", ret);
        goto done;
    }
```

`select` 监视 socket 的可写事件（`wfds`），连接成功和失败都会让 socket 变为可写。  
**为什么不用读事件（rfds）**：TCP 连接建立只需等待写就绪，SYN-ACK 收到后 socket 变可写。

```c
    // select返回可写，还需确认连接真正成功
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &sock_err, &sock_err_len) < 0
        || sock_err != 0)
    {
        SEAS_LOG_ERROR("tcp_test: connect failed: %s", strerror(sock_err));
        goto done;
    }
    connected = true;

done:
    close(sockfd);
    SEAS_LOG_INFO("tcp_test: %s:%d → %s", host, port, connected ? "OK" : "FAIL");
    return connected;
}
```

**为什么还要 `getsockopt(SO_ERROR)`**：select 可写不代表连接成功——连接被对端拒绝（RST）也会让 socket 变为可写，但此时 `SO_ERROR` 会是 `ECONNREFUSED`（111）。必须用 `getsockopt` 确认没有错误才算真正连接成功。

> **补充分析（来自切换逻辑深度分析）**：
> 
> **完整6步执行流程及各步骤含义：**
> 
> | 步骤 | 操作 | 说明 |
> |---|---|---|
> | 第1步 | `socket(AF_INET, SOCK_STREAM, 0)` | 创建TCP socket |
> | 第2步 | `fcntl(F_SETFL, O_NONBLOCK)` | 设置非阻塞。原因：阻塞 connect() 会卡住 dial_task 整个线程5秒，状态机停转 |
> | 第3步 | `connect()` 返回 EINPROGRESS | 发出TCP SYN包，立即返回，正常情况 |
> | 第4步 | `select()` 等最多5秒 | 等待socket可写（SYN-ACK或RST都触发） |
> | 第5步 | `getsockopt(SO_ERROR)` | 确认连接真正成功（sock_err==0） |
> | 第6步 | `close(sockfd)` | 关闭socket，避免fd泄漏 |
> 
> **三种失败日志对应底层原因：**
> 
> | 日志 | 对应步骤 | 含义 |
> |---|---|---|
> | `connect() failed immediately: Network is unreachable` | 第3步 | 路由不存在，数据包发不出去（通道未建立或已断开） |
> | `select() timeout or error (ret=0)` | 第4步 | 5秒内服务器无响应（服务器宕机或防火墙丢包） |
> | `connect failed: Connection refused` | 第5步 | 服务器在线但22端口拒绝连接 |
> 
> **Roamlink 启动期间的正常日志**：在 `roamlink_starting` 状态下，每10秒一次 TCP 测试，由于路由还没建立，会持续看到 `"Network is unreachable"` 日志，这是正常的过渡状态，不代表故障。

---

## nw_get_rmnet_rx_packets_sum() — 业务层存活检测的核心

```c
bool nw_get_rmnet_rx_packets_sum(uint64_t *p_sum)
{
    *p_sum = 0;

    dir = opendir("/sys/devices/virtual/net");
    if (dir == NULL) return false;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strncmp(entry->d_name, "rmnet_data", 10) != 0)
            continue;  // 跳过非rmnet_data开头的接口

        snprintf(path, sizeof(path),
                 "/sys/devices/virtual/net/%s/statistics/rx_packets",
                 entry->d_name);

        fd = open(path, O_RDONLY);
        if (fd < 0) continue;  // 接口消失了，跳过

        n = read(fd, val_bytes, sizeof(val_bytes) - 1);
        close(fd);

        if (n > 0)
        {
            *p_sum += strtoull(val_bytes, NULL, 0);  // 累加
            found = true;
        }
    }

    closedir(dir);
    return found;
}
```

**为什么累加所有 rmnet_data* 而不是指定某一个**：  
Roamlink 虚拟 SIM 使用的 `rmnet_dataX` 编号在不同设备、不同固件版本、不同重启时可能不同（可能是 rmnet_data0，也可能是 rmnet_data1 等）。  
累加所有 `rmnet_data*` 接口，不管 Roamlink 用的是哪一个，只要有数据流量总和就会增长。  
**两种场景都正确**：
- Roamlink 用 rmnet_data1：SIM 停了，只有 rmnet_data1 在涨，总和涨
- Roamlink 也用 rmnet_data0：SIM 停了，rmnet_data0 继续被 Roamlink 使用，总和涨

> **补充分析（来自切换逻辑深度分析）**：
> 
> **此函数与 TCP 测试的耦合关系**：
> 
> TCP 测试成功时，握手产生的 SYN-ACK 包本身就会让 rmnet_dataX 的 rx_packets 增加。因此，只要 TCP 测试通过，rx_packets 就一定会增长，业务层检测实际上只能在"TCP 测试通过但 Roamlink 业务数据断裂"的极端情况下独立发挥作用。
> 
> 实际场景中，触发点H（rx_packets 120s无增长，TCP测试通过）几乎不会单独触发，更多是作为冗余保障。

---

# 第六部分：status/status.c（状态文件输出）

---

## dial_stat_name() — 状态枚举转字符串

```c
const char *dial_stat_name(int stat)
{
    switch (stat)
    {
    case 0:  return "none";
    case 1:  return "init";
    case 2:  return "sim_init";
    ...
    case 16: return "net_connected";
    case 17: return "roamlink_starting";
    case 18: return "roamlink_active";
    default: return "unknown";
    }
}
```

把枚举数字转成可读字符串，用于写入 `/tmp/dial_status` 的 `state=` 字段。  
**注意**：这里用的是数字 case，而不是枚举名，必须和 dial.h 里的枚举顺序严格对应，新增状态时两处都要更新。

---

## dial_status_write() — 原子写入状态文件

```c
void dial_status_write(const dial_status_t *s)
{
    pthread_mutex_lock(&g_status_mutex);

    fp = fopen(DIAL_STATUS_TMP_PATH, "w");  // 先写临时文件
    if (fp == NULL) { ...; pthread_mutex_unlock(...); return; }

    // 写文件头
    fprintf(fp, "# dial status - auto generated by dial v%s\n", s->version);
    fprintf(fp, "# updated: %s\n", timebuf);
    fprintf(fp, "\n");

    // 写[dial]段
    fprintf(fp, "[dial]\n");
    fprintf(fp, "version=%s\n", s->version);
    fprintf(fp, "uptime=%ld\n", s->uptime_sec);
    fprintf(fp, "state=%s\n",   s->state);
    ...

    // 写[sim]、[network]、[counters]、[roamlink]、[stats]段
    ...

    fflush(fp);
    fclose(fp);

    // 原子替换：rename是POSIX原子操作
    rename(DIAL_STATUS_TMP_PATH, DIAL_STATUS_PATH);

    pthread_mutex_unlock(&g_status_mutex);
}
```

**为什么先写临时文件再 rename**：  
如果直接写 `/tmp/dial_status`，外部程序可能在写到一半时读到不完整内容（例如只写了 `[dial]` 段，`[sim]` 段还没写）。  
先写 `/tmp/dial_status.tmp`，写完后用 `rename()` 一步替换，`rename()` 是原子操作（POSIX 保证），外部读者要么读到旧的完整文件，要么读到新的完整文件，不会读到半途的文件。

**互斥锁 `g_status_mutex` 的作用**：dial_task 线程和主线程都可能调用 `dial_status_update()`，互斥锁防止两个线程同时写临时文件造成内容混乱。

> **补充分析（来自v2分析文档）**：
> 
> `/tmp/dial_status` 完整输出示例：
> 
> ```ini
> # dial status - auto generated by dial v1.29.07
> # updated: 2026-03-19 10:05:38
> 
> [dial]
> version=1.29.07
> uptime=329
> state=roamlink_active
> policy=1
> policy_name=PREFER_ROAMLINK
> channel=ROAMLINK           # SIM / ROAMLINK / SWITCHING / NONE
> roamlink_available=1
> license_pending=0
> license_wait_sec=0
> 
> [sim]
> iccid=89464283216100721879
> csq=21
> apn=internet.lte.cxn
> profile_idx=1
> plmn=46001
> 
> [network]
> status=1                   # 1=up, 0=down
> type=2                     # 1=SIM, 2=Roamlink
> ip=10.88.197.109
> ifname=rmnet_data0
> 
> [counters]
> tcp_fail_count=0
> roamlink_fail_count=0
> roamlink_rx_packets=143
> 
> [roamlink]
> state=active               # none / starting / active
> biz_no_data_sec=25
> roamlink_connect_wait_sec=0
> rbmaster_pid=1516
> 
> [stats]
> outage_count=2
> last_outage_sec=22
> total_outage_sec=134
> current_outage_sec=0
> ```
> 
> **字段解读**：
> - `channel=SWITCHING`：切换过程中，两个通道都不处于 `net_connected` / `roamlink_active` 状态
> - `biz_no_data_sec`：业务层无新收包已持续的秒数（Roamlink 通道才有意义）
> - `roamlink_connect_wait_sec`：在 `roamlink_starting` 状态中等待连通的累计秒数
> - `current_outage_sec`：当前断网已持续秒数（仅在断网时非零）
> - `uptime`：dial 进程运行时长（秒），由 `clock_gettime(CLOCK_MONOTONIC)` 计算，不受系统时间调整影响
> 
> 断网统计用边沿检测：`outage_count` 在 `net_connected`→非联网 的跳变时+1；`last_outage_sec` 在恢复联网时更新。进程重启（`killall dial`）后所有统计清零。

---

# 第七部分：check_network.sh（看门狗脚本）

```sh
#!/bin/sh
LOG_FILE="/usrdata/check_network.log"
DIAL_BIN="/usr/dial/dial"
POLL_INTERVAL=30
```

变量定义。日志存到持久化分区（`/usrdata/`），重启后保留。`POLL_INTERVAL=30` 每30秒检查一次。

```sh
log() {
    if [ -f "$LOG_FILE" ] && [ "$(wc -c < "$LOG_FILE")" -gt 1048576 ]; then
        echo "$(date '+%Y-%m-%d %H:%M:%S') [log rotated]" > "$LOG_FILE"
    fi
    echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >> "$LOG_FILE"
}
```

**日志自动轮转**：超过1MB（1048576字节）就清空重写。嵌入式设备闪存容量有限，防止日志撑爆存储。  
`wc -c < "$LOG_FILE"` 统计文件字节数。  
`>` 是覆盖写（清空），`>>` 是追加写。

```sh
log "==== check_network.sh v2.0 (Plan-A watchdog) started ===="

if ! pgrep -f "$DIAL_BIN" >/dev/null 2>&1; then
    "$DIAL_BIN" >/dev/null 2>&1 &
    log "dial started on boot (pid=$!)"
fi
```

开机时检查 dial 是否在运行。`pgrep -f` 按命令行全路径搜索进程，比 `pgrep dial` 更精确。  
`>/dev/null 2>&1` 把 dial 的 stdout 和 stderr 都丢弃（dial 有自己的日志系统）。  
`&` 后台运行。`$!` 是刚启动的后台进程 PID。

```sh
while true; do
    sleep "$POLL_INTERVAL"
    if ! pgrep -f "$DIAL_BIN" >/dev/null 2>&1; then
        log "dial not running, restarting..."
        "$DIAL_BIN" >/dev/null 2>&1 &
        log "dial restarted (pid=$!)"
    fi
done
```

无限循环，每30秒检查一次。dial 进程消失就重启它。  
**注意**：这个脚本不做任何网络测试或切换判断，所有逻辑都在 dial 里。这是和旧版 `check_network.sh` 最大的区别——旧版负责切换，新版只负责保活。

> **补充分析（来自v2分析文档）**：
> 
> ⚠️ **重要区分**：v2 分析文档（section 5.4）描述的 `check_network.sh` 是旧版 `roamlink_deploy` 包里的脚本，包含完整的网络监控和切换逻辑（使用 `echo|nc 127.0.0.1 5568` 发命令、读 `/tmp/network_type` 决策、调用 `start_dial` / `start_vsim` 等）。
> 
> **本文档（方案A v2）使用的是全新的看门狗专用版本**，与旧版的主要区别：
> 
> | 能力 | 旧版 check_network.sh | 方案A v2 check_network.sh |
> |---|---|---|
> | 网络检测 | 有（ping/curl测试） | 无 |
> | SIM↔Roamlink 切换 | 有（调用nc命令） | 无 |
> | 保活重启 dial | 有 | 有 |
> | 通道选择逻辑 | 在脚本中 | 完全移入 dial 进程内部 |
> | 配置依赖 | `/opt/conf.ini`、`/tmp/network_type` | 无（只依赖 pgrep） |
> 
> 方案A v2 的设计哲学：所有通道切换智能集中在 `dial` 进程的状态机里，Shell 脚本只做最简单的"死了就拉起来"。这样的好处是调试、测试、升级都更简单，不存在 Shell 和 C 程序之间的状态同步问题。

---

# 第八部分：install_update.sh（安装/升级脚本）

```sh
LOG_FILE="/usrdata/roamlink_install.log"
FORCE_INSTALL=0
```

安装日志存到持久化分区。`FORCE_INSTALL=0` 默认为非强制模式（已安装时只升级 dial，不重装 Roamlink）。

```sh
fail_exit() {
    log "ERROR: $*"
    exit 255
}

check_cmd() {
    "$@" || fail_exit "command failed: $*"
}
```

错误处理函数。`check_cmd` 执行命令，失败就打日志并退出（`exit 255`）。  
`"$@"` 展开为所有参数，如 `check_cmd cp -f src dst` 展开为 `cp -f src dst || fail_exit "cp -f src dst"`。

### 已安装判断（三分支）

```sh
if [ -d /usrdata/roamlink ]; then
    if [ $FORCE_INSTALL -eq 1 ]; then
        log "FORCE_INSTALL=1: removing existing roamlink"
        rm -rf /usrdata/roamlink     # 删除旧Roamlink目录
        rm -f /data/ufs/license.cer  # 删除备份license
        log "existing installation cleared"
        # 继续走首次安装流程
    else
        # 纯升级模式
        check_cmd cp -f ./dial_1.29.7 /usr/dial/dial
        chmod +x /usr/dial/dial

        for f in check_network.sh network.ini; do
            check_cmd cp "./$f" /usrdata/
        done
        chmod +x /usrdata/check_network.sh

        sync
        reboot
    fi
fi
```

**升级模式**只替换三个文件：
- `dial_1.29.7` → `/usr/dial/dial`（新版本程序）
- `check_network.sh` → `/usrdata/check_network.sh`（看门狗脚本可能也更新了）
- `network.ini` → `/usrdata/network.ini`（配置文件可能有新字段）

**不动 Roamlink 目录和 license**：保留已有授权，不清除。

### 固件版本检查

```sh
./check_fw_version
fw_version=$?
case $fw_version in
0)   log "firmware needs dfota upgrade" ;;     # 需要固件升级
1)   log "firmware OK, no dfota needed" ;;     # 固件合适，直接安装
2)   fail_exit "unsupported firmware" ;;       # 固件不支持，退出
255) fail_exit "failed to read firmware" ;;    # 读取失败，退出
esac
```

`check_fw_version` 是升级包里的二进制程序，通过 AT 命令检查模组固件版本，返回码表示结果。

### IMEI 读取和 License 预置

```sh
imei=$(./get_imei) || fail_exit "failed to read IMEI"
log "IMEI: $imei"

mkdir -p /data/ufs

license_name="${imei}_license.cer"
license_src="licenses/${license_name}"

if [ -s "${license_src}" ]; then
    # [ -s file ]：文件存在且非空
    check_cmd cp -f "${license_src}" ./roamlink/etc/.pconfig/license.cer
    check_cmd cp -f "${license_src}" /data/ufs/license.cer
    sync
    log "license pre-installed (U-disk mode): ${license_name}"
else
    log "no license found for this IMEI — will auto-download (OTA mode)"
fi
```

**自适应逻辑**：同一套升级包可以用于有 license 和没有 license 的场景：
- 有对应 IMEI 的 license 文件（U盘出厂）：预置到 Roamlink 主路径和备份路径，重启后直接可用
- 没有（OTA 场景）：跳过预置，重启后 dial 走 license_pending 模式，由 RBMaster 云端下载

`mkdir -p /data/ufs`：`-p` 表示如果目录已存在不报错，如果父目录不存在也一并创建。

### 安装文件

```sh
if [ "$fw_version" -eq 1 ]; then
    ln -svf /etc/init.d/start_check_network /etc/rc5.d/S60start_check_network
    unlink /etc/rc5.d/S20start_dial 2>/dev/null    # 禁用旧的dial启动项
    unlink /etc/rc5.d/S20start_vsim 2>/dev/null    # 禁用旧的vsim启动项
fi

check_cmd cp -af ./roamlink /usrdata          # 复制Roamlink组件目录
check_cmd cp -f ./dial_1.29.7 /usr/dial/dial  # 安装dial程序

for f in network.ini test_network check_network.sh; do
    check_cmd cp "./$f" /usrdata/
done
chmod +x /usrdata/test_network /usrdata/check_network.sh

check_cmd cp ./start_check_network /etc/init.d/
chmod +x /etc/init.d/start_check_network
```

`ln -svf`：创建符号链接（`-s`），显示详情（`-v`），强制覆盖（`-f`）。  
`cp -af`：`-a` 保留原始属性（权限、时间戳等），`-f` 强制覆盖。复制整个 roamlink 目录。  
`unlink /etc/rc5.d/S20start_dial 2>/dev/null`：删除旧的启动项，`2>/dev/null` 忽略"文件不存在"的错误。

### 屏蔽旧看门狗

```sh
if [ -f /opt/start_daemon.sh ]; then
    sed -i.bak '/^[[:space:]]*check_dial[[:space:]]*$/ s/^/#/' /opt/start_daemon.sh
fi
```

`sed -i.bak`：直接编辑文件（`-i`），备份为 `.bak`。  
正则 `/^[[:space:]]*check_dial[[:space:]]*$/`：匹配单独一行的 `check_dial`（允许前后有空格）。  
`s/^/#/`：在行首加 `#` 注释掉，屏蔽旧版看门狗的 `check_dial` 调用。  
**原因**：旧版 `start_daemon.sh` 会调用 `check_dial` 来管理 dial 进程，和新版 `check_network.sh` 冲突（两个看门狗同时管理同一个进程会造成混乱）。

```sh
sync
sleep 2
reboot
```

最后 `sync` 确保所有文件写到闪存，`sleep 2` 等 I/O 完成，`reboot` 重启应用新安装的程序。

> **补充分析（来自v2分析文档）**：
> 
> **`FORCE_INSTALL` 标志的使用场景**：
> 
> | 场景 | `FORCE_INSTALL` | 行为 |
> |---|---|---|
> | 正常升级（保留license） | 0（默认） | 只替换 dial + check_network.sh + network.ini，不动 Roamlink |
> | 强制重装（清除license） | 1 | 删除整个 `/usrdata/roamlink/` 目录和 `/data/ufs/license.cer`，全新安装 |
> | 出厂烧录 | 1 | 确保每台设备从干净状态开始，不会复用其他设备的 license |
> 
> 在部署脚本里设置 `FORCE_INSTALL=1` 时，会删除备份 license（`/data/ufs/license.cer`），这意味着重启后 `roamlink_license_restore_from_backup()` 无法恢复，设备必须重新走 OTA 下载流程。
> 
> **安装后启动链路**：
> 
> ```
> 开机
> └─ /etc/rc5.d/S60start_check_network（符号链接）
>    └─ /etc/init.d/start_check_network
>      └─ 执行 /usrdata/check_network.sh &
>         └─ 检查并启动 /usr/dial/dial
>            └─ dial 进程：dial_mng_new() → dial_task() 线程
> ```
> 
> `S60` 前缀表示启动顺序60，确保在网络相关服务（通常 S20~S50）之后启动。`unlink S20start_dial` 是删除旧版直接启动 dial 的脚本，改为由 check_network.sh 统一管理。

---

---

# 附录：切换逻辑横切面分析

> 以下内容来自《SIM_Roamlink_切换逻辑深度分析》，属于跨函数的主题性分析，补充进正文对应位置后此处单独归档。

---

## 附录A：切换路径全图

所有触发点与状态转移方向：

```
                    ┌─────────────────────┐
                    │                     │
         B(回切)    ▼                     │
  Roamlink ←── net_connected ─── A ──→  roamlink_starting ──→ roamlink_active
  starting         (SIM通道)  TCP失败×3   (Roamlink等待)      (Roamlink稳定)
     │              │                          │ E(超时)            │ F(TCP失败×3)
     │              │ C(rx超时)                │                    │ G(回切SIM)
     │              ▼                          │                    │ H(业务层死亡)
     │          reg_check                      │                    │
     │          (重新拨号)                      ▼                    ▼
     │                                    net_connected      net_connected
     │                                    (SIM通道)          (SIM通道)
     │
     └── D(连通) → roamlink_active
     └── E(超时，策略1/2) → net_connected(SIM)
     └── 策略3超时 → 重启Roamlink(停止+重启,留在roamlink_starting)
```

---

## 附录B：Roamlink→SIM 切换时的 `sim_initialized` 判断

每个 Roamlink→SIM 切换路径都有这段代码：

```c
if (p_dial_mng->sim_initialized)
{
    p_dial_mng->is_func_called = true;
    p_dial_mng->dial_st = dial_stat_sim_init;
}
else
{
    p_dial_mng->dial_st = dial_stat_init;
}
```

**`sim_initialized = false` 的情况**：策略1或3开机，`dial_stat_none` 直接跳 `roamlink_starting`，完全没走过 SIM 路径，`sim_initialized` 一直是 false。第一次切回 SIM 需要完整初始化。

**`sim_initialized = true` 的情况**：只要走到过 `dial_stat_init` 里的 `QL_MCM_NW_Client_Init()`，就置为 true。之后无论多少次切换，都保持 true，走 `sim_init` 重入路径。

**`is_func_called = true` 的作用**：控制 `dail_start_data_call()` 里是否重新调用 `QL_Data_Call_Init()`（注册回调函数）。每次 SIM 拨号前都需要重新注册，所以切回 SIM 时必须置 `true`。

---

## 附录C：策略1完整切换时序

```
策略1（PREFER_ROAMLINK）完整切换时序：

开机
  │
  ▼ [dial_stat_none]
  启动 RBMaster（sleep 3s 等初始化）
  发 AT+COPS=0
  发 RBstartServiceMaster → RBMaster 接管模组
  roamlink_timer = now
  进入 roamlink_starting
  │
  ▼ [roamlink_starting] 每10s测一次TCP
  TCP通过 → 进入 roamlink_active
  (读取rx_packets基准值，重置roamlink_no_data_timer)
  │
  ▼ [roamlink_active] 每30s测一次TCP
  ┌─── 正常运行 ───────────────────────────┐
  │  TCP通过，rx_packets增长，继续          │
  └─────────────────────────────────────────┘
  │
  │ TCP失败 count=1,2,3 → 触发切换
  ▼
  [切换执行]
  1. roamlink_stop_service()
     ├── 发 RBstopServiceMaster
     ├── 写 network_type=0
     └── sleep(3)  ← 等模组注销虚拟SIM

  2. p_apn_obj = NULL

  3. sim_fallback_timer = now  ← 记录进入SIM备用时刻

  4. tcp_test_timer = now

  5. sim_initialized?
     ├── false: dial_st = dial_stat_init（完整初始化）
     └── true:  is_func_called=true, dial_st = dial_stat_sim_init（快速重入）
  │
  ▼ [SIM拨号流程]
  sim_init → sim_check → sim_op(读ICCID) → reg_check → cereg_check
  → precondition_check → pre_start_call(查APN) → start_call → wait_for_connect
  │
  ▼ [net_connected]
  每60s测一次TCP
  ┌─── 正常运行（SIM备用中） ──────────────┐
  │  TCP通过，                              │
  │  检查: sim_fallback_timer.tv_sec > 0？  │
  │     && 距sim_fallback_timer >= 300s?    │
  │     → 是：尝试回切Roamlink              │
  └─────────────────────────────────────────┘
  │ 等到300s后
  ▼ [主动回切]
  dail_stop_data_call()
  p_apn_obj = NULL
  roamlink_start_master()  ← 确保RBMaster活着
  roamlink_start_service()
  roamlink_timer = now
  roamlink_fail_count = 0
  dial_st = roamlink_starting
  │
  ▼ 重新尝试 Roamlink 连通...
```

---

## 附录D：三个关键防抖机制

### D.1 失败计数防抖

SIM 通道和 Roamlink 通道都有连续失败3次才切换的机制：

```
为什么不单次失败就切换？
  → 网络偶尔抖动、服务器临时不响应，单次失败很常见
  → 单次切换会造成频繁无意义的通道来回切换
  → 3次连续失败确保是真正的持续故障
    SIM通道：3次 × 60秒间隔 = 最快180秒才切换
    Roamlink通道：3次 × 30秒间隔 = 最快90秒才切换

中间任何一次成功，计数清零：
  tcp_fail_count = 0       (TCP通过时)
  roamlink_fail_count = 0  (Roamlink TCP通过时)
```

### D.2 300秒稳定等待防抖

回切时等待300秒，确保备用通道稳定后再尝试回主通道：

```
为什么不立刻回切？
  → 主通道可能只是短暂恢复，马上回去又会失败
  → 频繁切换会造成业务中断
  → 300秒给主通道足够的恢复时间

回切失败的处理：
  → 回切尝试时发 RBstartServiceMaster
  → 如果 Roamlink 300秒内连不通（roamlink_starting 超时）
  → 自动切回 SIM，sim_fallback_timer 被重置
  → 再等300秒后再尝试
```

### D.3 `did_switch` 互斥防抖

在 `roamlink_active` 状态的 TCP 通过分支里：

```c
bool did_switch = false;
// 策略2回切（可能触发切换）
if (!did_switch && ...) { ...; did_switch = true; }
// 业务层检测（可能触发切换）
if (!did_switch && ...) { ... }
```

防止同一次循环里两个独立的切换逻辑都触发，造成 `roamlink_stop_service()` 被调用两次、`dial_st` 被覆盖两次的问题。

---

## 附录E：切换过程断网时间分析

### SIM → Roamlink

```
断网开始：dail_stop_data_call() 被调用
断网结束：nw_tcp_connectivity_test() 在 roamlink_starting 状态首次通过

中间过程：
  RBMaster 启动（如未运行：sleep 3s）
  AT+COPS=0（约1~2s）
  RBstartServiceMaster（RBMaster 接管模组，建立虚拟SIM，约10~60s不等）
  TCP 测试通过（最快1~2s，最慢等到下一次10s间隔）

实测断网时间：约 22~107 秒
  最快：RBMaster 已在运行，Roamlink 连通快：约22s
  最慢：RBMaster 需要启动 + Roamlink 慢：约107s
```

### Roamlink → SIM

```
断网开始：roamlink_stop_service() 被调用
断网结束：net_connected 状态建立（SIM 拨号成功）

中间过程：
  RBstopServiceMaster + sleep(3)（3s）
  SIM 拨号状态机：sim_init → sim_check → sim_op → reg_check
    → cereg_check → precondition_check → pre_start_call → start_call
    → wait_for_connect（整个流程约 15~60s）

实测断网时间：约 22~75 秒
  快速路径（sim_initialized=true）：约22s（从sim_init重入）
  完整路径（sim_initialized=false）：约40~75s
```

---

## 附录F：AT串口互斥锁在切换时的风险

`roamlink_start_service()` 内部发 `AT+COPS=0` 时需要持有 `g_at_port_mutex`：

```c
bool roamlink_start_service(int smd_fd)
{
    pthread_mutex_lock(&g_at_port_mutex);
    Ql_SendAT(smd_fd, "AT+COPS=0", "OK", 5000, rsp_msg);  // 超时5秒
    pthread_mutex_unlock(&g_at_port_mutex);
    ...
}
```

同时主线程每5秒发 `AT+CSQ`：

```c
// main.c 主线程
while (true) {
    pthread_mutex_lock(&g_at_port_mutex);  // 可能等待
    csq = nw_at_get_csq(smd_fd);
    pthread_mutex_unlock(&g_at_port_mutex);
    sleep(5);
}
```

**影响**：dial_task 切换时持有锁发 `AT+COPS=0`（超时5秒），主线程的 CSQ 查询会被阻塞最多5秒，导致 CSQ 字段更新延迟。反之，AT+CSQ 超时1秒，AT+COPS=0 最多等待1秒才能获取锁。

**结论**：这是已知的设计权衡，不影响功能，但需注意切换期间 CSQ 上报可能短暂滞后。

---

# 附录G：切换计时器管理总结（来自切换逻辑深度分析 §4.4）

> 切换时涉及多个计时器的重置，一旦漏掉可能导致下一轮检测时机不对。

**SIM → Roamlink 切换时：**

| 计时器 | 操作 | 原因 |
|--------|------|------|
| `roamlink_timer` | 设为当前时间 | Roamlink 开始等待连通，计时从0开始 |
| `roamlink_fail_count` | 清零 | 新一轮 Roamlink 连通尝试，失败计数归零 |
| `roamlink_fallback_timer` | 设为当前时间 | 记录进入 Roamlink 备用的时刻（策略2回切用）|
| `p_apn_obj` | 置 NULL | 强制下次 SIM 拨号重新查 APN |

**Roamlink → SIM 切换时：**

| 计时器 | 操作 | 原因 |
|--------|------|------|
| `tcp_test_timer` | 重置为当前时间 | SIM 刚联网，TCP 测试从当前时刻开始，不要立刻触发 |
| `tcp_fail_count` | 清零 | 新一轮 SIM 连通监控，失败计数归零 |
| `sim_fallback_timer` | 设为当前时间 | 记录进入 SIM 备用的时刻（策略1回切用）|
| `p_apn_obj` | 置 NULL | 强制重新查 APN |

---

# 附录H：最新源码修复记录（commit 9d15833，2026-04-10）

> 此批修复在所有分析文档写成之后合入，原文档描述的是旧行为，以下为准。

## H.1 main.c — 互斥锁双重加锁死锁（已修复）

**旧代码（有 Bug）：**
```c
// main() 主循环
pthread_mutex_lock(&g_at_port_mutex);   // ← 外层加锁
csq = nw_at_get_csq(p_dial_mng->smd_fd);  // 内部也会 lock，同线程 double-lock → 死锁
pthread_mutex_unlock(&g_at_port_mutex);
```

**修复后：**
```c
// nw_at_get_csq 内部已持 g_at_port_mutex，外层不能再加锁（同线程double-lock死锁）
csq = nw_at_get_csq(p_dial_mng->smd_fd);
```

> **附录F 中"已知设计权衡，目前没有问题"的描述不再准确**。double-lock 死锁是真实 Bug，已在此 commit 修复。AT互斥锁机制现在是正确的：`nw_at_get_csq()` 内部自行持锁，主线程不再外层加锁。

## H.2 dial.h — 新增注册超时常量

```c
/*
 * 注册超时阈值（覆盖 reg_check + cereg_check 两个阶段）
 * 物理SIM被运营商禁卡时，AT+CEREG 返回 stat=3 或 stat=2，无限重试。
 * 此超时触发后根据策略切换通道或 cfun 重置。
 * 300秒（5分钟）：正常注册通常 10~60 秒，5分钟足够区分"暂时慢"和"真的被禁"。
 */
#define REG_CHECK_TIMEOUT_SECONDS (300)
```

## H.3 dial.c — 注册超时切换策略（reg_timeout_handler）

`dial_timer` 在以下每个跳转到 `reg_check` 的位置被重置为当前时间作为超时起点：
- `sim_op_stat_get_iccid` 成功后
- `net_connected` 断线后
- `dial_stat_net_connected` TCP失败切换后
- `dial_stat_net_connected` rx_packets 超时后
- `dial_stat_roamlink_starting` / `dial_stat_roamlink_active` 超时切换后

`reg_check` 和 `cereg_check` 失败时共享同一 `dial_timer` 超时检查：

```c
dif_timer = diff(p_dial_mng->dial_timer, cur_timer);
if (dif_timer.tv_sec > REG_CHECK_TIMEOUT_SECONDS)
{
    goto reg_timeout_handler;
}
```

`reg_timeout_handler` 处理逻辑：

```c
reg_timeout_handler:
    nw_mark_network_status(0);

    if ((policy == PREFER_ROAMLINK || policy == PREFER_SIM)
        && roamlink_available && !license_pending)
    {
        // 策略1/2 + Roamlink可用：切换到 Roamlink 通道
        dail_stop_data_call();
        p_apn_obj = NULL;
        roamlink_start_master();
        roamlink_start_service(smd_fd);
        roamlink_timer = cur_timer;
        roamlink_fallback_timer = cur_timer;
        roamlink_fail_count = 0;
        dial_st = dial_stat_roamlink_starting;
    }
    else
    {
        // 策略4 / roamlink不可用 / license_pending：cfun重置后重试
        is_func_called = true;
        dial_st = dial_stat_stop_cfun;
    }
```

## H.4 dial.c — 断线重拨卡死（已修复）

**旧代码（有 Bug）：**
```c
// net_connected 检测到断线
nw_mark_network_status(0);
dial_st = dial_stat_reg_check;
tcp_fail_count = 0;
// ← 未重置 is_func_called，dail_start_data_call() 门控拦截，无法重拨
```

**修复后：**
```c
nw_mark_network_status(0);
p_dial_mng->dial_timer = cur_timer;  // 注册超时起点
p_dial_mng->is_func_called = true;   // 重置拨号门控：断线后重新走 QL_Data_Call_Init+Start
dial_st = dial_stat_reg_check;
tcp_fail_count = 0;
```

## H.5 dial.c — DIAL_TIMEOUT_SECONDS 超时清理（已修复）

**旧代码（有 Bug）：**
```c
// net_connected rx_packets 超时
dial_st = is_oper_select ? dial_stat_list_oper : dial_stat_reg_check;
// ← 未先停止底层数据呼叫，Quectel框架保留悬挂呼叫，下次 start_call 冲突
```

**修复后：**
```c
dail_stop_data_call(profile_idx);    // 先清理悬挂呼叫
p_dial_mng->is_func_called = true;  // 重置门控
dial_st = is_oper_select ? dial_stat_list_oper : dial_stat_reg_check;
if (dial_st == dial_stat_reg_check) {
    pthread_mutex_lock(&g_at_port_mutex);   // ← 新增：正确持锁
    Ql_SendAT(smd_fd, "AT+COPS=0", "OK", 180000, rsp_msg);
    pthread_mutex_unlock(&g_at_port_mutex);
    p_dial_mng->dial_timer = cur_timer;     // AT命令完成后才开始计算注册超时
}
```

---

# 附录I：两仓库协作关系（来自v2分析报告 §7）

```
                开发阶段
open_dial_for_artery（源码）
        │
        │  Yocto 交叉编译：arm-oe-linux-gnueabi-gcc + Quectel MCM SDK
        ▼
    dial_1.29.x（ARM 32-bit ELF）
        │
        │  拷贝到 roamlink 部署包
        ▼
    roamlink_deploy/
        ├── dial_1.29.x  ← 物理SIM拨号程序
        └── roamlink/    ← 虚拟SIM服务（闭源，来自RoamLink平台）

                部署阶段（设备现场）
    install_update.sh（U盘执行）
        │
        ├─ 检测固件版本 → 必要时 DFOTA 升级
        ├─ 验证 IMEI → 安装匹配的 license.cer
        ├─ 部署 dial → /usr/dial/dial
        ├─ 部署 roamlink/ → /usrdata/roamlink/
        ├─ 部署 check_network.sh → /usrdata/
        ├─ 注册 start_check_network 为开机自启动服务
        └─ 注释掉原始 check_dial 调用（避免冲突）

                运行阶段
    /etc/rc5.d/S60start_check_network
        │
        ▼
    /usrdata/check_network.sh（永久运行，纯watchdog）
        │
        └─ 每30s检查 dial 是否存活，不在则重启

        物理SIM通道              虚拟SIM通道
    /usr/dial/dial          /usrdata/roamlink/RBMaster
        │                           │
        │   共享 Quectel 模组        │
        │   (同一时间只有一个激活)    │
        └──────────── 互斥 ──────────┘
                     │
    /tmp/network_status（1=联网，0=断网）
    /tmp/network_type（1=SIM，2=Roamlink，0=切换中）
    /tmp/network_csq（CSQ值，每5秒更新）

这三个临时文件是两仓库之间的状态共享接口，上层应用读 network_status 判断联网状态。
```

---

# 附录J：已知 Bug 完整列表（含v2报告新增项）

> 综合 CLAUDE.md 已知缺陷 + v2报告逐条详解，以源码实际为准。

## Bug 1：at.c — sizeof 指针错误（严重）

```c
// 位置：Ql_SendAT() 第12-13行
iLen = sizeof(atCmd);      // 错误！atCmd 是 char*，sizeof = 4（ARM32）
strncpy(strAT, atCmd, iLen);  // 只复制前 4 字节

// 为什么没崩溃：后面的 sprintf 覆盖了 strAT，修正了错误
// 隐患：若 AT 命令本身以 \r 或 \n 结尾（不走 sprintf 分支），发送内容只有前4字节

// 修复：
iLen = strlen(atCmd);
strncpy(strAT, atCmd, iLen);
strAT[iLen] = '\0';
```

## Bug 2：apn.c — apn_scan_idx() 提前 return（死代码）

```c
// 位置：apn_scan_idx() 第4行
return apn_set(1, p_apn_obj);  // 提前 return，后续70行 APN 管理逻辑永远不执行
// 设计原因（V1.29.4备注）：高通模组只对 profile_idx=1 自动设置路由，强制使用
```

## Bug 3：apn.c — 赋值写成比较（死代码区，不执行）

```c
if (ret = -1)   // 应为 ret == -1，赋值结果为 true，if 分支永远执行
```

## Bug 4：sim.c — switch-case 缺少 break（fall-through）

```c
case sim_op_stat_get_iccid:
    // ... 获取 ICCID ...
    // ← 缺少 break！
case sim_op_stat_get_imsi:  // fall-through 到此，同时读 IMSI
```
实际效果：同时执行 GET_ICCID 和 GET_IMSI。当前无副作用（IMSI 未被使用）。

## Bug 5：apn.c — fread 失败路径内存泄漏

```c
str = malloc(len);
if (fread(str, 1, len, fp) < 0) {
    return NULL;   // str 未 free，fp 未 fclose
}
// 修复：free(str); fclose(fp); return NULL;
```

## Bug 6：nw.c — nw_get_ifaddrs() 悬空指针

```c
memcpy(p_ifaddrs, ifa, sizeof(struct ifaddrs));  // 浅拷贝，ifa_name 指向原链表
freeifaddrs(ifaddr);   // 释放原链表，ifa_name 变成悬空指针
return p_ifaddrs;      // 返回含悬空指针的结构体，访问即 UB
// 修复：p_ifaddrs->ifa_name = strdup(ifa->ifa_name);
```

## Bug 7：nw.c — nw_mark_network_status() 文件操作不规范

```c
fd = open(NW_STATUS_PATH, O_WRONLY | O_CREAT);
// 缺 mode 参数（O_CREAT 必须提供）、缺 O_TRUNC、未检查 fd<0
// 修复：open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644); + fd检查
```

## Bug 8：apn.c — apn_get_apn_obj() 日志逻辑取反

```c
if (p_apn_obj == NULL) {
    // 未找到，使用默认APN（正确）
} else {
    SEAS_LOG_ERROR("p_apn_obj is NULL");  // 错误！找到了却打印"is NULL"
}
// 修复：else { SEAS_LOG_INFO("Found APN: %s", p_apn_obj->apn); }
```

## Bug 9：check_network.sh — set -e 与守护进程不兼容

```bash
set -e  # 任何命令返回非0，脚本立即退出
# 问题：nc 连接 5568 端口失败（RBMaster未启动时）会触发 set -e，导致守护进程退出
# 修复：去掉 set -e，用 trap 捕获意外退出并重启
trap 'sleep 10; exec $0' EXIT
```

---

# 附录K：安全漏洞分析（来自v2报告 §9）

## 漏洞1：RBMaster 本地控制接口无认证（中危）

```
攻击面：本地 TCP 127.0.0.1:5568，无任何认证机制
影响：任何能执行 shell 的进程均可发送 "RBstopServiceMaster" 导致断网
      echo "RBstopServiceMaster" | nc 127.0.0.1 5568
改进：改用 Unix domain socket + 700 权限，或增加预共享 token 认证
```

## 漏洞2：test_network 硬编码 IP + 明文 TCP（低危）

```
18.196.0.17 无 TLS，无证书验证
若攻击者劫持路由，可伪造"连接成功"阻止切换，或拒绝连接导致频繁切换
改进：使用 HTTPS + 证书验证，或多目标 ICMP ping 投票
```

## 漏洞3：license.cer 明文存储（低危）

```
存放于 /usrdata/roamlink/etc/.pconfig/license.cer 和 /data/ufs/license.cer
可被 shell 访问者复制，但 license 与 IMEI 绑定，复制到其他设备无效
改进：额外加密，密钥与 CPU 序列号绑定
```

## 漏洞4：DFOTA 触发无额外验证（中危）

```
/usrdata/update.zip 存在即可触发固件升级，无签名验证
攻击路径：获取 shell → 替换 update.zip → 触发 DFOTA → 刷入恶意固件
改进：验证 update.zip 签名，限制 /usrdata 写入权限
```

## 漏洞5：nw.c system() 调用潜在注入（理论风险）

```c
sprintf(echo_cmd, "echo %s%s > %s", mcc, mnc, NW_PLMN_PATH);
system(echo_cmd);
// mcc/mnc 来自 AT+COPS 响应，恶意基站返回特殊字符可注入 shell 命令
// 实际风险低（正规运营商不注入），但修复成本极低
// 修复：直接 open()/write() 写文件，不经 shell
```

---

## 附录L：修复代码现状核查（基于最新源码 V1.29.07，提交 9d15833）

> v2分析文档第10章列出6项修复建议。以下逐条核对最新源码实际状态，区分**已修复**、**仍存在**、**不适用**三类。

### 修复状态总览

| 项 | v2建议修复点 | 文件 | 最新源码状态 |
|----|------------|------|------------|
| L.1a | Bug1：`sizeof` 用于指针（应用`strlen`） | `src/at/at.c:28` | ✅ **已修复**（源码已用`strlen`） |
| L.1b | Bug2：超时换算（ms→tv_sec+tv_usec） | `src/at/at.c:24-25` | ✅ **已修复**（源码已正确换算） |
| L.1c | Bug3：`memcpy(p_rsp_msg, strResponse, rdLen)` 无长度检查 | `src/at/at.c:78` | ❌ **仍存在** |
| L.2 | Bug6：`nw_get_ifaddrs()` 浅拷贝后 `freeifaddrs()` 导致悬空指针 | `src/nw/nw.c:334-347` | ❌ **仍存在** |
| L.3 | Bug5：`apn_scan_from_json()` fread失败路径内存泄漏 | `src/apn/apn.c:130-134` | ✅ **已修复** |
| L.4 | Bug7+漏洞5：`system("echo %d > ...")`/`system("echo %s%s > ...")` | `src/nw/nw.c:106,159` | ❌ **仍存在** |
| L.5 | Bug9：`check_network.sh` 使用 `set -e` | 部署包 | ✅ **不适用**（当前版本已是纯watchdog，无`set -e`） |
| L.6 | Bug4：`sim_op_stat_get_iccid` case 缺 `break` | `src/sim/sim.c:202` | ✅ **已修复** |

---

### L.1 at.c — Bug1/Bug2 已修复，Bug3 仍存在

**Bug1（strlen）和 Bug2（超时换算）**：当前源码 `src/at/at.c` 第28行已用 `strlen(atCmd)`，第24-25行已正确设置 `tv_sec`/`tv_usec`。**无需操作**。

**Bug3（memcpy 无界拷贝）**：当前源码第76-79行仍存在：

```c
// 当前源码（有问题）：src/at/at.c:76-79
if (p_rsp_msg != NULL)
{
    memcpy(p_rsp_msg, strResponse, rdLen);  // rdLen 最大为 AT_MSG_LENGTH_MAX，调用方缓冲区可能更小
}
```

修复方式：限制拷贝长度，防止写越界：

```c
// 修复后：
if (p_rsp_msg != NULL) {
    size_t copy_len = (rdLen < AT_MSG_LENGTH_MAX - 1) ? rdLen : AT_MSG_LENGTH_MAX - 1;
    memcpy(p_rsp_msg, strResponse, copy_len);
    p_rsp_msg[copy_len] = '\0';
}
```

---

### L.2 nw.c — `nw_get_ifaddrs()` 悬空指针仍存在

当前源码 `src/nw/nw.c:334-347`：

```c
// 当前源码（有问题）：
p_ifaddrs = (struct ifaddrs *)calloc(1, sizeof(struct ifaddrs));
memcpy(p_ifaddrs, ifa, sizeof(struct ifaddrs));  // 浅拷贝：ifa_name 指向原链表内存
SEAS_LOG_INFO("get ifaddrs with name: %s\n", p_ifaddrs->ifa_name);
break;
// ...
freeifaddrs(ifaddr);  // 释放后 p_ifaddrs->ifa_name 成悬空指针
return p_ifaddrs;     // 调用方拿到的 ifa_name 是野指针
```

修复方式：`strdup()` 深拷贝接口名，调用方负责 `free(p_ifaddrs->ifa_name); free(p_ifaddrs);`：

```c
// 修复后：
p_ifaddrs = (struct ifaddrs *)calloc(1, sizeof(struct ifaddrs));
if (p_ifaddrs == NULL) break;
if (ifa->ifa_name) {
    p_ifaddrs->ifa_name = strdup(ifa->ifa_name);  // 深拷贝，不依赖原链表内存
}
SEAS_LOG_INFO("get ifaddrs with name: %s\n", p_ifaddrs->ifa_name);
break;
// ...
freeifaddrs(ifaddr);  // 安全，p_ifaddrs->ifa_name 是独立副本
return p_ifaddrs;
```

---

### L.3 apn.c — Bug5 已修复（无需操作）

当前源码 `src/apn/apn.c:130-134` 已正确处理 fread 失败路径：

```c
if (fread(str, 1, len, fp) != (size_t)len) {
    SEAS_LOG_ERROR("fread error");
    free(str);    // 已释放
    fclose(fp);   // 已关闭
    return NULL;
}
```

---

### L.4 nw.c — `system()` 命令注入仍存在

当前源码中三处仍使用 `system()` 或不规范的文件写入：

```c
// src/nw/nw.c:105-106  nw_mark_plmn()
sprintf(echo_cmd, "echo %s%s > %s", mcc, mnc, NW_PLMN_PATH);
system(echo_cmd);   // mcc/mnc 来自 AT+COPS 响应，理论上可注入 shell 命令

// src/nw/nw.c:157-159  nw_at_get_csq()
sprintf(echo_cmd, "echo %d > %s", csq, NW_CSQ_PATH);
system(echo_cmd);   // csq 为整数，注入风险低，但仍走 shell

// src/nw/nw.c:173  nw_mark_network_status()
fd = open(NW_STATUS_PATH, O_WRONLY | O_CREAT);  // 缺 O_TRUNC，旧内容可能残留
```

修复方式：以 `open()/write()` 替换所有三处，同时修复 `O_TRUNC` 缺失：

```c
// 新增通用辅助函数（放在 nw.c 文件顶部）：
static void write_int_to_file(const char *path, int value)
{
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d", value);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { SEAS_LOG_ERROR("open %s: %s", path, strerror(errno)); return; }
    write(fd, buf, len);
    close(fd);
}

static void write_str_to_file(const char *path, const char *str)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { SEAS_LOG_ERROR("open %s: %s", path, strerror(errno)); return; }
    write(fd, str, strlen(str));
    close(fd);
}

// 替换 nw_mark_plmn()：
void nw_mark_plmn(char *mcc, char *mnc)
{
    char plmn[16] = {0};
    snprintf(plmn, sizeof(plmn), "%s%s", mcc, mnc);
    write_str_to_file(NW_PLMN_PATH, plmn);
}

// 替换 nw_at_get_csq() 中的 system() 段：
write_int_to_file(NW_CSQ_PATH, csq);

// 替换 nw_mark_network_status()：
void nw_mark_network_status(int net_status)
{
    write_int_to_file(NW_STATUS_PATH, net_status);
}
```

---

### L.5 check_network.sh — Bug9 不适用

当前部署包 `check_network.sh` 已是纯看门狗版本（方案A v2），无 `set -e`，无切换逻辑。该问题在旧版脚本中存在，新版本重写时已消除，**无需操作**。

---

### L.6 sim.c — Bug4 已修复（无需操作）

当前源码 `src/sim/sim.c:202` 已有 `break`，fall-through 问题已消除。

---

## 附录M：背景概念——物理SIM与虚拟SIM（来自v2分析文档第1章）

### M.1 物理SIM（Physical SIM）

物理 SIM 就是插在设备模组卡槽里的实体卡，由运营商签发，具有固定的 ICCID 和 IMSI。

```
物理SIM流程:
  实体SIM卡 → 插入模组卡槽 → 模组读取 ICCID/IMSI
           → 向运营商基站注册 → 获得 IP 地址 → 上网
```

**优点**：稳定可靠，适合单一国家/地区固定场景  
**缺点**：跨国漫游费高；SIM 卡损坏需现场更换；部分国家运营商信号弱时没有备选

`open_dial_for_artery` 是管理物理 SIM 拨号的程序。

### M.2 虚拟SIM（Virtual SIM / Soft SIM）

虚拟 SIM（本项目称 Soft SIM 或 softsim）用软件模拟 SIM 卡功能，通过互联网连接 RoamLink 云端平台，在云上完成 USIM 鉴权，设备可动态切换运营商，无需更换物理卡。

```
虚拟SIM流程（RoamLink方案）:
  softsim 进程（libTSSUMEmulator.so）
           ↕ 本地模拟 USIM 协议
  Quectel 模组（EC2x/EG2x）
           ↕ 通过 TCP/IP 回传鉴权数据
  RoamLink 云平台（远端运营商服务器）
           → 动态选择当前最优运营商
           → 返回运营商鉴权响应
           → 设备以该运营商身份注册网络 → 上网
```

**优点**：自动选择全球最优运营商；无需换卡；适合跨国设备  
**缺点**：初次上线依赖物理 SIM 或已有网络；依赖 license 授权；RBMaster 闭源

### M.3 两者在本项目中的关系

```
┌──────────────────────────────────────────────────────┐
│              Quectel EC2x/EG2x 模组                   │
│                                                      │
│   物理SIM卡槽                虚拟SIM接口              │
│   (实体SIM卡)                (软件模拟USIM)           │
│        │                          │                 │
│   dial 进程（open_dial）     softsim + RBMaster      │
│        │                          │                 │
│        └────────── 二选一 ─────────┘                 │
│          由 dial 内部状态机决策（方案A v2）           │
└──────────────────────────────────────────────────────┘
```

两者**不能同时激活**，共用同一个模组无线接口。当前方案A v2 中，切换决策完全在 `dial_task` 状态机内部完成（参见正文状态机部分及附录A-F）。

---

## 附录N：open_dial_for_artery 编译指南（来自v2分析文档第4章）

### N.1 前提条件

编译 open_dial 需要 Quectel OpenLinux SDK（针对 EC2x/EG2x），通常由 Quectel 提供给设备厂商，不公开。

### N.2 环境准备

```bash
# 1. 安装 Yocto/OpenEmbedded 交叉编译工具链
# （Quectel 会提供工具链安装包，例如 poky-glibc-x86_64-*.sh）
chmod +x poky-glibc-x86_64-*.sh
./poky-glibc-x86_64-*.sh   # 默认安装到 /opt/poky/...

# 2. 加载工具链环境变量
source /opt/poky/3.x/environment-setup-cortexa7hf-neon-poky-linux-gnueabi
# 此命令会设置：
#   CC=arm-poky-linux-gnueabi-gcc
#   SDKTARGETSYSROOT=/opt/poky/3.x/sysroots/cortexa7hf-...

# 3. 解压 Quectel SDK，目录结构应为：
# quectel_sdk/
# ├── lib/
# │   ├── interface/inc/   ← SDK 头文件（ql_oe.h, ql_wwan_v2.h 等）
# │   ├── libql_common_api.a
# │   └── ...
```

### N.3 目录放置要求

```
Makefile 中 QL_SDK_PATH 默认为 $(shell pwd)/../..
因此项目目录结构应为：

quectel_sdk/                   ← QL_SDK_PATH 指向这里
├── lib/
│   ├── interface/
│   │   └── inc/
│   │       ├── ql_oe.h
│   │       ├── ql_wwan_v2.h
│   │       └── ...
│   ├── libql_common_api.a
│   └── ...
└── examples/                  ← 或其他目录名
    └── open_dial_for_artery/  ← 项目目录（pwd/../.. = quectel_sdk/）
        ├── main.c
        ├── Makefile
        └── src/
```

或者手动指定 SDK 路径：

```bash
make QL_SDK_PATH=/path/to/quectel_sdk \
     SDKTARGETSYSROOT=/opt/poky/3.x/sysroots/cortexa7hf-neon-poky-linux-gnueabi
```

### N.4 编译命令

```bash
cd open_dial_for_artery/

# 方式一：使用 Makefile 默认设置
make

# 方式二：显式指定 SDK 和 sysroot 路径
make QL_SDK_PATH=/your/sdk/path \
     SDKTARGETSYSROOT=/your/sysroot/path

# 方式三：Makefile 注释中保留了 Artery 目标工具链配置
# arm-oe-linux-gnueabi-gcc 是当前激活的工具链（见 Makefile CC 变量）

make clean   # 清理编译产物
```

### N.5 编译产物部署

```bash
# 成功编译后生成 dial（ARM ELF 二进制）
file dial
# → ELF 32-bit LSB executable, ARM, EABI5 ...

# 部署到设备（方式一：ADB）
adb push dial /usr/dial/dial
adb shell chmod +x /usr/dial/dial

# 部署到设备（方式二：SCP）
scp dial root@<device_ip>:/usr/dial/dial

# 部署 APN 配置文件
adb push apn/apn\ normal/apn.json /usr/dial/apn.json

# 手动测试运行（正常由 init.d 脚本启动）
adb shell /usr/dial/dial
```

### N.6 APN 配置文件定制

根据实际使用的 SIM 卡，修改 `apn.json`：

```json
{
  "apn": [
    {
      "iccid": "你的SIM卡ICCID前缀（前6位）",
      "apn": "运营商APN名称",
      "usrname": "用户名（无则留空）",
      "pwd": "密码（无则留空）",
      "supplier": "运营商名称（仅注释用）",
      "is_oper_select": false
    }
  ]
}
```

查找 ICCID（设备上执行）：

```bash
# 通过 AT 命令读取
AT+ICCID      # 标准指令
AT+QCCID      # Quectel 专用指令
```

---

## 附录O：双卡切换架构参考（来自v2分析文档第6章）

> ⚠️ **架构说明**：
> - **6.1、6.2** 描述 dial 进程和 RBMaster 的内部工作流，**与当前实现一致**。
> - **6.3、6.4、6.5** 描述的是**旧版** check_network.sh 驱动切换的架构（脚本调用 start_roamlink_service/start_dial 切换）。当前方案A v2 中，check_network.sh 已简化为纯看门狗，**切换逻辑完全由 dial 状态机内部实现**（详见正文及附录A-F）。策略编号1-4及其行为定义仍有效，实现机制不同。

### O.1 物理SIM通道工作流（dial 内部流程，当前准确）

```
设备开机
    │
    ▼
dial 进程启动
    │
    ├─ 打开 /dev/smd8（AT串口）
    ├─ 注册 MCM 服务客户端
    │
    ├─ 等待 SIM 卡就绪（MCM_SIM_GetCardStatus → 状态10）
    │
    ├─ 读取 ICCID（AT命令或MCM API）
    ├─ 根据 ICCID 匹配 apn.json，获得 APN 配置
    │
    ├─ 检查 LTE 注册状态（AT+CEREG? → stat=1或5）
    │
    ├─ 配置 APN（QL_APN_Set，profile_idx=1）
    ├─ 启动数据呼叫（QL_Data_Call_Start）
    │   └─ 模组执行：附着PDN → 获取IP → 配置路由
    │
    ├─ 等待连接成功（QL_Data_Call_Info_Get → CONNECTED）
    │
    ├─ 记录 IP 地址和接口信息
    ├─ 写 /tmp/network_status = 1
    │
    └─ 进入保活监控（dial_stat_net_connected）：
        ├─ 每次循环检查 nw_get_connect_state()
        ├─ 每60s TCP测试（nw_tcp_connectivity_test）
        ├─ 监控 rx_packets（60s无新包则重拨）
        └─ 主线程每5s更新 /tmp/network_csq（信号强度）
```

### O.2 虚拟SIM通道工作流（RBMaster 内部机制，当前准确）

```
RBMaster 启动
    │
    ├─ 检查 license.cer（IMEI 授权验证）
    │
    ├─ 启动子进程组：
    │   ├─ softsim（虚拟USIM模拟器）
    │   ├─ cardManager（ecard/pcard切换管理）
    │   ├─ heartbeat（与云平台心跳保活）
    │   ├─ controller（执行云端控制指令）
    │   ├─ OemServer（OEM硬件适配层）
    │   └─ upgrade（OTA升级处理）
    │
    ├─ 接收 "RBstartServiceMaster" 命令（TCP port 5568）
    │
    ├─ softsim 向模组提供虚拟 USIM 接口
    │   └─ 模组认为插入了真实SIM卡
    │
    ├─ 模组向基站发起注册请求
    │   └─ 鉴权数据通过 softsim 转发到 RoamLink 云平台
    │
    ├─ 云平台动态选择最优运营商，返回鉴权响应
    │
    ├─ 模组完成注册，建立 LTE 数据连接
    │
    └─ heartbeat 进程维持与云平台的长连接
        └─ 支持云端动态切换运营商（cardManager）
```

### O.3 四种网络策略（旧版 check_network.sh 实现视角，仅供历史参考）

> ⚠️ 以下描述的是旧版切换实现。当前 dial 状态机实现的策略行为相同，但由 `dial_task` 内部驱动，不由脚本管理。

#### 策略矩阵

```
                        ┌──────────────────────────────────────────┐
                        │          初始网络通道                     │
                        ├────────────────────┬─────────────────────┤
                        │  Roamlink（虚拟SIM）│  物理SIM（dial）    │
┌───────────────┬───────┼────────────────────┼─────────────────────┤
│   网络异常时   │ 可切换 │   策略 1（优先RL） │   策略 2（优先SIM）  │
│   的处理方式   ├───────┼────────────────────┼─────────────────────┤
│               │ 不切换 │   策略 3（强制RL） │   策略 4（强制SIM）  │
└───────────────┴───────┴────────────────────┴─────────────────────┘
```

**策略1（优先Roamlink，network_select=1）**：以 Roamlink 启动，失败后切 SIM，SIM 失败后切回 Roamlink，在两者间交替。300s 后若当前通道正常则尝试回切。

**策略2（优先SIM，network_select=2）**：以 SIM 启动，失败后切 Roamlink，Roamlink 失败后切回 SIM。与策略1镜像对称。

**策略3（强制Roamlink，network_select=3）**：始终使用 Roamlink，失败时重启 Roamlink，绝不使用物理 SIM。

**策略4（强制SIM，network_select=4，默认）**：始终使用 SIM，失败时重拨 SIM，绝不使用 Roamlink。

### O.4 切换的底层TCP命令（当前准确）

虚拟SIM的启停通过向 RBMaster 发送 TCP 命令实现（dial 状态机内调用 `roamlink_start_service`/`roamlink_stop_service`）：

```bash
# 激活虚拟SIM
echo "RBstartServiceMaster" | nc 127.0.0.1 5568
# RBMaster 收到后：启动 softsim → 接管模组 USIM 接口 → 模组通过虚拟SIM注册

# 停止虚拟SIM
echo "RBstopServiceMaster" | nc 127.0.0.1 5568
# RBMaster 收到后：停止 softsim → 模组恢复物理SIM（若有）
# 注意：RBMaster 进程本身不停止，只是停止了虚拟SIM服务
```

**两通道互斥保证**：无内核级互斥机制，依赖 dial 状态机保证先停一个再启另一个，中间有 `sleep(3)` 等待模组状态恢复。

---

<!-- GENERATION_COMPLETE: 2026-05-18 -->


