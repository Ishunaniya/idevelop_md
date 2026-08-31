# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) Linux 系统时间同步 — 全文分析

> **文档全名**：AG35-CET&AG35-EUT QuecOpen(SDK) Linux 系统时间同步
> **适用模块**：移远通信 AG35-CET、AG35-EUT（LTE Standard 模块系列）
> **方案**：QuecOpen®（基于 Linux 的嵌入式开发平台，SDK 构建环境）
> **版本**：V1.0.1（状态：临时文件 / Preliminary）
> **日期**：2024-04-08（封面）/ 修订记录中 1.0.1 为 2025-04-08
> **总页数**：14 页（PDF 物理页），正文页脚编号 13 页
> **本分析覆盖页范围**：全部 14 页（封面、法律声明、文档历史、目录、表格索引、第 1~5 章及附录）
>
> 说明：第 1~5 页为封面、联系方式/前言/披露限制/版权/免责声明、文档历史与修订记录、目录、表格索引，属固定模板内容；真正的技术内容集中在第 6~13 页（正文 §1~§5）。下面对全部内容逐章逐节展开，技术细节（配置项、命令、数据结构、时序框图、术语）原样保留并补充解读。

---

## 0. 封面与前置页（PDF 第 1~5 页）

### 0.1 封面（第 1 页）
- 标题：**AG35-CET&AG35-EUT QuecOpen(SDK) Linux 系统时间同步**
- 子标题：LTE Standard 模块系列
- 版本：1.0.1；日期：2024-04-08；状态：临时文件（Preliminary）。

### 0.2 法律/前言/披露限制（第 2~3 页）
标准模板：移远通信（上海移远通信技术股份有限公司）联系方式、前言（参考设计仅为示例、"可用"基础提供）、使用和披露限制（许可协议、版权声明、商标、第三方权利）、隐私声明、免责声明（4 条），版权所有 © 2025。
> 解读：状态标注为"临时文件 / Preliminary Confidential"，水印贯穿全篇，意味着参数（尤其各配置项默认值、最小值）后续正式版可能调整，集成时以最新版为准。

### 0.3 文档历史 / 修订记录（第 4 页 / 页脚 3）

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2024-02-01 | Yumn HUANG | 文档创建 |
| 1.0.0 | 2024-02-01 | Yumn HUANG | 临时版本 |
| 1.0.1 | 2025-04-08 | Allen FENG | 临时版本；**新增配置项 `ntp_resync_interval_s` 的最小值（第 3.2 章）** |

> 解读：1.0.0 → 1.0.1 的唯一实质变更，就是给 `ntp_resync_interval_s` 加了一个"最小值 3600 秒"的硬下限（详见 §3.2 该配置项分析）。这是本版本最值得关注的工程约束变化。

### 0.4 目录（第 5 页 / 页脚 4）
正文结构：
- 1 引言（p6）
- 2 时间源（p7）
- 3 时间同步机制（p8）
  - 3.1 时间同步机制框架（p8）
  - 3.2 自动同步系统时间（p9）
  - 3.3 手动同步系统时间（p11）
  - 3.4 获取时间同步信息（p11）
- 4 设置系统时区（p12）
- 5 附录 参考文档及术语缩写（p13）

### 0.5 表格索引（第 6 页 / 页脚 5）
- 表 1：模块支持的时间源类型（p7）
- 表 2：时间同步配置文件包含的配置项（p9）
- 表 3：参考文档（p13）
- 表 4：术语和缩写（p13）

---

## 1. 引言（第 7 页 / 正文 §1）

- AG35-CET 和 AG35-EUT 模块支持 **QuecOpen®** 方案；QuecOpen® 是基于 **Linux** 的嵌入式开发平台，用于简化 **IoV（车联网）** 应用的软件设计与开发流程。QuecOpen 详细信息见参考文档 [1]（《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导》）。
- 本文档适用于 **SDK 构建环境** 的 QuecOpen® 方案，主题三块：
  1. 模块的**时间源**；
  2. **时间同步机制**；
  3. **系统时区设置**。

> 解读：本文聚焦 AG35 的 OpenLinux（QuecOpen SDK）侧的系统时间维护，而非 AT 指令侧。核心是一个常驻后台守护进程 `ql_time_daemon` + 配置文件 `/etc/ql_time_conf.json` + 几个 `/tmp` 下的标志/管道文件。与本仓库 RK3576/io_mng 的"系统时间该放哪"讨论相关：AG35 自带 NTP/NITZ/GNSS 多源 + chronyd 的成熟方案，可作为参考实现范式。

