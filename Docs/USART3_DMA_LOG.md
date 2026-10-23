# USART3 DMA 日志与接收

## 当前实现

分层与参考工程一致：`BSP/board_usart.c/.h` 负责串口，`Middleware/Ringbuffer/inc、src` 放通用环形缓冲区。环形库从参考工程复用并保留原许可证，新增非消费式 `rt_ringbuffer_peek_linear()` 与 DMA 完成后的 `rt_ringbuffer_discard()`。原库的 `peek()` 会推进读指针，本驱动不调用它。

CubeMX 界面配置步骤见 [USART3 DMA 配置](CUBEMX_USART3_DMA.md)。

参考 `../GD32_SieMens/IndustrialEmbedded/BSP/board_usart.c` 的收发队列、接收 DMA 游标和溢出统计思路。
参考工程日志发送使用 TBE 逐字节中断；本工程发送改为普通模式 DMA。接收采用循环 DMA + IDLE/HT/TC 事件，IDLE 不参与日志发送。

| 项目 | 配置 |
|---|---|
| 芯片与时钟 | STM32G071CBU3，HSI16，16 MHz |
| 串口 | USART3，PB8 TX、PB9 RX，115200、8N1 |
| TX DMA | DMA1 Channel1，DMAMUX USART3_TX，Normal，字节宽度 |
| RX DMA | DMA1 Channel2，DMAMUX USART3_RX，Circular，字节宽度 |
| 中断 | DMA1_Channel1、DMA1_Channel2_3、USART3_4_LPUART1，优先级均为 2 |
| TX 环形队列 | 2048 B；单次 DMA 最多 128 B；DMA 正在使用的字节在完成前不释放 |
| 格式化缓冲 | 192 B，最多 191 字节有效文本；超长整条拒绝，不发送截断日志 |
| RX 缓冲 | 128 B 循环 DMA + 512 B 软件环形缓冲 |

正常发送路径为：业务格式化 → 整条复制进 TX 环形队列 → 前台启动一段 DMA → DMA TC → UART TC → 完成回调置标志 → 前台释放该段并启动下一段。
不等待串口发送或队列空间。TX 半满中断关闭；每段正常发送产生 DMA 完成和 UART 发送完成中断，避免逐字节中断。

## 业务调用

```c
#include "board_usart.h"

/* MX_GPIO_Init() → MX_DMA_Init() → MX_USART3_UART_Init() 之后调用一次。 */
board_usart3_init();
(void)board_usart3_printf("[I2C] address=0x%02X status=%u\r\n", 0x3CU, 0U);

/* 主循环中持续调用。不要在软件 I2C 的每个 bit 内格式化日志。 */
board_usart3_poll();

/* 收到的数据保留在 RX 环形缓冲中，业务按需读取；当前无命令解析或自动回显。 */
uint8_t input[32];
uint16_t received = board_usart3_receive(input, sizeof(input));
(void)received;
```

当前 `main.c` 已调用初始化和 Poll，并每秒输出一次心跳。业务日志使用 `board_usart3_printf()`，不依赖标准 `printf()` 重定向。
Write、Printf、Read 仅允许前台单调用者；中断内调用 Write/Printf/Read 会返回 0，不执行格式化。未来引入多任务时需要重新设计生产者同步。
TX 队列空间不足时整条丢弃新消息并累计丢弃数；RX 环形缓冲满时保留未读数据，丢弃新收到的多余字节并累计字节数。

DMA 静态缓冲与队列由日志模块独占；长度以字节计，外设/内存宽度均为 8 bit。G071 Cortex-M0+ 无数据缓存，不需要 D-cache clean/invalidate。
TX 环形缓冲区仅由前台操作，中断只通知完成；DMA 使用的连续区在完成前不释放。RX 在中断中写入、前台读取；复用的环形库不提供内部同步，因此 BSP 在 RX 读取时保存 PRIMASK 并屏蔽中断，完成最多 512 B 的有界复制后恢复原状态。取出/清除回调事件也使用短临界区。日志格式化和 TX 入队不在关中断区内。

## 错误与时序边界

