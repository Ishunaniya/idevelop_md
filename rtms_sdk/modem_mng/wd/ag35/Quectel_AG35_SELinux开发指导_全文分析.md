# Quectel AG35-CET&AG35-EUT QuecOpen(SDK) SELinux 开发指导 — 全文分析

> **文档全名**：AG35-CET&AG35-EUT QuecOpen(SDK) SELinux 开发指导
> **适用模块**：LTE Standard 模块系列（AG35-CET / AG35-EUT）
> **版本**：V1.0（受控文件）
> **发布日期**：2024-12-10
> **作者**：Silas GAO
> **文档物理页数**：21 页（含封面；正文页脚编号 1/20 ~ 20/20）
> **本分析覆盖范围**：全部 21 页（封面、前言/声明、文档历史、目录、表格索引、第 1~7 章、附录）
> **分析定位**：本文档面向 QuecOpen(SDK) 方案的客户，讲解如何在 AG35 模块上**启用、配置、编写与验证 SELinux 策略**，包含系统配置、文件标签制作、策略框架与语法、完整实操示例、生效验证五大块。

---

## 文档元信息（封面 / 前言 / 声明，页 1~2）

- **厂商**：上海移远通信技术股份有限公司（Quectel Wireless Solutions Co., Ltd.）。
- **联系方式**：上海市闵行区田林路 1016 号科技绿洲 3 期（B 区）5 号楼，邮编 200233；电话 +86 21 5108 6236；邮箱 info@quectel.com；技术支持 support@quectel.com。
- **声明要点**：参考设计仅作示例（"可用"基础上提供）；含许可协议、版权声明、商标、第三方权利、隐私声明、免责声明等标准条款。版权所有 © 上海移远通信技术股份有限公司 2024。
- **隐私声明**：为实现产品功能，特定设备数据会上传至移远或第三方服务器（含运营商、芯片供应商）。

### 文档历史（修订记录，页 3）

| 版本 | 日期 | 作者 | 变更表述 |
|---|---|---|---|
| -（草稿） | 2024-01-19 | Silas GAO | 文档创建 |
| 1.0 | 2024-12-10 | Silas GAO | 受控版本 |

### 文档结构（目录 + 表格索引，页 4~5）

正文章节：1 引言；2 SELinux 简介（2.1 概述 / 2.2 运行条件）；3 系统配置；4 文件标签（4.1 介绍 / 4.2 制作）；5 SELinux 策略（5.1 策略构成：5.1.1 客体类别和权限集 / 5.1.2 角色与用户 / 5.1.3 类型；5.2 策略编写语法：5.2.1 常见访问向量规则关键字 / 5.2.2 安全上下文转换）；6 策略编写及使用实例（6.1 设置配置文件 / 6.2 文件标签编写 / 6.3 策略规则文件编写：6.3.1 类型文件 / 6.3.2 TE 规则文件 / 6.4 编译及使用：6.4.1 编译镜像 / 6.4.2 烧录 / 6.5 检查策略是否生效）；7 附录（参考文档 + 术语缩写）。

表格索引：表 1 参考文档（页 20）、表 2 术语缩写（页 20）。

---

## 1 引言（页 6）

- AG35-CET 和 AG35-EUT 模块支持 **QuecOpen®** 方案。QuecOpen 是基于 **Linux 系统**的嵌入式开发平台，用于简化 **IoV（车联网）** 应用的软件设计与开发。QuecOpen 详细信息参考 **文档 [1]**（即《Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导》）。
- 本文档适用于 **SDK 构建环境**的 QuecOpen 方案，介绍如何运行 SELinux：包含**系统配置、文件标签制作、策略编写**等内容，并结合实例说明如何运行与验证 SELinux。

---

## 2 SELinux 简介（页 7）

### 2.1 SELinux 概述

- **SELinux（Security-Enhanced Linux）** = 安全增强型 Linux，是一个 **Linux 内核的安全模块**，提供**访问控制安全策略机制**，包括**强制访问控制（MAC）**。
- 核心机制：SELinux 为**每个进程与文件**都打上一个**安全上下文标签**，通过该标签实现**主体（进程）对客体（资源文件）**的访问控制。

> **解读**：这是 MAC（Mandatory Access Control）的核心思想——访问决策不取决于文件属主（DAC 的 rwx），而取决于策略对"主体类型 → 客体类型 → 操作"三元组的显式授权。即使是 root 进程，若策略未授权也会被拒绝。

### 2.2 SELinux 运行条件

使用 SELinux 前必须同时满足三项条件：

| 条件 | 要求 | 详见 |
|---|---|---|
| **系统配置** | 在 SELinux 配置文件 `/etc/selinux/config` 中必须初始化 SELinux 工作状态 | 第 3 章 |
| **文件标签** | 每个文件都需要有特定的安全上下文标签；**SELinux 无法管理没有标签的文件** | 第 4 章 |
| **SELinux 策略** | 系统 `/etc/selinux/quectel/policy/` 目录下必须有 SELinux 的策略文件 | 第 5 章 |

