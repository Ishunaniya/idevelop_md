# tsp_0f7b_client 全源码分析报告

- 分析日期：2026-07-02
- 分析对象：当前工作区（含未提交修改，以工作区实际内容为准）
- 分析目的：以「发现问题」为核心，覆盖内存/线程安全/资源泄漏/协议打包/健壮性/逻辑/构建等类别
- 出处标注约定：所有结论给出 `文件:行号`；无法从代码直接确证者标注「推断/需验证」

---

## 一、仓库与构建总览

### 进程结构
`main.c` 单实例守护进程：通过 flock 锁 `/tmp/tsp_0f6b_client.pid` 保证单实例，写版本号到 `/tmp/tsp_0f6b_client.ver`，随后：
1. `can_mng_new()` 构造共享上下文 `can_mng_t`（读设备 id / SIM / 本地 MQTT 配置，生成 BCD 手机号）。
2. `mosquitto_lib_init()` + `load_pointsheet_json()` 解析点表到规则树。
3. 创建三线程并 `pthread_join`（永不返回的 run-forever 守护）：
   - CAN 线程（`src/can_server`）：TCP 客户端连三个本地端口 16002/16003/16004，`select` 读 `can_frame_t`，交给 `data_process` 更新信号。
   - GPS 线程（`src/gps`）：TCP 客户端连 `127.0.0.1:16005`，拷贝 `sany_gps_ec21_t` 到 `can_mng->gps_info`。
   - MQTT 线程（`src/mosquitto`）：连本地 broker，每 30s `binary_work_data_generate()` 快照信号 → `make_tsp_0f7b_frame()` 封帧 → 发布。

三线程共享同一个 `can_mng`（含 `data_process_mng`、`gps_info`）。**全局未见任何互斥锁 / 原子操作**（下详）。

### 整体架构图

> 下列各图为纯文本图，任意编辑器/终端可直接查看。图中 ⚠ 标注对应第三节问题编号，位置均已核对源码。

```text
[CAN 桥接进程]                          [GPS 提供进程]
127.0.0.1:16002/16003/16004             127.0.0.1:16005
（CAN0/1/2 三通道，外部进程）             （外部进程）
      │                                       │
      │ TCP 连接                              │ TCP 连接
      │ skt_res_new_socket                    │ skt_res_new_socket_ip
      │ （can_server.c:13）                    │ （skt_res.c:60）
      │ ⚠P02 重试 break 位置错误，              │ ⚠P01 connect 失败不 close，
      │   :36 失败路径同样不 close              │   反复重连泄漏 fd
      ▼                                       ▼
[CAN 线程 src/can_server]               [GPS 线程 src/gps]
select 三个 fd 读 can_frame_t            读 sany_gps_ec21_t
      │                                       │
      │ 写 value_int / time_rcv               │ 写 gps_info
      └─────────────────┬─────────────────────┘
                        ▼
        [共享状态 can_mng_t]
        data_process_mng 规则树 + gps_info
        ⚠P03 全仓库无任何互斥锁/原子操作（grep 确认零命中）
                        │
                        │ 读（多字段快照非原子 → 撕裂读风险）
                        ▼
        [MQTT 线程 src/mosquitto]
        每 30s：快照 → 封帧 → publish TSP_TOPIC_PUB_MSG
                        │
                        ▼
        [本地 Mosquitto broker]（外部进程）
        conf_ext.ini 的 mgw:local_ip/port（缺省回退 127.0.0.1:1883）
```

进程本体：`main.c`，flock `/tmp/tsp_0f6b_client.pid` 单实例；三线程共享同一 `can_mng`，run-forever。

### 数据流水线
点表 `Pointsheet_info1.json`（`dev[] → msg[] → rls[] → pts[]`）→ `load_pointsheet_json()` 解析为 `data_rule_t` 树（每 CAN 通道一个 uthash，key=`value`）+ 叶子 `data_item_t`。写侧：CAN 帧到达 → `parse_common_msg()` 递归匹配写入 `value_int/value_double/time_rcv`。读侧：MQTT 线程 `binary_work_data_generate()` 取值填入 `data_0f7b_t`（`__attribute__((packed))` 固定布局），`make_tsp_0f7b_frame()` 加 `level1_0f6b_t` 头后交 `tsp_cmd` 封装为 0x0f7b 帧。

### 数据流水线图（点表 → 规则树 → 写侧/读侧 → 线上帧）

```text
【点表加载 —— 启动时执行一次（main.c）】

Pointsheet_info1.json（dev[] → msg[] → rls[] → pts[]，34 个点，均 ptyp=1、len≤32）
    │
    │ load_pointsheet_json()（data_process.c）
    ▼
data_rule_t 规则树：每 CAN 通道一个 uthash（key=value），
叶子 data_item_t（信号名 / 位偏移 / 缩放 / 场景间隔）


【写侧 —— CAN 线程，随 CAN 流量持续运行】

CAN 帧到达
    │ can_msg_process()（can_server.c）
    ▼
parse_common_msg()：HASH_FIND_INT 查通道规则表 → 递归 set_common_data_item
    │ ⚠P11 递归子规则时忽略 val 过滤器（data_process.c:342-348），
    │      当前点表一一对应不触发，报文复用后会信号污染
    ▼
写入叶子 data_item_t 的 value_int / value_double / time_rcv


【读侧 —— MQTT 线程，每 30s 一轮】

按「信号名 + 帧 ID」反查叶子：get_0f6b_work_data / check_0f6b_data_item
（函数名沿用 0f6b，但属在用的 0f7b 路径）
    │ binary_work_data_generate()
    │ ⚠P04 填到 BMS1_BatSoc 即 return（data_process.c:670），
    │      data_0f7b_t 其后约 24 个字段（BMS2_data…TP_DT2）恒 0 上报
    ▼
data_0f7b_t（__attribute__((packed)) 固定布局，与平台 schema 手工同步维护）
    │ make_tsp_0f7b_frame()（frame.c）：前置 level1_0f6b_t（时间戳 + GPS 经纬度）
    ▼
tsp_cmd 封装（src/tsp_cmd）：cmd=0x0f7b、设备号 BCD、序号
    ▼
线上帧 → MQTT publish 到本地 broker
```

### 线程交互时序图（30s 上报周期与 P03 竞态窗口）

