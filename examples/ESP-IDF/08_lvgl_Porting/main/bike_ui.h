#ifndef _BIKE_UI_H_
#define _BIKE_UI_H_

#include "nmea_parser.h"
#include "trip_computer.h"
 
void bike_ui_init(trip_data_t *data);
void bike_ui_update(const trip_data_t *data);

#endif