> **解读**：三者缺一不可——配置决定"开不开/什么模式"，文件标签决定"客体是谁"，策略决定"谁能对谁做什么"。没标签的文件 SELinux 管不了，是一个常见坑点。

---

## 3 系统配置（页 8~9）

### 配置文件位置

- 运行系统中路径：`/etc/selinux/config`
- SDK 中路径：`/ql_selinux/config`

### 配置文件内容（原文逐行）

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

### 三个环境变量含义

**1. `SELINUX`** — 初始化 SELinux 工作状态，支持三种工作模式：

| 取值 | 模式 | 行为 |
|---|---|---|
| `enforcing` | 强制模式 | 当前操作不满足策略时，**记录日志**，并且**一切不满足安全策略的系统行为会被拒绝** |
| `permissive` | 宽容模式 | 当前操作不满足策略时，**记录日志**，但**不满足安全策略的系统行为不会被拒绝**（仅告警/审计） |
| `disabled` | 禁用 | 禁用 SELinux 功能 |

**2. `SELINUXTYPE`** — 决定 SELinux 加载策略时**在哪个目录寻找策略文件**。支持取值：

| 取值 | 含义 |
|---|---|
| `mls` | 多级别访问控制；**预留值，暂不支持配置** |
| `src` | 系统初始化时 SELinux 加载策略的位置；例如设为 `quectel` 表示 SELinux 在 `/etc/selinux/quectel/policy/` 目录下寻找策略文件 |
| `default` | **暂不支持** |

> 建议将 SELinux 策略置于某个目录之下，方便查询和控制。此处只需关注 `src` 即可——在上面的配置文件中，`src` 被显式设置为 `quectel`（即 `SELINUXTYPE=quectel`）。

> **解读（原文措辞的细节）**：注意配置注释里 `SELINUXTYPE` 标的是 `default/mls/src` 三选项，但实际值写成了 `quectel`。文档的解释是：`SELINUXTYPE` 的值就是策略所在子目录名，写 `quectel` 即把策略目录定位到 `/etc/selinux/quectel/policy/`。换言之 `src` 这一行的"自定义 source 策略"语义，被移远落地为"用 quectel 这个自定义策略目录名"。

**3. `SETLOCALDEFS`** — 预留值，**暂不支持配置**（配置文件中被注释掉 `# SETLOCALDEFS= C`）。

### 备注（重要默认值）

> `permissive`（宽容模式）**仅供调试使用**。移远提供的 `/etc/selinux/config` 文件，**默认将 `SELINUX` 设置成 `permissive`**，客户可按需修改为其他模式。

> **解读**：出厂默认是 permissive（只审计不拦截），便于开发期通过 `dmesg | grep avc` 收集 denied 日志来补策略；量产时若要真正强制，需手动改为 `enforcing`。本文第 6.5 节验证步骤里 avc 日志末尾的 `permissive=1` 正是这一默认状态的体现。

---

## 4 文件标签（页 10~11）

### 4.1 文件标签介绍

- **作用**：文件标签用于设置**客体资源的安全上下文**。
- **目的**：为使镜像文件支持 SELinux 功能，在镜像制作过程中需：
  1. 使用移远修改制作的 **`setfiles` 工具**给镜像文件打上 SELinux 标签；
  2. 使用 **`mksquashfs4` 工具**制作出包含 SELinux 标签的文件系统镜像。

镜像制作可参考第 6.4.1 章，部分脚本命令如下（原文）：

```bash
setfiles -r /ql-ol-rootfs /ql-ol-rootfs/etc/selinux/quectel/contexts/files/file_contexts /ql-ol-rootfs
mksquashfs4 /ql-ol-rootfs /target/root.squashfs -noappend -root-owned -comp xz -Xpreset 8 -Xe -Xlc 0 -Xlp 2 -Xpb 2 -Xbcj arm -b 64k -p '/dev d 755 0 0' -p '/dev/console c 600 0 0 5 1' -processors 1
```

**命令逐项解读：**
- `setfiles -r /ql-ol-rootfs <file_contexts路径> /ql-ol-rootfs`：以 `/ql-ol-rootfs` 为根（`-r`），按 `file_contexts` 规则递归给 rootfs 内文件打安全上下文标签。
- `mksquashfs4` 参数：
  - `/ql-ol-rootfs /target/root.squashfs`：源目录 → 目标镜像。
  - `-noappend`：不追加，重新生成。
  - `-root-owned`：所有文件归属 root。
  - `-comp xz -Xpreset 8 -Xe -Xlc 0 -Xlp 2 -Xpb 2`：xz 压缩及其参数（preset 8、启用 extreme、LZMA 字典参数）。
  - `-Xbcj arm`：对 ARM 可执行码做 BCJ 过滤以提高压缩率。
  - `-b 64k`：块大小 64 KB。
  - `-p '/dev d 755 0 0'`：伪文件——创建 `/dev` 目录（权限 755，uid/gid 0）。
  - `-p '/dev/console c 600 0 0 5 1'`：伪文件——创建字符设备 `/dev/console`（权限 600，主/次设备号 5/1）。
  - `-processors 1`：单线程（保证可复现/确定性）。

