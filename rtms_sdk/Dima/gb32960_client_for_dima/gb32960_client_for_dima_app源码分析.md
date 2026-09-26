# gb32960_client_for_dima 源码逻辑与运行边界分析

> 依据 `rtms_sdk` 仓库 `develop/rtms_sdk_v1.3_20240408` 分支、提交 `bc60961e` 的当前源码整理。本文的“执行”指能从本目录 `main.c` 追到的调用链；“保留代码”指存在实现但当前主链路未调用；“风险”指由代码条件和数据操作推导出的可能后果，未声称已在设备上复现。运行所需点表、平台配置和设备服务不在本目录内，不能据本目录源码断言最终平台验收或标准符合性。

## 1. 项目边界和构建

- 源码位于 `apps/gb32960_client_for_dima/`，属于上级 `rtms_sdk` 仓库。顶层 `apps/CMakeLists.txt` 的 `WITH_GB32960_CLIENT_FOR_DIMA` 默认 OFF；打开后才纳入构建。目录 CMake 项目和可执行文件名均为 `gb32960_client`，版本 1.1；还生成并安装 `libscene.so`。
- `GB32960_DEVICE_DIMA` 与 `GB32960_DEVICE_YUANJIN` 均默认 OFF；未选设备时 CMake 配置报错，同时选时先进入 Dima 分支。设备专用 `parse_special_msg()` 由宏选择。远锦文件中的 `parse_data_msg()` 仍为 TODO，故编译成功不代表远锦信号解析已完成。交叉编译分支支持 EC200A、EG25G、MCIMX6Y2CVM08AB，对应环境变量 `QL_MODULE_PLATFORM` 和 SDK 库路径；见本目录 `CMakeLists.txt:1-89`。
- 直接链接 nanomsg、Mosquitto、OpenSSL、cJSON、cn-cbor、tbox-common、appmng、pthread 等，不表示所有库都参与主报文路径。当前远端主链路由 `gb32960.c` 中 TCP socket 完成；内部 CAN/GPS/MCU 消息由 nanomsg 完成。

## 2. 总体流程图

下图只画 `main.c` 可达的主要运行路径。使用纯文本绘制，在普通 Markdown 阅读器中也能直接看到。各线程共享 `can_mng_t`；图中分支表示源码条件，非平台保证。

```text
┌──────────────────────────────┐
│ main：打印版本，can_mng_init │
└──────────────┬───────────────┘
               ↓
   读取平台 INI、VIN、ICCID、点表
               ↓
       CPActive 注册成功？
        ├─ 否 → 主线程返回错误
        └─ 是
            ├→ 保活线程：每 10 秒更新
            ├→ CAN 线程：订阅三路 CAN → 通用点表和设备专用解析 ─┐
            ├→ 本机消息线程：GPS、MCU → 更新 ACC、定位等 ──────┤
            └→ 等待 10 秒收集电芯数据                           │
                    ├→ 实时 TCP 状态机 ←────────────────────────┤
                    ├→ 离线保存线程 ←───────────────────────────┘
                    │       ↓ 写 SD 卡历史文件
                    └→ 历史补传线程 → 联网后读文件、改命令、发送、删除
```

### 2.1 启动顺序与失败处理

`main.c:31-90` 先设置日志标签与版本，`can_mng_init()` 用 `calloc` 建立共享对象，尝试从 `/opt/conf.ini` 读取 `dev:id`、`dev:secret`；此读取失败只打印错误。随后分配 `data_process_mng`，从 `/proc/self/exe` 获取程序目录，设置初始场景 `power_on`。`load_libscene()` 调用被注释，故当前不会动态加载共享库（`src/can_mng/can_mng.c:86-106`）。

之后读取 `/opt/parameter_conf.ini` 中 `server:domain`、`server:port`，但 `main()` 未检查返回值；再读取 `/opt/machine_vin`，有内容即复制 17 字节到共享 VIN。ICCID 先读 `/opt/sim_info` 的 `dev:ICCID`，再尝试模组接口，最后回退到字面值 `123456789`。加载固定路径 `/opt/Pointsheet_info1_gb32960.json` 后，`main()` 也未检查 `rc`。点表文件在本仓库未找到，所以字段映射只能分析解析算法与硬编码信号，不能列出部署点表的完整信号清单。`snprintf()` 为固定路径传了一个额外参数，格式串不使用该参数（`main.c:69-72`）。

CPActive 注册失败会使主线程返回错误；成功后依次创建保活、CAN、本机消息线程，等待 10 秒，再创建实时上报、历史补传、离线保存线程。`pthread_create()` 的返回值均未检查；最后只对最后一次写入的 `tid` 执行 `pthread_join()`，并非逐个线程管理。保活间隔 10 秒、登记超时 20 秒（`main.c:12-28,74-90`）。

## 3. 数据采集与共享状态

