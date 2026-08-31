# 12 — cc_deque/（cc_deque.c/.h + cc_common.c/.h）逐函数级

> **第三方开源库 Collections-C 双端队列**（作者 Srđan Panić，LGPLv3，文件头 c:1-19）。
> **关键事实：本工程内无任何业务代码引用它**（grep `cc_deque`/`cc_common`/`CC_Deque` 在目录外零命中）。Makefile 的 `find -name '*.c'` 会编译它，但无调用者 → vendored 死代码。
> 本篇按用户要求做**逐函数逐行**分析（1284 行全读）。因其未使用，末尾发现的上游缺陷不影响 open_dial 运行，仅如实记录。

---

## 一、cc_common（公共层）

### cc_common.h（69 行）
- `enum cc_stat`（h:36-48）：`CC_OK=0`、`CC_ERR_ALLOC=1`、`CC_ERR_INVALID_CAPACITY=2`、`CC_ERR_INVALID_RANGE=3`、`CC_ERR_MAX_CAPACITY=4`、`CC_ERR_KEY_NOT_FOUND=6`、`CC_ERR_VALUE_NOT_FOUND=7`、`CC_ERR_OUT_OF_RANGE=8`、`CC_ITER_END=9`（**注意跳过了 5**）。
- `MAX_POW_TWO`（h:29-33）：`ARCH_64` 定义时 `1<<63`，否则 `1<<31`。**本工程未定义 ARCH_64**（grep 无），故 aarch64 上仍取 `1<<31`——即容量上限被限制在 2³¹（需实测确认，但无调用无影响）。
- `CC_MAX_ELEMENTS=(size_t)-2`（h:50）；`INLINE`/`FORCE_INLINE` 跨编译器宏（h:52-62）；`CC_CMP_STRING=cc_common_cmp_str`（h:67）。

### cc_common.c（36 行）
- `int cc_common_cmp_str(const void *str1, const void *str2)`（c:33-36）：`return strcmp((const char*)str1,(const char*)str2)`。唯一函数。

---

## 二、cc_deque.c 结构与常量
- `struct cc_deque_s`（c:26-36）：`{size, capacity, first, last, void **buffer, mem_alloc, mem_calloc, mem_free}`——**环形缓冲**，`first`/`last` 为环形下标，`capacity` 恒为 2 的幂，故环绕用 `& (capacity-1)` 位与取模。
- `DEFAULT_CAPACITY=8`、`DEFAULT_EXPANSION_FACTOR=2`（c:23-24，后者定义但**全文件未使用**，扩容用硬编码 `<<1`）。

## 三、逐函数分析（cc_deque.c）

### 创建 / 配置 / 销毁
- **`cc_deque_new`**（c:51-56）：`cc_deque_conf_init` 填默认 conf → `cc_deque_new_conf`。
- **`cc_deque_new_conf`**（c:73-95）：`mem_calloc(1,sizeof)` 建结构（失败 `CC_ERR_ALLOC`）；`mem_alloc(conf->capacity*sizeof(void*))` 建 buffer（失败释放结构返回 ALLOC）；设分配器、`capacity=upper_pow_two(conf->capacity)`、`first=0`、`size=0`。
  - ⚠️ **上游缺陷1**：buffer 按 `conf->capacity` 分配（c:80），但 `capacity` 字段存的是 `upper_pow_two(conf->capacity)`（c:88）。若传入非 2 的幂的 capacity（如 5→向上取 8），buffer 只分配 5 个而 capacity=8，后续按 capacity 环绕写会**越界**。默认路径 capacity=8（已是 2 幂）不触发。
  - `last` 未显式赋值，靠 `mem_calloc` 清零为 0（c:75 用 calloc，安全）。
- **`cc_deque_conf_init`**（c:102-108）：capacity=8，分配器=malloc/calloc/free。
- **`cc_deque_destroy`**（c:115-119）：free buffer + free 结构（不动元素）。
- **`cc_deque_destroy_cb`**（c:130-134）：先 `remove_all_cb(cb)` 逐元素回调（释放元素），再 destroy。

