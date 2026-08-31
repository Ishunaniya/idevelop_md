#!/bin/sh
#
# eg25_rtms_diag_network.sh — rtms_sdk/apps/modem_mng（EG25 平台）现场联网问题一键诊断脚本
#
# 仿写自 /home/tronlong/lyp/code/open_dial_for_artery/md/eg25_old_diag_network.sh，
# 但不是改改路径的照抄——本仓库 EG25 虽然是对齐 open_dial_for_artery 移植的，仍有
# 若干实现细节不同（可执行文件名、日志落盘方式、cereg 心跳字段、连通性探测方式、
# tcp_fail_count 语义、ICCID 匹配规则、看门狗机制等），本脚本按本仓库当前源码重写，
# 逐项依据见每一节内的注释（文件:行号）。
#
# ==================== 用法 ====================
#   把这个文件拷到设备上任意可写目录（如 /usrdata/），然后执行：
#     sh eg25_rtms_diag_network.sh          # 普通模式
#     sh eg25_rtms_diag_network.sh -v       # 详细模式：额外打印 dial_status 原始全文、
#                                       # 最近日志片段等
#
#   全程只读（不改任何文件、不重启进程、不发 AT 指令），可以随时在正在跑的设备上
#   执行，不影响 modem_mng 运行。用到的 nc/ip/python3/ping 等工具缺失时会自动跳过
#   对应检查并提示，不会报错中断。
#
# ==================== 怎么看输出 ====================
#   每行前缀代表判定结果：
#     [OK]    正常，不用管
#     [WARN]  有异常，但未必是当前故障的根因，或处于"临界"状态，值得留意
#     [FAIL]  明确问题，建议优先处理
#     [--]    纯信息展示，不参与判定
#
#   跑完最后会打印一行 PASS=x WARN=y FAIL=z 汇总和一句话结论。
#   脚本进程本身退出码固定是 0，不代表设备网络健康，只看 PASS/WARN/FAIL 那行。
#
#   章节编号（0~8）建议排查顺序：第0节信号/驻网最基础；越往后越偏向 modem_mng
#   自身状态机/软件逻辑层面。第0节就有 [FAIL] 时通常不用急着往后看软件层面。
#
# ==================== 设计原则 ====================
#   1. 只读，不写。不修改 /tmp/network_*、不重启进程、不发 AT 指令。
#      -- AT 口 /dev/smd8 由 modem_mng 进程内部用 g_at_port_mutex 保护
#         （eg25/nw/nw.c、eg25/dial/dial.c 多处 pthread_mutex_lock(&g_at_port_mutex)），
#         这把锁只在 modem_mng 进程内有效，外部脚本并发发 AT 指令拿不到锁，
#         只会和 modem_mng 抢串口，可能造成状态机误判超时。本脚本完全不碰 /dev/smd8，
#         只读 modem_mng 已经写出来的状态文件/日志。
#   2. 目标是 busybox ash，不用 bash 专有语法，工具用之前先 command -v 探测。
#   3. 每一项检查的判据都来自本仓库 EG25 源码里的真实行为（状态机、常量、文件格式），
#      不是通用网络排查套路，也不是从 open_dial_for_artery 直接照搬的假设。
#

DIAL_BIN_NAME="modem_mng"               # CMakeLists.txt:2 project(modem_mng)，三平台共用同一可执行文件名
DIAL_STATUS="/tmp/dial_status"          # status/dial_status.c，INI 分节格式，但 grep '^key=' 不受分节头影响
NET_STATUS="/tmp/network_status"        # eg25/nw/nw.h:94
NET_TYPE="/tmp/network_type"            # roamlink/roamlink.h:45  0=切换中 1=物理SIM 2=Roamlink
NET_CSQ="/tmp/network_csq"              # eg25/nw/nw.h:95
NET_PLMN="/tmp/network_plmn"            # eg25/nw/nw.h:96 —— 见第4节，写入函数 nw_mark_plmn() 全仓库零调用，已知死代码
NETWORK_INI="/usrdata/network.ini"      # roamlink/roamlink.h:50，节名/键名 network:network_select
APN_JSON="/usr/dial/apn.json"           # eg25/apn/apn.c:362
CONF_INI="/opt/conf.ini"                # roamlink/roamlink.h:36
LICENSE_MAIN="/usrdata/roamlink/etc/.pconfig/license.cer"   # roamlink/roamlink.h:27
LICENSE_BACKUP="/data/ufs/license.cer"                      # roamlink/roamlink.h:28
RBMASTER_BIN="/usrdata/roamlink/RBMaster"                   # roamlink/roamlink.h:12
LICENSE_WAIT_TIMEOUT_SEC=300                                # roamlink/roamlink.h:76
SDCARD_DIR="/media/sdcard"
LOG_ROOT="/media/sdcard/dial_log"       # logger_sd.c:26，按天分目录 YYYY-MM-DD/dial_HHMMSS.log，时钟未同步落 unsynced/
PROGRAMS_STARTUP_LOG="/tmp/programs_startup"  # apps/sw_mng program_launcher.hpp:160，sw_mng 拉起/发现程序的记录
DIAL_STATUS_STALE_SEC=60                 # 依据：dialer_eg25.c:47 modem_mng 向 sw_mng 注册的心跳超时正好是 60s
                                          # （cpactive_add_pinfo(cpactive, 60, ...)），dial.c:485,531 靠
                                          # cpactive_upt_atime() 刷新；用同一个数字判"状态机是否卡死"有代码依据

# 连通性探测：modem_mng 自身用的就是 ICMP ping，不是 TCP —— eg25/dial/dial.c:317-319
#   bool test_can_ping_google() { system("ping -c 1 8.8.8.8 > /dev/null"); }
# 所以本脚本以 ping 8.8.8.8 为主判据，TCP 仅作为"ping不通时看是不是ICMP被限速/丢弃"的补充三角验证，
# 这和参考脚本"TCP为主、ICMP为辅"的顺序是反的——因为两边代码里实际担任连通性判据的协议不同。
PING_TEST_HOST="8.8.8.8"
ALT_TCP_HOST1="223.5.5.5"   # 阿里 DNS
ALT_TCP_PORT1="443"
ALT_TCP_HOST2="1.1.1.1"     # Cloudflare DNS，和 ping 目标(8.8.8.8)分属不同网络路径，避免单点误判
ALT_TCP_PORT2="443"

VERBOSE=0
[ "$1" = "-v" ] && VERBOSE=1

PASS=0
WARN=0
FAIL=0

hr() { printf -- '----------------------------------------------------------------\n'; }
sec() { printf '\n=== %s ===\n' "$1"; }
ok()   { PASS=$((PASS+1)); printf '[OK]   %s\n' "$1"; }
warn() { WARN=$((WARN+1)); printf '[WARN] %s\n' "$1"; }
fail() { FAIL=$((FAIL+1)); printf '[FAIL] %s\n' "$1"; }
info() { printf '[--]   %s\n' "$1"; }

have() { command -v "$1" >/dev/null 2>&1; }

now_epoch() { date +%s 2>/dev/null; }

mtime_epoch() {
    f="$1"
    if have stat; then
        stat -c %Y "$f" 2>/dev/null && return
    fi
    find "$f" -printf '%T@\n' 2>/dev/null | cut -d. -f1
}

age_of() {
    f="$1"
    [ -e "$f" ] || { echo -1; return; }
    m=$(mtime_epoch "$f")
    n=$(now_epoch)
    [ -n "$m" ] && [ -n "$n" ] && echo $((n - m)) || echo -1
}