| 来源 | 入口与转换 | 产出及使用 |
| --- | --- | --- |
| CAN0/1/2 | `start_can_server()` 建立三个 `NN_SUB`，连接 `127.0.0.1:16002/16003/16004`。`nn_poll()` 接到数据后按 `sizeof(can_frame_t)` 切帧，逐帧调用 `parse_common_msg()` 和 `parse_special_msg()`（`src/can_server/can_server.c:16-100`）。 | 更新点表信号、车辆、电池、电机、告警、VIN 和 CAN 活跃时间。接收缓冲区剩余不足一帧的字节不会作为完整帧处理。声明的 26002/26003/26004 发送端口数组当前未使用。 |
| GPS | `NN_SUB` 连接 `127.0.0.1:16005`，解析 JSON 的 `location_info` 下 `lat`、`lon`、`time`、`mode` 等字段（`src/app_server/gps.c:25-103`）。 | 定位组包函数使用共享 `gps_info`；有效坐标可写 `/opt/gps_data.txt`，无效时尝试复用本地或上次坐标，并设置无效定位标志（`new_energy_frame_pack.c:559-625`）。 |
| MCU | `NN_REQ` 连接 `127.0.0.1:38000`，定时器每 5 秒发送状态请求，解析 `status.io[]` 中 `dev=mcu` 的 `params`（`src/app_server/app_server.c:27-122`、`mcu.c:10-126`）。 | `acc_stat` 决定连接/上报门控；还保存版本、电压、温度、点火等字段。若 socket 初始化失败，消息线程可能退出或无法得到有效 MCU 状态。 |

### 3.1 通用点表规则

`load_pointsheet_json()` 从 JSON 读取 `mver/sver/pver/mtyp/sce/dev/app`，将设备槽位的报文规则及信号规则加入哈希表，并建立信号名、类型与采样间隔信息（`src/data_process/data_process.c:730-824`）。`parse_common_msg()` 以 `can_port` 和 CAN ID 查找规则；取位逻辑支持按 `byte_order` 分支处理、补码有符号数、比例 `param_scale`、偏移 `param_offset` 与上下限裁剪（同文件 `435-659`）。点表的 `ofs/len` 指针解释必须与传入的整个 `can_frame_t` 布局一致：当前 `can_server.c` 传的是 `&frame`，`set_common_data_item()` 直接把它当字节数组，而非自动跳到 `frame.data`。缺少实际部署点表时，不能断言每个规则偏移正确。`gb32960_get_work_data()` 再按扩展帧标志和 CAN ID、信号名取缓存值，用于部分组包字段（同文件 `1408-1496`）。

另有 JSON/GZIP/CBOR 工作数据与 CBOR schema 生成函数，但 `main.c` 的主 TCP 上报线程没有调用它们；`src/list/` 为点表规则提供通用链表容器。场景库 `judge_scene` 与 `load_libscene()` 实现存在，当前主流程未加载；不能把场景库逻辑画进实际运行路径。

### 3.2 Dima 硬编码 CAN 解析

`parse_special_msg()` 无条件调用 `parse_500k_vehicle_data_msg()` 和 `parse_250k_vehicle_data_msg()`（`src/data_process/dima_data_process.c:728-732`）。两者按不同 CAN ID `switch`，不是按 `can_port` 自动识别波特率。因此“500k/250k”是函数名与预期数据分组，实际连接哪条总线仍由上游 CAN 服务和车辆布线决定。两组解析分别含：

| 数据类别 | 500k 入口示例 | 250k 入口示例 | 共享结果 |
| --- | --- | --- | --- |
| VIN | `0x18E1F3EF` 三段拼接 | 同一 ID 三段拼接 | 与当前 VIN 不同则写 `/opt/machine_vin`（`dima_data_process.c:71-86,402-417`）。 |
| 整车和充电 | `0x180128D0`、`0x180228D0` | `0x0C0328D0`、`0x0CFF7C03` | 运行模式、车速、挡位、SOC、总电压/电流、充电状态；特定报文调用 `can_update_canframe_timemap()`。 |
| 电芯与温度 | `0x18E3EFF3`、`0x18C1EFF3`、`0x18C2EFF3` | `0x1800DC03`、`0x18C9DC03` 等 | 电芯总数、探针总数、每个电芯电压/温度；由电芯数确定 `_splitFrameNum`。 |
| 电机、极值和告警 | `0x180728D0`、`0x1886EFF3` 等 | `0x1801BFD0`、`0x0C5F7A03` 等 | 电机数量和参数、极值、告警级别与告警位。具体比例和位偏移以对应 `case` 为准。 |

当前两个 Dima 解析函数中，实际生效的 `case` 可按用途完整索引如下。这里列的是代码的输入 ID 与写入方向，不代替信号级字节/位定义；每个字段的缩放和边界仍应回到对应 `case` 检查。

