# 移植说明

上游：Zephyr 4.5.0（`v4.5.0`，`41d10b59730`），以及该源码树随附的 CMSIS 6 与 STM32CubeF4 头文件。`third_party/` 下的内容一字未改。

## 用到的上游内容

| 上游路径 | 作用 |
| --- | --- |
| `include/` | Zephyr 头文件（`device.h`、`init.h`、`devicetree.h`、各驱动 API 等） |
| `kernel/device.c` | 设备模型：`do_device_init()`、`device_is_ready()`、设备链表 |
| `drivers/timer/cortex_m_systick.c`、`drivers/timer/system_timer_generic.h` | 系统时基：SysTick 驱动、周期计数与 tick 记账（tickless 通用核心） |
| `drivers/gpio/gpio_stm32.c` | GPIO API 实现 |
| `soc/st/stm32/common/gpioport_mgr.c` | 每个端口的设备实例（4.5 把它们从 GPIO 驱动里移了出来） |
| `drivers/pinctrl/pinctrl_stm32.c` | 按设备树做引脚复用 |
| `drivers/clock_control/clock_stm32f2_f4_f7.c`、`clock_stm32_ll_common.c` | PLL 与外设时钟控制 |
| `drivers/interrupt_controller/intc_gpio_stm32.c`、`intc_exti_stm32.c` | 驱动无条件引用的 GPIO 中断底层代码 |
| `drivers/pinctrl/common.c` | `pinctrl_lookup_state()`，UART 驱动会引用 |
| `drivers/serial/uart_stm32.c` | UART API 实现，含 TX/RX DMA |
| `drivers/dma/dma_stm32.c`、`dma_stm32_v1.c` | STM32 DMA 控制器（V1，F4 用的就是这一版） |
| `drivers/console/uart_console.c` | `log_backend_uart` 所依赖的控制台后端 |
| `drivers/reset/reset_stm32.c` | RCC 复位控制器；UART 节点带有 `resets` 属性 |
| `subsys/logging/log_core.c`、`log_msg.c`、`log_output.c`、`log_cache.c`、`log_mgmt.c` | 日志子系统，延迟模式（`mpsc_pbuf`，由 `log_process()` 排空） |
| `subsys/logging/backends/log_backend_uart.c` | UART 日志后端（轮询式，不是异步变体） |
| `lib/os/cbprintf.c`、`cbprintf_complete.c`、`cbprintf_packaged.c` | `LOG_*` 背后的格式化实现 |
| `lib/utils/mpsc_pbuf.c` | 延迟日志背后的多生产者缓冲区 |
| `lib/os/printk.c` | `snprintk()` 以及控制台使用的 printk 字符输出钩子 |
| `lib/libc/armstdc/` | Zephyr 的 Arm Compiler C 库胶水（`errno.c`） |
| `soc/st/stm32/common/stm32_bitops.h`、`stm32_hsem.h`、`stm32_gpio_shared.h`、`pinctrl_soc.h` | SoC 辅助头文件 |
| `modules/hal/cmsis_6`、`modules/hal/stm32/stm32cube/stm32f4xx`、`common_ll` | CMSIS 内核头文件与 STM32F4 设备/LL 头文件，以及 `drivers/src/stm32f4xx_ll_utils.c`（`LL_SetFlashLatency()`） |
| `arch/common/include` | `gen_offset.h` 等 |

除此之外（内核、调度器、日志、UART 等）均有意不纳入。

## 垫片层（`port/`）

`port/` 是本工程唯一自己拥有代码的地方，其中包含：

