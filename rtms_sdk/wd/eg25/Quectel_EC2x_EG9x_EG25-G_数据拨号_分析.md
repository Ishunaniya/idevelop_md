# Quectel EC2x&EG9x&EG25-G 系列 QuecOpen 数据拨号用户指导分析

> 原始文档：Quectel_EC2x&EG9x&EG25-G系列_QuecOpen_数据拨号用户指导_V1.0.pdf  
> 文档版本：EC2x&EG9x&EG25-G_QuecOpen_数据拨号用户指导_V1.0  
> 文档日期：2020-05-31  
> 分析日期：2026-05-28

---

## 1. 文档概述

### 1.1 手册目的

本文档主要描述移远通信 LTE Standard QuecOpen 模块无线数据业务的建立过程，即**数据拨号**的完整流程。内容涵盖：
- AP 侧数据拨号流程（设备检查 → APN 配置 → 拨号）
- 单路拨号与多路拨号
- 3GPP2/CDMA 网络拨号
- 不同应用场景下的路由、转发、DNS 配置
- 数据拨号 API 完整定义（数据类型 + 函数）
- 常见问题排查方法

> **重要提示**：整个拨号过程需严格按照第 3 章的顺序执行，特别是初次接触无线业务的客户。

### 1.2 适用模块型号

| 模块系列 | 具体模块 |
|---------|---------|
| EC2x 系列 QuecOpen | EC25 系列、EC21 系列、EC20 R2.1、EC20-CN |
| EG9x 系列 QuecOpen | EG95 系列、EG91 系列 |
| EG25-G QuecOpen | EG25-G |

### 1.3 章节结构总览

| 章节 | 内容 |
|-----|------|
| 第1章 引言 | 适用模块列表 |
| 第2章 网卡连接 | USB-ECM 和 USB-RNDIS 网卡使用说明（参考外部文档） |
| 第3章 AP 侧拨号 | 设备检查、APN 配置、单路/多路拨号、CDMA 拨号流程 |
| 第4章 不同应用场景下的配置 | 8 种应用场景的路由、转发、DNS 完整配置方案 |
| 第5章 补充说明 | DNS API 说明、PING/NSLOOKUP 命令说明 |
| 第6章 数据拨号 API | 所有数据类型定义 + 11 个 API 函数详解 |
| 第7章 常见问题 | PCAP 日志抓取、iptables 查看、CDMA 鉴权失败、开机拨号失败、IPv6 DNS 问题 |
| 第8章 附录 | 参考文档、术语缩写 |

---

## 2. 拨号流程

### 2.1 AP 侧拨号总体流程

```
上电启动
    │
    ▼
设备检查（必须按顺序执行）
    ├─ 1. 检测(U)SIM 卡：QL_MCM_SIM_GetCardStatus()
    ├─ 2. 检测信号强度：QL_MCM_NW_GetSignalStrength()
    ├─ 3. 检测模块注网：QL_MCM_NW_GetRegStatus()
    ├─ 4. 查询运营商：QL_MCM_NW_GetRegStatus()
    ├─ 5. 查询网络接入技术：QL_MCM_NW_GetRegStatus()
    └─ 6. 查询拨号服务状态：QL_Data_Call_Init_Precondition()
    │
    ▼
APN 配置（多路拨号时需要配置特殊 APN）
    ├─ 查询：QL_APN_Get()
    └─ 配置：QL_APN_Set()
    │
    ▼
初始化拨号并注册回调：QL_Data_Call_Init(user_callback)
    │
    ▼
设置 Default Profile（多路拨号需设为 8 以禁用自动路由）
    │
    ├─ 单路拨号 ──────────► QL_Data_Call_Start(&data_call, &err)
    │
    └─ 多路拨号 ──────────► 依次调用 QL_Data_Call_Start() 建立多路通道
                            后续配置路由/转发/DNS
```

### 2.2 设备检查步骤

拨号前必须按以下顺序执行设备检查：

1. 通过 USB 转串口线连接 PC 至模块主串口或 USB 端口
2. 插入(U)SIM 卡和天线，并上电
3. 通过 API 函数按顺序检查：
   - `QL_MCM_SIM_GetCardStatus()` - 检测(U)SIM 卡
   - `QL_MCM_NW_GetSignalStrength()` - 检测信号强度
   - `QL_MCM_NW_GetRegStatus()` - 检测注网、查询运营商、查询接入技术
   - `QL_Data_Call_Init_Precondition()` - 查询拨号服务状态

