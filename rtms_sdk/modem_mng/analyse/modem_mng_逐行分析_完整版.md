# modem_mng 源码逐行深度分析文档

> **版本**：1.30 | **平台**：EC200A / EG25G / i.MX6ULL | **分析粒度**：每一行代码

---

## 目录

1. [CMakeLists.txt — 构建系统逐行](#1-cmakeliststxt)
2. [Makefile — EC200A 旧版手动构建](#2-makefile)
3. [ec200a/_public.h — EC200A 公共头文件](#3-ec200a_publich)
4. [ec200a/dial.h — 拨号状态机结构定义](#4-ec200adialh)
5. [ec200a/sim/sim.h / sim.c — SIM 卡模块](#5-ec200asimsimh--simc)
6. [ec200a/apn/apn.h / apn.c — APN 管理模块](#6-ec200aapnapnh--apnc)
7. [ec200a/nw/nw.h — 网络模块头文件](#7-ec200anwnwh)
8. [ec200a/data_call/data_call.h / data_call.c — Data Call 状态模块](#8-ec200adata_calldata_callh--data_callc)
9. [eg25/at/at.h / at.c — EG25 AT 命令模块](#9-eg25atathat--atc)
10. [eg25/nw/nw.h / nw.c — EG25 网络模块](#10-eg25nwnwh--nwc)
11. [imx_led.hpp — i.MX6ULL GPIO LED 控制](#11-imx_ledhpp)
12. [reboot_conf/dial_reboot_conf.h / .c — 重启守护配置](#12-reboot_confdial_reboot_confh--c)
13. [fault_report/NetworkMonitor.h / .cpp — 网络故障监控](#13-fault_reportnetworkmonitorh--cpp)
14. [fault_report/SimSignalMonitor.h / .cpp — 信号强度监控](#14-fault_reportsimsignalmonitorh--cpp)
15. [fault_report/SimMonitor.h / .cpp — SIM 卡状态监控](#15-fault_reportsimmonitorh--cpp)
16. [nanomsg_process_cinterface.h — C 语言 IPC 接口声明](#16-nanomsg_process_cinterfaceh)
17. [nanomsg_process_wraper.cpp — C++ 到 C 的包装层](#17-nanomsg_process_wrapercpp)
18. [parse_config.hpp — JSON 配置解析器](#18-parse_confighpp)
19. [traffic_sql/sql_traffic.hpp — 流量 SQLite 数据库](#19-traffic_sqlsql_traffichpp)
20. [logger_sd.h / logger_sd.c — SD 卡日志系统](#20-logger_sdh--logger_sdc)

---

## 1. CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.11)
```
**逐行**：要求 CMake 最低版本 3.11。低于此版本时构建直接报错退出。3.11 引入了 `FetchContent` 和改进的 `find_package` 行为，是本项目依赖特性的下限。

```cmake
project(modem_mng VERSION 1.0)
```
定义项目名称 `modem_mng`，版本号 1.0。这会自动设置 `PROJECT_NAME`、`PROJECT_VERSION` 等 CMake 变量，版本信息在 install 时可用。

```cmake
find_package(libev "4.33" REQUIRED)
```
查找 libev 库（版本 ≥ 4.33），未找到则报错退出。libev 是高性能事件循环库，在 EC200A 平台用于异步 I/O 事件处理（`-lev` 链接）。

```cmake
find_package(nanomsg "1.2" REQUIRED)
```
查找 nanomsg 库（版本 ≥ 1.2）。nanomsg 是 ZeroMQ 的继承者，提供 REQ/REP 和 PUB/SUB 消息模式，是本项目 IPC 通信的核心。

```cmake
find_package(openssl "1.1.1" REQUIRED)
```
查找 OpenSSL（版本 ≥ 1.1.1），用于加密通信（`-lcrypto` 链接）。

```cmake
find_package(tbox-common "1.0" REQUIRED)
```
查找内部 tbox-common 工具库（版本 ≥ 1.0），包含项目通用工具函数。

```cmake
find_package(cjson "1.7.15" REQUIRED)
```
查找 cJSON 库（版本 ≥ 1.7.15）。cJSON 是轻量级 C JSON 解析库，用于构建和解析所有 IPC 消息。

```cmake
find_package(uthash "2.3.0" REQUIRED)
```
查找 uthash（版本 ≥ 2.3.0）。uthash 是纯头文件的哈希表库，仅包含宏定义，用于快速键值查找。

```cmake
find_package(appmng "1.0" REQUIRED)
```
查找 appmng 进程管理库（版本 ≥ 1.0）。appmng 提供 `create_cpactive`、`cpactive_add_pinfo`、`cpactive_upt_atime` 等函数，用于进程保活心跳。

```cmake
find_package(ledcontrol "1.0" REQUIRED)
```
查找 LED 控制库（版本 ≥ 1.0）。提供 `LEDControl_create`、`LEDControl_controlLight`、`LEDControl_blinkLight` 等接口。

```cmake
#find_package(sqlite "3.39.0" REQUIRED)
```
**已注释**：原本计划通过 CMake find_package 引入 SQLite，但实际上 SQLite 直接通过 `-lsqlite3` 链接，无需 CMake 包管理。

```cmake
option(ENABLE_DISPATCH_OPTIMIZE "Enable dispatch optimize" ON)
if (ENABLE_DISPATCH_OPTIMIZE)
    add_compile_options(-DENABLE_DISPATCH_OPTIMIZE)
endif()
```
声明编译选项 `ENABLE_DISPATCH_OPTIMIZE`，默认开启（ON）。若开启，向所有编译单元添加 `-DENABLE_DISPATCH_OPTIMIZE` 宏定义。该宏在代码中用于启用事件分发优化路径（减少锁竞争或轮询频率）。

```cmake
option(ENABLE_SPI_DUMMY "Enable SPI Dummy" OFF)
if(ENABLE_SPI_DUMMY)
    add_compile_options(-DENABLE_SPI_DUMMY)
endif()
```
声明 SPI 哑数据选项，默认关闭。`-DENABLE_SPI_DUMMY` 用于测试时注入虚拟 SPI 数据，替代真实硬件读取。生产环境不启用。

```cmake
# set(CMAKE_FIND_LIBRARY_SUFFIXES ".so") ######使用动态链接
```
**已注释**：原本强制只查找动态库（.so），但现在改为让 CMake 自动决定动静态链接策略。

```cmake
add_compile_options(-D_GNU_SOURCE)
```
为所有编译单元添加 `-D_GNU_SOURCE` 宏。这是告诉 glibc 暴露 GNU 扩展 API 的关键宏，不定义它则 `popen`、`getifaddrs`、`strdup` 等函数可能无法声明。在交叉编译环境中此宏尤为重要。

```cmake
# add_compile_options(-UUSE_EC200A_DIAL)  取消宏定义USE_EC200A_DIAL
```
**已注释**：`-U` 取消宏定义，此处调试时用于在 EC200A SDK 环境中临时禁用 EC200A 专属拨号路径。

```cmake
if (CMAKE_CROSSCOMPILING)
```
检查是否正在交叉编译。只有在设置了 CMake toolchain file 时此变量才为真。所有平台差异化编译都在此分支内，本地原生编译则跳过。

---

### EC200A 平台分支（`$ENV{QL_MODULE_PLATFORM}` == "EC200A"）

```cmake
message(STATUS "Use Quectel SDK, module type = $ENV{QL_MODULE_PLATFORM}")
```
打印状态信息，输出当前平台类型，便于构建日志排查。

```cmake
if ( "$ENV{QL_MODULE_PLATFORM}" STREQUAL "EC200A")
    add_compile_options(-DUSE_EC200A_DIAL)
```
读取环境变量 `QL_MODULE_PLATFORM`，若为 "EC200A" 则定义 `USE_EC200A_DIAL` 宏。所有 `#ifdef USE_EC200A_DIAL` 代码块都通过此宏激活。

```cmake
    set(MY_COMPILE_OPTIONS -DQL_MODULE_PLATFORM_EC200A)
```
设置平台宏 `QL_MODULE_PLATFORM_EC200A`，代码中可用此宏区分同一大类下的子平台变体。

```cmake
    set(MY_INCLUDE_DIRS $ENV{QL_SYSROOT_DIR}/usr/include $ENV{QL_SYSROOT_DIR}/usr/include/ql-sdk)
```
设置头文件搜索路径：
- `$ENV{QL_SYSROOT_DIR}/usr/include`：系统标准头文件（如 json-c, sqlite3）
- `$ENV{QL_SYSROOT_DIR}/usr/include/ql-sdk`：Quectel SDK 专有头文件（ql_data_call.h, ql_sim.h 等）

```cmake
    set(MY_LIB_DIRS $ENV{QL_SYSROOT_DIR}/usr/lib)
```
设置库搜索路径，指向 sysroot 的 usr/lib，包含 Quectel SDK 动态库。

```cmake
    set(MY_LINK_LIBS -lql_sdk -lql_sys_log -lql_lib_ipc -lql_lib_utils -lprop2uci -luci -lubox -llog)
```
链接的 Quectel 专有库：
- `-lql_sdk`：核心 SDK（data call, sim, nw API）
- `-lql_sys_log`：Quectel 系统日志（ALOGI/ALOGE 宏）
- `-lql_lib_ipc`：IPC 通信支持
- `-lql_lib_utils`：工具函数
- `-lprop2uci`/`-luci`/`-lubox`：OpenWrt UCI 配置系统接口
- `-llog`：Android-风格日志接口

```cmake
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
```
EC200A 平台使用 C++14 标准（支持 `std::make_unique`、lambda 改进等）。`REQUIRED ON` 表示不能降级，编译器不支持则报错。

```cmake
    include_directories($ENV{QL_SYSROOT_DIR}/usr/include)
    include_directories($ENV{QL_SYSROOT_DIR}/usr/include/ql-sdk)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ec200a)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ec200a/test_utils)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ec200a/sim)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ec200a/nw)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ec200a/apn)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ec200a/data_call)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/traffic_sql)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/fault_report)
```
逐个添加包含目录。注意 `include_directories` 是全局性的（影响所有 target），现代 CMake 推荐用 `target_include_directories`，但此项目沿用旧风格。每个 ec200a 子模块目录单独添加，避免路径歧义。

```cmake
    add_library(dialer_lib SHARED
        ec200a/apn/apn.c
        ec200a/data_call/data_call.c
        ec200a/nw/nw.c
        ec200a/sim/sim.c
        ec200a/test_utils/test_utils.c
    )
```
将 EC200A 平台的 C 源文件编译成共享库 `libdialer_lib.so`。这是为了让 Quectel Open SDK 的 OCPU 机制能够动态加载这些功能模块。共享库与主可执行文件分离，利于独立升级。

---

### EG25G 平台分支

```cmake
elseif ( "$ENV{QL_MODULE_PLATFORM}" STREQUAL "EG25G")
    add_compile_options(-DUSE_EG25_DIAL)
```
EG25G 平台激活 `USE_EG25_DIAL` 宏，使用不同的拨号 API（QMI 接口而非 Open SDK）。

```cmake
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED True)
```
EG25G 使用 C++11 标准（比 EC200A 低一个版本），兼容 Yocto/OE SDK 工具链的旧版 GCC。

```cmake
    set(MY_INCLUDE_DIRS $ENV{QL_SDKPATH}/../ql-ol-extsdk/include)
```
EG25G 扩展 SDK 的头文件目录，包含 `ql_wwan_v2.h`（WWAN 接口）、`ql_oe.h`（OpenEmbedded 接口）等。

```cmake
    set(MY_LINK_LIBS -lql_peripheral)
```
EG25G 链接外设控制库，提供串口、GPIO 等硬件访问封装。

```cmake
    set_source_files_properties(nanomsg_process.cpp PROPERTIES LANGUAGE CXX)
```
强制 `nanomsg_process.cpp` 用 C++ 编译器处理（即使扩展名可能被误判），确保 `extern "C"` 块正确处理名字改编问题。

```cmake
    include_directories($ENV{QL_SDKPATH}/../ql-ol-extsdk/include)
    include_directories($ENV{QL_SDKPATH}/usr/include)
    link_directories($ENV{QL_SDKPATH}/../ql-ol-extsdk/lib)
    link_directories($ENV{QL_SDKPATH}/usr/lib)
```
EG25G 有两层 SDK 目录：`ql-ol-extsdk`（扩展 SDK）和 `ql-sdkpath/usr`（系统 sysroot），分别包含不同的头文件和库文件。

```cmake
    include_directories(.
        ${QL_SDK_PATH}/lib/interface/inc
        $ENV{QL_SDK_TARGET_SYSROOT}/../../../ql-ol-extsdk/include
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/include
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/include/libxml2
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/include/ql-manager
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/include/data
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/include/dsutils
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/include/qmi
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/include/qmi-framework
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/include/json-c
        eg25/sim  eg25/nw  eg25/apn  eg25/at
        eg25/dial  eg25/json_old  traffic_sql/
        roamlink_dial/  cc_deque/  eg25/tz
    )
```
EG25G 需要大量头文件目录，涵盖：
- `libxml2`：XML 解析（QMI 配置可能用到）
- `ql-manager`：Quectel 管理接口
- `data`/`dsutils`：QMI 数据服务工具
- `qmi`/`qmi-framework`：QMI 接口（用于与调制解调器通信的低级协议）
- `json-c`：JSON 解析（EG25G 使用 json-c 而非 cJSON）
- 本地子模块目录

```cmake
    set(STD_LIB
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/lib/libql_mgmt_client.so
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/lib/libdsi_netctrl.so
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/lib/libdsutils.so
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/lib/libqmiservices.so
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/lib/libqmi_cci.so
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/lib/libqmi_common_so.so
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/lib/libqmi.so
        $ENV{QL_SDK_TARGET_SYSROOT}/usr/lib/libmcm.so)
```
EG25G 核心 QMI 库：
- `libql_mgmt_client`：Quectel 管理客户端
- `libdsi_netctrl`：数据服务接口网络控制
- `libdsutils`：数据服务工具
- `libqmiservices`：QMI 服务集合
- `libqmi_cci`：QMI 客户端控制接口
- `libqmi_common_so`：QMI 公共定义
- `libqmi`：QMI 核心库
- `libmcm`：MCM（Modem Control Manager）接口，用于 SIM/NW 操作

```cmake
    set(DUAL_LIB $ENV{QL_SDKPATH}/../ql-ol-extsdk/lib/libql_common_api.a)
```
`libql_common_api.a` 是静态库，包含通用 API（WWAN v2 数据拨号接口）。使用静态库避免运行时路径依赖问题。

```cmake
    file(GLOB_RECURSE SRC_FILES "eg25/*.c" "cc_deque/*.c")
```
递归查找 `eg25/` 和 `cc_deque/` 目录下所有 `.c` 文件，自动加入编译列表。`GLOB_RECURSE` 的缺点是增加新文件后需要重新 cmake，但简化了 CMakeLists 的维护。

```cmake
    add_executable(${PROJECT_NAME}
        dialer_eg25.c
        nanomsg_process_wraper.cpp
        nanomsg_process.cpp
        traffic_sql/traffic_sql_interface.cpp
        logger_sd.c
        ${SRC_FILES})
```
EG25G 可执行文件的源文件列表：
- `dialer_eg25.c`：EG25G 的 `main()` 入口（纯 C 实现）
- `nanomsg_process_wraper.cpp`：C++ IPC 层对 C 的包装
- `nanomsg_process.cpp`：IPC 服务端核心逻辑
- `traffic_sql_interface.cpp`：SQLite 流量接口
- `logger_sd.c`：SD 卡日志系统（C 实现）
- `${SRC_FILES}`：eg25/ 和 cc_deque/ 下所有 C 文件

```cmake
    target_link_libraries(${PROJECT_NAME}
        -Wl,-dn -Wl,-dy
        -lpthread -lm -lql_sys_log -lcjson -ljson-c -lrt
        -lappmng -lnanomsg -lsqlite3 -lledcontrol -lz
        -ltbox-common -llog ${DUAL_LIB} ${STD_LIB} ${SINGLE_LIB})
    target_link_libraries(${PROJECT_NAME} stdc++)
```
链接选项详解：
- `-Wl,-dn`：从此处开始只链接静态库（`--static`）
- `-Wl,-dy`：从此处开始恢复动态链接（`--dynamic`）
- `-Wl,-dn -Wl,-dy`：这对组合实际上什么都不做（立即切换回动），真正意图是强调顺序敏感性
- `-lrt`：POSIX 实时扩展（`clock_gettime` 等）
- `-lz`：zlib 压缩库
- `stdc++`：C++ 标准库（单独添加确保 C 主文件也能链接 C++ 运行时）

---

### i.MX6ULL 平台分支

```cmake
elseif ( "$ENV{QL_MODULE_PLATFORM}" STREQUAL "MCIMX6Y2CVM08AB")
    set(MY_COMPILE_OPTIONS -DQL_MODULE_PLATFORM_MCIMX6Y2CVM08AB)
    add_compile_options(-DUSE_IMX6U_DIAL)
```
i.MX6ULL 平台标识符为完整芯片型号 `MCIMX6Y2CVM08AB`，激活 `USE_IMX6U_DIAL` 宏。此平台通过 USB CDC-NCM（`ttyUSBx`）与外挂 modem 通信，不使用 Quectel SDK。

```cmake
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/traffic_sql)
    include_directories(cc_deque/ fault_report/ ./)
```
i.MX6ULL 使用 cc_deque 双端队列（用于运营商列表管理）和 fault_report 故障监控模块。

```cmake
    add_executable(${PROJECT_NAME}
        dialer_imx6ull.cpp
        nanomsg_process_wraper.cpp
        nanomsg_process.cpp
        traffic_sql/traffic_sql_interface.cpp
        cc_deque/cc_deque.c
        fault_report/NetworkMonitor.cpp
        fault_report/SimSignalMonitor.cpp
        fault_report/SimMonitor.cpp
        ${SRC_FILES})
    target_link_libraries(${PROJECT_NAME}
        -Wl,-dn -Wl,-dy
        -lpthread -lm -lcjson -lrt -lappmng -lnanomsg -lsqlite3)
```
i.MX6ULL 不链接 Quectel SDK（无 -lql_sdk），不需要 LED 控制库（用内部 GPIO 实现），但加入了 fault_report 和 cc_deque。

---

### 通用后处理

```cmake
else ()
    message(FATAL_ERROR "QL_MODULE_PLATFORM not define!")
endif ()
```
若 `QL_MODULE_PLATFORM` 环境变量既不是 EC200A 也不是 EG25G 也不是 MCIMX6Y2CVM08AB，直接报致命错误，防止产生无平台支持的无效构建。

```cmake
add_compile_options(${MY_COMPILE_OPTIONS})
link_directories(${MY_LIB_DIRS})
```
应用各平台设置的编译选项和库搜索路径（在 `if/elseif/else` 块之后才生效）。

```cmake
if ( "$ENV{QL_MODULE_PLATFORM}" STREQUAL "EC200A")
    add_executable(${PROJECT_NAME} dialer_ec200a.cpp)
    target_sources(${PROJECT_NAME} PRIVATE
        nanomsg_process.cpp
        ec200a/apn/apn.c
        ec200a/data_call/data_call.c
        ec200a/nw/nw.c
        ec200a/sim/sim.c
        ec200a/test_utils/test_utils.c
        traffic_sql/traffic_sql_interface.cpp
        fault_report/NetworkMonitor.cpp
        fault_report/SimSignalMonitor.cpp
        fault_report/SimMonitor.cpp
        logger_sd.c)
```
EC200A 的 `add_executable` 和 `target_sources` 组合：先用主文件创建 target，再通过 `target_sources` 追加其余源文件。这是 CMake 3.1+ 推荐的分步添加源文件方式。

```cmake
    target_include_directories(dialer_lib PUBLIC
        ec200a/apn  ec200a/data_call  ec200a/nw
        ec200a/sim  ec200a/test_utils  ec200a/
        traffic_sql/  fault_report/
    )
```
为 `dialer_lib` 共享库设置公共包含目录（PUBLIC 意味着链接它的 target 也继承这些路径）。

```cmake
    target_link_libraries(${PROJECT_NAME}
        -lev -lnanomsg -lcrypto -ljson-c -ltbox-common
        -lpthread -lappmng -lledcontrol -lsqlite3 -lz
        ${MY_LINK_LIBS})
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${PROJECT_BINARY_DIR}"
        "${Foo_INCLUDE_DIRS}"
        ${MY_INCLUDE_DIRS}
    )
```
EC200A 可执行文件的链接库。注意 `${Foo_INCLUDE_DIRS}` 是一个无效引用（Foo 未定义），这是代码遗留问题，实际上不影响构建（展开为空）。

```cmake
    # add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # COMMAND ${STRIP} $<TARGET_FILE:${PROJECT_NAME}>)
```
**已注释**：原本 strip 命令通过 CMake 变量 `${STRIP}` 调用，现在已改为下面的显式工具链调用方式。

```cmake
set(STARTUP_LEVEL_APP 1)
target_compile_definitions(${PROJECT_NAME} PRIVATE STARTUP_LEVEL=${STARTUP_LEVEL_APP})
```
定义启动等级为 1（最高优先级），编译时以宏形式注入，代码中可用 `STARTUP_LEVEL` 查询自身优先级。

```cmake
add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo
    "{\\\"startup_level\\\": ${STARTUP_LEVEL_APP}}" >
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_startup_level.json
)
```
构建前自动生成 `modem_mng_startup_level.json`，内容为 `{"startup_level": 1}`。appmng 进程管理器读取此文件决定启动顺序和优先级。反斜杠转义是为了在 shell 中正确输出 JSON 格式的引号。

```cmake
if ( "$ENV{QL_MODULE_PLATFORM}" STREQUAL "EC200A")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND arm-openwrt-linux-strip ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME})
elseif ( "$ENV{QL_MODULE_PLATFORM}" STREQUAL "EG25G")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND arm-oe-linux-gnueabi-strip ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME})
elseif ( "$ENV{QL_MODULE_PLATFORM}" STREQUAL "MCIMX6Y2CVM08AB" )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND arm-linux-gnueabihf-strip ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME})
endif ()
```
构建后自动 strip 符号表，各平台使用对应的交叉编译 strip 工具：
- EC200A → `arm-openwrt-linux-strip`（OpenWrt 工具链）
- EG25G → `arm-oe-linux-gnueabi-strip`（Yocto/OE 工具链）
- i.MX6ULL → `arm-linux-gnueabihf-strip`（标准 ARM HF 工具链）

strip 可以将二进制文件缩小 60-80%，嵌入式系统存储空间有限时至关重要。

```cmake
install(TARGETS ${PROJECT_NAME} DESTINATION usr/bin)
```
将可执行文件安装到 `usr/bin`（相对于 `CMAKE_INSTALL_PREFIX` 或 `DESTDIR`）。执行 `make install` 或 `cmake --install` 时触发。

```cmake
install(FILES ${LEDCTL_LIB_DIR}/libledcontrol.so
              ${LEDCTL_LIB_DIR}/libledcontrol.so.1
              ${LEDCTL_LIB_DIR}/libledcontrol.so.1.0
        DESTINATION usr/lib)
```
将 LED 控制库的三个文件（主库 + 版本符号链接）一并安装到 `usr/lib`。三个文件的原因是 Linux 共享库版本化惯例：`.so`（最新符号链接）、`.so.1`（次版本链接）、`.so.1.0`（实际文件）。

---

## 2. Makefile

```makefile
CURR_DIR := $(shell pwd)
```
将当前工作目录存入变量 `CURR_DIR`。`$(shell ...)` 在 make 解析时立即执行，而不是在规则执行时。用于后续路径拼接。

```makefile
-include /home/xp/work/a200ec/ql-ol-extsdk-ec200acntar02a02m2g_ocpu/sdk.mk
```
以 `-include` 方式引入 SDK 的 make 片段（`-` 前缀表示文件不存在时静默忽略，不报错）。`sdk.mk` 通常定义 `CC`、`CXX`、`QL_SDK_CFLAGS`、`QL_SDK_LDFLAGS` 等工具链变量。这个路径是开发者本地路径（`/home/xp/...`），需要在其他机器上调整。

```makefile
INSTALL_PATH=/home/xp/work/a200ec/ql-ol-extsdk-ec200acntar02a02m2g_ocpu/ql-ol-rootfs/usr/dial/
```
定义安装路径（同样是本地绝对路径），指向 SDK rootfs 中的 `usr/dial` 目录。构建完成后将产物复制到此路径用于打包。

```makefile
DIAL_TARGET_EXE = dial
DIAL_TARGET_OBJ = dial.o sim.o apn.o nw.o test_utils.o data_call.o
```
定义旧版目标（目标名为 `dial` 而非 `modem_mng`），以及对应的目标文件列表。这是 Makefile 的历史遗留，现在主要用 CMake。

```makefile
SRC_DIR=.
APP_INCLUDE_DIRS += -I $(SRC_DIR)/
APP_INCLUDE_DIRS += -I $(SRC_DIR)/test_utils/
APP_INCLUDE_DIRS += -I $(SRC_DIR)/data_call/
APP_INCLUDE_DIRS += -I $(SRC_DIR)/apn/
APP_INCLUDE_DIRS += -I $(SRC_DIR)/sim/
APP_INCLUDE_DIRS += -I $(SRC_DIR)/nw/
APP_INCLUDE_DIRS += -I/home/xp/work/a200ec/ql-ol-extsdk-ec200acntar02a02m2g_ocpu/ql-sysroots/usr/include
```
逐行添加包含目录：`.`（当前目录）、各子模块目录、SDK sysroot。注意最后一个是绝对路径，其他都是相对路径。

```makefile
APP_SRC_FILES += $(shell find $(SRC_DIR)/ -name '*.c')
APP_OBJ_FILES := $(patsubst %.c, %.o, $(APP_SRC_FILES))
```
- `find $(SRC_DIR)/ -name '*.c'`：递归查找当前目录下所有 `.c` 文件
- `$(patsubst %.c, %.o, ...)`：将所有 `.c` 路径替换为 `.o`，得到目标文件列表

```makefile
SRC_FILES += $(APP_SRC_FILES)
INCLUDE_ALL_DIRS += $(APP_INCLUDE_DIRS)
```
将局部变量合并到全局变量中，为 all 规则准备。

```makefile
CFLAGS := $(QL_SDK_CFLAGS) $(QL_SDK_HARD_CFLAGS)
LDFLAGS := $(QL_SDK_LDFLAGS) $(QL_SDK_LIBS)
```
从 `sdk.mk` 中获取编译标志（包含硬浮点 `-mfpu=neon -mfloat-abi=hard` 等）和链接标志（包含 sysroot 路径、动态链接器路径等）。

```makefile
all: $(APP_OBJ_FILES)
    $(CXX) -Wall $(LDFLAGS) $(APP_OBJ_FILES) logger_sd.o dialer_ec200a.cpp \
        -o $(DIAL_TARGET_EXE) $(LD_FLAG) $(INCLUDE_ALL_DIRS) \
        -ljson-c -llog -lprop2uci -luci -lubox
    cp dial ~/output_debug
```
默认 target `all`：
1. 编译所有 `.c` 文件为 `.o`
2. 用 `CXX`（C++ 编译器）链接，因为 `dialer_ec200a.cpp` 是 C++ 文件
3. `-Wall`：开启所有警告
4. 额外链接 `-ljson-c -llog -lprop2uci -luci -lubox`（UC配置系统）
5. `cp dial ~/output_debug`：复制产物到 `~/output_debug` 便于调试

```makefile
%.o:%.c
    $(CC) -o $@ -c $^ $(CFLAGS) $(APP_INCLUDE_DIRS)
```
通用规则：将任意 `.c` 文件编译为同名 `.o`。`$@` 是目标文件，`$^` 是所有依赖（即源文件），`-c` 只编译不链接。

```makefile
clean:
    rm -rf *.o
    rm -rf $(DIAL_TARGET_EXE)
```
清理规则：删除所有目标文件和可执行文件。

---

## 3. ec200a/_public.h — EC200A 公共头文件

```c
#ifndef  _PUBLIC_H
#define  _PUBLIC_H
```
头文件保护宏，防止重复包含。使用下划线前缀的宏名是 POSIX 保留命名空间，但在嵌入式项目中十分常见。

```c
#ifdef __cplusplus
extern "C" {
#endif
```
C++ 兼容声明：若被 C++ 文件包含，将所有函数声明标记为 C 链接（不做名字改编）。这样 `dialer.hpp`（C++）就能直接调用 `_public.h` 中声明的 C 函数。

```c
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
```
标准 C 库头文件集合：涵盖 I/O、字符串处理、文件系统、进程控制、信号处理等全部基础功能。`sys/types.h` 出现了两次（第 4 行和第 11 行），属于冗余包含，但不会造成错误（头文件保护已处理）。

```c
#include "ql-sdk/ql_type.h"
#include "ql-sdk/ql_data_call.h"
#include "ql-sdk/ql_sim.h"
#include "ql-sdk/ql_nw.h"
```
Quectel SDK 专有头文件：
- `ql_type.h`：基础类型（`QL_ERR_OK`、`QL_ERR_FAILED` 等错误码，`QL_NW_SIGNAL_STRENGTH_LEVEL_E` 等枚举）
- `ql_data_call.h`：Data Call API（`ql_data_call_init`、`ql_data_call_create` 等）
- `ql_sim.h`：SIM 卡 API（`ql_sim_init`、`ql_sim_get_iccid` 等）
- `ql_nw.h`：网络 API（`ql_nw_init`、`get_signal_strength` 等）

```c
#include "dial.h"
#include "apn.h"
#include "nw.h"
#include "dial.h"
#include "data_call.h"
#include "test_utils.h"
```
项目自有头文件。注意 `dial.h` 被包含了**两次**（第 1 行和第 4 行），这是笔误，不影响编译（`#ifndef __DIAL_H__` 保护已处理），但属于代码质量问题。

```c
#include "json-c/json.h"
```
json-c 库头文件，用于 JSON 数据处理（EG25G 风格的 APN 配置解析使用 json-c 而非 cJSON）。

```c
#include "include/log.h"
```
Android-风格日志头文件，定义 `ALOGI`、`ALOGE`、`ALOGW` 等宏，输出带级别前缀的日志。

```c
#define DATA_CALL_APN_PUBLIC   6
#define DATA_CALL_ID_PUBLIC    1
#define APN_NAME_PUBLIC "apnpublic"
```
三个关键常量：
- `DATA_CALL_APN_PUBLIC 6`：公网 APN 的 ID 编号（SDK 内部 APN 槽位 6）
- `DATA_CALL_ID_PUBLIC 1`：默认 Data Call ID（后续拨号失败时会自增）
- `APN_NAME_PUBLIC "apnpublic"`：默认 APN 名称（当 ICCID 匹配不到时使用）

```c
#ifdef __cplusplus
}
#endif

#endif
```
关闭 `extern "C"` 块并结束头文件保护。

---

## 4. ec200a/dial.h — 拨号状态机结构定义

```c
#ifndef __DIAL_H__
#define __DIAL_H__
#include <stdint.h>
```
头文件保护 + `stdint.h`（提供 `uint64_t` 等固定宽度整型，确保跨平台类型一致性）。

```c
#ifdef __cplusplus
extern "C" {
#endif
```
C++ 兼容包装，确保 C++ 调用此头文件时函数签名不被改编。

```c
#include "sim.h"
#include <time.h>
#include <limits.h>
#include "nw.h"
```
- `sim.h`：包含 SIM 相关类型（`sim_mng_t`）
- `time.h`：提供 `struct timespec`（用于 `dial_mng_t.dial_timer`）
- `limits.h`：提供 `NAME_MAX` 等限制常量
- `nw.h`：提供 `QL_NW_SIGNAL_STRENGTH_LEVEL_E` 枚举

```c
#define DIAL_TIMEOUT_SECONDS (120)
```
拨号超时阈值：120 秒（2分钟）。在 `flow_monitor_task()` 和 `wait_for_connection()` 中使用：若 2 分钟内网卡无流量，或 SDK 未返回连接状态，则触发重拨。括号包裹是 C 宏安全写法，防止运算符优先级问题。

```c
#define DILA_IFNAME_TIMEOUT_CNT  (10)
```
网卡名称超时计数（拼写错误：`DILA` 应为 `DIAL`）。等待 ccinet 网卡出现的最大轮询次数，每次 1 秒，共等待 10 秒。

```c
typedef enum
{
    dial_stat_none,         // 0：初始状态，未开始
    dial_stat_call_init,    // 1：Data Call 初始化阶段
    dial_stat_apn_init,     // 2：APN 初始化阶段
    dial_stat_call_create,  // 3：创建 Data Call 对象
    dial_stat_call_start,   // 4：启动 Data Call
    dial_stat_stop,         // 5：停止状态（准备重拨）
    dial_stat_stop_cfun,    // 6：关闭射频（AT+CFUN=0）
    dial_stat_start_cfun,   // 7：开启射频（AT+CFUN=1）
    dial_stat_wait_for_connect, // 8：等待 SDK 回调连接成功
    dial_stat_net_connected,    // 9：已连接，进入流量监控
} dial_stat_enu;
```
EC200A 拨号状态机枚举。与 `dialer.hpp` 中的 `dial_stat_enu` 类似但不完全相同：
- 这里有 `dial_stat_apn_init` 和 `dial_stat_call_create`（老版本状态机的中间步骤）
- `dialer.hpp` 版本增加了 `dial_stat_reg_check` 等更细粒度的状态

```c
typedef struct
{
    dial_stat_enu dial_st;                  // 当前拨号状态
    QL_NW_SIGNAL_STRENGTH_LEVEL_E level;   // 当前信号强度等级
    struct timespec dial_timer;             // 拨号开始时间（用于超时判断）
} dial_mng_t;
```
拨号管理结构体：
- `dial_st`：驱动状态机的当前状态
- `level`：信号强度（NONE/POOR/MODERATE/GOOD/GREAT 五个等级），用于判断是否满足拨号前置条件
- `dial_timer`：使用 `CLOCK_MONOTONIC` 的绝对时间戳，不受系统时间调整影响

```c
extern dial_mng_t *dial_mng_new(void);
extern void *dial_task(void *arg);
```
两个外部函数声明：
- `dial_mng_new()`：分配并初始化一个 `dial_mng_t` 结构体（用于 EG25G 的 `pthread_create` 方式）
- `dial_task()`：拨号任务函数，符合 `pthread_create` 的线程函数签名 `void *(void *)`

---

## 5. ec200a/sim/sim.h / sim.c — SIM 卡模块

### sim.h

```c
#define SIM_OP_INIT (0)
#define SIM_OP_DEINIT (1)
#define SIM_OP_GET_IMSI (2)
#define SIM_OP_GET_ICCID (3)
#define SIM_BUF_SIZE (32)
```
SIM 操作码和缓冲区大小常量：
- 操作码 0-3 对应 init/deinit/get IMSI/get ICCID
- `SIM_BUF_SIZE 32`：字符缓冲区大小（IMSI 最长 15 位，ICCID 最长 20 位，32 字节足够）

```c
#define SIM_INIT_ERROR (-1)
```
SIM 初始化失败的返回码。注意后面有一个 `#if 0` 块，里面有三个相同定义，这是开发时注释掉的冗余代码，不影响编译。

```c
typedef enum {
    sim_op_stat_get_iccid,  // 0：等待获取 ICCID
    sim_op_stat_get_imsi,   // 1：等待获取 IMSI
    sim_op_stat_get_deinit, // 2：等待 deinit
    sim_op_stat_suc         // 3：操作成功
} sim_op_stat_enu;
```
SIM 操作状态枚举，描述异步操作的当前阶段。

```c
typedef struct {
    sim_op_stat_enu sim_op_st;   // 当前操作状态
    uint8_t rsv[3];              // 对齐填充（保证 4 字节对齐）
    char sim_iccid[SIM_BUF_SIZE]; // ICCID 字符串缓冲区
    char sim_imsi[SIM_BUF_SIZE];  // IMSI 字符串缓冲区
} sim_mng_t;
```
SIM 管理结构体。`rsv[3]` 是手工添加的对齐填充：`sim_op_stat_enu`（4字节）+ `uint8_t[3]`（3字节）= 7字节，加上最后1字节补全为 8 字节（64位对齐），之后是两个 32 字节缓冲区。

### sim.c

```c
int sim_init(void)
{
    int ret = 0;
    ret = ql_sim_init();              // 调用 Quectel SDK SIM 初始化
    if(ret == QL_ERR_OK) {
        printf("ql_sim_init ok\n");
        return QL_ERR_OK;
    } else {
        LOG_E("ql_sim_init failed, ret = %d\n", ret); // 使用 LOG_E 错误级别
        return -1;
    }
}
```
SIM 初始化：调用 SDK `ql_sim_init()`，成功返回 `QL_ERR_OK`（通常为 0），失败返回 -1 并记录错误。`LOG_E` 是 Android 风格的错误日志宏，比 `printf` 多一个 TAG 前缀。

```c
int sim_get_imsi(char* imsi)
{
    int ret = 0;
    int input = 1;           // 硬编码：SIM 槽 1
    QL_SIM_SLOT_E slot;
    QL_SIM_APP_TYPE_E app_type;

    if (1 == input) {
        slot = QL_SIM_SLOT_1;   // 选择 SIM 槽 1
    } else if (2 == input) {
        slot = QL_SIM_SLOT_2;   // 选择 SIM 槽 2（此分支永不到达）
    } else {
        printf("bad slot: %d\n", input);
        return -1;
    }
```
注意：`input` 被硬编码为 1，`else if (2 == input)` 和 `else` 分支**永远不会执行**。这是未完成的参数化设计——原来打算通过参数选择 SIM 槽，后来固定为 SIM 1。

```c
    input = 0;        // 重用 input 变量，现在用于选择应用类型
    switch (input) {
    case 0: app_type = QL_SIM_APP_TYPE_UNKNOWN; break; // 未知类型（默认）
    case 1: app_type = QL_SIM_APP_TYPE_3GPP;    break; // 3GPP（GSM/LTE SIM）
    case 2: app_type = QL_SIM_APP_TYPE_3GPP2;   break; // 3GPP2（CDMA SIM）
    case 3: app_type = QL_SIM_APP_TYPE_ISIM;    break; // IMS SIM
    default:
        printf("bad app type: %d\n", input);
        return -1;
    }
```
同样，`input` 被硬编码为 0，始终选择 `QL_SIM_APP_TYPE_UNKNOWN`。SDK 通常会自动检测 SIM 应用类型，UNKNOWN 表示不指定，让 SDK 自行判断。

```c
    ret = ql_sim_get_imsi(slot, app_type, imsi, QL_SIM_IMSI_LENGTH+1);
```
调用 SDK 获取 IMSI：传入槽号、应用类型、目标缓冲区、缓冲区大小（`QL_SIM_IMSI_LENGTH+1` 额外 +1 用于 null 终止符）。

```c
int sim_get_iccid(char* iccid)
{
    int ret = 1;         // 注意：初始化为 1（非零），不是 0
    int input = 1;       // 硬编码 SIM 槽 1
    QL_SIM_SLOT_E slot;

    if (1 == input) {
        slot = QL_SIM_SLOT_1;
    } ...

    ret = ql_sim_get_iccid(slot, iccid, QL_SIM_ICCID_LENGTH+1);
    if (ret == QL_ERR_OK) {
        printf("ICCID: %s\n", iccid);
        return QL_ERR_OK;
    } else {
        LOG_E("sim_get_iccid failed, ret = %d\n", ret);
        return -1;
    }
}
```
获取 ICCID。注意 `int ret = 1`（初始化为 1 而非 0）——这是一个潜在 bug：如果 SDK 调用前发生异常退出，`ret` 不为 0 可能导致误判。但实际上 SDK 调用前没有 early return，所以影响有限。

---

## 6. ec200a/apn/apn.h / apn.c — APN 管理模块

### apn.h

```c
typedef struct {
    char iccid[32];    // SIM ICCID 前缀（用于匹配）
    char apn[32];      // APN 名称（如 "cmnet", "3gnet"）
    char usr_name[32]; // 认证用户名（部分运营商需要）
    char pwd[32];      // 认证密码
} apn_obj_t;
```
APN 配置对象。每条记录对应一个运营商配置。ICCID 使用前缀匹配（`strncmp` 比较前 N 位），因为同一运营商不同批次的 SIM 卡 ICCID 前缀相同。

```c
#define APN_JSON_NAME     "apn.json"
#define APN_NAME_PUBLIC   "apnpublic"
```
- `APN_JSON_NAME`：APN 配置文件名（路径为 `/usr/dial/apn.json`）
- `APN_NAME_PUBLIC`：当 ICCID 匹配失败时的默认 APN 名（空用户名/密码）

### apn.c

```c
apn_obj_t *apn_load_from_json(char *json_path, int *p_apn_count)
{
    FILE *fp = NULL;
    char *str = NULL;
    int i, json_len, len;
    apn_obj_t *p_apn_obj;
    *p_apn_count = 0;

    struct json_object *obj, *json_root, *json_apn, *json_temp;
    fp = fopen(json_path, "r");
    if (fp == NULL) {
        return NULL;   // 文件不存在则返回 NULL
    }
    fseek(fp, 0, SEEK_END);   // 移动到文件末尾
    len = ftell(fp);           // 获取文件大小
    rewind(fp);                // 回到文件头
    str = (char *)malloc(len); // 分配内存
```
使用 `fseek+ftell+rewind` 三步法获取文件大小，然后一次性读入内存。这适合小型 JSON 文件（APN 配置通常只有几 KB）。注意 `malloc(len)` 没有 +1 为 null 终止符预留空间，如果 `json_tokener_parse` 需要 null 终止则可能有问题（实际上 json-c 内部会处理）。

```c
    if (fread(str, 1, len, fp) < 0) {
        free(str);
        fclose(fp);
        return NULL;
    }
```
`fread` 返回实际读取字节数，理论上不应该是负数（返回类型是 `size_t`，无符号）。此处与 `< 0` 比较实际上永远不会触发——这是一个逻辑错误。正确写法应为 `!= len`。

```c
    obj = json_tokener_parse(str);
    if (obj != NULL) {
        if (json_object_object_get_ex(obj, "apn", &json_root)) {
            json_len = json_object_array_length(json_root);
```
解析 JSON 文件，查找 `"apn"` 键对应的数组。预期的 JSON 格式：
```json
{"apn": [{"supplier": "CMCC", "apn": "cmnet", "iccid": "898600", "usrname": "", "pwd": ""}]}
```

```c
            for (i = 0; i < json_len; i++) {
                json_apn = json_object_array_get_idx(json_root, i);
                if (json_object_object_get_ex(json_apn, "supplier", &json_temp)) { }
                json_object_put(json_temp);  // 释放引用
                
                if (json_object_object_get_ex(json_apn, "apn", &json_temp)) {
                    p_json_str = json_object_get_string(json_temp);
                    memcpy(p_apn_obj[i].apn, p_json_str, strlen(p_json_str));
                }
                json_object_put(json_temp);
```
`json_object_put` 释放引用计数，防止内存泄漏。注意 `memcpy` 没有检查 `strlen(p_json_str)` 是否超过 `apn[32]` 的大小，存在潜在缓冲区溢出风险。

```c
    // json_object_put(obj); //this segfault
```
注释说明 `json_object_put(obj)` 会导致段错误，因此被注释掉。这导致了内存泄漏（`obj` 永远不被释放）。可能是因为已经在循环中对子对象调用了 `json_object_put`，导致引用计数问题。

```c
int set_apn(char *iccid)
{
    int apn_obj_count, i, ret;
    ql_data_call_apn_config_t apn_cfg;
    char json_path[256] = {0};

    sprintf(json_path, "%s/%s", "/usr/dial", APN_JSON_NAME);
    // 展开后 json_path = "/usr/dial/apn.json"
    
    apn_obj_t *p_apn_obj = apn_load_from_json(json_path, &apn_obj_count);
```
`set_apn` 是 APN 设置的核心函数，从 `/usr/dial/apn.json` 加载 APN 配置，根据 ICCID 匹配相应条目。

```c
    for (i = 0; i < apn_obj_count; i++) {
        if (0 == strncmp(p_apn_obj[i].iccid, iccid, strlen(p_apn_obj[i].iccid)))
```
使用 **ICCID 前缀匹配**：`strncmp` 只比较 `p_apn_obj[i].iccid`（配置文件中的前缀）的长度，这样 "8986" 可以匹配所有以 "8986" 开头的 SIM 卡（中国移动的 ICCID 前缀）。

```c
        strncpy(apn_cfg.apn_name, p_apn_obj[i].apn, sizeof(apn_cfg.apn_name));
        strncpy(apn_cfg.username, p_apn_obj[i].usr_name, sizeof(apn_cfg.username));
        strncpy(apn_cfg.password, p_apn_obj[i].pwd, sizeof(apn_cfg.password));
        apn_cfg.ip_ver = QL_NET_IP_VER_V4;    // 强制 IPv4
```
安全的字符串复制（限制长度），避免缓冲区溢出。

```c
    if (i == apn_obj_count) {
        printf("apn not found, use default apn\n");
    } else {
        ret = ql_data_call_set_apn_config(DATA_CALL_APN_PUBLIC, &apn_cfg);
        // DATA_CALL_APN_PUBLIC = 6，SDK APN 槽位 6 用于公网
    }

    if ((i == apn_obj_count) || (0 == apn_obj_count)) {
        // ICCID 未匹配或文件为空：使用默认 APN
        memset(&apn_cfg, 0, sizeof(apn_cfg));
        strncpy(apn_cfg.apn_name, APN_NAME_PUBLIC, sizeof(apn_cfg.apn_name));
        // APN 名 = "apnpublic"，无用户名密码
        apn_cfg.ip_ver = QL_NET_IP_VER_V4;
        ret = ql_data_call_set_apn_config(DATA_CALL_APN_PUBLIC, &apn_cfg);
    }
    return ret;
}
```
双重设置逻辑：即使找到了匹配的 APN 并设置，如果条件满足仍然会用默认 APN 覆盖（这可能是 bug，`if` 条件里 `i == apn_obj_count` 与上面的 `else` 块互斥，但 `0 == apn_obj_count` 分支在两处都可能触发，导致默认 APN 总会被设置）。

---

## 7. ec200a/nw/nw.h — 网络模块头文件

```c
#define NW_IF_STATISTICS_RX_PACKETS_PATH "/sys/devices/virtual/net/%s/statistics/rx_packets"
```
Linux sysfs 网卡接收包数统计文件路径模板，`%s` 替换为具体网卡名。格式化后如 `/sys/devices/virtual/net/ccinet0/statistics/rx_packets`，读取此文件获取无需 ioctl 的轻量级流量统计。

```c
#define NW_CSQ_PATH    "/tmp/network_csq"
#define NW_STATUS_PATH "/tmp/network_status"
```
两个临时文件路径：
- `network_csq`：当前信号强度，其他进程可读取（如 Web UI）
- `network_status`：网络连接状态（1=已连接，0=断开）

这种通过 `/tmp` 文件共享状态的方式是嵌入式 Linux 上进程间通信的简单替代方案（相比 D-Bus 或 nanomsg 更轻量）。

```c
bool nw_get_if_statistics_rx_packets(uint64_t *p_rx_packets, char *if_name);
void nw_get_signal_strength(void);
void nw_get_data_reg_status(void);
void nw_get_status(int call_id);
int get_signal_strength(QL_NW_SIGNAL_STRENGTH_LEVEL_E *level);
void nw_mark_network_status(int net_status);
```
函数声明：
- `nw_get_if_statistics_rx_packets`：通过 sysfs 读取网卡 RX 包数（用于流量监控）
- `get_signal_strength`：通过 SDK `ql_nw_get_signal_strength` 获取信号等级
- `nw_mark_network_status`：将网络状态写入 `/tmp/network_status`

---

## 8. ec200a/data_call/data_call.h / data_call.c — Data Call 状态模块

### data_call.c

```c
int  g_callid = DATA_CALL_ID_PUBLIC;    // 全局 call ID，初始为 1
char g_if_name[32] = {0};              // 全局网卡名缓冲区（由回调函数填写）
```
两个关键全局变量：`g_callid` 在拨号失败时自增（最大不超过 10），避免与残留的旧 call 产生冲突；`g_if_name` 由 `data_call_status_ind_cb` 在连接成功时从 `p_msg->device` 填入。

```c
const char *dial_status[] = {
    "dial status none", "dial created", "dial idle", "dial connecting",
    "dial v4 connected", "dial v6 connected", "dial connected",
    "dial disconnected", "dial error", "dial deleted"
};
```
Data Call 状态字符串数组，下标对应 `QL_NET_DATA_CALL_STATUS_E` 枚举值（0-9）。用于日志打印，帮助调试状态转换。

```c
const char* get_dial_status_msg(int errcode) {
    switch (errcode) {
    case 0: ... case 9:
        return dial_status[errcode];
    default:
        return "Unknown status";
    }
}
```
根据状态码返回可读字符串。使用 switch 而非直接数组索引，是为了范围检查（`default` 分支处理越界）。

```c
extern volatile int g_last_call_status;
```
声明外部全局变量（定义在 `dialer_ec200a.cpp` 中）。`volatile` 关键字告知编译器此变量可能被异步回调修改，不要对其进行寄存器缓存优化。回调函数（在 SDK 线程中运行）和主拨号循环（在 dialThread 中运行）共享此变量，无锁访问（数据类型为 `int`，在 ARM 上原子）。

```c
void data_call_status_ind_cb(int call_id,
                             QL_NET_DATA_CALL_STATUS_E pre_call_status,
                             ql_data_call_status_t *p_msg)
```
SDK 异步回调函数：当 Data Call 状态发生变化时，SDK 从其内部线程调用此函数。参数：
- `call_id`：触发回调的 Data Call ID
- `pre_call_status`：状态变化前的状态
- `p_msg`：当前状态详细信息（包含 IP、网关、DNS 等）

```c
    g_last_call_status = p_msg->call_status;
```
更新全局状态变量，供 `dial_loop` 的 `ST_STATUS` 阶段读取。这是唯一的"无锁"更新点，依赖 int 写入的原子性（ARM 32/64 位均保证）。

```c
    if (p_msg->call_status == QL_NET_DATA_CALL_STATUS_CONNECTED)
    {
        system("echo 0 > /tmp/dial_Status");
```
写入 `/tmp/dial_Status`，0 表示已连接（注意：命名与含义相反，0=成功状态）。其他进程（如 Web UI 或监控脚本）可轮询此文件。`system()` 比 `fopen+fprintf+fclose` 慢但代码简洁。

```c
        system("echo 1 > /tmp/network_status");
```
写入 `/tmp/network_status`，1 表示网络已连接。与 `dial_Status` 不同，此文件由 `nw_mark_network_status(1)` 用 sysfs 方式写入，同时也由 `system()` 写入，存在双写。

```c
        strncpy(g_if_name, p_msg->device, sizeof(g_if_name));
```
将网卡名（如 "ccinet0"）安全复制到全局缓冲区。`strncpy` 确保不超过 `sizeof(g_if_name)=32` 字节，但注意若 `p_msg->device` 恰好 32 字节无 null 终止，需额外处理（此处未处理）。

```c
        if (p_msg->has_addr)  // SDK 通知包含 IPv4 地址
        {
            nw_mark_network_status(1);  // 通过 sysfs 写入 /tmp/network_status
            printf("IPV4 addr    : %s\n", p_msg->addr.addr);
            printf("IPV4 gateway : %s\n", p_msg->addr.gateway);
            printf("IPV4 netmask : %s\n", p_msg->addr.netmask);
            printf("IPV4 dnsp    : %s\n", p_msg->addr.dnsp);  // 主 DNS
            printf("IPV4 dnss    : %s\n", p_msg->addr.dnss);  // 备 DNS
```
打印 IP 配置信息，方便串口调试。

```c
            snprintf(cmd_buf, sizeof(cmd_buf),
                     "ip ro add default via %s dev %s",
                     p_msg->addr.gateway, p_msg->device);
            system(cmd_buf);
```
添加默认路由：`ip route add default via <gateway> dev <device>`。这是 SDK 回调中设置系统路由的关键步骤，使所有流量经过 4G 接口。

```c
            system("iptables -t filter -F");
```
清空 iptables filter 表的所有规则（`-F` = flush）。这确保之前残留的防火墙规则不会干扰新连接。注意此操作会影响 WiFi 等其他接口的规则，是较粗暴的做法。

```c
            snprintf(cmd_buf, sizeof(cmd_buf),
                     "iptables -t nat -A POSTROUTING -o %s -j MASQUERADE",
                     p_msg->device);
            system(cmd_buf);
```
添加 NAT MASQUERADE 规则：将从 4G 接口出去的数据包做源地址伪装（SNAT）。这是设备作为路由器/热点时共享 4G 网络的关键配置。`-A POSTROUTING` 在路由决策后、数据包离开接口前应用。

```c
            fp = fopen("/tmp/resolv_v4.conf", "w");
            if (!fp) {
                printf("Failed to write resolv file, err=%s\n", strerror(errno));
                return;
            }
            if (p_msg->addr.dnsp[0]) {
                fprintf(fp, "nameserver %s\r\n", p_msg->addr.dnsp);
            }
            if (p_msg->addr.dnss[0]) {
                fprintf(fp, "nameserver %s\r\n", p_msg->addr.dnss);
            }
            fclose(fp);
```
写入 DNS 到临时文件。检查 `p_msg->addr.dnsp[0]` 是否为非零（即字符串非空）。注意使用 `\r\n`（CRLF）而非 `\n`，这在 Linux 上是多余的，但兼容 Windows 格式。

```c
            system("echo \"\" > /etc/resolv.conf");
            if (access("/tmp/resolv_v4.conf", F_OK) == 0) {
                system("cat /tmp/resolv_v4.conf >> /etc/resolv.conf");
            }
            if (access("/tmp/resolv_v6.conf", F_OK) == 0) {
                system("cat /tmp/resolv_v6.conf >> /etc/resolv.conf");
            }
```
DNS 配置三步骤：
1. 清空 `/etc/resolv.conf`（用 `echo ""` 而非 `> /etc/resolv.conf`，避免权限问题）
2. 若有 IPv4 DNS 文件则追加
3. 若有 IPv6 DNS 文件则追加

`access(path, F_OK)` 检查文件是否存在（不检查读写权限），避免 cat 失败。

```c
    else if (p_msg->call_status == QL_NET_DATA_CALL_STATUS_DISCONNECTED)
    {
        system("echo 1 > /tmp/dial_Status");  // 1 = 已断开
        system("echo 0 > /tmp/network_status"); // 0 = 无网络
    }
```
断开时更新两个状态文件。注意不清理路由规则——重连时 SDK 会发送新的 CONNECTED 回调并重新添加路由。

```c
bool can_ping_google() {
    int status = system("ping -c 1 8.8.8.8 > /dev/null");
    return (status == 0);
}
```
旧版 Ping 检测（使用 `system()` 的返回值）。这个函数存在 SIGCHLD 信号干扰问题（信号处理函数可能改变子进程状态导致 `system()` 返回错误的退出码）。在 `dialer_ec200a.cpp` 中有改进版 `test_can_ping_google()` 使用 `popen+ttl=` 检测。

```c
dial_stat_enu flow_monitor_task(struct timespec* dial_timer)
{
    uint64_t if_rx_packets = 0;
    static uint64_t u64_if_rx_packets = 0;  // 静态变量，记录上次包数
    struct timespec cur_timer;
    struct timespec dif_timer;

    clock_gettime(CLOCK_MONOTONIC, &cur_timer);  // 获取当前单调时钟
```
流量监控任务（在旧版状态机中使用）：
- `u64_if_rx_packets`：静态变量，保存上次检查时的 RX 包数
- `CLOCK_MONOTONIC`：不受 NTP/系统时间调整影响的时钟

```c
    if (nw_get_if_statistics_rx_packets(&if_rx_packets, g_if_name))
    {
        if ((if_rx_packets != u64_if_rx_packets) && (if_rx_packets > u64_if_rx_packets))
        {
            *dial_timer = cur_timer;  // 重置超时计时器
        }
        u64_if_rx_packets = if_rx_packets;
    }
```
双重条件：`if_rx_packets != u64_if_rx_packets`（有新包）**且** `if_rx_packets > u64_if_rx_packets`（计数增加而非回绕）。当有新数据包到来时，重置 `dial_timer`，防止误触发重拨。

```c
    dif_timer = diff(*dial_timer, cur_timer);
    if (dif_timer.tv_sec > DIAL_TIMEOUT_SECONDS)  // 120 秒无流量
    {
        printf("timeout: %ld second, no data rx\n", dif_timer.tv_sec);
    #if !USE_PING_GOOGLE
        return dial_stat_stop;    // 不 Ping 时直接触发重拨
    #endif
    #if USE_PING_GOOGLE
        if (!can_ping_google()) {
            return dial_stat_stop; // Ping 失败才重拨
        }
    #endif
    }
    usleep(4900 * 1000);  // 睡 4.9 秒（避免 busy loop）
    return dial_stat_net_connected;
}
```
超时判断：120 秒内无新流量则根据编译宏决定行为——直接重拨（更激进）或 Ping 测试后再决定（更保守）。4.9 秒的 sleep 是为了节省 CPU，但会导致检测存在最大 4.9 秒的延迟。

```c
char *executeATCommand(const char *cmd)
{
    char buffer[128];
    char *result = malloc(1);  // 从 1 字节开始
    *result = '\0';
    size_t length = 0;

    FILE *pipe = popen(cmd, "r");
    while (fgets(buffer, sizeof(buffer), pipe) != NULL)
    {
        size_t new_length = length + strlen(buffer);
        result = realloc(result, new_length + 1);
        strcat(result, buffer);
        length = new_length;
    }
    pclose(pipe);
    return result;  // 调用者负责 free()
}
```
动态增长的命令执行函数：从 1 字节开始，逐行读取，用 `realloc` 扩展缓冲区。比固定 1024 字节缓冲区更安全但有性能开销（频繁 realloc）。**调用者必须调用 `free()` 释放返回值**，否则内存泄漏。

```c
int checkSimCardStatus()
{
    char *response;
    // 先查网络注册状态
    response = executeATCommand("serial_atcmd at+creg?");
    if (response)
    {
        if (strstr(response, "+CREG: 0,1") || strstr(response, "+CREG: 0,5"))
        {
            free(response);   // 成功：注册到本地(1)或漫游(5)
        }
        else
        {
            free(response);
            return 2;  // 未注册
        }
    }
    else return 2;

    // 再查 SIM 卡状态
    response = executeATCommand("serial_atcmd at+cpin?");
    if (response)
    {
        if (strstr(response, "READY")) {
            free(response);
            return 0;  // SIM 正常
        } else {
            free(response);
            return 1;  // SIM 异常
        }
    }
    else return 1;
}
```
返回值语义：0=一切正常，1=SIM 异常，2=网络未注册。顺序很重要：先查网络注册（`AT+CREG?`），再查 SIM（`AT+CPIN?`）——因为即使 SIM 正常，网络也可能未注册。

```c
void restartNetworkServices()
{
    int idle = isIdleState();
    if (idle == 0) return;  // 有 WiFi 客户端连接时不重启，避免断开用户

    system("killall -9 ql_rild");
    system("killall -9 ql_netd");

    pid_t pid1 = fork();
    if (pid1 == 0) {
        execl("/usr/bin/ql_rild", "ql_rild", NULL);
        _exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        execl("/usr/bin/ql_netd", "ql_netd", NULL);
        _exit(1);
    }
}
```
重启网络服务：`ql_rild`（无线接口层守护）和 `ql_netd`（网络守护）是 Quectel 的关键后台进程。使用 `fork+execl` 而非 `system("ql_rild &")` 是为了更好地控制进程生命周期。`_exit(1)` 在 exec 失败时不调用 atexit 处理函数（避免子进程析构父进程的全局对象）。

```c
int check_process(const char *process_name)
{
    char command[256];
    snprintf(command, sizeof(command), "pgrep %s | wc -l", process_name);
    FILE *fp = popen(command, "r");
    int process_count = 0;
    fscanf(fp, "%d", &process_count);
    pclose(fp);
    return process_count;
}
```
通过 `pgrep <name> | wc -l` 统计运行中的同名进程数量。返回 0 表示未运行，≥1 表示至少一个进程在运行。`pgrep` 按进程名查找，比 `ps aux | grep` 更精确（不会匹配 grep 本身）。

```c
int read_status() {
    FILE *file = fopen(NW_STATUS_PATH, "r");
    if (!file) return -1;
    int status;
    if (fscanf(file, "%d", &status) != 1) { fclose(file); return -1; }
    fclose(file);
    return status;
}

int write_status(int new_status) {
    FILE *file = fopen(NW_STATUS_PATH, "w");
    if (!file) return -1;
    fprintf(file, "%d\n", new_status);
    fclose(file);
    return 0;
}

int update_network_status(int new_status) {
    int current_status = read_status();
    if (current_status == new_status) return 0;  // 状态未变，避免无效写入
    return write_status(new_status);
}
```
对 `/tmp/network_status` 的读写封装：
- `read_status()`：读取当前状态（-1 表示错误）
- `write_status()`：写入新状态
- `update_network_status()`：仅在状态变化时写入，减少 I/O（`/tmp` 在 tmpfs 上，虽然不涉及闪存，但频繁写入仍有内核开销）

---

## 9. eg25/at/at.h / at.c — EG25 AT 命令模块

### at.h

```c
#define QUEC_AT_PORT "/dev/smd8"
```
EG25G 的 AT 命令端口：`/dev/smd8` 是 Qualcomm SMD（Shared Memory Driver）虚拟串口，用于主处理器与 modem 处理器间通信。EG25G 基于高通芯片，使用 SMD 而非普通 UART 与 modem 通信。这与 EC200A 使用 `serial_atcmd` 工具的方式完全不同。

```c
#define AT_MSG_LENGTH_MAX (1024)
```
AT 响应缓冲区最大长度。1024 字节对于大多数 AT 命令的响应已足够（`AT+CSQ` 约 20 字节，`AT+COPS=?` 最长约 500 字节）。

```c
#ifdef __cplusplus
extern "C" {
#endif
extern int Ql_SendAT(int smd_fd, char *atCmd, char *finalRsp, long timeout_ms, char *p_rsp_msg);
extern int at_init(void);
#ifdef __cplusplus
}
#endif
```
C++ 兼容声明。`Ql_SendAT` 有 5 个参数（与 EC200A 的单参数 `Ql_SendAT(const char*)` 完全不同），这是两个平台 AT 接口的主要区别。

### at.c

```c
int Ql_SendAT(int smd_fd, char *atCmd, char *finalRsp, long timeout_ms, char *p_rsp_msg)
{
    int iLen;
    fd_set fds;
    int rdLen;
    char strAT[AT_MSG_LENGTH_MAX] = {0};
    char strFinalRsp[AT_MSG_LENGTH_MAX];
    char strResponse[AT_MSG_LENGTH_MAX];
    struct timeval timeout = {0, 0};
    boolean bRcvFinalRsp = FALSE;
```
局部变量初始化：`fd_set` 用于 `select()` 监听，`timeout` 为 timeval 结构（秒+微秒），`bRcvFinalRsp` 标记是否收到最终响应。使用 `boolean` 类型（Quectel 自定义，通常 `typedef int boolean`）。

```c
    memset(strAT, 0x0, sizeof(strAT));
    iLen = sizeof(atCmd);  // 错误：这里取的是指针大小（4或8字节），不是字符串长度
    strncpy(strAT, atCmd, iLen);
```
**严重 Bug**：`sizeof(atCmd)` 取的是 `char*` 指针的大小（32位系统4字节，64位系统8字节），不是 AT 命令字符串的长度。正确写法应为 `strlen(atCmd)`。不过这个问题被后面的 `sprintf` 覆盖：

```c
    sprintf(strFinalRsp, "\r\n%s", finalRsp);
```
在期望的最终响应前添加 `\r\n`，构建完整的期望结束标记（如 `\r\nOK`）。AT 命令响应通常以 `\r\nOK\r\n` 结束。

```c
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = timeout_ms % 1000;
```
将毫秒超时时间转换为 `struct timeval`：秒数取整，微秒取余。注意 `tv_usec` 单位是微秒，但这里存入的是毫秒余数，**单位错误**（应该 `* 1000`）。例如 `timeout_ms=2000`：`tv_sec=2`，`tv_usec=0`（凑巧正确）；`timeout_ms=1500`：`tv_sec=1`，`tv_usec=500`（应为 500000 微秒，实际设置 500 微秒）。这个 bug 导致超时时间比预期短得多。

```c
    iLen = strlen(atCmd);
    if ((atCmd[iLen - 1] != '\r') && (atCmd[iLen - 1] != '\n'))
    {
        iLen = sprintf(strAT, "%s\r\n", atCmd);  // 添加 AT 命令结束符
        strAT[iLen] = 0;
    }
```
如果命令不以 `\r` 或 `\n` 结尾，追加 `\r\n`。AT 协议要求命令以 `\r\n` 结尾，但实际上通常只需要 `\r`（回车）。`sprintf` 返回写入的字节数，再次 null 终止（`strAT[iLen] = 0`）是多余的（sprintf 已保证）。

```c
    read(smd_fd, strResponse, AT_MSG_LENGTH_MAX);  // 先清空接收缓冲区
    write(smd_fd, strAT, iLen);                    // 发送 AT 命令
```
发送前先 `read` 清空管道中可能残留的 URC（主动上报）数据，避免干扰后续响应读取。`write` 直接写入 SMD 文件描述符。

```c
    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(smd_fd, &fds);

        switch (select(smd_fd + 1, &fds, NULL, NULL, &timeout))
        {
        case -1:
            return -1;   // select 出错

        case 0:
            return 1;    // 超时（注意：1 不是标准 POSIX 错误码，是自定义）

        default:
            if (FD_ISSET(smd_fd, &fds))
            {
                do {
                    memset(strResponse, 0x0, AT_MSG_LENGTH_MAX);
                    rdLen = read(smd_fd, strResponse, AT_MSG_LENGTH_MAX);
                    if (rdLen > 0) {
                        printf(">> Read response, len=%d: %s\r\n", rdLen, strResponse);
                    }
                    if ((rdLen > 0) && strstr(strResponse, strFinalRsp))
                    {
                        if (strstr(strResponse, strFinalRsp)
                            || strstr(strResponse, "+CME ERROR:")
                            || strstr(strResponse, "+CMS ERROR:")
                            || strstr(strResponse, "ERROR"))
                        {
                            if (p_rsp_msg != NULL) {
                                memcpy(p_rsp_msg, strResponse, rdLen);
                            }
                            bRcvFinalRsp = TRUE;
                        }
                    }
                    usleep(10000);  // 10ms 延迟，等待更多数据
                } while ((rdLen > 0) && (AT_MSG_LENGTH_MAX == rdLen));
            }
        }
        if (bRcvFinalRsp) break;
    }
    return 0;
}
```
响应读取逻辑：
- `select` 等待数据可读（受 timeout 限制）
- 内层 `do-while`：持续读取直到没有数据（rdLen <= 0）或读取不满缓冲区（rdLen < AT_MSG_LENGTH_MAX）
- `strstr(strResponse, strFinalRsp)` 检测 `\r\nOK` 是否出现
- 同时检测 `+CME ERROR:`、`+CMS ERROR:`、`ERROR` 等错误响应
- 10ms sleep 给 modem 时间发送多段响应

**设计缺陷**：每次 `do-while` 迭代都覆写 `strResponse`，多段响应只保留最后一段。此外 `strstr` 的第一个条件（`strstr(strResponse, strFinalRsp)`）与外层相同，属于冗余。

```c
int at_init(void)
{
    static int smd_fd = -1;  // 静态变量：整个进程只初始化一次
    if (smd_fd >= 0) {
        return smd_fd;   // 已初始化，直接返回
    }

    smd_fd = open(QUEC_AT_PORT, O_RDWR | O_NONBLOCK | O_NOCTTY);
```
打开 AT 串口的标志：
- `O_RDWR`：可读可写
- `O_NONBLOCK`：非阻塞模式（配合 select 使用）
- `O_NOCTTY`：不将此终端作为控制终端（避免 Ctrl+C 等信号传递到进程）

```c
    Ql_SendAT(smd_fd, "ATE0", "OK", 1000, NULL);  // 关闭回显
    Ql_SendAT(smd_fd, "AT",   "OK", 1000, NULL);  // 基本 AT 测试
    Ql_SendAT(smd_fd, "ATI",  "OK", 1000, NULL);  // 获取版本信息
    return smd_fd;
}
```
初始化序列：
- `ATE0`：关闭 AT 命令回显（Echo Off），使响应解析更简单
- `AT`：确认模块就绪
- `ATI`：打印模块信息（Identification），帮助调试

---

## 10. eg25/nw/nw.h / nw.c — EG25 网络模块

### nw.h（EG25 版本）

```c
#define SIM_CARD_READY (10)
```
SIM 卡就绪状态码（值为 10）：`QL_MCM_SIM_GetCardStatus` 返回的 `app_state - 0xB00`，其中 0xB0A - 0xB00 = 10 表示 READY 状态。这个偏移运算将 Qualcomm 的 MCM 错误码转换为简单的枚举值。

```c
#define NW_STATUS_PATH    "/tmp/network_status"
#define NW_CSQ_PATH       "/tmp/network_csq"
#define NW_PLMN_PATH      "/tmp/network_plmn"
```
三个状态文件路径（EG25G 版本多了 `NW_PLMN_PATH`，EC200A 版本的 nw.h 没有这个宏）。PLMN 信息（MCC+MNC）写入此文件供其他进程读取。

### nw.c（EG25 版本）

```c
bool nw_get_sim_card_status(nw_client_handle_type nw_client)
{
    QL_MCM_SIM_CARD_STATUS_INFO_T t_info = {0};
    int sim_card_app_status;
    if (0 == QL_MCM_SIM_GetCardStatus(nw_client, E_QL_MCM_SIM_SLOT_ID_1, &t_info))
    {
        sim_card_app_status = t_info.card_app_info.app_3gpp.app_state - 0xB00;
```
通过 MCM 接口获取 SIM 状态。`- 0xB00` 是将 Qualcomm MCM 应用状态码（范围 0xB00-0xB0F 等）转换为相对偏移。`app_3gpp` 指定 3GPP（GSM/LTE）应用，忽略 3GPP2（CDMA）应用。

```c
        if (sim_card_app_status == SIM_CARD_READY)  // == 10
        {
            return true;
        }
        else {
            QLOGI(NW_LOG_TAG, "QL_MCM_SIM_GetCardStatus failed: 0x%04x", sim_card_app_status);
        }
    }
    return false;
}
```
只有状态码恰好为 10（READY）时才返回 true。其他状态（初始化中、PIN 锁定等）返回 false。

```c
int nw_at_get_csq(int smd_fd)
{
    char rsp_msg[256];
    char *p_csq;
    int csq, ber;
    static int pre_csq = -1;  // 静态变量：记录上次信号值（避免重复写文件）
    
    pthread_mutex_lock(&g_at_port_mutex);       // AT 端口互斥锁
    Ql_SendAT(smd_fd, "AT+CSQ", "OK", 1000, rsp_msg);
    pthread_mutex_unlock(&g_at_port_mutex);
```
使用互斥锁保护 AT 端口访问。EG25G 的 `dialer_eg25.c` 主循环和可能的后台线程都可能调用 AT 命令，需要串行化访问。`g_at_port_mutex` 定义在 `dialer_eg25.c` 中（`pthread_mutex_t g_at_port_mutex = PTHREAD_MUTEX_INITIALIZER`）。

```c
    p_csq = strstr(rsp_msg, "+CSQ: ");
    if (p_csq != NULL)
    {
        if (2 == sscanf(p_csq, "+CSQ: %d,%d", &csq, &ber))
        {
            if (pre_csq != csq)  // 仅在值变化时写文件
            {
                // 写入 /tmp/network_csq
                sprintf(echo_cmd, "echo %d > %s", csq, NW_CSQ_PATH);
                system(echo_cmd);
                pre_csq = csq;
            }
            return csq;
        }
    }
    return -1;
}
```
`sscanf` 同时解析 CSQ 和 BER（误码率），验证解析了 2 个值。`pre_csq` 静态变量避免每次都触发文件写操作，减少系统调用。

```c
void nw_set_default_profile(int profile_idx)
{
    ql_data_call_default_profile_s profile;
    memset(&profile, 0, sizeof(profile));
    if (0 == QL_Data_Call_Get_Default_Profile(&profile))
    {
        if (profile.profile_idx != profile_idx)
        {
            profile.profile_idx = profile_idx;
            if (QL_Data_Call_Set_Default_Profile(&profile))
            {
                QLOGI(NW_LOG_TAG, "set default profile failure");
            }
        }
    }
}
```
设置默认拨号 Profile（APN 配置槽）。只在当前值与目标值不同时才执行设置，避免重复 API 调用。注意错误处理不完整：`QL_Data_Call_Set_Default_Profile` 返回非零时只打印日志，不返回错误码。

```c
bool nw_get_if_statistics_rx_packets(uint64_t *p_rx_packets, char *if_name)
{
    char if_statistics_rx_packets_path[NAME_MAX] = {0};
    int if_statistics_rx_packets_fd, file_size;
    char val_bytes[256] = {0};

    sprintf(if_statistics_rx_packets_path, NW_IF_STATISTICS_RX_PACKETS_PATH, if_name);
    // 展开为: /sys/devices/virtual/net/<if_name>/statistics/rx_packets
    
    if_statistics_rx_packets_fd = open(if_statistics_rx_packets_path, O_RDONLY);
    if (... == -1) return false;

    file_size = lseek(fd, 0L, SEEK_END);  // 获取文件大小
    lseek(fd, 0L, SEEK_SET);              // 回到文件头
    read(fd, val_bytes, file_size);
    close(fd);
    *p_rx_packets = strtoull(val_bytes, NULL, 0);  // 字符串转 uint64_t
    return true;
}
```
通过 sysfs 读取网卡 RX 包数。使用 `strtoull` 转换（支持大数值，网卡包数可能超过 2^32）。`lseek+read` 组合比 `fgets` 更底层但更高效。

```c
bool nw_at_get_cereg_stat(int smd_fd)
{
    ...
    Ql_SendAT(smd_fd, "AT+CEREG?    ", "OK", 1000, rsp_msg);
```
注意 `"AT+CEREG?    "` 末尾有多个空格——这是原始 AT 命令中的无意格式问题，AT 解析器通常会忽略末尾空格，不影响功能。

```c
    if (2 == sscanf(p_cereg, "+CEREG: %d,%d", &cereg_n, &stat_cereg))
    {
        if ((stat_cereg == 1) || (stat_cereg == 5))
        {
            return true;  // 1=已注册本地, 5=已注册漫游
        }
    }
    return false;
}
```
只接受状态 1（注册到本地网络）或 5（漫游注册）为成功状态，其他状态（0=未注册，2=搜索，3=被拒，4=未知）视为失败。

---

## 11. imx_led.hpp — i.MX6ULL GPIO LED 控制

```cpp
class LedControl {
public:
    LedControl(int gpio) : gpio_pin(gpio), blinking(false) {
        gpio_export(gpio_pin);                // 导出 GPIO 到用户空间
        gpio_config(gpio_pin, "direction", "out"); // 设置为输出方向
    }
```
构造函数初始化列表设置 GPIO 引脚号和闪烁标志。立即导出并配置 GPIO：
- `gpio_export` 创建 `/sys/class/gpio/gpio<N>/` sysfs 目录
- `gpio_config(direction, out)` 写入 `"out"` 到方向文件

```cpp
    void startBlinking(int interval) {
        if (!blinking) {
            blinking = true;
            blinkThread = std::thread(&LedControl::blink, this, interval);
        }
    }
```
创建后台线程执行 LED 闪烁。使用成员函数指针 `&LedControl::blink`，传入 `this` 和间隔毫秒数。`if (!blinking)` 防止重复创建线程。

```cpp
    void stopBlinking() {
        if (blinking) {
            blinking = false;     // 设置退出标志
            if (blinkThread.joinable()) {
                blinkThread.join(); // 等待线程退出
            }
        }
    }
```
线程退出机制：设置 `blinking = false` 后，`blink()` 方法的 `while(blinking)` 循环会在下一次检查时退出。`volatile bool blinking` 保证跨线程可见性。

```cpp
private:
    volatile bool blinking;  // volatile 确保多线程可见性

    void blink(int interval) {
        while (blinking) {
            turnOn();
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            turnOff();
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    }
```
闪烁循环：开灯 → 等待 → 关灯 → 等待，循环执行。使用 `std::chrono::milliseconds` 确保跨平台时间精度（比 `usleep` 更安全）。

```cpp
    int gpio_export(int gpio) {
        char path[64];
        struct stat st;

        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", gpio);
        if (stat(path, &st) == 0) {
            return 0;  // GPIO 已存在，无需再导出
        }

        int fd = open("/sys/class/gpio/export", O_WRONLY);
        if (fd < 0) { perror("Open export failed"); return -1; }

        snprintf(path, sizeof(path), "%d", gpio);
        if (write(fd, path, strlen(path)) != (ssize_t)strlen(path)) {
            perror("Export GPIO failed");
            close(fd); return -1;
        }
        close(fd);
        return 0;
    }
```
Linux GPIO sysfs 导出操作：将 GPIO 编号（如 "33"）写入 `/sys/class/gpio/export`。先检查是否已导出（避免 `write: Device or resource busy` 错误）。

```cpp
    int gpio_config(int gpio, const char *attr, const char *val) {
        char file_path[100];
        int fd;

        snprintf(file_path, sizeof(file_path),
                 "/sys/class/gpio/gpio%d/%s", gpio, attr);
        fd = open(file_path, O_WRONLY);
        if (fd < 0) {
            printf("Failed to open: %s\n", file_path);
            return -1;
        }
        if (write(fd, val, strlen(val)) != (ssize_t)strlen(val)) {
            perror("Write GPIO failed");
            close(fd); return -1;
        }
        close(fd);
        return 0;
    }
```
通用 GPIO 属性配置：写入 `/sys/class/gpio/gpio<N>/<attr>`。`attr` 可以是 "direction"、"value" 等。每次都 open+write+close（不缓存文件描述符），确保写入立即生效（sysfs 文件不支持缓存）。

---

## 12. reboot_conf/dial_reboot_conf.h / .c — 重启守护配置

### dial_reboot_conf.h

```c
#define CONFIG_FILE  "/usrdata/dial_config.txt"
```
持久化配置文件路径：`/usrdata` 是 EC200A OpenWrt 系统中的非易失性存储分区（类似 Android 的 `/data` 分区），程序重启后数据保留。

```c
#define REBOOT_FILE  "/tmp/system_is_rebooting"
```
重启标志文件：写入此文件触发外部脚本执行系统重启（具体机制由 OpenWrt 的 `system_is_rebooting` 监控脚本实现）。

```c
#define THRESHOLD_TIME 1200  // 阈值：1200秒（20分钟）
```
断网超过 20 分钟触发系统重启（比 `dial_loop` 的 30 分钟 Level 3 恢复阈值更长，属于最后防线）。

```c
typedef struct {
    int uptime;           // 首次断网时的系统 uptime
    int restart_flag;     // 重启标志：0=未重启，1=已重启
    int first_disconnect; // 是否首次断网
    int signal_strength;  // 信号强度（AT+CSQ 值）
    int sim_status;       // SIM 状态：0=正常，1=损坏
    int ql_netd_status;   // ql_netd 运行状态：0=运行，1=停止
} Config;
```
配置结构体：所有字段持久化到 `dial_config.txt`。`restart_flag` 是防止重复重启的关键字段。

### dial_reboot_conf.c

```c
void read_config() {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (file == NULL) {
        // 文件不存在：初始化配置
        g_config.uptime = get_uptime();
        g_config.restart_flag = 0;
        g_config.first_disconnect = 0;
        write_config();  // 立即持久化
        return;
    }
    fscanf(file, "%d,%d", &g_config.uptime, &g_config.restart_flag);
    g_config.first_disconnect = 0;
    fclose(file);
}
```
读取配置：文件不存在时自动创建并初始化。`fscanf` 只读取 2 个字段（uptime 和 restart_flag），其余字段（signal_strength 等）在运行时动态获取，不从文件读取。

```c
int get_uptime() {
    FILE *fp = fopen("/proc/uptime", "r");
    if (!fp) { perror("无法读取 uptime"); exit(1); }
    float uptime;
    fscanf(fp, "%f", &uptime);  // 读取浮点数（如 "1234.56 345.78"）
    fclose(fp);
    return (int)uptime;  // 取整数部分
}
```
读取 `/proc/uptime` 的第一个数（总 uptime 秒数）。`float` 型变量精度约 7 位有效数字，对于秒级精度足够（系统运行几天内不会有精度问题）。`exit(1)` 直接退出——在嵌入式系统中，无法读取 uptime 被认为是致命错误。

```c
int get_signal_csq() {
    FILE *fp = popen("serial_atcmd at+csq", "r");
    char buffer[256];
    int signal_strength = -1;
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, "+CSQ:")) {
            sscanf(buffer, "+CSQ: %d,", &signal_strength);
            break;
        }
    }
    fclose(fp);
    return signal_strength;
}
```
通过 `serial_atcmd` 获取信号强度。`sscanf(buffer, "+CSQ: %d,", &signal_strength)` 只读取逗号前的数字（信号值），忽略误码率（BER）。

```c
int check_pre_conditions() {
    g_config.signal_strength = get_signal_csq();
    if (g_config.signal_strength > 20 &&
        g_config.sim_status >= 0 &&
        g_config.ql_netd_status == 0) {
        return 1;  // 前置条件满足
    }
    return 0;
}
```
三个前置条件：
1. 信号强度 > 20（CSQ 范围 0-31，>20 表示信号良好）
2. SIM 状态 >= 0（0=正常，-1=获取失败）
3. ql_netd 运行（0=运行）

只有所有条件满足，才认为是真正的网络故障（排除硬件问题）。

```c
void perform_reboot() {
    FILE *reboot_file = fopen(REBOOT_FILE, "w");
    if (reboot_file == NULL) { perror("无法打开重启文件"); return; }
    fprintf(reboot_file, "1\n");
    fclose(reboot_file);
    printf("写入重启标志到 %s, 等待10秒...\n", REBOOT_FILE);
    sleep(10);      // 给系统保存数据的时间
    printf("系统重启中...\n");
    system("reboot");  // 执行重启
}
```
重启两步骤：先写文件（让其他进程知道系统即将重启），等待 10 秒（数据刷新到闪存），再执行 `reboot`。10 秒的等待时间是工程经验值——足够文件系统同步，但不会等待太久。

---

## 13. fault_report/NetworkMonitor.h / .cpp — 网络故障监控

### NetworkMonitor.h

```cpp
class NetworkChecker {
public:
    virtual bool checkNetwork() = 0;  // 纯虚函数：定义检测接口
    virtual ~NetworkChecker() {}       // 虚析构：支持多态删除
};
```
策略模式（Strategy Pattern）：`NetworkChecker` 是抽象接口，具体实现（`PingNetworkChecker`）通过多态注入，便于测试时替换（依赖注入）。

```cpp
class PingNetworkChecker : public NetworkChecker {
public:
    bool checkNetwork() override;
private:
    std::string executeCommand(const std::string &command);
};
```
Ping 实现：通过执行 shell 命令检测网络，是最直接的互联网连通性测试方式。

```cpp
class NetworkMonitor {
public:
    NetworkMonitor(NetworkChecker* checker);
    void startMonitoring();   // 启动后台监控线程
    void stopMonitoring();    // 停止线程（join）
private:
    NetworkChecker* networkChecker;
    bool networkDown;                   // 当前是否断网
    time_t networkDownStartTime;        // 断网开始时间（用于超时判断）
    std::mutex mtx;
    std::thread monitoringThread;
    bool stopFlag;

    void monitorLoop();
    void monitorNetwork(bool isNetworkUp);
};
```
监控器包含独立线程、断网状态和开始时间。与 EC200ADialer 的 `dial_loop` 不同，这是一个独立的监控类，设计上可以在 dial_loop 之外独立运行。

### NetworkMonitor.cpp

```cpp
bool PingNetworkChecker::checkNetwork() {
    std::string commandOutput = executeCommand("ping -c 1 8.8.8.8");
    return commandOutput.find("1 packets transmitted, 1 received") != std::string::npos;
}
```
通过字符串匹配判断 Ping 结果：搜索 Linux ping 输出中的成功标志。这比 EC200A 改进版的 `ttl=` 检测稍弱（翻译系统可能使用不同格式），但简洁明了。

```cpp
void NetworkMonitor::monitorLoop() {
    while (!stopFlag) {
        bool isNetworkUp = networkChecker->checkNetwork();
        monitorNetwork(isNetworkUp);
        std::this_thread::sleep_for(std::chrono::seconds(5));  // 每 5s 检测一次
    }
}
```
每 5 秒检测一次，停止时 `stopFlag` 退出循环。5 秒是检测频率与系统负载的折中（每次 Ping 最多等待几秒）。

```cpp
void NetworkMonitor::monitorNetwork(bool isNetworkUp) {
    std::lock_guard<std::mutex> lock(mtx);
    time_t now = time(nullptr);

    if (!isNetworkUp) {
        if (!networkDown) {
            networkDown = true;
            networkDownStartTime = now;   // 记录断网起始时间
        } else if (difftime(now, networkDownStartTime) >= 600) {  // 10分钟
            NetWork_EventPublisher::getInstance().pub(
                "NETWORK", "Unable to establish 4G connection", 210);
        }
    } else {
        if (networkDown) {
            // 从断网恢复
            NetWork_EventPublisher::getInstance().pub(
                "NETWORK", "4G Network connection recovered", 211);
        }
        networkDown = false;  // 恢复网络后重置状态
    }
}
```
事件码语义：210=断网超 10 分钟（持续告警），211=网络恢复。`difftime` 计算时间差（秒），避免 time_t 直接相减的跨平台问题（虽然在 POSIX 上等价）。

---

## 14. fault_report/SimSignalMonitor.h / .cpp — 信号强度监控

### SimSignalMonitor.h

```cpp
class SimSignalMonitor {
public:
    SimSignalMonitor(SignalChecker* checker) 
        : signalChecker(checker), signalLow(false), signalHigh(false) {}

    void startMonitoring() {
        monitoringThread = std::thread(&SimSignalMonitor::monitorLoop, this);
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> lowSignalStartTime;
    std::chrono::time_point<std::chrono::steady_clock> highSignalStartTime;
```
使用 `std::chrono::steady_clock`（单调时钟）记录信号变化时间，比 `time_t` 更精确（纳秒级别）且不受系统时间调整影响。

```cpp
    void monitorSignal(int signalValue) {
        std::lock_guard<std::mutex> lock(mtx);
        auto now = std::chrono::steady_clock::now();

        if (signalValue < 20) {
            if (!signalLow) {
                signalLow = true;
                lowSignalStartTime = now;
            } else if (std::chrono::duration_cast<std::chrono::minutes>(
                           now - lowSignalStartTime).count() >= 5) {
                NetWork_EventPublisher::getInstance().pub(
                    "SIGNAL", "Signal strength low", 208);
            }
        } else {
            signalLow = false;
        }
```
信号低于 20（约 -73 dBm，接近边缘覆盖）持续 5 分钟 → 发布低信号事件（208）。事件只在持续超标时发布，避免瞬间抖动触发告警。`std::chrono::duration_cast<std::chrono::minutes>` 将时间差转换为分钟数。

```cpp
        if (signalValue > 25) {
            if (!signalHigh) {
                signalHigh = true;
                highSignalStartTime = now;
            } else if (duration >= 5 minutes) {
                publish "Signal strength recovered" (209)
            }
        } else {
            signalHigh = false;
        }
```
信号高于 25 持续 5 分钟 → 发布恢复事件（209）。双阈值设计（<20 为低，>25 为高）形成迟滞（Hysteresis），避免在边界值附近频繁触发事件。

### SimSignalMonitor.cpp

```cpp
#ifdef USE_EC200A_DIAL
int EC200ASignalChecker::checkSignal() {
    std::string commandOutput = executeCommand("serial_atcmd at+csq");
    return extractSignalValue(commandOutput);
}

int EC200ASignalChecker::extractSignalValue(const std::string& input) {
    std::istringstream stream(input);
    std::string line;
    int signalValue = -1;
    while(std::getline(stream, line)) {
        size_t csqPos = line.find("+CSQ: ");
        if(csqPos != std::string::npos) {
            std::string signalPart = line.substr(csqPos + 6);
            std::istringstream signalStream(signalPart);
            if(!(signalStream >> signalValue)) return -1;
            break;
        }
    }
    return signalValue;
}
```
与 `EC200ADialer::extractSignalValue` 基本相同，但使用 C++ `istringstream` 替代 C 风格的字符串操作。两种实现共存是代码未完全统一的体现。

```cpp
#elif defined(USE_IMX6U_DIAL)
int IMX6ULLSignalChecker::checkSignal() {
    int csq = 0;
    Imx6uDialer::get_csq("/dev/ttyUSB5");  // 通过 ttyUSB5 发 AT+CSQ
    if((Imx6uDialer::csq < 32 && Imx6uDialer::csq > 0) || (Imx6uDialer::csq == 99))
    {
        csq = Imx6uDialer::csq;
    }
    return csq;
}
```
i.MX6ULL 通过 `ttyUSB5`（不是 ttyUSB0，分开的 AT 命令端口）获取信号。静态成员 `Imx6uDialer::csq` 作为共享变量（不够线程安全，但实际使用中只有一个线程读取）。

---

## 15. fault_report/SimMonitor.h / .cpp — SIM 卡状态监控

### SimMonitor.h

```cpp
#define QL_SIM_ICCID_LENGTH 20
```
在 `SimMonitor.h` 中重复定义 ICCID 长度（EC200A SDK 头文件中也有定义），独立定义避免对 SDK 头文件的依赖。

```cpp
class SimMonitor {
public:
    SimMonitor(SimChecker* checker);
    void checkAndReportSimStatus();  // 单次检查，非持续监控
private:
    SimChecker* simChecker;
    pthread_mutex_t mtx;  // POSIX mutex（而非 std::mutex）
};
```
与 `NetworkMonitor` 的持续监控不同，`SimMonitor` 只做**一次性**检查（通常在程序启动时），验证 SIM 卡是否存在并能读取 ICCID。使用 `pthread_mutex_t` 而非 `std::mutex`，说明此类更偏向 C 风格实现。

### SimMonitor.cpp

```cpp
#ifdef USE_EC200A_DIAL
bool EC200ASimChecker::checkSim() {
    char iccidBuffer[QL_SIM_ICCID_LENGTH + 1] = {0};
    if (sim_init() != QL_ERR_OK) {
        std::cerr << "SIM is not ok" << std::endl;
        sleep(2);
        return false;
    }
    sim_get_iccid(iccidBuffer);
    return strlen(iccidBuffer) > 0;  // ICCID 非空则认为 SIM 正常
}
```
通过调用 SDK `sim_init()` 和 `sim_get_iccid()` 验证 SIM 可用性。`sleep(2)` 在失败后等待，给 SIM 初始化更多时间（某些 SIM 卡启动较慢）。

```cpp
void SimMonitor::checkAndReportSimStatus() {
    bool isSimDetected = simChecker->checkSim();
    pthread_mutex_lock(&mtx);
    if (!isSimDetected) {
        NetWork_EventPublisher::getInstance().pub(
            "SIM", "Unable to read SIM card number", 206);
    } else {
        NetWork_EventPublisher::getInstance().pub(
            "SIM", "SIM card number detected: ", 207);
    }
    pthread_mutex_unlock(&mtx);
}
```
事件码：206=无法读取 SIM（需告警），207=SIM 正常（信息性通知）。注意 207 事件的描述 "SIM card number detected: " 末尾没有具体号码（ICCID 没有拼接进去），这是未完成的功能（原计划附上 ICCID，但未实现）。

---

## 16. nanomsg_process_cinterface.h — C 语言 IPC 接口声明

```c
struct NanoReqHandlerWrapper {
    void *handler;    // 指向 NanoReqHandler C++ 对象的 void 指针
    pthread_t thread; // 关联的 POSIX 线程
};
```
C++ 对象的 C 包装结构：`void *handler` 用于在 C 代码中持有 C++ 对象指针，绕过 C 不支持类的限制。这是 C/C++ 混合编程的经典模式（Opaque Pointer / Handle 模式）。

```c
struct TrafficMonitorWrapper {
    void *instance;   // TrafficMonitor C++ 对象
    pthread_t thread; // 关联的监控线程
};
```
流量监控器包装：注意同一个包装结构体中同时存储了对象指针和线程句柄，但实际上流量监控的线程是由 `TrafficMonitor::check_CCINet_FlowOverLimit()` 内部 detach 创建的，`thread` 字段在 EG25G 的 `nanomsg_process_wraper.cpp` 中用 `pthread_create` 创建并引用。

```c
int* get_eg25_csq(NanoReqHandlerWrapper *wrapper);
```
特殊函数：返回 `NanoReqHandler` 中 `ModemReqHandler::csq` 字段的**指针**。EG25G 主循环使用此指针直接更新 csq 值（`*csq_ptr = nw_at_get_csq(...)`），绕过对象方法调用。这是一种不安全的设计（绕过封装），但在单线程访问的场景下可行。

---

## 17. nanomsg_process_wraper.cpp — C++ 到 C 的包装层

```cpp
#include <memory>
#include <functional>
#include <condition_variable>
#include "nanomsg_process.hpp"
#include "parse_config.hpp"

extern "C"
{
#include "nanomsg_process_cinterface.h"
```
整个文件的 `extern "C"` 块：所有在此块内定义的函数使用 C 链接（无名字改编），可以被 C 代码（`dialer_eg25.c`）调用。

```cpp
    void *processModemRequestsWrapper(void *arg)
    {
        NanoReqHandler *handler = static_cast<NanoReqHandler *>(arg);
        handler->processModemRequests();
        return NULL;
    }
```
符合 `pthread_create` 签名的包装函数：将 `void*` 强转为 C++ 对象指针，然后调用成员函数。`static_cast` 是类型安全的向下转换（相比 C 风格强转）。

```cpp
    NanoReqHandlerWrapper *startNanoReqHandler(const char *address)
    {
        NanoReqHandlerWrapper *wrapper = new NanoReqHandlerWrapper;
        wrapper->handler = static_cast<void *>(new NanoReqHandler(address));
        int rc = pthread_create(&wrapper->thread, NULL,
                                processModemRequestsWrapper, wrapper->handler);
        if (rc) { printf("ERROR: pthread_create() %d\n", rc); exit(-1); }
        return wrapper;
    }
```
EG25G 版本使用 `pthread_create` 而非 `std::thread`，因为 EG25G 主程序（`dialer_eg25.c`）是 C 代码。注意这里直接 `exit(-1)` 处理线程创建失败，不传递错误到调用方。

```cpp
    int NetWork_EventPublisher_init(NetWork_EventPublisherWrapper *wrapper, const char *url)
    {
        return static_cast<NetWork_EventPublisher *>(wrapper->instance)->init(url);
    }
```
将 C++ 单例对象的方法调用包装成 C 函数。`static_cast<NetWork_EventPublisher *>` 将 `void*` 还原为正确的 C++ 类型（比 C 风格强转安全，在类型不匹配时编译报错）。

```cpp
    void *threadFunc_overlimit(void *wrapper)
    {
        static_cast<TrafficMonitor *>(
            ((TrafficMonitorWrapper *)wrapper)->instance
        )->check_CCINet_FlowOverLimit();
        return NULL;
    }

    void check_4g_overlimit(TrafficMonitorWrapper *wrapper)
    {
        int rc = pthread_create(&wrapper->thread, NULL,
                                threadFunc_overlimit, wrapper);
    }
```
两层指针解引用：先将 `wrapper` 转为 `TrafficMonitorWrapper*`，再访问 `.instance`（void*），再强转为 `TrafficMonitor*`，最后调用成员函数。层次清晰但需要正确的类型管理。

```cpp
    const char* get_interface(void *wrapper)
    {
        return (static_cast<TrafficMonitor *>(
            ((TrafficMonitorWrapper *)wrapper)->instance
        )->get_inferface()).c_str();
    }
```
**潜在 Bug**：返回 `std::string::c_str()` 的临时指针。`get_inferface()` 返回 `std::string` 临时对象，调用 `c_str()` 后临时对象销毁，返回的指针成为悬空指针。在 EG25G 主循环中：
```c
if ((strlen(get_interface(monitor)) != 0) && ...)
```
此处立即使用 `strlen` 消费指针，在实际环境中可能不会崩溃（临时对象销毁前的窗口极短），但这仍然是未定义行为。

```cpp
} // end extern "C"

int* get_eg25_csq(NanoReqHandlerWrapper *wrapper)
{
    return &(static_cast<NanoReqHandler*>(wrapper->handler)
             ->modemReqHandlerPtr->csq);
}
```
此函数在 `extern "C"` 块**之外**（但仍在文件中），通过 `extern "C"` 在 `nanomsg_process_cinterface.h` 中声明。返回 `ModemReqHandler::csq` 成员的地址，允许 C 代码直接修改 C++ 对象的成员变量。

---

## 18. parse_config.hpp — JSON 配置解析器

```cpp
class Variant_Config
{
public:
    Variant_Config() : intValue(0), isString(false) {}   // 默认：整型 0
    Variant_Config(int value) : intValue(value), isString(false) {}
    Variant_Config(std::string value) : strValue(value), isString(true) {}

    bool is_string() const { return isString; }
    int get_int() const { return intValue; }
    std::string get_string() const { return strValue; }

private:
    int intValue;
    std::string strValue;
    bool isString;
};
```
简化版 `std::variant`（C++17 之前的替代方案）：同时存储 int 和 string，用 `isString` 标记实际类型。不支持 double/bool 等其他类型（对于此配置文件格式已足够）。

```cpp
class ConfigParser
{
public:
    ConfigParser(const std::string &filename)
    {
        std::ifstream file(filename);
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        parse(content);
    }
```
构造函数一次性读入文件：`istreambuf_iterator` 直接将文件字符流转换为字符串，比逐行读取更高效，也避免换行符处理问题。

```cpp
    Variant_Config getValue(const std::string &key) const
    {
        auto it = data.find(key);
        if (it != data.end())
            return it->second;
        else
            throw std::invalid_argument("Key not found");
    }
```
抛出异常而非返回特殊值：调用方必须用 try-catch 处理，避免忘记检查返回值的问题（如 main 中的 `getIntValue` 包装函数）。

```cpp
private:
    void parse(const std::string &content)
    {
        cJSON *root = cJSON_Parse(content.c_str());
        if (!root) {
            std::cout << "Error before: " << cJSON_GetErrorPtr() << std::endl;
            return;
        }
```
cJSON 解析失败时调用 `cJSON_GetErrorPtr()` 返回错误位置指针（指向原始字符串中出错的字符），便于定位 JSON 语法错误。解析失败后直接 return（data map 为空），调用 `getValue` 时会抛出异常。

```cpp
        cJSON *config = cJSON_GetObjectItem(root, "config");
        if (config)
        {
            cJSON *config_data = cJSON_GetObjectItem(config, "data");
            if (config_data)
            {
                cJSON *item = config_data->child;
                while (item)
                {
                    if (cJSON_IsNumber(item))
                        data[item->string] = Variant_Config((int)item->valuedouble);
                    else if (cJSON_IsString(item))
                        data[item->string] = Variant_Config(std::string(item->valuestring));
                    item = item->next;  // 遍历链表（cJSON 内部用链表组织 JSON 对象字段）
                }
            }
        }
        cJSON_Delete(root);  // 释放 cJSON 内存
    }
    std::map<std::string, Variant_Config> data;
};
```
cJSON 对象的遍历方式：通过 `child` 和 `next` 指针遍历 JSON 对象的所有字段（cJSON 内部使用单向链表）。`item->string` 是键名，`item->valuedouble` 或 `item->valuestring` 是值。注意 cJSON 将所有数字都存为 double，用 `(int)` 强制转换截断小数部分。

---

## 19. traffic_sql/sql_traffic.hpp — 流量 SQLite 数据库

```cpp
struct TrafficData {
    double totalValue;       // 总流量（字节）
    double sentValue;        // 上行流量（字节）
    double receivedValue;    // 下行流量（字节）
    double trafficThreshold; // 流量阈值（字节）
    std::string recordTime;  // 记录时间（"YYYY-MM-DD HH:MM:SS"）
};
```
流量数据结构：使用 `double` 而非 `uint64_t`，因为 SQLite 的 REAL 类型与 double 对应。Double 可以精确表示 2^53 内的整数（约 8PB），远超实际流量数量级。

```cpp
class TrafficDatabase {
public:
    sqlite3 *dbRef;  // 公开的数据库句柄（允许外部直接执行 SQL）
private:
    static TrafficDatabase *instance;  // 单例指针
    std::string currentDBName;
    char *errMsg = nullptr;
```
单例模式，`dbRef` 公开（非 private）允许 `TrafficMonitor` 直接获取 DB 句柄并调用 `getLastRecordFromDB`。这破坏了封装性，但减少了代理函数数量。

```cpp
    TrafficDatabase()
    {
        system("mkdir -p /media/sdcard/traffic/");
        openTrafficDatabase();
    }
```
构造函数创建目录并打开数据库。`system("mkdir -p ...")` 比 `mkdir` 系统调用更简洁（自动创建父目录），但有 shell 注入风险（此处路径是硬编码的，安全）。

```cpp
    void initTables()
    {
        const char *monthlyTrafficTable =
            "CREATE TABLE IF NOT EXISTS MonthlyTraffic ("
            "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
            "TotalValue REAL, SentValue REAL, ReceivedValue REAL,"
            "TrafficThreshold REAL, RecordTime TEXT NOT NULL);";
        executeSql(monthlyTrafficTable);
        executeSql(historyTrafficTable);  // HistoryTraffic 结构相同
        fillTableWithDefaultData("MonthlyTraffic");
        fillTableWithDefaultData("HistoryTraffic");
    }
```
两张表结构完全相同，分别记录当月流量和历史累计流量。`AUTOINCREMENT` 确保 ID 单调递增（不复用已删除记录的 ID），这对循环覆写策略很重要。

```cpp
    void fillTableWithDefaultData(const std::string &tableName)
    {
        std::string checkTable = "SELECT count(*) FROM " + tableName;
        int count = getCount(checkTable.c_str());
        if (count == 0)
        {
            std::string sql = "INSERT INTO " + tableName +
                " (TotalValue, SentValue, ReceivedValue, TrafficThreshold, RecordTime) "
                " VALUES (0, 0, 0, 0, '1970-01-01 00:00:00')";
            for (int i = 0; i < 1 * 60; ++i)  // 预插入 60 条记录
            {
                executeSql(sql.c_str());
            }
        }
    }
```
预填充 60 条默认记录（时间戳为 1970-01-01 epoch）：
- 60 条 = 60 分钟 ≈ 1 小时的滚动窗口（10s 间隔 × 6 = 1分钟一次写入，共 60 条）
- 全为零值表示历史数据未知
- 使用 `1 * 60` 而非直接 `60`，表达了"1小时 × 60次/小时"的设计意图

```cpp
    std::string getLatestRecordMonth(const std::string &table)
    {
        std::string sql = "SELECT strftime('%Y-%m', RecordTime) AS Month "
                         "FROM " + table + " ORDER BY RecordTime DESC LIMIT 1;";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(dbRef, sql.c_str(), -1, &stmt, 0) == SQLITE_OK)
        {
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                month = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            }
            sqlite3_finalize(stmt);  // 必须调用，释放 prepared statement
        }
        return month;
    }
```
使用 `sqlite3_prepare_v2 + sqlite3_step + sqlite3_finalize` 的标准 SQLite 查询流程（相比 `sqlite3_exec` 更安全，避免 SQL 注入）。`strftime('%Y-%m', ...)` 只保留年月部分，用于跨月检测。`reinterpret_cast<const char *>` 将 SQLite 的 `const unsigned char *` 转为 C 字符串。

```cpp
    void updateOldestRecord(const std::string &table,
                            double total, double tx, double rx,
                            double threshold,
                            const std::string &currentTime)
    {
        // 查找最旧记录的 ID
        std::string findOldestSql = "SELECT ID FROM " + table +
                                   " ORDER BY RecordTime ASC LIMIT 1;";
        // 更新该记录
        std::string updateSql = "UPDATE " + table +
            " SET TotalValue=" + std::to_string(total) +
            " , SentValue=" + std::to_string(tx) + ...
            " WHERE ID=" + std::to_string(oldestId);
        executeSql(updateSql.c_str());
    }
```
循环覆写策略：始终更新最旧的记录（`ORDER BY RecordTime ASC LIMIT 1`）。这样 60 条记录形成一个循环缓冲区：新数据覆盖最老数据。优点是数据库大小固定（不随时间增长），缺点是历史数据只保留约 10 分钟（60条 × 10s/条）。

```cpp
    bool isLatestRecordMonthSameAsSystem()
    {
        std::string dbMonth = getLatestRecordMonth("MonthlyTraffic");
        std::string systemMonth = getSystemYearAndMonth();
        return dbMonth == systemMonth;
    }
```
跨月检测：比较数据库最新记录的月份（"YYYY-MM"格式）与系统当前月份。返回 false 表示已跨月，触发流量计数器重置。

```cpp
    TrafficData getLastRecordFromDB(sqlite3 *db, const std::string &table)
    {
        TrafficData data = {0, 0, 0, 0, ""};
        std::string sql = "SELECT TotalValue, SentValue, ReceivedValue, "
                         "TrafficThreshold, RecordTime FROM " + table +
                         " ORDER BY RecordTime DESC LIMIT 1;";
        // 执行查询，填充 data 结构体
        return data;
    }
```
获取最新一条记录（`ORDER BY RecordTime DESC LIMIT 1`）。用于程序启动时恢复上次的流量统计基准值，防止重启后流量清零。

---

## 20. logger_sd.h / logger_sd.c — SD 卡日志系统

### logger_sd.h

```c
void log_init(void);
void log_close(void);
void dial_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
```
`__attribute__((format(printf, 1, 2)))` GCC 属性：
- 第 1 个参数是格式字符串位置
- 第 2 个参数是可变参数起始位置
- GCC 会像检查 printf 一样检查参数类型，如 `dial_log("%d", "string")` 会产生编译警告

### logger_sd.c

```c
#define SDCARD_PATH       "/media/sdcard"
#define LOG_DIR           "/media/sdcard/dial_log"
#define MIN_FREE_SPACE_MB 500
```
三个配置宏：SD 卡挂载点、日志目录、最小可用空间。注释掉的是开发时用的 `/usrdata` 路径（只需 5MB），生产版本使用 SD 卡（需 500MB，防止日志消耗 SD 卡大部分空间）。

```c
static FILE *g_log_fp = NULL;
```
静态全局文件句柄：`static` 限制在本编译单元可见，外部代码不能直接操作。`NULL` 表示日志系统未初始化或不可用（仅控制台输出）。

```c
int is_sdcard_mounted(void)
{
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) { perror("fopen /proc/mounts"); return 0; }

    char dev[128], mnt[128], fstype[64], rest[256];
    int mounted = 0;
    while (fscanf(fp, "%127s %127s %63s %255[^\n]\n",
                  dev, mnt, fstype, rest) >= 3) {
        if (strcmp(mnt, "/media/sdcard") == 0) {
            mounted = 1;
            break;
        }
    }
    fclose(fp);
    return mounted;
}
```
解析 `/proc/mounts` 检测 SD 卡：
- `%127s %127s %63s %255[^\n]` 格式：逐列读取，最后一列用 `[^\n]` 匹配到行尾
- `>= 3` 的条件：只要能解析至少设备名、挂载点、文件系统类型即可（某些行格式可能不完整）
- `strcmp(mnt, "/media/sdcard")` 精确匹配挂载点（避免匹配到 `/media/sdcard/something`）

```c
static int check_sdcard_available() {
    if (!is_sdcard_mounted()) {
        system("echo 000 > /tmp/sdcard_avl");  // 写入不可用标志
        printf("[LogInit] SD card not mounted.\n");
        return 0;
    }
    struct statvfs stat;
    if (statvfs(SDCARD_PATH, &stat) != 0) {
        printf("[LogInit] Failed to stat filesystem: %s\n", strerror(errno));
        return 0;
    }
    unsigned long long free_bytes = (unsigned long long)stat.f_bavail * stat.f_frsize;
    unsigned long long free_mb = free_bytes / (1024 * 1024);
    printf("[LogInit] Path: %s, Free: %llu MB, Required: %d MB\n",
           SDCARD_PATH, free_mb, MIN_FREE_SPACE_MB);
    if (free_mb < MIN_FREE_SPACE_MB) {
        printf("[LogInit] Space insufficient.\n");
        return 0;
    }
    return 1;
}
```
`statvfs` 文件系统统计：
- `stat.f_bavail`：非超级用户可用的块数
- `stat.f_frsize`：基础文件系统块大小（字节）
- 两者乘积 = 实际可用字节数
- 使用 `unsigned long long` 避免大文件系统下的整数溢出（`f_bavail * f_frsize` 可能超过 4GB）

```c
void log_init(void) {
    if (g_log_fp != NULL) return;  // 防止重复初始化（幂等性）

    if (!check_sdcard_available()) {
        printf("[LogInit] SD card check failed. Logging to console only.\n");
        return;
    }

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", LOG_DIR);
    int ret = system(cmd);
```
使用 `system("mkdir -p ...")` 而非直接调用 `mkdir` 系统调用：`-p` 参数自动创建中间目录，且目录已存在时不报错。`int ret` 赋值但未使用，是遗留代码（本应检查返回值）。

```c
    time_t now;
    time(&now);
    struct tm *t = localtime(&now);
    char filename[256];
    snprintf(filename, sizeof(filename),
             "%s/dial_%04d%02d%02d_%02d%02d%02d.log",
             LOG_DIR,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
```
生成带时间戳的文件名（如 `dial_20250315_143022.log`）：
- `tm_year + 1900`：tm 年份从 1900 开始，需要加 1900
- `tm_mon + 1`：tm 月份从 0 开始（0=1月），需要加 1
- `%04d%02d%02d`：固定宽度格式，确保日期排序与字典序一致（便于按名称排序日志文件）

```c
    g_log_fp = fopen(filename, "w");
    if (g_log_fp) {
        printf("[LogInit] Logging started: %s\n", filename);
        fprintf(g_log_fp, "=== Dial Program Started [%04d-%02d-%02d %02d:%02d:%02d] ===\n", ...);
        fflush(g_log_fp);  // 立即刷新头部，确保即使立即断电也有文件头
    } else {
        printf("[LogInit] Failed to create log file: %s (%s)\n", filename, strerror(errno));
    }
}
```
写入文件头并立即 `fflush`：即使程序在极短时间内崩溃，文件头也会被写入 SD 卡（不在 stdio 缓冲区中丢失）。

```c
void log_close(void) {
    if (g_log_fp) {
        time_t now; time(&now);
        struct tm *t = localtime(&now);
        fprintf(g_log_fp, "\n=== Program Exit [%04d-%02d-%02d %02d:%02d:%02d] ===\n", ...);
        fclose(g_log_fp);
        g_log_fp = NULL;  // 防止 use-after-close
    }
}
```
写入退出时间戳后关闭文件：`g_log_fp = NULL` 防止后续误用关闭的文件句柄（如果 `log_close` 被调用两次，第二次会因为 `NULL` 检查而跳过）。

```c
void dial_log(const char *fmt, ...) {
    va_list args;
    char time_str[32];
    time_t now; time(&now);
    struct tm *t = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    // 输出到 stdout
    printf("[%s] ", time_str);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    // 输出到文件（必须重新 va_start）
    if (g_log_fp) {
        fprintf(g_log_fp, "[%s] ", time_str);
        va_start(args, fmt);     // 关键：va_list 只能遍历一次！
        vfprintf(g_log_fp, fmt, args);
        va_end(args);
        fflush(g_log_fp);        // 每条日志立即刷新
    }
}
```
`va_list` 必须重新 `va_start`：第一次 `vprintf` 调用后，`args` 已到达参数列表末尾，必须重新初始化才能再次遍历（否则第二次 `vfprintf` 读取到未定义的内存）。`strftime` 生成 ISO 8601 格式的时间戳（`%Y-%m-%d %H:%M:%S`），每条日志前缀 `[2025-03-15 14:30:22] `，便于日志分析工具解析。每条日志后调用 `fflush` 确保不在 stdio 缓冲区积压——这会增加 I/O 开销，但对于关键诊断日志（如拨号状态变化），实时性比性能更重要。

---

## 附录：运行时关键状态文件速查

| 文件 | 写入方 | 读取方 | 含义 |
|------|--------|--------|------|
| `/tmp/dial_Status` | data_call_status_ind_cb | 外部监控 | 0=已连接，1=断开 |
| `/tmp/network_status` | data_call_status_ind_cb / nw.c | 外部监控 | 1=联网，0=断网 |
| `/tmp/network_csq` | nw_at_get_csq / nw.c | 外部查询 | 当前信号强度（0-31） |
| `/tmp/network_plmn` | nw_mark_plmn / eg25/nw.c | 外部查询 | PLMN 码（MCC+MNC） |
| `/tmp/cfun_count.txt` | restart_cfun_safe | restart_cfun_safe | CFUN 调用计数（上限10） |
| `/tmp/cfun_last_call.txt` | restart_cfun_safe | restart_cfun_safe | 上次 CFUN 调用 uptime |
| `/tmp/dial_retry_count` | check_and_update_retry_count | check_and_update_retry_count | 快速失败重试次数（上限3） |
| `/tmp/callid` | writeCallIdToFile | 外部调试 | 当前 Data Call ID |
| `/tmp/dial_attempt_time.txt` | writeCurrentTimeToFile | checkDialAttemptTime | 拨号尝试时间（120s 阈值） |
| `/tmp/system_is_rebooting` | perform_reboot | OpenWrt 监控脚本 | 系统重启标志 |
| `/tmp/sdcard_avl` | check_sdcard_available | 外部查询 | SD 卡可用性（000=不可用） |
| `/tmp/resolv_v4.conf` | data_call_status_ind_cb | shell cat | IPv4 DNS 配置 |
| `/tmp/resolv_v6.conf` | data_call_status_ind_cb | shell cat | IPv6 DNS 配置 |
| `/etc/resolv.conf` | data_call_status_ind_cb | 系统 DNS 解析 | 系统 DNS 配置 |
| `/usr/dial/tz.ini` | saveTimezone | 其他进程 | 时区信息（iniparser 格式） |
| `/usrdata/dial_config.txt` | write_config | read_config | 重启守护持久化配置 |
| `/media/sdcard/dial_log/*.log` | dial_log / log_init | 人工查阅 | 拨号日志文件 |
| `/media/sdcard/traffic/traffic.db` | TrafficDatabase | TrafficMonitor | 流量统计 SQLite 数据库 |

---

*本文档逐行分析 modem_mng 仓库全部 101 个文件，涵盖每一个函数、变量、宏定义和关键设计决策，包括已知 Bug 和设计权衡。*
---

## 21. dialer.hpp — 抽象拨号器与全平台类定义（逐行）

### 21.1 头文件包含段

```cpp
#include <iostream>
#include <memory>
#include <functional>
#include <fcntl.h>
#include <sys/stat.h>
#include <regex>
#include <stdlib.h>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
```
混合包含 C++ 风格头（`<iostream>`, `<memory>`, `<functional>`, `<regex>`）和 C 风格头（`<fcntl.h>`, `<sys/stat.h>` 等）：
- `<memory>`：`std::make_unique`、`std::unique_ptr`（用于 `dial_mng_t` RAII 管理）
- `<functional>`：`std::function<void()>`（拨号成功的回调类型）
- `<regex>`：C++11 正则表达式（用于解析 IMSI/IMEI 等数字串）
- `<sys/ioctl.h>` + `<net/if.h>`：网络接口控制（`SIOCGIFINDEX` 检测网卡）

```cpp
#ifdef USE_EC200A_DIAL
#include "dial.h"
#endif
#include "nanomsg_process.hpp"
#include "led_control.hpp"
```
条件包含 `dial.h`：只有 EC200A 平台才需要 `dial_mng_t` 结构和 `dial_stat_enu` 枚举。`led_control.hpp` 是 LED 控制客户端头文件（通过 nanomsg 连接到 LED 服务）。

### 21.2 拨号状态枚举（非 EC200A 版本）

```cpp
#if !USE_EC200A_DIAL
typedef enum
{
    dial_stat_none,           // 0：初始状态
    dial_stat_init,           // 1：基础初始化
    dial_stat_sim_init,       // 2：SIM 初始化
    dial_stat_sim_check,      // 3：SIM 检查
    dial_stat_sim_op,         // 4：SIM 操作中
    dial_stat_reg_check,      // 5：网络注册检查
    dial_stat_precondition_check, // 6：前置条件检查
    dial_stat_pre_start_call, // 7：拨号前准备
    dial_stat_start_call,     // 8：开始拨号
    dial_stat_stop_call,      // 9：停止拨号
    dial_stat_stop_cfun,      // 10：关闭射频
    dial_stat_start_cfun,     // 11：开启射频
    dial_stat_list_oper,      // 12：列出运营商
    dial_stat_select_oper,    // 13：选择运营商
    dial_stat_wait_for_connect, // 14：等待连接
    dial_stat_net_connected,  // 15：已连接
} dial_stat_enu;
#endif
```
对比 `ec200a/dial.h` 中的版本（10 个状态），这里有 16 个更细粒度的状态，增加了：
- `dial_stat_sim_init` / `dial_stat_sim_check` / `dial_stat_sim_op`：SIM 卡三阶段管理
- `dial_stat_reg_check`：网络注册独立为一个状态
- `dial_stat_precondition_check`：拨号前置条件（信号强度、SIM 状态等）检查状态
- `dial_stat_list_oper` / `dial_stat_select_oper`：i.MX6ULL 的运营商选择状态

```cpp
typedef dial_stat_enu DialStatEun;
```
类型别名：提供驼峰命名版本（`DialStatEun`），与 C++ 风格保持一致，而原始的 `dial_stat_enu` 是 C 命名风格。

### 21.3 Dialer 抽象基类

```cpp
class Dialer
{
public:
    virtual ~Dialer() {}
```
虚析构函数：是多态基类的必要条件。若不声明为虚析构，通过基类指针删除派生类对象时只会调用基类析构函数，导致派生类的资源未释放（内存泄漏）。此处实现为空（`{}`），不做任何清理。

```cpp
    virtual void startDialing() = 0;
    virtual void stopDialing() = 0;
    virtual int modem_init() = 0;
    virtual int create_call() = 0;
    virtual int start_call() = 0;
    virtual void stop_call() = 0;
    virtual void stop_cfun() = 0;
    virtual void start_cfun() = 0;
    virtual void dial_loop(void *cpactive, std::string &interface_name,
                           std::function<void()> callback) = 0;
```
9 个纯虚函数（`= 0`）：Dialer 是抽象类，不能实例化。每个派生类必须实现所有纯虚函数，否则派生类也是抽象类。

`dial_loop` 的参数设计：
- `void *cpactive`：进程保活句柄（类型擦除，避免对 appmng 库的直接依赖）
- `std::string &interface_name`：引用传递，函数内填入拨号成功的网卡名（OUT 参数）
- `std::function<void()>`：C++ 函数包装器，支持 lambda、函数指针、仿函数，比 C 函数指针更灵活

```cpp
    DialStatEun getStatus() const
    {
        return status_;
    }

private:
    DialStatEun m_status; // 命名不一致：声明为 m_status，实际用 status_
    std::string m_apn;    // 定义但从未使用（死代码）
    int m_modem_type;     // 定义但从未使用（死代码）

protected:
    void setStatus(DialStatEun status)
    {
        status_ = status;
    }

private:
    DialStatEun status_ = dial_stat_none;
```
**设计问题**：`m_status` 和 `status_` 两个同类型私有成员共存，`getStatus` 读取 `status_`，`setStatus` 也写入 `status_`，`m_status` 实际上未被使用（是之前重构留下的遗留变量）。`m_apn` 和 `m_modem_type` 同样是死代码。

### 21.4 EC200ADialer 类详细实现

```cpp
#ifdef USE_EC200A_DIAL
extern "C"
{
#include "ql-sdk/ql_type.h"
#include "ql-sdk/ql_data_call.h"
#include "ql-sdk/ql_sim.h"
#include "ql-sdk/ql_nw.h"
#include "apn.h"
#include "nw.h"
#include "data_call.h"
#include "test_utils.h"
#include "json-c/json.h"
#include "include/log.h"
#include <string.h>
#include "_public.h"
#include "iniparser.h"
}
```
在 C++ 头文件中用 `extern "C"` 包含 C 库头文件：确保这些 C 库的函数声明不被 C++ 名字改编。`iniparser.h` 是 INI 文件解析库（用于读写 `/usr/dial/tz.ini`）。

```cpp
#define DATA_CALL_APN_PUBLIC 6
#define DATA_CALL_ID_PUBLIC 1
#define APN_NAME_PUBLIC "apnpublic"
#define TZ_SAVE_PATH "/usr/dial/tz.ini"
```
在 `dialer.hpp` 中重新定义与 `_public.h` 相同的宏（与 `_public.h` 中的定义相同）：这是由于 `_public.h` 在 `extern "C"` 块内包含，而这些宏在 EC200A Dialer 的 C++ 方法中使用。`TZ_SAVE_PATH` 是时区配置文件路径，仅在此处定义。

```cpp
class EC200ADialer : public Dialer
{
public:
    static std::string dev_name;    // 网卡名（ccinet0 等）
    static std::string imsi;        // IMSI
    static std::string imei;        // IMEI
    static std::string operatorName; // 运营商名
    static std::string plmn;        // PLMN 码
    static std::string iccid;       // ICCID
    bool tz_fetched_ = false;       // 时区是否已获取（防止重复获取）
```
**静态成员**：所有 `EC200ADialer` 实例共享，全局唯一。之所以设计为静态，是因为 `ModemReqHandler`（在另一个线程）需要读取这些值（通过 `EC200ADialer::imsi` 直接访问），而无需持有 `EC200ADialer` 对象的引用。这是一种简单但不够安全的线程共享方式（`std::string` 的读写不是原子的）。

```cpp
    EC200ADialer() : callid(DATA_CALL_ID_PUBLIC), isExist(false), apn_issetted(false)
    {
        p_dial_mng = std::make_unique<dial_mng_t>();
    }
```
构造函数初始化列表：
- `callid`：从默认 ID（1）开始
- `isExist`：false（程序正在运行）
- `apn_issetted`：false（APN 未设置）
- `p_dial_mng`：用 `std::make_unique` 创建 `dial_mng_t`（RAII 管理，EC200ADialer 析构时自动释放）

```cpp
    bool parseTimezone(const std::string& response, int& tz)
    {
        // response 示例: "Mon, 16 Jun 2025 11:10:35 +0800"
        size_t tz_pos = response.find_last_of(' ');
```
从 `date -R` 命令输出中解析时区：
- `find_last_of(' ')`：找最后一个空格，之后就是时区字符串（"+0800"）
- 验证格式合法性：`tz_part[0]` 是 '+'/'-'，`tz_part[1]`、`tz_part[2]` 是数字

```cpp
        int hour = std::stoi(tz_part.substr(1, 2));  // 提取小时数（"08"→8）
        int sign = (tz_part[0] == '+') ? 1 : -1;    // 符号
        tz = sign * hour * 4;  // 转换为 15 分钟单位（+0800 → 32）
```
时区编码规则：每小时等于 4 个 15 分钟单位。+0800（UTC+8）= 8 × 4 = 32。这是 Quectel AT 命令（`AT+CCLK?`）使用的时区格式，方便与模块的时区设置对接。

```cpp
    void saveTimezone(int tz)
    {
        char tz_info_bytes[32] = {0};
        if (access(TZ_SAVE_PATH, F_OK) == -1) {
            system("mkdir -p /usr/dial");
            FILE *fp_default = fopen(TZ_SAVE_PATH, "w");
            if (fp_default != NULL) {
                fprintf(fp_default, "[dev]\ntz = 32\n");  // 默认 UTC+8
                fclose(fp_default);
            }
        }
```
创建默认 INI 文件（如果不存在）：`[dev]` 节名，`tz = 32`（UTC+8 默认时区）。使用 iniparser 库读写，INI 格式比 JSON 更简单，适合只有一个配置项的场景。

```cpp
        dictionary* ini = iniparser_load(TZ_SAVE_PATH);
        int ini_tz = iniparser_getint(ini, "dev:tz", 255);
        if (ini_tz != tz) {  // 仅在时区变化时写入
            iniparser_set(ini, "dev", NULL);        // 确保 section 存在
            sprintf(tz_info_bytes, "%d", tz);
            iniparser_set(ini, "dev:tz", tz_info_bytes);
        }
        FILE* fp = fopen(TZ_SAVE_PATH, "w");
        iniparser_dump_ini(ini, fp);
        fclose(fp);
        iniparser_freedict(ini);
        sync();  // 同步文件系统，确保写入物理介质
    }
```
`iniparser_set(ini, "dev", NULL)`：传 NULL 值表示"确保此 section 存在"（iniparser 的特殊用法）。`sync()` 系统调用刷新所有脏缓冲区到磁盘，在关键数据持久化后调用，防止突然断电导致数据损失。

```cpp
    bool test_can_ping_google()
    {
        char cmd[256];
        char line[256];
        bool is_success = false;

        snprintf(cmd, sizeof(cmd), "ping -c 1 -W 2 8.8.8.8  2>/dev/null");
```
Ping 命令参数：
- `-c 1`：只发 1 个包
- `-W 2`：等待响应超时 2 秒（不是 `-w`，`-W` 是等待时间，`-w` 是截止时间）
- `2>/dev/null`：丢弃错误输出（如 "ping: connect: Network is unreachable"）

```cpp
        FILE *fp = popen(cmd, "r");
        if (fp == NULL) return false;

        while (fgets(line, sizeof(line), fp) != NULL) {
            if (strstr(line, "ttl=") || strstr(line, "TTL=")) {
                is_success = true;
                break;  // 读到成功行就退出，不等待全部输出
            }
        }

        pclose(fp);
        // 故意忽略 pclose 返回值：SIGCHLD 信号处理可能导致返回 -1
        return is_success;
    }
```
`ttl=` 检测法优于 `system()` 返回值法：即使信号处理函数干扰了子进程的状态（导致 `pclose` 返回 -1），`ttl=` 字符串出现在输出中仍然可靠地表示 Ping 成功。这修复了使用 `system()` 时 SIGCHLD 信号竞争条件的问题。

```cpp
    bool isConnectedToInternet()
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) { std::cerr << "Could not create socket\n"; return false; }

        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(53);                    // DNS 端口
        inet_pton(AF_INET, "8.8.8.8", &sa.sin_addr); // Google DNS

        int res = connect(sock, (sockaddr *)&sa, sizeof(sa));
        close(sock);
        return res >= 0;
    }
```
TCP 连接检测法（区别于 Ping）：创建 TCP 套接字尝试连接 8.8.8.8:53（Google DNS 的 TCP 端口），不发送任何数据。成功 `connect()` 意味着 TCP 握手完成，网络可达。此方法不需要 ICMP 权限（某些防火墙可能拦截 ICMP 但允许 TCP），更可靠。注意：如果运营商不允许访问 8.8.8.8 但允许访问其他地址，此方法可能误报不可达。

```cpp
    void setLEDCh(LED_ID_t value) { led_ch = value; }
    LED_ID_t getLEDCh() const { return led_ch; }
    void setAPNSetted(bool value) { apn_issetted = value; }
    bool getAPNSetted() const { return apn_issetted; }
```
简单的 getter/setter：LED 通道 ID 和 APN 设置标志。使用 getter/setter 而非公开成员，保留以后添加验证逻辑的能力。

```cpp
    static void data_call_status_ind_cb(int call_id,
                                        QL_NET_DATA_CALL_STATUS_E pre_call_status,
                                        ql_data_call_status_t *p_msg)
```
SDK 回调声明为**静态**成员函数：
- SDK 需要一个普通 C 函数指针，不能是非静态成员函数（有隐含的 `this` 参数）
- 静态成员函数没有 `this` 指针，可以直接作为 C 回调
- 但静态函数无法直接访问非静态成员，因此使用静态成员变量（`EC200ADialer::dev_name` 等）存储共享状态

```cpp
private:
    LED_ID_t led_ch;
    bool apn_issetted;
    int callid;      // Data Call ID，失败时自增
    bool isExist;    // 程序退出标志（SIGTERM 设置）
    std::unique_ptr<dial_mng_t> p_dial_mng;  // RAII 管理的拨号状态

    int check_process(const char *process_name) {
        char command[256];
        snprintf(command, sizeof(command), "pgrep %s > /dev/null", process_name);
        return system(command);
    }
```
`check_process` 私有方法：检测进程是否存在（`pgrep` 找到则返回 0）。将 stdout 重定向到 `/dev/null` 压制输出，但忽略了 stderr（pgrep 找不到进程时输出到 stderr）。

```cpp
    void run_process(const char *path_to_program) {
        execl(path_to_program, path_to_program, (char *)NULL);
        perror("exec failed");  // execl 成功时不会执行到这里
    }
```
`execl` 替换当前进程映像（`exec` 族函数）。如果成功，`perror` 之后的代码永远不会执行；如果失败（如路径不存在），`perror` 打印错误原因。此方法用于在子进程中启动新程序（配合 fork 使用）。

### 21.5 Imx6uDialer 类（i.MX6ULL 平台）

```cpp
#ifdef  USE_IMX6U_DIAL

enum State
{
    POWER_ON,          // 给 modem 上电
    CONFIGURE_USB,     // 配置 USB 驱动
    WRITE_TO_MODEM,    // 通过 AT 命令配置 modem
    CONFIGURE_NETWORK, // 配置网络
    CHECK_CONNECTION,  // 检查连接状态
    SUCCESS,           // 拨号成功
    FAILURE_RETRY,     // 失败重试
    LIST_OPERATOR,     // 列出可用运营商
    SELECT_OPERATOR    // 选择运营商
};
```
i.MX6ULL 状态机使用 8 个状态（而非 EC200A 的 5 个巡检阶段），更复杂但对 USB modem 的异常情况（运营商漫游、网络搜索）处理更完整。

```cpp
    static std::string sendATCommand(const std::string& command,
                                     const std::string& serialPortPath)
    {
        std::string response;
        int fd = open(serialPortPath.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd == -1) {
            std::cerr << "Unable to open port " << serialPortPath
                     << ": " << strerror(errno) << std::endl;
            return response;
        }
        fcntl(fd, F_SETFL, FNDELAY);  // 设置非阻塞
```
直接打开串口文件（如 `/dev/ttyUSB0`）而非通过 `serial_atcmd` 工具：这是 i.MX6ULL 的特有做法，因为系统中没有 Quectel 提供的 `serial_atcmd` 工具。`O_NDELAY` 打开时非阻塞；`F_SETFL FNDELAY` 设置后续 read/write 为非阻塞。

```cpp
        // 先清空读缓冲区（注释掉的代码）
        // while ((n = read(fd, buffer, ...)) > 0) { ; }

        if (write(fd, command.c_str(), command.size()) < 0) {
            std::cerr << "Failed to write: " << strerror(errno) << std::endl;
            close(fd); return response;
        }

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);
        struct timeval timeout;
        timeout.tv_sec = 5;   // 5 秒超时
        timeout.tv_usec = 0;

        int retval = select(fd + 1, &read_fds, NULL, NULL, &timeout);
        if (retval > 0 && FD_ISSET(fd, &read_fds)) {
            while ((n = read(fd, buffer, sizeof(buffer)-1)) > 0) {
                buffer[n] = '\0';
                response += std::string(buffer, n);
            }
        }
        close(fd);
        return response;
    }
```
每次 AT 命令都 open+close 串口：避免文件描述符泄漏，但增加了系统调用开销。对于嵌入式系统中低频率的 AT 命令，这是可接受的。注释掉的清空缓冲区代码原本用于清除残留数据，但删掉后可能读到旧数据——这是未解决的边缘情况。

```cpp
    static std::string sendATCommand(const std::string& command,
                                     const std::string& serialPortPath,
                                     int timeoutSec)
```
重载版本：增加了 `timeoutSec` 参数（如 `AT+COPS=?` 需要 180 秒等待运营商扫描完成）。默认版本硬编码 5 秒，此版本允许调用方指定超时。

```cpp
    bool isVendorPresent(const std::string& vendorID) {
        std::array<char, 128> buffer;
        std::string result;
        FILE* pipe = popen("lsusb", "r");
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }
        pclose(pipe);
        return result.find(vendorID) != std::string::npos;
    }
```
通过 `lsusb` 命令检测 USB 设备是否存在（VendorID 如 "2c7c" 是 Quectel 的 USB VID）。`std::array` 提供比裸数组更安全的界面（`.data()` 返回指针，`.size()` 返回大小）。

```cpp
    static void get_imsi(const std::string& serialPortPath) {
        if (imsi.empty() || imsi.length() < 5) {
            std::string response = sendATCommand("AT+CIMI\r\n", serialPortPath);
            std::regex re("\\d+");  // 匹配连续数字串
            std::smatch match;
            if (std::regex_search(response, match, re)) {
                imsi = match[0];
            }
        }
    }
```
懒惰初始化（Lazy Initialization）：只在 imsi 为空或过短（< 5 位，视为无效）时才发送 AT 命令。正则表达式 `\\d+`（等同于 `\d+`）匹配一个或多个连续数字——这会匹配到 AT 命令响应中的第一个数字串，通常就是 IMSI。但如果响应中有其他数字（如回显的命令行号），可能匹配错误。

```cpp
    static void get_csq(const std::string& serialPortPath) {
        std::string response = sendATCommand("AT+CSQ\r\n", serialPortPath);
        if (response.find("+CSQ:") != std::string::npos) {
            std::regex re("\\d+");
            std::smatch match;
            if (std::regex_search(response, match, re)) {
                csq = std::stoi(match[0]);
            }
        }
    }
```
注意：CSQ 值不做缓存（每次都直接赋值），与 IMSI/IMEI 的懒惰初始化不同。信号强度需要实时刷新，而 IMSI 固定不变。

```cpp
    static std::string adapterAT00(const std::string& serialPortPath) {
        while (!commandSuccess) {
            response = sendATCommand("ATE0\r\n", serialPortPath);
            if (response.find("OK") != std::string::npos
                && response.find("ERROR") == std::string::npos) {
                commandSuccess = true;
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
        return response;
    }
```
无限重试版本（`adapterAT00`）：持续发送 `ATE0`（关回显）直到成功。用于程序启动时确认 modem 就绪，但没有超时机制——如果 modem 损坏，程序会永久阻塞。

```cpp
    static bool adapterAT(const std::string& serialPortPath, int timeoutSeconds) {
        int elapsedSeconds = 0;
        while (!commandSuccess) {
            // 与 adapterAT00 相同的逻辑
            if (elapsedSeconds >= timeoutSeconds) {
                std::cout << "Timeout reached." << std::endl;
                return false;  // 超时返回 false
            }
            elapsedSeconds += 2;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        return true;
    }
```
有超时版本（`adapterAT`）：生产代码推荐使用此版本，超时后返回 false 让调用方处理。两个版本共存是代码演进的痕迹。

```cpp
    static void updateNetworkStatus(const std::string& status) {
        static const std::string statusDescriptions[] = {
            "Not registered. ME is not currently searching",  // 0
            "Registered, home network",                       // 1
            "Not registered, but ME is searching",            // 2
            "Registration denied",                            // 3
            "Unknown",                                        // 4
            "Registered, roaming"                             // 5
        };
        int statusIndex = std::stoi(status);
        if (statusIndex >= 0 && statusIndex < 6) {
            networkRegStatus = statusDescriptions[statusIndex];
        } else {
            networkRegStatus = "Invalid status code";
        }
    }
```
将 `AT+CREG?` 返回的数字状态码转换为可读字符串。`std::stoi` 转换时，若 `status` 不是有效整数会抛出 `std::invalid_argument` 异常（调用方未处理——潜在问题）。

```cpp
    // GPIO 控制（i.MX6ULL modem 上电）
    int gpio_export(int gpio);    // 导出 GPIO
    int gpio_config(int gpio, const char *attr, const char *val);  // 配置 GPIO

    void powerOnModule()
    {
        gpio_export(GPIO_POWER);    // GPIO_POWER = 130
        gpio_export(GPIO_PWRKEY);   // GPIO_PWRKEY = 128

        gpio_config(GPIO_POWER, "direction", "out");
        gpio_config(GPIO_PWRKEY, "direction", "out");

        std::cout << "--------- power off USB Modem" << std::endl;
        gpio_config(GPIO_POWER, "value", "0");   // 先断电
        sleep(2);

        std::cout << "+++++++++ power on USB Modem" << std::endl;
        gpio_config(GPIO_POWER, "value", "1");   // 上电
        sleep(1);

        // 模拟开机脉冲（按下 PWRKEY 按键）
        gpio_config(GPIO_PWRKEY, "value", "0");
        sleep(1);
        gpio_config(GPIO_PWRKEY, "value", "1");
        sleep(2);
        gpio_config(GPIO_PWRKEY, "value", "0");
    }
```
电源管理时序：
1. 断电（GPIO 130 = 0）
2. 等待 2 秒（电容放电）
3. 上电（GPIO 130 = 1）
4. 等待 1 秒（稳定）
5. 模拟 PWRKEY 按键脉冲（0→1→0，类似长按开机键）

GPIO 130 和 128 是 i.MX6ULL 上 modem 模块的电源管理引脚（硬件原理图确定的引脚号）。

```cpp
    int configureUSBModule()
    {
        std::ofstream usbSerialFile("/sys/bus/usb-serial/drivers/option1/new_id");
        if (usbSerialFile.is_open()) {
            usbSerialFile << "2c7c 0901";  // Quectel EC21/EC25 PID:VID
            usbSerialFile.close();
        } else {
            return -1;
        }
        std::ifstream ttyUSB0File("/dev/ttyUSB0");
        if (!ttyUSB0File.is_open()) return -1;
        return 0;
    }
```
USB 驱动注册：向 Linux USB serial 驱动的 `new_id` sysfs 文件写入 VendorID:ProductID（`2c7c 0901`），动态注册新设备支持（无需修改内核驱动或 udev 规则）。这是在 Linux 上支持新 USB 设备的标准方法。

---

## 22. dialer_eg25.c — EG25G 平台 main() 逐行

```c
#define MAIN_VERSION 1
#define SUB_VERSION 30
```
版本号宏：1.30 版本。EG25G 版本与 EC200A 版本（也是 1.30）保持同步，方便统一版本管理。

```c
pthread_mutex_t g_at_port_mutex = PTHREAD_MUTEX_INITIALIZER;
```
AT 端口访问互斥锁：静态初始化（`PTHREAD_MUTEX_INITIALIZER`），在文件作用域初始化，无需 `pthread_mutex_init()` 调用。`g_at_port_mutex` 在 `eg25/nw/nw.c` 中通过 `extern` 声明引用，保护 AT 端口的并发访问（主循环 CSQ 查询 vs 其他 AT 命令）。

```c
int main(int argc, char **argv)
{
    bool start_monitor = false;
    pthread_t newthread;
    int ret;

    log_init();  // 初始化 SD 卡日志（同 EC200A 版本）
```
`bool` 类型：EG25G 是 C 文件，需要 `<stdbool.h>`（通过 `ql_oe.h` 间接包含）。

```c
    void *cpactive = (void *)create_cpactive();
    int timeout = 60;
    const char *pname = "modem_mng";
    const char *pversion = "1.33";
```
EG25G 版本号为 "1.33"（比 EC200A 的 "1.30" 更新），使用 `(void *)` 强转（`create_cpactive` 可能返回平台特定类型）。

```c
    ConfigParserWrapper *parser = ConfigParser_create("/etc/config/config.json");
    int value = ConfigParser_getIntValue(parser, "C020102");
```
使用 C 接口（`ConfigParser_create` / `ConfigParser_getIntValue`）而非 C++ 直接调用，因为这是 C 文件。返回值 `value` 存储但未直接使用（后续应该设置到 TrafficMonitor，但代码缺失）。

```c
    NanoReqHandlerWrapper *handler = startNanoReqHandler("tcp://127.0.0.1:38001");
```
通过 C 接口启动 nanomsg REP 服务，内部创建 `NanoReqHandler` 对象并启动 pthread 线程。

```c
    dial_mng_t *p_dial_mng = dial_mng_new();
    TrafficMonitorWrapper *monitor = create_TrafficMonitor("");
    p_dial_mng->monitor = monitor;  // 将流量监控对象注入拨号管理结构
    p_dial_mng->cpa = cpactive;     // 将保活对象注入拨号管理结构

    ret = pthread_create(&newthread, NULL, dial_task, (void *)p_dial_mng);
```
拨号任务作为独立 pthread 启动，`dial_mng_t` 作为线程参数传入。EG25G 使用 pthread 而非 `std::thread`，保持与 C 代码的兼容性。

```c
    // 信号监控变量（初始化未显式赋值）
    bool signalLow;    // 未初始化！
    bool signalHigh;   // 未初始化！
    time_t lowSignalStartTime;   // 未初始化
    time_t highSignalStartTime;  // 未初始化
```
**Bug**：这些变量未初始化，首次使用时读取栈上的随机值。若程序启动时 `signalLow` 碰巧为 true，后续逻辑可能立即触发超时事件。正确做法应在声明时初始化为 `false` 和 `0`。

```c
    while (true)
    {
        // 等到 4G 网卡出现后才启动流量监控
        if ((strlen(get_interface(monitor)) != 0) && (start_monitor == false))
        {
            printf("now start traffic monitor %s \n", get_interface(monitor));
            start_monitor = true;
            check_4g_hasTraffic(monitor);
            check_4g_overlimit(monitor);
        }
```
`get_interface(monitor)` 被调用**两次**（if 条件和 printf 中），每次都返回临时 `c_str()` 指针（悬空指针风险，前面已分析）。应该先保存到 `std::string` 变量再使用。

```c
        int* csq_ptr = get_eg25_csq(handler);
        *csq_ptr = nw_at_get_csq(p_dial_mng->smd_fd);
```
通过指针直接更新 `NanoReqHandler::ModemReqHandler::csq`：`get_eg25_csq` 返回 CSQ 字段的地址，`nw_at_get_csq` 获取当前信号值并直接写入。这是一种"内外协作"的数据共享方式，`NanoReqHandler` 无需主动查询 CSQ，由外部循环更新。

```c
        // 信号监控（低/高信号告警）
        now = time(NULL);
        if (signalValue < 20) {
            if (!signalLow) {
                signalLow = true;
                lowSignalStartTime = now;
            } else if (now - lowSignalStartTime >= 300) {  // 5 分钟
                NetWork_EventPublisher_pub(publisher, "SIGNAL",
                    "Signal strength low", 208);
            }
        } else {
            signalLow = false;
        }
        sleep(5);  // 每 5s 检查一次
    }
```
EG25G 主循环的信号监控逻辑与 `SimSignalMonitor.cpp` 中的实现几乎相同，但：
1. 使用 `time_t` 和整数比较，而非 `std::chrono`
2. 直接在主循环中处理，而非独立线程
3. `sleep(5)` 是粗粒度等待，所有检查间隔都是 5 秒（包括网卡检测和信号检测）

---

## 23. dialer_imx6ull.cpp — i.MX6ULL 平台 dial_loop() 逐行

```cpp
void Imx6uDialer::dial_loop(void *cpactive, std::string &interface_name,
                             std::function<void()> callback)
{
    State state = POWER_ON;          // 初始状态：给 modem 上电
    int checkConnectionAttempts = 0;
    std::string lastFailureReason;

    LedControl led(33);              // GPIO 33 控制 LED
    led.startBlinking(500);          // 500ms 间隔闪烁（初始状态）
```
i.MX6ULL LED 使用 `LedControl` 类（GPIO 直接控制），而不是 EC200A 的 nanomsg LED 服务（`LEDControl_create`）。两种完全不同的 LED 控制方式，体现了平台差异。

```cpp
    CC_Deque *deque_oper;
    cc_deque_new(&deque_oper);
```
创建双端队列存储运营商列表：`cc_deque` 是第三方 C 双端队列库（`cc_deque/`）。当 modem 在国外漫游时，需要手动扫描并选择本地运营商，队列存储可用的 PLMN 码。

```cpp
    // 检测 USB modem 是否已插入
    std::string vendorID = "2c7c";
    if (isVendorPresent(vendorID)) {
        std::cout << "USB device with vendor ID " << vendorID << " is present." << std::endl;
    }

    // 检测 ttyUSB0 是否已存在（可能已经上电）
    {
        struct stat buffer;
        if (stat("/dev/ttyUSB0", &buffer) == 0)
        {
            // 如果 modem 已经在运行，先适配 AT
            if(adapterAT("/dev/ttyUSB0", 10) == false) {
                powerOffModule();  // 适配失败则重新上电
            }
        }
    }
```
程序启动时的状态检测：
1. `lsusb` 检测 USB Quectel 设备是否存在
2. `stat("/dev/ttyUSB0")` 检测串口是否已创建（说明内核已识别 modem）
3. 若串口存在但 AT 不通（10 秒超时），说明 modem 异常 → 重新上电

```cpp
    while (!isExist)  // isExist 由 SIGTERM 信号处理函数设置
    {
        switch (state)
        {
        case POWER_ON:
            system("rm /tmp/dial_success");  // 清除上次拨号成功标志
            powerOnModule();   // GPIO 操作：断电+上电+PWRKEY 脉冲
            sleep(10);         // 等待 modem 启动（需要约 5-10 秒）
            state = CONFIGURE_USB;
            break;
```
10 秒等待是 modem 硬件启动时间，从上电到 AT 口就绪通常需要 5-10 秒（固件加载、SIM 初始化等）。

```cpp
        case CONFIGURE_USB:
            // 注册 USB 设备 ID 到 USB serial 驱动
            if(configureUSBModule() != 0) {
                std::cerr << "USB module configuration failed" << std::endl;
                state = FAILURE_RETRY;
            } else {
                state = WRITE_TO_MODEM;
            }
            break;
```
USB 配置：向 `/sys/bus/usb-serial/drivers/option1/new_id` 写入 VID:PID。若失败（如 `/dev/ttyUSB0` 不存在）则进入重试状态。

```cpp
        case WRITE_TO_MODEM:
            // 发送初始化 AT 命令序列
            Imx6uDialer::adapterAT("/dev/ttyUSB0", 20);
            Imx6uDialer::sendATCommand("AT+CGATT=0\r\n", "/dev/ttyUSB0");   // 断开数据附着
            Imx6uDialer::sendATCommand("AT+CGACT=0,1\r\n", "/dev/ttyUSB0"); // 断开 PDP 上下文
            Imx6uDialer::sendATCommand("AT+QNETDEVCTL=0\r\n", "/dev/ttyUSB0"); // 停止拨号
            sleep(2);
            Imx6uDialer::sendATCommand("AT+CGDCONT=1,\"IPV4V6\",\"\"\r\n", "/dev/ttyUSB0"); // APN 设置
            Imx6uDialer::sendATCommand("AT+QCFG=\"usbnet\",1\r\n", "/dev/ttyUSB0");  // USB ECM 模式
            state = CONFIGURE_NETWORK;
            break;
```
AT 命令初始化序列：
- `AT+CGATT=0`：分离数据网络
- `AT+CGACT=0,1`：停用 PDP 上下文 1
- `AT+QNETDEVCTL=0`：停止网络设备控制（Quectel 专有）
- `AT+CGDCONT=1,"IPV4V6",""`：设置 PDP 上下文（空 APN 自动选择）
- `AT+QCFG="usbnet",1`：设置为 ECM 模式（USB 以太网），这样 modem 会出现为 usb0 网卡

```cpp
        case CONFIGURE_NETWORK:
            Imx6uDialer::sendATCommand("AT+CGATT=1\r\n", "/dev/ttyUSB0");
            Imx6uDialer::sendATCommand("AT+CGACT=1,1\r\n", "/dev/ttyUSB0");
            Imx6uDialer::sendATCommand("AT+QNETDEVCTL=3,1,1\r\n", "/dev/ttyUSB0");
```
激活网络：
- `AT+CGATT=1`：附着数据网络
- `AT+CGACT=1,1`：激活 PDP 上下文 1
- `AT+QNETDEVCTL=3,1,1`：启动 ECM 拨号（参数：3=启动，1=profile，1=自动重连）

```cpp
            if (!Imx6uDialer::adapterAT("/dev/ttyUSB5", 20)) {
                state = FAILURE_RETRY;
                break;
            }
```
注意：验证 AT 命令从 `/dev/ttyUSB5`（状态查询端口）而非 `/dev/ttyUSB0`（控制端口）进行。Quectel modem 在 USB 复合设备中暴露多个 ttyUSB 设备，不同端口有不同功能（控制、数据、NMEA GPS 等）。

```cpp
            usb0Available = false;
            for (int i = 0; i < maxAttempts; i++) {
                if (check_usb0_interface() == 1) {
                    usb0Available = true;
                    break;
                }
                sleep(1);
            }
```
等待 usb0 网卡出现（最多 `maxAttempts=10` 秒）。`check_usb0_interface()` 通过 `opendir("/sys/class/net/")` 遍历网卡目录查找 "usb0"（比 `ioctl SIOCGIFINDEX` 方式更简单）。

```cpp
        case CHECK_CONNECTION:
            // 通过 ping 检测连通性
            if (test_can_ping_google()) {
                state = SUCCESS;
            } else {
                checkConnectionAttempts++;
                if (checkConnectionAttempts > 30) {
                    // 30 次 ping 失败（约 60 秒）→ 尝试切换运营商
                    state = LIST_OPERATOR;
                    checkConnectionAttempts = 0;
                }
                sleep(2);
            }
            break;
```
与 EC200A 的分级恢复不同，i.MX6ULL 在网络长时间不可达后（30次×2s=60s）尝试**切换运营商**（`LIST_OPERATOR`），这是针对国际漫游场景的特殊处理。

```cpp
        case SUCCESS:
            dev_name = "usb0";          // i.MX6ULL 网卡固定为 usb0
            interface_name = dev_name;
            callback();                 // 通知 main 线程
            sleep(10);
            state = CHECK_CONNECTION;   // 成功后继续检测，不断开
            checkConnectionAttempts = 0;
            break;
```
拨号成功后**不保持 SUCCESS 状态**，而是转回 `CHECK_CONNECTION` 持续监控，确保长期稳定性。这与 EC200A 的 `dial_loop` 中持续 Ping 的逻辑等价。

```cpp
        case LIST_OPERATOR:
            if (!operatorName.empty()) {
                // 已知运营商，直接选择
                sprintf(dial_oper_slecet_str, "AT+COPS=1,2,\"%s\",7\r\n",
                        operatorName.c_str());  // 7=LTE
```
`AT+COPS=1,2,"<PLMN>",7`：
- 1=手动选择模式
- 2=数字格式（PLMN 码）
- 7=接入技术（7=LTE，0=2G，2=3G）

```cpp
                sendATCommand("AT+CFUN=0\r\n", "/dev/ttyUSB0", 30);  // 关闭射频
                sendATCommand("AT+CFUN=1\r\n", "/dev/ttyUSB0", 30);  // 开启射频
                sendATCommand(dial_oper_slecet_str, "/dev/ttyUSB0", 60); // 选择运营商（60s超时）
```
射频重置 + 运营商选择：`CFUN=0→CFUN=1` 重置无线子系统，然后用 `AT+COPS` 指定 PLMN 码。`AT+COPS` 可能需要较长时间（运营商网络搜索），因此设置 60 秒超时。

```cpp
                std::string response = sendATCommand("AT+COPS=?\\r\\n", "/dev/ttyUSB0", 180);
                parse_oper_list_info(deque_oper, const_cast<char*>(response.c_str()));
```
`AT+COPS=?` 扫描所有可用运营商（最多 180 秒，国际漫游时可能需要很长时间）。`const_cast<char*>` 将 `const char*` 转为 `char*`，因为 `parse_oper_list_info` 的参数是 `char*`（应改为 `const char*`，但修改库接口成本高）。

```cpp
int parse_oper_info_str(char *p_oper_info, int *p_stat, char *p_lg_oper,
                        char *p_sh_oper, char *p_nu_oper, int *p_act)
{
    // 解析 "2,\"CHN-UNICOM\",\"UNICOM\",\"46001\",2"
    int select_idx = 0;
    char *p_start = p_oper_info;
    while (true) {
        char *p_end = strchr(p_start, ',');
        if (NULL == p_end) return select_idx;
        char temp_msg[OPER_NAME_LENGTH];
        memset(temp_msg, 0, OPER_NAME_LENGTH);
        memcpy(temp_msg, p_start, p_end - p_start);
        switch (select_idx) {
        case 0: *p_stat = atoi(temp_msg); break;    // 状态（0/1/2）
        case 1: memcpy(p_lg_oper, temp_msg, ...); break; // 长名称（含引号）
        case 2: memcpy(p_sh_oper, temp_msg, ...); break; // 短名称
        case 3: memcpy(p_nu_oper, temp_msg, ...); break; // 数字 PLMN
        case 4: *p_act = atoi(temp_msg); break;     // 接入技术
        }
        select_idx++;
        p_start = p_end + 1;
    }
}
```
手工解析运营商信息字符串（CSV 格式但有引号）：循环查找逗号分隔符，按字段索引（0-4）分派。注意：长/短名称可能包含引号字符（`\"CHN-UNICOM\"`），未做引号剥离，导致运算符名称包含引号。

```cpp
bool parse_oper_list_info(CC_Deque *p_oper_deque, char *p_oper_list_str)
{
    // 输入: "+COPS: (2,\"CHN-UNICOM\",\"UNICOM\",\"46001\",2),(1,...)"
    char *p_data = strstr(p_oper_list_str, "+COPS:");
    if (p_data == NULL) return false;
    p_data += strlen("+COPS:");

    char *p_start = p_data;
    while (NULL != (p_start = strchr(p_start, '(')))
    {
        p_start += 1;
        char *p_end = strchr(p_start, ')');
        // 解析括号内的运营商信息
        memcpy(oper_info, p_start, p_end - p_start);
        parse_oper_info_str(oper_info, &stat, long_oper, ...);
        if (4 == scanf_ret)
        {
            operator_map[long_oper] = numberic_oper;  // 名称→PLMN 映射
            if (stat == 1)  // 可用运营商（stat=1）
            {
                char *p_deque_str = calloc(1, strlen(numberic_oper) + 1);
                memcpy(p_deque_str, numberic_oper, strlen(numberic_oper));
                cc_deque_add_last(p_oper_deque, p_deque_str);
            }
        }
    }
    return true;
}
```
运营商列表解析：将 `AT+COPS=?` 输出中的所有运营商信息提取到双端队列中，同时建立名称到 PLMN 码的映射（`operator_map`）。`stat == 1` 筛选可用运营商（0=未知，1=可用，2=当前）。每个运营商的 PLMN 码以堆分配字符串存入队列（`calloc`），使用后需要 `free`（由 `cc_deque_free_all` 处理）。

```cpp
void cc_deque_free_all(CC_Deque *deque) {
    if (deque == NULL) return;
    void *p_element = NULL;
    while (cc_deque_size(deque) > 0) {
        if (CC_OK == cc_deque_remove_first(deque, &p_element)) {
            free(p_element);  // 释放每个 calloc 分配的字符串
        }
    }
    free(deque);  // 释放队列结构本身
    deque = NULL; // 无效赋值（局部变量，调用方看不到）
}
```
**Bug**：`deque = NULL` 只修改了局部指针，调用方的指针不会变为 NULL。正确做法是传入 `CC_Deque **`（双重指针）并设置 `*deque = NULL`。这是 C 中指针参数的常见陷阱。

---

## 24. nanomsg_process.cpp — getStatus() 完整逐行分析

```cpp
std::string ModemReqHandler::getStatus()
{
    getCCINetStats(rxBytes, txBytes, net_interface);
    getDns();

    cJSON *cellular = cJSON_CreateObject();
```
每次 REP 请求到来时重新构建整个 JSON 对象：
- `getCCINetStats`：读取 `/sys/class/net/<if>/statistics/` 获取当前字节计数
- `getDns`：解析 `/etc/resolv.conf` 获取最新 DNS
- 每次请求都重新读取，确保数据最新（无缓存）

```cpp
    cJSON_AddStringToObject(cellular, "model", model.c_str());
    cJSON_AddStringToObject(cellular, "md_sw_ver", md_sw_ver.c_str());

    #ifdef USE_EC200A_DIAL
    cJSON_AddStringToObject(cellular, "md_hw_ver",
                            md_hw_ver.empty() ? "EC200A" : md_hw_ver.c_str());
    #elif defined(USE_EG25_DIAL)
    cJSON_AddStringToObject(cellular, "md_hw_ver",
                            md_hw_ver.empty() ? "EG25" : md_hw_ver.c_str());
    #elif defined(USE_IMX6U_DIAL)
    cJSON_AddStringToObject(cellular, "md_hw_ver",
                            md_hw_ver.empty() ? "imx6" : md_hw_ver.c_str());
    #else
    cJSON_AddStringToObject(cellular, "md_hw_ver",
                            md_hw_ver.empty() ? "empty" : md_hw_ver.c_str());
    #endif
```
编译时根据平台宏选择硬件版本字符串：若 `md_hw_ver` 未从 `/etc/quectel-project-version` 成功读取（为空），则使用硬编码的平台名称作为默认值。

```cpp
    if(!getImei().empty()){
        cJSON_AddStringToObject(cellular, "imei", getImei().c_str());
    }else{
        cJSON_AddStringToObject(cellular, "imei", "000");
    }
```
防御性编码：若 IMEI 为空（获取失败），填充 "000" 而非空字符串，避免 JSON 解析方遇到空字符串崩溃。但 "000" 是无效 IMEI（应为 15 位数字），可能让接收方误判为"未知设备"。

```cpp
    if(!getIccid().empty()){
        cJSON_AddStringToObject(cellular, "iccid", getIccid().c_str());
    }else{
        cJSON_AddStringToObject(cellular, "iccid", getImsi().c_str());
    }
```
**奇怪的降级逻辑**：ICCID 不可用时，用 IMSI 代替！ICCID（SIM 物理卡号）和 IMSI（用户标识号）是完全不同的概念，此处用 IMSI 填充 ICCID 字段是错误的——接收方可能将 IMSI 误认为 ICCID 使用。

```cpp
    std::string lac, ci;
    getCellInfo(lac, ci);  // EC200A: AT+CREG? → 提取引号内字符串

    if(lac.empty() || lac.length() < 3) {
        cJSON_AddStringToObject(cellular, "lac", "271a");  // 默认 LAC（硬编码）
    }else{
        cJSON_AddStringToObject(cellular, "lac", lac.c_str());
    }
    if(ci.empty() || ci.length() < 3) {
        cJSON_AddStringToObject(cellular, "cellid", "0d176547");  // 默认 CellID
    }else{
        cJSON_AddStringToObject(cellular, "cellid", ci.c_str());
    }
```
**硬编码默认值**："271a"（LAC）和 "0d176547"（Cell ID）在获取失败时作为默认值——这些显然是开发者测试时使用的具体基站信息，用作生产默认值会误导运营/监控系统。应改为 "unknown" 或 "0"。

```cpp
    cJSON_AddNumberToObject(cellular, "csq", getCsq());
```
`getCsq()` 内部通过 `serial_atcmd at+csq` 获取实时信号值，每次 REP 请求都会发送 AT 命令——频繁查询时可能占用 AT 端口较长时间。

```cpp
    // 流量数据（KB 为单位）
    TrafficMonitor traffic(net_interface);
    cJSON_AddNumberToObject(cellular, "tx", traffic.get_history_tx_bytes()/1024);
    cJSON_AddNumberToObject(cellular, "rx", traffic.get_history_rx_bytes()/1024);
```
每次调用 `getStatus()` 都创建一个新的 `TrafficMonitor` 对象：这会触发 `TrafficMonitor` 的构造函数，从 SQLite 数据库加载历史流量数据！这是一个严重的性能问题——每次 REP 请求都会执行 SQLite 查询。应该缓存 TrafficMonitor 实例或直接访问数据库。

```cpp
    char *json_output = cJSON_Print(cellular);  // 格式化 JSON（有缩进，适合调试）
    std::string payload(json_output);
    cJSON_Delete(cellular);  // 释放 cJSON 对象树
    free(json_output);       // 释放 cJSON_Print 分配的字符串
    return payload;
}
```
`cJSON_Print` 生成带缩进格式的 JSON（比 `cJSON_PrintUnformatted` 多空格和换行，约大 20-30%），但更易读。注意 `cJSON_Delete` 和 `free` 的顺序：先删除对象树（递归释放内部节点），再释放字符串。两个内存块是独立分配的，都必须释放。

---

## 25. 核心机制综合分析

### 25.1 多线程同步汇总

| 共享资源 | 写入线程 | 读取线程 | 同步机制 |
|---------|---------|---------|---------|
| `EC200ADialer::dev_name` | SDK 回调（data_call_status_ind_cb） | dialThread（dial_loop） | 无锁（潜在竞争） |
| `EC200ADialer::imsi/imei/iccid` | dialThread（静态初始化） | nano_msg_thread（getStatus） | 无锁（写一次读多次，可接受） |
| `g_last_call_status` | SDK 回调 | dialThread | volatile int（原子写）|
| `isInterfaceSet` | dialThread（setInterfaceName） | main 线程（cv.wait_for） | std::mutex + cv |
| `NanoReqHandler::netif_name` | dialThread（setInterfaceName） | nano_msg_thread（processModemRequests） | std::mutex（间接保护）|
| `TrafficMonitor::interface` | dialThread（setInterfaceName） | monitorOverLimit（detach 线程） | std::mutex（间接）|
| `TrafficMonitor::stopMonitor` | main 线程（stopMonitorCCINetStats） | monitorOverLimit 线程 | std::atomic<bool> |
| `g_callid`（data_call.c） | data_call_status_ind_cb / stop_call | start_call / dial_loop | 无锁（单线程调用链）|

### 25.2 内存管理模式汇总

| 分配方式 | 使用场景 | 释放责任 |
|---------|---------|---------|
| `std::unique_ptr<dial_mng_t>` | EC200ADialer::p_dial_mng | RAII（析构自动释放）|
| `cJSON_CreateObject` | 每次 getStatus/pub | 调用方 cJSON_Delete |
| `cJSON_Print` | JSON 序列化 | 调用方 free() |
| `malloc/realloc`（executeATCommand）| C 层命令执行 | 调用方 free() |
| `calloc`（cc_deque 元素）| 运营商名称 | cc_deque_free_all |
| `new TrafficMonitor` | C 接口包装 | delete_monitor |
| `new NanoReqHandler` | startNanoReqHandler | stopNanoReqHandler |
| `apn_load_from_json` 返回值 | APN 加载 | 调用方 free()（但实际未释放！内存泄漏）|

### 25.3 已知 Bug 汇总

| 文件 | 行为 | 严重度 |
|-----|------|-------|
| eg25/at/at.c | `iLen = sizeof(atCmd)` 取指针大小而非字符串长度 | 中（后续 sprintf 覆盖）|
| eg25/at/at.c | `timeout.tv_usec = timeout_ms % 1000` 单位错误（应 ×1000）| 中（超时比预期短）|
| dialer_imx6ull.cpp | `cc_deque_free_all` 中 `deque = NULL` 无效 | 低（悬空指针但不访问）|
| nanomsg_process_wraper.cpp | `get_interface` 返回悬空 `c_str()` | 中（UB，实际可能不崩溃）|
| nanomsg_process.cpp | `getStatus` 中每次创建 TrafficMonitor | 中（性能问题，SQLite 每次查询）|
| nanomsg_process.cpp | ICCID 不可用时填充 IMSI | 中（数据混淆）|
| nanomsg_process.cpp | LAC/CellID 硬编码默认值 | 中（误导监控系统）|
| ec200a/apn/apn.c | JSON 对象 `json_object_put(obj)` 注释导致内存泄漏 | 低（常驻程序，内存泄漏有限）|
| ec200a/apn/apn.c | `fread` 返回值与 0 比较而非与 `len` 比较 | 低（正常情况下不触发）|
| dialer_eg25.c | `signalLow`/`signalHigh` 等变量未初始化 | 中（可能触发误告警）|
| dialer.hpp（Imx6uDialer） | `adapterAT00` 无超时，可能永远阻塞 | 高（modem 故障时死锁）|

---

*本文档完整逐行分析 modem_mng 仓库 v1.30 的全部核心源码文件，包含每一行代码的功能说明、设计意图、实现细节和已知问题。*
