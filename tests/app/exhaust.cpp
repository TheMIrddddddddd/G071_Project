#include <new>
static void *held;
extern "C" void test_exhaust_ui_pool(void) { held=::operator new(1800); (void)held; }
