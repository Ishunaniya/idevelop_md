# EC200A-CN(TA) QuecOpen Trusted Boot 应用指导 — 完整分析

> **源文档**：Quectel_EC200A-CN(TA)_QuecOpen_Trusted_Boot_应用指导_V1.0.0_Preliminary_20221021.pdf
> **适用平台**：LTE Standard 模块系列 — EC200A-CN(TA) QuecOpen（基于 Linux 的嵌入式开发平台）
> **版本**：1.0.0　**日期**：2022-10-21　**状态**：临时文件（Preliminary）
> **原文页数**：15 页　**底层芯片**：ASR1803（Marvell/ASR）
> **内容范围**：Trusted Boot（可信引导）原理、流程、开启与签名方法

---

## 文档历史（修订记录）

| 版本 | 日期 | 作者 | 变更描述 |
|---|---|---|---|
| - | 2022-10-21 | Zhan HE | 文档创建 |
| 1.0.0 | 2022-10-21 | Zhan HE | 临时版本 |

---

## 1 引言

EC200A-CN(TA) 模块支持 **QuecOpen®** 方案（开源、基于 Linux 的嵌入式开发平台）。本文档介绍 EC200A-CN(TA) 模块的 **Trusted Boot 功能**。详见参考文档 [1]。

---

## 2 Trusted Boot 概述

**Trusted Boot（可信引导）**是一种安全策略，可确保在 EC200A-CN(TA) 模块上加载和运行的所有镜像都是**合法的**，以**防止合法软件被替换为非法软件**。

### 2.1 流程

启动链：**BootROM → OBM →（TOS）→ OS Loader(Uboot) → Kernel/CP**

**（1）当模块复位或上电重启时，BootROM 首先运行：**
- a) 从 flash 读取 **TIM**（可信镜像模块，Trusted Image Module）；
- b) 使用 **DSA 算法**对 TIM 签名进行校验：计算保存在 TIM 中的密钥的哈希值，并将其与 **fuse[2]** 中保存的值比较；
- c) 通过 TIM 中保存的镜像信息，**加载并校验 OBM 镜像**；
- d) 将控制权移交至 **OBM**。

**（2）BootROM 成功验证后，运行 OBM：**
- a) 读取 TIM（也可使用从 BootROM 传入的 TIM）；
- b) 通过 TIM 中保存的信息查找 **OS Loader 镜像和 TOS 镜像**（若存在 TOS）。若找到 → 跳转至步骤 e，否则按顺序执行步骤 c；
- c) 通过 TIM 中的信息加载 **DTIM 镜像**；
- d) 校验 DTIM 镜像：先校验 DTIM 的公钥，再校验 DTIM 镜像；
- e) 通过 TIM 镜像或 DTIM 镜像中的信息，加载并校验 **OS Loader 镜像和 TOS 镜像**。目前，OS Loader 和 TOS 镜像信息均包含在 **DTIM 镜像**中；
- f) 若存在 TOS 镜像 → OBM 将控制权移交至 TOS，然后 TOS 将控制权移交至 OS Loader（Uboot）；否则 → OBM 直接将控制权移交至 OS Loader。

**（3）OBM 或 TOS 运行完毕后，运行 OS Loader（Uboot）：**
- a) 读取 DTIM 相关信息（可选）；
- b) 通过 DTIM 中的信息或其他方法来**校验 kernel**；
- c) 将控制权移交至 **kernel**。

**（4）运行 kernel，完成系统初始化。**

> **图 1：模块 Trusted Boot 流程** — `BootROM ⇒ OBM ⇒ (TOS, 虚线可选) ⇒ Uboot ⇒ Kernel/CP`

### 2.2 软件实现

| 校验角色 | 职责 |
|---|---|
| **BootROM** | 负责校验 **TIM 镜像和 OBM 镜像**的密钥，它是 **Trusted Boot 的根** |
| **OBM** | 负责校验 **DTIM 镜像、OS Loader(Uboot) 镜像和 TOS 镜像**。OS Loader 和 TOS 镜像的签名哈希值均集成在 **DTIM 镜像**中 |
| **OS Loader(Uboot)** | 负责校验 **kernel**，也可按需读取 rootfs 和其他镜像 |

