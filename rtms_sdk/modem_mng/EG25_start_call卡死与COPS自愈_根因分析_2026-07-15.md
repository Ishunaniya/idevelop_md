# EG25 `start_call` 状态机卡死 × COPS 手动锁自愈 —— 根因分析与改进建议

- 平台：**EG25G**（物理 SIM，`CH:SIM`）
- 涉及提交：`3eefecce`（"手动选网锁死自愈"，即 `reg_timeout` 里的 `AT+COPS=0`，版本 1.31.14）
- 日期：2026-07-15（起）；**2026-07-16 增补真机直证（§3.4）**
- 证据材料：现网 2 小时 `dial_log`（05:50–07:59）+ 三张 `microcom`/`AT+COPS?` 截图 + **2026-07-16 两组 `[SM]` 真机测试日志（§3.4）**
- 代码基线：`apps/modem_mng/eg25/dial/dial.c` 等

> **证据分级约定**：全文用【事实】=日志/代码直接可证；【强推断】=代码语义+时序推出、但未被日志直接显示；【待验】=需台架实验坐实。请勿把【强推断】当【事实】引用。

---

## 0. 一句话结论

现网这台 EG25（策略4/FORCE_SIM）从 **06:19:04 起，dial 状态机永久卡死在 `start_call`**，导致 `reg_check → reg_timeout → COPS=0 自愈` 整条路径进不去、成为死代码；两小时内所有断网恢复全靠**独立的、看 ping 的 L1/L2 恢复阶梯**硬撑。这解释了为什么 `3eefecce` 的 COPS=0 自愈**在那份现网日志里一次都没触发**。

**2026-07-16 真机（`[SM]` 日志，§3.4）已把上述从推断升为直证**，并厘清了三件事：
1. **卡死是真的**：恢复后 `[SM] dial_st=start_call | is_func_called=0` 持续 4 分钟+（Run 1）。
2. **自愈没触发有两条路**：路 A＝已卡死进不了 reg_check；路 B＝进了 reg_check 但 **L2（看 downtime）抢在 `reg_timeout`（看 reg_check 停留）前约 16s 触发、CFUN 救回**。
3. **自愈本身是好的、非冗余**：**冷启动 + 从未连过**时 L1/L2 被门控关，`reg_timeout` 停满 300s → `forcing COPS=0` → 救活设备（Run 2，真机直证）。

卡死的**直接原因**是 `start_call` 没有失败退路；**诱因**是恢复阶梯与状态机抢用同一个 `is_func_called` 标志/同一路数据呼叫，恢复动作先拨成把标志清零，状态机随后在 `start_call` 拨不动。**以上均已【真机直证】（§3.4）。**

---

## 1. 背景：这次排查是怎么一步步逼到根因的

起点是一个现场现象：设备正常联网后，**手动**把选网锁成电信（`AT+COPS=1,2,46011`）后连不上网，后续走到 L2 恢复（CFUN）就又能上网了，且 `AT+COPS?` 显示仍是**手动模式**、运营商变成了联通（`+COPS: 1,2,"46001",7`）。

三张截图（`md/` 下）证实：
1. `AT+COPS=1,2,46011`（46011=**中国电信**，手动锁）。
2. L2 日志：`[RECOVERY L2] AT+CFUN=0/1` → `[SDK] DataCall connected`。
3. `AT+COPS?` → `+COPS: 1,2,"46001",7`（mode=**1 手动**，46001=**中国联通**，LTE）。

围绕"这个 COPS 自愈到底有没有意义、为什么不触发"，逐层深挖，最终落到 `start_call` 卡死这个真根因。

---

## 2. 现场事实（日志/截图直接可证）

### 2.1 平台与配置【事实】
- EG25G，物理 SIM 通道（`CH:SIM`）。
- **策略 4（FORCE_SIM）**：L1/L2/L3 恢复阶梯被硬门控在 `network_policy == NET_POLICY_FORCE_SIM`（`dial.c:819`），日志里出现 `[RECOVERY L1/L2]` 即证明是策略4。

### 2.2 运营商与 SIM【事实 + 推断】
- 小区 `99D991` = QENG `460,01` = **联通 46001**（设备最终稳定驻留处）。
- 小区 `D17C148` = QENG `460,00` = **移动 46000**，多次 `LIMSRV`（限定服务）。
- 用户手动锁的 `46011` = **电信**。
- 设备开头（05:50–05:57）工作在 `D17C148`（移动），后段（06:59+）稳定在 `99D991`（联通）——**一张卡在移动和联通上都能拿到服务**。
- DNS 全程为 `193.181.246.57/58`（非国内三大运营商解析器网段）。
- 【强推断】综合"移动/联通双栖 + 境外 DNS"：这是一张**漫游/物联网 MVNO 卡**，不是普通国内单运营商卡。这类卡靠自由重选伙伴网工作，**手动锁死单个 PLMN 对它伤害最大**。SIM 身份/APN 未直接取证。

### 2.3 故障形态【事实】
- 05:58 起进入约 1 小时的抽风：`DataCall` 反复 connect/disconnect、REG 0/1 抖动、`CEER 6,258`（后期 `6,259`）、小区在联通↔移动 LIMSRV 间重选。
- 06:41 起失败模式**质变**：`err=0x0` → `err=0xd`、`ifname=?`（rmnet 接口消失）、`rx_packets` 冻结、`CGACT` 全 0。
- 两次 L2：`06:47:12`、`06:59:14`（`AT+CFUN=0/1`），均恢复。
- **06:59 之后稳定 1 小时+**（`99D991` 联通，rx 持续增长）。
- **全程 2 小时：`[REG TIMEOUT]` 零条、`[OPER]` 零条。**

**六段断网明细（`fault timer started` → `Network recovered` 计）：**

| # | 时间 | 恢复用时 | 恢复靠 | 备注 |
|---|---|---|---|---|
| 1 | 05:59–06:02 | 198s | L1 软重拨 | err 0x0 / CEER 6,258 |
| 2 | 06:04–06:07 | 163s | L1 | 小区 99D991→FFFFFFFF→99D991 |
| 3 | 06:14–06:19 | 309s | L1（**249s 处软重拨救回**） | **downtime 峰值 279s，离 L2/reg_timeout 只差几秒**；本段就是把 `is_func_called` 吃掉、诱发卡死的那次 |
| 4 | 06:33–06:34 | 30s | SDK 自恢复 | 短暂 |
| 5 | 06:41–06:47 | 330s | **L2 CFUN（06:47:12）** | err 0xd / ifname=? / CEER 6,259 |
| 6 | 06:53–06:59 | 330s | **L2 CFUN（06:59:14）** | err 0xd / ifname=? / CEER 6,259 |
| — | 06:59–07:59 | — | 稳定 1 小时+ | 99D991 联通 |

- 第 3 段是"自愈最接近触发的一次仍没触发"的佐证：downtime 冲到 279s，被 249s 的 L1 软重拨救回——而**正是这次 L1 软重拨吃掉了 `is_func_called`**，随后状态机走到 `start_call` 饿死（见 §3.2）。
- 温度：第一波不稳时 `Temp` 升到 57°C（`Temp:57,48,47`）后回落，暂判与故障无因果、更像模组搜网负荷，**未取证**。