```text
CAN 线程（写侧）              共享 can_mng（无锁）            MQTT 线程（读侧）
     │                             │                              │
     │ parse_common_msg            │                              │
     ├──写 value_int/time_rcv────▶ │                              │
     ├──写（持续不断）───────────▶ │                              │  ┌─ 每 30s 一轮
     │                             │ ◀────逐字段读取信号───────────┤  │ binary_work_data_generate
     ├──仍在写──────────────────▶ │ ◀────继续逐字段读取───────────┤  │
     │                             │                              │  │
     │            ⚠P03 竞态窗口：读取过程中写侧仍在写，            │  │
     │            多字段快照非原子 → 撕裂读（值/时间戳可能不一致）  │  │
     │                             │                              │  │
     │                             │                              │  │ make_tsp_0f7b_frame
     │                             │                              │  │ （加时间戳+GPS 一级头）
     │                             │                              │  │ tsp_cmd 封 0x0f7b 帧
     │                             │                              │  └─ publish 到本地 broker
     ▼                             ▼                              ▼
```

### 构建体系
- `Makefile` include `sdk.mk`，交叉编译（`arm-openwrt-linux-gcc`），`-Wall -Werror -std=c99`。
- 详见「构建问题」小节。

---

## 二、逐模块分析

### 2.1 构建系统（Makefile / sdk.mk）

**职责**：交叉编译到 ARM Quectel 目标。

问题清单：
- **[构建/中] 源码目录遗漏 `scene`、`scene_lib`、`log`**：`Makefile:24-36` 的 `APP_SRC_FILES` 只 glob 了 12 个目录，未含 `src/scene`、`src/scene_lib`、`src/log`。因此 `scene.c`、`scene_judge.c`、`log.c` 不参与编译。`scene`/`scene_lib` 为孤立死代码（符合 CLAUDE.md）；`log.c` 是否需编入取决于其是否被引用（见 2.15）。
- **[构建/高] 路径硬编码且 host 特定**：`sdk.mk:1-4` 将 `QL_SDK_DIR`、工具链路径写死为 `/home/tronlong/lyp/SDK/...`。换机器/换用户即失效，无 host-native 回退目标。
- **[构建/中] 库链接目标不一致风险**：`Makefile:6,38` 只链接 `lib/ec200a`，而仓库同时提供 `lib/eg25-g/`。切换硬件需手改 `EXTERNAL_LIBS`，无条件开关。
- **[构建/低] `INCLUDE_ALL_DIRS` 未含 `src/scene`、`src/log`**：`Makefile:10-22`，即便补回源码也需补 `-I`。
- **[构建/低·需验证] `-Werror` 与大量未检查返回值/`printf`**：若工具链告警级别高，`-Werror` 可能因未用返回值等使编译失败；能否过编译需真实 SDK 验证。

### 2.2 `main.c`

**职责**：单实例、建上下文、起三线程。

问题清单：
- **[资源/低] `is_instance_existing()` fd 泄漏（设计使然）**：`main.c:17` 打开 pid 文件后从不 `close`；作为持锁句柄贯穿进程属预期，检测到另一实例返回 true 时（`main.c:24-26`）fd 未关但随即 `exit`，可忽略。
- **[健壮/低] flock 非 EWOULDBLOCK 失败被当作「无实例」**：`main.c:22-27`，`flock` 返回非 0 且 `errno != EWOULDBLOCK` 时返回 false，锁异常时会误判为可启动。
- **[健壮/低] `set_app_ver()` 判据 `fd > 0` + 返回值未检查**：`main.c:35`，fd 为 0 时漏写；`write()` 返回值未检查（`main.c:37`）。
- **[线程安全/高] 无锁共享 + `sleep(1)` 时序 hack**：`main.c:67-71` 用两次 `sleep(1)` 错开线程启动，靠时序而非同步原语；`can_mng`/`data_process_mng`/`gps_info` 被三线程并发读写，全程无任何 mutex（详见 2.5/2.6/2.8）。
- **[健壮/低] `pthread_create` 返回值未检查**：`main.c:67-71`，创建失败后 `pthread_join` 为未定义行为。
- **[逻辑/低] PID/版本文件仍用 `0f6b` 命名**：`main.c:13,17`。

### 2.3 `src/can_mng`（上下文构造）

**职责**：分配 `can_mng_t`，读设备身份，算 BCD 手机号，置初始 scene。

问题清单：
- **[资源/中] `can_mng_new()` 错误路径释放不完整**：`can_mng.c:56-74`，读 id/SIM 失败分支对 `data_process_mng` 用裸 `free`，未递归释放其内部规则树/字符串（若已分配），为泄漏。属启动即退出场景，影响有限。
- **[内存/低·需验证] `check_phone_number` 边界**：`can_mng.c:6-24`，`phone_number[16]` 两分支均写 ≤11 字节，界内；`msi_sdn` 为空串时得 "00000000000"，可接受。未见越界。
- **[协议/中·需验证] BCD 前导零依赖有符号回绕**：`can_mng.c:78-80` 构造 `tran2bcd[12]`，`tran2bcd[0]=0x00`（NUL 非 '0'），`phone_number_to_bcd`（`common.c:141-144`）对 `[0]` 做 `(0x00-'0')<<4` 依赖 char 截断得 0x00 高半字节。结果虽正确但强依赖有符号回绕，脆弱；phone_number 含非数字时产生错误 BCD。
- **[逻辑/中] `cur_scene` 硬编码 "power_on"**：`can_mng.c:86`，全生命周期不变，导致上传间隔恒按 power_on 场景，scene 机制实际失效（影响见 2.5）。

### 2.4 `src/common`（设备身份/时间/BCD/hexdump）

**职责**：读 ini 配置、ACC/电压文件、BCD 转换、本地时间、hexdump。

问题清单：
- **[内存/低] `read_acc_status` 使用可能未初始化的 `value[0]`**：`common.c:52-60`，`char value[10]` 未初始化，`fread(...,1,fp)` 若文件为空返回 0，`value[0]` 为栈垃圾 → `if (value[0]!='0')` 可能误判 ACC=1。触发：`/tmp/acc_stat` 存在但为空。
- **[健壮/低] `read_mcu_*_voltage` fgets 返回值未检查**：`common.c:26,40`，空文件时 `str` 全 0，`atoi` 返回 0（非 -1），与「读失败」语义混淆。
- **[协议/低] `char2bcd`/`bcd_decimal_code_simple` 命名误导且输入>99 会溢出 BCD 语义**：`common.c:149-164`；当前调用点 `get_localtime_string` 输入均 <100，安全。
- **[内存/低·需验证] `hex_dump` 的 `snprintf(line+out, LINE_LEN-out, ...)`**：`common.c:203-211`，若 `out>128` 则 `LINE_LEN-out` 负转 `size_t` 巨大 + 越界。经算单行最大约 73<128，正常 16 字节/行不触发；脆弱但当前安全。
- **[健壮/低] `hex_dump` O(n²) 重复 asprintf+free**：`common.c:213-227`，性能问题非正确性问题。

