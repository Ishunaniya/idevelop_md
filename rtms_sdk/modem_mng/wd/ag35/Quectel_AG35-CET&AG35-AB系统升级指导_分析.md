# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) A/B 系统升级指导 — 全量分析

> 本文是对原始 PDF《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_AB系统升级指导_V1.0.pdf》的逐章全量精读还原。
> 讲的是**移远自研的 A/B 双系统 OTA 升级方案**：双分区互备、升级非激活系统再切换、分区损坏自动还原；以及配套的 `ql-lib-fota` / `ql-lib-absys` 两套 C 库 API。
> **与 open_dial v1.28.4 "新增 FOTA 工具"相关**，可作为后续固件 OTA 能力的设计参考。

---

## 0. 文档元信息

| 项 | 内容 |
|---|---|
| 文档名 | AG35-CET&AG35-EUT QuecOpen(SDK) A/B 系统升级指导 |
| 适用模块系列 | LTE Standard 模块系列：**AG35-CET、AG35-EUT** |
| 版本 | 1.0（**受控文件**，非临时版） |
| 日期 | 2024-07-30 |
| 总页数 | 36 页（正文 8~35） |
| 作者 | Searle FANG（创建 Ethan WEN, 2023-08-23） |
| 厂商 | 上海移远通信技术股份有限公司（Quectel） |

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| -（创建） | 2023-08-23 | Ethan WEN | 文档创建 |
| 1.0 | 2024-07-30 | Searle FANG | 1. 新增适用模块 AG35-EUT；2. 基于 QuecOpen 统一命名；3. 增 fbf 升级包文件说明(3.1.2.2)；4. ql-lib-fota 增 `ql_abfota_start_update_control()`(3.4.3.1)；5. ql-lib-absys 增 `ql_absys_sync_control()`(3.5.3.3)；6. 增升级过程 CPU 占用率测试(3.6.6) |

---

## 1. 整书目录树

```
1  引言
2  A/B 系统升级概述
   2.1  A/B 系统升级简介（双分区互备）
   2.2  A/B 系统启动流程（obm→asr flag→u-boot→kernel/rootfs/cpimage）
   2.3  A/B 系统升级方案（.img 升级包）
   2.4  A/B 系统分区损坏还原（is_damaged + 自动切换）
3  调用 ql-lib-absys 和 ql-lib-fota 库升级
   3.1  升级步骤
        3.1.1  制作升级包镜像（gen_fota_image.sh / mkotafbf / blf）
        3.1.2  下载升级包
   3.2  升级状态切换流程（6 态状态机）
   3.3  API 调用推荐流程
   3.4  ql-lib-fota 库（触发升级 + 查询状态）
        ql_abfota_start_update_control / ql_abfota_start_update / ql_abfota_get_update_status
   3.5  ql-lib-absys 库（查询/同步/切换）
        ql_absys_getstatus / get_cur_active_part / sync_control / sync / switch
   3.6  示例（test_abfota.c 交互流程 + 损坏还原 + CPU 占用测试）
   3.7  资源消耗评估（内存/Flash/CPU）
4  附录 参考文档及术语缩写
```

> 含多张界面/流程截图（图 1~10），但全部 API 原型、参数、枚举、状态机文本可 100% 还原，已完整收录。

---

## 2. 逐章全量内容

### 第 1 章 引言

- AG35-CET/EUT 支持 QuecOpen® 方案。本文档介绍**移远自研的 A/B 系统升级方案**：升级包制作、升级流程、`ql-lib-fota`/`ql-lib-absys` API、示例与资源消耗。

### 第 2 章 A/B 系统升级概述

#### 2.1 简介（双分区互备）

- 开机相关分区做成 **A、B 两套，均烧相同镜像、互为备份**。一套损坏即切到另一套运行，并还原损坏的那套。
- **有 A/B 两套的分区**（表 1，6 类）：

