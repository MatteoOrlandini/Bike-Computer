#ifndef _NMEA_PARSER_H
#define _NMEA_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parsed GPS data filled by nmea_parse_sentence().
 * Both $GPRMC and $GPGGA contribute to this struct.
 * Fields that haven't been updated yet retain their zero-initialised value.
 */
typedef struct {
    /* Position — from $GPRMC or $GPGGA */
    double  latitude;       /* decimal degrees, negative = South */
    double  longitude;      /* decimal degrees, negative = West  */

    /* Speed — from $GPRMC */
    float   speed_kmh;

    /* Altitude — from $GPGGA */
    float   altitude_m;

    /* Time (UTC) — from $GPRMC or $GPGGA */
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    /* Signal quality — from $GPGGA */
    uint8_t satellites;

    /* true once at least one valid $GPRMC fix has been received */
    bool    valid;
} nmea_data_t;

/**
 * Parse a single NMEA 0183 sentence into *data.
 *
 * Only $GPRMC and $GPGGA are processed; all other sentence types are
 * silently ignored.  The function updates only the fields relevant to
 * the sentence type received, leaving the rest unchanged, so callers
 * should pass the same nmea_data_t across successive calls.
 *
 * @param sentence  Null-terminated NMEA sentence, e.g. "$GPRMC,..."
 *                  The trailing <CR><LF> is optional.
 * @param data      Output struct to update. Must not be NULL.
 * @return          true  if the sentence was recognised and its checksum
 *                        was valid (data has been updated).
 *                  false if the sentence was ignored, malformed, or had
 *                        a bad checksum (data is unchanged).
 */
bool nmea_parse_sentence(const char *sentence, nmea_data_t *data);

#ifdef __cplusplus
}
#endif

#endif