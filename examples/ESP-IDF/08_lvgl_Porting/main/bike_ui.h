#ifndef _BIKE_UI_H_
#define _BIKE_UI_H_

#include "nmea_parser.h"

void bike_ui_init(nmea_data_t *data);
void bike_ui_update(const nmea_data_t *data);

#endif