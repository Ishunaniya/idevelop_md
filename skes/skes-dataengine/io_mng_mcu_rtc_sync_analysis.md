# io_mng ↔ MCU RTC 校时逻辑分析（原始代码，未经任何改动）

日期：2026-07-08
范围：`rtms_sdk/apps/io_mng`（RK3576 + USB，`-DUSE_IO_MNG_USB -DQL_MODULE_PLATFORM_RK3576`，非 `RK3576_GANGJI`）
说明：本文档**只描述工作区改动之前的原始代码逻辑**，所有代码片段均以 `git show HEAD:<path>` 核对过、逐字对照原始文件，不包含本次排查过程中新增的任何诊断代码、也不包含依赖那些诊断代码才能得出的测试结论。仓库当前分支：`rk3576_20250821`（未切换）。

## 1. 背景

SKES RK3576 设备的系统时间来自多个信号源（GPS、MCU的PCF85163硬件RTC、用户UI手动设置、NTP），本文档梳理这些来源如何相互作用、原始代码里各自的触发条件与副作用，作为进一步排查的基础事实。

## 2. 涉及组件与数据流（原始逻辑）

```
GPS模块 ──NMEA──▶ io_mng(location) ──▶ set_time_from_gps() ─┬─▶ system("date -s ...")        [直接改Linux系统钟]
                                                              └─▶ sync_time_from_gps() ──▶ cmd_set_lst ─┐
MCU(PCF85163) ──USB帧头rtc_timestamp──▶ set_time_from_mcu() ──┬─▶ system("date -s ...")                │
                                                                └─▶ 写 /tmp/mcu_set_rtc.flag              │
                                                                                                            │
skes-dataengine(UI手动设置) ──nanomsg IPC──▶ io_mng(cmd.c) ──▶ push_cmd_req_item() ──▶ cmd_req_lst ◀───────┤
stub-sany-forklift(周期同步,600s) ──nanomsg IPC──▶ 同上 ────────────────────────────▶ cmd_req_lst ◀────────┘
                                                                                            │
                                                                                   io_mng_pack_cmd_items_ext()
                                                                                            │
                                                                                   USB TAG_CMD 帧 (CMD_TIME_SET, cmd_tag=100)
                                                                                            │
                                                                                            ▼
                                                                                          MCU（回复 cmd_reply_t.error）
```

**两条独立入队队列**（`cmd_set_lst` 用于GPS路径，`cmd_req_lst` 用于外部IPC路径），但最终都汇入同一个 `io_mng_pack_cmd_items_ext()`，打包成同样格式的 USB `TAG_CMD` 帧发给MCU，协议层面完全等价，唯一区别是 `cmd_request_t.pid` 字段的值（GPS路径固定填 `io_mng_get_pid()`，IPC路径原样保留外部调用方自己的pid）。

## 3. 逐函数原始逻辑

### 3.0 先确认：哪个文件是本构建实际编译的

`apps/io_mng/src/io_mng/` 目录下存在**三份**各自独立实现了 `set_time_from_mcu`/GPS同步逻辑的文件，CMake用 `GLOB_RECURSE` 通配 `src/*/*.c` 全部纳入编译，靠文件顶部各自的 `#if` 决定是否真正参与：

| 文件 | 顶部编译条件 | 本构建（`QL_MODULE_PLATFORM_RK3576`+`USE_IO_MNG_USB`）是否生效 |
|---|---|---|
| `io_mng.c` | `#if defined(QL_MODULE_PLATFORM_EC200A) \|\| ... \|\| (defined(QL_MODULE_PLATFORM_RK3576) && defined(USE_IO_MNG_USB))` | **是，本文档下面全部内容基于此文件** |
| `io_mng_imx6.c` | 文件内写的是 `#if 0`（原条件 `QL_MODULE_PLATFORM_MCIMX6Y2CVM08AB` 被注释掉，永久禁用） | 否，任何平台都不编译，纯历史遗留代码 |
| `io_mng_rk3576_gangji.c` | `#if defined(RK3576_GANGJI)` | 否，只有 `RK3576_GANGJI` 变体（2路SocketCAN+1路UART MCU）才编译，本构建未定义此宏 |

以下 3.1~3.6 全部基于 `io_mng.c`。

另需说明：`apps/io_mng/src/com/mcu_com.c` 里存在**第三种独立的RTC写入机制**（`MCU_COM_CMD_RTC_WR`，走 `mcu_com_request()`/UART通信，跟本文档描述的USB `TAG_CMD`/`CMD_TIME_SET` 是完全不同的协议和通道），但该文件整体也是 `#if defined(RK3576_GANGJI)`，本构建不编译，因此不在本文档讨论范围内。

### 3.1 `set_time_from_mcu(uint32_t rtc)`

文件：`apps/io_mng/src/io_mng/io_mng.c`

```c
static void set_time_from_mcu(uint32_t rtc)
{
	static bool is_mcu_rtc_set = false;
	...
	time_t rtc_timestamp = rtc; //Fixed bug.

	if (is_mcu_rtc_set == false)
	{
		localtime_r((time_t *)&rtc_timestamp, &mcu_time);

		// mcu rtc greater than or equals to 123 and less than 199(2099)
		if ((mcu_time.tm_year >= 123) && (mcu_time.tm_year < 199))
		{
			cur_time = time(NULL);
			now_time = localtime(&cur_time);

			sprintf(echo_cmd, "date -s \"%d-%d-%d %d:%d:%d\"",
					mcu_time.tm_year + 1900, mcu_time.tm_mon + 1, mcu_time.tm_mday,
					mcu_time.tm_hour, mcu_time.tm_min, mcu_time.tm_sec);
			system(echo_cmd);

			if (access(MCU_SET_RTC_PATH, F_OK) == -1)
			{
				fd = fopen(MCU_SET_RTC_PATH, "w+");
				if (fd)
				{
					fprintf(fd, "%d-%d-%d %d:%d:%d",
							mcu_time.tm_year + 1900, mcu_time.tm_mon + 1, mcu_time.tm_mday,
							mcu_time.tm_hour, mcu_time.tm_min, mcu_time.tm_sec);
					fclose(fd);
				}
			}

			is_mcu_rtc_set = true;
		}
		// else: 分支体是被注释掉的调试代码，原始逻辑里什么也不做
	}
}
```

调用点：`io_mng_parse_frame()` 里，每次解析到一帧USB数据都会调用 `set_time_from_mcu(p_comm_hdr->rtc_timestamp)`（大约每20ms一次，取决于USB交换周期）。

逐条拆解：

- `rtc` 参数来自 `comm_hdr_t.rtc_timestamp`，是MCU每一帧USB通信都会填的字段，**原始逻辑没有对这个字段做任何"只处理一次"之外的特殊处理，是MCU持续提供的原始数值**。
- 函数体内部靠 `static bool is_mcu_rtc_set` 做门控：**一旦条件满足执行过一次，此后同一个io_mng进程生命周期内，不管这个函数被调用多少次，都不会再做任何事**（哪怕`rtc`后续变化）。
- 触发条件：`mcu_time.tm_year` 落在 `[123, 199)`，即公历 2023~2098 年（`tm_year` 是"距1900年"的偏移）。这是唯一的合理性校验，**没有跟当前Linux系统时间做任何比较或校验**，只要年份落在这个宽泛区间就直接采信。
- 采信动作有两个、无先后依赖、都会执行：① `system("date -s ...")` 把Linux系统钟设成MCU汇报的值；② 如果 `/tmp/mcu_set_rtc.flag` 不存在就创建并写入同样的时间（`access(...) == -1` 才写，**已存在则不会覆盖**，注意这跟"只执行一次"的静态变量门控是两套独立的机制，任何一套满足就会阻止后续写入）。

### 3.2 `set_time_from_gps(void *pObj)`

文件：`apps/io_mng/src/io_mng/io_mng.c`

```c
static void set_time_from_gps(void *pObj)
{
	io_mng_t *p_io_mng = (io_mng_t *)pObj;
	location_mng_t *pLocationMng = p_io_mng->pLocationMng;
	location_common_info_t *p_lcInfo = &pLocationMng->lcInfo;
	static bool isSync = false;
	...

	if ((fabs(p_lcInfo->latitude)>DBL_EPSILON)
		&& (fabs(p_lcInfo->longitude)>DBL_EPSILON)
		&& (0 != p_lcInfo->timestamp)
		&& !isSync) {

		isSync = true;
		cur_time = time(NULL);
		secs = p_lcInfo->timestamp+8*60*60;

		if (cur_time > secs) {
			if (cur_time - secs < 2) { return; }
		} else {
			if (secs - cur_time < 2) { return; }
		}

		t = (time_t)secs;
		new_time = localtime(&t);
		sync_time_from_gps(p_io_mng, secs);
		sprintf(tmp, "r=`date -d @%d \"+%%Y-%%m-%%d %%H:%%M:%%S\"`; date -s \"$r\" > /dev/null 2>&1", secs);
		system(tmp);
	}
}
```

调用点：同样在 `io_mng_parse_frame()` 里，紧接着 `set_time_from_mcu()` 之后调用（调用顺序固定：先 `set_time_from_mcu`，后 `set_time_from_gps`）。

逐条拆解：

- 触发条件四个，`&&` 全部满足：GPS有纬度、有经度、有时间戳、`!isSync`（同样是静态变量门控，io_mng进程生命周期内最多执行一次实际同步逻辑）。**没有任何针对GPS时间戳本身"是否合理"的校验**（不检查年份、不检查跟当前系统时间差距是否过大，唯一的判断只是"差距是否小于2秒"，小于2秒则认为不需要同步、直接return，不做任何操作）。
- 一旦判定需要同步（差距≥2秒），做两件事、无先后依赖、都会执行：
  1. `sync_time_from_gps(p_io_mng, secs)` —— 见3.3，把 `CMD_TIME_SET` 请求塞进 `cmd_set_lst` 队列，后续异步发给MCU；
  2. `system("date -s ...")` —— **直接、无条件地把Linux系统钟改成GPS给出的时间**，跟①是否成功、MCU是否响应完全无关，是两个独立的副作用。
- 与 `set_time_from_mcu` 的执行顺序关系：如果开机后第一帧USB数据到达时，`set_time_from_mcu` 判定条件满足（MCU的RTC年份在合理区间），会先把Linux系统钟设成MCU汇报的值；紧接着同一帧处理流程里 `set_time_from_gps` 被调用，如果此时GPS已经有（哪怕是错误的）定位数据，会在**同一次 `io_mng_parse_frame` 调用内**把刚刚设好的系统钟再次覆盖成GPS给出的值。两者之间原始代码没有任何互斥、校验或先后依赖的设计。

### 3.3 `sync_time_from_gps(void *pObj, uint32_t gps_time)`

文件：`apps/io_mng/src/cmd/cmd_settings.c`

```c
int sync_time_from_gps(void *pObj, uint32_t gps_time)
{
	io_mng_t *p_io_mng = (io_mng_t *)pObj;
	cmd_settings_t *pCmdSet = p_io_mng->pCmdSet;
	cmd_settings_obj_t *pSetObj = NULL;
	cmd_request_t *pReq;
	char *ptr = NULL;

	pSetObj = calloc(1, sizeof(cmd_settings_obj_t));
	if (pSetObj)
	{
		ptr = calloc(1, sizeof(cmd_request_t) + sizeof(uint32_t));
		if (ptr)
		{
			pReq = (cmd_request_t *)ptr;
			pReq->pid = io_mng_get_pid();
			pReq->cmd_tag = CMD_TIME_SET;
			pReq->cmd_idx = pCmdSet->txIdx++;
			pReq->cmd_len = sizeof(uint32_t);
			memcpy((uint32_t *)(ptr + sizeof(cmd_request_t)), (char *)&gps_time, sizeof(uint32_t));

			pSetObj->cmd_tag = CMD_TIME_SET;
			pSetObj->status = CMD_SET_STS_NEED_UPDATE;
			pSetObj->pCmdData = (char *)ptr;
			pSetObj->cmdDataLen = sizeof(cmd_request_t) + sizeof(uint32_t);
			pSetObj->txIdx = pReq->cmd_idx;

			pthread_mutex_lock(&pCmdSet->cmd_set_mutex);
			pCmdSet->cmd_set_lst = List_push_end(pCmdSet->cmd_set_lst, (void *)pSetObj);
			pthread_mutex_unlock(&pCmdSet->cmd_set_mutex);
		}
		else { free(pSetObj); }
	}
}
```

只是构造一个 `cmd_request_t{pid=io_mng_get_pid(), cmd_tag=CMD_TIME_SET, cmd_len=4, payload=gps_time}`，连同一个用于追踪状态的 `cmd_settings_obj_t{status=CMD_SET_STS_NEED_UPDATE}` 一起放进 `cmd_set_lst`。

真正发送在 `cmd_settings_loop_cb()`（同文件内的周期定时器回调，`CMD_SETTINGS_CYCLE_SEC` 周期触发）里，该函数完整逻辑分三段：

```c
static void cmd_settings_loop_cb(EV_P_ ev_timer *w, int revents)
{
	...
#if	ENABLE_QL_TIME_SYNC
	if (!timeUpdatedSent && cmd_check_time_is_update())
	{
		if (0 == cmd_load_time_settings(p_io_mng)) { timeUpdatedSent = true; }
	}
#endif

	if (pCmdSet->settingsIsUpdate)
	{
		pCmdSet->settingsIsUpdate = false;
		config_load_current();          // 重新读 /etc/config/config.json
		cmd_clear_setobjs(p_io_mng);
		cmd_load_settings(p_io_mng);    // 重新加载CAN/COM/DI/AI等外设配置
	}

	// 遍历 cmd_set_lst，按状态机推进
	pthread_mutex_lock(&pCmdSet->cmd_set_mutex);
	length = List_length(pCmdSet->cmd_set_lst);
	while (i < length) {
		pSetObj = List_get(pCmdSet->cmd_set_lst, i);
		switch (pSetObj->status) {
			case CMD_SET_STS_NEED_UPDATE:
				push_cmd_req_item(p_io_mng->pCmdMng, pSetObj->pCmdData, pSetObj->cmdDataLen);
				pSetObj->status = CMD_SET_STS_WAIT_RESP;
				break;
			case CMD_SET_STS_WAIT_RESP:
				// 超过 CMD_SETTINGS_TMOUT_SEC(5秒) 没收到回复则退回NEED_UPDATE重试
				break;
			case CMD_SET_STS_UPDATED:
			case CMD_SET_STS_NOT_SUPPORT:
				// 从cmd_set_lst移除
				break;
		}
		i++;
	}
	pthread_mutex_unlock(&pCmdSet->cmd_set_mutex);
}
```

- 第一段（`#if ENABLE_QL_TIME_SYNC`）：**当前构建该宏未定义，整段不参与编译**，`cmd_check_time_is_update()`/`cmd_load_time_settings()` 是死代码，涉及 `/tmp/ql_time_set_flag`（`RTC_SYNC_FLAG_PATH`）的读取逻辑就在这一段里，但既然不编译就不会执行。
- 第二段（`settingsIsUpdate`）：跟时间无关，是外设配置（CAN波特率等）的热重载入口，确认过 `cmd_load_settings()` 里没有任何 `CMD_TIME_SET` 相关代码。
- 第三段：真正把 `cmd_set_lst` 里状态为 `NEED_UPDATE` 的项（含GPS路径塞入的CMD_TIME_SET）通过 `push_cmd_req_item()` 转入 `cmd_req_lst`（与IPC路径在此汇合），状态推进为 `WAIT_RESP`；`CMD_SETTINGS_TMOUT_SEC`（5秒）内没等到回复则状态退回 `NEED_UPDATE` 自动重试；收到回复后由 `dispatch_cmd_settings_reply_to_myself()`（见3.6）把状态置为 `UPDATED` 或 `NOT_SUPPORT`，下一轮循环里被移出队列。

