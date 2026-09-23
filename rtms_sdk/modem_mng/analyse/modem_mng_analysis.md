# modem_mng 源码全面分析文档

> **版本**：1.30 | **分析日期**：2026-03 | **目标平台**：EC200A / EG25G / i.MX6ULL

---

## 目录

1. [仓库总览](#1-仓库总览)
2. [目录结构与文件说明](#2-目录结构与文件说明)
3. [架构设计](#3-架构设计)
4. [编译系统分析（CMakeLists.txt / Makefile）](#4-编译系统分析)
5. [核心模块逐行分析](#5-核心模块逐行分析)
   - 5.1 [dialer.hpp — 抽象拨号器与多平台类定义](#51-dialerhpp--抽象拨号器与多平台类定义)
   - 5.2 [dialer_ec200a.cpp — EC200A 拨号主逻辑](#52-dialer_ec200acpp--ec200a-拨号主逻辑)
   - 5.3 [nanomsg_process.hpp — IPC 服务端声明](#53-nanomsg_processhpp--ipc-服务端声明)
   - 5.4 [nanomsg_process.cpp — IPC 服务端实现](#54-nanomsg_processcpp--ipc-服务端实现)
   - 5.5 [logger_sd.h / logger_sd.c — SD 卡日志系统](#55-logger_sdh--logger_sdc--sd-卡日志系统)
   - 5.6 [parse_config.hpp — JSON 配置解析器](#56-parse_confighpp--json-配置解析器)
   - 5.7 [traffic_sql/sql_traffic.hpp — 流量数据库](#57-traffic_sqlsql_traffichpp--流量数据库)
   - 5.8 [reboot_conf/dial_reboot_conf.c — 重启守护配置](#58-reboot_confdial_reboot_confc--重启守护配置)
   - 5.9 [fault_report/ — 故障上报模块](#59-fault_report--故障上报模块)
6. [主程序启动流程（main）](#6-主程序启动流程main)
7. [拨号状态机完整流程图](#7-拨号状态机完整流程图)
8. [nanomsg IPC 通信协议](#8-nanomsg-ipc-通信协议)
9. [流量监控与 SQLite 持久化](#9-流量监控与-sqlite-持久化)
10. [分级故障恢复策略](#10-分级故障恢复策略)
11. [快速失败重试机制](#11-快速失败重试机制)
12. [CFUN 调用限制机制](#12-cfun-调用限制机制)
13. [SD 卡日志系统](#13-sd-卡日志系统)
14. [LED 状态控制](#14-led-状态控制)
15. [多平台差异对比](#15-多平台差异对比)
16. [编译安装全流程](#16-编译安装全流程)
17. [运行时文件与临时文件汇总](#17-运行时文件与临时文件汇总)
18. [关键数据流总览](#18-关键数据流总览)

---

## 1. 仓库总览

`modem_mng` 是一个嵌入式 Linux 上的 **4G 调制解调器管理守护进程**，负责：

- 建立并维护 4G 数据拨号连接（Data Call）
- 通过 **nanomsg REQ/REP** 对外暴露蜂窝状态查询接口
- 通过 **nanomsg PUB/SUB** 发布网络事件（超限、断网等）
- 统计并持久化流量数据（SQLite）
- 多级别故障自动恢复
- 向进程管理框架（`appmng`）注册心跳保活

支持的硬件平台：

| 宏定义                    | 模块型号   | 拨号方式         |
|---------------------------|------------|------------------|
| `USE_EC200A_DIAL`         | EC200A     | Quectel Open SDK |
| `USE_EG25_DIAL`           | EG25G      | QMI + MCM 接口   |
| `USE_IMX6U_DIAL`          | i.MX6ULL   | USB CDC-NCM（ttyUSB）|

---

## 2. 目录结构与文件说明

```
modem_mng/
├── CMakeLists.txt              # 主构建脚本，区分三种平台
├── Makefile                    # EC200A 平台旧版 Makefile（兼容手动构建）
├── dialer.hpp                  # 抽象基类 Dialer + 各平台子类声明
├── dialer_ec200a.cpp           # EC200A 主程序 main() + dial_loop() 实现
├── dialer_eg25.c               # EG25G 平台入口
├── dialer_imx6ull.cpp          # i.MX6ULL 平台入口
├── nanomsg_process.hpp         # NanoReqHandler / TrafficMonitor / NetWork_EventPublisher
├── nanomsg_process.cpp         # 上述三类的实现 + ModemReqHandler 各 getter 实现
├── nanomsg_process_wraper.cpp  # nanomsg C++ 包装层
├── nanomsg_process_cinterface.h# C 接口声明
├── logger_sd.h / logger_sd.c  # SD 卡日志系统
├── parse_config.hpp            # cJSON 配置解析器（读 config.json）
├── imx_led.hpp                 # i.MX6ULL LED 控制
├── CHANGELOG.md                # 版本变更历史
├── DIAL_MECHANISM.md           # 拨号机制文档
│
├── ec200a/                     # EC200A 专属子模块
│   ├── apn/apn.c(.h)           #   APN 管理（根据 ICCID 匹配运营商 APN）
│   ├── data_call/data_call.c(.h)# Data Call 状态管理与回调
│   ├── nw/nw.c(.h)             #   网络状态（注册、信号、IP）
│   ├── sim/sim.c(.h)           #   SIM 卡初始化与 ICCID 获取
│   ├── test_utils/             #   AT 命令测试工具
│   ├── dialer.hpp              #   EC200A 局部拨号器头
│   └── _public.h               #   公共宏定义
│
├── eg25/                       # EG25G 专属子模块
│   ├── at/at.c(.h)             #   AT 命令收发（串口层）
│   ├── apn/apn.c(.h)           #   APN 管理
│   ├── nw/nw.c(.h)             #   网络状态
│   ├── sim/sim.c(.h)           #   SIM 卡 MCM 接口
│   ├── dial/dial.c(.h)         #   拨号流程
│   ├── tz/tz.c(.h)             #   时区获取
│   └── opt_iniparser/          #   INI 文件解析器（iniparser 库）
│
├── traffic_sql/                # 流量统计 SQLite 模块
│   ├── sql_traffic.hpp         #   TrafficDatabase 单例（C++ header-only）
│   └── traffic_sql_interface.cpp/h # C 语言接口包装
│
├── reboot_conf/                # 重启守护配置
│   ├── dial_reboot_conf.c      #   基于超时时长触发系统重启
│   └── dial_reboot_conf.h
│
├── fault_report/               # 故障事件上报模块
│   ├── NetworkMonitor.h/.cpp   #   网络可达性监控
│   ├── SimSignalMonitor.h/.cpp #   信号强度监控
│   ├── SimMonitor.h/.cpp       #   SIM 卡状态监控
│   └── WiFiMonitor.*_todel     #   WiFi 监控（废弃，待删除）
│
└── cc_deque/                   # 通用双端队列（C 实现，第三方库）
    ├── cc_deque.h/.c
    └── cc_common.h/.c
```

---

## 3. 架构设计

### 3.1 整体分层架构

```
┌──────────────────────────────────────────────────────────────────┐
│                         外部消费者                                │
│     上位机 / 其他守护 / 配置管理器                                │
└───────────────┬──────────────────────┬───────────────────────────┘
                │ nanomsg REQ/REP      │ nanomsg PUB/SUB
                │ tcp://127.0.0.1:38001│ tcp://127.0.0.1:48001
┌───────────────▼──────────────────────▼───────────────────────────┐
│                      modem_mng 进程                               │
│                                                                  │
│  ┌────────────────────┐   ┌──────────────────────────────────┐  │
│  │  NanoReqHandler    │   │   NetWork_EventPublisher (单例)   │  │
│  │  (REP 服务端线程)  │   │   (PUB 发布者)                   │  │
│  │  ModemReqHandler   │   │                                  │  │
│  └────────────────────┘   └──────────────────────────────────┘  │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              EC200ADialer::dial_loop()                    │   │
│  │  ┌──────────┐ ┌─────────┐ ┌──────────┐ ┌────────────┐  │   │
│  │  │ST_STATUS │→│ ST_SIM  │→│ST_SIGNAL │→│  ST_PING   │  │   │
│  │  └──────────┘ └─────────┘ └──────────┘ └─────┬──────┘  │   │
│  │                                               │失败      │   │
│  │                                        ┌──────▼──────┐  │   │
│  │                                        │ ST_RECOVERY  │  │   │
│  │                                        │ L1/L2/L3     │  │   │
│  │                                        └─────────────┘  │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │   TrafficMonitor::check_CCINet_FlowOverLimit() (线程)     │   │
│  │   TrafficDatabase (SQLite /media/sdcard/traffic/)         │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │   logger_sd (SD 卡 /media/sdcard/dial_log/*.log)          │   │
│  └──────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────┘
                │
                ▼ Quectel SDK / AT 命令 / ioctl
┌──────────────────────────────────────────────────────────────────┐
│              硬件层（EC200A / EG25G / ttyUSB）                   │
└──────────────────────────────────────────────────────────────────┘
```

### 3.2 线程模型

| 线程                        | 创建位置             | 职责                          |
|-----------------------------|----------------------|-------------------------------|
| `main` 线程                 | `main()`             | 初始化、等待拨号完成、join     |
| `dialThread`                | `main()`             | EC200ADialer::dial_loop()     |
| `nano_msg_thread`           | `main()`             | NanoReqHandler::processModemRequests() |
| `monitorOverLimit`（detach）| TrafficMonitor       | 每 10s 检查流量是否超限       |
| `monitorHasFlow`（detach）  | TrafficMonitor       | 每 60s 检查是否有流量         |

---

## 4. 编译系统分析

### 4.1 CMakeLists.txt 逐段解析

```cmake
cmake_minimum_required(VERSION 3.11)
project(modem_mng VERSION 1.0)
```
要求 CMake ≥ 3.11，项目名为 `modem_mng`，版本 1.0。

```cmake
find_package(libev "4.33" REQUIRED)
find_package(nanomsg "1.2" REQUIRED)
find_package(openssl "1.1.1" REQUIRED)
find_package(tbox-common "1.0" REQUIRED)
find_package(cjson "1.7.15" REQUIRED)
find_package(uthash "2.3.0" REQUIRED)
find_package(appmng "1.0" REQUIRED)
find_package(ledcontrol "1.0" REQUIRED)
```
依赖声明：libev（事件循环）、nanomsg（IPC）、OpenSSL、tbox-common（工具库）、cJSON（JSON 解析）、uthash（哈希表）、appmng（进程保活）、ledcontrol（LED 控制）。

```cmake
option(ENABLE_DISPATCH_OPTIMIZE "Enable dispatch optimize" ON)
if (ENABLE_DISPATCH_OPTIMIZE)
    add_compile_options(-DENABLE_DISPATCH_OPTIMIZE)
endif()
```
编译优化开关，默认开启，通过 `-DENABLE_DISPATCH_OPTIMIZE` 控制优化路径。

```cmake
add_compile_options(-D_GNU_SOURCE)
```
启用 GNU 扩展（如 `popen`、`getifaddrs` 等 POSIX 扩展 API）。

#### 4.1.1 EC200A 平台分支

```cmake
if ( "$ENV{QL_MODULE_PLATFORM}" STREQUAL "EC200A")
    add_compile_options(-DUSE_EC200A_DIAL)
    set(MY_INCLUDE_DIRS $ENV{QL_SYSROOT_DIR}/usr/include ...)
    set(MY_LINK_LIBS -lql_sdk -lql_sys_log -lql_lib_ipc ...)
    # 编译 dialer_lib 共享库（供 SDK 内部调用）
    add_library(dialer_lib SHARED
        ec200a/apn/apn.c
        ec200a/data_call/data_call.c
        ec200a/nw/nw.c
        ec200a/sim/sim.c
        ec200a/test_utils/test_utils.c
    )
```
EC200A 使用 OpenWrt 工具链（`arm-openwrt-linux`），链接 Quectel 专有 SDK 库（`ql_sdk`, `ql_sys_log` 等），同时编译一个 `dialer_lib.so` 供内部调用。

```cmake
    # 可执行文件
    add_executable(${PROJECT_NAME} dialer_ec200a.cpp ...)
    target_link_libraries(${PROJECT_NAME}
        -lev -lnanomsg -lcrypto -ljson-c -ltbox-common
        -lpthread -lappmng -lledcontrol -lsqlite3 -lz
        ${MY_LINK_LIBS})
```
最终可执行文件链接上所有依赖库。

#### 4.1.2 EG25G 平台分支

```cmake
elseif ( "$ENV{QL_MODULE_PLATFORM}" STREQUAL "EG25G")
    add_compile_options(-DUSE_EG25_DIAL)
    set(CMAKE_CXX_STANDARD 11)
    # 使用 QMI 接口库
    set(STD_LIB
        libql_mgmt_client.so libdsi_netctrl.so
        libdsutils.so libqmiservices.so libqmi_cci.so ...)
    file(GLOB_RECURSE SRC_FILES "eg25/*.c" "cc_deque/*.c")
    add_executable(${PROJECT_NAME}
        dialer_eg25.c nanomsg_process_wraper.cpp nanomsg_process.cpp
        traffic_sql/traffic_sql_interface.cpp
        logger_sd.c ${SRC_FILES})
    target_link_libraries(${PROJECT_NAME}
        -lpthread -lm -lql_sys_log -lcjson -ljson-c -lrt
        -lappmng -lnanomsg -lsqlite3 -lledcontrol ...)
```
EG25G 使用 Yocto/OE 工具链（`arm-oe-linux-gnueabi`），通过 QMI 接口（`libdsi_netctrl`）控制拨号，需要额外的 MCM 接口库。

#### 4.1.3 i.MX6ULL 平台分支

```cmake
elseif ( "$ENV{QL_MODULE_PLATFORM}" STREQUAL "MCIMX6Y2CVM08AB")
    add_compile_options(-DUSE_IMX6U_DIAL)
    add_executable(${PROJECT_NAME}
        dialer_imx6ull.cpp nanomsg_process_wraper.cpp nanomsg_process.cpp
        traffic_sql/traffic_sql_interface.cpp
        cc_deque/cc_deque.c
        fault_report/NetworkMonitor.cpp
        fault_report/SimSignalMonitor.cpp
        fault_report/SimMonitor.cpp ${SRC_FILES})
    target_link_libraries(${PROJECT_NAME}
        -lpthread -lm -lcjson -lrt -lappmng -lnanomsg -lsqlite3)
```
i.MX6ULL 通过 USB CDC（ttyUSBx）与 modem 通信，使用 GCC ARM HF 工具链（`arm-linux-gnueabihf`），加入 fault_report 故障监控模块。

#### 4.1.4 通用后处理

```cmake
# 运行等级
set(STARTUP_LEVEL_APP 1)
target_compile_definitions(${PROJECT_NAME} PRIVATE STARTUP_LEVEL=1)
# 生成启动等级 JSON
add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo
    "{\"startup_level\": 1}" > modem_mng_startup_level.json)
# strip（不同平台工具链不同）
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND arm-openwrt-linux-strip ...)
# 安装路径
install(TARGETS ${PROJECT_NAME} DESTINATION usr/bin)
install(FILES libledcontrol.so ... DESTINATION usr/lib)
```
构建后自动 strip 二进制，生成启动等级配置文件，安装到 `usr/bin`。

---

## 5. 核心模块逐行分析

### 5.1 dialer.hpp — 抽象拨号器与多平台类定义

#### 5.1.1 拨号状态枚举

```cpp
typedef enum {
    dial_stat_none,           // 初始/未开始
    dial_stat_init,           // modem 初始化
    dial_stat_sim_init,       // SIM 初始化
    dial_stat_sim_check,      // SIM 检查中
    dial_stat_sim_op,         // SIM 操作中
    dial_stat_reg_check,      // 网络注册检查
    dial_stat_precondition_check,// 前置条件检查
    dial_stat_pre_start_call, // 拨号前准备
    dial_stat_start_call,     // 开始拨号
    dial_stat_stop_call,      // 停止拨号
    dial_stat_stop_cfun,      // 关闭射频
    dial_stat_start_cfun,     // 开启射频
    dial_stat_list_oper,      // 列出运营商
    dial_stat_select_oper,    // 选择运营商
    dial_stat_wait_for_connect, // 等待连接
    dial_stat_net_connected,  // 已连接
} dial_stat_enu;
```
这是整个拨号状态机的核心枚举，每个状态对应一个拨号阶段。

#### 5.1.2 抽象基类 Dialer

```cpp
class Dialer {
public:
    virtual void startDialing() = 0;   // 启动拨号
    virtual void stopDialing() = 0;    // 停止拨号
    virtual int modem_init() = 0;      // modem 硬件初始化
    virtual int create_call() = 0;     // 创建 Data Call 对象
    virtual int start_call() = 0;      // 启动 Data Call
    virtual void stop_call() = 0;      // 停止 Data Call
    virtual void stop_cfun() = 0;      // 关闭射频（AT+CFUN=0）
    virtual void start_cfun() = 0;     // 开启射频（AT+CFUN=1）
    virtual void dial_loop(void *cpactive,
                           std::string &interface_name,
                           std::function<void()> callback) = 0;
    // 状态读取/写入（protected setStatus，public getStatus）
    DialStatEun getStatus() const;
protected:
    void setStatus(DialStatEun status);
private:
    DialStatEun status_ = dial_stat_none;
};
```
`dial_loop` 是核心：传入 `cpactive`（进程保活句柄）、`interface_name`（OUT 参数，拨通后填入网卡名）、`callback`（拨通后的回调函数）。

#### 5.1.3 EC200ADialer 类关键成员（声明于 dialer.hpp）

```cpp
class EC200ADialer : public Dialer {
public:
    static std::string dev_name;     // 拨号成功后的网卡名（如 "ccinet1"）
    static std::string imsi;         // SIM IMSI（静态，供 ModemReqHandler 读取）
    static std::string imei;         // 设备 IMEI
    static std::string operatorName; // 运营商名称
    static std::string plmn;         // PLMN 码
    static std::string iccid;        // SIM ICCID
    bool tz_fetched_ = false;        // 时区是否已获取（避免重复获取）

    // 时区解析（从 date -R 输出中解析 +0800 → 32 个 15 分钟单位）
    bool parseTimezone(const std::string& response, int& tz);
    // 时区保存（写入 /usr/dial/tz.ini 使用 iniparser）
    void saveTimezone(int tz);
    
    // 改进版 Ping：用 popen 读 stdout，检查 "ttl=" 关键字
    // 解决 SIGCHLD 导致 system() 返回值错误的问题
    bool test_can_ping_google();
    
    // TCP 连接检测：尝试连接 8.8.8.8:53
    bool isConnectedToInternet();
    
    // CFUN 调用限制机制（最多 10 次，间隔 ≥ 600s）
    void restart_cfun_safe();
    
    // 快速失败重试计数
    int check_and_update_retry_count();
    void clear_retry_count();
    
    // 网卡就绪检测
    int is_interface_present(const char *ifname);
    int wait_for_interface(const char *ifname, int timeout_sec);
    
    // 拨号回调（Quectel SDK 异步通知）
    static void data_call_status_ind_cb(int call_id,
        QL_NET_DATA_CALL_STATUS_E pre_call_status,
        ql_data_call_status_t *p_msg);
    
    void dial_loop(void *cpactive,
                   std::string &interface_name,
                   std::function<void()> callback) override;
private:
    LED_ID_t led_ch;           // LED 通道 ID
    bool apn_issetted;         // APN 是否已配置
    int callid;                // Data Call ID（拨号失败后自增）
    bool isExist;              // 退出标志（SIGTERM 触发）
    std::unique_ptr<dial_mng_t> p_dial_mng; // SDK 拨号管理句柄
};
```

**`data_call_status_ind_cb` 连接成功时的处理（重点）**：

```cpp
// 连接成功：
system("echo 0 > /tmp/dial_Status");   // 0 = 已连接
system("echo 1 > /tmp/network_status");
// 配置默认路由
snprintf(cmd_buf, ..., "ip ro add default via %s dev %s",
         p_msg->addr.gateway, p_msg->device);
system(cmd_buf);
// 配置 iptables NAT
system("iptables -t filter -F");
snprintf(cmd_buf, ..., "iptables -t nat -A POSTROUTING -o %s -j MASQUERADE", ...);
system(cmd_buf);
// 写入 DNS 到 /tmp/resolv_v4.conf 并追加到 /etc/resolv.conf
fp = fopen("/tmp/resolv_v4.conf", "w");
fprintf(fp, "nameserver %s\r\n", p_msg->addr.dnsp);
fprintf(fp, "nameserver %s\r\n", p_msg->addr.dnss);
system("cat /tmp/resolv_v4.conf >> /etc/resolv.conf");

// 断开连接：
system("echo 1 > /tmp/dial_Status");
system("echo 0 > /tmp/network_status");
```

---

### 5.2 dialer_ec200a.cpp — EC200A 拨号主逻辑

#### 5.2.1 辅助函数

```cpp
// AT 命令发送（通过 serial_atcmd 工具）
int Ql_SendAT(const char *atCmd) {
    return run_cmd("serial_atcmd %s", atCmd);
}
// run_cmd 内部用 vsnprintf + system() 执行
```
所有 AT 命令通过系统调用 `serial_atcmd` 工具发送，而不是直接打开串口文件。这是 EC200A Open SDK 的设计模式——SDK 本身管理串口访问权限。

```cpp
// timespec 差值计算（用于拨号超时判断）
struct timespec diff(struct timespec start, struct timespec end) {
    // 处理 tv_nsec 借位（纳秒下溢）
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
}
```

```cpp
// 从 AT+CSQ 输出提取信号值（逐行扫描，健壮性高）
int EC200ADialer::get_csq_value_safe() {
    FILE *fp = popen("serial_atcmd at+csq", "r");
    while (fgets(line, sizeof(line), fp)) {
        char *p = strstr(line, "+CSQ:");
        if (p) {
            p += 5;                     // 跳过 "+CSQ:"
            sscanf(p, "%d", &rssi);     // 解析第一个数字
            break;
        }
    }
    pclose(fp);
    return rssi;  // 返回 0-31，或 99（无信号），或 -1（失败）
}
```

```cpp
// AT+CEREG? 解析：获取注册状态码
int EC200ADialer::get_cereg_status_safe() {
    // 输出示例: "+CEREG: 3,1,\"272d\",\"0d17c147\",7"
    // 需要第一个逗号后的数字（注册状态）
    p = strchr(p, ',');  // 找第一个逗号
    p++;                 // 跳过逗号
    status = atoi(p);    // 读注册状态（遇到非数字自动停止）
}
```

```cpp
// AT+CPIN? 解析：获取 SIM 卡状态字符串
void EC200ADialer::get_cpin_status_str(char *out_buf, int len) {
    // 查找 "+CPIN: " 前缀
    char *p = strstr(line, "+CPIN: ");
    p += 7;              // 跳过前缀
    // 截断尾部 \r\n
    while (*end && *end != '\r' && *end != '\n') end++;
    *end = '\0';
    strncpy(out_buf, p, len - 1);
}
```

#### 5.2.2 CFUN 调用限制机制

```cpp
#define MAX_CFUN_CALLS    10      // 最多调用 10 次
#define MIN_CFUN_INTERVAL 600     // 最小间隔 600 秒
#define CFUN_FILE  "/tmp/cfun_count.txt"
#define TIME_FILE  "/tmp/cfun_last_call.txt"

void EC200ADialer::restart_cfun_safe() {
    int count = read_cfun_count();
    if (count >= MAX_CFUN_CALLS) {
        dial_log("CFUN 调用次数已达到上限\n");
        return;  // 拒绝执行，记录日志
    }
    double current_time = get_system_uptime();  // 读 /proc/uptime
    double last_call_time = read_last_call_time();
    if (current_time - last_call_time < MIN_CFUN_INTERVAL) {
        dial_log("CFUN 调用间隔不足 600 秒\n");
        return;
    }
    Ql_SendAT("AT+CFUN=0");  // 关闭射频
    sleep(5);
    Ql_SendAT("AT+CFUN=1");  // 开启射频
    write_cfun_count(count + 1);
    write_last_call_time(current_time);
}
```
使用**系统 uptime** 而非 wall clock，避免 RTC 不准确的问题。

#### 5.2.3 快速失败重试计数

```cpp
#define RETRY_COUNT_FILE     "/tmp/dial_retry_count"
#define MAX_FAST_RETRY_TIMES 3
#define FAST_FAIL_TIMEOUT_MS 10000  // 10 秒

int EC200ADialer::check_and_update_retry_count() {
    // 读取 /tmp/dial_retry_count 中的当前次数
    fp = fopen(RETRY_COUNT_FILE, "r");
    fscanf(fp, "%d", &count);
    
    if (count >= MAX_FAST_RETRY_TIMES) return -1; // 已超限，不再快速退出
    
    count++;
    fp = fopen(RETRY_COUNT_FILE, "w");
    fprintf(fp, "%d", count);
    fflush(fp);
    fsync(fileno(fp));  // 确保落盘（防止崩溃时丢失计数）
    return count;       // 返回当前次数（1, 2, 3）
}

void EC200ADialer::clear_retry_count() {
    // 仅当文件存在时才删除（减少系统调用开销）
    if (access(RETRY_COUNT_FILE, F_OK) == 0) {
        unlink(RETRY_COUNT_FILE);
    }
}
```

#### 5.2.4 网卡就绪检测

```cpp
int EC200ADialer::is_interface_present(const char *ifname) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);  // 临时套接字
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    // SIOCGIFINDEX：获取网卡索引号；不存在返回 -1
    if (ioctl(sock, SIOCGIFINDEX, &ifr) >= 0) ret = 1;
    close(sock);
    return ret;
}

int EC200ADialer::wait_for_interface(const char *ifname, int timeout_sec) {
    while (elapsed < timeout_sec) {
        if (is_interface_present(ifname)) return 1; // 成功
        if (elapsed % 2 == 0) dial_log("等待网卡... (%d/%ds)\n", elapsed, timeout_sec);
        sleep(1);
        elapsed++;
    }
    dial_log("Error: 超时等待 %s\n", ifname);
    return 0; // 失败
}
```

#### 5.2.5 dial_loop() — 核心拨号循环（最关键函数）

```cpp
void EC200ADialer::dial_loop(void *cpactive,
                             std::string &interface_name,
                             std::function<void()> callback)
{
    // ① 处理快速失败重试计数
    int current_launch_count = check_and_update_retry_count();
    int is_fast_fail_mode = (current_launch_count != -1);
    
    // ② 内层初始化循环（最多失败 5 分钟后退出）
    const uint64_t INIT_MAX_FAIL_DURATION_MS = 5 * 60 * 1000;
    while (1) {
        // 步骤 2-1: ql_data_call_init() 最多重试 20s（200次×100ms）
        retry_cnt = 20*1000/100;
        while (retry_cnt > 0) {
            ret = ql_data_call_init();
            if (ret == QL_ERR_SERVICE_NOT_READY) { usleep(100000); retry_cnt--; continue; }
            break;
        }
        if (ret != QL_ERR_OK) { /* 记录失败时间，sleep 5s，continue */ }
        
        // 步骤 2-2: 注册状态回调
        ql_data_call_set_status_ind_cb(data_call_status_ind_cb);
        
        // 步骤 2-3: ql_data_call_create(g_call_id, "auto_network", 0)
        // 步骤 2-4: 配置 APN_ID=1, IPv4, 重连间隔 35s
        // 步骤 2-5: ql_data_call_start(g_call_id)
        // 全部成功则 break 进入巡检循环
    }
    
    // ③ 巡检循环（五状态机）
    enum Stage { ST_STATUS, ST_SIM, ST_SIGNAL, ST_PING, ST_RECOVERY };
    Stage stage = ST_STATUS;
    uint64_t start_fail_ts = 0;   // 首次 Ping 失败时间
    int has_notified_connect = 0; // 是否已通知连接成功
    
    while (1) {
        uint64_t tnow = now_ms();
        cpactive_upt_atime(cpactive);  // 喂保活看门狗
        
        // 快速失败检测
        if (is_fast_fail_mode && !has_notified_connect) {
            if (tnow - start_loop_ts > FAST_FAIL_TIMEOUT_MS) {
                dial_log("Fast Fail: 10s 内无连接，退出\n");
                exit(1);  // 让保活脚本重新拉起
            }
        }
        
        // 心跳日志（每 30s）
        if (tnow > next_heartbeat_ts) {
            get_cpin_status_str(sim_str, ...);
            int rssi = get_csq_value_safe();
            int reg_stat = get_cereg_status_safe();
            dial_log("[HEARTBEAT] SIM:%s | REG:%d | CSQ:%d | DownTime:%lds\n", ...);
        }
        
        switch (stage) {
            case ST_STATUS:  // 读取 g_last_call_status → 转入 ST_SIM
            case ST_SIM:     // 每 800ms 调 checkSimCardStatus()
            case ST_SIGNAL:  // 每 800ms 解析 AT+CSQ
            case ST_PING:    // 每 1500ms 调 test_can_ping_google()
                if (ok) {
                    clear_retry_count(); // 成功后清计数
                    interface_name = dev_name;
                    callback();          // 通知 main 线程设置网卡名
                    has_notified_connect = 1;
                    LEDControl_controlLight(handle, ledId, 1); // 亮灯
                } else {
                    // 计算断网时长，触发分级恢复
                    if (fail_duration > LEVEL3_TIMEOUT) recovery_level = 3;
                    else if (fail_duration > LEVEL2_TIMEOUT) recovery_level = 2;
                    else if (fail_duration > LEVEL1_TIMEOUT) recovery_level = 1;
                    if (recovery_level > 0) stage = ST_RECOVERY;
                }
            case ST_RECOVERY:
                switch (recovery_level) {
                    case 1: // 软重拨
                        ql_data_call_stop(g_call_id); sleep(2);
                        ql_data_call_start(g_call_id);
                    case 2: // 射频重置
                        restart_cfun_safe(); sleep(10);
                        ql_data_call_start(g_call_id);
                    case 3: // 模块重启（当前注释）
                        sync(); sleep(5);
                }
                has_notified_connect = 0;
                stage = ST_STATUS;
        }
        usleep(50 * 1000);  // 50ms 轻微 sleep，避免 busy loop
    }
}
```

#### 5.2.6 modem_init() / create_call() / start_call()

```cpp
int EC200ADialer::modem_init() {
    ql_nw_init();
    get_signal_strength(&p_dial_mng->level);
    // 信号不好则返回 -1（调用方会重试）
    if ((level != GOOD) && (level != GREAT) && (level != MODERATE)
        && ((signalValue == -1) || (signalValue == 99)))
        return -1;
    p_dial_mng->dial_st = dial_stat_call_init;
    return 0;
}

int EC200ADialer::create_call() {
    ql_data_call_create(callid, "public", 0);
    ql_data_call_param_set_apn_id(p_param, DATA_CALL_APN_PUBLIC);   // APN ID=6
    ql_data_call_param_set_ip_version(p_param, QL_NET_IP_VER_V4);   // IPv4
    ql_data_call_param_set_reconnect_mode(p_param, RECONNECT_NORMAL);
    time_interval_list[0] = 20;  // 20 秒自动重连间隔
    ql_data_call_config(callid, p_param);
}

int EC200ADialer::start_call() {
    ql_data_call_start(callid);
    p_dial_mng->dial_st = dial_stat_wait_for_connect;
    clock_gettime(CLOCK_MONOTONIC, &cur_timer);  // 记录拨号开始时间
}
```

---

### 5.3 nanomsg_process.hpp — IPC 服务端声明

#### 5.3.1 ModemReqHandler（modem 状态查询器）

```cpp
class ModemReqHandler {
public:
    // 构造时预加载基础信息
    ModemReqHandler(std::string resolv_file = "/etc/resolv.conf") {
        getModel();    // 读 /etc/quectel-project-version
        getImei();     // AT+CGSN（EC200A）或 MCM API（EG25G）
        getImsi();     // AT+CIMI（EC200A）或 MCM API（EG25G）
        get4gIp();     // ip addr show <interface> | grep inet
    }
    
    // 核心接口（均返回字符串，供 JSON 序列化）
    std::string getStatus();    // 组装完整 JSON 状态对象
    std::string getModel();     // 从 /etc/quectel-project-version 解析
    std::string getImei();
    std::string getImsi();
    std::string get4gIp();      // 解析 ip addr show 输出
    std::string getIccid();
    std::string getOperatorName();
    std::string getPlmn();      // AT+COPS? → 提取引号内字符串
    int getCsq();               // AT+CSQ → 解析信号值
    bool isRoaming();           // AT+CREG → 检查状态 5=漫游
    std::string getDns();       // 解析 /etc/resolv.conf
    std::string getApn();       // AT+CGDCONT? → 正则提取 APN
    int getNetworkOnline();     // 连接 8.8.8.8:53 TCP 测试
    static int getCCINetStats(int &rx, int &tx, std::string net_interface);
    // 读 /sys/class/net/<interface>/statistics/rx_bytes|tx_bytes
};
```

#### 5.3.2 NanoReqHandler（REP 服务端）

```cpp
class NanoReqHandler {
public:
    NanoReqHandler(const std::string &url)
        : modemReqHandlerPtr(new ModemReqHandler()), url(url) {
        sock = nn_socket(AF_SP, NN_REP);  // nanomsg REP 模式
        nn_bind(sock, url.c_str());       // 绑定 tcp://127.0.0.1:38001
    }
    void processModemRequests();  // 主循环（在独立线程中运行）
    
    static std::string netif_name;  // 网卡名（由 dial_loop 回调填入）
};
```

#### 5.3.3 NetWork_EventPublisher（PUB 事件发布者）

```cpp
class NetWork_EventPublisher {  // 单例模式
public:
    static NetWork_EventPublisher &getInstance() {
        static NetWork_EventPublisher instance;
        return instance;
    }
    
    bool init(const std::string &url) {
        sock = nn_socket(AF_SP, NN_PUB);  // nanomsg PUB 模式
        nn_bind(sock, url.c_str());       // 绑定 tcp://127.0.0.1:48001
    }
    
    bool pub(const std::string &event,
             const std::string &description,
             int event_code) {
        // 组装 JSON: { "ts": time, "event": { "all": { "names": [...], "data": [[ts, code, desc]] } } }
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "ts", (double)time(NULL));
        // ... 构建嵌套结构 ...
        nn_send(sock, msg, strlen(msg)+1, 0);
    }
};
```

#### 5.3.4 TrafficMonitor（流量监控）

```cpp
class TrafficMonitor {
public:
    static int PacketLimit;  // 流量阈值（字节），从 config.json C020102 读取
    
    TrafficMonitor(std::string netcard) {
        // 从 SQLite 数据库加载上次记录的流量值（防止程序重启丢数据）
        TrafficDatabase *db = TrafficDatabase::getInstance();
        TrafficData lastRecord_Month = db->getLastRecordFromDB(db->dbRef, "MonthlyTraffic");
        TrafficData lastRecord_History = db->getLastRecordFromDB(db->dbRef, "HistoryTraffic");
        // 初始化各种流量计数器...
        cur_FlowInBytes = MonthlyTraffic_init_total; // 当月累计
        history_FlowInBytes = lastRecord_History.totalValue; // 历史累计
    }
    
    void check_CCINet_FlowOverLimit() {
        // 启动独立线程，每 10s 检查一次
        std::thread monitorOverLimit([this]() {
            while (!stopMonitor) {
                checkTrafficOverLimit();  // 读网卡统计，更新累计值
                if (cur_FlowInBytes > PacketLimit && PacketLimit > 0) {
                    // 每 10 次发布一次（避免刷屏）
                    if (++overlimit_Pub_freq % 10 == 1)
                        NetWork_EventPublisher::getInstance().pub("TrafficOverLimit", ..., 6);
                }
                // 每 60s（6次×10s）写入 SQLite
                if (++refresh_todisk_file >= 6) {
                    // 跨月处理：重置当月计数
                    if (!dbInstance->isLatestRecordMonthSameAsSystem())
                        reset_flow_on_nextmonth(...);
                    else
                        dbInstance->updateOldestRecord("MonthlyTraffic", ...);
                    dbInstance->updateOldestRecord("HistoryTraffic", ...);
                }
                cv.wait_for(lk, 10s, []{return stopMonitor;});
            }
        });
        monitorOverLimit.detach();
    }
};
```

---

### 5.4 nanomsg_process.cpp — IPC 服务端实现

#### 5.4.1 getStatus() — 状态 JSON 组装

```cpp
std::string ModemReqHandler::getStatus() {
    getCCINetStats(rxBytes, txBytes, net_interface); // 读网卡统计
    getDns();                                        // 解析 /etc/resolv.conf
    
    cJSON *cellular = cJSON_CreateObject();
    cJSON_AddStringToObject(cellular, "model",   model.c_str());
    cJSON_AddStringToObject(cellular, "imei",    getImei().c_str());
    cJSON_AddStringToObject(cellular, "imsi",    getImsi().c_str());
    cJSON_AddStringToObject(cellular, "iccid",   getIccid().c_str());
    cJSON_AddStringToObject(cellular, "lac",     lac.c_str());    // 位置区码
    cJSON_AddStringToObject(cellular, "cellid",  ci.c_str());     // 小区 ID
    cJSON_AddNumberToObject(cellular, "csq",     getCsq());       // 信号强度
    cJSON_AddNumberToObject(cellular, "roaming", isRoaming());    // 是否漫游
    cJSON_AddStringToObject(cellular, "operator",getOperatorName().c_str());
    cJSON_AddStringToObject(cellular, "register",getRegisterStatus().c_str());
    cJSON_AddNumberToObject(cellular, "network_type", getNetworkType());
    cJSON_AddStringToObject(cellular, "plmn",    getPlmn().c_str());
    cJSON_AddNumberToObject(cellular, "network_online", isConnectedToInternet());
    cJSON_AddStringToObject(cellular, "ip",      ip_4g.c_str());
    cJSON_AddStringToObject(cellular, "dns1",    dns1.c_str());
    cJSON_AddStringToObject(cellular, "dns2",    dns2.c_str());
    cJSON_AddStringToObject(cellular, "gateway", changeLastOctetToOne(ip_4g).c_str());
    cJSON_AddStringToObject(cellular, "apn",     getApn().c_str());
    cJSON_AddNumberToObject(cellular, "tx",      traffic.get_history_tx_bytes()/1024); // KB
    cJSON_AddNumberToObject(cellular, "rx",      traffic.get_history_rx_bytes()/1024); // KB
    
    return std::string(cJSON_Print(cellular));
}
```

**网关推导**：由于没有直接 API 获取网关，代码通过将 IP 最后一段改为 1 推算（如 `10.64.64.64` → `10.64.64.1`）。这是一种假设，在少数情况下可能不准确。

#### 5.4.2 processModemRequests() — REP 服务端主循环

```cpp
void NanoReqHandler::processModemRequests() {
    while (!isExit) {
        // 阻塞等待请求（nn_recv，无超时）
        int req_bytes = nn_recv(sock, &req_msg, NN_MSG, 0);
        
        // 失败处理：最多重试 10 次，超限后尝试重连
        if (req_bytes < 0) {
            retries++;
            if (retries >= max_retries) {
                initialize_connection(sock, url);  // 重建 socket 并重新 bind
                retries = 0;
            }
            sleep(1);
            continue;
        }
        
        // 解析请求 JSON
        cJSON *request = cJSON_Parse(req_msg);
        cJSON *ts = cJSON_GetObjectItem(request, "ts");
        cJSON *status = cJSON_GetObjectItem(request, "status");
        cJSON *network = cJSON_GetObjectItem(status, "network");  // 数组
        
        // 遍历请求的数据类型
        for (int i = 0; i < networkSize; i++) {
            if (item_str.find("cellular") != std::string::npos) {
                // 组装响应
                std::string ret_str = modemReqHandlerPtr->getStatus();
                // 包装成 { "ts": ..., "status": { "network": { "cellular": {...} } } }
                nn_send(sock, out, strlen(out)+1, 0);
            }
        }
    }
}
```

**请求格式示例**：
```json
{
  "ts": 1700000000,
  "status": {
    "network": ["cellular"]
  }
}
```

**响应格式示例**：
```json
{
  "ts": 1700000001,
  "status": {
    "network": {
      "cellular": {
        "model": "EC200A",
        "imei": "123456789012345",
        "csq": 25,
        "roaming": 0,
        "network_online": 1,
        "ip": "10.64.64.64",
        "tx": 1024,
        "rx": 2048
      }
    }
  }
}
```

---

### 5.5 logger_sd.h / logger_sd.c — SD 卡日志系统

```c
/* 初始化流程 */
void log_init(void) {
    if (g_log_fp != NULL) return;  // 防止重复初始化
    
    // 1. 检查 /media/sdcard 是否已挂载（解析 /proc/mounts）
    if (!is_sdcard_mounted()) return; // 仅控制台输出
    
    // 2. 检查可用空间（statvfs）：需 > 500MB
    unsigned long long free_mb = stat.f_bavail * stat.f_frsize / (1024*1024);
    if (free_mb < MIN_FREE_SPACE_MB) return;
    
    // 3. 创建目录 /media/sdcard/dial_log/
    system("mkdir -p /media/sdcard/dial_log");
    
    // 4. 生成文件名：dial_YYYYMMDD_HHMMSS.log
    snprintf(filename, ..., "%s/dial_%04d%02d%02d_%02d%02d%02d.log", ...);
    
    // 5. 打开文件，写入头部，立即刷新
    g_log_fp = fopen(filename, "w");
    fprintf(g_log_fp, "=== Dial Program Started [...] ===\n");
    fflush(g_log_fp);
}

/* 日志写入 */
void dial_log(const char *fmt, ...) {
    // 生成时间戳 [YYYY-MM-DD HH:MM:SS]
    // 同时输出到 stdout 和 g_log_fp
    // va_list 必须 start 两次（一次 vprintf，一次 vfprintf）
    // 每条日志后立即 fflush()，防掉电丢日志
}
```

---

### 5.6 parse_config.hpp — JSON 配置解析器

```cpp
// 从 /etc/config/config.json 解析配置
// 文件格式: {"config": {"data": {"C020102": 500, "C010101": 250}}}
class ConfigParser {
public:
    ConfigParser(const std::string &filename) {
        // 一次性读入全文件，用 cJSON 解析
        cJSON *config = cJSON_GetObjectItem(root, "config");
        cJSON *config_data = cJSON_GetObjectItem(config, "data");
        cJSON *item = config_data->child;
        while (item) {
            if (cJSON_IsNumber(item)) data[item->string] = Variant_Config(item->valuedouble);
            else if (cJSON_IsString(item)) data[item->string] = Variant_Config(item->valuestring);
            item = item->next;
        }
    }
    Variant_Config getValue(const std::string &key) const;
};
```

使用示例（main 中）：
```cpp
ConfigParser parser("/etc/config/config.json");
getIntValue(parser, "C020102", TrafficMonitor::PacketLimit);
// C020102 为流量阈值配置项
```

---

### 5.7 traffic_sql/sql_traffic.hpp — 流量数据库

```cpp
class TrafficDatabase {  // 单例
private:
    TrafficDatabase() {
        system("mkdir -p /media/sdcard/traffic/");
        openTrafficDatabase();  // 打开或创建 SQLite 数据库
    }
    
    void initTables() {
        // 创建两张表（如不存在）
        CREATE TABLE IF NOT EXISTS MonthlyTraffic (
            ID INTEGER PRIMARY KEY AUTOINCREMENT,
            TotalValue REAL,    -- 总流量（字节）
            SentValue REAL,     -- 上行
            ReceivedValue REAL, -- 下行
            TrafficThreshold REAL,
            RecordTime TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS HistoryTraffic (...); // 相同结构
        
        // 若表为空，预填充 60 条默认记录（滚动存储）
        for (int i = 0; i < 60; i++) INSERT INTO ... VALUES (0, 0, 0, 0, '1970-01-01 ...')
    }
    
    // 跨月检测：比较最新记录的月份与系统时间
    bool isLatestRecordMonthSameAsSystem();
    
    // 更新最旧记录（循环覆写，节省存储空间）
    void updateOldestRecord(const string &table, double total, double tx, double rx, ...);
    
    // 获取最新记录（供 TrafficMonitor 初始化时加载）
    TrafficData getLastRecordFromDB(sqlite3 *db, const string &table);
};
```

**设计特点**：采用固定 60 条记录的循环写入策略（而非无限追加），通过 `updateOldestRecord` 每次覆盖最旧的记录，适合存储空间受限的嵌入式设备。

---

### 5.8 reboot_conf/dial_reboot_conf.c — 重启守护配置

```c
// 配置结构体
typedef struct {
    int uptime;           // 首次断网时的系统 uptime
    int restart_flag;     // 是否已执行重启（防止重复）
    int first_disconnect; // 首次断网标志
    int signal_strength;  // 当前信号强度（AT+CSQ）
    int sim_status;       // SIM 卡状态
    int ql_netd_status;   // ql_netd 服务状态
} Config;

void handle_dial_up_down() {
    // 前置条件检查
    if (!check_pre_conditions()) return;  // 信号 > 20 且 SIM OK 且 ql_netd 正常
    if (g_config.restart_flag == 1) return;  // 已重启过，不重复
    
    // 超时判断
    int current_uptime = get_uptime();
    if (current_uptime - g_config.uptime > THRESHOLD_TIME) {
        g_config.restart_flag = 1;
        write_config();        // 持久化重启标志
        perform_reboot();      // 写 /tmp/reboot_flag → system("reboot")
    }
}
```

注意：`perform_reboot()` 写入 `/tmp/reboot_flag` 并等待 10 秒后执行 `reboot`，给系统保存数据的机会。

---

### 5.9 fault_report/ — 故障上报模块

#### NetworkMonitor — 网络可达性监控

```cpp
class PingNetworkChecker : public NetworkChecker {
public:
    bool checkNetwork() override;  // 通过 ping 检测网络可达性
};

class NetworkMonitor {
    void monitorLoop() {
        while (!stopFlag) {
            bool isUp = networkChecker->checkNetwork();
            monitorNetwork(isUp);  // 断网超时则上报事件
            sleep(检测间隔);
        }
    }
};
```

#### SimSignalMonitor — 信号强度监控

```cpp
// 信号 < 20 持续 5 分钟 → 发布 SIGNAL 低信号事件（事件码 208）
// 信号 > 25 持续 5 分钟 → 发布 SIGNAL 信号恢复事件（事件码 209）
void monitorSignal(int signalValue) {
    if (signalValue < 20) {
        if (!signalLow) {
            signalLow = true;
            lowSignalStartTime = now;
        } else if (持续 >= 5 分钟) {
            NetWork_EventPublisher::getInstance().pub("SIGNAL", "Signal strength low", 208);
        }
    }
}
```

#### SimMonitor — SIM 卡状态监控

```cpp
class EC200ASimChecker : public SimChecker {
    bool checkSim() override;  // sim_init() + sim_get_iccid() 验证
};

class SimMonitor {
    void checkAndReportSimStatus();  // 单次检查，检测 SIM 异常后上报事件
};
```

---

## 6. 主程序启动流程（main）

```
main() (dialer_ec200a.cpp)
│
├─ log_init()
│   └─ 检查 SD 卡 → 创建 /media/sdcard/dial_log/dial_YYYYMMDD_HHMMSS.log
│
├─ EC200ADialer dial;
│   dial.wait_for_interface("ccinet0", 30)
│   └─ ioctl SIOCGIFINDEX 轮询，超时退出
│
├─ create_cpactive()
│   cpactive_add_pinfo(cpactive, 60, "modem_mng", "1.30", NULL)
│   └─ 向 appmng 进程管理框架注册，超时 60s
│
├─ NanoReqHandler NanoReqHandle("tcp://127.0.0.1:38001")
│   └─ nn_socket(NN_REP) + nn_bind()
│
├─ std::thread nano_msg_thread(processModemRequests)
│
├─ NetWork_EventPublisher::getInstance().init("tcp://127.0.0.1:48001")
│   └─ nn_socket(NN_PUB) + nn_bind()
│
├─ ConfigParser parser("/etc/config/config.json")
│   └─ 读取 C020102 → TrafficMonitor::PacketLimit
│
├─ TrafficMonitor monitor("")
│   └─ 从 SQLite 加载历史流量数据
│
├─ signal(SIGINT/SIGTERM, signal_handler)
│   └─ 收到信号时设置 isExist=true，让 dial_loop 优雅退出
│
├─ std::thread dialThread(dial.dial_loop(..., callback))
│
├─ cv.wait_for(lock, 60s, []{ return isInterfaceSet; })
│   ├─ 成功: 打印网卡名称
│   └─ 超时: dial.set_isExist(true) → 触发退出
│
├─ monitor.check_CCINet_FlowOverLimit()  // 启动流量监控线程
│
├─ dialThread.join()
│
├─ monitor.stopMonitorCCINetStats()
│   monitor.cv.notify_all()
│
└─ log_close() → exit(0)
```

---

## 7. 拨号状态机完整流程图

```
程序启动
    │
    ▼
wait_for_interface("ccinet0", 30s)
    │ 失败 → exit(-1)
    ▼ 成功
check_and_update_retry_count()
    ├─ 1~3次: is_fast_fail_mode = true（10s超时）
    └─ >3次:  is_fast_fail_mode = false（持久模式）
    │
    ▼
┌─────────────────────────────────────────┐
│         内层初始化循环（最长5分钟）      │
│                                         │
│  ql_data_call_init()   ← 重试最多20s    │
│  ↓ 成功                                 │
│  ql_data_call_set_status_ind_cb()       │
│  ↓                                      │
│  ql_data_call_create(g_call_id, ...)    │
│  ↓                                      │
│  ql_data_call_param_alloc()             │
│  ql_data_call_param_set_apn_id(1)       │
│  ql_data_call_param_set_ip_version(V4)  │
│  ql_data_call_param_set_reconnect(35s)  │
│  ql_data_call_config()                  │
│  ↓                                      │
│  ql_data_call_start()  ← 成功则 break   │
└─────────────────────────────────────────┘
    │
    ▼
┌────────────────────────────────────────────────────────────────┐
│                    巡检循环（每轮 ~50ms）                        │
│                                                                │
│  ┌──────────┐  ┌─────────────┐  ┌──────────────┐             │
│  │ ST_STATUS│→ │  ST_SIM     │→ │  ST_SIGNAL   │             │
│  │读g_last  │  │每800ms      │  │每800ms        │             │
│  │call_stat │  │checkSimCard │  │AT+CSQ         │             │
│  └──────────┘  └─────────────┘  └──────┬───────┘             │
│                                         │                      │
│                                  ┌──────▼────────┐            │
│                                  │   ST_PING     │            │
│                                  │  每1500ms      │            │
│                                  │ping 8.8.8.8   │            │
│                                  └───┬───────┬───┘            │
│                              成功 ◄──┘       └──► 失败         │
│                                 │                  │           │
│                    ┌────────────▼──┐    ┌──────────▼────┐     │
│                    │ 清除故障计时   │    │ 记录断网时长   │     │
│                    │ clear_retry   │    │ 判断恢复等级   │     │
│                    │ callback()    │    │                │     │
│                    │ LED亮         │    │ <60s: 无操作   │     │
│                    └───────────────┘    │ >60s: L1重拨   │     │
│                                         │ >5min: L2射频  │     │
│                                         │ >30min: L3重启 │     │
│                                         └──────┬─────────┘     │
│                                                │               │
│                                   ┌────────────▼──────────┐    │
│                                   │     ST_RECOVERY        │    │
│                                   │ L1: stop+start call    │    │
│                                   │ L2: restart_cfun_safe()│    │
│                                   │ L3: sync()+sleep(5)    │    │
│                                   └────────────────────────┘    │
└────────────────────────────────────────────────────────────────┘
```

---

## 8. nanomsg IPC 通信协议

### 8.1 REQ/REP（tcp://127.0.0.1:38001）

**客户端请求格式**：
```json
{
  "ts": 1700000000,
  "status": {
    "network": ["cellular"]
  }
}
```

**服务端响应格式**：
```json
{
  "ts": 1700000001,
  "status": {
    "network": {
      "cellular": {
        "model": "EC200ACNTAR02A04M2G",
        "md_sw_ver": "v1.2.3",
        "md_hw_ver": "EC200A",
        "imei": "123456789012345",
        "imsi": "460001234567890",
        "iccid": "89860012341234567890",
        "phonenum": "186",
        "lac": "271a",
        "cellid": "0d176547",
        "csq": 25,
        "roaming": 0,
        "operator": "CHINA MOBILE",
        "register": "undefined",
        "network_type": 1,
        "plmn": "46000",
        "network_online": 1,
        "ip": "10.64.64.64",
        "dns1": "218.4.4.4",
        "dns2": "218.4.40.40",
        "gateway": "10.64.64.1",
        "apn": "cmnet",
        "tx": 1024,
        "rx": 2048
      }
    }
  }
}
```

### 8.2 PUB/SUB（tcp://127.0.0.1:48001）

**事件消息格式**：
```json
{
  "ts": 1700000000,
  "event": {
    "all": {
      "names": ["ts", "event_code", "description"],
      "data": [[1700000000, 6, "packets over limit"]]
    }
  }
}
```

**已定义的事件码**：

| 事件名          | 事件码 | 触发条件                       |
|-----------------|--------|-------------------------------|
| TrafficOverLimit| 6      | 当月流量超过 PacketLimit 阈值  |
| NoTraffic       | 7      | 60s 内网卡无流量变化           |
| SIGNAL          | 208    | 信号强度 < 20 持续 5 分钟      |
| SIGNAL          | 209    | 信号强度 > 25 持续 5 分钟      |

---

## 9. 流量监控与 SQLite 持久化

### 9.1 流量计算公式

```
当月已用流量（字节）:
  cur_FlowInBytes = MonthlyTraffic_init_total   // 上次记录的当月基准值
                  + (rxBytes - rx_offset_Bytes)  // 本次启动后的下行增量
                  + (txBytes - tx_offset_Bytes)  // 本次启动后的上行增量

历史累计流量:
  history_FlowInBytes = init_history_FlowInBytes  // 历史基准值
                      + (rxBytes - his_rx_offset_Bytes)
                      + (txBytes - his_tx_offset_Bytes)
```

### 9.2 跨月处理

```cpp
// 每月 1 号：检测到 SQLite 最新记录的月份与系统月份不同
if (!dbInstance->isLatestRecordMonthSameAsSystem()) {
    // 计算上月增量（用当前累计 - 历史记录的基准值）
    MonthlyTraffic_init_tx    = tx_cur_FlowInBytes - record_last.sentValue;
    MonthlyTraffic_init_rx    = rx_cur_FlowInBytes - record_last.receivedValue;
    MonthlyTraffic_init_total = MonthlyTraffic_init_tx + MonthlyTraffic_init_rx;
    // 更新 MonthlyTraffic 表（从零开始新月统计）
    db->updateOldestRecord("MonthlyTraffic", MonthlyTraffic_init_total, ...);
    // 重置偏移值
    rx_offset_Bytes = rxBytes;
    tx_offset_Bytes = txBytes;
}
```

### 9.3 数据库文件位置

```
/media/sdcard/traffic/traffic.db
    ├── MonthlyTraffic   # 60条循环滚动，记录当月流量
    └── HistoryTraffic   # 60条循环滚动，记录历史累计流量
```

---

## 10. 分级故障恢复策略

| 等级 | 触发阈值    | 操作                                     | 执行间隔保护 |
|------|-------------|------------------------------------------|-------------|
| L1   | 断网 > 60s  | `ql_data_call_stop` + `ql_data_call_start` | 最小 60s    |
| L2   | 断网 > 5min | `restart_cfun_safe()`（AT+CFUN=0→1）    | 最小 5min   |
| L3   | 断网 > 30min| `sync()` + 等待（模块重启可选开启）       | —           |

恢复流程：
```
1. 记录 last_recovery_ts（防止频繁触发）
2. 执行对应级别的恢复操作
3. 重置 has_notified_connect = 0（触发重新上报连接事件）
4. 返回 ST_STATUS 状态重新开始检测
```

`restart_cfun_safe()` 带有双重保护：
- 调用次数上限：10次（写入 `/tmp/cfun_count.txt`）
- 调用间隔下限：600s（写入 `/tmp/cfun_last_call.txt`，基于 `/proc/uptime`）

---

## 11. 快速失败重试机制

```
程序启动（第1次）
    ├─ 写入 /tmp/dial_retry_count = 1
    ├─ is_fast_fail_mode = true
    └─ 10秒内未 Ping 通 → exit(1) 让保活脚本重启

程序重启（第2次）
    ├─ 读取计数=1，写入=2
    ├─ 仍处于快速失败模式
    └─ 同上

程序重启（第3次）
    ├─ 读取计数=2，写入=3
    ├─ 仍处于快速失败模式
    └─ 同上

程序重启（第4次）
    ├─ 读取计数=3 >= MAX(3)，返回 -1
    ├─ is_fast_fail_mode = false
    └─ 进入持久模式（不再 10s 超时退出）

Ping 成功后：
    └─ unlink("/tmp/dial_retry_count") → 计数清零
```

---

## 12. CFUN 调用限制机制

```
restart_cfun_safe() 调用流程：

1. read_cfun_count()           读 /tmp/cfun_count.txt
   count >= 10 → 拒绝，记日志，返回
   
2. get_system_uptime()         读 /proc/uptime（浮点）
   read_last_call_time()       读 /tmp/cfun_last_call.txt
   当前 uptime - 上次调用时间 < 600s → 拒绝，返回
   
3. Ql_SendAT("AT+CFUN=0")     关射频
   sleep(5)
   Ql_SendAT("AT+CFUN=1")     开射频
   
4. write_cfun_count(count+1)  更新计数
   write_last_call_time(current_uptime)  更新时间
   dial_log("重置 CFUN\n")
```

使用 `uptime`（系统启动时间）而非 wall clock 的原因：EC200A 平台可能没有 RTC，重启后 wall clock 会重置为 epoch，导致间隔计算错误。

---

## 13. SD 卡日志系统

### 13.1 初始化判断逻辑

```
log_init()
    │
    ├─ 解析 /proc/mounts 查找 "/media/sdcard" 挂载点
    │   └─ 未挂载 → 仅控制台输出
    │
    ├─ statvfs("/media/sdcard")
    │   └─ 可用空间 < 500MB → 仅控制台输出
    │
    ├─ system("mkdir -p /media/sdcard/dial_log")
    │
    └─ 打开 /media/sdcard/dial_log/dial_YYYYMMDD_HHMMSS.log
        └─ 写入头部 + fflush()
```

### 13.2 dial_log() 双写机制

每条日志同时写入 `stdout`（串口/SSH 调试）和 SD 卡文件，并在每条日志后立即 `fflush()`，确保掉电时最后几条日志不丢失。

---

## 14. LED 状态控制

`dial_loop` 内通过 `LEDControl_create("tcp://127.0.0.1:26008")` 连接到 LED 控制服务。

| LED 状态码 | 显示状态   | 触发条件       |
|------------|------------|----------------|
| 0          | 灭灯       | Ping 失败      |
| 1          | 闪灯（0x01，周期 0xf4） | 启动拨号阶段   |
| 2          | 常亮       | Ping 成功，网络正常 |

平台 LED 映射：
```cpp
if (led_type == 0) { // PRO 型号
    ledId.pro = LED_NET_GREEN;
} else {             // VBOX 型号
    ledId.vbox = EX_GPIO_LED_NET;
}
```

---

## 15. 多平台差异对比

| 功能                  | EC200A                           | EG25G                           | i.MX6ULL                        |
|-----------------------|----------------------------------|---------------------------------|---------------------------------|
| 拨号 API              | `ql_data_call_*()` SDK           | QMI + `libdsi_netctrl`          | AT 命令（ttyUSB）               |
| 网卡名称              | `ccinet0/1/...`                  | `rmnet_data0`                   | `usb0`                          |
| IMSI 获取             | `serial_atcmd at+cimi`           | `QL_MCM_SIM_GetIMSI()`          | `AT+CIMI` via ttyUSB            |
| IMEI 获取             | `serial_atcmd at+cgsn`           | `AT+CGSN` via AT 通道           | `AT+GSN` via ttyUSB             |
| ICCID 获取            | SDK `sim_get_iccid()`            | `QL_MCM_SIM_GetICCID()`         | `AT+QCCID` via ttyUSB           |
| 运营商名              | 由 PLMN 码推算                   | `AT+COPS?` 带缓存（60s）        | `AT+COPS?` via ttyUSB           |
| 时区获取              | `date -R` 命令解析 `+0800`       | AT+QLTS（EG25 专有命令）        | 不支持                          |
| modem 上电控制        | SDK 管理                         | SDK 管理                        | GPIO export + 模拟按键脉冲      |
| 故障恢复              | 分级策略（L1/L2/L3）             | 未移植（基础循环）              | 独立 fault_report 线程          |
| 日志系统              | SD 卡 + 控制台                   | SD 卡 + 控制台                  | 无 SD 卡日志                    |
| C++ 标准              | C++14                            | C++11                           | C++11                           |
| 工具链                | arm-openwrt-linux                | arm-oe-linux-gnueabi            | arm-linux-gnueabihf             |

---

## 16. 编译安装全流程

### 16.1 环境准备

#### EC200A 平台

```bash
# 前置条件
# 1. 安装 EC200A Open SDK
# 2. 设置环境变量
export QL_MODULE_PLATFORM=EC200A
export QL_SYSROOT_DIR=/path/to/ql-ol-sysroots
source /path/to/ql-ol-extsdk/sdk.sh  # 加载工具链

# 3. 安装依赖库（需提前交叉编译并安装到 sysroot）
#    nanomsg、cjson、sqlite3、tbox-common、appmng、ledcontrol
```

#### EG25G 平台

```bash
export QL_MODULE_PLATFORM=EG25G
export QL_SDKPATH=/path/to/eg25-sdk
export QL_SDK_TARGET_SYSROOT=/path/to/eg25-sdk/sysroots/armv7a-vfp-neon-oe-linux-gnueabi
source /path/to/eg25-sdk/environment-setup-armv7a-vfp-neon-oe-linux-gnueabi
```

#### i.MX6ULL 平台

```bash
export QL_MODULE_PLATFORM=MCIMX6Y2CVM08AB
# 安装 arm-linux-gnueabihf 工具链
sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

### 16.2 构建步骤

```bash
# 进入仓库根目录
cd modem_mng

# 创建构建目录
mkdir build && cd build

# 配置
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain.cmake \
    -DENABLE_DISPATCH_OPTIMIZE=ON \
    -DENABLE_SPI_DUMMY=OFF

# 编译（-j4 并行）
make -j4

# 安装（需指定 sysroot 或 DESTDIR）
make DESTDIR=/tmp/install install
```

构建产物：
- `modem_mng`（可执行文件，已 strip）
- `modem_mng_startup_level.json`（内容：`{"startup_level": 1}`）
- `dialer_lib.so`（仅 EC200A，Quectel SDK 调用库）

### 16.3 部署到设备

```bash
# SCP 上传
scp modem_mng root@<device_ip>:/usr/bin/
scp modem_mng_startup_level.json root@<device_ip>:/etc/appmng/

# 如果部署 ledcontrol 库
scp libledcontrol.so* root@<device_ip>:/usr/lib/

# 设置权限
chmod +x /usr/bin/modem_mng

# 创建必要目录
mkdir -p /etc/config          # 存放 config.json
mkdir -p /usr/dial            # 存放 tz.ini 等运行时文件
mkdir -p /media/sdcard        # SD 卡挂载点（硬件相关）
```

### 16.4 配置文件 /etc/config/config.json

```json
{
  "config": {
    "data": {
      "C020102": 524288000
    }
  }
}
```
`C020102` 为流量阈值，单位字节（示例：500MB = 524288000）。

### 16.5 运行

```bash
# 直接运行（调试）
/usr/bin/modem_mng

# 通过 appmng 守护运行（生产）
# appmng 读取 modem_mng_startup_level.json 中的 startup_level=1
# 并根据配置自动拉起程序

# 验证 nanomsg REP 服务
# 在另一终端发送测试请求
echo '{"ts":1700000000,"status":{"network":["cellular"]}}' | \
    nanomsg_client tcp://127.0.0.1:38001
```

---

## 17. 运行时文件与临时文件汇总

| 文件路径                                      | 读/写 | 用途                                        |
|-----------------------------------------------|-------|---------------------------------------------|
| `/tmp/dial_Status`                            | W     | 0=已连接，1=断开（由回调写入）              |
| `/tmp/network_status`                         | W     | 1=联网，0=断网                              |
| `/tmp/dial_attempt_time.txt`                  | RW    | 射频开关时间阈值检查（120s 内不重复）       |
| `/tmp/cfun_count.txt`                         | RW    | CFUN 调用次数（上限 10）                   |
| `/tmp/cfun_last_call.txt`                     | RW    | CFUN 上次调用的 uptime（单位秒）           |
| `/tmp/dial_retry_count`                       | RW    | 快速失败重试次数（上限 3）                  |
| `/tmp/callid`                                 | W     | 当前 Data Call ID（拨号失败后自增写入）     |
| `/tmp/resolv_v4.conf`                         | W     | SDK 回调写入的 IPv4 DNS                    |
| `/tmp/resolv_v6.conf`                         | W     | SDK 回调写入的 IPv6 DNS                    |
| `/tmp/sdcard_avl`                             | W     | SD 卡可用性标志（000=不可用）              |
| `/etc/resolv.conf`                            | W     | 系统 DNS（由回调清空并重写）               |
| `/usr/dial/tz.ini`                            | RW    | 时区信息持久化（iniparser INI 格式）        |
| `/etc/config/config.json`                     | R     | 应用配置（流量阈值等）                     |
| `/etc/quectel-project-version`                | R     | 模块型号和固件版本                         |
| `/proc/uptime`                                | R     | 系统启动时间（CFUN 间隔计算）              |
| `/proc/mounts`                                | R     | SD 卡挂载检测                              |
| `/sys/class/net/<if>/statistics/rx_bytes`     | R     | 网卡接收字节数                             |
| `/sys/class/net/<if>/statistics/tx_bytes`     | R     | 网卡发送字节数                             |
| `/media/sdcard/dial_log/dial_*.log`           | W     | SD 卡拨号日志                              |
| `/media/sdcard/traffic/traffic.db`            | RW    | 流量统计 SQLite 数据库                     |

---

## 18. 关键数据流总览

```
物理层（EC200A 模块）
    │
    │ Quectel Open SDK（ql_data_call_*）
    ▼
data_call_status_ind_cb()              ← SDK 异步回调
    │ 写入 dev_name（网卡名）
    │ 写入 /tmp/dial_Status, /tmp/network_status
    │ 执行 ip route add / iptables NAT / resolv.conf
    ▼
dial_loop() 巡检循环
    │ ST_PING → test_can_ping_google()
    │           成功 → callback() → setInterfaceName()
    │                               设置 NanoReqHandler::netif_name
    │                               设置 TrafficMonitor::interface
    │           失败 → 故障恢复状态机
    ▼
TrafficMonitor (独立线程)
    │ 每 10s 读 /sys/class/net/.../rx_bytes|tx_bytes
    │ 累加计算 cur_FlowInBytes
    │ 超限 → NetWork_EventPublisher::pub("TrafficOverLimit", ...)
    │ 每 60s 写入 SQLite
    ▼
NetWork_EventPublisher
    │ nn_send → tcp://127.0.0.1:48001 (PUB)
    ▼
外部订阅者（消费事件）

NanoReqHandler (独立线程)
    │ nn_recv ← tcp://127.0.0.1:38001 (REP)
    │ 解析 {"status":{"network":["cellular"]}}
    │ 调用 ModemReqHandler::getStatus()
    │   └─ 组装 JSON（IMEI/IMSI/CSQ/IP/DNS/流量...）
    │ nn_send → 返回响应 JSON
    ▼
外部查询者（获取蜂窝状态）
```

---

*文档由代码逐行分析生成，覆盖 modem_mng v1.30 全部源文件（101 个文件，约 87 万字节）。*
