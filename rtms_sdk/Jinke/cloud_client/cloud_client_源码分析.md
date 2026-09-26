# cloud_client 源码分析

> 分析对象：`/home/tronlong/lyp/code/rtms_sdk/apps/cloud_client/`，仓库分支 `develop/rtms_sdk_v1.3_20240408`，提交 `bc60961e`。本文只描述此版本源码可核对的行为；设备上的配置值、云平台响应、其他进程的实现和实际运行结果没有被本文预设。行号均指上述提交的文件。本文是独立分析，`AGENTS.md` 可作简要导航。

## 1. 定位、范围与构建

`cloud_client` 是 RTMS SDK 的设备侧云通信桥接进程：连接本地 MQTT Broker 与远端 Broker，转发工况和指令，处理云端对时、认证/注册与离线工况缓存。它依赖其他本机进程提供工况、schema、系统信息、命令回复、网络信息和 MCU 命令接收端；源码本身没有 CAN 采集逻辑。依据：`main.c:117-169`、`src/mosquitto/client_mosquitto.c:214-238,337-427,579-627`、`src/nn_sock/nn_sock.c:16-105`。

| 项目 | 源码事实 |
| --- | --- |
| 构建入口 | `apps/CMakeLists.txt:57-61` 的 `WITH_CLOUD_CLIENT` 默认 `OFF`；启用后进入本目录。 |
| 目标 | `CMakeLists.txt:1-3,59-82`：CMake ≥3.11，项目版本 1.5，单一可执行文件 `cloud_client`；安装到安装前缀的 `opt/`，若干依赖 `.so` 安装到 `usr/lib/`。 |
| 依赖 | `CMakeLists.txt:4-10,70` 查找 libev、nanomsg、OpenSSL、cJSON、appmng、tbox-common、Mosquitto；链接 SQLite、zlib、uuid、pthread 等。查找/链接不等于每个库都在主流程直接调用。 |
| 平台 | `CMakeLists.txt:18-54` 的交叉编译分支支持 `EC200A`、`EG25G`、`AG35GL`、`MCIMX6Y2CVM08AB`；其他 `QL_MODULE_PLATFORM` 值报错。 |
| 编译开关 | `NOT_USED_SCHEMA` 默认 `OFF`；`_GNU_SOURCE` 和 `WITH_TLS` 由本目录 CMake 定义。TLS 配置仅在远端 Broker 端口等于 8883 时填入证书路径，见 `client_mosquitto.c:687-696`。 |
| 编译范围 | `file(GLOB_RECURSE ... src/*/*.c)` 把各子目录 C 文件并入主程序，见 `CMakeLists.txt:57-72`。`client_shared.c` 中有通用命令行配置代码，但本进程 `main()` 不解析 `argc/argv`。 |

### 模块对应

| 文件 | 核心作用 |
| --- | --- |
| `main.c` | 单实例锁、进程状态、信号、线程启动与保活。 |
| `src/cloud_dev/cloud_dev.[ch]` | `dev_config_t` 全局状态、认证候选、SD 卡及数据库初始化、国内/海外区域选择。 |
| `src/mosquitto/client_mosquitto.[ch]` | 三类 MQTT 连接、本地/远端回调、主题转发、队列、注册、历史补传和对时定时器。 |
| `src/mosquitto/client_shared.[ch]` | Mosquitto 连接与认证/TLS 选项辅助；部分代码来自通用客户端配置解析，并非本程序的命令行入口。 |
| `src/json_parse/json_parse.[ch]` | 对时、命令、注册结果的 JSON 处理；构造标准信息和时间请求。 |
| `src/nn_sock/nn_sock.[ch]` | nanomsg 命令 PUB/SUB 与网络状态 REQ/REP。 |
| `src/common/common.[ch]` | INI、设备型号映射、压缩、时间、MD5、状态文件等辅助。 |
| `src/db_manage/`、`src/sdcard_check/` | SQLite 离线缓存；SD 卡挂载、可写、空间检查。 |
| `src/base64/`、`src/md5/`、`src/posix/`、`src/sany_log/` | 编码、摘要、POSIX 同步/时间和日志辅助。不能仅由函数存在推断其在主流程被调用；例如 `sany_b64_decode()` 在本目录只有定义和声明，没有调用。 |

辅助实现与主流程的实际联系：`sany_platform_posix.c:56-74` 分别提供单调时钟（超时和对时请求）与实时时钟（业务时间戳）；`common.c:340-384` 通过 zlib 的 gzip 包装压缩工况，`common.c:386-418` 用 UUID/MD5 构造请求 ID 和注册凭据；`sany_log.c:8-111` 按配置的日志级别过滤并输出带时间的日志。Base64 解码器被编入目标，但本目录的 `sany_b64_decode()` 没有主流程调用者。单个辅助函数存在并不等于它是当前可观察的业务功能。

## 2. 总体结构与线程

`g_dev_info` 是进程级共享状态，保存连接配置、MQTT 句柄、4 个带互斥锁的消息链表、数据库句柄、注册信号量、时间/schema/系统信息标志等。连接状态还通过 `g_local_mqtt_connected`、`g_remote_mqtt_connected` 和三个 `STATUS_*` 变量维护。依据：`main.c:24`、`cloud_dev.h:9-117`、`client_mosquitto.c:24-46`。

