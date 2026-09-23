#!/bin/sh
#
# ec200a_rtms_modem_network.sh — rtms_sdk/apps/modem_mng（EC200A 平台，physical SIM only）
# 现场联网问题一键诊断脚本
#
# 和同目录 eg25_diag_network.sh 是姊妹脚本，但不是同一份文件改改路径——EC200A 和
# EG25 虽然都对齐各自的 open_dial 系参考实现，彼此的诊断面完全不同：EC200A 没有
# Roamlink/policy/network.ini，没有 EG25 那种富 /tmp/dial_status，REG 字段语义也不
# 一样。以下每一节的判据都标了本仓库源码文件:行号依据。
#
# ==================== 用法 ====================
#   sh ec200a_rtms_modem_network.sh        # 普通模式
#   sh ec200a_rtms_modem_network.sh -v     # 详细模式：打印更多原始日志片段
#
#   全程只读，不改文件、不重启进程、不发 AT 指令。
#
# ==================== 怎么看输出 ====================
#   [OK]/[WARN]/[FAIL]/[--] 同 eg25_diag_network.sh；最后看 PASS/WARN/FAIL 汇总行，
#   脚本退出码固定 0，不代表设备网络健康。
#
# ==================== 版本兼容性说明（务必先看）====================
#   EC200A 侧几乎全部诊断能力（[HEARTBEAT]、[RECOVERY Lx]、[INIT] AP_FW/CP_FW、
#   按天分目录日志、diag_snapshot dmesg/logcat 快照、/tmp/dial_retry_count 快速
#   失败重试、/tmp/cfun_count.txt 等）是同一个提交一次性引入的：
#     commit 52673fcc [REFACTOR] EC200A 全面对齐 open_dial v1.28.2，版本号1.31.0
#   在此之前（≤1.30.x）的固件只有最基础的 /tmp/dial_Status / /tmp/network_status /
#   /tmp/network_csq 和一份平铺日志文件，本脚本第0/3/8节大部分内容会找不到对应
#   文件/标签——那是版本太老，不是故障，请先用 dial_Status 之外的办法（比如问烧录
#   记录、或看日志目录是不是平铺的 dial_*.log 而不是按天分目录）确认版本再看结论。
#
# ==================== 设计原则 ====================
#   1. 只读不写，不发 AT 指令。EC200A 走 Quectel SDK 的 ql_atc_send／Ql_SendAT 封装，
#      本仓库没有像 EG25 g_at_port_mutex 那样在这几个文件里能直接看到的显式互斥锁
#      名字，但同样存在"外部脚本并发发指令会跟 dial_task 抢串口"的风险，原则不变：
#      绝不主动发 AT 指令，只读 modem_mng 已经落盘的文件和日志。
#   2. busybox ash 兼容写法，工具用前 command -v 探测。
#

DIAL_STATUS_LEGACY="/tmp/dial_Status"    # 注意大写S，ec200a/data_call/data_call.c:89,232,259
                                          # 语义和常识相反：0=已连接，1=未连接/断开
NET_STATUS="/tmp/network_status"         # ec200a/nw/nw.h NW_STATUS_PATH，0=断 1=通
NET_CSQ="/tmp/network_csq"               # ec200a/nw/nw.h NW_CSQ_PATH
APN_JSON="/usr/dial/apn.json"            # ec200a/apn/apn.c:295（和 EG25 同路径）
RETRY_COUNT_FILE="/tmp/dial_retry_count" # ec200a/dial/dial.cpp:300，快速失败重试计数
MAX_FAST_RETRY_TIMES=3                   # dial.cpp:301
CFUN_COUNT_FILE="/tmp/cfun_count.txt"    # dial.cpp:205
CFUN_LAST_CALL_FILE="/tmp/cfun_last_call.txt"  # dial.cpp:206
MIN_CFUN_INTERVAL_SEC=600                # dial.cpp:204，restart_cfun_safe() 里的最小间隔
SDCARD_DIR="/media/sdcard"
LOG_ROOT="/media/sdcard/dial_log"        # logger_sd.c 共用实现，1.31.0起按天分目录，此前是平铺 dial_*.log
DIAG_SNAP_DIR="/media/sdcard/dial_log/dial_snap"  # ec200a/diag/diag.c:32，dmesg/logcat 故障快照
PING_TEST_HOST="8.8.8.8"                 # ec200a/dial/dial.cpp "[PING] Ping 8.8.8.8 OK"，与 CLAUDE.md ST_PING 一致
ALT_TCP_HOST1="223.5.5.5"; ALT_TCP_PORT1="443"
ALT_TCP_HOST2="1.1.1.1";   ALT_TCP_PORT2="443"

