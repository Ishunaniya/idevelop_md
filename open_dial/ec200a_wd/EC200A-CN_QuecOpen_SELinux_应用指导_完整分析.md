# EC200A-CN(TA) QuecOpen SELinux 应用指导 — 完整分析文档

> **原文档信息**
> - 文档名称：EC200A-CN(TA) QuecOpen SELinux 应用指导
> - 所属系列：车规级模块系列
> - 版本：1.0.0
> - 日期：2022-08-24
> - 状态：临时文件（Preliminary Document, Not Checked）
> - 作者：Ditto YU（上海移远通信技术股份有限公司）

---

## 目录

- [1. 引言](#1-引言)
- [2. SELinux 简介](#2-selinux-简介)
  - [2.1 SELinux 概述](#21-selinux-概述)
  - [2.2 SELinux 运行条件](#22-selinux-运行条件)
- [3. 系统配置](#3-系统配置)
- [4. 文件标签](#4-文件标签)
  - [4.1 文件标签介绍](#41-文件标签介绍)
  - [4.2 文件标签制作](#42-文件标签制作)
- [5. SELinux 策略](#5-selinux-策略)
  - [5.1 策略构成](#51-策略构成)
    - [5.1.1 客体类别和权限集](#511-客体类别和权限集)
    - [5.1.2 角色与用户](#512-角色与用户)
    - [5.1.3 类型](#513-类型)
  - [5.2 策略编写语法介绍](#52-策略编写语法介绍)
    - [5.2.1 常见访问向量规则关键字](#521-常见访问向量规则关键字)
    - [5.2.2 安全上下文转换](#522-安全上下文转换)
- [6. 策略编写及使用实例](#6-策略编写及使用实例)
  - [6.1 设置 SELinux 配置文件](#61-设置-selinux-配置文件)
  - [6.2 文件标签编写](#62-文件标签编写)
  - [6.3 策略规则文件编写](#63-策略规则文件编写)
    - [6.3.1 类型文件编写](#631-类型文件编写)
    - [6.3.2 TE 规则文件编写](#632-te-规则文件编写)
  - [6.4 编译及使用](#64-编译及使用)
    - [6.4.1 编译文件系统镜像](#641-编译文件系统镜像)
    - [6.4.2 烧录进模块](#642-烧录进模块)
  - [6.5 检查策略是否生效](#65-检查策略是否生效)
- [7. 附录：参考文档及术语缩写](#7-附录参考文档及术语缩写)

---

## 前言与版权声明

### 文档说明

移远通信（上海移远通信技术股份有限公司）提供本文档内容以支持客户的产品设计。客户须按照文档中提供的规范、参数来设计产品。移远通信提供的参考设计仅作为示例，客户应使用独立的分析、评估和判断来设计目标产品。

移远通信可在未事先通知的情况下，自行决定随时增加、修改或重述本文档。

### 联系方式

- **公司**：上海移远通信技术股份有限公司
- **地址**：上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼，邮编：200233
- **电话**：+86 21 5108 6236
- **邮箱**：info@quectel.com
- **销售支持**：http://www.quectel.com/cn/support/sales.htm
- **技术支持**：http://www.quectel.com/cn/support/technical.htm / support@quectel.com

### 使用和披露限制

#### 许可协议

除非移远通信特别授权，否则我司所提供硬件、材料和文档的接收方须对接收的内容保密，不得将其用于除本项目的实施与开展以外的任何其他目的。

#### 版权声明

移远通信产品和本协议项下的第三方产品可能包含受移远通信或第三方材料、硬软件和文档版权保护的相关资料。除非事先得到书面同意，否则不得获取、使用、向第三方披露我司所提供的文档和信息，或对此类受版权保护的资料进行复制、转载、抄袭、出版、展示、翻译、分发、合并、修改，或创造其衍生作品。移远通信或第三方对受版权保护的资料拥有专有权，不授予或转让任何专利、版权、商标或服务商标权的许可。

#### 商标

除另行规定，本文档中的任何内容均不授予在广告、宣传或其他方面使用移远通信或第三方的任何商标、商号及名称，或其缩略语，或其仿冒品的权利。

#### 第三方权利

本文档可能涉及一个或多个属于第三方的硬软件和文档（"第三方材料"）。对此类第三方材料的使用应受本文档的所有限制和义务约束。

### 免责声明

1. 移远通信不承担任何因未能遵守有关操作或设计规范而造成损害的责任。
2. 移远通信不承担因本文档中的任何因不准确、遗漏、或使用本文档中的信息而产生的任何责任。
3. 移远通信尽力确保开发中功能的完整性、准确性、及时性，但不排除上述功能错误或遗漏的可能。除非另有协议规定，否则移远通信对开发中功能的使用不做任何暗示或法定的保证。在适用法律允许的最大范围内，移远通信不对任何因使用开发中功能而遭受的损害承担责任，无论此类损害是否可以预见。
4. 移远通信对第三方网站及第三方资源的信息、内容、广告、商业报价、产品、服务和材料的可访问性、安全性、准确性、可用性、合法性和完整性不承担任何法律责任。

**版权所有 © 上海移远通信技术股份有限公司 2022，保留一切权利。**
**Copyright © Quectel Wireless Solutions Co., Ltd. 2022.**

---

## 文档历史

### 修订记录

| 版本  | 日期       | 作者     | 变更描述 |
|-------|------------|----------|----------|
| -     | 2022-08-24 | Ditto YU | 文档创建 |
| 1.0.0 | 2022-08-24 | Ditto YU | 临时版本 |

---

## 1. 引言

移远通信 EC200A-CN(TA) 模块支持 QuecOpen® 方案；QuecOpen® 是开源的基于 Linux 的嵌入式开发平台，可简化 IoT 应用的软件设计和开发过程。有关 QuecOpen® 的详细信息，请参考文档 **[1]**（`Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导`）。

**本文档主要介绍**在 QuecOpen® 方案下如何运行 SELinux，包含以下内容：

- **系统配置**：SELinux 工作状态的初始化配置方法
- **文件标签制作**：为镜像文件中的每个文件打上安全上下文标签的方法
- **策略编写**：SELinux 策略文件的结构、语法和编写规范
- **运行与验证**：结合实例说明如何运行和验证 SELinux 策略是否生效

---

## 2. SELinux 简介

### 2.1 SELinux 概述

**SELinux**（Security-Enhanced Linux，安全增强型 Linux）是一个 **Linux 内核的安全模块**，其提供了访问控制安全策略机制，包括**强制访问控制（MAC，Mandatory Access Control）**。

核心机制：

- SELinux 为系统中的每个**进程**（主体）与每个**文件**（客体/资源）都打上一个**安全上下文标签（Security Context）**
- 通过该标签实现**主体（进程）对客体（资源文件）的访问控制**
- 所有访问控制决策均由 SELinux 策略（Policy）决定，而非文件属主（区别于传统的 DAC 机制）

### 2.2 SELinux 运行条件

在使用 SELinux 之前，需满足以下三项条件（缺一不可）：

| 条件 | 说明 | 详见 |
|------|------|------|
| **系统配置** | 在 SELinux 配置文件 `/etc/selinux/config` 中必须初始化 SELinux 工作状态 | 第 3 章 |
| **文件标签** | 每个文件都需要有特定的安全上下文标签，SELinux 无法管理没有标签的文件 | 第 4 章 |
| **SELinux 策略** | 在系统 `/etc/selinux/quectel/policy` 目录下必须有 SELinux 的策略文件 | 第 5 章 |

---

## 3. 系统配置

### 配置文件路径

| 环境 | 路径 |
|------|------|
| 系统运行时 | `/etc/selinux/config` |
| SDK 包中 | `/selinux/config` |

### 配置文件内容示例

```ini
# This file controls the state of SELinux on the system.
# SELINUX= can take one of these three values:
# enforcing - SELinux security policy is enforced.
# permissive - SELinux prints warnings instead of enforcing.
# disabled - No SELinux policy is loaded.
SELINUX=permissive

# SELINUXTYPE= can take one of these two values:
# default - equivalent to the old strict and targeted policies
# mls - Multi-Level Security (for military and educational use)
# src - Custom policy built from source
SELINUXTYPE=quectel

# SETLOCALDEFS= C
```

### 配置变量详解

#### 变量 1：`SELINUX`

用于初始化 SELinux 工作状态，支持以下三种工作模式：

| 模式 | 说明 |
|------|------|
| **`enforcing`（强制模式）** | 如果当前执行的操作不满足 SELinux 策略，不满足操作权限的日志会被记录，并且**一切不满足安全策略的系统行为会被拒绝**（实际执行拦截） |
| **`permissive`（宽容模式）** | 如果当前执行的操作不满足 SELinux 策略，不满足操作权限的日志会被记录，但**不满足安全策略的系统行为不会被拒绝**（仅记录不拦截） |
| **`disabled`（关闭）** | 关闭 SELinux 功能，不加载任何策略 |

> **备注（重要）**：`permissive`（宽容模式）**仅供调试使用**。移远通信提供的 `/etc/selinux/config` 文件，默认将 `SELINUX` 设置成 `permissive`，客户可按需修改为其他模式（如生产环境建议改为 `enforcing`）。

#### 变量 2：`SELINUXTYPE`

决定了 SELinux 加载策略时，在哪个目录下寻找策略文件。支持的取值如下：

| 取值 | 说明 |
|------|------|
| **`mls`** | 表示多级访问控制；预留值，**暂不支持配置** |
| **`src`** | 表示系统初始化时 SELinux 加载策略文件的位置。例如，若将其设置为 `quectel`，则表示 SELinux 将在 `/etc/selinux/quectel/policy` 目录下寻找策略文件 |
| **`default`** | 暂不支持 |

> **建议**：将 SELinux 策略置于某个目录之下，方便查询和控制。此处只关注 `src` 即可。在上面提供的配置文件中，`src` 被显式设置为 `"quectel"`，即策略文件路径为 `/etc/selinux/quectel/policy`。

#### 变量 3：`SETLOCALDEFS`

预留变量，**暂不支持**。

---

## 4. 文件标签

### 4.1 文件标签介绍

**SELinux 文件标签**用于设置客体资源的安全上下文。为了使镜像文件支持 SELinux 功能，在镜像文件的制作过程中，需要：

1. 使用**移远通信修改制作的 `setfiles` 工具**给镜像文件打上 SELinux 标签
2. 使用 **`mksquashfs4` 工具**制作出包含 SELinux 标签的文件系统镜像

镜像制作可参考第 **6.4.1** 章。部分脚本命令如下所示：

```bash
# 步骤1：使用 setfiles 工具给镜像中所有文件打上 SELinux 安全上下文标签
setfiles -r /ql-ol-rootfs \
    /ql-ol-rootfs/etc/selinux/quectel/contexts/files/file_contexts \
    /ql-ol-rootfs

# 步骤2：使用 mksquashfs4 制作包含 SELinux 标签的 squashfs 文件系统镜像
mksquashfs4 /ql-ol-rootfs /target/root.squashfs \
    -noappend -root-owned -comp xz \
    -Xpreset 8 -Xe -Xlc 0 -Xlp 2 -Xpb 2 \
    -Xbcj arm -b 64k \
    -p '/dev d 755 0 0' \
    -p '/dev/console c 600 0 0 5 1' \
    -processors 1
```

其中，`file_contexts` 为文件标签配置文件，客户可以手动制作，制作方法请参考第 **4.2** 章。

### 4.2 文件标签制作

`file_contexts` 是**所有 SELinux 文件安全标签的配置文件**；所述 SELinux 文件包括镜像文件与系统动态创建的文件。

- 在 `mkfs.ubifs` 命令行中必须添加 `file_contexts` 的路径
- 当本包烧录进模块后，此文件将位于 `/etc/selinux/quectel/contexts/files` 目录下

#### file_contexts 语法

```
regexp <type> ( <file_label> | <<none>> )
```

**参数说明：**

| 字段 | 说明 |
|------|------|
| `regexp` | 正则表达式，用于匹配文件路径 |
| `<type>` | 文件类型过滤器，取值如下（见下表） |
| `<file_label>` | 安全上下文字符串（格式：`user:role:type:sensitivity`） |
| `<<none>>` | 忽略不设置（该路径匹配的文件不打标签） |

**`<type>` 取值说明：**

| 取值 | 含义 |
|------|------|
| `--` | 只匹配普通文件（regular file） |
| `-d` | 只匹配目录（directory） |
| `-l` | 只匹配符号链接文件（symbolic link） |
| `-c` | 只匹配字符设备文件（character device） |
| `-b` | 只匹配块设备文件（block device） |
| 不指定类型 | 匹配任何类型的文件 |

#### file_contexts 示例

```
/.* u:object_r:rootfs:s0

/bin/   -d   u:object_r:bin:s0
/bin/.* u:object_r:bin_exec:s0
/bin/sh -l   u:object_r:shell_exec:s0
```

**逐行解析：**

| 规则行 | 含义 |
|--------|------|
| `/.* u:object_r:rootfs:s0` | 匹配根目录下所有文件（任意类型），安全上下文设为 `u:object_r:rootfs:s0` |
| `/bin/ -d u:object_r:bin:s0` | 只匹配 `/bin/` 目录本身（`-d`），安全上下文设为 `u:object_r:bin:s0` |
| `/bin/.* u:object_r:bin_exec:s0` | 匹配 `/bin/` 下所有文件（任意类型），安全上下文设为 `u:object_r:bin_exec:s0` |
| `/bin/sh -l u:object_r:shell_exec:s0` | 只匹配 `/bin/sh` 符号链接文件（`-l`），安全上下文设为 `u:object_r:shell_exec:s0` |

#### 匹配规则（逐一匹配原则）

`file_contexts` 将逐一匹配上述举例中的内容；根据正则表达式：

- `/text.txt` 文件匹配 `/.* ` 成功，后续匹配都失败，因此 `/text.txt` 安全上下文会被设置为 `u:object_r:rootfs:s0`
- `/bin/dmesg` 文件匹配 `/.* ` 成功，匹配 `/bin/.*` 也成功，根据**逐一匹配原则，会以最后匹配成功的一行来设置上下文**，因此 `/bin/dmesg` 安全上下文会被设置为 `u:object_r:bin_exec:s0`

> **备注（重要）**：当编写 `file_contexts` 文件时，待设置的安全上下文需与第 **5.1.3** 章所述类型一致。`file_contexts` 设置的安全上下文由策略定义，否则无意义。

---

## 5. SELinux 策略

**SELinux 策略**是 SELinux 在运行时，判断是否允许主体进程对客体资源执行操作时所依据的准则。

SELinux 策略包括：
- 客体类别和权限集（Object Class & Permission Sets）
- 用户（Users）
- 角色（Roles）
- 类型（Types）
- 多级权限（MLS，Multi-Level Security）
- 条件控制（Conditionals）

### SELinux 策略文件框架

```
├── app
│   ├── quectel .............. 该目录下的文件定义移远进程的权限策略
│   └── system ............... 该目录下的文件定义系统进程的权限策略
├── class
│   ├── classes .............. 定义 SELinux 客体类型
│   └── perms ................ 定义 SELinux 客体类型的权限集
├── macros
│   ├── global_macros ........ 全局宏，定义客体类型权限组
│   ├── mls_macros ........... 多级权限宏定义
│   ├── neverallow_macros .... 定义 neverallow 权限宏
│   └── te_macros ............ 策略文件编写需要的宏
├── Makefile ................. 编译策略文件
├── misc
│   ├── fs_use ............... 定义文件系统的扩展属性，如果不定义，文件系统则不能设置安全上下文
│   └── genfs_contexts ....... 设置文件系统中文件的默认安全上下文，设置 proc 文件系统安全上下文
├── mls
│   └── mls .................. 定义 SELinux 多级权限策略
├── roles
│   └── roles ................ 定义 SELinux 角色
├── sid
│   ├── initial_sid_contexts . 初始化缺省安全上下文
│   └── initial_sids ......... 定义缺省安全上下文序列
├── type
│   ├── attributes ........... 定义 SELinux 的类型属性
│   ├── device.te ............ 定义 device 相关的 SELinux 类型
│   └── file_type ............ 定义文件的 SELinux 类型
├── users
│   └── users ................ 定义 SELinux 用户
└── users_extra .............. 定义策略前缀条目
```

### 主体与客体

- **SELinux 控制的主体是进程**，它限制进程对资源的访问权限
- **客体就是资源**，包括文件、设备、网络、信号和进程等
- 每个主体与客体都必须有一个**安全上下文**，该安全上下文决定了主体进程对客体资源是否有访问权限

---

### 5.1 策略构成

#### 5.1.1 客体类别和权限集

**客体**就是主体要访问的内容，在 SDK 路径 `/ql-selinux/class/classes` 中定义。

客体权限集中，**客体不同，权限也不同**：
- 如果客体是**文件**，那么文件有创建、打开、读、写和执行等操作
- 如果客体是 **socket**，那么客体有绑定、监听、接受和连接等操作

移远通信已经完成关键客体权限的定义，可根据需要增加。

##### 客体类别定义语法

```
# 类别定义语法
class class_name

# 参数说明：
# class：类别定义关键字
# class_name：要定义的类别标识符（可以是字母、数字和下划线）

# 示例：
class file
class dir
class chr_file
```

##### 类别权限集语法

```
# 权限集定义语法
common common_name {perm_set}

# 参数说明：
# common：权限集定义关键字
# common_name：权限集标识符
# perm_set：权限集

# 示例：
common file_perm
{
    read
    write
    ioctl
}
```

##### 客体类别与权限集关联语法

```
# 关联语法
class class_name [inherits common_name] [{perm_set}]

# 参数说明：
# class：类别定义关键字（并非定义一个新类别，而是之前已定义的类别）
# class_name：要定义的类别标识符（并非定义新的类别标识符，而是之前已定义的标识符）
# inherits：关键字，代表一个类别继承一个权限集
# common_name：权限集标识符（并非定义新的权限集标识符，而是之前已定义的标识符）
# perm_set：继承的权限集如果不够，还可以添加新的权限

# 示例：
class chr_file
inherits file_perm
{
    entrypoint
    execmod
    open
    audit_access
}
```

---

#### 5.1.2 角色与用户

以 `file_contexts` 的安全上下文 `u:object_r:bin_exec:s0` 为例：

| 字段 | 值 | 含义 |
|------|-----|------|
| `u` | 用户（user） | SELinux 用户标识符 |
| `object_r` | 角色（role） | SELinux 角色标识符 |
| `bin_exec` | 类型（type） | SELinux 类型标识符 |
| `s0` | 多级安全（MLS） | 多级安全敏感度标签 |

> **重要**：模块的 SELinux 中，移远通信已经完成对用户和角色的定义，**无需修改**。

##### SELinux 角色定义语法

```
# 语法形式1：直接定义一个角色
role role_name;

# 语法形式2：定义角色的同时，关联一个类型
role role_name [types type_set];

# 参数说明：
# role：角色定义关键字
# role_name：角色标识符
# types：角色关联类型的关键字
# type_set：关联的一个类型
# 第一种是直接定义一个角色，第二种是定义角色的同时，关联一个类型

# 示例：
role r;
role r types domain;   # 如果前面定义了 r 角色，这里只要关联一个 domain 类型就可以了
```

##### SELinux 用户定义语法

```
# 语法
user user_name roles {role_set};

# 参数说明：
# user：用户定义关键字
# user_name：用户标识符
# roles：与角色关联的关键字
# role_set：关联的角色集

# 示例：
user u roles {r};
```

---

#### 5.1.3 类型

在 SELinux 中**最主要的访问控制是 TE**（即类型强制访问控制，Type Enforcement），需要特别关注类型。

**类型是安全上下文中的第三个成员**，即 `u:object_r:bin_exec:s0` 中的 `bin_exec`。

**类型的作用示例**：在策略中写入 `allow bin_exec data_t:file open read`，则表示 `bin_exec` 类型的进程对 `data_t` 类型的文件有打开和读取操作的权限。

##### 类型定义语法

```
# SELinux 类型定义语法
type type_name [alias alias_set] [, attribute_set];

# 参数说明：
# type：类型定义关键字
# type_name：类型标识符
# alias：定义类型别名的关键字
# alias_set：别名集合
# attribute_set：类型属性集合（属性与类型标识符处在同一命名空间，
#               也就是说属性与类型不能重名）

# 示例：
type bin_exec, file_type, exec_type;
# 定义一个 bin_exec 类型，有 file_type 与 exec_type 两个属性
```

##### SELinux 类型属性定义语法

```
# 属性定义语法
attribute attribute_name;

# 参数说明：
# attribute：属性定义的关键字
# attribute_name：属性标识符
# 注意：属性本质也是类型，可以说它是类型的类型，它类似于一个数组组，
#       这个数组中可有多个类型

# 类型与属性关联语法
typeattribute type_name attribute_name;
# 注意：当我们定义一个新的类型时没有关联某个属性，那么就可以通过 typeattribute 来关联

# 示例：
type user_file_t;
attribute file_type;
type system_file_t, file_type;       # 定义类型时直接关联属性
typeattribute user_file_t file_type; # 定义类型完成后再关联属性
```

##### 属性的作用（等价展开）

属性可以作为多个类型的集合，使一条策略规则等价于多条规则：

```
# 使用属性的策略语句
allow bin_exec file_type:file {open read};

# 这个策略语句等价于下面两个语句的组合：
allow bin_exec user_file_t:file {open read};
allow bin_exec system_file_t:file {open read};
```

---

### 5.2 策略编写语法介绍

#### 5.2.1 常见访问向量规则关键字

策略编写语法中涉及到的常见**访问向量规则（Access Vector Rules）**关键字如下：

| 关键字 | 含义 |
|--------|------|
| **`allow`** | 表示**允许**主体对客体执行的操作（允许且不记录） |
| **`dontaudit`** | 表示**不记录违规**的决策信息，且违规则**不影响运行**（允许操作且不记录） |
| **`auditallow`** | 表示**允许操作并记录**决策信息（允许操作且记录） |
| **`neverallow`** | 表示**不允许**主体对客体执行指定的操作（强制禁止） |

#### 5.2.2 安全上下文转换

##### 背景说明

对于 TE 访问控制来说，Linux 系统中存在很多个进程，为对不同的进程实现不同的策略控制，这些进程需要具有不同的安全上下文。

- 系统**最先启动的用户进程是 `init` 进程**，所以后续启动的进程需要进行安全上下文转换
- 如果不能进行上下文转换，则 SELinux 没有任何作用
- SELinux 是主体进程对客体资源进行访问控制的安全机制；主体进程是随机创建的，例如 `init` 进程可以创建一个 `shell` 进程
- 如果 `shell` 进程与 `init` 进程保持相同的安全上下文，`shell` 进程和 `init` 进程将具有相同的权限，SELinux 的安全功能将失去作用
- 所以 `shell` 进程的安全上下文**必须与 `init` 进程不同**，此时就涉及到安全上下文转换

##### 安全上下文转换触发机制

安全上下文转换在 **`execve` 函数被调用时完成**，即 `execve` 加载可执行文件时，会通过 SELinux 策略检查是否要进行安全上下文转换。

##### 安全上下文转换所需的三个条件

以 `init` 进程（类型为 `init_t`）启动一个 `shell`（可执行文件为 `/bin/sh`，类型为 `shell_exec`），现在希望 `init` 进程启动一个 `shell` 时，`shell` 进程类型为 `shell_t`，那么至少需要满足以下条件：

| 条件 | 说明 |
|------|------|
| **`execute`** | `init` 进程必须对 `/bin/sh` 文件有执行权限 |
| **`entrypoint`** | `shell_t` 类型必须对 `/bin/sh` 文件有 `entrypoint` 权限 |
| **`transition`** | `init_t` 类型进程必须被允许向 `shell_t` 类型进程转换权限 |

##### 安全上下文转换语法

```
# 进程安全上下文转换语法
type_transition source_type temp_type : class target_type

# 参数说明：
# type_transition：类型转换关键字
# source_type：原类型（发起转换的进程类型）
# class：类别定义关键字
# target_type：目标类型（转换后的进程类型）
# temp_type：对于进程转换来说就是文件类型（可执行文件的 SELinux 类型）
```

##### 安全上下文转换完整示例

```
# 1. 声明类型转换规则：init_t 进程执行 shell_exec 类型文件后，新进程类型为 shell_t
type_transition init_t shell_exec:process shell_t;

# 2. 满足第一个 execute 权限条件：
#    init 进程对 /bin/sh（shell_exec 类型文件）有 getattr、open、read、execute 权限
allow init_t shell_exec:file { getattr open read execute };

# 3. 满足第二个 entrypoint 权限条件：
#    shell_t 类型对 /bin/sh 文件（shell_exec 类型）有 entrypoint、open、read、execute、getattr 权限
allow shell_t shell_exec:file { entrypoint open read execute getattr };

# 4. 满足第三个 transition 权限条件：
#    init_t 类型进程被允许向 shell_t 类型进程转换
allow init_t shell_t:process transition;
```

##### 宏简化写法

对于这种语句比较多的策略定义，移远通信已定义了宏操作，可以一步完成：

```
# 使用 domain_auto_trans 宏一步完成上述四条规则
domain_auto_trans(init_t, shell_exec, shell_t)
```

---

## 6. 策略编写及使用实例

本章以 SDK 包 `ql-ol-extsdk-ec200acntar02a03m2g_ocpu.tar.gz` 为例介绍 SELinux 编写规则。

### 实例需求目标

以需要完成如下策略要求的编写为例：

| 序号 | 策略需求 |
|------|---------|
| 1 | 将 `/bin/test_daemon` 设置为 `test_daemon_exec` 类型 |
| 2 | `test_daemon` 对类型为 `data_file_type` 的文件有访问（open）权限，**无读取（read）权限** |
| 3 | `/bin/sh` 对 `test_daemon_exec` 只有**访问（getattr）权限**，**没有执行（execute）权限** |

> **背景说明**：移远通信已经完成 SELinux 策略的框架，因此无需关注用户、角色和多级访问等，仅根据以下章节所述步骤完成上述策略编写即可。

> **备注**：如果编译过程出现错误，说明以上定义的类型可能被使用过，可以自行定义其他类型。

---

### 6.1 设置 SELinux 配置文件

**配置文件路径**：`ql-ol-extsdk-ec200acntar02a03m2g_ocpu/ql-selinux/config`

移远通信仅提供如下默认配置：
- `SELINUX` 默认配置为 `permissive`
- `SELINUXTYPE` 默认配置为 `quectel`

可根据需要参考**第 3 章**自行修改配置值。

---

### 6.2 文件标签编写

**文件标签配置文件路径**：`ql-ol-extsdk-ec200acntar02a03m2g_ocpu/ql-selinux/file_contexts`

在该文件中，增加如下内容：

```
/bin/sh          -l    u:object_r:shell_exec:s0
/bin/test_daemon --    u:object_r:test_daemon_exec:s0
/run/test_file   --    u:object_r:data_file_type:s0
```

**逐行解析：**

| 规则行 | 类型过滤 | 含义 |
|--------|---------|------|
| `/bin/sh -l u:object_r:shell_exec:s0` | `-l`（符号链接） | `/bin/sh` 符号链接文件，安全上下文设为 `shell_exec` 类型 |
| `/bin/test_daemon -- u:object_r:test_daemon_exec:s0` | `--`（普通文件） | `/bin/test_daemon` 普通文件，安全上下文设为 `test_daemon_exec` 类型 |
| `/run/test_file -- u:object_r:data_file_type:s0` | `--`（普通文件） | `/run/test_file` 普通文件，安全上下文设为 `data_file_type` 类型 |

---

### 6.3 策略规则文件编写

#### 6.3.1 类型文件编写

**类型文件路径**：`ql-ol-extsdk-ec200acntar02a03m2g_ocpu/ql-selinux/type/file_type`

在该文件中，增加如下内容：

```
type data_file_type, exec_type, file_type;
```

**说明**：定义一个名为 `data_file_type` 的新类型，同时关联 `exec_type` 和 `file_type` 两个属性。

#### 6.3.2 TE 规则文件编写

**策略文件路径**：`ql-ol-extsdk-ec200acntar02a03m2g_ocpu/ql-selinux/app/`

TE 规则文件编写步骤如下：

##### 步骤 1：创建 test_daemon.te 文件

在 `ql-ol-extsdk-ec200acntar02a03m2g_ocpu/ql-selinux/app/quectel/` 路径下，创建 `test_daemon.te` 文件，内容如下：

```
# 创建 test_daemon 类型，这是 test_daemon 成功运行后的进程类型
type test_daemon, domain, mlstrustedsubject;

# 创建 test_daemon 文件类型为 test_daemon_exec
type test_daemon_exec, exec_type, file_type;

# 初始化 test_daemon 进程域（自动处理 init 到 test_daemon 的上下文转换）
init_daemon_domain(test_daemon)

# 赋予 test_daemon 对 data_file_type 类型文件的 open（访问）权限
# 注意：此处只授予 open 权限，不授予 read 权限，满足需求2
allow test_daemon data_file_type:file{open};
```

**关键点说明：**

| 语句 | 说明 |
|------|------|
| `type test_daemon, domain, mlstrustedsubject;` | 定义 `test_daemon` 进程域类型，关联 `domain` 和 `mlstrustedsubject` 属性 |
| `type test_daemon_exec, exec_type, file_type;` | 定义 `test_daemon_exec` 可执行文件类型，关联 `exec_type` 和 `file_type` 属性 |
| `init_daemon_domain(test_daemon)` | 使用宏自动完成 `init_t → test_daemon` 的上下文转换所需的全部规则 |
| `allow test_daemon data_file_type:file{open};` | 授予 `test_daemon` 对 `data_file_type` 类型文件的 `open` 权限（仅 open，无 read） |

##### 步骤 2：修改 shell.te 文件

在 `ql-ol-extsdk-ec200acntar02a03m2g_ocpu/ql-selinux/app/system/` 目录下，修改 `shell.te` 文件，增加如下内容：

```
# 授予 shell 进程对 test_daemon_exec 类型文件的 getattr（访问/查看属性）权限
# 注意：只授予 getattr 权限，不授予 execute、read 或 execute_no_trans 权限，满足需求3
allow shell test_daemon_exec:file{getattr};
```

**关键点说明**：仅授予 `getattr`（查看文件属性）权限，不授予 `execute`、`read` 或 `execute_no_trans` 权限，确保 shell 进程无法执行 `test_daemon`。

---

### 6.4 编译及使用

#### 6.4.1 编译文件系统镜像

在 SDK **根目录**下，使用 **`make rootfs`** 可直接编译出含有 SELinux 策略的文件系统镜像：

```bash
cd <SDK根目录>
make rootfs
```

生成位置为 `ql-ol-extsdk-ec200acntar02a03m2g_ocpu/target` 目录下。

编译过程中，构建系统会自动完成：
1. 使用 `setfiles` 工具给镜像文件打上 SELinux 安全上下文标签
2. 使用 `mksquashfs4` 工具制作含 SELinux 标签的 squashfs 文件系统镜像（`root.squashfs`）

#### 6.4.2 烧录进模块

1. 将 `ql-ol-extsdk-ec200acntar02a03m2g_ocpu/target` 目录下编译生成的 `root.squashfs` 文件**拷贝到固件版本包中**
2. 然后通过**固件升级工具**将固件烧录进模块中

> 固件升级工具的使用方法可参考**文档 [1]**（`Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导`）。

---

### 6.5 检查策略是否生效

系统开机后，通过如下步骤检查策略是否生效：

#### 步骤 1：检查文件安全上下文标签

在系统的 `/bin` 目录下，执行 `ls -laZ` 查看 `test_daemon` 上下文类型是否为 `test_daemon_exec` 类型：

```bash
ls -laZ /bin/test_daemon
```

**预期输出（策略生效时）：**

```
-rwxr-xr-x 1 xxx xxx u:object_r:test_daemon_exec:s0 xxx xx xx xxxx test_daemon
```

**验证要点**：`u:object_r:test_daemon_exec:s0` 中的类型字段应为 `test_daemon_exec`，说明文件标签已正确打上。

#### 步骤 2：验证 test_daemon 对 data_file_type 文件无读取权限

通过 `dmesg|grep avc` 查看 AVC 日志，确认 `test_daemon` 对 `data_file_type` 类型的文件是否无读取权限：

```bash
dmesg | grep avc
```

**预期 AVC 日志（策略生效时）：**

```
[ 89.057338] type=1400 audit(315964856.019:6): avc: denied { read } for
    pid=1697 comm="sh" name="87" dev="proc" ino=7819
    scontext=u:r:test_daemon:s0
    tcontext=u:object_r:data_file_type:s0
    tclass=file permissive=1
```

**AVC 日志字段解析：**

| 字段 | 值（示例） | 含义 |
|------|-----------|------|
| `avc: denied` | — | 表示访问被拒绝（permissive 模式下记录但不实际拦截） |
| `{ read }` | `{ read }` | 被拒绝的操作是 `read`（读取） |
| `pid` | `1697` | 发起请求的进程 PID |
| `comm` | `"sh"` | 发起请求的进程名 |
| `scontext` | `u:r:test_daemon:s0` | **主体（subject）进程的安全上下文** |
| `tcontext` | `u:object_r:data_file_type:s0` | **客体（target/object）资源的安全上下文** |
| `tclass` | `file` | 客体类别（文件） |
| `permissive` | `1` | 宽容模式（1=permissive，0=enforcing） |

**结论**：从 AVC log 可以得出，`test_daemon` 类型的进程（由 `scontext` 决定）对 `data_file_type` 类型（由 `tcontext` 决定）的文件（由 `tclass` 决定）缺少 `read`（由 `denied` 决定）权限。

> **扩展**：此时若有需求变更，例如需要 `test_daemon` 对 `data_file_type` 类型的文件具有读取权限，那么需要增加策略：
> ```
> allow test_daemon data_file_type:file read;
> ```

#### 步骤 3：验证 shell 对 test_daemon_exec 无执行权限

执行 `./test_daemon` 后，通过 `dmesg|grep avc` 查看 AVC 日志，确认 `/bin/sh` 对 `test_daemon_exec` 是否只有访问权限，没有执行权限：

```bash
./test_daemon
dmesg | grep avc
```

**预期 AVC 日志（策略生效时）：**

```
[ 391.144781] audit: type=1400 audit(1603883212.531:198): avc:
    denied { execute } for pid=2602 comm="sh" name="test_daemon"
    dev="ubiblock0_0" ino=113
    scontext=u:r:shell:s0
    tcontext=u:object_r:test_daemon_exec:s0
    tclass=file permissive=1

[ 391.148476] audit: type=1400 audit(1603883212.531:199): avc:
    denied { read } for pid=2602 comm="sh" path="/bin/test_daemon"
    dev="ubiblock0_0" ino=113
    scontext=u:r:shell:s0
    tcontext=u:object_r:test_daemon_exec:s0
    tclass=file permissive=1

[ 391.200866] audit: type=1400 audit(1603883212.561:200): avc:
    denied { execute_no_trans } for pid=2602 comm="sh"
    path="/bin/test_daemon" dev="ubiblock0_0" ino=113
    scontext=u:r:shell:s0
    tcontext=u:object_r:test_daemon_exec:s0
    tclass=file permissive=1
```

**日志解析：**

上面三条 AVC log 表明：

| 被拒绝的权限 | 说明 |
|------------|------|
| `{ execute }` | shell 进程试图**执行** test_daemon 文件 → 被拒绝 |
| `{ read }` | shell 进程试图**读取** test_daemon 文件内容 → 被拒绝 |
| `{ execute_no_trans }` | shell 进程试图在**不发生上下文转换的情况下执行** test_daemon → 被拒绝 |

**综合结论**：AVC log 表明 shell 进程对 `test_daemon_exec` 有访问权限，但是**没有 `execute`、`read`、`execute_no_trans` 权限**，表明策略已**成功生效**。

---

## 7. 附录：参考文档及术语缩写

### 表 1：参考文档

| 编号 | 文档名称 |
|------|---------|
| [1]  | Quectel_EC200A-CN(TA)_QuecOpen_快速开发指导 |

### 表 2：术语缩写

| 缩写 | 英文全称 | 中文全称 |
|------|---------|---------|
| IoT | Internet of Things | 物联网 |
| SDK | Software Development Kit | 软件开发工具包 |
| SELinux | Security-Enhanced Linux | 安全增强型 Linux |
| TE | Type Enforcement | 类型强制 |

---

## 附：核心知识点总结

### SELinux 三要素

```
系统配置 (/etc/selinux/config)
    ├── SELINUX=enforcing/permissive/disabled
    └── SELINUXTYPE=quectel（策略目录名）

文件标签 (file_contexts)
    └── regexp <type> <user:role:type:sensitivity>

SELinux 策略 (/etc/selinux/quectel/policy/)
    ├── 客体类别 (class/classes)
    ├── 权限集 (class/perms)
    ├── 用户/角色 (users/roles) ← 移远已定义，无需修改
    └── 类型 + TE 规则 (type/*.te, app/**/*.te)
```

### 安全上下文格式

```
u : object_r : bin_exec : s0
│       │         │         │
用户   角色      类型    MLS敏感度
```

### 完整开发流程

```
1. 修改 ql-selinux/config          → 设置 SELinux 工作模式
2. 修改 ql-selinux/file_contexts   → 为自定义文件打标签
3. 修改 ql-selinux/type/file_type  → 定义新的文件类型
4. 创建 ql-selinux/app/quectel/xxx.te → 编写进程域 TE 规则
5. 修改 ql-selinux/app/system/*.te → 调整系统进程权限
6. make rootfs                      → 编译含 SELinux 策略的镜像
7. 烧录 root.squashfs 进模块        → 使用固件升级工具
8. 开机后 ls -laZ / dmesg|grep avc → 验证策略是否生效
```

### AVC 日志读取方法

```
avc: denied { <权限> } for
    pid=<进程ID> comm=<进程名>
    scontext=<主体安全上下文>   ← 发起访问的进程
    tcontext=<客体安全上下文>   ← 被访问的资源
    tclass=<客体类别>           ← 资源类型（file/dir/socket...）
    permissive=<0|1>           ← 0=enforcing拦截, 1=permissive仅记录

→ 解读：scontext 的进程 对 tcontext 的 tclass 类型资源
        缺少 { 权限 } 操作权限

→ 修复：add "allow <scontext类型> <tcontext类型>:<tclass> { <权限> };"
```

### 策略冷启动检查清单

| 检查项 | 命令 | 预期结果 |
|--------|------|---------|
| SELinux 工作状态 | `cat /etc/selinux/config` | `SELINUX=permissive/enforcing` |
| 策略目录是否存在 | `ls /etc/selinux/quectel/policy/` | 有策略二进制文件 |
| 文件标签 | `ls -laZ /bin/<程序名>` | 显示正确的安全上下文 |
| AVC 日志 | `dmesg \| grep avc` | 显示预期的 deny 记录 |

---

*本文档基于 Quectel EC200A-CN(TA) QuecOpen SELinux 应用指导 V1.0.0（2022-08-24）全文整理，涵盖原文 21 个物理页面的全部内容，无遗漏。*
