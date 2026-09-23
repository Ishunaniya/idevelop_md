一、进入 license_pending 的确切条件

  roamlink_probe()(roamlink.c)按顺序判四种结果:

  ┌───────────────────────────┬──────────────────────┐
  │           检查            │    不满足 → 返回     │
  ├───────────────────────────┼──────────────────────┤
  │ RBMaster 二进制存在(X_OK) │ 否 → NO_PACKAGE      │
  ├───────────────────────────┼──────────────────────┤
  │ conf.ini 存在(F_OK)       │ 否 → CONF_MISSING    │
  ├───────────────────────────┼──────────────────────┤
  │ license 文件存在且 size>0 │ 否 → LICENSE_MISSING │
  ├───────────────────────────┼──────────────────────┤
  │ 三者都齐                  │ → PROBE_OK           │
  └───────────────────────────┴──────────────────────┘
  
  所以你说的对——LICENSE_MISSING = RBMaster + conf.ini 就绪,但 license 缺失/为空。
  
  但进入 license_pending 还要再过一道(dial.c:91-122):probe 返回 LICENSE_MISSING 后,先试 roamlink_license_restore_from_backup()(从 /data/ufs/license.cer 恢复):
  
  - 恢复成功 + 重新 probe OK → 正常用 roamlink,不进 license_pending;
  - 恢复失败(没有有效备份) → 才 license_pending=true。

  一个要修正的细节:进入 license_pending 不是立刻触发下载。顺序是:先把 network_policy 强制成 FORCE_SIM(4) → 让物理 SIM 先联网 → SIM 连上后(net_connected)才拉起 RBMaster 下载 
  license(dial.c:1193,用已经通的 SIM 网络去下)。下载是在 SIM 通了之后才开始的。

  二、"下载失败是不是就进策略4" —— 不准确,它一开始就是策略4
  
  关键事实:license_pending 期间 network_policy 从进入那刻就已经是 FORCE_SIM(4)(dial.c:120),覆盖了配置里的 network_select=1(PREFER_ROAMLINK)。log1 开头那句 policy=4 on startup 就是证据。
  
  所以:

  - 配置本是 PREFER_ROAMLINK(1);
  - license 缺失 → 被强制改成 FORCE_SIM(4),整个 license_pending 期间都是 4;
  - 下载失败(300s 超时)→ license_pending=false,但 network_policy 维持 4 不变。

  即:不是"失败后才进 4",而是一进 license_pending 就是 4,失败只是让这个 4 变成永久(stay FORCE_SIM)。

  三、问题是怎么发生的(完整 bug 链)

  1. 设备配 network_select=1(想用 roamlink),但 license 文件缺 + 无备份 → license_pending,强制 FORCE_SIM(4)。
  2. 物理 SIM 联网成功 → 拉起 RBMaster 下载 license(dial.c:1193,rbmaster_started=true)。
  3. RBMaster 一运行就"抢占模组通道"(开发者注释 dial.c:134-135)→ 数据呼叫地址变 IP/GW/DNS=0.0.0.0 → SDK/QMI 据此把 resolv.conf 写成 nameserver 0.0.0.0 → 域名解析死。(但真实 PDP
  仍在传包,裸 IP ping 8.8.8.8 仍通。)
  4. 300s 到,license 始终没下来(服务器不可达/未开通)→ 超时 → license_pending=false,但 RBMaster 没被停。
  5. RBMaster 继续跑 → 0.0.0.0 持续;又因裸 IP 通 → DownTime=0 → L1/L2/L3 分级恢复永不触发 → DNS 卡死,直到下次重启。

  对照两条不卡的路径:
  - license 下载成功 → backup + reboot(RB_AUTOBOOT) 整机重启 → 重启后 probe OK → 走 roamlink,DNS 正常(成功分支自带"干净复位")。
  - 纯 SIM 设备(network_select=4)→ 从不拉 RBMaster → DNS 一直正常。
  
  → bug 的本质:成功分支有 reboot 自愈,失败分支缺对称清理,把抢通道的 RBMaster 永久留着了。

  四、问题是怎么解决的(方案A,1.31.7)

  在 300s 超时失败分支(dial.c:491 起)补上失败侧的清理:
  
  p_dial_mng->license_pending = false;
  if (p_dial_mng->rbmaster_started) {
      roamlink_stop_service();          // 发 RBstopServiceMaster,释放通道、切回物理SIM
      p_dial_mng->rbmaster_started = false;
      dail_stop_data_call(...);         // 重拨
      p_dial_mng->is_func_called = true;
      p_dial_mng->dial_st = dial_stat_reg_check;
  }

  - 机理:停掉抢通道的 RBMaster → 物理 SIM 重新拨号 → 像纯 FORCE_SIM 设备一样拿回真实地址/DNS → resolv.conf 从 0.0.0.0 变回真实 DNS。
  - 为什么不在失败分支也 reboot:若 license 长期不可得(没订购/服务器挂),"等 300s→失败→重启"会变重启死循环;停 RBMaster + 留在物理 SIM(带可用 DNS)继续跑,才是稳态。
  - 最终行为:设备从"想用 roamlink 但卡死无 DNS",变成"降级跑物理 SIM、但 DNS 可用";下次重启再重试 license。license_pending=false + rbmaster_started=false
  后也不会再触发那段下载逻辑,不循环。

  一句话:问题 = license 缺失→强制 SIM→拉 RBMaster 下 license→RBMaster 抢通道致 DNS=0→下载失败却不停它→永久卡死;解决 = 在下载失败那一刻把 RBMaster 停掉并重拨,让物理 SIM 拿回真实 DNS。