# 进程探测提前到这里：日志文件定位要用当前进程的 fd（见 find_log_by_proc），
# 第1节直接复用 $dial_pids/$dial_cnt，不重复 pgrep。
# 注意：本仓库三个平台（EC200A/EG25G/IMX6）编出来的可执行文件名都叫 modem_mng
# （CMakeLists.txt:2 project(modem_mng)），不叫 "dial"。
dial_pids=$(pgrep -f "$DIAL_BIN_NAME" 2>/dev/null)
dial_cnt=$(echo "$dial_pids" | grep -c '[0-9]')

# 【权威来源】当前 modem_mng 进程正打开着的日志文件。
# logger_sd.c 全程持有 g_log_fp 不关（只在跨天/滚动时 fclose+重开），所以扫
# /proc/<pid>/fd 里指向 dial_log 下 dial_*.log 的那个符号链接，拿到的一定是
# "这个进程此刻正在写的那个文件"，不受任何时钟问题影响。
find_log_by_proc() {
    [ -n "$dial_pids" ] || return 1
    for p in $dial_pids; do
        for fd in /proc/"$p"/fd/*; do
            t=""
            if have readlink; then
                t=$(readlink "$fd" 2>/dev/null)
            else
                t=$(ls -l "$fd" 2>/dev/null | sed -n 's|.*-> ||p')
            fi
            case "$t" in
                "$LOG_ROOT"/*dial_*.log) echo "$t"; return 0 ;;
            esac
        done
    done
    return 1
}

# 兜底：进程没跑/没权限读 /proc 时，按目录+mtime 猜。
# 【必须知道的坑】mtime 排序在时钟未同步时是不可信的：unsynced/ 目录里堆的是
# 历次开机的日志（logger_sd.c:395 对 unsynced/ 只按数量保留最新 N 个，不套天数规则），
# 而每次重启时钟都从 1970 重新走，上一个 boot 跑到 08:57 写出来的文件 mtime 就比
# 本次 boot 刚开机 00:01 写的文件"更新"，ls -t 会把**旧 boot 的日志**排在最前面。
# 真机上已经踩到过：脚本把上一个 boot 的 CEREG/COPS/FW/policy/rx_packets 当成
# "最近一次"报了出来。所以这条路径只作兜底，且命中 unsynced/ 时必须显式告警。
find_latest_log() {
    today=$(date +%Y-%m-%d 2>/dev/null)
    for d in "$LOG_ROOT/$today" "$LOG_ROOT/unsynced"; do
        if [ -d "$d" ]; then
            f=$(ls -t "$d"/dial_*.log 2>/dev/null | head -1)
            [ -n "$f" ] && { echo "$f"; return; }
        fi
    done
    find "$LOG_ROOT" -name 'dial_*.log' 2>/dev/null | while read -r p; do
        m=$(mtime_epoch "$p"); echo "$m $p"
    done | sort -rn 2>/dev/null | head -1 | cut -d' ' -f2-
}

hr
echo "modem_mng (EG25) 联网问题诊断脚本  $(date '+%Y-%m-%d %H:%M:%S')"
hr

# 定位日志文件：先问当前进程它正开着哪个（权威），拿不到再退回 mtime 猜测。
LOG_SRC=""
LATEST_LOG=$(find_log_by_proc)
if [ -n "$LATEST_LOG" ]; then
    LOG_SRC="proc"
else
    LATEST_LOG=$(find_latest_log)
    [ -n "$LATEST_LOG" ] && LOG_SRC="mtime"
fi

# 版本探测：三个来源按可靠性从高到低依次尝试。
#   1. /tmp/dial_version——当前进程启动时自己写的（dialer_eg25.c:83，EG25 v1.31.12 起，
#      commit 4cbca24e）。它描述的一定是**此刻在跑的这个进程**的版本，这正是现场要问的
#      问题。原先把它排在日志行之后是错的：日志行可能来自上一个 boot（见 find_latest_log
#      里的 mtime 坑），刚升级过的设备会被报成升级前的旧版本。
#   2. 日志里的启动行——dial_log("EG25 modem_mng Version: %s\n", pversion) 从 commit
#      dadb2c53 起就存在，是唯一不受后续功能门控的来源，用于 1.31.12 之前的老固件兜底；
#      但它的可信度取决于日志文件是不是当前 boot 的，所以排第二。
#   3. /tmp/dial_status 的 version= 字段——见下方版本兼容性说明，这是随 Roamlink
#      功能才引入的（commit 3bc83208），在这三个来源里反而是最不可靠的一个，因为
#      它存在与否本身就是本脚本要判断的"版本是否够新"的问题之一，用它反推版本
#      属于循环论证，只做最后兜底。
dial_ver=""
dial_ver_src=""
if [ -f /tmp/dial_version ]; then
    dial_ver=$(sed -n 's/^Version: *//p' /tmp/dial_version 2>/dev/null | head -1 | tr -d '\r')
    [ -n "$dial_ver" ] && dial_ver_src="/tmp/dial_version（当前进程写的）"
fi
if [ -z "$dial_ver" ] && [ -n "$LATEST_LOG" ] && [ -f "$LATEST_LOG" ]; then
    dial_ver=$(grep -m1 'modem_mng Version:' "$LATEST_LOG" 2>/dev/null | sed -n 's/.*Version: *//p' | tr -d '\r')
    if [ -n "$dial_ver" ]; then
        if [ "$LOG_SRC" = "proc" ]; then
            dial_ver_src="日志启动行（当前进程正在写的日志）"
        else
            dial_ver_src="日志启动行（该日志未必属于当前 boot，见下方告警）"
        fi
    fi
fi
if [ -z "$dial_ver" ] && [ -f "$DIAL_STATUS" ]; then
    dial_ver=$(grep '^version=' "$DIAL_STATUS" 2>/dev/null | head -1 | cut -d= -f2)
    [ -n "$dial_ver" ] && dial_ver_src="dial_status（最不可靠，见上方注释）"
fi
if [ -n "$dial_ver" ]; then
    info "当前 modem_mng 版本: $dial_ver  [来源: $dial_ver_src]"
else
    warn "三个版本来源（/tmp/dial_version / 当前日志 / dial_status）都没取到版本号——如果进程确实在跑，这本身就值得留意；如果进程没起来，见第1节"
fi

# --- 日志文件可信度告警：这一条直接决定第0节/第8节的结论能不能信 ---
if [ -z "$LATEST_LOG" ]; then
    :
elif [ "$LOG_SRC" = "proc" ]; then
    info "日志文件取自当前进程的 /proc/<pid>/fd，确定属于本次运行"
else
    case "$LATEST_LOG" in
        */unsynced/*)
            warn "只能按 mtime 猜日志文件，且命中的是 unsynced/ 目录（$LATEST_LOG）——该目录堆着历次开机的日志，时钟每次重启都从1970重走，mtime 排序会把**上一个 boot 的日志**排到最前。第0节的 CEREG/COPS/FW/policy/心跳 和第8节的 RECOVERY 结论都可能是上一次开机的，不要直接采信；请用第6节 live rx_packets 与心跳里的 rx_packets 对照（计数器只增不减，若 live 值反而更小，就坐实了日志是旧 boot 的）"
            ;;
        *)
            info "日志文件按 mtime 定位（$LATEST_LOG）——进程未运行或 /proc 不可读，无法确认它属于本次运行"
            ;;
    esac
fi

# ------------------------------------------------------------------
# 版本兼容性说明（务必先看）：本脚本是按本仓库 EG25 当前 HEAD（1.31.15，见 eg25/dial/dial.h
# 的 MODEM_MNG_VERSION_MAIN/SUB/PATCH 宏）的行为写的，
# 但下面几项能力是逐步引入的，现场老固件不一定有——缺失不代表故障，是版本问题：
#   - /tmp/dial_status（第3节全部内容的基础）是随 Roamlink 双卡切换功能一起引入的
#     （commit 3bc83208 [NEWFUNC] 新增 Roamlink 双卡切换逻辑），在此之前的固件根本
#     没有这个文件，第3节会一直报 [FAIL] 文件不存在，那不是状态机卡死，是版本太老。
#   - [HEARTBEAT] 扩展行的 cereg=/ifname=/rx_packets= 字段、[INIT] FW:/[INIT] policy=
#     是在 1.31.4（commit a39d81de，日志对齐 open_dial_for_artery）才定型的；再往前
#     的 bf0ec6d3 只有基础 [MODEM]/[INIT]/[CELL]/[SDK] 标签，没有这些扩展字段。
#   - [RECOVERY L1]/[RECOVERY L2]/[RECOVERY L3] 标签是 e5d22d46 起陆续加入、到 1.31.5
#     （commit b1bdc423，分级恢复改纯 downtime 驱动）才形成当前这套完整格式。
# 上面已经拿到版本号的话，可以直接对着这几个门槛版本号比较，不必再靠"文件在不在"
# 去猜；拿不到版本号时，第3/0/8节才退回"关键文件/标签存不存在"这种粗略能力探测，
# 命中缺失时按"版本可能太老"降级为 WARN/INFO，不直接判 FAIL。
# ------------------------------------------------------------------

# ------------------------------------------------------------------
sec "0. 无线层基础信息（信号 / 驻网）—— 现场排查第一优先级"
# ------------------------------------------------------------------
# 本节不发任何 AT 指令，全部读取 modem_mng 已经采集并落盘/落日志的结果。

# --- CSQ：eg25/nw/nw.c nw_at_get_csq() 每次拨号循环都会查，但只在值变化时才写文件
#     （pre_csq != csq 去重，见 nw.c:132），所以"很久没更新"不能当成"主线程卡死"的证据——
#     信号长期稳定时文件本就不会动，这一点和很多同类脚本的假设不同，务必注意。
if [ -f "$NET_CSQ" ]; then
    csq_val=$(cat "$NET_CSQ" 2>/dev/null)
    case "$csq_val" in
        ''|*[!0-9]*)
            fail "CSQ 内容异常（原始值: '$csq_val'，非纯数字）—— 可能读到了写入中途的半截内容，重新跑一次确认"
            ;;
        99)
            fail "CSQ=99 无效/未知 —— 模组读不到信号强度，优先检查天线/模组本身"
            ;;
        *)
            dbm=$((2 * csq_val - 113))
            if [ "$csq_val" -lt 10 ]; then
                fail "CSQ=$csq_val（约 ${dbm}dBm）信号弱 —— 先查天线安装/位置/运营商覆盖"
            elif [ "$csq_val" -lt 15 ]; then
                warn "CSQ=$csq_val（约 ${dbm}dBm）信号偏弱，可用但不稳"
            else
                ok "CSQ=$csq_val（约 ${dbm}dBm）信号正常"
            fi
            ;;
    esac
    info "该文件只在 CSQ 数值发生变化时才会被重写（nw.c:132 去重），文件长时间不更新不代表主线程卡死"
else
    fail "$NET_CSQ 不存在 —— modem_mng 从未成功查过 CSQ，可能主线程卡死或进程未启动"
fi

# $LATEST_LOG 已在脚本顶部版本探测处求过值，这里直接复用，不重复调用 find_latest_log
if [ -n "$LATEST_LOG" ] && [ -f "$LATEST_LOG" ]; then
    info "当前使用的日志文件: $LATEST_LOG"

    # --- 日志内容时间 vs 当前时钟：日志行首是 [YYYY-MM-DD HH:MM:SS]（logger_sd.c dial_log 格式）。
    #     若最后一行的时间比"现在"还晚，那这个文件不可能是本次 boot 写的——时钟未同步时
    #     每次开机都从 1970 重走，只有上一个 boot 才会留下"未来"的时间戳。这是判定
    #     "日志属于旧 boot"最直接的证据，比 mtime 可靠。
    last_log_ts=$(tail -20 "$LATEST_LOG" 2>/dev/null | sed -n 's/^\[\([0-9-]* [0-9:]*\)\].*/\1/p' | tail -1)
    if [ -n "$last_log_ts" ]; then
        now_ts=$(date '+%Y-%m-%d %H:%M:%S' 2>/dev/null)
        # 字符串比较即可：两者都是零填充的 "YYYY-MM-DD HH:MM:SS"，字典序等价于时间序
        if [ -n "$now_ts" ] && [ "$last_log_ts" \> "$now_ts" ]; then
            warn "日志最后一条记录的时间是 $last_log_ts，比当前系统时间 $now_ts 还晚 —— 这个文件不可能是本次开机写的，几乎可以断定来自上一个 boot（时钟未同步，每次重启从1970重走）。本节下面的 CEREG/COPS/FW/policy/心跳 全部是**上一次开机**的状态，不要当成现状"
        fi
    fi

    # --- 驻网状态：[HEARTBEAT] 扩展心跳每 5min 追加一次 cereg=0/1（eg25/diag/diag.c:148-153）。
    #     注意 cereg 这里已经是布尔结果（eg25/nw/nw.c:460-481 nw_at_get_cereg_stat 返回 bool，
    #     stat_cereg==1或5 才返回 true），不是原始 AT+CEREG 状态码；0 既可能是"未注册"也可能是
    #     "AT+CEREG 查询本身超时/解析失败"，两者在这个字段上无法区分。
    last_hb_ext=$(grep 'cereg=' "$LATEST_LOG" 2>/dev/null | tail -1)
    if [ -n "$last_hb_ext" ]; then
        info "最近一次扩展心跳（每5min一条）: $last_hb_ext"
        cereg_bit=$(echo "$last_hb_ext" | sed -n 's/.*cereg=\([0-9]\).*/\1/p')
        if [ "$cereg_bit" = "1" ]; then
            ok "CEREG 已注册（AT+CEREG stat=1 home 或 5 roaming）"
        else
            fail "CEREG 未注册，或 AT+CEREG 查询失败 —— 拨号即使成功也无法真正用网，优先于任何通道切换/APN排查"
        fi
    else
        info "当前日志中暂无扩展心跳记录（进程启动不足5min、从未联网触发过该分支，或固件版本早于1.31.4没有这个扩展字段——见上方版本兼容性说明），跳过驻网信息"
    fi

    last_init_cops=$(grep '\[INIT\] COPS:' "$LATEST_LOG" 2>/dev/null | tail -1)
    [ -n "$last_init_cops" ] && info "最近一次开机 COPS 快照: $last_init_cops （+COPS: 0,... = 自动选网；+COPS: 1,... = 手动锁网，可能是上次异常切换残留的锁定状态）"
    last_init_fw=$(grep '\[INIT\] FW:' "$LATEST_LOG" 2>/dev/null | tail -1)
    [ -n "$last_init_fw" ] && info "最近一次开机固件版本: $last_init_fw"
    last_init_policy=$(grep '\[INIT\] policy=' "$LATEST_LOG" 2>/dev/null | tail -1)
    [ -n "$last_init_policy" ] && info "开机时读取到的策略: $last_init_policy"
else
    info "$LOG_ROOT 下未找到当前日志文件（可能 SD 卡未挂载/空间不足，见第8节），跳过驻网/COPS 信息"
fi

# ------------------------------------------------------------------
sec "1. 进程存活"
# ------------------------------------------------------------------
# $dial_pids/$dial_cnt 已在脚本顶部（日志定位处）求过值，这里直接复用，不重复 pgrep。
if [ -z "$dial_pids" ]; then
    fail "modem_mng 进程未运行 —— 联网功能完全停摆，等 sw_mng 检测到进程消失后才会拉起（见下方看门狗说明，可能有延迟）"
elif [ "$dial_cnt" -gt 1 ]; then
    warn "检测到 $dial_cnt 个 modem_mng 进程同时存在 (pid: $(echo "$dial_pids" | tr '\n' ' ')) —— 正常应只有 1 个，多个实例会抢 /dev/smd8 和 /tmp/dial_status"
else
    ok "modem_mng 进程正常运行 (pid=$dial_pids)"
fi

rb_pids=$(pgrep -f "RBMaster" 2>/dev/null)
rb_cnt=$(echo "$rb_pids" | grep -c '[0-9]')
if [ -n "$rb_pids" ] && [ "$rb_cnt" -gt 1 ]; then
    warn "检测到 $rb_cnt 个 RBMaster 进程 (pid: $(echo "$rb_pids" | tr '\n' ' ')) —— 正常应≤1个"
elif [ -n "$rb_pids" ]; then
    info "RBMaster 运行中 (pid=$rb_pids)，是否应该运行取决于当前策略/通道，见下文 dial_status"
else
    info "RBMaster 未运行（若当前应处于 roamlink 通道，这本身就是异常，见下文 dial_status 交叉核对）"
fi

# 本仓库没有 open_dial_for_artery 里的 check_network.sh 看门狗脚本；实际拉起/监控
# modem_mng 的是同一 SDK 里的 sw_mng 进程（apps/sw_mng），机制和 check_network.sh 不同，
# 务必注意它的局限：
#   sw_mng/src/main.cpp:205-233 —— 每轮轮询共享内存里各进程的 atime，超过其注册时的
#   timeout（modem_mng 是 60s，见 dialer_eg25.c:47）只打一行"已经超时"日志，随后做的
#   kill(pid,0) 只是存在性探测；只有进程真的退出了才会被拉起，dial_task 线程卡死但
#   进程还在时，sw_mng 不会做任何事。也就是说"modem_mng 进程存在"不等于"状态机没卡死"，
#   要看第3节 dial_status 的更新时间。
if pgrep -f "sw_mng" >/dev/null 2>&1; then
    ok "sw_mng 看门狗运行中（注意：它只在 modem_mng 进程真正退出后才会拉起，进程卡死但存活时不会介入，见上方注释）"
else
    warn "sw_mng 未检测到 —— modem_mng 一旦崩溃退出将不会被自动拉起"
fi
if [ -f "$PROGRAMS_STARTUP_LOG" ]; then
    restart_cnt=$(grep -c "modem_mng.*launched" "$PROGRAMS_STARTUP_LOG" 2>/dev/null)
    if [ "$restart_cnt" -gt 3 ] 2>/dev/null; then
        warn "$PROGRAMS_STARTUP_LOG 中 modem_mng 的启动记录有 $restart_cnt 条 —— 若短时间内多次出现，说明进程在崩溃重启循环，需要结合 $LOG_ROOT 下的日志找崩溃点"
    fi
fi

# ------------------------------------------------------------------
sec "2. 配置文件完整性（policy / apn / roamlink 前置依赖）"
# ------------------------------------------------------------------

if [ -f "$NETWORK_INI" ]; then
    # 取最后一条匹配：iniparser_load()/iniparser_getint()（roamlink.c:243）在文件里
    # 出现重复 key 时是后面的覆盖前面的字典语义，这里用 tail -1 对齐这个行为，
    # 不能不加限制地让 grep -o 把所有重复行都吐出来（那样 $sel 会变成多行内容，
    # 后面的 case 匹配不上任何一个值，被误判成"非法值"）。
    sel=$(grep -o 'network_select *= *[0-9]*' "$NETWORK_INI" 2>/dev/null | grep -o '[0-9]*$' | tail -1)
    case "$sel" in
        1) name="PREFER_ROAMLINK";;
        2) name="PREFER_SIM";;
        3) name="FORCE_ROAMLINK";;
        4) name="FORCE_SIM";;
        *) name="";;
    esac
    if [ -n "$name" ]; then
        ok "$NETWORK_INI: network_select=$sel ($name)"
    else
        warn "$NETWORK_INI: network_select 值非法或缺失（原始值: '$sel'）—— roamlink_read_policy() 读取失败会退回默认策略 4 (FORCE_SIM)，注意这是仅在启动时读取一次（roamlink/roamlink.c:75 dial.c:75），改配置需重启 modem_mng 才生效"
    fi
