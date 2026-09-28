# filter_sep 源码全面分析

> 复核日期：2026-09-28；`skes` 仓库 `main_ui` 分支，当前 HEAD `b28d0b8`（检查时与 `origin/main_ui` 一致）。源码目录为 `/home/tronlong/lyp/code/skes/skes-apps/filter_sep`。已核对 `filter_sep` 文件相对原基线 `6bc22ef` 没有提交差异；上级 `skes-apps/CMakeLists.txt` 在工作区有未提交修改，相关构建描述以本次检查的文件为准。目标设备上的 CAN 单位、实际端口提供者和运行结果，需要协议表及设备联调确认。

## 1. 分析范围与结论边界

本模块的编译入口是 `src/main.cpp`、`src/can_server/can_server.cpp`、`src/filter_pub/filter_pubsub.cpp`，由本目录 `CMakeLists.txt:58-66` 明确列出。本文同时核对 `main.h`、`can_server.h`、`filter_pubsub.h`、`sep.ini`、`tests/`，以及消息封装和排障涉及的 `skes-core/skes-utils/utils.c`、`skes-3rd-party/skes-linker-git/`、`skes-3rd-party/skes-log/`、`skes-scripts/skes-sepd.sh` 中相关代码。关联模块仅用于确认接口方向，不把它们的业务实现归为本模块。

**核心结论：**此进程从三个本机 nanomsg CAN 帧订阅地址读取二进制帧，按四个 CAN ID 更新五个属性；首次收到 `0x16D61B96` 后，发布线程才允许把属性变化作为 `/skes/filter` 的 `filter_table` 消息发出。它也接受同主题的 `properties/query` 请求并返回已识别属性的当前值。源码没有实现滑动平均、低通等通用数值滤波算法；这里的 `filter` 主要是程序名及 SKES 主题名。证据见 `src/can_server/can_server.cpp:13-60`、`src/filter_pub/filter_pubsub.cpp:42-214,275-420`。

下文的“会调用”“会赋值”表示代码路径；“可能导致”表示由这些代码推导出的风险，不等于已经在目标板复现。没有给出 CAN 协议表的物理单位或现场网络拓扑时，本文不推断它们。

## 2. 文件、构建与运行位置

| 文件 | 可核对的作用 |
| --- | --- |
| `src/main.cpp`、`src/main.h` | 进程入口、线程启动；定义五个属性的结构体、有效标志、上次上报副本与互斥锁。 |
| `src/can_server/can_server.h`、`.cpp` | 三路接收端口、CAN 帧内存结构、位提取宏、CAN 消息轮询与四个 ID 的字段解析。 |
| `src/filter_pub/filter_pubsub.h`、`.cpp` | SKES 消息句柄、回调与查询应答、变化上报线程、停止和释放函数。 |
| `src/log/log_macro.h` | `LOG_*` 是写入系统 syslog 的宏；入口另调用 `skes-log` 设置该库的日志级别，两者不是同一套宏。 |
| `sep.ini` | `[sep]` 下 `enabled=true`、`name=filter_sep`、`param=`；没有启动参数。 |
| `tests/test.cpp`、`tests/test_request.cpp` | 位提取 GoogleTest 用例；循环发送查询消息的联调程序。 |

上级 `skes-apps/CMakeLists.txt:27` 启用 `add_subdirectory(filter_sep)`；`filter_sep-2` 在同文件第 29 行被注释。模块 CMake 要求 C++11，实际链接 `cjson`、`skes-linker`、`skes-utils`、`skes-log`、`nanomsg`、`pthread`、`z`。它还查找 e2fsprogs、OpenSSL、curl、collections-c 等包；被 `find_package` 查找不等于在本模块源码中直接调用。安装规则把可执行文件与 `sep.ini` 放在 `apps/filter_sep/`，并复制 cJSON、nanomsg 动态库到 `lib/`；相对动态库路径由安装 RPATH 中的 `$ORIGIN` 项定义。程序本身不读取 `sep.ini`；仓库的 `skes-scripts/skes-sepd.sh:119-162` 会读取其中的 `name/cmd/param/enabled`，按配置启动及巡检 SEP 进程。源码 `APP_VERSION` 为 `V0.0.2`，是程序内版本字符串，不是 Git 提交号。证据：`CMakeLists.txt:1-83`、`src/main.cpp:28`。

此项目虽设置 C++11，位提取宏使用 GNU 语句表达式 `({ ... })`，CAN 连接代码使用 `asprintf()`，CMake 也显式定义 `_GNU_SOURCE`；它依赖 GNU 环境扩展，不应仅凭“C++11”假定可用任意标准 C++ 编译器直接编译。证据：`can_server.h:15-44`、`can_server.cpp:123`、`CMakeLists.txt:10`。

## 3. 完整逻辑流程图（普通 Markdown 可显示）

以下全部采用等宽文本图，普通 Markdown 查看器只需支持代码块；本节后另有逐步文字版流程。

