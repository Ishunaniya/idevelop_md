#!/bin/sh
#
# ag35_rtms_modem_network.sh — rtms_sdk/apps/modem_mng（AG35 平台）现场联网问题一键诊断脚本
#
# 不是从零写的，是 ec200a_rtms_diag_network.sh 的超集：AG35 和 EC200A 编译的是同一份
# ec200a/ 源码树（同一个 dialer_ec200a.cpp 入口、同一个 dial.cpp），只是多编译进
# ec200a/oper/（海外选网）和 ec200a/slot/（双卡单待 DSSS）两个模块——这两个模块整个
# 用 #ifdef QL_MODULE_PLATFORM_AG35 包起来，EC200A 编出来是空翻译单元。所以第0/1/2(部分)
# /3/6/7/8/9 节的判据和 ec200a_rtms_diag_network.sh 完全一致（同一份代码），第4/5节
# （SLOT/OPER）是本脚本独有、EC200A 上跑这两节永远得不到任何输出。
#
# ==================== 用法 ====================
#   sh ag35_rtms_modem_network.sh        # 普通模式
#   sh ag35_rtms_modem_network.sh -v     # 详细模式：打印更多原始日志片段
#
#   全程只读，不改文件、不重启进程、不发 AT 指令。
#
# ==================== 怎么看输出 ====================
#   [OK]/[WARN]/[FAIL]/[--] 同 ec200a/eg25 姊妹脚本；最后看 PASS/WARN/FAIL 汇总行，
#   脚本退出码固定 0，不代表设备网络健康。
#
# ==================== 版本兼容性说明（务必先看）====================
#   dial.hpp 的版本宏自某个时间点起按平台分叉（ec200a/dial/dial.hpp:50-58 #ifdef
#   QL_MODULE_PLATFORM_AG35）：AG35 = 1.32.x，EC200A = 1.31.x，两边版本号独立，互不
#   对应。SLOT/OPER 两个模块是随 AG35 双卡功能一起进入这条 1.32.x 线的，老一点的
#   1.32.0 已经有；如果设备版本号是 1.31.x，说明它实际编的是 EC200A 配置（或者更早
#   期 AG35 曾经和 EC200A 共用过 1.31.x 号段——历史上有过这个阶段），第4/5节大概率
#   全是空，不代表故障，先看第0节脚本顶部打出来的版本号。
#   其余能力（[HEARTBEAT]/[RECOVERY Lx]/diag_snapshot/按天日志/快速失败重试）的版本
#   门槛和 EC200A 一致，见 ec200a_rtms_diag_network.sh 里的说明，不再重复。
#
# ==================== 设计原则 ====================
#   1. 只读不写，不发 AT 指令，原则同 ec200a_rtms_diag_network.sh。
#   2. busybox ash 兼容写法，工具用前 command -v 探测。
#

DIAL_STATUS_LEGACY="/tmp/dial_Status"    # 注意大写S，ec200a/data_call/data_call.c
                                          # 语义反常识：0=已连接，1=未连接/断开
NET_STATUS="/tmp/network_status"         # ec200a/nw/nw.h NW_STATUS_PATH，0=断 1=通
NET_CSQ="/tmp/network_csq"               # ec200a/nw/nw.h NW_CSQ_PATH
APN_JSON="/usr/dial/apn.json"            # ec200a/apn/apn.c
RETRY_COUNT_FILE="/tmp/dial_retry_count" # ec200a/dial/dial.cpp，快速失败重试计数
MAX_FAST_RETRY_TIMES=3
CFUN_COUNT_FILE="/tmp/cfun_count.txt"
CFUN_LAST_CALL_FILE="/tmp/cfun_last_call.txt"
MIN_CFUN_INTERVAL_SEC=600
SDCARD_DIR="/media/sdcard"
LOG_ROOT="/media/sdcard/dial_log"
DIAG_SNAP_DIR="/media/sdcard/dial_log/dial_snap"
PING_TEST_HOST="8.8.8.8"
ALT_TCP_HOST1="223.5.5.5"; ALT_TCP_PORT1="443"
ALT_TCP_HOST2="1.1.1.1";   ALT_TCP_PORT2="443"

# —— AG35 独有：双卡单待(DSSS) + 海外选网(OPER)，ec200a/slot/、ec200a/oper/ ——
DUALSIM_JSON="/usr/dial/dualsim.json"        # ec200a/slot/slot_mgr.c:56 DUALSIM_JSON_PATH，
                                              # 双卡+OPER配置共用一份文件，slot_mgr 独占读写；
                                              # 缺文件自播种、缺键合并补写，不走 config.json
SIM_SLOT_FILE="/tmp/sim_slot"                # slot_mgr.c write_sim_slot_file()，当前激活物理槽 1/2
SIM_SLOT_STATUS_JSON="/tmp/sim_slot_status.json"  # slot_mgr.c slot_mgr_write_status()
                                              # {"phy":N,"iccid":"..","reason":"..","switch_count":N,"ts":N}
                                              # 原子 tmp+rename 写，reason 取值见第4节
