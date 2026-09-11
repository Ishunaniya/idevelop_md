# Quectel EC200A-CN(TA) QuecOpen 软件版本变更说明分析
## 文档信息

| 字段 | 内容 |
|---|---|
| 源文档 | EC200A-CN(TA)_QuecOpen_软件版本变更说明_V0302 |
| 文档日期 | 2025-12-12 |
| 固件版本 | EC200ACNTAR03A02M2G_OCPU（相对于 R03A01 的增量变更说明） |
| QuecOpen SDK | ql-ol-extsdk-ec200acntar03a02m2g_ocpu.tar.gz |
| 工具链 | ql-ec200a-1803e-gcc-8.4.0-v1-toolchain.tar.gz（**与 R02A04 同版本**，无需重装） |
| 分析日期 | 2026-06-03 |

---

## 1. 版本跨度概述

本变更说明是从 **EC200ACNTAR03A01M2G_OCPU 到 EC200ACNTAR03A02M2G_OCPU** 的增量说明，但文档同时包含了从 R02A01 到 R03A02 的**全量历史**（按版本分区块列出），便于从旧版本跨版本评估影响。

版本演进路径（从旧到新）：
```
R02A01 → R02A02 → R02A03 → R02A04 → R02A05 → R02A06 → R02A07 → R02A08 → R02A09 → R02A10 → R03A01 → R03A02
```
> open_dial 当前编译使用的是 **R02A04**，本次升级跨越 8 个子版本。

---

## 2. 重大修改（Breaking Changes）

**R03A02 的"重大修改"一栏全为 "/"，无任何破坏性 API 变更。**

open_dial 代码无需因接口签名变更而修改，可直接替换 SDK 重编。

---

## 3. 与 open_dial 直接相关的修复（按重要性排序）

### 3.1 ★★★ CPIN 瞬断根因相关

#### 根因（ASR 原厂确认，2026-06-04）

| 字段 | 内容 |
|---|---|
| **触发场景** | LTE **TDD 小区**，基站配置了 `SpecialSubFramePatterns = ssp0 或 ssp5` |
| **触发机制** | 测量 GAP offset 恰好落在 TTI 7 起始位置 → 触发 **CP dump**（协议栈崩溃重启） |
| **表现** | CP dump → SIM 重新初始化 → `CPIN: NOT READY` → `CPIN: READY`，即所谓"瞬断" |
| **本质** | 不是 SIM 卡物理接触问题，也不是上层驱动问题，是 **CP 固件（ASR 闭源）的 L1/L2 调度 bug** |
| **ASR 修复时间** | 2022 年 12 月（修复后至今未再发现类似问题） |
| **修复载体** | 固件升级至 `EC200ACNTAR03A02M2G_OCPU`，含 ASR rls988 基线（**AUTO-160518** 条目） |
| **为何无法自行打 patch** | ASR CP 固件**闭源**，无法独立获取或编译，只能通过移远发布的整包固件升级 |

> **结论**：changelog 中无独立"CPIN"条目是正常的——CP 层 bug 修复统一归入基线版本升级（rls988），不在上层单独列出。**必须烧录固件才能修复，仅替换 SDK 重编无效。**

**QCS-101128（Network，R03A02）**
- 问题：执行 `AT+CFUN=0` 后，调用 `ql_nw_get_data_reg_status()` 返回的注网状态不正确。
- 影响：open_dial 的 **L2 恢复流程**（cfun=0 → cfun=1 → data_call_start）在旧固件中，cfun=0 执行后查询注网状态可能得到错误值，导致 L2 状态判断偏差，进而引发异常重拨或延迟恢复。
- 修复后：cfun=0 期间注网状态能正确反映为"已去注册"，L2 恢复逻辑更可靠。

---

### 3.2 ★★★ 拨号恢复逻辑直接相关

