# filter_sep 项目说明（/init 参考）

> 依据本地 `skes` 源码和构建配置整理；复核日期：2026-09-28。本文描述检查时的工作区，切换分支或修改代码后应重新核对。

## 项目位置与用途

| 项目 | 说明 |
| --- | --- |
| Git 仓库 | `/home/tronlong/lyp/code/skes`；`filter_sep` 是仓库内 `skes-apps/` 下的一个应用，不是独立 Git 仓库。 |
| 源码目录 | `/home/tronlong/lyp/code/skes/skes-apps/filter_sep`。上级 `skes-apps/CMakeLists.txt` 当前通过 `add_subdirectory(filter_sep)` 纳入构建。 |
| 文档目录 | `/home/tronlong/lyp/perCode/idevelop_md/skes/filter-sep`；此目录在源码仓库之外。目录名使用连字符，源码与可执行文件名使用下划线。 |
| Git 分支 | 检查时为 `main_ui`，跟踪 `origin/main_ui`，HEAD 为 `b28d0b8`（检查时与远端跟踪分支一致）。该分支属于整个 `skes` 仓库；`filter_sep` 没有自己的独立分支。`filter_sep` 文件相对先前记录的 `6bc22ef` 没有提交差异。`origin` 是远端仓库名，HEAD 指当前检出的提交；后续以 `git branch --show-current` 和 `git rev-parse --short HEAD` 为准。 |
| 程序与安装 | CMake 项目和可执行文件名均为 `filter_sep`；安装规则将程序及 `sep.ini` 放入安装前缀下的 `apps/filter_sep/`。`src/main.cpp` 中的应用版本字符串为 `V0.0.2`，它与 Git 分支、提交号是不同概念。 |
| 主要用途 | 从本机三个 CAN 帧发布端接收车辆数据，解析档位、车速及踏板相关字段，通过 SKES 的 `/skes/filter` 主题发布属性表，并响应属性查询。这里的 `filter` 是项目与消息主题的名称；当前代码主要做字段提取、换算和变化时上报，不能仅凭名称理解为通用数字滤波器。 |

SKES（Sky-Eyes System，天眼系统）是上级车载系统。SEP（SKES Endpoint）是其中承担独立业务的进程；本程序作为 SEP 使用 `skes-linker` 与 `skes-relay` 交换消息。`skes-relay` 是系统内的消息转发进程。上级目录中的 `filter_sep-2`、`filter_sep_gj` 是其他变体，本文仅针对 `filter_sep`；当前 `skes-apps/CMakeLists.txt` 中 `filter_sep-2` 被注释。

## 核心数据流

```text
本机 CAN 帧发布端（nanomsg PUB）
  → tcp://127.0.0.1:16002、16003、16004
  → src/can_server/can_server.cpp（SUB、解析字段）
  → filter_mng_t.curr_dt
  → src/filter_pub/filter_pubsub.cpp（字段变化时组装 JSON）
  → skes-linker → 本机 skes-relay → /skes/filter 订阅者

/skes/filter 属性查询 → skes-linker 回调 → 读取当前值 → /skes/filter 查询响应
```

`src/main.cpp` 初始化 SKES 日志、创建并运行 `filter_pubsub`，随后启动 CAN 接收线程。主线程每 200 毫秒查看运行标志。`filter_pubsub` 内另有消息接收线程和周期发布线程；发布线程每 200 毫秒检查一次数据，只有已有有效数据且结构体内容与上次不同才发送一份 `filter_table`。这个间隔是检查周期，不保证每 200 毫秒一定发消息。

## 代码导航

| 路径 | 主要职责 |
| --- | --- |
| `src/main.cpp`、`src/main.h` | 进程入口、线程启动，以及 `filter_data_t`、`filter_mng_t` 数据结构。 |
| `src/can_server/can_server.h`、`.cpp` | CAN 帧内存结构、三个本机订阅端口、位提取宏及 CAN ID 字段解析。 |
| `src/filter_pub/filter_pubsub.h`、`.cpp` | SKES 链接初始化、消息回调、属性查询响应、变化上报及主题配置。 |
| `src/log/log_macro.h` | 日志宏。 |
| `sep.ini` | SEP 启动配置，当前为 `enabled=true`、`name=filter_sep`、`param=`（空启动参数）。 |
| `CMakeLists.txt` | 编译源文件、依赖、链接库和安装规则。 |
| `tests/test.cpp` | `EXTRACT_BITS` 等位提取宏的 GoogleTest 测试。 |
| `tests/test_request.cpp` | 每 5 秒发送一次 `/skes/filter` 查询的手工联调程序。 |

