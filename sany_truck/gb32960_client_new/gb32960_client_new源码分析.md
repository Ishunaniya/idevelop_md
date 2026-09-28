# gb32960_client_new 仓库全面分析

> 本文对照参考模板 `tsp_0f7b_client/md/analyse/仓库全面分析.md` 的结构与深度，对本仓库源码做实测级、逐文件、file:line 佐证的全面分析。凡"事实"均给出代码位置；凡"推断/未确证"均显式标注。

---

## 0. 一句话概览

`gb32960_client_new` 是三一新能源重卡 T-BOX 上的 Quectel OpenCPU（EG25 / EC200A）嵌入式 C 应用，实现**国标 GB/T 32960**（新能源汽车远程服务与管理数据规范）：从 CAN 总线按点表解码整车/驱动电机/燃料电池/发动机/位置/极值/报警数据，评估报警等级，通过 **TCP 直连**上报实时数据、补发历史数据（含报警前 30 秒），并做登入/登出会话管理。区别于其它兄弟仓库走 `mgw` MQTT 网关，本仓库主上行是 **`send()` 到国标平台的裸 TCP 长连接**（`can_mng->iClient`）。

---

## 1. 项目定位与运行环境

- **平台**：Quectel EG25-G / EC200A OpenCPU，交叉编译，产物 `gb_32960_client`（`Makefile` `APP_NAME`）。
- **业务**：GB/T 32960-2016 国标车联网数据上报（面向政府/国标监管平台，非三一 TSP 企标）。
- **版本**：`README.md` 记为 v16；`main.c:17` `#define APP_VER 16`。注意 `main.c:46` 打印的 `version` 却硬编码为 `"6.9"`，与 `APP_VER` 不一致（历史遗留字符串，`set_app_ver()` 在 `main.c:58` 已被注释）。
- **车型适配**：423/431/460/861 单体电池布局不同；437 用独立点表并有 `-DMACHINE_TYPE_437` 编译分支（`Makefile:13` 注释掉的备用链接行）。运行时点表由 `/opt/gb_437_machine.flag` 存在与否选择（`main.c:100-107`）。
- **必备落地条件（`main.c` 启动即校验，缺则退出）**：
  - `/opt/machine_vin`（VIN，`main.c:78-96`，重试 10 次×2s）；
  - ICCID（`get_iccid_from_local_file`，`main.c:88-92`，缺则 `return 0` 退出）；
  - 点表 JSON（`main.c:108-112`）。

---

## 2. 构建系统

### 2.1 组成

- `Makefile`：主构建文件，通过 `QL_PLATFORM`（`EG25` / `EC200A`）选择工具链，默认 `EC200A`；EC200A 分支 `include sdk.mk`。
- `sdk.mk`：导出 `QL_SDK_DIR` 与工具链路径。
- `include/`：vendored `sqlite3.h`、`mosquitto.h`、`uthash.h`、`cn-cbor.h`。

### 2.2 关键事实

- 编译并 strip 出 `gb_32960_client`，无 `install` 目标；`make clean` 删二进制。
- **链接库**（`Makefile` LDFLAGS）：`-lpthread -lm -ldl -lcn-cbor -lmosquitto`。注意 `-lnanomsg -lappmng -lstdc++` 那一行被注释掉了 → 本仓库**不依赖 appmng/nanomsg**，与主上行走裸 TCP 相印证。
- **本地化**：`sdk.mk`/`Makefile` 已指向本地 SDK：EC200A sysroot 位于 `/home/tronlong/lyp/SDK/EC200A/...`，EG25 分支位于 `/home/tronlong/lyp/SDK/EG25/ql-ol-sdk/ql-ol-extsdk`。SDK 迁移需同步改这两处。
- 车型编译开关 `-DMACHINE_TYPE_437`（默认关闭）。

### 2.3 源码纳入范围（Makefile glob，`Makefile:63-72`）

`main.c` + `find` 纳入的目录：`can_mng`、`can_server`、`common`、`data_process`、`list`、`mosquitto`、`scene`、`cJSON`、`opt_iniparser`、`gps`、`new_energy`。

- **`src/scene_lib/scene_judge.c` 不进主二进制**（主 glob 只 `find src/scene`，不含 `scene_lib`），但**另有独立规则编译成 `libscene.so`**（`Makefile:88`：`$(CC) -fPIC ... src/scene_lib/scene_judge.c -shared -Wl,-soname,libscene.so -o libscene.so.1.0.0`），供主程序 `dlopen`。因此它**不是孤立死代码，而是独立 .so 编译目标**（详见 §8）。
- `src/mosquitto/client_mosquitto.c`（225 行）**被编入主二进制**，但其线程入口 `start_mqtt_task`（`client_mosquitto.c:97`）**全仓无任何 `pthread_create` 启动**（`main.c` 仅 `#include` 头文件，8 个线程无一是它）→ **实际为死代码**，本 build 下 MQTT 通道未启用（详见 §8）。主数据面完全走 TCP（`iClient`）。

---

## 3. 进程与线程模型

单进程，`is_instance_existing()`（`main.c:20-29`）用 `/tmp/1c04b866-...gb32960.pid` 的 `flock` 做单实例互斥。

`main()`（`main.c:43`）在加载配置/VIN/ICCID/点表、恢复持久化里程与 GPS 后，创建 **8 个工作线程**（全部共享同一 `can_mng`）：

