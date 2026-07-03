/*
 * nmea_uart.c  —  multi-UART NMEA source manager
 *
 * Manages two independent UART sources for the NEO-8M GPS module:
 *
 *   UART2  IO15(TX) / IO16(RX)  — direct TTL connection
 *   UART1  IO44(TX) / IO43(RX)  — RS485 terminal (SP3485EN, receive-only)
 *
 * Both UARTs run independent reader tasks that feed the same shared
 * nmea_data_t via the shared nmea_parser. Whichever source is physically
 * connected will update the state; the other produces no data and is
 * silently ignored. No priority arbitration needed.
 *
 * Thread-safe: a single mutex protects the shared nmea_data_t.
 */

#include "nmea_uart.h"
#include "nmea_parser.h"

#include <string.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define TAG "NMEA_UART"

/* ------------------------------------------------------------------ */
/*  Configuration                                                       */
/* ------------------------------------------------------------------ */

#define NMEA_BUF_SIZE    (256)
#define NMEA_LINE_MAX    (128)
#define NMEA_TASK_PRIO   (5)

#define NMEA_TXD_PIN (44)   // UART TX pin 
#define NMEA_RXD_PIN (43)   // UART RX pin 
#define NMEA_TEST_RTS (UART_PIN_NO_CHANGE)        // No RTS (request to send) 
#define NMEA_TEST_CTS (UART_PIN_NO_CHANGE)        // No CTS (clear to send) 

#define NMEA_UART_PORT_NUM      (2)    // UART port number 
#define NMEA_UART_BAUD_RATE     (9600)   // UART baud rate 
#define NMEA_TASK_STACK_SIZE    (3072)  // Task stack size 


/* ------------------------------------------------------------------ */
/*  Shared GPS state                                                    */
/* ------------------------------------------------------------------ */

static nmea_data_t       s_nmea_data;
static SemaphoreHandle_t s_mutex;
static char nmea_string[NMEA_BUF_SIZE];


/* ------------------------------------------------------------------ */
/*  Generic reader task — one instance per UART                        */
/* ------------------------------------------------------------------ */



/**
 * Parse a received byte from serial port to retrieve an NMEA 0183 
 * sentence into *data.
 *
 * Only $GPRMC and $GPGGA are processed; all other sentence types are
 * silently ignored.  The function updates only the fields relevant to
 * the sentence type received, leaving the rest unchanged, so callers
 * should pass the same nmea_data_t across successive calls.
 *
 * @param rx_byte   The received byte from serial port.
 * @param data      Output struct to update. Must not be NULL.
 * @return          true  if the sentence was recognised and its checksum
 *                        was valid (data has been updated).
 *                  false if the sentence was ignored, malformed, or had
 *                        a bad checksum (data is unchanged).
 */
static bool uart_parse_data(const char rx_byte, nmea_data_t *data)
{
    static int pos = 0;
    bool result = false;

    if (rx_byte == '$') 
    {
        pos = 0;
        nmea_string[pos++] = rx_byte;
    } 
    else if (pos > 0) 
    {
        if (pos < NMEA_LINE_MAX - 1) 
        {
            nmea_string[pos++] = rx_byte;
        }
        
        if (rx_byte == '\n') 
        {
            nmea_string[pos] = '\0';

            /* Read current state, parse into a copy, write back if valid */
            nmea_data_t tmp;
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            tmp = *data;
            xSemaphoreGive(s_mutex);

            result = nmea_parse_sentence(nmea_string, &tmp);
            if (result) 
            {
                xSemaphoreTake(s_mutex, portMAX_DELAY);
                *data = tmp;
                xSemaphoreGive(s_mutex);
                ESP_LOGD(TAG, "NMEA UART Reader Task: lat=%.6f lon=%.6f spd=%.1f alt=%.1f",
                            tmp.latitude, tmp.longitude,
                            tmp.speed_kmh, tmp.altitude_m);
            }

            pos = 0;
        }
    }
    return result;
}


static void uart_reader_task(void *arg)
{
    char    line[NMEA_BUF_SIZE];

    ESP_LOGI(TAG, "NMEA UART reader task started");

    for (;;) {
        // Read data from the UART 
        int len = uart_read_bytes(NMEA_UART_PORT_NUM, line, NMEA_BUF_SIZE, pdMS_TO_TICKS(100));        
        if (len > 0)
        {
            for (int i = 0; i < len; i++)
            {
                uart_parse_data(line[i], &s_nmea_data);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void nmea_uart_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    assert(s_mutex);
    memset(&s_nmea_data, 0, sizeof(s_nmea_data));

    const uart_config_t uart_cfg = {
        .baud_rate  = NMEA_UART_BAUD_RATE,  // Set baud rate 
        .data_bits  = UART_DATA_8_BITS,     // 8 data bits 
        .parity     = UART_PARITY_DISABLE,  // No parity bit 
        .stop_bits  = UART_STOP_BITS_1,      // 1 stop bit 
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE, // Disable hardware flow control 
        .source_clk = UART_SCLK_DEFAULT,   // Default clock source 
    };


    ESP_ERROR_CHECK(uart_driver_install(NMEA_UART_PORT_NUM, NMEA_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(NMEA_UART_PORT_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(NMEA_UART_PORT_NUM, NMEA_TXD_PIN, NMEA_RXD_PIN, NMEA_TEST_RTS, NMEA_TEST_CTS));   

    xTaskCreate(uart_reader_task, "NMEA UART Reader Task", NMEA_TASK_STACK_SIZE,
                NULL, NMEA_TASK_PRIO, NULL);

    ESP_LOGI(TAG, "Initialised NMEA UART Reader Task @ %d baud", NMEA_UART_BAUD_RATE);
}

bool nmea_uart_get_data(nmea_data_t *out)
{
    if (!out) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_nmea_data;
    xSemaphoreGive(s_mutex);
    return out->valid;
}