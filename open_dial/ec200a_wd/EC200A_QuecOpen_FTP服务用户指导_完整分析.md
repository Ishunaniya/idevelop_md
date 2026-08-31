# EC200A-CN(TA) QuecOpen FTP 服务用户指导 — 完整分析

> **源文档**：Quectel_EC200A-CN(TA)_QuecOpen_FTP服务用户指导_V1.0.0_Preliminary_20220719.pdf
> **适用平台**：LTE Standard 模块系列 — EC200A-CN(TA) QuecOpen（OpenCPU / 基于 Linux 的嵌入式开发平台）
> **版本**：1.0.0　**日期**：2022-07-19　**状态**：临时文件（Preliminary）
> **原文页数**：16 页

---

## 文档历史（修订记录）

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| - | 2022-07-19 | Allen FENG | 文档创建 |
| 1.0.0 | 2022-07-19 | Allen FENG | 临时版本 |

---

## 1 引言

- 移远通信 **EC200A-CN(TA)** 系列模块支持 **QuecOpen®** 方案。QuecOpen® 是**开源的、基于 Linux 的嵌入式开发平台**，可简化 IoT 应用的软件设计和开发过程。详见参考文档 [1]《Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导》。
- 本文档主要介绍在 QuecOpen® 方案下，**如何启动及验证使用** EC200A-CN(TA) QuecOpen® 模块的 **FTP 服务**。

---

## 2 FTP 服务

### 2.1 概述

- **FTP（File Transfer Protocol，文件传输协议）**：一种基于 **TCP** 的协议，采用**客户端/服务器**模式。通过 FTP 协议，用户可以在 FTP 服务器中进行文件的**上传或下载**等操作。
- 移远通信 EC200A-CN(TA) 系列 QuecOpen 模块提供的 FTP 服务是 **BusyBox 自带服务**，**默认不启动**；可通过第 2.2 章所述方式启动。
- 在模块 shell 命令行执行如下命令可查看 **FTP 使用帮助信息**（图 1）：

```sh
ftpd
```

### 2.2 启动方式

EC200A-CN(TA) 系列 QuecOpen 模块支持**三种方式**启动 FTP 服务：
1. **shell 命令行**
2. **init.d 服务**
3. **inetd 服务**

用户可根据实际情况选择启动方式。

---

#### 2.2.1 通过 shell 命令行启动

通过如下 shell 命令行启动 FTP 服务：

```sh
tcpsvd -vE 0.0.0.0 21 ftpd -w /data
```

**参数说明：**

| 参数 | 含义 |
|---|---|
| `0.0.0.0` | IP 地址 |
| `21` | FTP 端口 |
| `-w` | FTP 有上传权限 |
| `/data` | FTP 目录 |

> ⚠️ **注意**：`/data` 目录的 **owner 必须为登录用户**，否则会出现权限问题。可使用 `chown user:<username> /data` 修改 owner。

（对应图 2：通过 Shell 命令行启动 FTP 服务）

---

#### 2.2.2 通过 init.d 服务启动

如需通过 init.d 服务启动 FTP 服务，则需模块集成 FTP 服务启动脚本 **`start_ftpd_le`**。目前模块内部**默认未集成该脚本**，用户需将该脚本上传至模块的 `/etc/init.d` 目录，或放至 SDK 包中的 `ql-ol-rootfs/etc/init.d` 目录，方可通过 init.d 服务启动。

**步骤 1**：上传 `start_ftpd_le` 文件至 `/etc/init.d` 目录（或 SDK 包中的 `ql-ol-rootfs/etc/init.d`）。文件内容示例如下：

```sh
#!/bin/sh /etc/rc.common
#
# Copyright (c) 2009-2022 @ Quectel Wireless Solutions Co., Ltd. All Rights Reserved.
#
# FTP Server init.d script to start the busybox FTP daemon

START=80
STOP=99

PROG=/usr/sbin/ftpd
DAEMON=/usr/bin/tcpsvd
IP=0.0.0.0
PORT=21
PATH=/data

start() {
        /sbin/start-stop-daemon -S -b -a $DAEMON -- -vE $IP $PORT $PROG -w $PATH
}

stop() {
        /sbin/start-stop-daemon -K -x $DAEMON
}

restart() {
        stop
        start
}
```

> ⚠️ **注意**：`/data` 的 owner 必须为登录用户，否则会出现权限问题（使用 `chown user:<username> /data` 可修改 owner）。

**步骤 2**：在模块 shell 环境下执行如下命令设置脚本文件权限为可读、可写、可执行：

```sh
chmod 777 /etc/init.d/start_ftpd_le
```

**步骤 3**：启动 FTP 服务：

```sh
/etc/init.d/start_ftpd_le start
```

**步骤 4**：关闭 FTP 服务：

```sh
/etc/init.d/start_ftpd_le stop
```

（对应图 3：通过 init.d 服务启动/关闭 FTP 服务）

---

#### 2.2.3 通过 inetd 服务启动

**步骤 1**：添加 `/etc/init.d/inetd.busybox` 文件并加上可执行权限，文件内容示例如下：