| # | 线程入口 | 源文件 | 职责 |
|---|----------|--------|------|
| 1 | `sany_gps_read_thread_binary` | `gps` | GPS 读取（二进制协议） |
| 2 | `start_can_server` | `can_server` | CAN 接收 + 点表解码写入规则树 |
| 3 | `gb32960_check_warn_level` | `new_energy/gb32960.c:949` | 报警等级评估 |
| 4 | `pthread_save_log` | `new_energy/gb32960.c` | 日志落盘 |
| 5 | `gb32960_data_report` | `new_energy/gb32960.c:1729` | **国标实时上报主状态机**（TCP） |
| 6 | `history_data_to_transparent` | `new_energy/gb32960.c` | 历史/透传数据处理 |
| 7 | `pthread_save_history_data` | `new_energy/gb32960.c:1293` | 历史数据入 SQLite |
| 8 | `pthread_save_warn_30_data` | `new_energy/gb32960.c:2008` | 报警前 30 秒缓冲落盘 |

启动时序有意错开：线程 1-4 先起，`sleep(5)` 让单体列表数据收全再起线程 5（`main.c:121-123` 注释"等待10秒把单体列表数据收完再组包上传"，实际 sleep 5s），线程 6 起后 `sleep(1)` 再起 7、8。

主循环（`main.c:133-174`）：每 4s 读 ACC；每 180s 落盘里程/GPS（`/opt/mileage_data.txt`、`/opt/gps_data.txt`）；每轮写 `/tmp/gb32960_heatbeat_time` 心跳时间戳；若 `gb32960_block_flag==0x11`（平台下发封停）则 `exit(0)`（`main.c:160-163`）。

**共享保护**：仅单体电压/温度列表（`_battlist`/`_templist`/`cur_*`）由 `gb32960_mutex`（`can_mng.h:35`）保护；`iClient`、`_warnLevel`、`gb32960_block_flag`、`ptr_gps` 等跨线程字段**无锁**（详见 §21 与 §19）。

---

## 4. 总上下文 `can_mng_t` 与设备身份

### 4.1 结构（`can_mng.h:17-69`）

关键字段（原文注释保留）：

- `data_process_mng` —— 点表规则树管理器（解码核心）。
- `judge_scene`（函数指针）—— 运行时由 `dlopen` 载入 `libscene.so` 的 `judge_scene`（`scene/scene.c:7-36`）。
- `absolute_path` —— 程序所在目录，用于拼点表/so 路径。
- `can_fd[CAN_CHN_NUM]`、`can_rcv_cnt[]` —— CAN 通道 socket 与计数。
- `platform_info`（`platform_info_t`）—— 国标平台 IP/port（`get_32960_platform_info`）。
- `_battCellTotal`/`_tempCellTotal`/`_battBoxNum`/`_BattPackTotalNum` —— 单体电压/温度探针数、电池箱数。
- `_templist`/`_battlist` + `cur_*` + `gb32960_mutex` —— 单体列表双缓冲 + 互斥。
- `_warnLevel`/`_warnFlag`/`_warnReportStatistics` —— 报警等级状态。
- `iClient`/`_iServer` —— **国标平台 TCP 客户端 socket** / 本地服务端。
- `_intervalSave`/`_intervalReport` —— 存储/上报周期（国标可由平台参数下发）。
- `vin[18]`/`iccid[32]`、`latitude`/`longitude`、`local_mileage`、`heart_beat_time`、`gb32960_block_flag`、`network_offline`、`history_is_sending`。

### 4.2 `can_mng_init()`（`can_mng.c`）

分配并清零 `can_mng`，初始化 `gb32960_mutex`，解析 `absolute_path`，读取平台/设备配置，置各周期与状态默认值。（对照参考模板的 `can_mng_new()`，本仓库更名为 `can_mng_init`，语义一致。）

---

## 5. 数据管线（跨文件最多、最核心）

本仓库的点表→规则树→解码链路与 `tsp_0f7b_client` 同源（同一 `data_process` 家族），差异在于**上报侧按 GB/T 32960 数据单元组织**，而非三一企标 payload。

### 5.1 点表（数据来源，非代码）

- `Pointsheet_info1_gb32960.json`（通用）/ `Pointsheet_info1_gb32960_437.json`（437 车型）。
- 由 `/opt/gb_437_machine.flag` 是否存在 + 文件是否存在决定（`main.c:100-107`）。
- 点表定义 CAN-ID/bit → 国标字段的解码规则；**改数据映射改 JSON，不改 C 代码**。

### 5.2 加载：`load_pointsheet_json()`（`src/data_process`）

解析点表 JSON，递归建规则树（同参考模板 `get_common_data_rule` 家族）。

### 5.3 写侧（CAN 线程，持续运行）

`start_can_server`（`src/can_server`）从本地 socket 收 `can_frame_t`，按规则树把信号值写进树节点。此外，每帧还调用两个国标专用解码器（`can_server.c:50-51`、`120-121`）：

- **`parse_voltTempList_msg()`（`gb32960.c:452-943`，约 490 行）**：单体电压/温度**多帧组装解码器**——本仓库最复杂的解码函数。用一批 `static` 局部（`battBoxSerial`/`_battPackNum_total`/`_battCellNumPerPack`/`_curCellNum`/`_curTempNum` 等）跨帧累计箱体/串/并/温度探针数与当前取值游标，把每帧 12-bit 单体电压（`getVale(msg,8/20/32/44,12)`，`gb32960.c:747-757`）经 `toBigEndianShort` 写入 `_battlist._volt[]`。**因只在 CAN 单线程调用，`static` 状态无并发问题**；写入 `_battlist`/`_templist` 由 `gb32960_mutex` 保护。
- **`charge_check()`（`gb32960.c:1686`）**：从 CAN 帧判定充电状态（`_isIncharge`）。

单体槽位写入 `gb32960_volt_slot_write`/`gb32960_temp_slot_write`（`gb32960.c:431-449`）对 `idx` 做**双重上界钳制**（`_battCellTotal` 与 `GB32960_VOLT_ARRAY_MAX = sizeof(_volt)/sizeof(_volt[0])`，`gb32960.c:393-394/408-410`），越界丢弃——防御式写法良好，无数组越界风险。