| 执行单元 | 入口及作用 | 关键共享状态 |
| --- | --- | --- |
| 主线程 | `main.c:117-169` 初始化后每 10 秒休眠。 | `g_dev_info` |
| nanomsg 接收线程 | `nn_recv_thread` 轮询命令回复和网络信息回复，见 `nn_sock.c:259-289`。 | `plmn`、`operator`、`network_online` |
| 本地 MQTT 线程 | `mosq_local_client_task` 建立连接、订阅并发送待转发到本地的消息，见 `client_mosquitto.c:429-528`。 | `local_mosq`、`send_list` |
| 注册协调线程 | `mosq_register_task` 等信号量，停止旧远端线程后切换凭据或启动注册，见 `client_mosquitto.c:1100-1143`。 | `connect_info`、`device_register_sem`、`register_start` |
| 远端 MQTT 线程 | `mosq_remote_client_task` 连接云端、对时、上报系统/标准/schema 信息、发送实时队列，见 `client_mosquitto.c:644-894`。 | `remote_mosq`、`recv_list`、各上报标志 |
| 指令回复线程 | `mosq_cmd_reply_task` 将本地回复队列以原主题发往远端，见 `client_mosquitto.c:1240-1276`。 | `recv_reply_list` |
| 历史补传线程 | `mosq_history_data_task` 从 SQLite 和内存队列补传，见 `client_mosquitto.c:1179-1238`。 | `db_handle`、`history_list`、`cbor_schema_report` |
| 保活线程 | `cpactive_task` 每约 5 秒调用 `cpactive_upt_atime`，见 `main.c:106-115`。 | CPActive 句柄 |
| 临时注册线程 | 满足条件后由协调线程创建、等待退出，见 `client_mosquitto.c:927-1068,1119-1135`。 | 注册 MQTT 连接、认证结果 |

`main()` 对 `pthread_create()` 的返回值不作检查，线程间用若干 `sleep(1)` 错峰启动；这些休眠不是服务就绪确认。部分线程在入口 `pthread_detach()`，远端线程则由协调线程 `pthread_join()`。Mosquitto 网络循环另外通过 `mosquitto_loop_start()` 启动库线程。依据：`main.c:148-166`、`client_mosquitto.c:496-498,726-748,1100-1143,1179-1182,1240-1243`。

```text
main 启动
  │
  ▼
打开 /tmp/cloud_client.pid 并尝试 flock
  ├─ 锁被其他实例持有（EWOULDBLOCK）→ 退出 1
  └─ 加锁成功，或打开/加锁遇到其他错误 → 登记 CPActive → 云状态写 0 → 初始化 Mosquitto
                                      │
                                      ▼
                                 dev_info_init()
                                      ├─ 失败 → 返回 -1
                                      └─ 成功 → 启动 nanomsg、本地 MQTT 线程
                                                  ↓
                                              选择国内/海外域名
                                                  ↓
                                  启动注册协调、远端 MQTT、回复、历史、保活线程
                                                  ↓
                                         主线程每 10 秒休眠并循环
```

图中“打开/加锁遇到其他错误仍继续”是 `is_instance_existing()` 的实际返回路径：`open()` 失败返回 `false`；`flock()` 失败但错误不是 `EWOULDBLOCK` 时也返回 `false`。只有锁被占用这一分支阻止启动，不能把该锁当作所有异常下都有效的单实例保证。`select_domain_cfg()` 返回值也未由 `main()` 检查。依据：`main.c:26-41,148-159`。

## 3. 初始化和区域选择

`dev_info_init()` 先清零结构，再从 `/opt/conf.ini` 读取 `dev:id`、`dev:secret`；缺失则返回失败。接着尝试读取 `/opt/conf_ext2.ini`、`/opt/conf2.ini`、设备型号和模组信息；后四项的返回值没有在此函数逐一判错。认证候选：若 `conf2.ini` 有非空 ID/密钥，则候选 0 是该文件，候选 1 是 `conf.ini`；否则只有 `conf.ini` 一组。然后连接 nanomsg `16008` SUB、`26008` PUB、`38001` REQ，检查 SD 卡，条件满足才建立数据库与四个消息队列。依据：`cloud_dev.c:57-163`、`common.c:78-233`。

SD 卡检查以 `/media/sdcard` 为真实挂载点，检查只读、至少 `500×1024 KiB` 剩余空间和写入测试；检查失败时仍会建立内存队列，但不创建 SQLite 句柄。检查通过后若数据库打开失败，初始化失败。依据：`cloud_dev.c:75-140`、`sdcard_check.c:15-79`。

`sdcard_ready` 只在 `dev_info_init()` 期间赋值，主循环没有重新执行 `sdcard_check()`。因此启动时未挂载、启动后插卡、运行中拔卡是三种不同状态；从源码不能推断程序会自动切换数据库模式。排查时应同时记录**进程启动时间**与**SD 卡挂载变化时间**。依据：`cloud_dev.c:117-140`、`client_mosquitto.c:407-423`。

