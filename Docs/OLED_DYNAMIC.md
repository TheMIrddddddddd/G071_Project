# OLED 每秒动态刷新验证

此文为早期整帧动态验证记录，当前完整框架与默认 SHOWCASE 见 [G071_DISPLAY_FRAMEWORK.md](G071_DISPLAY_FRAMEWORK.md)。

## 当前行为

接续已通过板级显示确认的静态版本，现在每 1000 ms 生成并发送一张完整画面：

```text
STM32G071
RUN: 000123 s
RGB: RED
OLED: OK
```

RUN 从应用启动后按 HAL tick 累积秒数，独立于绘图和刷新耗时，不使用刷新次数代替秒数。屏幕首次初始化显示 OLED: START；后续刷新使用 OLED: OK，代表前一帧已成功传输。

RGB 灯仍每 500 ms 切换；应用仅在灯控写成功后记录对应名称，失败会将状态标为 UNKNOWN。OLED 每秒采样一次该状态，不逐个显示每个 500 ms 颜色阶段。画面是提交时的快照，传输期间也可能发生下一次灯控切换。

## BSP 接口与缓冲区

- `board_oled_start()`：保留原初始化、清 RAM、首帧传输和开屏流程。
- `board_oled_update()`：仅在 READY 接受新帧，从页列定位开始，完整发送 1024 字节；不再次初始化、清 RAM、关屏或开屏。OFF/FAULT 返回 NOT_READY，传输中返回 BUS_BUSY，错误上下文或空指针也会被拒绝。
- 每次 poll 最多一次短事务，数据包最多 32 个像素字节加控制字节。单次更新为 32 次定位加 32 次数据传输。
- 单个静态 framebuffer 由应用绘制，传输期间归 BSP 借用，READY/FAULT 后交还。应用检查 READY 后才清缓冲、重画和提交，避免半帧改写。
- RGB、地址探测和 OLED 都在同一前台顺序访问共享总线。中断保持开启，UART DMA 日志继续工作。

首次按一秒周期提交更新；若前一帧仍在传输，则等待 READY 后绘制最新状态，不排队补发旧画面。当前仍为整帧刷新，尚未引入差分更新。

## 日志和错误

示例格式（非实测数据）：

```text
[OLED] SH1106 128x64 addr=0x3C dynamic=1000ms start=OK
[OLED] frame=2 refresh=OK elapsed_ms=180 bytes=1024
[OLED] refresh=FAIL stage=8 page=3 col=64 elapsed_ms=85 error=DATA_NACK
```

completed_frames 只统计成功帧，初始化 start 会重新计数，update 保留计数。elapsed_ms 是第一条画面定位命令到成功或失败的墙上时间，包含期间的中断、RGB/探测/串口调度；不包含绘图、启动等待和清 RAM。首帧耗时包含首次开屏命令，后续帧不含开屏。时间使用无符号差值处理 tick 回绕，分辨率 1 ms。

事务失败后停止 OLED 传输并记录失败阶段/页/列，其他前台流程继续。屏幕可能保留部分更新，OLED: OK 字样不是对故障状态的实时保证；以串口错误为准。当前不自动重新初始化，修复故障后复位，或由应用显式重新 start。

## 本地验证（2026-10-23）

- OLED RAM 模型：初始化与隐藏列清零、完整动态帧映射、忙时拒绝替换、空指针与上下文检查、连续刷新不清屏/不重复开屏、成功计数与耗时、回绕、失败不增加成功数及故障恢复入口均通过。
- 软件 I²C、RGB 和现有 UART 11 项测试回归通过。
- Optimized/O2：Flash 27,500 B，RAM 6,672 B。
- Debug/O0：Flash 41,076 B，RAM 6,664 B。
- 两种固件均编译链接通过，无编译警告。未烧录，真实刷新耗时、视觉效果和灯控调度仍需板级验证。

烧录选择 `DAP: 编译并烧录 G071 Optimized（O2）`，在 COM9、115200 8N1 查看日志。确认 RUN 持续增加、无残影花屏、RGB 正常切换，以及 tx_fail/tx_drop 保持为零；结合 tick 检查灯控实际间隔。
