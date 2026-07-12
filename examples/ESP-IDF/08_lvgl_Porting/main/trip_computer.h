#ifndef _TRIP_COMPUTER_H
#define _TRIP_COMPUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "nmea_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BLE_STATUS_OFF = 0,
    BLE_STATUS_ON,           /* advertising, no client */
    BLE_STATUS_CONNECTED
} gps_ble_status_t;

typedef enum {
    GPS_UART_STATUS_OFF = 0, /* no sentences arriving at all */
    GPS_UART_STATUS_INVALID, /* sentences arriving but fix void */
    GPS_UART_STATUS_VALID    /* valid fix within last 2 s */
} gps_uart_status_t;

typedef struct {
    gps_ble_status_t  gps_ble;
    gps_uart_status_t gps_uart;
} gps_status_t;

/**
 * Computed trip statistics, updated by trip_computer_update().
 * Read from any task via trip_computer_get_data().
 */
typedef struct {
    double  last_lat;
    double  last_lon;
    double  last_alt;
    double  last_speed;
    float    speed_kmh;       /* current speed from latest fix          */
    float    avg_speed_kmh;   /* average speed (moving time only)       */
    float    distance_km;     /* total distance travelled               */
    float    gradient_pct;    /* current gradient % (smoothed, 5 fixes) */
    float    ascent_m;        /* cumulative positive altitude gain      */
    uint32_t elapsed_sec;     /* elapsed time since reset (wall clock)  */
    gps_status_t  gps_status;  
    bool valid_data;          /* true if data is valid */

} trip_data_t;

/**
 * Initialise the trip computer. Resets all counters.
 * Call once from app_main() after nmea_uart_init() and ble_gps_init().
 */
void trip_computer_init(void);

/**
 * Feed a new GPS fix into the trip computer.
 * Call from gps_task() every time s_gps_data is updated.
 * Thread-safe.
 */
void trip_computer_update(void);

/**
 * Reset all trip counters (distance, ascent, elapsed time, averages).
 * Current speed and position are preserved.
 * Thread-safe — safe to call from a UI button callback.
 */
void trip_computer_reset(void);

void trip_data_set_ble_status (gps_ble_status_t state);

void trip_data_set_uart_status (gps_uart_status_t state);

gps_ble_status_t trip_data_get_ble_status (void);

gps_uart_status_t trip_data_get_uart_status (void);

void trip_data_set_valid_data (bool value);

bool trip_data_get_valid_data (void);

#ifdef __cplusplus
}
#endif
#endif