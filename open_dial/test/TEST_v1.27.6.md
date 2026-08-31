# open_dial V1.27.6 变更测试用例

> 适用版本：**V1.27.6**（MAIN=1, SUB=27, PATCH=6）
> 受测改动：C.1 修复表 **#26 / #27 / #28 / #29 / #30**
> 硬件：EC200A 模组 + 目标单板（**eSIM 设备，断卡场景用 `at+cfun=4` 替代**）
> 说明：本文档只覆盖 v1.27.6 新增改动，回归基线仍执行 CLAUDE.md F.5 的 6 条核心用例。

---

## 0. 受测改动速查

| # | 文件 | 改动 | 行为影响 |
|---|------|------|---------|
| 26 | `dial.c` / `apn/apn.c` | `apn_id` 由 `1` 改回 `DATA_CALL_APN_PUBLIC`(=6)；默认 APN `"apnpublic"`→空串 | **影响拨号行为**，需真机回归 |
| 27 | `dial.c` | `ip_ver` `0`→`QL_NET_IP_VER_V4`(0x1) | **影响拨号行为**，需真机回归 |
| 28 | `dial.c` | fault 快照门控 `start_fail_ts==0`→`!diag_snap_done` | 诊断完整性，不影响联网 |
| 29 | `_public.h` / `misc.c` | 补头文件 include | 仅消除编译告警 |
| 30 | `logger_sd.c` | `ret` 非致命判断 + 备用函数标 unused | 仅消除编译告警 |

> **重点**：#26、#27 改变拨号路径，是本批唯一有现网风险的点，**上线前必须真机回归**；#28 仅补诊断；#29、#30 只需编译期验证。

---

## 1. 测试前准备

```bash
# 烧录前基线
mkdir -p /tmp/v1276_before
cat /tmp/dial_version                  > /tmp/v1276_before/version.txt 2>&1
ip route show                          > /tmp/v1276_before/route4.txt
ip -6 route show                       > /tmp/v1276_before/route6.txt
iptables -t nat -L POSTROUTING -n -v   > /tmp/v1276_before/nat.txt
cp /etc/resolv.conf                       /tmp/v1276_before/resolv.conf
serial_atcmd at+cgdcont?               > /tmp/v1276_before/cgdcont.txt   # 当前各 profile 的 APN
serial_atcmd at+cgpaddr                > /tmp/v1276_before/cgpaddr.txt   # 当前分配 IP

# 常用观察
tail -F /media/sdcard/dial_log/dial_*.log     # 运行日志
ls -lt /media/sdcard/dial_snap/               # 诊断快照（dmesg_*/logcat_*）
cat /tmp/dial_version                          # 应显示 Version: 1.27.6
```

记录所用测试卡的 **ICCID** 和它在 `/usr/dial/apn.json` 中是否有匹配条目——后续用例据此分流。

---

## 2. 用例

### T0 编译期验证（#29 / #30）

**目的**：确认零告警、产物正常。

```bash
cd open_dial/
make clean && make 2>&1 | tee /tmp/build.log
grep -c -i warning /tmp/build.log      # 期望: 0
grep -c -i error   /tmp/build.log      # 期望: 0
ls -l dial                             # 期望: 生成可执行文件
```

**判定**：warning 数 = 0，error 数 = 0，`dial` 生成成功。

---

### T1 已匹配 ICCID 拨号（#26 核心）⭐

**前置**：测试卡 ICCID **存在**于 `apn.json`，且该条目 APN 为运营商正确值。

**目的**：验证 apn.json 匹配出的 APN 真正被绑定到数据连接（修复前因绑 profile 1 而失效）。

