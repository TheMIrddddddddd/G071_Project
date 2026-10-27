# 动态图标组件与遥控器页面

LAYOUT 页面由 `App/ui/pages/remote_page.cpp` 组合状态栏和摇杆；状态栏在
`App/ui/status_bar/status_bar.cpp` 的 `defaults` 表中设置六个组件。每行依次为
图标类型、屏幕左上角 X、Y、完整循环时间（毫秒）、是否显示、初始相位（毫秒）。
修改配置后重新构建；默认值保持原布局和六种独立动画。

六个图标尺寸均为 20×16，完整显示需要 X 在 0..108、Y 在 0..48。各组件按表中
顺序绘制，允许重叠，重叠时的结果由绘制顺序和绘图方式决定。页面每次重绘完整
画面，OLED 仍通过既有差分刷新发送。

```cpp
// 修改 defaults 中的电池图标：坐标 (100,48)，一轮 8 秒。
{ UI_ICON_BATTERY, 100U, 48U, 8000U, true, 0U },
```

`App/ui/components/animated_icon.h` 提供固定存储的 `ui_icon_t`，可以单独创建
多个同类型实例；调用者负责持有实例并在页面绘图时调用 `ui_icon_draw`。同类
实例共享只读图像数据，但坐标、周期和时间独立，不使用动态分配。周期是整轮
动画的时间，各阶段等比例缩放；值必须大于 0。初始相位按周期取模。

需要在运行中修改默认状态栏组件时，前台可通过 `status_bar_get(type)` 获取实例，
然后使用 `ui_icon_set_position`、`ui_icon_set_period` 或 `ui_icon_set_visible`。
例如（`now_us` 为同一前台上下文的单调微秒时间）：

```cpp
ui_icon_t *battery = status_bar_get(UI_ICON_BATTERY);
if (battery != nullptr) {
    bool moved = ui_icon_set_position(battery, 100, 48);
    bool changed = ui_icon_set_period(battery, 8000, now_us);
    // 调用方应检查 moved 和 changed，再按需要处理错误。
}
```

这些接口仅限前台使用，不从中断调用。位置或周期非法时返回 false，原配置保持
不变；周期更新同时检查时间不得倒退，并保留当前动画进度。移动不重启动画；隐藏
时动画时间仍继续。图标与摇杆相互独立，显示更新仍受现有 40 ms 绘制节奏及实际
I²C 传输耗时限制，短周期可能跳过帧。

现有图标实现和生成的 `.inc` 数据位于 `App/ui/components/icons/`，供组件复用；生成脚本的输出路径已同步更新。
摇杆演示目标位于 `app_joystick_demo.cpp`，模型协调位于 `app_joystick.cpp`，
显示位于 `app_joystick_view.cpp`。当前图标与摇杆仍是演示，不代表接入真实状态。