---

## 2. 时间源（第 8 页 / 正文 §2 + 表 1）

AG35-CET / AG35-EUT 支持**自动同步系统时间**，可用时间源共 4 类：**NITZ、GNSS、NTP、USER（用户自定义）**。

### 表 1：模块支持的时间源类型（逐项展开）

| 时间源 | 含义 | 前提条件 | 关键细节 / 限制 |
|---|---|---|---|
| **NITZ** | Network Identity and Time Zone（网络标识和时区）：通过无线网络向移动设备提供本地日期/时间、时区、夏时制偏移以及网络商身份信息的机制 | 运营商基站支持 NITZ **且** 设备能正常注网 | NITZ 是运营商的**可选服务**，"目前尚不能确认所有运营商支持 NITZ 授时服务"——即不能假定一定可用 |
| **GNSS** | Global Navigation Satellite System：提供三维坐标、速度及时间信息的空基无线电导航定位系统；定位成功后可从 **GGA** 数据中获取 **UTC** 时间 | 模块调用 GNSS 相关 API 定位**并成功** | GNSS 默认只能同步到**时分秒**（不含年月日/时区），故 GNSS 授时**必须同时开启 `modem_sync_enable`**，通过 Modem 获取时区等信息（见 §3.2 `gnss_sync_enable` 注） |
| **NTP** | Network Time Protocol：互联网上计算机时钟同步的网络协议，使设备与 NTP 服务器同步时间 | 设备**可以访问互联网** | NTP 同步会访问互联网，**产生少量流量**（计费/省流场景需留意） |
| **USER** | 用户自定义时间 | 由用户应用主动写入 | 通过 `echo "user: <时间戳>" > /tmp/ql_time_set_pipe`，经管道传递给 `ql_time_daemon` 进程进行时间同步；详见 §3.3 |

### 硬件 RTC 说明（§2 末段）
- 模块含一个**硬件 RTC**，设备文件为 **`/dev/rtc1`**。
- **系统断电**：RTC 将**自动复位**（即掉电不保持，无后备电池/或电池失效语义）。
- **系统软重启或硬重启**：RTC **可继续工作**（保持当前时间）。

> 解读：
> - `/dev/rtc1`（不是 rtc0）是 AG35 上由 `ql_time_daemon` 维护的硬件 RTC。
> - "断电复位、重启保持"意味着：跨重启可短期依赖 RTC，但**跨断电不可依赖**——这正是需要 NTP/NITZ/GNSS 多源回填的根因。与本仓库 RK3576 笔记（无电池 RTC、靠 NTP 维持）的工程取舍同构。
> - 4 类源的可靠性梯度：GNSS（户外/有定位）> NTP（有公网）> NITZ（运营商支持且注网）> USER（应用自管）。下文 §3.1 给出它们的优先级裁决规则。

---

## 3. 时间同步机制（第 9~11 页 / 正文 §3）

### 3.1 时间同步机制框架（第 9 页 / §3.1 + 图 1）

**图 1：时间同步机制框架**（框图，数据流自下而上汇聚到 `ql_time_daemon`，再分发到 System Time / Chronyd）。逐节点与连线还原如下：

```
   ┌──────────────┐   Sync Time   ┌──────────┐
   │              │ ◄──────────── │ Chronyd  │
   │              │               └────▲─────┘
   │              │                    │ Boot
   │ System Time  │   Sync Time   ┌────┴─────────┐  H/m/s   ┌─────────────┐
   │              │ ◄──────────── │ ql_time_daemon│ ◄────── │ Ql_location │
   │              │               └─▲───▲────▲───┘          └──────▲──────┘
   └──────────────┘                 │   │    │                     │ GNSS Position
                              NTP Time│ RTC│  │NITZ Time            │
                                ┌─────┴┐ ┌┴──┐ ┌──────────────────────┐
                                │Network│ │RTC│ │       Modem          │
                                └───────┘ └───┘ └──────────────────────┘
```

