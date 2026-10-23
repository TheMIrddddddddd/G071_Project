# G071 SH1106 静态画面接入

本文记录最初静态接入阶段。当前应用已改为每秒动态刷新，接口与验收见 [OLED_DYNAMIC.md](OLED_DYNAMIC.md)；初始化和硬件参数仍沿用本文。

## 硬件与来源

- MCU：STM32G071，PB5=SCL、PB6=SDA，七位地址 `0x3C`。
- 面板：SH1106，128×64；132 列显示 RAM，画面列偏移 2。
- 参考相邻 `oled_test/Core/Src/oled_display.c` 的初始化序列：对比度 FF、A1/C8 方向、开屏前清 RAM、上电等待 100 ms、初始化后等待 200 ms。
- 保留该工程已经使用的 `20/02` 序列，不将其解释成 SH1106 的水平寻址指令；实际数据传输始终显式设置页和列。
- 复用 `oled_test` 的 u8g2 2.37.1 绘图端口及 5×7 字体，仅复制所需源码和许可证。传输使用当前 BSP 软件 I²C，不带入 L476 HAL、DWT 或硬件 I²C DMA。
- OLED 复位由板上 RC 电路提供，没有分配 MCU 复位 GPIO。MCU 复位后先发送 `00 12 AE` 同步命令解析器，沿用参考驱动已有的处理。

## 分层和缓冲区

`BSP/board_oled.c/.h` 只负责控制器协议和分步传输。`Middleware/Ports/u8g2` 与 `Middleware/Third_Party/u8g2` 负责绘图，不访问总线。`Core/Src/main.c` USER CODE 绘制测试界面、启动显示、输出日志。

应用持有静态 1024 字节 framebuffer，按 8 页×128 列存储。调用 `board_oled_start(frame)` 后，直到 READY/FAULT 不可改写或释放该缓冲区。驱动不分配堆内存。每次 poll 最多一次传输，像素包最多为控制字节 40 加 32 个数据字节。栈上包的生命周期覆盖同步软件 I²C 调用。

流程为：等待上电 → 命令同步 → 初始化 → 等待稳定 → 清除全部 8×132 字节 RAM → 写入 8×128 字节测试画面 → AF 开屏 → READY。每段像素发送前重新设置页列。READY 后不重复刷新，也不修改用户优化过的软件 I²C 时序。

主循环先服务 UART 与 500 ms 地址探测/RGB，再执行一步 OLED 传输；没有整屏关中断或长时间 HAL_Delay。软件 I²C 仍是同步逐位传输，每次短包耗时与实际总线速率及从机拉伸有关；不等价于硬件 DMA 后台发送。

## 显示与日志

界面四周有 128×64 边框，内部四行分别为：

```text
STM32G071
OLED TEST
I2C: PB5 / PB6
0123456789
```

正常时会输出以下格式，下面不是实测日志：

```text
[OLED] SH1106 128x64 addr=0x3C static start=OK
[OLED] init=OK, waiting before RAM clear
[OLED] clear=OK frame=OK display_on=OK static ready
```

任何一次事务失败即停止本次显示流程，保存错误阶段、页和列，输出 `[OLED] FAIL ...`，原 RGB/串口流程继续运行。阶段编号：2=同步、3=初始化、5=清屏定位、6=清屏数据、7=首帧定位、8=首帧数据、9=开屏。修正接线后复位重试；接口允许在 FAULT 后重新 start，但当前应用不无限自动重试。

READY 仅证明初始化/数据/开屏事务成功，不能单独证明物理屏幕显示正确。需检查四周完整、方向、字体、列偏移、花屏及多次复位显示，同时确认 RGB 灯效继续工作。

## 本地验证（2026-10-23）

- RAM 模型测试：清除全部 1056 字节、画面 1024 字节映射、列偏移 2、两边隐藏列清零、传输分段、READY 不再发送、错误停止与重启、计时回绕、ISR/关中断拒绝。
- 复用 u8g2 主机测试：逐像素位置、缓冲区边界、所有权和字体检查通过。
- 软件 I²C、RGB 驱动及现有 UART 11 项测试回归通过。
- Optimized/O2：Flash 26,736 B，RAM 6,632 B。
- Debug/O0：Flash 40,172 B，RAM 6,632 B。
- 新增 BSP 驱动与 main 通过 ARM GCC 严格告警和静态分析。未烧录、未提交，板级显示待确认。

```powershell
python tests/run_oled_tests.py
python tests/run_soft_i2c_tests.py
python tests/run_tca6507_tests.py
python tests/run_uart3_log_tests.py
```

u8g2 像素测试项目在 `Middleware/Ports/u8g2/tests`，使用本机原生 GCC 配置 CMake 后运行 CTest。复制来源及文件哈希记录在 `Middleware/u8g2-subset-manifest.json`，无须依赖相邻工程才能构建当前工程。
