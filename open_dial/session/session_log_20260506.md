# 2026-05-06 设备日志分析 + 代码修复记录

> **日志文件**：`dial_20260506_093031.log`  
> **设备**：EC200A-CNTA（OpenCPU，eSIM）  
> **固件**：EC200ACNTAR02A04M2G_OCPU（SubEdition: V02）  
> **程序版本**：open_dial V1.26  
> **日志时长**：09:30:31 ~ 15:52:52（约 6h22min，共 996 行）  
> **设备基本信息**：  
> - ICCID：89860820182590217623  
> - IMEI：860108077830099 / IMSI：460089065306613  
> - 运营商：中国移动物联网（+COPS: 0,2,"46000",7，LTE）

---

## 一、启动流程分析（符合预期）

```
09:30:31  程序启动，V1.26
09:30:32  [INIT] Fast Retry Mode: Attempt 1/3
09:30:32  data_call_init OK（无重试，立即成功）
09:30:34  PDP connected，开启 10s Ping 窗口
09:30:35  Ping 成功，retry count 清零，进入持久模式
09:30:35  [EVENT] Network Connected
```

Fast Retry 1/3 次成功，后续全程持久模式，启动链路完全正常。

---

## 二、核心发现：[DIAG] 触发机制（经源码验证）

### 2.1 源码中有两套完全不同的 DIAG 标签

分析日志时发现大量 `[DIAG]` 事件但无任何 `[ERROR] SIM Card disconnected`，通过阅读 `dial.c` 源码确认：

| 标签 | 触发位置 | 触发条件 |
|------|---------|---------|
| `[DIAG]` | `ST_PING`（dial.c:1626） | Ping 连续失败 3 次，且本故障周期首次触发 |
| `[ERROR] SIM Card disconnected, CPIN=X` | `ST_SIM`（dial.c:1497） | AT+CPIN? 返回非 READY |
| `[DIAG-SIM]` | `ST_SIM`（dial.c:1505） | SIM 断开时同步抓 QSIMSTAT+CEREG |
| `[DIAG-DMESG]` | `ST_SIM`（dial.c:1512） | SIM 断开时抓 dmesg 内核日志 |
| `[DIAG-SIM-REC]` | `ST_SIM`（dial.c:1532） | SIM 恢复时重抓 QSIMSTAT |
| `[DIAG-DMESG-REC]` | `ST_SIM`（dial.c:1538） | SIM 恢复时重抓 dmesg |

**关键代码（Ping 失败触发 [DIAG]）：**

```c
// dial.c:1605-1626
if (start_fail_ts == 0) {           // 本故障周期首次到达
    start_fail_ts = tnow;
    if (!diag_snap_done) {
        diag_snap_done = 1;         // 每周期只触发一次
        // ... 采集 CESQ/CREG/CGPADDR/CEER/CGACT ...
        if (diag[0]) dial_log("[DIAG] %s\n", diag);
    }
}
// diag_snap_done 仅在 Ping 成功时重置（dial.c:1577）
// 因此每次 [DIAG] = 一个独立 Ping 失败故障周期的开始
```

### 2.2 本次日志 DIAG 标签统计

```
[DIAG]          30+ 次   ← Ping 连续失败触发（网络层）
[DIAG-SIM]        0 次
[DIAG-DMESG]      0 次
[DIAG-SIM-REC]    0 次
[ERROR] SIM断卡   0 次
```

**结论：SIM 本身从未断开，所有 [DIAG] 均由 Ping 失败（网络层）触发。**

---

## 三、断网根因：LTE 小区切换（Handover）

### 3.1 证据

- 全程 `SIM:READY`、`REG:1`，AT+CPIN? 从未返回非 READY
- CID（小区ID）全天高频变化（每 5~30min 切换一次，下午更频繁）
- 每次 [DIAG] 后 30s 内 DownTime 归零，为独立短暂故障周期
- 设备处于移动场景（车辆），Handover 期间数据路径短暂中断

### 3.2 断网时序（典型）

```
[DIAG] 触发（第3次Ping失败，start_fail_ts 开始计时）
→ DownTime: 2~9s（Ping 失败持续）
→ DownTime: 0s（Ping 恢复，故障周期结束，diag_snap_done 重置）
```

实际每次完整断网时长 ≈ 3×1.5s（达阈值前）+ DownTime 值 ≈ **6~15s**。

### 3.3 下午集中爆发与唯一 L1 事件

下午 14:09 起，小区切换频率明显加快，DownTime 振荡且逐渐增大：

