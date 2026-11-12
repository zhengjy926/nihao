# MCU外设与引脚配置

## 文档信息

| 项目 | 内容 |
| --- | --- |
| 所属项目 | 脉冲信号发生板 |
| MCU | STM32G474QET6（LQFP128） |
| 依据 | `E:\Working Item\DSL\DSL_PFA\PFA_PSG_V2.0\Project\PFA_PSG.ioc` |
| 编写日期 | 2026-10-25 |
| 修订日期 | 2026-11-13 |

---

## 1. 芯片与时钟

| 项目 | 配置 |
| --- | --- |
| 封装 | LQFP128 |
| 时钟源 | HSE 8 MHz（`PF0-OSC_IN` / `PF1-OSC_OUT`） |
| PLL | HSE → ÷M(2) → ×N(85) → 340 MHz（VCO） |
| 系统时钟 | SYSCLK / HCLK = 170 MHz（VCO ÷R(2)） |
| ADC1/2 时钟 | 48.57 MHz（VCO ÷P(7)，ASYNC DIV1） |
| 调试口 | SWD（`PA13` SWDIO / `PA14` SWCLK） |
| SysTick | 使能（优先级 15） |

---

## 2. 引脚分配总表（按外设分组）

方向以 MCU 为参考：I = 输入，O = 输出，I/O = 双向，A = 模拟。

| 所属外设 | Pin Name | GPIO Label | Signal on Pin | GPIO output level | GPIO mode | GPIO Pull-up/Pull-down | Maximum output speed | Function |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| RCC | PF0-OSC_IN | n/a | RCC_OSC_IN | n/a | n/a | n/a | n/a | 8 MHz 晶振输入 |
| RCC | PF1-OSC_OUT | n/a | RCC_OSC_OUT | n/a | n/a | n/a | n/a | 8 MHz 晶振输出 |
| SYS | PA13 | n/a | SYS_JTMS-SWDIO | n/a | n/a | n/a | n/a | SWDIO |
| SYS | PA14 | n/a | SYS_JTCK-SWCLK | n/a | n/a | n/a | n/a | SWCLK |
| ADC/COMP | PA0 | n/a | COMP3_INP; ADC1_IN1 | n/a | Analog | No pull-up and no pull-down | n/a | 正通道电流采样 |
| ADC/COMP | PA1 | n/a | COMP1_INP; ADC2_IN2 | n/a | Analog | No pull-up and no pull-down | n/a | 负通道电流采样 |
| HRTIM1 | PA8 | n/a | HRTIM1_CHA1 | n/a | Alternate Function Push Pull | No pull-up and no pull-down | Very High | 正通道脉冲输出 |
| HRTIM1 | PA9 | n/a | HRTIM1_CHA2 | n/a | Alternate Function Push Pull | No pull-up and no pull-down | Very High | 负通道脉冲输出 |
| EXTI | PB4 | PULSE_START | GPIO_EXTI4 | n/a | External Interrupt Mode with Rising/Falling edge trigger detection | Pull-down | n/a | 端口启停输入 |
| EXTI | PB6 | TRIN | GPIO_EXTI6 | n/a | External Interrupt Mode with Rising edge trigger detection | Pull-down | n/a | R 波输入检测 |
| EXTI | PG7 | OCP_N | GPIO_EXTI7 | n/a | External Interrupt Mode with Rising edge trigger detection | Pull-down | n/a | 负通道过流锁存 |
| EXTI | PG8 | OCP_P | GPIO_EXTI8 | n/a | External Interrupt Mode with Rising edge trigger detection | Pull-down | n/a | 正通道过流锁存 |
| GPIO 输出 | PB5 | PULSE_BUSY | GPIO_Output | Low | Output Push Pull | No pull-up and no pull-down | Low | 运行指示 |
| GPIO 输出 | PB9 | LED | GPIO_Output | High | Output Push Pull | No pull-up and no pull-down | Low | 板载 LED |
| GPIO 输出 | PD8 | POLE_F | GPIO_Output | Low | Output Push Pull | No pull-up and no pull-down | Low | 杆 F MOSFET |
| GPIO 输出 | PD9 | POLE_E | GPIO_Output | Low | Output Push Pull | No pull-up and no pull-down | Low | 杆 E MOSFET |
| GPIO 输出 | PD10 | POLE_D | GPIO_Output | Low | Output Push Pull | No pull-up and no pull-down | Low | 杆 D MOSFET |
| GPIO 输出 | PD11 | POLE_C | GPIO_Output | Low | Output Push Pull | No pull-up and no pull-down | Low | 杆 C MOSFET |
| GPIO 输出 | PD12 | POLE_B | GPIO_Output | Low | Output Push Pull | No pull-up and no pull-down | Low | 杆 B MOSFET |
| GPIO 输出 | PD13 | POLE_A | GPIO_Output | Low | Output Push Pull | No pull-up and no pull-down | Low | 杆 A MOSFET |
| GPIO 输出 | PG9 | OCP_RST | GPIO_Output | Low | Output Push Pull | No pull-up and no pull-down | Low | 过流锁存复位 |
| USART1 | PC4 | n/a | USART1_TX | n/a | Alternate Function Push Pull | No pull-up and no pull-down | Low | 单板调试版协议口 TX（编译期二选一） |
| USART1 | PC5 | n/a | USART1_RX | n/a | Alternate Function Push Pull | No pull-up and no pull-down | Low | 单板调试版协议口 RX（编译期二选一） |
| USART2 | PA2 | n/a | USART2_TX | n/a | Alternate Function Push Pull | No pull-up and no pull-down | Low | 发布版协议口 TX |
| USART2 | PA3 | n/a | USART2_RX | n/a | Alternate Function Push Pull | No pull-up and no pull-down | Low | 发布版协议口 RX |

