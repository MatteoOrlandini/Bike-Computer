#ifndef _BIKE_UI_H_
#define _BIKE_UI_H_

#include "nmea_parser.h"
#include "trip_computer.h"

typedef enum {
    BLE_STATUS_OFF = 0,
    BLE_STATUS_ON,           /* advertising, no client */
    BLE_STATUS_CONNECTED
} ble_status_t;

typedef enum {
    GPS_UART_STATUS_OFF = 0, /* no sentences arriving at all */
    GPS_UART_STATUS_INVALID, /* sentences arriving but fix void */
    GPS_UART_STATUS_VALID    /* valid fix within last 2 s */
} gps_uart_status_t;

typedef struct {
    ble_status_t      ble;
    gps_uart_status_t gps_uart;
} ui_status_t;

typedef struct {
    trip_data_t  *trip;
    ui_status_t  *status;
} ui_timer_data_t;

void bike_ui_init(trip_data_t *data, ui_status_t *status);
void bike_ui_update(const trip_data_t *data, const ui_status_t *status);

#endif