### 2.5 `src/data_process`（点表解析 + 写侧 + 读侧打包）—— 核心模块

**职责**：解析点表为规则树；CAN 帧到达时写入信号（`parse_common_msg`→`set_common_data_item`）；每 30s 读出并打包 `data_0f7b_t`（`binary_work_data_generate`）。

#### 写侧 / 解析问题
- **[内存/高·确证] `set_common_data_item` VALUE_TYPE_STRING 分支越界读 + 堆溢出**：`data_process.c:320-333`。`char value_string[9]`，`size=end_byte-start_byte+1` 最大可为 9（甚至更大），`memcpy(value_string, can_msg_data+start_byte, size)` 从仅 8 字节的 CAN 帧读 `start_byte+size` 字节 → **越界读**；随后 `strdup(value_string)` 时 `value_string` 可能未以 NUL 结尾（9 字节填满）→ 越界读。二次进入时 `memset(data_item->value_string,0,size)+memcpy(...,size)` 写入一个首次 `strdup(strlen+1)` 的堆块，若 `size` 大于首值长度+1 → **堆缓冲区溢出**。触发：点表存在 `ptyp` 为 STRING/JSON_STRING 且长度较大的点。
- **[内存/高·确证] `set_common_data_item` little/big endian 读取移位 UB + 越界读**：`data_process.c:288-306`。当 `length` 接近 64 且 `start_bit>0` 时 `end_byte=start_byte+((start_bit+length-1)>>3)` 可达 `start_byte+8`，循环读 `can_msg_data[end_byte]` 越过 8 字节 CAN 帧（**越界读**），且移位量 `(n-start_byte)<<3` 可达 64，对 `uint64_t` 移位 64 位为**未定义行为**。触发：点表含 length>56 且非字节对齐偏移的点（需验证点表是否存在）。
- **[内存/高·需验证] `get_common_data_rule` 非 `#` 值 memcpy 长度越界写**：`data_process.c:138-139`。`memcpy(&data_rule->value, &value_temp, data_rule->length/8)`，`data_rule->value` 为 uint32（4 字节），若某规则 `val` 为纯数字字符串且 `len`（位）为 64 → `length/8=8` → 写 8 字节，溢出到相邻的 `data_sub_rule_list` 指针 → **结构体内存破坏**。当 `length` 非 8 倍数（如 0）时又只拷 0 字节使 value 恒 0。需验证点表实际字段。
- **[健壮/高·确证] 点表缺 `nm`/`mver` 等字段导致空指针解引用**：
  - `data_process.c:385` `strdup(cJSON_GetStringValue(json_mver))`：若点表无 `mver`，`cJSON_GetStringValue` 返回 NULL → `strdup(NULL)` **崩溃**。
  - `data_process.c:207-212`：若某点缺 `nm`，`data_item->name` 保持 NULL；随后 `check_0f6b_data_item`（`:459` `strcasecmp(data_item->name,name)`）对 NULL 解引用 **崩溃**。
- **[健壮/中·确证] 数值字段无类型/存在性校验**：`data_process.c:123-131,176-198` 对 `typ/ofs/len/bo/...` 直接 `cJSON_GetNumberValue(json_temp)`，缺字段时 cJSON 对 NULL 返回 0.0（部分版本），静默得 0，导致规则/点位偏移长度错误而无告警。
- **[资源/中·确证] `get_common_data_rule` 错误路径泄漏**：`data_process.c:173`（data_item calloc 失败 `return NULL`）、`:228`（scene 不匹配 `return NULL`）均未释放已分配的 `data_rule`、子规则链表、已建 `data_item`；`:249-251` 释放不完整（只 free data_rule + data_item，未释放已加入 sub_rule_list 的子规则）。均为点表解析期（启动）泄漏。
- **[逻辑/中·确证] scene 前缀匹配**：`data_process.c:223` `strncmp(scene[j], scene, strlen(scene))` 仅前缀匹配，"work" 会误配 "work_xxx"。
- **[逻辑/低] `check_0f6b_data_item` 递归返回值被忽略且总返回 -1**：`data_process.c:480,484`，命中与否靠输出参数区分，语义不清但功能可用。

#### 读侧 / 打包问题
- **[逻辑/高·确证] `data_0f7b_t` 大量字段从不被填充，恒为 0**：`data_process.c:573-671` 的**在用** `binary_work_data_generate` 只填到 `BMS1_BatSoc`（`:668`）即 `return`。结构体中其后的所有字段（`BMS2_data`、`BMS3_Temp[6]`、`BMS4_ChrState`、`BMS6_Bat_Warn[8]`、`BMS7_code`、`BMS8_*`、`BMS_VCU_*`、各 `DM1`/`TP_CM_BAM*`/`TP_DT2`、`ACCM_Remote`、`tboxACK` 等，`data_process.h:223-247`）**从未赋值**，依赖 payload 被调用方清零后恒为 0 上报。这是重大功能缺口（若非有意留白）。
- **[线程安全/高·确证] 读侧无锁读取 CAN 线程写入的信号**：`binary_work_data_generate` 全程在 MQTT 线程读取 `data_item->value_int/value_string`，与 CAN 线程 `set_common_data_item` 写入并发，无任何互斥；多字节/字符串字段可能**撕裂读**，快照非原子。
- **[逻辑/中·确证] `payload` 依赖调用方清零**：`data_process.c:578-582` `Tbox_Volt` 仅在 `voltage_mv>0` 时写、`:1830` `BMS_SN` 仅在收全时写。若 `make_tsp_0f7b_frame` 未对 payload 清零，则读到栈垃圾（见 2.7 frame 核验）。
- **[协议/中·确证] `htons`/`ntohl` 混用且对 int 直接转**：`data_process.c:615` `MCUFlt=ntohl(...)` 而 `:635` `IC_TotalMailage=htonl(...)`；ntohl 与 htonl 在本平台功能相同，但命名混乱、易误导；`work_data_item.value_int` 为 `int` 直接传 `htons/htonl`，>16/32 位被截断（多为预期）。
- **[逻辑/中·确证] `IC_TotalMailage` 缩放后再转序**：`data_process.c:635` `htonl(work_data_item.value_int*0.005)`，double 结果隐式转 uint32 截断，缩放系数与平台约定是否一致需核对点表/平台 schema（需验证）。
- **[逻辑/低] CAN ID 字面量风格不一致**：`data_process.c:601` `MCU_RunningSt` 用 `0x08CFF0104`（已含 EFF 高位），其余用 `0x0CFF0204` 形式（靠 `get_0f6b_work_data` 内 `|CAN_EFF_FLAG` 补位）。两种写法经 OR 后等价，但风格不统一易错。
- **[效率/低] 大量 `printf` 调试输出常驻**：`data_process.c:611,636,1847...` 等，生产环境噪声与性能开销。

