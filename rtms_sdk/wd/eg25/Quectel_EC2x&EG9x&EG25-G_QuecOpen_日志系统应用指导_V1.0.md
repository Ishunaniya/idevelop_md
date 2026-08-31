# EC2x&EG9x&EG25-G 系列 QuecOpen 日志系统应用指导

---

## 文档信息

| 项目 | 内容 |
|------|------|
| 文档标题 | EC2x&EG9x&EG25-G 系列 QuecOpen 日志系统应用指导 |
| 模块系列 | LTE Standard 模块系列 |
| 版本 | EC2x&EG9x&EG25-G 系列 QuecOpen_日志系统应用指导_V1.0 |
| 日期 | 2020-07-07 |
| 状态 | 受控文件 |
| 版权 | Copyright © Quectel Wireless Solutions Co., Ltd. 2020 |

---

## 文档历史

### 修订记录

| 版本 | 日期 | 作者 | 变更表述 |
|------|------|------|----------|
| 1.0 | 2020-07-07 | 匡昌胜 / 孙保 | 初始版本 |

---

## 目录

1. 引言
   - 1.1 适用模块
2. 日志系统
   - 2.1 日志系统概述
   - 2.2 日志的使用
   - 2.3 日志的查看
   - 2.4 日志的配置
3. 应用程序异常记录
   - 3.1 监听应用程序异常
   - 3.2 获取异常调用栈
4. 注意事项
5. 附录 A 术语缩写

---

## 表格索引

| 表格编号 | 表格名称 | 页码 |
|----------|----------|------|
| 表 1 | 适用模块 | 5 |
| 表 2 | 日志系统配置项 | 8 |
| 表 3 | 术语缩写 | 12 |

---

## 1 引言

移远通信 LTE Standard EC2x&EG9x&EG25-G 系列模块支持 QuecOpen® 方案，本文档主要描述这些模块日志系统的使用、查看和配置，以及相关应用程序异常记录以及注意事项。

### 1.1 适用模块

**表 1：适用模块**

| 模块系列 | 模块 |
|----------|------|
| EC2x 系列 | EC25 系列 |
| EC2x 系列 | EC21 系列 |
| EC2x 系列 | EC20 R2.1 |
| EC2x 系列 | EC20-CN |
| EG9x 系列 | EG95 系列 |
| EG9x 系列 | EG91 系列 |
| EG25-G | EG25-G |

---

## 2 日志系统

### 2.1 日志系统概述

EC2x&EG9x&EG25-G 系列 QuecOpen® 方案中的日志系统，采用 Android 的 Logcat 方案。Logcat 包含 4 个环形读写缓冲区（MAIN、RADIO、EVENTS 和 SYSTEM）和 6 个日志等级（VERBOSE、DEBUG、INFO、WARN、ERROR 和 FATAL）；4 个环形读写缓冲区的使用情况如下：

| 缓冲区名称 | 用途 |
|-----------|------|
| MAIN | 客户应用进程使用 |
| RADIO | 弃用 |
| EVENTS | 记录系统事件 |
| SYSTEM | 记录系统关键服务进程日志 |

可使用 Logcat 命令查看日志，并根据 tag 和日志等级进行过滤，具体用法可参考 https://developer.android.com/studio/command-line/logcat 。

### 2.2 日志的使用

日志既可采用 Android 封装的 liblog 系统，也可采用 QuecOpen® 方案中封装的 qlsyslog 系统。本章节主要介绍 qlsyslog 的使用。

**头文件：** `qlsyslog/ql_sys_log.h`

**库：** `libql_sys_log.so`

日志接口定义位于头文件 `qlsyslog/ql_sys_log.h` 中。日志接口的使用可参考如下的示例代码，并在 Makefile 中，将 `-lql_sys_log` 添加到链接选项中。

**示例代码：**

```c
#include "qlsyslog/ql_sys_log.h"

#define LOG_TAG "fortest"

int main(int argc, char *argv)
{
    QLOGV(LOG_TAG, "I am QL_SYS_LOG_VERBOSE");
    QLOGD(LOG_TAG, "I am QL_SYS_LOG_DEBUG");
    QLOGI(LOG_TAG, "I am QL_SYS_LOG_INFO");
    QLOGW(LOG_TAG, "I am QL_SYS_LOG_WARN");
    QLOGE(LOG_TAG, "I am QL_SYS_LOG_ERROR");
    QLOGF(LOG_TAG, "I am QL_SYS_LOG_FATAL");

    return 0;
}
```

**日志宏说明：**

| 宏名称 | 对应日志等级 | 说明 |
|--------|-------------|------|
| `QLOGV(tag, msg)` | VERBOSE | 详细日志，最低优先级 |
| `QLOGD(tag, msg)` | DEBUG | 调试日志 |
| `QLOGI(tag, msg)` | INFO | 信息日志 |
| `QLOGW(tag, msg)` | WARN | 警告日志 |
| `QLOGE(tag, msg)` | ERROR | 错误日志 |
| `QLOGF(tag, msg)` | FATAL | 致命错误日志，最高优先级 |

