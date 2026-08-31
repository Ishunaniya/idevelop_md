# open_dial 拨号流程说明

> 版本：对应 **V1.28.9**
> 目标模组：**Quectel EC200A（ASR1803，OpenCPU / aarch64）**，SDK = `libql_sdk.so`，联网栈 = ECM（ecm0 / ccinet）
> 用途：给移远（Quectel）技术对接用。所有描述均标注**源码文件:行号**，可逐条核对。

---

## 0. 一句话结论

本程序**稳态只维持 1 路数据拨号**：
- **单进程**（main.c 单实例护栏）
- **单 data call**：`call_id = 4`，`call_name = "auto_network"`（dial.c:273/298）
- **单 APN profile**：`DATA_CALL_APN_PUBLIC = 6`（dial.c:293）
- **纯 IPv4**：`QL_NET_IP_VER_V4`（dial.c:296）

日志中反复出现的 `dial connecting → connected` **不是并发多路**，而是**同一路**在分级恢复（L1/L2）里 stop→start 重拨。重拨后 SDK 的 `call_id` 会变，但 `call_name` 稳定，事件按 `call_name` 过滤（data_call.c:41 附近）。

---

## 1. 总体分层

| 阶段 | 入口 | 职责 |
|---|---|---|
| 启动准备 | `main.c : main()` | 等网卡、单实例、CFUN 限频、已连快捷判断 |
| 拨号初始化 | `dial.c : dial_loop()`（前半） | init 各 SDK 域 → 建 call → 配置 → start |
| 运行期编排 | `dial.c : dial_loop()`（while(1)） | 状态机 + 心跳 + 分级恢复，**永不正常退出** |
| PDP 事件 | `data_call.c : data_call_status_ind_cb()` | 连接建立/断开时配路由、DNS、iptables |

`main()` 在准备就绪后调用 `dial_loop(on_network_connected, NULL)`（main.c:406），此后不再返回。

---

## 2. 启动准备（main.c）

顺序如下：

1. **等待网卡 ecm0**：`wait_for_interface("ecm0", 30)`，最多等 30s，超时 `return -1` 退出（main.c:278）。
   > ecm0 未加载说明 SDK 服务/USB 网卡没起来,拨号没有意义。
2. **读配置 + 读 SIM 状态**：`read_config()` / `check_sim_status()`（main.c:291-296）。
3. **单实例护栏**：`check_pid_running()` → `check_process("dial") >= 2` 为真则 `exit(EXIT_FAILURE)`（main.c:262/300）。
   > 计数底层用 `pidof | wc -w`（v1.28.9 修复 `pgrep -x` 失效坑，见 CLAUDE.md）。
4. **SIGCHLD 回收**：`sigchld_handler` + `WNOHANG` 循环回收子进程（main.c:184/306），防僵尸（诊断 logcat 等子进程）。
5. **异常退出计数 → 条件 CFUN 重启**：累计异常退出达 `MAX_EXIT_COUNT_BEFORE_CFUN_RESTART = 20` 时 `restart_cfun()` 开关一次射频，且受 `MAX_CALLS=10` 次 / `MIN_INTERVAL=600s` 双重限频（main.c:32/94-117/316-324）。
6. **写版本文件**：`/tmp/dial_version`（main.c:328）。
7. **诊断子系统初始化**：`setup_cp_dump_capture()` / `diag_live_capture_init()`（认领上个实例遗留的实时 logcat）/ `diag_report_modem_log_media()`（main.c:343-345）。
8. **"已连快捷路径"**：若此刻 `test_can_ping_google()` 已通（如 L3 exit 重拉后 SDK 已自连），进入轻量 keep-alive 循环，每 6s ping 一次；连续失败 >10 次才 break 去重拨（main.c:378-398）。
9. 最终 `dial_loop(...)`（main.c:406）。

---

## 3. 拨号初始化（dial.c，dial_loop 前半）

固定参数（dial.c:273-298）：

```c
int         g_call_id          = 4;                    // 单一 data call id
const int   apn_id             = DATA_CALL_APN_PUBLIC; // = 6，公网 profile
const int   ip_ver             = QL_NET_IP_VER_V4;     // = 0x1，仅 IPv4
const int   reconnect_interval = 25;                   // 交给底层自动重连的间隔(秒)
const char *call_name          = "auto_network";
```

