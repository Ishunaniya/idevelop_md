# component_ota_for_sanytruck 仓库全面分析

> 本文基于对仓库源码、Makefile、头文件的逐文件通读/结构化梳理得出，结论均标注依据位置（`文件:行`）。
> 无法从代码直接确证、只能推断处，均显式标注"**推断**"。
> 参照对象：`tsp_0f7b_client/md/analyse/仓库全面分析.md`。
> **本仓库无加密源文件，全部可读。** 这是本工作区中**规模最大、最复杂**的仓库（自研 C 约 7100 行，含完整 UDS 诊断刷写栈）。

---

## 0. 一句话概览

跑在货车 TBOX（Quectel EG25 / EC200A）上的**整车 ECU 空中升级（OTA）刷写守护进程**：
接收三一重卡 TSP 平台经本机 MQTT 下发的 OTA 任务（RC40 JSON：升级包 URL/MD5/大小/密钥），
用 curl 下载并 `unzip` 升级包，解析出各控制器的固件文件，然后按 **UDS（ISO 14229）诊断协议**
经 CAN 总线对目标 ECU（VCU/BMS/MCU/TCU/DCDC/DCU/…共 22 种类型）执行"预编程→擦除→下载→校验→复位"刷写，
并把刷写进度/状态**同时回报给仪表（CAN）与 TSP 平台（MQTT）**。README 记版本 V1.13。

---

## 1. 项目定位与运行环境

| 项 | 事实 | 依据 |
|---|---|---|
| 二进制名 | EG25：`component_ota_client_eg25`；EC200A：`component_ota_client`；测试：`component_ota_client_test` | `Makefile`、`Makefile_EC200A`、`Makefile_EC200A_test` |
| 版本 | V1.13（`MAIN_VERSION.SUB_VERSION`，写 `/tmp/component_ota_client.ver`） | `README.md`、`main.c:28-42` |
| 目标硬件 | EG25-G（`arm-oe-linux-gnueabi-gcc`，主 `Makefile`）/ EC200A（`sdk.mk`） | `Makefile:8`、`Makefile_EC200A` |
| 关键编译宏 | `-DUDS_ENV_CHECK_BYPASS`：跳过刷写前环境检查并启用本地 `ota_task.ini` 升级 | `Makefile:119`、`main.c:81` |
| OTA 工作目录 | `/usrdata/uds_ota`（下载）、`/usrdata/uds_ota/ota_file`（解压） | `rc40_ota_zip_file_info.h:5-7` |
| 状态 CAN ID | `0x98FF11FC`（发仪表） | `ota_status.h:5` |

---

## 2. 构建系统

三份 Makefile：
- **`Makefile`（EG25）**：`arm-oe-linux-gnueabi-gcc`，输出 `component_ota_client_eg25`，`all:` 带 `-DUDS_ENV_CHECK_BYPASS`。依赖 `QL_SDK_PATH` 与 `SDKTARGETSYSROOT`（需 source EG25 交叉环境）。
- **`Makefile_EC200A`**（含 `sdk.mk`）：输出 `component_ota_client`，`strip`。
- **`Makefile_EC200A_test`**：输出 `component_ota_client_test`（带 bypass）。

源码 glob（主 `Makefile:54-78`）：`main.c` + `thirdparty/{json,opt_iniparser,mosquitto,cc_deque}` + `src/{sany_syslog, can, sany_open_sdk, common, skt_res, uds, hex2bin, s192bin, conf, cloud_dev, rc40_ota_zip_file_info, rc40_ota_status_reply, curl_ota_file, parse_ota_zip_file, ota_file_info, ota_task_info, vcu_uds_crc, ota_status, ota_env_check}`——**所有自研模块均编译，无孤立目录**。链接含 curl/openssl（下载+MD5）、mosquitto。

---

## 3. 进程与线程模型

`main()`（`main.c:67`）：`log_init`（`log_cfg.ini` 决定日志级别/端口）→ `commponent_ota_mng_new()` → 建 **MQTT 线程 `mosq_client_task`**（`thirdparty/mosquitto/client_mosquitto.c`，接收 OTA 任务）→ 主循环。

