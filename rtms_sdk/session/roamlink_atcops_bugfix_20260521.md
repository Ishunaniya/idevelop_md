# Roamlink AT+COPS=0 问题分析与修复记录

**日期**：2026-05-21  
**分支**：develop/rtms_sdk_v1.3_20240408_dc_switch  
**涉及文件**：`roamlink/roamlink.c`

---

## 一、问题描述

现场验证时，在 EG25 调制解调器的盒子上安装旧版 Roamlink 部署包（`roamlink_deploy_rtms_newC_260519.tar.gz`）后，出现以下现象：

- stub 通过 rtms 读取盒子参数（电池电压、外部电压）开始超时
- 平台下发的锁机/解锁命令，stub 侧延迟 **3-5 分钟**才收到反馈
- 移除 stub 侧的电压读取代码后，锁机命令反馈恢复为 **2 秒**

同事提出两个排查方向：
1. 排查 roamlink 安装后盒子内网发生了什么变化
2. 如何清理网络（能否自行恢复）

---

## 二、调查过程与文件依据

### 2.1 旧安装包（260519）对 RTMS 设备做了什么

**文件**：`/home/tronlong/lyp/zqzb/roamlink_deploy_rtms_newC_260519.tar.gz` → `install.sh`

RTMS 设备分支（有 `/etc/config/config.json`）的关键操作：

```sh
cp -f /usr/bin/modem_mng /usr/bin/dial
mv /usr/bin/modem_mng /usr/bin/modem_mng_bak     # ← 破坏 sw_mng 管理
ln -svf /usr/bin/dial /usr/dial/dial

ln -svf /etc/init.d/start_check_network /etc/rc5.d/S60start_check_network
unlink /etc/rc5.d/S20start_dial                   # ← 删除原有 dial 开机项
unlink /etc/rc5.d/S20start_vsim
```

直接后果（代码可证）：
- `/usr/bin/modem_mng` 被重命名为 `modem_mng_bak`，**sw_mng 的 apps.json 找不到 modem_mng**，看门狗失效
- `start_check_network` 服务接管网络管理，启动 `/usr/dial/dial`（= 当时机器上 modem_mng 的副本，脱离 sw_mng 独立运行）
- 260519 包**没有**包内 modem_mng 二进制，挪用已有的；260521 包有 `cp -f ./modem_mng /usr/bin/modem_mng`

### 2.2 AT+COPS=0 是触发点（双重代码证据）

**证据 1**：`check_network.sh`（260519 包，实际安装到设备的文件）

```sh
reset_operator() {
    echo -e "AT+COPS=0\r\n" >/dev/smd9
}
start_roamlink_service() {
    reset_operator
    echo "RBstartServiceMaster" | nc 127.0.0.1 5568
}
```

**证据 2**：当前代码 `roamlink/roamlink.c` → `roamlink_start_service()`

```c
#elif defined(USE_EG25_DIAL)
    pthread_mutex_lock(&g_at_port_mutex);
    if (0 != Ql_SendAT(g_smd_fd, "AT+COPS=0", "OK", 5000, rsp_msg))
        dial_log("[ROAMLINK] start_service: AT+COPS=0 failed: %s\n", rsp_msg);
    pthread_mutex_unlock(&g_at_port_mutex);
#endif
```

`roamlink_start_service()` 在 `dial.c` 中共有 **7 处调用**（lines 894/1028/1124/1794/1846/1890/1928），包括初始启动和策略回切，**每次都无条件发 AT+COPS=0**。

### 2.3 IPC 架构关键节点（代码可证）

**io_mng** 的 NN_REP 绑定在 `tcp://0.0.0.0:38000`（`cmd_sts_req.c` → `nn_bind`），对外全接口暴露。  
**stub** 通过 `req_client_new(MCU_REQ_PORT=38000)` 连接 `tcp://127.0.0.1:38000`（本地），查询 MCU 状态（含电池电压、ACC 状态、MCU 版本）。  
**modem_mng** 的 nanomsg REQ/REP 绑定在 `tcp://127.0.0.1:38001`（仅本地），不对外暴露。

### 2.4 rtms_client MQTT 断线机制（代码可证）

**文件**：`rtms_client/src/mosq_proc/client_mosquitto.c`