初始化顺序：

| 步骤 | 调用 | 位置 | 说明 |
|---|---|---|---|
| 1 | `ql_data_call_init()` | dial.c:305-317 | 最多重试 200 次×100ms，容忍 `-1001`（SDK 未就绪） |
| 2 | 注册回调 | dial.c:324-325 | `set_status_ind_cb` / `set_service_error_cb` |
| 3 | `ql_nw_init()` | dial.c:330 | **必须先调**，否则心跳里 SRV/RAT/信号/OPER 全 N/A（CLAUDE.md 坑） |
| 4 | `sim_init()` + 读 ICCID | dial.c:337-345 | 失败退避 2s |
| 5 | 注册 SIM 回调 + 主动查初始态 | dial.c:348-361 | 用回调替代高频 `AT+CPIN?` 轮询 |
| 6 | `set_apn(iccid)` | dial.c:365 | 见下方 APN 匹配 |
| 7 | `ql_data_call_create(g_call_id, "auto_network", 0)` | dial.c:375 | **全仓库唯一一处 create** |
| 8 | 配置 param | dial.c:377-398 | 绑 `apn_id`、`ip_ver`、reconnect 模式 NORMAL、interval=25s |
| 9 | `ql_data_call_start(g_call_id)` | dial.c:401 | **首次拨号** |

### 3.1 APN 匹配规则（apn/apn.c）

- 读 `/usr/dial/apn.json`，按 **ICCID `strcmp` 精确匹配**（apn.c:307，不是前缀匹配）。
- 命中：把该 APN 写入 profile **`DATA_CALL_APN_PUBLIC = 6`**（apn.c:338），IPv4。
- 未命中：`fall back to auto/default APN`（apn.c:328）——向同一 profile 6 写入空 APN，由模组按运营商自动选择 APN（apn.c:348-370）。
- **关键约束**：`set_apn()` 写入的 profile 必须与 dial.c 里 `param_set_apn_id` 用的 id 一致（都必须是 6）。曾误写成 1 导致 apn.json 全程不生效（dial.c:287-292 注释、CLAUDE.md 坑）。

> 本地这张卡（ICCID 8986…4051）未在 apn.json 里,走的是自动 APN。

### 3.2 apn.json 格式与边界

`/usr/dial/apn.json` 的根节点是 `apn` 数组，每项支持以下字段（apn.c:226-280）：

```json
{
  "apn": [
    {
      "iccid": "<完整 ICCID>",
      "apn": "<APN 名称>",
      "usrname": "<用户名，可留空>",
      "pwd": "<密码，可留空>"
    }
  ]
}
```

- 字段名是 `usrname` 和 `pwd`，不是 `username` 和 `password`。
- ICCID 必须填完整值；代码用 `strcmp` 比较，不支持前缀、号段、通配符或多条合并匹配。
- 匹配项的 APN、用户名和密码会写入 profile 6；`auth_pref` 没有显式设置，保持结构体清零后的默认值 0。
- `ql_data_call_set_apn_config()` 失败时 `set_apn()` 返回 `-1`，但 `dial_loop()` 当前未检查该返回值，仍会继续 create/config/start。
- 命中时日志会输出 APN、用户名、密码和 ICCID（`[APN] matched`），向外提供日志前应注意脱敏。

---

## 4. PDP 事件回调（data_call.c : data_call_status_ind_cb）

SDK 在其内部线程回调。**按 `call_name` 过滤**（不是 call_id，因为重拨后 id 变）：`strcmp(p_msg->call_name, DIAL_CALL_NAME)` 不等就 return（data_call.c:41 附近）。

- **CONNECTED**（data_call.c）：
  - 置 `g_pdp_connected = 1`，写 `/tmp/network_status`、`/tmp/dial_Status`；
  - 记住网卡名到 `g_if_name`；
  - **防御性清理**旧默认路由 + 循环删多余 MASQUERADE 规则（应对崩溃残留）；
  - 有 IPv4 地址时：`ip route add default dev <if>`、`iptables -t nat -A POSTROUTING -o <if> -j MASQUERADE`、写 `/tmp/resolv_v4.conf` 合成 `/etc/resolv.conf`。
    > 用 dev-only 路由（不写 via GW）：ccinet 是 NOARP/UNSPEC 接口，via GW 在回调时序内会静默失败。
    > DNS 写入用纯 `\n`（含 `\r` 会污染 resolv.conf，CLAUDE.md 坑）。
  - IPv6 分支存在但当前 `ip_ver=V4`，正常不会走。