### 5.4 读侧 + 取值

`gb32960_get_work_data(data_process_mng, out, "字段名", CAN_ID)`（`new_energy_frame_pack.c:140-141` 等处大量调用）从规则树按字段名 + CAN-ID 拉一个 `dataItem_t`（含 `value_int` 等）。报警等级评估用 `checkwarnLevel((char)dateItem.value_int, checkLevel)`（`gb32960.c:949+`）。

---

## 6. GB/T 32960 帧协议封装

### 6.1 帧头结构 `gb4_dataFrame_t`（`new_energy_frame_pack.h:142-151`，`__attribute__((packed))`）

| 字段 | 长度 | 说明 |
|------|------|------|
| `_szStart[2]` | 2 | 起始符固定 `##`（`0x23 0x23`） |
| `_Command` | 1 | 命令标识 |
| `_Ack` | 1 | 应答标志 `INSTRUCT_FLAGS` |
| `_szVIN[17]` | 17 | 车辆唯一识别码（VIN） |
| `_enEncryptionType` | 1 | 加密方式：`0x01`=不加密 / `ENCRYP_AES128`=AES128 |
| `_DataLen` | 2 | 数据单元长度（大端） |
| `_ChkBCC` | 1 | BCC 异或校验 |

有效头部 24 字节（`GB4_BODY_OFS=24`），BCC 追加在体尾 → wire 帧 = 24 + body_len + 1。

**命令字**（`new_energy_frame_pack.h:9-16`）：
- `INSTRUCT_LOGIN_01 (0x01)` 车辆登入
- `INSTRUCT_REAL_02 (0x02)` 实时信息上报
- `INSTRUCT_HISTORY_03 (0x03)` 补发信息上报
- `INSTRUCT_LOGOUT_04 (0x04)` 车辆登出

**数据单元类型**（`new_energy_frame_pack.h:71-77`）：`0x01`整车 / `0x02`驱动电机 / `0x03`燃料电池 / `0x04`发动机 / `0x05`车辆位置 / `0x06`极值 / `0x07`报警。

### 6.2 组帧：`gb4_frame_pack_full()`（`new_energy_frame_pack.c:27-61`）

1. `malloc(sizeof(gb4_dataFrame_t)+body_len)`；
2. 填 `##`、命令、应答、`memcpy(_szVIN, can_mng->vin, 17)`、`_enEncryptionType=0x01`、`_DataLen=toBigEndianShort(body_len)`；
3. `memcpy` 头 24B + `memcpy` body；
4. `_ChkBCC = getBCC(pFrameFullOut+2, frameFullOutLen-3)`（从起始符后一字节起到 BCC 前，`common.c:94-102` 逐字节异或）；
5. BCC 写入体尾。

AES128 变体 `gb4_frame_pack_AES128_full()`（`new_energy_frame_pack.c:72-104`）逻辑相同，差异：`_enEncryptionType=ENCRYP_AES128`，且 VIN 取自 `getVIN()` 而非 `can_mng->vin`。**但经全局检索，此函数从未被任何地方调用**（`ENCRYP_AES128` 仅出现在其自身定义 `:92`），所有实际组帧（登入 `:151`、登出 `:197`、实时 `:2500/:2639`）一律走 `gb4_frame_pack_full`，加密方式恒为 `0x01`（不加密）。**故本 build 下 AES128 组帧是死代码，实际不加密上报**（§20）。

### 6.3 登入体：`gb4_frame_pack_login()`（`new_energy_frame_pack.c:115-141+`）

`gb4_login_t`（`new_energy_frame_pack.h:154+`）：`_szTime[6]`（BCD：年%100/月/日/时/分/秒）+ `_sessionId`（大端登入流水号）+ `_strICCID[20]` + 电池包数等（`gb32960_get_work_data(..., "BMS6_BattPackNum_Total", ...)`）。会话流水号由 `/opt/conf_loginout.ini` 持久化（`loginout_get_sessionId` `gb32960.c:1596+`）。

### 6.4 登入/登出会话管理（`gb32960.c:1512-1620+`）

- `login_ready()`：读 `loginout:stat`，若为 `DEVICE_LOGIN` 判定已登入。
- `loginout_set_stat()`：`iniparser_set("loginout:stat", ...)` 回写。
- `loginout_info_default()`：默认 `[loginout] stat=LOGOUT / [session] id=0`。
- `loginout_get_sessionId()`：读 `session:id`，缺失/异常回落默认。

---

## 7. 关键工作线程细节

### 7.1 上报主状态机 `gb32960_data_report`（`gb32960.c:1729`）

- `loginout_info_init()` → `loginout_get_sessionId()` 恢复上次流水号；`sessionId<0` 时置 0（`gb32960.c` ~1751，`sessionId` 为 `unsigned short`，该判断恒假，见 §19）。
- 建立/维护到平台的 TCP 长连接：`skt_res_new_socket_ip(platform_info.ip, port)`（`gb32960.c:1821`），设 `TCP_USER_TIMEOUT`、`SO_RCVTIMEO`（`gb32960.c:1826-1828`）；断开时 `close(iClient); iClient=-1`（`gb32960.c:1813-1815`）后重连并打印 `creat new connect success`。
- 发送统一走 `data_send_out(can_mng->iClient, data, len, arg)`（`gb32960.c:301/328/1403`）。
- 接收平台指令：`recv_with_timeout_ms(iClient, rBuffer, ..., 10000)`（`gb32960.c:1780`）。
- 心跳超时：`heartbeat_time - heart_beat_time > 120`（`gb32960.c:143`）判定链路异常。

