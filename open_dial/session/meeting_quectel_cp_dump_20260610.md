# 与移远沟通会议议题文档

**主题**：EC200A-CN CP DUMP 根因确认与固件升级路径对齐
**日期**：2026-06-10
**参与方**：我方技术负责人 / 移远技术支持
**预计时长**：60 分钟
**背景版本**：当前固件 EC200ACNTAR02A04M2G_OCPU，目标固件 EC200ACNTAR03A02M2G_OCPU

---

## 背景说明

现场设备（SN 52251024011503）日志显示：行驶期间（2026-05-06，06:15~16:40）反复出现
`CPIN: UNKNOWN → CPIN: READY` 事件，共 39 次，每次持续约 25s，停车后消失。
移远反馈该现象为已知问题，根因为 ASR CP 固件 L1/L2 调度 bug，修复包含在
`EC200ACNTAR03A02M2G_OCPU`（rls988 基线，changelog AUTO-160518）。

本次会议目标：
1. 技术层面双方对齐根因
2. 确认 固件 升级路径，拿到可部署的升级包
3. 澄清若干 SDK 行为变更，评估 open_dial 适配需求

---

## 议题一：CP DUMP 根因技术确认

### 我方已掌握的信息

| 字段 | 内容 |
|---|---|
| 触发场景 | LTE TDD 小区，基站配置 SpecialSubFramePatterns = **ssp0 或 ssp5** |
| 触发机制 | 测量 GAP offset 落在 TTI 7 起始位置 → CP 固件调度竞争 → **CP dump** |
| 现象 | CP dump → SIM 重新初始化 → `CPIN: UNKNOWN`（约 25s）→ `CPIN: READY` |
| 本质 | 非 SIM 物理接触问题，非上层驱动问题，是 **ASR CP 固件闭源 bug** |
| ASR 修复时间 | 2022 年 12 月 |
| 修复载体 | EC200ACNTAR03A02M2G_OCPU（AUTO-160518，含 rls988 基线） |
| 为何无法自行修复 | ASR CP 固件闭源，无法独立编译，只能通过移远整包固件升级 |

### 需移远在会上明确的问题

1. 以上根因描述是否准确？是否有 ASR 原厂书面文档可以提供？
2. 该修复是纯 CP 固件层，还是 AP 侧（SDK/RIL）也有配合修改？
4. ssp0/ssp5 场景在 R03A02 之后是否还有其他遗留已知问题？

---

## 议题二：固件升级方案确认

> 本议题是会议核心，需要移远给出明确的可操作方案。

### 2.0 线刷包与 FOTA 包的区分（需先对齐）

移远本次提供的 `EC200ACNTAR03A02M2G_OCPU.zip` 是 **USB 线刷包**，不是 FOTA 包。

| 对比项 | USB 线刷包（已收到） | FOTA 包（待索取） |
|---|---|---|
| 文件形态 | `.zip`，内含 `update/ARBEL.bin`、`update/MSA.bin`、`update/root.squashfs`、`update/zImage` 等分区二进制 + `update.blf` 烧录脚本 | 独立 `.img` 文件 |
| 操作方式 | USB 物理连接 + SWDownloader 5.0.4 工具 + 设备进入下载模式 | SSH 远程，通过 `ql_abfota_start_update()` API 写入非活跃分区，约 112s，写完自动重启 |
| 适用场景 | 实验室测试设备初始烧录 / 现场变砖后救砖 | 现场设备远程升级 |
| 是否已有 | 已收到 | **尚未提供，需本次会议索取** |

**本次会议需明确**：
1. 请移远提供对应升级路径的 **FOTA 包（.img）**，以及每个包的 **MD5 校验值**。
2. 线刷包可用于实验室测试设备的先行烧录验证，再向现场设备推送 FOTA；移远是否认可此验证路径？

---

### 当前状况

- 设备当前固件：**R02A04**
- 目标固件：**R03A02**
- 现场升级方式：SSH 远程 FOTA（设备部署在车载环境，无法接线刷）
- 设备场景：车载，升级窗口短（点火/熄火），中途断电风险客观存在

### 已识别的三个直升风险

**风险 1：OBM 必须单独更新（changelog AUTO-99501 明确要求）**

> For modules updated from R02 to R03 via FOTA, the OBM of the module must be
> updated also by using newly added script file `gen_obm_fota_image.sh` and the
> `quectel_obm_ota.blf` file.

问题：移远提供的 FOTA 包是否已内置 OBM 处理？还是需要我们手动执行脚本？

**风险 2：基线版本跨度超出已知修复范围**

changelog AUTO-171870 修复的是"从 495 基线 FOTA 升至 rls988 失败"，
但 R02A04 的基线早于 V2102_292，比 495 更老，该修复不一定完全覆盖。

问题：R02A04 的确切基线版本号是多少？AUTO-171870 是否覆盖我们的场景？

**风险 3：FOTA 稳定性 fix 的鸡蛋问题**

R03A02 内含以下 FOTA 稳定性修复：
- AUTO-142749：升级中断电 → 状态变 FAILED
- AUTO-147577：分区校验双重失败 → 无法启动

这些 fix 在新固件里，但执行升级时用的是 R02A04 旧代码，旧 bug 仍然存在。
车载设备途中断电撞上升级窗口，有变砖风险。

### 需移远明确的升级方案

请移远给出以下其中一种方案，并说明理由：

**方案 A（两跳）**
```
R02A04 ─FOTA→ R02A10 ─FOTA→ R03A02
```
提供两个 FOTA 包，分两次部署。R02A10 作为安全中转，规避基线跨度和鸡蛋问题。