> 注：`PB4`（EXTI4）在 `.ioc` 中未使能 `NVIC.EXTI4_IRQn`，其中断处理需在代码/台架确认；`PB6` / `PG7` / `PG8` 走 `EXTI9_5_IRQn`（优先级 4）。

---

## 3. 外设配置

### 3.1 ADC1 / ADC2

| 参数 | ADC1 | ADC2 |
| --- | --- | --- |
| 采样通道 | CH1（IN1） | CH2（IN2） |
| 引脚 | PA0 | PA1 |
| 分辨率 | 12 bit | 12 bit |
| 采样时间 | 12.5 周期 | 12.5 周期 |
| 外部触发源 | HRTIM1 TRG1（Timer B CMP3） | HRTIM1 TRG3（Timer B CMP4） |
| 时钟预分频 | ASYNC DIV1 | ASYNC DIV1 |
| 过采样 | 禁用 | 禁用 |
| DMA | 循环，DMA1_Channel5 | 循环，DMA1_Channel6 |

### 3.2 COMP1 / COMP3

| 参数 | COMP1 | COMP3 |
| --- | --- | --- |
| 正输入 INP | PA1（负通道） | PA0（正通道） |
| 负输入 INM | DAC1_OUT1 | DAC3_OUT1 |
| 迟滞 | 70 mV | 70 mV |
| 触发模式 | 上升沿事件 | 上升沿事件 |

### 3.3 DAC1 / DAC3

| 参数 | DAC1 | DAC3 |
| --- | --- | --- |
| 输出 | OUT1（内部） | OUT1（内部） |
| 采样保持 | 禁用 | 默认 |
| 校准 | 出厂值 | 默认 |
| 用途 | COMP1 阈值（负通道） | COMP3 阈值（正通道） |

### 3.4 HRTIM1

**定时器 A（TA1 / TA2 输出）**

| 参数 | 值 |
| --- | --- |
| 输出 | TA1（PA8）、TA2（PA9） |
| 模式 | 连续（CONTINUOUS） |
| 预分频 | ×2（MUL2） |
| 周期 | 0xFFFD（65533） |
| 比较单元 | CMP1 / CMP2 / CMP3，比较值 65533 |
| 置位源 | TA1 ← CMP1，TA2 ← CMP3 |
| 复位源 | TA1 ← CMP2，TA2 ← PER |
| 空闲电平 | IDLE |
| 重复计数 | 0（更新禁用） |
| 预装载 | 禁用 |

**定时器 B（TB，无输出）**

| 参数 | 值 |
| --- | --- |
| 模式 | 连续（CONTINUOUS） |
| 预分频 | ×2（MUL2） |
| 预装载 | 使能 |
| 更新触发 | Timer B 更新 |
| 复位触发 | UPDATE |

**故障与保护**