else
    warn "$NETWORK_INI 不存在 —— roamlink_read_policy() 将使用默认策略 4 (FORCE_SIM)（roamlink/roamlink.c:225-231）"
fi

if [ -f "$APN_JSON" ]; then
    if have python3; then
        if python3 -c "import json,sys; json.load(open('$APN_JSON'))" >/dev/null 2>&1; then
            ok "$APN_JSON 存在且 JSON 格式合法"
        else
            fail "$APN_JSON 存在但 JSON 解析失败 —— apn_get_apn_obj() 匹配不到会退化为空 APN 默认对象，拨号大概率失败"
        fi
    else
        if grep -q '"apn"' "$APN_JSON" 2>/dev/null; then
            ok "$APN_JSON 存在（未装 python3，跳过严格 JSON 校验）"
        else
            warn "$APN_JSON 内容可疑，未找到 apn 字段"
        fi
    fi
else
    fail "$APN_JSON 不存在 —— 所有 ICCID 都匹配不到 APN，SIM 通道无法正常拨号"
fi

if [ -f "$CONF_INI" ]; then
    ok "$CONF_INI 存在（RBMaster 前置依赖满足）"
else
    info "$CONF_INI 不存在 —— roamlink_probe() 返回 CONF_MISSING（roamlink.c:52），本次运行永久降级 FORCE_SIM（若设备本就不用 roamlink 可忽略）"