### 3.1 启动、线程和退出

```text
main()
  |
  +--> 通过 syslog LOG_D 记录编译时间/版本；设置 skes-log DEBUG 级别
  |
  +--> filter_pubsub_new()
  |      |
  |      +--> malloc 状态对象
  |      +--> skes_linker_init(): 连接 SUB 19226 / PUB 19225
  |      +--> 失败：main 返回 -1
  |
  +--> filter_pubsub_run()
  |      |
  |      +--> 启动 SKES 接收线程 do_sre_msg_poller
  |      +--> 启动变化上报线程 do_sre_filter_pub
  |      +--> 失败：main 返回 -1
  |
  +--> 清零 filter_mng，初始化 mutex
  +--> 启动并 detach CAN 接收线程 start_can_server
  |
  +--> 每 200 ms 读取 sre->running
         |
         +--> true：继续循环
         +--> false：filter_pubsub_release()，main 返回 0
```

图中顺序严格对应 `src/main.cpp:36-67`，因此上报线程可能在 `filter_mng` 初始化前执行；CAN 线程创建结果未检查。`filter_pubsub_stop()` 虽有定义，入口没有调用它，正常主循环也没有主动把 `running` 置为 false。这里的“退出”只描述源码可见的分支，不表示已有可靠的优雅退出路径。

### 3.2 CAN 接收、解析与上报

```text
CAN 接收线程
  |
  +--> 依次创建三个 NN_SUB；订阅全部消息；nn_connect(16002/16003/16004)
  |      +--> 任一步返回错误：线程结束
  |
  +--> nn_poll(三个 socket，超时 2000 ms)
         |
         +--> 超时：继续轮询
         +--> 错误：退出接收函数与线程
         +--> 可读：nn_recv()
                |
                +--> 消息按 sizeof(can_frame_t) 分段，尾部不足一帧的字节不解析
                +--> 每帧清 CAN ID 最高位，再按 ID 更新 curr_dt
                       |
                       +--> 0x16D61B96：档位、车速，valid_dt=true
                       +--> 0x1AD18096：加速踏板；随后落入制动踏板分支
                       +--> 0x1AD19696：制动踏板
                       +--> 0x1ED6F396：另一制动踏板
                       +--> 其他：不更新

变化上报线程（每轮末尾休眠 200 ms）
  |
  +--> 复制 curr_dt
  +--> valid_dt 为 false？---- 是 --> 跳过本轮
  +--> 与 last_dt 整块相同？-- 是 --> 跳过本轮
  +--> 先把本次数据复制到 last_dt
  +--> 组装五项 properties/filter_table JSON
  +--> skes_send(..., "/skes/filter", ...)
  +--> 返回循环
```

接收分支来自 `src/can_server/can_server.cpp:62-140`，上报分支来自 `src/filter_pub/filter_pubsub.cpp:275-420`。图中的“连接”指 `nn_connect()` 调用返回成功，不保证当时已有远端发布者或收到了帧。

### 3.3 `/skes/filter` 查询分支

```text
SKES 接收线程 skes_run()
  -> 回调 on_skes_msg_received()
  -> 仅 filter 主题进入 on_msg_filter()
  -> 解析 JSON 和消息头
  -> 需要 action=request、外层 type=properties、内层 data.type=query
  -> 遍历 data.data[] 中的 property_name
       |
       +--> 至少一个属于五个已知字段？
              +--> 否：不发送响应
              +--> 是：在请求项上补 ts / value / result
                       -> 复制整个请求
                       -> 改 action=response
                       -> 向 /skes/filter 发送响应
```

对应 `src/filter_pub/filter_pubsub.cpp:42-231,250-273`。该函数末尾无论是否成功发送都返回 `-1`，上层回调也返回 `-1`。当前 `skes-linker` 的 `do_skes_recv_msg()` 调用回调后不使用其返回值，因此这个负值不会在该实现中直接改变接收函数的返回结果；见 `skes-3rd-party/skes-linker-git/skes-linker.c:501-527`。

### 3.4 文字版流程

1. 入口先通过 syslog 写调试级编译信息，再设置 `skes-log` 级别，初始化一对 SKES 句柄并启动两个线程：一个接收 SKES 主题消息，一个周期检查数据变化。调试日志是否出现在终端取决于系统日志配置，源码没有直接向标准输出打印它。
2. 入口随后才清零 `filter_mng`、初始化互斥锁、启动并分离 CAN 接收线程。CAN 线程依次连接三个本机地址，之后持续等待 CAN 帧消息。
3. 每条接收消息按本地 `can_frame_t` 结构尺寸切段，每帧清除 CAN ID 最高位，根据匹配 ID 更新最多五个内存字段。只有档位/车速帧会把 `valid_dt` 置为真。
4. 发布线程每轮复制当前字段；若尚无有效帧或整块数据与上次相同，就不发。否则先更新上次副本，再组装包含五个属性的 JSON，发到 `/skes/filter`，最后休眠 200 毫秒。
5. 收到 `/skes/filter` 查询时，回调检查动作、类型和属性名。至少命中一个已知属性才发送响应；响应保留请求其他字段，补充每项结果并将动作改成 `response`。
6. 主线程每 200 毫秒读运行标志。当前入口没有正常停止动作；CAN 接收线程遇到错误也不会把该标志置为 false。