### 7.2 报警等级 `gb32960_check_warn_level`（`gb32960.c:949`）

轮询多路报警信号，逐项 `checkwarnLevel((char)dateItem.value_int, checkLevel)` 取最高等级，写 `_warnLevel`/`_warnFlag`。报警触发时联动 30 秒缓冲上报（`warn_before30s`/`warn_behind30s` 在 `gb32960.c:1204-1205` 被注释，实际由 `pthread_save_warn_30_data` 承担）。

### 7.3 历史补发 `pthread_save_history_data` / 历史发送

- `pthread_save_history_data`（`gb32960.c:1293`）：离线/断网时把国标帧写入 SQLite（`include/sqlite3.h`，`GB32960_HISTORY` 目录）。
- 联网后（`iClient>0` 且 `isdirempty(GB32960_HISTORY)==0`，`gb32960.c:1352-1403`）逐条读出 `data_send_out` 补发（命令 `0x03`）。

### 7.4 CAN 线程 / GPS 线程

- `start_can_server`（`src/can_server`）：本地 socket 收 CAN，写规则树。
- `sany_gps_read_thread_binary`（`src/gps`）：GPS 二进制解析，填 `can_mng->ptr_gps`（`main.c:147-149` 主循环据此落盘经纬度，注意 `lat = ptr_gps->lat * 1000000` 为 `float→unsigned int` 截断）。

### 7.5 场景 `scene`（`dlopen`）

`load_libscene()`（`scene/scene.c:7-36`）：`dlopen("<absolute_path>/libscene.so", RTLD_NOW)`，`dlsym("judge_scene")` 挂到 `can_mng->judge_scene`。`.so` 缺失只打印 `dlerror()`，不致命。

---

## 8. 孤立 / 死代码 / 独立目标模块

| 模块 | 行数 | 状态 | 判据 |
|------|------|------|------|
| `src/scene_lib/scene_judge.c` | 97 | **独立 .so 编译目标（非死代码）** | 不进主二进制（主 glob 无 `scene_lib`），但 `Makefile:88` 专门把它编成 `libscene.so.1.0.0`，供主程序 `scene/scene.c:16-18` `dlopen`。属设计意图。 |
| `src/mosquitto/client_mosquitto.c` 的 `start_mqtt_task` | 225 | **死代码（编入但从不启动）** | 定义于 `client_mosquitto.c:97`、声明于 `.h:10`，但全仓 `grep pthread_create` 无一启动它（`main.c:117-127` 的 8 线程无 MQTT）。本 build 下 MQTT 通道未启用，回调 `mosq_*_callback` 均不触发。 |

其余目录（`can_mng`/`can_server`/`common`/`data_process`/`list`/`scene`/`cJSON`/`opt_iniparser`/`gps`/`new_energy`）均在 Makefile 主 glob 内并实际参与业务。**本仓库无加密源文件**（`find src -name '*.c/.h' | file` 全为文本，无 `^data`）。

> 与兄弟仓库对比：`tsp_vin_client` 的 `src/tsp/` SEAS 框架是"存在源码但完全不进二进制"的孤立模块；本仓库不同——`scene_lib` 是**有意的独立 so 目标**，`mosquitto` 是**编入却不启动的死代码**，两者性质各异，需区别对待。

---

## 9. 命名/一致性坑

1. **VIN 取值双源**：`gb4_frame_pack_full` 用 `can_mng->vin`（`nef_pack.c:47`），`gb4_frame_pack_AES128_full` 用 `getVIN()`（`nef_pack.c:90`）。两者理应一致，混用增加不一致风险（§21）。
2. **`_DataLen` 声明为 `short int` 有符号**（`nef_pack.h:148`），body_len 超 32767 时符号位翻转（国标单帧一般不至，但极值/单体多时需留意）。
3. **打印版本 `6.9` ≠ `APP_VER 16`**（`main.c:46` vs `main.c:17`）。
4. **`location_data` / `real_data` 组包函数各出现两份**：`MACHINE_TYPE_437` 的 `#if` 变体（车型差异，非重复 bug）。
5. `can_mng_init` vs 兄弟仓库 `can_mng_new`：同语义不同名。

---

## 10. 运行时外部文件依赖总表

| 路径 | 读/写 | 用途 | 代码位置 |
|------|-------|------|----------|
| `/opt/machine_vin` | 读 | VIN | `main.c:79` |
| ICCID 本地文件 | 读 | ICCID | `main.c:88` |
| `/opt/gb_437_machine.flag` | 读(存在性) | 选点表 | `main.c:100` |
| `Pointsheet_info1_gb32960[_437].json` | 读 | 点表 | `main.c:101-108` |
| `/opt/mileage_data.txt` | 读/写 | 里程持久化 | `main.c:114,144` |
| `/opt/gps_data.txt` | 读/写 | GPS 持久化 | `main.c:115,157` |
| `/opt/conf_loginout.ini` | 读/写 | 登入登出状态 + 会话流水号 | `gb32960.c:1522-1613` |
| `/opt/conf.ini`、`/opt/conf_ext.ini`、`/opt/conf_ext2.ini`、`/opt/parameter_conf.ini` | 读 | 设备/平台配置 | `get_dev_config`/`get_32960_platform_info` |
| `/tmp/warn.ini` | 读/写 | 报警状态 | 报警线程 |
| `/tmp/gb32960_heatbeat_time` | 写 | 心跳时间戳（看门狗） | `main.c:166-171` |
| `/tmp/1c04b866-...gb32960.pid` | 锁 | 单实例 | `main.c:21` |
| `GB32960_HISTORY`(SQLite) | 读/写 | 断网历史补发 | `gb32960.c:1293-1403` |
| `<path>/libscene.so` | dlopen | 场景判断 | `scene.c:16-18` |

