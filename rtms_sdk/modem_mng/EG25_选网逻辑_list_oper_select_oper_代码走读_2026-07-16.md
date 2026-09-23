# EG25 手动选网逻辑（`list_oper` / `select_oper` / `[OPER]`）代码走读

> 目标：把"数据拨号失败后，状态机怎么一步步选运营商、失败怎么回落"讲清楚。
> 范围：`eg25/dial/dial.c` 的 `wait_for_connect` 超时分支 + `list_oper` + `select_oper` + 下游 `stop_cfun`/`start_cfun` + `reg_timeout_handler` 的 COPS=0 自愈；候选表解析在 `eg25/nw/nw.c`。
> 证据口径：【事实·已读代码】逐行读过并标行号；【推断】为 3GPP/模组行为常识，非本代码所证；【待验】未在真机复现（本卡为澳门 MVNO 漫游，网络忽略 APN，造不出"注册上但数据不通"，故本逻辑**真机未触发**，本文为纯代码走读）。行号基准：2026-07-16 工作区（含本轮 `[APN]`/`[COPS]`/`[SM]` 门控等改动）。

---

## 0. 一句话

**数据拨号 120s 连不上 → `AT+COPS=?` 扫出周边可用网（只收 `stat==1`，排除当前失败网）→ 逐个 `AT+COPS=1,2,X` 试锁 + CFUN 复位重拨验证数据 → 通了就停在那家（手动锁 COPS=1）；不通就删了试下一个；全删光就复位重扫。注册不上会经 `reg_timeout` 发 `AT+COPS=0` 掰回自动，但"能注册却没数据"够不到这条回落。**

---

## 1. 触发点：`wait_for_connect` 拨号超时（`dial.c:1478-1502`）【事实】

`dial_stat_wait_for_connect` 里，数据呼叫迟迟不 connected：

- `1481-1482`：`dif_timer > DIAL_TIMEOUT_SECONDS`（=**120s**，`dial.h:23`）即超时；
- `1484-1485`：`dail_stop_data_call(profile_idx)` + `is_func_called = true`（下次重走 Init+Start）；
- `1490-1496`：**若 `is_oper_select==false`**（非手动选网）→ 发 `AT+COPS=0`（60s）掰回自动 + 打 `[OPER] !is_oper_select dial timeout: AT+COPS=0`；
- `1498`：分派
  - `is_oper_select==true`（**默认 true**，`apn.c:501`）→ `dial_st = dial_stat_list_oper`；
  - `is_oper_select==false` → `dial_st = dial_stat_reg_check`。

> 即：**只有 `is_oper_select=true` 的卡才进选网逻辑**；非选网卡超时只做 COPS=0 回自动 + 回注册。

---

## 2. `list_oper`：扫网 + 建候选表（`dial.c:1556-1615`）【事实】

1. **队列空**（`cc_deque_size==0`，`1557`）→ 发 **`AT+COPS=?`**（`1562`，超时 **180000ms**）：
   - 失败：`retry_nb++`，到 `AT_RETRY_NB` 上限 → `dial_st=reg_check`（`1568-1572`），`sleep(5)` 后 `continue`；
   - 成功：`retry_nb=0`（`1577`），`parse_oper_list_info(deque_oper, rsp)`（`1578`）解析入队。
2. **队列非空**（`1580`）→ 拼候选列表字符串（夹到 512 缓冲防溢出，`1588-1606`）→ 打 **`[OPER] Candidate list (N): 46001,46000,…`**（`1607`）→ `dial_st=select_oper`（`1609`）。
3. **扫完仍空**（周边无其它可用网）→ `dial_st=reg_check`（`1613`）。

### 2.1 候选表怎么来的：`parse_oper_list_info`（`nw.c:537-592`）【事实】

`AT+COPS=?` 应答形如 `+COPS: (2,"CHINA MOBILE","CMCC","46000",0),(1,"CHN-UNICOM","UNICOM","46001",2),…`。函数按 `(` `)` 切每个候选，`parse_oper_info_str` 解析出 `stat / 长名 / 短名 / 数字码 / AcT`：

- **`nw.c:586` 只有 `if (stat == 1)` 才 `cc_deque_add_last` 入队**（存**数字 PLMN 码**，如 `46001`）。
- 即：**只收 `stat==1` 的运营商**；非 1 一律不收。

> 【推断·3GPP 27.007】`stat`：0=unknown，1=available，2=current（当前正注册的、也就是刚才数据失败的那家），3=forbidden。故"只收 1"= **排除当前失败网与禁止网，只留其它可用网**。代码本身只证了"只收 stat==1"，各值含义是 3GPP 常识，非本代码所证。