```bash
# 1. 确认 apn.json 命中条目
cat /usr/dial/apn.json | grep -A1 "<本卡ICCID前若干位>"

# 2. 重启 dial（或冷启动），看日志
#    期望日志: [INIT] ICCID: <iccid>
#              find apn_name <apn>, ... apn iccid <iccid>
#              [INIT] APN set success
tail -F /media/sdcard/dial_log/dial_*.log

# 3. 拨通后核对模组里实际生效的 APN（profile 6）
serial_atcmd at+cgdcont?               # profile 6 的 APN 应 == apn.json 命中值
serial_atcmd at+cgpaddr                # 应分配到 IPv4
ping -I ccinet1 -c 3 8.8.8.8           # 应通
```

**判定**：日志命中正确 APN；`AT+CGDCONT?` 中 profile 6 的 APN 名 == apn.json 命中条目；分配到 IP 且 ping 通。

> **强证用例（如有专网/物联卡）**：用一张**必须显式 APN、自动 APN 拨不通**的卡，ICCID + 正确 APN 写入 apn.json。修复前（v1.27.5）必然拨不通，修复后（v1.27.6）能拨通——这是 #26 最有说服力的验证。

---

### T2 未匹配 ICCID 拨号（#26 配套：默认空串 APN）⭐

**前置**：测试卡 ICCID **不在** `apn.json` 中（或临时把它从 apn.json 移除）。

**目的**：验证未匹配卡走"自动 APN"（默认 APN 已由 `"apnpublic"` 改为空串），消费级卡不被强制 `apnpublic` 拨不通。

```bash
# 重启 dial，看日志
#   期望: apn not found, use default apn ...
tail -F /media/sdcard/dial_log/dial_*.log

serial_atcmd at+cgdcont?               # profile 6 的 APN 应为空（""）
serial_atcmd at+cgpaddr                # 仍应分配到 IP
ping -I ccinet1 -c 3 8.8.8.8           # 应通（模组自动选 APN）
```

**判定**：profile 6 的 APN 为空；卡仍能正常拨通 ping 通。

> 对照点：若 profile 6 显示为 `apnpublic` 而非空，说明本次空串改动未生效。

---

### T3 IPv4 拨号正常（#27）

**目的**：`ip_ver` 改为显式 `QL_NET_IP_VER_V4` 后，IPv4 拨号与取址正常。

```bash
# 冷启动拨通后
serial_atcmd at+cgpaddr                # 应有 IPv4 地址
ip addr show ccinet1 | grep "inet "    # 接口应有 v4 地址
ip route show | grep default           # 仅 1 条 default via ccinet*
ping -I ccinet1 -c 3 8.8.8.8           # 应通
```

**判定**：分配到 IPv4 地址，default 路由 1 条，ping 通。无异常断流/反复重拨。

---

### T4 SIM 故障触发 fault 快照（#28）⭐

**目的**：验证 SIM 路径导致的断网也能抓到 `dmesg_fault_*` / `logcat_fault_*` 快照（修复前只有 Ping 路径会抓）。

```bash
# 0. 先确认已拨通、快照目录基线
ls /media/sdcard/dial_snap/ | wc -l        # 记下当前文件数 N0

# 1. 触发 SIM 异常（eSIM 用 cfun=4 模拟；可插拔卡直接拔卡）
serial_atcmd at+cfun=4

# 2. 观察日志，SIM 状态应转非 READY，并出现 fault 快照
#    期望日志序列:
#      [ERROR] SIM Card disconnected during runtime, CPIN=...   (或 SIM_CB 转非 READY)
#      ping 连续失败累计到阈值
#      [DIAG] ...   后跟  [DIAG] Snapshot saved: .../dmesg_fault_* / logcat_fault_*
tail -F /media/sdcard/dial_log/dial_*.log

# 3. 核对快照文件确实新增
ls -lt /media/sdcard/dial_snap/ | head      # 应出现 dmesg_fault_<ts>.log / logcat_fault_<ts>.log

# 4. 恢复
serial_atcmd at+cfun=1
#    期望: SIM 恢复 READY，网络恢复后再出一组 *_recovery_* 快照
ls -lt /media/sdcard/dial_snap/ | head      # 应出现 dmesg_recovery_* / logcat_recovery_*
```

