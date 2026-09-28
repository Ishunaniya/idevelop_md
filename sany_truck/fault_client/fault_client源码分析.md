# fault_client 仓库全面分析

> 本文基于对仓库源码、Makefile、头文件的逐文件通读得出，所有结论均标注了依据位置（`文件:行`）。
> 无法从代码直接确证、只能推断处，均显式标注"**推断**"。
> 参照对象：`tsp_0f7b_client/md/analyse/仓库全面分析.md`。
> **加密文件**：`src/json_parse/json_parse.c`（E-SafeNet，`file` 判为 `data`）——但它**未被编译**（见 §2.3），故不影响运行分析。

---

## 0. 一句话概览

跑在货车 TBOX（Quectel EC200A）上的用户态守护进程，作用是：
从本机 CAN 桥接读取整车 **J1939 DM1 故障**（含 BAM 多包）与一批**冻结帧信号**，
在内存/SQLite 里维护"故障触发/恢复"状态与历史，并把故障连同冻结帧与 GPS 打包成
**`0F4A` 整车故障上报帧**（JT/T 808 式），经本机 MQTT 上传到三一 TSP 平台。README 明确：采集 DM1 故障和冻结帧，上报 `0F4A`。

---

## 1. 项目定位与运行环境

| 项 | 事实 | 依据 |
|---|---|---|
| 二进制名 | `fault_client` | `Makefile:11` |
| 版本 | `v1.06`（`MAIN_VERSION=1 SUB_VERSION=6`，`.ver` 无写入） | `main.c:11-16,71` |
| 目标硬件 | EG25（`Makefile` 主）/ EC200A（`Makefile-EC200A`+`sdk.mk`） | `Makefile`、`README.md` |
| 编译环境 | README：克隆入 `ql-ol-sdk/.../example`，`source` 交叉工具链，`make` | `README.md` |
| 存储 | SQLite `/media/sdcard/fault.db`（`fault_status`+`fault_history` 两表） | `app_mng.h:25-27` |
| MQTT client-id | `"fault_client"` | `client_mosquitto.c:106` |

---

## 2. 构建系统

### 2.1 组成
`Makefile`（EG25，`QL_SDK_PATH ?= $(pwd)/../..`）、`Makefile-EC200A`（含 `sdk.mk`）。

### 2.2 目标
`make`（`.o`/`.d` 增量编译并链接 `fault_client`）、`make clean`。链接 `-lmosquitto -lsqlite3 -lpthread` 等（`sdk.mk`）。

### 2.3 源码纳入范围（`Makefile:22-36` wildcard，**关键事实**）
Makefile 通配的目录：`can_mng, can_server, mosquitto, cJSON, list, opt_iniparser, skt_res, local_socket, app_mng, fault, common, db, tsp, gps`。
与实际 `src/` 目录对照：
- **实际存在且被编译**：`mosquitto, cJSON, list, opt_iniparser, local_socket, app_mng, fault, common, db, tsp`。
- **通配了但目录不存在**（wildcard 空展开，无副作用）：`can_mng, can_server, skt_res, gps`——即本仓库**没有独立 CAN/GPS 模块目录**，CAN/GPS 接收全在 `src/local_socket/` 内实现。
- **存在但未被通配 → 未编译**：`src/json_parse/`（其 `json_parse.c` 是加密文件，但因未编译而与运行无关；`json_parse.c` 明文仅 5 行占位，`json_parse.h` 9 行）。

---

## 3. 进程与线程模型

`main()`（`main.c:43`）：全局 `app_mng` 单例；装 `SIGTERM/INT/USR1/USR2` 处理（`keep_running`/`show_status`）；`app_mng_init` → `db_init` → `sync_list_from_db`（把 SQLite `fault_status` 载入内存 `db_lst`）；建 2 个 **detached** 线程：
- `handle_local_socket`（CAN/GPS 接收线程）
- `handle_remote_mqtt`（MQTT 上行线程）

主循环 `while(keep_running)`：每 1s 调 `handle_fault_data`（`main.c:88-102`）。

> **事实**：`main.c` 里 `handle_can_status` 被注释（`main.c:100`），故 `custom_cb_t cb[]` 回调表整段 `#if 0` 未用；**DM1 空闲超时→自动恢复故障** 的路径（`handle_can_status`→`act_lst` 计时→`dequeue`）**当前不运行**。故障恢复只靠 CAN 报文里的"全 0/全 FF"显式恢复（§5.2）。

> **并发（事实）**：`db_lst`/`act_lst` 用 `pthread_mutex` 保护（`fault.c`、`app_mng.h:45-48`）。但 `freeze_dat` 单份快照被 CAN 线程写、MQTT 线程（`tsp_frame_generate`）读，**无锁**；`history_count`、`db_sync` 跨线程读写无原子保护。

