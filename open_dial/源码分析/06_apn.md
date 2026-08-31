# 06 — apn/apn.c + apn/apn.h

## 一、文件职责概述

APN 配置管理：从 `/usr/dial/apn.json` 按 ICCID **精确匹配** APN 记录，写入模组的公网 profile `DATA_CALL_APN_PUBLIC(=6)`；未命中则写空 APN 让模组自动选。文件头仍标注 sample `m_data_call.c`，含若干 sample 测试函数（`item_*`）与实际使用函数混杂。

> 注意：`apn.c:26` 与 `dial.c:114` 各有一个 `data_call_service_error_cb`。**dial.c 的是 `static`（文件内私有），apn.c 的是非 static 但无任何调用者**，故不构成链接冲突（已核对：Makefile 用 `find -name '*.c'` 编译全部源文件，两者共存但 dial.c 的 static 版遮蔽，运行期注册的是 dial.c 内的 static 版）。

## 二、关键数据结构、宏（apn.h）
```c
typedef struct { char iccid[32]; char apn[32]; char usr_name[32]; char pwd[32]; } apn_obj_t;
```
| 宏 | 值 |
|---|---|
| `APN_JSON_NAME` | `"apn.json"`（apn.h:77） |
| `APN_NAME_PUBLIC` | `"apnpublic"`（apn.h:78，与 _public.h:37 重复定义，值相同） |

## 三、逐函数分析

### sample 函数（运行期不调用）
- `data_call_service_error_cb`（`apn.c:26-29`）：仅 printf。**同名冲突见上**。
- `item_ql_data_call_init/set_service_error_cb/deinit`（`apn.c:31-74`）：sample 菜单封装。
- `dump_data_call_config`（`apn.c:76-131`）：打印 param 的 apn_id/ip_version/reconnect_mode/interval。
- `item_ql_data_call_get_list`（`apn.c:133-155`）：`ql_data_call_get_list` 打印实例列表。
- `dump_apn_cfg`（`apn.c:157-164`）：打印 apn_config 字段。
- `apn_set(apn_id, profile_idx, usr_name, pwd)`（`apn.c:166-188`）：构造 `cfg`（ip_ver=V4，`apn_name="apn<idx>"`），`ql_data_call_set_apn_config(apn_id, &cfg)`。**无工程内调用者**。

### `apn_obj_t *apn_load_from_json(char *json_path, int *p_apn_count)`（`apn.c:190-297`）
- `fopen` → `fseek/ftell` 求长度 → `malloc(len+1)` → `fread`（校验读取字节数）→ `str[len]='\0'` → `json_tokener_parse`。
- 取 `"apn"` 数组，`malloc(sizeof(apn_obj_t)*json_len)` 并 memset，`*p_apn_count=json_len`。
- 逐条 `json_object_object_get_ex` 取 `apn/iccid/usrname/pwd`，`strncpy` 到结构体（带截断+补 `\0`）。`supplier` 仅读不存。
- 结束 `json_object_put(obj)`（注释说明修复过 put(json_temp) 的误用崩溃——borrowed reference 不应 put）。`free(str)`、`fclose`。
- 返回堆数组（调用方 free）。fopen 失败/malloc 失败/fread 失败均清理返回 NULL。
- **潜在点**：`ftell` 返回 -1 时 `malloc(len+1)=malloc(0)`、`fread(...,-1,...)` 未防护（`apn.c:206-215`）；正常文件不触发，异常文件需实测。

### `int set_apn(char *iccid)`（`apn.c:299-373`）—— **运行期实际使用**
1. 拼 `json_path = "/usr/dial/apn.json"`（`apn.c:305`）。
2. `apn_load_from_json` 载入，打 `[APN] load … count=N`。
3. 若 count>0 且非空：遍历，`strcmp(p_apn_obj[i].iccid, iccid)==0` **精确匹配**（符合 CLAUDE.md 约定），命中则拷 apn_name/username/password/ip_ver=V4，打 `[APN] matched`，break（`apn.c:310-325`）。
   - 未命中（`i==apn_obj_count`）：打 `[APN] iccid not matched … fall back`（`apn.c:326-329`）。
   - 命中：`ql_data_call_set_apn_config(DATA_CALL_APN_PUBLIC=6, &apn_cfg)`（`apn.c:338`）。**写入 profile 6，符合 CLAUDE.md 强约定**。失败 free+return -1。
4. 若 `p_apn_obj==NULL || i==apn_obj_count || apn_obj_count==0`（未命中/无文件）：memset apn_cfg，**apn_name 置空串**（注释：让模组自动选 APN，不再写死 "apnpublic"），ip_ver=V4，`ql_data_call_set_apn_config(6, &apn_cfg)`（`apn.c:348-368`）。失败 `ALOGE` + free + return -1。
5. `free(p_apn_obj)`，return ret（QL_ERR_OK）。

- **注意**：命中分支与兜底分支**都写 profile 6**；兜底写空 apn_name（自动 APN）。这两步**串行**：若命中已成功 set，条件 `i==apn_obj_count` 为假、`p_apn_obj!=NULL`、`apn_obj_count>0`，则第 4 段不执行——命中时**只 set 一次**（正确）。

## 四、关键分支/阈值

| 位置 | 内容 |
|---|---|
| `apn.c:312` | ICCID `strcmp` 精确匹配（CLAUDE.md 约定） |
| `apn.c:338/362` | 均写 `DATA_CALL_APN_PUBLIC=6`（CLAUDE.md 强约定） |
| `apn.c:321/360` | `ip_ver=QL_NET_IP_VER_V4`（CLAUDE.md 约定） |
| `apn.c:348` | 兜底条件：无对象/未命中/count=0 → 空 APN 自动选 |

## 五、潜在问题 / 需实测确认

1. **`data_call_service_error_cb` 在 apn.c:26 是非 static 的死代码**：dial.c:114 的同名函数是 static，故无链接冲突；但 apn.c 的非 static 版本占用全局符号名且无调用者，建议改 static 或删除以消除隐患。
2. `apn_load_from_json` 对 `ftell` 返回 -1（fseek 失败）无防护（`apn.c:206`），`malloc(0)`/`fread(负数)` 风险，正常路径不触发，异常文件需实测。
3. `apn_set`（`apn.c:174`）用 `sprintf(cfg.apn_name, "apn%d", profile_idx)` 无长度保护，但无调用者。
4. CLAUDE.md 提到"APN7 私网"，代码注释 `apn.c:336` 也写 APN7 私网，但**实际只用 APN6 公网 profile**，私网 profile 7 代码未体现写入逻辑（需实测确认私网卡是否走别的路径/暂未实现）。
5. `apn_get_idx`（apn.h:80 声明）在 apn.c 中**无定义**——若被引用会链接失败，当前无调用者。
