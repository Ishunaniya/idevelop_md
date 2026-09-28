# tsp_0f7b_client 仓库全面分析

> 本文基于对仓库源码、配置、点表 JSON 的逐文件通读得出，所有结论均标注了依据位置（`文件:行`）。
> 凡是无法从代码直接确证、只能推断的地方，均显式标注"**推断**"或"**未确证**"。
> 分析时的分支：`feature/JS`；最新提交 `f5393cc`。

---

## 0. 一句话概览

这是一个**跑在货车 TBOX（Quectel 蜂窝模组）上的用户态常驻守护进程**，作用是：
从本机的 CAN 桥接进程读取 CAN 报文 → 按点表 JSON 把信号解析进内存 → 每 30 秒把当前信号快照
打包成固定二进制结构体 → 套上 TSP 私有帧协议 → 通过本机 MQTT broker 发布出去。
面向"重卡事业部**轻卡**车企标数据"，TSP 命令号 `0x0f7b`。

---

## 1. 项目定位与运行环境

| 项 | 事实 | 依据 |
|---|---|---|
| 二进制名 | `tsp_0f7b_client`（Makefile `APP_NAME`）；但 PID/版本文件、MQTT client-id、README 仍叫 `0f6b` | `Makefile:2`、`main.c:13,17`、`client_mosquitto.c:55`、`README.md` |
| 目标硬件 | Quectel 模组 EC200A（Makefile 实际链接）/ EG25-G（备用库） | `Makefile:6`、`lib/ec200a`、`lib/eg25-g` |
| 目标架构 | ARM，`armv7-a + neon + hard-float`，交叉工具链 `arm-openwrt-linux-*` gcc 8.4.0 | `sdk.mk:13-31` |
| 语言/标准 | C99，`-Wall -Werror -D_GNU_SOURCE` | `Makefile:43` |
| 协议文档 | `md/doc/` 下有《三一重卡TBOX与TSP平台通讯协议(含电动车).docx》与《江山_重卡新能源企标数据功能(0f6b)V1.7_20260305.xlsx》 | `md/doc/` |

> 注：README 第一行写 `# tsp_0f6b_client`，第二行写"重卡事业部轻卡车企标数据：0f7b"——命名混用是本仓库的显著特征，详见 §9。

---

## 2. 构建系统

### 2.1 组成
- `Makefile`：定义源码 glob、头文件路径、链接参数。
- `sdk.mk`（被 Makefile `include`）：硬编码 Quectel SDK/工具链路径，`export` 出 `CC/AR/STRIP/...` 等交叉工具。

### 2.2 关键事实
- **`sdk.mk` 路径已被改成本机实际安装位置**：`QL_SDK_DIR=/home/tronlong/lyp/SDK/EC200A/ql-ol-extsdk-...`，工具链在 `/home/tronlong/lyp/SDK/EC200A/crosstools/...`（`sdk.mk:1-4`）。
  这与 `CLAUDE.md` 里描述的旧路径（`/home/tong/workdir/...`）**不一致**——最新提交 `f5393cc` 的说明正是"修正 SDK/工具链路径为本机实际安装位置"。**以 `sdk.mk` 实际内容为准。**
- 链接库：`-lpthread -lmosquitto`，`libmosquitto` 从 `lib/ec200a` 取（`Makefile:6,38`）。
- SDK 侧还预备了 `-lql_sdk -lql_sys_log -lql_lib_ipc -lql_lib_utils -lql_sdk_cmpt -lsqlite3`（`sdk.mk:41`），但**注意**：`Makefile:43` 的 `all` 规则实际只用了 `$(LDFLAGS)`（pthread+mosquitto），并**没有引用** `QL_SDK_LIBS`/`QL_SDK_CFLAGS`/`QL_SDK_LDFLAGS`。即 `sdk.mk` 里定义的这些 SDK 编译/链接变量当前**未被 Makefile 消费**，只有工具链 `export`（CC/STRIP 等）和 PATH 生效。
- 目标：`make`（编译）、`make release`（编译+strip）、`make clean`。无 host-native 目标、无测试、无 linter。
- 代码风格：`.clang-format`（4 空格缩进、Linux 大括号风格、110 列）。

### 2.3 源码纳入范围（Makefile glob）
被编译的目录：`can_mng, data_process, can_server, common, mosquitto, cJSON, list, opt_iniparser, frame, gps, skt_res, tsp_cmd` + `main.c`。
**未被纳入编译**的目录：`src/scene/`、`src/scene_lib/`、`src/log/`（详见 §8 孤立模块）。

---

## 3. 进程与线程模型

`main()`（`main.c:42`）的启动序列：

1. **单实例保护**：对 `/tmp/tsp_0f6b_client.pid` 加 `flock(LOCK_EX|LOCK_NB)`，已被占用则退出（`main.c:15-29,47`）。
2. **写版本号**：把 `APP_VER=8` 写到 `/tmp/tsp_0f6b_client.ver`（`main.c:12,31-40`）。
3. **`can_mng_new()`**：读设备身份/配置，构建总管理上下文（见 §4）。失败即退出。
4. **`mosquitto_lib_init()`**。
5. **`load_pointsheet_json(..., "Pointsheet_info1.json")`**：解析点表成规则树（见 §5）。失败即退出。
6. 依次创建 3 个线程（每个 `pthread_create` 后 `sleep(1)`）：
   - `start_can_server`（CAN 线程）
   - `start_gps_task`（GPS 线程）
   - `start_mqtt_task`（MQTT 线程）
7. `pthread_join` 三个线程——但它们都是死循环，正常情况下永不返回。这是一个 **run-forever 守护进程**。

三个线程共享同一个 `can_mng_t *can_mng` 上下文指针（`main.c:67-71`）。

> **并发说明（事实）**：CAN 线程持续写 `data_item_t` 的值，MQTT 线程每 30s 读同一批 `data_item_t`，两者**无锁**共享 `data_process_mng` 的规则树。代码中未见任何互斥/原子保护。这是设计现状，非本文评价。

---

## 4. 总上下文 `can_mng_t` 与设备身份

### 4.1 结构（`can_mng.h:18-27`）
```c
typedef struct {
    data_process_mng_t *data_process_mng; // 点表规则树 + 信号状态
    int can_fd[3];                        // 3 路 CAN 的 socket fd
    int can_rcv_cnt[3];                   // 每路收帧计数
    char *cur_scene;                      // 场景（恒为 "power_on"）
    short serial_num;                     // TSP 帧序列号(每发一帧自增)
    uint8_t lock_status;                  // 声明了但全程未被赋值/使用
    tbox_info_t *tbox_info;               // 设备身份/broker 地址
    sany_gps_ec21_t *gps_info;            // 最新 GPS
} can_mng_t;
```

### 4.2 `can_mng_new()`（`can_mng.c:26-88`）做的事
1. `data_process_mng_init()`：分配管理结构，`cur_scene` 初始化为 `"power_on"`（`data_process.c:1887-1894`）。
2. `sany_read_device_conf(can_mng)`：读 `/opt/conf_ext.ini` 的 `mgw:local_ip`/`mgw:local_port`（本机 MQTT broker 地址）。**文件不存在时不报错，静默回退到 `127.0.0.1:1883`**（`common.c:114-132`）。
3. `sany_read_device_id(can_mng)`：读 `/opt/conf.ini` 的 `dev:id`。**失败则整个 `can_mng_new` 失败、进程退出**（`common.c:69-93`）。
4. `sany_read_sim_info(can_mng)`：读 `/opt/sim_info` 的 `dev:msisdn`（SIM 手机号）。**失败同样致命**（`common.c:95-112`）。
5. `check_phone_number()`：把 msisdn 归一化成 11 位（不足前补 '0'，超长取首位+后 10 位）（`can_mng.c:6-24`）。
6. 手机号转 BCD（6 字节），作为 TSP 帧的 device_id（`can_mng.c:78-84`、`common.c:134-147`）。
7. `cur_scene = "power_on"`（`can_mng.c:86`）。

> 关键运行时依赖（只在真机存在，off-device 不可复现）：
> `/opt/conf.ini`、`/opt/sim_info` **缺失 → 进程退出**；`/opt/conf_ext.ini` 缺失 → 静默默认值。

---

## 5. 数据管线（本仓库最核心、跨文件最多的部分）

整条链路：**点表 JSON → 规则树 → CAN 写入 → 每 30s 读出打包 → TSP 帧 → MQTT**。

### 5.1 点表 `Pointsheet_info1.json`（数据来源，非代码）

顶层字段（`Pointsheet_info1.json:1-8`）：
- `mver:"LHT"`（machine_ver）、`sver:1`（schema_ver）、`pver:1`（pointsheet_ver）、`mtyp:3`（msg_type，=`MSG_TYPE_BINARY`，见 `data_process.h:25-30`）。
- `sce:["30000:30000:30000:30000"]`：唯一一个场景字符串。
- `dev[]`：本文件只有 **1 个 dev，`slot:0, typ:0`**（经统计确认）。