- `file_contexts` 为**文件标签配置文件**，客户可手动制作，制作方法见第 4.2 章。

### 4.2 文件标签制作

- `file_contexts` 是所有 SELinux **文件安全标签的配置文件**；这里的 SELinux 文件包括**镜像文件**与**系统动态创建的文件**。命令行中必须添加 `file_contexts` 的路径。
- 当版本包烧录进模块后，此文件位于 `/etc/selinux/quectel/contexts/files/` 目录下。

#### `file_contexts` 语法（原文）

```
regexp <-type> ( <file_label> | <<none>> )
regexp：正则表达式
<-type>：类型，取值如下：
    - -：只匹配普通文件
    -d：只匹配目录
    -l：只匹配符号链接文件
    -c：只匹配字符设备文件
    -b：只匹配块设备文件
    不指定类型：匹配任何文件
<file_label>：安全上下文字符串
<<none>>：忽略不设置
```

**`-type` 取值表：**

| 类型标志 | 匹配对象 |
|---|---|
| `--` | 只匹配普通文件 |
| `-d` | 只匹配目录 |
| `-l` | 只匹配符号链接文件 |
| `-c` | 只匹配字符设备文件 |
| `-b` | 只匹配块设备文件 |
| （不指定类型） | 匹配任何文件 |

**最后一列：**`<file_label>` 是安全上下文字符串；`<<none>>` 表示忽略不设置。

#### 示例（原文）

```
/.*          u:object_r:rootfs:s0
/bin/ -d     u:object_r:bin:s0
/bin/.*      u:object_r:bin_exec:s0
/bin/sh -l   u:object_r:shell_exec:s0
```

#### 匹配规则（关键：逐行匹配，最后匹配成功者生效）

`file_contexts` 将逐一匹配上述内容，根据正则表达式：

- 例：`/text.txt` 文件 → 匹配 `/.*` 成功，后续匹配失败，因此 `/text.txt` 安全上下文被设置为 **`u:object_r:rootfs:s0`**。
- 例：`/bin/dmesg` 文件 → 匹配 `/.*` 成功，匹配 `/bin/.*` 也成功；根据**逐一匹配原则，以最后匹配成功的一行来设置上下文**，因此 `/bin/dmesg` 安全上下文被设置为 **`u:object_r:bin_exec:s0`**。

> **解读**：这是关键的"最后匹配优先"语义（与许多正则规则文件"最长/最后匹配胜出"一致）。规则书写顺序很重要——越具体的规则要放在越靠后；`/bin/sh` 因为有更靠后的 `-l` 专属行，会被标为 `shell_exec` 而非 `bin_exec`。

#### 备注（约束）

> 当编写 `file_contexts` 文件时，待设置的安全上下文需与第 5.1.3 章所述**类型**一致。`file_contexts` 设置的安全上下文须**策略定义**，否则无意义。

> **解读**：file_contexts 里写的 `<file_label>`（如 `bin_exec`、`shell_exec`）必须是策略 type 文件中已 `type` 定义过的类型，否则标签虽打上但无策略引用 = 无效。

---

## 5 SELinux 策略（页 12~16）

### 概述

- **SELinux 策略**是 SELinux 运行时，判断是否允许**主体进程**对**客体资源**执行操作时所依据的准则。
- 策略包括：**客体类别和权限集、用户、角色、类型、多级权限、条件控制**等。
- **主体 = 进程**（限制其对资源的访问权限）；**客体 = 资源**（文件、设备、网络、信号和进程等）。每个主体与客体都必须有一个安全上下文，安全上下文决定了主体进程对客体资源是否有访问权限。

### SELinux 策略文件框架（原文目录树，逐项注释）

