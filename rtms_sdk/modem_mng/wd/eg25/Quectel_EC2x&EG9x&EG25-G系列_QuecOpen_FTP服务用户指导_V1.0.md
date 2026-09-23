# EC2x&EG9x&EG25-G 系列 QuecOpen FTP 服务用户指导

**LTE Standard 模块系列**

- 版本：EC2x&EG9x&EG25-G 系列_QuecOpen_FTP 服务用户指导_V1.0
- 日期：2020-07-17
- 状态：受控文件

---

## 公司及联系方式

上海移远通信技术股份有限公司始终以为客户提供最及时、最全面的服务为宗旨。如需任何帮助，请随时联系我司上海总部，联系方式如下：

**上海移远通信技术股份有限公司**
上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼　　邮编：200233
电话：+86 21 51086236　邮箱：info@quectel.com

或联系我司当地办事处，详情请登录：
http://www.quectel.com/cn/support/sales.htm

如需技术支持或反馈我司技术文档中的问题，可随时登陆如下网址：
http://www.quectel.com/cn/support/technical.htm
或发送邮件至：support@quectel.com

### 前言

上海移远通信技术股份有限公司提供该文档内容用以支持其客户的产品设计。客户须按照文档中提供的规范、参数来设计其产品。由于客户操作不当而造成的人身伤害或财产损失，本公司不承担任何责任。在未声明前，上海移远通信技术股份有限公司有权对该文档进行更新。

### 版权申明

本文档版权属于上海移远通信技术股份有限公司，任何人未经我司允许而复制转载该文档将承担法律责任。

版权所有 ©上海移远通信技术股份有限公司 2020，保留一切权利。
Copyright © Quectel Wireless Solutions Co., Ltd. 2020.

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| 1.0 | 2020-07-17 | 周守亚 / 孙保 | 初始版本 |

---

## 目录

- 文档历史
- 目录
- 表格索引
- 图片索引
- 1 引言
  - 1.1. 适用模块
- 2 FTP 服务
  - 2.1. 概述
  - 2.2. 启动方式
    - 2.2.1. 通过 Shell 命令行启动
    - 2.2.2. 通过 init.d 服务启动
    - 2.2.3. 通过 inetd 服务启动
  - 2.3. FTP 服务验证和使用
- 3 附录 A 术语缩写

### 表格索引

- 表 1：适用模块
- 表 2：术语缩写

### 图片索引

- 图 1：FTP 使用帮助信息
- 图 2：通过 Shell 命令行启动 FTP 服务
- 图 3：通过 init.d 服务启动/关闭 FTP 服务
- 图 4：通过 inetd 服务启动/关闭 FTP 服务
- 图 5：FTP 客户端连接 FTP 服务器
- 图 6：浏览器访问 FTP 服务

---

## 1 引言

移远通信 LTE Standard EC2x&EG9x&EG25-G 系列模块支持 QuecOpen® 方案。本文档主要用于指导客户如何启动及验证使用这些模块的 FTP 服务。

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

## 2 FTP 服务

### 2.1. 概述

FTP 即文件传输协议，是一种基于 TCP 的协议，采用客户端/服务器模式。通过 FTP 协议，用户可以在 FTP 服务器中进行文件的上传或下载等操作。

移远通信 EC2x&EG9x&EG25-G 系列 QuecOpen 模块提供的 FTP 服务是 Busybox 自带服务。默认不启动，可通过第 2.2 章所述方式启动。

在模块 shell 命令行执行如下命令可查看 FTP 使用帮助信息，如下图所示。

```bash
ftpd
```

> **图 1：FTP 使用帮助信息** —— 在 shell 中执行 `ftpd` 后显示的使用帮助信息。

### 2.2. 启动方式

EC2x&EG9x&EG25-G 系列 QuecOpen 模块支持三种方式启动 FTP 服务，分别为通过 shell 命令行、init.d 服务或 inetd 服务。用户可根据实际情况选择。本章节主要介绍这三种方式以及各自的启动步骤。

#### 2.2.1. 通过 Shell 命令行启动

通过如下 shell 命令行启动 FTP 服务。

```bash
tcpsvd -vE 0.0.0.0 21 ftpd -w /data
```

参数说明：

- `0.0.0.0`　表示 IP 地址
- `21`　表示 FTP 端口
- `-w`　表示 FTP 有上传权限
- `/data`　表示 FTP 目录

> **图 2：通过 Shell 命令行启动 FTP 服务** —— 执行 `tcpsvd -vE 0.0.0.0 21 ftpd -w /data` 启动 FTP 服务的终端输出。

#### 2.2.2. 通过 init.d 服务启动

如需通过 init.d 服务启动 FTP 服务，则需模块集成 FTP 服务启动脚本 `start_ftpd_le`。目前模块内部默认未集成该脚本，需将该脚本上传至模块的 `/etc/init.d` 目录（或放至 SDK 包中的 `ql-ol-rootfs/etc/init.d` 目录），方可通过 init.d 服务启动 FTP 服务。

