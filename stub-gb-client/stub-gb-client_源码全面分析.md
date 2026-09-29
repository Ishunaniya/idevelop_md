# stub-gb-client 源码全链路分析

> 分析对象：`/home/tronlong/lyp/code/stub-gb-client`，Git 分支 `master`，提交 `163def2`。本文只依据此提交的源码、`README.md`、`CMakeLists.txt` 和 `stub.ini`。路径和行号以该提交为准。文中的“发送”若无特别说明，仅指代码执行了本地写入或入队；平台实际接收需要设备日志与平台记录确认。

## 文档目录

建议按“一、二、三、四”建立整体认识，再按问题进入后续章节。

1. [一、项目概述](#一项目概述)：程序定位、边界和阅读前提。
2. [二、源码目录总览](#二源码目录总览)：目录树与文件职责。
3. [三、核心架构设计](#三核心架构设计)：系统边界、线程和两条对外路径。
4. [四、核心模块深度分析](#四核心模块深度分析)：入口、采集解析、两套协议、签名、SDK 事件及持久化。
5. [五、关键数据流与状态机](#五关键数据流与状态机)：从启动到备案、登录、实时与补传。
6. [六、运行参数、配置与构建](#六运行参数配置与构建)：支持矩阵、设备文件和 SDK 构建依赖。
7. [七、源码可确认的限制与风险](#七源码可确认的限制与风险)。
8. [八、设备故障排查](#八设备故障排查)：按症状定位并保留现场证据。
9. [九、术语与核验范围](#九术语与核验范围)。

第四节的模块顺序与报文经过的主要代码路径一致：`main/t4hj_main` → `local_skt` → `can_parse` → `fei_4/gb6` → `data_signature` → `nanomsg/stub-linker`；`db/INI` 提供持久化。

## 一、项目概述

该程序的 CMake 目标为 `stub-gb`，属于 `rootcloud-stp-sdk` 的 SCP 适配器工具。它接收本机 CAN、GPS、加密服务的数据，按非道路国四（源码称“非四”）或国六协议组帧，维护备案、登录、实时数据、历史补传和登出流程。防篡改备案使用直连国家平台的 TCP socket；其余登录、数据和拆除报警帧封装为 `stub-linker` 设备事件，再交给 SDK。程序自身不在这里实现企业平台的 MQTT 会话；`mqtt_config_t` 和 `MQTT_COFIG_PATH` 的声明不能当成已运行的 MQTT 客户端。

源码内的分支允许若干发动机类型，但 CAN 解析只覆盖车型 0/1 的发动机 0、2，以及车型 2/3 的发动机 2。国六打包器还列出 TWC 类型的结构与分支；这不意味着当前车型都有对应的 CAN 数据。混动发动机类型 5 可以通过参数检查并启动国六线程，但国六流数据打包函数没有该类型的主体分支。参见 `t4hj_main.c:35`、`src/can_parse/can_parse.c:4`、`src/gb6/hj1239_1_2021_frame_pack.c:363`。

## 二、源码目录总览

### 2.1 源码树

下图按当前源码检出目录整理。`src/json/` 虽有源码，当前 `CMakeLists.txt` 未将其加入编译源列表；`lib/` 中存在预编译库，不能用它代替自研代码的行为证据。

```text
stub-gb-client/
├── README.md
├── CMakeLists.txt
├── stub.ini
├── stub-gb-client.c
├── stub-gb-client.h
├── t4hj_main.c
├── t4hj_main.h
├── startup-service-install.sh
├── src/
│   ├── app_mng/          全局状态、VIN、队列初始化
│   ├── local_skt/        本机 socket 接收与 CAN 活跃时间
│   ├── skt_res/          TCP 建连辅助
│   ├── can_parse/        车型 CAN/PGN 解析与 BAM 组包
│   ├── fei_4/            非四状态机和帧打包
│   ├── gb6/              国六状态机和帧打包
│   ├── data_signature/   加密请求、答复和签名队列
│   ├── nanomsg/          SDK 设备事件封装
│   ├── db/               SQLite 历史记录
│   ├── iniparser/        INI 读写
│   ├── common/           时间、字节和格式转换
│   ├── list/             链表实现
│   ├── log/              日志辅助
│   └── json/             仓库内 cJSON 源码（当前目标未纳入）
└── lib/                  仓库内的预编译库文件
```

### 2.2 文件职责速查

| 代码位置 | 实际职责 |
| --- | --- |
| `stub-gb-client.c/.h` | `main()`、stub 发布订阅、心跳/版本/网络消息、缓存队列、主循环和日志级别。版本宏为 `1.7.0.0`。 |
| `t4hj_main.c`、`src/app_mng/` | 参数检查、全局状态、VIN、CAN 默认无效值、队列、SQLite 与线程初始化。 |
| `src/local_skt/`、`src/skt_res/`、`src/local_skt/can_active.c` | 五个本地 TCP 数据连接、接收分发、CAN 活跃时间。 |
| `src/can_parse/` | 车型选择、CAN ID/PGN 解析、J1939 BAM 多包重组、主动请求报文。 |
| `src/fei_4/`、`src/gb6/` | 非四/国六状态机、登录、备案、流数据、OBD/ECD、补传和帧打包。 |
| `src/data_signature/` | 芯片 ID 设置、待签名队列、加密服务请求/应答和签名结果分发。 |
| `src/nanomsg/` | 在线条件判断，将协议帧转为设备事件 JSON 并排入 stub 消息队列。 |
| `src/db/`、`src/iniparser/`、`src/common/`、`src/list/`、`src/log/` | SQLite BLOB 存储、INI、时间/字节转换、链表和辅助日志。 |
| `src/json/` | 仓库内有 cJSON 源文件；本目录 `CMakeLists.txt` 没有把该目录加入 `aux_source_directory`，而是查找并链接外部 `cjson-git`/`cjson`。 |

## 三、核心架构设计

### 3.1 系统边界与数据通路

`stub-gb` 是一个 SDK 适配器进程。CAN、GPS 和加密服务经本机 TCP 向它提供数据；常规协议报文由 `stub-linker` 设备事件交给 SDK，防篡改备案则使用单独的国家平台 TCP 连接。企业平台的后续传输不在本仓库内。参见 `stub-gb-client.c:592`、`src/local_skt/local_socket.c:114`、`src/data_signature/data_signature.c:200`、`src/nanomsg/nanomsg_proc.c:70`。

```text
进程外 CAN0/1/2、GPS TCP 服务
    ↓
本进程 local_skt → can_parse / GPS 数据更新 → app_mng → fei_4 或 gb6 状态机

状态机的三条输出：
  备案帧 0x07
    → data_signature ↔ 本机加密服务 :16009
    → 国家平台 TCP → 备案答复校验

  实时链表或历史 SQLite 数据
    → 在线且本地已登录时由历史优先的调度取出
    → data_signature ↔ 本机加密服务 :16009
    → 协议发送链表

  登录、登出、拆除报警帧
    → 协议发送链表

协议发送链表 → nanomsg 设备事件 JSON → stub 消息缓存 deque_msgs
    → 进程外 stub-linker / SDK → 企业平台后续链路（本仓库外）
```

已备案后，离线或未登录时生成的数据先走历史存储，不会立即进入签名调度。图中依次涉及实时链表或 SQLite 历史记录、签名链表、协议发送链表和 stub 消息缓存；每次出队与最终平台回执是不同事件。登录、登出、拆除报警帧不经过数据签名队列；备案签名结果不进入 SDK 设备事件通道。见 `src/fei_4/fei4.c:626`、`src/gb6/gb6.c:719`、`src/data_signature/data_signature.c:200`。

### 3.2 启动与并行任务

下列纯文本图可在普通 Markdown 查看器中阅读。

```text
启动与并行任务（箭头表示调用或数据交接）
main()
  ├─ 初始化日志、stub SUB/PUB ──失败──> 退出
  └─ t4hj_main(车型, 发动机, GPS 时间开关)
       ├─ 参数非法 ──> Usage / 退出
       └─ 初始化 VIN、队列、SQLite、芯片 ID
            ├─ 本机 socket 线程：连接 CAN0/1/2、GPS、加密服务 5 个端口
            ├─ 签名线程：写加密请求，等待本机加密答复
            └─ 非四或国六协议线程：备案/登录/采集/补传/登出
main() 随后启动 pdk_thread_handler()，主线程循环 stub_run() 和定时任务。
```

报文的分支和缓存生命周期见 3.1 架构图及第五节。

### 3.3 线程、共享状态与连接

| 执行位置 | 入口和周期 | 主要工作 |
| --- | --- | --- |
| 主线程 | `main()`、`stub_run()`、`run_pub_tasks()`；循环休眠 50 毫秒 | 处理 stub 消息、心跳和网络状态；定时调用 `nanomsg_client_task()`。 |
| stub 缓存线程 | `pdk_thread_handler()` | SCP 在线时从 `deque_msgs` 取消息并调用 `stub_msg_reply()`。 |
| 本机 socket 线程 | `start_local_socket()` | 连接五个本机端口，进入 `select()` 接收循环。 |
| 签名线程 | `data_sign_task()` | 取签名队首项，向 16009 写请求并等待答复状态。 |
| 协议线程（二选一） | `fei4_task()` 或 `gb6_task()`；约 100 毫秒一轮 | 运行状态机、签名调度和 `can_pgn_request()`。 |

这些线程共享 `app_mng` 与多个链表；队列访问使用各自的互斥锁。`nanomsg_client_task()` 是主线程定时任务，源码中单独创建它的线程语句已注释。`t4hj_main()` 没有逐一检查 `pthread_create()` 返回值。见 `t4hj_main.c:82`、`stub-gb-client.c:741`、`stub-gb-client.c:931`。

| 接口 | 创建方与用途 | 源码位置 |
| --- | --- | --- |
| `127.0.0.1:11216` / `:11215` | stub SUB / PUB，`stub_sc_new()` 连接而非监听；主线程 `stub_run()` 收心跳、版本、网络状态。 | `stub-gb-client.c:592`、`:931` |
| `127.0.0.1:16002`、`:16003`、`:16004` | CAN0/1/2 的本机 TCP 服务；`recv_msg_process()` 按 `can_frame_t` 大小切分一次 `read()` 的内容。 | `src/app_mng/app_mng.h:26`、`src/local_skt/local_socket.c:19` |
| `127.0.0.1:16005` | GPS 本机 TCP 服务；仅当一次 `read()` 长度恰好为 `sizeof(gps_info_t)` 时作为 GPS 消息处理。 | `src/local_skt/local_socket.c:63` |
| `127.0.0.1:16009` | 加密服务本机 TCP 连接；签名请求写入，同一连接的答复在 socket 线程读取。头文件注释称服务端在 `ciu98_mng` 进程。 | `src/app_mng/app_mng.h:30`、`src/data_signature/data_signature.c:505` |
| `fdlpfjk.vecc.org.cn:55001` / `g6check.vecc.org.cn:19006` | 非四/国六防篡改备案直连地址。`FEI4_DATA_PORT=53001` 仅定义，当前直连函数没有使用它。 | `src/fei_4/fei4.h:4`、`src/gb6/gb6.h:4` |

主线程每轮先 `stub_run()`，再按定时表调度任务并休眠 50 毫秒。版本请求、版本广播、心跳、设备参数请求的间隔是 60 秒，网络状态请求是 10 秒，`nanomsg_client_task` 的超时为 50 毫秒；实际触发还受主循环耗时影响。SCP 心跳应答刷新本地在线时间，超过 10 秒未刷新会标记离线；`remote_online()` 同时要求 SCP 在线且默认网络端点状态是 `connected`。普通消息缓存的出队线程 `pdk_thread_handler()` 只检查 SCP 在线。见 `stub-gb-client.c:741`、`:858`、`:958`、`src/nanomsg/nanomsg_proc.c:17`。

## 四、核心模块深度分析

本节按代码责任说明每个模块的入口、输入、输出和关键条件；跨模块的状态转移与缓存生命周期在第五节连起来看。

### 4.1 入口和全局状态：`stub-gb-client.c`、`t4hj_main.c`、`app_mng/`

`main()` 先初始化日志与 `stub-linker` SUB/PUB，再调用 `t4hj_main()`。后者检查三个启动参数，初始化 `app_mng`、VIN、队列、CAN 默认值与数据库，向签名队列放入芯片 ID 设置命令，然后启动本机 socket、签名和对应协议线程。初始化失败会阻止主循环开始；线程创建结果没有逐项检查。见 `stub-gb-client.c:931`、`t4hj_main.c:35`、`t4hj_main.c:82`。

`app_mng_t` 保存车型/发动机、VIN、CAN 与 GPS 数据、SQLite 句柄和行数、各协议的实时/历史/发送链表、签名链表以及本机和国家平台 socket。`canapp_mng_init()` 按发动机类型选非四或国六数据库、表名、默认芯片 ID 和 VIN 来源；国六 INI 未给 VIN 时会循环读取 `/tmp/vin.ini`。这使启动参数不仅决定线程，也决定持久化文件和密钥标识的默认值。见 `src/app_mng/app_mng.c:179`、`:289`、`:312`。

`on_stub_msg_received()` 当前处理心跳、协议版本和网络状态；命令/OTA 分支已注释。`request_dev_info()` 会发设备参数请求，但本文件没有相应的业务答复处理。启动参数的接受范围与真正有 CAN 解析的组合需分别核对，详见第六节。见 `stub-gb-client.c:475`、`:785`、`src/can_parse/can_parse.c:4`。

### 4.2 本机连接与接收：`local_skt/`、`skt_res/`

`start_local_socket()` 依次连接 CAN0/1/2、GPS、加密服务五个本机 TCP 端口；五个连接都建立后才进入 `recv_msg_process()` 的 `select()` 循环。CAN 消息按 `can_frame_t` 整数倍切分并交给 `can_frame_parse()`；GPS 仅在一次 `read()` 长度恰为 `sizeof(gps_info_t)` 时处理；加密答复送 `data_signatured_parse()`。这些是当前代码对 TCP 字节流的处理条件，不表示 TCP 自身提供消息边界。见 `src/local_skt/local_socket.c:19`、`:63`、`:102`、`:114`。

断连会使接收循环返回，外层重新连接。本机连接函数在 `connect()` 失败时未关闭新建 fd；`select()` 的错误条件也写成了 `-1 > ret`。两处具体影响见第七节。见 `src/skt_res/skt_res.c:49`、`src/local_skt/local_socket.c:48`。

### 4.3 车型解析与定位：`can_parse/`

`can_frame_parse()` 先按车型，再按发动机类型选择解析函数。SAC 两类车型仅为发动机 0/2 进入解析；STC 两类车型仅为发动机 2 进入解析。`can_pgn_request()` 使用相同的分支选择主动 PGN 请求函数。解析文件按 CAN 通道、CAN ID 和 PGN 更新 `app_mng` 的共享字段；`bam_proc.c` 处理 J1939 BAM 多包拼接。见 `src/can_parse/can_parse.c:4`、`:45`、`src/can_parse/bam_proc.c`。

`can_data_init()` 将速度、转速、排放、定位等字段先设置为源码约定的无效值或初值，例如速度 `0xFFFF`、定位经纬度 `0xFFFFFFFF`。各车型解析文件按规定 CAN 通道与 CAN ID 更新字段；SAC 非四解析要求 CAN1，SAC 国六和 STC 国六要求 CAN0。`can_update_canframe_timemap()` 在匹配到相关帧或 PGN 时更新活跃时间。故“本地 TCP 收到任意字节”与“协议状态机认为 CAN 活跃”是不同条件。见 `src/app_mng/app_mng.c:25`、`src/can_parse/can_parse.c:4`、`src/local_skt/can_active.c:1`。

车型解析同时包含主动请求：`can_pgn_request()` 依车型/发动机调用各文件的 `request_*_pgn()`；收到 J1939 BAM 控制帧 `0xECFF` 和数据帧 `0xEBFF` 时，`bam_proc.c` 按序号拼接多包，再由车型文件解释故障码等内容。BAM 组包状态是该文件中的静态对象。字段的 CAN ID、字节序、缩放与无效值散落在四个车型文件；新增车型应逐一核对通道、请求 PGN 和打包器的字段来源，不能只添加参数枚举。

GPS 接收后换算经纬度并更新定位状态。`enable_gps_time != 0` 且车型 2/3、发动机 2 时，GPS 消息另被复制进队列；国六流数据函数从队列取采集时间。关闭该模式时，按 10 秒常量使用当前设备时间和已保存的经纬度生成流数据。实时签名阶段，GPS 时间模式要求凑够 10 个国六实时数据项，普通模式要求 1 个。见 `src/local_skt/local_socket.c:63`、`src/gb6/gb6.c:240`、`:825`。

### 4.4 非四协议：`fei_4/`

`fei4_task()` 每轮依次执行 `fei4_pack_proc()`、`fei4_sign_proc()` 和 `can_pgn_request()`。打包状态机先检查 CAN 活跃与 VIN；未备案时尝试 0x07 防篡改备案，备案答复成功后才保存 `registerStat`；已备案且 `remote_online()` 为真时，按本地登录状态生成登录或数据。离线、未登录时生成的数据进入历史路径，CAN 失活时将实时队列转历史并尝试拆除报警、登出。见 `src/fei_4/fei4.c:626`、`:797`、`:832`。

`fei4.h` 的触发间隔为防篡改 60 秒、登录/登出 10 秒、实时流 60 秒、ECD 600 秒、拆除报警 60 秒、无 CAN 数据超时 15 秒。流数据与 ECD 数据由 `fei4_frame_pack_real_flow_data()`、`fei4_frame_pack_real_ecd_data()` 生成；离线/未登录时写历史 SQLite，在线且登录后进入实时链表。帧结构以 `##` 开头，包含命令、17 字节 VIN、版本、加密标识、长度、数据体和 BCC。登录帧内的 ICCID 当前由固定字面值填写，读取真实 ICCID 的代码在 `#if 0` 中。见 `src/fei_4/fei4.h:4`、`src/fei_4/fei4_frame_pack.c:20`、`:102`。

`fei4_sign_proc()` 在在线且本地登录时优先处理 SQLite 历史行，历史行数为零才处理实时队列；历史每轮最多读取一条。`fei4_send_backup_to_sign()` 在签名结果和平台回执之前删除读出的行。`fei4_send_out()` 正常入队分支没有返回值，但登录/登出函数依赖它判断成功，因此本地状态推进存在未定义行为。见 `src/fei_4/fei4.c:39`、`:762`、`:797`。

### 4.5 国六协议：`gb6/`

`gb6_task()` 的循环形态与非四相同，但数据项增加流数据、补充数据和 OBD。`gb6_pack_proc()` 使用 CAN 活跃、VIN、备案、`remote_online()` 与本地登录状态选分支；状态满足时写实时链表，否则写历史路径。登录状态是本地入队后的状态，源码没有在此等待企业平台业务应答。见 `src/gb6/gb6.c:719`、`:1034`、`:1070`。

`gb6.h` 的触发间隔为防篡改 60 秒、登录/登出 10 秒、流数据 10 秒、补充数据 60 秒、OBD 600 秒、拆除报警 60 秒、无 CAN 数据超时 15 秒。`hj1239_1_2021_frame_pack.c` 对发动机 2/3/4 有不同的流数据体，OBD 和补充数据为另外的数据项。国六完整帧同样以 `##` 开头并带 BCC；源码头文件还保留了 `0x7e` 起始符的注释，但当前宏与打包器实际使用 `0x23`。登录 ICCID 同样是固定字面值。见 `src/gb6/gb6.h:4`、`src/gb6/hj1239_1_2021_frame_pack.c:18`、`:122`、`:363`。

启用 GPS 时间模式时，`gb6_send_realtm_to_sign()` 要求实时链表至少有 10 项才组包；普通模式要求 1 项。历史每轮最多取 10 条合包，但 `gb6_send_backup_to_sign()` 在合包、入签名队列前就删库。流数据打包器处理发动机 2/3/4，没有混动 5 的主体分支；具体能否从 CAN 得到字段还要看第六节矩阵。见 `src/gb6/gb6.c:825`、`:936`、`src/gb6/hj1239_1_2021_frame_pack.c:363`。

### 4.6 签名服务：`data_signature/`

`encrypt_chip_id_set()`、`fei4_data_sign_add()` 和 `gb6_data_sign_add()` 把不同请求放入同一签名链表。`data_sign_task()` 一次处理队首，经 16009 写请求；本机 socket 线程读取答复后，`data_signatured_parse()` 校验格式、索引、状态和 BCC，再由 `data_signatured_handler()` 将 R/S 值拼入完整帧。超时常量为 3 秒、重发次数常量为 3；超限会移除队首。见 `src/data_signature/data_signature.c:24`、`:74`、`:299`、`:505`。

结果分流取决于命令和 `dataGroup`：备案 0x07 直写对应国家平台 socket，常规签名数据进入协议发送链表。重组函数当前对非四与国六都调用国六完整帧打包函数；这里只确认调用关系，平台兼容性仍需标准报文与抓包验证。答复解析函数未检查重组函数返回值就标记签名成功。见 `src/data_signature/data_signature.c:200`、`:299`。

### 4.7 SDK 事件与缓存：`nanomsg/`、`stub-gb-client.c`

`remote_online()` 同时要求 SCP 心跳在线与默认网络端点为 `connected`。满足条件且已有 VIN 时，主线程定时运行的 `nanomsg_client_task()` 从当前协议发送链表取一帧；`nanomsg_push_event_msg()` 将原始帧转换为大写十六进制字符串，放入设备事件 JSON 的 `param.data`，并设置 `param.encoding=raw`。这里的 `raw` 是字段值，不是 JSON 内的二进制数据。见 `src/nanomsg/nanomsg_proc.c:17`、`:30`、`:70`、`:107`。

`push_msg()` 将 JSON 放入 `deque_msgs`，上限 1024 条，满时先丢最旧项；独立的 `pdk_thread_handler()` 在 SCP 在线时从缓存取出并调用 `stub_msg_reply()`。`nanomsg_push_event_msg()` 忽略 `push_msg()` 的返回值并固定返回 0，调用方随后删除协议发送项。因而协议出队、stub 写入和平台接收必须分别检查。见 `stub-gb-client.c:220`、`:673`、`:741`、`src/nanomsg/nanomsg_proc.c:70`。

### 4.8 持久化与公共代码：`db/`、`iniparser/`、`common/`、`list/`、`log/`

`db_create_t4hj_table()` 建立自增 `id` 和 BLOB `data` 的历史表；`db_read_t4hj_history_rows()` 以 `SELECT * ... LIMIT N` 读取；`db_delete_t4hj_rows()` 则按 `id` 找界限删除。读取 SQL 未显式指定顺序，读取集合与删除集合在 SQL 语义上不保证对应。非四与国六分别使用 `FEI4`、`GB6` 表和独立数据库。见 `src/db/db.c:27`、`:172`、`:217`、`src/app_mng/app_mng.h:12`。

INI 存 VIN、注册/登录状态及流水号；公共代码提供字节、时间和十六进制转换，链表实现承载多级队列。`src/json/` 中有 cJSON 源码，但当前 CMake 未把它加入 `aux_source_directory`，构建查找外部 `cjson-git`。这些通用实现只在本应用的调用点上形成业务行为。见 `src/app_mng/app_mng.c:179`、`CMakeLists.txt:7`、`CMakeLists.txt:77`。

## 五、关键数据流与状态机

### 5.1 从启动到上报的执行顺序

1. `main()` 设置 `/tmp/stub-gb-client.log`、从 `/tmp/stub-gb-client.ini` 读日志级别，注册 `SIGUSR1`/`SIGPIPE`，创建 stub SUB/PUB 连接；失败则退出。见 `stub-gb-client.c:920`、`:931`、`:592`。
2. `t4hj_main()` 先清零全局 `app_mng`，再检查三个参数。若合法，初始化队列、VIN 和 CAN 默认值，打开并建表 SQLite；随后将芯片 ID 设置请求入签名队列，启动本地 socket、签名线程及一个协议线程。线程创建返回值没有在该函数中逐一检查。见 `t4hj_main.c:35`、`:82`。
3. 本地 socket 线程连接 CAN0/1/2、GPS、加密服务五个端口；五个都成功才进入 `select()` 接收循环。CAN 数据按车型交给解析器；GPS 更新位置，特定国六组合在 GPS 时间模式下入 GPS 队列；加密答复进入签名处理。见 `src/local_skt/local_socket.c:19`、`:114`。
4. 非四或国六线程从 INI 读取注册状态，循环运行打包状态机、历史/实时签名调度和 `can_pgn_request()`，循环末休眠 100 毫秒。CAN 活跃、VIN、备案状态、本地 `LOGIN` 标志和 `remote_online()` 决定每次走哪条支路。见 `src/fei_4/fei4.c:832`、`src/gb6/gb6.c:1070`。
5. 未备案时，协议线程尝试建立对应国家平台 TCP 连接，最多按 60 秒间隔构造防篡改/激活帧并送签名；签名完成后直接写该 socket，接收并校验备案答复。只有备案函数确认答复成功才保存 `registerStat=1`。连接失败时还存在下文所述的文件描述符判断问题。见 `src/fei_4/fei4.c:528`、`src/gb6/gb6.c:621`、`src/data_signature/data_signature.c:200`。
6. 备案后若 SDK 侧在线，状态机根据 INI 中的旧 `loginStat` 决定先登出还是登录。登录帧进入 SDK 发送队列；代码在本地入队函数返回成功时写 `LOGIN` 并推进状态，没有在本仓库实现企业平台登录业务应答确认。离线或未登录时生成的数据写 SQLite，在线且登录后进入实时队列。见 `src/fei_4/fei4.c:626`、`src/gb6/gb6.c:719`。
7. 在线登录后，签名调度优先处理 SQLite 历史行；当本地历史行数为零时处理实时队列。签名成功的常规报文进入协议发送队列；主线程的定时任务 `nanomsg_client_task()` 将其转成设备事件并放进 stub 消息缓存，独立的 `pdk_thread_handler()` 再调用 `stub_msg_reply()` 送到 stub PUB。CAN 失活时转存实时队列、尝试拆除报警与登出。见 `src/fei_4/fei4.c:797`、`src/gb6/gb6.c:1034`、`src/nanomsg/nanomsg_proc.c:107`、`stub-gb-client.c:673`。

### 5.2 备案、登录和失活状态

```text
CAN 不活跃 ──> 归档实时队列 ──> 如曾登录且 SDK 在线：拆除报警/登出
    │
    └─ CAN 恢复 + VIN 已得
             │
             ├─ 尚未备案：构造 0x07 -> 签名 -> 国家平台 TCP -> 校验答复
             │
             └─ 已备案：SDK 离线 -> 新数据存 SQLite
                         SDK 在线 + 本地未登录 -> 必要时先登出，再入队登录帧
                         SDK 在线 + 本地已登录 -> 生成实时数据
                                                历史优先签名/上报
```

这里的“登录”是本地状态机依据入队结果推进，代码没有等企业平台业务确认。备案是直连国家平台并检查答复，和 SDK 事件链路不同。两套协议都把注册、登录、实时数据流水号及日期保存在各自 INI 中；登录/实时流水号随日期变化重置，登录流水号打包前对 `65531` 取模。见 `src/fei_4/fei4.c:588`～`:729`、`src/gb6/gb6.c:681`～`:820`。

### 5.3 报文命令与两条出口

| 报文 | 当前路径 |
| --- | --- |
| 0x07 防篡改/激活 | 协议模块构造 -> 加密服务签名 -> 国家平台 TCP -> 校验备案答复。 |
| 0x01 登录、0x04 登出、0x05/0x06 拆除报警 | 生成完整协议帧 -> 对应 `platform_tx_list` -> SDK 设备事件通道。非四拆除报警为 0x05，国六为 0x06。 |
| 0x02 实时、0x03 历史 | 实时/历史数据项 -> 加密服务签名并重组完整帧 -> 对应 `platform_tx_list` -> SDK 设备事件通道。 |

国六/非四的头文件还声明其他命令或加密算法编号；不能仅凭枚举认定当前运行路径会发这些命令或使用这些算法。源码生成常规帧时常传 `ENCRYP_NO`，签名结果由本地加密服务追加处理；帧字段标志与签名是否完成必须分别理解。

### 5.4 缓存与历史补传

实时数据、历史记录、签名结果和 SDK 事件会经过不同的缓存层。下面列出各层的写入、移除和持久化条件；签名答复与 JSON 封装细节分别见 4.6、4.7。

| 缓冲层 | 写入条件与退出方式 | 持久性 |
| --- | --- | --- |
| 非四/国六实时链表 | 在线且本地登录时打包；国六 GPS 时间模式凑 10 项、普通模式 1 项后送签名。 | 内存 |
| SQLite 历史表 `FEI4`/`GB6` | 离线、未登录或 CAN 失活转存；恢复后历史优先，每轮非四最多读 1 条、国六最多读 10 条合包。 | `/media/sdcard/hj1014.db`、`hj1239.db` |
| 签名链表 | 等加密服务答复；成功后常规帧进入协议发送链表，备案帧直写国家平台。 | 内存 |
| 协议发送链表 | `nanomsg_client_task()` 取一项转 JSON，然后移除。 | 内存 |
| stub 消息缓存 `deque_msgs` | `push_msg()` 入队，SCP 在线时 `stub_msg_reply()` 出队；上限 1024，满时先丢最旧项。 | 内存 |

SQLite 表由 `db_create_t4hj_table()` 创建，字段为自增 `id` 与 BLOB `data`。历史流程在送签名附近就删除已读行，国六甚至在合包/入签名队列之前先删除行；并不等待签名、stub 传输或平台确认。源码最大行数非四 `11088`、国六 `61488`，达到上限会删除约十分之一旧行；这不是端到端可靠保存七天的保证。见 `src/db/db.c:27`、`src/fei_4/fei4.c:762`、`src/gb6/gb6.c:936`。

## 六、运行参数、配置与构建

### 6.1 启动参数与真实支持矩阵

命令行为 `stub-gb <vehicle_type> <engine_type> <enable_gps_time>`。`sscanf("%d")` 解析整数，要求 `argc == 4`；车型必须是 0～3，发动机必须是 0～5。GPS 时间参数只检查能解析为整数，后续按非零值判断。见 `t4hj_main.c:12`～`:79`。

| 车型参数 | 车型枚举 | 当前 CAN 解析入口 | 支持的发动机解析分支 |
| --- | --- | --- | --- |
| 0 | `SAC4000C8` | `sac4000c8.c` | 0 非四柴油、2 国六 DPF+SCR |
| 1 | `SAC2000C8` | `sac2000c8.c` | 0 非四柴油、2 国六 DPF+SCR |
| 2 | `STC250E5` | `stc250e5.c` | 2 国六 DPF+SCR |
| 3 | `STC250_550C5`，注释列出 STC250E5/C5、STC500C5、STC550C5 | `stc250_550c5.c` | 2 国六 DPF+SCR |

发动机 1（非四非柴油）、3/4（国六 TWC）和 5（国六混动）通过参数检查，线程会启动，但 `can_parse.c` 对这四类没有任何车型组合的分支。国六 `gb6_hj1239_make_flow_data_by_gps()` 只为发动机 2/3/4 选主体结构，未处理 5；因此发动机 5 的流数据路径不能视为完整支持。参数 2/3 的 STC 国六 DPF+SCR 在 GPS 时间模式下会把收到的 GPS 消息放入队列；其他组合启用该模式时，国六流数据函数可能因队列为空而持续等待。即使选择 0/2 这类有解析分支的组合，仍需目标设备提供相应 CAN ID、PGN 与端口数据。

### 6.2 配置文件和状态

| 配置/文件 | 作用与注意点 |
| --- | --- |
| `/opt/fei4_conf.ini` | `fei4:vin`、`registerStat`、`loginStat`、登录/实时/拆除报警流水号与日期。 |
| `/opt/gb6_conf.ini` | `gb6:vin`、同类状态与流水号。没有配置 VIN 时，`canapp_mng_init()` 每 5 秒尝试读取 `/tmp/vin.ini`，成功后回写国六 INI；缺文件会使初始化一直等待。 |
| 默认芯片 ID 与公钥 | `src/app_mng/app_mng.c` 内置非四、国六两种 16 字节芯片 ID 和固定公钥文本，并在初始化时复制到全局状态。它们是当前源码默认值，不能当作现场设备芯片身份或生产密钥已核验。 |
| `/media/sdcard/hj1014.db`、`hj1239.db` | SQLite 历史 BLOB 数据库，运行前目录需存在且可写。 |
| `/tmp/stub-gb-client.ini`、`/tmp/stub-gb-client.log` | 日志级别与 2 MiB 上限的日志文件。`SIGUSR1` 循环调整级别；`SIGPIPE` 被处理。 |
| `stub.ini` | SDK 插件启动示例参数 `2 2 0`；实际进程参数仍需核对设备启动记录。 |

### 6.3 SDK 构建与部署边界

`CMakeLists.txt` 指定 C99 和 Debug 构建，查找 `e2fsprogs-git`、`cjson-git`、`nanomsg-git`、`libcbor-git`、`collections-c-git`、`zlib-git`、`stub-linker-git`；还链接 `rcpv4-utils_static`、`erk-log`、`erk-utils`、SQLite、pthread 等。头文件和库路径依赖 SDK 上层目录与 `QL_SYSROOT_DIR`。安装路径是安装前缀下的 `tools/stub-gb/`，包含程序和 `stub.ini`。`README.md` 给出把此项目加入 `rootcloud-stp-sdk/erk-plugins/plugins/scp-adapter/tools/`、使用 EC200A 工具链执行 `cmake`、`make`、`make package` 的步骤。当前独立源码树本身不证明这些依赖齐全，也未在本次分析中完成目标机编译。

`stub.ini` 为 `[stub] enabled=true`、`name=stub-gb`、`param=2 2 0`。`startup-service-install.sh` 管理 `erk-daemon` 在设备端 `hqinit.hrc` 的自启动配置，默认指向 `/media/card/erk-root/erk-daemon.sh`；它不是直接启动本程序的脚本。

## 七、源码可确认的限制与风险

| 证据位置 | 源码事实 | 实际含义 |
| --- | --- | --- |
| `src/fei_4/fei4.c:39` | `fei4_send_out()` 声明返回 `int`，正常入队路径没有 `return`；登录/登出函数却依赖返回值。 | 非四本地登录/登出状态判断存在未定义返回值行为。 |
| `src/fei_4/fei4.c:535`、`src/gb6/gb6.c:628` | 两套备案函数在连接返回后使用 `if (fd)` 判断，`-1` 也满足此条件。 | TCP 连接失败后仍可能排入备案签名请求，并对无效描述符调用 `recv()`；不能把进入该分支解读成连接成功。 |
| `t4hj_main.c:10`、`src/nanomsg/nanomsg_proc.c:12` | `app_mng` 在两个 C 文件各有一处全局暂定定义。 | 采用 `-fno-common` 语义的工具链可能在链接时报重复定义；当前目标工具链是否因此失败尚未经完整构建验证。 |
| `src/data_signature/data_signature.c:299` | 签名答复校验成功后，调用 `data_signatured_handler()` 却不检查其返回值，直接标记 `DATA_SIGN_STAT_OK`。 | 后续组帧或队列入队失败可能被当作签名处理完成。 |
| `src/gb6/hj1239_1_2021_frame_pack.c:363` | 国六流数据只处理发动机 2/3/4，没有混动 5 的主体分支。 | 参数 5 可启动线程，但不能据此认定流数据打包完整。 |
| `src/nanomsg/nanomsg_proc.c:70`、`stub-gb-client.c:220` | JSON 发送函数忽略 `push_msg()` 返回值；缓存满 1024 条时淘汰最旧项。 | 协议发送链表出队与平台确认之间有明显缺口。 |
| `src/fei_4/fei4.c:762`、`src/gb6/gb6.c:936` | 历史数据在签名和平台确认之前删库。 | 断电、签名失败或后续发送失败时，历史记录可能无法再补传。 |
| `src/local_skt/local_socket.c:19` | 五个本地端口都接通才进入接收循环；GPS 仅处理一次 `read()` 恰好一个结构体的情况，CAN 按整帧数切分一次 `read()`。 | 本机 TCP 断连或流式拆包/粘包需要实机验证，不能假定每次读都是一条完整业务消息。 |
| `src/local_skt/local_socket.c:48` | `select()` 错误判断写成 `if (-1 > ret)`，对常见的 `ret == -1` 不成立。 | 该分支不能按作者意图处理 `select()` 返回 `-1` 的错误，后续行为应在设备上核查。 |
| `src/skt_res/skt_res.c:49` | `skt_res_new_socket()` 在 `connect()` 失败时直接返回 `-1`，没有关闭刚创建的 socket；本机 socket 线程会继续重试。 | 本机服务长期不可用时，进程可能逐次泄漏文件描述符；现场可结合进程 fd 数量和连接失败日志核查。 |
| `src/db/db.c:178`、`:217` | 读取历史行的 SQL 为 `SELECT * ... LIMIT N`，没有 `ORDER BY`；删除函数按 `id` 排序找到第 N 行，再删除 `id <=` 的记录。 | 读出的 N 行与删掉的 N 行在 SQL 语义上没有显式保证完全对应；维护时应核查实际数据库访问顺序。 |
| `src/fei_4/fei4.c:265`、`src/gb6/gb6.c:313` | 离线写入 SQLite 后，调用方不检查 `db_insert_t4hj_data_v2()` 的结果就增加内存中的 `db_rows`。 | 内存计数可能暂时与数据库实际行数不符，不能仅凭该计数判断数据已持久化。 |
| `src/nanomsg/nanomsg_proc.c:92` | VIN 云端请求函数只有 `TODO` 日志。 | 国六缺 VIN 时不能依靠该路径从云端自动补齐。 |
| `src/fei_4/fei4_frame_pack.c:102`、`src/gb6/hj1239_1_2021_frame_pack.c:18` | 两种登录帧都写入固定 ICCID 字面值，设备读取分支被 `#if 0` 禁用。 | 报文里的 ICCID 不能当作当前 SIM 的实测值。 |

这张表列的是代码行为和可从代码直接推出的影响，并非已在目标设备复现的故障结论。对具体平台协议符合性、CAN 缩放正确性和设备部署状态，需要结合标准文本、实际 CAN 样本、目标固件及平台回执另行验证。

## 八、设备故障排查

### 8.1 先定位停在哪一层

本节是依据此提交源码写的现场排查路径。先保留故障发生时的启动命令、日志时间、目标机系统时间和平台回执，再按下面的顺序找第一个异常检查点。只读检查优先；不要先清数据库、改 `registerStat`/`loginStat`、重置配置或反复重启，否则会丢失现场证据。设备的服务名称、日志采集方式和命令是否可用以实际固件为准。

```text
进程不存在或退出
  └─ 启动参数 -> stub SUB/PUB 初始化 -> VIN/数据库初始化
进程在运行，但无采集数据
  └─ 5 个本机端口 -> CAN 帧边界/通道/车型解析 -> CAN 活跃 -> GPS 队列
已有采集，不能备案
  └─ VIN -> 国家平台 TCP -> 加密服务签名 -> 0x07 答复校验 -> registerStat
已备案，不能登录或发送
  └─ SCP 心跳 -> 默认网络端点 connected -> 本地登录状态 -> 协议发送队列
报文显示已发送，平台无数据
  └─ push_msg() -> stub_msg_reply() -> SDK 后续链路 -> 平台回执
数据不连续或字段不对
  └─ SQLite 历史行 -> 签名/删库顺序 -> 组帧字段 -> CAN/GPS 原始值
```

### 8.2 固定现场证据与只读命令

记录设备型号、固件版本、进程实际命令行、车型/发动机/GPS 时间参数、故障起止时间、国家/企业平台地址与回执。采集 `/tmp/stub-gb-client.log`、进程 stdout/stderr、`/opt/fei4_conf.ini` 或 `/opt/gb6_conf.ini`、`/tmp/vin.ini`、`stub.ini` 与数据库文件的**副本**。配置、日志与报文可能包含 VIN、芯片标识、定位等敏感数据，转交时按现场要求处理。下列命令仅用于示例；缺少 `ps`、`ss`、`sqlite3` 时换用目标系统已有工具，不要因工具缺失误判为本程序故障。

```sh
ps -ef | grep '[s]tub-gb'
ls -l /tmp/stub-gb-client.log /tmp/vin.ini /opt/fei4_conf.ini /opt/gb6_conf.ini
ss -tnp
# 先确认实际使用的是哪一种协议、哪个数据库；在拷出的数据库副本上执行：
sqlite3 -readonly /path/to/hj1014.db 'SELECT count(*), min(id), max(id) FROM FEI4;'
sqlite3 -readonly /path/to/hj1239.db 'SELECT count(*), min(id), max(id) FROM GB6;'
```

`ss -tnp` 重点看 `127.0.0.1:11216/11215`、`16002/16003/16004/16005/16009` 和对应国家平台地址。已连接只能说明 TCP 连接存在，不能证明服务发送了完整结构体或平台确认了报文。`sqlite3 -readonly` 依赖目标机 SQLite CLI 支持该选项；若不支持，在数据库副本上用可用工具查询。数据库源文件位于 `/media/sdcard/hj1014.db` 或 `/media/sdcard/hj1239.db`；复制运行中的 SQLite 数据库时应使用一致性快照方式，避免把不完整副本当成数据损坏。

### 8.3 按症状核查

| 现场表现 | 先核对的证据与代码机制 | 如何判断下一步 |
| --- | --- | --- |
| 启动即打印 `Usage` | 对照 `stub.ini` 和进程实际参数。程序要求三个可解析整数：车型 0～3、发动机 0～5、GPS 时间开关整数；接受参数不等于有对应 CAN 解析。见 `t4hj_main.c:12`。 | 参数不合法属于部署配置；参数合法但该组合无 `can_parse.c` 分支，属于源码支持范围问题。 |
| 启动失败或卡在初始化 | 查 `stub_sc_new()` 是否建立 11216/11215 连接；查 `canapp_mng_init failed!`、`db_open_t4hj_db failed`、`db_create_t4hj_table failed`，及 `/tmp/vin.ini` 和 `/media/sdcard/`。国六 VIN 缺失时初始化每 5 秒等待；数据库路径不可写会退出。见 `stub-gb-client.c:592`、`t4hj_main.c:82`、`src/app_mng/app_mng.c`。 | 先确认服务、文件和挂载。依赖正常仍卡住，再查程序读配置与异常处理。 |
| 进程在运行，但本机 CAN/GPS/加密数据均不进来 | 本机 socket 线程须同时连上五个端口才接收，失败会报 `local socket[%d] create failed!!` 并重连；接收失败报 `local skt recv error, idx=%d`。端口序号 0/1/2 为 CAN，3 为 GPS，4 为加密。`connect()` 失败路径未关闭新建 fd。见 `src/local_skt/local_socket.c:113`、`src/skt_res/skt_res.c:49`。 | 某服务未监听/断开是首要依赖故障；反复失败时同时查看进程 fd 数量。五个端口都有数据但程序未处理，再对比每次 `read()` 的长度。 |
| TCP 有字节，CAN 字段仍不更新 | CAN 一次 `read()` 按 `sizeof(can_frame_t)` 整数倍切分，尾部不足一帧被忽略；没有跨次读取缓存。核对源端帧长度、CAN 通道、CAN ID/PGN、车型/发动机分支和 `can_update_canframe_timemap()`。见 `src/local_skt/local_socket.c:102`、`src/can_parse/can_parse.c`。 | 若帧被拆包/粘包导致遗漏或解析条件与设备实际帧不符，可归到本程序接收/解析实现；原始 CAN 不符合约定时回查采集服务或设备。 |
| 报 `no can data after ...`，出现拆除报警/登出 | 对照 CAN 原始帧与活跃时间更新条件；收到无关 CAN 帧不代表状态机认为活跃。超时常量为 15 秒。见 `src/local_skt/can_active.c`、`src/fei_4/fei4.c:518`、`src/gb6/gb6.c:611`。 | CAN 持续正常且匹配解析器，但活跃时间没更新，重点查程序的过滤和状态条件。 |
| 国六 GPS 时间模式报 `NO GPS Message!!!!!` 或流数据停滞 | GPS 只在一次 `read()` 恰好 `sizeof(gps_info_t)` 时处理；仅车型 2/3 且发动机 2 把 GPS 消息放进 GPS 时间队列。检查第三参数、GPS 源是否持续送完整结构体、设备时钟。见 `src/local_skt/local_socket.c:66`、`src/gb6/gb6.c:240`。 | 其他组合启用 GPS 时间模式可能长期等队列，属于源码路径限制；支持的组合仍缺完整 GPS 消息则先查源端/接收边界。 |
| 无法完成防篡改备案 | 查 VIN、对应 `/opt/*_conf.ini` 的 `registerStat`，国家平台 DNS/TCP、16009 加密服务和 `prevent change data ...`、`... recv failed` 日志。备案 0x07 要签名并直连国家平台，成功必须校验答复。见 `src/fei_4/fei4.c:528`、`src/gb6/gb6.c:621`。 | TCP 失败时源码用 `if(fd)`，`-1` 也会进入分支；不能以进入签名分支认定平台连通。区分网络失败、签名失败和答复校验失败。 |
| 已备案但不登录、不上报 | 查 SCP 心跳和默认网络端点状态。`remote_online()` 要求 `is_online` 与 `network_connected` 同时为真；旧 `loginStat=LOGIN` 还会先触发登出流程。见 `src/nanomsg/nanomsg_proc.c:17`、`stub-gb-client.c:356`、`:449`、`src/gb6/gb6.c:719`。 | 本地 `GB6/FEI4 platform login` 日志或 `LOGIN` 配置只代表本地状态推进，不等于平台确认登录。核对 SDK 和平台回执。 |
| 加密请求失败、排队或丢包 | 查 16009 连接、`scoket send data to encrypt failed`、`data encrypt failed sleep 10s before resend data.`、加密答复格式和索引。签名线程等待超时 3 秒，超过重发阈值会移除队首。见 `src/data_signature/data_signature.c:337`、`:505`。 | 加密服务无答复先查服务；答复有效却组帧/入队失败，要查 `data_signatured_handler()` 返回值未检查的问题。 |
| 日志有 `t4hj_data [common] sent!`，平台却没有 | 该日志只说明 `nanomsg_push_event_msg()` 返回 0 并移除协议发送项；函数忽略 `push_msg()` 的真实返回值。再看 `push t4hj event msg`、`stub_msg_reply ret = ...`、SDK 与平台回执。见 `src/nanomsg/nanomsg_proc.c:70`、`:147`、`stub-gb-client.c:673`。 | `stub_msg_reply()` 返回成功也只到本地 SDK 接口；以平台记录核实最终接收。1024 条缓存满会淘汰旧消息，出队后即使回复失败也释放。 |
| 历史数据减少但平台缺报，或断电后缺报 | 查询数据库副本的行数/id 与故障时间。历史行在签名和平台确认前就被删除，国六在合包/签名入队前亦可能删行；写库返回值未被上层检查。见 `src/db/db.c`、`src/fei_4/fei4.c:762`、`src/gb6/gb6.c:936`。 | 行数下降不代表成功补传；若缺口位于删库至平台接收之间，属于程序可靠性风险，需与平台日志和重启时间关联。 |
| 平台收到帧，但 VIN、时间、定位、ICCID 或业务字段异常 | 保存原始 CAN/GPS、配置 VIN、原始十六进制帧和平台解码结果，逐层核对车型解析、单位/无效值、组帧、签名。两种登录帧的 ICCID 是固定文本；`param.encoding=raw` 时 `param.data` 实际为十六进制字符串。见 `src/can_parse/`、`src/fei_4/fei4_frame_pack.c`、`src/gb6/hj1239_1_2021_frame_pack.c`、`src/nanomsg/nanomsg_proc.c:30`。 | 输入数据正确、程序打包错误才可归因到本程序；平台解码与标准差异须用标准原文和抓包共同验证。 |

默认日志级别由 `/tmp/stub-gb-client.ini` 控制，启动代码默认使用 `WARNING`；一些 `INFO`/`NOTICE` 级日志在现场可能看不到。`SIGUSR1` 可切换级别，但应先核对运行版本与现场操作规程，并保存切换前日志；缺少某条低级别日志不能直接判定代码没执行。见 `stub-gb-client.c:117`、`:920`。

### 8.4 如何归因与记录结论

按“输入 → 程序检查点 → 输出 → 下游回执”成对留证。可归因到本程序的典型证据是：本机服务提供了满足约定的完整 CAN/GPS/加密消息而程序漏解析；合法参数组合缺解析/打包分支；程序在 `push_msg()` 失败后仍丢协议队列项；历史行在未获确认前被删除。若仅有国家平台不可达、SDK 服务未运行、加密服务无答复、原始 CAN 缺失或平台无回执，则先记录为对应链路故障，不能只凭本地 `LOGIN`/`sent` 把责任归给某一端。

建议每个故障记录：设备与固件、进程命令行、源码/二进制版本、精确时间段、当时的 CAN/GPS 样本、五个本机端口与远端连接状态、关键 INI 值、SQLite 行数/id 变化、签名结果、JSON 事件及 `stub_msg_reply` 返回值、平台回执、第一处不一致的检查点。若设备二进制并非本文提交构建，先核对其版本和源码差异，再应用本节结论。

## 九、术语与核验范围

### 9.1 术语

| 术语 | 在本项目中的具体含义 |
| --- | --- |
| 备案/防篡改注册 | 生成命令 0x07，经本机加密服务签名后直连国家平台，校验答复后才把本地 `registerStat` 置为 1。 |
| 登录 | 备案后的命令 0x01，经 SDK 事件链路发送；`loginStat=LOGIN` 和内存 `gb_login=1` 是本地状态，源码未等待企业平台业务应答。 |
| SCP / stub SDK | 本程序通过 11216/11215 的本地 SUB/PUB 与 SDK 通信；心跳和默认网络端点状态共同影响 `remote_online()`。SDK 后续转发流程不在本仓库。 |
| CAN / PGN / BAM | CAN 是设备采集输入；解析器按通道、CAN ID 和参数组编号（PGN）选择数据。此代码用 `0xECFF`/`0xEBFF` 帧处理 J1939 BAM 多包拼接。 |
| VIN / ICCID | VIN 是车辆识别码，帧打包按 17 字节使用；登录报文的 ICCID 在当前源码中是固定文本，并非现场读取值。 |
| BCC / R、S | BCC 是完整协议帧的校验字节；R、S 是加密服务答复中的签名分量，由程序拼回报文。两者不能互相替代。 |
| 实时/历史 | 实时数据先放内存队列；离线或未登录时写 SQLite，恢复后按历史优先送签名。数据库行被删除与平台成功接收是不同事件。 |
| ECD / OBD | 分别是非四和国六代码中使用的附加诊断报文名称；具体字段含义和符合性需按相应标准文本、原始 CAN 与平台解码核验。 |

### 9.2 核验范围

本次逐一检查了入口与构建脚本、所有自研模块目录、两套协议状态机和帧打包器、四个车型解析文件、BAM 组包、SQLite、签名、stub 消息及配置常量。`src/iniparser/`、`src/json/`、`src/list/` 等通用实现按本项目调用点说明，没有把它们的每个通用 API 当作本应用的业务流程。仓库中也有 `.o`、`.d` 和预编译 `.so`；这些构建产物不能替代当前源文件的行为证据。本分析未执行目标机联调或端到端平台测试，因此不声称报文已被设备、国家平台或企业平台成功接收。
