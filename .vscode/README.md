# G071 编译、烧录与调试

目标：STM32G071CBU3；CMSIS-DAP；SWD 1000 kHz；Debug ELF：`build/Debug/g071_test.elf`。

## VS Code 入口

- `Ctrl+Shift+B`：配置并编译 Debug。
- `Ctrl+Shift+P` → `Tasks: Run Task` → `DAP: 编译并烧录 G071`：编译、下载、校验、复位运行。
- `F5` → `DAP: G071 编译下载并调试`：先编译，再下载，停在 `main()`；`F5` 继续、`F10` 单步、`Shift+F5` 结束。
- `DAP: G071 附加调试（不下载）`：连接并暂停当前程序；本地 ELF 必须与芯片中的固件一致。
- 任务 `G071: 清理 Debug`：清理构建产物。
- 任务 `DAP: 检查 G071 连接（不烧录）`：连接指定探针，探测 Flash 并读取芯片 ID、容量，不下载或复位。
- 任务 `DAP: 检查配置（不连接芯片）`：只解析 OpenOCD 配置，不初始化探针。

## 接线

| CMSIS-DAP | G071 板 |
|---|---|
| SWDIO | PA13 |
| SWCLK | PA14 |
| GND | GND |
| VTref（若探针提供该输入） | 板上 3.3 V |

目标板需要供电。当前通过 SYSRESETREQ 软件复位，不要求连接 NRST。
当前固定使用 HID 后端及序列号 `b79c4c58` 的 Horco CMSIS-DAP，已在本板验证连接；更换探针后需要更新 `openocd-dap.cfg` 中的 `adapter serial`。

## 本机工具

- CMake：`C:/ST/STM32CubeCLT_1.20.0/CMake/bin/cmake.exe`
- Ninja：`C:/ST/STM32CubeCLT_1.20.0/Ninja/bin/ninja.exe`
- GCC/GDB：`C:/ST/STM32CubeCLT_1.20.0/GNU-tools-for-STM32/bin`
- OpenOCD：`D:/浏览器下载/OpenOCD-20260121-0.12.0/bin/openocd.exe`

这些路径按本机已安装工具配置；迁移电脑或升级工具后需要同步更新。
OpenOCD 的 G0 默认 reset-init 会切到 64 MHz，本工程覆盖该钩子，保持复位后的 HSI16 和 1 MHz SWD。应用运行时的时钟由 `SystemClock_Config()` 决定。

本次配置不自动执行烧录。构建和离线解析通过不等于 SWD 已连接或板上已运行；接好探针后再使用烧录或调试入口验收。

## 配置验证记录（2026-10-22）

- 按 `tasks.json` 中的命令完成配置、清理及 Debug 全量构建：25/25 步骤通过，无编译或链接警告。
- 链接占用：Flash 11148 B / 128 KiB；RAM 1728 B / 36 KiB（含堆、栈预留）。
- OpenOCD 离线解析通过，输出 `G071 CMSIS-DAP config OK`。
- JSON、任务依赖、工具路径及已安装 Cortex-Debug 的配置字段检查通过。
- GDB 可读取 ELF，并解析 `main` 符号与 ARMv6-M 架构。
- 首次离线配置时未连接探针；插入探针后的验证见下节。

## 插入探针后的连接复查（2026-10-22）

- 系统同时识别到两只 Horco CMSIS-DAP：`b5d64c58` 和 `b79c4c58`。
- 自动选择 `b5d64c58` 时，CMSIS-DAP v2 初始化报 `CMD_INFO failed`；单独选择该探针的 HID 后端也出现同一错误，尚未确定其原因。
- 使用 HID 后端并固定 `b79c4c58` 后成功连接，SWD 1000 kHz，DPIDR 为 `0x0BC11477`。
- OpenOCD 识别 Cortex-M0+ r0p1，支持 4 个硬件断点、2 个观察点。
- 读取 `DBGMCU_IDCODE`（`0x40015800`）得到 `0x20016460`，低 12 位设备 ID 为 `0x460`。
- 读取 Flash 容量寄存器（`0x1FFF75E0`）得到 `0x0080`，对应 128 KiB。
- Flash 驱动探测通过，识别为 STM32G07/G08xx，128 KiB 单 Bank，RDP Level 0；未执行解锁操作。
- 本次只验证探针初始化、SWD 通信及寄存器读取，没有执行下载、擦除、复位或程序断点测试。芯片封装和完整订货型号仍以 `.ioc` 配置及实物标记为准。

## USART3 当前日志实现

当前已改为 TX DMA + 环形队列、RX 循环 DMA + IDLE/HT/TC + 接收环形缓冲。详见 [USART3 DMA 日志说明](../Docs/USART3_DMA_LOG.md)。
调试时观察 `uart_log_count` 和 `board_usart3_stats`；旧版 `uart_log_tx_busy`、`uart_log_tx_complete`、`uart_log_failures` 已由模块统计替代。编译烧录调试入口不变。

## 首版 USART3 中断打印测试（历史记录）

测试逻辑放在 `Core/Src/main.c` 的 CubeMX 用户区，不改变时钟、引脚和中断配置。
USART3 发送由该测试独占；主循环格式化日志并调用 `HAL_UART_Transmit_IT()`，完成回调只置标志。
192 B 静态发送缓冲区在完成回调或发送终止前保持不变。失败后间隔 10 ms 重试，250 ms 未完成则在前台终止发送并重试同一条消息。

串口连接：PB8/TX → USB 转串口 RXD，GND 共地；115200、8N1、无流控。USB 串口需要实际接到 PB8 才能收到日志。
正常输出示例（tick 为实际毫秒计数）：

```text
[BOOT] STM32G071 started
[BOOT] SYSCLK=16000000 Hz
[BOOT] USART3: 115200 8N1, PB8=TX PB9=RX, TX=IT
[RUN] count=1 tick=1000 tx_fail=0
[RUN] count=2 tick=2000 tx_fail=0
```

验收：启动信息出现一次，随后每秒递增计数，连续观察至少一分钟，无乱码、重启或停顿，正常时 `tx_fail=0`。计时比较使用无符号差值以处理 HAL tick 回绕。

2026-10-22 源码及构建验证：Debug 编译链接通过，Flash 15804 B，RAM 2360 B（含堆、栈预留）；本次应用代码通过 `-Wall -Wextra -Werror -Wformat=2 -Wconversion -fanalyzer` 编译检查，厂商头文件按系统头处理。已确认自定义发送完成回调进入 ELF。本轮未烧录，尚无这版日志的板级输出证据。
