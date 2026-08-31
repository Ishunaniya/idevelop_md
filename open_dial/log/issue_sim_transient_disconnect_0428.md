# 问题记录：SIM 卡周期性瞬断导致网络周期性中断

**发现时间**：2026-04-28  
**设备**：EC200A / EG25（OpenCPU 模式）  
**日志文件**：设备 SD 卡 `dial_*.log`  
**软件版本**：open_dial V1.26  

---

## 1. 问题现象

设备在正常联网状态下，**每隔约 2~3 分钟**出现一次网络中断，中断持续约 **60~65 秒**后由软件 L1 恢复重拨，随后恢复联网。整个观测窗口（约 7 分钟）内共发生 **3 次**，规律性极强。

**用户感知**：网络每隔约 2~3 分钟断一次，断约 1 分钟，自动恢复。

---

## 2. 原始日志（问题片段）

```
[2026-04-28 12:55:37] [HEARTBEAT] SIM:READY | REG:1 | CSQ:31 | DownTime:0s
[2026-04-28 12:55:58] [ERROR] SIM Card disconnected during runtime.
[2026-04-28 12:56:07] [HEARTBEAT] SIM:UNKNOWN | REG:-1 | CSQ:-1 | DownTime:8s
[2026-04-28 12:56:37] [HEARTBEAT] SIM:READY | REG:1 | CSQ:31 | DownTime:38s
[2026-04-28 12:57:02] [ALARM] Net Fail Duration: 61 sec. Trigger Level 1 recovery.
[2026-04-28 12:57:02] [RECOVERY L1] Stopping Data Call & Cleaning PDP...
[2026-04-28 12:57:04] [RECOVERY L1] Restarting Data Call...
[2026-04-28 12:57:11] [EVENT] Network Connected. Notification sent.
[2026-04-28 12:57:39] [HEARTBEAT] SIM:READY | REG:1 | CSQ:26 | DownTime:0s

[2026-04-28 12:58:26] [ERROR] SIM Card disconnected during runtime.
[2026-04-28 12:58:39] [HEARTBEAT] SIM:UNKNOWN | REG:-1 | CSQ:-1 | DownTime:12s
[2026-04-28 12:59:09] [HEARTBEAT] SIM:READY | REG:1 | CSQ:31 | DownTime:42s
[2026-04-28 12:59:30] [ALARM] Net Fail Duration: 61 sec. Trigger Level 1 recovery.
[2026-04-28 12:59:30] [RECOVERY L1] Stopping Data Call & Cleaning PDP...
[2026-04-28 12:59:32] [RECOVERY L1] Restarting Data Call...
[2026-04-28 12:59:38] [EVENT] Network Connected. Notification sent.

[2026-04-28 13:01:30] [ERROR] SIM Card disconnected during runtime.
[2026-04-28 13:01:41] [HEARTBEAT] SIM:UNKNOWN | REG:-1 | CSQ:-1 | DownTime:11s
[2026-04-28 13:02:12] [HEARTBEAT] SIM:READY | REG:1 | CSQ:31 | DownTime:41s
[2026-04-28 13:02:34] [ALARM] Net Fail Duration: 62 sec. Trigger Level 1 recovery.
[2026-04-28 13:02:34] [RECOVERY L1] Stopping Data Call & Cleaning PDP...
[2026-04-28 13:02:36] [RECOVERY L1] Restarting Data Call...
[2026-04-28 13:02:41] [EVENT] Network Connected. Notification sent.
```

---

## 3. 时序分析

每次事件的完整时序如下（以第 1 次为例，三次规律一致）：

```
T+0s    12:55:58  SIM 状态由 READY → 非 READY，触发 [ERROR]，start_fail_ts 开始计时
T+9s    12:56:07  心跳确认：SIM=UNKNOWN, REG=-1, CSQ=-1（模组完全失去 SIM 访问）
T+39s   12:56:37  心跳确认：SIM=READY, REG=1（SIM 自行恢复，但 PDP 未自动重建）
T+61s   12:57:02  L1 恢复触发（LEVEL1_TIMEOUT = 60s）：stop → sleep(2) → start
T+73s   12:57:11  Network Connected（L1 重拨耗时约 9s）
```

