#!/bin/sh
#
# eg25_old_diag_network.sh — open_dial 现场联网问题一键诊断脚本
#
# ==================== 用法 ====================
#   把这个文件拷到设备上任意可写目录（如 /usrdata/），然后执行：
#     sh eg25_old_diag_network.sh          # 普通模式
#     sh eg25_old_diag_network.sh -v       # 详细模式：额外打印 dial_status 原始全文、
#                                  # seas_log 最近 ERROR/FATAL 明细等
#
#   全程只读（不改任何文件、不重启进程、不发 AT 指令），可以随时在
#   正在跑的设备上执行，不影响 dial 运行。不需要额外装东西，用到的
#   nc/ip/python3/ping 等工具缺失时会自动跳过对应检查并提示，不会报错中断。
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
#   章节编号（0~8）就是建议的排查顺序：第0节信号/驻网最基础、最该先看；
#   越往后越偏向 dial 自身状态机/软件逻辑层面（第3节起）。如果第0节就有
#   [FAIL]，通常不用急着往后看软件层面的东西。
#
# ==================== 设计原则 ====================
#   1. 只读，不写。不修改 /tmp/network_*、不重启进程、不发 AT 指令。
#      -- AT 口 /dev/smd8 由 dial 进程内部用 g_at_port_mutex 保护，
#         这个锁只在 dial 进程内有效，外部脚本并发发 AT 指令拿不到这把锁，
#         只会和 dial 抢串口，可能造成 dial 拨号状态机误判超时。
#         所以本脚本完全不碰 /dev/smd8，只读 dial 已经写出来的状态文件。
#   2. 目标是 busybox ash（与仓库里的 check_network.sh 同一运行环境），
#      不用 bash 专有语法，工具用之前先 command -v 探测，缺了就跳过并提示。
#   3. 每一项检查的判断依据都来自 open_dial 源码里的真实行为
#      （状态机、常量、文件格式），不是通用网络排查套路。
#

DIAL_BIN="/usr/dial/dial"
DIAL_STATUS="/tmp/dial_status"
NET_STATUS="/tmp/network_status"
NET_TYPE="/tmp/network_type"
NET_CSQ="/tmp/network_csq"
NET_PLMN="/tmp/network_plmn"
NETWORK_INI="/usrdata/network.ini"
APN_JSON="/usr/dial/apn.json"
CONF_INI="/opt/conf.ini"
LICENSE_MAIN="/usrdata/roamlink/etc/.pconfig/license.cer"
LICENSE_BACKUP="/data/ufs/license.cer"
RBMASTER_BIN="/usrdata/roamlink/RBMaster"
CHECK_NET_LOG="/usrdata/check_network.log"
SDCARD_DIR="/media/sdcard"
SEAS_LOG="/media/sdcard/seas_log_dial.log"
TCP_TEST_HOST="18.196.0.17"
TCP_TEST_PORT="22"
# 备用连通性目标：与 dial 内部测试目标不同，用来区分
# "18.196.0.17 这个单点探测目标本身不可达" 还是 "模组/信道真的不通"
# 用两个地理/网络路径不同的目标做三角验证，避免单一备用目标自己也被墙/干扰时误判：
#   ALT_TEST_HOST  : 阿里 DNS，国内网络下通常最稳
#   ALT_TEST_HOST2 : Google DNS，国际网络下通常最稳，但中国大陆方向可能不稳定
#   （本项目设备是多国部署的 Roamlink 虚拟SIM设备，apn.json 里能看到挪威/泰国/印尼/
#     中国移动等运营商，没有单一目标能覆盖所有现场，所以用三个而不是一个）
ALT_TEST_HOST="223.5.5.5"
ALT_TEST_PORT="443"
ALT_TEST_HOST2="8.8.8.8"
ALT_TEST_PORT2="53"

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

# 取文件 mtime（不同 busybox stat 参数不一致，做兼容）
mtime_epoch() {
    f="$1"
    if have stat; then
        stat -c %Y "$f" 2>/dev/null && return
    fi
    # 退化方案：无 stat 时用 find -printf（同样非所有 busybox 都支持）
    find "$f" -printf '%T@\n' 2>/dev/null | cut -d. -f1
}

age_of() {
    f="$1"
    [ -e "$f" ] || { echo -1; return; }
    m=$(mtime_epoch "$f")
    n=$(now_epoch)
    [ -n "$m" ] && [ -n "$n" ] && echo $((n - m)) || echo -1
}

hr
echo "open_dial 联网问题诊断脚本  $(date '+%Y-%m-%d %H:%M:%S')"
hr