数据流/职责梳理：
- **Network → ql_time_daemon**：提供 **NTP Time**。
- **RTC → ql_time_daemon**：提供 **RTC Time**（双向，daemon 也维护/回写 RTC）。
- **Modem → ql_time_daemon**：提供 **NITZ Time**。
- **Modem → Ql_location → ql_time_daemon**：GNSS 定位（**GNSS Position**）经 `Ql_location` 解析后，以 **H/m/s（时/分/秒）** 形式交给 daemon——印证 §2 中"GNSS 只给时分秒"的说法。
- **ql_time_daemon → System Time**：Sync Time（写系统时间）。
- **Chronyd → System Time**：Sync Time（chronyd 也参与维护系统时间）。
- **ql_time_daemon → Chronyd**：**Boot**（daemon 在启动时拉起/对接 chronyd，与 §3.2 `chronyd_sync_enable` 呼应）。

**优先级裁决规则（§3.1 正文）：**
- **GNSS 定位有效时，GNSS 时间源优先级最高**（不可被其他源覆盖）。
- **GNSS 定位无效时**：由后台进程 `ql_time_daemon` 使用 **NTP、NITZ、USER** 三类源同步系统时间；这三者的优先级顺序可通过配置文件 `/etc/ql_time_conf.json` 的配置项 **`time_source_priority`** 配置（详见 §3.2）。

> 解读：优先级模型是"两层"——GNSS 永远压顶（仅当定位有效），其余三源在 GNSS 缺位时按 `time_source_priority` 竞争。默认排序见下文 `time_source_priority`（NITZ < NTP < USER < GNSS）。

---

### 3.2 自动同步系统时间（第 9~10 页 / §3.2 + 表 2）

**守护进程与标志文件：**
- 后台进程 **`ql_time_daemon`** 自动同步系统时间，并维护模块的**硬件 RTC**。
- 标志文件 **`/tmp/ql_time_set_flag`**：
  - **存在** → 系统已完成时间同步；
  - **不存在** → 还未进行时间同步。
  - 该文件内容还可用于查看**当前系统时间的时间源**以及**时间同步的初始时间**（即第一次同步成功的时间与来源）。
- 时间同步配置文件：**`/etc/ql_time_conf.json`**（JSON 格式），包含表 2 所列配置项。

> 工程提示：判断"系统时间是否已同步"应检查 `/tmp/ql_time_set_flag` 是否存在，而不是直接比对系统时间——这是 AG35 上对外暴露的同步状态标志，比本仓库自定义的 `/tmp/dial_*` 标志更标准。

#### 表 2：时间同步配置文件 `/etc/ql_time_conf.json` 配置项（逐项详解）

> 可选性栏：**必选**=配置文件中必须出现；**可选**=可省略。下面每项给出含义、默认值、取值范围、注意事项。

##### (1) `sync_accuracy_ms` —— 必选
- **含义**：仅当"通过时间源获取到的时间"与"系统当前时间"之**差大于该值**时，才会自动同步系统时间，以避免频繁同步。
- **默认值**：`2000`，单位**毫秒**。
- **特例**：若设为 `0`，表示**一旦从时间源获取到时间就立即自动同步**系统时间（不设阈值）。
- 解读：这是一个"死区/迟滞"阈值，默认 2 秒。生产中若对时间精度要求高且不在意频繁 settimeofday，可调小或置 0；若要稳态、避免时间被反复微调，保持默认或调大。

##### (2) `modem_sync_enable` —— 必选
- **含义**：是否使能通过 **NITZ 时间源**同步系统时间。
- **取值**：`1`（默认）使能；`0` 禁止。
- **注**：模块**正常注网**才能从 NITZ 获取准确时间。
- 解读：注意这个开关名叫 "modem_sync" 但管的是 NITZ。它同时也是 GNSS 授时的依赖项（GNSS 只给时分秒，年月日/时区要靠 Modem/NITZ 补全，见下条）。

##### (3) `gnss_sync_enable` —— 必选
- **含义**：是否使能从**定位信息**中获取 **UTC 时间**并同步系统时间。
- **取值**：`1`（默认）使能；`0` 禁止。
- **关键注**：
  - GNSS **成功定位后**才能从定位信息获取 UTC 时间并同步。
  - GNSS **默认只能同步到时分秒**，故要让 GNSS 授时生效，**必须同时开启 `modem_sync_enable`**，通过 Modem 获取**时区等信息**（补全年月日/时区）。
