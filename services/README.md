# services

与具体板卡无关的功能域。本目录暂时没有代码，这份文件定的是"什么该放进来"。

## 判据

一个子目录一个服务，服务是可以被 `app/` 装配、推进和查询的东西，例如脉冲引擎、
ECSP 协议栈、电流采集与结算、参数与标定。判断标准是：**换一块板子，它是否还成立**。
不成立的东西属于板级（`boards/`）或垫片层（`port/`）。

## 依赖方向

```
app  →  services  →  port  →  third_party
```

- `services/` 不得 include `app/` 的任何头文件，也不得 include `boards/` 下的具体板级头文件。
- `services/` 与硬件打交道只经过上游 Zephyr API（`<zephyr/drivers/...>`）和 `port/` 提供的垫片；
  不直接读写寄存器。
- 一个服务对外只暴露一个头文件 `<服务名>.h`，其余符号 `static`；头文件按仓库根引用，
  写 `#include "services/<服务名>.h"`。

## 命名

目录名用小写、下划线分隔的领域名（`pulse_engine`、`ecsp`、`current_sample`），
不用 `util`、`common`、`misc` 这类装不满又装得下的名字。

## 清单

（空）