| No. | A 系统 | B 系统 | 更新频率 |
|---|---|---|---|
| 1 | oemapp-a | oemapp-b | 用户程序及配置，仅升级用户程序/配置时更新 |
| 2 | cpimage-a | cpimage-b | 移远更新固件基线/内容时更新 |
| 3 | u-boot-a | u-boot-b | 客户改分区时需升级 |
| 4 | kernel-a | kernel-b | 移远更新或用户自定义修改时更新 |
| 5 | rootfs-a | rootfs-b | 同上 |
| 6 | tos-a | tos-b | 移远更新固件基线/内容时更新 |

- **非双份的分区**：`obm`、`asr flag`（激活标志）、`dtim`、`rootfs_data`、`oemdata`、`QUEC BBM`（坏块管理）。

#### 2.2 启动流程

- 上电后 **obm 从 `asr flag` 分区固定位置读激活标志**（"a"/"b"）。
- 以启动 B 为例：obm 加载 B 的 u-boot+tos → u-boot 再读 asr flag → 从 B 的 dtim 解析 kernel/rootfs/cpimage 的 flash 地址 → 校验并加载 → 启动参数加 `system=b` 传给内核 → 挂载 rootfs → 加载 oemapp。
- 每个镜像在内核设备树对应一个分区 = 一个 MTD 设备 `/dev/mtdblockx`。
- 校验失败则**切换到另一系统**（A↔B）；两套同名分区都坏则启动失败，等重新烧录固件。
- **关键约定**：开机启动的系统=激活系统，未启动的=非激活系统。**升级只升非激活系统，运行中的激活系统不允许升级。**

#### 2.4 分区损坏还原

- 开机若分区损坏，模块自动切系统加载，并把 `is_damaged` 标记为 TRUE、记录损坏分区名。
- **应用程序需主动调 `ql_absys_getstatus()` 检测非激活系统是否损坏；若损坏，调 `ql_absys_sync()` 同步还原。** 还原完成后自动清标记（is_damaged=FALSE，分区名=NULL）。
- 若 A/B 同名分区**同时**损坏 → 启动失败，等重烧。

### 第 3 章 调用 ql-lib-absys 和 ql-lib-fota 库升级

> **重要备注**：模块默认通过这两库 API 触发升级，并由 SDK 中 **`ql-ol-rootfs/usr/bin/ql_otad` 服务**管理升级与分区同步。客户可选移除 `ql_otad`，但**移除后分区自动还原功能也没了**，需自己写自启动服务调 `ql_absys_getstatus()` + `ql_absys_sync()`。

#### 3.1 升级步骤

1. 从移远拿目标固件包，制作升级包镜像 **`quectel_AB_OTA.img`**。
2. 把升级镜像 + 升级程序 `test_abfota` 下载到模块指定目录（内部 flash 或 eMMC）。
3. 用 `test_abfota` 触发升级（或自写升级程序）。

#### 3.1.1 制作升级包镜像

- 环境（Linux）：脚本 `tools/fota/gen_fota_image.sh`，指导 `tools/fota/Readme.txt`。
- 所需文件：`quectel_skylark_pm802_standard_AB.blf`（非工厂配置）、`quectel_AB_OTA.blf`（OTA 配置）、`mversion`（FOTA 版本号）。
- 所需工具：`tim_builder`（编译固件生成 tim）、`mkotafbf`（制作 fbf 升级包）。
- `mkotafbf` 常用参数：`-f` 输入 blf 名 / `-o` 输出 fbf 名 / `-v` FOTA 版本号(mversion)。
- **blf 一致性坑**：改了分区表要同步改固件包 `update/update.blf`（见参考文档[2]）；`*_standard_AB.blf` 须与 `update.blf` 一致；`quectel_AB_OTA.blf` 的 `Image_Flash_Entry_Address` 与 dtim 起始地址须与 `update.blf` 对应字段一致。`quectel_AB_OTA.blf` 比 `update.blf` 少 `tim_falcon_qspinand.bin`、`TLoader_QSPINAND.bin` 两镜像属**正常**。
- 制 fbf 时须把固件包的 `TLoader_QSPINAND.bin` 拷到 `/fota/` 目录以编进 TIM。
- **全量包制作**（SDK 默认全量）：`chmod` 赋权 → 拷目标固件 + `TLoder_QSPINAND.bin` 到 `/fota/` → 确认 blf 一致 → `./gen_fota_image.sh` → 生成 `quectel_AB_OTA.img`。