---

## 11. 完整数据流（端到端时序）

```
物理CAN → 桥接服务 → 本地socket(16002/3/4)
   → start_can_server 收 can_frame_t
   → 点表规则树解码 → 树节点 / _battlist / _templist(加锁)
                          │
   GPS二进制 → ptr_gps ───┤
                          ▼
   gb32960_data_report(每 _intervalReport)：
       gb32960_get_work_data 拉各字段
       → 组各数据单元(0x01整车…0x07报警)
       → gb4_frame_pack_login/real/... 组体
       → gb4_frame_pack_full(##+头+体+BCC)
       → data_send_out(iClient) ──TCP──▶ 国标平台
                          │
   断网 → pthread_save_history_data → SQLite
   联网 → 补发(命令0x03) → data_send_out(iClient)
                          │
   报警 → gb32960_check_warn_level → _warnLevel
        → pthread_save_warn_30_data(报警前30s) → 落盘/上报
```

---

## 12. 结论性要点（速览）

1. 主上行是 **裸 TCP 直连国标平台**（`iClient`/`data_send_out`/`send`），与兄弟仓库走 `mgw` MQTT 网关**不同**；`mosquitto` 模块虽被编入，但 `start_mqtt_task` 从不启动（死代码），本 build 无 MQTT 通道。
2. 帧格式严格遵循 GB/T 32960：`##` + 命令 + 应答 + VIN + 加密方式 + 大端长度 + body + BCC。虽有 AES128 组帧函数，但**从未被调用（死代码），实际上报恒为不加密 `0x01`**。
3. 数据面与 `tsp_0f7b` 同源的"点表→规则树→按字段名取值"链路，改映射改 JSON。
4. 断网走 SQLite 历史缓存 + 联网 `0x03` 补发；报警走前 30 秒缓冲。
5. 车型差异（437 / 单体布局）由点表 + `MACHINE_TYPE_437` 编译分支双重适配。
6. `scene_lib` 编译成独立 `libscene.so`，主程序 `dlopen` 动态挂载 `judge_scene`。
7. 缺 VIN/ICCID/点表任一即启动失败退出；平台可下发 `block_flag=0x11` 令进程 `exit(0)`。

---

## 13. GB/T 32960 国标定位（与三一企标的关系）

- 本仓库对应**国标监管上报**（GB/T 32960-2016 新能源汽车远程服务与管理系统），对端是国标/政府平台，与 `tsp_0f6b/0f7b` 系列的**三一企标**（以 `B` 结尾的自定义命令号、走 mgw/MQTT）是两套并行体系。
- 命令号体系是国标固定的 `0x01/0x02/0x03/0x04`（登入/实时/补发/登出），数据单元 `0x01~0x07`，与企标的 `0fxx` 命令族无交集。
- 加密：国标支持数据单元 AES128（`ENCRYP_AES128`），企标用 XOR/转义。二者组帧完全不同。

---

## 14. 若需扩展国标数据单元 / 新增字段的落地路径

### 14.1 现有框架已是"每类数据单元一个 pack 函数"（可复用）

`gb4_frame_pack_login/real/location/motor/extreme/warn/...` 各自成体，`gb4_frame_pack_full` 统一加头加校验。

### 14.2 落地步骤（按依赖顺序）

1. 点表 JSON 增字段映射（CAN-ID/bit → 字段名）。
2. 在对应数据单元 pack 函数里 `gb32960_get_work_data(..., "新字段名", CAN_ID)` 取值并按国标字节序写入体。
3. 若涉及新数据单元类型，在 `nef_pack.h:71-77` 扩枚举，并在实时组包主流程串接。
4. 若车型差异，走 `MACHINE_TYPE_437` `#if` 分支。

### 14.3 阻塞项 / 需外部确认

- 新字段的国标编码/量纲/字节序需对国标手册（非代码可闭合）。
- 平台侧参数下发（`_intervalReport`/`_intervalSave`）的报文格式需对端文档。

---

## 15. `README` / `doc` 文档定位

- `README.md`：版本变更（v16），车型（423/431/460/861/437）差异说明。
- 点表 JSON 是"事实上的配置文档"——字段级映射以它为准。
- 国标字段语义、AES128 密钥来源、平台参数下发格式，仓库内**无法闭合**，需 GB/T 32960 手册与平台接入文档。

---

## 16. 覆盖范围与边界（自评）

### 16.1 已核实覆盖（事实级）

- 构建/链接/本地化（`Makefile`/`sdk.mk`）；线程模型（`main.c:117-127`）；帧格式（`nef_pack.h:142-151`、`nef_pack.c:27-104`）；命令/数据单元枚举（`nef_pack.h:9-77`）；登入/会话（`gb32960.c:1512-1620`）；TCP 上行与重连（`gb32960.c:1780-1828`）；BCC（`common.c:94-102`）；外部文件依赖（逐一 file:line）；孤立模块判据（Makefile glob）；无加密文件（file 扫描）。

### 16.2 主动未深审（已声明，非遗漏）

- `cJSON`（3110 行，vendored）、`opt_iniparser`（1305 行，vendored dictionary/iniparser）、`list`（1624 行，通用容器）——第三方/通用库，按接口使用无独立缺陷价值。
- 各车型点表 JSON 逐字段语义（属配置数据）。

### 16.3 仓库内无法闭合的边界

- 国标字段编码/量纲、AES128 密钥、平台参数下发格式、`libscene.so` 的 `judge_scene` 实现（若 so 未随源提供则运行期行为不可静态确定）。

---

## 17. 结构与流向图集

### 17.1 模块依赖

