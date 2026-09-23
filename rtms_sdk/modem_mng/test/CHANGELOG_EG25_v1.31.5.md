# Changelog — modem_mng（EG25 平台）拨号程序

---

## v1.31.5

**平台**：EG25G（`QL_MODULE_PLATFORM=EG25G`）
**二进制**：`/usr/bin/modem_mng`（由 `sw_mng` 看门狗拉起）
**版本来源**：`eg25/dial/dial.h` 的 `MODEM_MNG_VERSION_*` 宏（1 / 31 / 5）

> 本文是开发交付的提测说明（changelog），只描述「行为是什么样、怎么配置」。
> 测试场景与用例由测试侧依据本文自行设计，不在此罗列。

> **本版（v1.31.5）相对 v1.31.4 的行为变更（仅影响策略4 强制SIM 的分级恢复）**：
> 1. **分级恢复改纯 downtime 驱动**：移除"SIM 恢复即立即触发 L1"（`sim_error_active`）逻辑。
>    修复现网长断网时复位走完误打一条 `[RECOVERY L1] SIM recovered ...` 的问题。
>    L1 现在只由断网时长 ∈ [60s,300s) 触发。
> 2. **策略4 的 CFUN 射频复位归 L2 独占**：已连通过之后，注册超时(reg_timeout)不再自己做
>    CFUN，让位给 L2 的 downtime 阶梯统一复位，消除"L2 与 reg_timeout 在同一段 reg_check
>    期各复位一次"的双 CFUN。冷启动(尚未连通)仍由 reg_timeout 复位兜底；策略L1/L2/L3 不受影响。
> 3. **`[INIT]` 启动信息只打印一次**：cfun 重初始化不再每 ~5min 重刷 ICCID/IMSI/IMEI/PDP/FW。

---

### 一、进程与职责

- `modem_mng` 由 `sw_mng` 看门狗拉起，是**唯一负责拨号与 Roamlink 通道管理的进程**。
- 通道切换、网络连通性检测、RBMaster 启停、证书下载**全部在 `modem_mng` 进程内完成**，不依赖任何外部脚本。
- 无固定等待窗口：主循环每秒推进状态机，每 **30 秒**打一次心跳（SIM/REG/CSQ/ping），每 **5 分钟**追加一次扩展心跳，所有切换判定由下文各计时器驱动。
  _日志_：状态机每次状态变化打 `[STATE] sim_check -> reg_check`；心跳打 `[HEARTBEAT] ...`；启动信息 `[INIT] ICCID/IMSI/IMEI/PDP/FW/policy=...`、`[MODEM] Model/FW/...` **仅在首次进入 sim_op 时打印一次**（运行期不变，cfun 重初始化不再重刷；对外 nano 发布的 ICCID/IMSI 仍每次刷新）。

---

### 二、四种网络策略（`/usrdata/network.ini`）

```ini
[network]
network_select = 1   # 1=优先Roamlink  2=优先SIM  3=强制Roamlink  4=强制SIM
```

| 值 | 名称 | 行为概述 | `/tmp/network_type` |
|---|---|---|---|
| 1 | PREFER_ROAMLINK | 优先 Roamlink，失败切物理 SIM 备用，SIM 稳定后主动回切 Roamlink | 切到哪写哪：2=Roamlink / 1=SIM |
| 2 | PREFER_SIM | 优先物理 SIM，失败切 Roamlink 备用，Roamlink 稳定后主动回切 SIM | 同上 |
| 3 | FORCE_ROAMLINK | 强制 Roamlink，失败只重启 Roamlink 服务，**永不切 SIM** | 2 |
| 4 | FORCE_SIM | 强制物理 SIM，失败走 L1/L2/L3 分级恢复，**永不切 Roamlink** | 1 |

- `network.ini` 出厂预置值为 `1`（优先 Roamlink）。
- 代码内 `NET_POLICY_DEFAULT = 4`，仅作为**文件读不到/非法时的兜底**（强制 SIM），与出厂值不冲突。
- `network.ini` 在**启动时读取一次**（`dial_mng_new`），运行中不重读；**修改后需重启 `modem_mng`**（由 `sw_mng` 重拉）才生效。
- `/tmp/network_type`：`1` = 物理 SIM，`2` = Roamlink，`0` = 未设置/切换中。

---

### 三、双卡切换状态机（核心行为逻辑）

所有切换判定基于以下计时器（单一事实来源在 `roamlink/roamlink.h`）：