- **CONNECTED → IDLE**（如 `AT+CFUN=0` 关射频）：SDK 发 IDLE 而非 DISCONNECTED，同样把 `g_pdp_connected = 0` 并清状态,否则该标志永不归零（data_call.c 末段）。

`on_network_connected()`（dial.c:121）：连接**首次**确认后由主循环回调一次，更新 `/tmp/network_status` 为已连。

### 4.1 路由、NAT 与 DNS 的详细边界

| 项目 | 当前行为 | 不包含的能力 / 注意事项 |
|---|---|---|
| IPv4 默认路由 | 连接前删除当前拨号网卡的旧默认路由，再执行 `ip route add default dev <if>` | 不删除其他网卡的默认路由；不设 `via`、`metric`、独立路由表或 `ip rule` |
| IPv6 默认路由 | 代码有 `ip -6 route add default dev <if>` 分支 | 当前 data call 固定为 IPv4，正常不进入该分支 |
| NAT | 添加 `POSTROUTING -o <if> -j MASQUERADE`；连接前和断线时最多循环删除 10 条同样规则 | 只管理这一条 MASQUERADE；不设置 `net.ipv4.ip_forward`，不配置 FORWARD 链放行规则 |
| DNS | 将 PDP 下发的 DNS 写入 `/tmp/resolv_v4.conf`，清空后重建 `/etc/resolv.conf` | 这是覆盖系统 DNS，不保留原来内容；断线时只删临时文件，不会清空或恢复 `/etc/resolv.conf` |
| 静态/策略路由 | 无 | 不配置业务网段静态路由、多 WAN 选路、源地址策略路由或路由优先级 |

连接建立前的清理和 IDLE/DISCONNECTED 时的清理都按拨号网卡名精确执行，不会主动删除其他网卡的路由或 NAT 规则。但也因此，如果系统已有冲突的默认路由，`ip route add default` 可能失败。路由、iptables 和 DNS 合并命令均通过 `system()` 执行，当前未统一检查返回码，也没有重试或回滚；现场排查时应同时核对 `ip route`、`iptables -t nat -S` 和 `/etc/resolv.conf` 的实际状态。

---

## 5. 运行期状态机（dial_loop while(1)）

每轮 `usleep(50ms)`（dial.c:942）。轮内先做 Fast-Fail 检测与心跳，再走业务状态机：

```
ST_STATUS → ST_SIM → ST_SIGNAL → ST_PING → (正常?回到 ST_STATUS : 判级→ST_RECOVERY)
```

| 状态 | 周期 | 动作 | 位置 |
|---|---|---|---|
| ST_SIM | 800ms | 读 SIM（优先回调标志，降级 `AT+CPIN?`）；运行期掉卡 → 记 `start_fail_ts`、清抖动计数 | dial.c:609-659 |
| ST_SIGNAL | 800ms | 占位（信号由心跳采集） | dial.c:661-665 |
| ST_PING | 1500ms | `test_can_ping_google()` 判连通 | dial.c:667 |

**心跳**（与状态机并行）：
- 30s 基本心跳：SIM/REG/SRV/CSQ/RSRP.../TEMP/DownTime（dial.c:523-557）；
- 5min 扩展心跳：OPER + CID + IP（dial.c:565-599）。

**抖动过滤**：ping 连续失败需达 `PING_FAIL_THRESHOLD` 次才启动故障计时（dial.c:721-726），避免单次丢包误判。

**Ping 成功分支**（dial.c:671-716）：清 `start_fail_ts`、停故障期实时 logcat、抓 recovery 快照、置 `has_connected_once=1`（**永不复位**）、必要时回调 on_connected。

---

## 6. 分级故障恢复（核心）

断网时长阈值（dial.c:421-423）：

```
0 ─── 30s ─────────── 5.5min ───────────── 30.5min
     L0 结束          L2 射频重置            L3 进程重启
     L1 软重拨
```

