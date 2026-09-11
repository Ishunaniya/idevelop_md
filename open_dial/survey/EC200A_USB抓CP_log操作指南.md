# EC200A —— 电脑经 USB 抓取 CP Log 操作指南

> 适用机型：**Quectel EC200A（ASR1803，OpenCPU 模式，aarch64）**
> 现网固件：**EC200ACNTAR02A04M2G_OCPU（R02A04）**
> 目标：移远要求"电脑用 USB 线接设备抓 **CP log**"（不是 CP dump）
> 参考原文：
> - `md/ec200a_wd/quectel_log_guide_analysis.md`（AG35 QuecOpen Log 抓取指导 V1.0.0 分析）
> - `md/ec200a_wd/EC200A_QuecOpen_USB_配置指导.md`（USB 组合口/DIAG 口）
> - `md/ec200a_wd/QT-AN-01-007_ASR平台qllog抓取日志_完整解析.md`（qllog 适配性/数据量级）
> 编写日期：2026-07-24

---

## 0. 先分清：CP log ≠ CP dump（别抓错层）

| | **CP log**（本文目标） | CP dump |
|---|---|---|
| 是什么 | CP（Modem/基带）**实时运行日志**，`.sdl` 格式 | CP **崩溃瞬间的内存快照** |
| 怎么抓 | 电脑 **CATStudio + USB DIAG 口** 实时录制 | 崩溃时自动存 `/sdcard/`，或进 dump 模式经 RNDIS+TFTPD64 传 |
| 触发时机 | 正常运行期间连续录，**主动复现问题** | 只在 CP 发生 dump 时才有 |
| 工具 | CATStudio、Quectel USB 驱动、解析数据库 | TFTPD64 / 直接拷 SD 卡文件 |

> **本文只讲 CP log。** CP dump 是另一套流程（见 `quectel_log_guide_analysis.md` §2.2）。
> ⚠️ 如果移远真正想要的是 CP **崩溃 dump**，直接从设备 `/media/sdcard`（bind mount 到 `/sdcard`）拷文件即可，别走本文流程。

---

## 1. 抓 CP log 的唯一途径：CATStudio + USB（重要）

EC200A 上抓 CP log **只有 CATStudio + USB DIAG 口这一条路**。有个看似更省事的替代——移远的 `qllog -s modem`
命令（文档写明"和 CatStudio 抓取保存的日志相同"）——但**在本机型不可用**：

> qllog / ql_logd 服务**仅 ASR1806 平台 + 2024-12 之后 SDK 才提供**（QT-AN-01-007 第 1 章原文）。
> 本设备为 EC200A / **ASR1803**，2026-07-23 实测：设备内无 `qllog` 二进制、无 `ql_logd` 进程、无 `/etc/ql_logd.conf`；
> 三版 EC200A SDK（r02a02/r02a04/r03a02）均无 `ql_log.h`。属**平台代差**，无补装路径。

**结论：别在 EC200A 上找 qllog 抓 CP log，抓 CP log = CATStudio + USB DIAG 口。**

---

## 2. 需要准备的东西（清单）

| # | 项目 | 说明 | 从哪来 | 本地是否已有 |
|---|---|---|---|---|
| 1 | **USB 线** | 接设备的 **USB DIAG 口**（枚举出 `Quectel USB DIAG Port`），不是普通供电口 | 你们硬件 | — |
| 2 | **Windows 电脑** | CATStudio 是 Windows 工具 | — | — |
| 3 | **Quectel USB 驱动** | 装上电脑才认得出 `Quectel USB DIAG Port`，否则 CATStudio 连不上 | ⚠️ **找移远技术支持要** | ❌ 需索取 |
| 4 | **CATStudio 软件** | 抓 CP log 的主工具 | ⚠️ **找移远协助安装** | ❌ 需索取 |
| 5 | **CP log 解析数据库**（2 个）| `MDB.txt`（Communication）+ `Boerne_DIAG.mdb.txt`（Application），CATStudio 靠它把二进制流解析成可读日志 | 固件调试包 `dbg/` 目录 | ⚠️ **只有 R03A02，缺 R02A04**（见 §6）|

> **两项前置条件（均已就绪，见 §3）**：① 设备 `default_media=1`（USB）；② 默认 USB 组合口含 DIAG 口。

---

## 3. 两项前置条件核查（抓前先确认）

