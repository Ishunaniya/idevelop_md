# EC200A-CN(TA) QuecOpen 文档分析索引（ec200a_wd）

> 本目录收录针对移远 EC200A-CN(TA) QuecOpen 原厂 PDF 的逐页完整分析（Markdown）。原始 PDF 见 `../ec200a_pdf/` 及工程根目录。
> 最近更新：2026-06-04 新增 FTP / I2C / RGMII / Trusted Boot / 设备树开发 五篇。

## 一、本次新增（2026-06-04）

| 文档 | 说明 | 源 PDF 版本/日期 |
|---|---|---|
| [FTP 服务用户指导](EC200A_QuecOpen_FTP服务用户指导_完整分析.md) | 三种启动方式（shell/init.d/inetd）脚本、账号密码设置、FileZilla 验证 | V1.0.0 / 2022-07-19 |
| [I2C 开发指导](EC200A_QuecOpen_I2C_开发指导_完整分析.md) | twsi0 引脚、三处设备树配置源码、sample/i2c 示例、调试方法 | V1.0.0 / 2022-06-29 |
| [RGMII 应用指导](EC200A_QuecOpen_RGMII_应用指导_完整分析.md) | EMAC/PHY 设备树、驱动加载、YT8521 五张寄存器位域表、调试步骤 | V1.0.0 / 2022-07-21 |
| [Trusted Boot 应用指导](EC200A_QuecOpen_Trusted_Boot_应用指导_完整分析.md) | 信任链流程、BLF 配置、RSA 密钥、镜像签名与烧录验证 | V1.0.0 / 2022-10-21 |
| [设备树开发指导](EC200A_QuecOpen_设备树开发指导_完整分析.md) | DTS/DTSI/DTC/DTB、overlay 用法、新增 GPIO 字段与内核解析 API | V1.0.0 / 2022-07-12 |

## 上一批新增（2026-06-03）

| 文档 | 说明 | 源 PDF 版本/日期 |
|---|---|---|
| [UART 开发指导](EC200A_QuecOpen_UART_开发指导_完整分析.md) | 两路串口（DEBUG/MAIN）引脚、设备树、编译与回环/流控验证 | V1.0.0 / 2022-08-09 |
| [固件烧录指导](EC200A_QuecOpen_固件烧录指导_完整分析.md) | SWDownloader.exe + USB 驱动烧录流程、强制下载模式 | V1.0.0 / 2022-07-08 |
| [分区调整指导](EC200A_QuecOpen_分区调整指导_完整分析.md) | flash layout / uboot / blf 调整、新增 backup_data、UBI 制作 | V1.0.0 / 2023-03-20 |
| [Wi-Fi API 参考手册](EC200A_QuecOpen_Wi-Fi_API参考手册_完整分析.md) | 67 个 Wi-Fi API（AP/STA/P2P）+ 结构体/枚举 + 示例 | V1.0.0 / 2023-01-10 |

## 二、系统与开发

- [快速开发指导](EC200A-CN_TA_QuecOpen_快速开发指导_完整分析.md)
- [AT 指令手册](EC200A-CN_QuecOpen_AT_Commands_Manual_完整分析.md)
- [设备管理指导](EC200A-CNTA_QuecOpenSDK_设备管理指导_完整分析.md)
- [网络管理使用指导](EC200A-CNTA_QuecOpen_网络管理使用指导_完整分析.md)
- [SELinux 应用指导](EC200A-CN_QuecOpen_SELinux_应用指导_完整分析.md)
- [低功耗模式应用指导](EC200A_low_power.md)
- [Linux 系统时间同步](EC200A_time_sync.md)
- [A/B 系统升级指导](EC200A_ab_upgrade.md)
- [USB 配置指导](EC200A_QuecOpen_USB_配置指导.md)
- [FTP 服务用户指导](EC200A_QuecOpen_FTP服务用户指导_完整分析.md)
- [设备树开发指导](EC200A_QuecOpen_设备树开发指导_完整分析.md)
- [Trusted Boot 应用指导](EC200A_QuecOpen_Trusted_Boot_应用指导_完整分析.md)
- [Log 抓取指导](quectel_log_guide_analysis.md)

## 三、存储与外设

- [eMMC 用户指导](EC200A-CNTA_QuecOpen_eMMC用户指导_完整分析.md)
- [分区调整指导](EC200A_QuecOpen_分区调整指导_完整分析.md)
- [UART 开发指导](EC200A_QuecOpen_UART_开发指导_完整分析.md)
- [SPI 开发指导](EC200A_spi_guide.md)
- [I2C 开发指导](EC200A_QuecOpen_I2C_开发指导_完整分析.md)
- [RGMII 应用指导（以太网 PHY）](EC200A_QuecOpen_RGMII_应用指导_完整分析.md)
- [GPIO API 参考手册](EC200A_QuecOpen_GPIO_API_参考手册.md)
- [ADC 用户指导](EC200A_QuecOpen_ADC_用户指导_分析.md)

## 四、通信能力 API

- [数据拨号 API 参考手册](EC200A_QuecOpen_数据拨号API参考手册_详细分析.md)
- [蜂窝网络信息 API 参考手册](EC200A_cellular_api.md)
- [SIM API 参考手册](EC200A_QuecOpen_SIM_API参考手册_分析.md)
- [SMS API 参考手册](EC200A_QuecOpen_SMS_API_参考手册_全面分析.md)
- [Wi-Fi API 参考手册](EC200A_QuecOpen_Wi-Fi_API参考手册_完整分析.md)

## 五、固件烧录

- [固件烧录指导](EC200A_QuecOpen_固件烧录指导_完整分析.md)

## 六、LTE Standard(A) 系列固件升级

- [LTE Standard(A) 系列 DFOTA 升级指导 V1.5](LTE_StandardA_DFOTA_升级指导_V1.5_完整分析.md)
