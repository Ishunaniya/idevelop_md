# modbus_app 项目说明

## 项目位置与用途

- 源码目录：`rtms_sdk/apps/modbus_app`，由 SDK 的 `apps/CMakeLists.txt` 纳入构建。
- Git 仓库根目录：`/home/tronlong/lyp/code/rtms_sdk`。2026-09-23 核对时，当前检出分支为 `rk3568_ubuntu_20241218`，跟踪 `origin/rk3568_ubuntu_20241218`。分支是工作区状态，切换分支后应重新核对本文与源码。
- 这是一个 C 语言常驻程序：按 JSON 配置轮询最多 4 路串口上的 Modbus RTU 设备，再通过 nanomsg PUB 发布采集结果。
- 本文档依据当前源码编写。串口设备名和网络端口是代码中的目标设备约定，部署时应与实际硬件核对。

## 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| Modbus RTU | 通过串口传输的 Modbus 二进制帧格式；程序发送读请求并解析设备响应。它不是程序对外发布数据所用的格式。 |
| 主站、从站、站号（`slave_id`） | 本程序主动发请求，扮演主站；串口上的设备响应请求，扮演从站；站号用于选择目标设备。配置站号为 0 时，代码会探测 1 至 15。 |
| 功能码（`function_code`） | 标识读取对象；代码支持 1（线圈）、2（离散输入）、3（保持寄存器）、4（输入寄存器）。线圈和离散输入按位读取，寄存器按 16 位单元读取。 |
| 地址与数量（`read_address`、`read_quantity`） | 请求中的起始地址和读取数量。地址直接写入 Modbus 请求，不自动转换常见的 0xxxx、3xxxx、4xxxx 显示编号。 |
| CRC16 | Modbus RTU 帧尾的校验值；程序给请求附加 CRC，并校验收到的响应。 |
| 串口参数 | 波特率、数据位、停止位和奇偶校验，必须与实际从站的通信设置匹配；设备节点固定写在 `src/modbus_forward.h` 中。 |
| 采集点、通道 | 一个采集点对应一组站号、功能码、地址和数量；一个通道对应一个 `comN` 配置、一个串口线程和一个发布端口。 |
| nanomsg PUB/SUB | 发布/订阅消息机制。程序使用 PUB 绑定 TCP 端口，接收方用 SUB 订阅；发布内容是本项目的原始结构体数组。 |
| `modbus_frame_t` | 程序内部和发布数据使用的 C 结构体，包含站号、功能码、地址、数量及最多 8 字节数据；其内存布局是接收方解码的依据。 |
| syslog | Linux 日志接口；程序以 `modbus_app` 为标识记录启动、串口和通信信息。 |
| CMake、交叉编译 | CMake 组织 SDK 构建；交叉编译是在开发机上生成目标 RK3568 环境的程序，需使用 SDK 指定工具链。 |
| 启动级别 | 构建生成的 `modbus_app_startup_level.json` 中的数值，本项目设为 1；SDK 的 `generate_img.sh` 会读取它并写入更新文件信息的 `runlevel` 字段。它不是 Modbus 协议参数。 |

## 数据流

1. `main.c` 调用 `modbus_forward_start()`；它从固定路径读取 JSON，初始化通道并创建 4 个线程。
2. 启用的通道按配置点顺序发送 Modbus RTU 读请求，串口收取响应后检查站号/功能码、字节数、总长度和 CRC。
3. 有效响应被转成进程内的 `modbus_frame_t`，按通道缓存，再经 nanomsg PUB 发送。消息可能含多个连续的结构体，发送的是原始内存布局，不是 JSON 或标准 Modbus TCP 帧。
4. 主线程每 10 秒休眠一次，保持进程运行；日志由 `syslog` 输出。

