# SIM / Roamlink 切换逻辑深度分析

> 聚焦 nw.c、roamlink.c、dial.c 三个文件中所有与通道切换有关的代码。  
> 包括：触发条件、执行步骤、时序约束、潜在问题、各策略差异。

---

## 一、先搞清楚"切换"涉及哪些层

切换不是一个简单的 if-else，它牵扯三层：

```
┌─────────────────────────────────────────────────────┐
│  应用层决策（dial.c）                                │
│  "检测到失败 → 决定切换方向 → 调用下面两层"          │
├─────────────────────────────────────────────────────┤
│  通道控制层（roamlink.c）                            │
│  "实际操控 RBMaster 启停虚拟SIM服务"                 │
├─────────────────────────────────────────────────────┤
│  网络检测层（nw.c）                                  │
│  "提供 TCP 测试、rx_packets 读取等判断依据"           │
└─────────────────────────────────────────────────────┘
```

三层各司其职，dial.c 根据 nw.c 提供的检测结果做决策，通过调用 roamlink.c 执行切换动作。

---

## 二、网络检测层（nw.c）——切换的"眼睛"

切换的所有决策依据都来自 nw.c 提供的两个检测函数。

### 2.1 TCP 连通性测试 `nw_tcp_connectivity_test()`

这是整个切换系统最核心的判断依据。

```c
bool nw_tcp_connectivity_test(const char *host, int port, int timeout_sec)
// 调用方固定传入：host="18.196.0.17", port=22, timeout_sec=5
```

**完整执行流程：**

```
第1步：socket(AF_INET, SOCK_STREAM, 0)
       创建 TCP socket（IPv4，流式，TCP协议）

第2步：fcntl(sockfd, F_SETFL, flags | O_NONBLOCK)
       设置非阻塞模式
       原因：如果用阻塞 connect()，当服务器不响应时会卡住整个 dial_task 线程
             5秒的等待会让状态机停转，其他定时器都在这5秒里积累误差

第3步：connect(sockfd, &server_addr, sizeof(server_addr))
       发出 TCP SYN 包，立即返回（非阻塞）
       返回值有三种情况：
         ret == 0         → 立即连上（本地回环时可能，连远端几乎不会）
         errno == EINPROGRESS → 正在连接中（正常情况，继续等）
         其他 errno       → 立即失败（网络不可达、被拒绝等）

       "Network is unreachable" 日志就来自这里：
       当 SIM 停了或 Roamlink 还没建立路由时，connect() 立刻返回 ENETUNREACH，
       在日志里看到 "connect() failed immediately: Network is unreachable"

第4步：select(sockfd+1, NULL, &wfds, NULL, &tv)  // 等最多5秒
       等待 socket 变为"可写"
       连接成功（收到SYN-ACK）→ socket 可写
       连接失败（收到RST）     → socket 也可写（！）
       超时（5秒无响应）       → select 返回 0

       注意：可写并不代表连接成功，必须继续确认

第5步：getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &sock_err, &sock_err_len)
       查询 socket 的错误状态
       sock_err == 0   → 连接真正成功，返回 true
       sock_err != 0   → 连接失败（如 ECONNREFUSED=111），返回 false

第6步：close(sockfd)
       无论成功失败都关闭，避免 fd 泄漏
```

**三种失败日志对应的底层原因：**

| 日志 | 对应步骤 | 含义 |
|---|---|---|
| `connect() failed immediately: Network is unreachable` | 第3步 | 路由不存在，数据包发不出去（通道还没建立或已断开） |
| `select() timeout or error (ret=0)` | 第4步 | 5秒内服务器没有响应（服务器宕机、防火墙丢包） |
| `connect failed: Connection refused` | 第5步 | 服务器在线但22端口拒绝连接（端口关闭） |

**切换触发的是哪种失败？**  
实测中主要出现前两种。Roamlink 启动过程中路由还没建立，会看到 `Network is unreachable`，这是正常的过渡状态，在 `roamlink_starting` 状态下预期会持续一段时间。

---

### 2.2 业务层收包检测 `nw_get_rmnet_rx_packets_sum()`

```c
bool nw_get_rmnet_rx_packets_sum(uint64_t *p_sum)
```

**执行流程：**

```
opendir("/sys/devices/virtual/net")
遍历目录中所有条目：
  只处理 "rmnet_data" 开头的接口（strncmp 前10个字符）

  对每个接口读取：
  /sys/devices/virtual/net/{接口名}/statistics/rx_packets
  这是 Linux 内核维护的接口统计文件，内容是一个十进制数字

  把数字累加到 *p_sum

返回 found（是否至少找到一个接口）
```