| 宏 | 值 | 含义 |
|---|---|---|
| `ROAMLINK_CONNECT_TIMEOUT_SEC` | 300（5min） | Roamlink 发出 start 后，等待**首次 ping 通**的最长时间；以及通道激活后**断联多久**判失败 |
| `ROAMLINK_NO_DATA_TIMEOUT_SEC` | 120（2min） | Roamlink 通道虽连着，但业务层 `rmnet_data*` 收包数（rx_packets）多久无增长即判失败 |
| `ROAMLINK_SWITCH_TIMEOUT_SEC` | 180（3min） | 物理 SIM 侧 ping 失败累计多久后，切到 Roamlink（策略 1/2/3） |
| `REG_CHECK_TIMEOUT_SECONDS` | 300（5min） | 物理 SIM 注册（CEREG）超时多久后，按策略切 Roamlink 或 cfun 重置（`eg25/dial/dial.h`） |
| `SIM_FALLBACK_RETRY_SEC` | 300（5min） | 策略1：SIM 备用稳定多久后，主动尝试回切 Roamlink |
| `ROAMLINK_FALLBACK_RETRY_SEC` | 300（5min） | 策略2：Roamlink 备用稳定多久后，主动尝试回切 SIM |

#### 策略1（PREFER_ROAMLINK）完整流转 —— 测试侧最关注的场景

1. **开机**：直接启动 RBMaster 并激活 Roamlink 通道，进入 `roamlink_starting`。
   _日志_：`[ROAMLINK] Policy requires Roamlink as initial channel. Starting RBMaster...` → `[ROAMLINK] Initial channel: Roamlink (starting)`
2. **starting → active**：心跳 ping 通一次即转 `active`，并记录业务层 rx_packets 基准。
   _日志_：`[ROAMLINK] Ping OK in starting state, channel now active`
3. **starting 阶段连不上**：发出 start 后 **5 分钟（CONNECT_TIMEOUT）**仍 ping 不通 → 切回物理 SIM。
   _日志_：`[ROAMLINK] Connect timeout (300s), switching to SIM`
4. **active 阶段掉线**：上次 ping 成功后 **5 分钟（CONNECT_TIMEOUT）**未再 ping 通 → 切回物理 SIM。
   _日志_：`[ROAMLINK] Channel fail timeout (300s), switching to SIM`
5. **active 阶段无业务流量**：虽连着，但 rx_packets **2 分钟（NO_DATA_TIMEOUT）**无增长 → 切回物理 SIM。
   _日志_：`[ROAMLINK] Biz layer no data for 121s, switching to SIM`
6. **回切后不放弃**：切回 SIM 后持续计时，SIM 备用稳定 **300 秒（SIM_FALLBACK_RETRY）**后主动尝试回切 Roamlink；
   若回切又失败，重置计时，再等 300 秒继续尝试，循环直到 Roamlink 恢复。
   _日志_：`[ROAMLINK] Policy1: SIM stable 300s, trying Roamlink recovery` → 成功 `[ROAMLINK] Switched back to Roamlink channel (starting)` / 失败 `[ROAMLINK] Switch back failed, retry in 300s`
7. **SIM 侧注册失败**：物理 SIM 注册超过 **5 分钟（REG_CHECK_TIMEOUT）**未成功（如运营商禁卡），同样切 Roamlink。
   _日志_：`[REG TIMEOUT] policy=1, switching to Roamlink`

#### 策略2（PREFER_SIM）

- 优先物理 SIM 拨号。SIM 侧 ping 失败累计 **3 分钟（SWITCH_TIMEOUT）**或注册超时 5 分钟 → 切 Roamlink。
  _日志_：`[ROAMLINK] policy=2: SIM down 180s, switching to Roamlink` → `[ROAMLINK] Switched to Roamlink channel (starting)`
- Roamlink 备用稳定 **300 秒（ROAMLINK_FALLBACK_RETRY）**后主动回切 SIM；失败重置再等 300 秒，循环。
  _日志_：`[ROAMLINK] Policy2: Roamlink stable 300s, trying SIM recovery` → `[ROAMLINK] Switched back to SIM channel`
- Roamlink 通道自身的连通/无数据超时判定与策略1相同。

#### 策略3（FORCE_ROAMLINK）

- 始终走 Roamlink。任何超时（starting 连不上 / active 掉线 / 无数据）**只重启 Roamlink 服务**回到 `roamlink_starting`，**绝不切 SIM**。
  _日志_：`[ROAMLINK] FORCE_ROAMLINK: connect timeout, restarting service` / `[ROAMLINK] FORCE_ROAMLINK: channel timeout, restarting service` / `[ROAMLINK] FORCE_ROAMLINK: biz no data, restarting service`

