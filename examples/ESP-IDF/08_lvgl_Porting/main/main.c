/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "waveshare_rgb_lcd_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bike_ui.h"
#include "ble_gps.h"
#include "nmea_uart.h"
#include "nmea_parser.h"
#include "trip_computer.h"

static nmea_data_t s_gps_data;
static trip_data_t s_trip_data;

static void gps_task(void *arg)
{
    nmea_data_t tmp;

    for (;;) {
        if (ble_gps_is_connected()) {
            ble_gps_get_data(&tmp);
        } else {
            nmea_uart_get_data(&tmp);
        }

        s_gps_data = tmp;

        /* Feed the trip computer on every valid fix */
        trip_computer_update(&tmp);
        trip_computer_get_data(&s_trip_data);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    ble_gps_init();
    nmea_uart_init();
    trip_computer_init();

    waveshare_esp32_s3_rgb_lcd_init();

    if (lvgl_port_lock(-1)) {
        bike_ui_init(&s_trip_data);
        lvgl_port_unlock();
    }

    xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL);
}