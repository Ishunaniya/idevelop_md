# iec_app 项目说明

## 项目位置与用途

- Git 仓库根目录：`rtms_sdk`；本项目源码目录：`rtms_sdk/apps/iec_app`，由 `apps/CMakeLists.txt` 纳入 SDK 构建。
- 编写本文档时检出的 Git 分支：`rk3568_ubuntu_20241218`。这是当前工作树的分支名，切换分支后应重新核对本文档与源码。
- 这是面向 RK3568 Ubuntu 设备的 C 语言应用。它订阅本机 CAN、Modbus 和 GPS 数据，依据点表处理数据，并通过 IEC 101、IEC 104 或不同地区的 MQTT 协议与上级平台通信；还保存部分历史数据。
- 本文档依据当前分支的源码整理。运行依赖目标设备上的配置文件、其他本机数据发布进程、安全芯片及网络环境，具体取决于所选协议。

### 相关技术和名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| RK3568、Ubuntu | 目标硬件平台和 Linux 系统；CMake 的交叉编译分支要求 `QL_MODULE_PLATFORM=RK3568_UBUNTU`。 |
| IEC 101 / IEC 104 | 电力远动通信协议；本项目通过 `src/iecLayer/` 处理报文。浙江 IEC 104 是该项目实现的扩展分支。 |
| MQTT | 面向平台的数据发布和订阅协议；`mqtt` 对应源码中的四川旧协议，`mqtt2` 对应四川新协议，另有多个地区变体。 |
| CAN、Modbus | 设备数据来源；此应用通过 nanomsg 接收本机其他进程发布的帧，再映射为点表中的数据。 |
| nanomsg | 本机进程间消息库；CAN、Modbus、GPS 接入代码使用其订阅套接字连接 `127.0.0.1` 上的端口。 |
| 点表 | `iec_app_pointsheet.json` 中的数据映射配置，用于把采集值组织为对外通信的数据点。 |
| COA、IOA | IEC 报文中的公共地址和信息体地址；本项目用它们定位设备及数据点。 |
| SQLite | 嵌入式数据库；本项目用它把历史数据写入 `/root/iec_app.db`。 |
| CMake、交叉编译 | CMake 负责生成构建和安装规则；交叉编译是在开发机上生成可在目标设备上运行的程序。 |

## 主要文件

| 路径 | 职责 |
| --- | --- |
| `CMakeLists.txt`、`global_config.h.in` | 定义 `iec_app` 目标、版本、依赖、安装内容及启动级别。 |
| `main.c` | 加载配置、初始化数据管理，启动 CAN、Modbus、存储及消息任务，再按 `remote_proto` 启动 IEC 或 MQTT 通信。 |
| `src/linkLayer/`、`src/secLayer/`、`src/iecLayer/`、`src/mqttLayer/` | 链路、安全、IEC 和 MQTT 协议实现。 |
| `src/ioLayer/`、`src/data_process/`、`src/can_server/`、`src/modbus_server/` | 点表映射、数据处理、CAN 与 Modbus 消息订阅解析。 |
| `src/config/`、`src/iniparser/`、`src/db/`、`src/http/` | 配置解析、INI、SQLite 和 HTTP 登录等功能。 |
| `src/app_server/`、`src/scene/`、`src/scene_lib/`、`src/common/` | 应用消息、场景处理及公共代码。 |
| `etc/` | 通用配置、点表、浙江参数示例及参数检查脚本。 |
| `docs/`、`tool/` | 协议及需求文档、点表工作簿和配置工具。 |

## 构建与部署

- 项目要求 CMake 3.11 及以上，依赖 nanomsg、cjson、cn-cbor、curl、sqlite；链接还使用 pthread、rt 和 stdc++。交叉编译分支要求 `QL_MODULE_PLATFORM=RK3568_UBUNTU`。
- 从 `rtms_sdk` 顶层构建。顶层 `README.md` 给出的 RK3568 流程是先加载 `cross-profiles/rk3568-crosstool-env-init`，再用 `cross-profiles/toolchain_aarch64-rk3568.cmake` 配置 CMake，执行 `make` 和 `make install`。
- CMake 安装 `iec_app` 到 `usr/local/bin`，安装三个 JSON 文件到 `usr/local/etc`，并安装相关动态库。构建时生成启动级别为 `1` 的 `iec_app_startup_level.json`。
- `src/` 下新增 C 文件会自动加入构建；新增头文件所在目录也会自动加入包含路径。

## 运行与修改时关注

- 程序启动时读取 `/usr/local/etc/iec_app_general.json`，初始化数据管理和存储任务，并启动 CAN、Modbus 及应用消息线程。点表 `/usr/local/etc/iec_app_pointsheet.json` 加载失败会导致初始化失败。
- CAN 订阅通道使用本机端口 `16002`、`16003`、`16004`；Modbus 使用 `16006`、`16007`、`16030`、`16031`；GPS 使用 `16005`。这些端口由本机其他进程发布数据，本程序在相应模块中连接并订阅。
- 存储任务使用 SQLite 文件 `/root/iec_app.db` 的 `history_table`，按约 10 秒的间隔记录数据；运行环境需允许创建和写入该文件。
- `remote_proto` 决定 IEC 101、IEC 104、浙江 IEC 104，或 MQTT 及地区变体的启动分支。IEC 分支还使用 `secure_mode`、`tcp_mode`、`server_mode` 和链路/地址参数。修改协议选择时，同时检查 `main.c` 的分支和对应的协议层实现。
- MQTT 分支包含普通、`mqtt2` 及宁夏、湖南、福建、河南、新疆、冀北、重庆等变体。`mqtt2` 和冀北分支在非测试 JSON 模式下先执行 HTTP 登录；冀北分支还读取安全芯片序列号。配置及设备依赖需按所选协议核对。
- 浙江 IEC 104 分支读取 `/usr/local/etc/iec_app_zj_paras.json`，并启动独立的历史快照任务。`data_mng.h` 还定义 `/usr/local/etc/iec_app_idtype.json`：读取失败只记录警告，但当前 `CMakeLists.txt` 未安装该文件；调整配置或部署时需核对设备镜像提供方式。
- `main.c` 还依据可执行文件名 `s1161y_test` 进入测试入口。它会初始化硬件，适合在目标设备上运行。
- 应用消息任务接收 GPS 数据，还会尝试读取 `/root/car_fei_3d/data/hmi_work_stats.json` 更新工作时长和次数；缺失该文件时对应数据不更新。
- 修改点表、协议字段或配置项时，联查 `etc/` 中的样例、`src/config/`、`src/data_process/` 和相应的协议模块。涉及设备路径、网络和数据库时，应在目标环境验证。
- 本项目的 `CMakeLists.txt` 未定义自动化测试目标；`src/secLayer/s1161y_test.c` 是安全芯片相关测试入口，不等同于覆盖各协议的测试套件。
