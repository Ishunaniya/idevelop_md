# open_dial 综合完整分析文档

> **版本**：V1.29.07  
> **分析日期**：2026-05-15  
> **分支**：`feature/dual_channel_switch`  
> **仓库**：`/home/tronlong/lyp/code/open_dial_for_artery`  
> **本文档融合来源**：
> - `log_level_analysis.md`
> - `open_dial_roamlink_analysis.md`
> - `open_dial_roamlink_analysis_v2.md`
> - `roamlink_migration_analysis.md`
> - `SIM_Roamlink_切换逻辑深度分析.md`
> - `方案A_v2_源码完整分析A.md`
> - `方案A_v2_源码逐行分析B.md`（节选）

---

## 目录

1. [第1章：项目概览](#第1章项目概览)
2. [第2章：架构总览](#第2章架构总览)
3. [第3章：状态机深度分析](#第3章状态机深度分析)
4. [第4章：双通道切换逻辑](#第4章双通道切换逻辑)
5. [第5章：Roamlink 模块（RBMaster 管理）](#第5章roamlink-模块rbmaster-管理)
6. [第6章：网络层（nw.c）](#第6章网络层nwc)
7. [第7章：APN 选择（apn.c）](#第7章apn-选择apnc)
8. [第8章：状态输出（/tmp/dial_status）](#第8章状态输出tmpdial_status)
9. [第9章：部署包分析](#第9章部署包分析)
10. [第10章：OTA 版与 U 版安装脚本差异](#第10章ota-版与-u-版安装脚本差异)
11. [第11章：已知缺陷与生产注意事项](#第11章已知缺陷与生产注意事项)
12. [第12章：配置速查表](#第12章配置速查表)

---

## 第1章：项目概览

### 1.1 项目定位

`open_dial` 是运行于嵌入式 Linux（OpenNPC，ARMv7）的蜂窝调制解调器拨号管理程序，专为 Quectel EC2x/EG2x 系列模组设计。其核心职责是建立并持续维持蜂窝数据连接，同时支持**物理 SIM 卡**与 **Roamlink 虚拟 SIM**（双通道）两种工作模式的自动切换。

当前版本：**V1.29.07**（`main.c`：`MAIN_VERSION=1, SUB_VERSION=29, TEST_VERSION=7`）

配套的 `roamlink_deploy` 部署仓库将编译好的 `dial` 二进制与闭源的 `RBMaster`（Roamlink 虚拟 SIM 服务）及 `check_network.sh` 看门狗脚本打包为部署包。

### 1.2 启动链

设备启动后，SysV init 依序执行：

```
rc5.d/S60start_check_network
    └─ /etc/init.d/start_check_network start
           └─ start-stop-daemon → /usrdata/check_network.sh（后台常驻）
                  └─ 启动并监控 /usr/dial/dial 进程
```

`check_network.sh` 为纯进程看门狗（v2.0），每 30 秒检查 `dial` 是否仍在运行，宕机即重启。

### 1.3 运行时配置文件

| 文件路径 | 用途 |
|---------|------|
| `/usrdata/network.ini` | 网络策略（`network_select` 字段，进程启动时读取一次） |
| `/usr/dial/apn.json` | APN 配置，按 ICCID 前6位匹配 |
| `/opt/conf.ini` | RBMaster 运行时配置；Roamlink 激活的必要条件 |
| `/usrdata/roamlink/etc/.pconfig/license.cer` | Roamlink 主路径证书（RBMaster 读取） |
| `/data/ufs/license.cer` | Roamlink 证书备份（持久化 flash 分区） |
| `/usrdata/roamlink/RBMaster` | RBMaster 闭源二进制 |
| `/tmp/dial_status` | JSON-like INI 状态输出（运行时、版本、中断统计、当前状态） |
| `/tmp/network_status` | dial 写入：`1`=已连接，`0`=断开 |
| `/tmp/network_type` | dial 写入：`1`=SIM，`2`=Roamlink，`0`=切换中 |
| `/tmp/network_csq` | 主线程每 5s 写入：信号强度整数 |
| `/tmp/network_plmn` | dial 写入：当前运营商 PLMN（MCC+MNC） |
| `/usr/dial/tz.ini` | dial 从网络获取时区后写入 |
| `/media/sdcard/seas_log_dial.log` | 轮转日志（最大 50 MB；仅在 sdcard 存在且有 ≥1 GB 剩余时写入） |
| `/usrdata/check_network.log` | check_network.sh 看门狗日志（1 MB，自动轮转） |

### 1.4 交叉编译

```sh
source /path/to/opennpc-sdk/environment-setup-*
make       # 输出 ./dial
make clean
```

Makefile 关键参数：

- `CC = arm-oe-linux-gnueabi-gcc`
- `COMPILER_FLAGS = -march=armv7ve`
- 递归收集 `src/` 下所有 `.c` 文件
- 链接库：`libql_mgmt_client.so`、`libdsi_netctrl.so`、`libmcm.so`、`libql_common_api.a`、`libpthread`、`librt`

无单元测试。

---

## 第2章：架构总览

### 2.1 双线程模型

进程运行两个线程，共享 AT 串口文件描述符 `smd_fd`，用 `pthread_mutex_t g_at_port_mutex`（`main.c` 定义，`extern` 声明）保护。

```
main()
  ├─ seas_log_config_init()     等待最多 10s SD 卡（≥1 GB 可用）
  ├─ at_init()                  打开 /dev/smd8，发送 ATE0/AT/ATI
  ├─ dial_mng_new()             调用 roamlink_probe()，确定初始路由策略
  ├─ pthread_create(dial_task)  ← 主业务线程
  └─ 主线程循环：每 5s 调用 nw_at_get_csq()，写 /tmp/network_csq

dial_task()
  └─ 永久循环 switch(dial_mng->dial_st)
       └─ 完整的连接生命周期：状态机 + TCP 测试 + 通道切换 + 状态输出
```

**重要约束**：`nw_at_get_csq()` 内部已持有 `g_at_port_mutex`，外层代码**不得**再次加锁（会死锁）。此约束已在 `main.c` 注释中明确标注。

### 2.2 模块地图

```
main.c              — 入口：日志初始化、线程创建、CSQ 轮询循环
src/dial/           — dial.h（结构体/常量），dial.c（状态机 + 状态输出）
src/roamlink/       — roamlink.h/.c：RBMaster 生命周期、probe、发送命令
src/nw/             — nw.h/.c：MCM API 封装、TCP 测试、rx_packets 读取
src/at/             — AT 命令在 /dev/smd8 上的收发
src/sim/            — SIM 初始化/反初始化、ICCID/IMSI 获取（MCM API）
src/apn/            — 从 /usr/dial/apn.json 按 ICCID 获取 APN 配置
src/status/         — 写 /tmp/dial_status（原子 rename via .tmp）
src/tz/             — AT+QLTS=1 获取时区 → /usr/dial/tz.ini
src/seas_log/       — 结构化轮转日志，带 SD 卡检测
src/json/           — 捆绑的 cJSON 库
src/opt_iniparser/  — INI 文件解析器（roamlink_read_policy 使用）
src/cc_deque/       — 双端队列（apn.c 运营商列表轮询使用）
```

### 2.3 核心数据结构 `dial_mng_t`

```c
typedef struct {
    /* 状态机 */
    dial_stat_enu dial_st;           // 当前状态

    /* 策略与探测 */
    int network_select;              // 策略 1~4，来自 network.ini
    bool roamlink_available;         // RBMaster+conf.ini+license 均存在
    bool license_pending;            // 等待 license OTA 下载
    bool sim_initialized;            // QL_MCM_NW_Client_Init 已调用
    bool rbmaster_started;           // license_pending: RBMaster 已启动标志

    /* SIM 通道 */
    int tcp_fail_count;
    struct timespec tcp_test_timer;

    /* Roamlink 通道 */
    int roamlink_fail_count;
    struct timespec roamlink_timer;
    uint64_t roamlink_rx_packets;    // 上次检测时的 rx_packets 基线
    struct timespec roamlink_no_data_timer;
    struct timespec roamlink_biz_log_timer;

    /* 回退定时器（零值 = "未处于回退模式"） */
    struct timespec sim_fallback_timer;      // 策略1：进入 SIM 备用的时间
    struct timespec roamlink_fallback_timer; // 策略2：进入 Roamlink 备用的时间

    /* license 下载等待 */
    struct timespec license_wait_start;
    struct timespec license_check_timer;

    /* 中断统计 */
    bool net_was_down;
    bool ever_connected;
    struct timespec net_down_since;
    int outage_count;
    long last_outage_sec;
    long total_outage_sec;

    /* 硬件 / 其他 */
    int smd_fd;                      // AT 串口 fd（/dev/smd8）
    uint8_t csq;                     // 主线程每 5s 更新
    sim_mng_t *p_sim_mng;
    apn_obj_t *p_apn_obj;            // NULL = 下次拨号强制重新查找
    struct timespec dial_timer;      // rx_packets 超时定时器
    CC_Deque *deque_oper;
    // ... MCM 句柄、ifaddrs 等
} dial_mng_t;
```

### 2.4 AT 串口通信（at.c）

`Ql_SendAT()` 实现要点：

1. 发送前先用 `read()` 清空缓冲区中残留数据
2. 发送 AT 命令后用 `select()` 等待响应，超时在循环中重置
3. 响应数据通过 `memcpy` 复制到调用方提供的缓冲区

`at_init()` 以 `O_RDWR|O_NONBLOCK|O_NOCTTY` 打开 `/dev/smd8`，依次发送 `ATE0`/`AT`/`ATI` 完成握手。

---

## 第3章：状态机深度分析

### 3.1 状态枚举总览

`dial_stat_enu` 共定义 19 个状态（`src/dial/dial.h`）：

| 状态名 | 编号 | 含义 |
|--------|------|------|
| `dial_stat_none` | 0 | 初始路由决策 |
| `dial_stat_init` | 1 | MCM NW 客户端初始化 |
| `dial_stat_sim_init` | 2 | SIM 初始化 |
| `dial_stat_sim_check` | 3 | 等待 SIM 卡就绪 |
| `dial_stat_sim_op` | 4 | 读取 ICCID |
| `dial_stat_reg_check` | 5 | PS 域注册检查 |
| `dial_stat_cereg_check` | 6 | AT+CEREG? 检查 |
| `dial_stat_precondition_check` | 7 | 数据呼叫预条件检查 |
| `dial_stat_pre_start_call` | 8 | APN 配置 + Profile 设置 |
| `dial_stat_start_call` | 9 | 启动数据呼叫 |
| `dial_stat_wait_for_connect` | 10 | 轮询连接状态 |
| `dial_stat_net_connected` | 11 | SIM 通道监控 |
| `dial_stat_list_oper` | 12 | AT+COPS=? 扫描运营商 |
| `dial_stat_select_oper` | 13 | AT+COPS=1,2,\<PLMN\> 选择运营商 |
| `dial_stat_stop_cfun` | 14 | AT+CFUN=0 |
| `dial_stat_start_cfun` | 15 | AT+CFUN=1 |
| `dial_stat_roamlink_starting` | 16 | 等待 Roamlink 建立连接 |
| `dial_stat_roamlink_active` | 17 | Roamlink 通道监控 |
| `dial_stat_max` | 18 | 枚举边界 |

### 3.2 状态流转图

```
dial_stat_none
  │
  ├─ license_pending=true ──────────────────────────→ dial_stat_init（SIM 先上，下载 license）
  ├─ 策略1/3 且 roamlink_available=true ──────────→ 启动 RBMaster → dial_stat_roamlink_starting
  └─ 策略2/4，或 roamlink 不可用 ────────────────→ dial_stat_init

═══ SIM 路径 ══════════════════════════════════════════════════════════════

dial_stat_init
  └─ QL_MCM_NW_Client_Init()（整个运行期只调用一次）
       ↓
dial_stat_sim_init
  └─ sim_op(SIM_OP_INIT)
       ↓
dial_stat_sim_check
  └─ nw_get_sim_card_status()；超时 3600s → dial_stat_stop_cfun
       ↓
dial_stat_sim_op
  └─ sim_op_handler(get_iccid)
       ↓
dial_stat_reg_check
  └─ nw_reg_status_check()（PS 域）；超时 300s → 切 Roamlink 或 cfun 重置
       ↓
dial_stat_cereg_check
  └─ AT+CEREG? stat=1 或 5；超时 300s → 切 Roamlink 或 cfun 重置
       ↓
dial_stat_precondition_check
  └─ QL_Data_Call_Init_Precondition()
       ↓
dial_stat_pre_start_call
  └─ apn_get_apn_obj(iccid) + QL_Data_Call_Set_Default_Profile()
       ↓
dial_stat_start_call
  └─ QL_Data_Call_Start()
       ↓
dial_stat_wait_for_connect
  └─ 轮询 nw_get_connect_state()；超时 60s：
       is_oper_select=true  → dial_stat_list_oper
       is_oper_select=false → AT+COPS=0 → dial_stat_reg_check
       ↓（连接成功）
dial_stat_net_connected
  └─ SIM 通道监控（60s TCP 测试、60s rx_packets 超时）

═══ 运营商选择子路径 ════════════════════════════════════════════════════════

dial_stat_list_oper → AT+COPS=?（180s 超时）→ 填充 deque_oper
  ↓
dial_stat_select_oper → AT+COPS=1,2,<PLMN>（30s，2次重试）→ 遍历 deque
  ↓（选择成功）→ dial_stat_reg_check

═══ cfun 重置路径 ════════════════════════════════════════════════════════════

dial_stat_stop_cfun → AT+CFUN=0
  ↓
dial_stat_start_cfun → AT+CFUN=1 → dial_stat_sim_init

═══ Roamlink 路径 ════════════════════════════════════════════════════════════

dial_stat_roamlink_starting
  └─ 每 10s TCP 测试，最长等待 300s
       连接成功 → 读 rx_packets 基线 → dial_stat_roamlink_active
       超时：策略1/2 → 回 SIM；策略3 → 重启 Roamlink
       ↓（连接成功）
dial_stat_roamlink_active
  └─ Roamlink 通道监控（每 30s TCP 测试）
```

### 3.3 关键状态行为详解

#### `dial_stat_none` — 初始路由决策

```
if license_pending:
    → dial_stat_init  # 先用 SIM 上网，等待 RBMaster 从云端下载 license

if (policy 1 or 3) AND roamlink_available:
    → 启动 RBMaster → dial_stat_roamlink_starting

if (policy 2 or 4) OR NOT roamlink_available:
    → dial_stat_init
```

#### `dial_stat_net_connected` — SIM 通道监控

每次循环均执行：

1. `nw_get_connect_state()` 返回 0 → 跳至 `dial_stat_reg_check`
2. 读取 `rx_packets`：若增加 → 重置 `dial_timer`；若 `dial_timer` > 60s 无增加 → 跳至 `dial_stat_reg_check`
3. 每 60s（`TCP_TEST_INTERVAL_SECONDS`）执行 TCP 测试：
   - **通过**：清零 `tcp_fail_count`；策略1 且 `sim_fallback_timer>0` 且已过 300s → 切回 Roamlink
   - **失败**：`tcp_fail_count++`，重置 `dial_timer`（防止 rx_packets 超时抢先）；达 3 次失败：
     - `license_pending` → `reg_check`（保持 SIM，不切换）
     - 策略1/2 → 切 Roamlink（`roamlink_starting`）
     - 策略4 → `reg_check`（SIM 重拨）

**为何 TCP 失败时要重置 `dial_timer`**：若不重置，TCP 测试失败时 rx_packets 也通常不增长，两个超时可能同时触发，造成竞态——优先执行 TCP 路径（切换），rx_packets 路径（重拨）须被阻止。

#### `dial_stat_roamlink_starting` — 等待 Roamlink 建立连接

- 每 10s：TCP 测试
  - 通过 → 读 `rmnet_data*` rx_packets 基线，进入 `roamlink_active`
  - 300s 超时：策略1/2 → 切回 SIM；策略3 → 重启 Roamlink

#### `dial_stat_roamlink_active` — Roamlink 通道监控

每 30s TCP 测试：

- **通过**：
  1. `did_switch=false`；策略2 且 `roamlink_fallback_timer>0` 且已过 300s → 切回 SIM；`did_switch=true`
  2. `!did_switch`：读 `rmnet_data*` rx_packets 总和；若增加 → 更新基线；若 120s 内未增加 → 策略1/2 切 SIM，策略3 重启 Roamlink
- **失败**：`roamlink_fail_count++`；达 3 次：策略1/2 → 切 SIM；策略3 → 重启 Roamlink

#### `did_switch` 互斥标志的作用

同一个 TCP 测试成功周期内，"策略2回退检查"和"业务层无数据检查"只应触发其一：若回退已执行切换（`did_switch=true`），则跳过 rx_packets 检查，避免双重切换。

#### `dial_stat_reg_check` / `dial_stat_cereg_check` — 注册等待

300s 总超时（`reg_check_timer` 跨两个状态共享）：

- 超时后：策略1/2 且 `roamlink_available` → 切 Roamlink；否则 → cfun 重置

---

## 第4章：双通道切换逻辑

### 4.1 四种网络策略

策略由 `/usrdata/network.ini` 中的 `network_select` 字段控制，进程启动时读取一次。

| 值 | 常量名 | 行为描述 |
|----|--------|---------|
| `1` | `NET_POLICY_PREFER_ROAMLINK` | 优先 Roamlink；故障时回 SIM；每 300s 尝试切回 Roamlink |
| `2` | `NET_POLICY_PREFER_SIM` | 优先 SIM；故障时回 Roamlink；每 300s 尝试切回 SIM |
| `3` | `NET_POLICY_FORCE_ROAMLINK` | 强制 Roamlink；故障时仅重启 Roamlink，不切 SIM |
| `4` | `NET_POLICY_FORCE_SIM` | 强制 SIM；故障时仅重拨 SIM，不切 Roamlink。**默认值** |

策略读取由 `roamlink_read_policy()` 完成：用 `iniparser_load` 解析 INI，合法值为 1~4，超出范围返回 `NET_POLICY_DEFAULT=4`。

### 4.2 切换触发点汇总（8 个）

| # | 触发位置 | 触发条件 | 切换方向 |
|---|---------|---------|---------|
| 1 | `dial_stat_none` | 策略1/3 且 `roamlink_available` | → Roamlink |
| 2 | `dial_stat_reg_check/cereg_check` 超时 | 策略1/2 且 `roamlink_available` | → Roamlink |
| 3 | `dial_stat_net_connected` TCP 失败 3 次 | 策略1/2 | → Roamlink |
| 4 | `dial_stat_net_connected` TCP 通过 + 策略1 回退 | 策略1，`sim_fallback_timer` > 300s | → Roamlink |
| 5 | `dial_stat_roamlink_starting` 超时 300s | 策略1/2 | → SIM |
| 6 | `dial_stat_roamlink_active` TCP 失败 3 次 | 策略1/2 | → SIM |
| 7 | `dial_stat_roamlink_active` TCP 通过 + 策略2 回退 | 策略2，`roamlink_fallback_timer` > 300s | → SIM |
| 8 | `dial_stat_roamlink_active` 无数据 120s | 策略1/2 | → SIM |

### 4.3 SIM → Roamlink 切换序列（6 步）

```c
// 步骤1：停止物理 SIM 数据呼叫
dail_stop_data_call();

// 步骤2：清除 APN 缓存（强制下次重新查找）
p_apn_obj = NULL;

// 步骤3：确保 RBMaster 进程存活（幂等）
roamlink_start_master();

// 步骤4：AT+COPS=0（清除手动运营商锁定）
//        发送 "RBstartServiceMaster" TCP 命令
//        写入 network_type=2
roamlink_start_service(smd_fd);

// 步骤5：记录定时器
roamlink_timer = now;
roamlink_fail_count = 0;
roamlink_fallback_timer = now;  // 策略2 回退计时起点

// 步骤6：进入等待状态
dial_st = dial_stat_roamlink_starting;
```

### 4.4 Roamlink → SIM 切换序列（4 步）

```c
// 步骤1：发送 "RBstopServiceMaster" TCP 命令
//        写入 network_type=0
//        sleep(3)：等待模组注销虚拟 SIM
roamlink_stop_service();

// 步骤2：清除 APN 缓存
p_apn_obj = NULL;

// 步骤3：记录定时器，清零 SIM 侧计数器
sim_fallback_timer = now;       // 策略1 回退计时起点
tcp_test_timer = now;           // 防止 SIM 刚连接即触发 TCP 测试
tcp_fail_count = 0;

// 步骤4：根据 SIM 是否曾初始化决定入口点
if (!sim_initialized):
    dial_st = dial_stat_init    // 全路径（需调用 QL_MCM_NW_Client_Init）
else:
    is_func_called = true
    dial_st = dial_stat_sim_init  // 快速重入（跳过 MCM init）
```

### 4.5 通道切换定时器对照表

**SIM → Roamlink 时重置的字段**：

| 字段 | 操作 | 用途 |
|------|------|------|
| `roamlink_timer` | = now | Roamlink 等待连接及间隔计时 |
| `roamlink_fail_count` | = 0 | 清零失败计数 |
| `roamlink_fallback_timer` | = now | 策略2 回 SIM 计时起点 |
| `p_apn_obj` | = NULL | 强制重新查找 APN |

**Roamlink → SIM 时重置的字段**：

| 字段 | 操作 | 用途 |
|------|------|------|
| `tcp_test_timer` | = now | 防止 SIM 连接后立即触发 TCP 测试 |
| `tcp_fail_count` | = 0 | 清零失败计数 |
| `sim_fallback_timer` | = now | 策略1 回 Roamlink 计时起点 |
| `p_apn_obj` | = NULL | 强制重新查找 APN |

### 4.6 三重防抖机制

1. **TCP 失败计数**：需连续 3 次失败（`TCP_FAIL_THRESHOLD=3`、`ROAMLINK_FAIL_THRESHOLD=3`）才触发切换，单次毛刺不切换。
2. **rx_packets 超时**：120s 内无任何数据（`ROAMLINK_NO_DATA_TIMEOUT_SEC`）才判定为业务层故障，区分"有 TCP 连通但无流量"与"完全断网"。
3. **回退间隔**：策略1/2 的回退尝试间隔为 300s（`SIM_FALLBACK_RETRY_SEC`/`ROAMLINK_FALLBACK_RETRY_SEC`），防止在两个通道间高频震荡。

---

## 第5章：Roamlink 模块（RBMaster 管理）

### 5.1 `roamlink_probe()` 四态结果

`roamlink_probe()` 在 `dial_mng_new()` 中调用一次，按顺序执行三项检查：

| 结果 | 判断条件 | `dial_mng_new` 中的处理 |
|------|---------|----------------------|
| `ROAMLINK_PROBE_OK` | RBMaster + conf.ini + license 均存在且非空 | `roamlink_available = true` |
| `ROAMLINK_PROBE_NO_PACKAGE` | RBMaster 二进制缺失 | 本次运行永久 `FORCE_SIM` |
| `ROAMLINK_PROBE_CONF_MISSING` | RBMaster 存在，`/opt/conf.ini` 缺失 | 永久 `FORCE_SIM`（factoryApp 未运行） |
| `ROAMLINK_PROBE_LICENSE_MISSING` | RBMaster + conf.ini 存在，license 缺失/空 | 先尝试 `roamlink_license_restore_from_backup()`；若失败 → `license_pending=true`，临时 `FORCE_SIM` |

```
ROAMLINK_MASTER_PATH    = "/usrdata/roamlink/RBMaster"
ROAMLINK_CONF_PATH      = "/opt/conf.ini"
ROAMLINK_LICENSE_PATH   = "/usrdata/roamlink/etc/.pconfig/license.cer"
ROAMLINK_LICENSE_BACKUP_PATH = "/data/ufs/license.cer"
```

### 5.2 `license_pending` 流程（OTA 首次上线）

```
设备重启
  │
  ├─ roamlink_probe() → LICENSE_MISSING（主路径和备份路径均无 license）
  │       → license_pending = true
  │
  ↓
dial_stat_none → dial_stat_init（SIM 路径）
  │
  ↓ SIM 连接成功（dial_stat_net_connected）
  │
  ├─ !rbmaster_started → 启动 RBMaster（rbmaster_started = true）
  │
  ↓ 每 60s 检查 license 文件（最多等待 LICENSE_WAIT_TIMEOUT_SEC=300s ⚠️）
  │
  ├─ license 文件出现（两次 stat() 间隔 2s，大小稳定）
  │       → roamlink_license_backup_and_reboot()
  │              ├─ 备份到 /data/ufs/license.cer
  │              ├─ fsync → sync → sleep(2)
  │              └─ reboot(RB_AUTOBOOT)（不返回）
  │
  └─ 超时 → SEAS_LOG_ERROR，继续保持 SIM 连接（不重启不切换）
```

**⚠️ 生产注意**：`LICENSE_WAIT_TIMEOUT_SEC` 当前值为 `300`（测试值），**发布前必须改为 `7200`**（2小时）。

### 5.3 `roamlink_license_appeared()` 稳定性检测

连续两次 `stat()` 调用，间隔 2s，要求：
- 两次均成功
- 文件大小 > 0 且两次相等

目的：防止把正在写入一半的 license 文件判断为"已到达"。

### 5.4 RBMaster 双重验证机制

`roamlink_is_master_running()` 使用两个独立检查，均通过才判定为"真正运行"：

```c
// 检查1：扫描 /proc/<pid>/cmdline 找 "RBMaster"
//        僵尸进程仍会出现在此处，所以不能仅凭此判断
foreach pid in /proc/:
    if cmdline contains "RBMaster": found = true

// 检查2：非阻塞 TCP 连接到 127.0.0.1:5568，超时 1s
//        僵尸进程不持有端口，此检查可区分真实运行与僵尸
connect(127.0.0.1, ROAMLINK_CTRL_PORT=5568, timeout=1s)
```

### 5.5 RBMaster 控制接口

RBMaster 监听 `127.0.0.1:5568`，接收明文 TCP 命令：

| 命令 | 作用 |
|------|------|
| `"RBstartServiceMaster"` | 虚拟 SIM 接管调制解调器 |
| `"RBstopServiceMaster"` | 虚拟 SIM 释放调制解调器，还给物理 SIM |

`roamlink_start_master()` 逻辑：
1. 调用 `roamlink_is_master_running()`
2. 若未运行：`fork()` + `exec(ROAMLINK_MASTER_PATH)`，`sleep(3)` 等待初始化

`roamlink_start_service(smd_fd)` 步骤：
1. `AT+COPS=0`（清除手动运营商锁定，防止 Roamlink 注册被锁定运营商拒绝）
2. 发送 `"RBstartServiceMaster"`
3. 写 `network_type=2`

`roamlink_stop_service()` 步骤：
1. 发送 `"RBstopServiceMaster"`
2. 写 `network_type=0`
3. `sleep(3)`（等待调制解调器注销虚拟 SIM）

---

## 第6章：网络层（nw.c）

### 6.1 导出函数总览

| 函数 | 描述 |
|------|------|
| `nw_data_call()` | 建立数据连接（始终使用 `profile_idx=1`，Qualcomm 自动路由） |
| `nw_tcp_connectivity_test(host, port, timeout_sec)` | 非阻塞 TCP 连接到 `18.196.0.17:22`，5s 超时，返回 `bool` |
| `nw_get_rmnet_rx_packets_sum(p_sum)` | 读取 `/sys/devices/virtual/net/rmnet_data*/statistics/rx_packets`，汇总所有接口 |
| `nw_get_connect_state(profile_idx)` | 调用 `QL_Data_Call_Info_Get()`；返回 1=已连接，0=断开，-1=错误 |
| `nw_at_get_csq(smd_fd)` | `AT+CSQ` → 整数 CSQ；通过 `system()` 写 `/tmp/network_csq` |
| `nw_mark_plmn(mcc, mnc)` | 通过 `system()` 写 `/tmp/network_plmn` ⚠️ 注入风险 |
| `nw_mark_network_status(status)` | 写 `/tmp/network_status` ⚠️ 缺少 O_TRUNC 和 mode |
| `nw_at_get_cereg_stat(smd_fd)` | `AT+CEREG?`，返回 stat 字段（1=本地注册，5=漫游） |
| `nw_get_ifaddrs()` | 获取网络接口地址 ⚠️ 浅拷贝后 freeifaddrs 导致悬空指针 |
| `nw_reg_status_check()` | 检查 PS 域注册状态 |
| `nw_get_sim_card_status()` | 状态 `0xB00==SIM_CARD_READY(10)` 判断 SIM 就绪 |

### 6.2 TCP 连通性测试实现（6 步）

```c
bool nw_tcp_connectivity_test(const char *host, int port, int timeout_sec) {
    // 步骤1：创建 socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    // 步骤2：设置非阻塞
    fcntl(fd, F_SETFL, O_NONBLOCK);

    // 步骤3：发起连接（立即返回 EINPROGRESS）
    connect(fd, &addr, sizeof(addr));  // errno == EINPROGRESS

    // 步骤4：select() 等待可写，超时 5s
    FD_ZERO(&wfds); FD_SET(fd, &wfds);
    tv.tv_sec = timeout_sec;  // 实际使用 5s
    ret = select(fd+1, NULL, &wfds, NULL, &tv);

    // 步骤5：ret==0 → 超时（服务器 5s 内不可达）
    //        ret<0 → 错误
    //        ret>0 → 继续

    // 步骤6：getsockopt(SO_ERROR) 检查实际连接结果
    //   0 → 成功（已建立 TCP 连接）
    //   ECONNREFUSED → 服务器在线但端口 22 关闭
    //   其他错误 → 失败
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
    close(fd);
    return (err == 0);
}
```

三种失败模式：

| 失败点 | 错误 | 含义 |
|--------|------|------|
| `connect()` 阶段 | `ENETUNREACH` | 路由不存在（通道未建立或已断） |
| `select()` 超时 | `ret==0` | 服务器 5s 内无响应 |
| `getsockopt` | `ECONNREFUSED` | 服务器在线但 22 端口关闭 |

### 6.3 rx_packets 读取路径

```c
// 读取路径：/sys/devices/virtual/net/rmnet_data*/statistics/rx_packets
// 使用 glob() 匹配所有 rmnet_data* 接口，汇总 rx_packets 之和
int nw_get_rmnet_rx_packets_sum(uint64_t *p_sum) {
    glob_t globbuf;
    glob("/sys/devices/virtual/net/rmnet_data*/statistics/rx_packets",
         GLOB_NOSORT, NULL, &globbuf);
    *p_sum = 0;
    for each path in globbuf:
        read file → parse uint64_t → *p_sum += value;
    globfree(&globbuf);
}
```

此函数用于 Roamlink 通道的业务层存活检测：若 120s 内 `rx_packets` 总和不增加，则判定为业务层断流（`ROAMLINK_NO_DATA_TIMEOUT_SEC=120`）。

### 6.4 已知缺陷

**缺陷1：`nw_mark_plmn()` shell 注入风险（中等）**

```c
// 现有实现：MCC/MNC 来自调制解调器，若含特殊字符可被 shell 解释
char cmd[64];
snprintf(cmd, sizeof(cmd), "echo %s %s > /tmp/network_plmn", mcc, mnc);
system(cmd);  // ⚠️ 风险

// 建议修复：
int fd = open("/tmp/network_plmn", O_WRONLY|O_CREAT|O_TRUNC, 0644);
dprintf(fd, "%s %s\n", mcc, mnc);
close(fd);
```

**缺陷2：`nw_mark_network_status()` 无 O_TRUNC 且无 mode（低）**

```c
// 现有实现：
int fd = open(NW_STATUS_PATH, O_WRONLY | O_CREAT);
// 缺少 O_TRUNC：若旧内容更长，会有残留
// 缺少 mode：文件权限不确定

// 建议修复：
int fd = open(NW_STATUS_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
```

**缺陷3：`nw_get_ifaddrs()` 悬空指针（中等）**

```c
// 现有实现：
getifaddrs(&ifaddr);
// 浅拷贝 struct ifaddrs（ifa_name 是指向内部缓冲区的指针）
memcpy(&local_copy, ifaddr, sizeof(struct ifaddrs));
freeifaddrs(ifaddr);  // ⚠️ 此时 local_copy.ifa_name 已是悬空指针
```

---

## 第7章：APN 选择（apn.c）

### 7.1 JSON 配置格式

```json
[
  {
    "iccid_prefix": "898600",
    "apn": "cmnet",
    "username": "",
    "password": "",
    "operators": ["46000", "46002", "46007"]
  },
  ...
]
```

匹配规则：取 ICCID 前 6 位（`iccid_prefix`）精确匹配。

### 7.2 ICCID 匹配与 APN 设置

`apn_get_apn_obj(iccid)` 流程：

1. 调用 `apn_scan_from_json()` 读取并解析 `/usr/dial/apn.json`（`fread` + cJSON 解析）
2. 对每条记录，取 ICCID 前 6 位与 `iccid_prefix` 对比
3. 匹配后构建 `apn_obj_t`，运营商列表填入 `CC_Deque`

### 7.3 运营商队列（`CC_Deque`）

匹配到 APN 条目后，`operators` 数组中的所有 PLMN 压入双端队列。`dial_stat_select_oper` 状态遍历队列，对每个 PLMN 尝试 `AT+COPS=1,2,<PLMN>`；失败则轮到下一个，最多尝试 2 次。

### 7.4 `apn_scan_idx()` 死代码

```c
int apn_scan_idx(apn_obj_t *p_apn_obj) {
    return apn_set(1, p_apn_obj);  // ← 第4行直接 return
    // 下面 100+ 行 APN 列表扫描代码永远不会执行
    // 这是故意设计（强制 profile_idx=1，Qualcomm 自动路由）
    // 但代码未清理，属于死代码
    ...
}
```

这是有意为之：Qualcomm 平台始终使用 `profile_idx=1` 让底层自动路由，不需要也不应使用其他 profile。但该死代码未被删除，属于已知技术债。

### 7.5 `apn_scan_from_json()` 资源泄漏

```c
// fread 错误路径：
char *str = malloc(file_size + 1);
fread(str, 1, file_size, fp);
if (error) {
    // ⚠️ 未执行 free(str)
    // ⚠️ 未执行 fclose(fp)
    return NULL;
}
```

此路径在正常运行中极少触发，但属于低严重度资源泄漏。

---

## 第8章：状态输出（/tmp/dial_status）

### 8.1 原子写入机制

```c
// src/status/status.c
// 先写到临时文件，再原子 rename，避免读者看到半写状态
FILE *fp = fopen("/tmp/dial_status.tmp", "w");
// ... 写入内容 ...
fclose(fp);
rename("/tmp/dial_status.tmp", "/tmp/dial_status");
```

### 8.2 完整输出格式

```ini
# dial status - auto generated by dial v1.29.07
# updated: 2026-03-19 10:05:38

[dial]
version=1.29.07
uptime=329                    # 进程运行秒数
state=roamlink_active         # 当前状态机状态名
policy=1                      # network_select 值
policy_name=PREFER_ROAMLINK   # 策略可读名称
channel=ROAMLINK              # SIM / ROAMLINK / SWITCHING / NONE
roamlink_available=1          # roamlink_probe() == OK
license_pending=0             # 正在等待 OTA license
license_wait_sec=0            # license_pending 时等待秒数

[sim]
iccid=89464283216100721879    # 从 MCM API 读取
csq=21                        # 主线程每 5s 更新
apn=internet.lte.cxn
profile_idx=1
plmn=46001                    # MCC+MNC

[network]
status=1                      # 1=连接，0=断开
type=2                        # 1=SIM，2=Roamlink
ip=10.88.197.109
ifname=rmnet_data0

[counters]
tcp_fail_count=0              # 当前连续 TCP 失败次数
roamlink_fail_count=0
roamlink_rx_packets=143       # 最近一次读取的 rx_packets

[roamlink]
state=active                  # none / starting / active
biz_no_data_sec=25            # 距上次 rx_packets 增长的秒数
roamlink_connect_wait_sec=0   # roamlink_starting 等待秒数
rbmaster_pid=1516             # RBMaster 进程 PID

[stats]
outage_count=2                # 中断次数（进程重启归零）
last_outage_sec=22            # 上次中断持续秒数
total_outage_sec=134          # 累计中断秒数
current_outage_sec=0          # 当前正在进行的中断秒数（若中断）
```

### 8.3 中断统计边沿检测逻辑

```
若 network_status 从 1 → 0：
    net_was_down = true
    ever_connected = true
    net_down_since = now
    outage_count++

若 network_status 从 0 → 1（且 net_was_down）：
    duration = now - net_down_since
    last_outage_sec = duration
    total_outage_sec += duration
    net_was_down = false
```

`ever_connected` 标志确保进程启动后尚未建立连接的初始阶段不被计为中断。

---

## 第9章：部署包分析

### 9.1 部署包目录结构

```
roamlink_deploy_by_license_state_{OTA,U}_260324/
├── install_update.sh       # 主安装脚本（OTA/U 版唯一差异在此）
├── dial_1.29.7             # 本仓库编译产物
├── check_network.sh        # 看门狗脚本 v2.0
├── network.ini             # 默认策略（network_select=1）
├── start_check_network     # init.d 脚本，管理 check_network.sh 守护进程
├── start_daemon            # init.d 脚本，管理 /opt/start_daemon.sh
├── check_fw_version        # 固件版本检测工具
├── get_imei                # IMEI 读取工具
├── test_network            # 网络测试工具
├── roamlink/               # RBMaster 及其配置文件树
│   └── etc/.pconfig/       # （出厂 U 版）预置 license.cer 放于此处
└── licenses/               # license 文件目录
    └── {IMEI}_license.cer  # （U 盘出厂）设备专属证书
```

### 9.2 check_network.sh v2.0（纯看门狗）

```sh
#!/bin/sh
# v2.0：纯进程看门狗，不含通道切换逻辑（切换由 dial 内部状态机负责）

POLL_INTERVAL=30          # 每 30s 检查一次
DIAL_BIN="/usr/dial/dial"
PID_FILE="/tmp/dial_watchdog.pid"

echo $$ > "$PID_FILE"     # 写入 watchdog 自身 PID

while true; do
    if ! pgrep -x "dial" > /dev/null; then
        # dial 进程不存在 → 重启
        start-stop-daemon -S -b -a "$DIAL_BIN"
    fi
    sleep $POLL_INTERVAL
done
```

与旧版（含 `celluar_switch` 切换逻辑）的根本区别：v2.0 完全不含切换决策，仅保证 dial 进程存活。

### 9.3 start_check_network（init.d 脚本）

```sh
#!/bin/sh
networkScript=/usrdata/check_network.sh

case "$1" in
start)   start-stop-daemon -S -b -a $networkScript ;;
stop)    start-stop-daemon -K -n $networkScript ;;
restart) $0 stop; $0 start ;;
esac
```

通过软链接 `rc5.d/S60start_check_network → /etc/init.d/start_check_network` 在 runlevel 5 自动启动。

### 9.4 start_daemon（init.d 脚本）

```sh
#!/bin/sh
startDaemonScript="/opt/start_daemon.sh"

case "$1" in
start)   start-stop-daemon -S -b -a $startDaemonScript ;;
stop)    start-stop-daemon -K -n $startDaemonScript ;;
restart) $0 stop; $0 start ;;
esac
```

`install_update.sh` 中会注释掉 `/opt/start_daemon.sh` 里的 `check_dial` 调用，防止与新版 `check_network.sh` 逻辑冲突。

### 9.5 network.ini 默认配置

```ini
[network]
network_select=1   # NET_POLICY_PREFER_ROAMLINK：优先使用 Roamlink
```

---

## 第10章：OTA 版与 U 版安装脚本差异

### 10.1 唯一差异

两个部署包（`roamlink_deploy_by_license_state_OTA_260324` 和 `roamlink_deploy_by_license_state_U_260324`）的 `install_update.sh` **只有一处不同**：

| 参数 | OTA 版 | U 盘出厂版 |
|------|--------|----------|
| `FORCE_INSTALL` | `0` | `1` |

其余所有逻辑、命令、文件列表完全相同。

### 10.2 `FORCE_INSTALL` 的行为差异

**OTA 版（FORCE_INSTALL=0）**：

```sh
if [ -d /usrdata/roamlink ]; then
    # 检测到已安装 → 升级模式
    # 只替换 dial 程序和配置文件，不清除 license 和 roamlink 组件
    cp -f ./dial_1.29.7 /usr/dial/dial
    cp ./check_network.sh /usrdata/
    cp ./network.ini /usrdata/
    reboot
    # （后续安装步骤全部跳过）
fi
```

**U 盘出厂版（FORCE_INSTALL=1）**：

```sh
if [ -d /usrdata/roamlink ]; then
    # 强制清除旧安装
    rm -rf /usrdata/roamlink
    rm -f /data/ufs/license.cer
    # （继续执行完整安装流程）
fi
```

### 10.3 完整安装流程（首次安装或 FORCE_INSTALL=1）

```
1. check_fw_version        → 检测固件版本（返回码 0=需 DFOTA，1=已是目标版本，2=不支持，255=失败）
2. get_imei                → 读取设备 IMEI
3. license 自适应预置：
   - licenses/{IMEI}_license.cer 存在 → 预置到主路径和备份路径（U 盘出厂模式）
   - 不存在 → 跳过（OTA 模式，dial 启动后自动下载）
4. 固件相关处理：
   - fw_version==0（需 DFOTA）：安装 start_dfota init.d 脚本，挂载 DFOTA 链接
   - fw_version==1（已是目标）：安装 start_check_network init.d 软链接
5. 安装 roamlink/ 目录（cp -af）
6. 安装 dial_1.29.7 → /usr/dial/dial
7. 安装 network.ini、test_network、check_network.sh
8. 安装 start_check_network init.d 脚本
9. 注释掉 /opt/start_daemon.sh 中的 check_dial 调用（sed -i）
10. 清理 /tmp/dial_watchdog.pid
11. sync → reboot
```

### 10.4 三种使用场景

| 场景 | FORCE_INSTALL | licenses/ 目录 | 首次重启后行为 |
|------|--------------|---------------|-------------|
| U 盘出厂（含证书） | 1 | 含 `{IMEI}_license.cer` | Roamlink 立即可用，按 network_select 策略运行 |
| OTA 升级（有旧安装） | 0 | 任意 | 仅更新 dial 和脚本，保留原 license，重启 |
| OTA 首次安装（无证书） | 0 | 空 | SIM 先上网，RBMaster 云端下载 license，下载后自动重启 |

---

## 第11章：已知缺陷与生产注意事项

### 11.1 缺陷清单

| # | 位置 | 问题描述 | 严重度 | 建议修复 |
|---|------|---------|--------|---------|
| 1 | `src/nw/nw.c` `nw_mark_plmn()` | `system("echo MCC MNC > /tmp/...")` — MCC/MNC 来自调制解调器，含特殊字符可造成 shell 注入 | 中等 | 改用 `open()/write()` 直接写文件 |
| 2 | `src/nw/nw.c` `nw_mark_network_status()` | `open()` 有 `O_CREAT` 但无 `O_TRUNC`，无 mode 位 — 可能保留旧内容，文件权限不确定 | 低 | 加 `O_TRUNC` 和 `0644` mode |
| 3 | `src/nw/nw.c` `nw_get_ifaddrs()` | 浅拷贝 `struct ifaddrs` 后调用 `freeifaddrs()` — `ifa_name` 成为悬空指针 | 中等 | 深拷贝 `ifa_name`（`strdup`），在使用后手动 `free` |
| 4 | `src/apn/apn.c` `apn_scan_from_json()` | `fread()` 错误路径未 `free(str)` 且未 `fclose(fp)` | 低 | 添加 cleanup goto 路径 |
| 5 | `src/apn/apn.c` `apn_scan_idx()` | `return apn_set(1, p_apn_obj)` 之后 100+ 行代码为死代码 | 信息 | 删除死代码（或保留注释说明意图） |
| 6 | `src/seas_log/seas_log.h` `SEAS_LOG_FATAL` | 宏体内引用 `SEAS_LEVEL_FATAL` 和 `SEAS_COLOR_FATAL`，两者均未定义 — 若被调用则编译错误 | 中等（隐患） | 删除 `SEAS_LOG_FATAL` 宏，或补全 `SEAS_LEVEL_FATAL=6`（现为 ALL） |
| 7 | `src/seas_log/seas_log.h` | `SEAS_LOG_CRITICAL` 宏不存在，但 `SEAS_LEVEL_CRITICAL=5` 和 `SEAS_COLOR_CRITICAL` 均已定义 — 不一致 | 低 | 补充 `SEAS_LOG_CRITICAL` 宏 |
| 8 | `src/seas_log/seas_log.h` | `#define ENABLE_SEAS_LOG` 硬编码紧接 `#ifdef`，编译时无法通过 `-DDISABLE_SEAS_LOG` 关闭 | 低 | 改为 `#ifndef DISABLE_SEAS_LOG` + `#define ENABLE_SEAS_LOG` 模式 |
| 9 | `src/seas_log/seas_log.h` | `SEAS_LOG_NOTICE` 和 `SEAS_LOG_WARNING` 宏定义但全仓库从未调用 | 信息 | 按需删除或实际使用 |
| 10 | `src/nw/nw.c` `nw_at_get_csq()` | `system("echo ... > /tmp/network_csq")` 同样存在 shell 注入风险，且 CSQ 为整数，风险实际较低 | 低 | 改用 `open()/write()` |

### 11.2 发布前必须修复项（⚠️）

**`LICENSE_WAIT_TIMEOUT_SEC` = 300 → 必须改为 7200**

```c
// src/roamlink/roamlink.h 第N行：
#define LICENSE_WAIT_TIMEOUT_SEC 300   // ⚠️ 测试值！必须改为 7200（2小时）
```

背景：在 commit `b1ca8d0`（`roamlink_migration_analysis.md` 记录）中为加速测试将此值从 7200 改为 300。设备实际激活流程中，网络注册、证书服务器下发可能需要超过 5 分钟，300s 超时会导致 OTA 首次激活失败。

### 11.3 设计级风险（⚠️）

**`NW_TCP_TEST_HOST = "18.196.0.17"` — 单点故障**

```c
// src/nw/nw.h：
#define NW_TCP_TEST_HOST "18.196.0.17"
#define NW_TCP_TEST_PORT 22
```

TCP 连通性测试（SIM 和 Roamlink 两个通道均使用同一目标）依赖单个 IP 地址的可达性。若该服务器不可访问（维护、封锁、IP 变更），两个通道均无法通过 TCP 测试，系统将在 SIM 和 Roamlink 之间无限制地切换（flip-flop），导致网络服务中断。

建议：引入至少两个备用测试目标（如 `8.8.8.8:53`、`1.1.1.1:53`），任一通过则判定为连通。

### 11.4 sim.c 历史缺陷（已修复）

`open_dial_roamlink_analysis.md` 记录了早期版本 `sim_op_handler()` 中 `sim_op_stat_get_iccid` case 缺少 `break` 的贯穿缺陷。当前版本（`feature/dual_channel_switch` 分支）每个 case 后均有 `break`，**此缺陷已修复**。

### 11.5 at.c 历史缺陷（已修复）

早期版本 `Ql_SendAT()` 中存在 `sizeof(atCmd)` 返回指针大小（4字节）而非缓冲区大小的问题。当前版本已修正缓冲区处理逻辑，**此缺陷已修复**。

---

## 第12章：配置速查表

### 12.1 所有时间常量

| 常量名 | 值 | 描述 | 文件 |
|--------|-----|------|------|
| `SIM_CHECK_TIMEOUT_SECONDS` | 3600s | SIM 卡检测超时（超时前 cfun 重置） | `dial.h` |
| `DIAL_TIMEOUT_SECONDS` | 60s | rx_packets 无增长超时（重拨 SIM） | `dial.h` |
| `REG_CHECK_TIMEOUT_SECONDS` | 300s | reg_check + cereg_check 合计超时 | `dial.h` |
| `TCP_TEST_INTERVAL_SECONDS` | 60s | SIM 通道 TCP 测试间隔 | `dial.h` |
| `TCP_FAIL_THRESHOLD` | 3 次 | 连续 TCP 失败触发 SIM 通道切换 | `dial.h` |
| `ROAMLINK_CONNECT_WAIT_SEC` | 300s | Roamlink 建立连接最长等待 | `dial.h` |
| `ROAMLINK_CHECK_INTERVAL_SEC` | 30s | Roamlink 通道 TCP 测试间隔 | `dial.h` |
| `ROAMLINK_FAIL_THRESHOLD` | 3 次 | 连续 TCP 失败触发 Roamlink 通道切换 | `dial.h` |
| `ROAMLINK_NO_DATA_TIMEOUT_SEC` | 120s | 业务层无数据超时（rx_packets 不增长） | `dial.h` |
| `ROAMLINK_BIZ_LOG_INTERVAL_SEC` | 3600s | "Roamlink biz OK" 日志降频间隔 | `dial.h` |
| `SIM_FALLBACK_RETRY_SEC` | 300s | 策略1：SIM 备用时尝试切回 Roamlink 的间隔 | `dial.h` |
| `ROAMLINK_FALLBACK_RETRY_SEC` | 300s | 策略2：Roamlink 备用时尝试切回 SIM 的间隔 | `dial.h` |
| `LICENSE_WAIT_TIMEOUT_SEC` | **300s ⚠️** | license 下载等待超时（**生产必须改为 7200**） | `roamlink.h` |
| `LICENSE_CHECK_INTERVAL_SEC` | 60s | license 文件轮询间隔 | `roamlink.h` |

### 12.2 所有运行时文件路径

| 路径 | 写入方 | 内容 |
|------|--------|------|
| `/tmp/dial_status` | `status.c` | 完整 INI 格式状态（原子 rename） |
| `/tmp/network_status` | `nw.c` | `1`=连接，`0`=断开 |
| `/tmp/network_type` | `roamlink.c` | `1`=SIM，`2`=Roamlink，`0`=切换中 |
| `/tmp/network_csq` | `nw.c` | CSQ 整数字符串（主线程每 5s，仅变化时写） |
| `/tmp/network_plmn` | `nw.c` | MCC+MNC 字符串 |
| `/tmp/dial_watchdog.pid` | `check_network.sh` | watchdog 自身 PID |
| `/usr/dial/tz.ini` | `tz.c` | `AT+QLTS=1` 获取的时区 |
| `/media/sdcard/seas_log_dial.log` | `seas_log` | 轮转日志（50 MB 上限） |
| `/usrdata/check_network.log` | `check_network.sh` | watchdog 日志（1 MB，自动轮转） |
| `/usrdata/network.ini` | 用户/部署 | `network_select=1~4` |
| `/usr/dial/apn.json` | 用户/部署 | APN 配置 JSON |
| `/opt/conf.ini` | factoryApp | RBMaster 运行配置 |
| `/usrdata/roamlink/etc/.pconfig/license.cer` | 部署/RBMaster | Roamlink 主路径证书 |
| `/data/ufs/license.cer` | `roamlink.c` | Roamlink 备份证书 |

### 12.3 状态枚举与字符串映射

| 枚举值 | 字符串（`dial_status` 中的 `state=` 值） |
|--------|---------------------------------------|
| `dial_stat_none` | `"none"` |
| `dial_stat_init` | `"init"` |
| `dial_stat_sim_init` | `"sim_init"` |
| `dial_stat_sim_check` | `"sim_check"` |
| `dial_stat_sim_op` | `"sim_op"` |
| `dial_stat_reg_check` | `"reg_check"` |
| `dial_stat_cereg_check` | `"cereg_check"` |
| `dial_stat_precondition_check` | `"precondition_check"` |
| `dial_stat_pre_start_call` | `"pre_start_call"` |
| `dial_stat_start_call` | `"start_call"` |
| `dial_stat_wait_for_connect` | `"wait_for_connect"` |
| `dial_stat_net_connected` | `"net_connected"` |
| `dial_stat_list_oper` | `"list_oper"` |
| `dial_stat_select_oper` | `"select_oper"` |
| `dial_stat_stop_cfun` | `"stop_cfun"` |
| `dial_stat_start_cfun` | `"start_cfun"` |
| `dial_stat_roamlink_starting` | `"roamlink_starting"` |
| `dial_stat_roamlink_active` | `"roamlink_active"` |

### 12.4 网络策略速查

| `network_select` | 常量 | 初始通道 | 故障切换 | 回退尝试 |
|-----------------|------|---------|---------|---------|
| `1` | `NET_POLICY_PREFER_ROAMLINK` | Roamlink | → SIM | 每 300s 尝试回 Roamlink |
| `2` | `NET_POLICY_PREFER_SIM` | SIM | → Roamlink | 每 300s 尝试回 SIM |
| `3` | `NET_POLICY_FORCE_ROAMLINK` | Roamlink | 重启 Roamlink | 无 |
| `4` | `NET_POLICY_FORCE_SIM`（默认） | SIM | 重拨 SIM | 无 |

### 12.5 日志级别与颜色

| 宏 | 级别值 | 颜色 ANSI | 状态 |
|----|--------|-----------|------|
| `SEAS_LOG_DEBUG` | 0 | `\x1B[34;1m`（蓝色粗体） | 正常可用 |
| `SEAS_LOG_INFO` | 1 | `\x1B[36m`（青色） | 正常可用，使用最频繁 |
| `SEAS_LOG_NOTICE` | 2 | `\x1B[32;1m`（绿色粗体） | 已定义，全仓库未使用 |
| `SEAS_LOG_WARNING` | 3 | `\x1B[33m`（黄色） | 已定义，全仓库未使用 |
| `SEAS_LOG_ERROR` | 4 | `\x1B[31m`（红色） | 正常可用，使用频繁 |
| `SEAS_LOG_CRITICAL` | 5 | `\x1B[41;1m`（红背景粗体） | `SEAS_LEVEL_CRITICAL`/`SEAS_COLOR_CRITICAL` 已定义，但 `SEAS_LOG_CRITICAL` 宏**不存在** |
| `SEAS_LOG_FATAL` | - | - | 宏存在，但 `SEAS_LEVEL_FATAL`/`SEAS_COLOR_FATAL` **未定义**，调用即编译报错 |
| `SEAS_LOG_ALL` | 6 | `\x1B[35;1m`（紫色粗体） | 已定义，用于调试全量输出 |

`SEAS_LOG_ENABLED(level)` 宏：`seas_log_get_level() <= (SEAS_LEVEL_##level)`，可用于在调用前检查级别。

### 12.6 Roamlink 探测结果速查

| 结果 | 含义 | `roamlink_available` | `license_pending` |
|------|------|---------------------|-------------------|
| `ROAMLINK_PROBE_OK` | 全部就绪 | `true` | `false` |
| `ROAMLINK_PROBE_NO_PACKAGE` | RBMaster 缺失 | `false` | `false` |
| `ROAMLINK_PROBE_CONF_MISSING` | conf.ini 缺失 | `false` | `false` |
| `ROAMLINK_PROBE_LICENSE_MISSING` | license 缺失 | `false` | `true` |

---

*文档完*
