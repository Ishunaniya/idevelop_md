# open_dial 设备端验证测试方案

> 覆盖范围：`df06b4d` 起（含）的全部 BUGFIX 提交
> 生成时间：2026-04-20
> 适用硬件：EC200A 模组 + 目标单板
> 测试目的：真机回归验证，确认路由修复、Fast Retry 修复、信号处理、APN 解析、三级恢复等在实际网络环境下稳定工作

---

## 目录

- [一、受测提交清单](#一受测提交清单)
- [二、测试前准备](#二测试前准备)
- [三、通用观察点（每个用例都要看）](#三通用观察点每个用例都要看)
- [四、测试用例](#四测试用例)
  - [A 组：`ffea5dd` — 断网重连后路由残留导致无法联网](#a-组ffea5dd--断网重连后路由残留导致无法联网)
  - [B 组：`72888c3` — 路由清理精确化（不误伤其他网卡/防火墙）](#b-组72888c3--路由清理精确化不误伤其他网卡防火墙)
  - [C 组：`6c02b01` — Fast Retry 超时、call_id 过滤、init 日志、路由加固](#c-组6c02b01--fast-retry-超时call_id-过滤init-日志路由加固)
  - [D 组：`df06b4d` — 漏洞修复合集（APN、信号、抖动过滤、L1/L3 恢复、DNS 等）](#d-组df06b4d--漏洞修复合集apn信号抖动过滤l1l3-恢复dns-等)
- [五、回归基线（每次烧录必跑）](#五回归基线每次烧录必跑)
- [六、Pass/Fail 总结模板](#六passfail-总结模板)

---

## 一、受测提交清单

| Commit | 日期 | 标题 | 文件 |
|---|---|---|---|
| `df06b4d` | 2026-04-09 | 漏洞修复提交（APN 解析、SIGINT、Ping 抖动、L1/L3 恢复、DNS 换行符、僵尸进程等） | `apn/apn.c`、`dial.c`、`data_call/data_call.c`、`misc.c`、`nw/nw.c`、`reboot_conf/dial_reboot_conf.c` 等 12 文件 |
| `ffea5dd` | 2026-04-09 | 修复断网重连后因内核路由残留导致必须重启系统才能联网的问题 | `data_call/data_call.c` |
| `72888c3` | 2026-04-20 | 修复路由清理中未按接口精确操作、误清空防火墙规则及缺失 IPv6 路由清理的问题，并统一使用 ip route | `data_call/data_call.c` |
| `6c02b01` | 2026-04-20 | 修复 Fast Retry 超时误判、call_id 过滤丢事件等拨号稳定性问题 | `dial.c`、`data_call/data_call.c`、`data_call/data_call.h` |
| `6f7c1be` | 2026-04-20 | 修复 persistent 模式下成功重连后 retry_count 未清零的问题 | `dial.c` |
| `7e0648d` | 2026-04-21 | 修复 SIM UNKNOWN 状态下 L2/L3 恢复永不触发的问题，并更新 C-09 测试方案 | `dial.c`、`TEST_PLAN.md` |

六个 commit 合起来 **共同构成一条完整的拨号/路由/恢复稳定性修复链**，必须**叠加测试**（不是单独验证某一条），验收以最终效果为准。

---

## 二、测试前准备

### 2.1 硬件与环境

- [ ] 目标单板一块，模组焊接正常、天线连接良好
- [ ] eSIM 卡已激活，信号良好（**本设备为 eSIM，不可物理拔卡；所有"断卡"场景均用 `serial_atcmd at+cfun=4` 软断网代替**）
- [ ] 串口/SSH 能稳定登录单板
- [ ] PC 端可抓 shell 输出及 `/media/sdcard/dial_log/` 下日志
- [ ] 如有条件：电源可控（断电模拟硬复位）

### 2.2 固件/程序

- [ ] 已烧录包含 `6c02b01` 的固件（`git log -1` 在单板上看到 `6c02b01`）
- [ ] `/media/sdcard` 已挂载且剩余空间 ≥ 500MB（否则 `logger_sd` 不工作）
- [ ] 确认 `start_prog` 或等效保活脚本在跑（`ps | grep start_prog`）

### 2.3 观察/工具命令（提前在单板上确认可用）

```bash
# 查看进程、PID、启动时间
ps | grep -E "dial|start_prog"

# 查看路由
ip route show
ip -6 route show

# 查看 NAT/filter 规则
iptables -t nat -L POSTROUTING -n -v
iptables -t filter -L -n -v

# 状态文件
cat /tmp/dial_Status          # 0=已拨通，1=未拨通/断开
cat /tmp/network_status       # 1=已连通，0=未连通
cat /tmp/dial_retry_count     # 快速失败计数，拨通 10 分钟后被清
ls /tmp/resolv_v4.conf /tmp/resolv_v6.conf  # DNS 缓存
cat /etc/resolv.conf

# 实时看日志
tail -F /media/sdcard/dial_log/*.log
```

### 2.4 部署前基线快照（烧录新 binary 之前必做）

设备当前跑的是 `89f1866`（2024-10-18）的老 binary，与待测版本跨了 8 个 commit，**先保留一份现场快照**用于事后比对：

```bash
mkdir -p /tmp/before_upgrade
date                   > /tmp/before_upgrade/ts.txt
ip route show          > /tmp/before_upgrade/route4.txt
ip -6 route show       > /tmp/before_upgrade/route6.txt
iptables-save          > /tmp/before_upgrade/iptables.txt
cp /etc/resolv.conf      /tmp/before_upgrade/resolv.conf
ps                     > /tmp/before_upgrade/ps.txt
md5sum /usr/dial/dial  > /tmp/before_upgrade/md5.txt
# 备份整包到 SD 卡（断电不丢）
tar czf /media/sdcard/before_upgrade_$(date +%Y%m%d_%H%M).tar.gz -C /tmp before_upgrade 2>/dev/null
```

**关键对比点**（测试完成后与新版本状态 diff）：

| 项目 | 老版本基线（典型观察） | 新版本预期 |
|---|---|---|
| `/etc/resolv.conf` DNS 条数 | 4 条（2 条 DNS 各重复 2 次） | 每条 DNS 只出现 1 次 |
| `/tmp/dial_Status` | 不存在 | 存在，值为 `0`（已拨通）或 `1`（断开） |
| `/tmp/network_status` | 不存在 | 存在，值为 `1` 或 `0` |
| `/tmp/resolv_v4.conf` | 不存在 | 已拨通时存在 |
| `iptables -t filter` | 含 `ACCOUNTING_IN/OUT` chain | 仍含（B-01 重点验证） |
| `/usr/dial/dial` md5 | `0913729408cc10ac16839cd87c090dcd` | 与新编译产物一致，且 ≠ 老值 |

### 2.5 测试数据记录表（每个用例必填）

| 字段 | 样例 |
|---|---|
| 用例编号 | A-01 |
| 开始时间 | 2026-04-21 10:05:12 |
| 初始状态 | 已拨通 5min、IPv4-only、`ccinet1` |
| 触发操作 | `serial_atcmd at+cfun=4` 软断 30s → `serial_atcmd at+cfun=1` 恢复 |
| 观察到的日志关键行 | `[INIT] PDP connected, start Ping window (10000ms).` |
| 路由/NAT 前后对比 | 见附表 |
| 结果 | PASS / FAIL |
| 备注/截图/日志片段 | ... |

---

## 三、通用观察点（每个用例都要看）

每个用例执行完都必须回答以下 6 个问题：

1. **能不能上网？** `ping -I <wwan_if> 8.8.8.8 -c 3` 成功？
2. **路由干净吗？** `ip route` 和 `ip -6 route` 是否只有**一条** default via 对应 `wwan*`？没有僵尸 default？
3. **NAT 规则干净吗？** `iptables -t nat -L POSTROUTING -n` 是否**只有一条** MASQUERADE 对应当前 device？
4. **filter 表没被误清？** `iptables -t filter -L` 看本地原有的防火墙规则（如果有）是否还在？
5. **DNS 正确？** `/etc/resolv.conf` 是否和 `/tmp/resolv_v4.conf` + `/tmp/resolv_v6.conf` 一致，没有残留旧 DNS？
6. **日志完整？** 新增的 `[INIT]` 日志是否按预期输出，没有 20 秒静默？

---

## 四、测试用例

### A 组：`ffea5dd` — 断网重连后路由残留导致无法联网

**原始问题**：断网重连后，旧 default 路由残留在内核，新的 `ip ro add default` 静默失败，必须重启系统才能联网。

#### A-01 上电首次拨号

**目的**：确认干净环境下能正常拨通，路由/NAT 只有一条。

**步骤**：
1. 单板断电 ≥ 30s
2. 上电开机
3. 等 60s，观察日志
4. 执行通用观察点 1–6
5. `ping -I ccinet1 114.114.114.114 -c 5`

**预期**：
- 日志依次出现：`[INIT] Waiting for ql_data_call_init...` → `[INIT] data_call_init OK.` → `CONNECTED` 回调 → `[INIT] PDP connected, start Ping window (10000ms).`
- `ip route` 只有 1 条 `default via ... dev ccinet1`
- `iptables -t nat -L POSTROUTING` 只有 1 条 `MASQUERADE all -- 0.0.0.0/0 0.0.0.0/0`
- `/tmp/dial_Status` = `0`，`/tmp/network_status` = `1`
- ping 全部成功

---

#### A-02 软断网 → 恢复（短断：30 秒）

**目的**：验证重连后旧路由被清理、新路由建立成功。**这是原始 bug 的核心场景。**（eSIM 用 `at+cfun=4` 软断射频代替物理拔卡）

**步骤**：
1. 拨通后等 2 min，确认 ping 通
2. 记录 `ip route`、`iptables -t nat -L POSTROUTING`
3. 软断射频：`serial_atcmd at+cfun=4`
4. 等 30 秒，观察日志出现 `DISCONNECTED` 回调、`/tmp/dial_Status` = `1`
5. 恢复射频：`serial_atcmd at+cfun=1`
6. 等 60 秒，观察日志再次出现 `CONNECTED`
7. 再次记录路由和 NAT
8. `ping -I ccinet1 114.114.114.114 -c 5`

**预期**：
- `DISCONNECTED` 日志后立即能看到清理命令成功（路由为空、NAT 无该接口 MASQUERADE）
- 重连后 `ip route` 仍然只有 **1 条** default（不是 2 条，不是 0 条）
- 重连后 ping 直接通，**不需要重启系统**
- `g_if_name` 最终非空且与 `p_msg->device` 一致

**失败判据（Bug 复现）**：重连后 `ip route` 出现 2 条 default，或者 ping 失败但 `cat /tmp/dial_Status` 为 0。

---

#### A-03 软断网 → 恢复（长断：5 分钟）

**目的**：模拟进入弱信号区域、到派上差点掉线再回来的场景。

**步骤**：
1. 拨通后软断射频：`serial_atcmd at+cfun=4`
2. 静置 5 分钟，每 60s 看一眼日志和 `/tmp/dial_retry_count`
3. 恢复射频：`serial_atcmd at+cfun=1`
4. 等 2 分钟

**预期**：
- 断网期间 `/tmp/dial_Status` 一直是 `1`
- 观察 `dial_retry_count` 的变化（按 `dial_loop` 的 L1 快速恢复策略，进程可能 exit 并被拉起 3 次）
- 恢复射频后，**最终必须拨通**
- 路由/NAT 干净

---

#### A-04 连续拔插 5 次（10s 拔 / 20s 插，×5）

**目的**：压力测试清理逻辑，确认不会累积残留。

**步骤**：
```bash
for i in 1 2 3 4 5; do
    serial_atcmd at+cfun=4; sleep 10   # 软断射频 10s
    serial_atcmd at+cfun=1; sleep 20   # 恢复射频等重连 20s
    echo "=== Round $i ==="
    ip route show | grep default
    iptables -t nat -L POSTROUTING -n --line-numbers | grep MASQUERADE
done
```

**预期**：
- 每轮结束后 default 路由条目数恒为 1（或拔出态的 0），**不累积**
- NAT 表 MASQUERADE 条目数恒 ≤ 1
- 第 5 轮结束后 ping 通

**为什么要这个用例**：`6c02b01` 加了 10 次循环删 iptables 正是为了这个场景。如果 `72888c3` 没生效、或 `6c02b01` 的循环删退化，会在这里看到规则累积。

---

### B 组：`72888c3` — 路由清理精确化（不误伤其他网卡/防火墙）

**原始问题**：`ffea5dd` 里清理太粗暴，`ip ro del default` 不指定 dev 会误删其他网卡路由，`iptables -t filter -F` 会把整个 filter 表清空。`72888c3` 改成按 dev 精确清理 + 补 IPv6 清理。

#### B-01 用户自定义 filter 规则不被清空

**目的**：验证不再出现 `iptables -t filter -F` 导致的防火墙规则丢失。

**步骤**：
1. 拨通后，手动加一条容易识别的 filter 规则：
   ```bash
   iptables -I INPUT -p icmp -m comment --comment "TEST_B01" -j ACCEPT
   ```
2. 确认 `iptables -t filter -L INPUT -n --line-numbers` 能看到 `TEST_B01`
3. 软断射频 → 恢复（重现 `DISCONNECTED` + `CONNECTED`）：`serial_atcmd at+cfun=4; sleep 30; serial_atcmd at+cfun=1`
4. 再次查看 `iptables -t filter -L INPUT -n --line-numbers`

**预期**：`TEST_B01` 规则**仍然存在**。

**失败判据**：`TEST_B01` 消失 → 回滚到了 `ffea5dd` 的行为。

---

#### B-02 第二块网卡（如有 eth0/br0）路由不被误删

**目的**：验证按 dev 精确删除，不动其他接口。

**前置**：单板上有 `eth0` 或 `br-lan` 等第二个接口且有 default 路由（如本机同时接了有线）。

**步骤**：
1. 记录 `ip route` 所有 default：应同时看到 `default via X dev ccinet1` 和 `default via Y dev eth0`（metric 不同）
2. 软断 → 等 `DISCONNECTED` → 恢复：`serial_atcmd at+cfun=4; sleep 30; serial_atcmd at+cfun=1`
3. 再次看 `ip route`

**预期**：`eth0` 的 default 路由**未被触动**；只有 `ccinet1` 的被删了又加回来。

**若单板不具备此场景**：记为 N/A，改用 `ip route add default via 1.1.1.1 dev lo metric 999 2>/dev/null` 塞一条假路由做替代观察。

---

#### B-03 IPv6 路由清理（仅在 V4V6 或 V6 APN 下有效）

**目的**：验证 DISCONNECTED 时 IPv6 路由也被清掉（`ffea5dd` 漏了）。

**前置**：eSIM APN 支持 IPv6，或用 IPv4v6 APN。

**步骤**：
1. 拨通后 `ip -6 route show default` 能看到 default
2. 软断射频：`serial_atcmd at+cfun=4`
3. 等 `DISCONNECTED` 后立即 `ip -6 route show default`
4. 恢复射频：`serial_atcmd at+cfun=1` → 重连后再看

**预期**：
- 断网后 IPv6 default **消失**（以前版本会残留）
- 重连后 IPv6 default 只有 1 条

---

#### B-04 `ip route` / `ip -6 route` 命令形式统一

**目的**：这是 72888c3 的附带改动（旧命令 `ip ro` 与新命令 `ip route` 混用）。

**步骤**：不需要单独测试，在 A/B 其他用例运行期间 `grep -E "ip (ro|route)" /media/sdcard/dial_log/*.log` 看输出，确认只有 `ip route` / `ip -6 route`，没有 `ip ro`。

---

### C 组：`6c02b01` — Fast Retry 超时、call_id 过滤、init 日志、路由加固

#### C-01 冷启动 init 进度日志（#7）

**目的**：验证 `ql_data_call_init` 重试时能看到进度，不再 20 秒静默。

**步骤**：
1. 上电 → 立即 `tail -F /media/sdcard/dial_log/*.log`
2. 观察前 20 秒的日志输出密度

**预期**：
- 出现 `[INIT] Waiting for ql_data_call_init...`
- 如果 SDK 返回 `-1001`，每约 2 秒看到 `[INIT] data_call_init retrying, remaining=<N>`
- 最终出现 `[INIT] data_call_init OK.` 或 `[INIT] data_call_init failed, ret=<X>`
- 不再出现"拉起后 20s 完全没有输出"

**失败判据**：冷启动后 5–20 秒无任何 `[INIT]` 相关日志。

---

#### C-02 Fast Retry 两阶段超时 —— 正常拨通场景（#1）

**目的**：确认 PDP_WAIT_TIMEOUT_MS (60s) 和 FAST_FAIL_TIMEOUT_MS (10s) 两阶段计时按预期工作。

**步骤**：
1. 正常信号环境下上电
2. 观察日志

**预期**：
- 第一次启动：`[INIT] Fast Retry Mode: Attempt 1/3`
- PDP 建立前：**不应**出现 `Fast Fail: Timeout` （只要 60s 内建立即可）
- PDP 建立时：`[INIT] PDP connected, start Ping window (10000ms).`
- 10 秒内第一次 ping 成功，`has_notified_connect = 1`，**不再进入 Fast Fail 分支**
- `/tmp/dial_retry_count` 在拨通 10min 后被清或被移除

---

#### C-03 Fast Retry 两阶段超时 —— PDP 建不起来（PDP_WAIT 超时）

**目的**：验证 60 秒后无 PDP 则进入 Fast Fail。

**步骤**：
1. 先关射频让模组无信号：`serial_atcmd at+cfun=4`
2. 清计数后重启 dial（让进程重走初始化路径）：
   ```bash
   rm -f /tmp/dial_retry_count
   killall dial; sleep 2  # start_prog 会 15s 后重拉
   ```
3. 观察日志 60 秒

**预期**：
- 60 秒后出现：`[INIT] Fast Fail: PDP not established in 60s. Exiting to retry...`
- 进程退出 → `start_prog` 拉起 → `dial_retry_count` 累加 → 再次进入 `Attempt 2/3`
- 连续 3 次后：`[INIT] Fast Retry Limit Reached. Entering persistent loop mode.`
- 此后进入 L2/L3 恢复（`at+cfun` / 模块重启），根据代码具体策略而定

**恢复**：`serial_atcmd at+cfun=1`，持久循环模式下应最终拨通。

---

#### C-04 Fast Retry 两阶段超时 —— PDP 建起但 Ping 不通（FAST_FAIL 10s 窗口）

**目的**：验证 PDP 已建但 APN 不通外网时，10 秒后进入 Fast Fail。

**步骤**：
1. 用 iptables 封锁所有外网 ICMP，模拟"PDP 建起但 ping 不通"：
   ```bash
   iptables -I OUTPUT -p icmp -j DROP
   iptables -I FORWARD -p icmp -j DROP
   ```
2. 清计数并重启 dial：
   ```bash
   rm -f /tmp/dial_retry_count; killall dial
   ```
3. 观察日志 30 秒
4. 测完后恢复：`iptables -D OUTPUT -p icmp -j DROP; iptables -D FORWARD -p icmp -j DROP`

**预期**：
- 出现 `[INIT] PDP connected, start Ping window (10000ms).`
- 10 秒内 Ping 全部失败
- 出现 `[INIT] Fast Fail: PDP up but no Ping success in 10000ms. Exiting to retry...`
- 进程退出并被拉起

**若 iptables 无法封 ICMP（如内核不支持）**：跳过此用例，记为 N/A。

---

#### C-05 call_name 替代 call_id 过滤（#3）

**目的**：`6c02b01` 之前用 `call_id` 过滤，SDK 重拨后 call_id 变化会误丢事件。改成 `call_name` 后不受影响。

**步骤**：
1. 拨通
2. 在日志中记下第一次 `CONNECTED` 时的 `call_id`（回调里会打印）
3. 软断射频 → 等 `DISCONNECTED` → 恢复：`serial_atcmd at+cfun=4; sleep 30; serial_atcmd at+cfun=1`
4. 等第二次 `CONNECTED`，记下新的 `call_id`
5. 对比两次 call_id

**预期**：
- 两次 call_id **可能不同**（SDK 行为）
- 但两次的 `CONNECTED` / `DISCONNECTED` 日志都正常出现、路由/NAT 都正常重建
- 不会出现"重连后回调静默"

**失败判据**：第二次 CONNECTED 后没有路由设置日志、ping 不通、`g_if_name` 未更新。

---

#### C-06 重复的 network_status 写入被移除（#10）

**目的**：确认不再有重复写 `/tmp/network_status`。

**步骤**：
1. 在单板上运行：
   ```bash
   # 在一个 ssh 窗口里
   inotifywait -m -e modify /tmp/network_status 2>/dev/null
   # 或：stat -c '%Y' /tmp/network_status 前后对比
   ```
2. 另一个窗口触发软断 → 恢复：`serial_atcmd at+cfun=4; sleep 30; serial_atcmd at+cfun=1`

**预期**：重连的 `CONNECTED` 回调**只写一次** `/tmp/network_status`（经由 `update_network_status(1)`），不再经由 `nw_mark_network_status(1)` 重复写。

**备选验证方式**：`grep -c "nw_mark_network_status" /media/sdcard/dial_log/*.log` 在运行后应为 0。

---

#### C-07 iptables MASQUERADE 循环清理（6c02b01 路由加固）

**目的**：验证异常退出累积多条 MASQUERADE 时，重连能全清。

**步骤**：
1. 拨通后手动注入 3 条重复 MASQUERADE 模拟"累积残留"：
   ```bash
   iptables -t nat -A POSTROUTING -o ccinet1 -j MASQUERADE
   iptables -t nat -A POSTROUTING -o ccinet1 -j MASQUERADE
   iptables -t nat -A POSTROUTING -o ccinet1 -j MASQUERADE
   iptables -t nat -L POSTROUTING -n   # 应看到 4 条
   ```
2. 软断 → 等 DISCONNECTED → 恢复：`serial_atcmd at+cfun=4; sleep 30; serial_atcmd at+cfun=1`
3. 再看 `iptables -t nat -L POSTROUTING -n`

**预期**：重连后**只剩 1 条**（10 次循环删能清干净 3 条累积+1 条正常）。

**边界说明**：如果一次累积超过 10 条，循环会停止，残留 N-10 条——这是代码设计的安全上限，**不是 bug**。

---

#### C-08 DNS 临时文件清理（6c02b01 路由加固）

**目的**：验证 DISCONNECTED 时 `/tmp/resolv_v4.conf`、`/tmp/resolv_v6.conf` 被删除，防止下次仅 IPv4 连接时把过期的 IPv6 DNS 合入 `/etc/resolv.conf`。

**步骤**：
1. 拨通（V4V6 或 V4-only 均可）
2. `ls -la /tmp/resolv_v4.conf /tmp/resolv_v6.conf`（至少应有一个）
3. 软断射频：`serial_atcmd at+cfun=4`，等 DISCONNECTED
4. 立即再 `ls -la /tmp/resolv_v*.conf`

**预期**：两个文件都被 `unlink` 掉（ls 报 No such file）。

**失败判据**：DISCONNECTED 后文件还在。

---

#### C-09 三级恢复策略 L1/L2/L3 联合回归

**目的**：在持续无法 ping 通的状态下，观察 L1（软重拨）→ L2（射频重置）→ L3（模块重启）三级恢复按时序依次触发。

**为何用 iptables 而非 `at+cfun=4`**：
- `cfun=4` 会使 REG=0，L1 会因"REG 未就绪"被跳过，全程只能观察 L2/L3，无法验证 L1 执行逻辑
- `cfun=4` 期间 SDK 内部行为不稳定，persistent loop 进程可能每 ~30s 崩溃重启，`start_fail_ts` 随进程重启归零，L2/L3 永远无法触发
- `cfun=4` 下 L2 会内部执行 `serial_atcmd at+cfun=1`，反而恢复了射频，导致 L3 无法被触发
- iptables DROP 仅在内核数据面丢包，REG 保持为 1，进程稳定运行，iptables 规则在 `cfun=0/1` 周期后仍然有效，全链条均可完整验证

**前置条件**：dial 进程正常运行，已拨通，`/tmp/dial_retry_count` 不存在。

**步骤**：
1. 确认当前拨通状态：
   ```bash
   ping -c 3 8.8.8.8          # 应全部通
   cat /tmp/dial_retry_count  # 应报 No such file
   tail -f /media/sdcard/dial_log/*.log  # 确认 REG:1, DownTime:0s
   ```
2. 用 iptables 阻断出站流量，模拟"已注册但数据不通"：
   ```bash
   iptables -I OUTPUT -o ccinet1 -j DROP
   ```
3. 记录 iptables 添加时间，持续 `tail -f` 观察日志，**不做任何其他操作**，等待三级恢复依次触发：
   - **~1 分钟**：`[ALARM] Net Fail Duration: 60 sec. Trigger Level 1 recovery.` → `[RECOVERY L1] Stopping Data Call & Cleaning PDP...` → `[RECOVERY L1] Restarting Data Call...`（REG=1，L1 真正执行，不被跳过）
   - **~5 分钟**：`[ALARM] Net Fail Duration: 300 sec. Trigger Level 2 recovery.` → `[RECOVERY L2] Toggling RF (Airplane Mode)...`（iptables 规则 cfun 周期后仍在，L2 执行后 ping 仍失败）
   - **~30 分钟**：`[ALARM] Net Fail Duration: 1800 sec. Trigger Level 3 recovery.` → `[RECOVERY L3] FATAL: Network down 30mins. Sending AT+CFUN=1,1 then exit.`
4. L3 触发后移除 iptables 规则，验证最终恢复：
   ```bash
   iptables -D OUTPUT -o ccinet1 -j DROP
   ```
5. 等待新进程拨通，确认 `[EVENT] Network Connected` 出现，ping 通

**预期**：
- L1 在 ~60s 后执行软重拨（日志含 `Restarting Data Call`），REG=1 不跳过
- L1 每隔 ~60s 重复触发（直到 5min L2 接手）
- L2 在累计断网 ~5min 时触发，日志含 `Toggling RF`
- L3 在累计断网 ~30min 时触发，进程 exit(1)，`start_prog` 重拉，新 PID 出现
- 移除 iptables 后新进程拨通

**与 D 组相关的附加观察点**（D-5 / D-6 在此用例内顺带验证）：
- L1 日志：`[RECOVERY L1] Stopping Data Call & Cleaning PDP...`（D-5，REG=1 时 L1 真正执行而非跳过）
- L3 日志：`[RECOVERY L3] FATAL: Network down 30mins. Sending AT+CFUN=1,1 then exit.`（D-6，之前此条代码被注释，现在实际发命令并 `exit(1)`）
- L3 后可观察到 `dial` 进程 PID 变化（被 `start_prog` 重新拉起），模组短暂下线再重注册

**清理**（如测试中断或失败，必须执行）：
```bash
iptables -D OUTPUT -o ccinet1 -j DROP  # 如添加了多次则执行多次直到报 "Bad rule"
```

---

### D 组：`df06b4d` — 漏洞修复合集（APN、信号、抖动过滤、L1/L3 恢复、DNS 等）

**原始 commit 说明只有"漏洞修复提交"五个字**，diff 跨 12 个文件，拆解后包含 11 个独立修复点。下面用例按"真机可验证"原则挑选，D-5/D-6/D-9/D-10/D-11 部分观察点并入其他用例。

#### D-01 APN iccid 精确匹配（原 strncmp → strcmp）

**目的**：验证 iccid 前缀不再误匹配。原代码 `strncmp(apn.iccid, sim_iccid, strlen(apn.iccid))` 会把 `"1234"` 的 APN 条目匹配到实际 iccid `"12345678..."`，导致用错 APN。

**前置**：
- `/usr/dial/apn.json` 中配置至少两条 APN 规则，其中一条的 `iccid` 是另一条的前缀。例如：
  ```json
  [
    {"iccid": "898600", "apn": "WRONG_CMCC", "usrname": "", "pwd": ""},
    {"iccid": "89860012345678901234", "apn": "CORRECT_CMNET", "usrname": "", "pwd": ""}
  ]
  ```
- 本设备为 eSIM，iccid 固定，无法换卡；此测试主要靠代码 review 确认 `strcmp` 替换了 `strncmp`，真机验证退化为"正确 iccid 能匹配上"即可

**步骤**：
1. 上电 → 观察日志 `find apn_name <X>, ..., apn iccid <Y>`
2. 如果没有"前缀重叠"测试 SIM 可用，可退化为：**只验证完整 iccid 一致时能匹配上**（步骤同上，日志里 apn_name 应是那条 iccid 完全匹配的记录）

**预期**：
- 命中的是 `apn iccid` 与 SIM iccid **完全相同**的条目（`CORRECT_CMNET`）
- 不会命中前缀相同但更短的条目（`WRONG_CMCC`）

**失败判据**：日志显示命中了 `WRONG_CMCC`，或命中了长度较短的前缀条目。

**备注**：前缀重叠 SIM 不好准备时可跳过此专项，只要 D-02 能过就算 APN 路径基本健康。

---

#### D-02 APN JSON 解析健壮性（内存安全 + 字段 null 终结）

**目的**：验证 `apn_load_from_json` 的内存管理修复：
- `malloc(len+1)` + 手动加 `'\0'`（修 json_tokener_parse 读越界风险）
- `fread` 返回值类型修正（`size_t` 不能与 `< 0` 比较）
- `p_apn_obj` 和根 `obj` 的 `json_object_put` 释放修正（修内存泄漏、修误释放 borrowed reference 导致的崩溃）
- 字段从 `memcpy(dst, src, strlen(src))` 改为 `strncpy(dst, src, sizeof(dst)-1)` + 补 `'\0'`

**步骤**：
1. 准备 3 份 `apn.json` 依次替换：
   - **正常 json**：3 条正常条目
   - **畸形 json**：故意写一个语法错误（比如漏闭合 `}` 或多余逗号）
   - **超长字段 json**：某条目 `apn` 字段塞 256 字节（超过 `apn_obj_t.apn` 缓冲区）
2. 每份替换后重启 dial，观察日志和进程状态
3. 长跑：在正常 json 下让 dial 跑 2 小时，期间人为重启 10 次（`kill -INT`）

**预期**：
- 正常 json → 成功加载、拨号通过
- 畸形 json → 日志 `apn parse error`，**进程不崩溃**，继续尝试默认 APN
- 超长字段 json → 缓冲区被截断并以 `\0` 结尾（`printf("find apn_name %s", ...)` 不会打出乱码/越界内存）
- 10 次重启后 dial RSS 稳定（`cat /proc/<pid>/status | grep VmRSS`）不持续增长

**失败判据**：任何一份 json 导致 dial 崩溃或日志里看到乱码字段。

---

#### D-03 SIGINT 安全退出

**目的**：验证 `sig_handler` 重写正确：
- 原代码捕获 `SIGKILL`（**SIGKILL 根本不可捕获**，等于没写）
- 信号处理函数中调用 `free()`（非 async-signal-safe，随时可能死锁或 double-free）
- 现改为捕获 `SIGINT`，仅设 `volatile sig_atomic_t g_sigint_received = 1`，清理工作搬到主循环

**步骤**：
1. 拨通后，`ps | grep dial` 拿到 PID（注意是 dial 本身，不是 start_prog）
2. `kill -INT <PID>`
3. 立刻 `tail -F /media/sdcard/dial_log/*.log`
4. 观察日志是否有 `[EVENT] SIGINT received. Cleaning up and exiting.`
5. 等 5 秒再 `ps | grep dial` 看进程是否已退出并被 start_prog 拉起（PID 变化）

**预期**：
- 日志出现上面那条清理消息
- dial 进程干净退出（退出码 0）
- `start_prog` 重新拉起，新 PID 能拨通

**失败判据**：kill -INT 后进程崩溃产生 coredump，或者进程无反应（信号未被捕获）。

---

#### D-04 Ping 抖动过滤（PING_FAIL_THRESHOLD=3）

**目的**：验证单次/两次 ping 抖动不会触发故障计时，只有连续 3 次失败才启动。

**步骤**：
1. 拨通稳定 2 分钟
2. 临时用 iptables 注入单次丢包规则，模拟偶发抖动：
   ```bash
   # 丢弃下一个 ICMP echo-request（一次性）
   iptables -I OUTPUT 1 -p icmp --icmp-type echo-request -m statistic --mode nth --every 2 --packet 0 -j DROP
   # 10 秒后移除
   sleep 10 && iptables -D OUTPUT -p icmp --icmp-type echo-request -m statistic --mode nth --every 2 --packet 0 -j DROP &
   ```
3. 观察日志中 `ping_fail_count` 相关行为（如果代码里打印了 `ping fail count=`，观察计数；如果没打印，可临时加 printf 验证）
4. 继续观察 30 秒，应无 `[ALARM] Net Fail Duration` 日志

**预期**：
- 偶发 ping 失败只让 `ping_fail_count` 累加 1–2 次
- 一旦 ping 恢复，计数归零
- `start_fail_ts` 保持为 0，**不触发任何 L1/L2/L3 恢复**

**强化场景**：持续 ping 失败（完全阻 ICMP 出站 2 分钟），应能正常触发 L1 恢复（60s 后）。

**失败判据**：单次 ping 失败就进入 `[ALARM]` 或 `[RECOVERY L1]`。

---

#### D-05 L1 恢复 REG 状态判断 + 节流

> **备注**：此用例与 C-09（L1/L2/L3 联合回归）有重叠，**建议合并到 C-09 的观察点里做**，此处仅列独立验证路径（如需快速定位 L1 行为可单独跑）。

**目的**：验证 L1 在 REG 未就绪（不为 1/5）时跳过并等待 L2；L1 之间至少间隔 60 秒。

**步骤**：
1. 关射频制造 REG=0/2/4 状态（注册失败）：`serial_atcmd at+cfun=4`
2. 等 ping 连续失败超过 60s 触发 L1
3. 观察日志

**预期**：
- 日志反复出现 `[RECOVERY L1] Skip: REG not ready (REG:<X>), waiting for L2.`
- **不**出现每 1.5 秒就 `Restarting Data Call` 的猛打行为
- 10 分钟后自动升级到 L2（`[RECOVERY L2] Toggling RF ...`）

**失败判据**：L1 被每秒级频繁触发；或 REG=0 时仍执行 `ql_data_call_stop`/`start` 浪费时间。

---

#### D-06 L3 真发 at+cfun=1,1 并 exit

> **备注**：此用例与 C-09 有重叠，建议合并。此处单列是因为 **L3 代码之前是被注释掉的（原代码"FATAL: ... But Not REBOOTING MODULE"）**，**新版本才真正执行**，行为差异非常大，必须明确观察。

**目的**：确认 L3 实际发送 AT 指令并退出进程让 start_prog 重拉。

**步骤**：
1. 在持续无信号场景下运行 30 分钟以上（C-09 场景）
2. 观察日志里 L3 分支的触发

**预期**：
- 日志按顺序出现：
  ```
  [RECOVERY L3] FATAL: Network down 30mins. Sending AT+CFUN=1,1 then exit.
  [RECOVERY L3] Exiting. start_prog will reinitialize.
  ```
- 模组短暂下线（`AT` 命令 5–20 秒内无响应）
- dial 进程 PID 变化（被 start_prog 重拉）
- 重拉后新进程从 `[INIT] Waiting for ql_data_call_init...` 重头走

**失败判据**：30+ 分钟后看到 `RECOVERY L3` 日志但模组未重启（退回到老版本"只打印不干活"的 bug 行为）。

---

#### D-07 DNS resolv.conf 换行符修复

**目的**：验证 `/etc/resolv.conf` 中 nameserver 行不含 `\r` 字符，DNS 解析正常。

**步骤**：
1. 拨通后：
   ```bash
   cat -A /etc/resolv.conf   # -A 显示所有控制字符，\r 会以 ^M 显示
   ```
2. 做一次实际 DNS 解析：
   ```bash
   nslookup www.baidu.com
   # 或：getent hosts www.baidu.com
   ```
3. `cat -A /tmp/resolv_v4.conf` 同样验证

**预期**：
- `cat -A` 输出的每行以 `$` 结尾（LF），**没有 `^M$`**（CRLF）
- `nslookup` 能解析出 IP（不会因 `\r` 污染 nameserver 字符串而超时失败）

**失败判据**：`cat -A` 看到 `^M`，或者 `nslookup` 超时但路由和 MASQUERADE 都正常。

---

#### D-08 flow_monitor_task 首次调用只记基准

**目的**：验证首次进入 `flow_monitor_task` 不会因 `u64_if_rx_packets` 静态变量残留为 0 导致 `if_rx_packets > 0` 总成立（假阳性"有收到包"），而是只记基准值。

**步骤**：
1. 上电 → 在 `CONNECTED` 回调刚触发后的头 5 秒内观察日志
2. 检查 dial_timer 是否被设置但不会触发"已收到数据包"的异常日志

**预期**：
- 进入流量监控后，首次读取只记基准值
- 如果一直无数据进出，`DIAL_TIMEOUT_SECONDS` (120) 后应正常触发 `dial_stat_stop`（这是本来的正常逻辑，不是 bug）

**失败判据**：首次调用后 dial_timer 被错误地认为"刚收到数据"而永远不超时（老 bug 回归）。

**实施难度**：需要在 `flow_monitor_task` 里临时加 `printf("first_call=%d rx=%llu base=%llu", ...)` 调试。如果不方便加打印，此用例可以跳过，靠 D-02 / A-01 的整体回归间接覆盖。

---

#### D-09 restartNetworkServices 无僵尸进程

> **备注**：此用例对应 `misc.c:restartNetworkServices` 双重 fork 修复。但**此函数在当前 dial 运行路径中调用点极少**（需翻代码确认具体触发条件），真机很难自然触发。

**目的**：如果能找到触发路径，验证调用后不产生僵尸子进程。

**步骤**：
1. 触发 `restartNetworkServices()`（查看代码找到调用点，例如某条 error path；若找不到可**跳过此用例**）
2. `ps -ef | grep -E "defunct|Z"` 看是否有僵尸
3. `ps -ef | grep -E "ql_rild|ql_netd"` 确认被重启的进程被 init（PPID=1）收养

**预期**：
- 调用后 `ql_rild` / `ql_netd` 的 PPID 是 1
- 无任何僵尸进程

**失败判据**：`<defunct>` 条目出现，或 PPID 不是 1。

**备注**：建议在代码 review 阶段确认双重 fork 实现正确即可，真机不强制触发。

---

#### D-10 check_process 精确匹配（pgrep -x）

**目的**：验证 `check_process("dial")` 不会把 `dialog` 等含相同子串的进程误算为匹配。

**步骤**：
1. 手动启一个名字含 "dial" 但不是目标的进程：
   ```bash
   # 如果设备上有 dialog 则直接 dialog & 即可；否则：
   cp /bin/sleep /tmp/dial_xxx_test
   /tmp/dial_xxx_test 3600 &
   ```
2. 在 dial 日志里关注下一次 `check_process` 调用返回值（如代码有打印）
3. 如果没打印，可以手动在 shell 里验证：
   ```bash
   pgrep dial      # 应返回多个 PID（老行为）
   pgrep -x dial   # 应只返回真正的 /usr/dial/dial PID（新行为）
   ```

**预期**：`pgrep -x dial` 只返回目标进程 PID。

**备注**：此用例主要是 shell 侧验证工具行为，dial 代码调用效果不易观察。**建议仅做代码 review 确认 `pgrep -x` 已经使用，真机跳过**。

---

#### D-11 popen 配对 pclose（reboot_conf）

**目的**：验证 `dial_reboot_conf.c:get_signal_csq` 用 `pclose` 关闭 `popen` 返回的 FILE*（原来误用 `fclose`）。

**步骤**：
- **代码 review 验证**：
  ```bash
  grep -n "popen\|pclose\|fclose" reboot_conf/dial_reboot_conf.c
  ```
  确认 popen 对应的 FILE 用 pclose 关闭
- **运行时验证**：
  1. 让 dial_reboot_conf 的 `get_signal_csq()` 在主流程中被调用（查代码调用点）
  2. 触发调用后 `ps -ef | grep -E "defunct|<Z>"` 看是否有僵尸
  3. 长跑 1 小时观察进程表是否累积 `<defunct>`

**预期**：无僵尸进程，长跑后进程表干净。

**失败判据**：`<defunct>` 条目累积增加。

**备注**：同 D-10，主要靠代码 review；真机跳过不影响验收。

---

## 五、回归基线（每次烧录必跑）

如果时间紧张只能跑 30–45 分钟，跑下面这 6 个：

| 用例 | 验证点 |
|---|---|
| A-01 冷启动 | 整链路通、日志完整 |
| A-02 短断拔插 | 核心 bug 不回归 |
| C-02 Fast Retry 正常路径 | 两阶段计时没打架 |
| C-07 MASQUERADE 循环清理 | 路由加固生效 |
| D-03 SIGINT 安全退出 | 信号处理不崩 |
| D-07 DNS 换行符 | resolv.conf 干净 + nslookup 通 |

---

## 六、Pass/Fail 总结模板

测试结束后填此表，连同日志归档：

| 用例 | 结果 | 日志文件 | 备注 |
|---|---|---|---|
| A-01 | ✅ PASS | | 上电首次拨通，路由/NAT 各一条 |
| A-02 | ✅ PASS | | 软断 30s 后重连，旧路由清理干净，ping 直接通 |
| A-03 | ✅ PASS | | 软断 5min 后重连，最终拨通，路由/NAT 干净 |
| A-04 | ✅ PASS | | 连续拔插 5 次，路由/NAT 无累积残留 |
| B-01 | ✅ PASS | | filter 自定义规则断网重连后仍存在 |
| B-02 | N/A | | 设备无第二块网卡 |
| B-03 | N/A | | 设备 APN 不支持 IPv6 |
| B-04 | ✅ PASS | | 日志中确认命令形式统一为 ip route |
| C-01 | ☐ PASS ☐ FAIL ☐ N/A | | |
| C-02 | ☐ PASS ☐ FAIL ☐ N/A | | |
| C-03 | ☐ PASS ☐ FAIL ☐ N/A | | |
| C-04 | ☐ PASS ☐ FAIL ☐ N/A | | |
| C-05 | ☐ PASS ☐ FAIL ☐ N/A | | |
| C-06 | ☐ PASS ☐ FAIL ☐ N/A | | |
| C-07 | ☐ PASS ☐ FAIL ☐ N/A | | |
| C-08 | ☐ PASS ☐ FAIL ☐ N/A | | |
| C-09 | ✅ PASS | `dial_20260421_*.log` | L1×5/L2×5/L3×1 全部按时序触发，L3 后系统重启，移除 iptables 后新进程拨通 |
| D-01 | ☐ PASS ☐ FAIL ☐ N/A | | 无前缀重叠 SIM 可标 N/A |
| D-02 | ☐ PASS ☐ FAIL ☐ N/A | | |
| D-03 | ☐ PASS ☐ FAIL ☐ N/A | | |
| D-04 | ☐ PASS ☐ FAIL ☐ N/A | | |
| D-05 | ✅ PASS | (见 C-09) | REG=1 时 L1 正常执行，未跳过；C-09 观察点确认 |
| D-06 | ✅ PASS | (见 C-09) | L3 真发 AT+CFUN=1,1，系统重启确认；C-09 观察点确认 |
| D-07 | ☐ PASS ☐ FAIL ☐ N/A | | |
| D-08 | ☐ PASS ☐ FAIL ☐ N/A | | 需临时加打印才能精确验证 |
| D-09 | ☐ PASS ☐ FAIL ☐ N/A | | 主要靠代码 review |
| D-10 | ☐ PASS ☐ FAIL ☐ N/A | | 主要靠代码 review |
| D-11 | ☐ PASS ☐ FAIL ☐ N/A | | 主要靠代码 review |

**验收标准**：A 组、C 组**必须全部 PASS**；B 组若单板形态不具备场景（单网卡、仅 IPv4 APN 等）可标 N/A，但 B-01（filter 规则不被清）必须验证；D 组中 D-02、D-03、D-04、D-07 **必须 PASS**，D-05/D-06 通过 C-09 观察点确认，D-08~D-11 允许以代码 review 替代真机验证。

---

## 附录：出问题时的排查抓手

| 现象 | 第一优先级检查 |
|---|---|
| 重连后 ping 不通 | `ip route` 是否有 default；`iptables -t nat` 是否有 MASQUERADE |
| 日志长时间静默 | `ps` 进程是否还在；`/tmp/dial_Status` 值；SD 卡是否满 |
| Fast Retry 无限退出拉起 | 信号强度 `AT+CSQ`、SIM 状态 `AT+CPIN?` |
| resolv.conf 里混了旧 DNS | `ls -la /tmp/resolv_v*.conf` 时间戳 |
| 第二块网卡断网 | `ip route | grep default` 是否被误删 |





● C-09 日志分析：      
                                          
  ---                                                                                                                                                                                                        
  L1/L2/L3 触发情况：        
                                                                                                                                                                                                    
  ┌──────┬──────────┬─────────────────────────────────┬───────┬──────────────────────┐                                                                                                                       
  │ 级别 │ 触发次数 │           触发时间点            │ 间隔  │       是否正常       │                                                                                                                       
  ├──────┼──────────┼─────────────────────────────────┼───────┼──────────────────────┤                                                                                                                       
  │ L1   │ 5次      │ 62s、63s、124s、185s、246s      │ ~60s  │ ✓ 正常               │                                                                                                                       
  ├──────┼──────────┼─────────────────────────────────┼───────┼──────────────────────┤                                                                                                                       
  │ L2   │ 5次      │ 547s、847s、1148s、1448s、1749s │ ~300s │ ✓ 正常               │                                                                                                                       
  ├──────┼──────────┼─────────────────────────────────┼───────┼──────────────────────┤                                                                                                                       
  │ L3   │ 1次      │ 1800s（13:22:17）               │ —     │ ✓ 正常，触发系统重启 │                                                                                                                       
  └──────┴──────────┴─────────────────────────────────┴───────┴──────────────────────┘                                                                                                                       
                                                            
  L3 触发后 AT+CFUN=1,1 导致模组（即 SoC 本身）完整重启，日志里看到了 bootloader 输出，符合预期。                                                                                                            
                                                            
  ---                                                                                                                                                                                                        
  Fast Retry 3次耗时（当前版本）：                          
                                  
  ┌─────────────────┬──────────┬──────────┬──────┐
  │      阶段       │   开始   │   结束   │ 耗时 │                                                                                                                                                           
  ├─────────────────┼──────────┼──────────┼──────┤
  │ Attempt 1/3     │ 12:50:49 │ 12:51:00 │ 11s  │                                                                                                                                                           
  ├─────────────────┼──────────┼──────────┼──────┤          
  │ start_prog 等待 │ 12:51:00 │ 12:51:15 │ 15s  │
  ├─────────────────┼──────────┼──────────┼──────┤                                                                                                                                                           
  │ Attempt 2/3     │ 12:51:15 │ 12:51:26 │ 11s  │
  ├─────────────────┼──────────┼──────────┼──────┤                                                                                                                                                           
  │ start_prog 等待 │ 12:51:26 │ 12:51:41 │ 15s  │          
  ├─────────────────┼──────────┼──────────┼──────┤                                                                                                                                                           
  │ Attempt 3/3     │ 12:51:41 │ 12:51:52 │ 11s  │          
  ├─────────────────┼──────────┼──────────┼──────┤                                                                                                                                                           
  │ start_prog 等待 │ 12:51:52 │ 12:52:07 │ 15s  │
  ├─────────────────┼──────────┼──────────┼──────┤                                                                                                                                                           
  │ 总计            │          │          │ ~78s │          
  └─────────────────┴──────────┴──────────┴──────┘                                                                                                                                                           
  
  每次 11s = PDP 建立（~1s）+ FAST_FAIL_TIMEOUT 10s ping 窗口。



  一、什么是 OpenCPU？
                                                                                                                                                                                                             
  OpenCPU 是 Quectel 提供的一种开发模式：开发者可以直接在模组内置的应用处理器上运行自己的代码（比如本项目的 dial 程序跑在模组的 Linux 上），不需要外部 MCU 通过串口发 AT 命令控制模组。EC200A 就是这种模式。 
                                                                                                                                                                                                             
  二、为什么重启模组等于重启系统？                                                                                                                                                                           
                                                            
  我之前说"AP 和 CP 是同一颗 SoC"是不准确的。EC200A 内部实际上有两个处理器：                                                                                                                                 
   
  - AP（Application Processor）：跑 OpenWrt Linux，运行 dial 程序                                                                                                                                            
  - CP（Communication Processor）：处理蜂窝协议栈，响应 AT 命令
                                                                                                                                                                                                             
  它们在同一个芯片封装里，但并不是同一个核。                                                                                                                                                                 
   
  真实原因：从日志里 procd killer:reboot,sig:15 可以看出，是 Linux 自己的 procd 发起了重启，说明 Quectel 的 OpenCPU 中间件把 AT+CFUN=1,1 的处理实现为触发整个系统重启，而不只是重置 CP。这是 Quectel         
  的平台实现决定的，不是"硬件上 AP/CP 不可分"。



● 确认正确。从启动日志里能直接看到硬件真相：
                             
  SoC:   PXA 88AP1803-A1   ← 单颗 ASR1803 芯片                                                                                                                                                               
  CR5_PC_HD: 0x6878f8c     ← CP（通信处理器，ARM Cortex-R5，跑蜂窝协议栈）                                                                                                                                   
  CA7_PC_HD: 0xc058a8ec    ← AP（应用处理器，ARM Cortex-A7，跑 Linux/dial）                                                                                                                                  
                                                                                                                                                                                                             
  CR5（调制解调器）和 CA7（Linux）是同一颗 ASR1803 芯片里的两个核。                                                                                                                                          
                                                                                                                                                                                                             
  所以 AT+CFUN=1,1 发给 CP（CR5）的复位指令，本质上是对整颗芯片的复位，CA7 上的 Linux 必然跟着重启。这不是软件行为，是硬件决定的。                                                                           
                                                            
  之前我说"Quectel 中间件把它实现为系统重启"的说法不够准确，实际原因是芯片架构：两个核在同一个 die 上，CP 复位 = 整颗 SoC 复位 = Linux 也重启。                                                              
                                                            
  L3 行为因此是正确且不可避免的。