fi

if [ -x "$RBMASTER_BIN" ]; then
    if [ -s "$LICENSE_MAIN" ]; then
        ok "license 主文件存在且非空: $LICENSE_MAIN"
    else
        if [ -s "$LICENSE_BACKUP" ]; then
            warn "license 主文件缺失/为空，但备份存在 ($LICENSE_BACKUP) —— modem_mng 启动时会自动尝试恢复（roamlink.c 备份恢复逻辑）；若日志里没看到恢复成功，检查文件权限"
        else
            warn "license 主/备份均缺失或为空 —— roamlink_probe()=LICENSE_MISSING，策略1/2会进入 license_pending 等待下载（超时 ${LICENSE_WAIT_TIMEOUT_SEC}s），策略3(FORCE_ROAMLINK)会停在不可用状态等待人工介入（不会降级为FORCE_SIM）"
        fi
    fi
else
    info "RBMaster 二进制不存在或不可执行 ($RBMASTER_BIN) —— 本设备无 roamlink 能力，本节后续检查可忽略"
fi

# ------------------------------------------------------------------
sec "3. modem_mng 内部状态 (/tmp/dial_status)"
# ------------------------------------------------------------------

if [ ! -f "$DIAL_STATUS" ]; then
    if [ "$dial_cnt" -gt 0 ] 2>/dev/null; then
        warn "$DIAL_STATUS 不存在，但 modem_mng 进程在跑 —— 两种可能：①固件版本早于 Roamlink 功能引入点(commit 3bc83208)，本来就没有这个文件，不代表故障，请核对烧录版本；②当前版本确实有但状态机线程从未跑到 dial_status_write（刚启动，或已卡死）。本脚本无法仅凭这一项区分，请结合能否 ping 通、rmnet 接口是否有 IP（第5/7节）综合判断"
    else
        fail "$DIAL_STATUS 不存在，且 modem_mng 进程也未运行 —— 联网功能完全停摆"
    fi
