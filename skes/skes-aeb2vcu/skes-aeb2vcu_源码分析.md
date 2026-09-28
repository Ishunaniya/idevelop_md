# skes-aeb2vcu 源码逻辑分析

## 1. 分析范围与结论边界

本文依据 `/home/tronlong/lyp/code/skes` 仓库当前工作树中的 `skes-apps/skes-aeb2vcu/` 编写。2026-09-28 再次复核时分支为 `main_ui`，HEAD 为 `b28d0b8`；本模块的 `main.c`、`skes-aeb2vcu.c/.h`、`di_get.c/.h`、`CMakeLists.txt` 和测试构建文件相对 HEAD 均无本地差异。主要依据还包括三个设备 INI、`sep.ini`；为核对消息回调与消息头行为，阅读了 `skes-core/skes-utils/utils.c` 的 `skes_linker_init`、`skes-3rd-party/skes-linker-git/skes-linker.c` 的接收实现以及同目录 `skes-msg.c` 的消息头解析。下文区分**当前实际执行路径**、**只定义但未调用的代码**与**需目标设备验证的行为**。这里只做静态源码分析，没有在车辆或目标板上执行控制链路。

程序名称表示把 AEB（自动紧急制动）信息送往 VCU（车辆控制单元）的桥接应用；源码还处理 BSD（盲区检测）提示、故障码与履带吊配重识别。程序并不直接操作 CAN 控制器或物理串口：它通过本机 nanomsg 连接把封装好的数据交给其他服务。

## 2. 入口、线程和数据关系

### 普通 Markdown 可显示的总流程图

```text
进程启动 main.c
  |
  +-- 初始化日志：读/写 skes-aeb2vcu.ini 的 [log] level
  +-- 注册 SIGUSR1：收到后循环切换日志等级
  +-- skes_aeb2vcu_new
  |     +-- 分配并清零状态对象
  |     +-- 创建 SKES 订阅/发布连接
  |     +-- 读 [dev] type；按设备类型读附加参数
  |     +-- 初始化 RS-485、RS-232 的本机发送连接
  +-- skes_aeb2vcu_run
  |     +-- 线程 A：轮询 SKES 消息，更新共享状态
  |     +-- 线程 B：读取共享状态，组装并发送 CAN 帧
  |     +-- 线程 C：查询 DI 通道 1，更新行驶方向状态
  +-- main 每 200 ms 查询 running 标志
        +-- 标志为真：继续等待
        +-- 标志为假：释放 SKES 句柄和状态对象，退出
```

**文字版流程：** `main` 先设定日志文件、大小上限 5 MiB、文件数 4，并从 INI 读取日志等级再写回；随后注册 `SIGUSR1`、记录版本号 `1.1.3.0`。`skes_aeb2vcu_new` 创建状态及通信句柄，初值为装载机类型，然后读取配置决定具体设备分支。`skes_aeb2vcu_run` 启动消息、CAN、DI 三个线程。主线程每 200 ms 检查 `sre->running`。当前 `main` 没有调用 `skes_aeb2vcu_stop`，也没有针对 `SIGTERM`/`SIGINT` 的显式退出处理；因此正常停机与资源收尾不能只根据 `stop` 函数的存在来推断。

**共享状态：** `skes_aeb2vcu_entry_t` 保存设备类型、故障列表指针、装载机 AEB/告警开关与 BSD 状态、叉车告警级别和方向、履带吊两路配重等。消息线程与 DI 线程写这些字段，CAN 线程读取；本文件未见这些字段的互斥锁或原子同步。这个事实提示并发审查重点，不能仅凭静态阅读确定某次竞态的实际表现。

## 3. 消息接收与分发

```text
本机 SKES broker -> 订阅句柄 -> on_skes_msg_received
  |
  +-- filter 主题
  |     +-- JSON 头部 type=properties、data.type=AEB
  |           -> 按 property_name 更新装载机 AEB 控制字段
  +-- event 主题
        +-- JSON 头部 type=fault -> 更新故障码列表
        +-- JSON 头部 type=alarm -> AEB 前后告警 / BSD 方位状态
        +-- JSON 头部 type=event -> source=counterweight-recognition
                                      更新履带吊通道 4/5 配重
```

**文字版流程：** `skes_linker_init` 以 `is_bind=false` 创建 SUB 与 PUB 句柄，订阅端连接 `127.0.0.1:19226`，发布端连接 `127.0.0.1:19225`。消息轮询线程把接收超时设为无限、等待模式设为真，循环调用 `skes_run`。当前回调只按 `filter` 和 `event` 两个主题分发；心跳、版本、设备数据、原始设备数据和定位的回调分支位于 `#if 0`。创建 PUB 句柄不代表活跃路径在定期发布消息。