#### 策略4（FORCE_SIM）

- 始终走物理 SIM，不启用任何 Roamlink 切换。断网后走 **L1/L2/L3 分级恢复**（见第五节）。
  _日志_：启动 `[INIT] policy=4`；恢复见第五节 `[RECOVERY L*]`

> 切换代价提示（供测试参考）：物理 SIM ↔ Roamlink 之间一次切换会有一段无服务窗口，
> 切换不宜过于频繁。上述 300 秒回切节流即为此设计的一部分。

---

### 四、证书（license）与切换的关系

Roamlink 能否使用，取决于启动时的四态探测 `roamlink_probe()`（在 `dial_mng_new()` 执行一次）：

| 探测结果 | 条件 | 处理 | 日志 |
|---|---|---|---|
| `OK` | RBMaster + `/opt/conf.ini` + license 均就绪 | 按策略正常使用 Roamlink | `[ROAMLINK] probe OK ...` |
| `NO_PACKAGE` | RBMaster 二进制不存在 | 本次运行**永久降级 FORCE_SIM** | `[ROAMLINK] probe NO_PACKAGE: RBMaster missing, force SIM` |
| `CONF_MISSING` | RBMaster 存在但 `/opt/conf.ini` 缺失 | 降级 FORCE_SIM，不启 RBMaster，等 factoryApp 拿到 conf.ini 后重启再探测 | `[ROAMLINK] probe CONF_MISSING: /opt/conf.ini absent, force SIM` |
| `LICENSE_MISSING` | RBMaster + conf.ini 就绪但 license 缺失 | 进入 `license_pending`，临时走 SIM 并自动下载 license | `[ROAMLINK] license missing or empty ... → LICENSE_MISSING` |

**证书路径**

- 主路径（RBMaster 实际读取）：`/usrdata/roamlink/etc/.pconfig/license.cer`
- 备份路径（掉电持久化分区）：`/data/ufs/license.cer`

**license 自动下载（`license_pending` 流程）**

1. 主路径 license 缺失时，`dial_mng_new()` 先尝试从备份路径恢复；恢复成功则重新探测。
   _日志_：成功 `[ROAMLINK] license restored from backup, roamlink available` / 失败 `[ROAMLINK] license missing, backup restore failed: ...`
2. 备份也没有 → 进入 `license_pending`，**降级走物理 SIM 拨号**先把网络拉通。
   _日志_：`[ROAMLINK] license missing or empty at /usrdata/roamlink/etc/.pconfig/license.cer → LICENSE_MISSING`
3. **证书下载用的是 SIM 卡的网络**：物理 SIM 数据呼叫首次连上（进入 `net_connected`）后，才启动 RBMaster（`rbmaster_started` 标志保证只启一次），由它**借用已通的 SIM 网络**连云端下载 license。证书没到之前不会去切 Roamlink（没证书切了也连不上）。
   _日志_：`[ROAMLINK] SIM connected, starting RBMaster for license download`
4. 每 **60 秒（LICENSE_CHECK_INTERVAL）**轮询一次主路径；license 出现后 → **备份到 `/data/ufs/` 并 reboot**，重启后按策略正常运行（此时才真正走 Roamlink）。
   _日志_：`[ROAMLINK] license check (60s / 300s elapsed)...` → `[ROAMLINK] license file appeared! backing up and rebooting` → `[ROAMLINK] license backed up to /data/ufs/license.cer` → `[ROAMLINK] rebooting now`
5. 总等待超过 **5 分钟（LICENSE_WAIT_TIMEOUT）**仍未下到 → 放弃 pending，保持 FORCE_SIM 继续运行。
   _日志_：`[ROAMLINK] license wait timeout (301s), giving up license_pending mode, stay FORCE_SIM`

**证书与策略的关系**：在 `license_pending` 期间，注册超时**不会**切 Roamlink（license 还没到，切了也没用），
而是走 cfun 重置重试，等 license 到位重启后再按策略走 Roamlink。

---

### 五、强制 SIM（策略4）下的分级恢复 L1/L2/L3

仅在 `NET_POLICY_FORCE_SIM` 且当前未在 Roamlink 通道时启用：

分级**纯按断网时长（downtime）驱动**（v1.31.5 起，不再读 SIM 状态做"立即恢复"）：

