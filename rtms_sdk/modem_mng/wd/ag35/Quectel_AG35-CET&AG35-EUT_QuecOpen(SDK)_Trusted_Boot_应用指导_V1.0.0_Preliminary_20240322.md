# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) Trusted Boot 应用指导 V1.0.0 — 文档分析

## 一、文档基本信息

- **标题**：AG35-CET&AG35-EUT QuecOpen(SDK) Trusted Boot 应用指导
- **适用产品/型号**：LTE Standard 模块系列，具体为 **AG35-CET** 和 **AG35-EUT** 模块
- **方案前提**：本文档适用于 **SDK 构建环境的 QuecOpen® 方案**（QuecOpen® 是基于 Linux 的嵌入式开发平台，用于简化 IoV 车联网应用的软件设计与开发过程）
- **版本号**：1.0.0
- **日期**：2024-03-22
- **状态**：临时版本（Preliminary）
- **作者**：Mike ZHOU
- **文档历史**：仅两条记录 —— "-"（2024-03-22，文档创建）与 "1.0.0"（2024-03-22，临时版本），同一天创建并发布临时版，说明这是首版、未经多轮修订
- **文档总页数**：正文编号 1/18 ~ 18/18（PDF 物理页共 19 页，第 1 页为封面，不计入正文编号）
- **发布单位**：上海移远通信技术股份有限公司（Quectel）
- **保密/水印标记**：全文带 "Preliminary Confidential" / "Quectel Confidential" 水印，文档明确为内部/客户限定分发，非公开最终版
- **适用范围**：主要介绍 AG35-CET 和 AG35-EUT 模块的 **Trusted Boot（可信启动）** 功能，包括该功能的实现流程及具体开启步骤

## 二、目录结构概览

```
文档历史 .......................................................... 3
目录 .............................................................. 4
表格索引 .......................................................... 5
图片索引 .......................................................... 6
1 引言 ............................................................. 7
2 Trusted Boot 概述 ................................................ 8
  2.1. 签名与密钥 ..................................................... 8
  2.2. Trusted Boot 流程 .............................................. 9
3 Trusted Boot 功能开启 ........................................... 11
  3.1. BLF 文件配置 .................................................. 11
  3.2. RSA 密钥配置 .................................................. 14
  3.3. 镜像签名 ...................................................... 15
  3.4. 镜像签名后再烧录 .............................................. 16
    3.4.1. FBFMake 工具 .............................................. 16
    3.4.2. FBFDownloader 工具 ........................................ 17
4 附录 参考文档及术语缩写 ........................................... 18
```

图片索引（4幅图）：
- 图1：模块 Trusted Boot 流程（第9页）
- 图2：源码中的 BLF 文件（第11页）
- 图3：固件包中的 update.blf 文件（第11页）
- 图4：使用 SWDownloader 工具打开 BLF 文件（第15页）

表格索引（2张表，均在附录）：
- 表1：参考文档
- 表2：术语缩写

## 三、逐章节详细摘要

### 第1章 引言（正文第7页）

- 移远通信 AG35-CET 和 AG35-EUT 模块支持 QuecOpen® 方案
- QuecOpen® 是基于 Linux 的嵌入式开发平台，可简化 IoV（车联网）应用的软件设计和开发过程
- QuecOpen® 详细信息参考文档 [1]：《Quectel_AG35-CET_QuecOpen_快速开发指导》
- 本文档主要介绍 AG35-CET 和 AG35-EUT 模块的 Trusted Boot 功能，包括功能实现流程以及具体开启步骤
- 本章内容简短，无具体步骤或参数

### 第2章 Trusted Boot 概述（正文第8~10页）

**核心定义**：Trusted Boot（可信启动）是一种安全策略，确保 AG35-CET 和 AG35-EUT 模块上加载和运行的所有镜像都是合法的，以防止合法软件被替换为非法软件。

#### 2.1 签名与密钥（正文第8页）

- **TIM（Trusted Image Module，可信镜像模块）**：软件包中包含的一类特殊镜像，由 SoftwareDownloader、Timbuilder 等工具根据 BLF 文件生成，包含硬件参数、签名密钥、安全配置、镜像哈希等信息
- **签名机制**：Trusted Boot 方案是通过对 TIM 签名间接对其它镜像进行签名。工具在生成 TIM 阶段同步完成 TIM 的签名
- **启动阶段验证流程**：先校验 TIM 的签名以验证 TIM 的合法性，然后计算相应镜像的哈希并与 TIM 中该镜像的哈希进行比较，哈希值匹配则该镜像合法
- **算法**：签名算法为 **RSA2048**，哈希算法为 **SHA256**
- **两组密钥**：
  1. **第一组密钥**：用来签名和校验 BootLoader（即 OBM），对应的 TIM 为 **TIMH**；OBM 的哈希等信息保存在 TIMH 当中
  2. **第二组密钥**：用来签名 OS Loader（即 Uboot）、内核、只读文件系统等镜像，对应的 TIM 为 **DTIM.Primary**；Uboot 等镜像的信息保存在 DTIM.Primary 当中
  - 两组密钥可以相同，也可以不同