- TX 250 ms 未收到完成事件：前台停止该次传输，停止成功后再重试同一段；停止失败时继续保留 DMA 数据及队列位置，不覆盖它。
- HAL 启动失败及恢复重试间隔至少 10 ms，不忙等。
- UART/DMA 错误回调只记录事件，前台恢复收发。错误后的重发可能重复已经发送的前缀，日志不提供消息恰好送达一次的保证。
- RX IDLE、HT、TC 中读取实时 DMA 计数器，不直接信任可能延迟到达的回调位置，避免相邻事件重复提交；正常 IDLE 不停止和重启循环 DMA。
- RX 事件回调只进行有界复制与计数，无打印、协议解析或等待。128 B DMA 在 115200、8N1 下每半缓冲约 5.56 ms；必须及时服务中断。软件 I2C 不应整帧关闭中断；整圈数据被 DMA 覆盖后无法仅凭计数器完整恢复。
- DMA 降低发送 CPU 开销，格式化、队列复制和 ISR 仍有开销；当前没有板上性能测量结果。115200、8N1 的持续线路容量约为 11520 B/s，应控制日志频率并观察丢弃计数。

## VS Code 验证

继续使用 `DAP: G071 编译下载并调试`；停在 main 后按 F5 继续运行，暂停后观察：

| WATCH | 正常含义 |
|---|---|
| `uart_log_count` | 心跳计数持续增长 |
| `board_usart3_stats` | 展开查看全部收发统计 |
| `board_usart3_stats.tx_dma_starts` | DMA 发送段数增长 |
| `board_usart3_stats.tx_complete_events` | 发送完成回调累计数增长 |
| `board_usart3_stats.tx_queued_bytes` | 间歇日志发送完成后回到 0 |
| `board_usart3_stats.tx_errors` / `tx_timeouts` | 正常保持 0 |
| `board_usart3_stats.tx_dropped_messages` | 正常日志负载下保持 0 |
| `board_usart3_stats.rx_bytes` / `rx_idle_events` | PB9 收到数据后增长 |
| `board_usart3_stats.rx_dropped_bytes` | RX 满后增长；应用需要调用 Read 消费数据 |

串口查看需要 PB8 → USB 串口 RXD、GND 共地；测试接收还需要 USB 串口 TXD → PB9。仅连接 SWD 不会传送 UART 文本。

预期日志格式：

```text
[BOOT] STM32G071 started
[BOOT] SYSCLK=16000000 Hz
[BOOT] USART3: 115200 8N1, TX=DMA+ring, RX=DMA+IDLE+ring
[RUN] count=1 tick=1000 tx_fail=0 tx_drop=0 rx=0
```

## 构建与测试记录（2026-10-22）

- Debug、Release 均已编译链接通过，无编译或链接警告。Debug Flash 23944 B，Release Flash 15952 B；RAM 均为 5352 B（包含堆、栈预留）。
- BSP/应用通过 ARM GCC `-Wall -Wextra -Werror -Wformat=2 -Wconversion -fanalyzer` 检查；复用的环形库使用 `-Wall -Wextra -Werror -fanalyzer` 检查，保留其已有整数与位域写法。
- `python tests/run_uart3_log_tests.py`：11 项主机测试通过，编译真实 BSP 和 Middleware，加假 HAL；覆盖 TX 回绕、满队列、DMA 所有权、HAL_BUSY、停止失败与超时重试、格式边界、RX HT/TC/IDLE 去重、接收溢出及错误恢复、tick 回绕、中断屏蔽状态恢复，以及新增中间件查看/释放接口。
- `.ioc` 已配置匹配的 DMA 请求和 NVIC；生成源放在 CubeMX CMake 列表，BSP 与 Middleware 放在顶层 CMake 用户源列表。
- 在独立副本 `build/cubemx-layer-check/g071_test` 实际执行 CubeMX 再生成：BSP、Middleware、main 的 USER 区及顶层 CMake 内容保持不变，中断入口无重复。副本全量构建 28/28 通过（Flash 23976 B，RAM 5352 B）；主工程仍使用其本地生成文件，未用副本绝对路径覆盖主工程。
- 执行期间发现 `.vscode/settings.json` 被外部改回 `cube-cmake` 并追加 `CMAKE_COMMAND` 参数；该外部变更予以保留。本次验证使用当前 tasks 的绝对 CMake 路径及 CMake presets。
- 本版未烧录、未提交 Git；尚未取得这版固件的 DMA 完成、串口收发或软件 I2C 并行运行证据。

主机测试需要原生 GCC；默认自动寻找 PATH 中的 gcc，否则使用本机 Qt MinGW。其他电脑可运行 `python tests/run_uart3_log_tests.py --cc <原生gcc路径>`。测试产物位于已忽略的 `build/host-tests/`。