**AUTO-151903（Network，R03A02）**
- 问题：网络异常时，调用 `ql_data_call_stop()` 断开拨号成功后，网络回复时系统会**自动重拨**（SDK 内部重连未被 stop 状态门控）。
- 影响：open_dial 的 L1/L2 恢复流程依赖"stop → sleep → start"序列。旧固件 SDK 在执行 stop 后，内部可能仍触发自动重拨，与 dial.c 的恢复序列产生竞争，造成拨号实例状态混乱。
- 修复后：SDK 内部增加了对拨号实例 stop 状态的判断，stop 后不再自动重拨，恢复序列由上层（open_dial）完全控制。

**QCS-69074（Data Service，R03A02）**
- 问题：弱网环境下，两路数据拨号有一路拨号失败。
- 影响：open_dial 使用单路拨号，但此修复改善了 Data Service 模块在弱网下的通用稳定性，间接受益。

**AUTO-88150（Data Service，R02A10）**（跨版本历史修复）
- 问题：两个 APN 相同时，第二个 APN 无法发起数据呼叫。
- 影响：与我们的 APN profile 机制（DATA_CALL_APN_PUBLIC=6）相关，此修复已在 R02A10 中完成。

---

### 3.3 ★★★ SIM 卡相关

**AUTO-166888（(U)SIM，R03A02）**
- 问题：调用 `ql_sim_get_card_info()` 造成**内存泄漏**。
- 影响：open_dial 的 `sim.c` 模块在主循环中周期性调用此接口查询 SIM 状态。旧固件长期运行存在内存泄漏风险，可能是某些现场设备长时间运行后稳定性下降的隐因。
- 修复后：无内存泄漏。

---

### 3.4 ★★ AT 命令层相关

**QCS-101239（GENERAL，R03A02）**
- 问题：通过 `ql_atc_send()` 发送 ATI 命令时出现**段错误**（segfault）。
- 影响：open_dial 的 `at/` 模块使用 `ql_atc_send()` 发送各类 AT 命令。旧固件在特定 AT 命令（ATI）下会 crash，可能影响 diag 诊断流程中的 AT 采集。

**AUTO-170178（GENERAL，R03A02）**
- 修复 `AT+QFIREWALL` 命令的**注入漏洞**。
- 与 open_dial 无直接关联，但属于系统安全加固。

---

### 3.5 ★★ DNS/网络连通性相关

**AUTO-142927（GENERAL，R03A02）**
- 问题：UDP DNS 请求超时导致**无法上网**。
- 影响：open_dial 的连通性检测（`misc.c`）依赖 ping/DNS，旧固件的 DNS 请求超时可能导致连通性误判（判断为断网而实际网络正常），触发不必要的 L1/L2 恢复。
- 修复后：DNS 解析更可靠，减少误触发。

**AUTO-184249（Data Service，R03A02）**
- `ql_dnsmasq` 从 2.85 升级至 2.90，内置 DNS 缓存服务更新。

---

### 3.6 ★ 稳定性/系统级修复

**AUTO-210761（GENERAL，R03A02）**
- 低概率因 DDR 参数异常导致模块无法开机。

**AUTO-205533（GENERAL，R03A02）**
- 修复高温环境下模块无法开机的问题（车载场景相关）。

**AUTO-145935（GENERAL，R03A02）**
- 解决 `cp_rd_backup` 异常导致模块开机后**反复重启**（boot loop）的问题。
- 影响：此类 boot loop 在旧固件中会被误判为 open_dial 未启动，实际是固件层问题。

**QCS-85470（GENERAL，R03A02）**
- 解决偶因内存检测失败导致系统 dump 的问题（影响整体稳定性）。

**QCS-57069（NETWORK，R03A01）**（历史修复）
- ql_rild 进程重启后，`ql_nw_get_voice_reg_status()` 和 `ql_nw_get_data_reg_status()` 无法获取注网信息。
- 影响：open_dial 在 L3 恢复（exit 重拉进程）后调用这些接口可能曾遇到此问题，R03A01 已修复。

---

## 4. 新增 API（对 open_dial 的潜在价值）

**AUTO-122684（GENERAL，R03A02）**
- 新增 `ql_dm_set_modem_restart()`：用于**软重启 Modem（CP 层）**。
- 意义：目前 open_dial v1.28.0 已将 L3 恢复改为纯 `exit(1)`（不再用 `AT+CFUN=1,1` 复位 SoC）。此新 API 提供了一个"只重启 CP，不重启 AP/Linux"的选项，可作为未来 L2.5 级恢复手段评估（介于射频重置和进程重启之间），但需要评估在 OpenCPU 模式下该 API 是否同样触发 SoC 全复位。