### 增
- **`cc_deque_add`**（c:146-149）：= `add_last`。
- **`cc_deque_add_first`**（c:160-170）：满则 `expand_capacity`（失败 ALLOC）；`first=(first-1)&(cap-1)` 前移，写入，size++。
- **`cc_deque_add_last`**（c:181-191）：满则扩容；写 `buffer[last]`，`last=(last+1)&(cap-1)`，size++。
- **`cc_deque_add_at`**（c:206-289）：最复杂。越界 `CC_ERR_OUT_OF_RANGE`；满则扩容。计算环形下标 `c=cap-1, l, f, p`。index==0→add_first，index==c→add_last。否则按 index 在前半还是后半，选**左移**或**右移** memmove 腾位（含缓冲环绕跨界的分段 memmove，c:225-284），写入 `buffer[p]`，size++。逐分支 memmove 边界见 c:234-283。

### 删
- **`cc_deque_remove`**（c:332-341）：`index_of` 找位置 → `remove_at`。
- **`cc_deque_remove_at`**（c:356-422）：越界 OUT_OF_RANGE。index==0→remove_first，index==c→remove_last。否则按前/后半选 memmove 收缩（含环绕分段），`first`/`last` 相应内移，size--。
  - ⚠️ **上游缺陷2**（c:366）：`void *removed = deque->buffer[index]` 用的是**线性 index 而非环形下标 p**。当缓冲已环绕（first≠0）时，`out` 返回的"被删元素"是错误位置的值。memmove 逻辑本身用的是环形下标 p，故结构正确，仅 `*out` 回填值可能错。这是 Collections-C 已知问题。
- **`cc_deque_remove_first`**（c:435-448）：空 OUT_OF_RANGE；取 `buffer[first]`，`first=(first+1)&(cap-1)`，size--。
- **`cc_deque_remove_last`**（c:459-473）：空 OUT_OF_RANGE；`last=(last-1)&(cap-1)` 取值，size--。
- **`cc_deque_remove_all`**（c:482-487）：first=last=size=0（不缩容，不释放元素）。
- **`cc_deque_remove_all_cb`**（c:498-502）：先 `foreach(cb)` 逐元素回调，再 remove_all。

### 查
- **`cc_deque_get_at`**（c:515-523）：`i=(first+index)&(cap-1)`，`*out=buffer[i]`。
  - ⚠️ **上游缺陷3**（c:517）：越界判断 `if (index > deque->size)` 应为 `>=`。index==size 时越界一位仍返回 CC_OK，读到无效槽。
- **`cc_deque_get_first`**（c:534-541）/ **`cc_deque_get_last`**（c:552-560）：空 OUT_OF_RANGE，否则取头/尾。
- **`cc_deque_contains`**（c:710-721）：遍历环形，指针相等计数。
- **`cc_deque_contains_value`**（c:733-744）：同上但用 `cmp()==0`。
- **`cc_deque_index_of`**（c:757-769）：首个匹配返回其逻辑 index 与 CC_OK，否则 OUT_OF_RANGE。
- **`cc_deque_size`**（c:779-782）/ **`cc_deque_capacity`**（c:793-796）/ **`cc_deque_get_buffer`**（c:807-810）：返回字段/底层 buffer（const）。

### 替换 / 拷贝 / 变换
- **`cc_deque_replace_at`**（c:305-318）：越界 OUT_OF_RANGE；`i=(first+index)&(cap-1)`，可选回填旧值，写新值。（此函数用环形下标 i 正确，与 remove_at 的 c:366 缺陷对比可见）。
- **`cc_deque_copy_shallow`**（c:575-599）：分配同容量新结构+buffer，`copy_buffer(NULL)` 浅拷并**重新对齐**（first=0,last=size）。
- **`cc_deque_copy_deep`**（c:616-642）：同上但 `copy_buffer(cp)` 对每元素调 cp 深拷。
- **`cc_deque_trim_capacity`**（c:653-676）：容量==size 直接 OK；`new=upper_pow_two(size)`，等于当前则 OK；否则分配新 buffer、copy_buffer 对齐、释放旧、更新字段。
- **`cc_deque_reverse`**（c:683-700）：环形下标双指针 i/j 交换。**循环条件 `i < (s-1)/2`**（c:692），奇数长度中间元素不动，正确。
- **`cc_deque_foreach`**（c:819-827）：对每环形元素调 `fn`。
- **`cc_deque_filter_mut`**（c:840-858）：空 OUT_OF_RANGE；边遍历边 `remove_at` 不满足 pred 的元素（remove 后 i 不增），原地过滤。
- **`cc_deque_filter`**（c:874-896）：空 OUT_OF_RANGE；`cc_deque_new` 建新 deque，把满足 pred 的元素 add 进去，`*out` 返回。

