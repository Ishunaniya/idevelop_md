# EC200A R03A02 全量升级、OBM 集成与远程 SDK 审计记录

> 日期：2026-07-28  
> 范围：移远沟通内容、当前 `md/firmware` 交付物、远程 R03A02 SDK 半成品只读检查  
> 目的：明确现有包能否继续使用、R03A02 正确升级基线、OBM 集成机制，以及后续缺口  
> 说明：远程检查全程只读，没有修改、编译、删除或复制远程文件。本文不记录登录密码。

## 1. 结论摘要

### 1.1 核心结论

1. 移远已经明确：从当前版本升级至 R03A02，需要做**全量升级**，并且**必须升级
   OBM**。
2. 我方对 `zImage` 和 `root.squashfs` 做过产品适配，不能直接用移远原始 R03A02
   镜像替换；正确方法是以 R03A02 SDK 为底座迁移适配后重新构建。
3. 本地当前固件交付物
   `md/firmware/fota_out_R03A02CP_20260630/` 采用的是：
   - R02A04 AP/内核/rootfs/引导底座；
   - 仅 CP 三件套升级至 R03A02；
   - 不包含 OBM 自动升级材料。
4. 上述本地包不符合移远最新明确要求，不能作为正式升级候选包，也不能用于验证最终
   R03A02 全量升级方案。
5. 远程服务器存在一套可成功构建的 R03A02 SDK，但当前状态更像“原始/基础 SDK
   完整编译过一次”，而不是完成我方产品迁移的候选工程：
   - 已生成 R03A02 `zImage`、`root.squashfs`、`u-boot.bin`；
   - 没找到我方 `dial`、`/usr/dial`、`start_prog` 等业务内容；
   - `/boot` 为空，尚未集成 OBM 三个产物；
   - 没有 Git 仓库，缺乏源码修改历史；
   - 没有证据证明我方板级内核适配已经迁移完成。
6. 服务器 R03 rootfs 自带完整的 OBM 自动升级执行框架，但缺少真正要写入的 OBM
   材料。

### 1.2 当前包能否“先升级验证”

结论：**不能用来验证最终升级方案，不建议继续刷。**

它只能在一个非常狭窄的场景中作为历史问题复现实验材料：

- 使用明确可通过 USB SWDownloader 恢复的专用报废/实验设备；
- 实验目标只是确认“R02 AP/rootfs + R03 CP、无 OBM”的混合组合会发生什么；
- 接受设备可能卡 OBM、CP crash、verity 失败、反复重启，甚至需要线刷恢复；
- 不把实验成功解释为正式方案安全。

已有早期 `cpimage-only` 真机测试已经出现旧 OBM 无法正常启动 R03 CP、随后 A/B
回滚的结果，因此再次刷当前混合包的新增验证价值很低，风险高于收益。

## 2. 信息来源与证据等级

本文把结论分为四类，避免把推断写成事实。

### 2.1 移远明确答复

来自本次提供的聊天原文：

- 当前镜像是全量包，包含根文件系统和 OEM 数据。
- 目标升级版本是 R03A02。
- 升级到 R03A02 时内核、根文件系统等需要全量升级。
- R02 → R03 必须注意升级 OBM。
- 我方已有适配应同步迁移至 R03A02 镜像。
- OBM 参考流程是：
  1. 把 R03 固件包中的 `TLoader_QSPINAND.bin`、`hotfix.bin` 放进 R03 SDK
     `tools/fota/`；
  2. 执行 `gen_obm_fota_image.sh`；
  3. 生成并复制 `quectel_obm.md5`、`quectel_obm_ota.img`、
     `quectel_obm.size` 至 `ql-ol-rootfs/boot/`；
  4. 重新 `makerootfs`；
  5. 用新 rootfs 制作 R03 全量 OTA；
  6. 切换启动 R03 后，由后台机制自动检查并升级 OBM。

### 2.2 我方同事明确说明