#### 死代码 / 命名
- **[逻辑/中·确证] `#if 0` 包裹的 0f6b 版 `binary_work_data_generate` 为死代码**：`data_process.c:675-1895`（约 1200 行），含大量 `FIXME`、位段拼装、`data_0f6b_t`。不参与编译，符合 CLAUDE.md「遗留」定性；但体量巨大，维护/审计负担重。
- **[逻辑/低] 读侧函数名 `get_0f6b_work_data`/`check_0f6b_data_item` 仍带 0f6b**：`data_process.c:447,487`，实际服务 0f7b，命名误导。
- **[逻辑/中·确证] `set_cur_scene` 无调用者 → scene 恒 "power_on"**：`data_process.c:1906` 定义但全仓库无调用（`can_mng.c:86` 硬编码），`data_item.interval[]`/`scene[]` 机制实际不生效，上传节流完全失效（当前上报节奏仅由 MQTT 线程固定 30s 决定，见 2.8）。
- **[健壮/低] `parse_bms_sn_frame` 帧号校验正确**：`data_process.c:1955-1964`，`frame_number∈[1,4]`，`index=6*(fn-1)∈{0,6,12,18}`，`memcpy(...,6)` 恰好落在 `bms_sn[24]` 内，无越界。逻辑正确。此函数是否被 CAN 线程调用需在 can_server 核实（见 2.6）。

### 2.6 `src/can_server`（CAN 桥接客户端 + 写侧驱动）

**职责**：连三个本地 TCP 端口，`select` 读 `can_frame_t`，驱动 `parse_common_msg` / `parse_bms_sn_frame`。

问题清单：
- **[逻辑/高·确证] `skt_res_new_socket` 重试循环失效 + 返回未连接 fd**：`can_server.c:30-42`。`while(1)` 内 `break`（`:40`）在 `if` 之外无条件执行，循环仅跑一次：connect 失败时（非 retrycount==0）执行 `usleep` 后 `break`，然后 `return sockfd` **返回一个连接失败的 socket fd**。调用方 `start_can_server`（`:110`）以 `fd<0` 判成功 → 用坏 socket。`retrycount` 递减逻辑形同虚设，`return -1`（`:36`）几乎不可达。实际靠外层 read 失败后整体重连兜底，但设计错误、误导。
- **[资源/低] `skt_res_new_socket` give-up 路径 fd 泄漏（当前不可达）**：`can_server.c:35-36` `return -1` 前未 `close(sockfd)`；因上条 break 缺陷该路径实际走不到，理论泄漏。
- **[健壮/中·确证] TCP 流按固定 20 字节记录切分，无消息边界 → 帧错位/丢弃**：`can_server.c:77-94`。`read(...,1440)` 后 `frm_cnt=ret/sizeof(can_frame_t)`（20 字节），余数字节被丢弃（每轮 `memset` 重置缓冲）。TCP 是字节流不保证按 20 字节对齐；一旦某次 `read` 返回非 20 倍数，剩余半帧丢失且后续帧全部错位。是否安全取决于桥接端是否保证每包一帧（需验证）。
- **[协议/中·需验证] 写侧用原始 `frdup.can_id` 匹配规则，读侧却 `|CAN_EFF_FLAG`**：`can_server.c:92` `parse_common_msg(...,frdup.can_id,i)` 直接用帧 can_id；而规则 key 存的是含 EFF 高位的值（`data_process.c` CharToHexMem 解析 `#8C...`），读侧 `get_0f6b_work_data` 又主动 `|CAN_EFF_FLAG`。若桥接给的 `can_id` 未含 EFF 高位，写侧将匹配不到任何规则（信号永不更新）。两侧处理不一致，需核实帧 can_id 是否已带 EFF 标志。
- **[健壮/低] `select` 返回 -1 未 `continue`**：`can_server.c:70-72` 仅打印，随后仍进入 `FD_ISSET` 遍历，此时 `rset` 未定义，可能误读。
- **[线程安全/高·确证] 写侧无锁写入共享信号**：`set_common_data_item`（经 `:92`）在 CAN 线程写 `data_item->value_*`，与 MQTT 线程读并发，无互斥（与 2.5 对应）。
- **[逻辑/低] 计数 `cnt`/`frm_cnt` 混用、`can_rcv_cnt` 无人读取**：`can_server.c:53,88-89`，`can_rcv_cnt` 仅累加从不使用。

### 2.7 `src/frame`（一级头封装）

**职责**：`make_tsp_0f7b_frame` 给 payload 加 `level1_0f6b_t` 头（版本/GPS/时间），交 tsp_cmd 封帧。

问题清单：
- **[逻辑/确认-无害] payload 由调用方清零**：核实 `client_mosquitto.c:100` 在调用 `binary_work_data_generate` 前 `memset(&payload_0f7b,0,...)`，故 2.5 中「payload 依赖清零」已满足，未填字段恒为 0（非栈垃圾）。但 2.5「大量字段恒 0」的功能缺口仍成立。
- **[资源/中·确证] `calloc` 返回值未检查**：`frame.c:79` `uint8_t *data=calloc(1,len)` 未判 NULL，随后 `memcpy(data,...)` 对 NULL 解引用崩溃（内存不足时）。
- **[线程安全/高·确证] 读 `gps_info->lat/lon` 无锁**：`frame.c:68-69` MQTT 线程读 `gps_info` 的 double 字段，与 GPS 线程 `memcpy(gps_data,...)`（`gps.c:50`）并发，double 8 字节在 32 位 ARM 上非原子 → **撕裂读**，可能得到经纬度混合值。
- **[协议/低] `version` 字段 0f7b 版设为 0x0，0f6b 版设为 0x04**：`frame.c:65` vs `:20`，与平台约定是否一致需核对（需验证）。
- **[逻辑/低] `altitude`/`direction` 恒填 0**：`frame.c:70-71`，即便 GPS 有 altitude/direction 也不上报。
- **[逻辑/低] `make_tsp_0f6b_frame` 为死代码**：`frame.c:14-57` 无调用者。
- **[健壮/低] `hex_dump` 返回 NULL 时 `printf("%s",NULL)`**：`frame.c:97-99`，glibc 容忍（打印 "(null)"），非标准。

