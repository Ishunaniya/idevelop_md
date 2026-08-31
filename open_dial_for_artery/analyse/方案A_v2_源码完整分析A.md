# 方案A v2 源码完整分析文档

> 版本：dial v1.29.07  
> 更新日期：2026-03-20  
> 仓库：`open_dial_for_artery` + `roamlink_deploy`

---

## 目录

1. [整体概述](#1-整体概述)
2. [仓库文件结构](#2-仓库文件结构)
3. [系统架构图](#3-系统架构图)
4. [各文件详解](#4-各文件详解)
   - 4.1 main.c
   - 4.2 dial.h
   - 4.3 dial.c
   - 4.4 roamlink.h / roamlink.c
   - 4.5 nw.h / nw.c
   - 4.6 status/status.h / status.c
   - 4.7 check_network.sh
   - 4.8 install_update.sh
5. [状态机完整流程图](#5-状态机完整流程图)
6. [四种策略行为对照表](#6-四种策略行为对照表)
7. [关键数据流](#7-关键数据流)
8. [重要常量一览](#8-重要常量一览)
9. [文件路径速查表](#9-文件路径速查表)
10. [已知问题与待办](#10-已知问题与待办)

---

## 1 整体概述

### 这套代码是干什么的

设备上有两种上网方式：

- **物理 SIM 卡**：插在设备里的普通 SIM 卡，走 4G/LTE 拨号上网
- **Roamlink 虚拟 SIM**：由第三方服务商提供的虚拟 SIM 技术，通过 RBMaster 程序接管模组，实现跨运营商漫游

方案A v2 的目标是：让 `dial` 这个 C 程序**自动管理这两种上网方式的切换**，根据配置策略和网络状态，决定什么时候用哪个通道，以及某个通道断线后怎么处理。

### 与旧版的核心区别

| 对比项 | 旧版（check_network.sh V1） | 新版（方案A v2） |
|---|---|---|
| 逻辑在哪 | Shell 脚本 | C 代码（dial_task 状态机） |
| 切换触发 | 定期 test_network 失败则重启 dial | TCP 测试失败3次则切换通道 |
| 策略 | 无，写死 | 4种策略可配置 |
| License 管理 | 手动预置 | 自动 OTA 下载 + 备份恢复 |
| check_network.sh | 负责切换逻辑 | 只负责看门狗（保活 dial） |
| 断网时间统计 | 无 | 有，写入 /tmp/dial_status |

---

## 2 仓库文件结构

```
open_dial_for_artery/           ← C 源码仓库
│
├── main.c                      ← 程序入口，初始化 + 启动线程
├── dial.h                      ← 状态机结构体和常量定义
├── dial.c                      ← 核心：状态机实现（最重要的文件）
│
├── src/
│   ├── nw/
│   │   ├── nw.h                ← 网络操作接口声明
│   │   └── nw.c                ← 网络操作实现（TCP测试、rx_packets等）
│   ├── roamlink/
│   │   ├── roamlink.h          ← Roamlink 控制接口声明
│   │   └── roamlink.c          ← Roamlink 控制实现（RBMaster管理）
│   └── status/
│       ├── status.h            ← 状态文件结构体定义
│       └── status.c            ← 写 /tmp/dial_status 文件
│
└── Makefile                    ← 编译规则

roamlink_deploy/                ← 部署脚本仓库
│
├── install_update.sh           ← 安装/升级脚本
├── check_network.sh            ← 看门狗脚本（开机自启）
├── network.ini                 ← 策略配置文件
├── dial_1.29.7                 ← 编译好的 dial 二进制
├── roamlink/                   ← RBMaster 及其依赖文件
└── licenses/                   ← 各设备的 license 文件（按 IMEI 命名）
```

---

## 3 系统架构图

```
开机
  │
  ▼
/etc/rc5.d/S60start_check_network
  │
  ▼
check_network.sh（后台守护进程）
  │  每30秒检查 dial 是否存活，挂了就重启
  │
  ▼
/usr/dial/dial（主程序，后台运行）
  │
  ├── main线程：每5秒查询信号强度（CSQ）
  │
  └── dial_task线程（状态机）：
        │
        ├── 读 /usrdata/network.ini 获取策略（1~4）
        │
        ├── 探测 Roamlink 状态（RBMaster是否在、license是否有）
        │
        ├── 根据策略决定用哪个通道
        │   ├── 策略1/3 → 先走 Roamlink
        │   └── 策略2/4 → 先走 SIM
        │
        ├── 每60s做 TCP 测试（连 18.196.0.17:22）
        │   └── 连续失败3次 → 切换或重拨
        │
        ├── 写 /tmp/dial_status（每30s刷新）
        ├── 写 /tmp/network_type（通道标识）
        └── 写 /tmp/network_status（联网状态）

RBMaster（独立进程，由 dial_task 管理）
  │  负责虚拟SIM接入，监听 127.0.0.1:5568
  │
  ├── 收到 "RBstartServiceMaster" → 启动虚拟SIM服务
  └── 收到 "RBstopServiceMaster" → 停止虚拟SIM服务
```

---

## 4 各文件详解

---

### 4.1 main.c — 程序入口

**职责**：初始化日志、创建状态机线程、在主线程里循环查询信号强度。

**逐行解析**：

```c
#define MAIN_VERSION 1
#define SUB_VERSION 29
#define TEST_VERSION 7
```
定义版本号，拼出来是 `1.29.07`，这就是你在日志里看到的 `DIAL Version: 1.29.07`。

```c
pthread_mutex_t g_at_port_mutex = PTHREAD_MUTEX_INITIALIZER;
```
全局互斥锁，用来保护 AT 串口。AT 串口同一时刻只能有一个线程在发命令，这把锁防止主线程查 CSQ 和 dial_task 线程发 AT 命令冲突。

```c
void seas_log_config_init()
```
初始化日志配置。逻辑是：
1. 等最多10秒，看 `/media/sdcard`（SD卡）是否存在且剩余空间 >= 1GB
2. 有SD卡且空间足够：日志写到 `/media/sdcard/seas_log_dial.log`，最大50MB
3. 没有SD卡：`log_config.size = 0`，不写日志文件（只打印到控制台）

```c
clock_gettime(CLOCK_MONOTONIC, &g_dial_start_time);
```
记录程序启动时间，后面用来计算 `uptime`（运行了多少秒）。

```c
dial_mng_t *p_dial_mng = dial_mng_new();
pthread_create(&newthread, NULL, dial_task, (void *)p_dial_mng);
```
两步走：先创建状态机管理结构体，再创建一个线程专门跑状态机。

```c
while (true) {
    csq = nw_at_get_csq(p_dial_mng->smd_fd);
    ...
    sleep(5);
}
```
主线程什么都不做，只是每5秒查一次信号强度（CSQ），更新到 `p_dial_mng->csq`，这样状态文件里的信号强度字段能及时更新。

---

### 4.2 dial.h — 核心数据结构

**职责**：定义状态机用到的所有常量、枚举类型、结构体。读懂这个文件就能理解状态机的"全部状态"和"全部参数"。

#### 重要常量

```c
#define TCP_TEST_INTERVAL_SECONDS (60)   // SIM通道：每60秒做一次TCP测试
#define TCP_FAIL_THRESHOLD        (3)    // 连续失败3次才触发切换

#define ROAMLINK_CONNECT_WAIT_SEC (300)  // Roamlink启动后最多等300秒连通
#define ROAMLINK_CHECK_INTERVAL_SEC (30) // Roamlink通道：每30秒做一次TCP测试
#define ROAMLINK_FAIL_THRESHOLD   (3)    // 连续失败3次触发切换
#define ROAMLINK_NO_DATA_TIMEOUT_SEC (120) // 业务层无数据超过120秒判定死亡

#define SIM_FALLBACK_RETRY_SEC      (300)  // 策略1：SIM备用300秒后尝试回切Roamlink
#define ROAMLINK_FALLBACK_RETRY_SEC (300)  // 策略2：Roamlink备用300秒后尝试回切SIM
```

#### 状态枚举 `dial_stat_enu`

这是状态机的所有状态，dial_task 每次循环都处于其中一个：

```
dial_stat_none              ← 刚启动，决定走哪个通道
dial_stat_init              ← 初始化 QL_MCM_NW 客户端
dial_stat_sim_init          ← 初始化 SIM 操作
dial_stat_sim_check         ← 检查 SIM 卡是否在位
dial_stat_sim_op            ← 读取 ICCID（SIM卡识别码）
dial_stat_reg_check         ← 检查是否注册到网络
dial_stat_cereg_check       ← 检查 LTE 注册状态
dial_stat_precondition_check← 检查拨号前提条件
dial_stat_pre_start_call    ← 查询 APN，设置拨号参数
dial_stat_start_call        ← 发起数据呼叫
dial_stat_wait_for_connect  ← 等待连接建立
dial_stat_net_connected     ← SIM通道已联网，正在监控
dial_stat_roamlink_starting ← Roamlink服务已启动，等待连通
dial_stat_roamlink_active   ← Roamlink通道已连通，正在监控
```

可以理解为一条流水线：`none → init → sim_init → ... → net_connected`，中间任何一步失败就停在那等重试或重来。

#### 结构体 `dial_mng_t` 核心字段

```c
typedef struct {
    dial_stat_enu dial_st;      // 当前处于哪个状态（最重要的字段）
    int network_select;         // 策略值 1~4（从network.ini读来的）
    bool roamlink_available;    // 设备有没有安装Roamlink（RBMaster+license）
    bool license_pending;       // 是否正在等待license云端下载

    // SIM通道相关
    int tcp_fail_count;         // 当前SIM通道连续TCP失败次数（最多到3）
    struct timespec tcp_test_timer; // 上次做TCP测试的时刻

    // Roamlink通道相关
    int roamlink_fail_count;    // 当前Roamlink通道连续失败次数
    struct timespec roamlink_timer; // Roamlink计时器（等待/测试用）
    uint64_t roamlink_rx_packets;   // 上次检测时的收包数（业务层检测用）
    struct timespec roamlink_no_data_timer; // 最后一次收到数据包的时刻

    // 回切计时器（新增）
    struct timespec sim_fallback_timer;      // 进入SIM备用的时刻（策略1回切用）
    struct timespec roamlink_fallback_timer; // 进入Roamlink备用的时刻（策略2回切用）

    // 断网统计（新增）
    bool ever_connected;        // 是否曾经联网过（防止开机未联网就计入断网）
    bool net_was_down;          // 上一个状态是否为断网
    struct timespec net_down_since; // 断网开始时刻
    int outage_count;           // 本次运行断网次数
    long last_outage_sec;       // 上次断网持续秒数
    long total_outage_sec;      // 本次运行累计断网秒数

    // 其他
    bool sim_initialized;       // QL_MCM_NW_Client_Init是否已调用过
    bool rbmaster_started;      // license_pending模式下RBMaster是否已启动
    int smd_fd;                 // AT串口文件描述符
    uint8_t csq;                // 当前信号强度（主线程更新）
    sim_mng_t *p_sim_mng;       // SIM卡信息（ICCID等）
    apn_obj_t *p_apn_obj;       // 当前APN对象（拨号参数）
} dial_mng_t;
```

---

### 4.3 dial.c — 核心状态机（最重要的文件）

**职责**：实现整个网络管理逻辑。包含两个主要函数：`dial_mng_new()`（初始化）和 `dial_task()`（状态机主循环）。

#### 4.3.1 dial_mng_new() — 初始化函数

程序启动时调用一次，完成所有初始化。关键步骤：

**第1步：读取策略**
```c
p_dial_mng->network_select = roamlink_read_policy();
```
从 `/usrdata/network.ini` 读取 `network_select` 值（1~4）。

**第2步：Bug1修复 — 策略为SIM类时先停Roamlink**
```c
if (network_select == PREFER_SIM || FORCE_SIM) {
    if (roamlink_is_master_running()) {
        roamlink_stop_service();
        sleep(2);  // 等模组切回物理SIM
    }
}
```
防止前一个 dial 实例走 Roamlink 被 killall 后，新实例启动时模组还在虚拟SIM状态，导致读到错误 ICCID。

**第3步：探测 Roamlink 状态（三态）**

```
roamlink_probe() 返回三种结果：

ROAMLINK_PROBE_OK
  → RBMaster存在 + license有效
  → roamlink_available = true，正常使用

ROAMLINK_PROBE_NO_PACKAGE
  → RBMaster根本不存在（设备未安装Roamlink包）
  → 永久降级为FORCE_SIM，不做任何等待

ROAMLINK_PROBE_LICENSE_MISSING
  → RBMaster存在但license缺失
  → 先尝试从备份(/data/ufs/license.cer)恢复
     ├── 恢复成功 → 重新probe，如果OK则正常使用
     └── 恢复失败 → 进入license_pending模式：
                    临时FORCE_SIM + SIM联网后启动RBMaster下载license
```

**第4步：初始化所有计时器和计数器**

把所有 `_count`、`_sec`、`_timer` 字段归零，回切计时器用 `memset` 清零（零值代表"还没进入备用通道"）。

#### 4.3.2 dial_task() — 状态机主循环

这是程序的心脏，每次循环都会：
1. 检查 license_pending 模式（定期轮询license文件是否下载完成）
2. 每30秒刷新一次 `/tmp/dial_status`
3. 根据当前状态执行对应逻辑

**各状态详解：**

---

**`dial_stat_none` — 起点，决定走哪条路**

```
有license_pending？
  → 是：直接走SIM（dial_stat_init）
  → 否：看策略
       策略1/3（Roamlink类）：
           有roamlink_available？
             → 是：启动RBMaster + 启动服务 → roamlink_starting
             → 否（防呆）：强制SIM
       策略2/4（SIM类）：直接走SIM（dial_stat_init）
```

---

**`dial_stat_init` → `dial_stat_sim_init` → `dial_stat_sim_check` → `dial_stat_sim_op` → `dial_stat_reg_check` → `dial_stat_cereg_check` → `dial_stat_precondition_check` → `dial_stat_pre_start_call` → `dial_stat_start_call` → `dial_stat_wait_for_connect`**

这一串状态是 SIM 拨号的标准流程，依次完成：
1. 初始化网络客户端
2. 初始化SIM操作句柄
3. 检查SIM卡在位
4. 读取ICCID，查询对应APN
5. 检查网络注册状态（2G/3G/4G）
6. 检查LTE注册状态
7. 检查数据呼叫前置条件
8. 设置APN参数
9. 发起数据呼叫
10. 等待连接建立（IP分配）

每个步骤失败都会停在那里等重试（一般 sleep 2 秒后重试），不会崩溃。

---

**`dial_stat_net_connected` — SIM通道联网监控（核心状态）**

进入这里说明SIM已经联网，开始周期性监控：

```
每次循环检查：
├── nw_get_connect_state() 连接还在吗？
│   → 断了：跳回 dial_stat_reg_check 重新拨号
│
└── 连接还在：
    ├── 读rx_packets，有增长则重置 dial_timer（60秒超时计时器）
    │
    ├── 每60秒做一次TCP测试（连 18.196.0.17:22）
    │   ├── TCP通过：
    │   │   ├── 清零 tcp_fail_count
    │   │   ├── 策略1 且 sim_fallback_timer > 0 且 已等够300s：
    │   │   │   → 尝试回切Roamlink（roamlink_starting）
    │   │   └── （其他策略：什么都不做，继续监控）
    │   │
    │   └── TCP失败：
    │       ├── tcp_fail_count++，重置dial_timer
    │       └── tcp_fail_count >= 3：
    │           ├── license_pending模式 → 重拨SIM（不切换）
    │           ├── 策略1/2 → 切到Roamlink（roamlink_starting）
    │           └── 策略4（FORCE_SIM）→ 重拨SIM
    │
    └── dial_timer超过60秒（rx_packets长期无增长）：
        → 跳回 dial_stat_reg_check
```

---

**`dial_stat_roamlink_starting` — 等待Roamlink连通**

发出 `RBstartServiceMaster` 后进入此状态，每10秒做一次TCP测试：

```
TCP通过？
├── 是：进入 roamlink_active
└── 否：等待计时 > 300秒？
    ├── 策略1/2 → 超时，切回SIM
    └── 策略3 → 超时，重启Roamlink（不切SIM）
```

---

**`dial_stat_roamlink_active` — Roamlink通道联网监控（核心状态）**

进入这里说明Roamlink已连通，每30秒做一次TCP测试：

```
TCP通过：
├── 策略2 且 roamlink_fallback_timer > 0 且 已等够300s：
│   → 尝试回切SIM（sim_init）
│
└── 业务层检测（did_switch为false才执行）：
    ├── 读所有rmnet_data*接口的rx_packets总和
    ├── 有增长 → 业务正常，更新基准
    └── 无增长超过120秒 → 判定业务层死亡：
        ├── 策略1/2 → 切回SIM
        └── 策略3 → 重启Roamlink

TCP失败：
└── roamlink_fail_count >= 3：
    ├── 策略1/2 → 切回SIM
    └── 策略3 → 重启Roamlink（永远不切SIM）
```

---

**`dial_status_update()` — 状态文件更新**

在状态切换点和每30秒周期调用，做两件事：
1. 断网边沿检测（联网→断网时计数，断网→联网时结算时长）
2. 把当前状态写入 `/tmp/dial_status`

---

### 4.4 roamlink.h / roamlink.c — Roamlink控制层

**职责**：封装所有与 Roamlink 相关的操作，让 dial.c 不需要关心底层细节。

#### roamlink_probe() — 三态探测

```
检查1：access(RBMaster, X_OK) → 文件存在且可执行？
  → 不存在：返回 NO_PACKAGE

检查2：stat(license.cer) → 文件存在且 size > 0？
  → 不存在或为空：返回 LICENSE_MISSING

两项都通过：返回 OK
```

#### roamlink_license_appeared() — 检测license是否下载完成

两次 `stat` 对比文件大小，间隔2秒：
- 第一次 size=0：文件还没出现，返回 false
- 第一次 size>0，第二次 size 不同：还在写入中，返回 false
- 两次 size 一致且 >0：写入完成，返回 true

这样能防止 RBMaster 写文件到一半时被误判为下载完成。

#### roamlink_license_backup_and_reboot() — 备份并重启（不返回）

license 下载完成后调用：
1. 把 license 从主路径复制到备份路径（`/data/ufs/license.cer`）
2. `fsync` 确保写入闪存
3. `sync()` 全局同步
4. `reboot(RB_AUTOBOOT)` 重启

重启后重新 probe，这次 license 存在，正常进入策略流程。

#### roamlink_is_master_running() — 双重存活检测

为什么要双重检测？因为只扫 `/proc` 不够：RBMaster 崩溃后可能变成僵尸进程，`/proc` 里还能看到它，但它已经不工作了。

```
第1步：扫 /proc/<pid>/cmdline，找到包含"RBMaster"的进程
  → 找不到：直接返回 false

第2步：非阻塞连接 127.0.0.1:5568，超时1秒
  → 连不上：说明是僵尸进程，返回 false（会触发重启）
  → 连上了：真的在运行，返回 true
```

#### roamlink_start_master() — 启动RBMaster进程

```
先调 roamlink_is_master_running()：
  → 已在运行：直接返回，不重复启动

fork() 创建子进程：
  子进程：
    chdir(/usrdata/roamlink)  ← RBMaster用相对路径加载.so，必须切目录
    stdout/stderr重定向到/dev/null
    execl(RBMaster)

父进程：
    sleep(3)  ← 等RBMaster完成初始化，5568端口就绪
```

#### roamlink_start_service() / roamlink_stop_service()

```
start_service(smd_fd):
  1. AT+COPS=0（自动选运营商，清除手动锁定）
  2. 发送 "RBstartServiceMaster" 到 127.0.0.1:5568
  3. 写 /tmp/network_type = 2

stop_service():
  1. 发送 "RBstopServiceMaster" 到 127.0.0.1:5568
  2. 写 /tmp/network_type = 0
  3. sleep(3)  ← 给模组时间完成注销
```

---

### 4.5 nw.h / nw.c — 网络操作层

**职责**：封装底层网络查询操作，提供给 dial.c 调用。

#### 重要函数

**`nw_tcp_connectivity_test(host, port, timeout_sec)`**

TCP 连通性测试，是整个切换逻辑的判断依据：
```
创建 TCP socket（非阻塞模式）
连接 18.196.0.17:22
使用 select() 等待，超时5秒
  → 连接成功（收到SYN-ACK）：返回 true（网络通）
  → 超时或被拒绝：返回 false（网络不通）
```

注意：这只是测试"能不能连上那台服务器"，不代表业务数据能正常流通。

**`nw_get_rmnet_rx_packets_sum(p_sum)`**

读取所有 `rmnet_data*` 接口的收包总数，用于业务层存活检测：
```
扫描 /sys/devices/virtual/net/ 目录
找到所有以 "rmnet_data" 开头的接口
读取每个接口的 /statistics/rx_packets 文件
累加到 *p_sum
```

**`nw_mark_network_status(status)`**

写 `/tmp/network_status` 文件：
- `1` = 网络已连通
- `0` = 网络断开

**`nw_at_get_cereg_stat(smd_fd)`**

发送 `AT+CEREG?` 查询 LTE 注册状态：
- 返回 1 或 5 = 已注册（国内或漫游）
- 其他 = 未注册

---

### 4.6 status/status.h / status.c — 状态文件输出

**职责**：将当前运行状态格式化写入 `/tmp/dial_status`，供外部查看。

#### 写入格式

```ini
# dial status - auto generated by dial v1.29.07
# updated: 2026-03-19 10:05:38

[dial]
version=1.29.07         ← 程序版本
uptime=329              ← 本次运行秒数
state=roamlink_active   ← 当前状态名
policy=1                ← 策略值
policy_name=PREFER_ROAMLINK  ← 策略名称
channel=ROAMLINK        ← 当前通道（SIM/ROAMLINK/SWITCHING/NONE）
roamlink_available=1    ← 设备有没有安装Roamlink
license_pending=0       ← 是否正在等待license下载
license_wait_sec=0      ← 已等待license多少秒

[sim]
iccid=89464283216100721879  ← SIM卡识别码
csq=21                      ← 信号强度（0~31）
apn=internet.lte.cxn        ← 当前APN
profile_idx=1               ← 拨号profile编号
plmn=46001                  ← 运营商网络代码

[network]
status=1                ← 网络是否联通（1=通，0=断）
type=2                  ← 当前通道类型（1=SIM，2=Roamlink）
ip=10.88.197.109        ← 当前IP地址
ifname=rmnet_data0      ← 网络接口名

[counters]
tcp_fail_count=0        ← 当前连续TCP失败次数
roamlink_fail_count=0   ← 当前Roamlink连续失败次数
roamlink_rx_packets=143 ← Roamlink通道累计收包数

[roamlink]
state=active            ← Roamlink内部状态（none/starting/active）
biz_no_data_sec=25      ← 业务层无数据持续秒数
roamlink_connect_wait_sec=0  ← 等待Roamlink连通已用秒数
rbmaster_pid=1516       ← RBMaster进程PID（0=未运行）

[stats]
outage_count=2          ← 本次运行断网次数
last_outage_sec=22      ← 上次断网持续秒数
total_outage_sec=134    ← 本次运行累计断网秒数
current_outage_sec=0    ← 当前正在断网的秒数（0=已联网）
```

#### 原子写入机制

为防止外部读到写了一半的文件：
1. 先写到临时文件 `/tmp/dial_status.tmp`
2. 写完后 `rename()` 替换正式文件

`rename()` 是原子操作（POSIX保证），外部读到的永远是完整内容。

#### 断网统计逻辑（边沿检测）

```
每次调用 dial_status_update() 时：

计算 net_up = (状态是 net_connected 或 roamlink_active)

if (net_up) → ever_connected = true  // 记录曾经联网过

if (!net_up && !net_was_down && ever_connected):
    // 联网→断网的边沿
    记录断网开始时刻 net_down_since
    outage_count++
    net_was_down = true

elif (net_up && net_was_down):
    // 断网→联网的边沿
    计算本次断网时长 = now - net_down_since
    last_outage_sec = 本次时长
    total_outage_sec += 本次时长
    net_was_down = false
```

---

### 4.7 check_network.sh — 看门狗脚本

**职责**：确保 `/usr/dial/dial` 进程始终存活。注意：在方案A v2 中，这个脚本**不再负责任何切换逻辑**，只是一个简单的进程保活看门狗。

```sh
# 开机时启动 dial（dial 还没运行时）
if ! pgrep -f "$DIAL_BIN" >/dev/null; then
    "$DIAL_BIN" &     # 后台启动
fi

# 主循环：每30秒检查一次
while true; do
    sleep 30
    if ! pgrep -f "$DIAL_BIN" >/dev/null; then
        # dial 不在了，重新启动
        "$DIAL_BIN" &
    fi
done
```

**日志路径**：`/usrdata/check_network.log`，超过1MB自动覆盖重写。

**开机路径**：
```
开机 → rc5.d/S60start_check_network → /etc/init.d/start_check_network → check_network.sh（后台）
```

---

### 4.8 install_update.sh — 安装/升级脚本

**职责**：将升级包内容安装到设备上，支持三种场景。

#### 三种场景判断逻辑

```
场景判断：
  /usrdata/roamlink 目录存在？
  ├── 存在 且 FORCE_INSTALL=0：
  │   纯升级模式（只替换dial/check_network.sh/network.ini）
  │   → 不动license，不动roamlink目录，重启
  │
  ├── 存在 且 FORCE_INSTALL=1：
  │   强制重装模式（先删除旧的再装）
  │   → rm -rf /usrdata/roamlink + rm -f /data/ufs/license.cer
  │   → 走首次安装流程
  │
  └── 不存在：
      首次安装模式
      → 走完整安装流程
```

#### 首次安装流程

```
1. check_fw_version → 判断固件版本
   → 0: 需要DFOTA升级固件
   → 1: 固件OK，直接安装
   → 2/255: 不支持的版本，退出

2. get_imei → 获取设备IMEI

3. license预置判断：
   找 licenses/{IMEI}_license.cer
   → 找到（U盘出厂场景）：
       cp 到 roamlink/etc/.pconfig/license.cer（主路径）
       cp 到 /data/ufs/license.cer（备份路径）
   → 找不到（OTA场景）：
       跳过，重启后dial自动下载

4. 安装文件：
   cp -af ./roamlink /usrdata/         ← Roamlink组件
   cp ./dial_1.29.7 /usr/dial/dial    ← dial程序
   cp ./network.ini /usrdata/          ← 策略配置
   cp ./check_network.sh /usrdata/     ← 看门狗脚本
   cp ./start_check_network /etc/init.d/ ← 开机启动脚本

5. 注释掉 /opt/start_daemon.sh 中的 check_dial 调用
   （防止老版本看门狗和新版本冲突）

6. sync + sleep 2 + reboot
```

---

## 5 状态机完整流程图

```
程序启动
    │
    ▼
dial_mng_new()
    ├── 读策略（network.ini）
    ├── Bug1修复：SIM策略先停Roamlink服务
    ├── roamlink_probe() 三态探测
    │   ├── OK → roamlink_available=true
    │   ├── NO_PACKAGE → 强制FORCE_SIM
    │   └── LICENSE_MISSING
    │       ├── 备份恢复成功 → roamlink_available=true
    │       └── 备份也没有 → license_pending=true, 临时FORCE_SIM
    └── 初始化所有字段
    │
    ▼
dial_task() 主循环
    │
    ├── [周期任务] license_pending检查（每60s轮询license文件）
    ├── [周期任务] 状态文件刷新（每30s）
    │
    ▼
┌──────────────────────────────────────────────────┐
│ dial_stat_none                                   │
│  license_pending？→ dial_stat_init               │
│  策略1/3（Roamlink类）？                          │
│    roamlink_available？→ roamlink_starting        │
│    否（防呆）→ FORCE_SIM → dial_stat_init         │
│  策略2/4（SIM类）→ dial_stat_init                 │
└──────────────────────────────────────────────────┘
    │
    ▼ (SIM路径)
dial_stat_init → dial_stat_sim_init → dial_stat_sim_check
    → dial_stat_sim_op（读ICCID/APN）
    → dial_stat_reg_check → dial_stat_cereg_check
    → dial_stat_precondition_check
    → dial_stat_pre_start_call（设置APN参数）
    → dial_stat_start_call（发起数据呼叫）
    → dial_stat_wait_for_connect（等待IP分配）
    │
    ▼ (联网成功)
┌──────────────────────────────────────────────────┐
│ dial_stat_net_connected（SIM通道监控）            │
│                                                  │
│ 每60s TCP测试：                                  │
│  通过 → 策略1且等够300s → 回切Roamlink            │
│  失败3次：                                        │
│    license_pending → 重拨SIM                     │
│    策略1/2 → 切Roamlink（roamlink_starting）      │
│    策略4 → 重拨SIM（reg_check）                   │
│                                                  │
│ rx_packets 60s无增长 → reg_check                 │
└──────────────────────────────────────────────────┘
    │ (策略1/2 SIM失败)
    ▼
┌──────────────────────────────────────────────────┐
│ dial_stat_roamlink_starting（等待Roamlink连通）   │
│                                                  │
│ 每10s TCP测试：                                  │
│  通过 → 进入 roamlink_active                     │
│  超过300s未通：                                   │
│    策略1/2 → 切回SIM（sim_init或init）            │
│    策略3 → 重启Roamlink（停止+重新启动）           │
└──────────────────────────────────────────────────┘
    │ (连通)
    ▼
┌──────────────────────────────────────────────────┐
│ dial_stat_roamlink_active（Roamlink通道监控）     │
│                                                  │
│ 每30s TCP测试：                                  │
│  通过：                                           │
│    策略2且等够300s → 回切SIM                      │
│    业务层检测（rx_packets无增长120s）→ 切SIM/重启  │
│  失败3次：                                        │
│    策略1/2 → 切回SIM                             │
│    策略3 → 重启Roamlink                          │
└──────────────────────────────────────────────────┘
```

---

## 6 四种策略行为对照表

| | 策略1 PREFER_ROAMLINK | 策略2 PREFER_SIM | 策略3 FORCE_ROAMLINK | 策略4 FORCE_SIM |
|---|---|---|---|---|
| **初始通道** | Roamlink | SIM | Roamlink | SIM |
| **主通道失败** | 切到SIM备用 | 切到Roamlink备用 | 重启Roamlink（不切SIM） | 重拨SIM（不切Roamlink） |
| **备用通道失败** | 再切回Roamlink | 再切回SIM | — | — |
| **主通道恢复** | 每300s主动尝试回切Roamlink | 每300s主动尝试回切SIM | — | — |
| **适用场景** | 主要用Roamlink，SIM做备份 | 主要用SIM，Roamlink做备份 | 只用Roamlink，不允许降级 | 只用SIM，完全不用Roamlink |
| **Roamlink缺失时** | 自动降级FORCE_SIM | — | 自动降级FORCE_SIM | — |

---

## 7 关键数据流

### 7.1 开机正常启动（策略1，有license）

```
开机
→ check_network.sh 启动 dial
→ dial_mng_new():
    读策略=1, probe OK, roamlink_available=true
→ dial_stat_none: 策略1, 启动RBMaster, roamlink_starting
→ 每10s TCP测试，通过
→ dial_stat_roamlink_active
→ channel=ROAMLINK, status=1
→ 每30s刷新 /tmp/dial_status
```

### 7.2 开机OTA下载license（无license）

```
开机
→ dial_mng_new():
    probe = LICENSE_MISSING, 备份也没有
    license_pending=true, 临时FORCE_SIM
→ dial_stat_none: license_pending → dial_stat_init
→ SIM拨号流程 → dial_stat_net_connected（SIM已联网）
→ rbmaster_started=false, 首次进入net_connected:
    启动RBMaster, rbmaster_started=true
→ RBMaster连云端，下载license到主路径
→ 每60s检测license文件: 出现了！
→ roamlink_license_backup_and_reboot():
    备份license到/data/ufs/
    重启
→ 重启后: probe OK, 正常走策略1流程
```

### 7.3 Roamlink失败切SIM（策略1）

```
Roamlink active
→ TCP测试失败 count:1/3
→ TCP测试失败 count:2/3
→ TCP测试失败 count:3/3
→ policy=1: Roamlink failed, switching to SIM
→ roamlink_stop_service()（发RBstopServiceMaster, network_type=0）
→ p_apn_obj=NULL（清空APN缓存）
→ sim_fallback_timer=now（记录进入SIM备用的时刻）
→ SIM拨号流程 → net_connected
→ 之后每60s TCP通过时检查: 距sim_fallback_timer够300s了吗？
    → 够了: 尝试回切Roamlink
```

### 7.4 TCP测试目标失效（已知问题）

```
18.196.0.17 服务器不响应
→ SIM TCP测试一直失败（实际SIM网络正常）
→ tcp_fail_count 累积到3
→ 切到Roamlink
→ Roamlink也用同一目标测试，也失败
→ 300s超时 → 切回SIM
→ 无限循环，两个通道来回切
→ 实际网络可用（ping 8.8.8.8通），只是切换系统乱了
```

---

## 8 重要常量一览

| 常量 | 值 | 含义 |
|---|---|---|
| `TCP_TEST_INTERVAL_SECONDS` | 60s | SIM通道TCP测试间隔 |
| `TCP_FAIL_THRESHOLD` | 3次 | 触发SIM通道切换的连续失败次数 |
| `ROAMLINK_CONNECT_WAIT_SEC` | 300s | Roamlink启动后等待连通的超时 |
| `ROAMLINK_CHECK_INTERVAL_SEC` | 30s | Roamlink通道TCP测试间隔 |
| `ROAMLINK_FAIL_THRESHOLD` | 3次 | 触发Roamlink通道切换的连续失败次数 |
| `ROAMLINK_NO_DATA_TIMEOUT_SEC` | 120s | 业务层无数据判定超时 |
| `SIM_FALLBACK_RETRY_SEC` | 300s | 策略1在SIM备用后多久尝试回切Roamlink |
| `ROAMLINK_FALLBACK_RETRY_SEC` | 300s | 策略2在Roamlink备用后多久尝试回切SIM |
| `LICENSE_WAIT_TIMEOUT_SEC` | **300s（测试）** | license下载等待超时（**生产须改为7200**） |
| `LICENSE_CHECK_INTERVAL_SEC` | 60s | 轮询license文件的间隔 |
| `DIAL_TIMEOUT_SECONDS` | 60s | rx_packets无增长的超时阈值 |
| `NW_TCP_TEST_HOST` | 18.196.0.17 | TCP测试目标IP（单点，有隐患） |
| `NW_TCP_TEST_PORT` | 22 | TCP测试目标端口 |
| `ROAMLINK_CTRL_PORT` | 5568 | RBMaster控制端口 |

---

## 9 文件路径速查表

| 文件路径 | 内容 | 读写方 |
|---|---|---|
| `/usr/dial/dial` | dial程序二进制 | 安装脚本写，check_network.sh执行 |
| `/usrdata/network.ini` | 策略配置（network_select=1~4） | 人工配置/安装脚本写，dial读 |
| `/usrdata/roamlink/` | Roamlink组件目录 | 安装脚本写 |
| `/usrdata/roamlink/RBMaster` | RBMaster可执行文件 | dial_task执行 |
| `/usrdata/roamlink/etc/.pconfig/license.cer` | License主路径 | RBMaster写/读，dial检测 |
| `/data/ufs/license.cer` | License备份路径（持久化分区） | dial备份，重刷固件后可恢复 |
| `/tmp/dial_status` | 当前运行状态（INI格式） | dial写，外部可cat查看 |
| `/tmp/network_type` | 当前通道类型（1=SIM, 2=Roamlink） | dial写，外部程序读 |
| `/tmp/network_status` | 当前联网状态（0/1） | dial写，外部程序读 |
| `/tmp/network_csq` | 当前信号强度 | dial写 |
| `/tmp/network_plmn` | 当前运营商PLMN | dial写 |
| `/usrdata/check_network.log` | 看门狗日志（超1MB自动rotate） | check_network.sh写 |
| `/media/sdcard/seas_log_dial.log` | dial详细日志（50MB上限） | dial写（需SD卡且有1GB空间） |
| `/usrdata/roamlink_install.log` | 安装日志 | install_update.sh写 |
| `/etc/init.d/start_check_network` | 开机启动脚本 | 安装脚本写，系统开机执行 |

---

## 10 已知问题与待办

### 生产前必须做的

| 优先级 | 问题 | 处理方法 |
|---|---|---|
| 🔴 高 | `LICENSE_WAIT_TIMEOUT_SEC` 当前是300s（测试值） | 生产出包前改为7200（2小时） |
| 🔴 高 | TCP测试目标 `18.196.0.17:22` 是单点，服务器挂了整个切换系统失效 | 改为 `8.8.8.8:53`（Google DNS），或配置多目标轮询 |

### 待验证的场景

| 场景 | 说明 | 建议 |
|---|---|---|
| `dail_stop_data_call` 是否真正停止rmnet流量 | 切换到Roamlink时调用，从未实际验证过是否能停干净 | 生产环境抓包确认 |
| 策略1/2 的300s回切计时在车载环境是否合适 | 信号不稳定时可能频繁无效回切 | 收集生产数据后调整 |
| 业务层假连通场景（TCP通但rx_packets不增长） | 测试环境无法制造，需等生产环境实际遇到 | 遇到后分析日志确认 |

### 代码层面的设计局限

| 问题 | 说明 |
|---|---|
| 业务层检测与TCP检测耦合 | TCP测试通过本身会产生rx_packets，导致业务层检测计时器被重置，两个机制无法独立工作。如需真正解耦，需把TCP测试改为ICMP ping（不产生rmnet包） |
| 断网统计重启后清零 | `outage_count` 等统计字段存在内存里，`killall dial` 后归零。如需持久化需写文件 |
| RBMaster崩溃无自动重启 | `roamlink_is_master_running()` 能检测到僵尸进程，但只有在下次需要启动Roamlink时才触发重启，中间有延迟 |