SIM_LAST_GOOD_FILE="/media/sdcard/sim_last_good"  # slot_mgr.c:41 SM_LAST_GOOD_PATH，格式 "<phy> <unix_ts>"
                                              # 只在"最优驻留"时写，强制切换不污染它（slot_mgr_mark_good）

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
echo "modem_mng (AG35) 联网问题诊断脚本  $(date '+%Y-%m-%d %H:%M:%S')"
hr

LATEST_LOG=$(find_latest_log)
dial_ver=""
if [ -n "$LATEST_LOG" ] && [ -f "$LATEST_LOG" ]; then
    dial_ver=$(grep -m1 'Program started. Version:' "$LATEST_LOG" 2>/dev/null | sed -n 's/.*Version: *//p')
fi
if [ -z "$dial_ver" ] && [ -f /tmp/dial_version ]; then
    dial_ver=$(sed -n 's/^Version: *//p' /tmp/dial_version 2>/dev/null | head -1)
fi
if [ -n "$dial_ver" ]; then
    info "当前 modem_mng 版本: $dial_ver（AG35 走 1.32.x 号段，见上方版本兼容性说明；若是 1.31.x 说明第4/5节大概率为空）"
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

if [ -n "$LATEST_LOG" ] && [ -f "$LATEST_LOG" ]; then
    info "当前使用的日志文件: $LATEST_LOG"

    last_hb=$(grep '\[HEARTBEAT\]' "$LATEST_LOG" 2>/dev/null | tail -1)
    if [ -n "$last_hb" ]; then
        info "最近一次心跳（每30s一条）: $last_hb"
        # AG35 的心跳行比 EC200A 多一个末尾 SLOT:%s 字段（dial.cpp:1063，双卡专属分支），
        # 每 30s 就能看一眼当前驻留槽，比单独查 /tmp/sim_slot 更能看出"是不是刚变过"。
        hb_slot=$(echo "$last_hb" | sed -n 's/.*SLOT:\([A-Za-z?]*\).*/\1/p')
        [ -n "$hb_slot" ] && info "心跳里的当前驻留槽: $hb_slot（若和第4节 /tmp/sim_slot 不一致，以心跳更新时间较晚者为准）"
        reg_val=$(echo "$last_hb" | sed -n 's/.*REG:\([0-9]\).*/\1/p')
        case "$reg_val" in
            1|5) ok "REG=$reg_val（1=home已注册 / 5=roaming已注册）" ;;
            3)   fail "REG=3（被网络拒绝注册，SIM/账户问题；AG35 上可能触发第4节 A-denied 快切槽，见第4节）" ;;
            0|2) warn "REG=$reg_val（0=未注册未搜索 / 2=搜索中）—— 若长期停在这个值，优先查信号覆盖/天线而不是软件逻辑" ;;
            4)   warn "REG=4（未知状态）" ;;
            "")  info "未能从心跳行中解析出 REG 字段" ;;
            *)   warn "REG=$reg_val（不在标准 AT+CEREG 状态码 0~5 范围内，原始心跳行: $last_hb）" ;;
        esac
    else
        info "当前日志中没有 [HEARTBEAT] 记录 —— 若固件版本较老可能本来就没有这个标签，不代表故障"
    fi

    last_init_iccid=$(grep '\[INIT\] ICCID:' "$LATEST_LOG" 2>/dev/null | tail -1)
    [ -n "$last_init_iccid" ] && info "最近一次开机 ICCID: $last_init_iccid"
    last_init_fw=$(grep '\[INIT\] FW:' "$LATEST_LOG" 2>/dev/null | tail -1)
    [ -n "$last_init_fw" ] && info "最近一次开机固件版本: $last_init_fw（AP_FW/CP_FW 同理，-v 模式可看全部）"
else
    info "$LOG_ROOT 下未找到日志文件（SD卡未挂载/空间不足，见第9节），跳过驻网/INIT信息"
fi

# ------------------------------------------------------------------
sec "1. 进程存活"
# ------------------------------------------------------------------

dial_pids=$(pgrep -f "modem_mng" 2>/dev/null)
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
    ok "sw_mng 看门狗运行中（注意：只在 modem_mng 进程真正退出后才会拉起，进程卡死但存活时不会介入）"
else
    warn "sw_mng 未检测到 —— 进程一旦崩溃退出将不会被自动拉起"
fi

# ------------------------------------------------------------------
sec "1b. SDK 数据服务层（ql_netd / ql_rild / ql_atc）"
# ------------------------------------------------------------------
# 依据同 EC200A：固件 ql_procd_scripts.conf 启动 ql_rild/ql_netd/ql_atc。
#   ql_netd = 数据服务守护（ql_data_call_* 服务端，管 ccinet0~7、IPC=/tmp/.ql_net_ipc_path、
#             就绪标志=/tmp/ql_net_srv_ready.flag，依赖 ql_rild）；
#   ql_rild = RIL/CP 面；  ql_atc = AT 命令服务（独立）。
# 专治 "ql_data_call_init failed, ret=-1067(SERVICE_NOT_READY)"：已注册/AT能查/CP有IP 却 ping
# 不通，根因在这一层、非 SIM/账户。见 md/session/EC200A_数据服务未就绪_ping不通_根因调研_2026-07-14.md
# （现象与守护进程栈 EC200A/AG35 共用同一 SDK，判据一致。）

