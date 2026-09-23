# hdx_aux 项目说明

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/rtms_sdk`；本项目源码：`/home/tronlong/lyp/code/rtms_sdk/apps/hdx_aux`。SDK 顶层 `CMakeLists.txt` 通过 `apps/CMakeLists.txt` 构建此子目录。
- 编写本文档时的本地分支：`rk3568_ubuntu_20241218`，提交：`dcd34abb4ec5`；该分支跟踪 `origin/rk3568_ubuntu_20241218`。切换分支或更新提交后，应重新核对文档中的路径、入口和端口。
- 语言与产物：C 项目；`apps/hdx_aux/CMakeLists.txt` 生成 `hdx_aux` 可执行文件，版本号为 1.0，并安装到 `usr/local/bin`。
- 主要用途：在设备上维持辅助网络接口，响应 CAN、RS485、RS422 的产测报文；同一份 `main.c` 还包含以 `hdx_dev` 名称调用的产测命令。
- 本文档依据当前源码整理。设备镜像、systemd 服务及硬件连线没有在本目录完整定义，相关行为须以目标设备为准。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| SDK、RK3568、交叉编译 | SDK 是包含多个应用、第三方库和工具链配置的完整工程；RK3568 是目标硬件平台。交叉编译指在开发机生成供目标设备运行的程序。 |
| CMake、`hdx_aux` | CMake 按顶层和本目录的 `CMakeLists.txt` 生成构建规则；`hdx_aux` 既是构建目标名，也是常驻入口识别的程序名。 |
| `hdx_dev`、产测 | `hdx_dev` 是 `main.c` 识别的另一个程序名，进入按命令执行的出厂检测流程；它没有独立的 CMake 目标。 |
| CAN | 控制器局域网络，设备间的帧通信总线。此项目通过本机 nanomsg 端口收发 CAN 帧，由其他服务负责实际总线接入。 |
| RS485、RS422 | 两类串行通信电气接口。本项目通过 `/dev/ttyS*` 设备节点读写，常驻线程处理约定的产测字节序列。 |
| nanomsg、PUB/SUB | nanomsg 是消息通信库；PUB 是发布，SUB 是订阅。本项目连接到本机 TCP 端口接收 CAN 消息，并向另一个端口发布响应。 |
| `br0`、`wlan0`、AP | `br0` 是网桥接口，`wlan0` 是无线网卡接口；AP 指 Wi-Fi 接入点。常驻入口会配置它们并调用 hostapd。 |
| systemd、syslog | systemd 管理设备上的服务，源码通过 `systemctl` 操作部分服务；syslog 接收 `LOG_*` 宏输出的日志。 |
| Modbus、IEC 101/104、MQTT | 分别是工业设备通信协议、电力通信协议和消息发布订阅协议。仓库中有相关源码，但不能据此推断常驻入口已启动对应模块。 |
| cJSON、cn-cbor、curl | 构建时依赖的 JSON、CBOR 数据处理及网络传输库。JSON 配置解析代码位于 `src/config/`。 |
| SPI、MCU | SPI 是外设串行总线，源码定义了 SPI 设备；MCU 是微控制器，`rtc` 产测会设置其时间。 |

## 入口与实际运行流程

`main.c` 通过 `argv[0]` 中的名称选择运行分支：

| 名称 | 行为 |
| --- | --- |
| `hdx_aux` | 调用 `init_aux_can()`、`init_aux_rs485()`、`init_aux_rs422()` 创建线程；随后每秒调用 `init_aux_eth()` 和 `init_aux_wifi()`。 |
| `hdx_dev` | 根据第一个参数执行一次产测，再退出。支持 `id_secret <值>`、`rtc <Unix 秒数>`、`eth`、`can`、`rs485`、`rs422`、`enc`、`gps`、`wifi`、`cellular`、`touchscreen`。 |

`id_secret` 写入 `/opt/conf.ini`，`rtc` 修改系统及 MCU 时间。其余命令会访问串口、网络、显示等设备资源，并可能停止或启动系统服务。源码中的多数产测函数会打印成功或失败；`main()` 最后仍返回 0，调用者不能只凭进程退出码判断产测结果。

当前 CMake 只生成、安装 `hdx_aux`，并未定义名为 `hdx_dev` 的独立目标或链接。`chs/factory.sh` 使用 `./hdx_dev`；该名称在设备上的提供方式需要查看镜像或部署流程。

## 源码导航

| 路径 | 内容与状态 |
| --- | --- |
| `main.c` | 两种入口、启动流程、各项产测函数。先从这里追踪实际调用。 |
| `src/can_server/` | 订阅两个 CAN nanomsg 接收端口，对约定测试帧发回响应；常驻入口会启动。 |
| `src/serial/` | 串口打开与参数设置；常驻入口会启动 RS485 与 RS422 测试线程。 |
| `src/config/`、`src/iniparser/` | JSON 通用配置解析和 `/opt/conf.ini` 中设备 ID、密钥读取；这些源码参与编译，但不是 `hdx_aux` 主循环的直接初始化步骤。 |
| `src/linkLayer/`、`src/secLayer/`、`src/iecLayer/`、`src/ioLayer/` | 链路、安全、IEC 与 I/O 层代码；参与编译，不要据此推断常驻入口已启动这些层。 |
| `src/modbus_server/`、`src/mqttLayer/`、`src/MQTTPacket/` | Modbus 与 MQTT 相关实现；`start_modbus_server()` 未由当前 `main()` 调用。 |
| `src/data_process/`、`src/scene/`、`src/scene_lib/`、`src/app_server/`、`src/http/`、`src/list/`、`src/common/` | 数据处理、场景、应用服务及公共工具。 |
| `chs/` | 产测脚本、随附程序和 Qt 运行资源。`chs/factory.sh` 是设备端脚本，并非 CMake 构建入口。 |

## 编译与安装

从 `rtms_sdk` 顶层构建，不要只看本子目录的依赖。顶层 `README.md` 的 RK3568 流程为：

```sh
cd rtms_sdk
# 先按本机工具链位置修改 cross-profiles/rk3568-crosstool-env-init 中的 TOOLCHAIN_DIR
source cross-profiles/rk3568-crosstool-env-init
cmake -S . -B build_rk3568 \
  -DCMAKE_TOOLCHAIN_FILE=cross-profiles/toolchain_aarch64-rk3568.cmake \
  -DCMAKE_INSTALL_PREFIX=./package_install