VERBOSE=0
[ "$1" = "-v" ] && VERBOSE=1

PASS=0; WARN=0; FAIL=0
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
    if have stat; then stat -c %Y "$f" 2>/dev/null && return; fi
    find "$f" -printf '%T@\n' 2>/dev/null | cut -d. -f1
}
age_of() {
    f="$1"
    [ -e "$f" ] || { echo -1; return; }
    m=$(mtime_epoch "$f"); n=$(now_epoch)
    [ -n "$m" ] && [ -n "$n" ] && echo $((n - m)) || echo -1
}

# 按天分目录是 1.31.0 起才有；两种布局都尝试，找不到分天目录就退回在 LOG_ROOT
# 根下直接找平铺的 dial_*.log（老固件的样子），最后按 mtime 兜底。
find_latest_log() {
    today=$(date +%Y-%m-%d 2>/dev/null)
    for d in "$LOG_ROOT/$today" "$LOG_ROOT/unsynced"; do
        if [ -d "$d" ]; then
            f=$(ls -t "$d"/dial_*.log 2>/dev/null | head -1)
            [ -n "$f" ] && { echo "$f"; return; }
        fi
    done
    f=$(ls -t "$LOG_ROOT"/dial_*.log 2>/dev/null | head -1)
    [ -n "$f" ] && { echo "$f"; return; }
    find "$LOG_ROOT" -name 'dial_*.log' 2>/dev/null | while read -r p; do
        m=$(mtime_epoch "$p"); echo "$m $p"
    done | sort -rn 2>/dev/null | head -1 | cut -d' ' -f2-
}

hr
echo "modem_mng (EC200A) 联网问题诊断脚本  $(date '+%Y-%m-%d %H:%M:%S')"
hr

# 版本探测：两个来源，可靠性从高到低。
#   1. 日志启动行——dial_log("Program started. Version: %s\n", pversion)
#      （dialer_ec200a.cpp:135）从 EC200A 最早的提交（342f552a 一脉）起就存在，
#      每次进程启动都打一行，不受任何后续功能门控，是最可靠的来源。
#   2. /tmp/dial_version——commit 4cbca24e 才加入（EC200A v1.31.5 起），比日志行新，
#      日志被滚动清理掉时可以拿它兜底，不用找日志文件。
# EC200A 没有 EG25 那种 dial_status.c，没有第三个来源可兜底。
LATEST_LOG=$(find_latest_log)
dial_ver=""
if [ -n "$LATEST_LOG" ] && [ -f "$LATEST_LOG" ]; then
    dial_ver=$(grep -m1 'Program started. Version:' "$LATEST_LOG" 2>/dev/null | sed -n 's/.*Version: *//p')
fi
if [ -z "$dial_ver" ] && [ -f /tmp/dial_version ]; then
    dial_ver=$(sed -n 's/^Version: *//p' /tmp/dial_version 2>/dev/null | head -1)
fi
if [ -n "$dial_ver" ]; then
    info "当前 modem_mng 版本: $dial_ver"
else
    warn "两个版本来源（当前日志/tmp/dial_version）都没取到版本号——如果进程确实在跑，这本身就值得留意；如果进程没起来，见第1节"
fi

# ------------------------------------------------------------------
sec "0. 无线层基础信息（信号 / 驻网）—— 现场排查第一优先级"
# ------------------------------------------------------------------

if [ -f "$NET_CSQ" ]; then
    csq_val=$(cat "$NET_CSQ" 2>/dev/null)
    case "$csq_val" in
        ''|*[!0-9]*)
            fail "CSQ 内容异常（原始值: '$csq_val'，非纯数字）—— 可能读到了写入中途的半截内容"
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
else
    fail "$NET_CSQ 不存在 —— modem_mng 从未成功查过 CSQ，可能主线程卡死或进程未启动"
fi

