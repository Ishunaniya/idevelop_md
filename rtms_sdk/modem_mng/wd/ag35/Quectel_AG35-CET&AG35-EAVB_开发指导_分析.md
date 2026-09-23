# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) EAVB 开发指导 — 全量分析

> 本文是对原始 PDF《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_EAVB_开发指导_V1.0.0_Preliminary_20250509.pdf》的逐章全量精读还原。
> 讲的是**以太网音视频桥接（EAVB / TSN）中的 gPTP 时间同步**：用开源 Linux PTP（ptp4l/phc2sys）或 SDK 的 `ql_eavb.h` API 实现主从时钟同步。**车载以太网 TSN 场景专用，与蜂窝拨号无关。**

---

## 0. 文档元信息

| 项 | 内容 |
|---|---|
| 文档名 | AG35-CET&AG35-EUT QuecOpen(SDK) EAVB 开发指导 |
| 适用模块系列 | LTE Standard：AG35-CET、AG35-EUT |
| 版本 | 1.0.0（**临时版本 Preliminary**，参数可能未完全验证） |
| 日期 | 2025-05-09 |
| 总页数 | 21 页（正文 7~21） |
| 作者 | Gabriel LI |

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| -/1.0.0 | 2025-05-09 | Gabriel LI | 文档创建 / 临时版本 |

---

## 1. 整书目录树

```
1  引言（gPTP / IEEE 802.1AS / E2E vs P2P）
2  环境要求（2 模块 + 2 PHY(YT8521) + 2 EVB）
3  Linux PTP 时间同步
   3.1  Linux PTP 简介（ptp4l / phc2sys / S0~S3 状态）
   3.2  设置 gPTP 主时钟（仅测试）
   3.3  设置 gPTP 从时钟
   3.4  确认时钟同步（ptp4l 日志 rms / master offset / s3）
4  EAVB API
   4.1  头文件 ql_eavb.h
   4.2  函数概览（6 个函数）
   4.3  函数详解（init/deinit/gptp_conf_set/get/set_status/get_status）
   4.4  API 调用流程（ql_sdk_api_test 选 15）
5  附录
```

> 含 tcpdump 抓包图与 ptp4l 日志样例，已收录。

---

## 2. 逐章全量内容

### 第 1 章 引言

- **gPTP（IEEE 802.1AS）** 是 IEEE1588 的延伸，为 **TSN（时间敏感网络）** 提供全局精确时钟，实现流同步。
- IEEE1588 两种路径延时测量（不可在同一网络共存）：
  - **E2E（端到端）**：master↔slave 端到端延时，中间可有普通 switch（透传，驻留时间随机影响精度），master 压力大、扩展性差。
  - **P2P（点对点）**：测相邻节点延时，要求所有节点都支持 P2P，利于扩展；master 变更只需关心相邻节点。E2E 用 Sync/Follow_Up/Delay_Req/Delay_Resp 把校时与路径测量结合，P2P 则分开。
- **关键限制（备注）**：**AG35-CET/EUT 仅支持作为 gPTP 从机（Slave），不支持作主机；可作简单主机仅用于调试。**

### 第 2 章 环境要求

- 2 片模块（AG35-CET）+ 2 个 PHY 芯片（YT8521 为例）+ 2 个 EVB；内核版本以 SDK 为准。

### 第 3 章 Linux PTP 时间同步（开源软件方式，推荐）

#### 3.1 简介

- **`ptp4l`**：用户空间守护进程，做 PTP 设备通信/时钟同步。**`phc2sys`**：同步 PTP 时钟域与系统时钟。
- 以太网负责硬件记录 SYNC/PDELAY_REQ/PDELAY_RESP 报文时间戳（驱动 `marvell/linux/drivers/net/ethernet/asr/emac_ptp.c`），交给 PTP 程序做偏差/频率同步。
- **同步状态机**：`S0 UNLOCKED`（初始）→ `S1 JUMP`（首获偏差，等调整）→ `S2 LOCKED`（频率同步）→ `S3 LOCKED_STABLE`（连续多次偏差小于阈值）。

#### 3.2 主时钟（EVB-A，仅测试）

```sh
brctl delif bridge0 eth0
ifconfig eth0 192.168.1.100 up
ptp4l -i eth0 -H -m -f automotive-master.cfg          # -i 接口 / -f 配置(向技术支持取) / -m 日志输出
phc2sys -s CLOCK_REALTIME -c eth0 -w --transportSpecific=1 --step_threshold=1 -O 0
```

#### 3.3 从时钟（EVB-B）

```sh
echo 0x0009-0x1A00 > /sys/kernel/debug/eth/phy_reg_dump   # YT8521 参考，其他型号查数据手册
echo 0x0000-0x9140 > /sys/kernel/debug/eth/phy_reg_dump
brctl delif bridge0 eth0
ifconfig eth0 192.168.1.111 up                            # 之后须验证两模块能互 ping
ptp4l -i eth0 -H -m -f automotive-slave.cfg
phc2sys -s eth0 -w --transportSpecific=1 --step_threshold=1 -O 0
```

#### 3.4 确认同步

- 看 ptp4l 上报 `rms`（PHC 与主时钟偏移均方根）、`master offset`、`s3`、`path delay`。
- **持续上报 `s3` 且偏移低于 S3 阈值 = 主从锁定完成，PHC 已同步。**

