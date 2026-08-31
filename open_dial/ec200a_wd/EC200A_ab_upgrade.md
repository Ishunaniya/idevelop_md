# EC200A-CN(TA) QuecOpen A/B 系统升级指导

> **文档版本**：V1.0.0 Preliminary  
> **发布日期**：2022-12-06  
> **适用平台**：EC200A-CN(TA)（QuecOpen 模式）  
> **关键字**：A/B 系统、OTA 升级、FOTA、ql-lib-fota、ql-lib-absys、双系统、在线升级

---

## 目录

- [1. 引言](#1-引言)
- [2. A/B 系统介绍](#2-ab-系统介绍)
  - [2.1 A/B 系统概念](#21-ab-系统概念)
  - [2.2 Flash 分区布局](#22-flash-分区布局)
  - [2.3 升级包](#23-升级包)
  - [2.4 制作升级包](#24-制作升级包)
- [3. API 参考](#3-api-参考)
  - [3.1 升级概览](#31-升级概览)
  - [3.2 升级状态机](#32-升级状态机)
  - [3.3 API 调用推荐流程](#33-api-调用推荐流程)
  - [3.4 ql-lib-fota 库相关 API](#34-ql-lib-fota-库相关-api)
    - [3.4.1 头文件](#341-头文件)
    - [3.4.2 函数概览](#342-函数概览)
    - [3.4.3 函数详解](#343-函数详解)
  - [3.5 ql-lib-absys 库相关 API](#35-ql-lib-absys-库相关-api)
    - [3.5.1 头文件](#351-头文件)
    - [3.5.2 函数概览](#352-函数概览)
    - [3.5.3 函数详解](#353-函数详解)
  - [3.6 示例](#36-示例)
    - [3.6.1 制作升级包](#361-制作升级包)
    - [3.6.2 编译测试程序](#362-编译测试程序)
    - [3.6.3 下载升级包](#363-下载升级包)
    - [3.6.4 执行测试程序](#364-执行测试程序)
    - [3.6.5 AB 系统非激活分区损坏查询及恢复处理](#365-ab-系统非激活分区损坏查询及恢复处理)
  - [3.7 资源消耗评估](#37-资源消耗评估)
    - [3.7.1 内存消耗评估](#371-内存消耗评估)
    - [3.7.2 Flash 空间消耗评估](#372-flash-空间消耗评估)
    - [3.7.3 CPU 资源消耗评估](#373-cpu-资源消耗评估)
- [附录 参考文档及术语缩写](#附录-参考文档及术语缩写)

---

## 1. 引言

移远通信 EC200A-CN(TA) 模块支持 QuecOpen® 方案。QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。

本文档主要介绍 EC200A-CN(TA) 模块的 **A/B 系统升级**功能。A/B 系统升级（也称双系统 OTA 升级）允许在设备正常运行期间对非激活系统分区进行固件更新，更新完成后切换到新系统，实现无中断在线升级。

---

## 2. A/B 系统介绍

### 2.1 A/B 系统概念

A/B 系统升级方案的核心思想：设备 Flash 中保存两套完整的系统固件（称为 **A 系统** 和 **B 系统**），同一时刻只有一套系统处于**激活（Active）**状态并运行，另一套为**非激活（Non-Active）**系统。

升级过程：
1. 将新固件写入**非激活**系统分区（设备正常运行不受影响）；
2. 写入完成后，切换激活系统（模块自动重启）；
3. 在新系统上运行后，将当前（新）系统的内容同步（备份）到另一分区；
4. 同步完成，A/B 系统内容一致，升级完成。

优点：
- 升级过程中设备持续运行（写非激活分区）；
- 升级失败（分区损坏）时自动切回原系统，具备回滚保护；
- 升级信息存储在 Flash 中，断电不丢失。

---

### 2.2 Flash 分区布局

EC200A-CN(TA) 的 Flash 分区采用双套设计，每个系统组件都有 A、B 两份：

| MTD 设备 | 分区名 | 说明 |
|---------|-------|------|
| mtd0 | dtlm-a | A 系统 DTLM |
| mtd1 | (预留) | — |
| mtd2 | cpimage-a | A 系统 CP 镜像（固件 1） |
| mtd3 | (预留) | — |
| mtd4 | u-boot-a | A 系统 Bootloader |
| mtd5 | kernel-a | A 系统内核 |
| mtd6 | rootfs-a | A 系统根文件系统 |
| mtd7 | oem_data-a | A 系统 OEM 数据 |
| mtd13 | dtlm-b | B 系统 DTLM |
| mtd14 | cpimage-b | B 系统 CP 镜像 |
| mtd15 | u-boot-b | B 系统 Bootloader |
| mtd16 | kernel-b | B 系统内核 |
| mtd17 | rootfs-b | B 系统根文件系统 |
| mtd18 | oem_data-b | B 系统 OEM 数据 |
| mtd19 | asr_flag | ASR 标志分区（存储升级/损坏状态，掉电不丢失） |
| mtd20 | device_info | 设备信息 |
| mtd21 | cust_info | 客户信息 |
| mtd22 | rootfs_data | 可读写数据分区（用户数据，升级不影响） |
| mtd23 | QUEC_BBM | Quectel BBM 分区 |

查看当前分区布局：

```bash
cat /proc/mtd
```

---

### 2.3 升级包

A/B 系统升级包文件名为 **`quectel_AB_OTA.img`**，为完整的全量升级包，内含所有需要更新的分区镜像。

升级包存放位置（可访问分区）：
- `/data/quectel_AB_OTA.img`（rootfs_data 分区）
- `/mnt/sdcard/quectel_AB_OTA.img`（外挂 eMMC）

升级包内容（约 22 MB，不含用户镜像文件）：

| 镜像文件 | 说明 |
|---------|------|
| CPImage（3 个固件） | CP 固件镜像 |
| zImage | Linux 内核镜像 |
| root | 根文件系统 |
| oem_data | OEM 数据 |
| u-boot | Bootloader |
| tos | 安全环境镜像 |

> **说明**：用户可将自己的程序及镜像导入到 `oem_data` 分区中，则占用 Flash 总大小约为 22 MB + oem_data 的大小。

> **重要**：无论升级成功或失败，模块均不会删除升级包，升级包下载/删除由用户应用程序管理。

---

### 2.4 制作升级包

使用 SDK 中的 `ql-ota-tools` 工具制作升级包：

**工具**：`ql-ota-tools/ab_update_packager`

**配置文件**：`upgrade_config.json`（描述需要打包的镜像列表）

**命令**：

```bash
ab_update_packager -c upgrade_config.json -o quectel_AB_OTA.img
```

生成的升级包名称为 `quectel_AB_OTA.img`。

---

## 3. API 参考

### 3.1 升级概览

A/B 系统升级通过两个库完成：

- **`ql-lib-fota`**：负责将新固件写入非激活系统分区（升级写入操作）；
- **`ql-lib-absys`**：负责 A/B 系统状态查询、系统切换、分区同步等管理操作。

两个库配合使用，完成完整的 A/B 升级流程。

---

### 3.2 升级状态机

升级过程按照以下步骤执行：

| 步骤 | 操作 | 升级状态变化 |
|------|------|------------|
| (1) 触发升级 | 调用 `ql_abfota_start_update()` 升级非激活系统 | → `UPDATE`（正在升级非激活系统） |
| (2) 查询状态 | 调用 `ql_abfota_get_update_status()` 获取升级信息 | 等待 `ota_state == WRITEDONE` 或不再是 `UPDATE` |
| (3) 轮询 | 若 `ota_state == UPDATE`，继续轮询状态 | 保持 `UPDATE` |
| (4) 执行系统切换 | `ota_state == WRITEDONE` 时，调用 `ql_absys_switch()` 切换系统 | 函数执行成功后自动重启模块，切换到升级后系统，升级状态自动切换为 `NEEDSYNC`（A、B 系统不一致，需同步） |
| (5) 执行分区同步 | 调用 `ql_absys_sync()` 同步分区 | → `BACKUP`（正在同步分区） |
| (6) 同步失败 | 若 A、B 系统同步失败 | → `FAILED`（当前非激活系统被破坏，需进行还原） |
| (7) 同步成功 | 若 A、B 系统同步成功 | → `SUCCEED`（升级完成且 A、B 系统已同步） |

**备注**：
1. 若非激活系统升级失败，非激活系统固件被损坏，升级状态切换为 `FAILED`。如需继续升级，可直接再次升级非激活系统；如无需升级，调用 `ql_absys_sync()` 进行还原即可。
2. 无论升级成功或失败，模块均不会删除升级包，升级包下载/删除由用户应用程序管理。
3. 升级状态切换到 `NEEDSYNC` 时，说明模块已经启动并在已升级系统上运行，此时可无需等待系统同步完成即可上报升级结果至客户应用程序，但分区同步必须要执行。

---

### 3.3 API 调用推荐流程

```
开始
  │
  ▼
调用 ql_abfota_start_update() 升级非激活系统
  │
  ▼
调用 ql_abfota_get_update_status() 获取升级信息
  │
  ├─ ota_state == WRITEDONE? ──否──→ ota_state == UPDATE? ──是──→ (继续轮询) ──┐
  │                                   │                                         │
  │                                   └──否──→ ota_state == FAILED? ──是──→ 升级失败
  │                                                              │
  │                                                           ──否──→ 其他处理
  │ 是
  ▼
调用 ql_absys_switch() 切换系统
  │
  ▼
调用者重启系统完成系统切换
  │
  ▼
ota_state == NEEDSYNC? ──否──→ 继续其他流程
  │ 是
  ▼
调用 ql_absys_sync() 同步系统
  │
  ▼
ota_state == SUCCEED? ──否──→ 还原完成（FAILED时调用sync还原）
  │ 是
  ▼
升级完成
  │
  ▼
结束
```

**图 11：ql-lib-fota 和 ql-lib-absys 库相关 API 调用推荐流程**

---

### 3.4 ql-lib-fota 库相关 API

#### 3.4.1 头文件

头文件为 `ql_fota_api.h` 和 `fota_info.h`，位于 SDK 包的 `ql-sysroots\usr\include` 目录下。若无特别说明，本文档所涉及头文件均在该目录下。

---

#### 3.4.2 函数概览

**表 2：ql-lib-fota 库函数概览**

| 函数 | 说明 |
|------|------|
| `ql_abfota_start_update()` | 触发 A/B 系统升级 |
| `ql_abfota_get_update_status()` | 获取模块当前升级信息 |

---

#### 3.4.3 函数详解

##### 3.4.3.1 ql_abfota_start_update

该函数用于触发 A/B 系统升级。

**函数原型**

```c
int ql_abfota_start_update(const char *package_path)
```

**参数**

- `package_path`：
  - [In] 指针，指向升级包所在路径。升级包的名称为 `quectel_AB_OTA.img`。
  - 例如：升级包存储在 `/mnt/sdcard/` 目录下，则该参数值为 `/mnt/sdcard/quectel_AB_OTA.img`。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败，传入参数无效，或者设置升级起始状态失败 |
| 其他值 | 升级失败，详见 [3.4.3.1.1 fota_exit_code_t](#34311-fota_exit_code_t) |

---

##### 3.4.3.1.1 fota_exit_code_t

异常退出码枚举定义如下：

```c
typedef enum
{
    E_NO_ERROR             = 0,
    E_FOTA_INIT_FAILED     = 1,
    E_UPDATE_PACKAGE_NOEXIST = 2,
    E_WRITE_SEG_FAILED     = 3,
} fota_exit_code_t
```

**成员**

| 成员 | 描述 |
|------|------|
| `E_NO_ERROR` | 升级成功 |
| `E_FOTA_INIT_FAILED` | 升级初始化失败 |
| `E_UPDATE_PACKAGE_NOEXIST` | 升级包不存在 |
| `E_WRITE_SEG_FAILED` | 下载段数据失败 |

---

##### 3.4.3.2 ql_abfota_get_update_status

该函数用于获取模块当前升级信息。例如：升级状态、升级进度，以及升级异常退出码。

**函数原型**

```c
int ql_abfota_get_update_status(update_info_t *update_info)
```

**参数**

- `update_info`：
  - [In/Out] 指向存储当前升级状态、升级进度、以及升级异常退出码的内存地址；详见 [3.4.3.2.1 update_info_t](#34321-update_info_t)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

> **备注**：当调用该函数获取的升级状态为 `FAILED` 且退出码不为 0 时，说明升级失败，此时若需继续升级，调用 `ql_abfota_start_update()` 即可恢复正常升级状态。

---

##### 3.4.3.2.1 update_info_t

升级信息结构体信息定义如下：

```c
typedef struct
{
    unsigned int    percentage;
    fota_state_t    ota_state;
    fota_exit_code_t exit_code;
} update_info_t
```

**参数**

| 类型 | 参数 | 描述 |
|------|------|------|
| `unsigned int` | `percentage` | 升级的进度百分比。范围：0~100。 |
| `fota_state_t` | `ota_state` | 升级状态；详见 [3.4.3.2.2 fota_state_t](#34322-fota_state_t)。 |
| `fota_exit_code_t` | `exit_code` | 升级异常退出码；详见 [3.4.3.1.1 fota_exit_code_t](#34311-fota_exit_code_t)。 |

---

##### 3.4.3.2.2 fota_state_t

升级状态枚举定义如下。该信息存储在 Flash 中，掉电不丢失。

```c
typedef enum
{
    SUCCEED        = 0,
    UPDATE,
    BACKUP,
    FAILED,
    WRITEDONE,
    NEEDSYNC,
    UNKNOWN_STATUS
} fota_state_t
```

**成员**

| 成员 | 描述 |
|------|------|
| `SUCCEED` | 默认状态。升级成功，并且 A/B 系统同步成功。 |
| `UPDATE` | 正在升级非激活系统 |
| `BACKUP` | 正在同步非激活系统 |
| `FAILED` | 升级失败 |
| `WRITEDONE` | 升级非激活系统成功 |
| `NEEDSYNC` | 已切换运行系统，尚未同步系统。 |
| `UNKNOWN_STATUS` | 未知状态，代码中用于边界检查。 |

---

### 3.5 ql-lib-absys 库相关 API

#### 3.5.1 头文件

头文件为 `ql_absys.h` 和 `fota_info.h`，位于 SDK 包的 `ql-sysroots\usr\include` 目录下。若无特别说明，本文档所涉及头文件均在该目录下。

---

#### 3.5.2 函数概览

**表 3：ql-lib-absys 库函数概览**

| 函数 | 说明 |
|------|------|
| `ql_absys_getstatus()` | 查询非激活系统状态 |
| `ql_absys_get_cur_active_part()` | 查询当前运行系统 |
| `ql_absys_sync()` | 同步未激活系统 |
| `ql_absys_switch()` | 切换待运行系统 |

---

#### 3.5.3 函数详解

##### 3.5.3.1 ql_absys_getstatus

该函数用于查询非激活系统状态。

**函数原型**

```c
int ql_absys_getstatus(sysstatus_t *sys_state)
```

**参数**

- `sys_state`：
  - [Out] 指针，指向存放非激活系统状态的结构体；详见 [3.5.3.1.1 sysstatus_t](#35311-sysstatus_t)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

---

##### 3.5.3.1.1 sysstatus_t

非激活系统状态结构体定义如下。状态信息存储在 Flash 中，掉电不会丢失。

```c
typedef struct
{
    fota_state_t    ota_state;
    bool            is_damaged;
    unsigned char   damaged_partname[16]
} sysstatus_t
```

**参数**

| 类型 | 参数 | 描述 |
|------|------|------|
| `fota_state_t` | `ota_state` | 当前非激活系统升级状态；详见 [3.4.3.2.2 fota_state_t](#34322-fota_state_t)。 |
| `bool` | `is_damaged` | 非激活系统是否损坏 |
| `unsigned char` | `damaged_partname` | 非激活系统发生损坏的分区名称 |

---

##### 3.5.3.2 ql_absys_get_cur_active_part

该函数用于查询当前运行的系统。

**函数原型**

```c
int ql_absys_get_cur_active_part(absystem_t *cur_system)
```

**参数**

- `cur_system`：
  - [In/Out] 指针，指向用于存储读取到的信息枚举；详见 [3.5.3.2.1 absystem_t](#35321-absystem_t)。

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

---

##### 3.5.3.2.1 absystem_t

A/B 系统枚举定义如下：

```c
typedef enum absystem
{
    SYSTEM_A = 0,
    SYSTEM_B = 1
} absystem_t
```

**成员**

| 成员 | 描述 |
|------|------|
| `SYSTEM_A` | A 系统 |
| `SYSTEM_B` | B 系统 |

---

##### 3.5.3.3 ql_absys_sync

该函数用于同步非激活系统，将当前运行系统的镜像文件拷贝并覆盖至非激活系统。调用该函数后升级状态切换为 `BACKUP`；同步成功后升级状态切换为 `SUCCEED`，同时清除分区损坏标记（即 `is_damaged = FALSE`）以及损坏分区名称记录。

**函数原型**

```c
int ql_absys_sync(void)
```

**参数**：无

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功 |
| -1 | 函数执行失败 |

---

##### 3.5.3.4 ql_absys_switch

该函数用于切换 A/B 系统。成功执行该函数设置完系统标记后，自动重启模块实现系统切换。

切换条件说明：

1. 若升级状态为 `SUCCEED`，可切换系统；
2. 若升级状态为 `WRITEDONE`，表示非激活系统已升级完成，可进行系统切换；切换过程中模块将自动重启且升级状态更新为 `NEEDSYNC`；
3. 若升级状态为 `UPDATE` 或者 `BACKUP`，表示正在升级非激活系统或者正在同步系统，**不可切换系统**；若进行系统切换，升级会失败并可能造成其他错误；
4. 若升级状态为 `FAILED`，表示非激活系统被损坏（升级非激活系统失败或同步非激活系统失败），**不可切换系统**；若进行系统切换，切换后检测到系统损坏会再次切换回来；
5. 若 `is_damaged` 是 `TRUE`，表示非激活系统有分区损坏，**不可切换系统**；若进行系统切换，切换后检测到系统损坏会再次切换回来，可调用 `ql_absys_getstatus()` 查询损坏分区信息。

**函数原型**

```c
int ql_absys_switch(void)
```

**参数**：无

**返回值**

| 返回值 | 说明 |
|--------|------|
| 0 | 函数执行成功，等待模块重启 |
| -1 | 获取或者设置状态时出错 |
| -2 | 非激活系统正在被升级或者正在被同步，不能切换 |
| -3 | 非激活系统升级时被破坏或者同步时被破坏，不能切换 |

---

### 3.6 示例

QuecOpen SDK 中提供函数测试示例 `sample/absys/test_absys`，本章节介绍如何使用该测试程序进行 A/B 系统升级。

#### 3.6.1 制作升级包

请参考 **第 3.1.2 章**制作升级包。

---

#### 3.6.2 编译测试程序

在 Ubuntu 终端窗口进入 QuecOpen SDK 的 `sample/absys/` 目录，执行 `make` 开始编译。编译完成后，生成可执行文件 `test_absys` 并存储在当前目录下。

```bash
cd ~/workspace/ASR_ADK/ql-ol-extsdk_ec200acntar02a06n2g_ocpu/sample/absys
make
# 编译成功后生成: Makefile  test_absys  test_absys.c  test_absys.o
```

> **备注**：测试用例中 `sample/fota/` 目录下的 `test_fota` 只提供了 `ql_abfota_start_update()` 和 `ql_abfota_get_update_status()` 两个 API 的测试，`test_absys` 提供了 A/B 系统升级所有函数的测试，因此用户只需参考使用 `test_absys`。

---

#### 3.6.3 下载升级包

将制作完成的升级包镜像 `quectel_AB_OTA.img` 下载至模块的可访问分区（rootfs_data、外挂 eMMC）；将编译生成的可执行文件 `test_absys` 上传至模块的可读写分区 `rootfs_data`。

```bash
# Windows 端使用 adb 工具下载
D:\platform-tools> adb push Z:\quectel_AB_OTA.img /data
Z:\quectel_AB_OTA.img: 1 file pushed. 1.8 MB/s (31318016 bytes in 16.215s)

D:\platform-tools> adb push Z:\test_absys /data
Z:\test_absys: 1 file pushed. 0.8 MB/s (15740 bytes in 0.020s)
```

---

#### 3.6.4 执行测试程序

赋予测试程序 `test_absys` 可执行权限并运行。

```bash
root@OpenWrt:/data# chmod 777 test_absys
root@OpenWrt:/data# ./test_absys
Quectel OTA API test sample, version : v0.0.1

0   API : ql_abfota_start_update
1   API : ql_abfota_get_update_status
2   API : ql_absys_switch
3   API : ql_absys_sync
4   API : ql_absys_get_cur_active_part
5   API : ql_absys_getstatus
-1  exit
Please enter your choice:
```

**升级操作步骤：**

**1.** 运行程序后根据提示输入 `"0"`，再输入升级包所在路径 `/data/quectel_AB_OTA.img` 或者 `/mnt/sdcard/quectel_AB_OTA.img`，回车开始升级非激活系统。

```
Please enter your choice: 0
please input the fota fbf package file: /data/quectel_AB_OTA.img
status: 0, percent: 10%
status: 0, percent: 20%
...
status: 0, percent: 100%
download firmware success!
[DBG][test_ota_api_start_195][443] Update in-active partition SUCCEED
```

**2.** 升级非激活系统成功后，显示 `"update in-active partition SUCCEED"`，随后根据提示输入 `"1"`，回车返回升级的状态信息，其中包括升级进度百分比、升级状态以及退出码：

```
[DBG][test_get_fota_upgrade_info_172][481] Current fota progress: 100
[DBG][test_get_fota_upgrade_info_173][481] Current fota state: WRITEDONE
[DBG][test_get_fota_upgrade_info_174][481] Current fota exit code: 0
```

**3.** 根据提示输入 `"4"` 查询当前运行的系统（此时为 A 系统）：

```
[DBG][test_ql_absys_get_cur_active_part_222][550] Current active part is A
```

**4.** 根据提示输入 `"2"` 进行系统切换。输入 `"2"` 并回车后，模块将自动重启并切换至已升级系统运行：

```
[DBG][test_ql_absys_switch_188][577] It is okay to swith AB part to run
[ 441.070893] procd killer:reboot:sig:15
[ 441.874736] caller:test_absys
...
```

**5.** 模块在已更新系统上运行后，执行 `cat /etc/quectel-project-version` 查询固件版本号是否已经更新至目标版本：

```bash
root@OpenWrt:/data# cat /etc/quectel-project-version
Project Name: EC200A-CNTA
Project Rev : EC200ACNTAR02A06M2GV01
Build    Date: Dec 01 2022 14:46:11
```

**6.** 根据提示输入 `"1"` 查询当前升级状态为 `NEEDSYNC`；输入 `"4"` 查询当前运行系统为 B 系统；输入 `"3"` 同步当前未激活系统，将当前运行系统（B 系统）的固件同步至非激活系统（A 系统）中；再次输入 `"1"` 查询当前升级状态为 `SUCCEED`，即完成升级：

```
[DBG][test_get_fota_upgrade_info_172][712] Current fota progress: 100
[DBG][test_get_fota_upgrade_info_173][712] Current fota state: NEEDSYNC
[DBG][test_get_fota_upgrade_info_174][712] Current fota exit code: 0

[DBG][test_ql_absys_get_cur_active_part_222][550] Current active part is B

[DBG][test_ql_absys_sync_206][937620] do AB sync succeed

[DBG][test_get_fota_upgrade_info_172][937625] Current fota progress: 100
[DBG][test_get_fota_upgrade_info_173][937625] Current fota state: SUCCEED
[DBG][test_get_fota_upgrade_info_174][937625] Current fota exit code: 0
```

---

#### 3.6.5 AB 系统非激活分区损坏查询及恢复处理

当系统使用过程中发生分区固件损坏，以 Kernel-b 分区损坏为例，则当前激活的 B 系统在下次重启后校验 Kernel-b 分区固件为损坏，并将损坏分区固件标记写入 flash flag 分区中记录，并触发系统切换到 A 系统启动，此时用户需要输入 `"4"` 查看当前激活分区发生改变，再输入 `"5"` 查询非激活 B 系统的分区损坏位置，最后输入 `"3"` 进行分区同步恢复。

**示例（Kernel-b 损坏后恢复）：**

**1.** 当前运行的激活分区是 B 系统，A/B 系统分区状态为 `succeed`。

**2.** 破坏 Kernel-b 分区固件，重新启动：

```bash
root@OpenWrt:/overlay# mtd erase /dev/mtd16
Unlocking /dev/mtd16 ...
Erasing /dev/mtd16 ...
```

重启后分区布局（可见 mtd16 为 kernel-b）：

```
mtd13: 00020000 00020000 "dtlm-b"
mtd14: 00f00000 00020000 "cpimage-b"
mtd15: 000c0000 00020000 "u-boot-b"
mtd16: 00500000 00020000 "kernel-b"      ← 被擦除
mtd17: 01400000 00020000 "rootfs-b"
```

**3.** 重启后，输入 `"4"` 查询可知当前激活系统为 A 系统，说明 Kernel-b 的破坏导致 AB 系统校验 kernel-b 失败，从而发生了系统切换。输入 `"5"`，查询 A/B 系统的状态为 `damaged`，发生分区损坏的位置在 Kernel-b，则需输入 `"3"` 进行同步。同步完成后再输入 `"5"` 进行系统状态查查，发现恢复为 `succeed` 状态，此时 Kernel-b 被恢复。

```
[DBG][test_ql_absys_get_cur_active_part_222][951654] Current active part is A

[DBG][test_ql_absys_getstatus_126][951658] absys partition status : damaged
[DBG][test_ql_absys_getstatus_127][951658] absys partition damaged position : kernel-b
[DBG][test_ql_absys_getstatus_128][951658] absys needsync!!!

[DBG][test_ql_absys_sync_206][952434] do AB sync succeed

[DBG][test_ql_absys_getstatus_122][952440] absys partition status : succeed
```

---

### 3.7 资源消耗评估

#### 3.7.1 内存消耗评估

A/B 系统升级采用全量升级方式，为流式升级，以数据包的形式进行发送、接收和写入，消耗内存很少。

| 状态 | MemTotal | MemFree | MemAvailable |
|------|---------|---------|-------------|
| 升级前 | 98052 kB | 42860 kB | 76728 kB |
| 升级中 | 98052 kB | 41256 kB | 75932 kB |

如上图对比，可用内存及空闲内存均只有略微使用，可知 A/B 系统升级对内存的依赖较小。

---

#### 3.7.2 Flash 空间消耗评估

Flash 空间消耗主要表现为升级包占用的空间。

全量升级包中包含 CPImage（3 个固件）、zImage、root、oem_data、u-boot 和 tos（安全环境）6 个分区的 8 个镜像，升级包大小约为 **22 MB**（不包含用户镜像文件）。用户可将自己的程序及镜像导入到 `oem_data` 分区中，则占用 Flash 总大小约为 22 MB + oem_data 的大小。

---

#### 3.7.3 CPU 资源消耗评估

系统运行正常后，进行 A/B 系统升级，升级过程中 CPU loading 最大值达到 **8%**，总体 CPU 占用较小；由于 A/B 系统升级的全过程由相关接口控制，完全将整个升级过程的执行权限交给用户，一般不会导致资源竞争问题。

在升级的 UPDATE 及 BACKUP 阶段进行 CPU 占用测试，CPU 使用率没有明显增加，load average 值略微上升，说明没有因为升级进程而过度影响 CPU 占用率，只是在任务等待上有所增加，可知 A/B 系统升级过程对 CPU 资源消耗影响较小。

---

## 附录 参考文档及术语缩写

**表 1：参考文档**

| 文档名称 |
|---------|
| [1] Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

**表 术语缩写**

| 缩写 | 英文描述 | 中文描述 |
|------|---------|---------|
| A/B 系统 | A/B System | 双系统，即系统 A 和系统 B |
| API | Application Programming Interface | 应用编程接口 |
| eMMC | Embedded Multi Media Card | 嵌入式多媒体卡 |
| Flash | — | 闪存 |
| FOTA | Firmware Over-The-Air | 空中固件下载 |
| MTD | Memory Technology Device | 内存技术设备（Flash 分区管理） |
| OTA | Over-The-Air | 空中下载 |
| SDK | Software Development Kit | 软件开发工具包 |