### 3.4 IPC路径：UI手动设置 / stub-forklight周期同步 → `cmd_req_lst`

文件：`apps/io_mng/src/cmd/cmd.c`

`cmd_ipc_rcv_cb()`（nanomsg SUB socket收到外部IPC数据的回调）收到数据后，根据编译宏走以下三选一的分支，**本项目实际只有第三个分支参与编译**，前两个分支列出是为了说明完整性：

```c
#if defined (QL_MODULE_PLATFORM_MCIMX6Y2CVM08AB)
void parse_cmd_req_item(...)   // 只处理 CMD_NMEA_REQ / CMD_GPS_COM_REQ，其余（含CMD_TIME_SET）直接忽略、不入队
#elif defined (RK3576_GANGJI)
void parse_cmd_req_item(...)   // 只处理 CMD_TIME_SET，塞进cmd_req_lst；其余cmd_tag一律忽略、不入队
#else
void push_cmd_req_item(...)    // 本构建实际使用：不区分cmd_tag，全部原样入队cmd_req_lst
#endif
```

本构建（`-DUSE_IO_MNG_USB -DQL_MODULE_PLATFORM_RK3576`，未定义 `RK3576_GANGJI`）落在 `#else` 分支：

```c
void push_cmd_req_item(cmd_mng_t *pCmdMng, const char *pData, int dataLen)
{
	cmd_request_t *pCmdReq = NULL;
	cmd_req_obj_t *pCmdReqLstObj = NULL;
	int headerSize = sizeof(cmd_request_t);

	if (dataLen >= headerSize)
	{
		pCmdReq = (cmd_request_t *)pData;
		if (dataLen >= headerSize + pCmdReq->cmd_len)
		{
			pCmdReqLstObj = calloc(1, sizeof(cmd_req_obj_t));
			if (pCmdReqLstObj)
			{
				memcpy(&pCmdReqLstObj->cmdReqHdr, pCmdReq, headerSize);
				if (pCmdReq->cmd_len)
				{
					pCmdReqLstObj->pCmdBytes = calloc(1, pCmdReq->cmd_len);
					if (pCmdReqLstObj->pCmdBytes)
						memcpy(pCmdReqLstObj->pCmdBytes, pData+headerSize, pCmdReq->cmd_len);
					else { free(pCmdReqLstObj); return; }
				}
				pthread_mutex_lock(&pCmdMng->cmd_req_mutex);
				pCmdMng->cmd_req_lst = List_push_end(pCmdMng->cmd_req_lst, (void *)pCmdReqLstObj);
				pthread_mutex_unlock(&pCmdMng->cmd_req_mutex);
			}
		}
	}
}
```

这是 `#else` 分支（非 `QL_MODULE_PLATFORM_MCIMX6Y2CVM08AB` 也非 `RK3576_GANGJI`，即本项目实际编译使用的分支）。原样把外部通过nanomsg IPC发来的 `cmd_request_t`（含调用方自己填的pid）**不做任何校验或改写**，直接放入 `cmd_req_lst`。调用点：`cmd_ipc_rcv_cb()`（nanomsg SUB socket收到数据的回调）。

- UI手动设置时间：`skes-dataengine/vehicles/forklift/SkesQtBridge.cpp` 的 `sendTimeSetToMcu()`，构造 `cmd_request_t{pid=getpid(), cmd_tag=100, cmd_len=4, payload=UTC epoch(小端)}`，通过 `nn_send()` 一次性发送，**不等待、不处理MCU的回复**。
- 周期同步：`stub-sany-forklift.c` 的 `sync_time_to_mcu()`（`ev_timer`，开机60秒后首次触发，之后每600秒一次），条件 `handle->network_connected || access("/tmp/ql_time_set_flag", F_OK) == 0` 满足时，调用 `set_mcu_time()` 发送当前Linux时间（同样是fire-and-forget，不处理回复）。

### 3.5 打包发送（两条路径汇合点）

文件：`apps/io_mng/src/io_mng/io_mng.c`，`io_mng_pack_cmd_items_ext()`（`USE_IO_MNG_USB` 分支）

```c
static char* io_mng_pack_cmd_items_ext(void *pObj, char *p_spi_tx, int *p_room_len)
{
	...
	listLen = List_length(pCmdMng->cmd_req_lst);
	listLen = (listLen<SPI_EXCHANGE_CMD_ITEMS)? listLen: SPI_EXCHANGE_CMD_ITEMS;   // SPI_EXCHANGE_CMD_ITEMS = 1

	for (i=0; i<listLen; i++)
	{
		pCmdMng->cmd_req_lst = List_pop(pCmdMng->cmd_req_lst, (void **)&pCmdReqLstObj);
		if (pCmdReqLstObj)
		{
			... // 打成 usb_item_hdr_t{tag=TAG_CMD} + cmd_request_t + payload，写入USB发送缓冲区
		}
	}
	return p_spi_tx;
}
```

`SPI_EXCHANGE_CMD_ITEMS` 宏定义为 `1`，即**每个USB交换周期（约20ms）最多从 `cmd_req_lst` 弹出并发送1条 `CMD_TIME_SET`（或其它cmd_tag）请求**，不区分来源（GPS路径还是IPC路径），先进先出。

### 3.6 MCU回复处理

MCU的回复从USB接收到最终被消费，分两步，之前遗漏了第一步（接收解析），这里补全。

**第一步：接收解析，USB原始字节 → `cmd_rep_lst`**

文件：`apps/io_mng/src/io_mng/io_mng.c`，`io_mng_parse_frame()` 的 `case TAG_CMD` 分支：

```c
case TAG_CMD:
	push_cmd_rep_item(p_io_mng->pCmdMng, pItemInput, itemCount, itemSize);
	break;
```

`push_cmd_rep_item()`（文件：`apps/io_mng/src/cmd/cmd.c`）把这一帧里的 `cmd_reply_t` 原始字节解析出来，逐条 `calloc` 成 `cmd_rep_obj_t` 放入 `cmd_rep_lst`（跟 `cmd_req_lst` 是两个不同的队列，一个存"待发的请求"，一个存"收到的回复"）。这一步**没有任何针对 `cmd_tag` 的过滤**，任何tag的回复都会入队。

**第二步：消费处理，`cmd_rep_lst` → 按pid分派**

文件：`apps/io_mng/src/io_mng/io_mng.c`，`io_mng_dispatch_cmd_items()`。调用节奏取决于编译选项 `ENABLE_DISPATCH_OPTIMIZE`（`io_mng`的CMakeLists.txt里默认 `ON`）：

- `ENABLE_DISPATCH_OPTIMIZE=ON`（默认）：由 `io_mng_ipc_dispatch()` 这个 `ev_timer` 周期触发，周期 `IPC_DISPATCH_CYCLE_SEC = 0.1`秒（100ms）一次，同一次回调里还会顺带处理CAN/COM/GPS/事件/IMU等其它类型的分发。
- 若关闭该选项：走一个独立线程 `io_mng_ipc_dispatch()`（同名但是线程函数版本），`while(1)` 循环内 `usleep(IPC_DISPATCH_CYCLE_MSEC)`（`10000`微秒，即10ms）后重复处理同一批分发。

每次调用最多处理 `IPC_DISPATCH_CMD_ITEMS`（`10`）条回复。具体分派逻辑：

```c
if (pCmdRepLstObj->cmdRepHdr.pid == io_mng_get_pid())
{
	switch (pCmdRepLstObj->cmdRepHdr.cmd_tag)
	{
		case CMD_CAN_SET: case CMD_COM_SET: case CMD_GPS_SET:
		case CMD_DI_SET: case CMD_AI_SET: case CMD_TIME_SET: case CMD_EC_INFO_SET:
			dispatch_cmd_settings_reply_to_myself(p_io_mng->pCmdSet, pCmdRepLstObj);
			break;
		...
	}
}
else
{
	// 转发原始回复给外部IPC调用方
	set_pubsub_endpoint_snddata(pCmdMng->ep, ptr, headerSize + pCmdRepLstObj->cmdRepHdr.cmd_len);
}
```

