# Linux 系统使用手册

## 文档基本信息
- **文档标题**：Linux 系统使用手册
- **厂商**：广州创龙科技（Tronlong）
- **版本号**：V1.0（2025/04/03 初始版本）
- **总页数**：96 页
- **适用平台**：RK3576 / TL3576-EVM 评估板
- **适用开发环境**：
  - Windows：Windows 7 64bit、Windows 10 64bit
  - Linux：VMware16.2.5、Ubuntu22.04.5 64bit
  - U-Boot：U-Boot-2017.09
  - Kernel：Linux-6.1.115
  - Buildroot 文件系统：Buildroot-2024.02
  - LinuxSDK：基于 rk3576_linux6.1_release_v1.1.0
  - 交叉编译工具链：Buildroot/应用开发用 `aarch64-buildroot-linux-gnu-gcc-12.4.0`；U-Boot/内核开发用 `gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu`
- **软件包位置**：产品资料 "4-软件资料\Linux\"，含 U-Boot、Kernel、Filesystem、LinuxSDK、updateimg、Tools 文件夹。

### 文档目录结构
1. 前言
2. 第1章 LinuxSDK 安装（1.1 安装依赖软件、1.2 解压 LinuxSDK、1.3 解压 dl.tar.gz、1.4 系统开发环境配置、1.5 应用开发环境配置）
3. 第2章 Linux 系统镜像编译、生成（2.1 配置编译选项、2.2 编译 LinuxSDK、2.3 局部编译）
4. 第3章 rootfs 根文件系统修改说明
5. 第4章 Linux 系统文件替换说明
6. 第5章 U-Boot 命令和环境变量说明
7. 第6章 文件系统中文支持说明
8. 第7章 系统信息查询
9. 第8章 文件系统组件安装说明
10. 第9章 内存分配说明
11. 第10章 程序开机自启动说明
12. 第11章 CPU 主频调节说明
13. 第12章 TFTP 使用说明
14. 第13章 NFS 使用说明
15. 第14章 基于 TFTP + NFS 的系统启动说明
16. 第15章 Linux 设备驱动说明
17. 更多帮助

---

## 逐章节内容摘要

### 前言
- 先按《Linux 开发环境搭建》文档搭建开发环境。系统编译需下载软件工具包，确保上位机 Ubuntu 可正常访问互联网。
- 文件清单（表1/表2）：
  - U-Boot：image\xxx（镜像）、src（源码压缩包）、特性支持说明文件。
  - Kernel：image\xxx、src、特性支持说明文件。
  - Filesystem（buildroot-2024.02）：image\xxx、src、特性支持说明文件。
  - updateimg：update-[版本号].img（Linux 系统镜像）。
  - LinuxSDK：LinuxSDK-[版本号].tar.gz（基于 rk3576_linux6.1_release_v1.1.0）；rk3576-buildroot-2024.02-sysroot-[版本号].tar.gz（含程序运行库、Qt 库及交叉编译工具链）；dl.tar.gz（官网下载的开源软件包，缩减编译时间）；特性支持说明文件。
  - Tools：LZ4_old1-dev.zip（快速压缩解压）；gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu.tar.gz（U-Boot/内核交叉编译工具链）。
- 系统开发用 LinuxSDK + gcc-arm-10.3 工具链；应用开发用 LinuxSDK + rk3576-buildroot-sysroot。备注：应用开发工具链含在 LinuxSDK 编译后才生成，也可解压 sysroot 协同使用。

### 第1章 LinuxSDK 安装

#### 1.1 安装依赖软件
- （1）安装库和工具集：
  - `sudo apt-get install -y git ssh make gcc libssl-dev liblz4-tool expect expect-dev g++ patchelf chrpath gawk texinfo chrpath diffstat binfmt-support qemu-user-static live-build bison flex fakeroot cmake gcc-multilib g++-multilib unzip device-tree-compiler ncurses-dev libgucharmap-2-90-dev bzip2 expat gpgv2 cpp-aarch64-linux-gnu libgmp-dev libmpc-dev bc python-is-python3 python2`
  - 备注：保证 Ubuntu 可正常访问互联网，若提示 "*** is already the newest version ***" 表示已安装，忽略。
- （2）配置 Python 版本（创建软链接，Python 版本需 3.6.0 及以上）：
  - `sudo rm /usr/bin/python`
  - `sudo ln -s /usr/bin/python3 /usr/bin/python`
  - `ls -al /usr/bin/python*`、`python --version`（确认链接成功，图4 显示 Python 3.10.12）。
- （3）安装 LZ4（LZ4 版本需 v1.7.3 及以上）：
  - `unzip LZ4_old1-dev.zip`
  - `cd LZ4_old1-dev/` → `make` → `sudo make install` → `sudo install -m 0755 lz4 /usr/bin/lz4`
  - `lz4 -v`（图8 显示 v1.9.4）。

#### 1.2 解压 LinuxSDK
- LinuxSDK-[版本号].tar.gz 位于 "4-软件资料\Linux\LinuxSDK\"，基于 rk3576_linux6.1_release_v1.1.0。
- 创建工作目录：`mkdir -p /home/tronlong/RK3576`
- 拷贝并解压：`cd /home/tronlong/RK3576/` → `tar -zxf LinuxSDK-v1.0.tar.gz`（耗时约 5min，生成 rk3576_linux6.1_release 文件夹）。
- 查看目录：`cd rk3576_linux6.1_release/` → `ls -l`。
- **LinuxSDK 源码目录结构（表3）**：
  - app：上层应用 APP（应用 Demo）
  - buildroot：Buildroot 根文件系统
  - build.sh：编译镜像的脚本文件
  - Copyright_Statement.md：版权说明文件
  - debian：Debian 根文件系统
  - device/rockchip：芯片板级配置及编译/打包固件脚本
  - docs：官方参考资料
  - external：第三方仓库（显示、音视频、摄像头、网络、安全等）
  - hal：Bare-metal 系统源码
  - kernel：kernel-6.1 软链接文件
  - kernel-6.1：Linux 内核源码
  - Makefile：LinuxSDK 顶层 Makefile
  - output：编译生成的固件信息、编译信息、XML、主机环境等
  - prebuilts：交叉编译工具链
  - rkbin：Rockchip 相关二进制和工具
  - rkflash.sh：Linux 环境下系统固化脚本
  - rtos：RTOS 系统源码
  - tools：Linux 和 Windows 常用工具
  - u-boot：2017.09 版本 U-Boot 源码
  - yocto：Yocto 根文件系统

#### 1.3 解压 dl.tar.gz 压缩包
- dl.tar.gz 用于存放官网下载的开源软件包，Buildroot 编译时从该目录获取，节约下载时间、提高效率、避免网络问题导致编译失败。
- 拷贝至 "RK3576/rk3576_linux6.1_release/buildroot/" 目录，解压：`cd buildroot/` → `tar -zxf dl.tar.gz`。

#### 1.4 系统开发环境配置（U-Boot、内核开发）
- 在 LinuxSDK 源码目录下配置环境变量。备注：配置系统开发环境变量前，请勿配置应用环境变量，否则导致系统镜像编译失败。
- `export PATH=/home/tronlong/RK3576/rk3576_linux6.1_release/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/:$PATH`
- `aarch64-none-linux-gnu-gcc -v`（图14 显示 gcc version 10.3.1）。