### 2.3 APN 配置说明

APN 配置规则：
- **必须在拨号之前进行 APN 参数配置**，配置后参数自动保存，重启仍有效
- 通常公网 APN 无需设置用户名、密码及鉴权参数
- 专网 APN 是否需要用户名密码需咨询运营商
- 针对 CDMA eHRPD 网络，强制设置 `profile_id` 为 0，在拨号接口传入用户名和密码

### 2.4 单路拨号

```c
// 步骤1：设置默认 profile
QL_Data_Call_Set_Default_Profile(profile_id);  // 与请求拨号的 profile_id 值一致

// 步骤2：初始化并注册回调
QL_Data_Call_Init(user_callback);

// 步骤3：发起拨号
ql_data_call_s data_call_paras;
data_call_paras.profile_idx = 1;
data_call_paras.ip_family = QL_DATA_CALL_IPV4;
data_call_paras.reconnect = true;

int err_code;
QL_Data_Call_Start(&data_call_paras, &err_code);
// 成功后系统出现 rmnet_data0
```

> 无需考虑路由、转发表和 DNS 的配置。

### 2.5 多路拨号

```c
// 初始化并注册回调函数
Ql_Data_Call_Init(user_callback);

// 不使用自动配置默认路由和默认转发（重要！）
QL_Data_Call_Set_Default_Profile(8);

// 建立第1路（profile_idx = 1）
int err_code1;
ql_data_call_s data1_call_paras;
data1_call_paras.profile_idx = 1;
data1_call_paras.ip_family = QL_DATA_CALL_IPV4;
data1_call_paras.reconnect = true;
Ql_Data_Call_Start(&data1_call_paras, &err_code1);
// 系统出现 rmnet_data0

// 建立第2路（profile_idx = 2）
int err_code2;
ql_data_call_s data2_call_paras;
data2_call_paras.profile_idx = 2;
data2_call_paras.ip_family = QL_DATA_CALL_IPV4;
data2_call_paras.reconnect = true;
Ql_Data_Call_Start(&data2_call_paras, &err_code2);
// 系统出现 rmnet_data1

// 建立第3路（profile_idx = 3）
int err_code3;
ql_data_call_s data3_call_paras;
data3_call_paras.profile_idx = 3;
data3_call_paras.ip_family = QL_DATA_CALL_IPV4;
data3_call_paras.reconnect = true;
Ql_Data_Call_Start(&data3_call_paras, &err_code3);
// 系统出现 rmnet_data2
```

> **备注**：多路拨号时 `profile_idx` 值无需和 `QL_Data_Call_Get_Default_Profile()` 值保持一致。

### 2.6 3GPP2/CDMA 网络拨号

```c
// 步骤1：确认 SIM 卡运营商支持 CDMA
// 步骤2：调用 QL_MCM_NW_GetRegStatu() 查询注册在 LTE 还是 CDMA
// 步骤3：profile_id 强制设为 0，CDMA 不支持多路拨号

int err_code1;
ql_data_call_s data1_call_paras;
char username[] = {"this is example"};
char passwd[] = {"this is example"};

data1_call_paras.profile_idx = 0;          // 必须为 0
data1_call_paras.ip_family = QL_DATA_CALL_IPV4;
data1_call_paras.reconnect = true;
data1_call_paras.cdma_username = username;
data1_call_paras.cdma_password = passwd;

Ql_Data_Call_Start(&data1_call_paras, &err_code1);
// 系统出现 rmnet_data0
```

> **备注**：当前网络是 HPRD 还是 eHRPD 对 3GPP2/CDMA 网络拨号没有影响。

---

## 3. 不同应用场景下的配置

### 3.1 场景1：单路

**拓扑**：模块 AP 侧 → rmnet_data0（APN1）→ 外网

**配置**：无需任何路由、转发表和 DNS 配置，按第3章拨号步骤执行即可。

---

### 3.2 场景2：单路 + ECM 设备

**拓扑**：上位机 → ECM → 模块 AP 侧 → rmnet_data0（APN1）→ 外网

**配置**：无需任何路由、转发表和 DNS 配置，按第3章拨号步骤执行即可。

---

### 3.3 场景3：多路

**拓扑**：模块 AP 侧 → rmnet_data0（APN1）→ 外网，同时 AP 侧 → rmnet_data1（APN2）→ 专网

**配置步骤**：

