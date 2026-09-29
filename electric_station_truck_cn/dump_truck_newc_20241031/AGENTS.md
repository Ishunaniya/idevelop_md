# electric_station_truck_cn 项目说明（新 C 自卸车分支）

## 项目位置与用途

- 源码仓库：`/home/tronlong/lyp/code/electric_station_truck_cn/`；本文档位置：`/home/tronlong/lyp/perCode/idevelop_md/electric_station_truck_cn/dump_truck_newc_20241031/AGENTS.md`。
- 本文依据 Git 分支 `feat/dump_truck_newc_20241031`、提交 `24d5f4f87074b06300a9ad9c856b26c591cf983f` 核对。分支是仓库的开发线，提交哈希才固定本次阅读的源码版本；切换分支或拉取新提交后需重新核对。目录名中的 `dump_truck_newc_20241031` 是资料分类名，不控制程序行为。
- 本仓库生成车载程序 `bs_client`，服务于国内重卡及自卸车换电。程序从本机 CAN/GPS 服务取得车况与电池数据，通过 TCP 连接换电站进行登录、鉴权、心跳、参数设置和属性查询，再把换电状态、唤醒请求与故障数据发送给本机命令服务。新 C 自卸车还向 CAN0 发送 T-Box ID；存在故障等级时发送故障报文。
- 当前分支的 `DEFAULT_VEHICLE_TYPE` 和 `ini/electic_station_dump_truck_newc.ini` 均为 `dump_truck_newc`。运行时真正使用的车型取自设备 `/opt/electic_station.ini` 的 `station:vehicle_type`；缺失时才用默认值。源码同时保留 `dump_truck`、`heavy_truck` 分支，Git 分支名不会自动修改设备配置。
- 主体是 C，日志模块是 C++；通过 Makefile 构建移远模块 Linux 程序。根目录 `Makefile` 链接到 `Makefile-EC200A`，另有 `Makefile-EG25`。`README.md` 只有项目名称，本文件作为本分支的阅读入口。

### 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| Git 分支、提交哈希 | `feat/dump_truck_newc_20241031` 标识开发线；上面的 40 位提交哈希标识本次核对版本。排查行为时应同时记录两者。 |
| `dump_truck_newc`、`dump_truck`、`heavy_truck` | `station:vehicle_type` 的三个车型值。`newc` 在源码中注释为“自卸车新C版本”；新 C 与旧自卸车使用不同的部分 CAN 报文 ID、字段解析及 CAN 发送路径。不要把名称直接当作硬件规格说明。 |
| T-Box、`bs_client`、换电站 | T-Box 是车载通信终端；`bs_client` 是本仓库生成的设备进程；换电站是它通过站端 TCP 地址连接的系统。 |
| EC200A、EG25、SDK、交叉编译 | 两个 Makefile 对应移远模块构建环境。SDK 提供目标平台头文件和库；交叉编译是在开发机上用目标工具链生成设备可执行文件。默认 EC200A 构建定义 `EC200A_ENABLE`。 |
| TCP socket、`bridge0:0`、`io_mng` | socket 用于站端及本机服务通信。站端连接前检查 `bridge0:0` 为 `192.168.100.1`；启动脚本负责配置。本机 CAN/GPS 数据由设备上的 `io_mng` 类服务经回环 TCP 提供，本仓库是客户端。 |
| CAN、J1939、PGN、TP.BAM/TP.DT | CAN 是车内总线。J1939 是重型车辆常用的 CAN 上层协议；PGN 标识报文参数组；TP.BAM 宣告多包数据，TP.DT 承载分包。新 C VIN 接收使用 `0x9CECFF4A` 和 `0x9CEBFF4A`，检查长度 17、包数 3、PGN `0xFEEC` 后拼接 VIN。 |
| VCU、BMS、SOC、BCC1 | VCU 是整车控制器，BMS 是电池管理系统，SOC 是电池荷电状态。`BCC1_*` 是源码中的换电控制、反馈和车辆状态字段；具体电气含义应与车辆协议核对。 |
| DI、DO、锁销、连接器、ACC | DI 是数字输入，程序从 `/tmp/di_in_07`、`/tmp/di_in_08`、`/tmp/di_pwm_09` 读锁销与连接器反馈并反向换算电平。DO 是数字输出，`/tmp/do_06` 用于新 C 自卸车的解锁、锁止控制。ACC 是钥匙电状态，读取 `/tmp/acc_stat`。 |
| VIN、T-Box ID、ICCID | VIN 是 17 字节车辆识别码，可来自配置、新 C 的 J1939 分包或运行时 `/opt/machine_vin`；T-Box ID 从 `/opt/conf.ini` 的 `dev:id` 读取；ICCID 是 SIM 卡标识。EC200A 构建调用移远 SIM SDK 取 ICCID，非 EC200A 编译分支返回固定占位值 `12345678901234567890`。 |
| 登录、鉴权、AES-128 | 站端登录后请求随机数，再用 OpenSSL `libcrypto` 的 AES-128 ECB 路径构造鉴权应答；源码中有固定 16 字节密钥。`src/aes128/` 的自带实现也参与构建，但当前站端鉴权路径调用 OpenSSL。 |
| BCC、J1939 DM1、SPN/FMI | 站端帧尾 BCC 是异或校验字节，与 `BCC1_*` 字段不同。DM1 是 J1939 故障消息；SPN/FMI 标识故障参数和模式。`src/fault/` 把故障掩码映射为 DM1，供新 C 自卸车经 CAN0 发送。 |
| INI、`iniparser` | INI 是按节、键保存的文本配置；`src/iniparser/` 负责解析。设备实际配置为 `/opt/electic_station.ini`，`electic` 是源码中的原样拼写；仓库 `ini/` 只有部署样例。 |
| 换电状态、唤醒请求、故障掩码 | `BCC1_ExchangeState` 表示换电阶段，`BCC1_ExchangePowerBatWakeUpVCUReq` 表示唤醒请求；`fault_mask` 按位保存故障，`fault_level` 表示故障等级。它们经本机命令服务传递，部分状态也用于站端属性应答和 CAN 故障上报。 |