区域选择 `domain_select:type` 支持 `domestic`、`oversea`、`auto`，缺少键时默认为 `auto`；默认 Broker 域名及端口来自 `common.c:88-141`。普通编译路径通过 nanomsg 网络回复读取 PLMN/运营商，优先看 PLMN `460`；带 `EG25`、`AG35GL`、`EC200A` 宏时使用模组 API 读取 MCC。自动模式总计约 10 分钟超时后回退海外；配置无效也回退海外并返回错误。普通路径若网络已在线却一直没有 PLMN/运营商，还设有约 9 分钟的单独等待阈值。依据：`cloud_dev.c:166-347`、`nn_sock.c:107-199`。

```text
读取 domain_select:type
  ├─ domestic → 国内业务 Broker + 国内注册 Broker
  ├─ oversea  → 海外业务 Broker + 海外注册 Broker
  ├─ auto
  │    ├─ EG25 / AG35GL / EC200A：模组 API 读取 MCC
  │    │    ├─ MCC = 460 → 国内
  │    │    ├─ 其他有效 MCC → 海外
  │    │    └─ 约 10 分钟无有效值 → 海外
  │    └─ 其他编译路径：nanomsg 查询 PLMN / 运营商
  │         ├─ PLMN 前缀 460，或运营商 CHN/CHINA → 国内
  │         ├─ 其他有效值 → 海外
  │         └─ 超时无有效值 → 海外
  ├─ 缺少 type 字段 → 默认 auto，按上面的 auto 分支处理
  └─ type 值无效 → 海外（整个文件缺失的行为另见正文）
```

`/opt/conf_ext2.ini` 整个文件加载失败时，读取函数直接返回 `-1`，而 `dev_info_init()` 不检查这个返回值。字段的回退值只适用于 INI 成功加载后的缺失字段；文件不存在时无法据此断言连接配置有效。依据：`common.c:78-146`、`cloud_dev.c:96-108`。

## 4. MQTT 连接、认证与注册

本地 MQTT 从配置取 IP/端口（字段默认 `127.0.0.1:1883`），客户端 ID 固定为 `cloud_client`，使用 clean session、20 秒 keepalive；连接成功后订阅 `client_mosquitto.c:222-238` 的主题。远端先调用 `domain_to_ip()` 作 IPv4 DNS 解析和重试，随后连接函数仍使用域名本身；它不是直接把预解析的 IP 传给 Mosquitto。远端用当前认证 ID/密钥作为 client ID、用户名/密码，keepalive 60 秒；远端端口为 8883 时设置 `/opt/cert/ca.crt`、`client.crt`、`client.key`。远端连接成功后云状态写 `2` 并订阅八个云端主题；失败/断开写 `1`。依据：`common.c:313-338`、`client_mosquitto.c:429-577,644-750`。

连接被拒时，回调把当前候选标为失败并唤醒注册协调线程。协调线程要求旧远端线程退出，再等待 10 秒；若机器序列号可读、模型 ID 有效且所有候选都被标失败，就启动临时注册连接；否则轮换下一候选并新建远端线程。注册连接用租户、模型、设备 ID 组装 client ID，按源码中的 MD5 公式生成密码，登录注册 Broker 后订阅 `v4/s/register`。收到注册响应时解析 `body.results.username/password` 并保存，临时注册线程在连接断开后才退出；若没有获得凭据，协调线程调用 `exit(1)`。依据：`client_mosquitto.c:547-565,927-1143`、`json_parse.c:296-347`、`common.c:489-535`。

```text
选择首组凭据（conf2.ini 有效时优先，否则用 conf.ini）
  ↓
连接远端 MQTT
  ├─ 成功 → 订阅云主题 → cloud_status 写 2 → 进入业务循环
  ├─ 普通网络/DNS 失败 → 原连接路径重试
  └─ 连接被拒 → 标记本组失败 → 通知注册协调线程
                                 ↓
                         停旧远端线程并等待 10 秒
                                 ↓
                 序列号/模型 ID 可用，且候选凭据全失败？
                      ├─ 否 → 轮换下一组凭据 → 重新连接远端 MQTT
                      └─ 是 → 连接注册 Broker 并订阅 v4/s/register
                                   ↓
                      ├─ 收到含用户名/密码的响应 → 解析回调立即尝试保存 conf2.ini
                      │                            ↓
                      │                      等注册连接断开 → 重建远端连接
                      └─ 断开时仍无用户名/密码 → exit(1)
```

这里的“连接被拒”特指回调中 `result == MOSQ_ERR_CONN_REFUSED` 的分支；普通 DNS/网络连接失败走重试路径，不自动等价于注册触发。注册客户端源码没有主动发布注册请求消息，实际协议交互需由注册 Broker 行为证实。注册响应中的 `hasError` 被存入状态，但协调线程判断成功只检查用户名/密码非空，见 `json_parse.c:296-347` 与 `client_mosquitto.c:1119-1135`。远端连接成功还可能覆盖保存 `/opt/conf2.ini`，见 `client_mosquitto.c:545-555`。

## 5. 本地数据、云端下发和主题映射

本地连接回调订阅四种工况/定位源、schema、系统信息以及 OTA、锁、文件、配置、实时数据、命令等回复主题。收到 schema 时只更新内存字节数组并清 `cbor_schema_report`；收到系统信息时比较 `body.info`，有变化才替换并清上报标志。其他本地消息按远端连接状态分流。依据：`client_mosquitto.c:214-427`。