### 2.4 关键异常信号【事实】
- 全日志最后一条 `[STATE]` 是 **`06:19:04 pre_start_call -> start_call`**，之后到结束（约 1 小时 40 分）**再无任何 `[STATE]`**，尽管期间设备上线→掉线→上线→掉线→上线好几轮。
- `06:15:31` 有 `[STATE] net_connected -> reg_check`（说明它当时确实会进 reg_check），但 06:41/06:53 两段 REG=0 长断网期间**没有**任何 `net_connected -> reg_check`。

---

## 3. 根因：`start_call` 永久卡死

### 3.1 第一层：卡在 `start_call`（【事实】，日志逼出来的）

1. `[STATE]` 是**每次 `dial_st` 一变就打**（`dial.c:1010-1021`，dedup 只与上一个比较）。已核实全循环的 `continue`/`goto` 均在 `[STATE]` 段（1010）之后，故该段**每轮必执行**、不会被跳过。
2. 最后一条 `[STATE]` 停在 `06:19:04 -> start_call`，之后一片空白。
3. **反证防线**：`[RECOVERY L1/L2]`、`[HEARTBEAT]` 与 `[STATE]` 同在 dial_task 主循环里。06:43/06:44/06:47 这些 RECOVERY 都打出来了 → 说明循环确实转到了 `[STATE]` 段、只是没检测到状态变化。

∴ **`dial_st` 从 06:19:04 起恒等于 `start_call`，没动过。** 这一层是确定的，不是推断。

### 3.2 第二层：为什么卡在 `start_call`（原【强推断】，2026-07-16 已【真机直证】，见 §3.4）

- `dial_stat_start_call`（`dial.c:1281-1287`）**唯一出路**是 `dail_start_data_call()` 返回 true；**无 else、无超时**。
- `dail_start_data_call()`（`dial.c:258-290`）整体被 `if(is_func_called)` 包着：标志为 false 就空转返回 false；拨号成功则把标志清成 false（`285`）。

**核验 A（grep 全部 `is_func_called=true` 赋值点）**：分布在 `72/574/682/717/784/854/883/1383/1413/1441/1662/1713/1761/1922`。逐个对到状态——`cereg_check`/`precondition_check`/`pre_start_call`/`start_call` **无一置 true**。即从 reg_check 成功到 start_call 这一路没人重置标志。

**核验 B（时序，06:15→06:19）**：

| 时刻 | 事件 | `is_func_called` |
|---|---|---|
| 06:15:31 | `net_connected -> reg_check`（`1413`/`1441` 置 true） | **true** |
| 06:15:31~06:18:21 | 卡 reg_check，L1 "REG down, skip redial"（不碰标志） | true |
| **06:18:53** | `[RECOVERY L1] REG live, soft redial`：`854` 置 true → `855` 拨号**成功** → `285` **清 false**（日志 06:18:56 "issued OK"、06:18:57 connected） | **false** |
| 06:19:00~04 | reg_check 成功 → cereg → precondition → `start_call` | false |
| **06:19:04** | 进 `start_call`：`dail_start_data_call` 见标志=false → 空转返回 false → **原地饿死** | false（永久） |

**对照组（05:58，没卡）**：同样 `net_connected -> reg_check` 置 true，但那次断网短、**第一条 L1 到 06:00:22 才出现**，reg_check→start_call 之间没有 L1 抢标志 → 05:58:46 进 start_call 时标志仍 true → `start_call -> wait_for_connect -> net_connected` 一路打通。

> **两次唯一变量**：断网期间 L1 有没有先拨成一次、把 `is_func_called` 吃掉。长断网 → L1 抢先 → 卡死；短断网 → 状态机自持标志 → 走通。

### 3.3 连带后果【事实】

卡死在 `start_call` → **永远回不到 `reg_check`** → `reg_timeout`（仅由 `reg_check` 内 `goto` 到达，`dial.c:1207/1239`）永不触发 → 挂在 `reg_timeout` 顶部的 **COPS=0 自愈（`dial.c:1827-1854`）成为死代码**。设备靠 ping 驱动的 L1/L2 阶梯（与 `dial_st` 无关）硬撑存活。

这就是 06:41/06:53 两段 REG=0 长断网、**没有**触发 `3eefecce` 修改的真正原因——不是"抽断太快"，是状态机根本没在跑注册那条路。

> **（2026-07-16 更正）** 之前把"两个 300s 表"降级为次要、说"被卡死取代"——这是错的，真机（§3.4 Run 1）证明它是**另一条独立的死路**：当掉线时机器**还在 net_connected**（不是已卡死），会掉进 reg_check、`is_func_called=1`，但 **L2 看 downtime、`reg_timeout` 看 reg_check 停留**，downtime 比 reg_check 早起跑约 42s（net_connected 赖着不掉的那段），两阈值都 300s，故 **L2 永远先到、CFUN 救回、把机器拽出 reg_check，`reg_timeout` 差最后约 16s 触发不了**。所以自愈够不着有**两条路**：路 A＝已卡死在 start_call 进不了 reg_check（§3.1/§3.2）；路 B＝进了 reg_check 但被 L2 抢跑（此条）。**两条都被真机直证。**

### 3.4 真机直证（`[SM]` 日志，2026-07-16 两组测试）

用新加的 `[SM]` 观测日志在真机复现，两组测试把 §3.1/§3.2 的推断全部升为直证，并额外证实了 `3eefecce` 的有效边界。设备：EG25 1.31.14，策略4，IMSI 455…（澳门/漫游卡），手动锁 `AT+COPS=1,2,23415`（英国 Vodafone，当地不存在）。

**Run 1 — 运行中锁网（`has_connected_once=1`，走路 B）**

| 时刻 | 观测 | 意义 |
|---|---|---|
| 开机 00:09 | `[INIT] COPS: 1,0,"CHINA MOBILE"` | **COPS 存 NV 跨重启＝直证** |
| 00:11:21 | 锁 23415 后 REG=0，但 `[SM] dial_st=net_connected` | SDK PDP 仍报连着，机器赖在 net_connected |
| 00:13:12 | `net_connected -> reg_check`（比 downtime 晚 ~42s） | 路 B 起点 |
| 00:13:31~17:10 | `[SM] dial_st=reg_check \| is_func_called=1`；L1 全 "REG down skip" | **reg_check 里标志=1、L1 skip 不消耗＝直证** |
| 00:17:42 | `[RECOVERY L2]`（downtime=312s）→ CFUN → 00:17:56 离开 reg_check（停留仅 284s） | **L2 抢跑 reg_timeout（差 ~16s）＝直证** |
| 00:18:03~22:01 | `[SM] dial_st=start_call \| is_func_called=0`（持续 4 分钟+） | **恢复后卡死在 start_call、标志=0＝直证** |
| 全程 | 无 `[REG TIMEOUT]` | 自愈未触发（被 L2 抢跑 + 随后卡死） |

**Run 2 — 冷启动锁网（`kill -9` 重拉，`has_connected_once=0`，自愈触发）**