```c
mqtt_cfg.qos = 0;
mqtt_cfg.clean_session = true;
mosquitto_reconnect_delay_set(mosq, 60, 600, false);   // 重连延迟 60-600s
```

AT+COPS=0 → 调制解调器重选运营商 → rmnet_data* 接口短暂断开 → MQTT TCP 连接断线 → 重连等待 **60-600s**（即 1-10 分钟）。断线期间：
- `qos=0`：rtms_server 下发的命令不缓存，直接丢弃
- `clean_session=true`：重连后无历史队列

**这直接解释了"3-5 分钟才收到锁机反馈"**：命令在 rtms 断线期间无法到达盒子，等 MQTT 重连后才送达。

### 2.5 stub 侧 ev 循环阻塞（结构性风险，代码可证）

**文件**：`stub-sany-forklift/stub-sany-forklift.c`

`timer_task_thread`（ev 循环，1s 定时器）中，两个操作在**同一线程**顺序执行：

```c
static void timer_task(struct ev_loop *loop, ev_timer *w, int revents) {
    handle_cmd_timeout(handle);   // 锁机命令处理 ← 时间敏感
    if (cnt >= 5) {
        send_req_lock_status(handle);
        req_mcu_info(handle);     // nn_send(flag=0) ← 阻塞调用
        cnt = 0;
    }
}
```

`req_mcu_info()` 使用阻塞 `nn_send`（flag=0），连接到 `tcp://127.0.0.1:38000`（io_mng 本地 NN_REP）。  
nanomsg NN_REQ 默认重发间隔 60s（`NN_REQ_DEFAULT_RESEND_IVL = 60000`），stub 代码未设置 `NN_REQ_RESEND_IVL`。

若 io_mng 响应慢，`nn_send` 阻塞 → ev 循环停止 → `handle_cmd_timeout()` 不再执行 → 锁机命令积压无法回复。

> **注**：io_mng 响应慢的具体原因没有代码证据，是结构性风险，不是已证实的触发链。

**移除电压读取恢复 2 秒响应的解释**：移除 `req_mcu_info()` = 移除 ev 循环中的阻塞 `nn_send` = ev 循环恢复正常 → 锁机命令 2 秒内处理。

---

## 三、事实与推断的边界

| 结论 | 性质 |
|---|---|
| AT+COPS=0 在 7 处调用点无条件触发 | **代码事实** |
| rtms_client 重连延迟 60-600s，qos=0 命令丢失 | **代码事实** |
| stub ev 循环中阻塞 nn_send 与锁机处理同线程 | **代码事实** |
| 260519 包破坏 sw_mng 对 modem_mng 的管理 | **代码事实** |
| AT+COPS=0 导致 MQTT TCP 断线 | **网络行为推断**（代码只能证明 AT+COPS=0 被发出） |
| io_mng 响应慢导致 req_mcu_info() 阻塞 ev 循环 | **推断**（io_mng 响应速度无代码证据） |

---

## 四、同事两个排查方向的结论

### 排查点 1：roamlink 安装后盒子内网发生了什么变化

**有结论**：AT+COPS=0 是核心变化。

```
AT+COPS=0
  → 调制解调器放弃当前运营商，重新扫描注册
  → rmnet_data* 接口短暂断开
  → rtms_client MQTT TCP 连接断线
  → 重连等待 60-600s
  → 断线期间命令全部丢失（qos=0 + clean_session=true）
```

260519 包还额外：`mv modem_mng modem_mng_bak` → sw_mng 失去对 modem_mng 的管理和看门狗。

### 排查点 2：如何清理网络（能否恢复）

**有结论**：

- **旧包（260519）+ 旧 modem_mng（有 120s 重拨 bug）**：每 120s 触发一次重拨，反复断线，无法自行稳定。需重新安装 260521 包，恢复 `/usr/bin/modem_mng`，让 sw_mng 重新接管。
- **当前代码（bf0ec6d3 后）**：120s 重拨 bug 已修复，AT+COPS=0 一次性触发，MQTT 重连后网络自行稳定，不需要额外清理。

---

## 五、修复方案

### 5.1 修改文件：`roamlink/roamlink.c` → `roamlink_start_service()`