```text
本地 MQTT 收到非空消息
  ├─ schema / 系统信息 → 更新内存缓存与上报标志 → 返回
  └─ 其他主题
       ├─ 远端已连接
       │    ├─ 尚未完成首次对时 → 丢弃该消息
       │    └─ 已对时
       │         ├─ 四种工况/定位主题 → 实时队列（达 100 条先移除旧消息）
       │         └─ 其他已订阅主题 → 回复队列
       └─ 远端未连接
            ├─ 非四种工况/定位主题 → 不缓存
            └─ 四种工况/定位主题 → 历史内存队列
                 ├─ SD 卡初始化检查通过且累计 10 条 → 批量写 SQLite
                 └─ SD 卡不可用且累计 100 条 → 移除旧消息
```

| 本地源主题 | 远端在线的实时发布 | 历史补传发布 | 源码位置 |
| --- | --- | --- | --- |
| `local/data/general_data`、`local/data/gps_data` | `v4/p/post/thing/live/json/1.0` | `v4/p/post/thing/history/json/1.0` | `client_mosquitto.c:838-845,1145-1153` |
| `local/data/cbor_general_data` | `v4/p/post/thing/live/cbor/1.1` | `v4/p/post/thing/history/cbor/1.1` | `client_mosquitto.c:834-839,1153-1156` |
| `local/data/gz_general_data` | 压缩成功 `v4/p/post/thing/live/jsongz/1.1`；失败回退 `.../live/json/1.1` | 压缩成功 `v4/p/post/thing/history/jsongz/1.1`；失败回退 `v4/post/thing/history/json/1.1` | `client_mosquitto.c:845-860,1156-1171` |
| 其他已订阅的本地回复主题 | 通过回复线程以原主题、QoS 1 发往远端 | 离线不缓存 | `client_mosquitto.c:382-427,1240-1276` |

最后一个历史 JSON 1.1 主题在源码中确实少了 `/p/`，此处原样记录，不替它猜测协议正确值。实时工况发布使用 QoS 0，历史补传也是 QoS 0；对时、标准信息、系统信息、schema 和回复路径用 QoS 1。调用 `mosquitto_publish()` 返回成功只表示客户端库接受了发布请求，不能证明云端业务入库。依据：`client_mosquitto.c:764-867,1145-1177,1240-1276`。

远端消息先交给 `dealwith_payload()`：时间主题设置设备时间，命令主题解析本进程关心的 `REBOOT`、`SCHEMA`、`CUSTOM`，注册主题在注册连接回调中解析。远端主连接收到 OTA、文件、锁、配置、实时数据和命令时，仍把原主题与 payload 放入 `send_list`，由本地 MQTT 线程发送给本地 Broker；本进程直接生成的回复目前仅由命令解析分支产生。`TOPIC_SUB_CONFIG`、`TOPIC_SUB_FILES` 有直接回复发布代码，但当前 `dealwith_payload()` 不为这两种主题生成 `reply_payload`。依据：`client_mosquitto.c:579-627`、`json_parse.c:349-372`。

本地订阅列表包含 `v4/p/post/thing/live/cbor/1.2`、事件和位置信息等主题，但这些主题未进入四种工况的专用映射分支；在线且已对时时会作为原主题回复消息发送，远端离线时不缓存。头文件定义 `local/data/event` 历史映射，连接回调却没有订阅 `local/data/event`。这些都应以实际订阅和回调条件判断，不能把“常量已定义”视为功能已打通。依据：`client_mosquitto.c:222-238,337-427,1145-1177`。

## 6. 对时、标准信息和设备命令

远端连接后先发送一次对时请求，5 秒定时器在 `sync_time_flag` 尚未置位时继续请求；24 小时定时器继续请求。请求 JSON 仅含 `header.msgId` 和 `header.ts`，其中 `ts` 用单调时钟毫秒数；收到响应后按 `deviceSendTime/hubRecvTime/hubSendTime/deviceRecvTime` 计算时间，设置系统时钟。非 IMX6 分支还发 nanomsg MCU 时间命令，并向 `/tmp/ql_time_set_pipe` 写模组时间；IMX6 分支调用 `hwclock -w -f /dev/rtc1`。首次解析到符合结构的对时响应时调用 `killall can_client alarm_client` 后置 `sync_time_flag`。`parse_time_payload()` 即使 `set_mcu_time()` 或 `set_clock_time()` 失败也返回 `0`，因此这个标志不能证明系统时钟已设置成功。依据：`client_mosquitto.c:47-121,760-778`、`json_parse.c:18-95,349-372,423-449`、`nn_sock.c:201-257`。

标准信息由 `make_standard_info_payload()` 构造：`body.id` 为空字符串，`items[0].properties` 包含 `tbox_id`、`Tbox_cellular_type`、可得时的 `machine_sn` 和有效模型 ID；只有时间同步后尝试发布到实时 JSON 1.0 主题。系统信息来自本地同名主题；schema 来自 `local/data/schema`，`NOT_USED_SCHEMA` 为 `OFF` 时远端线程在 schema 未报告前反复请求本地 `local/data/get_schema`。这三类上报标志在 `mosquitto_publish()` 返回成功时就置位，发布回调收到对应消息 ID 的确认时又置位，因此标志不能当作云端业务处理成功的证明。依据：`json_parse.c:374-422`、`client_mosquitto.c:258-335,629-641,779-831`。