| 级别 | 触发（断网时长） | 动作 | 日志 |
|---|---|---|---|
| L1 | ∈ [60s, 5min) | 软重拨：停数据呼叫 + 重启数据呼叫（REG 在线才执行；REG 不在线只打日志跳过）。每 60s 节流一次 | `[RECOVERY L1] REG live, soft redial: stop+start data call (downtime=60s)` / `[RECOVERY L1] REG down, skip redial (downtime=60s)` |
| L2 | ≥ 5min | 射频重置：`AT+CFUN=0` → `AT+CFUN=1`，再重启数据呼叫（5min 冷却）。**策略4 下 CFUN 复位由 L2 独占** | `[RECOVERY L2] AT+CFUN=0 rsp: OK` → `[RECOVERY L2] AT+CFUN=1 rsp: OK` |
| L3 | ≥ 30min | 进程 `exit(1)`，由看门狗重拉、重走完整初始化 | `[RECOVERY L3] FATAL: Network down 30mins. Exiting for watchdog to reinitialize.` |

> 每次触发恢复前还会先打一条快照：`[RECOVERY L1] LastErr: ... | PDP: ...`（CEER 错误码 + PDP 激活状态）。

> **CFUN 复位的所有权（v1.31.5）**：策略4 **已连通过**后，注册超时处理（reg_timeout）**不再自己做 CFUN**，让位给 L2 统一复位——日志打 `[REG TIMEOUT] policy=4 (connected): defer cfun reset to L2 ladder`，避免与 L2 在同一段 reg_check 卡死期对模组重复复位。**冷启动（尚未连通）**时 L1/L2/L3 仍被关闭，注册超时照常做 `[REG TIMEOUT] policy=4, cfun reset and retry` 兜底。

> L3 为纯 `exit(1)`，不发 `AT+CFUN=1,1`：EG25 为 OpenCPU，cfun=1,1 会整机重启并连带 Linux，故改由看门狗重拉进程。

---

### 六、SD 卡日志系统（`dial_log`）

日志同时输出到 stdout 和 SD 卡（`/media/sdcard`），格式 `[YYYY-MM-DD HH:MM:SS] message`。

- **按天分目录**：`/media/sdcard/dial_log/YYYY-MM-DD/dial_HHMMSS.log`，跨零点自动换到新一天的目录。
- **40 天保留**：`LOG_RETAIN_DAYS = 40`，删除超过 40 天的整个日期文件夹。
- **空间兜底**：可用空间低于 `LOG_FREE_FLOOR_MB = 1024` MB 时，从最旧开始清理；初始化最低门槛 `MIN_FREE_SPACE_MB = 500` MB（SD 卡未挂载/空间不足时最多等待 30s）。
- **时钟未同步处理**：开机时钟还在 1970 的日志统一进 `unsynced/`，不套天数规则，改按数量保留最新 `UNSYNCED_KEEP = 20` 个文件；时钟同步后切回正常按天目录。
- **旧平铺日志迁移**：历史平铺的 `dial_*.log` 一次性迁入对应日期目录。
- **线程安全**：`dial_log` 被主循环与 SDK 指示回调线程并发调用，换文件分支用**递归 pthread 锁**保护共享文件句柄，避免 use-after-free。

_日志_：清理打 `[CLEANUP] /media/sdcard/dial_log: kept 40, removed 3 old.`、`[LOGCLEAN] removed dir ...`；迁移打 `[LOGMIGR] dial_xxx.log -> .../ (ret=0)`。

> 该日志模块（`logger_sd.c`）为 EC200A/EG25/IMX6 三平台共用。

---

### 七、运行时配置与状态文件

| 文件 | 用途 |
|---|---|
| `/usrdata/network.ini` | 策略配置（`network_select`），启动时读一次，改后需重启 `modem_mng` 生效 |
| `/opt/conf.ini` | RBMaster 前置依赖（factoryApp 下载） |
| `/usrdata/roamlink/etc/.pconfig/license.cer` | license 主路径 |
| `/data/ufs/license.cer` | license 备份（持久化） |
| `/tmp/network_type` | 当前通道：1=SIM / 2=Roamlink / 0=切换中 |
| `/tmp/dial_status` | INI 格式实时状态（version/state/policy/channel/license_pending/断网统计等），每 30s 刷新 |

RBMaster 存活判定为**双重检测**：`/proc/<pid>/cmdline` 含 "RBMaster"（进程存在）
+ 非阻塞连接 `127.0.0.1:5568`（控制端口就绪），两项都过才算正常，防僵尸进程误判。