**为什么累加而不是指定接口？**

Roamlink 虚拟 SIM 具体用哪个 rmnet_dataX 编号目前未确认（待生产验证）。可能的情况：
- 用 rmnet_data0（和物理 SIM 同一个）：SIM 停后 Roamlink 接管同一接口，累加值继续增长 ✓
- 用 rmnet_data1 或其他：物理 SIM 停了，只有 Roamlink 的接口在涨，累加值仍然增长 ✓

两种情况累加逻辑都正确，这是设计上的巧妙之处。

**这个函数的局限性（和 TCP 测试的耦合）：**

TCP 测试通过时，TCP 的 SYN-ACK 包本身就会让 rmnet_dataX 的 rx_packets 增加。所以只要 TCP 测试通过，rx_packets 就一定会增长，业务层检测实际上只能在 TCP 也不通的极端情况下独立发挥作用。

---

### 2.3 数据连接状态查询 `nw_get_connect_state()`

```c
int nw_get_connect_state(int profile_idx)
// 返回：1=已连接，0=未连接，-1=查询失败
```

调用 Quectel SDK 的 `QL_Data_Call_Info_Get()`，查询指定 profile 的数据呼叫状态。  
这是底层连接的硬状态——如果这个返回0，表示运营商侧的 PDN 连接已经断开，不需要等 TCP 测试，直接跳回重拨流程。

**两种检测的区别：**

| 检测方式 | 检测的是什么 | 触发时机 |
|---|---|---|
| `nw_get_connect_state()` | 数据呼叫是否建立（PDP/PDN 连接） | 每次 `net_connected` 状态循环都检查 |
| `nw_tcp_connectivity_test()` | 端到端 TCP 能否连到测试目标 | 每60秒检查一次 |

前者检测基础连接，后者检测实际可用性。基础连接有时候"建立"但不可用（如分配了 IP 但路由有问题），TCP 测试就是为了发现这种情况。

---

## 三、通道控制层（roamlink.c）——切换的"手"

当 dial.c 决定要切换时，具体操作由 roamlink.c 执行。

### 3.1 SIM → Roamlink 需要哪些步骤

`roamlink_start_service()` 是 SIM→Roamlink 方向的核心动作，但在调用它之前，dial.c 还需要做额外准备：

**完整的 SIM→Roamlink 切换序列：**

```
1. dail_stop_data_call(profile_idx)          [dial.c]
   │ 调用 QL_Data_Call_Stop()
   │ 停止物理 SIM 的数据呼叫（PDN 连接断开）
   │ ⚠️ 是否真正停止 rmnet_data0 流量待验证

2. p_apn_obj = NULL                          [dial.c - Bug4修复]
   │ 清空 APN 缓存
   │ 原因：切回 SIM 时必须重新查 APN，防止复用 Roamlink 通道时残留的状态

3. roamlink_start_master()                   [roamlink.c - Bug5修复]
   │ 检查 RBMaster 是否在运行（双重检测：/proc + 5568端口）
   │ 已在运行 → 直接返回（幂等）
   │ 未运行 → fork() + exec() 启动，sleep(3) 等初始化完成
   │ 原因：策略2全新启动时 RBMaster 从未启动；策略3重启时 RBMaster 可能已崩溃

4. roamlink_mark_network_type(2)             [roamlink.c]
   │ 写 /tmp/network_type = "2"
   │ 通知外部程序当前通道是 Roamlink

5. roamlink_start_service(smd_fd)            [roamlink.c]
   │ 步骤5a: AT+COPS=0（需要持有 g_at_port_mutex）
   │          自动运营商选择，清除可能残留的手动锁定
   │          即使 AT 命令失败也继续（非致命）
   │ 步骤5b: roamlink_send_cmd("RBstartServiceMaster")
   │          通过 TCP socket 连接 127.0.0.1:5568 发送命令
   │          非阻塞 connect + select 5秒超时
   │          RBMaster 收到命令后接管模组，建立虚拟 SIM 连接
   │ 步骤5c: roamlink_mark_network_type(2)（内部再写一次，确保）

6. roamlink_timer = cur_timer                [dial.c]
   记录启动时刻，用于 roamlink_starting 状态的300秒超时计时

7. roamlink_fail_count = 0                   [dial.c]
   清零 Roamlink 失败计数

8. dial_st = dial_stat_roamlink_starting     [dial.c]
   进入等待连通状态
```

