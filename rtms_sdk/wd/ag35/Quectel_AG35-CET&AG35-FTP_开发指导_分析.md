# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) FTP 开发指导 — 全量分析

> 本文是对原始 PDF《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_FTP_开发指导_V1.0.0_Preliminary_20240201.pdf》的逐章全量精读还原。
> 讲的是**如何在 QuecOpen 模块上启动并使用 BusyBox 自带的 FTP 服务**（三种启动方式 + 账号设置 + 客户端验证）。**纯运维/调试用途，无 C SDK API。**

---

## 0. 文档元信息

| 项 | 内容 |
|---|---|
| 文档名 | AG35-CET&AG35-EUT QuecOpen(SDK) FTP 开发指导 |
| 适用模块系列 | LTE Standard：**AG35-CET、AG35-EUT** |
| 版本 | 1.0.0（**临时版本 Preliminary**） |
| 日期 | 2024-02-01 |
| 总页数 | 16 页（正文 7~15） |
| 作者 | Yumn HUANG |

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| -（创建） | 2024-02-01 | Yumn HUANG | 文档创建 |
| 1.0.0 | 2024-02-01 | Yumn HUANG | 临时版本 |

---

## 1. 整书目录树

```
1  引言
2  FTP 服务
   2.1  概述（BusyBox 自带，默认不启动）
   2.2  启动方式
        2.2.1  通过 shell 命令行启动（tcpsvd）
        2.2.2  通过 init.d 服务启动（start_ftpd_le 脚本）
        2.2.3  通过 inetd 服务启动（inetd.busybox + inetd.conf）
   2.3  FTP 登录账号用户名/密码设置（passwd / group）
   2.4  FTP 服务验证和使用（FileZilla）
3  附录 参考文档及术语缩写
```

> 含 6 张界面截图（帮助信息/各启动方式/FileZilla），命令与配置文本可 100% 还原。

---

## 2. 逐章全量内容

### 第 1 章 引言

- 介绍 QuecOpen 方案下如何**启动和验证 AG35-CET/EUT 的 FTP 服务**。

### 第 2 章 FTP 服务

#### 2.1 概述

- FTP = 基于 TCP 的文件传输协议，客户端/服务器模式。
- 模块的 FTP 服务是 **BusyBox 自带服务，默认不启动**。
- shell 执行 `ftpd` 查看使用帮助。

#### 2.2 启动方式（三选一）

**2.2.1 shell 命令行直接启动**：
```sh
tcpsvd -vE 0.0.0.0 21 ftpd -w /data
```
- `0.0.0.0` 监听 IP；`21` FTP 端口；`-w` 允许上传；`/data` FTP 目录。
- **关键坑**：`/data` 目录所有者**必须为登录用户**，否则权限问题 → `chown user:<username> /data`。

**2.2.2 init.d 服务启动**（需自带脚本 `start_ftpd_le`）：
- `/etc/` 只读，脚本须上传到 `/data/` 或放进 SDK `ql-ol-rootfs/etc/init.d/`。
- 脚本核心（START=80 STOP=99）：
  ```sh
  PROG=/usr/sbin/ftpd; DAEMON=/usr/bin/tcpsvd; IP=0.0.0.0; PORT=21; PATH=/data
  start() { /sbin/start-stop-daemon -S -b -a $DAEMON -- -vE $IP $PORT $PROG -w $PATH; }
  stop()  { /sbin/start-stop-daemon -K -x $DAEMON; }
  ```
- `chmod 777 /etc/init.d/start_ftpd_le` → `start_ftpd_le start` / `stop` 启停。

**2.2.3 inetd 服务启动**（按需托管）：
- 加 `inetd.busybox` 到 `ql-ol-rootfs/etc/init.d/`（START=80 STOP=99，调 `/usr/sbin/inetd`）。
- 在 `ql-ol-rootfs/etc/inetd.conf` 加一行：
  ```
  21 stream tcp nowait root ftpd ftpd -w /data
  ```
- 编译 rootfs 烧录后：`/etc/init.d/inetd.busybox start` / `stop`。

#### 2.3 登录账号设置

- **内置账号**：用户名 `quectel` / 密码 `oelinux123`（FTP 目录所有者须为 `quectel`）。
- **自定义账号**：
  - 改 `ql-ol-rootfs/etc/passwd` 加 `user:x:126:126:user:/user:/bin/ash`（用户名:密码:UID:GID:描述:目录:shell）。
  - 改 `ql-ol-rootfs/etc/group` 加 `user:x:126:user`（组名:组密码:GID:描述）。

#### 2.4 验证使用（FileZilla）

- Host `192.168.225.1` / username `quectel` / password `oelinux123` → 连接后上传/下载。

---

## 3. 关键警告与坑（手册汇总）

1. **FTP 默认不启动**，需手动选一种方式启用。
2. **FTP 目录所有者必须 = 登录用户**（内置账号下须为 `quectel`），否则上传/下载权限报错 → `chown`。
3. **`/etc/` 只读** —— init.d/inetd 脚本要么放 `/data/`，要么编进 SDK rootfs 重烧。
4. **默认口令 `oelinux123`** —— 出厂默认，量产/联网暴露需改强口令（与 Log抓取文档同一默认口令）。
5. **inetd 方式需重编 rootfs** 才生效，不如 shell/init.d 灵活。

---

## 4. 对 open_dial 项目的适用性批注

> open_dial 跑 **EC200A**，本文档面向 **AG35**。FTP 走的是 **BusyBox 通用机制**（`tcpsvd`/`ftpd`/`inetd`），与具体模块型号无关，**EC200A 若带同款 BusyBox 即可直接套用**（实测 `which ftpd tcpsvd inetd` 确认）。

| 文档内容 | 与 open_dial 的关系 / 可借鉴点 |
|---|---|
| **BusyBox ftpd 一行启动** | 可作为现场**取日志/取证的临时通道**：`tcpsvd -vE 0.0.0.0 21 ftpd -w /media/sdcard/dial_log` 临时开 FTP，把 `dial_log`、`/sdcard/` CP dump 拉出来分析，免插卡/拆机。用完即关。 |
| **目录所有者权限坑** | 若用 FTP 拉 `dial_log`，注意 open_dial 日志根 `/media/sdcard/dial_log` 的属主，否则下载权限失败。 |
| **默认口令 `oelinux123`** | 安全提醒：open_dial 设备若联网且开了 FTP/SSH，务必改默认口令，避免暴露。 |

**结论**：本文档对 open_dial **相关度低**，与拨号守护核心逻辑无关。唯一实用价值是**现场临时取证通道**（开 FTP 拉日志/CP dump）。生产环境不建议常开 FTP（明文 + 默认口令有安全风险），仅调试临时用。
