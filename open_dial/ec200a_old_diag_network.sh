#!/bin/sh
#
# ec200a_old_diag_network.sh — open_dial（单 SIM 通道版，EC200A/EG25）现场联网问题一键诊断脚本
#
# ==================== 用法 ====================
#   把这个文件拷到设备上任意可写目录（如 /usrdata/），然后执行：
#     sh ec200a_old_diag_network.sh          # 普通模式
#     sh ec200a_old_diag_network.sh -v       # 详细模式：额外打印最近日志 ERROR/WARNING/FATAL 明细
#
#   全程只读（不改任何文件、不重启进程、不发 AT 指令），可以随时在
#   正在跑的设备上执行，不影响 dial 运行。用到的 nc/ip/python3/ping
#   等工具缺失时会自动跳过对应检查并提示，不会报错中断。
#
# ==================== 怎么看输出 ====================
#   每行前缀代表判定结果：
#     [OK]    正常，不用管
#     [WARN]  有异常，但未必是当前故障的根因，或处于"临界"状态，值得留意
#     [FAIL]  明确问题，建议优先处理
#     [--]    纯信息展示，不参与判定（比如打印一个原始字段值）
#
#   跑完最后会打印一行 PASS=x WARN=y FAIL=z 汇总和一句话结论，照这个看就行。
#   注意：脚本进程本身的退出码固定是 0（表示"跑完了"），不代表设备网络健康，
#   不要用 $? 判断结果，只看 PASS/WARN/FAIL 那行。
#
# ==================== 设计原则 ====================
#   1. 只读，不写。不修改 /tmp/network_*、不重启进程、不发 AT 指令。
#      AT 口由 dial 进程内部串口访问逻辑独占使用，外部脚本并发发 AT 指令
#      拿不到内部保护，只会和 dial 抢串口，可能造成拨号状态机误判超时。
#      所以本脚本完全不碰串口，只读 dial 已经写出来的状态文件/日志。
#   2. 目标是 busybox ash，不用 bash 专有语法，工具用之前先 command -v 探测，
#      缺了就跳过并提示。
#   3. 每一项检查的判断依据都来自 open_dial 源码里的真实行为（状态机、常量、
#      文件格式），不是通用网络排查套路。本脚本针对当前仓库（单 SIM 通道，
#      无 roamlink/双通道切换）编写，不要拿去套用有双通道逻辑的分支/项目。
#   4. 这个仓库里好几个 /tmp 状态文件是历史遗留的死代码（写入函数从未被
#      调用），脚本里会明确标注，避免"文件不存在/不更新"被误判为故障。
#

DIAL_PROC="dial"                    # pgrep -x 精确匹配的进程名（Makefile DIAL_TARGET_EXE）
DIAL_VERSION="/tmp/dial_version"
NET_STATUS="/tmp/network_status"    # update_network_status() 写，1=已连接 0=未连接（可靠，多处调用）
DIAL_STATUS_INV="/tmp/dial_Status"  # 注意大写S，且语义与上面相反：0=已连接 1=未连接（data_call.c 里 system(echo) 直写）
NET_CSQ="/tmp/network_csq"          # 写入函数 get_signal_strength() 全仓库无调用点，视为死代码/不可信
CALLID_FILE="/tmp/callid"           # 读写函数全仓库无调用点，视为死代码
RETRY_COUNT_FILE="/tmp/dial_retry_count"   # 启动 Fast-Fail 计数，成功 ping 通后删除
CFUN_COUNT_FILE="/tmp/cfun_count.txt"      # 开机 exit_count>=20 时的 CFUN 限频计数（生命周期=本次开机）
CFUN_TIME_FILE="/tmp/cfun_last_call.txt"   # 同上，配合 MIN_INTERVAL=600s 使用
EXIT_COUNT_FILE="/tmp/exit_count.txt"      # 当前代码只在启动时读取并在>=20时清零，未发现任何自增写入点
SDCARD_AVL="/tmp/sdcard_avl"        # 仅在 SD 卡不可用时被写为 000，可用时不会写任何"正常"标记
APN_JSON="/usr/dial/apn.json"
DIAL_CONF="/usrdata/dial_config.txt"       # reboot_conf: "uptime,restart_flag"，仅启动时读写一次
SDCARD_DIR="/media/sdcard"
LOG_ROOT="/media/sdcard/dial_log"
DIAG_SNAP_DIR="$LOG_ROOT/dial_snap"        # 故障/恢复时的 dmesg+logcat 快照
CP_DUMP_DIR="/sdcard/modem_dump"           # CP(协议栈)崩溃 dump，仅 bind mount 后可见
STARTUP_IF="ecm0"                          # main.c 启动时等待的网卡，只是就绪门槛，不是 PDP 数据接口
DEFAULT_PDP_IF="ccinet0"                   # PDP 数据接口默认名（NOARP/UNSPEC，源码注释确认），实际以日志 IF= 为准
PING_HOST="8.8.8.8"                        # 与 misc.c:test_can_ping_google() 内部探测目标一致

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