---

## 5. 跨版本历史修复中值得关注的条目（R02A04 → R03A02 期间）

| 版本 | 编号 | 内容 | 相关性 |
|---|---|---|---|
| R03A01 | QCS-59985 | **新增 glib 库**（SDK 依赖项增加） | 编译环境新增依赖，若出现链接错误需检查 |
| R02A07 | AUTO-48881 | 解决执行 `AT+CFUN=0` 或无网络时，`RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED` 被**反复上报**的问题 | L2 恢复执行 cfun=0 期间，旧固件会产生大量无效状态上报，可能干扰 open_dial 的 SDK 回调处理 |
| R02A07 | AUTO-48169 | 解决使用中国电信 SIM 卡时小区接入状态不上报的概率性问题 | 影响注网回调可靠性 |
| R02A07 | AUTO-50891 | 解决多路数据呼叫中有一路无法重连的问题 | 拨号重连稳定性 |
| R02A08 | AUTO-47587 | 默认禁用 chronyd（时钟同步服务默认关闭） | open_dial 依赖 NTP 同步，需确认时钟同步方式不受影响 |
| R02A08 | QCS-36975 | `g_set_attach_flag` 未置 0 导致 `ql_data_call_get_status()` 获取状态失败 | data_call.c 的状态查询 |
| R02A09 | QCS-43524 | `/etc/` 目录改为**只读** | 若 open_dial 有写 /etc 的操作需排查 |
| R02A09 | AUTO-66076 | 解决配置默认路由失败的问题 | 影响 ecm0 路由建立 |
| R02A10 | QCS-48923 | 解决 `AT+CFUN=0` 断网后 `AT+CFUN=1` 恢复时数据呼叫重连被**异常停止**的问题 | L2 恢复流程关键修复（R02A10 已含） |
| R02A10 | AUTO-83460 | GNSS 时间有效性判断，防止基站 NITZ 错误时间污染系统时钟 | 影响 open_dial 的日志时间戳（unsynced 目录判断） |
| R03A01 | AUTO-99913 | 基线更新至 AP846_CP067 | CP 固件基线变更 |

---

## 6. 资源度量（R03A02 基准）

### 6.1 Flash 资源

| 指标 | 数值 |
|---|---|
| Flash 总大小 | 262144 KB（256 MB） |
| Flash 已用 | 61676.5 KB（~60 MB） |
| rootfs-a/b（squashfs，只读） | 各 19968 KB，已用 14313 KB |
| rootfs_data（/data，UBIFS，读写） | 115072 KB（~112 MB） |
| cpimage-a/b | 各 15360 KB，已用 10083 KB |
| kernel-a/b | 各 8192 KB，已用 4890 KB |
| oem_data-a/b（/NVM/oem_data，只读） | 各 20480 KB，已用 384 KB |

与 R02A04 相比，rootfs 占用基本持平（14313 KB），分区布局无变化。

### 6.2 RAM 资源

**物理内存**：系统预留 127 MB（Linux 不可见），通过 `cat /sys/kernel/debug/memblock/memory` 查看。

**Linux 可用内存（idle 状态）**：

| 指标 | 数值 |
|---|---|
| MemTotal | 93224 KB（~91 MB） |
| MemFree | 33320 KB |
| MemAvailable | 57300 KB（~56 MB）|
| Buffers | 6296 KB |
| Cached | 19736 KB |
| SwapTotal / SwapFree | 22524 KB / 22524 KB |
| Slab | 19568 KB |
| SUnreclaim | 13308 KB |
| CommitLimit | 69136 KB |
| Committed_AS | 21840 KB |
| VmallocTotal | 901120 KB |
| VmallocUsed | 5592 KB |
| 默认已用（MemTotal − MemAvailable） | ~35 MB |
| idle 时 CPU 占用 | 2% |

