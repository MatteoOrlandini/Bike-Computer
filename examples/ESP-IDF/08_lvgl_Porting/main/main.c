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

static void gps_task(void *arg)
{
    float speed = 0.0f;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    int64_t last_valid_data_timestamp = INVALID_TIMESTAMP;
    int64_t timestamp = INVALID_TIMESTAMP;
    nmea_source_t source = NMEA_SOURCE_NUMBER_OF_SOURCE;

    for (;;) {
        trip_data_set_valid_data(false);
        //int64_t now_us = esp_timer_get_time();

        /* Data valid? */
        if (nmea_data_get_time(&hour, &minute, &second, &source, &timestamp))
        {
            ESP_LOGI("GPS_TASK", "TIME OK");
            if (source == NMEA_SOURCE_UART)
            {
                if (nmea_data_get_speed(&speed, &source, &timestamp))
                {
                    ESP_LOGI("GPS_TASK", "SPEED OK");
                    trip_data_set_valid_data(true);
                    /* UART held valid fixes for >2 s — promote it, kill BLE */
                    ESP_LOGI("GPS_TASK", "UART GPS data valid — disabling BLE");
                    trip_data_set_uart_status(GPS_UART_STATUS_VALID);
                }
                else
                {
                    /* UART held valid fixes for >2 s — promote it, kill BLE */
                    ESP_LOGI("GPS_TASK", "UART GPS data invalid — disabling BLE");
                    trip_data_set_uart_status(GPS_UART_STATUS_INVALID);
                }
                trip_data_set_ble_status(BLE_STATUS_OFF);
                ble_gps_disable();
            }
            else if (source == NMEA_SOURCE_BLE)
            {
                if (nmea_data_get_speed(&speed, &source, &timestamp))
                {
                    trip_data_set_valid_data(true);
                    ESP_LOGI("GPS_TASK", "BLE GPS data valid");
                }
                else
                {
                    ESP_LOGI("GPS_TASK", "BLE GPS data invalid");
                }
                trip_data_set_ble_status(BLE_STATUS_CONNECTED);
                trip_data_set_uart_status(GPS_UART_STATUS_OFF);
            }
            else
            {
                trip_data_set_ble_status(BLE_STATUS_ON);
                trip_data_set_uart_status(GPS_UART_STATUS_INVALID);
                ESP_LOGI("GPS_TASK", "UART GPS fix lost — enabling BLE");
                ble_gps_enable();
            }
        }
        /* Data not valid but received from UART or BLE */
        else if (source < NMEA_SOURCE_NUMBER_OF_SOURCE)
        {
            if (source == NMEA_SOURCE_UART)
            {
                trip_data_set_ble_status(BLE_STATUS_ON);
                trip_data_set_uart_status(GPS_UART_STATUS_INVALID);
                ESP_LOGI("GPS_TASK", "UART GPS fix lost — enabling BLE");
                ble_gps_enable();
            }
            else if (source == NMEA_SOURCE_BLE)
            {
                trip_data_set_ble_status(BLE_STATUS_CONNECTED);
                trip_data_set_uart_status(GPS_UART_STATUS_OFF);
            }
            else
            {
                /* TO DO */
            }
        }
        /* No data received from UART or BLE*/
        else
        {
            trip_data_set_ble_status(BLE_STATUS_ON);
            trip_data_set_uart_status(GPS_UART_STATUS_OFF);
            ESP_LOGI("GPS_TASK", "UART GPS fix lost — enabling BLE");
            ble_gps_enable();
        }

        if (trip_data_get_valid_data())
        {
            last_valid_data_timestamp = timestamp;
        }
        /*
        @TODO: update ui in another task
        */
        trip_computer_update();
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