| 时刻 | 观测 | 意义 |
|---|---|---|
| 00:32:33 | 进程冷启动；`[INIT] COPS: +COPS: 1`（手动、未驻留） | 锁在不存在的 23415，未注册 |
| 00:32:51→ | `[SM] dial_st=reg_check \| is_func_called=1`，REG=0，**全程无 `[RECOVERY]`** | `has_connected_once=0` → L1/L2/L3 门控关（`dial.c:816`），无人抢跑 |
| **00:37:59**（停 308s） | **`[REG TIMEOUT] COPS not auto (+COPS: 1), forcing COPS=0` + `AT+COPS=0 rsp: OK`** | **`3eefecce` 自愈真机触发＝直证** |
| 00:38:00~23 | `policy=4 cfun reset` → stop_cfun/start_cfun → net_connected | 清手动锁后 CFUN 重注册 |
| 00:38:48 | `Network recovered after 282s`，回 China Mobile，新 IP 10.94.157.3 | **自愈把设备救活＝直证** |

**结论（真机）**：`3eefecce` 在其目标场景（**冷启动 + 从未连过**，L2 被门控关）**确实触发并救活设备**；在**已连过**的场景则被路 B（L2 抢跑）或路 A（卡死）挡住。所以自愈**不是冗余、是有效的**，只是当前**仅冷启动路径能触发**——要让"已连过"也能兜底，得修 §5 的 `start_call` 卡死（路 A）。

> **（2026-07-16 重审纠正，重要）**：上面"已连过被路 B（L2 抢跑）挡住 → COPS=0 触发不了"这句是**旧固件（含卡死 bug）**的结论，**改动①② 后不再成立**。改动① 后 L2 **只发 CFUN、不改 dial_st、不重置 dial_timer**（`dial.c:1016-1017` 注释「dial_st 维持原状」），`reg_check` 的 dwell 计时入口设一次、中途不重置 → 模组 CFUN 后若仍**注册不上**，reg_check 一路 dwell 到 300s → reg_timeout → **COPS=0 照跑，L2 抢不走**。所谓"路 B 抢跑"其实是**旧 L2 的 `dail_start_data_call` 强拉数据呼叫、逼 reg_check 通过→冲 start_call 卡死**才够不到 reg_timeout（=路 A 卡死的另一面）。∴ **改动①② 同时修好了"连过设备 COPS=0 兜底"的可达性**：L2 的 CFUN 先试，能重注册就恢复（真机 Run A 锁 23415→camp 回移动即此列，够不到也不需要 COPS=0）；CFUN 清不掉 NV 手动锁时，reg_check dwell 满 300s → COPS=0 清锁兜底（**代码可达、未真机复现**——23415 的 CFUN 总能救回，没造出"CFUN 清不掉的锁"）。详见 `md/EG25_选网逻辑_list_oper_select_oper_代码走读_2026-07-16.md` §6.2/§5.1。

### 3.5 根因提交定位（`git` 直证）与本次修复

**卡死是哪条提交引入的——已 `git` 定位到出生地【事实】**：

- **`0310629343f3832b6f0262d0ba60eb3d6cb53f0e`**（minglei.li，2026-02-11，**「EG25: 增加三级恢复机制」**，`eg25/dial/dial.c` **+102 行、未删任何行、未改 start_call**）——**三级恢复的首次引入，就是卡死的出生地。**
- 它新增的 L1/L2 当场就是"抢钥匙"写法（`git show` 直证）：
  - L1：`dail_stop_data_call` → `is_func_called=true` → **`dail_start_data_call`**
  - L2：`AT+CFUN=0/1` → `sleep(10)` → `is_func_called=true` → **`dail_start_data_call`**
- 当时**触发面比现在更广**：门控只有 `if (!net_ok && start_fail_ts > 0)`——**没有 `has_connected_once`、没有 `NET_POLICY_FORCE_SIM` 门控**（这两道是后来才加的）。
- `start_call`"无 else、无超时"的隐患**在此之前就潜伏**（继承自 artery，但 artery 全仓 0 个 `dail_start_data_call` 额外消费者，故安全，见 §9）。**这条提交加进 L1、L2 两个 switch 外的消费者，一次性把潜伏的坑激活。** 此后所有提交只是收窄门控、L1 加 REG 判断、L3 改 `exit(1)` 等微调，**"抢钥匙"的核心写法原样保留至今。**

> ∴ 卡死从 `0310629` 出生、一路活到 1.31.14。它不是某次改坏的回归，而是三级恢复**从设计之初就与 `is_func_called` 门控状态机不兼容**。

**本次修复（改动①②，2026-07-16 已应用，未编译未提交）**：把重拨从阶梯收敛回状态机唯一一处 `start_call`，对齐 artery——

- **L1**：删掉 `dail_start_data_call`，只保留 `dail_stop_data_call` + `is_func_called=true`；
- **L2**：删掉 `dail_start_data_call`，只保留 `AT+CFUN=0/1` + `is_func_called=true`（与该处 996–999 行原注释"CFUN 后由状态机重拨"对齐——原代码自相矛盾）；
- 重拨统一由**既有正确路径**完成：`net_connected` 掉线 → `dial.c:1462` 置 `is_func_called=true` + `dial_st=reg_check` → cereg → precondition → `start_call`（此时标志=true，正常 Init+Start）。
- **grep 核实**：改后 `dail_start_data_call` **活跃调用点只剩 `start_call` 一处**（= 与 artery 一致，0 个额外消费者）。块头注释（936–）与本函数注释已同步校正。

**真机验证（2026-07-16，新固件=改动①②，版本仍 1.31.14）**：

- **改动①（L2 不偷钥匙）＝真机直证修复**。同一设置（锁 23415、REG=0、L1 全 skip、L2 在 downtime≈312s 触发）下，**旧固件（§3.4 Run 1）卡死在 `start_call | is_func_called=0` 4min+，新固件（本次 Run A）`start_call → wait_for_connect → net_connected` 342s 恢复**——唯一代码差异即改动①，构成干净 A/B 对照。链路：01:54:11 掉线经既有路径 1462 置 `is_func_called=1`+回 reg_check → 全程 reg_check 钥匙稳为 1（L1 只 skip、L2 不再抢）→ L2 只做 CFUN、重拨交状态机 → 01:59:00 到 start_call 钥匙=1 → 拨通。
- **改动②（L1 REG-live 不偷钥匙）＝仅代码审查，运行时未验**。两次测试锁的都是不存在的网（23415）→ REG 恒 0 → L1 全程走 "REG down, skip redial"，**偷钥匙分支从未执行**。需另造"REG=1 但数据不通"（改错 APN）场景补验。
- **COPS=0 自愈（Run B 冷启动）＝真机直证、无回归**：冷启动 has_connected=0，L1/L2 整块被门控关、改动①② 不执行，`3eefecce` 原样跑通（02:07:09 forcing COPS=0 → 恢复）。

**尚未做**：改动③（`start_call` 超时安全网 + `START_CALL_TIMEOUT_SECONDS`，见 §5 P0-1）未应用；**改动②（L1 路径）运行时未验**——完整闭环还差"REG=1 数据不通"一测（见 §6 验证表）。

---