---

## 4. 核心数据结构（`app_mng.h`、`db.h`）

- `app_mng_t`（`app_mng.h:111-136`）：`can_skt_fd[3]`/`gps_skt_fd`、`phone_number[6]`、`lgps_inf`（本地 GPS）、`freeze_dat`（冻结帧快照）、`sqlite3 *db`、`db_lst`（故障状态内存镜像）、`act_lst`（活动 SA 表）、`history_count`。
- `fault_code_t`（sa/spn/fmi，packed，`app_mng.h:77-82`）、`freeze_frame_dat_t`（15 个整车信号，`app_mng.h:84-101`）、`frame_head_t`（0F4A 帧头，`app_mng.h:103-109`）。
- `db_obj_t`（sa/spn/fmi/category/time/first_start）、`db_history_obj_t`（+latitude/longitude/altitude/direction，**latitude/longitude 为 `int`**，`db.h:16-27`）。

---

## 5. 数据管线

### 5.1 接收侧 `handle_local_socket`（`local_socket.c:217-334`）
非阻塞 `connect` + `select` 本机端口：CAN0/1/2 = `16002/16003/16004`、GPS = `16005`（`app_mng.h:29-32`）。收到即 `parse_can_msg`/`parse_gps_msg`。按 `sizeof(can_frame_t)` 切帧。

### 5.2 DM1 故障解析 `parse_dm1_frame`（`local_socket.c:62-134`）
按 J1939 PGN 分派（`frm_pgn=(can_id>>8)&0x03FFFF`，`sa=can_id&0xFF`）：
- `0xECFF`(TP.CM_BAM)/`0xEBFF`(TP.DT) → `bam_frame_parse` 重组多包，完成且 `dt_pgn==0xFECA` 则 `enqueue_new_faults`。
- `0xFECA`(DM1 单包) → 全 0 或 `ff ff ff 7f`（无故障）则 `dequeue_fault_according_sa`（恢复），否则 `enqueue_new_faults`。
- `0xEBEA` → 同 0xFECA 逻辑（偏移 +1）。
- SPN/FMI 解码：`spn=(errCode&0xFFFF)|((errCode&0xE00000)>>5)`、`fmi=(errCode&0x1F0000)>>16`（`fault.c:56-57`）。

### 5.3 冻结帧解析 `parse_can_frame`（`local_socket.c:136-197`）
从 9 个 CAN ID（`0x18FFF101` 车速/瞬时电耗、`0x18FF03A3` 踏板、`0x18F0010B` EBS、`0x18FF7708` 转速、`0x18FF26B6` 冷却液出口/最高温、`0x18FF25B5` 最高电压、`0x18FFF303` 挡位、`0x16FF31C1` 电机扭矩、`0x18FF0966` 总重、`0x19FFF312` 冷却液进口）解析进 `freeze_dat`。

### 5.4 故障状态维护 `fault.c` + `handle_fault_data`
`enqueue_new_faults`/`dequeue_fault_according_sa` 维护 `db_lst`（触发/恢复、去重、`first_start` 首发标记）；`handle_fault_data`（`fault.c:175-262`）把 `db_lst` 快照与 SQLite 对比，`db_insert/update_status` 并在触发/恢复变化时 `db_insert_history`（`history_count++`）。

### 5.5 BAM 重组 `bam_proc.c`
`bam[100]` 按 SA 分槽；`TP.CM_BAM`(0xECFF) 记录 `dt_pgn/pkt_len/frm_cnt` 并 `malloc`，`TP.DT`(0xEBFF) 按帧序拼入，末帧完成返回。

### 5.6 上行侧 `handle_remote_mqtt`（`client_mosquitto.c:93-216`）
连本机 **硬编码 `127.0.0.1:1883`**（`remote_domain/port` 与 `fault_client.ini` 均未实际使用），订阅 `LOCAL_TOPIC_PUB_GET_SCHEMA`/`TOPIC_PUB_EVENT`（消息回调基本空操作）。主循环：连接就绪且 `/tmp/dial_success` 存在时，从 `fault_history` 读 1 行 → `tsp_frame_generate` 造 `0F4A` 帧 → `mosquitto_publish` 到 `LOCAL_TOPIC_PUB_HEAVY` → **无论成败 `db_delete_rows(...,1)` 删该行**（`client_mosquitto.c:194-196`，注释明示"有 1 删 1"）。

### 5.7 `0F4A` 帧构造 `tsp_frame_generate`（`tsp.c:121-234`）
`frame_head(msg_id=htons(0x0F4A), phone_bcd, serial++)` + `remote_gps(version/lat/lon/alt/dir/bcdTime/level2=1)` + `category(1B)` + `bodyLen(1B)` + `freeze_frame_dat_t`（各字段 `htons`）+ `故障数(1B)=1` + `fault_code(sa/spn htonl/fmi)` → 尾附 **BCC 异或校验**（`getBCC`）→ `tsp_cmd_pton` 转义（`7e→7d02`/`7d→7d01`）→ 首尾 `0x7e`。

