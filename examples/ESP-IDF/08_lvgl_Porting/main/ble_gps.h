#ifndef _BLE_GPS_H
#define _BLE_GPS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * GPS data received from the smartphone over BLE.
 * Fields are updated whenever the phone writes new data.
 */
typedef struct {
    double  latitude;    // degrees, e.g. 43.1234
    double  longitude;   // degrees, e.g. 12.5678
    float   speed_kmh;   // km/h
    float   altitude_m;  // metres
    bool    valid;       // true once at least one fix has been received
} ble_gps_data_t;

/**
 * Initialise the NimBLE stack and register the GPS GATT service.
 * Call once from app_main(), before the LVGL loop starts.
 */
void ble_gps_init(void);

/**
 * Copy the latest GPS fix into *out.
 * Thread-safe (uses a mutex internally).
 * Returns true if a valid fix is available, false otherwise.
 */
bool ble_gps_get_data(ble_gps_data_t *out);

#ifdef __cplusplus
}
#endif
#endif