## 4. 关于 `3eefecce`（COPS=0 自愈）本身是否有意义

结论（**2026-07-16 真机更新**）：**有意义、且已真机证明有效**——冷启动 + 从未连过时会触发并救活设备（§3.4 Run 2）。之前"很可能冗余/悬而未决"的判断**已被推翻**，此处保留原推理链仅作演变记录。

- 它治的是「病 A」：**彻底注册不上、手动锁死** → 设备一直卡在 reg_check → 5 分钟后 `AT+COPS=0` 掰回自动。**冷启动路径已真机直证（§3.4 Run 2：`00:37:59 forcing COPS=0` → 恢复）。**
- 这份日志是「病 B」：抽断+有可回落网络（联通）→ **L2 的 CFUN 就能靠回落救回**，`AT+COPS?` 恢复后仍 `mode=1`（截图证实，CFUN 不清手动模式，与 `3eefecce` 前提一致）。
- **德国那台**（修改来由）之所以困死，据用户澄清是**那个版本压根没有 L1/L2/L3 和自愈——一个恢复机制都没有**，只能人工 `COPS=0`。**不是"CFUN 试过救不回"**。
- ~~【强推断】…COPS=0 自愈**很可能冗余**…~~ **（已推翻）** 冷启动路径真机证明自愈是唯一恢复手段（L2 被 `has_connected_once` 门控关，只有 reg_timeout 兜底），并非冗余。已连过场景 L2 CFUN 确能靠回落救回（§3.4 Run 1），但那是**另一条恢复路**，不代表自愈无用——两条覆盖不同场景（冷启动 vs 运行中）。
- COPS=0 自愈的**位置是对的**（挂在 reg_check 超时上，只在真卡死时才发），**不建议**搬到 L2（会在不需要时乱发、并可能误伤手动选网功能）。让它"够不着"的是 §3 的 `start_call` 卡死；修了 P0，它自然能在该触发时触发。

### 4·补 `is_oper_select` 选网行为：手动切自动后，重启会不会回到手动？

这是排查最初的问题，与 COPS/手动选网直接相关，一并记录。

**先分清两个东西：**

| | 存哪 | 重启是否保留 | 谁改 |
|---|---|---|---|
| **COPS 模式**（0 自动 / 1 手动） | 模组 **NV** | 保留（【强推断】现场证据支持、台架未证） | `select_oper` 发 `COPS=1,2,X`、自愈发 `COPS=0` |
| **`is_oper_select`**（默认 true） | apn.json，**每次开机重读** | 配置不变就一直是 true | 只有人工改 apn.json（`apn.c:501-504`） |

**关键事实（代码穷举可证）：**
1. init 时按 **ICCID** 匹配 apn.json（`apn.c:521` / `dial.c:1264`）：APN 写入 PDP（立即生效），`is_oper_select` 仅**存进变量**、此刻**不发任何命令**。
2. 读到 `is_oper_select=true` **≠ 主动去锁手动**——它只是"装填"。
3. 全模块锁手动 `AT+COPS=1,2,X` **只有一处**（`dial.c:1521`，在 `select_oper` 内）；进 `select_oper` 的**唯一外部入口**是 `wait_for_connect` 的**数据拨号超时**（`dial.c:1396`，当 `is_oper_select=true`）。`list_oper`/`select_oper` 状态赋值仅见于 `1396`(外部)、`1500`/`1559`(内部互跳)。

**∴ 重启是否回手动：**
- **不会因为"重启"本身回手动**【事实】：开机流程不发任何 `COPS=1`；只要开机能连上，就一路 `net_connected`，永不进 `select_oper`。COPS 模式在 NV 里是上次自愈后的 0（自动）。
- **但没根治**：apn.json 里该卡仍是 `is_oper_select=true`，"枪"一直装填。**重启后若又出现数据拨号超时**，`select_oper` 会重发 `COPS=1,2,<候选PLMN>` 再锁手动；锁上又注册不上，300s 后（若能进 reg_check）自愈再 `COPS=0`……可来回摆，但不会像老固件那样永久困死。
- **要彻底不走手动**：在 apn.json 该卡条目显式写 `"is_oper_select": false`（`3eefecce` 未改此默认值/语义）。
- 补充：reg_timeout 的 COPS=0 自愈**只看 FORCE_SIM、不看 `is_oper_select`**（`dial.c:1827`）——即便该卡是手动选网设备，注册超时照样会掰回自动。

> ⚠️ 本小节与 §3 联动：由于现网设备卡死在 `start_call`、进不了 `reg_check`，上面"若能进 reg_check 则自愈"的分支在该设备上实际也够不着——同属 P0 卡死的连带影响。

---

## 5. 问题清单与改进建议（按优先级）

### P0-1 `start_call` 没有失败退路【确认缺陷】
`dial.c:1281-1287` 无超时、无 else。对比 `sim_check`(1h)、`reg_check`(300s)、`wait_for_connect`(120s) 都有超时退路，唯独它没有。
- **改法**：加与其它状态一致的超时退路（例如停留 >120s 退回 `stop_cfun` 做 RF 复位）。纯安全网，风险低。

### P0-2 `start_call` 依赖上游留好 `is_func_called`【确认根因】
`dail_start_data_call` 靠 `is_func_called` 门控，而该标志被 L1/L2 抢用清零。
- **改法（治本）**：让 `start_call` 自己在调用前置 `is_func_called=true`（必要时先 `dail_stop_data_call` 再 start，与 L1 `853-855` 对齐），不再指望上游。
- **⚠️ 待验**：此时阶梯可能已有一路呼叫在跑，重新 Init+Start 是否与现有呼叫打架需台架实测（L1 是先 stop 再 start 的，start_call 大概率也要照做）。

> P0-1 与 P0-2 建议一起改：P0-2 让它拨得动，P0-1 保证万一还拨不动也不永久卡死。

### P1 恢复阶梯与状态机"两套并行控制系统抢一个 modem"【架构性】
L1/L2/L3（看 ping）与 dial 状态机（看 SDK 呼叫态）共用同一路数据呼叫、同一个 `is_func_called`，互相踩，本次卡死即由此而来。彻底解需把恢复并入状态机、或明确二者职责边界与互斥。工作量大，**单独立项**，不混入本次。

### P1 `net_connected` 过度相信 SDK 呼叫态【可靠性】
`net_connected`（`1401-1445`）用 `nw_get_connect_state`（SDK PDP 态）判在线，而 `err=0xd` 时 SDK 会谎报"连着"。虽有 120s rx 停摆兜底（`1438`），但"真在不在线"应以 ping/rx 为权威。可考虑让掉线判定更依赖 ping/rx。

### P2 其它小项
- **L2 忽略返回值**（`883`：`is_func_called=true; dail_start_data_call(...);`）——建议判返回、失败记日志/重试。
- **卡死可观测性**：已加 `[SM]` 心跳日志（见 §6）；可再加"`dial_st` 长时间不变（如 start_call 停留 >N 秒）"的显式告警。
- **COPS=0 自愈**：位置不动；待 P0 修复后按 §6 场景二验证其触发。

