# tsp_lock_client 仓库全面分析

> 本文基于对仓库源码、Makefile、头文件的逐文件通读得出，所有结论均标注了依据位置（`文件:行`）。
> 无法从代码直接确证、只能推断处，均显式标注"**推断**"。
> 参照对象：`tsp_0f7b_client/md/analyse/仓库全面分析.md` 的格式与纪实性要求。
> **本仓库无加密源文件，全部可读；所有 `src/` 模块均被编译**（无孤立目录）。

---

## 0. 一句话概览

这是一个**跑在货车 TBOX（Quectel EC200A）上的金融/远程锁车守护进程**：
接收平台经本机 MQTT 下发的 `0x8F40` 锁车指令（绑定/解绑/一~三级锁车/解锁/状态查询），
通过 CAN 总线与整车锁车 ECU 做 **MD5 握手鉴权**并下发锁/解锁命令；同时实现
**离线自动锁车**（长期连不上平台则强制锁车）与**基于里程的延迟解锁**校验。README 记版本 V2.10（去掉外发锁车状态报文 `0x18FFECFC`）。

---

## 1. 项目定位与运行环境

| 项 | 事实 | 依据 |
|---|---|---|
| 二进制名 | `tsp_lock_client`（曾名 `lock_client`） | `Makefile:2`、`README.md` |
| 版本 | `2.10`（`MAJOR_VER=2 MINOR_VER=10`，`.ver` 只写主版本 "2"） | `main.c:17-18,22-30` |
| 目标硬件 | Quectel EC200A（`sdk.mk`，本机 `/home/tronlong/lyp/SDK/EC200A/...`） | `Makefile`、`sdk.mk` |
| MQTT client-id | `"tsp_lock_client"` | `client_mosquitto.c:194` |
| 语言 | C，`make` 编译并 `strip` | `Makefile:42-45` |
| 依赖库 | mosquitto、iniparser、cc_slist、md5、CRC16 | `Makefile:23-35` |

---

## 2. 构建系统

- `Makefile` glob 纳入（`Makefile:22-35`）：`main.c` + `src/{lock_mgr, can_server, common, mosquitto, list, opt_iniparser, cmd, skt_res, lock, tsp_cmd, log, posix, md5}`。**所有 `src/` 目录都在编译范围内，无孤立模块。**
- `sdk.mk`（EC200A）导出交叉工具链（已按本机修正，见根 `CLAUDE.md`）。
- 目标：`make`（编译+strip）、`make clean`。无测试/linter。

---

## 3. 进程与线程模型

`main()`（`main.c:43`）：
1. 打印版本、`set_app_ver()` 写 `/tmp/tsp_lock_client.ver`。
2. `sany_debug_func_register(printf, DEBUG)` 装日志后端（`main.c:49`、`log.c`）。
3. `sigignore(SIGPIPE)`（`main.c:51`）。
4. 单实例：`flock(/tmp/tsp_lock_client.pid)`（`main.c:32-41,53`）。
5. `lock_mgr_new()`：建总上下文（见 §4）。失败退出。
6. 读 ACC 状态入 `machine_power_status`（`main.c:63`）。
7. 建 3 线程：`start_can_server`、`start_mqtt_task`、`start_lock_task`（均传 `lock_mgr`）。
8. 主循环：每 3s 刷新 `machine_power_status`（`main.c:70-73`），**不 join**（无限循环）。

> **并发说明（事实）**：`lock_mgr` 被 4 个执行流共享——CAN 线程（写 `lock_status`/`md5_lock_status`、经 `parse_delay_unlock_data` 写 `cmd_type`）、MQTT 网络线程（`mosquitto_loop_start` 起，回调里写 `cmd_type`/`network_state`、操作 `recv_msg_list`）、MQTT `start_mqtt_task` 主循环（也读写 `recv_msg_list`）、锁任务线程（读写 `cmd_type`、读 `lock_status`）。**全程无锁**（未见 mutex），`recv_msg_list` 存在**跨线程竞态**（回调线程 `cc_slist_add` vs 主循环 `cc_slist_get_first/remove_first`，`client_mosquitto.c:159,247,263`）。

---

## 4. 总上下文 `lock_mgr_t`（`lock_mgr.h:13-35`）