```bash
# 4.3.1 规则清空
route del default
iptables -t filter -F
iptables -t nat -F

# 4.3.2 默认路由（假设 APN1 网关为 10.112.7.176）
# 获取 APN1 网关
# Ql_Data_Call_Info_Get(1, &info);
# 方法一：
route add default dev rmnet_data0
# 方法二：
route add -net 10.112.7.0/24 dev rmnet_data0
route add default gw 10.112.7.176

# 4.3.3 默认 DNS 设置（假设 APN1 DNS 为 211.138.180.2 和 211.138.180.3）
# Ql_Data_Call_Info_Get(1, &info);
echo "nameserver 211.138.180.2" > /etc/resolv.conf
echo "nameserver 211.138.180.3" >> /etc/resolv.conf

# 4.3.4 通过 APN2 访问某个 Server
# 4.3.4.1 DNS 路由配置（APN2 DNS 为 121.158.200.8 和 121.158.200.9）
ip route add 121.158.200.8/32 dev rmnet_data1
ip route add 121.158.200.9/32 dev rmnet_data1

# 4.3.4.2 域名解析
# QL_nslookup(www.xxx.com, 121.158.200.8, IPV4, resolved_output)

# 4.3.4.3 路由配置（目标 47.88.189.189，APN2 网关 10.32.80.46）
route add -net 47.88.189.189/32 gw 10.32.80.46 dev rmnet_data1
```

---

### 3.4 场景4：多路 + ECM（1）

**拓扑**：上位机 → ECM → 模块 AP 侧 → rmnet_data0（APN1）→ 外网（AP 侧可访问 APN2 专网）

**配置**：在场景3基础上，打开默认转发表：

```bash
# 开启默认转发
echo 1 > /proc/sys/net/ipv4/ip_forward
# 设置转发表（上位机通过 APN1 访问外网）
iptables -t nat -A POSTROUTING -o rmnet_data0 -j MASQUERADE --random
```

---

### 3.5 场景5：多路 + ECM（2）

**拓扑**：上位机 → ECM → 模块 AP 侧 → rmnet_data0（APN1）→ 外网，上位机还需通过 APN2 访问特定 Server

**配置**：在场景4基础上，为上位机通过 APN2 访问 Server 配置：

```bash
# 4.5.2 路由设置（假设目标 Server IP 47.88.189.189）
# 方法一：精确转发
iptables -t nat -A POSTROUTING -d 47.88.189.189 -o rmnet_data1 -j MASQUERADE
# 方法二：全部转发到 APN2
iptables -t nat -A POSTROUTING -o rmnet_data1 -j MASQUERADE --random
# 路由规则
ip route add 47.88.189.189/32 dev rmnet_data1
```

> **说明**：上位机域名解析由 AP 侧 dnsmasq 通过 APN1 DNS 解析，若上位机需域名访问，需在 AP 侧建立 Socket 服务完成 DNS 解析和转发配置。

---

### 3.6 场景6：多路 + ECM（3）

**拓扑**：上位机 → ECM → 模块 AP 侧，AP 侧通过 APN2 访问外网，上位机通过 APN1 访问专网

**配置步骤**：

```bash
# 4.6.1 规则清空
route del default
iptables -t filter -F
iptables -t nat -F

# 4.6.2 AP 侧路由设置（默认走 APN2，假设网关 10.32.80.46）
route add default dev rmnet_data1
# 或
route add -net 10.32.80.0/24 dev rmnet_data1
route add default gw 10.32.80.46

# 4.6.3 AP 侧 DNS 设置（使用 APN2 DNS）
echo "nameserver 121.158.280.8" > /etc/resolv.conf
echo "nameserver 121.158.280.9" >> /etc/resolv.conf

# 4.6.4 转发表设置（上位机通过 APN1 访问外网）
echo 1 > /proc/sys/net/ipv4/ip_forward
iptables -t nat -A POSTROUTING -o rmnet_data0 -j MASQUERADE --random

# 4.6.5 策略路由设置（为 ECM 设备 192.168.254.0/24 创建策略路由）
echo "200 rmnet_data_apn1" >> /etc/iproute2/rt_tables
ip rule add from 192.168.254.0/24 table 200
# 方法一：
ip route add dev rmnet_data0 table 200
# 方法二：
ip route add via 10.112.7.176 table 200  # 10.112.7.176 为 APN1 网关

# 4.6.6 上位机 DNS 服务器设置（若需要借助模块 dnsmasq 解析）
# 修改 /etc/dnsmasq.conf 并重启
# 创建 /etc/dnsmasq_resolv.conf 添加 APN1 DNS 地址
echo "nameserver 211.138.180.2" > /etc/dnsmasq_resolv.conf
echo "nameserver 211.138.180.3" >> /etc/dnsmasq_resolv.conf

# APN1 DNS 路由设置（确保 DNS 请求走 APN1）
ip route add 211.138.180.2/32 dev rmnet_data0
ip route add 211.138.180.3/32 dev rmnet_data0
```

