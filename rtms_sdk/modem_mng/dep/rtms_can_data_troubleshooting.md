# RTMS 框架 CAN 数据链路排查手册

> 场景：同事想用 rtms 框架查 CAN 数据，确认"有没有数据进来"、"数据在哪个通道"。
> 主体（第1~5节）覆盖 **CAN 数据本地采集/分发链路**（io_mng → can_dspt → 消费端），并特别指出 CAN 数据在 IMX6（直连 SocketCAN）和 EC200A/EG25G（经 MCU USB/SPI 链路）上的架构差异（见 1.1）。第 6 节是一种更直接、不用解析二进制的排查手段（向 io_mng 查询 CAN/COM/DO/AO/DI/AI/MCU 的计数器和状态）；第 7 节 GPS、第 8 节 COM 串口透传、第 9 节 IMU——这三节和 CAN 一样，在 EC200A/EG25G 上都走同一条 MCU 链路；第 10 节补充 DI/DO/AI/AO 的查询方式差异和输出控制；第 11 节是 EVT 故障事件总线和通用的 CMD 命令通道。rtms_client 把状态上报到云端之后的协议细节不在本次审查范围内。
> 标注说明：`[代码]` = 直接读源码确认的事实（附 file:line）；`[推断]` = 根据代码逻辑推出但未在真机验证；`[未验证]` = 需要上机器实测才能确认。

---

## 0. 快速回答：有没有CAN数据？有几个通道？在哪个通道？

不想看全文的话，三个问题的答案都在这里，命令直接抄：

**Q1：有没有 CAN 数据进来？**
```sh
# 第一步：进程在不在跑（Step 0）
ps | grep -E "io_mng|can_dspt|can_client"

# 第二步：最直接的方法——问 io_mng 要收发计数器，两次请求间隔几秒对比数字有没有涨（6.1节）
nanocat --req --connect tcp://127.0.0.1:38000 --data '{"status":{"io":["can"]}}' --format ascii
# 返回 JSON 里每一路是 {"dev":"can","chn":N,"params":{"mcu_rx":.., "mng_rx":..,...}}
# mcu_rx/mng_rx 两次请求之间在涨 = 这一路有数据进来；不涨 = 没数据
```
如果嫌 REQREP 麻烦，也可以直接抓包看原始报文（Step 3/6）：
```sh
nanocat --sub --connect tcp://127.0.0.1:16002 --subscribe "" --format hex   # io_mng 原始层
nanocat --sub --connect tcp://127.0.0.1:16028 --subscribe "" --format hex   # can_dspt 过滤后（前提：已经用16029下发过订阅，见Step 5）
```

**Q2：有几个通道？**
框架最多支持 8 路（channel 0~7），但这台设备实际有几路看 `/opt/board_info.json` 里的 `can_nb` 字段：
```sh
cat /opt/board_info.json
```
或者上面 Q1 的 REQREP 查询结果里，返回数组里有几个 `{"dev":"can","chn":N,...}` 对象，就是几路。

**Q3：数据在哪个通道？**
- 用 Q1 的 REQREP 方法：返回 JSON 里的 `"chn"` 字段直接就是通道号。
- 用抓包方法：16028（或非 IMX6 平台的 16022）上每条数据是 24 字节的 `can_frame_ext_t`/`can_msg_mixed_item_t` 结构体，**第 17 个字节（偏移16，紧跟在 timestamp 后面）就是 channel（0~7），对应 can0~can7**；16002/16003/16004/16023~16027 这几个端口本身也是按通道号分开的（16002=can0、16003=can1……）。

以上是最常用的三个问题的直接答案。如果查完发现"没数据"，往下翻**第 4 节的根因清单**，最常见的原因是：can_dspt 没人下发过订阅（Step 5），或者 can_dspt 重启过订阅被清空了（Step 5 里的操作风险提醒）。

---

## 1. 整体链路

```
物理层：IMX6 = Linux SocketCAN (can0/can1/...)；其它平台 = MCU 经 USB/SPI 上报（见下方架构说明）
        ▼
   io_mng（进程）
   - 按通道各自用 Nanomsg PUB 转发未过滤的原始帧
        │  tcp://127.0.0.1:16002~16004, 16023~16027（每个通道一个端口）；部分平台还有 16022 混合端口
        ▼
   can_dspt（进程，编译开关 WITH_CAN_DSPT，默认关闭）
   - 对每个通道 Nanomsg SUB 连接上面的端口，收原始帧
   - 用 UDP:16029 收到的“订阅 CAN ID 列表”做过滤（哈希表命中才转发，未命中的帧直接丢弃）
   - 打上 channel(0~7) 标记后，统一用 Nanomsg PUB 广播
        │  tcp://127.0.0.1:16028（过滤后、带 channel 字段）
        ▼
   消费端（can_client / forward_can / gb32960_client_for_dima 等，各自编译开关）
   - Nanomsg SUB 连接 16028，拿到过滤后的 can_frame_ext_t
```

**关键点**：16028 上能不能看到数据，取决于两件独立的事——① CAN 总线本身有没有报文、② can_dspt 有没有被下发过“要哪些 CAN ID”的订阅。这两件事任何一个不满足，16028 都是空的，现象一样（“没数据”），但排查方向完全不同，必须分层验证。

### 1.1 重要架构说明：io_mng 的外设数据大多共用同一条 MCU 链路

读代码时发现一个对整篇文档都有影响的事实，先说在前面：**io_mng 有两套完全独立、互斥的主实现，按平台宏在编译期二选一**（`[代码]` apps/io_mng/src/io_mng/io_mng.c:1 头部 `#if defined(QL_MODULE_PLATFORM_EC200A) || defined(QL_MODULE_PLATFORM_EG25G)`；apps/io_mng/src/io_mng/io_mng_imx6.c:1 头部 `#if defined(QL_MODULE_PLATFORM_MCIMX6Y2CVM08AB)`；平台宏由 apps/io_mng/CMakeLists.txt 里的 `QL_MODULE_PLATFORM` 环境变量决定）：

- **EC200A / EG25G**（`io_mng.c`）：io_mng 和 MCU 之间走 USB 或 SPI（`[代码]` apps/io_mng/src/io_mng/io_mng.h:46-51，`#ifdef USE_IO_MNG_USB` 二选一），MCU 把 CAN/COM/GPS/IMU/DI/AI 等所有外设数据打包成一串"带标签的条目"发过来，io_mng 收到后按标签分发（`[代码]` apps/io_mng/src/io_mng/io_mng.c:704-870，`switch (p_item_hdr->tag)`：`TAG_CAN0~7`、`TAG_CAN_MIXED`、`TAG_COM0~2`、`TAG_GPS`/`TAG_NMEA`、`TAG_GPS_COM`、`TAG_LOG_COM`、`TAG_IMU_COM`、`TAG_DI`、`TAG_AI`、`TAG_CMD`、`TAG_EVENT`），再各自发到对应的 Nanomsg 端口。**这几乎是本文档除 CAN 之外新增的所有小节（COM/IMU）的共同物理基础**——它们全部来自同一条 USB/SPI 链路，这条链路本身好不好，直接决定这些通道是不是同时都没数据。具体怎么在命令行上直接验证这条链路的健康状态，本次没有找到现成工具，标 `[未验证]`。
- **IMX6**（`io_mng_imx6.c`）：CAN 直接走 Linux SocketCAN（`can_socket.c`，`can0`/`can1` 网络接口），不经过 USB/SPI。**下面第 3 节的 Step 1（`ip -s link show can0`/`candump`）只对 IMX6 成立**。IMX6 上 COM/IMU 等其它外设具体怎么实现，本次没有读 `io_mng_imx6.c` 的细节，不在本文档覆盖范围内 `[未验证]`。