- MCU的回复结构是 `cmd_reply_t{cmd_tag, error, pid, ...}`。
- 若回复里的 `pid` 等于 `io_mng_get_pid()`（即这条请求本身就是GPS路径以 `io_mng` 自己的pid发出的），走 `dispatch_cmd_settings_reply_to_myself()`：在 `pCmdSet->cmd_set_lst` 里查找匹配的 `cmd_tag`+`channel` 项，`error==0` 则把状态置为 `CMD_SET_STS_UPDATED`（下一轮 `cmd_settings_loop_cb()` 会把它从队列移除），否则置为 `CMD_SET_STS_NOT_SUPPORT`。
- 若 `pid` 不等于 `io_mng_get_pid()`（即这条请求来自外部IPC调用方，如dataengine或stub-forklight），原始回复原样通过nanomsg转发回该调用方（前提是调用方有监听SUB socket并处理，dataengine当前的 `sendTimeSetToMcu()` 实现里并未监听/处理这个回复）。
- **`dispatch_cmd_settings_reply_to_myself()` 里对 `error` 字段的处理，只是把内部状态从 `WAIT_RESP` 置为 `UPDATED` 或 `NOT_SUPPORT`，原始代码没有任何进一步校验"MCU是否真的执行了对应操作"，纯粹是协议层面"收到回复"的记账。**

## 4. 原始逻辑本身蕴含的问题（纯代码推导，不依赖任何运行时测试）

仅从上述代码结构即可推导出一个明确的设计缺陷，不需要任何额外的运行时观测：

**`set_time_from_gps()` 里的 `system("date -s ...")` 对GPS给出的时间戳没有做任何合理性校验（不检查年份范围，不检查跟当前已知时间的偏差是否合理，唯一的判断"差距<2秒则跳过"只是避免无意义的重复设置），而且这个 `date -s` 调用与 `sync_time_from_gps()`（发给MCU的部分）之间没有任何依赖或联动——不管MCU那边最终是否响应、响应是否成功，Linux系统钟都会被立即、无条件地改写。**

这意味着：如果GPS模块在真正锁星之前的某个阶段上报了一个不合理的时间戳（这是GPS接收机冷启动阶段的常见行为，不属于本代码库范畴），只要这个时间戳的经纬度非零、时间戳非零，且与当前系统时间的差距≥2秒，`set_time_from_gps()` 就会不加区分地执行 `system("date -s ...")`，把Linux系统钟改写成这个不合理的值。

同时，由于 `set_time_from_mcu()` 与 `set_time_from_gps()` 都各自只在**io_mng进程生命周期内执行一次真正的采信动作**（分别由 `is_mcu_rtc_set`、`isSync` 两个独立的静态布尔量控制），且固定顺序是"先MCU后GPS"，如果两者都在开机后短时间内触发一次，效果是：先用MCU的PCF85163时间把系统钟设对，然后立刻被GPS的（可能不合理的）时间戳覆盖——且这个覆盖动作在原始代码里没有任何撤销或重试机制，`isSync` 一旦置位，之后即使GPS给出更合理的时间也不会再触发。

## 5. GPS触发机制与数据来源完整分析

这一节专门梳理GPS这条路径——从"数据从哪来"到"实测复现过什么"到"能不能在这个仓库里修"，是完整的、经过交叉核实的结论。

### 5.1 代码层面的触发条件（3.2节已提过，这里完整复述）

```c
if ((fabs(p_lcInfo->latitude)>DBL_EPSILON)
    && (fabs(p_lcInfo->longitude)>DBL_EPSILON)
    && (0 != p_lcInfo->timestamp)
    && !isSync) {
```

四个条件`&&`全部满足才触发：纬度非零、经度非零、时间戳非零、本次io_mng进程还没同步过。**没有任何字段校验数值是否合理**（不检查年份范围、不检查跟当前系统时间的偏差是否离谱，唯一的判断是"差距<2秒就跳过不做"）。

### 5.2 关键发现：`p_lcInfo`（GPS数据）的真正来源，不是本地NMEA解析

`apps/io_mng/src/location/` 目录下只有4个文件：`gps_serial.c`、`gps_serial.h`、`location.c`、`location.h`。

**`gps_serial.c`（真正做NMEA解析、把GPRMC/GPGGA语句算成lat/lon/timestamp的代码）整个文件被**：

```c
#if defined (QL_MODULE_PLATFORM_RK3568_UBUNTU)  || defined(RK3576_GANGJI)
... (全部内容)
#endif
```

**包住。本构建是 `QL_MODULE_PLATFORM_RK3576` + `USE_IO_MNG_USB`，两个条件都不满足——这个文件对我们这次用的固件来说，一行代码都不会被编译进去，是完全的死代码。**

那么GPS数据实际是从哪来的？答案在 `location.c` 的 `push_location_items()`：

```c
case LC_INFO_TAG_0:
    pLcInfo = (location_common_info_t *)(pItemInput + sizeof(location_items_hdr_t));
    memcpy(&pLocationMng->lcInfo, pLcInfo, sizeof(location_common_info_t));
    pItemInput += itemSize;
    break;
```

这个函数**没有平台限制，无条件编译**，被 `io_mng_parse_frame()` 的 `case TAG_GPS: case TAG_NMEA:` 分支调用：

```c
case TAG_GPS:
case TAG_NMEA:
    push_location_items(p_io_mng->pLocationMng, pItemInput, itemCount, itemSize);
    break;
```

**结论：`location_common_info_t`（含lat/lon/timestamp）是通过USB `TAG_GPS`/`TAG_NMEA` 帧，由MCU发过来的一个已经解析好的结构体，io_mng这边只是原样`memcpy`塞进内存，不做任何解析、不做任何合理性判断。** 即使这个结构体里有 `is_gps_antenna_valid` 字段（暗示存在"天线/定位是否有效"这个概念），`push_location_items` 和 `set_time_from_gps` 都完全没有检查这个字段。

（注：`pGpsCom` 是另一套完全不同的机制，走 `TAG_GPS_COM`，用于GPS串口数据的透传/查询，跟这里讨论的定位信息无关，不要混淆。）

### 5.3 实测证据：GPS确实稳定给出错误时间戳，且真实写入过硬件RTC

跨2次独立开机复现（设备日志实测数据）：

| 开机 | GPS给出的目标(UTC+8本地时间戳) | 换算结果 |
|---|---|---|
| A | `1701849665` | 2023-12-06 16:01:05 |
| B | `1701849682` | 2023-12-06 16:01:22 |

两次仅相差17秒，且都精确落在"2023年12月6日"附近。这个模式（不是随机值，而是稳定复现在同一个基准附近）符合GPS模组常见行为：锁星前，芯片会输出一个固定/接近固定的占位时间戳（可能是芯片固件编译时间、或内部计数器的默认起点），而不是真正的卫星授时结果。**如果这个模块内部有自己的电池供电RTC且一直连续走时，两次读数不该只差17秒（应该是"2023-12-06 + 两次测试之间实际流逝的真实时间"，那样差值会是几个小时甚至更久），所以更可能是：这颗GPS芯片每次冷启动，内部计数器都从同一个固件默认基准重新开始，而不是靠自己的电池RTC连续走时。**

两次都被年份守卫加入前的原始逻辑忠实执行：`system("date -s ...")` 改了Linux系统钟，`sync_time_from_gps()` 发的CMD_TIME_SET也**真实写入了PCF85163硬件**（活体rtc_timestamp精确跳变到GPS给的错误值，日志有精确对应的`packed UTC=`/跳变/`error=0`三重证据）。

### 5.4 根因分析：这个错误值到底是怎么产生的

**核心判断依据：两次开机相隔的是不同的真实时间点（间隔应有数小时以上），但GPS给出的两次错误值只相差17秒（1701849665 vs 1701849682），且都精确落在2023年12月6日附近。**

如果这颗GPS模组内部有自己的电池供电RTC、并且这个RTC一直在连续走时，那么两次读数应该是"2023-12-06 + 两次开机之间实际流逝的真实时间"，差值该有数小时甚至更久，不可能只差17秒。

**更合理的解释是：这颗GPS模组根本没有独立的电池供电RTC（或者电池未装/已耗尽），每次冷启动、真正锁星之前，芯片固件会输出一个写死在固件里的默认/占位时间戳——很可能是这批GPS模组的出厂/固件烧录/测试日期，凑巧落在2023年12月6日前后。每次上电，这个占位值都从同一个固件常量重新开始，跟真实流逝的时间毫无关系，这就是为什么两次相隔很久的开机读数会如此接近。**

这意味着这个错误值**不是偶发噪声，而是每次冷启动、锁星完成前的必经阶段必然会出现的正常现象**——不是小概率异常，是100%会发生的事。

**真正的问题从来不是"错误值出现了要怎么防"，而是"这个锁星前的占位数据，本不该被当成有效定位往上传"。** NMEA协议本身就有专门表示这件事的字段——RMC语句的Status字段（'A'=有效定位，'V'=无效定位）、GGA语句的fix quality字段（0=无定位）。规范的做法应该是：**在数据源头（GPS模组固件、或者MCU侧解析NMEA的代码）判断这些有效性标志位，无效定位就不应该把lat/lon/timestamp当作"有数据"往上报给Linux侧。**

### 5.5 根因边界：这是MCU固件+GPS硬件模组的问题，不在rtms_sdk范围内

综合5.2~5.4：**GPS数据的解析（NMEA→lat/lon/timestamp）以及"这个定位是否可信"的判断，完全发生在MCU固件和实际连接的GPS硬件模组内部，rtms_sdk这个仓库（Linux侧代码）对这部分完全没有可见性，也没有代码可以修改。**

**因此，继续在io_mng的`set_time_from_gps()`里加更多针对"这次时间戳看着合不合理"的补丁（比如再加一道年份检查、再加一个阈值），是在用Linux侧的猜测去弥补数据源头本该做的有效性判断，治标不治本，而且永远补不完——只要MCU侧不做NMEA有效性过滤，任何基于数值特征的启发式判断都可能被绕过。**

真正需要的修复，是MCU固件团队排查GPS模组NMEA解析那部分代码，在往Linux侧转发`location_common_info_t`之前，先检查RMC的Status字段/GGA的fix quality，无效定位就不转发（或者转发但明确标记无效，供Linux侧据此判断），而不是像现在这样把包括无效定位在内的所有数据都透传上来。这是本次排查最终定位到的核心问题所在，已整理成书面报告，需要转交MCU团队跟进。

### 5.6 修复方案与验证结果

**修复**：在 `set_time_from_gps()` 触发条件之前加一道年份守卫——

```c
if (access(MCU_SET_RTC_PATH, F_OK) == 0) {
    FILE *fp = fopen(MCU_SET_RTC_PATH, "r");
    if (fp) {
        int y = 0;
        fscanf(fp, "%d", &y);
        fclose(fp);
        if (y >= 2025) {
            isSync = true;
            return;
        }
    }
}
```

逻辑：如果 `set_time_from_mcu()` 已经用MCU自己的PCF85163汇报值确认过"当前年份是靠谱的"（>=2025），就直接跳过GPS这次同步（含跳过`date -s`和跳过发CMD_TIME_SET给MCU两个动作），不管GPS这次给的数据对不对。

**验证**：修复恢复（取消注释）后，跨2次独立开机测试，日志里`MCU_FIRST_FRAME`之后再也没有出现过`GPS set_time_from_gps FIRED`这一行——对比修复前几乎每次开机都会看到这行、并伴随Dec2023污染，前后差异清晰、可直接在日志里对照确认。**这部分修复已验证生效。**

### 5.7 守卫的边界：GPS并非在所有情况下都被拦住

年份守卫**不是无条件拦截GPS**，只在下面这一种情况下才真正生效（`isSync=true; return;`）：flag文件存在**且**里面的年份`>=2025`。除此之外，GPS依然会正常执行`date -s`和发CMD_TIME_SET给MCU：

