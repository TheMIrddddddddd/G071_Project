#include "smooth_animation.h"
#include "smooth_port.h"
#include "smooth_project.hpp"
#include <algorithm>
#include <new>
using namespace smooth_ui_toolkit;
namespace
{
void reset_value(AnimateValue &a, float value)
{
    a.cancel();
    a.springOptions().velocity = 0;
    a.teleport(value);
}
class DemoMenu : public SmoothSelectorMenu
{
    uint32_t action_ = 0;

  public:
    Vector4 frame;
    DemoMenu()
    {
        static const float widths[] = {38, 43, 18, 48}; // STATUS, CONTROL, IO, SETTINGS
        _data.option_list.reserve(4);
        for (int i = 0; i < 4; ++i)
            addOption({{7.0f, 14.0f + 11.0f * i, widths[i], 10.0f}, nullptr});
        setConfig().cameraSize = {0, 0};
        setConfig().readInputInterval = 0;
        setConfig().renderInterval = 0;
        for (AnimateValue *a : {&_data.selector_postion.x, &_data.selector_postion.y,
                                &_data.selector_shape.x, &_data.selector_shape.y})
        {
            a->springOptions().stiffness = 100;
            a->springOptions().damping = 18;
            a->springOptions().restDelta = 0.02f;
            a->springOptions().restSpeed = 0.02f;
        }
    }
    void reset()
    {
        action_ = 0;
        _data.selected_option_index = 0;
        _data.read_input_time_count = 0;
        _data.render_time_count = 0;
        _data.is_changed = false;
        _data.is_pressing = false;
        _data.was_released = false;
        _data.is_opening = false;
        _data.was_opened = false;
        reset_value(_data.selector_postion.x, 7);
        reset_value(_data.selector_postion.y, 14);
        reset_value(_data.selector_shape.x, 38);
        reset_value(_data.selector_shape.y, 10);
        frame = {7, 14, 38, 10};
    }
    void cancel_all()
    {
        for (AnimateValue *a :
             {&_data.selector_postion.x, &_data.selector_postion.y, &_data.selector_shape.x,
              &_data.selector_shape.y, &_data.camera_offset.x, &_data.camera_offset.y})
            a->cancel();
    }
    void onReadInput() override
    {
        static const uint32_t times[] = {800, 1600, 1750, 1900, 2050, 3000, 4100, 5200};
        static const bool next[] = {true, true, true, false, false, true, true, false};
        while (action_ < 8 && SmoothPort_Millis() >= times[action_])
        {
            if (next[action_])
                goNext();
            else
                goLast();
            ++action_;
        }
    }
    void onRender() override
    {
        const auto p = getSelectorPostion().directValue(), s = getSelectorShape().directValue();
        frame = {p.x, p.y, s.x, s.y};
    }
};
struct Model
{
    AnimateVector2 cursor;
    DemoMenu menu;
    Animate curves[4];
    AnimateSequence sequence;
    SmoothSnapshot snapshot{};
    uint64_t scene_start = 0;
    uint32_t target_step = 0, curve_cycle = 0, sequence_cycle = 0;
    bool running = true;
    Model()
    {
        const std::function<float(float)> easing[] = {ease::linear, ease::ease_out_quad,
                                                      ease::ease_in_out_quad, ease::ease_out_back};
        for (int i = 0; i < 4; ++i)
        {
            curves[i].easingOptions().duration = 2.0f;
            curves[i].easingOptions().easingFunction = easing[i];
            curves[i].init();
        }
        sequence.setSequence({0, 1, 2, 3, 4, 5});
        sequence.onPlay(
            [](AnimateValue &a)
            {
                a.easingOptions().duration = 1.4f;
                a.easingOptions().easingFunction = ease::ease_in_out_quad;
            });
        sequence.onStep(
            [](AnimateValue &a, std::vector<float> &, int step)
            {
                static const float durations[] = {0, 1.4f, 1.0f, 0.8f, 1.0f, 1.4f};
                a.easingOptions().duration = durations[step];
            });
        sequence.play();
        sequence.cancel(); // Prewarm its one reusable generator.
    }
};
alignas(Model) unsigned char storage[sizeof(Model)];
Model *model;
constexpr uint32_t durations[] = {12000, 12000, 10000, 12000, 12000};
void start_curves(bool reverse)
{
    for (auto &a : model->curves)
    {
        a.cancel();
        a.start = reverse ? 116.0f : 62.0f;
        a.end = reverse ? 62.0f : 116.0f;
        a.init();
        a.play();
    }
}
} // namespace
void SmoothAnimation_Init(uint64_t now_us)
{
    if (model)
        return;
    SmoothPort_Init(now_us);
    model = new (storage) Model;
    SmoothAnimation_SelectScene(SMOOTH_SPRING_FAST, now_us);
}
void SmoothAnimation_Stop(void)
{
    if (!model)
        return;
    model->running = false;
    model->cursor.x.cancel();
    model->cursor.y.cancel();
    model->menu.cancel_all();
    for (auto &a : model->curves)
        a.cancel();
    model->sequence.cancel();
}
void SmoothAnimation_SelectScene(SmoothScene scene, uint64_t now_us)
{
    if (!model || static_cast<unsigned>(scene) >= SMOOTH_SCENE_COUNT)
        return;
    SmoothAnimation_Stop();
    SmoothPort_ResetEpoch(now_us);
    model->scene_start = now_us;
    model->running = true;
    model->target_step = 0;
    model->curve_cycle = 0;
    model->sequence_cycle = 0;
    model->snapshot = {};
    model->snapshot.scene = scene;
    if (scene == SMOOTH_SPRING_FAST || scene == SMOOTH_SPRING_BOUNCE)
    {
        for (AnimateValue *a : {&model->cursor.x, &model->cursor.y})
        {
            auto &options = a->springOptions();
            options.stiffness = scene == SMOOTH_SPRING_FAST ? 100.0f : 64.0f;
            options.damping = scene == SMOOTH_SPRING_FAST ? 20.0f : 10.0f;
            options.restSpeed = 0.02f;
            options.restDelta = 0.02f;
        }
        reset_value(model->cursor.x, 28);
        reset_value(model->cursor.y, 37);
        model->snapshot.target_x = 28;
        model->snapshot.target_y = 37;
    }
    else if (scene == SMOOTH_MENU)
        model->menu.reset();
    else if (scene == SMOOTH_EASING)
        start_curves(false);
    else if (scene == SMOOTH_SEQUENCE)
        model->sequence.play();
    SmoothAnimation_Update(now_us);
}
void SmoothAnimation_Update(uint64_t now_us)
{
    if (!model || !model->running)
        return;
    auto &s = model->snapshot;
    if (now_us - model->scene_start >= static_cast<uint64_t>(durations[s.scene]) * 1000U)
    {
        SmoothAnimation_SelectScene(static_cast<SmoothScene>((s.scene + 1) % SMOOTH_SCENE_COUNT),
                                    now_us);
        return;
    }
    s.scene_ms = SmoothPort_Sample(now_us);
    if (s.scene <= SMOOTH_SPRING_BOUNCE)
    {
        static const uint32_t times[] = {1000, 1150, 3000, 5000};
        static const Vector2 targets[] = {{100, 37}, {28, 37}, {100, 22}, {64, 37}};
        while (model->target_step < 4 && s.scene_ms >= times[model->target_step])
        {
            auto target = targets[model->target_step++];
            model->cursor.move(target);
            s.target_x = target.x;
            s.target_y = target.y;
        }
        model->cursor.update(s.scene_ms / 1000.0f);
        auto current = model->cursor.directValue();
        s.current_x = current.x;
        s.current_y = current.y;
        s.stage = model->target_step;
    }
    else if (s.scene == SMOOTH_MENU)
    {
        model->menu.update(s.scene_ms);
        auto f = model->menu.frame;
        s.current_x = f.x;
        s.current_y = f.y;
        s.width = f.width;
        s.height = f.height;
        auto target = model->menu.getSelectedKeyframe();
        s.target_x = target.x;
        s.target_y = target.y;
        s.stage = static_cast<uint32_t>(model->menu.getSelectedOptionIndex());
    }
    else if (s.scene == SMOOTH_EASING)
    {
        uint32_t cycle = s.scene_ms / 3000U;
        if (cycle != model->curve_cycle)
        {
            model->curve_cycle = cycle;
            start_curves((cycle & 1U) != 0);
        }
        for (int i = 0; i < 4; ++i)
        {
            model->curves[i].update(s.scene_ms / 1000.0f);
            s.lanes[i] = model->curves[i].value();
        }
        s.current_x = s.lanes[0];
        s.target_x = model->curves[0].end;
        s.stage = cycle & 1U;
    }
    else
    {
        uint32_t cycle = s.scene_ms / 8000U;
        if (cycle != model->sequence_cycle)
        {
            model->sequence_cycle = cycle;
            model->sequence.play();
        }
        model->sequence.update();
        float v = std::clamp(model->sequence.directValue(), 0.0f, 5.0f);
        s.stage = static_cast<uint32_t>(std::min(v, 4.0f));
        s.current_x = 18 + 66 * std::clamp(v, 0.0f, 1.0f) - 66 * std::clamp(v - 4, 0.0f, 1.0f);
        // Keep the moving solid block inside SH1106 page 4 (rows 32..39).
        // Serial writes still aren't an atomic panel flip, but the block no
        // longer has three vertically separated page writes at different times.
        s.current_y = 32;
        s.width = 8 + 22 * std::clamp(v - 1, 0.0f, 1.0f) - 22 * std::clamp(v - 3, 0.0f, 1.0f);
        s.height = 8;
        s.target_x = s.stage == 4 ? 18 : 84;
        s.target_y = 32;
    }
}
void SmoothAnimation_Snapshot(SmoothSnapshot *out)
{
    if (model && out)
        *out = model->snapshot;
}
const char *SmoothAnimation_SceneName(SmoothScene scene)
{
    static const char *names[] = {"spring.fast", "spring.bounce", "menu", "easing", "sequence"};
    return static_cast<unsigned>(scene) < SMOOTH_SCENE_COUNT ? names[scene] : "none";
}
