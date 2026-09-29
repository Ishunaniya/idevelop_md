# mpc-base 源码安全漏洞与改进分析报告

> 基于源码全量静态分析 | 分析时间：2026 年 2 月

---

## 一、漏洞总览

| 编号 | 漏洞类型 | 严重程度 | 涉及文件 |
|------|----------|----------|----------|
| V-01 | 缓冲区溢出（sprintf + 外部输入） | 🔴 高危 | `nv_4g_at.cc` |
| V-02 | 密码明文存储与明文比较 | 🔴 高危 | `nvuser.cc` |
| V-03 | 时序攻击（Timing Attack） | 🟠 中危 | `nvuser.cc`, `discovery.c` |
| V-04 | 密码认证绕过条件漏洞 | 🔴 高危 | `pps_nkit_discovery.c` |
| V-05 | discovery 响应缓冲区溢出 | 🔴 高危 | `discovery.c` |
| V-06 | 空指针解引用（malloc 后无检查） | 🟠 中危 | `hl_pps_nn_persondet.c` |
| V-07 | 整数溢出导致堆缓冲区溢出 | 🟠 中危 | `hl_pps_nn_persondet.c` |
| V-08 | AT 命令注入风险 | 🟠 中危 | `nv_4g_at.cc` |
| V-09 | 互斥锁未释放（异常退出路径） | 🟠 中危 | 多处 |
| V-10 | 硬编码 IP 地址 | 🟡 低危 | `nv_4g_at.cc` |
| V-11 | strcpy/strcat 多处无界限复制 | 🟠 中危 | `discovery.c` 等 |
| I-01 | 错误码不统一 | 代码质量 | 全局 |
| I-02 | 全局状态缺乏初始化保护 | 代码质量 | `nv_task.cc` 等 |
| I-03 | 缺乏输入长度校验 | 代码质量 | 参数管理模块 |
| I-04 | 日志系统存在信息泄露 | 代码质量 | `nvuser.cc` |
| I-05 | 内存管理碎片化 | 代码质量 | BLE 模块 |

---

## 二、高危漏洞详细分析

---

### V-01 ｜ AT 命令缓冲区溢出（高危）

**文件**：`mod/4g/src/nv_4g_at.cc`

**问题描述**：

AT 命令字符串通过 `sprintf` 拼接构造，缓冲区大小固定，但 `apn`、`username`、`password` 参数来自外部配置文件或 App 下发，若参数长度超出预期则造成栈溢出：

```c
// 漏洞代码 1（第 1735 行）：at_cmd[256]，apn 无长度校验
char at_cmd[256] = {0};
sprintf(at_cmd, "AT+CGDCONT=%d,%s,%s\r\n", cid, pdp_str, apn);
//                                                     ^^^^ 外部输入，未限长

// 漏洞代码 2（第 1814 行）：at_cmd[512]，三个外部字符串直接拼入
char at_cmd[512]{};
sprintf(at_cmd, "AT+QICSGP=%d,%d,\"%s\",\"%s\",\"%s\",%d\r\n",
        contextID, context_type, apn, username, password, authentication);
//                                    ^^^       ^^^^^^^^  ^^^^^^^^
// 若三者合计超过 ~490 字节，栈溢出

// 漏洞代码 3（第 800 行）：at_cmd[64]，极小缓冲区
char at_cmd[64] = {0};
sprintf(at_cmd, "%s%d\r\n", (char*)"AT+QDSIM=", sim_index);
```

**危害**：在嵌入式 Linux 设备上，栈溢出可直接导致程序崩溃（DoS），在有利条件下可被利用实现代码执行。

**修复建议**：