# $LATEST_LOG 已在脚本顶部版本探测处求过值，这里直接复用，不重复调用 find_latest_log
if [ -n "$LATEST_LOG" ] && [ -f "$LATEST_LOG" ]; then
    info "当前使用的日志文件: $LATEST_LOG"

    # [HEARTBEAT] 每30s一条（HEARTBEAT_INTERVAL_MS=30000，dial.cpp:810），REG 字段
    # 是 get_cereg_status_safe() 的原始 AT+CEREG 状态码（0~5），不是布尔值——
    # 这一点和 EG25 的 [HEARTBEAT] cereg= 字段（那边是预先算好的0/1）不一样，
    # 两个平台的脚本不能互相套判断逻辑。
    last_hb=$(grep '\[HEARTBEAT\]' "$LATEST_LOG" 2>/dev/null | tail -1)
    if [ -n "$last_hb" ]; then
        info "最近一次心跳（每30s一条）: $last_hb"
        reg_val=$(echo "$last_hb" | sed -n 's/.*REG:\([0-9]\).*/\1/p')
        case "$reg_val" in
            1|5) ok "REG=$reg_val（1=home已注册 / 5=roaming已注册）" ;;
            3)   fail "REG=3（被网络拒绝注册，SIM/账户问题，L1/L2/L3 分级恢复大概率救不回来，见第3节 has_connected_once 门控说明）" ;;
            0|2) warn "REG=$reg_val（0=未注册未搜索 / 2=搜索中）—— 若长期停在这个值，优先查信号覆盖/天线而不是软件逻辑" ;;
            4)   warn "REG=4（未知状态）" ;;
            "")  info "未能从心跳行中解析出 REG 字段" ;;
            *)   warn "REG=$reg_val（不在标准 AT+CEREG 状态码 0~5 范围内，原始心跳行: $last_hb）" ;;
        esac
    else
        info "当前日志中没有 [HEARTBEAT] 记录 —— 若固件版本 ≤1.30.x（早于 commit 52673fcc/1.31.0）本来就没有这个标签，不代表故障，见上方版本兼容性说明"
    fi

    last_init_iccid=$(grep '\[INIT\] ICCID:' "$LATEST_LOG" 2>/dev/null | tail -1)
    [ -n "$last_init_iccid" ] && info "最近一次开机 ICCID: $last_init_iccid"
    last_init_fw=$(grep '\[INIT\] FW:' "$LATEST_LOG" 2>/dev/null | tail -1)
    [ -n "$last_init_fw" ] && info "最近一次开机固件版本: $last_init_fw（AP_FW/CP_FW 同理，-v 模式可看全部）"
else
    info "$LOG_ROOT 下未找到日志文件（SD卡未挂载/空间不足，见第8节），跳过驻网/INIT信息"
fi

# ------------------------------------------------------------------
sec "1. 进程存活"
# ------------------------------------------------------------------
# 本仓库 CMake 构建统一叫 modem_mng（CMakeLists.txt:2 project(modem_mng)）；但 EC200A
# 还留着一条独立的 Legacy Makefile 构建路径（仓库根 Makefile，DIAL_TARGET_EXE=dial，
# 装到 .../usr/dial/ 下），产物名叫 "dial"，不叫 "modem_mng"。这条路径最后一次改动
# 停留在很早期（~1.30版本前后），本次没能确认现场设备到底是哪条流水线烧的，两个
# 名字都探测一遍，不要假设只有一种。

dial_pids=$(pgrep -f "modem_mng" 2>/dev/null)
# 不用 "\b" 词边界 + "|" 分支这种正则写法：busybox pgrep 的正则实现普遍比 GNU
# procps 弱，ERE 扩展语法（尤其 \b）未必支持——实测在本机 GNU pgrep 下这种写法
# 曾经把命令行里恰好包含该子串的无关进程也匹配进来（自匹配误报），改成一次
# 纯字符串 pgrep 调用，不依赖分支/词边界语法，两个搜索串("modem_mng"和
# "/usr/dial/dial")本身互不重叠，不需要额外排重。
legacy_pids=$(pgrep -f "/usr/dial/dial" 2>/dev/null)
all_pids="$dial_pids $legacy_pids"
pid_cnt=$(echo "$all_pids" | tr ' ' '\n' | grep -c '[0-9]')

if [ "$pid_cnt" -eq 0 ] 2>/dev/null; then
    fail "modem_mng/dial 进程未运行 —— 联网功能完全停摆"
elif [ -n "$dial_pids" ] && [ -n "$legacy_pids" ]; then
    warn "同时匹配到 modem_mng 和 dial 两种进程名 (modem_mng:$dial_pids dial:$legacy_pids) —— 检查是不是新旧两套构建产物都在跑，正常应只有一个"
elif [ "$pid_cnt" -gt 1 ]; then
    warn "检测到 $pid_cnt 个进程实例 (pid: $(echo "$all_pids" | tr '\n' ' ' | tr -s ' ')) —— 正常应只有 1 个，多实例会抢串口/状态文件"