### 2.3 日志的查看

请使用 Logcat 工具查看日志，Logcat 工具的使用指南参考 https://developer.android.com/studio/command-line/logcat 。

本章节以第 **2.2 章** 的示例代码为例，简单介绍 Logcat 过滤、查看日志的三种使用方法。

**方法 1：通过 tag 过滤日志**

```bash
/data # logcat -s fortest
---------- beginning of system
---------- beginning of main
07-17 12:24:11.472  2039  2039 V fortest : I am QL_SYS_LOG_VERBOSE
07-17 12:24:11.472  2039  2039 D fortest : I am QL_SYS_LOG_DEBUG
07-17 12:24:11.472  2039  2039 I fortest : I am QL_SYS_LOG_INFO
07-17 12:24:11.472  2039  2039 W fortest : I am QL_SYS_LOG_WARN
07-17 12:24:11.472  2039  2039 E fortest : I am QL_SYS_LOG_ERROR
07-17 12:24:11.472  2039  2039 F fortest : I am QL_SYS_LOG_FATAL
```

**方法 2：通过等级过滤日志**

命令格式：`logcat -s <tag>:<level>`，其中等级字母：V=VERBOSE, D=DEBUG, I=INFO, W=WARN, E=ERROR, F=FATAL

```bash
/data # logcat -s fortest:w
---------- beginning of system
---------- beginning of main
07-17 12:24:11.472  2039  2039 W fortest : I am QL_SYS_LOG_WARN
07-17 12:24:11.472  2039  2039 E fortest : I am QL_SYS_LOG_ERROR
07-17 12:24:11.472  2039  2039 F fortest : I am QL_SYS_LOG_FATAL
```

上述命令表示只显示 WARN 及以上等级（WARN、ERROR、FATAL）的日志。

**方法 3：通过缓冲区过滤日志**

使用 `-b` 参数指定要读取的缓冲区（main / system / events）：

```bash
/data # logcat -b main
07-17 12:24:11.472  2039  2039 V fortest : I am QL_SYS_LOG_VERBOSE
07-17 12:24:11.472  2039  2039 D fortest : I am QL_SYS_LOG_DEBUG
07-17 12:24:11.472  2039  2039 I fortest : I am QL_SYS_LOG_INFO
07-17 12:24:11.472  2039  2039 W fortest : I am QL_SYS_LOG_WARN
07-17 12:24:11.472  2039  2039 E fortest : I am QL_SYS_LOG_ERROR
07-17 12:24:11.472  2039  2039 F fortest : I am QL_SYS_LOG_FATAL
```

**日志输出格式说明：**

```
<日期> <时间>  <PID>  <TID> <等级> <TAG> : <消息内容>
07-17 12:24:11.472  2039  2039 V fortest : I am QL_SYS_LOG_VERBOSE
```

| 字段 | 说明 |
|------|------|
| `07-17` | 月-日 |
| `12:24:11.472` | 时:分:秒.毫秒 |
| `2039` (第一个) | 进程 ID (PID) |
| `2039` (第二个) | 线程 ID (TID) |
| `V` | 日志等级（V/D/I/W/E/F） |
| `fortest` | LOG_TAG |
| `I am QL_SYS_LOG_VERBOSE` | 日志内容 |

### 2.4 日志的配置

qlsyslog 系统会根据过滤规则将内核日志和应用程序日志保存到 `log_file` 指定的位置中。配置文件位于 `/data/qllog.json`，配置文件为 JSON 格式，主要配置选项如下表。

**表 2：日志系统配置项**

| 配置项 | 选项 | 描述 |
|--------|------|------|
| `log_file` | 必选 | 日志文件保存位置。在系统运行中，可能会对日志文件频繁写入，不可将日志保存在关键系统分区。 |
| `rotate_file_size` | 必选 | 单个日志文件大小限制。单位：KB。 |
| `rotate_file_count` | 必选 | 最大日志文件个数。 |
| `log_format` | 可选 | 日志文件输出格式。可选 `default`、`csv`。 |
| `kernel_priority` | 可选 | 内核日志等级。内核日志等级即 printk 的等级，可选 m（突发性事件消息）、a（示警消息）、c（严重的软硬件错误消息）、e（错误信息）、w（关于问题状况的警告信息）、n（安全相关报告信息）、i（信息消息）、d（调试信息）和 `*`（所有等级）。 |
| `buffer_list.{i}.name` | 必选 | 缓冲区名称。可选 `main`、`system`、`events`。 |
| `buffer_list.{i}.filter_list.{i}.tag` | 可选 | 需要保存日志的 tag。若未设置，默认使用所有 tag。 |
| `buffer_list.{i}.filter_list.{i}.priority` | 可选 | 需要保存日志的等级。可选 v（VERBOSE）、d（DEBUG）、i（INFO）、w（WARN）、e（ERROR）、f（FATAL）和 `*`（所有等级）。 |

---

## 3 应用程序异常记录

### 3.1 监听应用程序异常