#### 1.5 应用开发环境配置
- sysroot 压缩包含应用开发库（常用开发库、Qt 库等），搭配 LinuxSDK 使用。
- 拷贝 sysroot 压缩包至 "rk3576_linux6.1_release/" 目录，解压：`tar -zxf rk3576-buildroot-2024.02-sysroot-v1.0.tar.gz`（解压后在 buildroot 目录下增加 output 目录，含 sysroot 文件）。
- 备注：如需进行系统开发，请将 output 目录删除，否则导致 LinuxSDK 编译失败。
- 应用开发交叉编译工具链位于 "buildroot/output/rockchip_rk3576/host/bin/" 目录。
- 配置应用环境变量。备注：执行后当前 Terminal 不再适用 U-Boot、内核开发，如需系统开发请另开 Terminal。
  - `source /home/tronlong/RK3576/rk3576_linux6.1_release/buildroot/output/rockchip_rk3576/host/environment-setup`
  - `aarch64-buildroot-linux-gnu-gcc -v`（图20 显示 gcc version 12.4.0 Buildroot 2024.02）。

### 第2章 Linux 系统镜像编译、生成

#### 2.1 配置编译选项
- **2.1.1 配置 LinuxSDK 编译选项**：
  - 进入 LinuxSDK 源码目录，`./build.sh lunch:tl3576_evm_defconfig`（图21 配置成功）。
- **2.1.2 配置设备树**：
  - 设备树文件位于 "kernel/arch/arm64/boot/dts/rockchip/" 目录。设备树文件（表4）：
    - `tl3576-evm.dts`：基础设备树，支持 TL3576-EVM 基本外设，不支持 LVDS LCD 显示。
    - `tl3576-lvds-1024x768.dts`：基于 tl3576-evm.dts 增加 LVDS LCD 显示支持，但不支持 MIPI LCD 显示。
  - 设备树依赖关系（表5）：`tl3576-lvds-1024x768.dts` → `tl3576-evm.dts` → 引用 `rk3576.dtsi`、`rk3576-evb1.dtsi`、`rk3576-rk806.dtsi`、`rk3576-linux.dtsi`；rk3576.dtsi 又引用 `rk3576-pinctrl.dtsi`、`rockchip-pinconf.dtsi`；rk3576-evb1.dtsi 引用 `rk3576-evb.dtsi`、`rk3576-evb-input-keymap.dtsi`。
  - 当前默认编译 tl3576-evm.dts。如需替换为 LVDS LCD 设备树：
    - `vi device/rockchip/rk3576/tl3576_evm_defconfig`
    - 修改内容：注释 `#RK_KERNEL_DTS_NAME="tl3576-evm"`，添加 `RK_KERNEL_DTS_NAME="tl3576-lvds-1024x768"`。
    - 重新配置：`./build.sh lunch:tl3576_evm_defconfig`。
- **2.1.3 配置内核选项**：
  - 默认配置 Linux 内核（推荐）。修改内核编译选项执行 `make xxx menuconfig` 打开图形界面（读取当前目录 .config）。
  - menuconfig 需 ncurses 库支持：
    - `sudo apt-get install libncurses5-dev`
    - `sudo apt-get install libncursesw5-dev`
  - 打开图形配置界面：`./build.sh kernel-config`（图27 显示 Final configs，含 RK_KERNEL_DTS、RK_KERNEL_IMG=kernel/arch/arm64/boot/Image 等；图28 为 Linux 6.1.115 Kernel Configuration 菜单）。
  - 操作说明：方向键选菜单，`<Select>` 选中后 Enter 进入子菜单；蓝色高亮字母为快捷键；菜单项前括号表示配置状态；按 Y 编译进内核（变为 `<*>`），按 N 不编译，按 M 编译为模块（变为 `<M>`），按 `/` 搜索。
  - 配置完成选 `<Save>` 保存，`<Exit>` 退出。配置自动保存到 "kernel/arch/arm64/configs/rockchip_linux_defconfig"。
  - 如需配置 Linux-RT 内核，先备份：`cd kernel` → `cp arch/arm64/configs/rockchip_linux_defconfig arch/arm64/configs/rockchip_linux_defconfig-bak`。
  - 添加 RT 配置：`make ARCH=arm64 rockchip_linux_defconfig rockchip_rt.config`（在 rockchip_linux_defconfig 基础上叠加 rockchip_rt.config，合并 NO_HZ_FULL、PREEMPT_RT 等配置）。
  - 保存配置覆盖到 defconfig：`make ARCH=arm64 savedefconfig` → `mv defconfig arch/arm64/configs/rockchip_linux_defconfig`。
  - 恢复默认内核配置：`cd kernel` → `mv arch/arm64/configs/rockchip_linux_defconfig-bak arch/arm64/configs/rockchip_linux_defconfig`，然后参考“局部编译”章节编译内核。
- **2.1.4 配置 Buildroot 编译选项**：
  - `cd /home/tronlong/RK3576/rk3576_linux6.1_release/buildroot/`
  - `./envsetup.sh rockchip_rk3576`
  - `make menuconfig`（图36 为 Buildroot 2024.02 配置界面，含 Target options、Toolchain、Build options、System configuration、Kernel、Target packages、Filesystem images、Bootloaders、Host utilities、Legacy config options）。
  - 操作同 menuconfig（方向键、Enter、Y/N、`/` 搜索），`<Save>` 保存、`<Exit>` 退出。
  - 保存配置：`make savedefconfig`（保存至 "buildroot/configs/rockchip_rk3576_defconfig"）。

#### 2.2 编译 LinuxSDK
- 在 rk3576_linux6.1_release 目录执行 `./build.sh`，一键编译生成 U-Boot、Kernel、Buildroot、Recovery 等镜像，并打包生成 Linux 系统镜像。
- 预计耗时约 2 小时，最终在 rockdev 目录生成镜像。`ls rockdev/` 含 boot.img、MiniLoaderAll.bin、misc.img、oem.img、parameter.txt、recovery.img、rootfs.img、uboot.img、update.img、userdata.img。
- **镜像文件说明（表6）**：
  - boot.img：内核镜像，包含内核和设备树
  - MiniLoaderAll.bin：DDR 初始化镜像和 U-Boot SPL 镜像
  - misc.img：misc 镜像
  - oem.img：oem 镜像，存放用户数据，挂载至文件系统 "/oem" 目录
  - parameter.txt：分区表，update.img 按此分区表存放各个镜像
  - recovery.img：recovery 镜像，用于系统升级
  - rootfs.img：文件系统镜像
  - uboot.img：U-Boot 镜像
  - userdata.img：userdata 镜像，挂载至文件系统 "/userdata" 目录
  - update.img：Linux 系统镜像，包含上述所有镜像
- 参考《Linux 系统启动卡制作及系统固化》文档，将镜像固化至 Micro SD 卡或 eMMC。

#### 2.3 局部编译
- 如需单独编译 U-Boot、Linux 内核或文件系统，参考本章节。
- **2.3.1 U-Boot 编译**：先配置 LinuxSDK，再 `cd /home/tronlong/RK3576/rk3576_linux6.1_release/` → `./build.sh uboot`。编译完成在 rockdev 目录生成 MiniLoaderAll.bin 和 uboot.img。
- **2.3.2 内核编译**：先配置 LinuxSDK，`cd .../rk3576_linux6.1_release/` → `./build.sh kernel`。编译完成在 rockdev 目录生成 boot.img（图45 显示 FIT 镜像含 fdt、kernel、resource）。
- **2.3.3 文件系统编译**：`./build.sh buildroot`。备注：编译前确保已解压 dl.tar.gz 至 buildroot 目录。编译完成在 rockdev 目录生成 rootfs.img。
- **2.3.4 生成 update.img 镜像**：单独编译后，可重新打包生成 update.img。备注：替换 U-Boot/内核/文件系统镜像，可替换 "rk3576_linux6.1_release/output/firmware/" 目录下对应镜像，重新生成。`./build.sh firmware`。编译完成在 rockdev 目录生成 update.img。

