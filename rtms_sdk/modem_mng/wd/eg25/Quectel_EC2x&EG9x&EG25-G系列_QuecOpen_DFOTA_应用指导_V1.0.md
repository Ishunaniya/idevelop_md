# EC2x&EG9x&EG25-G 系列 QuecOpen DFOTA 应用指导

**LTE Standard 模块系列**

- 版本：EC2x&EG9x&EG25-G 系列_QuecOpen_DFOTA_应用指导_V1.0
- 日期：2020-07-31
- 状态：受控文件

---

## 公司及联系方式

上海移远通信技术股份有限公司始终以为客户提供最及时、最全面的服务为宗旨。如需任何帮助，请随时联系我司上海总部，联系方式如下：

**上海移远通信技术股份有限公司**
上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼　　邮编：200233
电话：+86 21 51086236　邮箱：info@quectel.com

或联系我司当地办事处，详情请登录：http://www.quectel.com/cn/support/sales.htm 。

如需技术支持或反馈我司技术文档中的问题，可随时登陆如下网址：
http://www.quectel.com/cn/support/technical.htm 或发送邮件至：support@quectel.com 。

### 前言

上海移远通信技术股份有限公司提供该文档内容用以支持其客户的产品设计。客户须按照文档中提供的规范、参数来设计其产品。因未能遵守有关操作或设计规范而造成的损害，上海移远通信技术股份有限公司不承担任何责任。在未声明前，上海移远通信技术股份有限公司有权对该文档进行更新。

### 免责声明

上海移远通信技术股份有限公司尽力确保开发中功能的完整性、准确性、及时性或效用，但不排除上述功能错误或遗漏的可能。除非其他有效协议另有规定，否则上海移远通信技术股份有限公司对开发中功能的使用不做任何暗示或明示的保证。在适用法律允许的最大范围内，上海移远通信技术股份有限公司不对任何因使用开发中功能而遭受的损失或损害承担责任，无论此类损失或损害是否可以预见。

### 版权申明

本文档版权属于上海移远通信技术股份有限公司，任何人未经我司允许而复制转载该文档将承担法律责任。

版权所有 ©上海移远通信技术股份有限公司 2020，保留一切权利。
Copyright © Quectel Wireless Solutions Co., Ltd. 2020.

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| 1.0 | 2020-07-31 | 张威 / 张涛 / 赵崇宇 | 初始版本 |

---

## 目录

- 文档历史
- 目录
- 表格索引
- 图片索引
- 1 引言
  - 1.1. 适用模块
- 2 制作差分包
  - 2.1. 制作目标固件版本
  - 2.2. 生成差分包
  - 2.3. 生成 UBI 文件差分包
    - 2.3.1. 需要文件
    - 2.3.2. 文件路径
    - 2.3.3. 执行命令
- 3 检查差分包
- 4 差分固件升级流程
  - 4.1. 差分包导入
    - 4.1.1. ADB 方式导入
    - 4.1.2. RZ 方式导入
  - 4.2. 差分固件升级
    - 4.2.1. CMDLINE 升级
    - 4.2.2. APP 升级
  - 4.3. 差分固件升级流程图
- 5 问题排查
- 6 注意事项
- 7 附录 A 参考文档及术语缩写

### 表格索引

- 表 1：适用模块
- 表 2：URC 描述
- 表 3：参考文档
- 表 4：术语缩写

### 图片索引

- 图 1：替换 boot.img 和 recovery.img 文件
- 图 2：替换 rootfs 文件
- 图 3：需要文件名称
- 图 4：v1 目录与 v2 目录下的文件
- 图 5：差分包制作工具版本信息
- 图 6：差分包中的差分包制作工具版本信息
- 图 7：ADB 命令查询到的设备
- 图 8：DFOTA 升级流程图

---

## 1 引言

