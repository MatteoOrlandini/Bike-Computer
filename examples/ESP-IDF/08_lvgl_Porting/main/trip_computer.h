#ifndef _TRIP_COMPUTER_H
#define _TRIP_COMPUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "nmea_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Computed trip statistics, updated by trip_computer_update().
 * Read from any task via trip_computer_get_data().
 */
typedef struct {
    float    speed_kmh;       /* current speed from latest fix          */
    float    avg_speed_kmh;   /* average speed (moving time only)       */
    float    distance_km;     /* total distance travelled               */
    float    gradient_pct;    /* current gradient % (smoothed, 5 fixes) */
    float    ascent_m;        /* cumulative positive altitude gain      */
    uint32_t elapsed_sec;     /* elapsed time since reset (wall clock)  */
    bool     valid;           /* true once at least one fix processed   */
    bool     gps_fix_lost;    /* true when fix has been absent > 2 s   */  // ADD THIS
    uint8_t  ble_status;     // 0=off, 1=on/advertising, 2=connected
    uint8_t  gps_uart_status; // 0=no data, 1=data but not valid, 2=valid <2s ago

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
void trip_computer_update(const nmea_data_t *fix);

/**
 * Copy the latest trip data into *out.
 * Thread-safe. Returns true if at least one fix has been processed.
 */
bool trip_computer_get_data(trip_data_t *out);

/**
 * Reset all trip counters (distance, ascent, elapsed time, averages).
 * Current speed and position are preserved.
 * Thread-safe — safe to call from a UI button callback.
 */
void trip_computer_reset(void);

#ifdef __cplusplus
}
#endif
#endif