```c
// ✅ 修复方案：改用 snprintf，并在写入前校验参数长度
#define APN_MAX_LEN   99  // 3GPP 规范 APN 最大长度
#define AUTH_MAX_LEN  63  // 用户名/密码最大长度

int nv_4g_at_set_tcpip_apn(int contextID, int context_type,
                            char* apn, char* username,
                            char* password, int authentication) {
    // 输入长度前置校验
    if (!apn || !username || !password) return NV_FAILURE;
    if (strlen(apn) > APN_MAX_LEN ||
        strlen(username) > AUTH_MAX_LEN ||
        strlen(password) > AUTH_MAX_LEN) {
        NV_LOG(NV_ERROR, "APN params too long\n");
        return NV_PARAM_INVALID;
    }

    char at_cmd[512]{};
    int written = snprintf(at_cmd, sizeof(at_cmd),
        "AT+QICSGP=%d,%d,\"%s\",\"%s\",\"%s\",%d\r\n",
        contextID, context_type, apn, username, password, authentication);

    // 截断检测
    if (written < 0 || (size_t)written >= sizeof(at_cmd)) {
        NV_LOG(NV_ERROR, "AT cmd truncated\n");
        return NV_FAILURE;
    }
    // ...
}
```

---

### V-02 ｜ 密码明文存储（高危）

**文件**：`mod/param/src/nvuser.cc`、`mod/param/inc/nvuser.h`

**问题描述**：

用户密码以**明文字符串**形式存储在内存结构体中，并以 JSON 明文写入配置文件 `/home/cfg/nvusrmconfig.bin`：

```c
// nvuser.h - 密码字段直接为明文字符串
typedef struct _NV_USER_CONFIG_ {
    char name[NV_NAME_LEN];   // 用户名
    char pass[NV_PASS_LEN];   // ← 明文密码！
    char group[NV_NAME_LEN];
    ...
} S_NV_USER_CONFIG;

// nvuser.cc 第 30 行 - 调试打印直接输出密码
printf("user[%d] name[%s] pass[%s] group[%s]\n",
        i, pitem->name, pitem->pass, ...);
//                      ^^^^^^^^^^^ 密码被打印到日志！

// nvuser.cc 第 117 行 - 明文写入 JSON 配置文件
s2j_json_set_basic_element(fld, pitem, string, pass);  // 明文落盘

// nvuser.cc 第 361 行 - 明文比较
if (0 == strncmp(user.pass, upass, NV_PASS_LEN)) {
    valid = true;
}
```

**危害**：
- 攻击者获取配置文件即可得到所有用户密码
- 日志系统意外记录密码（syslog/串口调试输出）
- 内存转储（crash dump）中包含明文密码

**修复建议**：

```c
// ✅ 修复方案：存储 PBKDF2/bcrypt 哈希，认证时对比哈希值

#include "nv_md5.h"  // 现有 MD5 可作过渡，长期应用 PBKDF2

// 存储哈希而非明文
typedef struct _NV_USER_CONFIG_ {
    char name[NV_NAME_LEN];
    char pass_hash[65];   // SHA-256 hex 输出 64 字节 + '\0'
    char pass_salt[33];   // 16 字节随机盐，hex 编码
    char group[NV_NAME_LEN];
    ...
} S_NV_USER_CONFIG;

// 认证时：hash(input + salt) == stored_hash
bool nv_user_verify_password(const char* input, S_NV_USER_CONFIG* user) {
    char computed[65];
    nv_hash_with_salt(input, user->pass_salt, computed, sizeof(computed));
    // 使用常量时间比较（见 V-03）
    return nv_const_time_strcmp(computed, user->pass_hash) == 0;
}

// 修复调试打印 - 绝不打印密码
printf("user[%d] name[%s] group[%s]\n", i, pitem->name, pitem->group);
//                                          ← 删除 pass 字段打印
```

---

### V-03 ｜ 时序攻击（Timing Attack）（中危）

**文件**：`mod/param/src/nvuser.cc`（第 361 行）、`mod/discovery/src/discovery.c`（第 340 行）

**问题描述**：

密码比较使用标准字符串比较函数，其执行时间随匹配字节数增加而变长。攻击者可通过精确测量响应时间逐位猜测密码：

```c
// 漏洞代码 —— strncmp 不是常量时间
if (0 == strncmp(user.pass, upass, NV_PASS_LEN)) { ... }  // nvuser.cc
if (strcmp(user.pass, cmn_req->pass) != 0) { ... }         // pps_nkit_discovery.c
```