## 4. 数据结构与二进制输入

`can_frame_t` 字段按定义顺序为：`uint32_t can_id`、`uint16_t can_dlc`、`uint16_t rsv_ms`、`uint8_t data[8]`、`uint32_t timestamp`（`src/can_server/can_server.h:46-53`）。源码注释称 `rsv_ms` 为 MCU 运行时间、`timestamp` 为 RTC 时间戳；本模块解析时只用 `can_id` 和 `data`，没有用这两个时间字段，也没有核验 `can_dlc <= 8`。按 `sizeof(can_frame_t)` 直接切分意味着收发双方要使用相容的结构布局、字节序和对齐方式；源码中没有独立的跨平台解码层。

`filter_data_t` 包含 `CurrentGear`（`unsigned char`）、`VehSpeed`（`unsigned int`）和三个 `double` 踏板字段。`filter_mng_t` 包含 `valid_dt`、当前数据 `curr_dt`、上次发布副本 `last_dt`、`pthread_mutex_t`（`src/main.h:13-28`）。`valid_dt` 是整体开关，没有每个字段的独立有效标志、来源帧时间或过期时间。

`EXTRACT_BITS(buffer, offset, width)` 从 `buffer + offset/8` 起固定 `memcpy` 8 字节到 `uint64_t`，右移 `offset%8` 位再按宽度掩码。活跃解析路径只把 8 字节 CAN 负载复制到 `char tmpbuf[16]` 的前 8 字节；例如偏移 48 的提取会读取 `tmpbuf[6..13]`，其中 `[8..13]` 未初始化。偏移 32 的提取也会读到未初始化的 `[8..11]`。这些读取没有越过 16 字节数组，但仍是读取未初始化内容；即使在小端机器上，高位最终被窄宽度掩码丢弃，也不能据此证明实现具有可移植、可靠的行为。位解释本身还依赖编译目标的内存字节序；测试给出的低字节在前的期望见 `tests/test.cpp:8-34`。`EXTRACT_BITS_BIG_ENDIAN` 只在测试中使用，活跃四个 CAN ID 分支均没有调用它。测试中的 8 字节数组与该大端宏某些偏移组合另有**越界读取**风险，应与前述“数组内未初始化读取”区分。证据：`can_server.cpp:17-21,34-55`、`can_server.h:15-44`、`tests/test.cpp:47-58`。

## 5. 四个 CAN ID 到五个属性的精确映射

当前编译走 `src/can_server/can_server.cpp:33-56` 的 `#else`；`#if 0` 中的 `0x18001701` 不会编入本版本。CAN ID 比较前先执行 `can_id & ~0x80000000`，故这里列的是清位后的 ID。下表位偏移按代码中的 `EXTRACT_BITS` 参数记录，从临时数组起始处算；未给物理单位，因为源码未定义。

| 清位后 CAN ID | 源码计算 | 写入字段与影响 |
| --- | --- | --- |
| `0x16D61B96` | `EXTRACT_BITS(tmpbuf,0,8)` 得档位字节；`0x7A/7B/7C -> 0`，`0x7E/7F/80/81 -> 2`，其余 `-> 1`。`EXTRACT_BITS(tmpbuf,48,16) * 0.00390625` 转为 `unsigned int`。 | `CurrentGear`、`VehSpeed`；设置 `valid_dt=true`。车速小数部分在整型转换时舍去。 |
| `0x1AD18096` | `EXTRACT_BITS(tmpbuf,32,16) * 0.001`；**没有 `break`**，随后继续用同帧第 0 位起的 16 位原始值。 | 先写 `VCU_IP_HCT1_Drv_ACC_Ped`，再写 `VCU_IP_HCT1_Drv_BRK_Ped`。 |
| `0x1AD19696` | `EXTRACT_BITS(tmpbuf,0,16)`，无比例系数。 | 写 `VCU_IP_HCT1_Drv_BRK_Ped`。 |
| `0x1ED6F396` | `EXTRACT_BITS(tmpbuf,32,8) * 0.4`。 | 写 `HCT1_Drv_BRK_Ped`。 |

档位表是代码的**设计表达式**，不是所有编译目标都已验证的运行结果：变量 `TransGearinfo` 是普通 `char`，若该平台的 `char` 为有符号类型，`0x80`、`0x81` 转入该变量后与正整数常量比较不会命中前进档条件，而会走默认档位。第 48 位起的车速原始值最大为 65535，按源码系数计算结果小于 256，存成整数后范围为 0～255；这只是代码可推得的数字范围，不是物理速度范围。