# 注意：以下路径/进程名来自 EC200A A02 SDK 固件实证，AG35 及别的固件版本可能不同。
# 故"标志缺失"只作 [WARN] 不作 [FAIL]，最终以第6节接口 IP、第8节 ping 为准；
# 进程存活判据（ql_netd/ql_rild）相对稳定，仍作 [FAIL]。
QL_NET_READY_FLAG="/tmp/ql_net_srv_ready.flag"
QL_NET_IPC_PATH="/tmp/.ql_net_ipc_path"

if pgrep -f "ql_netd" >/dev/null 2>&1; then
    ok "ql_netd 数据服务守护在跑"
    if [ -e "$QL_NET_READY_FLAG" ]; then
        ok "$QL_NET_READY_FLAG 存在（数据服务已就绪）"
    else
        warn "$QL_NET_READY_FLAG 缺失 —— 可能 ql_netd 在跑但未就绪(卡等 ql_rild，会致 ql_data_call_init SERVICE_NOT_READY)；也可能此固件版本标志路径不同(A02 实证)。以接口 IP/ping 结果为准"
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
    info "ql_atc（AT 命令服务）在跑 —— 独立于数据服务，故 AT 能查 IMEI/COPS 不代表数据面正常"
else
    warn "ql_atc 未检测到 —— 连 AT 查询也可能异常，比数据服务问题更靠底层"
fi

# ------------------------------------------------------------------
sec "2. 配置文件完整性"
# ------------------------------------------------------------------
# AG35 没有 Roamlink/policy/network.ini（EG25G-only），本节核对 apn.json + dualsim.json。

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
    info "$APN_JSON 不存在 —— 不是致命问题：apn_get_apn_obj() 找不到条目时退化为自动/默认 APN，部分运营商能自动选对，但建议确认这张卡是否需要专用 APN"
fi

if [ -f "$DUALSIM_JSON" ]; then
    if have python3; then
        if python3 -c "import json,sys; json.load(open('$DUALSIM_JSON'))" >/dev/null 2>&1; then
            ok "$DUALSIM_JSON 存在且 JSON 格式合法"
        else
            warn "$DUALSIM_JSON 存在但 JSON 解析失败 —— slot_mgr_load_config() 解析失败时保留内置默认值、不碰文件，不是致命故障，但你在这个文件里做的任何修改都不会生效"
        fi
    else
        ok "$DUALSIM_JSON 存在（未装 python3，跳过严格 JSON 校验）"
    fi
else
    info "$DUALSIM_JSON 不存在 —— 首次启动会自动按内置默认值播种一份（enable=1，即双卡功能默认开），不是故障"
fi

# ------------------------------------------------------------------
sec "3. 运行状态文件"
# ------------------------------------------------------------------

if [ -f "$DIAL_STATUS_LEGACY" ]; then
    v=$(cat "$DIAL_STATUS_LEGACY" 2>/dev/null)
    age=$(age_of "$DIAL_STATUS_LEGACY")
    case "$v" in
        0) ok "$DIAL_STATUS_LEGACY = 0（已连接），更新于 ${age}s 前" ;;
        1) fail "$DIAL_STATUS_LEGACY = 1（未连接/断开），更新于 ${age}s 前" ;;
        *) warn "$DIAL_STATUS_LEGACY 内容异常: '$v'" ;;
    esac
else
    if [ "$pid_cnt" -gt 0 ] 2>/dev/null; then
        warn "$DIAL_STATUS_LEGACY 不存在，但进程在跑 —— 数据呼叫状态回调从未触发过，结合第6/8节的 IP/ping 结果判断"
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
        if { [ "$v" = "0" ] && [ "$ns" != "1" ]; } || { [ "$v" = "1" ] && [ "$ns" != "0" ]; }; then
            warn "$DIAL_STATUS_LEGACY=$v 与 $NET_STATUS=$ns 不一致（两者 0/1 含义相反：dial_Status 0=连接，network_status 1=联网）—— 瞬时不一致可能是两处写入时机差，持续不一致才需关注"
        fi
    fi
else
    info "$NET_STATUS 不存在"
fi

if [ -f "$RETRY_COUNT_FILE" ]; then
    rc=$(cat "$RETRY_COUNT_FILE" 2>/dev/null)
    if [ -n "$rc" ] && [ "$rc" -ge "$MAX_FAST_RETRY_TIMES" ] 2>/dev/null; then
        info "$RETRY_COUNT_FILE=$rc（达到快速失败上限 $MAX_FAST_RETRY_TIMES，已放弃快速退出策略，转入持续运行模式，这是正常降级不是故障）"
    elif [ -n "$rc" ]; then
        warn "$RETRY_COUNT_FILE=$rc（<$MAX_FAST_RETRY_TIMES）—— 说明最近几次启动在10秒内都没能 ping 通就退出了，处于快速失败重试阶段，应关注是不是反复重启"
    fi
