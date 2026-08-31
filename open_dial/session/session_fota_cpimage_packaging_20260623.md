# EC200A CP 固件升级(ARBEL/MSA/RFPLUGIN)— 线刷 vs FOTA 自打包 完整记录

**日期**：2026-06-23
**目标**：把工作区三个 CP 固件 bin(`ARBEL.bin` / `MSA.bin` / `RFPLUGIN.bin`，R03A02，移远确认的"最小改动"升级件)升级到 EC200A 模组，解决 CP dump；现场设备需**远程升级**。
**一句话结论**：
- ✅ 能打出"只升 CP"的 FOTA `.img`(`gen_fota_image.sh cpimage`)，设备只刷 CP 分区、AP 不动，可远程。
- ❌ 但**不能只拿这 3 个 bin 打包** —— 打包器 `mkotafbf` 硬性要 `DTim.Primary`，必须用**全套 R03A02 固件文件**让 `tim_builder` 先建表。整包是"打包时的建表素材"，用完即丢，**不进包、不上设备**。
- ⚠️ 本机当前**没有** R03A02 的 `update/` 目录(只有 SDK 与 3 个散 bin)，需从原始线刷包/烧录电脑取回。

---

## 0. 升级背景(为什么升 R03A02)

- 现象:观测到 **CPIN 瞬断 / CP dump**。移远与 ASR 原厂确认这是**已知问题**。
- 修复:**EC200ACNTAR02A07M2G_OCPU 以上版本**含修改点可解决,移远建议替换 SDK 并升固件至 `EC200ACNTAR03A02M2G_OCPU`。
- R03A02 还内含若干 **FOTA 稳定性修复**(纪要列了 AUTO-171870 等 changelog 项)。
- 由此引出本次诉求:把 CP 三件套升到 R03A02,**现场设备需远程(FOTA)升级**。

---

## 1. 素材与出处

### 1.1 工作区三个 bin

| 文件 | 大小 | md5 | 性质 |
|---|---|---|---|
| `ARBEL.bin` | 7,719,389 B | `bb9fc75beab6642712dd0e2d07815d9c` | CP 主固件(ARM 向量头 `00 f0 20 e3`) |
| `MSA.bin` | 2,621,440 B | `b959fcedcda00a0e0bf4927c78cb123a` | CP 子系统 |
| `RFPLUGIN.bin` | 32,768 B | `b5c2630b18dd76a645915f5fc0a7ffd1` | RF 校准/插件 |

- 来源：**`EC200ACNTAR03A02M2G_OCPU` USB 线刷包**的 `update/` 目录，移远确认为升 R03A02 的最小改动件。
- 这 3 个文件**未被 git 跟踪**，工作区出现时间 2026-06-22 11:12。
- 大小/md5 与 R02A04 线刷包内同名文件不同 → 确为不同(更新)固件构建。
- bin 内**无明文版本串**(`strings` 抠不到 `R03A02`，固件压缩/加扰)→ "=R03A02" 靠出处确认，非字符串实测。

### 1.2 移远本次提供的三个文件(见 `session_sdk_fota_20260604.md`)

| 文件 | 角色 |
|---|---|
| `EC200ACNTAR03A02M2G_OCPU.zip` | **USB 线刷包**(非 FOTA 包)，3 个 bin 出处 |
| `ql-ol-extsdk-ec200acntar03a02m2g_ocpu.tar.gz` | QuecOpen **SDK**(头文件/库/sysroot/sdk.mk) |
| `ql-ec200a-1803e-gcc-8.4.0-v1-toolchain.tar.gz` | 交叉工具链(与 R02A04 同名同版本，**无需重装**) |

> **移远只给了 USB 线刷包，未给 FOTA `.img`** → 要远程升级，必须自打 `.img`，或继续向移远索取官方 `.img`+MD5。

---

## 2. 两条升级路线辨析

| 路线 | 文档 | 输入 | 操作 | 现场可用性 |
|---|---|---|---|---|
| **USB 线刷** | `EC200A_QuecOpen_固件烧录指导` | 整个 `update/` + `update.blf` | Windows + SWDownloader + 进下载模式，物理 USB | ❌ 车载现场无法接线刷，仅实验室/可上手设备、变砖救砖 |
| **FOTA/AB 升级** | `EC200A_ab_upgrade.md` | 单个 `quectel_AB_OTA.img` | SSH 远程 `ql_abfota_start_update()`，约 112s 自动重启，A/B 回滚保护 | ✅ 现场远程升级的唯一途径 |

关键点：
- `EC200A_ab_upgrade.md`(FOTA)**只吃 `.img`，不吃散 bin**。要远程升级必须先把 bin 打成 `.img`。
- 移远只给线刷包(`.zip`) → 现场远程升级当前**卡在"没有 FOTA 包"**，要么自打、要么催移远。

---

## 3. 方法 A：USB 线刷(实验室/救砖)

### 3.1 工具与驱动
- 工具：`SWDownloader.exe`(示例 4.9.1.3)，向移远技术支持索取。
- 驱动：先装 `Quectel_Windows_USB_Driver(A)_Customer`(双击 `setup.exe`)。

### 3.2 update.blf 镜像清单(ImgID ↔ 文件)
加载 `update/update.blf` 后，列表每行有勾选列(S.)，可选择性烧录：

| ImgID | ImgType | 文件 | CP/AP |
|---|---|---|---|
| TIMH | RAW | `tim_falcon_qspinand.bin` | 引导 |
| OBMI | RAW | `TLoader_QSPINAND.bin` | 引导(OBM) |
| **ARBI** | RAW | **`ARBEL.bin`** | **CP** |
| **GRBI** | RAW | **`MSA.bin`** | **CP** |
| **RFBI** | RAW | **`RFPLUGIN.bin`** | **CP** |
| OSLO | RAW | `u-boot.bin` | AP |
| ZIMG | RAW | `zImage` | AP |
| SYSJ | RAW | `root.squashfs` | AP |
| TZSI | RAW | `tos.bin` | 安全环境 |
| OEMD | RAW | `oem_data.ubi` | AP/用户 |
| TIM1/TIM4/TIM2 | DTIM | `DTim.Primary/PPsetting/Recovery` | TIM |
| ERAS | ERASO | NULL | 擦除项 |

### 3.3 "最小改动"= 只勾 3 行 CP
加载 blf 后**只勾 `ARBI`/`GRBI`/`RFBI`，其余全不勾** → 只刷 CP，AP(内核/根文件系统/dial/oem_data)不动。

### 3.4 烧录步骤
1. File → Open → 选 `update/update.blf`。
2. 只勾 ARBI/GRBI/RFBI。
3. 点左上角**绿灯**进入待烧录(左灯灰、右灯红)。
4. USB 连模组 → 按 reset/start 重启；或 USB_BOOT 短接 1.8V 进强制下载模式后上电。
5. 等 `Status: PASS` + 弹 `Elapsed time`(约 50s~1min) + 红灯变灰 = 成功。
6. 验证：`cat /etc/quectel-project-version`、`serial_atcmd at+qgmr`。

> 限制：**Windows PC + 物理 USB 操作**，无法远程；现场车载设备做不了。

---

## 4. 方法 B：FOTA/AB 远程升级机制(`EC200A_ab_upgrade.md`)

### 4.1 A/B 双系统与分区
Flash 双套(A/B)，同一时刻一套激活、一套非激活：升级写非激活分区→切换重启→同步另一分区。失败自动回滚，掉电不丢状态。
CP 镜像分区：`mtd2 cpimage-a` / `mtd14 cpimage-b`；状态存 `mtd19 asr_flag`。
> 注：ARBEL/MSA/RFPLUGIN 是 cpimage 内的子部件(各自有 Flash 地址)，**不能直接 `flashcp` 到某个 mtd**，必须经打包/线刷工具按地址写入。

### 4.2 升级状态机
`SUCCEED(0)` → `UPDATE` → `WRITEDONE` →(switch+重启)→ `NEEDSYNC` →(sync)→ `SUCCEED`；失败 `FAILED`。
枚举：`SUCCEED, UPDATE, BACKUP, FAILED, WRITEDONE, NEEDSYNC, UNKNOWN_STATUS`。

### 4.3 关键 API
- `ql-lib-fota`：`ql_abfota_start_update(path)`、`ql_abfota_get_update_status(&info)`。
- `ql-lib-absys`：`ql_absys_switch()`、`ql_absys_sync()`、`ql_absys_getstatus()`、`ql_absys_get_cur_active_part()`。

### 4.4 仓库现成部署工具 `fota_update`(v1.28.4，commit 74bb1ac 新增)
`fota_update.c`：`access` 检查包 → `ql_abfota_start_update` → 轮询到 `WRITEDONE/SUCCEED` → `sync()` → `reboot()`。默认包路径 `/media/sdcard/fota.img`。
操作要点(源码头注释)：
- 用 **SIGINT(非 SIGTERM)** 干净停 dial：`kill -SIGINT $(pgrep -x dial)`。
- 升级约 112s，写完自动重启；A/B 回滚防变砖，但断电仍有风险，保持供电。
- 升级期间 `start_prog` 约 15s 后会重拉 dial，不影响 FOTA 写入，勿手动干预。
- `ql_abfota_start_update` 返回非 0 = 包未写入，可重试。
Makefile 链接：`-lql_lib_fota -lubus -lubox -luci -lblobmsg_json -ljson-c -ljson_script -lrilutil -llog -lprop2uci -lmtel`。