# ------------------------------------------------------------------
# 机型护栏（先于一切检查）：本脚本针对 EG25 机型
#   —— 高通 MDM9607 / armv7 / Quectel OpenLinux(内核3.18) / QMI(rmnet_data0/bridge0)。
# 若误跑到 EC200A(ASR1803 / aarch64 / ql-sdk / ECM, ecm0/ccinet0) 机器上，本脚本的
# rmnet/roamlink/dial_status 等检查会全部失配误报——那台应改用 open_dial 仓库的
# ec200a_old_diag_network.sh。这里命中"非 EG25 架构"就提示换脚本并退出。
# ------------------------------------------------------------------
arch=$(uname -m 2>/dev/null)
case "$arch" in
    aarch64|arm64)
        warn "机型不匹配：本机是 $arch(64位)，而本脚本针对 EG25(高通 MDM9607/armv7/OpenLinux)。"
        info "若这是 EC200A(ASR1803, aarch64) 机器，请改用 open_dial 仓库的 ec200a_old_diag_network.sh。"
        info "→ 判定：此机非本脚本目标机型，已退出，避免一堆失配误报。"
        hr
        echo "结论：机型不匹配，未执行 EG25 诊断。"
        hr
        exit 0
        ;;
esac

# ------------------------------------------------------------------
# 版本兼容性检测：本脚本是对照 feature/dual_channel_switch 分支（约 V1.29.13）
# 的行为/文件格式写的。dial 是长期迭代的项目（README 版本历史能追溯到 V1.10），
# 现场设备很可能还在跑旧固件，而下面几项能力是这个分支才有的：
#   - [HEARTBEAT]/[INIT] 日志格式：V1.29.10 才把 CSQ 刷屏日志改成心跳，这之前
#     没有这两种 tag，第0节的驻网/小区信息会一直是"没取到"，不代表没问题
#   - [INIT] COPS 快照：V1.29.13 才加，更旧版本没有很正常
# check_network.sh 和 dial 是同一个部署包一起发布的（roamlink_deploy 打包），
# 版本号对上即可，不必再单独去核对 check_network.sh 内容。
# 这里做个尽力而为的版本探测，命中就提醒，不阻塞后续检查。
# ------------------------------------------------------------------
if [ -f "$DIAL_STATUS" ]; then
    dial_ver=$(grep '^version=' "$DIAL_STATUS" 2>/dev/null | head -1 | cut -d= -f2)
else
    dial_ver=""
fi

version_ge() {
    # 粗略数值比较 "1.29.13" 这种三段版本号，非数字/缺段时认为比较失败（不下结论）
    v1=$(echo "$1" | awk -F. '{ if (NF>=3) printf "%d%03d%03d", $1+0,$2+0,$3+0 }' 2>/dev/null)
    v2=$(echo "$2" | awk -F. '{ if (NF>=3) printf "%d%03d%03d", $1+0,$2+0,$3+0 }' 2>/dev/null)
    [ -n "$v1" ] && [ -n "$v2" ] && [ "$v1" -ge "$v2" ] 2>/dev/null
}

if [ -z "$dial_ver" ]; then
    warn "读不到 dial 版本号（$DIAL_STATUS 不存在，或版本太旧没有 version 字段）—— 下面的判断请谨慎参考，尤其是第0节驻网/小区信息和依赖 [HEARTBEAT]/[INIT] 日志 tag 的部分，可能是版本原因取不到，不是设备真的有问题"
else
    info "当前 dial 版本: $dial_ver（本脚本按 V1.29.13 行为编写）"
    if ! version_ge "$dial_ver" "1.29.10"; then
        warn "版本低于 V1.29.10 —— 该版本才把 CSQ 刷屏日志重构成 [HEARTBEAT]/[INIT]，此前版本没有这些 tag，第0节里驻网(CEREG)/驻留小区(cellid/pci/tac)/RSRP/RSRQ 会全部取不到，这是版本能力问题，不代表设备无信号"
    elif ! version_ge "$dial_ver" "1.29.13"; then
        info "版本低于 V1.29.13 —— [INIT] COPS 开机快照是这个版本才加的，没有属于正常，不影响其余判断"
    fi
fi

# ------------------------------------------------------------------
sec "0. 无线层基础信息（信号 / 驻网 / 小区）—— 现场排查第一优先级"
# ------------------------------------------------------------------
# 注意：本节不发任何 AT 指令，全部读取 dial 自己已经采集并落盘/落日志的结果。
# /dev/smd8 由 dial 进程内的 g_at_port_mutex 保护，外部脚本并发发 AT 指令
# 拿不到这把锁，只会跟 dial 抢串口、可能干扰其状态机，所以绝不在这里发指令。