1. 上传 `start_ftpd_le` 文件至 `/etc/init.d` 目录。文件内容示例如下：

   ```sh
   #! /bin/sh
   #
   # Copyright (c) 2009-2020 @ Quectel Wireless Solutions Co., Ltd. All Rights Reserved.
   #
   # FTP Server init.d script to start the busybox FTP daemon

   DAEMON=/bin/tcpsvd
   IP=0.0.0.0
   PORT=21
   PATH=/data

   set -e

   case "$1" in
     start)
            echo -n "Starting Busybox FTP Server: "
            /sbin/start-stop-daemon -S -b -a $DAEMON -- -vE $IP $PORT /sbin/ftpd -w $PATH
            echo "done"
            ;;
     stop)
            echo -n "Stopping Busybox FTP Server: "
            /sbin/start-stop-daemon -K -x $DAEMON
            echo "done"
            ;;
     restart)
            $0 stop
            $0 start
            ;;
     *)
            echo "Usage $0 { start | stop | restart}" >&2
            exit 1
            ;;
   esac

   exit 0
   ```

2. 在模块 shell 环境下执行如下命令启动 FTP 服务。

   ```bash
   /etc/init.d/start_ftpd_le start
   ```

   在模块 shell 环境下执行如下命令可关闭 FTP 服务。

   ```bash
   /etc/init.d/start_ftpd_le stop
   ```

   > **图 3：通过 init.d 服务启动/关闭 FTP 服务** —— 执行 `start_ftpd_le start` / `stop` 启动与关闭 FTP 服务的终端输出。

#### 2.2.3. 通过 inetd 服务启动

1. 修改模块内部 `/etc/init.d/inetd.busybox` 文件，文件内容示例如下。

   ```sh
   #!/bin/sh
   #
   # start/stop inetd super server.

   INETD_DAEMON=/sbin/inetd

   if ! [ -x $INETD_DAEMON ]; then
          exit 0
   fi

   case "$1" in
       start)
       echo -n "Starting internet superserver:"
       echo -n " inetd" ; start-stop-daemon -S -x $INETD_DAEMON > /dev/null
       echo "."
       ;;
       stop)
       echo -n "Stopping internet superserver:"
       echo -n " inetd" ; start-stop-daemon -K -x $INETD_DAEMON > /dev/null
       echo "."
       ;;
       restart)
       echo -n "Restarting internet superserver:"
       echo -n " inetd "
       killall -HUP inetd
       echo "."
       ;;
       *)
       echo "Usage: /etc/init.d/inetd {start|stop|restart}"
       exit 1
       ;;
   esac

   exit 0
   ```

2. 在 `/etc/inetd.conf` 配置文件内容中添加一行信息：

   ```
   21 stream    tcp   nowait root ftpd ftpd -w /data
   ```

   参数说明：

   - `21`　表示 FTP 端口号
   - `/data`　表示 FTP 目录

   配置文件示例如下：

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
   21      stream tcp nowait root ftpd ftpd -w /data
   ```

3. 在 shell 命令行下输入如下命令启动服务。

   ```bash
   /etc/init.d/inetd.busybox start
   ```

   在 shell 命令行下输入如下命令可关闭服务。

   ```bash
   /etc/init.d/inetd.busybox stop
   ```

   > **图 4：通过 inetd 服务启动/关闭 FTP 服务** —— 执行 `inetd.busybox start` / `stop` 启动与关闭 FTP 服务的终端输出。

### 2.3. FTP 服务验证和使用

可通过如下步骤来验证 FTP 服务连接及使用。

1. **FTP 客户端连接 FTP 服务器。**

   启动 FTP 服务后，FTP 客户端连接 FTP 服务器，用户名和密码需与通过调试串口方式登录模块的用户名和密码一致。示例如下图所示（以 Windows 操作系统为例，其中连接时的用户名为 `root`，密码为 `quectel123`）。连接成功后方可使用 FTP 服务。

   > **图 5：FTP 客户端连接 FTP 服务器（Windows 操作系统）** —— FTP 客户端输入服务器地址、用户名 `root`、密码 `quectel123` 连接 FTP 服务器。

2. **FTP 服务连接成功后，使用浏览器访问 FTP 服务。**

   > **图 6：浏览器访问 FTP 服务（Windows 操作系统）** —— 在浏览器中通过 FTP 地址访问并浏览 FTP 服务目录内容。

---

## 3 附录 A 术语缩写

**表 2：术语缩写**

| 术语 | 英文全称 | 中文全称 |
|---|---|---|
| FTP | File Transfer Protocol | 文件传输协议 |
| IP | Internet Protocol | 网际互连协议 |
| LTE | Long Term Evolution | 长期演进 |
| SDK | Software Development Kit | 软件开发工具包 |
