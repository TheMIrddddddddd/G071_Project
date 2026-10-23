# G071 显示框架与 SHOWCASE 迁移交接

## 当前遥控器界面

Debug/Optimized 当前选择 `APP_DISPLAY_MODE=LAYOUT`：上方 16 行（0..15）为状态栏，y=16 是 1 行分隔线，下方 47 行（17..63）为四个摇杆。左上角第一个 20×16 区域（x=0..19）显示灯光图标，第二个区域（x=21..40）显示锁图标，第三个区域（x=42..61）显示快速模式闪电，第四个区域（x=63..82）显示雨刮器，第五个区域（x=84..103）显示手形 ↔ 方向盘，第六个区域（x=105..124）显示电池，相邻区域留 1 列间隔。通过现有 Optimized 编译烧录任务加载。下面的 SHOWCASE 迁移与资源记录属于原模式，选择 `APP_DISPLAY_MODE=SHOWCASE` 可使用该模式。

灯光轮廓根据用户提供的实物开关图片重绘：侧向灯体、左侧灯座、关闭斜杠、开启内部弧线及外侧四条光芒。参考 `01_灯光开关_OLED128x64.gif` 的 2760 ms 周期：关闭 480 ms、打开过渡 600 ms、开启 640 ms、关闭过渡 600 ms、关闭 440 ms。当前是自动模拟，不控制实际照明灯，也未接入 RK 状态报文。

锁图标参考 `锁梁左右180旋转_瘦高版_OLED128x64.gif`：锁体和锁孔固定，锁梁绕左腿由闭合位置转开到左侧，再转回闭合。`App/app_lock_icon.cpp` 用整数缓动改变锁梁横向投影，无浮点三角函数、无动态分配。1960 ms 周期包含闭合 240 ms、转开 440 ms、打开 520 ms、转回 440 ms、闭合 320 ms，与灯光周期独立；当前六个图标均为模拟状态，未接入 RK。

快速模式使用小图标的单像素 OFF 斜线和实心 ON 闪电，2520 ms 循环包含斜线收回、轮廓逐渐填充、点亮停留、逆向熄灭及斜线恢复。`scripts/generate_fast_icon.py` 从随工程保存的用户参考 GIF 生成 20 个去重的 20×16 位图与 39 段时间表，小图使用 1 像素禁止线。`App/app_fast_icon.cpp` 从 Flash 直接播放，不在 MCU 解码 GIF，不增加静态 RAM 或动态分配。可用 `python scripts/generate_fast_icon.py --check` 核对生成数据。

雨刮器以用户确认的大范围 GIF 为标准，原点在下方中央；保留关闭静止 800 ms、开启连续四轮往复（每轮 1200 ms）、关闭停靠 800 ms 的 6400 ms 周期。参考原件保存在 `scripts/assets/wiper_wide_sweep_128x64.gif`。`scripts/generate_wiper_icon.py` 使用固定裁剪及 20×12 光栅居中放入 20×16 格，适当增加垂直像素以区分下边框和雨刮臂，所有帧使用同一映射。生成 16 个去重位图及时间表；`App/app_wiper_icon.cpp` 仅依据经过时间选帧，不解码 GIF、不分配动态内存。

手形 ↔ 方向盘以确认后的清晰四角星预览为基础，手指朝上，指尖为直接收口的圆头，方向盘右上角为 7×7 等长四尖星。原始 GIF 保存在 `scripts/assets/hand_wheel_clear_star.gif`；`scripts/generate_hand_wheel_icon.py` 检查每个放大像素块为纯黑或纯白，修正手指顶部轮廓并按原缓动和 Bayer 阈值同步修正双向过渡，再打包为 14 个 20×16 XBM 位图和 80 项帧序列，共 752 B 常量。周期 3200 ms，每步 40 ms，包含两个方向的过渡和停留；`App/app_hand_wheel_icon.cpp` 按 64 位经过时间直接选帧，不解码 GIF、不动态分配、不增加静态 RAM。生成数据携带原 GIF 的 SHA256 和指尖修正说明。

