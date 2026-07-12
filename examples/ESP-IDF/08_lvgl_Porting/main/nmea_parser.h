#ifndef _NMEA_PARSER_H
#define _NMEA_PARSER_H

#include "nmea_data.h"  

#ifdef __cplusplus
extern "C" {
#endif




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
 * @param source    Source of the message (BLE or UART).
 * @return          true  if the sentence was recognised and its checksum
 *                        was valid (data has been updated).
 *                  false if the sentence was ignored, malformed, or had
 *                        a bad checksum (data is unchanged).
 */
bool nmea_parse_sentence(const char *sentence, nmea_source_t source);


#ifdef __cplusplus
}
#endif

#endif