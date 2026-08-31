# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) GNSS 开发指导 — 全量分析

> 本文是对原始 PDF《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_GNSS_开发指导_V1.0.1_Preliminary_20240920.pdf》的逐章全量精读还原。
> 讲的是**用 SDK 的 `ql_gnss.h` C API 实现卫星定位**：初始化→注册回调→启用→设置星座/NMEA→AGNSS 辅助定位→热启动。**功能型独立模块，与拨号守护无直接耦合。**

---

## 0. 文档元信息

| 项 | 内容 |
|---|---|
| 文档名 | AG35-CET&AG35-EUT QuecOpen(SDK) GNSS 开发指导 |
| 适用模块系列 | LTE Standard：**AG35-CET、AG35-EUT** |
| 版本 | 1.0.1（**临时版本 Preliminary**） |
| 日期 | 2024-09-20 |
| 总页数 | 36 页（正文 7~35） |
| 作者 | Aurora JIANG / William LIU（V1.0.1 Sunshine HUANG） |

### 修订记录

| 版本 | 日期 | 变更表述 |
|---|---|---|
| -/1.0.0 | 2024-03-20 | 文档创建 / 临时版本 |
| 1.0.1 | 2024-09-20 | 更新星座支持；更新 `QL_GNSS_CONSTELLATION_MASK_E`；更新 start/stop/suspend/resume/inject_agnss_data 描述；新增 `ql_gnss_inject_agnss_data_full()`、`ql_gnss_inject_local_agnss_data()`；更新 AGNSS 简介；**新增常见问题（第 7 章）** |

---

## 1. 整书目录树

```
1  引言
2  GNSS 功能特性介绍（2.1 特性 / 2.2 性能指标）
3  调用流程（init→ind_cb→error_cb→start→设参→while→stop）
4  GNSS API
   4.1 头文件 ql_gnss.h
   4.2 函数概览（表 2，19 个函数）
   4.3 函数详解（init/deinit/回调/start/stop/start_mode/engine_state/
                 constellation/nmea_version/nmea_type/suspend/resume/inject_agnss×3）
   4.4 示例代码
5  AGNSS 辅助定位技术（5.1 简介 / 5.2 使用）
6  实现热启动（6.1 示例 / 6.2 休眠 32K 时钟配置）
7  常见问题（7.1 -1099 处理 / 7.2 休眠唤醒）
8  附录
```

> 含 2 段完整 C 示例（基础流程 + 热启动），均已收录关键片段。

---

## 2. 逐章全量内容

### 第 2 章 功能特性与性能

- **支持星座**：GPS、BDS、**Galileo（仅 AG35-EUT）**、GLONASS，多系统联合定位；支持 AGNSS。
- **性能指标（表 1）**：

| 参数 | 指标 |
|---|---|
| 冷启动 TTFF | ≤ 32 s |
| 热启动 TTFF | ≤ 1 s |
| 重捕获 TTFF | ≤ 1 s |
| 冷/热启动捕获灵敏度 | -148 / -156 dBm |
| 重捕获 / 跟踪灵敏度 | -160 / -162 dBm |
| 定位精度 | < 2.5 m (CEP50) |
| 测速精度 | < 0.1 m/s (1σ) |
| 定位更新率 | 1 Hz |

### 第 3 章 调用流程

`ql_gnss_init()` → `set_ind_cb()`（数据上报）→ `set_service_error_cb()`（异常）→ `ql_gnss_start()` → 按需设参 → `while(1)` 处理 → `ql_gnss_stop()`。

**关键备注（4 条）**：
1. **不可在回调里调用任何其他 GNSS API**。
2. **不可在回调里做阻塞任务**。
3. 要用 API 设参，须在 `/etc/ql_locationd.conf` 配 `dynamic_set=1`。
4. **GNSS API 不支持多线程调用**；支持多进程但可能让 `ql_locationd` 状态错乱，**不建议多进程**。

### 第 4 章 GNSS API（头文件 `ql_gnss.h`，`ql-sysroots/usr/include/ql-sdk/`）