---

### 3.7 场景7：多路 + ECM（4）

**拓扑**：在场景6基础上，AP 侧需通过 APN1 访问特定 Server，上位机需通过 APN2 访问特定 Server

**配置**：首先完成场景6所有基本设置，然后：

```bash
# AP 侧通过 APN1 访问 www.xxx.com（IP 47.88.189.189，APN1 网关 10.112.7.176）
# 域名解析
QL_nslookup(www.xxx.com, 211.138.180.2, IPV4, resolved_output);  # 使用 APN1 DNS
# 路由设置
route add -net 47.88.189.189/32 gw 10.112.7.176 dev rmnet_data0

# 上位机通过 APN2 访问（假设目标 47.88.189.189）
# 方法一：
iptables -t nat -A POSTROUTING -d 47.88.189.189 -o rmnet_data1 -j MASQUERADE
# 方法二：
iptables -t nat -A POSTROUTING -o rmnet_data1 -j MASQUERADE --random
# 路由：
ip route add 47.88.189.189/32 dev rmnet_data1 table 200
# 或基于策略路由：
ip rule add to 47.88.189.189 table main
```

---

### 3.8 场景8：多路 + ECM（5）

**拓扑**：两个 ECM 设备，分别通过 APN1 和 APN2 访问各自网络

**配置**：
- AP 侧路由和 DNS：参照场景6的 4.6.1 和 4.6.3
- 上位机策略路由：参照场景6的 4.6.4 和 4.6.5
- DNS 解析设置：各 APN 的 DNS 地址路由到对应 APN 网关

---

## 4. DNS 补充说明

### 4.1 Linux DNS API 对比

| API | 使用的 DNS | 支持的地址类型 |
|-----|-----------|--------------|
| `gethostbyname()` | `/etc/resolv.conf` 中的 DNS | 仅 IPv4 |
| `getaddrinfo()` | `/etc/resolv.conf` 中的 DNS | IPv4 + IPv6 |
| `QL_nslookup()` | 指定的 DNS 地址 | 可单独指定 IPv4 或 IPv6 |

> **重要**：以上 API 都不能保证一次性解析成功，需要反复调用 2~3 次。原因是 DNS 协议传输层采用 UDP，丢包是正常现象，DNS 服务器本身的网络连通性也很重要。

### 4.2 命令行工具说明

| 命令 | DNS 来源 |
|-----|---------|
| `ping` | Linux 系统默认 DNS（`/etc/resolv.conf`） |
| `nslookup` | `resolv.conf` 默认 DNS（与 PC 上使用有差异） |

---

## 5. 数据拨号 API 详解

### 5.1 数据类型定义

---

#### `ql_data_call_error_e`（错误码枚举）

```c
typedef enum {
    QL_DATA_CALL_ERROR_NONE = 0,        // 无错误
    QL_DATA_CALL_ERROR_INVALID_PARAMS,  // 无效参数
} ql_data_call_error_e;
```

| 值 | 含义 |
|---|------|
| `QL_DATA_CALL_ERROR_NONE` | 无错误 |
| `QL_DATA_CALL_ERROR_INVALID_PARAMS` | 无效参数 |

---

#### `ql_data_call_state_e`（数据拨号连接状态枚举）

```c
typedef enum {
    QL_DATA_CALL_DISCONNECTED = 0,  // 断开连接
    QL_DATA_CALL_CONNECTED,         // 已连接
} ql_data_call_state_e;
```

---

#### `ql_data_call_ip_family_e`（IP 协议族枚举）

```c
typedef enum {
    QL_DATA_CALL_TYPE_IPV4   = 0,  // IPv4 拨号
    QL_DATA_CALL_TYPE_IPV6,        // IPv6 拨号
    QL_DATA_CALL_TYPE_IPV4V6,      // IPv4 和 IPv6 同时拨号（仅用于 Start/Stop）
} ql_data_call_ip_family_e;
```

---