`filter` 消息需是 `properties` 类型，内部 `data.type` 为 `AEB`。只有 `aeb_enabled` 为真时，才按 `property_name` 写入 `AEBActivateCmd`、`FlashCmd`、`AlarmCmd`、`BrkCurCmd_Front`、`BrkCurCmd_Rear` 对应字段。该开关由装载机发送线程从 UI 文件更新；因此源码表明这些属性在开关尚未启用时会被忽略。前桥电流先转为 `uint16_t`，后桥电流却先转为 `uint8_t` 再写入 12 位字段；后者无法原样保留大于 255 的有效 12 位数值，超出转换范围时的具体结果不能仅凭源码保证。

`event` 主题内再按消息头 `type` 区分：`fault` 按 `source` 更新故障列表，`alarm` 按 `source=aeb` 或 `bsd` 更新告警状态，`event` 按 `source=counterweight-recognition` 更新配重。处理函数未按当前设备类型过滤输入；设备类型是在 CAN 发送阶段决定哪些状态被使用。

当前 `skes_msg_header_parse` 对本程序调用方式的必需字段是 JSON 根对象中的 `type` 和 `data`；`version`、`id`、`ts`、`msg_id`、`action` 在这些回调中不作为解析成功的前提。不能因为某条消息缺少这些可选头字段就直接断定它被本程序拒绝。`type` 必须与本程序支持的分支匹配，`data` 还要符合对应分支的内部结构。

`on_msg_filter` 与 `on_msg_event` 在处理结束后仍返回 `-1`，外层回调也转成 `-1`。但当前 `skes-linker.c` 的 `do_skes_recv_msg` 调用回调后没有检查其返回值，故不能把该返回值直接等同于消息重试或丢弃；更换底层实现时应重新核对。

## 4. 各类输入怎样改变状态

| 输入 | 实际解析条件与状态变化 |
| --- | --- |
| AEB 属性 `filter/properties` | `data.type=AEB`；在 `aeb_enabled` 时，把上述五种属性转为整数写入 `aeb_fault_t` 对应位字段。未知属性略过。 |
| 故障 `event/fault` | `source` 为 `camera`、`sound_light_alarm`、`object_detection`、`data`、`communication` 时，按位置/类型匹配静态故障表，记录输入 `code`。输入为 0 时相当于清除该表项。 |
| AEB 告警 `event/alarm` | `source=aeb` 且 `status=triggered` 时，从数据数组的 `camera.position` 与 `alarm.trigger/level` 更新前、后触发及等级，并记录 `alarm_seq`、`alarm_ts_ms`；没有出现在本次数组中的方向沿用旧触发状态。非 `triggered` 状态会清除前后触发和等级。若数据数组含多项，等级在循环中被后处理的项覆盖；若 `status`、`data` 或必要的内层对象缺失，可能提前返回而保留旧状态。 |
| BSD 告警 `event/alarm` | `source=bsd` 且 `status=normal` 清除四个方向；`triggered` 时仅统计 `camera.status=triggered` 的前、后、左、右项目，最后整体替换四个方向状态。装载机实际声音输出只读取后、左、右。 |
| 配重识别 `event/event` | `source=counterweight-recognition`；只接受 `data.camera.channel` 为 4 或 5。`status=triggered` 时累加 `texts` 中分数达到阈值、`atof(text)>0` 的值；其他状态把该通道配重置 0。两个通道分别保存。 |
| DI 查询 | 向本机 38000 端口请求 IO 状态，在响应 `status.io[]` 中查找 `dev=di`、`chn=1`，取 `params.level`；与 `forward_di_level` 相等则 `is_driving_forward=true`。查询失败时该方向状态保持上次值。但源码对 `chn` 的空值检查误查了 `dev`，对 `level` 对象也没有空值检查；当前 cJSON 对缺失数字返回 `NAN`，随后转换成整数，不能保证得到可靠方向。匹配到通道和 `params` 后即使 `level` 缺失，解析函数仍可能报告成功。 |

### 故障码表

`fault_code_list` 共 16 项；发送线程逐项扫描，遇到第一个 `code != 0` 的表项就把对应的固定 `fault_code` 填入当前装载机 AEB 帧，并从下一项继续轮询。没有活跃故障时发送 0。代码没有把事件输入的 `code` 数值原样放入 CAN 帧。