`REBOOT` 命令调用 `set_reboot_flag()` 并安排 10 秒后 `SIGALRM`，信号处理器执行 `system("reboot")`；`SCHEMA` 命令清 schema 上报标志；`CUSTOM` 中目前只处理包含 `type=register` 的参数并据模型 ID 更新设备型号映射。命令回复是否生成受 `header.noReply` 控制，默认需要回复，回复含原 `msgId` 和结果码；无论是否生成直接回复，远端回调仍把原命令发给本地 Broker。依据：`json_parse.c:97-295`、`main.c:43-72`、`common.c:509-559`。

## 7. 离线缓存和补传的真实边界

数据库文件固定为 `/media/sdcard/mqtt_history.db`，表 `mqtt_cache(id, topic, payload)`，元数据表 `mqtt_cache_meta(row_count)`；打开时设置 WAL、`synchronous=NORMAL` 和 500 ms busy timeout。`sdcard_check()` 要求挂载、可写、至少 500 MiB 剩余；另一个 `check_space()` 用 80% 占用率和 10 MiB 空间阈值，但该检查只在单条插入函数后调用，批量插入路径没有调用它。当前本地回调走批量插入；单条插入函数在本目录没有调用者。`check_db_integrity()`、`free_space()`、`delete_all_rows()` 也没有进入当前主流程。依据：`db_manage.c:19-101,150-266,303-499,612-633`、`cloud_dev.c:75-140`、`sdcard_check.c:45-79`。

```text
远端离线收到四种工况 → 加入 history_list
  ├─ SD 卡启动时检查通过，累计 10 条 → 事务写 mqtt_cache
  └─ SD 卡不可用，累计 100 条 → 移除旧消息
                          ↓
                    远端重新连接
                          ↓
              cbor_schema_report 为 true？
                  ├─ 否 → 每 5 秒继续等待
                  └─ 是 → 从 SQLite 查询一条 → QoS 0 发布
                              ├─ publish 返回成功 → 删除数据库一条
                              └─ publish 失败 → 保留数据库记录
                                      ↓
                              尝试发布内存队列首条
                                      ↓
                              约 200 ms 后再次循环
```

补传线程在远端连上后**无条件**等待 `cbor_schema_report`；`NOT_USED_SCHEMA=ON` 会跳过远端线程中的 schema 上报分支，却不会跳过历史线程的等待，因此不能仅凭开关名认为补传可工作。无 SD 卡时 `db_handle` 为空，`select_data_from_db()` 会返回错误，之后代码仍尝试内存队列；但未对返回值做处理，实际环境还需验证。依据：`client_mosquitto.c:806-831,1179-1238`、`db_manage.c:501-555`。

持久化范围有明显限制：未满 10 条的历史消息只在内存中；在线但尚未完成首次对时的消息直接丢弃；离线的非工况消息不缓存；实时队列最多保留约 100 条。数据库 `SELECT ... LIMIT 1` 和删除子查询均无 `ORDER BY`，不能视作严格 FIFO；删除条件是发布函数返回成功，而不是 MQTT 确认或云端业务确认。`publish_history_data()` 对未知主题的初始返回值为 `0`，可能令未知主题记录被当作已发送删除。批量写库在逐项绑定/执行时先从链表弹出并释放消息，后续事务失败/回滚时这些消息不能从该链表恢复。依据：`client_mosquitto.c:337-427,1145-1238`、`db_manage.c:387-499,501-555,590-609`。

## 8. 配置、路径、状态与外部依赖

| 路径/端口 | 字段或作用 | 代码依据 |
| --- | --- | --- |
| `/opt/conf.ini` | `[dev] id`、`secret`；初始化硬条件，也是候选连接凭据。 | `common.c:147-180`、`cloud_dev.c:88-94` |
| `/opt/conf_ext2.ini` | `[mgw] domain_oversea/domain_domestic/port/local_ip/local_port`；`[register] domain_oversea/domain_domestic/port/machinelink_tenant_id/rootcloud_tenant_id`；`[domain_select] type`；`[log] log_level`。默认值只在文件成功加载后适用。 | `common.c:78-146` |
| `/opt/conf2.ini` | `[dev] id`、`secret`：已保存的云连接认证，可能由注册或远端连接成功时覆盖。 | `common.c:214-233,522-535` |
| `/opt/device_type.ini`、`/opt/device_model.json`、`/opt/machine_sn` | 设备类型、类型与模型 ID 映射、机器序列号；注册和标准信息使用。 | `common.c:182-213,420-520` |
| `/opt/cert/ca.crt`、`client.crt`、`client.key` | 远端业务 Broker 端口为 8883 时配置证书。注册连接源码没有同样的证书路径赋值。 | `client_mosquitto.c:687-696,927-1068` |
| `tcp://127.0.0.1:16008`、`:26008`、`:38001` | 命令回复 SUB、命令请求 PUB、网络信息 REQ；对应服务端由其他进程提供。 | `nn_sock.h:17-26`、`cloud_dev.c:102-116` |
| `/media/sdcard/mqtt_history.db` | 条件启用的离线工况 SQLite 数据库。 | `db_manage.c:150-163` |
| `/tmp/cloud_client.pid` | `flock` 单实例锁，进程存活期间未主动关闭文件描述符。 | `main.c:26-41` |
| `/tmp/cloud_status` | 启动写 `0`、断开或清理写 `1`、远端连接成功写 `2`；不保证其他进程如何解释。 | `main.c:60,137`、`client_mosquitto.c:529-577` |
| `/tmp/system_is_rebooting`、`/tmp/ql_time_set_pipe` | 分别由重启命令写标志、非 IMX6 对时分支写模组时间。 | `common.c:550-559`、`json_parse.c:69-80` |

