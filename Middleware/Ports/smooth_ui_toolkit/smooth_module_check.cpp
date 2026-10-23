#include "smooth_project.hpp"
#include "tools/fpm/ios.hpp"
#include <cmath>
#include <cstdio>
#include <sstream>
using namespace smooth_ui_toolkit;
namespace
{
class DvdCheck : public games::dvd_screensaver::DvdScreensaver
{
  public:
    int renders = 0, collisions = 0;
    void onBuildLevel() override
    {
        addScreenFrameAsWall({128, 64});
        addLogo(1, {64, 32}, {8, 4}, {1, 1}, 40);
    }
    void onRender(float) override { ++renders; }
    void onLogoCollide(int) override { ++collisions; }
};
class BreakoutCheck : public games::breakout::Breakout
{
  public:
    int renders = 0, collisions = 0;
    void onBuildLevel() override
    {
        addWall({64, 0}, {128, 1});
        addWall({0, 32}, {1, 64});
        addWall({128, 32}, {1, 64});
        addPaddle({64, 58}, {24, 4}, 40, {12, 116});
        addBall({64, 50}, 2, 30);
        addBrick({64, 20}, {20, 5});
        _screen_height = 64;
    }
    void onInit() override { resetBall(); }
    bool onReadAction(games::breakout::Action action) override
    {
        return action == games::breakout::Action::Fire;
    }
    void onRender(float) override { ++renders; }
    void onBallCollide(games::breakout::Group) override { ++collisions; }
};
} // namespace
// Optional foreground diagnostic, with real operations. Not called in the demo's
// sealed allocation phase. Building this TU checks all retained header modules.
bool SmoothModule_Check()
{
    auto bezier = cubic_bezier(0.25f, 0.1f, 0.25f, 1.0f);
    if (bezier(0) != 0 || bezier(1) != 1 || !std::isfinite(bezier(0.5f)))
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    RingBuffer<int, 3> ring;
    ring.push(1);
    ring.push(2);
    ring.push(3);
    ring.push(4);
    if (ring.front() != 2 || ring.back() != 4)
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    int sum = 0;
    ring.popAll([&](int &v) { sum += v; });
    if (sum != 9 || !ring.empty())
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    Signal<int> signal;
    auto id = signal.connect([&](int v) { sum = v; });
    signal.emit(7);
    signal.disconnect(id);
    signal.emit(8);
    if (sum != 7)
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    EventQueue<int> queue;
    queue.emit(5);
    queue.poll([&](const int &v) { sum = v; });
    if (sum != 5 || !queue.empty())
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    auto callback = [](void *ctx, const int &value) { *static_cast<int *>(ctx) = value; };
    Observable<int> value(1);
    auto observer = value.observe(&sum, callback);
    value.set(9);
    if (sum != 9)
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    value.removeObserver(observer);
    value.set(3);
    if (sum != 9)
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    SingleObservable<int> single(2);
    single.observe(&sum, callback);
    single.set(8);
    if (sum != 8)
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    single.removeObserver();
    ObjectPool<Poolable> pool;
    int first = pool.create(std::make_unique<Poolable>());
    pool.get(first)->requestDestroy();
    pool.cleanup();
    if (pool.get(first))
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    if (pool.create(std::make_unique<Poolable>()) != first)
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    RingObjectPool<Poolable> fixed(2);
    fixed.create(std::make_unique<Poolable>());
    fixed.create(std::make_unique<Poolable>());
    if (fixed.create(std::make_unique<Poolable>()) != 0)
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    auto &random = Random::getInstance();
    for (int i = 0; i < 100; ++i)
    {
        int n = random.getInt(-3, 3);
        if (n < -3 || n > 3)
            return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    }
    std::vector<int> choices{1, 2};
    if (random.choice(choices) < 1)
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    if (random.getFloat(1, 1) != 1 || random.getDouble(2, 2) != 2)
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    fpm::fixed_16_16 f(2);
    auto result = fpm::sqrt(f * f);
    if (static_cast<int>(result) != 2)
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    std::ostringstream text;
    text << result;
    if (text.str().empty())
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    SmoothOptionsMenu options;
    options.addOption();
    options.setLastKeyframe({0, 0, 20, 10});
    options.addOption();
    options.setLastKeyframe({0, 12, 30, 10});
    options.goNext();
    options.update(10);
    if (options.getOptionList().size() != 2 || options.getSelectedOptionIndex() < 0)
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    uitk::Vector2 vector(3, 4);
    if (std::abs(vector.length() - 5) > 0.001f)
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    Vector4 rectangle(1, 2, 10, 6);
    auto anchor = shape::get_anchor_offset<float>(rectangle, shape::Anchor::Center);
    if (anchor.x != 5 || anchor.y != 3)
        return false;
    uint32_t tick = 0;
    ui_hal::on_get_tick([&] { return tick; });
    DvdCheck dvd;
    dvd.init();
    BreakoutCheck breakout;
    breakout.init();
    for (tick = 10; tick <= 3000; tick += 10)
    {
        dvd.update();
        breakout.update();
    }
    std::printf("games dvd render=%d collision=%d breakout render=%d collision=%d\n", dvd.renders,
                dvd.collisions, breakout.renders, breakout.collisions);
    if (dvd.renders != 300 || breakout.renders != 300 || dvd.collisions == 0 ||
        breakout.collisions == 0)
        return (std::fprintf(stderr, "module check failed at %d\n", __LINE__), false);
    ui_hal::on_get_tick([] { return 0U; });
    return true;
}
