# sound_light_alarm_sep 项目说明

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/skes`；本项目源码目录：`skes-apps/sound_light_alarm_sep`。仓库顶层 `CMakeLists.txt` 进入 `skes-apps/`，后者通过 `add_subdirectory(sound_light_alarm_sep)` 纳入构建。
- 编写本文档时检出的 Git 分支：`main_ui`。分支名表示当前代码版本线，不是运行参数；切换分支后，应重新核对功能、配置及端口。本文档依据该分支核验时的源码整理。
- 本程序是 SKES 系统的声光报警应用。它通过 SKES 消息链路接收人脸识别、安全带、DMS 和 BSD 相关消息，按设备类型决定报警内容，并通过串口、RS485 发送链路或数字输出（DO）控制报警器及授权输出。
- 当前主要区分装载机（`wheel_loader`）和叉车（`forklift`）：装载机在 DMS/BSD 告警时读取 UI 报警开关与音量，向本机串口发送声光报警指令；叉车依据人脸识别与安全带状态控制授权 DO，并按 BSD 方位或 DMS 状态发送对应语音/灯光指令。`crawler_crane` 可被解析为设备类型，但当前发送线程没有对应分支。
- 同仓库的 `sep-aeb-sla` 是后装叉车声光报警与电子油门控制的合并应用；上级 CMake 注释明确提示它不能与本程序及 `skes-aeb2vcu` 同时使用，部署时应核对所选应用。

### 相关技术和名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| Git 分支 `main_ui` | 当前检出的代码分支；本文档的功能说明以该分支的工作树为准。 |
| SKES、SEP | SKES 是上级应用系统；`sep.ini` 中的 `[sep]` 段声明应用名与启用状态，随可执行文件安装，供系统的应用管理流程使用。 |
| CMake、交叉编译 | CMake 定义可执行文件、依赖和安装规则；交叉编译是在开发机上为目标设备构建程序。本项目源码为 C99，测试程序为 C++11。 |
| DMS | Driver Monitoring System，驾驶员监测系统；本程序消费其报警状态，叉车逻辑识别“打电话”“抽烟”等状态。 |
| BSD | Blind Spot Detection，盲区监测；装载机使用总体触发状态，叉车还区分前、后、左、右方位。 |
| 人脸识别、安全带 | 通过 `/skes/filter` 的 `properties` 消息读取 `FaceRecognitionResult` 与 `seatbelt_fasten`；叉车逻辑据此判断是否授权和输出对应提示；源码将识别结果 `1` 视为通过、`-1` 视为失败，其余值进入未确定状态。 |
| RS485、RS232、UART | 串行通信接口。装载机直接使用 `/dev/ttyS1`（9600 波特率、8 数据位、1 停止位、无校验）并切换 GPIO 27/26 的收发方向；叉车将报警指令发给本机串口服务的 RS485 通道。代码也初始化 RS232 发布通道，但当前报警发送调用使用 RS485 通道。 |
| Modbus、CRC16 | 装载机发送 `0x01 0x10` 开头的寄存器写入帧，运行时计算 CRC16 并按低字节在前附加；叉车发送 `0xFF 0x06` 开头的预设 8 字节语音命令。两类帧格式不同，修改时须分别核对设备协议及校验字节。 |
| GPIO、DO | GPIO 是通用输入输出引脚；DO 是数字输出。装载机使用 GPIO 27/26 控制 RS485 方向，启动发送线程时将 GPIO 95/94 设为输出高电平；叉车通过本机 DO 命令服务控制第 2 路输出（枚举值 `DO_CTRL_CH_2=1`）。 |
| nanomsg、PUB/SUB | 本机进程间消息库和发布/订阅模式。程序经 `skes-linker` 消费 SKES 消息，并用 nanomsg `PUB` 套接字向本机串口及 DO 服务发送二进制命令。 |
| cJSON、INI、JSON | cJSON 解析消息和 UI 配置；`sound_light_alarm_sep.ini` 配置设备类型与日志级别；`/tmp/ui_config.json` 提供装载机报警开关及音量。 |
| `triggered`、`normal` | 上游报警 JSON 中的触发和恢复状态。本程序根据 `source`、`status`，以及叉车 BSD 的方位字段更新内部状态。 |

## 主要文件与数据流

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt` | 声明依赖、编译参数、可执行文件与安装规则；按 `SLA_TYPE` 选择安装装载机或叉车配置。 |
| `sep.ini` | 声明 `sound_light_alarm_sep` 应用已启用；安装到 `apps/sound_light_alarm_sep`。 |
| `sound_light_alarm_sep.ini.wheel_loader`、`sound_light_alarm_sep.ini.forklift` | 分别给出设备类型和日志配置；安装时统一重命名为 `sound_light_alarm_sep.ini`。叉车配置另有 `do_trigger_level=0`。 |
| `src/main.c` | 初始化日志、响应 `SIGUSR1` 切换日志级别、创建并运行报警对象。 |
| `src/sound_light_alarm_sep.c`、`.h` | 解析 SKES 消息、维护告警状态、执行装载机和叉车分支、发送串口/DO 指令，定义版本和端口常量；当前源码版本宏为 `0.1.1.0`。 |
| `src/serial.c`、`.h`；`src/gpio.c`、`.h` | 串口参数、写入重试及 sysfs GPIO 操作。 |
| `tests/test.cpp`、`tests/CMakeLists.txt` | 构建 `SLATest` 手工消息发布程序，循环向本机事件端口发送 DMS/BSD 触发和恢复样例；它不包含自动断言。 |

