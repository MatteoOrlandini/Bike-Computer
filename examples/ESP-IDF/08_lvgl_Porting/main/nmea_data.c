#include "nmea_data.h"
#include "esp_timer.h"
#include "esp_log.h"

#define VALID_TIMESTAMP_TIME (2000000LL)

static nmea_data_t nmea_data;

void nmea_data_init (void)
{
    nmea_data.latitude = 0;
    nmea_data.latitude_source = NMEA_SOURCE_NUMBER_OF_SOURCE; /* latitude value source (BLE or UART) */
    nmea_data.latitude_last_timestamp = INVALID_TIMESTAMP; /* latitude last timestamp in microseconds */

    nmea_data.longitude = 0;      /* decimal degrees, negative = West  */
    nmea_data.longitude_source = NMEA_SOURCE_NUMBER_OF_SOURCE; /* longitude value source (BLE or UART) */
    nmea_data.longitude_last_timestamp = INVALID_TIMESTAMP; /* longitude last timestamp in microseconds */

    nmea_data.speed = 0;  /* speed in km/h */
    nmea_data.speed_source = NMEA_SOURCE_NUMBER_OF_SOURCE; /* speed value source (BLE or UART) */
    nmea_data.speed_last_timestamp = INVALID_TIMESTAMP; /* speed last timestamp in microseconds */

    /* Altitude — from $GPGGA */
    nmea_data.altitude = 0; /* altitude in meter */
    nmea_data.altitude_source = NMEA_SOURCE_NUMBER_OF_SOURCE; /* altitude value source (BLE or UART) */
    nmea_data.altitude_last_timestamp = INVALID_TIMESTAMP; /* altitude last timestamp in microseconds */

    /* Time (UTC) — from $GPRMC or $GPGGA */
    nmea_data.hour = 0;
    nmea_data.minute = 0;
    nmea_data.second = 0;
    nmea_data.time_source = NMEA_SOURCE_NUMBER_OF_SOURCE; /* time value source (BLE or UART) */
    nmea_data.time_last_timestamp = INVALID_TIMESTAMP; /* time last timestamp in microseconds */

    /* Signal quality — from $GPGGA */
    nmea_data.satellites = 0;
    nmea_data.satellites_source = NMEA_SOURCE_NUMBER_OF_SOURCE; /* satellites value source (BLE or UART) */
    nmea_data.satellites_last_timestamp = INVALID_TIMESTAMP; /* satellites last timestamp in microseconds */
}

void nmea_data_set_latitude(double value, nmea_source_t source, int64_t timestamp)
{
    nmea_data.latitude = value;
    if (source < NMEA_SOURCE_NUMBER_OF_SOURCE)
        nmea_data.latitude_source = source; 
    nmea_data.latitude_last_timestamp = timestamp;
}

bool nmea_data_get_latitude(double* value, nmea_source_t* source, int64_t* timestamp)
{
    bool is_valid = false;
    *value = nmea_data.latitude;
    *timestamp = nmea_data.latitude_last_timestamp;
    int64_t now_us = esp_timer_get_time();
    if ((nmea_data.latitude < NMEA_SOURCE_NUMBER_OF_SOURCE) && \
    (nmea_data.latitude_last_timestamp != INVALID_TIMESTAMP) && \
    ((now_us - nmea_data.latitude_last_timestamp) < VALID_TIMESTAMP_TIME))
    {
        *source = nmea_data.latitude;
        is_valid = true;
    }
    return is_valid;
}


void nmea_data_set_longitude(double value, nmea_source_t source, int64_t timestamp)
{
    nmea_data.longitude = value;
    if (source < NMEA_SOURCE_NUMBER_OF_SOURCE)
        nmea_data.longitude_source = source;
    nmea_data.longitude_last_timestamp = timestamp; 
}

bool nmea_data_get_longitude(double* value, nmea_source_t* source, int64_t* timestamp)
{
    bool is_valid = false;
    *value = nmea_data.longitude;
    *timestamp = nmea_data.longitude_last_timestamp;
    int64_t now_us = esp_timer_get_time();
    if ((nmea_data.longitude_source < NMEA_SOURCE_NUMBER_OF_SOURCE) && \
    (nmea_data.longitude_last_timestamp != INVALID_TIMESTAMP) && \
    ((now_us - nmea_data.longitude_last_timestamp) < VALID_TIMESTAMP_TIME))
    {
        *source = nmea_data.longitude_source;
        is_valid = true;
    }    
    return is_valid;
}

