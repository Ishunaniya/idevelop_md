# 升级包逐文件溯源清单（MANIFEST）

> 生成于 2026-06-30，来源用 md5 逐一比对判定，非人工记忆。
> 三类来源：**R03A02** = 来自 R03A02 升级固件（本次真正的更新）；**出货R02A04** = 设备现有底座，原样保留；**重生成** = 用 tim_builder 按新 CP 重算（hash 随 CP 更新）。

## 一、增量线刷集（增量_线刷_SWDownloader/）

| 文件 | 来源 | 改动 | 说明 |
|---|---|---|---|
| `ARBEL.bin` | **R03A02** | ✅更新 | CP 基带镜像，本次升级目标 |
| `DDR_ESMT_64M0xFFFF_DTim.PPsetting` | 出货R02A04 | 保留 | 底座原件 |
| `DDR_ESMT_64M0xFFFF_tim_falcon_qspinand.bin` | **重生成** | ♻️重算 | TIM/DTim，按新 R03A02 CP 重算 SHA-256 |
| `DDR_WINBOND_128M0xFFFF_DTim.PPsetting` | 出货R02A04 | 保留 | 底座原件 |
| `DDR_WINBOND_128M0xFFFF_tim_falcon_qspinand.bin` | **重生成** | ♻️重算 | TIM/DTim，按新 R03A02 CP 重算 SHA-256 |
| `DDR_ZMOS_64M0xFFFF_DTim.PPsetting` | 出货R02A04 | 保留 | 底座原件 |
| `DDR_ZMOS_64M0xFFFF_tim_falcon_qspinand.bin` | **重生成** | ♻️重算 | TIM/DTim，按新 R03A02 CP 重算 SHA-256 |
| `DTim.PPsetting` | 出货R02A04 | 保留 | 底座原件 |
| `DTim.Primary` | **重生成** | ♻️重算 | TIM/DTim，按新 R03A02 CP 重算 SHA-256 |
| `DTim.Recovery` | **重生成** | ♻️重算 | TIM/DTim，按新 R03A02 CP 重算 SHA-256 |
| `MSA.bin` | **R03A02** | ✅更新 | CP 基带镜像，本次升级目标 |
| `oem_data.ubi` | 出货R02A04 | 保留 | oem 数据分区 |
| `RFPLUGIN.bin` | **R03A02** | ✅更新 | CP 基带镜像，本次升级目标 |
| `root.squashfs` | 出货R02A04 | 保留 | **我方根文件系统(含 dial)**，不动 |
| `tim_falcon_qspinand.bin` | 出货R02A04 | 保留(重算后同) | 主 TIM，重生成结果与出厂逐字节相同(不含CP hash) |
| `TLoader_QSPINAND.bin` | 出货R02A04 | 保留 | OBM 二级引导，**未升级** |
| `TLoader_QSPINAND_ProductBuild.bin` | 出货R02A04 | 保留 | OBM 二级引导，**未升级** |
| `tos.bin` | 出货R02A04 | 保留 | TrustZone OS |
| `ubinize-oem.cfg` | 出货R02A04 | 保留 | 底座原件 |
| `u-boot.bin` | 出货R02A04 | 保留 | AP 引导 |
| `update.blf` | 出货R02A04 | 保留 | 分区/镜像表(线刷用) |
| `zImage` | 出货R02A04 | 保留 | AP 内核，版本不变 |

**小结**：增量集 22 个文件中——**3 个 CP 换成 R03A02**；**5 个 TIM/DTim 随新 CP 重生成**（DTim.Primary/Recovery + 3 个 DDR 变体 tim_falcon）；**其余 14 个（含我方 dial rootfs、内核、引导、OBM）全部保留 R02A04 出货原件、未改**。OBM(TLoader) **未升级**。

## 二、存量 FOTA 包（存量_FOTA_test_absys/quectel_AB_OTA.img）

单一 FBF 文件(≈36.5MB)，由 gen_fota_image.sh(full_fota) 打成，内含以下镜像（写入非激活槽）：

| 内含镜像 | 来源 | 改动 |
|---|---|---|
| ARBEL/MSA/RFPLUGIN | **R03A02** | ✅更新(CP) |
| u-boot/zImage/tos/oem_data | 出货R02A04 | 保留 |
| root.squashfs | 出货R02A04 | 保留(**含 dial**) |
| DTim.Primary(嵌入) | **重生成** | ♻️随新CP重算SHA-256 |

**不含 OBM**(TLoader/tim_falcon)：quectel_AB_OTA.blf 的 OTA 镜像表不列 OBM，故 FOTA 不更新 OBM。

## 三、外部来源路径（可追溯）
- 底座出货包：`md/firmware/image_ec200a_ytl_Vbox_HT_eletric_B0608014916_SY0007751CV_20260612/`（R02A04 + 我方 dial rootfs）
- R03A02 CP 源：`SDK/EC200A/ASR_CP_DUMP_FIRMWARE/EC200ACNTAR03A02M2G_OCPU/update/`（ARBEL/MSA/RFPLUGIN）
- blf：`SDK/EC200A/ql-ol-extsdk-ec200acntar02a04m2g_ocpu/tools/fota/`（quectel_AB_OTA.blf + standard_AB.blf，实测匹配真机分区）
- 打包工具：`SDK/EC200A/ql-ol-extsdk-ec200acntar03a02m2g_ocpu/tools/fota/`（tim_builder/mkotafbf，拷出使用，未改 SDK）