## 6. 消息通道与 JSON 契约

### 6.1 地址、方向和封装

| 用途 | 本模块行为 | 源码 |
| --- | --- | --- |
| CAN 帧输入 | `NN_SUB` 连接 `tcp://127.0.0.1:16002/16003/16004`，空过滤器订阅所有消息。 | `can_server.h:8-11`、`can_server.cpp:103-139` |
| SKES 消息接收 | `skes-linker` 的 SUB 连接 `tcp://127.0.0.1:19226`，空过滤器订阅，由回调按主题再筛选。 | `filter_pubsub.cpp:234-247`、`skes-core/skes-utils/utils.c:1309-1345` |
| SKES 消息发送 | `skes-linker` 的 PUB 连接 `tcp://127.0.0.1:19225`，向 `/skes/filter` 发送。 | `filter_pubsub.h:24-27`、`filter_pubsub.cpp:234-247,398` |

`can_server.h` 中另定义 `CAN0_SKT_TX_PORT=26002`，本模块活跃接收路径未使用它。`filter_pubsub.h` 定义 `FILTER_PUB_PORT=19225`、`FILTER_SUB_PORT=19226`，但 `filter_pubsub_new()` 直接写死地址字符串，不读取这两个宏；只改宏不会改变实际连接地址。`CAN_CHN_NUM=3` 用于数组尺寸，但 `start_can_server()` 的建连循环和 `can_msg_process()` 的 `nn_poll()` 数量也直接写了 `3`，改通道数时需一起核对。证据：`can_server.h:8-13`、`can_server.cpp:74,109`、`filter_pubsub.h:24-27`、`filter_pubsub.cpp:242-246`。

`skes_send()` 负责在 JSON 前加主题长度、主题名和载荷长度，具体二进制封装见 `skes-3rd-party/skes-linker-git/skes-linker.c:296-322`。`skes-relay` 的 `on_msg_filter()` 解析 JSON 后转发该主题（`skes-core/skes-relay/skes-relay.c:241-267`）。因此抓取原始 nanomsg 字节时不能把整个报文直接当纯 JSON；下文示例只展示 JSON 载荷。

### 6.2 主动上报

只有 `valid_dt=true` 且 `memcmp(curr_dt,last_dt,sizeof(filter_data_t)) != 0` 才尝试发布；线程每轮末尾 `usleep(200000)`。因此首次收到有效档位/车速帧也未必立即上报：如果当时整个 `curr_dt` 与清零后的 `last_dt` 相同，变化条件不成立。消息外层固定填空字符串 `version`、`id`，`ts` 取发布时 `CLOCK_REALTIME` 的毫秒值，`action="post"`、`type="properties"`；内层 `data.type="filter_table"`。属性数组固定包含以下五项，均带 `index=-1` 和 `data` 数值（`filter_pubsub.cpp:288-418`）：

| `property_name` | JSON `data_type` | 数据来源 |
| --- | --- | --- |
| `CurrentGear` | `int8` | `curr_dt.CurrentGear` |
| `VehSpeed` | `int16` | `curr_dt.VehSpeed`，C++ 实际类型为 `unsigned int` |
| `VCU_IP_HCT1_Drv_ACC_Ped` | `double` | `curr_dt.VCU_IP_HCT1_Drv_ACC_Ped` |
| `VCU_IP_HCT1_Drv_BRK_Ped` | `double` | `curr_dt.VCU_IP_HCT1_Drv_BRK_Ped` |
| `HCT1_Drv_BRK_Ped` | `double` | `curr_dt.HCT1_Drv_BRK_Ped` |

示例载荷（数值仅示意结构，时间戳不是现场值）：

```json
{
  "version": "", "id": "", "ts": 1780000000000,
  "action": "post", "type": "properties",
  "data": {
    "type": "filter_table",
    "data": [
      {"index": -1, "property_name": "CurrentGear", "data_type": "int8", "data": 2},
      {"index": -1, "property_name": "VehSpeed", "data_type": "int16", "data": 10},
      {"index": -1, "property_name": "VCU_IP_HCT1_Drv_ACC_Ped", "data_type": "double", "data": 0.5},
      {"index": -1, "property_name": "VCU_IP_HCT1_Drv_BRK_Ped", "data_type": "double", "data": 0},
      {"index": -1, "property_name": "HCT1_Drv_BRK_Ped", "data_type": "double", "data": 0}
    ]
  }
}
```

源码先复制到 `last_dt` 再构造/发送 JSON，而且没有检查 `skes_send()` 返回值。因此发送失败或构造失败时，这一状态也已被记录为“上次数据”；如果后续字段不再变化，该状态不会靠定时器自动重发。源码不添加 `msg_id`；也没有心跳、固定周期全量重发或字段级过期判断。

### 6.3 查询与响应