else
    info "$RETRY_COUNT_FILE 不存在（正常：首次启动，或已成功联网后被清零）"
fi

if [ -f "$CFUN_COUNT_FILE" ]; then
    cc=$(cat "$CFUN_COUNT_FILE" 2>/dev/null)
    info "$CFUN_COUNT_FILE=$cc —— 注意：这个计数器只统计经 restart_cfun_safe() 走的 CFUN 调用，当前 L2 恢复分级是直接调 AT+CFUN=0/1、不经过这个计数器，所以这个数字会低估实际 CFUN 复位次数；AG35 切卡（slot_mgr_switch_phy）也会发 CFUN 0/1 但同样不经过这个计数器，不能拿它当唯一依据判断'CFUN是不是被频繁调用'"
fi
if [ -f "$CFUN_LAST_CALL_FILE" ]; then
    lc=$(cat "$CFUN_LAST_CALL_FILE" 2>/dev/null)
    info "$CFUN_LAST_CALL_FILE=$lc（uptime秒数，同样只覆盖 restart_cfun_safe() 路径，最小间隔 ${MIN_CFUN_INTERVAL_SEC}s，见上方说明）"
fi

# ------------------------------------------------------------------
sec "4. 双卡单待 (DSSS, ec200a/slot/slot_mgr.c) —— AG35 独有"
# ------------------------------------------------------------------
# 命门：SDK 看不到非激活槽，探测需要切卡（CFUN 0/1，~6-8s 数据中断），所以本节
# 全部只读现有落盘文件/日志，不做任何探测性切卡。

if [ -n "$LATEST_LOG" ] && [ -f "$LATEST_LOG" ]; then
    slot_init=$(grep -m1 '\[SLOT\] init:' "$LATEST_LOG" 2>/dev/null)
    if [ -n "$slot_init" ]; then
        info "开机生效配置: $slot_init"
        case "$slot_init" in
            *"enable=0"*) info "双卡功能当前是关闭状态（enable=0），本节后续检查仍会跑，但语义上这台设备当前按单卡逻辑工作" ;;
        esac
    else
        info "当前日志中没有 [SLOT] init: 行 —— 若这是 EC200A 而非 AG35 固件（版本号 1.31.x），本来就不会有；若确认是 AG35 但没有这行，本节其余判断请谨慎参考"
    fi

    # dualsim.json 加载状态：slot_mgr_load_config() 四种分支各有独立日志
    # （slot_mgr.c:166,176,178,209,211），缺文件自播种/缺键合并补写都是正常预期行为，
    # 只有 parse fail 才是需要关注的（会导致运维在文件里改的值全部不生效、静默回退默认）。
    cfg_seed=$(grep -c 'absent -> seeded defaults' "$LATEST_LOG" 2>/dev/null)
    [ "$cfg_seed" -gt 0 ] 2>/dev/null && info "日志显示本次运行曾自动播种过 dualsim.json（文件缺失时的正常首次行为）"
    cfg_merge=$(grep -m1 'missing key(s) merged into dualsim.json' "$LATEST_LOG" 2>/dev/null)
    [ -n "$cfg_merge" ] && info "配置合并补写: $cfg_merge（老文件升级后自动补齐新键，不影响已有值）"
    cfg_parse_fail=$(grep -c 'dualsim.json.*parse fail, use defaults\|parse fail.*use defaults' "$LATEST_LOG" 2>/dev/null)
    if [ "$cfg_parse_fail" -gt 0 ] 2>/dev/null; then
        fail "日志显示 dualsim.json 解析失败、本次运行全程使用内置默认值 —— 检查 $DUALSIM_JSON 是否被手工改坏（JSON 语法错误），第2节的 JSON 校验结果供交叉核对"
    fi
else
    info "未定位到当前日志文件，跳过 [SLOT] init 配置核对"
fi

if [ -f "$SIM_SLOT_FILE" ]; then
    phy=$(cat "$SIM_SLOT_FILE" 2>/dev/null)
    case "$phy" in
        1) info "/tmp/sim_slot = 1（当前驻留 eSIM，PHY_SLOT_1，2026-06-26 真机核实为焊死卡）" ;;
        2) info "/tmp/sim_slot = 2（当前驻留可插拔物理卡，PHY_SLOT_2）" ;;
        *) warn "/tmp/sim_slot 内容异常: '$phy'" ;;
    esac
else
    info "$SIM_SLOT_FILE 不存在 —— 尚未完成过一次冷启动择优/快启动命中，或双卡功能关闭"
fi