- 解读：GNSS 与 NITZ 是"耦合"的——单独开 GNSS 拿不到完整日期。要禁用 GNSS 授时，置本项为 `0`（见 `time_source_priority` 注，因 GNSS 优先级不可改，禁用 GNSS 的唯一途径就是此开关）。

##### (4) `ntp_sync_enable` —— 必选
- **含义**：是否使能通过 **NTP 时间源**同步系统时间。
- **取值**：`1`（默认）使能；`0` 禁止。
- **注**：模块**正常访问 NTP 服务器**才能从 NTP 获取准确时间。

##### (5) `time_source_priority` —— 必选
- **含义**：不同时间源的**优先级顺序（由低到高排序）**。
- **默认排序**：`NITZ < NTP < USER < GNSS`（NITZ 最低，GNSS 最高）。
- **注**：**GNSS 时间源优先级最高，不可修改**。如需禁止通过 GNSS 同步系统时间，应设置 `gnss_sync_enable` 为 `0`（而不是改优先级把它排低）。
- 解读：可调的只是 NITZ/NTP/USER 三者的相对次序；GNSS 钉死在最高。USER（应用注入）默认高于 NTP/NITZ，意味着应用一旦写入即覆盖网络源——设计上把"应用显式授时"看作比网络源更可信。

##### (6) `ntp_sync_policy` —— 必选
- **含义**：NTP 时间源的两种同步模式：**Assist（辅助模式）** 与 **Normal（正常模式）**。
- **Assist 辅助模式**：在可访问互联网的前提下，**若当前系统时间仍未同步**，则自动启动 NTP 进行时间同步；**获取 NTP 时间后，停止 NTP 服务**。（即只用 NTP 兜底"冷启动还没同步"的场景，拿到一次就收手。）
- **Normal 正常模式**：在可访问互联网的前提下，**模块开机后自动启动 NTP 进行时间同步**；同步成功后，模块根据 `ntp_resync_interval_s` 的配置，**退出 NTP 或按配置的时间间隔再次进行时间同步**（即周期性持续校时）。
- 解读：Assist = 一次性兜底（最省流量，依赖其他源持续维护）；Normal = 周期性 NTP 校时（更精准但有持续流量）。`ntp_resync_interval_s` 仅在 Normal 模式下生效（见下）。

##### (7) `ntp_server_probe_interval_ms` —— 必选
- **含义**：向 NTP 服务器请求时间信息**失败后的重试间隔**。
- **默认值**：`2000`，单位**毫秒**。
- **取值约束**：必须为 **≥ 1 的整数**。

##### (8) `ntp_server_probe_retry_count` —— 必选
- **含义**：向 NTP 服务器请求时间信息**失败后的重试次数**。
- **取值约束**：必须为 **≥ 1 的整数**。**默认值 `2`**。
- 解读：(7)(8) 共同定义单台 NTP 服务器的"探测重试"行为：失败后每隔 `ntp_server_probe_interval_ms` 重试，最多 `ntp_server_probe_retry_count` 次。

##### (9) `ntp_failed_retry_interval_s` —— 必选
- **含义**：向**所有** NTP 服务器请求时间信息**均失败后**，重新发起时间同步请求的时间间隔。
- **默认值**：`300`，单位**秒**。
- **特例**：若设为 `0`，表示**不进行重试**。
- 解读：层级关系——(7)(8) 是"单台服务器的内层重试"，(9) 是"整轮（所有服务器）都失败后的外层退避"，默认 5 分钟后再整轮重来。

##### (10) `ntp_resync_interval_s` —— 必选 ★（V1.0.1 新增最小值约束）
- **含义**：NTP 时间同步**成功之后**，下一次进行同步的时间间隔。
- **默认值**：`86400`，单位**秒**（= 1 天）。
- **最小值**：`3600`（秒）。**若设置值小于该最小值，则会被强制重置为 3600**。 ← 这是 V1.0.1 相对 V1.0.0 的唯一实质新增。
- **生效条件**：**仅在 Normal 模式下有效**。
- **特例**：若设为 `0`，表示**时间同步成功之后退出 NTP**（即只同步一次后不再周期校时）。
- **建议**：建议设置为**较大值**，例如 `86400` 秒（1 天）。
- 解读：这是 NTP 周期校时的节流参数。新增"最小 3600 秒"硬下限是为了**防止过于频繁的 NTP 请求**（省流量、避免对公共 NTP 服务器造成压力）。集成时若误填了小值（如 60），daemon 会静默抬到 3600——排障时要意识到"配置值与实际生效值可能不一致"。

