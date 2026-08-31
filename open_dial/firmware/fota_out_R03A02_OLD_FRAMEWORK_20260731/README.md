# EC200A R02A04 → R03A02 完整 A/B FOTA 交付说明

## 1. 交付结论

已基于同事提供的
`IMAGE_EC200ACNTAR03A02M2G_OCPU_OLD_FRAMEWORK_20260731.zip`，使用本机
R03A02 SDK 的原始 `tim_builder`、`mkotafbf`、BLF 和生成脚本制作出完整 A/B
FOTA：

- FOTA：`quectel_AB_OTA.img`
- 大小：33,595,392 字节
- MD5：`1cae3817a6e84db3fcfb00173ca4fc04`
- SHA-256：`b3df54dddac322a52ffe9383eb14c6b48208c9ef159455a471276e7989bebbc6`

该包不是之前的“R02 AP + R03 CP”混合包，而是使用新 ZIP 中同一套 R03A02
`ARBEL/MSA/RFPLUGIN/u-boot/zImage/rootfs/tos/oem_data` 制作。只对 ZIP 中的
`root.squashfs` 做了一项必要修改：把由该 ZIP 的 `TLoader_QSPINAND.bin` 和
`hotfix.bin` 生成的 OBM 三文件放入 `/boot`，然后按 SDK 参数重新生成
SquashFS、SELinux xattr 和 dm-verity。

这是“打包和离线结构验证通过”的候选包，尚未在设备上完成 R02A04→R03A02
真机升级验证。第一次必须使用供电稳定、串口全程记录、USB SWDownloader 可救砖的
实验设备；不能直接用于现场批量升级。

## 2. 输入和事实边界

### 2.1 源 ZIP

- 路径：`../IMAGE_EC200ACNTAR03A02M2G_OCPU_OLD_FRAMEWORK_20260731.zip`
- 大小：28,541,105 字节
- MD5：`09be6b459e708674677dc796f95b60c3`
- SHA-256：`3eb7552e4f5a58316cfab61238cd552cfd72faea83fbab07863adde9218c6b8f`

源 rootfs 内已经确认存在：

- `/usr/dial/dial`：92,884 字节，可执行
- `/usr/sbin/start_prog`：10,904 字节，可执行
- `/lib/preinit/83_upgrade_obm`
- `/usr/bin/ql_ota_obm`
- 多个最新业务程序和库
- `/etc/quectel-project-version`：`EC200ACNTAR03A02M2G_OCPU`
- Build Date：`Jul 31 2026 10:05:57`

因此，本次没有从旧 R02 rootfs 猜测或另行拼接 `dial`、`start_prog`；它们来自同事
提供的新 ZIP。文件名中的 `OLD_FRAMEWORK` 只按源文件名记录，不对其业务含义作推断。

### 2.2 SDK

使用：

`/home/tronlong/lyp/SDK/EC200A/ql-ol-extsdk-ec200acntar03a02m2g_ocpu`

使用了 SDK 原始文件：

- `tools/fota/gen_obm_fota_image.sh`
- `tools/fota/gen_fota_image.sh`
- `tools/fota/quectel_obm_ota.blf`
- `tools/fota/quectel_AB_OTA.blf`
- `tools/fota/quectel_skylark_pm802_standard_AB.blf`
- `tools/fota/tim_builder`
- `tools/fota/mkotafbf`
- `tools/bin/setfiles`、`fakeroot`、`mksquashfs4`
- `tools/dm/prepare-dm-verity-create-script.sh`

没有手工修改 BLF。这里的事实是“按移远提供的 R03 SDK 原始流程成功生成”；它并不
等价于“BLF 对所有硬件批次都已真机证明兼容”。设备型号、Flash 布局和硬件版本仍应
在首台实验机核对。

## 3. OBM 三文件是什么

| 文件 | 本次值/大小 | 作用 |
|---|---:|---|
| `quectel_obm_ota.img` | 172,032 字节 | `mkotafbf` 生成的 OBM OTA 容器，包含目标 TLoader 和由 TIM 工具生成的相关启动数据 |
| `quectel_obm.md5` | `c96d2f1a72dcba67bae85661ee81e64a` | 目标 `TLoader_QSPINAND.bin` 的 MD5，启动脚本升级后用它读取 Flash 并复核 |
| `quectel_obm.size` | `112524` | 目标 `TLoader_QSPINAND.bin` 的真实长度，读取 Flash 后按此长度截断再算 MD5 |

三文件位于最终 rootfs：

```text
/boot/quectel_obm_ota.img
/boot/quectel_obm.md5
/boot/quectel_obm.size
```

主 FOTA 的顶层负载日志列出 8 类系统镜像，并不会把 OBM 显示成第 9 类顶层镜像。
OBM 三文件是先放进 `root.squashfs`；设备切到 R03 rootfs 启动时，
`/lib/preinit/83_upgrade_obm` 再调用 `/usr/bin/ql_ota_obm` 更新全局 OBM。

### OBM 自动升级脚本的真实分支

根据最终 rootfs 中的 `83_upgrade_obm`：

1. 三文件任意一个不存在，脚本退出，不升级。
2. `/NVM/.fota_obm_state` 为 `1` 时，脚本直接认为 OBM 已更新并退出。
3. 状态标志不是 `1` 时，先读取 `/dev/mtd0` 的 OBM 区，按 `quectel_obm.size`
   截断并计算 MD5；若已经与目标一致，只写状态标志，不重复升级。
