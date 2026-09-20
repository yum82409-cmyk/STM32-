# STM32F103 双机控制与 Proteus 仿真

[![Privacy and integrity check](https://github.com/yum82409-cmyk/STM32-/actions/workflows/privacy-and-integrity.yml/badge.svg)](https://github.com/yum82409-cmyk/STM32-/actions/workflows/privacy-and-integrity.yml)

这是学校电子设计竞赛题目的脱敏公开版工程。项目使用两片 STM32F103C8T6，实现 OLED 中文显示、流水灯、两路 PWM、电机调速与正反转、双机串口通信以及远程暂停/恢复。

## 主要功能

- A 机：四路流水灯、0.01 秒计时、PWM1 自动渐变、PWM2 档位调节、电机正反转。
- B 机：接收并显示两路 PWM 状态，判断电机停止/正常/异常，发送暂停与恢复命令。
- 通信：USART1，115200-8-N-1，定长帧头、长度字段和 XOR 校验。
- 仿真：Proteus 双机电路、OLED、L293D、电机、按键与数字示波器。

## 快速入口

- [赛题原文 PDF](赛题要求/2025年第7届电赛校赛初赛题目.pdf)
- [赛题要求 PPT](赛题要求/赛题要求.pptx)
- [Proteus 工程](Proteus/Dual_Board_Sim.pdsprj)
- [A 机源码与 Keil 工程](Firmware/A/)
- [B 机源码与 Keil 工程](Firmware/B/)
- [操作说明](docs/操作说明.md)
- [设计说明与验收报告](docs/设计说明与验收报告.md)
- [最终验收清单](FINAL_CHECKLIST.md)

仓库根目录中的 `A.hex`、`B.hex` 可直接用于交付。编译记录见 `docs/A_release_build.log` 与 `docs/B_release_build.log`。

需要直接下载时可打开 [最新发布页](https://github.com/yum82409-cmyk/STM32-/releases/latest)，其中附带 A/B 固件、赛题 PDF 和赛题 PPT。

## 运行仿真

1. 用 Proteus 8 打开 `Proteus/Dual_Board_Sim.pdsprj`。
2. 保持目录结构不变并启动仿真。
3. 需要查看 PWM1 时，在仿真运行后选择 `调试(D) → 5. Digital Oscilloscope`；双击原理图上的小示波器图标只会打开元件属性。

完整的按键位置、预期现象、串口协议和故障排查见 `docs/操作说明.md`。

## 编译

Keil 工程分别位于：

- `Firmware/A/MDK-ARM/A_Firmware.uvprojx`
- `Firmware/B/MDK-ARM/B_Firmware.uvprojx`

已归档的正式编译结果均为 `0 Error(s), 0 Warning(s)`。部分旧版 Keil 无法读取中文路径，遇到工程无法打开时请复制到纯 ASCII 路径后再编译。

## 隐私说明

公开版 OLED 姓名字段统一使用隐私别名“张三”，不包含参赛者真实姓名、Windows 用户名或本机绝对路径。字库可通过 `py tools/gen_font.py 显示别名` 重新生成；公开发布前请继续使用非真实称谓。

## 校验

关键交付物的 SHA-256 摘要记录在 `SHA256SUMS.txt`。

每次推送和合并请求都会自动校验关键文件哈希，并扫描已知个人标识和 Windows 用户目录。
