idf_component_register(
    SRCS "trip_computer.c" "nmea_uart.c" "nmea_parser.c" "ch422g_pwm.c" "ble_gps.c" "waveshare_rgb_lcd_port.c" "main.c" "lvgl_port.c" "ch422g_pwm.c" "bike_ui.c" "ble_gps.c" "nmea_parser.c" "nmea_uart.c" "trip_computer.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash bt esp_timer)
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_link_libraries(${COMPONENT_LIB} PRIVATE m)
 
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)