在 EC200A/EG25G 上，CAN 具体走 `TAG_CAN0~7`（进 16002~16027，走 can_dspt 那条链路）还是 `TAG_CAN_MIXED`（进单独的 16022 混合端口，结构体 `can_msg_mixed_item_t` 自带 channel 字段，`[代码]` apps/io_mng/src/can/can_mixed.h:6-13），取决于对接的 MCU 固件版本，本次没有找到能直接判断的配置项，需要 `nanocat` 分别订阅 16002~16027 和 16022 实测确认 `[未验证]`。

### 1.2 全文反复出现的“查 xx 进程日志”具体指哪里（这次已经查到底）

前面好几处（Step 2、Step 4 等）都写了“查 io_mng/can_dspt 的运行日志”，一直没说清楚具体在哪——现在补上，链路是：

1. **所有进程的 `LOG_I`/`LOG_D`/`LOG_E` 等宏，最终都调用的是 `__android_log_print()`**（`[代码]` 3rdparty/tbox-common/src/logger.h:32-37），也就是说不管哪个 app（io_mng、can_dspt、modem_mng……），日志都写进同一套 Android 风格的内核日志环形缓冲区，设备节点是 `/dev/log_main`（`[代码]` apps/sys_mng/src/sys_log/sys_log.h:15）。
2. **`sys_mng` 进程**（属于 `WITH_RTMS_CORE`，见 Step 0）起一个专门线程 `sys_logcat_thread` 读这个节点，按等级/标签过滤后落盘（`[代码]` apps/sys_mng/src/sys_log/sys_log.c:435-547）。落到哪要看配置（配置项 ID `C030701`~`C030706`，在 `/etc/config/config.json` 里，`[代码]` apps/sys_mng/src/app_mng/app_mng.h:47-52）：

   | 配置项 | 含义 | 默认值 |
   |---|---|---|
   | `C030701` 使能 | 是否落盘 | 1（开） |
   | `C030702` 前缀 | 日志文件名前缀 | `RTMS` |
   | `C030703` 等级 | 输出等级（3=debug） | 3 |
   | `C030704` 单文件大小 | 单个文件多大后轮转，单位KB | 10240（10MB） |
   | `C030705` 文件数量上限 | 超过就删最旧的 | 10 |
   | `C030706` 存储介质 | 0=只输出到 console(stdout)不落盘；1=内存；2=Flash/SD卡 | **2（Flash）** |

   `[代码]` apps/sys_mng/src/app_mng/app_mng.h:84-88（默认值）。存储介质对应的实际目录（`[代码]` apps/sys_mng/src/sys_log/sys_log.h:18-19）：
   - `storage_space=1`（内存）：`/tmp/rtms_log/`
   - `storage_space=2`（Flash，**默认就是这个**）：`/media/sdcard/rtms_log/`

   文件名格式：`{目录}/{前缀}_{YYYY-MM-DD_HHMMSS}.log`，比如 `/media/sdcard/rtms_log/RTMS_2026-07-08_143022.log`。

   **上表默认值是代码里 `config.json` 读取失败时的兜底值，不等于设备上实际生效的值**——仓库里 `apps/rtms_client/doc/config.json` 这份参考配置样例，实际写的是 `C030701=0`（**日志直接禁用**）、`C030706=1`（内存 `/tmp/rtms_log/`，重启就丢，不是 Flash）、`C030704=8`（单文件 8KB）、`C030705=100`（100个文件）——跟代码兜底值明显不一样（`[代码]` apps/rtms_client/doc/config.json:140-145）。**这份是不是同事设备上实际部署的配置，本次没法确认，只能说明"别直接信默认值，先去查这台设备真实的 `/etc/config/config.json`"** `[未验证]`。

3. **所有进程共用同一批日志文件**，靠每个进程启动时调用的 `logSetTag(名字)` 来区分是谁打的（`[代码]` apps/io_mng/main.c:46、apps/can_dspt/main.c:32 分别设成各自进程名）。单条日志格式是 `时间戳.毫秒 等级字符/TAG : 消息内容`（`[代码]` apps/sys_mng/src/sys_log/sys_log.c:530）。所以查某个进程（比如 can_dspt）的日志，直接：
   ```sh
   grep "can_dspt" /media/sdcard/rtms_log/RTMS_*.log
   # 或者只看最新文件
   tail -f /media/sdcard/rtms_log/RTMS_*.log | grep "io_mng"
   ```
   如果设备上翻不到这个目录，先确认一下 `C030706` 配置是不是被改成了 0（console，不落盘）或 1（改到了 `/tmp`，重启即丢）。

这条结论对 apps/io_mng、apps/can_dspt 等所有走 `logger.h` 这套宏的进程都适用（**前提是 `sys_mng` 本身在跑**，如果 `sys_mng` 没起来，`/dev/log_main` 里的日志没人读、也不会落盘，只能确认进程是否真的在写日志，落不了盘）。

---

## 2. 关键端口 / 数据结构速查

### 2.1 端口表

| 端口 | 协议 | 绑定方 | 连接方 | 内容 |
|---|---|---|---|---|
| tcp://127.0.0.1:16002 | Nanomsg PUB | io_mng | can_dspt | can0 原始帧 `[代码]` apps/io_mng/src/io_mng/io_mng.h:61, apps/can_dspt/src/can_server/can_server.h:9 |
| tcp://127.0.0.1:16003 | Nanomsg PUB | io_mng | can_dspt | can1 原始帧 |
| tcp://127.0.0.1:16004 | Nanomsg PUB | io_mng | can_dspt | can2 原始帧 |
| tcp://127.0.0.1:16023~16027 | Nanomsg PUB | io_mng | can_dspt | can3~can7 原始帧 |
| tcp://127.0.0.1:16022 | Nanomsg PUB | io_mng | 无（can_dspt 不连这个口） | **非 IMX6 平台专用**：MCU 走 `TAG_CAN_MIXED` 上报时的混合通道，自带 channel 字段，见 1.1 `[代码]` apps/io_mng/src/io_mng/io_mng.h:60/94, apps/can_mixed.h:6-13 |
| udp://127.0.0.1:16029 | UDP | can_dspt | 任意客户端 | 下发/撤销 CAN ID 过滤规则 `[代码]` apps/can_dspt/src/data_process/data_process.c:128 |
| tcp://127.0.0.1:16028 | Nanomsg PUB | can_dspt | can_client 等 | 过滤后、带 channel 字段的 CAN 帧 `[代码]` apps/can_dspt/src/can_server/can_server.c:146 |

