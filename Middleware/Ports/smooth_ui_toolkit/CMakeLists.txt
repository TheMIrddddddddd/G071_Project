set(SMOOTH_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../Third_Party/smooth_ui_toolkit/src")
# Every retained upstream translation unit is compiled, even if firmware GC
# later discards unused code. The only excluded directories are absent on disk.
file(GLOB_RECURSE SMOOTH_UPSTREAM_SOURCES CONFIGURE_DEPENDS "${SMOOTH_ROOT}/*.cpp")
add_library(oled_smooth_upstream STATIC ${SMOOTH_UPSTREAM_SOURCES})
target_include_directories(oled_smooth_upstream SYSTEM PUBLIC "${SMOOTH_ROOT}")
target_include_directories(oled_smooth_upstream PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_compile_features(oled_smooth_upstream PUBLIC cxx_std_17)
target_compile_options(oled_smooth_upstream PUBLIC
    "$<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_LIST_DIR}/smooth_config.hpp>")
target_compile_definitions(oled_smooth_upstream PUBLIC
    SMOOTH_UI_TOOLKIT_ENABLE_DEFAULT_HAL=0 SMOOTH_UI_TOOLKIT_ENABLE_COLOR=0
    SMOOTH_UI_TOOLKIT_SINGLE_CONTEXT=1 SMOOTH_UI_TOOLKIT_RANDOM_SEED=0x4761106U
    _USE_MATH_DEFINES)
add_library(oled_smooth_port STATIC smooth_port.cpp smooth_animation.cpp smooth_module_check.cpp)
target_link_libraries(oled_smooth_port PUBLIC oled_smooth_upstream)
add_library(oled_joystick STATIC joystick_follow.cpp joystick_draw.cpp joystick_input_draw.cpp remote_panel_draw.cpp)
target_include_directories(oled_joystick PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_compile_features(oled_joystick PUBLIC cxx_std_17)
target_link_libraries(oled_joystick PUBLIC oled_u8g2_port oled_smooth_upstream)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(oled_smooth_upstream PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare)
    target_compile_options(oled_smooth_port PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare)
    target_compile_options(oled_joystick PRIVATE -Wall -Wextra -Werror)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # The pinned CubicBezier implementation captures two unused control points.
    target_compile_options(oled_smooth_upstream PRIVATE -Wno-unused-lambda-capture)
endif()