### 建议实施顺序
1. 先合入 `[SM]` 观测日志，让测试**先在真机直证 §3 卡死**；
2. 拿到直证后改 P0-1（超时退路，低风险先上）+ P0-2（自持标志，需台架验证不与阶梯打架）；
3. P1 两处单独立项。

---

## 6. 复现与测试方法

### 6.1 已加入的诊断日志 `[SM]`（纯观测，不改逻辑）
在心跳后新增一行（`dial.c` 心跳块内，`dial_log_heartbeat` 之后），与心跳同频（30s）：
```
[SM] dial_st=<状态名> | is_func_called=<0/1>
```
复用 `dial_eg25_stat_name`（`status/dial_status.c:21`）。已用本机 EG25 工具链（GCC 4.9.3）实编通过，`strings` 确认串已编入 ARM 二进制。**当前仅在工作区，未提交。**

现网 `[STATE]` 只在状态变化时打，卡死时反而空白；`[SM]` 每 30s 复读当前状态，用于当场抓卡死点。

### 6.2 场景一：直证 `start_call` 卡死（运行中锁网）— ✅ 已完成，结果见 §3.4 Run 1
1. 策略4 设备正常联网后，AT 口：`AT+COPS=1,2,"46011"`（或换一个本卡确实用不了的运营商）。
2. 盯日志。断网期间（REG=0、downtime 上爬）若持续出现
   `[SM] dial_st=start_call | is_func_called=0`
   且同时 REG=0、**无** `[REG TIMEOUT]` → **直证卡死点与诱因**（把 §3.2 的强推断升为直证）。

### 6.3 场景二：验证 COPS=0 自愈能触发（冷启动锁网）— ✅ 已完成，结果见 §3.4 Run 2
1. 锁同一个用不了的运营商，然后**重启设备/应用**。
2. 冷启动天然待在注册步，`[SM]` 应显示 `dial_st=reg_check`。
3. 卡满 5 分钟，应打出
   `[REG TIMEOUT] COPS not auto (...), forcing COPS=0` + `AT+COPS=0 rsp: OK`，随后自动恢复、`AT+COPS?` 变 `+COPS: 0`。
   → 这是 `3eefecce` **正确且被触发**的证据。
4. **（2026-07-16 已验证，见 §3.4 Run 2）**：`kill -9` 冷启动 + 锁 `AT+COPS=1,2,23415`（UK，当地不存在），reg_check 停满 308s → `00:37:59 [REG TIMEOUT] forcing COPS=0` → 恢复。**本模组冷启动下不 CFUN 就不自己回落**（干等 5 分钟直到自愈），故用"不存在的网"也能卡住、触发成功。

### 6.4 触发原则说明（给测试，已按真机修正）
- 冷启动自愈触发要点：**冷启动（进程重拉/设备重启）+ 手动锁一个上不去的网 + 别让它连上**。因为 `has_connected_once=0` 时 L1/L2/L3 被门控关，只剩 reg_timeout 兜底，reg_check 才能停满 300s。
- **锁"当地不存在的网"（如 UK 23415）在冷启动下就能卡住**——真机实测本模组不 CFUN 不回落（推翻了此前"会自己回落绕开、卡不住"的担心）。
- 反例：**运行中（已连过）锁网触发不了自愈**——见 §3.4 Run 1，会被 L2 抢跑、随后卡死在 start_call。这是 P0 卡死要修的场景。

### 6.5 附加：`[INIT] COPS` 详细化 + 选运营商逻辑复现

**`[INIT] COPS` 已增强（本次改动，纯观测）**：`AT+COPS?` 在未驻留时按 3GPP 只回 `+COPS: <mode>`（故会看到光秃秃的 `+COPS: 1`＝手动+未注册）。现改为解码 `<mode>` 文字并追加 `AT+CEREG?`，打成：
`[INIT] COPS: <原始> (mode=1 manual) | CEREG: <注册态>`
一眼区分"手动+已驻留 X" vs "手动+搜不到目标(未驻留)"。另把 `[REG TIMEOUT] COPS not auto (...)` 里原始应答的 `\r\n` 压成空格、不再碎成多行。局限：手动**锁定的目标 PLMN**（如 23415）在 `AT+COPS?` 读里无标准回读，日志无法直接显示"锁到了哪家"。

**复现选运营商逻辑（`list_oper`/`select_oper`/`[OPER]`）**：这条路**从 `wait_for_connect` 的数据拨号超时进**（`dial.c:1380→1396`，`is_oper_select=true`），要造 **"能注册但数据连不上"**，不是"注册不上"：
1. 该卡 apn 条目保持 `is_oper_select` 不写 false（默认 true）；
2. 把该卡实际用的 APN（本机是 `internet.lte.cxn`）改成一个**错误 APN**（如 `badapn.invalid`）——能注册、PDP 激活失败；
3. 重启 modem_mng → REG=1 → 进 `wait_for_connect`，数据 120s（`DIAL_TIMEOUT_SECONDS`）连不上 → `wait_for_connect -> list_oper` → `[OPER] Candidate list (N): …` → `select_oper` → `[OPER] Try operator X` → `AT+COPS=1,2,X` → `[OPER] X failed, rsp=…`。
> 注意：别用"锁不存在的网"复现选运营商——那是注册失败、走 reg_check，不进 select_oper。

**⚠️ 改坏 APN 的头号坑：ICCID 必须是本机卡前缀（2026-07-16 真机踩过）**
`apn.json` 靠 **ICCID 前缀**匹配（`apn.c:451` `strncmp(json_iccid, 设备iccid, strlen(json_iccid))`），json 里的 `iccid` 当作前缀。**改坏 APN 必须挂在能命中本机卡的条目下**，否则改了也白改。例：本机卡 ICCID `89852019925010000888`（前缀 `898520`），若把 `badapn.invalid` 挂在 `894642` 下 → 不匹配 → 落空 APN 兜底 → 照常连上、根本不进 `[OPER]`（真机实测过一次白改）。正确做法：新增/改一条 `"iccid":"898520"` + `"apn":"badapn.invalid"`，且放在最前（首个前缀匹配即用）。设备命令：
```sh
cp /usr/dial/apn.json /usr/dial/apn.json.bak   # 备份
# 编辑 /usr/dial/apn.json，第一条改成 {"iccid":"898520","apn":"badapn.invalid",...}
kill $(pidof modem_mng)                          # 重启（看门狗重拉）
# 测完还原： cp /usr/dial/apn.json.bak /usr/dial/apn.json && kill $(pidof modem_mng)
```

**匹配三归宿（EG25 上 apn.json 确实生效：`apn_scan_idx` 恒调 `apn_set(1,…)`→`QL_APN_Set` 写 profile 1）**：
- **命中"坏 APN"**（要造的）→ 卡能注册(REG=1，注册不看数据 APN)但 PDP 激活失败 → 数据连不上 → 120s → `[OPER]`；
- **命中"好 APN"** → 正常联网；
- **一条都不命中** → 走空 APN 兜底：`apn_get_apn_obj` 无匹配返回 NULL（`apn.c:521`）→ `apn.c:528-532` `memcpy cnst_default_apn = {"","","","",NULL}`（`apn=""`）→ `apn_set(1,{apn=""})` 把**空 APN 写 profile 1** → 空 APN = 让模组用**网络默认承载**联网，**通常也能连上**（故"没配对也能上网"，不进 `[OPER]`）。