1. **`/tmp/mcu_set_rtc.flag` 不存在**——即`set_time_from_mcu()`没能成功写入flag（MCU汇报的rtc_timestamp本身不在`[2023,2099)`合理区间内）时，极少见但理论存在。
2. **flag存在，但里面的年份`<2025`**——即MCU自己的PCF85163当前正卡在一个陈旧值（比如2023、2024年）上。这是设计上有意放行的：思路是"MCU本身就不靠谱，放GPS去试着纠正"。

**但这里有一个未被消除的真实风险**：如果MCU当前卡在陈旧值（触发场景2），而GPS这次给出的数据**同样是陈旧/错误的**（比如仍然是5.3节观测到的Dec2023默认基准），守卫不会拦，GPS的错误数据会被正常写入——即"MCU已经脏了，GPS用另一个错误值继续往里写"这种情况，年份守卫防不住。这个缺口的根本解法不是在这里继续加校验（见5.8），而是等MCU固件那边解决GPS数据本身的有效性过滤问题（见5.5）。

### 5.8 结论：io_mng的年份守卫修改还有没有必要保留

**有必要，但定位要放对——它不是"修复"，是重启前防御，真正的修复在MCU固件侧（5.5）。**

理由：

1. **它不能替代MCU侧的NMEA有效性过滤，也不该被继续加码。** 5.5已经说明：数据源头不做有效性判断，Linux侧靠猜数值特征（年份、阈值……）去防，防不完、也不是正确的分工。5.7指出的缺口（MCU和GPS恰好同时给陈旧值）就是这种"猜"的方式必然存在盲区的证明，**不应该通过在io_mng里继续叠加更多启发式判断来补这个洞**——那只是在同一条错误路线上越走越远。
2. **但它作为一道低成本的防御，依然值得留着**：即使MCU固件把NMEA有效性过滤补上了，年份守卫也不会有负面影响——GPS数据一旦从源头就是有效的，年份守卫的判断条件根本不会被触发（因为不会再有"GPS给出陈旧时间戳"这种情况）；而在MCU固件修复上线之前的过渡期、或者未及时升级固件的存量设备上，它依然能挡住我们已经复现过2次的这个具体污染场景（MCU本身靠谱、GPS给错误值这一种情况）。
3. **它的成本几乎为零**：只是读一个本地flag文件、比较一个整数，不引入新的复杂度或副作用。

所以结论是：**保留年份守卫，但不再往里面加更多校验逻辑**（比如不加"GPS时间戳本身的年份检查"这类补丁）。5.7指出的那个缺口，正确的解决路径是推动MCU固件团队解决NMEA有效性过滤（5.5），而不是在io_mng这边继续堆更多针对数值特征的防御。

## 6. 涉及的文件/外部资源（原始逻辑范围内）

| 文件/资源 | 原始逻辑里的读 | 原始逻辑里的写 | 作用 |
|---|---|---|---|
| `/tmp/mcu_set_rtc.flag`<br>(`MCU_SET_RTC_PATH`) | 无（原始代码中没有任何函数读取此文件的内容，只有 `set_time_from_mcu` 用 `access()` 判断是否存在） | `set_time_from_mcu()`：仅当文件不存在时创建并写入一次 | 存在性标记，原始逻辑里唯一用途是防止 `set_time_from_mcu` 重复写文件（注意这跟函数本身"只执行一次"的静态变量门控是重复的双重保险） |
| `/tmp/ql_time_set_flag`<br>(`RTC_SYNC_FLAG_PATH`，定义于 `cmd_settings.h`) | 1) `stub-sany-forklift.c` 的 `sync_time_to_mcu()`：决定离线设备是否仍发送周期性CMD_TIME_SET；2) `cmd_settings.c` 的 `cmd_check_time_is_update()`（**整段代码被 `#if ENABLE_QL_TIME_SYNC` 包裹，当前构建该宏未定义/为0，属于不参与编译的代码**） | 在已排查的 `rtms_sdk`、`stub-sany-forklift`、`skes` 三个仓库范围内，**未找到任何写入此文件的代码** | 写入方不明，需要额外确认（可能是产线脚本或其他未纳入排查范围的组件） |
| `/data/forklift_persistent.json`<br>(`PERSISTENT_FILE`，dataengine) | dataengine `loadPersistent()`：读取 `autoSetDateTime`（默认1=NTP自动） | dataengine `savePersistent()`：UI修改时写回 | 控制chronyd是否启用，与MCU/PCF85163的RTC完全是两套独立机制 |
| `/etc/localtime` | libc内部所有 `localtime()`/`localtime_r()` 调用（含io_mng的 `set_time_from_mcu`/`set_time_from_gps`） | dataengine `applyTimezone()`：`symlink()` 到 `/usr/share/zoneinfo/<zone>` | 决定UTC↔本地时间换算规则 |
| `/etc/init.d/S49chrony`（脚本） | 无 | dataengine `setNtpEnabled()` 调用其 `start`/`stop` | 控制chronyd（NTP客户端）进程本身的启停 |
| USB `comm_hdr_t.rtc_timestamp` 字段 | `set_time_from_mcu()` 每帧读取（仅开机后第一次真正采信） | 由MCU固件填写（协议字段，非文件） | MCU侧PCF85163的实时镜像值 |
| Linux `CLOCK_REALTIME`（非文件） | 各处 `time(NULL)` | 1) `set_time_from_mcu`/`set_time_from_gps` 里的 `system("date -s ...")`；2) dataengine `setSystemClockUTC()`（`clock_settime()`）；3) chronyd（若启用） | 三个独立写入方之间原始逻辑没有任何互斥或协调机制 |

## 7. 小结：原始逻辑的关键特征

1. **两个独立的一次性门控**（`set_time_from_mcu` 的 `is_mcu_rtc_set`，`set_time_from_gps` 的 `isSync`），互不知晓对方状态，固定按"先MCU后GPS"的顺序各自最多生效一次。
2. **GPS路径对时间戳合理性零校验**，只要坐标非零、时间戳非零、跟当前系统时间差距≥2秒，就会无条件执行 `date -s`。
3. **`date -s`（改Linux系统钟）与"发CMD_TIME_SET给MCU"是两个并列、无依赖的副作用**，前者的执行不以后者成功为前提。
4. **MCU的回复（`cmd_reply_t.error`）在原始代码里只用于维护 `cmd_set_lst` 内部状态机（WAIT_RESP→UPDATED/NOT_SUPPORT），不代表、也没有任何机制去验证MCU是否真的完成了对应的硬件操作**——这是纯代码结构层面就能看出的设计，不需要额外测试验证。
5. IPC路径（UI手动设置、stub-forklight周期同步）和GPS路径殊途同归，最终都通过同一个 `cmd_req_lst` 队列、同一个打包函数、同一种USB帧格式发给MCU，纯协议层面看不出任何差异对待。

## 8. 新发现的独立问题：erk-agent云端时间同步会污染系统钟

排查过程中，通过给 `rootcloud-stp-sdk/erk-core/rcpv4/rcpv4-cmd-time.c` 的 `rcpv4_cmd_on_post_time()` 加诊断日志（写入同一份 `/tmp/io_mng_cmd.log`），发现了第三个、与GPS完全独立的系统钟污染源。

### 8.1 机制

`rcpv4_cmd_on_post_time()` 是erk-agent处理云端/Hub"post time"响应的回调，用类似NTP的往返时延算法计算时间偏差：

```c
ts = (device_recv_time + hub_recv_time + hub_send_time - device_send_time) / 2;
...
if (diff < proto_handle->timing_info.diff) {  // 阈值30秒
    // 差距小，不调整
} else {
    erk_set_clock_time(ts);  // 差距大，直接改系统钟
}
```

`device_send_time`/`device_recv_time` 是本机的tick计数（小数值，相对时间），`hub_recv_time`/`hub_send_time` 是Hub返回的绝对时间戳（毫秒级epoch）。

### 8.2 实测证据：跨2次独立开机复现，每次都把系统钟改成上一次开机session的陈旧值

| 开机 | Hub给出的cloud_ts | 实际当前时间 | 时间差 | 是否真实执行了`erk_set_clock_time` |
|---|---|---|---|---|
| A | 2026-07-09 17:41:19 | 2026-07-09 20:36:03 | 约2小时55分 | 是，`rc=0` |
| B | 2026-07-09 18:04:38 | 2026-07-09 20:11:12 | 约2小时6分 | 是，`rc=0` |

两次的关键特征完全一致：**`hubRecvTime` 和 `hubSendTime` 两个值完全相等**（正常的网络往返里，Hub收到请求和发出回复的时间戳不该分毫不差），且给出的值都精确落在**上一次开机session的活动区间内**——强烈暗示这是Hub端返回了一个缓存/滞留的旧响应，而不是针对本次请求实时计算的结果。

### 8.3 完整污染链路：从erk-agent到PCF85163，两次独立复现，时间线精确到秒验证

**erk-agent污染Linux系统钟 → 该错误基准正常走时 → stub-forklight周期同步（开机后60秒首次触发）读到已被污染的系统钟、如实转发给MCU → PCF85163被写脏。**

以复现B为例，两条时间线精确对应（用Python计算验证过）：

- 真实时间线：erk-agent污染发生在 `20:11:12`，stub-forklight读取发生在 `20:12:10`，间隔 **58秒**
- 被污染时间线：污染基准 `18:04:38`，stub-forklight读到的值 `18:05:36`，间隔 **58秒**

两个58秒分秒不差，证明Linux系统钟被污染后按正常速度继续走时，不是随机跳变。stub-forklight自己打印的日志（`pid=6554 wallclock=18:05:36 computed_sec=18:05:36`）直接证实它读到的就是这个被污染的值，不是靠pid或时间点去猜的推论。

**这个链路里，stub-forklight本身没有任何问题**——它只是忠实地把当时Linux系统钟显示的值转发给MCU，错误的源头是erk-agent，不是stub-forklight。

### 8.4 临时挡板：已加入，但不是根治

在调用`erk_set_clock_time(ts)`之前，加了一道基于"本次开机基准"的合理性检查：

```c
/* 读 /tmp/mcu_set_rtc.flag（本次开机时MCU自己汇报的已知靠谱时间）作为基准，
 * 如果云端算出来的ts比这个基准还早，判定为陈旧/缓存响应，跳过、不采信 */
FILE *fp = fopen("/tmp/mcu_set_rtc.flag", "r");
if (fp) {
    // 解析出 boot_baseline_ms
    if (ts < boot_baseline_ms) {
        // 记诊断日志 SKIPPED(临时屏蔽,...)，然后 return -1，不调用erk_set_clock_time
    }
    fclose(fp);
}
```

逻辑：一个合理的时间修正，不该把时钟拨到"比这次开机还早"的时间点（除非设备真的被人为拨回过去）。两次实测复现的污染值都精确落在上一次开机session内，必然早于本次开机基准，这道挡板能够拦住已观测到的这两次场景。

**这不是根治方案，是权宜之计**，代码注释里也写明了。已知的两处边界（review时发现，不是bug，是设计取舍）：

1. **失效开放（fail-open）**：如果`/tmp/mcu_set_rtc.flag`还不存在（比如io_mng还没来得及在本次开机写入）或者解析失败，挡板不拦截，直接放行——宁可这种罕见情况下不拦，也不让整个云端校时功能锁死。
2. **可能误拦合理的大幅度往回修正**：如果设备连续运行很久、时钟漂移量大到需要往回拉、且拉的幅度跨过了开机边界，这种（理论上存在但现实中概率很低的）合理场景也会被一并挡住。

代码同时改动了两处（这两份文件此前确认过内容完全一致，需要同步维护）：
- `rootcloud-stp-sdk/erk-core/rcpv4/rcpv4-cmd-time.c`（实际编译使用的版本）
- `stub-sany-forklift/stub-sany-forklift.c`（独立仓库那份，未参与本次实际编译，留作后续参考）