| `source` | `type`/`position` | 源码编号 `code_name` | CAN `fault_code` |
| --- | --- | --- | --- |
| `camera` | `left_side` | `Y1201` | `0x01` |
| `camera` | `right_side` | `Y1202` | `0x02` |
| `camera` | `rear` | `Y1203` | `0x03` |
| `camera` | `front` | `Y1204` | `0x04` |
| `camera` | `dms` | `Y1205` | `0x05` |
| `sound_light_alarm` | 无细分 | `Y1306` | `0x06` |
| `object_detection` | `seat_departure` | `Y2410` | `0x10` |
| `object_detection` | `personnel_instrusion` | `Y2411` | `0x11` |
| `object_detection` | `fatigue_detection` | `Y2412` | `0x12` |
| `data` | 无细分 | `Y2513` | `0x13` |
| `communication` | `rs485` | `Y2616` | `0x14` |
| `communication` | `lte` | `Y2617` | `0x15` |
| `communication` | `gps` | `Y2618` | `0x16` |
| `communication` | `bluetooth` | `Y2619` | `0x17` |
| `communication` | `ota` | `Y2620` | `0x18` |
| `communication` | `ntp` | `Y2621` | `0x19` |

`personnel_instrusion` 是源码中的拼写，匹配输入时必须使用相同字符串。`code_name` 保存在静态表中，但当前 CAN 发送逻辑使用的是 `fault_code` 字段。

## 5. CAN 发送线程：三个设备分支

```text
do_can_msg_sender：确认 aeb_fault_t 恰好 8 字节 -> 连接 CAN0 TX 26002
  |
  +-- wheel_loader -> 刷新 UI 使能 -> 轮询故障码、BSD 提示
  |                   -> 写生命信号/校验 -> 发 AEB 帧 -> 等 100 ms
  +-- crawler_crane -> 通道 4 + 通道 5 -> 乘 10 写入帧低两字节
  |                   -> 发配重帧 -> 等 500 ms
  +-- forklift -> AEB 前后触发 + DI 方向 + 告警等级
                -> 计算当前制动值 -> 按电机版本编码
                -> 发制动帧 -> 等 100 ms（版本 1）或 35 ms（版本 2）
```

**文字版流程：** 线程创建 nanomsg `NN_PUB` 套接字并连接 `tcp://127.0.0.1:26002`。它发送的是头文件定义的 `can_frame_t` 结构（含 `can_id`、`can_dlc`、8 字节 `data` 等），不是直接调用 SocketCAN。每轮依 `dev_type` 选择一个分支并执行对应等待。若套接字创建或连接失败，线程直接返回；源码未见该线程内部自动重连。

`can_frame_t` 的字段依次为 `uint32_t can_id`、`uint16_t can_dlc`、`uint16_t rsv_ms`、`uint8_t data[8]`、`uint32_t timestamp`。发送线程将局部帧初始化为零，之后设置 ID、DLC 和数据；当前函数没有更新 `rsv_ms` 与 `timestamp`。它用 `nn_send(..., &frame, sizeof(can_frame_t), ...)` 发出整个 C 结构，结构布局与目标 ABI 有关，不能把它当作只含 8 字节数据的标准线缆 CAN 帧。

### 5.1 装载机 `wheel_loader`

- 约每 5 秒读一次 `/tmp/ui_config.json`：根对象 `aebEnabled` 控制 AEB 字段，`alarm.enabled` 控制声音提示。文件读取或 JSON 解析失败时保留已有开关值；若 JSON 可解析但缺少某个字段，本轮传入的局部布尔初值为假，对应开关会被写为假。状态对象初始清零，故在首次成功读取前两个开关为假。
- 故障表按固定顺序找第一个活跃项；故障码字段每帧先清零，找不到活跃项就保持 0。
- BSD 后、左、右提示按索引轮流尝试，分别编码为 1、2、3；非零声音值的下一轮先发 0。BSD 前方状态虽然解析并保存，当前声音编码不使用。
- AEB 禁用时清零激活、闪灯、报警及前后制动电流字段；启用时沿用已收到的属性值。声音提示由 `alarm.enabled` 单独控制。
- 每帧递增 4 位生命信号；校验计算为“前 7 个数据字节之和 + 生命信号低 4 位 + CAN ID 四个字节”，再按 `(((sum >> 6) & 3) + (sum >> 3) + sum) & 7` 得到校验值。将 8 字节位字段结构复制进帧数据，DLC 设为 8。
- 当前 CMake 定义 `USE_NEWC_VEHICLE`，协议 CAN ID 为 `0x168B9664`；写入 `can_frame_t.can_id` 时还会或入 `0x80000000` 扩展帧标志。关闭该宏会改用头文件中的 `0x0CFF2003` 和另一种 8 字节字段布局，不能只替换 ID。
- `aeb_fault_t` 使用 C 位字段并要求 `sizeof(aeb_fault_t)==8`；长度检查只能确认总字节数，不能单独证明各位在目标编译器上的布局与 VCU 协议一致。协议排查需对照目标板实际发出的 8 字节数据。