**AT+COPS=0 的作用：**

COPS 是 operator selection 的 AT 命令，`AT+COPS=0` 表示自动选择运营商。  
在之前如果有过手动锁定运营商（如运营商选择流程里的 `AT+COPS=1,2,xxxxx`），此时需要先解锁，让模组自由注册到 Roamlink 指定的运营商。不发这条命令可能导致 Roamlink 无法正常注册。

---

### 3.2 Roamlink → SIM 需要哪些步骤

**完整的 Roamlink→SIM 切换序列：**

```
1. roamlink_stop_service()                   [roamlink.c]
   │ 步骤1a: roamlink_send_cmd("RBstopServiceMaster")
   │          发送停止命令，RBMaster 释放虚拟 SIM 控制权，模组恢复物理 SIM
   │ 步骤1b: roamlink_mark_network_type(0)
   │          写 /tmp/network_type = "0"（切换中，未确定通道）
   │ 步骤1c: sleep(3)
   │          ⚠️ 关键等待：给模组时间完成虚拟SIM注销
   │          如果不等，立刻启动物理SIM拨号，模组内部状态混乱会导致失败

2. p_apn_obj = NULL                          [dial.c - Bug4修复]
   清空 APN 缓存，强制重新查 APN

3. sim_fallback_timer = cur_timer            [dial.c - Bug2/3修复]
   记录进入 SIM 备用的时刻，用于后续主动回切计时

4. tcp_test_timer 重置                       [dial.c]
   重置 SIM 通道 TCP 测试计时器

5. roamlink_mark_network_type(1)             [dial.c → nw.c]
   写 /tmp/network_type = "1"（物理SIM通道）

6. 判断 sim_initialized                      [dial.c]
   │ false（首次进SIM路径）:
   │   dial_st = dial_stat_init
   │   走完整初始化：init → sim_init → sim_check → ... → net_connected
   │
   └─ true（之前走过SIM路径）:
       is_func_called = true
       dial_st = dial_stat_sim_init
       跳过 QL_MCM_NW_Client_Init，从 sim_init 重入
       节省初始化时间（约10~30秒）
```

**`sim_initialized` 标志为什么重要？**

`QL_MCM_NW_Client_Init()` 只能调用一次。如果是策略1开机直接走 Roamlink，之后切到 SIM，此时 `sim_initialized = false`，需要完整初始化。但如果之前 SIM 已经初始化过（切换过来又切回去），`sim_initialized = true`，只需要从 `sim_init` 重入即可，快了好几个状态。

---

### 3.3 RBMaster 的存活检测为什么要双重验证

```c
bool roamlink_is_master_running(void)
```

**第1步只做 /proc 扫描的问题：**

```
RBMaster 崩溃（例如段错误）
→ 进程退出，变为僵尸进程（zombie）
→ 父进程（init）未调用 waitpid 前，/proc/<pid> 仍然存在
→ /proc/<pid>/cmdline 仍然可读，包含 "RBMaster"
→ 只做 /proc 扫描会误判为"正在运行"
→ 不会重启 RBMaster
→ dial_task 发出 RBstartServiceMaster 命令
→ 5568 端口没有监听者，连接失败
→ Roamlink 永远无法启动
```

**加上第2步端口检测：**

```
僵尸进程不持有端口（进程已退出，socket 已关闭）
→ 连接 127.0.0.1:5568 失败
→ 判断为"RBMaster 未正常运行"
→ 触发重启（fork + exec）
→ 新 RBMaster 绑定5568，正常工作
```

**超时设1秒的原因：**

127.0.0.1 是本地回环，延迟理论上是微秒级。如果1秒都连不上5568端口，说明没有进程在监听，不需要等更久。

---

## 四、应用层决策（dial.c）——切换的"大脑"

### 4.1 所有切换触发点全览

整个代码里共有 **8 个地方**会触发通道切换，逐一分析：

---

#### 触发点 A：`dial_stat_net_connected` — SIM TCP 失败 3 次

```c
// 条件
dif_timer.tv_sec >= TCP_TEST_INTERVAL_SECONDS  // 距上次测试已过60秒
nw_tcp_connectivity_test() == false            // 本次测试失败
tcp_fail_count >= TCP_FAIL_THRESHOLD           // 累计连续失败 >= 3次
```