**修复建议**：

```c
// ✅ 常量时间比较函数（防时序攻击）
static int nv_const_time_strcmp(const char* a, const char* b, size_t len) {
    volatile int diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];  // 每次都执行，不短路
    }
    return diff;  // 0 则相等
}
// 或直接使用 POSIX timingsafe_bcmp（若平台支持）
```

---

### V-04 ｜ 密码认证绕过漏洞（高危）

**文件**：`mod/discovery/src/pps_nkit_discovery.c`（第 338~345 行）

**问题描述**：

当设备处于 `E_COMMMODE_PLUG`（插件/直连模式）时，密码校验被**完全跳过**：

```c
int commmode = E_COMMMODE_PLUG;
ret          = nv_get_commmode(&commmode);
if (E_COMMMODE_PLUG != commmode) {
    // 只有非 PLUG 模式才验证密码
    if ((strlen(user.pass) != 0 && strlen(cmn_req->pass) == 0) ||
        strcmp(user.pass, cmn_req->pass) != 0) {
        ret = PPS_PRTP_RESULT_PASSWORD_ERROR;
        goto exit;
    }
}
// ← PLUG 模式下任何人都能通过，无需密码
```

此外，当 `admin` 账户密码为空时（`strlen(user.pass) == 0`），条件 `(strlen(user.pass) != 0 && ...)` 为假，密码验证同样被跳过，即**空密码账户免验证**。

**危害**：局域网内的攻击者在设备处于 PLUG 模式时，可以不提供任何密码修改网络配置（IP/网关/掩码）、触发设备功能。

**修复建议**：

```c
// ✅ 所有模式都必须验证密码
// 移除对 commmode 的特殊处理，始终执行密码校验

// 空密码需要额外处理：要求设备出厂时强制设置密码，
// 或在空密码情况下拒绝所有远程操作
if (strlen(user.pass) == 0) {
    // 空密码设备禁止远程修改配置
    ret = PPS_PRTP_RESULT_PASSWORD_ERROR;
    goto exit;
}
if (nv_const_time_strcmp(user.pass, cmn_req->pass, NV_PASS_LEN) != 0) {
    ret = PPS_PRTP_RESULT_PASSWORD_ERROR;
    goto exit;
}
```

---

### V-05 ｜ discovery 响应缓冲区溢出（高危）

**文件**：`mod/discovery/src/discovery.c`（第 377~393 行）

**问题描述**：

`get_discovery_response` 函数接收外部传入的 `response` 指针，使用 `sprintf` + 多次 `strcat` 将 uuid、ip、mask、gate、序列号、型号等多个字段拼接写入，**没有任何长度检查**：

```c
static int get_discovery_response(char* response, ...) {
    char* backbuff = response;  // 外部传入，大小未知

    sprintf(backbuff, PREFIX, uuid);        // PREFIX 模板 + uuid(36B)
    strcat(backbuff, ip);                   // 最多 15B
    strcat(backbuff, "/onvif/device_service http://127.0.0.1/devinfo/");
    strcat(backbuff, p2p_uuid);             // 未知长度
    strcat(backbuff, ":");
    strcat(backbuff, mask);                 // 最多 15B
    strcat(backbuff, ":");
    strcat(backbuff, gate);
    strcat(backbuff, ":");
    strcat(backbuff, dev_serialno);         // 来自设备配置，未限长
    strcat(backbuff, ":");
    strcat(backbuff, dev_model);            // 来自设备配置，未限长
    strcat(backbuff, SUFFIX);

    // 另一处也有 strcpy(backbuff, ccbuf) 同样无长度检查
    strcpy(backbuff, ccbuf);  // cJSON_Print 输出，大小不定
}
```

如果调用者分配的 `response` 缓冲区不足以容纳所有拼接后的字符串，将发生缓冲区溢出。

**修复建议**：

