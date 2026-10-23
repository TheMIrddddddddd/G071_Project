# PB5/PB6 软件 I²C 与 500 ms 地址探测

## 模块职责

- `BSP/board_soft_i2c.c/.h`：独占 PB5/SCL、PB6/SDA 和 TIM2，封装开漏线操作、微秒时序、协议、ACK、超时和总线恢复；不依赖串口、OLED 或 TCA6507 驱动。
- `Core/Src/main.c` 的 USER CODE：组织应用，每 500 ms 分别探测 OLED `0x3C` 和 TCA6507 `0x45`，将两次真实返回状态放入同一行日志。
- `BSP/board_usart.c`：维持现有 DMA 和环形缓冲区日志；I²C 驱动不直接调用串口。
- CubeMX：维护 GPIO、TIM2 初始化及 HAL 驱动。自定义源文件由顶层 CMake 纳入；应用代码位于 USER CODE 区。

## 硬件与计时前提

PB5/PB6 均为开漏、初始 High、无内部上下拉；High 表示释放，引脚高电平依赖外部上拉。应确认板上有合适的上拉到 3.3 V，空闲时两条线都为高。

当前 HCLK=PCLK=16 MHz，APB 分频为 1；TIM2 内部时钟、PSC=15、ARR=0xFFFFFFFF。驱动检查这些前提并启动 TIM2。TIM2 归驱动独占，运行中不可停表、重置计数器或调整时钟。每计数 1 μs，计数差使用无符号减法处理回绕。

数据位和 ACK 位的低、高阶段各保留一次 6 个 TIM2 计数的等待（1 μs 分辨率），取消拉低 SCL 后与下一位建立阶段重复的等待。START/STOP 和恢复时钟仍保留必要等待。GPIO 使用 BSRR 原子写入和 IDR 实际电平读取，保持开漏释放、ACK 检测和时钟拉伸超时语义；不使用 ODR 读改写。实际周期还包含计时量化、函数调用、GPIO 操作和中断开销，需要测量。GPIO Speed=Medium 不代表总线频率。中断保持开启，会延长位时序，驱动不会补偿性缩短后续阶段。

## 接口约定

- `board_soft_i2c_init()`：在 `MX_GPIO_Init()`、`MX_TIM2_Init()` 后调用，启动时间基准并检查空闲电平；不发送地址、不自动恢复。
- `board_soft_i2c_probe(address)`：START → 7 位地址及写方向 → 读取第 9 个时钟的 ACK → STOP。地址参数不左移，例如传 `0x3C`，线上地址字节是 `0x78`；`0x45` 对应 `0x8A`。不发送寄存器或业务数据。
- `board_soft_i2c_transfer()`：支持纯写、纯读及写后 repeated START 再读；读操作最后一个字节由主机发送 NACK，其余发送 ACK。错误时丢弃本次读取的数据，可能只写入了缓冲区前缀。
- `board_soft_i2c_recover()`：显式执行最多 9 个恢复时钟，再尝试 STOP；不能释放的 SDA 报 BUS_BUSY，不能升高的 SCL 报 SCL_TIMEOUT。STOP 本身也需要一次 SCL 上升沿。

单主机、单前台调用者；不得在 ISR 或关中断环境调用，不可跨任务并发访问。无动态分配，没有全局关中断。单次 SCL 拉高最多等待 2 ms；完整事务耗时还与字节数、逐位拉伸及中断有关。

## 主循环与输出

主循环持续调用 `board_usart3_poll()`，另以 `HAL_GetTick()` 的差值调度探测，无 `HAL_Delay(500)`。第一轮在初始化后约 500 ms 开始，以后每轮启动间隔至少 500 ms；不追赶补发。两设备串行探测，同一轮输出一行，例如：

```text
[BOOT] SoftI2C PB5=SCL PB6=SDA TIM2=1us init=OK
[I2C] count=1 tick=501 OLED(0x3C)=ACK TCA6507(0x45)=ACK tx_fail=0 tx_drop=0
[I2C] count=2 tick=1001 OLED(0x3C)=ADDR_NACK TCA6507(0x45)=ACK tx_fail=0 tx_drop=0
```

以上是格式示例，非板上采样。两地址不匹配或设备未接会出现 ADDR_NACK；SCL_TIMEOUT、BUS_BUSY 是总线状态错误。发现 BUS_BUSY 后，应用每轮最多恢复一次，并单独打印恢复结果。UART 队列非阻塞，日志实际在线上出现的时间晚于探测时间；tick 表示本轮开始时刻。

## 验证与板级验收

2026-10-23：GPIO 级主机模型运行真实 BSP 驱动，验证地址移位、ACK/NACK、STOP、NACK 后下一次成功、初始及传输中途 SCL 超时、SDA 卡低/恢复、时钟拉伸、32 位计数回绕、ISR/关中断拒绝、定时器未启动/配置错误拒绝、数据 NACK、写入数据、多字节读取、重复起始及末字节 NACK。现有 UART 11 项测试同时回归。

结果：上述主机测试全部通过；Debug 编译链接通过，无编译警告，Flash 27,800 B、RAM 5,432 B（链接器统计含预留堆栈）。`main.c` 和软件 I²C 驱动通过 ARM GCC 的 `-Wextra -Werror -Wconversion -Wformat=2 -fanalyzer` 检查，检查时将厂商 Drivers 头文件作为系统头处理。

2026-10-23 时序优化：保留实际编译参数 `-O0 -g3`，未改变编译优化、系统时钟、CubeMX 配置或 500 ms 应用调度。GPIO 模型增加 BSRR 高低半字解码、IDR 采样和位周期检查，确认不再重复等待低阶段且高低阶段均有等待；原有协议测试和 UART 11 项测试通过。Debug 编译及驱动严格静态检查通过，更新后 Flash 27,744 B、RAM 5,432 B。主机模型不包含真实 CPU/GPIO 指令耗时，不能据此宣称板上达到某个频率；未执行烧录，需要重新测量 SCL 周期、高低电平及上升沿。

```powershell
python tests/run_soft_i2c_tests.py
python tests/run_uart3_log_tests.py
```

主机模型不能验证物理上拉、上升沿、实际频率或从机兼容性。本次不烧录；后续授权烧录后，在 COM9 用 115200 8N1 观察每 500 ms 的 ACK 日志，用逻辑分析仪确认地址、ACK 和 STOP，再进行 TCA6507 寄存器及 OLED 功能验证。地址 ACK 不代表设备业务功能已验证。