### 5.2 履带吊 `crawler_crane`

通道 4 和 5 的识别数值分别存在 `counterweight_value[0/1]`。默认识别分数阈值是 0.85，安装模板也配置为 0.85；低于阈值、文本转数值后不大于 0 的项目不计入。发送时先把两路结果相加，再乘 10 并转为 `uint16_t`，低字节放 `data[0]`、高字节放 `data[1]`，其余数据字节清零，DLC 为 8，CAN ID 为 `0x600`。代码没有对溢出或失效数据设置额外有效位；未收到新事件时保留上次通道值。

### 5.3 叉车 `forklift`

```text
每轮先取：前/后 AEB 触发、当前等级、DI 方向、上次等级与时间
  |
  +-- 满足立即重算条件？
  |     +-- 是 -> 按当前告警和方向重算
  |     +-- 否 -> 距上次触发不足 1 秒？
  |                  +-- 是 -> 沿用上次制动值
  |                  +-- 否 -> 按当前告警和方向重算
  |
  +-- 重算时：方向匹配且等级为 1/2 -> 取对应前/后配置值、更新时间
  |            否则 -> 本轮值为 0
  |
  +-- 电机版本 1：写 ID 0x440 的 data[0]，发送，等待约 100 ms
  +-- 电机版本 2：写 ID 0x120 的 data[0..2]，发送，等待约 35 ms
  +-- 其他版本：不发送，等待约 100 ms
```

**文字版流程：** 先判断是否立即按当前告警重算制动值；未满足立即条件时，若距上次触发不足 1 秒，直接使用上次值，**这一保持分支不重新检查当前 DI 方向和前后触发是否匹配**；超过 1 秒才重算。重算只在告警方向与 DI 方向相配且等级为 1 或 2 时更新对应配置值。最后按制动电机版本选择 CAN ID 与数据字节；未知版本没有发送分支。下面列出具体条件和编码。

1. `alarm` 提供前/后触发和 `level`；DI 通道 1 决定当前方向。只有“前方触发且前进”或“后方触发且非前进”才进入相应制动值分支。`is_driving_forward` 的初始值为假，DI 查询失败时也不更新；源码在方向尚未确认时没有单独的“未知”状态。
2. 当上次等级为 0 且当前非 0，或上次等级非 0 且当前非 0、`last_level >= current_level` 时，立即重新计算。其他情况若距离上次触发不足 1 秒，保持上次制动值，即使 DI 方向在这个时间窗内改变也不会在保持分支重新校验；超过 1 秒后再按当前方向与等级计算。代码的等级比较使用数值关系，配置模板中一级制动值 60、二级 40，不能据此推断级别数字越大制动力越大。
3. 当前等级 1 或 2 且方向匹配时，分别取 `front_brake_level1/2_value` 或 `rear_brake_level1/2_value`，同时更新上次等级、时间和保持值。未匹配方向或等级时本轮局部制动值默认为 0；短时保持分支除外。`status` 解除告警会把当前触发与等级清零，但发送线程里的 `last_aeb_trigger_level` 只在匹配的触发分支更新。
4. `brake_motor_version=1`：CAN ID `0x440`，`data[0]` 直接放当前制动值，其余字节清零，约 100 ms 一帧。`brake_motor_version=2`：CAN ID `0x120`，制动值大于 0 时 `data[0..1]=03 84`，否则为 `00 00`；`data[2]` 是不超过 100 的当前制动值，其余字节清零，约 35 ms 一帧。其他版本不发制动帧，只等待约 100 ms。两种有效版本的 DLC 都为 8。

## 6. 配置、端口和构建

### 配置文件

| 文件/字段 | 源码中的作用 |
| --- | --- |
| `sep.ini` | `enabled=true`、`name=skes-aeb2vcu`、空 `param`，由安装规则复制到应用目录。 |
| `skes-aeb2vcu.ini.wheel_loader` | `[dev] type=wheel_loader`，`[log] level=4`。 |
| `skes-aeb2vcu.ini.crawler_crane` | `[dev] type=crawler_crane`、`score_threshold=0.85`，`[log] level=4`。 |
| `skes-aeb2vcu.ini.forklift` | `[dev] type=forklift`、`brake_motor_version=2`、前/后一级值 60、前/后二级值 40、`forward_di_level=0`，`[log] level=4`。 |
| `/tmp/ui_config.json` | 装载机运行时读取 `aebEnabled` 与 `alarm.enabled`；这个绝对路径不由本项目安装规则生成。 |