| 函数 | 说明 |
|---|---|
| `ql_gnss_init()` / `ql_gnss_deinit()` | 初始化 / 去初始化（用前必 init，用后须 deinit 释放） |
| `ql_gnss_set_ind_cb()` | 注册数据上报回调 `void(*)(void *msg)` |
| `ql_gnss_set_service_error_cb()` | 注册服务异常回调 `void(*)(int error)` |
| `ql_gnss_start()` / `ql_gnss_stop()` | 上电启用 / 下电关闭 |
| `ql_gnss_set_start_mode()` | 设启动模式（冷/温/热） |
| `ql_gnss_get_engine_state()` | 获取引擎状态（ON=1/OFF=0） |
| `ql_gnss_set_constellation()` / `get` | 设/取参与定位的星座组合 |
| `ql_gnss_set_nmea_version()` / `get` | 设/取 NMEA 版本（V30/V41） |
| `ql_gnss_set_nmea_type()` | 设输出 NMEA 语句类型（bitmask） |
| `ql_gnss_suspend()` / `ql_gnss_resume()` | 停/恢复 NMEA 输出但**保持引擎工作** |
| `ql_gnss_inject_agnss_data()` | 注入 **EPH** AGNSS 数据 |
| `ql_gnss_inject_agnss_data_full(lat, lon)` | 注入 **FULL** AGNSS 数据（带粗略经纬度，误差≤15km） |
| `ql_gnss_inject_local_agnss_data()` | 注入本地 `/data/agps.bin`（每次网络请求成功后覆盖） |

**关键枚举/掩码**：

```c
typedef enum { QL_GNSS_COLD_START=0, QL_GNSS_WARM_START, QL_GNSS_HOT_START } QL_GNSS_START_MODE_E;
typedef enum { QL_GNSS_ENGINE_STATE_OFF=0, QL_GNSS_ENGINE_STATE_ON=1 } QL_GNSS_ENGINE_STATE_E;

typedef enum {                          // 星座组合
  GPS_ONLY=0x01, BDS_ONLY=0x02, GPS_BDS=0x03, GLONASS_ONLY=0x04,
  GPS_GLONASS=0x05, BDS_GLONASS=0x06, GPS_BDS_GLONASS=0x07, GPS_SBAS_QZSS=0x08,
  GPS_BDS_GALILEO_SBAS_QZSS=0x11,
  GPS_GLONASS_GALILEO_SBAS_QZSS=0x101   // 暂不支持
} QL_GNSS_CONSTELLATION_MASK_E;

typedef enum { QL_GNSS_NMEA_VERSION_V30, QL_GNSS_NMEA_VERSION_V41 } QL_GNSS_NMEA_VERSION_ID_E;

// NMEA 语句类型 bitmask（置 1 输出对应语句）
GGA(1<<0) GLL(1<<1) GSA(1<<2) GSV(1<<3) RMC(1<<4) VTG(1<<5) ZDA(1<<6) GST(1<<7)
```

- `ql_gnss_start()` 备注：默认用 `/etc/ql_locationd.conf` 配参；要用 API 设参须 `dynamic_set=1` 且**先 start**。
- 示例代码：`init`→`set_ind_cb`→`set_service_error_cb`→`start`→`sleep(2)`→查引擎状态/NMEA 版本→`while` 等输入 `-1` 则 `stop`+`deinit`。回调里区分 `QL_GNSS_NMEA_MSG`（打印 NMEA 语句）与 `QL_GNSS_STATUS_MSG`（FOTA_START/BOOT/FIRM/WORKING 状态）。

### 第 5 章 AGNSS 辅助定位

- 服务器经 TCP 提供 GPS/BDS 实时星历，缩短首次定位时间。**AGNSS 数据有效期 4 小时**（自下载起），超期 GNSS 芯片不再使用。
- 需联网；移远已封装代理服务器与账号（**账号密码已含在 SDK 中无需输入**），可自建服务器。
- **常见场景**：① 冷启动注入加快定位；② 长时间失锁注入恢复；③ 地库 4h 内出库可注入本地 `agps.bin` 旧数据或有网时注入新数据；④ 定位偏差大注入修正（推荐最新）。
- 流程：开机冷启动/休眠唤醒 → 注入 FULL（较全，一次约 6kB）或 EPH 数据。
- **前提备注**：用 AGNSS 前须**已正常拨号、能上网**（参考数据拨号开发指导）。

### 第 6 章 实现热启动

