# tsp_vin_client 仓库全面分析

> 本文基于对仓库源码、Makefile、头文件的逐文件通读得出，所有结论均标注了依据位置（`文件:行`）。
> 凡是无法从代码直接确证、只能推断的地方，均显式标注"**推断**"或"**未确证**"。
> 参照对象：`tsp_0f7b_client/md/analyse/仓库全面分析.md` 的格式与纪实性要求。
> **重要前提**：`src/mosquitto/client_mosquitto.c` 被 **E-SafeNet DLP 加密**（`file` 判定为 `data`，首部含 `E-SafeNet/LOCK` 标记），**无法静态阅读**。MQTT 线程的下行指令分发主体在此文件内，本文对该线程行为的描述只能通过它必然调用的已编译辅助函数（`frame.c`/`can_data.c`/`can_mng.c`/`version_process.c`/`tsp_cmd.c`）与 `can_mng_t` 字段**反推**，凡涉及处均标注"**推断（客户端不可读）**"。

---

## 0. 一句话概览

这是一个**跑在货车 TBOX（Quectel EC200A 蜂窝模组）上的用户态常驻守护进程**，核心职责是：
从平台/本地拿到车辆 VIN、软件版本、时间、GPS，**周期性（每 1 秒）主动把这些信息以固定 CAN 报文广播到整车 CAN 总线**（供仪表/VCU 等 ECU 使用）；同时接收平台经本机 MQTT 下发的远程指令（VIN 下发、电子围栏限速、远程补电、CAN 波特率设置、0fbd 标定），并把指令翻译成对应的 CAN 控制帧。README 命名为"从重卡平台获取车辆 VIN 码"。

> 与 `tsp_0f7b_client` 的根本差异：0f7b 是 **CAN→MQTT 的上行遥测**；本仓库主线是 **MQTT→CAN 的下行控制 + TBOX→CAN 的信息广播**，方向相反。

---

## 1. 项目定位与运行环境

| 项 | 事实 | 依据 |
|---|---|---|
| 二进制名 | `tsp_vin_client` | `Makefile:1` |
| 版本 | `_VERSION=13`（编译期 `-DX_VERSION=13`，写入 `/tmp/tsp_vin_client.ver`） | `Makefile:3`、`version.h:13`、`main.c:35-44` |
| 目标硬件 | Quectel EC200A（`_QL_PLATFORM=_QL_EC200A=3`，链接 `lib/ec200a`）；EG25/EC20/HOST 为备选分支 | `Makefile:8-30` |
| 目标架构 | ARM，交叉工具链经 `sdk.mk`（EC200A OCPU SDK，本机 `/home/tronlong/lyp/SDK/EC200A/...`） | `Makefile:26`、`sdk.mk` |
| 语言/标准 | C99，`-Werror -std=c99 -D_GNU_SOURCE` | `Makefile:77` |
| 代码风格 | `.clang-format`（与 0f7b 同一份，仓库根目录） | `.clang-format` |
| 协议参考 | `doc/demoPackage.txt`（仅一份，非权威字段规范）；README 记 v9/v10 变更（电子围栏限速、远程充电、高密上传开关） | `doc/`、`README.md` |

> README 与版本号不一致：README 顶部写到 v10，Makefile `_VERSION=13`，`can_data.c` 内注释记录到 2026-04-16。以 `Makefile` 与源码为准。

---

## 2. 构建系统

### 2.1 组成
- `Makefile`：定义源码 glob、头文件路径、平台选择、链接参数。
- `sdk.mk`（`_QL_EC200A` 分支 `-include`）：导出交叉工具链（已按本机改为 `/home/tronlong/lyp/SDK/EC200A/...`，见根目录 `CLAUDE.md`）。

### 2.2 关键事实
- 目标：`make`（`all:` 规则，编译并输出 `tsp_vin_client`）、`make clean`。无 strip 目标、无测试、无 linter。
- 链接库：`-lpthread -lmosquitto -lm`，`libmosquitto` 从 `lib/ec200a` 取（`Makefile:27,72`）。
- 编译期宏注入版本/时间/目录/平台（`Makefile:17`）。

### 2.3 源码纳入范围（Makefile glob，**本仓库最重要的构建事实**）

被编译进二进制的目录（`Makefile:57-70`）：
`main.c` + `thirdparty/{json,list,opt_iniparser}` + `src/{can, can_data, can_server, skt_res, mosquitto, gps, common, frame, can_mng, tsp_cmd, version_process}`。

