# SDK 升级 & FOTA 部署完整分析记录
**日期**：2026-06-03 / 2026-06-04  
**版本跨度**：EC200ACNTAR02A04M2G_OCPU → EC200ACNTAR03A02M2G_OCPU  
**问题起点**：移远反馈 CPIN 瞬断为已知问题，建议升级 SDK 与固件

---

## 1. 背景

移远与 ASR 原厂沟通后确认：**EC200ACNTAR02A07M2G_OCPU 以上版本**包含修改点可解决此前观测到的 CPIN 瞬断问题。移远建议替换 SDK 并升级固件至 `EC200ACNTAR03A02M2G_OCPU`。

移远提供了三个文件：

| 文件 | 角色 |
|---|---|
| `EC200ACNTAR03A02M2G_OCPU.zip` | **固件 USB 线刷包**（非 FOTA 包） |
| `ql-ol-extsdk-ec200acntar03a02m2g_ocpu.tar.gz` | **QuecOpen SDK**（头文件/库/sysroot/sdk.mk） |
| `ql-ec200a-1803e-gcc-8.4.0-v1-toolchain.tar.gz` | **交叉工具链**（gcc-8.4.0，与 R02A04 同名同版本） |

**关键判断**：工具链文件名与现有版本完全相同，`sdk.mk` 中 `STAGING_DIR_HOST` 指向 `/opt/ql_crosstools/` 下的同名路径，**工具链无需重装**，只需替换 SDK。

---

## 2. SDK 目录结构调整（已执行）

### 2.1 操作步骤

```bash
# 新建按固件版本号命名的文件夹（与现有风格一致）
mkdir -p /home/tronlong/lyp/SDK/EC200A/EC200ACNTAR03A02M2G_OCPU/

# 将两个 tar.gz 从仓库工作区移入
mv /home/tronlong/lyp/code/open_dial/ql-ec200a-1803e-gcc-8.4.0-v1-toolchain.tar.gz \
   /home/tronlong/lyp/SDK/EC200A/EC200ACNTAR03A02M2G_OCPU/
mv /home/tronlong/lyp/code/open_dial/ql-ol-extsdk-ec200acntar03a02m2g_ocpu.tar.gz \
   /home/tronlong/lyp/SDK/EC200A/EC200ACNTAR03A02M2G_OCPU/

# 在 SDK 根目录解压（与旧版本并排，方便对比）
cd /home/tronlong/lyp/SDK/EC200A/
tar -xzf EC200ACNTAR03A02M2G_OCPU/ql-ol-extsdk-ec200acntar03a02m2g_ocpu.tar.gz
```

### 2.2 最终目录布局

```
/home/tronlong/lyp/SDK/EC200A/
├── EC200ACNTAR03A02M2G_OCPU/               ← 新建归档目录
│   ├── ql-ec200a-1803e-gcc-8.4.0-v1-toolchain.tar.gz
│   └── ql-ol-extsdk-ec200acntar03a02m2g_ocpu.tar.gz
├── ql-ol-extsdk-ec200acntar03a02m2g_ocpu/  ← 解压后新 SDK（Makefile 引用此处）
├── ql-ol-extsdk-ec200acntar02a04m2g_ocpu/  ← 旧 SDK R02A04（保留对比）
└── ql-ol-extsdk-ec200acntar02a02m2g_ocpu/  ← 旧 SDK R02A02（保留）
```

---

## 3. Makefile 修改（已执行）

### 3.1 SDK 路径更新（2 处）

| 行号 | 改动前 | 改动后 |
|---|---|---|
| Line 3 | `-include .../ql-ol-extsdk-ec200acntar02a04m2g_ocpu/sdk.mk` | `-include .../ql-ol-extsdk-ec200acntar03a02m2g_ocpu/sdk.mk` |
| Line 23 | `-I.../ql-ol-extsdk-ec200acntar02a04m2g_ocpu/ql-sysroots/usr/include` | `-I.../ql-ol-extsdk-ec200acntar03a02m2g_ocpu/ql-sysroots/usr/include` |

> Line 5 的 `INSTALL_PATH` 保留原样，该行实际未被 `all` target 使用。

### 3.2 新增 fota_update target