---

## 3. `select_oper`：逐个试锁（`dial.c:1616-1695`）【事实】

取队列**第一个** `X`（`1623` peek index 0；空则显式置 `p_oper=NULL` 防悬空指针，`1623-1626`）。若 `X != NULL`：

1. 打 `[OPER] Try operator X`（`1629`）→ 拼 `AT+COPS=1,2,X`（`1630`，格式 2=数字码）→ 发（`1639`，超时 **60000ms**）。
2. **失败**（`Ql_SendAT != 0`，`1639`）：
   - 打 `[OPER] AT+COPS=1,2,X failed, rsp="…"`（`1648`，空串=host 等满超时放弃、命令可能仍在模组侧执行；非空=模组明确返回错误）；
   - `retry_nb++`（`1650`）：
     - **未到上限** → `continue`（`1672`，5s 后再试**同一个 X**）；
     - **到 `AT_RETRY_NB` 上限** → 打 `[OPER] Remove operator X after retry failure`（`1655`），从队列 `cc_deque_remove_first` + `free`（`1656-1660`）：
       - 队列**空了** → `dial_st = start_cfun`（`1664`）；
       - 队列**还有** → `dial_st = list_oper`（`1668`，此时队列非空、不重扫，直接又进 select 试下一个）。
3. **成功**（`AT+COPS=1,2,X` 回 OK，`1674`）：
   - `retry_nb=0`（`1675`），打 `[OPER] Selected operator X, response: …`（`1677`），从队列删除 + `free`（`1679-1682`），`p_oper=NULL`。
   - 落到 `1693`：`dial_st = dial_stat_stop_cfun`。
4. `X == NULL`（队列空，正常路径不可达）：打 `[OPER] select_oper entered with no candidate operator (deque empty)`（`1691`），仍 `dial_st=stop_cfun`（`1693`）。

> 【推断·3GPP】`AT+COPS=1,2,X` 回 OK 表示模组**接受并（尝试）注册**到 X，不代表数据通；数据通不通要靠随后 CFUN 重拨再验。代码只证"OK 则进 stop_cfun 重拨流程"，"OK=注册成功"是 3GPP 行为推断。

---

## 4. 下游：`stop_cfun` → `start_cfun` → 重拨（`dial.c:1696-1746`）【事实】

`select_oper` 成功后：

- `stop_cfun`（`1696`）：发 `AT+CFUN=0`（`1701`，15s；失败重试到上限回 reg_check），成功 → `dial_st=start_cfun`（`1719`）。**本状态只有 CFUN、没有 COPS 动作**——手动锁靠模组 NV 跨 CFUN 保留。
- `start_cfun`（`1721`）：发 `AT+CFUN=1`（`1726`），成功 → `dial_st = dial_stat_sim_init`（`1741`）。

即：**射频复位（0→1）应用手动锁 → 从 `sim_init` 重走整条拨号**（sim_init→sim_check→sim_op→reg_check→cereg→precondition→pre_start_call→start_call→wait_for_connect），此时 COPS 锁在 X 上，重新注册 X 并重拨。

- **X 能注册但数据不通** → 又 120s → 回 `list_oper`（试下一个；X 已被移除，不重复）；
- **X 注册不上** → `reg_check` 停满 300s → `goto reg_timeout_handler`（见 §5）。

---

## 5. 回落：`reg_timeout_handler` 的 COPS=0 自愈（`dial.c:1909-2038`）【事实】

`reg_check`/`cereg_check` 停满 `REG_CHECK_TIMEOUT_SECONDS`（=**300s**，`dial.h:34`）→ `goto reg_timeout_handler`：

1. **`1936-1968`：FORCE_SIM 的 COPS=0 自愈（无条件先跑，不看 has_connected_once）**：
   - `AT+COPS?`（`1940`）→ 若含 `+COPS: 0` → 打 `[REG TIMEOUT] COPS already auto, skip`（`1944`，幂等）；
   - 否则 → 打 `[REG TIMEOUT] COPS not auto (…), forcing COPS=0`（`1959`，原始应答去 `\r\n`）→ 发 `AT+COPS=0`（`1964`，30s）→ 打 `[REG TIMEOUT] AT+COPS=0 rsp: …`（`1966`）。
   - **这就是把 `AT+COPS=1,2,X` 锁死后掰回自动的地方**（提交 `3eefecce`）。