层级结构（4 层匹配树）：
```
dev[]  (slot=CAN通道号, typ)
 └ msg[]  一条 CAN 帧匹配规则: typ/ofs/len/val   (val 如 "#8CFF0204" = 29位扩展帧ID)
    └ rls[]  嵌套子规则: 匹配帧内某字节子字段 (如 val "8" = 源地址/PGN 字节)
       └ pts[]  叶子"点": nm(信号名)/ofs(位偏移)/len(位长)/bo(字节序)/sc(scale)/pofs(offset)/sign/sce/scems
```

统计事实：本点表共 **34 个 `nm` 点**、**39 个 `val` 匹配节点**（含 msg 层与 rls 层）。
所有 `msg.val` 帧 ID（19 个，均在 slot 0）：
`8CFF0204, 98FF0304, 8CFF0104, 98FF0404, 8CFF0004, 98FF8062, 8CFE0427, 8CFF2227, 98FEC1EE, 8CFF300B, 98FF2027, 8CFF1012, 98FF1312, 98FF1412, 98FF1512, 98FF1212, 98FF1612, 98FF1712, 98FF1812`。
（`rls` 层几乎都用 `val:"8"` 匹配一个 8-bit 子字段。）

### 5.2 加载：`load_pointsheet_json()`（`data_process.c:360-436`）
- 读文件 → cJSON 解析 → 填 `machine_ver/schema_ver/pointsheet_ver/msg_type/scene[]`。
- 遍历 `dev[] → msg[]`，对每个 msg 调 `get_common_data_rule()` 递归建规则，然后
  `HASH_ADD_INT` 以 `data_rule->value`（= CAN ID）为键，加入 `data_process_mng->data_rule[port]`——
  **每个 CAN 通道一张 uthash 哈希表**（`data_rule[3]`，`data_process.h:80`）。

### 5.3 建树：`get_common_data_rule()`（`data_process.c:114-259`，递归）
- 解析 `typ/ofs/len`；解析 `val`：
  - 以 `#` 开头 → `CharToHexMem()` 按 16 进制转成 `data_rule->value`。
    经核对，`"#8CFF0204"` 会被转成小端内存 `04 02 FF 8C`，即数值 `0x8CFF0204`——**顶位已置 1，等价于带 CAN 扩展帧标志**（`data_process.c:43-58,135-136`）。
  - 否则 `atoi` 后按 `len/8` 字节拷贝（`data_process.c:137-140`）。
- 有 `rls` → 递归建子规则，挂到 `data_sub_rule_list`。
- 有 `pts` → 每个点建一个 `data_item_t`，挂到 `data_item_list`。
- **一个规则要么是子规则容器、要么是叶子点容器**（读写两侧都按此二分处理）。
- 点的场景处理（`data_process.c:214-245`）：`sce` 数组里的每个场景名必须能在全局 `scene[]` 里找到，否则 `load` 直接失败返回 NULL；`scems` 填 `interval[]`。

数据结构：
- `data_rule_t`（`data_process.h:32-40`）：`type/offset/length/value(键)` + `data_sub_rule_list` + `data_item_list` + `UT_hash_handle`。
- `data_item_t`（`data_process.h:42-65`）：信号名、位偏移/长度、字节序、值类型、符号、scale/offset、以及**运行时活值** `value_int/value_double/value_string`、`time_rcv/time_pub`、场景/间隔。

### 5.4 写侧（CAN 线程，持续运行）
`can_msg_process()`（`can_server.c:45-98`）对每一帧调用两个函数：
1. **`parse_common_msg(mng, &frame, frame.can_id, chn)`**（`data_process.c:348-358`）：
   用 `HASH_FIND_INT` 在该通道哈希表里找 `can_id` 对应规则，命中则 `set_common_data_item()` 递归下钻，
   把报文里对应比特位段解析后写入叶子 `data_item->value_int/value_string` 和 `time_rcv`（`data_process.c:261-346`）。
   - 位解析同时支持小端（`bo=0`）和大端（`bo=1`），用移位+掩码取任意 bit 段（`data_process.c:280-303`）。
2. **`parse_bms_sn_frame(mng, &frame)`**（`data_process.c:1941-1958`）：
   专门处理 CAN ID `0x18E1EFF3` 的多帧 BMS 序列号（4 帧、每帧 6 字节拼成 24 字节），用位标志 `bms_frame_recv_flag`（收全=`0x0F`）记录进度。

> **写侧键匹配（事实）**：写侧直接用 `frame.can_id` 做哈希键，而规则键（如 `0x8CFF0204`）顶位已含扩展帧标志。
> 因此**要命中，桥接进来的 `frame.can_id` 必须自带扩展帧标志位**。这与读侧的处理方式不同（读侧见 §5.5）。

### 5.5 读侧 + 打包：`binary_work_data_generate()`（0f7b 实时版，`data_process.c:563-661`）
- 由 MQTT 线程每 30s 调一次。
- 首字段 `Tbox_Volt`：来自 `read_mcu_power_voltage_mv()`（读 `/tmp/mcu_pwr_voltage`），换算 `mv*0.001/0.05` 后 `htons`（`data_process.c:568-571`）。
- 其余通过 **21 次 `get_0f6b_work_data(mng, &item, "信号名", 0x0CFF0204)`** 逐个把命名信号取回来填进结构体。
  - `get_0f6b_work_data()`（`data_process.c:478-495`）：遍历 3 个通道哈希表，用 **`can_id | CAN_EFF_FLAG`** 做键（即读侧在这里补上扩展帧标志），命中后 `check_0f6b_data_item()` 按信号名在规则树里递归查找匹配的叶子，回填 `value_int`（`data_process.c:438-476`）。
  - 多字节字段用 `htons/htonl` 转网络序；里程 `IC_TotalMailage` 还乘了 `0.005`（`data_process.c:625`）。
- 返回 `sizeof(data_0f7b_t)`（= **156 字节**，见 §6.1）。

**实时路径实际填充的 22 个字段**（1 个电压 + 21 个 CAN 信号）：
`Tbox_Volt, MCU_MotorTorque, MCU_MotorSpeed, MCU_MotorTemp, MCU_ConttrollerTemp, MCU_RunningSt,
MCU_BusVolt, MCU_BusCurrent, MCUFlt, DCDC1_DTCCode, DCDC1_DCDCState, VCU_Gear,
VCU_AccelPedalPosition, IC_TotalMailage, ABS1_ABSFailed, ABS1_VehicleSpeed,
VCU_VehicleHighestWarningLevel, VCU_HVLockSt, SUP_ePTState, BMS1_BatVoltage, BMS1_BatCurrent, BMS1_BatSoc`。

> **重要事实**：`data_0f7b_t` 共有 **46 个字段**，实时函数只赋值了上述 **22 个**。
> 其余 **24 个字段全程从未被写入，永远保持 memset 的 0**（`client_mosquitto.c:100` 每轮 `memset`）：
> `BMS2_data, BMS3_Temp[6], BMS4_ChrState, BMS6_Bat_Warn[8], BMS7_code[8], BMS8_A_ID_Volt,
> BMS8_Model_number_Volt, BMS8_CellTemp_1, BMS_VCU_WarningState, BMS_VCU_Negative, BMS_VCU_FaultCode,
> MCU_DM1[6], TP_CM_BAM[8], DCDC_DM1[6], BMS_DM1[6], VCU_DM1[6], TP_CM_BAM1[8], TP_CM_BAM2[8],
> TP_CM_BAM3[8], ACCM_Remote, tboxACK, DM2[6], TP_CM_BAM_2[8], TP_DT2[8]`。
> 这些多为 J1939 诊断/传输协议（DM1/BAM/DT）与握手字段，当前实现留作**占位**。
>
> **另一处事实**：`parse_bms_sn_frame` 每帧都在拼 BMS 序列号进 `mng->bms_sn`，但 **`data_0f7b_t` 结构体里没有 BMS_SN 字段**，实时 0f7b 打包函数也从不使用 `bms_sn`。BMS SN 只有在 `#if 0` 的旧 0f6b 版里才会被拷进 `payload->BMS_SN`（`data_process.c:1819-1821`）。即当前 BMS SN 组装结果是**算了但没用**。

---

## 6. TSP 帧协议封装

### 6.1 payload 结构体 `data_0f7b_t`（`data_process.h:200-248`）
- `__attribute__((packed))`，逐字段紧凑排列，**总大小 156 字节**（按字段累加核算）。
- 字段用 `BYTE/WORD/DWORD`（=u8/u16/u32）typedef。

### 6.2 一级头 `level1_0f6b_t`（`frame.h:6-15`，21 字节 packed）
`version(1) + level1packageNum(1) + lat(4) + lon(4) + altitude(2) + direction(2) + date[6] + level2PackageNum(1)`。

### 6.3 `make_tsp_0f7b_frame()`（`frame.c:59-102`）
1. 填一级头：`version=0x0`、经纬度 = `gps_info->lat/lon * 1e6` 后 `htonl`、`altitude/direction` 写死 0、`date[6]` = 本地时间 BCD（`get_localtime_string`，`common.c:174-187`）。
2. `data = level1头(21) + body(payload 156)`。
3. `tsp_cmd_attr_new(false, 0, len)` → `tsp_cmd_header_new(attr, 0x0f7b, phone_bcd, serial_num++, 1, 1)` → `tsp_cmd_make` → `tsp_cmd_frame_make`。
4. `hex_dump` 打印整帧到 stdout，返回帧指针（调用方负责 `free`）。