cmake --build build_rk3568 --target hdx_aux
# 需要整个 SDK 安装包时，再执行 cmake --install build_rk3568
```

- `rk3568-crosstool-env-init` 设置编译器及 `QL_MODULE_PLATFORM=RK3568_UBUNTU`。`hdx_aux/CMakeLists.txt` 的交叉编译分支只接受这个平台值。
- CMake 至少需要 3.11。此目标查找 nanomsg 1.2、cjson 1.7.15、cn-cbor 1.0、curl 7.69.1，并链接 pthread、rt、stdc++ 等。SDK 的 `3rdparty/` 提供相关构建定义。
- `CMakeLists.txt` 递归收集 `src/*.c`，也递归收集头文件目录作为包含目录。新增源文件通常无需逐个登记，但会直接进入可执行文件。
- 安装规则还复制 nanomsg、curl、cjson、cn-cbor 的共享库到 `usr/local/lib`；构建时生成 `hdx_aux_startup_level.json`，启动级别为 1。
- 当前目录没有独立的 CTest 或单元测试构建目标。`src/secLayer/s1161y_test.c` 属于递归编译的源文件，不能据文件名认为存在自动化测试入口。

## 端口、文件与设备资源

| 资源 | 来源与用途 |
| --- | --- |
| `tcp://127.0.0.1:16002`、`:16003` | CAN0、CAN1 的 nanomsg 订阅端口；`26002`、`26003` 用于发送响应。 |
| `/dev/ttyS3`、`ttyS4`、`ttyS7`、`ttyS8` | RS485 常驻测试线程，配置为 115200、8 数据位、1 停止位。 |
| `/dev/ttyS5` | RS422 常驻测试线程；管理工具相关代码也定义了 `/dev/ttyS8`。 |
| `/dev/ttyUSB1`、`usb0` | 蜂窝网络产测所用设备与接口。 |
| `br0`、`wlan0`、`wlan0-vxd` | 以太网及 Wi-Fi 初始化和产测；地址与接口名在 `main.c` 中写死。 |
| `/opt/conf.ini` | `hdx_dev id_secret` 写入 `[dev]` 段的 `id`、`secret`；`src/config/ini_process.c` 读取这两个字段。 |
| `/root/hostapd.conf`、`/root/wpa_supplicant.conf`、`/root/rtl8189es.ko` | Wi-Fi 配置及驱动路径。 |
| `/root/chs/` | 蜂窝和显示产测调用的脚本与程序所在目录。 |
| `/usr/local/etc/iec_app_general.json`、`iec_app_pointsheet.json` | 部分编译模块使用的配置路径；当前 `hdx_aux` 常驻入口没有直接加载。 |

其他源码还定义 `16005`（GPS）、`16006`、`16007`、`16030`、`16031`（Modbus）以及 `16008`、`26008`（I/O 层）等端口。这些模块的存在不表示当前主入口会连接或监听它们。日志宏统一写入 syslog；产测函数也使用 `printf`。

## 修改与排查要点

1. 先看 `main.c` 的 `argv[0]` 分支，再判断修改影响常驻流程还是产测命令。追踪调用链时区分“参与编译”和“实际运行”。
2. CAN 报文处理依赖本机 nanomsg 服务端；串口、Wi-Fi、蜂窝与显示逻辑依赖目标设备。开发主机上直接启动会改动网络与服务状态，也无法代表设备行为。
3. 修改设备路径、端口或协议报文时，同时检查 `main.c`、相应 `src/` 模块以及 SDK 中提供这些端口的服务。
4. `src/can_server/can_server.c` 定义了两个 `nn_pollfd`，但 `can_msg_process()` 当前以数量 3 调用 `nn_poll()`。触碰 CAN 接收流程时应优先检查这个越界风险。
5. `main.c` 的若干产测过程会执行 `systemctl stop/disable`、`killall`、`ifconfig` 等命令；仅在隔离的目标设备环境执行。查看日志时同时检查 syslog 和标准输出。