移远通信 LTE Standard EC2x&EG9x&EG25-G 系列模块支持 QuecOpen® 方案。本文档主要介绍在 QuecOpen® 方案下，如何通过 DFOTA 功能升级上述模块的固件。

DFOTA 功能即通过差分包可实现固件升级或降级。所述差分包仅包含当前固件版本和目标固件版本之间的差异，因此数据传输量大大降低、传输时间大大缩短。

### 1.1. 适用模块

**表 1：适用模块**

| 模块系列 | 模块 |
|---|---|
| EC2x 系列 | EC25 系列 |
| EC2x 系列 | EC21 系列 |
| EC2x 系列 | EC20 R2.1 |
| EC2x 系列 | EC20-CN |
| EG9x 系列 | EG95 系列 |
| EG9x 系列 | EG91 系列 |
| EG25-G | EG25-G |

---

## 2 制作差分包

### 2.1. 制作目标固件版本

目标固件包中 `/upgrade` 目录下的 `targetfiles.zip` 文件为所有原始文件的压缩文件，用于制作差分包。在开发过程中，如果 Openlinux 客户修改了 `boot.img` 和 `rootfs` 文件，则修改后的文件与 `targetfiles.zip` 里的原始文件不同，会导致差分包无法升级。因此，在发布目标固件版本之前，必须同时更新 `targetfiles.zip`。制作 `targetfiles.zip` 的流程如下：

1. **解压 targetfiles.zip 到 /target 目录**

   将原始 `targetfiles.zip` 复制到 Ubuntu 系统里解压，执行如下命令将 `targetfiles.zip` 解压到 `/target` 目录里。

   ```bash
   unzip -d ./target ./targetfiles.zip
   ```

2. **替换 targetfiles 里的 boot.img 和 recovery.img 文件**

   使用新生成的 `mdm9607-perf-boot.img` 文件替换上述解压后 targetfiles 文件的 `/BOOTABLE_IMAGES` 目录下的 `boot.img` 和 `recovery.img` 文件，替换后将文件名改为 `boot.img`。

   > **图 1：替换 boot.img 和 recovery.img 文件** —— 在 `/BOOTABLE_IMAGES` 目录下用新的 `mdm9607-perf-boot.img` 替换 `boot.img` 与 `recovery.img`，替换后统一命名为 `boot.img`。

3. **替换 rootfs 文件**

   打开解压后的原始 targetfiles 文件的 `/target` 目录，删除 `/SYSTEM` 目录下除 `/firmware` 目录外所有文件和目录，即仅保留 `/firmware` 目录。随后，将在 SDK 包里编译后的 `/ql-ol-rootfs` 目录里的文件（删除 `/firmware` 目录）复制到 `/SYSTEM` 目录。

   > **图 2：替换 rootfs 文件** —— `/SYSTEM` 目录仅保留 `/firmware`，再将编译后的 `/ql-ol-rootfs`（删除 `/firmware`）文件复制进 `/SYSTEM`。

4. **重新打包生成 targetfiles.zip 包**

   进入 `targetfiles.zip` 解压后的 `/target` 目录下执行如下打包命令，在 `/target` 目录的上层目录下即可生成重新制作的 `targetfiles.zip`。

   ```bash
   zip -qry0 ../targetfiles.zip ./*
   ```

   > **备注**
   >
   > 生成压缩包时，不能直接使用打包工具打包文件，一定要使用打包命令生成压缩包。

### 2.2. 生成差分包

工作环境：实测环境 Ubuntu 14.04 64 bit。生成差分包的流程如下，以 EC20 R2.1 为例：

1. **解压 EC20-delta-gentools**

   执行如下命令，解压 EC20-delta-gentools。

   ```bash
   tar -xzf EC20-delta-gentools.tar.gz
   ```