八路原始端口与 io_mng 的宏定义逐一核对过，完全一致：`[代码]` apps/io_mng/src/io_mng/io_mng.h:60-68 与 apps/can_dspt/src/can_server/can_server.h:9-16。

### 2.2 数据结构

原始帧（16002~16027 上，每条 20 字节，一次 nn_recv 可能包含多条，需按 `sizeof(can_frame_t)` 切片）：
```c
typedef struct {
    uint32_t can_id;
    uint16_t can_dlc;
    uint16_t rsv_ms;     // mcu uptime
    uint8_t  data[8];
    uint32_t timestamp;  // rtc时间戳
} can_frame_t;           // 4+2+2+8+4 = 20 字节
```

过滤后扩展帧（16028 上，每条 24 字节）：
```c
typedef struct {
    uint32_t can_id;
    uint16_t can_dlc;
    uint16_t rsv_ms;
    uint8_t  data[8];
    uint32_t timestamp;
    uint8_t  channel;    // 0~7，对应 can0~can7，即上面 16002/16003/... 的通道号
    uint8_t  rsv[3];
} can_frame_ext_t;       // 20+1+3 = 24 字节
```
`[代码]` apps/can_dspt/src/can_server/can_server.h:18-36。**“在哪个通道”就看这个 channel 字段**，它是 can_dspt 收原始帧时按端口下标直接写入的（`[代码]` apps/can_dspt/src/can_server/can_server.c:13-17,55：`dispatch_can(..., channel=i)`，`i` 是 16002+i 对应的通道号）。

---

## 3. 排查步骤（自下而上）

### Step 0：确认相关进程是否真的在跑

`[代码]` apps/CMakeLists.txt:15-24 — `WITH_RTMS_CORE` 只包含 `rtms_client / sys_mng / io_mng / sw_mng / network_mng / modem_mng`，**不包含 can_dspt 和 can_client**。这两个各自有独立开关：`WITH_CAN_DSPT`（apps/CMakeLists.txt:105）、`WITH_CAN_CLIENT`（apps/CMakeLists.txt:57），默认都是 OFF。

排查第一步永远是确认这两个进程存在且在跑：
```sh
ps | grep -E "io_mng|can_dspt|can_client"
```
如果 `can_dspt` 根本没起来，后面全白查——先确认固件打包时这个 app 有没有编进去、有没有被启动脚本拉起。

### Step 1：SocketCAN 物理层是否真的有数据（**仅 IMX6 平台适用**，完全独立于 RTMS 框架）

**先确认平台**：这一步只对 IMX6（`QL_MODULE_PLATFORM_MCIMX6Y2CVM08AB`）成立——该平台的 io_mng 走标准 Linux SocketCAN，接口名固定为 `can0`、`can1`…（`[代码]` apps/io_mng/src/can/can_socket.c:247 `snprintf(ifr.ifr_name, ..., "can%d", channel)`，文件头 `#if defined(QL_MODULE_PLATFORM_MCIMX6Y2CVM08AB)`）。EC200A/EG25G 等平台的 CAN 数据来自 1.1 说的 MCU USB/SPI 链路，**没有 `can0` 这个 Linux 网络接口**，本步骤跳过，直接从 Step 2 开始（或者说，对这些平台，"链路层"就是 1.1 提到的 USB/SPI 链路，目前没找到能在命令行直接探测它的方法）。

IMX6 上这一层最快、最不依赖框架本身是否有 bug：

```sh
ip -s link show can0      # 看 RX 包数是否在涨，波特率/总线状态(ERROR-ACTIVE等)
# 若设备上有 can-utils：
candump can0              # 直接肉眼看原始报文
```
如果这里都没有数据，说明问题在总线接线/波特率配置/外部设备，跟 RTMS 框架完全无关，不用往上查了。

### Step 2：io_mng 是否把该通道 socketcan 接口配置起来了（同样仅 IMX6 适用）

`[代码]` apps/io_mng/src/can/can_socket.c 里有日志：
```
"can%d iface is ready\n"     // 正常
"can%d iface is not ready\n" // 异常
"close can%d socket fd..."   // 配置变化重建
```
查 io_mng 的运行日志/标准输出里有没有这些行，确认对应通道波特率(BTR)配置下发成功、接口是 up 的。

### Step 3：io_mng 有没有把原始帧转发到对应端口

用 nanomsg 自带的命令行工具 `nanocat`（源码里能编出来，`3rdparty/nanomsg` 构建产物为 `build_dir/bin/nanocat`；是否已经打进设备镜像 `[未验证]`，需要在设备上 `which nanocat` 确认）直接订阅某个通道的原始端口：

```sh
# 以 can0 (端口16002) 为例，其余通道换端口号即可对照 2.1 表格
nanocat --sub --connect tcp://127.0.0.1:16002 --subscribe "" --format hex
```
只要总线上有报文，这里应该能立刻看到二进制内容在刷（每条 20 字节，按 2.2 的 `can_frame_t` 手工切）。**这一步能确认“数据到没到 io_mng 这一层，以及具体是哪个通道端口在收”**——不需要 can_dspt 参与。

**非 IMX6 平台**：如果 16002~16027 这几个端口订阅后一直没有数据，别急着下结论，按 1.1 的说明再试一下混合端口：
```sh
nanocat --sub --connect tcp://127.0.0.1:16022 --subscribe "" --format hex
```
这个端口上的数据结构是 `can_msg_mixed_item_t`（id/dlc/timestamp_ms/dt[8]/timestamp/channel/rsv[3]，共 24 字节，和 2.2 的 `can_frame_ext_t` 布局一致），自带 channel 字段，能同时回答"有没有数据"和"哪个通道"。这台设备的 MCU 固件到底走 16002~16027 分通道上报还是走 16022 混合上报，本次没有找到判断依据，两个都试一下即可。

### Step 4：can_dspt 是否连上了 io_mng 的原始端口

can_dspt 启动时会为每个通道打印：
```
connected to can%d port
```
`[代码]` apps/can_dspt/src/can_server/can_server.c:105。查 can_dspt 的标准输出/日志，确认已连接的通道数和板卡实际通道数一致——通道数由 `/opt/board_info.json` 里的 `can_nb` 字段决定，解析失败则默认按 2 路处理（`[代码]` apps/can_dspt/src/can_mng/can_mng.c:10-40）。可以先 `cat /opt/board_info.json` 看看这台设备声明了几路 CAN。

### Step 5：【最容易被忽略的坑】can_dspt 的过滤表是不是空的

can_dspt 对外只广播“被订阅过的 CAN ID”：`dispatch_can()` 里用 `HASH_FIND_INT` 命中才写入环形缓冲、才会出现在 16028 上；没命中的帧直接丢弃，**不做任何日志**（`[代码]` apps/can_dspt/src/can_server/can_server.c:13-28）。也就是说：

> 即使 CAN 总线上一直有报文、io_mng 也在正常转发，只要没人往 16029 下发过 CAN ID 订阅，16028 上会永远是空的——这不是故障，是设计如此。