```
├── app
│   ├── quectel ........................ 该目录下的文件定义移远进程的权限策略
│   └── system ......................... 该目录下的文件定义系统进程的权限策略
├── class
│   ├── classes ........................ 定义 SELinux 客体类别
│   └── perms .......................... 定义 SELinux 客体类别的权限集
├── macros
│   ├── global_macros .................. 全局宏，定义客体类型权限组
│   ├── mls_macros ..................... 多级权限宏定义
│   ├── neverallow_macros .............. 定义 neverallow 权限宏
│   └── te_macros ...................... 策略文件编写需要的宏
├── Makefile ........................... 编译策略文件
├── misc
│   ├── fs_use ......................... 定义文件系统的扩展属性，如果不定义，文件系统则不能设置安全上下文
│   └── genfs_contexts ................. 设置文件系统中文件的默认安全上下文，设置 proc 文件系统安全上下文
├── mls
│   └── mls ............................ 定义 SELinux 多级权限策略
├── roles
│   └── roles .......................... 定义 SELinux 角色
├── sid
│   ├── initial_sid_contexts ........... 初始化缺省安全上下文
│   └── initial_sids ................... 定义缺省安全上下文序列
├── type
│   ├── attributes ..................... 定义 SELinux 的类型属性
│   ├── device.te ...................... 定义设备相关的 SELinux 类型
│   └── file_type ...................... 定义文件的 SELinux 类型
├── users
│   └── users .......................... 定义 SELinux 用户
└── users_extra ........................ 定义策略前缀条目
```

> **解读**：开发者主要改动点集中在 `app/quectel/`（自定义进程域规则）、`app/system/`（系统进程如 shell 的规则）、`type/file_type`（自定义文件类型）、`contexts/files/file_contexts`（打标签）。其余 class/macros/mls/roles/sid/users 由移远预置，通常无需改动。

### 5.1 策略构成

#### 5.1.1 客体类别和权限集（页 13）

- **客体**就是主体要访问的内容，在 SDK 的 `/ql-selinux/class/classes` 中定义。
- 客体权限集：客体不同，权限也不同。例如客体是文件，则有创建、打开、读、写和执行等操作；如果客体是 socket，则有绑定、监听、接受和连接等操作。
- 移远已经完成关键客体权限的定义，可根据需求增加。

**客体类别定义语法（原文）：**

```
#类别定义语法
class class_name
class：类别定义关键字
class_name：要定义的类别标识符（可以是字母、数字和下划线）
示例：
class file
class dir
class chr_file

#类别权限集语法
common common_name {perm_set}
common：权限集定义关键字
common_name：权限集标识符
perm_set：权限集
示例：
common file_perm
{
read
write
ioctl
}

#客体类别与权限集相关联
class class_name [inherits common_name] [{perm_set}]
class：类别定义关键字（并非定义一个新类别，而是之前已定义的类别）
class_name：要定义的类别标识符（并非定义新的类别标识符，而是之前已定义的标识符）
inherits：关键字，代表一个类别继承一个权限集
common_name：权限集标识符（并非定义新的权限集标识符，而是之前已定义的标识符）
perm_set：继承的权限集如果不够，还可以添加新的权限
示例：
class chr_file
inherits file_perm
{
execute_no_trans
entrypoint
execmod
open
audit_access
}
```

> **解读**：
> - `class` 声明客体类别（file/dir/chr_file/socket…）。
> - `common` 定义一组可复用的权限集（如 file_perm 含 read/write/ioctl…）。
> - `class X inherits common_name { 额外权限 }`：让具体类别继承公共权限集，并补充该类别特有权限（如字符设备 chr_file 在 file_perm 基础上补 `execute_no_trans/entrypoint/execmod/open/audit_access`）。

#### 5.1.2 角色与用户（页 14）

以 `file_contexts` 的安全上下文 **`u:object_r:bin_exec:s0`** 为例，四段含义：

| 字段 | 示例值 | 含义 |
|---|---|---|
| 用户 | `u` | 表示用户 |
| 角色 | `object_r` | 表示角色 |
| 类型 | `bin_exec` | 表示类型 |
| 多级安全 | `s0` | 表示多级安全（MLS 级别） |

模块的 SELinux 中，移远已经完成对用户和角色的定义，**无需修改**。语法如下（原文）：

```
#SELinux 角色定义
1.  role role_name;
或
2.  role role_name [types type_set];
role：角色定义关键字
role_name：角色标识符
types：角色关联类型的关键字
type_set：关联的一个类型
#第一种是直接定义一个角色，第二种是定义角色的同时，关联一个类型
示例：
role r;
role r types domain;   #如果前面定义了 r 角色，这里只需关联一个 domain 类型

#SELinux 用户定义
user user_name roles {role_set};
user：用户定义关键字
user_name：用户标识符
roles：与角色关联的关键字
role_set：关联的角色集
示例：
user u roles {r};
```

> **解读**：安全上下文是四元组 `user:role:type:level`。对 TE（类型强制）而言真正起作用的是第三段 **type**；user/role/level 在嵌入式单一策略里基本固定（`u` / `object_r`(客体) 或 `r`(主体) / `s0`），移远已预置，开发者一般不动。

#### 5.1.3 类型（页 14~15）

- 在 SELinux 中**最主要的访问控制是 TE（类型强制访问控制，Type Enforcement）**，需要特别关注**类型**。
- 类型是安全上下文中的**第三个成员**，即 `u:object_r:bin_exec:s0` 中的 `bin_exec`。
- 例：策略中写 `allow bin_exec data_t:file open read`，则表示 **`bin_exec` 类型的进程**对 **`data_t` 类型的文件**有**打开和读取**操作的权限。