### 第3章 rootfs 根文件系统修改说明
- 演示 rootfs 根文件系统镜像的修改、提取和镜像制作方法。
- 厂商提供的 Buildroot rootfs 镜像位于 "4-软件资料\Linux\Filesystem\buildroot-2024.02\image\buildroot-202402-[版本号]-[Git 系列号]\"。

#### 3.1 修改 rootfs 根文件系统（以增加文件为例）
- 创建挂载目录并挂载：`mkdir -p ~/mount_dir` → `sudo mount -o loop rootfs.img ~/mount_dir`。
- 进入 chroot：`sudo chroot ~/mount_dir`（进入 bash-5.2#）。
- 新增文件：`touch test`（备注：修改或删除文件操作同理）。
- 退出并卸载：`exit` → `sudo umount -l ~/mount_dir`。
- 重新挂载可确认 test 文件已增加。

#### 3.2 修改 rootfs 根文件镜像系统大小（扩容）
- 基于 rootfs 增加文件需确认剩余空间，不足时扩容。
- （1）查看剩余空间：`sudo mount -o loop rootfs.img ~/mount_dir` → `sudo mount -t proc /proc ~/mount_dir/proc` → `sudo chroot ~/mount_dir` → `df -lh`（示例：rootfs 镜像大小 1.3GByte，剩余 74MByte）。退出卸载：`exit` → `sudo umount ~/mount_dir/proc` → `sudo umount -l ~/mount_dir`。
- （2）修改镜像大小（将 1.3GByte 扩大为 2GByte）：
  - 创建空镜像：`dd if=/dev/zero of=rootfs-expanded.img bs=1G count=2`
  - 格式化为 EXT4：`mkfs.ext4 rootfs-expanded.img`
  - 挂载原镜像：`sudo mount -o loop rootfs.img ~/mount_dir/`
  - 创建并挂载新镜像：`mkdir -p ~/mount_expanded` → `sudo mount -o loop rootfs-expanded.img ~/mount_expanded/`
  - 迁移文件：`sudo mv ~/mount_dir/* ~/mount_expanded/` → `sync`
  - 挂载 proc 并 chroot：`sudo mount -t proc /proc ~/mount_expanded/proc/` → `sudo chroot ~/mount_expanded` → `df -lh`（确认 2.0GByte）
  - 退出卸载：`exit` → `sudo umount ~/mount_expanded/proc` → `sudo umount -l ~/mount_expanded` → `sudo umount -l ~/mount_dir`
  - 替换：`rm rootfs.img` → `mv rootfs-expanded.img rootfs.img`

#### 3.3 基于 rootfs.img 提取 rootfs.tar.gz
- rootfs.tar.gz 为根文件系统文件集合并压缩，比 rootfs.img 更小，便于存储传输。
- 挂载：`sudo mount -o loop rootfs.img ~/mount_dir`
- 提取：`sudo tar -czf rootfs.tar.gz -C ~/mount_dir/ .`（在当前目录生成 rootfs.tar.gz）
- 卸载：`sudo umount -l ~/mount_dir`

#### 3.4 通过 rootfs.tar.gz 压缩包制作 rootfs.img
- 创建目录解压：`mkdir -p rootfs` → `sudo tar -zxf rootfs.tar.gz -C rootfs/`
- 查看大小：`sudo du -sh rootfs`（示例 1.1GByte，因此 rootfs.img 需大于 1.1GByte）
- 创建空镜像（2GByte）：`dd if=/dev/zero of=rootfs.img bs=1G count=2`
- 格式化 EXT4：`mkfs.ext4 rootfs.img`
- 挂载并解压：`sudo mount -o loop rootfs.img ~/mount_dir` → `sudo tar -zxf rootfs.tar.gz -C ~/mount_dir`
- 卸载：`sudo umount -l ~/mount_dir`（再次挂载可确认制作成功）

### 第4章 Linux 系统文件替换说明
- 厂商提供 U-Boot 镜像、Linux 内核镜像、文件系统镜像，位于 "4-软件资料\Linux\"（表7）：
  - U-Boot/image/xxx → uboot.img（U-Boot 镜像文件）
  - Kernel/image/xxx → boot.img（Linux 内核镜像文件）
  - Filesystem/buildroot-2024.02/image/xxx → rootfs.img（Buildroot 文件系统镜像文件）
- 可通过 Linux 命令行或瑞芯微开发工具 RKDevTool 固化至 Linux 系统启动卡或 eMMC。
- **4.1 通过 Linux 命令行固化**：替换 U-Boot 镜像、内核镜像至 Linux 系统启动卡或 eMMC。备注：文件系统暂不支持 Linux 命令行固化，但可通过 RKDevTool 固化。
  - **4.1.1 替换 U-Boot 镜像**：将 uboot.img 拷贝至评估板文件系统：
    - `dd if=uboot.img of=/dev/mmcblk1p1`
    - `sync`
    - `reboot`
    - 备注：如需固化至 eMMC，将设备节点修改为 "/dev/mmcblk0p1"。
  - **4.1.2 替换内核镜像**：将 boot.img 拷贝至评估板文件系统：
    - `dd if=boot.img of=/dev/mmcblk1p3`
    - `sync`
    - `reboot`
    - 备注：如需固化至 eMMC，将设备节点修改为 "/dev/mmcblk0p3"。
- **4.2 通过瑞芯微开发工具 RKDevTool 固化**：
  - 确保 Micro SD 卡槽未插卡，用 Type-C 线将评估板 USB3.2 OTG 接口连接至 PC USB。备注：本节方法仅支持固化至 eMMC，不支持固化至 Linux 系统启动卡。
  - 评估板断电，长按 USER1(KEY4) 上电启动，工具界面出现"发现一个 LOADER 设备"，松开 USER1。
  - 点击"设备分区表"获取分区表信息（弹窗"是否更新下载地址？"选"是"，提示"读分区表成功"）。
  - 将待替换镜像拷贝至 Windows 非中文工作目录；打开瑞芯微开发工具，选择对应镜像存放路径并勾选对应选项（图89/90/91 分别为替换 U-Boot/内核/文件系统镜像选项）。
  - 以替换内核镜像为例：boot 选择 boot.img 路径并勾选 boot 选项，点击"执行"，固化至 eMMC，提示"下载完成"后评估板自动重启。

### 第5章 U-Boot 命令和环境变量说明
- 评估板上电启动，在 U-Boot 倒计时结束前按 "Ctrl + C" 进入 U-Boot 命令行模式，执行 `help` 或 `?` 查看支持的命令。
- 命令较多（图95/96），如 atags、bdinfo、boot、bootcmd、bootm、bootz、crc32、dhcp、env、ext4load、ext4ls、fatload、fdt、gpio、gpt、help、iomem、load、ls、md、mw、mmc、mmcinfo、nfs、nm、part、ping、pinmux、printenv、reset、rockchip_show_bmp/logo、run、save、saveenv、setenv、tftp、tftpboot、ums、usb、version 等。
- **主要命令解析（表8）**：
  - `setenv`：设置或修改环境变量的值
  - `saveenv`：保存修改后的环境变量，保存至 eMMC 的 BOOT0 分区
  - `env default -f -a`：恢复默认环境变量
  - `printenv`：输出当前 U-Boot 环境变量信息
  - `boot`：读取环境变量 bootcmd（U-Boot 启动命令集合）来启动 Linux 系统
  - `help` 或 `?`：查看当前 U-Boot 支持的命令