---

## 6. 运行时外部依赖总表

| 资源 | 用途 | 缺失后果 | 依据 |
|---|---|---|---|
| `/opt/sim_info` `dev:msisdn` | 手机号→device_id BCD | `get_phone_info` 失败→`app_mng_init` 失败→退出 | `common.c:216`、`app_mng.c:62` |
| `/media/sdcard/fault.db` | 故障状态/历史 SQLite | `db_init` 失败→退出 | `app_mng.c:120` |
| `/tmp/dial_success` | 拨号成功标志（上行门控） | 不存在→**不上报**（只采集入库） | `client_mosquitto.c:170,207` |
| `127.0.0.1:16002/3/4` | CAN0/1/2 桥接 | 重连等待 | `local_socket.c:263` |
| `127.0.0.1:16005` | GPS 源 | 重连等待 | `local_socket.c:238` |
| 本机 MQTT broker `127.0.0.1:1883`（硬编码） | 上行出口 | 循环重连 | `client_mosquitto.c:104` |
| `/tmp/fault_status` | `SIGUSR1` 转储（当前 `dump_status` 未接线） | — | `fault.c:264-340`、`app_mng.h:19` |
| `fault_client.ini` | README 述配置，代码中 `FAULT_CFG_PATH` 定义但未见 load | — | `app_mng.h:18` |

---

## 7. 已识别的问题清单（按严重度）

| 编号 | 严重度 | 位置 | 问题 | 触发条件与影响 | 确证/推断 |
|---|---|---|---|---|---|
| F1 | **高** | `tsp.c:148-149` | `htonl(double_to_int_10_6(obj->latitude))`：`obj->latitude` 已是 ×10^6 的 `int`（`fault.c:218` 存库时已缩放），此处**再次 ×10^6** | `0F4A` 上报的经纬度整型**溢出为垃圾值**（现役上行路径） | 确证（`db.h:23` int + `common.c:129` 二次缩放）|
| F2 | **中** | `bam_proc.c:7-30` | `check_bam_index(sa,int*index)` 计算了槽位却**从不写回 `*index`**；`bam_frame_parse` 的 `i` 恒为初值 0 | 所有 SA 的 BAM 多包共用 `bam[0]`，**多控制器并发多包 DM1 会互相串包/覆盖** | 确证 |
| F3 | 中 | `tsp.c:50` | `if (obj->category = FAULT_INVALID)`：`=` 误用为 `==`，赋值并恒假 | 位于 `make_alarm_payload`——该函数因 `cnt` 恒 0（`tsp.c:32` 在 `#if 0`）**恒返回 NULL、当前未被调用**，故暂无害但为潜伏缺陷 | 确证（死代码内）|
| F4 | 中 | `main.c:100`、`local_socket.c:364-398` | `handle_can_status` 未接线 → **DM1 空闲超时自动恢复**路径不运行 | 若某 ECU 停发 DM1 但未发"无故障"帧，其故障**不会因空闲而恢复** | 确证（行为差异，**推断**是否符合预期需业务确认）|
| F5 | 低 | `client_mosquitto.c:185-196` | 上报后无条件 `db_delete_rows`，发布失败也删 | broker 抖动时**故障历史丢失**（注释明示有意"有 1 删 1"） | 确证（设计取舍）|
| F6 | 低 | `local_socket.c:285-290` | `select` 出错仅 `close` `can_skt_fd[0]`/`[1]`，漏 `[2]` 与 `gps_fd` | fd 泄漏/状态不一致（小概率路径） | 确证 |
| F7 | 低 | `common.c:114-126`（`getBCC`） | 每字节 `printf` 校验数据 | 生产日志刷屏 | 确证 |
| F8 | 低 | `freeze_dat` 跨线程无锁 | CAN 线程写、MQTT 线程读同一快照 | 撕裂读（各字段独立、后果轻） | 确证 |
| F9 | 低 | `app_mng.c:87`、`common.c:430,468` | 死代码：`dev_info_init`/`sany_get_file_data`/`set_cloud_status` 三个函数编入却全仓 0 调用（经引用计数=1，仅定义无调用，非 `#if 0`） | 维护干扰 | 确证 |

---