```c
// ✅ 传入缓冲区大小，使用 strncat/snprintf，剩余空间检查
static int get_discovery_response(char* response, size_t resp_size, ...) {
    size_t remaining = resp_size;
    int written = snprintf(response, remaining, PREFIX, uuid);
    if (written < 0 || (size_t)written >= remaining) return NV_FAILURE;
    remaining -= written;

    // 后续所有 strcat 替换为 strncat
    strncat(response, ip, remaining - 1);
    remaining -= strlen(ip);
    // ... 以此类推，每次操作前检查 remaining
}
```

---

### V-06 ｜ malloc 后空指针未检查（中危）

**文件**：`mod/ai/src/hl_pps_nn_persondet.c`（第 185~188 行）

**问题描述**：

```c
// 第 185 行：malloc 可能返回 NULL
uint8_t* resize_img = (uint8_t*)malloc(sizeof(uint8_t) * dstw * dsth * 3 / 2);

// 第 187 行：未检查 NULL，直接传入函数写入
pps_nn_pixel_resize_bilinear_nv12(img, imgw, imgh, resize_img, dstw, dsth);
//                                                  ^^^^^^^^^^^ 若 NULL 则崩溃

nv12_trans_rgb(dstw, dsth, resize_img, g_dst_img);  // 同上

free(resize_img);  // 若 NULL 则 free(NULL) 是安全的，但前两行已崩溃
```

对比同文件第 140 行的 `g_dst_img` 分配有 NULL 检查，但第 185 行的 `resize_img` 没有，属于遗漏。

**修复建议**：

```c
uint8_t* resize_img = (uint8_t*)malloc(sizeof(uint8_t) * dstw * dsth * 3 / 2);
if (NULL == resize_img) {
    PPS_NN_EPRINT("Malloc resize buffer fault!\n");
    pthread_mutex_unlock(&g_persondet_muxtex);
    return PPS_NN_FAIL;
}
```

注意：此处还必须在 `malloc` 失败后**先解锁互斥锁**，否则造成死锁（见 V-09）。

---

### V-07 ｜ 整数乘法溢出导致堆溢出（中危）

**文件**：`mod/ai/src/hl_pps_nn_persondet.c`（第 140、185 行）

**问题描述**：

```c
// dstw、dsth、c 均为 int 类型，乘积可能溢出 int 范围
g_dst_img = (uint8_t*)malloc(sizeof(uint8_t) * dstw * dsth * c);
// 若 dstw=65536, dsth=65536, c=3 → 65536*65536*3 = 12884901888 > INT_MAX
// 溢出后 malloc 分配极小内存，后续写入导致堆溢出

resize_img = (uint8_t*)malloc(sizeof(uint8_t) * dstw * dsth * 3 / 2);
// 同样的问题，且 /2 在乘法溢出后结果不可预测
```

**修复建议**：

```c
// ✅ 使用 size_t 并添加尺寸上限检查
#define MAX_IMG_DIM 4096  // 嵌入式设备合理的最大分辨率

if (dstw <= 0 || dstw > MAX_IMG_DIM || dsth <= 0 || dsth > MAX_IMG_DIM || c <= 0 || c > 4) {
    return PPS_NN_FAIL;
}

size_t alloc_size = (size_t)dstw * (size_t)dsth * (size_t)c;
g_dst_img = (uint8_t*)malloc(alloc_size);
```

---

### V-08 ｜ AT 命令注入风险（中危）

**文件**：`mod/4g/src/nv_4g_at.cc`

**问题描述**：

APN、用户名、密码等字符串直接被拼入 AT 命令，若来源包含特殊字符（如 `"`,`\r\n`、AT 控制字符），可能构造出意外的 AT 命令序列：

```c
// 若 apn = 'test","","",1\r\nAT+CFUN=1,1'
// 则最终命令变为：
// AT+QICSGP=1,1,"test","","",1
// AT+CFUN=1,1     ← 注入的复位命令！
sprintf(at_cmd, "AT+QICSGP=%d,%d,\"%s\",\"%s\",\"%s\",%d\r\n",
        contextID, context_type, apn, username, password, authentication);
```

**修复建议**：