- 修改环境变量后须 `saveenv` 保存，否则重启使用旧值，修改完成后执行 `reset`。示例：`env default -f -a` → `saveenv` → `reset`。
- **printenv 主要环境变量解析**：
  - `baudrate=115200`：调试串口波特率
  - `bootcmd=boot_android ${devtype} ${devnum};boot_fit;bootrkp;run distro_bootcmd;`：系统启动时先设置启动方式
  - `distro_bootcmd=setenv scsi_need_init; for target in ${boot_targets}; do run bootcmd_${target}; done`：设置启动方式为 Linux 启动
  - `bootdelay=0`：启动延时 0 秒
  - `fdt_addr_r=0x48300000`：设备树文件读取至 DDR 的加载地址
  - `kernel_addr_r=0x40400000`：内核镜像读取至 DDR 的加载地址
  - `mmc_boot=if mmc dev ${devnum}; then setenv devtype mmc; run scan_dev_for_boot_part; fi`：mmc 启动命令，配置 mmc 启动参数

### 第6章 文件系统中文支持说明
- 厂商提供的文件系统已加入中文语言库，支持输入并显示中文。
- 创建中文名目录或文件：
  - `touch 创龙科技`
  - `mkdir 测试目录`

### 第7章 系统信息查询
- 评估板系统启动后自动登录 root 用户。
- （1）查看登录欢迎信息：`cat /etc/issue`（示例 "Welcome to RK3576 Buildroot"）。欢迎信息记录在 "/etc/issue"，修改此文件改变登录信息。
- （2）查看计算机名：`hostname`（示例 RK3576-Tronlong）。主机名记录在 "/etc/hostname"。
- （3）查看 Linux 内核版本：`cat /proc/version`。信息解析（表9，示例 Linux version 6.1.115-rt16-03117-ge2d5ae01404e）：
  - 6.1.115-rt16：内核版本
  - rt16：实时内核补丁版本
  - ge2d5ae01404e：Git 序列号
  - aarch64-none-linux-gnu-gcc：交叉编译工具链
  - GNU Toolchain for the A-profile Architecture 10.3-2021.07(arm-10.29)：工具链版本
  - #1：清理内核后编译次数
  - SMP / SMP PREEMPT_RT：SMP 为 Linux 内核，SMP PREEMPT_RT 为 Linux-RT 内核
  - Thu Mar 6 10:46:10 CST 2025：内核镜像编译时间
- （4）查看 CPU 使用率：`top`。
- （5）查看内存使用情况：`cat /proc/meminfo`（示例 MemTotal: 2008880 kB）。
- （6）查看系统环境变量：`env`（含 SHELL、QT_TRONLONG_DISPLAY_CONFIG=HDMI、LANG=en_US.UTF-8、PATH、GST_V4L2SRC_MAX_RESOLUTION=3840x2160 等）。
- （7）查看文件系统支持库目录：`ls /usr/lib/`（含 alsa-lib、chromium、gstreamer-1.0、gtk-2.0、libEGL、libGLES 等）。

### 第8章 文件系统组件安装说明
- 部分组件需手动安装，详见 "4-软件资料\Ubuntu\Filesystem\buildroot-2024.02\" 下《rootfs-feature-support》。本章通过 Buildroot 配置界面安装系统组件。
- 以安装 nodejs 为例：
  - 进入 "buildroot/package/nodejs/" 目录，`vim Config.in` 查看 `BR2_PACKAGE_NODEJS`（Buildroot 配置中引导 nodejs 安装的关键字）。
  - 在 buildroot 目录执行 `make menuconfig` 打开配置界面。
  - 按 `/` 搜索 `BR2_PACKAGE_NODEJS`，搜索结果显示其位于 "Target packages → Interpreter languages and scripting"，定义于 package/nodejs/Config.in:16。
  - 输入 1 选择第一个选项，进入后按 Y 配置（nodejs 项变为 `[*]`）。
  - 配置完成后参考"Linux 系统镜像编译、生成"章节保存 Buildroot 配置，编译生成系统镜像，同时将 nodejs 安装在系统镜像内。

### 第9章 内存分配说明
- 厂商 Linux 系统已对核心板 DDR 内存进行划分（表10）：
  - **2GByte DDR**：Linux 系统，0x40200000~0x483fffff（129MByte）+ 0x49400000~0xbfffffff（1899MByte），属于内核管理的内存。
  - **4GByte DDR**：0x40200000~0x483fffff（129MByte）+ 0x49400000~0x13fffffff（3947MByte），内核管理。
  - **8GByte DDR**：0x40200000~0x483fffff（129MByte）+ 0x49400000~0x23fffffff（8043MByte），内核管理。
- 查看内核管理内存空间（以 2GByte DDR 为例）：
  - （1）`cat /sys/kernel/debug/memblock/memory`（显示 0x40200000~0x483fffff、0x49400000~0xbfffffff 两段）。
  - （2）`cat /sys/kernel/debug/memblock/reserved`（查看内核已分配内存）。
  - 从 U-Boot 启动打印信息看：编号 0 存放 kernel panel 信息（drm-logo/ramoops addr=40110000）；编号 1、2 存放内核镜像文件；编号 3 存放设备树文件（fdt_addr 0x48300000）。
  - 从 Kernel 启动打印信息看：编号 4 为 CMA（连续内存区管理）空间，其余为内核管理空间。kernel 加载地址 0x40400000，fdt blob 0x48300000。Kernel 启动打印含 "cma: Reserved 16 MiB at 0x00000000be800000"（CMA 16MiB），DDR zone DMA mem 0x40200000-0xbfffffff。

### 第10章 程序开机自启动说明
- Linux 系统下，可通过 init 进程方式和 systemd 服务方式实现程序开机自启动。本章以 led_flash 案例为例，演示 init 进程方式。
- 将 "4-软件资料\Demo\base-demos\led_flash\bin\" 下的可执行文件拷贝至评估板 "/etc/init.d/" 目录。
- 改变权限：`chmod a+x /etc/init.d/led_flash`。
- 修改 "/etc/init.d/" 下的 rcS 配置文件：`vi /etc/init.d/rcS`，在文件末添加：
  - `/etc/init.d/led_flash -n 2 &`（设置自启动的程序路径，& 表示后台运行）
- 保存退出。评估板断电重启，Linux 系统自动运行程序，串口终端打印 System leds/Flashing leds 信息，LED 闪烁。
- 取消开机自启动：修改 rcS 配置文件，用 "#" 注释相应命令（`#/etc/init.d/led_flash -n 2 &`）。

### 第11章 CPU 主频调节说明
- 根据《Rockchip RK3576J Datasheet V1.0-20241028》，RK3576J 处理器长时间在 overdrive mode 运行（特别是高温条件），处理器使用寿命可能缩短。
- 为保障寿命，厂商已将 RK3576J/RK3576 处理器 Cortex-A72 核心最高主频默认配置为 1.6GHz，Cortex-A53 核心最高主频默认配置为 1.4GHz。如需调整更高主频，参考"通过设备树配置方法"。