#### `ql_apn_pdp_type_e`（APN PDP 类型枚举）

```c
typedef enum {
    QL_APN_PDP_TYPE_IPV4   = 0,  // IPv4
    QL_APN_PDP_TYPE_PPP,         // PPP
    QL_APN_PDP_TYPE_IPV6,        // IPv6
    QL_APN_PDP_TYPE_IPV4V6,      // IPv4 + IPv6
} ql_apn_pdp_type_e;
```

---

#### `ql_apn_auth_proto_e`（APN 鉴权协议枚举）

```c
typedef enum {
    QL_APN_AUTH_PROTO_DEFAULT  = 0,  // 默认鉴权
    QL_APN_AUTH_PROTO_NONE,          // 无鉴权
    QL_APN_AUTH_PROTO_PAP,           // PAP 鉴权
    QL_APN_AUTH_PROTO_CHAP,          // CHAP 鉴权
    QL_APN_AUTH_PROTO_PAP_CHAP,      // PAP 或 CHAP 鉴权
} ql_apn_auth_proto_e;
```

---

#### `v4_address_status`（IPv4 地址状态结构体）

```c
struct v4_address_status {
    struct in_addr ip;       // 公共 IPv4 地址
    struct in_addr gateway;  // IPv4 网关地址
    struct in_addr pri_dns;  // 主 DNS 地址
    struct in_addr sec_dns;  // 辅 DNS 地址
};
```

---

#### `v6_address_status`（IPv6 地址状态结构体）

```c
struct v6_address_status {
    struct in6_addr ip;       // 公共 IPv6 地址
    struct in6_addr gateway;  // IPv6 网关地址
    struct in6_addr pri_dns;  // 主 IPv6 DNS 地址
    struct in6_addr sec_dns;  // 辅 IPv6 DNS 地址
};
```

---

#### `ql_data_call_state_s`（数据拨号状态结构体）

```c
typedef struct {
    char                     profile_idx;  // UMTS/CDMA profile ID
    char                     name[16];     // 网络接口名称（如 rmnet_data0）
    ql_data_call_ip_family_e ip_family;    // IP 版本
    ql_data_call_state_e     state;        // 拨号连接状态
    ql_data_call_error_e     err;          // 断开后的错误码
    union {
        struct v4_address_status v4;       // IPv4 地址信息
        struct v6_address_status v6;       // IPv6 地址信息
    };
} ql_data_call_state_s;
```

---

#### `ql_data_call_evt_cb_t`（回调函数类型）

```c
typedef void (*ql_data_call_evt_cb_t)(ql_data_call_state_s *state);
```

当数据拨号状态发生变化时，此回调函数被调用，参数为最新的拨号状态信息。

---

#### `ql_data_call_s`（数据拨号参数结构体）

```c
typedef struct {
    char                     profile_idx;       // UMTS/CDMA profile ID
    bool                     reconnect;          // 断网后是否自动重连
    ql_data_call_ip_family_e ip_family;          // IP 版本
    char                     cdma_username[127]; // CDMA 网络用户名
    char                     cdma_password[127]; // CDMA 网络密码
} ql_data_call_s;
```

| 字段 | 类型 | 说明 |
|-----|-----|------|
| `profile_idx` | `char` | UMTS/CDMA profile ID（CDMA 固定为 0） |
| `reconnect` | `bool` | true=断网后自动重连，false=不重连 |
| `ip_family` | `ql_data_call_ip_family_e` | IP 协议版本 |
| `cdma_username` | `char[127]` | CDMA 网络用户名（仅 CDMA 时使用） |
| `cdma_password` | `char[127]` | CDMA 网络密码（仅 CDMA 时使用） |

---

#### `pkt_stats`（数据包统计结构体）

```c
struct pkt_stats {
    unsigned long pkts_tx;         // 发送的数据包数量
    unsigned long pkts_rx;         // 接收的数据包数量
    long long     bytes_tx;        // 发送的字节数
    long long     bytes_rx;        // 接收的字节数
    unsigned long pkts_dropped_tx; // 发送丢失的数据包数量
    unsigned long pkts_dropped_rx; // 接收丢失的数据包数量
};
```

---

#### `v4_info`（IPv4 连接信息结构体）

```c
struct v4_info {
    char                     name[16];   // 网络接口名称
    ql_data_call_state_e     state;      // 拨号连接状态
    bool                     reconnect;  // 是否自动重连
    struct v4_address_status addr;       // IPv4 地址信息
    struct pkt_stats         stats;      // IPv4 数据包统计
};
```