电池沿用确认的 20×16 GIF，外框和右侧电极保持固定，充电用实心闪电表示，非充电依次演示 1、2、3、4 格电量，状态间保留原过渡。完整周期 6640 ms，当前是自动演示，未读取充电引脚或 RK 电量报文。原件保存在 `scripts/assets/battery_charge_levels_20x16.gif`，`scripts/generate_battery_icon.py` 原样打包为 31 个去重位图、32 段时间表；`App/app_battery_icon.cpp` 从 Flash 按 64 位经过时间选帧，不解码 GIF、不新增静态 RAM 或动态分配。

`App/app_light_icon.cpp` 只接收经过时间并绘制；两个 20×16 位图以常量行掩码保存，使用整数缓动及固定空间阈值作单色渐变，不在 MCU 上解码 GIF，不分配堆内存。`app_ui` 使用 TIM2 的 64 位时间推进模型；LAYOUT 最多每 40 ms 绘图一次，静止阶段像素不变时没有 OLED 数据传输。动画修改只在前台进行。

验证：`python tests/run_layout_tests.py` 检查图标范围、上下分区、停留阶段、双向过渡、五个独立循环、锁梁侧视状态、固定锁体、雨刮关闭停靠与连续刮动、四角星轮廓、64 位时刻及差分区域。可加输出路径导出每 40 ms 一帧的原始 1024 字节画面，例如 `python tests/run_layout_tests.py build/five-icons-preview/frames.bin`（先创建输出目录）。`python scripts/generate_hand_wheel_icon.py --check --verify-ui build/five-icons-preview/frames.bin` 已核对 320 帧第五格像素与指尖修正后的动画完全相同，包含循环回绕。修正范围仅在手形顶部，方向盘和四角星像素保持原样；手势源位图哈希与本次修改前一致，第四格雨刮放大预览为 `build/five-icons-preview/wiper-detail.gif`。

`python scripts/generate_wiper_icon.py --check --verify-ui build/five-icons-preview/frames.bin` 已核对 320 帧第四格像素与转换后的标准 GIF 完全相同，覆盖两轮及关闭停靠。

六图标版本继续通过 `python tests/run_layout_tests.py` 验证，新增电池固定外框、充电停留、四档电量与间隔、循环回绕和 64 位时刻检查。`python scripts/generate_battery_icon.py --check --verify-ui build/six-icons-preview/frames.bin` 已核对 320 帧第六格与确认的 GIF 逐像素一致。另与修改前保存的 `build/six-icons-preview/before.bin` 比较，320 帧中前五格及第六格外的所有像素完全不变。

六图标基准预览位于 `build/six-icons-preview/status-six-icons.gif`；接入摇杆前 Optimized Flash 40748 B、含预留 RAM 12656 B，Debug Flash 57132 B、RAM 12696 B。

## 四摇杆临界阻尼跟随

`App/app_joystick.cpp` 实现选定布局：双轴圆心为 (23,41)/(104,41)，外半径 17 px、实心圆点半径 2 px，叠加 06 的中心十字和 07 的四向短刻线。中间单轴位于 x=57/70，从 y=25 到 y=57；零位 y=41 处有左右各 4 px、总长 9 px 的贯穿横线，圆点经过零位时横线保持完整。双轴运动范围为半径 10 px，单轴上下各 11 px，圆点及清除背景均不会碰到外轮廓或状态栏。

算法直接调用已有 `Middleware/Ports/smooth_ui_toolkit/joystick_follow.cpp`，其六个固定 `smooth_ui_toolkit::Spring` 对象分别跟随左右 X/Y 和两个单轴。采用 k=100、m=1、c=2×sqrt(k×m)=20 的临界阻尼参数，静止位置/速度阈值均沿用库的 0.02。更新和绘图分离，前台常规采样最多 25 Hz；用 TIM2 的 64 位微秒时间计算解析位置。途中重定目标前先采样旧段，保留位置和速度；重复目标不会重新启动，静止后精确落到目标。没有新增堆分配，不在 ISR 计算或绘图，原库文件及哈希保持不变。

当前仍是模拟输入：左双轴 8 秒、右双轴 7.2 秒、左单轴 5.6 秒、右单轴 6.8 秒独立循环，包含不同方向、途中反向及回中停留。循环回绕不会重置弹簧。前台晚到时先采样旧段，再接收最新模拟目标，不补播错过的中间目标，避免补帧阻塞。

