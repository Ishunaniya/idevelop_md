# tsp_0f6b_client 仓库全面分析

> 本文基于对仓库源码、Makefile、头文件的逐文件通读/结构化梳理得出，结论均标注依据位置（`文件:行`）。
> 无法从代码直接确证、只能推断处，均显式标注"**推断**"。
> 参照对象：`tsp_0f7b_client/md/analyse/仓库全面分析.md`。**本仓库是 `tsp_0f7b_client` 的父本**——0f7b 是从本仓库分叉出的"轻卡实时精简版"，因此**本仓库中 0f7b 里被 `#if 0` 冻结的 `data_0f6b_t` 全量打包路径是现役在跑的主路径**。共享的"点表→规则树→CAN 解析→打包→TSP 帧→MQTT"管线与 0f7b **同源**，本文对该管线只做要点复述，细节可直接引用 0f7b 分析报告的 §5/§6/§17；本文重点放在 **0f6b 专属能力**（0F9B 高密、0F4D 事件、AEBS/FTP、TPMS、智驾、碰撞、冷却液）与本仓库独有缺陷。
> **本仓库无加密源文件，全部可读。**

---

## 0. 一句话概览

跑在货车 TBOX（Quectel EC200A）上的**新能源重卡"企标"工况数据上报守护进程**：
按点表把整车 CAN 信号解析进内存，周期性打包成 **`0x0F6B` 工况数据**上传 TSP；
并在其之上扩展了 **`0x0F9B` 高密能耗数据**、**`0x0F4D` 事件/统计**（故障码、冷却液、碰撞、智能驾驶、AEBS 文件通知）、**胎压(TPMS)**，以及 **AEBS 高密日志落盘 + FTP 上传**。README 记版本 V13（电池包序号、智能驾驶数据上报）。

---

## 1. 项目定位与运行环境

| 项 | 事实 | 依据 |
|---|---|---|
| 二进制名 | `tsp_0f6b_client` | `Makefile:2` |
| 版本 | `APP_VER=14`（写 `/tmp/tsp_0f6b_client.ver`） | `main.c:16,35-44` |
| 目标硬件 | Quectel EC200A（`sdk.mk`，本机 `/home/tronlong/lyp/SDK/EC200A/...`） | `sdk.mk` |
| 语言 | C99，`-Werror -std=c99 -D_GNU_SOURCE`，`make` 编译+strip | `Makefile:46-49` |
| MQTT client-id | `"tsp_0f6b_client"` | `client_mosquitto.c:75` |
| 点表 | `Pointsheet_info1.json`（工作目录相对，265KB） | `main.c:69` |
| 依赖 | mosquitto、cJSON、iniparser、**libcurl+openssl**（FTP/高密） | `include/`、`ftp_upload.c` |

---

## 2. 构建系统与源码纳入范围

`Makefile`（含 `sdk.mk`）glob（`Makefile:27-39`）：`main.c` + `can_mng, data_process, can_server, common, mosquitto, cJSON, list, opt_iniparser, frame, gps, skt_res, tsp_cmd, ftp_upload`。

**未纳入编译（孤立，与 0f7b 一致）**：`src/scene/`、`src/scene_lib/`、`src/log/`（`grep` 确认 Makefile 无 `find src/scene|scene_lib|log`）。场景机制在本仓库同样是"已解析未接线"（`data_item_t` 有 `scene/interval`，但无人切场景，上传节流靠固定计数器）。

---

## 3. 进程与线程模型

`main()`（`main.c:46`）：单实例 flock（`/tmp/tsp_0f6b_client.pid`）→ `set_app_ver` → `can_mng_new` → `mosquitto_lib_init` → `load_pointsheet_json("Pointsheet_info1.json")` → 读 `/opt/machine_vin` → 建高密目录 `/media/sdcard/{high_data,aebs_data}`、加载既有 `DENSE_log_<VIN>` 进 `aebs_buf`（64000B）→ 建 **4 线程**：
- `start_can_server`（CAN 线程，`can_server.c` 2150 行——CAN 解析 + 工况写入 + AEBS 缓存 + 碰撞/智驾/胎压采集）
- `start_gps_task`（GPS 线程）
- `start_mqtt_task`（MQTT 上行线程）
- `pthread_save_aebs_log`（AEBS/高密 DENSE 日志落盘线程）

主循环（`main.c:138-154`，每 3s）：刷新 `acc_stat`、补读 VIN、周期 `keep_newest_files("aebs_data",50)`。