### 2.8 `src/mosquitto`（MQTT 上报线程）

**职责**：连本地 broker，每 30s 生成 payload、封帧、发布。

问题清单：
- **[线程安全/中·确证] 全局 `mqtt_status` 无锁跨线程访问**：`client_mosquitto.c:20`。`mosquitto_loop_start`（`:89`）起库内部网络线程，回调（`:22-43`）在该线程写 `mqtt_status`，而主任务循环（`:99`）在本线程读；非原子、无内存屏障，状态机存在竞态（int 读写在 ARM 上多为原子，但无 happens-before 保证）。
- **[构建/中·确证] `printf("...%d\n", sizeof(...))` 格式不匹配**：`client_mosquitto.c:101`，`sizeof` 为 `size_t`，`%d` 期望 `int` → `-Wformat` 告警，配合 `Makefile` 的 `-Werror` 可能**直接编译失败**（需在真实工具链确认）。
- **[逻辑/中·确证] 上报周期固定 30s，scene 节流未接入**：`client_mosquitto.c:116` `sleep(30)` 硬编码。结合 2.5「`set_cur_scene` 无调用」，点表中每信号的 `interval[]`/`scene[]` 完全不起作用，所有信号一律 30s 全量上报。
- **[健壮/低] 首次连接失败进入 `while(mqtt_status==DISCONNECTED)` 死循环重试，但断连后依赖库自动重连**：`client_mosquitto.c:76-85` 仅在启动阶段执行一次；运行中断连后靠 `mosquitto_loop_start` 内部自动重连恢复（`reconnect_delay_set`），逻辑可用但主循环 `sleep(5)` 空转直到回调置回 `CONNACK_RECVD`。
- **[逻辑/低] 客户端 id 硬编码 `"tsp_0f6b_client"`**：`client_mosquitto.c:55`，0f6b 命名遗留；多设备同 broker 可能 id 冲突（需验证 broker 是否按此 id 区分）。
- **[逻辑/低] 函数尾部 `disconnect/destroy` 为死代码**：`client_mosquitto.c:123-126` 在 `while(1)` 之后不可达。

### 2.9 `src/tsp_cmd`（TSP 线协议封装，0f6b/0f7b 共用）

**职责**：构造/序列化 TSP 帧（头位段、设备 id、序号、转义、异或校验、0x7e 边界）。

问题清单：
- **[协议/高·需验证] `tsp_cmd_attr_t` 混合基础类型位段，布局/大小依赖实现**：`tsp_cmd.h:26-31`（`uint16_t msg_len:10; uint8_t encrypt:3; bool split:1; uint8_t res:2;`）。跨 `uint16_t`/`uint8_t`/`bool` 的位段其存储单元分配与位序均为实现定义；`tsp_cmd.c:262` 用 `memcpy(&cmd_attr, &cmd_attr字段, sizeof(tsp_cmd_attr_t))` 当作 `uint16_t` 处理，若 `sizeof != 2` 或位序与平台 schema 不符则 `msg_len/encrypt/split` 上线位置错误。收发双方同一编译器可自洽，但与 TSP 平台线格式一致性无法从代码确证。
- **[协议/中·确证] 位段经 `ntohs` 转序依赖字节序假设**：`tsp_cmd.c:263-264`，将位段内存当 `uint16_t` 做 `ntohs` 后拆两字节写线。正确性绑定于编译器位段布局，脆弱。
- **[资源/低] 多处 `calloc` 未检查返回**：`tsp_cmd.c:141,170,201,224,226,257,286,300` 等，内存不足时崩溃。
- **[逻辑/中·确证] `tsp_cmd_frame_parse` / `general_resp` 为未使用死代码且含缺陷**：接收路径在本客户端无调用者。其内 `:159-160` pack_num 读 1 字节却 `index+=2`、`:165` 长度校验用原始 `len`（含转义前长度）而非 `cmd_len`、`:170` `calloc(1,msg_len)` 可能为 0，均为潜在 bug，但因不被调用无实际影响。
- **[内存/低] `tsp_cmd_ntop` 转义非法序列时置 `*len=-1` 但已释放 `out`**：`tsp_cmd.c:31-33`，调用方 `frame_parse` 判 `0>=cmd_len` 后直接返回，未二次 free，无 double free；仅属死代码路径。

### 2.10 `src/skt_res`（TCP 客户端连接助手）

**职责**：`skt_res_new_socket_ip` 供 GPS 线程连接。

问题清单：
- **[资源/高·确证] `skt_res_new_socket_ip` connect 失败时 fd 泄漏**：`skt_res.c:60-81`。`socket()` 成功后若 `connect` 失败，`:78-80` 直接 `return -1` **未 `close(sockfd)`**。GPS 线程在断连/未就绪时循环调用并 `sleep(1)` 重试（`gps.c:34-37`），每次失败泄漏 1 个 fd，长期运行**耗尽文件描述符**导致无法再建 socket。属确证的持续性资源泄漏。
- **[资源/低] `skt_res_new_socket_bak` 同样泄漏且为死代码**：`skt_res.c:29-58`（`:48-51` connect 失败 return -1 未 close），无调用者。
- **[健壮/低] `printf` 缺换行**：`skt_res.c:49,56` 部分日志无 `\n`。

### 2.11 `src/gps`（GPS 采集线程）

**职责**：连 `127.0.0.1:16005`，节流拷贝 `sany_gps_ec21_t` 到 `can_mng->gps_info`。

