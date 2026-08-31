# AG35-CET&AG35-EUT QuecOpen(SDK)设备管理指导

## 文档基本信息
- 标题：AG35-CET&AG35-EUT QuecOpen(SDK)设备管理指导
- 适用模块：AG35-CET、AG35-EUT（LTE Standard 模块系列）
- 版本：1.0.1
- 日期：2024-03-13
- 状态：临时文件（Preliminary）
- 发布单位：上海移远通信技术股份有限公司（Quectel）
- 总页数：31 页（正文）
- 文档历史：
  - `-`（2023-11-21，Jensen ZHANG）：文档创建
  - `1.0.0`（2023-11-21，Jensen ZHANG）：临时版本
  - `1.0.1`（2024-03-13，Jensen ZHANG）：临时版本；新增适用模块 AG35-EUT；第 3.3 章新增函数 `ql_dm_set_radio_on()`、`ql_dm_set_radio_off()`、`ql_dm_set_service_error_cb()`、`ql_dm_set_qoos_config()`、`ql_dm_get_qoos_config()`、`ql_dm_set_qoos_enable()`、`ql_dm_get_qoos_enable()`；新增部分示例（第 4.12 章~第 4.18 章）

## 逐章节内容摘要

### 第 1 章 引言
AG35-CET 和 AG35-EUT 模块支持 QuecOpen® 方案，这是基于 Linux 的嵌入式开发平台，用于简化 IoV（车联网）应用的软件设计和开发过程。本文档适用于 SDK 构建环境的 QuecOpen® 方案，主要介绍 AG35-CET/AG35-EUT 模块 SDK 中提供的"设备管理"API 功能及使用方法。设备管理是终端管理业务，用于模块状态信息的查询及参数设定。

### 第 2 章 飞行模式功能介绍
- 模块进入飞行模式后，会关闭射频电路供电，此时模块不能进行任何无线信号的收发，甚至无法拨打紧急电话。
- 退出飞行模式后，会恢复射频电路供电，模块可重新进行注网、接打电话、收发短信、拨号等业务。
- **关键约束**：无论飞行模式处于开启还是关闭状态，模块断电重启后都会退出飞行模式，并恢复默认的飞行模式配置（即飞行模式设置不持久化，重启后失效）。
- 飞行模式枚举定义：
```c
typedef enum QL_DW_AIR_PLANE_MODE_TYPE_ENUM
{
    QL_DM_AIR_PLANE_MODE_UNKNOWN  = 0,
    QL_DM_AIR_PLANE_MODE_ON       = 1,
    QL_DM_AIR_PLANE_MODE_OFF      = 2,
    QL_DM_AIR_PLANE_MODE_NA       = 3
}QL_DM_AIR_PLANE_MODE_TYPE_E;
```
  - `QL_DM_AIR_PLANE_MODE_UNKNOWN`：飞行模式状态未知
  - `QL_DM_AIR_PLANE_MODE_ON`：飞行模式开启
  - `QL_DM_AIR_PLANE_MODE_OFF`：飞行模式关闭（默认状态）
  - `QL_DM_AIR_PLANE_MODE_NA`：飞行模式不可用

### 第 3 章 设备管理 API

#### 3.1 头文件
设备管理 API 头文件为 `ql_dm.h`，位于 SDK 的 `ql-sysroots/usr/include/ql-sdk/` 目录下。文档中若无特殊说明，提到的头文件均在该目录下。

#### 3.2 函数概览
设备管理共提供 20 个 API（见下方"关键 API/命令清单"完整列表），涵盖：服务初始化/去初始化、软件版本查询、modem 固件版本查询、modem 状态查询、晶振温度查询、序列号（IMEI/MEID）查询、飞行模式查询/设置/回调、CPU 占用率查询、内存使用率查询、modem 状态改变回调、射频开启/关闭、服务异常事件回调、QoS 使能/配置查询与设置。

> **备注（原文重要提示）**：若无特别说明，本文档所述函数均不支持并发调用，并且不能在相关回调函数中调用以上函数。