else
    ok "进程正常运行 (pid: $(echo "$all_pids" | tr '\n' ' ' | tr -s ' '))（匹配名: ${dial_pids:+modem_mng}${legacy_pids:+dial}）"
fi

if pgrep -f "sw_mng" >/dev/null 2>&1; then
    ok "sw_mng 看门狗运行中（注意：sw_mng/src/main.cpp 的 atime 超时检测只打日志，kill(pid,0) 只是存在性探测——进程卡死但存活时不会被杀/拉起，只有进程真退出才会拉起）"
else
    warn "sw_mng 未检测到 —— 进程一旦崩溃退出将不会被自动拉起"
fi

# ------------------------------------------------------------------
sec "1b. SDK 数据服务层（ql_netd / ql_rild / ql_atc）"
# ------------------------------------------------------------------
# 依据：固件 ql_procd_scripts.conf 启动 ql_rild/ql_netd/ql_atc；rootfs 二进制实证——
#   ql_netd = 数据服务守护（ql_data_call_* 的服务端，管 ccinet0~7、写 resolv、
#             IPC=/tmp/.ql_net_ipc_path、就绪标志=/tmp/ql_net_srv_ready.flag，
#             且含字符串 "Timeout for waiting RIL service ready" → 依赖 ql_rild）；
#   ql_rild = RIL 守护，面向 CP/射频；  ql_atc = AT 命令服务（独立进程）。
# 本节专治 modem_mng 日志里 "ql_data_call_init failed, ret=-1067(SERVICE_NOT_READY)" 那类：
# 设备已注册、AT 能查、CP 甚至有 IP，但 ping 不通——根因在这一层，不是 SIM/账户。
# （2026-07-14 现场案例，见 md/session/EC200A_数据服务未就绪_ping不通_根因调研_2026-07-14.md）

# 注意：以下路径/进程名来自 A02 SDK（ec200acntar02a02m2g）固件实证，别的固件版本
# （如现场 A04）可能不同。因此"标志缺失"只作 [WARN] 不作 [FAIL]，最终以第4/6节接口 IP、
# 第8节 ping 为准；进程存活判据（ql_netd/ql_rild）相对稳定，仍作 [FAIL]。
QL_NET_READY_FLAG="/tmp/ql_net_srv_ready.flag"
QL_NET_IPC_PATH="/tmp/.ql_net_ipc_path"

if pgrep -f "ql_netd" >/dev/null 2>&1; then
    ok "ql_netd 数据服务守护在跑"
    # netd 活着但未就绪 → 多半卡在"等 ql_rild(CP) ready"
    if [ -e "$QL_NET_READY_FLAG" ]; then
        ok "$QL_NET_READY_FLAG 存在（数据服务已就绪）"
    else
        warn "$QL_NET_READY_FLAG 缺失 —— 可能 ql_netd 在跑但未就绪(卡等 ql_rild，会致 ql_data_call_init SERVICE_NOT_READY)；也可能此固件版本标志路径不同(本路径 A02 实证)。以接口 IP/ping 结果为准"
    fi
    [ -e "$QL_NET_IPC_PATH" ] || warn "$QL_NET_IPC_PATH IPC 端点缺失 —— 客户端(libql_sdk)可能连不上数据服务"
else
    fail "ql_netd 未在运行 —— AP 侧数据服务守护不在，ql_data_call_init 必然 SERVICE_NOT_READY，整条数据面死；进程/整机重启或查固件侧该服务"
fi

if pgrep -f "ql_rild" >/dev/null 2>&1; then
    ok "ql_rild（RIL/CP 面）在跑"
else
    fail "ql_rild 未在运行 —— ql_netd 依赖它，RIL 不在则数据服务永远不就绪"
fi

if pgrep -f "ql_atc" >/dev/null 2>&1; then
    info "ql_atc（AT 命令服务）在跑 —— 它独立于数据服务，故 AT 能查到 IMEI/COPS 等，不代表数据面正常"
else
    warn "ql_atc 未检测到 —— 连 AT 查询也可能异常，比数据服务问题更靠底层"
fi

# ------------------------------------------------------------------
sec "2. 配置文件完整性"
# ------------------------------------------------------------------
# EC200A 是纯物理SIM，没有 Roamlink/policy/network.ini 这套东西（那是 EG25G-only
# 功能，见 roamlink/ 模块注释），本节只需要核对 apn.json。