## 9. 可由源码确认的风险与未证实事项

| 事项 | 代码可确认的边界 | 建议核对点 |
| --- | --- | --- |
| 配置错误传播 | `sany_read_device_conf()` 等若干返回值未由 `dev_info_init()` 逐项检查；`select_domain_cfg()` 返回值未由 `main()` 检查。 | 缺配置文件、缺租户字段及无网络信息时的启动日志和实际连接状态。 |
| schema 与补传 | 默认 `NOT_USED_SCHEMA=OFF`；远端上报线程等待 schema，历史线程无条件等其上报标志。 | 本地 schema 生产者是否响应 `local/data/get_schema`；切换开关后的补传表现。 |
| 上报标志与重连 | schema、系统信息、标准信息的标志在发布调用成功后即置位；远端断线回调没有清这些标志，`sync_time_flag` 也没有在重连时复位。 | 远端重连后是否需要重新上报、重新对时，须以产品协议和设备日志核对。 |
| 消息交付 | QoS 0 工况和历史记录在库接受发布调用后即移除；实时队列和离线内存队列有上限。 | 断网、重连、掉电、云端拒收时是否出现丢失或重复。 |
| 本地转发与权限 | 云端命令可原主题转发至本地；`REBOOT` 在本进程也会执行。 | 本地订阅者及云平台认证策略；只在受控设备验证重启路径。 |
| 敏感数据 | `common.c:171`、`client_mosquitto.c:699-700,955-982` 等路径会打印设备密钥、连接密码或注册口令。 | 设备日志级别、收集方式和脱敏措施。本文不复制实际密钥。 |
| 锁与线程生命周期 | pid 文件打开失败仍继续；部分线程创建返回值未检查；信号处理器中调用了 MQTT 清理与 `system()`。 | 目标平台下的异常启动和退出行为；这里不声称已发生故障。 |
| 对时判定 | 对时 JSON 解析路径即使设置 MCU/系统时钟失败仍返回成功并置 `sync_time_flag`。 | 核对 `clock_settime` 权限、模组时间通道及实际系统时钟。 |
| 库连接失败路径 | 通用 `client_connect()` 在连接失败时调用 `mosquitto_lib_cleanup()`，其调用者仍会在同一进程里重试。 | 核对目标版本 Mosquitto 对该序列的行为；不能从静态代码断言每次重试都会成功。 |
| 数据库顺序与失败处理 | 查询/删除没有显式排序；批量插入在事务提交前已释放弹出的内存消息。 | 校验补传顺序和写库失败的真实数据损失范围。 |
| 外部系统 | 本目录没有云协议完整定义、现场 INI/证书、Broker 服务端、CAN/MCU/网络服务端实现或目标机日志。 | 端到端兼容性、时延和可用性需要结合设备及服务端验证，不能从本源码单独下结论。 |

## 10. 复核顺序

1. 用 `git branch --show-current`、`git rev-parse --short HEAD` 确认分析版本；切分支后先复核 `CMakeLists.txt`、`main.c` 和 `client_mosquitto.c`。
2. 设备侧核对上述 INI/JSON 文件、SD 卡真实挂载和证书，避免把代码默认值误认为部署值；核对本地 Broker、nanomsg 服务与工况/schema 生产者。
3. 分别验证首次启动、已有认证、连接被拒后候选切换/注册、国内/海外自动选择、对时前后本地消息、云端命令与回复、断线缓存和恢复补传；对每条结果保留实际日志或消息记录。
4. 本目录未定义专用测试目标；本文是静态源码分析，没有宣称已在目标设备或云平台上跑通上述路径。

## 11. 设备故障排查：从症状定位到代码路径

以下步骤用于**怀疑本程序异常时的定位**，不预设所有故障都由它引起。先保留故障发生时间、设备型号、程序版本、网络状态和原始日志，再做只读检查；不要先删数据库、改认证文件或重启进程，否则会丢失现场。设备命令是否可用取决于 BusyBox/发行版；示例中的 PID `1234` 和 `/path/to/cloud_client.log` 是替换用示例值。命令输出如需外发，应去掉设备 ID、密钥、序列号、证书、MQTT payload 和注册口令。

### 11.1 先确认“运行的是谁、日志在哪里”