```c
typedef struct {
  int can_fd[CAN_CHN_NUM];       // 3 路，实际只用 [0][1]
  int can_rcv_cnt[CAN_CHN_NUM];
  int cmd_fd;                    // 连 16008 的 MCU 命令通道
  char *msi_sdn; char *phone_number; char phone_number_bcd[6];
  local_cfg_t local_cfg;         // 本机 broker ip/port
  CC_SList *recv_msg_list;        // 待处理的下行锁车指令队列
  CC_SList *send_msg_list;        // 声明但基本未用
  uint8_t lock_status;            // 来自 CAN 0x18FF0800 (data[6]>>4)
  uint8_t gps_id[3]; uint8_t fixed_key[3];
  uint8_t cmd_type;               // 当前锁车指令(持久化到 /opt/lock_cmd.ini)
  lock_param_t lock_cmd_param; active_param_t active_cmd_param;
  int machine_power_status;       // ACC
  uint8_t md5_lock_status;        // 0x18FF0800 (data[2]&0x02)>>1
  int network_state;
} lock_mgr_t;
```

`lock_mgr_new()`（`lock_mgr.c:101-134`）：
1. `get_dev_config`：读 `/opt/conf_ext.ini` 的 `mgw:local_ip/port`。**文件缺失即返回 -1 → 进程退出**（`lock_mgr.c:15-33,107-111`）。
2. `get_phone_info` → `get_sim_info`：读 `/opt/conf.ini`（存在性）与 `/opt/sim_info` 的 `dev:msisdn`，归一化 11 位、转 6 字节 BCD。**任一失败 → 退出**（`lock_mgr.c:47-99,113-118`）。
3. `load_lock_cmd`：从 `/opt/lock_cmd.ini` 的 `lock:cmd_type` 恢复上次指令（`lock.c:411-432`）。
4. `lock_status=0xFF`，建 `recv/send_msg_list`。
5. `cmd_init`：连 `16008`（`CMD_SOCKET_ID`）作为 MCU 命令通道（`cmd.c:9-15`、`cmd.h:6`）。

---

## 5. 锁车业务全貌

### 5.1 指令模型（`lock.h:21-31`）
`CMD_BIND(1)/UNBIND(2)/LOCK(3)/UNLOCK(4)/LOCK_LV1(5)/LV2(6)/LV3(7)/REQ_STATUS(0xA)`。

### 5.2 MQTT 下行处理（`client_mosquitto.c`，现役）
- 连本机 broker（`local_cfg.ip:port`），**订阅主题 = `phone_number`**（`client_mosquitto.c:30`），发布主题 `MQTT_TOPIC_CMD_RESP="mqtt_upload_message"`（`client_mosquitto.h:3`）。
- `mosq_message_callback`（`client_mosquitto.c:60-183`）：`tsp_cmd_frame_parse` 解析；仅处理 `cmd_id==0x8F40`；先发 `0x0001` 通用应答；取 `cmd_type=data[1]`；变化则 `save_lock_cmd` 持久化；构造 `finance_lock_msg` 按指令类型拷 `active_param`/`lock_param`；非 `REQ_STATUS` 入 `recv_msg_list`，`REQ_STATUS` 立即回 `0x8F40` 状态应答（`get_result_from_lock_status`）。
- `start_mqtt_task` 主循环（`client_mosquitto.c:245-315`）：队列中 `finance_lock_msg` **延迟 10s** 后回 `0x8F40` 应答并出队（给锁任务留执行时间）；开机时若锁状态变化主动上报一次。

### 5.3 锁任务状态机（`lock.c: start_lock_task`，现役，`lock.c:896-1058`）
`while(1)`（1s）当 `machine_power_status==1`（ACC on）时按 `cmd_type` 执行：
- `CMD_BIND/UNBIND` → `send_bind_to_ecu`（`MSG_BIND_REQ 0x18FFD4FC`，带 `gps_id`+`solid_passwd`）
- `CMD_LOCK/LV1/LV2/LV3` → `send_lock_to_ecu(LOCK, level)`（`MSG_LOCK_REQ 0x18FFD6FC`，`data[0..1]` 为锁级对应"值" 6400/8000/10400）
- `CMD_UNLOCK` → `send_lock_to_ecu(UNLOCK)`（值 28000）
- **网络/离线判定**：每 20s 查 `/tmp/network_status`、`/tmp/clound_status`、`/tmp/dial_success` 与 `network_state`；在线则清零离线计时并处理 `force_lock.flag`；离线且锁状态为"激活"(1/6)时按 2 分钟累加 `offline_min` 存 `/opt/network_offline.time`。
- **离线自动锁车**：`offline_min >= left_lock_min`（默认 `DEFAULT_OFFLINE_LOCK_MIN=30*24*60`）→ 强制 `CMD_LOCK_LV3` 并 `touch /opt/force_lock.flag`（`lock.c:1040-1052`、`lock.c:27,31`）。