**判定**：故障期产生 `*_fault_*` 快照，恢复期产生 `*_recovery_*` 快照，且每个故障周期各抓一次（`diag_snap_done` 单次门控）。

> **注**：eSIM 下 `at+cfun=4` 不一定让 SIM app_state 转非 READY（可能仍是飞行模式下 SIM 仍 READY、仅注册丢失）。若 SIM_CB 未转非 READY，本用例退化为"Ping 路径触发 fault 快照"（见 T4b），#28 的 SIM 专用分支改由**代码评审**确认门控逻辑：fault 快照块已从 `if(start_fail_ts==0)` 移出，改为独立 `if(!diag_snap_done)`。

---

### T4b Ping 路径 fault 快照回归（#28 副验）

**目的**：确认把门控从 `start_fail_ts==0` 改为 `!diag_snap_done` 后，原 Ping 路径快照行为不回退。

```bash
ls /media/sdcard/dial_snap/ | wc -l         # 基线 N0
iptables -I OUTPUT -o ccinet1 -j DROP        # 阻断出站，保持 REG=1
# 等 ping 连续失败累计到阈值（约数秒）
tail -F /media/sdcard/dial_log/dial_*.log    # 应出现 [DIAG] Snapshot saved: dmesg_fault_*
iptables -D OUTPUT -o ccinet1 -j DROP        # 恢复，应再出 *_recovery_*
```

**判定**：故障/恢复各抓一次快照，且**同一故障周期内不重复抓**（验证 `diag_snap_done` 单次语义）。

---

## 3. Pass/Fail 汇总

| 用例 | 验证点 | 结果 | 日志/证据 | 备注 |
|------|--------|------|-----------|------|
| T0  | 零 warning / 零 error | ☐ PASS ☐ FAIL | build.log | #29/#30 |
| T1  | 匹配卡 APN(profile6) 真正生效、ping 通 | ☐ PASS ☐ FAIL | | #26 核心 |
| T2  | 未匹配卡 profile6 为空、走自动 APN、ping 通 | ☐ PASS ☐ FAIL | | #26 配套 |
| T3  | IPv4 取址、default 1 条、ping 通 | ☐ PASS ☐ FAIL | | #27 |
| T4  | SIM 故障产生 *_fault_* / *_recovery_* 快照 | ☐ PASS ☐ FAIL ☐ N/A | | #28（eSIM 可能 N/A，转代码评审） |
| T4b | Ping 故障快照不回退、单周期一次 | ☐ PASS ☐ FAIL | | #28 副验 |

**验收标准**：
- T0、T3 必须 PASS；
- T1 必须 PASS（无专网卡时至少消费级卡 PASS + profile6 APN 核对正确）；
- T2 必须 PASS（profile6 为空、能拨通）；
- T4b 必须 PASS；T4 若因 eSIM 受限可记 N/A，以代码评审替代。

---

## 4. 回滚与风险

- **唯一现网风险点**：#26 + #27 改了拨号绑定路径与 IP 版本取值。若现网设备全是依赖自动 APN 的消费级卡，重点确认 T2（profile6 空串 → 自动 APN）不退化。
- **回滚方式**：`apn_id` 改回 `1`、`ip_ver` 改回 `0`、`set_apn` 默认分支恢复 `APN_NAME_PUBLIC` 即回到 v1.27.5 行为（但 apn.json 匹配将再次失效，仅建议作为临时排障手段）。
- **历史依据**：#26 的正确写法（绑 profile 6）即首次提交 `afb6936` 的原始行为，`e92733c` 重构时回归为 1；本次为"改回正确值"，非引入新设计。

---

*本文档随 V1.27.6 改动生成，配套 CLAUDE.md C.1 #26–#30。回归基线见 CLAUDE.md F.5。*