# --- CSQ 信号强度：主线程每 5s 发一次 AT+CSQ，写 /tmp/network_csq ---
if [ -f "$NET_CSQ" ]; then
    csq_val=$(cat "$NET_CSQ" 2>/dev/null)
    if [ "$csq_val" = "99" ] || [ -z "$csq_val" ]; then
        fail "CSQ=$csq_val 无效/未知 —— 模组读不到信号强度，优先检查天线/模组本身，而不是软件逻辑"
    else
        case "$csq_val" in
            ''|*[!0-9]*)
                # 非纯数字：可能读到写入中途的半截内容，不能做算术，否则部分 shell(如 dash)
                # 遇到 $((...)) 非法数字会直接中止整个脚本，而不只是这一步失败
                fail "CSQ 内容异常（原始值: '$csq_val'，非纯数字）—— /tmp/network_csq 可能读到了写入中途的半截内容，重新跑一次确认"
                ;;
            *)
                # 3GPP TS 27.007 标准换算，dial 源码里定义了 CSQ_TO_RSSI_OFS=113 但从未实际用到，
                # 这里换算仅供人读参考，不是 dial 原生输出
                dbm=$((2 * csq_val - 113))
                if [ "$csq_val" -lt 10 ]; then
                    fail "CSQ=$csq_val（约 ${dbm}dBm）信号弱 —— 现场先查天线安装/位置/运营商覆盖，这是比任何通道切换逻辑更底层的问题"
                elif [ "$csq_val" -lt 15 ]; then
                    warn "CSQ=$csq_val（约 ${dbm}dBm）信号偏弱，可用但不稳"
                else
                    ok "CSQ=$csq_val（约 ${dbm}dBm）信号正常"
                fi
                ;;
        esac
    fi
else
    fail "/tmp/network_csq 不存在 —— dial 主线程从未成功写过 CSQ，可能主线程卡死或 dial 未启动"
fi

# --- 驻网状态 + 驻留小区 + 实测信号质量：主线程每 300s 发一次 CEREG/QENG，写入 seas_log 的扩展心跳行 ---
if [ -f "$SEAS_LOG" ]; then
    last_hb_ext=$(grep '\[HEARTBEAT\] cereg=' "$SEAS_LOG" 2>/dev/null | tail -1)
    if [ -n "$last_hb_ext" ]; then
        info "最近一次扩展心跳（每300s一条）: $last_hb_ext"
        cereg_bit=$(echo "$last_hb_ext" | sed -n 's/.*cereg=\([0-9]\).*/\1/p')
        if [ "$cereg_bit" = "1" ]; then
            ok "CEREG 已注册（stat=1 home 或 5 roaming）"
        else
            fail "CEREG 未注册 —— 拨号即使成功也无法真正用网，这是驻网层问题，优先于任何通道切换/APN排查"
            last_cereg_detail=$(grep 'cereg_n:' "$SEAS_LOG" 2>/dev/null | tail -1)
            if [ -n "$last_cereg_detail" ]; then
                info "最近一条原始 CEREG 明细: $last_cereg_detail （stat_cereg: 0=未注册未搜索 2=搜索中 3=被拒 4=未知 1/5=已注册)"
            fi
        fi
        if echo "$last_hb_ext" | grep -q 'cellid=NA'; then
            warn "最近一次未取到驻留小区信息（cellid=NA）—— 非LTE制式，或 AT+QENG 解析失败/当时未驻网"
        fi
    else
        info "seas_log 中暂无扩展心跳记录（进程启动不足 ${HEARTBEAT_EXT_SEC:-300}s，或从未联网触发过该分支），跳过驻网/小区信息"
    fi

    last_init_cops=$(grep '\[INIT\] COPS:' "$SEAS_LOG" 2>/dev/null | tail -1)
    [ -n "$last_init_cops" ] && info "最近一次开机 COPS 快照: $last_init_cops （+COPS: 0,... = 自动选网；+COPS: 1,... = 手动锁网，可能是上次异常切换残留的锁定状态）"
    last_init_fw=$(grep '\[INIT\] FW:' "$SEAS_LOG" 2>/dev/null | tail -1)
    [ -n "$last_init_fw" ] && info "最近一次开机固件版本: $last_init_fw"
else
    info "$SEAS_LOG 不存在，无法读取驻网/小区/COPS 信息（原因见第8节 sdcard 检查）"
fi

# ------------------------------------------------------------------
sec "1. 进程存活"
# ------------------------------------------------------------------