- **密钥与签名细节**：
  - 密钥包括公钥和私钥，保存在 BLF 文件当中
  - 工具利用 BLF 中的私钥对 TIM 进行签名，公钥以一定格式保存在 TIM 当中
  - 第一组密钥的公钥保存在 TIMH 中，第二组密钥的公钥保存在 DTIM.Primary 中
  - **第一组公钥的合法性**由芯片的 **efuse** 保证：设备生产时将公钥的哈希烧写到 efuse 当中；设备启动时，BootROM 计算 TIMH 中第一组公钥的哈希，并与 efuse 中的哈希进行比较，若不匹配，设备不启动
  - **第二组公钥的合法性**由第一组密钥来保证：工具在签名时用 TIMH 的私钥对 DTIM.Primary 的公钥的模数和指数分别进行签名，两个签名一起放在 DTIM.Primary 的尾部；在用 DTIM.Primary 的公钥对 DTIM.Primary 进行签名验证的同时，用 TIMH 的公钥对 DTIM.Primary 公钥的模数和指数的两个签名分别进行验证，以确保 DTIM.Primary 公钥的合法性

#### 2.2 Trusted Boot 流程（正文第9~10页）

**图1 流程图**：BootROM → OBM →（与 TOS 互相虚线箭头）→ Uboot → Kernel/CP

**关键角色分工**：
- **BootROM** 负责校验 TIM 镜像、OBM 镜像和 TIM 镜像的密钥，它是可信启动的根（Root of Trust）
- **OBM** 负责校验 DTIM 镜像、OS Loader（Uboot）镜像和 TOS 镜像
- **OS Loader（Uboot）镜像和 TOS 镜像**的签名哈希值均集成在 DTIM 镜像中；OS Loader（Uboot）负责校验 kernel，也可按需读取 rootfs 和其他镜像
- 整个 Trusted Boot 流程中使用相同的哈希算法和签名算法

**详细流程步骤**（共4个阶段）：

1) **模块复位或上电重启时，BootROM 首先运行**：
   - a) 从 flash 中读取 TIM
   - b) 使用 RSA 算法对 TIM 签名进行校验
   - c) 验证 TIM 的公钥的合法性（计算公钥哈希值，并与 fuse[2] 中的哈希进行比较）
   - d) 通过 TIM 中保存的 OBM 镜像信息，加载并验证 OBM 镜像（即 BootLoader）
   - e) 将控制权移交给 OBM

2) **BootROM 成功完成验证后，运行 OBM**：
   - a) 从 flash 中读取 TIM（也可使用从 BootROM 传入的 TIM）
   - b) 通过 TIM 中保存的信息查找 OS Loader 镜像和 TOS 镜像（若存在 TOS），若找到，则跳转至步骤 e，否则按顺序执行步骤 c
   - c) 通过 TIM 中的信息加载 DTIM.Primary 到内存中
   - d) 校验 DTIM.Primary：先校验 DTIM.Primary 的公钥，再校验 DTIM.Primary
   - e) 通过 DTIM.Primary 中的信息，加载并校验 OS Loader（Uboot），若存在 TOS 镜像，则同时加载并校验 TOS 镜像
   - f) 若存在 TOS 镜像，则 OBM 将控制权移交至 TOS，TOS 初始化完成后将控制权移交至 OS Loader（Uboot）；否则，OBM 直接将控制权移交至 OS Loader

3) **OBM 或 TOS 运行完毕后，运行 OS Loader（Uboot）**：
   - a) 读取 DTIM.Primary
   - b) 通过 DTIM.Primary 中的信息加载并校验 kernel
   - c) 将控制权移交至 kernel

4) **运行 kernel，完成文件系统挂载**

> 注：步骤 2)b) 原文存在轻微文字疑似笔误（"若找到，则跳转至步骤 e，否则按顺序执行步骤 c"），但已按文档原文准确转录，未做主观修正。

### 第3章 Trusted Boot 功能开启（正文第11~17页）

#### 3.1 BLF 文件配置（正文第11~13页）

