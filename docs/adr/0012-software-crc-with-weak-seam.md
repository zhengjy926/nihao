# 帧校验用上游软件 CRC，硬件 CRC 本版不上，留 __weak 接缝

协议帧的 CRC16-MODBUS 用上游 `crc16_ansi()`（`<zephyr/sys/crc.h>`，等价于 `crc16_reflect(0xA001, 0xFFFF, buf, len)`），实现文件 `subsys/crc/crc16_sw.c` 入库。代价是 `O(n·8)` 位运算：典型 10–30 B 帧约 5–10 µs，最坏 512 B 帧约 120 µs，每轮最多 8 帧约 1 ms，仍在 50 ms 预算内。

上游 `crc16_reflect()` 是 `__weak`，将来要用 STM32 CRC 单元时补一个强符号即可，帧层不动。上游 `drivers/crc/crc_stm32.c` 的 `crc_ctx` 虽支持 16 位多项式 + 字节反转 + 位反转（能算 MODBUS），但它把 CRC 从纯逻辑层挪进设备 API（要 dts、Kconfig、配置快照，且 `crc_update` 走 `k_sem`），收益只是常数级加速——本版不上。

放弃的方案：

- **硬件 CRC（`crc_stm32.c`）**：常数级加速，代价是把纯逻辑的帧校验拽进设备 API 与 `k_sem`，要 dts/Kconfig/配置快照，破坏帧层的主机可测试性。
- **自写软件 CRC**：上游已有等价实现，自写只是重复一份没有测试覆盖的代码。

## Consequences

- `ecsp` 帧层只 include 纯算法头 `<zephyr/sys/crc.h>`，主机测试 include 路径带 `third_party/zephyr/include` 即可。
- 若实测 CRC 吃紧，先换表驱动软件 CRC（约 1 µs/512 B），再考虑硬件；接缝已由 `__weak` 预留。
- 新增上游文件 `subsys/crc/crc16_sw.c` 入库（`tools/sync_upstream.py`、`third_party/MANIFEST.md`、`gen_project.py`）。