hr
echo "open_dial 联网问题诊断脚本（单 SIM 通道版）  $(date '+%Y-%m-%d %H:%M:%S')"
hr

# ------------------------------------------------------------------
# 机型护栏（先于一切检查）：本脚本只适用于 open_dial 目标机
#   —— aarch64 + ql-sdk(libql_sdk.so) + ECM(ecm0/ccinet0)。
# 现场踩过坑：脚本被误跑到"根云 LI1526-DC-T-GL PRO / Quectel OpenLinux"车载盒上，
# 那台是高通 EG25/EC20 家族(MDM9207, armv7, 内核3.18)，/usr/dial/dial 是厂商基于
# ql-ol-sdk 编的 QMI/QCMAP 拨号程序(版本串 "DIAL Version TEST2:")，走 rmnet+bridge0
# 上网，与本项目毫无关系。在这种机器上继续跑，所有基于 ecm0/ccinet0/ql-sdk 的检查
# 会全部误报 FAIL(实际网络/业务正常)。故这里先做机型识别，命中"异类栈"就说明并退出。
# 判据(命中任一即认定非目标机)：CPU非aarch64 / dial二进制指纹(ql-ol-sdk等) / 只有rmnet没有ecm。
# ------------------------------------------------------------------
foreign_hits=""
arch=$(uname -m 2>/dev/null)
case "$arch" in
    armv6*|armv7*|arm) foreign_hits="${foreign_hits}CPU=$arch(非aarch64) " ;;
esac
case "$(head -1 /proc/version 2>/dev/null)" in
    *"version 3.18"*) foreign_hits="${foreign_hits}内核3.18(OpenLinux) " ;;
esac
if grep -qiE 'Qualcomm|MDM9[0-9]{3}|MSM[0-9]' /proc/cpuinfo 2>/dev/null; then
    foreign_hits="${foreign_hits}CPU=高通MDM/MSM "
fi
if have strings && [ -f /usr/dial/dial ]; then
    if strings /usr/dial/dial 2>/dev/null \
        | grep -qiE 'ql-ol-sdk|qcmap|wireless_data_service|DIAL Version TEST2|EC20[CF]|MDM9x07'; then
        foreign_hits="${foreign_hits}dial二进制指纹(ql-ol-sdk/QMI) "
    fi
fi
# 走 rmnet 且不存在 ecm0/ccinet0：QMI 机型旁证（本项目用 ECM，ecm0 一定在）
if [ -d /sys/class/net/rmnet_data0 ] && \
   [ ! -d /sys/class/net/ecm0 ] && [ ! -d /sys/class/net/ccinet0 ]; then
    foreign_hits="${foreign_hits}联网走rmnet无ecm0 "
fi

if [ -n "$foreign_hits" ]; then
    warn "机型不匹配：本机疑似【非 open_dial 目标机】，命中特征: $foreign_hits"
    info "本脚本只适用于 open_dial 目标机(aarch64 + ql-sdk + ECM, ecm0/ccinet0)。"
    info "当前特征指向 Quectel OpenLinux QMI 机型(如 根云 LI1526 / EG25/EC20：armv7 +"
    info "内核3.18 + ql-ol-sdk + QMI/QCMAP + rmnet/bridge0)，其 /usr/dial/dial 是厂商另一套"
    info "dial(版本串 \"DIAL Version TEST2:\")，与本项目无关。继续跑所有 ECM 检查都会误报 FAIL。"
    info "→ 判定：此机非本项目目标机，诊断不适用，已退出(网络/业务未必有问题)。"
    hr
    echo "结论：机型不匹配，未执行 open_dial 诊断。"
    hr
    exit 0
fi

# 找出当前所有 dial_*.log（按天分文件夹存放，可能跨多个文件夹）。
# 提到最前面统一算一次，第0节(模组固件信息)和第4/7节(PDP接口/故障日志)都要用，
# 避免同样的 find 跑两遍。
# 注意：不能用 `find ... | xargs ls -t`——find 空结果时 xargs 仍会跑一次无参数的
# `ls -t`，那样会退化成列出当前目录，把无关文件误当成日志（已踩坑验证过）。
log_candidates=$(find "$LOG_ROOT" -name 'dial_*.log' 2>/dev/null)
last_pdp_log=""
if [ -n "$log_candidates" ]; then
    last_pdp_log=$(ls -t $log_candidates 2>/dev/null | head -1)
fi