### 第 4 章 EAVB API（不推荐路径，无法调具体参数）

- 头文件 `ql_eavb.h`（`ql-sysroots/usr/include/ql-sdk/`），结构体 `ql_eavb_common.h`，错误码 `ql_type.h`。

| 函数 | 说明 |
|---|---|
| `ql_eavb_init()` / `ql_eavb_deinit()` | 创建/取消 EAVB 设置通道 |
| `ql_eavb_gptp_conf_set()` / `_get()` | 设/取 gPTP 协议配置 |
| `ql_eavb_set_gptp_service_status()` / `_get_...()` | 开关/查 gPTP 应用 |

```c
typedef struct {
    char ifname[20];          // gPTP 网口名
    QL_EAVB_GPTP_E mode;      // SLAVE / MASTER(仅测试)
    int automotive;           // BMCA 算法：0 开启 / 1 关闭
} ql_eavb_gptp_t;

typedef enum { QL_EAVB_GPTP_SLAVE, QL_EAVB_GPTP_MASTER } QL_EAVB_GPTP_E;
typedef enum { QL_EAVB_STATUS_OFF, QL_EAVB_STATUS_ON } QL_EAVB_STATUS_E;
```

- 常见返回码：`QL_ERR_OK` / `QL_ERR_INVALID_ARG` / `QL_ERR_NOT_INIT` / `QL_ERR_SERVICE_NOT_READY`(需重试) / `QL_ERR_UNKNOWN`。
- **备注（重要）**：① **建议用开源软件方式跑 gPTP**；用 API 则**无法调具体参数**。② 这些函数**不支持并发调用，且不能在回调里调用**。

#### 4.4 API 调用流程

- `ql_sdk_api_test` → 选 `15`（eavb）进入子菜单（0 init / 1 conf_set / 2 conf_get / 3 set_status / 4 get_status / 5 deinit）。
- 主机端依次选 `0,1,1,1,2`；从机端依次选 `0,1,0,1,2`。
- **确认同步**：从机执行 `logcat -s ql_eavbd`，打印同步成功日志即可。

> 旁注：`ql_sdk_api_test` 菜单完整列出了 SDK 各域测试项（0 data call / 1 sms / 2 nw / 5 sim / 6 atc / 10 gnss / 14 log / 15 eavb …），可见 EC200A/AG35 SDK 的功能域划分。

---

## 3. 关键警告与坑（手册"备注"汇总）

1. **AG35 只能做 gPTP 从机**，主机仅调试用——量产组网必须有真正的主时钟源。
2. **推荐开源 ptp4l/phc2sys，而非 SDK API**——API 无法调参数，灵活性差。
3. **EAVB API 不支持并发、不能在回调里调**——与 GNSS/ATC 同类并发纪律。
4. **从机需先配 PHY 寄存器**（YT8521 特定值），其他 PHY 须查手册，否则同步不起来。
5. **配置前须 `brctl delif bridge0 eth0` 把网口移出网桥** + 两端能互 ping，否则 PTP 报文不通。
6. **临时版本，参数可能未验证**。

---

## 4. 对 open_dial 项目的适用性批注

> EAVB/gPTP 是**车载以太网 TSN 时间同步**功能，跑在以太网口上，与 open_dial 负责的**蜂窝 4G 拨号**完全是两条独立链路。本文档面向 AG35，EC200A 是否带 EAVB 需以其 SDK 为准。

| 文档内容 | 与 open_dial 的关系 / 可借鉴点 |
|---|---|
| **gPTP 时间同步 / PTP 时钟** | open_dial 关心的是**系统墙钟是否同步**（CLAUDE.md：时钟未同步=1970 时日志进 `unsynced/` 按数量保留）。gPTP 提供的是**网络硬件时钟（PHC）高精度同步**，与 open_dial 靠蜂窝网络/NTP 获取的系统时间是不同来源——但若同机部署 gPTP 且 `phc2sys -s CLOCK_REALTIME` 把 PHC 同步进系统时钟，理论上能更早让系统时间脱离 1970，**间接影响 open_dial 的 `unsynced/` 判定**。属"可能相关"的边缘联系，需实测验证同机是否真有此链路。 |
| **API 不支持并发/回调内调用** | 再次印证 QuecOpen SDK 的通用并发纪律（与 GNSS/ATC/EAVB 一致），与 open_dial v1.28.2 回调并发 use-after-free 教训同源。 |
| **`ql_sdk_api_test` 菜单（含 0 data call / 5 sim / 6 atc）** | 旁证 SDK 域划分，与 open_dial 的分层（data_call/sim/at/nw）对应，可作为查 EC200A SDK 对应测试入口的参考。 |
| **`logcat -s ql_eavbd` 看服务日志** | 与 Log抓取文档的 `logcat -s ql_audio` 一致——QuecOpen 各服务都能用 `logcat -s <tag>` 单独看日志，open_dial 排查时可用 `logcat -s` 看 ql_rild/ql_netd 等拨号相关服务日志。 |

**结论**：本文档对 open_dial **相关度低**（不同链路、不同功能域）。唯一稍有意义的边缘联系是 gPTP→PHC→系统时钟同步可能间接影响 open_dial 的"时钟未同步"日志分支，但需同机确有 EAVB 部署才成立。另可借鉴 `logcat -s <服务tag>` 这一通用日志查看法用于拨号服务排障。