#### 3.1.2 下载升级包

- `quectel_AB_OTA.img` 下到 flash 或 eMMC，**所在分区必须开机自动挂载**，否则重上电找不到升级包报错。

#### 3.2 升级状态切换流程（6 态状态机）

状态：`SUCCEED / UPDATE / WRITEDONE / NEEDSYNC / BACKUP / FAILED`，存于 flash **掉电不丢**。

```
SUCCEED ──(1)触发升级──> UPDATE ──(2)失败──> FAILED
                          │(3)成功
                          ▼
                       WRITEDONE ──(4)switch切换+重启──> NEEDSYNC
                                                          │(5)sync同步
                                                          ▼
                                                        BACKUP ──(6)同步失败──> FAILED
                                                          │(7)同步成功
                                                          ▼
                                                        SUCCEED
```

1. SUCCEED = 初始/可升级态。
2. `ql_abfota_start_update()` 触发 → UPDATE（升级非激活系统）。
3. 升级成功 → WRITEDONE；失败 → FAILED。
4. `ql_absys_switch()` 切系统 → 应用程序重启模块 → 跑新固件 → NEEDSYNC（A/B 不一致需同步）。
5. `ql_absys_sync()` 同步 → BACKUP。
6. 同步成功 → SUCCEED；失败 → FAILED。

**备注**：① 升级失败可直接再升级，或调 `ql_absys_sync()` 还原；② 无论成败模块都**不删升级包**，下载/删除由应用管理；③ NEEDSYNC 时模块已在新系统运行，**可先上报升级结果，但分区同步必须执行**。

#### 3.4 ql-lib-fota 库

- 头文件：`ql_fota_api.h`、`fota_info.h`（`ql-sysroots/usr/include/`）。

| 函数 | 说明 |
|---|---|
| `ql_abfota_start_update_control()` | 触发升级 + 设升级时 CPU 占用率 |
| `ql_abfota_start_update()` | 触发升级 |
| `ql_abfota_get_update_status()` | 获取当前升级信息 |

```c
int ql_abfota_start_update_control(const char *package_file, int cpu_loading_level);
int ql_abfota_start_update(const char *package_path);
int ql_abfota_get_update_status(update_info_t *update_info);
```

- `package_file/path`：升级包路径，名字固定 `quectel_AB_OTA.img`，如 `/mnt/sdcard/quectel_AB_OTA.img`。
- **`cpu_loading_level` 0~5**（控制升级占 CPU）：`0≈98%` / `1≈55%` / `2≈38%` / `3≈30%` / `4≈25%` / `5≈20%`。值越大占用越低、升级越慢。
- 返回：`0` 成功 / `-1` 参数无效或设起始状态失败 / 其他=`fota_exit_code_t`。

```c
typedef enum {            // fota_exit_code_t 异常退出码
    E_NO_ERROR=0,             // 升级成功
    E_FOTA_INIT_FAILED=1,     // 升级初始化失败
    E_UPDATE_PACKAGE_NOEXIST=2, // 升级包不存在
    E_WRITE_SEG_FAILED=3,     // 下载段数据失败
} fota_exit_code_t;

typedef struct {          // update_info_t
    unsigned int     percentage;  // 进度 0~100
    fota_state_t     ota_state;    // 升级状态
    fota_exit_code_t exit_code;    // 异常退出码
} update_info_t;

typedef enum {            // fota_state_t（存 flash 掉电不丢）
    SUCCEED = 0, UPDATE, BACKUP, FAILED, WRITEDONE, NEEDSYNC, UNKNOWN_STATUS
} fota_state_t;
```