### 3.1 default_media=1（CP log 走 USB）
- CP log 输出方式由 `/etc/mrvl_tel_diag.cfg` 的 `default_media` 决定：`1=USB(默认)`、`3=TCP`。
- **2026-07-23 设备实测 = 1**，前置已满足，**不用改**。
- 若某台设备连不上 CATStudio，先查这项：改回 `default_media=1`、保存、**重启生效**。
- `AT+QCFG="cplog"` 默认 =1，CP log 默认就在输出，无需额外打开。

### 3.2 USB DIAG 口默认已暴露
- EC200A 默认 USB 组合口为 `ecm,marvell_diag,acm,marvell_modem,adb`（USB 配置指导 §2.2），
  其中 **`marvell_diag` 即 CATStudio 要用的 DIAG 口**——默认就在，**一般不需要切 usbcfg**。
- 设备侧自查命令（可选，坐实）：
  ```bash
  cat /sys/devices/virtual/android_usb/android0/functions   # 应含 marvell_diag
  cat /sys/devices/virtual/android_usb/android0/state        # 应为 CONFIGURED（枚举成功）
  cat /sys/class/udc/mv-udc/current_speed                    # high-speed = USB2.0 正常
  ```
- 若 `functions` 里缺 `marvell_diag`（被改过组合口），临时恢复（重启即复位，仅调试用）：
  ```bash
  echo 0 > /sys/devices/virtual/android_usb/android0/enable
  echo ecm,marvell_diag,acm,marvell_modem,adb > /sys/devices/virtual/android_usb/android0/functions
  echo 1 > /sys/devices/virtual/android_usb/android0/enable
  sync
  ```

---

## 4. 数据库文件：EC200A 与 AG35 文档的差异（重要）

参考文档基于 **AG35**，其 Communication 库叫 `LWG_MDB.txt`。**EC200A 不叫这个名字**——
2026-07-24 本地固件调试包实测：

| 角色 | AG35 文档写的 | **EC200A 实际** |
|---|---|---|
| Communication 库 | `LWG_MDB.txt` | ❌ 改名为 **`MDB.txt`**（~12.7 MB）|
| Application 库 | `Boerne_DIAG.mdb.txt` | ✅ **同名 `Boerne_DIAG.mdb.txt`**（~83 KB）|

> **若照 AG35 文档去 CATStudio 里找 `LWG_MDB.txt` 会找不到、数据库匹配不上、Logger 不亮绿灯。**
> EC200A 请选 `MDB.txt` 作为 Communication 库。

R03A02 调试包实测目录（本地已有，仅供了解结构，**不是现网 R02A04 的库**）：
```
/home/tronlong/lyp/SDK/EC200A/ASR_CP_DUMP_FIRMWARE/EC200ACNTAR03A02M2G_OCPU/dbg/
├── MDB.txt              12.7 MB   ← Communication 数据库
├── Boerne_DIAG.mdb.txt  83 KB     ← Application 数据库
└── file-in-system-image.txt
```

---

## 5. 操作步骤（EC200A 版，基于文档 §2.1.2 修正库名）

### 5.1 接线与识别
1. USB 线连设备 **DIAG 口** → 电脑。
2. 电脑装好 **Quectel USB 驱动**后，设备管理器出现 **`Quectel USB DIAG Port`**。
   - 若没出现：先解决驱动（找移远要驱动包 + 安装文档）；再按 §3.2 确认设备侧 DIAG 口已暴露、枚举 `CONFIGURED`。

### 5.2 CATStudio 配置与录制
3. 打开 CATStudio，选 **"在线解析日志（Generic Target Online）"**。
4. 右下角 `Device Communication` → `Device 0` → `Settings`。
5. 选 **`Quectel USB DIAG Port`**，波特率 **115200**，点 **OK**。
6. `Logger` 选项卡 → **`CpLogStart`**。
7. `Database` → `Update`，按下表选库，然后 **`UpdateAll`**：
   | CATStudio 栏位 | 选哪个文件 |
   |---|---|
   | **Communication** | **`MDB.txt`** ← 注意不是 LWG_MDB.txt |
   | **Application** | **`Boerne_DIAG.mdb.txt`** |
8. 数据库匹配成功后，Logger 界面 **亮起两个绿灯**，开始录制。

### 5.3 复现与导出
9. **复现问题**（等目标故障场景发生，如掉网/CP 异常）。
10. `Modules` → `LogViewer` 查看日志。
11. `Log` → **`Export Log-File...`** 导出 **`.sdl`** 文件，发回给移远分析。

