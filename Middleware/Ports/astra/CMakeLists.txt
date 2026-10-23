set(ASTRA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../Third_Party/astra/Core/Src")
add_library(oled_astra STATIC
    ${ASTRA_ROOT}/hal/hal.cpp
    ${ASTRA_ROOT}/astra/ui/item/menu/menu.cpp
    ${ASTRA_ROOT}/astra/ui/item/selector/selector.cpp
    ${ASTRA_ROOT}/astra/ui/item/camera/camera.cpp
    astra_port.cpp)
target_include_directories(oled_astra PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_include_directories(oled_astra SYSTEM PUBLIC "${ASTRA_ROOT}")
target_link_libraries(oled_astra PUBLIC oled_u8g2_port)
target_compile_features(oled_astra PUBLIC cxx_std_17)
target_compile_options(oled_astra PRIVATE -Wall -Wextra -Wno-unused-parameter
    -Wno-unused-variable -Wno-unused-function -Wno-type-limits)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(oled_astra PRIVATE -Wno-bitwise-instead-of-logical)
endif()