问题清单：
- **[资源/高·确证] 借道 `skt_res_new_socket_ip` 导致的 fd 泄漏**：`gps.c:35`，每次连接失败（GPS 桥接未就绪时高频发生）经 `skt_res.c:78-80` 泄漏 fd（见 2.10），是本进程最现实的资源耗尽点。
- **[健壮/中·确证] `recv` 按结构体整长读，无消息边界处理**：`gps.c:41` `recv(sockfd,&recv_gps_data,sizeof(sany_gps_ec21_t),0)`，TCP 流可能返回部分结构（`len>0` 但 `< sizeof`），随后 `memcpy(gps_data,&recv_gps_data,sizeof)`（`:50`）把半填充缓冲整体拷入 → GPS 字段错位/垃圾。
- **[线程安全/高·确证] 无锁写共享 `gps_info`**：`gps.c:50` GPS 线程 `memcpy` 整个结构，与 MQTT 线程 `frame.c:68-69` 读 `lat/lon` 并发，无互斥 → 撕裂读（见 2.7）。
- **[逻辑/中·确证] 仅每 ≥5s 更新一次 `gps_info`，其余数据丢弃**：`gps.c:44-51`，`cur_time-pre_time>=5` 才 `memcpy`，期间收到的 GPS 全被丢弃，`gps_info` 最长滞后 5s（外加断连 10s 才重连，`:53-57`）。
- **[健壮/低] 断连感知慢**：`gps.c:53` 需连续 10 次 `recv<=0` 且各 `sleep(1)` 才关闭重连，约 10s 才恢复。

### 2.12 `src/scene`（动态库场景判定加载器）—— 未编译死代码

**职责**（设计意图）：`dlopen("./libscene.so")` 取 `judge_scene` 符号存入 `can_mng->judge_scene`。

问题清单：
- **[构建/逻辑·确证] 不参与编译且引用不存在的结构体成员**：`scene.c` 未列入 `Makefile`（2.1）。`scene.c:34` `can_mng->judge_scene = funcp`，但 `can_mng_t`（`can_mng.h:18-27`）**无 `judge_scene` 成员**，一旦编入必然编译失败。确认为孤立死代码。
- **[资源/低·确证] `dlopen` 失败仍 `dlclose(NULL)`**：`scene.c:16-19,37-38`，`lib_handle==NULL` 时 `goto ERROR` 执行 `dlclose(NULL)`（未定义行为）。死代码，无实际影响。
- **[逻辑/确认] 全仓库无 `load_libscene` 调用者**：确证 `scene.h/scene.c` 仅自引用（见交叉引用扫描），场景机制未接线。

### 2.13 `src/scene_lib`（`judge_scene` 实现，拟编为 libscene.so）—— 未编译死代码

**职责**（设计意图）：按 engine_speed/drive_speed 返回场景字符串。

问题清单：
- **[逻辑/中·确证] `calc_value` 存在完全重复的代码块**：`scene_judge.c:60-69` 与 `:70-79` 逐字重复（两段 `if((length%8)!=0)` 计算 `cBitsValue` 并 `memcpy` 到同一位置），第二段纯属复制粘贴冗余，第二次还会二次 `cbyteIdx += length/8` 导致读位置漂移（若真运行）。
- **[内存/低·确证] `param_cfg` 偏移/长度对齐假设**：`scene_judge.c:23` offset=64/len=16，`calc_value` 从结构体首字节计偏移（byte 8 起为 `data[0]`），落在 `data[]` 内；但注释「偏移量一定是8的倍数」与代码耦合脆弱。死代码，无实际影响。
- **[逻辑/确认] 未列入 `Makefile` 也无独立构建 libscene.so 的规则**：与 `scene.c` 共同构成未接线的孤立设计残留。

### 2.14 `src/log`（分级日志）—— 未编译，当前无引用

**职责**：`SANY_LOG_*` 宏 + 注册回调 `s_func` 输出。

问题清单：
- **[构建/低·确证] `log.c` 未编入且无处调用**：交叉引用扫描显示 `SANY_LOG_*`/`log.h` 仅在 `log.c/log.h` 内部出现，业务代码全用裸 `printf`。故 `log.c` 未编入 `Makefile` 不产生链接错误。属未启用模块。
- **[逻辑/低] 即便启用，`s_func` 未注册则所有日志被吞**：`log.c:30,131`，`sany_log_level_check` 在 `s_func==NULL` 时返回 -1，日志不输出；需先 `sany_debug_func_register`，当前无人调用。
- **[健壮/低] `vasprintf` 返回值未检查**：`log.c:127`，失败时 `msg` 未定义，`:132` 传入 `s_func` 再 `free(msg)` 可能出错。仅在启用后有意义。

### 2.15 点表一致性与「代码路径 vs 当前数据」核验（`Pointsheet_info1.json`）

对 33KB 点表实测统计，用于判定 2.5 中若干「代码层确证」的内存缺陷是否被**当前数据**触发：
- **所有 34 个叶子点 `ptyp` 均为 1（INTEGER）**：→ 2.5 的 `VALUE_TYPE_STRING` 越界读/堆溢出路径（`data_process.c:320-333`）**当前点表不触发**，属**潜在**（代码确证、数据未触发）。
- **点位 `len` 最大为 32（分布 1/2/4/8/13/16/32）**：→ 2.5 的 64 位移位 UB / 跨 8 字节越界读（`data_process.c:288-306`）与 `value_int` memcpy（≤4 字节，已 clamp）**当前点表不触发**，属**潜在**。
- **msg 级 `val` 均为 `#`-前缀十六进制、`len=32`；非 `#` 值仅子规则的 `"8"`（`len=8`）**：→ 2.5 的「非 `#` 值 memcpy 长度越界写」（`data_process.c:138-139`，需 `len=64`）**当前点表不触发**，属**潜在**。
- **所有点均含 `nm`、根含 `mver:"LHT"`**：→ 2.5 的 `strdup(NULL)` / `strcasecmp(NULL)` 崩溃**当前点表不触发**，属**潜在**（数据缺字段即崩）。
- **[逻辑/中·确证] 子规则（`rls`）的匹配条件在写侧被完全忽略**：`data_process.c:342-348` `set_common_data_item` 对 `data_sub_rule_list` **无条件递归全部子规则**，从不校验子规则的 `type/offset/length/value`（即点表里 `rls` 的 `val:"8"` 字节过滤器）。当前点表每个 CAN 帧下仅一个 `rls`（一一对应），故暂无串扰；但一旦某 CAN ID 下出现多个按子字段（PGN/源地址）区分的复用子规则，写侧会把所有变体的点全部写入 → **信号值相互污染**。属确证的设计缺陷 + 潜在数据触发。
- **[协议/需验证] `data_0f7b_t` 字段 ↔ 点表一致性**：点表仅定义 34 个点（MCU/BMS/VCU/ABS/DCDC 等），而 `data_0f7b_t`（`data_process.h:200-248`）字段更多且读侧只填到 `BMS1_BatSoc`。二者与接收平台 schema 的逐字段对齐无法从代码/点表确证，需对照平台协议文档（见 2.5 读侧「大量字段恒 0」）。