```
14:15:59  DownTime:21s → 短暂恢复 → start_fail_ts 重置
14:24:38  DownTime:21s → 短暂恢复 → 重置
14:26:39  DownTime:36s → 短暂恢复 → 重置（最长单次，36s < 60s，未触 L1）
14:29:40  DownTime:25s → 短暂恢复 → 重置
14:34:15  DownTime:47s → ...
14:34:30  [ALARM] Net Fail Duration: 60s → L1 触发
14:34:31  [RECOVERY L1] Stopping Data Call... REG=1
14:34:34  [RECOVERY L1] Restarting Data Call...
14:34:40  [EVENT] Network Connected（L1 耗时 9s）
```

L1 之后完全稳定，后续 1.5h 无任何告警。

### 3.4 DownTime 振荡现象说明

每次 Ping 短暂成功就会 `start_fail_ts = 0` 重置，使得 L1 迟迟无法触发，直到某次单独超过 60s 才动作。这是现有"单次计时"设计的边界场景，详见第五节。

---

## 四、与历史设备对比

| 维度 | 本次 0506 设备 | 0428/0504 设备 |
|------|--------------|--------------|
| SIM 状态 | 全程 READY | 反复 UNKNOWN（约 22~30s） |
| 断网根因 | LTE 小区切换（Handover） | eSIM OTA SIM REFRESH |
| 单次断网时长 | 6~15s | 62~72s |
| L1 触发次数 | 1 次（6h 内） | 每次 SIM 断卡必触发 |
| `[ERROR] SIM断卡` | 无 | 有（每次） |
| `[DIAG-SIM]` 标签 | 无 | 有（每次） |
| 问题性质 | 网络层，正常移动场景现象 | SIM 层，eSIM 平台 OTA 操作 |

---

## 五、代码修复

### 5.1 AT+CGMR → AT+QGMR（Fix）

**现象**：日志显示 `[INIT] FW:    XX-YY-ZZ`，经设备验证：

```bash
serial_atcmd at+cgmr   → XX-YY-ZZ   （该固件 AT+CGMR 返回占位符，非代码问题）
serial_atcmd at+qgmr   → EC200ACNTAR02A04M2G_OCPU  （真实版本）
serial_atcmd at+cgmm   → EC200A-CNTA
```

**修复**（`dial.c:get_cgmr_safe()`）：将 popen 命令从 `at+cgmr` 改为 `at+qgmr`，解析逻辑不变（响应格式相同）。

```c
// 修复前
FILE *fp = popen("serial_atcmd at+cgmr", "r");
// 修复后
FILE *fp = popen("serial_atcmd at+qgmr", "r");
```

修复后日志：`[INIT] FW:    EC200ACNTAR02A04M2G_OCPU`

### 5.2 去除 SUB 字段冗余前缀（Fix）

**现象**：`[INIT] SUB:   SubEdition: V02`，"SubEdition:" 与日志标签 `SUB:` 重复。

**修复**（`dial.c:get_csub_safe()`）：解析到 "SubEdition:" 后跳过前缀 11 个字符。

```c
char *p = strstr(line, "SubEdition:");
if (p) {
    p += 11;   /* 跳过 "SubEdition:" 前缀，只保留版本号 */
    strncpy(out, p, len - 1);
```

修复后日志：`[INIT] SUB:   V02`

---

## 六、未处理的潜在改进项（不紧迫）

### 累计断网时长（非单次计时）

**背景**：当前 `start_fail_ts` 在每次 Ping 成功时清零，导致高频短断（每次 < 60s）反复重置计时器，L1 迟迟不触发。本次日志中，14:09~14:34 长达 25 分钟内用户实际体验反复断网，但直到 14:34:30 才有一次单次超 60s 触发 L1。

**思路**：在现有机制基础上增加滑动窗口累计断网时长，例如"过去 5 分钟内累计断网超 45s 则触发 L1"，对移动高频切换场景更敏感。

**结论**：暂不实施。L1 最终能兜底，且 Handover 期间主动 stop/start 有时会干扰切换过程，效果存疑。待更多设备日志积累后评估。

---

## 七、结论

| 结论项 | 内容 |
|-------|------|
| 程序运行状态 | 正常，无 Bug |
| 断网根因 | LTE 小区切换（移动场景），非 SIM 层故障 |
| L1 恢复 | 1 次，正确触发，9s 恢复，后续稳定 |
| 代码修复 | 2 处：FW 版本命令、SUB 前缀 |
| 与历史设备关系 | 不同根因，本次无 eSIM OTA 问题 |
| 下一步 | 无紧急事项；如业务对断网敏感可评估累计计时方案 |