- **BLF（Binary List File，二进制列表文件）**用于镜像分区配置、DDR 设置、启动模式和其他功能配置
- 在**源码**中，BLF 类型文件保存在 `/marvell/swd/FALCON/` 目录下（图2 展示了该目录下的文件列表，例如 `asr1806_p401_QSPINAND_Trusted_CMCC_LPDDR2_AB.blf`、`asr1806_p401_QSPINAND_Trusted_CMCC_LPDDR2_IMA_AB.blf`、`asr1806_p401_QSPINAND_Trusted_TOS_CMCC_LPDDR2_AB.blf`、`asr1806_p401_QSPINAND_Trusted_TOS_CMCC_LPDDR2_IMA_AB.blf` 等，以及 `cfg_files`、`extra_blfs.sh`、`gen_pb.sh`）
- 使用源码或 SDK 编译出的**固件包**中包含最终的 BLF 文件 `/update/update.blf`（图3）
- 要开启 Trusted Boot 功能，可在源码中的 BLF 文件中进行配置，或在固件包中的 `update.blf` 文件中进行配置，最终通过 **SWDownloader** 工具读取 BLF 文件，并发送命令到模块

**通过修改固件中的 update.blf 文件开启 Trusted Boot 功能的方法（共4步）**：

**(1) 使能 Trusted Boot**
- 在 BLF 文件中加入 `Trusted = 1`
- 示意配置段：
  ```
  Processor_Type = ASR1806
  FFOS_Type = OWRT
  OEM_UniqueID = 0x4E5A4133
  Issue_Date = 0x20140601
  Version = 0x00030400
  Trusted = 1
  SDTIM = 0
  [Reserved_Data]
  DTYP
  DDR_Type = 0x00000001
  End DTYP
  ```

**备注（文档原文，重要警示）**：
1. **若模块已开启 Trusted Boot 功能并已熔断 FUSE，但 BLF 文件中没有添加 `Trusted = 1`，会导致固件烧录失败**
2. 请联系移远通信技术支持获取 SWDownloader 工具

**(2) 使能 FUSE 熔断**
- 在 BLF 文件中添加：
  ```
  FUSE
  Enabled = 0x00000001
  End_FUSE
  ```
- 添加完后的 BLF 文件片段示例（约第59~65行）：
  ```
  BBMT
  Version = 0x00000001
  End_BBMT
  FUSE
  Enabled = 0x00000001
  End_FUSE
  HTFX
  ```

**(3) 编译 OBM**
- 在编译 OBM 镜像时，若配置了 `PRODUCT_BUILD=1`，会将 OEM 公钥哈希值写入到 `fuse[2]`，进行 FUSE 熔断
- 当前源码中已配置 `PRODUCT_BUILD=1`，编译得到的二进制文件默认命名为 `TLoader_QSPINAND_ProductBuild.bin`
- 用 product-build 的 OBM 镜像替换正常镜像进行熔断操作，需将 `update.blf` 文件中：
  - `3_Image_Path = TLoader_QSPINAND.bin` 替换为 `3_Image_Path = TLoader_QSPINAND_ProductBuild.bin`
  - `4_Image_Path = TLoader_QSPINAND.bin` 替换为 `4_Image_Path = TLoader_QSPINAND_ProductBuild.bin`
- 替换后的 update.blf 文件内容片段（约第1822~1836行）：
  ```
  3_Image_Path = TLoader_QSPINAND_ProductBuild.bin
  3_Image_Flash_Entry_Address = 0x00040000
  3_Image_Load_Address = 0x004E8000
  3_Image_Type = RAW
  3_Image_ID_Name = OBMI
  3_Image_Erase_Size =
  3_Image_Partition_Number = 0
  3_Image_Algorithm_ID = SHA-256
  3_Image_Image_Size_To_Hash_in_bytes = 0xFFFFFFFF
  4_Image_Enable = 1
  4_Image_Tim_Included = 5
  4_Image_Image_ID = 0x4F424D49
  4_Image_Next_Image_ID = 0x4350524C
  4_Image_Path = TLoader_QSPINAND_ProductBuild.bin
  4_Image_Flash_Entry_Address = 0x00060000
  ```