未来前台收到 RK 状态后，可调用 `app_joystick_set_targets(now_us, &targets)`：六个输入为 [-1,1] 的归一化量，+X 向右、双轴 +Y 和单轴正值向上。有限越界输入会裁剪，NaN/Inf/空指针和倒退时刻会原样拒绝。第一次有效调用停止自动模拟，之后由调用方提供目标；重新初始化可恢复模拟。该接口会在新目标时刻同步采样以保证连续性，绘图仍受现有 25 Hz 节奏限制。当前没有读取真实 ADC 或解析 RK 报文。

验证：LAYOUT 的 11 项主机测试覆盖布局、临界阻尼解析值、重复目标、保留速度的反向、采样节奏、非法输入与时间、圆点边界、四控件独立模拟、静止画面、长时间运行，以及初始化/更新/绘图零 C++ 分配；另验证摇杆静止时状态栏持续动画。完整 `tests/run_framework_tests.py` 回归通过，包括 12 项 SHOWCASE/内存测试、7 项 App 集成测试、刷新/软件 I²C/OLED/RGB/UART 测试及 153 个源文件/许可证哈希。

`python tests/run_layout_tests.py build/joystick-preview/frames.bin` 导出的 320 帧中，状态栏和分隔线与接入前基准逐像素相同。完整动态预览为 `build/joystick-preview/critical-spring-joysticks.gif`。当前 Debug/Optimized 均编译通过：Optimized Flash 62080 B、含预留 RAM 13368 B；Debug Flash 81632 B、含预留 RAM 13400 B，均保留至少 4 KB 栈。静态 `.su` 中摇杆更新/绘图函数分别为 56/80 B，这不是完整调用链栈高水位。板上耗时及实际帧率需烧录后观察 PERF 日志；本次未烧录、提交或推送。

## SHOWCASE 交付记录

默认 SHOWCASE 与源 `oled_test` 的实际烧录模式一致：Astra 菜单 29 秒、六种摇杆算法各 12 秒，共 101 秒循环。保留菜单树、图标、弹窗、记忆位置及速度 73；不叠加调试状态。模型按原算法目标 75 Hz 推进，绘图最多 30 Hz，屏幕帧率取决于 CPU、软件 I²C 和画面变化量，三者不能混称为 75 FPS。

状态面板通过独立构建模式选择，显示 uptime、成功应用的 RGB 颜色、刷新帧数和耗时、累计总线错误。RGB 仍独立每 500 ms 红→绿→蓝→全亮→全灭循环。两种画面共享相同传输、恢复和日志框架。

本次不包含真实摇杆 ADC、PCA9535 输入或其他外设迁移。源 SHOWCASE 使用自动输入，无须这些器件。

## 分层与接口

- `Core/Src/main.c`：CubeMX 初始化后只调用 `app_main_init()`，循环只调用 `app_main_poll()`。
- App：`app_display` 管理模型、绘图、发布与诊断；`app_rgb` 管理目标与成功状态；`app_bus` 管理设备退避和共享总线恢复。
- BSP：已验证的 PB5/PB6 软件 I²C 保留时序；OLED 负责初始化和页列定位，TCA6507 负责寄存器，UART 保留 DMA 环形缓冲。TIM2 64 位微秒累计接口只读计数器，不改配置，前台须至少每约 71 分钟读一次；正常每轮均读取。
- Middleware：u8g2 绘图，Astra 菜单，smooth_ui_toolkit 动画，OledRefresh 纯算法与缓冲所有权。

刷新使用两份所有权交换画面和一份成功画面，共 3072 字节。活动帧不可变；绘图可替换另一份待发送帧，只保留最新状态。每帧完整绘制后发布，不向活动帧增量画图。

差分算法按页查找变化，合并短间隙；每个区域只在开始时定位一次，然后按最多 32 个像素字节分段连续写入。每包仍是独立 START/地址/0x40/数据/STOP 事务，依靠 SH1106 列地址自增续写；跨区域、跨页必须重新定位。成本为 `length + 5 + 2 × ceil(length/32)` 字节、`1 + ceil(length/32)` 次事务。整帧为 1128 个线中字节、40 次事务（含地址和控制，不含 START/STOP 时长），比原先 1248 字节/64 次事务少 120 字节，字节量减少约 9.6%。驱动与规划器共用 32 字节块大小定义。合并仍是有界启发式，不宣称拆包后的全局最优；按新协议重新计算成本，不优于整帧时回退。无变化不发送。