> **与 0f6b 版的差别（事实）**：`make_tsp_0f6b_frame`（`frame.c:14-57`）除 `cmd_id=0x0f6b`、`version=0x04` 外，逻辑与 0f7b 版几乎一模一样。0f7b 版 `version=0x0`。二者是复制关系。

### 6.4 通用 TSP 帧编码 `src/tsp_cmd`（协议无关的底层框架）
- 头结构 `tsp_cmd_header_t`（`tsp_cmd.h:33-40`）：`cmd_id(2) + cmd_attr(2 位域) + device_id[6] + seq(2) + pack_total(2) + pack_num(2)`。
- `cmd_attr` 位域（`tsp_cmd.h:26-31`）：`msg_len:10 / encrypt:3 / split:1 / res:2`。→ **msg_len 最大 1023**；本用例 `21+156=177`，安全。
- `tsp_cmd_frame_make()`（`tsp_cmd.c:252-293`）：
  - 按小端逐字节铺 `cmd_id/cmd_attr/device_id/seq`（split 时再加 pack_total/pack_num）+ body；
  - 末尾追加 **XOR 校验和**（`tsp_cmd_make_check_sum`，`tsp_cmd.c:64-74`）；
  - **转义**（`tsp_cmd_pton`，`tsp_cmd.c:42-62`）：`0x7e→7d 02`、`0x7d→7d 01`；
  - 首尾各加 `0x7e` 定界符。
- 反向函数齐备：`tsp_cmd_frame_parse`/`tsp_cmd_ntop`/`tsp_cmd_check`/`tsp_cmd_general_resp_make` 等，但**本客户端只发不收**，这些解析/应答函数在当前二进制里未被调用（**推断**：为将来处理平台下行指令预留，或从公共库继承而来）。
- 版权头标注来自 `ROOTCLOUD/irootech`（`tsp_cmd.h:1-14`），是外来通用库。

> **潜在脆弱点（事实陈述）**：`make_tsp_0f7b_frame(void*, char *body, char body_len, int*)` 的长度参数是 **`char` 类型**（`frame.h:18`、`frame.c:59`），而 `client_mosquitto.c:106` 传入的是 `int payload_len = 156`。
> 156 超过 `signed char` 的 127 上限。**在 ARM 上 `char` 默认无符号**，156 能正确表达（≤255），故目标平台上可正常工作；但若 payload 超过 255 字节、或在 `char` 为有符号的平台上编译，`body_len` 会被截断/变负。这是依赖平台 ABI 的隐患，非当前功能缺陷。

---

## 7. 三个工作线程细节

### 7.1 CAN 线程 `src/can_server`
- `start_can_server()`（`can_server.c:100-128`）：作为 **TCP 客户端** `connect` 到本机 `127.0.0.1` 的三个端口 **16002/16003/16004**（分别对应 CAN0/1/2，`can_server.h:8-11`）。即真实 CAN 硬件由**另一个在模组上的桥接进程**接管，本程序不碰 SocketCAN。
- 连接建立函数 `skt_res_new_socket()`（`can_server.c:13-43`）带最多 50 次重试。三路全连上才进入 `can_msg_process()`；任一路断开则关闭全部、外层 `while(true)` 无限重连。
- `can_msg_process()`（`can_server.c:45-98`）：`select()` 超时 1s 轮询三 fd，`read` 最多 1440 字节，按 `sizeof(can_frame_t)=24` 切分成多帧，逐帧调 `parse_common_msg` + `parse_bms_sn_frame`。连续读错 10 次则 `return 0` 触发上层重连。
- **收帧结构 `can_frame_t`（`can_server.h:13-19`，24 字节）**：`can_id(4) + can_dlc(2) + rsv_ms(2) + data[8] + rsv[4]`。这是桥接进程自定义的线格式，**不是内核 `struct can_frame`**。

### 7.2 GPS 线程 `src/gps`
- `start_gps_task()`（`gps.c:14-65`）：TCP 客户端连 `127.0.0.1:16005`，`recv` 定长 `sany_gps_ec21_t`（`gps.h:6-17`，packed，含 `time/lon/lat/nsat/velocity/date/altitude/...` + 116 字节保留），拷进 `can_mng->gps_info`。
- **限流**：虽然每收到就 recv，但**每 5 秒才真正更新一次** `gps_info` 并打印（`gps.c:46-53`）。断连计数 ≥10 次重连。
- `pthread_detach(pthread_self())`——注意主线程仍对它 `pthread_join`（`main.c:74`），detach 后 join 行为未定义，但因线程死循环永不退出，实际不触发。

### 7.3 MQTT 线程 `src/mosquitto/client_mosquitto.c`
- `start_mqtt_task()`（`client_mosquitto.c:45-127`）：`mosquitto_new("tsp_0f6b_client", clean_session=true, can_mng)`，连 `can_mng->tbox_info->local_ip:local_port`（本机 broker），keepalive 60s。
- 用回调维护 `mqtt_status` 状态机（CONNECTING/CONNACK_RECVD/...），`mosquitto_loop_start` 起后台网络线程。
- 主循环（`client_mosquitto.c:98-121`）：连接就绪时每轮
  `memset payload → binary_work_data_generate → make_tsp_0f7b_frame → mosquitto_publish(topic="mqtt_upload_message", qos=0)`，然后 **`sleep(30)`**。
- 发布主题：`TSP_TOPIC_PUB_MSG = "mqtt_upload_message"`（`client_mosquitto.h:4`），QoS 0，非 retain。

---

## 8. 孤立/未编译模块（存在于源码但不进二进制）

| 目录/文件 | 状态 | 依据 |
|---|---|---|
| `src/scene/scene.c` | **未编译**（不在 Makefile glob）。`dlopen("./libscene.so")` 取 `judge_scene` 符号，赋给 `can_mng->judge_scene`——但 **`can_mng_t` 结构体里根本没有 `judge_scene` 字段**（`can_mng.h`），若编译会直接报错。 | `scene.c:34`、`can_mng.h:18-27`、`Makefile:25-36` |
| `src/scene_lib/scene_judge.c` | **未编译**。疑似本应被编译成 `libscene.so` 的 `judge_scene()` 实现。 | `Makefile` glob 未含 |
| `src/log/` | **未编译**。printf 风格的分级日志宏（`SANY_LOG_*`）。全仓库实际用裸 `printf/fprintf`。 | `Makefile` glob 未含 |

**场景（scene）机制现状（事实）**：
- `data_item_t` 里有 `scene[]/interval[]`，`data_process_mng` 有 `cur_scene`，`set_cur_scene()` 也实现了（`data_process.c:1896-1905`）。
- 但**全仓库没有任何地方调用 `set_cur_scene()`**（经检索）。`cur_scene` 在 `can_mng_new` 设为 `"power_on"` 后终生不变。
- 实时读侧 `binary_work_data_generate` **完全没有按场景/间隔做上传节流**——它无条件每 30s 全量打包。
- 因此场景/上传间隔是**已解析但未接线**的设计残留。

**其他第三方 vendored 库**（视为上游原样，除非确证 bug）：
`src/cJSON`、`src/list`（`cc_slist`/`cc_common` 泛型单链表）、`src/opt_iniparser`、`src/skt_res`、`include/uthash.h`、`include/cn-cbor.h`（CBOR，本流程未用到，msg_type=BINARY）、`include/mosquitto.h`。

---

## 9. 命名坑："0f6b" vs "0f7b"（务必留意）

本仓库是早期 `0f6b` 客户端的分叉变体，两套命名并存：

| 类别 | 活跃（0f7b，被调用） | 遗留（0f6b，死代码/未调用） |
|---|---|---|
| payload 结构体 | `data_0f7b_t`（`data_process.h:200`） | `data_0f6b_t`（`data_process.h:90-198`） |
| 打包函数 | `binary_work_data_generate(..., data_0f7b_t*)`（`data_process.c:563`，短，21 信号） | `binary_work_data_generate(..., data_0f6b_t*)`（`data_process.c:665-1885`，被 `#if 0` 包裹，上千行、含 BS1~BS19 位域拼装） |
| 帧构造 | `make_tsp_0f7b_frame`（cmd_id `0x0f7b`） | `make_tsp_0f6b_frame`（cmd_id `0x0f6b`，未被 `client_mosquitto.c` 调用） |
| 命名残留 | — | PID `/tmp/tsp_0f6b_client.pid`、版本文件 `.ver`、MQTT client-id `"tsp_0f6b_client"`、README、`.gitignore` 里的 `tsp_0f6b_client`、连读函数名都叫 `get_0f6b_work_data`/`check_0f6b_data_item`（实为 0f7b 在用） |

**编辑规则**：改 payload/帧时务必确认动的是 **0f7b** 变体；`#if 0` 那段 0f6b 巨型函数是历史实现，除非明确要求，不要动。