#### 11.1 CPU 主频说明
- 评估板支持 normal mode 和 overdrive mode，系统默认配置为 normal mode。
- **normal mode 可配置主频（表11）**：
  - RK3576 Cortex-A53：0.408/0.600/0.816/1.008/1.200/1.416GHz
  - RK3576 Cortex-A72：0.408/0.600/0.816/1.008/1.200/1.416/1.608GHz
  - RK3576J 同上（A53 无 1.608GHz，A72 含 1.608GHz）
- **7 种 CPU 主频模式（表12，默认 performance）**：
  - performance：性能优先，始终最高频（A53 1.416GHz、A72 1.608GHz）
  - userspace：用户自定义电压和频率，系统不自动调整（1.200GHz）
  - powersave：功耗优先，始终最低频（0.408GHz）
  - interactive：动态调节，快升慢降（高负载迅速提高至高频，低负载逐渐降低）
  - ondemand：动态调节，按需，调频幅度大（工作时迅速提至最高频，空闲迅速降低）
  - conservative：动态调节，慢升快降
  - schedutil：动态调节，Android 新调度策略，通过 PELT 统计 Task 负载，调节延时低

#### 11.2 查看 CPU 主频
- Linux 系统默认将处理器核心分为 2 组：4 个 Cortex-A53 为 policy0 组，4 个 Cortex-A72 为 policy4 组。
- **11.2.1 通过 policy 方式查看**（以 policy0 为例，policy4 同理）：
  - `cat /sys/devices/system/cpu/cpufreq/policy0/scaling_governor`（查看主频模式，示例 performance）
  - `cat /sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq`（查看当前主频，示例 1416000）
  - `cat /sys/devices/system/cpu/cpufreq/policy0/scaling_available_frequencies`（可配置主频：408000 600000 816000 1008000 1200000 1416000）
  - `cat /sys/devices/system/cpu/cpufreq/policy0/scaling_available_governors`（支持的模式：interactive conservative ondemand userspace powersave performance schedutil）
- **11.2.2 通过单个 CPU 核心方式查看**（以 cpu7/cpu0/cpu2 为例，可改为 cpux）：
  - `cat /sys/devices/system/cpu/cpu7/cpufreq/scaling_governor`（示例 performance）
  - `cat /sys/devices/system/cpu/cpu7/cpufreq/cpuinfo_cur_freq`（示例 1608000）
  - `cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies`
  - `cat /sys/devices/system/cpu/cpu2/cpufreq/scaling_available_governors`

#### 11.3 CPU 主频配置
- 厂商默认对主频做了限制（A72 最高 1.6GHz、A53 最高 1.4GHz）。备注：通过修改内核或命令行方式仅支持配置 CPU 主频模式，无法调整为更高主频；调整更高主频需用设备树配置方法。
- **11.3.1 通过内核配置方法**：
  - `./build.sh kernel-config` 打开内核图形配置界面。
  - 进入 "CPU Power Management → CPU Frequency scaling" 路径，选中 "Default CPUFreq governor"（默认 performance）。
  - 修改为 powersave 等模式，`<Save>` 保存、`<Exit>` 退出。
  - 参考"内核编译"重新编译生成内核镜像，参考"Linux 系统文件替换说明"替换内核镜像，断电重启生效。
  - 验证：`cat /sys/devices/system/cpu/cpufreq/policy0/scaling_governor` 和 policy4（示例 powersave）。
- **11.3.2 通过命令行配置方法**（命令行配置临时生效，重启后重新配置）：
  - policy 组方式（以 policy0 为例，配置 userspace 模式、主频 1200000kHz）：
    - `echo userspace > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor`
    - `echo 1200000 > /sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed`
    - `cat /sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq`（示例 1200000）
  - 单核心方式（以 cpu4 为例）：
    - `echo userspace > /sys/devices/system/cpu/cpu4/cpufreq/scaling_governor`
    - `echo 1200000 > /sys/devices/system/cpu/cpu4/cpufreq/scaling_setspeed`
    - `cat /sys/devices/system/cpu/cpu4/cpufreq/scaling_cur_freq`
- **11.3.3 通过设备树配置方法**（可调整更高主频）：
  - 修改 "kernel/arch/arm64/boot/dts/rockchip/" 下的 tl3576-evm.dts。CPU 为 RK3576J 修改 `opp-j-m-xxxxxxxx`，RK3576（商业级）修改 `opp-xxxxxxxx`。
  - 以 RK3576（商业级）policy4 组为例，调整支持 1.800GHz、2.016GHz、2.208GHz：
    - `cd /home/tronlong/RK3576/rk3576_linux6.1_release/kernel`
    - `vim arch/arm64/boot/dts/rockchip/tl3576-evm.dts`
    - 在 `&cluster1_opp_table`（RK3576 A72 core limit at 1.6GHz）下，对 `/delete-node/ opp-1800000000;`、`opp-2016000000;`、`opp-2208000000;` 添加 "//" 注释符（删除节点被注释即恢复该主频）。
  - 重新编译内核镜像并替换，断电重启生效。验证 policy4：
    - `cat .../policy4/scaling_available_frequencies`（含 1800000 2016000 2208000）
    - `cat .../policy4/scaling_governor`（performance）
    - `cat .../policy4/scaling_cur_freq`（示例 2208000）

### 第12章 TFTP 使用说明
- TFTP（Trivial File Transfer Protocol）是用来下载远程文件的简单网络协议，基于 UDP 协议。嵌入式 Linux 的 TFTP 包括服务器和客户端，用于评估板（客户端）和 PC（服务器）之间文件传输，避免频繁 U 盘拷贝。
- 本章演示评估板作客户端，PC Linux 搭建 TFTP 服务器。验证 TFTP 工具：`find /usr/ -name "tftp"`（/usr/bin/tftp）。

#### 12.1 TFTP 服务器搭建
- 安装服务器软件：`sudo apt-get install tftp-hpa tftpd-hpa xinetd -y`（tftp-hpa 客户端、tftpd-hpa 服务器、xinetd 配置文件）。
- 配置 "/etc/xinetd.conf"：`sudo vi /etc/xinetd.conf`（不存在则新建，内容须含 `defaults {...}` 和 `includedir /etc/xinetd.d`）。
- 新建 TFTP 工作目录并改权限：
  - `mkdir -p /home/tronlong/tftpboot`
  - `sudo chmod 777 /home/tronlong/tftpboot`（可读可写可执行，允许其他客户端下载上传）
- 配置 "/etc/default/tftpd-hpa"：`sudo vi /etc/default/tftpd-hpa`，内容：
  - `TFTP_USERNAME="tftp"`
  - `TFTP_DIRECTORY="/home/tronlong/tftpboot"`（TFTP 服务器工作目录）
  - `TFTP_ADDRESS=":69"`
  - `TFTP_OPTIONS="--secure -c"`
- 重启服务：
  - `sudo service tftpd-hpa restart`
  - `sudo service xinetd reload`
  - `sudo service xinetd restart`