### 8.5 根因边界：尚未定位，需要进一步排查

`hubRecvTime == hubSendTime` 这个特征目前只是观察到的现象，**为什么Hub会返回一个陈旧、缓存的响应，具体原因尚未查明**，怀疑与设备重启导致连接中断、重连后收到中断前已在路上的滞留响应有关，但这只是推测，没有验证。需要在云端/Hub协议层或erk-agent连接管理逻辑（重连时是否正确丢弃过期的在途请求/响应）里进一步排查。已加诊断日志，下次复现可以获取更多细节（比如`deviceSendTime`等字段是否也能反映出连接重建的时间点）。**8.4的挡板只是让复现时不再产生实际破坏，不能替代这里的根因排查。**

### 8.6 相关的排查用诊断日志清单（额外补充，非原始逻辑一部分）

为了闭环这条链路的证据，除了`rcpv4_cmd_on_post_time()`本身的日志外，还在`stub-sany-forklift.c`的`sync_time_to_mcu()`里加了诊断日志，直接打印`pid`（用`getpid()`，不用再靠pid+时间点去推测身份）、读到的系统钟原始值、换算后的本地时间、`network_connected`/`ql_time_set_flag`状态、是否会发送——这是本次调查里唯一一处对stub-forklight源码的改动，纯诊断，不改变原有行为。

### 8.7 复现测试证实：挡板本身有效，但暴露了第4个、目前仍未定位的污染源

2026-07-10的一次完整复现（UI设15:30→reboot）里，挡板首次在真实复现场景下被验证：

```
ERK_AGENT rcpv4_cmd_on_post_time wallclock=15:30:42 cloud_ts=11:30:55 local_ts_sys=15:30:42 diff_ms=14387596 ...
ERK_AGENT rcpv4_cmd_on_post_time SKIPPED(临时屏蔽,ts早于本次开机基准) target_ts_ms=... boot_baseline_ms=...
```

`local_ts_sys`（erk-agent自己读到的系统钟）在这一刻是**正确的15:30:42**，陈旧的`cloud_ts`被挡板正确拦截，没有调用`erk_set_clock_time`。**这证实8.4的挡板对它设计要拦截的场景（erk-agent直接采信陈旧云端响应）确实有效。**

但同一次复现里，系统钟仍然在56秒后（15:31:40附近）被改成了错误值（`STUB_FORKLIFT`读到`get_clock_time_ms`对应`11:31:53`），进而被stub-forklight原样转发写脏了MCU。这次污染**不是**通过erk-agent发生的——`rcpv4_cmd_on_post_time()`在这段窗口里只被调用过一次，就是上面那条被挡板拦下的记录，之后没有任何调用，更没有任何`erk_set_clock_time_rc=`的日志（该日志无条件打印在每次真正调用`erk_set_clock_time`之后，没有等于没发生）。

结论：**问题3（erk-agent直接污染）已经过真实复现验证并被挡板正确拦截，可以视为该来源已封堵。但系统钟仍会在开机后一分钟以内被某个尚未定位的第4个来源污染，只是这次不是走erk-agent。**

排查过程中排除了两个曾经怀疑的方向：

1. **`get_clock_time()`本身有问题（比如内部维护软件RTC/缓存了错误的基准时间）**——已直接读取`stub-sany-forklift.c`第196-204行源码排除：这个函数就是纯粹的`clock_gettime(CLOCK_REALTIME, &tv)`，没有任何缓存或内部状态，跟`erk_get_clock_time()`语义完全一致。它读到错误值只能说明当时Linux的`CLOCK_REALTIME`本身真的已经被别的东西写坏了，不是它自己算错的。
2. **开机脚本里有`fake-hwclock`之类的时钟恢复机制，读取了一个陈旧的持久化文件**——已在设备上执行`grep -rl hwclock /etc/init.d/`、`find / -iname "*fake-hwclock*"`等命令排除，均无匹配。

当前唯一还没排除的候选是`ntpd`（`-u ntp:ntp -g -p /var/run/ntpd.pid`，独立于dataengine对chronyd的控制之外常驻运行，`-g`参数关闭了ntpd正常的"panic"合理性检查，允许无限制的初次时间跳变）。**这仍然只是嫌疑，没有直接证据**——见8.8的事故记录，针对它的持久化strace尝试目前因为一次严重的操作事故而中断，还没有真正抓到过一次污染现场。

### 8.8 重大事故记录：用`strace -f`包裹ntpd二进制导致设备开机卡死（wifi/UI全部起不来）

为了让ntpd的strace能在重启后依然生效（之前用`strace -p <pid>`附加到已运行进程，重启后pid失效、旧strace进程随之消亡，完全没监控到下一次开机），尝试了跟erk-agent同样成功过的"二进制wrapper"手法：

```bash
mv /usr/sbin/ntpd /usr/sbin/ntpd.real
cat > /usr/sbin/ntpd << 'EOF'
#!/bin/sh
exec strace -f -e trace=clock_settime,settimeofday -tt -o /data/ntpd_strace.log "$(dirname "$0")/ntpd.real" "$@"
EOF
chmod +x /usr/sbin/ntpd
```

**这个方法对erk-agent有效，但对ntpd是致命的**：`strace -f`会持续跟踪所有fork出去的子进程，直到它们全部退出才返回；而ntpd是自己fork到后台常驻的daemon（永不退出），open init脚本里同步调用的`ntpd ...`这一行（此时已经变成`strace -f ... ntpd`）因此**永远不会返回**。这块设备的init是严格顺序执行的（sysvinit风格的S脚本），"Starting ntpd:"这一行卡死后，后面所有服务（wifi、UI、getty登录）全部没有机会启动。

**故障确认证据**：对比开机log里其它服务的输出模式——

```
Starting chrony: [...内核消息...] OK
Starting network: ... OK
Starting ntpd: [...内核消息，穿插了一次跟ntpd完全无关的sound_light_alarm的GPIO pin冲突WARNING...]
（再没有任何输出，"** 266 console messages dropped **"之后彻底静默）
```

`Starting ntpd:`后面始终没有出现`OK`，跟其它服务形成鲜明对比，直接证实卡在这一步。13.4秒那次内核WARNING（`Comm: sound_light_ala`触发的GPIO pin复用冲突）是完全独立的、内核自己扛过去了的问题，跟这次卡死无关，容易被误判为"内核崩溃"，实际上内核和其它已启动的后台任务都还活着，只是**用户态的顺序init流程被卡死**。

**恢复过程**：设备唯一还能用的通道是U-Boot串口。上电时连续按`Ctrl+C`打断autoboot，进入U-Boot命令行后：

```
setenv bootargs "${bootargs} init=/bin/sh"
boot
```

让内核跳过正常init直接进裸shell，再手动挂载可写、回滚wrapper：

```
mount -o remount,rw /
rm -f /usr/sbin/ntpd
mv /usr/sbin/ntpd.real /usr/sbin/ntpd
sync
reboot
```

恢复后wifi/UI均已正常。

**教训（后续任何人再碰这个方向都要记住）**：

1. **绝对不要用`strace -f`包裹一个会daemonize、长期常驻的进程**，尤其是在同步执行的init脚本里调用它的场景。这类进程只能用`strace -p <pid>`附加到已经跑起来的进程，且必须接受"进程重启后需要重新附加"这个代价。
2. 如果确实需要"重启后自动重新附加"这种持久化效果，正确做法不是包裹二进制本身，而是**用一个开机后自动运行一次的脚本，在检测到目标进程存活后立刻`strace -p`附加**（比如挂一个短暂的开机钩子脚本轮询进程是否存在），而不是替换掉会被同步等待的可执行文件。
3. erk-agent这个wrapper手法之所以没出问题，本质原因是erk-agent的启动方式/进程模型跟ntpd不同（没有触发同样的"父进程不退出、子进程也不退出"的卡死条件），**不能想当然地认为一种进程上验证有效的手法可以直接套用到另一种进程上**，属于事实分析不够全面导致的操作事故。

## 9. 最终结论与问题状态汇总

本次排查一共定位了四个相互独立的系统时间问题，状态各不相同：

| # | 问题 | 根因 | 修复状态 | 验证情况 |
|---|---|---|---|---|
| 1 | 最初报告的"重启后变Dec2023" | `set_time_from_gps()`的`date -s`对GPS数据零校验、无条件执行 | **已修复**：年份守卫（5.6节） | 跨3次独立开机验证，`GPS ... FIRED`不再出现 |
| 2 | GPS为什么会给错误时间戳 | GPS模组固件冷启动占位时间戳，未做NMEA有效性过滤；这部分逻辑在MCU固件里，rtms_sdk不可见、不可修 | **无法在本仓库修复**，年份守卫只能间接兜底，且有已知缺口（5.7节） | 已跨2次复现坐实根因（5.3/5.4节），需转交MCU固件团队 |
| 3 | erk-agent云端时间同步污染系统钟 | Hub返回陈旧/缓存响应，原因未明，`rcpv4_cmd_on_post_time()`未加校验直接采信 | **临时挡板已加（8.4节）并经真实复现验证有效（8.7节）**，Hub为什么给旧响应这一根因仍待查，但拦截效果已确认 | 已跨2次+1次复现，链路精确到秒验证过；挡板本身已在2026-07-10的复现中实测拦截成功 |
| 4 | 系统钟在开机后极短时间内被改回"网络真实时间"，把UI手动设置的时间冲掉 | **已定位并经隔离实验坐实（见第10节）**：唯一真凶是`ntpd`（由`/etc/init.d/S49ntp`启动、带`-g`、完全不受dataengine管），它抢在开机早期联网把系统钟"纠正"回真实时间；stub-forklight随后把被改错的系统钟如实转发写脏MCU。`chronyd`经对照实验证明清白（dataengine现有的`stop_chronyd`足够快，它来不及跳钟） | **已修复（设备运行时层面）并经隔离对照实验验证**：只把`S49ntp`移出`/etc/init.d/`即可，`S49chrony`保持原始版不动 | 2026-07-10：①"只留ntpd"→污染；②"只留chronyd、禁ntpd"→完全无污染。两组对照实验分别坐实ntpd有罪、chronyd无罪 |

**问题4已定位并修复**（详见第10节）。当前状态：问题1已解决；问题2已做到Linux侧能做的极限，后续在MCU团队；问题3挡板已验证有效；问题4根因已查明（**真凶是ntpd，chronyd清白**），设备运行时已修复（仅需禁ntpd），但**修复尚未落进任何版本控制**，见第10节的落地建议。

## 10. 问题4根因定位与修复（2026-07-10 最终结论）

### 10.1 根因：真凶是ntpd（chronyd经实验证明清白）

设备上有**两个**NTP自动校时进程，开机的S49阶段都会被拉起。一开始两个都被列为嫌疑，但经过10.1a的隔离对照实验，**已坐实唯一真凶是ntpd，chronyd清白**。

**（1）ntpd —— 唯一真凶** —— 由`/etc/init.d/S49ntp`启动，命令行`/usr/sbin/ntpd -u ntp:ntp -g`。
- `-g`参数会关闭ntpd正常的"panic"合理性检查，允许开机首次同步时**无限制地大幅跳变**系统钟。
- **完全不在dataengine的管控范围内**：dataengine的`setNtpEnabled()`（SkesQtBridge.cpp:468）只调用`S49chrony`，从来没有管过ntpd。所以UI上关掉"自动校时"对ntpd毫无作用，它照样跑、照样改钟——这就是它成为真凶的直接原因：没有任何东西拦它。
- ⚠️ 代码里有一处**错误注释**需要纠正：`setNtpEnabled()`第467~470行注释写着"只用 chronyd 一个 NTP 客户端 / 已去掉重复的 ntpd"，但事实是ntpd一直由`S49ntp`独立启动、从未被去掉，正是本问题的真凶。这条注释反映的是一个与事实不符的假设。

