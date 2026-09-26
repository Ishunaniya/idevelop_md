# lock_client 项目索引

## 项目位置与用途

- Git 仓库根目录：`/home/tronlong/lyp/code/rtms_sdk`；源码目录：`apps/lock_client/`；文档目录：`/home/tronlong/lyp/perCode/idevelop_md/rtms_sdk/Jinke/lock_client/`。本应用是 `rtms_sdk` 仓库的子目录，没有独立 Git 仓库或分支。
- 本文对应仓库分支 `develop/rtms_sdk_v1.3_20240408`、提交 `bc60961e`，本目录 CMake 版本 1.1。分支属于整个仓库；源码版本变化后应重新核对协议和运行流程。
- `lock_client` 从 MQTT 接收锁车/绑定 JSON 指令，转成本机 nanomsg 命令，并发布接收确认和本地处理回执。实际锁具动作依赖本机命令服务和设备固件，不能仅凭本进程的 `code=0` 回执判断执行成功。
- **详细源码、三张可直接显示的流程图、指令映射和异常路径统一见 [lock_client_源码全面分析.md](./lock_client_源码全面分析.md)。** 本文件只作为项目入口和名词索引。

## 相关技术与名词

| 名词 | 在本项目中的含义 |
| --- | --- |
| RTMS SDK、Git 分支、提交 | `rtms_sdk` 是整仓；分支代表整仓开发线，提交哈希定位本文核对的源码快照。`apps/lock_client/` 不单独切换分支。 |
| CMake、交叉编译、`QL_MODULE_PLATFORM` | CMake 生成本目录目标及安装规则。交叉编译使用目标设备工具链；本目录的交叉编译分支识别 `EC200A` 和 `EG25G`。顶层 `apps/CMakeLists.txt` 当前没有纳入本应用。 |
| MQTT、broker、Mosquitto、主题 | MQTT 是发布/订阅协议，broker 是消息服务器，Mosquitto 是本程序使用的客户端库。指令和回复使用不同主题；地址从 `/opt/conf_ext2.ini` 读取。 |
| QoS 1、`msgId` | QoS 1 表示 MQTT 协议层至少一次交付，可能重发。`msgId` 是业务消息标识，回执原样带回；本程序没有按它去重。 |
| JSON、cJSON、`lock`/`bind`/`level`/`delay` | cJSON 解析指令。`lock` 与 `bind` 决定动作组合，`level` 选 1～3 级锁车；`delay` 当前只写日志，没有实际延迟效果。 |
| nanomsg、PUB/SUB、PID | nanomsg 负责本机进程通信；本进程向 `26008` PUB 发送命令，从 `16008` SUB 接收以本进程 PID 为前缀的回复。 |
| libev、定时器、FIFO | libev 监听本机回复，并约每 5 秒处理至多一条待执行指令。FIFO 队列按先入先出顺序缓存指令和回执。 |
| 密码、命令标签 | 处理指令前，本程序用标签 200 请求密码；锁车/绑定请求用标签 202、密码前 6 字节和一个命令码。密码来源及实际执行由设备上的服务决定。 |
| ACK、`code` | `isAck=true` 是接收确认；`isAck=false` 的 `code=0` 仅说明本地命令发送调用未报错，`code=1` 表示命令组合无效。两者均不是锁具动作确认。 |
| INI、CPActive | INI 文件 `/opt/conf_ext2.ini` 提供 MQTT 地址；CPActive 是本地进程保活记录，超时登记为 10 秒，约每 5 秒刷新一次。 |

## 阅读顺序

1. 在 [源码全面分析](./lock_client_源码全面分析.md) 中先看“总体结构图”和“启动与初始化流程”。
2. 再看“JSON 解析与命令映射”“本机 nanomsg 协议和回复处理”。
3. 排查问题时对照“失败路径和源码可见风险”；需要判断锁具是否真正动作时，继续核对设备上的命令服务和硬件记录。