#### 3.3 函数详解（第 3.3.1 ~ 3.3.17，对应页 10~19）

- **3.3.1 `ql_dm_init`**：初始化设备管理服务，无参数，返回 `QL_ERR_OK` 表示成功。**程序启动后必须先调用本函数**才能使用大部分其他设备管理 API。
- **3.3.2 `ql_dm_deinit`**：去初始化设备管理服务，无参数。
- **3.3.3 `ql_dm_get_software_version`**：获取 AP 侧软件版本号；**不要求**设备管理服务必须先初始化。参数 `soft_ver`[Out]、`soft_ver_len`[In]。
- **3.3.4 `ql_dm_get_device_firmware_rev_id`**：获取 modem 侧固件版本号；**要求**设备管理服务初始化成功后才可使用。参数 `firmware_rev_id`[Out]、`firmware_rev_id_len`[In]（单位字节）。
- **3.3.5 `ql_dm_get_modem_state`**：获取 modem 状态；不要求先初始化。参数 `modem_state`[Out]，类型见 3.3.5.1。
  - **3.3.5.1 `QL_DM_MODEM_STATE_TYPE_E`**：
    ```c
    typedef enum QL_DM_MODEM_STATE_TYPE_ENUM
    {
        QL_DM_MODEM_STATE_UNKNOWN = 2,
        QL_DM_MODEM_STATE_ONLINE  = 1,
        QL_DM_MODEM_STATE_OFFLINE = 0
    }QL_DM_MODEM_STATE_TYPE_E;
    ```
    - `UNKNOWN`=2：modem 状态未知；`ONLINE`=1：modem 在线；`OFFLINE`=0：modem 掉线
- **3.3.6 `ql_dm_get_temperature`**：获取模块晶振温度；不要求先初始化。参数 `temperature`[Out] (float)。**备注**：模块中还有其他温度值（芯片温度、功率放大器温度等），可直接访问 `/sys/class/thermal/thermal_zoneX/type` 查看温度类型，同目录下 `temp` 文件为对应温度值。
- **3.3.7 `ql_dm_get_device_serial_numbers`**：获取模块序列号；要求设备管理服务初始化成功后才可使用。参数 `p_info`[Out]，结构见 3.3.7.1。
  - **3.3.7.1 `ql_dm_device_serial_numbers_info_t`**：
    ```c
    typedef struct
    {
        uint8_t imei_valid;
        char imei[QL_DM_IMEI_MAX_LEN + 1];
        uint8_t meid_valid;
        char meid[QL_DM_MEID_MAX_LEN + 1];
    }ql_dm_device_serial_numbers_info_t;
    ```
    - `imei_valid`：0=无效，非0=有效；`imei`：设备 IMEI 号，适用于 GSM/WCDMA 网络制式
    - `meid_valid`：0=无效，非0=有效；`meid`：设备 MEID 号，适用于 CDMA 网络制式
- **3.3.8 `ql_dm_get_air_plane_mode`**：获取模块飞行模式；要求设备管理服务初始化成功后才可使用。参数 `p_info`[Out]，类型见第 2 章。
- **3.3.9 `ql_dm_set_air_plane_mode`**：设置模块飞行模式；飞行模式默认关闭状态，**配置断电不保存**；要求设备管理服务初始化成功后才可使用。参数 `air_plane_mode`[In]。
- **3.3.10 `ql_dm_set_air_plane_mode_ind_cb`**：设置飞行模式事件回调函数。该函数所做设置在**程序退出后失效**，需要时须重新调用本函数设置。设置成功后，若飞行模式状态发生变化，会调用该回调函数。要求设备管理服务初始化成功后才可使用。参数 `cb_func`[Out]，类型见 3.3.10.1。
  - **3.3.10.1 `ql_dm_air_plane_mode_ind_cb`**：
    ```c
    typedef void (*ql_dm_air_plane_mode_ind_cb)(QL_DM_AIR_PLANE_MODE_TYPE_E air_plane_mode)
    ```
    参数 `air_plane_mode`[In]：模块飞行模式；无返回值。