**⚠️ 操作风险提醒**：这份订阅表是**全局唯一、所有消费端共用**的一个哈希表，不是"谁订阅归谁"（`[代码]` apps/can_dspt/src/data_process/data_process.c:19-101，`HASH_ADD_INT`/`HASH_DEL` 直接操作 `can_mng_handle->hash_head` 这一份表，没有按客户端区分、也没有引用计数）。如果这台设备上 `can_client` 等正式消费端已经在跑，**排查完随手 `delete` 掉自己加的测试 ID 之前，一定要确认这个 ID 不是别的正式消费端也在用的**——因为 delete 是按 CAN ID 值删的，不管这个 ID 是谁加的，一旦删掉，所有依赖这个 ID 的消费端会一起断流，而且不会有任何报错提示你"删错了"。

下发订阅（CAN ID 用 `0x` + 十六进制，不需要强制补齐到8位，示例里补了8位是历史用法）：
```sh
echo "{\"action\":\"add\",\"list\":[\"0x18FF1234\",\"0x18FF4321\"]}" | nc -u 127.0.0.1 16029
```
撤销：
```sh
echo "{\"action\":\"delete\",\"list\":[\"0x18FF1234\",\"0x18FF4321\"]}" | nc -u 127.0.0.1 16029
```
`[代码]` apps/can_dspt/README.md:18-30，apps/can_dspt/src/data_process/data_process.c:19-101。

注意过滤是按 `can_frame_t.can_id` 精确匹配的原始值，如果总线用的是 J1939 扩展帧，`can_id` 里可能包含扩展帧标志位等，实际值要以 Step 3 抓到的原始帧为准，不要凭猜测去订阅一个 CAN ID。

**这里还有一个更深的坑，回答"CAN 相关排查是否齐全"这个问题时必须提一下**：这份过滤表**完全没有持久化**——`can_mng_init()` 里 `hash_head` 直接初始化成 `NULL`（`[代码]` apps/can_dspt/src/can_mng/can_mng.c:42-62），`can_dspt` 的 `main()` 里也没有任何从文件加载已订阅 ID 列表的代码（`[代码]` apps/can_dspt/main.c 全文）。也就是说：

> **只要 can_dspt 进程重启一次（崩溃、被看门狗杀、升级重启、设备重启……），之前下发的所有订阅全部清空，16028 立刻变回没有任何数据，直到有人重新发送 UDP add 命令。**

而且——本次在整个仓库里（包括 `can_client`、`forward_can`、`gb32960_client_for_dima` 等下游消费端源码，以及部署产物目录）**没有找到任何一个消费端在自己启动时主动向 16029 重新下发订阅**（`can_client` 源码里完全没有出现 16029/UDP 订阅相关代码；唯一两处出现同款"add/delete" UDP 监听代码的是 can_dspt 自己和 `apps/collect_vbat`，但那是 collect_vbat 自带的独立同款服务端代码，不是"下发订阅"的客户端）。换句话说：**谁负责在 can_dspt 重启后把订阅重新灌回去，本次没有在代码里找到答案** `[未验证]`——如果同事发现"昨天还有数据，今天突然没了"，can_dspt 是不是刚好重启过、订阅是不是被清空了，是排查 CAN 数据丢失时应该优先确认的一环，比怀疑总线或 can_dspt 本身有 bug 更值得先查。

### Step 6：在 16028 上验证过滤后的数据 + 确认 channel

```sh
nanocat --sub --connect tcp://127.0.0.1:16028 --subscribe "" --format hex
```
拿到的每 24 字节按 2.2 的 `can_frame_ext_t` 切，**第 17 个字节（偏移 16，紧跟在 timestamp 后面）就是 channel（0~7）**，直接读出来即是“数据在哪个通道”的答案。

### Step 7：下游消费者复核

`can_client`、`forward_can` 等都是用同样的方式 `NN_SUB` 连接 16028、订阅空字符串收全量（`[代码]` apps/can_client/src/can_server/can_server.c:153-172）。如果 Step 6 在 16028 上已经能看到数据，但某个具体消费端还是说没数据，问题就在那个消费端自己的解析/过滤逻辑里，和 io_mng/can_dspt 无关，需要单独看该消费端代码。

---

## 4. 常见“没数据”根因清单

| 现象 | 可能原因 | 对应排查 Step |
|---|---|---|
| 所有层都没数据 | can_dspt/can_client 进程没编译进固件或没启动 | Step 0 |
| SocketCAN 层都没数据 | 总线接线/波特率/外部设备问题，与框架无关 | Step 1 |
| 原始端口（16002等）没数据 | io_mng 该通道没配置起来 / board_info.json 通道数配错 | Step 2、3 |
| 原始端口有数据，16028 没数据 | **没人下发过 CAN ID 订阅（最常见）**，或订阅的 ID 和实际总线上跑的对不上 | Step 5 |
| 16028 有数据，具体消费端说没数据 | 消费端自身逻辑问题 | Step 7 |
| 只有部分通道有数据 | 对照 board_info.json 的 can_nb 和物理接线通道数是否一致 | Step 2、3 |
| 非 IMX6 平台：16022 有数据，16002~16027 没数据，can_dspt/16028 也没数据 | **MCU 固件走的是 `TAG_CAN_MIXED`（混合上报）而不是分通道的 `TAG_CAN0~7`；can_dspt 的代码只连接 16002~16027，不连接 16022**（`[代码]` apps/can_dspt/src/can_server/can_server.c:75-108 硬编码的就是 CAN0~7_SKT_PORT），这种情况下 can_dspt 这条链路天生连不上，要么改用 16022 直接订阅，要么确认 can_dspt 是否需要适配 | Step 3 |
| **之前一直有数据，突然全没了**（16002~16027/16022 有数据，但 16028 空了） | can_dspt 重启过，订阅表被清空（过滤表不持久化，见 Step 5），且没有任何模块自动重新下发订阅 | Step 5 |

---

## 5. 命令速查

```sh
# 进程是否在跑
ps | grep -E "io_mng|can_dspt|can_client"

# 板卡声明的CAN通道数
cat /opt/board_info.json

# SocketCAN 物理层（与框架无关，最快定位是否总线本身没数据）
ip -s link show can0
candump can0                                   # 需设备上有 can-utils，[未验证]是否已安装

# 订阅某路 io_mng 原始转发端口（通道号→端口号见2.1表）
nanocat --sub --connect tcp://127.0.0.1:16002 --subscribe "" --format hex

# 给 can_dspt 下发/撤销 CAN ID 过滤订阅
echo '{"action":"add","list":["0x18FF1234"]}' | nc -u 127.0.0.1 16029
echo '{"action":"delete","list":["0x18FF1234"]}' | nc -u 127.0.0.1 16029

# 订阅 can_dspt 过滤后的统一出口（每条24字节，偏移16处的1字节是channel）
nanocat --sub --connect tcp://127.0.0.1:16028 --subscribe "" --format hex
```

---

## 6. 其他排查/使用手段

### 6.1 更直接的方法：向 io_mng 要“收发计数器”，不用抓包解析二进制

除了 Step 3/6 那种拿原始报文肉眼看之外，io_mng 自己维护了每路 CAN 的收发统计，并且开了一个 **请求/应答（Nanomsg REQ/REP）** 接口可以直接查——这是比抓包更快确认“有没有数据、在哪个通道”的办法，因为不用管 CAN ID 过滤、不用切二进制，直接拿数字。