主要数据流：上游识别/监测应用 → SKES `filter` 或 `event` 消息 → `skes-linker` 回调解析 → 更新告警状态 → 按设备类型发送串口报警命令或 DO 命令。装载机另从 `/tmp/ui_config.json` 读取开关与音量。

## 构建与部署

- 由 `skes` 仓库顶层 CMake 统一构建。项目配置 C99，并查找 `e2fsprogs-git`、`openssl-git`、`curl-git`、`zlib-git`、`cjson-git`、`collections-c-git`、`nanomsg-git`、`skes-log`、`skes-linker-git`；链接 cJSON、skes-linker、skes-utils、skes-log、nanomsg、curl、OpenSSL、zlib、pthread、rt。具体工具链与包路径应以仓库构建环境为准。
- 安装规则将可执行文件、`sep.ini` 和按 `SLA_TYPE` 选中的设备配置放到 `apps/sound_light_alarm_sep`，依赖动态库放到 `lib`；设备上的绝对路径由顶层安装前缀决定。`SLA_TYPE=forklift` 安装叉车配置，`SLA_TYPE=wheel_loader` 或其他值（包括未指定）时安装装载机配置。`sep.ini` 的 `param` 当前为空，`main` 不解析命令行参数；构建类型由 `SLA_TYPE` 决定，运行时设备逻辑由安装后的 INI `dev.type` 决定，两者应保持一致。
- 主程序以相对路径读取并回写 `sound_light_alarm_sep.ini` 的 `[log] level`，日志文件名为 `sound_light_alarm_sep.log`（单文件上限 5 MiB、保留 4 个文件）；启动目录需要能找到配置且允许写入。`SIGUSR1` 会在运行中循环切换日志级别，但当前信号处理函数调用了非异步信号安全的日志函数，源码已有待修复注释。
- `tests` 目录可构建 `SLATest`，用途是人工发送事件样例；该程序持续循环发送，未注册为 CTest 自动断言用例。验证真实声光输出还需要目标板的串口、GPIO、报警器、本机消息服务及上游识别应用。

## 运行与修改时关注

- SKES 消息订阅地址为 `tcp://127.0.0.1:19226`，发布地址为 `tcp://127.0.0.1:19225`。实际回调只处理 `filter` 和 `event`：`filter` 仅解析 `properties` 中 `data.type=FaceRecognition` 的属性；`event` 仅解析消息头 `type=alarm` 的 DMS/BSD 项。心跳、版本、设备数据、位置相关分支位于 `#if 0` 中，当前不生效。
- 本机串口服务 RS485 发送端口为 `26006`，RS232 发送端口为 `26007`；DO 命令发送到 `26008`。头文件也定义了对应接收端口，但本程序当前发送路径使用上述发送端口。
- 装载机从 `/tmp/ui_config.json` 的 `alarm.enabled` 和 `alarm.volume` 读取报警控制，音量数值按 `volume / 100 * 30` 换算为设备级别，代码没有做 0 至 100 的输入范围限制。配置读取失败时该循环不发送新的报警指令。报警触发取 DMS/BSD 总状态。源码把重发计数和上次音量变量声明在循环内，每轮重置：换算后音量非零时，持续告警阶段会每轮重新发送触发帧；音量为零时计数无法累积到 15，不能按注释预期周期重发。未触发或关闭时每轮发送关闭帧；详见同目录《sound_light_alarm_sep_源码分析.md》。
- 叉车配置使用 `FaceRecognitionResult`、`seatbelt_fasten`、BSD 方位及 DMS 状态。启动后收到人脸识别消息或等待超过 10 秒才发送初始化提示；只有完成授权后才播放方位和 DMS 警报，选择顺序为 BSD 前、后、左、右，再到 DMS 抽烟、打电话、疲劳。现有 DMS 解析会设置“打电话”“抽烟”“吃东西”“正常驾驶”标志；发送逻辑使用打电话、抽烟和疲劳标志，而疲劳标志在当前解析路径没有赋值；“吃东西”和“正常驾驶”虽被识别，也没有对应发送分支。DMS 收到 `normal` 时只清除总体触发标志，没有清除各细分标志；下次授权后可能再次选中旧状态，修改映射时应一并核对。
- `do_trigger_level` 从叉车 INI 读取，并在 `do_output` 中决定触发时的 DO 电平：非零时触发为高电平，零时触发为低电平。修改配置时应核对外部继电器的有效电平。`crawler_crane` 虽能解析，当前没有输出分支。
- 装载机使用 `/dev/ttyS1` 和 `/sys/class/gpio`，叉车依赖本机串口/DO 服务。普通开发机没有这些设备和服务时，不能据此验证实际声光输出。
- `sound_light_alarm_new` 默认设备类型为装载机；读取设备类型或 `do_trigger_level` 失败时仅记录/忽略错误，初始化仍继续。设备配置应在部署前核对，避免程序按默认分支运行。
- `sound_light_alarm_run` 创建消息轮询、发送、串口接收三个线程，但发送与接收线程复用同一个 `com_pid` 字段；`sound_light_alarm_stop` 因而只能按该字段等待其中一个线程。`main` 的常规退出路径也没有调用 `sound_light_alarm_stop`。若修改退出/重启逻辑，需先梳理线程回收与资源释放。