**类型语法（原文）：**

```
#SELinux 类型定义语法
type type_name [ alias alias_set ] [, attribute_set ] ;
type：类型定义关键字
type_name：类型标识符
alias：定义类型别名的关键字
alias_set：别名集合
attribute_set：类型属性集合（属性与类型标识符处在同一命名空间，即属性与类型不能重名）
示例：
type bin_exec,file_type,exec_type;   #定义一个 bin_exec 类型，有 file_type 与 exec_type 两个属性

#SELinux 类型属性定义
attribute attribute_name;
attribute：属性定义的关键字
attribute_name：属性标识符
#注意：属性本质也是类型，可以说它是类型的类型，它类似于一个数组，这个数组中有多个类型。

#类型与属性关联语法
typeattribute type_name attritbute_name;
#注意：当我们定义一个新的类型时没有关联某个属性，那么就可以通过 typeattribute 来关联
示例：
type user_file_t;
attribute file_type;
type system_file_t,file_type;        #定义类型时直接关联属性
typeattribute user_file_t file_type; #定义类型完成后关联属性

#属性的作用：
allow bin_exec file_type:file {open read};
#这个策略语句可以说明属性的作用，这个语句等价于下面两个语句的组合
allow bin_exec user_file_t:file {open read};
allow bin_exec system_file_t:file {open read};
```

> **解读**：
> - **type** 是 TE 的核心标识。`type bin_exec, file_type, exec_type;` 一行同时声明类型 `bin_exec` 并把它归入 `file_type` 和 `exec_type` 两个属性。
> - **attribute** 是"类型的类型"（类型分组），相当于一个类型集合。
> - **属性的威力**：`allow bin_exec file_type:file {open read}` 一句等价于对 `file_type` 属性下所有具体类型（user_file_t、system_file_t…）分别 allow——用属性可批量授权、避免逐类型重复。

### 5.2 策略编写语法介绍

#### 5.2.1 常见访问向量规则关键字（页 15）

| 关键字 | 含义 |
|---|---|
| `allow` | 表示**允许**主体对客体执行的操作 |
| `dontaudit` | 表示**不记录**违反规则的决策信息，且违反规则**不影响运行**（允许操作且不记录） |
| `auditallow` | 表示**允许操作并记录**访问决策信息（允许操作且记录） |
| `neverallow` | 表示**不允许**主体对客体执行指定的操作 |

> **解读**：
> - `allow`：放行并（默认）不刷 denied 日志。
> - `dontaudit`：仍是放行结果上的"静默"——抑制本应产生的 denied 审计噪声（用于已知无害的拒绝）。注意原文表述"违反规则不影响运行"指的是抑制审计，不改变 allow/deny 实质；它主要用来屏蔽日志噪声。
> - `auditallow`：放行 + 强制记一条审计（用于审计敏感操作）。
> - `neverallow`：**编译期**断言——若其他规则违反它，策略编译会报错（不是运行期拦截），用于守住安全底线。

#### 5.2.2 安全上下文转换（域转换，页 15~16）

**背景**：
- SELinux 是主体进程对客体资源进行访问控制的安全机制。对 TE 访问控制来说，Linux 中存在很多进程，为对不同进程分别实现策略控制，这些进程需具备不同的安全上下文。
- 系统最先启动的用户进程是 **`init` 进程**，所以对后续启动的进程需要进行**安全上下文转换**。
- 主体进程是随机创建的，例如 `init` 进程可以创建一个 `shell` 进程；如果 `shell` 进程与 `init` 进程保持相同的安全上下文，二者将具有相同的权限，SELinux 的安全功能将失去作用。所以 `shell` 进程的安全上下文必须与 `init` 进程不同，此时就涉及到安全上下文转换。

**转换时机**：
- 安全上下文转换在 **`execve` 函数簇**调用时完成，即 `execve` 加载可执行文件时，会通过 SELinux 策略检查是否要进行安全上下文转换，如果需要则转换。

**示例场景**：`init` 进程类型为 `init_t`，`shell` 的可执行文件为 `/bin/sh`，类型为 `shell_exec`，现希望 `init` 进程启动一个 `shell` 时，`shell` 进程类型为 `shell_t`。则至少需要满足以下三个条件：

| 条件 | 权限 | 含义 |
|---|---|---|
| `execute` | init 进程必须对 `/bin/sh` 文件有**执行**权限 | 源域能执行入口文件 |
| `entrypoint` | `shell_t` 类型必须对 `/bin/sh` 文件有 **entrypoint** 权限 | 该文件是目标域的合法入口 |
| `transition` | `init_t` 类型进程必须被允许向 `shell_t` 类型进程**转换**权限 | 允许域跃迁 |

**安全上下文转换语法（原文）：**