if [ -f "$APN_JSON" ]; then
    if have python3; then
        if python3 -c "import json,sys; json.load(open('$APN_JSON'))" >/dev/null 2>&1; then
            ok "$APN_JSON 存在且 JSON 格式合法"
        else
            fail "$APN_JSON 存在但 JSON 解析失败 —— apn_get_apn_obj()/set_apn() 匹配不到会退化为空/自动 APN，拨号大概率失败或用错 APN"
        fi
    else
        if grep -q '"iccid"' "$APN_JSON" 2>/dev/null; then
            ok "$APN_JSON 存在（未装 python3，跳过严格 JSON 校验）"
        else
            warn "$APN_JSON 内容可疑，未找到 iccid 字段"
        fi
    fi
else
    info "$APN_JSON 不存在 —— 不是致命问题：apn_get_apn_obj() 找不到条目时退化为自动/默认 APN（ec200a/apn/apn.c），部分运营商能自动选对，但建议确认这张卡是否需要专用 APN"
fi

# ------------------------------------------------------------------
sec "3. 运行状态文件"
# ------------------------------------------------------------------
# EC200A 没有 EG25 那种富 /tmp/dial_status（INI多段格式），状态分散在几个独立文件里。

if [ -f "$DIAL_STATUS_LEGACY" ]; then
    v=$(cat "$DIAL_STATUS_LEGACY" 2>/dev/null)
    age=$(age_of "$DIAL_STATUS_LEGACY")
    # 语义反直觉：0=已连接，1=未连接（ec200a/data_call/data_call.c:89 连接成功写0，
    # :232/:259 失败/断开写1），别按"0=正常安静/1=有问题"这种通用惯例去猜。
    case "$v" in
        0) ok "$DIAL_STATUS_LEGACY = 0（已连接），更新于 ${age}s 前" ;;
        1) fail "$DIAL_STATUS_LEGACY = 1（未连接/断开），更新于 ${age}s 前" ;;
        *) warn "$DIAL_STATUS_LEGACY 内容异常: '$v'" ;;
    esac
else
    if [ "$pid_cnt" -gt 0 ] 2>/dev/null; then
        warn "$DIAL_STATUS_LEGACY 不存在，但进程在跑 —— 数据呼叫状态回调从未触发过，结合第5/7节的 IP/ping 结果判断"
    else
        fail "$DIAL_STATUS_LEGACY 不存在，进程也未运行"
    fi
fi

if [ -f "$NET_STATUS" ]; then
    ns=$(cat "$NET_STATUS" 2>/dev/null)
    case "$ns" in
        1) ok "$NET_STATUS = 1（联网中）" ;;
        0) fail "$NET_STATUS = 0（未联网）" ;;
        *) warn "$NET_STATUS 内容异常: '$ns'" ;;
    esac
    if [ -f "$DIAL_STATUS_LEGACY" ] && [ -n "$v" ] && [ -n "$ns" ]; then
        # 两个文件语义相反：dial_Status 0=连接 对应 network_status 1=联网
        if { [ "$v" = "0" ] && [ "$ns" != "1" ]; } || { [ "$v" = "1" ] && [ "$ns" != "0" ]; }; then
            warn "$DIAL_STATUS_LEGACY=$v 与 $NET_STATUS=$ns 不一致（注意两者 0/1 含义相反：dial_Status 0=连接，network_status 1=联网）—— 瞬时不一致可能是两处写入时机差，持续不一致才需关注"
        fi
    fi
else
    info "$NET_STATUS 不存在"
fi

if [ -f "$RETRY_COUNT_FILE" ]; then
    rc=$(cat "$RETRY_COUNT_FILE" 2>/dev/null)
    if [ -n "$rc" ] && [ "$rc" -ge "$MAX_FAST_RETRY_TIMES" ] 2>/dev/null; then
        info "$RETRY_COUNT_FILE=$rc（达到快速失败上限 $MAX_FAST_RETRY_TIMES，check_and_update_retry_count() 已放弃快速退出策略，转入持续运行模式，这是正常降级不是故障）"
    elif [ -n "$rc" ]; then
        warn "$RETRY_COUNT_FILE=$rc（<$MAX_FAST_RETRY_TIMES）—— 说明最近几次启动在10秒内都没能 ping 通就退出了（FAST_FAIL_TIMEOUT_MS=10000），处于快速失败重试阶段，应关注是不是反复重启"
    fi
else
    info "$RETRY_COUNT_FILE 不存在（正常：首次启动，或已成功联网后被清零——dial.cpp 里 ping 通后会清除这个计数器）"
fi