# ------------------------------------------------------------------
# 版本号解析 + 比较：本仓库从 2023-03（首次提交）到 2025-12（3级恢复重构 e92733c）
# 有近 2.7 年没有 L1/L2/L3 分级恢复、没有 [HEARTBEAT]/[ALARM]/[RECOVERY Lx] 日志 tag；
# [FATAL] SDK崩溃诊断是 v1.27.1（2026-05-12）才加的；diag快照/CP dump 是 v1.27.7
# （2026-06-01）才加的；has_connected_once 门控是 v1.28.3（2026-06-08）才加的。
# 若设备跑的是这些节点之前的老固件，第7节里"日志中无 [FATAL]/[ALARM]"这类判断
# 不能证明"健康"，只能说明"这个版本还没有这个诊断能力"——必须先弄清楚实际
# 版本号再解读，本节以下就是做这件事。
# 版本文件格式也变过：v1.27.0（977f204）之前是两段式 "Version: X.YY"，之后
# 才是三段式 "Version: X.YY.Z"，下面解析函数对两种格式都兼容（缺 patch 按0算）。
# ------------------------------------------------------------------
version_ge() {
    # 粗略数值比较，缺 patch 段按0处理；非数字/解析失败时返回失败（不下结论）
    v1=$(echo "$1" | awk -F. '{ if (NF>=2) printf "%d%03d%03d", $1+0,$2+0,$3+0 }' 2>/dev/null)
    v2=$(echo "$2" | awk -F. '{ if (NF>=2) printf "%d%03d%03d", $1+0,$2+0,$3+0 }' 2>/dev/null)
    [ -n "$v1" ] && [ -n "$v2" ] && [ "$v1" -ge "$v2" ] 2>/dev/null
}

dial_ver_num=""
has_l123_recovery=""      # v1.26+：3级恢复机制 + HEARTBEAT/ALARM/RECOVERY 日志tag
has_fatal_tag=""          # v1.27.1+：[FATAL] SDK崩溃诊断
has_diag_snapshot=""      # v1.27.7+：diag.c 故障快照 + CP dump
has_ap_cp_fw=""           # v1.27.5+：AT*ZCGMR/AT*CGMR 的 AP/CP 固件打印（d0bb476）

sec "0. 版本信息"
if [ -f "$DIAL_VERSION" ]; then
    dial_ver=$(cat "$DIAL_VERSION" 2>/dev/null | tr -d '\r\n')
    dial_ver_num=$(echo "$dial_ver" | sed -n 's/^Version: *//p')
    [ -z "$dial_ver_num" ] && dial_ver_num="$dial_ver"
    info "当前运行版本: $dial_ver"
    if version_ge "$dial_ver_num" "1.26.0"; then
        has_l123_recovery=1
    else
        warn "版本号解析为 '$dial_ver_num'，低于/无法确认 v1.26 —— 本仓库 v1.26（2025-12-11 e92733c）才引入 L1/L2/L3 分级恢复及 [HEARTBEAT]/[ALARM]/[RECOVERY Lx] 日志tag，这之前的固件完全没有分级恢复机制，第7节相关判断会自动降级为'无法确认'而不是'正常'"
    fi
    version_ge "$dial_ver_num" "1.27.1" && has_fatal_tag=1
    version_ge "$dial_ver_num" "1.27.7" && has_diag_snapshot=1
    version_ge "$dial_ver_num" "1.27.5" && has_ap_cp_fw=1
    if [ -n "$has_l123_recovery" ] && [ -z "$has_fatal_tag" ]; then
        info "版本低于 v1.27.1 —— 该版本才加 [FATAL] SDK服务崩溃诊断，此前版本没有这个 tag，不代表没发生过 SDK 崩溃"
    fi
    if [ -n "$has_l123_recovery" ] && [ -z "$has_diag_snapshot" ]; then
        info "版本低于 v1.27.7 —— 该版本才加故障快照(dmesg/logcat)和 CP dump bind mount，此前版本第7节相关目录检查不适用"
    fi
else
    warn "$DIAL_VERSION 不存在 —— dial 从未成功启动写过版本文件（main.c 启动时写一次），可能刚烧录还没首次运行，或该文件被清理过。版本号未知，本脚本无法判断当前固件是否具备下面依赖的日志诊断能力，第7节相关'正常'判断请谨慎参考"
fi