#### 12.2 TFTP 文件传输测试
- 进入工作目录新建 test 文件：`cd /home/tronlong/tftpboot/` → `echo www.tronlong.com > test` → `chmod 777 test`。
- 进入 TFTP 命令行模式：`sudo tftp localhost`（`help` 查看命令，`q` 退出）。TFTP 命令含 connect、mode、put、get、quit、verbose、trace、binary、ascii、rexmt、timeout 等。
- 将评估板 ETH1 RGMII 网口和 PC 通过路由器/交换机互联或网线直连。用 `ifconfig` 确保同一网段（示例 PC 192.168.13.46、评估板 192.168.13.51，子网掩码 255.255.255.0）。
- PC 上 `ping 192.168.13.51` 测试连通性。
- 评估板下载文件：`tftp -g -r test 192.168.13.46`，`cat test` 查看内容。
- **TFTP 命令参数解析（表13）**：
  - 192.168.13.46：TFTP 服务器 IP 地址
  - -g：get，从 TFTP 服务器工作目录下载文件
  - -r：remote，远程服务器
  - test：TFTP 服务器工作目录中的文件名称

### 第13章 NFS 使用说明
- NFS（Network File System）网络文件系统，基于 UDP/IP 协议，通过网络让不同主机共享文件或目录。NFS 客户端可通过挂载方式将远程 NFS 服务器共享数据目录挂载到本地系统。
- 本章演示 PC Ubuntu 搭建 NFS 服务器，评估板挂载并访问。验证工具：`find /sbin/ -name "*nfs"`（/sbin/mount.nfs、/sbin/umount.nfs）。
- **13.1 NFS 服务器搭建**：
  - 进入 PC Ubuntu，安装 NFS 服务器安装包：`sudo apt-get install nfs-kernel-server -y`（图166 安装过程；图167 显示创建 nfs-server.service 软链接、creating config file /etc/exports、/etc/default/nfs-kernel-server 等）。
  - 创建 NFS 共享目录并新建测试文件：
    - `sudo mkdir /home/tronlong/nfs_share/`
    - `sudo chmod 777 nfs_share`
    - `echo www.tronlong.com > nfs_share/test`（图168 用 `cat nfs_share/test` 验证）。
  - 打开 "/etc/exports" 配置文件设置共享目录及操作权限：`sudo vi /etc/exports`，在末尾添加：
    - `/home/tronlong/nfs_share/ *(rw,nohide,insecure,no_subtree_check,async,no_root_squash)`（图170）。
  - **配置命令参数解析（表14、表15 续）**：
    - `/home/tronlong/nfs_share/`：NFS 共享目录
    - `*`：允许任何网段 IP 的系统访问 NFS 共享目录
    - `rw`：访问者具有可读写权限
    - `nohide`：共享 NFS 目录的子目录
    - `insecure`：NFS 通过 1024 以上的端口发送
    - `no_subtree_check`：若共享 "/usr/bin" 之类的子目录时，不检查父目录权限
    - `async`：资料不同步写入内存和硬盘
    - `no_root_squash`：访问者对 NFS 共享目录具有 root 权限
  - 重启 NFS 服务器并查询共享目录：
    - `sudo /etc/init.d/nfs-kernel-server restart`
    - `showmount -e`（图171 显示 Export list：/home/tronlong/nfs_share *）。
- **13.2 NFS 共享目录挂载测试**：
  - 将评估板 ETH1 RGMII 网口与 PC 通过路由器/交换机互联或网线直连，用 `ifconfig` 确保同一网段（图172 PC 192.168.13.46；图173 评估板 192.168.13.51，子网掩码 255.255.255.0）。
  - PC 上 `ping 192.168.13.51` 测试连通性（图174）。
  - 评估板建立客户端挂载目录并改权限：`mkdir -p /nfs/` → `chmod 777 /nfs/`（图175）。
  - 挂载 PC 共享目录到评估板：`mount -t nfs -o nolock 192.168.13.46:/home/tronlong/nfs_share /nfs/`（图176 用 `df -h` 验证已挂载，198G/111G/78G 60%）。
  - **NFS 挂载命令参数解析（表15）**：
    - `-t nfs`：挂载类型为 NFS
    - `-o nolock`：不加文件锁（NFS 挂载时默认选项为文件锁 lock）
    - `192.168.13.51`：NFS 服务器 IP 地址（注：表中此处文档写的是评估板地址，实际命令中服务器为 192.168.13.46）
    - `/home/tronlong/nfs_share`：NFS 服务器共享目录
    - `/nfs/`：NFS 客户端挂载目录
  - 验证双向同步：评估板 `ls /nfs/`、`cat /nfs/test`（图177，内容 www.tronlong.com 与 PC 一致，图178）。评估板 `echo "Hello Tronlong" > /nfs/test1`（图179），PC `cat nfs_share/test1` 可见 Hello Tronlong（图180）。
  - 卸载：`umount /nfs/` → `df -h`（图181 确认已卸载）。

### 第14章 基于 TFTP + NFS 的系统启动说明
- 本章演示评估板基于 TFTP + NFS 启动 Linux 系统：通过 TFTP 启动 Linux 内核，通过 NFS 挂载 Linux 文件系统。要求 Ubuntu 网络配置为桥接模式，且已正常安装 TFTP 和 NFS。
- （1）拷贝内核镜像至 TFTP 工作目录：将 "4-软件资料\Linux\Kernel\image\linux-6.1.115-[版本号]-[Git 系列号]\" 下的 boot.img 拷贝至 Windows SharedFolders 共享目录，再：
  - `cp /mnt/hgfs/SharedFolders/boot.img /home/tronlong/tftpboot/`（图182/183，`ls` 确认 boot.img、test）。
- （2）拷贝文件系统镜像并挂载到 NFS 共享目录：将 "4-软件资料\Linux\Filesystem\buildroot-2024.02\image\buildroot-202402-[版本号]-[Git 系列号]\" 下的 rootfs.img 拷贝至 SharedFolders：
  - `cp /mnt/hgfs/SharedFolders/rootfs.img /home/tronlong/RK3576/`（图184/185）
  - `sudo mount -o loop /home/tronlong/RK3576/rootfs.img /home/tronlong/nfs_share/`（图186，nfs_share 下出现 bin/boot/dev/etc... 文件系统目录）。
  - 如需进入 "/home/tronlong/nfs_share/root/" 目录，可 `sudo su` 切换 root，`exit` 退出（图187）。
- （3）查看 Ubuntu IP：`ifconfig`（本次 192.168.13.46，图188）。
- （4）U-Boot 网络配置（评估板上电长按 "Ctrl + C" 进入 U-Boot 命令行，图189）：
  - `setenv serverip 192.168.13.46`（设置 NFS 和 TFTP 服务器 IP 地址）
  - `setenv ipaddr 192.168.13.51`（设置评估板 IP 地址，与服务器 IP 同网段）
  - `setenv gatewayip 192.168.13.1`（设置网关 IP 地址）
  - `setenv nfspath /home/tronlong/nfs_share`（设置 NFS 服务器共享目录路径）
  - `setenv bootcmd 'run net_boot'`（设置启动方式为 NFS）
  - `saveenv`（保存环境变量）
  - `reset`（重启）