> 整个流程中使用**相同的哈希算法和签名算法**。

---

## 3 开启 Trusted Boot

### 3.1 BLF/OBM 开启 Trusted Boot

- 在源码中，**BLF 类型文件**保存在 `/marvell/swd/FALCON` 目录下（图 2）。该目录示例文件：
  ```
  asr1803_p401_QSPINAND_Trusted_CMCC_LPDDR2_AB.blf
  asr1803_p401_QSPINAND_Trusted_CMCC_LPDDR2.blf
  asr1803_p401_QSPINAND_Trusted_CMCC_LPDDR2_IMA_AB.blf
  asr1803_p401_QSPINAND_Trusted_CMCC_LPDDR2_IMA.blf
  asr1803_p401_QSPINAND_Trusted_TOS_CMCC_LPDDR2_AB.blf
  asr1803_p401_QSPINAND_Trusted_TOS_CMCC_LPDDR2.blf
  asr1803_p401_QSPINAND_Trusted_TOS_CMCC_LPDDR2_IMA_AB.blf
  asr1803_p401_QSPINAND_Trusted_TOS_CMCC_LPDDR2_IMA.blf
  cfg_files   extra_blfs.sh   gen_pb.sh
  ```
- 使用源码或 SDK 编译出的固件包中包括最终的 BLF 文件 **`/update/update.blf`**（图 3）。固件包 `update` 目录还含：`oem_data.ubi`、`RFPLUGIN.bin`、`root.squashfs`、`tim_falcon_qspinand.bin`、`TLoader_QSPINAND.bin`、`TLoader_QSPINAND_ProductBuild.bin`、`tos.bin`、`u-boot.bin`、`update.blf` 等。

**BLF 文件用途**：进行**镜像分区配置、DDR 设置、启动模式和其他功能配置**。

要开启 Trusted Boot 功能，可在源码中的 BLF 文件中配置，或在固件包的 `update.blf` 文件中配置，最终通过 **SWDownloader 工具**读取 BLF 文件，并发送命令到模块。

通过修改固件中的 `update.blf` 文件来开启 Trusted Boot 功能的方法如下：

#### （1）使能 Trusted Boot

在 BLF 文件中加入 `Trusted = 1`：

```
FFOS_Type = OWRT
OEM_UniqueID = 0x4E5A4133
Issue_Date = 0x20140601
Version = 0x00030400
Trusted = 1
[Reserved_Data]
```

> ⚠️ **备注**：若模块**已开启 Trusted Boot 功能并已熔断 FUSE**，但 BLF 文件中**没有添加 `Trusted = 1`**，会导致**固件烧录失败**。

#### （2）使能 FUSE 熔断

在 BLF 文件中添加如下信息，使能 FUSE 熔断：

```
FUSE
Enabled = 0x00000001
End_FUSE
```

添加后的 BLF 文件内容示例（位于 `End_BBMT` 之后）：

```
End_OTAI
BBMT
Version = 0x00000001
End_BBMT
FUSE
Enabled = 0x00000001
End_FUSE
CITA_CUMIZED_INFO_TRANSFER_TOAP_ID
```

#### （3）OBM 编译

- 在编译 OBM 镜像时配置 **`PRODUCT_BUILD=1`**，会将 **OEM 公钥哈希值写入到 fuse[2]**，进行 **FUSE 熔断**。
- 当前源码中已配置 `PRODUCT_BUILD=1`，编译得到的二进制文件默认命名为 **`TLoader_QSPINAND_ProductBuild.bin`**。
- 用 product-build 的 OBM 镜像替换正常镜像进行熔断操作，需将 `update.elf` 文件中的
  `2_Image_Path = TLoader_QSPINAND.bin` 替换为 `2_Image_Path = TLoader_QSPINAND_ProductBuild.bin`。

替换后 `update.elf` 文件内容示例：