---

#### `v6_info`（IPv6 连接信息结构体）

```c
struct v6_info {
    char                     name[16];   // 网络接口名称
    ql_data_call_state_e     state;      // 拨号连接状态
    bool                     reconnect;  // 是否自动重连
    struct v6_address_status addr;       // IPv6 地址信息
    struct pkt_stats         stats;      // IPv6 数据包统计
};
```

---

#### `ql_data_call_info_s`（数据拨号完整信息结构体）

```c
typedef struct {
    char                     profile_idx;  // UMTS/CDMA profile ID
    ql_data_call_ip_family_e ip_family;    // IP 版本
    struct v4_info           v4;           // IPv4 信息
    struct v6_info           v6;           // IPv6 信息
} ql_data_call_info_s;
```

---

#### `ql_apn_info_s`（APN 配置信息结构体）

```c
typedef struct {
    unsigned char       profile_idx;                  // UMTS/CDMA profile ID
    ql_apn_pdp_type_e   pdp_type;                     // PDP 类型
    ql_apn_auth_proto_e auth_proto;                    // 鉴权协议
    char                apn_name[QL_APN_NAME_SIZE];   // APN 名称
    char                username[QL_APN_USERNAME_SIZE]; // 鉴权用户名
    char                password[QL_APN_PASSWORD_SIZE]; // 鉴权密码
} ql_apn_info_s;
```

---

#### `ql_apn_add_s`（新增 APN 实例结构体）

```c
typedef struct {
    ql_apn_pdp_type_e   pdp_type;                     // PDP 类型
    ql_apn_auth_proto_e auth_proto;                    // 鉴权协议
    char                apn_name[QL_APN_NAME_SIZE];   // APN 名称
    char                username[QL_APN_USERNAME_SIZE]; // 鉴权用户名
    char                password[QL_APN_PASSWORD_SIZE]; // 鉴权密码
} ql_apn_add_s;
```

> 与 `ql_apn_info_s` 的区别：没有 `profile_idx` 字段，`profile_idx` 由系统分配后通过 `QL_APN_Add()` 输出参数返回。

---

#### `ql_apn_info_list_s`（APN 配置列表结构体）

```c
typedef struct {
    int           cnt;                     // APN 数量
    ql_apn_info_s apn[QL_APN_MAX_LIST];   // APN 配置列表
} ql_apn_info_list_s;
```

---

### 5.2 函数详解

---

### `QL_Data_Call_Init`

#### 功能描述

初始化模块数据拨号，并注册回调函数。

#### 函数原型

```c
int QL_Data_Call_Init(ql_data_call_evt_cb_t evt_cb)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `evt_cb` | `ql_data_call_evt_cb_t` | 输入（In） | 回调函数指针，当数据拨号状态变化时被调用 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 执行成功 |
| `-1` | 执行失败 |

---

### `QL_Data_Call_Destroy`

#### 功能描述

释放模块数据拨号资源，注销回调函数。

#### 函数原型

```c
void QL_Data_Call_Destroy(void)
```

#### 参数说明

无参数

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 执行成功 |
| `-1` | 执行失败 |

---

### `QL_Data_Call_Start`

#### 功能描述

开始数据拨号，建立数据业务通道。

#### 函数原型

```c
int QL_Data_Call_Start(ql_data_call_s *data_call, ql_data_call_error_e *err)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `data_call` | `ql_data_call_s *` | 输入（In） | 数据拨号参数（profile_idx、ip_family、reconnect 等） |
| `err` | `ql_data_call_error_e *` | 输出（Out） | 数据拨号返回的错误码 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 执行成功 |
| `-1` | 执行失败 |

---

### `QL_Data_Call_Stop`

#### 功能描述

结束数据拨号，断开指定数据业务通道。

#### 函数原型

