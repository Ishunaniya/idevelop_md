# modem_mng 拨号机制文档

## 版本信息
- **当前版本**: 1.30
- **最后更新**: 2025-01-28
- **基于**: EC200A Quectel SDK

---

## 目录
1. [概述](#概述)
2. [启动流程](#启动流程)
3. [拨号初始化](#拨号初始化)
4. [拨号循环机制](#拨号循环机制)
5. [故障恢复策略](#故障恢复策略)
6. [快速失败重试机制](#快速失败重试机制)
7. [CFUN 调用限制机制](#cfun-调用限制机制)
8. [心跳日志机制](#心跳日志机制)
9. [网络配置](#网络配置)
10. [状态监控](#状态监控)

---

## 概述

modem_mng 是 EC200A 模块的拨号管理程序，负责建立和维护 4G 网络连接。版本 1.30 集成了从 `open_dial` 项目移植的优秀机制，大幅提升了拨号的稳定性和可维护性。

### 核心特性
- **多阶段拨号流程**：初始化 → 创建连接 → 启动连接 → 状态监控
- **分级故障恢复**：根据断网时长自动触发不同等级的恢复策略
- **快速失败重试**：前3次启动时快速检测，避免长时间卡死
- **CFUN 调用限制**：防止频繁调用射频开关导致模块异常
- **心跳日志**：定期输出诊断信息，便于问题排查
- **SD卡日志**：所有关键操作记录到SD卡，支持离线分析

---

## 启动流程

### 1. 程序入口 (`main`)

```cpp
int main(int argc, char *argv[])
```

**执行步骤：**

1. **初始化日志系统**
   ```cpp
   log_init();
   ```
   - 检查 SD 卡挂载状态（`/media/sdcard`）
   - 检查剩余空间（需大于 500MB）
   - 创建日志目录（`/media/sdcard/dial_log/`）
   - 生成时间戳日志文件（`dial_YYYYMMDD_HHMMSS.log`）

2. **网卡就绪检测**
   ```cpp
   dial.wait_for_interface("ccinet0", 30);
   ```
   - 使用 `ioctl(SIOCGIFINDEX)` 检测网卡存在性
   - 最多等待 30 秒
   - 超时则退出程序

3. **注册到进程管理系统**
   ```cpp
   cpactive_add_pinfo(cpactive, timeout, pname, pversion, NULL);
   ```
   - 程序名：`modem_mng`
   - 版本号：`1.30`
   - 超时时间：60 秒

4. **启动拨号循环**
   ```cpp
   dial.dial_loop(cpactive, interface_name, callback);
   ```

---

## 拨号初始化

### 1. 快速失败重试检测

在进入拨号循环前，检查启动计数：

```cpp
int current_launch_count = check_and_update_retry_count();
```

**机制说明：**
- 读取 `/tmp/dial_retry_count` 文件获取尝试次数
- 如果次数 < 3，进入快速失败模式（10秒超时）
- 如果次数 >= 3，进入持久循环模式（正常运行）
- 成功后自动清除计数文件

### 2. 数据拨号服务初始化

```cpp
ql_data_call_init();
```

**重试机制：**
- 最多等待 20 秒（200 次 × 100ms）
- 如果返回 `QL_ERR_SERVICE_NOT_READY` 或 `-1001`，继续重试
- 初始化成功后设置状态回调函数

### 3. 创建和配置 Data Call

```cpp
// 创建 Data Call
ql_data_call_create(g_call_id, "auto_network", 0);

// 配置参数
ql_data_call_param_t *p_cfg = ql_data_call_param_alloc();
ql_data_call_param_set_apn_id(p_cfg, 1);              // APN ID = 1
ql_data_call_param_set_ip_version(p_cfg, QL_NET_IP_VER_V4);  // IPv4
ql_data_call_param_set_reconnect_mode(p_cfg, QL_NET_DATA_CALL_RECONNECT_NORMAL);
ql_data_call_param_set_reconnect_interval(p_cfg, time_list, 2);  // 35秒自动重连

// 应用配置
ql_data_call_config(g_call_id, p_cfg);

// 启动连接
ql_data_call_start(g_call_id);
```

**配置参数：**
- **APN ID**: 1（默认）
- **IP 版本**: IPv4
- **自动重连间隔**: 35 秒
- **连接名称**: `auto_network`

---

## 拨号循环机制

拨号循环采用**多阶段轮询**机制，持续监控网络状态。

### 状态机设计

```cpp
enum Stage { 
    ST_STATUS = 0,    // 读取拨号状态
    ST_SIM,          // 检查 SIM 卡状态
    ST_SIGNAL,       // 检查信号强度
    ST_PING,         // 检查网络连通性
    ST_RECOVERY      // 执行故障恢复
};
```

### 各阶段说明

#### 1. ST_STATUS - 状态读取
- **功能**: 读取最近一次回调更新的拨号状态
- **数据源**: `g_last_call_status`（由 `data_call_status_ind_cb` 更新）
- **执行频率**: 每轮循环
- **耗时**: 极短（仅读取变量）

#### 2. ST_SIM - SIM 卡检查
- **功能**: 检查 SIM 卡状态和网络注册状态
- **执行频率**: 每 800ms（`SIM_INTERVAL_MS`）
- **检查内容**:
  - SIM 卡状态（READY/NOT INSERTED/ERROR）
  - 网络注册状态（0=未注册, 1=已注册, 2=搜索中, 3=注册被拒绝, 4=未知, 5=已注册漫游）
- **日志输出**: 
  - `SIM&Reg OK` - 正常
  - `SIM status NOT OK` - SIM 卡异常
  - `Network NOT registered` - 未注册网络

#### 3. ST_SIGNAL - 信号强度检查
- **功能**: 检查信号强度（RSSI）
- **执行频率**: 每 800ms（`SIG_INTERVAL_MS`）
- **检查方法**: 执行 `AT+CSQ` 命令
- **返回值**: 0-31（正常信号），99（无信号或未知）
- **日志输出**: `Signal RSSI=X (0..31, 99=unknown)`

#### 4. ST_PING - 网络连通性检查
- **功能**: 检查是否能够访问互联网
- **执行频率**: 每 1500ms（`PING_INTERVAL_MS`）
- **检查方法**: Ping `8.8.8.8`（Google DNS）
- **实现方式**: 
  - 使用 `popen` 执行 `ping -c 1 -W 2 8.8.8.8`
  - 通过判断输出中是否包含 "ttl=" 确定成功
  - 解决 SIGCHLD 信号处理导致的返回值错误问题

**Ping 成功时：**
- 清除故障计时器
- 清除恢复等级
- 如果处于快速失败模式，清除计数文件
- 触发连接成功回调
- LED 状态：亮灯（状态 2）

**Ping 失败时：**
- 记录故障开始时间
- 根据断网时长判断是否需要触发恢复策略
- LED 状态：灭灯（状态 0）

#### 5. ST_RECOVERY - 故障恢复
- **功能**: 执行分级故障恢复策略
- **触发条件**: Ping 失败且达到恢复阈值
- **恢复等级**: Level 1/2/3（详见[故障恢复策略](#故障恢复策略)）

### 循环控制

```cpp
while (1) {
    uint64_t tnow = now_ms();
    
    // 快速失败检测（前3次启动）
    if (is_fast_fail_mode && !has_notified_connect) {
        if (tnow - start_loop_ts > FAST_FAIL_TIMEOUT_MS) {
            exit(1);  // 10秒超时，退出程序
        }
    }
    
    // 心跳日志（30秒间隔）
    if (tnow > next_heartbeat_ts) {
        // 输出诊断信息
    }
    
    // 状态机轮询
    switch (stage) {
        // ...
    }
    
    // LED 控制
    // ...
    
    usleep(50 * 1000);  // 50ms，避免 busy loop
}
```

---

## 故障恢复策略

采用**分级故障恢复策略**，根据断网时长自动触发不同等级的恢复动作。

### 恢复等级定义

| 等级 | 触发条件 | 恢复动作 | 说明 |
|------|----------|----------|------|
| **Level 1** | 断网 > 60 秒 | 软重拨 | 停止并重启 Data Call |
| **Level 2** | 断网 > 5 分钟 | 射频重置 | CFUN 开关（带调用限制） |
| **Level 3** | 断网 > 30 分钟 | 模块重启 | 可选，当前注释 |

### 时间阈值

```cpp
const uint64_t LEVEL1_TIMEOUT = 60 * 1000;       // 1分钟
const uint64_t LEVEL2_TIMEOUT = 5 * 60 * 1000;   // 5分钟
const uint64_t LEVEL3_TIMEOUT = 30 * 60 * 1000;  // 30分钟
```

### 防频繁触发机制

- **Level 1**: 两次恢复动作间隔至少 60 秒
- **Level 2**: 两次恢复动作间隔至少 5 分钟
- **Level 3**: 当前未启用

### Level 1: 软重拨（Soft Redial）

```cpp
case 1:
    dial_log("[RECOVERY L1] Stopping Data Call & Restarting...\n");
    ql_data_call_stop(g_call_id);
    sleep(2);
    ql_data_call_start(g_call_id);
    sleep(3);
    break;
```

**适用场景**: 网络短暂中断，可能是数据连接异常

### Level 2: 射频重置（Radio Reset）

```cpp
case 2:
    dial_log("[RECOVERY L2] Toggling RF (Airplane Mode)...\n");
    restart_cfun_safe();  // 带调用限制的 CFUN 重启
    sleep(10);  // 等待网络注册
    ql_data_call_start(g_call_id);
    break;
```

**适用场景**: 网络长时间中断，可能是射频模块异常

**注意事项**: 
- 使用 `restart_cfun_safe()` 确保不超过调用限制
- 最大调用次数：10 次
- 最小调用间隔：600 秒（10 分钟）

### Level 3: 模块重启（Hard Reboot）

```cpp
case 3:
    dial_log("[RECOVERY L3] FATAL: Network down for 30mins. But Not REBOOTING MODULE...\n");
    sync();  // 同步文件系统
    sleep(5);
    // system("serial_atcmd at+cfun=1,1");  // 可选，当前注释
    break;
```

**适用场景**: 网络长时间中断，可能是模块硬件故障

**当前状态**: 已注释，可根据需要启用

### 恢复策略开关

```cpp
int enable_policy_recovery = 1;  // 0=关闭，1=开启
```

可通过修改此变量控制是否启用分级恢复策略。

---

## 快速失败重试机制

### 设计目的

避免程序在启动阶段长时间卡死，提高恢复速度。

### 工作机制

1. **启动计数**
   - 读取 `/tmp/dial_retry_count` 获取尝试次数
   - 如果次数 < 3，进入快速失败模式
   - 如果次数 >= 3，进入持久循环模式

2. **快速失败模式**
   - 10 秒内未建立连接，立即退出程序
   - 退出码：1
   - 保活脚本会自动重新拉起程序
   - 计数器自动 +1

3. **持久循环模式**
   - 正常运行，不进行快速退出
   - 持续尝试建立连接

4. **成功清除**
   - 连接成功后，自动删除 `/tmp/dial_retry_count` 文件
   - 下次意外断网重启时，重新从第一次开始计数

### 配置参数

```cpp
#define MAX_FAST_RETRY_TIMES 3        // 最大快速重试次数
#define FAST_FAIL_TIMEOUT_MS 10000    // 快速失败超时（10秒）
#define RETRY_COUNT_FILE "/tmp/dial_retry_count"  // 计数文件路径
```

### 日志输出

```
[INIT] Fast Retry Mode: Attempt 1/3
[INIT] Fast Retry Mode: Attempt 2/3
[INIT] Fast Retry Mode: Attempt 3/3
[INIT] Fast Retry Limit Reached. Entering persistent loop mode.
[INIT] Fast Fail: Timeout 10000ms without connection. Exiting to retry...
[INIT] Dial success. Retry count cleared.
```

---

## CFUN 调用限制机制

### 设计目的

防止频繁调用 `AT+CFUN`（射频开关）导致模块异常或损坏。

### 限制规则

- **最大调用次数**: 10 次
- **最小调用间隔**: 600 秒（10 分钟）
- **时间基准**: 系统启动时间（`/proc/uptime`）

### 存储文件

- `/tmp/cfun_count.txt` - 记录调用次数
- `/tmp/cfun_last_call.txt` - 记录上次调用时间（系统启动后的秒数）

### 实现逻辑

```cpp
void EC200ADialer::restart_cfun_safe() {
    // 1. 检查调用次数
    int count = read_cfun_count();
    if (count >= MAX_CFUN_CALLS) {
        dial_log("CFUN 调用次数已达到上限，无法继续调用\n");
        return;
    }
    
    // 2. 检查调用间隔
    double current_time = get_system_uptime();
    double last_call_time = read_last_call_time();
    if (current_time - last_call_time < MIN_CFUN_INTERVAL) {
        dial_log("CFUN 调用间隔不足 600 秒，无法继续调用\n");
        return;
    }
    
    // 3. 执行 CFUN 重启
    Ql_SendAT("AT+CFUN=0");
    sleep(5);
    Ql_SendAT("AT+CFUN=1");
    
    // 4. 更新计数和时间
    write_cfun_count(count + 1);
    write_last_call_time(current_time);
}
```

### 配置参数

```cpp
#define MAX_CFUN_CALLS    10      // 最大允许调用次数
#define MIN_CFUN_INTERVAL 600    // 最小调用间隔（秒）
```

---

## 心跳日志机制

### 设计目的

定期输出诊断信息，便于离线分析和问题排查。

### 输出频率

- **间隔**: 30 秒（`HEARTBEAT_INTERVAL_MS = 30000`）
- **输出位置**: 控制台 + SD 卡日志文件

### 输出内容

```
[HEARTBEAT] SIM:READY | REG:1 | CSQ:25 | DownTime:0s
```

**字段说明：**
- **SIM**: SIM 卡状态（READY/NOT INSERTED/ERROR/UNKNOWN）
- **REG**: 网络注册状态（0-5）
  - 0 = 未注册
  - 1 = 已注册（本地网络）
  - 2 = 搜索中
  - 3 = 注册被拒绝（会额外输出警告）
  - 4 = 未知
  - 5 = 已注册（漫游）
- **CSQ**: 信号强度（0-31，99=未知）
- **DownTime**: 断网时长（秒），0 表示正常

### 特殊警告

当注册状态为 3（注册被拒绝）时，会额外输出：

```
[WARNING] Registration Denied! Code 3.
```

---

## 网络配置

### 状态回调函数

当 Data Call 状态改变时，SDK 会调用 `data_call_status_ind_cb` 回调函数。

#### 连接成功时（`QL_NET_DATA_CALL_STATUS_CONNECTED`）

1. **更新状态文件**
   ```bash
   echo 0 > /tmp/dial_Status
   echo 1 > /tmp/network_status
   ```

2. **配置系统路由**
   ```bash
   ip ro add default via <gateway> dev <device>
   ```

3. **配置 NAT**
   ```bash
   iptables -t filter -F
   iptables -t nat -A POSTROUTING -o <device> -j MASQUERADE
   ```

4. **配置 DNS**
   - 写入 `/tmp/resolv_v4.conf`（IPv4 DNS）
   - 写入 `/tmp/resolv_v6.conf`（IPv6 DNS，如果存在）
   - 合并到 `/etc/resolv.conf`

#### 断开连接时（`QL_NET_DATA_CALL_STATUS_DISCONNECTED`）

```bash
echo 1 > /tmp/dial_Status
echo 0 > /tmp/network_status
```

### 网络接口

- **默认接口名**: `ccinet0`（由 SDK 分配）
- **接口检测**: 启动前等待接口就绪（最多 30 秒）

---

## 状态监控

### 全局状态变量

```cpp
volatile int g_last_call_status = QL_NET_DATA_CALL_STATUS_NONE;
```

由 `data_call_status_ind_cb` 回调函数更新，在主循环中读取。

### 状态值定义

- `QL_NET_DATA_CALL_STATUS_NONE` - 无状态
- `QL_NET_DATA_CALL_STATUS_CREATED` - 已创建
- `QL_NET_DATA_CALL_STATUS_IDLE` - 空闲
- `QL_NET_DATA_CALL_STATUS_CONNECTING` - 连接中
- `QL_NET_DATA_CALL_STATUS_CONNECTED` - 已连接
- `QL_NET_DATA_CALL_STATUS_DISCONNECTED` - 已断开
- `QL_NET_DATA_CALL_STATUS_ERROR` - 错误

### LED 状态指示

| LED 状态 | 数值 | 含义 |
|----------|------|------|
| 灭灯 | 0 | 网络未连接或 Ping 失败 |
| 闪灯 | 1 | 拨号中或恢复中 |
| 亮灯 | 2 | 网络已连接且 Ping 成功 |

---

## 日志系统

### SD 卡日志

- **日志目录**: `/media/sdcard/dial_log/`
- **文件命名**: `dial_YYYYMMDD_HHMMSS.log`
- **日志格式**: `[YYYY-MM-DD HH:MM:SS] 日志内容`
- **存储要求**: 
  - SD 卡需挂载到 `/media/sdcard`
  - 剩余空间需大于 500MB

### 日志级别

所有关键操作都使用 `dial_log()` 记录，包括：
- 启动和退出
- 拨号状态变化
- 故障恢复动作
- 心跳诊断信息
- 错误和警告

### 日志示例

```
[2025-01-28 10:30:00] Program started. Version: 1.30
[2025-01-28 10:30:01] [NetCheck] Interface 'ccinet0' is ready!
[2025-01-28 10:30:02] Starting dial initialization...
[2025-01-28 10:30:05] Ping 8.8.8.8 OK
[2025-01-28 10:30:35] [HEARTBEAT] SIM:READY | REG:1 | CSQ:25 | DownTime:0s
[2025-01-28 10:35:00] [ALARM] Net Fail Duration: 300 sec. Trigger Level 2 recovery.
[2025-01-28 10:35:01] [RECOVERY L2] Toggling RF (Airplane Mode)...
```

---

## 配置参数总结

### 时间参数

| 参数 | 值 | 说明 |
|------|-----|------|
| `PING_INTERVAL_MS` | 1500ms | Ping 检查间隔 |
| `SIM_INTERVAL_MS` | 800ms | SIM 检查间隔 |
| `SIG_INTERVAL_MS` | 800ms | 信号检查间隔 |
| `HEARTBEAT_INTERVAL_MS` | 30000ms | 心跳日志间隔 |
| `FAST_FAIL_TIMEOUT_MS` | 10000ms | 快速失败超时 |
| `LEVEL1_TIMEOUT` | 60秒 | Level 1 恢复阈值 |
| `LEVEL2_TIMEOUT` | 5分钟 | Level 2 恢复阈值 |
| `LEVEL3_TIMEOUT` | 30分钟 | Level 3 恢复阈值 |
| `MIN_CFUN_INTERVAL` | 600秒 | CFUN 最小调用间隔 |
| `reconnect_interval` | 35秒 | SDK 自动重连间隔 |

### 计数参数

| 参数 | 值 | 说明 |
|------|-----|------|
| `MAX_FAST_RETRY_TIMES` | 3 | 最大快速重试次数 |
| `MAX_CFUN_CALLS` | 10 | CFUN 最大调用次数 |

### 文件路径

| 文件 | 路径 | 说明 |
|------|------|------|
| 日志目录 | `/media/sdcard/dial_log/` | SD 卡日志目录 |
| 重试计数 | `/tmp/dial_retry_count` | 快速重试计数 |
| CFUN 计数 | `/tmp/cfun_count.txt` | CFUN 调用次数 |
| CFUN 时间 | `/tmp/cfun_last_call.txt` | CFUN 上次调用时间 |
| 拨号状态 | `/tmp/dial_Status` | 拨号状态（0=成功，1=失败） |
| 网络状态 | `/tmp/network_status` | 网络状态（0=断开，1=连接） |

---

## 故障排查指南

### 常见问题

1. **无法建立连接**
   - 检查 SIM 卡状态（心跳日志中的 SIM 字段）
   - 检查网络注册状态（心跳日志中的 REG 字段）
   - 检查信号强度（心跳日志中的 CSQ 字段）
   - 查看 SD 卡日志文件

2. **频繁触发恢复**
   - 检查网络环境是否稳定
   - 检查 SIM 卡是否正常
   - 查看恢复日志，确认触发的等级

3. **CFUN 调用被拒绝**
   - 检查 `/tmp/cfun_count.txt` 是否已达到上限
   - 检查 `/tmp/cfun_last_call.txt` 距离当前时间是否不足 600 秒

4. **日志文件未生成**
   - 检查 SD 卡是否挂载到 `/media/sdcard`
   - 检查 SD 卡剩余空间是否大于 500MB
   - 检查日志目录权限

### 调试技巧

1. **查看实时日志**
   ```bash
   tail -f /media/sdcard/dial_log/dial_*.log
   ```

2. **检查状态文件**
   ```bash
   cat /tmp/dial_Status
   cat /tmp/network_status
   ```

3. **检查计数文件**
   ```bash
   cat /tmp/dial_retry_count
   cat /tmp/cfun_count.txt
   cat /tmp/cfun_last_call.txt
   ```

4. **手动测试 Ping**
   ```bash
   ping -c 1 -W 2 8.8.8.8
   ```

---

## 版本历史

- **1.30** - 集成 open_dial 优秀机制，新增分级恢复、快速重试、CFUN 限制等功能
- **1.25** - 添加日志系统和心跳日志
- **1.24** - 基础版本

---

## 参考资料

- Quectel EC200A SDK 文档
- open_dial 项目源码
- CHANGELOG.md - 详细更新日志

---

**文档维护**: 请随代码更新及时更新本文档
