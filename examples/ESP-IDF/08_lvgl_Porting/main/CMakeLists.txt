idf_component_register(
    SRCS "nmea_parser.c" "ch422g_pwm.c" "ble_gps.c" "waveshare_rgb_lcd_port.c" "main.c" "lvgl_port.c" "ch422g_pwm.c" "bike_ui.c" "ble_gps.c" 
    INCLUDE_DIRS "."
    REQUIRES nvs_flash bt)
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)