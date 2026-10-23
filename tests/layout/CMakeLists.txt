cmake_minimum_required(VERSION 3.22)
project(g071_layout_tests LANGUAGES C CXX)
enable_testing()
set(REPO "${CMAKE_CURRENT_LIST_DIR}/../..")
add_subdirectory("${REPO}/Middleware/Ports/u8g2" u8g2)
add_subdirectory("${REPO}/Middleware/Ports/smooth_ui_toolkit" smooth)
add_executable(test_layout test_layout.cpp "${REPO}/App/app_ui.cpp"
 "${REPO}/App/app_light_icon.cpp"
 "${REPO}/App/app_lock_icon.cpp"
 "${REPO}/App/app_fast_icon.cpp"
 "${REPO}/App/app_hand_wheel_icon.cpp"
 "${REPO}/App/app_wiper_icon.cpp"
 "${REPO}/App/app_battery_icon.cpp"
 "${REPO}/App/app_joystick.cpp"
 "${REPO}/Middleware/OledRefresh/oled_refresh.c")
target_include_directories(test_layout PRIVATE "${REPO}/App" "${REPO}/Middleware/OledRefresh")
target_compile_definitions(test_layout PRIVATE APP_SHOWCASE=0 APP_LAYOUT=1)
target_compile_options(test_layout PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_layout PRIVATE oled_u8g2_port oled_joystick)
add_test(NAME status_animations COMMAND test_layout)
add_executable(test_joysticks test_joysticks.cpp "${REPO}/App/app_joystick.cpp")
target_include_directories(test_joysticks PRIVATE "${REPO}/App")
target_compile_options(test_joysticks PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_joysticks PRIVATE oled_joystick oled_u8g2_port)
foreach(scenario geometry critical repeat retarget cadence guards bounds demo idle long_time)
 add_test(NAME joystick_${scenario} COMMAND test_joysticks ${scenario})
endforeach()