**未被纳入编译**（孤立/死代码，详见 §8）：
- **整个 `src/tsp/` 目录**（`tsp.c`、`tsp-cmd.c`、`tsp-cmd-file.c`、`tsp-cmd-remote-ctrl.c`、`tsp-comm.c`，合计约 **3200 行**，是仓库中最大的一块代码）——`Makefile` 的 `APP_SRC_FILES` 完全没有 `find $(SRC_DIR)/tsp`。
- `thirdparty/posix`、`thirdparty/sany_syslog`（`Makefile` 只 `find` 了 json/list/opt_iniparser）。
- `Makefile:52` 有 `-I $(SRC_DIR)/vin`，但仓库中**不存在 `src/vin` 目录**（历史遗留 include）。

---

## 3. 进程与线程模型

`main()`（`main.c:93`）启动序列：
1. 打印版本与编译时间（`main.c:102`）。
2. **单实例保护**：`flock(/tmp/tsp_vin_client.pid, LOCK_EX|LOCK_NB)`，已占用则退出（`main.c:46-56,104-107`）。
3. `set_app_ver()`：把 `VERSION` 写入 `/tmp/tsp_vin_client.ver`（`main.c:35-44,109`）。
4. `setvbuf(stdout, _IOLBF)`：stdout 行缓冲（`main.c:110`）。
5. `can_mng_new()`：建总上下文、读设备身份/配置、手机号转 BCD、读 VIN、读 CAN 波特率（见 §4）。失败退出。
6. 初始化 `set_speed_val=0xff`（不限速），读一次 ACC 状态（`main.c:116-119`）。
7. 依次创建 3 个线程（每个 `pthread_create` 后 `sleep(1)`）：
   - `start_can_server`（CAN 线程）— 参数 `can_mng`
   - `start_mqtt_task`（MQTT 线程）— 参数 `can_mng`，**实现被加密**
   - `start_gps_task`（GPS 线程）— **参数是 `can_mng->gps_info`**（不是整个 can_mng，`main.c:125`）
8. 读 `/opt/VehicleType.file`（12 字节车型码，`main.c:128-130`）。
9. 进入 `while(1)` 主循环（周期 `usleep(1000000)`≈1s，`main.c:140-312`），末尾 `pthread_join` 三线程（死循环，永不返回）。

**主循环（`acc_status` 为真时）每秒做的事**（全部是 **TBOX→CAN 广播**）：
- VIN 帧 `0x19FFB0FC`：17 位 VIN 分 4 次发送（`vin_send_cnt` 0~3），发往 `can_fd[0]` 与 `can_fd[1]`；写失败则关闭并 `skt_res_new_socket` 重连（`main.c:142-188`）。
- GPS 位置帧 `0x18FEF3FC`（经纬度 ×1e6）、车速帧 `0x0CFE6CFC`、限速/高程帧 `0x18FEE8FC`（`set_speed_val` 有效时下发限速）（`main.c:191-258`）。
- 授时帧 `can_send_time`（`0x18FEE6FC`）、版本帧 `can_send_version`（`0x18FF00D7`）、车型码帧 `can_send_modelcode`（`0x1AD6AE4A`）（`main.c:260-281`）。
- **补电唤醒**：`battery_charge_allow==2` 时，向 VCU 连发 30 次 `can_send_vcu_wakup`（`0x18FFA2FC`），第 28 次 `system("echo 0 > /tmp/do_06")`（`main.c:285-303`）。
- 每 3 秒刷新一次 ACC 状态（`main.c:306-310`）。

> **`LONG_BATTERY` 编译宏**：决定 GPS/版本/车型码等发往 `can_fd[0]`（长电版）还是 `can_fd[1]`（`main.c:211-281`）。默认未定义 → 发 `can_fd[1]`。

> **并发说明（事实）**：三线程共享 `can_mng`。CAN 线程写 `isIncharge/soc/charge_status/battery_charge_flag/feedback_0fbd`，主循环读 `set_speed_val/battery_charge_allow/vehicle_info`，MQTT 线程（加密）写 `vehicle_info/set_speed_val/battery_charge_allow` 等。全程**无锁**（未见任何 mutex）。这是设计现状。

---

## 4. 总上下文 `can_mng_t` 与设备身份