| 解析组 | 有效 CAN ID | 写入或处理内容 |
| --- | --- | --- |
| 500k | `0x18E1F3EF` | 三段 VIN，必要时回写文件。 |
| 500k | `0x180128D0`、`0x1801FA28` | 运行与车辆状态、车速、挡位、踏板及制动、里程。 |
| 500k | `0x1884EFF3`、`0x180428D0`、`0x180228D0`、`0x1885EFF3` | SOC、电池总电压/电流、DCDC、充电状态与活跃时间、绝缘电阻。 |
| 500k | `0x18E3EFF3`、`0x18C1EFF3`、`0x18C2EFF3` | 电芯和温探数量、单体电压、单体温度及分帧数。 |
| 500k | `0x1886EFF3`、`0x1887EFF3`、`0x1888EFF3` | 温度与电压极值及对应探针、单体编号。 |
| 500k | `0x180728D0`、`0x180528D0`、`0x180628D0` | 电机数量与状态、温度、转速、扭矩、电压电流。 |
| 500k | `0x1881EFF3`、`0x1882EFF3` | 告警位和最高告警级别、DCDC 状态告警。 |
| 250k | `0x18E1F3EF` | 三段 VIN，必要时回写文件。 |
| 250k | `0x0C0328D0`、`0x18FF0128` | 运行与车辆状态、车速、挡位、踏板及制动、里程。 |
| 250k | `0x0CFF7C03`、`0x0C4F7A03` | SOC、电池总电压/电流、充电状态与活跃时间、绝缘电阻。 |
| 250k | `0x1800DC03`、`0x18C9DC03` | 电芯数量和电压、温探数量和温度、分帧数。 |
| 250k | `0x0C2F7A03`、`0x0C3F7A03` | 温度和电压极值及对应编号。 |
| 250k | `0x1801BFD0`、`0x1803BFD0`、`0x1802BFD0` | 电机数量与状态、温度、转速、扭矩、电压电流。 |
| 250k | `0x0C5F7A03` | 告警位和最高告警级别。 |

250k 函数中 `0x180428D0` 和 `0x180728D0` 的两个分支在 `/* ... */` 内，不属于上表的有效解析；500k 函数中同名 `0x180728D0` 分支则有效。电芯分帧数规则为 `0..200 → 1`、`201..400 → 2`、`401..600 → 3`、`601..800 → 4`，依赖收到数量报文；数量 0 也会被设成 1 帧，不能据此认定电芯数据已经收到。500k 来源若给出大于 800 的数量，则该判断链不会设置新的分帧数。`check_can_active()` 比较最后一次指定 CAN 报文更新的时间戳与当前时间，阈值 15 秒；不是“任意 CAN 帧在 15 秒内到达”就算活跃（`src/can_server/can_active.c:6-28`，`dima_data_process.c:8-22,160-187,508-530`）。

远锦 `parse_data_msg()` 只有 TODO，`parse_special_msg()` 调它后没有完成车辆字段解析（`yuanjin_data_process.c:1-41`）。

## 4. 平台 TCP 状态机

源码状态枚举还有 HEARTBEAT、GPS、HISTORY，但 `gb32960_data_report()` 的 `switch` 只执行 UNKNOWN、LOGIN、REPORT、LOGOUT 四类分支；图中仅画可达状态（`src/new_energy/gb32960.c:22-35,1321-1498`）。

```text
                  ┌──────────────────────────┐
                  │ UNKNOWN：检查运行门控条件 │←───────────────┐
                  └────────────┬─────────────┘                │
                               │条件满足                      │
                               ↓                              │
                     解析域名、建立 TCP 连接 ──失败───────────┘
                               │成功
                ┌──────────────┴────────────────┐
                │本地状态非 LOGIN                │本地状态为 LOGIN
                ↓                              ↓
        LOGIN：发送登录帧               LOGOUT：发送登出帧
        ├─ recv 无正字节 → 稍后重试     ├─ recv 无正字节 → 稍后重试
        ├─ 发送失败/门控消失 → UNKNOWN ├─ 发送失败 → UNKNOWN
        └─ recv 正字节 → REPORT         ├─ 正字节且门控满足 → LOGIN
                                       └─ 正字节且门控消失 → UNKNOWN
                REPORT：到周期逐帧打包并 send
                ├─ 发送成功 → 留在 REPORT，等待下个周期
                ├─ 发送失败 → UNKNOWN
                └─ 门控消失 → LOGOUT
```

门控条件是 `(mcu_info && acc_stat) || (_isIncharge && check_can_active())`。UNKNOWN 状态需要时解析平台域名并调用 TCP 连接；连接成功设置 10 秒接收超时和 60 秒 `TCP_USER_TIMEOUT`。前次本地 INI 仍为 LOGIN 时先登出，再尝试登录。登录流水号来自 `/opt/conf_loginout.ini`，循环中按 65531 取模，尝试登录时递增。登录/登出只以 `recv()` 是否返回正字节推进本地状态，未见完整的应答命令、VIN、校验码和结果码验证（`gb32960.c:1333-1447`）。