2. **环境配置**

   - 第一步：首次执行前需配置其环境，执行如下命令进行环境配置（若环境已配置则可忽略此步骤）：

     ```bash
     cd EC20-delta-gentools
     ```

   - 第二步：执行如下命令安装 LZO 库以防出现 `No module named lzo` 的报错信息。

     ```bash
     sudo apt-get install liblzo2-dev
     sudo pip install python-lzo
     ```

   - 第三步：拷贝 `cd` 命令运入的目录下的库文件到 `/usr/lib` 中以防出现 `bsdiff: error while loading shared libraries: libbz2.so.0: cannot open shared object file: No such file or directory` 的报错信息：

     ```bash
     sudo cp libbz2.so.0* /usr/lib/
     ```

   - 第四步：在 `/EC20-delta-gentools` 目录下，执行如下命令，赋予文件 `bsdiff`、`imgdiff`、`ota_from_target_files` 和 `update_gen.sh` 可执行权限：

     ```bash
     chmod 777 bsdiff imgdiff ota_from_target_files update_gen.sh
     ```

3. **将 targetfiles.zip 文件放至相应目录**

   进入 `/EC20-delta-gentools` 目录，将当前使用的固件包和目标固件包里的 `/upgrade` 目录下的 `targetfiles.zip` 文件分别放到 `/v1` 和 `/v2` 目录下。

   > **备注**
   >
   > `/v1` 目录放置模块当前运行版本的 `targetfiles.zip`；`/v2` 放置模块目标版本的 `targetfiles.zip`。

4. **生成差分包并将其放至相应服务器**

   执行如下命令，将在 `/EC20-delta-gentools` 目录下生成最终的 `update.zip` 差分升级包，将其放至相应的 HTTP、FTP 服务器即可。

   ```bash
   ./update_gen.sh a
   ```

   > **备注**
   >
   > 上述命令的可选参数：`<m：modem；l：linux；o：boot；a：all>`，如需全选，输入 `a` 参数即可。

### 2.3. 生成 UBI 文件差分包

当前差分包制作工具已经支持直接对 UBI 文件进行差分升级。生成 UBI 文件差分包的流程如下：

#### 2.3.1. 需要文件

生成 UBI 差分包所需要的文件为固件包中 `/update` 目录下的四个文件：`mdm9607-boot.img`、`mdm9607-sysfs.ubi`、`mdm9607-recovery.ubi` 和 `NON-HLOS.ubi`，分别对应 Linux 内核镜像、主文件系统、备份文件系统与 Modem 文件系统。虽然四个文件名称在不同项目中可能不完全一致，但只需找到对应功能的文件即可。

各分区烧录文件的命名规则为：

- **system 分区**：`mdm.*sysfs.ubi`
- **modem 分区**：`NON-HLOS.ubi`
- **boot 分区**：`mdm.*boot.img`
- **recovery 分区**：`mdm.*recovery.*.ubi`

其中，`*` 可以匹配任意字符或为空。

> **图 3：需要文件名称** —— 展示 `/update` 目录下四个所需文件的名称。

#### 2.3.2. 文件路径

进入差分包制作工具目录，当前 `/v1` 目录与 `/v2` 目录需分别存放上述四个文件：`/v1` 目录为源版本文件存放目录，即模块当前烧录并运行的文件；`/v2` 目录为待升级文件的存放目录。如果更新了 Linux 内核镜像或主文件系统，则需要更新 `/v2` 目录下的对应文件。

> **图 4：v1 目录与 v2 目录下的文件** —— `/v1`（源版本）与 `/v2`（目标版本）目录下分别存放的四个 UBI/镜像文件。

#### 2.3.3. 执行命令

执行如下命令，对比 `/v2` 目录与 `/v1` 目录下的四个文件的差异，基于 `/v1` 目录下的文件，将会生成最终的差分包 `update.zip`，将其放入 HTTP、FTP 服务器即可。

```bash
./update_gen.sh a.ubi
```