- 热启动模式 1~3s 完成定位。
- 示例：`set_start_mode(2)`（即 `QL_GNSS_HOT_START`，代码用 `mode=2`）。
- **6.2 休眠 32K 时钟配置**：热启动需打开 32K 时钟，模块休眠时默认关。改 `ql-ol-rootfs/etc/ql_locationd.conf` 的 `GPIO_32K_STATUE=0`→`1` → `make` 重编 → 替换固件包 `/update/` 下 zImage、root.squashfs → 烧录。**代价：热启动耗流增加 0.2 mA。**

### 第 7 章 常见问题

- **7.1 `-1099` 错误码（服务端异常）处理**：① `ql_gnss_deinit()` 关客户端；② `/etc/init.d/ql_locationd restart` 重启服务；③ 检测 `/tmp/ql_gnss_srv_ready.flag==1` 表示重启完成；④ 重做开机后的 GNSS 操作。
- **7.2 休眠唤醒**：`ql_gnss_stop()` → 进休眠后唤醒 → `ql_gnss_start()`。

---

## 3. 关键警告与坑（手册"备注"汇总）

1. **回调里禁调任何 GNSS API、禁阻塞** —— 回调运行在服务线程，违反会死锁/错乱。
2. **API 设参需 `dynamic_set=1` 且先 start** —— 否则只认配置文件。
3. **GNSS API 不支持多线程，不建议多进程** —— 否则 `ql_locationd` 状态错乱。
4. **AGNSS 数据 4 小时有效期** —— 过期无效，需重新注入。
5. **AGNSS 依赖联网** —— 必须先拨号成功（与 open_dial 强相关，见下）。
6. **热启动需 32K 时钟 + 重编固件**，且增耗流 0.2mA。
7. **`-1099` 必须 deinit + 重启 ql_locationd**，靠 flag 文件判就绪。
8. **Galileo 仅 AG35-EUT 支持**，`GPS_GLONASS_GALILEO_SBAS_QZSS` 暂不支持。

---

## 4. 对 open_dial 项目的适用性批注

> open_dial 是**拨号守护进程**，不做定位；GNSS 是独立功能模块。两者**唯一交集**是 AGNSS 依赖网络。本文档面向 AG35，EC200A 是否带 GNSS/`ql_gnss.h` 需以其 SDK 为准。

| 文档内容 | 与 open_dial 的关系 / 可借鉴点 |
|---|---|
| **AGNSS 必须先拨号上网（5 章备注）** | **最直接的交集**：若同机有 GNSS+AGNSS 业务，则 open_dial 的拨号可用性直接决定 AGNSS 能否更新星历、能否快速定位。open_dial 断网恢复越快，对 AGNSS 体验越友好——印证拨号守护是其它联网功能的基础设施。 |
| **回调里禁调 API、禁阻塞、不支持多线程** | 与 open_dial CLAUDE.md v1.28.2 教训**完全同源**：SDK 回调在独立线程触发，回调内做重活/调 SDK 会出问题。open_dial 的 `dial_log` 递归锁、SIM/SDK 回调里只置标志不干重活，正是这条通用纪律的体现，可互相印证。 |
| **`-1099`(SERVICE_ABORT) 恢复套路** | 与 ATC 文档的 `-1099` 一致：deinit+重启服务。若 open_dial 将来直接调 SDK 服务（而非 serial_atcmd），可复用"服务异常→deinit→重启 init"的健壮性模式。 |
| **`/etc/init.d/ql_locationd restart` + ready flag 文件** | "重启服务 + 轮询 `/tmp/*.flag` 判就绪" 是 QuecOpen 服务管理的通用范式，与 open_dial 的 `/tmp/network_status`、`/tmp/dial_version` 状态文件思路一致。 |
| **配置走 `/etc/*.conf` + 重编固件生效** | 与 open_dial `reboot_conf/` 持久化、apn.json 配置文件的思路同类；提示 QuecOpen 很多底层行为靠 `/etc/` 配置 + 重编。 |

**结论**：本文档对 open_dial **相关度低**（拨号守护不涉及定位）。价值有二：① 若同机跑 GNSS/AGNSS，则拨号稳定性是其前提，强化 open_dial 的基础设施定位；② GNSS API 的"回调禁阻塞/禁调 API/单线程"纪律与 open_dial 已踩过的并发坑同源，可作为 SDK 回调编程的通用反面教材引用。