> 补充事实：那段 `#if 0` 的 0f6b 代码里含大量 `FIXME`/`TODO` 注释（位宽与平台约定不一致、取值截断等），是早期重卡整车/BMS/电机/变速箱全量信号的位域打包逻辑，信息量远大于当前 0f7b 活跃路径。它保留了这个协议族"完整版"的样貌，可作为字段语义参考，但**不参与运行**。

---

## 10. 运行时的外部文件依赖总表

| 文件 | 用途 | 缺失后果 | 依据 |
|---|---|---|---|
| `/opt/conf.ini` `dev:id` | 设备 ID | **进程退出** | `common.c:69-93` |
| `/opt/sim_info` `dev:msisdn` | SIM 手机号（→ device_id BCD） | **进程退出** | `common.c:95-112` |
| `/opt/conf_ext.ini` `mgw:local_ip/port` | 本机 broker 地址 | 静默回退 `127.0.0.1:1883` | `common.c:114-132` |
| `/tmp/mcu_pwr_voltage` | TBOX 电源电压(mV) | 返回 -1，`Tbox_Volt` 不更新 | `common.c:20-32` |
| `/tmp/mcu_bat_voltage` | 备电电压 | 有读函数但**当前无人调用** | `common.c:34-46` |
| `/tmp/acc_stat` | ACC 状态 | 有读函数但**当前无人调用** | `common.c:48-67` |
| `127.0.0.1:16002/3/4` | CAN0/1/2 桥接 | 无限重连等待 | `can_server.c` |
| `127.0.0.1:16005` | GPS 源 | 无限重连等待 | `gps.c:37` |
| 本机 MQTT broker | 数据上行出口 | 循环重连 | `client_mosquitto.c` |
| `Pointsheet_info1.json`（工作目录相对路径） | 点表 | **进程退出** | `main.c:61` |

> `read_acc_status()`、`read_mcu_battery_voltage_mv()` 已实现但当前调用链中无人使用（**推断**：预留/历史）。

---

## 11. 完整数据流（端到端时序）

```
[真实 CAN 总线]
      │ (模组上另一进程做桥接)
      ▼  TCP 127.0.0.1:16002/3/4，自定义 can_frame_t(24B)
[CAN 线程] can_msg_process
      ├─ parse_common_msg ── HASH_FIND_INT(can_id) ── set_common_data_item
      │                         └→ 写 data_item_t.value_int / time_rcv   (按点表位段解析)
      └─ parse_bms_sn_frame(0x18E1EFF3) → 拼 bms_sn (当前 0f7b 未使用)
                                   │
        [GPS 线程] 127.0.0.1:16005 → can_mng->gps_info (每5s)
                                   │  ← 共享内存(无锁)
                                   ▼  每 30s
[MQTT 线程] binary_work_data_generate
      ├─ Tbox_Volt ← /tmp/mcu_pwr_voltage
      └─ 21× get_0f6b_work_data(信号名, canid|EFF) ── check_0f6b_data_item ── 回填 data_0f7b_t(156B)
                                   ▼
        make_tsp_0f7b_frame: level1头(21B, 含GPS经纬度/时间) + payload
                                   ▼
        tsp_cmd: [cmd_id=0f7b|attr|device_id(手机BCD)|seq++] + body + XOR校验 + 转义(7d/7e) + 7e定界
                                   ▼
        mosquitto_publish("mqtt_upload_message", qos=0)  →  本机 broker  →  TSP 平台
```

---

## 12. 结论性要点（速览）

1. **本质**：CAN→MQTT 的单向遥测上报守护进程，30s 周期、固定二进制协议 `0x0f7b`。
2. **配置驱动**：CAN 信号布局完全由 `Pointsheet_info1.json` 点表决定（本表 slot0、19 个帧、34 个点）；但 payload 结构体字段布局是**手工维护**、需与点表和平台 schema 三方对齐。
3. **实时路径很"薄"**：`data_0f7b_t` 46 字段中仅 22 个被真正填充，其余 24 个（诊断/传输协议/握手类）恒为 0；BMS SN 组装了但没用。相比之下 `#if 0` 的旧 0f6b 路径才是"全量"实现。
4. **命名混乱**：0f6b/0f7b 名称交织，PID/client-id/函数名仍带 0f6b；编辑时须认准 0f7b 活跃变体。
5. **未接线的设计**：scene 场景节流、`src/scene*`、`src/log`、TSP 下行解析/应答、`lock_status`、ACC/备电读取——均为已实现但当前不参与运行的残留。
6. **构建现实**：只能用 `sdk.mk` 指定的 Quectel ARM 交叉工具链编译；`sdk.mk` 路径已按本机修正。无 host 构建、无测试、无 linter。
7. **需留意的脆弱点**：帧构造的 `char body_len` 依赖 ARM 无符号 char；三线程无锁共享信号状态；GPS 线程 detach 后又被 join。以上均为事实描述，非缺陷判定。

---

## 13. 协议族版图（命令号体系）与 md/doc 两份文档的定位

> 本节依据对 `md/doc/` 两份文档抽取文本 + 全仓库检索得出。抽取方式：docx/xlsx 本质是 zip，
> 用 `unzip`+`python3` 取 `word/document.xml`、`xl/sharedStrings.xml`、`xl/workbook.xml` 后去标签。

### 13.1 两份文档分别是什么（事实）

| 文档 | 实质 | 用的命令号 |
|---|---|---|
| 《三一重卡TBOX与TSP平台通讯协议(含电动车).docx》 | **平台"通用"基础协议**（故障/实时/统计/事件/远程诊断/电动车批量采集等标准消息） | `0x0F3A` 故障信息、`0x0F3B` 数据实时上传、`0x0F3C` 统计信息、`0x0F3D` 事件(报警)、`0x0F44` 远程诊断结果、`0x0F4B` 电动车大批量采集(新能源 200 项) |
| 《江山_重卡新能源企标数据功能(0f6b)V1.7_20260305.xlsx》 | **"江山换电"车型的新能源企标规范**（在通用协议之上的客户定制上报） | 主上报 `0x0F6B`（工作表 `S2_新能源企标上报_0x0F6B`，历史 `0F4B→0F6B`）、`0x0F9B` 能耗高密、`0x0F4A` 整车故障码、`0x0F4C` 驾驶行为统计 |

**关键结论**：`md/doc/` 里的两份文档，一份是通用协议、一份是**江山 0f6b** 企标；
**都不是本仓库代码所实现的 `0f7b`（轻卡企标）的字段权威规范**。

### 13.2 "企标上报"命令号是一族以 `B` 结尾的自定义号（事实 + 推断）

在通用协议（`0F3x/0F4x`）之上，各车型/数据类别的**企标上报**用一族命令号，观察到的号呈
`…0F6B、0F7B、0F8B、0F9B … 0FBD` 规律（xlsx 中确证存在 `0F6B` 与 `0F9B`；代码中确证 `0f6b/0f7b`）。
**推断**：每个号对应一个车型项目或一类数据的企标主上报；`0fbd` 是待新增的一路。

### 13.3 命令号在"代码 / 文档"中的实际分布（事实，经全局检索）

| 命令号 | 代码 | md/doc 文档 | 说明 |
|---|---|---|---|
| `0f6b` | 有：`data_0f6b_t` + `make_tsp_0f6b_frame` + `#if 0` 巨型打包函数（**全为死代码，未被调用**） | ✅ 江山 xlsx 的主上报 | 遗留路径 |
| `0f7b` | ✅ **当前唯一在运行的上报**：`data_0f7b_t` + `make_tsp_0f7b_frame` + 活跃 `binary_work_data_generate` | ❌ 两份文档均无 | README 标注"轻卡"；字段规范不在仓库 |
| `0f8b` | ❌ 全仓库检索无 | ❌ 无 | 见 §13.4 待确认 |
| `0fbd` | ❌ 无（待新增） | ❌ 无 | 与"后续加入"一致 |

（检索计数：代码内 `0f6b` 出现 295 处——多为 PID/client-id/函数名等遗留命名；`0f7b` 16 处；`0f8b`/`0fbd` 均 0 处。）

### 13.4 待与同事确认的两点（重要，影响加 0fbd 的参照系）

1. **"现役到底是 0f7b 还是 0f8b"**：交接口述为"主要是 0f6b 和 0f8b"，但**代码里活跃的是 `0f7b`，而 `0f8b` 在代码与文档中均不存在**。需澄清：`0f8b` 是"0f7b"的口误，还是另一个尚未并入本仓库的项目上报。
2. **`0f7b` 载荷的权威字段规范缺失**：`data_0f7b_t` 的字段名（`Tbox_Volt / MCU_MotorTorque / BMS1_BatVoltage`…）在江山 0f6b 规范表里 **0 命中**，说明 0f7b（轻卡）用的是**另一份未入库的规范**。因此本报告 §6 对 `data_0f7b_t` 的描述**只能来自 C 代码本身，未能与平台权威 schema 对齐**。

---

## 14. 新增 `0fbd` 数据上报的落地路径

> 目标：在现有框架内增加一路 `cmd_id = 0x0fbd` 的企标数据上报。
> 前置依赖（**必须先拿到**）：①`0fbd` 的字段定义表（决定 §14 第 1、3 步）；②它对应的 CAN 信号点表；
> ③发布策略（同主题 `mqtt_upload_message`？同 30s 周期？还是独立节奏）。