主循环（`main.c:101-172`）：轮询 `task_receive_flag`：
1. 有任务且 `task_begin==0` → `ota_task_info_new`（从 `p_ota_zip_file_info` 建任务）。
2. `ota_task_info_load`（`unzip` 升级包 + `pase_ota_zip_file` 解析 + `uds_new`，`ota_task_info.c:15-40`）。
3. `uds_start_update_task(...，ota_comp_type | (can_chn<<...))` 启动 UDS 刷写线程。
4. 成功 `set_ota_status(ota_st_updating)`；失败 `ota_status_reply(...cancel)` 并释放。
5. 刷写非 `updating` 时清 `task_receive_flag`。

若定义 `UDS_ENV_CHECK_BYPASS`：启动时先从 `ota_task.ini` 加载本地升级任务（`main.c:81-97`）。

> **并发/退出（事实）**：UDS 刷写在独立线程内以状态机运行；**刷写彻底失败时整个进程 `exit(1)`**（`uds.c:704`，先经 socket 16008 通知 MCU"升级结束"），刷写成功则线程退出、状态线程续发状态最长 7 天。符合 README V1.08"刷完退出进程"。

---

## 4. OTA 任务接收与升级包处理

### 4.1 任务下发（MQTT → `rc40_ota_zip_file_info.c`）
平台下发 RC40 OTA JSON，`parse_ota_payload`（`rc40_ota_zip_file_info.c:37-113`）解析出 `ota_zip_file_info_t`：`url / md5 / file_name / file_size / key / file_type / msg_id`。

### 4.2 下载（`curl_ota_file.c`）
按 `url` 用 libcurl 下载升级包到 `OTA_FILE_SAVE_PATH`；带下载进度百分比（`dl_percentage`）。

### 4.3 解压与解析（`ota_task_info.c` + `parse_ota_zip_file.c`）
`system("unzip -o ... -d /usrdata/uds_ota/ota_file")` → `pase_ota_zip_file` 遍历解压目录，为每个控制器固件建 `ota_file_info_t` 入 `p_ota_files_queue`（`cc_deque` 队列）。

### 4.4 固件格式转换（`hex2bin/` `s192bin/` `vcu_uds_crc/`）
- `hex2bin.c`/`binary_file.c`：Intel HEX → bin。
- `s192bin.c`：S19（Motorola S-record）→ bin。
- `vcu_uds_crc.c`：刷写块 CRC 计算（VCU 校验）。

---

## 5. UDS 刷写栈（本仓库核心，`src/uds/`）

### 5.1 设备类型（`uds_dev_info.h:134-155`）
22 种控制器：`VCU(0)/BMS_KL(1)/MCU(2)/TCU(3)/EPS(4)/APC(5)/PDU(6)/DCDC(7)/IT(8)/BMS(9)/TCU_ST(10 索特)/BMS_LN(11 锂能)/HT_BCM(12 自研车身)/PEPS(13)/ESCL(14)/YK_VCU(15 优控)/BMS_GT(16 高泰)/MCU_LHT(17 轻卡)/DCU(18)/XCU(19)/TDC(20)/TDC2(21)`。

### 5.2 策略分派（`uds.c: uds_start_update_task`）
按 `dev_type` 为 `uds_mng` 装配三个函数指针（`uds.c:207-246`）：`f_uds_pre_program` / `f_uds_program` / `f_uds_after_program`。当前多数类型走通用实现 `uds_*_for_yk_vcu`（`uds_for_yk_vcu.c`），个别（DCU/XCU/YK_VCU）有专门分支。同时按类型选择 CAN 收发 ID（功能寻址 `*_FUN_TX_ID` / 物理寻址 `*_PHY_TX_ID`，`uds.c:710-741`）。

### 5.3 刷写状态机（`uds.c: uds_server`，`uds.c:670-849`）
`uds_program_step`：`none → init → connect → pre → program → after → del_file → suc`（另有 `read_info`/`error`）。
- **init**：重试控制；失败时对多类控制器**连发 20~30 次扩展会话帧**（`0x10 0x83`）促使 ECU 复位重进刷写（`uds.c:745-761`）；重试耗尽 → `ota_status_reply(...14)` + `set_ota_status(failed)` + 通知 MCU + `exit(1)`。
- **connect**：`uds_connect`（扩展诊断会话；DCU 的 `10 83` 不需应答即进下一步）。
- **pre/program/after**：调对应函数指针，内部完成 安全访问(0x27)、指纹写入、请求下载(0x34)、块传输(0x36)、传输退出(0x37)、擦除例程(0x31 + erase RID)、DID 读写(0x22/0x2E)、复位(0x11) 等（SID 定义见 `uds_pkt_send.c:408-448`）。
- **del_file**：`delet_dir_file` 清升级临时目录 → `suc`。