---

## 6. ⚠️ 现网 R02A04 的关键坑：本地无匹配数据库

**解析库必须与设备固件版本严格匹配，版本不对会解析乱码/匹配不上。**

现网固件是 **R02A04**，但 2026-07-24 全盘核查结果：

| 版本 | 本地是否有 diag 库 | 说明 |
|---|---|---|
| EC200A **R02A04**（现网）| ❌ **没有** | SDK 源码 `ql-ol-extsdk-ec200acntar02a04m2g_ocpu` 不含 diag 库；烧录包 `EC200ACNTAR02A04M2G_OCPU.zip` 只有 binary（ARBEL/MSA/zImage/root.squashfs 等），也不含 `dbg/` |
| EC200A R03A02 | ✅ 有 | 版本与现网不符，**不能直接用来解析 R02A04 的 log** |
| AG35 R01A04 | ✅ 有 | 机型都不对 |

> **结论**：diag 解析库（`MDB.txt`/`Boerne_DIAG.mdb.txt`）随 **`ASR_CP_DUMP_FIRMWARE` 调试包**单独发布，
> SDK 与烧录 zip 里都没有；本地这个调试包只有 R03A02。

### 两条解决路径（任选）
1. **优先·找移远要 R02A04 调试库**
   跟移远说明：*"现网固件 EC200ACNTAR02A04M2G_OCPU，请提供该版本的 CP log 解析数据库（`MDB.txt` + `Boerne_DIAG.mdb.txt`，即 `ASR_CP_DUMP_FIRMWARE` 包的 `dbg/` 目录）。"*
2. **备选·升级到 R03A02**
   本地已有 R03A02 的烧录固件 + 调试库。若车队可刷，升到 R03A02 后本地这套 `MDB.txt`/`Boerne_DIAG.mdb.txt` 即可直接用，无需再等移远。
   ——是否升级属产品决策，需评估现网影响。

---

## 7. 抓取前先核对固件版本（务必做）

抓 log 前，确认现网设备实际固件号与"R02A04"及所用数据库一致：

```bash
serial_atcmd AT+QGMR      # 查询固件版本（返回类似 EC200ACNTAR02A04M2G_OCPU...）
# 或
serial_atcmd ATI
```
- 返回版本号与准备好的数据库版本**必须一致**。
- 版本不一致 → 换对应版本的库，否则解析结果不可信。

（异常排查前也可先看基础状态，文档 §1 建议：`AT+CFUN?`、`AT+CPIN?`。）

---

## 8. 录多久 / 占多大：CP log 数据量级（务必预估）

CP log **数据量非常大**，录之前要规划好时长和落盘位置。移远参考数据（QT-AN-01-007 §7，
"qlsdk 原始版本、未跑复杂业务"的量级，**实际以现场实测为准**）：

| CP log 等级 | 量级 |
|---|---|
| **Error（精简）** | **15 MB / 10 分钟** |
| **Debug（全量）** | **85 MB / 10 分钟** ≈ 0.5 GB/时 ≈ 12 GB/天 |

> 提醒：
> - ASR 底层限制，modem log **只有 Error 和 Debug 两档**（无 warning/info 之说）。
> - 别为等一个偶发故障长时间开 Debug 全量录——会迅速吃满盘。**尽量在"快复现"时段开录**，或先 Error 精简、
>   确认能复现再切 Debug。
> - CATStudio 录制落在**上位机 Windows 磁盘**，确保剩余空间够（按上表估算 × 计划时长）。

---

## 9. 补充路径

### 9.1 射频 Sulog（排"极端掉网/驻网"问题时移远可能要）
open_dial 治的就是掉网/掉注册，移远排 **RF 层**问题时，除 CP log 外**很可能还要 Sulog**（射频相关日志）。
Sulog 也走 CATStudio + USB，但需**额外的 ASR Sulog 驱动**（`sulogusb.inf` → 设备管理器出现 `ASR Sulog Device`）。
步骤见 `quectel_log_guide_analysis.md` §2.1.3，要点：
- CATStudio 切 `Sulog` 标签页，`Record By` = USB Streaming，`UE Communications` = HWSulog；
- `Device Communication` 选 `Quectel USB DIAG Port`，`Start`/`Stop`（Sulog 有概率卡住，等 CATStudio 恢复再点 Stop）。

> 若移远开口要"射频 log / Sulog"，别只给 CP log。**驱动要一并找移远要（ASR Sulog 驱动）。**