# --- 模组固件版本信息 ---
# print_init_info()（dial.c）在 dial_loop 每次"进程启动"时只打印一次 [INIT] IMEI/
# FW/SUB/AP_FW/CP_FW/IMSI/NW mode，不是每天/每次心跳都有。如果 dial 已经连续运行
# 很多天没重启过，这些行大概率在更早的历史日志文件里，不一定在"今天"这份日志里
# ——所以要在全部现存日志文件（受 40 天保留限制）里找最近一次，不能只看最新文件，
# 这是和第7节故意只看最新一份日志的关键区别。
# 版本引入节点：FW(AT+QGMR)/SUB(AT+CSUB) 是 d51f798（2026-05-05）加的；
# AP_FW(AT*ZCGMR)/CP_FW(AT*CGMR) 是 v1.27.5（d0bb476，2026-05-27）才加的，
# 用现有 version_ge 机制能精确门控；FW/SUB 引入时版本号未同步变化（同属 v1.26
# 标签内的多次提交），无法用版本号精确门控，只能给出大致时间点提示。
if [ -n "$log_candidates" ]; then
    fw_logs=$(ls -tr $log_candidates 2>/dev/null)
    imei_line=$(grep -h '\[INIT\] IMEI:' $fw_logs 2>/dev/null | tail -1)
    fw_line=$(grep -h '\[INIT\] FW:' $fw_logs 2>/dev/null | tail -1)
    sub_line=$(grep -h '\[INIT\] SUB:' $fw_logs 2>/dev/null | tail -1)
    apfw_line=$(grep -h '\[INIT\] AP_FW:' $fw_logs 2>/dev/null | tail -1)
    cpfw_line=$(grep -h '\[INIT\] CP_FW:' $fw_logs 2>/dev/null | tail -1)
    imsi_line=$(grep -h '\[INIT\] IMSI:' $fw_logs 2>/dev/null | tail -1)
    nwmode_line=$(grep -h '\[INIT\] NW mode:' $fw_logs 2>/dev/null | tail -1)

    if [ -n "$fw_line" ] || [ -n "$sub_line" ] || [ -n "$imei_line" ]; then
        info "模组固件信息（来自 dial 最近一次进程启动时的打印，可能是历史日志，不一定是今天）："
        [ -n "$imei_line" ] && info "  $imei_line"
        [ -n "$fw_line" ] && info "  $fw_line  <- AT+QGMR 模组固件版本，向移远提交问题单需要这个字段"
        [ -n "$sub_line" ] && info "  $sub_line  <- AT+CSUB SubEdition，判断 CPIN瞬断/CP dump 类底层bug 是否已在此固件修复要看这个"
        [ -n "$apfw_line" ] && info "  $apfw_line  <- AT*ZCGMR ASR1803 AP/Linux侧固件"
        [ -n "$cpfw_line" ] && info "  $cpfw_line  <- AT*CGMR ASR1803 CP/协议栈固件（含CP SDK版本号）"
        [ -n "$imsi_line" ] && info "  $imsi_line"
        [ -n "$nwmode_line" ] && info "  $nwmode_line"
        if [ -z "$apfw_line" ] && [ -z "$cpfw_line" ]; then
            if [ -n "$has_ap_cp_fw" ]; then
                info "没找到 AP_FW/CP_FW —— 版本判断应该有这个能力，可能是日志已轮转超出保留范围，或该次启动打印被截断"
            else
                info "没找到 AP_FW/CP_FW —— 当前版本可能低于 v1.27.5（该版本才加这两行打印），不代表异常"
            fi
        fi
    else
        info "现存日志里没找到 [INIT] FW/SUB/IMEI 等模组固件信息 —— 可能是日志已被清理/轮转掉（保留40天），或固件版本早于 2026-05-05（d51f798，这个功能是那时候才加的）"
    fi
else
    info "未找到任何日志文件，无法读取模组固件版本信息（需要 AT+QGMR/AT+CSUB 等信息的话可用 -v 模式下第7节的日志路径手动核对，或直接现场发 AT+QGMR）"
fi

# ------------------------------------------------------------------
sec "1. 进程存活"
# ------------------------------------------------------------------

dial_pids=$(pgrep -x "$DIAL_PROC" 2>/dev/null)
dial_cnt=$(echo "$dial_pids" | grep -c '[0-9]')
if [ -z "$dial_pids" ]; then
    fail "dial 进程未运行（pgrep -x $DIAL_PROC 无结果）—— 联网功能完全停摆，需要外部机制拉起（本仓库内无看门狗脚本，拉起由仓库之外的 start_prog/init 机制负责）"
elif [ "$dial_cnt" -gt 1 ]; then
    warn "检测到 $dial_cnt 个 dial 进程同时存在 (pid: $(echo $dial_pids | tr '\n' ' ')) —— main.c 启动时用 pgrep -x dial 计数>=2 才拒绝启动，理论上不该出现多实例，出现说明该检查被绕过或存在竞态"
else
    ok "dial 进程正常运行 (pid=$dial_pids)"
fi

if have pgrep; then
    if pgrep -x ql_rild >/dev/null 2>&1; then
        ok "ql_rild 运行中"
    else
        warn "ql_rild 未检测到 —— 注意：main.c 里检查/拉起 ql_rild 的代码整段被 #if 0 编译期禁用（main.c:349-370），当前版本 dial 实际上完全不会去检测或拉起 ql_rild，缺失也不会被自动处理，需要外部机制介入"
    fi
    if pgrep -x ql_netd >/dev/null 2>&1; then
        ok "ql_netd 运行中"
    else
        warn "ql_netd 未检测到 —— restart_ql_netd()（misc.c）虽然存在，但全仓库没有任何调用点，当前版本 dial 同样不会自动拉起它"
    fi