##### (11) `timing_synchronization_enable` —— 必选
- **含义**：使能从 NITZ 时间源同步系统时间（即 `modem_sync_enable=1`）时，是否**循环**从 Modem 获取时间并同步 Linux 系统时间。
- **取值**：`1`（默认）循环获取；`0` 不循环获取。
- **注**：循环获取 Modem 时间并同步，可**减小 Linux 系统在长时间运行后因设备特性导致的 Linux 系统时间与 Modem 时间的误差**（即抑制长期时钟漂移）。
- 解读：与 `modem_sync_enable` 配合——前者是"开不开 NITZ 源"，本项是"开了之后要不要周期性重取以纠漂"。

##### (12) `timing_interval_s` —— 必选
- **含义**：循环从 Modem 处获取时间并进行系统时间同步的**时间间隔**。
- **默认值**：`3600`，单位**秒**。
- **建议**：建议设置为**较大值**，例如 `3600` 秒。
- 解读：即 (11) 循环纠漂的周期，默认 1 小时一次。

##### (13) `chronyd_sync_enable` —— 必选
- **含义**：是否由本机制检查并主动开启 **chronyd** 服务。
- **取值**：`1` 系统检查并主动开启 chronyd 服务；`0`（**默认**）系统不检查、不控制 chronyd 服务。
- 解读：注意默认是 **0**（不接管 chronyd）。图 1 中 `ql_time_daemon → Chronyd (Boot)` 这条线只有在本项置 1 时才生效。chronyd 与 ql_time_daemon 双写系统时间存在潜在竞争，默认关闭是合理的保守选择。

##### (14) `sync_system_time_enable` —— 必选
- **含义**：是否使能**系统时间同步**（总开关）。
- **取值**：`1`（默认）使能；`0` 禁止。
- 解读：这是最顶层的总闸。置 0 则上述所有源都不会去写系统时间。

##### (15) `rtc_sync_enable` —— 必选
- **含义**：是否使能 **RTC 时间同步**（即把同步到的时间回写到硬件 RTC `/dev/rtc1`）。
- **取值**：`1`（默认）使能；`0` 禁止。
- 解读：关掉它，系统时间仍可被同步，但不会回写 RTC，跨软重启就拿不到上次的时间。

##### (16) `ntp_server_list` —— **可选**
- **含义**：NTP 服务器地址（列表）。
- **默认值**：`ntp.org` 系列的时间同步服务器。
- 解读：国内部署可改为国内 NTP（如 `ntp.aliyun.com`、`cn.pool.ntp.org`）以提高成功率/降低延迟。

##### (17) `sync_system_time_to_pipe` —— **可选**
- **含义**：定义 `ql_time_daemon` 中**时间同步相关数据的管道路径**；详见 §3.4。
- **行为**：`ql_time_daemon` 进程**获取到新时间时**，会向指定管道写入时间数据。
- **数据格式**：
  ```
  time_source=%s time_msec=%lld time_monotonic=%lld
  ```
  即三个字段：`time_source`（时间源名，字符串）、`time_msec`（毫秒时间戳，long long）、`time_monotonic`（当前模块开机时间/单调时钟，long long）。
- **默认值**：**空**（即默认不向任何管道写）。
- 解读：这是把"系统时间同步事件"通知给用户应用的标准出口——配一个 `/tmp/...` 管道路径，应用 `cat` 即可拿到每次同步的源、绝对时间(ms)与开机单调时钟。`time_monotonic` 让应用能区分"这次同步是开机后多久发生的"。

---

### 3.3 手动同步系统时间（第 11 页 / §3.3）

- 模块支持**手动同步**系统时间：将时间信息写入文件 **`/tmp/ql_time_set_pipe`** 即可更新系统时间。
- **写入格式**：`user: <时间戳>`（**时间戳单位：毫秒**）。
- **示例**：将系统时间设为 `2019-12-31 17:01:01`：
  ```sh
  echo "user: 1577811661000" > /tmp/ql_time_set_pipe
  ```
- **备注（限制）**：**可设置的最早系统时间为 `2019-12-31 17:01:01`，在此之前的时间视为无效时间。**

