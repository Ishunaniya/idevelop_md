# IMX6ULL 4G 分级恢复真机自测用例（v1.25.1）

> 适用对象：`QL_MODULE_PLATFORM=MCIMX6Y2CVM08AB` 构建的 `modem_mng`，版本 `imx6ull 1.25.1`。
>
> 本文是**真机执行用例**，不是设计说明。执行前请先读“限制和安全”，并在每个用例结束后执行对应的回滚步骤。

## 1. 本版本实际恢复阶梯

| 级别 | 代码日志名 | 实际动作 | 进入条件 | 默认阈值 |
|---|---|---|---|---|
| L1 | `L1_DHCP` | 停旧 `udhcpc`，重新启动 DHCP，重新取得 IP/默认路由 | 蜂窝网卡无 IPv4 或无默认路由 | 最多 3 次；每次等 DHCP 最多 20 秒 |
| L2 | `L2_PDP` | 停 DHCP，`modem_init()` + 重新建立 CID1 PDP/网卡 | 连续 5 次绑定蜂窝网卡的 TCP 探测失败；或 L1 耗尽 | 最多 3 次 |
| L3 | `L3_CFUN` | `AT+CFUN=0`，等待 1 秒，`AT+CFUN=1` | CEREG 注册超时、SIM 未 READY、L2 耗尽 | CFUN 冷却 600 秒 |
| L4 | `L4_HARDWARE` | GPIO130 断电/上电，GPIO128 PWRKEY 开机脉冲 | USB 枚举超时或连续 AT 基本探针失败 | 硬复位冷却 900 秒 |

**重要：L3 注册失败并不会自动升级至 L4。** L4 是 AT/USB 控制面故障的路径。不要把“持续无信号后自动断电”写成当前版本的预期。

## 2. 限制和安全

- 所有操作都应在实验室、可接触设备且具备串口/本地控制台时完成；不得仅依赖待测 4G 远程登录。
- 下面的网络操作会真实中断 4G 业务；L4 会真实切断模组电源。不得在车辆、生产设备或有任务的设备上执行。
- `modem_mng` 是 AT 端口唯一所有者。运行期间**禁止**用 `cat /dev/ttyUSB*`、minicom、串口助手或独立 AT 脚本发命令。
- 本文所有命令均以 `root` 执行；`IFACE` 必须替换为实际蜂窝接口，常见为 `usb0`，不能凭经验直接执行。
- `iptables` 仅用于实验室故障注入；测试结束必须删除本文创建的规则链。若目标系统没有 `iptables`，用网关/测试仪做等价阻断，不要安装新包改变镜像。

## 3. 一次性测试准备

### 3.1 记录环境

填写本表后再开始：

| 项目 | 记录值 |
|---|---|
| 日期/测试人 | |
| 板卡 SN/硬件版本 | |
| 模组型号/固件 | |
| `cat /tmp/dial_version` | |
| 实际蜂窝接口 `IFACE` | |
| SIM/运营商/所在地 | |
| 当前探测端点 | |

确认基础网络正常：

```sh
cat /tmp/dial_version
ip -br link
ip -4 addr show
ip route show
cat /tmp/dial_success
ps | grep -E '[m]odem_mng|[u]dhcpc'
```

从启动日志确认探测端点和恢复策略。日志目录通常是 `/media/sdcard/dial_log/<日期>/`；如系统时钟未同步则在 `unsynced/` 下。

```sh
find /media/sdcard/dial_log -type f -name '*.log' -print | sort | tail -n 5
grep -E '\[RECOVERY\] policy|\[NET\] interface-bound probe' <本次modem_mng日志>
```

期望看到（默认值）：

```text
[RECOVERY] policy dhcp_limit=3 dhcp_acquire_s=20 pdp_limit=3 at_limit=3 cfun_cooldown_s=600 hard_reset_cooldown_s=900 ...
[NET] interface-bound probe endpoints=1.1.1.1:443,8.8.8.8:443
```

定义变量，仅在确认接口无误后执行：

```sh
IFACE=usb0                    # 按实际接口修改
ip -4 addr show dev "$IFACE"
ip route show default dev "$IFACE"
```

### 3.2 统一恢复检查