消息回调只分派 `skes_topic_type_filter`。它先用 cJSON 解析载荷，并通过 `skes_msg_header_parse()` 取得外层 `action/type/data`；该解析函数要求存在 `type` 和 `data`，不要求 `msg_id` 必填。查询还要求外层 `action=request`、`type=properties`，内层 `data.type=query`，然后遍历 `data.data` 数组。五个已知 `property_name` 中只要有一个命中，就会生成响应；全部未知或数组为空则不发送。比较是 `strcmp`，属性名大小写必须一致。证据：`filter_pubsub.cpp:104-214`、`skes-3rd-party/skes-linker-git/skes-msg.c:22-54`。

每个命中的项被追加毫秒 `ts`、`value`、`result`：当检查时 `valid_dt` 为真，`result=0`；否则 `result=-1`。如果请求项已有字符串类型的 `data_type`，`cJSON_SetValuestring()` 把它改成上表类型；若无此键，不会新建；若该键不是 JSON 字符串，所用 cJSON 实现会拒绝改写，而调用方没有检查其返回值。响应是已经改写的请求 JSON 的副本，只把外层 `action` 改为 `response`：外层请求 `ts`、`msg_id`、未知属性以及其他原有字段都会保留。**单项 `ts` 是响应处理时取的时间，外层 `ts` 仍是原请求时间。**证据：`filter_pubsub.cpp:121-165,194-210`、`skes-3rd-party/cjson-git/cJSON.c:400-425`。

请求数组项若原本已有 `ts`、`value` 或 `result`，处理函数使用 `cJSON_AddNumberToObject()` 再追加同名键，并不替换旧键；当前 cJSON 的 `add_item_to_object()` 会把新项追加到对象链表。这会产生重复键，消费者如何解释取决于其 JSON 解析逻辑。诊断异常响应时应先查看原始请求和原始响应，而不能假定这些键唯一。证据：`filter_pubsub.cpp:121-165`、`skes-3rd-party/cjson-git/cJSON.c:2044-2075,2160-2169`。

示例请求与对应响应（假设已收到有效 CAN 数据，数值仅演示结构）：

```json
{"version":"","id":"","ts":1780000000000,"msg_id":"example-1","action":"request","type":"properties","data":{"type":"query","data":[{"index":-1,"property_name":"CurrentGear","data_type":"boolean"}]}}
```

```json
{"version":"","id":"","ts":1780000000000,"msg_id":"example-1","action":"response","type":"properties","data":{"type":"query","data":[{"index":-1,"property_name":"CurrentGear","data_type":"int8","ts":1780000000100,"value":2,"result":0}]}}
```

**无有效数据时的限制：**`temp_dt` 是局部变量，仅在 `valid_dt=true` 时赋值；即使未赋值，命中字段后代码仍读取它写 `value`，并标 `result=-1`。此时 `value` 不能使用，C++ 层面的未初始化读取也使运行行为不可靠。成功发送响应之后 `on_msg_filter()` 仍返回 `-1`，回调也返回 `-1`；当前 `skes-linker` 实现忽略回调返回值，不能把该负值解释为“没有发送响应”。

## 7. 线程、状态及失败路径

| 位置 | 源码中的具体行为 | 分析意义 |
| --- | --- | --- |
| `filter_pubsub_new()` | `malloc` 状态对象，不清零；调用 `skes_linker_init()` 后直接返回其结果。 | 初始化失败时入口直接返回，源码未释放已分配对象。 |
| `filter_pubsub_run()` | 将 `running=true`，依次创建接收与发布线程；若第二个创建失败，直接返回错误。 | 第一线程若已启动，错误路径未在此处停止/回收它。 |
| `main()` | 启动发布线程后才 `memset(filter_mng)`、`pthread_mutex_init()`；互斥锁初始化、CAN 线程创建和 detach 的返回值均未检查。 | 发布线程可能访问尚未初始化的 mutex/数据；初始化或创建失败也未传回主循环。 |
| CAN 线程 | 创建或连接 socket 失败即返回；`nn_poll` 错误或非 `EAGAIN` 接收错误会退出；当前代码没有重连循环，也没有在部分连接已成功后的错误路径关闭先前 socket。 | CAN 路径停止后，主进程仍可能继续存活并保持旧属性。`nn_connect()` 成功不等于远端实际在线。 |
| CAN 帧解析 | 写 `curr_dt`、`valid_dt` 时没有持有 mutex；查询与发布读取部分则加锁。 | 同一把锁没有覆盖写侧，不能保证线程间取得一致快照。 |
| 运行标志 | `running` 是普通 `bool`，主线程、SKES 接收线程和发布线程会读写它，但没有统一使用互斥锁或原子操作。 | 跨线程停止与状态观察缺少同步保证；不能把当前循环当作可靠的线程生命周期协议。 |
| 位提取临时数组 | 16 字节 `tmpbuf` 只初始化前 8 字节；偏移 32/48 的宏调用会固定再复制 8 字节。 | 活跃解析路径读到数组内未初始化尾部；这与大端测试宏的数组越界是两个不同问题，均需单独验证或修正。 |
| 上报线程 | 先更新 `last_dt` 后才构造、发送；直接 `memcmp` 整个 `filter_data_t`。 | 发送失败不会自动重试；整块比较包含结构体可能存在的填充字节。 |
| 查询线程 | 只在 `valid_dt=true` 时复制 `curr_dt`，但无效时仍读局部 `temp_dt`。 | `result=-1` 时 `value` 不可靠；有效标志读取本身也没有统一同步。 |
| 退出路径 | `filter_pubsub_stop()` 会把标志设为 false 并调用 `pthread_join()` 等待两个 SKES 线程；入口未调用它。接收线程把 `skes_run()` 的超时设为 `-1`，需核验停止时怎样唤醒等待。`filter_pubsub_release()` 只释放 SKES 句柄和状态对象。CAN 线程已 detach。 | 当前入口看不到对三个线程与 CAN socket 的完整停止、join、关闭顺序。 |