if [ -f "$SIM_SLOT_STATUS_JSON" ]; then
    status_age=$(age_of "$SIM_SLOT_STATUS_JSON")
    if have python3; then
        status_line=$(python3 -c "
import json,sys
try:
    d = json.load(open('$SIM_SLOT_STATUS_JSON'))
    print('phy=%s iccid=%s reason=%s switch_count=%s' % (d.get('phy'), d.get('iccid'), d.get('reason'), d.get('switch_count')))
except Exception as e:
    sys.exit(1)
" 2>/dev/null)
        if [ -n "$status_line" ]; then
            info "$SIM_SLOT_STATUS_JSON（${status_age}s 前）: $status_line"
            case "$status_line" in
                *reason=C6-card-absent*) warn "最近一次记录原因是 C6-card-absent（卡槽检测到卡缺失）——检查物理卡是否插好/损坏" ;;
                *reason=A-denied*) warn "最近一次记录原因是 A-denied（对面网络拒绝注册 REG=3）——可能是 SIM/账户问题，不是信号问题" ;;
                *reason=A-no-net*) info "最近一次记录原因是 A-no-net（无网触发切卡）" ;;
                *reason=B-data-dead*) info "最近一次记录原因是 B-data-dead（数据面死判定触发切卡）" ;;
            esac
        else
            warn "$SIM_SLOT_STATUS_JSON 解析失败或为空，可能读到了写入中途的内容（原子 tmp+rename 写正常不该出现，重跑一次确认）"
        fi
    else
        info "$SIM_SLOT_STATUS_JSON 存在（${status_age}s 前更新），未装 python3 跳过字段解析，可用 -v 看原文"
        [ "$VERBOSE" = "1" ] && cat "$SIM_SLOT_STATUS_JSON" 2>/dev/null
    fi
else
    info "$SIM_SLOT_STATUS_JSON 不存在 —— 尚未发生过一次完整的连通确认（slot_mgr_write_status 在 slot_mgr_mark_good 里被调用）"
fi

if [ -f "$SIM_LAST_GOOD_FILE" ]; then
    lg_content=$(cat "$SIM_LAST_GOOD_FILE" 2>/dev/null)
    lg_phy=$(echo "$lg_content" | awk '{print $1}')
    lg_ts=$(echo "$lg_content" | awk '{print $2}')
    if [ -n "$lg_ts" ]; then
        lg_age=$(( $(now_epoch) - lg_ts ))
        info "$SIM_LAST_GOOD_FILE: phy=$lg_phy, ${lg_age}s 前记录（默认 TTL 86400s=1天，超期 fast-boot 会放弃它、回退冷启动择优）"
    else
        warn "$SIM_LAST_GOOD_FILE 内容异常: '$lg_content'"
    fi
else
    info "$SIM_LAST_GOOD_FILE 不存在 —— 从未有过一次'最优驻留'确认，下次重启会走冷启动两槽择优（~30s+），不是故障；若这台设备已经稳定运行很久还是没有这个文件，检查 SD 卡是否只读/写满（slot_mgr_mark_good 写失败会在日志留 [SLOT] mark last-good ... FAILED，见下方日志检查）"
fi

