# 07 — data_call/data_call.c + data_call/data_call.h

## 一、文件职责概述

数据连接（PDP）核心回调层。核心是 `data_call_status_ind_cb`——SDK 在 PDP 状态变化时回调，负责：维护 `g_pdp_connected`/`g_if_name`、写 `/tmp/network_status`、配置默认路由/NAT/DNS、断线清理。**这是 dial 真正的"上网生效"落地点**。

## 二、全局变量、宏
| 名称 | 定义 | 含义 |
|---|---|---|
| `g_callid` | `= DATA_CALL_ID_PUBLIC(1)`（`data_call.c:5`） | 全局 call id，misc.c readCallIdFromFile 用；**注意 dial_loop 实际用局部硬编码 4，二者不同** |
| `g_if_name[32]` | `{0}`（`data_call.c:6`） | 当前 PDP 接口名，回调写入 |
| `g_pdp_connected` | `volatile int 0`（`data_call.c:7`） | PDP 建立标志，dial_loop Fast-Fail 计时用 |
| `dial_status[]` | 10 个字符串（`data_call.c:8-9`） | call_status 码→字符串 |
| `DIAL_CALL_NAME` | `"auto_network"`（data_call.h:8） | 回调事件过滤依据（与 dial.c create 传入一致） |

## 三、逐函数分析

### `const char* get_dial_status_msg(int errcode)`（`data_call.c:11-28`）
- 0-9 返回 `dial_status[errcode]`，其余 "Unknown status"。

### `void data_call_status_ind_cb(int call_id, pre_call_status, ql_data_call_status_t *p_msg)`（`data_call.c:31-241`）
**SDK 内部线程回调**。事件过滤：`p_msg==NULL || strcmp(p_msg->call_name, DIAL_CALL_NAME)!=0` 直接 return（`data_call.c:42-44`）——**用 call_name 过滤而非 call_id**（注释说明重拨后 call_id 会变）。打 `[EVENT] DataCall` 状态迁移日志。

分三个状态分支：

**(1) CONNECTED**（`data_call.c:50-179`）：
- `g_pdp_connected=1`；`echo 0 > /tmp/dial_Status`；`update_network_status(1)`；`strncpy g_if_name = p_msg->device`（`data_call.c:52-60`）。
- **防御性预清理**（`data_call.c:64-72`）：删旧 IPv4/IPv6 默认路由（指定 dev）、循环删 MASQUERADE 规则（最多 10 次，防累积）。
- **IPv4（has_addr）**（`data_call.c:74-128`）：打 IPv4 日志；`ip route add default dev <device>`（**dev-only 路由**，注释说明 ccinet0 是 NOARP 接口，via GW 会时序失败）；`iptables -t nat -A POSTROUTING -o <dev> -j MASQUERADE`；写 `/tmp/resolv_v4.conf`（`nameserver %s\n`，**纯 LF**，符合 CLAUDE.md DNS 约定）；清空 `/etc/resolv.conf` 后 cat v4+v6 conf 合入。
- **IPv6（has_addr6）**（`data_call.c:130-178`）：类似，`ip -6 route add default dev`，写 `/tmp/resolv_v6.conf`，重新合成 resolv.conf。
- **注意**：`if (call_id == DATA_CALL_ID_PUBLIC)` 条件被注释掉（`data_call.c:88-89,142-143`），改为无条件执行（注释：重拨后 call_id 改变）。

**(2) IDLE（was CONNECTED）**（`data_call.c:180-209`）：处理 `AT+CFUN=0/4` 关射频时 SDK 发的 CONNECTED→IDLE（而非 DISCONNECTED）。打 `[EVENT] PDP Idle`；`g_pdp_connected=0`；`echo 1 > /tmp/dial_Status`；`update_network_status(0)`；清路由/NAT（同上循环删）；`g_if_name[0]='\0'`；unlink 两个 resolv 临时文件。
- **对应 CLAUDE.md 参考文档中"PDP connected→idle"排查**，防 `g_pdp_connected` 永不归零。

**(3) DISCONNECTED**（`data_call.c:210-240`）：打 `[EVENT] PDP Disconnected`；`g_pdp_connected=0`；`echo 1 > /tmp/dial_Status`；`update_network_status(0)`；清路由/NAT；`g_if_name[0]='\0'`；unlink resolv 临时文件。

### `dial_stat_enu flow_monitor_task(struct timespec* dial_timer)`（`data_call.c:244-277`）
- 读网卡 rx_packets（`nw_get_if_statistics_rx_packets`），首次记基准；后续若包数增长则刷新 `dial_timer`。`diff` 计算距上次增长时长，`> DIAL_TIMEOUT_SECONDS(120)` 返回 `dial_stat_stop`，否则 `usleep(4900ms)` 返回 `dial_stat_net_connected`。
- **经 grep 核实：`flow_monitor_task` 在工程内无调用者**（死代码，流量看门狗未启用）。

## 四、关键分支/阈值

| 位置 | 内容 |
|---|---|
| `data_call.c:42` | 事件过滤用 call_name==`"auto_network"` |
| `data_call.c:52/186/213` | g_pdp_connected 置位/清零点 |
| `data_call.c:71/202/232` | iptables MASQUERADE 循环删除上限 10 |
| `data_call.c:93/146` | 默认路由用 dev-only（NOARP 接口） |
| `data_call.c:109/159` | DNS 写入用 `\n`（LF，CLAUDE.md 约定） |
| `data_call.c:268` | 流量超时阈值 120s（flow_monitor_task，未启用） |

## 五、潜在问题 / 需实测确认

1. **回调线程内大量 `system()` 调用**（ip/iptables/echo/cat）：`data_call_status_ind_cb` 在 SDK 内部线程执行，每次 CONNECTED 触发约 8-10 个 fork+exec。与主循环并发；日志经 dial_log 递归锁保护（CLAUDE.md v1.28.2），但 system() 本身的并发副作用（如同时改 resolv.conf）需实测。
2. **`fp` 打开失败时中途 return**（`data_call.c:104,154`）：CONNECTED 分支若 resolv 文件打开失败会提前 return，跳过后续 IPv6 处理及可能的清理——中断在已设好路由/NAT 之后，状态部分完成（非致命，下次回调会重来）。
3. **`g_callid`（=1）与 dial_loop 局部 `g_call_id`（=4）是两个变量**：本文件 g_callid 仅 misc readCallIdFromFile 维护，回调过滤已改用 call_name，g_callid 在回调里未参与逻辑（`call_id==DATA_CALL_ID_PUBLIC` 判断已注释）。命名易混淆。
4. `flow_monitor_task` 为死代码，依赖 `diff`（test_utils）与 `DIAL_TIMEOUT_SECONDS`；如需启用流量看门狗需接入主循环。
5. IDLE 与 DISCONNECTED 两分支路由/NAT/DNS 清理代码高度重复（`data_call.c:190-208` 与 `220-239`），可提取公共函数（重构建议，非 bug）。