```makefile
FOTA_SYSROOT := /home/tronlong/lyp/SDK/EC200A/ql-ol-extsdk-ec200acntar03a02m2g_ocpu/ql-sysroots
FOTA_LDFLAGS := $(QL_SDK_FLAGS) \
    -L$(QL_TOOLCHAIN_DIR)/../usr/lib \
    -L$(QL_TOOLCHAIN_DIR)/../lib \
    -L$(FOTA_SYSROOT)/lib \
    -L$(FOTA_SYSROOT)/usr/lib \
    -lql_lib_fota -lubus -lubox -luci \
    -lblobmsg_json -ljson-c -ljson_script \
    -lrilutil -llog -lprop2uci -lmtel

fota_update: fota_update.c
    $(CC) -Wall -I$(FOTA_SYSROOT)/usr/include fota_update.c $(FOTA_LDFLAGS) -o fota_update
```

同时在 `find` 命令加 `! -name 'fota_update.c'`，防止 `fota_update.c` 被并入 `dial` 编译单元。

```makefile
APP_SRC_FILES += $(shell find $(SRC_DIR)/ -name '*.c' ! -name 'fota_update.c')
```

---

## 4. 变更说明 PDF 全面分析（V0302）

### 4.1 文档基本信息

| 字段 | 内容 |
|---|---|
| 文档名称 | EC200A-CN(TA)_QuecOpen_软件版本变更说明_V0302 |
| 文档日期 | 2025-12-12 |
| 升级基准 | EC200ACNTAR03A01M2G_OCPU → EC200ACNTAR03A02M2G_OCPU |
| 包含历史 | R02A01 ~ R03A02 全量历史 |
| 重大修改（Breaking Change） | **无**（R03A02 重大修改栏全为 "/"） |

> **open_dial 代码无需因接口签名变更而修改，可直接替换 SDK 重编。**

版本演进路径：
```
R02A01 → R02A02 → R02A03 → R02A04 → R02A05 → R02A06 → R02A07
→ R02A08 → R02A09 → R02A10 → R03A01 → R03A02
```
当前编译使用 **R02A04**，本次升级跨越 8 个子版本。

---

### 4.2 与 open_dial 直接相关的修复

#### ★★★ CPIN 瞬断 & 网络状态相关

| 编号 | 版本 | 内容 | 影响 |
|---|---|---|---|
| AUTO-160518 | R03A02 | 升级基线至 rls988 | **CPIN 瞬断根本修复载体**（CP 固件层，见第 5 节） |
| QCS-101128 | R03A02 | 修复 `CFUN=0` 后 `ql_nw_get_data_reg_status()` 返回注网状态不正确 | **直接影响 L2 恢复流程**（cfun=0→cfun=1 后查询状态可能得到错误值） |
| AUTO-48881 | R02A07 | 执行 `AT+CFUN=0` 或无网络时 `RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED` 被反复上报 | L2 恢复执行 cfun=0 期间旧固件产生大量无效状态上报，干扰 SDK 回调 |

#### ★★★ 拨号恢复逻辑相关

| 编号 | 版本 | 内容 | 影响 |
|---|---|---|---|
| AUTO-151903 | R03A02 | 优化网络恢复机制：增加对拨号实例是否为 stop 状态的判断，解决 `ql_data_call_stop()` 后网络回复时 SDK 内部自动重拨 | **直接影响 L1 恢复**：stop→sleep→start 序列，旧固件 stop 后内部仍可能重拨，与 dial.c 产生竞争 |
| QCS-48923 | R02A10 | 解决 `AT+CFUN=0` 断网后 `AT+CFUN=1` 恢复时数据呼叫重连被异常停止 | L2 恢复关键修复（R02A10 已含） |
| QCS-69074 | R03A02 | 解决弱网下两路数据拨号有一路失败 | 弱网稳定性改善，间接受益 |
| AUTO-88150 | R02A10 | 两个 APN 相同时第二个 APN 无法发起数据呼叫 | 与 APN profile 机制相关 |

#### ★★★ SIM 卡相关

