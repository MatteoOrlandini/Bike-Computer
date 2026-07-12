/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "waveshare_rgb_lcd_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "bike_ui.h"
#include "ble_gps.h"
#include "nmea_uart.h"
#include "nmea_parser.h"
#include "trip_computer.h"

#define UART_PROMOTE_US   2000000LL   /* 2 s of consecutive valid UART fixes to take over */


static nmea_data_t s_gps_data;
static trip_data_t s_trip_data;
static ui_status_t s_ui_status;

static void gps_task(void *arg)
{
    nmea_data_t  tmp;
    int64_t      uart_valid_since_us = 0;   /* tick when UART streak started  */
    bool         uart_promoted       = false; /* UART is primary, BLE is off  */

    for (;;) {
        /* --- Check UART fix --- */
        nmea_data_t uart_tmp;
        bool uart_valid = nmea_uart_get_data(&uart_tmp) && uart_tmp.valid;

        int64_t now_us = esp_timer_get_time();

        if (uart_valid) {
            if (uart_valid_since_us == 0) {
                uart_valid_since_us = now_us;   /* start streak timer */
            }

            if (!uart_promoted &&
                (now_us - uart_valid_since_us) >= UART_PROMOTE_US) {
                /* UART held valid fixes for >2 s — promote it, kill BLE */
                ESP_LOGI("GPS_TASK", "UART GPS promoted — disabling BLE");
                ble_gps_disable();
                uart_promoted = true;
            }
        } else {
            /* UART fix lost — reset streak, re-enable BLE if needed */
            if (uart_valid_since_us != 0) {
                ESP_LOGI("GPS_TASK", "UART GPS fix lost — enabling BLE");
            }
            uart_valid_since_us = 0;

            if (uart_promoted) {
                ble_gps_enable();
                uart_promoted = false;
            }
        }
        
        if (!ble_gps_is_running()) {   // you'll need ble_gps_is_running() or track locally
            s_ui_status.ble = BLE_STATUS_OFF;
        } else if (ble_gps_is_connected()) {
            s_ui_status.ble = BLE_STATUS_CONNECTED;
        } else {
            s_ui_status.ble = BLE_STATUS_ON;
        }

        if (!uart_valid) {
            s_ui_status.gps_uart = GPS_UART_STATUS_OFF;
        } else if (!uart_tmp.valid) {
            s_ui_status.gps_uart = GPS_UART_STATUS_INVALID;
        } else {
            s_ui_status.gps_uart = GPS_UART_STATUS_VALID;
        }

        /* --- Select source --- */
        if (uart_promoted) {
            tmp = uart_tmp;
        } else if (ble_gps_is_connected()) {
            ble_gps_get_data(&tmp);
        } else {
            /* Neither UART promoted nor BLE connected — use UART anyway
               (may be invalid, trip_computer handles that gracefully)  */
            tmp = uart_tmp;
        }

        s_gps_data = tmp;
        trip_computer_update(&tmp);
        trip_computer_get_data(&s_trip_data);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void app_main(void)
{
    ble_gps_init();
    #ifdef TEST_GPS_BAUD_RATE
    baudrate_autoscan_run();
    #endif
    nmea_uart_init();
    trip_computer_init();

    waveshare_esp32_s3_rgb_lcd_init();

    if (lvgl_port_lock(-1)) {
        bike_ui_init(&s_trip_data, &s_ui_status);

        lvgl_port_unlock();
    }

    xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL);
}