else
    age=$(age_of "$DIAL_STATUS")
    if [ "$age" -gt "$DIAL_STATUS_STALE_SEC" ]; then
        fail "$DIAL_STATUS 已 ${age}s 未更新（阈值 ${DIAL_STATUS_STALE_SEC}s，与 modem_mng 向 sw_mng 注册的心跳超时一致）—— 状态机大概率卡死；注意 sw_mng 不会因此杀掉/重启它（见第1节），进程存在不代表业务在跑"
    else
        ok "$DIAL_STATUS 更新于 ${age}s 前，状态机未卡死"
    fi

    getval() { grep "^$1=" "$DIAL_STATUS" 2>/dev/null | head -1 | cut -d= -f2-; }
    # status/dial_status.c 里 [dial] 和 [roamlink] 两个分节都无条件写了同名的 "state="
    # 键（status/dial_status.c:147,189），plain getval() 靠 head -1 蒙对了 [dial] 那个
    # 纯属文件里 [dial] 分节先写、算是巧合，[roamlink] 分节的 state（starting/active）
    # 用 getval 永远拿不到。用 awk 按分节精确取值，两个字段名相同但需要区分来源时用它。
    getval_section() {
        awk -v sec="[$1]" -v key="$2=" '
            $0 == sec { insec=1; next }
            /^\[/ { insec=0 }
            insec && index($0, key) == 1 { print substr($0, length(key)+1); exit }
        ' "$DIAL_STATUS" 2>/dev/null
    }

    st_state=$(getval_section dial state)
    st_policy=$(getval policy)
    st_policy_name=$(getval policy_name)
    st_channel=$(getval channel)
    st_lic_pending=$(getval license_pending)
    st_lic_wait=$(getval license_wait_sec)
    st_iccid=$(getval iccid)
    st_csq=$(getval csq)
    st_apn=$(getval apn)
    st_imsi=$(getval imsi)
    st_profile=$(getval profile_idx)
    st_plmn=$(getval plmn)
    st_net_status=$(getval status)
    st_net_type=$(getval type)
    st_ip=$(getval ip)
    st_ifname=$(getval ifname)
    st_tcp_fail=$(getval tcp_fail_count)
    st_rl_fail=$(getval roamlink_fail_count)
    st_outage_cnt=$(getval outage_count)
    st_cur_outage=$(getval current_outage_sec)
    st_last_outage=$(getval last_outage_sec)
    st_total_outage=$(getval total_outage_sec)
    st_rbpid=$(getval rbmaster_pid)
    # [roamlink] 分节独有字段：之前版本漏取了这三个，调 roamlink_starting 卡住的现场
    # 时最需要看的恰恰是这几个（等了多久、业务层多久没新数据）
    st_rl_state=$(getval_section roamlink state)
    st_biz_no_data=$(getval_section roamlink biz_no_data_sec)
    st_rl_wait=$(getval_section roamlink roamlink_connect_wait_sec)

    info "state=$st_state  channel=$st_channel  policy=$st_policy($st_policy_name)"
    if [ -n "$st_rl_state" ] && [ "$st_rl_state" != "none" ]; then
        info "roamlink.state=$st_rl_state（starting=正在连接虚拟SIM / active=已激活）"
        [ -n "$st_rl_wait" ] && [ "$st_rl_wait" -gt 0 ] 2>/dev/null && \
            warn "roamlink_connect_wait_sec=${st_rl_wait}s —— roamlink_starting 已等待这么久，若持续增长且迟迟不到 active，参考 roamlink/roamlink.h 里的连接超时常量核实是否该切回SIM"
        [ -n "$st_biz_no_data" ] && [ "$st_biz_no_data" -gt 0 ] 2>/dev/null && \
            warn "biz_no_data_sec=${st_biz_no_data}s —— 业务层已经这么久没有新数据包了（ROAMLINK_NO_DATA_TIMEOUT 相关判死依据之一）"
    fi

    if [ -n "$sel" ] && [ -n "$st_policy" ] && [ "$sel" != "$st_policy" ]; then
        warn "network.ini 当前 network_select=$sel，但 dial_status 里 policy=$st_policy —— 说明改配置后 modem_mng 还没重启，仍按旧策略跑"
    fi

    case "$st_net_status" in
        1) ok "network.status=1 (已联网)";;
        0) fail "network.status=0 (未联网) —— 当前处于断网状态";;
        *) warn "network.status 字段读取异常: '$st_net_status'";;
    esac

    case "$st_net_type" in
        1) info "network.type=1 (物理SIM通道)";;
        2) info "network.type=2 (Roamlink虚拟SIM通道)";;
        0) warn "network.type=0 (切换中) —— 若长时间停留在此值，说明卡在通道切换过程中出不来";;
        *) warn "network.type 字段异常: '$st_net_type'";;
    esac

    # dial_status 自己记的 ip/ifname/rbmaster_pid：和第5节 `ip addr` 现查的、第1节
    # pgrep 现查的是两个独立来源，可能出现"dial_status 还停留在旧值，live 已经变了"
    # 这种滞后——之前这三个字段只取了值但从没显示/比对过，白取了。
    if [ "$st_net_type" = "1" ]; then
        [ -n "$st_ip" ] && info "dial_status 记录的 IP: $st_ip  ifname: ${st_ifname:-unknown}（和第5节 live 查询的结果核对，持续不一致说明 dial_status 更新滞后）"
    fi
    if [ -n "$st_rbpid" ] && [ "$st_rbpid" != "0" ]; then
        if [ -n "$rb_pids" ]; then
            # rb_pids 可能是 pgrep 输出的多行 pid，先转成空格分隔再做子串匹配，
            # 否则遇到多实例时 case 的 " $rb_pids " 分隔符对不上换行符会永远匹配不到。
            rb_pids_sp=$(echo "$rb_pids" | tr '\n' ' ')
            case " $rb_pids_sp " in
                *" $st_rbpid "*) info "dial_status 记录的 rbmaster_pid=$st_rbpid 与实际运行中的 RBMaster 进程一致" ;;
                *) warn "dial_status 记录的 rbmaster_pid=$st_rbpid，但实际运行中的 RBMaster pid 是 [$rb_pids_sp]，二者不一致 —— dial_status 里这个字段可能没跟上 RBMaster 重启" ;;
            esac
        else
            warn "dial_status 记录的 rbmaster_pid=$st_rbpid，但第1节没有探测到任何 RBMaster 进程 —— RBMaster 可能已经退出但 dial_status 没有更新"
        fi
    fi

    # --- SIM 卡信息 ---
    if [ -z "$st_iccid" ]; then
        warn "iccid 为空 —— SIM 卡尚未识别成功（物理SIM未插好/损坏，或当前在 roamlink 通道下这是正常的）"
    else
        ok "ICCID: $st_iccid"
        # apn.json 的 ICCID 匹配用的是"json 里该条目字符串本身的长度"做前缀长度
        # （eg25/apn/apn.c:449-451 size_t prefix_len = strlen(p_json_str)），不是固定取前6位，
        # 各条目前缀长度可以不一样——所以不能按固定位数截取。
        # 但这**不需要 python3**：shell 的 case "$iccid" in "$p"*) 天生就是变长前缀匹配。
        # 原先只用 python3 实现，而现场设备（busybox）普遍没装 python3，导致这项检查
        # 从来没真正跑过。这里改为纯 shell：
        #   tr -d ' \t\r' 去掉空白 → tr '{' '\n' 把数组拆成"每个对象一行"，
        #   这样无论 apn.json 是压缩成一行还是 pretty-print 都能处理（apn 数组里是扁平对象，
        #   字段 iccid/supplier/apn/usrname/...，无嵌套，见 apn.c:445-470）。
        # 顺序很重要：apn.c 是**首个前缀命中即 return**，所以这里也必须取第一条命中。
        if [ -f "$APN_JSON" ]; then
            match_prefix=""
            match_apn=""
            objs=$(tr -d ' \t\r\n' < "$APN_JSON" 2>/dev/null | tr '{' '\n')
            oldifs=$IFS; IFS='
