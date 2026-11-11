# 配置在主机侧生成快照，Keil 构建零外部工具依赖

设备树与 Kconfig 只在主机侧生成一次头文件快照并提交进仓库，Keil 构建只 include 快照，不调用 python、dtc 或 cmake。选择它而不是手写配置，是因为上游驱动直接消费设备树宏（`DEVICE_DT_INST_DEFINE`、`DT_INST_*`），手写等于重造设备模型；而把生成过程留在构建期会把主机工具链变成日常依赖。