上述并发与生命周期问题是从调用顺序和锁的覆盖范围直接得出的源码风险；是否在现场触发、触发频率及后果，需要针对目标编译器和运行环境验证。另需注意 `0x1AD18096` 缺少 `break` 的穿透行为，以及档位 `0x80/0x81` 的 `char` 符号性问题，均不能在文档中自动修正为“预期业务值”。

## 8. 外部模块关系与部署条件

`skes-relay` 的 filter 主题处理函数会转发消息；`sep-aeb/sep-aeb.c` 的 `on_msg_filter()` 可处理 `filter_table` 上报和匹配自身 `msg_id` 的查询响应，并有发起查询的代码。当前工作区的 `skes-dataengine` 也使用 `/skes/filter` 主题；所以订阅方需要结合消息 `action`、`data.type` 和业务字段区分消息。此处只证明源码存在这些接口，不断言设备上这些服务同时运行。相关证据：`skes-core/skes-relay/skes-relay.c:241-267`、`skes-apps/sep-aeb/sep-aeb.c:484-537,835-847`、`skes-apps/skes-dataengine/vehicles/forklift/skes_qt_bridge.cc` 中的 `/skes/filter` 处理。

`filter_sep` 自己固定尝试连接三路 CAN 输入；相邻 `skes-dataengine/can/can_receiver.cc` 的默认通道数却是 2，虽然数组列有第三个相同地址。两者是否连接同一个现场发布端、是否启用 `16004`，需按部署配置核对。程序还依赖 SKES 消息通道可用及对应动态库存在；仅运行该可执行文件并不能在没有 CAN 帧来源时产生有效属性。

## 9. 测试覆盖、可核验项与未验证项

`tests/CMakeLists.txt` 生成 `FilterUnitTest`、`FilterRequestTest`。顶层 `CMakeLists.txt:37` 调用 `enable_testing()`，本模块只通过 `add_test()` 注册前者；后者会每 5 秒循环发送固定查询，没有被注册为 CTest 用例。`FilterUnitTest` 检查部分位提取、64 位全 1 情况和大端宏的几个例子；没有覆盖四个活跃 CAN ID 的端到端解析、JSON 发布/查询、三路连接错误、并发、超时与退出。`FilterRequestTest` 固定查询四个字段，没有查询 `HCT1_Drv_BRK_Ped`，其请求 `data_type` 均写为 `boolean`，不能当作当前真实输出类型。证据：`tests/CMakeLists.txt:26-37`、`tests/test.cpp:8-58`、`tests/test_request.cpp:18-75`。

已有与本工作区相符的构建目录及依赖时，可以在仓库根目录执行：

```bash
cmake --build build --target filter_sep FilterUnitTest
ctest --test-dir build -R '^FilterUnitTest$' --output-on-failure
```

这两条是源码对应的验证入口，不代表本文已执行。本文完成的是静态源码核对及文档中 JSON 示例的语法检查；未运行目标程序、单元测试、CAN 注入或设备联调。物理单位、真实车速比例、硬件 CAN 通道映射、第三路端口、实际消息时序与并发问题发生概率均不能仅由本模块源码最终判定。

## 10. 设备故障排查手册

本节按“部署与进程 → CAN 输入 → 进程内解析 → SKES 发送 → relay 转发 → 消费者处理”的顺序定位问题。先记录故障时刻、设备实际二进制和原始报文，再考虑重启；重启会抹去线程已退出或偶发错误的现场。**进程存在、端口可见、`nn_connect()` 返回成功**各自只证明一层现象，不能单独证明已经收到正确 CAN 帧或成功发布属性。

### 10.1 核对部署和启动链