'
            for obj in $objs; do
                p=$(echo "$obj" | sed -n 's/.*"iccid":"\([^"]*\)".*/\1/p')
                [ -n "$p" ] || continue
                case "$st_iccid" in
                    "$p"*)
                        match_prefix="$p"
                        match_apn=$(echo "$obj" | sed -n 's/.*"apn":"\([^"]*\)".*/\1/p')
                        break
                        ;;
                esac
            done
            IFS=$oldifs
            if [ -n "$match_prefix" ]; then
                ok "ICCID 在 $APN_JSON 中匹配到条目（前缀: $match_prefix → apn: ${match_apn:-未填}）"
                # 交叉核对：dial_status 里"正在用的 APN"应该就是上面匹配到的那条。
                # 不一致说明拨号用的 APN 不是 apn.json 现在这份的意思——常见于改了
                # apn.json 但没重启 modem_mng（apn 在 pre_start_call 阶段取一次）。
                if [ -n "$match_apn" ] && [ -n "$st_apn" ] && [ "$st_apn" != "unknown" ] && [ "$st_apn" != "$match_apn" ]; then
                    warn "dial_status 里正在用的 APN 是 '$st_apn'，但按当前 $APN_JSON 这张卡应该匹配到 '$match_apn' —— 两者不一致：要么改了 apn.json 之后没重启 modem_mng（APN 只在拨号前取一次），要么这条链路根本没走到匹配逻辑"
                fi
            elif [ -z "$objs" ]; then
                info "$APN_JSON 读取/解析失败，跳过 ICCID 匹配核对（第2节已报告 JSON 格式问题）"
            else
                fail "ICCID($st_iccid) 在 $APN_JSON 中没有任何条目前缀匹配 —— apn_get_apn_obj() 会退化返回空 APN 默认对象，拨号大概率失败或拨通也无法上网，需要确认这张卡对应的运营商是否已经录入 apn.json"
            fi
        fi
    fi

    if [ -n "$st_apn" ]; then
        info "当前使用的 APN: $st_apn  profile_idx=$st_profile"
    else
        info "dial_status 里 apn 字段为空 —— 尚未进入 pre_start_call 阶段，或 ICCID 未匹配到任何 apn.json 条目（见上一行判定）"
    fi

    # plmn 字段是已知死代码：nw_mark_plmn() 定义在 eg25/nw/nw.c 但全仓库零调用，
    # /tmp/network_plmn 永不会被写，dial_status 的 plmn 固定读到 unknown，不能作为判断依据
    info "PLMN 字段（当前值: ${st_plmn:-unknown}）固定读不到真实运营商代码 —— nw_mark_plmn() 是死代码，无任何调用方写过 /tmp/network_plmn，不代表未注网"

    # IMSI：dial_status 里**根本没有这个字段**——status/dial_status.c 的 [sim] 分节只写了
    # iccid/csq/apn/profile_idx/plmn（dial_status.c:157-162），全文件没有任何一处写过 imsi=。
    # 所以 $st_imsi 恒为空，这不是"采集失败"（原先的措辞会把人往 SIM 卡问题上引），
    # 而是和 plmn 同类的"字段不存在"。保留这一行只为解释"为什么这里看不到 IMSI"。
    if [ -n "$st_imsi" ]; then
        ok "IMSI: $st_imsi（意外情况：当前源码不写这个字段，若真读到值说明 dial_status.c 已改，需要重新核实本脚本）"
    else
        info "dial_status 里没有 IMSI 字段（不是采集失败）—— status/dial_status.c 的 [sim] 分节从未写过 imsi=，要看 IMSI 请查 nanomsg 38001 状态接口或日志"
    fi

    # CSQ 交叉核对：两者同源但落盘时机不同（/tmp/network_csq 只在值变化时重写、
    # dial_status 是周期性整体落盘），差个一两格是采样抖动的常态，原先只要不相等就
    # 报 WARN，属于噪音。这里给 ±2 格（≈4dBm）容差，超出容差才算值得看。
    if [ -n "$st_csq" ]; then
        if [ -n "$csq_val" ] && [ "$csq_val" != "$st_csq" ]; then
            csq_diff=$((csq_val - st_csq))
            [ "$csq_diff" -lt 0 ] && csq_diff=$((-csq_diff))
            if [ "$csq_diff" -gt 2 ]; then
                warn "dial_status.csq=$st_csq 与 /tmp/network_csq=$csq_val 相差 $csq_diff 格 —— 两者理论同源，差距超过采样抖动范围，若持续如此说明其中一条落盘路径滞后"
            else
                info "dial_status.csq=$st_csq / network_csq=$csq_val（相差 $csq_diff 格，属采样时机差异，正常；信号严重性判断见第0节）"
            fi
        else
            info "dial_status.csq=$st_csq（信号严重性判断见第0节）"
        fi
    fi

    if [ "$st_lic_pending" = "1" ] && [ -n "$st_lic_wait" ]; then
        warn "license_pending=1，已等待 ${st_lic_wait}s（超时阈值 ${LICENSE_WAIT_TIMEOUT_SEC}s）—— roamlink 尚未拿到 license，SIM 通道在临时顶替，超时后会放弃 roamlink 转入纯 SIM 重拨"
    fi

    # 注意：tcp_fail_count 在本仓库不是"连续TCP探测失败计数+阈值触发切换"，
    # 而是 dial.c:963 直接把 outage_count 赋值过去的别名（dstat.tcp_fail_count = p_dial_mng->outage_count），
    # 单纯代表"本次运行断网次数"，没有一个固定阈值会因为它触发通道切换 —— 不要套用
    # open_dial_for_artery 里"tcp_fail_count>=2接近阈值3"那套判断逻辑，这里语义不同。
    if [ -n "$st_tcp_fail" ]; then
        info "tcp_fail_count=$st_tcp_fail（本仓库语义：等同 outage_count，本次运行断网次数，非独立的切换触发计数器）"
    fi
    [ -n "$st_rl_fail" ] && info "roamlink_fail_count=$st_rl_fail（Roamlink 切回物理SIM的累计次数）"

    if [ -n "$st_cur_outage" ] && [ "$st_cur_outage" -gt 0 ] 2>/dev/null; then
        fail "current_outage_sec=$st_cur_outage —— 当前正处于断网中，已持续 ${st_cur_outage}s"
    fi
    if [ -n "$st_outage_cnt" ]; then
        info "本次运行累计断网 outage_count=$st_outage_cnt 次，last_outage_sec=${st_last_outage}s，total_outage_sec=${st_total_outage}s（进程重启会清零，不代表设备终身状态）"
    fi

    if [ "$VERBOSE" = "1" ]; then
        echo "---- 原始 dial_status ----"
        cat "$DIAL_STATUS"
        echo "--------------------------"
    fi