**(4) 烧写验证**
- 使用 SWDownloader 工具下载修改了 `update.blf` 文件的固件，下载完成后上电重启模块，然后通过串口工具查看启动日志
- 若未进行第(2)步操作，会打印 `Trusted OBM without ProductBuild`
- 若进行了第(2)步操作，会打印 `Trusted OBM with ProductBuild`
- 示例日志：
  ```
  Verify Image: PASS
  Xfer to OBM
  start at 0x4e8000
  ASR1806 OBM with SWD >= 4.9.1.9
  OBM heap: 0x8000-->0x4e8000, 0x518800-->0x10000000
  Reserve memory 0x30800@0x4e8000
  -- Oct 11 2023 - 03:49:50 --
  pFuses->value[0]: 0x8b9a0241
  pFuses->value[1]: 0x0
  pFuses->bits.SBE: 1
  Trusted OBM with ProductBuild
  MMU Enabled
  PMIC ID: 0x9
  ```
- 同时可查看到 `fuse[2]` 的值不为0：
  ```
  I/TC: Primary CPU initializing
  I/TC: ASR Processor 1806
  I/TC: EFuse dump, format: high <-- low
  I/TC: Bank0: 58000000 000001ff 80000000 001e0794 00000000 d800007f fff65900 00000000
  I/TC: Bank1: 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
  I/TC: Bank2: 0d1b1286 a736a65f 89931c93 d055114a bb29755d d6b1cb8e 2d10c05e 81909484
  I/TC: Bank3: 8ac15f00 3d400000 00000000 00000000 00000000 00000000 00000000 00000000
  I/TC: Primary CPU switching to normal world boot
  ```

#### 3.2 RSA 密钥配置（正文第14页）

- 用户可通过 **RSAKeyGenerator** 工具生成 RSA 密钥对，并将其配置到 BLF 文件中的 TIM 和 DTIM 信息中
- RSAKeyGenerator 在源码中的路径：
  - `marvell/swd/tools/RSAKeyGenerator/RSAKeyGenerator.exe`（Windows）
  - `marvell/swd/tools/RSAKeyGenerator/RSAKeyGeneratorCmd`（Linux）
- **每个 RSA 密钥对由三部分组成**：
  a) 公钥指数：1字，在 BLF 文件中 `RSA_Public_Exponent` 之后
  b) 公钥模数：64字，在 BLF 文件中 `RSA_System_Modulus` 之后
  c) 私钥：64字，在 BLF 文件中 `RSA_Private_Key` 之后
- 生成的密钥对信息保存在 `output_key.blf` 文件中
- **获取 output_key.blf 的方法**：
  - Windows：`RSAKeyGenerator.exe -h SHA-256 -s 2048 -f output_key.blf`
  - Linux：`./RSAKeyGeneratorCmd -h SHA-256 -s 2048 -f output_key.blf`
- 将 `output_key.blf` 的内容添加到 `update.blf` 文件中的 `End_Extended_Reserved_Data` 和 `[Image_List]` 之间
- 示意配置段：
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

#### 3.3 镜像签名（正文第15页）

- 模块的镜像签名通过获取 TIM 或 DTIM 中的信息来间接进行。镜像的哈希值保存在 TIM 或 DTIM 中，TIM 或 DTIM 通过 RSA 密钥进行签名，因此**签名是在 TIM 或 DTIM 编译的过程中进行的**
- 镜像和 TIM（或 DTIM）之间的关系保存在 BLF 文件中，每个镜像都有 `X_Image_Tim_Included` 属性值，对应值可以是 0、1 或 2：
  - `X_Image_Tim_Included = 0` //信息不包含在 TIM 中
  - `X_Image_Tim_Included = 1` //信息包含在 TIM 中，主要用于 OBM 镜像
  - `X_Image_Tim_Included = 2` //信息包含在 DTIM 中，主要用于 Uboot、Kernel 和 TOS
- 根据第3.1~3.2章配置好 BLF 文件后，用户可使用 **SWDownloader** 工具打开 BLF 文件
- 图4 展示了 SWDownloader 4.9.1.3 工具打开 `update.blf` 后的界面，表格列包括：S（选中）、P（Partition）、T（Tim）、ImgID、Erase、Flash Address、Load Addr、HashAlgm（均为 SHA-256）、Size to Ha...、ImgType（RAW/DTIM等）、Img File Path
  - 列出的镜像 ID 包括：TIM-H、OBMI、ARBI、GRBI、RFBI、OSLO、ZIMG、SYSJ、TZSI、OEMD（重复出现于不同 Partition）、TIM1、TIM4、TIM2、ERAS（擦除操作，ImgType=ERASO...）
- **操作**：点击绿色图标，镜像将被签名，并准备烧录到 flash 中

#### 3.4 镜像签名后再烧录（正文第16~17页）