**修改逻辑**：发 AT+COPS=0 前先查 AT+COPS? 当前模式，若已是 mode=0（自动选网）则跳过，避免不必要的重注册。

```c
/* 修改前 */
pthread_mutex_lock(&g_at_port_mutex);
if (0 != Ql_SendAT(g_smd_fd, "AT+COPS=0", "OK", 5000, rsp_msg))
    dial_log("[ROAMLINK] start_service: AT+COPS=0 failed: %s\n", rsp_msg);
else
    dial_log("[ROAMLINK] start_service: AT+COPS=0 OK\n");
pthread_mutex_unlock(&g_at_port_mutex);
```

```c
/* 修改后 */
pthread_mutex_lock(&g_at_port_mutex);
/* 先查当前 COPS 模式：若已是自动选网（mode=0）则跳过，避免不必要的重注册 */
Ql_SendAT(g_smd_fd, "AT+COPS?", "OK", 3000, rsp_msg);
if (strstr(rsp_msg, "+COPS: 0") != NULL)
{
    dial_log("[ROAMLINK] start_service: AT+COPS already auto, skip AT+COPS=0\n");
}
else
{
    memset(rsp_msg, 0, sizeof(rsp_msg));
    if (0 != Ql_SendAT(g_smd_fd, "AT+COPS=0", "OK", 5000, rsp_msg))
        dial_log("[ROAMLINK] start_service: AT+COPS=0 failed: %s\n", rsp_msg);
    else
        dial_log("[ROAMLINK] start_service: AT+COPS=0 OK\n");
}
pthread_mutex_unlock(&g_at_port_mutex);
```

**AT+COPS? 响应格式说明**：
- `+COPS: 0,...` → mode=0，自动选网，跳过
- `+COPS: 1,2,"46001",...` → mode=1，手动锁定，发 AT+COPS=0
- `AT+COPS?` 超时/失败 → rsp_msg 为空，strstr 返回 NULL，走 else 分支继续发 AT+COPS=0（安全降级）

### 5.2 改动的实际影响分析

**正面影响（代码可证）**：

正常情况下物理 SIM 拨号后调制解调器已处于自动模式，`AT+COPS?` 返回 `+COPS: 0,...`，直接跳过 `AT+COPS=0`。`roamlink_start_service()` 的 7 个调用点（含策略回切）均受益，不触发重注册。

**修改前存在的不确定性**（验证后已消除）：

RBMaster 是否依赖 AT+COPS=0 才能注册虚拟 SIM — 黑盒，无源码。  
→ 设备实测已证明：RBMaster 不需要 AT+COPS=0 也能正常建立连接（~57s 建立，数据持续增长）。

**边界情况（安全）**：

`AT+COPS?` 超时或失败时 `rsp_msg` 为空，`strstr` 返回 `NULL`，走 else 分支继续发 `AT+COPS=0`，行为与修改前一致，不引入新风险。

---

## 六、验证结果

### 6.1 设备状态输出

```sh
~ # cat /usrdata/network.ini
[network]
network_select=1    # PREFER_ROAMLINK 策略

~ # ping -c 3 8.8.8.8
64 bytes from 8.8.8.8: icmp_req=1 ttl=112 time=56.6 ms
64 bytes from 8.8.8.8: icmp_req=2 ttl=112 time=28.1 ms
64 bytes from 8.8.8.8: icmp_req=3 ttl=112 time=58.1 ms
3 packets transmitted, 3 received, 0% packet loss

~ # cat /tmp/network_type
2   # 2 = Roamlink 通道

~ # cat /tmp/dial_status
[dial]
version=1.31.0
state=roamlink_active
policy=1 (PREFER_ROAMLINK)
channel=ROAMLINK
roamlink_available=1

[network]
status=1
type=2

[counters]
tcp_fail_count=0
roamlink_fail_count=0
roamlink_rx_packets=65

[roamlink]
state=active
biz_no_data_sec=0
rbmaster_pid=1505

[stats]
outage_count=0
last_outage_sec=0
total_outage_sec=0
```

### 6.2 设备日志（实测）