### 4.1 结构（`can_mng.h:25-45`）
```c
typedef struct {
    int can_fd[CAN_CHN_NUM];        // 3 路 CAN 的 socket fd（实际只用 [0][1]，见 §7.1）
    int vin_get_flag;
    short serial_num;
    tbox_info_t *tbox_info;         // dev_id/手机号/BCD/本机 broker 地址
    gps_info_t *gps_info;
    tsp_8f42_vehicle_info_t *vehicle_info; // 平台下发的车辆信息(VIN/车型/波特率/K值...)
    int battery_charge_allow;       // ==2 触发补电唤醒
    char peps_seed[4]; int peps_auth_status; int peps_opt_status; // PEPS 无钥匙
    int soc; int battery_charge_flag; int acc_status;
    tsp_coolant_payload_t coolant_status;
    int set_speed_val;              // 电子围栏限速值(0xff=不限速)
    int charge_status; int isIncharge;
    tsp_8fbd_payload_t fbd_info;    // 0fbd 标定下发
    tsp_0fbd_payload_t feedback_0fbd; // 0fbd 标定回读(CAN采集)
} can_mng_t;
```

### 4.2 `can_mng_new()`（`can_mng.c:244-291`）做的事
1. `calloc` 主结构 + `tbox_info` + `gps_info` + `vehicle_info`（`can_mng.c:246-264`）。
2. `sany_get_device_conf`：读 `/opt/conf_ext.ini` 的 `mgw:local_ip`/`mgw:local_port`；**文件不存在时静默回退 `127.0.0.1:1883`**（`can_mng.c:41-60`）。
3. `sany_read_device_id`：读 `/opt/conf.ini` 的 `dev:id`（`can_mng.c:23-39`）。
4. `sany_read_sim_info`：读 `/opt/sim_info` 的 `dev:msisdn`（`can_mng.c:62-79`）。
5. `check_phone_number`：msisdn 归一化为 11 位（`can_mng.c:207-227`）。
6. 手机号转 6 字节 BCD（前补 1 字节 0 后取 12 位 → 6 BCD，`can_mng.c:271-273`、`phone_number_to_bcd:229-242`）。
7. 若 `/opt/machine_vin` 存在，读 17 字节 VIN 存入 `vehicle_info->vin`（`can_mng.c:280-286`）。
8. `sany_read_can_baud_info`：读 `/tmp/can{0,1,2}_baudrate`（`atoi*5`）（`can_mng.c:121-156`）。

> **与 0f7b 的关键差异（事实，潜在缺陷）**：`can_mng_new` **不检查** `sany_read_device_id`/`sany_read_sim_info` 的返回值（`can_mng.c:267-268`），失败时 `dev_id`/`msi_sdn` 保持 NULL。随后 `check_phone_number` 直接 `strlen(msi_sdn)`（`can_mng.c:214`）——**若 `/opt/sim_info` 缺失或无 `dev:msisdn`，此处解引用 NULL 崩溃**。0f7b 是"缺失即优雅退出"，本仓库是"缺失即段错误"（**推断**：真机上 `/opt/sim_info` 恒存在，故未暴露）。

---

## 5. 数据流（本仓库主线：MQTT 下行 + CAN 广播）

### 5.1 CAN 发送侧（`src/can_data/can_data.c`，全部现役）
统一模式：构造 `can_frame_t`（24B 结构，实测 20B，见 §6.1）、填 `can_id | CAN_EFF_FLAG`、`can_dlc=8`、填 `data[8]`、把开机毫秒拆成 `rsv_ms`(ms) + `rsv[4]`(sec) 时间戳、`write(can_fd, ...)`。逐帧清单：

| 函数 | CAN ID | 内容 | 依据 |
|---|---|---|---|
| `can_send_version` / `make_version_can_frame` | `0x18FF00D7` | TBOX 版本 BCD（`get_ver_v2` 硬编码 `"E35031A040201"`） | `can_data.c:101-150` |
| `can_send_modelcode` | `0x1AD6AE4A` | 12 位车型码打包 | `can_data.c:152-190` |
| `can_send_time` | `0x18FEE6FC` | 授时（`get_local_time_tz(-8)`，J1939 时间字段编码） | `can_data.c:202-239` |
| `can_send_vcu_wakup` | `0x18FFA2FC` | 补电唤醒 `data[0]=1` | `can_data.c:241-263` |
| `send_charge_cmd` | `0x1ACDEB4A` | 充电控制 `data[3]=cmd`，连发 `send_cnt` 次（间隔 500ms） | `can_data.c:265-286` |
| `peps_seed_request` | `0x18FFEDFC` | PEPS 无钥匙 seed 请求 | `can_data.c:288-310` |
| `peps_seed_authen` + `uds_get_key` | `0x18FFEDFC` | 用 mask `0x8EACBD34` 做 35 轮移位 seed→key 认证 | `can_data.c:312-370` |
| `peps_send_ctl_cmd` | `0x18FFEDFC` | PEPS 控制 `data[4]=cmd<<1` | `can_data.c:372-399` |
| `send_speed_to_vcu` | `0x18FEE8FC` | 限速 `data[1]=speed` | `can_data.c:401-423` |
| `get_ver` | — | 旧版本 BCD 编码（`E350317120101` 风格），当前未被 `make_version_can_frame` 调用 | `can_data.c:59-85`，**未用** |

