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
#define NMEA_UART_BAUD_RATE_9600     (9600)   // UART baud rate low speed
#define NMEA_UART_BAUD_RATE_115200     (115200)   // UART baud rate high speed
#define NMEA_TASK_STACK_SIZE    (4096)  // Task stack size 

// u-blox UBX binary commands
// 1. Set Navigation Mode to Pedestrian (NAV5)
const uint8_t UBX_NAV5_PEDESTRIAN[] = {
    0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x4D
};

// 2. Set Update Rate to 5Hz / 200ms (RATE)
const uint8_t UBX_RATE_5HZ[] = {
    0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xC8, 0x00, 0x01, 0x00, 0x01, 0x00, 0xDE, 0x6A
};

// 3. Set Baud Rate to 115200 (PRT)
const uint8_t UBX_PRT_115200[] = {
    0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0xD0, 0x08, 0x00, 0x00, 0x00, 0xC2,
    0x01, 0x00, 0x07, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x7E
};

// UBX-CFG-ODO command to enable low-pass filtering on speed and distance calculations
const uint8_t UBX_CFG_FILTER_SPEED[] = {
    0xB5, 0x62, 0x06, 0x1E, 0x14, 0x00, 
    0x00,       // Version 0
    0x00, 0x00, 0x00, // Reserved
    0x03,       // Flags: Enable both Odometer and Low-Pass Speed Filter
    0x00,       // GNSS Profile (0 = default/auto)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Reserved
    0x05,       // CogFilter: Velocity low-pass filter time constant (5 seconds is great for bikes)
    0x00, 0x00, 0x00, 0x00, 0x00, // Reserved
    0x41, 0x6E  // Checksum (CK_A, CK_B)
};

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
        int len = uart_read_bytes(NMEA_UART_PORT_NUM, line, NMEA_BUF_SIZE, 100 / portTICK_PERIOD_MS);        
        if (len > 0)
        {
            for (int i = 0; i < len; i++)
            {
                uart_parse_data(line[i], &s_nmea_data);
            }
        }
    }
}

// Helper function to send commands over UART
static void send_ubx_cmd(const uint8_t *cmd, size_t len) {
    uart_write_bytes(NMEA_UART_PORT_NUM, (const char *)cmd, len);
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
        .baud_rate  = NMEA_UART_BAUD_RATE_9600,  // Set baud rate 
        .data_bits  = UART_DATA_8_BITS,     // 8 data bits 
        .parity     = UART_PARITY_DISABLE,  // No parity bit 
        .stop_bits  = UART_STOP_BITS_1,      // 1 stop bit 
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE, // Disable hardware flow control 
        .source_clk = UART_SCLK_DEFAULT,   // Default clock source 
    };


    ESP_ERROR_CHECK(uart_driver_install(NMEA_UART_PORT_NUM, NMEA_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(NMEA_UART_PORT_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(NMEA_UART_PORT_NUM, NMEA_TXD_PIN, NMEA_RXD_PIN, NMEA_TEST_RTS, NMEA_TEST_CTS));   

    vTaskDelay(pdMS_TO_TICKS(500));

    // 2. Command the GPS module to shift internal speed to 115200 baud
    ESP_LOGI(TAG, "Upgrading GPS hardware baudrate to 115200...");
    send_ubx_cmd(UBX_PRT_115200, sizeof(UBX_PRT_115200));
    vTaskDelay(pdMS_TO_TICKS(200));

    // 3. Update ESP32-S3 UART hardware to match the new speed (115200)
    ESP_ERROR_CHECK(uart_set_baudrate(NMEA_UART_PORT_NUM, 115200));
    vTaskDelay(pdMS_TO_TICKS(500));

    // 4. Inject Pedestrian profile, 5Hz execution configurations and enable low-pass filtering on speed and distance calculations
    ESP_LOGI(TAG, "Injecting Pedestrian Profile, 5Hz update rate configurations and low-pass filtering on speed and distance calculations...");
    send_ubx_cmd(UBX_NAV5_PEDESTRIAN, sizeof(UBX_NAV5_PEDESTRIAN));
    vTaskDelay(pdMS_TO_TICKS(150));
    send_ubx_cmd(UBX_RATE_5HZ, sizeof(UBX_RATE_5HZ));
    vTaskDelay(pdMS_TO_TICKS(150));
    send_ubx_cmd(UBX_CFG_FILTER_SPEED, sizeof(UBX_CFG_FILTER_SPEED));
    vTaskDelay(pdMS_TO_TICKS(150));

    ESP_LOGI(TAG, "U-blox configuration successful. Starting GPS Task loop...");

    xTaskCreate(uart_reader_task, "NMEA UART Reader Task", NMEA_TASK_STACK_SIZE,
                NULL, NMEA_TASK_PRIO, NULL);

    ESP_LOGI(TAG, "Initialised NMEA UART Reader Task");
}

bool nmea_uart_get_data(nmea_data_t *out)
{
    if (!out) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_nmea_data;
    xSemaphoreGive(s_mutex);
    return out->valid;
}