```
[1970-01-01 09:21:14] === Dial Program Started ===
[1970-01-01 09:21:17] EG25 modem_mng Version: 1.31.0
[1970-01-01 09:21:17] [ROAMLINK] network_select = 1
[1970-01-01 09:21:17] [ROAMLINK] probe OK (RBMaster + conf.ini + license valid)
[1970-01-01 09:21:19] [ROAMLINK] Policy requires Roamlink as initial channel. Starting RBMaster...
[1970-01-01 09:21:22] [ROAMLINK] start_service: AT+COPS already auto, skip AT+COPS=0   ← 修改生效
[1970-01-01 09:21:22] [ROAMLINK] send_cmd: sent "RBstartServiceMaster" to 127.0.0.1:5568
[1970-01-01 09:21:22] [ROAMLINK] start_service: Roamlink service started, network_type=2
[1970-01-01 09:22:19] [ROAMLINK] Ping OK in starting state, channel now active          ← 约57s 建立连接
[1970-01-01 09:22:19] [HEARTBEAT] CH:ROAMLINK | SIM:1 | CSQ:16 | RX_PKT:43
[1970-01-01 09:22:49] [HEARTBEAT] RX_PKT:65
[1970-01-01 09:23:19] [HEARTBEAT] RX_PKT:92
[1970-01-01 09:23:49] [HEARTBEAT] RX_PKT:117
[1970-01-01 09:24:19] [HEARTBEAT] RX_PKT:139
[1970-01-01 09:24:50] [HEARTBEAT] RX_PKT:166
```

### 6.3 验证结论

| 验证项 | 结果 |
|---|---|
| AT+COPS=0 被跳过 | **已证明**（日志直接输出） |
| RBMaster 无需 AT+COPS=0 也能正常连接 | **已证明**（~57s 建立连接，RX_PKT 持续增长） |
| Roamlink 连接稳定 | **已证明**（DownTime:0s，RL_FAIL:0，outage_count:0） |
| rtms_client MQTT 在启动期间未断线 | **未验证**（缺 rtms_client 侧日志） |

---

## 七、当前代码状态评估

| 问题 | 修复前状态 | 修复后状态 |
|---|---|---|
| AT+COPS=0 无条件触发（7 处） | 每次 roamlink_start_service() 都触发 | 调制解调器已在自动模式时跳过 |
| 120s 周期重拨（dangling pointer bug） | 已在 bf0ec6d3 修复 | 已修复 |
| sw_mng 管理 modem_mng | 260521 包恢复正常 | 正常 |

---

## 八、提交说明

```
[modem_mng] [BUGFIX] EG25 roamlink_start_service 跳过无效 AT+COPS=0，避免 4G 重注册中断 rtms 连接

- 问题：roamlink_start_service() 无条件发 AT+COPS=0；若调制解调器
  已处于自动选网模式（mode=0），该命令会触发不必要的重注册，导致
  rmnet_data* 接口短暂断开、rtms_client MQTT 断线（重连延迟 60-600s），
  期间下发命令因 qos=0+clean_session 全部丢失

- 修复：发 AT+COPS=0 前先查 AT+COPS? 当前模式；若返回 "+COPS: 0"
  （已是自动模式）则跳过，仅在手动锁定运营商时才发

- 验证（设备日志）：
    [ROAMLINK] start_service: AT+COPS already auto, skip AT+COPS=0
    [ROAMLINK] Ping OK in starting state, channel now active  (~57s)
    RX_PKT 持续增长，DownTime:0s，RL_FAIL:0
  RBMaster 无需 AT+COPS=0 即可正常建立虚拟 SIM 连接

- 影响范围：roamlink.c roamlink_start_service()，仅 USE_EG25_DIAL 平台
```

---

## 九、遗留问题

1. **rtms_client MQTT 连接稳定性未验证**：需在 roamlink 启动期间同时抓取 rtms_client 日志，确认 MQTT 未断线，才能完整证明修复效果
2. **stub ev 循环阻塞问题未根治**：`req_mcu_info()` 在 `timer_task_thread` 中使用阻塞 `nn_send(flag=0)`，与 `handle_cmd_timeout()` 同线程，存在结构性风险。建议改为 `NN_DONTWAIT` 或将 MCU 查询移到独立线程（属于 stub 侧问题，不在 modem_mng 范围内）
