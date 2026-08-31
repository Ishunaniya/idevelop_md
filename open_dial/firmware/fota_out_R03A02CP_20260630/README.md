# EC200A CP→R03A02 升级包（2026-06-30 打）

> **一句话**：底座=设备出货版 R02A04（含我方 dial 的 rootfs），**只把 3 个 CP 换成 R03A02**（ARBEL/MSA/RFPLUGIN），AP/内核/引导版本不变。
> **本包未经真机验证，务必先在能 USB 线刷救砖的实验设备上试，严禁现场首测。**

## 一、两个交付物

### 1. 存量设备（已在现网）→ FOTA，用 test_absys
- `存量_FOTA_test_absys/quectel_AB_OTA.img`（≈36.5MB，md5 见 md5sums.txt）
- 升级方式：`ql_abfota_start_update("quectel_AB_OTA.img")` → 轮询到 WRITEDONE → `ql_absys_switch()` → 重启 → 起来后 `ql_absys_sync()`（NEEDSYNC 时）。即 fota_out 里 test_absys/fota_update 那套流程。
- 全量 A/B FOTA：写非激活槽 → 切换 → 同步。**含 AP+CP，不含 OBM**（见风险 3）。

### 2. 增量设备（新生产）→ USB 线刷，用 SWDownloader
- `增量_线刷_SWDownloader/`（出货包全套 + 3 个 R03A02 CP，其余原样）
- 用 SWDownloader 加载该目录的 `update.blf` 线刷。分区布局未变，只有 CP 内容变新。

## 二、怎么打出来的（可复现）
1. 工作台 = R03A02 SDK 的 `tim_builder`/`mkotafbf`/`gen_fota_image.sh`（拷出，未改 SDK）。
2. blf = **R02A04 SDK** 的 `quectel_AB_OTA.blf` + `quectel_skylark_pm802_standard_AB.blf`（经实测与本设备真机分区一致：NandSize/映射表/偏移全同）。
3. 镜像 = 出货包 `image_..._20260612` 全套（**root.squashfs 含我方 dial**）。
4. 覆盖 3 个 CP 为 R03A02（`ASR_CP_DUMP_FIRMWARE/EC200ACNTAR03A02M2G_OCPU/update/`）。
5. `./gen_fota_image.sh`（full）→ `quectel_AB_OTA.img`。

## 三、⚠️ 风险与未决项（照这个逐条盯）
1. **基线组合 = CP R03A02(988) + AP R02A04(211)**。这正是移远"我本地成功了"配方，但也正是移远后来"**AP/CP 基线必须对应、不能只升 CP**"否定的组合（自相矛盾未澄清）。→ **升级前务必让移远书面确认这个产物有效、无隐患。**
2. **必须先在可 USB 救砖的实验设备上验证**：升级→重启→能起、能拨号、CP DUMP 是否真修好；确认没问题再考虑扩面。现场无救砖退路，绝不首测。
3. **本 FOTA 包不含 OBM**（`quectel_AB_OTA.blf` 无 TLoader/tim_falcon）。此前 cpimage-only 卡 OBM，疑因当时误用了 R03A02 新分区 blf（256MB/新映射）导致错位，本包已改用匹配真机的 R02A04 blf；但"OBM 到底需不需要同升"仍未定论，以实验设备实测结果为准。
4. **dm-verity**：本包 rootfs 直接用出货版（未改），其 verity 根哈希理应与设备一致；但 A/B FOTA 切到非激活槽时，该槽 cmdline 的 verity 根哈希如何生成/校验未验证，实验设备升级后需确认能过 verity（起不来常见于此）。
5. **增量线刷**：只换了 CP、保留出货 `update.blf` 与 tim_falcon（分区未变、新 CP 仍在 15MB cpimage 分区内）；SWDownloader 是否接受、要不要重生成 TIM，以线刷实测为准。

## 四、验证结论（2026-06-30 逐项核过）
**存量 FOTA 包（quectel_AB_OTA.img）—— 通过：**
- 8 种镜像全打入（ARBEL/MSA/RFPLUGIN/u-boot/zImage/root.squashfs/tos/oem_data），build 日志除已修 zImage 外无报错。
- 各镜像 Flash 地址与真机 `/proc/mtd` A 槽偏移逐个吻合（RFPLUGIN@0x140000、MSA@0x180000、ARBEL@0x580000、u-boot@0x1040000、zImage@0x1100000、rootfs@0x1900000、tos@0x2c80000、oem_data@0x2d00000）。
- 二进制取证：包内 CP 三件均为 **R03A02（HIT）、无 R02A04 残留（MISS）**；rootfs 命中我方 `root.squashfs` 中段块（含 dial）。
- **hash 一致**：`gen_fota_image.sh` 经 tim_builder 重生成 DTim.Primary（新 CP 的 SHA-256），并已嵌入 img（取证 HIT）。

**增量线刷集 —— 曾发现缺陷，已修复：**
- ⚠️ 初次组装只换了 CP 文件、**未重生成 TIM/DTim**，出货包 TIM 里仍是旧 R02A04 CP 的 SHA-256（update.blf 每镜像配 SHA-256 校验）→ 线刷后 hash 不匹配。
- ✅ 已在增量目录用 R03A02 tim_builder 按 `update.blf` **重生成全部 TIM/DTim（含 DTim.Primary/Recovery、3 个 DDR 变体）**，现与新 CP 一致。
- 正向佐证：重生成的主 `tim_falcon_qspinand.bin`（md5 `01936a4d…`）与出货包原件**逐字节相同**，证明该工具链忠实复现出厂 TIM 生成流程。
- ARBEL 增大（7.59M→7.72M）仍在 15MB cpimage 分区内（可用 11.27MB），放得下。

> 仍属**打包/结构层验证**；真机能否起、过 verity、CP DUMP 是否真修好，须实验设备实测（见风险须知）。

## 五、目录清单
- `存量_FOTA_test_absys/quectel_AB_OTA.img`
- `增量_线刷_SWDownloader/`（update.blf + 全套镜像，CP 已换 R03A02，TIM/DTim 已随新 CP 重生成）
- `md5sums.txt`

*底座出货包：image_ec200a_ytl_Vbox_HT_eletric_B0608014916_SY0007751CV_20260612；CP 源：EC200ACNTAR03A02M2G_OCPU/update；详见 md/session/session_fota_cpimage_packaging_20260623.md §19~§21。*