- `zImage` 涉及 USB 口、EXTFS 挂载、SPI 等硬件接口适配。
- `root.squashfs` 增加了我方应用，其余大部分为移远内置内容。
- 正确迁移不是简单替换两个 bin：
  - 需要 R03 SDK；
  - 需要修改/确认内核配置；
  - 需要将我方应用放入 R03 rootfs；
  - 需要重新构建 rootfs。
- 服务器上可能存在一套 R03 SDK 半成品。
- rootfs 迁移较复杂，因为缺少完整的应用、库和配置清单。

### 2.3 本地文件实查

本地目录：

```text
md/firmware/fota_out_R03A02CP_20260630/
```

其 `README.md` 和 `MANIFEST.md` 明确记录：

- 出货底座为 R02A04；
- `root.squashfs` 保留出货版，包含我方 dial；
- `zImage` 保留 R02A04；
- `u-boot.bin`、`tos.bin`、`oem_data.ubi` 等保留 R02A04；
- 仅 `ARBEL.bin`、`MSA.bin`、`RFPLUGIN.bin` 换成 R03A02；
- TIM/DTim 随新 CP 重算；
- FOTA 包不包含 OBM；
- 当前包尚未通过真机验证。

### 2.4 远程服务器只读实查

检查路径：

```text
/home/xp/tbox/ql-ol-extsdk-ec200acntar03a02m2g_ocpu
```

远程工程没有执行任何修改或构建操作。

## 3. 对移远沟通内容的逐项分析

### 3.1 “这个是全量包”

移远确认其语境中的 R03A02 OTA 是全量包，至少覆盖：

- CP；
- 内核；
- rootfs；
- OEM 数据；
- 全量升级描述中列出的其他 AP 组件。

这与当前本地“R02A04 底座 + R03A02 CP”方案不同。不能因为当前本地包尺寸约
36.5 MB、包含多个分区镜像，就把它等同于“R03A02 全量基线包”。

“包含多个镜像”描述的是包装内容；“全量 R03A02”还要求镜像版本和基线属于 R03A02。

### 3.2 “内核、根文件这些也要升级”

移远的答复是肯定的，因此当前保留 R02A04 `zImage` 和 `root.squashfs` 的方案失去正式
依据。

但不能直接采用移远原始 R03A02 的 `zImage/root.squashfs`，因为我方已有产品适配。
正确方法不是“保留旧镜像”，也不是“直接替换成原厂新镜像”，而是：

```text
R03A02 原始基线
  + 我方内核/设备树/驱动适配
  + 我方应用、库、配置、启动脚本
  = 我方 R03A02 产品镜像
```

### 3.3 我方内核适配的影响

已知适配范围包括：

- USB 接口；
- EXTFS/EXFAT 等文件系统挂载能力；
- SPI；
- 其他硬件接口。

需要进一步恢复的具体内容包括：

- 内核 `.config` 差异；
- 设备树/DTS 差异；
- GPIO、pinmux、供电和时钟配置；
- USB Host/Device 模式与 Gadget 组合；
- SPI 总线号、片选、频率、工作模式；
- 外部存储挂载脚本；
- 是否修改驱动源码；
- 内核命令行和 dm-verity 参数。

“相关功能已在配置中启用”不等于板级适配完成。

### 3.4 rootfs 迁移的影响

已知 rootfs 至少包含我方业务应用。完整迁移还可能涉及：

- `dial` 主程序；
- 动态库；
- 配置文件；
- init/procd 服务；
- `start_prog` 或守护逻辑；
- 证书、数据文件和软链接；
- 文件权限和用户组；
- 日志目录；
- NVM、SD 卡、EXTFS 挂载逻辑；
- SDK 用户态库兼容性；
- SELinux 策略。

只复制一个可执行文件不能证明 rootfs 迁移完整。

### 3.5 OEM 数据分区

移远确认全量包包含 OEM 数据，但当前尚不能据此断定可以无条件覆盖设备 OEM 分区。
仍需确认：

- 是否含设备唯一信息、证书、标定或客户配置；
- R03A02 是否改变 OEM 数据格式；
- A/B OTA 对 OEM 数据的具体写入策略；
- 是否应由 R03 SDK 重建；
- 是否需要保留我方现有 OEM 内容。

## 4. OBM 官方集成流程分析

