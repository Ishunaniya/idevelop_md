# EC2x&EG9x&EG25-G 系列 QuecOpen Wi-Fi API 参考手册 V1.0

> **文档信息**
> - 适用平台：LTE Standard 模块系列（EC2x、EG9x、EG25-G）
> - 版本：1.0
> - 日期：2021-03-15
> - 状态：受控文件
> - 作者：Arthur CHEN
> - 发布方：上海移远通信技术股份有限公司
> - 总页数：36 页

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2021-01-27 | Arthur CHEN | 创建文档 |
| 1.0 | 2021-03-15 | Arthur CHEN | 受控版本 |

---

## 目录

1. [引言](#1-引言)
   - 1.1 适用模块
2. [Wi-Fi 接口函数](#2-wi-fi-接口函数)
   - 2.1 头文件
   - 2.2 函数概览
   - 2.3 函数详解（2.2.1 ~ 2.2.36）
3. [Wi-Fi 接口使用实例](#3-wi-fi-接口使用实例)
   - 3.1 设置 STA 模式
   - 3.2 设置 AP 模式
   - 3.3 设置 AP-AP 模式
   - 3.4 设置 AP-STA 模式
4. [附录 A 术语缩写](#4-附录-a-术语缩写)

---

## 表格索引

| 表格 | 页码 |
|---|---|
| 表 1：适用模块 | 6 |
| 表 2：函数概览 | 8 |
| 表 3：参考文档 | 36 |
| 表 4：术语缩写 | 36 |

---

## 1 引言

移远通信 LTE Standard EC2x 系列、EG9x 系列和 EG25-G 模块支持 QuecOpen® 方案；QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。有关 QuecOpen® 的详细信息，请参考**文档 [1]**。

本文档主要介绍移远通信 EC2x 系列、EG9x 系列和 EG25-G QuecOpen® 模块 **Wi-Fi 功能**相关的 API 及相关实例。

EC2x 系列、EG9x 系列和 EG25-G QuecOpen® 模块的 Wi-Fi 功能需搭配移远通信 Wi-Fi & 蓝牙模块 **FC20 系列**使用，有关 Wi-Fi & 蓝牙模块 FC20 系列的详细信息，请参考《Quectel_FC20 系列_硬件设计手册》。移远通信 FC20 系列支持四种工作模式，分别为：

### 1) AP 模式
AP 模式下，FC20 系列模块仅开放一个热点，支持 2.4 GHz 或 5 GHz 频段，但是每次只能启动其中一种频段。

### 2) STA 模式
STA 模式下，FC20 系列模块作为一个无线网卡使用，能够连接 2.4 GHz 和 5 GHz 频段的热点。

### 3) AP-STA 共存模式
AP-STA 共存模式下，FC20 系列模块可以同时运行 AP 模式和 STA 模式。

### 4) AP-AP 模式
AP-AP 模式下，FC20 系列模块可以同时运行两个热点，最常用的场景：设置相同的 SSID，一个覆盖 2.4 GHz 频段，一个覆盖 5 GHz 频段。

### 1.1 适用模块

**表 1：适用模块**

| 模块系列 | 模块 |
|---|---|
| EC2x 系列 | EC25 系列 |
| EC2x 系列 | EC21 系列 |
| EC2x 系列 | EC20 R2.1 |
| EG9x 系列 | EG95 系列 |
| EG9x 系列 | EG91 系列 |
| EG25-G | EG25-G |

---

## 2 Wi-Fi 接口函数

### 2.1 头文件

Wi-Fi 接口头文件 `ql_wifi.h` 位于 SDK 包中：

```
ql-ol-crosstool/sysroots/armv7a-vfp-neon-oe-linux-gnueabi/usr/include/quectel-openlinux-sdk/
```

> **备注**：若无特别说明，所有 Wi-Fi 接口函数均**不支持并发调用**。

### 2.2 函数概览

**表 2：函数概览**

| 函数 | 说明 |
|---|---|
| `ql_wifi_register_handle` | 注册 Wi-Fi 状态上报的回调函数 |
| `ql_wifi_unregister_handle` | 注销 Wi-Fi 回调函数 |
| `ql_wifi_enable` | 开启 Wi-Fi 功能（同步） |
| `ql_wifi_disable` | 关闭 Wi-Fi 功能（同步） |
| `ql_wifi_enable_async` | 异步开启 Wi-Fi 功能 |
| `ql_wifi_disable_async` | 异步关闭 Wi-Fi 功能 |
| `ql_wifi_module_enable` | 根据当前设置的工作模式加载 Wi-Fi 驱动 |
| `ql_wifi_module_disable` | 根据当前设置的工作模式卸载 Wi-Fi 驱动 |
| `ql_wifi_work_mode_set` | 设置当前工作模式 |
| `ql_wifi_work_mode_get` | 获取当前工作模式 |
| `ql_wifi_ap_ssid_set` | 设置 AP 模式的 SSID |
| `ql_wifi_ap_ssid_get` | 获取 AP 模式的 SSID |
| `ql_wifi_ap_ssid_hide_set` | 设置 AP 模式隐藏 SSID |
| `ql_wifi_ap_ssid_hide_get` | 获取 AP 模式隐藏 SSID 状态 |
| `ql_wifi_ap_mode_set` | 设置 AP 模式工作协议模式 |
| `ql_wifi_ap_mode_get` | 获取 AP 模式工作协议模式 |
| `ql_wifi_ap_bandwidth_set` | 设置 AP 模式带宽 |
| `ql_wifi_ap_bandwidth_get` | 获取 AP 模式带宽 |
| `ql_wifi_ap_channel_set` | 设置 AP 模式信道 |
| `ql_wifi_ap_channel_get` | 获取 AP 模式信道 |
| `ql_wifi_ap_auth_set` | 设置 AP 模式安全认证 |
| `ql_wifi_ap_auth_get` | 获取 AP 模式安全认证 |
| `ql_wifi_ap_max_sta_set` | 设置 AP 模式终端最大连接数量 |
| `ql_wifi_ap_max_sta_get` | 获取 AP 模式终端最大连接数量 |
| `ql_wifi_ap_sta_info_get` | 获取 AP 模式的客户端信息 |
| `ql_wifi_ap_start` | 开启 AP 模式上层应用 hostapd |
| `ql_wifi_ap_stop` | 关闭 AP 模式上层应用 hostapd |
| `ql_wifi_ap_restart` | 重启 AP 模式上层应用 hostapd |
| `ql_wifi_status_get` | 获取当前 Wi-Fi 状态 |
| `ql_wifi_sta_ssid_set` | 设置 STA 模式的 SSID |
| `ql_wifi_sta_ssid_get` | 获取 STA 模式的 SSID |
| `ql_wifi_sta_auth_set` | 设置 STA 模式安全认证 |
| `ql_wifi_sta_auth_get` | 获取 STA 模式安全认证 |
| `ql_wifi_sta_connect` | 开启 STA 模式上层应用 wpa_supplicant |
| `ql_wifi_sta_disconnect` | 关闭 STA 模式上层应用 wpa_supplicant |
| `ql_wifi_sta_status` | 获取 STA 模式连接状态 |

### 2.3 函数详解

---

#### 2.2.1 ql_wifi_register_handle

该函数用于注册 Wi-Fi 状态上报的回调函数。

**函数原型**
```c
int ql_wifi_register_handle(wifi_event_handle event_handle, void *arg);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `event_handle` | [In] | 定义上报的回调函数 |
| `arg` | [In] | 用户自定义参数 |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 注册成功 |
| -1 | 注册失败 |

---

#### 2.2.2 ql_wifi_unregister_handle

该函数用于注销 Wi-Fi 回调函数。

**函数原型**
```c
void ql_wifi_unregister_handle(void);
```

**参数**：无

**返回值**：无

---

#### 2.2.3 ql_wifi_enable

该函数用于开启 Wi-Fi 功能（同步方式）。

**函数原型**
```c
int ql_wifi_enable(void);
```

**参数**：无

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 开启成功 |
| -1 | 开启失败 |

---

#### 2.2.4 ql_wifi_disable

该函数用于关闭 Wi-Fi 功能（同步方式）。

**函数原型**
```c
int ql_wifi_disable(void);
```

**参数**：无

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 关闭成功 |
| -1 | 关闭失败 |

---

#### 2.2.5 ql_wifi_enable_async

该函数用于异步开启 Wi-Fi 功能。

**函数原型**
```c
int ql_wifi_enable_async(void);
```

**参数**：无

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 异步开启成功 |
| -1 | 异步开启失败 |

---

#### 2.2.6 ql_wifi_disable_async

该函数用于异步关闭 Wi-Fi 功能。

**函数原型**
```c
int ql_wifi_disable_async(void);
```

**参数**：无

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 异步关闭成功 |
| -1 | 异步关闭失败 |

---

#### 2.2.7 ql_wifi_module_enable

该函数用于根据当前设置的工作模式加载 Wi-Fi 驱动。

**函数原型**
```c
int ql_wifi_module_enable(void);
```

**参数**：无

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 驱动加载成功 |
| -1 | 驱动加载失败 |

---

#### 2.2.8 ql_wifi_module_disable

该函数用于根据当前设置的工作模式卸载 Wi-Fi 驱动。

**函数原型**
```c
int ql_wifi_module_disable(void);
```

**参数**：无

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 卸载驱动成功 |
| -1 | 卸载驱动失败 |

---

#### 2.2.9 ql_wifi_work_mode_set

该函数用于设置当前工作模式。

**函数原型**
```c
int ql_wifi_work_mode_set(ql_wifi_work_mode_e mode);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `mode` | [In] | 设置工作模式。 |

`mode` 枚举值（`ql_wifi_work_mode_e`）：

| 枚举值 | 说明 |
|---|---|
| `QL_WIFI_WORK_MODE_STA` | STA 模式 |
| `QL_WIFI_WORK_MODE_AP0` | AP 模式 |
| `QL_WIFI_WORK_MODE_AP0_STA` | AP-STA 模式 |
| `QL_WIFI_WORK_MODE_AP0_AP1` | AP-AP 模式 |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 设置工作模式成功 |
| -1 | 设置工作模式失败 |

---

#### 2.2.10 ql_wifi_work_mode_get

该函数用于获取当前工作模式。

**函数原型**
```c
int ql_wifi_work_mode_get(ql_wifi_work_mode_e *mode);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `mode` | [Out] | 获取当前工作模式。 |

`mode` 枚举值（`ql_wifi_work_mode_e`）：

| 枚举值 | 说明 |
|---|---|
| `QL_WIFI_WORK_MODE_STA` | STA 模式 |
| `QL_WIFI_WORK_MODE_AP0` | AP 模式 |
| `QL_WIFI_WORK_MODE_AP0_STA` | AP-STA 模式 |
| `QL_WIFI_WORK_MODE_AP0_AP1` | AP-AP 模式 |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 获取当前工作模式成功 |
| -1 | 获取当前工作模式失败 |

---

#### 2.2.11 ql_wifi_ap_ssid_set

该函数用于设置 AP 模式的 SSID。

**函数原型**
```c
int ql_wifi_ap_ssid_set(ql_wifi_ap_index_e idx, char *ssid);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `ssid` | [In] | 设置的 SSID。**SSID 不可超过 32 字节，且不为空。** |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 设置 SSID 成功 |
| -1 | 设置 SSID 失败 |

---

#### 2.2.12 ql_wifi_ap_ssid_get

该函数用于获取 AP 模式的 SSID。

**函数原型**
```c
int ql_wifi_ap_ssid_get(ql_wifi_ap_index_e idx, char *ssid);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `ssid` | [Out] | 获取的 SSID |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 获取 SSID 成功 |
| -1 | 获取 SSID 失败 |

---

#### 2.2.13 ql_wifi_ap_ssid_hide_set

该函数用于设置 AP 模式隐藏 SSID。

**函数原型**
```c
int ql_wifi_ap_ssid_hide_set(ql_wifi_ap_index_e idx, bool hide);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `hide` | [In] | 设置 AP 模式是否隐藏 SSID。`true`=开启隐藏 SSID；`false`=关闭隐藏 SSID |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 设置隐藏 SSID 成功 |
| -1 | 设置隐藏 SSID 失败 |

---

#### 2.2.14 ql_wifi_ap_ssid_hide_get

该函数用于获取 AP 模式隐藏 SSID 状态。

**函数原型**
```c
int ql_wifi_ap_ssid_hide_get(ql_wifi_ap_index_e idx, bool *hide);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `hide` | [Out] | 获取隐藏 SSID 状态 |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 获取隐藏 SSID 状态成功 |
| -1 | 获取隐藏 SSID 状态失败 |

---

#### 2.2.15 ql_wifi_ap_mode_set

该函数用于设置 AP 模式工作协议模式。

**函数原型**
```c
int ql_wifi_ap_mode_set(ql_wifi_ap_index_e idx, ql_wifi_mode_type_e mode);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `mode` | [In] | 工作协议模式。 |

`mode` 枚举值（`ql_wifi_mode_type_e`）：

| 枚举值 | 说明 |
|---|---|
| `QL_WIFI_MODE_80211B` | IEEE 802.11b（2.4 GHz） |
| `QL_WIFI_MODE_80211BG` | IEEE 802.11b/g（2.4 GHz） |
| `QL_WIFI_MODE_80211BGN` | IEEE 802.11b/g/n（2.4 GHz） |
| `QL_WIFI_MODE_80211A` | IEEE 802.11a（5 GHz） |
| `QL_WIFI_MODE_80211AN` | IEEE 802.11a/n（5 GHz） |
| `QL_WIFI_MODE_80211AC` | IEEE 802.11a/c（5 GHz） |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 设置工作协议模式成功 |
| -1 | 设置工作协议模式失败 |

---

#### 2.2.16 ql_wifi_ap_mode_get

该函数用于获取 AP 模式工作协议模式。

**函数原型**
```c
int ql_wifi_ap_mode_get(ql_wifi_ap_index_e idx, ql_wifi_mode_type_e *mode);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `mode` | [Out] | 工作协议模式（枚举同上） |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 获取工作协议模式成功 |
| -1 | 获取工作协议模式失败 |

---

#### 2.2.17 ql_wifi_ap_bandwidth_set

该函数用于设置 AP 模式带宽。

**函数原型**
```c
int ql_wifi_ap_bandwidth_set(ql_wifi_ap_index_e idx, ql_wifi_bandwidth_type_e bandwidth);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `bandwidth` | [In] | 工作带宽。 |

`bandwidth` 枚举值（`ql_wifi_bandwidth_type_e`）：

| 枚举值 | 说明 |
|---|---|
| `QL_WIFI_BANDWIDTH_HT20` | 20 MHz |
| `QL_WIFI_BANDWIDTH_HT40` | 40 MHz |
| `QL_WIFI_BANDWIDTH_HT80` | 80 MHz |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 设置带宽成功 |
| -1 | 获取带宽失败 |

---

#### 2.2.18 ql_wifi_ap_bandwidth_get

该函数用于获取 AP 模式带宽。

**函数原型**
```c
int ql_wifi_ap_bandwidth_get(ql_wifi_ap_index_e idx, ql_wifi_bandwidth_type_e bandwidth);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `bandwidth` | [Out] | 工作带宽（枚举同上，`QL_WIFI_BANDWIDTH_HT20` / `HT40` / `HT80`） |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 获取带宽成功 |
| -1 | 获取带宽失败 |

---

#### 2.2.19 ql_wifi_ap_channel_set

该函数用于设置 AP 模式信道。

**函数原型**
```c
int ql_wifi_ap_channel_set(ql_wifi_ap_index_e idx, int channel);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `channel` | [In] | 设置 2.4 G 或者 5 G 的信道。 |

可用信道：

| 频段 | 信道值 |
|---|---|
| 2.4 G 信道 | 0/1/2/3/4/5/6/7/8/9/10/11/12/13/14 |
| 5 G 信道 | 36/40/44/48/52/56/60/64/100/104/108/112/116/120/124/128/132/136/140/144/149/153/157/161/165/175/181 |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 设置信道成功 |
| -1 | 设置信道失败 |

---

#### 2.2.20 ql_wifi_ap_channel_get

该函数用于获取 AP 模式信道。

**函数原型**
```c
int ql_wifi_ap_channel_get(ql_wifi_ap_index_e idx, int *channel);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `channel` | [Out] | 获取 2.4 G 或者 5 G 信道（取值范围同 set） |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 获取信道成功 |
| -1 | 获取信道失败 |

---

#### 2.2.21 ql_wifi_ap_auth_set

该函数用于设置 AP 模式安全认证。

**函数原型**
```c
int ql_wifi_ap_auth_set(ql_wifi_ap_index_e idx, ql_wifi_ap_auth_s *auth);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `auth` | [In] | 安全认证。详见 [2.2.22.1 ql_wifi_ap_auth_s](#22221-ql_wifi_ap_auth_s) |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 设置安全认证成功 |
| -1 | 设置安全认证失败 |

---

#### 2.2.22 ql_wifi_ap_auth_get

该函数用于获取 AP 模式安全认证。

**函数原型**
```c
int ql_wifi_ap_auth_get(ql_wifi_ap_index_e idx, ql_wifi_ap_auth_s *auth);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `auth` | [Out] | 安全认证。详见 [2.2.22.1 ql_wifi_ap_auth_s](#22221-ql_wifi_ap_auth_s) |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 获取安全认证成功 |
| -1 | 获取安全认证失败 |

##### 2.2.22.1 ql_wifi_ap_auth_s

安全认证类型定义如下，通常只使用 union 里 `wpa_psk`：

```c
typedef struct {
    int auth;
    union {
        struct {
            int  default_index;
            char passwd[4][64];
        } wep;

        struct {
            short pairwise;
            char  passwd[64];
            int   group_rekey;
        } wpa_psk;
    };
} ql_wifi_ap_auth_s;
```

| 类型 | 参数 | 说明 |
|---|---|---|
| `int` | `auth` | 安全类型（0=OPEN，2=WPA PSK，3=WPA2 PSK，4=WPA-PSK&WPA2-PSK） |
| `int` | `default_index` | WEP 的配置索引 |
| `char` | `passwd[4][64]` | WEP 的密码（最多 4 组） |
| `short` | `pairwise` | 加密方式（0=AUTH，1=TKIP，2=AES） |
| `char` | `passwd[64]` | WPA/WPA2 的密码（最多 64 字节） |
| `int` | `group_rekey` | 组密钥更新间隔（秒） |

---

#### 2.2.23 ql_wifi_ap_max_sta_set

该函数用于设置 AP 模式终端最大连接数量。

**函数原型**
```c
int ql_wifi_ap_max_sta_set(ql_wifi_ap_index_e idx, int max_sta_num);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `max_sta_num` | [In] | 设置 AP 模式终端最大连接数量，**最大不超过 16**。 |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 设置终端最大连接数量成功 |
| -1 | 设置终端最大连接数量失败 |

---

#### 2.2.24 ql_wifi_ap_max_sta_get

该函数用于获取 AP 模式终端最大连接数量。

**函数原型**
```c
int ql_wifi_ap_max_sta_get(ql_wifi_ap_index_e idx, int *max_sta_num);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `max_sta_num` | [Out] | 获取 AP 模式终端最大连接数量 |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 获取终端最大连接数量成功 |
| -1 | 获取终端最大连接数量失败 |

---

#### 2.2.25 ql_wifi_ap_sta_info_get

该函数用于获取 AP 模式的客户端信息。

**函数原型**
```c
int ql_wifi_ap_sta_info_get(ql_wifi_ap_index_e idx, ql_wifi_ap_sta_info_s *sta_info);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |
| `sta_info` | [Out] | 获取的客户端信息。详见 [2.2.25.1 ql_wifi_ap_sta_info_s](#22251-ql_wifi_ap_sta_info_s) |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 获取客户端信息成功 |
| -1 | 获取客户端信息失败 |

##### 2.2.25.1 ql_wifi_ap_sta_info_s

客户端信息定义如下：

```c
typedef struct {
    int num;
    struct {
        unsigned char mac[6];
        in_addr_t     addr;
        char          hostname[33];
    } sta[16];
} ql_wifi_ap_sta_info_s;
```

| 类型 | 参数 | 说明 |
|---|---|---|
| `int` | `num` | 客户端设备数量 |
| `unsigned char` | `mac[6]` | 设备 MAC 地址（6 字节） |
| `in_addr_t` | `addr` | 设备 IP 地址 |
| `char` | `hostname[33]` | 主机名称（最多 32 字符） |

---

#### 2.2.26 ql_wifi_ap_start

该函数用于开启 AP 模式上层应用 **hostapd**，使新的配置信息生效。

**函数原型**
```c
int ql_wifi_ap_start(ql_wifi_ap_index_e idx);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 开启上层应用 hostapd 成功 |
| -1 | 开启上层应用 hostapd 失败 |

---

#### 2.2.27 ql_wifi_ap_stop

该函数用于关闭 AP 模式上层应用 **hostapd**。

**函数原型**
```c
int ql_wifi_ap_stop(ql_wifi_ap_index_e idx);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 关闭上层应用 hostapd 成功 |
| -1 | 关闭上层应用 hostapd 失败 |

---

#### 2.2.28 ql_wifi_ap_restart

该函数用于重启 AP 模式上层应用 **hostapd**，使新的配置信息生效。

**函数原型**
```c
int ql_wifi_ap_restart(ql_wifi_ap_index_e idx);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `idx` | [In] | AP 模式索引。`QL_WIFI_AP_INDEX_AP0`=热点 0；`QL_WIFI_AP_INDEX_AP1`=热点 1 |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 重启上层应用 hostapd 成功 |
| -1 | 重启上层应用 hostapd 失败 |

---

#### 2.2.29 ql_wifi_status_get

该函数用于获取当前 Wi-Fi 状态。

**函数原型**
```c
int ql_wifi_status_get(ql_wifi_status_e *status);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `status` | [Out] | 当前 Wi-Fi 状态信息。 |

`status` 枚举值（`ql_wifi_status_e`）：

| 枚举值 | 说明 |
|---|---|
| `QL_WIFI_STATUS_ENABLE` | 当前为开启状态 |
| `QL_WIFI_STATUS_DISABLE` | 当前为关闭状态 |
| `QL_WIFI_STATUS_ERR_DRIVER` | 驱动错误 |
| `QL_WIFI_STATUS_ERR_SOFTWARE` | 软件错误 |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 获取 Wi-Fi 状态成功 |
| -1 | 获取 Wi-Fi 状态失败 |

---

#### 2.2.30 ql_wifi_sta_ssid_set

该函数用于设置 STA 模式连接的 SSID。

**函数原型**
```c
int ql_wifi_sta_ssid_set(char *ssid);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `ssid` | [In] | 设置的 SSID。**SSID 不可超过 32 字节，且不为空。** |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 设置 SSID 成功 |
| -1 | 设置 SSID 失败 |

---

#### 2.2.31 ql_wifi_sta_ssid_get

该函数用于获取 STA 模式连接的 SSID。

**函数原型**
```c
int ql_wifi_sta_ssid_get(char *ssid);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `ssid` | [Out] | 获取的 SSID |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 获取 SSID 成功 |
| -1 | 获取 SSID 失败 |

---

#### 2.2.32 ql_wifi_sta_auth_set

该函数用于设置 STA 模式的安全认证。

**函数原型**
```c
int ql_wifi_sta_auth_set(ql_wifi_sta_auth_s *auth);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `auth` | [In] | 设置安全认证。详见 [2.2.33.1 ql_wifi_sta_auth_s](#22331-ql_wifi_sta_auth_s) |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 设置 STA 模式安全认证成功 |
| -1 | 设置 STA 模式安全认证失败 |

---

#### 2.2.33 ql_wifi_sta_auth_get

该函数用于获取 STA 模式的安全认证。

**函数原型**
```c
int ql_wifi_sta_auth_get(ql_wifi_sta_auth_s *auth);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `auth` | [Out] | 获取安全认证。详见 [2.2.33.1 ql_wifi_sta_auth_s](#22331-ql_wifi_sta_auth_s) |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 获取 STA 模式安全认证成功 |
| -1 | 获取 STA 模式安全认证失败 |

##### 2.2.33.1 ql_wifi_sta_auth_s

安全认证信息定义如下：

```c
typedef struct {
    int auth;
    union {
        struct {
            char passwd[64];
        } wep;

        struct {
            short pairwise;
            char  passwd[64];
        } wpa_psk;
    };
} ql_wifi_sta_auth_s;
```

| 类型 | 参数 | 说明 |
|---|---|---|
| `int` | `auth` | 安全类型（0=OPEN，2=WPA PSK，3=WPA2 PSK，4=WPA2&WPA） |
| `char` | `passwd[64]` | WEP 的密码 |
| `short` | `pairwise` | 加密方式（0=AUTH，1=TKIP，2=AES） |
| `char` | `passwd[64]` | WPA/WPA2 的密码 |

---

#### 2.2.34 ql_wifi_sta_connect

该函数用于开启 STA 模式的上层应用 **wpa_supplicant**，使新的配置信息生效。

**函数原型**
```c
int ql_wifi_sta_connect(void);
```

**参数**：无

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 开启 STA 模式上层应用成功 |
| -1 | 开启 STA 模式上层应用失败 |

---

#### 2.2.35 ql_wifi_sta_disconnect

该函数用于关闭 STA 模式的上层应用 **wpa_supplicant**。

**函数原型**
```c
int ql_wifi_sta_disconnect(void);
```

**参数**：无

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 关闭上层应用成功 |
| -1 | 关闭上层应用失败 |

---

#### 2.2.36 ql_wifi_sta_status

该函数获取 STA 模式的连接状态。

**函数原型**
```c
int ql_wifi_sta_status(ql_wifi_station_status_e *status);
```

**参数**

| 参数 | 方向 | 说明 |
|---|---|---|
| `status` | [Out] | STA 模式的连接状态。 |

`status` 枚举值（`ql_wifi_station_status_e`）：

| 枚举值 | 说明 |
|---|---|
| `QL_WIFI_STATION_DISABLE` | STA 模式没有打开 |
| `QL_WIFI_STATION_CONNECTED` | STA 模式已连接状态 |
| `QL_WIFI_STATION_DISCONNECTED` | STA 模式断开 |

**返回值**

| 值 | 说明 |
|---|---|
| 0 | 获取 STA 模式的连接状态成功 |
| -1 | 获取 STA 模式的连接状态失败 |

---

## 3 Wi-Fi 接口使用实例

示例程序路径：
- STA 模式 / AP 模式：`example/wifi/example_wifi.c`
- AP-AP 模式：`example/wifi/ap_ap_mode.c`
- AP-STA 模式：`example/wifi/ap_sta_mode.c`

### 3.1 设置 STA 模式

参考例程 `example/wifi/example_wifi.c`，运行程序如下：

```
Please input case: 7
Please input WiFi handle register(0-register/1-unregister): 0    //注册回调函数。

case 1: Configure WiFi
case 2: Enable WiFi
case 3: Activate Hostapd
case 4: Activate WPA Supplicant
case 5: Get WPA Supplicant status
case 6: Get WiFi configuraction
case 7: Register Event Callback
case 8: Enable Module WiFi
case 9: Get WiFi status
case 100: exit
Please input case: 1
Please input WiFi work mode(0-STA/1-AP0/2-AP0+STA/3-AP0+AP1): 0  //设置 STA 工作模式。

Please input STA ssid(maximun size 32): quectel_test    //设置 SSID。

Please input STA type of authentication(0-OPEN/2-WPA PSK/3-WPA2 PSK/4-WPA2&WPA): 4
//设置认证模式。

Please input STA WPA PSK pairwise(0-AUTH/1-TKIP/2-AES): 0    //设置加密方式。

Please input STA WPA PSK password: 12345678    //设置密码。

...（配置完成后）...

Please input case: 2
Please input WiFi status(0-Disable/1-Enable/2-Async Enable/3-Async Disable): 2  //异步打开。
```

### 3.2 设置 AP 模式

参考例程 `example/wifi/example_wifi.c`，运行程序如下：

```
Please input case: 7
Please input WiFi handle register(0-register/1-unregister): 0    //注册回调函数。

...

Please input case: 1
Please input WiFi work mode(0-STA/1-AP0/2-AP0+STA/3-AP0+AP1): 1    //设置 AP 工作模式。

Please input WiFi index 0 ssid(maximun size 32): quectel_test    //设置 SSID。

Please input WiFi index 0 IEEE 802.11 mode(0-b/1-bg/2-bgn/3-a/4-an/5-ac): 2    //设置协议模式。

Please input WiFi index 0 Bandwidth(0-20MHz/1-40MHz/2-80MHz): 0    //设置带宽。

Please input WiFi index 0 channel: 6    //设置信道。

Please input WiFi index 0 maximun station(1-16): 16    //设置最大连接数量。

Please input WiFi index 0 type of authentication(0-OPEN/2-WPA PSK/3-WPA2 PSK/4-WPA-PSK&WPA2-PSK): 4
//设置认证模式。

Please input WiFi index 0 WPA PSK pairwise(0-AUTH/1-TKIP/2-AES): 0    //设置加密方式。

Please input WiFi index 0 WPA PSK password: 12345678    //设置密码。

...（配置完成后）...

Please input case: 2
Please input WiFi status(0-Disable/1-Enable/2-Async Enable/3-Async Disable): 2  //异步打开。
```

### 3.3 设置 AP-AP 模式

参考例程 `example/wifi/ap_ap_mode.c`，代码片段如下：

```c
ql_wifi_ap_auth_s auth_ap0;
ql_wifi_ap_auth_s auth_ap1;

/* setting AP0 */
ql_wifi_work_mode_set(QL_WIFI_WORK_MODE_AP0_AP1); // 设置 AP-AP 工作模式。
ql_wifi_ap_ssid_set(QL_WIFI_AP_INDEX_AP0, "Quectel-WORK_MODE_AP0_AP1"); // 设置 SSID。
ql_wifi_ap_mode_set(QL_WIFI_AP_INDEX_AP0, QL_WIFI_MODE_80211BGN); // 设置 b/g/n 模式。
ql_wifi_ap_bandwidth_set(QL_WIFI_AP_INDEX_AP0, QL_WIFI_BANDWIDTH_HT20); // 设置带宽。
ql_wifi_ap_channel_set(QL_WIFI_AP_INDEX_AP0, 11); // 设置 2.4G 信道。
auth_ap0.auth = QL_WIFI_AUTH_WPA_PSK; // 设置认证模式。
auth_ap0.wpa_psk.pairwise = QL_WIFI_AUTH_WPA_PAIRWISE_AUTO; // 设置加密方式。
auth_ap0.wpa_psk.group_rekey = 3600; /* one hour */ // 设置组密钥。
strcpy(auth_ap0.wpa_psk.passwd, "12345678"); // 设置密码。
ql_wifi_ap_auth_set(QL_WIFI_AP_INDEX_AP0, &auth_ap0);

/* setting AP1 */
ql_wifi_ap_ssid_set(QL_WIFI_AP_INDEX_AP1, "Quectel-WORK_MODE_AP0_AP1"); // 设置 SSID。
ql_wifi_ap_mode_set(QL_WIFI_AP_INDEX_AP1, QL_WIFI_MODE_80211AN); // 设置 a/n 模式。
ql_wifi_ap_bandwidth_set(QL_WIFI_AP_INDEX_AP1, QL_WIFI_BANDWIDTH_HT20); // 设置带宽。
ql_wifi_ap_channel_set(QL_WIFI_AP_INDEX_AP1, 149); // 设置 5G 信道。
auth_ap1.auth = QL_WIFI_AUTH_WPA_PSK; // 设置认证模式。
auth_ap1.wpa_psk.pairwise = QL_WIFI_AUTH_WPA_PAIRWISE_AUTO; // 设置加密方式。
auth_ap1.wpa_psk.group_rekey = 3600; /* one hour */ // 设置组密钥。
strcpy(auth_ap1.wpa_psk.passwd, "12345678"); // 设置密码。
ql_wifi_ap_auth_set(QL_WIFI_AP_INDEX_AP1, &auth_ap1);

ql_wifi_enable(); // 打开 Wi-Fi。
return 0;
```

### 3.4 设置 AP-STA 模式

参考例程 `example/wifi/ap_sta_mode.c`，代码片段如下：

```c
ql_wifi_ap_auth_s  ap_auth;
ql_wifi_sta_auth_s sta_auth;

/* setting AP0 */
ql_wifi_work_mode_set(QL_WIFI_WORK_MODE_AP0_STA); // 设置工作模式 AP-STA。
ql_wifi_ap_ssid_set(QL_WIFI_AP_INDEX_AP0, "Quectel-WORK_MODE_AP0"); // 设置 SSID。
ql_wifi_ap_mode_set(QL_WIFI_AP_INDEX_AP0, QL_WIFI_MODE_80211BGN); // 设置 g/b/n 模式。
ql_wifi_ap_bandwidth_set(QL_WIFI_AP_INDEX_AP0, QL_WIFI_BANDWIDTH_HT20); // 设置带宽。
ql_wifi_ap_channel_set(QL_WIFI_AP_INDEX_AP0, 11); // 设置 2.4G 信道。

ap_auth.auth = QL_WIFI_AUTH_WPA_PSK; // 设置认证模式。
ap_auth.wpa_psk.pairwise = QL_WIFI_AUTH_WPA_PAIRWISE_AUTO; // 设置加密方式。
ap_auth.wpa_psk.group_rekey = 3600; /* one hour */ // 设置组密钥。
strcpy(ap_auth.wpa_psk.passwd, "12345678"); // 设置密码。
ql_wifi_ap_auth_set(QL_WIFI_AP_INDEX_AP0, &ap_auth);

/* setting STA */
ql_wifi_sta_ssid_set("Quectel-test"); // 设置 SSID。

sta_auth.auth = QL_WIFI_AUTH_WPA_PSK; // 设置认证模式。
sta_auth.wpa_psk.pairwise = QL_WIFI_AUTH_WPA_PAIRWISE_AUTO; // 设置加密方式。
strcpy(sta_auth.wpa_psk.passwd, "12345678"); // 设置密码。
ql_wifi_sta_auth_set(&sta_auth);

ql_wifi_enable(); // 打开 Wi-Fi。
return 0;
```

---

## 4 附录 A 术语缩写

### 参考文档（表 3）

| 序号 | 文档名称 | 备注 |
|---|---|---|
| [1] | Quectel_EC2x&EG9x&EG25-G 系列_QuecOpen_快速开发指导 | EC2x&EG9x&EG25-G 系列 QuecOpen 方案的快速开发指导 |
| [2] | Quectel_FC20 系列_硬件设计手册 | FC20 系列硬件设计手册 |

### 术语缩写（表 4）

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| AP | Access Point | 热点模式 |
| API | Application Programming Interface | 应用程序接口 |
| SDK | Software Development Kit | 软件开发工具包 |
| SSID | Service Set Identifier | 无线名称 |
| STA | Station | 站点 |
| WEP | Wired Equivalent Privacy | 有线等效保密 |
| Wi-Fi | Wireless Fidelity | 无线局域网 |
| WPA | Wi-Fi Protected Access | Wi-Fi 网络安全接入 |