此优化同时用于 132 列初始化清屏、128 列全帧更新和差分区域。每次 poll 最多一个事务，不把整页合成一次阻塞发送；RGB 可在 OLED 两包之间使用另一设备地址。任一定位或数据事务失败后停止续写，按原 App 恢复流程重新初始化、强制全帧同步，不信任失败后的列地址。

协议依据：[SH1106 数据手册](https://www.pololu.com/file/0J1813/SH1106.pdf) 第 13 页 I²C Protocol、第 29 页 Write Display Data 描述数据指针随写入自动更新；STOP 结束总线事务。本实现按控制器地址寄存器在事务间保留设计，主机 RAM 模型验证该流程，实际模块在 STOP/START 后续写及与 RGB 交替通信仍需板级验收。不能将主机模型当作实际模块的测量结果。

当前工作区的软件 I²C 阶段延时已由用户改为 `3U`，TIM2 仍为 PSC=15 的 1 MHz 基准；本次优化保留此设置。字节数收益与 SCL 速率无关，实际耗时与帧率需在板上对比。

扩展接口：

- `board_oled_start(frame)`：初始化、清 132×8 RAM、全帧和首次开屏。
- `board_oled_submit(frame, plan)`：借用不可变画面和页内区域规划，直到 READY/FAULT 交还；验证范围，防止长度求和回绕。
- `board_oled_abort(reason)`：停止软件传输状态机，不复位总线；返回后的同步传输已无硬件读者。
- `OLED_RefreshBeginDraw/Publish/Start/Complete`：管理活动、待发送及成功基准；失败使基准失效，保留最新待发送帧；CancelDraw 显式取消未完成绘图。
- `app_rgb_set(mode,color)`：CYCLE/FIXED/FLASH/BREATH，颜色掩码 P0=绿、P1=蓝、P2=红。FLASH/BREATH 仅在目标变化或恢复后写芯片 BANK0，由芯片独立运行；静态输出 P3-P6 固定关。

闪烁固定为约 256 ms 亮/256 ms 灭；呼吸为约 512 ms 渐亮、64 ms 保持、512 ms 渐灭、128 ms 灭。芯片 BANK 的双脉冲序列保持两个相同脉冲。具体时长受芯片内部时钟公差影响，尚未实测。

## 错误恢复与边界

轮询顺序为 UART → 总线恢复 → 到期 RGB → 模型、绘图和一次 OLED 短事务。没有 ISR 中绘图或恢复，没有整帧关中断。软件 I²C 同步短包仍占用 CPU；RGB 调度误差可能包含单包和一次绘图耗时，`rgb_late_ms` 供板级评估，不是硬实时零抖动保证。

地址 NACK 只退避对应设备：先两次等待 500 ms，第三次失败等待 5 秒，再继续探测/初始化。OLED 初始化还有自身 100/200 ms 等待，因此实际失败事务间隔不小于退避时间。健康 RGB 不因 OLED 离线被重新初始化。

BUS_BUSY/SCL_TIMEOUT 令共享总线进入阻塞状态；显示立即放弃活动帧并使基准失效。500 ms 间隔尝试恢复，最多三次；三次失败后冷却 5 秒再开始下一组。每次恢复释放线、检查 SCL、最多九个恢复时钟及 STOP。外部持续拉低或断电无法靠软件保证解除，只能有界报告并周期重试。

恢复成功增加总线代次，OLED 重新初始化并全帧同步最新画面，RGB 重写目标。不无限阻塞，不使用全局中断屏蔽。错误累计、连续失败、失败阶段/页/列、恢复结果可在 App/BSP 状态中查看，串口每 5 秒输出 DISPLAY/DIFF/PERF 汇总。

## C++ 内存与资源

迁移源码保留许可证和版本，记录于 `Middleware/showcase-source-manifest.json`，本地端口调整有独立 SHA256。厂商源码保持与相邻工程一致；当前工程独立构建，只有 `--reference` 像素对比测试需要原工程。

UI 使用固定 2048 字节首次适配分配池，可合并空闲块；初始化后封闭 `new/new[]`。越界或封闭后分配抛出 bad_alloc，由 App UI 边界捕获，串口报告并显示回退状态页，RGB/UART 继续运行。为正确展开构造与菜单调用链，UI C++ 开启异常；不使用 longjmp 跨越 C++ 对象。池的固定空间已计入链接 RAM；C 库最小堆 512 字节、MSP 栈预留 4096 字节。

链接器额外限制 RAM 预算为 32 KB（物理 36 KB），并保留原 Flash 128 KB 上限。2026-10-23 连续分包定位优化后构建（原 JSON 验证快照保留优化前的构建记录）：

| 配置 | 画面 | Flash B | RAM B，含预留 |
| --- | --- | ---: | ---: |
| Optimized | SHOWCASE | 89040 | 29680 |
| Debug | SHOWCASE | 128856 | 29760 |
| StatusOptimized | 状态面板 | 34900 | 12640 |
| StatusDebug | 状态面板 | 50704 | 12672 |

Debug Flash 余量较小，日常演示建议 Optimized。静态 `.su` 检查中 App 显示轮询单函数约 152 B、模型构造约 208 B；这些不是完整调用链或板上栈高水位测量，4 KB 栈仍待板级长期验证。

## 使用方式

当前分区预览：运行 **DAP: 编译并烧录 G071 Optimized（O2）**（Debug/Optimized preset 均选择 LAYOUT）。

状态面板：运行 **DAP: 编译并烧录 G071 状态面板 Optimized（O2）**。两者分别使用 build/Optimized 和 build/StatusOptimized，任务自动配置正确模式；也提供 Debug 版本及对应调试配置。

COM9、115200 8N1。关注启动 UI init/heap_peak、FAULT/RECOVER、DISPLAY 的成功帧与耗时、DIFF 字节数以及 PERF 的绘图耗时和串口丢弃。日志中的帧数为成功传输数，不代表屏幕经过肉眼验收。

## 已完成的本地验收

- SHOWCASE 11 个源行为场景及有界内存失败测试通过，初始化后无新增 C++ 分配。
- 实际 App/模型/库/差分/BSP 联合模型测试 7 个场景通过：模拟 304 秒超过三轮演示、OLED 单独离线、RGB 单独离线、部分写失败、SCL 卡住/冷却恢复、独立硬件灯效配置、UI 内存不足。
- 差分测试 70540 个输入（含 65536 个穷举掩码和 5000 个随机帧）：边界、像素重建、实际包成本、整帧上限、所有权、替换及失败全帧重同步通过。
- OLED 初始化/全帧/区域发送验证每页或每区域仅一次定位、最多 32 字节数据包、每 poll 最多一次事务、跨页和列偏移、间隙像素保留、中途失败停止及重新初始化后全帧同步；软件 I²C（当前 3 μs 阶段配置）与 TIM2 累计、RGB 数据包、UART 11 项测试通过。
- 源与迁移版本使用相同主机工具链，在整轮 101 秒每 80 ms 抽样输出像素；两文件逐字节一致，SHA256 为 `0c98ae36a146dfb3ccd44670bac48a620e3f094e39dfd03145b03ccff38b8663`。
- 四种 ARM 构建与内存预算检查通过，App、刷新算法及新增 BSP 通过严格告警/转换检查和 GCC 静态分析。源码无 L476 HAL、DWT 或硬件 I²C DMA 依赖。

复现：

```powershell
python tests/run_framework_tests.py --reference
python scripts/verify_ui_sources.py
python scripts/validate_build_budget.py build/Optimized/g071_test.elf
```

## 板级待验证

本轮未烧录、未提交、未上传。后续授权烧录后，至少观察三轮 SHOWCASE、RGB 连续切换、多次复位，验证设备掉线/重新接入，以及受控故障恢复。记录真实帧率、总线波形、RGB lateness、UART 错误/丢弃及栈使用；主机的模拟时间和相同像素不证明 G071 上的显示速度或电气恢复。