### 4.1 不是把两个文件直接塞进 FOTA

移远提供的流程实质是：

```text
R03 TLoader + hotfix
        ↓
gen_obm_fota_image.sh
        ↓
quectel_obm_ota.img/.md5/.size
        ↓
放进 R03 rootfs 的 /boot
        ↓
重新 makerootfs
        ↓
制作 R03 全量 OTA
        ↓
新系统 preinit 自动升级 OBM
```

因此“保留现有 rootfs，只补 OBM”在严格意义上不存在。只要把 OBM 材料放进
`/boot`，就必须重新构建 `root.squashfs`，并处理对应校验。

### 4.2 远程 R03 rootfs 的真实执行逻辑

服务器中存在：

```text
ql-ol-rootfs/lib/preinit/83_upgrade_obm
ql-ol-rootfs/usr/bin/ql_ota_obm
```

`83_upgrade_obm` 在 preinit 阶段检查：

```text
/boot/quectel_obm_ota.img
/boot/quectel_obm.md5
/boot/quectel_obm.size
```

其主要流程：

1. 如果任一文件不存在，则退出，不升级 OBM。
2. 读取目标 OBM MD5 和真实长度。
3. 检查 `/NVM/.fota_obm_state`。
4. 从 `/dev/mtd0` 导出当前 OBM 并计算 MD5。
5. 如果当前 OBM 已匹配，则写完成标志并退出。
6. 检查 OTA/OTAD 版本是否至少为 rls846。
7. 挂载 debugfs。
8. 解锁 bootloader MTD 写保护。
9. 调用 `/usr/bin/ql_ota_obm /boot/quectel_obm_ota.img`。
10. `sync()` 后重新读取 MTD0 校验。
11. 失败最多重试三次。
12. 成功后重新锁定写保护，并写入 `/NVM/.fota_obm_state`。

### 4.3 OBM 写入风险

脚本原文明确警告：

- OBM 写入约需 200 ms；
- 写入过程中掉电或复位会破坏 OBM；
- 破坏后设备可能无法再次启动。

因此 OBM 不能按普通 A/B 分区看待。即使 AP/CP 能回滚，OBM 写坏也可能无法由 A/B
自动恢复。

### 4.4 hotfix 的未决问题

服务器的 `gen_obm_fota_image.sh` 明面逻辑：

- 运行 `tim_builder`；
- 使用 `quectel_obm_ota.blf` 生成 `quectel_obm_ota.img`；
- 对 `TLoader_QSPINAND.bin` 生成 MD5 和 size。

脚本正文没有直接出现 `hotfix.bin`。可能原因包括：

- `hotfix.bin` 由 BLF/TIM 间接引用；
- 移远提供的是另一版脚本；
- 聊天描述省略了脚本或文件替换步骤；
- 当前服务器 SDK 与移远口述版本不完全一致。

必须向移远索取：

- 与目标完整型号匹配的 `TLoader_QSPINAND.bin`；
- `hotfix.bin`；
- 正式脚本和 BLF；
- 文件 MD5/SHA-256；
- 首次启动、写入、成功确认和失败恢复说明。

## 5. 远程 R03A02 SDK 审计结果

### 5.1 工程版本

`ql-ol-rootfs/etc/quectel-project-version`：

```text
Project Name: EC200A-CNTA
Project Rev : EC200ACNTAR03A02M2G_OCPU
Build Date  : Mar 05 2026 20:03:49
```

### 5.2 已有构建产物

```text
target/zImage          4,433,731 bytes
target/root.squashfs  18,133,311 bytes
target/u-boot.bin        480,236 bytes
```

说明 SDK 至少成功完成过一次全量基础构建。

### 5.3 Git 与可追溯性

工程目录不是 Git 仓库：

```text
fatal: not a git repository
```

因此无法通过 commit、diff 或 blame 直接追溯谁修改了哪些源文件。

### 5.4 内核配置

当前配置已启用：

```text
CONFIG_USB=y
CONFIG_USB_STORAGE=m
CONFIG_SPI=y
CONFIG_SPI_SPIDEV=m
CONFIG_EXFAT_FS=y
CONFIG_EXT4_FS=y
CONFIG_FUSE_FS=y
```