fi

# ------------------------------------------------------------------
sec "4. /tmp 状态文件交叉核对"
# ------------------------------------------------------------------

if [ -f "$NET_STATUS" ] && [ -f "$DIAL_STATUS" ]; then
    ns=$(cat "$NET_STATUS" 2>/dev/null)
    if [ -n "$st_net_status" ] && [ "$ns" != "$st_net_status" ]; then
        warn "/tmp/network_status=$ns 与 dial_status 里的 status=$st_net_status 不一致 —— 两者由不同代码路径写入，瞬时不一致是已知设计，若持续不一致才需关注"
    fi
fi

if [ -f "$NET_TYPE" ]; then
    nt=$(cat "$NET_TYPE" 2>/dev/null)
    if [ -f "$DIAL_STATUS" ] && [ -n "$st_net_type" ] && [ -n "$nt" ] && [ "$nt" != "$st_net_type" ]; then
        warn "/tmp/network_type=$nt 与 dial_status 里的 type=$st_net_type 不一致 —— roamlink_mark_network_type() 直写这个文件、dial_status_write() 是另一条路径周期性落盘（roamlink/roamlink.c:525起），瞬时不一致可能是采样时机差，持续不一致才需关注"
    fi
else
    info "/tmp/network_type 不存在 —— roamlink_mark_network_type() 从未被调用过，可能是尚未发生过任何一次通道判定/切换"
fi

if [ -f "$NET_PLMN" ]; then
    info "/tmp/network_plmn 存在，内容: $(cat "$NET_PLMN" 2>/dev/null)（意外情况：见上方死代码说明，理论上不该有内容被写入，若确实存在需要重新核实调用链）"
else
    info "/tmp/network_plmn 不存在 —— 符合预期：nw_mark_plmn() 是死代码，当前源码没有任何调用方，该文件本来就不会被写"
fi

# ------------------------------------------------------------------
sec "5. 网络接口与路由"
# ------------------------------------------------------------------

if have ip; then
    rmnet_if=$(ip -o link show 2>/dev/null | awk -F': ' '{print $2}' | grep '^rmnet')
elif have ifconfig; then
    rmnet_if=$(ifconfig -a 2>/dev/null | grep -o '^rmnet[a-zA-Z0-9_]*')
fi

# 之前这里 ip/ifconfig 都不存在时 $rmnet_if 直接留空，会被下面 -z 判断误报成
# "没有 rmnet 接口"（[FAIL]），实际是"没法查"，不是"查了确实没有"，两者结论
# 完全不同，必须先排除工具缺失这种情况。
if ! have ip && ! have ifconfig; then
    info "未找到 ip/ifconfig 命令，无法检查网络接口，跳过本节接口检测"
elif [ -z "$rmnet_if" ]; then
    fail "未找到任何 rmnet* 网络接口 —— 模组数据通道从驱动层就没建立起来，比软件状态机问题更底层"
else
    # 【判据】只有 dial_status 里 ifname= 指向的那条通道才**应该**有 IP。
    # EG25 驱动固定创建 rmnet0 + rmnet_data0~7 共 9 个接口，没拨号的那几条没有 IP 是常态，
    # 不是异常——原先逐个报 WARN 会一次性刷出 8 条噪音，把真正值得看的项淹掉。
    # 真正的 FAIL 判据只有两个：①活动接口没 IP；②一条 rmnet_data* 都没 IP。
    active_if="$st_ifname"
    case "$active_if" in none|n/a|unknown) active_if="" ;; esac
    any_ip=0
    idle_list=""
    for ifn in $rmnet_if; do
        if have ip; then
            ipaddr=$(ip -4 -o addr show "$ifn" 2>/dev/null | awk '{print $4}' | cut -d/ -f1)
        else
            ipaddr=$(ifconfig "$ifn" 2>/dev/null | grep -o 'inet addr:[0-9.]*' | cut -d: -f2)
        fi
        if [ -z "$ipaddr" ]; then
            if [ -n "$active_if" ] && [ "$ifn" = "$active_if" ]; then
                fail "活动接口 $ifn（dial_status 记录的正在用的接口）存在但没有 IP —— 拨号未真正建立或 IP 已被释放，而 dial_status 还停留在旧值"
            else
                idle_list="$idle_list $ifn"
            fi
        elif [ "$ipaddr" = "0.0.0.0" ]; then
            fail "接口 $ifn 分配到全零地址 0.0.0.0 —— DNS/IP 下发异常，常见于 license_pending 超时后 RBMaster 未及时释放信道"
        else
            any_ip=1
            if [ -z "$active_if" ]; then
                ok "接口 $ifn 分配到 IP: $ipaddr（dial_status 未记录活动接口名，无法精确核对是不是这一条）"
            elif [ "$ifn" = "$active_if" ]; then
                ok "活动接口 $ifn 分配到 IP: $ipaddr（与 dial_status 记录一致）"
            else
                info "接口 $ifn 也有 IP: $ipaddr，但 dial_status 记录的活动接口是 $active_if —— 多条通道同时拿到 IP 通常无害，持续如此可留意"
            fi
        fi
    done
    [ -n "$idle_list" ] && info "以下 rmnet 接口存在但无 IP（未拨号的备用通道，EG25 驱动固定创建 rmnet0+rmnet_data0~7，属正常，不计入告警）:$idle_list"
    if [ "$any_ip" = "0" ]; then
        fail "所有 rmnet* 接口都没有 IP —— 数据通道完全没建立起来"
    fi
fi

if have ip; then
    defroute=$(ip route show default 2>/dev/null)
else
    defroute=$(route -n 2>/dev/null | grep '^0.0.0.0')
fi
if [ -z "$defroute" ]; then
    fail "没有默认路由 —— 即使拨号 IP 有效也无法访问外网"
else
    ok "默认路由存在: $defroute"
fi

if [ -f /etc/resolv.conf ] && [ -s /etc/resolv.conf ]; then
    ok "/etc/resolv.conf 有内容: $(grep -c nameserver /etc/resolv.conf 2>/dev/null) 条 nameserver"
else
    warn "/etc/resolv.conf 缺失或为空 —— 即使数据通道通，域名解析也会失败"
fi

# ------------------------------------------------------------------
sec "6. rx_packets 增长（业务层是否真的有数据流动）"
# ------------------------------------------------------------------
# 依据：eg25/nw/nw.c:413 nw_get_rmnet_rx_packets_sum() 就是扫
# /sys/devices/virtual/net/rmnet_data*/statistics/rx_packets 汇总，和本节检测方式一致。

have_rmnet_data() {
    for d in /sys/devices/virtual/net/rmnet_data*/statistics/rx_packets; do
        [ -f "$d" ] && return 0
    done
    return 1
}