**方案 B（直升 FullFOTA）**
```
R02A04 ─FullFOTA→ R03A02
```
移远需提供专为 pre-495 基线直升定制的完整包，并书面确认：
- OBM 更新已内置
- 覆盖 R02A04 基线的直升
- 中途断电的安全保障

### 期望输出物

| 输出物 | 说明 |
|---|---|
| FOTA 包文件 | 对应升级路径的 `.img` 文件 |
| MD5 校验值 | 每个包各一个，用于设备端完整性验证 |
| 书面升级路径确认 | 邮件或文档形式，说明已覆盖的风险点 |

---

### 2.4 不刷固件能解决什么——改动分层

升级动作分两层：**① 只更新 open_dial 应用**（重编 + 部署，不动模组固件）；**② 刷固件**（固件包含 rootfs 中的 libql_sdk.so 等运行时库和 CP 固件）。两层效果截然不同，需在会上与移远对齐边界。

#### 第一层：仅更新 open_dial 应用（无需刷固件）

| 改动 | 能解决的问题 | 备注 |
|---|---|---|
| L1 恢复 `stop → sleep(2) → start` 序列加状态轮询 | 在旧固件上缓解 AUTO-151903 的竞争（stop 后 SDK 内部仍自动重拨），通过轮询确认 stop 状态再执行 start，降低竞争概率 | 属于 workaround，不是根本修复 |
| L2 恢复 cfun=0 后加重试逻辑 | 在旧固件上缓解 QCS-101128 的误读（cfun=0 后注网状态返回不正确），通过多次重试取稳定值 | 不是根本修复 |

**结论**：纯应用层改动不能解决根本性问题，**无法消除任何 changelog 中的根本 bug**——因为那些 fix 在 libql_sdk.so（运行时库，随固件发布）和 CP 固件里，不在 open_dial 代码里。

#### 第二层：刷固件后才能根本修复的问题

| 问题 | changelog 编号 | 影响 |
|---|---|---|
| **CPIN 瞬断 / CP DUMP** | AUTO-160518（rls988 基线） | 核心问题，CP 固件 bug，不刷无解 |
| `ql_sim_get_card_info()` 内存泄漏 | AUTO-166888 | 长时间运行内存持续增长，libql_sdk.so 内部问题 |
| stop 后 SDK 内部自动重拨 | AUTO-151903 | libql_sdk.so 内部状态机问题 |
| cfun=0 后注网状态不正确 | QCS-101128 | libql_sdk.so 问题，旧固件永久存在 |
| UDP DNS 请求超时 | AUTO-142927 | 系统 ql_dnsmasq 问题，open_dial 无法绕过 |
| `ql_atc_send()` segfault | QCS-101239 | libql_sdk.so 问题，特定 AT 命令下 crash |
| cp_rd_backup 导致 boot loop | AUTO-145935 | CP 固件问题，不刷无解 |

#### 需移远确认的问题

1. **libql_sdk.so 能否单独更新**（不做全量固件刷写）？部分 AP 侧 fix（如 AUTO-151903、AUTO-166888）理论上只涉及 libql_sdk.so，是否可以单独推送 .so 文件到 `/usr/lib/` 而不做完整 FOTA？若可行，可作为现场设备的最小风险中间方案。
2. **R03A02 SDK 头文件编译的 dial 能否运行在 R02A04 固件上**（即 libql_sdk.so 版本不匹配时是否有 ABI 兼容性风险）？这决定了"先部署新 dial、再刷固件"的步骤顺序是否安全。

---

## 议题三：R03A02 SDK 行为变更对齐

### 3.1 AUTO-151903：stop 后 SDK 内部自动重拨行为

**旧固件问题**：`ql_data_call_stop()` 执行成功后，SDK 内部仍可能触发自动重拨，
与 open_dial L1 恢复的 `stop → sleep(2) → start` 序列产生竞争，导致拨号实例状态混乱。

**R03A02 修复**：增加了对拨号实例 stop 状态的判断，stop 后不再自动重拨。

**需确认**：
- 修复后，stop 之后 SDK 是"完全静默，等待上层主动 start"，还是有超时重试机制？
- 若有超时重试，超时时间是多少？我们的 `sleep(2)` 冷却间隔是否需要调整？

### 3.2 QCS-101128：cfun=0 后注网状态查询返回值

**旧固件问题**：执行 `AT+CFUN=0` 后，`ql_nw_get_data_reg_status()` 返回的注网状态不正确，
导致 L2 恢复（cfun=0 → cfun=1 → data_call_start）的状态判断出现偏差。

**需确认**：
- 修复后，cfun=0 期间该接口应返回什么值（期望值）？
- cfun=1 完成后，注网状态回归正常的最大延迟是多少？方便我们设定合理的等待窗口。

### 3.3 AUTO-122684：新 API ql_dm_set_modem_restart() 行为咨询

**背景**：open_dial v1.28.0 已将 L3 恢复由 `AT+CFUN=1,1`（SoC 全复位）改为纯 `exit(1)`
（进程重启），原因是 EC200A OpenCPU 模式下 `AT+CFUN=1,1` 会复位整个 SoC，
中断同机 WiFi 及其他业务，无法接受。

R03A02 新增 `ql_dm_set_modem_restart()`，用于软重启 Modem（CP 层）。
该 API 对我们有潜在价值——若能做到"只重启 CP，AP/Linux 继续运行"，
可作为 L2.5 级恢复手段（介于射频重置和进程退出之间）。

**需确认**：
- 在 **EC200A OpenCPU 模式**下，`ql_dm_set_modem_restart()` 是只重启 CP，
  还是同样触发 SoC 全复位（等效于 `AT+CFUN=1,1`）？
- 若只重启 CP，CP 重启期间 AP 侧 SDK（libql_sdk.so）的行为是什么？
  是否会有回调通知，是否需要重新调用 SDK 初始化流程？

---