主循环内联构造的帧（不经 can_data）：VIN `0x19FFB0FC`、GPS `0x18FEF3FC`、车速 `0x0CFE6CFC`、限速/高程 `0x18FEE8FC`（`main.c:144-249`）。

### 5.2 CAN 接收侧（`src/can_server/can_server.c: can_msg_process`）
CAN 线程 `select()` 轮询 fd（超时 100ms），`read` 最多 1440B，按 `sizeof(can_frame_t)` 切帧，逐帧：
1. `make_full_version(&frdup)`（`version_process.c:218`）：若 `can_id & CAN_EFF_MASK` 命中 27 项 `version_cfg` 之一，把 `data[0..6]` 半字节重排为 13 位版本串，变化时打印并缓存（供 `make_version_json` 汇总上报）（`version_process.c:16-26,403-431`）。
2. 按 `can_id` 分派采集（`can_server.c:138-168`）：
   - `0x98ff24d0` → `battery_charge_flag`（补电状态）
   - `0x98FF23B3` → `soc`
   - `0x98FF21B1` → `charge_status`
   - `0x9AD918D0` / `0x9AD91AD0` → `feedback_0fbd` 各标定字段（`getVale` 按位段取，`can_server.c:151-165`）
   - 其余 → `charge_check`：`0x19FFF413`/`0x19FFF312`/（CAN2 上的）`0x410` → `isIncharge=1`，10s 无充电帧则清 0（`can_server.c:64-94`）

### 5.3 MQTT 下行侧（`start_mqtt_task`，**加密不可读**）
由头文件与已编译的辅助函数**反推**其职责（**推断（客户端不可读）**）：
- 连接本机 broker（`tbox_info->local_ip:port`），发布主题 `LOCAL_TOPIC_PUB_HEAVY="mqtt_upload_message"`（`client_mosquitto.h:4`）。
- 订阅平台下行，解析 `8F42` 车辆信息 JSON（调 `frame.c:parse_8f42_vehicle_info` → `vehicle_info`），据此设置 VIN/车型/波特率/K值/`energyDataSwitch` 标志（`frame.c:47-145`）。
- 处理远程设置/查询（调 `frame.c:tsp_cmd_remote_set_resp_make`、`tsp_cmd.c:tsp_cmd_general_resp_make`），下发限速（`set_speed_val`）、补电（`battery_charge_allow`）、PEPS、标定等，翻译为 §5.1 的 CAN 帧。
- 汇总版本 JSON（`version_process.c:make_version_json`）上报 `0F42`。
- 提供 `save_log()`（被 `main.c` 调用，`client_mosquitto.h:7`、`main.c:180,187,240`）。

---

## 6. TSP 帧协议封装（`src/tsp_cmd/tsp_cmd.c`，现役）

与 `tsp_0f7b_client` 的 `tsp_cmd` **同源**（版权头均为 `ROOTCLOUD/irootech`，`tsp_cmd.h:1-14`），wire 协议一致：

### 6.1 结构与大小（宿主 gcc `sizeof` 实测）
| 结构 | 大小 | 说明 | 依据 |
|---|---|---|---|
| `can_frame_t` | **20** 字节 | can_id(4)+can_dlc(2)+rsv_ms(2)+data[8]+rsv[4]，**非 packed 但自然对齐即 20**。**注意：这是与桥接进程约定的线格式** | `can.h:86-92` |
| `gps_info_t` | 164 字节（packed） | GPS 源记录 | `gps.h:15-26` |
| `tsp_cmd_attr_t` | 2 字节（位域 msg_len:10/encrypt:3/split:1/res:2） | msg_len ≤ 1023 | `tsp_cmd.h:26-31` |
| `tsp_cmd_header_t` | 16 字节 | cmd_id(2)+attr(2)+device_id[6]+seq(2)+pack_total(2)+pack_num(2) | `tsp_cmd.h:33-40` |
| `tsp_coolant_payload_t` | 8 字节（packed） | 冷却液（0fxx） | `frame.h:48-55` |
| `tsp_8fbd_payload_t` | 24 字节（packed） | 0fbd 标定下发 | `frame.h:72-87` |
| `tsp_0fbd_payload_t` | 26 字节（packed） | 0fbd 标定回读 | `frame.h:89-106` |