每个用例的恢复成功均以以下四项为准，不能只看“进程仍在”：

- 日志有 `[RECOVERY] downtime_s=...`；
- `/tmp/dial_success` 已重新出现；
- `$IFACE` 有非 `0.0.0.0` 的 IPv4，且有该接口默认路由；
- 从 `$IFACE` 出发可连接启动日志中的任一 TCP 探测端点。

建议保存测试前后证据：

```sh
date
ip -4 addr show dev "$IFACE"
ip route show default dev "$IFACE"
cat /tmp/dial_success 2>/dev/null || true
grep -E '\[STATE\]|\[RECOVERY\]|\[NET\]|\[PDP\]|\[CFUN\]|\[POWER\]|\[USB\]|\[REG\]' <本次modem_mng日志>
```

### 3.3 实验室缩短冷却（可选）

只有测试镜像可在 `modem_mng` 的**启动环境**中设置以下变量，然后重启服务；不能在进程已经运行后 `export` 到另一个 shell 指望生效：

```text
RTMS_MODEM_DHCP_RECOVERY_LIMIT=3
RTMS_MODEM_DHCP_ACQUIRE_SECONDS=10
RTMS_MODEM_PDP_RECOVERY_LIMIT=3
RTMS_MODEM_AT_UNRESPONSIVE_LIMIT=3
RTMS_MODEM_CFUN_COOLDOWN_SECONDS=30
RTMS_MODEM_HARD_RESET_COOLDOWN_SECONDS=60
```

最低合法值分别是 1、10、1、1、30、60。注册等待 120 秒是代码常量，本版本没有环境变量可缩短。

## 4. 测试用例

### 4.1 覆盖边界与完成定义

本文件的“完整”指 **v1.25.1 当前恢复状态机的主外部可观测分支** 都有对应的测试项；不代表仅靠一块普通运营商 SIM 就能确定性注入每个 AT/USB 回包。下表中标为“工装/模拟器”的项目，若没有相应条件，应记录 `Blocked`，不能伪造 `Pass`。

| 范围 | 覆盖用例 |
|---|---|
| 冷启动、已联网进程重启、已运行但断网启动 | TC-BOOT-01、02、03 |
| USB 枚举、PDP 状态/命令、CEREG 有效和无效回包 | TC-USB-01、TC-PDP-01、TC-REG-01、TC-L3-01 |
| L1、L1→L2、L2、L2→L3、L3、L4 | TC-L1-01、02；TC-L2-01、02；TC-L3-01、02；TC-L4-01 |
| 探测端点容灾、AT 失效、冷却、服务超时、可观测性 | TC-PROBE-01、TC-AT-01、TC-COOL-01、02、TC-SVC-01、TC-OBS-01 |
| 长稳与重复恢复 | TC-SOAK-01 |

下列项目不是本版本的承诺能力，故不设“应通过”的测试结论：SIM 热插拔（依赖卡座/模组支持）、整机掉电后 L4 冷却延续（时间戳在 `/tmp`）、L3 自动升级 L4（代码未实现）。

### TC-00 基线稳定性

**目的**：确认测试起点健康，避免把已有故障误判为恢复结果。

**步骤**：保持正常网络 5 分钟，持续采集日志和接口状态。

**预期**：

- `SUCCESS` 与 `CHECK_CONNECTION` 正常循环；
- 没有 `action=enter` 的恢复日志；
- 无 `dial_failure`；
- 无 AT 超时/USB 枚举超时。

**通过**：连续 5 分钟业务和探测正常。

---

### TC-BOOT-01 冷启动正常拨号

**目的**：验证整机冷启动后，电源、USB 枚举、AT、注册、PDP、DHCP 的完整启动链路。

**步骤**：对测试板执行一次正常的整机冷启动（断电再上电或重启）。不要用“只重启 `modem_mng`”代替本用例；`/tmp/modem_started` 是否存在不是冷启动的判据，真正判据是模组从断电状态完成上电、枚举和拨号。

**观察点**：

```text
[POWER] request modem power on
[USB] waiting up to 30s for modem AT port and network interface
[DEVICE] AT ready port=... net=...
[REG] CEREG=1|5
[PDP] requested CID1 data connection
[NET] DHCP started on ...
```