### 2.16 第三方 vendored 库扫描结论

按要求逐一扫描，均视为上游未改动代码，仅在被业务错误使用时才追责：
- **`src/cJSON`（cJSON.c/.h，3110 行）**：标准 cJSON。关键点：`cJSON_GetObjectItem` 对不存在键返回 NULL，`cJSON_GetNumberValue(NULL)` 返回 `NAN`、`cJSON_GetStringValue(NULL)` 返回 NULL——本仓库多处未判空即使用（见 2.5），**问题在调用方而非库**。库本身未见改动痕迹。
- **`src/list`（cc_slist.c 1588 行 / cc_common）**：通用单链表（Collections-C 血统）。`cc_slist_get_at/size/add/new` 返回 `CC_OK/err`，业务侧多数忽略返回值（如 `cc_slist_get_at` 失败时出参指针未定义）；`data_process.c` 中若 `get_at` 失败会解引用未初始化指针，但索引来自 `cc_slist_size` 循环，正常不越界。库本身扫描未见缺陷。
- **`src/opt_iniparser`（iniparser.c 903 / dictionary.c 402）**：标准 iniparser。`iniparser_getstring` 默认值语义正确；`common.c` 使用方式正常。库本身未见问题。
- **`include/uthash.h`（1138 行）**：标准 uthash。`HASH_ADD_INT/FIND_INT` 以 `data_rule_t.value` 为 key。注意 `HASH_ADD_INT` 要求 key 字段为 `int` 语义，此处 `value` 为 `uint32_t`，混入 EFF 高位（0x8C...）在 32 位上按位一致，功能正确。库本身未见问题。
- **`include/mosquitto.h`（3083 行）/ `include/cn-cbor.h`（652 行）**：外部库头文件。`cn-cbor` 未被任何源文件包含使用（CBOR 上报路径 `MSG_TYPE_CBOR` 未实现），属未使用头。`mosquitto.h` 与 `lib/ec200a/libmosquitto.so` 配套，正常使用。

---

## 三、问题汇总表

> 严重度：高＝可致崩溃/数据错误/资源耗尽/上报失真；中＝条件触发或功能缺失；低＝健壮性/风格/性能。
> 「确证」＝代码层可确认；「确证(代码)/数据未触发」＝逻辑确凿但当前 `Pointsheet_info1.json` 不触发，为潜在；「需验证」＝依赖外部（平台 schema/桥接行为/工具链）。

| 编号 | 严重度 | 文件:行号 | 问题 | 触发条件与影响 | 确证/推断 |
|---|---|---|---|---|---|
| P01 | 高 | skt_res.c:78-80 | `skt_res_new_socket_ip` connect 失败不 `close(sockfd)` | GPS 桥接未就绪时每秒泄漏 1 fd，长期耗尽 fd，进程无法再建连接 | 确证 |
| P02 | 高 | can_server.c:30-42 | `skt_res_new_socket` `break` 在 `if` 外，重试失效并返回未连接 fd | connect 失败仍返回"有效"fd，靠外层兜底；设计错误、误导 | 确证 |
| P03 | 高 | 全局（main.c:67-71 等） | 三线程共享 `can_mng`/`data_process_mng`/`gps_info` 全程无锁 | CAN 写 vs MQTT 读、GPS 写 vs MQTT 读并发；多字节/double/字符串撕裂读，快照非原子 | 确证（无 mutex） |
| P04 | 高 | data_process.c:573-670 | 在用 `binary_work_data_generate` 只填到 `BMS1_BatSoc` 即返回 | `data_0f7b_t` 自 `BMS2_data` 起大量字段恒 0 上报，功能缺口 | 确证 |
| P05 | 高 | data_process.c:320-333 | STRING 分支越界读 + 二次进入堆溢出 | 需点表存在 `ptyp∈{3,4}` 且长度较大的点 | 确证(代码)/当前点表未触发 |
| P06 | 高 | data_process.c:288-306 | length≈64 且非字节对齐时移位 64 位 UB + 跨 8 字节越界读 | 需点表存在 len>56 且非对齐偏移点 | 确证(代码)/当前点表未触发 |
| P07 | 高 | data_process.c:138-139 | 非 `#` 值 `memcpy(&value,...,len/8)`，len=64 时溢出相邻指针 | 需点表出现非 `#` 值且 `len=64` 的规则 | 确证(代码)/当前点表未触发 |
| P08 | 高 | data_process.c:385,207-212/459 | 点表缺 `mver`/`nm` 致 `strdup(NULL)`/`strcasecmp(NULL)` 崩溃 | 点表任一点缺 `nm` 或根缺 `mver` 即启动/运行崩溃 | 确证(代码)/当前点表未触发 |
| P09 | 中 | can_server.c:77-94 | 按固定 20 字节切分 TCP 流，余数丢弃 → 帧错位 | 桥接单次 `read` 非 20 倍数时后续帧全错位 | 确证(代码)/需验证桥接分包 |
| P10 | 中 | can_server.c:92 vs data_process.c:495 | 写侧用原始 can_id、读侧 `|EFF_FLAG`，两侧不一致 | 若帧 can_id 不含 EFF 高位，写侧永不匹配规则，信号不更新 | 需验证桥接 can_id 格式 |
| P11 | 中 | data_process.c:342-348 | 写侧递归子规则时忽略子规则匹配条件（`val` 过滤器） | 同一 CAN ID 下多复用子规则时信号互相污染 | 确证(代码)/当前点表一一对应未触发 |
| P12 | 中 | client_mosquitto.c:101 | `printf("%d", sizeof(...))` 格式不匹配 | 配合 `-Werror` 可能编译失败 | 确证(代码)/需工具链验证 |
| P13 | 中 | tsp_cmd.h:26-31 / tsp_cmd.c:262-264 | `tsp_cmd_attr_t` 混合类型位段，布局/位序实现定义，被当 uint16 处理 | 若 `sizeof!=2` 或位序不符平台，则 `msg_len/split/encrypt` 上线位置错 | 需验证平台线格式 |
| P14 | 中 | gps.c:41-50 | `recv` 无消息边界，部分包整体拷入 `gps_info` | TCP 分片时经纬度等字段错位/垃圾 | 确证(代码)/需验证桥接 |
| P15 | 中 | can_mng.c:86 / data_process.c:1906 | `set_cur_scene` 无调用者，scene 恒 "power_on" | 点表 `interval/scene` 节流全失效，一律 30s 全量上报 | 确证 |
| P16 | 中 | client_mosquitto.c:116 | 上报周期硬编码 `sleep(30)`，未接 scene 节流 | 无法按场景/信号差异化上报 | 确证 |
| P17 | 中 | frame.c:79 / tsp_cmd.c 多处 | `calloc` 返回值未检查即 `memcpy` | 内存不足时崩溃 | 确证 |
| P18 | 中 | client_mosquitto.c:20 | 全局 `mqtt_status` 无锁跨线程 | 库网络线程与主循环竞态改状态机 | 确证 |
| P19 | 中 | can_mng.c:56-74 | `can_mng_new` 错误路径裸 `free(data_process_mng)` 不递归释放 | 启动读 id/SIM 失败时泄漏（进程随即退出，影响小） | 确证 |
| P20 | 中 | data_process.c:173,228,249 | `get_common_data_rule` 错误路径泄漏 data_rule/子规则/data_item | 点表畸形/内存不足时启动期泄漏 | 确证 |
| P21 | 中 | can_mng.c:78-80 / common.c:141-144 | BCD 前导零依赖有符号回绕；非数字字符产生错误 BCD | msisdn 含非数字时设备 id BCD 错误 | 确证 |
| P22 | 低 | common.c:52-60 | `read_acc_status` 用未初始化 `value[0]` | `/tmp/acc_stat` 为空时 ACC 误判为 1 | 确证 |
| P23 | 低 | main.c:22-35 | flock 非 EWOULDBLOCK 失败误判可启动；`set_app_ver` `fd>0` | 锁异常时误启动；fd=0 漏写版本 | 确证 |
| P24 | 低 | scene.c:34 / Makefile | `scene.c` 引用不存在的 `judge_scene` 成员，且未编译 | 一旦编入必编译失败；当前为死代码 | 确证 |
| P25 | 低 | scene_judge.c:60-79 | `calc_value` 完全重复代码块 | 若运行则读位置漂移；当前为死代码 | 确证 |
| P26 | 低 | frame.c:14 / data_process.c:675-1895 / client_mosquitto.c:123 等 | 大量 0f6b 死代码、`make_tsp_0f6b_frame`、`#if 0` 千行块、不可达清理 | 维护/审计负担，命名混淆 | 确证 |
| P27 | 低 | data_process.c:611,636,1847 等 | 生产常驻 `printf` 调试输出 | 噪声与性能开销 | 确证 |
| P28 | 低 | sdk.mk:1-4 / Makefile:6,38 | SDK 路径硬编码；只链 ec200a，eg25-g 需手改 | 换机器/换硬件即失效 | 确证 |