### 6.6 修复验证结果（2026-07-16，新固件＝改动①②）

在带改动①② 的新固件（版本仍 1.31.14）上跑了两组，结果：

| # | 场景 | 结果 | 结论 |
|---|---|---|---|
| **V2** | 运行中锁 23415（**与旧固件 §3.4 Run 1 同一设置**） | L2 CFUN(downtime 312s) 后 `start_call→wait_for_connect→net_connected`，**342s 恢复、无卡死**；全程 `is_func_called` 在 reg_check 稳为 1 | ✅ **改动① 真机直证**：旧固件此设置卡死 4min+，新固件恢复，唯一差异＝改动① |
| **V3** | 冷启动(`kill -9`)锁 23415 | reg_check 停 307s → `02:07:09 [REG TIMEOUT] forcing COPS=0` → 恢复 | ✅ COPS=0 自愈无回归（L1/L2 门控关、改动①② 不参与） |
| **V1（L1 路径）** | `iptables -A OUTPUT -p icmp -d 8.8.8.8 -j DROP` 造 REG=1 数据在走但 ping 失败 | **L1 REG-live 软重拨连打 4 次**（downtime 65/129/190/251s），每次 `[SM] dial_st=start_call \| is_func_called=1` → 拨通 → net_connected，**一次没卡**；随后 L2×2（312s/617s）也全闭环 | ✅ **改动② 真机直证**：L1 交状态机后 start_call 钥匙恒 1，与旧固件"L1 自拨偷钥匙→start_call 饿死"形成 L1 路径 A/B |

**关键方法论修正（给测试）**：**锁"不存在的网"（23415）测不到 L1 偷钥匙路径**——REG 恒 0 使 L1 永远走 "REG down, skip redial"，能偷钥匙的只有 L2。验改动②（L1 路径）改用 **iptables DROP ICMP** 造 "REG=1、数据在走、只 ping 失败"（见下 V1 补测方法）。改动③（start_call 超时网）三轮测试都没走到需要它的死角。

**V1 附带发现（`is_func_called=1` 滞留，无害）**：iptables 测试中，最后一次 L2 后 SDK 自动重连、`dial_st` 从未离开 net_connected → `start_call` 未跑 → L2 置的 `is_func_called=true` 无人消费，即使 ping 恢复后仍显示 `is_func_called=1`。**这是改动① 的副作用**（旧 L2 会自调 `dail_start_data_call` 可能清 0）。**已论证无害、不修**：全仓消费该标志的活跃点只剩 `start_call`（仅下次掉线经 reg_check 才到），而任何掉线 net_connected 出口（`dial.c:1474`）本就把它置回 true，CFUN 后标志=1（下次先 Init 再 Start）反是正确语义。

**A/B 对照最强证据**：§3.4 Run 1（旧）↔ §6.6 V2（新），设置逐项相同、结局相反，直证改动① 修复了 L2 路径卡死。

#### V1 补测方法：逼出 L1 的 REG-live 软重拨（验证改动②）

**目标**：造 **REG=1（已注册）但 ping 不通** 的状态，让 L1 走它的 **REG-live 软重拨分支**（`nw_reg_status_check` 为真的那支——改动② 改的就是这里），看新固件下 L1「stop→交状态机」后能否正常重拨、**不卡死在 start_call**。

**前置**：设备先用**正常网络连上一次**（`has_connected_once=1`，否则 L1/L2 被门控关，`[SM]` 会显示 REG 正常但阶梯不动）。策略4。

**造"注册着但 ping 不通"（二选一）**：

- **方法 A（最确定，推荐）——host 侧丢弃 ping 目标**：数据/PDP/注册全部保持，只让守护进程的 `test_can_ping_google` 失败：
  ```
  iptables -A OUTPUT -p icmp -d 8.8.8.8 -j DROP    # 若无 iptables 用下一法
  ```
  之后 REG 恒 1、ping 恒失败、downtime 上爬。
- **方法 B——拆 PDP 保注册**：`echo -e 'AT+CGACT=0,1\r' > /dev/smd8`（停 1 号 PDP 上下文，注册不掉）。注意 SDK `reconnect=true` 可能很快自动重激活，需反复发或配合观察。

**通过判据（新固件应看到）**：
1. downtime 到 **60s** 时打 `[RECOVERY L1] REG live, soft redial: stop, hand off to state machine`（**不再是** "REG down, skip redial"，也不是旧固件的 "soft redial issued OK"）；
2. 紧接 `[SM]` 里 `dial_st` 走 `reg_check → … → start_call → wait_for_connect`，**全程 `is_func_called` 该 1 时是 1、进 start_call 能拨出**——**绝不出现 `[SM] dial_st=start_call | is_func_called=0` 长期不动**；
3. 方法 A 因 ping 一直被 DROP，会周期性 L1→（再 L1）→L2，正好反复演练 L1 REG-live 路径而始终不卡。
4. **反证（可选，在旧固件上做）**：同样操作，旧固件应在**第一次 L1 REG-live 重拨后**就卡死在 `start_call | is_func_called=0`（旧 L1 自己拨、偷了钥匙）——与新固件形成 L1 路径的 A/B 对照。

> 跑完方法 A 记得 `iptables -D OUTPUT -p icmp -d 8.8.8.8 -j DROP` 撤销。

---

## 7. 证据等级汇总与未决项

| 结论 | 等级 |
|---|---|
| 恢复后卡死在 `start_call`、`is_func_called=0` 拨不动又无退路 | **真机直证**（§3.4 Run 1：`00:18:03–00:22:01` `[SM] start_call\|0`） |
| 卡死诱因＝恢复动作（L1/L2）拨成清零 `is_func_called`；reg_check 里 L1 skip 不消耗 | **真机直证**（§3.4 Run 1：reg_check 全程 `is_func_called=1`；恢复后转 0） |
| L2（downtime 表）抢在 `reg_timeout`（reg_check 停留表）前触发、把机器拽出 reg_check | **真机直证**（§3.4 Run 1：reg_check 停 284s，L2 于 downtime 312s 先救回，差 ~16s） |
| 冷启动+从未连过时，L1/L2 门控关，`reg_timeout` 停满 300s 触发 COPS=0 并救活 | **真机直证**（§3.4 Run 2：`00:37:59 forcing COPS=0` → 恢复） |
| COPS 模式跨重启存 NV | **真机直证**（开机 `[INIT] COPS: 1,0,"CHINA MOBILE"`） |
| `3eefecce` 有效、非冗余（冷启动路径） | **真机直证**（同上 Run 2） |
| CFUN 不清手动模式（mode 仍=1） | **事实**（截图 + Run 1 未见 COPS 变 0） |
| `46011→46001`／回落归属网、mode 字段未复位 | **强推断**（模组固件行为，本仓库无法证明） |
| 该卡为漫游/MVNO 卡 | **强推断**（双栖运营商 + IMSI 455…澳门 + 境外 DNS 202.175.x） |
| 现网 2h 日志的闪断根因是手动锁 vs 覆盖差 | **未定**（倾向覆盖边缘为主、手动锁放大；需对照实验） |