## 主要文件与职责

| 路径 | 职责 |
| --- | --- |
| `Makefile`、`Makefile-EC200A`、`Makefile-EG25`、`sdk.mk` | 选择目标平台、工具链、源码、库和 `bs_client` 版本号。当前两个 Makefile 均包含 `src/fault/`。 |
| `main.c` | 单实例检查、日志和信号初始化；启动站端接收、站端发送、CAN 接收线程，执行约 500 ms 一轮的周期任务。 |
| `src/app_mng/` | 初始化共享状态，读取设备 ID、车型、VIN、ICCID、站端地址和历史换电状态。 |
| `src/batt_stand_client/` | 站端帧打包、解析、登录、鉴权、心跳、参数设置和属性查询；新 C 自卸车的站端解锁、锁止指令也会触发 DO 写入。 |
| `src/can_server/` | 接收本机三路 CAN 和一路 GPS 数据；按车型解析车况、电池与 VIN；新 C 路径向 CAN0 发送 T-Box ID 和 DM1 故障。 |
| `src/cmd/` | 将唤醒、换电状态、故障掩码和等级发往本机命令服务，`Car_Type` 写为 `CHINA_CAR`。 |
| `src/fault/` | 判断 CAN、Wi-Fi 接口、内存、锁销和连接器故障，并映射到故障掩码及 J1939 DM1。当前 GPS/4G 故障判断由 `#if 0` 屏蔽。 |
| `src/common/`、`src/iniparser/`、`src/net_utils/`、`src/skt_res/` | 设备信息、配置读写、网络地址和 TCP 辅助功能。 |
| `src/aes128/`、`src/list/`、`src/log/` | AES 实现、待发消息链表和日志。 |
| `ini/electic_station_dump_truck_newc.ini`、`scripts/start_bs.sh` | 新 C 车型配置样例；设备端网络配置、启动和保活脚本。 |

## 启动与主要数据流

1. `main()` 检查单实例，初始化日志和信号处理，读取设备配置；随后启动站端接收、站端发送和本机 CAN/GPS 接收线程，主线程进入周期任务。
2. 站端接收线程等待 `bridge0:0` 地址为 `192.168.100.1`，连接配置的站端 TCP 地址。发送线程依次处理登录、随机数请求、加密鉴权、心跳和待发消息。登录、鉴权、心跳尝试间隔均为 5 秒；约 60 秒没有有效站端响应会断线重连，登录失败后约 120 秒再尝试。
3. 本机接收线程连接 `127.0.0.1:16002`、`:16003`、`:16004` 三路 CAN 服务及 `:16005` GPS 服务；四路都连接后进入读取循环。断连后重建连接。新 C 车型从专有 CAN ID 解析档位、手刹、SOC、电池容量、里程、累计充放电量和 VIN。
4. 周期任务检查 CAN/BMS 超时和锁销、连接器反馈。新 C 自卸车从 `/tmp/acc_stat` 推导 `VCU_ePTState` 与 `VCU_ExPowerBatAllow`，根据锁销反馈推进换电状态，将变化写回 `station:exchange_state`；处理故障、发送 T-Box ID，故障等级非零时发送 DM1 CAN 帧。命令服务在状态变化或超过约 5 秒后收到一次控制数据。`show_can_data()` 还约每 15 秒记录一次车况，Wi-Fi 接入终端数由设备上的 `hostapd_cli` 查询。
5. 站端参数 `ExchangeState=0x02` 触发解锁并写 `/tmp/do_06` 为 1；`0x04` 触发锁止，只有连接器反馈为连接状态才写 DO 为 0。两种命令均等待锁销反馈后答复，约 10 秒未完成则返回超时。其他有效状态一般直接答复。站端属性查询在鉴权通过后排队回复；非全量查询还设置车辆唤醒请求。