if [ -f "$CFUN_COUNT_FILE" ]; then
    cc=$(cat "$CFUN_COUNT_FILE" 2>/dev/null)
    info "$CFUN_COUNT_FILE=$cc —— 注意：这个计数器只统计经 restart_cfun_safe() 走的 CFUN 调用（dial.cpp:249起），当前 L2 恢复分级是直接调 Ql_SendAT(\"AT+CFUN=0/1\")，不经过这个计数器（dial.cpp:1279-1281），所以这个数字会低估实际 CFUN 复位次数，不能拿它当唯一依据判断'CFUN是不是被频繁调用'"
fi
if [ -f "$CFUN_LAST_CALL_FILE" ]; then
    lc=$(cat "$CFUN_LAST_CALL_FILE" 2>/dev/null)
    info "$CFUN_LAST_CALL_FILE=$lc（uptime秒数，同样只覆盖 restart_cfun_safe() 路径，最小间隔 ${MIN_CFUN_INTERVAL_SEC}s，见上方说明）"
fi

# ------------------------------------------------------------------
sec "4. 网络接口与路由"
# ------------------------------------------------------------------

# EC200A（ASR 平台）的数据网卡是 ccinet0~7，不是 Qualcomm 系的 rmnet*：
# dialer_ec200a.cpp:107 wait_for_interface("ccinet0")、NetworkMonitor check_CCINet_*、
# ql_netd 管理 ccinet0~7。原脚本此处/第5节找 rmnet 疑似照 EG25/高通模板搬来未改，
# 会在健康的 EC200A 上误报"未找到接口"。（改自代码事实，建议真机 `ip link` 复核。）
if have ip; then
    data_if=$(ip -o link show 2>/dev/null | awk -F': ' '{print $2}' | grep '^ccinet')
elif have ifconfig; then
    data_if=$(ifconfig -a 2>/dev/null | grep -o '^ccinet[a-zA-Z0-9_]*')
fi

# ip/ifconfig 都没有时 $data_if 会一直是空，不能和"查了确实没有接口"混为一谈，
# 否则会把"没法查"误报成 [FAIL]。
if ! have ip && ! have ifconfig; then
    info "未找到 ip/ifconfig 命令，无法检查网络接口，跳过本节接口检测"
elif [ -z "$data_if" ]; then
    fail "未找到任何 ccinet* 网络接口 —— 数据通道从驱动层就没建立起来，比软件状态机问题更底层"
else
    for ifn in $data_if; do
        if have ip; then
            ipaddr=$(ip -4 -o addr show "$ifn" 2>/dev/null | awk '{print $4}' | cut -d/ -f1)
        else
            ipaddr=$(ifconfig "$ifn" 2>/dev/null | grep -o 'inet addr:[0-9.]*' | cut -d: -f2)
        fi
        if [ -z "$ipaddr" ]; then
            warn "接口 $ifn 存在但未分配 IP"
        elif [ "$ipaddr" = "0.0.0.0" ]; then
            fail "接口 $ifn 分配到全零地址 0.0.0.0"
        else
            ok "接口 $ifn 分配到 IP: $ipaddr"
        fi
    done
fi

if have ip; then
    defroute=$(ip route show default 2>/dev/null)
else
    defroute=$(route -n 2>/dev/null | grep '^0.0.0.0')
fi
if [ -z "$defroute" ]; then
    fail "没有默认路由"
else
    ok "默认路由存在: $defroute"
fi

if [ -f /etc/resolv.conf ] && [ -s /etc/resolv.conf ]; then
    ok "/etc/resolv.conf 有内容: $(grep -c nameserver /etc/resolv.conf 2>/dev/null) 条 nameserver"
else
    warn "/etc/resolv.conf 缺失或为空"
fi

# ------------------------------------------------------------------
sec "5. rx_packets 增长"
# ------------------------------------------------------------------

# EC200A 数据网卡是 ccinet*，统计走标准 netdev sysfs（/sys/class/net/ccinet*/statistics）。
# 原脚本查 /sys/devices/virtual/net/rmnet_data*（高通系路径），在 EC200A 上恒空、静默跳过。
have_data_stats() {
    for d in /sys/class/net/ccinet*/statistics/rx_packets; do
        [ -f "$d" ] && return 0
    done
    return 1
}
sum_rx() {
    total=0
    for d in /sys/class/net/ccinet*/statistics/rx_packets; do
        [ -f "$d" ] || continue
        v=$(cat "$d" 2>/dev/null)
        case "$v" in ''|*[!0-9]*) continue ;; esac
        total=$((total + v))
    done
    echo "$total"
}
if have_data_stats; then
    rx1=$(sum_rx); sleep 3; rx2=$(sum_rx)
    if [ "$rx2" -gt "$rx1" ] 2>/dev/null; then
        ok "ccinet* rx_packets 3秒内增长 $((rx2 - rx1)) 包"
    else
        warn "ccinet* rx_packets 3秒内无增长（$rx1 -> $rx2）—— 采样太短仅供参考，结合第0节 [HEARTBEAT] 的 DownTime 字段判断"
    fi