| 级别 | 触发 | 动作 | 冷却 | 位置 |
|---|---|---|---|---|
| **L0** | 0~30s | 不干预，等 SDK 自动重连（interval 25s） | — | — |
| **L1** | >30s 且 REG≠0 | `ql_data_call_stop` → sleep(2) → `ql_data_call_start`（同 `g_call_id`） | 60s | dial.c:854-878 |
| **L2** | >5.5min，**或** REG=0 且 >90s | `serial_atcmd at+cfun=0` → sleep(3) → `at+cfun=1` → sleep(10) → `data_call_start` | 正常 5min / REG=0 90s | dial.c:880-906 |
| **L3** | >30.5min | `sync()` → `log_close()` → **`exit(1)`**，由父进程 start_prog 重拉、重走完整 init | — | dial.c:908-930 |

要点：
- **L3 是纯 exit,不是整机重启**（v1.28.0 起）。EC200A 是 OpenCPU，`AT+CFUN=1,1` = 整颗 SoC 复位会中断同机 wifi/换电等业务,故改为仅重启 AP 进程（dial.c:909-916 注释）。
  > 代价：exit 清不掉 CP 固件 hang。若现网出现超 L3 阈值仍不恢复的死循环，需另加更高一级（整机重启+空闲门控）。
- **REG=0 快速线**：注册丢失（`CEREG_STAT_NOT_REGISTERED`）时跳过 L1 直奔 L2，>90s（`L2_REG0_FAST_TIMEOUT_MS`）即触发。因为实测 REG=0 期间 `ql_data_call_start` 持续返回 `-1001`，L1 无效，只有 cfun 飞行模式切换能强制 CP 重新扫频（dial.c:791-804）。
- **`has_connected_once` 门控**：L1/L2/L3 触发条件为 `enable_policy_recovery==1 && has_connected_once`（dial.c:783）。**从未连上过**（停机/无数据权限/REG:3 拒绝）时关闭破坏性恢复，安静停在 L0 等 SDK 自连,只打一行 `never-connected ... gated` 自证日志（dial.c:820-827）。冷启动卡死另由 Fast-Fail 兜底。

---

## 7. 启动快速失败（Fast-Fail）

独立于分级恢复，仅在进程**前 `MAX_FAST_RETRY_TIMES=3` 次启动内**生效（dial.c:276-281、on_network_connected 附近宏）：

- **PDP 未建立**：允许最长 `PDP_WAIT_TIMEOUT_MS=60s`，超时 `exit(1)` 让父进程快速重拉（dial.c:511-516）；
- **PDP 已建立**：从 `pdp_ready_ts` 起给 `FAST_FAIL_TIMEOUT_MS=10s` 的 Ping 窗口，窗口内没 ping 通即 `exit(1)`（dial.c:496-507）;
- Ping 一旦成功 → `clear_retry_count()`,退出 Fast-Fail 模式（dial.c:702-707）。

> 目的：冷启动早期卡死时用"快速整进程重来"兜底，比慢慢等分级恢复快。

---

## 8. 对外状态文件与产物

| 路径 | 含义 |
|---|---|
| `/tmp/network_status` | 1=已连接 / 0=断开 |
| `/tmp/dial_Status` | 拨号态标志 |
| `/tmp/dial_version` | `Version: 1.28.9` |
| `/etc/resolv.conf` | 由 PDP 回调按下发 DNS 合成 |
| `/media/sdcard/dial_log/*/dial_*.log` | 按天分文件夹日志 |
| `/media/sdcard/dial_log/dial_snap/` | 故障/恢复 dmesg+logcat 快照 |
| `/media/sdcard/dial_log/dial_snap/live/` | 故障期实时 logcat（跟随模式，恢复时停） |

---

## 9. 全流程图