### 5.4 CAN 接收与握手（`can_server.c: can_msg_process`）
- `MSG_STATUS 0x18FF0800` → `lock_status=data[6]>>4`、`md5_lock_status=(data[2]&0x02)>>1`（`can_server.c:218-220`）。
- `MSG_HANDSHAKE_REQ 0x18FFCF00`（ECU 发 seed）→ 延后 100ms 调 `respond_handshake_to_ecu`（`can_server.c:221-233`）。
- `parse_delay_unlock_data`（`can_server.c:65-159`）：`0x18FEC1EE` 取里程、`0x1AD60E28` 用里程/gps_id/天数做多重校验，命中则下发延迟解锁并回 `0x1ACDEB4A`（`data[2]=0x10/0x20`）。

### 5.5 安全/鉴权（`lock.c`）
- `solid_passwd = CRC16(/opt/conf.ini dev:secret)`（`lock.c:204-219`）。
- `gps_id = CRC16(/opt/conf.ini dev:id)`（`lock.c:233-249`）。
- 握手应答 `respond_handshake_to_ecu`（`lock.c:309-347`）：`data = [solid_passwd(2), 0, seed(5)]` → `MD5CalculateDigest` → 取前 8 字节经 `MSG_HANDSHAKE_RSP 0x18FFD5FC` 回 ECU（16 字节 MD5 只发前 8）。

---

## 6. TSP 帧协议（`src/tsp_cmd/tsp-cmd.c`）
与 `tsp_0f7b`/`tsp_vin` **同源**（`ROOTCLOUD/irootech` 版权头，`tsp-cmd.h:1-14`）：cmd_attr 位域 `msg_len:10/encrypt:3/split:1/res:2`，帧 = 头+体+XOR 校验+转义(`7e/7d`)+首尾 `0x7e`。`tsp_cmd_frame_parse` 用于解析下行 `0x8F40`。锁车专用应答在 `lock.c`：`tsp_cmd_8F40_resp_make`/`_with_finance`/`tsp_cmd_0F40_lock_status_make`（cmd_id `0x0F40`，`lock.c:602-736`）。

`get_result_from_lock_status`（`lock.c:558-600`）把 `lock_status 0x00~0x0A` 映射为 `0xE0~0xEA` 回给平台。

---

## 7. 运行时外部文件/端口依赖总表

| 资源 | 用途 | 缺失后果 | 依据 |
|---|---|---|---|
| `/opt/conf_ext.ini` `mgw:*` | 本机 broker 地址 | **进程退出** | `lock_mgr.c:15-33` |
| `/opt/conf.ini` `dev:id`/`dev:secret` | gps_id/鉴权密码源(CRC16) | 存在性缺失→退出；secret 缺失→passwd=0xFFFF | `lock_mgr.c:66`、`lock.c:206,235` |
| `/opt/sim_info` `dev:msisdn` | 手机号→订阅主题/device_id | **进程退出** | `lock_mgr.c:47-63` |
| `/opt/lock_cmd.ini` `lock:cmd_type` | 持久化当前锁指令 | 首次为 CMD_NO_ACTION | `lock.c:411-463` |
| `/opt/network_offline.time` | 累计离线分钟 | 从 0 起算 | `lock.c:56-88` |
| `/opt/left_lock_time.time` | 剩余锁车倒计时(分) | 默认 30 天 | `lock.c:90-122` |
| `/opt/force_lock.flag` | 强制锁车标志 | — | `lock.c:1014,1046` |
| `/tmp/acc_stat` | ACC 状态 | 视为下电（不执行锁逻辑） | `common.c:161-179` |
| `/tmp/network_status`,`/tmp/clound_status`,`/tmp/dial_success` | 在线判定 | 判为离线（推进离线计时） | `lock.c:987-1003` |
| `/media/sdcard/lock.log` | 锁车事件日志(1M 备份) | — | `can_server.c:34-63` |
| `127.0.0.1:16002/16003` | CAN0/1 桥接 | 重连等待 | `can_server.c:245-250` |
| `127.0.0.1:16008` | MCU 命令通道 | `cmd_fd<0`（`send_cmd_msg` 失败） | `cmd.c:9-15` |
| 本机 MQTT broker | 下行指令入口/上行应答 | 循环重连 | `client_mosquitto.c:215-224` |