REPORT 用单调时钟计算间隔，普通周期 10000 ms，`_warnLevel==0x03` 时 1000 ms；按 `_splitFrameNum` 调组包并逐帧 `send()`。`data_send_out()` 要求单次 `send()` 返回完整长度；失败则置 `network_offline=1` 并返回 UNKNOWN。普通实时帧没有在此处等待逐帧平台 ACK。`pre_timer` 在进入循环前声明但未初始化，首次 `diff_ms(pre_timer,cur_timer)` 使用未初始化值；首帧何时触发不能仅由“10 秒周期”确定（`gb32960.c:1340-1344,1450-1482`）。

## 5. 报文组织与分帧

`new_energy_frame_pack.h:142-167` 定义固定帧头：`##` 两字节、命令字、应答标志、17 字节 VIN、加密方式、两字节数据单元长度、数据体和 BCC。普通组帧函数将加密方式写为 `0x01`（不加密），长度使用大端转换，BCC 对起始符之后至 BCC 之前的字节计算；登录 `0x01`、实时 `0x02`、历史补发 `0x03`、登出 `0x04` 的命令值见 `new_energy_frame_pack.h:9-22`。源码有 AES128 密钥常量与函数，但当前登录、登出和实时组包均调用普通 `gb4_frame_pack_full()`；不能据保留函数推断线上启用了 AES（`new_energy_frame_pack.c:14-100,113-207,1972-1975`）。

| 数据单元 | 实际来源及条件 |
| --- | --- |
| 登录/登出 | 当前本地时间和流水号；登录还填充 ICCID、储能子系统信息；本地状态保存在 INI。 |
| 整车 `0x01` | 共享 `vehicle` 中的状态、车速、里程、SOC、总电压电流等。 |
| 驱动电机 `0x02` | `motor_header._count==0` 时不生成；否则按电机数量组包（`new_energy_frame_pack.c:439-487`）。 |
| 位置 `0x05` | GPS 有效时填经纬度；无效时尝试复用先前或文件中的坐标并置无效标志（同文件 `559-625`）。 |
| 极值 `0x06`、报警 `0x07` | 来自硬编码 CAN 解析保存的极值、告警等级与位图；电芯和温探总数都为 0 时极值函数返回 0（同文件 `658-1136`）。 |
| 单体电压 `0x08`、温度 `0x09` | 电池总数决定每帧最多 200 个电芯电压；温度数据按总探针数打包（同文件 `1140-1368`）。 |
| 自定义 `0x80` | 当前调用的是 `gb32960_pack_dev_id_data()`，长度和内容来自 `/opt/conf.ini` 的设备 ID；另一个自定义数据函数位于 `#if 0`（同文件 `1371-1431`）。 |

实时组包函数按 `_splitFrameNum` 分 1～4 帧。单帧把整车、电机、位置、极值、告警、电压、温度、设备 ID 组合；多帧时首帧带主要状态和第一段电压，其余帧主要是剩余电压及极值。氢燃料数据组装调用位于 `#if 0`，不能算进当前帧。组包使用设备本地时间；函数签名中的 `timespec` 参数当前未用于帧时间。`cur_battlist` 在发送前从共享列表复制，温度虽也复制到 `cur_templist`，温度组包函数却读取 `_templist`，因此复制操作并不能保证整帧温度与电压来自同一快照（`gb32960.c:1458-1467`，`new_energy_frame_pack.c:1332-1369`）。

**双分帧首帧的确定性长度问题：**`splitTotal==2` 时计算长度已计入告警段与一次电压段（`new_energy_frame_pack.c:1606-1627`），但复制时没有复制告警段，却复制了两次电压段（同文件 `1633-1645`）。该分支的电压段包含 200 个电芯的电压，明显长于固定长度告警段，因此成功进入该分支并完成分配后，复制总长度会超过 `malloc(reldataLen)` 的容量。这里仅依据代码的长度与复制语句判断，未在设备上复现内存损坏。三、四分帧路径另有不同字段排列，应以源码逐分支检查，不能假设所有分帧的内容与单帧一致。

## 6. 离线保存与历史补传

```text
【保存线程】
运行门控成立？ → network_offline == 1？ → 到保存周期？
      否 ↺               否 ↺                 否 ↺
                                                │是
                                                ↓
                                    按分帧数生成实时帧
                                                ↓
                                    VIN 非空且文件可写？
                                      ├─ 否 → 返回循环
                                      └─ 是 → 写 FF FF + 本机 short 长度 + 完整帧
                                               ↓
                                         文件大于 5,000,000 字节？
                                          ├─ 是 → 关闭并轮换
                                          └─ 否 → 继续使用

【补传线程】
检查历史目录 → TCP 有效且 network_offline == 0？
                 ├─ 否 → 约 60 秒后重查
                 └─ 是 → 选文件并逐条读 → 命令改 03、重算 BCC、调用 send
                                                  ↓
                                      关闭文件并执行删除 → 再检查目录
```