### 14.1 现有框架已经是"每路上报一套模板"（可复用）
一路上报 = **载荷结构体 + 打包函数 + 帧构造函数 + 在 MQTT 循环里接线**，四件套。0f7b 就是现成范例：

| 环节 | 0f7b 现状 | 位置 |
|---|---|---|
| 载荷结构体 | `data_0f7b_t`（packed，156B） | `data_process.h:200-248` |
| 读侧打包 | `binary_work_data_generate(mng, data_0f7b_t*)` | `data_process.c:563-661` |
| 帧构造 | `make_tsp_0f7b_frame`（cmd_id `0x0f7b`） | `frame.c:59-102` |
| 接线发布 | MQTT 主循环每 30s 调用 | `client_mosquitto.c:98-118` |

### 14.2 落地步骤（按依赖顺序）

1. **点表**：把 `0fbd` 需要的 CAN 信号补进 `Pointsheet_info1.json`（若与 0f7b 是同一批 CAN、可复用现有点；否则新增 `dev/msg/rls/pts`）。加载逻辑（§5.2）无需改动。
2. **定义 `data_0fbd_t`**：在 `data_process.h` 按 0fbd 规范新增 packed 结构体（参照 `data_0f7b_t` 写法）。
   - ⚠️ **务必核对总字节数**：若结构体 > 255 字节，会踩到 §6.3 的 `char body_len` 截断隐患——需同时把 `make_tsp_*_frame` 的 `body_len` 形参由 `char` 改为 `int`（`frame.h`/`frame.c`）。
   - `cmd_attr.msg_len` 是 10 位（≤1023），载荷 + 21B 一级头需 ≤1023。
3. **写 `binary_work_data_generate` 的 0fbd 变体**：复制 0f7b 版，逐字段 `get_0f6b_work_data(mng, &item, "信号名", 0x...)` 回填；多字节记得 `htons/htonl`。建议改名避免与 0f7b 版符号冲突（如 `binary_work_data_generate_0fbd`）。
4. **写 `make_tsp_0fbd_frame`**：复制 `make_tsp_0f7b_frame`，只改 `cmd_id` 为 `0x0fbd`（`version` 字段按 0fbd 规范设定）。
5. **在 MQTT 循环接线**（`client_mosquitto.c`）：在现有 30s 循环内追加一次 `生成→make_tsp_0fbd_frame→mosquitto_publish`。
   - 若发布**主题/周期不同**，需相应扩展循环结构（现循环是单主题、单周期）。
6. **Makefile**：无需改（新代码都落在已被 glob 的 `data_process/`、`frame/`、`mosquitto/` 目录内）。

### 14.3 顺带需要决策/留意的点
- **命名一致性**：现有函数名混用 `0f6b`（如 `get_0f6b_work_data`/`check_0f6b_data_item` 实为通用读取器）。新增 0fbd 时建议不再沿用 `0f6b` 命名，减少后人误解（见 §9）。
- **是否复用 `level1_0f6b_t` 一级头**：0f7b 复用了它（含 GPS 经纬度/时间）。0fbd 若头部定义不同，需要新头结构。
- **未使用能力可按需启用**：若 0fbd 涉及场景化上传频率，可考虑接回 §8 的 scene 机制（目前完全未接线）。

### 14.4 阻塞项（现在就缺）
- ❌ **0fbd 字段规范表**：不在仓库，无法开始第 2、3 步。
- ❌ **0f7b/0f8b 现役归属**：见 §13.4，影响参照哪套做 0fbd。
- ❌ **发布策略**（主题/周期）未知：影响第 5 步。

---

## 15. `md/doc/` 两份文档的完整分析

> 方法：docx/xlsx 均为 zip，用 `unzip`+`python3` 分别抽取 `word/document.xml`（正文，约 8900 行）
> 与 20 个工作表（按 sharedStrings 还原单元格、按行输出）。以下为逐文档/逐工作表梳理，并与代码交叉验证。

### 15.1 文档 A：《三一重卡TBOX与TSP平台通讯协议(含电动车).docx》

**性质**：三一重卡 TBOX↔TSP 平台的**通用基础通讯协议**（基于 JT/T 808 体系扩展），定义 wire 帧结构、
通用消息框架与各标准业务消息。**它是 `src/tsp_cmd` 与一级包的权威来源**。

**§5 帧结构 —— 与代码 `tsp_cmd.c` 逐项交叉验证，完全一致（强证据）：**

| 协议规定（docx） | 代码实现 | 结论 |
|---|---|---|
| 消息=标识位+消息头+消息体+校验码，首尾 `0x7e` 定界（454） | `tsp_cmd_frame_make` 首尾加 `0x7e`（tsp_cmd.c:287-289） | ✅一致 |
| 转义：`0x7e→0x7d 0x02`、`0x7d→0x7d 0x01`；示例 `30 7e 08 7d 55`→`7e 30 7d02 08 7d01 55 7e`（464-469） | `tsp_cmd_pton`/`tsp_cmd_ntop`（tsp_cmd.c:9-62） | ✅逐字节一致 |
| 收发序：封装→算校验→转义 / 转义→验校验→解析（467） | `frame_make`/`frame_parse` 顺序一致（tsp_cmd.c:252-293,118-175） | ✅一致 |
| 消息体属性：低 10 位长度、bit10-12 加密、bit13 分包（526-534） | `tsp_cmd_attr_t` 位域 `msg_len:10/encrypt:3/split:1/res:2`（tsp_cmd.h:26-31） | ✅一致 |
| 分包时消息头含 总包数+包序号，否则无（500,534,545） | `pack_total`/`pack_num` 仅 `split` 时读写（tsp_cmd.c:156-164,269-274） | ✅一致 |
| 校验码=从消息头起逐字节异或至校验码前一字节（551-552） | `tsp_cmd_make_check_sum` XOR（tsp_cmd.c:64-74） | ✅一致 |
| 消息头含命令标识、消息体属性、设备号、流水号（471-494） | `tsp_cmd_header_t`（cmd_id/attr/device_id[6]/seq）（tsp_cmd.h:33-40） | ✅一致 |

**docx 定义的消息 ID 目录（业务消息）**：`0x0001` 通用应答、`0x0100` 终端注册、`0x0002` 心跳、
`0x0003` 注销、`0x0F3A` 故障信息、`0x0F3B` 数据实时上传、`0x0F3C` 统计信息、`0x0F3D` 事件(报警)、
`0x0F44` 远程诊断结果、`0x0F4B` 电动车大批量采集(新能源 200 项)、`0x8F41` 远控设置、
`0x8F51` 远控查询、`0x8F42` VIN/版本。此外含车辆控制、远程诊断、电子围栏、重大事故、ADASIS 等章节。

**一级包交叉验证**：`0x0F4B` 电动车批量采集的一级包结构（版本 / 纬度 DWORD / 经度 DWORD / 高程 WORD /
方向 WORD / 时间 BCD[6] / 二级数据包个数）与代码 `level1_0f6b_t`（frame.h:6-15）**逐字段对应**。

### 15.2 文档 B：《江山_重卡新能源企标数据功能(0f6b)V1.7_20260305.xlsx》

**性质**：具体车型（初为"江山换电车"，后扩展为"全平台树根 TBOX 车辆"）的**新能源"企标"数据功能落地规范**，
即在通用协议之上的客户定制上报。当前 **V1.7 / 2025-03-06**。共 20 个工作表：

