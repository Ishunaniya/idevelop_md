# open_dial V1.26 — 源码问题分析与修复方案

> 基于源码逐行分析 + 实际设备日志验证  
> 问题总数：12项 | 严重：3项 | 中等：5项 | 轻微：4项

---

## 目录

1. [Fast Retry 倒计时起点错误（严重）](#1-fast-retry-倒计时起点错误严重)
2. [L3 恢复被完全注释（严重）](#2-l3-恢复被完全注释严重)
3. [call_id 过滤注释导致路由防火墙被破坏（严重）](#3-call_id-过滤注释导致路由防火墙被破坏严重)
4. [DownTime 无抖动过滤（中等）](#4-downtime-无抖动过滤中等)
5. [L2 射频重置无频率保护（中等）](#5-l2-射频重置无频率保护中等)
6. [断开连接时不清理路由表（中等）](#6-断开连接时不清理路由表中等)
7. [data_call_init 最长阻塞 20 秒无日志（中等）](#7-data_call_init-最长阻塞-20-秒无日志中等)
8. [断开时 start_fail_ts 未重置（中等）](#8-断开时-start_fail_ts-未重置中等)
9. [JSON 内存泄漏（轻微）](#9-json-内存泄漏轻微)
10. [两套网络状态写入接口并存（轻微）](#10-两套网络状态写入接口并存轻微)
11. [executeATCommand 内存管理责任不明（轻微）](#11-executeatcommand-内存管理责任不明轻微)
12. [SIM 卡硬编码 SLOT_1（轻微）](#12-sim-卡硬编码-slot_1轻微)

---

## 1. Fast Retry 倒计时起点错误（严重）

### 问题描述

倒计时从 `dial_loop()` 入口开始，而 `ql_data_call_start()` 是异步 API，PDP 上下文的实际建立需要数秒到十几秒，建立完成前 Ping 必然失败。初始化流程本身也会消耗时间，留给 PDP 建立和 Ping 的窗口不够用。

### 实际日志印证

```
01:00:02  Program started  ← start_loop_ts 打点，倒计时开始
01:00:02  apn set success  ← ql_data_call_start() 已异步发出
01:00:03  [HEARTBEAT] SIM:READY | REG:1 | CSQ:28 | DownTime:0s
          ↑ 只有 1 秒初始化，但 PDP 还没建立好，Ping 全部失败
01:00:14  Fast Fail: Timeout 10000ms  ← 误判超时退出
```

### 问题代码

**文件：`dial.c` — `dial_loop()` 函数**

```c
void dial_loop(...) {
    // ❌ 倒计时在这里开始，PDP 还没建立
    uint64_t start_loop_ts = now_ms();
    int is_fast_fail_mode = (current_launch_count != -1);

    // ... 初始化流程 ...
    ql_data_call_start(g_call_id);  // 异步，立即返回，PDP 还没好

    // 直接进状态机开始 Ping，注定失败
    while (1) {
        if (is_fast_fail_mode && !has_notified_connect) {
            if (tnow - start_loop_ts > FAST_FAIL_TIMEOUT_MS) {
                exit(1);  // ❌ PDP 还没建立好就被误判超时
            }
        }
    }
}
```

### 修复方案

**第一步：在 `data_call.c` 的回调里设置 PDP 就绪标志**

```c
// dial.c 顶部添加全局标志
static volatile int g_pdp_connected = 0;
```

```c
// data_call/data_call.c — data_call_status_ind_cb()
void data_call_status_ind_cb(int call_id, ...) {
    if (p_msg->call_status == QL_NET_DATA_CALL_STATUS_CONNECTED) {
        g_pdp_connected = 1;   // ✅ PDP 建立成功
        // 原有路由/DNS 配置逻辑不变...
    }
    else if (p_msg->call_status == QL_NET_DATA_CALL_STATUS_DISCONNECTED) {
        g_pdp_connected = 0;   // ✅ 断开时清零
        // ...
    }
}
```

**第二步：修改 Fast Fail 检测逻辑，从 PDP 就绪后才开始计时**

```c
void dial_loop(...) {
    uint64_t start_loop_ts = now_ms();
    uint64_t pdp_ready_ts = 0;              // ✅ 新增：PDP 就绪时间戳

    #define PDP_WAIT_TIMEOUT_MS  60000      // PDP 建立最长等待 60 秒

    while (1) {
        uint64_t tnow = now_ms();

        if (is_fast_fail_mode && !has_notified_connect) {
            if (g_pdp_connected) {
                // ✅ PDP 已建立，从这一刻开始计时
                if (pdp_ready_ts == 0) {
                    pdp_ready_ts = tnow;
                    dial_log("[INIT] PDP connected, starting Ping timeout window.\n");
                }
                if (tnow - pdp_ready_ts > FAST_FAIL_TIMEOUT_MS) {
                    dial_log("[INIT] Fast Fail: PDP ready but no Ping success in %dms.\n",
                             FAST_FAIL_TIMEOUT_MS);
                    exit(1);
                }
            } else {
                // ✅ PDP 还没建立，不计 Ping 超时，只检查 PDP 建立总超时
                if (tnow - start_loop_ts > PDP_WAIT_TIMEOUT_MS) {
                    dial_log("[INIT] Fast Fail: PDP not established in %ds.\n",
                             PDP_WAIT_TIMEOUT_MS / 1000);
                    exit(1);
                }
            }
        }
        // 其余逻辑不变...
    }
}
```

### 修复效果

```
修复前：
  倒计时开始 → 初始化(1~5s) → Ping失败(PDP未建立) → 10s超时 → 误判退出

修复后：
  倒计时开始 → 等待PDP建立(最多60s) → PDP就绪 → 10s内Ping成功 → 正常
                                                         ↓ Ping失败
                                                    Fast Fail退出（真实故障）
```

---

## 2. L3 恢复被完全注释（严重）

### 问题描述

断网超过 30 分钟后触发 L3，但所有恢复操作均被注释，只打印一行日志，程序继续运行但永远无法自动恢复，必须人工介入。

### 问题代码

**文件：`dial.c` — `ST_RECOVERY case 3`**

```c
case 3:
    dial_log("[RECOVERY L3] FATAL: Network down for 30mins. But Not REBOOTING MODULE...\n");
    sync();
    sleep(5);
    // ❌ 以下全部注释，什么都不做：
    // system("serial_atcmd at+cfun=1,1");
    // system("reboot");
    // exit(1);
    break;
```

### 修复方案

建议分两步执行，先尝试模组软重启，失败后再系统重启：

```c
case 3:
    dial_log("[RECOVERY L3] Network down 30mins. Attempting module reboot...\n");
    log_close();   // ✅ 确保日志落盘
    sync();        // ✅ 同步文件系统

    // ✅ 第一步：模组软重启
    system("serial_atcmd at+cfun=1,1");
    sleep(15);     // 等待模组重启完成

    // ✅ 重启后重新发起拨号
    ql_data_call_start(g_call_id);

    // ✅ 重置故障计时器，重新开始监测
    start_fail_ts = 0;
    last_recovery_ts = tnow;
    has_notified_connect = 0;

    // ✅ 如果模组重启后仍无法恢复，下一轮 30 分钟后再次触发 L3
    // 如果需要系统重启作为终极兜底，可以增加 L3 触发次数计数：
    // static int l3_count = 0;
    // if (++l3_count >= 2) { system("reboot"); }
    break;
```

---

## 3. call_id 过滤注释导致路由防火墙被破坏（严重）

### 问题描述

`data_call_status_ind_cb` 中的 `call_id` 过滤被注释，任何数据连接的状态变化都会触发路由设置和 `iptables -t filter -F`（清空全部防火墙规则），在多连接场景下是破坏性操作。

### 问题代码

**文件：`data_call/data_call.c`**

```c
void data_call_status_ind_cb(int call_id, ...) {
    // ❌ call_id 过滤被注释，所有连接事件都会执行
    // if (call_id != DATA_CALL_ID_PUBLIC) { return; }

    if (p_msg->call_status == QL_NET_DATA_CALL_STATUS_CONNECTED) {
        // ...
        system("iptables -t filter -F");   // ❌ 清空全部防火墙规则！
        // 任何其他连接触发也会执行这里
    }
}
```

### 修复方案

```c
void data_call_status_ind_cb(int call_id, ...) {
    // ✅ 恢复 call_id 过滤
    if (call_id != DATA_CALL_ID_PUBLIC) {
        dial_log("[DATACALL] Ignore event for call_id=%d\n", call_id);
        return;
    }

    if (p_msg->call_status == QL_NET_DATA_CALL_STATUS_CONNECTED) {
        // ✅ 不要用 -F 清空全部规则，只操作本连接相关的规则
        // 先删除可能存在的旧规则，避免重复叠加
        char del_cmd[256];
        snprintf(del_cmd, sizeof(del_cmd),
            "iptables -t nat -D POSTROUTING -o %s -j MASQUERADE 2>/dev/null",
            p_msg->device);
        system(del_cmd);

        // 再添加新规则
        snprintf(cmd_buf, sizeof(cmd_buf),
            "iptables -t nat -A POSTROUTING -o %s -j MASQUERADE",
            p_msg->device);
        system(cmd_buf);

        // ... 其余逻辑不变
    }
}
```

---

## 4. DownTime 无抖动过滤（中等）

### 问题描述

1 次 Ping 失败就立即设置 `start_fail_ts`，蜂窝网络正常的偶发丢包（小区重选、单包超时）会在日志里产生 DownTime:2s、DownTime:10s 的噪声，无法区分真实断网和正常抖动。

### 问题代码

**文件：`dial.c` — `ST_PING case`**

```c
case ST_PING:
    bool ok = test_can_ping_google(NULL);
    if (!ok) {
        // ❌ 第一次失败立即计时，无任何过滤
        if (start_fail_ts == 0) start_fail_ts = tnow;
    }
```

### 修复方案

引入连续失败计数器，连续失败 N 次后才认定为断网：

```c
// dial_loop() 局部变量，加在状态机变量声明区
int consecutive_ping_fail = 0;
#define PING_FAIL_THRESHOLD  3   // 连续失败 3 次才计为断网（约 4.5 秒）

case ST_PING:
    bool ok = test_can_ping_google(NULL);
    if (ok) {
        consecutive_ping_fail = 0;   // ✅ 成功后重置计数
        start_fail_ts = 0;
        // ... 原有成功逻辑
    } else {
        consecutive_ping_fail++;
        if (consecutive_ping_fail >= PING_FAIL_THRESHOLD) {
            // ✅ 连续失败才开始计时
            if (start_fail_ts == 0) {
                start_fail_ts = tnow;
                dial_log("[WARN] Consecutive ping fail %d times, start fault timer.\n",
                         PING_FAIL_THRESHOLD);
            }
        }
        // ... 原有分级恢复逻辑
    }
```

同时将 Ping 命令从 `-c 1` 改为 `-c 3`，进一步降低单包丢失的误判率：

```c
// test_can_ping_google() 里
// ❌ 原来
snprintf(cmd, sizeof(cmd), "ping -I %s -c 1 -W 2 8.8.8.8 2>/dev/null", interface_name);

// ✅ 改为：3包中任1包成功即判通，超时改为3秒
snprintf(cmd, sizeof(cmd), "ping -I %s -c 3 -W 3 8.8.8.8 2>/dev/null", interface_name);
```

---

## 5. L2 射频重置无频率保护（中等）

### 问题描述

`restart_cfun()` 有次数上限和时间间隔保护，但 L2 直接通过 `system()` 调用 AT+CFUN，只有 5 分钟冷却，理论上可以无限次执行射频重置，对模组硬件有潜在损耗风险。

### 问题代码

**文件：`dial.c` — `ST_RECOVERY case 2`**

```c
case 2:
    // ❌ 直接调用，无次数限制
    system("serial_atcmd at+cfun=0");
    sleep(3);
    system("serial_atcmd at+cfun=1");
```

### 修复方案

复用 `restart_cfun()` 的保护逻辑，或者给 L2 加独立计数：

```c
// dial_loop() 变量区新增
static int l2_count = 0;
#define L2_MAX_COUNT  20   // L2 最多执行 20 次

case 2:
    if (l2_count >= L2_MAX_COUNT) {
        dial_log("[RECOVERY L2] L2 limit reached (%d), skip.\n", L2_MAX_COUNT);
        break;
    }
    l2_count++;
    dial_log("[RECOVERY L2] Toggling RF, count=%d/%d\n", l2_count, L2_MAX_COUNT);
    system("serial_atcmd at+cfun=0");
    sleep(3);
    system("serial_atcmd at+cfun=1");
    sleep(10);
    ql_data_call_start(g_call_id);
    break;
```

---

## 6. 断开连接时不清理路由表（中等）

### 问题描述

连接断开时只更新状态文件，不删除旧的 default 路由。重连后执行 `ip ro add default` 会叠加路由条目，多次重连后路由表出现多条 default 路由，造成路由不确定性。

### 问题代码

**文件：`data_call/data_call.c`**

```c
// 连接成功：设置路由
system("ip ro add default via %s dev %s");  // 只增不删

// 连接断开：什么都没做
else if (p_msg->call_status == QL_NET_DATA_CALL_STATUS_DISCONNECTED) {
    update_network_status(0);
    // ❌ 没有清理路由
}
```

### 修复方案

```c
else if (p_msg->call_status == QL_NET_DATA_CALL_STATUS_DISCONNECTED) {
    update_network_status(0);

    // ✅ 清理该接口的路由
    if (strlen(g_if_name) > 0) {
        char del_cmd[256];
        snprintf(del_cmd, sizeof(del_cmd),
            "ip ro del default dev %s 2>/dev/null", g_if_name);
        system(del_cmd);
        dial_log("[DATACALL] Route cleared for dev %s\n", g_if_name);
    }
}
```

连接成功时也先删后加，避免重复叠加：

```c
// 设置路由前先删除旧路由
char del_cmd[256];
snprintf(del_cmd, sizeof(del_cmd),
    "ip ro del default dev %s 2>/dev/null", p_msg->device);
system(del_cmd);

// 再添加新路由
snprintf(cmd_buf, sizeof(cmd_buf),
    "ip ro add default via %s dev %s", p_msg->addr.gateway, p_msg->device);
system(cmd_buf);
```

---

## 7. data_call_init 最长阻塞 20 秒无日志（中等）

### 问题描述

`ql_data_call_init()` 最多重试 200 次，每次间隔 100ms，总计最长阻塞 20 秒。期间没有任何日志输出，进程看起来像卡死了，给调试带来困难。

### 问题代码

**文件：`dial.c` — `dial_loop()`**

```c
retry_cnt = 200;
while (retry_cnt > 0) {
    ret = ql_data_call_init();
    if (ret == -1001) {
        retry_cnt--;
        usleep(100 * 1000);
        continue;   // ❌ 每次重试无任何打印
    }
    break;
}
```

### 修复方案

```c
retry_cnt = 200;
dial_log("[INIT] Waiting for ql_data_call_init...\n");
while (retry_cnt > 0) {
    ret = ql_data_call_init();
    if (ret == -1001) {
        retry_cnt--;
        // ✅ 每 20 次打印一次进度（每 2 秒）
        if (retry_cnt % 20 == 0) {
            dial_log("[INIT] data_call_init retry, remaining=%d\n", retry_cnt);
        }
        usleep(100 * 1000);
        continue;
    }
    break;
}

if (ret != QL_ERR_OK) {
    dial_log("[ERROR] data_call_init failed after 200 retries, ret=%d\n", ret);
} else {
    dial_log("[INIT] data_call_init OK.\n");
}
```

---

## 8. 断开时 start_fail_ts 未重置（中等）

### 问题描述

`data_call_status_ind_cb` 收到 DISCONNECTED 时，没有通知状态机重置 `start_fail_ts`。如果连接断开时 `start_fail_ts` 已经在计时，断开事件本身不会影响它，可能导致 L1/L2 触发时机与实际断网时长不对应。

### 问题代码

**文件：`data_call/data_call.c`**

```c
else if (p_msg->call_status == QL_NET_DATA_CALL_STATUS_DISCONNECTED) {
    update_network_status(0);
    // ❌ 没有重置故障计时器
}
```

### 修复方案

在全局变量区声明一个标志：

```c
// dial.c 顶部
volatile int g_need_reset_fail_ts = 0;
```

```c
// data_call.c DISCONNECTED 处理
else if (p_msg->call_status == QL_NET_DATA_CALL_STATUS_DISCONNECTED) {
    update_network_status(0);
    g_need_reset_fail_ts = 1;   // ✅ 通知状态机重置计时器
}
```

```c
// dial_loop() ST_PING 开头
case ST_PING:
    // ✅ 检查是否需要重置故障计时器
    if (g_need_reset_fail_ts) {
        g_need_reset_fail_ts = 0;
        start_fail_ts = tnow;   // 从断开事件时刻重新开始计时
        dial_log("[EVENT] Data call disconnected, reset fault timer.\n");
    }
```

---

## 9. JSON 内存泄漏（轻微）

### 问题描述

`apn_load_from_json()` 中对 json-c 的引用计数处理有误，导致每次调用产生内存泄漏。虽然 `set_apn()` 只在启动时调用一次，但说明 json-c API 使用方式有根本性误解。

### 问题代码

**文件：`apn/apn.c`**

```c
// ❌ 问题一：json_object_put(json_temp) 在读取后立即调用
// json_object_object_get_ex 返回的是借用引用，不需要 put
if (json_object_object_get_ex(json_apn, "apn", &json_temp)) {
    memcpy(p_apn_obj[i].apn, json_object_get_string(json_temp), ...);
}
json_object_put(json_temp);   // ❌ 不应该调用

// ❌ 问题二：根对象永远不释放
// json_object_put(obj);   // 注释掉了，永久内存泄漏
```

### 修复方案

```c
// ✅ json_object_object_get_ex 返回借用引用，不需要 put
// 移除所有字段读取后的 json_object_put(json_temp) 调用

if (json_object_object_get_ex(json_apn, "apn", &json_temp)) {
    p_json_str = json_object_get_string(json_temp);
    memcpy(p_apn_obj[i].apn, p_json_str, strlen(p_json_str));
    // ✅ 不调用 json_object_put(json_temp)
}

// ... 其他字段同理 ...

// ✅ 函数末尾释放根对象（需验证 json-c 版本兼容性）
if (obj) {
    json_object_put(obj);
}
```

---

## 10. 两套网络状态写入接口并存（轻微）

### 问题描述

`update_network_status()`（misc.c）和 `nw_mark_network_status()`（nw.c）都写同一个文件 `/tmp/network_status`，接口行为不一致，维护混乱。`nw_mark_network_status` 还缺少 `O_TRUNC` 标志。

### 问题代码

**文件：`misc.c`**

```c
// 接口一：先读再比较，有变化才写
int update_network_status(int new_status) {
    int current = read_status();
    if (current == new_status) return 0;
    write_status(new_status);
}
```

**文件：`nw.c`**

```c
// 接口二：直接写，无比较，且缺少 O_TRUNC
void nw_mark_network_status(int net_status) {
    int fd = open(NW_STATUS_PATH, O_WRONLY | O_CREAT);  // ❌ 缺 O_TRUNC
    write(fd, br_str, strlen(br_str));
    close(fd);
}
```

### 修复方案

统一使用 `update_network_status()`，废弃 `nw_mark_network_status()`：

```c
// nw.c 中替换调用
// ❌ nw_mark_network_status(1);
// ✅ update_network_status(1);

// 并在 nw.c 中删除 nw_mark_network_status() 的实现
// 或保留但内部改为调用 update_network_status()
void nw_mark_network_status(int net_status) {
    update_network_status(net_status);  // ✅ 统一入口
}
```

---

## 11. executeATCommand 内存管理责任不明（轻微）

### 问题描述

`executeATCommand()` 返回 `malloc` 分配的内存，调用方必须手动 `free()`，接口设计将内存管理责任抛给调用方，后续维护时容易遗漏。

### 问题代码

**文件：`misc.c`**

```c
char *executeATCommand(const char *cmd) {
    char *result = malloc(1);   // 动态分配，调用方必须释放
    // ...
    return result;
}
```

### 修复方案

改为调用方传入 buffer，函数内部填充，避免堆分配：

```c
// ✅ 调用方提供 buffer，无需 free
int executeATCommand(const char *cmd, char *out_buf, size_t buf_size) {
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return -1;

    size_t offset = 0;
    char line[128];
    while (fgets(line, sizeof(line), pipe) && offset < buf_size - 1) {
        size_t len = strlen(line);
        if (offset + len >= buf_size - 1) len = buf_size - 1 - offset;
        memcpy(out_buf + offset, line, len);
        offset += len;
    }
    out_buf[offset] = '\0';
    pclose(pipe);
    return 0;
}

// 调用示例
char response[512];
if (executeATCommand("serial_atcmd at+csq", response, sizeof(response)) == 0) {
    // 直接使用 response，无需 free
}
```

---

## 12. SIM 卡硬编码 SLOT_1（轻微）

### 问题描述

`sim_get_iccid()` 和 `sim_get_imsi()` 均硬编码使用 `QL_SIM_SLOT_1`，无法支持双卡设备。

### 问题代码

**文件：`sim/sim.c`**

```c
int sim_get_iccid(char *iccid) {
    QL_SIM_SLOT_E slot = QL_SIM_SLOT_1;  // ❌ 硬编码
    return ql_sim_get_iccid(slot, iccid, QL_SIM_ICCID_LENGTH + 1);
}
```

### 修复方案

```c
// sim.h 新增
#define DEFAULT_SIM_SLOT  QL_SIM_SLOT_1

// sim.c 改为参数化
int sim_get_iccid(char *iccid, QL_SIM_SLOT_E slot) {
    return ql_sim_get_iccid(slot, iccid, QL_SIM_ICCID_LENGTH + 1);
}

// 调用方
sim_get_iccid(iccid, DEFAULT_SIM_SLOT);   // 默认用卡槽1
// 双卡场景
sim_get_iccid(iccid, QL_SIM_SLOT_2);     // 切换卡槽2
```

---

## 修复优先级汇总

| # | 问题 | 严重程度 | 修复难度 | 建议优先级 |
|---|------|---------|---------|----------|
| 1 | Fast Retry 倒计时起点错误 | 🔴 严重 | 低 | P0 立即修复 |
| 2 | L3 恢复被注释 | 🔴 严重 | 低 | P0 立即修复 |
| 3 | call_id 过滤注释 + iptables -F | 🔴 严重 | 低 | P0 立即修复 |
| 4 | DownTime 无抖动过滤 | 🟡 中等 | 低 | P1 近期修复 |
| 5 | L2 无频率保护 | 🟡 中等 | 低 | P1 近期修复 |
| 6 | 断开不清理路由 | 🟡 中等 | 低 | P1 近期修复 |
| 7 | data_call_init 阻塞无日志 | 🟡 中等 | 低 | P1 近期修复 |
| 8 | 断开时 start_fail_ts 未重置 | 🟡 中等 | 中 | P1 近期修复 |
| 9 | JSON 内存泄漏 | 🟢 轻微 | 中 | P2 版本迭代 |
| 10 | 两套状态写入接口 | 🟢 轻微 | 低 | P2 版本迭代 |
| 11 | executeATCommand 内存管理 | 🟢 轻微 | 中 | P2 版本迭代 |
| 12 | SIM 卡硬编码 SLOT_1 | 🟢 轻微 | 低 | P2 版本迭代 |

---

*文档基于 open_dial V1.26 全部源码分析及实际设备日志验证生成*