2. `1970-1991`：策略 1/2 + roamlink 可用 + 非 license_pending → 切 Roamlink。
3. `1992-2005`：**FORCE_SIM && has_connected_once** → 打 `[REG TIMEOUT] policy=4 (connected): defer cfun reset to L2 ladder` → **让位给 L1/L2/L3 的 L2**（只重置 dial_timer、留在 reg_check，等 L2 在 downtime≥5min 做 CFUN），避免与 L2 双 CFUN。
4. `2006-2038`：else（**FORCE_SIM 冷启动未连过** / roamlink 不可用 / license_pending）→ 打 `[REG DIAG] cold-start no-reg …` 快照 → `is_func_called=true` + `dial_st=stop_cfun`（CFUN 复位重来）。

### 5.1 COPS=0 到底何时"真回落"——可达性两道限制【事实 + 一处推断】

COPS=0 块虽在 reg_timeout 头部无条件跑，但**得先"够到" reg_timeout**：

- **限制 A【事实】**：只有卡在 `reg_check` 停满 300s 才 `goto`。**"能注册但数据不通"** 走的是 `wait_for_connect → list_oper`，**根本够不到 reg_timeout → 不会 COPS=0**。
- **限制 B（2026-07-16 重审并纠正，见 §6.2）**：**改动①② 后 L2 不"抢跑"COPS=0**。`reg_check` 的 dwell 计时入口设一次、中途不重置；**L2（改动① 版）只发 CFUN、不改 dial_st、不重置 dial_timer**（`dial.c:1016-1017` 注释「L2 复位后 dial_st 维持原状」）。故模组 CFUN 后若仍**注册不上**，reg_check 一路 dwell 到 300s → reg_timeout → COPS=0，L2 抢不走。**L2 唯一"提前结束 reg_check dwell"的途径是它的 CFUN 让模组重新注册成功**——那时 reg_check 提前离开、COPS=0 不触发**也不需要**（因为已恢复）。所以 L2 与 COPS=0 是**互补**：CFUN 先试，救不回才由 COPS=0 清 NV 手动锁兜底。*旧固件"L2 抢跑、连过设备 COPS=0 永远触发不了"是**卡死 bug**（L2 `dail_start_data_call` 逼 reg_check 通过→冲 start_call 卡死→够不到 reg_timeout）造成的假象，改动①② 已消除，见 §6.2。*

**精确结论（改动①② 后）**：
| 场景 | 是否 COPS=0 回落 |
|---|---|
| 锁到的网**注册不上** + 冷启动（未连过，L1/L2/L3 门控关） | ✅ reg_timeout → COPS=0，会救（真机 Run 2 已证） |
| 锁到的网 **CFUN 能救回**（重注册成功，真机 Run A：锁 23415→camp 回移动） | reg_check 提前离开 → COPS=0 不触发**也不需要**（L2/CFUN 已恢复） |
| 锁到的网 **CFUN 救不回**（CFUN 后仍注册不上）+ 已连过 | ✅ reg_check dwell 满 300s → reg_timeout → COPS=0（**代码可达，未真机复现**——23415 CFUN 总能救回，没造出此情形） |
| **能注册但没数据**（任一候选网） | ❌ 在 list_oper/select_oper 里循环，够不到 reg_timeout，COPS 长期手动锁、不回落 |

---

## 6. 完整流程图

```
wait_for_connect ──120s 未连上(dial.c:1482)──> stop data call + is_func_called=true
   │
   ├─ is_oper_select=false ─> AT+COPS=0(回自动) ─> reg_check
   │
   └─ is_oper_select=true ─> list_oper
        │
        ├─ 队列空 ─> AT+COPS=? 扫网(180s)
        │     ├─ 失败×AT_RETRY_NB ─> reg_check
        │     └─ 成功 ─> parse(只收 stat==1 的数字码入队)
        ├─ 扫完仍空 ─> reg_check
        └─ 有候选 ─> [OPER] Candidate list ─> select_oper
             │
             取队首 X ─> AT+COPS=1,2,X (60s)
             ├─ 失败×AT_RETRY_NB ─> 删X ─┬─ 队列还有 ─> list_oper(试下一个)
             │                            └─ 队列空   ─> start_cfun ─> sim_init ─> 重走拨号
             └─ 成功 ─> [OPER] Selected ─> 删X ─> stop_cfun ─> start_cfun ─> sim_init ─> 重走拨号(COPS 锁在X)
                                                                                  │
                            ┌─────────────────────────────────────────────────────┤
                            │                                                     │
                    X 能注册但数据不通                                     X 注册不上
                            │                                                     │
                    120s ─> list_oper(试下一个)                    reg_check 停 300s ─> reg_timeout
                            │                                                     │
                    (循环，COPS 长期手动锁，                          FORCE_SIM: AT+COPS=0(掰回自动)
                     够不到 reg_timeout，不 COPS=0)                    然后按 has_connected_once 分派 CFUN/让位L2
```