| 工作表 | 行 | 内容/作用 |
|---|---|---|
| 版本履历 | 21 | 20210705→20250306 演进；主上报报文经历 **`0F4B`→`0F6B`** 改名；应用范围由江山换电扩到全平台 |
| 变更记录 | 24 | 关键沿革：`0F4B→0F6B`；24V 电瓶电压借用"后轴2相对速度"两字节；`0x18FF08D0→0x18FF09D0`；故障 `0F3A→0F4A`、统计 `0F3C→0F4C`；20240829 字段精简；v1.7 集成热管理 |
| 功能清单 | 72 | 功能项与状态：GB32960 法规数据、车况/报警/自定义/补发/存储、实时(见 S2)、电池、远控(见 S7)、故障码(见 S3)、统计(见 S5)、换电、金融锁车、授时、VIN、版本… |
| URL白名单 | 15 | 平台/升级域名白名单 |
| **S1_江山换电整车通讯协议** | 1627 | 整车 **CAN 信号矩阵（DBC 级）**：各 ECU 报文/信号定义，是企标字段的 CAN 来源底表 |
| **S2_新能源企标上报_0x0F6B** | 360 | **核心**：企标主上报字段规范。一级包头 + **约 318 条信号**（英文名/中文/CAN id/位/控制器/起始字节位/长度/精度/偏移/单位/枚举）；上传 ID 列多为 `0x0F4B`（历史）；含 ADAS(HWA/LDW/FCW/紧急制动) 与 2026 出口(卢旺达)新增横向加速度 |
| 0x0F9B_能耗高密数据上传 | 129 | 高频能耗子集（车速/里程/电机转速扭矩/母线电压电流/电池电流电压/DCDC…），用于高密采样能耗监控 |
| S3_0x0F4A_整车故障码上传 | 67 | 故障上报：一级包 + 故障信息项列表（类别/长度/故障信息/冻结帧），历史 `0F3A→0F4A` |
| S4_TBOX故障码表 | 11 | TBOX 自身故障码定义 |
| S5_驾驶行为统计信息_0x0F4C | 137 | 行程统计（行程起止时间/积分里程/GPS 里程/积分电耗/总电耗/载重…），历史 `0F3C→0F4C` |
| S6_终端CAN发送 | 33 | TBOX 主动发到整车 CAN 的报文（授时 `0x18FEE6FC`、版本号 `0x18FF006FC` 等） |
| S7_远控功能 | 24 | 远程指令（开锁/车窗/寻车/远近光/巡检/WIFI/设置 VIN）走 `0x8F41/0x8F42/0x8F51`，含下发 CAN 与应答 |
| S8_换电控制 | 30 | 换电站/锁止机构/换电连接器状态与控制 CAN |
| S9_外发版本辅助说明 | 13 | 版本编码辅助 |
| S10_TBOX用到的ID_仅供参考 | 64 | CAN ID 汇总 |
| S11_金融锁车换电CAN数据 | 58 | 金融锁车相关 CAN |
| S12_软件版本号上传及车架号请求 | 63 | `0F42` VIN 请求/版本上传，13 位版本编码规则 |
| S13_其它需求汇总 | 3 | 杂项需求 |
| S14_注册登录鉴权 | 22 | 注册 `0x0100`/注销 `0x0003`/心跳 `0x0002`，省域/市县/制造商 ID/终端型号/终端 ID（808 体系） |

**S2 字段规范 ←→ 代码交叉验证（关键结论）**：
- S2 的字段英文名（`Target_Velocity / Target_Torque / BMS6_BattCoolingInTemp / F_Axis_Velocity /
  FL_Velocity / Total_Mileage / Velocity / Torque / VCU_SOC / Battery_Current / Battery_AveTemp /
  Cell_Min_Temp …`）与代码 **`data_0f6b_t`（data_process.h:90-198）及 `#if 0` 的 0f6b 打包函数逐字段吻合**。
  ⇒ **这份 xlsx 是"遗留 0f6b 路径"的权威字段规范。**
- 反向：现役 `data_0f7b_t` 的字段（`Tbox_Volt / MCU_MotorTorque / MCU_RunningSt / DCDC1_DTCCode …`）
  在 S2 中 **0 命中** ⇒ **现役 0f7b（轻卡）用的是另一套规范，不在本仓库。**

### 15.3 两份文档与代码的定位小结

| 文档 | 权威覆盖的代码对象 | 未覆盖 |
|---|---|---|
| docx 通用协议 | `src/tsp_cmd`（帧封装/转义/校验/属性位域）、`level1_0f6b_t` 一级包、`0F4B` 电动车批量采集范式 | 企标载荷具体字段 |
| xlsx 江山 0f6b | `data_0f6b_t`（遗留）+ `#if 0` 打包逻辑的字段语义 | **现役 `data_0f7b_t`、待加 `0fbd`** |

⇒ **现役 0f7b（轻卡）与待新增 0fbd 的字段权威规范，这两份文档都不含**（见 §13.4、§14.4）。

---

## 16. 覆盖范围与边界（本报告自评）

### 16.1 已核实覆盖（事实级）
- **一手（自研）代码**：13 个 `.c` 全部通读；唯一跳读的 `data_process.c:1016-1419` 经 `awk` 确认整段在 `#if 0`(665)…`#endif`(1885) 内，为死代码。
- **结构体大小**：用宿主 `gcc` 编译验证 `level1_0f6b_t=21`、`data_0f7b_t=156`、帧体 `=177` 字节（纯 packed 类型，不依赖 SDK）。
- **未编译模块**：`log/scene/scene_lib` 三目录经 grep 确认不在 Makefile glob；内容均已读。
- **两份文档**：均已结构化提取并逐表/逐章梳理（§15）；docx 帧结构与 `tsp_cmd.c` 逐项交叉验证一致；xlsx `S2(0F6B)` 与 `data_0f6b_t` 逐字段吻合。

### 16.2 主动未深审（已声明，非遗漏）
- **第三方 vendored 库**（`cJSON / cc_slist / opt_iniparser / uthash / cn-cbor / mosquitto`）未逐行审计，按上游原样对待。
- **`#if 0` 旧 0f6b 千行打包逻辑**读了结构与代表性字段（BS1~BS19 位域、FIXME），未逐字段核对（死代码）。

### 16.3 仓库内**无法闭合**的边界（需外部文档或运行环境）
1. **现役 `0f7b`（轻卡）载荷的字段权威规范不在仓库** → §6 对 `data_0f7b_t` 的描述只能源自 C 代码，未能与平台 schema 对齐（xlsx 是 0f6b 规范，已证不适用）。
2. **`0fbd` 字段规范不存在**（待新增，见 §14.4）。
3. **"现役 0f7b vs 口述 0f8b"归属**需同事确认（§13.4）；`0f8b` 在代码与两份文档中均无。
4. **仅运行期/对端可证的行为**：桥接 `frame.can_id` 是否带扩展帧标志、`can_frame_t` 线格式、GPS 源与 broker 实际行为、平台是否接受那 24 个恒为 0 的占位字段——静态分析给不了定论，报告中均标为推断。

---

## 17. 结构与流向图集

> 本节补齐 §11（端到端数据流）之外缺失的结构图/调用链，均据已读代码绘制，标注 `文件:行`。

### 17.1 分层 / 模块依赖图

```
                              main.c  (入口/单实例/建线程)
                                 │
                    ┌────────────┼───────────────────────────┐
                    ▼            ▼                            ▼
          [协调层] can_mng ── can_mng_t 总上下文(3线程共享) ──┐
                    │  tbox_info(身份/broker) · gps_info · data_process_mng
                    │                                          │
   ┌────────────────┼───────────────────┐                     │
   ▼(CAN线程)        ▼(GPS线程)          ▼(MQTT线程)           │
 can_server        gps            mosquitto/client_mosquitto  │
   │ skt_res         │ skt_res          │                     │
   └──────┬──────────┘                  │                     │
          ▼                             ▼                     ▼
   [数据核心] data_process  ◄───────────┴───── 读侧 ──► [协议] frame ─► tsp_cmd
     规则树/位段解析/打包                                level1头(21B)   wire帧封装
          │  ▲                                                              │
     写侧 │  │ 建树                                                          ▼
          │  └──── load_pointsheet_json ◄── Pointsheet_info1.json      mosquitto_publish
          ▼
   [支撑] common(身份/BCD/时间/hexdump) · list(cc_slist) · cJSON · opt_iniparser · uthash

   [孤立/未编译] scene · scene_lib · log        [遗留/死代码] data_0f6b_t · make_tsp_0f6b_frame · #if 0 打包
```

### 17.2 线程模型与共享上下文

```
 main() ──creates──►  can_tid   : start_can_server ─┐
              (sleep1)  gps_tid  : start_gps_task    ├─ 均传入同一 can_mng*  (main.c:67-71)
              (sleep1)  mqtt_tid : start_mqtt_task  ─┘
   │
   └─ pthread_join ×3 (三线程皆死循环，正常不返回)

 共享写/读（无锁）：
   CAN线程 ── 写 ──►  data_process_mng.data_rule[]  的 data_item_t.value_*  ◄── 读 ── MQTT线程(每30s)
   GPS线程 ── 写 ──►  can_mng.gps_info               ◄── 读 ── MQTT线程(帧头经纬度)
```

### 17.3 运行时数据结构关系（规则树）

```
 data_process_mng_t                                    (data_process.h:67-81)
  ├─ data_rule[0..2]  每CAN通道一张 uthash 表 (键 = CAN id)
  │     │
  │     ▼ (HASH_FIND_INT)
  │   data_rule_t ──┐  hh(uthash)                       (data_process.h:32-40)
  │     type/offset/length/value(键)
  │     │  「容器二选一」
  │     ├─ data_sub_rule_list : CC_SList<data_rule_t>  ── 递归子规则(匹配帧内子字段)
  │     └─ data_item_list     : CC_SList<data_item_t>  ── 叶子(具名信号)
  │                                  │
  │                                  ▼
  │                             data_item_t             (data_process.h:42-65)
  │                               name/offset/length/byte_order/sign/scale/offset
  │                               value_int / value_double / value_string  ← 活值
  │                               time_rcv / time_pub · scene[]/interval[]
  ├─ scene[] · cur_scene(恒"power_on") · scene_num
  └─ bms_sn[24] · bms_frame_recv_flag(收全=0x0F)

 can_mng_t (can_mng.h:18-27): data_process_mng · can_fd[3]/can_rcv_cnt[3] ·
            tbox_info(dev_id/local_ip:port/phone_number/phone_bcd[6]) · gps_info · serial_num · cur_scene
```

### 17.4 点表 JSON → 规则树 建树映射

