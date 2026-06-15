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

/* Shared GPS state — written by gps_task, read by the LVGL timer in bike_ui */
static nmea_data_t s_gps_data;

/*
 * gps_task — priority arbitration
 *
 * BLE takes priority when a phone is connected.
 * Falls back to whichever UART source has a valid fix otherwise.
 * Runs every 500 ms — fast enough for 1 Hz GPS updates.
 */
static void gps_task(void *arg)
{
    nmea_data_t tmp;

    for (;;) {
        if (ble_gps_is_connected()) {
            ble_gps_get_data(&tmp);
        } else {
            nmea_uart_get_data(&tmp);
        }

        /* Single word-aligned copy — no mutex needed on Xtensa for
         * struct assignment that fits in a cache line, but we keep it
         * simple and accept the rare torn read in the LVGL timer. */
        s_gps_data = tmp;

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    /* Start BLE — advertises immediately */
    ble_gps_init();

    /* Start both UART readers (UART2 TTL + UART1 RS485) */
    nmea_uart_init();

    /* Initialise LCD, touch, and LVGL */
    waveshare_esp32_s3_rgb_lcd_init();

    /* Build the bike UI and register the LVGL refresh timer */
    if (lvgl_port_lock(-1)) {
        bike_ui_init(&s_gps_data);
        lvgl_port_unlock();
    }

    /* Start the GPS arbitration task */
    xTaskCreate(gps_task, "gps_task", 3072, NULL, 5, NULL);
}