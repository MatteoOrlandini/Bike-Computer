/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "waveshare_rgb_lcd_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "bike_ui.h"
#include "ble_gps.h"
#include "nmea_uart.h"
#include "nmea_parser.h"
#include "nmea_data.h"
#include "trip_computer.h"

static void gps_apply_source_state(nmea_source_t source, bool has_valid_fix)
{
    switch (source) {
        case NMEA_SOURCE_UART:
            trip_data_set_ble_status(BLE_STATUS_OFF);
            trip_data_set_uart_status(has_valid_fix ? GPS_UART_STATUS_VALID : GPS_UART_STATUS_INVALID);
            if (has_valid_fix) {
                ESP_LOGD("GPS_TASK", "UART GPS data valid — disabling BLE");
                ble_gps_disable();
            } else {
                ESP_LOGD("GPS_TASK", "UART GPS fix lost — enabling BLE");
                ble_gps_enable();
            }
            break;

        case NMEA_SOURCE_BLE:
            trip_data_set_ble_status(BLE_STATUS_CONNECTED);
            trip_data_set_uart_status(GPS_UART_STATUS_OFF);
            if (has_valid_fix) {
                ESP_LOGD("GPS_TASK", "BLE GPS data valid");
            } else {
                ESP_LOGD("GPS_TASK", "BLE GPS data invalid");
            }
            break;

        default:
            trip_data_set_ble_status(BLE_STATUS_ON);
            trip_data_set_uart_status(GPS_UART_STATUS_OFF);
            ESP_LOGD("GPS_TASK", "No GPS source active — enabling BLE");
            ble_gps_enable();
            break;
    }
}

static void gps_task(void *arg)
{
    float speed = 0.0f;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    int64_t timestamp = INVALID_TIMESTAMP;
    nmea_source_t source = NMEA_SOURCE_NUMBER_OF_SOURCE;

    for (;;) {
        trip_data_set_valid_data(false);

        if (nmea_data_get_time(&hour, &minute, &second, &source, &timestamp)) {
            ESP_LOGD("GPS_TASK", "TIME OK");
            const bool speed_valid = nmea_data_get_speed(&speed, &source, &timestamp);

            if (source == NMEA_SOURCE_UART || source == NMEA_SOURCE_BLE) {
                trip_data_set_valid_data(speed_valid);
                gps_apply_source_state(source, speed_valid);

                if (speed_valid) {
                    trip_computer_update();
                }
            } else {
                gps_apply_source_state(NMEA_SOURCE_NUMBER_OF_SOURCE, false);
            }
        } else if (source < NMEA_SOURCE_NUMBER_OF_SOURCE) {
            gps_apply_source_state(source, false);
        } else {
            gps_apply_source_state(NMEA_SOURCE_NUMBER_OF_SOURCE, false);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void app_main(void)
{
    nmea_data_init();
    ble_gps_init();
    #ifdef TEST_GPS_BAUD_RATE
    baudrate_autoscan_run();
    #endif
    nmea_uart_init();
    trip_computer_init();

    waveshare_esp32_s3_rgb_lcd_init();

    if (lvgl_port_lock(-1)) {
        bike_ui_init();

        lvgl_port_unlock();
    }

    xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL);
}