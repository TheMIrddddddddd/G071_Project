cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME g071_test)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C CXX ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)
add_subdirectory(Middleware/Ports/u8g2)
set(APP_DISPLAY_MODE "SHOWCASE" CACHE STRING "SHOWCASE or STATUS")
set_property(CACHE APP_DISPLAY_MODE PROPERTY STRINGS SHOWCASE STATUS)
if(APP_DISPLAY_MODE STREQUAL "SHOWCASE")
    add_subdirectory(Middleware/Ports/smooth_ui_toolkit)
    add_subdirectory(Middleware/Ports/astra)
    add_subdirectory(Middleware/Ports/showcase)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE Middleware/Ports/smooth_ui_toolkit/smooth_memory.cpp)
    target_link_libraries(${CMAKE_PROJECT_NAME} oled_showcase)
    foreach(ui_target oled_astra oled_smooth_upstream oled_showcase)
        target_compile_options(${ui_target} PRIVATE -fexceptions)
    endforeach()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE APP_SHOWCASE=1)
elseif(APP_DISPLAY_MODE STREQUAL "STATUS")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE APP_SHOWCASE=0)
else()
    message(FATAL_ERROR "APP_DISPLAY_MODE must be SHOWCASE or STATUS")
endif()
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fexceptions;-Wall;-Wextra;-Werror>")

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    BSP/board_usart.c
    BSP/board_soft_i2c.c
    BSP/board_tca6507.c
    BSP/board_oled.c
    BSP/board_time.c
    App/app_main.c
    App/app_bus.c
    App/app_rgb.c
    App/app_display.c
    App/app_ui.cpp
    Middleware/OledRefresh/oled_refresh.c
    Middleware/Ringbuffer/src/ringbuffer.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    BSP
    App
    Middleware/OledRefresh
    Middleware/Ringbuffer/inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    oled_u8g2_port

    # Add user defined libraries
)
