# 上游驱动复用边界：比较器与 Flash 用上游，ADC、DAC 与 HRTIM 自写

判据不是"能不能编进来"，而是"接口是否覆盖本板用法"。

复用：`comparator_stm32_comp.c`（LL 实现，绑定已覆盖正负输入、迟滞与 `st,exti-line`，依赖的
`intc_exti_stm32.c` 已在编）、`flash_stm32g4x.c` + `flash_stm32.c`（LL、双 bank 感知、页布局、锁用
`k_sem`，垫片已有；`z_stm32_hsem_*` 在非双核 G4 上是空实现）。

自写：`current_capture`（ADC1/2 由 HRTIM 的 TIMB 比较触发，上游把注入触发源写死为软件触发；
`adc_context` 的阻塞等待不能进出波路径）、`threshold_dac`（DAC1/DAC3 要作比较器内部反相输入，
上游只支持输出到引脚）、`pulse_timer`（HRTIM 在 4.5 里没有 binding、没有 SoC 节点、没有驱动）。

放弃的方案：

- **给上游 ADC 驱动打补丁**：违反"上游零修改、文件级选择"（ADR-0003）。
- **绕过上游驱动直接写 ADC 寄存器**：同一个外设出现两个 owner，比自写更糟。
- **用 NVS 或 FCB 当记录层**：两者都要 `flash_map` 分区表与 `k_mutex`，且落盘布局与 E-011 冻结的
  "A/B 双区 + 版本 + CRC + 活动标志切换"不是同一套，改用它们就得在需求文档里解释偏差。

## Consequences

- 引入上游驱动要改 `tools/sync_upstream.py`、`third_party/MANIFEST.md` 与 `gen_project.py`，并重生成配置快照。
- 出波期间禁止调用会阻塞或依赖信号量的上游 API。
- 比较器的 COMP→FLT 交叉开关由 `protection` 配置，上游驱动只负责比较器本身的配置与事件。
