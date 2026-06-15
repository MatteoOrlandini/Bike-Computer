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

#define NMEA_BAUD        9600
#define NMEA_BUF_SIZE    256
#define NMEA_LINE_MAX    128
#define NMEA_TASK_STACK  3072
#define NMEA_TASK_PRIO   5

/* Per-UART descriptor — passed as task argument */
typedef struct {
    uart_port_t port;
    int         rx_pin;
    int         tx_pin;
    const char *name;
} uart_cfg_t;

static const uart_cfg_t s_uarts[] = {
    {
        .port   = UART_NUM_2,
        .rx_pin = 16,
        .tx_pin = 15,
        .name   = "TTL(IO15/16)",
    },
    {
        .port   = UART_NUM_1,
        .rx_pin = 43,
        .tx_pin = 44,
        .name   = "RS485(IO43/44)",
    },
};

#define NUM_UARTS (sizeof(s_uarts) / sizeof(s_uarts[0]))

/* ------------------------------------------------------------------ */
/*  Shared GPS state                                                    */
/* ------------------------------------------------------------------ */

static nmea_data_t       s_nmea_data;
static SemaphoreHandle_t s_mutex;

/* ------------------------------------------------------------------ */
/*  Generic reader task — one instance per UART                        */
/* ------------------------------------------------------------------ */

static void uart_reader_task(void *arg)
{
    const uart_cfg_t *cfg = (const uart_cfg_t *)arg;
    char    line[NMEA_LINE_MAX];
    int     pos = 0;
    uint8_t byte;

    ESP_LOGI(TAG, "[%s] reader task started", cfg->name);

    for (;;) {
        int len = uart_read_bytes(cfg->port, &byte, 1, pdMS_TO_TICKS(100));
        if (len <= 0) continue;

        if (byte == '$') {
            pos = 0;
            line[pos++] = (char)byte;
        } else if (pos > 0) {
            if (pos < NMEA_LINE_MAX - 1) {
                line[pos++] = (char)byte;
            }

            if (byte == '\n') {
                line[pos] = '\0';

                /* Read current state, parse into a copy, write back if valid */
                nmea_data_t tmp;
                xSemaphoreTake(s_mutex, portMAX_DELAY);
                tmp = s_nmea_data;
                xSemaphoreGive(s_mutex);

                if (nmea_parse_sentence(line, &tmp)) {
                    xSemaphoreTake(s_mutex, portMAX_DELAY);
                    s_nmea_data = tmp;
                    xSemaphoreGive(s_mutex);
                    ESP_LOGD(TAG, "[%s] lat=%.6f lon=%.6f spd=%.1f alt=%.1f",
                             cfg->name, tmp.latitude, tmp.longitude,
                             tmp.speed_kmh, tmp.altitude_m);
                }

                pos = 0;
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
        .baud_rate  = NMEA_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };

    for (int i = 0; i < NUM_UARTS; i++) {
        const uart_cfg_t *u = &s_uarts[i];

        ESP_ERROR_CHECK(uart_param_config(u->port, &uart_cfg));
        ESP_ERROR_CHECK(uart_set_pin(u->port,
                                     u->tx_pin, u->rx_pin,
                                     UART_PIN_NO_CHANGE,
                                     UART_PIN_NO_CHANGE));
        ESP_ERROR_CHECK(uart_driver_install(u->port,
                                            NMEA_BUF_SIZE, 0,
                                            0, NULL, 0));

        char task_name[24];
        snprintf(task_name, sizeof(task_name), "nmea_%s", u->name);
        xTaskCreate(uart_reader_task, task_name, NMEA_TASK_STACK,
                    (void *)u, NMEA_TASK_PRIO, NULL);

        ESP_LOGI(TAG, "Initialised %s @ %d baud", u->name, NMEA_BAUD);
    }
}

bool nmea_uart_get_data(nmea_data_t *out)
{
    if (!out) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_nmea_data;
    xSemaphoreGive(s_mutex);
    return out->valid;
}