- **3.3.11 `ql_dm_get_cpu_occupancy`**：获取 AP 侧 CPU 占有率；不要求先初始化。参数 `cpu_occupancy`[Out] (float)。
- **3.3.12 `ql_dm_get_mem_usage`**：获取 AP 侧内存使用率；不要求先初始化。参数 `mem_use`[Out] (float)。
- **3.3.13 `ql_ms_dm_set_modem_state_change_ind_cb`**：设置 modem 状态改变回调函数。**设置断电不保存**；设置成功后，若 modem 状态改变，会调用该回调。要求设备管理服务初始化成功后才可使用。参数 `cb_func`[Out]，类型见 3.3.13.1。
  - **3.3.13.1 `ql_dm_modem_state_ind_cb`**：
    ```c
    typedef void (*ql_dm_modem_state_ind_cb)(int modem_state)
    ```
    参数 `modem_state`[In]：modem 状态，详见 3.3.5.1；无返回值。
- **3.3.14 `ql_dm_set_radio_on`**：开启射频；其作用与 `AT+CFUN=1` 相同；要求设备管理服务初始化成功后才可使用。无参数。
- **3.3.15 `ql_dm_set_radio_off`**：关闭射频；其作用与 `AT+CFUN=0` 相同；要求设备管理服务初始化成功后才可使用。无参数。
- **3.3.16 `ql_dm_set_service_error_cb`**：设置服务异常事件回调函数；要求设备管理服务初始化成功后才可使用。参数 `cb`[In]，类型见 3.3.16.1。
  - **3.3.16.1 `ql_dm_service_error_cb_f`**：
    ```c
    typedef void (*ql_dm_service_error_cb_f)(int error)
    ```
    参数 `error`[In]：异常码，取值描述：`-1099` = Service Abort；无返回值。
- **3.3.17 `ql_dm_get_qoos_enable`**：获取 QoS 使能状态。函数原型：`int ql_dm_get_qoos_enable(char *enable);`。参数 `enable`[out]：QoS 使能状态。返回值：`QL_ERR_OK` 成功，其他值失败（错误码详见 `ql_type.h`）。**备注（原文重要提示）**：模块中 QoS 机制为开启状态，不可设置。该函数为兼容 QoS 机制而实现，无实际功能。
- **3.3.18 `ql_dm_set_qoos_enable`**：设置 QoS 使能状态。函数原型：`int ql_dm_set_qoos_enable(char enable);`。参数 `enable`[in]：QoS 使能状态。返回值同上。**备注（原文重要提示，与 3.3.17 相同）**：模块中 QoS 机制为开启状态，不可设置，该函数为兼容 QoS 机制而实现，无实际功能。
- **3.3.19 `ql_dm_set_qoos_config`**：设置 QoS 配置参数；要求设备管理服务初始化成功后才可使用。函数原型：`int ql_dm_set_qoos_config(int p1, int p2, int p3)`。参数：
  - `p1`[In]：第一轮搜网间隔时间，范围 1~225，单位：秒
  - `p2`[In]：第二轮搜网间隔时间，范围 1~225，单位：秒
  - `p3`[In]：第三轮搜网间隔时间，范围 1~225，单位：秒
  返回值：`QL_ERR_OK` 成功，其他值失败。
- **3.3.20 `ql_dm_get_qoos_config`**：获取 QoS 配置参数；要求设备管理服务初始化成功后才可使用。函数原型：`int ql_dm_get_qoos_config(int *p1, int *p2, int *p3)`。参数：
  - `p1`[Out]：第一轮搜网间隔时间，范围 1~225，单位：秒
  - `p2`[Out]：第二轮搜网间隔时间，范围 1~225，单位：秒
  - `p3`[Out]：第三轮搜网间隔时间，范围 1~225，单位：秒
  返回值：`QL_ERR_OK` 成功，其他值失败。