**时序分析：**

```
T+0s    进入 net_connected，tcp_test_timer = now
T+60s   第1次 TCP 测试 → FAIL，count=1，dial_timer = now（重置保活计时器）
T+120s  第2次 TCP 测试 → FAIL，count=2，dial_timer = now
T+180s  第3次 TCP 测试 → FAIL，count=3
        触发切换！
```

**为什么每次 TCP 失败都要重置 `dial_timer`？**

`dial_timer` 也是 rx_packets 保活超时的计时器（60秒无增长重拨）。如果不重置：

```
T+60s   TCP 失败，count=1
T+120s  TCP 失败，count=2，此时 dial_timer 已经过了120秒 > 60秒！
        → rx_packets 超时先触发，把状态跳回 reg_check，tcp_fail_count 清零
        → 永远无法累积到3次，切换逻辑永远不会触发
```

重置 `dial_timer` 确保 rx_packets 超时始终从最近一次 TCP 测试起算，保证 TCP 3次失败（180s）先于 rx_packets 超时（60s）触发。

**分策略处理：**

```c
if (license_pending)
    → 重拨 SIM（跳 reg_check），不切换
      原因：Roamlink 没有 license 切过去没用；
            RBMaster 正通过 SIM 下载 license，切换会中断下载

else if (policy == PREFER_SIM || PREFER_ROAMLINK)
    → 切换到 Roamlink（roamlink_starting）
      记录 roamlink_fallback_timer（策略2回切用）

else (policy == FORCE_SIM)
    → 重拨 SIM（跳 reg_check），永不切换
```

---

#### 触发点 B：`dial_stat_net_connected` — SIM TCP 通过，策略1主动回切

```c
// 条件
nw_tcp_connectivity_test() == true             // TCP 通过
policy == NET_POLICY_PREFER_ROAMLINK           // 策略1
roamlink_available == true                     // Roamlink 可用
sim_fallback_timer.tv_sec > 0                  // 曾经进入过 SIM 备用
diff(sim_fallback_timer, now) >= 300s          // 已在 SIM 备用 300 秒
```

**逻辑含义：**

策略1的语义是"优先 Roamlink"。切到 SIM 备用后不能一直待在 SIM 上，需要定期尝试切回 Roamlink（主通道可能已经恢复了）。等300秒的原因是给 Roamlink 足够的恢复时间，避免频繁切换。

**`sim_fallback_timer.tv_sec > 0` 的保护意义：**

`sim_fallback_timer` 初始化为零值（`{0,0}`）。策略1开机直接走 Roamlink，`net_connected` 状态不会被进入，`sim_fallback_timer` 一直是零。但万一程序 bug 导致意外进入了 `net_connected`，这个检查确保不会误触发回切（没有进入过 SIM 备用就不应该触发回切）。

---

#### 触发点 C：`dial_stat_net_connected` — rx_packets 60秒无增长

```c
// 条件
diff(dial_timer, now) > DIAL_TIMEOUT_SECONDS  // 超过60秒
```

**逻辑含义：**

底层连接还"活着"（`nw_get_connect_state()` 返回1），但60秒内没有任何数据包收进来，说明连接可能进入了僵死状态（路由问题、NAT 超时等）。

**这里不做通道切换，只做重拨：**

```c
dial_st = dial_stat_reg_check;
tcp_fail_count = 0;
```

重新走完 SIM 拨号流程。之所以不切换，是因为 60 秒无收包可能是业务层长时间没有数据交换（设备空闲），不一定是网络故障。TCP 测试才是更可靠的判断依据。

---

#### 触发点 D：`dial_stat_roamlink_starting` — Roamlink 连通（正向跳转）

```c
// 条件
nw_tcp_connectivity_test() == true
```

这不算"切换"，而是从等待状态进入正常运行状态。但进入时的初始化很关键：

```c
nw_get_rmnet_rx_packets_sum(&roamlink_rx_packets);  // 记录当前收包数作为基准
roamlink_no_data_timer = cur_timer;                 // 重置无数据计时器
dial_st = dial_stat_roamlink_active;
```

**为什么要记录 rx_packets 基准？**

进入 `roamlink_active` 时，如果用 0 作为基准，而此时 rmnet_data0 已经积累了很大的收包数（比如之前 SIM 产生的收包），第一次检查就会看到 `cur_rx > 0 = roamlink_rx_packets`，误判为"有增长，业务正常"。读取当前实际值作为基准，之后的增量才真正代表 Roamlink 通道的数据流量。