构建变量 `SKES_AEB2VCU_TYPE` 只决定安装哪份模板，并把它重命名为 `skes-aeb2vcu.ini`；程序运行时仍从 INI 的 `[dev] type` 决定设备类型。变量不匹配三个已知值时，安装规则选装载机模板。运行时若 INI 读取或类型解析失败，初始设备类型保留 `wheel_loader`，但源码会记错误；不能把这理解为可靠的配置校验流程。配置路径及日志文件名都是相对路径，需核对应用实际工作目录。

### 本机连接与数据形式

| 地址 | 当前用途 |
| --- | --- |
| `tcp://127.0.0.1:19226` | SKES 消息 SUB 连接；接收 `filter` 和 `event` 主题。 |
| `tcp://127.0.0.1:19225` | SKES 消息 PUB 连接；当前活跃路径未见业务发布调用。 |
| `tcp://127.0.0.1:26002` | CAN0 TX 服务；发送 `can_frame_t` 结构。头文件也定义 CAN1/CAN2 的收发端口，但发送线程当前只连接 CAN0 TX。 |
| `tcp://127.0.0.1:26006`、`:26007` | RS-485、RS-232 发送连接；初始化会执行，`com_output` 在当前源码没有调用点。 |
| `tcp://127.0.0.1:38000` | DI 查询连接使用 `NN_REQ` 套接字；请求 `status.io` 中的 DI 数据，接收超时设为 5 秒。服务端具体实现不在本项目源码中。 |

DI 线程对所有设备类型都会启动，循环执行 `di_get(1, ...)`，每轮结束后睡眠 1 秒；其结果只在叉车 CAN 分支使用。`di_get` 先发 JSON 请求，再在响应数组中匹配通道 1。若查询失败，线程只记错误，不把方向状态重置为“未知”。

本目录 CMake 使用 C99，定义 `USE_NEWC_VEHICLE`，查找 cJSON、nanomsg、skes-linker、skes-log、curl、OpenSSL、zlib、collections-c、e2fsprogs 等包；应用由顶层 `skes/CMakeLists.txt` 经 `skes-apps/CMakeLists.txt` 纳入构建。安装规则把可执行文件和两个 INI 放到 `apps/skes-aeb2vcu/`，依赖库放到 `lib/`。实际目标设备绝对路径取决于顶层安装前缀。

## 7. 当前源码的边界与核验重点

| 事实 | 对分析或验证的影响 |
| --- | --- |
| `com_output` 及头文件 DO 命令结构存在，但主流程无调用。 | 不能描述为当前已经通过串口发送 DO 或叉车控制命令。 |
| 头文件中的 `FORKLIFT_BRAKING_THRESHOLD_3M/5M` 和 `BRAKE_DEPTH_100/50_PERCENT` 等常量未在当前发送分支引用。 | 叉车实际制动值来自告警 `level`、方向和 INI 参数；不能把这些未用常量解释为已生效的距离触发阈值。 |
| 多个 `static int` 解析函数正常路径缺少显式 `return`；`di_loop` 的 `void*` 函数末尾也没有显式返回。 | 返回值行为需按编译器与调用链核验；本文仅依据其明确写入的状态描述功能。 |
| `main` 不调用 `skes_aeb2vcu_stop`；线程创建失败时直接返回；CAN 发送线程的连接失败后直接退出，正常发送时也未检查 `nn_send` 返回值。 | 源码没有完整展示失败恢复及正常停机路径，不能假定它会自动重连或确认每帧发送成功。 |
| `skes_aeb2vcu_stop` 先清 `running`，再依次 `pthread_join` 三个线程；消息轮询线程把 `skes_run` 设为无限等待，而 `stop` 没有调用 `skes_break`。 | 若消息线程正阻塞且没有新的唤醒事件，`stop` 存在等待无法结束的可能；这属于由当前调用关系得出的风险推断，需运行验证。 |
| `skes_aeb2vcu_release` 释放 SKES 句柄及状态对象，本身不停止或等待工作线程。 | 调用方必须确认线程已结束；当前 `main` 未展示完整的停机顺序。 |
| 消息线程、DI 线程和 CAN 线程共用状态，未见加锁。 | 变更字段或排查偶发问题时需考虑并发读写，实际影响需结合目标平台验证。 |
| 履带吊初始化时先打印 `sre->crawler_crane.score_threshold`，随后才把配置值赋给它。 | 因状态对象先清零，这条初始化日志显示的可能是 0，而实际后续过滤阈值是读到的值或默认 0.85；不能只凭这条日志判断阈值。 |
| `di_init` 在连接或设置接收超时失败时关闭套接字，但没有把全局 `di_nn_fd` 重新设为 `-1`；下一次调用遇到非负值便跳过重新创建。 | DI 故障后可能持续使用已关闭的描述符，不能假定服务恢复后本进程会自动恢复查询；需结合套接字状态与返回值核验。 |
| AEB `triggered` 消息只覆盖本次出现的 `front` 或 `rear` 状态，另一方向不自动清零；某些缺字段路径在更新前返回。 | 若上游未发送清除状态或完整方向数组，旧方向可能保留；排查叉车误动作时需要按消息顺序复原状态，不能只看最后一条。 |
| DI 响应解析对 `chn` 和 `params.level` 的存在性检查不完整；当前 cJSON 对缺失数字返回 `NAN`，代码再强制转换为整数。 | 畸形响应不能可靠解释为电平 0 或“查询失败”；必须检查原始响应字段，并在目标环境验证实际方向状态。 |
| `tests/CMakeLists.txt` 构建 `LoadDeviceTypeTest` 和 `Aeb2VcuTest`，仅前者注册到 CTest。 | 名为 `LoadDeviceTypeTest` 的用例实际模拟 `[log] level` 读取，并未调用真正的 `load_device_type`；`Aeb2VcuTest` 是向本机事件端口循环发消息的示例。两者均不能替代车辆协议联调。 |
| `README.md` 只是一段通用消息总线说明。 | 项目运行细节应以本文列出的入口、配置和源码为准。 |

