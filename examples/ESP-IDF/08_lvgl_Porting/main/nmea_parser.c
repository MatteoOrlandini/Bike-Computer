/*
 * nmea_parser.c
 *
 * Parses $GPRMC and $GPGGA NMEA 0183 sentences.
 * No dynamic allocation. No external dependencies beyond the C standard library.
 */

#include "nmea_parser.h"
#include "nmea_data.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/**
 * Verify the NMEA checksum.
 * The checksum is the XOR of all bytes between '$' and '*' (exclusive).
 * Returns true if valid or if no '*' is present (checksum omitted).
 */
static bool verify_checksum(const char *sentence)
{
    if (!sentence || sentence[0] != '$') return false;

    const char *p = sentence + 1;   /* skip '$' */
    uint8_t calc = 0;

    while (*p && *p != '*') {
        calc ^= (uint8_t)*p++;
    }

    if (*p != '*') return true;     /* no checksum — accept */

    /* Parse the two hex digits after '*' */
    char hex[3] = { p[1], p[2], '\0' };
    uint8_t expected = (uint8_t)strtol(hex, NULL, 16);

    return calc == expected;
}

/**
 * Split a comma-separated NMEA sentence into fields.
 * Writes pointers into the 'fields' array (pointing into a copy of the
 * sentence stored in 'buf').  Returns the number of fields found.
 * The trailing checksum (*XX) is stripped from the last field.
 */
#define MAX_FIELDS 20

static int split_fields(const char *sentence, char *buf, size_t buf_size,
                        const char *fields[], int max_fields)
{
    /* Copy sentence into a mutable buffer, strip \r\n */
    size_t len = strlen(sentence);
    if (len >= buf_size) len = buf_size - 1;
    memcpy(buf, sentence, len);
    buf[len] = '\0';

    /* Strip trailing \r \n */
    while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n')) {
        buf[--len] = '\0';
    }

    /* Strip checksum (*XX) from the end */
    char *star = strrchr(buf, '*');
    if (star) *star = '\0';

    /* Tokenise on commas */
    int count = 0;
    char *tok = buf;
    char *end = buf + strlen(buf);

    while (tok <= end && count < max_fields) {
        fields[count++] = tok;
        char *comma = strchr(tok, ',');
        if (!comma) break;
        *comma = '\0';
        tok = comma + 1;
    }

    return count;
}

/**
 * Convert an NMEA coordinate string (DDDMM.MMMMM) to decimal degrees.
 * hemisphere is 'N', 'S', 'E', or 'W'.
 */
static double nmea_coord_to_degrees(const char *coord, char hemisphere)
{
    if (!coord || coord[0] == '\0') return 0.0;

    double raw = atof(coord);

    /* Integer degrees are everything left of the last two digits before '.' */
    int degrees = (int)(raw / 100);
    double minutes = raw - (degrees * 100.0);
    double result = degrees + minutes / 60.0;

    if (hemisphere == 'S' || hemisphere == 'W') result = -result;

    return result;
}

/* ------------------------------------------------------------------ */
/*  $GPRMC parser                                                       */
/* ------------------------------------------------------------------ */
/*
 * $GPRMC,HHMMSS.ss,A,LLLL.LL,a,YYYYY.YY,a,x.x,x.x,DDMMYY,x.x,a*hh
 *  [0]   $GPRMC
 *  [1]   UTC time  HHMMSS.ss
 *  [2]   Status    A=active V=void
 *  [3]   Latitude  DDMM.MMMMM
 *  [4]   N/S
 *  [5]   Longitude DDDMM.MMMMM
 *  [6]   E/W
 *  [7]   Speed over ground, knots
 *  [8]   Track angle, degrees
 *  [9]   Date      DDMMYY
 * [10]   Magnetic variation
 * [11]   E/W
 */