fi

# ------------------------------------------------------------------
sec "2. 配置文件完整性"
# ------------------------------------------------------------------

if [ -f "$APN_JSON" ]; then
    if have python3; then
        python3 -c "import json,sys; json.load(open('$APN_JSON'))" >/dev/null 2>&1 \
            && ok "$APN_JSON 存在且 JSON 格式合法" \
            || fail "$APN_JSON 存在但 JSON 解析失败 —— apn_load_from_json() 解析失败会导致所有 ICCID 都匹配不到，落到自动 APN 默认分支"
    else
        if grep -q '"apn"' "$APN_JSON" 2>/dev/null; then
            ok "$APN_JSON 存在（未装 python3，跳过严格 JSON 校验）"
        else
            warn "$APN_JSON 内容可疑，未找到 apn 字段"
        fi
    fi
    info "匹配规则：ICCID 用 strcmp 全量精确匹配（apn.c），不是前缀匹配，json 里的 iccid 字段必须是完整 20 位"
else
    fail "$APN_JSON 不存在 —— 所有 ICCID 都匹配不到 APN，走自动 APN 默认分支，专网 APN 卡会失败"
fi

if [ -f "$DIAL_CONF" ]; then
    info "$DIAL_CONF 内容: $(cat "$DIAL_CONF" 2>/dev/null)（格式: uptime,restart_flag，仅启动时读写一次，仅供参考）"
else
    info "$DIAL_CONF 不存在，忽略（该文件只在特定重启配置路径下使用，非核心必需项）"
fi

# ------------------------------------------------------------------
sec "3. /tmp 状态文件"
# ------------------------------------------------------------------

if [ -f "$NET_STATUS" ]; then
    ns=$(cat "$NET_STATUS" 2>/dev/null | tr -d '\n')
    case "$ns" in
        1) ok "/tmp/network_status=1（已连接）" ;;
        0) fail "/tmp/network_status=0（未连接）—— 当前处于断网状态" ;;
        *) warn "/tmp/network_status 内容异常: '$ns'" ;;
    esac
else
    fail "$NET_STATUS 不存在 —— dial 从未成功写过联网状态（update_network_status() 从未被调用过，可能刚启动或主流程未跑到）"
fi

if [ -f "$DIAL_STATUS_INV" ]; then
    ds=$(cat "$DIAL_STATUS_INV" 2>/dev/null | tr -d '\n')
    info "/tmp/dial_Status(大写S)=$ds —— 注意这个文件语义和 network_status 相反：0=已连接，1=未连接（data_call.c 里 PDP 回调直接 system(echo) 写的，和 network_status 不是同一套代码路径）"
    if [ -n "$ns" ]; then
        case "$ds:$ns" in
            0:1|1:0) : ;;  # 一致（相反语义下的一致态）
            *) warn "/tmp/dial_Status=$ds 与 /tmp/network_status=$ns 语义上不一致（按 0=连接/1=连接 分别理解后应互为相反值）—— 两者由不同代码路径且无锁写入，瞬时不一致是已知情况，持续不一致才需关注" ;;
        esac
    fi
else
    info "/tmp/dial_Status 不存在，跳过交叉核对"
fi

info "/tmp/network_csq、/tmp/callid 两个文件对应的写入函数（get_signal_strength/writeCallIdToFile）在当前代码里没有任何调用点，属于死代码 —— 即使这两个文件不存在或内容陈旧也不代表故障，不要以此下结论"

if [ -f "$RETRY_COUNT_FILE" ]; then
    rc=$(cat "$RETRY_COUNT_FILE" 2>/dev/null | tr -d '\n')
    warn "$RETRY_COUNT_FILE 存在，值=$rc（上限3）—— 说明 dial 最近处于 Fast-Fail 快速重试模式（PDP建立/Ping 在短超时窗口内失败后主动 exit 重启)，正常拨号成功会自动删除此文件，长期存在说明反复冷启动失败"
else
    ok "$RETRY_COUNT_FILE 不存在 —— 不处于 Fast-Fail 重试状态"
fi

if [ -f "$CFUN_COUNT_FILE" ]; then
    cc=$(cat "$CFUN_COUNT_FILE" 2>/dev/null | tr -d '\n')
    lt=$(cat "$CFUN_TIME_FILE" 2>/dev/null | tr -d '\n')
    info "CFUN 限频计数=$cc（上限10）, 上次调用系统uptime=$lt 秒 —— 这是开机时 exit_count>=20 触发的整机 CFUN=0/1 复位限频，和 dial.c 内部 L2 恢复(单独的 AT+CFUN 分级恢复)不是同一套计数。代码里没有清零该计数的调用点，是否跨重启保留取决于本设备 /tmp 的挂载方式(本仓库未见相关 fstab 配置，未核实)"
fi