## CAN 输入与属性字段

接收端订阅 `tcp://127.0.0.1:16002`、`16003`、`16004`，分别作为代码里的 CAN0、CAN1、CAN2 来源。这里是本机 **TCP 上的 nanomsg 消息**，不是程序直接打开 Linux `can0` 设备。每条消息按 `can_frame_t` 大小拆成若干帧；帧含 `can_id`、`can_dlc`、`rsv_ms`、8 字节 `data`、`timestamp`。不足一帧的尾部字节不会解析，当前接收代码没有检查 `can_dlc` 是否超出 8。代码在匹配前清除 CAN ID 的最高位 `0x80000000`。具体 CAN 帧来自哪个硬件通道，应结合现场发布端配置确认。

| CAN ID | 解析内容 | 输出属性 |
| --- | --- | --- |
| `0x16D61B96` | 第 0 字节映射档位：代码意图将 `0x7A`/`0x7B`/`0x7C` 记为倒档 `0`，`0x7E`/`0x7F`/`0x80`/`0x81` 记为前进档 `2`，其他记为中档 `1`。第 48 位起的 16 位整数乘 `0.00390625`，再存入无符号整型。 | `CurrentGear`、`VehSpeed`；该帧也把 `valid_dt` 设为真。 |
| `0x1AD18096` | 第 32 位起 16 位整数乘 `0.001`。 | `VCU_IP_HCT1_Drv_ACC_Ped`。 |
| `0x1AD19696` | 第 0 位起 16 位整数，不另乘比例。 | `VCU_IP_HCT1_Drv_BRK_Ped`。 |
| `0x1ED6F396` | 第 32 位起 8 位整数乘 `0.4`。 | `HCT1_Drv_BRK_Ped`。 |

这些换算系数与档位编码来自当前源码；`VehSpeed` 的物理单位、踏板字段的工程单位不能仅靠这里的实现确定，联调时需核对 CAN 协议定义。车速转成整数时小数部分被舍去。档位中 `0x80`、`0x81` 与普通 `char` 变量比较；若目标编译器的 `char` 默认为有符号，这两个值会落入“其他”分支，不能把表中的代码意图直接当作已验证的运行结果。`0x18001701` 的另一套解析代码位于 `#if 0`，当前不参与编译。`CAN0_SKT_TX_PORT` 定义为 `26002`，但本模块当前接收路径未使用它。

三个 CAN 地址在 `start_can_server()` 中逐个连接；任意一个连接失败，该接收线程便返回。相邻 `skes-dataengine/can/can_receiver.cc` 虽列有三个相同地址，当前默认通道数为 2，因此不能仅凭地址列表断定现场一定有 `16004` 的发布端。排查“无属性上报”时，应核对三路发布端是否实际启动、各端口是否可连接，以及运行时通道配置。

## `/skes/filter` 消息

`skes-linker` 的发布连接使用 `tcp://127.0.0.1:19225`，订阅连接使用 `tcp://127.0.0.1:19226`；发送主题为 `/skes/filter`。这两个端口是本机 SKES 消息通道，和前面的三个 CAN 帧端口用途不同。消息体使用 JSON，由 `cJSON` 创建和解析。

- **主动上报**：外层 `action="post"`、`type="properties"`；`data.type="filter_table"`；`data.data` 为属性数组。每项带 `index=-1`、`property_name`、`data_type`、`data`。当前一次上报包含上表五个属性；`ts` 为墙上时钟的毫秒时间戳。
- **查询响应**：只处理外层 `action="request"`、`type="properties"` 且 `data.type="query"` 的消息。至少有一个受支持的 `property_name` 时，复制请求 JSON，将 `action` 改为 `response`，在识别的数组项增加 `ts`、`value`、`result`，再发回 `/skes/filter`；不认识的属性不会被填值。请求项已有字符串类型的 `data_type` 时会改为对应类型；缺少该键时不会新建，若该键不是字符串则改写失败。`result=0` 表示已有有效数据，`-1` 表示尚未取得有效数据，后者的 `value` 不可靠。请求里的 `msg_id` 随复制后的响应保留，便于调用方关联请求。
- **字段类型**：`CurrentGear` 为 `int8`，`VehSpeed` 为 `int16`，三个踏板字段为 `double`。这是 JSON 的 `data_type` 标记；实际 `filter_data_t.VehSpeed` 在 C++ 中是 `unsigned int`，消费者仍应按协议约定处理范围。

