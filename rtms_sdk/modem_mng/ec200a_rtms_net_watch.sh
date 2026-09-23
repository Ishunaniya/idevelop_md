#!/bin/sh
#
# ec200a_rtms_net_watch.sh — 间歇性联网故障 常驻采样 + 边沿自动深抓
#   针对"时好时坏"：常驻轻量采样，一旦从"通"翻到"断"自动 dump dmesg + 跑诊断脚本，
#   把间歇故障的"过渡瞬间"抓下来（无人值守，不依赖有人在场）。
#
# 设计原则（和 ec200a_rtms_diag_network.sh 一致）：
#   1. 全程只读，绝不发 AT 指令 —— REG/RSRP/RSRQ 一律解析 modem_mng 自己打的日志
#      （[HEARTBEAT]/[DIAG]），CSQ 退读 /tmp/network_csq；状态以 modem_mng 的
#      /tmp/network_status 为准（1=通）。绝不主动 Ql_SendAT/atc，免和 dial_task 抢串口。
#   2. busybox ash 兼容；工具用前 command -v 探测。
#   3. 不刷固件也能用：拷到设备直接跑（连同 ec200a_rtms_diag_network.sh 一起拷）。
#
# 用法：
#   nohup sh ec200a_rtms_net_watch.sh >/dev/null 2>&1 &      # 后台常驻
#   # 设备重启后需重新拉起（可加到 rc.local / cron @reboot）
#
# 产出（都落 SD 卡，跨重启保留）：
#   /media/sdcard/dial_log/intermittent/watch_samples.log        连续采样，一行一条
#   /media/sdcard/dial_log/intermittent/edge_down_<ts>/          每次"通→断"的深快照目录
#       ├─ dmesg.log      内核/CP 复位、assert（间歇 RF/CP 问题的铁证；环形缓冲，必须及时抓）
#       ├─ svc.log        ps + ql_netd/ql_rild 存活 + 就绪标志 + ccinet 接口 + 路由/DNS
#       ├─ diag.log       完整 ec200a_rtms_diag_network.sh 输出（若脚本在设备上）
#       └─ dial_tail.log  当时 modem_mng 日志尾部 200 行
#
# 怎么读结论（把几次 edge_down 的 dmesg/svc/采样趋势对齐）：
#   · 断的瞬间 RSRP/RSRQ 跳水、REG 掉、但 ql_netd/ql_rild 正常 → RF/天线（物理层）
#   · ql_netd 活着但 ready 标志消失 / ccinet 丢 IP        → 数据服务卡"未就绪"（服务层）
#   · dmesg 里有 CP reset / assert                        → CP 侧被 RF 事件卡住
#   · dial_tail 里出现 data_call_init failed/SERVICE_NOT_READY → init 期没起来

INTERVAL=20                                  # 采样间隔（秒）
COOLDOWN=180                                 # 深抓冷却，防状态抖动时反复深抓
OUT_DIR="/media/sdcard/dial_log/intermittent"
SAMPLE_LOG="$OUT_DIR/watch_samples.log"
SAMPLE_MAX_BYTES=5000000                     # 采样日志超过 ~5MB 就滚动一次，防塞满 SD
LOG_ROOT="/media/sdcard/dial_log"
NET_STATUS="/tmp/network_status"             # ec200a/nw/nw.h，1=通 0=断
NET_CSQ="/tmp/network_csq"
QL_NET_READY_FLAG="/tmp/ql_net_srv_ready.flag"   # A02 SDK 实证；别的固件可能不同，仅供参考
QL_NET_IPC_PATH="/tmp/.ql_net_ipc_path"
# 诊断脚本在设备上的位置（把 ec200a_rtms_diag_network.sh 拷到这里，或改成实际路径）
DIAG_SCRIPT="/media/sdcard/ec200a_rtms_diag_network.sh"

have() { command -v "$1" >/dev/null 2>&1; }
mkdir -p "$OUT_DIR" 2>/dev/null

# 找当前 modem_mng 日志：按天目录优先，退平铺
find_latest_log() {
    today=$(date +%Y-%m-%d 2>/dev/null)
    for d in "$LOG_ROOT/$today" "$LOG_ROOT/unsynced"; do
        if [ -d "$d" ]; then
            f=$(ls -t "$d"/dial_*.log 2>/dev/null | head -1)
            [ -n "$f" ] && { echo "$f"; return; }
        fi
    done
    ls -t "$LOG_ROOT"/dial_*.log 2>/dev/null | head -1
}

# 从最新日志取 最后一条 HEARTBEAT 的 REG/CSQ、最后一条 DIAG 的 RSRP/RSRQ。
# 注意：DIAG 打得稀疏，RSRP/RSRQ 可能偏旧（属"最近一次已知"，够做相关性判断）。
read_rf() {
    reg="?"; csq="?"; rsrp="?"; rsrq="?"
    LOG=$(find_latest_log)
    if [ -n "$LOG" ] && [ -f "$LOG" ]; then
        hb=$(grep '\[HEARTBEAT\]' "$LOG" 2>/dev/null | tail -1)
        r=$(echo "$hb" | sed -n 's/.*REG:\([0-9-]*\).*/\1/p'); [ -n "$r" ] && reg="$r"
        c=$(echo "$hb" | sed -n 's/.*CSQ:\([0-9-]*\).*/\1/p'); [ -n "$c" ] && csq="$c"
        dg=$(grep '\[DIAG\]' "$LOG" 2>/dev/null | tail -1)
        p=$(echo "$dg" | sed -n 's/.*RSRP:\([0-9-]*\).*/\1/p'); [ -n "$p" ] && rsrp="$p"
        q=$(echo "$dg" | sed -n 's/.*RSRQ:\([0-9-]*\).*/\1/p'); [ -n "$q" ] && rsrq="$q"
    fi
    if [ "$csq" = "?" ] && [ -f "$NET_CSQ" ]; then csq=$(cat "$NET_CSQ" 2>/dev/null); fi
}