```
main.c
 ├─ can_mng(总上下文, iClient/battlist/mutex)
 ├─ data_process(点表→规则树→取值)
 ├─ can_server(CAN收→写树/列表)
 ├─ gps(ptr_gps)
 ├─ new_energy/gb32960.c(状态机:登入/实时/补发/报警/历史)
 │    └─ new_energy_frame_pack.c(组帧:##+头+体+BCC / AES128)
 ├─ common(getBCC/toBigEndian/BCD/时间)
 ├─ mosquitto(编入但 start_mqtt_task 从不启动 = 死代码)
 ├─ scene(dlopen libscene.so)
 ├─ opt_iniparser / cJSON / list(基础设施)
 └─ [scene_lib → Makefile:88 独立编成 libscene.so, 不进主二进制]
```

### 17.2 线程与共享上下文

```
              ┌──────────── can_mng_t(共享) ────────────┐
gps线程 ──────┤ ptr_gps                                  │
can_server ───┤ 规则树 / _battlist/_templist(gb32960_mutex)│
warn_level ───┤ _warnLevel/_warnFlag                     │
data_report ──┤ iClient(TCP, 无锁) ──▶ 国标平台           │
history/warn30┤ SQLite / 30s缓冲                          │
save_log ─────┤ log_msg                                  │
主循环 ───────┤ acc_status/local_mileage/block_flag      │
              └─────────────────────────────────────────┘
```

### 17.3 最终 wire 帧字节布局

```
偏移  长度  字段
0     2     ## (0x23 0x23)
2     1     命令(0x01登入/0x02实时/0x03补发/0x04登出)
3     1     应答(INSTRUCT_FLAGS)
4     17    VIN
21    1     加密方式(0x01不加密 / AES128)
22    2     数据单元长度(大端, short有符号)
24    N     数据单元(0x01整车…0x07报警)
24+N  1     BCC = XOR(bytes[2 .. 24+N-1])
```

---

## 18. 函数级功能清单

### main.c
- `is_instance_existing`（`:20`）—— flock 单实例。
- `set_app_ver`（`:31`，**已注释未调用**）—— 写 `/tmp/gb32960_client.ver`。
- `main`（`:43`）—— 初始化 → 校验 VIN/ICCID/点表 → 建 8 线程 → 主循环（ACC/里程/GPS 落盘/心跳/封停）。

### can_mng/can_mng.c
- `can_mng_init` —— 建总上下文、初始化 mutex、解析路径、读配置、置默认。
- `get_dev_config` / `get_32960_platform_info` —— 设备与国标平台配置。

### common/common.c
- `getBCC`（`:94`）—— 逐字节异或校验。
- `getBCDfromhoneNum`（`:105`）—— 字符串→BCD。
- `decimal_bcd_code`（`:121`）—— 十进制→BCD。
- `toBigEndianInt`（`:343`）/`toBigEndianShort`（`:359`）/`toBigEndianShort12bit`（`:371`）—— 字节序转换。

### new_energy/new_energy_frame_pack.c（全 24 个顶层函数，逐一核对）
组帧底座：
- `gb4_frame_pack_full`（`:27`）—— 不加密组帧（实际唯一路径）。
- `gb4_frame_pack_AES128_full`（`:72`）—— AES128 组帧（**死代码，0 调用**，§20）。
- `gb4_frame_release_login_logout`（`:215`）/`gb4_real_data_copy`（`:1973`，static）/`gb4_real_data_release`（`:1988`，static）—— 内存释放/拷贝辅助。

各数据单元/命令体组包：
- `gb4_frame_pack_login`（`:115`）/`gb4_frame_pack_logout`（`:170`）。
- `gb4_frame_pack_vehicle_data`（`:284`，整车 0x01）/`gb4_frame_pack_Hydrogen_data`（`:561`，燃料电池 0x03）/`gb4_frame_pack_motor_data`（`:655`，驱动电机 0x02）/`gb4_frame_pack_location_data`（`:762`、`:840` 两份=437 `#if` 变体，位置 0x05）/`gb4_frame_pack_extreme_data`（`:929`，极值 0x06）/`gb4_frame_pack_warn_data`（`:1471`，报警 0x07）。
- **`gb32960_pack_volt_data`（`:1540`）/`gb32960_pack_temp_data`（`:1901`）—— 单体电压/温度数据单元打包（上传侧），带 `splitTotal/splitFrame` 分帧（单体多时拆多帧，`:2078-2620` 大量调用）。**
- `get_max_and_min_data`（`:906`）—— 极值（最高/最低电压温度）计算。
- `gb4_frame_pack_real_data`（`:2003`、`:2526` 两份=437 `#if` 变体，实时汇总）—— 串接 `vehicle/motor/extreme/volt/temp/...` 各单元 + `gb4_frame_pack_full(INSTRUCT_REAL_02)`。

持久化辅助（定义于此，`main.c` 与本文件双处调用）：
- `read_local_mileage`（`:229`）/`save_local_mileage`（`:254`）/`read_local_gps`（`:789`）/`save_local_gps`（`:827`）—— 里程/GPS 掉电持久化（`/opt/mileage_data.txt`、`/opt/gps_data.txt`）。