| 编号 | 版本 | 内容 | 影响 |
|---|---|---|---|
| AUTO-166888 | R03A02 | `ql_sim_get_card_info()` 造成内存泄漏 | `sim.c` 周期性调用此接口，长期运行泄漏内存，可能是现场设备运行久后不稳定的隐因 |

#### ★★ AT 命令层

| 编号 | 版本 | 内容 | 影响 |
|---|---|---|---|
| QCS-101239 | R03A02 | `ql_atc_send()` 发送 ATI 命令时段错误（segfault） | `at/` 模块使用此接口，旧版特定 AT 命令下可能 crash |

#### ★★ DNS / 连通性

| 编号 | 版本 | 内容 | 影响 |
|---|---|---|---|
| AUTO-142927 | R03A02 | UDP DNS 请求超时导致无法上网 | 连通性检测依赖 DNS，旧版超时可能误判断网，触发不必要的 L1/L2 |
| AUTO-184249 | R03A02 | `ql_dnsmasq` 从 2.85 升级至 2.90 | 内置 DNS 缓存服务更新 |

#### ★ 稳定性 / 系统

| 编号 | 版本 | 内容 |
|---|---|---|
| QCS-57069 | R03A01 | ql_rild 重启后 `ql_nw_get_data_reg_status()` 无法获取注网信息（L3 重拉后曾受影响） |
| AUTO-145935 | R03A02 | cp_rd_backup 异常导致模块开机后反复重启（boot loop，旧版误判为 open_dial 未启动） |
| QCS-85470 | R03A02 | 偶因内存检测失败导致系统 dump |
| AUTO-210761 | R03A02 | 低概率因 DDR 参数异常导致模块无法开机 |
| AUTO-205533 | R03A02 | 高温环境下模块无法开机（车载场景相关） |

---

### 4.3 新增 API（对 open_dial 的潜在价值）

**AUTO-122684（R03A02）**：新增 `ql_dm_set_modem_restart()`，用于软重启 Modem（CP 层）。

- **潜在价值**：可作为未来 L2.5 级恢复手段（介于 L2 射频重置和 L3 进程退出之间）
- **注意**：需评估 OpenCPU 模式下该 API 是否与 `AT+CFUN=1,1` 一样触发 SoC 全复位；v1.28.0 已将 L3 改为纯 `exit(1)` 正是为了避免全复位影响同机业务

**QCS-59985（R03A01）**：新增 glib 库（SDK 依赖项增加），编译时若出现链接错误需检查。

---

### 4.4 跨版本历史修复（R02A04 → R03A02 期间，值得关注）

| 版本 | 编号 | 内容 | 关注点 |
|---|---|---|---|
| R02A07 | AUTO-48169 | 中国电信 SIM 卡小区接入状态不上报（概率性） | 注网回调可靠性 |
| R02A07 | AUTO-50891 | 多路数据呼叫有一路无法重连 | 重连稳定性 |
| R02A08 | AUTO-47587 | 默认禁用 chronyd | 时钟同步方式需确认 |
| R02A08 | QCS-36975 | `g_set_attach_flag` 未置 0 导致 `ql_data_call_get_status()` 失败 | `data_call.c` 状态查询 |
| R02A09 | QCS-43524 | `/etc/` 目录改为只读 | 若 open_dial 有写 /etc 的操作需排查 |
| R02A09 | AUTO-66076 | 配置默认路由失败 | 影响 ecm0 路由建立 |
| R02A10 | AUTO-83460 | GNSS 时间有效性判断，防止 NITZ 错误时间污染系统时钟 | 影响 open_dial 日志时间戳（unsynced 目录判断） |
| R03A01 | QCS-56248 | `ql_gnss_stop()` 失败返回 -1033 | GNSS 稳定性 |

---

### 4.5 资源度量（R03A02 基准）

#### Flash

| 分区 | 大小 | 已用 |
|---|---|---|
| Flash 总计 | 262144 KB（256 MB） | 61676.5 KB |
| rootfs-a/b（squashfs，只读） | 各 19968 KB | 14313 KB |
| rootfs_data（/data，UBIFS，读写） | 115072 KB（~112 MB） | 28 KB |
| cpimage-a/b | 各 15360 KB | 10083 KB |
| kernel-a/b | 各 8192 KB | 4890 KB |

#### RAM