if [ -f "$EXIT_COUNT_FILE" ]; then
    ec=$(cat "$EXIT_COUNT_FILE" 2>/dev/null | tr -d '\n')
    info "$EXIT_COUNT_FILE=$ec —— 当前代码里只在启动时读取，达到20会清零并触发一次 CFUN 复位，但代码中未发现任何自增该值的调用点，实际递增机制可能在仓库之外（如外部 supervisor），此字段仅供参考，不作为独立判据"
fi

if [ -f "$SDCARD_AVL" ]; then
    sv=$(cat "$SDCARD_AVL" 2>/dev/null | tr -d '\n')
    if [ "$sv" = "000" ]; then
        fail "/tmp/sdcard_avl=000 —— SD 卡在日志初始化时被判定不可用，dial_log 已降级为仅控制台输出，第7节的日志分析可能拿不到数据"
    else
        info "/tmp/sdcard_avl 内容异常: '$sv'"
    fi
else
    info "/tmp/sdcard_avl 不存在 —— 该文件只在 SD 卡不可用时才会被写入 000，不存在通常说明 SD 检测正常，但没有对应的\"正常\"标记文件可以二次确认，结合第7节能否找到日志文件综合判断"
fi

# ------------------------------------------------------------------
sec "4. 网络接口与路由"
# ------------------------------------------------------------------

if have ip; then
    if ip link show "$STARTUP_IF" >/dev/null 2>&1; then
        ok "启动门槛网卡 $STARTUP_IF 存在（main.c 启动时等待此网卡最多30秒才继续，这不是 PDP 数据接口，只是模组USB控制通道就绪标志）"
    else
        fail "启动门槛网卡 $STARTUP_IF 不存在 —— dial 若刚重启会在此卡住最多30秒后放弃退出，模组USB侧驱动/枚举可能有问题"
    fi
else
    info "无 ip 命令，跳过网卡检查"
fi

# 实际 PDP 数据接口名从最近一条 [EVENT] PDP IPv4/IPv6 日志里的 IF= 字段提取，
# 源码注释里默认是 ccinet0（NOARP/UNSPEC 接口），取不到日志时退化用这个默认值
pdp_if=""
if [ -n "$last_pdp_log" ]; then
    pdp_if=$(grep -E '\[EVENT\] PDP (IPv4|IPv6):' "$last_pdp_log" 2>/dev/null | tail -1 | sed -n 's/.*IF=\([^ ]*\).*/\1/p')
fi
[ -z "$pdp_if" ] && pdp_if="$DEFAULT_PDP_IF"
info "PDP 数据接口: $pdp_if（$([ -n "$last_pdp_log" ] && echo "从最近日志 IF= 字段提取" || echo "未取到日志，使用默认值 $DEFAULT_PDP_IF")）"

if have ip; then
    if ip link show "$pdp_if" >/dev/null 2>&1; then
        ipaddr=$(ip -4 -o addr show "$pdp_if" 2>/dev/null | awk '{print $4}' | cut -d/ -f1)
        if [ -z "$ipaddr" ]; then
            warn "接口 $pdp_if 存在但未分配 IPv4 地址"
        else
            ok "接口 $pdp_if 分配到 IP: $ipaddr"
        fi
    else
        fail "PDP 数据接口 $pdp_if 不存在 —— 拨号未成功建立，或接口名与实际不符（可用 -v 模式看日志原文确认真实接口名）"
    fi
else
    info "无 ip 命令，跳过接口 IP 检查"
fi

if have ip; then
    defroute=$(ip route show default 2>/dev/null)
else
    defroute=$(route -n 2>/dev/null | grep '^0.0.0.0')
fi
if [ -z "$defroute" ]; then
    fail "没有默认路由 —— dial 用 'ip route add default dev <if>' 方式加路由（非 via 网关方式，NOARP 接口语义），未加上说明拨号流程未走完或加路由失败"
else
    ok "默认路由存在: $defroute"
fi

if [ -f /etc/resolv.conf ] && [ -s /etc/resolv.conf ]; then
    ns_cnt=$(grep -c nameserver /etc/resolv.conf 2>/dev/null)
    ok "/etc/resolv.conf 有内容: $ns_cnt 条 nameserver"
    cr=$(printf '\r')
    if grep -q "$cr" /etc/resolv.conf 2>/dev/null; then
        fail "/etc/resolv.conf 含 \\r（CR）字符 —— CLAUDE.md 明确记录过这会污染解析导致域名解析失败，正常写入应为纯 \\n(LF)，出现CR说明写入路径有问题"
    fi
else
    warn "/etc/resolv.conf 缺失或为空 —— 即使数据通道通，域名解析也会失败"
fi

# ------------------------------------------------------------------
sec "5. rx_packets 增长（业务层是否真的有数据流动）"
# ------------------------------------------------------------------