### 6.2 帧编码 `tsp_cmd_frame_make`（`tsp_cmd.c:238-279`）
逐字节铺 `cmd_id/cmd_attr/device_id/seq`（split 时加 pack_total/num）+ body → 追加 **XOR 校验和**（`tsp_cmd_make_check_sum:62-71`）→ **转义** `0x7e→7d 02`/`0x7d→7d 01`（`tsp_cmd_pton:41-60`）→ 首尾加 `0x7e` 定界。反向 `tsp_cmd_frame_parse`/`tsp_cmd_ntop`/`tsp_cmd_check` 齐备（`tsp_cmd.c:9-166`），用于解析下行指令（由加密的 MQTT 线程调用）。

### 6.3 响应构造（`frame.c`，现役）
- `tsp_cmd_remote_set_resp_make`（`0x0F41` 应答，`frame.c:235-258`）
- `tsp_cmd_general_resp_make`（`0x0001` 通用应答，`tsp_cmd.c:301-324`）
- `make_0f42_payload`/`make_0f42_version_info`（版本上报载荷，`frame.c:15-45`）
- `make_tsp_0f6b_frame`/`8F40` 系列被 `#if 0` 包裹（死代码，`frame.c:261-333`）

---

## 7. 三个工作线程细节

### 7.1 CAN 线程 `src/can_server/can_server.c`
- `start_can_server`（`can_server.c:176-204`）：**循环 `for(i=0;i<2;i++)`** 连接 `CAN0_SKT_PORT=16002`、`CAN1_SKT_PORT=16003`（`can.h:71-74`）。**注意：`CAN_CHN_NUM=3` 但代码只开 2 路，`can_fd[2]`（CAN2/16004）从不建立**（`can_server.c:183`）。两路都连上才进 `can_msg_process`。
- `can_msg_process`（`can_server.c:96-174`）：`select` 超时 100ms 轮询 `can_fd[0..1]`，`read` 错误累计 10 次 `return -1` 触发上层重连。
- 连接经 `skt_res_new_socket`（TCP 连 `127.0.0.1:port`，最多重试 30 次，`skt_res.c:27-61`）。真实 CAN 硬件由**模组上另一桥接进程**接管，本程序不碰 SocketCAN。
- **收帧结构即 `can_frame_t`（20B，见 §6.1）**，非内核 `struct can_frame`。

### 7.2 GPS 线程 `src/gps/gps.c`
- `start_gps_task`（`gps.c:14-48`）：TCP 客户端连 `127.0.0.1:16005`（`GPS_SKT_PORT`，`gps.h:13`），`recv` 定长 `gps_info_t`（164B），拷进传入的 `gps_info` 指针。
- 用 `getsockopt(TCP_INFO)` 探测连接态，非 `ESTABLISHED` 则重连；连续 10 次 `lon==lat==0` 也重连（`gps.c:26-45`）。每秒一轮。
- **缺陷（事实）**：`gps_skt_fd` 初值为 `0`（`gps.c:21`），首轮 `getsockopt(0,...)` 作用在 **fd 0（stdin）** 上，随后 `close(gps_skt_fd)` 会**关闭 stdin**（`gps.c:28`）。真机上 stdin 通常无用，故无观测影响（**推断**）。

### 7.3 MQTT 线程 `src/mosquitto/client_mosquitto.c`
- **文件被 E-SafeNet 加密，无法阅读。** 已知：入口 `start_mqtt_task`、导出 `save_log`、发布主题 `"mqtt_upload_message"`（`client_mosquitto.h`）。其余职责见 §5.3（反推）。

---

## 8. 孤立/未编译模块（存在于源码但不进二进制）