应用程序可通过监听信号来记录异常相关信息，示例代码如下。在异常处理程序中，函数 `ql_sys_log_signal(qlsyslog/ql_sys_log.h)` 将尽可能多的记录程序的异常信息。

**示例代码：**

```c
#include <signal.h>
#include <stdlib.h>
#include "qlsyslog/ql_sys_log.h"

#define LOG_TAG "fortest"

static void handle_signal(int sig_num, siginfo_t *info, void *ptr)
{
    ql_sys_log_signal(QL_SYS_LOG_ID_MAIN, QL_SYS_LOG_FATAL, LOG_TAG, sig_num, info, ptr);
    exit(-1);
}

int main(int argc, char *argv)
{
    struct sigaction sa = {0};
    sa.sa_sigaction = handle_signal;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);

    QLOGI(LOG_TAG, "bootup");

    /* other code */

    return 0;
}
```

**代码说明：**

- 通过 `sigaction()` 注册多种信号处理器，监听以下信号：
  - `SIGTERM`：终止信号
  - `SIGSEGV`：段错误（非法内存访问）
  - `SIGABRT`：异常终止（如 assert 失败）
  - `SIGINT`：中断信号（Ctrl+C）
  - `SIGBUS`：总线错误
- 在信号处理函数 `handle_signal` 中调用 `ql_sys_log_signal()` 尽量记录程序的异常信息，然后退出程序。

**`ql_sys_log_signal` 函数参数说明：**

| 参数 | 说明 |
|------|------|
| `QL_SYS_LOG_ID_MAIN` | 写入 MAIN 缓冲区 |
| `QL_SYS_LOG_FATAL` | 日志等级为 FATAL |
| `LOG_TAG` | 日志 tag |
| `sig_num` | 信号编号 |
| `info` | 信号详细信息（siginfo_t） |
| `ptr` | 上下文指针 |

### 3.2 获取异常调用栈

应用程序发生异常时，可通过详细的应用程序调用栈定位异常代码位置。`ql_sys_log_signal` 可以回溯函数调用栈，但同时也有一定的局限性。

**方法**

删除应用程序编译选项中的优化选项 `-O1`、`-O2`、`-O3` 和 `-fomit-frame-pointer`，并添加编译选项 `-fasynchronous-unwind-tables –rdynamic`。

具体操作：
- 从编译命令中移除：`-O1`、`-O2`、`-O3`、`-fomit-frame-pointer`
- 添加编译选项：`-fasynchronous-unwind-tables -rdynamic`

**局限**

有些库在编译时添加了优化选项。若库函数发生异常，可能无法回溯详细的调用栈。

---

## 4 注意事项

在系统运行过程中，可能会产生大量的日志，以及频繁写入日志文件。若日志文件存放在 flash 中，会缩短 flash 使用寿命（flash 技术参数有最大写入次数限制）。

在研发测试阶段，可将日志保存在 flash 中，以便于调试。但是在后续阶段，建议将日志文件保存到临时文件系统，示例配置如下：

**配置示例（`/data/qllog.json`）：**

```json
{
    /* log output format : log type, time, pid, tid, tag, msg */
    /* log type : k->kernel, e->events, s->system, m->main */
    /* (mandatory) log file path */
    "log_file": "/tmp/qllog/ql_log.txt",

    /* (optional) rotate log every kbytes, default is unlimit */
    "rotate_file_size": 1024,
    /* (optional) max number of rotated logs, default is 4 */
    "rotate_file_count": 2,

    /* (optional) log format, default or csv */
    "log_format": "default",
    ...
}
```

**配置说明：**
- `"log_file": "/tmp/qllog/ql_log.txt"`：将日志文件保存到 `/tmp` 临时文件系统，避免频繁写入 flash
- `/tmp` 目录位于内存（tmpfs），掉电后数据不保留，适用于研发后续的生产阶段

> **重要提示：** 在系统运行中可能会对日志文件频繁写入，不可将日志保存在关键系统分区（如 `/` 根分区或 flash 存储区域），以避免损坏文件系统或缩短 flash 寿命。

---

## 5 附录 A 术语缩写

**表 3：术语缩写**

| 缩写 | 英文全称 | 中文全称 |
|------|----------|----------|
| API | Application Program Interface | 应用程序接口 |
| CPU | Central Processing Unit | 中央处理器 |
| DEBUG | Debug Level | 调试等级 |
| ERROR | Error Level | 错误等级 |
| FATAL | Fatal Level | 致命错误等级 |
| INFO | Information Level | 信息等级 |
| IoT | Internet of Things | 物联网 |
| LTE | Long Term Evolution | 长期演进 |
| PID | Process Identifier | 进程标识符 |
| QuecOpen | Quectel Open Platform | 移远通信开放平台 |
| SDK | Software Development Kit | 软件开发工具包 |
| TID | Thread Identifier | 线程标识符 |
| VERBOSE | Verbose Level | 详细等级 |
| WARN | Warning Level | 警告等级 |

---

*本文档版权属于上海移远通信技术股份有限公司，任何人未经我司允许而复制转载该文档将承担法律责任。*