> **并发（事实）**：CAN 线程写、MQTT 线程读同一 `can_mng`（`high_speed_data`、`data_0f9b`、`error_code_list`、`auto_drive_data`、`crash_status`、`tpms`、点表规则树的活值），**全程无锁**（同 0f7b 家族特征）。

---

## 4. 共享数据管线（与 0f7b 同源，要点复述）

点表 `Pointsheet_info1.json`（`dev→msg→rls→pts` 四层）→ `load_pointsheet_json` 建 `data_rule_t` 规则树（每 CAN 通道一张 uthash 表）→ CAN 线程 `parse_common_msg`/`set_common_data_item` 按位段解析写入叶子 `data_item_t.value_int` → 读侧 `get_0f6b_work_data`/`check_0f6b_data_item` 按信号名回填打包（`data_process.c:439-565`）。**这套函数名/结构与 0f7b 完全一致**；区别是本仓库读侧目标结构是全量 `data_0f6b_t` 且**现役**（`binary_work_data_generate` 在 `data_process.c:565`，**未被 `#if 0`**）。

> 与 0f7b 报告 §20 的差异（**已在本版本修复**）：0f7b 报告指出遗留 0f6b 打包里 `DCM3_DoorOpenSts`/`GLC_value` 漏 `htons`；本仓库这两个字段**均已加 `htons`**（`data_process.c:1736,1790`）——即本仓库是更新、更干净的版本。

---

## 5. MQTT 上行（`client_mosquitto.c: start_mqtt_task`，现役，send-only）

连本机 broker（`tbox_info->local_ip:port`），发布主题 `TSP_TOPIC_PUB_MSG`。主循环约 **1s 一轮**（`usleep(1000000)` + `time_cnt++`），各帧节奏：

| 帧 | cmd_id | 触发 | 内容 | 依据 |
|---|---|---|---|---|
| 工况 | `0x0F6B` | **每 30s**（`time_cnt>=30`） | `binary_work_data_generate(data_0f6b_t, TPMS)` 全量工况 | `client_mosquitto.c:136-149`、`frame.c:15` |
| 事件/统计 | `0x0F4D` | 事件驱动 | 故障码(`error_code_list`)、冷却液(`coolant_status`)、碰撞(`crash_status`)、智驾(`auto_drive_data` 变化时)、AEBS 文件通知 | `client_mosquitto.c:152-289`、`frame.c:168` |
| 高密能耗 | `0x0F9B` | **每 1s，需 `/tmp/energyDataSwitch.flag`** 存在 | `data_0f9b`（高频子集，平台指令开关） | `client_mosquitto.c:227-237`、`frame.c:118` |

帧封装 `make_tsp_0f{6b,9b,4d}_frame` 复用 `src/tsp_cmd`（ROOTCLOUD wire 协议，同 0f7b：头+体+XOR+转义+`0x7e` 定界）。**`make_tsp_0f6b_frame` 的 `body_len` 形参已是 `int`**（`frame.c:15`），不存在 0f7b 报告 §6.3 的 `char` 截断隐患。

> **下行**：可见代码中 MQTT 只连接/发布，未见 `mosquitto_subscribe`/命令解析——**send-only**；`energyDataSwitch.flag` 等开关由**别的进程**（如 `tsp_vin_client` 的 8F42 处理）写入（**推断**）。

---

## 6. AEBS / 高密数据落盘与 FTP 上传

- CAN 线程把高密/AEBS 帧写入 `aebs_cach_data`（`CacheManager` 链表）与 `aebs_buf`；`pthread_save_aebs_log` 线程落盘到 `/media/sdcard/high_data/DENSE_log_<VIN>`。
- 触发 AEBS 事件文件后（`aebs_report_file_flag`）：先发 `0F4D` 文件通知（`msg_id=5` + 文件名），再 `start_ftp_file_upload(...)`（`ftp_upload.c:268`，libcurl FTP），成功则 `remove` 本地文件（`client_mosquitto.c:253-289`）。
- `keep_newest_files(dir,50)`（`ftp_upload.c:243`）：按 mtime 只保留最新 50 个文件，防撑爆 SD 卡。

---

## 7. 运行时外部依赖总表