- （5）评估板重启后 U-Boot 自动从 TFTP 服务器工作目录下载内核镜像（图190 TFTP from server 192.168.13.46，下载 boot.img，Bytes transferred 43050496，启动 FIT Image），下载完成启动内核并从 NFS 共享目录挂载文件系统（图191 内核启动打印 Machine model: TL3576-EVM；图192 自动登录 root，Welcome to RK3576 Buildroot）。
- （6）排障：如根文件系统无法加载导致系统卡住，修改 Ubuntu 的 NFS 配置文件 `sudo vim /etc/default/nfs-kernel-server`（图193），修改内容（图194）：
  - `RPCNFSDCOUNT="-V 2 8"`
  - `RPCMOUNTDOPTS="-V 2 --manage-gids"`
  - `RPCNFSDOPTS="--nfs-version 2,3,4 --debug --syslog"`
  - 重启 NFS 服务后将评估板断电重启：`sudo systemctl restart nfs-kernel-server`（图195）。
  - 验证双向同步：PC `sudo touch nfs_share/test.txt`（图196），评估板可见 test.txt（图197）；评估板 `echo Tronlong > /test.txt`，PC `cat nfs_share/test.txt` 显示 Tronlong（图198）。
- （7）恢复默认启动模式：在 U-Boot 命令行执行 `env default -a -f` → `saveenv` → `reset`（图199）。

### 第15章 Linux 设备驱动说明
- 给出主要 Linux 设备驱动的内核驱动源码路径与对应设备文件/头文件（表16，"X" 表示可变化数值）：
  - SOM LED 2/3、EVM LED 2/3/4：`drivers/leds/leds-gpio.c` → `/sys/class/leds/user-ledX`
  - USB2.0 HOST：`drivers/phy/rockchip/phy-rockchip-inno-usb2.c` → `/sys/bus/usb/`
  - USB3.2 HOST：`drivers/usb/dwc3/core.c` → `/sys/bus/usb/`
  - USB3.2 OTG：`drivers/usb/dwc3/core.c`、`drivers/usb/typec/tcpm/tcpci_husb311.c` → `/sys/bus/usb/`
  - KEY4：`drivers/input/keyboard/gpio_keys.c` → `/dev/input/eventX`
  - KEY5：`drivers/input/keyboard/adc-keys.c` → `/dev/input/eventX`
  - RTC：`drivers/rtc/rtc-ds1307.c` → `/dev/rtcX`
  - HDMI OUT：`drivers/gpu/drm/rockchip/dw_hdmi-rockchip.c` → `/dev/fbX`
  - eMMC：`drivers/mmc/host/dw_mmc-rockchip.c` → `/dev/mmcblkX`
  - Micro SD：`drivers/mmc/host/dw_mmc-rockchip.c` → `/dev/mmcblkX`
  - MIPI LCD：`drivers/gpu/drm/panel/panel-simple.c` → `/dev/fbX`
  - MIPI LCD CAP TS：`drivers/input/touchscreen/goodix.c` → `/dev/input/eventX`
  - MIPI LCD BACKLIGHT：`drivers/video/backlight/pwm_bl.c` → `/sys/class/backlight/`
  - ETH1/2 RGMII：`drivers/net/ethernet/stmicro/stmmac/dwmac-rk.c` → `/sys/class/net/ethX`
  - ETH3/4(USB)：`drivers/usb/gadget/udc/core.c` → `/sys/class/net/ethX`
  - RS485：`drivers/tty/serial/8250/8250_dw.c` → `/dev/ttySX`
  - RS232：`drivers/tty/serial/8250/8250_dw.c` → `/dev/ttySX`
  - CAN-FD1/2：`drivers/net/can/rockchip/rk3576_canfd.c` → `/sys/class/net/canX`
  - MIC IN/LINE IN/HP OUT：`sound/soc/codecs/es8323.c`、`sound/soc/rockchip/rockchip_sai.c` → `/dev/snd/`
  - CAMERA1：`drivers/media/i2c/ov13850.c` → `/dev/videoX`
  - CAMERA2/3/4/5：`drivers/media/i2c/imx219.c` → `/dev/videoX`
  - LVDS：`drivers/gpu/drm/panel/panel-simple.c` → `/dev/fbX`
  - LVDS BACKLIGHT：`drivers/video/backlight/pwm_bl.c` → `/sys/class/backlight/`
  - LVDS RES TS：`input/touchscreen/ads7846.c` → `/dev/input/eventX`
  - M.2 PCIe NVMe：`drivers/pci/controller/dwc/pcie-dw-rockchip.c` → `/sys/class/pci_bus/`
  - ADC：`drivers/iio/adc/rockchip_saradc.c` → `/sys/bus/iio/devices/iio:deviceX/in_voltageX_raw`
- 查询表中未列出的驱动源码：进入内核源码 drivers 目录，用 `find | grep` 查找。以 ADS7846 驱动为例：
  - `find | grep "ads7846"`（图200，结果含 input/touchscreen/ads7846.c、ads7846.o.cmd、ads7846.o；"ads7846" 为查找关键字）。

### 更多帮助
- 销售邮箱：sales@tronlong.com；技术邮箱：support@tronlong.com
- 创龙总机：020-8998-6280；技术热线：020-3893-9734
- 创龙官网：www.tronlong.com；技术论坛：www.51ele.net；官方商城：tronlong.tmall.com

---

## 关键 API / 命令清单

### 系统安装与依赖
- `sudo apt-get install -y git ssh make gcc libssl-dev liblz4-tool expect ... python2`：安装编译 LinuxSDK 所需的库与工具集。
- `sudo ln -s /usr/bin/python3 /usr/bin/python`：将 python 软链接指向 python3（要求 ≥3.6.0）。
- `unzip LZ4_old1-dev.zip` / `make` / `sudo make install` / `sudo install -m 0755 lz4 /usr/bin/lz4`：编译安装 LZ4（≥v1.7.3）。
- `tar -zxf LinuxSDK-v1.0.tar.gz`：解压 LinuxSDK 源码。
- `export PATH=.../gcc-arm-10.3-2021.07-.../bin/:$PATH`：配置 U-Boot/内核交叉编译工具链环境变量（aarch64-none-linux-gnu-gcc）。
- `source .../host/environment-setup`：配置应用开发交叉工具链环境变量（aarch64-buildroot-linux-gnu-gcc 12.4.0）。

### LinuxSDK 编译
- `./build.sh lunch:tl3576_evm_defconfig`：配置 LinuxSDK 编译选项（选择板级配置）。
- `./build.sh`：一键编译生成 U-Boot/Kernel/Buildroot/Recovery 并打包 update.img。
- `./build.sh kernel-config`：打开内核图形配置界面（menuconfig），保存至 kernel/arch/arm64/configs/rockchip_linux_defconfig。
- `./build.sh uboot` / `./build.sh kernel` / `./build.sh buildroot` / `./build.sh firmware`：分别局部编译 U-Boot、内核、文件系统、打包 update.img。
- `make ARCH=arm64 rockchip_linux_defconfig rockchip_rt.config`：叠加 RT 实时内核配置。
- `make ARCH=arm64 savedefconfig` / `mv defconfig arch/arm64/configs/rockchip_linux_defconfig`：保存内核 defconfig。
- `./envsetup.sh rockchip_rk3576` + `make menuconfig` + `make savedefconfig`：Buildroot 配置与保存（buildroot/configs/rockchip_rk3576_defconfig）。

### rootfs 操作
- `sudo mount -o loop rootfs.img ~/mount_dir`：以 loop 方式挂载镜像。
- `sudo chroot ~/mount_dir`：切根进入文件系统内部。
- `dd if=/dev/zero of=xxx.img bs=1G count=N` + `mkfs.ext4 xxx.img`：创建并格式化空 EXT4 镜像（用于扩容/重制）。
- `sudo tar -czf rootfs.tar.gz -C ~/mount_dir/ .`：从挂载目录提取 rootfs.tar.gz。
- `sudo umount -l ~/mount_dir`：延迟卸载（lazy）。