**核验顺序建议：** 先确认安装模板与运行时 INI 的设备类型一致，再确认 19226 消息输入、对应消息头与字段、UI/DI 输入，最后抓取 26002 的 `can_frame_t` 并与车辆协议和实车表现核对。此顺序是针对上述代码依赖关系的排查建议，不等同于已经完成目标设备验证。

## 8. 源码定位索引

| 主题 | 文件与位置 |
| --- | --- |
| 进程入口、日志、信号与版本 | `main.c:58-127`：`init_log`、`sig_handler`、`main`；`skes-aeb2vcu.h:17-20`：`SKES_RELAY_VERSION_*`。 |
| 配置读取与设备实例初始化 | `skes-aeb2vcu.c:125-255`：设备参数读取；`skes-aeb2vcu.c:1122-1175`：`skes_aeb2vcu_new`。 |
| 消息分发与三类事件 | `skes-aeb2vcu.c:477-1119`：`on_msg_filter`、各类 `on_msg_*_parse`、`on_msg_event`、`on_skes_msg_received`。 |
| CAN 帧、串口和 UI 配置 | `skes-aeb2vcu.c:1206-1634`：校验、串口连接、UI 文件和 `do_can_msg_sender`；`skes-aeb2vcu.h:22-140`：帧结构及 ID。 |
| DI 查询与线程生命周期 | `di_get.c:11-152`：初始化、响应解析及查询；`skes-aeb2vcu.c:1638-1719`：`di_loop`、`skes_aeb2vcu_run/stop/release`。 |
| 上游通信实现 | `skes-core/skes-utils/utils.c:1309-1342`：`skes_linker_init`；`skes-3rd-party/skes-linker-git/skes-linker.c:501-625`：消息接收和轮询。 |
| 消息头与缺失数字字段 | `skes-3rd-party/skes-linker-git/skes-msg.c:22-58`：头部 `type/data` 检查；`skes-3rd-party/cjson-git/cJSON.c:109-118`：非数字返回 `NAN`。 |

## 9. 设备异常时如何排查本程序

### 9.1 先定位故障落在哪一段

```text
设备异常
  |
  +-- 程序未运行/启动后退出 -> 查工作目录、INI、启动报错
  +-- 程序在运行，上游消息未到达 -> 查 19226、主题和消息封装
  +-- 消息到了，状态条件不满足 -> 查 JSON、UI 开关、DI、阈值
  +-- 条件满足，26002 无帧 -> 查发送线程、设备类型、连接和编码
  +-- 26002 有正确帧，车辆无动作 -> 查 CAN 服务、物理总线和 VCU
```

**文字版流程：** 先确认运行的确实是本程序及其实际配置，再确认消息到达订阅端且满足解析条件；然后核对 UI、DI、阈值和告警状态；接着在本机 CAN 服务接收端查 `can_frame_t`；最后对照物理 CAN 与 VCU 记录。TCP 连接存在不证明应用消息到达或 `nn_send` 成功。这个顺序按源码数据流划分责任边界，实际结论需有同一时间段的设备证据。

### 9.2 先保存现场证据

记录故障时间、车型、实际运行的二进制、当前行驶方向、告警现象，并保存故障前后的日志与输入/输出样本。优先只读检查；改配置、重启、注入告警或制动指令应放在受控测试流程中。