> 注：`fota_state_t` 枚举**底层数值顺序**为 SUCCEED=0/UPDATE=1/BACKUP=2/FAILED=3/WRITEDONE=4/NEEDSYNC=5（与 3.2 章节叙述顺序不同，比较时以此枚举为准）。

- 备注：`get_update_status` 取到 FAILED 且 exit_code≠0 = 升级失败；继续升级调 `ql_abfota_start_update()` 即可恢复正常状态。

#### 3.5 ql-lib-absys 库

- 头文件：`ql_absys.h`、`fota_info.h`。

| 函数 | 说明 |
|---|---|
| `ql_absys_getstatus()` | 查非激活系统状态 |
| `ql_absys_get_cur_active_part()` | 查当前运行系统 |
| `ql_absys_sync_control()` | 同步非激活系统 + 设 CPU 占用率 |
| `ql_absys_sync()` | 同步非激活系统 |
| `ql_absys_switch()` | 切换 A/B 系统 |

```c
int ql_absys_getstatus(sysstatus_t *sys_state);
int ql_absys_get_cur_active_part(absystem_t *cur_system);
int ql_absys_sync_control(int cpu_loading_level);   // cpu_loading_level 0~5 同上
int ql_absys_sync(void);
int ql_absys_switch(void);

typedef struct {          // sysstatus_t（存 flash 掉电不丢）
    fota_state_t  ota_state;
    bool          is_damaged;            // 非激活系统是否损坏
    unsigned char damaged_partname[16];  // 损坏分区名
} sysstatus_t;

typedef enum absystem { SYSTEM_A=0, SYSTEM_B=1 } absystem_t;
```

- **`ql_absys_switch()` 切换规则与返回值**：
  - SUCCEED 可切；WRITEDONE 可切（切时自动重启，状态→NEEDSYNC）。
  - UPDATE/BACKUP **不可切**（正在升级/同步，强切会失败甚至出错）。
  - FAILED **不可切**（非激活系统已损坏，切后检测到损坏会自动切回）。
  - `is_damaged=TRUE` **不可切**（同上，切回；用 `getstatus` 查损坏分区）。
  - 返回：`0` 成功等重启 / `-1` 取/设状态出错 / `-2` 正在升级或同步不可切 / `-3` 升级或同步时被破坏不可切。
- `sync`/`sync_control` 成功后自动清 `is_damaged=FALSE`、`damaged_partname=NULL`。

#### 3.6 示例（`sample/abfota/test_abfota.c`）

交互菜单（数字命令）：`0`=升级非激活系统（输路径）→显示 "Update in-active partition SUCCEED"；`1`=查升级状态信息；`4`=查当前运行系统；`2`=切换系统（自动重启）；`3`=同步非激活系统；`5`=查/检系统损坏状态；`6`=带 CPU 占用率升级；`7`=带 CPU 占用率同步。

- **验证固件版本**：切换并跑新系统后 `cat /etc/quectel-project-version` 确认升级到目标版本。
- **下载位置**：升级包 `.img` 放可访问分区（`rootfs_data`/`oemdata`/外挂 eMMC）；`test_abfota` 放可读写分区（`rootfs_data`/`oemdata`）。
- **3.6.5 损坏还原实测**（以 kernel-b 损坏为例）：破坏 kernel-b → 重启 → 校验失败自动切到 A 系统 → 输 `5` 查到 kernel-b damaged → 输 `3` 同步 → 输 `5` 复查恢复 SUCCEED。

#### 3.7 资源消耗

- **内存**：全量流式升级（按数据包发收写），内存消耗很小。
- **Flash**：全量包含 6 分区 8 镜像——cpimage(ARBEL.bin/MSA.bin/RFPLUGIN.bin)、kernel(zImage)、rootfs(root.squashfs)、oemapp(oemapp.squashfs)、u-boot(u-boot.bin)、tos(tos.bin)，**约 30 MiB**（不含用户镜像；含则 30MiB+oemapp）。
- **CPU**：升级时 CPU 负载最大约 **8%**，全过程由 API 控制，一般无资源竞争；UPDATE/BACKUP 阶段 CPU 无明显增加。

---