```
#进程安全上下文转换语法
type_transition source_type temp_type : class target_type
type_transition：类型转换关键字
source_type：原类型
class：类别定义关键字
target_type：目标类型
temp_type：对于进程转换来说，是文件类型
示例：
type_transition init_t shell_exec:process shell_t;
allow init_t shell_exec:file { getattr open read execute };   #满足第一个 execute 权限条件
allow shell_t shell_exec:file { entrypoint open read execute getattr }; #满足第二个 entrypoint 权限
allow init_t shell_t:process transition;   #满足第三个 transition 权限
注意：对于这种语句比较多的策略定义，移远已定义了宏操作，可以一步完成，例如：
domain_auto_trans(init_t,shell_exec,shell_t)
```

> **解读**：
> - 域转换（domain transition）= 进程通过 execve 执行某个入口可执行文件后，自动切换到新的 type（域）。三要素：源域可 execute 入口文件、目标域对入口文件有 entrypoint、源域可 transition 到目标域。
> - `type_transition init_t shell_exec:process shell_t;` 声明"init_t 执行 shell_exec 标记的文件时，进程应转到 shell_t"，但仅声明还不够，还需上面三条 `allow` 同时满足。
> - **宏 `domain_auto_trans(init_t, shell_exec, shell_t)`** 把这四行（type_transition + 三条 allow）一次写完，是推荐写法。第 6 章实例里 `init_daemon_domain(...)` 同理是封装宏。

---

## 6 策略编写及使用实例（页 17~19）

### 实例目标

本章以 SDK `ql-ol-extsdk-ag35cetcar01a03m2g_ocpu.tar.gz` 为例介绍 SELinux 编写规则。要完成的策略需求：

1. 将 `/bin/test_daemon` 设置为 **`test_daemon_exec`** 类型；
2. `test_daemon` 对类型为 `data_file_type` 的文件有**访问权限，无读取权限**；
3. `/bin/sh` 对 `test_daemo_exec`（原文如此拼写）只有**访问权限，没有执行权限**。

> 移远已经完成 SELinux 策略的框架，因此无需关注用户、角色和多级访问等，仅根据以下步骤完成策略编写即可。

**备注**：如果编译过程出现错误，说明以上定义的类型可能被使用过，可以自行定义其他类型。

### 6.1 设置 SELinux 配置文件

- 配置文件路径：`ql-ol-extsdk-ag35cetcar01a03m2g_ocpu/ql-selinux/config`。
- 移远仅提供如下默认配置：`SELINUX` 默认为 **`permissive`**，`SELINUXTYPE` 默认为 **`quectel`**。
- 可根据需要参考第 3 章自行修改配置值。

### 6.2 文件标签编写

- 文件标签配置文件路径：`ql-ol-extsdk-ag35cetcar01a03m2g_ocpu/ql-selinux/file_contexts`。
- 在该文件中增加如下内容（原文）：

```
/bin/sh           -l    u:object_r:shell_exec:s0
/bin/test_daemon  --    u:object_r:test_daemon_exec:s0
/run/test_file    --    u:object_r: data_file_type:s0
```

| 路径正则 | 类型标志 | 安全上下文 | 说明 |
|---|---|---|---|
| `/bin/sh` | `-l`（符号链接） | `u:object_r:shell_exec:s0` | shell 入口可执行 |
| `/bin/test_daemon` | `--`（普通文件） | `u:object_r:test_daemon_exec:s0` | 守护进程入口可执行 |
| `/run/test_file` | `--`（普通文件） | `u:object_r:data_file_type:s0` | 测试数据文件 |

> **解读**：注意原文 `data_file_type` 前有一个空格（`u:object_r: data_file_type:s0`），属书写排版；语义上该数据文件类型为 `data_file_type`。这一步对应第 4.2 节"file_contexts 里的类型必须在 type 文件中定义"——下一步 6.3.1 就定义这些类型。

### 6.3 策略规则文件编写

#### 6.3.1 类型文件编写

- 类型文件路径：`ql-ol-extsdk-ag35cetcar01a03m2g_ocpu/ql-selinux/type/file_type`。
- 在该文件中增加如下内容（原文）：

```
type data_file_type, exec_type, file_type;
```

> **解读**：声明 `data_file_type`，并把它归入 `exec_type` 与 `file_type` 两个属性（这样它就被属性级 allow 规则覆盖到，且被识别为文件类型）。

#### 6.3.2 TE 规则文件编写

策略文件目录：`ql-ol-extsdk-ag35cetcar01a03m2g_ocpu/ql-selinux/app/`。步骤如下：

**步骤 1**：在 `.../ql-selinux/app/quectel/` 目录下，创建 `test_daemon.te` 文件，内容如下（原文）：