```c
int QL_Data_Call_Stop(
    char                     profile_idx,
    ql_data_call_ip_family_e ip_family,
    ql_data_call_error_e     *err
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `profile_idx` | `char` | 输入（In） | UMTS/CDMA profile ID，指定要停止的拨号通道 |
| `ip_family` | `ql_data_call_ip_family_e` | 输入（In） | IP 版本 |
| `err` | `ql_data_call_error_e *` | 输出（Out） | 数据拨号返回的错误码 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 执行成功 |
| `-1` | 执行失败 |

---

### `QL_Data_Call_Info_Get`

#### 功能描述

获取数据拨号信息（IP 地址、网关、DNS、统计数据等）。

#### 函数原型

```c
int QL_Data_Call_Info_Get(
    char                     profile_idx,
    ql_data_call_ip_family_e ip_family,
    ql_data_call_info_s      *info,
    ql_data_call_error_e     *err
)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `profile_idx` | `char` | 输入（In） | UMTS/CDMA profile ID |
| `ip_family` | `ql_data_call_ip_family_e` | 输入（In） | IP 版本 |
| `info` | `ql_data_call_info_s *` | 输出（Out） | 数据拨号完整信息（含 IP、网关、DNS、统计等） |
| `err` | `ql_data_call_error_e *` | 输出（Out） | 数据拨号返回的错误码 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 执行成功 |
| `-1` | 执行失败 |

#### 典型用途

```c
ql_data_call_info_s info;
ql_data_call_error_e err;
// 获取 profile 1（APN1）的拨号信息
QL_Data_Call_Info_Get(1, QL_DATA_CALL_TYPE_IPV4, &info, &err);
// 获取网关地址（用于路由设置）
struct in_addr gw = info.v4.addr.gateway;
// 获取 DNS 地址（用于 DNS 路由设置）
struct in_addr pri_dns = info.v4.addr.pri_dns;
```

---

### `QL_APN_Set`

#### 功能描述

更改 APN 配置文件中的设置。若文件不存在，将创建新的配置文件。配置后自动保存，重启有效。

#### 函数原型

```c
int QL_APN_Set(ql_apn_info_s *apn)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `apn` | `ql_apn_info_s *` | 输入（In） | APN 配置信息（含 profile_idx、pdp_type、auth_proto、apn_name、username、password） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 执行成功 |
| `-1` | 执行失败 |

---

### `QL_APN_Get`

#### 功能描述

读取指定 profile 的 APN 配置文件内容。

#### 函数原型

```c
int QL_APN_Get(unsigned char profile_idx, ql_apn_info_s *apn)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `profile_idx` | `unsigned char` | 输入（In） | UMTS/CDMA profile ID |
| `apn` | `ql_apn_info_s *` | 输出（Out） | 读取到的 APN 配置信息 |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 执行成功 |
| `-1` | 执行失败 |

---

### `QL_APN_Add`

#### 功能描述

添加一路新的 APN 实例，系统分配 profile_idx 并返回。

#### 函数原型

```c
int QL_APN_Add(ql_apn_add_s *apn, unsigned char *profile_idx)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `apn` | `ql_apn_add_s *` | 输入（In） | 新 APN 配置信息（不含 profile_idx） |
| `profile_idx` | `unsigned char *` | 输出（Out） | 系统分配的 UMTS/CDMA profile ID |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 执行成功 |
| `-1` | 执行失败 |

---

### `QL_APN_Del`

#### 功能描述

删除指定 profile 的 APN 配置文件。

#### 函数原型

```c
int QL_APN_Del(unsigned char profile_idx)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `profile_idx` | `unsigned char` | 输入（In） | 要删除的 UMTS/CDMA profile ID |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 执行成功 |
| `-1` | 执行失败 |

---

### `QL_APN_Get_Lists`

#### 功能描述

读取所有 APN 配置文件列表中的设置。

#### 函数原型

```c
int QL_APN_Get_Lists(ql_apn_info_list_s *apn_list)
```

#### 参数说明

| 参数名 | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `apn_list` | `ql_apn_info_list_s *` | 输出（Out） | 所有 APN 配置信息列表（含数量和各 APN 详情） |

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 执行成功 |
| `-1` | 执行错误 |

---

### `QL_Data_Call_Init_Precondition`

#### 功能描述

获取 Quectel Manager 服务的运行状态，用于判断数据拨号服务是否已启动完成。

#### 函数原型

```c
int QL_Data_Call_Init_Precondition(void)
```

#### 参数说明

无参数

#### 返回值

| 返回值 | 含义 |
|-------|------|
| `0` | 服务工作正常，可以进行拨号 |
| `-1` | 服务不正常，暂时无法拨号 |

#### 使用场景

解决模块开机拨号失败问题：模块开机自动运行 APP 时，拨号服务进程可能尚未启动完成。调用此函数轮询直到返回 0，再进行拨号。

```c
// 等待拨号服务启动完成
int ret;
do {
    ret = QL_Data_Call_Init_Precondition();
    if (ret != 0) {
        sleep(1);
    }
} while (ret != 0);
// 服务就绪，可以拨号
QL_Data_Call_Init(user_callback);
```