### 6.1 选网时序图（一次成功选网 + CFUN 重拨）

```
时间→   状态机                模组(AT)                  网络/结果
 t0     wait_for_connect ......(数据呼叫迟迟不 connected).......
 t0+120 │  dail_stop_data_call
        │  is_func_called=true
        ▼
        list_oper ───────────► AT+COPS=?（≤180s 扫网）───────► 返回 (2,"本网",…),(1,"甲",…),(1,"乙",…)
        │  parse: 只收 stat==1  ◄──────────────────────────────  入队: [甲, 乙]
        │  [OPER] Candidate list (2): 甲,乙
        ▼
        select_oper ─────────► AT+COPS=1,2,甲（≤60s）─────────► OK? 
        │  [OPER] Try operator 甲
        │  成功 ─ [OPER] Selected 甲 ─ 删甲(队列剩[乙])
        ▼
        stop_cfun ───────────► AT+CFUN=0 ──────────────────────► 射频关
        start_cfun ──────────► AT+CFUN=1 ──────────────────────► 射频开，按"手动锁甲"重注册
        ▼
        sim_init→…→reg_check→…→start_call→wait_for_connect（COPS 锁在 甲）
        ▼
        ├─ 甲 数据通 ─► net_connected（停在甲，手动锁 COPS=1）✔
        └─ 甲 数据不通 ─120s─► list_oper（队列[乙]非空，不重扫）─► 试乙 …
```

### 6.2 时序图：L2 与 reg_timeout 的 COPS=0 谁先到（回答"COPS=0 还有没有意义"）

**关键事实**：`reg_check` 的 dwell 计时（`dial_timer`）入口设一次、中途不重置；**L2（改动① 版）只发 CFUN、不改 dial_st、不重置 dial_timer**（`dial.c:1016-1017` 注释：「L2 复位后 dial_st 维持原状」）。所以只要模组 CFUN 后仍**注册不上**，reg_check 就一路 dwell 到 300s → reg_timeout → COPS=0，**L2 抢不走**。

```
连过的设备(has_connected_once=1)被手动锁到"CFUN 也救不回"的网：

downtime(ping)  ├──────────────────300s──────────────────►L2(CFUN=0/1)──►(仍注册不上)──►L2…每5min
reg_check dwell        ├──────────────────300s──────────────────────────►reg_timeout
                       (入口设 dial_timer，L2 不重置它)                        │
                                                                    【AT+COPS=0 掰回自动】←清 NV 手动锁
                                                                    (1936，无条件先跑；1992 仅跳过第二次CFUN)
                       ▲ L2 的 CFUN 若"救回"(重新注册成功) → reg_check 提前离开、够不到 reg_timeout、
                         COPS=0 不触发也不需要(真机 Run A: 锁 23415，CFUN 后 camp 回移动即属此列)
```

**对比·旧固件(有卡死 bug)**：L2 会 `dail_start_data_call` 强拉数据呼叫 → 逼 reg_check 通过 → 冲到 `start_call` **卡死** → 永远够不到 reg_timeout → COPS=0 触发不了。**这才是"COPS=0 对连过设备永远触发不了"的真相——是卡死 bug 挡住的，不是设计如此。改动①② 移除该 bug 后，COPS=0 兜底重新可达。**

---

## 7. 证据等级汇总

| 结论 | 等级 |
|---|---|
| 触发点、list_oper、select_oper、stop/start_cfun、reg_timeout COPS=0 的**代码流程与行号** | **事实·已读代码** |
| `parse_oper_list_info` 只收 `stat==1` 入队（数字码） | **事实**（`nw.c:586`） |
| reg_timeout 头部对 FORCE_SIM 无条件跑 COPS=0（不看 has_connected_once） | **事实**（`dial.c:1936`） |
| "能注册但数据不通"够不到 reg_timeout、不 COPS=0 | **事实**（限制 A，控制流可证） |
| `stat` 2=current/3=forbidden → "排除当前/禁止网" | **推断·3GPP**（代码只判 `==1`） |
| `AT+COPS=1,2,X` 回 OK = 注册成功（非数据通） | **推断·3GPP** |
| has_connected_once=1 时 L2 抢跑 reg_timeout 截胡 COPS=0（在 [OPER] 循环内） | **推断**（未逐周期追 `start_fail_ts`） |
| 整条逻辑真机行为 | **未验**（本卡漫游网络忽略 APN，造不出触发条件；纯代码走读） |

