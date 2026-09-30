# can_client_for_jinke 全链路源码分析与流程图

<!-- rtms-analysis-guide:start -->

## 文档目录

本报告对应 `rtms_sdk/apps/can_client_for_jinke/` 在 `bc60961e` 的源码；跨项目关系见[源码分析总览](../../源码分析总览.md)。下文原章节编号保留，先用概述、目录、架构、模块和流程五节建立整体脉络。

- [项目概述](#project-overview)
- [源码目录总览](#source-overview)
- [核心架构设计](#core-architecture)
- [核心模块深度分析](#core-modules)
- [关键流程与数据流](#key-flows)
- [0. 分析依据与范围](#analysis-1)
- [1. 项目定位、产物与外部边界](#analysis-2)
- [2. 编译条件：代码意图与当前实际约束](#analysis-3)
- [3. 启动逻辑流程图](#analysis-4)
- [4. 启动资源与运行目录](#analysis-5)
- [5. 配置解析：从 JSON 到内存规则](#analysis-6)
- [6. CAN 接收与字段转换流程图](#analysis-7)
- [7. 上报选择、编码与 MQTT 流程图](#analysis-8)
- [8. 旧路径：GPS/MCU、工作状态、金科事件与 FEI4 统计](#analysis-9)
- [9. 数据与控制路径的失败模式](#analysis-10)
- [10. 设备故障排查手册](#analysis-11)
- [11. 验证状态与尚需外部资料](#analysis-12)

**顺读方式：**先读下面五节，再从原第 1 节起顺序阅读详细分析；需要查特定模块时，可用模块表跳到对应章节。

<a id="project-overview"></a>

## 项目概述

`can_client_for_jinke` 从本机 CAN 消息提取点表字段，经场景与间隔筛选后发布到配置的 MQTT Broker；构建宏组合和现场点表决定可达功能。

<a id="source-overview"></a>

## 源码目录总览

以下路径相对于该提交的 `apps/can_client_for_jinke/`；“职责”只描述源码中的构建或调用角色。

| 路径 | 职责 |
| --- | --- |
| `main.c`、`CMakeLists.txt` | 启动、目标与条件编译 |
| `src/can_mng/`、`src/can_server/` | 共享状态、场景库和 CAN 接收 |
| `src/data_process/` | Schema/点表解析及字段编码 |
| `src/scene/`、`src/scene_lib/` | 场景装载与判断 |
| `src/mosquitto/` | MQTT 连接和筛选发布 |
| `src/app_server/`、`src/event/`、`src/fei4/` | 受宏与入口条件约束的旧路径 |

<a id="core-architecture"></a>

## 核心架构设计

```text
本机 CAN → can_server → data_process / scene → MQTT 主题
配置 / Schema / 点表 ────────┘
NOT_USED_SCHEMA + USE_JINKE 控制旧 GPS / MCU / 事件路径
构建宏组合本身还需先满足类型可见性约束
```

顶层 WITH_CAN_CLIENT_FOR_JINKE 默认关闭；部分宏组合在当前源码中存在类型可见性问题，不能把默认选项写成已验证可运行配置。图中箭头表示源码中的数据或控制方向；外部服务和设备效果以正文标明的证据边界为准。

<a id="core-modules"></a>

## 核心模块深度分析

下表给出主链路模块的入口、关键判断和详细分析位置；具体函数、常量与失败路径以所链章节中的源码引用为准。

| 模块 | 源码入口 | 关键判断与输出 | 详细分析 |
| --- | --- | --- | --- |
| 构建与入口 | `CMakeLists.txt`、`main.c` | 三组开关共同决定目标纳入和可达路径 | [进入章节](#analysis-3) |
| CAN 与点表 | `src/can_server/`、`src/data_process/` | 按槽位、CAN ID、字段规则更新数据项 | [进入章节](#analysis-7) |
| 场景和上报 | `src/scene_lib/`、`src/mosquitto/` | 场景、间隔、变化阈值及编码分支控制发布 | [进入章节](#analysis-8) |
| 旧路径 | `src/app_server/`、`src/event/`、`src/fei4/` | 只有满足宏与入口条件才考虑 GPS/MCU、事件和统计 | [进入章节](#analysis-9) |

<a id="key-flows"></a>

## 关键流程与数据流

~~~text
配置 / 点表 → CAN 接收 → 字段换算与场景更新
字段 + 场景 → 间隔 / 接收时间 / 变化阈值筛选
合格字段 → 按 mtyp 编码 → 对应 MQTT 主题
~~~

这是构建宏使主链路可达时的顺序。先核对[构建条件](#analysis-3)，再顺读[启动图](#analysis-4)、[CAN 图](#analysis-7)、[上报图](#analysis-8)；Schema 和旧事件路径受条件编译限制。

<!-- rtms-analysis-guide:end -->

<a id="analysis-1"></a>

## 0. 分析依据与范围

- 源码仓库：`/home/tronlong/lyp/code/rtms_sdk`；项目：`apps/can_client_for_jinke/`。分析时本地分支为 `develop/rtms_sdk_v1.3_20240408`，提交短哈希 `bc60961e`。本目录是 SDK 的子项目，不是独立 Git 仓库。
- 本文逐项核对本目录 `main.c`、`CMakeLists.txt`、`src/` 中的实现，并检查 `apps/CMakeLists.txt:100-103` 的接入条件。下文 `文件:行号` 均相对于本项目源码目录，顶层文件会显式标注。
- 结论分为**源码事实**和**条件/边界**：代码未提供金科专用运行点表、目标设备日志和云端回执，本文不给出这些材料才能证明的车型字段、最终部署组合或端到端成功结论。没有执行交叉编译或设备联调。
- 本文分析的是 `can_client_for_jinke` 这份源码。SDK 其他应用的同名 `can_client` 或其他设备型号的 JSON 配置不能直接代入本项目。

**阅读顺序：**想了解整体流程，先看第 3、6、7、8 节的文本流程图；设备现场排查直接看第 10 节；编译或配置疑问看第 2、4、5 节。所有流程图均使用普通文本代码块，无需 Mermaid 支持。

<a id="analysis-2"></a>

## 1. 项目定位、产物与外部边界

`apps/CMakeLists.txt` 的 `WITH_CAN_CLIENT_FOR_JINKE` 默认 `OFF`，设为 `ON` 才 `add_subdirectory(can_client_for_jinke)`。本目录 CMake 声明项目 `can_client`、版本 1.5、C99，生成 `can_client` 可执行文件和名为 `scene` 的共享库 `libscene.so`，安装到安装前缀下的 `opt/`。`src/*/*.c` 被递归收集到可执行文件；`src/scene_lib/scene_judge.c` 从该列表移除后单独构建共享库。依据：`CMakeLists.txt:1-2,10,44-50,73-79`。

程序的职责是**CAN 数据整理与 MQTT 发布**：从本机 nanomsg CAN 服务接收帧，依据点表提取字段，由动态库判断场景，再按点表间隔和变化阈值发布到 MQTT Topic。源码不打开 Linux CAN 设备节点，也不实现 Broker 服务；MQTT 连接目的地取自 `/opt/conf_ext2.ini`，缺省为 `127.0.0.1:1883`，但配置值可能指向其他主机。源码没有另建一条独立的云平台连接或业务回执链路。是否有其他进程转发 `local/data/*`、下游是否接收，须看部署系统。依据：`src/can_server/can_server.c:190-225`、`src/can_mng/can_mng.c:10-29`、`src/mosquitto/client_mosquitto.c:108-246`。

| 模块 | 源码入口 | 实际职责 |
| --- | --- | --- |
| 程序入口 | `main.c:39-115` | 初始化、加载配置、注册保活、启动线程。 |
| 共享上下文 | `src/can_mng/can_mng.c:10-29,98-115` | 读取 MQTT Broker 地址、定位程序目录、加载场景库、持有点表和运行状态。 |
| CAN 订阅 | `src/can_server/can_server.c:144-227` | 订阅三个本地端口、按帧结构拆消息、更新字段和场景。 |
| 配置/字段 | `src/data_process/data_process.c:136-849` | 解析 Schema 与点表、创建规则和字段、提取 CAN/应用值。 |
| 上报筛选/编码 | `src/data_process/data_process.c:852-1371` | 按场景、间隔、变化阈值筛选，生成两种 JSON 或 CBOR 载荷及 CBOR Schema。 |
| 场景判断 | `src/scene/scene.c:7-41`、`src/scene_lib/scene_judge.c:76-97` | 动态加载 `judge_scene`，基于指定 CAN ID 返回场景名。 |
| MQTT 客户端 | `src/mosquitto/client_mosquitto.c:46-247` | 连接配置的 Broker，处理 Schema 请求/发布、工作数据与可选事件发布。 |
| 旧路径扩展 | `src/app_server/`、`src/event/`、`src/fei4/`、`src/queue/` | 受编译宏控制的 GPS/MCU、金科钻孔事件和统计。 |

<a id="analysis-3"></a>

## 2. 编译条件：代码意图与当前实际约束

| 选项 | 定义与默认值 | 代码中的主要作用 |
| --- | --- | --- |
| `WITH_CAN_CLIENT_FOR_JINKE` | `apps/CMakeLists.txt:100`，`OFF` | 是否纳入顶层构建。 |
| `NOT_USED_SCHEMA` | `CMakeLists.txt:13`，`OFF` | `OFF` 加载 `GeneralParam.json` 并发布 Schema；`ON` 跳过该文件、启用 GPS/MCU 旧路径。 |
| `USE_JINKE` | `CMakeLists.txt:19`，`OFF` | 与 `NOT_USED_SCHEMA` 同时生效时启用事件、日期、FEI4 统计；单独开启不足以启动这些线程。 |

**明确的源码冲突：**`can_mng_t.can_data` 和 `can_data_t` 仅在 `NOT_USED_SCHEMA && USE_JINKE` 下声明（`src/can_mng/can_mng.h:21-29,82,117-130`），但 `src/can_mng/can_mng.c:48,112` 无条件使用 `can_data_t` 和 `can_mng->can_data`；`src/fei4/fei4.c` 也被 CMake 无条件收集。因此其余三种宏组合按当前代码会出现类型/成员不可见的编译问题。这是静态源码结论，尚未用目标工具链编译验证两宏同时开启的完整可编译性。

| `NOT_USED_SCHEMA` | `USE_JINKE` | 入口分支 | 对当前源码的结论 |
| --- | --- | --- | --- |
| OFF | OFF | 加载通用参数并发布 Schema | `can_data_t` 未定义，不能把默认选项当作已可运行配置。 |
| OFF | ON | 入口仍走默认 Schema 分支 | `can_data_t` 未定义；仅开 `USE_JINKE` 不启动金科线程。 |
| ON | OFF | GPS/MCU 旧路径 | `can_data_t` 未定义。 |
| ON | ON | GPS/MCU、金科事件与统计 | 相关类型可见；后续编译、链接和设备运行仍需验证。 |

交叉编译分支只处理 `QL_MODULE_PLATFORM=EC200A` 与 `EG25G`，其他值使 CMake 报错。CMake 查找 nanomsg、OpenSSL、cJSON、appmng、Mosquitto，并链接 cn-cbor、tbox-common、pthread、dl 等。依赖查找通过和目标程序真正可链接是两层检查。依据：`CMakeLists.txt:4-8,25-42,66-79`。

<a id="analysis-4"></a>

## 3. 启动逻辑流程图

图中步骤来自 `main.c:39-115`，构建分支以宏标出。`can_mng_init()` 内的场景库加载失败不会使主函数直接退出，后续风险见第 9 节。

```text
main
  │
  ├─ 初始化日志、Mosquitto
  ├─ can_mng_init：分配上下文，确定程序目录，尝试加载 libscene.so
  │     └─ 分配失败 ──────────────────────────────────────→ 退出 -1
  ├─ 读取 /opt/conf_ext2.ini 的 mgw 地址
  │     └─ 读取失败 ──────────────────────────────────────→ 退出 -1
  ├─ NOT_USED_SCHEMA=ON 且 USE_JINKE=ON：检查日期、初始化事件队列
  ├─ NOT_USED_SCHEMA=OFF：加载 GeneralParam.json
  │     └─ 加载失败 ──────────────────────────────────────→ 退出 -1
  ├─ 加载 Pointsheet_info1.json
  │     └─ 加载失败 ──────────────────────────────────────→ 退出 -1
  ├─ 登记 CPActive（超时 10 秒）
  │     └─ 登记失败 ──────────────────────────────────────→ 退出 -1
  ├─ 启动保活、CAN、MQTT 线程
  ├─ NOT_USED_SCHEMA=ON：再启动 GPS/MCU 应用消息线程
  ├─ 两宏均 ON：再启动日期检查、FEI4 统计线程
  └─ pthread_join(最后写入的 tid)；若返回则清理 Mosquitto
```

`main()` 对 `can_mng_init()`、INI、两份 JSON、`cpactive_add_pinfo()` 有显式失败检查；对 `create_cpactive()` 返回指针、`load_libscene()` 返回值和每次 `pthread_create()` 的返回值没有完整检查。同一个 `pthread_t tid` 反复作为输出参数，最终只等待最后一次赋值的线程 ID。`cpactive_task()` 每 5 秒更新一次访问时间，登记超时为 10 秒；它是进程管理心跳，不是 CAN/MQTT 的数据健康证明。依据：`main.c:24-37,57-112`、`src/can_mng/can_mng.c:98-114`。

<a id="analysis-5"></a>

## 4. 启动资源与运行目录

| 资源 | 读取方式 | 必要性与准确边界 |
| --- | --- | --- |
| `/opt/conf_ext2.ini` | `iniparser_load()`，键 `mgw:local_ip`、`mgw:local_port` | 文件打不开时主程序退出；单个键缺失时默认 `127.0.0.1:1883`。键名虽为 `local_ip`，代码没有强制地址必须在本机。`src/can_mng/can_mng.c:8-29`。 |
| 程序目录 `libscene.so` | `/proc/self/exe` 推得目录，`dlopen` 后查找 `judge_scene` | 此文件要与可执行程序同目录；加载错误未传回主函数。`src/can_mng/can_mng.c:31-45,109-111`、`src/scene/scene.c:13-41`。 |
| 程序目录 `GeneralParam.json` | 默认路径调用 `load_general_param()` | `NOT_USED_SCHEMA=OFF` 才读取；读取/解析失败退出。`main.c:77-84`。 |
| 程序目录 `Pointsheet_info1.json` | 调用 `load_pointsheet_json()` | 两种入口路径均读取；失败退出。`main.c:86-92`。 |
| 本机 CAN 数据端口 | nanomsg SUB 连接 `127.0.0.1:16002/16003/16004` | 由其他进程发布帧；本程序不配置物理 CAN 控制器。`src/can_server/can_server.h:9-11`、`.c:190-225`。 |
| MQTT Broker | Mosquitto 客户端连接 INI 地址 | 客户端 ID 固定为 `can_client`，keepalive 参数 30 秒。`src/mosquitto/client_mosquitto.c:108-147`。 |
| `/media/sdcard/jk_evt_no` | 读写 `日号:工单号` | 仅金科事件分支使用，不是 MQTT 离线消息队列。`src/event/jk_event.h:7`、`.c:8-34`。 |

源码目录本身没有金科专用的两份 JSON，CMake 安装语句也不安装它们。仓库其他设备目录存在同名文件，不足以确定本项目使用哪一份；因此本文不列出“金科实际点位清单”。

<a id="analysis-6"></a>

## 5. 配置解析：从 JSON 到内存规则

`GeneralParam.json` 在默认路径读取顶层 `sver/pver/mtyp` 到 `general_info`，并从 `dev[].msg.rls` 建立 Schema 项，包含 `nm`、`ptyp`、`cbidx`。`Pointsheet_info1.json` 再读取 `mver/sver/pver/mtyp/sce/dev/app`；实际工作数据格式读取的是点表的 `data_process_mng->msg_type`，不能把通用参数中的 `general_info.msg_type` 直接当作发送开关。依据：`src/data_process/data_process.c:688-749,756-849`、`src/mosquitto/client_mosquitto.c:182-195`。

| 点表字段 | 实现中的含义 | 依据 |
| --- | --- | --- |
| `sce` | 顶层场景名数组，填入 `scene[]`；字段各自也有适用的 `sce`。 | `data_process.c:790-801,388-406` |
| `dev[].slot` | 作为 `data_rule[port]` 索引，预期 0～2。 | `data_process.c:807-827`、`data_process.h:104` |
| `dev[].msg[].val` | 最外层规则的值作为 CAN ID 哈希键。字符串可用 `#` 开头十六进制或十进制。 | `data_process.c:278-288,825-827,676-685` |
| `rls`、`pts` | 分别创建子规则和字段列表。处理函数若存在 `pts`，优先处理字段；没有字段列表时才递归子规则。 | `data_process.c:290-312,453-568` |
| `pts[].ofs/len/bo/sign` | 起始位、位宽、字节序、是否有符号。 | `data_process.c:322-338,465-502` |
| `pts[].sc/pofs/min/max` | 数值的比例、物理偏移和上下限。 | `data_process.c:340-351,504-515` |
| `pts[].nm/ptyp/cbidx` | 名称、值类型、CBOR 索引。默认路径按名称沿用 Schema 或从 50 起分配新索引；旧路径使用点表索引。 | `data_process.c:353-385,223-250` |
| `pts[].sce/scems/udif` | 场景、每场景毫秒间隔、变化阈值。`scems` 被收集到全局间隔哈希表。 | `data_process.c:330-332,388-429` |
| `app[].nm/params` | 名为 `can_client` 的应用数据项会作为特殊规则 `UINT32_MAX` 放在通道 0，可由 GPS/MCU/统计函数按名称写值。 | `data_process.c:136-257,829-844,571-642` |

**子规则边界：**`get_common_data_rule()` 解析并存储每层 `typ/ofs/len/val`；接收帧时 `parse_common_msg()` 只用最外层 CAN ID 查哈希。`set_common_data_item()` 对子规则逐个递归，没有再次比较子规则 `val` 或其 `typ/ofs/len`。因此不能把点表的 `rls` 理解成已经按子条件筛选的分支；若目标配置依赖这种条件，需要针对真实点表检查输出。依据：`data_process.c:260-309,445-568,676-685`。

`get_file_data()` 负责读文件，但没有检查 `fread()` 的实际读取字节数；JSON 解析依赖缓冲区内容。场景和间隔数组写入时没有系统性的 `SCENE_MAX=10` 上限检查；`dev[].slot` 也直接用于三个元素的 `data_rule[]`。这些是输入有效性约束，不是已经发生的现场故障。依据：`data_process.c:103-133,180-219,390-429,807-827`、`data_process.h:9,67-69,104`。

<a id="analysis-7"></a>

## 6. CAN 接收与字段转换流程图

`can_frame_t` 按源码字段顺序是 `can_id`（32 位）、`can_dlc`（16 位）、`rsv_ms`（16 位）、`data[8]` 和 `timestamp`（32 位）。两个处理函数都以整个帧结构的地址作为位偏移基准；数据区从第 8 字节即位偏移 64 开始。点表偏移应匹配这个结构，不是直接从 `data[0]` 的位 0 开始。依据：`src/can_server/can_server.h:13-20`、`src/data_process/data_process.c:453-487`、`src/scene_lib/scene_judge.c:35-72`。

```text
CAN 线程
  │
  ├─ 连接 16002 / 16003 / 16004 三个 NN_SUB 端口
  └─ 循环 nn_poll（最多等待 2000 ms）
       ├─ 无消息 → 继续等待
       └─ 有消息 → nn_recv → 按 sizeof(can_frame_t) 切出完整帧
                                │
                                ├─ NOT_USED_SCHEMA=ON：parse_special_msg
                                ├─ parse_common_msg：按通道和 CAN ID 查点表
                                │    └─ 命中规则才提取字段：位偏移、字节序、符号、缩放
                                ├─ judge_scene：返回场景名
                                └─ set_cur_scene → 下一帧；处理完释放消息
```

普通规则先用通道与 CAN ID 查找，找到后依 `ofs/len/bo` 提取原始整数。数值类型 1/2/3 用 `sign` 做补码符号处理，再按 `raw × sc + pofs` 换算并限幅；类型 7 把原始值按 `int16_t` 转换后同样缩放；类型 4/5 按字节读字符串。处理成功会更新 `time_rcv`，代表本进程收到并解析值的单调时钟时间。依据：`data_process.c:445-568,676-685`。代码未依据 `can_dlc` 限制字段读取长度，点表位宽超出帧结构时存在越界风险。

动态场景库的 `param_cfg` 固定使用 CAN ID `0x171` 与 `0x172`，分别缓存发动机转速和速度；即使当前帧是其他 CAN ID，也会用此前缓存的两个值计算场景。速度 >0 返回 `drive`；否则转速 0 返回 `power_on`，1～800 返回 `idle`，>800 返回 `work`。初始两个缓存值为 0，初始场景为 `power_on`。这与旧路径的 `device_standard_work_state` 数字状态是不同变量与判定逻辑。依据：`scene_judge.c:35-97`、`data_process.c:1374-1392`。

<a id="analysis-8"></a>

## 7. 上报选择、编码与 MQTT 流程图

MQTT 线程以 `can_client` 为 Client ID，注册连接、断开、订阅与消息回调。首次 `mosquitto_connect()` 返回成功后启动 `mosquitto_loop_start()`；连接回调把状态设为 `STATUS_CONNACK_RECVD`。默认 Schema 路径随后循环尝试生成并发布一次 CBOR Schema，直至 `mosquitto_publish()` 返回成功。若收到 `local/data/get_schema` 的非空载荷，则再次发布 Schema。依据：`src/mosquitto/client_mosquitto.c:46-179`。这里的“成功”是客户端 API 的返回值，不是业务端确认。

```text
MQTT 线程 → 连接 INI 指定的 Broker
  │          └─ 首次 connect 调用失败：每 2 秒重试
  └─ connect 调用成功 → 启动 Mosquitto 网络循环
  │
  ├─ 连接回调失败：回调本身不显式重发连接请求
  └─ 连接成功
       ├─ NOT_USED_SCHEMA=OFF：订阅 get_schema，发布一次 CBOR Schema
       └─ 工作数据循环：遍历点表的 interval 列表
            ├─ 间隔未到 → 检查下一个间隔
            └─ 间隔已到 → 按场景、间隔、time_rcv、udif 筛字段
                 ├─ 无合格字段 → 不发布工作数据
                 └─ 有合格字段 → 按 mtyp 编码并发布 QoS 0
                      ├─ 0：普通 JSON → local/data/general_data
                      ├─ 1：CBOR      → local/data/cbor_general_data
                      └─ 2：普通 JSON → local/data/gz_general_data（未压缩）
                      发布后释放载荷 → 继续循环
```

间隔表来自点表中各字段的 `scems`，按整数去重、排序。MQTT 循环以单调时钟判断每个间隔是否到期，到期后立即更新该间隔的 `time_pub`，再生成载荷。对每个字段，筛选需同时满足：当前 `cur_scene` 与字段 `sce[j]` 相符、字段 `interval[j]` 等于当前间隔、`time_rcv != 0`。第一次合格上报通过；后续 `udif=0` 允许周期重复；否则数值差值达到阈值、或字符串变化才通过。字段的 `time_pub` 在生成载荷时更新，不等待 MQTT 或下游确认。依据：`client_mosquitto.c:182-231`、`data_process.c:852-1104`。

| `mtyp` | Topic | 实际载荷结构 | MQTT 设置 |
| --- | --- | --- | --- |
| 0 `MSG_TYPE_JSON` | `local/data/general_data` | `body.id=""`，`body.items[0]` 含毫秒时间戳 `ts` 和 `properties`。 | QoS 0、retain=false。 |
| 1 `MSG_TYPE_CBOR` | `local/data/cbor_general_data` | CBOR 前缀与流水号后，编码设备数据、Schema 版本、时间戳和字段索引映射。 | QoS 0、retain=false。 |
| 2 `MSG_TYPE_JSONGZ` | `local/data/gz_general_data` | `body.things[0].id=""`，其 `items[0]` 含 `ts`、`properties`；**实际是普通 JSON 字符串，没有 Gzip 压缩步骤**。 | QoS 0、retain=false。 |
| Schema | `local/data/schema` | CBOR Schema，字段项由名称和类型构成。 | QoS 1、retain=false，仅默认路径。 |
| 请求 Schema | `local/data/get_schema` | 收到非空消息时触发再次发布 Schema。 | 订阅 QoS 1，仅默认路径。 |
| 金科事件 | `local/data/event` | `DrillingCompletionEvent` JSON。 | QoS 0、retain=false，仅双宏路径。 |

类型枚举见 `data_process.h:12-27`；数据生成见 `data_process.c:1106-1371`；Topic 与 QoS 见 `client_mosquitto.h:4-9`、`client_mosquitto.c:46-52,93-103,196-239`。普通 JSON 与 `JSONGZ` 都在无字段通过时返回空指针，因此不会发布空工作数据。CBOR 也在无字段时返回 0。源码没有为工作数据建本地数据库或逐条平台应答状态机；客户端 API 调用成功不能推出最终业务接收。

### 关于 Schema 与 CBOR 索引

默认路径先从 `GeneralParam.json` 取部分 Schema 项，点表读入时按字段名查找；新名字以从 50 起递增的 `global_cbor_index` 分配索引。旧路径跳过通用配置，应用字段要求显式 `cbidx`。`cbor_model_schema_generate()` 输出的 Schema 条目代码只附加“名称、值类型”，没有显式把 `cbor_index` 作为每项第三个值写入。因此不能仅根据“Schema 带有索引”的口头描述推断接收方解析方式；需按生成函数和对应下游协议核对。依据：`data_process.c:15-19,223-250,364-383,1280-1371`。

<a id="analysis-9"></a>

## 8. 旧路径：GPS/MCU、工作状态、金科事件与 FEI4 统计

`NOT_USED_SCHEMA=ON` 时 `main()` 启动 `app_msg_task()`。GPS 通道是 `tcp://127.0.0.1:16005` 的 nanomsg SUB；MCU 通道是 `tcp://127.0.0.1:38000` 的 nanomsg REQ。应用线程创建 5 秒周期的定时器发 MCU 请求，接收后分别解析 GPS `location_info` 与 MCU `status.io` 的 JSON，把值更新到点表的 `app` 字段。依据：`src/app_server/app_server.c:154-217`、`gps.c:10-93`、`mcu.c:10-126`。

`quick_check_working_state()` 由旧路径收到发动机 CAN ID `0xCF00400` 后调用，结合 MCU ACC 与发动机转速计算数字状态：ACC 关为 0，ACC 开且转速 0 为 1，转速 1～900 为 2，>900 为 3。MCU 更新时还检查发动机数据超过 60 秒未更新的情况。这个数字状态通过应用数据项发布；它与场景库的 `power_on/idle/work/drive` 不是一套逻辑。依据：`can_server.c:55-74`、`app_server.c:18-23,58-110`。

`USE_JINKE=ON` 且 `NOT_USED_SCHEMA=ON` 时，金科扩展按如下流程运行：

```text
收到 CAN ID 0x215
  │
  ├─ 提取工单号、计划/实际深度、耗时和成功标志
  ├─ 工单号未变化 → 忽略，等待下一帧
  └─ 工单号变化 → 创建事件并尝试入队
       ├─ 入队失败 → 等待下一帧（当前代码未释放该事件）
       └─ 入队成功 → 更新上一工单号和 /media/sdcard/jk_evt_no
            └─ MQTT 循环出队 → 生成 DrillingCompletionEvent JSON
                 └─ 发布 local/data/event（QoS 0）
```

`0x215` 的 8 字节分别包含工单号、计划深度两字节、实际深度两字节、耗时两字节、成功标志。内存队列用互斥锁保护，`MAX_SIZE=10` 的环形队列以一个空槽区分满/空，最多存放 9 个事件；队列满时入队失败，代码没有在该分支释放刚分配的事件。事件生成函数从队列取出后释放事件对象，再调用 `mosquitto_publish()`；不以 Broker/云端确认作为出队条件。日期检查线程每 10 秒读取本地日号，不同时把上一工单号置零并写入 `/media/sdcard/jk_evt_no`。依据：`can_server.c:20-52,76-100`、`event/jk_event.c:8-98`、`queue/queue.h:4-13`、`queue/queue.c:7-60`、`client_mosquitto.c:233-240`。

同一双宏路径还启动 `fei4_statistics_handler()`。代码每约 300 秒依次执行 `statistical_data_setup()`、清零、`statistical_data_accumulate()`：它先用此前保存的样本计算参考扭矩、功率、SCR 上下游 NOx 浓度、进出口温度、燃料流量等应用字段，再在清零后采入当前样本。累加函数没有在 0.5 秒循环的每次迭代中调用，因此不能把这些值称为连续采集 300 秒所得的时间平均值；首次到达 300 秒时也尚无此前累加样本。未获得有效原始值的项目不一定写出，NOx 质量流量代码位于 `#if 0`。本目录没有单独的国四平台联网协议状态机；“计算并写应用字段”是源码可以确认的行为。依据：`src/fei4/fei4.c:7,25-136,139-227`。

<a id="analysis-10"></a>

## 9. 数据与控制路径的失败模式

| 位置 | 源码可确认的行为 | 运维/开发上的含义 |
| --- | --- | --- |
| `can_mng_init()` 调 `load_libscene()` | 返回值未检查；场景函数指针可能仍为空。 | CAN 帧到达时有空函数指针调用风险；启动日志不足以证明场景库正常。 |
| `load_libscene()` 错误标签 | `dlopen()` 失败也会进入调用 `dlclose(lib_handle)` 的路径。 | 失败处理本身应单独验证，不能假设一定能返回 `-1`。 |
| `read_absolute_path()` | `readlink()` 结果未检查是否等于 `PATH_MAX`，随后写 `realpath[cnt]='\0'`。 | 特殊超长路径有越界风险；这是代码边界，不代表现有部署必现。 |
| 点表装载 | `slot`、`sce`、`scems`、字段偏移/位宽缺少完整边界验证。 | 不匹配的点表可能产生错误字段甚至越界；需离线校验配置。 |
| CAN 接收 | 按 `sizeof(can_frame_t)` 拆包，不检查 `can_dlc` 和剩余不足一帧的字节。 | 对上游帧结构有严格依赖；不完整尾部不处理。 |
| 规则递归 | 子规则 `val` 未在字段提取时再次比较。 | 不能假定嵌套规则实现了条件分支筛选。 |
| CAN 数值字节拼接 | 提取函数把原始帧转为 `char *`，再将 `can_msg_data[n]` 转为 `uint64_t` 并移位；若目标平台 `char` 为有符号类型，最高位为 1 的字节可能在转换前被符号扩展。场景库的 `calc_value()` 也采用相同写法。 | 多字节字段可能读错；具体数值需针对目标编译器、位宽和帧数据验证，不能只看点表换算式。 |
| 字符串字段 | 提取时先用固定 `char value_string[9]`，再按点表计算的 `size` 执行 `memcpy()`，未显式限制 `size<=8`。 | 超长或错误点表可能写越界。 |
| 工作数据发布 | `time_pub` 在生成载荷时更新，QoS 0，无持久化。 | 调用成功不等于平台接收；失败后不能从本目录找到可靠重发队列。 |
| 事件发布 | 从内存队列取出并释放，再发布 QoS 0。 | MQTT 失败时事件已不在队列中。 |
| 线程启动 | 未逐项检查 `pthread_create()`，复用同一个 `tid`。 | 进程存活不足以证明各工作线程均运行。 |

表中“风险”由对应控制流推导，并非设备现场故障记录。依据集中在 `src/scene/scene.c:18-41`、`src/scene_lib/scene_judge.c:41-73`、`src/can_mng/can_mng.c:31-45,98-114`、`src/data_process/data_process.c:180-219,390-429,445-568,807-827,852-1074`、`src/can_server/can_server.c:144-187`、`main.c:100-111`、`src/event/jk_event.c:36-98`、`src/mosquitto/client_mosquitto.c:196-240`。

<a id="analysis-11"></a>

## 10. 设备故障排查手册

以下命令是**在目标设备上观察现状的示例**，不是已执行的实测结果。设备可能使用 BusyBox，缺少 `ss`、`jq`、`mosquitto_sub`、`journalctl`、`file` 等工具；缺少时保留同一检查目的，用设备已有工具或从部署日志获取证据。先记录设备型号、固件版本、实际 `can_client` 二进制、启动方式与时间，再按下述顺序定位。不要为排查而在正在运行的设备上再启动一个 `can_client`：源码使用固定 MQTT Client ID `can_client`，重复实例会干扰连接判断（`client_mosquitto.c:117`）。

```text
确认故障时间、设备和固件版本
  │
  ├─ can_client 进程不存在
  │    → 查构建选项、启动方式、INI/JSON、动态库和启动日志
  └─ 进程存在
       ├─ CAN 未进入 → 查 16002～16004 的发布者、通道和帧结构
       └─ CAN 已进入
            ├─ 字段/场景不满足规则 → 查 CAN ID、ofs/len、sce/scems/udif
            └─ 字段满足规则
                 ├─ Broker 上无对应消息 → 查连接日志、mtyp、生成函数
                 └─ Broker 上有对应消息
                      ├─ 下游/云端异常 → 查订阅转发进程和远端链路
                      └─ 内容不符 → 查字段值、时间戳和业务预期
```

### 10.1 固定现场证据与确定正在运行的程序

先在设备上记录故障开始时间、是否重启、CAN0/1/2 哪个通道、缺失的是哪个 Topic 或字段、故障前后配置是否变更。`can_client` 在 `main.c` 中不解析命令行参数，因此不要期待通过进程参数判断设备型号；以实际二进制位置、固件构建记录与同目录配置为准。

```sh
date
pidof can_client
# 用上一步得到的真实 PID 替换 1234
readlink -f /proc/1234/exe
ls -l /proc/1234/task
ls -l /proc/1234/fd
```

`/proc/<PID>/task` 能看到线程 ID，但仅有线程并不能证明其仍在正常收帧/发报。`/proc/<PID>/exe` 给出正在运行的二进制；结合它的目录查 `libscene.so`、`Pointsheet_info1.json` 和默认路径所需的 `GeneralParam.json`，避免检查错版本。`pidof` 无结果时转到 10.2。日志要同时查程序的标准输出/错误输出（有 `printf`、`fprintf`）与 `LOG_*` 所用平台日志；只有确认启动管理器是 systemd 时，才用 `journalctl -u <实际服务名>`。源码没有规定统一的服务名或日志文件路径。

### 10.2 进程不存在、反复重启或启动即退出

按 `main.c:57-98` 的实际检查顺序核对：`can_mng_init()` → `/opt/conf_ext2.ini` → （默认路径）`GeneralParam.json` → `Pointsheet_info1.json` → CPActive 登记。以下为只读示例；`/实际程序目录` 要用上一节读出的路径替换。

```sh
ls -l /opt/conf_ext2.ini
ls -l /实际程序目录/can_client /实际程序目录/libscene.so
ls -l /实际程序目录/GeneralParam.json /实际程序目录/Pointsheet_info1.json
file /实际程序目录/can_client /实际程序目录/libscene.so
```

| 日志或现象 | 代码位置 | 下一项检查 |
| --- | --- | --- |
| `can_mng_init error` | `main.c:57-61`、`can_mng.c:98-108` | 此返回值由上下文或数据处理器分配失败触发。`readlink`/`dlopen` 失败有各自输出，但当前不会导致这条日志。 |
| `get device config error` | `main.c:63-67` | `/opt/conf_ext2.ini` 是否存在可读；`mgw:local_ip/local_port` 缺键有默认值，**整个文件缺失才直接失败**。 |
| `load generalparam error` | `main.c:77-83` | 仅默认路径出现；检查同目录文件是否为空、是否为有效 JSON。 |
| `load pointsheet error` | `main.c:86-92` | 两条路径都可能出现；检查同目录点表是否存在、非空、能解析。 |
| `failed to add process info to CPActive` | `main.c:94-98` | 排查进程管理接口/目标设备环境，不要将其当作 CAN 或 MQTT 错误。 |
| 找不到程序或加载动态库失败 | `CMakeLists.txt:73-79`、`scene.c:13-41` | 核对目标平台二进制、库依赖与实际安装目录。 |

若目标设备有 `jq`，可先做只读 JSON 语法和关键字段检查：

```sh
jq empty /实际程序目录/GeneralParam.json
jq empty /实际程序目录/Pointsheet_info1.json
jq '{mver, sver, pver, mtyp, sce, slots: [.dev[]?.slot]}' /实际程序目录/Pointsheet_info1.json
```

`jq empty` 通过只证明 JSON 语法正确，不证明点表字段适合这台设备；`GeneralParam.json` 在 `NOT_USED_SCHEMA=ON` 时可不装。源码中 `load_libscene()` 返回值未检查，若启动后遇到 CAN 帧才崩溃，应转查 10.7。实际编译选项需要从构建记录/CMakeCache 核实；不能仅凭运行日志里的 `schema not used` 推断全部宏，尤其不能由目录名推断启用了 `USE_JINKE`。

### 10.3 进程在运行，但没有 CAN 数据或数据不完整

先确认上游 CAN 发布进程及本机端口存在；`can_client` 对 `16002/16003/16004` 建立 nanomsg SUB 连接，不直接读 `can0` 网卡。`ss` 在设备可用时可观察端口与 TCP 连接，但连接存在只证明传输层状态，不能证明消息结构正确。

```sh
ss -ltn
ss -tnp
```

依序对照 `can_server.c:190-225` 与 `can_msg_process():144-187`：日志是否出现 `connected to can0/1/2 port`，是否有 `nn_socket`、`nn_setsockopt`、`nn_connect`、`nn_recv` 错误；上游是否确实往三端口发布；每条消息是否按当前 `can_frame_t` 结构发送，长度是否至少一个完整结构；是否发到点表 `dev[].slot` 指定的通道。`can_rcv_cnt[i]` 虽在源码中累加，但本目录没有向外暴露读取接口，不能在设备命令行直接读取它；需要日志或受控调试手段确认实际收帧。

若“某个 CAN ID 有数据、对应字段始终没有值”，按 `data_process.c:676-685` 看该通道哈希表是否存在完全相等的 CAN ID；再看点表位偏移是相对于整个 `can_frame_t`，数据字节从位偏移 64 起。`can_dlc` 没有用来保护字段访问，错误点表可导致错误解码甚至越界。`rls` 子规则虽读取 `val`，当前字段提取函数并未二次比较它。不要把上游 CAN 总线上有报文，直接等同于本进程成功解析出这个字段。

### 10.4 CAN 数据已到，但场景、字段或上报频率错误

分别检查**解码值**和**上报条件**。场景库只缓存 `0x171`、`0x172` 两种帧中的转速/速度，并据此返回 `power_on/idle/work/drive`；其他帧也会用此前缓存值再判断一次。若实际设备没有这两个 ID 或其字段布局不同，程序可能长期保持初始场景 `power_on`。点表 `sce` 必须与场景名称一致。旧路径的数字 `device_standard_work_state` 与这套场景不是同一判断。依据：`scene_judge.c:37-97`。

字段不上报时逐项确认：① 当前场景在字段 `sce` 中；② 同位置的 `scems` 是期望间隔（毫秒）；③ 此字段至少收过一次消息（`time_rcv != 0`）；④ `udif` 为 0，或本次变化达到阈值/字符串不同；⑤ 点表 `mtyp` 是实际订阅的格式。第一次满足条件会通过，后续受到 `udif` 限制。需要对某字段做数值核算时，记录原始 CAN 帧与点表 `ofs/len/bo/sign/sc/pofs/min/max`，按源码 `raw × sc + pofs` 和限幅重算；若高位字节涉及 `0x80`～`0xFF`，同时核查第 9 节指出的 `char` 符号扩展风险。依据：`data_process.c:445-568,852-1074`。

`json msg:`、`jsongz msg:`、`cbor msg:` 是生成载荷的标准输出线索，但在服务管理器未收集 stdout 时可能看不到；看不到这些行不能单独证明没有生成数据。`time_pub` 在载荷生成时更新，不是 MQTT 交付时间。不同字段按场景与间隔可能在不同批次发布，不能只看一条消息就判定某字段永久缺失。

### 10.5 有字段，但 Broker 上没有对应 MQTT 消息

先从 `/opt/conf_ext2.ini` 的 `[mgw]` 取得**实际** `local_ip/local_port`；若仅键缺失，代码默认 `127.0.0.1:1883`。`local_ip` 存在 20 字节数组中，`snprintf()` 最多保存 19 个可见字符；配置过长会被截断（`can_mng.c:15-25`）。查看 `begin mqtt connect`、`mqtt connected`、`client connect error`、`mqtt disconnect error`、`mosquitto_publish` 等日志，并检查是否有其他实例使用同一 MQTT Client ID。连接成功后默认路径先发布一次 Schema；这一步的 publish API 一直未返回成功时，工作数据循环不会开始（`client_mosquitto.c:164-180`）。

设备有 `mosquitto_sub` 且目标 Broker 允许订阅时，可以用**新的观察客户端 ID**订阅对应主题；不要使用 `can_client` 作为观察客户端 ID。以下命令只观察普通 JSON 主题，避免将 CBOR 二进制直接输出到终端。若 Broker 要求认证，需使用设备实际只读账号与配置；本文没有这类凭据。

```sh
command -v mosquitto_sub
mosquitto_sub -h 127.0.0.1 -p 1883 -t 'local/data/general_data' -v
mosquitto_sub -h 127.0.0.1 -p 1883 -t 'local/data/gz_general_data' -v
```

命令中的地址端口只是**默认值示例**，要换成设备 INI 的实际值；观察够一个完整 `scems` 间隔后主动结束。`mtyp=0/1/2` 分别对应普通 JSON、CBOR、`JSONGZ` JSON，订错 Topic 会造成“没有消息”的假象。`gz_general_data` 内容按当前源码仍是未压缩 JSON；若下游报 gzip 解压错误，应检查下游是否按主题名误判格式。`mosquitto_publish()` 成功只说明客户端库接收请求；Broker 观察到消息，也只证明某个发布者发到了该 Broker。依据：`client_mosquitto.c:182-231`、`data_process.c:1106-1277`。

### 10.6 Broker 有消息，但平台没有或内容不对

先保存同一时间窗内的 MQTT Topic、载荷、设备时间、字段名/值，以及下游接收进程日志，再判断边界。Broker 上看到 `local/data/general_data`、`gz_general_data` 或 `cbor_general_data` 只证明**有发布者**发到了 Broker；SDK 中其他程序也使用同名 Topic，须结合本进程生成日志、载荷和时间确认是否由该 `can_client` 发出。确认后再核查实际部署的订阅与转发进程、它是否订阅该 Topic、是否能解码对应格式、远端连接和平台回执。SDK 中存在 `cloud_client`、`cloud_client_newc` 等程序源码，它们订阅相关主题，但不能据仓库存在就断言目标设备运行了哪一个。依据：`apps/cloud_client/src/mosquitto/client_mosquitto.c:222-225`、`apps/cloud_client_newc/src/mosquitto/client_mosquitto.c:232-237`（相对于 SDK 仓库根目录）。

内容不对时按阶段分辨：CAN 原始帧是否正确 → `can_frame_t` 布局/点表偏移是否匹配 → 场景是否正确 → 缩放/符号/上下限是否正确 → 编码格式是否对应 `mtyp`。JSON 中设备 `id` 当前写空字符串、`ts` 取进程 `CLOCK_REALTIME`；CBOR 中也写入时间戳与空设备 ID。若平台要求非空设备 ID 或可靠时钟，需要结合下游补全规则确认，不能直接把空字符串理解成配置已设置。依据：`data_process.c:1106-1251`。

### 10.7 崩溃、数据偶发异常或线程已停止

收 CAN 后崩溃优先查 `libscene.so` 是否位于**运行中二进制**的目录，并确认其导出 `judge_scene`；`load_libscene()` 失败不阻止主程序继续，后续 `judge_scene(&frame)` 会使用指针。其次检查点表 `slot` 是否为 0～2，`sce/scems` 数组是否不超过 10，位宽和字符串长度是否落在帧与本地缓冲区内。依据：`scene.c:13-41`、`can_server.c:168-174`、`data_process.h:9,67-69,104`、`data_process.c:445-568,807-827`。

如果进程存在但长期不产出数据，要分别核查 CAN、MQTT、可选 GPS/MCU 线程。`main()` 没逐项检查 `pthread_create()` 返回值，末尾只 `pthread_join()` 最后覆盖的 `tid`；某工作线程退出时进程仍可能存在。日志中 CAN `nn_poll`/`nn_recv` 错误会令 CAN 线程返回；MQTT 客户端网络循环启动失败会反复打印错误。`/proc/<PID>/task` 的线程数变化只能作为线索，无法单独映射到具体线程。依据：`main.c:100-111`、`can_server.c:154-187`、`client_mosquitto.c:151-158`。如设备保留 core dump，先按平台方式保存 core、程序与 `libscene.so` 的精确版本，再用匹配的符号文件定位；不要先替换配置或库而丢失复现证据。

### 10.8 金科事件、GPS/MCU 或统计不正确

先确认构建时**两个**宏都开启；默认 CMake 选项下这些线程不启动。GPS 走 16005 的 nanomsg SUB，MCU 走 38000 的 REQ；应用字段只在点表 `app[].nm="can_client"` 且包含对应字段时才有容器可写。旧路径 `app_msg_task()` 每 5 秒请求 MCU，GPS 数据由上游主动发布。状态数字与场景字符串分属两套逻辑，不应用一种状态解释另一种。依据：`main.c:103-109`、`app_server.c:160-217`、`gps.c:10-35`、`mcu.c:10-31`、`data_process.c:136-257`。

钻孔事件排查顺序：收到 `0x215` → 工单号是否相对 `last_order_number` 变化 → 是否入队 → MQTT 是否为已连接状态 → `local/data/event` 是否出现消息。环形队列最多容纳 9 个事件，满队列时入队失败；`/media/sdcard/jk_evt_no` 保存的是日期与上一工单号，文件丢失、无写权限或日期变化会影响重复判定。事件出队后先释放对象再发布 QoS 0，没有失败重入队。依据：`can_server.c:20-52,76-100`、`queue/queue.h:4-13`、`queue/queue.c:24-60`、`event/jk_event.c:8-98`、`client_mosquitto.c:233-240`。

FEI4 统计每约 300 秒才执行一次“输出上次样本 → 清零 → 采当前样本”的调用序列，第一次周期不具备完整此前样本。若发现统计值与“300 秒内所有帧的平均值”不符，先不要按连续采样模型解释它；对照 `fei4.c:206-227` 和实际 CAN 输入逐项核算。

### 10.9 故障归属的最小证据

| 已确认事实 | 可支持的判断 | 仍不能证明 |
| --- | --- | --- |
| 进程在、心跳在 | 入口与保活线程至少执行过。 | CAN 和 MQTT 线程都健康。 |
| 本机 nanomsg 端口可连接 | 有传输端点。 | 帧结构、CAN ID、点表字段正确。 |
| 本程序日志打印 `json msg`/`cbor msg` | 生成函数产生过载荷。 | Broker 或下游接收成功。 |
| 独立观察客户端收到正确 Topic | Broker 已收到某个发布者的一次消息。 | 一定是本进程发布，或云端/业务平台确认接收。 |
| 事件文件中工单号更新 | 代码至少执行过写文件路径。 | 事件 MQTT 发布成功。 |

复现记录建议保留：设备/固件/分支与构建宏、正在运行的二进制路径及版本、三份配置文件的校验和、故障前后时间窗口的 CAN ID/原始帧、相关 Topic 及载荷、程序和下游日志、期望值与实际值。配置文件可能包含设备信息，导出前按现场要求处理；排查时优先保留原文件，以便复现具体点表规则。这样才能把“本程序无数据”“本机发布成功但下游异常”“字段被条件过滤”区分开。

<a id="analysis-12"></a>

## 11. 验证状态与尚需外部资料

已完成的是对当前提交的静态源码、条件编译、调用链、配置读取、Topic 和流程图的核对；未进行编译、单元测试、目标板运行、CAN 注入或 MQTT Broker 联调。仓库未提供本目录专用金科点表，所以无法严谨列出具体车型全部点位与量纲；没有设备部署脚本与下游消费者约定，也无法把 MQTT Topic 的发布结果等同于云端接收。若将来取得金科 JSON 点表、构建参数和设备日志，可在本文件上述流程上补充“具体字段与实测结果”，而不需要改变已由源码证明的分支关系。