### 9.2 车机/现场够不到设备时：diagsulogger（设备侧抓）
现场整机在车上、拿不到 Windows 电脑做 USB 直连时，可用移远 `diagsulogger` 工具在**设备侧**抓 Diag log，
再把 `.sdl` 拷回给 CATStudio 解析（见 `quectel_log_guide_analysis.md` §3）：
```bash
./diagsulogger -t 0          # USB Diag，抓 .sdl
./diagsulogger -t 3          # Sulog，抓 .bin
# 或 TCP 模式（default_media=3 + diag_ip 改模块 IP，见 §3.1.3）
```
> `diagsulogger` 需**找移远获取源码编译**；本设备是否已集成未验证，需确认。

---

## 10. 常见问题（FAQ）

| 现象 | 原因 / 处理 |
|---|---|
| 设备管理器没有 `Quectel USB DIAG Port` | ① Quectel USB 驱动未装好；② 接的是供电口不是 DIAG 口；③ 设备侧组合口被改（§3.2 恢复 `marvell_diag`）；④ 枚举失败（`state` ≠ CONFIGURED）|
| CATStudio 里找不到 `LWG_MDB.txt` | EC200A 的 Communication 库叫 **`MDB.txt`**，不是 LWG_MDB.txt（§4）|
| `UpdateAll` 后 Logger 不亮两绿灯 | 数据库版本与固件不匹配（现网 R02A04 用了 R03A02 的库）；或库文件选错（§6）|
| 导出 .sdl 打开是乱码/字段对不上 | 同上，库与固件版本不匹配 |
| 抓不到任何 CP log | 确认 `AT+QCFG="cplog"` =1（默认开）；确认 `default_media=1`（设备实测已=1，§3.1）|
| 想用 `qllog -s modem` 抓 | **本机型无 qllog**（ASR1803 平台代差，§1），走不通 |
| 磁盘很快满 | CP log 量大（Debug 85MB/10min，§8），缩短录制时长或改 Error 档 |

---

## 11. 待移远确认清单（发给移远时可直接照抄）

1. 请提供 **EC200A 的 Quectel USB 驱动**安装包 + 安装文档。
2. 请协助安装 **CATStudio**。
3. 请提供**现网固件 `EC200ACNTAR02A04M2G_OCPU`（R02A04）对应的 CP log 解析数据库**：
   `MDB.txt`（Communication）+ `Boerne_DIAG.mdb.txt`（Application），即该版本 `ASR_CP_DUMP_FIRMWARE` 包的 `dbg/` 目录。
4. 确认 EC200A 抓 CP log 走 **CATStudio + `Quectel USB DIAG Port`（115200）** 这一套流程无误（AG35 文档流程外推，需机型确认）。
5. 若还需**射频 Sulog**，请一并提供 **ASR Sulog 驱动**（`sulogusb.inf`）及对应抓取要点。

---

## 附：本文事实来源标注

| 结论 | 依据 |
|---|---|
| CP log 走 CATStudio + USB DIAG 口 115200、操作步骤、需 USB 驱动、Sulog 步骤 | AG35 文档 §2.1.2/§2.1.3（`quectel_log_guide_analysis.md`）|
| EC200A Communication 库为 `MDB.txt`（非 `LWG_MDB.txt`），Application 库同名 | 2026-07-24 本地 R03A02 调试包 `dbg/` 实测 |
| 默认 USB 组合口含 `marvell_diag`（DIAG 口默认暴露）+ 自查/恢复命令 | USB 配置指导 §2.2/§4.2（`EC200A_QuecOpen_USB_配置指导.md`）|
| 设备 `default_media=1`(usb)、`cplog` 默认开 | 设备实测（2026-07-23）+ 文档 §4.2.4 / QT-AN-01-007 §2.5 |
| qllog `-s modem` = CatStudio CP log，但本机型无 qllog（ASR1803 代差） | QT-AN-01-007 §1 原文 + §8 设备/SDK 实测 |
| CP log 量级 15MB/10min(Error)、85MB/10min(Debug)、只有 Error/Debug 两档 | QT-AN-01-007 §7 / §2.5 |
| 本地无 R02A04 diag 库；SDK/烧录 zip 均不含库 | 2026-07-24 本地全盘核查 |
| "EC200A 完全照 AG35 流程" | 机型合理外推，**未经 EC200A 官方文档背书 / 未在设备验证**，需向移远确认 |