```
 Pointsheet_info1.json                 load_pointsheet_json / get_common_data_rule (data_process.c:360,114)
   dev[] {slot,typ}         ─────►  选定 data_rule[slot] 这张哈希表
     msg[] {typ,ofs,len,val} ────►  data_rule_t (val"#8CFF0204"→value=0x8CFF0204, HASH_ADD_INT)
       rls[] {…,val"8"}      ────►  data_sub_rule_list 里的子 data_rule_t (递归)
         pts[] {nm,ofs,len,  ────►  data_item_list 里的 data_item_t (叶子)
                bo,sc,pofs,sign,
                sce,scems}
```

### 17.5 写侧调用链（CAN 线程，持续）

```
 start_can_server (can_server.c:100)
   └─ skt_res_new_socket ×3  (16002/3/4, 各重试50次)
   └─ can_msg_process (can_server.c:45)  ── select(1s) 轮询3fd ─ read(≤1440B) ─ 按24B切帧
        每帧:
        ├─ parse_common_msg(mng,&f,f.can_id,chn)      (data_process.c:348)
        │    └─ HASH_FIND_INT(data_rule[chn], can_id)
        │         └─ set_common_data_item(rule,msg)   (递归, data_process.c:261)
        │              └─ 位段解析(大/小端移位+掩码) ─► data_item.value_int/value_string · time_rcv
        └─ parse_bms_sn_frame(mng,&f)                 (0x18E1EFF3, data_process.c:1941)
             └─ 拼 bms_sn[24] · 置 bms_frame_recv_flag   (当前 0f7b 未使用其结果)
```

### 17.6 读侧 + 打包 + 发布调用链（MQTT 线程，每 30s）

```
 start_mqtt_task (client_mosquitto.c:45)  ── connect(local_ip:port) ─ loop_start ─ 每30s:
   ├─ binary_work_data_generate(mng,&payload_0f7b)          (data_process.c:563) → 返回156
   │    ├─ read_mcu_power_voltage_mv() ─► payload.Tbox_Volt
   │    └─ 21× get_0f6b_work_data(mng,&item,"信号名",canid)  (data_process.c:478)
   │          └─ HASH_FIND_INT(canid|CAN_EFF_FLAG)
   │               └─ check_0f6b_data_item(rule,item,name)   (按名递归, data_process.c:438)
   │                    └─ item.value_int ─► payload.<字段> (htons/htonl)
   ├─ make_tsp_0f7b_frame(can_mng,&payload,156,&len)         (frame.c:59)
   │    ├─ level1_0f6b_t: version=0 · lat/lon=gps×1e6(htonl) · date=BCD本地时
   │    ├─ tsp_cmd_attr_new(split=0,enc=0,msg_len=177)
   │    ├─ tsp_cmd_header_new(cmd_id=0x0f7b, device_id=phone_bcd, seq=serial_num++)
   │    ├─ tsp_cmd_make → tsp_cmd_frame_make: 逐字节铺头+体 → XOR校验 → 转义(7e/7d) → 首尾0x7e
   │    └─ hex_dump 打印
   └─ mosquitto_publish("mqtt_upload_message", frame, qos=0)
```

### 17.7 最终 wire 帧字节布局

```
 ┌────┬──────────── 转义前的命令内容(参与XOR校验) ─────────────┬────┐
 0x7e │ cmd_id │ cmd_attr │ device_id[6] │ seq │ {分包4B?} │ 消息体 │ XOR│ 0x7e
      │ 2B     │ 2B       │ 手机号BCD     │ 2B  │ split=0→无 │  177B │ 1B │
      │ 0F 7B  │msg_len:10/enc:3/split:1/res:2                          │
                                                     │
                          消息体(177B) = ┌ level1_0f6b_t (21B) ┬ data_0f7b_t (156B) ┐
                                          version(1) l1pkg(1)     Tbox_Volt … TP_DT2
                                          lat(4BE) lon(4BE)       (§6.1，22字段有值/24字段恒0)
                                          alt(2) dir(2) date[6]BCD
                                          l2pkg(1)
 说明: 整段 [cmd_id..XOR] 中的 0x7e→7d02、0x7d→7d01 转义后, 再包 0x7e 定界 (tsp_cmd.c:252-293)
 (cmd_id/attr/seq 上线为大端: 0x0f7b→字节 0F 7B)
```

---

## 18. 函数级功能清单（"代码功能"全览）

> 逐文件列出每个函数与一句话职责，并标注运行态：**[现役]** 在跑 / **[未用]** 已实现但无人调用 /
> **[死码]** `#if 0` 或未编译 / **[孤立]** 目录未进 Makefile。第三方 vendored 库不展开。

### main.c
| 函数 | 职责 | 态 |
|---|---|---|
| `is_instance_existing` | flock `/tmp/tsp_0f6b_client.pid` 做单实例 | 现役 |
| `set_app_ver` | 写 `APP_VER=8` 到 `.ver` 文件 | 现役 |
| `main` | 建上下文→加载点表→起3线程→join | 现役 |

### can_mng/can_mng.c
| `check_phone_number` | msisdn 归一化为 11 位 | 现役 |
| `can_mng_new` | 读身份/配置、建 `can_mng_t`、手机号转 BCD | 现役 |

### common/common.c
| `read_mcu_power_voltage_mv` | 读 `/tmp/mcu_pwr_voltage` | 现役(Tbox_Volt) |
| `read_mcu_battery_voltage_mv` | 读 `/tmp/mcu_bat_voltage` | 未用 |
| `read_acc_status` | 读 `/tmp/acc_stat` | 未用 |
| `sany_read_device_id` | `/opt/conf.ini`→dev_id | 现役 |
| `sany_read_sim_info` | `/opt/sim_info`→msisdn | 现役 |
| `sany_read_device_conf` | `/opt/conf_ext.ini`→broker地址(缺省回退) | 现役 |
| `phone_number_to_bcd` / `char2bcd` / `bcd_decimal_code_simple` | 手机号/时间转 BCD | 现役 |
| `get_local_time` / `get_localtime_string` | 本地时间→6字节BCD(帧头date) | 现役 |
| `hex_dump` | 十六进制打印整帧 | 现役 |

### can_server/can_server.c · skt_res/skt_res.c
| `skt_res_new_socket` | 连本机CAN桥接端口(重试50次) | 现役 |
| `can_msg_process` | select轮询3路→切帧→分发解析 | 现役 |
| `start_can_server` | CAN线程主体+无限重连 | 现役 |
| `skt_res_new_socket_ip` | 按IP:端口建TCP(GPS用) | 现役 |
| `skt_res_new_socket_bak` | 非阻塞版socket | 未用 |

### gps/gps.c
| `start_gps_task` | GPS线程:连16005、每5s更新gps_info | 现役 |