## 主要文件

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt` | 定义 `modbus_app` 目标、依赖、启动级别及安装内容。 |
| `main.c` | 记录版本信息、启动转发线程并维持主进程运行。 |
| `src/modbus_forward.c`、`src/modbus_forward.h` | 加载配置、构造 Modbus RTU 请求、校验响应、按通道发布数据。 |
| `src/serial.c`、`src/serial.h` | 打开与配置串口，处理串口写入。 |
| `src/ipc_pubsub.c`、`src/ipc_pubsub.h` | nanomsg PUB/SUB 端点封装。 |
| `src/sany_list.c`、`src/sany_list.h` | 保存轮询点的链表实现。 |
| `etc/modbus_app_config.json` | 默认串口参数及轮询点配置，安装到 `/usr/local/etc/`。 |
| `tool/` | 点表转换脚本、示例点表、订阅调试工具及其构建脚本。 |

## 构建与安装

- 项目要求 CMake 3.11 或更新版本，查找 nanomsg 1.2 和 cjson 1.7.15，并链接 pthread、stdc++。交叉编译分支要求 `QL_MODULE_PLATFORM=RK3568_UBUNTU`。
- SDK 顶层 `README.md` 提供 RK3568 构建流程：配置并加载 `cross-profiles/rk3568-crosstool-env-init`，用 `cross-profiles/toolchain_aarch64-rk3568.cmake` 配置 CMake，再运行 `make` 和 `make install`。
- 安装目标包括 `/usr/local/bin/modbus_app`、`/usr/local/etc/modbus_app_config.json` 及所需 nanomsg、cjson 动态库。构建还会生成启动级别为 1 的 JSON 文件。
- 版本由 CMake 的 `project(modbus_app VERSION 1.5)` 写入生成的 `global_config.h`，启动日志打印主、次版本号。启动级别 JSON 在构建目录生成，`modbus_app/CMakeLists.txt` 本身未声明安装该文件。
- `tool/build.sh` 单独编译 `modbus_tool`，使用 `aarch64-linux-gnu-gcc` 和相对路径下的 SDK 构建产物；它不是主程序的 CMake 构建步骤。

## 配置与运行

- 程序固定读取 `/usr/local/etc/modbus_app_config.json`。顶层键为 `com0` 至 `com3`；每个通道可设置 `enable`、`serial_port`（`baudrate`、`data_bit`、`stop_bit`、`parity`）和 `modbus_point.point`。
- 仓库随附的配置目前包含 `com0`、`com1`、`com2`，三者均设为启用；没有 `com3` 条目。缺少整个配置文件、JSON 解析失败或找不到任何通道条目时，初始化失败，主程序退出。
- 每个点的数组按 `description`、`slave_id`、`function_code`、`read_address`、`read_quantity` 排列。代码读取后四项构造请求；支持读取线圈、离散输入、保持寄存器和输入寄存器（功能码 1 至 4）。
- `enable=1` 的通道需要有有效点数组，空点表会导致该通道禁用。串口参数中的波特率实现支持 2400、4800、9600、19200、38400、57600、115200、230400，其他值回退至 9600；数据位支持 5 至 8，其他值回退至 8；`stop_bit=2` 为 2 位，否则为 1 位；`parity=1` 为奇校验，`2` 为偶校验，其他值为无校验。
- 功能码 1、2 的读取数量被限制为 1 至 64 位；功能码 3、4 被限制为 1 至 4 个寄存器。数量为 0 时改为 1；超上限时截断。设 `slave_id=0` 会尝试站号 1 至 15，首次收到有效响应后固定为该站号。
- 通道与设备节点、发布地址的对应关系如下。端口由 nanomsg PUB 绑定，订阅端可连接这些端口。

| 通道 | 串口 | PUB 端口 |
| --- | --- | --- |
| `com0` | `/dev/ttyS3` | `16006` |
| `com1` | `/dev/ttyS4` | `16007` |
| `com2` | `/dev/ttyS7` | `16030` |
| `com3` | `/dev/ttyS8` | `16031` |

- `modbus_forward_start()` 为 4 个通道创建线程；未启用的线程随即退出。启用的线程轮询配置点，检查响应长度与 CRC，并分批发布采集帧。
- 普通轮询的串口读取超时设置为 1 秒；探测站号时设置为 400 毫秒。每次读到数据或发生读错误后还会按代码等待 200 毫秒。缓存有数据且经过 1 秒，或下一帧会超过 2048 字节时发送；没有有效响应就没有对应采集帧。
- `tool/modbus_xlsx2json.py` 用 Tkinter 和 openpyxl 将 Excel 点表转成 `modbus_app_config.json`；输出到脚本运行时的当前目录。`tool/modbus_tool.c` 是连接上述端口的订阅调试工具。

## 调试与已知边界

- `tool/modbus_tool` 的源码可用 `tool/build.sh` 构建；运行形式为 `modbus_tool recv` 或 `modbus_tool recv debug`。该工具尝试连接全部 4 个 PUB 端口，调试模式逐帧打印字段，适合在目标环境检查发布数据。
- 无法打开串口或创建 PUB 端点时，通道线程会持续重试；日志可按 `modbus_app` 的 syslog 标识查找。源码没有动态重载配置的入口，配置变更后需要重新启动进程。
- 配置加载器只检查部分 JSON 字段是否存在，未完整校验数值类型和取值范围；`serial_port` 对象整体缺失时也不会在该函数中报错。生成或手改配置后，应先检查 JSON 结构和串口参数。
- 响应站号与功能码的判定条件在源码中用 `&&` 连接两个不匹配比较；因此其中一个字段错误但另一个正确时，可能继续进入后续校验。文档描述的是当前行为，不能将这一步视为严格的双字段校验。
- 代码以单次 `read()` 接收响应，依赖串口时序一次拿到完整帧；分段到达的响应可能被判为无效。运行验证需要目标串口、从站及相应网络端口，不能仅凭主机编译结果确认通信。

## 修改时关注

- 修改串口映射、PUB 端口或帧格式时，同时检查 `src/modbus_forward.h`、`src/modbus_forward.c` 和 `tool/modbus_tool.c` 的对应定义。
- 修改点表字段时，同时检查配置加载函数、`etc/modbus_app_config.json` 与 `tool/modbus_xlsx2json.py`。
- 新增 `src/` 下的 C 文件及头文件时，CMake 的 `GLOB_RECURSE` 会自动收集；运行验证仍需目标设备串口及相应从站。
- 修改帧结构或发布方式时，需要同步检查订阅端对 `modbus_frame_t` 大小、字段顺序和字节序的假设；当前源码直接发送结构体字节，没有独立的序列化协议。