- `init_runner.c` —— 遍历初始化表，取代 `z_cstart()`。`scatter.sct` 给每个 Zephyr 初始化级别分配独立的执行域，两侧用标记括起，因此各级别按顺序执行，级别内的优先级仍保持上游（按名称排序）的次序。
- `irq_loader.c` —— 中断装载器：启动时把 `.intList` 里的 `<IRQ_CONNECT>` 登记项复制进一张 RAM 表，并按异常号分发。启动文件的默认处理函数会跳转到这里（见 ADR-0004）。
- `device_list_bounds.c` —— 导出 `_device_list_start`/`_device_list_end`，`kernel/device.c` 期望这两个符号由链接器提供。
- `arch_irq_shim.c` —— 在 CMSIS NVIC 之上实现 `arm_irq_enable/disable/priority_set`。
- `errno_shim.c` —— 为 Arm Compiler C 库提供 `z_impl_z_errno()` 与 `__aeabi_errno_addr()`（上游的钩子需要内核线程指针）。
- `hal_shim.c` —— 只剩 `HAL_RCC_GetSysClockFreq()`（复位态 HSI = 16 MHz）；`LL_SetFlashLatency()` 已改用上游 `stm32f4xx_ll_utils.c`。完整的 HAL RCC 驱动仍然进不来，原因见[已知偏差](#已知偏差与陷阱)里的 armlink 条目。
- `sys_clock_glue.c` —— 上游系统时基驱动所缺的内核那一半：`sys_clock_announce_locked()` 只推进 tick 计数、再按固定的 1 ms 重装 SysTick（没有超时队列，也不在中断里跑工作处理函数）；`k_uptime_ticks` 由整 tick 加亚 tick 周期组成。启动文件的 SysTick 向量直接指向驱动里的 `sys_clock_isr`。
- `log_section_bounds.c` —— 提供日志子系统遍历的可迭代链表所需的 `_log_*_list_start`/`_end`；`scatter.sct` 用标记把每个链表括起来。
- `rodata_bounds.s` —— `__rodata_region_start`/`_end`，即 cbprintf 在就地引用格式串之前要检查的 Flash 窗口。
- `kernel_shim.c` —— 时间来自上游系统时基（`sys_clock_cycle_get_64()`），`k_sleep_ticks` 是有界自旋；计数信号量，其阻塞式 take 是有界的 `WFE` 自旋；以及 UART 用到的 `k_work_delayable` 调用，它们被并进 `nihao_work_poll()`（见[延迟工作](#延迟工作没有工作队列时的-k_work_delayable)）。
- `libc_shim.c` —— 控制台用的 `__stdout_hook_install()`/`fputc()`。上游版本位于 `armstdc/src/libc-hooks.c`，但它同时用当前线程来定义 `__aeabi_errno_addr()`，而这里没有线程。

对上游源码施加的补丁数：**0**。

## 延迟工作：没有工作队列时的 `k_work_delayable`

`uart_stm32` 的异步路径持有两个 `k_work_delayable`：`dma_rx.timeout_work` 与 `dma_tx.timeout_work`，并期待系统工作队列来执行它们。RX 那个负责把 `uart_rx_enable()` 的 `timeout` 变成 `UART_RX_RDY` 事件：USART IDLE 中断按 `timeout` 装载该工作项，工作项执行时把 DMA 收到的数据冲刷出去。这里没有工作队列，因此 `port/src/kernel_shim.c` 维护一张小型的延迟工作项表和一个轮询执行器 `nihao_work_poll()`，主循环每轮都会调用它（`main()` 与 `idle_until()`）。

### 数据模型

```c
struct nihao_work {
	struct k_work_delayable *dwork; /* identity: Zephyr hands out no cookie */
	k_work_handler_t handler;
	uint64_t deadline;              /* absolute, in system timer cycles */
	bool pending;
};

static struct nihao_work works[NIHAO_WORK_SLOTS]; /* 4 */
```

某个 `dwork` 的槽位由它第一次的 `k_work_init_delayable()`/`k_work_reschedule()` 占用，此后按指针身份匹配；查找是一次线性扫描。两个槽位供 UART 使用，另外两个留给下一个需要它的驱动当余量。

### API 范围

| 调用 | 这里的实现 | 上游契约 |
| --- | --- | --- |
| `k_work_init_delayable(dwork, handler)` | 占用一个槽位，保存处理函数，清 `pending`。返回 `void`，所以占用失败是**静默**的 | 创建一个空闲工作项 |
| `k_work_reschedule(dwork, delay)` | `K_FOREVER` → 取消并返回 0；`K_NO_WAIT` → 立即到期；否则 `deadline = cycles() + delay.ticks * CYCLES_PER_TICK`。返回 1；没有空闲槽位时返回 `-ENOMEM` | 挂到系统工作队列；原本待处理返回 1，否则返回 0，出错返回负值 |
| `k_work_cancel_delayable(dwork)` | 清 `pending`，原本待处理则返回 1 | 取消；原本待处理返回 1 |
| 其余 | 不存在：`k_work_submit`、`k_work_schedule`、`k_work_flush`、`k_work_delayable_busy_get`、`k_work_delayable_remaining_get`、`K_WORK_DELAYABLE_DEFINE` | |

### 时间基准

`k_timeout_t.ticks` 里存的已经是 tick 数，而不是调用者传入的单位：`K_USEC()`/`K_MSEC()`/`K_TICKS()` 经 `Z_TIMEOUT_US()` → `k_us_to_ticks_ceil32()` 在编译期向上取整。（这也解释了为什么 `K_FOREVER` 就是 `{.ticks = -1}`、`K_NO_WAIT` 就是 `{.ticks = 0}`，于是 `K_TIMEOUT_EQ()` 只是两个整数的比较。）因此垫片只需把 tick 换算成周期：

```
CYCLES_PER_TICK = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / CONFIG_SYS_CLOCK_TICKS_PER_SEC
                = 180000000 / 10000 = 18000 cycles = 100 us
```

这里所有 deadline 的粒度都是 100 µs。驱动在一段短 DMA 突发之后重新装载的是 `K_TICKS(1)` = 100 µs；`main.c` 请求的是 `K_USEC(1000)` = 10 个 tick。

`sys_clock_cycle_get_64()` 由驱动与 timer core 合成：SysTick 只有 24 位递减计数，驱动用 `last_load` 与 `SysTick->VAL` 在软件里拼出 32 位周期计数，再由 timer core 按 announce 基线扩展成 64 位。因为计数是合成的（`TIMER_CORE_COUNTER_NONATOMIC`），getter 内部会取一次时钟锁——UP 下就是 `arch_irq_lock()`，保存/恢复 PRIMASK，所以主循环、中断与 `irq_lock()` 内部都能调用，自旋里的开销只是几条指令。

### 执行

`nihao_work_poll()` 在主上下文中就地执行每个到期的处理函数，一直跑到结束，并在调用**之前**清 `pending`，这样处理函数可以重新排定自己。装载都是一次性的；没有周期模式，而 `uart_stm32` 的用法恰好就是这样。

从到期到执行之间的时延，是两次轮询之间的最长间隔：一次 `log_process()` 排空再加上一个回显块的 TX 等待，板子空闲时也就是几十微秒。因此 RX 空闲冲刷落在“1 ms 加上环路时延”而不是精确的 1 ms，短包之后的 `UART_RX_RDY` 也有同样的延迟。

### 并发

`k_work_reschedule()` 会从中断上下文到达：USART1 IDLE 中断（`uart_stm32_isr`）把 RX 空闲定时器装载成 `K_USEC(timeout)`，DMA RX 回调则在缓冲区填满且没有下一个缓冲区排队时把它重新装载成 `K_TICKS(1)`。`nihao_work_poll()` 运行在主上下文。这里没有锁；让它成立的是发布顺序：

- 写者先写 `deadline`，再置 `pending = true`；
- 读者先判断 `pending`，再读 `deadline`。

在 Cortex-M4 上每次写入都是单条 32 位 STR，所以一次轮询若看到 `pending == true`，它读到的 `deadline` 必然是在该标志之前写下的。残余的竞争是**陈旧**的 deadline：中断服务程序若在读者判断 `pending` 之后、读取 `deadline` 之前重新装载，处理函数就可能按更早的 deadline 执行。对空闲冲刷来说这无害（提前冲刷仍然是冲刷）。这不是通用工作队列；未经重新审视本节，不应把它扩成通用工作队列。

### 失败模式

- **槽位耗尽。** 第五个不同的 `k_work_delayable` 会从 `k_work_reschedule()` 拿到 `-ENOMEM`，而因为 `k_work_init_delayable()` 返回 `void`，失败不会被任何地方报告。症状：字节已经到达 UART，日志 alive 行里的 `irq` 在上涨，但应用始终拿不到 `UART_RX_RDY`，因为空闲冲刷从未触发。调大 `NIHAO_WORK_SLOTS`。
- **64 位 `deadline` 比较**是普通的无符号运算；回绕需要约 10^11 秒的开机时间。

## 配置快照

`config_gen/4.5.0/` 保存的是针对 `boards/alientek/f429igt6` 跑一次 Zephyr configure 所产生的头文件。Keil 通过 `-imacros .../autoconf.h` 与 `-I config_gen/4.5.0` 消费它们。

用 `python tools/gen_config.py` 重新生成。脚本会：

1. 把 `tools/patches/0001-armclang-dts-preprocessor.patch` 应用到 Zephyr 树，然后还原；
2. 以 `ZEPHYR_TOOLCHAIN_VARIANT=armclang` 配置，使生成的 `autoconf.h` 与 Arm Compiler 的 C 库匹配（GCC/picolibc 的配置会在 `errno` 上出问题）；
3. 构建 `version_h`、`syscall_list_h_target`、`kobj_types_h_target` 与 `heap_constants_h`，并把输出复制进快照。

本地补丁之所以存在，是因为 Arm Compiler 6.23 写出的 `-E` 输出所用的容器，这台工作站上的 python-devicetree 读不了；它只在设备树预处理这一步换成 Zephyr SDK 的 GCC。这不影响 Keil 构建。

## 已知偏差与陷阱

- **初始化顺序**先按级别执行域、级别内再按段名排序——与 Zephyr 自己的链接器所用的排序规则相同。
- **必须用 `-O1` 或更高**：在 `-O0` 下 armclang 会为未使用的 `static inline` 内核辅助函数生成代码，从而拖入未解析的内核符号。
- **栈为 8 KiB**（`startup_stm32f429xx.s` 中的 `Stack_Size`）。`scatter.sct` 给栈和堆各自分配固定的执行域（`ER_STACK`、`ER_HEAP`），这样静态数据长到它们身上时会在链接期失败，而不是在运行时压坏栈。`armasm` 无法引用链接器的 `Image$$...` 符号，所以尺寸仍写在启动文件里，scatter 执行域只负责强制这些尺寸。
- **armlink 会忽略固定基地址执行域内的段选择器**：在 `ER_IROM1` 与 `RW_IRAM1` 中只有 `.ANY` 生效。因此每个可迭代链表都有自己的 `+0` 执行域，而括起链表的标记段用 `+First`/`+Last`，因为零长度段否则会被放到所在执行域的末尾。`.intList` 被编译器标为 RW，所以那里 `+Last` 不起作用：它的结束标记放在紧随其后的独立执行域（`ER_INTLIST_END`）里。
- **端点加密产品会弄乱 `python.exe` 写出的文件**：它们以加密形式存储，只有白名单内的读取者（`armclang`、`python`）能看到明文。`armasm` 不在白名单里，所以任何 `.s` 文件都必须由明文写入者（PowerShell）写出，否则汇编器读到的是 `%TSD-Header-###%` 并报 `A1271E: Line too long`。
- **零长度标记数组必须从另一个编译单元索引。** 在定义它的单元里索引 `const char x[0]` 属于未定义行为，所以 `-O1` 会把整个循环删掉（修复前 `nihao_irq_table_load` 被编译成一条存储指令）。在一处定义标记，在遍历它们的地方以 `extern const struct ... []` 声明。
- **`uart_rx_enable()` 的 `timeout` 是空闲冲刷超时，不是使能截止时间。** `SYS_FOREVER_US` 会让驱动把冲刷工作排到约 35 分钟之后（`K_USEC(INT32_MAX)`），于是短输入在 64 字节缓冲区填满之前永远到不了调用者。请用小值（这里是 1 ms）。
- **延迟日志需要 `log_core_init()`。** `z_cstart()` 在任何 SYS_INIT 级别之前运行 `LOG_CORE_INIT()`，`z_log_msg_init()` 正是在那里做 `mpsc_pbuf_init()` 并设置当前消息缓冲区。只跑初始化级别会让 `curr_log_buffer` 为 NULL：此时 `LOG_INF()` 调用成功、花掉一个时间戳、然后丢弃消息，于是什么也到不了后端。`init_runner.c` 在同一位置调用了它。
- **一个 UART 设备只有一个异步回调。** `LOG_BACKEND_UART_ASYNC` 的 `char_out` 会安装自己的回调，并等待只有该回调才会给出的信号量，所以应用若同时调用 `uart_callback_set()`（用于 RX），就会占掉后端的槽位，后端则永远等下去。日志后端与应用只能有一方拥有回调——本移植把它留给应用，让日志后端改为轮询（`LOG_BACKEND_UART`）。
- **`clk_hse` 必须在板级设备树中启用。** SoC 的 dtsi 提供它时 `status = "disabled"`。若 PLL 选择 HSE 而节点仍处于禁用状态，`set_up_fixed_clock_sources()` 就不会启动晶振，`stm32_clock_control_init()` 会在 `PLLRDY` 上死等。症状：镜像在 `main` 之前挂住，`RCC->CR = 0x01007D83`（PLLON 置位、PLLRDY 清零、HSEON 清零）。
- **`HAL_RCC_GetSysClockFreq()` 返回的是复位态 HSI 频率。** 它只被用一次，用于在 PLL 编程之前计算 Flash 等待周期，这是 `hal_shim.c` 现在唯一的内容。
- **armlink 对被删段里的未定义符号仍然报错。** 即使 `--remove` 已经把 `HAL_RCC_OscConfig`、`HAL_RCC_ClockConfig`、`HAL_RCC_MCOConfig` 整段删掉，编进 `stm32f4xx_hal_rcc.c` 依然要求 `HAL_GetTick()`、`HAL_InitTick()`、`uwTickPrio`、`HAL_GPIO_Init()` 有定义——所以"只取一个函数"不是文件级选择能解决的：要么保留 `HAL_RCC_GetSysClockFreq()` 的垫片，要么把 HAL core 和它的 SysTick 时基一起链进来（那正是第二条时钟路径）。`stm32f4xx_ll_utils.c` 没有这个问题：它自足，除 `LL_SetFlashLatency()` 外的函数被正常 GC（`LL_mDelay`、`LL_Init1msTick`、`LL_PLL_ConfigSystemClock_*` 都在删除列表里）。上游版 `LL_SetFlashLatency` 带电源档位分支与有界轮询，比原来手写的 114 B 表多 212 B。
- **时基是固定的 1 ms 周期，不是上游的 tickless。** 4.5 的驱动与 timer core 按 tickless 设计，靠内核在超时变化时调用 `sys_clock_set_timeout()`；这里没有超时队列，所以 `sys_clock_announce_locked()` 在每次 announce 之后都重装 `NIHAO_TICKS_PER_MS`（10 个 tick = 1 ms）。`k_uptime_ticks()` 仍是“整 tick + `sys_clock_elapsed()`”，亚 tick 分辨率没有损失，只是中断固定 1 kHz；要按 deadline 唤醒，就得把工作表的最近截止时间喂给 `sys_clock_set_timeout()`。
- **SysTick 的优先级是 1，会抢占 USART/DMA 中断。** `CONFIG_CORTEX_M_SYSTICK_INTERRUPT_PRIORITY=0`，驱动再加 `_IRQ_PRIO_OFFSET`（本板为 1），0 留给处理器 fault。ISR 约 300 B，只推进计数并重装 LOAD，不调用任何驱动回调。
- **`z_arm_int_exit()` 是空实现。** 直接装在向量表里的 ISR 退出时调用它（上游是 `arch/arm/core/cortex_m/exc_exit.c` 的别名，职责只是按需 pend PendSV）。没有调度器，所以 `arch_irq_shim.c` 给了一个空函数；异常返回仍由 `sys_clock_isr` 自己的 epilogue 完成。
- **DWT 不再参与计时。** 原来的 DWT 周期计数已删除，`DWT->CYCCNT` 空出来留给以后的高精度剖析；要用上游的 `z_arm_dwt_*` 助手还需把 `arch/arm/include/cortex_m/dwt.h` 收进 `third_party/`。
- **没有 `SystemInit` 时钟配置**：`system_stm32f4xx.c` 提供了启动钩子，但 PLL/总线配置来自 `clock_stm32` 与设备树，因此 `f429igt6.dts` 是时钟的唯一事实来源。

## 后续工作

1. 加一个能通过 UART 报告的故障处理程序（这是目前唯一仍会丢掉全部诊断信息的地方）。
2. 等有驱动需要时，再补 `k_mutex`/`k_work`（非延迟）垫片。