```sh
pgrep -af skes-aeb2vcu
ps -T -p <PID>
readlink /proc/<PID>/exe
readlink /proc/<PID>/cwd
ls -l /proc/<PID>/fd
ss -tnp
```

把 `<PID>` 换成进程号。`ps -T` 可辅助确认线程是否明显减少，但线程未命名，不能仅凭数量断定是哪一个线程；`readlink` 核对实际可执行文件和工作目录，`fd` 列表辅助检查打开资源；`ss` 查看与 19225、19226、26002、26006、26007、38000 的连接，连接状态不能当作业务收发证据。目标板缺少某个命令时可用等价只读工具。日志文件名 `skes-aeb2vcu.log` 是相对路径，先在进程工作目录定位；源码设置单文件最大 5 MiB、文件数 4。再读取同一工作目录的 `skes-aeb2vcu.ini` 和装载机的 `/tmp/ui_config.json`，与设备安装模板比对。

**日志级别：** 三份模板均设置 `[log] level=4`。`skes-log.h` 定义 0=DEBUG、1=INFO、2=NOTICE、3=WARNING、4=ERROR；日志宏以 `当前等级 <= 对应等级` 判断，故等级 4 下多数 INFO/NOTICE/DEBUG 诊断行不会出现。`SKES_LOG_ALL` 的条件使用等级 6，等级 4 下仍可能出现 `[ALL]`。部分 `nn_socket`、`nn_connect`、DI 超时提示由 `printf`/`fprintf` 写向标准输出或标准错误，可能不在应用日志中。`SIGUSR1` 每次把内存日志等级加 1，到最大值后回到最小值；发送一次不会直接打开 DEBUG，也不会写回 INI。排查前应记下当前等级，避免把缺日志误判为代码没运行。

### 9.3 按症状查证

| 症状 | 核对项目 | 判断边界 |
| --- | --- | --- |
| 进程不存在或启动后退出 | 查看工作目录、INI、`invalid device type`、`failed to create thread`，以及标准输出中的 `sizeof(aeb_fault_t) != 8`、`nn_socket`、`nn_connect`。 | `main` 在实例初始化或建线程失败时返回；CAN 线程自行退出不一定使主进程退出，所以进程还在不等于 CAN 发送还在。 |
| 进程在，但无 CAN 输出 | 确认本进程是否连接 26002；在 CAN 服务接收端抓取 `can_frame_t`，再反查设备类型、输入和使能状态。 | 26002 已连接不能证明 `nn_send` 成功；本程序未检查 CAN `nn_send` 返回值。 |
| 装载机无 AEB 动作 | 查 `/tmp/ui_config.json` 的 `aebEnabled`，以及 `filter/properties` 的 `data.type=AEB`、五种属性名和值；核对 0x168B9664 和扩展帧标志。 | `aebEnabled=false` 时属性被忽略且控制字段会被清零；文件读取失败时保留旧开关值，首次启动的旧值是假。后桥电流还需检查 `uint8_t` 转换问题。 |
| 装载机无声光提示或方向不符 | 查 `alarm.enabled`、BSD `status`、每个 `camera.position/status`，并连续观察声音字段。 | 当前只编码后/左/右为 1/2/3；非零提示下一帧先发 0，前方 BSD 状态未用于该声音字段。 |
| 有故障却见 CAN 故障码 0 | 依第 4 节核对 `source`、`position/type`、输入 `code` 与固定 CAN 故障码表；连续看消息和 CAN 帧。 | 输入 `code` 不直接作为 CAN 码；该帧为 0 表示发送线程当轮未找到活跃表项，但不能单凭这帧证明上游故障已经解除或事件已被正确解析。 |
| 叉车不制动或方向相反 | 查 `[dev] type=forklift`、`brake_motor_version`、前后两级值、`forward_di_level`；同时抓 DI 通道 1 和 AEB 前后位置、`trigger/level`。 | 前触发只与前进方向匹配，后触发只与非前进方向匹配；方向初值为假，DI 失败时沿用旧值，没有“方向未知”分支。 |
| 叉车解除延迟或等级切换异常 | 连续记录 `triggered/normal`、前后方向、等级、DI 值与 0x440/0x120 帧，核对距上次触发是否不足 1 秒。 | 源码有 1 秒保持窗口，保持分支不重查方向；`triggered` 只更新消息中出现的方向，`last_aeb_trigger_level` 也不在 `normal` 时直接清零。需看完整时间序列。 |
| 履带吊配重为 0、不更新或保留旧值 | 查 `event/event` 中的 `source=counterweight-recognition`、`status`、通道 4/5、各项 `text/score` 和阈值。 | 无新事件时保留旧值；非 `triggered` 状态会清零对应通道；低分、非正数和错误通道不会形成有效配重。初始化时打印的阈值先于赋值，不能只看那一行日志。 |
| DI 报错、查询慢或方向异常 | 查 38000 服务及完整响应，逐项确认 `status.io[]` 的 `dev=di`、`chn=1`、`params.level` 确实存在且是数值。 | 接收超时设为 5 秒，循环末尾再睡 1 秒；显式失败时保留上次方向。连接初始化失败后描述符未重置；畸形数字字段还可能走到不可靠的整数转换。所有设备类型都会运行 DI 线程。 |
| 26002 上帧正确，车辆仍无动作 | 对照本机 CAN 服务输入、物理总线抓包、VCU 收到的 ID、扩展标志、DLC、8 字节数据。 | 若本机输出与源码和协议一致，下游丢帧或 VCU 条件也可能造成无动作；用同一时间段证据划分责任。 |