**通过条件**：30 秒 USB 枚举窗口内发现 AT 与网卡；最终符合“统一恢复检查”。

---

### TC-BOOT-02 已联网时重启 `modem_mng`

**目的**：验证进程重启不会无故对已经正常的数据会话做 GPIO 断电。

**步骤**：网络正常时按平台正常方式重启 `modem_mng`，保留本次重启前后日志。

**通过条件**：控制台会输出“已经联网.”，日志状态转移为 `CONFIGURE_USB`；不能出现 `[POWER] existing run without modem connectivity` 或新的 GPIO 断电序列；网络短暂状态变化如有，需记录实际时长。

---

### TC-BOOT-03 已运行标记存在但网络失效时启动

**目的**：覆盖 `/tmp/modem_started` 已存在、启动时又无网络的保护路径。

**前置**：设备至少运行过一次，以保证 `/tmp/modem_started` 存在；确认本地控制台可用。

**注入**：使用 TC-L2-01 的 TCP 阻断规则使启动前蜂窝网络探测失败，然后重启 `modem_mng`。

**预期**：

```text
[POWER] existing run without modem connectivity; request modem power off
[POWER] request modem power on
```

**通过条件**：能完成一次受控下电再启动并恢复；删除阻断规则后无残留 DHCP/路由异常。

---

### TC-USB-01 USB 枚举超时进入 L4

**目的**：单独验证“未发现 AT+网卡”这条 USB 故障分类，而非把它混入 AT 或 RF 故障。

**注入**：用工装让模组 USB 数据通道不可见，但保持本地控制台、GPIO 控制和电源可用。

**预期**：30 秒内周期输出等待信息，超时后：

```text
[DEVICE] USB enumeration timed out ...
[RECOVERY] class=USB level=L4_HARDWARE action=enter next=RECOVER_HARDWARE ...
```

**通过条件**：故障分类必须是 `USB`，且恢复 USB 后能重新选到有效 AT 端口和网卡。L4 的电气时序另由 TC-L4-01 验收。

---

### TC-REG-01 CEREG 回包无法解析

**目的**：覆盖“AT 端口仍存在但 `AT+CEREG?` 回包无效”与“明确未注册”两个不同注册故障。

**方法**：需要可编程 AT 模拟器/模组调试固件。让基础 `AT` 返回 `OK`，但 `AT+CEREG?` 连续返回不含可解析 `+CEREG:` 的 `OK` 或畸形回包。

**预期**：120 秒后：

```text
[REG] CEREG response unparseable; waiting for registration
[REG] CEREG wait timed out after invalid responses
[RECOVERY] class=REGISTRATION level=L3_CFUN ...
```

**通过条件**：故障被归类为 `REGISTRATION`，不是 `AT`/`USB`；恢复正常 `CEREG` 回包后可继续拨号。

---

### TC-PDP-01 PDP 建立命令被拒绝

**目的**：覆盖 `AT+QNETDEVCTL=3,1,1` 被拒绝时立即进入 L2 的路径。

**方法**：优先使用 APN/核心网测试环境或 AT 模拟器，让注册保持 `CEREG=1/5`，但 PDP 建立返回 `ERROR`。不要把生产 APN 随意改成错误值作为唯一证据，因为运营商可能延迟返回或仍给出默认 APN。

**预期**：

```text
[PDP] CID1 data connection request rejected response="..."
[RECOVERY] class=PDP level=L2_PDP action=enter next=RECOVER_PDP ...
```

**通过条件**：直接进入 `L2_PDP`；恢复 PDP 服务后完成 DHCP 和联网。

---

### TC-PROBE-01 多探测端点容灾

**目的**：验证只要任一配置端点可连通，就不会把单个公网端点故障误判为 4G 故障。

**前置**：本次启动日志显示至少两个端点；确认其中一个仍可正常连接。

**注入**：仅阻断 `1.1.1.1:443`，不阻断 `8.8.8.8:443`：

```sh
iptables -N MM_IMX6_SELFTEST
iptables -I OUTPUT 1 -o "$IFACE" -j MM_IMX6_SELFTEST
iptables -A MM_IMX6_SELFTEST -p tcp -d 1.1.1.1 --dport 443 -j REJECT
```