dial_pids=$(pgrep -f "$DIAL_BIN" 2>/dev/null)
dial_cnt=$(echo "$dial_pids" | grep -c '[0-9]')
if [ -z "$dial_pids" ]; then
    fail "dial 进程未运行 ($DIAL_BIN) —— 联网功能完全停摆，看门狗应在 30s 内拉起"
elif [ "$dial_cnt" -gt 1 ]; then
    warn "检测到 $dial_cnt 个 dial 进程同时存在 (pid: $(echo $dial_pids | tr '\n' ' ')) —— 正常应只有 1 个，多个实例会抢 /dev/smd8 和 /tmp/dial_status"
else
    ok "dial 进程正常运行 (pid=$dial_pids)"
fi

# RBMaster：源码里 roamlink_is_master_running() 只扫 /proc/<pid>/cmdline，不做端口探测
# （历史上端口探测导致误判进程已死、反复 fork 造成进程堆积，已在 V1.29.11 移除）
rb_pids=$(pgrep -f "RBMaster" 2>/dev/null)
rb_cnt=$(echo "$rb_pids" | grep -c '[0-9]')
if [ -n "$rb_pids" ] && [ "$rb_cnt" -gt 1 ]; then
    warn "检测到 $rb_cnt 个 RBMaster 进程 (pid: $(echo $rb_pids | tr '\n' ' ')) —— 正常应≤1个，多实例是进程堆积的信号，参考 roamlink_kill_master() 修复记录"
elif [ -n "$rb_pids" ]; then
    info "RBMaster 运行中 (pid=$rb_pids)，是否应该运行取决于当前策略/通道，见下文 dial_status"
else
    info "RBMaster 未运行（若当前应处于 roamlink 通道，这本身就是异常，见下文 dial_status 交叉核对）"
fi

if pgrep -f "check_network.sh" >/dev/null 2>&1; then
    ok "check_network.sh 看门狗运行中"
else
    warn "check_network.sh 看门狗未检测到 —— dial 一旦崩溃将不会被自动拉起"
fi

# ------------------------------------------------------------------
sec "2. 配置文件完整性（policy / apn / roamlink 前置依赖）"
# ------------------------------------------------------------------

if [ -f "$NETWORK_INI" ]; then
    sel=$(grep -o 'network_select *= *[0-9]*' "$NETWORK_INI" 2>/dev/null | grep -o '[0-9]*$')
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
        warn "$NETWORK_INI: network_select 值非法或缺失（原始值: '$sel'）—— dial 启动时读取失败会静默退回默认策略 4 (FORCE_SIM)，注意这是仅在启动时读取一次，改配置需重启 dial 才生效"
    fi
else
    warn "$NETWORK_INI 不存在 —— dial 将使用默认策略 4 (FORCE_SIM)"
fi

if [ -f "$APN_JSON" ]; then
    if have python3; then
        python3 -c "import json,sys; json.load(open('$APN_JSON'))" >/dev/null 2>&1 \
            && ok "$APN_JSON 存在且 JSON 格式合法" \
            || fail "$APN_JSON 存在但 JSON 解析失败 —— apn_get_apn_obj() 匹配不到会退化为空 APN 默认对象，拨号大概率失败"
    else
        # 无 python 时退化为粗略括号配对检查
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
    info "$CONF_INI 不存在 —— roamlink_probe() 会返回 CONF_MISSING，本次运行永久降级 FORCE_SIM（若设备本就不用 roamlink 可忽略）"
fi

if [ -x "$RBMASTER_BIN" ]; then
    if [ -s "$LICENSE_MAIN" ]; then
        ok "license 主文件存在且非空: $LICENSE_MAIN"
    else
        if [ -s "$LICENSE_BACKUP" ]; then
            warn "license 主文件缺失/为空，但备份存在 ($LICENSE_BACKUP) —— dial 启动时会自动尝试恢复；若日志里没看到恢复成功，检查文件权限"
        else
            warn "license 主/备份均缺失或为空 —— roamlink_probe()=LICENSE_MISSING，策略1/2/4会进入 license_pending 等待下载（当前超时 300s，见下文 dial_status.license_wait_sec 是否接近超时），策略3会卡死等待人工介入"
        fi
    fi
else
    info "RBMaster 二进制不存在或不可执行 ($RBMASTER_BIN) —— 本设备无 roamlink 能力，本节后续检查可忽略"
fi

# ------------------------------------------------------------------
sec "3. dial 内部状态 (/tmp/dial_status)"
# ------------------------------------------------------------------

if [ ! -f "$DIAL_STATUS" ]; then
    fail "$DIAL_STATUS 不存在 —— dial 从未成功写过状态（刚启动，或 dial_task 线程卡死在某个状态里从未跑到 dial_status_update）"