- 端口：`tcp://127.0.0.1:38000`（`IPC_URL_PERIPH_REQREP`），io_mng 侧 `nn_bind` 在此（`[代码]` apps/io_mng/src/io_mng/io_mng.h:92，apps/io_mng/src/cmd/cmd_sts_req.c:1418-1419）。
- 请求格式（发什么问什么，`ts` 字段可省略，实际只解析 `status.io` 数组）：
  ```json
  {"status":{"io":["can"]}}
  ```
  `[代码]` apps/io_mng/src/cmd/cmd_sts_req.c:1033-1063（解析逻辑，只认 `status.io` 里的字符串，"can"/"com"/"gps" 等）。
- 命令行验证（nanocat 支持 `--data` 发送 REQ 报文）：
  ```sh
  nanocat --req --connect tcp://127.0.0.1:38000 --data '{"status":{"io":["can"]}}' --format ascii
  ```
- 返回的 JSON 里每一路 CAN 是一个 `{"dev":"can","chn":N,"params":{...}}` 对象，关键字段（`[代码]` apps/io_mng/src/cmd/cmd_sts_req.c:464-486）：

  | 字段 | 含义 |
  |---|---|
  | `chn` | 通道号（对应 can0~can7） |
  | `mcu_rx` / `mcu_tx` | MCU 侧统计的收/发帧数 |
  | `mcu_rx_error` | MCU 侧收帧错误计数 |
  | `mcu_rx_ovs` / `mcu_tx_ovs` | MCU 侧收/发溢出计数 |
  | `mng_rx` / `mng_tx` | io_mng 自己统计的收/发帧数 |
  | `mng_rx_ovs` / `mng_tx_ovs` | io_mng 侧收/发溢出计数 |
  | `btr` / `btr_type` | 当前波特率、是手动配置(user_btr)还是自动探测(auto_btr) |

  **两次请求间隔几秒，对比 `mcu_rx`/`mng_rx` 是否在涨，就直接回答了“这一路有没有数据进来”；`chn` 就直接是通道号。** 如果 `mcu_rx` 在涨但 `mng_rx` 不涨，说明问题出在 MCU→io_mng 这一段内部转发，而不是总线本身没数据——这是抓原始报文（Step 3）分不出来的细节。

- **这套查询方式不止 CAN 能用**，`status.io` 数组里能放的 key 一共 7 个：`can`/`com`/`do`/`ao`/`di`/`ai`/`mcu`（`[代码]` apps/io_mng/src/cmd/cmd_sts_req.c:1054-1128 逐个 `strcmp` 分支），换掉请求里的 key 就能查其他外设，返回的 `dev` 字段和参数如下：

  | 请求 key | 返回 `dev` | 关键参数 | 代码位置 |
  |---|---|---|---|
  | `can` | `can` | 见上表（mcu_rx/mng_rx/btr等） | cmd_sts_req.c:445-489 |
  | `com` | `com` | mcu_rx/mcu_tx/mng_rx/mng_tx/btr（同 CAN 逻辑，串口收发计数） | cmd_sts_req.c:492-533 |
  | `do` | `do` | `level`（数字量输出电平） | cmd_sts_req.c:536-560 |
  | `ao` | `ao` | `voltage`（模拟量输出电压） | cmd_sts_req.c:563-587 |
  | `di` | `di` | `level`、`input_count`（数字量输入电平+触发计数，**计数会不会涨是判断输入信号有没有变化的直接依据**） | cmd_sts_req.c:630-655 |
  | `ai` | `ai` | `voltage`（模拟量输入电压） | cmd_sts_req.c:658-683 |
  | `mcu` | `mcu` | `mcu_pwd_volt`/`mcu_bat_volt`/`mcu_temperature`/`mcu_boot_src`/`acc_stat`(ACC状态)/`ig_stat`(点火状态)/固件版本 | cmd_sts_req.c:609-628 |

  例如查数字量输入（DI）有没有触发：
  ```sh
  nanocat --req --connect tcp://127.0.0.1:38000 --data '{"status":{"io":["di"]}}' --format ascii
  ```
  也可以一次问多个：`{"status":{"io":["can","di","mcu"]}}`。

  **例外：`gps` 这个 key 虽然能被解析（`reqTypeMask` 会被置位），但在实际组装回复的 `cmd_sts_handle_reply()` 里，`switch` 语句只处理了 `PERIPH_TYPE_CAN/COM/DO/AO`（走 MCU 请求-应答队列的分支）和 `PERIPH_TYPE_DI/AI`（走本地缓存分支），完全没有 `PERIPH_TYPE_GPS` 的 `case`（`[代码]` apps/io_mng/src/cmd/cmd_sts_req.c:749-785、794-814 通读确认，两处 switch 都没有 GPS 分支）。也就是说，**请求里带 `"gps"` 目前不会在这条 REQREP 通道上得到任何 GPS 字段**，查 GPS 数据要用第 7 节的方法（订阅 16005/16010 端口），不要在这里等它。

- **平台差异 `[代码]`**：这套统计逻辑在 apps/io_mng/src/cmd/cmd_sts_req.c 里是 `#if defined(QL_MODULE_PLATFORM_EC200A) || defined(QL_MODULE_PLATFORM_EG25G)`（cmd_sts_req.c:1），即目前确认覆盖 EC200A / EG25G。IMX6 有独立实现 apps/io_mng/src/cmd/cmd_sts_req_imx6.c，同样定义了 `PERIPH_TYPE_CAN` 和 `cmd_sts_pack_can_response`，看起来是等价的，但具体细节未逐行比对 `[未验证]`。AG35/RK3576 是否有对应实现本次未核实 `[未验证]`。

- **这套机制是真实在用的，不是摆设**：rtms_client 自己就是这个接口的正式客户端，每隔一定周期轮询它来拼云端上报的状态（`[代码]` apps/rtms_client/src/data_proc/data_proc.c:1245-1247：`peripheral_send_request/peripheral_rcv_cb` 连的就是 `IPC_URL_PERIPH_REQREP`）。反而各消费端项目（can_client、gb_client_for_mixer 等）里各自的 `mcu.c` 里那份请求代码都包在 `#ifdef NOT_USED_SCHEMA` 里，是死代码，不要参考那份抄。

### 6.2 云端侧也能看，不用登录设备

如果同事有平台/云端的访问权限，rtms_client 上报的状态属性表里本来就包含每一路 CAN 的收发计数器，属性名形如 `status.io.can[0].mcu_rx`、`status.io.can[0].mng_rx`、`status.io.can[0].mng_tx`、`status.io.can[0].mcu_rx_error`、`status.io.can[0].btr` 等，通道 0~7 都有对应字段（`[代码]` apps/rtms_client/doc/status_desc.json:6-628，例如 can[0] 的 `S010101`~`S010113`、can[1] 的 `S010201`~... 依次类推）。在云端平台上看这几个属性有没有随时间递增，效果等同于 6.1 的本地查询，只是不用碰设备。

### 6.3 同样方法在其它数据通道上的适用情况（未深入，仅列出入口供参考）