---

## 8. 端到端数据流

```
[平台] ──MQTT(主题=phone_number)──► [本机broker] ──► mosq_message_callback
     解析 0x8F40 → cmd_type(data[1]) → save /opt/lock_cmd.ini → finance_lock_msg 入 recv_msg_list
                                          │(无锁共享)                      │
                                          ▼                                ▼(延迟10s)
                          [锁任务线程] 按 cmd_type            [MQTT主循环] 回 0x8F40 应答 → broker → 平台
                          send_bind/lock/unlock_to_ecu ──CAN──► 锁车ECU
                                          ▲ 读 lock_status
[锁车ECU] ──0x18FF0800 状态──► [CAN线程] lock_status/md5_lock_status
          ──0x18FFCF00 seed──► respond_handshake(MD5(CRC16(secret)+seed)) ──0x18FFD5FC──► ECU
          ──0x18FEC1EE 里程 / 0x1AD60E28 校验──► parse_delay_unlock_data ──0x1ACDEB4A──► ECU

[离线自愈] 无网累计 offline_min ≥ 30天 → 强制 CMD_LOCK_LV3 + force_lock.flag
```

---

## 9. 已识别的问题清单（按严重度）

| 编号 | 严重度 | 位置 | 问题 | 触发条件与影响 | 确证/推断 |
|---|---|---|---|---|---|
| L1 | **高** | `client_mosquitto.c:159,247,263` | `recv_msg_list` 在 mosquitto 网络线程（回调 `cc_slist_add`）与 `start_mqtt_task` 主循环（`get_first/remove_first`）间**无锁并发** | 指令并发到达时链表撕裂/漏处理/崩溃 | 确证（竞态窗口存在，实际概率取决于下发频率）|
| L2 | 中 | `lock.c:558-659`（`tsp_cmd_8F40_resp_format` case 10）+ `lock.c:19-20` | `s_gps_id`/`s_fixed_key` 为 static `[3]` **从未赋值**，`REQ_STATUS` 应答里的 gps_id/fixed_key 恒为 0 | 平台收到的状态查询响应缺失真实 gps_id/key | 确证 |
| L3 | 中 | `client_mosquitto.c:252-259` | `tsp_cmd_8F40_resp_make_with_finance` 可能返回 NULL 时 `frame_len` 未被设置即用于 `mosquitto_publish` | 极少数构造失败路径→发出垃圾长度/空指针 | 确证（构造失败才触发）|
| L4 | 中 | `lock.c:995-1003`（`start_lock_task`） | 离线计时/自动锁车整段仅在 `machine_power_status==1`（ACC on）内执行 | 停车下电期间**离线时间不累积**，30 天离线锁车仅在通电时推进 | 确证（是否符合预期需业务确认，**推断**为有意）|
| L5 | 低 | `can_server.c:98` | `("printfcheck_1 and check2 ok\n");` 是无效表达式语句（`printf` 被误写入字符串） | 该日志永远不打印 | 确证（仅日志缺失）|
| L6 | 低 | `can_server.c:249,175`、`can_server.h:8` | `CAN_CHN_NUM=3` 但只开/轮询 2 路，`can_fd[2]` 恒未建立 | `parse_delay_unlock_data` 中 `can_idx` 永不为 2 | 确证（CAN2 未启用）|
| L7 | 低 | `lock.c:953-957`（LOCK 分级） | `if(LV1) else if(LV2); if(LV3)`——第二个 `if` 非 `else if`；`CMD_LOCK(3)` 时 `level` 保持初值 3 | 逻辑冗余，当前结果正确 | 确证 |
| L8 | 低 | `lock.c` | 大量死代码：`#if 0` 旧 `start_lock_task`、旧 `save_log`/`get_string_time_str`、`send_lock_to_display`（`0x18FFECFC`，V2.10 已停用）、`#if 0 send_msg_to_can/send_bind_to_ecu`；**另有两个活动编译区内定义却从不被调用的函数：`send_new_bind_to_ecu`（`lock.c:283`）、`send_no_action_to_ecu`（`lock.c:491`，static）**（经全仓引用计数=1，即仅定义无调用；非 `#if 0` 包裹） | 维护干扰 | 确证 |

---

