# 日志经 SEGGER RTT 输出，固件不带控制台

日志后端换成上游的 `subsys/logging/backends/log_backend_rtt.c`，并彻底移除控制台：`CONFIG_CONSOLE=n`，设备树不再有 `zephyr,console` 与 `zephyr,shell-uart`，应用改为通过 `zephyr,protocol-uart` 这个 chosen 节点拿 USART1。选择它而不是把日志留在串口上，是因为 USART1 是板上唯一接出来的数据口，将来跑协议；日志是开发期设施，不该与业务争一条物理链路，也不该决定业务口的波特率与帧格式。

## 两条硬约束

- **日志不得阻塞固件。** 后端模式固定为 `LOG_BACKEND_RTT_MODE_DROP`（配合 SEGGER 的默认 `NO_BLOCK_SKIP`）：缓冲满就整条丢弃。`LOG_BACKEND_RTT_MODE_BLOCK`、`_OVERWRITE` 以及 SEGGER 的 `BLOCK_IF_FIFO_FULL` 都被排除——后者的 `_WriteBlocking` 在没有调试器时会原地自旋。
- **没有调试器时也必须能跑。** 主机不读，`RdOff` 不动，写入恒失败；后端第一次失败就把 `host_present` 置 false 并停止重试。panic 路径的等待由重试计数封顶（`RETRY_CNT` 4 × `RETRY_DELAY_MS` 5 ≈ 25 ms）。代价是现场没有 J-Link 时没有日志输出，这是接受的：RTT 是调试期能力，不是故障记录设施。

## Considered Options

- **保留 UART 日志后端，换到第二个串口**：板上的第二个口（USART2/PA2/PA3）连节点都没有，且在发布版里它才是业务口；为一个调试设施先接一个口，本末倒置。
- **自己写一个最小 RTT 后端**：`port/` 会多出一个需要长期维护、且没有上游测试覆盖的分叉；上游那份已经带了丢包计数、主机探测与有界 panic 等待。
- **同时上 RTT 控制台与 RTT 日志、各占一条上行缓冲**：`rtt_console.c` 把目标写死成 0 号通道，日志就只能挪到 1 号，而 RTT Viewer 默认只看 0 号；共用 0 号又会被上游注明“中断里的 printk 会撕裂数据”。既然只剩 `LOG_*` 一种输出，控制台没有存在理由。

## Consequences

- `printk()` 失去输出去处（`LOG_PRINTK=n`），诊断信息只有 `LOG_*`；`__ASSERT` 关闭（`CONFIG_ASSERT=n`）的现状不变。
- 每条日志行上限 256 B（`LOG_BACKEND_RTT_MESSAGE_SIZE`），超长行整条丢弃并计入丢包；默认的 128 B 会在计数器变宽后截断每秒健康行。
- 上行缓冲 1024 B 落在 `.bss`（`SEGGER_RTT.c` 的 `.rtt_buff_data` 段由 `.ANY (+RW +ZI)` 收进 `RW_IRAM1`），`scatter.sct` 不需要为它开执行域。
- 故障处理程序不能再“往串口打一行”；它必须走 RTT（`PORTING.md` 的后续工作第 1 条）。
