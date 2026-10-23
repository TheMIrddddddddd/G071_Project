# TCA6507 RGB 基础灯控

此文保留初期灯控接入记录。当前 App 独立调度、硬件灯效和恢复策略见 [G071_DISPLAY_FRAMEWORK.md](G071_DISPLAY_FRAMEWORK.md)。

## 接线与控制方式

沿用相邻 `i2c_test/Core/Src/board_test.c` 记录的接线：P0=绿、P1=蓝、P2=红，七位地址为 `0x45`。实际颜色对应关系仍需看灯确认。

依据 [TI TCA6507 数据手册](https://www.ti.com/lit/ds/symlink/tca6507.pdf) 的控制字节和输出选择表：命令 `0x10` 开启从 SELECT0 开始的寄存器自动递增；`SELECT2:SELECT1:SELECT0=100` 表示全亮、输出低，`000` 表示关灯、高阻。

每次设置发送 `[0x10, 0x00, 0x00, color_mask]`，分别写入 SELECT0/1/2。驱动独占本芯片七路输出，P3-P6 固定关闭。基础测试不使用 PWM、亮度或渐变寄存器。

| 目标 | 掩码 | 对应输出 |
| --- | --- | --- |
| OFF | 0x00 | 全灭 |
| RED | 0x04 | P2 |
| GREEN | 0x01 | P0 |
| BLUE | 0x02 | P1 |
| WHITE | 0x07 | RGB 全亮，实际混色未必是标准白色 |

## 模块边界

- `BSP/board_tca6507.c/.h`：初始化全灭、设置 RGB 位掩码、检查参数并返回 I²C 状态；不负责时间、日志或总线初始化。所有寄存器细节留在驱动内部。
- `BSP/board_soft_i2c.c/.h`：复用用户优化后的传输；本轮没有修改其 GPIO 访问或时序。
- `Core/Src/main.c` USER CODE：应用状态机与日志，启动时先全灭，之后每 500 ms 执行 RED → GREEN → BLUE → WHITE → OFF → RED 循环。全灭也保持一个周期，没有首尾重复的灭灯步骤。

原来的 OLED/TCA6507 地址探测保留，每轮先输出 `[I2C]`，再写灯并输出 `[TCA6507]`；事务在同一前台顺序执行。无 `HAL_Delay(500)`，主循环继续服务 UART DMA。

写成功后才推进颜色。写失败时不能保证实际灯状态不变，下一周期完整重写同一个目标颜色。总线忙时沿用应用的每轮一次恢复机制；驱动不自行复位设备、重启总线或吞掉错误。

## 日志示例与验收

下面仅为格式示例，不是实际板上输出：

```text
[TCA6507] init=OFF write=OK map=P0:G,P1:B,P2:R
[TCA6507] tick=501 step=1 target=RED write=OK
[TCA6507] tick=1001 step=2 target=GREEN write=OK
[TCA6507] tick=1501 step=3 target=BLUE write=OK
[TCA6507] tick=2001 step=4 target=WHITE write=OK
[TCA6507] tick=2501 step=5 target=OFF write=OK
```

`write=OK` 表示地址、命令和数据的 I²C 传输成功，尚未读取寄存器回验；需要结合 RGB 实际变化判断板级功能。检查各颜色、全亮和全灭，以及失败日志能否区分地址 NACK、数据 NACK、超时与总线忙。使用 COM9、115200 8N1 查看日志。

## 本次本地验证（2026-10-23）

- TCA6507 主机测试：初始化包、8 种 RGB 掩码、接线定义、非法位拒绝、底层错误透传和错误后重试全部通过。
- 软件 I²C 协议主机测试通过；现有 UART DMA/环形缓冲区 11 项测试通过。
- Optimized（O2）：Flash 20,228 B，RAM 5,440 B。
- Debug（O0）：Flash 28,860 B，RAM 5,440 B。
- 均编译链接通过，无编译警告。本轮未烧录、未提交，真实灯光效果待板级验证。

```powershell
python tests/run_tca6507_tests.py
python tests/run_soft_i2c_tests.py
python tests/run_uart3_log_tests.py
```