### 系统文件替换
- `dd if=uboot.img of=/dev/mmcblk1p1` / `dd if=boot.img of=/dev/mmcblk1p3` + `sync` + `reboot`：命令行固化 U-Boot/内核镜像（Micro SD 为 mmcblk1，eMMC 为 mmcblk0）。

### U-Boot 命令（表8）
- `setenv`：设置/修改环境变量。
- `saveenv`：保存环境变量到 eMMC BOOT0 分区。
- `env default -f -a`（或 `env default -a -f`）：恢复默认环境变量。
- `printenv`：输出当前环境变量。
- `boot`：按 bootcmd 启动 Linux。
- `help` / `?`：查看支持的命令。
- `reset`：复位重启。
- 网络启动相关：`setenv serverip/ipaddr/gatewayip/nfspath`、`setenv bootcmd 'run net_boot'`、`run net_boot`。

### 系统信息查询
- `cat /etc/issue`：登录欢迎信息；`hostname`（/etc/hostname）：计算机名。
- `cat /proc/version`：内核版本（表9 解析各字段）。
- `top`：CPU 使用率；`cat /proc/meminfo`：内存；`env`：环境变量；`ls /usr/lib/`：支持库目录。

### CPU 主频（cpufreq sysfs）
- `cat /sys/devices/system/cpu/cpufreq/policy0/scaling_governor`：查看主频模式（policy0=A53，policy4=A72）。
- `.../cpuinfo_cur_freq` / `.../scaling_cur_freq`：当前主频。
- `.../scaling_available_frequencies`：可配置主频列表。
- `.../scaling_available_governors`：支持的模式列表。
- `echo userspace > .../scaling_governor` + `echo 1200000 > .../scaling_setspeed`：命令行临时配置模式与频率。
- 单核方式：`/sys/devices/system/cpu/cpuX/cpufreq/...`。
- 设备树 OPP：在 tl3576-evm.dts 的 `&cluster1_opp_table` 下注释 `/delete-node/ opp-1800000000;` 等以恢复更高主频。

### TFTP
- `sudo apt-get install tftp-hpa tftpd-hpa xinetd -y`：安装 TFTP 服务端/客户端/配置。
- 配置 `/etc/default/tftpd-hpa`（TFTP_DIRECTORY、TFTP_OPTIONS="--secure -c" 等）。
- `sudo service tftpd-hpa restart` / `sudo service xinetd reload|restart`：重启服务。
- `tftp -g -r test 192.168.13.46`（表13）：从服务器 get 文件（-g get，-r remote，test 文件名）。

### NFS
- `sudo apt-get install nfs-kernel-server -y`：安装 NFS 服务器。
- 配置 `/etc/exports`：`目录 *(rw,nohide,insecure,no_subtree_check,async,no_root_squash)`（表14 参数解析）。
- `sudo /etc/init.d/nfs-kernel-server restart` / `sudo systemctl restart nfs-kernel-server`：重启 NFS。
- `showmount -e`：查询本地共享目录。
- `mount -t nfs -o nolock 192.168.13.46:/home/tronlong/nfs_share /nfs/`（表15）：评估板挂载远程共享。
- `umount /nfs/`：卸载；排障时修改 /etc/default/nfs-kernel-server 的 RPCNFSDCOUNT/RPCMOUNTDOPTS/RPCNFSDOPTS。

### 开机自启动
- `chmod a+x /etc/init.d/led_flash`：赋可执行权限。
- 在 `/etc/init.d/rcS` 末尾添加 `/etc/init.d/led_flash -n 2 &`（init 进程方式自启动，& 后台运行）；注释 `#` 取消自启动。

### 内存
- `cat /sys/kernel/debug/memblock/memory`：内核管理内存空间。
- `cat /sys/kernel/debug/memblock/reserved`：内核已分配（保留）内存。

### 组件安装
- `make menuconfig` 中按 `/` 搜索 `BR2_PACKAGE_NODEJS` 等关键字，按 Y 选中后保存编译。

### 设备驱动查询
- `find | grep "ads7846"`：在内核 drivers 目录查找指定驱动源码。

---

## 示例代码 / 示例配置说明

1. **/etc/exports 配置行** `/home/tronlong/nfs_share/ *(rw,nohide,insecure,no_subtree_check,async,no_root_squash)`：定义一个对所有 IP（`*`）开放、可读写（rw）、客户端拥有 root 权限（no_root_squash）、不检查子目录父权限（no_subtree_check）、异步写（async）的 NFS 共享目录。适用于开发调试环境（生产应收紧权限）。

2. **NFS 排障配置（/etc/default/nfs-kernel-server）**：
   - `RPCNFSDCOUNT="-V 2 8"`：启动 8 个 nfsd 守护进程，并显式声明支持 NFS v2。
   - `RPCMOUNTDOPTS="-V 2 --manage-gids"`：mountd 启用 v2 并由服务端管理用户附加组 ID。
   - `RPCNFSDOPTS="--nfs-version 2,3,4 --debug --syslog"`：同时支持 v2/v3/v4 协议并输出调试日志。
   - 关键作用：评估板内核 NFS 客户端默认走较低版本协议，当默认仅启 v3/v4 导致根文件系统挂载失败、系统卡死时，显式开启 v2 兼容后即可挂载。

3. **U-Boot 网络启动环境变量序列**（基于 TFTP+NFS 启动）：
   - serverip/ipaddr/gatewayip 三者必须同网段，否则 TFTP 下载内核或 NFS 挂载会失败。
   - `setenv nfspath /home/tronlong/nfs_share`：内核以该目录作为根文件系统（NFS rootfs）。
   - `setenv bootcmd 'run net_boot'`：覆盖默认 bootcmd，改为网络启动脚本；`saveenv` 持久化后 `reset` 生效。
   - 注意：调试完成需 `env default -a -f` + `saveenv` 恢复，否则评估板始终尝试网络启动。

4. **rcS 自启动行** `/etc/init.d/led_flash -n 2 &`：init 进程方式，`-n 2` 为程序自定义参数（闪烁参数），`&` 使其后台运行，避免阻塞 rcS 后续启动流程。注释为 `#/etc/init.d/led_flash -n 2 &` 即可关闭。

5. **rootfs 扩容流程**（dd 创建 2G 空镜像 → mkfs.ext4 → 双挂载迁移 `sudo mv ~/mount_dir/* ~/mount_expanded/` → sync → chroot 验证 → 替换）：核心是先确认旧镜像剩余空间不足（`df -lh`），再通过新建大镜像 + 文件搬迁实现扩容，最后 `rm`+`mv` 替换。注意迁移后需 `sync` 确保写盘，卸载时 proc 要单独 umount。

6. **CPU 设备树 OPP 解注释**：商业级 RK3576 默认在 `&cluster1_opp_table` 中用 `/delete-node/ opp-1800000000;` 删除了 1.8/2.016/2.208GHz 高频节点；在这些行前加 `//` 注释即"取消删除"，重新编译内核并替换后 A72 即可运行至 2.208GHz。RK3576J 需改 `opp-j-m-xxxxxxxx`。注意官方因寿命/高温风险默认限频，提频需自行评估散热。

<!-- GENERATION_COMPLETE: 2026-06-25_03:45 -->