**通过条件**：连续观察至少 2 分钟，不出现 `connection failed after 5 interface-bound internet checks`、L2/L3/L4；业务保持可用。

**回滚**：

```sh
iptables -D OUTPUT -o "$IFACE" -j MM_IMX6_SELFTEST
iptables -F MM_IMX6_SELFTEST
iptables -X MM_IMX6_SELFTEST
```

---

### TC-L1-01 默认路由丢失后的 DHCP 恢复

**目的**：验证纯本地网络配置故障只走 L1，不扰动 PDP、注册和无线功能。

**前置**：`$IFACE` 已有 IPv4 和 `default dev $IFACE` 路由。

**注入**：

```sh
ip route del default dev "$IFACE"
```

**观察点**：

```text
[NET] network interface <IFACE> has no default route after DHCP ...
[STATE] CHECK_CONNECTION -> CONFIGURE_NETWORK
[DHCP] ...
[NET] DHCP lease wait interface=<IFACE> ready=1 ...
[RECOVERY] downtime_s=...
```

**通过条件**：恢复后有默认路由、IP 和业务；全程**不能**出现 `level=L2_PDP`、`[CFUN]`、`action=power-cycle`。

**回滚**：通常 DHCP 自动恢复路由。若 60 秒仍未恢复，先保存日志，再按板卡既有方式重启 `modem_mng`；不要手工添加未知网关。

---

### TC-L1-02 DHCP 连续失败后升级到 L2

**目的**：验证 L1 三次耗尽后进入 L2，而不是无限 DHCP 循环。

**风险**：持续约 1 分钟以上断网；需要本地控制台。

**注入**：先删除 IPv4，再建立仅对本次测试有效的 DHCP 阻断链。

```sh
ip addr flush dev "$IFACE"
iptables -N MM_IMX6_SELFTEST
iptables -I OUTPUT 1 -o "$IFACE" -j MM_IMX6_SELFTEST
iptables -A MM_IMX6_SELFTEST -p udp --dport 67:68 -j DROP
iptables -I INPUT 1 -i "$IFACE" -p udp --sport 67:68 -j DROP
```

**观察点**：至少三次 DHCP/路由失败后，必须出现：

```text
... DHCP/route recovery limit exhausted
[RECOVERY] class=PDP level=L2_PDP action=enter next=RECOVER_PDP ...
```

**通过条件**：不发生 CFUN/硬件复位前，明确进入 `L2_PDP`。

**立即回滚**：

```sh
iptables -D INPUT -i "$IFACE" -p udp --sport 67:68 -j DROP
iptables -D OUTPUT -o "$IFACE" -j MM_IMX6_SELFTEST
iptables -F MM_IMX6_SELFTEST
iptables -X MM_IMX6_SELFTEST
```

随后等待 L2/DHCP 自愈；若 2 分钟未恢复，收集日志后按既有服务管理方式重启 `modem_mng`。

---

### TC-L2-01 数据路径故障后的 PDP 软重建

**目的**：验证“已注册、有 IP/路由、但公网不可达”时走 L2，而非把它误判为 DHCP 或 L3。

**前置**：从本次启动日志确认端点仍是 `1.1.1.1:443,8.8.8.8:443`。若端点被 `RTMS_MODEM_PROBE_ENDPOINTS` 改过，下面 IP 必须同步替换。

**注入**：

```sh
iptables -N MM_IMX6_SELFTEST
iptables -I OUTPUT 1 -o "$IFACE" -j MM_IMX6_SELFTEST
iptables -A MM_IMX6_SELFTEST -p tcp -d 1.1.1.1 --dport 443 -j REJECT
iptables -A MM_IMX6_SELFTEST -p tcp -d 8.8.8.8 --dport 443 -j REJECT
```

**观察点**：

```text
[NET] interface-bound internet probe failed, attempt 1 of 5
...
[NET] connection failed after 5 interface-bound internet checks
[RECOVERY] class=DATA_PATH level=L2_PDP action=enter next=RECOVER_PDP ...
[RECOVERY] class=DATA_PATH level=L2_PDP action=soft-rebuild attempt=1/3 ...
```