```
2_Image_Image_ID = 0x4F424D49
2_Image_Next_Image_ID = 0x41524249
2_Image_Path = TLoader_QSPINAND_ProductBuild.bin
2_Image_Flash_Entry_Address = 0x00020000
2_Image_Load_Address = 0x005E8000
```

#### （4）烧录验证

- 使用 **SWDownloader** 工具下载修改了 `update.blf` 文件的固件，下载完后上电重启模块，通过串口工具查看启动日志。
  - 若**未进行**第（2）步操作 → 打印 `Trusted OBM without ProductBuild`；
  - 若**进行了**第（2）步操作 → 打印 `Trusted OBM with ProductBuild`。

启动日志示例：

```
Verify Image: PASS
Xfer to OBM
start at 0x5e8000
SWDownloader 4.9.1.5 for ASR1803
OBM 4.9.1.5 for ASR1803
-- Sep  7 2022 - 11:41:54 --
pFuses->value[0]: 0x839a0241
pFuses->value[1]: 0x0
pFuses->bits.SBE: 1
Trusted OBM with ProductBuild
MMU Enabled
PMIC ID: 0x00000013
```

- 同时可查看到 **fuse[2] 的值不为 0**（EFuse dump 中 Bank2 一行非全 0），示例：

```
ABBT order: positive
Flash Init Done
E/TC:0 00 EFuse dump, format: high <-- low
E/TC:0 00 Bank0: 00000000 000001ff e44e44e0 188d4620 d8000012 4924db00 00000000
E/TC:0 00 Bank1: 00000000 00000000 00000000 00000000 00000000 00000000 00000000
E/TC:0 00 Bank2: 0d1b1286 a736a65f 89931c93 d055114a bb29755d d6b1cb8e 2d10c05e 81909484
E/TC:0 00 Bank3: 7caf5914 8a860000 00000000 00000000 00000000 00000000 00000000
E/TC:0 00 ASR Processor 1803
```

### 3.2 RSA 秘钥配置

用户可通过 **RSAKeyGenerator** 工具生成 **RSA 密钥对**，并将其配置到 BLF 文件中的 **TIM 和 DTIM 信息**中。

**RSAKeyGenerator 工具路径**（源码中）：
- Windows：`marvell/swd/tools/RSAKeyGenerator/RSAKeyGenerator.exe`
- Linux：`marvell/swd/tools/RSAKeyGenerator/RSAKeyGeneratorCmd`

**每个 RSA 密钥由三个部分组成：**

| 部分 | 长度 | 在 BLF 中的位置 |
|---|---|---|
| a) 公钥指数 | 1 字 | `RSA_Public_Exponent` 之后 |
| b) 公钥模量 | 64 字 | `RSA_System_Modulus` 之后 |
| c) 私钥 | 64 字 | `RSA_Private_Key` 之后 |

生成的密钥对信息保存在 **`output_key.blf`** 文件中。获取方法：

- **Windows 系统**，在 shell 终端中运行：
  ```sh
  RSAKeyGenerator.exe -h SHA-256 -s 2048 -f output_key.blf
  ```
- **Linux 系统**，输入：
  ```sh
  ./RSAKeyGeneratorCmd -h SHA-256 -s 2048 -f output_key.blf
  ```
  > 参数：`-h SHA-256`（哈希算法），`-s 2048`（密钥位长），`-f output_key.blf`（输出文件）。

将 `output_key.blf` 的内容添加到 `update.blf` 文件中的 **`End_Extended_Reserved_Data`** 和 **`[Image_List]`** 之间，示例（数字签名数据段）：

```
END_Vendor_DDR_Initialization
End_Extended_Reserved_Data
[Digital_Signature_Data]
Hash_Algorithm_ID = SHA-256
DSA_Algorithm = PKCS1_v1_5_Ippcp
Key_Size_in_bits = 2048
RSA_Public_Exponent
#1 = 0x00010001
End_RSA_Public_Exponent
RSA_System_Modulus
#1 = 0x6FBF41D3
#2 = 0xD44FB898
...
#60 = 0xF42F7525
#61 = 0xE5CB44CB
#62 = 0x1BE9E44A
#63 = 0xEE0A3107
#64 = 0xBB3EABBE
End_RSA_Private_Key
End_DTIM_Keys_Data
[Image_List]
1_Image_Enable = 1
1_Image_Tim_Included = 1
```

