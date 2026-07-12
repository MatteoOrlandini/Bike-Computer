#ifndef _NMEA_DATA_H
#define _NMEA_DATA_H

#include <stdint.h>
#include <stdbool.h>

#define INVALID_TIMESTAMP (-1)

typedef enum {
    NMEA_SOURCE_UART = 0,  
    NMEA_SOURCE_BLE,  
    NMEA_SOURCE_NUMBER_OF_SOURCE,                           
} nmea_source_t;

/**
 * Parsed GPS data filled by nmea_parse_sentence().
 * Both $GPRMC and $GPGGA contribute to this struct.
 * Fields that haven't been updated yet retain their zero-initialised value.
 */
typedef struct {
    /* Position — from $GPRMC or $GPGGA */
    double  latitude;       /* decimal degrees, negative = South */
    nmea_source_t latitude_source; /* latitude value source (BLE or UART) */
    int64_t latitude_last_timestamp; /* latitude last timestamp in microseconds */

    double  longitude;      /* decimal degrees, negative = West  */
    nmea_source_t longitude_source; /* longitude value source (BLE or UART) */
    int64_t longitude_last_timestamp; /* longitude last timestamp in microseconds */

    /* Speed — from $GPRMC */
    float   speed;  /* speed in km/h */
    nmea_source_t speed_source; /* speed value source (BLE or UART) */
    int64_t speed_last_timestamp; /* speed last timestamp in microseconds */

    /* Altitude — from $GPGGA */
    float   altitude; /* altitude in meter */
    nmea_source_t altitude_source; /* altitude value source (BLE or UART) */
    int64_t altitude_last_timestamp; /* altitude last timestamp in microseconds */

    /* Time (UTC) — from $GPRMC or $GPGGA */
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    nmea_source_t time_source; /* time value source (BLE or UART) */
    int64_t time_last_timestamp; /* time last timestamp in microseconds */

    /* Signal quality — from $GPGGA */
    uint8_t satellites;
    nmea_source_t satellites_source; /* satellites value source (BLE or UART) */
    int64_t satellites_last_timestamp; /* satellites last timestamp in microseconds */
} nmea_data_t;


void nmea_data_init (void);

void nmea_data_set_latitude(double value, nmea_source_t source, int64_t timestamp);

bool nmea_data_get_latitude(double* value, nmea_source_t* source, int64_t *timestamp);

void nmea_data_set_longitude(double value, nmea_source_t source, int64_t timestamp);

bool nmea_data_get_longitude(double* value, nmea_source_t* source, int64_t *timestamp);

void nmea_data_set_speed(float value, nmea_source_t source, int64_t timestamp);

bool nmea_data_get_speed(float* value, nmea_source_t* source, int64_t *timestamp);

void nmea_data_set_altitude(double value, nmea_source_t source, int64_t timestamp);

bool nmea_data_get_altitude(double* value, nmea_source_t* source, int64_t *timestamp);

void nmea_data_set_time(uint8_t hour, uint8_t minute, uint8_t second, nmea_source_t source, int64_t timestamp);

bool nmea_data_get_time(uint8_t* hour, uint8_t* minute, uint8_t* second, nmea_source_t* source, int64_t *timestamp);

void nmea_data_set_satellites(uint8_t value, nmea_source_t source, int64_t timestamp);

bool nmea_data_get_satellites(uint8_t* value, nmea_source_t* source, int64_t *timestamp);

#endif