/*
 * trip_computer.c
 *
 * Computes cycling trip statistics from consecutive GPS fixes:
 *   - Distance      : Haversine formula between consecutive positions
 *   - Gradient      : (Δalt / distance) * 100, smoothed over 5 fixes
 *   - Ascent        : cumulative positive altitude gain (>2 m threshold)
 *   - Average speed : distance / moving time (speed > 4 km/h only)
 *   - Elapsed time  : wall-clock seconds since last reset
 *
 * Tunable constants are grouped at the top.
 */

#include "trip_computer.h"
#include "nmea_data.h"
#include "bike_ui.h"

#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "TRIP"

#define FIX_TIMEOUT_US  2000000LL   /* 2 seconds in microseconds */

/* ------------------------------------------------------------------ */
/*  Tunable constants                                                   */
/* ------------------------------------------------------------------ */

#define MIN_SPEED_KMH        0.0f   /* ignore updates below this speed  */
#define MIN_ALT_CHANGE_M     0.0f   /* ignore altitude changes below this */
#define GRADIENT_WINDOW      5      /* number of fixes to smooth gradient */
#define EARTH_RADIUS_M       6371000.0  /* mean Earth radius, metres     */

/* ------------------------------------------------------------------ */
/*  Internal state                                                      */
/* ------------------------------------------------------------------ */

static trip_data_t      trip_data;
static SemaphoreHandle_t s_mutex;

/* ------------------------------------------------------------------ */
/*  Haversine distance (metres)                                         */
/* ------------------------------------------------------------------ */

static float haversine_m(double lat1, double lon1, double lat2, double lon2)
{
    return 0.0;
}

/* ------------------------------------------------------------------ */
/*  Gradient calculation over smoothing window                          */
/* ------------------------------------------------------------------ */

static float compute_smoothed_gradient(void)
{
    return 0.0;
}

/* ------------------------------------------------------------------ */
/*  Internal reset (call with mutex held)                               */
/* ------------------------------------------------------------------ */

static void reset_locked(void)
{
    /* Preserve last known position and validity */
    double last_lat  = trip_data.last_lat;
    double last_lon  = trip_data.last_lon;
    float  last_alt  = trip_data.last_alt;

    memset(&trip_data, 0, sizeof(trip_data));

    trip_data.last_lat   = last_lat;
    trip_data.last_lon   = last_lon;
    trip_data.last_alt  = last_alt;

    ESP_LOGI(TAG, "Trip reset");
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void trip_computer_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    assert(s_mutex);
    memset(&trip_data, 0, sizeof(trip_data));
    ESP_LOGI(TAG, "Trip computer initialised");
}

void trip_computer_update(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    double latitude;
    double longitude;
    float speed;
    double altitude;
    nmea_source_t source;
    int64_t timestamp;

    if (trip_data_get_valid_data())
    {
        nmea_data_get_latitude(&latitude, &source, &timestamp);
        nmea_data_get_longitude(&longitude, &source, &timestamp);
        nmea_data_get_speed(&speed, &source, &timestamp);
        nmea_data_get_altitude(&altitude, &source, &timestamp);

        /* ---- Distance ---- */
        if ((trip_data.last_lat != 0) && (trip_data.last_lon != 0))
        {
            float dist_m = haversine_m(trip_data.last_lat, trip_data.last_lon,
                                        latitude,   longitude);
            trip_data.distance_km      += dist_m / 1000.0f;
        }

        trip_data.avg_speed_kmh = (trip_data.speed_kmh + trip_data.avg_speed_kmh) / 2;

        /* ---- Altitude & gradient ---- */
        float delta_alt = altitude - trip_data.last_alt;

        /* Ascent: count only gains above noise threshold */
        if (delta_alt > MIN_ALT_CHANGE_M) {
            trip_data.ascent_m  += delta_alt;
        }


        trip_data.gradient_pct = compute_smoothed_gradient();

        /* ---- Advance position ---- */
        trip_data.last_lat   = latitude;
        trip_data.last_lon   = longitude;
        trip_data.last_alt = altitude;

        xSemaphoreGive(s_mutex);
    }

    bike_ui_update(trip_data);
}

void trip_computer_reset(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    reset_locked();
    xSemaphoreGive(s_mutex);
}

void trip_data_set_ble_status (gps_ble_status_t state)
{
    trip_data.gps_status.gps_ble = state;
}

gps_ble_status_t trip_data_get_ble_status (void)
{
    return trip_data.gps_status.gps_ble;
}

void trip_data_set_uart_status (gps_uart_status_t state)
{
    trip_data.gps_status.gps_uart = state;
}

gps_uart_status_t trip_data_get_uart_status (void)
{
    return trip_data.gps_status.gps_uart;
}

void trip_data_set_valid_data (bool value)
{
    trip_data.valid_data = value;
}

bool trip_data_get_valid_data (void)
{
    return trip_data.valid_data;
}