**（2）chronyd —— 嫌疑，但经实验证明清白** —— 由`/etc/init.d/S49chrony`启动，配置文件`/etc/chrony.conf`里有`makestep 1.0 3`（前3次同步差超过1秒就直接硬跳）。这个配置理论上危险，所以一度被列为嫌疑；**但dataengine在开机极早期就会`stop`掉chronyd（`setNtpEnabled(OFF)`），这个已有逻辑足够快，chronyd根本来不及联网跳钟**。10.1a的实验直接证明了这一点：即使让chronyd开机无条件启动、只禁掉ntpd，系统钟也全程正确、没有任何污染。所以chronyd不需要额外处理，dataengine现有代码已经管住它了。

### 10.1a 隔离对照实验（2026-07-10，坐实真凶）

用"一次只放一个出来"的方式分离两个变量：

| 实验 | 条件 | 结果 | 结论 |
|---|---|---|---|
| 实验一（意外触发） | 只有ntpd活着（chronyd当时被补丁压住） | UI设的时间被改回真实时间，**污染** | ntpd单独就能作案 |
| 实验二（专门设计） | 只有chronyd会开机启动，ntpd已移出目录禁用 | UI设20:45→reboot后`date`稳定在20:45线（20:46:08…20:47:07），`ps`确认ntpd/chronyd都不在跑（chronyd被dataengine停掉了），`/var/log/messages`无chrony跳钟记录，**完全无污染** | chronyd无罪；dataengine的stop足够快 |

两组对照分别证明：**ntpd有罪且单独充分，chronyd无罪**。真凶唯一，就是ntpd。

### 10.2 为什么dataengine在应用层拦不住——开机竞态

开机脚本执行顺序（`/etc/init.d/rcS`按文件名数字顺序遍历`S??*`）：
- **S49阶段**：`S49ntp`、`S49chrony`先启动。
- **S100阶段**：`S100_skes`才启动dataengine，dataengine随后调用`setNtpEnabled(OFF)`去`stop`chronyd。

对chronyd来说，dataengine这个`stop`足够快（实测chronyd来不及跳钟就被停了，见10.1a实验二）。**问题出在ntpd身上：dataengine从头到尾只停chronyd、根本没停ntpd**，ntpd带`-g`一路裸跑，没有任何东西拦它，于是它联网后就把系统钟跳回真实时间。所以`setNtpEnabled()`这个"运行时"机制对ntpd完全无效——**修复必须放在开机/init脚本层禁掉ntpd，不是应用层能解决的**。

这也解释了之前的所有观测特征：污染发生得极快（早于用户拿到shell）、改成的是真实时间（NTP本职工作）、dataengine/io_mng/erk-agent三处已埋点函数在污染窗口内全部沉默（因为动手的是ntpd这个从没被埋点的守护进程）。

### 10.3 排查中踩的两个坑（务必记录，避免重复）

**坑1：`mv S49ntp S49ntp.disabled`这种"改后缀禁用"是无效的。**
`rcS`遍历用的glob是`/etc/init.d/S??*`——只要求"S + 2个任意字符 + 任意后续内容"，`S49ntp.disabled`这个文件名**照样匹配**，照样被`$i start`执行，而文件内容没改，等于完全没禁用。这直接导致中途"禁了ntpd问题还在→排除ntpd"的结论是**建立在错误前提上的无效结论**，白白绕了一大圈。**正确做法是用`mv`把文件移出`/etc/init.d/`目录**（移到`/root/`等），glob 就再也扫不到。

**坑2：用`strace -f`包裹ntpd二进制导致开机卡死。**（详见8.8节）`strace -f`会一直等被跟踪的daemon退出，而ntpd永不退出，同步执行的init脚本因此永远卡在`Starting ntpd:`，后面wifi/UI全部起不来，最后靠U-Boot `init=/bin/sh`救机。**教训：不要用strace包裹会daemonize的常驻进程。**

### 10.4 已实施的修复（当前仅存在于设备运行时，未进版本控制）

由10.1a实验确认chronyd无罪后，**实际只需要一处修复**：

- **禁ntpd自启**：`mv /etc/init.d/S49ntp /root/S49ntp.disabled.bak`（移出目录，非改后缀——见10.3坑1）。

**`S49chrony`保持原始版不动**（不需要之前那个`autoSetDateTime`补丁——dataengine现有的`stop_chronyd`已经管住chronyd）。排查过程中一度加过的补丁版备份为`/etc/init.d/S49chrony.patched.bak`，原始版为`/etc/init.d/S49chrony.orig.bak`，当前设备上生效的是原始版。

**验证**：仅禁ntpd、chronyd保持原始版的条件下完整复现（UI设20:45→reboot），`date`全程稳定在20:45线（20:46:08…20:47:07），MCU/系统钟均未被改回真实时间——干净通过。

### 10.5 落地建议：修复不属于dataengine代码，需要放对地方

**关键事实**：`S49ntp`是Buildroot打出来的rootfs里的init脚本，**dataengine仓库并不拥有它**（dataengine只安装一个`etc/`符号链接和应用二进制）。所以严格说这个修复**无法"提交进dataengine仓库"**，当前的运行时改动会被下次整机刷固件/刷rootfs冲掉。要持久化，有三个位置（注意：现在只需处理ntpd一件事，比之前简单）：

| 方案 | 放哪 | 优点 | 缺点 |
|---|---|---|---|
| A（最正确） | Buildroot rootfs overlay / buildroot配置：直接删/禁S49ntp | 一刷固件就是对的，最干净 | 不在SKES仓库，需掌控rootfs构建 |
| B（务实，可进SKES git） | `skes-scripts/install.sh`（OTA/安装钩子）里加：禁ntpd | 进git、随OTA生效、SKES团队自控 | 依赖install流程跑过；rootfs只读需remount处理 |
| C（补充，纠正代码错误） | dataengine应用内：修正10.1那条错误注释；可选让`setNtpEnabled`也停/禁ntpd作为双保险 | 顺带纠正与事实不符的注释 | 单靠它解决不了开机竞态（10.2），ntpd在dataengine启动前就已跳钟 |

推荐 **A或B为主（真正治本，禁ntpd）+ C顺带纠正错误注释**。A/B二选一取决于SKES团队是否掌控RK3576的Buildroot rootfs构建——这一点待确认后再落地。**本次按用户要求只更新文档，代码/脚本的版本控制落地留待后续。**

## 11. 问题2真相修正：GPS是"定位正常但日期错误"，不是"无定位给占位值"（2026-07-10 GPS_RAW打印坐实）

在 `push_location_items()`（`location.c`，GPS原始数据 memcpy 进 lcInfo 的第一现场）加了 `GPS_RAW` 诊断打印后，拿到了决定性的原始数据，**推翻了此前"GPS冷启动无定位→给占位时间戳"的猜测（5.4节的推断需按本节修正）**。

实测原始数据（多帧一致）：

```
GPS_RAW push_location_items timestamp=1701820975 (UTC 2023-12-06 00:02:55) lat=23.103991 lon=113.339008 inuse_sat=10 inview_sat=12 mode=0 antenna_valid=1 lbs=0
GPS_RAW push_location_items timestamp=1701820976 (UTC 2023-12-06 00:02:56) lat=23.103991 lon=113.339008 inuse_sat=10 inview_sat=12 mode=0 antenna_valid=1 lbs=0
```

逐字段分析：
- `inuse_sat=10 inview_sat=12`：在用10颗、可见12颗卫星，**定位质量很好**；
- `antenna_valid=1`：天线正常；
- `lat=23.10 lon=113.34`：真实有效坐标（广州一带），且逐秒微漂——是真实卫星定位，不是占位；
- `timestamp`：却是 **2023-12-06**，逐秒递增（975→976→977…）。

**结论（基于事实）：GPS模组定位完全正常、坐标真实，但它输出的日期错误地冻结在2023年12月。** 即"位置对、日期错"，而不是"没信号给占位值"。这几乎可确定是 GPS模组/MCU固件层面的日期问题（疑似 GPS 周数翻转 week-number rollover，或星历/导航电文未解码完成时用了默认日期）——**这部分逻辑在固件里，rtms_sdk 不可见、不可改，须转 MCU/GPS 固件团队**。

**一个可能有用的修复线索**：所有 GPS_RAW 行的 `mode=0` 始终不变。如果 `mode` 字段代表"定位/时间有效性"，那 `mode=0` 可能就是"时间不可信"的标志位——将来若要在Linux侧对GPS时间做有效性校验，检查 `mode`（或让固件明确一个有效性字段）会比"猜年份"更准。此点需 MCU 固件团队确认 `mode` 的确切含义。

## 12. 系统时间总模型：两个时钟 + "谁写谁"（全部经代码+日志核实）

这是理解整个问题的骨架，四个校时来源的行为差异全在这张表里。

**两个独立时钟：**
- **Linux 系统钟**（`CLOCK_REALTIME`，软件，断电即失）
- **MCU 硬件 RTC（PCF85163）**（带电池，**唯一跨重启持久化**，开机靠它恢复时间）

**谁能写这两个时钟：**

| 主体 | 写 Linux 系统钟 | 写 MCU RTC | 机制/证据 |
|---|:---:|:---:|---|
| 开机首帧 MCU_FIRST_FRAME | ✅ `date -s` | ❌（只读MCU） | `set_time_from_mcu`：开机把MCU RTC读出来恢复Linux |
| **GPS** (`set_time_from_gps`) | ✅ `date -s` | ✅ `CMD_TIME_SET` | **两个都写**：`io_mng.c:840` 调 `sync_time_from_gps()`→`cmd_settings.c:85` 生成 CMD_TIME_SET 写MCU；`io_mng.c:852` `system("date -s")` 写Linux |
| **erk-agent** | ✅ `clock_settime` | ❌ | `erk_set_clock_time()`=`clock_settime(CLOCK_REALTIME)`（`erk-utils.c:84`），只写Linux |
| ntpd | ✅ | ❌ | 只写Linux（**当前已禁用**） |
| chronyd | ✅ | ❌ | 只写Linux（dataengine 管，实验证明来不及跳） |
| **stub-forklift** | ❌（只读Linux） | ✅ `CMD_TIME_SET` | `sync_time_to_mcu()`：读Linux钟(`get_clock_time`)→发CMD_TIME_SET写MCU RTC |
| dataengine（UI设置） | ✅ `clock_settime` | ✅ `CMD_TIME_SET` | UI手动设置，两个都写 |

**由此表得出的三个关键结论：**

1. **MCU RTC（持久层）只有三个直接写入者：GPS、stub-forklift、dataengine(UI)。**
2. **GPS 是唯一会主动把"错误值"直接写进 MCU RTC 的**——它连Linux带MCU一起污染，还被持久化。日志实锤：`GPS ... FIRED`→`CMD_TIME_SET UTC=1701849649`→`MCU rtc_timestamp=...(2023-12-6)`。这是必须最优先堵死的一环。
3. **stub-forklift 是"Linux侧一切污染进入MCU的总闸门"**：erk-agent/ntpd/chronyd 本身只改Linux钟、不碰MCU，但只要Linux钟被它们改错，stub-forklift 下次周期同步就会如实读出、写进MCU RTC。stub-forklift 本身无错，只是忠实转发。

## 13. 去掉两个挡板（#1年份守卫、#3 erk-agent挡板）的裸行为验证（2026-07-10）

为观察"没有任何创可贴时"的真实行为，按需求把两处临时防御都注释掉后编译验证：
- `io_mng.c` `set_time_from_gps` 的年份守卫（#1）已注释；
- `rcpv4-cmd-time.c` 的临时挡板（#3）已注释。

**验证结果：手动时间在重启后确实丢失，两个底层问题都如实复现。** 以"UI设22:25→reboot"为例，完整污染链（逐行还原）：