if [ -n "$LATEST_LOG" ] && [ -f "$LATEST_LOG" ]; then
    lg_fail_cnt=$(grep -c '\[SLOT\] mark last-good.*FAILED' "$LATEST_LOG" 2>/dev/null)
    if [ "$lg_fail_cnt" -gt 0 ] 2>/dev/null; then
        fail "当前日志中有 $lg_fail_cnt 条 last-good 写入失败记录 —— 每次开机都会退化为全量冷启动择优（~30s+），常见原因是 SD 卡只读/写满/未挂载"
    fi
    # 卡槽切换总数：含冷启动择优/快启动探测/故障触发/回探迁移等全部切换，不代表故障次数。
    switch_cnt=$(grep -c '\[SLOT\] switch to' "$LATEST_LOG" 2>/dev/null)
    [ "$switch_cnt" -gt 0 ] 2>/dev/null && info "当前日志中有 $switch_cnt 次卡槽切换记录（含冷启动/快启动探测，不等于故障切换次数，故障触发次数见下方 trigger 记录）"

    # 故障触发的强制切卡：dial.cpp 独立于 L1/L2/L3 的切卡触发器，命中时打
    # "[SLOT] trigger(reason): down %lds REG=%d CSQ=%d count=%d/%d -> switch card"
    # （dial.cpp:1477），reason ∈ {C6-card-absent, A-denied, A-no-net, B-data-dead}——
    # 这是"这次开机因为故障被迫切了几次卡"的精确计数，比上面的总切换数更有诊断价值。
    trigger_cnt=$(grep -c '\[SLOT\] trigger(' "$LATEST_LOG" 2>/dev/null)
    if [ "$trigger_cnt" -gt 0 ] 2>/dev/null; then
        warn "当前日志中有 $trigger_cnt 次故障触发的强制切卡（[SLOT] trigger(...)）"
        if [ "$VERBOSE" = "1" ]; then
            echo "---- 切卡触发记录 ----"
            grep '\[SLOT\] trigger(' "$LATEST_LOG" 2>/dev/null | tail -10
            echo "----------------------"
        fi
    else
        ok "当前日志中无故障触发的强制切卡记录"
    fi
    trigger_fail=$(grep -c '\[SLOT\] switch failed ret=.*budget still consumed' "$LATEST_LOG" 2>/dev/null)
    [ "$trigger_fail" -gt 0 ] 2>/dev/null && fail "当前日志中有 $trigger_fail 次故障触发切卡本身失败（预算已消耗但切换未成功）—— 检查目标槽是否有卡"

    fallback_cnt=$(grep -c '\[SLOT\] residency=fallback' "$LATEST_LOG" 2>/dev/null)
    if [ "$fallback_cnt" -gt 0 ] 2>/dev/null; then
        warn "当前日志中有 $fallback_cnt 次'非最优驻留(强制切换)'记录 —— 当前可能停留在兜底卡上而不是最优卡，若配置了 probe_back_interval_sec>0 会周期性盲回探，否则要等下次重启才会重新择优"
    fi

    # 切槽失败：slot_mgr_switch_phy() 两种失败——ql_sim_switch_slot 本身失败(SDK/硬件层)，
    # 或切过去后卡迟迟不 READY(缺卡/未开通)。两者都比"数据不通"更底层，值得单独标出来。
    switch_fail_cnt=$(grep -c '\[SLOT\] switch_slot FAIL' "$LATEST_LOG" 2>/dev/null)
    [ "$switch_fail_cnt" -gt 0 ] 2>/dev/null && fail "当前日志中有 $switch_fail_cnt 次 switch_slot FAIL（ql_sim_switch_slot 本身调用失败，SDK/硬件层问题，比数据不通更底层）"
    not_ready_cnt=$(grep -c '\[SLOT\] new card not READY' "$LATEST_LOG" 2>/dev/null)
    [ "$not_ready_cnt" -gt 0 ] 2>/dev/null && warn "当前日志中有 $not_ready_cnt 次切槽后新卡未就绪记录（ABSENT/未开通）—— 检查对应物理槽是否有卡/卡是否已开通"

    # 恢复归因：网络恢复是"切卡救回来的"还是"原卡自己好的"，dial.cpp 会分别打不同的
    # [EVENT]/[INFO] 文案（见 dial.cpp:1197,1263），不看这个会把换卡救回来的误读成"原卡自愈"。
    recov_by_switch=$(grep -c 'Recovered via card-switch\|active SIM READY after card-switch' "$LATEST_LOG" 2>/dev/null)
    [ "$recov_by_switch" -gt 0 ] 2>/dev/null && info "当前日志中有 $recov_by_switch 次'切卡后恢复'记录 —— 这些恢复是换卡救回来的，不是原卡/原网络自愈，若一直靠切卡续命，原卡本身的问题(欠费/信号/被拒)没解决"

    if [ "$VERBOSE" = "1" ]; then
        echo "---- 最近的 [SLOT] 记录 ----"
        grep '\[SLOT\]' "$LATEST_LOG" 2>/dev/null | tail -30
        echo "----------------------------"
    fi

    # 开机路径结果：fast-boot 命中(HIT)可省掉~30s+的冷启动择优；未命中/无 last-good 会
    # 落 cold_select，能看到两槽评分对比。这两条决定了本次开机走的是哪条路径、耗时多少。
    fastboot_hit=$(grep -c 'fast-boot: HIT' "$LATEST_LOG" 2>/dev/null)
    fastboot_fallback=$(grep -c 'fast-boot:.*fall back to cold select' "$LATEST_LOG" 2>/dev/null)
    if [ "$fastboot_hit" -gt 0 ] 2>/dev/null; then
        info "本次开机 fast-boot 命中过 last-good 槽，跳过了两槽冷启动择优"
    elif [ "$fastboot_fallback" -gt 0 ] 2>/dev/null; then
        fb_reason=$(grep -m1 'fast-boot:.*fall back to cold select' "$LATEST_LOG" 2>/dev/null)
        info "本次开机 fast-boot 未命中，落到了冷启动两槽择优: $fb_reason"
    fi
    cold_result=$(grep -m1 '\[SLOT\] cold select result:' "$LATEST_LOG" 2>/dev/null)
    [ -n "$cold_result" ] && info "冷启动择优结果: $cold_result"
fi

# ------------------------------------------------------------------
sec "5. 海外选网 (OPER, ec200a/oper/oper.cpp) —— AG35 独有"
# ------------------------------------------------------------------
# 强调：oper_select 的核心选网轮(AT+COPS=1 逐家试)从未在真机上跑通验证过——生产环境
# 应保持 oper_select=0。本节检查到 oper_select=1 时会明确标出来，不是常规 WARN。