怎么测(关键是"先复现 bug,再验证修复")
  
  前置:用 SDK 环境先把 1.31.7 编出来(没增删文件,cmake --build 即可)。
  
  第 1 步:构造触发条件(进入 license_pending)
  在一台 roamlink 设备(RBMaster + conf.ini 齐全)上:
  - network_select 设 1/2/3(任意 roamlink 策略,别用 4);
  - 删掉主 license 和备份:/usrdata/roamlink/etc/.pconfig/license.cer 和 /data/ufs/license.cer 都清掉/清空(备份在才会被 restore 救回,就进不了 license_pending);
  - 让 license 下不来(断开 roamlink 下载服务器,或就让它自然失败),确保 300s 内拿不到。看后续点！！

  第 2 步:旧固件(1.31.6)先复现 bug
  启动后等约 5 分钟(LICENSE_WAIT_TIMEOUT_SEC=300),看到 license wait timeout ... stay FORCE_SIM 后:
  - cat /var/run/resolv.conf → 应是 nameserver 0.0.0.0;
  - ping www.baidu.com 失败,但 ping 8.8.8.8 通 → bug 重现,且一直卡死。

  第 3 步:新固件(1.31.7)验证修复
  同样条件,超时后日志里应出现新行:
  [ROAMLINK] license wait timeout (...), giving up license_pending mode, stay FORCE_SIM
  [ROAMLINK] license failed, stopping RBMaster to free SIM channel and redial
  [STATE] ... -> reg_check        ...        [STATE] wait_for_connect -> net_connected
  [SDK] DataCall connected | profile=1 | IP=<真实> ... DNS=<真实，非0>
  然后:
  - cat /var/run/resolv.conf → 真实 DNS(不再是 0.0.0.0);
  - ping www.baidu.com → 解析成功;
  - 观察几分钟:设备不重启、不循环(验证"不在失败分支 reboot"的意图)。
  
  通过判据:超时后 resolv.conf 变真实 DNS + 域名能解析 + 无重启循环。

  别忘了的回归项(确认没碰坏其他路径)

  ┌───────────────────────────────────────┬─────────────────────────┬─────────────────────────────────────┐
  │                 场景                  │          期望           │                说明                 │
  ├───────────────────────────────────────┼─────────────────────────┼─────────────────────────────────────┤
  │ network_select=4 纯 SIM               │ DNS 正常                │ 根本不进 license_pending,本就健康   │
  ├───────────────────────────────────────┼─────────────────────────┼─────────────────────────────────────┤
  │ roamlink + license 有效               │ roamlink 正常(193.x)    │ 未触及该分支                        │
  ├───────────────────────────────────────┼─────────────────────────┼─────────────────────────────────────┤
  │ license_pending 期间 license 真下来了 │ 仍 backup + 整机 reboot │ 成功分支没改,务必单独验一次别被破坏 │
  └───────────────────────────────────────┴─────────────────────────┴─────────────────────────────────────┘

 进入 license_pending 的硬条件(按 probe 顺序)
  
  ┌────────────────────────────────────────────┬────────────┬───────────────────────────────────────────┐
  │                    文件                    │  状态要求  │               不满足的后果                │
  ├────────────────────────────────────────────┼────────────┼───────────────────────────────────────────┤
  │ /usrdata/roamlink/RBMaster                 │ 必须存在   │ 缺 → NO_PACKAGE → 直接 FORCE_SIM,不复现   │
  ├────────────────────────────────────────────┼────────────┼───────────────────────────────────────────┤
  │ /opt/conf.ini                              │ 必须存在   │ 缺 → CONF_MISSING → 直接 FORCE_SIM,不复现 │
  ├────────────────────────────────────────────┼────────────┼───────────────────────────────────────────┤
  │ /usrdata/roamlink/etc/.pconfig/license.cer │ 缺失或为空 │ 存在 → PROBE_OK → 正常用 roamlink,不复现  │
  ├────────────────────────────────────────────┼────────────┼───────────────────────────────────────────┤
  │ /data/ufs/license.cer(备份)                │ 缺失或为空 │ 存在 → 被 restore 救回,不进 pending       │
  └────────────────────────────────────────────┴────────────┴───────────────────────────────────────────┘
  
  加上 network.ini 里 network_select = 1/2/3(roamlink 策略)。
  
  ⚠️  关键坑:"没配置过"如果连 /opt/conf.ini 都没有(纯白板设备),probe 会返回 CONF_MISSING → 直接干净 FORCE_SIM、根本不拉 RBMaster,bug 复现不出来。必须是 conf.ini 在、但 license 不在
  这个特定状态(即"装了 roamlink、跑过 factoryApp,但 license 还没下下来")。

  第二个前提:下载必须在 300s 内失败
  
  这才是你担心的"动不了服务器"那点。逻辑是:
  
  - 如果这台设备/这张卡在 roamlink 服务器上是被授权的 → RBMaster 会下载成功 → 走成功分支 backup + 整机 reboot → 你测到的是成功路径,不是要验的失败分支。
  - 只有当服务器不给这台/这卡发 license 时,才会 300s 超时 → 命中失败分支。

  所以你不需要改服务器,只需要一台/一张服务器不会给它发 license 的设备/卡(没开通授权的就行)。怎么确认你测的是失败分支?看启动日志:

  - 出现 [ROAMLINK] license wait timeout (...) stay FORCE_SIM → 失败分支,对了;
  - 出现 [ROAMLINK] license file appeared! backing up and rebooting → 下载成功了,这卡被授权,换一张没授权的。

  我的建议:最省事的两个选择

  首选——直接用 log1 那台瑞典设备(ICCID 89464283216097005161):它本来就在自然复现这个 bug(license 缺失 + 下载失败),零搭建、零服务器操作。旧固件先确认 resolv.conf=0.0.0.0,刷 1.31.7
  看是否变真实 DNS。这是最干净的对照。

  次选——一台 conf.ini 在、license 没配过、且卡未授权的设备,上电前用这几条核对状态:
  
  ls -l /usrdata/roamlink/RBMaster                       # 要在
  ls -l /opt/conf.ini                                    # 要在
  ls -l /usrdata/roamlink/etc/.pconfig/license.cer       # 要"没有/0字节"
  ls -l /data/ufs/license.cer                            # 要"没有/0字节"
  # network.ini 里 network_select 是 1/2/3

  启动后等约 5 分钟,确认日志走到 license wait timeout ... stay FORCE_SIM,这就是失败分支,可以验修复了。

  一句话:不用动服务器,用一台"装了 roamlink 但 license 没授权下来"的设备即可;而最稳的就是直接拿 log1 那台已经在犯病的瑞典设备测。



