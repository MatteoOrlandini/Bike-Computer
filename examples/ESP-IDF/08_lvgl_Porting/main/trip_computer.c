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

#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "TRIP"

/* ------------------------------------------------------------------ */
/*  Tunable constants                                                   */
/* ------------------------------------------------------------------ */

#define MIN_SPEED_KMH        4.0f   /* ignore updates below this speed  */
#define MIN_ALT_CHANGE_M     2.0f   /* ignore altitude changes below this */
#define GRADIENT_WINDOW      5      /* number of fixes to smooth gradient */
#define EARTH_RADIUS_M       6371000.0  /* mean Earth radius, metres     */

/* ------------------------------------------------------------------ */
/*  Internal state                                                      */
/* ------------------------------------------------------------------ */

/* Gradient smoothing ring buffer — stores last N altitude/distance pairs */
typedef struct {
    float delta_alt_m;   /* altitude change for this segment (metres) */
    float delta_dist_m;  /* horizontal distance for this segment (m)  */
} gradient_sample_t;

typedef struct {
    /* Latest fix position (for delta calculations) */
    double   last_lat;
    double   last_lon;
    float    last_alt_m;
    bool     has_prev;          /* true once we have a previous fix     */

    /* Trip accumulators */
    float    distance_km;
    float    ascent_m;
    uint32_t moving_time_sec;   /* seconds spent above MIN_SPEED_KMH   */
    int64_t  start_us;          /* esp_timer_get_time() at reset        */

    /* Gradient smoothing */
    gradient_sample_t grad_buf[GRADIENT_WINDOW];
    int               grad_head;   /* next write index                  */
    int               grad_count;  /* valid entries in buffer           */

    /* Published output */
    trip_data_t       data;
} trip_state_t;

static trip_state_t      s_state;
static SemaphoreHandle_t s_mutex;

/* ------------------------------------------------------------------ */
/*  Haversine distance (metres)                                         */
/* ------------------------------------------------------------------ */

static float haversine_m(double lat1, double lon1, double lat2, double lon2)
{
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dlat / 2) * sin(dlat / 2)
             + cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0)
             * sin(dlon / 2) * sin(dlon / 2);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return (float)(EARTH_RADIUS_M * c);
}

/* ------------------------------------------------------------------ */
/*  Gradient calculation over smoothing window                          */
/* ------------------------------------------------------------------ */

static float compute_smoothed_gradient(void)
{
    if (s_state.grad_count == 0) return 0.0f;

    float total_alt  = 0.0f;
    float total_dist = 0.0f;

    for (int i = 0; i < s_state.grad_count; i++) {
        total_alt  += s_state.grad_buf[i].delta_alt_m;
        total_dist += s_state.grad_buf[i].delta_dist_m;
    }

    if (total_dist < 1.0f) return 0.0f;   /* avoid division by near-zero */
    return (total_alt / total_dist) * 100.0f;
}

/* ------------------------------------------------------------------ */
/*  Internal reset (call with mutex held)                               */
/* ------------------------------------------------------------------ */

static void reset_locked(void)
{
    /* Preserve last known position and validity */
    double last_lat  = s_state.last_lat;
    double last_lon  = s_state.last_lon;
    float  last_alt  = s_state.last_alt_m;
    bool   has_prev  = s_state.has_prev;
    bool   valid     = s_state.data.valid;
    float  speed     = s_state.data.speed_kmh;

    memset(&s_state, 0, sizeof(s_state));

    s_state.last_lat   = last_lat;
    s_state.last_lon   = last_lon;
    s_state.last_alt_m = last_alt;
    s_state.has_prev   = has_prev;
    s_state.data.valid = valid;
    s_state.data.speed_kmh = speed;
    s_state.start_us   = esp_timer_get_time();

    ESP_LOGI(TAG, "Trip reset");
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void trip_computer_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    assert(s_mutex);
    memset(&s_state, 0, sizeof(s_state));
    s_state.start_us = esp_timer_get_time();
    ESP_LOGI(TAG, "Trip computer initialised");
}

void trip_computer_update(const nmea_data_t *fix)
{
    if (!fix || !fix->valid) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* Always update current speed */
    s_state.data.speed_kmh = fix->speed_kmh;
    s_state.data.valid     = true;

    /* Elapsed time */
    int64_t now_us = esp_timer_get_time();
    s_state.data.elapsed_sec =
        (uint32_t)((now_us - s_state.start_us) / 1000000LL);

    /* Skip movement calculations below minimum speed */
    if (fix->speed_kmh < MIN_SPEED_KMH) {
        s_state.has_prev = false;   /* reset continuity so next valid fix
                                       starts a fresh segment             */
        xSemaphoreGive(s_mutex);
        return;
    }

    if (!s_state.has_prev) {
        /* First fix above threshold — record position, nothing to compute */
        s_state.last_lat   = fix->latitude;
        s_state.last_lon   = fix->longitude;
        s_state.last_alt_m = fix->altitude_m;
        s_state.has_prev   = true;
        xSemaphoreGive(s_mutex);
        return;
    }

    /* ---- Distance ---- */
    float dist_m = haversine_m(s_state.last_lat, s_state.last_lon,
                                fix->latitude,   fix->longitude);
    s_state.distance_km      += dist_m / 1000.0f;
    s_state.data.distance_km  = s_state.distance_km;

    /* ---- Moving time & average speed ---- */
    s_state.moving_time_sec++;   /* one fix ≈ one second at 1 Hz */
    if (s_state.moving_time_sec > 0) {
        s_state.data.avg_speed_kmh =
            (s_state.distance_km / (float)s_state.moving_time_sec) * 3600.0f;
    }

    /* ---- Altitude & gradient ---- */
    float delta_alt = fix->altitude_m - s_state.last_alt_m;

    /* Ascent: count only gains above noise threshold */
    if (delta_alt > MIN_ALT_CHANGE_M) {
        s_state.ascent_m      += delta_alt;
        s_state.data.ascent_m  = s_state.ascent_m;
    }

    /* Gradient smoothing ring buffer */
    s_state.grad_buf[s_state.grad_head].delta_alt_m  = delta_alt;
    s_state.grad_buf[s_state.grad_head].delta_dist_m = dist_m;
    s_state.grad_head = (s_state.grad_head + 1) % GRADIENT_WINDOW;
    if (s_state.grad_count < GRADIENT_WINDOW) s_state.grad_count++;

    s_state.data.gradient_pct = compute_smoothed_gradient();

    /* ---- Advance position ---- */
    s_state.last_lat   = fix->latitude;
    s_state.last_lon   = fix->longitude;
    s_state.last_alt_m = fix->altitude_m;

    xSemaphoreGive(s_mutex);
}

bool trip_computer_get_data(trip_data_t *out)
{
    if (!out) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_state.data;
    xSemaphoreGive(s_mutex);
    return out->valid;
}

void trip_computer_reset(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    reset_locked();
    xSemaphoreGive(s_mutex);
}