---

#### 触发点 E：`dial_stat_roamlink_starting` — 300秒超时，切回 SIM

```c
// 条件
diff(roamlink_timer, now) > ROAMLINK_CONNECT_WAIT_SEC  // 超过300秒
policy == PREFER_ROAMLINK || PREFER_SIM
```

Roamlink 启动后300秒还没连通（TCP 测试一直失败），认为 Roamlink 无法使用，切回 SIM。

**`sleep(10)` 在这个状态里的作用：**

```c
sleep(10);  // 10秒检查一次
```

每10秒做一次 TCP 测试，总共最多等30次（300秒/10秒）。比 `net_connected` 状态的60秒间隔更频繁，原因是希望 Roamlink 一旦连通就能快速检测到，减少断网时间。

---

#### 触发点 F：`dial_stat_roamlink_active` — Roamlink TCP 失败 3 次

```c
// 条件
diff(roamlink_timer, now) >= ROAMLINK_CHECK_INTERVAL_SEC  // 每30秒
nw_tcp_connectivity_test() == false
roamlink_fail_count >= ROAMLINK_FAIL_THRESHOLD            // 累计 >= 3次
```

**时序分析：**

```
T+0s    进入 roamlink_active
T+30s   第1次 TCP 测试 → FAIL，fail_count=1
T+60s   第2次 TCP 测试 → FAIL，fail_count=2
T+90s   第3次 TCP 测试 → FAIL，fail_count=3
        触发切换！（从进入 active 到切换，最快90秒）
```

对比 SIM 通道（最快180秒），Roamlink 通道的切换检测更快（30s间隔 vs 60s间隔），因为 Roamlink 作为虚拟SIM稳定性相对弱，需要更敏感的检测。

**注意 roamlink_active 里没有 rx_packets 保活超时：**

SIM 通道有 60 秒 rx_packets 无增长就重拨的逻辑，但 `roamlink_active` 没有。Roamlink 通道的保活完全依赖 TCP 测试（每30秒一次）和业务层检测（rx_packets 无增长120秒）。

---

#### 触发点 G：`dial_stat_roamlink_active` — TCP 通过，策略2主动回切 SIM

```c
// 条件
nw_tcp_connectivity_test() == true
policy == NET_POLICY_PREFER_SIM
roamlink_fallback_timer.tv_sec > 0
diff(roamlink_fallback_timer, now) >= ROAMLINK_FALLBACK_RETRY_SEC  // 300秒
```

对称于触发点B。策略2语义是"优先SIM"，在 Roamlink 备用通道稳定300秒后，尝试回切回首选的 SIM 通道。

**`did_switch` 标志（Bug8修复）的作用：**

```c
bool did_switch = false;

// 先判断策略2回切
if (!did_switch && policy == PREFER_SIM && roamlink_fallback_timer.tv_sec > 0) {
    if (fallback_dif >= 300s) {
        // 执行回切...
        did_switch = true;  // 标记已切换
    }
}

// 再判断业务层检测
if (!did_switch && nw_get_rmnet_rx_packets_sum(&cur_rx)) {
    // ...
}
```

如果策略2回切已经触发（`did_switch = true`），后面的业务层检测就跳过。原因是两个分支都可能调用 `roamlink_stop_service()`，如果都触发了，这个函数会被调用两次，`dial_st` 也会被覆盖两次，造成状态机混乱。

---

#### 触发点 H：`dial_stat_roamlink_active` — 业务层检测，rx_packets 120秒无增长

```c
// 条件
nw_tcp_connectivity_test() == true                      // TCP 通过（！）
!did_switch                                             // 没有更高优先级的切换
nw_get_rmnet_rx_packets_sum(&cur_rx)                   // 读取成功
cur_rx <= roamlink_rx_packets                          // 收包数没增长
diff(roamlink_no_data_timer, now) >= 120s              // 已经120秒没有新包
```

**这个检测的独特价值（理论上）：**

普通 TCP 测试只能验证"能不能连上 18.196.0.17:22"，不能验证"Roamlink 的虚拟 SIM 是否真正注册成功且数据通路正常"。

理论场景：
```
Roamlink TCP 测试通过（18.196.0.17 可达）
但 Roamlink 虚拟 SIM 注册失败
→ rmnet_data* 没有数据包
→ rx_packets 不增长
→ 120秒后业务层检测触发切换
→ TCP 测试触发不了，只有这个检测能发现
```