| 指标 | 数值 |
|---|---|
| MemTotal | 93224 KB（~91 MB） |
| MemAvailable（idle） | 57300 KB（~56 MB） |
| 默认已用（idle） | ~35 MB |
| SwapTotal / SwapFree | 22524 KB / 22524 KB |
| idle 时 CPU 占用 | 2% |

#### 启动时间

| 指标 | 数值 |
|---|---|
| Systemd 服务完成 | 23.27 s |
| 模块登录时间 | 20 s |
| Modem 启动完成 | 37.21 s |
| 开机后注网 | ~2 s |
| 休眠后唤醒 | ~2 s |

> open_dial 等待 ecm0 + SDK 服务的超时设定需以"Modem 启动完成 37.21s"为基准，保留足够裕量。

#### 耗流（mA）

| 场景 | 电流 |
|---|---|
| CMCC AUTO | 2.93 mA |
| CU AUTO | 2.67 mA |
| CT AUTO | 5.33 mA |
| CFUN0（射频关闭） | 1.82 mA |
| Idle（空闲在线） | 33.58 mA |
| 电话 | 222.953 mA |
| 短信 | 169.2 mA |

> L2 恢复执行 cfun=0 期间模组射频功耗降至 1.82 mA；日常在线基准为 33.58 mA。

#### A/B 升级耗时

| 场景 | 耗时 |
|---|---|
| 整包升级（写入非活跃分区） | 112 s，CPU 最高 65% |
| 整包备份 | 85 s |

---

## 5. CPIN 瞬断根因（ASR 原厂确认，2026-06-04）

### 5.1 根因描述

| 字段 | 内容 |
|---|---|
| **触发场景** | LTE **TDD 小区**，基站配置 `SpecialSubFramePatterns = ssp0 或 ssp5` |
| **触发机制** | 测量 GAP offset 恰好落在 TTI 7 起始位置 → 触发 **CP dump（协议栈崩溃重启）** |
| **表现** | CP 重启 → SIM 重新初始化 → `CPIN: NOT READY`（约 25s）→ `CPIN: READY` |
| **本质** | 不是 SIM 卡物理接触问题，也不是上层驱动问题，是 **ASR CP 固件 L1/L2 调度 bug** |
| **ASR 修复时间** | 2022 年 12 月，修复后至今无复发 |
| **修复载体** | `EC200ACNTAR03A02M2G_OCPU`（含 rls988 基线，changelog AUTO-160518 条目） |
| **为何无法自行 patch** | ASR CP 固件**闭源**，无法独立获取或编译，只能通过移远整包固件升级 |

---

### 5.2 触发条件技术详解

#### LTE TDD 帧结构

LTE TDD 每帧（10ms）含"特殊子帧"，用于上下行切换缓冲：
```
TTI:  0    1    2    3    4    5    6    7    8    9
     [DL] [DL] [特] [UL] [UL] [DL] [DL] [特] [UL] [UL]
```
特殊子帧内部分为：`DwPTS（下行导频）| GP（保护间隔）| UpPTS（上行导频）`

#### SpecialSubFramePattern 与覆盖场景的关系

| SSP | DwPTS | GP | 典型用途 | 最大小区半径 |
|---|---|---|---|---|
| **ssp0** | 3 符号 | 10 符号 | **农村/郊区大覆盖宏站** | ~107 km |
| ssp3 | 11 符号 | 1 符号 | 城区密集小区 | ~15 km |
| **ssp5** | 3 符号 | 9 符号 | **大覆盖宏站** | ~100 km |
| ssp7 | 12 符号 | 1 符号 | 城区主流配置 | ~15 km |

**ssp0/ssp5 的长保护间隔专为大小区半径设计，主要部署在郊区/高速/山区/高铁沿线。**

#### 测量 GAP

网络通过 RRC 信令为 UE 分配测量间隙（`gapOffset`，`gapPeriod`），用于测量其他频段信号。进入新小区时重新分配 → 每次进新小区都是一次"是否触发"的抽签。

#### 触发链