## 3. 关键警告与坑（手册"备注"汇总）

1. **只能升级非激活系统**，运行中的激活系统不可升级。
2. **升级包分区必须开机自动挂载**，否则重上电找不到包报错。
3. **移除 `ql_otad` = 丢失分区自动还原**，必须自己写服务定期 `getstatus`+`sync`。
4. **UPDATE/BACKUP/FAILED/is_damaged 时禁止 `switch`**，强切会失败或自动切回。
5. **switch 后状态变 NEEDSYNC，分区同步必须执行**，否则 A/B 长期不一致，下次某系统损坏失去备份保护。
6. **模块不自动删升级包**，下载/清理由应用负责（注意 flash 空间）。
7. **blf 文件一致性**：改分区表必须同步 `update.blf`，否则升级包地址错乱。
8. **A/B 同名分区同时损坏 = 砖**，只能重烧固件。
9. **cpu_loading_level 取 0 会吃满 ~98% CPU**，车机场景应取较大值（如 4/5）保业务。

---

## 4. 对 open_dial 项目的适用性批注

> open_dial 跑 **EC200A（OpenCPU/ASR1803）**，本文档面向 **AG35（LTE Standard）**。两者同属移远 ASR QuecOpen，A/B 升级是 ASR 平台共性能力，但 **EC200A 是否支持 A/B 双系统、库名/分区名是否一致，必须以 EC200A 的 SDK（`ql_absys.h`/`ql_fota_api.h` 是否存在、是否有 `ql_otad`）为准核对**，不能照搬。CLAUDE.md 记 v1.28.4 "新增 FOTA 工具"，本文档正是评估这条路线的参考。

| 文档内容 | 与 open_dial 的关系 / 可借鉴点 |
|---|---|
| **A/B 双系统 + 损坏自动还原** | 与 open_dial 的可靠性目标高度契合：若 EC200A 支持，OTA 升级失败可回退、分区损坏自愈，比"升坏变砖"安全得多。是把现有 FOTA 工具升级为"安全 OTA"的方向。 |
| **升级只升非激活系统、运行不中断** | A/B 升级期间当前系统照常跑——这对 open_dial "OpenCPU 下不可整机复位影响同机业务"的核心约束很友好：升级过程不打断拨号，只在 `switch` 那一刻重启切换。 |
| **`cpu_loading_level` 控 CPU 占用** | 升级/同步可限 CPU（车机选 4/5≈20~25%），避免抢占 dial 主循环与 ping 检测的 CPU，保拨号守护实时性。 |
| **6 态状态机 + flash 持久化** | `fota_state_t`/`sysstatus_t` 存 flash 掉电不丢——open_dial 若做 OTA，可参考这套状态机做断点续传/重启续传，与现有 `reboot_conf/` 持久化思路一致。 |
| **`ql_absys_getstatus()` + `is_damaged` 巡检** | 若移除 `ql_otad` 自管，可把"开机查非激活系统是否损坏→损坏则 sync 还原"做进 open_dial 的初始化或 diag 巡检，提升整机健壮性。 |
| **switch 触发重启 = 应用主动 reboot** | 与 open_dial L3（exit 重拉，避免 cfun=1,1 整机复位）的取舍同源：A/B switch 必然重启整机，需评估是否能接受（同机 wifi/换电业务中断），最好做空闲门控后再切。 |
| **`cat /etc/quectel-project-version` 验版本** | 可作为 OTA 后自检手段，与 open_dial `/tmp/dial_version` 版本上报并列，确认底层固件与应用版本匹配。 |

**结论**：本文档对 open_dial 是**中等相关、属能力规划/可选增强**——不是当前 `fix_cp_dump` 的直接答案，但若把 v1.28.4 的 FOTA 工具演进为"带回退的安全固件 OTA"，A/B 方案是首选路线。**前置动作**：先在 EC200A SDK 中确认是否存在 `ql-lib-absys`/`ql-lib-fota` 与 `ql_otad`；不存在则本方案不适用，需走 EC200A 自身的升级机制。