`pthread_save_history_data()` 初始查询目录大小，若大于 `450*1024` KiB，调用函数删除两个旧文件；这是启动线程时的一次检查，不是运行期持续容量上限。保存门控与主上报相同，还要求 `network_offline==1`；常规保存间隔 9990 ms，三级告警 990 ms。每条历史记录前写 `0xFF 0xFF` 和本机 `short` 长度，后接完整实时帧；单文件超过 5,000,000 字节才轮换。写历史时只检查 VIN 非空，目录需要 SD 卡及权限（`gb32960.c:846-985`）。

补传线程约每 60 秒扫描一次目录，要求 `iClient>0` 且 `network_offline==0`。读出记录后将帧第 3 字节改为补发命令 `0x03` 并重算 BCC。`read_history_file_to_sent_loop()` 对读取正文只检查 `rc>0`，没有要求完整读取 `dataLen`；发送后不检查 `data_send_out()` 返回值。外层不使用已发送条数或完整读完标志，直接对所选文件执行删除。因此发送失败、半帧读取、连接在补传中断开或平台未确认时，都存在数据未可靠送达而文件被删的风险（`gb32960.c:987-1139`）。

还有两条行为边界：主线程在首次成功登录前 `network_offline` 由 `calloc` 为 0；若登录发出但未收到正字节，代码保持 LOGIN 重试，却没有在该分支置离线标志，保存线程可能尚未启动有效缓存。补传与实时上报共用 `iClient`，分别在不同线程调用 `send()`，没有针对该 socket 的统一发送锁；`io_mutex` 只覆盖历史文件读写片段，不能证明网络字节流的帧顺序完全按业务意图排列。这些是静态代码推导的并发边界，需在目标机做断网和高负载验证。

## 7. 配置与外部依赖清单

| 路径/接口 | 代码中的用途与前提 |
| --- | --- |
| `/opt/parameter_conf.ini` | `server:domain`、`server:port` 控制主 TCP 平台；默认域名字符串 `fyai.top`，端口缺失则读为 0，不能将默认域名视作通用生产平台。 |
| `/opt/conf.ini` | 读取 `dev:id`、`dev:secret`；设备 ID 加入 `0x80` 数据段。读取失败不终止 `can_mng_init()`。 |
| `/opt/sim_info` | 读取 `dev:ICCID`；失败时尝试 Quectel SIM API，再失败使用占位值。 |
| `/opt/machine_vin` | 启动时读 VIN，Dima CAN 完成三段 VIN 接收时可改写。 |
| `/opt/Pointsheet_info1_gb32960.json` | 通用点表；仓库当前未含该部署文件，需从设备或配置包获取才能验证完整映射。 |
| `/opt/conf_loginout.ini` | `loginout:stat`、`session:id` 本地登录状态和流水号，首次缺失或空文件可生成默认值。 |
| `/opt/gps_data.txt` | 定位上次有效坐标的本地回退文件。 |
| `/media/sdcard/gb32960_history` | 离线文件目录，文件名含 VIN 与时间；补传后代码删除文件。 |
| `127.0.0.1:16002/16003/16004` | 本机三路 CAN nanomsg SUB 服务。 |
| `127.0.0.1:16005`、`127.0.0.1:38000` | 分别为 GPS SUB、MCU REQ 服务。 |
| `/tmp/log_print.flag` | 实时发送成功后，如文件存在则打印完整报文十六进制。 |

`/opt/conf_ext2.ini`、`/tmp/offline.flag`、`/media/sdcard/gb32960.log`、`/media/sdcard/gb32960_recorder` 在本目录有读取辅助函数、函数定义或常量，但没有证据表明它们都由当前 `main()` 的主上报链路持续使用。`NEW_ENERGY_IP/PORT` 也是保留的测试地址常量，实际 TCP 地址来自平台 INI。上述路径不应与主链路配置混为一谈。

## 8. 核查结论与验证点

| 结论 | 静态证据 | 需要实机确认的部分 |
| --- | --- | --- |
| 主链路为 CAN/GPS/MCU 采集 → 共享状态 → TCP 登录和周期上报 → 离线文件补传。 | `main.c:44-89`、`can_server.c:16-100`、`gb32960.c:1321-1498`。 | 上游服务提供的数据格式、平台是否接受帧。 |
| Dima 硬编码 CAN 解析有实质实现；远锦专用解析尚未实现。 | `dima_data_process.c:49-732`、`yuanjin_data_process.c:27-39`。 | 具体车辆 CAN ID、波特率、点表文件与实际车型的一致性。 |
| `splitTotal==2` 首帧存在分配长度与实际复制长度不一致。 | `new_energy_frame_pack.c:1606-1645`。 | 对应车型是否触发及实际运行影响；可用隔离测试或 ASan 复现，避免在生产设备试验。 |
| 补传文件删除不等待平台业务确认，也不以发送返回值控制。 | `gb32960.c:1015-1029,1098-1115`。 | 服务端是否已收到、接收多少帧，以及断网中断时的丢数范围。 |
| 当前无法证明完整标准符合性或线上长期稳定性。 | 部署点表和平台配置未在仓库中；代码未做完整 ACK 校验。 | 按目标标准版本、设备服务和平台协议做互通、断网及压力验证。 |