```
TDD 小区（必要条件，FDD 完全不触发）
    + 基站配置 ssp0 或 ssp5（网络侧，UE 无法控制）
    + GAP offset 落在 TTI 7 起始（概率性，进新小区重新分配）
    ──────────────────────────────────────────────
    = CP 固件调度冲突 → CP dump → SIM 重新初始化
    → CPIN: NOT READY（约 25s）→ CPIN: READY
```

---

### 5.3 高风险 / 低风险场景

| 场景 | 风险等级 | 原因 |
|---|---|---|
| **高速公路行驶（省际）** | ★★★★★ | 大宏站，ssp0/ssp5 比例高，100km/h 速度每几分钟换小区 |
| **国道 / 省道（乡镇间）** | ★★★★ | 覆盖场景类似，小区更大 |
| **高铁沿线** | ★★★★★ | 铁路专用 TDD 大小区，高速移动 |
| **山区 / 丘陵穿越** | ★★★★ | 大小区覆盖，SSP 倾向长保护间隔 |
| **城区密集覆盖** | ★ | ssp7/ssp3 为主，极少 ssp0/ssp5 |
| **静止设备** | ★ | 不换小区，GAP 固定，要么一直触发要么永不触发 |
| **FDD 覆盖区域（联通/电信 B1/B3）** | 0 | 无 SSP 概念，根本不触发 |

#### 触发的概率性说明

重要区分：

- **条件 C（GAP offset = TTI7）是网络侧对该设备的固定分配**——在同一个小区内，一旦网络分配了命中 TTI7 的 GAP，该设备在此小区内**每次测量都必然触发**，不是随机的
- **"概率性"体现在进入哪个小区**：每进入一个新小区，网络重新分配 GAP，相当于重新抽一次签
- 因此：**一旦进入 ssp0/ssp5 + GAP-TTI7 的命中小区，dump 是确定性的**，不是每次有一定概率

这也解释了为什么静止设备的行为是"要么从不触发、要么周期性触发"——因为驻留在固定小区，GAP 分配不变。

#### 与 SN 52251024011503 的对照（2026-05-06 日志）

| 观测数据 | 解释 |
|---|---|
| 静止期 00:00~06:14（4.5h）零事件 | 驻留单一小区，GAP 固定；该小区不命中 |
| 行驶期 06:15~16:40，39 次事件 | 经过 69 个小区，其中 39 个为 ssp0/ssp5+TTI7 命中小区（约 57%）；每进入命中小区必然 dump |
| 停车后 20:00+ 零事件 | 再次驻留，不换小区 |
| 每次持续约 25s | 与 CP 重启耗时完全吻合 |

> **57% 的解读**：不是"每个小区有 57% 的概率触发"，而是"该行驶路线上 57% 的小区恰好是命中小区"。高速/国道沿线大宏站密集，ssp0/ssp5 部署比例本身就高，57% 符合实际地理分布。

**历史假说（TAU/eSIM REFRESH）更新**：之前假设为跨 TA 边界触发 REFRESH 导致 AT 阻塞约 25s，现在根因确认为 **CP dump 导致的 SIM 重新初始化**，两者不互斥但 CP dump 解释更准确。

---

## 6. 修复方案评估

### 6.1 有无代码层修复（open_dial 侧）

**结论：无。原因逐层分析：**

**① open_dial / AT 层无法干预 CP dump**
- CP dump 发生在 AT 接口之下（ASR 协议栈内部调度层）
- 触发条件（ssp0/ssp5）是**基站侧配置**，UE 发不了任何 AT 命令去修改它
- GAP offset 的分配是**网络侧 RRC 信令**，UE 只能接受

**② AT 命令频段锁定——理论可行，实际不可接受**

用 `AT+QCFG="band"` 锁定只使用 FDD 频段（B1/B3/B8）可完全规避 TDD 小区：

| 运营商 | FDD 频段 | TDD 频段（锁定后损失） |
|---|---|---|
| 中国移动 | B1（少量） | **B38/B39/B40/B41（主力）** |
| 中国联通 | B1/B3 | B41 |
| 中国电信 | B1/B3 | B41 |

中国移动 4G 网络大量依赖 TDD（B38/B40/B41 是其主力频段），锁 FDD 后移动卡在广大区域无法上网。对于需要全国覆盖的车载移动设备，**覆盖损失不可接受**。