else
    age=$(age_of "$DIAL_STATUS")
    if [ "$age" -gt 60 ]; then
        fail "$DIAL_STATUS 已 ${age}s 未更新 —— dial_task 线程大概率卡死（正常每次状态机循环都会刷新），但主线程仍可能存活，进程存在不代表业务在跑"
    else
        ok "$DIAL_STATUS 更新于 ${age}s 前，状态机未卡死"
    fi

    getval() { grep "^$1=" "$DIAL_STATUS" 2>/dev/null | head -1 | cut -d= -f2-; }

    st_state=$(getval state)
    st_policy=$(getval policy)
    st_policy_name=$(getval policy_name)
    st_channel=$(getval channel)
    st_rl_avail=$(getval roamlink_available)
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
    st_total_outage=$(getval total_outage_sec)
    st_rbpid=$(getval rbmaster_pid)

    info "state=$st_state  channel=$st_channel  policy=$st_policy($st_policy_name)"

    # policy 与 network.ini 是否一致（network.ini 只在启动时读一次，改了不重启不生效）
    if [ -n "$sel" ] && [ -n "$st_policy" ] && [ "$sel" != "$st_policy" ]; then
        warn "network.ini 当前 network_select=$sel，但 dial_status 里 policy=$st_policy —— 说明改配置后 dial 还没重启，仍按旧策略跑"
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

    # --- SIM 卡信息 ---
    if [ -z "$st_iccid" ]; then
        warn "iccid 为空 —— SIM 卡尚未识别成功（物理SIM未插好/损坏，或当前在 roamlink 通道下这是正常的）"
    else
        ok "ICCID: $st_iccid"
        iccid_prefix=$(echo "$st_iccid" | cut -c1-6)
        if [ -f "$APN_JSON" ]; then
            if grep -q "\"iccid\":\"$iccid_prefix\"" "$APN_JSON" 2>/dev/null; then
                ok "ICCID 前6位($iccid_prefix)能在 $APN_JSON 中匹配到条目"
            else
                fail "ICCID 前6位($iccid_prefix)在 $APN_JSON 中没有匹配条目 —— apn_get_apn_obj() 会退化返回一个空 APN 默认对象（APN/用户名/密码全为空），状态机不会卡死，但拨号大概率失败或拨通了也无法上网。需要确认这张卡对应的运营商是否已经录入 apn.json"
            fi
        fi
    fi

    if [ -n "$st_apn" ]; then
        info "当前使用的 APN: $st_apn  profile_idx=$st_profile"
    else
        info "dial_status 里 apn 字段为空 —— 尚未进入 pre_start_call 阶段，或 ICCID 未匹配到任何 apn.json 条目（见上一行判定）"
    fi
    if [ -n "$st_profile" ] && [ "$st_profile" != "1" ]; then
        info "profile_idx=$st_profile（源码里 apn_scan_idx() 固定强制走高通自动路由分支，正常恒为1；非1不代表故障，但和文档描述不符，值得留意）"
    fi

    # plmn 字段是已知死代码：nw_mark_plmn() 无调用方，/tmp/network_plmn 永不会被写，
    # dial_status 的 plmn 固定是 unknown，不能作为判断依据，这里只是明确告知避免误读
    info "PLMN 字段（当前值: ${st_plmn:-unknown}）固定读不到真实运营商代码 —— nw_mark_plmn() 是死代码，无任何调用方写过 /tmp/network_plmn，不代表未注网"
    if [ -n "$st_imsi" ] && [ "$st_imsi" != "unknown" ]; then
        ok "IMSI: $st_imsi"
    else
        info "IMSI 未取到（值: ${st_imsi:-空}）—— dial_stat_sim_op 拿 IMSI 是 best-effort，失败不重试也不阻塞状态机，可能是模组当时对 AT+CIMI 应答异常，或版本比较旧还没有 IMSI 采集逻辑（旧版本 dial_status 里没有 imsi 字段，本行会一直是空）"
    fi

    # csq 的严重性判断（弱/偏弱/正常）已经在第0节对 /tmp/network_csq 做过了。
    # dial_status.csq 和 /tmp/network_csq 理论上同源（主线程同一次 AT+CSQ 结果的两份拷贝），
    # 这里不重复判定严重性（避免同一指标在两节给出不同结论），只做一致性核对。
    if [ -n "$st_csq" ]; then
        if [ -n "$csq_val" ] && [ "$csq_val" != "$st_csq" ]; then
            warn "dial_status.csq=$st_csq 与 /tmp/network_csq=$csq_val 不一致 —— 两者应同源，不一致可能是读取跨越了一次 CSQ 变化瞬间，持续不一致才需关注"
        else
            info "dial_status.csq=$st_csq（信号严重性判断见第0节）"
        fi
    fi

    if [ "$st_lic_pending" = "1" ]; then
        # 当前源码超时是 300s（README 明确标注生产前需改 7200），用当前值做判断更准确，不要写死 7200
        if [ -n "$st_lic_wait" ]; then
            warn "license_pending=1，已等待 ${st_lic_wait}s —— roamlink 尚未拿到 license，SIM 通道在临时顶替。若长期停在这个状态且等待时间接近超时阈值（当前代码 LICENSE_WAIT_TIMEOUT_SEC=300s，注意这是测试值，生产环境应为 7200s，确认此设备用的是哪个版本），超时后会放弃 roamlink 转入纯 SIM 重拨"
        fi
    fi

    if [ "$st_tcp_fail" -ge 2 ] 2>/dev/null; then
        warn "tcp_fail_count=$st_tcp_fail（阈值3次触发切换）—— 当前通道的 TCP 探测连续失败，接近触发切换"
    fi
    if [ "$st_rl_fail" -ge 2 ] 2>/dev/null; then
        warn "roamlink_fail_count=$st_rl_fail（阈值3次触发切换）"
    fi

    if [ -n "$st_cur_outage" ] && [ "$st_cur_outage" -gt 0 ] 2>/dev/null; then
        fail "current_outage_sec=$st_cur_outage —— 当前正处于断网中，已持续 ${st_cur_outage}s"
    fi
    if [ -n "$st_outage_cnt" ]; then
        info "本次运行累计断网 outage_count=$st_outage_cnt 次，total_outage_sec=${st_total_outage}s（进程重启会清零，不代表设备终身状态）"
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
        warn "/tmp/network_status=$ns 与 dial_status 里的 status=$st_net_status 不一致 —— 两者由不同代码路径写入且无锁（dial_task 主流程 + SDK 拨号回调），瞬时不一致是已知设计，若持续不一致才需关注"
    fi