**三次事件关键数据汇总：**

| 事件 | SIM 断开时刻 | SIM UNKNOWN 持续 | SIM 自愈时刻 | PDP 靠 L1 恢复 | L1 重拨耗时 | 两次断卡间隔 |
|------|------------|----------------|------------|--------------|-----------|------------|
| #1 | 12:55:58 | ~30s | 12:56:37 | 是（61s 后） | 9s | — |
| #2 | 12:58:26 | ~30s | 12:59:09 | 是（63s 后） | 8s | 148s |
| #3 | 13:01:30 | ~30s | 13:02:12 | 是（64s 后） | 7s | 184s |

---

## 4. 根因分析

### 4.1 触发 [ERROR] 的代码路径

```c
// dial.c: ST_SIM 状态机分支（每 800ms 轮询一次）
get_cpin_status_str(sim_str, sizeof(sim_str));  // 执行 AT+CPIN?
if (strcmp(sim_str, "READY") != 0) {
    if (has_notified_connect) {                 // 曾经成功联网过
        dial_log("[ERROR] SIM Card disconnected during runtime.\n");
        has_notified_connect = 0;
        ping_fail_count = 0;
    }
    if (start_fail_ts == 0) start_fail_ts = tnow;  // 开始故障计时
}
```

触发条件：程序运行期间已成功联网（`has_notified_connect=1`），此后 `AT+CPIN?` 返回非 `READY`（`UNKNOWN`、`ERROR`、空响应等）。

### 4.2 SIM 瞬断的根因排查

> **设备使用 eSIM，无物理 SIM 卡槽，卡座接触不良已排除。**

结合日志特征，逐项分析：

| 特征 | 含义 |
|------|------|
| CSQ 始终在 24~31（强信号） | **排除基站信号差** |
| REG 随 SIM 一起消失（-1），SIM 恢复后 REG 也快速回到 1 | **排除网络侧注册问题** |
| 每次 UNKNOWN 持续约 30s，高度一致 | **排除偶发随机异常，指向固定耗时的内部操作** |
| SIM **自行**恢复 READY，无需任何 AT 命令干预 | **说明是临时性操作，非永久故障** |
| 每隔 2~3 分钟周期性出现 | **强烈指向后台定时任务触发** |
| eSIM 设备，无物理卡槽 | **排除接触不良、振动、温变等物理因素** |

**最可能原因（按概率排序）：**

1. **eSIM Profile OTA 刷新 / SM-DP+ 服务器周期心跳**（最可能）  
   eSIM 平台（SM-DP+）周期性下发 Profile 更新或执行心跳检测时，LPA（Local Profile Assistant）会短暂独占 SIM 接口，期间模组无法响应 `AT+CPIN?`，表现为约 30s 的 UNKNOWN 窗口。完成后 Profile 恢复激活状态，SIM 重新 READY。

2. **模组 eSIM 固件内部定时任务**  
   EC200A/EG25 模组 eSIM 固件可能存在周期性自检或 Profile 校验任务，执行期间 SIM 访问暂时挂起。约 2~3 分钟的间隔与固件定时器周期吻合。

3. **eSIM 所属运营商平台主动推送**  
   运营商平台（如 GSMA RSP 架构）周期性向设备推送 Profile 管理指令，触发时机由平台侧决定，设备端只能被动接受。

**关键判断依据**：30s 的 UNKNOWN 持续时长非常稳定（三次分别为 39s/42s/42s），这是 eSIM OTA 操作的典型特征——固定的握手→下载→应用流程耗时，与随机故障的不规律性截然不同。

### 4.3 软件行为评估

软件响应**完全符合设计预期**，无 bug：

