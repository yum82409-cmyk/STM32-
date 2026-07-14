# STM32 Dual-Board PWM Motor Control System

这是一个基于两块 **STM32F103C8T6** 的双机联动项目。A 机负责按键输入、双路 PWM、电机方向控制和本地 OLED 显示；B 机通过 USART1 接收 A 机状态，在另一块 OLED 上同步显示 PWM 与运行状态。

项目包含 STM32CubeMX 配置、Keil MDK 工程、Proteus 仿真工程、需求文档和运行结果截图，可用于课程设计、双机串口通信及 PWM 电机控制实验。

## 功能概览

- A/B 两块 STM32F103C8T6 通过 USART1 双向通信
- 两路 PWM 占空比调节与 OLED 实时显示
- 直流电机启停和正反转控制
- 四路 LED 流水效果
- 按键切换 PWM、方向和系统暂停状态
- 非阻塞定时器节拍与串口中断接收
- Proteus 仿真工程和实机/仿真结果截图

## 系统分工

### A 机

- 读取 PA5、PA6、PA7 按键
- 通过 TIM1_CH1（PA8）输出 PWM1
- 通过 TIM3_CH3（PB0）输出 PWM2
- 使用 PB1、PB2 控制电机方向
- 使用 PA1 至 PA4 驱动状态 LED
- 通过 PB6、PB7 软件 I2C 驱动 OLED
- 通过 USART1 向 B 机发送状态

### B 机

- 通过 USART1 接收 A 机状态
- 解析 `PWM1:<value>,PWM2:<value>` 文本协议
- 通过 PB6、PB7 软件 I2C 驱动 OLED
- 通过 PA5 按键向 A 机发送暂停/恢复命令 `S`

## 双机连接

| A 机 | B 机 | 说明 |
| --- | --- | --- |
| PA9 / USART1_TX | PA10 / USART1_RX | A 机发送状态 |
| PA10 / USART1_RX | PA9 / USART1_TX | B 机发送控制命令 |
| GND | GND | 必须共地 |

两块板均采用 USART1、8 数据位、1 停止位、无校验、无硬件流控。波特率以各工程当前配置为准，烧录前应确认两端一致。

## 目录结构

```text
.
|-- A_Machine_Code/          # A 机 CubeMX 与 Keil 工程
|   |-- A_Machine.ioc
|   |-- Core/
|   |-- Drivers/
|   `-- MDK-ARM/A_Machine.uvprojx
|-- B_Machine_Code/          # B 机 CubeMX 与 Keil 工程
|   |-- Machine_B.ioc
|   |-- Core/
|   |-- Drivers/
|   `-- MDK-ARM/Machine_B.uvprojx
|-- finished images/         # 运行结果截图
|-- Circuit Images.pdsprj    # Proteus 仿真工程
`-- Requirements.pdf         # 项目需求
```

## 构建与烧录

### 环境

- Keil MDK-ARM 5
- STM32CubeMX（需要重新生成初始化代码时）
- STM32F1 HAL 驱动（已包含在工程目录中）
- ST-Link 或其他兼容下载器

### A 机

1. 使用 Keil 打开 `A_Machine_Code/MDK-ARM/A_Machine.uvprojx`。
2. 选择 `STM32F103C8Tx` 目标器件。
3. 编译工程并确认无错误。
4. 将固件烧录到 A 机。

### B 机

1. 使用 Keil 打开 `B_Machine_Code/MDK-ARM/Machine_B.uvprojx`。
2. 选择 `STM32F103C8Tx` 目标器件。
3. 编译工程并确认无错误。
4. 将固件烧录到 B 机。

烧录完成后连接串口交叉线和公共地，再分别连接 OLED、电机驱动、按键和 LED。建议先在电机断电状态下验证 OLED、按键和双机通信，再接通电机电源。

## 运行结果

| 状态 | 截图 |
| --- | --- |
| PWM 20% | ![PWM 20 percent](finished%20images/PWM20%25normal.png) |
| PWM 40% | ![PWM 40 percent](finished%20images/PWM40%25normal.png) |
| PWM 80% 异常记录 | ![PWM 80 percent abnormal](finished%20images/PWM80%25abnormal.png) |
| 系统停止 | ![System stopped](finished%20images/STOP.png) |

## 调试要点

- 两块控制板必须共地，否则串口通信可能不稳定。
- USART1 的 TX/RX 需要交叉连接，并确保两端参数一致。
- 电机驱动电源应与逻辑电源分开核对，首次测试建议限流。
- OLED 无显示时，优先检查 PB6/PB7 接线、供电和 I2C 地址。
- PWM 80% 的异常截图已保留在仓库中，可作为后续排查电源、电机负载和驱动能力的依据。

## 后续改进

- 将串口文本协议增加帧头、校验和及超时恢复机制
- 把 OLED、按键和电机逻辑拆分为独立模块
- 增加速度闭环和电流保护
- 清理历史构建产物，并通过 Release 提供可烧录的 HEX 文件
- 补充完整接线图和实物照片标注