static bool parse_gprmc(const char *fields[], int count, nmea_source_t source)
{
    int64_t now = esp_timer_get_time();
    if (count < 8) return false;

    /* Time */
    const char *t = fields[1];
    if (strlen(t) >= 6) {
        char s_hour[3] = {0};
        char s_minute[3] = {0};
        char s_second[3] = {0};
        s_hour[0] = t[0]; 
        s_hour[1] = t[1];
        s_minute[0] = t[2]; 
        s_minute[1] = t[3];
        s_second[0] = t[4];
        s_second[1] = t[5];
        nmea_data_set_time((uint8_t)atoi(s_hour), (uint8_t)atoi(s_minute), (uint8_t)atoi(s_second), source, now);
    }

    /* Status must be 'A' (active) for a valid fix */
    if (fields[2][0] != 'A') {
        ESP_LOGD("parse_gprmc RX", "Invalid data");
        return false;
    }
    else
    {
        ESP_LOGD("parse_gprmc RX", "valid data");
    }

    /* Position */
    nmea_data_set_latitude( nmea_coord_to_degrees(fields[3], fields[4][0]), source, now);
    nmea_data_set_longitude( nmea_coord_to_degrees(fields[5], fields[6][0]), source, now);

    /* Speed: knots → km/h (1 knot = 1.852 km/h) */
    nmea_data_set_speed(((float)(atof(fields[7])) * 1.852), source, now);

    return true;
}

/* ------------------------------------------------------------------ */
/*  $GPGGA parser                                                       */
/* ------------------------------------------------------------------ */
/*
 * $GPGGA,HHMMSS.ss,LLLL.LL,a,YYYYY.YY,a,x,xx,x.x,x.x,M,x.x,M,,*hh
 *  [0]   $GPGGA
 *  [1]   UTC time
 *  [2]   Latitude
 *  [3]   N/S
 *  [4]   Longitude
 *  [5]   E/W
 *  [6]   Fix quality  0=invalid 1=GPS 2=DGPS
 *  [7]   Satellites in use
 *  [8]   HDOP
 *  [9]   Altitude (above mean sea level)
 * [10]   M (metres)
 */
static bool parse_gpgga(const char *fields[], int count, nmea_source_t source)
{
    int64_t now = esp_timer_get_time();

    if (count < 10) return false;

    /* Fix quality 0 means no fix */
    if (fields[6][0] == '0' || fields[6][0] == '\0')
    {
        ESP_LOGD("parse_gpgga RX", "Invalid data");
        return false;
    } 
    else
    {        
        ESP_LOGD("parse_gpgga RX", "valid data");
    }

    /* Altitude */
    nmea_data_set_altitude((float)atof(fields[9]), source, now);

    /* Satellites */
    nmea_data_set_satellites((uint8_t)atoi(fields[7]), source, now);

    /* Position (also present in GGA, update for consistency) */
    if (fields[2][0] != '\0' && fields[4][0] != '\0') {
        nmea_data_set_latitude(nmea_coord_to_degrees(fields[2], fields[3][0]), source, now);
        nmea_data_set_longitude(nmea_coord_to_degrees(fields[4], fields[5][0]), source, now);
    }

    return true;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

bool nmea_parse_sentence(const char *sentence, nmea_source_t source)
{
    if (!sentence) return false;
    if (sentence[0] != '$')  return false;

    if (!verify_checksum(sentence)) 
    {
        ESP_LOGD("nmea_parse_sentence", "Wrong checksum: %s", sentence);
        return false;
    }

    char buf[128];
    const char *fields[MAX_FIELDS];
    int count = split_fields(sentence, buf, sizeof(buf), fields, MAX_FIELDS);
    if (count < 1) return false;

    /* Dispatch on sentence type.
     * Accept both $GPxxx and $GNxxx (multi-constellation prefix). */
    const char *type = fields[0] + 1;   /* skip '$' */
    if (strlen(type) < 5)               return false;
    const char *name = type + 2;        /* skip 'GP' / 'GN' / 'GL' etc. */

    if (strcmp(name, "RMC") == 0) 
    {
        ESP_LOGI("nmea_parse_sentence", "RMC sentence: %s", sentence);
        return parse_gprmc(fields, count, source);
    }
    else if (strcmp(name, "GGA") == 0) 
    {
        ESP_LOGI("nmea_parse_sentence", "GGA sentence: %s", sentence);
        return parse_gpgga(fields, count, source);
    }
    else
    {
        ESP_LOGI("nmea_parse_sentence", "Other sentence: %s", sentence);
    }

    return false;   /* unrecognised sentence type */
}