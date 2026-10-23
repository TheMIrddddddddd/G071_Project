cmake_minimum_required(VERSION 3.22)
project(g071_app_tests LANGUAGES C CXX)
enable_testing()
set(REPO "${CMAKE_CURRENT_LIST_DIR}/../..")
add_subdirectory("${REPO}/Middleware/Ports/u8g2" u8g2)
add_subdirectory("${REPO}/Middleware/Ports/smooth_ui_toolkit" smooth)
add_subdirectory("${REPO}/Middleware/Ports/astra" astra)
add_subdirectory("${REPO}/Middleware/Ports/showcase" showcase)
add_executable(test_app test_app.c exhaust.cpp
 "${REPO}/App/app_main.c" "${REPO}/App/app_bus.c" "${REPO}/App/app_rgb.c"
 "${REPO}/App/app_display.c" "${REPO}/App/app_ui.cpp"
 "${REPO}/BSP/board_oled.c" "${REPO}/BSP/board_tca6507.c"
 "${REPO}/Middleware/OledRefresh/oled_refresh.c"
 "${REPO}/Middleware/Ports/smooth_ui_toolkit/smooth_memory.cpp")
target_include_directories(test_app PRIVATE fakes "${REPO}/App" "${REPO}/BSP" "${REPO}/Middleware/OledRefresh")
target_compile_definitions(test_app PRIVATE APP_SHOWCASE=1)
target_compile_options(test_app PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_app PRIVATE oled_showcase)
foreach(scenario normal offline partial stuck rgb memory rgb_offline)
 add_test(NAME app_${scenario} COMMAND test_app ${scenario})
endforeach()
