# MCU 引脚功能映射表

## 文档信息

| 项目 | 内容 |
| --- | --- |
| 所属项目 | 脉冲信号发生板 |
| MCU | STM32G474QET6（LQFP128） |
| 编写日期 | 2026-10-25 |
| 修订日期 | 2026-10-25 |

---

## 1. 芯片与时钟

| 项目 | 配置 |
| --- | --- |
| 封装 | LQFP128 |
| 外部晶振 | HSE 8 MHz（`PF0-OSC_IN` / `PF1-OSC_OUT`） |
| 系统时钟 | PLL，SYSCLK / HCLK = 170 MHz |
| 调试口 | SWD |

---

## 2. 引脚分配总表

方向以 MCU 为参考：I = 输入，O = 输出，I/O = 双向，A = 模拟。

| Pin Name | Signal on Pin | GPIO output level | GPIO mode | GPIO Pull-up/Pull-down | Maximum output speed | Function |
| --- | --- | --- | --- | --- | --- | --- |
| PB4 | n/a | n/a | External Interrupt Mode with Rising/Falling edge trigger detection | Pull-down | n/a | 端口启停输入 |
| PB5 | n/a | Low | Output Push Pull | No pull-up and no pull-down | Low | 运行指示 |
| PB6 | n/a | n/a | External Interrupt Mode with Rising edge trigger detection | Pull-down | n/a | R 波输入检测 |
| PB9 | n/a | High | Output Push Pull | No pull-up and no pull-down | Low | 板载 LED |
| PD8 | n/a | Low | Output Push Pull | No pull-up and no pull-down | Low | 杆 F MOSFET |
| PD9 | n/a | Low | Output Push Pull | No pull-up and no pull-down | Low | 杆 E MOSFET |
| PD10 | n/a | Low | Output Push Pull | No pull-up and no pull-down | Low | 杆 D MOSFET |
| PD11 | n/a | Low | Output Push Pull | No pull-up and no pull-down | Low | 杆 C MOSFET |
| PD12 | n/a | Low | Output Push Pull | No pull-up and no pull-down | Low | 杆 B MOSFET |
| PD13 | n/a | Low | Output Push Pull | No pull-up and no pull-down | Low | 杆 A MOSFET |
| PG7 | n/a | n/a | External Interrupt Mode with Rising edge trigger detection | Pull-down | n/a | 正通道过流锁存 |
| PG8 | n/a | n/a | External Interrupt Mode with Rising edge trigger detection | Pull-down | n/a | 负通道过流锁存 |
| PG9 | n/a | n/a | Output Push Pull | No pull-up and no pull-down | Low | 过流锁存复位 |
| PA0 | COMP3_INP; ADC1_IN1 | n/a | Analog | No pull-up and no pull-down | n/a | 正通道电流采样 |
| PA1 | COMP1_INP; ADC2_IN2 | n/a | Analog | No pull-up and no pull-down | n/a | 负通道电流采样 |
| PA8 | HRTIM1_CHA1 | n/a | Alternate Function Push Pull | No pull-up and no pull-down | Very High | 正通道脉冲输出 |
| PA9 | HRTIM1_CHA2 | n/a | Alternate Function Push Pull | No pull-up and no pull-down | Very High | 负通道脉冲输出 |
| PF0-OSC_IN | RCC_OSC_IN | n/a | n/a | n/a | n/a | 8 MHz 晶振输入 |
| PF1-OSC_OUT | RCC_OSC_OUT | n/a | n/a | n/a | n/a | 8 MHz 晶振输出 |
| PA13 | SYS_JTMS-SWDIO | n/a | n/a | n/a | n/a | SWDIO |
| PA14 | SYS_JTCK-SWCLK | n/a | n/a | n/a | n/a | SWCLK |
| PC4 | USART1_TX | n/a | Alternate Function Push Pull | No pull-up and no pull-down | Low | 上位机调试串口 |
| PC5 | USART1_RX | n/a | Alternate Function Push Pull | No pull-up and no pull-down | Low | 上位机调试串口 |
| PA2 | USART2_TX | n/a | Alternate Function Push Pull | No pull-up and no pull-down | Low | 上位机通信串口 |
| PA3 | USART2_RX | n/a | Alternate Function Push Pull | No pull-up and no pull-down | Low | 上位机通信串口 |