1. 在设备上用 `ps | grep '[c]loud_client'` 看进程是否存在、是否重复。若 `pidof` 可用，可用 `pidof cloud_client` 取 PID；再用 `readlink /proc/1234/exe`（将 `1234` 换成真实 PID）核对实际运行文件。`CMakeLists.txt` 的安装目标是安装前缀下 `opt/cloud_client`，但仓库的设备打包样例还列出 `/opt/device_apps/.../cloud_client`；实际路径必须以对应 PID 的 `/proc/.../exe` 为准，不能按源码目录或打包样例猜。依据：`CMakeLists.txt:82`、`main.c:26-41`、`todel/file.info` 中的设备应用条目。
2. 读 `cat /tmp/cloud_status`：源码写 `0` 表示启动阶段、`1` 表示远端断开/清理、`2` 表示远端 MQTT 连接成功。该文件不是心跳，进程异常退出后可能保留旧值；应与 PID、日志时间和实际 MQTT 状态交叉核对。依据：`main.c:137`、`client_mosquitto.c:529-577`、`common.c:537-548`。
3. 日志的保存位置取决于启动该进程的脚本或管理器，本目录源码没有写定日志文件路径。`SANY_LOG_*` 通过 `sany_debug_func_register(printf, log_level)` 输出到标准输出；可用 `readlink /proc/1234/fd/1`（替换 PID）看它连向文件、终端还是管道，再查对应的服务管理日志。`log:log_level` 缺失时默认 `8`，对应当前日志实现中的 ERROR 级别，INFO/DEBUG 行可能被过滤；`/opt/conf_ext2.ini` 无法加载时日志函数可能尚未注册。因此缺少 `connected to ...` 等 INFO 日志并不能单独证明连接从未成功。依据：`common.c:78-91`、`sany_log.h:4-8`、`sany_log.c:8-25,100-111`。
4. 日志筛选只匹配状态或错误关键词，避免打印整个配置、payload 或含密码的日志行。若已有日志文件，可用下例；没有文件时按设备的进程管理方式查看标准输出。

```sh
grep -E 'APP Version|dev_info_init failed|SD card status:|connected to local broker|connected to remote broker|disconnected from remote broker|failed to connect to|failed to get remote broker ip|start register|failed to register|add query schema msg to local list|schema msg id|failed to set clock time|insert local mqtt msg to db failed|mosquitto_publish:' /path/to/cloud_client.log
```

### 11.2 总体排查流程图

```text
设备异常
  ├─ 找不到 cloud_client 进程
  │    ├─ 核对实际二进制和启动管理器
  │    └─ 查单实例锁、CPActive 登记、dev_info_init 和数据库打开日志
  └─ 进程存在
       ├─ 本地 Broker 未连接 → 查本地 IP/端口、Broker、订阅日志
       ├─ 远端 Broker 未连接 → 查区域选择、DNS、端口/TLS、认证/注册
       └─ 远端 Broker 已连接
            ├─ 工况不上报 → 查首次对时、schema、源主题、实时队列
            ├─ 云端命令无响应 → 查远端订阅、本地转发、回复主题
            └─ 离线数据不补传 → 查 schema 标志、SD 卡/数据库、历史线程
```

### 11.3 症状、证据与下一步