1. 在设备上确认实际安装目录，不要把开发机的源码路径或 `build/` 当设备路径。记录设备时间、软件包版本、实际执行文件和启动参数。`main.cpp` 的 `APP_VERSION="V0.0.2"` 是编译期日志字符串，不能单独证明二进制等于本文所查 HEAD。
2. 查看实际 `apps/filter_sep/sep.ini`：源码安装的版本为 `enabled=true`、`name=filter_sep`、`param=`。`skes-sepd.sh` 读取 `name/cmd/param/enabled`；若设置 `cmd`，优先用它作为可执行文件名。`filter_sep` 程序自身不读取该文件。
3. 查 `filter_sep`、`skes-sepd.sh`、`skes-relay` 的进程和 PID 是否稳定。`skes-sepd.sh` 启动时清理同名 SEP，之后约每 5 秒扫描、拉起缺失进程（`skes-scripts/skes-sepd.sh:71-89,119-166`）。PID 反复变化应查进程退出和启动日志；进程一直存活却无数据，还需查其 CAN 接收线程，因为启动器只检查进程。脚本用 `/tmp/filter_sep.pid` 作 `flock` 锁路径，**该文件存在不代表程序正在运行**（同文件 `:113-116`）。
4. 若无法启动，核对实际二进制、执行权限、目标架构和共享库。模块链接库见第 2 节；动态加载失败可能发生在 `main()` 之前，应以设备加载器报错为准。下面的命令只读，路径须换成设备实际安装前缀。

```sh
APP_DIR=/实际安装前缀/apps/filter_sep
cat "$APP_DIR/sep.ini"
ls -l "$APP_DIR/filter_sep"
ps -ef | grep '[f]ilter_sep'
ps -ef | grep '[s]kes-sepd'
ps -ef | grep '[s]kes-relay'
```

若设备提供 `ldd`，在**设备上**对实际执行的文件运行 `ldd "$APP_DIR/filter_sep"`，查看是否有 `not found`；精简系统可能没有该命令，此时看加载器输出和包内 `lib/`。不要用开发机对交叉编译二进制执行 `ldd` 的结果判断设备依赖。

### 10.2 日志位置和可观察证据

| 来源 | 源码中确实会出现什么 | 不能据此推出什么 |
| --- | --- | --- |
| 系统 syslog | `main.cpp:36` 通过 `LOG_D` 记录编译日期、时间和 `V0.0.2`；宏调用 `syslog(LOG_DEBUG,...)`。 | 是否保存 DEBUG 级别取决于设备日志配置；这条日志不能证明 CAN 或 SKES 通信成功。 |
| `skes-log` 的 stdout | `received skes filter data message`、`invalid JSON`、`missing type or data in header or incorrect format`、`failed to create thread` 等。 | 收到 filter 主题消息不等于收到 CAN 帧；本模块没有设置专用日志文件路径。 |
| CAN 线程的 stdout/stderr | 三次 `nn_connect()` 返回成功会依次打印 `connected to can0/1/2 port`；创建、订阅、连接失败会打印 `nn_socket`、`nn_setsockopt`、`nn_connect` 或 `asprintf failed`。 | nanomsg 连接可能异步；`connected` 不能证明远端发布端在线或数据格式正确。 |
| 缺少的日志 | CAN ID 逐帧日志处于注释状态；发布处不检查或记录 `skes_send()` 返回值。 | 日志中没有 CAN 帧或发送成功记录，是可观测性缺口，不能单凭日志空白判断哪一层故障。 |

`skes-log` 实现会向 stdout 输出；文件输出需要设置路径并启用，而本模块没有调用这两个配置函数（`skes-3rd-party/skes-log/skes-log.c:159-172,334-415`）。设备若用 systemd 可按实际服务查看 `journalctl`；若用 BusyBox/syslog，可看 `logread` 或部署脚本接收 stdout/stderr 的位置。源码没有指定固定的 `filter_sep` 日志文件。

### 10.3 快速定位图（普通 Markdown 可显示）

```text
设备故障
  |
  +--> filter_sep 无进程或 PID 反复变化？
  |      +--> 查 sep.ini、skes-sepd、二进制架构/依赖和启动日志
  |
  +--> 进程在，但无属性？
  |      +--> 查三路 CAN 发布端及实际帧；重点确认 0x16D61B96
  |      +--> 再查 valid_dt 门槛、字段是否变化、relay 19225/19226
  |
  +--> 有属性但值不对？
  |      +--> 保存原始 8 字节；按第 5 节复算；核对协议表/编译平台
  |
  +--> 属性正确但下游异常？
         +--> 查 /skes/filter 的 action、data.type、字段和 msg_id
         +--> 再查 relay 转发及对应消费者
```

这是检查顺序，不是自动诊断结论。本程序没有“已收 CAN 帧数”“上报成功数”的统计；需结合上游记录或受控抓包/订阅工具，才能在图中的层次之间作出判断。

### 10.4 按现象核对的清单

