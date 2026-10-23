cmake_minimum_required(VERSION 3.22)
project(source_showcase_reference LANGUAGES C CXX)
set(SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../../oled_test")
add_subdirectory("${SOURCE}/UserMiddlewares/Ports/u8g2" u8g2)
add_subdirectory("${SOURCE}/UserMiddlewares/Ports/smooth_ui_toolkit" smooth)
add_subdirectory("${SOURCE}/UserMiddlewares/Ports/astra" astra)
add_executable(test_reference "${SOURCE}/tests/showcase/test_showcase.cpp"
 "${SOURCE}/UserMiddlewares/Ports/showcase/showcase_model.cpp"
 "${SOURCE}/UserMiddlewares/Ports/smooth_ui_toolkit/smooth_memory.cpp")
target_include_directories(test_reference PRIVATE "${SOURCE}/UserMiddlewares/Ports/showcase")
target_link_libraries(test_reference PRIVATE oled_smooth_upstream oled_u8g2_port oled_astra)