```
#创建 test_daemon 类型，这是 test_daemon 成功运行后的进程类型
type test_daemon, domain, mlstrustedsubject;
#创建 test_daemon 文件类型为 test_daemon_exec
type test_daemon_exec, exec_type, file_type;

init_daemon_domain(test_daemon)
allow test_daemon data_file_type:file {open};
```

逐行解读：
- `type test_daemon, domain, mlstrustedsubject;`：定义进程域类型 `test_daemon`，归入 `domain`（标识它是一个进程域）和 `mlstrustedsubject`（MLS 可信主体）属性。这是 `/bin/test_daemon` 运行后进程应处的域。
- `type test_daemon_exec, exec_type, file_type;`：定义 `/bin/test_daemon` 文件的类型 `test_daemon_exec`，归入 `exec_type`（可执行）和 `file_type`（文件）属性。
- `init_daemon_domain(test_daemon)`：移远封装宏，自动建立"从 init 域执行 `test_daemon_exec` 入口文件后转换进 `test_daemon` 域"的完整域转换规则（等价于 type_transition + 一组 allow，参见 5.2.2）。
- `allow test_daemon data_file_type:file {open};`：授予 `test_daemon` 域对 `data_file_type` 文件的 **open（访问）权限**——注意只给 `open`、**不给 `read`**，恰好对应需求 2"有访问权限、无读取权限"。

**步骤 2**：在 `.../ql-selinux/app/system/` 目录下，修改 `shell.te` 文件，增加如下内容（原文）：

```
allow shell test_daemon_exec:file {getattr};
```

> **解读**：授予 `shell` 域对 `test_daemon_exec` 文件 **`getattr`（获取属性 = 访问）权限，但不给 execute/read**——对应需求 3"/bin/sh 对 test_daemon_exec 只有访问权限、没有执行权限"。

### 6.4 编译及使用

#### 6.4.1 编译文件系统镜像

- 在 SDK 根目录下，使用 **`make rootfs`** 可直接编译出含有 SELinux 策略的文件系统镜像。
- 生成位置：`ql-ol-extsdk-ag35cetcar01a03m2g_ocpu/target/` 目录下。

> 该步骤内部即会调用 4.1 节的 `setfiles`（打标签）+ `mksquashfs4`（打包），产出 `root.squashfs`。

#### 6.4.2 烧录进模块

- 将 `.../target/` 目录下编译生成的 **`root.squashfs`** 文件拷贝到固件版本包中，然后通过固件升级工具将固件烧录进模块（可参考文档 [1]）。

### 6.5 检查策略是否生效（页 19，关键验证流程）

系统开机后，通过如下步骤检查策略是否生效：

**步骤 1**：在系统 `/bin/` 目录下，执行 `ls -laZ` 查看 `test_daemon` 上下文类型是否为 `test_daemon_exec` 类型，如下：

```
-rwxr-xr-x   1 xxx xxx u:object_r:test_daemon_exec:s0 xxx xx xx xxxx test_daemon
```

> `ls -laZ` 的 `-Z` 显示 SELinux 安全上下文。看到第 5 列为 `u:object_r:test_daemon_exec:s0` 即文件标签生效。

**步骤 2**：通过 `dmesg | grep avc` 查看 avc log，确认 `test_daemon` 对 `data_file_type` 类型的文件是否无读取权限，如下：

```
[ 89.057338] type=1400 audit(315964856.019:6): avc: denied { read } for pid=1697 comm="sh"
name="87" dev="proc" ino=7819 scontext=u:r:test_daemon:s0
tcontext=u:object_r:data_file_type:s0 tclass=file permissive=1
```

avc 日志字段解读（原文标记为蓝色的内容）：

| 字段 | 值 | 含义 |
|---|---|---|
| `scontext` | `u:r:test_daemon:s0` | **主体进程**的安全上下文（谁在访问） |
| `tcontext` | `u:object_r:data_file_type:s0` | **客体资源**的安全上下文（被访问的目标） |
| `tclass` | `file` | 客体类别 |
| `denied { read }` | — | 被拒绝的操作是 `read` |
| `permissive=1` | — | 当前为 permissive 模式（仅记录、不真正拦截） |

> 因此可得出：`test_daemon` 类型的进程（由 scontext 决定）对 `data_file_type` 类型（由 tcontext 决定）的文件（由 tclass 决定）**缺少 read 权限**（由 denied 决定）。
> 此时若有需求变更，例如需要 `test_daemon` 对 `data_file_type` 类型文件具有读取权限，则需增加策略：**`allow test_daemon data_file_type:file read`**。

**步骤 3**：执行 `./test_daemon` 后，通过 `dmesg | grep avc` 查看 avc log，确认 `/bin/sh` 对 `test_daemon_exec` 是否只有访问权限、没有执行权限。如下：