fi

if [ -f "$NET_CSQ" ]; then
    csq_age=$(age_of "$NET_CSQ")
    if [ "$csq_age" -gt 30 ]; then
        warn "/tmp/network_csq 已 ${csq_age}s 未更新（主线程每5s刷一次）—— 主线程可能卡在等待 AT 口互斥锁（例如 dial_task 正在跑180s超时的 AT+COPS=? / AT+COPS=0）"
    else
        ok "/tmp/network_csq 更新及时 (${csq_age}s 前)，csq=$(cat "$NET_CSQ" 2>/dev/null)"
    fi
fi

if [ -f "$NET_PLMN" ]; then
    info "/tmp/network_plmn 存在，内容: $(cat "$NET_PLMN" 2>/dev/null)"
else
    info "/tmp/network_plmn 不存在 —— 这是已知情况：nw_mark_plmn() 是死代码、当前版本没有任何调用方，该文件本来就不会被写，dial_status 里 plmn 字段固定是 unknown，不代表故障"
fi

# ------------------------------------------------------------------
sec "5. 网络接口与路由"
# ------------------------------------------------------------------

if have ip; then
    rmnet_if=$(ip -o link show 2>/dev/null | awk -F': ' '{print $2}' | grep '^rmnet')
elif have ifconfig; then
    rmnet_if=$(ifconfig -a 2>/dev/null | grep -o '^rmnet[a-zA-Z0-9_]*')
fi

if [ -z "$rmnet_if" ]; then
    fail "未找到任何 rmnet* 网络接口 —— 模组数据通道从驱动层就没建立起来，比软件状态机问题更底层"
else
    for ifn in $rmnet_if; do
        if have ip; then
            ipaddr=$(ip -4 -o addr show "$ifn" 2>/dev/null | awk '{print $4}' | cut -d/ -f1)
        else
            ipaddr=$(ifconfig "$ifn" 2>/dev/null | grep -o 'inet addr:[0-9.]*' | cut -d: -f2)
        fi
        if [ -z "$ipaddr" ]; then
            warn "接口 $ifn 存在但未分配 IP"
        elif [ "$ipaddr" = "0.0.0.0" ]; then
            fail "接口 $ifn 分配到全零地址 0.0.0.0 —— 对应源码里已知的 '全零地址' 问题场景（常见于 license_pending 超时后 RBMaster 未及时释放信道，或 DNS/IP 下发异常），当前代码里这个兜底逻辑是预留但未启用的（zero_addr_redialed 字段），需要手动重拨/重启恢复"
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
    fail "没有默认路由 —— 即使拨号 IP 有效也无法访问外网"