---

### 八、日志关键字速查表（测试 grep 用）

所有日志均落 SD 卡（`/media/sdcard/dial_log/`）。关键字为源码原文，示例值为代表性真实数（`...` 处为运行时填充）。

| 关注点 | grep 关键字 | 示例日志行 |
|---|---|---|
| 启动信息 | `[INIT]` | `[INIT] policy=1` / `[INIT] ICCID: ...` / `[INIT] IMSI: ...` |
| 模组信息 | `[MODEM]` | `[MODEM] Model: ...` / `[MODEM] FW: ...` |
| 状态机流转 | `[STATE]` | `[STATE] sim_check -> reg_check` |
| 心跳 | `[HEARTBEAT]` | `[HEARTBEAT] Network recovered after 47s` / `[HEARTBEAT] Ping failed 3 consecutive times, fault timer started` |
| 小区切换 | `[CELL CHANGE]` | `[CELL CHANGE] 12345 -> 67890 \| ...` |
| 故障快照 | `[DIAG]` | `[DIAG] ...` |
| probe 探测 | `[ROAMLINK] probe` | `[ROAMLINK] probe OK ...` / `... NO_PACKAGE ...` / `... CONF_MISSING ...` |
| 证书缺失/恢复 | `[ROAMLINK] license` | `[ROAMLINK] license restored from backup ...` / `[ROAMLINK] license missing or empty ... → LICENSE_MISSING` |
| 用 SIM 下证书 | `starting RBMaster for license` | `[ROAMLINK] SIM connected, starting RBMaster for license download` |
| 证书下到/备份重启 | `license file appeared` / `backed up` / `rebooting` | `[ROAMLINK] license file appeared! backing up and rebooting` |
| 证书等待超时 | `license wait timeout` | `[ROAMLINK] license wait timeout (301s), giving up license_pending mode, stay FORCE_SIM` |
| 开机走 Roamlink | `Initial channel: Roamlink` | `[ROAMLINK] Initial channel: Roamlink (starting)` |
| starting→active | `channel now active` | `[ROAMLINK] Ping OK in starting state, channel now active` |
| 连接超时切 SIM | `Connect timeout` | `[ROAMLINK] Connect timeout (300s), switching to SIM` |
| 掉线超时切 SIM | `Channel fail timeout` | `[ROAMLINK] Channel fail timeout (300s), switching to SIM` |
| 无数据切 SIM | `Biz layer no data` | `[ROAMLINK] Biz layer no data for 121s, switching to SIM` |
| SIM 断网切 Roamlink | `switching to Roamlink` | `[ROAMLINK] policy=2: SIM down 180s, switching to Roamlink` |
| 策略1 回切 Roamlink | `Policy1: SIM stable` | `[ROAMLINK] Policy1: SIM stable 300s, trying Roamlink recovery` |
| 策略2 回切 SIM | `Policy2: Roamlink stable` | `[ROAMLINK] Policy2: Roamlink stable 300s, trying SIM recovery` |
| 策略3 重启服务 | `FORCE_ROAMLINK` | `[ROAMLINK] FORCE_ROAMLINK: connect timeout, restarting service` |
| 注册超时分派 | `[REG TIMEOUT]` | `[REG TIMEOUT] policy=1, switching to Roamlink` / `[REG TIMEOUT] policy=4, cfun reset and retry`（冷启动）/ `[REG TIMEOUT] policy=4 (connected): defer cfun reset to L2 ladder`（已连过，让位 L2） |
| 分级恢复 | `[RECOVERY L` | `[RECOVERY L1] REG live, soft redial ...` / `[RECOVERY L2] AT+CFUN=0 rsp: OK` / `[RECOVERY L3] FATAL: Network down 30mins. Exiting ...` |
| CFUN 操作 | `[CFUN]` | `[CFUN] Sending AT+CFUN=0 (stop RF)` |
| 运营商选择 | `[OPER]` | `[OPER] Selected operator ..., response: ...` |
| SDK 数据呼叫回调 | `[SDK]` | `[SDK] DataCall connected \| profile=0 \| IP=...` / `[SDK] DataCall disconnected ...` |
| 日志清理/迁移 | `[CLEANUP]` / `[LOGMIGR]` | `[CLEANUP] /media/sdcard/dial_log: kept 40, removed 3 old.` |

> 关键字取自当前 v1.31.5 源码（`eg25/dial/dial.c`、`roamlink/roamlink.c`、`logger_sd.c`），后续版本如改动字符串以代码为准。
