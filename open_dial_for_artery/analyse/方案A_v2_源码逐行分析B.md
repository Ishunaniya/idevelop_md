# 方案A v2 源码逐行分析

> 版本：dial v1.29.07  
> 本文档对每一行（或每一段）代码的功能、原因、注意事项做详细说明。  
> 代码能力不强的读者也能完全看懂。

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

**情况3：ROAMLINK_PROBE_LICENSE_MISSING**
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
**替代原 `echo "..." | nc 127.0.0.1 5568`**：Shell 管道+nc 的方式每次都要 fork 子进程，在 C 代码里直接用 socket 更高效，也便于错误处理。

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