### 内部辅助（static）
- **`copy_buffer`**（c:909-935）：cp==NULL 浅拷——若未环绕（last>first）一次 memcpy；否则分两段（first→末尾、开头→last）拼接对齐。cp≠NULL 时逐元素 cp。
- **`expand_capacity`**（c:948-968）：达 `MAX_POW_TWO` 返回 `CC_ERR_MAX_CAPACITY`；否则容量 `<<1`，`mem_calloc` 新 buffer，copy_buffer 对齐搬移，释放旧，重置 first=0/last=size。
- **`upper_pow_two`**（c:977-999）：≥MAX_POW_TWO 封顶；n==0 返回 2；否则位运算（Stanford bithacks）向上取 2 的幂。

### 迭代器（单）
- **`cc_deque_iter_init`**（c:1007-1012）：deque/index=0/last_removed=false。
- **`cc_deque_iter_next`**（c:1024-1040）：空或 index≥size 返回 `CC_ITER_END`；否则取当前环形元素，index++。
- **`cc_deque_iter_remove`**（c:1058-1072）：last_removed 则 VALUE_NOT_FOUND；`remove_at(index)`，成功 index--、last_removed=true。
- **`cc_deque_iter_add`**（c:1088-1095）：`add_at(index)`，成功 index++。
- **`cc_deque_iter_replace`**（c:1113-1116）：`replace_at(index)`。
- **`cc_deque_iter_index`**（c:1130-1133）：返回 `index-1`（上一个 next 返回的元素下标）。

### 迭代器（zip 双队列）
- **`cc_deque_zip_iter_init`**（c:1142-1148）：d1/d2/index=0/last_removed=false。
- **`cc_deque_zip_iter_next`**（c:1160-1186）：任一队列耗尽即 `CC_ITER_END`；否则同步输出两队列当前元素，index++。
- **`cc_deque_zip_iter_add`**（c:1201-1220）：index 越任一界 OUT_OF_RANGE；**先确保两队列都够容量再插**（原子性，注释 c:1206-1208），两 `add_at`，index++。
  - ⚠️ **上游缺陷4**（c:1209-1210）：容量预检用 `&&` 连接两个"满且扩容失败"，逻辑上只有**两个都满且都扩容失败**才返回 ALLOC；若只有一个满且扩容失败，会继续执行 add_at（该队列内部会再扩，但破坏了注释宣称的"要么都插要么都不插"原子性）。
- **`cc_deque_zip_iter_remove`**（c:1234-1249）：last_removed 则 VALUE_NOT_FOUND；越界检查；两 `remove_at(index-1)`，index--。
- **`cc_deque_zip_iter_replace`**（c:1263-1272）：越界检查；两 `replace_at(index-1)`。
- **`cc_deque_zip_iter_index`**（c:1281-1284）：返回 `index-1`。

## 四、头文件宏
- `CC_DEQUE_FOREACH`（cc_deque.h:142-149）：包一层 iter 遍历。
- `CC_DEQUE_FOREACH_ZIP`（cc_deque.h:152-160）：⚠️ **缺陷5**：init 用变量 `cc_deque_zip_iter_ea08d3e52f25883b`（h:155），next 用 `...883b3`（h:158，多一个 `3`），**两名字不一致**。一旦该宏被使用会编译报未声明变量。因无人使用不触发。

## 五、结论
- **对 open_dial 功能零影响**：全模块无业务调用者，可整目录移除而不需改任何业务代码（Makefile 的 `find` 移除后自然不再编译）。
- **上游缺陷 5 处**（缺陷1 buffer 容量、缺陷2 remove_at out 用线性 index、缺陷3 get_at 边界 `>` 应 `>=`、缺陷4 zip_add 原子性、缺陷5 FOREACH_ZIP 宏笔误）——均为 Collections-C 上游代码，**因未使用不影响本工程**，若将来启用需先修。
- **LGPLv3 许可**：随代码保留上游版权头，发布需注意与项目其余 Quectel/私有代码的许可差异。