open_dial 进程本身内存极小（<2 MB），idle 下整机仍有 **~56 MB** 空闲，无内存压力。

### 6.3 Linux 启动时间

| 指标 | 数值 |
|---|---|
| Systemd 服务完成时间 | 23.27 s |
| 模块登录时间 | 20 s |
| Modem 启动完成时间 | 37.21 s |

> open_dial 的 `main.c` 等待 ecm0 网卡 + SDK 服务的超时设置需参考"Modem 启动完成 37.21 s"这个基准，目前代码中的等待逻辑应留足裕量。

### 6.4 耗流测试（mA）

| 场景 | 电流（mA） |
|---|---|
| CMCC AUTO（移动自动注网） | 2.93 |
| CU AUTO（联通自动注网） | 2.67 |
| CT AUTO（电信自动注网） | 5.33 |
| CFUN0（射频关闭） | 1.82 |
| Idle（空闲） | 33.58 |
| 电话（通话中） | 222.953 |
| 短信 | 169.2 |

> 车载场景重点关注 **Idle 33.58 mA**（open_dial 持续在线的功耗基准）和 **CFUN0 1.82 mA**（L2 恢复执行 cfun=0 期间模组射频功耗）。

### 6.5 A/B 系统升级耗时

| 升级场景 | RAM MemAvailable | CPU Loading | 耗时（s） |
|---|---|---|---|
| 整包升级 | 56324 KB | 65% | 112 |
| 整包备份 | 56448 KB | 21% | 85 |

> 升级期间 CPU 占用最高 65%，open_dial 若在此期间检测到连通性下降，需注意区分"升级导致的短暂中断"与真实网络故障。

### 6.6 开机注网 / 休眠唤醒耗时

| 场景 | 耗时（s） |
|---|---|
| 模块开机后注网 | 2 |
| 模块休眠后唤醒 | 2 |

> 备注（文档原文）：受网络环境、(U)SIM 卡状态等因素影响，可能存在差异。
> open_dial 的 L0 阶段（0~5 min 不干预）已远大于此基准，无需调整。

---

## 7. 升级操作清单

### 7.1 硬件固件烧录（必须，否则 CPIN 瞬断问题无法修复）
- 工具：SWDownloader 5.0.4
- 固件包：`EC200ACNTAR03A02M2G_OCPU.zip`（`update/` 目录下各 bin 文件）
- 验证环境：Windows 10 + Quectel_Windows_USB_Driver(A)_Customer_V1.4.0_1016

### 7.2 SDK 替换与重编（已完成）
- [x] 创建 `/home/tronlong/lyp/SDK/EC200A/EC200ACNTAR03A02M2G_OCPU/` 目录
- [x] 移入 `ql-ec200a-1803e-gcc-8.4.0-v1-toolchain.tar.gz` 和 `ql-ol-extsdk-ec200acntar03a02m2g_ocpu.tar.gz`
- [x] 在 SDK 根目录解压新 SDK（生成 `ql-ol-extsdk-ec200acntar03a02m2g_ocpu/`）
- [x] Makefile Line 3：`-include` 路径改为 R03A02
- [x] Makefile Line 23：头文件 `-I` 路径改为 R03A02
- [ ] 执行 `make clean && make` 重新编译
- [ ] 将新编译的 `dial` 部署到硬件

### 7.3 回归验证要点
1. **CPIN 瞬断**：连续运行 24h，观察 SIM 状态是否出现瞬断（`CPIN: NOT READY` → `CPIN: READY`）
2. **L2 恢复**：手动触发 cfun=0 → cfun=1，验证注网状态查询正确性
3. **L1 恢复**：验证 stop 后 SDK 不再自动重拨，恢复序列由 open_dial 主导
4. **DNS 解析**：验证连通性检测无误判
5. **长时间内存**：运行 72h 后检查 `/proc/<pid>/status` 的 VmRSS 无持续增长

---

## 8. 已知问题（R03A02 文档标注）

文档"已知问题"一栏全为 "/"，即**官方无未解决的已知问题**。

---

*分析基于：Quectel_EC200A-CN-TA_QuecOpen_软件版本变更说明_V0302.pdf（2025-12-12）*