rx_path="/sys/devices/virtual/net/$pdp_if/statistics/rx_packets"
if [ -f "$rx_path" ]; then
    rx1=$(cat "$rx_path" 2>/dev/null)
    sleep 3
    rx2=$(cat "$rx_path" 2>/dev/null)
    # 分别校验两次读数，不能拼接成一个字符串再判空/判非数字——
    # 拼接会掩盖"其中一个是空、另一个是合法数字"的情况（比如两次采样之间
    # dial 恰好重启导致接口消失），拼接后仍是全数字，会漏判直接进入 -gt 比较，
    # 那种情况下单侧为空的 [ -gt ] 虽然不会让脚本崩溃（会被 2>/dev/null 吞掉，
    # 走 else 分支报"无增长"），但那是误报，不是真的无增长。
    rx_valid=1
    case "$rx1" in ''|*[!0-9]*) rx_valid="" ;; esac
    case "$rx2" in ''|*[!0-9]*) rx_valid="" ;; esac
    if [ -z "$rx_valid" ]; then
        info "rx_packets 两次采样读到非数字/空内容（rx1='$rx1' rx2='$rx2'），跳过增长判断——可能是采样期间接口被重建（比如恰好触发了 L1/L2 恢复）"
    elif [ "$rx2" -gt "$rx1" ] 2>/dev/null; then
        ok "$pdp_if rx_packets 3秒内增长 $((rx2 - rx1)) 包 —— 确实有数据在流动"
    else
        warn "$pdp_if rx_packets 3秒内无增长（$rx1 -> $rx2）—— 采样太短仅供参考，若长期无增长但 network_status=1，可能是 PDP 僵尸态（连着但无数据），可结合第7节 [RECOVERY] PDP state 日志交叉核对"
    fi
else
    info "$rx_path 不存在，跳过 rx_packets 检查（接口未建立，或接口名与实际不符）"
fi

# ------------------------------------------------------------------
sec "6. 连通性测试（与 dial 内部探测逻辑一致：ping $PING_HOST）"
# ------------------------------------------------------------------
# dial 内部 test_can_ping_google() 就是用 `ping -c 1 -W 2 8.8.8.8`，检查输出里是否含 ttl=，
# 这里复用同样的探测方式，结果能直接和 dial 自己的判断对齐，而不是另用一套逻辑产生矛盾结论。

if have ping; then
    ping_out=$(ping -c 1 -W 2 "$PING_HOST" 2>/dev/null)
    if echo "$ping_out" | grep -iq 'ttl='; then
        ok "ping $PING_HOST 通（与 dial 内部探测逻辑一致的判据）"
    else
        fail "ping $PING_HOST 不通 —— 若信号/驻网正常但探测持续失败，注意该地区/运营商若对 ICMP 限速或丢弃，dial 自身的联网判断也会跟着受影响（它没有 TCP 兜底逻辑），不是脚本本身的局限"
    fi
else
    info "未找到 ping 命令，跳过连通性测试"
fi

# ------------------------------------------------------------------
sec "7. 日志分析"
# ------------------------------------------------------------------

if [ -d "$SDCARD_DIR" ]; then
    if have df; then
        free_kb=$(df -k "$SDCARD_DIR" 2>/dev/null | awk 'NR==2{print $4}')
        if [ -n "$free_kb" ] && [ "$free_kb" -lt 1048576 ] 2>/dev/null; then
            warn "$SDCARD_DIR 剩余空间 <1GB —— 低于 LOG_FREE_FLOOR_MB(1024MB) 兜底阈值，logger_sd 会从最旧日期文件夹开始删，可能影响历史日志可用性"
        fi
    fi
else
    info "$SDCARD_DIR 不存在 —— dial 会判定无 SD 卡，日志降级为仅控制台输出，本节日志分析无法进行"
fi