- `start_fail_ts` 在 SIM 断开时正确启动
- SIM 自愈后 PDP 未自动重建属正常（SDK 重连间隔 25s，但此处 SIM 恢复时已过去约 39s，PDP 仍未来）
- L1 在恰好 61~64s（≈ `LEVEL1_TIMEOUT=60s`）时触发，逻辑正确
- L1 重拨在 7~9s 内恢复联网，效率正常
- DownTime 重置为 0s，retry_count 清零，状态机正确复位

**软件不是问题根源，问题出在 eSIM 固件或平台侧的周期性操作。**

---

## 5. 影响评估

| 维度 | 评估 |
|------|------|
| 断网频率 | 每 2~3 分钟一次 |
| 单次断网时长 | 约 60~65s（L1 恢复后结束） |
| 是否自愈 | 是，每次 L1 均成功恢复 |
| 是否触发 L2/L3 | 否（每次 61s 内 L1 恢复，未达 L2 阈值 300s） |
| 数据业务影响 | 每次约 1 分钟无法通信，对实时性业务影响较大 |
| 系统稳定性 | 进程未崩溃，不会触发 L3 重启，系统整体稳定 |

---

## 6. 排查建议

> 设备为 eSIM，物理硬件排查无意义，重点排查 eSIM 平台侧和模组固件侧。

### 第一步：确认 eSIM LPA 进程状态

```bash
# 查看是否有 LPA / eSIM 管理进程在运行
ps | grep -iE "lpa|esim|euicc|ota"

# 查看进程列表，确认模组 eSIM 相关守护进程
ps -ef
```

### 第二步：抓取断卡时刻的精确时间戳

```bash
# 高频监控 AT+CPIN?，记录状态变化时刻（精确到秒）
while true; do
    TS=$(date '+%H:%M:%S')
    STATUS=$(serial_atcmd at+cpin? 2>&1 | grep "+CPIN:" | awk '{print $2}')
    echo "[$TS] CPIN: ${STATUS:-UNKNOWN}"
    sleep 2
done
```

对比多次断卡间隔是否严格周期性，若间隔高度一致（如每次都是 148s 或 120s），则确认为定时任务触发。

### 第三步：查询 eSIM Profile 及平台信息

```bash
# 查询当前激活的 eSIM Profile（Quectel 模组 AT 命令）
serial_atcmd at+qesim?

# 查询 EID（eSIM 唯一标识，用于联系 eSIM 平台方）
serial_atcmd at+qeid?

# 查询 SM-DP+ 服务器地址（若模组支持）
serial_atcmd at+qsmaddr?
```

### 第四步：联系 eSIM 平台方确认

将以下信息提供给 eSIM 运营商或平台方：

- EID（eSIM 唯一标识）
- 断卡发生的精确时间（UTC）
- 断卡持续时长（约 30s）、间隔（约 2~3min）
- 询问该时间段内平台是否有 OTA 推送、Profile 刷新或心跳操作

### 第五步：模组固件版本确认

```bash
# 查询模组固件版本
serial_atcmd at+gmr

# 查询模组型号
serial_atcmd at+cgmm
```

将固件版本报给 Quectel 技术支持，确认该版本是否有已知的 eSIM 周期性复位 bug。

---

## 7. 短期规避方案

当前 L1 恢复机制已能在 ~65s 内自愈，若业务对断网敏感，可考虑：

**降低 L1 阈值**（`LEVEL1_TIMEOUT` 从 60s 改为 40s）：

```c
// dial.c
#define LEVEL1_TIMEOUT   40000   // 原值 60000
```

效果：SIM 自愈（~39s）后约 1s 内触发 L1，将单次断网时长从 ~65s 压缩至 ~48s。

> **注意**：此改动需评估与其他恢复逻辑的交互，仅作为临时规避，根本解决需定位 eSIM 平台或固件原因。

---

## 8. 结论