| 症状 | 先看什么 | 如何解释及继续定位 |
| --- | --- | --- |
| 进程起不来或反复退出 | `ps`、实际二进制、启动日志中的 `another instance is running`、`failed to add process info to CPActive`、`dev_info_init failed`。检查 `/opt/conf.ini` 是否存在及 `[dev] id/secret` 是否配置，避免直接打印密钥。 | `flock` 返回 `EWOULDBLOCK` 会退出 1；CPActive 登记失败或 `dev_info_init()` 失败会结束启动。`dev_info_init()` 中 nanomsg 连接或已检查通过的 SD 卡数据库打开失败也会返回错误。源码：`main.c:26-41,117-147`、`cloud_dev.c:79-140`。 |
| 进程在但本地 MQTT 不通 | 日志 `begin connect to local broker`、`connected to local broker` 或 `failed to connect to local broker`；核对 `/opt/conf_ext2.ini` 的 `mgw:local_ip/local_port`。可用 `ss -ltn`（没有则 `netstat -ltn`）看本地 Broker 是否监听。 | 配置字段缺失时默认 `127.0.0.1:1883`，整个配置文件加载失败则不能套用字段默认值。若连接成功但没数据，继续查本地主题生产者和订阅结果。源码：`common.c:78-108`、`client_mosquitto.c:214-255,429-528`。 |
| 远端 MQTT 不通或 `cloud_status=1` | 日志 `domain select ...`、`failed to get remote broker ip`、`begin connect to remote broker`、`failed to connect to remote broker`；核对 `mgw` 国内/海外域名、端口和 `domain_select:type`。设备具备 `getent`/`nslookup` 时核对 DNS，`ss -tn` 看连接状态；端口 8883 时核对三个证书文件是否存在。 | DNS 预解析失败每 5 秒重试；普通连接失败不直接触发注册。若 ConnAck 拒绝，才进入候选凭据切换/注册协调路径。`cloud_status=2` 仅说明 MQTT 建连成功，不代表对时、schema 或上报成功。源码：`cloud_dev.c:166-347`、`client_mosquitto.c:529-577,644-750`。 |
| 一直注册或注册失败 | 区分 `failed to connect to remote broker`、`start register`、`connected to register broker`、`failed to register` 的发生顺序；只检查 `/opt/conf2.ini`、`/opt/device_type.ini`、`/opt/device_model.json`、`/opt/machine_sn` 的存在与权限，不输出认证值。 | 两组候选凭据需按源码条件都被标为拒绝，且机器序列号/模型 ID 有效，才发起注册；注册线程依赖注册 Broker 断开后结束。收到响应却没有用户名/密码时协调线程会 `exit(1)`。注册服务的真正响应条件需要服务端日志确认。源码：`cloud_dev.c:57-75`、`client_mosquitto.c:547-565,927-1143`、`json_parse.c:296-347`。 |
| 云端已连但实时工况不上报 | 先看本地连接和四种源主题是否真的有生产者，再查云端时间响应及 `failed to set clock time`，随后看 `add query schema msg to local list`、`schema msg id`。 | 远端已连但 `sync_time_flag=false` 时，schema/系统信息以外的本地消息直接丢弃；默认构建下，远端发送线程还可能卡在 schema 等待循环，直到本地 schema 到达并发布调用成功。`schema msg id` 表明发布调用成功，不等于云平台业务确认。源码：`client_mosquitto.c:337-427,760-875`、`json_parse.c:18-95`。 |
| 设备时间不对或首次工况异常 | 对照对时请求/响应时间、`failed to set mcu time`、`failed to set clock time`、设备 `date -u`；非 IMX6 检查本机 MCU 通道和 `/tmp/ql_time_set_pipe` 的外部服务是否工作。 | 源码即使设置系统时钟失败也可能置 `sync_time_flag`；该标志不是对时成功证明。首次解析成功还会执行 `killall can_client alarm_client`，要检查这些进程是否由管理器重新拉起。源码：`json_parse.c:18-95,349-372`、`nn_sock.c:201-257`。 |
| 云端命令无回复或设备动作未发生 | 看远端 MQTT 是否订阅对应命令主题；核对云端消息是否到达、本地 Broker 是否在线、本地对应进程是否订阅原主题并发布回复。 | 本进程只直接处理时间及部分命令；OTA、文件、锁、配置、实时数据等原样转给本地 MQTT。`REBOOT` 同时在本进程安排系统重启。`/tmp/system_is_rebooting` 可由该命令写入，但文件可能是旧值，需结合时间线。源码：`client_mosquitto.c:579-627,1240-1276`、`json_parse.c:165-295,349-372`。 |
| 断线期间数据丢失或历史不补传 | 看 `SD card status:`、`insert local mqtt msg to db failed`、数据库文件大小与剩余空间；确认本地 schema 是否到达、远端是否连上，并对照进程启动与 SD 卡挂载时间。若设备装有 `sqlite3`，用只读 URI 查询 `mqtt_cache` 条数。 | SD 卡可用状态只在启动时判定；启动时不可用则只保留至多约 100 条内存消息，启动时可用但未满 10 条也仍在内存。历史线程无条件等 `cbor_schema_report`，`NOT_USED_SCHEMA=ON` 也可能一直等。即使补传调用成功，QoS 0 与删除时机也不能证明云端收到。源码：`cloud_dev.c:117-140`、`client_mosquitto.c:385-427,1179-1238`、`db_manage.c:387-555`。 |
| `cloud_status=2` 但平台仍没数据 | 依次确认本地数据生产者 → 首次对时 → schema → 实时发布主题/长度 → 云平台订阅/解析。只看状态文件会漏掉后四段。 | 该值只在远端 MQTT 连接回调写入；队列、对时、schema 和平台处理有独立条件。源码：`client_mosquitto.c:529-545,760-875`。 |

### 11.4 可在设备上执行的只读检查

下面的命令只读取状态；工具缺失时跳过对应项。`grep` 只看非密钥配置。不要直接 `cat /opt/conf.ini`、`cat /opt/conf2.ini` 或分享完整程序日志，因为源码可能打印 secret/password。

```sh
ps | grep '[c]loud_client'
cat /tmp/cloud_status
ls -l /opt/conf.ini /opt/conf_ext2.ini /opt/conf2.ini /opt/device_type.ini /opt/device_model.json /opt/machine_sn
grep -E '^(domain_oversea|domain_domestic|port|local_ip|local_port|type|log_level)=' /opt/conf_ext2.ini
grep ' /media/sdcard ' /proc/mounts
df -k /media/sdcard
ls -lh /media/sdcard/mqtt_history.db*
ss -ltn
ss -tn
date -u
```

如果安装了 `sqlite3`，可在**不修改数据库**的前提下执行 `sqlite3 'file:/media/sdcard/mqtt_history.db?mode=ro' 'SELECT COUNT(*) FROM mqtt_cache;'`。命令报锁、无表或打不开文件时先记录原始错误，不要直接删库或运行 `VACUUM`；本程序主流程也没有自动调用 `free_space()`。数据库条数只能表明本地待处理记录数量，不能代表云端已经收取的数量。依据：`db_manage.c:25-33,150-266,501-609`。

### 11.5 如何判断是否确为 cloud_client 的问题

按链路找**第一个不成立的节点**：本地生产者有无发送 → 本地 Broker 是否收到 → `cloud_client` 本地回调是否收到 → 远端 MQTT 是否连通 → 对时/schema 条件是否满足 → 发布调用是否成功 → 云平台是否收到并接受。若前一个节点没有数据，先看其生产者或 Broker；若本程序已发布但平台未收到，需同时查看网络、Broker 和平台侧证据。避免用单条 `connected to remote broker` 日志、`cloud_status=2` 或数据库行数直接归因。只读观察无法解释时，再在可控设备上增加脱敏日志或抓取相关主题，并把故障时间线、进程 PID、实际二进制路径、配置非敏感字段和两端 MQTT 记录放在一起复核。
