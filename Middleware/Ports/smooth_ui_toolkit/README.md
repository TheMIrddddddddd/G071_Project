# smooth_ui_toolkit STM32 移植

`joystick_follow` / `joystick_draw` 为项目适配。v36.2 已将跟随改为上游
`smooth_ui_toolkit::Spring` 临界阻尼解析生成器，保留真实 u8g2 绘图。
六个固定对象直接调用 `init/retarget/next`，不经过会分配生成器的 Animate 包装，
也不使用有 60 秒限制的 SmoothPort_Sample。段时间由64位单调时钟先相减再转换。
真实输入刚度3600、阻尼120、质量1；自动演示100/20/1。接口及验收见
`../../../Docs/OLED_V36_2_CRITICAL_SPRING.md`。v26 文档保留早期指数模型历史。

官方仓库：[Forairaaaaa/smooth_ui_toolkit](https://github.com/Forairaaaaa/smooth_ui_toolkit)。
固定 commit：`3cfe6a373edee2e325b8490491f921ecc71b29e6`，未更换用户指定版本。
归档：`https://codeload.github.com/Forairaaaaa/smooth_ui_toolkit/tar.gz/3cfe6a373edee2e325b8490491f921ecc71b29e6`。
归档 SHA-256：`f7d2a1cd73f2e3ac2709deeb5078e95ec46e0f019c6fc68cfca7a53b3304565d`。
保留根 LICENSE（MIT、Forairaaaaa）、README，以及源文件内原有版权声明。

## 引入范围

只排除 `src/core/color/` 和 `src/lvgl/`，共 24 个文件；详细路径在
`../../smooth_ui_toolkit-source-manifest.json` 的 `excluded_files` 中。
其余 `src` 共 64 文件，加 LICENSE/README，共 66 文件；包括 14 个 `.cpp`、
49 个头文件和 FPM readme。上游仓库根目录的例程和测试不是本次引入范围。
这是明确裁剪并打补丁的源码树，不能描述为完整上游归档逐文件一致。
目录中没有嵌套 `.git`。

`CMakeLists.txt` 编译全部 14 个保留的 `.cpp`；`smooth_module_check.cpp`
包含并实例化工具、数学、两个菜单和两个游戏模块。最终固件可由链接垃圾回收
去除未调用的功能。编译依赖审计检查全部 49 个头文件确实被编译器读取。

```powershell
python UserMiddlewares/verify_smooth_vendor.py
python UserMiddlewares/verify_smooth_vendor.py --compile-commands build/Debug/compile_commands.json
```

清单保存原文件 SHA-256、16 个修改文件的当前 SHA-256，以及
`patches/stm32-port.patch` 的哈希。补丁已对从固定归档恢复的文件执行
`git apply --check`，新增平台文件在本目录中，不混入上游目录。

## 本地补丁

| 文件/模块 | 修改原因与验证 |
| --- | --- |
| `core/core.hpp` | 颜色入口由 `SMOOTH_UI_TOOLKIT_ENABLE_COLOR=0` 排除；完整入口头文件仍参加编译检查 |
| `tools/random/random.hpp` | 同样禁用颜色依赖和 `getRgbColor` API；数值随机和 choice 保留，使用固定种子代替桌面 random_device |
| `tools/event/signal.hpp`、`event_queue.hpp` | 选择项目的单上下文锁适配；仍执行事件入队、出队和观察者回调 |
| `animate/animate.cpp` | 重新播放初始化生成器；暂停期间改目标重置该段时间，恢复不会沿用旧段已经消耗的时长 |
| `animate_value/animate_value.cpp` | stop 先读取当前值，再清运行标记，避免回退到旧默认值 |
| `generators/spring/spring.cpp/.hpp` | 定义初始速度；正速度对应坐标正方向，途中 retarget 保留速度；修正过阻尼解析系数，使用衰减实指数避免长时间 `0*inf`；收敛后精确落到目标 |
| `generators/easing/easing.cpp/.hpp` | 零/负时长直接完成，避免除零与 NaN |
| `sequence/animate_sequence.cpp/.hpp` | setSequence 替换而非追加；单元素取消不越界；保留并复用已创建生成器；起始关键帧不附带额外动画时长；暂停阻止阶段推进；取消后 update 不继续执行 |
| 两个 `widget/select_menu/*.cpp` | 空菜单索引检查避免 size−1 下溢；selector.jumpTo 真正瞬移 |
| `games/core/frame_clock.hpp` | 先做 uint32 毫秒差再转秒，修复回绕和长运行浮点差分精度问题 |
| `games/core/system/collision.hpp` | 碰撞表使用 ShapeType，而非 ComponentType；圆与矩形不会被当成同一种形状 |

测试先复现了零时长缓动、弹簧初速度/改目标连续性、暂停改目标、序列追加、
stop 丢失当前值、空菜单、游戏时钟回绕和序列刚开始暂停的失败。
游戏最小调用中 DVD 有碰撞、Breakout 无碰撞的差异定位到了碰撞表类型索引。
补丁后的本项目回归覆盖这些边界；没有把这些测试称为上游官方测试套件。

`getRgbColor` 实质依赖被排除的颜色模块，因此该 API 在此配置中不可用，
没有提供空实现。LVGL 适配层完全不引入。数值随机、事件、对象池、游戏仍存在。

## 平台、时间与内存约束

- `smooth_project.hpp` 为项目入口，所有使用者链接 `oled_smooth_port` 取得编译宏。
- `SMOOTH_UI_TOOLKIT_ENABLE_DEFAULT_HAL=0` 作用到上游 `hal.cpp` 本身。
  时间回调返回同一轮计算冻结的毫秒值；没有 chrono/thread/sleep 依赖。
- `smooth_port.cpp` 接受既有 `OLED_TimeUs()` 的 uint64 时间。场景开始时先取消旧动画，
  再重设相对时间原点；每场景不超过 12 秒，避免把长运行绝对秒转换为 float。
  多通道共享一次采样，使用同一显式时间；自动 value 调用也读取相同冻结时刻，
  不与 dt 更新模式混用。禁止的 delay 调用会终止程序，不默默忽略等待请求。
- 动画对象通过 placement new 在显式 Init 中构造，此时硬件和时间源已经就绪。
  全局 storage 只是未构造的字节区；不存在启动前访问 HAL 的动画全局构造函数。
- 保留工具链原有 `-fno-rtti -fno-exceptions -fno-threadsafe-statics`：源码没有
  dynamic_cast/typeid/throw/catch，游戏组件通过类型标记访问，初始化只在同一前台发生。
  分配失败采用显式终止，不依赖异常恢复。
- `smooth_memory.cpp` 包装全局 C++ new/delete，限制尚未释放的请求总量
  （含包装头）为 16 KiB，并统计次数/峰值。启动后 Seal 禁止任何新的 C++ 分配。
  该统计不包含 newlib malloc 的内部元数据或 C 库自己的分配，不等同整个堆实测占用。
- Linker 为堆预留 16 KiB、栈预留 4 KiB，`_sbrk` 保留原有栈边界检查。
  `_Min_Heap_Size` 是链接预留量，不是 malloc 的独立硬上限。
- Host 模型启动 19 次分配，峰值 1792 B（x64 ABI）；三分钟循环、场景切换、
  完整 DMA 模拟和错误恢复后 new/delete 次数不变。ARM 的实际数值由启动
  `[ANIM] heap_peak/heap_live/alloc` 输出，尚未取得板上记录。
- `smooth_single_context.hpp` 只支持同一前台上下文，锁记录占用并拒绝回调重入。
  它不是 ISR/RTOS 并发互斥量；所有此类 API 禁止从 ISR 调用。

## 模块状态

下表“测试”是本项目的边界或最小行为验证，不代表上游所有 API 的完整覆盖。
所有行均尚未板测。

| 上游模块 | 引入/编译 | 本地测试 | OLED 演示 |
| --- | --- | --- | --- |
| Animate、AnimateValue | 是 | 起止、初速度、retarget、pause/resume、stop/restart、同目标 | A/B/C/D |
| AnimateVector2 / Vector4 | 是 | 二/四轴更新、同时改目标、反向；Vector4 四轴收敛 | Vector2 用于 A/B |
| Spring / Easing 生成器 | 是 | 三种阻尼、零位移、零时长、固定时刻、长时有限值 | A/B/C/D |
| animation/sequence | 是 | 顺序、暂停、取消、重新配置、复用对象 | D |
| easing / CubicBezier | 是 | 端点、中点、Bezier 最小调用 | C 使用 LINEAR/OUT/INOUT/BACK |
| hal | 是，默认桌面 HAL 关闭 | 同步采样、相对时间、跨 HAL 回绕/第十天重建场景 | 全部 |
| math、uitk | 是，含头文件实例化 | 向量长度、矩形锚点、namespace 别名 | 全部 |
| 两个 select_menu | 是 | 空菜单、选择、瞬移；options 最小调用 | B 用 SmoothSelectorMenu |
| event / observable / ring_buffer / object_pool / random | 是，模板实例化 | 事件投递、通知、覆盖、销毁复用、数值随机 | 不进入屏幕场景 |
| FPM fixed/math/ios | 是，模板实例化 | 定点运算、sqrt、流格式化 | 不进入屏幕场景 |
| games core / DVD / Breakout | 是 | 世界更新、生命周期、矩形/圆碰撞、时钟回绕 | 未增加游戏场景 |

游戏碰撞系统会构造临时容器，碰撞集合也可能分配节点，因此不适合直接放入
已经 Seal 的演示循环。两个游戏的功能验证使用独立诊断进程，保留实际功能。
后续若增加游戏场景，必须先为这些工作区和集合提供有界、可复用的存储。

完整应用与板级验收说明见 `../../../Docs/OLED_V24_SMOOTH_ANIMATION.md`。
