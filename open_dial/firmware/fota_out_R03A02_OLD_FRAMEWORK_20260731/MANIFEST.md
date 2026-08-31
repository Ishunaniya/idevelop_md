# 构建清单与校验记录

## 最终产物

| 文件 | 字节 | MD5 | SHA-256 |
|---|---:|---|---|
| `quectel_AB_OTA.img` | 33,595,392 | `1cae3817a6e84db3fcfb00173ca4fc04` | `b3df54dddac322a52ffe9383eb14c6b48208c9ef159455a471276e7989bebbc6` |
| `test_absys` | 15,788 | `4e51ce8968441a25e9834678dc010116` | `59f4e7d9200e1f2cef76670335385d2b315da6c5d744ea4b883654da4faa226d` |
| `root.squashfs` | 17,805,605 | `3bb7c7fd30f9220018c754c2ae663794` | `d2586fe74d546021787d73ab94d04e63e8cc098c30b4c06c4e89d2c149d0afd3` |
| `obm/quectel_obm_ota.img` | 172,032 | `9e94f2ace033943236077a9d0ae4e5ac` | `e7876286ddfbe157f638352f26dbcc52aed22d4dbad8f581c493df6cc68a2974` |
| `obm/quectel_obm.md5` | 33 | 内容为 `c96d2f1a72dcba67bae85661ee81e64a` | 见 `sha256sums.txt` |
| `obm/quectel_obm.size` | 7 | 内容为 `112524` | 见 `sha256sums.txt` |

## 源 ZIP

| 项目 | 值 |
|---|---|
| 文件 | `IMAGE_EC200ACNTAR03A02M2G_OCPU_OLD_FRAMEWORK_20260731.zip` |
| 字节 | 28,541,105 |
| MD5 | `09be6b459e708674677dc796f95b60c3` |
| SHA-256 | `3eb7552e4f5a58316cfab61238cd552cfd72faea83fbab07863adde9218c6b8f` |

`test_absys` 取自移远 R03A02 FOTA 目录；它与本机 R02A04 SDK
`sample/absys/test_absys` 的大小和 MD5 完全相同，适合在升级前的 R02A04 系统运行。

## 源镜像 MD5

| 文件 | MD5 | 在最终 FOTA 中的处理 |
|---|---|---|
| ARBEL.bin | `bb9fc75beab6642712dd0e2d07815d9c` | 原样 |
| MSA.bin | `b959fcedcda00a0e0bf4927c78cb123a` | 原样 |
| RFPLUGIN.bin | `b5c2630b18dd76a645915f5fc0a7ffd1` | 原样 |
| u-boot.bin | `42a30fa7848b32f28d66729a777edeab` | 原样 |
| zImage | `b66bbd241115e131763728debcc61718` | 原样 |
| tos.bin | `973c4889c2b988ee53f693386cc52413` | 原样 |
| oem_data.squashfs | `66174a298d9c50fef1534c0992cdd3ea` | 原样 |
| root.squashfs | `f03cb31992ea2e396ac34c45ea9f9692` | 加入 OBM 三文件后重封并重建 dm-verity |
| TLoader_QSPINAND.bin | `c96d2f1a72dcba67bae85661ee81e64a` | 用于生成 OBM 三文件 |
| hotfix.bin | `068c5a7b39ddc56028cae087e08ca5bf` | 作为 TIM/OBM 生成输入 |

## 版本和 rootfs 关键文件

```text
Project Name: EC200A-CNTA
Project Rev : EC200ACNTAR03A02M2G_OCPU
Build   Date: Jul 31 2026 10:05:57

/system/etc/mversion:
OW21.02_asr1803p401_rls988_1.057.067_20251120_13_59_bld1547
```

已在最终 `root.squashfs` 逐项确认：

| 路径 | 字节 | 权限 |
|---|---:|---:|
| `/usr/dial/dial` | 92,884 | 0755 |
| `/usr/sbin/start_prog` | 10,904 | 0755 |
| `/lib/preinit/83_upgrade_obm` | 4,320 | 0755 |
| `/usr/bin/ql_ota_obm` | 34,008 | 0755 |
| `/boot/quectel_obm_ota.img` | 172,032 | 0644 |
| `/boot/quectel_obm.md5` | 33 | 0644 |
| `/boot/quectel_obm.size` | 7 | 0644 |

## dm-verity

```text
SquashFS 基础长度: 17657856
最终 rootfs 长度: 17805605
ROOT_SECTORS: 34488
HASH_BLOCKS: 4311
Root hash: e2be6bae8296d1e9ca1f6d8f5810ab8f60767e239d51d79047fe94fd9e82f914
Salt: 8ff64b352edcb77a5b25c82d3b4eb2a1bbb33046093c1dd65c21eeb4a71696a0
veritysetup verify: 返回 0
```

dm-verity 的 salt 和最终 rootfs 哈希是本次构建结果；重新构建时可能改变，必须以新构建
日志和校验文件为准。

## 验证等级

- 已完成：来源校验、逐字节输入比对、rootfs 内容检查、分区容量检查、dm-verity
  验证、官方工具打包成功检查。
- 未完成：设备写入、切槽、R03 启动、OBM 实际更新、A/B 回滚兼容、业务/网络/射频
  回归、长稳测试。