---

## 6. 常见问题排查

### 6.1 抓取 PCAP 日志

```bash
# 抓取所有网口 PCAP 日志
tcpdump -i any -p -vv -s 0 -w ./capture1.pcap &

# 抓取指定网口 PCAP 日志（如 rmnet_data0）
tcpdump -i rmnet_data0 -p -vv -s 0 -w ./capture1.pcap &

# 运行程序后中断 tcpdump，上传 capture1.pcap 文件分析
```

### 6.2 查看 iptables 表

```bash
# 查看 nat 表
iptables -nvt nat -L

# 查看 filter 表
iptables -nvt filter -L
```

### 6.3 3GPP2/CDMA 因鉴权无法拨号上网

**Modem 侧检查**：
```
AT+QCTPWDCFG="<username>","<userpasswd>"
AT+QCFG="cdmaruim",1
AT+QIACT=1  （失败返回 ERROR 多数是用户名密码不正确）
```

**AP 侧检查**：
- 检查 `profile_id` 是否为 0
- 检查鉴权参数是否正确（参照第 3.2 章和第 3.5 章）

### 6.4 模块开机拨号失败

**原因**：开机时拨号服务进程尚未启动完成。

**解决方案**：使用 `QL_Data_Call_Init_Precondition()` 轮询等待服务就绪后再拨号。

### 6.5 发送 DNS Server IPv6 Address Request 失败

**原因**：采用 IPv4&IPv6 拨号时，当前网络不支持 IPv6。

**解决方案**：在调用 `QL_Data_Call_Start` 时，指定 `ip_family` 为 `QL_DATA_CALL_TYPE_IPV4`，仅使用 IPv4 拨号。

```c
data_call_paras.ip_family = QL_DATA_CALL_TYPE_IPV4;  // 改为仅 IPv4
```

---

## 7. 错误码汇总

| 错误码 | 值 | 含义 |
|-------|---|------|
| `QL_DATA_CALL_ERROR_NONE` | 0 | 无错误 |
| `QL_DATA_CALL_ERROR_INVALID_PARAMS` | 1 | 无效参数 |
| 函数返回 `0` | — | 执行成功 |
| 函数返回 `-1` | — | 执行失败 |

---

## 8. 版本与兼容性信息

| 项目 | 信息 |
|-----|------|
| 文档版本 | V1.0 |
| 文档日期 | 2020-05-31 |
| 文档状态 | 受控文件 |
| 作者 | 王辉/钱润生/匡昌胜/钱云绿/周守亚 |
| 适用模块 | EC2x（EC25/EC21/EC20 R2.1/EC20-CN）、EG9x（EG95/EG91）、EG25-G |
| 参考文档 | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_ECM_用户指导；QuecOpen_RNDIS_用户指导 |

---

## 9. 附录：术语缩写

| 术语 | 英文全称 | 中文全称 |
|-----|---------|---------|
| 3GPP2 | 3rd Generation Partnership Project 2 | 第三代合作伙伴计划 2 |
| API | Application Programming Interface | 应用程序编程接口 |
| APN | Access Point Name | 接入点名称 |
| CDMA | Code Division Multiple Access | 码分多址 |
| DNS | Domain Name System | 域名系统 |
| ECM | Ethernet Networking Control Model | 以太网控制模型 |
| eHRPD | Evolved High Rate Package Data | 演进的高速分组网络 |
| HRPD | High Rate Packet Data | 高速率分组数据 |
| IPv4 | Internet Protocol version 4 | 网际协议版本 4 |
| IPv6 | Internet Protocol version 6 | 网际协议版本 6 |
| LTE | Long Term Evolution | 长期演进 |
| PDP | Packet Data Protocol | 分组数据协议 |
| RNDIS | Remote Network Driver Interface Specification | 远程网络驱动接口规范 |
| SDK | Software Development Kit | 软件开发工具包 |
| SIM | Subscriber Identification Module | 用户身份识别卡 |
| UART | Universal Asynchronous Receiver/Transmitter | 通用异步收发传输器 |
| (U)SIM | (Universal) Subscriber Identification Module | （全球）用户身份识别卡 |
| USB | Universal Serial Bus | 通用串行总线 |
| UMTS | Universal Mobile Telecommunications System | 通用移动通信系统 |

<!-- GENERATION_COMPLETE: 2026-05-28 -->