| 资源 | 用途 | 依据 |
|---|---|---|
| `/opt/conf.ini`/`/opt/conf_ext.ini`/`/opt/sim_info` | 设备 ID/broker/手机号（`can_mng_new`） | `can_mng.c`、`common.c` |
| `Pointsheet_info1.json` | CAN 信号点表 | `main.c:69` |
| `/opt/machine_vin` | 车架号（高密文件命名） | `main.c:76` |
| `/tmp/acc_stat` | ACC 状态 | `main.c:139` |
| `/tmp/energyDataSwitch.flag` | 0F9B 高密上报开关（平台控制） | `client_mosquitto.c:227` |
| `/media/sdcard/high_data`,`/media/sdcard/aebs_data` | 高密/AEBS 落盘 | `main.c:82-100` |
| 本机 MQTT broker | 上行出口 | `client_mosquitto.c:72` |
| FTP 服务器（`ftp_upload` 参数） | 高密文件上传 | `ftp_upload.c:268` |
| `127.0.0.1:16002/3/4`、`16005` | CAN0/1/2、GPS 桥接 | `can_server.c`、`gps.c` |

---

## 8. 已识别的问题清单（按严重度）

| 编号 | 严重度 | 位置 | 问题 | 影响 | 确证/推断 |
|---|---|---|---|---|---|
| B1 | **中** | `client_mosquitto.c:187,203` | 冷却液处理块位于 `for(i=0;i<ERROR_LIST_CNT;i++)` **循环之后**，此时 `i==128`（`ERROR_LIST_CNT`），却执行 `can_mng->error_code_list[i].send_status=1` | **数组越界写**（`error_code_list[128]`，合法索引 0~127），破坏 `can_mng` 相邻内存；且在冷却液处理里改故障码状态语义错误（疑似复制粘贴遗留） | 确证 |
| B2 | 低 | `main.c:102-123` | `aebs_buf=malloc(32*2000)`（64000B），启动时 `strcat` 逐行拼接 `DENSE_log_<VIN>` 文件内容，**无长度上限判断** | 若历史 DENSE 日志累计超 64000 字节 → `strcat` **堆溢出** | 确证（依赖历史文件大小，**推断**日常受 keep_newest 控制不易触发）|
| B3 | 低 | `client_mosquitto.c:334`、`:131` | 断连后 `while(mqtt_status==CONNACK_RECVD)` 退出到外层 `sleep(5)`，但未重连（依赖 `mosquitto_reconnect_delay_set` 自动重连回调置回状态） | 若自动重连不生效则停发 | 确证（**推断**依赖库自动重连）|
| B4 | 低 | 多帧共用 `frame_len`/`tsp_frame` | 各发布块局部 `int frame_len` 与外层 `frame_len` 混用；`make_tsp_*` 失败返回 NULL 时对应 `frame_len` 可能未设 | 边界路径发布垃圾长度（概率低） | 确证 |
| B5 | 信息 | `src/scene`、`src/scene_lib`、`src/log` | 未编译（孤立），场景/上传节流未接线（同 0f7b） | 维护干扰 | 确证 |
| B6 | 信息 | `ftp_upload.c:46,96` | 死代码：`mkdir_recursive`、`usage`（static）编入却全仓 0 调用（引用计数=1，疑 curl FTP 示例残留） | 维护干扰 | 确证 |
| **B7** | **中（本轮彻查新增）** | `can_server.c:1212-1245`（`save_high_speed_data`，活动路径，由 `:2104` 调用） | `char data[128]` 用 `snprintf` 打底（时间+2×`%f` ≈ 46B）后**连续 17 次 `strcat(data, tmp_data)`** 追加 `高速/GPS` 各字段（每个 `"%d,"` 最多 12B），**无总长校验** | 17×最多12B + 46B ≈ 250B **远超 `data[128]` → 栈缓冲区溢出**；即便典型取值（每字段~5B）累计也 >128B，**常态即可越界**，破坏 `save_high_speed_data` 栈帧（含返回地址） | 确证（缓冲 `data[128]`、`strcat` 17 次、`snprintf` 打底均经读码核实）|

> 共享管线的其它运行期假设（桥接扩展帧标志、`can_frame_t` 线格式、点表位段解析、平台 schema 对齐）与 0f7b 报告 §21 一致，此处不重复。

---