if [ -f "$DUALSIM_JSON" ] && have python3; then
    oper_sel=$(python3 -c "
import json
try:
    d = json.load(open('$DUALSIM_JSON'))
    print(d.get('oper_select', 'missing'))
except Exception:
    print('parse_error')
" 2>/dev/null)
    case "$oper_sel" in
        1) fail "dualsim.json 里 oper_select=1（已启用海外选网）—— 这条功能的核心选网轮(COPS=1逐家扫)从未经过真机验证，CLAUDE.md 明确要求生产环境保持 oper_select=0，请立刻确认这是不是有意为之的测试机" ;;
        0) ok "dualsim.json 里 oper_select=0（海外选网关闭，符合生产环境要求）" ;;
        missing) info "dualsim.json 里没有 oper_select 键 —— 缺键时 slot_mgr_load_config() 按 0=关处理，等价于关闭" ;;
        parse_error) info "dualsim.json 解析失败，跳过 oper_select 取值核对（第2节已报告 JSON 格式问题）" ;;
    esac
else
    info "跳过 dualsim.json 的 oper_select 取值核对（文件不存在或未装 python3），改看日志里的 [OPER] 初始化行"
fi

if [ -n "$LATEST_LOG" ] && [ -f "$LATEST_LOG" ]; then
    oper_init_line=$(grep -m1 'overseas operator-select (dualsim.json oper_select)' "$LATEST_LOG" 2>/dev/null)
    [ -n "$oper_init_line" ] && info "开机生效状态: $oper_init_line"

    # oper_init() 失败(ql_atc_init/ql_nw_init)会导致 g_oper_select_enable 被打回 0
    # （dial.cpp:775），功能虽然在配置里开着但实际静默降级禁用了，日志是唯一能看出来的地方。
    oper_init_fail=$(grep -c '\[OPER\] ql_atc_init failed\|\[OPER\] ql_nw_init failed' "$LATEST_LOG" 2>/dev/null)
    if [ "$oper_init_fail" -gt 0 ] 2>/dev/null; then
        warn "当前日志中有 $oper_init_fail 条 oper_init 失败记录 —— 即使 dualsim.json 里 oper_select=1，功能也已被静默降级禁用（dial.cpp 里失败后会把 g_oper_select_enable 打回0），不是配置问题，是 SDK/AT通道层问题"
    fi

    # COPS 自动恢复：连网稳定10min后触发（dial.cpp:1310附近），触发线和worker实际执行线是两条：
    # 前者是"决定要恢复了"，后者（下面 restore_line）是 worker 真正查完模式、按需发 COPS=0。
    stable_restore=$(grep -m1 'connection stable 10min, restoring automatic selection' "$LATEST_LOG" 2>/dev/null)
    [ -n "$stable_restore" ] && info "触发过10min稳定后的自动选网恢复: $stable_restore"

    # COPS 自动恢复 worker：进程重启后清理上次可能残留的手动选网模式（AT+COPS?查到mode=1才动作）。
    restore_line=$(grep -m1 '\[OPER\] restore COPS=0' "$LATEST_LOG" 2>/dev/null)
    [ -n "$restore_line" ] && info "本次运行触发过 COPS 自动恢复: $restore_line（说明上次进程退出时模组处于手动选网残留状态）"

    round_start=$(grep -c '\[OPER\] selection round start' "$LATEST_LOG" 2>/dev/null)
    if [ "$round_start" -gt 0 ] 2>/dev/null; then
        warn "当前日志中有 $round_start 次选网轮触发记录 —— 触发条件是 g_oper_select_enable 开 && 本次开机从未连上 && 断网>10min；出现意味着设备处于'从未连过网+长时间断网'状态，这本身就是需要关注的信号，不只是选网功能本身"
        if [ "$VERBOSE" = "1" ]; then
            echo "---- 最近的 [OPER] 记录 ----"
            grep '\[OPER\]' "$LATEST_LOG" 2>/dev/null | tail -30
            echo "----------------------------"
        fi
    fi
    selected_cnt=$(grep -c '\[OPER\] selected' "$LATEST_LOG" 2>/dev/null)
    [ "$selected_cnt" -gt 0 ] 2>/dev/null && info "当前日志中有 $selected_cnt 次 COPS=1 手动选中记录（等待 ping 验证是否真的可用）"
    exhausted_cnt=$(grep -c 'all.*tries failed, COPS=0 to clear manual residue' "$LATEST_LOG" 2>/dev/null)
    [ "$exhausted_cnt" -gt 0 ] 2>/dev/null && warn "当前日志中有 $exhausted_cnt 次选网轮扫完全部候选仍失败、已发 COPS=0 恢复自动选网的记录 —— 说明扫过的运营商都注册不上，问题大概率不在软件层面"
else
    info "未定位到当前日志文件，跳过 [OPER] 记录检查"
fi

# ------------------------------------------------------------------
sec "6. 网络接口与路由"
# ------------------------------------------------------------------

# AG35 共用 dialer_ec200a.cpp（无条件 wait_for_interface("ccinet0")），且 AG35 是真机可用
# 平台 → 应用层数据网卡是 ccinet0~7，不是 rmnet*。注意：AG35 硬件为高通系（MDM），底层
# 可能另有 rmnet_data*，但本节要看的是应用数据面 ccinet。（改自代码事实，AG35 尤其建议真机
# `ip link` 复核确认 ccinet 而非 rmnet。）
if have ip; then
    data_if=$(ip -o link show 2>/dev/null | awk -F': ' '{print $2}' | grep '^ccinet')