> ⚠️ **重大更正(2026-06-24)：`fota_update` 流程不完整,勿直接用于正式升级。**
> 它只做 `start_update → reboot`，**缺少 `ql_absys_switch()`(reboot 前设切换标志)和 `ql_absys_sync()`(reboot 后同步)**。
> 据 `ql_absys_api.h` 原文："*Set flags in fota state file, need the caller do reboot after this api return 0, then A/B system will be switch after reboot.*"——切换标志由 `ql_absys_switch()` 设；不调它而直接 reboot,**很可能切回原系统、升级不生效**。
> **移远确认的正确做法**：打包用 R03A02 SDK，应用端用 **`test_absys`**(走完整 start→switch→reboot→sync）。详见 §17。

---

## 5. SDK 自带 FOTA 打包链(R03A02)

路径 `<R03A02 SDK>/tools/fota/`：
```
gen_fota_image.sh        # 打包脚本:full 与 single(uboot/zimage/rootfs/oemapp/cpimage)
gen_obm_fota_image.sh    # 打 OBM 包(R03 新增;R02→R03 需要)
mkotafbf                 # FOTA 包封装器(硬性依赖 DTim.Primary)
tim_builder              # 生成 TIM/DTim.Primary(需全套固件文件)
quectel_AB_OTA.blf       # FOTA 镜像清单(裁剪后只留 cpimage)
quectel_obm_ota.blf      # OBM 包清单(只引用 tim_falcon + TLoader)
quectel_skylark_pm802_standard_AB.blf  # tim_builder 输入,引用全部 18 个镜像
target/  Readme.txt  part_resizing.sh
```

### 5.1 single_fota 各分区镜像索引(裁剪规则)
| part_name | A 系统镜像号 | B 系统镜像号 |
|---|---|---|
| uboot | 4 | 12 |
| zimage | 5 | 13 |
| rootfs | 6 | 14 |
| oemapp | 8 | 16 |
| **cpimage** | **1/2/3** | **9/10/11** |

`single_fota cpimage` 把 `quectel_AB_OTA.blf` 裁剪到只剩 `1/2/3` 与 `9/10/11`：
```
1_Image_Path = ARBEL.bin   2_Image_Path = MSA.bin   3_Image_Path = RFPLUGIN.bin
9_Image_Path = ARBEL.bin  10_Image_Path = MSA.bin  11_Image_Path = RFPLUGIN.bin
```
→ 产出 `quectel_AB_OTA.img` **只含 CP 三件套**。

其它打包模式(同需全套文件在场):
- 全量包:`./gen_fota_image.sh`(无参)→ AP+CP 全换。
- 单分区:`uboot` / `zimage` / `rootfs` / `oemapp` 同 cpimage 用法。
- **OBM 包**:`./gen_obm_fota_image.sh` → 产出 `quectel_obm_ota.img` + `quectel_obm.md5` + `quectel_obm.size`(基于 `quectel_obm_ota.blf`,只引用 `tim_falcon_qspinand.bin` + `TLoader_QSPINAND.bin`)。**R02→R03 升级需要它**(见 §8)。

### 5.2 standard_AB.blf 全量 18 镜像(tim_builder 需全部在场)
A 系统 1-9：`tim_falcon_qspinand.bin, TLoader_QSPINAND.bin, ARBEL.bin, MSA.bin, RFPLUGIN.bin, u-boot.bin, zImage, root.squashfs, tos.bin`；
10：`oem_data.squashfs`；11-18：B 系统重复 ARBEL/MSA/RFPLUGIN/u-boot/zImage/root.squashfs/tos/oem_data。另 `HTFX_PATH = hotfix.bin`。

### 5.3 不能用旧 SDK(R02A04)打包
| 项 | R02A04 | R03A02 |
|---|---|---|
| `gen_fota_image.sh` | 262 B，**仅全量**(Readme: *only supports full firmware upgrades*) | 1469 B，含 cpimage 单升 |
| `gen_obm_fota_image.sh` | ❌ 无 | ✅ 有 |
| `tim_builder` | 4,092,048 B | 6,167,136 B |
| `mkotafbf` | 4,246,224 B | 6,305,120 B |
| `quectel_AB_OTA.blf` | 15,230 B | 15,682 B |

**打包工具必须跟"目标固件 R03A02"那套，不能用 74bb1ac 之前的 R02A04 SDK**——版本/分区布局对不上，会产出错误甚至变砖的包。打包版本跟"升到哪"走，不跟"设备现在是哪"走。

---

## 6. 为什么"只用 3 个 bin"打不了 —— 实测+根因

### 6.1 两次实测(均失败在同一处)
- ① 只放 3 个 bin 跑 `./gen_fota_image.sh cpimage` → `Open file DTim.Primary failed` → 无包。
- ② 跳过 tim_builder、复刻裁剪后直接跑 `mkotafbf` → 仍 `Open DTIM.primary failed....` → 无包。

### 6.2 根因(二进制 strings 实证)
```
mkotafbf 二进制硬编码：  /DTim.Primary   /DTim.Recovery   "Open DTIM.primary failed...."
tim_builder 里：         DTim.Primary  PRIMARYIMAGE      ← 它负责"生成"DTim.Primary
quectel_AB_OTA.blf 里：  无 DTim 路径   ← mkotafbf 按固定文件名在 cwd 找
```
因果链：
1. `mkotafbf`(打包器)**硬性要求 cwd 有 `DTim.Primary`**(TIM 分区映射/可信镜像表，设备靠它定位写入+校验签名)，与 blf 内容无关。
2. `DTim.Primary` **只能由 `tim_builder` 生成**。
3. `tim_builder` 生成它**必须读全套固件文件**(`standard_AB.blf` 引用全部 18 个镜像)。

→ 只有 3 个 bin → 生不出 `DTim.Primary` → `mkotafbf` 拒绝出包。**工具死规定，绕不过。**
→ 但"整包"只是建表素材，**最终 `.img` 仍只含 CP，推给设备也只刷 CP**。

---

## 7. 正确打包方法(自己照做)

### 7.1 前提：取得完整 R03A02 线刷包 `update/` 目录
即拆出 3 个 bin 的同一个包。**本机当前没有**(只有 SDK `tar.gz` + 3 个散 bin)，需从 Windows 烧录电脑/原始 zip 拷过来。

需要的全套文件(放进 `tools/fota/`)：
```
ARBEL.bin  MSA.bin  RFPLUGIN.bin       (已有)
tim_falcon_qspinand.bin  TLoader_QSPINAND.bin  u-boot.bin
zImage  root.squashfs  tos.bin  oem_data.squashfs  hotfix.bin   (待补)
```

### 7.2 oem_data 命名(本包已澄清，无冲突)
R02A04 线刷包里是 `oem_data.ubi`，曾担心与 FOTA `standard_AB.blf` 要的 `oem_data.squashfs` 对不上。
**实测：R03A02 线刷包(`ASR_CP_DUMP_FIRMWARE/.../update/`)直接就是 `oem_data.squashfs`，名字匹配，无需改名。** 另 `tim_falcon_qspinand.bin` 该包未提供，但 cpimage 打包**实测不需要它**，照样成功。

### 7.3 步骤
```bash
# 1. 复制打包目录(别污染 SDK 原件)
SDK=/home/tronlong/lyp/SDK/EC200A/ql-ol-extsdk-ec200acntar03a02m2g_ocpu
cp -a $SDK/tools/fota /tmp/fota_build && cd /tmp/fota_build

# 2. 拷全套固件(SRC 换成真实 update/ 路径;拷前先 ls 核对文件名,尤其 oem_data)
SRC=<R03A02线刷包update目录>
cp $SRC/tim_falcon_qspinand.bin $SRC/TLoader_QSPINAND.bin \
   $SRC/ARBEL.bin $SRC/MSA.bin $SRC/RFPLUGIN.bin \
   $SRC/u-boot.bin $SRC/zImage $SRC/root.squashfs \
   $SRC/tos.bin $SRC/oem_data.squashfs $SRC/hotfix.bin  .

# 3. 打包(只打 CP)
./gen_fota_image.sh cpimage

# 4. 看结果
ls -la quectel_AB_OTA.img        # 出现即成功;报某文件 failed = 缺文件/名字不对
```

### 7.4 实跑结果(2026-06-24，已验证)
- **固件源**：`/home/tronlong/lyp/SDK/EC200A/ASR_CP_DUMP_FIRMWARE/EC200ACNTAR03A02M2G_OCPU/update/`(`mversion = ...rls988_1.057.067_20251120`，证实 R03A02)。工作区 3 个 bin 与此 md5 一致。
- **CP 包**：`./gen_fota_image.sh cpimage` → `MakeFOTAImage for WEB successfully` → `quectel_AB_OTA.img` **10,420,224 B**(≈CP 三件套 10,373,597 B + 头，远小于含 AP 的 >20MB，确认只含 CP)。md5 `24350afb69033212967c874b2043269e`。
- **OBM 包**：`./gen_obm_fota_image.sh` → `quectel_obm_ota.img` **172,032 B**，md5 `9e94f2ace033943236077a9d0ae4e5ac`(源 TLoader md5 `c96d2f1a...`，size 112524)。
- **交付目录**：`ASR_CP_DUMP_FIRMWARE/EC200ACNTAR03A02M2G_OCPU/fota_out/`(`cpimage_R03A02_AB_OTA.img`、`quectel_obm_ota.img`、`md5sums.txt`)。

### 7.5 部署(用仓库现成的 fota_update)
```bash
scp quectel_AB_OTA.img root@<设备IP>:/media/sdcard/fota.img
ssh root@<设备IP> "kill -SIGINT \$(pgrep -x dial)"   # 干净停 dial
ssh root@<设备IP> /usr/dial/fota_update              # 写完自动重启,A/B 回滚保护
ssh root@<设备IP> "cat /etc/quectel-project-version && serial_atcmd at+qgmr"  # 验证
```

---

## 8. 升级路径方案与未决风险(勿跳过)

### 8.1 两个升级路径方案(纪要决策点，待与移远确认)
- **方案 A(分两次中转)**：`R02A04 ─FOTA→ R02A10 ─FOTA→ R03A02`。R02A10 作安全中转，规避基线跨度与"鸡蛋问题"。需移远提供**两个 FOTA 包**。
- **方案 B(直升 FullFOTA)**：`R02A04 ─FullFOTA→ R03A02`。一步到位，但跨度大、风险高。
- 当前自打的是**单 CP 包**(最小改动)，不等于上述任一全量路径；是否被移远认可、能否单独只升 CP 跨 R02→R03，**须书面确认**。

### 8.2 未决风险
1. **R02→R03 需连 OBM 一起升**：移远 changelog(见 `meeting_quectel_cp_dump_20260610.md` L79-83)明确：经 FOTA 从 R02 升 R03，**OBM 必须用 `gen_obm_fota_image.sh` 一并更新**(命令见 §5.1)。设备现为 R02A04，单 CP 包可能不够。OBM 包与 CP 包的**先后顺序、是否必须，须书面确认**。
2. **跨基线"鸡蛋问题"**：自打跨基线 FOTA 有基线跨度风险。团队原诉求是要移远官方 `.img`+MD5，自打等于把风险揽回；回滚保护对 OBM 这类底层件未必兜得住。
3. **ABI/部署顺序未决**：R03A02 SDK 头文件编译的 `dial` 能否运行在 R02A04 固件上(`libql_sdk.so` 版本不匹配是否有 ABI 风险)?这决定"先部署新 dial 再刷固件"的步骤顺序是否安全——**须先验证**。
4. **首测务必在能 USB 线刷救砖的实验室设备上**：变砖可线刷救回；现场车载无此退路，绝不可首测。
5. **最小风险中间方案待确认**：部分 AP 侧 fix 理论上只涉及 `libql_sdk.so`，能否单独推 `.so` 到 `/usr/lib/` 而不做完整 FOTA(纪要未决项)。

---

## 9. 本机磁盘盘点(消除混淆)

| 路径/文件 | 状态 | 说明 |
|---|---|---|
| `code/open_dial/{ARBEL,MSA,RFPLUGIN}.bin` | ✅ 在 | 工作区 3 个 R03A02 CP bin(未 git 跟踪) |
| R03A02 `update/` 目录 / 线刷 zip | ❌ **不在本机** | 打包必需，需取回 |
| `SDK/EC200A/ql-ol-extsdk-ec200acntar03a02m2g_ocpu/` | ✅ 在 | R03A02 **SDK**(含 tools/fota 打包链) |
| `SDK/EC200A/EC200ACNTAR03A02M2G_OCPU/*.tar.gz` | ✅ 在 | R03A02 SDK + 工具链归档(非固件) |
| `SDK/EC200A/EC200ACNTAR02A04M2G_OCPU.zip` | ✅ 在 | **旧 R02A04 线刷包**，用作 update/ 结构参考 |
| `SDK/AG35/.../images/ARBEL.bin`(8,865,610 B) | 旁证 | **AG35 模块**的 ARBEL，与 EC200A 无关，勿混 |
| `20260612-172941.png` | 干扰项 | 内容是 **AG35 频段表**，与本次升级无关 |

**R02A04 线刷包 `update/` 实样(12 文件，结构参考)**：
```
ARBEL.bin  MSA.bin  RFPLUGIN.bin  oem_data.ubi  root.squashfs
tim_falcon_qspinand.bin  TLoader_QSPINAND.bin  TLoader_QSPINAND_ProductBuild.bin
tos.bin  u-boot.bin  update.blf  zImage
```

---

## 10. 验证状态台账(诚实标注)

| 结论 | 依据 | 性质 |
|---|---|---|
| 3 个 bin 是 CP 固件 | `file`/`hexdump`(ARM 向量头) | ✅ 实测 |
| 3 个 bin 非 R02A04、无明文版本串 | md5/大小对比、`strings` | ✅ 实测 |
| cpimage 模式只打 CP 进包 | 读脚本裁剪逻辑 + blf | ✅ 实读 |
| 只放 3 个 bin → 打包失败(DTim.Primary) | 实跑两次(全脚本/跳 tim_builder) | ✅ 实测 |
| mkotafbf 硬依赖 DTim.Primary、tim_builder 生成它 | 二进制 strings | ✅ 实证 |
| R02A04 旧 SDK 仅全量、无 OBM | 读其 Readme + ls | ✅ 实测 |
| 本机无 R03A02 update/ 目录 | 全盘 find | ✅ 实测 |
| AG35 ARBEL / png 为干扰项 | 读文件/图片 | ✅ 实测 |
| 补齐全套后能成功出包 | **2026-06-24 实跑出 10.4MB 包** | ✅ 实测(已验) |
| OBM 包能打出 | **`gen_obm_fota_image.sh` 实跑出 172KB 包** | ✅ 实测(已验) |
| cpimage 包只含 CP(不含 AP) | 包 10.4MB≈CP 三件套，dump 日志只 3 件 | ✅ 实测(强证据) |
| oem_data 命名冲突 | **R03A02 包本就是 `.squashfs`，无冲突** | ✅ 实测(已排除) |
| tim_falcon_qspinand.bin 是否必需 | **缺它 cpimage 打包仍成功** | ✅ 实测(非必需) |
| ~~出的包刷上设备只动 CP 分区(待真机)~~ | 已真机验证,见下 | ✅ 已真机(2026-06-25) |
| cpimage-only 包能写入 + switch 成功 | 真机 test_absys:WRITEDONE→switch OK | ✅ 真机 |
| **但跨 R02→R03 重启崩溃(卡 OBM、CP crash)** | boot log:OBM 2022 旧版 + CR5 崩溃 dump | ✅ 真机(§18) |
| A/B 自动回滚到 A,设备未变砖 | 断电后 active 回 A、at+qgmr=R02A04 | ✅ 真机 |
| 回滚后残留 NEEDSYNC,须 sync 清掉 | 真机 menu 3 → SUCCEED,A/B 一致 | ✅ 真机 |
| 根因 = OBM 未更新(旧 OBM 带不动新 CP) | boot log 诊断 + 移远 AUTO-99501 要求吻合 | ⚠️ 诊断+文档(待移远认定) |
| "OBM 必须更新"原件 = 移远 R03A01 AUTO-99501 | 我方笔记转录;R03A01 原件本机无 | 📄 转录,原件待索取 |
| quectel_obm_ota.img 含 tim_falcon(@0x0)+TLoader(@0x20000) | obm blf + img 实查 | ✅ 实查 |
| **OBM/TLoader 是单份,无 A/B 回滚** | standard_AB.blf 的 B 列表无 TLoader;OBM blf 单份 | ✅ 实查(关键安全点) |
| OBM 与 CP 不可自行合包刷(高危,真变砖) | OBM 非 A/B 槽 + 单份无回滚 + 移远独立脚本 | ⚠️ 判断(强依据) |
| OBM 升级:打包有记录、应用/顺序/救砖无记录 | 全 md 实查 | 📄 开放项,待移远确认 |
| 打包工具 tim_builder/mkotafbf 是 x86-64 主机程序 | `file` 实测 | ✅ 实测 |
| 3 件套是 cpimage 分区内子镜像(地址 0x140000/0x180000/0x580000) | blf Flash_Entry_Address + dts 分区表 | ✅ 实读 |
| A/B 双分区出厂预置(cpimage-a/-b 各 15MB)，OTA 无需新增交换分区 | dts `reg=<0x140000 0xf00000>` 等 | ✅ 实读 |
| OTA 内存占用极小(总 98MB，升级中多用约 1.6MB) | `ab_upgrade.md §3.7.1` | 📄 文档实测值 |
| fota_update 是动态链接 ARM 程序，运行时依赖设备库 | `readelf -d` 实测 | ✅ 实测 |
| fota_update 只用 2 个 FOTA 函数，两版声明逐字一致 | `grep` 源码 + diff 头文件 | ✅ 实测 |
| fota_update 依赖的全部带版本 soname 两版完全相同 | 两版 SDK lib 对比 | ✅ 实测 |
| R03A02 编的 fota_update 能跑在 R02A04 设备 | 符号+soname 静态推定 | ⚠️ 待真机加载验证 |
| OTA 流程由设备运行固件的 FOTA 代理执行，与编译 SDK 无关 | API 为瘦触发器(ubus/IPC) | ✅ 实读 |
| ~~本版无 absys、不需手动 switch/sync~~ **作废(文件名搜错)** | 真名 `ql_absys_api.h`，两版都有 | ❌ 旧结论已更正 |
| 两版都有 `ql_absys_api.h` + `libql_lib_absys.so`(符号一致) | `ls`/`nm -D`/diff 实测 | ✅ 实测 |
| 完整升级须 start→**switch**→reboot→**sync** | `ql_absys_api.h` 注释 + `ab_upgrade.md §3.6.4` | ✅ 实读 |
| `fota_update` 缺 switch/sync → 不完整，应用端改用 test_absys | 读 fota_update.c + 移远确认 | ✅ 实读+移远 |
| 方案定调：R03A02 打包 + test_absys 应用，无风险 | 移远 2026-06-24 答复 | 📄 移远说法 |
| R02→R03 需更新 OBM 及与 CP 包顺序 | 移远 changelog(纪要引用) | 📄 文档说法，须向移远确认 |

---

# 附：打包原理与 OTA 机制深度问答(Q1~Q6，2026-06-24 补)

> 本部分把"这个 img 装了什么、怎么打的、跟 SDK/工具链什么关系、设备怎么 OTA、要什么条件、SDK 版本与 ABI"逐项讲透，全部基于实测/实读，推断项明确标注。

## §11 Q1：cpimage_R03A02_AB_OTA.img 只含 ARBEL/MSA/RFPLUGIN —— 两条铁证

(ASCII/hex grep 镜像 ID 不可靠——4 字节模式在 10MB 里会偶然命中、有字节对齐与字节序噪声，**不作证据**。可靠的是下面两条。)

1. **打包日志铁证**：`mkotafbf` 运行时只 dump 了 3 个 image header(RFPLUGIN→MSA→ARBEL)，随即 `Finish to dump to fbf file` / `MakeFOTAImage successfully`，**全程未处理 u-boot/zImage/root.squashfs/tos/oem_data**。
2. **体积账铁证**：`img = 10,420,224 B − CP三件套 10,373,597 B = 46,627 B`(仅够包头/TIM)。最小的 AP 件 tos=430,616 B、u-boot=480,359 B，rootfs 更是 17.5 MB，**都塞不进这点零头** → 只可能是 CP。

## §12 Q2：打包机制 + 工具路径 + 烧录地址 + 详细步骤

### 12.1 工具/文件精确路径(根：`<R03A02 SDK>/tools/fota/`)
| 文件 | 类型(`file` 实测) | 作用 |
|---|---|---|
| `gen_fota_image.sh` | Bash 脚本 | 编排：裁剪 blf → tim_builder → mkotafbf |
| `gen_obm_fota_image.sh` | Bash 脚本 | 打 OBM 包 |
| `tim_builder` | **ELF x86-64 静态链接** | 读 blf 生成 TIM/`DTim.Primary`(分区映射+签名) |
| `mkotafbf` | **ELF x86-64 静态链接** | 把镜像+TIM 封装成 `Marvell_FBF` 格式 .img |
| `quectel_AB_OTA.blf` | INI 配置 | FOTA 镜像清单(裁剪后只剩 cpimage) |
| `quectel_skylark_pm802_standard_AB.blf` | INI 配置 | tim_builder 输入，引用全 18 镜像 |
| `mversion` | 文本 | 版本串，作 mkotafbf 的 `-v` 参数 |

### 12.2 三件套烧录地址(证明是"一个分区里的三段子镜像")
```
3_Image_Path = RFPLUGIN.bin   Flash_Entry_Address = 0x00140000   Type = RAW
2_Image_Path = MSA.bin        Flash_Entry_Address = 0x00180000   Type = RAW
1_Image_Path = ARBEL.bin      Flash_Entry_Address = 0x00580000   Type = RAW
```
对照分区表 `cpimage-a: reg = <0x140000 0xf00000>`(起 0x140000、大小 15MB)：三个地址全落在 cpimage 分区内 → ARBEL/MSA/RFPLUGIN 是**同一 cpimage 分区里三段不同偏移的子镜像**(所以不能 `flashcp ARBEL.bin /dev/mtdX`)。

### 12.3 `gen_fota_image.sh cpimage` 内部逻辑(实读源码)
1. 备份原 blf 到 `blf_backup/`；
2. `sed` 插一行 `SkipTimBuilder = 1`；
3. **裁剪**：`image_a="^1_Image_\|^2_Image_\|^3_Image_"`、`image_b="^9_Image_\|^10_Image_\|^11_Image"`，把 blf 里除这 6 个镜像外的条目全删(`sed ...{/.../!d}`) → 只留 A 系统 1/2/3 + B 系统 9/10/11 = ARBEL/MSA/RFPLUGIN；
4. `./tim_builder -r quectel_skylark_pm802_standard_AB.blf` → 生成 `DTim.Primary` 等；
5. `./mkotafbf -f quectel_AB_OTA.blf -o quectel_AB_OTA.img -v $(cat mversion)`；
6. 还原 blf。

> 关键：tim_builder 用**未裁剪的 standard_AB.blf**(全 18 镜像)→ 建表要全套文件在场(="不能只给 3 个 bin"的根因，见 §6)；mkotafbf 用**裁剪后的 AB_OTA.blf**→ **最终包只含 3 个 CP 镜像**。

### 12.4 详细步骤(已实跑可复现)
```bash
# ① 复制打包目录(别污染 SDK 原件)
SDK=/home/tronlong/lyp/SDK/EC200A/ql-ol-extsdk-ec200acntar03a02m2g_ocpu
cp -a $SDK/tools/fota /tmp/fota_build && cd /tmp/fota_build
# ② 拷全套固件(建表素材)，源=R03A02 线刷包 update/
SRC=/home/tronlong/lyp/SDK/EC200A/ASR_CP_DUMP_FIRMWARE/EC200ACNTAR03A02M2G_OCPU/update
cp $SRC/{ARBEL.bin,MSA.bin,RFPLUGIN.bin,TLoader_QSPINAND.bin,u-boot.bin,zImage,root.squashfs,tos.bin,oem_data.squashfs,hotfix.bin} .
# ③ 打 CP 包 → quectel_AB_OTA.img (10.4MB, 只含CP)
./gen_fota_image.sh cpimage
# ④ 打 OBM 包 → quectel_obm_ota.img + .md5 + .size
./gen_obm_fota_image.sh
# ⑤ 校验
md5sum quectel_AB_OTA.img quectel_obm_ota.img
```

## §13 Q3：与 SDK 版本 / 交叉工具链的关系

| | 编译 `fota_update`(跑在设备上的程序) | 打 `.img`(固件包) |
|---|---|---|
| 用什么 | **交叉工具链** `arm-openwrt-linux-gcc` | **x86-64 主机工具** tim_builder/mkotafbf |
| 路径 | `/opt/ql_crosstools/ql-ec200a-1803e-gcc-8.4.0-v1-toolchain/bin`(sdk.mk L4-5) | `<SDK>/tools/fota/`(ELF x86-64) |
| 在哪定义 | 仓库 `Makefile` L58-59 `fota_update: ... $(CC)` | **不在 Makefile**，手动跑脚本 |
| 本质 | 把 C 源码编成 ARM 可执行 | 把移远预编译固件**重新封装**，不编译任何东西 |

**结论**：
- **打 `.img` 与交叉工具链无关**(主机工具只搬运/封装二进制)。
- **与 SDK 版本强相关**：blf 的 Flash 地址、分区布局、TIM/FBF 格式由 R03A02 定义，**必须匹配目标固件 R03A02**。打包跟"升到哪"走，不跟"设备现在是哪"走。

## §14 Q4 & Q6：客户侧 OTA 流程详解

> **重大更正(2026-06-24)**：本节原写"本 SDK 无 absys、不需手动 switch/sync"——**错误**。起因是我把头文件名搜成了 `ql_absys.h`，真名是 **`ql_absys_api.h`**。实查:R02A04 与 R03A02 **两版都有** `ql_absys_api.h` + `libql_lib_absys.so`(导出 `ql_absys_switch/sync/getstatus/get_cur_active_part`)。**完整升级必须手动 switch + sync**，详见下方正确时序与 §17。

依据：`ql_fota_api.h`、**`ql_absys_api.h`**、官方 `sample/absys/test_absys.c`、`ab_upgrade.md §3.6.4`、仓库 `fota_update.c`。

**正确完整时序(start → switch → reboot → sync)**：
```
【设备正常运行，跑在 A 系统】
①  把 .img 送上设备            scp cpimage_R03A02_AB_OTA.img → /media/sdcard/fota.img
②  ql_abfota_start_update(path)
      → 库把包内 3 个 CP 子镜像，按 blf 偏移写进【非激活】cpimage-b(0x4140000 起)
      → 流式写入约 112s；A 系统照常跑业务，不中断
      → 写完返回 0；get_update_status 查到 WRITEDONE
③  ql_absys_switch()           ← 关键!设"切换标志",状态转 NEEDSYNC
      ql_absys_api.h 原文:"need the caller do reboot after this api return 0,
      then A/B system will be switch after reboot"
④  reboot()                    重启后 bootloader 才真正切到 B 分区
⑤  【从 B 系统启动】= 新 CP 固件 R03A02 生效
⑥  ql_absys_sync()             新系统起来后,把另一分区(a)同步成与 b 一致 → SUCCEED
```
- **切换标志由 `ql_absys_switch()` 设,不是 `start_update` 设,也不是 reboot 自动设**。少了 ③ 而直接 reboot → 很可能切回原系统、升级不生效。
- 仓库 `fota_update` 只做 ②→④(缺 ③⑤⑥ 的 switch/sync)→ **不完整,勿直接用**(见 §4.4 警告)。**应用端改用 `test_absys`**(移远确认),详见 §17。
- **A/B 安全性**：写非激活分区，升级不停机；b 分区写坏 bootloader 校验失败会**自动回滚**留在 A；状态存 Flash，掉电不丢。

## §15 Q5：OTA 对设备的要求(含"要不要交换分区/内存")

### 15.1 核心澄清：不需要额外的"交换分区"，内存占用极小
分区调整文档 dts 实证：
```
cpimage-a:  reg = <0x140000  0xf00000>   ← 15MB，A 系统 CP
cpimage-b:  reg = <0x4140000 0xf00000>   ← 15MB，B 系统 CP
(u-boot/kernel/rootfs/tos/oem_data 也都各有 -a/-b 两份)
```
- **A/B 双分区是出厂就预置在 Flash 布局里的**，不是 OTA 时临时创建的"交换分区"。
- OTA 只是往**已存在**的非激活分区(cpimage-b)写，不开辟新空间；"双份"的 Flash 代价是**出厂一次性付掉**的。
- **RAM**：流式写入，`ab_upgrade.md §3.7.1` 实测——总 98MB，升级中仅多用约 1.6MB，无特殊内存要求。

### 15.2 完整要求清单(逐项带依据)
| 要求 | 数值/依据 | 满足 |
|---|---|---|
| Flash 已有 A/B 双分区 | dts 出厂布局，cpimage-a + cpimage-b 各 15MB | ✅ 平台自带 |
| 非激活分区放得下 CP | cpimage-b 15MB(0xf00000)> CP 内容 10.4MB | ✅ |
| 运行内存(RAM) | 总 98MB，升级中多用约 1.6MB | ✅ 占用极小 |
| 存 img 的可写空间 | img≈10.4MB，存 `/data`(rootfs_data 112MB)或 `/media/sdcard` | ✅ 需 ~10MB+ 空闲 |
| 固件支持 A/B + 有 `libql_lib_fota.so` | EC200A QuecOpen 本就 A/B；库随固件 | ✅ |
| 部署调 API 的程序 | 仓库 `fota_update`(或 App 内嵌 API) | 需部署 |
| 供电稳定 | 写入+重启≈2min；A/B 可回滚但断电仍有风险 | ⚠️ |
| 传输通道 | SSH/scp 或远程下发 | 需具备 |
| R02→R03 额外：OBM | changelog 要求连 OBM 升；顺序须移远确认 | ⚠️ 待确认 |

**一句话**：OTA **不需要额外交换分区或大内存**——双分区是 Flash 出厂自带、内存只多约 1.6MB；唯一要保证的是有 ~10MB 可写区存 img，且 cpimage-b(15MB)能装下 CP(10.4MB，已满足)。

## §16 SDK 版本与 ABI 辨析(打包 vs fota_update)

### 16.1 常见误解纠正
- **打包 `.img` 必须 R03A02，不能 R02A04**(见 §13、§5.3)。
- "设备现在跑 R02A04"与"用哪个 SDK 打包"是**两件独立的事**：打包决定固件内容/格式(跟目标版本走)；fota_update 是设备侧触发器(运行时依赖设备库)。

### 16.2 SDK 版本对 fota_update 的影响 —— 实测：基本无影响
`fota_update` 是**动态链接** ARM 程序(`file` 实测)。逐层验证：
1. **只调 2 个 FOTA 函数**(`grep` 源码)：`ql_abfota_start_update` + `ql_abfota_get_update_status`，未用 R03A02 新增的 `ql_fota_set_ota_delay`。
2. **这 2 个函数声明两版逐字一致**(diff 实测)；两版唯一差异是 R03A02 多了 `set_ota_delay`，fota_update 没用 → 无影响。
3. **依赖的全部带版本 soname 两版完全相同**(实测)。`readelf -d fota_update` 的完整 NEEDED 列表(共 13 项)：
   ```
   libql_lib_fota.so          libubus.so.20210630       libubox.so.20210516
   libuci.so                  libblobmsg_json.so.20210516  libjson-c.so.5
   libjson_script.so.20210516 librilutil.so             liblog.so
   libprop2uci.so             libmtel.so                libgcc_s.so.1   libc.so
   ```
   逐一比对 R02A04 与 R03A02 两版 SDK：带版本号的 soname(`libubus.so.20210630`、`libubox.so.20210516`、`libblobmsg_json.so.20210516`、`libjson_script.so.20210516`、`libjson-c.so.5.1.0`、`libql_lib_fota.so` 等)**一字不差**。
   解释器为 `/lib/ld-musl-armhf.so.1`(musl，32-bit ARM EABI5)。

→ **用 R03A02 编的 fota_update 拿到 R02A04 设备上也能正常加载、调用**(符号与 soname 都对得上)。SDK 版本对 fota_update 实质无影响。

> ⚠️ 此"低风险"只针对 **fota_update**(小、只碰 2 个跨版一致函数)。主程序 `dial` 链接 `libql_sdk.so`，面大得多，跨版 ABI 是另一码事(见 §8.2.3)，勿套用。

**实操验证建议(诚实标注：以上为 soname+签名静态推定，未真机加载验证)**：首次把 R03A02 编的 fota_update 放上 R02A04 设备时，先 `ldd /usr/dial/fota_update` 或直接执行一次，确认**不报 `symbol not found` / 库找不到**，再正式用于升级。

### 16.3 SDK 版本对 OTA 流程(§14)的影响 —— 不影响
真正执行"写非激活分区/切换/同步"动作的是**设备上正在跑的那套固件**(FOTA 守护进程 + bootloader)，应用程序(test_absys / fota_update)只是**通过 API 触发**这些动作，**不是编译它的 SDK 决定的**。
- 触发链:`ql_abfota_start_update` / `ql_absys_switch` / `ql_absys_sync` → 经 `libql_lib_fota.so` / `libql_lib_absys.so` 通过 ubus/IPC 通知设备 FOTA 守护进程干活。
- 切换/同步**需应用端显式调用 `ql_absys_switch()` / `ql_absys_sync()`**(见 §14 更正),不是平台全自动；`set_ota_delay` 仅调时延参数。
- OTA 实际行为取决于**设备当前固件版本**:R02A04 设备由 R02A04 守护进程执行(其自带 absys 库);升上 R03A02 后由 R03A02 守护进程执行。
- 接收方不关心包内是什么，只把包写进非激活分区;包是 R03A02 由打包阶段保证。

### 16.4 小结表
| 问题 | 答案 | 依据 |
|---|---|---|
| 打包用哪版 SDK | **必须 R03A02** | R02A04 无 cpimage/无 OBM/地址布局不符 |
| 打包跟 ABI/工具链有关吗 | 无关 | 主机工具只封装，不编译 |
| fota_update 用哪版 SDK 编 | R03A02 即可，R02A04 也行，**无区别** | 只用 2 个跨版一致函数 + soname 全同 |
| R03A02 编的 fota_update 能跑在 R02A04 设备吗 | **能** | 符号/soname 都对得上；⚠️ 未真机验 |
| SDK 版本影响 OTA 流程吗 | 不影响 | 流程由设备运行固件的 FOTA 代理执行 |
| R02A04 设备能收 R03A02 包吗 | 能 | R02A04 固件自带 AB FOTA 代理(SDK 有 libql_lib_fota) |

---

# §17 移远确认的最终方案 + test_absys 应用步骤(2026-06-24)

## 17.1 移远定调(原话归纳)
> "升级包制作逻辑都一样，可用新版本 SDK 包做升级包；和旧 SDK 包的 test_absys push 到模组升级；且新 SDK 打包的升级没有风险。"

拆成可执行结论：
- **打包**：用 **R03A02 SDK** 的 `gen_fota_image.sh cpimage`(已完成，产物 `cpimage_R03A02_AB_OTA.img`)。移远定调"无风险"。
- **应用**：用 **`test_absys`** push 到模组，走完整 start→switch→reboot→sync。
- **不要用裸 `fota_update`**(缺 switch/sync，见 §4.4、§14)。

## 17.2 旧 SDK 还是 R03A02 SDK 的 test_absys?
**都行，API 一致。** 实测：两版 `ql_absys_api.h` 函数声明逐字相同，`libql_lib_absys.so` 导出符号相同。移远建议旧 SDK(已知 good)；用 R03A02 编也等效。设备(R02A04 固件)自带 `libql_lib_absys.so`，test_absys 可正常运行。
> 编译：进 `<SDK>/sample/absys/` 跑 `make`(链接 `-lql_lib_absys -lql_lib_fota -lubus -lubox -luci …`)。

## 17.3 应用步骤(对应 ab_upgrade.md §3.6.4)
```bash
# 传包 + 工具
scp cpimage_R03A02_AB_OTA.img root@<IP>:/media/sdcard/fota.img
scp test_absys root@<IP>:/data/ ; ssh root@<IP> chmod +x /data/test_absys
# 干净停 dial
ssh root@<IP> "kill -SIGINT \$(pgrep -x dial)"
# 跑 test_absys，按菜单：
#   0 → 输入 /media/sdcard/fota.img   写非激活分区，等到 WRITEDONE
#   1 → 查状态确认 WRITEDONE
#   2 → ql_absys_switch（设切换标志）→ 模块自动重启切到新分区
#   —— 重启后再跑 test_absys ——
#   1 → 确认 NEEDSYNC
#   3 → ql_absys_sync（同步另一分区）→ SUCCEED
# 验证
ssh root@<IP> "cat /etc/quectel-project-version && serial_atcmd at+qgmr"
```

## 17.4 仍要把住的两点(闭环)
1. **"无风险"建议落到书面、且点明覆盖 R02→R03 + OBM**：移远自己 changelog 曾写"R02→R03 经 FOTA 需连 OBM 一起升"。请确认这句"无风险"是否已包含跨基线 + OBM(自动处理了还是不需要了),别用一句泛泛盖掉具体项。
2. **首测仍在能 USB 线刷救砖的实验室设备上**：流程对≠不用验,验通过再上现场车载。

## 17.5 fota_update 的处置(二选一)
- **方案 A(首测/实验室,跟移远)**：用 `test_absys` 人工走 0→1→2→重启→1→3,可控、零改码。
- **方案 B(现场全自动)**：改 `fota_update` 为完整流程。**已实现并编译通过**(2026-06-25),下面给完整代码、Makefile 改动、开机接线、完整流程、验证表。

### 17.5.1 已落地的源码改动(本仓库)
- `fota_update.c`：重写为完整 `start→switch→reboot→[重启后]sync` 状态机,新增 `#include "ql_absys_api.h"`,新增 `--sync` 收尾模式。
- `Makefile`：`FOTA_LDFLAGS` 加 `-lql_lib_absys`(放在 `-lql_lib_fota` 前)。
- 编译验证:`make fota_update` 通过;`readelf -d` 确认 NEEDED 含 `libql_lib_absys.so`+`libql_lib_fota.so`,符号 `ql_absys_switch/ql_absys_sync/ql_abfota_*` 均正常引入。

### 17.5.2 两种运行模式(关键设计:跨重启 + 防重刷循环)
完整时序跨一次重启,单次运行做不完(③reboot 后程序已退出,④sync 要在新系统里做),故拆两模式:
| 命令 | 模式 | 行为 |
|---|---|---|
| `fota_update <img>` | 升级 | 做 ①写入 ②设切换标志 ③reboot;若检测到已是 NEEDSYNC 则改为先做 sync(护栏) |
| `fota_update --sync` | 收尾 | 仅当状态 NEEDSYNC 时做 ④sync;否则空操作。**幂等,开机每次调用安全,绝不触发新升级** |
> 为什么要 `--sync` 而不是开机直接跑 `fota_update`:若开机无条件跑升级模式且 `/media/sdcard/fota.img` 还在,会**每次开机重刷** → 死循环。`--sync` 只收尾、不重刷。

### 17.5.3 完整 fota_update.c 代码
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include "ql_fota_api.h"
#include "ql_absys_api.h"

#define FOTA_DEFAULT_IMG  "/media/sdcard/fota.img"

/* 收尾: NEEDSYNC 时同步另一分区 → SUCCEED;非 NEEDSYNC 空操作。幂等。 */
static int do_sync_if_needed(void)
{
    update_info_t info;
    if (ql_abfota_get_update_status(&info) != 0) {
        fprintf(stderr, "get_update_status failed\n");
        return 1;
    }
    if (info.ota_state != NEEDSYNC) {
        fprintf(stdout, "no sync needed (state=%d)\n", (int)info.ota_state);
        return 0;
    }
    fprintf(stdout, "state=NEEDSYNC, syncing inactive partition...\n");
    fflush(stdout);
    int ret = ql_absys_sync();
    if (ret != 0) { fprintf(stderr, "ql_absys_sync failed: %d\n", ret); return 1; }
    fprintf(stdout, "AB sync succeed\n");
    return 0;
}

int main(int argc, char *argv[])
{
    update_info_t info;
    int ret;

    /* 收尾模式: 开机钩子调用,只做 sync,绝不触发新升级 */
    if (argc > 1 && strcmp(argv[1], "--sync") == 0)
        return do_sync_if_needed();

    const char *img_path = (argc > 1) ? argv[1] : FOTA_DEFAULT_IMG;

    /* 护栏: 已处 NEEDSYNC 则先收尾,不重复升级 */
    if (ql_abfota_get_update_status(&info) == 0 && info.ota_state == NEEDSYNC) {
        fprintf(stdout, "pending NEEDSYNC detected, finishing sync instead of re-flashing\n");
        return do_sync_if_needed();
    }

    if (access(img_path, F_OK) != 0) {
        fprintf(stderr, "FOTA package not found: %s\n", img_path);
        return 1;
    }

    /* ① 写非激活分区 */
    fprintf(stdout, "Starting FOTA update: %s\n", img_path); fflush(stdout);
    ret = ql_abfota_start_update(img_path);
    if (ret != 0) { fprintf(stderr, "ql_abfota_start_update failed: %d\n", ret); return 1; }

    /* ② 轮询到 WRITEDONE */
    while (1) {
        if (ql_abfota_get_update_status(&info) != 0) {
            fprintf(stderr, "get_update_status failed\n"); return 1;
        }
        fprintf(stdout, "progress=%3d%%  state=%d\n", info.percentage, (int)info.ota_state);
        fflush(stdout);
        if (info.ota_state == WRITEDONE) break;
        if (info.ota_state == FAILED) {
            fprintf(stderr, "FOTA FAILED (exit_code=%d)\n", info.exit_code); return 1;
        }
        sleep(3);
    }

    /* ③ 设切换标志(关键!没这步 reboot 会切回原系统) */
    ret = ql_absys_switch();
    if (ret != 0) {
        fprintf(stderr, "ql_absys_switch failed: %d (-2=updating/backup, -3=damaged)\n", ret);
        return 1;
    }
    fprintf(stdout, "switch flag set, rebooting to activate the new system...\n"); fflush(stdout);

    /* ④ 重启(切换重启后生效);重启后由 `fota_update --sync` 完成分区同步 */
    sync();              /* Linux 文件系统 flush,非 absys sync */
    sleep(1);
    reboot(RB_AUTOBOOT);
    return 0;
}
```

### 17.5.4 Makefile 改动
```diff
 FOTA_LDFLAGS := $(QL_SDK_FLAGS) \
 	-L$(QL_TOOLCHAIN_DIR)/../usr/lib \
 	-L$(QL_TOOLCHAIN_DIR)/../lib \
 	-L$(FOTA_SYSROOT)/lib \
 	-L$(FOTA_SYSROOT)/usr/lib \
-	-lql_lib_fota -lubus -lubox -luci \
+	-lql_lib_absys -lql_lib_fota -lubus -lubox -luci \
 	-lblobmsg_json -ljson-c -ljson_script \
 	-lrilutil -llog -lprop2uci -lmtel
```
(头文件 `-I$(FOTA_SYSROOT)/usr/include` 已含 `ql_absys_api.h`,不用动。)

### 17.5.5 "重启后自动 sync"接线(关键!不接则 A/B 不一致、丢回滚冗余)
③reboot 后必须有人再拉一次 `fota_update --sync`。`start_prog` 是设备上父进程(不在仓库),故用**开机脚本**最干净,不碰 dial。

**做法:在设备上加一个开机 oneshot 脚本** `/etc/init.d/fota_sync`(procd):
```sh
#!/bin/sh /etc/rc.common
START=98
boot() {
    /usr/dial/fota_update --sync >> /var/log/fota_sync.log 2>&1
}
```
启用:`chmod +x /etc/init.d/fota_sync && /etc/init.d/fota_sync enable`
> 它每次开机跑一次:NEEDSYNC 就同步、否则空退出。幂等,长期留着无副作用。

**替代做法(若不想加 init 脚本)**:把"开机若 NEEDSYNC 则 `ql_absys_sync()`"这段逻辑并入 dial 启动初始化(dial 每次开机都跑)。代价:dial 需链接 `-lql_lib_absys`,改动面更大,**勿轻易动主程序**,非必要不推荐。

### 17.5.6 方案 B 完整部署流程(无遗漏)
```bash
# 0)（本机已编好交付物）fota_out/: cpimage_R03A02_AB_OTA.img, fota_update, test_absys, quectel_obm_ota.img
# 1) 上传升级包 + 工具
scp fota_out/cpimage_R03A02_AB_OTA.img root@<IP>:/media/sdcard/fota.img
scp fota_out/fota_update              root@<IP>:/usr/dial/fota_update
ssh root@<IP> chmod +x /usr/dial/fota_update
# 2) 校验包 MD5(与本机 fota_out/md5sums.txt 比对)
ssh root@<IP> md5sum /media/sdcard/fota.img
# 3) 部署开机 sync 钩子(仅首次)
#    把上面 /etc/init.d/fota_sync 脚本放到设备并 enable
# 4) 干净停 dial
ssh root@<IP> "kill -SIGINT \$(pgrep -x dial)"
# 5) 触发升级(写入→设切换标志→自动重启,约 112s)
ssh root@<IP> /usr/dial/fota_update
# 6) 设备重启 → 开机钩子自动跑 fota_update --sync 收尾(或手动跑一次)
ssh root@<IP> /usr/dial/fota_update --sync
# 7) 验证(见 17.5.7)
```

### 17.5.7 升级后验证表(确认跑的是新 CP 固件)
> 注:ARBEL/MSA/RFPLUGIN 是**同一 cpimage 分区的三段子镜像**,不是三个独立分区;确认粒度到"CP 固件整体=R03A02 + 激活槽=B"。**标准手段无法逐个查单个 bin 版本**(它们对外只暴露 CP 固件整体版本),勿臆测逐 bin 校验。

| 查什么 | 命令/操作 | 期望 | 证明 |
|---|---|---|---|
| 激活槽 | test_absys 菜单 `4`(`ql_absys_get_cur_active_part`) | `Current active part is B`(原 A) | 切换成功 |
| 分区健康 | test_absys 菜单 `5`(`ql_absys_getstatus`) | `succeed`(is_damaged=0) | sync 完成、无损坏 |
| FOTA 状态 | test_absys 菜单 `1` / `fota_update --sync` 输出 | `SUCCEED` | 全周期走完 |
| **CP 固件版本** | `serial_atcmd at+qgmr`(AT+QGMR) | 返回 R03A02/rls988 版本串 | **跑的是新 CP 固件(三段整体)** |
| 工程版本 | `cat /etc/quectel-project-version` | 显示 R03A02 | 系统层面 |
| 最终目的 | 跑一段时间观察 | **CP dump 不再出现** | 功能层面真解决 |

---

# §18 真机实测结论(2026-06-25)：cpimage-only 跨 R02→R03 启动崩溃 + A/B 回滚

> **一句话**：在 R02A04 实验室设备上,用 test_absys 刷 `cpimage_R03A02_AB_OTA.img`(只升 CP),**写入 OK、switch OK,但重启后卡死在 OBM、CP 启动崩溃,A/B 自动回滚到 A**。根因:**只升 CP 没升 OBM**,旧 OBM 带不动新 CP。设备未变砖,sync 后完全回退干净。

## 18.1 实测过程与现象
设备升级前:`at+qgmr = EC200ACNTAR02A04M2G_OCPU`,active part = A。
用 test_absys 操作:`0`(写 /media/sdcard/fota.img,MD5 `24350afb...` 校验通过)→ 进度 100% `WRITEDONE` → `2`(switch)→ 自动重启。

**重启后卡死,串口日志关键行**:
```
procd killer:reboot,sig:15  caller:test_absys        ← switch 触发的重启
...
Load Image 0x4F424D49 : PASS                          ← 0x4F424D49 = ASCII "OBMI",加载 OBM
SWDownloader 4.9.1.5 for ASR1803
OBM 4.9.1.5 for ASR1803  -- Aug 31 2022 - 06:02:40 -- ← OBM 自报:仍是旧 R02A04 的 OBM(2022)
...
Init DDR: PASS / Flash Init Done                      ← OBM 跑完 DDR/Flash 初始化
（OBM 跑了第二遍,寄存器为崩溃现场值:
 CR5_PC_HD: 0x24026240  CA7_PC_HD: 0xdda832b0 ...）   ← CP(CR5)启动崩溃 → 复位 → 循环
卡在 "Flash Init Done" 不再前进
```

## 18.2 故障恢复 SOP(实测有效,设备未变砖)

> 适用:刷了 cpimage-only 包、`switch` 后重启卡死在 OBM(CP 崩溃循环)。
> 前提:本机制只对 **cpimage(CP)** 有效——CP 有 A/B 双份+回滚;**OBM 单份无回滚,写坏这套救不了**(见 §18.7)。

**第 1 步:断电回滚**
彻底断电、再上电(power-cycle)。A/B fail-safe 检测到 B 启动失败 → 自动回滚到 A 分区 → 设备从 A(原 R02A04)正常启动。
> 别一直中断它,让它自己重试/回滚;必要时多断电几次。

**第 2 步:确认已回到 A**
```
serial_atcmd at+qgmr            # 实测: EC200ACNTAR02A04M2G_OCPU
./test_absys → 输入 4           # 实测: Current active part is A
```

**第 3 步:查残留状态(回滚后会挂半截状态)**
```
./test_absys → 输入 5           # 实测: absys partition status : succeed   (B 未损坏)
            → 输入 1           # 实测: Current fota state : NEEDSYNC       (半截状态,需清)
```

**第 4 步:sync 收尾,清掉 NEEDSYNC**
```
./test_absys → 输入 3           # 实测: do AB sync succeed
```
方向 active(A=R02A04)→ 覆盖非激活 B;回滚后 active 是 A,所以是"用好的 A 覆盖坏的 B",**安全**。

**第 5 步:确认完全干净(五项全对)**
```
./test_absys → 输入 1           # 实测: Current fota state : SUCCEED
            → 输入 4           # 实测: Current active part is A
            → 输入 5           # 实测: succeed
            → 输入 -1          # 退出
serial_atcmd at+qgmr            # 实测: EC200ACNTAR02A04M2G_OCPU
```
→ 五项全对 = **完全回退干净**,A/B 两套都 R02A04、一致、无残留。

**⚠️ 千万别做**:回滚后**别再输 `2`(switch)**——会再切到坏的 B、再崩一次。

> 本次真机时间线(实测时间戳为证):switch 触发重启 → 卡 OBM 崩溃循环 → 断电 → 回滚 A → `getstatus=succeed` / `state=NEEDSYNC` → 输入 3 `do AB sync succeed` → `state=SUCCEED` / `active=A` → `at+qgmr=R02A04`。

## 18.3 根因:OBM 未更新
- 日志里 OBM 自报 **2022 年旧版**(`OBM 4.9.1.5 ... Aug 31 2022`)→ **OBM 没被更新**(我们只刷了 cpimage = ARBEL/MSA/RFPLUGIN,OBM 是单独的 `TLoader_QSPINAND.bin`)。
- OBM 与 CP 固件**强耦合配套**;旧 OBM(R02A04)去拉新 CP(R03A02)→ 接口/格式不匹配 → CP 启动崩溃。
- 这与移远文档"R02→R03 必须连 OBM 一起升"的要求**完全吻合**(见 §18.5)。
> 区分:"必须升 OBM"是**移远文档要求**;"**这次**崩溃=OBM 没升"是**我方现场诊断**(从 boot log 推),两者高度吻合但性质不同。

## 18.4 OBM 是什么
**OBM = ASR1803 芯片的二级引导程序**(BootROM 之后、第一个可更新的引导阶段),对应固件文件 **`TLoader_QSPINAND.bin`**(烧录清单镜像 ID = `OBMI`)。
启动链:`BootROM(芯片固化) → TIM(镜像映射表) → OBM → u-boot → kernel → Linux`,OBM 还负责**拉起 CP 协议栈**。
职责(boot log 可逐句对上):`Init DDR`(初始化内存)、`Flash Init Done`(初始化 Flash)、`SecurityInitialization`(安全校验)、加载并启动 CP(ARBEL 等)。
**OBM 包**就是 `quectel_obm_ota.img`(由 `gen_obm_fota_image.sh` 打,内含 `TLoader_QSPINAND.bin`),升级的就是它。

## 18.5 "OBM 必须更新"结论的出处链(含原件缺口,诚实标注)
- **一级出处(移远官方)**:移远 **R03A01 软件版本变更说明,条目 `AUTO-99501`** 原文:
  > *For modules updated from R02 to R03 via FOTA, the OBM of the module must be updated also by using newly added script file `gen_obm_fota_image.sh` and the `quectel_obm_ota.blf` file.*
- **我方记录(转录)**:`session_sdk_fota_20260604.md:382`(2026-06-04,最早记录,注明"AUTO-99501 / R03A01")+ `meeting_quectel_cp_dump_20260609/20260610.md`(列为"风险 1")。
- **⚠️ 原件缺口**:本机的 `quectel_sdk_changelog_R03A02.md` 是 **R03A02 相对 R03A01 的增量**说明,**不含此条**(grep "OBM" = 0)。AUTO-99501 属 **R03A01**,**那份原件本机没有** → 这句目前只能追到我方笔记转录,**移远 R03A01 changelog 原件需向移远索取核对**。

## 18.6 教训与下一步
1. **cpimage-only 跨 R02→R03 不可行**(实测坐实):必须连 OBM(甚至整套)一起升。
2. **A/B 回滚机制有效、设备不变砖**——但前提是**首测在能 USB 线刷救砖的设备上**(本次正是);现场车载无此退路,**绝不可拿现场首测**。
3. **下一步等移远书面确认**:OBM(`quectel_obm_ota.img`)与 CP 包的**确切顺序/分几次 switch**,还是必须**全量 FOTA**;若全量,**如何保留自研 dial 应用**(全量包 rootfs/oem_data 是移远 stock,会覆盖)。配方没拿到前**别再刷**。
4. 把本次 **boot 崩溃日志**发移远定位 + 索取 R03A01 changelog 原件。

## 18.7 OBM 包内容、单份无回滚、不可自行合包(2026-06-25 实查)

### 18.7.1 quectel_obm_ota.img 包含什么
格式 `Marvell_FBF`(同 cpimage 包),172 KB,含 **2 个镜像**(据 `quectel_obm_ota.blf` + img 实查):
| 镜像 | 文件 | Flash 地址 | 说明 |
|---|---|---|---|
| TIM | `tim_falcon_qspinand.bin` | `0x00000000` | 镜像映射表 |
| OBM | `TLoader_QSPINAND.bin` | `0x00020000` | 二级引导(就是它) |

### 18.7.2 ⚠️ 关键安全事实:OBM 是单份,无 A/B 回滚
Flash 布局(同一地址空间):
```
0x00000000  TIM            ┐ 单份!引导区,A/B 共用,无 B 备份
0x00020000  OBM(TLoader)   ┘
0x00100000  dtim-a         ┐
0x00140000  cpimage-a(CP)  ┤ A/B 分区区,各有 -a/-b 两份
0x04140000  cpimage-b(CP)  ┘
```
**铁证**:`standard_AB.blf` 的 B 系统镜像列表(11~18)里**只有 ARBEL/MSA/RFPLUGIN/u-boot/kernel/rootfs/tos/oem_data,没有 tim_falcon、没有 TLoader**;OBM blf 里 TLoader 也只有 1 份(无 B 副本)。
→ **OBM 是 A、B 共用的单份引导,刷它=直接覆盖唯一引导区,没有 A/B 回滚兜底。一旦写坏=真变砖**(上次救回设备的 A/B 回滚,对 OBM **不生效**)。

### 18.7.3 能不能把 OBM 和 CP 合成一个包一次刷?——不建议、危险
| 维度 | 事实 |
|---|---|
| mkotafbf 能否拼出含两者的 img | 技术上**能**(造合并 blf 即可) |
| 设备能否正确应用 | **存疑**:CP 走 A/B(写非激活槽→switch),OBM 在 0x0 引导区**不在 A/B 槽**,A/B FOTA 代理未必能正确写 OBM |
| 移远为何分开 | 专门给独立脚本 `gen_obm_fota_image.sh` + 独立 blf → 强烈暗示 OBM 是**另一套应用机制**,非塞进 cpimage FOTA |
| 风险 | OBM 单份无回滚 → 写坏=真变砖,**无退路** |

**结论:别自行合包刷。** 这不是"通不通"的问题,是"赌注=整机变砖且无救"的问题。

### 18.7.4 OBM 升级方法的记录程度(诚实台账)
| 环节 | 文档有无记录 | 依据 |
|---|---|---|
| **打包(怎么做 OBM 包)** | ✅ **有** | `gen_obm_fota_image.sh` + `quectel_obm_ota.blf` → `quectel_obm_ota.img`(已实跑产出,§5.1/§18.7.1) |
| **应用(怎么刷到设备)** | ❌ **无** | 全文通篇"待移远确认";`ab_upgrade.md`/烧录指导均无单刷 OBM 步骤 |
| **顺序(OBM 与 CP 谁先/能否合包)** | ❌ **无** | 开放项,§8.1/§17.4/§18.6 反复标"须移远书面确认" |
| **失败救砖(OBM 写坏怎么办)** | ❌ **无,且高危** | OBM 单份无 A/B 回滚,救砖手段未知 |
> 一句话:**OBM"怎么做包"有记录且已做好;"怎么刷、什么顺序、坏了怎么救"都没有,属待移远确认的开放项,且因单份无回滚而高危。**

### 18.7.5 给移远的精确问法(应用+顺序+救砖)
1. `quectel_obm_ota.img` 在设备上**具体怎么刷**?(也用 test_absys/`ql_abfota_start_update`,还是别的工具?)
2. OBM 单份无回滚,**刷 OBM 失败怎么救砖**?有无保护?
3. OBM 与 CP:**先刷哪个?能否合成一个包一次刷?中间重启安不安全?**
4. 是否干脆**走移远官方全量 FOTA 包**(内嵌 OBM 处理),避免我方自拼?(若全量,如何保留自研 dial 应用?)

---

## 19. 移远全量包配方 + 两版 blf 实测 diff(2026-06-26)

> 本章对应一台**首测设备**(可 USB 线刷救砖)上 cpimage-only 升级失败后,移远给出的"全量包配方",
> 以及我方对 R02A04 vs R03A02 两个 SDK 的 `tools/fota/*.blf` 做的**实测 diff**。
> **结论先行**:移远"新 SDK 调整过分区"一句**经 diff 实锤为事实**;cpimage-only 失败"分区错位"这条推断**现被证实有据**。

### 19.1 移远配方原文(转录,二手信息)
> "我本地做全量包升级成功了,就是把 **R02A04 的全部镜像**放到新 sdk 包 `tool/fota` 下,然后**替换新的三个 CP 镜像**,制作**全量升级包**;注意还需要把**旧 sdk 包** `tool/fota` 下的 `quectel_AB_OTA.blf` 和 `quectel_skylark_pm802_standard_AB.blf` 放到新的下面,因为**新 sdk 包有调整过分区**。"

拆成四个动作:
1. **底座** = R02A04 的全部镜像(OBM/TLoader/u-boot/kernel/rootfs/oem_data/cpimage… 整套)放进 R03A02 SDK 的 `tools/fota/`。
2. **只换 3 个 CP 镜像**为 R03A02 的(ARBEL/MSA/RFPLUGIN)。
3. 打**全量 FOTA 包**(非 cpimage-only)。
4. **关键**:blf 用**旧 R02A04 SDK 的**两个文件,**不要用 R03A02 SDK 自带的** —— 因 R03A02 SDK 分区被改过。

本质 = "**框架旧(R02A04 分区/底座) + 内容新(3 个 R03A02 CP bin)**"的混搭。

### 19.2 为什么全量包能成、cpimage-only 必败(事实印证)
- cpimage-only **不含 OBM/TLoader**(§18.7 已查),现场实测**卡死 OBM + CP crash + A/B 回滚**(§18)。
- R02→R03 跨版本 **OBM 必须同升**(R03A01 changelog AUTO-99501,§18.5)。
- 全量包**含 OBM/TLoader** → 新 CP 能被匹配引导拉起。
→ **OBM 缺失**是失败主因之一;全量包正面补上。

### 19.3 ⚠️ 关键新发现:两个 blf 实测 diff(纯读取,事实)
命令:`diff` R02A04 与 R03A02 两 SDK `tools/fota/` 下同名 blf。md5 即不同:

| 文件 | R02A04 md5 | R03A02 md5 |
|---|---|---|
| `quectel_skylark_pm802_standard_AB.blf` | `943b5d81fdd49b2795e0bd09070bdf1b` | `ef96baa2e89192638d8840968cfde7ac` |
| `quectel_AB_OTA.blf` | `de7b0d5c36e9d7992df7d96598b0c162` | `a9510d468596609725e27710f965fdb9` |

**分区层面差异(OTA.blf 与 standard.blf 一致出现,互相印证)——这就是"分区调整过"的硬证据:**

| # | 项 | R02A04 | R03A02 | 含义 |
|---|---|---|---|---|
| 1 | **NAND 容量声明** `1_NandSign_1_NandSize` | `0x08000000`(128MB) | `0x10000000`(**256MB**) | 最底层 flash 尺寸假设变了 |
| 2 | **镜像映射表** `NUM_MAPS` | `3` | `4`(新增) | R03A02 多挂一项 |
| 2b | 新增第 4 映射 | 无 | `4_Image_ID=0x54494D35`("**TIM5**"), `4_Flash_Address_Lo=0x04120000`, 类型 `PPSETINGIMAG_2` | 0x04120000 新挂 TIM5 |
| 3 | **擦除区** `Total_Eraseonly_Areas` | `1` | `2`(新增) | R03A02 多一擦除区 |
| 3b | 新增第 2 擦除区 | 无 | `@0x08100000`, size `0x00100000`(1MB) | 多擦 1MB |
| 4 | **oem_data 文件系统** | `oem_data.ubi` | `oem_data.squashfs` | 格式从 ubi 改为 squashfs |

**其他(非分区,但说明两版底层不同):**
- 新增 `HTFX`(hotfix)段 `Load_Address=0xD1004000`,`hotfix.bin`,`Patch_Size=0x36C`。
- `FAST`(=0x54534146)配置项被删;新增 `ATDL`(=0x4154444C)配置项,`ATDL_Cfg_Value=0x00000001`。
- 大量 **DDR 时序/电压 WRITE 序列**不同(Refresh timing `0x003000CB`→`0x004600CB`、Pre-Charge timing `0x06040205`→`0x06060205`、新增 DQ training/`0xC015xxxx` 写、BUCK1 电压序列等)——属 OBM/TIM 硬件 bring-up,两版 DDR 初始化不同。

### 19.4 结论(现为事实,不再是推断)
1. **移远没说错**:R03A02 SDK 的 blf 改了 NAND 尺寸(128→256MB)、映射表(多 TIM5@0x04120000)、擦除区(多 0x08100000)、oem_data 格式(ubi→squashfs)。
2. **§18 那次 cpimage-only 失败"分区错位"推断,现被证实有据**:那次用 **R03A02 的 blf** 打包,它按 **256MB/新映射** 写,设备实为 **R02A04 的 128MB/旧映射** → NAND 尺寸与映射表双重对不上,**叠加缺 OBM** = 双重错位。
3. **移远配方逻辑成立**:给 R02A04 设备升级,**分区框架必须沿用 R02A04 旧 blf**(匹配设备真实 128MB/旧映射/ubi),只把 3 个 CP bin 换成 R03A02 内容。

### 19.5 ⚠️ 全量包的副作用:会动 AP,可能冲掉自研 dial
全量包含 **rootfs/oem_data(AP 侧)**。若直接用移远 stock R02A04 rootfs/oem_data:
- 升级后 AP 被刷成移远原厂 → **dial 守护进程 / apn.json / /usr/dial 全部被覆盖**,量产不可接受。
- **应对(待确认)**:全量包的 rootfs/oem_data 须用**我方现网含 dial 的出货版**,而非移远 demo;须先确认手上有无可打包的这版 rootfs/oem_data。

### 19.6 ⚠️ diff 新冒出的矛盾点(观察,待验证解决)
旧 blf 里 oem_data 是 **`oem_data.ubi`**,但 R03A02 固件源目录里是 **`oem_data.squashfs`**。照配方用**旧 blf** 会去找 `.ubi`,而手上 R03A02 oem_data 是 squashfs → **格式可能对不上**。打包前须落实:全量包 oem_data 用哪份、什么格式。**此点尚未验证解决,先标出。**

### 19.7 与"当前卡死设备"的关系(勿混)
- 本章配方解决的是"**怎么正确做升级包**"(未来)。
- **另一台卡死设备**(§18 之后,USB 全量线刷 R02A04 仍卡 OBM)是另一码事,推断为 **ERAS(擦除项)未勾 → asr_flag(mtd19)残留坏 A/B 状态**(**推断,无 JTAG 实锤**)。须**先线刷救回(重刷且勾 ERAS,或找移远要带擦除恢复流程)**,救活前**不要碰任何 FOTA**。

### 19.8 落地前必须坐实的事实点(打包前)
1. **底座 rootfs/oem_data 用哪版**?移远 stock R02A04?还是我方现网含 dial 出货版?(决定会不会冲掉 dial)
2. **旧 blf 已确认存在**:`ql-ol-extsdk-ec200acntar02a04m2g_ocpu/tools/fota/` 下两 blf 实在(§19.3),已 diff。
3. **移远成功那台升级前是否也是 R02A04**?是→配方可照搬;否→分区前提不同。
4. **oem_data 格式矛盾**(§19.6)如何解。
5. 旧 blf 引用的镜像(尤其 `oem_data.ubi`)在 R02A04 SDK `tools/fota/` 是否齐全(下一步可纯读取核对)。 → **已核对,见 §19.9**

### 19.9 备料核对:R02A04 底座镜像本机是否齐全(2026-06-26 纯读取实查)

> 对应 §19.8 第 5 点。结论:**移远配方卡在第一步"把 R02A04 全部镜像放进去"——本机无这套底座镜像,缺料,打不了。**

**(1) R02A04 旧 blf 引用的镜像清单(事实,grep `_Image_Path`)**
- `standard_AB.blf` 18 项 / `AB_OTA.blf` 16 项,去重后共 10 种文件:
  `tim_falcon_qspinand.bin`、`TLoader_QSPINAND.bin`、`ARBEL.bin`、`MSA.bin`、`RFPLUGIN.bin`、`u-boot.bin`、`zImage`、`root.squashfs`、`tos.bin`、`oem_data.ubi`。
- 注意 R02A04 blf 用 **`oem_data.ubi`**(对比 R03A02 blf 用 `oem_data.squashfs`,§19.3)。

**(2) ⚠️ 这些镜像本机一个都没有(事实)**
| 查找位置 | 结果 |
|---|---|
| R02A04 `tools/fota/` | **零镜像**:只有 `gen_fota_image.sh`、`mkotafbf`、`tim_builder`、两 blf、`mversion`、`Readme.txt` |
| R02A04 SDK 顶层 | 只有**源码**:`ql-ol-rootfs.tar.gz`、`ql-ol-kernel`、`ql-ol-bootloader`、`ql-sysroots`、`sample`、`tools`;**无构建好的镜像** |
| 全盘 `find oem_data.ubi` | **空** |
| 全盘 `find tim_falcon_qspinand.bin` | **空**(两版固件源都无,见(4)) |
| 全盘 `find root.squashfs / ARBEL.bin`(R02A04 版) | R02A04 SDK 内**空**;唯一存在的是 R03A02 固件源里的 R03A02 版 |
| `ASR_CP_DUMP_FIRMWARE/` | **只有 `EC200ACNTAR03A02M2G_OCPU/` 一个目录,无 R02A04 目录** |

**(3) 本机唯一整套镜像 = R03A02 固件源(squashfs/新格式)**
`ASR_CP_DUMP_FIRMWARE/EC200ACNTAR03A02M2G_OCPU/update/` 实有:
`ARBEL.bin`(7,719,389)、`MSA.bin`(2,621,440)、`RFPLUGIN.bin`(32,768) ← **配方要换进去的 3 个 CP**;
另有 `TLoader_QSPINAND.bin`、`tos.bin`、`u-boot.bin`、`zImage`、`root.squashfs`、`oem_data.squashfs`(311B)、`hotfix.bin`、`update.blf`。
→ 这套是 **R03A02 新格式**,**不能**当 R02A04 底座(分区/格式不匹配,§19.3)。

**(4) 顺带澄清(事实)**
- `tim_falcon_qspinand.bin` 两版固件源都没有 → 它是 **`tim_builder` 按 blf 现场生成的产物**,不随包发,"缺"属正常,打包时工具会生成。
- **§19.6 的 oem_data 矛盾解开**:只要按配方用**整套 R02A04 底座**,oem_data 即 R02A04 的 `.ubi`,与 R02A04 旧 blf 一致 → **不冲突**;R03A02 的 `oem_data.squashfs` 不参与(只取其 3 个 CP bin)。前提是**拿到整套 R02A04 底座**。

**(5) 结论与缺口(诚实)**
- **缺口**:配方第一步要的 **R02A04 全套底座镜像**(`oem_data.ubi`/`root.squashfs`/`u-boot.bin`/`zImage`/`tos.bin`/`TLoader_QSPINAND.bin`/旧 ARBEL·MSA·RFPLUGIN)**本机全无**。它们在**移远 R02A04 线刷包**里,该包在 Windows + SWDownloader 那侧(救砖用),**不在本台 Linux**。
- **所以**:全量包在本机**当前打不了**,须先**取得移远 R02A04 线刷包(或其全套镜像)**拷到 R03A02 SDK `tools/fota/`,再换 3 个 R03A02 CP bin,用**旧 blf** 打包。
- **另一条更稳的路**(待定):直接**向移远索取他们本地打成功的那个全量 FOTA 包**,或索取**带 dial 的我方出货版底座**——避免我方自拼底座、也回避 §19.5 的 rootfs 覆盖 dial 问题。

---

## 20. 移远再对齐:单包作废、AP/CP 基线必须对应、换 CP 路线被否(2026-06-26)

> 本章记录移远在收到我方 cpimage-only 实测失败反馈后,**内部再次对齐**给出的最新口径。
> **它推翻了 §17/§19 移远自己先前给的"旧底座 + 换 3 个 CP"配方。** 信息为**移远口述(二手)**,矛盾点为**客观事实**,影响为**我方推断**,已分别标注。

### 20.1 移远最新口径(转录,二手信息)
> "第一,客户的版本**不支持单包升级,只能全量升级**,只能制作全量包,大小约 **30M**。
> 第二,内部讨论对齐:**不能只升级 CP 镜像、其他 AP 镜像不升级**。现在 **kernel/uboot 等都是 211 基线,而 CP 已是 988 基线**,横跨很多基线、改动过多,要求 **AP 和 CP 版本一一对应**;而 **211 基线 ASR 已不维护、出不了 patch**,所以要解决此问题**仍需做大改动**。不能只升级 CP——已跨很多基线,CP 要与 AP 版本对应。"

### 20.2 两点拆解
1. **单包(cpimage-only)作废**:客户版本不支持,只能全量包(~30M)。与 §18 实测(cpimage-only 卡 OBM)一致 → 官方确认单包路线作废。
2. **不能只升 CP**(全新、关键):AP(211 基线)与 CP(988 基线)跨基线太多,移远要求 AP/CP 版本一一对应;211 基线 ASR 已停维护出不了 patch,真正解决需 AP+CP 整体大改动。

### 20.3 ⚠️ 推翻了移远自己先前的配方(事实层面的矛盾)
- §17/§19 移远先前配方 = "**R02A04 全套底座(AP 留 211 基线) + 只换 3 个 R03A02 CP bin(CP 988 基线)**",本质就是 **AP 211 + CP 988 混搭**。
- 移远本次新口径"**AP/CP 基线必须对应、不能只升 CP**"**恰好否定了该配方**。
- **客观矛盾**:移远先前还说过"**我本地做全量包升级成功了**"(§19.1),那个成功包正是 AP 211 + CP 988 混搭。**"成功" 与 "基线必须对应、不能只升 CP" 两段移远原话直接冲突。**
- **待澄清(我方推断两种可能)**:① 那次"成功"仅"能启动/不卡 OBM",未做完整业务验证、隐患未暴露;或 ② 内部重评后认为该路有隐患、不可投产。**须移远统一口径。**

### 20.4 对我方项目的冲击(我方推断)
- **换 3 个 CP 路线被移远自判不成立** → 问题从"打个包升级"升级为"**整机软件基线从 211 迁到 988**",属需求/排期级决策。
- **dial 运行环境**:AP 整体升级(uboot/kernel/rootfs 全换 988 对应版)→ dial 的内核/根文件系统全变,移植/回归量远超"保留 dial"一层。
- **维护风险**:211 基线 ASR 停维护、出不了 patch,该升级链路本身有长期维护风险。

### 20.5 我方立场与对移远的反驳要点(2026-06-26)
**我方目前不接受**任何会把风险转嫁到现网的方案。移远提议"**先本地测好升级,再现场升级几台设备运行测试以降低风险**",我方反驳依据:
1. **现场无救砖退路**:现场(车载/远程)无 USB 线刷 + SWDownloader 条件;cpimage-only 已实测会卡 OBM,一旦失败=现场变砖,需人到现场甚至返厂。"试几台"是把**实验室可救的砖,变成现场不可救的砖**。
2. **OBM 单份无 A/B 回滚**(§18.7 已证):刷 OBM 失败无兜底,现场无救砖手段=真变砖。
3. **小样本不能证明安全**:本地通过 ≠ 现场通过;现场有信号/电源/温度/断电时机等变量,FOTA 中途断电高危。"几台"统计上不证明安全,但每台失败都是实打实现网损失。
4. **根因是设计层不匹配,测试消不掉**:移远自述 AP(211)/CP(988) 跨基线、必须对应、211 无维护——这是**架构不匹配**,多测几台只能发现问题、不能修复基线不匹配。
5. **成本/责任不对等**:根因(基线不匹配+无维护+无救砖)未解决前,不应把验证成本转嫁现网设备;现场失败的返厂/停运成本归属未定。
> **结论立场**:三条(根因未解决、现场无救砖退路、OBM 单份无回滚)同时存在时,"现场试升几台"不是降风险,而是把可控的实验室风险转为不可控的现网变砖风险。**要求:先在实验室把方案做到稳定可回滚、且具备现场救砖手段后,再谈现场。**

### 20.5a 移远坚持 AP+CP 同升 → "≈ 整机系统重做" 反驳(2026-06-30)

我方发出 20.5 反驳后,**移远回复:坚持 AP 和 CP 必须同时更新**。我方再反驳,落点 = **拒绝整机基线迁移**:

**(1) AP+CP 同升 ≈ 重新做一版系统固件吗?(诚实定性)**
- **不等于"从零重新开发功能"**:dial 业务逻辑/源码都在、不重写;硬件不变(EC200A/ASR1803);软件架构不推翻。
- **但量级接近"整机软件平台迁移 + 全量重新集成与回归"**:AP 三大件 kernel/uboot/rootfs **全换基线**(211→988);交叉编译工具链、内核版本、系统库、驱动/接口可能都变 → dial 要**重新移植/重编/很可能改代码适配**;所有外设与业务**全量回归**;出货镜像/量产固件**重建 + 重新验证固化**。
- **准确说法(对外少被挑用词)**:这是一次**整机软件平台迁移、重出一版系统固件**,不是一次固件升级。为修一个 CP 问题付出此量级代价,**不成比例**。

**(2) ⚠️ 对"客户侧存量设备"更不可接受(立场里最硬一条)**
| # | 理由 |
|---|---|
| 1 | **现场无救砖退路**:存量设备已在现网(远程/车载),无 USB 线刷条件;整机刷 AP+CP + 动单份无回滚 OBM,失败即不可恢复变砖(产线没此风险,存量设备致命) |
| 2 | **量级被放大**:不是改一台,是现网**全部存量设备**各承受一次"整机平台迁移",基数越大出事绝对数越高 |
| 3 | **代价/收益彻底倒挂**:设备本在正常运行,为修一个 CP 偶发 DUMP,要每台整机搬平台 + 全量回归 = "重出一版系统"量级,只换来修一个 CP bug |
| 4 | **211 已停维护**:迁过去也无 ASR patch 保障,等于把存量设备推到无长期维护的平台,风险单向增加 |
> **结论**:产线新设备做基线迁移尚算一次性投入;**现网存量设备**做整机迁移——无救砖、整机刷 + 单份 OBM 失败即不可恢复,等于让每台在运设备赌一次变砖,只为修一个 CP 偶发问题,**代价收益完全倒挂,对存量设备更不可接受**。
>
> **我方核心诉求(对外话术)**:这是 **CP 侧问题**,请移远在**不动 AP 的前提下**从 CP 侧或以兼容方式解决,而非把一个 CP bug 转嫁成客户的**整机系统重做**。

### 20.6 待移远澄清/提供
1. 先前"全量包成功"那台**是否做过完整业务验证**?与"必须 AP/CP 对应"如何统一口径?
2. 若必须 AP+CP 整体升 988,移远能否提供**完整对应基线的全量包(含 AP)**?
3. 整机升级后**我方 dial 如何承接**(移远是否支持客制 rootfs/保留应用)?
4. 现场升级若失败,**救砖方案**是什么(尤其 OBM 单份无回滚)?

---

## 21. 真机分区/rootfs 状态实测 + 出货镜像包分析 + 旧结论订正(2026-06-30)

> 本章两件事:① **在设备控制台实测**当前 rootfs 格式、分区表、A/B 槽、版本(留作**升级前基线**,升 AP/CP 后回来逐项对比);
> ② 分析 `md/firmware/image_ec200a_ytl_Vbox_HT_eletric_B0608014916_SY0007751CV_20260612/`(设备出货镜像包)并验证同事结论。
> **顺带订正 §19.3/§19.4/§19.9 的若干推断错误，及 CLAUDE.md 的架构口径错误。**

### 21.1 升级前设备状态基线(2026-06-30 实测,升级后照此对比)
设备:`AT+QGMR = EC200ACNTAR02A04M2G_OCPU`,`uname = Linux 5.4.195 armv7l`,`/tmp/dial_version = 1.28.10`。

**(a) 根文件系统 = squashfs(只读) + dm-verity,当前 A 槽**
```
# cat /proc/mounts   (关键行)
/dev/dm-0 / squashfs ro,seclabel,relatime 0 0
/dev/ubiblock1_0 /NVM/oem_data squashfs ro,seclabel,relatime 0 0
ubi0:data /data ubifs ...   ubi0:etc /overlay/etc ubifs ...   ubi0:nvm /overlay/nvm ubifs ...
overlayfs:/data/usr /usr overlay ... lowerdir=/usr,upperdir=/data/usr/root ...   ← /usr 是可写 overlay(dial 可经此更新)
/dev/mmcblk0p1 /media/sdcard ext4 ...

# cat /proc/cmdline
rootfstype=squashfs ... root=/dev/dm-0 system=a
  dm-mod.create="root,,,ro,0 36912 verity 1 /dev/mtdblock11 /dev/mtdblock11 4096 4096 4614 4615 sha256
  4a71ba93e7ca5b623d4a85a55f6d03489069aa6224844aad402739fa25ca85ae
  e587e287954cb360432fe5a4ed94929fb3c78ef65bf14feae47d109b631275c7"
  mem=128M@0x0 cpmem=24M@0x6800000 tosmem=4M@0x2000000 FAST=47574c33
```
- 判据:根=squashfs 只读;**dm-verity 完整性校验**,后端 `/dev/mtdblock11`=**rootfs-a**;`system=a` → **当前激活 A 槽**;
  verity **根哈希 `4a71ba93…`**(升级/换 rootfs 后此哈希必变,是最灵敏的对比锚点)。
- **含义**:换 rootfs 不能只刷分区——verity 根哈希烧在 cmdline,rootfs 一动就要重算哈希并改启动参数,否则校验失败起不来。

**(b) 分区表 = 26 个 MTD,完整 A/B 双槽,NAND 总量 256MB**
```
# cat /proc/mtd
mtd0  00040000 "bootloader"          mtd13 01400000 "oem_data-a"
mtd1  00020000 "cp_reliabledata"     mtd14 00040000 "dtim-b"
mtd2  00020000 "ap_reliabledata"     mtd15 00f00000 "cpimage-b"
mtd3  00020000 "mep-ota"             mtd16 000c0000 "u-boot-b"
mtd4  00020000 "cp_reliabledata_backup"  mtd17 00800000 "kernel-b"
mtd5  00020000 "ap_reliabledata_backup"  mtd18 01380000 "rootfs-b"
mtd6  00020000 "mep-ota_backup"      mtd19 00080000 "tos-b"
mtd7  00040000 "dtim-a"              mtd20 01400000 "oem_data-b"
mtd8  00f00000 "cpimage-a"  (15MB)   mtd21 00100000 "asr_flag"
mtd9  000c0000 "u-boot-a"            mtd22 00040000 "misc"
mtd10 00800000 "kernel-a"  (8MB)     mtd23 00040000 "cust_info"
mtd11 01380000 "rootfs-a"  (19.5MB)  mtd24 07060000 "rootfs_data" (112MB)
mtd12 00080000 "tos-a"               mtd25 00d20000 "QUEC_BBM"
```
- **NAND 总量**:`/proc/partitions` 全部 mtdblock 累加 = **262144 KB = 256MB**(实测,非 blf 声明)。
- **A/B 对**:dtim/cpimage/u-boot/kernel/rootfs/tos/oem_data 各有 -a/-b 两份;引导 `bootloader`(mtd0)单份、无 A/B;`asr_flag`(mtd21)存 A/B 切换状态。

**(c) UBI 卷(可写数据区与 oem_data)**
```
# ubinfo -a   (摘要)
ubi0 (承载 rootfs_data,~108MB): 卷0 data(94.3MB) / 卷1 etc(5MB) / 卷2 nvm(4.1MB)  —— ubifs 可写
ubi1 (承载 oem_data,~19.3MB):   卷0 oem_data(18.8MB)  —— 内含 squashfs,经 /dev/ubiblock1_0 只读挂载
```

**(d) 升级后要对比的关键锚点(清单)**
| 锚点 | 取法 | 现值(基线) |
|---|---|---|
| 固件版本 | `AT+QGMR` | EC200ACNTAR02A04M2G_OCPU |
| 激活槽 | `cat /proc/cmdline` 里 `system=` | a |
| rootfs verity 根哈希 | `cat /proc/cmdline` 里 sha256 第 1 串 | `4a71ba93…85ae` |
| 内核 | `uname -a` | 5.4.195 armv7l |
| 分区表 | `cat /proc/mtd` | 26 分区/256MB(见上) |
| dial 版本 | `cat /tmp/dial_version` | 1.28.10 |

### 21.2 出货镜像包分析(image_..._20260612)+ 同事结论验证
**目录性质**:一整套**该设备线刷镜像包**(EC200A YTL Vbox HT,B0608014916/SY0007751CV,日期 20260612),含 CP 三件、AP(root.squashfs/zImage/u-boot)、引导(TLoader/tim_falcon/DTim)、tos、oem_data.ubi、update.blf、多 DDR 变体 PPsetting。

**同事结论**:"这些镜像是设备目前使用的;root.squashfs、zImage 是我方适配过的,其他都是移远的。" **验证如下:**

| 判断 | 结论 | 实测依据 |
|---|---|---|
| root.squashfs 是我方的 | ✅ **证实** | `unsquashfs -l` 出 `usr/dial/dial`+`usr/dial/apn.json`(我方应用);created 2026-06-12(合文件夹日期);ARM/armhf 合设备 |
| CP 三件是移远的 | ✅ **证实** | ARBEL/MSA/RFPLUGIN 的 md5 **与 R03A02 stock 全不同**,日期 Sep 2022=R02A04 原厂 CP |
| 引导/AP 其他是移远的 | ✅ 合理 | TLoader/u-boot/tos/tim_falcon/DTim/DDR* 均 2022~2023 移远 stock 组件 |
| zImage 是我方"适配过" | ⚠️ **仅部分确认** | `file` 确认是 ARM zImage、合设备;但"我方适配"**无法独立验证**,按同事说法记 |
| 这些镜像=设备目前使用 | ⚠️ **对,但有修正** | 底座一致(CP=R02A04、rootfs 含我方 dial,合真机);**但运行中 dial ≠ 这套里的 dial**:包内 rootfs 烤的 dial 是 **2025-12-30 旧版**,设备实跑 **1.28.10**——因 `/usr` 是可写 overlay,dial 经 overlay 更新、未重刷 rootfs;另**未做 verity 哈希比对**,只能判"同血统底座",不能判"逐字节相同" |

**总判**:同事结论**基本正确**,两点修正——① zImage"我方适配"未独立证实;② 是**刷机底座**,运行 dial 已经 overlay 更新到更高版本。

### 21.3 ⚠️ 对前文推断的订正(诚实纠错)
| 位置 | 原结论 | 订正(依据 21.1/21.2 实测) |
|---|---|---|
| §19.4 | "设备是 R02A04 的 **128MB** 布局" | **错**。真机实测 **256MB**;磁盘上 R02A04 SDK blf 声明 128MB,**与真机不符**;反而 R03A02 blf 声明 256MB 与真机吻合。→ 打包要用的"匹配真机旧 blf"**不是磁盘那份 128MB 的**,应以本出货包的 `update.blf`(设备实配)为准,待比对 |
| §19.9 | "R02A04 全套底座镜像**本机缺失**" | **错/过时**。料在 `md/firmware/image_..._20260612/`:R02A04 全套底座 + **含 dial 的我方 rootfs** 都在。→ 走移远配方时 rootfs 可直接用这套(不被 stock 覆盖),解掉 §19.5 冲掉 dial 的风险 |
| §19.6 | oem_data 格式 ubi/squashfs 矛盾 | **进一步澄清**:真机 oem_data=UBI 容器(ubi1)内装 squashfs;本出货包 `oem_data.ubi` 与之同构 → 用旧底座即 `.ubi`,不冲突 |
| 早期笔记 | "asr_flag = mtd19" | **对本机错**:本机 `asr_flag`=**mtd21**,mtd19 是 `tos-b`。救砖涉及 asr_flag 时以 mtd21 为准 |

### 21.4 ⚠️ CLAUDE.md 架构口径错误(顺带查到,待订正)
- CLAUDE.md 通篇称目标平台 **aarch64**,判据写 `uname -m`=aarch64。
- **实测:`uname -a`=armv7l**;`file` 编译产物 `dial`=**ELF 32-bit ARM, ld-musl-armhf**(32 位 armhf)。
- → **真实目标架构是 armv7/armhf(32 位),不是 aarch64**。CLAUDE.md 该处及判据需订正(本次仅记录,未改 CLAUDE.md)。
- 注:移远所说"**211 基线**"是 ASR 内部平台基线号,与此处 Linux 内核 5.4.195 是两回事,勿混。

### 21.5 对 FOTA 打包路径的影响(基于本次实测)
1. **底座料齐了**:`md/firmware/image_..._20260612/` = R02A04 全套 + 含 dial rootfs → 移远"旧底座+换 3 个 R03A02 CP"配方,**物料本机已具备**(§19.9 缺料结论作废)。
2. **rootfs 保 dial 可行**:用本包 root.squashfs 作底座,dial/apn.json 不被移远 stock 覆盖。
3. **仍未解**:① 本包 `update.blf`/DTim 与真机 256MB 分区是否逐项匹配(下一步比对);② dm-verity——换任何进 rootfs 的内容都要重算 verity 根哈希、改 cmdline,打包链是否覆盖这一步待验;③ §20 移远最新口径"不能只换 CP、AP/CP 基线须对应"仍悬,即便料齐、能打包,**移远是否认可只换 CP 的产物**仍需其统一口径。

### 21.6 下一步(纯读取/验证类,不动设备)
1. 比对本出货包 `update.blf` 的分区/地址/NandSize 与真机 `/proc/mtd`(确认这份 blf 是否真机实配)。
2. 比对本出货包 CP 三件 与 R02A04 SDK、与 R03A02 的关系(确认版本谱系)。
3. 评估打包链对 dm-verity 根哈希的处理(换 rootfs 时是否自动重算)。

### 21.7 原始日志留档(完整,2026-06-30,升级后逐项 diff 用)
> §21.1 为可读性做过摘要;此处为**设备控制台完整原始输出**,升 AP/CP 后重采一次同样命令,与本节逐条 diff。

**`cat /proc/mounts`**
```
/dev/dm-0 / squashfs ro,seclabel,relatime 0 0
sysfs /sys sysfs rw,seclabel,relatime 0 0
selinuxfs /sys/fs/selinux selinuxfs rw,nosuid,noexec,relatime 0 0
proc /proc proc rw,nosuid,nodev,noexec,noatime 0 0
tmpfs /tmp tmpfs rw,seclabel,nosuid,nodev,noatime 0 0
ubi0:data /data ubifs rw,seclabel,noatime,assert=read-only,ubi=0,vol=0 0 0
ubi0:data /mnt ubifs rw,seclabel,noatime,assert=read-only,ubi=0,vol=0 0 0
ubi0:data /log ubifs rw,seclabel,noatime,assert=read-only,ubi=0,vol=0 0 0
ubi0:etc /overlay/etc ubifs rw,seclabel,noatime,assert=read-only,ubi=0,vol=1 0 0
ubi0:nvm /overlay/nvm ubifs rw,seclabel,noatime,assert=read-only,ubi=0,vol=2 0 0
overlayfs:/overlay/etc /etc overlay rw,seclabel,noatime,lowerdir=/etc,upperdir=/overlay/etc/root,workdir=/overlay/etc/work 0 0
overlayfs:/overlay/nvm /NVM overlay rw,seclabel,noatime,lowerdir=/NVM,upperdir=/overlay/nvm/root,workdir=/overlay/nvm/work 0 0
overlayfs:/data/opt /opt overlay rw,seclabel,noatime,lowerdir=/opt,upperdir=/data/opt/root,workdir=/data/opt/work 0 0
overlayfs:/data/media /media overlay rw,seclabel,noatime,lowerdir=/media,upperdir=/data/media/root,workdir=/data/media/work 0 0
overlayfs:/data/usrdata /usrdata overlay rw,seclabel,noatime,lowerdir=/usrdata,upperdir=/data/usrdata/root,workdir=/data/usrdata/work 0 0
overlayfs:/data/lib /lib overlay rw,seclabel,noatime,lowerdir=/lib,upperdir=/data/lib/root,workdir=/data/lib/work 0 0
overlayfs:/data/usr /usr overlay rw,seclabel,noatime,lowerdir=/usr,upperdir=/data/usr/root,workdir=/data/usr/work 0 0
/dev/ubiblock1_0 /NVM/oem_data squashfs ro,seclabel,relatime 0 0
tmpfs /dev tmpfs rw,seclabel,nosuid,relatime,size=512k,mode=755 0 0
devpts /dev/pts devpts rw,seclabel,nosuid,noexec,relatime,mode=600,ptmxmode=000 0 0
debugfs /sys/kernel/debug debugfs rw,seclabel,noatime 0 0
none /sys/fs/bpf bpf rw,nosuid,nodev,noexec,noatime,mode=700 0 0
overlayfs:/data/media /sdcard overlay rw,seclabel,noatime,lowerdir=/media,upperdir=/data/media/root,workdir=/data/media/work 0 0
/dev/mmcblk0p1 /media/sdcard ext4 rw,seclabel,relatime 0 0
securityfs /sys/kernel/security securityfs rw,relatime 0 0
/dev/mmcblk0p1 /sdcard ext4 rw,seclabel,relatime 0 0
```

**`cat /proc/cmdline`**
```
rootfstype=squashfs init=/etc/preinit noinitrd console=ttyS0,115200 panic_debug crashkernel=4k@0x00E00000 RDCA=0x00E00400 rootsize=0xF00000 ima_tcb=0 ima_appraise_tcb=0 ima_appraise=off force_pm80x_fg mem=128M@0x0 cpmem=24M@0x6800000 nocpload=0x6800000 tosmem=4M@0x2000000  ddr_mode=1 APMF=1 PROD=0 eehP=2 IMSD=1 RDUP=1 FAST=47574c33 root=/dev/dm-0 system=a dm-mod.create="root,,,ro,0 36912 verity 1 /dev/mtdblock11 /dev/mtdblock11 4096 4096 4614 4615 sha256 4a71ba93e7ca5b623d4a85a55f6d03489069aa6224844aad402739fa25ca85ae e587e287954cb360432fe5a4ed94929fb3c78ef65bf14feae47d109b631275c7"
```

**`cat /proc/mtd`**
```
dev:    size   erasesize  name
mtd0: 00040000 00020000 "bootloader"
mtd1: 00020000 00020000 "cp_reliabledata"
mtd2: 00020000 00020000 "ap_reliabledata"
mtd3: 00020000 00020000 "mep-ota"
mtd4: 00020000 00020000 "cp_reliabledata_backup"
mtd5: 00020000 00020000 "ap_reliabledata_backup"
mtd6: 00020000 00020000 "mep-ota_backup"
mtd7: 00040000 00020000 "dtim-a"
mtd8: 00f00000 00020000 "cpimage-a"
mtd9: 000c0000 00020000 "u-boot-a"
mtd10: 00800000 00020000 "kernel-a"
mtd11: 01380000 00020000 "rootfs-a"
mtd12: 00080000 00020000 "tos-a"
mtd13: 01400000 00020000 "oem_data-a"
mtd14: 00040000 00020000 "dtim-b"
mtd15: 00f00000 00020000 "cpimage-b"
mtd16: 000c0000 00020000 "u-boot-b"
mtd17: 00800000 00020000 "kernel-b"
mtd18: 01380000 00020000 "rootfs-b"
mtd19: 00080000 00020000 "tos-b"
mtd20: 01400000 00020000 "oem_data-b"
mtd21: 00100000 00020000 "asr_flag"
mtd22: 00040000 00020000 "misc"
mtd23: 00040000 00020000 "cust_info"
mtd24: 07060000 00020000 "rootfs_data"
mtd25: 00d20000 00020000 "QUEC_BBM"
```

**`cat /proc/partitions`**(mtdblock 部分,单位=1KB 块;全量累加=262144KB=256MB)
```
  31        0        256 mtdblock0      31       13      20480 mtdblock13
  31        1        128 mtdblock1      31       14        256 mtdblock14
  31        2        128 mtdblock2      31       15      15360 mtdblock15
  31        3        128 mtdblock3      31       16        768 mtdblock16
  31        4        128 mtdblock4      31       17       8192 mtdblock17
  31        5        128 mtdblock5      31       18      19968 mtdblock18
  31        6        128 mtdblock6      31       19        512 mtdblock19
  31        7        256 mtdblock7      31       20      20480 mtdblock20
  31        8      15360 mtdblock8      31       21       1024 mtdblock21
  31        9        768 mtdblock9      31       22        256 mtdblock22
  31       10       8192 mtdblock10     31       23        256 mtdblock23
  31       11      19968 mtdblock11     31       24     115072 mtdblock24
  31       12        512 mtdblock12     31       25      13440 mtdblock25
  (另: mmcblk0 61063168 / mmcblk0p1 61062144 / dm-0 18456 / ubiblock1_0 19344)
```

**`ubinfo -a`**(完整)
```
UBI version: 1 ; Count of UBI devices: 2 ; Present: ubi0, ubi1
ubi0: Volumes count 3 ; LEB size 126976B(124KiB) ; Total LEB 899(108.8MiB) ; avail 0 ; bad 0 ; reserved 40 ; min I/O 2048B
  Vol 0 dynamic  data  779 LEB (98914304B, 94.3MiB)  State OK
  Vol 1 dynamic  etc    42 LEB ( 5332992B,  5.0MiB)  State OK
  Vol 2 dynamic  nvm    34 LEB ( 4317184B,  4.1MiB)  State OK
ubi1: Volumes count 1 ; LEB size 126976B(124KiB) ; Total LEB 160(19.3MiB) ; avail 0 ; bad 0 ; reserved 0 ; min I/O 2048B
  Vol 0 dynamic  oem_data 156 LEB (19808256B, 18.8MiB)  State OK
```

**`ls -l /dev/ubi*`**
```
/dev/ubi0  /dev/ubi0_0  /dev/ubi0_1  /dev/ubi0_2   (ubi0 + 3 卷)
/dev/ubi1  /dev/ubi1_0                              (ubi1 + 1 卷)
/dev/ubi_ctrl  /dev/ubiblock1_0(块设备,oem_data squashfs 只读挂载源)
```

**版本三连**
```
# serial_atcmd "AT+QGMR"  →  EC200ACNTAR02A04M2G_OCPU
# uname -a                →  Linux OpenWrt 5.4.195 #87 PREEMPT Thu Jul 27 06:07:49 UTC 2023 armv7l GNU/Linux
# cat /tmp/dial_version   →  Version: 1.28.10
```
> 注:`/dev/mtd*` 字符设备节点(mtd0..mtd25 + 各 ro)略,与 `/proc/mtd` 一一对应,无额外信息。

---

## 22. 实打两个升级包(存量 FOTA + 增量线刷)+ 逐项验证(2026-06-30)

> 按移远全量包配方 + 我方实测结论,**在独立工作目录(scratchpad,未碰 SDK)** 实打两个包并逐项验证。
> 产物:`md/firmware/fota_out_R03A02CP_20260630/`(含 README.md / MANIFEST.md / md5sums.txt)。
> **核心:本次真正改的只有 CP→R03A02 + TIM/DTim 随新 CP 重算 hash;AP/内核/引导/我方 dial/OBM 全不动。**

### 22.1 配方落地(区别于移远原话的两处修正)
- 移远配方:R03A02 SDK 的 `tools/fota/` 放 R02A04 全套镜像 → 换 3 个 R03A02 CP → 用旧 blf → `gen_fota_image.sh` 全量包。
- **我方两处修正**(基于 §21 实测):
  1. **镜像底座用出货包 `image_..._20260612`**(R02A04 全套 + **含 dial 的 root.squashfs**),而非移远 stock rootfs → 保住 dial(解 §19.5)。
  2. blf 用 **R02A04 SDK 的两份**(§21.4 已证:出货 `update.blf` 与 R02A04 SDK `standard_AB.blf` 仅差 45 行琐碎项,NandSize/映射/偏移全同 → **§21.3"128MB 对不上"是我过度纠错,R02A04 SDK blf 确匹配真机**)。
- 工具:R03A02 SDK 的 `tim_builder`/`mkotafbf`(拷出用,未改 SDK)。

### 22.2 两个交付物
| 交付 | 用途 | 产物 | 大小/校验 |
|---|---|---|---|
| **存量_FOTA_test_absys/** | 现网设备远程 FOTA,走 `ql_abfota_start_update`+test_absys | `quectel_AB_OTA.img` | ≈36.5MB,md5 `ff2260ba030173cccd5f880680d93392` |
| **增量_线刷_SWDownloader/** | 新设备 USB 线刷,走 SWDownloader 读 `update.blf` | 22 个文件(与出货包同名集) | 见 md5sums.txt |

### 22.3 打包中踩的坑(已修,留记)
1. **zImage 无扩展名被通配符漏拷** → tim_builder 报 `TimBuild failed`、打出缺内核残包(33MB 作废)。补拷 zImage 重打 → 8 种镜像全入、TIM 正确重建(36.5MB)。
2. **增量集初装只换 CP、未重生成 TIM/DTim** → `update.blf` 每镜像配 SHA-256,里面仍是旧 R02A04 CP 的 hash → 线刷会 hash 不匹配。**修:** 在增量目录用 R03A02 tim_builder 按 `update.blf` 重生成全部 TIM/DTim(Primary/Recovery+3 DDR 变体)。
3. **重生成多出 `DKB_timheader.bin`**(出货包本无、update.blf 不引用) → 删除,使增量集文件名集合与出货包**完全一致(22 个)**。

### 22.4 逐项验证结论
**存量 FOTA 包 —— 结构验证通过:**
- 8 种镜像全入;build 无报错(除已修 zImage)。
- 8 个 Flash 地址与真机 `/proc/mtd` A 槽偏移**逐个吻合**(RFPLUGIN@0x140000/MSA@0x180000/ARBEL@0x580000/u-boot@0x1040000/zImage@0x1100000/rootfs@0x1900000/tos@0x2c80000/oem_data@0x2d00000)。
- **二进制取证**(差异区指纹):CP 三件均 R03A02(HIT)、无 R02A04 残留(MISS);rootfs 命中我方 `root.squashfs` 中段块。
- **hash 一致**:tim_builder 重生成 DTim.Primary(新 CP SHA-256)并嵌入 img(取证 HIT)。

**增量线刷集 —— 缺陷已修、结构一致:**
- CP=R03A02;TIM/DTim 已随新 CP 重算;主 `tim_falcon` 重生成结果与出厂**逐字节相同**(md5 `01936a4d…`)→ **佐证工具链忠实复现出厂 TIM 流程**,故按新 CP 生成的 DTim 可信。
- ARBEL 增大(7.59M→7.72M)仍在 15MB cpimage 分区内(可用 11.27MB),放得下。
- 文件名集合与出货包**完全一致**;溯源见 MANIFEST.md。

### 22.5 ⚠️ 验证覆盖不到、仍必须真机验的(勿当"没问题")
- **结构对 ≠ 真机能用**。以下只能上**能 USB 救砖的实验设备**实测,严禁现场首测:
  1. **CP988+AP211 组合**移远认不认(§20.3 自相矛盾未澄清);
  2. **dm-verity**:A/B 切槽后该槽 verity 根哈希能否过(§21.1 基线 `4a71ba93…`,起不来常见于此);
  3. **CP DUMP 是否真修好**;
  4. **OBM 未升级**:两个包都不含 OBM(FOTA blf 无 OBM 项),此前卡 OBM 疑为误用 R03A02 新分区 blf(本次已用匹配真机的旧 blf),但 OBM 到底需不需要同升仍以实测为准。
- **升级前仍应让移远书面确认**这个"只换 CP"产物有效、无隐患(§20 立场未变)。

### 22.6 可复现命令摘要
1. 工作台拷 R03A02 SDK 的 tim_builder/mkotafbf/gen_fota_image.sh/mversion。
2. 拷 R02A04 SDK 的 quectel_AB_OTA.blf + quectel_skylark_pm802_standard_AB.blf。
3. 拷出货包全部镜像(含 zImage!无扩展名易漏),用 R03A02 的 ARBEL/MSA/RFPLUGIN 覆盖。
4. `./gen_fota_image.sh` → `quectel_AB_OTA.img`(存量)。
5. 增量:出货包全套 + 换 3 CP,在该目录 `./tim_builder -r update.blf` 重生成 TIM/DTim,删 `DKB_timheader.bin`。

---

*整理于 2026-06-23，Q1~Q6 深度问答、实跑结果、absys/switch-sync 更正与移远最终方案于 2026-06-24 补充，真机实测(cpimage-only 崩溃+回滚、OBM 根因、出处链)与 OBM 单份无回滚/不可合包/升级方法记录程度于 2026-06-25 补充，移远全量包配方、两版 blf 实测 diff(分区调整实锤)与备料核对(R02A04 底座本机缺料)于 2026-06-26 补充，移远再对齐(单包作废、AP/CP 基线必须对应、换 CP 路线被否、现场试升反驳要点)于 2026-06-26 补充，移远坚持 AP+CP 同升、我方"≈整机平台迁移/存量设备更不可接受"反驳于 2026-06-30 补充，真机分区/rootfs 状态基线实测(含完整原始日志留档 §21.7)、出货镜像包分析与同事结论验证、§19.3/§19.4/§19.9 及 CLAUDE.md 架构订正于 2026-06-30 补充，实打两个升级包(存量 FOTA + 增量线刷)+逐项验证(含 zImage 漏拷/增量旧 hash 两处缺陷的发现与修复、§21.3 过度纠错的再订正)于 2026-06-30 补充。相关：`md/firmware/fota_out_R03A02CP_20260630/`(README/MANIFEST/md5sums)、`session_sdk_fota_20260604.md`、`meeting_quectel_cp_dump_20260609.md`、`meeting_quectel_cp_dump_20260610.md`、`quectel_sdk_changelog_R03A02.md`、`EC200A_ab_upgrade.md`、`EC200A_QuecOpen_固件烧录指导_完整分析.md`、`EC200A_QuecOpen_分区调整指导_完整分析.md`。*