```mermaid
flowchart TD
    A[main: 等 ecm0 30s] -->|超时| Z[退出 -1]
    A -->|就绪| B[单实例检查 / 读配置 / CFUN 限频]
    B --> C{已能 ping 通?}
    C -->|是| K[keep-alive 轻量监控\n连续失败>10 退出监控]
    C -->|否| D[dial_loop 初始化]
    K --> D
    D --> D1[data_call_init / nw_init / sim_init]
    D1 --> D2[set_apn: ICCID 精确匹配→profile 6 / 否则自动APN]
    D2 --> D3[create call_id=4 / 配置 IPv4+reconnect / start]
    D3 --> E{{while(1) 状态机}}
    E --> F[ST_SIM 800ms]
    F --> G[ST_SIGNAL 800ms]
    G --> H[ST_PING 1500ms]
    H -->|通| I[清故障计时\nhas_connected_once=1\n必要时回调]
    I --> E
    H -->|连续失败达阈值| J[按断网时长判级]
    J -->|<30s| L0[L0 不干预]
    J -->|>30s REG≠0| L1[L1 stop→start 同一路]
    J -->|>5.5min 或 REG=0>90s| L2[L2 cfun=0/1→start]
    J -->|>30.5min| L3[L3 exit 由父进程重拉]
    L0 --> E
    L1 --> E
    L2 --> E
    L3 --> A
```

---

## 10. 给移远的问答速记

- **起几路拨号？** 1 路（call_id=4 / profile 6 / IPv4 单栈）。日志里反复 connecting/connected 是同一路重拨。
- **为什么 call_id 会变？** L1/L2 恢复调 `data_call_start` 后 SDK 重分配 id；程序按稳定的 `call_name="auto_network"` 过滤事件，不受影响。
- **REG=0 为什么直接拉 cfun？** 实测 REG=0 时 `data_call_start` 恒 `-1001`，仅 `AT+CFUN=0/1` 能让 CP 重扫频注册。
- **L3 为什么不用 CFUN=1,1？** OpenCPU 下等于整 SoC 复位，会拖挂同机业务；改纯 exit 重拉 AP 进程。
- **从未连上的卡为什么不恢复？** `has_connected_once` 门控关闭，判为停机/无权限,不做无谓的射频/进程重启,只等 SDK 自连。

---

# 附录 A：移远 SDK 接口调用与配置详解

> 头文件路径：`ql-sysroots/usr/include/ql-sdk/`（`ql_data_call.h` / `ql_nw.h` / `ql_sim.h` / `ql_net_common.h`）。
> 以下签名、结构体、枚举均摘自本 SDK 头文件,不是通用文档。

## A.1 拨号链上按调用顺序的 SDK 接口

| # | 接口原型 | 本程序传参 | 说明 |
|---|---|---|---|
| 1 | `int ql_data_call_init(void)` | — | 容忍返回 `-1001`(SDK 未就绪),重试 200×100ms |
| 2 | `int ql_data_call_set_status_ind_cb(cb)` | `data_call_status_ind_cb` | 注册 PDP 状态变化回调 |
| 3 | `int ql_data_call_set_service_error_cb(cb)` | `data_call_service_error_cb` | CP 服务崩溃回调 |
| 4 | `int ql_nw_init(void)` | — | **必须在其它 ql_nw_* 前调**,否则信号/注网字段全 N/A |
| 5 | `int ql_sim_init(void)` | — | SIM 域初始化 |
| 6 | `int ql_sim_get_iccid(slot, buf, len)` | `QL_SIM_SLOT_1` | 取 ICCID,用于 APN 匹配 |
| 7 | `int ql_sim_set_card_status_cb(cb)` | `sim_card_status_cb` | SIM 状态回调,替代 `AT+CPIN?` 轮询 |
| 8 | `int ql_sim_get_card_info(slot, &info)` | `QL_SIM_SLOT_1` | 主动读初始 SIM app_state |
| 9 | `int ql_data_call_set_apn_config(apn_id, &cfg)` | `apn_id=6` | 命中 apn.json 时写公网 profile(见 A.3) |
| 10 | `int ql_data_call_create(call_id, name, is_bg)` | `4, "auto_network", 0` | **唯一一处 create**,单路 |
| 11 | `ql_data_call_param_t *ql_data_call_param_alloc()` | — | 分配配置句柄 |
| 12 | `ql_data_call_param_set_apn_id(p, id)` | `6` | 绑定 profile,必须与第 9 步同 id |
| 13 | `ql_data_call_param_set_ip_version(p, ver)` | `QL_NET_IP_VER_V4`(0x1) | **仅 IPv4** |
| 14 | `ql_data_call_param_set_reconnect_mode(p, m)` | `RECONNECT_NORMAL`(0x1) | 底层自动重连开 |
| 15 | `ql_data_call_param_set_reconnect_interval(p, list, n)` | `{25,0}, 1` | 重连间隔 25s |
| 16 | `int ql_data_call_config(call_id, p)` | `4, p_cfg` | 配置落到 call 实例 |
| 17 | `ql_data_call_param_free(p)` | — | 释放句柄 |
| 18 | `int ql_data_call_start(call_id)` | `4` | **首次拨号**;L1/L2 恢复复用此接口 |
| — | `int ql_data_call_stop(call_id)` | `4` | 仅 L1 软重拨时调 |

