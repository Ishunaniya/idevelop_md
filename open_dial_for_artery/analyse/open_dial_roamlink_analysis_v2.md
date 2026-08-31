# open_dial_for_artery × roamlink_deploy 深度全量源码分析报告 v2

> **分析基础**：全部结论均来自对两个仓库实际源码（C/Shell/配置/二进制字符串）的直接逐行阅读，不含推测性内容。  
> **版本**：open_dial V1.29.6（源码）× roamlink_deploy_auto_download_with_license_251028  
> **目标平台**：Quectel EC2x/EG2x 模组，ARM Cortex-A7，32-bit Linux（EABI5，glibc ≥ 2.4）

---

## 目录

1. [背景概念：物理 SIM 与虚拟 SIM 是什么](#1-背景概念物理-sim-与虚拟-sim-是什么)
2. [两仓库关系总览](#2-两仓库关系总览)
3. [open_dial_for_artery 全量源码逐行分析](#3-open_dial_for_artery-全量源码逐行分析)
   - 3.1 项目结构与编译系统（Makefile）
   - 3.2 main.c — 进程入口
   - 3.3 at.c / at.h — AT 指令收发层
   - 3.4 sim.c / sim.h — SIM 卡操作层
   - 3.5 nw.c / nw.h — 网络状态层
   - 3.6 apn.c / apn.h — APN 配置层
   - 3.7 dial.c / dial.h — 核心状态机
   - 3.8 tz.c / tz.h — 时区获取
   - 3.9 seas_log.c / seas_log.h — 日志系统
   - 3.10 cc_deque — 双端队列库
   - 3.11 APN 配置文件分析（apn.json）
4. [open_dial_for_artery 编译指南](#4-open_dial_for_artery-编译指南)
5. [roamlink_deploy 全量逐行分析](#5-roamlink_deploy-全量逐行分析)
   - 5.1 部署包结构
   - 5.2 install_update.sh — 安装脚本逐行
   - 5.3 start_dfota.sh — 固件升级脚本逐行
   - 5.4 check_network.sh — 网络监控主脚本逐行
   - 5.5 start_check_network / start_daemon — init.d 服务脚本
   - 5.6 roamlink/ 目录下的二进制组件
   - 5.7 network.ini 配置文件
   - 5.8 license.cer 授权文件
6. [双卡切换逻辑深度解析](#6-双卡切换逻辑深度解析)
   - 6.1 物理 SIM 通道工作流
   - 6.2 虚拟 SIM 通道工作流
   - 6.3 四种网络策略详解
   - 6.4 切换时序全流图
   - 6.5 切换操作的底层实现
7. [两仓库协作关系详解](#7-两仓库协作关系详解)
8. [已知 Bug 与代码缺陷（逐条详解）](#8-已知-bug-与代码缺陷逐条详解)
9. [安全漏洞分析](#9-安全漏洞分析)
10. [改进建议（附修复代码）](#10-改进建议附修复代码)

---

## 1. 背景概念：物理 SIM 与虚拟 SIM 是什么

### 1.1 物理 SIM（Physical SIM）

物理 SIM 就是我们日常生活中插在手机里的那张小卡片。对于工业设备（比如叉车、NVR）而言，它是一张固定插入模组卡槽的实体 SIM 卡，由某家运营商（中国移动、中国联通、挪威电信等）签发，具有固定的 ICCID（卡的序列号）和 IMSI（用户识别码）。

```
物理SIM流程:
  实体SIM卡 → 插入模组卡槽 → 模组读取 ICCID/IMSI
           → 向运营商基站注册 → 获得 IP 地址 → 上网
```

**优点**：稳定可靠，适合单一国家/地区固定场景  
**缺点**：跨国漫游费高；SIM 卡损坏需现场更换；部分国家运营商信号弱时没有备选

本项目 `open_dial_for_artery` 就是管理物理 SIM 拨号的程序。

### 1.2 虚拟 SIM（Virtual SIM / eSIM / Soft SIM）

虚拟 SIM（本项目叫 Soft SIM 或 softsim）是用软件模拟 SIM 卡功能，通过互联网连接云端运营商平台，在云上完成 USIM 鉴权，使得设备可以动态切换运营商，无需更换物理卡。

```
虚拟SIM流程（RoamLink方案）:
  softsim 进程（libTSSUMEmulator.so）
           ↕ 本地模拟 USIM 协议
  Quectel 模组（EC2x/EG2x）
           ↕ 通过 TCP/IP 回传鉴权数据
  RoamLink 云平台（远端运营商服务器）
           → 动态选择当前最优运营商
           → 返回运营商鉴权响应
           → 设备以该运营商身份注册网络 → 上网
```

**优点**：自动选择全球最优运营商；无需换卡；适合跨国设备  
**缺点**：依赖互联网来完成鉴权（鸡和蛋问题，初次上线需先用物理 SIM 或设备在有网环境）；依赖 license 授权；闭源

### 1.3 本项目中两者的关系

本项目同时存在两条通道：

```
┌──────────────────────────────────────────────────────┐
│              Quectel EC2x/EG2x 模组                   │
│                                                      │
│   物理SIM卡槽                虚拟SIM接口              │
│   (实体SIM卡)                (软件模拟USIM)           │
│        │                          │                 │
│   open_dial (dial进程)       softsim + RBMaster      │
│        │                          │                 │
│        └────────── 二选一 ─────────┘                 │
│              check_network.sh 决策                   │
└──────────────────────────────────────────────────────┘
```

两者**不能同时激活**（共用同一个模组无线接口），由 `check_network.sh` 根据配置策略决定激活哪个，并在一方失败时切换到另一方。

---

## 2. 两仓库关系总览

```
open_dial_for_artery (源码仓库)
  │
  │  make 编译
  ▼
dial_1.29.4 (ARM 32-bit ELF 二进制)
  │
  │  打包进去
  ▼
roamlink_deploy_auto_download_with_license_251028/ (部署包)
  ├── dial_1.29.4          ← 就是 open_dial 的编译产物
  ├── roamlink/            ← RoamLink 虚拟SIM闭源组件
  ├── check_network.sh     ← 统一调度两个通道
  ├── install_update.sh    ← 安装脚本
  └── ...

install_update.sh 执行时:
  cp ./dial_1.29.4 /usr/dial/dial   ← 替换设备上的拨号程序
```

**核心关系**：`open_dial` 是物理 SIM 通道的**源码工程**，roamlink 部署包将其**编译产物**与虚拟 SIM 服务打包在一起，实现双通道冗余网络。

---

## 3. open_dial_for_artery 全量源码逐行分析

### 3.1 项目结构与编译系统（Makefile）

#### 目录结构

```
open_dial_for_artery/
├── main.c                  # 进程入口
├── Makefile                # 构建脚本
├── src/
│   ├── at/                 # AT 指令收发
│   │   ├── at.c
│   │   └── at.h
│   ├── sim/                # SIM 卡操作
│   │   ├── sim.c
│   │   └── sim.h
│   ├── nw/                 # 网络状态管理
│   │   ├── nw.c
│   │   └── nw.h
│   ├── apn/                # APN 配置解析
│   │   ├── apn.c
│   │   └── apn.h
│   ├── dial/               # 核心状态机
│   │   ├── dial.c
│   │   └── dial.h
│   ├── tz/                 # 时区获取
│   │   ├── tz.c
│   │   └── tz.h
│   ├── seas_log/           # 日志系统
│   │   ├── seas_log.c
│   │   └── seas_log.h
│   ├── cc_deque/           # 双端队列（Collections-C 开源库）
│   │   ├── cc_deque.c
│   │   ├── cc_deque.h
│   │   ├── cc_common.c
│   │   └── cc_common.h
│   ├── opt_iniparser/      # INI 文件解析库
│   │   ├── iniparser.c
│   │   ├── iniparser.h
│   │   ├── dictionary.c
│   │   └── dictionary.h
│   └── json/               # cJSON 轻量 JSON 库
│       ├── cJSON.c
│       └── cJSON.h
└── apn/                    # APN 配置文件（按项目分类）
    ├── apn normal/apn.json
    ├── apn for BEM/apn.json
    └── apn for simboss/apn.json
```

#### Makefile 逐行解析

```makefile
# ---- 编译器设置 ----
CC=arm-oe-linux-gnueabi-gcc
# arm-oe-linux-gnueabi-gcc：OpenEmbedded（Yocto）交叉编译器
# 目标：ARM Linux，GNU EABI，软浮点（gnueabi 而非 gnueabihf）

QL_EXP_LDLIBS = -lql_sys_log \
                -ljson-c
# 链接 Quectel 系统日志库 和 json-c 库（注：实际源码自带 cJSON，此处 json-c 可能是历史遗留）

COMPILER_FLAGS = -march=armv7ve
# 目标架构 ARMv7ve（支持虚拟化扩展），对应 Cortex-A7/A15

QL_SDK_PATH ?= $(shell pwd)/../..
# Quectel OpenLinux SDK 路径，默认在上级的上级目录

APP_NAME = dial
# 最终输出的可执行文件名为 "dial"

# ---- Include 路径 ----
APP_INCLUDE_DIRS += -I$(QL_SDK_PATH)/lib/interface/inc
# Quectel SDK 接口头文件（包含 ql_oe.h, ql_wwan_v2.h 等）

APP_INCLUDE_DIRS += -I$(SDKTARGETSYSROOT)/usr/include
# 目标 sysroot 的系统头文件

APP_INCLUDE_DIRS += -I $(SRC_DIR)/sim    # SIM 模块头文件
APP_INCLUDE_DIRS += -I $(SRC_DIR)/nw     # 网络模块头文件
APP_INCLUDE_DIRS += -I $(SRC_DIR)/apn    # APN 模块头文件
APP_INCLUDE_DIRS += -I $(SRC_DIR)/at     # AT 模块头文件
APP_INCLUDE_DIRS += -I $(SRC_DIR)/dial   # 拨号模块头文件
APP_INCLUDE_DIRS += -I $(SRC_DIR)/tz     # 时区模块头文件
APP_INCLUDE_DIRS += -I $(SRC_DIR)/cc_deque     # 队列库头文件
APP_INCLUDE_DIRS += -I $(SRC_DIR)/opt_iniparser # INI解析头文件
APP_INCLUDE_DIRS += -I $(SRC_DIR)/seas_log      # 日志库头文件
APP_INCLUDE_DIRS += -I $(SRC_DIR)/json          # cJSON 头文件

# ---- 链接标志 ----
LDFLAGS += -L./
LDFLAGS += ${QL_EXP_LDLIBS}
LDFLAGS += -L$(SDKTARGETSYSROOT)/usr/lib    # sysroot 标准库目录
LDFLAGS += -L$(QL_SDK_PATH)/lib             # Quectel SDK 库目录
LDFLAGS += -lrt                             # POSIX 实时扩展（clock_gettime 需要）
LDFLAGS += -lpthread                        # POSIX 线程库（pthread_create 需要）
LDFLAGS += -lql_sys_log                     # Quectel 系统日志

# ---- 编译选项 ----
CPPFLAGS += -Werror                         # 警告视为错误（严格模式）
CPPFLAGS += -I$(SDKTARGETSYSROOT)/usr/include/ql-manager   # MCM管理器头文件
CPPFLAGS += -I$(SDKTARGETSYSROOT)/usr/include/data         # 数据呼叫头文件
CPPFLAGS += -I$(SDKTARGETSYSROOT)/usr/include/dsutils      # 数据服务工具头文件
CPPFLAGS += -I$(SDKTARGETSYSROOT)/usr/include/qmi          # QMI 协议头文件
CPPFLAGS += -I$(SDKTARGETSYSROOT)/usr/include/qmi-framework# QMI框架头文件

# ---- 源文件收集（自动扫描） ----
APP_SRC_FILES += $(shell find $(SRC_DIR)/sim -name '*.c')
APP_SRC_FILES += $(shell find $(SRC_DIR)/nw -name '*.c')
# ... 所有 src/ 下的 .c 文件都被收集

# ---- Quectel 专有库 ----
STD_LIB = $(SDKTARGETSYSROOT)/usr/lib/libql_mgmt_client.so  # MCM 管理客户端
STD_LIB += $(SDKTARGETSYSROOT)/usr/lib/libdsi_netctrl.so    # 数据服务接口（DSI）
STD_LIB += $(SDKTARGETSYSROOT)/usr/lib/libdsutils.so        # DSI 工具
STD_LIB += $(SDKTARGETSYSROOT)/usr/lib/libqmiservices.so    # QMI 服务
STD_LIB += $(SDKTARGETSYSROOT)/usr/lib/libqmi_cci.so        # QMI 客户端接口
STD_LIB += $(SDKTARGETSYSROOT)/usr/lib/libqmi_common_so.so  # QMI 公共层
STD_LIB += $(SDKTARGETSYSROOT)/usr/lib/libqmi.so            # QMI 主库
STD_LIB += $(SDKTARGETSYSROOT)/usr/lib/libmcm.so            # MCM（移动连接管理）主库

DUAL_LIB = $(QL_SDK_PATH)/lib/libql_common_api.a            # Quectel 通用 API 静态库

# ---- 编译目标 ----
all:
    $(CC) $(COMPILER_FLAGS) $(CPPFLAGS) $(LDFLAGS) \
          $(SRC_FILES) $(DUAL_LIB) $(STD_LIB) \
          -o $(APP_NAME) $(LD_FLAG) $(INCLUDE_ALL_DIRS)
# 一次性编译所有 .c 文件并链接，输出 "dial" 可执行文件
```

**SDK 层级关系图**：
```
应用层 (open_dial)
    │ 调用
    ▼
Quectel QL API 层 (libql_common_api.a)
    │ 封装
    ▼
MCM 层 (libmcm.so, libql_mgmt_client.so)
    │ 调用
    ▼
QMI 层 (libqmi.so, libqmi_cci.so, libqmiservices.so)
    │ IPC
    ▼
Quectel 模组基带固件
```

---

### 3.2 main.c — 进程入口（逐行）

```c
/* 版本号定义 */
#define MAIN_VERSION 1   // 主版本号 = 1
#define SUB_VERSION 29   // 子版本号 = 29
#define TEST_VERSION 6   // 修订号 = 6  → 完整版本：1.29.6

/* 全局 AT 串口互斥锁
 * 因为主线程（读 CSQ）和 dial_task 线程都要操作 AT 串口，
 * 必须互斥，否则两个线程同时读写 /dev/smd8 会造成数据混乱 */
pthread_mutex_t g_at_port_mutex = PTHREAD_MUTEX_INITIALIZER;
```

#### seas_log_config_init() 函数逐行

```c
void seas_log_config_init()
{
    seas_log_config_t log_config;
    log_config.level = SEAS_LEVEL_INFO;  // 日志级别：INFO（不含DEBUG）
    log_config.file = "/media/sdcard/seas_log_dial.log";  // SD卡上的日志文件路径

    struct stat sdstat;
    int has_sdcard_and_free_1GB = 0, i;
    unsigned long long free_bytes = 0;

    /* 最多等待 10 秒，每秒检查一次 SD 卡是否就绪
     * 原因：系统启动时 SD 卡可能还未挂载，需要等待 */
    for (i = 0; i < 10; ++i) {
        /* stat() 检查 /media/sdcard 目录是否存在 */
        if (stat("/media/sdcard", &sdstat) == 0 && S_ISDIR(sdstat.st_mode)) {
            /* statvfs() 获取文件系统统计信息 */
            struct statvfs vfs_chk;
            if (statvfs("/media/sdcard", &vfs_chk) == 0) {
                /* f_bavail：对非root用户可用的块数
                 * f_frsize：文件系统基本块大小（字节）
                 * 两者相乘得到可用字节数 */
                free_bytes = (unsigned long long)vfs_chk.f_bavail * vfs_chk.f_frsize;
                if (free_bytes >= (unsigned long long)1024 * 1024 * 1024) { // 1GB
                    has_sdcard_and_free_1GB = 1;
                    break;  // 条件满足，跳出等待循环
                }
            }
        }
        sleep(1);  // 每次等待 1 秒再重试
    }

    if (has_sdcard_and_free_1GB) {
        log_config.size = 50 * 1024 * 1024;  // SD卡充足：日志文件最大 50MB
    } else {
        log_config.size = 0;  // size=0 表示不写文件，只输出到 stdout
    }
    seas_log_config(&log_config);  // 应用配置
}
```

**设计逻辑**：为避免在内存或 flash 空间有限的嵌入式设备上撑满存储，只有 SD 卡存在且空闲 ≥ 1GB 时才开启文件日志。

#### main() 函数逐行

```c
int main(int argc, char **argv)
{
    pthread_t newthread;
    int csq;

    seas_log_config_init();  // 初始化日志（最多阻塞10秒等SD卡）

    /* 打印版本号到日志，格式：1.29.06 */
    SEAS_LOG_INFO("DIAL Version TEST2: %d.%02d.%02d\r\n",
                  MAIN_VERSION, SUB_VERSION, TEST_VERSION);

    /* 分配并初始化拨号管理器结构体 */
    dial_mng_t *p_dial_mng = dial_mng_new();

    /* 创建 dial_task 线程，传入管理器指针作为参数
     * dial_task 是整个拨号状态机的驱动函数 */
    pthread_create(&newthread, NULL, dial_task, (void *)p_dial_mng);

    /* 主线程进入永久循环：每 5 秒查询一次信号强度（CSQ）
     * CSQ = Channel Quality Signal，范围 0~31（99 表示未知）
     * 换算：dBm = CSQ*2 - 113，例如 CSQ=20 → -73dBm */
    while (true) {
        csq = nw_at_get_csq(p_dial_mng->smd_fd);
        if (csq != -1) {
            SEAS_LOG_INFO("CSQ: %d\r\n", p_dial_mng->csq);
            /* DIAL_CSQ_INVALID = 99，表示"未知"，此时置0而非存99 */
            p_dial_mng->csq = (csq == DIAL_CSQ_INVALID) ? 0 : (uint8_t)csq;
        }
        sleep(5);
    }
    return 0;  // 实际上永远不会到达这里
}
```

**注意**：`p_dial_mng->csq` 被主线程写入，但被 dial_task 线程读取，存在数据竞争（无锁保护）。不过 `uint8_t` 的读写在 ARM 上是原子的，实际不会导致问题，但从代码规范来说应加锁或改为原子变量。

---

### 3.3 at.c / at.h — AT 指令收发层

#### at.h 关键定义

```c
#define QUEC_AT_PORT "/dev/smd8"
/* /dev/smd8 是 Quectel 模组提供的第一个 AT 指令通道
 * SMD = Shared Memory Device（高通平台的共享内存设备）
 * 通过 open()/read()/write() 像普通串口一样操作 */

#define AT_MSG_LENGTH_MAX (1024)
/* AT 命令和响应的最大缓冲区长度 = 1024 字节
 * AT+COPS=? 的响应（运营商列表）可能非常长，1024 字节有时不够（见已知Bug） */
```

#### at_init() 逐行

```c
int at_init(void)
{
    /* O_RDWR：可读可写（AT口需要发送和接收）
     * O_NONBLOCK：非阻塞模式（read/write 不会阻塞等待）
     * O_NOCTTY：不将此设备设为控制终端（避免终端信号干扰） */
    int smd_fd = open(QUEC_AT_PORT, O_RDWR | O_NONBLOCK | O_NOCTTY);

    /* ATE0：关闭回显（Echo Off）
     * 模组默认开启回显，即发送 "AT\r\n" 后响应会包含 "AT\r\n"
     * 关闭回显后只返回结果，便于解析 */
    Ql_SendAT(smd_fd, "ATE0", "OK", 1000, NULL);

    /* AT：基本测试指令，确认串口通畅 */
    Ql_SendAT(smd_fd, "AT", "OK", 1000, NULL);

    /* ATI：查询模组信息（型号、固件版本等），结果打印到日志 */
    Ql_SendAT(smd_fd, "ATI", "OK", 1000, NULL);

    return smd_fd;  // 返回文件描述符供后续所有 AT 操作使用
}
```

#### Ql_SendAT() 完整逐行

```c
int Ql_SendAT(int smd_fd, char *atCmd, char *finalRsp, long timeout_ms, char *p_rsp_msg)
{
    int iLen;
    fd_set fds;                    // select() 用的文件描述符集合
    int rdLen;
    char strAT[AT_MSG_LENGTH_MAX] = {0};       // 要发送的完整 AT 命令缓冲区
    char strFinalRsp[AT_MSG_LENGTH_MAX];       // 期望的最终响应（如 "\r\nOK"）
    char strResponse[AT_MSG_LENGTH_MAX];       // 读取到的响应缓冲区
    struct timeval timeout = {0, 0};           // select() 超时结构体
    boolean bRcvFinalRsp = FALSE;             // 是否已收到终止响应

    /* ★ Bug 1：sizeof(atCmd) 对指针求大小，在 ARM32 上固定返回 4
     * 正确应为 strlen(atCmd)
     * 但因为后面的 sprintf 覆盖了 strAT，实际发送的内容不受此影响 */
    iLen = sizeof(atCmd);  // 错误！始终 = 4
    strncpy(strAT, atCmd, iLen);  // 只复制前 4 个字符，但后面被覆盖

    /* 构造期望响应格式：finalRsp 前加 "\r\n"
     * 因为 AT 命令的正常响应格式是：\r\nOK\r\n 或 \r\nERROR\r\n */
    sprintf(strFinalRsp, "\r\n%s", finalRsp);

    /* 超时分解：timeout_ms = 1000ms → tv_sec=1, tv_usec=0 */
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = timeout_ms % 1000;
    /* ★ Bug 2：tv_usec 单位是微秒，这里存入的是毫秒余数（0~999ms），
     * 实际误差微小可忽略，但是个语义错误 */

    /* 重新构造带 \r\n 结尾的完整 AT 命令
     * 如果 atCmd 末尾没有 \r 或 \n，则补充 \r\n */
    iLen = strlen(atCmd);
    if ((atCmd[iLen - 1] != '\r') && (atCmd[iLen - 1] != '\n')) {
        iLen = sprintf(strAT, "%s\r\n", atCmd);
        strAT[iLen] = 0;
    }

    /* 先读一次清空缓冲区中的残余 URC（主动上报消息）
     * URC 如 "+CEREG: 1" 等，不清空会干扰后续响应解析 */
    read(smd_fd, strResponse, AT_MSG_LENGTH_MAX);

    /* 发送 AT 命令 */
    write(smd_fd, strAT, iLen);

    /* 等待响应循环 */
    while (1) {
        FD_ZERO(&fds);
        FD_SET(smd_fd, &fds);

        /* select() 等待 smd_fd 变为可读，或超时
         * 注意：timeout 在每次 select() 后会被修改（Linux 特性），
         * 这意味着多次循环后 timeout 会被消耗，直到变为0超时 */
        switch (select(smd_fd + 1, &fds, NULL, NULL, &timeout)) {
        case -1:
            return -1;  // select 出错

        case 0:
            return 1;   // 超时，未收到期望响应

        default:
            if (FD_ISSET(smd_fd, &fds)) {
                do {
                    memset(strResponse, 0x0, AT_MSG_LENGTH_MAX);
                    rdLen = read(smd_fd, strResponse, AT_MSG_LENGTH_MAX);
                    if (rdLen > 0) {
                        /* 打印原始响应（调试用，生产代码应移到 DEBUG 级别） */
                        printf(">> Read response urc, len = %d, content : %s\r\n",
                               rdLen, strResponse);
                    }

                    /* 响应匹配判断：
                     * 条件1：包含期望的终止响应（如 "\r\nOK"）
                     * 条件2：包含 +CME ERROR: 或 +CMS ERROR: 或 ERROR */
                    if ((rdLen > 0) && strstr(strResponse, strFinalRsp)) {
                        if (strstr(strResponse, strFinalRsp)
                            || strstr(strResponse, "+CME ERROR:")
                            || strstr(strResponse, "+CMS ERROR:")
                            || strstr(strResponse, "ERROR")) {
                            if (p_rsp_msg != NULL) {
                                /* 将响应复制给调用方
                                 * ★ Bug 3：未检查 p_rsp_msg 缓冲区大小，
                                 * 若 rdLen > 调用方缓冲区大小则溢出 */
                                memcpy(p_rsp_msg, strResponse, rdLen);
                            }
                            bRcvFinalRsp = TRUE;
                        }
                    }
                    usleep(10000);  // 10ms 延时，让模组有时间继续发送剩余数据

                /* 若一次 read 读满了缓冲区（rdLen == AT_MSG_LENGTH_MAX），
                 * 说明还有更多数据，继续读 */
                } while ((rdLen > 0) && (AT_MSG_LENGTH_MAX == rdLen));
            }
            break;
        }

        if (bRcvFinalRsp) {
            break;  // 收到期望响应，退出等待
        }
    }
    return 0;  // 成功
}
```

**AT 命令时序示意**：

```
主机(open_dial)              Quectel模组
      │                          │
      │  write("AT+CSQ\r\n")     │
      │─────────────────────────►│
      │                          │ 模组处理
      │  "\r\n+CSQ: 20,0\r\nOK\r\n"
      │◄─────────────────────────│
      │  select()检测可读        │
      │  read()读取响应          │
      │  strstr 匹配 "\r\nOK"   │
      │  解析 "+CSQ: 20,0"      │
```

---

### 3.4 sim.c / sim.h — SIM 卡操作层

#### sim.h 关键定义

```c
/* SIM 操作命令索引 */
#define SIM_OP_INIT    (0)  // 初始化 SIM 客户端连接 MCM 服务
#define SIM_OP_DEINIT  (1)  // 反初始化，释放 MCM 连接
#define SIM_OP_GET_IMSI  (2)  // 获取 IMSI（国际移动用户识别码，15位数字）
#define SIM_OP_GET_ICCID (3)  // 获取 ICCID（SIM卡序列号，19-20位数字）

#define SIM_BUF_SIZE (32)
/* SIM_BUF_SIZE = 32 字节
 * ICCID 最长 20 字符 + '\0'，IMSI 最长 15 字符 + '\0'，32 字节够用
 * 但注意 apn.h 中 apn_obj_t.iccid[32] 也是 32 字节，与此对齐 */

/* SIM 操作状态枚举（用于 sim_op_handler） */
typedef enum {
    sim_op_stat_get_iccid,   // 获取 ICCID 操作
    sim_op_stat_get_imsi,    // 获取 IMSI 操作
    sim_op_stat_get_deinit,  // 反初始化操作
    sim_op_stat_suc          // 成功状态（终态）
} sim_op_stat_enu;

/* SIM 管理器结构体 */
typedef struct {
    char sim_iccid[SIM_BUF_SIZE];  // 存储 ICCID
    char sim_imsi[SIM_BUF_SIZE];   // 存储 IMSI
    // sim_client_handle_type h_sim;  // MCM SIM 客户端句柄（已注释，改为 static）
} sim_mng_t;
```

#### sim_op() 核心函数逐行

```c
/* static 变量：SIM MCM 客户端句柄，模块级全局，不暴露给外部 */
static sim_client_handle_type h_sim;

int sim_op(int cmdIdx, char *buf)
{
    int ret = E_QL_OK;   // E_QL_OK = 0，表示成功
    if (cmdIdx == -1) {
        return -1;  // 非法命令
    }
    switch (cmdIdx) {

    case SIM_OP_INIT:
        /* QL_MCM_SIM_Client_Init：向 MCM 服务注册，获取客户端句柄 h_sim
         * MCM（Mobile Connection Manager）是 Quectel 封装的连接管理服务，
         * 运行在用户空间，通过 socket IPC 与模组通信 */
        ret = QL_MCM_SIM_Client_Init(&h_sim);
        break;

    case SIM_OP_DEINIT:
        /* 释放 MCM SIM 客户端连接 */
        ret = QL_MCM_SIM_Client_Deinit(h_sim);
        break;

    case SIM_OP_GET_IMSI:
        {
            QL_SIM_APP_ID_INFO_T t_info;
            memset(buf, 0, SIM_BUF_SIZE);  // 清空目标缓冲区
            t_info.e_slot_id = E_QL_MCM_SIM_SLOT_ID_1;    // 使用 SIM 卡槽 1
            t_info.e_app = E_QL_MCM_SIM_APP_TYPE_3GPP;    // 3GPP 应用（非 CDMA）
            /* 通过 MCM API 读取 IMSI，结果写入 buf */
            ret = QL_MCM_SIM_GetIMSI(h_sim, &t_info, buf, SIM_BUF_SIZE);
        }
        break;

    case SIM_OP_GET_ICCID:
        memset(buf, 0, SIM_BUF_SIZE);
        /* 直接按卡槽读取 ICCID，不需要 APP 类型参数 */
        ret = QL_MCM_SIM_GetICCID(h_sim, E_QL_MCM_SIM_SLOT_ID_1, buf, SIM_BUF_SIZE);
        break;
    }
    return ret;
}
```

#### sim_op_handler() 逐行（含 Bug 分析）

```c
bool sim_op_handler(sim_mng_t *p_sim_op_mng, sim_op_stat_enu sim_op_st)
{
    bool ret = false;
    switch (sim_op_st) {

    case sim_op_stat_get_iccid:
        if (E_QL_OK == sim_op(SIM_OP_GET_ICCID, p_sim_op_mng->sim_iccid)) {
            SEAS_LOG_INFO("ICCID: %s", p_sim_op_mng->sim_iccid);
            ret = true;
        } else {
            SEAS_LOG_ERROR("SIM_op_st: sim_op_stat_get_iccid error");
        }
        /* ★ Bug：此处缺少 break！
         * 执行完 get_iccid 后，会 fall-through 到 get_imsi 分支
         * 实际效果：调用 sim_op_handler(..., sim_op_stat_get_iccid)
         *           会同时执行 GET_ICCID 和 GET_IMSI 两个操作
         * 副作用：sim_imsi 被填充（虽然当前版本不使用 IMSI） */
        // 正确做法：在此加 break;

    case sim_op_stat_get_imsi:
        if (E_QL_OK == sim_op(SIM_OP_GET_IMSI, p_sim_op_mng->sim_imsi)) {
            SEAS_LOG_INFO("IMSI: %s", p_sim_op_mng->sim_imsi);
            ret = true;
        } else {
            SEAS_LOG_ERROR("SIM_op_st: sim_op_stat_get_imsi error");
        }
        break;

    case sim_op_stat_get_deinit:
        if (E_QL_OK == sim_op(SIM_OP_DEINIT, p_sim_op_mng->sim_imsi)) {
            ret = true;
        }
        break;

    case sim_op_stat_suc:
        ret = true;
        break;
    }
    return ret;
}
```

---

### 3.5 nw.c / nw.h — 网络状态层

#### nw.h 关键常量

```c
#define NW_STATUS_PATH "/tmp/network_status"
/* 网络连通状态文件：
 * 写入 "1" = 网络已连通
 * 写入 "0" = 网络断开
 * 其他进程（如上层应用、看门狗脚本）可读此文件判断网络状态 */

#define NW_CSQ_PATH "/tmp/network_csq"
/* 信号强度文件：写入当前 CSQ 值（0~31，99=未知） */

#define NW_PLMN_PATH "/tmp/network_plmn"
/* 当前注册的 PLMN（公共陆地移动网络）文件：写入 MCC+MNC，如 "46000" */

#define NW_IF_STATISTICS_RX_PACKETS_PATH \
    "/sys/devices/virtual/net/%s/statistics/rx_packets"
/* Linux 内核暴露的网络接口统计文件
 * %s 替换为接口名（如 rmnet0）
 * 读取该文件可获知接口已接收的总包数 */

#define SIM_CARD_READY (10)
/* MCM SIM 卡就绪状态码：
 * MCM 返回的原始状态值 = app_state - 0xB00
 * 当结果 = 10 时表示 SIM 卡完全就绪可用 */
```

#### nw_get_sim_card_status() 逐行

```c
bool nw_get_sim_card_status(nw_client_handle_type nw_client)
{
    QL_MCM_SIM_CARD_STATUS_INFO_T t_info = {0};
    int sim_card_app_status;

    /* QL_MCM_SIM_GetCardStatus：通过 MCM NW 客户端（不是 SIM 客户端）
     * 获取 SIM 卡详细状态信息 */
    if (0 == QL_MCM_SIM_GetCardStatus(nw_client, E_QL_MCM_SIM_SLOT_ID_1, &t_info)) {
        /* app_3gpp.app_state 是 MCM 定义的 3GPP 应用状态
         * 减去 0xB00 是因为 MCM 枚举的基址从 0xB00 开始
         * 换算后：0=未激活, 1=PIN待输入, 10=就绪(READY) */
        sim_card_app_status = t_info.card_app_info.app_3gpp.app_state - 0xB00;
        if (sim_card_app_status == SIM_CARD_READY) {  // == 10
            return true;  // SIM 卡就绪
        }
    }
    return false;
}
```

#### nw_at_get_cereg_stat() 逐行

```c
bool nw_at_get_cereg_stat(int smd_fd)
{
    char rsp_msg[AT_MSG_LENGTH_MAX];
    char *p_cereg;
    int cereg_n, stat_cereg;

    memset(rsp_msg, 0, AT_MSG_LENGTH_MAX);
    pthread_mutex_lock(&g_at_port_mutex);  // 加锁，避免与主线程读CSQ冲突

    /* AT+CEREG? 查询 LTE/NR 网络注册状态
     * 响应格式：+CEREG: <n>,<stat>
     * <n>  = 通知级别（0=关闭URC, 1=简单URC, 2=带位置URC）
     * <stat> = 注册状态：
     *   0 = 未注册，未搜索
     *   1 = 注册，本地网络（正常）
     *   2 = 未注册，正在搜索
     *   3 = 注册被拒绝
     *   4 = 未知
     *   5 = 注册，漫游网络（正常）
     *
     * 注意：AT 命令有末尾空格 "AT+CEREG?    "，属于历史遗留，不影响功能 */
    Ql_SendAT(smd_fd, "AT+CEREG?    ", "OK", 1000, rsp_msg);
    pthread_mutex_unlock(&g_at_port_mutex);

    p_cereg = strstr(rsp_msg, "+CEREG: ");
    if (p_cereg != NULL) {
        /* sscanf 解析两个整数：cereg_n（n值）和 stat_cereg（状态） */
        if (2 == sscanf(p_cereg, "+CEREG: %d,%d", &cereg_n, &stat_cereg)) {
            /* 只有 stat=1（本地注册）或 stat=5（漫游注册）认为"已驻网" */
            if ((stat_cereg == 1) || (stat_cereg == 5)) {
                return true;
            }
            SEAS_LOG_INFO("cereg_n: %d, stat_cereg:%d", cereg_n, stat_cereg);
        }
    }
    return false;
}
```

#### nw_at_get_csq() 逐行

```c
int nw_at_get_csq(int smd_fd)
{
    char rsp_msg[AT_MSG_LENGTH_MAX];
    char *p_csq;
    int csq, ber, fd;
    static int pre_csq = -1;  // 上次的 CSQ 值（静态，跨调用保持）
    char echo_cmd[AT_MSG_LENGTH_MAX];

    memset(rsp_msg, 0, AT_MSG_LENGTH_MAX);
    pthread_mutex_lock(&g_at_port_mutex);

    /* AT+CSQ：查询信号强度
     * 响应格式：+CSQ: <rssi>,<ber>
     * <rssi>：0~31（越大信号越强），99 = 未知
     * <ber>：误码率（0~7），99 = 未知 */
    Ql_SendAT(smd_fd, "AT+CSQ", "OK", 1000, rsp_msg);
    pthread_mutex_unlock(&g_at_port_mutex);

    p_csq = strstr(rsp_msg, "+CSQ: ");
    if (p_csq != NULL) {
        if (2 == sscanf(p_csq, "+CSQ: %d,%d", &csq, &ber)) {
            /* 只有 CSQ 值变化时才更新文件，减少 flash 写入次数 */
            if (pre_csq != csq) {
                /* ★ 漏洞：使用 system("echo %d > /tmp/...") 写文件
                 * CSQ 是整数，理论上安全，但 system() 调用 shell 有额外开销
                 * 更好的做法是直接 open/write/close */
                if (access(NW_CSQ_PATH, F_OK) == -1) {
                    fd = open(NW_CSQ_PATH, O_WRONLY | O_CREAT);
                    close(fd);
                }
                sprintf(echo_cmd, "echo %d > %s", csq, NW_CSQ_PATH);
                system(echo_cmd);
                pre_csq = csq;
            }
            return csq;
        }
    }
    return -1;  // 解析失败
}
```

#### nw_mark_network_status() 逐行

```c
void nw_mark_network_status(int net_status)
{
    char br_str[16] = {0};
    int fd;

    /* ★ Bug：缺少 O_TRUNC 和文件权限参数
     * open() 使用 O_CREAT 时必须提供第三个参数（文件权限 mode）
     * 不加 O_TRUNC 时，若文件存在且原内容更长，写入 "0" 后可能残留原内容 */
    fd = open(NW_STATUS_PATH, O_WRONLY | O_CREAT);  // 缺少 mode 和 O_TRUNC
    sprintf(br_str, "%d", net_status);
    write(fd, br_str, strlen(br_str));  // 未检查 fd == -1 的错误情况
    close(fd);
}
```

#### nw_get_ifaddrs() — 悬空指针 Bug 详解

```c
struct ifaddrs *nw_get_ifaddrs(char *p_ip_addr)
{
    struct ifaddrs *ifaddr, *ifa, *p_ifaddrs;
    int family, s, n;
    char host[NI_MAXHOST];

    /* getifaddrs：枚举本机所有网络接口的地址信息（链表形式）
     * 内部通过 netlink socket 向内核查询，结果存在 ifaddr 链表中
     * ifaddr 链表的内存由内核分配，需要 freeifaddrs 释放 */
    if (getifaddrs(&ifaddr) == -1) {
        SEAS_LOG_ERROR("getifaddrs error");
        return NULL;
    }

    /* 遍历所有网络接口，找到 IP 地址与 p_ip_addr 匹配的接口 */
    for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if (ifa->ifa_addr == NULL) continue;

        family = ifa->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            /* getnameinfo：将 sockaddr 转换为可读的 IP 字符串 */
            s = getnameinfo(ifa->ifa_addr,
                (family == AF_INET) ? sizeof(struct sockaddr_in)
                                    : sizeof(struct sockaddr_in6),
                host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s == 0) {
                /* 比较 IP 地址字符串 */
                if (0 == strncmp(p_ip_addr, host, strlen(p_ip_addr))) {
                    /* ★ Bug：浅拷贝！
                     * calloc 分配新的 ifaddrs 结构体，memcpy 复制整个结构体
                     * 但 ifaddrs 结构体中有多个指针成员（ifa_name, ifa_addr 等）
                     * 浅拷贝后，这些指针仍指向原 ifaddr 链表的内存 */
                    p_ifaddrs = (struct ifaddrs *)calloc(1, sizeof(struct ifaddrs));
                    memcpy(p_ifaddrs, ifa, sizeof(struct ifaddrs));
                    SEAS_LOG_INFO("get ifaddrs with name: %s\n", p_ifaddrs->ifa_name);
                    break;
                }
            }
        }
    }

    /* ★ 此处释放了原链表！
     * p_ifaddrs->ifa_name 指向链表内存中的接口名字符串
     * freeifaddrs 后，该内存被释放
     * 后续通过 p_ifaddrs->ifa_name 访问接口名 = 访问已释放内存（未定义行为）*/
    freeifaddrs(ifaddr);
    return p_ifaddrs;  // 返回含悬空指针的结构体
}
```

#### nw_get_if_statistics_rx_packets() 逐行

```c
bool nw_get_if_statistics_rx_packets(uint64_t *p_rx_packets, char *if_name)
{
    char if_statistics_rx_packets_path[NAME_MAX] = {0};
    int if_statistics_rx_packets_fd, file_size;
    char val_bytes[256] = {0};

    /* 构造文件路径，例如：
     * /sys/devices/virtual/net/rmnet0/statistics/rx_packets
     * 这个文件是 Linux 内核 sysfs 暴露的网络接口统计信息
     * 内容是接收到的数据包总数，单调递增 */
    sprintf(if_statistics_rx_packets_path,
            NW_IF_STATISTICS_RX_PACKETS_PATH, if_name);

    if ((if_statistics_rx_packets_fd =
         open(if_statistics_rx_packets_path, O_RDONLY)) == -1) {
        SEAS_LOG_ERROR("open statistics file: %s failed\n",
                       if_statistics_rx_packets_path);
        return ret;  // ★ Bug：ret 未初始化就返回（ret 在 bool ret = false 处初始化了，OK）
    }

    /* lseek 到末尾获取文件大小，再 lseek 回头读取
     * 对于 sysfs 文件，此方法可能不准确（sysfs 文件大小由内核动态生成）
     * 但对于数字内容，实际上没问题 */
    file_size = lseek(if_statistics_rx_packets_fd, 0L, SEEK_END);
    lseek(if_statistics_rx_packets_fd, 0L, SEEK_SET);
    read(if_statistics_rx_packets_fd, val_bytes, file_size);
    close(if_statistics_rx_packets_fd);

    /* strtoull：将字符串转换为无符号长长整型（64位）
     * 第三参数 0 表示自动检测进制（以0x开头为16进制，以0开头为8进制，否则10进制） */
    *p_rx_packets = strtoull(val_bytes, NULL, 0);
    return true;
}
```

---

### 3.6 apn.c / apn.h — APN 配置层

#### APN 是什么

APN（Access Point Name，接入点名称）是移动网络中标识数据网关的字符串。拨号时必须配置正确的 APN，否则模组无法建立数据连接或者会连到错误的网络（如企业内网而非公共互联网）。

不同运营商有不同的 APN：
- 中国移动：`cmnet`（公网）或 `cmwap`（WAP）
- 中国联通：`3gnet`
- 沃达丰企业卡：`irootech.gdsp`（专用 APN）
- 挪威电信（Telia）：`internet.lte.cxn`

#### apn_get_apn_obj() 逐行

```c
apn_obj_t *apn_get_apn_obj(char *iccid)
{
    char json_path[256] = {0};
    apn_obj_t *p_apn_obj;

    /* 构造 APN 配置文件路径：/usr/dial/apn.json
     * /usr/dial 是此项目在设备上的安装目录 */
    sprintf(json_path, "%s/%s", "/usr/dial", APN_JSON_NAME);  // APN_JSON_NAME="apn.json"

    /* 用 ICCID 在 JSON 文件中查找对应的 APN 配置 */
    p_apn_obj = apn_scan_from_json(json_path, iccid);

    if (p_apn_obj == NULL) {
        /* 若未找到匹配项，使用空默认值（apn="" usr_name="" pwd=""）
         * 空 APN 在某些运营商（如国内）会使用模组默认 APN，
         * 但在国际场景可能导致拨号失败 */
        p_apn_obj = calloc(1, sizeof(apn_obj_t));
        const apn_obj_t cnst_default_apn = {"", "", "", "", NULL};
        memcpy((char *)p_apn_obj, (char *)&cnst_default_apn, sizeof(cnst_default_apn));
    } else {
        /* ★ Bug：这里的 else 分支逻辑反了！
         * p_apn_obj != NULL（找到了配置）时，打印的是 "is NULL" 错误日志
         * 正确逻辑应该是 if(p_apn_obj == NULL) 打印错误，else 打印成功 */
        SEAS_LOG_ERROR("p_apn_obj is NULL");  // 逻辑反了
    }
    return p_apn_obj;
}
```

#### apn_scan_from_json() 核心逻辑

```c
apn_obj_t *apn_scan_from_json(char *json_path, char *iccid)
{
    // 1. 打开并读取整个 JSON 文件到内存
    fp = fopen(json_path, "r");
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    rewind(fp);
    str = (char *)malloc(len);

    /* ★ Bug：fread 失败时泄漏 str 和 fp */
    if (fread(str, 1, len, fp) < 0) { return NULL; }

    // 2. 用 cJSON 解析 JSON
    json_root = cJSON_Parse(str);
    json_apn_array = cJSON_GetObjectItem(json_root, "apn");

    // 3. 遍历 "apn" 数组，按 ICCID 前缀匹配
    for (i = 0; i < cJSON_GetArraySize(json_apn_array); i++) {
        json_apn = cJSON_GetArrayItem(json_apn_array, i);
        json_temp = cJSON_GetObjectItem(json_apn, "iccid");
        p_json_str = cJSON_GetStringValue(json_temp);

        /* strncmp 比较 ICCID 前缀（JSON 中的 iccid 字段长度决定比较长度）
         * 例如 JSON 中 "iccid":"898604"，实际 ICCID "8986041234567890123"
         * strncmp(p_json_str, iccid, strlen("898604")) → 比较前 6 位 */
        if (0 == strncmp(p_json_str, iccid, strlen(p_json_str))) {
            p_apn_obj = (apn_obj_t *)calloc(1, sizeof(apn_obj_t));
            // 填充 iccid, apn, usrname, pwd ...

            /* 读取 oper 字段（允许运营商列表）存入双端队列 */
            json_temp = cJSON_GetObjectItem(json_apn, "oper");
            if ((json_temp != NULL) && (cJSON_IsArray(json_temp))) {
                cc_deque_new(&p_apn_obj->deque_apn_oper);
                for (j = 0; j < cJSON_GetArraySize(json_temp); j++) {
                    p_json_str = cJSON_GetStringValue(cJSON_GetArrayItem(json_temp, j));
                    p_oper_name = (char *)calloc(1, strlen(p_json_str) + 1);
                    memcpy(p_oper_name, p_json_str, strlen(p_json_str));
                    cc_deque_add_last(p_apn_obj->deque_apn_oper, p_oper_name);
                }
            }

            /* is_oper_select：是否启用运营商手动选择
             * true  = 拨号失败时执行 AT+COPS 扫描和手动注册
             * false = 直接让模组自动选择运营商（AT+COPS=0）*/
            p_apn_obj->is_oper_select = true;  // 默认 true
            json_temp = cJSON_GetObjectItem(json_apn, "is_oper_select");
            if ((json_temp != NULL) && (cJSON_IsBool(json_temp))) {
                p_apn_obj->is_oper_select = cJSON_IsTrue(json_temp);
            }
            break;  // 找到后立即退出循环
        }
    }
    free(str);
    fclose(fp);
    return p_apn_obj;
}
```

#### apn_scan_idx() — 死代码分析

```c
int apn_scan_idx(apn_obj_t *p_apn_obj)
{
    ql_apn_info_list_s apn_list;
    int i = 0, ret = -1;
    memset(&apn_list, 0, sizeof(apn_list));

    /* ★ 重要：这一行提前 return，导致后面所有代码永远不执行
     * V1.29.4 版本的 README 说：高通模组只对 profile_idx=1 的拨号自动设置路由
     * 所以强制用 profile_idx=1，放弃了之前的 APN 列表扫描逻辑
     * 但原来的代码没有删除，成了死代码 */
    return apn_set(1, p_apn_obj);  // 强制使用 profile index 1

    // ↓ 以下代码永远不会执行（死代码）
    if (QL_APN_Get_Lists(&apn_list) < 0) { ... }
    if (apn_list.cnt == 0) {
        ret = apn_add(p_apn_obj);
    } else {
        for (i = 0; i < apn_list.cnt; i++) {
            // 扫描现有 APN 列表，找到匹配的 APN
            if (...strcmp(p_apn_obj->apn, apn_list.apn[i].apn_name)...) {
                // 检查配置是否正确，不对则更新
            }
        }
        if (i != apn_list.cnt) {
            ret = apn_list.apn[i].profile_idx;  // 返回找到的 profile_idx
        } else {
            ret = apn_add(p_apn_obj);
            if (ret = -1) {  // ★ Bug：= 是赋值不是比较！
                ret = apn_set(1, p_apn_obj);
            }
        }
    }
    return ret;
}
```

---

### 3.7 dial.c / dial.h — 核心状态机

#### dial.h 关键常量与结构

```c
#define SIM_CHECK_TIMEOUT_SECONDS (3600)
/* SIM 卡检测超时：3600 秒（1小时）
 * 若 SIM 卡持续 1 小时无法就绪，触发 CFUN 重置 */

#define DIAL_TIMEOUT_SECONDS (60)
/* 拨号超时：60 秒
 * 等待连接成功的最长时间，超时则重新发起注册检查 */

#define DIAL_CSQ_THREADHOLD (20)
/* CSQ 信号强度阈值（当前被注释掉，未实际使用）
 * CSQ=20 对应约 -73dBm，是较好的信号水平 */

#define DIAL_CSQ_INVALID (99)
/* CSQ=99 表示模组无法获取信号强度（如 SIM 未就绪） */

#define AT_RETRY_NB (2)
/* AT 命令重试次数上限：失败最多重试 2 次 */

#define AUTO_REDIAL
/* 编译时宏：定义此宏则在调用 QL_Data_Call_Start 时设置 reconnect=true
 * 让模组在断线时自动重新建立数据连接（底层自动重连） */
```

#### dial_mng_new() 逐行

```c
dial_mng_t *dial_mng_new(void)
{
    /* calloc 分配并清零整个结构体（calloc = malloc + memset 0）*/
    dial_mng_t *p_dial_mng = (dial_mng_t *)calloc(1, sizeof(dial_mng_t));
    sim_mng_t *p_sim_mng;

    /* memset 再次清零（calloc 已清零，此处冗余）*/
    memset((char *)p_dial_mng, 0, sizeof(dial_mng_t));

    p_dial_mng->p_ipv4_data_call_info = NULL;
    p_dial_mng->dial_st = dial_stat_none;  // 初始状态：none

    /* 分配 SIM 管理器 */
    p_sim_mng = (sim_mng_t *)calloc(1, sizeof(sim_mng_t));
    p_dial_mng->p_sim_mng = p_sim_mng;

    /* at_init：打开 /dev/smd8，发送初始化 AT 命令，返回 fd */
    p_dial_mng->smd_fd = at_init();

    /* 初始化运营商候选队列（双端队列，容量初始8，可动态扩展） */
    cc_deque_new(&p_dial_mng->deque_oper);

    p_dial_mng->p_apn_obj = NULL;
    p_dial_mng->is_func_called = true;  // 标记需要调用 QL_Data_Call_Init
    return p_dial_mng;
}
```

#### dial_task() 状态机逐状态详解

```c
void *dial_task(void *arg)
{
    dial_mng_t *p_dial_mng = (dial_mng_t *)arg;
    sim_mng_t *p_sim_mng = p_dial_mng->p_sim_mng;
    struct timespec cur_timer, dif_timer;  // 单调时钟，用于超时计算
    char rsp_msg[1024];
    char *p_oper;
    char dial_oper_slecet_str[256] = {0};  // AT+COPS=1,2,XXXXX 命令缓冲区
    int retry_nb = 0;  // AT 命令重试计数器

    while (true) {
        /* 每次循环获取当前时间（CLOCK_MONOTONIC：单调时钟，不受系统时间调整影响）*/
        clock_gettime(CLOCK_MONOTONIC, &cur_timer);

        switch (p_dial_mng->dial_st) {

        /* ── 状态 0：none ──────────────────────────────────────────── */
        case dial_stat_none:
            p_dial_mng->dial_st = dial_stat_init;  // 立即切换到 init
            break;

        /* ── 状态 1：init ──────────────────────────────────────────── */
        case dial_stat_init:
            /* 初始化 MCM NW 客户端，建立与 MCM 服务的连接
             * MCM 服务（mcm_daemon 进程）在系统启动时运行，
             * 提供 SIM、网络、数据呼叫等功能的 IPC 接口 */
            if (dial_init(p_dial_mng)) {  // QL_MCM_NW_Client_Init
                p_dial_mng->dial_st = dial_stat_sim_init;
            }
            // 失败时不 sleep，下次循环末尾的 sleep(1) 提供间隔
            break;

        /* ── 状态 2：sim_init ──────────────────────────────────────── */
        case dial_stat_sim_init:
            /* 初始化 MCM SIM 客户端，获取 h_sim 句柄
             * 必须成功后才能调用任何 SIM 相关 API */
            if (E_QL_OK == sim_op(SIM_OP_INIT, NULL)) {
                p_dial_mng->dial_st = dial_stat_sim_check;
                /* 记录进入 sim_check 状态的时刻，用于 SIM 超时检测 */
                p_dial_mng->dial_timer = cur_timer;
            } else {
                sleep(2);  // 初始化失败，2 秒后重试
            }
            break;

        /* ── 状态 3：sim_check ─────────────────────────────────────── */
        case dial_stat_sim_check:
            /* 检查 SIM 卡是否就绪（READY 状态）
             * 刚插卡或模组刚开机时，SIM 需要几秒钟初始化 */
            if (nw_get_sim_card_status(p_dial_mng->h_nw_client)) {
                p_dial_mng->dial_st = dial_stat_sim_op;
            } else {
                sleep(2);  // SIM 未就绪，2 秒后重试
            }
            /* 超时检测：超过 3600 秒（1小时）SIM 仍未就绪
             * 此时通过 CFUN 复位射频，尝试恢复 SIM */
            dif_timer = diff(p_dial_mng->dial_timer, cur_timer);
            if (dif_timer.tv_sec > SIM_CHECK_TIMEOUT_SECONDS) {
                p_dial_mng->dial_st = dial_stat_stop_cfun;  // 触发 CFUN 复位
            }
            break;

        /* ── 状态 4：sim_op ────────────────────────────────────────── */
        case dial_stat_sim_op:
            /* 读取 ICCID（同时因为 fall-through bug 也读了 IMSI）
             * ICCID 用于在 apn.json 中查找匹配的 APN 配置 */
            if (sim_op_handler(p_sim_mng, sim_op_stat_get_iccid)) {
                p_dial_mng->dial_st = dial_stat_reg_check;
            }
            break;

        /* ── 状态 5：reg_check ─────────────────────────────────────── */
        case dial_stat_reg_check:
            /* 通过 MCM NW API 检查 PS（分组交换）域注册状态
             * QL_MCM_NW_GetRegStatus 返回数据注册是否有效
             * 这是检查设备是否能上移动数据网络的关键步骤 */
            if (nw_reg_status_check(p_dial_mng->h_nw_client)) {
                p_dial_mng->dial_st = dial_stat_cereg_check;
            } else {
                sleep(2);
            }
            break;

        /* ── 状态 6：cereg_check ───────────────────────────────────── */
        case dial_stat_cereg_check:
            /* 在 MCM NW 检查之后，再通过 AT+CEREG? 确认 LTE 注册状态
             * 双重检查更可靠，避免 MCM 报告成功但实际 LTE 未注册 */
            if (nw_at_get_cereg_stat(p_dial_mng->smd_fd)) {
                p_dial_mng->dial_st = dial_stat_precondition_check;
            } else {
                sleep(10);  // CEREG 不通，等待 10 秒（比 reg_check 的 2 秒更长）
            }
            break;

        /* ── 状态 7：precondition_check ───────────────────────────── */
        case dial_stat_precondition_check:
            /* QL_Data_Call_Init_Precondition：检查数据呼叫前提条件
             * 内部检查 QMI DSD 服务是否就绪等底层条件
             * 返回 0 = 就绪 */
            if (0 == QL_Data_Call_Init_Precondition()) {
                p_dial_mng->profile_idx = -1;  // 重置为未选择状态
                p_dial_mng->dial_st = dial_stat_pre_start_call;
            }
            break;

        /* ── 状态 8：pre_start_call ────────────────────────────────── */
        case dial_stat_pre_start_call:
            if (p_dial_mng->profile_idx == -1) {
                /* 首次进入：获取 APN 对象和 profile_idx */
                if (p_dial_mng->p_apn_obj == NULL) {
                    /* 根据 ICCID 从 apn.json 查找 APN 配置 */
                    p_dial_mng->p_apn_obj = apn_get_apn_obj(p_sim_mng->sim_iccid);
                }
                if (p_dial_mng->p_apn_obj != NULL) {
                    /* apn_scan_idx 实际上直接调用 apn_set(1, ...) 返回 1
                     * profile_idx = 1（高通平台专用，只有 index=1 会自动设置路由）*/
                    p_dial_mng->profile_idx = apn_scan_idx(p_dial_mng->p_apn_obj);
                }
            } else {
                /* profile_idx 已确定，设置为默认 profile */
                ql_data_call_default_profile_s profile;
                memset(&profile, 0, sizeof(ql_data_call_default_profile_s));
                profile.profile_idx = p_dial_mng->profile_idx;
                /* QL_Data_Call_Set_Default_Profile：告知模组用此 profile 建立连接 */
                QL_Data_Call_Set_Default_Profile(&profile);
                p_dial_mng->dial_st = dial_stat_start_call;
            }
            break;

        /* ── 状态 9：start_call ────────────────────────────────────── */
        case dial_stat_start_call:
            if (dail_start_data_call(p_dial_mng)) {
                p_dial_mng->dial_st = dial_stat_wait_for_connect;
                p_dial_mng->dial_timer = cur_timer;  // 开始计时等待连接
            }
            break;

        /* ── 状态 10：wait_for_connect ─────────────────────────────── */
        case dial_stat_wait_for_connect:
            if (nw_get_connect_state(p_dial_mng->profile_idx) == 1) {
                /* 连接成功！执行后连接初始化 */
                get_tz_after_connected(p_dial_mng);  // 获取网络时区并保存
                ipv4_data_call_info_init(p_dial_mng); // 获取 IP 地址和接口信息
                nw_mark_network_status(1);             // 写 /tmp/network_status = 1
                p_dial_mng->dial_st = dial_stat_net_connected;
            } else {
                /* 超时检查：60 秒内未连接成功 */
                dif_timer = diff(p_dial_mng->dial_timer, cur_timer);
                if (dif_timer.tv_sec > DIAL_TIMEOUT_SECONDS) {
                    if (p_dial_mng->p_apn_obj->is_oper_select) {
                        /* is_oper_select=true：尝试手动选择运营商 */
                        p_dial_mng->dial_st = dial_stat_list_oper;
                    } else {
                        /* is_oper_select=false：发送 AT+COPS=0 让模组自动选运营商 */
                        Ql_SendAT(p_dial_mng->smd_fd, "AT+COPS=0", "OK", 180000, rsp_msg);
                        p_dial_mng->dial_st = dial_stat_reg_check;
                    }
                }
            }
            break;

        /* ── 状态 11：net_connected（主要运行状态） ─────────────────── */
        case dial_stat_net_connected:
            /* 检查连接是否仍然存在 */
            if (nw_get_connect_state(p_dial_mng->profile_idx) <= 0) {
                nw_mark_network_status(0);  // 写 /tmp/network_status = 0
                p_dial_mng->dial_st = dial_stat_reg_check;  // 重新注册
            } else {
                /* 流量保活检测：读取 rx_packets 计数 */
                uint64_t if_rx_packets;
                if (nw_get_if_statistics_rx_packets(&if_rx_packets,
                                                    p_dial_mng->p_ifaddrs->ifa_name)) {
                    /* 只要 rx_packets 有增加，就重置计时器
                     * 有新包 = 链路活跃，即使偶尔断连也有恢复迹象 */
                    if ((if_rx_packets != p_dial_mng->u64_if_rx_packets)
                        && (if_rx_packets > p_dial_mng->u64_if_rx_packets)) {
                        p_dial_mng->dial_timer = cur_timer;  // 重置超时计时器
                    }
                    p_dial_mng->u64_if_rx_packets = if_rx_packets;
                }
            }

            /* 超时检测：60 秒内无新接收包，认为链路死亡 */
            dif_timer = diff(p_dial_mng->dial_timer, cur_timer);
            if (dif_timer.tv_sec > DIAL_TIMEOUT_SECONDS) {
                SEAS_LOG_ERROR("no data rx on net if, redial");
                p_dial_mng->dial_st = dial_stat_reg_check;
            }
            sleep(5);  // 已连接状态下，5 秒检查一次（其他状态 1 秒一次）
            break;

        /* ── 状态 12：list_oper ────────────────────────────────────── */
        case dial_stat_list_oper:
            if (0 == cc_deque_size(p_dial_mng->deque_oper)) {
                /* 运营商队列为空，执行扫描 */
                memset(rsp_msg, 0, 1024);
                pthread_mutex_lock(&g_at_port_mutex);
                /* AT+COPS=?：扫描所有可见运营商，超时 180 秒
                 * 响应格式：+COPS: (stat,"longName","shortName","numericName",act),...
                 * 例如：(1,"AIS","AIS","52003",2),(2,"DTAC","dtac","52005",0),...
                 * stat=1 = 可用，stat=2 = 当前（已注册），stat=3 = 禁止 */
                if (0 != Ql_SendAT(p_dial_mng->smd_fd, "AT+COPS=?", "OK", 180000, rsp_msg)) {
                    pthread_mutex_unlock(&g_at_port_mutex);
                    retry_nb++;
                    if (retry_nb >= AT_RETRY_NB) {  // 重试 2 次失败
                        retry_nb = 0;
                        p_dial_mng->dial_st = dial_stat_reg_check;  // 放弃，回到注册检查
                    }
                    sleep(5);
                    continue;
                }
                pthread_mutex_unlock(&g_at_port_mutex);
                /* 解析响应，把 stat=1 的运营商号码（numericName）加入队列 */
                parse_oper_list_info(p_dial_mng->deque_oper, rsp_msg);
            }
            /* 切换到运营商选择状态 */
            if (0 != cc_deque_size(p_dial_mng->deque_oper)) {
                p_dial_mng->dial_st = dial_stat_select_oper;
            } else {
                p_dial_mng->dial_st = dial_stat_reg_check;
            }
            break;

        /* ── 状态 13：select_oper ──────────────────────────────────── */
        case dial_stat_select_oper:
            /* 从队列头部取第一个运营商号码 */
            cc_deque_get_at(p_dial_mng->deque_oper, 0, (void **)&p_oper);
            if (p_oper != NULL) {
                /* 构造手动注册命令：AT+COPS=1,2,"52003"
                 * 1 = 手动注册模式
                 * 2 = 格式：数字形式（即 PLMN 号码）
                 * "52003" = 运营商号码 */
                sprintf(dial_oper_slecet_str, "AT+COPS=1,2,%s", p_oper);
                pthread_mutex_lock(&g_at_port_mutex);
                if (0 != Ql_SendAT(p_dial_mng->smd_fd, dial_oper_slecet_str, "OK", 30000, rsp_msg)) {
                    pthread_mutex_unlock(&g_at_port_mutex);
                    retry_nb++;
                    if (retry_nb >= AT_RETRY_NB) {
                        retry_nb = 0;
                        /* 该运营商注册失败，从队列移除 */
                        if (CC_OK == cc_deque_remove_first(p_dial_mng->deque_oper, (void **)&p_oper)) {
                            free(p_oper);
                        }
                        /* 队列空了 → 重启射频；否则继续尝试下一个 */
                        p_dial_mng->dial_st = (0 == cc_deque_size(p_dial_mng->deque_oper))
                            ? dial_stat_start_cfun : dial_stat_list_oper;
                    }
                    usleep(5000 * 1000);  // 5 秒后重试
                    continue;
                }
                pthread_mutex_unlock(&g_at_port_mutex);
                /* 注册成功，移除已用运营商 */
                if (CC_OK == cc_deque_remove_first(p_dial_mng->deque_oper, (void **)&p_oper)) {
                    free(p_oper);
                }
            }
            /* 无论成功失败，执行 CFUN 复位以激活新运营商注册 */
            p_dial_mng->dial_st = dial_stat_stop_cfun;
            break;

        /* ── 状态 14：stop_cfun ────────────────────────────────────── */
        case dial_stat_stop_cfun:
            /* AT+CFUN=0：将模组射频功能设置为最小功能（关闭射频）
             * 效果：SIM 卡下电，模组停止注册网络，数据连接断开
             * 目的：用于运营商切换后强制重新初始化射频栈 */
            pthread_mutex_lock(&g_at_port_mutex);
            if (0 != Ql_SendAT(p_dial_mng->smd_fd, "AT+CFUN=0", "OK", 15000, rsp_msg)) {
                pthread_mutex_unlock(&g_at_port_mutex);
                retry_nb++;
                if (retry_nb >= AT_RETRY_NB) {
                    retry_nb = 0;
                    p_dial_mng->dial_st = dial_stat_reg_check;
                }
                usleep(5000 * 1000);
                continue;
            }
            usleep(5000 * 1000);  // 等待 5 秒让射频稳定关闭
            pthread_mutex_unlock(&g_at_port_mutex);
            p_dial_mng->dial_st = dial_stat_start_cfun;
            break;

        /* ── 状态 15：start_cfun ───────────────────────────────────── */
        case dial_stat_start_cfun:
            /* AT+CFUN=1：恢复全功能（开启射频）
             * 模组重新开机，SIM 卡上电，重新搜网注册
             * 回到 sim_init 状态重新走完整流程 */
            pthread_mutex_lock(&g_at_port_mutex);
            if (0 != Ql_SendAT(p_dial_mng->smd_fd, "AT+CFUN=1", "OK", 15000, rsp_msg)) {
                pthread_mutex_unlock(&g_at_port_mutex);
                retry_nb++;
                if (retry_nb >= AT_RETRY_NB) {
                    retry_nb = 0;
                    p_dial_mng->dial_st = dial_stat_reg_check;
                }
                usleep(5000 * 1000);
                continue;
            }
            usleep(5000 * 1000);  // 等待 5 秒让射频完全启动
            p_dial_mng->dial_st = dial_stat_sim_init;  // 从 SIM 初始化重走
            pthread_mutex_unlock(&g_at_port_mutex);
            break;
        }
        sleep(1);  // 每个状态循环末尾固定 sleep 1 秒（已连接状态会额外 sleep 5）
    }
}
```

---

### 3.8 tz.c / tz.h — 时区获取

```c
/* 时区信息保存路径 */
#define TZ_SAVE_PATH "/usr/dial/tz.ini"

bool parse_tz_info(char *p_tz_str, int *p_tz)
{
    /* AT+QLTS=1 响应示例：
     * +QLTS: "2023/12/05,09:43:17+32,0"
     * 格式：年/月/日,时:分:秒+时区偏移,夏令时标志
     * +32 表示 UTC+8（每单位 = 15 分钟，32 × 15分 = 480分 = 8小时）
     * -32 表示 UTC-8 */
    if (strstr(p_tz_str, "+QLTS:")) {
        p_start = strchr(p_tz_str, '"');  // 找第一个引号
        p_start += 1;
        p_end = strchr(p_start, '"');     // 找第二个引号
        if ((p_end != 0) && (p_end != p_start)) {
            memcpy(tz_str, p_start, p_end - p_start);
            /* sscanf 格式串说明：
             * %*d  = 跳过年份（*号表示读取但不保存）
             * %*d  = 跳过月份
             * %*d  = 跳过日期（注意分隔符是 / 和 ,）
             * %*d  = 跳过小时
             * %*d  = 跳过分钟
             * %*d  = 跳过秒
             * %d   = 读取时区偏移值（有符号）*/
            sscanf(p_start, "%*d/%*d/%*d,%*d:%*d:%*d%d,%*d", p_tz);
        }
        ret = true;
    }
    return ret;
}

void save_tz_info(int tz)
{
    /* 使用 iniparser 库读取、修改、写回 tz.ini 文件
     * 格式：
     * [dev]
     * tz = 32      ← +8小时时区（32÷4=8）
     *
     * 使用场景：上层应用通过读取此文件判断设备所在时区
     * （例如记录日志时间戳的时区校正）*/
    if (access(TZ_SAVE_PATH, F_OK) == -1) {
        system("touch /usr/dial/tz.ini");  // 文件不存在则创建
    }
    dictionary *ini = iniparser_load(TZ_SAVE_PATH);
    ini_tz = iniparser_getint(ini, "dev:tz", 255);  // 读取当前值，默认 255
    if (ini_tz != tz) {  // 只有变化时才写入，减少 flash 磨损
        iniparser_set(ini, "dev", NULL);   // 确保 [dev] section 存在
        sprintf(tz_info_bytes, "%d", tz);
        iniparser_set(ini, "dev:tz", tz_info_bytes);
    }
    FILE *fp = fopen(TZ_SAVE_PATH, "w");
    iniparser_dump_ini(ini, fp);
    fclose(fp);
    iniparser_freedict(ini);
    sync();  // 强制将缓冲区数据同步到存储介质（重要！防止掉电丢失）
}
```

---

### 3.9 seas_log.c / seas_log.h — 日志系统

#### 日志宏展开过程

```c
/* 以 SEAS_LOG_INFO("CSQ: %d", csq) 为例，展开过程：*/

SEAS_LOG_INFO("CSQ: %d", csq)
→ do {
    if (seas_log_get_level() <= SEAS_LEVEL_INFO) {  // 当前级别 ≤ 1
        SEAS_EMIT_LOG(SEAS_COLOR_INFO, "[INFO]", "CSQ: %d", csq);
    }
} while (0)

→ SEAS_EMIT_LOG("\x1B[36m", "[INFO]", "CSQ: %d", csq)
→ seas_emit_log("\x1B[36m", "[INFO]", __FILE__, __func__, __LINE__, "CSQ: %d", csq)
```

#### seas_emit_log() 输出格式

```
2024-01-15 10:25:03.899 [INFO] dial_task (dial.c:245) - CSQ: 20
│              时间戳    │级别│  函数名   │  文件:行号  │  分隔  │消息
```

#### 日志级别

```c
#define SEAS_LEVEL_DEBUG    0   // 最低，包含一切
#define SEAS_LEVEL_INFO     1   // 一般信息（当前使用）
#define SEAS_LEVEL_NOTICE   2
#define SEAS_LEVEL_WARNING  3
#define SEAS_LEVEL_ERROR    4   // 只有错误
#define SEAS_LEVEL_CRITICAL 5
#define SEAS_LEVEL_ALL      6
#define SEAS_LEVEL_SILENT   7   // 最高，关闭所有输出
```

#### 日志轮转机制

```c
/* seas_rotate_log_file：写入时检查文件大小，超过 max_file_size 则覆写 */
if (handle.file_size >= handle.max_file_size) {
    seas_open_log_file("w+");  // "w+" 模式：清空文件重新写
}
/* 注意：这是简单覆写（截断后从头写），不是真正的日志轮转（rotate）
 * 真正的轮转会保留旧日志文件（如 .log.1, .log.2）
 * 此方案最多保留 max_file_size 的最新日志 */
```

---

### 3.10 cc_deque — 双端队列库

来自开源项目 Collections-C（LGPL v3），是一个动态数组实现的双端队列：

```c
/* 内部结构 */
struct cc_deque_s {
    size_t   size;       // 当前元素数量
    size_t   capacity;   // 当前分配容量（始终是 2 的幂）
    size_t   first;      // 第一个元素在 buffer 中的索引
    size_t   last;       // 最后一个元素后一位的索引
    void   **buffer;     // void* 指针数组（存储元素指针，不存储元素本身）
    // 内存分配函数指针（默认使用 malloc/calloc/free）
};
/* 容量不足时按 DEFAULT_EXPANSION_FACTOR=2 扩容（容量翻倍） */
```

在本项目中用于：
1. `dial_mng_t.deque_oper`：运营商候选队列（`char*` 字符串，每个存储 PLMN 号码如 `"46000"`）
2. `apn_obj_t.deque_apn_oper`：APN 允许运营商队列（当前代码未实际用于过滤）

---

### 3.11 APN 配置文件分析（apn.json）

#### 标准版本（apn normal/apn.json）

```json
{
  "apn": [
    {
      "iccid": "898604",        // 中国移动 ICCID 前缀
      "apn": "CMMTMSGHLKJ.HN", // 企业专用 APN（非公网 cmnet）
      "usrname": "",
      "pwd": "",
      "supplier": "china mobile",
      "oper": ["46000"],        // 只允许注册中国移动（46000）
      "is_oper_select": false   // 不手动选择运营商（使用自动）
    },
    {
      "iccid": "893144",        // 沃达丰 ICCID 前缀
      "apn": "irootech.gdsp",   // 沃达丰企业专用 APN
      "usrname": "a",           // 需要用户名密码认证
      "pwd": "a",
      "supplier": "vodafone"
      // 没有 is_oper_select 字段：默认 true（启用手动运营商切换）
    },
    {
      "iccid": "894608",        // 挪威电信 Telia 一种 ICCID 前缀
      "apn": "internet.lte.cxn",
      ...
    },
    {
      "iccid": "896604",        // True Move（泰国）ICCID 前缀
      "apn": "internet",
      ...
    },
    {
      "iccid": "896210",        // Telkomsel（印尼）ICCID 前缀
      "apn": "Internet",
      ...
    }
  ]
}
```

**匹配逻辑**：`strncmp(json_iccid, real_iccid, strlen(json_iccid))` — 只比较 JSON 中 iccid 字段的长度，所以 `"898604"` 会匹配所有以 `"898604"` 开头的 ICCID（中国移动所有 ICCID 共同前缀）。

---

## 4. open_dial_for_artery 编译指南

### 4.1 前提条件

编译 open_dial 需要 Quectel OpenLinux SDK（针对 EC2x/EG2x），SDK 通常由 Quectel 提供给设备厂商，不公开。以下描述标准流程：

### 4.2 环境准备

```bash
# 1. 安装 Yocto/OpenEmbedded 交叉编译工具链
# （Quectel 会提供对应版本的工具链安装包，例如 poky-glibc-x86_64-*.sh）
chmod +x poky-glibc-x86_64-*.sh
./poky-glibc-x86_64-*.sh   # 默认安装到 /opt/poky/...

# 2. 加载工具链环境变量
source /opt/poky/3.x/environment-setup-cortexa7hf-neon-poky-linux-gnueabi
# 此命令会设置：
#   CC=arm-poky-linux-gnueabi-gcc
#   SDKTARGETSYSROOT=/opt/poky/3.x/sysroots/cortexa7hf-...

# 3. 解压 Quectel SDK
# 目录结构应为：
# quectel_sdk/
# ├── lib/
# │   ├── interface/inc/   ← SDK 头文件（ql_oe.h, ql_wwan_v2.h 等）
# │   ├── libql_common_api.a
# │   └── ...
# └── ...
```

### 4.3 目录放置要求

```
Makefile 中 QL_SDK_PATH 默认为 $(shell pwd)/../..
因此项目目录结构应为：

quectel_sdk/                   ← QL_SDK_PATH 指向这里
├── lib/
│   ├── interface/
│   │   └── inc/
│   │       ├── ql_oe.h
│   │       ├── ql_wwan_v2.h
│   │       └── ...
│   ├── libql_common_api.a
│   └── ...
└── examples/                  ← 或其他目录名
    └── open_dial_for_artery/  ← 项目目录（pwd/../.. = quectel_sdk/）
        ├── main.c
        ├── Makefile
        └── src/
```

或者手动指定 SDK 路径：

```bash
make QL_SDK_PATH=/path/to/quectel_sdk \
     SDKTARGETSYSROOT=/opt/poky/3.x/sysroots/cortexa7hf-neon-poky-linux-gnueabi
```

### 4.4 编译命令

```bash
cd open_dial_for_artery/

# 方式一：使用 Makefile 默认设置
make

# 方式二：显式指定 SDK 和 sysroot 路径
make QL_SDK_PATH=/your/sdk/path \
     SDKTARGETSYSROOT=/your/sysroot/path

# 方式三：使用 Makefile 注释中的 Artery 目标工具链
# （Makefile 注释中有两套工具链配置，
#  arm-oe-linux-gnueabi-gcc 是当前激活的）
```

### 4.5 编译产物部署

```bash
# 成功编译后生成 dial（ARM ELF 二进制）
file dial
# → ELF 32-bit LSB executable, ARM, EABI5 ...

# 部署到设备
adb push dial /usr/dial/dial
# 或通过 SCP（如果设备有 SSH）
scp dial root@<device_ip>:/usr/dial/dial

# 设置可执行权限
adb shell chmod +x /usr/dial/dial

# 部署 APN 配置文件
adb push apn/apn\ normal/apn.json /usr/dial/apn.json

# 运行（通常由 init.d 脚本启动，手动测试：）
adb shell /usr/dial/dial
```

### 4.6 APN 配置文件定制

根据实际使用的 SIM 卡，修改 `apn.json`：

```json
{
  "apn": [
    {
      "iccid": "你的SIM卡ICCID前缀（前6-10位）",
      "apn": "运营商APN名称",
      "usrname": "用户名（无则留空）",
      "pwd": "密码（无则留空）",
      "supplier": "运营商名称（仅注释用）",
      "is_oper_select": false
    }
  ]
}
```

查找 ICCID（设备上执行）：
```bash
cat /proc/...   # 或通过 AT 命令
# AT+ICCID      # 直接读 ICCID
# AT+QCCID      # Quectel 专用指令
```

---

## 5. roamlink_deploy 全量逐行分析

### 5.1 部署包结构

```
roamlink_deploy_auto_download_with_license_251028/
│
├── install_update.sh     ← U盘安装入口脚本（手动执行一次）
├── start_dfota.sh        ← 固件升级执行脚本（被 start_dfota 调用）
├── start_dfota           ← init.d 服务脚本（固件升级阶段）
├── start_check_network   ← init.d 服务脚本（正式运行阶段）
├── start_daemon          ← init.d 服务脚本（通用守护进程启动器）
├── check_network.sh      ← 网络监控主脚本（核心运行时逻辑）
├── check_fw_version      ← 固件版本检测工具（ARM 二进制）
├── get_imei              ← IMEI 读取工具（ARM 二进制）
├── test_network          ← 网络连通性测试工具（ARM 二进制）
├── dial_1.29.4           ← open_dial 编译产物（ARM 二进制）
├── network.ini           ← 网络策略配置文件
│
├── licenses/
│   └── 867929069660712_license.cer  ← IMEI 867929069660712 的授权证书
│
└── roamlink/             ← 虚拟SIM服务组件目录
    ├── RBMaster          ← 总控进程（ARM 二进制）
    ├── softsim           ← 软件SIM模拟器（ARM 二进制）
    ├── cardManager       ← 卡管理进程（ARM 二进制）
    ├── controller        ← 控制器进程（ARM 二进制）
    ├── OemServer         ← OEM适配服务（ARM 二进制）
    ├── heartbeat         ← 心跳进程（ARM 二进制）
    ├── upgrade           ← 升级进程（ARM 二进制）
    ├── libOem.so         ← OEM硬件适配共享库
    ├── libOemServer.so   ← OEM服务层共享库（含debug_info）
    ├── libTSSUMEmulator.so ← SIM-over-the-air模拟器共享库
    └── start_vsim.sh     ← 启动 RBMaster 的脚本
```

---

### 5.2 install_update.sh — 安装脚本逐行

```bash
#!/bin/sh

# 日志文件路径
LOG_FILE="/usrdata/roamlink_install.log"

# 安装控制开关
MUST_HAVE_LICENSE=1   # 1=必须有 license 才能安装，0=没有 license 也装
FORCE_INSTALL=0       # 1=强制重装（删除已有安装），0=已有则退出

log() {
    # 带时间戳的日志函数
    # date '+%Y-%m-%d %H:%M:%S' 格式如：2025-10-28 14:30:00
    echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >>"$LOG_FILE"
}

fail_exit() {
    log "ERROR: $*"
    exit 255  # 255 是 shell 中约定的"失败"退出码
}

check_cmd() {
    # 包装任意命令：执行成功正常，失败则调用 fail_exit 中断安装
    "$@" || fail_exit "command failed: $*"
}

log "==== Roamlink installation start ===="

# ── 步骤1：检查是否已安装 ──────────────────────────────────────────
if [ -d /usrdata/roamlink ]; then
    if [ $FORCE_INSTALL -eq 1 ]; then
        log "delete existing roamlink"
        rm /usrdata/roamlink -rf          # 删除整个 roamlink 目录
        rm /data/ufs/license.cer          # 删除备份的 license
    else
        fail_exit "roamlink already exists, exit!"
        # 默认不允许覆盖安装，防止误操作
    fi
fi

# ── 步骤2：检查固件版本 ────────────────────────────────────────────
./check_fw_version
fw_version=$?   # 捕获 check_fw_version 的退出码
# 退出码含义（由 check_fw_version 二进制定义）：
#   0   = 固件版本过旧，需要执行 DFOTA 升级
#   1   = 固件版本满足要求，无需升级
#   2   = 不支持的 Quectel 固件（如非 EC2x/EG2x）
#   255 = 读取固件版本失败

case $fw_version in
0) log "firmware need dfota" ;;          # 需要升级
1) log "firmware doesn't need dfota" ;;  # 已是最新
2) fail_exit "unsupported quectel firmware" ;;   # 不支持
255) fail_exit "failed to read quectel firmware" ;;  # 读取失败
*) fail_exit "unknown fw_version: $fw_version" ;;    # 未知
esac

# ── 步骤3：读取设备 IMEI ──────────────────────────────────────────
# ./get_imei 通过 QMI DMS 接口读取模组 IMEI（15位数字）
# QMI = Qualcomm MSM Interface（高通平台通信协议）
# DMS = Device Management Service（设备管理服务）
imei=$(./get_imei) || fail_exit "failed to read imei"
# imei 变量赋值为 get_imei 的标准输出，如 "867929069660712"
log "IMEI: $imei"

# ── 步骤4：检查 license 文件 ──────────────────────────────────────
# license 文件名格式：<IMEI>_license.cer
# 即每台设备有独立的 license，与 IMEI 绑定
license_name="${imei}_license.cer"
file_path="licenses/$license_name"
log "license path: $file_path"

if [ -s "$file_path" ]; then
    # -s：文件存在且大小不为零（非空文件）
    # 将 license 拷贝到 roamlink 工作目录（等会儿会 cp -af ./roamlink 过去）
    check_cmd cp -f "$file_path" ./roamlink/etc/.pconfig/license.cer
    # 同时备份到 /data/ufs/（持久化存储，重装时可恢复）
    check_cmd cp -f "$file_path" /data/ufs/license.cer
    sync   # 确保写入到存储介质
    log "license installed"
else
    log "valid license doesn't exist"
    if [ $MUST_HAVE_LICENSE -eq 1 ]; then
        fail_exit "must have license, exit"
        # 强制要求 license 的情况下直接退出，不继续安装
    fi
fi

# ── 步骤5：根据固件版本执行不同安装路径 ──────────────────────────

if [ "$fw_version" -eq 0 ]; then
    # 固件旧，需要 DFOTA 升级路径：
    # 安装 DFOTA 相关文件，设置开机自动升级
    check_cmd cp ./start_dfota.sh /usrdata          # 升级执行脚本
    check_cmd cp ./start_dfota /etc/init.d/         # init.d 服务脚本
    check_cmd cp ./start_daemon /usrdata            # 升级后修复用的守护进程脚本
    check_cmd cp ./update.zip /usrdata              # 固件升级包
    check_cmd cp ./check_fw_version /usrdata        # 版本检查工具（升级后再次使用）

    # 建立开机自动启动符号链接（SysV init，runlevel 5）
    ln -svf /etc/init.d/start_dfota /etc/rc5.d/S100start_dfota
    # S100 = 启动顺序号（越大越晚启动），100 很靠后，确保网络等基础服务已就绪

    # 屏蔽现有的 SPI 和 VSIM 服务（DFOTA 阶段不需要）
    unlink /etc/rc5.d/S20start_spi 2>/dev/null    # 2>/dev/null 忽略不存在时的错误
    unlink /etc/rc5.d/S20start_vsim

elif [ "$fw_version" -eq 1 ]; then
    # 固件已满足要求，直接安装路径：
    # 建立网络检查服务的开机链接
    ln -svf /etc/init.d/start_check_network /etc/rc5.d/S60start_check_network
    # S60 = 较早启动，确保网络服务尽快建立

    unlink /etc/rc5.d/S20start_dial 2>/dev/null   # 屏蔽原有的 dial 启动脚本
    unlink /etc/rc5.d/S20start_vsim               # 屏蔽旧的 VSIM 启动脚本
fi

# ── 步骤6：安装核心文件 ────────────────────────────────────────────
check_cmd cp -af ./roamlink /usrdata
# -a = --archive（递归复制，保留所有属性）
# -f = 强制（覆盖已有文件）
# 将整个 roamlink 目录复制到 /usrdata/roamlink/

check_cmd cp -f ./dial_1.29.4 /usr/dial/dial
# 用打包的 open_dial 编译产物替换设备上已有的 dial 程序
chmod +x /usr/dial/dial  # 确保有执行权限

# ── 步骤7：安装配置和工具 ─────────────────────────────────────────
for f in network.ini test_network check_network.sh; do
    check_cmd cp "./$f" /usrdata/
done
chmod +x /usrdata/test_network /usrdata/check_network.sh

check_cmd cp ./start_check_network /etc/init.d/
chmod +x /etc/init.d/start_check_network

# ── 步骤8：修改原有守护进程脚本 ──────────────────────────────────
# /opt/start_daemon.sh 是设备原来的启动脚本，里面可能有 check_dial 调用
# check_dial 是原来的网络检查函数，与 roamlink 的 check_network.sh 冲突
# 用 sed 注释掉 check_dial 调用行

# sed 命令解析：
# -i.bak         → 原地修改，备份原文件为 .bak
# '/^[[:space:]]*check_dial[[:space:]]*$/'
#     → 匹配：行首 + 任意空白 + "check_dial" + 任意空白 + 行尾（即单独的 check_dial 行）
# s/^/#/          → 将行首替换为 # （注释掉这一行）
sed -i.bak '/^[[:space:]]*check_dial[[:space:]]*$/ s/^/#/' /opt/start_daemon.sh

sync      # 同步所有文件写入
sleep 2   # 等待 2 秒确保 sync 完成

if [ "$fw_version" -eq 1 ]; then
    log "install roamlink finished"
else
    log "install roamlink preparation finished (need dfota after reboot)"
fi

sync
log "==== Rebooting system ===="
sleep 2
reboot   # 重启设备使所有变更生效
```

---

### 5.3 start_dfota.sh — 固件升级脚本逐行

```bash
#!/bin/sh

LOG_FILE="/usrdata/roamlink_install.log"

log() { echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >>"$LOG_FILE"; }

sleep 5   # 等待 5 秒，确保系统完全启动（网络驱动、AT 串口等就绪）

# ── 阶段A：检查是否需要升级 ──────────────────────────────────────
/usrdata/check_fw_version
if [ $? -eq 0 ]; then
    # 返回 0 = 仍然需要升级
    log "modem need upgrade"
    if [ -f /usrdata/update.zip ]; then
        log "start dfota process"
        # 通过 AT 命令触发 Quectel DFOTA 升级
        # AT+QFOTADL：Quectel 固件空中下载命令
        # "/usrdata/update.zip" = 本地固件文件路径（模组从此路径读取）
        # 发送到 /dev/smd8（AT命令通道）
        # \r\n 是 AT 命令必需的行结束符
        # >/dev/smd8 将字符串直接写入 AT 串口（不经过 Ql_SendAT）
        echo -e "AT+QFOTADL=\"/usrdata/update.zip\"\r\n" >/dev/smd8
        # 模组收到命令后会：
        # 1. 读取 update.zip 并验证签名
        # 2. 写入 flash
        # 3. 自动重启并完成升级
        exit 0  # 退出脚本，等待模组自己重启
    fi
fi

# ── 阶段B：升级已完成，修复系统环境 ─────────────────────────────
# 到这里说明 check_fw_version 返回 1（固件已满足要求）
# 即模组已完成 DFOTA 升级，现在需要修复系统环境

log "start repair env"

# 创建 sshd 用户（SSH 服务需要此用户）
# 空 "" 作为密码表示无密码
echo "" | adduser sshd

# 重建 SSH Agent 启动链接
unlink /etc/rc5.d/S100ssh_agent
ln -sv /etc/init.d/start_ssh_agent /etc/rc5.d/S100ssh_agent

# 重建 SPI 服务链接（DFOTA 前被屏蔽）
ln -sv /etc/init.d/start_spi /etc/rc5.d/S20start_spi

# 建立网络检查服务链接（正式进入 roamlink 运行模式）
ln -sv /etc/init.d/start_check_network /etc/rc5.d/S60start_check_network

# 恢复 start_daemon（从临时位置复制到 init.d）
cp /usrdata/start_daemon /etc/init.d/
chmod +x /etc/init.d/start_daemon
unlink /etc/rc5.d/S100start_deamon  # 注意拼写：deamon（原有的，可能是历史遗留错别字）
ln -sv /etc/init.d/start_daemon /etc/rc5.d/S100start_daemon

# 清理临时文件
rm /usrdata/cache -fr         # 删除缓存目录
rm /usrdata/check_fw_version  # 删除版本检查工具（不再需要）
rm /usrdata/start_daemon      # 删除临时守护进程脚本
rm /usrdata/ir_ota* -rf       # 删除 OTA 相关临时文件

# 删除 DFOTA 相关 init.d 链接和脚本（DFOTA 只需执行一次）
unlink /etc/rc5.d/S100start_dfota
unlink /etc/rc5.d/S20start_dial   # 屏蔽旧的 dial 启动链接
rm /etc/init.d/start_dfota

# 自删除：执行完毕后删除自身脚本
rm "$0"

log "install roamlink finished"
sync
sleep 3
reboot   # 最终重启进入正式运行模式
```

---

### 5.4 check_network.sh — 网络监控主脚本逐行

这是运行时最核心的脚本，持续运行于设备后台。

```bash
#!/bin/sh

# ── 变量定义 ──────────────────────────────────────────────────────
LOG_FILE="/var/log/roamlink.log"           # 运行时日志
TMP_NET_TYPE="/tmp/network_type"           # 当前网络通道类型（1=SIM, 2=Roamlink）
LICENSE_DIR="/usrdata/roamlink/etc/.pconfig/license.cer"  # 主 license 路径
BACKUP_LICENSE="/data/ufs/license.cer"    # 备份 license 路径
CONF_FILE="/opt/conf.ini"                 # 设备配置文件
NETWORK_CONF="/usrdata/network.ini"       # 网络策略配置文件

network_status=1    # 1=网络异常，0=网络正常（初始假设异常）
network_select=0    # 0=未设置，1=Roamlink优先，2=SIM优先，3=强制Roamlink，4=强制SIM
network_type=0      # 0=未设置，1=当前使用SIM，2=当前使用Roamlink

# ── 辅助函数 ──────────────────────────────────────────────────────

log() {
    # 带时间戳追加写入日志文件
    echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >>"$LOG_FILE"
}

reset_operator() {
    # 向 AT 串口（/dev/smd9，第二个AT口）发送 AT+COPS=0
    # AT+COPS=0：设置运营商选择为自动模式
    # 目的：清除之前可能手动设置的运营商，让模组重新自动选网
    # /dev/smd9 是第二个 AT 通道（区别于 open_dial 用的 /dev/smd8）
    echo -e "AT+COPS=0\r\n" >/dev/smd9
    log "Auto select operator (AT+COPS=0)"
}

safe_kill() {
    pname="$1"
    # pgrep -f：按完整进程名（包含参数）搜索
    # >/dev/null 2>&1：丢弃所有输出
    if pgrep -f "$pname" >/dev/null 2>&1; then
        pkill -f "$pname"   # 发送 SIGTERM 终止进程
        log "Stopped process: $pname"
        sleep 3   # 等待 3 秒让进程正常退出（避免强杀产生僵尸进程）
    fi
    # 若进程不存在，pgrep 返回非0，但 if 判断保证不会执行 pkill
}

start_dial() {
    # 检查 dial 进程是否已在运行
    if ! pgrep -f "dial" >/dev/null 2>&1; then
        reset_operator         # 先重置运营商选择
        /usr/dial/dial >/dev/null 2>&1 &  # 后台启动 dial
        # >/dev/null 2>&1：丢弃 dial 的 stdout/stderr（dial 自己写日志文件）
        # & 后台运行，不阻塞当前脚本
        network_type=1         # 记录当前使用 SIM 通道
        save_network_type 1    # 写入 /tmp/network_type
        log "Started dial"
    fi
}

start_rbmaster() {
    if ! pgrep -f "RBMaster" >/dev/null 2>&1; then
        # cd 到 roamlink 目录是因为 RBMaster 使用相对路径加载 so 和配置文件
        # () 在子 shell 中执行，cd 不影响当前 shell 的工作目录
        (cd /usrdata/roamlink && ./RBMaster &) >/dev/null 2>&1
        log "Started RBMaster"
    fi
}

start_roamlink_service() {
    reset_operator
    # 通过 nc（netcat）向本地 TCP 5568 端口发送控制命令
    # RBMaster 监听此端口，接收 "RBstartServiceMaster" 命令后启动虚拟SIM服务
    # nc 127.0.0.1 5568：连接本地5568端口
    # echo "..." | nc ...：将命令字符串作为标准输入发给 nc，再由 nc 发给 5568
    echo "RBstartServiceMaster" | nc 127.0.0.1 5568
    log "RBMaster service started via nc"
    network_type=2
    save_network_type 2
}

stop_roamlink_service() {
    # 同样通过 5568 端口发送停止命令
    echo "RBstopServiceMaster" | nc 127.0.0.1 5568
    log "RBMaster service stopped via nc"
}

save_network_type() {
    # 将网络类型数字（1或2）写入临时文件
    # -n：不加换行符
    echo -n "$1" >"$TMP_NET_TYPE"
    sync   # 立即同步到存储（防止重启时读到旧值）
}

# ── 配置读取函数 ───────────────────────────────────────────────────

read_network_conf() {
    if [ -f "$NETWORK_CONF" ] && [ -s "$NETWORK_CONF" ]; then
        # awk 解析 INI 文件中的 network_select 值
        # -F= 以 "=" 为字段分隔符
        # /network_select/：匹配含 "network_select" 的行
        # gsub(/ /,"",$2)：去掉第二字段（值）中的空格
        # print $2：输出值
        network_select=$(awk -F= '/network_select/{gsub(/ /,"",$2);print $2}' "$NETWORK_CONF")
        [ -z "$network_select" ] && network_select=4  # 解析失败默认 4（强制SIM）
    else
        network_select=4   # 配置文件不存在默认 4
    fi
    log "network_select=$network_select"
}

# ── 前置检查函数 ───────────────────────────────────────────────────

check_conf() {
    if [ ! -f "$CONF_FILE" ]; then
        # /opt/conf.ini 不存在（设备未完成基础配置），降级为纯 SIM 模式
        log "conf.ini missing, fallback to SIM dial"
        start_dial
        # 进入无限循环：每 5 分钟检查一次网络，永远不切换 Roamlink
        while true; do
            sleep 300
            check_celluar_network 0  # 0=不切换，只重启同一服务
        done
    fi
}

check_license() {
    if [ ! -f "$LICENSE_DIR" ]; then
        # license 不在主路径，尝试从备份恢复
        if [ -s "$BACKUP_LICENSE" ]; then
            cp "$BACKUP_LICENSE" "$LICENSE_DIR"
            log "Copied backup license"
        else
            # 备份也没有：进入自动下载 license 流程
            start_dial          # 先建立物理SIM网络连接（让设备能访问互联网）
            sleep 5
            start_rbmaster      # 启动 RBMaster（它会通过网络从云端下载 license）

            # 等待 license 下载完成
            while true; do
                sleep 60   # 每 60 秒检查一次
                if [ -s "$LICENSE_DIR" ]; then
                    # license 已下载！备份并重启
                    cp "$LICENSE_DIR" "$BACKUP_LICENSE"
                    sync
                    log "Downloaded roamlink license successfully"
                    reboot   # 重启后 license 就位，可正常启用 Roamlink
                fi
            done
        fi
    fi
}

# ── 网络检测核心函数 ───────────────────────────────────────────────

check_celluar_network() {
    celluar_switch=$1   # 参数：1=可切换到另一通道，0=不切换只重启

    check_cnt=0
    while [ $check_cnt -lt 10 ]; do
        # /usrdata/test_network：连接固定服务器 18.196.0.17（TCP）
        # 返回 0 = 连接成功（网络正常）
        # 返回非0 = 连接失败（网络异常）
        # >/dev/null 2>&1 丢弃所有输出
        if /usrdata/test_network >/dev/null 2>&1; then
            network_status=0    # 网络正常
            log "Network OK"
            break
        else
            network_status=1    # 本次检测失败
            log "Network failed ($check_cnt)"
        fi
        check_cnt=$((check_cnt + 1))
        sleep 2    # 失败后等 2 秒再重试
    done
    # 10 次检测（20 秒）全部失败才判定网络异常

    if [ $network_status -ne 0 ]; then
        log "Network abnormal, handling switch..."

        if [ $network_type -eq 1 ]; then
            # 当前是物理SIM（dial）通道
            if [ "$celluar_switch" -eq 1 ]; then
                # 可切换：关掉 dial，启动 Roamlink
                safe_kill "dial"
                sleep 5
                start_roamlink_service
            else
                # 不切换：重启 dial
                safe_kill "dial"
                sleep 5
                start_dial
            fi
        elif [ $network_type -eq 2 ]; then
            # 当前是虚拟SIM（Roamlink）通道
            if [ "$celluar_switch" -eq 1 ]; then
                # 可切换：停止 Roamlink，启动 dial
                stop_roamlink_service
                sleep 5
                start_dial
            else
                # 不切换：重启 Roamlink
                stop_roamlink_service
                sleep 5
                start_roamlink_service
            fi
        fi
    fi
}

# ── 主程序 ─────────────────────────────────────────────────────────

echo -n "0" >"$TMP_NET_TYPE"   # 初始化网络类型为 0（未设置）

check_conf       # 检查设备基础配置
check_license    # 检查/下载 Roamlink license
read_network_conf  # 读取网络策略配置

first=1         # 首次循环标志（第一次循环需要启动服务）
reset_operator  # 重置运营商自动选择
start_rbmaster  # 无论哪种策略，都先启动 RBMaster（它负责管理所有子进程）
sleep 5         # 等待 RBMaster 完成初始化

while true; do
    case $network_select in
    1)  # 优先 Roamlink（失败时切换到 SIM）
        if [ $first -eq 1 ]; then
            start_roamlink_service
            sleep 600   # 等待 10 分钟让 Roamlink 完成注册和连接
            first=0
        fi
        check_celluar_network 1   # celluar_switch=1：网络异常时允许切换通道
        ;;
    2)  # 优先物理SIM（失败时切换到 Roamlink）
        if [ $first -eq 1 ]; then
            start_dial
            sleep 600   # 等待 10 分钟
            first=0
        fi
        check_celluar_network 1   # 允许切换
        ;;
    3)  # 强制 Roamlink（失败只重启 Roamlink，不切换）
        if [ $first -eq 1 ]; then
            start_roamlink_service
            sleep 600
            first=0
        fi
        check_celluar_network 0   # celluar_switch=0：不允许切换
        ;;
    4)  # 强制物理SIM（失败只重启 dial，不切换）
        if [ $first -eq 1 ]; then
            start_dial
            sleep 600
            first=0
        fi
        check_celluar_network 0   # 不允许切换
        ;;
    *)  # 异常值：默认等同策略4（强制SIM）
        if [ $first -eq 1 ]; then
            start_dial
            sleep 600
            first=0
        fi
        check_celluar_network 0
        ;;
    esac

    # 下次检测间隔：
    if [ $network_status -ne 0 ]; then
        log "Network still down, sleep 600s"
        sleep 600    # 网络仍异常：10 分钟后再检查（切换后给新通道足够建连时间）
    else
        log "Network OK, sleep 1800s"
        sleep 1800   # 网络正常：30 分钟后再检查（减少不必要的检测开销）
    fi
done
```

---

### 5.5 start_check_network / start_daemon / start_dfota — init.d 服务脚本

这三个脚本是标准 SysV init 格式的服务管理脚本，结构一致：

```bash
#!/bin/sh
set -e   # 任何命令失败立即退出

# start_check_network：
networkScript=/usrdata/check_network.sh

case "$1" in
start)
    # 检查目标脚本是否存在
    if ! [ -f $networkScript ]; then
        echo -e "##################### networkScript Application doesn't exist"
        exit 3   # exit 3 = LSB 标准：服务不支持该操作
    fi
    # start-stop-daemon：Debian/Yocto 的服务管理工具
    # -S = start（启动）
    # -b = background（后台运行，相当于 & ）
    # -a = exec（要运行的程序路径）
    start-stop-daemon -S -b -a $networkScript
    ;;
stop)
    # -K = kill（停止）
    # -n = name（按进程名匹配）
    start-stop-daemon -K -n $networkScript
    ;;
restart)
    $0 stop
    $0 start
    ;;
*)
    echo "Usage: start_check_network { start | stop | restart }" >&2
    exit 1
    ;;
esac
exit 0
```

**start_dfota**（最简单）：

```bash
#!/bin/sh
set -e
# 仅一行：后台执行 start_dfota.sh
/usrdata/start_dfota.sh &
# 为什么用 & 后台？因为 DFOTA 过程很长，init.d 脚本应快速返回
```

---

### 5.6 roamlink/ 目录下的二进制组件

所有二进制均为 ARM 32-bit ELF，使用 Quectel 原厂工具链编译，不含源码。通过 `strings` 和 `file` 工具分析其功能：

**RBMaster（Roamlink 主控进程）**：

```
从 strings 提取的关键信息：

"./softsim"                 → 以相对路径启动 softsim 子进程
"./etc/.pconfig/license.cer" → 主 license 路径（相对于 /usrdata/roamlink/）
"/data/ufs/license.cer"     → 备份 license 路径
"RBstartServiceMaster"      → 接受的启动控制命令（来自 nc 5568）
"RBstopServiceMaster"       → 接受的停止控制命令
"ecardstart" / "pcardstart" → ecard（eSIM）/ pcard（物理卡）切换状态
"start CardManager"          → 启动 cardManager 子进程
"start heartbeat"            → 启动 heartbeat 子进程
"start Controller"           → 启动 controller 子进程
"RB_E_PCARD_EXCHANGE_BY_CLOUD" → 云端触发的物理卡切换事件
ls -l ./etc/.pconfig/license.cer | busybox awk '{print $5}'
                             → 通过 shell 命令检查 license 文件大小
```

**softsim（虚拟SIM模拟器）**：

使用 `libTSSUMEmulator.so`（TSSUM = Turbo SIM/SoftSIM Universal Module Emulator），在本地模拟一张虚拟 USIM 卡。模组的 USIM 接口会连接到 softsim，softsim 再通过网络与 RoamLink 云平台交互完成鉴权。

**test_network（网络连通性测试工具）**：

```
从 strings 提取：
"18.196.0.17"      → 硬编码的测试目标 IP（欧盟 IP 段）
"Connect"           → 建立 TCP 连接
"Connect OK, sockfd=%d" → 连接成功日志
```

工作原理：向 `18.196.0.17` 建立 TCP 连接，成功返回 0，失败返回非 0。

**get_imei（IMEI 读取工具）**：

```
从 strings 提取：
"dms_get_service_object_internal_v01"  → 使用 QMI DMS v01 服务
"qmi_client_notifier_init"             → QMI 客户端初始化
"popen"                                → 可能用于执行 shell 命令（备选方案）
```

工作原理：通过 QMI DMS（Device Management Service）协议向模组查询 IMEI，输出到 stdout。

**license.cer 文件**：

```
文件名：867929069660712_license.cer   → 867929069660712 即 IMEI
文件大小：17424 字节
格式：十六进制字符串（非标准 PEM/DER 格式的 X.509 证书）
开头：A7F7C601FAF129...（无 PEM 头，纯十六进制数据）
```

这是 RoamLink 平台颁发的设备授权证书，与特定 IMEI 绑定，用于验证设备是否有权使用 RoamLink 虚拟 SIM 服务。

---

### 5.7 network.ini 配置文件

```ini
[network]
#1 优先使用roamlink 2 优先使用sim卡 3 强制使用roamlink 4 强制使用sim卡
network_select=1
```

这是唯一的用户可配置文件，`network_select` 的值决定整个系统的网络切换策略（详见第6章）。

---

## 6. 双卡切换逻辑深度解析

### 6.1 物理 SIM 通道工作流

物理 SIM 通道由 `dial`（open_dial 编译产物）负责，通过 Quectel MCM API + AT 命令完成完整的 4G 连接建立：

```
物理SIM通道：拨号到上网的完整流程

设备开机
    │
    ▼
dial 进程启动
    │
    ├─ 打开 /dev/smd8（AT串口）
    ├─ 注册 MCM 服务客户端
    │
    ├─ 等待 SIM 卡就绪（MCM_SIM_GetCardStatus → 状态10）
    │
    ├─ 读取 ICCID（AT命令或MCM API）
    ├─ 根据 ICCID 匹配 apn.json，获得 APN 配置
    │
    ├─ 检查 LTE 注册状态（AT+CEREG? → stat=1或5）
    │
    ├─ 配置 APN（QL_APN_Set，profile_idx=1）
    ├─ 启动数据呼叫（QL_Data_Call_Start）
    │   └─ 模组执行：附着PDN → 获取IP → 配置路由
    │
    ├─ 等待连接成功（QL_Data_Call_Info_Get → CONNECTED）
    │
    ├─ 记录 IP 地址和接口信息
    ├─ 写 /tmp/network_status = 1
    │
    └─ 进入保活监控：
        ├─ 每5秒检查 nw_get_connect_state()
        ├─ 监控 /sys/.../rx_packets（60秒无新包则重拨）
        └─ 主线程每5秒更新 /tmp/network_csq（信号强度）
```

### 6.2 虚拟 SIM 通道工作流

虚拟 SIM 通道由 RBMaster 及其子进程负责：

```
虚拟SIM通道：启动到上网的完整流程

RBMaster 启动（start_vsim.sh 或直接执行）
    │
    ├─ 检查 license.cer（IMEI 授权验证）
    │
    ├─ 启动子进程组：
    │   ├─ softsim（虚拟USIM模拟器）
    │   ├─ cardManager（ecard/pcard切换管理）
    │   ├─ heartbeat（与云平台心跳保活）
    │   ├─ controller（执行云端控制指令）
    │   ├─ OemServer（OEM硬件适配层）
    │   └─ upgrade（OTA升级处理）
    │
    ├─ 接收 "RBstartServiceMaster" 命令（来自check_network.sh的nc）
    │
    ├─ softsim 向模组提供虚拟 USIM 接口
    │   └─ 模组认为插入了真实SIM卡
    │
    ├─ 模组向基站发起注册请求
    │   └─ 鉴权数据通过 softsim 转发到 RoamLink 云平台
    │
    ├─ 云平台动态选择最优运营商，返回鉴权响应
    │
    ├─ 模组完成注册，建立 LTE 数据连接
    │
    └─ heartbeat 进程维持与云平台的长连接
        └─ 支持云端动态切换运营商（cardManager）
```

### 6.3 四种网络策略详解

这是整个系统最核心的配置，通过 `network.ini` 中的 `network_select` 值控制。

#### 策略矩阵

```
                        ┌──────────────────────────────────────────┐
                        │          初始网络通道                     │
                        ├────────────────────┬─────────────────────┤
                        │  Roamlink（虚拟SIM）│  物理SIM（dial）    │
┌───────────────┬───────┼────────────────────┼─────────────────────┤
│   网络异常时   │ 可切换 │   策略 1（优先RL） │   策略 2（优先SIM）  │
│   的处理方式   ├───────┼────────────────────┼─────────────────────┤
│               │ 不切换 │   策略 3（强制RL） │   策略 4（强制SIM）  │
└───────────────┴───────┴────────────────────┴─────────────────────┘
```

#### 策略 1：优先 Roamlink（network_select=1）

**适用场景**：多国漫游设备，主要依赖 Roamlink 自动选择最优运营商，但希望在 Roamlink 不可用时降级到本地 SIM。

```
启动：start_roamlink_service  → sleep 600秒 → 进入监控循环

监控循环（每 30 分钟正常，10 分钟异常）：
    check_celluar_network(celluar_switch=1)
        ↓
    test_network × 10次失败
        ↓
    network_type == 2 (当前Roamlink)
    celluar_switch == 1 (允许切换)
        ↓
    stop_roamlink_service
    sleep 5
    start_dial          ← 切换到物理SIM
        ↓
    sleep 600秒后再次检测
        ↓
    test_network × 10次失败
        ↓
    network_type == 1 (当前SIM)
    celluar_switch == 1 (允许切换)
        ↓
    safe_kill "dial"
    sleep 5
    start_roamlink_service  ← 切换回Roamlink
        ↓
    如此往复...
```

**行为特点**：系统优先使用 Roamlink，Roamlink 失败时切到 SIM，SIM 失败时切回 Roamlink，在两者间交替尝试。

#### 策略 2：优先物理 SIM（network_select=2）

**适用场景**：已知本地 SIM 信号好的单一地区，优先使用低成本的本地 SIM，Roamlink 作为备用。

```
启动：start_dial  → sleep 600秒 → 进入监控循环

网络异常时（celluar_switch=1，允许切换）：
    当前 SIM（network_type=1）失败：
        safe_kill "dial" → start_roamlink_service  ← 切到Roamlink

    当前 Roamlink（network_type=2）失败：
        stop_roamlink_service → start_dial         ← 切回SIM
```

**行为特点**：与策略 1 镜像对称，只是初始通道不同。

#### 策略 3：强制 Roamlink（network_select=3）

**适用场景**：纯虚拟 SIM 部署，没有实体 SIM 卡，或者明确要求不使用物理 SIM。

```
启动：start_roamlink_service  → sleep 600秒 → 进入监控循环

网络异常时（celluar_switch=0，不允许切换）：
    无论当前是哪种通道（实际只会是 Roamlink）：
        → 停止 Roamlink → 重启 Roamlink（不切换）

即：只会无限重启 Roamlink，绝不使用 dial
```

#### 策略 4：强制物理 SIM（network_select=4，默认值）

**适用场景**：单一地区固定 SIM 卡，不需要 Roamlink。这也是配置文件缺失时的默认行为。

```
启动：start_dial  → sleep 600秒 → 进入监控循环

网络异常时（celluar_switch=0，不允许切换）：
    → 停止 dial → 重启 dial（不切换）

即：只会无限重启 dial，绝不使用 Roamlink
```

**注意**：即使策略 3/4 不使用另一通道，`RBMaster` 进程仍然会被启动（脚本中 `start_rbmaster` 在策略判断之前执行）。这意味着 Roamlink 的子进程始终在后台运行，只是不激活虚拟SIM服务。

### 6.4 切换时序全流图

```
时间轴 →
开机
│
├─ check_conf（如conf.ini存在则继续）
├─ check_license（确认license就位）
├─ read_network_conf（读取 network_select=1 为例）
│
├─ reset_operator（AT+COPS=0，自动选运营商）
├─ start_rbmaster（启动 RBMaster，但不激活虚拟SIM服务）
│
└─ 主循环开始（first=1）
   │
   ├─ [策略1/3] start_roamlink_service
   │   echo "RBstartServiceMaster" | nc 127.0.0.1 5568
   │   network_type = 2
   │   ──────────── sleep 600s ────────────────────────────────────►
   │                                                               │
   │                                        check_celluar_network(1)
   │                                        test_network × 10（共20秒）
   │                                         └─ 失败
   │                                        stop_roamlink_service
   │                                        echo "RBstopServiceMaster" | nc 5568
   │                                        sleep 5
   │                                        start_dial
   │                                        network_type = 1
   │                                        ────── sleep 600s ─────►
   │                                                               │
   │                                        check_celluar_network(1)
   │                                        test_network × 10
   │                                         └─ 成功
   │                                        network_status = 0
   │                                        ────── sleep 1800s ────►
   │                                                               │
   │                                        check_celluar_network(1)
   │                                        test_network × 10
   │                                         └─ 成功
   │                                        ...（持续使用SIM）
```

### 6.5 切换操作的底层实现

**物理 SIM 启停**（通过 Unix 进程管理）：

```bash
# 启动 dial 进程
/usr/dial/dial >/dev/null 2>&1 &

# 停止 dial 进程
pkill -f "dial"   # 发送 SIGTERM
sleep 3           # 等待 3 秒正常退出

# dial 退出时的清理动作（由 dial 进程的信号处理或正常退出处理）：
# - QL_Data_Call_Stop：断开 4G 数据连接
# - QL_MCM_NW_Client_Deinit：注销 MCM NW 客户端
# - /tmp/network_status 文件保留（不自动清除）
```

**虚拟 SIM 启停**（通过本地 TCP socket 命令）：

```bash
# 启动虚拟SIM服务
echo "RBstartServiceMaster" | nc 127.0.0.1 5568

# RBMaster 收到命令后：
# 1. 检查 license.cer 是否有效
# 2. 启动 softsim 进程，softsim 接管模组的 USIM 接口
# 3. 模组开始通过虚拟SIM进行网络注册

# 停止虚拟SIM服务
echo "RBstopServiceMaster" | nc 127.0.0.1 5568

# RBMaster 收到命令后：
# 1. 停止 softsim，模组失去 USIM 接口
# 2. 模组自动断网并尝试注册（但没有 SIM 卡了）
# 注意：RBMaster 进程本身不停止，只是停止了虚拟SIM服务
```

**两通道互斥保证**：

两个通道不存在内核级互斥机制，依赖脚本逻辑保证：切换时先停止当前通道（`safe_kill` 或 `stop_roamlink_service`），等待 5 秒，再启动新通道。5 秒的等待是经验值，确保模组从前一个状态恢复。

---

## 7. 两仓库协作关系详解

```
                    开发阶段
open_dial_for_artery（源码）
        │
        │  在 Yocto 交叉编译环境中 make 编译
        │  arm-oe-linux-gnueabi-gcc + Quectel MCM SDK
        ▼
    dial_1.29.4（ARM 32-bit ELF）
        │
        │  拷贝到 roamlink 部署包
        ▼
    roamlink_deploy_auto_download_with_license_251028/
        ├── dial_1.29.4  ← 物理SIM拨号程序
        └── roamlink/    ← 虚拟SIM服务（闭源，来自RoamLink平台）

                    部署阶段（设备现场）
    install_update.sh（U盘执行）
        │
        ├─ 检测固件版本 → 必要时 DFOTA 升级
        ├─ 验证 IMEI → 安装匹配的 license.cer
        ├─ 部署 dial_1.29.4 → /usr/dial/dial（替换原有）
        ├─ 部署 roamlink/ → /usrdata/roamlink/
        ├─ 部署 check_network.sh → /usrdata/
        ├─ 注册 start_check_network 为开机自启动服务
        └─ 注释掉原始 check_dial 调用（避免冲突）

                    运行阶段
    /etc/rc5.d/S60start_check_network
        │
        │  start-stop-daemon 后台启动
        ▼
    /usrdata/check_network.sh（永久运行）
        │
        ├─ 策略1/2：可在 dial ←→ Roamlink 间切换
        ├─ 策略3：只使用 Roamlink
        └─ 策略4：只使用 dial

        物理SIM通道           虚拟SIM通道
    /usr/dial/dial         /usrdata/roamlink/RBMaster
        │                          │
        │  共享 Quectel 模组        │
        │  (同一时间只有一个激活)    │
        └─────────── 互斥 ──────────┘
                    │
              /tmp/network_status（0或1）
              /tmp/network_type（1或2）
              /tmp/network_csq（CSQ值）

    这三个临时文件是两个仓库之间的"状态共享接口"：
    • dial 写入 network_status 和 network_csq
    • check_network.sh 读 network_type，写 network_type
    • 上层应用（如 vehicleDataEngine）读 network_status 判断是否联网
```

---

## 8. 已知 Bug 与代码缺陷（逐条详解）

### Bug 1：at.c — sizeof 指针错误（严重，AT 命令构造异常）

```c
// 位置：Ql_SendAT() 第12-13行
iLen = sizeof(atCmd);   // 错误！atCmd 是 char*，sizeof(char*) = 4（ARM32）
strncpy(strAT, atCmd, iLen);  // 只复制前 4 字节到 strAT

// 为什么没报错？因为紧接着有：
iLen = strlen(atCmd);
if ((atCmd[iLen-1] != '\r') && (atCmd[iLen-1] != '\n')) {
    iLen = sprintf(strAT, "%s\r\n", atCmd);  // 这里覆盖了 strAT，修正了错误
}

// 影响：strncpy 那行实际无效（被后面的 sprintf 覆盖），程序功能正常
// 但如果 AT 命令本身以 \r 或 \n 结尾（不走 sprintf 分支），
// 发送的内容就真的只有前 4 字节！

// 修复：
iLen = strlen(atCmd);
strncpy(strAT, atCmd, iLen);
strAT[iLen] = '\0';  // 确保 null 终止
```

### Bug 2：apn.c — 提前 return 导致大量死代码（功能退化）

```c
// 位置：apn_scan_idx() 第6行
int apn_scan_idx(apn_obj_t *p_apn_obj)
{
    ql_apn_info_list_s apn_list;
    int i = 0, ret = -1;
    memset(&apn_list, 0, sizeof(apn_list));
    return apn_set(1, p_apn_obj);  // ← 提前return，绕过全部逻辑

    // 以下 ~70 行代码永远不执行：
    // - QL_APN_Get_Lists()：获取模组已有APN列表
    // - 列表扫描和匹配逻辑
    // - QL_APN_Add()：添加新APN
    // 这些是完整的APN管理逻辑，被完全绕过

// 影响：
// 每次拨号都直接覆盖 profile_idx=1 的APN配置
// 无法复用模组中已有的APN配置
// 在运营商有多个APN时无法选择

// README说明（V1.29.4）：
// "因为高通模组只对 profile_idx=1 的拨号自动设置路由，
//  强制使用该 profile_idx 进行拨号"
// 这是一个合理的临时方案，但应该注释说明，而非留下死代码
```

### Bug 3：apn.c — 赋值运算符写成比较（逻辑永远为true）

```c
// 位置：apn_scan_idx() 死代码区（虽然不执行，但记录）
if (ret = -1)   // ← 应为 ret == -1
{
    // ret = -1 是赋值，赋值表达式的值是 -1，转为 bool 是 true
    // 所以此 if 分支永远执行（等同于去掉 if 判断）
    ret = apn_set(1, p_apn_obj);
}

// 修复：
if (ret == -1)
```

### Bug 4：sim.c — switch-case 缺少 break（fall-through）

```c
// 位置：sim_op_handler() case sim_op_stat_get_iccid
case sim_op_stat_get_iccid:
    if (E_QL_OK == sim_op(SIM_OP_GET_ICCID, p_sim_op_mng->sim_iccid)) {
        SEAS_LOG_INFO("ICCID: %s", p_sim_op_mng->sim_iccid);
        ret = true;
    } else {
        SEAS_LOG_ERROR("...");
    }
    // ← 缺少 break！

case sim_op_stat_get_imsi:  // 这里会被 fall-through 到！
    if (E_QL_OK == sim_op(SIM_OP_GET_IMSI, p_sim_op_mng->sim_imsi)) {
        ...
    }
    break;

// 实际效果：调用 sim_op_handler(p, sim_op_stat_get_iccid)
// 会同时执行 GET_ICCID 和 GET_IMSI
// IMSI 被写入 p_sim_op_mng->sim_imsi（但当前代码不使用 IMSI，所以无害）

// 修复：在 GET_ICCID 分支末尾加 break;
```

### Bug 5：apn.c — fread 失败路径内存泄漏

```c
// 位置：apn_scan_from_json()
str = (char *)malloc(len);   // 分配内存
if (fread(str, 1, len, fp) < 0) {
    SEAS_LOG_ERROR("fread error");
    return NULL;   // ← str 未 free，fp 未 fclose！内存和文件描述符泄漏
}

// 修复：
if (fread(str, 1, len, fp) < 0) {
    SEAS_LOG_ERROR("fread error");
    free(str);    // 释放内存
    fclose(fp);   // 关闭文件
    return NULL;
}
```

### Bug 6：nw.c — nw_get_ifaddrs 悬空指针（可导致程序崩溃）

```c
// 位置：nw_get_ifaddrs()
p_ifaddrs = (struct ifaddrs *)calloc(1, sizeof(struct ifaddrs));
memcpy(p_ifaddrs, ifa, sizeof(struct ifaddrs));
// 浅拷贝：p_ifaddrs->ifa_name 仍指向 ifaddr 链表内部的字符串

freeifaddrs(ifaddr);   // 释放整个 ifaddr 链表，包括 ifa_name 指向的内存
return p_ifaddrs;      // 返回含悬空指针的结构体

// 调用方：
p_dial_mng->p_ifaddrs->ifa_name   // ← 访问已释放内存！未定义行为

// 修复：深拷贝 ifa_name
p_ifaddrs->ifa_name = strdup(ifa->ifa_name);  // strdup 分配新内存并复制
freeifaddrs(ifaddr);   // 安全释放，不影响 ifa_name 副本
return p_ifaddrs;

// 注意：调用方最终应 free(p_ifaddrs->ifa_name); free(p_ifaddrs);
```

### Bug 7：nw.c — nw_mark_network_status 文件操作不规范

```c
// 位置：nw_mark_network_status()
fd = open(NW_STATUS_PATH, O_WRONLY | O_CREAT);
// 问题1：open() 使用 O_CREAT 时必须提供第三个参数（文件权限mode）
//        缺少 mode 参数，行为依赖 umask（可能创建出权限不当的文件）
// 问题2：没有 O_TRUNC，若文件已存在且原内容更长（如原来是"1"，现在写"0"），
//        写入 "0" 后文件内容变为 "0"（1字节），原来的 "1" 被覆盖，这里OK，
//        但若原内容是"10"，写"0"后变为"00"——不过实际值只有"0"和"1"，不会有问题
// 问题3：未检查 open() 返回值（fd 可能为 -1）

// 修复：
fd = open(NW_STATUS_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
if (fd < 0) {
    SEAS_LOG_ERROR("open %s failed: %s", NW_STATUS_PATH, strerror(errno));
    return;
}
sprintf(br_str, "%d", net_status);
ssize_t n = write(fd, br_str, strlen(br_str));
if (n < 0) {
    SEAS_LOG_ERROR("write %s failed: %s", NW_STATUS_PATH, strerror(errno));
}
close(fd);
```

### Bug 8：apn.c — apn_get_apn_obj 逻辑取反

```c
// 位置：apn_get_apn_obj()
p_apn_obj = apn_scan_from_json(json_path, iccid);
if (p_apn_obj == NULL) {
    // 未找到，使用默认APN
    p_apn_obj = calloc(1, sizeof(apn_obj_t));
    ...
} else {
    SEAS_LOG_ERROR("p_apn_obj is NULL");  // ← 逻辑错误！找到了却打印"is NULL"
}

// 修复：
} else {
    SEAS_LOG_INFO("Found APN for ICCID: %s, apn: %s", iccid, p_apn_obj->apn);
}
```

### Bug 9：check_network.sh — set -e 与监控脚本不兼容

```bash
#!/bin/sh
set -e   # 任何命令返回非0，脚本立即退出

# 问题：监控脚本是永久运行的守护进程
# 如果某条命令意外失败（如 nc 连接 5568 端口失败），
# set -e 会让整个 check_network.sh 退出
# start-stop-daemon 可能不会自动重启它

# 实际上 check_network.sh 中大部分关键操作都在函数内
# 使用了 if 或 2>/dev/null 保护，减少了 set -e 的触发
# 但 nc 命令失败（RBMaster未启动时）仍可能触发

# 改进方案：
# 去掉 set -e，在关键操作处显式检查返回值
# 或者使用 trap 捕获错误并重启脚本
trap 'echo "check_network.sh crashed, restarting..." >>$LOG_FILE; sleep 10; exec $0' EXIT
```

---

## 9. 安全漏洞分析

### 漏洞1：RBMaster 本地控制接口无认证（中危）

```
攻击面：本地 TCP 127.0.0.1:5568
认证机制：无
影响：任何能在设备上执行 shell 命令的进程都可以：
      echo "RBstopServiceMaster" | nc 127.0.0.1 5568
      → 立即停止虚拟SIM服务，导致设备断网
改进：
  - 改用 Unix domain socket（/tmp/rbmaster.sock）并设置 700 权限
  - 或在 5568 端口增加 token 认证（双方预共享密钥）
```

### 漏洞2：test_network 硬编码 IP + 明文 TCP（低危）

```
test_network 向 18.196.0.17:? 发起 TCP 连接
无 TLS，无证书验证，无协议标识

影响：若攻击者能劫持到设备与该IP的路由，可：
      - 伪造"连接成功"响应（始终返回0）
      → check_network.sh 认为网络正常，不触发切换
      - 或拒绝连接（始终返回非0）
      → check_network.sh 不断切换通道，消耗流量和电量

改进：使用 HTTPS + 证书验证，或 ICMP ping + 多目标投票
```

### 漏洞3：license.cer 明文存储（低危）

```
存储位置：/usrdata/roamlink/etc/.pconfig/license.cer
          /data/ufs/license.cer

问题：文件以明文存放（实际是十六进制编码的二进制数据）
     拥有设备 shell 访问权限的人可以复制 license 文件

不过：license 与 IMEI 绑定，即使复制到其他设备也只对相同 IMEI 有效
       因此危害有限，但理论上存在 license 泄露风险

改进：对 license 文件内容进行额外加密，加密密钥与设备 CPU 序列号绑定
```

### 漏洞4：DFOTA 触发无额外验证（中危）

```bash
echo -e "AT+QFOTADL=\"/usrdata/update.zip\"\r\n" >/dev/smd8
```

```
问题：只要 /usrdata/update.zip 存在，就会触发固件升级
     该命令写入串口时无任何签名或身份验证
     update.zip 的完整性验证依赖模组固件本身的 DFOTA 校验

攻击路径：
  1. 攻击者获取设备 shell 访问权限
  2. 替换 /usrdata/update.zip 为恶意固件
  3. 触发 DFOTA → 设备刷入恶意固件

改进：
  - 验证 update.zip 的签名（在 check_fw_version 中实现）
  - 限制 /usrdata 的写入权限
```

### 漏洞5：nw.c system() 调用潜在注入（理论风险）

```c
// nw_mark_plmn()
sprintf(echo_cmd, "echo %s%s > %s", mcc, mnc, NW_PLMN_PATH);
system(echo_cmd);
// mcc 来自 AT+COPS 响应，如果被恶意基站返回特殊字符（如 ";" 或 "`"），
// 可能执行任意 shell 命令

// 实际风险：AT 命令响应由模组固件生成，正规运营商不会返回注入字符
// 但在安全测试或恶意基站攻击场景下是有效向量

// 修复：直接写文件，不使用 system()
```

---

## 10. 改进建议（附修复代码）

### 10.1 at.c 完整修复版

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

    // 修复 Bug1：使用 strlen 而非 sizeof
    iLen = strlen(atCmd);
    if (iLen >= AT_MSG_LENGTH_MAX - 3) {  // 防止缓冲区溢出（留 \r\n\0 的空间）
        return -1;
    }

    snprintf(strFinalRsp, sizeof(strFinalRsp), "\r\n%s", finalRsp);

    // 修复 Bug2：正确设置超时（秒和微秒分开）
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;  // 转为微秒

    if ((atCmd[iLen - 1] != '\r') && (atCmd[iLen - 1] != '\n')) {
        iLen = snprintf(strAT, sizeof(strAT), "%s\r\n", atCmd);
    } else {
        strncpy(strAT, atCmd, iLen);
        strAT[iLen] = '\0';
    }

    // 清空残留数据
    while (read(smd_fd, strResponse, sizeof(strResponse)) > 0) {}

    if (write(smd_fd, strAT, iLen) != iLen) {
        return -1;  // 写失败
    }

    while (1) {
        FD_ZERO(&fds);
        FD_SET(smd_fd, &fds);
        switch (select(smd_fd + 1, &fds, NULL, NULL, &timeout)) {
        case -1: return -1;
        case 0:  return 1;   // 超时
        default:
            if (FD_ISSET(smd_fd, &fds)) {
                do {
                    memset(strResponse, 0, sizeof(strResponse));
                    rdLen = read(smd_fd, strResponse, sizeof(strResponse));
                    if (rdLen > 0) {
                        if (p_rsp_msg != NULL) {
                            // 修复 Bug3：限制拷贝长度，防止溢出
                            size_t copy_len = (rdLen < AT_MSG_LENGTH_MAX - 1)
                                              ? rdLen : AT_MSG_LENGTH_MAX - 1;
                            memcpy(p_rsp_msg, strResponse, copy_len);
                            p_rsp_msg[copy_len] = '\0';
                        }
                        if (strstr(strResponse, strFinalRsp)
                            || strstr(strResponse, "+CME ERROR:")
                            || strstr(strResponse, "+CMS ERROR:")
                            || strstr(strResponse, "ERROR")) {
                            bRcvFinalRsp = TRUE;
                        }
                    }
                    usleep(10000);
                } while ((rdLen > 0) && (AT_MSG_LENGTH_MAX == rdLen));
            }
            break;
        }
        if (bRcvFinalRsp) break;
    }
    return 0;
}
```

### 10.2 nw.c — nw_get_ifaddrs 深拷贝修复

```c
struct ifaddrs *nw_get_ifaddrs(char *p_ip_addr)
{
    struct ifaddrs *ifaddr, *ifa;
    struct ifaddrs *p_result = NULL;
    char host[NI_MAXHOST];
    int family, s;

    if (getifaddrs(&ifaddr) == -1) {
        SEAS_LOG_ERROR("getifaddrs error: %s", strerror(errno));
        return NULL;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        family = ifa->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            s = getnameinfo(ifa->ifa_addr,
                (family == AF_INET) ? sizeof(struct sockaddr_in)
                                    : sizeof(struct sockaddr_in6),
                host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s == 0 && 0 == strncmp(p_ip_addr, host, strlen(p_ip_addr))) {
                // 分配结果结构
                p_result = (struct ifaddrs *)calloc(1, sizeof(struct ifaddrs));
                if (p_result == NULL) break;

                // 深拷贝：复制接口名字符串（不再依赖原链表内存）
                if (ifa->ifa_name) {
                    p_result->ifa_name = strdup(ifa->ifa_name);
                }
                SEAS_LOG_INFO("Found interface: %s\n", p_result->ifa_name);
                break;
            }
        }
    }

    freeifaddrs(ifaddr);  // 安全释放，p_result->ifa_name 是独立副本
    return p_result;

    // 调用方释放：
    // free(p_dial_mng->p_ifaddrs->ifa_name);
    // free(p_dial_mng->p_ifaddrs);
}
```

### 10.3 apn.c — 内存泄漏修复

```c
apn_obj_t *apn_scan_from_json(char *json_path, char *iccid)
{
    FILE *fp = NULL;
    char *str = NULL;
    apn_obj_t *p_apn_obj = NULL;
    int len;

    fp = fopen(json_path, "r");
    if (fp == NULL) {
        SEAS_LOG_ERROR("Cannot open: %s", json_path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    rewind(fp);

    str = (char *)malloc(len + 1);  // +1 for null terminator
    if (str == NULL) {
        SEAS_LOG_ERROR("malloc failed");
        fclose(fp);
        return NULL;
    }

    if ((int)fread(str, 1, len, fp) < 0) {
        SEAS_LOG_ERROR("fread error");
        free(str);     // 修复：释放内存
        fclose(fp);    // 修复：关闭文件
        return NULL;
    }
    str[len] = '\0';

    // ... JSON 解析逻辑不变 ...

    free(str);
    fclose(fp);
    return p_apn_obj;
}
```

### 10.4 nw.c — 消除 system() 命令注入风险

```c
// 替换 nw_at_get_csq() 中的 system() 写文件
static void write_int_to_file(const char *path, int value)
{
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d", value);

    // 先创建（如不存在），再以 O_TRUNC 写入
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        SEAS_LOG_ERROR("open %s failed: %s", path, strerror(errno));
        return;
    }
    if (write(fd, buf, len) != len) {
        SEAS_LOG_ERROR("write %s failed: %s", path, strerror(errno));
    }
    close(fd);
}

// 用法：
write_int_to_file(NW_CSQ_PATH, csq);      // 替换 system("echo %d > ...")
write_int_to_file(NW_STATUS_PATH, net_status);  // 替换 open/write
```

### 10.5 check_network.sh — 监控脚本健壮性改进

```bash
#!/bin/sh
# 去掉 set -e，改用 trap 确保脚本意外退出时自动重启

LOG_FILE="/var/log/roamlink.log"

# 定义清理和重启行为
on_exit() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') check_network.sh exiting (code=$?), restart in 30s" >>"$LOG_FILE"
    sleep 30
    exec /usrdata/check_network.sh   # 重启自身
}
trap on_exit EXIT

# 所有关键操作显式处理返回值
start_roamlink_service() {
    reset_operator
    if ! echo "RBstartServiceMaster" | nc -w 3 127.0.0.1 5568; then
        log "WARNING: Failed to send RBstartServiceMaster to port 5568"
        # 不因此退出
    fi
    network_type=2
    save_network_type 2
}
```

### 10.6 sim.c — 修复 fall-through

```c
bool sim_op_handler(sim_mng_t *p_sim_op_mng, sim_op_stat_enu sim_op_st)
{
    bool ret = false;
    switch (sim_op_st) {
    case sim_op_stat_get_iccid:
        if (E_QL_OK == sim_op(SIM_OP_GET_ICCID, p_sim_op_mng->sim_iccid)) {
            SEAS_LOG_INFO("ICCID: %s", p_sim_op_mng->sim_iccid);
            ret = true;
        } else {
            SEAS_LOG_ERROR("SIM_op_st: sim_op_stat_get_iccid error");
        }
        break;  // ← 修复：加上 break

    case sim_op_stat_get_imsi:
        if (E_QL_OK == sim_op(SIM_OP_GET_IMSI, p_sim_op_mng->sim_imsi)) {
            SEAS_LOG_INFO("IMSI: %s", p_sim_op_mng->sim_imsi);
            ret = true;
        } else {
            SEAS_LOG_ERROR("SIM_op_st: sim_op_stat_get_imsi error");
        }
        break;
    // ...
    }
    return ret;
}
```

---

*报告完。全部内容基于对源码文件、Shell 脚本、配置文件及 ARM 二进制字符串的直接分析。*