例如，主动上报的结构为（数值只是展示格式）：

```json
{
  "version": "", "id": "", "ts": 1780000000000,
  "action": "post", "type": "properties",
  "data": {"type": "filter_table", "data": [
    {"index": -1, "property_name": "CurrentGear", "data_type": "int8", "data": 2},
    {"index": -1, "property_name": "VehSpeed", "data_type": "int16", "data": 10},
    {"index": -1, "property_name": "VCU_IP_HCT1_Drv_ACC_Ped", "data_type": "double", "data": 0.5},
    {"index": -1, "property_name": "VCU_IP_HCT1_Drv_BRK_Ped", "data_type": "double", "data": 0},
    {"index": -1, "property_name": "HCT1_Drv_BRK_Ped", "data_type": "double", "data": 0}
  ]}
}
```

实际 `ts` 使用运行时墙上时钟，`data` 数值来自收到的 CAN 帧。`tests/test_request.cpp` 提供四个属性的查询报文示例；它使用旧的请求 `data_type="boolean"`，响应处理代码会把已识别属性改成当前类型，因此不能把测试请求的类型当作输出字段类型。

### 与其他模块的关系

`skes-relay` 转发 `/skes/filter` 消息；`sep-aeb/sep-aeb.c` 既能处理 `filter_table` 上报，也能发起 `query` 并处理响应。当前工作区中的 `skes-dataengine` 也使用同名主题发布状态；订阅方需要同时检查外层 `action` 和内层 `data.type`，不能只按主题名判断消息来自哪个业务。这里仅说明代码中的交互路径，不推断设备上这些程序是否同时启用。

## 构建、运行和测试

- 本项目由 `skes` 顶层 CMake 构建；自身 `CMakeLists.txt` 声明 C++11，编译 `src/main.cpp`、`src/can_server/can_server.cpp`、`src/filter_pub/filter_pubsub.cpp`。直接链接 `cjson`、`skes-linker`、`skes-utils`、`skes-log`、`nanomsg`、`pthread`、`z`。CMake 还查找 e2fsprogs、OpenSSL、curl、collections-c 等包；查找包不代表该模块直接调用每个库。
- `sep.ini` 随可执行文件安装到 `apps/filter_sep/`；运行依赖 SKES relay、本机 CAN 帧发布端及对应共享库。安装 RPATH 使用 `$ORIGIN` 相对路径寻找库，`$ORIGIN` 指可执行文件所在目录。
- `tests/CMakeLists.txt` 定义 `FilterUnitTest` 和 `FilterRequestTest`；前者是位提取测试，后者是持续发送查询的联调程序。构建测试目标需要 GoogleTest 等依赖。仓库顶层调用 `enable_testing()`，该文件通过 `add_test()` 注册 `FilterUnitTest`，但没有注册 `FilterRequestTest`；后者会持续发消息，应在联调环境中手动运行并主动结束。本文未运行构建、测试或设备端联调。

已有完整构建目录且依赖就绪时，可在仓库根目录执行 `cmake --build build --target filter_sep FilterUnitTest`；执行 `ctest --test-dir build -R '^FilterUnitTest$' --output-on-failure` 前，应确认当前构建目录对应本工作区的配置。目标设备上的运行验证还需要三路 CAN 发布端和 SKES relay。

常用只读定位命令（在源码目录执行）：

```bash
git branch --show-current
git rev-parse --short HEAD
rg -n 'CAN[012]_SKT_PORT|case 0x|FILTER_PUB_TOPIC|FILTER_PUB_INTERVAL_US' src
rg -n 'add_executable|target_link_libraries|install\(' CMakeLists.txt tests/CMakeLists.txt
```

## 名词解释