---

## 四、总结与修复优先级建议

### P0（应立即修，影响稳定性/正确性）
1. **修 fd 泄漏 P01**：`skt_res_new_socket_ip` connect 失败前 `close(sockfd)`。这是长期运行最可能拖垮进程的缺陷（GPS 反复重连）。
2. **修 P02 重试逻辑**：将 `break` 移入 connect 成功分支，失败时 `close` 并按 `retrycount` 真正重试，失败返回 -1。
3. **加锁 P03**：为共享信号状态引入互斥（方案 A：一把 `data_process_mng` 级 mutex，写侧 `set_common_data_item`、读侧 `binary_work_data_generate` 各自加锁；方案 B：读侧先在锁内做一次整体快照到本地结构再解析）。`gps_info` 单独加锁或用 seqlock/double-buffer。
4. **补全 P04 打包**：确认 `data_0f7b_t` 自 `BMS2_data` 起字段是否应上报；若应，补齐 `binary_work_data_generate`；若确为占位，删字段并同步平台 schema，避免恒 0 误导平台。

### P1（尽快修，条件触发即出问题）
5. **点表解析健壮化 P05-P08**：对 STRING 长度做 `size>sizeof(value_string)` 边界钳制并保证 NUL 结尾；`length` 上限校验（已有 >64 跳过，需再防 `size>8`/移位量=64）；非 `#` 值 `memcpy` 长度钳到 `sizeof(value)`；`cJSON_GetStringValue` 返回值判空后再 `strdup`；缺 `nm` 的点跳过或告警。即便当前点表不触发，换点表即暴雷。
6. **CAN 流分帧 P09 / EFF 一致性 P10**：改为按 20 字节做粘包重组（保留残余字节跨 read 拼接）；统一写/读两侧对 EFF 标志的处理。
7. **子规则匹配 P11**：写侧递归前按子规则 `type/offset/length/value` 校验命中再下钻，恢复 `val` 过滤语义。
8. **编译阻断 P12**：`printf("%zu", sizeof ...)` 或强转 `(int)`，避免 `-Werror` 断编译。

### P2（择机修，健壮性/可维护性）
9. scene 机制 P15/P16：要么接线 `set_cur_scene` 并按 `interval` 节流上报，要么彻底移除 scene 死代码与相关字段，明确「固定 30s 全量」为设计。
10. 位段协议 P13、BCD P21、`calloc` 判空 P17、启动期泄漏 P19/P20、ACC 未初始化 P22、main 健壮性 P23。
11. 死代码清理 P24-P27（`scene`/`scene_lib`/`#if 0` 0f6b 块/0f6b 帧函数/调试 printf），降低审计与误改风险。
12. 构建 P28：将 SDK 路径改为可覆盖变量（`?=`）或从环境读取；ec200a/eg25-g 用条件变量切换。

### 需向外部确认（无法从代码定论）
- 桥接进程是否保证「每 socket 消息=一条完整 20 字节 CAN 记录 / 完整 GPS 结构」（关乎 P09/P14）。
- 帧 `can_id` 是否已带 `CAN_EFF_FLAG`（关乎 P10）。
- `tsp_cmd_attr_t` 位段在目标工具链的 `sizeof` 与位序是否符合 TSP 平台线格式（关乎 P13）。
- `data_0f7b_t` 完整字段布局与接收平台 schema 的逐字段对齐（关乎 P04）。
- 目标 Quectel 工具链在 `-Wall -Werror` 下能否通过当前代码（关乎 P12 及大量未检查返回值）。

<!-- GENERATION_COMPLETE: 2026-07-02 -->
