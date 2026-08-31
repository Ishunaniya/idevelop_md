# open_dial_for_artery × roamlink_deploy 深度源码分析报告

> **分析基础**：全部结论均来自对两个仓库实际源码（C/Shell/配置/二进制字符串）的直接阅读，不含推测性描述。  
> **时间**：2025年10月（roamlink版本号 `roamlink_deploy_auto_download_with_license_251028`）  
> **目标平台**：Quectel EC2x/EG2x 模组，ARM32 嵌入式 Linux（EABI5，glibc 2.4+）

---

## 目录

1. [仓库总览与关系](#1-仓库总览与关系)
2. [open_dial_for_artery 深度分析](#2-open_dial_for_artery-深度分析)
   - 2.1 整体架构
   - 2.2 模块说明
   - 2.3 状态机全流图
   - 2.4 关键数据结构
   - 2.5 核心逻辑详解
3. [roamlink_deploy 深度分析](#3-roamlink_deploy-深度分析)
   - 3.1 整体架构
   - 3.2 组件说明
   - 3.3 安装流程
   - 3.4 运行时流程与网络切换逻辑
4. [两仓库关系与协作机制](#4-两仓库关系与协作机制)
5. [已知 Bug 与代码缺陷](#5-已知-bug-与代码缺陷)
6. [安全漏洞分析](#6-安全漏洞分析)
7. [改进建议](#7-改进建议)

---

## 1. 仓库总览与关系

| 属性 | open_dial_for_artery | roamlink_deploy_auto_download_with_license_251028 |
|------|---------------------|--------------------------------------------------|
| **性质** | C 语言源码项目 | 嵌入式设备部署包（Shell + 闭源 ARM 二进制） |
| **核心功能** | Quectel 模组 4G/5G 拨号守护进程 | 虚拟 SIM（eSIM 云服务）+ 物理 SIM 双轨管理 |
| **当前版本** | V1.29.6（源码）/ V1.29.4（编译产物） | 2025-10-28 |
| **目标架构** | ARM Cortex-A7（EABI5） | ARM 32-bit Linux（ld-linux.so.3） |
| **编译工具链** | `arm-oe-linux-gnueabi-gcc` | 预编译闭源二进制 |
| **依赖** | Quectel MCM SDK、QL OpenLinux API | Quectel 模组固件、RoamLink 云平台 |

**核心关系**：`open_dial` 编译产物 `dial_1.29.4` 被直接打包进 roamlink 部署包，作为物理 SIM 拨号通道的执行程序，与 roamlink 虚拟 SIM 服务共存并受 `check_network.sh` 统一调度。

---

## 2. open_dial_for_artery 深度分析

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                      main.c (主进程)                     │
│  ┌──────────────────┐   ┌─────────────────────────────┐ │
│  │  seas_log 配置    │   │   主线程：每5秒轮询 AT+CSQ   │ │
│  │  (sdcard检测)     │   │   写入 /tmp/network_csq     │ │
│  └──────────────────┘   └─────────────────────────────┘ │
│                                │                         │
│                   pthread_create(dial_task)               │
│                                │                         │
│  ┌─────────────────────────────▼─────────────────────┐  │
│  │              dial_task (独立线程)                   │  │
│  │         状态机驱动的完整拨号生命周期                │  │
│  └─────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
         │              │              │              │
    ┌────▼────┐   ┌─────▼────┐  ┌────▼────┐   ┌────▼────┐
    │  nw.c   │   │  sim.c   │  │  apn.c  │   │  at.c   │
    │ 网络状态 │   │ SIM 操作 │  │ APN 配置│   │ AT 指令 │
    │ MCM API │   │ MCM API  │  │ cJSON   │   │ smd 串口│
    └─────────┘   └──────────┘  └─────────┘   └─────────┘
         │
    ┌────▼────┐   ┌──────────┐  ┌──────────┐
    │  tz.c   │   │seas_log.c│  │cc_deque.c│
    │ 时区获取 │   │ 日志系统 │  │ 双端队列 │
    └─────────┘   └──────────┘  └──────────┘
```

进程采用**双线程模型**：
- **主线程**：无限循环，每 5 秒调用 `nw_at_get_csq()` 获取信号强度写入 `/tmp/network_csq`
- **dial_task 线程**：状态机驱动，负责完整的连接生命周期管理

两线程共享 AT 串口 fd（`smd_fd`），通过 `pthread_mutex_t g_at_port_mutex` 互斥访问。

### 2.2 模块说明

#### 2.2.1 at.c — AT 指令收发层

核心函数 `Ql_SendAT(smd_fd, atCmd, finalRsp, timeout_ms, p_rsp_msg)`：

1. 先执行一次 `read()` 清空 smd 缓冲区中的 URC 残留
2. 拼接 `\r\n` 后 `write()` 发送指令
3. 用 `select()` 等待响应，超时返回 `1`，错误返回 `-1`，成功返回 `0`
4. 响应匹配逻辑：同时检查 `finalRsp`、`+CME ERROR`、`+CMS ERROR`、`ERROR`

`at_init()` 打开 `/dev/smd8`（`QUEC_AT_PORT`），依次发送 `ATE0 / AT / ATI` 进行初始化。

#### 2.2.2 sim.c — SIM 卡操作层

封装 Quectel MCM SIM API，支持四类操作（`SIM_OP_INIT / DEINIT / GET_IMSI / GET_ICCID`），全部通过 `sim_op(cmdIdx, buf)` 统一调度。`sim_op_handler()` 提供状态枚举驱动的高层接口。

#### 2.2.3 nw.c — 网络状态层

关键职责：

| 函数 | 功能 | 输出路径 |
|------|------|---------|
| `nw_get_sim_card_status()` | SIM 卡就绪检测（状态值 offset `0xB00`，`SIM_CARD_READY=10`） | — |
| `nw_reg_status_check()` | PS 域注册状态检查（MCM NW API） | — |
| `nw_at_get_cereg_stat()` | AT+CEREG? 查询 LTE 注册状态（stat=1 或 5 为合法） | — |
| `nw_at_get_csq()` | AT+CSQ 读取信号强度 | `/tmp/network_csq` |
| `nw_mark_network_status()` | 写入网络连通标志（0/1） | `/tmp/network_status` |
| `nw_get_if_statistics_rx_packets()` | 读取 `/sys/.../statistics/rx_packets` | — |
| `nw_get_ifaddrs()` | 按 IP 匹配网络接口信息 | — |

#### 2.2.4 apn.c — APN 配置层

从 `/usr/dial/apn.json` 按 ICCID 前缀匹配 APN 配置：

```json
{
  "apn": [
    {
      "iccid": "898600...",
      "supplier": "ChinaMobile",
      "apn": "cmnet",
      "usrname": "",
      "pwd": "",
      "is_oper_select": false
    }
  ]
}
```

支持 `oper` 字段（指定允许运营商号码列表），通过 `cc_deque` 队列管理候选运营商切换。

#### 2.2.5 dial.c — 核心状态机

见 2.3 节。

#### 2.2.6 seas_log.c — 日志系统

自定义轮转日志：文件超过 `max_file_size`（最大 50MB）时覆写重建，支持 DEBUG/INFO/ERROR/SILENT 四级。实际记录触发条件：`/media/sdcard` 存在且空闲空间 ≥ 1GB。使用 `pthread_mutex` 保护文件写入。

#### 2.2.7 tz.c — 时区

通过 `AT+QLTS=1` 获取网络时区，用 iniparser 写入 `/usr/dial/tz.ini`，格式 `[dev]\ntz = 32`（值 ÷ 4 = 时区偏移小时）。

### 2.3 状态机全流图

```
        ┌──────────────────────────────────────────┐
        │              dial_stat_none              │
        └──────────────────┬───────────────────────┘
                           │ 立即
        ┌──────────────────▼───────────────────────┐
        │              dial_stat_init              │
        │  QL_MCM_NW_Client_Init()                 │
        └──────────────────┬───────────────────────┘
                   成功    │    失败→重试(sleep 无限)
        ┌──────────────────▼───────────────────────┐
        │            dial_stat_sim_init             │
        │  sim_op(SIM_OP_INIT)                     │
        └──────────────────┬───────────────────────┘
                   成功    │    失败→sleep(2)→重试
        ┌──────────────────▼───────────────────────┐
        │            dial_stat_sim_check            │ ◄──┐
        │  nw_get_sim_card_status()                │    │
        │  超时 > 3600s → dial_stat_stop_cfun      │    │
        └──────────────────┬───────────────────────┘    │
                   成功    │    失败→sleep(2)→重试      │
        ┌──────────────────▼───────────────────────┐    │
        │             dial_stat_sim_op              │    │
        │  sim_op_handler(sim_op_stat_get_iccid)   │    │
        └──────────────────┬───────────────────────┘    │
                   成功    │    失败→重试               │
        ┌──────────────────▼───────────────────────┐    │
        │            dial_stat_reg_check            │ ◄──┼── 多处失败回退
        │  nw_reg_status_check() (PS域注册)         │    │
        └──────────────────┬───────────────────────┘    │
                   成功    │    失败→sleep(2)→重试      │
        ┌──────────────────▼───────────────────────┐    │
        │           dial_stat_cereg_check           │    │
        │  AT+CEREG? (LTE注册,stat=1/5)             │    │
        └──────────────────┬───────────────────────┘    │
                   成功    │    失败→sleep(10)→重试     │
        ┌──────────────────▼───────────────────────┐    │
        │        dial_stat_precondition_check       │    │
        │  QL_Data_Call_Init_Precondition()         │    │
        └──────────────────┬───────────────────────┘    │
                   成功    │    失败→重试               │
        ┌──────────────────▼───────────────────────┐    │
        │          dial_stat_pre_start_call         │    │
        │  apn_get_apn_obj(iccid) → apn_scan_idx() │    │
        │  QL_Data_Call_Set_Default_Profile()       │    │
        └──────────────────┬───────────────────────┘    │
                  配置完成  │                            │
        ┌──────────────────▼───────────────────────┐    │
        │           dial_stat_start_call            │    │
        │  QL_Data_Call_Start()                    │    │
        └──────────────────┬───────────────────────┘    │
                   成功    │    失败→重试               │
        ┌──────────────────▼───────────────────────┐    │
        │         dial_stat_wait_for_connect        │    │
        │  轮询 nw_get_connect_state()              │    │
        │  超时 > 60s:                              │    │
        │   is_oper_select=true → list_oper         │    │
        │   is_oper_select=false → AT+COPS=0        │    │
        │                          → reg_check ─────┼───┘
        └──────────────────┬───────────────────────┘
                  连接成功  │
                           │ AT+QLTS=1 获取时区
                           │ nw_get_ifaddrs() 记录接口
        ┌──────────────────▼───────────────────────┐
        │          dial_stat_net_connected          │ ◄───────────────┐
        │  每5秒: 检测 rx_packets 是否递增          │                 │
        │  超时 > 60s 无新包 → dial_stat_reg_check  │                 │
        │  nw_get_connect_state() == 0 → reg_check  │                 │
        └───────────────────────────────────────────┘                 │
                                                                       │
        ┌─────────── 运营商切换支路 ────────────────┐                 │
        │                                           │                 │
        │  dial_stat_list_oper                      │                 │
        │    AT+COPS=? (超时180s)                   │                 │
        │    解析可用运营商→加入 deque_oper          │                 │
        │          │                                │                 │
        │  dial_stat_select_oper                    │                 │
        │    AT+COPS=1,2,<number> (超时30s)         │                 │
        │    失败重试2次→移除该运营商→继续下一个     │                 │
        │          │                                │                 │
        │  dial_stat_stop_cfun                      │                 │
        │    AT+CFUN=0 (射频关闭)                   │                 │
        │          │                                │                 │
        │  dial_stat_start_cfun                     │                 │
        │    AT+CFUN=1 (射频开启)                   │                 │
        │    → dial_stat_sim_init ──────────────────┼─────────────────┘
        └───────────────────────────────────────────┘
```

### 2.4 关键数据结构

```c
/* 拨号管理器主结构 (dial.h) */
typedef struct {
    dial_stat_enu          dial_st;           // 当前状态机状态
    nw_client_handle_type  h_nw_client;       // MCM NW 客户端句柄
    ql_data_call_info_s   *p_ipv4_data_call_info; // IPv4连接信息
    struct ifaddrs        *p_ifaddrs;         // 网络接口地址信息
    uint64_t               u64_if_rx_packets; // 上次接收包计数
    uint8_t                csq;               // 当前信号强度
    bool                   is_func_called;    // 数据呼叫初始化标志
    int                    profile_idx;       // APN profile 索引
    sim_mng_t             *p_sim_mng;         // SIM 管理器
    struct timespec        dial_timer;        // 状态超时计时
    int                    smd_fd;            // AT 串口文件描述符
    CC_Deque              *deque_oper;        // 可用运营商队列
    apn_obj_t             *p_apn_obj;         // APN 配置对象
} dial_mng_t;

/* APN 配置对象 (apn.h) */
typedef struct {
    char      iccid[256];
    char      apn[256];
    char      usr_name[256];
    char      pwd[256];
    CC_Deque *deque_apn_oper;  // 允许运营商号码队列
    bool      is_oper_select;  // 是否启用运营商切换
} apn_obj_t;

/* SIM 管理器 (sim.h) */
typedef struct {
    char sim_iccid[SIM_BUF_SIZE];  // ICCID
    char sim_imsi[SIM_BUF_SIZE];   // IMSI
} sim_mng_t;
```

### 2.5 核心逻辑详解

#### 断网检测机制

连接后通过两个独立手段检测断网：

1. **MCM 状态轮询**：`nw_get_connect_state()` → `QL_Data_Call_Info_Get()` 直接查询数据呼叫状态（每轮循环执行）
2. **接口流量监控**：读取 `/sys/devices/virtual/net/<ifname>/statistics/rx_packets`，60 秒内无新增接收包则判定链路死亡重拨

注意：`DIAL_CSQ_THREADHOLD` 相关的信号强度判断逻辑在代码中被注释掉，实际未生效。

#### 运营商选择机制（is_oper_select = true）

1. `AT+COPS=?` 扫描所有可用运营商（最长 180 秒）
2. 筛选 stat=1（可用）的运营商，存入 deque_oper
3. 依次尝试 `AT+COPS=1,2,<PLMN>` 手动注册（每个最多重试 2 次，超时 30 秒）
4. 无论成功失败，完成一轮后执行 CFUN=0 → CFUN=1 重启射频

---

## 3. roamlink_deploy 深度分析

### 3.1 整体架构

```
┌──────────────────────────────────────────────────────────────┐
│                   roamlink 部署包总体架构                     │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                 check_network.sh (监控守护)            │   │
│  │  由 start_check_network (init.d) 以后台进程启动        │   │
│  │  网络模式由 /usrdata/network.ini 中 network_select 控制│   │
│  └──────┬────────────────────────────────────┬───────────┘   │
│         │ 物理SIM通道                         │ 虚拟SIM通道   │
│  ┌──────▼──────┐                   ┌─────────▼────────────┐  │
│  │    dial     │                   │       RBMaster        │  │
│  │ (open_dial  │                   │  (Roamlink 主控进程)  │  │
│  │  编译产物)  │                   │  监听本地 5568 端口   │  │
│  └──────┬──────┘                   └────┬────────┬─────────┘  │
│         │                              │        │            │
│    AT 指令                        ┌────▼──┐ ┌───▼────┐      │
│    /dev/smd8~9                    │softsim│ │cardMgr │      │
│                                   │(虚拟  │ │(卡管理)│      │
│  /tmp/network_status              │SIM)   │ └───┬────┘      │
│  /tmp/network_type                └───────┘     │           │
│                                         ┌───────▼───┐      │
│                                         │heartbeat  │      │
│                                         │controller │      │
│                                         │OemServer  │      │
│                                         │upgrade    │      │
│                                         └───────────┘      │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐  │
│  │              license 管理路径                           │  │
│  │  主路径: /usrdata/roamlink/etc/.pconfig/license.cer    │  │
│  │  备份路径: /data/ufs/license.cer                       │  │
│  │  License 与设备 IMEI 绑定                              │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

### 3.2 组件说明

#### 可执行文件与库（ARM32 ELF，不含源码）

| 文件 | 类型 | 功能 |
|------|------|------|
| `RBMaster` | 主进程（not stripped） | Roamlink 总控，管理 softsim/cardManager/heartbeat/controller/upgrade 子进程生命周期，监听本地 Unix 域 socket（port 5568，nc 接口） |
| `softsim` | 子进程（not stripped） | 软件 SIM 模拟器，需 license.cer 授权，实现虚拟 USIM 协议 |
| `cardManager` | 子进程 | 卡切换管理，处理 ecard/pcard（eSIM/物理卡）切换 |
| `heartbeat` | 子进程 | 平台心跳保活 |
| `controller` | 子进程 | 通过 `Oem::sendMsg()` 与 OemServer 通信，执行控制指令 |
| `OemServer` | 子进程 | OEM 适配层服务端 |
| `upgrade` | 子进程 | 固件 OTA 升级处理 |
| `libOem.so` | 共享库 | OEM 硬件适配层 |
| `libOemServer.so` | 共享库（with debug_info） | OEM 服务层 |
| `libTSSUMEmulator.so` | 共享库 | TS SUM（SIM-over-the-air）模拟器 |
| `test_network` | 独立工具 | TCP 连接 `18.196.0.17`（硬编码），返回 0 表示网络可达 |
| `get_imei` | 独立工具（with debug_info） | 通过 QMI 接口（`dms_get_service_object_internal_v01`）读取 IMEI |
| `dial_1.29.4` | open_dial 编译产物 | 物理 SIM 拨号程序（with debug_info，对应 open_dial V1.29.4 源码） |

#### Shell 脚本

| 脚本 | 调用方式 | 职责 |
|------|---------|------|
| `install_update.sh` | 手动执行（U盘安装） | 完整安装流程：检测固件版本→读 IMEI→安装 license→部署文件→修改 rc 符号链接→重启 |
| `check_network.sh` | init.d 守护（后台） | 网络监控+切换主逻辑（核心脚本） |
| `start_check_network` | SysV init.d | start-stop-daemon 启动 check_network.sh |
| `start_daemon` | SysV init.d | start-stop-daemon 启动 /opt/start_daemon.sh |
| `start_dfota.sh` | /usrdata 一次性 | 执行 DFOTA（`AT+QFOTADL="/usrdata/update.zip"` → `/dev/smd8`） |
| `start_dfota` | init.d | 调用 start_dfota.sh（固件升级流程使用） |
| `roamlink/start_vsim.sh` | RBMaster 内部调用 | `cd /usrdata/roamlink && ./RBMaster &` |

### 3.3 安装流程

```
用户将 U 盘插入设备
    │
    ▼
./install_update.sh
    │
    ├─ 检查 /usrdata/roamlink 是否已存在（FORCE_INSTALL=0 则退出）
    │
    ├─ ./check_fw_version → 返回值：
    │      0 = 需要 DFOTA 升级固件
    │      1 = 固件已满足要求
    │      2 = 不支持的固件
    │     255 = 读取失败
    │
    ├─ ./get_imei → 读取设备 IMEI（QMI 接口）
    │
    ├─ 查找 licenses/<IMEI>_license.cer
    │      ┌ 存在 → cp 至 roamlink/etc/.pconfig/license.cer
    │      │        cp 至 /data/ufs/license.cer（备份）
    │      └ 不存在 → MUST_HAVE_LICENSE=1 则退出(255)
    │
    ├─ 根据 fw_version:
    │   fw_version=0（需DFOTA）：
    │      cp start_dfota.sh /usrdata
    │      cp start_dfota /etc/init.d/
    │      ln S100start_dfota → 启动后自动执行DFOTA
    │      屏蔽 S20start_vsim
    │
    │   fw_version=1（无需DFOTA）：
    │      ln S60start_check_network
    │      unlink S20start_dial（屏蔽原拨号）
    │      unlink S20start_vsim
    │
    ├─ cp -af ./roamlink /usrdata/
    ├─ cp ./dial_1.29.4 /usr/dial/dial（替换原有dial程序）
    ├─ cp network.ini / test_network / check_network.sh → /usrdata/
    ├─ sed -i 注释掉 /opt/start_daemon.sh 中的 check_dial 调用
    │
    └─ sync + reboot

如果 fw_version=0，重启后：
    start_dfota.sh:
        check_fw_version == 0 → AT+QFOTADL="/usrdata/update.zip" → /dev/smd8
        固件升级完成后再次重启
        start_dfota.sh:
            check_fw_version == 1 → 执行环境修复
            adduser sshd、重建 rc 符号链接、清理 DFOTA 文件
            → reboot（正式进入运行状态）
```

### 3.4 运行时流程与网络切换逻辑

`check_network.sh` 是运行时的核心，以 `/usrdata/network.ini` 中 `network_select` 为策略基础：

```
network_select 值:
  1 = 优先 Roamlink（失败时切到 SIM）
  2 = 优先物理 SIM（失败时切到 Roamlink）
  3 = 强制 Roamlink（失败后重启同一服务）
  4 = 强制物理 SIM（失败后重启 dial）
  其他 = 等同于 4
```

**check_celluar_network 函数逻辑（network_status 检测）：**

```
循环最多10次，每次间隔2秒:
    /usrdata/test_network → 连接 18.196.0.17
        成功 → network_status=0，退出循环
        失败 → network_status=1，继续

如果 network_status != 0（网络异常）:
    当前为 SIM (network_type=1):
        celluar_switch=1 → 停止 dial → 启动 Roamlink
        celluar_switch=0 → 停止 dial → 重启 dial

    当前为 Roamlink (network_type=2):
        celluar_switch=1 → 停止 Roamlink → 启动 dial
        celluar_switch=0 → 停止 Roamlink → 重启 Roamlink
```

**主循环节拍：**
- 网络正常：sleep 1800 秒（30 分钟）后再次检测
- 网络异常：切换后 sleep 600 秒（10 分钟）再次检测

**Roamlink 服务控制机制（RBMaster 本地 socket）：**

```bash
# 启动虚拟SIM服务
echo "RBstartServiceMaster" | nc 127.0.0.1 5568

# 停止虚拟SIM服务
echo "RBstopServiceMaster" | nc 127.0.0.1 5568
```

---

## 4. 两仓库关系与协作机制

```
┌─────────────────────────────────────────────────────────────────┐
│                         硬件层                                   │
│              Quectel EC2x/EG2x 模组（ARM Cortex-A7）            │
│          /dev/smd8（AT口）  /dev/smd9（第二AT口）               │
└───────────────────────────┬─────────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────────┐
│                 open_dial 编译产物（dial）                        │
│  • 源码来自 open_dial_for_artery（V1.29.4 对应版本）             │
│  • 负责物理 SIM 卡的 4G 拨号                                     │
│  • 输出: /tmp/network_status, /tmp/network_csq                   │
│  • 被 roamlink 的 check_network.sh 以进程方式启停                │
└───────────────────────────┬─────────────────────────────────────┘
                            │ 共存/互斥
┌───────────────────────────▼─────────────────────────────────────┐
│               RBMaster（Roamlink 虚拟SIM服务）                   │
│  • 闭源 ARM 二进制，由 Roamlink 云平台提供                       │
│  • 负责 eSIM/虚拟SIM 的数据通道                                  │
│  • 受 license.cer（IMEI 绑定）授权控制                           │
└───────────────────────────┬─────────────────────────────────────┘
                            │ 统一调度
┌───────────────────────────▼─────────────────────────────────────┐
│              check_network.sh（统一网络管理器）                   │
│  • 唯一决策者：根据 network_select 配置决定使用哪个通道           │
│  • 每 30 分钟（正常）或 10 分钟（异常）检测一次                  │
│  • 使用 test_network 二进制测试连通性（TCP 到固定服务器）         │
│  • 切换时: pkill dial 或 nc RBstopServiceMaster                  │
└─────────────────────────────────────────────────────────────────┘

关键协作点:
1. dial_1.29.4 是 open_dial 源码的直接编译产物，被 roamlink 包打包分发
2. install_update.sh 用 dial_1.29.4 替换设备上已有的 /usr/dial/dial
3. check_network.sh 同时管理 dial 进程和 RBMaster 服务
4. 两者共享同一个 AT 串口资源（/dev/smd8~9），运行时互斥
5. 物理 SIM 拨号失败时，自动降级到虚拟 SIM；反之亦然（strategy 1/2）
```

---

## 5. 已知 Bug 与代码缺陷

### 5.1 at.c — AT 命令长度截断（严重）

**位置**：`Ql_SendAT()` 第7行

```c
// 错误代码
iLen = sizeof(atCmd);   // sizeof(char*) = 4（在32位ARM上）
strncpy(strAT, atCmd, iLen);

// 正确应该是
iLen = strlen(atCmd);
```

`sizeof(atCmd)` 对指针求大小返回 4，导致所有 AT 命令字符串被截断至前 4 个字节。AT 命令实际能正确发送的原因是后面重新用 `sprintf(strAT, "%s\r\n", atCmd)` 覆盖了 strAT，但 strncpy 这一行是错误的，且 strAT 的首次构造存在问题。

### 5.2 apn.c — 死代码（apn_scan_idx 函数逻辑永远绕过）

**位置**：`apn_scan_idx()` 第 4 行

```c
int apn_scan_idx(apn_obj_t *p_apn_obj) {
    ql_apn_info_list_s apn_list;
    int i = 0, ret = -1;
    int apn_profile_idx = -1;
    memset(&apn_list, 0, sizeof(apn_list));
    return apn_set(1, p_apn_obj);  // ← 提前 return，以下 100+ 行完整匹配逻辑全部是死代码
    // ... 永远不会执行的 APN 列表扫描逻辑
}
```

这是开发过程中临时 workaround（强制使用 profile_idx=1，README V1.29.4 中有说明"高通模组只对 profile_idx=1 自动设置路由"），但未清理死代码。

### 5.3 apn.c — 赋值运算符错误（Bug 导致逻辑永远走 apn_set）

**位置**：`apn_scan_idx()` 死代码区（虽然不执行，但记录为设计缺陷）

```c
if (ret = -1)   // ← 赋值，不是比较！永远为 true
{
    ret = apn_set(1, p_apn_obj);
}
```

应为 `if (ret == -1)`。

### 5.4 sim.c — switch-case fall-through 无 break（逻辑缺陷）

**位置**：`sim_op_handler()` case `sim_op_stat_get_iccid`

```c
case sim_op_stat_get_iccid:
    if (E_QL_OK == sim_op(SIM_OP_GET_ICCID, p_sim_op_mng->sim_iccid)) {
        SEAS_LOG_INFO("ICCID: %s", p_sim_op_mng->sim_iccid);
        ret = true;
    }
    // ← 缺少 break，直接 fall-through 到下面的 get_imsi
case sim_op_stat_get_imsi:
    if (E_QL_OK == sim_op(SIM_OP_GET_IMSI, p_sim_op_mng->sim_imsi)) {
```

调用 `sim_op_handler(p_sim_op_mng, sim_op_stat_get_iccid)` 会同时执行 ICCID 和 IMSI 两个操作，副作用可接受但属于代码逻辑缺陷。

### 5.5 apn.c — 内存泄漏（fread 失败路径）

**位置**：`apn_scan_from_json()`

```c
str = (char *)malloc(len);
if (fread(str, 1, len, fp) < 0) {
    SEAS_LOG_ERROR("fread error");
    return NULL;  // ← str 未 free，fp 未 fclose
}
```

### 5.6 nw.c — 悬空指针（nw_get_ifaddrs 返回值）

**位置**：`nw_get_ifaddrs()`

```c
p_ifaddrs = (struct ifaddrs *)calloc(1, sizeof(struct ifaddrs));
memcpy(p_ifaddrs, ifa, sizeof(struct ifaddrs));  // 浅拷贝，ifa_name 指向原链表内存
// ...
freeifaddrs(ifaddr);  // 原链表被释放
return p_ifaddrs;     // ← p_ifaddrs->ifa_name 已是悬空指针
```

`ifa_name` 是指针成员，浅拷贝后 `freeifaddrs()` 释放了原内存，返回的结构体中 `ifa_name` 指针指向无效内存。调用方 `dial_task` 通过 `p_dial_mng->p_ifaddrs->ifa_name` 访问接口名会触发未定义行为。

### 5.7 nw.c — shell 注入风险（system() 使用）

**位置**：`nw_at_get_csq()`、`nw_mark_plmn()`

```c
sprintf(echo_cmd, "echo %d > %s", csq, NW_CSQ_PATH);
system(echo_cmd);  // csq 来自 AT+CSQ 响应，理论可控但危险实践

sprintf(echo_cmd, "echo %s%s > %s", mcc, mnc, NW_PLMN_PATH);
system(echo_cmd);  // mcc/mnc 来自网络，如有注入字符则执行任意命令
```

### 5.8 nw.c — nw_mark_network_status 文件写入不安全

**位置**：`nw_mark_network_status()`

```c
fd = open(NW_STATUS_PATH, O_WRONLY | O_CREAT);  // 无 O_TRUNC，可能残留旧内容
// 无权限设置（mode 参数缺失，O_CREAT 需第三参数）
sprintf(br_str, "%d", net_status);
write(fd, br_str, strlen(br_str));
// 无 close() 的错误检测
```

`open()` 使用 `O_CREAT` 时缺少第三参数（文件权限），行为依赖 umask，且不加 `O_TRUNC` 可能写入 "0" 后残留旧的 "1"（若旧值更长则变 "10"）。

---

## 6. 安全漏洞分析

### 6.1 RBMaster 本地控制接口无认证

**位置**：`check_network.sh` 及 RBMaster 设计

```bash
echo "RBstartServiceMaster" | nc 127.0.0.1 5568
echo "RBstopServiceMaster" | nc 127.0.0.1 5568
```

RBMaster 在 127.0.0.1:5568 上监听，任何有 shell 访问权限的进程或用户均可发送控制命令启停虚拟 SIM 服务，无任何身份验证。

**风险**：本地权限提升或恶意脚本可随意控制网络服务状态。

### 6.2 test_network 明文 TCP 连通性检测

**位置**：`test_network` 二进制（strings 提取）

```
18.196.0.17
Connect OK, sockfd=%d
```

test_network 直接 TCP 连接硬编码 IP `18.196.0.17`，无任何 TLS 加密或证书验证。若该 IP 被中间人劫持或 DNS 污染（虽然此处是 IP，无 DNS），攻击者可伪造"网络正常"状态，使网络切换逻辑失效。

### 6.3 license.cer 明文存储与传输

**位置**：`check_license()`、`RBMaster`

```bash
cp "$BACKUP_LICENSE" "$LICENSE_DIR"  # 明文文件拷贝
```

license.cer 以明文文件形式存放于 `/data/ufs/` 和 `/usrdata/roamlink/etc/.pconfig/`，无加密保护，有读取权限的进程均可获取并可能被复制到其他设备用于非授权使用（虽然 license 与 IMEI 绑定，但文件本身无加密）。

### 6.4 DFOTA 通过 AT 命令直接触发

**位置**：`start_dfota.sh`

```bash
echo -e "AT+QFOTADL=\"/usrdata/update.zip\"\r\n" >/dev/smd8
```

固件升级使用本地文件路径通过 AT 命令直接触发，无签名验证（验证逻辑依赖 check_fw_version 二进制，源码不可见）。若 `/usrdata/update.zip` 被替换为恶意固件，可能导致设备被刷入恶意固件。

### 6.5 at.c 中 Ql_SendAT 响应缓冲区无边界保护

**位置**：`Ql_SendAT()`

```c
char strResponse[AT_MSG_LENGTH_MAX];  // 固定大小缓冲区
rdLen = read(smd_fd, strResponse, AT_MSG_LENGTH_MAX);
// 若 rdLen == AT_MSG_LENGTH_MAX 则进入 while 继续读（逻辑正确）
// 但 memcpy(p_rsp_msg, strResponse, rdLen) 中：
// p_rsp_msg 由调用方提供，其大小无传入保护
```

调用方（dial.c）中 `rsp_msg[1024]` 与 `AT_MSG_LENGTH_MAX` 定义一致，但这是约定而非代码强制保证。如果 `AT_MSG_LENGTH_MAX` 被修改而 dial.c 中的局部变量未同步修改，则存在缓冲区溢出。

### 6.6 check_network.sh 中 set -e 与 safe_kill 竞争

**位置**：`check_network.sh`

```bash
set -e   # 任何命令失败即退出
# 但 safe_kill() 中 pgrep 的非零返回（进程不存在）会触发 set -e 退出
# safe_kill 内部 pgrep 失败会导致整个 monitor 脚本退出
```

实际 `safe_kill` 函数将 `pgrep` 输出重定向到 `/dev/null 2>&1` 后跟 `if`，因此在此函数内不会触发 `set -e`，但在脚本其他位置的命令失败（如 `nc` 连接失败）仍会异常退出，导致监控守护进程意外终止。

---

## 7. 改进建议

### 7.1 at.c — 修复 AT 命令长度计算

```c
// 修复前
iLen = sizeof(atCmd);

// 修复后
iLen = strlen(atCmd);
```

### 7.2 apn.c — 清理死代码，修复赋值运算符

```c
// apn_scan_idx：删除提前的 return 语句，保留完整 APN 扫描逻辑
// 或者将强制 profile_idx=1 的意图显式注释说明

// 修复赋值错误
if (ret == -1)  // 原为 if (ret = -1)
```

### 7.3 apn.c — 修复内存泄漏

```c
if (fread(str, 1, len, fp) < 0) {
    SEAS_LOG_ERROR("fread error");
    free(str);      // 新增
    fclose(fp);     // 新增
    return NULL;
}
```

### 7.4 nw.c — 修复悬空指针（nw_get_ifaddrs）

```c
// 方案1：拷贝 ifa_name 字符串而不是仅拷贝指针
p_ifaddrs = (struct ifaddrs *)calloc(1, sizeof(struct ifaddrs));
memcpy(p_ifaddrs, ifa, sizeof(struct ifaddrs));
// 深拷贝 ifa_name
char *name_copy = strdup(ifa->ifa_name);
p_ifaddrs->ifa_name = name_copy;
freeifaddrs(ifaddr);
return p_ifaddrs;
```

### 7.5 nw.c — 用 open()/write() 替代 system() 写文件

```c
// 修复 nw_at_get_csq 和 nw_mark_plmn 中的 system() 调用
// 使用直接文件写入代替 shell 命令，消除注入风险
int fd = open(NW_CSQ_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
if (fd >= 0) {
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%d", csq);
    write(fd, buf, len);
    close(fd);
}
```

### 7.6 nw.c — 修复 nw_mark_network_status

```c
// 加入 O_TRUNC 和文件权限
fd = open(NW_STATUS_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
if (fd < 0) {
    SEAS_LOG_ERROR("open %s failed: %s", NW_STATUS_PATH, strerror(errno));
    return;
}
// ... write / close
```

### 7.7 sim.c — 修复 switch-case fall-through

```c
case sim_op_stat_get_iccid:
    if (E_QL_OK == sim_op(SIM_OP_GET_ICCID, p_sim_op_mng->sim_iccid)) {
        SEAS_LOG_INFO("ICCID: %s", p_sim_op_mng->sim_iccid);
        ret = true;
    }
    break;  // 新增 break
case sim_op_stat_get_imsi:
    // ...
```

### 7.8 roamlink — RBMaster 控制接口认证

建议在本地 socket 通信层增加 token 验证（如共享密钥或 Unix socket 权限控制），或改用 Unix domain socket（文件权限控制）替代 TCP 5568。

### 7.9 roamlink — test_network 改用 HTTPS/mTLS

将 `test_network` 改为通过 HTTPS 或具备证书验证的方式检测网络连通性，或使用 ICMP ping（需评估对目标服务器的影响），避免依赖明文 TCP 可被欺骗。

### 7.10 roamlink — check_network.sh 守护进程健壮性

```bash
# 去掉顶层 set -e，改为在关键函数内部显式处理错误
# 或者用 trap 捕获错误并重启自身
trap 'log "ERROR: script crashed, restarting..."; sleep 10; exec $0' ERR
```

### 7.11 旧版本 dial 残留日志路径问题

README 注释中提及 seas_log 文件路径曾为 `/media/sdcard/dial_seas_log.log`，但 dial_1.29.4 二进制字符串显示路径为 `/media/sdcard/seas_log_dial.log`，与当前 main.c 源码一致。建议统一清理历史注释避免混淆。

---

*报告完。所有代码引用均直接来自仓库源文件，二进制分析结论来自 `strings` 输出与 `file` 命令。*