**通过条件**：出现一次 `soft-rebuild attempt=1/3`；在规则仍存在时不应进入 L1。此时可继续 TC-L2-02，或立即回滚。

**回滚**：

```sh
iptables -D OUTPUT -o "$IFACE" -j MM_IMX6_SELFTEST
iptables -F MM_IMX6_SELFTEST
iptables -X MM_IMX6_SELFTEST
```

删除规则后按“统一恢复检查”验收。

---

### TC-L2-02 L2 耗尽后升级到 L3/CFUN

**目的**：验证 PDP 软重建连续失败三次后执行一次 CFUN 周期。

**前置**：承接 TC-L2-01，**保持 TCP 阻断规则**，并确认 AT/USB 没被阻断。

**观察点**：

```text
[RECOVERY] ... level=L2_PDP action=soft-rebuild attempt=3/3 ...
[RECOVERY] class=PDP level=L3_CFUN action=escalate ...
[RECOVERY] ... level=L3_CFUN action=cycle attempt=1 ...
[CFUN] radio function cycle complete ...
```

**通过条件**：上述四类日志均存在；`CFUN` 成功后程序重新等待注册并重新建立数据呼叫。

**回滚**：按 TC-L2-01 删除 TCP 阻断链。恢复后必须看到 `[RECOVERY] downtime_s=...`。

**注意**：不要在规则保留时等待“L3 自动升 L4”；当前代码不会因数据探测失败自动把 L3 变成 L4。

---

### TC-L3-01 无注册网络时的 CFUN 恢复

**目的**：直接覆盖注册失败 → L3，而不依赖数据路径故障。

**注入方式（任选一个）**：

1. 将设备置于可靠的 RF 屏蔽箱；或
2. 通过测试仪拒绝该 SIM 注册。

不要通过另一 AT 客户端设置 CFUN/COPS 造故障。

**观察点**：约 120 秒内持续看到 `CEREG` 非 1/5，随后：

```text
[REG] registration wait timed out ...
[RECOVERY] class=REGISTRATION level=L3_CFUN action=enter next=RECOVER_CFUN ...
[CFUN] radio function cycle complete ...
```

**回滚**：撤销 RF 屏蔽/测试仪拒绝，等待重新注册和联网。

**通过条件**：有明确 L3 日志，撤销故障后恢复联网。持续无信号期间出现 CFUN 冷却属于预期，不是 L4 漏触发。

---

### TC-L3-02 SIM 非 READY 时的 CFUN 恢复（可选）

**目的**：覆盖 `AT+CPIN?` 非 READY → L3。

**前置**：使用专用测试 SIM、可热插拔卡座，确认拔卡不会伤害设备或业务。

**步骤**：拔出 SIM，等待日志进入 `SIM is not ready` 和 L3；插回 SIM，等待恢复。

**观察点**：

```text
[SIM] state=...
[SIM] SIM not ready
[RECOVERY] class=SIM level=L3_CFUN action=enter ...
```

**通过条件**：插卡后重新识别、注册、拨号成功。若模块或卡座不支持热插拔，不执行本用例。

---

### TC-L4-01 USB/AT 控制面丢失后的硬件恢复

**目的**：验证 L4 真正执行 GPIO 电源序列、USB 重新枚举、AT 口重新可用。

**风险**：真实断电。执行前确认 GPIO130 是模组 3.8V 使能、GPIO128 是 PWRKEY，且二者不与其他器件共用。

**推荐注入**：使用 USB/模组测试工装暂时断开模组数据通道，同时保持设备本地控制台可用。不要在不清楚 USB sysfs 绑定名时随意 `unbind` 驱动。

**观察点**：USB 枚举最多等待 30 秒，随后：

```text
[DEVICE] USB enumeration timed out ...
[RECOVERY] class=USB level=L4_HARDWARE action=enter next=RECOVER_HARDWARE ...
[RECOVERY] ... level=L4_HARDWARE action=power-cycle attempt=1 ...
[POWER] request modem power on
[USB] waiting up to 30s for modem AT port and network interface
[DEVICE] AT ready port=... net=...
```

**硬件验收**：用示波器或逻辑分析仪确认 GPIO130 和 GPIO128 时序符合：130 拉低约 2 秒、130 拉高约 1 秒、128 低 1 秒/高 2 秒/再低。