### 5.4 UDS 报文收发（`uds_pkt_send.c`，ISO-TP）
`cur_uds_single_frame_send` 发单帧；多帧走 ISO-TP（首帧/连续帧/流控）。发送后按 `UDS_SID_CONFIRM(sid)`（`sid|0x40`）校验肯定响应，`0x36` 块传输带序号与重试（`uds_pkt_send.c:294-350`）。

### 5.5 下载管理（`uds_download.c`）
`uds_request_download`（0x34）协商块大小；`uds_download_mng` 跟踪 `cur_uds_updated_file_size / total_uds_update_file_size / uds_update_progress`（0~100，供状态回报）。

### 5.6 环境检查（`ota_env_check.c`）
刷写前检查升级条件（如车辆状态/VCU 许可 `waiting_vcu_ota_permit`）；`-DUDS_ENV_CHECK_BYPASS` 可跳过。

---

## 6. 状态回报（双路）

### 6.1 发仪表（CAN，`ota_status.c: ota_status_send_task`）
每 500ms 发 `0x98FF11FC`：`data[0]=ota_st`、`data[1]=dev_typ`、`data[2]=进度`、`data[6]=包序`、`data[7]=校验(前7字节和 XOR 0xFF)`。成功/失败后最长续发 7 天（`ota_st_send_count=1209600`）。对 TDC/TCU_ST 额外发唤醒帧（`0x98ff12fc`/`0x98FF04A3`）。

### 6.2 回 TSP（MQTT，`rc40_ota_status_reply.c: ota_status_reply`）
把 `msg_id/version/进度/状态码` 打包回平台。状态枚举含"允许/不允许刷写等待、刷写超次数失败、VCU 不回应失败"等（README V1.05）。

---

## 7. 运行时外部依赖总表

| 资源 | 用途 | 依据 |
|---|---|---|
| 本机 MQTT broker | 接收 OTA 任务 / 回报状态 | `main.c:78`、`thirdparty/mosquitto/client_mosquitto.c` |
| `ota_task.ini` | 本地升级任务（bypass 模式） | `main.c:84` |
| `/usrdata/uds_ota[/ota_file]` | 升级包下载/解压目录 | `rc40_ota_zip_file_info.h:5-7` |
| `log_cfg.ini` | 日志级别/端口 | `main.c:49`、根目录 `log_cfg.ini` |
| `127.0.0.1:16008` | MCU 命令通道（通知"升级开始/结束"） | `uds.c:664-668,697-702` |
| CAN（经 `can.c`/`skt_res`） | UDS 收发 + 状态帧 | `can.c`、`ota_status.c` |
| curl 目标 `url` | 升级包源 | `curl_ota_file.c` |

---

## 8. 已识别的问题清单（按严重度）