### 换电状态与控制边界

| 状态 | 源码中的含义和处理 |
| --- | --- |
| `0x00`、`0x01` | 无效/初始、已连接站控。`0x00` 时主循环可根据锁销反馈转为解锁完成或锁止完成；`0x01` 的具体动作由站端流程决定。 |
| `0x02`、`0x03` | 准备换电/解锁命令、动力电池卸载中。`0x02` 设置唤醒和解锁控制，锁销反馈为解锁后主循环转 `0x03`。 |
| `0x04`、`0x05`、`0x06` | 电池装配中/锁止命令、电池已放置、电池锁止。锁销反馈为锁止后主循环转 `0x06`。 |
| `0x07`、`0x08` | 换电成功、换电失败；站端下发后清除唤醒请求，失败状态还依据连接器反馈设置故障结果字段。 |

主循环在车辆 Ready、手刹释放且站端断开等条件下，会把换电状态改为 `0x04`；但此处调用 `do_ctrl(0)` 的条件仅是旧 `dump_truck`。`dump_truck_newc` 的这条自动锁止路径**没有直接写 `/tmp/do_06`**，最终动作需要结合本机命令服务、MCU 和 VCU 的实际行为验证。站端断开后重置登录和鉴权，并将 `BCC1_VCUControl` 置为无效值；唤醒请求和部分换电状态有意保留，离线超时约 1800 秒后可清除唤醒请求。

### 新 C 自卸车的 CAN 差异

| 数据 | 新 C 路径 |
| --- | --- |
| VIN | `0x9CECFF4A` 宣告、`0x9CEBFF4A` 分包，PGN `0xFEEC`；成功后更新内存与站端 VIN，并写回 INI。 |
| 档位、手刹 | `0x98F00503` 解析档位；`0x98FEF100` 解析手刹。 |
| SOC、电池编码、额定容量 | `0x9ACE0DF3`、`0x9ACE03F3`、`0x9ACE04F3`。 |
| 累计充放电量、里程 | `0x9ACE15F3`、`0x98FEC117`。 |
| T-Box ID、故障 | 主循环调用 `can_tbox_id_send()`、`can_tbox_fault_send()`，经 CAN0 发送；后者仅在 `fault_level` 非零时发送，故障报文按 DM1 单帧或多帧封装。 |

### 故障判断与上报

- 自卸车故障处理在系统启动满约 60 秒后开始，每约 10 秒检查一次。ACC 开启而 CAN 无数据、`wlan0` 接口未处于 UP、空闲内存低于源码阈值，以及锁销或连接器反馈异常会设置故障位。这里的 `wlan0` 接口状态与站端 TCP 连接状态 `wifiState` 含义不同。
- GPS 和 4G 故障检查及其 DM1 映射在当前提交中由 `#if 0` 屏蔽；GPS TCP 接收仍存在。故障位 34 与 43、35 与 44、36 与 44 在实现中可能由同一反馈条件共同置位，不能把不同故障位直接理解成独立传感器来源。
- 新 C 车型将非零故障等级转换为 J1939 DM1 并发送到 CAN0；所有车型都会把故障掩码与等级交给本机命令服务。故障清除后的零故障等级不会触发 `can_tbox_fault_send()` 发送清除帧，应核对接收方的清故障机制。

## 配置、接口与部署