**通过条件**：AT 口和网卡重新发现，恢复联网；不能只以“打印了 power-cycle”判通过。

---

### TC-AT-01 AT 控制面失败的判别与 L4 门限

**目的**：验证 AT 调用失败不会因单次偶发失败立刻断电；基础 `AT` 连续无响应达到阈值才归类为 L4。

**方法**：需要 AT 模拟器、串口断接工装或可控的模组调试环境；无法靠 iptables 注入。分两步执行：

1. 让一次业务 AT 请求失败，但基础 `AT` 仍回复 `OK`；
2. 让基础 `AT` 连续不回复，直至达到 `at_limit`（默认 3）。

**预期**：

- 第 1 步只有 `action=retry`，不应硬复位；
- 第 2 步出现 `AT probe ... unresponsive`，随后：

```text
[RECOVERY] class=AT level=L4_HARDWARE action=enter next=RECOVER_HARDWARE ...
```

**通过条件**：AT 健康时的业务失败与 AT 不响应被区分；L4 仅在门限达到后执行。

---

### TC-OBS-01 故障状态可观测性和恢复后清理

**目的**：验证恢复过程对日志、`/tmp/dial_success` 和状态查询的一致性。

**步骤**：执行 TC-L2-01，从故障注入到恢复期间分别记录：

```sh
test -e /tmp/dial_success; echo $?
ip -4 addr show dev "$IFACE"
ip route show default dev "$IFACE"
```

同时用项目既有 38001 查询客户端读取蜂窝状态。IMX6 状态回复在故障期应包含 `cellular.pdn_status`；若 `dial_failure` 非空，也应包含 `cellular.dial_failure`。

**通过条件**：

- 状态机进入恢复后 `/tmp/dial_success` 被移除；
- 恢复后该文件重新创建，`dial_failure` 被清空；
- `pdn_status`、注册状态、IP/路由与日志相互不矛盾；
- 状态查询本身不应造成 AT 端口争用、超时或额外恢复。

---

### TC-SVC-01 初始拨号超时策略

**目的**：验证服务层的 `RTMS_MODEM_INITIAL_DIAL_TIMEOUT_SECONDS` 不会悄悄终止本应持续恢复的设备。

**步骤**：分别在测试启动环境设置 `0` 和一个小的正数（例如 `60`），以 TC-L3-01 的持续无注册故障启动服务。

**预期**：

- 值为 `0`：日志显示 `initial dial timeout=0s (0 means continuous recovery)`，恢复状态机持续运行；
- 值为 `60`：服务层在 60 秒后结束拨号线程，日志有 `dial timeout after 60s`，这是配置行为，不是恢复成功。

**通过条件**：产品启动配置应记录为 `0`，或业务明确允许有限超时；非法值必须有 `invalid RTMS_MODEM_INITIAL_DIAL_TIMEOUT_SECONDS` 日志并回退为持续恢复。

---

### TC-COOL-01 CFUN 冷却

**目的**：验证 L3 不会高频反复切无线功能。

**步骤**：在完成一次 TC-L3-01 或 TC-L2-02 的 CFUN 后，保持同类故障不恢复，继续观察。

**预期**：600 秒内不应再次出现实际 `action=cycle`，而应出现：

```text
[RECOVERY] ... reason="CFUN recovery is cooling down"
```

**通过条件**：冷却窗口内无第二次 `AT+CFUN=0/1`；窗口结束后，若故障仍在，才允许再次 CFUN。

---

### TC-COOL-02 硬件复位冷却与进程重启

**目的**：验证 L4 防抖不会因 `modem_mng` 进程重启而失效。

**步骤**：

1. 完成一次 TC-L4-01，记录 `/tmp/modem_mng_imx6ull_hard_reset_ms`；
2. 在冷却窗口内，按平台正常方式重启 `modem_mng`（不是整机重启）；
3. 保持 USB/AT 故障，观察日志。

**预期**：读取到上次时间戳并输出 `hardware recovery is cooling down`，不立即再次电源循环。

**限制**：该时间戳在 `/tmp`；通常可跨进程重启，但不保证跨整机重启。整机掉电后的行为应另列需求，不能以本用例替代。