| 编号 | 严重度 | 位置 | 问题 | 影响 | 确证/推断 |
|---|---|---|---|---|---|
| C1 | 中 | `main.c:88` vs `main.c:122` | 同一 `dev_type` 参数：本地升级路径用 `can_chn<<0x08`，MQTT 路径用 `can_chn<<0x04`，**移位位数不一致** | 两条路径对 CAN 通道的编码不同，其一与 `uds_start_update_task` 内解码不符则通道选错 | 确证（需对 `uds_start_update_task` 解码方核对哪条正确）|
| C2 | 中 | `ota_task_info.c:97-104`（`ota_task_info_destory`） | `ota_task_info_release(p)` 内已 `free(p)`（`:62`），返回后又 `free(p)`（`:102`） | **double free**——但经全仓核实 `ota_task_info_destory` 仅在 `.c:97` 定义、`.h:23` 声明，**全仓 0 调用**，故当前潜伏不触发（属 C7 死代码之一） | 确证（缺陷确凿；当前因函数未被调用而不触发，已由引用计数证实）|
| C3 | 中 | `main.c:104-159` | 多条失败路径 `free(p_ota_zip_file_info)` 但 `p_ota_task_info` 的释放注释为"在哪里释放？"，部分路径未释放 | OTA 任务反复失败时 `ota_task_info_t`/`uds_mng` 内存泄漏 | 确证（源码注释自承不确定）|
| C4 | 低 | `main.c:37` `set_ota_client_version` | `open(..., O_WRONLY|O_CREAT)` **缺 mode 参数** | 新建 `.ver` 文件权限为栈上不定值 | 确证 |
| C5 | 低 | `uds.c` | `static int do_again`/`static int do_again=0` 等函数内 static 状态跨任务保留 | 单次刷写后进程退出，故当前无害；若改为常驻多任务将残留 | 确证（**推断**当前无害）|
| C6 | 低 | `ota_status.c:102-122` | 唤醒帧 `tdc_frame`/`tcu_frame` 的 `data[2..7]` 未初始化即 `write` | 发出的唤醒帧尾部含栈垃圾 | 确证（ECU 是否在意未知，**推断**只看前 2 字节）|
| C7 | 低 | `ota_task_info.c:66,97`、`rc40_ota_zip_file_info.c:205` | 死代码：内存释放/销毁函数 `ota_task_info_table_release`、`ota_task_info_destory`（含 C2 的 double free）、`ota_zip_file_info_destroy` 均编入却全仓 0 调用 | 维护干扰；C2 的缺陷因此不触发 | 确证（引用计数=1，仅定义）|
| **C8** | **中（本轮彻查新增）** | `uds_update_file.c:83`、`parse_ota_zip_file.c:28`、`rc40_ota_zip_file_info.c:149` | OTA 包内/平台下发的 `file_name`（最长 `NAME_MAX`≈255）经无界 `sprintf(buf,"%s/%s",前缀,file_name)` 写入 `file_path[NAME_MAX]`/`shellCmd[256]`：前缀 `OTA_FILE_UNZIP_PATH="/usrdata/uds_ota/ota_file"`(25B)+`/`+255B **可达 281B > 255B** | 长文件名 → **栈缓冲区溢出**；`rc40:149` 更把 `file_name` 直接拼进 `system("cp %s ...")` → 兼具**命令注入**面（平台半可信，故列中） | 确证（`NAME_MAX` 缓冲 + 无界 `sprintf` + 前缀长度均核实）|
| **C9** | **低（本轮彻查新增）** | `parse_ota_zip_file.c:43` | `memcpy(p_ota_file_info->file_name, p_file_name, strlen(p_file_name))`：拷贝长度取源串长、**无 `≤ NAME_MAX` 上界**，且拷 `strlen` 字节未补 `\0` | 源文件名 ≥255B 时溢出 `file_name[NAME_MAX]`；即便不溢出，若目标缓冲非全零则缺终止符 | 确证 |

---

> **修复状态（2026-07-10，working tree 未提交；EC200A 交叉工具链 `-Werror` 完整编译通过，二进制生成，待真机验证）**
> - **C1 已修复**：`main.c:122` `<<0x04`→`<<0x08`，与解码 `(dev_type>>0x08)&0x0F` 及本地升级路径一致，消除 `can_chn!=0` 时对 `uds_dev_type` 的位污染。（注：`uint8_t` 形参截断致"通道恒 can1"是更深层问题，改它需真机验证 CAN 总线，未擅动。）
> - **C3 部分修复**：**load-fail 路径**安全释放 `p_ota_task_info`（此时未创建线程），并规避原 NULL `p_ota_status` 解引用崩溃；**start-fail 路径保持原样不释放**——因 `uds.c:279` 已无条件创建持 `p_ota_task_info` 的状态线程（运行≤7天），释放会 use-after-free，泄漏是必要取舍。
> - **C8/C9 已修复**：`uds_update_file.c` / `parse_ota_zip_file.c` / `rc40_ota_zip_file_info.c` 的文件名 `sprintf`→`snprintf` 收界、`memcpy`→带上界拷贝、`p_file_name` 判空。
> - 其余 C2（函数 0 调用不触发）、C4–C7（低severity/死代码）按范围未改。

## 9. 端到端数据流