> **备注**
>
> 上述命令的可选参数为：`<m.ubi：modem；l.ubi：linux；o.ubi：boot；a.ubi：all>`，如需全选，输入 `a.ubi` 参数即可。

---

## 3 检查差分包

获取当前使用的差分包制作工具版本信息和生成的差分包中的差分包制作工具版本信息，通过核对差分包制作工具的版本信息，检查差分包是否由正确的差分包制作工具生成。差分包制作工具版本信息的获取方式如下：

1. **获取当前使用的差分包制作工具版本信息**

   执行如下命令，即可获取当前使用的差分包制作工具的版本信息：

   ```bash
   ./update_gen.sh --version
   ```

   > **图 5：差分包制作工具版本信息** —— 执行 `--version` 后终端显示的工具版本信息。

2. **获取差分包中的差分包制作工具版本信息**

   执行如下命令，解压差分包：

   ```bash
   unzip -d ./update ./update.zip
   ```

   执行如下命令，即可获取差分包中的差分包制作工具的版本信息：

   ```bash
   head -5 update/META-INF/com/google/android/updater-script
   ```

   > **图 6：差分包中的差分包制作工具版本信息** —— `updater-script` 头部显示的差分包内工具版本信息。

---

## 4 差分固件升级流程

### 4.1. 差分包导入

`update.zip` 差分包检查完成之后，需将生成的 `update.zip` 差分包导入到模块中，用于差分升级。常用的导入方法有 ADB 以及 RZ。

#### 4.1.1. ADB 方式导入

模块开机后，插上 USB 线连接电脑。打开电脑的 cmd 窗口，通过 `adb devices` 命令查询是否存在该设备。如果当前无 ADB 命令，可以通过网上获取，配置全局变量即可使用。如果设备已连接，则查询结果如下图所示。

> **图 7：ADB 命令查询到的设备** —— `adb devices` 命令列出已连接设备的终端输出。

此时，可以通过 `adb push "xxx\update.zip" "/usrdata/"` 命令将生成的 `update.zip` 差分包导入至模块的 `/usrdata` 目录。使用 ADB 方式导入文件，文件传输速度很快，推荐使用该方式导入差分包。

#### 4.1.2. RZ 方式导入

模块开机后，进入 `/usrdata` 目录下，输入 `rz -E` 命令之后会弹出选择框，选择 `update.zip` 文件即可导入，或者直接把 `update.zip` 文件拖到终端命令框里。使用 RZ 方式导入文件，文件传输速率很慢，不推荐使用该方式导入差分包。

### 4.2. 差分固件升级

#### 4.2.1. CMDLINE 升级

最终差分包会被保存至 `/usrdata/cache/fota/ipth_package.bin` 路径下的 `ipth_package.bin` 文件中，所以 CMDLINE 升级方式也是先下载差分包，再将差分包保存至路径 `/usrdata/cache/fota/ipth_package.bin`。如果想要查看升级进度，还需要配置模块的文件系统 recoveryfs 中的 AT 口进行进度汇报。在下载或者保存差分包时，必须保证 usrdata 分区有足够的空间，即 usrdata 分区的可用空间必须大于差分包的大小，否则会导致差分包下载或保存失败。

CMDLINE 的具体升级过程分为三步，详见第 4.2.1.1 章，第 4.2.1.2 章，第 4.2.1.3 章。

##### 4.2.1.1. URC 口配置

在升级过程中，如果需要实时查看升级进度，需配置相应的 URC 口，具体方法如下。

1. **挂载 recoveryfs**

   使用如下命令挂载 recoveryfs：

   ```bash
   mtdnum=`cat /proc/mtd | grep -w recoveryfs | awk -F["d"":"] '{print $2}'`
   ubiattach -m $mtdnum -d 3 /dev/ubi_ctrl
   mkdir -p /tmp/mount_recovery
   mount -t ubifs /dev/ubi3_0 /tmp/mount_recovery -o bulk_read
   ```