else
    ok "默认路由存在: $defroute"
fi

if [ -f /etc/resolv.conf ] && [ -s /etc/resolv.conf ]; then
    ok "/etc/resolv.conf 有内容: $(grep -c nameserver /etc/resolv.conf 2>/dev/null) 条 nameserver"
else
    warn "/etc/resolv.conf 缺失或为空 —— 即使数据通道通，域名解析也会失败（与'全零地址'是同一类症状的另一种表现）"
fi

# ------------------------------------------------------------------
sec "6. rx_packets 增长（业务层是否真的有数据流动）"
# ------------------------------------------------------------------

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
        # 真实切换信道时 rmnet_data* 可能被拆掉重建，读到空/非数字内容不是没可能；
        # 非纯数字直接跳过这个接口，不能做算术（同 CSQ 那里的道理，避免部分 shell 中止整个脚本）
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
        warn "rmnet_data* rx_packets 3秒内无增长（$rx1 -> $rx2）—— 与源码里 ROAMLINK_NO_DATA_TIMEOUT_SEC(120s)/DIAL_TIMEOUT_SECONDS(60s) 判死判据一致的现象，但3秒采样太短仅供参考，建议结合 dial_status 里 biz_no_data_sec 字段判断是否已达到阈值"
    fi
else
    info "未找到 rmnet_data* 统计接口，跳过 rx_packets 检查"
fi

# ------------------------------------------------------------------
sec "7. TCP 连通性测试"
# ------------------------------------------------------------------
# 判定逻辑固定用 TCP（和 dial 内部 nw_tcp_connectivity_test 同协议），
# 不用 ICMP ping 来定 PASS/FAIL —— 很多蜂窝网络/Roamlink 虚拟SIM通道会限速或丢弃 ICMP
# 但放行 TCP，单独 ping 失败并不能证明"不通"，反而会和 TCP 结果互相矛盾。
# ping 结果仅作为本节末尾的补充参考信息展示，不参与 PASS/WARN/FAIL 判定。

tcp_test() {
    host="$1"; port="$2"; timeout="$3"
    if have nc; then
        # 坑：BusyBox 的 nc 不支持 -z（现场实测报 "invalid option -- 'z'" 后恒失败，
        # 会让本节把好设备误判成"三个目标全不通/无出网能力"）。这里先探测本机 nc 是否
        # 支持 -z：支持(GNU/OpenBSD nc)就用 -z（连上即断、最干净）；不支持(BusyBox)则用
        # </dev/null 让连上后立即 EOF 关闭、-w 兜底超时，凭退出码判可达。探测只做一次。
        if [ -z "$NC_HAS_Z" ]; then
            if nc -z -w1 127.0.0.1 1 2>&1 | grep -qiE 'invalid|unrecognized|Usage'; then
                NC_HAS_Z=no
            else
                NC_HAS_Z=yes
            fi
        fi
        if [ "$NC_HAS_Z" = yes ]; then
            nc -z -w "$timeout" "$host" "$port" >/dev/null 2>&1
        else
            nc -w "$timeout" "$host" "$port" </dev/null >/dev/null 2>&1
        fi
        return $?
    fi
    # busybox ash 部分编译版本支持 /dev/tcp
    ( exec 3<>"/dev/tcp/$host/$port" ) >/dev/null 2>&1
    ret=$?
    exec 3>&- 2>/dev/null
    return $ret
}

if have nc || (echo >"/dev/tcp/127.0.0.1/1" 2>/dev/null); then
    if tcp_test "$TCP_TEST_HOST" "$TCP_TEST_PORT" 5; then
        ok "探测目标 $TCP_TEST_HOST:$TCP_TEST_PORT（与 dial 内部测试目标相同）连通"
    else
        warn "探测目标 $TCP_TEST_HOST:$TCP_TEST_PORT 不通 —— 注意这是代码里硬编码的单点探测目标（NW_TCP_TEST_HOST），如果只是这一个 IP/端口在公网侧出问题，dial 会误判为'两个通道都不行'反复切换，请看后面备用目标的结果再下结论"
        if tcp_test "$ALT_TEST_HOST" "$ALT_TEST_PORT" 5; then
            warn "但备用目标 $ALT_TEST_HOST:$ALT_TEST_PORT 是通的 —— 大概率是探测目标单点问题，不是模组/信道故障，建议检查 18.196.0.17:22 的可达性（防火墙/运营商侧封禁/目标服务器本身问题），而非排查设备本身"
        elif tcp_test "$ALT_TEST_HOST2" "$ALT_TEST_PORT2" 5; then
            warn "备用目标1 $ALT_TEST_HOST:$ALT_TEST_PORT 也不通，但备用目标2 $ALT_TEST_HOST2:$ALT_TEST_PORT2 通 —— 更可能是区域性/单一目标封锁（例如设备所在国家对备用目标1的方向有干扰），不是设备本身无出网能力；注意本项目设备是多国部署（apn.json 里有挪威/泰国/印尼/中国移动等），8.8.8.8 在部分地区（含中国大陆）本身就可能不稳定，三个目标结合起来看，不要只依赖某一个"
        else
            fail "三个互不相关的目标（$TCP_TEST_HOST:$TCP_TEST_PORT / $ALT_TEST_HOST:$ALT_TEST_PORT / $ALT_TEST_HOST2:$ALT_TEST_PORT2）全部不通 —— 指向设备本身没有出网能力（信道/路由/DNS 问题），而不是单一探测目标的问题"
        fi
    fi