io_mng 不止转发 CAN，还有别的数据用同一套 Nanomsg PUB/SUB 机制往外发，Step 3/6 那种 `nanocat --sub` 的方法同样适用，只是端口和数据结构不同（以下端口均来自 `[代码]` apps/io_mng/src/io_mng/io_mng.h:79-90，具体报文结构本次未逐一深入，标 `[未验证]`）：

| 端口 | 用途 |
|---|---|
| tcp://127.0.0.1:16005 | GPS 定位数据 (`IPC_URL_GPS_PUB`)，详见第 7 节 |
| tcp://127.0.0.1:16006 / 16007 | COM0/COM1 串口透传 (`IPC_URL_COM0_PUB`/`COM1_PUB`)，详见第 8 节 |
| tcp://127.0.0.1:16008 | 命令通道 (`IPC_URL_CMD_PUB`，与 26008 的 `_SUB` 配对) |
| tcp://127.0.0.1:16010 | GPS 相关的串口数据 (`IPC_URL_GPS_COM_PUB`) |
| tcp://127.0.0.1:16011 | 日志相关串口数据 (`IPC_URL_LOG_COM_PUB`) |
| tcp://127.0.0.1:16012 | IMU 数据 (`IPC_URL_IMU_COM_PUB`)，详见第 9 节 |
| tcp://127.0.0.1:48000 | 事件总线 (`IPC_URL_EVT_PUB`) |
| tcp://127.0.0.1:38000 | 外设状态请求/应答（本节 6.1 用的就是这个） |

26002~26027（以及 COM 对应的 26006/26007）这一组 `_SUB` 端口是反方向（应用 → io_mng → 下发到 CAN/COM 总线，即"发数据"而不是"收数据"）——这条 TX 链路确认是真实接通的：应用发到这些端口的字节会被 io_mng 收下、缓存，再周期性地经 USB/SPI 送回 MCU 由 MCU 实际发送到总线上（IMX6 平台上 CAN TX 则是直接 `write()` 到 SocketCAN，`[代码]` apps/io_mng/src/can/can_channel.c:25-44，apps/io_mng/src/io_mng/io_mng.c:180,232,296,344）。具体要发送的报文用什么结构体（`can_msg_item_t`）本次没有找到定义文件核实完整字段 `[未验证]`，如果同事的需求是"往 CAN 总线上发报文"而不只是查，入口就在这里，但发送前建议先确认清楚这个结构体的字段布局，避免发错数据。

### 6.4 更上层的东西（本文档没有覆盖，需要单独排查）

- 消费端自己的业务逻辑（比如 can_client 的场景判断 `apps/can_client/src/scene`、事件触发 `apps/can_client/src/event`）——16028 上有数据但业务没触发，问题在这一层，需要单独看对应消费端的代码。
- rtms_client 把数据/状态上报到云端之后的协议、云端确认方式——本次未审查。
- sw_mng 进程看门狗只做存活检查、不会因为“CAN 没数据”杀掉进程，所以“进程一直在跑但没数据”不代表框架有异常重启逻辑介入；这属于另一个话题，不在本文档范围内。

---

## 7. GPS 数据排查（除 CAN 外最常问的第二个通道）

GPS 跟 CAN 是两套不同的机制：**没有过滤订阅这一关**，只要 io_mng 收到定位模块的数据就会一直往外发；但也**不走 6.1 的 REQREP 查询**（上一节已确认 `gps` key 是死分支），只能直接订阅端口。

### 7.1 数据从哪来、发到哪

- io_mng 定时（每个定位周期）把当前定位结果打包成 JSON，通过 `set_pubsub_endpoint_snddata` 发到 `IPC_URL_GPS_PUB = tcp://127.0.0.1:16005`（`[代码]` apps/io_mng/src/location/location.c:110-123, 344）。
- JSON 字段（`[代码]` apps/io_mng/src/location/location.c:236-247）：

  | 字段 | 含义 |
  |---|---|
  | `time` | 定位时间戳 |
  | `lat` / `lon` | 纬度/经度 |
  | `velocity` | 速度 |
  | `altitude` | 海拔 |
  | `direction` | 航向 |
  | `accuracy` | 定位精度 |
  | `used_nsat` | 参与定位的卫星数 |
  | `view_nsat` | 可见卫星数 |
  | `mode` | 定位模式（0 通常表示未定位/无效，具体取值定义未深入 `[未验证]`） |
  | `snr` | 信噪比 |
  | `antenna_valid` | GPS 天线是否正常（**天线断开/短路时这个字段和 mode 是最先该看的**） |

### 7.2 排查命令

```sh
nanocat --sub --connect tcp://127.0.0.1:16005 --subscribe "" --format ascii
```
这里发的是 JSON 文本（不是像 CAN 那样的二进制结构体），`--format ascii` 就能直接看懂。**看到 JSON 持续在刷就是“有数据进来”；`used_nsat`>0 且 `antenna_valid`=1（具体“正常值”是1还是0未在文档里找到定义，需结合实际输出判断，标 `[未验证]`）说明定位已生效，`lat`/`lon` 非 0 也是一个直观判断依据。**

如果 16005 上完全没有 JSON 输出：
1. 先按 Step 0 的方法确认 io_mng 进程在跑；
2. 检查天线/模块接线是否正常（`antenna_valid` 字段的前提是硬件层面能收到卫星信号，这一层和 CAN 的 Step 1 SocketCAN 检查类似，与 RTMS 框架无关）；
3. 确认设备当前所在环境有没有卫星信号（室内/地下车库拿不到定位是正常现象，不是 bug）。

### 7.3 更细的原始 NMEA 语句（进阶，未完全走通）

io_mng 还支持按需下发一条“启用 NMEA 透传”的请求，之后会把原始 RMC/GGA/GLL/GSV/GSA/VTG/TXT/ZDA 语句发到 `tcp://127.0.0.1:16010`（`IPC_URL_GPS_COM_PUB`，`[代码]` apps/io_mng/src/location/location.c:125-136, 262-269）——这跟 CAN 那边“不订阅就没数据”的设计是同一个思路的变体：**默认不发原始 NMEA，需要先发一条启用请求**。

这条启用请求现在追到底了：走的是第 12 节介绍的 CMD 通道（`tcp://127.0.0.1:26008` 发请求，`tcp://127.0.0.1:16008` 收应答），`cmd_tag` 是 `CMD_NMEA_REQ`（普通 NMEA 数据）或 `CMD_GPS_COM_REQ`（串口透传），请求体是二进制结构体不是 JSON（`[代码]` apps/io_mng/src/cmd/cmd.h:9-41 `cmd_request_t`，apps/io_mng/src/location/location.h:46-63 `nmea_req_t`/`gps_com_req_t`：`action`(1字节，是否启用)/`interval`(2字节，上报间隔)/`duration`(4字节，持续时长)）。因为是要手工拼二进制包头+结构体，`nanocat --data` 那种纯文本命令不好直接凑，需要写个几行的小脚本或者用 Python `struct.pack` 拼包再发到 26008，本次没有实际拼出来验证过 `[未验证]`。日常排查“有没有定位数据”用 7.2 的 16005 已经够用，这一条只在需要原始 NMEA 语句时才用得上。

---

## 8. COM 串口透传排查（EC200A/EG25G）