**③ open_dial 侧当前行为已经足够**
- CP dump 发生 → SIM 回调感知 `CPIN: NOT READY` → 等待 `CPIN: READY`（约 25s CP 重启）→ 触发恢复拨号
- 这个路径 open_dial 已完整覆盖，downtime ≈ 25s + 重新注网 + 重拨时间
- 无需也无法进一步缩短（25s 是 CP 重启的硬时间）

### 6.2 FOTA 升级路径评估

#### 关于 EC200ACNTAR03A02M2G_OCPU.zip

**这是 USB 线刷包，不是 FOTA 包。** 内含：`update/ARBEL.bin`、`update/MSA.bin`、`update/root.squashfs`、`update/zImage` 等分区二进制 + `update.blf`（SWDownloader 烧录脚本）。需接 USB + SWDownloader 5.0.4 工具烧录，设备需进入下载模式。

**FOTA 包**是一个独立的 `.img` 文件，需向移远另行索取。

#### R02A04 → R03A02 直升的三个风险

**风险 1：OBM 必须单独更新（硬性要求）**

changelog AUTO-99501（R03A01）原文：
> For modules updated from R02 to R03 via FOTA, the OBM of the module must be updated also by using newly added script file `gen_obm_fota_image.sh` and the `quectel_obm_ota.blf` file.

R02→R03 跨大版本时 OBM（片上 Bootloader）必须单独更新，跳过则设备升级后状态未知。

**风险 2：基线版本跨度过大**

| 版本 | 基线 |
|---|---|
| R02A05 | V2102_292 |
| R02A07 | V2102_AP495_CP043（baseline 495 起点） |
| R03A02 | **rls988** |
| **R02A04** | **早于 V2102_292（未明确）** |

AUTO-171870 修复的是"从 **495 基线** FOTA 升至 988 基线失败"，R02A04 基线比 495 更老，该修复不一定完全覆盖。

**风险 3：多个 FOTA 修复存在鸡和蛋问题**

R03A02 内含 AUTO-142749（升级中断电→状态变 FAILED）、AUTO-147577（分区校验双重失败→无法启动）等 FOTA 稳定性修复。这些 fix 在新固件里——但用旧 R02A04 的 FOTA 代码写入新固件时，旧 bug 仍然存在。车载设备途中断电（点火熄火）撞上升级窗口，有变砖风险。

#### 建议升级路径

```
R02A04 ─FOTA→ R02A10 ─FOTA→ R03A02
        （中转）        （目标）
```

或向移远明确索取：**R02A04 → R03A02 FullFOTA（完整包，内嵌 OBM 处理）**，并确认：
1. 是否覆盖 pre-495 基线的直升
2. OBM 更新是否已集成到包内

---

## 7. FOTA 工具开发

### 7.1 工具文件

新增 `fota_update.c`，独立于 `dial` 的功能单一可执行文件，核心 API：

```c
ql_abfota_start_update("/media/sdcard/fota.img");  // 写入非活跃分区，约 112s
ql_abfota_get_update_status(&info);                // 轮询进度
reboot(RB_AUTOBOOT);                               // 写完后自动重启切换分区
```

编译：
```bash
make fota_update   # 使用 Makefile 内新增的 fota_update target
```

### 7.2 SSH 远程升级步骤

```bash
# === 在你的电脑上 ===

# 1. 编译工具
make fota_update

# 2. 上传 FOTA 包和工具（FOTA 包需向移远单独索取）
scp 固件包.img         root@<设备IP>:/media/sdcard/fota.img
scp fota_update        root@<设备IP>:/usr/dial/fota_update
ssh root@<设备IP> "chmod +x /usr/dial/fota_update"

# 3. 验证包完整性
ssh root@<设备IP> "md5sum /media/sdcard/fota.img"
# 与移远提供的 MD5 比对

# 4. 确认 SD 卡空间足够
ssh root@<设备IP> "df -h /media/sdcard"

# 5. 干净停止 dial（必须用 SIGINT，不能用 SIGTERM）
ssh root@<设备IP> "kill -SIGINT \$(pgrep -x dial)"
#   ↑ SIGTERM handler 在 dial.c 中被注释掉，SIGTERM 会绕过 log_close()
#   ↑ SIGINT 走 sig_handler → log_close() → 干净退出

# 6. 执行升级（约 112s，自动 reboot）
ssh root@<设备IP> "/usr/dial/fota_update /media/sdcard/fota.img"

# === 等待约 60s 设备重启完成 ===

# 7. 验证
ssh root@<设备IP> "serial_atcmd 'at+qgmr' && cat /tmp/dial_version && cat /tmp/network_status"
# 期望: 固件版本 EC200ACNTAR03A02M2G_OCPU，dial 正常启动，网络连接（1）
```