**待做实验（更新）**：
1. ~~直证 `start_call` 卡死~~ ✅ 已完成（§3.4 Run 1）；
2. ~~验证 COPS=0 自愈触发~~ ✅ 已完成（§3.4 Run 2，冷启动）；
3. **待验**：造"在场但被拒"的运营商（present-but-rejecting），看运行中已连过时 CFUN 能否回落——判"手动锁的目标网在场且不可回落时 CFUN 是否救不回、非 COPS=0 不可"这个仍未证实的场景；
4. **待验**：`CEER 6,258/6,259` 按 EG25 手册解码；
5. **待验**：修完 §5 的 `start_call` 卡死后，回归"已连过 + 断网"能否正常经 reg_check 恢复、不再卡 start_call。

---

## 8. 关键代码位置索引

> 行号基准：**2026-07-16 改动①②应用后**的工作区（初始化日志已抽成 `dial_log_modem_info`/`dial_log_init_static_info`，L1/L2 已删 `dail_start_data_call`）。

| 位置 | 说明 |
|---|---|
| **提交 `0310629`** | **根因提交**：2026-02-11「EG25: 增加三级恢复机制」，首次引入 L1/L2 抢钥匙写法（§3.5） |
| `dial.c:1342` | `dial_stat_start_call`：**无超时/无 else 的卡死点**（改动③安全网未应用） |
| `dial.c:258-296` | `dail_start_data_call`：`if(is_func_called)` 门控 **Init(322 那句)+Start**，成功清 false（285）；**现全仓唯一活跃调用点＝1339（start_call 内）** |
| `dial.c:980-996` | L1：REG down skip / **改动②后：只 `dail_stop_data_call`+`is_func_called=true`（994），不自己拨** |
| `dial.c:998-1027` | L2：`AT+CFUN=0/1` + **改动①后：只 `is_func_called=true`（1026），不自己拨**（对齐本处 996-999 原注释） |
| `dial.c:1462-1464` | **既有正确重拨路径**：`net_connected` 掉线 → 置 `is_func_called=true` + `dial_st=reg_check` |
| `dial.c:948-951` | 恢复阶梯门控（仅 FORCE_SIM + has_connected_once；根因提交时**无此门控**，§3.5） |
| `dial.c:1462-1509` | `net_connected`：`1350` 用 SDK 呼叫态判在线；rx 停摆 120s 兜底；掉线出口 1462-1464 |
| `dial.c:1251-1300` | `reg_check`：`1264/1296` 300s 超时 → `goto reg_timeout_handler`（1268/1300） |
| `dial.c:1861 / 1911` | `reg_timeout_handler` 与 COPS=0 自愈（`1911` forcing COPS=0，仅 FORCE_SIM，提交 `3eefecce`） |
| `dial.c:886 / 1157` | `[SM]` 诊断日志（886）、`[STATE]` 状态变化日志（1157，每变必打） |
| `dial.h:24/34` | `DIAL_TIMEOUT_SECONDS=120`、`REG_CHECK_TIMEOUT_SECONDS=300`（改动③ 需新增 `START_CALL_TIMEOUT_SECONDS`） |
| `status/dial_status.c:21` | `dial_eg25_stat_name`（`[STATE]`/`[SM]` 复用） |
| `is_func_called` 置 true 点 | `72/694/802/837/916/994/1026/1444/1474/1502/1988`（init/roamlink/L1(994)/L2(1026)/net_connected 掉线(1464)等；**cereg/precondition/pre_start_call/start_call 仍无一置 true**——故 start_call 只能靠上游/掉线路径留好的标志） |

---

## 9. 上游对比核实：两个参考仓都没有这个卡死（2026-07-16 grep 逐条核实）

CLAUDE.md 记：**EG25 状态机对齐 `open_dial_for_artery`（artery），恢复阶梯 L1/L2/L3 来自 `open_dial`**。分别核实两个来源仓有没有同款卡死——**结论：都没有，卡死是 rtms-EG25 把两者缝在一起时新引入的。**

判据只需数两件事：谁会把共享钥匙 `is_func_called` 清掉、谁会去调拨号。

| 仓 | `is_func_called` 清 false 的点 | `dail_start_data_call` 活跃调用点 | switch 外额外消费者 | 会卡死？ |
|---|---|---|---|---|
| **artery**（状态机来源） | 1 处（仅 `dail_start_data_call` 成功，342） | 1 处（仅 `start_call`，1085） | **0** | ❌ 不会 |
| **open_dial**（阶梯来源） | **无此标志** | **无此封装**，恢复阶梯直接 `ql_data_call_start(g_call_id)`（`dial.c:798/803/837`） | — | ❌ 不会（阶梯不碰共享钥匙） |
| **rtms-EG25**（改前） | 1 处（285） | **3 处**：`start_call`(1332,switch 内) + **L1(987)+L2(1015)，都在 switch(1155)之前** | **2** | ✅ **会** |
| **rtms-EG25**（改动①②后） | 1 处（285） | **1 处**：仅 `start_call`（L1/L2 已删 `dail_start_data_call`） | **0** | ❌ 不会（= 对齐 artery） |

> **溯源（§3.5，`git` 直证）**：这两个额外消费者由**根因提交 `0310629`（2026-02-11「EG25: 增加三级恢复机制」）**引入，是卡死出生地。改动①② 已把它们删除，rtms-EG25 回到"仅 start_call 一个消费者"，与 artery 一致。

**核实要点（事实）**：
- artery：`is_func_called` 只有一个清点、`dail_start_data_call` 只有一个调用点（就 start_call），**永远只有一个人开门、没人抢钥匙** → 不会卡死。artery 作者在 `dail_start_data_call`（342 行附近）还写了注释：Start 失败要保住 `is_func_called=true`，"否则永久卡死在 start_call"——说明他们**知道**这个卡死点，只是没有第二消费者，从没踩到。
- open_dial：**根本没有 `is_func_called`/`dail_start_data_call` 这套**（用 `stage`+`recovery_level` 结构）；恢复阶梯是**直接调 SDK** `ql_data_call_start(g_call_id)`，不经过任何共享门控标志 → 阶梯与主拨号之间不存在"抢钥匙"。
- rtms-EG25：把 artery 的"`is_func_called` 门控状态机"与 open_dial 的"L1/L2 阶梯"缝合，**而缝合点上 L1/L2 走的是 artery 的 `dail_start_data_call`（消费共享钥匙），不是 open_dial 那样直接调 SDK**。这一步是卡死的引入点：假如当初 EG25 的 L1/L2 也像 open_dial 直接 `ql_data_call_start`，就没有被抢的钥匙、也就不会卡死。

**推论（对 P0 修法的支撑）**：§5 的 P0 治本方向（让 `start_call` 自持 `is_func_called`，或让 L1/L2 不消费该标志）本质就是**把缝合点改回两个上游各自安全的形态之一**——要么回到 artery（只有 start_call 拨号），要么回到 open_dial（阶梯直接调 SDK、不碰门控）。二选一都能根除。

## 10. 日志抽离评估：该抽的已抽尽，不建议再抽（2026-07-16）