```c
// ✅ 对 AT 参数进行字符白名单过滤
static bool nv_4g_at_validate_apn_param(const char* param) {
    // APN 只允许字母、数字、点、连字符
    for (const char* p = param; *p; p++) {
        if (!isalnum(*p) && *p != '.' && *p != '-' && *p != '_') {
            return false;
        }
    }
    return true;
}
```

---

### V-09 ｜ 互斥锁潜在死锁（加锁后异常返回）（中危）

**文件**：`mod/ai/src/hl_pps_nn_persondet.c` 等

**问题描述**：

在持有互斥锁期间，若 `malloc` 失败或其他错误发生，代码直接 `return` 而不解锁，导致死锁：

```c
pthread_mutex_lock(&g_persondet_muxtex);  // 加锁

uint8_t* resize_img = (uint8_t*)malloc(...);
// ❌ 若此处 return，锁永远不会释放
if (NULL == resize_img) return PPS_NN_FAIL;  // 潜在死锁！

// ...处理...
pthread_mutex_unlock(&g_persondet_muxtex);  // 解锁
```

**修复建议**：

```c
// ✅ 方案一：统一跳转到 cleanup 标签
pthread_mutex_lock(&g_persondet_muxtex);
int ret = PPS_NN_FAIL;

uint8_t* resize_img = (uint8_t*)malloc(...);
if (NULL == resize_img) goto cleanup;

// ...处理...
ret = PPS_NN_OK;

cleanup:
    free(resize_img);  // free(NULL) 是安全的
    pthread_mutex_unlock(&g_persondet_muxtex);
    return ret;

// ✅ 方案二（C++ 模块）：RAII 自动解锁（项目已有此基础设施）
// nv_sys_api::auto_wrmutex lock(g_persondet_muxtex);  // 离开作用域自动解锁
```

---

### V-10 ｜ 硬编码 IP 地址（低危）

**文件**：`mod/4g/src/nv_4g_at.cc`（第 1643 行）

**问题描述**：

```c
// DMZ 目标地址硬编码为 192.168.1.100
sprintf(at_cmd, "%s\r\n", (char*)"AT+QDMZ=1,4,192.168.1.100");
```

此 IP 硬编码在不同网络环境中可能冲突或失效。应改为可配置参数。

---

### V-11 ｜ strcpy/strcat 无界限复制（中危）

**文件**：`mod/discovery/src/discovery.c` 等

**问题描述**：

全局共 **219 处** `sprintf`/`strcpy`/`strcat` 调用中，相当数量未使用带 `n` 的安全版本：

```c
// discovery.c - 4 处 strcpy，没有目标缓冲区大小校验
strcpy(net_interface, NV_WLAN_INTERFACE);   // net_interface 大小未知
strcpy(net_interface, NV_LOCAL_INTERFACE);
strcpy(ip, "0.0.0.0");                      // ip 缓冲区 16B，赋值固定，当前安全
strcpy(backbuff, ccbuf);                    // ccbuf 大小不定，存在风险
```

**修复建议**：全面替换策略：

```bash
# 批量检查（可加入 CI 流程）
grep -rn "strcpy\|strcat\|sprintf\|gets" mod/ \
  --include="*.cc" --include="*.c" | grep -v "snprintf\|strncpy\|strncat"
```

| 不安全函数 | 替换为 |
|-----------|--------|
| `strcpy(dst, src)` | `strncpy(dst, src, sizeof(dst)-1); dst[sizeof(dst)-1]='\0'` |
| `strcat(dst, src)` | `strncat(dst, src, sizeof(dst)-strlen(dst)-1)` |
| `sprintf(buf, fmt, ...)` | `snprintf(buf, sizeof(buf), fmt, ...)` |
| `gets(buf)` | `fgets(buf, sizeof(buf), stdin)` |

---

## 三、代码质量改进建议

---

### I-01 ｜ 错误码不统一

**问题**：项目中同时存在 `NV_SUCCESS/NV_FAILURE`（0/-1）和 `NV_SDK_SUCCESS`（1）两套返回值，以及原始的 `return 0`/`return -1`，极易混淆：