sum_rx() {
    total=0
    for d in /sys/devices/virtual/net/rmnet_data*/statistics/rx_packets; do
        [ -f "$d" ] || continue
        v=$(cat "$d" 2>/dev/null)
        case "$v" in
            ''|*[!0-9]*) continue ;;
        esac
        total=$((total + v))
    done
    echo "$total"
}

if have_rmnet_data; then
    rx1=$(sum_rx)
    sleep 3
    rx2=$(sum_rx)
    if [ "$rx2" -gt "$rx1" ] 2>/dev/null; then
        ok "rmnet_data* rx_packets 3秒内增长 $((rx2 - rx1)) 包 —— 确实有数据在流动"
    else
        warn "rmnet_data* rx_packets 3秒内无增长（$rx1 -> $rx2）—— 3秒采样太短仅供参考，建议结合第0节 ping 结果及 [HEARTBEAT] 里的 DownTime 字段判断。另注意：这里的 live 值与心跳里的 rx_packets 同源（都是 nw.c:413 扫 /sys/devices/virtual/net/rmnet_data*/statistics/rx_packets 汇总），计数器只增不减——若心跳里的数**比这里的 live 值还大**，那不是数据倒流，是那条心跳来自上一个 boot（见脚本顶部的日志可信度告警），不要拿两个 boot 的数对比"
    fi
else
    info "未找到 rmnet_data* 统计接口，跳过 rx_packets 检查"
fi

# ------------------------------------------------------------------
sec "7. 连通性测试（ping 为主，TCP 为辅）"
# ------------------------------------------------------------------
# modem_mng 自身的连通性判据就是 ICMP ping 8.8.8.8（eg25/dial/dial.c:317-319
# test_can_ping_google()，PING_FAIL_THRESHOLD=3 见 dial.c:24：连续3次失败才开始计断网时长），
# 不是 TCP。所以这里以 ping 为主判据，与参考脚本"TCP为主"的顺序相反；TCP 仅用来在
# ping 不通时排除"该地区/运营商限速丢弃 ICMP 但实际出网正常"的可能。

if have ping; then
    if ping -c 3 -W 2 "$PING_TEST_HOST" >/dev/null 2>&1; then
        ok "ping $PING_TEST_HOST（与 modem_mng 内部判据相同）连通"
    else
        fail "ping $PING_TEST_HOST 不通 —— 与 modem_mng 内部 test_can_ping_google() 用的是同一目标，若这里也不通，dial.c 大概率也在判定断网/计入 DownTime"
        if have nc || (echo >"/dev/tcp/127.0.0.1/1" 2>/dev/null); then
            tcp_test() {
                host="$1"; port="$2"; timeout="$3"
                if have nc; then
                    nc -z -w "$timeout" "$host" "$port" >/dev/null 2>&1
                    return $?
                fi
                ( exec 3<>"/dev/tcp/$host/$port" ) >/dev/null 2>&1
                ret=$?
                exec 3>&- 2>/dev/null
                return $ret
            }
            if tcp_test "$ALT_TCP_HOST1" "$ALT_TCP_PORT1" 5; then
                warn "但 TCP 探测 $ALT_TCP_HOST1:$ALT_TCP_PORT1 是通的 —— 可能是该地区/运营商对 ICMP 限速或丢弃，实际数据通道未必真的不通，别只看 ping 结果下结论"
            elif tcp_test "$ALT_TCP_HOST2" "$ALT_TCP_PORT2" 5; then
                warn "TCP 探测 $ALT_TCP_HOST1 不通，但 $ALT_TCP_HOST2:$ALT_TCP_PORT2 通 —— 同上，不能排除单一目标/路径问题，但也不能完全排除设备侧确有问题"
            else
                fail "TCP 探测两个目标也都不通 —— 与 ping 结果一致，指向设备本身没有出网能力"
            fi
        else
            info "未找到 nc 且 /dev/tcp 不受支持，跳过 TCP 补充验证"
        fi
    fi
else
    info "未找到 ping 命令，无法复现 modem_mng 自身的连通性判据，跳过本节"
fi

# ------------------------------------------------------------------
sec "8. 日志分析"
# ------------------------------------------------------------------

if [ -d "$SDCARD_DIR" ]; then
    if have df; then
        free_kb=$(df -k "$SDCARD_DIR" 2>/dev/null | awk 'NR==2{print $4}')
        if [ -n "$free_kb" ] && [ "$free_kb" -lt 1048576 ] 2>/dev/null; then
            warn "$SDCARD_DIR 剩余空间 <1GB —— logger_sd.c 有 1024MB 空间floor兜底，可能导致日志写入受限/被清理"
        fi
    fi
else
    info "$SDCARD_DIR 不存在 —— logger_sd.c 判定无 sdcard 时日志不会被写入，本节日志分析无法进行"
fi

if [ -n "$LATEST_LOG" ] && [ -f "$LATEST_LOG" ]; then
    # 本仓库 dial_log() 调用里没有统一的 ERROR/FATAL 日志级别约定（曾核实过，唯一的字面
    # "FATAL" 只出现在 L3 即将退出前那一行，见 dial.c:893），直接照搬 grep 'ERROR|FATAL'
    # 会把 QL_DATA_CALL_ERROR_NONE 之类的常量名当成异常，产生大量误报。改用真实存在的
    # 恢复分级 tag [RECOVERY Lx] 和那一行 FATAL 作为信号。
    l3_cnt=$(grep -c '\[RECOVERY L3\] FATAL' "$LATEST_LOG" 2>/dev/null)
    if [ "$l3_cnt" -gt 0 ] 2>/dev/null; then
        fail "当前日志中出现 $l3_cnt 次 [RECOVERY L3] FATAL —— 已触发过35min级最终恢复(exit重启)，说明发生过长时间断网"
    fi
    recovery_cnt=$(grep -Ec '\[RECOVERY L[12]\]' "$LATEST_LOG" 2>/dev/null)
    if [ "$recovery_cnt" -gt 0 ] 2>/dev/null; then
        warn "当前日志中有 $recovery_cnt 条 [RECOVERY L1]/[RECOVERY L2] 记录 —— 触发过软重拨/CFUN复位，说明期间发生过 5~10min 级断网"
        if [ "$VERBOSE" = "1" ]; then
            echo "---- 最近的 RECOVERY 记录 ----"
            grep -E '\[RECOVERY L' "$LATEST_LOG" 2>/dev/null | tail -20
            echo "------------------------------"
        fi
    else
        ok "当前日志中无 [RECOVERY Lx] 记录（未触发过分级恢复；若版本早于 1.31.5/e5d22d46 也不会有这个标签，见上方版本兼容性说明，不要单凭这一条断言从未故障）"
    fi
else
    info "未定位到当前日志文件，跳过日志内容分析"
fi

# ------------------------------------------------------------------
sec "诊断结果汇总"
# ------------------------------------------------------------------

hr
printf 'PASS=%d  WARN=%d  FAIL=%d\n' "$PASS" "$WARN" "$FAIL"
hr

if [ "$FAIL" -gt 0 ]; then
    echo "存在 [FAIL] 项，建议优先处理，它们直接对应联网中断/明确故障。"
elif [ "$WARN" -gt 0 ]; then
    echo "无 [FAIL] 但有 [WARN]，设备可能处于'临界'或'刚恢复'状态，建议留意是否复发。"
else
    echo "未发现明显异常。若用户仍反馈无法联网，建议结合 -v 参数重跑，人工核对原始 dial_status 与最近日志。"
fi

exit 0