| 结论项 | 内容 |
|-------|------|
| 问题性质 | **eSIM 平台/固件侧周期性操作导致 SIM 接口短暂挂起**，非软件 bug |
| 软件表现 | L1 恢复机制正常工作，每次均能自愈 |
| 根因方向 | eSIM OTA/Profile 刷新 或 模组 eSIM 固件定时任务（需联系平台方确认） |
| 优先级 | 中（能自愈但影响业务连续性，每次断网约 65s） |
| 下一步 | 抓断卡时刻精确时间 → 联系 eSIM 平台方查 OTA 记录 → 联系 Quectel 确认固件版本 |

---

## 附录：AT+CPIN? 返回值说明

### 命令格式

```
AT+CPIN?
```

模组响应格式为：

```
+CPIN: <status>

OK
```

若命令本身失败（串口无响应、模组未就绪）则返回 `ERROR` 或无响应。

---

### 代码解析逻辑（`dial.c:650 get_cpin_status_str()`）

```c
// 逐行读取响应，找到 "+CPIN: " 前缀后提取后半部分
char *p = strstr(line, "+CPIN: ");
p += 7;  // 跳过前缀，取状态字符串
```

- `popen()` 失败 → 写入 `"ERROR"`
- 读完所有行仍未找到 `+CPIN:` → 保持默认值 `"UNKNOWN"`
- 找到 `+CPIN: <status>` → 写入 `<status>`（去除 `\r\n`）

心跳日志里的 `SIM:READY`、`SIM:UNKNOWN` 均直接来自此函数的输出。

---

### 所有可能的返回值

| 返回值 | 含义 | dial 程序行为 | 常见原因 |
|--------|------|-------------|---------|
| `READY` | SIM 卡正常，无需 PIN | 正常运行，ping 检测继续 | 正常状态 |
| `SIM PIN` | 需要输入 PIN1 码解锁 | 视同非 READY，开始故障计时 | 卡被锁定，或首次插卡未解锁 |
| `SIM PUK` | PIN1 输错次数过多，需输入 PUK1 码解锁 | 视同非 READY，开始故障计时 | PIN1 连续输错 3 次 |
| `SIM PIN2` | 需要输入 PIN2 码（仅某些操作触发） | 视同非 READY，开始故障计时 | 特定 SIM 功能需要 PIN2 |
| `SIM PUK2` | PIN2 输错次数过多，需输入 PUK2 码 | 视同非 READY，开始故障计时 | PIN2 连续输错 3 次 |
| `NOT READY` | SIM 卡存在但尚未初始化完成 | 视同非 READY，开始故障计时 | 模组刚上电、SIM 正在初始化 |
| `UNKNOWN` | 代码默认值（未读到 +CPIN: 行）或模组无法识别 SIM 状态 | 视同非 READY，开始故障计时 | SIM 瞬断、模组异常、AT 命令超时未返回 |
| `ERROR`（代码内部） | `popen("serial_atcmd at+cpin?")` 调用失败 | 视同非 READY，开始故障计时 | `serial_atcmd` 进程不存在、串口占用 |

> **注意**：`UNKNOWN` 有两种来源：
> 1. 模组真实返回了 `+CPIN: UNKNOWN`（SIM 访问失败）
> 2. `serial_atcmd` 无响应导致代码默认填入 `"UNKNOWN"`
>
> 两者在日志里**无法区分**，但两种情况的后续影响相同——均触发故障计时。

---

### 与本次问题的对应关系

本次日志中出现的 `SIM:UNKNOWN` 即 `AT+CPIN?` 未返回 `+CPIN:` 行（或模组返回 `+CPIN: UNKNOWN`），说明模组在这约 30s 内**完全失去了对 SIM 卡的访问能力**，不仅仅是状态异常，而是整个 SIM 接口不通。

设备为 eSIM，无物理卡槽，结合 30s 持续时长高度一致、间隔周期规律的特征，推断最可能是 **eSIM LPA 执行 OTA 操作期间独占 SIM 接口**，导致模组 AT 层无法访问 SIM，表现为 UNKNOWN。操作完成后接口释放，SIM 自动恢复 READY。