### 7.3 关于 start_prog 重拉 dial 的说明

执行 `kill -SIGINT dial` 后，**start_prog 会在约 15s 后重新拉起 dial**，这是正常行为：

- 新启动的 `dial` 进行 SDK 初始化和数据拨号（使用 `libql_sdk.so` / modem 数据通道）
- `fota_update` 写入非活跃分区（使用 `libql_lib_fota.so` / `ql_otad` 服务）
- **两者操作的是完全独立的子系统，不会相互干扰**
- 约 112s 后 `fota_update` 调用 `reboot()`，内核统一杀掉所有进程（包括 15s 后刚起来的新 dial），进入新固件

**不需要手动阻止 start_prog，也不需要等它重拉完再执行升级。**

### 7.4 FOTA 状态机

| state 值 | 含义 |
|---|---|
| UPDATE(1) | 正在写入非活跃分区 |
| BACKUP(2) | 写入完成，等待分区同步 |
| WRITEDONE(4) | 写入完成（正常退出循环） |
| SUCCEED(0) | 升级并同步全部成功 |
| FAILED(3) | 升级失败（exit_code 可查具体原因） |

---

## 8. 代码改动记录

### 8.1 TDD/FDD 小区类型检测

**需求背景**：open_dial 日志中原本无法区分当前驻留小区是 TDD 还是 FDD，无法从现场日志核实 CPIN 瞬断时的网络制式。

**改动文件三处**：

#### `at/at.c`：新增 `get_qnwinfo_safe()`

```c
/* 获取当前网络制式及频段信息 (AT+QNWINFO)
 * 回包: +QNWINFO: "TDD LTE","46000","LTE BAND 41",39150
 * 输出: "TDD/B41/F39150" 或 "FDD/B3/F1300"，无服务时输出空串 */
void get_qnwinfo_safe(char *out, int len);
```

解析逻辑：提取接入技术（TDD/FDD）、频段号（Band 41）、EARFCN（39150），组合为紧凑字符串。

#### `at/at.h`：加声明

```c
void get_qnwinfo_safe(char *out, int len);
```

#### `dial.c`：5 分钟扩展心跳加入 QNWINFO

```c
char nwinfo_str[32] = {0};
get_qnwinfo_safe(nwinfo_str, sizeof(nwinfo_str));

char ext_line[320] = {0};   // 从 256 扩至 320
int n = build_ext_line(ext_line, sizeof(ext_line), ...);
if (nwinfo_str[0] && n >= 0 && n < (int)sizeof(ext_line) - 24)
    n += snprintf(ext_line + n, sizeof(ext_line) - n,
                  "%sNW:%s", n > 0 ? " | " : "", nwinfo_str);
dial_log("[HEARTBEAT] %s\n", ext_line);
```

**升级后心跳日志示例**：
```
[HEARTBEAT] RSRP:-85 | RSRQ:-11 | CID:0A1B2C3D | IP:10.100.1.55 | NW:TDD/B41/F39150
[HEARTBEAT] RSRP:-72 | RSRQ:-8  | CID:9E8F1A2B | IP:10.100.1.55 | NW:FDD/B3/F1300
```

有了 `NW:TDD/B41/F39150` 字段，从现场日志即可直接核实：
- CPIN 瞬断发生时是否在 TDD 小区
- 具体频段（B41 是中国移动 TDD 主力频段）
- EARFCN 可精确定位频点，协助向移远提供复现信息

---

## 9. 待办事项