| 顺序 | 日志 | 状态 |
|---|---|---|
| 1 | `MCU_FIRST_FRAME date -s "22:25:31"` | MCU与Linux都正确保住手动设的22:25 |
| 2 | `GPS ... FIRED gps_secs=1701849649`→`CMD_TIME_SET UTC=1701849649` | 守卫没了，GPS开火，把Dec2023推给MCU |
| 3 | `MCU rtc_timestamp=1701849649 (2023-12-6)`；`DATAENGINE ... wallclock=2023-12-06` | **MCU与Linux都被GPS污染成Dec2023** |
| 4 | `ERK_AGENT ... cloud_ts=2026-07-10 16:31:01 ... erk_set_clock_time_rc=0` | 挡板没了，erk-agent把Linux拉到**真实时间16:31**（不是手动的22:25!） |
| 5 | `STUB_FORKLIFT ... 16:32:00`→`CMD_TIME_SET UTC=1783672320`→`MCU ...(16:32:0)` | stub-forklift把16:32转发给MCU，MCU定格真实时间 |

**最终 `date`=16:32（真实时间），手动设的22:25彻底丢失。** 手动时间被两股力量前后夹击：GPS先砸成Dec2023（纯错误），erk-agent再拉回真实时间（时间对但不是手动值）。

**顺带修正问题3的一个前提**：8.4节那道挡板当初假设"Hub返回陈旧缓存垃圾"。但本轮及之前多次证据（如 `cloud_ts=16:31:01`/`15:48:47` 都精确等于当时真实时间）证明：**云端给的是准确的真实网络时间，不是陈旧值**。之所以被判成"早于开机基准"，纯粹是因为用户把时间设成了未来值，任何准确的真实时间自然都比这个未来基准早。所以**问题3与问题4本质同类**：erk-agent 也是一个"自动校时源"，挡板前提（陈旧响应）不成立，正确定性应为"自动校时冲掉手动时间"。

## 14. 统一修复方向（已设计，未实施）

综合11–13节的事实，正确的修复不是继续叠加各自为战的启发式挡板，而是**围绕"两类来源、一个开关"来收口**：

**（1）把"数据本身就错"的 GPS 单独管住。**
GPS 定位再好、日期也是错的（Dec2023），而开机时我们有 MCU 硬件 RTC 这个可信源，**根本不需要靠 GPS 来设系统时间**。所以正解不是"年份守卫"这种猜值，而是二选一：
- 彻底不让 GPS 写系统钟/MCU（推荐，最干净）；或
- 严格用 `mode`/有效性字段卡住（需固件先明确 `mode` 含义，见11节）。
同时把"GPS日期为何是Dec2023"（问题2）转 MCU/GPS 固件团队做 NMEA 有效性过滤。

**（2）把所有"真实网络校时源"统一收到 UI"自动同步日期时间"开关（`autoSetDateTime`）下。**
目前只有 chronyd 受该开关管，ntpd 和 erk-agent 都绕过它：

| 校时源 | 当前是否受开关管 | 应做 |
|---|:---:|---|
| chronyd | ✅ 是（dataengine启停） | 保持 |
| ntpd | ❌ 否（裸跑，真凶） | 禁用（方案甲，推荐）或按开关gate（方案乙） |
| erk-agent 云端校时 | ❌ 否（原挡板已去） | 让 `rcpv4_cmd_on_post_time` 读 `autoSetDateTime`，手动模式直接return（这才是那道挡板"该有的正式版本"） |

做到"开关OFF（手动模式）时，GPS/erk-agent/ntpd/chronyd 全部不改钟"，手动时间才能稳。**这三处（GPS受控 + erk-agent按开关gate + ntpd禁用/gate）配合，才是真正治本；单独任何一个都不彻底。**

**当前状态**：以上方向已明确，但**正式修复尚未实施**。当前设备/代码处于"裸行为验证态"——年份守卫与erk-agent挡板均已注释、ntpd运行时已禁用、chronyd原始版。待方案（尤其#4走方案甲还是乙）确认后再落地正式修复，并解决版本控制归属（见10.5）。

## 15. 推荐解决方案汇总（一站式，含首选标记与责任方）

核心设计原则一句话：**手动模式（`autoSetDateTime=0`）下，除用户外任何来源都不许改时间；自动模式（=1）下，只允许 chronyd 这一条受控的网络校时。GPS 因数据本身即错，任何模式下都不参与设时间。**

| # | 问题 | 推荐方案（★为首选） | 落地位置 | 责任方 |
|---|---|---|---|---|
| 1 | GPS 把错误时间写进 Linux+MCU RTC | ★ 彻底不让 GPS 设时间：注释掉 `set_time_from_gps` 里的 `sync_time_from_gps()`（写MCU）和 `system("date -s")`（写Linux），GPS 只用于定位。<br>备选：保留但严格按 `mode`/有效性字段+手动开关双重 gate | `rtms_sdk` io_mng：`io_mng.c set_time_from_gps` | 应用侧（本仓库可改） |
| 2 | GPS 为何给 Dec2023（定位正常、日期错） | 固件侧做 NMEA 时间有效性过滤（卫星定位有效才输出时间/日期），并排查日期错根因（疑似周数翻转/星历未解码）；同时确认 `mode` 字段是否为有效性标志 | GPS模组/MCU 固件 | **MCU/GPS 固件团队**（本仓库不可改） |
| 3 | erk-agent 云端校时覆盖手动时间 | ★ 在 `rcpv4_cmd_on_post_time()` 开头读 `autoSetDateTime`，手动模式直接 `return`（不设钟）——即那道已删挡板"该有的正式版本"，改为按开关而非按"是否早于开机基准" | `rootcloud-stp-sdk` erk-agent：`rcpv4-cmd-time.c` | 应用侧（本仓库可改） |
| 4 | ntpd 绕过 UI 开关、裸跑改钟 | ★ 方案甲：彻底禁用 ntpd（chronyd 已覆盖自动校时，ntpd 多余且冲突）。<br>备选 方案乙：保留但按 `autoSetDateTime` gate。<br>另：顺带纠正 `SkesQtBridge.cpp:467~470` 与事实不符的注释 | 方案甲/乙：rootfs 的 `S49ntp` 或 `skes-scripts/install.sh`；注释：dataengine | 应用侧 + 需定 rootfs 归属（见10.5） |

**首选组合（推荐整体采纳）：**
1. **#1 → 注释掉 GPS 设时间**（GPS 只定位不设钟，最干净，彻底断掉"错误值直接进持久层"这条最严重的链）；
2. **#3 → erk-agent 按 `autoSetDateTime` gate**（手动模式 return）；
3. **#4 → 方案甲：禁用 ntpd** + 纠正错误注释；
4. **chronyd 保持现状**（dataengine 已管住，无需动）；
5. **#2 → 转 MCU/GPS 固件团队**（应用侧做不了）。

这套组合下，四个校时来源在手动模式全部闭嘴、自动模式只走受控的 chronyd，GPS 无论何时都不设时间，用户手动设置的时间即可稳定保留至下次重启（设置值+流逝时间），满足最初需求。

**落地遗留项**：#4 的 ntpd 禁用、以及#1/#3 若要随整机固件生效，都涉及"rootfs/安装脚本归属"问题（见10.5），需确认 SKES 团队是否掌控 Buildroot 构建后再定 A/B 落点。**#1、#3 是本仓库源码可直接改并随 io_mng / erk-agent 固件发布的**。

## 16. 已知局限与待验证项（诚实边界）

第1–15节的**根因分析与推荐方案是完整、自洽的**（每条结论都有代码行号或日志实锤），但以下三点尚未闭合，明确记录以免误认为"全部已验证"：

1. **推荐方案（§15）是推理得出的，尚未经实测验证。** 目前只验证了"去掉挡板→问题复现"（§13 裸行为验证），还没有验证"按§15首选组合改完→问题解决"。方案逻辑站得住，但**实现后必须再跑一轮完整复现测试**（UI设未来时间→重启→观察是否稳定保留），才能从"合理"升级到"已验证有效"。

2. **#1 的隐含假设：MCU 硬件 RTC 在开机时总是可信。** "彻底不让 GPS 设时间"的前提，是 MCU RTC（带电池）永远能提供靠谱的开机基准。若存在"首次出厂/电池耗尽导致 MCU RTC 未初始化"的极端场景，则无兜底——但该场景下 GPS 给的 Dec2023 也无法充当有效兜底，故不改变结论，仅记录此假设。

3. **GPS 的 Dec2023 错误日期影响面不止系统钟——已确认会外泄到定位数据。**（本节新增核实）`io_mng.c:1374` 调 `get_location_json_string()`，后者在 `location.c:261` 把 GPS 的 `timestamp` 原样写入导出 JSON 的 `"time"` 字段。因此**只要 GPS 日期是错的，导出的定位数据里携带的时间就是 Dec2023**，任何消费该定位 JSON 的下游（如遥测/位置上报链路）都会拿到错误时间戳。**注意：§15 的 #1 方案（不让 GPS 设"系统钟"）并不能修复这一条**——它只切断了"GPS→系统钟/MCU RTC"，并未改变"GPS timestamp→定位JSON"的导出路径。这条同样指向问题2的根治：**GPS 日期本身必须由固件修对**，否则定位数据里的时间戳会一直是错的。下游具体哪些模块消费了该字段、错误时间戳造成何种业务影响，尚未逐一追查，是一个独立的待展开面。

## 17. 问题4 的正式落地方案（2026-07-10 实施，取代 §15 中 #4 的"禁用 ntpd"草案）

§15 对 #4 给的是"彻底禁用 ntpd、待定 rootfs 归属"的粗草案。经与设备实况反复核对，最终落地形态与草案**不同且更完整**：不是"禁用"，而是"把所有自动校时守护移出开机自启、统一交 dataengine 按 `autoSetDateTime` 独家启停"，并覆盖 chronyd、备好 PTP、做到叉车专属+无挂载竞态+增量兼容。本节为 #4 的权威落地说明。

### 17.1 逼出最终形态的硬约束（全部经设备核实）

- 设备是 buildroot + busybox/SysV init（`rcS` 只自动运行 `/etc/init.d/S??*`），**无 systemd/timedatectl**，主流的 `timedatectl set-ntp` 用不了，只能手搓等价物。
- rootfs `/`（`/dev/root`, 14G）**只读**；`install.sh` 会 `mount -o remount,rw /` 改完再 ro，是唯一能改 rootfs 的受控通道。
- **buildroot 不受控**：刷机会还原 rootfs，故任何 rootfs 改动只能靠 OTA 的 `install.sh` 反复施加。
- **`install.sh` 全车型共用**：无条件改会波及装载机/履带吊 → **必须加叉车判断**。
- flag 在 `/data/forklift_persistent.json`；`/data → /userdata`（`mmcblk0p8`，**独立分区**，fstab `pass=2` 开机会 fsck → **可能挂载慢**）。而 rootfs 上的 init 脚本始终最早可用。
- 启动顺序 **S49(ntpd/chrony) 早于 S100(dataengine)**。

### 17.2 为什么排除了几个更简单的做法（演进记录，避免回头重犯）

| 被否方案 | 致命问题 |
|---|---|
| app 层只在 dataengine(S100) `stop` | 拦不住 S49 早期抢跑；且 stub-forklift(+60s) 会把被污染的 Linux 钟固化进 MCU RTC → **跨重启仍错** |
| 开机脚本自读 flag（"方案甲"） | flag 在 `/userdata`，`pass=2` fsck 可能挂载慢，S49 运行时读不到 → 默认放行 → **漏** |
| `install.sh` 无条件改名 | 波及**全车型**，不可接受 |
| "纠正+不落库"（dataengine 停后从 RTC 重校） | 赌 S100 早于 stub +60s、还要跨模块读 RTC，**脆** |

→ 收敛为："`install.sh` 叉车分支把守护移出开机自启 + dataengine 独家管"（即"方案乙"叉车专属版）。

### 17.3 最终方案（三层协同，airtight 且无挂载竞态）