> 解读：
> - 这就是 §2 表 1 中 **USER 时间源**的具体注入方式，对应 `time_source_priority` 里默认排第二高的 USER。
> - 时间戳是**毫秒级 Unix 时间戳**（`1577811661000` = 1577811661 秒 × 1000）。注意 `1577811661` 对应 UTC `2019-12-31 09:01:01`，文档写 `17:01:01` 是按 UTC+8 本地时间表述——集成时要明确自己写入的是 UTC 毫秒戳。
> - **下限保护**：早于 2019-12-31 17:01:01 的值会被判无效，防止把系统时间设回 1970 之类的脏值。

---

### 3.4 获取时间同步信息（第 11 页 / §3.4）

- 模块支持把 §3.1 所述的时间同步机制**透传给用户应用程序**：配置 `/etc/ql_time_conf.json` 的 **`sync_system_time_to_pipe`** 配置项定义管道路径；配置完成后，`ql_time_daemon` 在获取到新时间时，会把**时间戳、时间源、当前模块开机时间**写入管道供用户应用读取。

**操作三步（步骤一/二/三）：**
1. **步骤一**：修改 `/etc/ql_time_conf.json`，配置 `sync_system_time_to_pipe: "/tmp/ql_time_pipe"`；
2. **步骤二**：重启模块；
3. **步骤三**：执行 `cat /tmp/ql_time_pipe` 即可读取时间。

- **备注（建议）**：**建议将管道路径定义到 `/tmp/` 目录下**（tmpfs，避免磨损 flash、重启自动清理）。

> 解读：与 §3.2 的 `sync_system_time_to_pipe`、数据格式 `time_source=%s time_msec=%lld time_monotonic=%lld` 闭环。注意需要**重启模块**让配置生效。这是 AG35 推荐的"应用感知时间同步事件"的官方方式——比应用自己轮询系统时间更可靠，能拿到"是哪个源、什么时候同步的"。

---

## 4. 设置系统时区（第 12 页 / 正文 §4）

- 模块通过修改 **`ql-ol-extsdk-xxx/ql-ol-rootfs/system/etc/config/system`**（OpenWrt 风格的 UCI 配置文件）进行系统时区设置。
- 示例：将时区设为 **`Asia/Shanghai`**，按四步操作。

**步骤一**：编辑 `ql-ol-extsdk-xxx/ql-ol-rootfs/system/etc/config/system`，内容如下：

```
config system
        option hostname 'OpenWrt'
        option timezone 'CST-7'
        option zonename 'Asia/Shanghai'
        option ttylogin '0'
        option log_size '64'
        option urandom_seed '0'

config timeserver 'ntp'
        option enabled '1'
        option enable_server '0'
        list server '0.openwrt.pool.ntp.org'
        list server '1.openwrt.pool.ntp.org'
        list server '2.openwrt.pool.ntp.org'
        list server '3.openwrt.pool.ntp.org'
```

- 若**没有**该文件，则需手动创建一个并添加上述格式内容。
- 需要修改时区时，**直接修改文件中的 `timezone` 字段和 `zonename` 字段即可**。

**步骤二**：在 SDK 中**编译 rootfs**。
**步骤三**：将编译好的 rootfs **烧录到模块**中。
**步骤四**：运行 **`date -R`** 命令查看当前系统时区是否已更新成功。

> 逐字段解读：
> - `config system` 段：
>   - `hostname 'OpenWrt'`：主机名。
>   - **`timezone 'CST-7'`**：POSIX TZ 字符串。注意 `CST-7` 中的 `-7` 是 POSIX 约定的"偏移取反"——实际表示 UTC+7？不，POSIX 中 `CST-8` 才是 UTC+8（东八区）。文档示例用 `CST-7` 配 `Asia/Shanghai` **存在不一致**（Asia/Shanghai 应为 `CST-8`）。集成时应以 `zonename` 对应的标准时区为准核对 `timezone` 偏移，避免照抄示例造成 1 小时偏差。
>   - **`zonename 'Asia/Shanghai'`**：IANA tz database 区域名。
>   - `ttylogin '0'`：是否要求 tty 登录（0 关）。
>   - `log_size '64'`：系统日志缓冲大小（KB）。
>   - `urandom_seed '0'`：urandom 种子开关。
> - `config timeserver 'ntp'` 段：OpenWrt 自带 sysntpd 的配置——`enabled '1'` 启用本地 NTP 客户端、`enable_server '0'` 不作为 NTP 服务器对外、`list server` 为上游 NTP 池。这与 §3.2 的 `ntp_server_list` / chronyd 是**两套并存的 NTP 机制**（OpenWrt sysntpd vs. ql_time_daemon/chronyd），集成时要厘清实际由谁授时，避免重复/冲突。
> - 时区设置走的是**编译期 rootfs**路线（改文件 → 编 rootfs → 烧录 → `date -R` 验证），不是运行期 `/tmp` 热改——因此时区是相对固定的出厂配置。