实际上由于 TCP 测试本身产生 rx_packets，两者耦合，见之前分析。

---

### 4.2 切换路径全图

用图表示所有触发点和转移方向：

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

### 4.3 Roamlink→SIM 切换时的关键细节：`sim_initialized` 判断

每个 Roamlink→SIM 的切换路径都有这段代码：

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

**什么时候 `sim_initialized = false`？**

策略1或3开机，`dial_stat_none` 直接跳 `roamlink_starting`，完全没走过 SIM 路径，`sim_initialized` 一直是 false。第一次切回 SIM 需要完整初始化。

**什么时候 `sim_initialized = true`？**

只要走到过 `dial_stat_init` 里的 `QL_MCM_NW_Client_Init()`，就置为 true。之后无论多少次切换，都保持 true，走 `sim_init` 重入路径。

**`is_func_called = true` 的作用：**

`is_func_called` 控制 `dail_start_data_call()` 里是否重新调用 `QL_Data_Call_Init()`（注册回调函数）。每次 SIM 拨号前都需要重新注册一次，所以切回 SIM 时必须置 `true`。

---

### 4.4 切换时的计时器管理总结

切换时涉及多个计时器的重置，一旦漏掉可能导致下一轮检测时机不对：

**SIM → Roamlink 切换时：**

| 计时器 | 操作 | 原因 |
|---|---|---|
| `roamlink_timer` | 设为当前时间 | Roamlink 开始等待连通，计时从0开始 |
| `roamlink_fail_count` | 清零 | 新一轮 Roamlink 连通尝试，失败计数归零 |
| `roamlink_fallback_timer` | 设为当前时间 | 记录进入 Roamlink 备用的时刻（策略2回切用） |
| `p_apn_obj` | 置 NULL | 强制下次 SIM 拨号重新查 APN |

**Roamlink → SIM 切换时：**

| 计时器 | 操作 | 原因 |
|---|---|---|
| `tcp_test_timer` | 重置为当前时间 | SIM 刚联网，TCP 测试从当前时刻开始，不要立刻触发 |
| `tcp_fail_count` | 清零 | 新一轮 SIM 连通监控，失败计数归零 |
| `sim_fallback_timer` | 设为当前时间 | 记录进入 SIM 备用的时刻（策略1回切用） |
| `p_apn_obj` | 置 NULL | 强制重新查 APN |

---

### 4.5 三段切换序列的完整时序（以策略1为例）

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

## 五、三个关键的"防抖"机制

### 5.1 失败计数防抖

SIM 通道和 Roamlink 通道都有连续失败3次才切换的机制：

```
为什么不单次失败就切换？
  → 网络偶尔抖动、服务器临时不响应，单次失败很常见
  → 单次切换会造成频繁无意义的通道来回切换
  → 3次连续失败（SIM通道需要180秒，Roamlink通道需要90秒）确保是真正的持续故障

中间任何一次成功，计数清零：
  tcp_fail_count = 0   (TCP通过时)
  roamlink_fail_count = 0  (Roamlink TCP通过时)
```

### 5.2 300秒稳定等待防抖

回切时等待300秒，确保备用通道稳定后再尝试回主通道：

```
为什么不立刻回切？
  → 主通道可能只是短暂恢复，马上回去又会失败
  → 频繁切换会造成业务中断
  → 300秒给主通道足够的恢复时间

实际上回切失败怎么办？
  → 回切尝试时发 RBstartServiceMaster
  → 如果 Roamlink 300秒内连不通（roamlink_starting 超时）
  → 自动切回 SIM
  → sim_fallback_timer 被重置
  → 再等300秒后再尝试
```

### 5.3 `did_switch` 互斥防抖

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

## 六、切换过程中断网时间分析

### SIM → Roamlink（策略1 Roamlink 主动回切，或策略2 SIM失败切换）

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

### Roamlink → SIM（策略1 Roamlink 失败切 SIM，或策略2 Roamlink 回切）

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

## 七、一个需要特别注意的潜在问题

### AT 串口互斥锁在切换时的风险

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

如果 dial_task 切换时持有锁发 `AT+COPS=0`（超时5秒），主线程的 CSQ 查询会被阻塞最多5秒。这不影响功能，但会导致 CSQ 字段更新延迟。反之亦然，AT+CSQ 超时1秒，AT+COPS=0 最多等待1秒才能获取锁。

这是已知的设计权衡，目前没有问题。
