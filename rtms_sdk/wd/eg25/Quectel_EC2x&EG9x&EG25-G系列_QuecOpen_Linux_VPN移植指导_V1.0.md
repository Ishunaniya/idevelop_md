# EC2x&EG9x&EG25-G 系列 QuecOpen Linux VPN 移植指导

> **模块系列**：LTE Standard 模块系列
> **版本**：1.0
> **日期**：2020-08-13
> **状态**：受控文件
> **版权**：版权所有 © 上海移远通信技术股份有限公司 2020，保留一切权利。Copyright © Quectel Wireless Solutions Co., Ltd. 2020.

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| 1.0 | 2020-08-13 | 孙保 | 初始版本 |

---

## 目录

1. [引言](#1-引言)
   - 1.1 [适用模块](#11-适用模块)
2. [移植过程](#2-移植过程)
   - 2.1 [下载源码](#21-下载源码)
   - 2.2 [编译准备工作](#22-编译准备工作)
   - 2.3 [修改内核编译选项](#23-修改内核编译选项)
   - 2.4 [编译文件](#24-编译文件)
   - 2.5 [将编译生成的文件打包至 rootfs](#25-将编译生成的文件打包至-rootfs)
3. [软件测试](#3-软件测试)
   - 3.1 [PPTP](#31-pptp)
   - 3.2 [L2TP](#32-l2tp)
   - 3.3 [IPSEC](#33-ipsec)
     - 3.3.1 [服务器配置（192.168.10.154）](#331-服务器配置19216810154)
     - 3.3.2 [模块配置（192.168.22.17）](#332-模块配置1921682217)
4. [附录 A 术语缩写](#4-附录-a-术语缩写)

### 表格索引

- 表 1：适用模块
- 表 2：术语缩写

---

## 1 引言

移远通信 LTE Standard EC2x&EG9x&EG25-G 系列模块支持 QuecOpen® 方案。本文档介绍了如何在 QuecOpen® 方案下，移植适用于 Linux 系统的 VPN 软件至模块。在 QuecOpen® 中移植 VPN 软件和在其他交叉编译环境下移植第三方开源软件没有区别，因此也可参考此文档在 QuecOpen® 方案中移植其他开源软件。

> 在移植 VPN 软件之前，请确定已经搭建 QuecOpen® 的编译环境。

### 1.1. 适用模块

**表 1：适用模块**

| 模块系列 | 模块 |
|---|---|
| EC2x 系列 | EC25 系列 |
| | EC21 系列 |
| | EC20 R2.1 |
| | EC20-CN |
| EG9x 系列 | EG95 系列 |
| | EG91 系列 |
| EG25-G | EG25-G |

---

## 2 移植过程

### 2.1. 下载源码

可根据需要下载 VPN 相关软件源码：`pptp-linux`、`xl2tpd`、`pppd`、`gmp`、`strongswan`。软件的主页、版本及源码下载地址如下所示：

**1) PPTP**
- 软件：pptp-linux
- 主页：http://pptpclient.sourceforge.net/
- 版本：1.10.0
- 下载地址：https://nchc.dl.sourceforge.net/project/pptpclient/pptp/pptp-1.10.0/pptp-1.10.0.tar.gz

**2) L2TP**
- 软件：xl2tpd
- 主页：https://www.xelerance.com/archives/155
- 版本：1.3.11
- 下载地址：https://github.com/xelerance/xl2tpd/releases/tag/v1.3.11

**3) PPP**
- 软件：pppd
- 主页：https://download.samba.org/
- 版本：2.4.7
- 下载地址：https://download.samba.org/pub/ppp/ppp-2.4.7.tar.gz

**4) GMP**
- 软件：gmp
- 主页：https://gmplib.org/
- 版本：6.1.2
- 下载地址：https://gmplib.org/download/gmp/gmp-6.1.2.tar.bz2

**5) IPSEC**
- 软件：strongswan
- 主页：https://www.strongswan.org/
- 版本：5.6.2
- 下载地址：https://download.strongswan.org/strongswan-5.6.2.tar.bz2

### 2.2. 编译准备工作

**1.** 在 `ql-ol-sdk` 目录下新建 `opensrc` 目录，将下载好的源码文件放到 `opensrc` 目录下。

```sh
mkdir opensrc
```

**2.** 在 `opensrc` 目录下创建 `Makefile` 文件并输入如下内容：

```makefile
CURR_DIR := $(shell pwd)
OPENSRC_DIR := $(subst /opensrc, /opensrc, $(CURR_DIR))
OPENSRC_DIR := $(word 1, $(WORKSPACE_DIR))

export PKG_CONFIG_SYSROOT_DIR=$(SDKTARGETSYSROOT)
export PKG_CONFIG_PATH=$(SDKTARGETSYSROOT)/usr/lib/pkgconfig

BUILD_DESTDIR=$(CURR_DIR)/rootfs
BUILD_HOST=arm-oe-linux-gnueabi
BUILD_TARGET=arm-oe-linux-gnueabi

targets_build=pppd_build pptp_build xl2tpd_build libgmp_build strongswan_build
targets_clean=pppd_clean pptp_clean xl2tpd_clean libgmp_clean strongswan_clean

CFLAGS+=-I$(BUILD_DESTDIR)/include -I$(BUILD_DESTDIR)/usr/include
LDFLAGS+=-L$(BUILD_DESTDIR)/lib -L$(BUILD_DESTDIR)/usr/lib

SRC_PPPD:=ppp-2.4.7
SRC_GMP:=gmp-6.1.2
SRC_STRONGWAN=strongswan-5.6.2
SRC_XL2TPD=xl2tpd-1.3.11
SRC_PPTP=pptp-1.10.0

.PHONY: all
all: $(targets_build)
	rm -rf rootfs_build;
	cp -arf rootfs rootfs_build;
	rm -rf rootfs_build/include rootfs_build/usr/include rootfs_build/share rootfs_build/usr/share;
	find rootfs_build -name "*.a" | xargs rm -f
	@for ff in $(shell find rootfs_build -type f); do \
		$(STRIP) $$ff 2>/dev/null && echo "STRIP FILE :" $$ff;\
	done
	@echo "======================compile $(targets) complete======================"


clean: $(targets_clean)
	rm -rf $(BUILD_DESTDIR)
	@echo "======================clean $(targets) complete======================"

pptp_build:
	if [ ! -d $(SRC_PPTP) ]; then \
		tar xkf $(SRC_PPTP).tar.gz 2>/dev/null; \
	fi
	cd $(SRC_PPTP) && make DESTDIR=$(BUILD_DESTDIR) CC="$(CC)" IP="/sbin/ip" PPPD="/usr/sbin/pppd" && \
	fakeroot make install DESTDIR=$(BUILD_DESTDIR)
	@echo "compile $(SRC_PPTP) completed"

pptp_clean:
	if [ -d $(SRC_PPTP) ]; then \
		cd $(SRC_PPTP) && make clean; \
	fi

xl2tpd_build:
	if [ ! -d $(SRC_XL2TPD) ]; then \
		unzip -n $(SRC_XL2TPD).zip 2>/dev/null; \
	fi
	cd $(SRC_XL2TPD) && make PREFIX=$(BUILD_DESTDIR) && \
	make install PREFIX=$(BUILD_DESTDIR)
	@echo "compile $(SRC_XL2TPD) completed"

xl2tpd_clean:
	if [ -d $(SRC_XL2TPD) ]; then \
		cd $(SRC_XL2TPD) && make clean; \
	fi

pppd_build:
	if [ ! -e $(SRC_PPPD)/Makefile ]; then \
		tar xkf $(SRC_PPPD).tar.gz 2>/dev/null; \
		cd $(SRC_PPPD); \
		./configure \
		INSTROOT="$(BUILD_DESTDIR)" \
		DESTDIR="$(BUILD_DESTDIR)" \
		BINDIR=$(BUILD_DESTDIR)/usr/sbin; \
	fi
	cd $(SRC_PPPD) && make && make INSTROOT="$(BUILD_DESTDIR)" \
		INSTALL="install --strip-program=$(STRIP)" \
		DESTDIR="$(BUILD_DESTDIR)" \
		BINDIR=$(BUILD_DESTDIR)/usr/sbin install
	@echo "compile $(SRC_PPPD) completed"

pppd_clean:
	if [ -e $(SRC_PPPD)/Makefile ]; then \
		cd $(SRC_PPPD) && make clean;\
	fi

libgmp_build:
	if [ ! -e $(SRC_GMP)/Makefile ]; then \
		tar xkf $(SRC_GMP).tar.bz2 2>/dev/null; \
		cd $(SRC_GMP); \
		./configure \
		--host=$(BUILD_HOST) \
		--target=$(BUILD_TARGET) \
		--prefix=$(BUILD_DESTDIR) \
		--disable-silent-rules \
		--disable-dependency-tracking \
		--enable-cxx=detect \
		--with-readline=no; \
	fi
	cd $(SRC_GMP) && make && make install
	@echo "compile $(SRC_GMP) completed"

libgmp_clean:
	if [ -e $(SRC_GMP)/Makefile ]; then \
		cd $(SRC_GMP) && make clean; \
	fi

strongswan_build:
	if [ ! -e $(SRC_STRONGWAN)/Makefile ]; then \
		tar xkf $(SRC_STRONGWAN).tar.bz2 2>/dev/null; \
		cd $(SRC_STRONGWAN); \
		./configure \
		--host=$(BUILD_HOST) \
		--target=$(BUILD_TARGET) \
		--prefix=/ \
		--disable-silent-rules \
		--disable-dependency-tracking \
		--without-lib-prefix \
		--without-systemdsystemunitdir \
		--disable-aesni \
		--enable-charon \
		--enable-curl \
		--enable-gmp \
		--enable-eap-md5 \
		--disable-ldap \
		--disable-mysql \
		--enable-openssl \
		--disable-scepclient \
		--disable-soup \
		--enable-sqlite \
		--enable-stroke \
		--disable-swanctl \
		--disable-systemd \
		CFLAGS="$(CFLAGS)" \
		LDFLAGS="$(LDFLAGS)"; \
	fi
	cd $(SRC_STRONGWAN) && make && make install DESTDIR=$(BUILD_DESTDIR)
	@echo "compile $(SRC_STRONGWAN) completed"

strongswan_clean:
	if [ -e $(SRC_STRONGWAN)/Makefile ]; then \
		cd $(SRC_STRONGWAN) && make clean; \
	fi
```

最后 `ql-ol-sdk` 目录结构如下：

```text
ql-ol-sdk
├── Makefile
├── opensrc
│   ├── gmp-6.1.2.tar.bz2
│   ├── Makefile
│   ├── ppp-2.4.7.tar.gz
│   ├── pptp-1.10.0.tar.gz
│   ├── strongswan-5.6.2.tar.bz2
│   └── xl2tpd-1.3.11.zip
├── ql-ol-bootloader
├── ql-ol-crosstool
├── ql-ol-extsdk
├── ql-ol-kernel
├── ql-ol-rootfs
├── ql-ol-usrdata
├── ql-ol-usrfs
└── target
```

### 2.3. 修改内核编译选项

移植 VPN 软件，需修改内核编译选项。以修改 PPTP、L2TP 和 IPSEC 的内核编译选项文件为例，文件路径如下：

```text
ql-ol-kernel/msm-3.18/arch/arm/configs/mdm9607-perf_defconfig
```

修改后的内核编译选项如下：

**PPTP：**
```text
CONFIG_PPP_MPPE=y
```

**L2TP：**
```text
CONFIG_NET_UDP_TUNNEL=y
CONFIG_L2TP=y
```

**IPSEC：**
```text
CONFIG_INET_AH=m
CONFIG_INET_ESP=m
CONFIG_INET_IPCOMP=m
CONFIG_INET_XFRM_TUNNEL=m
CONFIG_INET_TUNNEL=m
CONFIG_XFRM_USER=m
```

### 2.4. 编译文件

PPTP 和 L2TP 的使用依赖于 PPP 相关插件，在使用 PPTP 和 L2TP 之前，需要重新编译 PPP。IPSEC 使用 strongswan，而 strongswan 的使用依赖于库 libgmp，所以在编译 strongswan 之前，需要先编译 libgmp。以编译 strongswan 为例：

**1.** strongswan 在编译配置过程中需要使用 m4 工具。以 Ubuntu 环境为例，运行如下命令安装 m4 工具：

```sh
sudo apt install m4
```

**2.** 进入 `ql-ol-sdk` 目录，运行如下命令编译 strongswan：

```sh
source ql-ol-crosstool/ql-ol-crosstool-env-init
make kernel
make kernel_module
cd opensrc
make
```

**3.** 编译完成后，目标文件位于路径 `opensrc/rootfs_build` 下。

### 2.5. 将编译生成的文件打包至 rootfs

编辑完 `ql-ol-sdk` 目录中的 `makefile` 文件后，执行 `make rootfs` 命令，将 `opensrc` 目录中编译出的文件打包至 `ql-ol-sdk/ql-ol-rootfs` 路径下的 rootfs。具体需编辑 SDK 包中的 `Makefile`（原文档以"修改前 / 修改后"两张截图说明，此处略去截图）。

---

## 3 软件测试

本章节介绍了如何对 QuecOpen 方案下的 VPN 软件进行测试，以确认是否移植成功。以 `pptp-linux`、`xl2tpd`、`strongswan` 为例，这些软件使用的协议分别为 PPTP、L2TP、IPSEC。

### 3.1. PPTP

如下举例说明如何使用认证用户名（`test`）密码（`11111111`）连接 PPTP 服务器配置（`192.168.20.49`）。

**1.** 编辑 `/etc/PPP/chap-secrets` 路径下的文件，添加认证用户名和密码。

```text
test * 11111111 *
```

**2.** 编辑 `/etc/PPP/peers/pptpvpn` 路径下的文件。

```text
pty "pptp 192.168.20.49 --nolaunchpppd"
lock
noauth
nobsdcomp
nodeflate
name test
remotename pptpvpn
ipparam pptpvpn
require-mppe-128
```

**3.** 开始拨号。

```sh
pppd call pptpvpn updetach
```

**4.** 若拨号状态正常（原文档为截图），则表明移植成功。

### 3.2. L2TP

如下举例说明如何使用用户名（`test`）密码（`11111111`）连接 L2TP 服务器配置（`192.168.20.49`）。

**1.** 编辑 xl2tpd 的配置文件，文件路径为 `/etc/xl2tpd/xl2tpd.conf`。

```ini
[global]
port = 1701
debug state = yes
debug tunnel = yes

[lac testvpn]
lns = 192.168.20.49
require chap = yes
refuse pap = yes
require authentication = yes
name = test
ppp debug = yes
pppoptfile = /etc/ppp/peers/testvpn.l2tpd
length bit = yes
```

**2.** 创建 `/etc/xl2tpd/xl2tpd.conf` 路径下的文件中指定的 L2TP 连接配置文件，文件路径为 `/etc/PPP/peers/testvpn.l2tpd`。

```text
user test
password 11111111
noauth
lock
lcp-echo-interval 3
lcp-echo-failure 30
asyncmap 0
```

**3.** 开始拨号：

```sh
mkdir /var/run/xl2tpd
xl2tpd –D &
echo "c testvpn" > /var/run/xl2tpd/l2tp-control
```

**4.** 若拨号状态正常（原文档为截图），则表明移植成功。

### 3.3. IPSEC

strongswan 的测试配置可参考 https://www.strongswan.org/testresults.html ，采用 `ikev1/net2net-psk` 场景来测试。服务器（`192.168.10.154`）为 WAN 侧一台运行 strongswan 的主机，客户端（`192.168.22.17`）为移远通信模块。

#### 3.3.1. 服务器配置（192.168.10.154）

**1.** 编辑 `/etc/IPSEC.conf` 路径下的文件。

```text
config setup

conn %default
    ikelifetime=60m
    keylife=20m
    rekeymargin=3m
    keyingtries=1
    keyexchange=ikev2
    authby=secret

conn net-net
    left=192.168.10.154
    leftid=@moon.strongswan.org
    right=192.168.22.17
    rightid=@sun.strongswan.org
    auto=add
```

**2.** 编辑 `/etc/IPSEC.secrets` 路径下的文件。

```text
@moon.strongswan.org @sun.strongswan.org : PSK 0sv+NkxY9LLZvwj4qCC2o/gGrWDF2d21jL
```

**3.** 编辑 `/etc/strongswan.conf` 路径下的文件。

```text
charon {
    load = random nonce aes sha1 sha2 curve25519 hmac stroke kernel-netlink socket-default updown
}
```

**4.** 启动 strongswan 服务。

```sh
sudo ipsec start --nofork --debug-all
```

#### 3.3.2. 模块配置（192.168.22.17）

**1.** 编辑 `/etc/IPSEC.conf` 路径下的文件。

```text
config setup

conn %default
    ikelifetime=60m
    keylife=20m
    rekeymargin=3m
    keyingtries=1
    keyexchange=ikev2
    authby=secret

conn net-net
    left=192.168.22.17
    leftid=@sun.strongswan.org
    leftfirewall=yes
    right=192.168.10.154
    rightid=@moon.strongswan.org
    auto=add
```

**2.** 编辑 `/etc/ipsec.secrets` 路径下的文件。

```text
@moon.strongswan.org @sun.strongswan.org : PSK 0sv+NkxY9LLZvwj4qCC2o/gGrWDF2d21jL
```

**3.** 编辑 `/etc/strongswan.conf` 路径下的文件。

```text
charon {
    load = random nonce aes sha1 sha2 curve25519 hmac stroke kernel-netlink socket-default updown
}
```

**4.** 开始拨号（原文档为截图）。

**5.** 若拨号状态正常（原文档为截图），则表明移植成功。

---

## 4 附录 A 术语缩写

**表 2：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| GMP | GNU Multiple Precision Arithmetic Library | GNU 多重精度运算库 |
| IPSEC | Internet Protocol Security | 互联网安全协议 |
| L2TP | Layer-2 Tunneling Protocol | 第二层隧道协议 |
| PPTP | Point-to-Point Tunneling Protocol | 点对点隧道协议 |
| PPP | Point to Point Protocol | 点对点通信协议 |
| SDK | Software Development Kit | 软件开发工具包 |
| VPN | Virtual Private Network | 虚拟私人网络 |
| WAN | Wide Area Network | 广域网 |