### 9.4 关键日志如何解释

| 日志或输出片段 | 源码含义与下一步 |
| --- | --- |
| `load 'skes-aeb2vcu.ini' failed` / `invalid device type` | 配置路径、文件内容或设备类型解析失败。`skes_aeb2vcu_new` 不因这些读取错误直接退出；类型可能保留装载机初值。 |
| `front_brake_level... failed`、`rear_brake_level... failed`、`brake_motor_version failed` | 叉车参数逐项加载，在第一次失败处提前返回。已经读到的字段会保留，未读字段沿用局部默认值：电机版本 1、前后两级均 255。不能把它当作整个配置原子失败。 |
| `failed to read ui_config` | 读取或解析 `/tmp/ui_config.json` 失败，本轮保留旧开关值。可解析的 JSON 若缺字段，局部布尔初值为假，不一定产生该错误。 |
| `missing type or data in header or incorrect format` / `invalid JSON` | JSON 解析或消息头校验未通过。当前回调必需的根字段是 `type` 和 `data`，`version/id/ts/msg_id/action` 不是这一检查的必需项；继续核对主题、完整负载及内部业务字段。 |
| `Get invalid channel` / `Get score too low` / `Get text value invalid` | 履带吊配重事件被通道、分数或数值条件排除；后两条是 INFO，等级 4 下通常看不到。 |
| `di recv error or timeout` / `failed to get di level` | DI 没有有效结果；前一条来自标准输出。检查 38000 及响应字段。 |
| `aeb keep last trigger` / `aeb front trigger` / `aeb rear trigger` | 叉车保持或重算分支的 INFO 日志；默认等级 4 下不出现，不能以缺日志证明分支未走。 |
| `nn_connect` / `nn_socket` / `sizeof(aeb_fault_t) != 8` | 初始化或发送线程遇到失败；最后一项会调用 `exit(-1)`。这些提示经 `printf` 输出，应查标准输出的收集位置。 |

### 9.5 用输入、程序输出和下游证据划分责任

1. **输入边界：** 从消息生产者和本程序订阅侧记录同一时段的主题、完整 JSON 和时间。`skes_msg_parse` 使用“1 字节主题长度 + 主题 + 4 字节负载长度 + JSON 负载”的封装；只说“上游已经发布”不足以证明本进程收到，原始 TCP 字节也不能直接当纯 JSON。若输入不满足第 4 节条件，先检查消息源、主题和字段。
2. **程序边界：** 核对实际配置、UI/DI 输入及设备类型，再在 CAN0 TX 服务接收端观察本进程的 `can_frame_t`。输入满足条件却长期没有对应帧，重点查进程内的发送线程与连接；有帧但字段不符合第 5 节编码规则，才有更直接的本程序编码问题证据。
3. **下游边界：** 将本机 `can_frame_t`、CAN 服务转换后的物理帧和 VCU 侧记录按时间对齐。缺少这些同场景证据时，不宜把“车辆没动作”直接归结为某一层。需要主动复现时，应使用经批准的台架或受控测试流程，避免在运行车辆上直接注入制动或伪造告警。

**本章依据：** 配置和默认值见 `skes-aeb2vcu.c:125-255,1122-1175`；消息解析见 `477-1119`；CAN 与 DI 输出见 `1372-1660`；DI 响应解析见 `di_get.c:46-152`；日志条件见 `skes-3rd-party/skes-log/skes-log.h:223-283`；总线封装见 `skes-3rd-party/skes-linker-git/skes-linker.c:440-530`；消息头必需字段见同目录 `skes-msg.c:22-58`。排查方法由这些代码路径推导，端口服务、物理 CAN 和 VCU 行为仍需目标设备验证。