COM0/COM1 是 io_mng 帮 MCU 转发的两路串口透传通道，走 1.1 说的同一条 USB/SPI 链路，机制和 CAN 几乎一模一样，只是没有 CAN 那种"过滤订阅"的环节。

- **收（外部设备 → 应用）**：MCU 收到的串口字节，打上 `TAG_COM0`/`TAG_COM1` 标签经 USB/SPI 送到 io_mng，io_mng 存进环形缓冲后周期性通过 `set_pubsub_endpoint_snddata` 发到 Nanomsg PUB（`[代码]` apps/io_mng/src/io_mng/io_mng.c:782, 973-1010）。**发的是原始字节流，没有额外包装**（不像 CAN 有 `can_frame_t` 结构体，也不像 GPS 是 JSON）。
  - COM0 端口：`tcp://127.0.0.1:16006`（`IPC_URL_COM0_PUB`）
  - COM1 端口：`tcp://127.0.0.1:16007`（`IPC_URL_COM1_PUB`）
  ```sh
  nanocat --sub --connect tcp://127.0.0.1:16006 --subscribe "" --format ascii
  ```
  订阅后如果外部设备往对应串口发字符，这里应该原样能看到（文本协议用 `--format ascii` 能直接读；如果是二进制协议换 `--format hex`）。
- **发（应用 → 外部设备）**：应用往 `tcp://127.0.0.1:26006`/`26007`（`IPC_URL_COM0_SUB`/`COM1_SUB`）发的字节，会被 io_mng 收下（`com0_ipc_rcv_cb` → `push_com_tx_items`，`[代码]` apps/io_mng/src/com/com_process.c:24-49），再由周期性任务 `pop_com_tx_items` 取出、打包发回 MCU 由串口物理发送（`[代码]` apps/io_mng/src/io_mng/io_mng.c:296, 344）——这条 TX 链路也是真实接通的，不是摆设。

有收发计数器（`mcu_rx`/`mcu_tx`/`mng_rx`/`mng_tx`）可以查，用法和第 6.1 节 CAN 部分完全一样，只要把请求 key 换成 `"com"` 即可：
```sh
nanocat --req --connect tcp://127.0.0.1:38000 --data '{"status":{"io":["com"]}}' --format ascii
```

同一套通道机制还派生出两个专用变体，用法一样，只是端口不同：
- GPS 相关串口透传：`tcp://127.0.0.1:16010`（`TAG_GPS_COM`，`[代码]` apps/io_mng/src/io_mng/io_mng.c:835-848）——和第 7.3 节提到的"启用 NMEA 透传"是同一个底层通道。
- 日志相关串口：`tcp://127.0.0.1:16011`（`TAG_LOG_COM`，`[代码]` apps/io_mng/src/io_mng/io_mng.c:849-862）。

## 9. IMU 数据排查（EC200A/EG25G）

IMU（惯性测量单元，姿态/加速度传感器）同样走 1.1 的 USB/SPI 链路，`TAG_IMU_COM` 标签的数据会被整段拷贝进一个最大 128 字节的缓冲区，原样转发（`[代码]` apps/io_mng/src/imu_com/imu_com.h:6-9 `imu_buf[128]`；apps/io_mng/src/io_mng/io_mng.c:863-870 拷贝逻辑，1175-1180 周期性 `set_pubsub_endpoint_snddata` 发送）：

```sh
nanocat --sub --connect tcp://127.0.0.1:16012 --subscribe "" --format hex
```

**IMU 数据里面具体是哪几个字节代表加速度/角速度/温度等字段，这个代码仓库里没有解析、也没有注释定义** —— io_mng 只是原样透传 MCU 发来的字节，具体格式需要找 MCU 固件那边的通信协议文档，本次没有找到，标 `[未验证]`。所以这里能确认的只有"有没有数据在刷"（判断链路通不通），具体数值含义解不出来。

如果同事关注的是"IMU 有没有数据"这种问题，上面的命令能直接回答；如果要进一步解析里面的字段，需要先拿到 MCU 协议文档。

---

## 10. DI/DO/AI/AO 补充：两种查法的区别 + 怎么控制输出

第 6.1 节已经给过 DI/AI/DO/AO 的 REQREP 查询方法（`{"status":{"io":[...]}}`），这里补两件事：

### 10.1 DI/AI 是“本地缓存”，CAN/COM/DO/AO/MCU 是“现场问 MCU”

读 `cmd_sts_req.c` 的分发逻辑发现一个有意思的区别（`[代码]` apps/io_mng/src/cmd/cmd_sts_req.c:723-819）：
- **DI/AI**：MCU 会持续主动上报（`TAG_DI`/`TAG_AI`，见第 1.1 节），io_mng 一直把最新值缓存在 `pDiItems`/`pAiItems` 数组里（`[代码]` apps/io_mng/src/io_mng/io_mng.c:806-834）。查询时直接读缓存，**立即返回，不用等 MCU**。
- **CAN/COM/DO/AO/MCU**：这几类走一个"异步请求-应答队列"，查询时才真的往 MCU 发一条请求，等 MCU 回应后才能拼出结果（`reqNeedMask` 标记，`[代码]` cmd_sts_req.c:727-741）。**如果 MCU 暂时没响应或响应慢，这几类的查询可能超时/拿不到数据**，这跟"这一路本身有没有数据"是两回事，排查时不要搞混——查 CAN/COM 状态卡住或超时，先怀疑 MCU 通信本身（回到 1.1 的 USB/SPI 链路），不要直接怀疑总线没数据。

### 10.2 DO/AO 不只能查，还能控制（CMD 通道，二进制协议）

DO（数字量输出，比如继电器）、AO（模拟量输出）不仅能查状态，还能通过第 12.2 节的 CMD 通道下发控制命令：`CMD_DO_CTRL` 设置输出电平、`CMD_AO_CTRL` 设置输出电压（`[代码]` apps/io_mng/src/cmd/cmd.h:16-17）。如果同事的需求是"用框架控制一路输出"而不只是查状态，入口在这里；跟 7.3 的 GPS NMEA 请求一样是二进制协议，具体 `cmd_bytes` 里的字段布局本次没有找到对应的结构体定义，需要进一步找 `apps/io_mng/src/gpio`/`apps/io_mng/src/adc` 目录确认 `[未验证]`。

---

## 11. EVT 事件总线 + CMD 命令通道

### 11.1 EVT（48000）：故障/恢复事件，排查“为什么突然没数据了”的好帮手

io_mng 会在特定条件触发时主动推送一条事件 JSON 到 `tcp://127.0.0.1:48000`（`IPC_URL_EVT_PUB`），格式（`[代码]` apps/io_mng/src/evt_proc/evt_proc.c:24-30 注释）：
```json
{"ts":1678083450000,"event":{"all":{"names":["ts","event_code","description"],"data":[[1678083450000,200,""]]}}}
```
**这个端口不是持续心跳，只有真的发生事件才会有输出**——订阅后长时间没数据是正常的，不代表链路断了。已确认的事件码（`[代码]` apps/io_mng/src/evt_proc/evt_proc.h:9-46）跟本文档几个通道直接相关：