| 故障 | 源 | 滤波 | 复位模式 | 计数器值 | 输出电平 |
| --- | --- | --- | --- | --- | --- |
| FLT4 | HRTIM_FAULT_4 | 无 | 条件复位 | 2 | INACTIVE |
| FLT5 | HRTIM_FAULT_5 | 无 | 条件复位 | 2 | INACTIVE |

**ADC 触发**

| 触发 | 源 | 更新源 |
| --- | --- | --- |
| ADC TRG1 | Timer B CMP3 | Timer B |
| ADC TRG3 | Timer B CMP4 | Timer B |

**突发模式（Burst）**

| 参数 | 值 |
| --- | --- |
| 使能 | 使能 |
| 模式 | 单次（SINGLESHOT） |
| 时钟源 | FHRTIM |
| 预分频 | ÷512 |
| 触发源 | Timer A 重复 |

**中断**

| 中断源 | NVIC 中断 | 优先级 |
| --- | --- | --- |
| SYSFLT / FLT4 / FLT5 | HRTIM1_FLT_IRQn | 4 |
| TIMA 重复（REP） | HRTIM1_TIMA_IRQn | 4 |

### 3.5 TIM6

| 参数 | 值 |
| --- | --- |
| 时钟源 | 内部（APB1，170 MHz） |
| 预分频 | 1699（÷1700 → 100 kHz） |
| 中断 | TIM6_DAC_IRQn（优先级 5） |

> 注：自动重载值 ARR 由代码在运行时设定，`.ioc` 未定义。

### 3.6 IWDG

| 参数 | 值 |
| --- | --- |
| 预分频 | /8 |
| 重载 | 3999 |
| 超时 | 约 1 s（基于 LSI 32 kHz） |

### 3.7 USART1 / USART2

**通道配置**

| 参数 | USART1 | USART2 |
| --- | --- | --- |
| 模式 | 异步 | 异步 |
| 引脚 | PC4（TX）/ PC5（RX） | PA2（TX）/ PA3（RX） |
| 用途 | 单板调试版协议口 | 发布版协议口 |

宏定义二选一。

**参数配置（两路一致，来自代码初始化）**

| 参数 | 值 |
| --- | --- |
| 波特率 | 115200 Bits/s |
| 字长 | 8 位（含校验位） |
| 校验 | 无 |
| 停止位 | 1 |
| 数据方向 | 收发 |
| 过采样 | 16 |
| 单采样 | 禁用 |
| 时钟预分频 | 1 |
| FIFO 模式 | 使能 |
| TX / RX FIFO 阈值 | 1/8 |
| 自动波特率 | 禁用 |
| TX / RX 引脚反相 | 禁用 |
| 数据反相 | 禁用 |
| TX / RX 引脚交换 | 禁用 |
| 溢出 | 使能 |
| DMA on RX Error | 启用 |
| MSB First | 禁用 |

### 3.8 DMA 映射

| 请求 | 通道 | 方向 | 模式 | 数据宽度 |
| --- | --- | --- | --- | --- |
| USART1_RX | DMA1_Channel1 | 外设→内存 | 普通 | 字节 |
| USART1_TX | DMA1_Channel2 | 内存→外设 | 普通 | 字节 |
| USART2_RX | DMA1_Channel3 | 外设→内存 | 普通 | 字节 |
| USART2_TX | DMA1_Channel4 | 内存→外设 | 普通 | 字节 |
| ADC1 | DMA1_Channel5 | 外设→内存 | 循环 | 半字（16 bit） |
| ADC2 | DMA1_Channel6 | 外设→内存 | 循环 | 半字（16 bit） |

> 所有 DMA 请求优先级均为 Low。

### 3.9 NVIC 中断优先级

优先级分组 = 4（全部为抢占优先级）。

| 中断 | 抢占优先级 |
| --- | --- |
| SysTick | 15 |
| DMA1_Channel1–6 | 5 |
| TIM6_DAC | 5 |
| USART1 | 5 |
| USART2 | 5 |
| EXTI9_5（PB6 / PG7 / PG8） | 4 |
| HRTIM1_FLT | 4 |
| HRTIM1_TIMA | 4 |

### 3.10 SYS

| 参数 | 值 |
| --- | --- |
| 调试口 | SWD（PA13 / PA14） |
| SysTick | 使能（优先级 15） |
| Dead Battery | 禁用 |