| 目录/文件 | 状态 | 说明 | 依据 |
|---|---|---|---|
| `src/tsp/`（整目录，~3200 行） | **未编译** | 一整套 **SEAS 框架**：`tsp.c`（config.json 加载、`tsp_handle`）、`tsp-comm.c`（MQTT 订阅/发布队列 `Deque`）、`tsp-cmd.c`（另一份 tsp_cmd，与 `src/tsp_cmd` 重复）、`tsp-cmd-file.c`（AUL 异步文件上传 `8F45/8F46/8F49`）、`tsp-cmd-remote-ctrl.c`（远程设置/查询 `8F41/8F51`）。依赖 `seas-aul-ftp/seas-file-storage/seas-timer` 等**仓库内不存在的头文件**，若编译必失败。 | `Makefile:57-70` 未含；`tsp.c:8-14` |
| `thirdparty/posix` | **未编译** | 未进 Makefile glob | `Makefile:57-59` |
| `thirdparty/sany_syslog` | **未编译** | 同上 | `Makefile:57-59` |

**判读**：`src/tsp/` 是一套**更完整/更规范的下行指令框架**（含文件上传、订阅管理、远程读写），但当前**没有接线**；实际运行用的是**加密的 `client_mosquitto.c`**。二者是"新框架未启用、旧实现在跑"的关系（**推断**：`src/tsp/` 为迁移中或备用实现）。可作为字段/协议语义参考，但不参与运行。

**第三方 vendored 库**（`thirdparty/json`=cJSON、`thirdparty/list`、`thirdparty/opt_iniparser`、`include/mosquitto.h`）视为上游原样，未逐行审计。

---

## 9. 运行时的外部文件/端口依赖总表

| 资源 | 用途 | 缺失后果 | 依据 |
|---|---|---|---|
| `/opt/conf.ini` `dev:id` | 设备 ID | 静默（不退出，dev_id=NULL） | `can_mng.c:23-39,267` |
| `/opt/sim_info` `dev:msisdn` | SIM 手机号（→device_id BCD） | **`check_phone_number` 解引用 NULL 崩溃** | `can_mng.c:62-79,214` |
| `/opt/conf_ext.ini` `mgw:local_ip/port` | 本机 broker 地址 | 静默回退 `127.0.0.1:1883` | `can_mng.c:41-60` |
| `/opt/machine_vin` | 本地 VIN（17B） | 不读，`vehicle_info->vin` 可能为空→VIN 帧不发 | `can_mng.c:280`、`main.c:142` |
| `/opt/version_info.ini` | 内核/包版本（版本上报用） | `read_version_info` 返回 -1 | `version_process.c:103-129` |
| `/opt/VehicleType.file` | 12 位车型码 | `VehicleType` 空串 | `main.c:129` |
| `/tmp/can{0,1,2}_baudrate` | 各路 CAN 波特率（×5） | 不更新 `vehicle_info->*BaudRate` | `can_mng.c:121-156`、`can.h:76-78` |
| `/tmp/acc_stat` | ACC 状态 | `waiting_ig_key_off` 返回 false（不广播） | `common.c:162-180`、`common.h:35` |
| `/tmp/mcu_pwr_voltage` | 电源电压 | `get_power_value` 返回 0 | `common.c:182-199` |
| `/tmp/mcu_main_version` `/tmp/mcu_load_version` | MCU 版本 | 版本串填 0 | `version_process.c:135-156` |
| `Pointsheet_info1.json`（工作目录相对） | 只取 `sver`/`pver` 上报 | 版本字段填 0 | `version_process.c:162` |
| `/tmp/do_06` | 补电 DO 控制（`system("echo ...")`） | — | `main.c:292,300` |
| `/tmp/energyDataSwitch.flag` | 高密数据开关（`touch/rm`） | — | `frame.c:130-132` |
| `/media/sdcard/budian_wakup.log` | 补电日志（1M 备份） | — | `main.c:58-91` |
| `127.0.0.1:16002/16003` | CAN0/1 桥接 | 重连等待（30 次后放弃再循环） | `can_server.c:183`、`skt_res.c:30` |
| `127.0.0.1:16005` | GPS 源 | 无限重连 | `gps.c:29` |
| `127.0.0.1:16008` | 波特率设置命令通道 | `save_can_baud_info` 写失败 | `can_mng.c:197` |
| 本机 MQTT broker | 下行指令入口/上行出口 | （加密线程，行为不可见） | `client_mosquitto.h` |

---

## 10. 完整数据流（端到端）