本文只记录当前提交的可见行为及明确的静态边界。若切换 Git 分支、替换部署点表或修复上述代码，流程图和结论应同步更新。

## 9. 设备故障时如何定位到本程序

这一节用于设备已经出现问题时收集证据。先记录故障时间、VIN（对外共享日志时遮盖）、程序版本、设备型号与构建选项，再按下图逐层缩小范围。下面的命令是目标设备上的**只读检查示例**；嵌入式系统可能只提供 BusyBox 工具，命令不存在时用同类工具替代。不要仅凭单条日志判断“平台已收到”，也不要在保全历史文件和日志前重启、清理目录或改登录 INI。

```text
故障现象
   ↓
进程在运行？ ──否──→ 看退出码/崩溃日志/依赖库/启动配置（9.2）
   │是
   ↓
CAN、GPS、MCU 本机消息正常？ ──否──→ 定位上游服务、端口、帧格式、VIN（9.3）
   │是
   ↓
ACC 开启，或充电且指定 CAN 报文活跃？ ──否──→ 检查门控与告警来源（9.3）
   │是
   ↓
平台域名可解析、TCP 可连接？ ──否──→ 检查 INI、DNS、路由、平台端口（9.4）
   │是
   ↓
有登录应答正字节、进入 REPORT？ ──否──→ 查登录帧/VIN/ICCID/平台应答（9.4）
   │是
   ↓
_splitFrameNum>0 且 data_len>0？ ──否──→ 查电芯数量、分帧组包（9.5）
   │是
   ↓
设备 send 成功且平台记录到帧？ ──否──→ 查报文结构、分帧、平台业务应答（9.5）
   │是
   ↓
若仍有丢数：查离线保存、补传和文件删除时机（9.6）
```

### 9.1 先固定证据和运行版本

1. 记录故障发生的设备本地时间及平台时间，确认时区、系统时钟；实时帧时间取设备本地时间。拍下或保存故障前后的原始日志，特别是 `APP Version`、`domain[...]`、`port [...]`、`connected to can0/1/2 port`、`plateform_login`、`data_len`、`report real data`、`read history file`。日志中的 `plateform` 和 `faild` 是源码原拼写。
2. 先定位真实运行的可执行文件与进程，而非只看安装目录。示例：`ps -ef | grep '[g]b32960_client'`、`pidof gb32960_client`、`readlink -f /proc/<PID>/exe`、`ls -l /proc/<PID>/fd`。`APP Version: 1.1` 只表示 CMake 版本；如需对应到源码提交，应查构建包记录或二进制校验值，不能仅凭版本号断言就是本文的 `bc60961e`。
3. 检查设备环境：`df -h /media/sdcard`、`ls -ld /opt /media/sdcard /media/sdcard/gb32960_history`、`ls -l /opt/parameter_conf.ini /opt/conf.ini /opt/sim_info /opt/machine_vin /opt/Pointsheet_info1_gb32960.json /opt/conf_loginout.ini`。只看文件存在和权限不代表内容有效。`/opt/conf.ini` 含 `secret`，而 `sany_read_device_id()` 成功时还会把设备 ID 和密钥直接打印到日志（`can_mng.c:43`）；对外共享日志前须遮盖该行及 VIN、ICCID。

常用只读命令可按需逐条执行，先确认系统是否提供相应工具：

```sh
ps -ef | grep '[g]b32960_client'
df -h /media/sdcard
ls -ld /media/sdcard/gb32960_history
du -sk /media/sdcard/gb32960_history
ss -tan
```

### 9.2 程序未启动、反复退出或卡住

