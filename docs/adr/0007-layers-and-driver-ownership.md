# 四层：app 是宿主、services 是功能域、drivers 是上游缺口、port 是垫片

在 ADR-0005（`app` / `services` 两层）之上新增 `drivers/` 层，并把"与硬件打交道"的判据写死：
**上游有可用的驱动就用上游，没有或不覆盖本板用法才自己写**。

`app` 分别依赖 `services/` 与 `drivers/`，并在应用装配处适配接口。服务与驱动不互相包含头文件。脉冲引擎通过
`pulse_hw.h` 定义所需的 `ops`，用物理单位提交时序，寄存器换算由驱动负责。主机测试可以注入假驱动。

放弃的方案：

- **把自写硬件代码放进 `port/`**：`port/` 的判据是"补上游缺的 Zephyr API"，把 HRTIM 放进去会让两套完全
  不同的判据管同一个目录，`port/` 迟早变成杂物间。
- **放进 `boards/psg/g474qet6/`**：板级目录一旦承载业务接口实现，"换一块板子是否成立"这条判据就靠自觉了。
- **全部复用上游（含 ADC 与 DAC）**：上游 `adc_stm32.c` 把注入触发源写死为软件触发，`adc_context` 用
  `k_sem` 阻塞等完成；`dac_stm32.c` 只支持输出到引脚，不提供到比较器的内部路由。两者都不覆盖本板用法。
- **全部自写（含比较器与 Flash）**：上游比较器驱动与 Flash 驱动都是纯 LL 实现、依赖已在仓库内或垫片内
  （`intc_exti_stm32`、`k_sem`），自写只是重造一份没有测试覆盖的实现。

## Consequences

- `tools/gen_project.py` 增加 `drivers` 工程组；`tools/sync_upstream.py` 与 `third_party/MANIFEST.md`
  增加比较器与 Flash 两个驱动文件；需要重生成一次配置快照（新增 `CONFIG_COMPARATOR`、`CONFIG_FLASH`）。
- 出波期间禁止调用会阻塞的上游 API，这条成为评审项。
- 驱动能力按"能力"命名（`pulse_timer`、`current_capture`），不按外设命名（不叫 `hrtim.h`），
  以后换实现不动服务。