```
                         [本机 MQTT broker] ── 平台下行指令
                                  │ (订阅, 加密线程 client_mosquitto.c 不可读)
                                  ▼
[MQTT线程] 解析 8F42车辆信息JSON(frame.c:parse_8f42_vehicle_info) / 远程设置(8F41)
     ├─ 写 vehicle_info(VIN/车型/波特率/K值) · set_speed_val · battery_charge_allow
     │        │ 共享(无锁)
     ▼        ▼
[主循环 每1s, ACC on] ── TBOX→CAN 广播:
     ├─ VIN 0x19FFB0FC(分4帧) → can_fd[0]&[1]
     ├─ GPS 0x18FEF3FC / 车速 0x0CFE6CFC / 限速+高程 0x18FEE8FC
     ├─ 授时 0x18FEE6FC / 版本 0x18FF00D7 / 车型码 0x1AD6AE4A
     └─ 补电唤醒 0x18FFA2FC (battery_charge_allow==2, 30次)
                                  ▲ 读 set_speed_val 等
        [GPS线程] 127.0.0.1:16005 → gps_info (每1s)
                                  
[CAN线程] can_msg_process ← can_fd[0..1] (16002/3)
     ├─ make_full_version: 各ECU版本采集 → version_cfg 缓存
     └─ 采集 soc/charge_status/battery_charge_flag/isIncharge/feedback_0fbd(标定回读)
```

---

## 11. 已识别的问题清单（按严重度）

> 均基于实读代码；凡触发条件依赖真机/对端者标注。分级：高=当前很可能出错；中=确凿缺陷但当前多不触发；低=健壮性/可移植性。

| 编号 | 严重度 | 位置 | 问题 | 触发条件与影响 | 确证/推断 |
|---|---|---|---|---|---|
| V1 | **高** | `can_mng.c:214`（`check_phone_number`） | `/opt/sim_info` 缺失时 `msi_sdn=NULL` 未判空即 `strlen` | 无 SIM 配置文件→**启动即段错误** | 确证（依赖真机文件存在）|
| V2 | 中 | `gps.c:21,28` | `gps_skt_fd` 初值 0，首轮 `close(0)` 关闭 stdin | 每次进程启动首轮 | 确证，影响小（**推断**无害）|
| V3 | 中 | `frame.c:223-229`（`tsp_cmd_remote_set_resp_format`） | `tsp_remote_set_resp_cmd_t resp_cmd` **未初始化**即 `memcpy` 其 `num/ins/result` 进应答 | 每次远程设置应答，回给平台 3 字节栈垃圾 | 确证（是否调用取决于加密线程，**推断**在用）|
| V4 | 中 | `can_server.c:56`（`getVale`） | `((uint64_t)ptrCanFrm[n])`，`ptrCanFrm` 为 `char*`，字节≥0x80 时符号扩展 | 标定字段含高位字节时数值错误 | 确证；ARM `char` 无符号→目标平台侥幸正确（**推断**）|
| V5 | 低 | `can_server.c:183,96`、`can.h:71` | `CAN_CHN_NUM=3` 但只开/轮询 2 路，`can_fd[2]` 恒未初始化(=0) | `charge_check` 里 `can_port==2` 分支永不成立 | 确证（CAN2 功能实际未启用）|
| V6 | 低 | `main.c:60-68`（`save_wakup_log`） | `lstat` 返回值未查，文件不存在时 `dstat.st_size` 未定义 | 首次可能误触发 `mv` 备份 | 确证，影响小 |
| V7 | 低 | `can_data.c` 多处 | `int ret;` 声明后未初始化（`peps_*`/`send_charge_cmd`），仅在既定路径赋值 | 编译告警级；`send_charge_cmd` 的 `ret` 定义后未用 | 确证 |
| V8 | 低 | `version_process.c:444`、`can_data.c:105` | TBOX 版本串**硬编码**（`"E35031A040201"`/`get_ver_v2`），`make_tbox_version_string` 动态版本被注释弃用 | 版本上报/CAN 版本帧恒为写死值 | 确证（设计如此）|
| V9 | 低 | `Makefile:52` | `-I $(SRC_DIR)/vin` 指向不存在目录 | 无（include 路径无效但不报错） | 确证 |

---

> **修复状态（2026-07-10，working tree 未提交；EC200A 交叉工具链 `-Werror -std=c99` 语法检查通过，待真机验证）**
> - **V1 已修复**：`can_mng.c` `check_phone_number` 增加 `msi_sdn==NULL` 判空后返回。
> - **V2 已修复**：`gps.c` `gps_skt_fd` 初值改 -1，`close`/`getsockopt` 加 `>=0` 守护，避免误操作 stdin。
> - **V3 已修复**：`frame.c` `resp_cmd` 零初始化，消除回传平台的栈垃圾（确切协议值仍待对规范）。
> - 其余 V4（ARM char 无符号侥幸无害）、V5–V9（低severity）按范围未改。
> - 注：整仓完整编译受既有加密文件 `client_mosquitto.c` 阻塞（与本次修复无关），本机无 DLP 解密层。