### data_process/data_process.c
| `OneCharToHex`/`TwoCharToHex`/`CharToHexMem` | 十六进制字符串→字节(解析val) | 现役 |
| `big_endian` | 字节反序 | 现役 |
| `get_tick_count` | 单调毫秒时钟 | 现役 |
| `get_file_data` | 读文件到内存 | 现役 |
| `get_common_data_rule` | 递归建规则/叶子(建树) | 现役 |
| `set_common_data_item` | 递归写:位段解析→data_item值 | 现役(写侧) |
| `parse_common_msg` | 按can_id查表并写入 | 现役(写侧) |
| `load_pointsheet_json` | 解析点表→规则树 | 现役 |
| `check_0f6b_data_item` | 按信号名递归取叶子值 | 现役(读侧) |
| `get_0f6b_work_data` | 按 canid\|EFF 查表取具名信号 | 现役(读侧) |
| `check_lock_status` | 锁车状态→active/level | 死码(仅#if0调用) |
| `binary_work_data_generate`(→`data_0f7b_t`) | 读侧打包22字段 | **现役** |
| `binary_work_data_generate`(→`data_0f6b_t`) | 旧全量打包(千行) | 死码(#if0) |
| `data_process_mng_init` | 建管理结构 | 现役 |
| `set_cur_scene` | 切场景 | 未用(无人调) |
| `print_data_rule` | 调试打印规则树 | 未用 |
| `parse_bms_sn_frame` | 拼BMS序列号(0x18E1EFF3) | 现役但结果未被0f7b使用 |

### mosquitto/client_mosquitto.c
| `mosq_connect_callback`/`mosq_disconnect_callback` | 连接状态机回调 | 现役 |
| `start_mqtt_task` | MQTT线程:连broker→每30s生成并发布 | 现役 |

### frame/frame.c
| `make_tsp_0f7b_frame` | 一级包+调tsp_cmd构帧(cmd 0x0f7b) | **现役** |
| `make_tsp_0f6b_frame` | 同上(cmd 0x0f6b) | 未用 |

### tsp_cmd/tsp_cmd.c（wire协议，与 docx §5 一致）
| `tsp_cmd_pton`/`tsp_cmd_ntop` | 转义/反转义(7e/7d) | 现役(pton) |
| `tsp_cmd_make_check_sum`/`tsp_cmd_check` | XOR校验 生成/校验 | 现役(生成) |
| `tsp_cmd_attr_new`/`_del` · `tsp_cmd_header_new`/`_del` · `tsp_cmd_make`/`_dup`/`_del` | 属性/头/命令 构造与释放 | 现役(除dup) |
| `tsp_cmd_frame_make` | 头+体+校验+转义+定界→wire帧 | **现役** |
| `tsp_cmd_frame_parse` · `tsp_cmd_get_*` · `tsp_cmd_general_resp_make`/`_format` | 解析/取字段/通用应答(下行) | 未用(只发不收) |

### 孤立/未编译（不进二进制）
| `scene.c: load_libscene` | dlopen libscene.so 取 judge_scene | 孤立(且引用不存在的字段) |
| `scene_judge.c: judge_scene/calc_value/MakeMaskValue` | 按引擎转速/车速判场景 | 孤立(应为libscene.so) |
| `log.c: sany_log_*` | 分级日志实现 | 孤立(全程用裸printf) |

---

## 19. 缺陷修复记录（working tree，未提交）

> 状态：以下 10 处修改已应用到工作区；四个改动的 `.c` 文件经宿主 `gcc -fsyntax-only -Wall` 通过、**无新增告警**
> （唯一的 `%lld` 告警是原有的、宿主与目标 `int64_t` 定义差异所致，与本次无关）；**未做完整交叉编译**
> （需 Quectel SDK）、**未 `git commit`**。行号为修改前定位，可按函数名检索。

### 19.1 修复清单

| 档 | 位置 | 缺陷 | 修法 |
|---|---|---|---|
| A1 | `data_process.c:634` `binary_work_data_generate`(0f7b) | `ABS1_VehicleSpeed`(WORD) 误用 32 位 `ntohl` | 改 `htons` |
| A2 | `can_server.c:78` `can_msg_process` | `read()==0`(对端EOF) 未处理→空转+不重连 | `if(ret<0)`→`if(ret<=0)` |
| A3 | `gps.c:57` `start_gps_task` | 重连前未 `close` 旧 fd→泄漏 | 置 `-1` 前加 `close(sockfd)` |
| D1 | `data_process.c:551` `check_lock_status` | `case 0x0D` 漏 `break` 穿透 | 补 `break;` |
| D2 | `gps.c:16` `start_gps_task` | `pthread_detach` 后又被 `join`(UB) | 删除 `pthread_detach` |
| B1 | `frame.h:17-18` / `frame.c:14,59` | 帧长形参 `char`(载荷156>127) | `char`→`int`(0f6b/0f7b 均改) |
| B2 | `data_process.c:270` `set_common_data_item` | CAN 字节按 `char` 读→可能符号扩展 | `char*`→`const uint8_t*` |
| B3 | `data_process.c:311` `set_common_data_item` | `memcpy datelen` 可越界写 `value_int` | 按 `sizeof(value_int)` 钳位 |
| — | `data_process.c:280` `set_common_data_item` | `UINT64_MAX>>(64-len)` 在 `len==0/>64` 是移位 UB | 加位长守卫 `continue` |
| D4 | `data_process.c:428` `load_pointsheet_json` | `HASH_ADD_INT` 可能解引用 NULL | 建规则失败 `continue` |

### 19.2 修复的性质分级（重要：并非都在"修正正在发生的错误"）

| 性质 | 条目 | 说明 |
|---|---|---|
| **① 确认必现 bug** | **A1 · A2 · A3** | 当前硬件+数据下**正在产生错误**：车速恒 0 / EOF 时 CPU 空转且不重连 / GPS 每次重连泄漏 fd。证据为数值级或逻辑级可证。 |
| **② 确凿缺陷但当前无害** | D1 · D2 | D1 在 `#if 0` 死代码内不参与运行；D2 的 join-on-detached 属 UB，但线程死循环永不退出，实际不触发。 |
| **③ 防御性加固（当前不触发）** | B1 · B2 · B3 · mask守卫 · D4 | 均为"能工作但脆弱/依赖平台假设"：B1/B2 靠 **ARM `char` 无符号**侥幸正确；B3/mask 因当前点表**无 >32 位或 0 位信号**不触发；D4 仅 `calloc` 失败时触发。属健壮性提升，非修复观测到的故障。 |

> 若需**改动最小化**，可只保留 ① 档（A1/A2/A3，+可选 D1/D2），回退 ③ 档 5 处。

---

## 20. 死代码（`#if 0` 旧 0f6b 打包）逐字段扫描结果

> 对 `data_process.c:665-1885` 的遗留 `binary_work_data_generate(data_0f6b_t*)` 做与 A1 同法的机械核对。
> **均属死代码**，当前不参与运行；仅当重新激活 0f6b 路径才需处理。

- **字节序/位宽一致性：干净。** 33 处 `htons` 全部对应 WORD 字段、14 处 `htonl` 全部对应 DWORD 字段，**无 A1 类错配**，且整段**无 `ntohl/ntohs` 误用** → A1 是 0f7b 新路径独有的缺陷。
- **新发现（疑似字节序漏转换）**：`DCM3_DoorOpenSts`、`GLC_value`（均为 16 位 WORD 位打包字段）**全程未套 `htons`**，而同类位打包字段 `BS2/BS16/BS17` 都在末尾 `htons`；这两个字段掩码跨到 bit15、横跨两字节，漏转换会导致字节序与兄弟字段不一致（`data_process.c:1827-1836,1871-1879`）。均为"2024-07-07 增加"的字段。
- **原作者已 `FIXME` 标注的位宽截断**：`InstancePwrConsume`(报文12位→上报8位)、`BMS15_BattVolt_Inner`(16→12)、`BMS15_TMSPwrDwnReq`(2→1)、`BMS1_stMode`(4→3)、`BMS9_Cell_Temp_Low` 等，属已知的"报文位宽 vs 上报位宽"取舍，非疏漏。
- **位域结构体可移植性**：`data_0f6b_t` 的 `IC_WheelDiffLockSt:2`/`ACC_State:2` 等位域及 `BS9[3]`/`BS13[3]` 三字节字段，其内存排布依赖编译器 ABI（当前单一交叉编译器下自洽）。

---

## 21. 需真机 / 对端才能验证的假设点（静态分析给不了定论）

> 这些是代码**隐含依赖但仓库内无法证实**的前提；报告正文凡涉及处均已标"推断"。按契约方分组。

**A. 与 CAN 桥接进程的契约**
1. **（最关键）** 桥接送来的 `frame.can_id` **带 CAN 扩展帧标志位(bit31)**——写侧用原始 `can_id` 做哈希键，而键值（如 `0x8CFF0204`）顶位已置 1；若桥接给的是不带标志的 29 位 ID，则 `HASH_FIND` 全部落空 → **所有信号恒为初值、报文全 0**。
2. `can_frame_t`（24B：`can_id/dlc/rsv_ms/data[8]/rsv[4]`）**与桥接发送的记录逐字节一致**；若是内核 16B `struct can_frame` 或别的排布，切帧与 `data[]` 偏移全部错位。
3. 端口 `16002/16003/16004` ↔ CAN0/1/2 的映射正确。

**B. 与 GPS 进程的契约**
4. `sany_gps_ec21_t`（packed，含 116B 保留）与 GPS 源记录逐字节一致；`lat/lon` 为十进制度（代码 ×1e6）。

**C. 与 TSP 平台的契约（最需规范/对端确认）**
5. 平台按"**原始总线值 + 平台侧套精度**"解析——客户端的 `param_scale/param_offset/sign` 全程未应用（对应 §16.3 的 C2）。
6. `IC_TotalMailage` 期望"**×0.005 后的整数 km**"而非原始值（对应 C1）。
7. 平台接受 `data_0f7b_t` 中 **24 个恒为 0 的占位字段**（诊断/BAM/握手类），视其为保留而非缺失（见 §5.5）。
8. 一级包 `version=0x0`（0f7b）、`altitude/direction=0` 为平台可接受。
9. `device_id` = 手机号后 11 位转 6 字节 BCD 的规则与平台注册一致。
10. 各信号 `bo`（字节序）标注与实际总线编码一致；位段解析（尤其大端分支 `>>(8*size-length-start_bit)`）只有用真实报文能验证。

**D. 与运行平台/环境**
11. 目标 ARM 上 `char` 无符号（B1/B2 修复后已改为显式类型，历史上依赖此）。
12. 设备时区为 GMT+8（`get_localtime_string` 直接取本地时，协议要求 GMT+8）。
13. `/tmp/mcu_pwr_voltage` 单位为 mV。
14. 本机 MQTT broker 会把 `mqtt_upload_message` 主题的帧转发到 TSP 平台；30s 周期被接受。
15. 启动工作目录含 `Pointsheet_info1.json`（相对路径依赖，见 §D6）。

---

*（本报告只做静态阅读与文档抽取，未执行目标程序或联网；量化数字来自对源码/JSON 的检索统计；结构体大小经宿主 gcc `sizeof` 验证；§15 结论来自对 `md/doc/` 两份文档的结构化提取与逐项代码交叉验证；§17 图集与 §18 函数清单据已读源码绘制并标注 `文件:行`；§19 记录的代码修改在 working tree、经宿主语法检查、未交叉编译、未提交；§20/§21 为死代码扫描与运行期假设梳理。）*