### 第 4 章 示例
用户可自行通过 `/ql-sdk/sample/test_sdk_api/m_dm.c` 查看设备管理 API 的完整示例，本章所述代码示例为部分示例。**程序启动后，必须调用 `ql_dm_init()` 初始化设备管理服务**。本章共 18 个示例（4.1~4.18），逐一对应第 3.3 节的 18 个主要 API（不含两个内部回调类型定义），详见下方"示例代码说明"一节的逐段解读。

### 第 5 章 附录 参考文档及术语缩写
- **表 2：参考文档**
  - [1] `Quectel_AG35-CER_QuecOpen_快速开发指导`
- **表 3：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| AP | Access Point/Application Processor | 接入点/应用处理器 |
| API | Application Programming Interface | 应用程序接口 |
| CPU | Central Processing Unit | 中央处理器 |
| CMDA | Code-Division Multiple Access | 码分多址（利用码序列相关性实现的多址通信）|
| DM | Device Management | 设备管理 |
| GSM | Global System for Mobile Communications | 全球移动通信系统 |
| IMEI | International Mobile Equipment Identifier | 国际移动设备识别码 |
| IoV | Internet of Vehicles | 车联网 |
| MEID | Mobile Equipment Identifier | 移动设备识别码 |
| NVRAM | Non-Volatile Random Access Memory | 非易失性随机访问存储器 |
| SDK | Software Development Kit | 软件开发包 |
| WCDMA | Wideband Code Division Multiple Access | 宽带码分多址 |

> 注：表 3 中 `CMDA` 为原文文档拼写（原文术语表确实写作 CMDA 而非 CDMA），此处按原文如实记录，未做更正。

## 关键 API / 命令清单

下表汇总文档第 3.3 节全部 20 个函数（含 18 个主 API + 2 个回调类型定义），按文档出现顺序排列：