| 现象/证据 | 对应代码检查 | 下一步 |
| --- | --- | --- |
| 日志只到 `can_mng_init error` | `main.c:44-48`；`data_process_mng_init()` 分配失败会使初始化返回 NULL。设备 ID/密钥读取失败只打印 `failed to read device id & secret`，本身不会立即退出。 | 核对内存与进程退出码；不要把设备 ID 报错直接当成唯一退出原因。 |
| `failed to add process info to CPActive` | `main.c:74-77` 直接返回错误。 | 查 CPActive 依赖服务/共享库与进程注册条件。 |
| 线程看似启动但无 CAN/GPS/MCU 结果 | `pthread_create()` 返回值未检查，`main()` 最后只 `join` 最后一个线程（`main.c:80-89`）。 | 结合进程线程列表（若有 `ps -T -p <PID>`）和各线程入口日志，判断线程是否确实创建、是否提前退出。 |
| 在 `_splitFrameNum = 2`、`data_len` 附近崩溃或堆异常 | 双分帧首帧长度不匹配见第 5 节和 `new_energy_frame_pack.c:1606-1645`。 | 保存崩溃日志与对应电芯数量，在隔离构建中用 ASan/最小复现验证；不要在运行设备上直接反复触发。 |
| 长时间无输出 | 上报线程在门控不满足时可睡 5 秒，连接失败可睡 10 秒，登录失败可睡 5 秒；补传线程每轮可睡 60 秒（`gb32960.c:1380-1411,1139-1141`）。 | 看进程是否仍在、最后日志停在哪个状态，并检查上游输入；“暂时没日志”不等于死锁。 |

若设备有 `dmesg` 或系统日志，检查 OOM、段错误、库加载及文件系统错误；这些信号需要与同一时间的程序日志对应。不要把 `APP Version` 后的单次卡顿直接归因于网络，启动顺序中还有固定 10 秒等待（`main.c:83-87`）。

### 9.3 本机数据与上报门控

| 现象 | 先查什么 | 判断依据 |
| --- | --- | --- |
| 没有 `connected to can0/1/2 port` | CAN nanomsg 发布者是否在本机 `16002/16003/16004` 提供服务；检查 `nn_connect` 错误及上游进程状态。 | `start_can_server()` 某个 socket 初始化或连接失败就返回，后续 CAN 解析不会继续（`can_server.c:58-100`）。`nn_connect` 成功本身也不证明持续收到帧。 |
| 有 CAN 连接，仍无车辆/电芯值 | 核对 Dima 编译宏、目标设备 CAN ID、CAN 帧结构与三个通道；比对第 3.2 节 ID 索引及 `/opt/Pointsheet_info1_gb32960.json`。 | 通用规则按通道和原始 `can_id` 查表；Dima 专用规则把最高扩展帧标志位去掉后 `switch`。若点表偏移与 `can_frame_t` 不一致，通用值可能错误（`data_process.c:650-659`、`dima_data_process.c:69-70`）。 |
| 一直显示 `Acc-off and Non-charged` | 核对 MCU 是否返回 `status.io[].dev=mcu` 的 `params.acc_stat`；充电时检查 `_isIncharge` 来源报文及 15 秒活跃条件。 | 门控不是“有 CAN 数据”即可通过，而是 `acc_stat` 为真，或充电且指定 CAN 时间戳活跃（`gb32960.c:1351-1352`、`dima_data_process.c:142-150,461-490`）。 |
| 没有 GPS 或定位被标无效 | 检查本机 `16005` GPS 消息的 `location_info.lat/lon`、进程日志、`/opt/gps_data.txt`。 | 组包函数只有经纬度换算后均大于 0 才标有效；其他情况可复用旧坐标并标无效（`new_energy_frame_pack.c:559-625`）。 |
| VIN/ICCID 不对 | 检查 `/opt/machine_vin` 实际字节数、Dima VIN 三段 CAN 报文、`/opt/sim_info` 的 `dev:ICCID`；比较登录帧中的值。 | 启动只要 VIN 字符串长度大于 1 就复制 17 字节，未在 `main()` 校验完整 17 位；ICCID 失败时会回退到 `123456789`（`main.c:52-67`）。 |

### 9.4 平台连接和登录

1. 从 `/opt/parameter_conf.ini` 的 `[server]` 读取域名和端口；不要把 `/opt/conf_ext2.ini` 的 `mgw:local_port` 当作主平台端口。核对启动日志的 `domain[...]`、`port [...]`；若出现 `get port failed` 或端口为 0，`main()` 仍会继续，之后 TCP 连接可能失败（`gb32960.c:187-219`，`main.c:50`）。
2. 用设备上可用的 `nslookup <平台域名>`、`ip route`、`ss -tan`（或 `netstat -tan`）看解析、路由与 TCP 状态。域名解析日志 `can not get domain ip` 与 TCP 日志 `socket connect to ... failed` 指向不同层；`creat new connect success` 只表示 TCP 建连，未完成登录（`common.c:193-219,694-740`，`gb32960.c:1357-1387`）。
3. 若重复 `plateform_login` 并打印 `login fail`，检查登录帧的 VIN、ICCID、流水号与平台应答，区分超时、断连、平台拒绝。当前代码只以 `recv()>0` 判定可进入 REPORT，没有业务应答校验；`loginout:stat=LOGIN` 也只是本地记录。不要因 `recv` 有数据就断言服务端接受了报文（`gb32960.c:1394-1419`）。
4. 若重连后先 `plateform_logout`，核对 `/opt/conf_loginout.ini` 中 `loginout:stat` 是否保留 LOGIN；这是代码预期的重连路径。不要在未记录原始内容前手工将状态改成 LOGOUT，它会改变随后重连行为（`gb32960.c:1371-1376,1147-1319`）。

