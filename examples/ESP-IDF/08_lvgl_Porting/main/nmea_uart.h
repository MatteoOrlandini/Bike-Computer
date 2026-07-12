#ifndef _NMEA_UART_H
#define _NMEA_UART_H

#include <stdbool.h>
#include "nmea_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialise both UART sources and start their reader tasks:
 *   UART2  IO15(TX)/IO16(RX)  — direct TTL
 *   UART1  IO44(TX)/IO43(RX)  — RS485 terminal
 * Call once from app_main() before the LVGL loop starts.
 */
void nmea_uart_init(void);

/**
 * Copy the latest GPS fix received over UART into *out.
 * Thread-safe. Returns true if at least one valid fix has been received.
 */
bool nmea_uart_get_data(nmea_data_t *out);
#ifdef TEST_GPS_BAUD_RATE
void baudrate_autoscan_run(void);
#endif

#ifdef __cplusplus
}
#endif

#endif