| 事件码 | 含义 |
|---|---|
| 200 / 201 | CAN 总线 bus-off 触发 / 恢复——**如果 16028 突然没数据了，先来这里看看是不是 200 先响了** |
| 232/233、234/235、236/237 | CAN0/CAN1/CAN2 单独的错误触发/恢复 |
| 218/219 | GPS 模块错误触发/恢复 |
| 220/221 | GPS 天线触发/恢复（呼应第 7 节 `antenna_valid` 字段） |
| 222/223 | GPS 定位状态触发/恢复 |
| 226/227 | IMU 错误触发/恢复 |
| 202/203、204/205 | 外部电源电压过高/过低触发/恢复 |
| 258/259、260/261 | 电池电压过高/过低触发/恢复 |
| 262/263 | ACC（点火）信号错误触发/恢复 |
| 248/249、250/251 | RTC 芯片/时间错误触发/恢复 |

```sh
nanocat --sub --connect tcp://127.0.0.1:48000 --subscribe "" --format ascii
```
排查思路：**某个通道数据突然消失时，先订阅 48000 看看有没有对应的错误事件，比自己去猜是过滤规则问题还是硬件问题要快。**

### 11.2 CMD（16008 收应答 / 26008 发请求）：通用控制/查询命令通道

这是一个通用的二进制请求-应答通道，本文档前面提到的好几个"没查完"的东西都是通过它实现的（GPS NMEA 启用见 7.3、DO/AO 控制见 11.2）。协议头 `[代码]` apps/io_mng/src/cmd/cmd.h:9-53：

```c
typedef struct {
    uint32_t pid;       // 请求方进程号
    uint16_t cmd_tag;   // 命令类型，见下表
    uint8_t  cmd_idx;   // 请求序号，应答里会原样带回，用来对上号
    uint8_t  cmd_len;   // cmd_bytes 长度
    uint8_t  rsv[4];
    char     cmd_bytes[]; // 具体命令内容，按 cmd_tag 解释
} cmd_request_t;
```

支持的 `cmd_tag`（`[代码]` apps/io_mng/src/cmd/cmd.h:9-31）：

| cmd_tag | 用途 |
|---|---|
| `CMD_CAN_SET`/`CMD_COM_SET`/`CMD_GPS_SET`/`CMD_DI_SET`/`CMD_AI_SET` | 配置对应外设参数（波特率等），具体字段未深入 `[未验证]` |
| `CMD_DO_CTRL`/`CMD_AO_CTRL` | 控制数字量/模拟量输出，见 11.2 |
| `CMD_LED_CTRL` | 控灯 |
| `CMD_TIME_SET` | 设置时间 |
| `CMD_NMEA_REQ`/`CMD_GPS_COM_REQ` | 启用 GPS 原始 NMEA/串口透传，见 7.3 |
| `CMD_MCU_LOG_REQ` | 拉取 MCU 日志 |
| `CMD_MCU_INFO_REQ`/`CMD_CAN_INFO_REQ`/`CMD_COM_INFO_REQ`/`CMD_DO_INFO_REQ`/`CMD_AO_INFO_REQ` | 第 6.1 节 REQREP(38000) 查询在更底层实际发的就是这些命令 |
| `CMD_TAG_LOCK`/`CMD_TAG_FOTA` | 锁车/固件升级相关，未深入 `[未验证]` |

这是个二进制协议，不是 JSON，纯命令行 `nanocat --data` 不太好拼二进制包，日常排查（确认数据、查计数器）用前面几节的方法已经够用，这个通道主要在需要"主动控制"（下发命令而不是被动查询）时才用得上。

---

## 12. 未验证/需上机确认的事项

- Step 5 提到的 can_dspt 订阅表重启后清空的问题：整个仓库（含消费端源码和部署产物目录 `todel/`）里没有找到任何"进程启动时自动向 16029 重新下发订阅"的代码，到底是有一个本次没找到的外部机制在做这件事，还是这确实是个需要人工介入的操作缺口，需要找团队里更熟悉现场部署流程的人确认。
- `nanocat` 是否已经打进目标设备镜像（源码能编出来，但安装规则未逐一核实每个平台的 package_install 是否包含它）。
- `candump`/can-utils 是否已安装在目标设备上。
- 实际板卡的物理 CAN 通道数、接线方式（需要看具体项目的 board_info.json 和硬件文档）。
- rtms_client 把 CAN 数据继续上报云端之后的链路（上报协议、云端确认方式）未在本次审查范围内。
- 6.1 的 PERIPH_REQREP 状态查询在 IMX6（cmd_sts_req_imx6.c）上是否逐字段等价，未逐行比对；在 AG35/RK3576 上是否存在同等实现，未核实（这两个平台的 io_mng 主文件本次没有找到，只确认了 EC200A/EG25G 用 io_mng.c、IMX6 用 io_mng_imx6.c）。
- 6.3 里 EVT(48000)/CMD(16008) 两个端口，第 11 节已经补充深入；COM/IMU 在第 8、9 节。
- 11.2/12.2 提到的 `CMD_DO_CTRL`/`CMD_AO_CTRL`/`CMD_CAN_SET`/`CMD_COM_SET`/`CMD_GPS_SET`/`CMD_DI_SET`/`CMD_AI_SET` 这几个命令 `cmd_bytes` 里具体的结构体字段布局，本次没有找到对应定义（可能在 apps/io_mng/src/gpio、apps/io_mng/src/adc 或 MCU 协议文档里），只确认了 `cmd_tag` 编号本身。
- 12.2 的 CMD 二进制协议本次没有实际构造一个请求包发送验证过，`nmea_req_t`/`cmd_request_t` 的字段布局是读代码得出的，未经真机测试。
- 7.1 GPS JSON 里 `mode` 字段各取值的含义、`antenna_valid` 正常值是 0 还是 1，没有在代码注释或文档里找到定义，需要上机实测对照。
- 7.3 GPS 原始 NMEA 透传的启用请求走哪个通道、什么结构体已经查清楚了（见更新后的 7.3 节），但没有实际拼包发送验证过，标 `[未验证]`。
- 1.2 提到的日志配置（存储介质/单文件大小/文件数量）代码兜底默认值和仓库里 `apps/rtms_client/doc/config.json` 样例值不一致，具体这台设备实际生效的是哪套，需要直接查看设备上的 `/etc/config/config.json`。
- 1.1 提到的 io_mng↔MCU USB/SPI 链路本身，命令行上怎么直接判断"链路通不通"（比如有没有类似 `dmesg`/`lsusb`/SPI 设备节点可以看），本次没有找到答案。
- EC200A/EG25G 上 CAN 具体走 `TAG_CAN0~7`（16002~16027）还是 `TAG_CAN_MIXED`（16022），取决于对接的 MCU 固件，本次没有找到能直接判断走哪种的配置项或文档，需要实测两个端口。
- 9. IMU 数据里各字节代表的具体物理量（加速度/角速度/温度等字段的偏移和单位），本仓库没有解析代码也没有协议注释，需要找 MCU 端协议文档。
- IMX6 平台的 io_mng_imx6.c 本次完全没有读取内部实现，COM/IMU/DI/AI 在 IMX6 上是否存在、走什么端口，未核实。