else
    info "未找到 ccinet* 统计接口，跳过"
fi

# ------------------------------------------------------------------
sec "6. 连通性测试（ping 为主，TCP 为辅）"
# ------------------------------------------------------------------
# modem_mng 自身的判据就是 ping 8.8.8.8（[PING] Ping 8.8.8.8 OK/失败见心跳 DownTime），
# 和 EG25 一致，都不是 TCP。

if have ping; then
    if ping -c 3 -W 2 "$PING_TEST_HOST" >/dev/null 2>&1; then
        ok "ping $PING_TEST_HOST（与 modem_mng 内部判据相同）连通"
    else
        fail "ping $PING_TEST_HOST 不通 —— 与 modem_mng 内部判据同一目标，dial.cpp 大概率也在判定断网/累加 DownTime"
        if have nc || (echo >"/dev/tcp/127.0.0.1/1" 2>/dev/null); then
            tcp_test() {
                host="$1"; port="$2"; timeout="$3"
                if have nc; then nc -z -w "$timeout" "$host" "$port" >/dev/null 2>&1; return $?; fi
                ( exec 3<>"/dev/tcp/$host/$port" ) >/dev/null 2>&1
                ret=$?; exec 3>&- 2>/dev/null; return $ret
            }
            if tcp_test "$ALT_TCP_HOST1" "$ALT_TCP_PORT1" 5; then
                warn "但 TCP 探测 $ALT_TCP_HOST1:$ALT_TCP_PORT1 是通的 —— 可能是该地区/运营商限速丢弃 ICMP，别只看 ping 结果下结论"
            elif tcp_test "$ALT_TCP_HOST2" "$ALT_TCP_PORT2" 5; then
                warn "TCP 探测 $ALT_TCP_HOST1 不通，但 $ALT_TCP_HOST2:$ALT_TCP_PORT2 通"
            else
                fail "TCP 探测两个目标也都不通 —— 指向设备本身没有出网能力"
            fi
        else
            info "未找到 nc 且 /dev/tcp 不受支持，跳过 TCP 补充验证"
        fi
    fi
else
    info "未找到 ping 命令，跳过"
fi

# ------------------------------------------------------------------
sec "7. 日志分析 / 故障快照"
# ------------------------------------------------------------------
# 和 EG25 不同，本仓库 EC200A 的 dial_log 调用里确实有字面的 [ERROR]/[FATAL]/[WARN]/
# [WARNING] 标签（ec200a/dial/dial.cpp），不是只有枚举常量名混进来，可以直接 grep。

if [ -d "$SDCARD_DIR" ]; then
    if have df; then
        free_kb=$(df -k "$SDCARD_DIR" 2>/dev/null | awk 'NR==2{print $4}')
        if [ -n "$free_kb" ] && [ "$free_kb" -lt 1048576 ] 2>/dev/null; then
            warn "$SDCARD_DIR 剩余空间 <1GB —— logger_sd.c 有 1024MB 空间floor兜底，可能导致日志写入受限"
        fi
    fi
else
    info "$SDCARD_DIR 不存在 —— 日志不会被写入，本节分析无法进行"
fi