if [ -n "$last_pdp_log" ]; then
    info "使用日志文件: $last_pdp_log（按最近修改时间选取；正常按天分文件夹存放，时钟未同步时在 unsynced/ 下）"

    fatal_cnt=$(grep -c '\[FATAL\]' "$last_pdp_log" 2>/dev/null)
    error_cnt=$(grep -c '\[ERROR\]' "$last_pdp_log" 2>/dev/null)
    warning_cnt=$(grep -c '\[WARNING\]' "$last_pdp_log" 2>/dev/null)
    alarm_cnt=$(grep -c '\[ALARM\]' "$last_pdp_log" 2>/dev/null)

    if [ "$fatal_cnt" -gt 0 ] 2>/dev/null; then
        fail "日志中有 $fatal_cnt 条 [FATAL] —— 通常对应 SDK 数据业务服务异常(QL_ERR_ABORTED 类)，非本项目代码内存问题"
    elif [ -n "$has_fatal_tag" ]; then
        ok "日志中无 [FATAL]"
    else
        info "日志中无 [FATAL]，但当前版本可能低于 v1.27.1（该版本才加这个 tag）—— 这不代表没发生过 SDK 服务崩溃，只是这个版本的固件没有能力记录，不能当作'正常'的证据"
    fi
    [ "$error_cnt" -gt 0 ] 2>/dev/null && warn "日志中有 $error_cnt 条 [ERROR]"
    [ "$warning_cnt" -gt 0 ] 2>/dev/null && info "日志中有 $warning_cnt 条 [WARNING]（如 Registration Denied Code 3，需结合具体内容判断是否真实故障还是从未开卡）"
    if [ "$alarm_cnt" -gt 0 ] 2>/dev/null; then
        warn "日志中有 $alarm_cnt 条 [ALARM]（分级恢复触发记录）"
        last_alarm=$(grep '\[ALARM\]' "$last_pdp_log" 2>/dev/null | tail -1)
        info "最近一条: $last_alarm"
    elif [ -n "$has_l123_recovery" ]; then
        ok "日志中无 [ALARM]（未触发过 L1/L2/L3 分级恢复）"
    else
        info "日志中无 [ALARM]，但当前版本可能低于 v1.26（该版本才引入 L1/L2/L3 分级恢复本身）—— 这个版本的固件断网后完全没有任何主动恢复机制，只能靠 SDK 自动重连，无 [ALARM] 不代表'没触发恢复=一直健康'，请重点看第0节的版本号"
    fi

    last_hb=$(grep '\[HEARTBEAT\]' "$last_pdp_log" 2>/dev/null | tail -1)
    [ -n "$last_hb" ] && info "最近一条心跳: $last_hb"

    last_recovered=$(grep -E '\[EVENT\] Network (Recovered|Connected)' "$last_pdp_log" 2>/dev/null | tail -1)
    [ -n "$last_recovered" ] && info "最近一条联网事件: $last_recovered"

    gated=$(grep -c 'never-connected.*gated' "$last_pdp_log" 2>/dev/null)
    [ "$gated" -gt 0 ] 2>/dev/null && info "日志中出现 $gated 次 \"never-connected...gated\" 自证日志 —— 说明设备处于从未连过网的状态（如停机/无数据权限），has_connected_once 门控生效，不会做 L2/L3 破坏性恢复，这是设计内行为，不是恢复逻辑哑火"

    if [ "$VERBOSE" = "1" ]; then
        echo "---- 最近 ERROR/WARNING/FATAL 明细 ----"
        grep -E '\[ERROR\]|\[WARNING\]|\[FATAL\]' "$last_pdp_log" 2>/dev/null | tail -20
        echo "----------------------------------------"
    fi
else
    info "$LOG_ROOT 下未找到 dial_*.log，跳过日志分析"
fi

if [ -d "$DIAG_SNAP_DIR" ]; then
    snap_cnt=$(ls "$DIAG_SNAP_DIR" 2>/dev/null | grep -c '.')
    if [ "$snap_cnt" -gt 0 ] 2>/dev/null; then
        info "$DIAG_SNAP_DIR 下有 $snap_cnt 个故障诊断快照文件（dmesg/logcat，故障/恢复各触发一次）—— 有历史故障发生过，具体次数/时间点看文件名时间戳"
    fi
elif [ -n "$has_diag_snapshot" ]; then
    info "$DIAG_SNAP_DIR 不存在 —— 尚未触发过故障快照（首次断网确认时才会创建）"
else
    info "$DIAG_SNAP_DIR 不存在，但当前版本可能低于 v1.27.7（该版本才加 diag.c 故障快照能力）—— 目录不存在可能只是这个版本压根没有这个功能，不代表从未故障"
fi

if [ -d "$CP_DUMP_DIR" ]; then
    dump_cnt=$(ls "$CP_DUMP_DIR" 2>/dev/null | grep -c '.')
    if [ "$dump_cnt" -gt 0 ] 2>/dev/null; then
        fail "$CP_DUMP_DIR 下有 $dump_cnt 个 CP dump 文件 —— 说明协议栈(CP)发生过崩溃重启，这是比软件状态机更底层的问题，建议连同 diag.sdl 一起提交给移远分析"
    else
        ok "$CP_DUMP_DIR 存在但为空 —— bind mount 已建立，未发生过 CP dump"
    fi
elif [ -n "$has_diag_snapshot" ]; then
    info "$CP_DUMP_DIR 不存在 —— 可能是 /media/sdcard 不可用导致 bind mount 未建立（setup_cp_dump_capture 会跳过），也可能确实从未发生过 CP dump"
else
    info "$CP_DUMP_DIR 不存在，当前版本可能低于 v1.27.7（该版本才加 CP dump bind mount 能力，此前 /sdcard 不会被绑定到 SD 卡，CP dump 发生了也留不下来）—— 这个检查在老版本上天然拿不到数据，不代表没发生过 CP dump"
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
    echo "未发现明显异常。若用户仍反馈无法联网，建议结合 -v 参数重跑，人工核对最近日志与 [HEARTBEAT] 心跳。"
fi

exit 0