void nmea_data_set_speed(float value, nmea_source_t source, int64_t timestamp)
{
    nmea_data.speed = value;
    if (source < NMEA_SOURCE_NUMBER_OF_SOURCE)
        nmea_data.speed_source = source;
    nmea_data.speed_last_timestamp = timestamp; 
}

bool nmea_data_get_speed(float* value, nmea_source_t* source, int64_t* timestamp)
{
    bool is_valid = false;
    *value = nmea_data.speed;
    *timestamp = nmea_data.speed_last_timestamp;
    int64_t now_us = esp_timer_get_time();
    if (( nmea_data.speed_source < NMEA_SOURCE_NUMBER_OF_SOURCE) && \
    (nmea_data.speed_last_timestamp != INVALID_TIMESTAMP) && \
    ((now_us - nmea_data.speed_last_timestamp) < VALID_TIMESTAMP_TIME))
    {
        *source = nmea_data.speed_source;
        is_valid = true;
    }
    return is_valid;
}

void nmea_data_set_altitude(double value, nmea_source_t source, int64_t timestamp)
{
    nmea_data.altitude = value;
    if (source < NMEA_SOURCE_NUMBER_OF_SOURCE)
        nmea_data.altitude_source = source;
    nmea_data.altitude_last_timestamp = timestamp; 
}

bool nmea_data_get_altitude(double* value, nmea_source_t* source, int64_t* timestamp)
{
    bool is_valid = false;
    *value = nmea_data.altitude;
    *timestamp = nmea_data.altitude_last_timestamp;
    int64_t now_us = esp_timer_get_time();
    if ((nmea_data.altitude_source < NMEA_SOURCE_NUMBER_OF_SOURCE) && \
    (nmea_data.altitude_last_timestamp != INVALID_TIMESTAMP) && \
    ((now_us - nmea_data.altitude_last_timestamp) < VALID_TIMESTAMP_TIME))
    {
        *source = nmea_data.altitude_source;
        is_valid = true;
    }    
    return is_valid;
}

void nmea_data_set_time(uint8_t hour, uint8_t minute, uint8_t second, nmea_source_t source, int64_t timestamp)
{
    nmea_data.hour = hour;
    nmea_data.minute = minute;
    nmea_data.second = second;
    if (source < NMEA_SOURCE_NUMBER_OF_SOURCE)
        nmea_data.time_source = source;
    nmea_data.time_last_timestamp = timestamp; 
}

bool nmea_data_get_time(uint8_t* hour, uint8_t* minute, uint8_t* second, nmea_source_t *source, int64_t *timestamp)
{
    bool is_valid = false;
    *hour = nmea_data.hour;
    *minute = nmea_data.minute;
    *second = nmea_data.second;
    *timestamp = nmea_data.time_last_timestamp;
    int64_t now_us = esp_timer_get_time();
    ESP_LOGI("nmea_data_get_time", "now: %lld", now_us);
    ESP_LOGI("nmea_data_get_time", "last_time_stamp: %lld", nmea_data.time_last_timestamp);
    if ((nmea_data.time_source < NMEA_SOURCE_NUMBER_OF_SOURCE) && \
    (nmea_data.time_last_timestamp != INVALID_TIMESTAMP) && \
    ((now_us - nmea_data.time_last_timestamp) < VALID_TIMESTAMP_TIME))
    {
        *source = nmea_data.time_source;
        is_valid = true;
    }
        
    return is_valid;
}

void nmea_data_set_satellites(uint8_t value, nmea_source_t source, int64_t timestamp)
{
    nmea_data.satellites = value;
    if (source < NMEA_SOURCE_NUMBER_OF_SOURCE)
        nmea_data.satellites_source = source;
    nmea_data.satellites_last_timestamp = timestamp; 
}

bool nmea_data_get_satellites(uint8_t* value, nmea_source_t* source, int64_t *timestamp)
{
    bool is_valid = false;
    *value = nmea_data.satellites;
    *timestamp = nmea_data.satellites_last_timestamp;
    int64_t now_us = esp_timer_get_time();
    if ((nmea_data.satellites_source < NMEA_SOURCE_NUMBER_OF_SOURCE) &&  \
    (nmea_data.satellites_last_timestamp != INVALID_TIMESTAMP) && \
    ((now_us - nmea_data.satellites_last_timestamp) < VALID_TIMESTAMP_TIME))
    {
        *source = nmea_data.satellites_source;
        is_valid = true;
    }    
    return is_valid;
}