if [ -n "$LATEST_LOG" ] && [ -f "$LATEST_LOG" ]; then
    l3_cnt=$(grep -c '\[RECOVERY L3\]' "$LATEST_LOG" 2>/dev/null)
    if [ "$l3_cnt" -gt 0 ] 2>/dev/null; then
        fail "当前日志中出现 $l3_cnt 次 [RECOVERY L3] —— 触发过35min级最终恢复(exit重启)，说明发生过长时间断网"
    fi
    recovery_cnt=$(grep -Ec '\[RECOVERY L[12]\]' "$LATEST_LOG" 2>/dev/null)
    if [ "$recovery_cnt" -gt 0 ] 2>/dev/null; then
        warn "当前日志中有 $recovery_cnt 条 [RECOVERY L1]/[RECOVERY L2] 记录 —— 触发过软重拨/CFUN复位"
        if [ "$VERBOSE" = "1" ]; then
            echo "---- 最近的 RECOVERY 记录 ----"
            grep -E '\[RECOVERY' "$LATEST_LOG" 2>/dev/null | tail -20
            echo "------------------------------"
        fi
    else
        info "当前日志中无 [RECOVERY Lx] 记录（未触发过分级恢复；若固件 ≤1.30.x 也不会有这个标签，见上方版本兼容性说明）"
    fi

    err_cnt=$(grep -Ec '\[ERROR\]|\[FATAL\]|\[WARNING\]' "$LATEST_LOG" 2>/dev/null)
    if [ "$err_cnt" -gt 0 ] 2>/dev/null; then
        warn "当前日志中有 $err_cnt 条 [ERROR]/[FATAL]/[WARNING] 记录"
        [ "$VERBOSE" = "1" ] && grep -E '\[ERROR\]|\[FATAL\]|\[WARNING\]' "$LATEST_LOG" 2>/dev/null | tail -20
    else
        ok "当前日志中无 [ERROR]/[FATAL]/[WARNING] 记录"
    fi

    # 数据服务 init 失败（2026-07-14 现场案例的核心特征）：ql_data_call_init 未就绪，
    # 整条数据面在 SDK 服务层就断了，与 SIM/账户无关。这类会连带触发下面的 never-connected 门控。
    dcinit_fail=$(grep -Ec 'data_call_init failed|SERVICE_NOT_READY|Data service did not init this session' "$LATEST_LOG" 2>/dev/null)
    if [ "$dcinit_fail" -gt 0 ] 2>/dev/null; then
        fail "日志出现 ql_data_call_init 失败 / SERVICE_NOT_READY（$dcinit_fail 次）—— AP 侧数据服务(ql_netd)本次没起来，数据面断在 SDK 服务层、与 SIM/账户无关；对照第 1b 节 ql_netd/ql_rild 状态定位"
        [ "$VERBOSE" = "1" ] && grep -E 'data_call_init failed|SERVICE_NOT_READY|Data service did not init this session' "$LATEST_LOG" 2>/dev/null | tail -10
    fi

    gated=$(grep -c 'never-connected, policy recovery' "$LATEST_LOG" 2>/dev/null)
    if [ "$gated" -gt 0 ] 2>/dev/null; then
        # 门控自证会在两种性质不同的情形下都出现，别一律判成 SIM/账户问题（对齐 commit f2cc1345）：
        if [ "$dcinit_fail" -gt 0 ] 2>/dev/null; then
            warn "日志出现 'never-connected ... gated'，但同时有数据服务 init 失败 —— 这是数据服务层故障（查第 1b 节 ql_netd/ql_rild/CP），不是 SIM/账户问题；旧脚本/旧日志会把此情形误报为 SIM，勿被带偏"
        else
            info "日志出现过 'never-connected, policy recovery gated'，且未见数据服务 init 失败 —— 设备从未成功联网、数据服务本身正常，此时才更像 SIM卡/账户问题（停机/无套餐），见 CLAUDE.md 恢复门控说明"
        fi
    fi
else
    info "未定位到当前日志文件，跳过日志内容分析"
fi

if [ -d "$DIAG_SNAP_DIR" ]; then
    last_snap=$(ls -t "$DIAG_SNAP_DIR" 2>/dev/null | head -1)
    if [ -n "$last_snap" ]; then
        snap_age=$(age_of "$DIAG_SNAP_DIR/$last_snap")
        info "最近一次故障快照(dmesg/logcat): $last_snap（${snap_age}s 前，diag_snapshot 在断网故障首次确认/恢复时各触发一次）"
    else
        info "$DIAG_SNAP_DIR 目录存在但为空 —— 尚未触发过故障快照"
    fi
else
    info "$DIAG_SNAP_DIR 不存在 —— 若固件 ≤1.30.x（早于1.31.0）本来就没有这个能力，不代表故障；若是新固件则说明从未触发过 diag_snapshot"
fi

# ------------------------------------------------------------------
sec "诊断结果汇总"
# ------------------------------------------------------------------

hr
printf 'PASS=%d  WARN=%d  FAIL=%d\n' "$PASS" "$WARN" "$FAIL"
hr

if [ "$FAIL" -gt 0 ]; then
    echo "存在 [FAIL] 项，建议优先处理。"
elif [ "$WARN" -gt 0 ]; then
    echo "无 [FAIL] 但有 [WARN]，设备可能处于临界/刚恢复状态，建议留意是否复发。"
else
    echo "未发现明显异常。若用户仍反馈无法联网，建议结合 -v 参数重跑，人工核对日志。"
fi

exit 0