| 序号 | 函数/类型 | 作用 | 关键参数 | 初始化要求 | 备注 |
|---|---|---|---|---|---|
| 3.3.1 | `int ql_dm_init(void)` | 初始化设备管理服务 | 无 | 无（这是初始化函数本身） | 程序启动后必须先调用 |
| 3.3.2 | `int ql_dm_deinit(void)` | 去初始化设备管理服务 | 无 | 无 | - |
| 3.3.3 | `int ql_dm_get_software_version(char *soft_ver, int soft_ver_len)` | 获取 AP 侧软件版本号 | `soft_ver`[Out]，`soft_ver_len`[In] | 不要求 | - |
| 3.3.4 | `int ql_dm_get_device_firmware_rev_id(char *firmware_rev_id, int firmware_rev_id_len)` | 获取 modem 侧固件版本号 | `firmware_rev_id`[Out]，`firmware_rev_id_len`[In]（字节） | 要求已初始化 | - |
| 3.3.5 | `int ql_dm_get_modem_state(QL_DM_MODEM_STATE_TYPE_E *modem_state)` | 获取 modem 状态 | `modem_state`[Out] | 不要求 | 枚举：UNKNOWN=2/ONLINE=1/OFFLINE=0 |
| 3.3.6 | `int ql_dm_get_temperature(float *temperature)` | 获取模块晶振温度 | `temperature`[Out] | 不要求 | 其他温度（芯片/功放）见 `/sys/class/thermal/thermal_zoneX/` |
| 3.3.7 | `int ql_dm_get_device_serial_numbers(ql_dm_device_serial_numbers_info_t *p_info)` | 获取模块序列号（IMEI/MEID） | `p_info`[Out] | 要求已初始化 | 结构体含 `imei_valid`/`imei`/`meid_valid`/`meid` |
| 3.3.8 | `int ql_dm_get_air_plane_mode(QL_DM_AIR_PLANE_MODE_TYPE_E *p_info)` | 获取模块飞行模式 | `p_info`[Out] | 要求已初始化 | - |
| 3.3.9 | `int ql_dm_set_air_plane_mode(QL_DM_AIR_PLANE_MODE_TYPE_E air_plane_mode)` | 设置模块飞行模式 | `air_plane_mode`[In] | 要求已初始化 | 配置断电不保存 |
| 3.3.10 | `int ql_dm_set_air_plane_mode_ind_cb(ql_dm_air_plane_mode_ind_cb cb_func)` | 设置飞行模式事件回调 | `cb_func`[Out] | 要求已初始化 | 程序退出后失效，需重新设置 |
| 3.3.10.1 | `typedef void (*ql_dm_air_plane_mode_ind_cb)(QL_DM_AIR_PLANE_MODE_TYPE_E air_plane_mode)` | 飞行模式事件回调类型 | `air_plane_mode`[In] | - | 无返回值 |
| 3.3.11 | `int ql_dm_get_cpu_occupancy(float *cpu_occupancy)` | 获取 AP 侧 CPU 占有率 | `cpu_occupancy`[Out] | 不要求 | - |
| 3.3.12 | `int ql_dm_get_mem_usage(float *mem_use)` | 获取 AP 侧内存使用率 | `mem_use`[Out] | 不要求 | - |
| 3.3.13 | `int ql_ms_dm_set_modem_state_change_ind_cb(ql_dm_modem_state_ind_cb cb_func)` | 设置 modem 状态改变回调 | `cb_func`[Out] | 要求已初始化 | 设置断电不保存 |
| 3.3.13.1 | `typedef void (*ql_dm_modem_state_ind_cb)(int modem_state)` | modem 状态改变回调类型 | `modem_state`[In] | - | 无返回值 |
| 3.3.14 | `int ql_dm_set_radio_on(void)` | 开启射频 | 无 | 要求已初始化 | 等价于 `AT+CFUN=1` |
| 3.3.15 | `int ql_dm_set_radio_off(void)` | 关闭射频 | 无 | 要求已初始化 | 等价于 `AT+CFUN=0` |
| 3.3.16 | `int ql_dm_set_service_error_cb(ql_dm_service_error_cb_f cb)` | 设置服务异常事件回调 | `cb`[In] | 要求已初始化 | - |
| 3.3.16.1 | `typedef void (*ql_dm_service_error_cb_f)(int error)` | 服务异常事件回调类型 | `error`[In] | - | `-1099`=Service Abort |
| 3.3.17 | `int ql_dm_get_qoos_enable(char *enable)` | 获取 QoS 使能状态 | `enable`[out] | 未明确说明 | QoS 始终开启，函数仅兼容用，无实际功能 |
| 3.3.18 | `int ql_dm_set_qoos_enable(char enable)` | 设置 QoS 使能状态 | `enable`[in] | 未明确说明 | QoS 始终开启不可设置，无实际功能 |
| 3.3.19 | `int ql_dm_set_qoos_config(int p1, int p2, int p3)` | 设置 QoS 配置参数（三轮搜网间隔） | `p1`/`p2`/`p3`[In]，均 1~225 秒 | 要求已初始化 | - |
| 3.3.20 | `int ql_dm_get_qoos_config(int *p1, int *p2, int *p3)` | 获取 QoS 配置参数（三轮搜网间隔） | `p1`/`p2`/`p3`[Out]，均 1~225 秒 | 要求已初始化 | - |

**相关 AT 命令**（文档中提及的等价关系）：
- `AT+CFUN=1` ⟺ `ql_dm_set_radio_on()`（开启射频）
- `AT+CFUN=0` ⟺ `ql_dm_set_radio_off()`（关闭射频）

**返回值约定**：除标注"无返回值"的回调类型外，所有 API 统一返回 `QL_ERR_OK` 表示成功，其他值表示失败，具体错误码定义见 `ql_type.h`。

**全局重要约束（原文 3.2 节备注）**：若无特别说明，本文档所述函数均不支持并发调用，且不能在相关回调函数中调用以上函数。

## 示例代码说明

第 4 章共 18 个示例，均节选自 `/ql-sdk/sample/test_sdk_api/m_dm.c`，逐段解读如下：

