# 在 CubeMX 配置 USART3 DMA

打开工程根目录 `g071_test.ioc`。当前文件已保存下面的 DMA/NVIC 配置，可以在界面核对；新建配置时按这些步骤添加。

## 1. 串口和引脚

进入 **Connectivity → USART3**，模式选择 **Asynchronous**。

| 项目 | 设置 |
|---|---|
| PB8 | USART3_TX，AF4 |
| PB9 | USART3_RX，AF4 |
| Baud Rate | 115200 |
| Word Length / Parity / Stop Bits | 8 Bits / None / 1 |
| Data Direction | Receive and Transmit |
| Hardware Flow Control | None |

系统时钟继续使用内部 HSI16、SYSCLK/HCLK/PCLK 16 MHz，不需要为 DMA 开 PLL。

## 2. 添加发送 DMA

在 USART3 的 **DMA Settings** 页点击 **Add**，选择 `USART3_TX`，设置：

| 项目 | 设置 |
|---|---|
| DMA Request | USART3_TX |
| DMA Channel | DMA1 Channel1 |
| Direction | Memory to Peripheral |
| Mode | Normal |
| Priority | Low |
| Peripheral Increment | Disable |
| Memory Increment | Enable |
| Peripheral Data Width | Byte |
| Memory Data Width | Byte |

DMAMUX 同步和请求发生器保持默认关闭。发送环形队列在软件中维护，TX DMA 发送一段后重新调度，因此这里选择 **Normal**。

## 3. 添加接收 DMA

再次点击 **Add**，选择 `USART3_RX`：

| 项目 | 设置 |
|---|---|
| DMA Request | USART3_RX |
| DMA Channel | DMA1 Channel2 |
| Direction | Peripheral to Memory |
| Mode | Circular |
| Priority | High |
| Peripheral Increment | Disable |
| Memory Increment | Enable |
| Peripheral Data Width | Byte |
| Memory Data Width | Byte |

DMAMUX 同步和请求发生器保持默认关闭。RX 需要一直写入循环 DMA 缓冲，因此这里选择 **Circular**。

## 4. 开启 NVIC

进入 **System Core → NVIC**，核对以下中断已启用；USART3 的中断也可以在 USART3 的 **NVIC Settings** 页开启。

| CubeMX 中断 | 代码名称 | 抢占优先级 |
|---|---|---:|
| DMA1 channel 1 global interrupt | DMA1_Channel1_IRQn | 2 |
| DMA1 channel 2 and channel 3 global interrupt | DMA1_Channel2_3_IRQn | 2 |
| USART3 / USART4 / LPUART1 global interrupt | USART3_4_LPUART1_IRQn | 2 |

子优先级若显示则保持 0。UART 和 RX DMA 使用相同抢占优先级，避免 IDLE 与 DMA 回调嵌套操作接收队列。

**IDLE 空闲检测由 BSP 中 `HAL_UARTEx_ReceiveToIdle_DMA()` 开启。** CubeMX 负责 USART3 全局中断、RX DMA 和 Circular 配置；无需寻找单独的 IDLE 勾选项。接收 DMA 的半满和满中断保留启用。

即使只发送日志，USART3 全局中断也需要开启：普通 TX DMA 完成后，HAL 还通过 UART TC 中断结束发送并调用完成回调。

## 5. 保存和生成

在 **Project Manager → Code Generator** 勾选 **Keep User Code when re-generating**，保持按外设生成独立 `.c/.h` 的选项，然后保存 `.ioc`，点击 **GENERATE CODE**。

核对初始化顺序：

```c
MX_GPIO_Init();
MX_DMA_Init();
MX_USART3_UART_Init();
/* USER CODE BEGIN 2 */
UART_LogInit(); /* 里面调用 board_usart3_init() */
/* USER CODE END 2 */
```

DMA 时钟必须在 UART MSP 中初始化 DMA handle 前开启。若顺序不符，在 **Project Manager → Advanced Settings → Generated Function Calls** 中把 `MX_DMA_Init()` 调整到 `MX_USART3_UART_Init()` 之前，再生成。

## 6. 代码的保存边界

```text
Core/                      CubeMX 管理外设初始化与中断入口
  Src/main.c               应用接入只写 USER CODE 区
BSP/
  board_usart.c/.h         DMA 收发调度、回调、日志格式化、错误恢复
Middleware/Ringbuffer/
  inc/ringbuffer.h         参考工程的环形缓冲区接口
  src/ringbuffer.c         通用环形算法及 DMA 查看/释放接口
CMakeLists.txt             顶层用户源与 include 配置
```

- `dma.c`、`usart.c`、`stm32g0xx_it.c` 等外设设置从 `.ioc` 重新生成。不要在这些文件生成区维护日志队列或业务代码。
- BSP 和 Middleware 独立于生成目录，源码和头文件路径已纳入顶层 `CMakeLists.txt` 的用户配置。
- `main.c` 的 `#include "board_usart.h"`、日志初始化与轮询位于 USER CODE 区。
- 应用使用 `board_usart3_printf()`；调试器观察 `board_usart3_stats`。

重新生成后先运行 `cmake --preset Debug`、`cmake --build --preset Debug`，通过后再使用已有的 DAP 烧录调试入口。