承接"把初始化日志抽成集合函数"（已完成 `dial_log_modem_info`/`dial_log_init_static_info`），把 `dial.c` 全部 100 处 `dial_log` 过了一遍，判断还有没有可干净抽离的。**结论：没有了，再抽只会增加小函数、割裂控制流。**

分三类：

| 类别 | 典型 | 能否抽 | 事实依据 |
|---|---|---|---|
| **A. 一次性静态信息块**（连续多行纯打印、固定点触发、中间无控制流） | `[MODEM]`(5 行)、`[INIT]`(8 行) | ✅ **已抽** | 全文件此类**仅此两坨**（连续 ≥2 行 `dial_log`、之间无 `if`/循环/AT 调用） |
| **B. 单行边沿/事件日志**（带运行期局部变量、在分支点触发） | `[STATE]`、`[SM]`、`[CELL]`、`[CELL CHANGE]`、`[HEARTBEAT]`、`[OPER] Try/Selected/failed`、`[ROAMLINK] Switched…`、`[RECOVERY L1/L2/L3]`、`[REG DIAG]` | ❌ 不该抽 | 每条一行、依赖局部变量（old/new state、downtime、p_oper…）；抽出去要么传一大堆参数、要么变几十个一行小函数——正是"太多日志函数" |
| **C. 日志与动作交织**（打印夹在 AT 下发/重试逻辑里） | `[CFUN]` send→fail(retry_nb)→success 序列（1639–1686）、`[REG TIMEOUT]` 处理块（1885–1970，夹 `AT+COPS=0`+策略分支） | ❌ 不该抽 | 抽日志会把 CFUN 重试、COPS=0 动作一起拖出去，是抽逻辑不是抽日志 |

心跳与 `[DIAG]` 快照早已在 `eg25/diag/` 独立模块（`dial_log_heartbeat`/`dial_do_diag_snapshot`）。**A 类抽完后，剩下 98 处非 B 即 C，状态机日志粒度已到位。**

---

## 11. 状态灯缺陷：断网期常亮 / 闪↔常亮抖动 / L2 后假常亮（与 wedge 解耦）

**由 V1（iptables DROP ICMP）测试暴露，2026-07-16。这是既有缺陷，非改动①② 引入，但改动② 让抖动更显眼。**

### 11.1 根因（代码直证）

`net_connected` 状态原来第一行 **无条件** `led_set(handle, ledId, LED_STATE_ON, ...)`（原 `dial.c:1463`）——**灯只跟 `dial_st`（即 SDK 呼叫态）走，完全不看 ping**。故只要 SDK 报"连着"，灯就常亮，哪怕 ping 一直失败。

### 11.2 三个现象（V1 日志直证）

1. **断网期大部分时间常亮**：ping 被 DROP 但数据在走（`rx_packets` 涨）、SDK 报连着 → `dial_st` 多停 net_connected → 常亮，尽管 ConsecFail 一路涨。
2. **每次 L1 触发"常亮↔闪"抖一下**：L1 置 BLINK → 下一轮 net_connected case 的 1463 又置 ON → 同 case 往下 3 行检测到 SDK 掉线又置 BLINK → 走一圈回 net_connected 又 ON。
3. **L2 后彻底假常亮**：L2 的 `CFUN` 中间 `sleep(2/10)` 阻塞单线程循环，期间 SDK 自动重连，`dial_st` 从未离开 net_connected → 恒常亮。**最硬的证据**：04:33:02 那条 `[LED] 1->2` 之后到测试结束，灯纹丝不动，却横跨了"ping 失败(04:33–04:35:26)"与"ping 恢复(04:35:26 后)"两种相反状态——**证明灯完全不反映 ping**。

同根问题：这也是 §5 P1「`net_connected` 过度相信 SDK 呼叫态」在灯上的体现；另附带"SDK 连着但 ping 不通时每 5min 无谓 CFUN"循环（同为既有、非本次引入）。

### 11.3 修复（改动④，2026-07-16 已应用，未编译未提交）

把 1463 改为**按 ping 判**：
```c
led_set(handle, ledId,
        (start_fail_ts == 0) ? LED_STATE_ON : LED_STATE_BLINK,
        &lastLEDState);
```
`start_fail_ts != 0`（ping 故障计时进行中）→ 闪；`==0`（真 ping 通）→ 常亮。与恢复阶梯同用 `start_fail_ts` 判在线，口径一致。**边界**：`start_fail_ts` 连挂 3 次(~90s)才置，故断网头 ~90s 仍常亮再转闪（去抖，可接受）。

### 11.4 改动④ 真机验证结果（2026-07-16，通过）

新固件（含改动④）iptables DROP 测试，灯行为与旧固件形成 A/B：

| | 旧固件（04:22） | 新固件＝改动④（05:05） |
|---|---|---|
| 3 连挂、`start_fail_ts` 置 | 04:22:31 灯 2→1（闪） | 05:05:41 灯 2→1（闪） |
| 紧接 | **04:22:33 灯 1→2 又常亮**（1463 拽回） | —— 无 |
| 断网期（跨多次 L1） | 每次 L1 闪↔常亮抖 | **05:05:41→05:10:08 共 4.5min、跨 3 次 L1，零 LED 变化，稳定闪** |
| ping 恢复 | — | **05:10:08 灯 1→2 常亮**（`Network recovered after 277s`） |

**结论**：断网确认后到 ping 恢复之间，中间 3 次 L1（05:06:50/07:59/09:08）、`dial_st` 反复 net_connected↔reg_check，**灯一条日志都没有＝稳定闪不抖**；ping 恢复即常亮。去抖边界也符合（iptables 后 ping 先通到 05:04:31 才首挂，05:05:41 三连挂才置 `start_fail_ts` 转闪）。**改动④ 真机直证通过。** 另本轮 `[SM]` 全程 `is_func_called=0`（net_connected 处）——L1 路径每次经 start_call 消费钥匙、不留滞留，印证 §6.6 的滞留是 L2-SDK自动重连特有。

**L2 后不假常亮＝已直证（2026-07-16 补测）**：iptables 持续 >10min，触发 4×L1 + **2×L2 CFUN**（05:19:40 downtime 311s、05:24:50 downtime 620s）。灯从 **05:14:30 `state 2->1`（转闪）到 05:25:50 `state 1->2`（ping 恢复常亮）之间零 LED 变化**——跨两次 L2 CFUN 始终稳定闪。对比旧固件上一轮 L2 后 04:33:02 变常亮（假常亮）。**改动④ 全覆盖：L1 循环期 + L2 循环期 + 恢复，LED 均正确。** 同轮再确认改动①②（L1×4+L2×2 无卡死）与 `is_func_called=1` L2 后良性滞留。

---

*本文档由现网日志 + 源码 + 2026-07-16 真机 `[SM]` 测试交叉核实生成。核心结论（start_call 卡死、is_func_called 诱因、L2 抢跑、冷启动自愈生效、COPS 存 NV）均已【真机直证】（§3.4）；仍标【强推断】/【未定】/【待验】者（回落 mode 未复位、CEER 解码、present-but-rejecting 场景、卡死修复回归）请坐实后再作定论。*