- **4.1 `item_ql_dm_get_software_version`**：声明 128 字节缓冲区 `soft_ver`，调用 `ql_dm_get_software_version(soft_ver, sizeof(soft_ver))` 后直接 `printf` 打印返回值和版本号字符串。最简单的"获取并打印"模式，是后续大部分只读 API 示例的通用范式。
- **4.2 `item_ql_dm_get_device_firmware_rev_id`**：缓冲区大小用 `QL_DM_FIRMWARE_REV_MAX_LEN + 1`（而非像 4.1 那样写死 128），体现 SDK 提供了专门的最大长度宏，调用方应优先使用宏而非硬编码长度，避免缓冲区不足。
- **4.3 `item_ql_dm_get_modem_state`**：调用 `ql_dm_get_modem_state(&modem_state)` 后，用 `if/else if/else` 三分支分别处理 `ONLINE`/`OFFLINE`/其他（视为 UNKNOWN）三种状态并打印对应文本，是枚举类返回值的典型"翻译成可读字符串"写法。
- **4.4 `item_ql_dm_get_temperature`**：获取 `temperature`（float）后，打印时执行了 `temperature/1000.0` 再用 `%.2f` 格式化——说明 API 实际返回的温度值单位是"毫摄氏度"（千分之一摄氏度），需要调用方自行换算成摄氏度，这是文档正文未直接写明、但示例代码暴露出的关键细节。
- **4.5 `item_ql_dm_get_device_serial_numbers`**：先 `memset` 清零 `ql_dm_device_serial_numbers_info_t` 结构体，再调用获取序列号；随后分别用 `imei_valid`/`imei2_valid`/`meid_valid` 三个有效位判断后才打印对应号码。**关键发现**：示例代码中出现了 `t_info.imei2_valid` / `t_info.imei2` 字段，但这两个字段并未出现在文档第 3.3.7.1 节给出的结构体定义（该处只列了 `imei_valid`/`imei`/`meid_valid`/`meid` 四个字段）。这说明文档正文的结构体定义描述**不完整**——实际结构体至少还应包含 `imei2_valid`/`imei2`（用于双 IMEI/双卡场景的第二个 IMEI 号），但文档第 3.3.7.1 节漏写了，是文档本身的一处缺陷。
- **4.6 `item_ql_dm_get_air_plane_mode`**：调用 `ql_dm_get_air_plane_mode(&air_plane_mode)` 后，用一个示例代码中**未在文档中定义**的辅助函数 `internal_dm_get_air_plane_mode(air_plane_mode, mode_info, sizeof(mode_info))` 把枚举值转成可读字符串 `mode_info`；该辅助函数返回 0 时表示"无法识别该模式"（打印 unrecognized），非 0（成功转换）才打印 `mode_info` 文本。这个 `internal_dm_get_air_plane_mode` 是示例工程内部的私有工具函数，不属于 SDK 公开 API，文档也未对其单独说明。
- **4.7 `item_ql_dm_set_air_plane_mode`**：交互式示例：先 `printf` 提示用户输入 `1: ON, 2: OFF`，用 `t_get_int(&mode)` 读取用户输入的整数；校验 `ret != 0`（非法输入）和 `air_plane_mode` 是否恰好等于 `QL_DM_AIR_PLANE_MODE_ON`/`OFF` 两个合法值之一，否则直接 `return`；通过校验才调用 `ql_dm_set_air_plane_mode()`。体现了对用户输入做"双重校验"（输入格式 + 取值范围）的防御性写法。
- **4.8 `item_ql_dm_set_air_plane_mode_ind_cb`**：交互式示例，输入 `0: unreg, other: reg` 决定是注册还是取消注册回调：`reg_flag` 非 0 时传入实际回调函数指针 `dm_air_plane_mode_event_ind_cb`，为 0 时传 `NULL` 取消回调。说明该类"设置回调"API 同时承担"注册"和"反注册"两种语义，传 `NULL` 即为反注册，是 SDK 回调类 API 的通用约定。
- **4.9 `item_ql_dm_get_cpu_occupancy`** / **4.10 `item_ql_dm_get_mem_usage`**：均为最简单的"调用+打印"模式，打印格式用 `%.2f%%`，说明返回的 `cpu_occupancy`/`mem_use` 已经是百分比数值（如 23.45 表示 23.45%），不需要再额外乘 100。
- **4.11 `item_ql_dm_set_modem_state_change_ind_cb`**：结构与 4.8 完全一致（注册/反注册模式），回调函数指针为 `dm_modem_state_change_ind_cb`，进一步印证 4.8 中归纳的"传 NULL 即反注册"约定具有一致性，是整个 SDK 回调设置类 API 的统一范式。
- **4.12 `item_ql_dm_set_radio_on`** / **4.13 `item_ql_dm_set_radio_off`**：均为最简调用，无参数，直接打印返回值，验证开启/关闭射频的调用方式。
- **4.14 `item_ql_dm_set_service_error_cb`**：调用 `ql_dm_set_service_error_cb(dm_service_error_cb)` 注册服务异常回调，并按返回值是否等于 `QL_ERR_OK` 分别打印 `failed`/`successful`，是错误码判断的标准写法（与 4.1~4.13 多数示例只打印 `ret` 数值不同，这里做了显式分支判断）。
- **4.15 `item_ql_dm_get_qoos_enable`**：获取 `enable` 后，若 `enable == 1` 额外打印 "qoos customized is enable"。**注意**：示例最后一行打印用的是 `"ql_dm_set_qoos_config ret = %d\n"`（函数名写成了 `set_qoos_config` 而不是 `get_qoos_enable`），这是示例源码中的一处**命名/复制粘贴错误**（推测是从 4.17/4.18 的打印语句复制过来时未同步修改字符串内容），不影响功能但属于文档/示例的瑕疵，如实记录。
- **4.16 `item_ql_dm_set_qoos_enable`**：交互式示例，提示输入 `0/1` 后调用 `t_get_int(&enable)` 读取，再调用 `ql_dm_set_qoos_enable(enable)`。同样存在打印字符串写成 `"ql_dm_set_qoos_config ret = %d\n"` 而非 `set_qoos_enable` 的同类瑕疵。**功能性提示**：结合 3.3.17/3.3.18 的备注（QoS 机制始终开启、不可设置、函数仅为兼容而实现），4.15/4.16 这两个示例实际运行时设置/查询大概率不会改变真实行为，只是 API 形式上可调用。
- **4.17 `item_ql_dm_set_qoos_config`**：连续三次调用 `t_get_int` 分别读取 `phase1`/`phase2`/`phase3`（对应文档中的 `p1`/`p2`/`p3`，即三轮搜网间隔秒数），再调用 `ql_dm_set_qoos_config(phase1, phase2, phase3)`。示例变量命名 `phase1/2/3` 与函数签名形参名 `p1/p2/p3` 不一致，但语义对应一致（均为搜网间隔），是文档示例与 API 签名之间命名风格不统一的体现。
- **4.18 `item_ql_dm_get_qoos_config`**：调用 `ql_dm_get_qoos_config(&phase1, &phase2, &phase3)` 后打印三个值，是 4.17 的对称读取版本，验证设置/获取的配对关系。

**示例代码整体观察**：
1. 多数"获取类"示例（4.1~4.6、4.9~4.10、4.15）遵循"声明变量→调用 API→格式化打印"三步模式；"设置类"和"注册回调类"示例（4.7、4.8、4.11、4.16、4.17）则普遍先用 `t_get_int()` 做交互式输入并校验合法性，再调用对应 API。
2. 完整示例需查看 `/ql-sdk/sample/test_sdk_api/m_dm.c`，本文档第 4 章仅为部分示例（文档原文 4 章开头已明确声明这一点）。
3. 文档与示例代码之间存在两类不一致，均已在上面逐条标出：① 第 3.3.7.1 节结构体定义缺少示例代码中实际使用的 `imei2_valid`/`imei2` 字段；② 4.15/4.16 示例打印语句的函数名字符串存在复制粘贴遗留错误。

<!-- GENERATION_COMPLETE: 2026-06-24 -->