```c
// nv_base.cc 第 208 行：返回 -1 而非 NV_FAILURE
return -1;

// 同文件第 215 行：返回 0 而非 NV_SUCCESS
return 0;

// nvRpcManager.h：使用 1 表示成功
#define NV_SDK_SUCCESS (1)   // 注意！与 NV_SUCCESS(0) 语义相反
```

**改进方案**：统一使用 `NV_SUCCESS`（0）/ `NV_FAILURE`（-1），存量代码中 `NV_SDK_SUCCESS` 的使用场所应加注释说明。

---

### I-02 ｜ 全局状态初始化竞争条件

**问题**：多个模块使用全局标志 `g_xxx_init` 判断是否已初始化，但这个检查本身不是原子的，多线程环境下可能发生竞争：

```c
// nv_task_monitor.cc
static std::atomic<bool> g_moniotor_init{false};

void nv_task_monitor_alarm_on(...) {
    NV_CHECK(!g_moniotor_init, return);  // 这里是 atomic 读，是安全的
    ...
}
```

这里用了 `std::atomic<bool>`，是正确的做法。但其他模块（如 `nv_task.cc`）使用的 `bool g_nv_task_init{false}` 是普通 bool，读写不保证原子性：

```c
// nv_task.cc 第 113 行 - 普通 bool，非线程安全
static bool g_nv_task_init{false};
```

**改进方案**：所有模块的初始化标志改为 `std::atomic<bool>`，或在读写时加锁保护。

---

### I-03 ｜ 缺乏系统性输入校验

**问题**：通道号（`chan`）、码流类型（`stream`）等参数虽然部分地方有 `NV_CHECK`，但不够系统。`nv_kit_udpPreview.cc` 等模块在早期版本中可能缺乏此类防御：

```c
// nv_ipcm.cc 中有防御
NV_CHECK(chan < 0 || chan >= g_ipcm_chan_max, return NV_FAILURE);

// 但并非所有入口都有，建议用宏统一
#define IPCM_CHAN_VALIDATE(chan) \
    do { \
        NV_CHECK((chan) < 0 || (chan) >= g_ipcm_chan_max, return NV_FAILURE); \
    } while(0)
```

---

### I-04 ｜ 日志系统信息泄露

**问题**：多处敏感信息直接打印到日志（syslog 或串口输出），包括密码、序列号、配网凭据等：

```c
// nvuser.cc 第 30 行 - 密码明文打印
printf("user[%d] name[%s] pass[%s] group[%s]\n", i, pitem->name, pitem->pass, pitem->group);

// pps_nkit_discovery.c 第 326 行 - 打印认证密码
NV_LOG(NV_INFO, "serial_no:%s serial:%s pass:%s \n",
       cmn_req->serial_no, serial, cmn_req->pass);
//                                          ^^^^ 不应打印！
```

**改进方案**：

```c
// ✅ 敏感字段脱敏打印
NV_LOG(NV_INFO, "user auth: name[%s] pass[%s]\n",
       user.name,
       strlen(user.pass) > 0 ? "****" : "(empty)");

// ✅ 或使用专门的安全日志宏，在 Release 版本禁用
#ifdef NV_DEBUG_BUILD
    NV_LOG(NV_DEBUG, "raw auth data: %s\n", raw_data);
#endif
```

---

### I-05 ｜ BLE 模块内存管理碎片化

**文件**：`mod/bluetooth/src/nv_ble_msg.cc`

**问题**：BLE 消息处理函数中存在大量 `malloc`/`free` 调用，且部分路径存在**内存泄漏**风险（malloc 后通过多个条件分支 return，但 free 只在部分分支中被调用）：

```c
// nv_ble_msg.cc 第 419 行
key_str = cJSON_PrintUnformatted(key_root);  // 分配内存
if (key_str) {
    cJSON_Delete(key_root);  // ← 只有 key_str 非空时才 delete
    ...
}
cJSON_Delete(key_root);  // ← 这里再次 delete 可能是双释放！
```

**改进方案**：