同时包含 USB Gadget、RNDIS、NCM、MBIM 等功能。

但是将当前 `.config` 与服务器保存的 SDK 压缩包中的 `.config` 精确比较后，差异仅为：

- 编译器 build 标识变化；
- `PLUGIN_HOSTCC`；
- GCC plugins 自动配置。

没有发现 USB、SPI、EXFAT、EXT4、FUSE 等功能开关变化。

因此基于现有证据只能得出：

> R03 SDK 当前配置具备相关通用能力，但不能证明我方板级适配已迁移。

### 5.5 rootfs 内容

未找到：

```text
dial
/usr/dial
start_prog
open_dial
```

当前 rootfs 与 SDK 原始 `ql-ol-rootfs.tar.gz` 比较，主要差异为：

- 重新编译产生的内核模块；
- 工程版本和构建时间；
- SELinux 构建产物；
- 少量文件权限变化。

没有发现我方业务应用和完整运行环境已经加入的证据。

### 5.6 OBM 材料

当前：

```text
ql-ol-rootfs/boot/
```

为空。

`tools/fota/` 中存在：

```text
gen_obm_fota_image.sh
quectel_obm_ota.blf
tim_builder
mkotafbf
```

但不存在：

```text
TLoader_QSPINAND.bin
hotfix.bin
quectel_obm_ota.img
quectel_obm.md5
quectel_obm.size
```

所以服务器半成品尚未集成 OBM。

## 6. 当前本地固件包分析

### 6.1 当前组合

本地 `fota_out_R03A02CP_20260630` 实际组合：

```text
OBM/TLoader：R02A04，且 FOTA 不升级
CP：R03A02
zImage：R02A04
root.squashfs：R02A04，含我方 dial
u-boot/tos/oem_data：R02A04
TIM/DTim：按新 CP 重算
```

### 6.2 已完成的验证

- 文件来源和 MD5 溯源；
- CP 三件套内容替换；
- TIM/DTim 哈希重算；
- FOTA 包结构和镜像地址检查；
- rootfs 中保留我方 dial 的二进制取证；
- 分区容量检查。

这些只能证明“包装结构自洽”，不能证明系统版本组合可启动。

### 6.3 未完成或已经冲突的部分

- 未使用 R03A02 AP/rootfs 基线；
- 未迁移我方适配；
- 未集成 OBM；
- 未包含 `hotfix.bin`；
- dm-verity 切槽行为未验证；
- 未验证 R03A02 全量首次启动；
- 未验证 OBM 自动升级；
- 不符合移远最新“全量 + 必须 OBM”的要求。

## 7. 为什么不建议用当前存量包先试

### 7.1 不能验证正确目标

最终目标是：

```text
我方适配后的 R03A02 AP + R03A02 CP + R03A02 OBM
```

当前包是：

```text
R02A04 AP/rootfs + R03A02 CP + R02A04/未升级 OBM
```

即使当前包启动成功，也不能证明最终全量包安全；如果失败，也只能再次证明混合基线有问题。

### 7.2 已有失败证据

此前 cpimage-only 真机测试已经出现：

- 写入完成；
- switch 成功；
- 新槽启动时旧 OBM + 新 CP 组合发生 CP crash；
- 设备卡在启动过程；
- 最终依赖 A/B 回滚恢复；
- 回滚后残留 `NEEDSYNC`，再由健康原槽 sync 收尾。

当前包依然没有升级 OBM，因此存在同类风险。

### 7.3 当前包更大的额外风险

当前 36.5 MB 包不仅更新 CP，还会向非激活槽写 AP、rootfs、OEM 数据等内容。需要额外
面对：

- dm-verity 根哈希；
- rootfs/内核启动匹配；
- OEM 数据覆盖；
- A/B 两槽同步；
- AP/CP版本混合；
- OBM 与 CP 启动兼容。

### 7.4 唯一可接受的使用边界

如果确有研究需要，必须同时满足：

