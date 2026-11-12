# 应用层与服务于目录中分层：app 是宿主，services 是功能域

产品代码从 `src/` 拆成 `app/` 与 `services/` 两层，CMSIS 启动文件归到板级目录。`src/` 已删除。

判据只有一条：**换一块板子，它是否还成立**。不成立的东西属于板级（`boards/`）或垫片层（`port/`）。

- `app/` 是宿主：拥有 `main`、超级循环、把服务装配起来并每轮推进它们。
- `services/` 是与板无关的功能域，只依赖 `port/` 与上游 Zephyr API，**禁止**反向依赖 `app/`。

依赖方向为 `app → services → port → third_party`。工程文件由 `tools/gen_project.py` 按目录生成
Keil 组（`board` / `app` / `services` / `port`），头文件按仓库根引用，写 `#include "app/app.h"`、
`#include "services/<服务名>.h"`，于是 include 行本身就是依赖声明。

`app/` 的接口是 `int app_init(void)` 与 `void app_task(void)`。只有「LED 未就绪」算致命失败并返回
`-ENODEV`，其余初始化失败照旧记日志继续运行；`main()` 拿到非 0 就打印一行并停机。`app_task()`
非阻塞，500 ms 翻转与 1 s 健康行的节拍在 `app` 内部按 `k_uptime_get()` 判定，`main()` 只剩入口。

## Considered Options

- **保留 `src/` 当应用目录**：少一次改动，但 `src` 是「源代码」的同义词，表达不了层级；等第二个目录出现时反正要重命名。
- **启动文件留在应用目录**：应用目录从第一天就混进裸机运行时入口，与 `port/` 的职责重叠。
- **`services/` 放通用工具**（环形缓冲、CRC）：`services/` 会退化成 `lib/`，真正的功能域（脉冲引擎、ECSP 协议栈）反而无处安放。
- **头文件按目录加 include 路径、写裸名**：include 行不再携带层次信息，评审时看不出依赖方向，且不同目录的同名头会撞。

## Consequences

- 新增目录 = 新增一个工程组，不必手改 `uvprojx`；代价是工程文件是生成物，在 uVision 里手动改组会被下次生成覆盖。
- `services/` 目前为空，工程里有一个空的 `services` 组占位；第一个服务落地时才出现文件条目。
- 失败路径只打印 `app init failed: <rc>`，原因由返回码表达；LED 缺失时不再打印 `led0 device not ready`。
- 若要进低功耗，`app_task()` 需要改成返回「下次截止时间」，由 `main()` 决定等待方式。