> ⚠️ 重点提醒（结合 §2/§3.2）：GNSS/NTP 拿到的是 **UTC**，NITZ 才带时区。系统本地时间 = UTC + 时区偏移，时区由本章 `timezone/zonename` 决定。若时区配错，即使时间源正确，`date` 显示的本地时间仍会偏。

---

## 5. 附录 参考文档及术语缩写（第 13 页 / 正文 §5）

### 表 3：参考文档
| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |

### 表 4：术语和缩写
| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| API | Application Programming Interface | 应用程序编程接口 |
| GNSS | Global Navigation Satellite System | 全球导航卫星系统 |
| IoV | Internet of Vehicles | 车联网 |
| NMEA | NMEA (National Marine Electronics Association) 0183 Interface Standard | NMEA（美国国家海洋电子协会）0183 接口标准 |
| NITZ | Network Identity and Time Zone | 网络标识和时区 |
| NTP | Network Time Protocol | 网络时间协议 |
| PPE | Precise Positioning Engine | 精确定位引擎 |
| RTC | Real-Time Clock | 实时时钟 |
| SDK | Software Development Kit | 软件开发工具包 |
| UTC | Universal Time Coordinated | 通用协调时 |

> 注：NMEA 0183 / GGA 语句是 §2 中 GNSS 授时取 UTC 的数据来源；PPE（精确定位引擎）虽列入术语但正文未直接展开，属 GNSS 相关背景。

---

## 6. 全文要点速查（工程落地总结）

1. **核心组件**：后台守护进程 `ql_time_daemon` + 配置 `/etc/ql_time_conf.json` + 硬件 RTC `/dev/rtc1` + 一组 `/tmp` 文件（标志/管道）。可选拉起 `chronyd`（默认不接管）。
2. **四类时间源**：GNSS（最高、不可改优先级、只给时分秒需配 Modem 补日期/时区）> 可配的 USER/NTP/NITZ（默认 `NITZ < NTP < USER`）。
3. **同步状态判定**：检查 `/tmp/ql_time_set_flag` 是否存在；该文件还记录当前源与首次同步时间。
4. **手动/USER 授时**：`echo "user: <毫秒时间戳>" > /tmp/ql_time_set_pipe`；下限 2019-12-31 17:01:01，更早视为无效。
5. **应用感知同步事件**：配 `sync_system_time_to_pipe` → 重启 → `cat 管道`；格式 `time_source=%s time_msec=%lld time_monotonic=%lld`。
6. **NTP 节流关键参数**：`ntp_sync_policy`（Assist 一次性 / Normal 周期）、`ntp_resync_interval_s`（默认 86400，**V1.0.1 起最小 3600，小于则强制重置**）、`ntp_failed_retry_interval_s`（默认 300，0 不重试）、单台探测 `ntp_server_probe_interval_ms`(2000)/`retry_count`(2)。
7. **防漂移**：`timing_synchronization_enable` + `timing_interval_s`（默认 3600s 循环取 Modem 时间纠漂）；`sync_accuracy_ms`（默认 2000ms 死区，0=不设阈值立即同步）。
8. **RTC 行为**：断电复位（不保持），软/硬重启保持；`rtc_sync_enable=1` 时同步结果回写 RTC。
9. **时区**：编译期改 `etc/config/system` 的 `timezone`/`zonename`，编 rootfs→烧录→`date -R` 验证。**注意文档示例 `CST-7` 与 `Asia/Shanghai`（应 `CST-8`）偏移不一致，需自行核对。**
10. **流量提示**：NTP 会走公网产生少量流量；省流场景用 Assist 模式或拉大 `ntp_resync_interval_s`，并优先依赖 GNSS/NITZ。

---

*（本分析已覆盖 PDF 全部 14 页：封面、法律/前言、文档历史与修订记录、目录、表格索引、第 1~5 章正文及附录表 1~表 4，无遗漏。）*

<!-- GENERATION_COMPLETE -->