2. **配置 URC 口**

   使用如下命令配置 URC 口：

   ```bash
   rm /tmp/mount_recovery/sbin/usb/boot_hsusb_composition
   ln -s /sbin/usb/compositions/recovery_9607 \
   /tmp/mount_recovery/sbin/usb/boot_hsusb_composition
   ```

##### 4.2.1.2. 移动差分包

移动差分包时，确保原有路径已经存在，并且该路径下无旧差分包和 log 文件，最后确保文件写入 flash，过程如下。

使用如下命令移动差分包：

```bash
mkdir -p /usrdata/cache/fota
rm -rf /usrdata/cache/fota/*
mv 本地差分包 /usrdata/cache/fota/ipth_package.bin
sync "/sys/bus/msm_subsys/devices/subsys1/restart_level"
```

##### 4.2.1.3. 启动升级程序

在控制台中，DFOTA 升级程序有两种启动方式：

- `reboot-recovery`
- `sync && sys_reboot recovery`（在重启前增加 sync 操作，防止文件没有完全写入 flash 导致 DFOTA 升级失败。）

以上两种方式没有本质区别。命令执行后，即可启动 DFOTA 升级程序。

> **备注**
>
> 1. 第 4.2.1.1 章描述的步骤可以省略，因为不配置 URC 口对升级程序没有影响，只是为了查看升级进度。
> 2. 第 4.2.1.2 章描述的步骤亦可以省略，如果在 `/usrdata/cache/fota/ipth_package.bin` 路径下的 `ipth_package.bin` 文件存在且文件正确，可以直接使用 `reboot-recovery` 命令进行 DFOTA 升级。
> 3. 第 4.2.1.3 章描述的步骤不能省略。

#### 4.2.2. APP 升级

移远通信在系统里添加了一键升级方式的 APP 命令，具体命令为 `qlfotapp`，相对于前一种升级方法，该方法优点是一键化，简单化，使用方法如下。

执行 `qlfotapp ./location_file` 命令，即可启动 DFOTA 升级程序，`location_file` 为本地差分文件参数。

命令执行后，如果差分包存在，系统就会自动重启进入 DFOTA 升级，此后的过程就和 CMDLINE 升级过程一样。

> **备注**
>
> 由于该命令目前仅支持部分版本，所以在使用前，请确认所使用的版本是否存在 `qlfotapp` 命令。

### 4.3. 差分固件升级流程图

下图阐述了 DFOTA 升级的流程。

> **图 8：DFOTA 升级流程图** —— 整体流程分为三大泳道（“升级开始 / 上电、重启”、“DFOTA 升级开始 / 启动 recovery 程序”、“升级开始 / 读升级脚本”），关键节点与判断如下：
>
> - **左侧（Boot / 启动判定）**：升级开始 → 上电/重启 → 判断“重试次数 > 5”：
>   - 若是（Y）：清除 boot flag 并重启；判断 `Boot flag == recovery`：
>     - 否（N）→ 正常启动；
>     - 是（Y）→ DFOTA 升级。
>   - 如果由于 5 次升级失败等原因导致的某个分区不能正常启动，程序则会从相应的备份分区还原该分区。
> - **中间（recovery 升级程序，自此后的 DFOTA 升级都是该程序完成的）**：DFOTA 升级开始 → 启动 recovery 程序 → 开始升级 → 判断“升级文件存在？解压成功？”：
>   - 是（Y）→ 启动子进程升级 → 通过管道进行进程间通信完成 URC 上报等 → 判断“升级正常？”：
>     - 是（Y）→ 升级备份分区 → 判断“备份分区正常升级完成” → URC 上报 END 0 → DFOTA 升级结束；
>     - 否（N）→ URC 上报 ERROR TYPE。
> - **右侧（升级脚本 / 差分升级与校验）**：升级开始 → 读升级脚本 → 进行差分升级 → 判断“源文件 hash 校验成功？目标缓存文件校验成功？”：
>   - 是（Y）→ 设置升级进度；
>   - 否（N）→ 使用目标缓存文件替换目标文件；若失败则返回失败信息。
> - 流程末端：升级结束 / DFOTA 升级结束 / 升级结束。