### 3.3 镜像签名

EC200A-CN(TA) 模块的镜像通过获取 **TIM 或 DTIM 中的信息**来进行签名。镜像的哈希值保存在 TIM 或 DTIM 中，TIM 或 DTIM 通过 **RSA 密钥**进行签名，因此**签名是在 TIM 或 DTIM 编译的过程中进行的**。

镜像和 TIM 之间的关系保存在 BLF 文件中，每一个镜像都有 **`Image_Tim_Include`** 属性值，对应值可以是 0、1 或 2：

```
X_Image_Tim_Included = 0  //信息不包含在 TIM 中
X_Image_Tim_Included = 1  //信息包含在 TIM 中，主要用于 OBM 镜像
X_Image_Tim_Included = 2  //信息包含在 DTIM 中，主要用于 Uboot、Kernel 和 TOS
```

| 取值 | 含义 | 主要用于 |
|---|---|---|
| 0 | 信息不包含在 TIM 中 | — |
| 1 | 信息包含在 TIM 中 | OBM 镜像 |
| 2 | 信息包含在 DTIM 中 | Uboot、Kernel、TOS |

**操作**：根据第 3.1~3.2 章配置好 BLF 文件后，用户可使用 **SWDownloader 工具打开 BLF 文件**（图 4）。
> **点击绿色图标**，镜像将被签名，并准备烧入到 flash 中。

---

## 4 附录 参考文档及术语缩写

### 表 1：参考文档

| 序号 | 文档名称 |
|---|---|
| [1] | Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

### 表 2：术语缩写

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| DAP | Debug Access Port | 调试访问端口 |
| DDR | Double Data Rate | 双倍数据速率 |
| DSA | Digital Signature Algorithm | 数字签名算法 |
| DTIM | Dynamic Trusted Image Module | 动态可信镜像模块 |
| IoT | Internet of Things | 物联网 |
| OBM | OEM Boot Module | OEM 启动模块 |
| OEM | Original Equipment Manufacturer | 原始设备制造商 |
| SDK | Software Development Kit | 软件开发工具包 |
| TIM | Trusted Image Module | 可信镜像模块 |
| TOS | Trusted OS | 可信操作系统 |

---

## 关键要点速查（实践提炼）

- **信任链**：BootROM（根，固化在芯片）→ 校验 TIM/OBM → OBM 校验 DTIM/Uboot/TOS → Uboot 校验 kernel。每级只信任校验通过的下一级。
- **TIM vs DTIM**：OBM 信息在 TIM（`Tim_Included=1`）；Uboot/Kernel/TOS 信息在 DTIM（`Tim_Included=2`）。
- **开启三件套**：BLF 中 `Trusted = 1` + `FUSE/Enabled=0x1/End_FUSE` + OBM 用 `PRODUCT_BUILD=1` 编出的 `TLoader_QSPINAND_ProductBuild.bin`（替换 `update.elf` 的 `2_Image_Path`）。
- ⚠️ **不可逆 + 顺序坑**：FUSE 熔断后，BLF 缺 `Trusted = 1` 会导致后续烧录失败；fuse[2] 写入 OEM 公钥哈希。
- **验证**：串口日志看 `Trusted OBM with ProductBuild`，EFuse dump 的 Bank2 非全 0（fuse[2]≠0）。
- **密钥**：RSAKeyGenerator 生成 `output_key.blf`（SHA-256 + RSA 2048，公钥指数1字/模量64字/私钥64字），内容插入 `update.blf` 的 `End_Extended_Reserved_Data` 与 `[Image_List]` 之间。
- **签名**：SWDownloader 打开 BLF → 点绿色图标 → 镜像在 TIM/DTIM 编译过程中被 RSA 签名并准备烧录。