| 名词 | 含义及在本项目中的作用 |
| --- | --- |
| CAN / CAN ID / CAN 帧 | CAN 是车辆控制网络；CAN ID 标识报文类型。这里接收的是本机发布端转发的 CAN 帧结构，再按 ID 解析信号。 |
| VCU / ACC / BRK / HCT1 | VCU 通常指车辆控制单元，ACC、BRK 在字段名中分别指加速、制动踏板；`HCT1` 是原始信号名的一部分，具体设备定义应以 CAN 协议表为准。 |
| 档位与车速 | `CurrentGear` 为源码映射后的倒档/中档/前进档编码；`VehSpeed` 为按系数换算并转成整数的车速字段，当前代码没有额外平滑滤波。 |
| 位提取与比例系数 | `EXTRACT_BITS` 按偏移和长度从 CAN 负载中取位段；乘数把原始整数换算为业务数值。宏按宿主机内存字节序解释数据，文档列出的换算以当前代码路径为准；测试覆盖部分提取情况。 |
| nanomsg / PUB-SUB | nanomsg 是进程间通信库；发布端发送，订阅端接收。本模块既订阅 CAN 帧，也通过 `skes-linker` 接收和发布 SKES 消息。 |
| skes-linker / skes-relay | 前者封装 SKES 消息协议和主题收发；后者在本机转发各 SEP 的消息。`/skes/filter` 是主题名，不是文件路径。 |
| `filter_table` / `query` | 前者是变化上报的 `data.type`，后者是按属性名请求当前值的 `data.type`。外层 `action` 分别是 `post`、`request` 或 `response`。 |
| cJSON / JSON | cJSON 是 C 语言 JSON 库；JSON 是本模块属性消息的文本格式。 |
| CMake / RPATH / `$ORIGIN` | CMake 定义编译和安装；RPATH 告诉程序运行时在哪里寻找动态库；`$ORIGIN` 代表程序或动态库自身所在目录。 |
| Git 分支 / HEAD / origin | 分支标识代码开发线；HEAD 标识当前提交；`origin` 是常见远端名。本文记录的是检查时的状态，不保证之后仍相同。 |

## 读代码时的注意点

- `0x1AD18096` 分支在设置加速踏板后没有 `break`，会继续执行 `0x1AD19696` 分支并改写制动踏板值。若排查踏板数据异常，应先确认这一处是否符合设计。
- `parse_filter_data()` 只把 8 字节负载写入 16 字节 `tmpbuf` 的前半部分，而 `EXTRACT_BITS` 在偏移 32/48 时固定复制后续 8 字节，会读到数组内未初始化的尾部。这个问题不同于 `EXTRACT_BITS_BIG_ENDIAN` 测试中的数组越界；详见同目录 `filter_sep_源码分析.md` 第 4 节。
- `valid_dt` 只在 `0x16D61B96` 分支设为真；仅收到其他 CAN ID 时，主动上报不会开始。查询路径在尚无有效数据时仍会从未初始化的临时结构读取字段、填入 `value` 和 `result=-1`，此时 `value` 不可靠。
- 查询响应通过追加方式写 `ts/value/result`；若请求项已含同名键，响应可能出现重复键。联调时应核对原始 JSON，详见同目录 `filter_sep_源码分析.md` 第 6.3 节。
- CAN 接收线程写 `curr_dt` 时没有持有 `filter_mng.mutex`，但发布和查询线程读取时使用了该锁。当前实现并未构成完整的读写同步；并发行为和启动顺序需要在修改时重点检查。
- `running` 是多个线程共享的普通 `bool`，没有原子或锁保护；`main()` 也没有检查互斥锁初始化结果。停止逻辑与启动失败路径应按线程生命周期一起核对。
- `filter_pubsub_run()` 启动发布线程后，`main()` 才清空 `filter_mng` 并初始化互斥锁。文档记录的是当前代码顺序，不能据此假定启动时数据总是有效。
- `filter_pubsub_stop()` 会设置停止标志并等待消息线程、发布线程结束，但 `main()` 没有调用它；消息线程的等待可能需要主动唤醒。入口退出路径只调用 `filter_pubsub_release()`，CAN 接收线程被设为 detached。若修改退出逻辑，需一并核对线程生命周期与资源释放。

本文为源码导览，未验证目标设备的 CAN 协议、运行配置或实际消息吞吐。