4. rootfs 的 OTAD 版本必须不低于 rls846。本包 `/system/etc/mversion` 为
   `OW21.02_asr1803p401_rls988_1.057.067_20251120_13_59_bld1547`，满足脚本条件。
5. 需要升级时，脚本解锁 bootloader、调用 `ql_ota_obm`、`sync`、重新读回并校验
   MD5；失败最多尝试 3 次，成功后重新锁定并将状态写为 `1`。
6. 脚本明确警告：实际写 OBM 的约 200 ms 内断电或复位可能破坏 OBM并导致设备
   无法启动。

OBM 是全局启动区，不属于 A/B 两个 rootfs 槽。即使 AP 槽可以回滚，OBM 也不会随
A/B 槽自动恢复旧版本。因此，升级后回到 R02 槽是否兼容新 OBM，当前本地证据不能
证明，需移远确认并由实验机验证。

## 4. 实际制作流程

1. 在 `/tmp` 创建隔离工作目录；源 ZIP、源 SDK和设备均未被修改。
2. 解压新 ZIP，把其中 8 类 R03 系统镜像、`TLoader_QSPINAND.bin` 和
   `hotfix.bin` 复制到 SDK `tools/fota` 的临时副本。
3. 执行 SDK `gen_obm_fota_image.sh`：
   - `tim_builder -r quectel_skylark_pm802_standard_AB.blf`
   - `mkotafbf -f quectel_obm_ota.blf`
   - 生成 OBM OTA、TLoader MD5 和 TLoader size。
4. 解开源 `root.squashfs`，把 OBM 三文件放入 `/boot`。
5. 使用 SDK 原始参数执行 `setfiles` 和 `mksquashfs4`：XZ、64 KiB block、ARM
   filter、root-owned，并创建 `/dev/console`。
6. 使用 SDK `prepare-dm-verity-create-script.sh` 生成哈希树和启动元数据并追加到
   rootfs。
7. 用 `veritysetup verify` 验证新 rootfs，返回码为 0。
8. 用新 rootfs 覆盖临时 FOTA 输入，执行 SDK `gen_fota_image.sh` 完整模式。
9. `mkotafbf` 输出 `MakeFOTAImage for WEB successfully`，生成最终文件。

## 5. 打入 FOTA 的系统负载

`mkotafbf` 日志列出 8 个唯一负载。BLF 内分别定义 A/B 映射，FOTA API 将其写入
非激活槽；相同负载不会在日志中重复列成 16 份。

| 负载 | BLF A 槽地址 | 来源 |
|---|---:|---|
| RFPLUGIN.bin | `0x00140000` | 新 ZIP，逐字节一致 |
| MSA.bin | `0x00180000` | 新 ZIP，逐字节一致 |
| ARBEL.bin | `0x00580000` | 新 ZIP，逐字节一致 |
| u-boot.bin | `0x01040000` | 新 ZIP，逐字节一致 |
| zImage | `0x01100000` | 新 ZIP，逐字节一致 |
| root.squashfs | `0x01900000` | 新 ZIP rootfs + OBM 三文件 + 重建 dm-verity |
| tos.bin | `0x02c80000` | 新 ZIP，逐字节一致 |
| oem_data.squashfs | `0x02d00000` | 新 ZIP，逐字节一致 |

新 rootfs 为 17,805,605 字节；BLF 为 A、B 各分配 `0x01380000` =
20,447,232 字节，剩余 2,641,627 字节。

## 6. 已完成的离线验证

- 新 ZIP 中除 rootfs 外的 9 个使用输入（含 TLoader、hotfix）与临时 FOTA 输入逐字节
  `cmp` 一致。
- 新 rootfs 保留 `dial`、`start_prog`、OBM 脚本和 `ql_ota_obm`。
- 新 rootfs 中 OBM 三文件大小、MD5内容和源 TLoader 一致。
- 新 rootfs SELinux xattr 已由 SDK `setfiles` 重建。
- 新 rootfs 有 1 个设备节点（`/dev/console`），所有 UID/GID 为 root，与 SDK
  `-root-owned` 配置一致。
- dm-verity 根哈希验证通过。
- rootfs 未超过 BLF 分区长度。
- `gen_obm_fota_image.sh` 和 `gen_fota_image.sh` 均成功。
- FOTA 日志列出 8 个系统负载，成功标志 1 次，未发现 `error/failed/fail`。

这些验证只能证明输入来源、容器结构、大小、哈希和工具链执行结果，不能证明设备一定
能启动、射频一定正常或业务一定无回归。

## 7. 交付目录

```text
quectel_AB_OTA.img       最终给 test_absys 使用的 A/B FOTA
test_absys               移远 R03 FOTA 目录提供的 ARM 测试程序；与本机 R02A04 SDK 版本逐字节相同
root.squashfs            已加入 OBM 三文件并重建 dm-verity 的 rootfs
obm/                     OBM 三文件的独立留档
logs/                    OBM、rootfs、dm-verity 和 FOTA 构建日志
md5sums.txt              交付文件 MD5
sha256sums.txt           交付文件 SHA-256
MANIFEST.md              输入、输出及验证清单
TEST_ABSYS_GUIDE.md      真机操作、验收和异常分支
```

真机操作前必须完整阅读 `TEST_ABSYS_GUIDE.md`。