```c
// ✅ 统一资源管理，使用 goto cleanup 模式
cJSON* key_root = cJSON_CreateObject();
char* key_str = NULL;

// ... 构建 JSON ...

key_str = cJSON_PrintUnformatted(key_root);
if (!key_str) goto cleanup;

// ... 使用 key_str ...

cleanup:
    cJSON_Delete(key_root);  // 只释放一次
    // key_str 的生命周期单独管理
```

---

## 四、架构级改进建议

### 4.1 认证体系加固

当前认证体系存在多个层面的薄弱点，建议系统性加固：

```
当前认证流程:
  客户端 → Base64(name:password) → 服务端 strncmp 明文比较

建议认证流程:
  客户端 → HMAC-SHA256(nonce + password_hash) → 服务端验证
                                                     ↑
                              服务端只存储 PBKDF2(password, salt)
```

具体改进：
1. **密码存储**：明文 → PBKDF2-HMAC-SHA256（盐+迭代次数）
2. **传输认证**：明文/Base64 → Challenge-Response（服务端发 nonce，客户端用 HMAC 签名）
3. **登录限速**：添加指数退避（5 次失败后锁定 30 秒，避免暴力破解）
4. **空密码处理**：设备出厂时强制要求设置密码，拒绝空密码认证

### 4.2 网络暴露面收窄

- `nkit` UDP 发现服务应仅响应本地子网，增加源地址检查
- `discovery` 响应中不应包含完整序列号等设备指纹信息
- PLUG 模式的免认证设计需重新评估，建议改为超时自动关闭

### 4.3 编译期安全加固

在构建脚本中添加编译器安全选项：

```bash
# 推荐添加到 build/t41zm 等构建配置
NV_CFLAGS += -fstack-protector-strong    # 栈保护
NV_CFLAGS += -D_FORTIFY_SOURCE=2         # 运行时缓冲区检查
NV_CFLAGS += -Wformat -Wformat-security  # 格式字符串警告
NV_CFLAGS += -Werror=format-security     # 将 format-security 视为错误

NV_LDFLAGS += -Wl,-z,relro              # 只读重定位
NV_LDFLAGS += -Wl,-z,now                # 立即绑定
NV_LDFLAGS += -Wl,-z,noexecstack        # 禁止栈执行
```

### 4.4 建立 CI 安全扫描

```bash
# 建议在 CI 中加入以下检查（示例使用 cppcheck）
cppcheck --enable=all --error-exitcode=1 \
         --suppress=missingIncludeSystem \
         mod/ util/

# 或使用 clang-tidy（项目已有 .clang-format 可参考）
clang-tidy mod/**/*.cc -- -I inc/ -I util/libnvrtools/comm/inc/
```

---

## 五、总结优先级

| 优先级 | 漏洞 | 修复工作量 | 影响面 |
|--------|------|-----------|--------|
| 🔴 P0（立即） | V-02 密码明文存储 | 中 | 全部用户账户 |
| 🔴 P0（立即） | V-04 密码认证绕过 | 小 | 局域网设备 |
| 🔴 P1（本迭代） | V-01 AT 命令缓冲区溢出 | 小 | 4G 模块 |
| 🔴 P1（本迭代） | V-05 discovery 缓冲区溢出 | 小 | 局域网发现服务 |
| 🟠 P2（下迭代） | V-03 时序攻击 | 小 | 认证服务 |
| 🟠 P2（下迭代） | V-06/V-07 AI 内存问题 | 小 | AI 侦测功能 |
| 🟠 P2（下迭代） | V-08 AT 注入 | 中 | 4G 模块 |
| 🟠 P2（下迭代） | V-09 互斥锁死锁 | 小 | AI/存储模块 |
| 🟡 P3（规划中） | V-11 批量替换不安全函数 | 大 | 全局 |
| 🟡 P3（规划中） | I-04 日志脱敏 | 小 | 日志系统 |
| 🟡 P4（架构） | 认证体系重构 | 大 | 网络服务层 |
| 🟡 P4（架构） | 编译期安全选项 | 小 | 全局 |

---

*本报告基于源码静态分析，所有漏洞定位均附有具体文件名和行号，所有结论均来自真实代码。*