| 项目 | 当前源码约定 |
| --- | --- |
| `/opt/electic_station.ini` | `station:server_ip`、`server_port`、`vehicle_type`、`vin`、`enable_vin_request`、`exchange_state`。默认车型 `dump_truck_newc`，默认站端 `192.168.100.101:7701`。初始化会写回若干配置；状态变化和成功获取 VIN 也会写回。 |
| `ini/electic_station_dump_truck_newc.ini` | 部署样例设置 `vehicle_type=dump_truck_newc`、`enable_vin_request=true`。需按设备流程放到 `/opt/electic_station.ini`，仓库样例不会自动生效。 |
| `/opt/conf.ini`、`/opt/machine_vin` | 前者 `dev:id` 提供 T-Box ID，缺失时使用源码默认值；后者会在主循环中定期尝试读取 VIN。新 C 初始化阶段不优先读该 VIN 文件，主循环中约每 15 秒尝试一次，直到标记已取得机器 VIN。 |
| 本机端口 | CAN0/1/2 为 `127.0.0.1:16002/16003/16004`，GPS 为 `:16005`，命令服务为 `:16008`；服务端不在本仓库。 |
| 网络与临时文件 | `bridge0:0` 必须有 `192.168.100.1` 才尝试连接站端；`/tmp/wifi_state` 记录站端 TCP 连接状态。`/tmp/acc_stat`、`/tmp/di_in_07`、`/tmp/di_in_08`、`/tmp/di_pwm_09` 是状态来源，`/tmp/do_06` 是控制输出。 |
| 日志与启动 | 日志目录 `/media/sdcard/bs_client_log/`，文件名前缀为 `truck_hdz`。`bs_client [days]` 在启动时清理旧日志，默认 2 天，参数大于 7 也按 2 天处理。`scripts/start_bs.sh` 配置网口、先强制结束已有 `bs_client`，再从 `/opt/bs_client` 启动并约每 20 秒保活。 |
| 构建 | 默认 `make` 用 EC200A 配置；`make -f Makefile-EG25` 用 EG25 配置。两者版本号均为 `1.31`，并把 Git 提交号编入版本日志；链接 pthread、C++ 运行库和 OpenSSL `libcrypto`。`sdk.mk` 指向 `/home/wason/working/ec200a/` 下的 SDK 与交叉工具链；本机核验时这些绝对路径不存在，因此 `make -n bs_client` 只能确认命令展开，**不能算编译通过**。需按实际 SDK 环境修正路径或通过构建环境注入变量。 |

## 修改与验证提示

- 修改车型或 CAN 字段时，联查 `src/app_mng/app_mng.h`、`src/can_server/can_server.c`、`src/can_server/can_frame_pack.c` 和站端属性打包逻辑；运行时用设备上的 `vehicle_type` 确认实际路径。
- 修改站端交互时，联查 `batt_stand_client.c`、`batt_message_pack.c`、`batt_message_parse.c`。外层帧以 `0x23 0x23` 开始，含命令、应答标志、17 字节 VIN、加密标志、长度、数据和异或 BCC；登录、心跳、自定义命令分别为 `0x01`、`0x07`、`0x81`。
- 锁销与连接器反馈经过反向电平换算；新 C 的 DO 控制与故障上报要在目标设备或台架核对。`waiting_acc_key_on()` 打开 `/tmp/acc_stat` 失败后仍继续读取无效描述符，缺失文件可能被当成 ACC 开启；`do_ctrl()` 的时间门限也可能影响快速连续命令。
- 新 C VIN 分包接收检查长度、包数与 PGN，但没有验证三个数据包属于同一轮且顺序完整。本机 CAN/GPS TCP 接收也按本次 `read()` 的完整结构体数量处理，不缓存残留字节；需验证分包边界与乱序情形。
- 站端接收每次最多读取 1440 字节并直接解析，未见跨 `read()` 缓存未完整帧。解析器含同一次读取中处理后续帧的循环，但 BCC 校验使用当前剩余 `message_len`，需要用粘包、拆包报文核验；不能简单认为多个完整帧同批到达一定可正确处理。全量属性查询 `_propNumber=255` 分支计算了完整属性列表，却把请求的 `_propListLen` 传给应答函数；需用站端报文验证结果。
- `src/can_server/can_server.c` 中新 C 累计充、放电量解析连续两次使用赋值，最高字节在下一次赋值时被覆盖；使用该统计值前应与原始 CAN 帧比对。T-Box ID 和故障 CAN 发送函数用 16 位变量保存毫秒时钟，发送节奏应在长时间运行时验证。
- 本仓库未见专用自动化测试目标。先在匹配 SDK 下编译，再在具备站端、本机 CAN/GPS/命令服务和硬件反馈的设备上检查登录鉴权、心跳、VIN、状态上报、解锁锁止、配置写回及 DM1 故障。主机侧编译不能证明设备控制结果。
