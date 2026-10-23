set(U8G2_CSRC "${CMAKE_CURRENT_LIST_DIR}/../../Third_Party/u8g2/csrc")

# Selected upstream drawing sources are vendored; firmware compiles only the
# dependencies below. The extracted font is copied unchanged from oled_test;
# source hashes are in the subset manifest. No generated constructors or allocators.
add_library(oled_u8g2_port STATIC
    u8g2_port.c
    u8g2_font_5x7_tf.c
    ${U8G2_CSRC}/u8g2_bitmap.c
    ${U8G2_CSRC}/u8g2_box.c
    ${U8G2_CSRC}/u8g2_buffer.c
    ${U8G2_CSRC}/u8g2_circle.c
    ${U8G2_CSRC}/u8g2_font.c
    ${U8G2_CSRC}/u8g2_hvline.c
    ${U8G2_CSRC}/u8g2_intersection.c
    ${U8G2_CSRC}/u8g2_kerning.c
    ${U8G2_CSRC}/u8g2_line.c
    ${U8G2_CSRC}/u8g2_ll_hvline.c
    ${U8G2_CSRC}/u8g2_setup.c
    ${U8G2_CSRC}/u8x8_byte.c
    ${U8G2_CSRC}/u8x8_8x8.c
    ${U8G2_CSRC}/u8x8_capture.c
    ${U8G2_CSRC}/u8x8_cad.c
    ${U8G2_CSRC}/u8x8_display.c
    ${U8G2_CSRC}/u8x8_gpio.c
    ${U8G2_CSRC}/u8x8_setup.c
    ${U8G2_CSRC}/u8x8_string.c
    ${U8G2_CSRC}/u8x8_u16toa.c
    ${U8G2_CSRC}/u8x8_d_ssd1306_128x64_noname.c
)
target_include_directories(oled_u8g2_port PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${U8G2_CSRC}
)
target_compile_features(oled_u8g2_port PUBLIC c_std_11)
# Upstream calls externally-owned buffers "dynamic allocation"; this option
# only exposes SetBufferPtr and suppresses hidden statics. No malloc is used.
target_compile_definitions(oled_u8g2_port PUBLIC U8G2_USE_DYNAMIC_ALLOC)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(oled_u8g2_port PRIVATE -ffunction-sections -fdata-sections)
endif()