```sh
#!/bin/sh /etc/rc.common
#
# Copyright (c) 2009-2022 @ Quectel Wireless Solutions Co., Ltd. All Rights Reserved.
#
# inetd Server
START=80
STOP=99

INETD_DAEMON=/usr/sbin/inetd

if ! [ -x $INETD_DAEMON ]; then
        exit 0
fi

start() {
        start-stop-daemon -S -x $INETD_DAEMON > /dev/null
}

stop() {
        start-stop-daemon -K -x $INETD_DAEMON
}

restart() {
        stop
        start
}
```

**步骤 2**：在 `/etc/inetd.conf` 配置文件内容中添加一行信息：

```
21 stream    tcp    nowait root ftpd ftpd -w /data
```

**说明：**

| 参数 | 含义 |
|---|---|
| `21` | FTP 端口号 |
| `/data` | FTP 目录 |

> ⚠️ **注意**：`/data` 目录的 owner 必须为登录用户，否则会出现权限问题（使用 `chown user:<username> /data` 可修改 owner）。

`/etc/inetd.conf` 配置文件示例如下：

```
# /etc/inetd.conf: see inetd(8) for further informations.
#
# Internet server configuration database
#
# If you want to disable an entry so it isn't touched during
# package updates just comment it out with a single '#' character.
#
# <service_name> <sock_type> <proto> <flags> <user> <server_path> <args>
#
#:INTERNAL: Internal services
21         stream tcp nowait root ftpd ftpd -w /data
```

**步骤 3**：在 shell 命令行下输入如下命令启动 FTP 服务：

```sh
/etc/init.d/inetd.busybox start
```

**步骤 4**：关闭 FTP 服务：

```sh
/etc/init.d/inetd.busybox stop
```

（对应图 4：通过 inetd 服务启动/关闭 FTP 服务）

---

### 2.3 FTP 服务登录账号密码设置

用户可使用**系统内置账号**登录 FTP 服务。

> ⚠️ **重要**：对应的 FTP 目录的 **owner 必须为 `quectel`**，否则在上传或下载文件时会出现权限问题。

**内置账号密码：**

| 项 | 值 |
|---|---|
| 用户名 | `quectel` |
| 密码 | `oelinux123` |

**设置自定义账号密码方法：**

**步骤 1**：在 `/etc/passwd` 添加如下行：

```
user:x:126:126:user:/user:/bin/ash
```

**字段说明：**

| 字段 | 含义 |
|---|---|
| `user` | 用户名 |
| `x` | 密码 |
| `126` | 用户 ID |
| `126` | 组 ID |
| `user` | 用户描述 |
| `/user` | 用户家目录 |
| `/bin/ash` | 命令行解释器 |

> 各列用户可自行定义具体的值。

**步骤 2**：在 `/etc/group` 添加如下行：

```
user:x:126:user
```

**字段说明：**

| 字段 | 含义 |
|---|---|
| `user` | 用户组名 |
| `x` | 组密码 |
| `126` | 组 ID |
| `user` | 组描述 |

> 各列用户可自行定义具体的值。

**步骤 3**：使用 `passwd` 命令修改密码（**必须执行**）。

（对应图 5：修改用户密码）

---

### 2.4 FTP 服务验证和使用

可通过如下步骤来验证 FTP 服务连接及使用：

**步骤 1**：连接 FTP 客户端至 FTP 服务器。

启动 FTP 服务后，连接 FTP 客户端至 FTP 服务器。Linux 操作系统下，使用 **FileZilla** 程序连接 FTP 服务器：

| 项 | 值 |
|---|---|
| Host（主机） | `192.168.225.1` |
| username（用户名） | `quectel` |
| password（密码） | `oelinux123` |

> 上述为默认账号，用户可以添加自定义的账号密码。

（对应图 6：FTP 客户端连接 FTP 服务器）

**步骤 2**：FTP 服务连接成功后，使用 FileZilla 进行**上传或者下载**操作。

（对应图 7：FileZilla 访问 FTP 服务）

---

## 3 附录 参考文档及术语缩写

### 表 1：参考文档

| 序号 | 文档名称 |
|---|---|
| [1] | Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

### 表 2：术语缩写

| 术语 | 英文全称 | 中文全称 |
|---|---|---|
| FTP | File Transfer Protocol | 文件传输协议 |
| IoT | Internet of Things | 物联网 |
| IP | Internet Protocol | 互连协议 |
| SDK | Software Development Kit | 软件开发工具包 |
| TCP | Transmission Control Protocol | 传输控制协议 |

---

## 关键要点速查（实践提炼）

- FTP 服务由 **BusyBox** 提供，**默认关闭**，三种启动方式任选其一。
- 默认 FTP 端口 **21**，默认目录 `/data`，`-w` 开放上传权限。
- 默认账号 `quectel` / `oelinux123`；**目录 owner 必须匹配登录用户**（内置账号为 `quectel`），否则上传/下载报权限错误。
- init.d 方式需自行上传 `start_ftpd_le` 脚本并 `chmod 777`；inetd 方式需配置 `/etc/inetd.conf` 与 `inetd.busybox` 脚本。
- 自定义账号需改 `/etc/passwd` + `/etc/group`，并**务必用 `passwd` 重新设置密码**。
- 默认模块 IP 为 `192.168.225.1`，可用 FileZilla 验证连接。