---

### TC-ERR-01 配置错误不进入恢复风暴

**目的**：验证不可恢复配置错误被正确暴露，而不误做 CFUN/断电。

**步骤**：仅在测试启动环境中把 `RTMS_MODEM_PDP_TYPE` 设为非法值（如 `BAD`），重启 `modem_mng`。

**预期**：

```text
[APN] dialing stopped: RTMS_MODEM_PDP_TYPE is invalid
[RECOVERY] class=CONFIGURATION level=NONE action=enter next=CONFIG_ERROR ...
```

**通过条件**：无 L2/L3/L4 动作；状态接口可见拨号失败原因。

**回滚**：删除该变量或改回 `IP`、`IPV6`、`IPV4V6` 后重启服务，并完成一次正常拨号。

---

### TC-SOAK-01 重复恢复与长稳

**目的**：验证恢复成功后计数器、冷却、DHCP 进程和状态不会累积污染下一轮。

**步骤**：建议连续执行以下循环至少 20 次；量产前建议 100 次或 24 小时运行：

1. 用 TC-L1-01 注入路由丢失，等待恢复；
2. 用 TC-L2-01 注入并解除数据路径阻断，等待恢复；
3. 每轮记录恢复时长、`udhcpc` PID、IP、默认路由和日志中的 `failures/retries`；
4. 每 5 轮重启一次 `modem_mng`，确认无僵尸 `udhcpc` 和无意外 L4。

**通过条件**：

- 每轮均恢复，且恢复后 `[RECOVERY]` 成功摘要中的计数会在下一次健康周期清零；
- 每接口至多一个有效 `udhcpc`；
- 无内存持续增长、文件描述符泄漏、重复默认路由、残留防火墙规则；
- 无非预期 `CFUN` 或 `power-cycle`。

**建议附加采样**：每小时保存一次 `ps`、`ls /proc/<modem_mng_pid>/fd | wc -l`、`ip route`、日志文件大小和 `/tmp/modem_mng_imx6ull_hard_reset_ms`。

## 5. 回归结果表

| 用例 | 结果（Pass/Fail/Blocked） | 恢复耗时 | 关键日志起止行/附件 | 异常说明 |
|---|---|---:|---|---|
| TC-00 | | | | |
| TC-BOOT-01 | | | | |
| TC-BOOT-02 | | | | |
| TC-BOOT-03 | | | | |
| TC-USB-01 | | | | |
| TC-REG-01 | | | | |
| TC-PDP-01 | | | | |
| TC-PROBE-01 | | | | |
| TC-L1-01 | | | | |
| TC-L1-02 | | | | |
| TC-L2-01 | | | | |
| TC-L2-02 | | | | |
| TC-L3-01 | | | | |
| TC-L3-02 | | | | |
| TC-L4-01 | | | | |
| TC-AT-01 | | | | |
| TC-OBS-01 | | | | |
| TC-SVC-01 | | | | |
| TC-COOL-01 | | | | |
| TC-COOL-02 | | | | |
| TC-ERR-01 | | | | |
| TC-SOAK-01 | | | | |

## 6. 提交问题时必须附带的材料

- 本表和板卡/SIM/模组固件信息；
- 从故障注入前 30 秒到恢复后 60 秒的完整 `modem_mng` 日志；
- `ip -4 addr show dev $IFACE` 与 `ip route show` 的前后输出；
- 若涉及 L4：GPIO130、GPIO128、3.8V 电源波形或实测时序；
- 执行的故障注入及回滚命令；
- `/tmp/modem_mng_imx6ull_hard_reset_ms` 内容（涉及冷却时）。

## 7. 代码对照（供问题定位）

- 阶梯定义、默认策略、冷却：`imx6ull/dial/imx6ull_dialer.cpp`；
- L1/L2 与公网 TCP 探测：`CHECK_CONNECTION` / `RECOVER_PDP`；
- L3/L4：`RECOVER_CFUN` / `RECOVER_HARDWARE`；
- GPIO 电源时序：`imx6ull/dial/imx6ull_dialer.hpp::powerOnModule()`；
- DHCP 和接口绑定 TCP 探测：`imx6ull/nw/network_configurator.cpp`。