1. 专用实验设备，禁止生产/现场设备；
2. USB线刷和串口连接已经实际验证；
3. 出货固件和线刷包已准备；
4. 稳定供电；
5. 接受 A/B 回滚失败后人工救砖；
6. 明确实验只验证“旧方案行为”，不验证正式 R03A02方案；
7. 不执行任何未获移远确认的单独 OBM写入。

从当前信息看，不建议投入这个实验，优先完成正确 R03A02候选包更有价值。

## 8. 正确候选包的形成路径

### 阶段一：冻结来源

1. 让移远确认完整目标型号：
   - `EC200ACNTAR03A02M2G_OCPU`
   - 或聊天中出现的 `EC200ACNTAR03A02V01M2G_OCPU`
2. 固定官方 R03A02 SDK、全量固件、TLoader、hotfix及其校验值。
3. 明确 AP、CP、OBM、OEM 数据的版本对应表。

### 阶段二：恢复我方差异

1. 找到 R02A04 产品内核源码或构建目录。
2. 对比原始 R02A04 SDK，恢复：
   - `.config`；
   - DTS/设备树；
   - 驱动；
   - USB、SPI、EXTFS相关补丁。
3. 从当前出货 rootfs 和工程恢复：
   - 应用；
   - 动态库；
   - 配置；
   - init/procd脚本；
   - SELinux规则；
   - 权限、软链接和挂载逻辑。

### 阶段三：迁移至 R03A02

1. 将确认后的内核差异迁移至 R03内核。
2. 重新编译 R03 `zImage` 和模块。
3. 将我方业务内容迁移至 R03 rootfs。
4. 验证应用与 R03用户态库兼容。
5. 按移远正式步骤生成并放入：
   - `quectel_obm_ota.img`
   - `quectel_obm.md5`
   - `quectel_obm.size`
6. 重新 `makerootfs`。
7. 处理 rootfs和 dm-verity配套校验。

### 阶段四：制作全量 OTA

1. 使用匹配真机分区布局的 BLF。
2. 使用 R03A02配套工具生成全量 OTA。
3. 检查 AP、CP、OBM、rootfs、OEM 数据来源。
4. 生成逐文件 manifest、MD5/SHA-256和构建日志。

### 阶段五：实验室验证

验证顺序至少包括：

1. 升级前版本、A/B槽位和健康状态；
2. 写入非激活槽；
3. switch和第一次启动；
4. preinit阶段 OBM升级日志；
5. OBM最终 MD5/版本；
6. AP、CP和工程版本；
7. USB、EXTFS/EXFAT、SPI等硬件接口；
8. dial、依赖库、配置和网络；
9. dm-verity；
10. `NEEDSYNC → SUCCEED`；
11. 掉电、启动失败和回滚测试；
12. 持续运行观察 CP DUMP。

## 9. 必须向移远书面确认的问题

1. 精确目标版本是否带 `V01`，两种名称是否为同一发布物。
2. R02 → R03 是否必须同时更新 AP、CP、OBM。
3. 官方全量包应包含哪些分区和镜像。
4. `TLoader_QSPINAND.bin`、`hotfix.bin` 的准确来源和校验值。
5. 为什么服务器脚本不直接出现 `hotfix.bin`，它通过何种方式进入 OBM产物。
6. 第一次切入 R03时，旧 OBM是否足以执行到 preinit升级脚本。
7. OBM升级成功后是否自动重启，以及如何确认最终版本。
8. OBM写入失败、掉电或校验失败时的官方恢复方法。
9. OEM数据是否包含唯一数据，是否允许全量覆盖。
10. R03 rootfs、zImage与 dm-verity的完整打包要求。

## 10. 最终判断

当前工程状态可概括为：

```text
本地包：保留产品应用，但版本基线错误且缺OBM
远程SDK：版本基线正确，但产品应用/适配和OBM材料不完整
```

两者都不能直接用于正式升级。

下一步不是把本地包继续试刷，也不是直接采用服务器 `target/` 的三个镜像，而是把本地
产品差异完整迁移到远程 R03A02 SDK，在官方要求下集成 OBM，重新形成可追溯的全量候选
包，再在可救砖实验设备上验证。