## A.2 本程序采用的配置值一览（给移远核对）

| 配置项 | 取值 | 枚举/常量 | 源码 |
|---|---|---|---|
| data call 数量 | 1 | — | dial.c:375 唯一 create |
| call_id | 4 | 硬编码 | dial.c:273 |
| call_name | "auto_network" | 稳定标识,用于事件过滤 | dial.c:298 |
| APN profile id | 6 | `DATA_CALL_APN_PUBLIC` | dial.c:293 |
| IP 版本 | 0x1 | `QL_NET_IP_VER_V4`(仅 v4) | dial.c:296 |
| 重连模式 | 0x1 | `QL_NET_DATA_CALL_RECONNECT_NORMAL` | dial.c:383 |
| 重连间隔 | 25s | interval_list[0] | dial.c:387-388 |
| 鉴权 | 未设(默认 0) | `auth_pref` 留 0 | apn.c:317 附近 |
| is_background | 0 | 前台 call | dial.c:375 |

## A.3 APN 配置结构体 `ql_data_call_apn_config_t`

```c
typedef struct {
    QL_NET_IP_VER_E    ip_ver;                          // 本程序填 V4(0x1)
    QL_NET_AUTH_PREF_E auth_pref;                        // 本程序留 0(未设)
    char apn_name[QL_NET_MAX_APN_NAME_LEN];             // 来自 apn.json 命中项
    char username[QL_NET_MAX_APN_USERNAME_LEN];         // 来自 apn.json
    char password[QL_NET_MAX_APN_PASSWORD_LEN];         // 来自 apn.json
} ql_data_call_apn_config_t;
```

- 通过 `ql_data_call_set_apn_config(6, &cfg)` 写入 **profile 6**（apn.c:338）。
- ICCID `strcmp` 精确命中时写入配置文件中的 APN；未命中时向 profile 6 写入空 APN，由模组自动选择（apn.c:311/348-370）。

## A.4 关键枚举（摘自 `ql_net_common.h` / `ql_sim.h`）

**IP 版本** `QL_NET_IP_VER_E`：
```
MIN = 0   V4 = 0x1   V6 = 0x2   V4V6 = 0x3
```
> ⚠️ `MIN=0`,所以 ip_ver 写 0 = MIN 不是 V4。本程序显式用 V4=0x1（曾误写 0,dial.c:294 注释）。

**重连模式** `QL_NET_DATA_CALL_RECONNECT_MODE_E`：
```
DISABLE = 0x0
NORMAL  = 0x1   // 间隔 = reconnect_interval[0] 秒  ← 本程序用这个
MODE_1  = 0x2   // 间隔列表循环
MODE_2  = 0x3
```

**鉴权** `QL_NET_AUTH_PREF_E`：`0` 不允许 PAP/CHAP · `1` PAP · `2` CHAP · `3` 两者皆可。

**data call 状态** `QL_NET_DATA_CALL_STATUS_E`（回调里的 call_status）：
```
NONE=0  CREATED=1  IDLE=2  CONNECTING=3
PARTIAL_V4=4  PARTIAL_V6=5  CONNECTED=6  DISCONNECTED=7  ERROR=8  DELETED=9
```
> 本程序在回调里只关心 **CONNECTED(6)**（配路由/DNS）与 **CONNECTED→IDLE(2)**（cfun 关射频时的降级,清 g_pdp_connected）。日志 `none→created→idle→connecting→connected` 就是这套枚举的字符串。