> **修复状态（2026-07-10，working tree 未提交；EC200A 交叉工具链 `-Werror -std=c99` 完整编译通过，二进制生成，待真机验证）**
> - **B1 已修复**：`client_mosquitto.c` 删除冷却液两分支中越界且语义错误的 `error_code_list[i].send_status=1`（i 已是循环后 128，越界写）。
> - **B7 已修复**：`can_server.c` `save_high_speed_data` 的 `data[128]`→`data[512]`、`tmp_data[8]`→`[16]`，容纳 27 字段 CSV，消除栈溢出。
> - 其余 B2–B6（低severity/信息）按范围未改。

## 9. 端到端数据流

```
[CAN桥接 16002/3/4]──► [CAN线程 can_server] 点表解析→data_item / high_speed_data / data_0f9b / tpms / auto_drive / crash / AEBS缓存
        │(无锁共享 can_mng)                                    │ pthread_save_aebs_log → /media/sdcard/high_data/DENSE_log_<VIN>
        ▼                                                       
[GPS线程 16005]──► gps_info                                     
        │                                                       
[MQTT线程 每1s]                                                 
   ├─(每30s) binary_work_data_generate(data_0f6b_t+TPMS) → 0x0F6B → publish
   ├─(事件)  error_code/coolant/crash/auto_drive/AEBS文件 → 0x0F4D → publish
   ├─(每1s,若 energyDataSwitch.flag) data_0f9b → 0x0F9B → publish
   └─(AEBS)  0F4D文件通知 + start_ftp_file_upload → 删本地文件
        ▼
   本机 broker(TSP_TOPIC_PUB_MSG) → TSP 平台 ;  FTP 服务器 ← 高密文件
```

---

## 10. 结论性要点（速览）

1. **本质**：新能源重卡企标工况上报进程；在 0f6b 主上报之上叠加 0f9b 高密 + 0f4d 事件 + AEBS/FTP，是 0f7b 的"全量父本"。
2. **共享管线与 0f7b 同源**：点表→规则树→解析→打包→wire 帧，可直接引用 0f7b 报告；本版本已修复 0f7b 报告 §20 指出的 `DCM3_DoorOpenSts/GLC_value` 漏 `htons`、且 `body_len` 已用 `int`。
3. **首要缺陷 B1**：冷却液分支的 `error_code_list[128]` 越界写（确凿），建议优先修。
4. **规模**：`can_server.c`(2150) 与 `data_process.c`(2171) 为两大主体，承载全部 CAN 采集与打包。
5. **孤立模块**：scene/scene_lib/log 未编译（同 0f7b）。
6. **构建**：EC200A 交叉工具链（`sdk.mk` 已本机化）；无测试/linter；无加密文件。
7. **权威规范缺失**：0F6B/0F9B/0F4D 字段权威定义见 `md/doc` 江山企标 xlsx（0f7b 报告 §15 已梳理），代码字段与其对齐需以该表为准。

---

## 11. 覆盖范围与边界（自评）

### 11.1 已核实覆盖
- **入口与上行主流程通读**：`main.c`、`client_mosquitto.c`（全量上报逻辑）、`can_mng.h`（上下文结构）、`frame.c`（三种帧构造）、`ftp_upload.c`（关键函数）。
- **构建纳入范围**据 `Makefile:27-39` 逐条确认，scene/scene_lib/log 未编译、无加密文件已核实。
- **关键交叉验证**：`DCM3_DoorOpenSts/GLC_value` 的 `htons` 存在性、`binary_work_data_generate` 非 `#if 0`、B1 越界的 `i` 作用域，均经 grep/读码确认。

### 11.2 主动未深审（已声明）
- **`can_server.c`(2150) 与 `data_process.c`(2171) 未逐行**：共享管线与 0f7b 同源，已借用 0f7b 报告；本仓库的 AEBS 缓存/碰撞/智驾/胎压采集细节按模块定性，未逐字段核对全部 CAN ID 与位段。
- 第三方 vendored（cJSON/list/iniparser/mosquitto）与 `tsp_cmd`（同 0f7b）未重复审计。

### 11.3 仓库内无法闭合的边界
1. 0F6B/0F9B/0F4D 各字段与平台/整车的**权威定义**（见 `md/doc`，非代码）。
2. `energyDataSwitch.flag` 的写入方、FTP 服务器参数、桥接线格式 等运行期契约。
3. 共享管线的运行期假设点，同 0f7b 报告 §21。

---

*（本报告以静态阅读 + 结构化 grep + 复用同源 0f7b 报告 为方法；结论均标 `文件:行`；两大 2000+ 行文件未逐行、已在 §11 声明；无加密文件；量化行号可按函数名检索。）*