### 9.5 实时发送异常、缺字段和上报节奏异常

| 现象/日志 | 解释和定位 |
| --- | --- |
| 已进入 REPORT 且周期触发，却长期没有 `_splitFrameNum = ...` 和 `data_len` 日志 | 这两条日志写在 `for (i=0; i<_splitFrameNum; i++)` 内；分帧数为 0 时连这两条日志也不会出现。先核对电芯数量报文、Dima 解析分支及 `_splitFrameNum` 赋值。500k 报文中的数量大于 800 时不会设置新的 1～4 帧值；数量 0 会设为 1 帧，但不代表单体电压有效（`gb32960.c:1456-1467`、`dima_data_process.c:160-187`）。 |
| 有 `data_len` 但部分数据段缺席，或 `data_len` 为 0 | 沿 `gb4_frame_pack_real_data()` 看子段分配与组帧返回值，再核对 `_battCellTotal/_tempCellTotal`。电机数为 0 时电机段自然缺席；两类计数都为 0 时极值段缺席，但这些单独条件不必然使整帧 `data_len` 为 0（`new_energy_frame_pack.c:439-487,658-667,1972-1996`）。 |
| `report real data success`，平台却没数据 | 该日志只证明一次 `send()` 返回完整长度；代码未在实时上报后读取该帧的业务 ACK。抓取对应时间段的设备发送帧与平台接收记录，按 VIN、命令字、数据长度、BCC、流水和时间逐项核对（`gb32960.c:122-136,1465-1481`）。 |
| 普通上报不严格每 10 秒，首次上报时间异常 | 常规门限 10000 ms、三级告警 1000 ms，但 `pre_timer` 首次使用前未初始化。还要核对 `_warnLevel`、线程调度、门控与连接状态（`gb32960.c:1340-1344,1450-1457`）。 |
| 201～400 个电芯时崩溃、报文异常 | 对应双分帧首帧的重复电压段复制和长度不匹配，见第 5 节。优先在非生产环境隔离复现，不应把该分支输出当作可信报文（`new_energy_frame_pack.c:1606-1645`）。 |
| GPS/温度字段时有跳变 | GPS 无效时可复用上次或文件坐标；温度组包直接读共享 `_templist`，而发送线程复制的是 `cur_templist`。把原始 GPS/CAN 输入与报文同一时间窗口对齐，辨别源数据变化与共享状态快照不一致（`new_energy_frame_pack.c:559-625,1332-1369`）。 |

### 9.6 离线缓存、补传与丢数

1. 看 `df -h /media/sdcard`、`ls -ld /media/sdcard/gb32960_history`、`du -sk /media/sdcard/gb32960_history`；结合 `failed to create`、`save data error`、`history_file_size` 判断目录是否存在、可写和容量是否足够。写入只在门控成立且 `network_offline==1` 时发生；首次登录未成功但没有置离线标志时，可能根本没有缓存（`gb32960.c:846-985,1400-1411`）。
2. 保存失败时检查 VIN 是否非空、`_splitFrameNum` 是否大于 0、SD 卡是否可写、单文件是否因超过 5,000,000 字节轮换。保存线程只在启动时做一次总量超过 `450*1024` KiB 的旧文件删除；不能把这个值当作持续配额（`gb32960.c:866-934,949-980`）。
3. 补传时对齐 `read history file`、`send frame cnt`、`system cmd: rm ...` 与平台收到的补发命令 `0x03` 数量。代码可能在发送失败或只读到部分记录后删除整个文件，且不等待平台确认。**不要以历史目录变空作为补传成功证据**；应以平台记录或明确 ACK 作为结论（`gb32960.c:987-1139`）。
4. 若平台同时看到实时帧和补传帧交错，注意两个线程共享同一 TCP socket 且没有统一发送锁。核对抓包字节流、帧边界和时间顺序，再判断是链路问题还是设备端并发发送问题（`gb32960.c:1019-1029,1458-1482`）。

### 9.7 如何形成可复查的问题结论

把一次故障整理为“现象和时间 → 运行二进制/构建选项 → 输入证据（CAN/GPS/MCU、配置）→ 状态机日志 → 发送帧或历史文件 → 平台接收结果 → 对应源码行号”。若只有设备端日志，没有抓包或平台记录，结论应写成“设备端发送函数返回成功”或“本地文件被删除”，不要写成“平台确认接收”。若涉及第 5 节分帧长度问题，可先在隔离环境做针对该分支的内存检查；若涉及丢数，应先保全历史目录和时间窗口内的平台记录，再考虑修复与回归。