**SIM app_state** `QL_SIM_APP_STATE_E`：`UNKNOWN=0xB00 … READY=0xB0A`（中间有 DETECTED / PIN1_REQ / INITALIZATING 等）。本程序以 `READY(0xB0A)` 为就绪判据。
> 日志里 `SIM_AT:UNKNOWN` 即对应 `0xB00`——SIM/CP 通道失联。

## A.5 PDP 状态回调 `ql_data_call_status_t`（回调入参）

```c
typedef struct {
    int  call_id;
    char call_name[QL_NET_MAX_NAME_LEN];   // ← 用它过滤事件(重拨后 call_id 变)
    QL_NET_IP_VER_E ip_ver;
    QL_NET_DATA_CALL_STATUS_E call_status; // CONNECTED=6 时配网
    char device[QL_NET_MAX_NAME_LEN];      // 网卡名 → g_if_name
    uint8_t has_addr;   ql_net_addr_t  addr;   // addr/gateway/netmask/dnsp/dnss
    uint8_t has_addr6;  ql_net_addr6_t addr6;
    int call_end_reason_type;              // 断开原因类型
    int call_end_reason_code;              // 断开原因码
} ql_data_call_status_t;
```

回调内动作（data_call.c，见正文第 4 节）：置 `g_pdp_connected`、记 `device`、清残留路由、`ip route add default dev <if>`、`iptables MASQUERADE`、写 DNS 合成 `/etc/resolv.conf`。

## A.6 心跳采集用到的 nw / sim 接口（只读，不影响拨号）

| 接口 | 用途 | 本固件注意 |
|---|---|---|
| `ql_nw_get_signal_strength(&info, &level)` | RSRP/RSRQ/RSSI/SNR | **SNR 头文件标 0.1dB,本固件未按此填,实测恒近 0,不可用**;判质用 RSRP/RSRQ |
| `ql_nw_get_data_reg_status(&info)` | SRV/RAT/DENY(reg_state/radio_tech/denied_reason) | 正常 |
| `ql_nw_get_mobile_operator_name(&info)` | 运营商 OPER | 正常 |
| `ql_nw_get_cell_info(...)` | 小区 PCI/EARFCN | **本固件(1.057.030)取不到,全制式 valid=0,勿用**;小区标识改用 `AT+CREG` 的 CID |

## A.7 观测到的 SDK 返回码含义

| 返回码 | 出现场景 | 处理 |
|---|---|---|
| `QL_ERR_OK`(0) | 成功 | — |
| `-1001` | `data_call_init` SDK 未就绪;**REG=0 时 `data_call_start` 持续返回此码** | init 重试;REG=0 走 L2 cfun 而非 L1 |
| `-1013` | L2 后 `data_call_start` 撞上 SDK 已自动重连 | 视为 already connected,正常 |
| `QL_ERR_ABORTED` | CP 侧数据服务崩溃(service_error_cb) | 置 `g_sdk_service_error`,日志后由 keepalive 重拉 |

---

# 附录 B：与移远沟通的常见追问

- **你们用的是 `ql_data_call_*` 高层接口还是 AT？** 主链路全用 `ql_data_call_*`（libql_sdk.so）;仅 L2 射频重置用 `serial_atcmd at+cfun=0/1`,诊断另用 AT（CEER/CGACT/CREG 等）。
- **reconnect 交给 SDK 了,为什么还自己做恢复？** SDK 的 `RECONNECT_NORMAL` 只覆盖 L0(25s 级自连);L1/L2/L3 是应对 SDK 自连失效(REG=0 恒 -1001、CP 半死)的上层兜底。
- **APN profile 为什么是 6?** `DATA_CALL_APN_PUBLIC=6` 公网 profile;`set_apn_config` 与 `param_set_apn_id` 必须同 id(对照 SDK sample `sample_multi_data_call.c`)。
- **为什么只拨 IPv4?** 业务只需 v4;`ip_ver=V4(0x1)`,不建 v4v6 双栈,避免多一条 bearer。
- **SNR 为什么恒 0?** 本固件 `ql_nw_get_signal_strength` 的 `lte.snr` 未按头文件 0.1dB 口径填充,实测无效,已弃用,判质以 RSRP/RSRQ 为准（见 A.6）。