```
[ 391.144781] audit: type=1400 audit(1603883212.531:198): avc: denied { execute } for
pid=2602 comm="sh" name="test_daemon" dev="ubiblock0_0" ino=113 scontext=u:r:shell:s0
tcontext=u:object_r:test_daemon_exec:s0 tclass=file permissive=1

[ 391.148476] audit: type=1400 audit(1603883212.531:199): avc: denied { read } for
pid=2602 comm="sh" path="/bin/test_daemon" dev="ubiblock0_0" ino=113 scontext=u:r:shell:s0
tcontext=u:object_r:test_daemon_exec:s0 tclass=file permissive=1

[ 391.200866] audit: type=1400 audit(1603883212.561:200): avc: denied
{ execute_no_trans } for pid=2602 comm="sh" path="/bin/test_daemon" dev="ubiblock0_0"
ino=113 scontext=u:r:shell:s0 tcontext=u:object_r:test_daemon_exec:s0 tclass=file permissive=1
```

> 上面的 avc log 表明 `shell` 进程对 `test_daemon_exec` 有访问权限（前面 6.3.2 步骤 2 授予了 `getattr`），但是**没有 `execute`、`read`、`execute_no_trans` 权限**；表明策略已成功生效（与需求 3 一致——shell 只能访问、不能执行 test_daemon）。

> **解读（整章验证逻辑闭环）**：
> - 步骤 1 验证**文件标签**对（type 正确）。
> - 步骤 2 验证**需求 2**：test_daemon 域对 data_file_type 文件只 open 不 read → 出现 `denied { read }`，说明没给 read，符合预期。
> - 步骤 3 验证**需求 3**：shell 域执行/读 test_daemon_exec 全被 denied（execute / read / execute_no_trans），仅 getattr 放行，符合预期。
> - 三条 avc 都带 `permissive=1`：当前是宽容模式，denied 只记录不拦截。若要真正阻断，需把 config 改成 `enforcing` 重新生效。`execute_no_trans` 指"执行但不发生域转换"，是另一种执行能力，这里也被拒，进一步坐实"shell 完全不能跑 test_daemon"。

---

## 7 附录（页 20）

### 表 1：参考文档

| 编号 | 文档名称 |
|---|---|
| [1] | Quectel_AG35-CET&AG35-EUT_QuecOpen(SDK)_快速开发指导 |

### 表 2：术语缩写

| 缩写 | 英文全称 | 中文全称 |
|---|---|---|
| IoV | Internet of Vehicles | 车联网 |
| SDK | Software Development Kit | 软件开发工具包 |
| SELinux | Security-Enhanced Linux | 安全增强型 Linux |
| TE | Type Enforcement | 类型强制 |

---

## 全文要点速查 / 工程落地提示

1. **三大前置条件**：`/etc/selinux/config`（模式 + 策略目录）、文件标签（file_contexts）、策略文件（`/etc/selinux/quectel/policy/`），三者缺一 SELinux 不生效。
2. **默认模式是 `permissive`**（移远出厂值）——只审计不拦截；量产强制需手动改 `enforcing`。`SELINUXTYPE=quectel` 把策略目录定位到 `/etc/selinux/quectel/`。
3. **安全上下文四元组** `user:role:type:level`（如 `u:object_r:bin_exec:s0`），TE 真正起作用的是 **type**。
4. **file_contexts 匹配规则**：逐行正则匹配，**最后匹配成功的一行胜出**；`-type` 标志（`-- / -d / -l / -c / -b`）限定文件种类；写的 type 必须在 type 文件中已定义。
5. **属性（attribute）= 类型分组**，可用属性级 allow 批量授权（`allow X file_type:file {...}` 覆盖所有归属 file_type 的类型）。
6. **域转换三要素**：源域 `execute` 入口文件、目标域对入口文件 `entrypoint`、源域可 `transition` 到目标域；推荐用宏 `domain_auto_trans(src, exec, dst)` / `init_daemon_domain(domain)` 一步完成。
7. **AV 规则关键字**：`allow`（放行）、`dontaudit`（抑制审计）、`auditallow`（放行并审计）、`neverallow`（编译期禁止断言）。
8. **完整开发闭环**：改 config → 写 file_contexts → 在 `type/file_type` 定义类型 → 在 `app/quectel/*.te`（自有进程）和 `app/system/shell.te`（系统进程）写 TE 规则 → `make rootfs` → 拷贝 `target/root.squashfs` 入固件烧录。
9. **验证三步**：`ls -laZ` 看标签 → `dmesg | grep avc` 看 `denied` 项 → 对照 `scontext/tcontext/tclass/denied{op}/permissive` 字段判断策略是否如预期；缺权限就补 `allow scontext tcontext:tclass op`。
10. **AG35 平台细节**：SDK 包名示例 `ql-ol-extsdk-ag35cetcar01a03m2g_ocpu`，文件系统为 squashfs（`mksquashfs4`，xz+arm BCJ），设备节点示例 `ubiblock0_0`（UBI 块设备）。

<!-- GENERATION_COMPLETE -->