elif have ifconfig; then
    data_if=$(ifconfig -a 2>/dev/null | grep -o '^ccinet[a-zA-Z0-9_]*')
fi

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
sec "7. rx_packets 增长"
# ------------------------------------------------------------------

# AG35 数据网卡 ccinet*，统计走标准 netdev sysfs（/sys/class/net/ccinet*/statistics）。
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
sec "8. 连通性测试（ping 为主，TCP 为辅）"
# ------------------------------------------------------------------

if have ping; then
    if ping -c 3 -W 2 "$PING_TEST_HOST" >/dev/null 2>&1; then
        ok "ping $PING_TEST_HOST（与 modem_mng 内部判据相同）连通"
    else
        fail "ping $PING_TEST_HOST 不通 —— 与 modem_mng 内部判据同一目标"
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
                fail "TCP 探测两个目标也都不通 —— 指向设备本身没有出网能力（先排除是不是当前驻留卡本身的问题，见第4节）"
            fi
        else
            info "未找到 nc 且 /dev/tcp 不受支持，跳过 TCP 补充验证"
        fi
    fi
else
    info "未找到 ping 命令，跳过"
fi

# ------------------------------------------------------------------
sec "9. 日志分析 / 故障快照"
# ------------------------------------------------------------------

if [ -d "$SDCARD_DIR" ]; then
    if have df; then
        free_kb=$(df -k "$SDCARD_DIR" 2>/dev/null | awk 'NR==2{print $4}')
        if [ -n "$free_kb" ] && [ "$free_kb" -lt 1048576 ] 2>/dev/null; then
            warn "$SDCARD_DIR 剩余空间 <1GB —— 可能导致日志写入受限，也会影响 sim_last_good 的持久化（第4节）"
        fi
    fi
else
    info "$SDCARD_DIR 不存在 —— 日志不会被写入，sim_last_good 也无法持久化，本节分析无法进行"
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
        ok "当前日志中无 [RECOVERY Lx] 记录（未触发过分级恢复）"
    fi

    err_cnt=$(grep -Ec '\[ERROR\]|\[FATAL\]|\[WARNING\]' "$LATEST_LOG" 2>/dev/null)
    if [ "$err_cnt" -gt 0 ] 2>/dev/null; then
        warn "当前日志中有 $err_cnt 条 [ERROR]/[FATAL]/[WARNING] 记录"
        [ "$VERBOSE" = "1" ] && grep -E '\[ERROR\]|\[FATAL\]|\[WARNING\]' "$LATEST_LOG" 2>/dev/null | tail -20
    else
        ok "当前日志中无 [ERROR]/[FATAL]/[WARNING] 记录"
    fi

    # 数据服务 init 失败（2026-07-14 EC200A 现场案例特征，AG35 同栈同样适用）：
    dcinit_fail=$(grep -Ec 'data_call_init failed|SERVICE_NOT_READY|Data service did not init this session' "$LATEST_LOG" 2>/dev/null)
    if [ "$dcinit_fail" -gt 0 ] 2>/dev/null; then
        fail "日志出现 ql_data_call_init 失败 / SERVICE_NOT_READY（$dcinit_fail 次）—— AP 侧数据服务(ql_netd)本次没起来，数据面断在 SDK 服务层、与 SIM/账户/选网无关；对照第 1b 节 ql_netd/ql_rild 状态定位"
        [ "$VERBOSE" = "1" ] && grep -E 'data_call_init failed|SERVICE_NOT_READY|Data service did not init this session' "$LATEST_LOG" 2>/dev/null | tail -10
    fi

    gated=$(grep -c 'never-connected, policy recovery' "$LATEST_LOG" 2>/dev/null)
    if [ "$gated" -gt 0 ] 2>/dev/null; then
        if [ "$dcinit_fail" -gt 0 ] 2>/dev/null; then
            warn "日志出现 'never-connected ... gated'，但同时有数据服务 init 失败 —— 这是数据服务层故障（查第 1b 节 ql_netd/ql_rild/CP），先别往 SIM/选网方向查"
        else
            info "日志中出现过 'never-connected, policy recovery gated'，且未见数据服务 init 失败 —— 设备从未成功联网、数据服务本身正常，结合第5节看是否已经/该触发海外选网"
        fi
    fi
else
    info "未定位到当前日志文件，跳过日志内容分析"
fi

if [ -d "$DIAG_SNAP_DIR" ]; then
    last_snap=$(ls -t "$DIAG_SNAP_DIR" 2>/dev/null | head -1)
    if [ -n "$last_snap" ]; then
        snap_age=$(age_of "$DIAG_SNAP_DIR/$last_snap")
        info "最近一次故障快照(dmesg/logcat): $last_snap（${snap_age}s 前）"
    else
        info "$DIAG_SNAP_DIR 目录存在但为空 —— 尚未触发过故障快照"
    fi
else
    info "$DIAG_SNAP_DIR 不存在 —— 若固件较老可能本来就没有这个能力，不代表故障"
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