> **备注**
>
> 上图的 DFOTA 升级流程，只针对有备份区的模块（内存容量为 256 M + 256 M / 512 M + 256 M）。而对于没有备份区的模块（内存容量为 128 M + 128 M），如果没有升级成功，则会一直升级，且不会退出升级。

---

## 5 问题排查

一般来说，在 DFOTA 升级过程中，升级时间和差分包的大小是成正比的。对于通过 HTTP(S)/FTP 方式下载的差分包，建议在升级前使用 `md5sum` 命令检查一下差分包的完整性。

在升级过程中，由于各种无法预知情况的发生，如 flash 的稳定性发生变化、温度环境发生变化、随机断电等，不能保证每一次升级都能完全成功。对于有备份区的模块，在整个升级过程中，硬件尝试 5 次升级，如果 5 次均升级失败，则整个升级过程失败。这时模块会触发还原，版本会被还原为备份分区的版本，且无论在哪个阶段升级失败，模块最终都会自动重启并进入正常状态。而对于无备份区的模块（特指 128 M + 128 M），若一次升级失败，则会一直升级。

在升级过程中，除了上报正常的 URC 值、升级的百分比、FOTA START 和 FOTA END 以外，如果发生异常，导致升级失败，URC 也会上报相应的异常 URC 值，上报格式如下：

```
+QIND: "FOTA","START"
+QIND: "FOTA","UPDATING",<percent>
+QIND: "FOTA","UPDATING",<percent>
...
+QIND: "FOTA","END",<err>
```

下表列举出了一些常见的 URC 上报值所代表的意义。

**表 2：URC 描述**

| URC 值 | 描述 |
|---|---|
| 0 | 升级成功 |
| 1 | 升级备份区失败，常见于 flash 空间不足，导致相应 ubi 文件生成失败。 |
| 501 | 升级开始，该 URC 一般是差分包不能正常解压，或者解压所需的空间不足导致。 |
| 502 | 升级中，该 URC 一般是升级程序异常退出导致。 |
| 510 | 升级中，文件校验失败。 |
| 511 | 升级中，升级时所需的空间不足。 |
| OK | 命令执行成功（仅适用 AT 升级） |
| ERROR | 命令格式错误（仅适用 AT 升级） |

---

## 6 注意事项

如下为 DFOTA 升级前和升级后的注意事项：

- DFOTA 升级前，需下载相应的差分包并保存在本地，并确保差分包的正确性。
- DFOTA 升级后，由于客户增加的文件存在权限被更改的可能，后续会在制作差分包工具中增加配置权限列表，便于客户配置。

---

## 7 附录 A 参考文档及术语缩写

**表 3：参考文档**

| 序号 | 文件名称 | 备注 |
|---|---|---|
| [1] | Quectel_EC2x&EG9x&EM05_DFOTA_User_Guide | EC2x&EG9x&EM05 DFOTA 用户指导 |

**表 4：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| ADB | Android Debug Bridge | 安卓调试桥 |
| APP | Application | 应用程序 |
| DFOTA | Delta Firmware Upgrade Over-The-Air | 无线差分固件升级 |
| FOTA | Firmware Over-The-Air | 固件空中升级 |
| FTP | File Transfer Protocol | 文件传输协议 |
| HTTP | Hyper Text Transfer Protocol | 超文本传输协议 |
| LZO | Lempel-Ziv-Oberhumer | 数据压缩算法 |
| SDK | Software Development Kit | 软件开发包 |
| UBI | Unsorted Block Images | 无序区块镜像 |
| URC | Unsolicited Result Code | 未经请求的结果代码 |
