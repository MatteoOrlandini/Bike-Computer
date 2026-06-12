#ifndef _BLE_GPS_H
#define _BLE_GPS_H

#include <stdbool.h>
#include "nmea_parser.h"

#ifdef __cplusplus
extern "C" {
#endif
 
/**
 * Initialise the NimBLE stack and start advertising as "BikeGPS".
 * Call once from app_main() before the LVGL loop starts.
 */
void ble_gps_init(void);
 
/**
 * Copy the latest GPS fix into *out.
 * Thread-safe. Returns true if at least one valid fix has been received.
 */
bool ble_gps_get_data(nmea_data_t *out);
 
/**
 * Returns true while a phone is actively connected over BLE.
 * Use this for the BLE > UART priority logic.
 */
bool ble_gps_is_connected(void);
 
#ifdef __cplusplus
}
#endif
 
#endif