| 层 | 机制 | 覆盖 |
|---|---|---|
| **开机层**（rootfs，可靠） | `install.sh` 叉车分支把 NTP 校时守护移出 `S??` 自启命名空间（去 S 前缀），开机都不自启，**不读 /userdata** | 开机绝不抢跑校时；无挂载竞态 |
| **决策层**（S100，/userdata 已挂） | dataengine `setNtpEnabled` 按 flag 独家启停 | 重启生效 |
| **运行层** | UI 拨开关 → `setNtpEnabled` 即时 start/stop | 当次生效 |

原理：`rcS` 只跑 `S??*`，去掉 S 前缀（`S49ntp→ntpd`、`S49chrony→chronyd`）即摘除自启，脚本本体保留、dataengine 仍可 `/etc/init.d/ntpd|chronyd start|stop`。**不能用改后缀 `.disabled`**——`S49ntp.disabled` 仍匹配 `S??*` 会照样自启（已踩坑）。

### 17.4 具体改动（2 个文件）

**`skes-scripts/install.sh`**（`make_install` 的 remount rw 窗口内）：
- 叉车判据取"或"，兼顾全量与增量：①包内 dataengine profile `vehicle_type=forklift`（全量/新设备命中）；②设备存在 `/data/forklift_persistent.json`（**增量升级**时包里可能没带 profile，但存量叉车设备一定有此文件）。
- 命中则 `mv -f S49ntp→ntpd`、`S49chrony→chronyd`（幂等：已改名则源不存在即跳过）。

**`vehicles/forklift/SkesQtBridge.cpp`**：
- `ntpInitScript()/chronyInitScript()` 用 `access()` 探测：改名成功用新名，否则**回退 stock `S49xxx`** → 增量/部分升级即使改名没生效也**不硬失败**（最坏退化成"无开机门控"，运行时仍能停 NTP）。
- `setNtpEnabled(on)`：ON 起 chronyd+ntpd、OFF 停两者；`loadPersistent`（开机）与 `setSystemProp`（拨开关）都调，后者随即 `savePersistent()` 落盘 → 下次开机能读到。

### 17.5 PTP（ptp4l/phc2sys）：确认为系统钟写者，代码已备但**暂整体注释、不删除**

排查中发现 `/etc/init.d/` 里还有第三类校时服务：
- **`S66phc2sys`** 参数 `-a -r -S 1.0`：`-a` 自动模式跟随 ptp4l、`-r` **允许把系统钟 CLOCK_REALTIME 也 slave 同步**、`-S 1.0` 偏差>1s **直接 step**。→ **一旦网络出现 PTP 主时钟、ptp4l 锁定，phc2sys 就会把系统钟跳到 PTP 时间、覆盖手动**，性质与 ntpd 完全同类，是**确凿的系统钟写者**。
- **`S65ptp4l`** 只驯 PHC 硬件钟、不直接碰系统钟。
- **现状**：网络无 PTP 主源，隔离实验旁证其休眠、当前不构成威胁。
- **决定**：`install.sh` 的两条 `mv`、dataengine 的 `ptp4lInitScript/phc2sysInitScript` 两个 helper 与 `setNtpEnabled` 内 4 处启停，**已写好但整体注释掉、不删除**（两文件成对，带"PTP 暂注释"标记），待网络可能引入 PTP 源时一一放开。理由：当前无威胁，避免扩大改动面。列为**已知潜伏项**。
- 改名安全性已从脚本内容确认：ntpd/chrony/ptp4l/phc2sys 均 `DAEMON=` 硬编码、start/stop 不依赖 `$0`（chrony 用 `killall chronyd`，其余用 `start-stop-daemon`+pidfile），改名不影响功能。

### 17.6 掉电文件落点结论：保持 `/userdata`，不迁 `/mnt/sdcard`

分区关系（`df`/`fstab` 核实）：

| 挂载点 | 设备 | 性质 |
|---|---|---|
| `/`（rootfs） | `/dev/root`（板载 eMMC, 14G） | 只读，最早挂、始终在 → init 脚本改动可靠 |
| `/userdata`（`/data` 软链指向它） | `mmcblk0p8`（板载 eMMC, 221M, pass=2 fsck） | 持久数据，掉电文件在此；可能挂得慢 |
| `/oem` | `mmcblk0p7`（板载 eMMC, 123M） | OEM/出厂数据 |
| `/mnt/sdcard` | `mmcblk1p1`（**外插 SD**, 120G） | 大文件；**可插拔、可能缺席、掉电易损** |

结论：掉电文件属"必须可靠不可丢"，**应留在板载 eMMC 的 `/userdata`**；`/mnt/sdcard` 是外置可插拔、掉电易损的介质，是错误落点。挂载慢的顾虑已被本方案规避（开机不在早期读 `/userdata`，改到 S100 的 dataengine 读）。

### 17.7 与 §15 的关系、覆盖边界与待实测项

- **取代关系**：本方案取代 §15 表中 #4 的"禁用 ntpd"草案——不是禁用，而是"移出自启 + dataengine 独家管"，且覆盖 chronyd、备好 PTP、叉车专属、增量兼容。§15 其余各行（#1/#2/#3）不变。
- **覆盖边界**：本方案只解决 **#4 类（所有自动校时源抢改）**；**#1（GPS 直写 RTC）仍需 §15 的 #1 方案**。"重启后时间正确"的完整闭环仍差 #1（及 #3 erk-agent），当前设备处于"注释掉守卫/挡板"的裸验证态，未落正式修复。
- **待实测项**（实现完成、尚未上机验证）：① 改名后的 `/etc/init.d/chronyd` 实测能 start/stop（脚本已静态确认不依赖 `$0`，仍需跑一次）；② 测试前须先把测试设备的 stock `S49ntp/S49chrony` 恢复原位（当前裸验证态下 `S49ntp` 已被手动挪走，否则会误判）；③ 改完重启后 `ls /etc/init.d/ | grep -E "S49ntp|S49chrony"` 应为空、手动模式下 `ps` 里两进程都不在；④ UI 关自动同步→设未来时间→重启→时间应保留（仅验证 #4，#1 未修不代表总目标达成）。
- **上述待实测项已于 2026-07-14 全部实测通过，见 §19。**

## 18. 问题3 的正式落地方案（2026-07-14 实施，取代 §8.4 的临时挡板）

§8.4 曾加过一道"临时挡板"（基于"云端时间早于本次开机基准就跳过"的启发式），但其前提被 §13/§8.7 证伪——云端给的是**准确的真实网络时间**，之所以"早于"用户设的时间，是因为用户把时间设到了未来。所以问题3 与问题4 同类：**erk-agent 只是一个不受 UI 开关管的自动校时源**。正式修复即把它纳入 `autoSetDateTime` 门控。

### 18.1 改动

**仓库 / 文件**：`rootcloud-stp-sdk` → `erk-core/rcpv4/rcpv4-cmd-time.c`，函数 `rcpv4_cmd_on_post_time()`。

**逻辑**：在真正 `erk_set_clock_time(ts)` 之前插入门控——
- 读 `/data/forklift_persistent.json`，解析 `autoSetDateTime`；
- `=0`（手动模式）→ 写一条 `ERK_AGENT ... SKIPPED(手动模式 autoSetDateTime=0,不改系统钟)` 日志，`return -1`，**不调 `erk_set_clock_time`**；
- `=1`（自动）或文件读不到 → 照常校时。

**要点**：
- 与 #4（ntpd/chronyd）**同一个开关、同一套策略**，手动模式下所有自动校时源一致闭嘴。
- erk-agent 只写 Linux 系统钟（`clock_settime`），**不直写 MCU RTC**，故手动模式下"不写"即安全；被污染的系统钟原本要靠 stub-forklift 才落到 RTC，现在系统钟不被动，stub 转发的也就是正确值。
- **默认放行**：判据文件是叉车持久化配置，非叉车设备无此文件 → 默认校时（维持原行为）→ 对其它车型零影响。与 §17 install.sh 判据同一思路。
- 原临时挡板代码块保留为注释（不删除），其上方注释说明为何前提不成立。

## 19. 三仓库代码改动清单 + 联调验收结果（2026-07-14）

本次"手动模式重启后时间稳定保留"涉及**三个独立仓库**，各自构建、各自发布：

| 仓库 | 文件 | 改动内容 | 对应问题 | 状态 |
|---|---|---|---|---|
| **skes** | `skes-apps/skes-dataengine/vehicles/forklift/SkesQtBridge.cpp` | `setNtpEnabled()` 改为按 `autoSetDateTime` 独家启停 chronyd+ntpd；新增 `ntpInitScript()/chronyInitScript()` 用 `access()` 解析改名后/stock 路径（增量兼容）；ptp4l/phc2sys 的 helper 与启停**已写好但整体注释**（PTP 潜伏项） | #4 | ✅ 已实测通过 |
| **skes** | `skes-scripts/install.sh` | `make_install` 内新增 forklift 分支（判据：包内 `vehicle_type=forklift` **或** 设备存在 `/data/forklift_persistent.json`），把 `S49ntp→ntpd`、`S49chrony→chronyd` 移出开机自启命名空间；PTP 两条 `mv` 注释保留 | #4 | ✅ 已实测通过 |
| **rootcloud-stp-sdk** | `erk-core/rcpv4/rcpv4-cmd-time.c` | `rcpv4_cmd_on_post_time()` 内、`erk_set_clock_time` 前加 `autoSetDateTime` 门控，手动模式 `return` 不改钟；原临时挡板注释保留 | #3 | ✅ 已实测通过 |
| **rtms_sdk** | `apps/io_mng/src/{io_mng/io_mng.c, location/location.c, com/mcu_com.c, cmd/cmd.c}` | **#1 尚未修复**（按需求暂缓）。现有 io_mng 改动是**排查/裸验证手段**、非正式修复：诊断打印（GPS_RAW、`MCU_FIRST_FRAME ... FIRED`、`GPS set_time_from_gps FIRED` 等）+ **年份守卫被注释**（`// isSync = true;`，即放任 GPS 设时间以观察裸行为）。**注意：并未注释 `sync_time_from_gps`/`date -s`（那才是 #1 正式修复）**。这些改动以 **git stash** 形式保存在 `rk3576_20250821` 分支上（stash commit `63ddf94`，msg `On rk3576_20250821: IO_mng`）；当前工作树在别的开发分支、保持干净 | #1 | ⏳ 待落 |

**部署配套约束**：`install.sh` 的改名 与 `SkesQtBridge.cpp` 调改名后路径**必须同一 skes 包一起发**（dataengine 侧已用 `access()` 回退兜底增量场景）；erk-agent 随 `rootcloud-stp-sdk` 独立发布；三仓库分别构建部署。

### 19.1 联调验收结果（2026-07-14 设备实测）

| 验证项 | 方法 | 结果 |
|---|---|---|
| #4 改名生效 | 重启后 `ls /etc/init.d/`：只见 `ntpd`/`chronyd`，无 `S49ntp`/`S49chrony` | ✅ 通过 |
| #4 开机不自启 | 手动模式重启后 `ps`：无 ntpd/chronyd | ✅ 通过 |
| #4 改名脚本可用 | UI 拨开关 ON → `ps` 出现 chronyd+ntpd；OFF → 消失 | ✅ 通过（chrony 改名可用性一并确认） |
| #3 erk-agent 门控 | 手动模式日志出现 `ERK_AGENT ... SKIPPED(手动模式 autoSetDateTime=0)`，未调 `erk_set_clock_time` | ✅ 通过 |
| **端到端验收** | 手动模式设 19:35 → 重启 → 观察 | ✅ **通过：重启后 = 19:39（设置值 19:35 + 流逝），满足最初需求** |
| #1（GPS） | 本轮靠**关闭 GPS 模拟器**绕过 | ⚠️ 代码未修；真实 GPS 给错误日期时仍会污染 RTC，生产需补 §15 的 #1 方案 |

**结论**：#3、#4 从代码到设备闭环并实测通过；手动设置的时间可跨重启稳定保留为"设置值+流逝时间"。**唯一遗留是 #1（GPS 直写 RTC）尚未做代码级修复**，当前靠关模拟器规避，真机现场需补齐才算生产完整。
