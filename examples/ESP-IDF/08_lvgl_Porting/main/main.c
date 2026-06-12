/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "waveshare_rgb_lcd_port.h"
#include "nvs_flash.h"
/* BLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "bike_ui.h" 
#include "freertos/task.h"
#include "ble_gps.h"
#include "ch422g_pwm.h"
#include "ble_gps.h"
#include "nmea_parser.h"


// static const char *TAG = "bike_computer";
// static const char *BLE_DEVICE_NAME = "BikeComputer";
static nmea_data_t s_gps_data;  // lives in main, shared across sources

void app_main()
{
    ble_gps_init();

    waveshare_esp32_s3_rgb_lcd_init();

    // ch422g_pwm_init();          // starts the PWM task at 0% duty
    // ch422g_pwm_set_duty(100);    // call this anytime to change

    /*
    ESP_LOGI(TAG, "Display LVGL demos");
    if (lvgl_port_lock(-1)) {
#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911
        lv_demo_widgets();
#else
        lv_demo_music();
#endif
        lvgl_port_unlock();
    }
    */
    
    ESP_LOGI(TAG, "Starting bike UI");
    if (lvgl_port_lock(-1)) {
        bike_ui_init(&s_gps_data);
        lvgl_port_unlock();
    }
}