## 12. 需真机/对端才能验证的假设点

1. 桥接进程送来的 `frame.can_id` **带扩展帧标志位**（采集分派用原始 `can_id` 直接比较 `0x98ff24d0` 等，而 `charge_check` 用 `can_id ^ 0x80000000` 去标志——两处处理不一致，`can_server.c:67,138`）。
2. `can_frame_t` 线格式为 **20 字节**且与桥接一致；否则切帧错位。
3. 端口 16002/16003↔CAN0/1、16005↔GPS、16008↔波特率命令通道 的映射正确。
4. `gps_info_t`（164B packed，含 116B 保留）与 GPS 源逐字节一致；`lat/lon` 为十进制度（主循环 ×1e6）。
5. 平台下发 `8F42` JSON 的键名（`vin/VehicleType/pcanBaudRate/energyDataSwitch/...`）与 `frame.c:parse_8f42_vehicle_info` 解析一致。
6. VIN 帧 `0x19FFB0FC` 的 4 帧分包格式、PEPS `uds_get_key` 的 mask `0x8EACBD34`、授时 `-8` 时区偏移 等与整车 ECU 约定一致。
7. 加密的 `client_mosquitto.c` 行为（订阅主题、指令码、如何写 `set_speed_val`/`battery_charge_allow`）——**静态不可见**。

---

## 13. 结论性要点（速览）

1. **本质**：MQTT 下行控制 + TBOX→CAN 信息广播的守护进程，主循环 1s 周期，方向与 0f7b（上行遥测）相反。
2. **加密盲区**：核心下行分发在 **E-SafeNet 加密的 `client_mosquitto.c`**，本报告对 MQTT 线程的描述均为经已编译辅助函数的反推。
3. **大量代码不参与运行**：整个 `src/tsp/`（~3200 行 SEAS 框架，含文件上传/订阅/远程读写）**未编译**；`can_frame_t` 只用 2 路 CAN；`get_ver`/`make_tbox_version_string` 等被弃用；`#if 0` 死代码若干。
4. **构建现实**：只能用 `sdk.mk` 指定的 EC200A 交叉工具链（已按本机修正）编译；无 strip/测试/linter。
5. **最需留意的缺陷**：V1（缺 `/opt/sim_info` 崩溃）、V3（未初始化栈值回给平台）、V4（`char` 符号扩展依赖 ARM ABI）。
6. **权威规范缺失**：`doc/` 仅 `demoPackage.txt`，各 CAN 帧/8F42 字段的权威定义**不在仓库**，§5/§6 的帧描述只能源自 C 代码。

---

## 14. 覆盖范围与边界（本报告自评）

### 14.1 已核实覆盖
- **已编译的一手代码全部通读**：`main.c`、`can.c`、`can_data.c`、`can_server.c`、`can_mng.c`、`common.c`、`gps.c`、`skt_res.c`、`frame.c`、`tsp_cmd.c`、`version_process.c` 及对应头文件。
- **结构体大小**经宿主 gcc `sizeof` 实测（§6.1）。
- **构建纳入范围**据 `Makefile:57-70` 逐条确认；`src/tsp/`、`thirdparty/posix`、`thirdparty/sany_syslog` 未编译已核实。

### 14.2 主动未深审（已声明）
- **`src/mosquitto/client_mosquitto.c`：E-SafeNet 加密，无法阅读**（这是本报告最大的确定性缺口）。
- `src/tsp/` 孤立框架：读了 `tsp.c` 全文与其余文件的结构/函数名，未逐行核对（死代码）。
- 第三方 vendored 库（cJSON/list/iniparser/mosquitto）未逐行审计。

### 14.3 仓库内无法闭合的边界
1. MQTT 下行指令的**指令码与分发逻辑**（在加密文件内）。
2. 各 CAN 帧 `data[]` 字段布局与整车 ECU 的**权威约定**（不在仓库）。
3. `8F42` JSON、PEPS 认证、波特率命令通道 的**对端契约**——静态分析给不了定论。

---

*（本报告只做静态阅读，未执行目标程序；结构体大小经宿主 gcc `sizeof` 验证；`client_mosquitto.c` 因 E-SafeNet 加密未能阅读，相关结论均为反推并已标注；量化行号可按函数名检索。）*