| 现象 | 检查顺序与源码依据 |
| --- | --- |
| 起不来或反复重启 | 先看设备 `sep.ini`、启动器与 PID，再查加载器/线程创建错误。`filter_pubsub_new()`、`filter_pubsub_run()` 失败会使 `main()` 返回 `-1`；启动器约 5 秒后可能再次拉起。若根本没有入口日志，也可能是日志未保存或程序在进入 `main()` 前失败，不能直接定性。 |
| 进程在，但长期无 `filter_table` | 确认上游确实提供 `16002/16003/16004`，再核对收到了清位后 CAN ID `0x16D61B96`。只有该帧将 `valid_dt` 设为真；第三路是否在设备上启用不能从本模块推断。`connected to canN port` 只表示建连调用成功。数据与 `last_dt` 相同也不会发送。 |
| 曾上报，随后停止更新 | 查上游帧是否仍到达、字段是否仍变化，以及 CAN 线程是否因 `nn_poll`/`nn_recv` 错误退出。该线程退出不改变主进程运行标志，因此启动器可能仍认为进程正常。当前实现不发送心跳，不做过期判断；发布前已更新 `last_dt`，发送失败不会自动重发同一状态。 |
| 首次上报中踏板为零 | 核对四类目标 CAN ID 到达顺序。`valid_dt` 是整体标志；只收到档位/车速帧即可发布五项，但其他三个字段可能仍是初始化零值。没有逐字段有效标志，不能把初始零直接当已测得的真实零值。 |
| 档位/车速/踏板数值异常 | 保存帧 ID、8 字节负载、通道、时间和目标架构，按第 5 节复算。`0x80/0x81` 档位与 `char` 符号性有关；`0x1AD18096` 会穿透改写制动字段；车速会舍去小数。位提取宏还会读取临时数组中未初始化的尾部（第 4 节）；物理单位、上游字节序和结构体布局需查设备协议及发送端，不能自行猜测。 |
| 查询没有响应 | 核对主题 `/skes/filter`、外层 `action=request/type=properties`、内层 `data.type=query`、数组中至少一个完全匹配的五个属性名，以及 relay 和 `19226` 接收链路。全是未知属性则不发；无有效 CAN 数据仍可能返回 `result=-1`。回调固定返回 `-1`，但当前消息库忽略它，不能据此判为未发送。 |
| 查询有 `result=-1` 或 `value` 怪异 | 检查查询时是否已收到 `0x16D61B96`；`valid_dt=false` 时 `value` 来自未初始化局部结构，不可用于业务判断。若请求 `data_type` 不是字符串，代码调用的 cJSON 改写函数不会更新该键；若请求项已经带 `ts/value/result`，响应可能含重复键，应检查原始 JSON。 |
| `/skes/filter` 有消息，下游 AEB/UI 仍无结果 | 区分 `post/filter_table`、`request/query`、`response/query`；对查询检查 `msg_id`。同主题还有其他模块使用；`sep-aeb` 用自己的 `msg_id` 匹配查询响应，看到主题存在不代表消费者接受了该消息。 |
| 上报间隔不稳定或静止时无上报 | 对照原始帧与输出：200 毫秒是发布线程每轮末尾休眠时长，不是保证的上报周期；整体数据不变则跳过，发送失败返回值也没有检查。 |

设备若有 `ss`，可用以下**只读**命令辅助检查端口；`LISTEN`、`ESTAB` 仍不能代替 CAN 负载和 JSON 载荷的核对：

```sh
ss -lntp | grep -E ':(16002|16003|16004|19225|19226)([^0-9]|$)'
ss -tnp  | grep -E ':(16002|16003|16004|19225|19226)([^0-9]|$)'
```

### 10.5 受控复现与结论记录

1. 在测试设备或隔离环境保存同一时间段的上游原始 CAN 帧（ID、负载、通道、时间）、进程/启动器状态、`/skes/filter` JSON。先比对帧与第 5 节的换算，再追踪 relay 和消费者；仅凭页面显示值无法定位是哪一层。
2. 用明确的 `msg_id` 发送一次结构正确的查询，核对 `action=response`、相同 `msg_id`、所查字段的 `result/value`。第 6.3 节有载荷示例。`tests/test_request.cpp` 会每 5 秒无限循环发请求，只覆盖四个属性；它不能当作“只查一次”的生产设备诊断命令。
3. 在隔离环境分别注入四个已知 CAN ID 的帧，核对五个属性；对 `0x1AD18096` 要同时观察制动字段，对 `0x80/0x81` 要在目标编译器上确认 `char` 符号性。还应核对“无 `0x16D61B96`”“数据不变”“未知属性查询”“relay 暂时不可用”的分支。
4. 结论明确归到**上游帧未到、本模块解析错误、本模块未尝试或发送失败、relay 未转发、消费者未接受**中的哪一层，并附原始证据。当前代码缺少逐帧和发送成功日志时，不要仅凭日志空白把责任归给某层；必要时在受控构建中添加临时观测点。

本节不把假设中的设备日志路径、CAN 物理单位或现场端口提供者写成已确认事实。