> **修复状态（2026-07-10，working tree 未提交；EC200A 交叉工具链完整编译通过，二进制生成，待真机验证）**
> - **L1 已修复**：`client_mosquitto.c` 新增文件级互斥量 `s_recv_list_mutex`，对 `recv_msg_list` 的 add/size/get_first/remove_first 逐调用加锁。
> - **L2 已修复**：`lock.c` `tsp_cmd_8F40_resp_format` 用真实 `get_gps_id()`/`get_solid_passwd()`（与绑定报文一致字节序）填充 gps_id/fixed_key，替换恒零（确切 3 字节编码仍待对规范）。
> - **L3 已修复**：主循环 `frame_len` 先清零 + `reply_payload` 判空 + 构造失败丢弃防卡队列 + 补 `free(reply_payload)` 防泄漏。
> - 其余 L4（离线计时依赖 ACC，设计取舍待业务确认）、L5–L8（低severity/死代码）按范围未改。

## 10. 需真机/对端才能验证的假设点

1. 桥接 `frame.can_id` **带扩展帧标志**（采集用 `MSG_STATUS|CAN_EFF_FLAG` 比较，`can_server.c:218`）。
2. `can_frame_t`（20 字节，同 §tsp_vin 结构）与桥接线格式一致。
3. 端口 16002/16003↔CAN0/1、16008↔MCU 命令通道 映射正确。
4. 锁车 ECU 的 MD5 握手算法（`MD5(CRC16(secret)[2] + 0 + seed[5])`，取前 8 字节）与 ECU 端一致——**决定鉴权成败**。
5. `send_lock_to_ecu` 的锁级数值（6400/8000/10400/28000）语义与 ECU 约定一致。
6. 平台下发 `0x8F40` 载荷布局（`data[0]=protocol_type`、`data[1]=cmd_type`、`data[2..]=lock/active_param`）与 `client_mosquitto.c:132-152` 解析一致。
7. `parse_delay_unlock_data` 的里程/天数校验算法与平台"延迟解锁"下发算法一致。

---

## 11. 结论性要点（速览）

1. **本质**：金融/远程锁车守护进程——MQTT 下行 `0x8F40` 指令 → CAN 锁车 ECU（MD5 握手），含离线自动锁车与延迟解锁。方向以下行控制为主。
2. **全部源码可读、全部编译**：无加密文件、无孤立目录（与 `tsp_vin` 的加密 MQTT + 未编译 `src/tsp` 形成对比）。
3. **安全强相关**：鉴权靠 `CRC16(secret)` + `MD5(seed)`，密钥源在 `/opt/conf.ini`；离线自愈锁车是防拆核心。
4. **首要缺陷**：L1（`recv_msg_list` 无锁竞态）、L2（状态查询回传恒零 gps_id/key）、L4（离线计时依赖 ACC 通电）。
5. **构建**：EC200A 交叉工具链（已本机化），全模块编译，`make` 带 strip；无测试/linter。
6. **权威规范缺失**：`0x8F40` 载荷、各锁车 CAN 帧字段、延迟解锁算法的权威定义**不在仓库**，本文相关描述均源自 C 代码。

---

## 12. 覆盖范围与边界（自评）

### 12.1 已核实覆盖
- **全部一手代码通读**：`main.c`、`lock_mgr.c`、`lock.c`、`can_server.c`、`client_mosquitto.c`、`cmd.c`、`common.c`、`tsp-cmd.c`、`sany_platform_posix.c`、`log.c`、`CRC16.c` 及对应头文件。
- **构建纳入范围**据 `Makefile:22-35` 逐条确认，全部 `src/` 模块编译、无孤立。
- **无加密文件**（`file` 判定，全仓库 `.c/.h` 均为文本）。

### 12.2 主动未深审
- 第三方 vendored 库：`md5`（md5.c/md5Helper.c）、`CRC16`、`list`(cc_slist)、`opt_iniparser`、`mosquitto` 仅确认接口用法，未逐行审计算法正确性。
- `sany_platform_posix.c`（端序/时间/socket 抽象）：读了函数清单，实际被引用点未逐一追踪。

### 12.3 仓库内无法闭合的边界
1. MD5 握手/锁级数值/延迟解锁校验的**对端 ECU 算法**（决定功能成败）。
2. `0x8F40` 载荷与各 CAN 帧字段的**平台/整车权威定义**（不在仓库）。
3. 仅运行期可证的行为：桥接线格式、broker 转发、离线判定文件的真实写入方——静态分析给不了定论。

---

*（本报告只做静态阅读，未执行目标程序；结论均标 `文件:行`；`can_frame_t` 大小按结构布局为 20 字节，与 `tsp_vin_client` 同型；量化行号可按函数名检索。）*