---

## 8. 行为边界 / 可加固点（非崩溃 bug）

- **无明显崩溃/悬空类 bug**：队列空显式置 `p_oper=NULL` 防悬空、候选拼串夹缓冲防溢出、`retry_nb` 跨状态清零、失败原始应答落 SD——防御到位。
- **边界（可加固点，2026-07-16 评估后「未采纳」）**：**"所有可用网都能注册、但都不给数据"** 时，`is_oper_select=true` 设备会在 `list_oper`/`select_oper` 里**无限试网、COPS 长期手动锁**——每轮都注册成功、reg_check 够不到 300s dwell，故 reg_timeout 的 COPS=0 永远够不到，永不回自动。曾实现过一版"试穷 3 轮 → `AT+COPS=0` 回自动"的对称出口（跨轮计数器 + give-up 闸，已交叉编译通过），**2026-07-16 决定回退**，理由：
  1. **治的病没见过**：该场景现网零观测、零日志。违反 `session/EG25_德国设备COPS锁死_根因调研全记录_2026-07-03.md` §35.3 的"避免为不存在的需求做设计"。
  2. **动存量**：改的是 `is_oper_select=true` 设备的行为，而按该文档 §37 的实证，那是**整个机队**（无一条 apn.json 显式配过该键，全吃 `apn.c:501` 缺省 true）。违反 §36 定下的"不动存量"约束。
  3. **与德国那台无关**：德国案是**注册失败**路（reg_check→reg_timeout→COPS=0），本兜底挂在**数据失败**路（wait_for_connect→list_oper），完全碰不到。
  4. 阈值（3 轮）无任何依据，真机从未触发过。
  - **真问题在上游**：若 [OPER] 确在"乱锁网"（§36.1 的 F1+F2：无视配置、扫一圈、锁第一个扫到的），正解在**别让它锁**，而不是"锁完 N 轮后放它走"。
  - 局限（若将来重提）：**[OPER] 只换运营商、不换 APN**；若"没数据"根因是 APN/账户，COPS=0 回自动也救不回数据——该兜底价值只是"不再手动锁死在失败网上"，非"恢复数据"。
- **小观察**：候选全删空走 `start_cfun`（只 `CFUN=1`），比成功路径 `stop_cfun→start_cfun`（`CFUN=0→1` 全循环）少一步 `CFUN=0`，轻微不对称，无害。

---

## 9. 关键代码位置索引

| 位置 | 说明 |
|---|---|
| `dial.c:1478-1502` | `wait_for_connect` 120s 超时分派（`!is_oper_select`→COPS=0+reg_check；`is_oper_select`→list_oper） |
| `dial.c:1556-1615` | `list_oper`：`AT+COPS=?`(1562,180s) / parse(1578) / 候选表日志(1607) / →select_oper(1609) / 空→reg_check(1613) |
| ~~§8 兜底相关行号~~ | ~~give-up 闸~~ —— **已于 2026-07-16 回退，代码不存在**（理由见 §8） |
| `nw.c:537-592` | `parse_oper_list_info`：`if(stat==1)`(586) 入队数字码 |
| `dial.c:1616-1695` | `select_oper`：Try(1629) / `AT+COPS=1,2,X`(1639,60s) / 失败删+试下一个(1650-1669) / 成功→stop_cfun(1693) |
| `dial.c:1696-1720` | `stop_cfun`：`AT+CFUN=0` → start_cfun（无 COPS） |
| `dial.c:1721-1746` | `start_cfun`：`AT+CFUN=1` → `sim_init`(1741) |
| `dial.c:1936-1968` | `reg_timeout` 的 **FORCE_SIM COPS=0 自愈**（无条件先跑，`3eefecce`） |
| `dial.c:1992-2005` | reg_timeout：FORCE_SIM && 已连过 → 让位 L2 |
| `dial.c:2006-2038` | reg_timeout：FORCE_SIM 冷启动/roamlink不可用/license_pending → `[REG DIAG]` + CFUN 复位 |
| `dial.h:23/34` | `DIAL_TIMEOUT_SECONDS=120`、`REG_CHECK_TIMEOUT_SECONDS=300` |
| `apn.c:501` | `is_oper_select` 缺省 true |

---

*本文为 2026-07-16 纯代码走读（`eg25/dial/dial.c` + `eg25/nw/nw.c`），逐行读过并标行号。标【推断】者为 3GPP/模组行为常识、非本代码所证；整条逻辑因本卡（澳门 MVNO 漫游、网络忽略 APN）无法造出触发条件而**真机未验证**。*