> **修复状态（2026-07-10，working tree 未提交；EC200A 交叉工具链完整编译通过，二进制生成，待真机验证）**
> - **F1 已修复**：`tsp.c:148-149` 去掉重复的 `double_to_int_10_6`，直接 `htonl(obj->latitude/longitude)`（存库时已缩放为 ×10^6 的 int）。
> - **F2 已修复**：`bam_proc.c` `check_bam_index` 命中槽位后写回 `*index`，消除多 SA 的 BAM 共用 `bam[0]` 串包。
> - 其余 F3（死代码内 `=` 赋值）、F4（空闲自动恢复未接线，待业务确认）、F5–F9（低severity/取舍）按范围未改。
> - 注：`bam_proc.c` 编译有既有 `memset/memcpy` 隐式声明告警（在 `bam_frame_parse`，非本次改动的 `check_bam_index`）。

## 8. 端到端数据流

```
[CAN桥接] 16002/3/4 ─┐            [GPS] 16005
                     ▼               │
[local_socket线程] parse_can_msg ────┤
   ├─ parse_dm1_frame: DM1(0xFECA)/BAM(0xECFF+0xEBFF)/0xEBEA
   │     └─ enqueue_new_faults / dequeue(全0/全FF) → db_lst(mutex)
   └─ parse_can_frame: 9个ID → freeze_dat(无锁)
                     │
[主循环 1s] handle_fault_data: db_lst ⇄ SQLite(fault_status/history), history_count++
                     │
[mqtt线程] 等 /tmp/dial_success → 每~2s 取 1 条 history
   └─ tsp_frame_generate: 0F4A(帧头+GPS+冻结帧+故障码+BCC+转义) → publish(LOCAL_TOPIC_PUB_HEAVY) → 删该行
                     ▼
           本机 broker 127.0.0.1:1883 → TSP 平台
```

---

## 9. 需真机/对端才能验证的假设点

1. 桥接 `frame.can_id` 的扩展帧标志与 `can_id & 0x7FFFFFFF` 去标志方式匹配（`local_socket.c:77,143`）。
2. `can_frame_t` 线格式（同族 20 字节结构）与桥接一致。
3. DM1/BAM 的 PGN（0xFECA/0xECFF/0xEBFF/0xEBEA）与整车实际报文一致。
4. `0F4A` 帧头/冻结帧字段顺序与平台权威定义一致（**规范不在仓库**）。
5. 9 个冻结帧 CAN ID 的字节/精度解析与整车约定一致。
6. `/tmp/dial_success` 由拨号进程写入且语义为"可上行"。

---

## 10. 结论性要点（速览）

1. **本质**：DM1 故障 + 冻结帧采集 → SQLite 落库 → `0F4A` 经 MQTT 上行的单向诊断上报进程。
2. **模块布局特殊**：无独立 CAN 模块，CAN/GPS 接收在 `local_socket.c`；`json_parse` 加密但**未编译**。
3. **两处确凿缺陷影响正确性**：F1（0F4A 经纬度二次缩放溢出，现役）、F2（BAM 恒用 slot0，多 SA 串包）。
4. **一处行为待确认**：F4（故障空闲自动恢复未接线）。
5. **上行强依赖 `/tmp/dial_success`**，且"有 1 删 1"策略下发布失败会丢历史（F5）。
6. **构建**：EG25 主 / EC200A 交叉（`sdk.mk` 已本机化）；无测试/linter。
7. **权威规范缺失**：`0F4A` 载荷、冻结帧字段、DM1 PGN 的权威定义均不在仓库（`doc/` 有测试报告与 CAN 导出 csv 供参考）。

---

## 11. 覆盖范围与边界（自评）

### 11.1 已核实覆盖
- **全部已编译的一手代码通读**：`main.c`、`app_mng.c`、`fault.c`、`local_socket.c`、`bam_proc.c`、`tsp.c`、`client_mosquitto.c`、`common.c`（关键函数）、`db.h` 及相关头文件。
- **构建纳入范围**据 `Makefile:22-36` 逐条比对实际目录，明确 4 个通配目录不存在、`json_parse` 未编译。
- **F1 双重缩放**经 `db.h` 字段类型 + `common.c:double_to_int_10_6` 交叉确认。

### 11.2 主动未深审
- `db.c`（577 行 SQLite CRUD）：读了 `db.h` 全部接口与调用点，未逐行核对 SQL 语句。
- 第三方 vendored（cJSON/list/opt_iniparser/mosquitto/uthash/cn-cbor）未逐行审计。
- `src/json_parse/json_parse.c`：加密且**未编译**，未纳入。

### 11.3 仓库内无法闭合的边界
1. `0F4A` 帧/冻结帧/DM1 PGN 的**平台与整车权威定义**（不在仓库）。
2. 桥接线格式、`/tmp/dial_success` 写入方 等仅运行期可证的契约。

---

*（本报告只做静态阅读，未执行目标程序；结论均标 `文件:行`；`json_parse.c` 加密但未编译，未纳入；量化行号可按函数名检索。）*