**背景与动机**：由于使用 SWDownloader 工具进行烧录时，在 `update.blf` 文件中会暴露 TIM 和 DTIM 的私钥，在生产阶段中很容易导致密钥被盗窃。为了解决这个问题，可使用 **FBFMake** 工具来对镜像进行签名并生成下载包，然后通过 **FBFDownloader** 工具下载。在生产阶段，可直接提供该下载包，避免提供 `update.blf` 文件。

- FBFMake 和 FBFDownloader 工具位于源码的 `marvell/swd/tools/FBFMake/` 目录下，也可联系移远通信技术支持获取
- Windows 系统文件：`fbfdownloader`、`FBFDownloader.exe`、`fbfmake`、`FBFMake.exe`
- Linux 系统：源码目录 `ol@ol:~/code/asr1806/marvell/swd/tools/FBFMake$ ls`（具体文件列表未在文档中详细列出）

##### 3.4.1 FBFMake 工具（正文第16页）

- 涉及两个参数：
  - `-r`：固件包中 `update.blf` 文件的绝对路径
  - `-f`：生成的下载包路径
- **命令示例**：
  ```
  .\FBFMake.exe -r E:\code\asr1806\AG35CETCAR01A01M2G_OCPU\update\update.blf -f out
  ```
- 执行日志摘录：
  ```
  FBFMake Version: 5.0.0.0
  FBFMake Date: 2023-7-14
  PreTempPcrDownload...
  ProcessBLF...
  ASR8XT platform type
  Parsing BLF file: E:\code\asr1806\AG35CETCAR01A01M2G_OCPU\update\update.blf
  ASR8XT platform type
  Parsing Reserved data.....
  complete to parsing reserved data
  Parsing ExtendedReservedData
  complete to parse ExtendedReservedData
  begin to parsing Erase Only Area
  ```