else
    info "未找到 nc 且 /dev/tcp 不受支持，跳过 TCP 连通性测试（可安装 busybox nc 或手动用 telnet $TCP_TEST_HOST $TCP_TEST_PORT 验证）"
fi

# --- ICMP ping 补充参考（仅供参考，不参与判定，见本节开头说明）---
if have ping; then
    if ping -c 2 -W 2 "$ALT_TEST_HOST2" >/dev/null 2>&1; then
        info "补充参考：ping $ALT_TEST_HOST2 (Google DNS) 通"
    else
        info "补充参考：ping $ALT_TEST_HOST2 不通 —— 若上面 TCP 测试是通的，这大概率是该地区/该运营商对 ICMP 限速或丢弃，不代表网络故障，可忽略"
    fi
else
    info "未找到 ping 命令，跳过 ICMP 补充参考测试"
fi

# ------------------------------------------------------------------
sec "8. 日志分析"
# ------------------------------------------------------------------

if [ -d "$SDCARD_DIR" ]; then
    if have df; then
        free_kb=$(df -k "$SDCARD_DIR" 2>/dev/null | awk 'NR==2{print $4}')
        if [ -n "$free_kb" ] && [ "$free_kb" -lt 1048576 ] 2>/dev/null; then
            warn "$SDCARD_DIR 剩余空间 <1GB —— dial 启动时判定不足会把 log size 置0，可能导致 $SEAS_LOG 实际没有内容"
        fi
    fi
else
    info "$SDCARD_DIR 不存在 —— 按源码逻辑 dial 启动时会判定无 sdcard，$SEAS_LOG 不会被写入，本节日志分析无法进行"
fi

if [ -f "$SEAS_LOG" ]; then
    recent_err=$(tail -c 200000 "$SEAS_LOG" 2>/dev/null | grep -Ec 'ERROR|FATAL')
    if [ "$recent_err" -gt 0 ] 2>/dev/null; then
        warn "$SEAS_LOG 最近约200KB内容中有 $recent_err 条 ERROR/FATAL 日志"
        if [ "$VERBOSE" = "1" ]; then
            echo "---- 最近的 ERROR/FATAL ----"
            tail -c 200000 "$SEAS_LOG" 2>/dev/null | grep -E 'ERROR|FATAL' | tail -20
            echo "----------------------------"
        fi
    else
        ok "$SEAS_LOG 最近内容无 ERROR/FATAL"
    fi
    switch_cnt=$(tail -c 200000 "$SEAS_LOG" 2>/dev/null | grep -Ec '切换|switch.*roamlink|switch.*sim')
    [ "$switch_cnt" -gt 5 ] 2>/dev/null && warn "日志中检测到较多通道切换关键字（约 $switch_cnt 次，取样范围内）—— 若短时间内频繁切换，考虑是否是 TCP 探测单点目标(18.196.0.17)不稳定导致的'双通道抖动'，而非真实信道问题"
else
    info "$SEAS_LOG 不存在，跳过业务日志分析"
fi

if [ -f "$CHECK_NET_LOG" ]; then
    crash_cnt=$(grep -c "CRASH DETECTED" "$CHECK_NET_LOG" 2>/dev/null)
    if [ "$crash_cnt" -gt 0 ] 2>/dev/null; then
        warn "$CHECK_NET_LOG 中检测到 dial 崩溃重启记录 $crash_cnt 次 —— dial 进程存在崩溃-拉起循环，联网中断很可能与此相关，建议结合 core dump / 崩溃时间点比对 $SEAS_LOG"
    else
        ok "$CHECK_NET_LOG 无崩溃重启记录"
    fi
else
    info "$CHECK_NET_LOG 不存在"
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