```
[TSP平台] ──MQTT RC40任务JSON──► [mosq_client_task] parse_ota_payload → ota_zip_file_info(url/md5/key/msg_id)
                                          │ task_receive_flag=1
                                          ▼
[主循环] ota_task_info_new → ota_task_info_load
   ├─ curl 下载升级包 → /usrdata/uds_ota
   ├─ unzip → /usrdata/uds_ota/ota_file
   ├─ pase_ota_zip_file → ota_file_info 队列 (hex2bin/s192bin/vcu_uds_crc 转换)
   └─ uds_new: 按 dev_type 装 f_uds_pre/program/after 指针 + CAN 收发ID
                                          ▼
[UDS刷写线程] uds_server 状态机:
   init(重试/连发会话帧) → connect(0x10 83) → pre → program(0x27安全/0x34下载/0x36块传/0x37退出/0x31擦除) → after(0x11复位) → del_file → suc
        │ 进度 uds_update_progress                    │ 失败 → exit(1) + 通知MCU(16008)
        ▼                                             ▼
[ota_status_send_task] CAN 0x98FF11FC 发仪表(每500ms)   [ota_status_reply] MQTT 回TSP(状态码)
```

---

## 10. 需真机/对端才能验证的假设点

1. C1 的通道位移哪条正确，取决于 `uds_start_update_task` 对 `dev_type` 高位的解码（需对照 `uds.c:174-320` 解码逻辑与真机通道映射）。
2. 各控制器的 UDS 收发 CAN ID（`*_FUN_TX_ID`/`*_PHY_TX_ID`）、安全访问算法、擦除/下载例程参数 与真实 ECU 一致——**决定刷写成败**。
3. Intel HEX / S19 解析、`vcu_uds_crc` 校验算法 与固件打包侧一致。
4. `/usrdata` 可写、`unzip`/`curl`/`sync` 命令在目标存在。
5. VCU OTA 许可帧（`waiting_vcu_ota_permit`）、TDC/TCU 唤醒帧 与整车约定一致。

---

## 11. 结论性要点（速览）

1. **本质**：TSP→CAN 的整车 ECU OTA 刷写守护进程，完整实现 UDS 诊断刷写栈（22 种控制器，策略化分派）。
2. **规模与复杂度最高**：OTA 收包/下载/解压/格式转换/UDS 状态机/双路状态回报 六大子系统。
3. **失败即退出进程**（`exit(1)`），刷写为一次性任务；状态可续发 7 天。
4. **需重点核对**：C1（通道位移不一致）、C3（失败路径内存泄漏，源码自承）、C2（`ota_task_info_destory` double free）。
5. **构建**：EG25 主 / EC200A（`sdk.mk` 已本机化）双线；`-DUDS_ENV_CHECK_BYPASS` 控制环境检查与本地升级；无测试/linter。
6. **权威规范缺失**：各 ECU 的 UDS 寻址 ID、安全算法、下载/擦除例程、固件格式 的权威定义均不在仓库（`bms sample/`、`uds sample/` 提供样例序列可参考）。

---

## 12. 覆盖范围与边界（自评）

### 12.1 已核实覆盖
- **架构与主流程通读**：`main.c`、`ota_task_info.c`、`ota_status.c`、`uds.c`（状态机与策略分派）、`rc40_ota_zip_file_info.c`，并 grep 核实 UDS SID 集合（`uds_pkt_send.c`）、设备类型表（`uds_dev_info.h`）、下载管理（`uds_download.c`）。
- **构建纳入范围**据主 `Makefile:54-78` 逐条确认，全部自研模块编译、无孤立、无加密文件。
- 三份 Makefile 的产物名与 bypass 宏差异已核实。

### 12.2 主动未深审（已声明）
- **UDS 传输细节未逐行**：`uds_pkt_send.c`（ISO-TP 收发）、`uds_for_yk_vcu.c`（各阶段实现）、`uds_download.c`、`uds_security_access.c`、`uds_finger_print.c` 读了结构与 SID/调用关系，未逐行核对每条报文字节。
- **格式转换未逐行**：`hex2bin.c`、`s192bin.c`、`vcu_uds_crc.c` 按模块职责定性，未验证解析算法。
- 第三方 vendored（cJSON/cc_deque/opt_iniparser/mosquitto）未审计。

### 12.3 仓库内无法闭合的边界
1. 各 ECU 的 UDS 寻址/安全/例程/固件格式 的**权威定义**（不在仓库）。
2. C1 通道位移的正确性需结合真机通道映射判定。
3. curl 源、`/usrdata` 可写性、`unzip`/`sync` 可用性 等运行环境契约。

---

*（本报告以静态阅读 + 结构化 grep 为主，未执行目标程序或真机刷写；结论均标 `文件:行`；UDS 报文级字节与格式转换算法未逐行核对，已在 §12 声明；量化行号可按函数名检索。）*