- 上述命令执行后在 `/out/` 目录下生成 `BinFile.bin` 文件，该文件打包了签名后的镜像
- 文件信息示例（PowerShell `ls .\out\`）：`BinFile.bin`，大小 32480100 字节，时间 2023/11/14 周二 13:42

##### 3.4.2 FBFDownloader 工具（正文第17页）

- 用于将打包了签名后镜像的二进制文件下载到模块中
- 涉及一个参数：
  - `-b`：需下载的文件的路径
- **命令示例**：
  ```
  .\FBFDownloader.exe -b .\out\BinFile.bin
  ```
- 执行日志摘录：
  ```
  FBFDownloader Version: 5.0.0.0
  FBFDownloader Date: 2023-7-14
  DKB_timheader is C:\Users\Zhan.He\AppData\Local\Temp\SWD7B82.tmp\temp\DKB_timheader.bin  DKB is C:\Users\Zhan.He\AppData\...
  ...（多行临时文件路径日志，含 FBF head 文件如 0x05080xFFFF_FBF_h.bin 等）
  InitializeDL successfully...
  please plug in USB device....
  Add an WTPTP device: Device 1.......
  Device 1:Burning flash percentage is 100
  Device 1:Download Completed successfully..........
  TerminateDL successful ...
  ```

### 第4章 附录 参考文档及术语缩写（正文第18页）

**表1：参考文档**

| 序号 | 文档名称 |
|------|----------|
| [1] | Quectel_AG35-CET_QuecOpen_快速开发指导 |

**表2：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|------|----------|----------|
| BLF | Binary List File | 二进制列表文件 |
| DAP | Debug Access Port | 调试访问端口 |
| DDR | Double Data Rate | 双倍数据速率 |
| DSA | Digital Signature Algorithm | 数字签名算法 |
| DTIM | Dynamic Trusted Image Module | 动态可信镜像模块 |
| IoV | Internet of Vehicles | 车联网 |
| OBM | OEM Boot Module | OEM 启动模块 |
| OEM | Original Equipment Manufacturer | 原始设备制造商 |
| SDK | Software Development Kit | 软件开发工具包 |
| TIM | Trusted Image Module | 可信镜像模块 |
| TOS | Trusted OS | 可信操作系统 |

> 注：DAP（Debug Access Port）出现在术语表中，但正文中未提及其具体用途；文档未对 DAP 做进一步说明。另外，正文第2.1节将 DTIM 称为 "DTIM.Primary"（含"Primary"），而术语表中 DTIM 全称为 "Dynamic Trusted Image Module"，文档未解释 ".Primary" 后缀的具体含义或是否存在 DTIM 的其他变体（如 Secondary）。

## 四、关键命令/接口/参数汇总表

| 类别 | 名称 | 说明 |
|------|------|------|
| 算法 | RSA2048 | Trusted Boot 签名算法 |
| 算法 | SHA256 | Trusted Boot 哈希算法 |
| BLF 配置项 | `Trusted = 1` | 使能 Trusted Boot 功能 |
| BLF 配置项 | `FUSE` / `Enabled = 0x00000001` / `End_FUSE` | 使能 FUSE 熔断 |
| BLF 配置项 | `X_Image_Tim_Included` | 镜像与 TIM/DTIM 关系：0=不包含/1=含于TIM(OBM)/2=含于DTIM(Uboot/Kernel/TOS) |
| BLF 配置项 | `3_Image_Path` / `4_Image_Path` | OBM 镜像路径配置项，product-build 时替换为 `*_ProductBuild.bin` |
| 编译宏 | `PRODUCT_BUILD=1` | 编译 OBM 时使能，将 OEM 公钥哈希写入 fuse[2] 进行熔断 |
| 工具 | RSAKeyGenerator.exe / RSAKeyGeneratorCmd | 生成 RSA 密钥对，输出 `output_key.blf` |
| 工具命令 | `RSAKeyGenerator.exe -h SHA-256 -s 2048 -f output_key.blf` | Windows 下生成密钥 |
| 工具命令 | `./RSAKeyGeneratorCmd -h SHA-256 -s 2048 -f output_key.blf` | Linux 下生成密钥 |
| 工具 | SWDownloader（V4.9.1.3） | 读取/打开 BLF 文件、对镜像签名并烧录到 flash；需联系移远技术支持获取 |
| 工具 | FBFMake.exe（V5.0.0.0） | 对镜像签名生成下载包，避免暴露私钥 |
| 工具命令 | `.\FBFMake.exe -r <update.blf绝对路径> -f <输出目录>` | 生成签名下载包 BinFile.bin |
| 工具 | FBFDownloader.exe（V5.0.0.0） | 下载已签名的下载包到模块 |
| 工具命令 | `.\FBFDownloader.exe -b <BinFile.bin路径>` | 下载固件 |
| 关键内存地址 | fuse[2] / efuse Bank0~Bank3 | 存储第一组公钥哈希（合法性校验根） |
| 关键文件 | TIMH | 第一组密钥签名结果，含 OBM 哈希信息 |
| 关键文件 | DTIM.Primary | 第二组密钥签名结果，含 Uboot/Kernel/只读文件系统等哈希信息 |
| 关键文件 | update.blf | 固件包最终 BLF 配置文件，路径 `/update/update.blf` |
| 关键目录 | `/marvell/swd/FALCON/` | 源码中 BLF 文件存放目录 |
| 关键目录 | `marvell/swd/tools/RSAKeyGenerator/` | RSA 密钥生成工具目录 |
| 关键目录 | `marvell/swd/tools/FBFMake/` | FBFMake / FBFDownloader 工具目录 |
| 启动日志关键字 | `Trusted OBM without ProductBuild` | 未执行 FUSE 熔断时的启动日志标识 |
| 启动日志关键字 | `Trusted OBM with ProductBuild` | 已执行 FUSE 熔断时的启动日志标识 |

## 五、完整操作流程还原

### A. Trusted Boot 启动期校验流程（运行时，模块固件内部行为，不可由用户干预，仅供理解）

1. 模块复位/上电 → BootROM 运行
   1. 从 flash 读取 TIM
   2. RSA 校验 TIM 签名
   3. 计算 TIM 公钥哈希，与 fuse[2] 中保存的哈希比较，确认公钥合法性
   4. 通过 TIM 中保存的 OBM 镜像信息，加载并验证 OBM（BootLoader）
   5. 控制权移交给 OBM
2. OBM 运行
   1. 读取 TIM
   2. 通过 TIM 信息查找 OS Loader / TOS 镜像，若找到跳到 (5)，否则继续 (3)
   3. 加载 DTIM.Primary 到内存
   4. 先校验 DTIM.Primary 公钥，再校验 DTIM.Primary 本身
   5. 通过 DTIM.Primary 信息，加载并校验 OS Loader（Uboot），若有 TOS 同时加载校验 TOS
   6. 若有 TOS，OBM 将控制权交给 TOS，TOS 初始化完毕后交给 Uboot；否则 OBM 直接交给 Uboot
3. Uboot 运行
   1. 读取 DTIM.Primary
   2. 加载并校验 kernel
   3. 控制权移交给 kernel
4. kernel 运行，完成文件系统挂载

### B. 用户开启 Trusted Boot 的操作流程（开发/生产阶段，用户可执行的步骤）

1. **配置 BLF 文件**（在源码 BLF 或固件包 `update.blf` 中）：
   1. 添加 `Trusted = 1`，使能 Trusted Boot
   2. 添加 `FUSE` / `Enabled = 0x00000001` / `End_FUSE`，使能 FUSE 熔断
2. **编译 OBM**：确认源码已配置 `PRODUCT_BUILD=1`（默认已配置），得到 `TLoader_QSPINAND_ProductBuild.bin`；将 `update.blf` 中 `3_Image_Path`、`4_Image_Path` 的镜像路径替换为该 ProductBuild 版本
3. **（可选）生成自定义 RSA 密钥**：
   1. 使用 RSAKeyGenerator 工具生成密钥对，得到 `output_key.blf`
   2. 将 `output_key.blf` 内容插入 `update.blf` 中 `End_Extended_Reserved_Data` 与 `[Image_List]` 之间
4. **镜像签名**：使用 SWDownloader 工具打开配置好的 BLF 文件（自动间接通过 TIM/DTIM 完成签名），点击绿色图标完成签名并准备烧录到 flash
5. **烧写验证**：下载固件后重启模块，通过串口查看启动日志，确认输出 `Trusted OBM with ProductBuild`（已熔断）或 `Trusted OBM without ProductBuild`（未熔断），并核对 efuse Bank2 等寄存器值不为 0
6. **（生产阶段，避免私钥暴露）使用 FBFMake + FBFDownloader 两段式烧录**：
   1. `FBFMake.exe -r <update.blf路径> -f <输出目录>` 生成已签名的 `BinFile.bin`，避免在生产现场暴露 `update.blf` 中的 TIM/DTIM 私钥
   2. 将 `BinFile.bin` 分发给生产现场，使用 `FBFDownloader.exe -b <BinFile.bin路径>` 下载固件到模块（USB 方式，WTPTP 设备）

## 六、与本项目（open_dial）的潜在关联点

经核对项目 CLAUDE.md 中描述的 `open_dial` 拨号管理程序架构与本 PDF 内容，**两者无直接代码层面关联**，原因如下：

- `open_dial` 是运行在 Quectel **EC2x/EG2x** 模组（以及部署文档提到 AG35 也属于支持范围）上的**用户态拨号管理应用程序**，工作在 Linux kernel 之上、通过 MCM/QMI API 及 AT 口与基带交互，关注的是数据连接建立、SIM/Roamlink 双通道切换等业务逻辑
- 本 PDF 描述的 **Trusted Boot** 是芯片级/固件级的安全启动机制，工作在 **BootROM → OBM → Uboot → Kernel** 这一引导链路上，发生在 `open_dial` 进程启动**之前**（kernel 尚未运行，文件系统尚未挂载），二者运行在完全不同的层次

**间接关联/可参考之处**：
1. **设备生命周期认知**：若部署 `open_dial` 的设备开启了 Trusted Boot 并已熔断 FUSE，那么后续任何固件升级（包括可能携带 `dial` 二进制的根文件系统/squashfs 镜像）都必须经过 TIM/DTIM 签名流程，否则会被 OBM/Uboot 拒绝加载，导致设备无法启动。这与项目 README 中提到的固件升级、`roamlink_deploy` 打包 `dial` 二进制等运维场景有关——**如果目标设备启用了 Trusted Boot，则 `roamlink_deploy` 打包的固件镜像必须按本文档流程签名后才能烧录**，否则会出现"固件烧录失败"（文档原文备注1：已熔断 FUSE 但 BLF 未加 `Trusted=1` 会导致烧录失败）
2. **效率影响排查方向**：若未来 `open_dial` 所在设备出现"刷机失败""固件无法启动"等问题，且设备本身开启了 Trusted Boot/FUSE 熔断，应排查固件签名链是否完整（TIM/DTIM 签名、BLF 配置是否正确），而非仅排查 `open_dial` 自身逻辑
3. **当前项目代码本身不涉及** Trusted Boot 校验逻辑——`open_dial` 没有任何代码读取 TIM/DTIM、efuse 或调用 SWDownloader/FBFMake 工具，CLAUDE.md 中也未提及 Trusted Boot 相关内容
4. 模块型号差异提醒：本 PDF 针对的是 **AG35-CET/AG35-EUT**（ASR1806 平台），而 `open_dial` CLAUDE.md 主要提及 **EC2x/EG2x**（高通平台，使用 QMI/DSI API，如 `QL_Data_Call_Start()`、`QL_MCM_NW_Client_Init()`）。两者底层芯片平台不同（ASR vs 高通），Trusted Boot 实现细节（efuse、BootROM、OBM 等机制）很可能因平台而异，本文档的具体操作步骤（BLF 文件格式、FUSE 寄存器位置等）**不能直接套用于 EC2x/EG2x 平台**，仅供安全机制原理参考

**结论**：与本项目无直接代码关联，但在涉及固件烧录、设备安全启动链路、生产部署流程时具有背景知识参考价值。

## 七、文档局限性、未说明清楚之处及已知问题

1. **版本状态为"临时版本"（Preliminary）**，文档历史仅记录创建当天即发布临时版，未经后续修订，可能存在未完善之处
2. **DAP（Debug Access Port）术语在表2中列出，但正文全文未提及其用途**，文档未说明其与 Trusted Boot 的关系（可能与调试端口禁用相关的安全策略有关，但文档未展开）
3. **"DTIM.Primary" 命名中的 "Primary" 后缀未被解释**——是否存在 "DTIM.Secondary" 或其他变体，文档未说明
4. **FBFMake/FBFDownloader 工具在 Linux 系统下的具体文件名未列出**——文档仅给出一条 `ls` 命令但未展示其输出结果（"ol@ol:~/code/asr1806/marvell/swd/tools/FBFMake$ ls" 后紧跟章节标题，没有文件清单），而 Windows 系统下列出了 `fbfdownloader`、`FBFDownloader.exe`、`fbfmake`、`FBFMake.exe`
5. **FUSE 熔断的不可逆性未在文档中明确警示**——文档描述了熔断流程（写入 fuse[2]）和验证方法，但未提及 FUSE 熔断是否可逆、熔断错误密钥后的恢复方案，这是生产场景中的高风险操作点，文档未充分提示风险
6. **`output_key.blf` 自定义密钥与默认密钥的关系未说明**——文档未解释如果不执行 3.2 节自定义密钥步骤，系统使用的是什么默认密钥，密钥来源及管理流程未在文档范围内
7. **关于 SWDownloader 工具获取方式**，文档两次提及"请联系移远通信技术支持获取"（备注2 及 3.4 节），说明该工具不随 SDK 公开发布，存在获取门槛，文档本身未提供下载方式
8. **图1 流程图中 OBM 与 TOS 之间为双向虚线箭头**，但正文叙述的实际控制流为单向（OBM→TOS→Uboot，或 OBM→Uboot 不经过 TOS），图示的双向虚线箭头含义文档未做图注解释（可能表示 OBM 加载 TOS 与 TOS 返回控制权两个方向，但未明确标注）
9. 文档未提供任何关于 **Trusted Boot 失效/绕过的安全风险评估**、**密钥泄露后的补救措施**、或 **生产环境中私钥管理的最佳实践**（仅说明 FBFMake/FBFDownloader 可避免在生产现场暴露 update.blf 中的私钥，但未涉及密钥生成、存储、备份的安全规范）
10. 第2.2节步骤 2)b) 表述"若找到，则跳转至步骤 e，否则按顺序执行步骤 c"在逻辑上存在跳跃（步骤 c→d→e 是顺序的，但"找到"时跳到 e 略过 c、d 即跳过 DTIM.Primary 加载校验），文档未解释这种情况下 OS Loader/TOS 镜像信息从何处获得校验依据（可能是因为此时这些镜像信息已直接保存在 TIM 中而非 DTIM 中，但文档未明确说明这种"找到"分支下镜像合法性如何保证）

## 八、总结

本文档是一份**面向产线/工程师的操作指导文档**，系统介绍了 Quectel AG35-CET/AG35-EUT 模块（ASR1806 平台）的 Trusted Boot 安全启动机制，包括：
- 双密钥（TIM/DTIM）+ RSA2048/SHA256 签名校验体系
- BootROM→OBM→(TOS)→Uboot→Kernel 的完整可信启动链
- 通过 BLF 文件配置 `Trusted=1` 和 FUSE 熔断来开启该功能的具体步骤
- RSA 密钥生成、镜像签名、以及为保护私钥而设计的 FBFMake/FBFDownloader 两段式生产烧录方案

文档结构完整、步骤具体（含命令示例和日志截图），但作为"临时版本"在部分细节（如 DAP 用途、FUSE 不可逆风险提示、密钥管理规范）上说明不够充分。该机制与 `open_dial` 项目本身的拨号管理逻辑无直接代码关联，分属不同抽象层次（固件安全启动 vs 用户态网络应用），但在涉及目标设备固件烧录/升级运维场景时具有背景参考价值。

<!-- GENERATION_COMPLETE: 2026-06-24 -->