ccinet_ip() {
    if have ip; then
        ip -4 -o addr show 2>/dev/null | grep ccinet | awk '{print $4}' | head -1
    elif have ifconfig; then
        ifconfig 2>/dev/null | grep -A1 '^ccinet' | grep -o 'inet addr:[0-9.]*' | cut -d: -f2 | head -1
    fi
}

# 当前状态 up/down：以 modem_mng 的 network_status 为准；文件缺则退一次 ping（ping 不碰 AT 口）
cur_state() {
    if [ -f "$NET_STATUS" ]; then
        [ "$(cat "$NET_STATUS" 2>/dev/null)" = "1" ] && echo up || echo down
    elif have ping; then
        ping -c1 -W2 8.8.8.8 >/dev/null 2>&1 && echo up || echo down
    else
        echo unknown
    fi
}

deep_capture() {
    reason="$1"
    ts=$(date '+%Y%m%d_%H%M%S' 2>/dev/null)
    d="$OUT_DIR/edge_${reason}_${ts}"
    mkdir -p "$d" 2>/dev/null
    dmesg > "$d/dmesg.log" 2>/dev/null
    # 服务层证据（和 diag.c 的 svc 同款，脱离固件也能抓）：纯采集、不断言，缺项只落无害错误行
    { echo '=== ps ==='; ps w 2>/dev/null || ps 2>/dev/null
      echo '=== ql procs (移远服务) ==='; ps w 2>/dev/null | grep '[q]l_'
      echo '=== ready flags (/tmp/*.flag) ==='; ls -l /tmp/*.flag 2>&1
      echo '=== ql svc tmp files ==='; ls -la "$QL_NET_READY_FLAG" "$QL_NET_IPC_PATH" 2>&1; ls -la /tmp 2>/dev/null | grep -i ql
      echo '=== interfaces ==='; ip -o link show 2>/dev/null; ip -o addr show 2>/dev/null; cat /proc/net/dev 2>/dev/null
      echo '=== ccinet rx ==='; for s in /sys/class/net/ccinet*/statistics/rx_packets; do echo "$s=$(cat "$s" 2>/dev/null)"; done
      echo '=== route/dns ==='; ip route show 2>/dev/null; cat /etc/resolv.conf 2>/dev/null
    } > "$d/svc.log" 2>&1
    [ -f "$DIAG_SCRIPT" ] && sh "$DIAG_SCRIPT" > "$d/diag.log" 2>&1
    LOG=$(find_latest_log); [ -n "$LOG" ] && tail -200 "$LOG" > "$d/dial_tail.log" 2>/dev/null
    echo "[$(date '+%F %T')] ===== DEEP CAPTURE ($reason) -> $d =====" >> "$SAMPLE_LOG"
}

rotate_if_big() {
    [ -f "$SAMPLE_LOG" ] || return 0
    sz=$(wc -c < "$SAMPLE_LOG" 2>/dev/null)
    case "$sz" in ''|*[!0-9]*) return 0 ;; esac
    [ "$sz" -gt "$SAMPLE_MAX_BYTES" ] && mv "$SAMPLE_LOG" "$SAMPLE_LOG.1" 2>/dev/null
}

prev="init"
last_deep=0
echo "[$(date '+%F %T')] watch started, interval=${INTERVAL}s cooldown=${COOLDOWN}s" >> "$SAMPLE_LOG"

while :; do
    rotate_if_big
    st=$(cur_state)
    read_rf
    ip4=$(ccinet_ip)
    netd="down"; pgrep -f "ql_netd" >/dev/null 2>&1 && netd="up"
    rild="down"; pgrep -f "ql_rild" >/dev/null 2>&1 && rild="up"
    flag="no"; [ -e "$QL_NET_READY_FLAG" ] && flag="yes"
    # catch-all（不假设确切进程名/标志名，适配不同固件版本）：
    #   qlprocs = 移远(ql_)服务进程总数（[q]l_ 避免 grep 匹配自身）
    #   flags   = /tmp 下所有 *.flag（各服务 ready 标识）的文件名清单
    qlprocs=$(ps w 2>/dev/null | grep -c '[q]l_'); [ -z "$qlprocs" ] && qlprocs=0
    flags=$(ls /tmp/*.flag 2>/dev/null | sed 's#.*/##' | tr '\n' ',' | sed 's/,$//'); [ -z "$flags" ] && flags=none

    echo "[$(date '+%F %T')] state=$st REG=$reg CSQ=$csq RSRP=$rsrp RSRQ=$rsrq ccinet=${ip4:-none} ql_netd=$netd ql_rild=$rild ready=$flag qlprocs=$qlprocs flags=$flags" >> "$SAMPLE_LOG"

    # 通→断 边沿：自动深抓（带冷却，防抖动刷屏）
    if [ "$prev" = "up" ] && [ "$st" = "down" ]; then
        now=$(date +%s 2>/dev/null)
        [ -z "$now" ] && now=0
        if [ $((now - last_deep)) -ge "$COOLDOWN" ]; then
            deep_capture "down"
            last_deep=$now
        fi
    fi
    [ "$st" != "unknown" ] && prev="$st"
    sleep "$INTERVAL"
done