### data_process/data_process.c（点表解码核心，全 30 个函数）
- 十六进制/字节序辅助：`OneCharToHex`（`:22`）/`TwoCharToHex`（`:36`）/`CharToHexMem`（`:44`）/`big_endian`（`:61`）/`compare_double`（`:71`）/`get_clock_time`（`:81`）/`get_tick_count`（`:92`）/`get_file_data`（`:103`）（均 static）。
- 建规则树：`get_app_data_rule`（`:136`）/`get_common_data_rule`（`:250`，递归）/`load_pointsheet_json`（`:729`）/`load_general_param`（`:661`）/`by_id`（`:724`）。
- 写侧（CAN 解码）：`set_common_data_item`（`:434`）/`set_app_data_item_number`（`:589`）/`set_app_data_item_string`（`:617`）/`parse_common_msg`（`:649`，CAN 帧→规则树入口）。
- 读侧（国标取值）：`gb32960_check_data_item`（`:1407`，static）/**`gb32960_get_work_data`（`:1476`，按字段名+canID 取 `dataItem_t`，组包侧核心入口）**。
- 其它序列化器（非国标主链路）：`json_work_data_generate`（`:1075`）/`gz_work_data_generate`（`:1115`）/`cbor_work_data_generate`（`:1160`）/`get_schema_item`（`:1245`）/`cbor_model_schema_generate`（`:1267`）—— 沿用共享 data_process 血统的 CBOR/JSON/gzip 生成器，各仅个位数调用点，非 GB 二进制帧主链路。
- 管理/调试：`data_process_mng_init`（`:1339`）/`set_cur_scene`（`:1348`）/`print_data_rule`（`:1359`）/`print_interval`（`:1393`）。

### new_energy/gb32960.c（全 32 个顶层函数，逐一核对）
日志/离线：`save_log`（`:57`）/`send_log_msg`（`:87`）/`creat_offline_flag`（`:96`）/`remove_offline_flag`（`:106`）/`pthread_save_log`（`:116`，线程）。

网络收发：
- `data_send_out`（`:152`，`send()` 于 `:160`）—— 单次 `send(iClient)` 出口，用于登入/登出/补发。
- `core_network_send`（`:180`，含 `:219` 分片重发循环）—— 带超时完整发送，实时帧走此路径（`:1941`）。
- `wait_socket_data_ms`（`:244`，static）/`recv_with_timeout_ms`（`:266`，static）—— `select` 超时收。

登入登出/平台：
- `gb4_device_login`（`:287`）/`gb4_device_logout`（`:315`）—— 登入/登出发送。
- `get_32960_platform_info`（`:334`）—— 读平台 IP/port。
- `login_ready`（`:1512`）/`loginout_set_stat`（`:1541`）/`loginout_info_default`（`:1572`，static）/`loginout_get_sessionId`（`:1596`）/`loginout_set_sessionId`（`:1624`）/`loginout_info_init`（`:1663`）—— 会话状态机（`conf_loginout.ini`）。

CAN 解码（写侧）：
- `getVale`（`:369`）—— 按 bit 偏移/长度从 `can_frame_t` 取值（§19 G9 符号扩展）。
- `gb32960_volt_limit_cells`（`:396`，static）/`gb32960_temp_limit_sensors`（`:414`，static）/`gb32960_volt_slot_write`（`:431`，static）/`gb32960_temp_slot_write`（`:441`，static）—— 单体槽位上界钳制与写入。
- `parse_voltTempList_msg`（`:452`，约 490 行）—— 单体电压/温度多帧组装解码（§5.3）。
- `charge_check`（`:1686`）—— 充电状态判定。

报警/历史/上报：
- `checkwarnLevel`（`:944`）/`gb32960_check_warn_level`（`:949`，线程）—— 报警等级评估。
- `save_date_to_file`（`:1221`）—— 帧落盘。
- `pthread_save_history_data`（`:1293`，线程）/`history_data_to_transparent`（`:1338`，线程）/ 历史补发（`:1352-1403`）—— SQLite 缓存与 `0x03` 补发。
- `gb32960_data_report`（`:1729`，线程）—— 实时上报主状态机 + TCP 连接管理。
- `pthread_save_warn_30_data`（`:2008`，线程）—— 报警前 30 秒缓冲。

### mosquitto/client_mosquitto.c（编入但不启动）
- `start_mqtt_task`（`:97`）+ `mosq_connect/disconnect/subscribe/message_callback`（`:41-97`）—— **死代码**，无线程启动（§8）。

### scene/scene.c
- `load_libscene`（`:7`）—— `dlopen libscene.so` + `dlsym judge_scene`。

### 孤立/未编译（不进二进制）
- `src/scene_lib/*`（编译成 `libscene.so`，非主程序目标）。

---

## 19. 潜在缺陷清单（静态分析，附 file:line 佐证）

> 说明：以下为静态可疑点，部分需真机/对端验证（见 §21）。等级为影响评估。

**G1（中）`open()` 缺 mode 参数** —— `main.c:34`（`set_app_ver`，已注释）、`main.c:166`（心跳文件）`open(path, O_WRONLY | O_CREAT)` 未给第三个 mode 参数，新建文件权限位为栈上随机值。心跳文件 `main.c:166` 实际生效，建议补 `0644`。

**G2（中）`sessionId<0` 恒假** —— `gb32960.c` ~1751：`sessionId` 声明为 `unsigned short`，`if(sessionId<0) sessionId=0` 恒不成立；`loginout_get_sessionId` 返回 `-1`（`:1609`）赋给无符号量会变 `0xFFFF`，无法被此判据兜底 → 首次/异常时可能以 `0xFFFF` 作流水号登入。建议用有符号中间变量判断。

**G3（中）VIN 取值双源不一致** —— `nef_pack.c:47` 用 `can_mng->vin`，`nef_pack.c:90` 用 `getVIN()`。若二者来源/时序不同步，加密帧与非加密帧 VIN 可能不一致。建议统一。

**G4（低-中）`_DataLen` 有符号 short** —— `nef_pack.h:148` `short int _DataLen`，`toBigEndianShort(body_len)` 入参 body_len 超 32767 会符号溢出。单体数极多的极值/实时帧需评估上界。

**G5（中）`iClient` 跨线程无锁** —— `data_report` 线程在 `:1813-1821` `close`/重连改写 `iClient`，`history` 补发线程在 `:1358-1403` 读 `iClient` 并 `data_send_out`。两线程对同一 fd 无锁访问，重连瞬间历史线程可能向已 `close` 的 fd（或复用后的新 fd）写 → 竞态。建议加锁或收敛到单发送线程。

**G6（低）`ptr_gps` 空判后仍有截断** —— `main.c:147-149`：`lat = ptr_gps->lat * 1000000`（`lat` 为 `unsigned int`），浮点乘后隐式截断；南纬/西经为负时 `>0` 判据（`:154`）会误滤有效负坐标。国标位置数据单元另有经纬度符号位处理，此处仅落盘持久化，影响面为掉电恢复初值。

**G7（低）版本号不一致** —— `main.c:46` 打印 `6.9` 与 `APP_VER 16`（`:17`）不符，易误导现场排障。

**G8（观察）报警 30s 双线程注释切换** —— `gb32960.c:1204-1205` 的 `warn_before30s`/`warn_behind30s` 被注释，改由 `pthread_save_warn_30_data` 承担；需确认无功能缺口（前 30s 是否完整覆盖）。

**G10（低-中，本轮彻查新增）`read_local_gps` 解析 `/opt/gps_data.txt` 无界字符串拷贝** —— `new_energy_frame_pack.c:807-813`：`strncpy(lon_str, p+1, strlen(p+1))` 的拷贝长度取**源串长**而非 `sizeof(lon_str)`，对 `lon_str[32]` **毫无保护**；随后 `strcpy(lat_str, buff)` 亦无界，写入 `lat_str[32]`。`buff` 来自 `/opt/gps_data.txt`（本程序以 `"%d,%d"` 写入，正常约 21B < 32 故日常不触发）。若该文件被损坏/篡改为超长内容 → **栈缓冲区溢出**。建议 `strncpy` 用 `sizeof(dst)-1` 并补 `\0`、`strcpy` 换 `snprintf`。

**G9（低，平台相关，真机良性）`getVale` 有符号 char 符号扩展** —— `gb32960.c:369-372`：`char *ptrCanFrm = (char*)frdup->data; ... value |= ((uint64_t)ptrCanFrm[n]) << shift`。`ptrCanFrm[n]` 为 `char`，当字节 ≥0x80 时，若 `char` 为**有符号**会先符号扩展成巨大值再移位/或，污染结果。**目标平台 EG25/EC200A 均为 ARM，`char` 默认 unsigned → 真机良性**；但在 x86（host 单测）或显式 `-fsigned-char` 下会出错。属可移植性隐患，与兄弟仓库 `getVale` 同源。建议改 `unsigned char*`。此函数用于 12-bit 单体电压解码（`:747-757`）。

---

> **修复状态（2026-07，working tree 未提交；EC200A 交叉工具链完整编译通过，`gb_32960_client` 二进制生成，待真机验证）**
> - **G1 已修复**：`main.c:166` 心跳文件 `open(...,O_WRONLY|O_CREAT,0644)` 补 mode。
> - **G2 已修复**：`gb32960.c` 用有符号中间量 `session_id_ret` 判 `<0`，避免 `unsigned short` 判断恒假与 `-1→0xFFFF`。
> - **G5 已修复**：新增文件级互斥量 `s_iclient_mutex`，`data_send_out` 整帧 send、`core_network_send` 调用点、`close(iClient)` 均加锁，串行化两线程对平台 socket 的发送/关闭，消除 GB 帧字节流交错。
> - 其余 G3（分叉路径为死代码）、G4/G10（需超界/文件损坏才触发）、G6–G9（低severity/ARM 无害）按范围未改。

## 20. 死代码 / 注释代码扫描

- **整个 `src/mosquitto/client_mosquitto.c`（`start_mqtt_task` 及所有回调）—— 编入二进制却无任何线程启动，本 build 下为死代码**（§8）。
- `main.c:58` `//set_app_ver()` —— 版本文件写入被禁用。
- `main.c:75` `//char iccid[32]`、`nef_pack.c:135-136` 读 `/opt/iccid` 的注释 —— ICCID 来源已改。
- `gb32960.c:1204-1205` —— 报警前后 30s 独立线程被注释。
- `Makefile:13`、`Makefile` LDFLAGS `-lnanomsg -lappmng -lstdc++` 注释行 —— 旧 mgw/appmng 依赖已弃用，印证主上行改 TCP。
- `nef_pack.c:121` `//gb4_login_t loginAES128Body` 等——AES128 早期草稿。
- **`gb4_frame_pack_AES128_full`（`nef_pack.c:72-104`）整函数——定义完整但全仓 0 调用，实际上报恒为不加密（`_enEncryptionType=0x01`）。AES128 加密上报是死代码。**

---

## 21. 需真机 / 对端才能验证的假设点

1. `libscene.so` 是否随固件部署、`judge_scene` 具体规则（源在 `scene_lib`，产物为独立 so）。
2. 国标平台的 AES128 密钥协商 / 分发方式（代码只见 `ENCRYP_AES128` 标志，未见密钥来源闭合）。
3. `_intervalReport`/`_intervalSave` 是否由平台参数查询/设置报文动态下发，报文格式。
4. `gb32960_block_flag=0x11`（`main.c:160`）的平台下发触发条件与命令编码。
5. G5 的 `iClient` 竞态是否在真机重连频率下实际触发。
6. 各车型点表 JSON 字段与国标编码的对应正确性（配置正确性无法静态判定）。
7. VIN 双源（G3）在 AES/非 AES 帧混用场景下是否实际不一致。
8. `_DataLen` 有符号（G4）在最大单体数机型上是否触及 32767 上界。

---

*（本报告基于源码静态分析，file:line 均为实测；标注"推断/需验证"处未经真机确认。tsp_0f7b_client 作为参考模板未被本次分析修改。）*
