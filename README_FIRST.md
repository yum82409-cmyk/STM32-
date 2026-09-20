# STM32F103 双机控制项目公开版

## 快速入口

- Proteus 仿真：`Proteus/Dual_Board_Sim.pdsprj`
- A 机固件：`A.hex`
- B 机固件：`B.hex`
- A 机源码与 Keil 工程：`Firmware/A/`
- B 机源码与 Keil 工程：`Firmware/B/`
- 验收操作：`docs/操作说明.md`
- 设计与验收报告：`docs/设计说明与验收报告.md`
- 证据截图：`docs/evidence/`
- 赛题原文与 PPT：`赛题要求/`
- 中文字库生成脚本：`tools/gen_font.py`

## 当前正式配置

- `ACCEPTANCE_DEMO=0`：使用真实按键，不运行自动演示。
- A/B 两机 `DEBUG_DISPLAY=0`：OLED 不显示调试计数器。
- 两机时钟与 Proteus 统一为 8MHz。
- 2026-09-20 重新编译结果：A、B 均为 `0 Error(s), 0 Warning(s)`。
- 为保护隐私，A 机 OLED 姓名统一使用别名“张三”。仓库不包含参赛者真实姓名、账户路径或机器名。

## 打开仿真

保持本目录结构不变，打开 `Proteus/Dual_Board_Sim.pdsprj` 后直接运行。原理图使用相对固件路径，`单片机/` 目录中的 HEX 镜像已经随包提供。

独立示波器的正确打开方式：仿真运行后选择 `调试(D) → 5. Digital Oscilloscope`。双击原理图中的小示波器图标只会打开元件属性。

## 隐私说明

本仓库用于公开展示和复现，姓名字段使用隐私别名“张三”。如在非公开环境中更换显示文字，请运行 `py tools/gen_font.py 显示别名`，更新 A 机枚举后重新编译，并同步 `A.hex` 与 `单片机/` 下的 Proteus 固件镜像。