| 项目 | 状态 | 优先级 |
|---|---|---|
| 向移远索取 FOTA 包（确认 R02A04→R03A02 路径和 OBM 处理） | ⏳ 待发出 | P0 |
| 确认 FOTA 包 MD5 | ⏳ 待移远提供 | P0 |
| 编译新 dial（已换 R03A02 SDK）并部署到测试设备 | ⏳ 待执行 | P1 |
| 硬件固件烧录（线刷或 FOTA）到现场设备 | ⏳ 待 FOTA 包就绪 | P0 |
| 烧录后验证 CPIN 瞬断是否消失（连续运行 24h+，移动场景） | ⏳ 待烧录后执行 | P0 |
| L2 恢复回归验证（cfun=0→cfun=1 注网状态正确性，QCS-101128） | ⏳ 待部署后执行 | P1 |
| 内存泄漏观察（AUTO-166888，sim_get_card_info 泄漏已修复） | ⏳ 观察 VmRSS 72h | P2 |
| `ql_dm_set_modem_restart()` 新 API 评估（L2.5 恢复手段） | ⏳ 待调研 | P3 |

---

## 10. 本次会话执行的 git 提交

| commit | 内容 |
|---|---|
| `[BUILD] 升级 QuecOpen SDK R02A04 → R03A02` | Makefile 两处路径更新 + fota_update target + find 排除 |
| TDD/FDD 检测（未提交，待确认后一起提交） | `get_qnwinfo_safe()` + 扩展心跳改动 |

---






/* ── TDD 与 FDD 的区别 ──────────────────────────────────────────────────────
 *
 * FDD（频分双工, Frequency Division Duplex）：
 *   上行（UL）和下行（DL）使用两个不同频段同时收发，互不干扰。
 *   帧结构连续，无保护间隔，无"特殊子帧"概念。
 *   中国 FDD 主要频段：B1(2100MHz)、B3(1800MHz)、B8(900MHz)。
 *   运营商：联通/电信 FDD 为主。
 *
 * TDD（时分双工, Time Division Duplex）：
 *   上下行共用同一频段，在时间轴上交替收发。
 *   每帧（10ms/10个TTI）中含"特殊子帧"用于 DL→UL 切换缓冲，
 *   特殊子帧由 DwPTS + GP（保护间隔）+ UpPTS 组成。
 *   中国 TDD 主要频段：B38(2600MHz)、B39(1900MHz)、B40(2300MHz)、B41(2500MHz)。
 *   运营商：中国移动 TDD 为主（B38/B40/B41 是其主力），联通/电信亦有 B41。
 *
 * ──── 为何 TDD 会触发 CPIN 瞬断（CP dump）────────────────────────────────
 *
 * ASR 原厂（EC200A 芯片厂商）2022-12 确认：
 *   当基站配置 SpecialSubFramePattern = ssp0 或 ssp5（大覆盖宏站专用，
 *   保护间隔 GP 长达 9~10 个符号，支持小区半径 ~100km），且网络分配的
 *   测量 GAP offset 恰好落在 TTI 7 起始位置时，CP 固件内部调度出现竞争
 *   条件，导致协议栈崩溃重启（CP dump）。
 *
 *   CP dump → SIM 重新初始化 → CPIN: NOT READY（约 25s）→ CPIN: READY
 *   这就是现场观测到的"CPIN 瞬断"现象的根本原因。
 *
 *   FDD 无特殊子帧概念，完全不触发此 bug。
 *   TDD 城区小站（ssp7/ssp3，GP 仅 1 个符号）极少触发。
 *   高速/国道/高铁沿线大宏站（ssp0/ssp5）为高风险场景。
 *
 *   修复载体：EC200ACNTAR03A02M2G_OCPU（含 ASR rls988 基线）。
 *   修复方式：需烧录固件，纯 SDK/代码层无法绕过。
 *
 * ──── 本函数的用途 ────────────────────────────────────────────────────────
 *   通过 AT+QNWINFO 读取当前制式（TDD/FDD）、频段、EARFCN，
 *   写入 5 分钟扩展心跳日志（NW:TDD/B41/F39150），
 *   便于从现场日志直接核实 CPIN 瞬断发生时的小区制式。
 * ─────────────────────────────────────────────────────────────────────────*/

*本文件记录于 2026-06-04，基于与 Claude Code 的完整会话内容整理。*
