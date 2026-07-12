/*
 * nmea_uart.c  —  multi-UART NMEA source manager
 *
 * Manages UART2  IO44(TX) / IO43(RX)
 *
 */
#ifdef TEST_GPS_BAUD_RATE
// === TEMPORARY BAUDRATE AUTO-SCAN ===
// Da chiamare UNA VOLTA in app_main(), PRIMA di nmea_uart_init()
// e PRIMA di ublox_configure_for_cycling().
// Rimuovere una volta trovato il baudrate corretto.

#include "driver/uart.h"
#include "esp_log.h"
#include <ctype.h>
#include "esp_timer.h"
#endif
#include "nmea_uart.h"
#include "nmea_parser.h"
#include "nmea_data.h"

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
static const uint8_t UBX_NAV5_PEDESTRIAN[] = {
    0xB5, 0x62,             // sync char 1, 2
    0x06, 0x24,             // class = CFG, id = NAV5
    0x24, 0x00,             // length = 36 (little endian)
    0x01, 0x00,             // mask: bit0 (dyn) = 1, resto = 0
    0x00,                   // dynModel = 0 -> Portable
    0x03,                   // fixMode = 3 (Auto 2D/3D)
    0x00, 0x00, 0x00, 0x00, // fixedAlt
    0x00, 0x00, 0x00, 0x00, // fixedAltVar
    0x00,                   // minElev
    0x00,                   // drLimit
    0x00, 0x00,             // pDop
    0x00, 0x00,             // tDop
    0x00, 0x00,             // pAcc
    0x00, 0x00,             // tAcc
    0x00,                   // staticHoldThresh
    0x00,                   // dgnssTimeout
    0x00,                   // cnoThreshNumSVs
    0x00,                   // cnoThresh
    0x00, 0x00,             // reserved1
    0x00, 0x00,             // staticHoldMaxDist
    0x00,                   // utcStandard
    0x00, 0x00, 0x00, 0x00, 0x00, // reserved2
    0x52, 0x4B              // checksum (CK_A, CK_B)
};

// 2. Set Update Rate to 5Hz / 200ms (RATE)
static const uint8_t UBX_RATE_5HZ[] = {
    0xB5, 0x62,             // sync char 1, 2
    0x06, 0x08, 0x06, 0x00,
    0xC8, 0x00,   // measRate = 200 ms
    0x01, 0x00,   // navRate = 1
    0x01, 0x00,   // timeRef = 1 (GPS time)
    0xDE, 0x6A    // checksum
};

// UBX-CFG-GNSS: GPS + SBAS + Galileo + QZSS + GLONASS attivi, BeiDou + IMES disattivati
// (per rispettare il budget canali del chip M8 e garantire GPS+GLONASS+Galileo concorrenti stabili)
static const uint8_t UBX_CFG_GNSS_GPS_GLONASS_GALILEO[] = {
    0xB5, 0x62,             // sync char 1, 2
    0x06, 0x3E,             // class = CFG, id = GNSS
    0x3C, 0x00,             // length = 60 (little endian)

    0x00,                   // msgVer = 0
    0x00,                   // numTrkChHw (read-only, ignorato in scrittura)
    0xFF,                   // numTrkChUse = 0xFF -> usa il massimo disponibile
    0x07,                   // numConfigBlocks = 7

    // --- Blocco 1: GPS ---
    0x00,                   // gnssId = 0 (GPS)
    0x08,                   // resTrkCh = 8 (canali minimi riservati)
    0x10,                   // maxTrkCh = 16 (canali massimi)
    0x00,                   // reserved1
    0x01,                   // flags byte0: bit0 = 1 -> abilitato
    0x00,                   // flags byte1: riservato
    0x01,                   // flags byte2: sigCfgMask = 0x01 (L1C/A)
    0x00,                   // flags byte3: riservato

    // --- Blocco 2: SBAS ---
    0x01,                   // gnssId = 1 (SBAS)
    0x01,                   // resTrkCh = 1
    0x03,                   // maxTrkCh = 3
    0x00,                   // reserved1
    0x01,                   // flags byte0: abilitato
    0x00,                   // flags byte1: riservato
    0x01,                   // flags byte2: sigCfgMask = 0x01 (L1C/A)
    0x00,                   // flags byte3: riservato

    // --- Blocco 3: Galileo ---
    0x02,                   // gnssId = 2 (Galileo)
    0x04,                   // resTrkCh = 4
    0x08,                   // maxTrkCh = 8
    0x00,                   // reserved1
    0x01,                   // flags byte0: abilitato
    0x00,                   // flags byte1: riservato
    0x01,                   // flags byte2: sigCfgMask = 0x01 (E1OS)
    0x00,                   // flags byte3: riservato

    // --- Blocco 4: BeiDou (disabilitato per liberare canali) ---
    0x03,                   // gnssId = 3 (BeiDou)
    0x08,                   // resTrkCh = 8
    0x10,                   // maxTrkCh = 16
    0x00,                   // reserved1
    0x00,                   // flags byte0: bit0 = 0 -> DISABILITATO
    0x00,                   // flags byte1: riservato
    0x01,                   // flags byte2: sigCfgMask = 0x01 (non attivo, bit enable = 0)
    0x00,                   // flags byte3: riservato

    // --- Blocco 5: IMES (disabilitato) ---
    0x04,                   // gnssId = 4 (IMES)
    0x00,                   // resTrkCh = 0
    0x08,                   // maxTrkCh = 8
    0x00,                   // reserved1
    0x00,                   // flags byte0: DISABILITATO
    0x00,                   // flags byte1: riservato
    0x01,                   // flags byte2: sigCfgMask
    0x00,                   // flags byte3: riservato

    // --- Blocco 6: QZSS ---
    0x05,                   // gnssId = 5 (QZSS)
    0x00,                   // resTrkCh = 0
    0x03,                   // maxTrkCh = 3
    0x00,                   // reserved1
    0x01,                   // flags byte0: abilitato
    0x00,                   // flags byte1: riservato
    0x05,                   // flags byte2: sigCfgMask = 0x05 (L1C/A + L1S)
    0x00,                   // flags byte3: riservato

    // --- Blocco 7: GLONASS ---
    0x06,                   // gnssId = 6 (GLONASS)
    0x08,                   // resTrkCh = 8
    0x10,                   // maxTrkCh = 16
    0x00,                   // reserved1
    0x01,                   // flags byte0: abilitato
    0x00,                   // flags byte1: riservato
    0x01,                   // flags byte2: sigCfgMask = 0x01 (L1OF)
    0x00,                   // flags byte3: riservato

    0x0E, 0xB8              // checksum (CK_A, CK_B)
};

/*
// UBX-MON-VER poll request: nessun payload, chiede al modulo di rispondere
// con la sua versione firmware/hardware
static const uint8_t UBX_POLL_MON_VER[] = {
    0xB5, 0x62,   // sync char 1, 2
    0x0A, 0x04,   // class = MON, id = VER
    0x00, 0x00,   // length = 0 (nessun payload nel poll request)
    0x0E, 0x34    // checksum (CK_A, CK_B)
};
*/

// UBX-CFG-MSG: abilita GGA su UART1 (rate = 1, una volta per soluzione nav)
static const uint8_t UBX_CFG_MSG_GGA_ON[] = {
    0xB5, 0x62,             // sync char 1, 2
    0x06, 0x01,             // class = CFG, id = MSG
    0x08, 0x00,             // length = 8

    0xF0, 0x00,             // msgClass=0xF0 (NMEA), msgID=0x00 -> GGA
    0x00,                   // rate su I2C/DDC = 0
    0x01,                   // rate su UART1 = 1 (abilitata)
    0x00, 0x00, 0x00, 0x00, // rate su UART2, USB, SPI, reserved = 0

    0x00, 0x28              // checksum
};

// UBX-CFG-MSG: abilita RMC su UART1 (rate = 1)
static const uint8_t UBX_CFG_MSG_RMC_ON[] = {
    0xB5, 0x62, 0x06, 0x01, 0x08, 0x00,
    0xF0, 0x04,             // msgID=0x04 -> RMC
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x44              // checksum
};

// UBX-CFG-MSG: disabilita GLL (rate = 0 su tutte le porte)
static const uint8_t UBX_CFG_MSG_GLL_OFF[] = {
    0xB5, 0x62, 0x06, 0x01, 0x08, 0x00,
    0xF0, 0x01,             // msgID=0x01 -> GLL
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x2A
};

// UBX-CFG-MSG: disabilita GSA (rate = 0)
static const uint8_t UBX_CFG_MSG_GSA_OFF[] = {
    0xB5, 0x62, 0x06, 0x01, 0x08, 0x00,
    0xF0, 0x02,             // msgID=0x02 -> GSA
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x31
};

// UBX-CFG-MSG: disabilita GSV (rate = 0)
static const uint8_t UBX_CFG_MSG_GSV_OFF[] = {
    0xB5, 0x62, 0x06, 0x01, 0x08, 0x00,
    0xF0, 0x03,             // msgID=0x03 -> GSV
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x38
};

// UBX-CFG-MSG: disabilita VTG (rate = 0)
static const uint8_t UBX_CFG_MSG_TVG_OFF[] = {
    0xB5, 0x62, 0x06, 0x01, 0x08, 0x00,
    0xF0, 0x05,             // msgID=0x05 -> VTG
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x46
};

/* ------------------------------------------------------------------ */
/*  Shared GPS state                                                    */
/* ------------------------------------------------------------------ */

static SemaphoreHandle_t s_mutex;
static char nmea_string[NMEA_BUF_SIZE];


/* ------------------------------------------------------------------ */
/*  Generic reader task — one instance per UART                        */
/* ------------------------------------------------------------------ */
#ifdef TEST_GPS_BAUD_RATE
// === TEMPORARY BAUDRATE AUTO-SCAN ===
// Da chiamare UNA VOLTA in app_main(), PRIMA di nmea_uart_init()
// e PRIMA di ublox_configure_for_cycling().
// Rimuovere una volta trovato il baudrate corretto.

static const char *SCAN_TAG = "BAUD_SCAN";

// Adatta questi valori alla UART/pin del tuo NEO-8M/M10 (UART2, GPIO15/16)
#define SCAN_UART_PORT      UART_NUM_2
#define SCAN_UART_TX_PIN    16
#define SCAN_UART_RX_PIN    15
#define SCAN_BUF_SIZE       512
#define SCAN_WINDOW_MS      400   // finestra di ascolto per ogni baudrate (> periodo 200-300ms osservato)

static bool scan_try_baud(int baud)
{

    const uart_config_t cfg = {
        .baud_rate  = baud,  // Set baud rate 
        .data_bits  = UART_DATA_8_BITS,     // 8 data bits 
        .parity     = UART_PARITY_DISABLE,  // No parity bit 
        .stop_bits  = UART_STOP_BITS_1,      // 1 stop bit 
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE, // Disable hardware flow control 
        .source_clk = UART_SCLK_DEFAULT,   // Default clock source 
    };


    ESP_ERROR_CHECK(uart_driver_install(NMEA_UART_PORT_NUM, NMEA_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(NMEA_UART_PORT_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(NMEA_UART_PORT_NUM, NMEA_TXD_PIN, NMEA_RXD_PIN, NMEA_TEST_RTS, NMEA_TEST_CTS));  

    uint8_t buf[SCAN_BUF_SIZE];
    int total = 0;
    int64_t start = esp_timer_get_time();

    while ((esp_timer_get_time() - start) < (SCAN_WINDOW_MS * 1000)) {
        int len = uart_read_bytes(SCAN_UART_PORT, buf + total,
                                   SCAN_BUF_SIZE - total - 1, pdMS_TO_TICKS(50));
        if (len > 0) {
            total += len;
            if (total >= SCAN_BUF_SIZE - 1) break;
        }
    }

    ESP_LOGI(SCAN_TAG, "Baud %d: ricevuti %d byte", baud, total);

    bool found_nmea = false;
    bool found_ubx = false;

    for (int i = 0; i < total; i++) {
        if (buf[i] == '$' && !found_nmea) {
            // controlla che seguano caratteri ASCII stampabili plausibili (talker id)
            if (i + 5 < total &&
                isalpha((int)buf[i+1]) && isalpha((int)buf[i+2]) &&
                isalpha((int)buf[i+3]) && isalpha((int)buf[i+4]) &&
                isalpha((int)buf[i+5])) {
                found_nmea = true;
                ESP_LOGI(SCAN_TAG, "  -> possibile NMEA a offset %d: %.10s", i, &buf[i]);
            }
        }
        if (i + 1 < total && buf[i] == 0xB5 && buf[i+1] == 0x62 && !found_ubx) {
            found_ubx = true;
            ESP_LOGI(SCAN_TAG, "  -> possibile UBX sync a offset %d", i);
        }
    }

    uart_driver_delete(SCAN_UART_PORT);

    if (found_nmea || found_ubx) {
        ESP_LOGW(SCAN_TAG, "*** BAUD %d SEMBRA VALIDO (NMEA=%d UBX=%d) ***",
                 baud, found_nmea, found_ubx);
        return true;
    }
    return false;
}

void baudrate_autoscan_run(void)
{
    const int candidates[] = { 9600, 19200, 38400, 57600, 115200, 230400 };
    const int n = sizeof(candidates) / sizeof(candidates[0]);

    ESP_LOGW(SCAN_TAG, "=== INIZIO AUTO-SCAN BAUDRATE (UART2, GPIO15/16) ===");

    for (int i = 0; i < n; i++) {
        if (scan_try_baud(candidates[i])) {
            ESP_LOGW(SCAN_TAG, "=== TROVATO: %d baud === (fermare lo scan, usare questo valore)", candidates[i]);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGE(SCAN_TAG, "=== NESSUN BAUDRATE VALIDO TROVATO tra i candidati testati ===");
}
#endif

/**
 * Parse a received byte from serial port to retrieve an NMEA 0183 
 * sentence into *data.
 *
 * Only $GPRMC and $GPGGA are processed; all other sentence types are
 * silently ignored.  
 *
 * @param rx_byte   The received byte from serial port.
 * @return          true  if the sentence was recognised and its checksum
 *                        was valid (data has been updated).
 *                  false if the sentence was ignored, malformed, or had
 *                        a bad checksum (data is unchanged).
 */
static bool uart_parse_data(const char rx_byte, nmea_source_t source)
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

            result = nmea_parse_sentence(nmea_string, source);
            if (result) 
            {
                ESP_LOGD(TAG, "Parsed string: %s", nmea_string);
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
                uart_parse_data(line[i], NMEA_SOURCE_UART);
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

    const uart_config_t uart_cfg = {
        .baud_rate  = NMEA_UART_BAUD_RATE_115200,  // Set baud rate 
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
#if 0
    // 2. Command the GPS module to shift internal speed to 115200 baud
    ESP_LOGI(TAG, "Upgrading GPS hardware baudrate to 115200...");
    send_ubx_cmd(UBX_PRT_115200, sizeof(UBX_PRT_115200));
    vTaskDelay(pdMS_TO_TICKS(200));

    // 3. Update ESP32-S3 UART hardware to match the new speed (115200)
    ESP_ERROR_CHECK(uart_set_baudrate(NMEA_UART_PORT_NUM, 115200));
    vTaskDelay(pdMS_TO_TICKS(500));
#endif
    // 4. Inject Pedestrian profile
    ESP_LOGI(TAG, "Injecting Pedestrian Profile...");
    send_ubx_cmd(UBX_NAV5_PEDESTRIAN, sizeof(UBX_NAV5_PEDESTRIAN));
    vTaskDelay(pdMS_TO_TICKS(150));

    // 5. Set 5Hz execution configurations 
    ESP_LOGI(TAG, "Settings 5Hz update rate configurations and use GPS, GLONASS and GALILEO...");
    send_ubx_cmd(UBX_RATE_5HZ, sizeof(UBX_RATE_5HZ));
    vTaskDelay(pdMS_TO_TICKS(150));

    // 6. Using GPS, GLONASS and GALILEO
    ESP_LOGI(TAG, "Using GPS, GLONASS and GALILEO...");
    send_ubx_cmd(UBX_CFG_GNSS_GPS_GLONASS_GALILEO, sizeof(UBX_CFG_GNSS_GPS_GLONASS_GALILEO));
    vTaskDelay(pdMS_TO_TICKS(150));

    // 7. Enable GGA string
    ESP_LOGI(TAG, "Enable GGA string...");
    send_ubx_cmd(UBX_CFG_MSG_GGA_ON, sizeof(UBX_CFG_MSG_GGA_ON));
    vTaskDelay(pdMS_TO_TICKS(150));

    // 8. Enable RMC string
    ESP_LOGI(TAG, "Enable RMC string...");
    send_ubx_cmd(UBX_CFG_MSG_RMC_ON, sizeof(UBX_CFG_MSG_RMC_ON));
    vTaskDelay(pdMS_TO_TICKS(150));

    // 9. Disable GLL string
    ESP_LOGI(TAG, "Disable GLL string...");
    send_ubx_cmd(UBX_CFG_MSG_GLL_OFF, sizeof(UBX_CFG_MSG_GLL_OFF));
    vTaskDelay(pdMS_TO_TICKS(150));

    // 10. Disable GSA string
    ESP_LOGI(TAG, "Disable GSA string...");
    send_ubx_cmd(UBX_CFG_MSG_GSA_OFF, sizeof(UBX_CFG_MSG_GSA_OFF));
    vTaskDelay(pdMS_TO_TICKS(150));

    // 11. Disable GSV string
    ESP_LOGI(TAG, "Disable GSV string...");
    send_ubx_cmd(UBX_CFG_MSG_GSV_OFF, sizeof(UBX_CFG_MSG_GSV_OFF));
    vTaskDelay(pdMS_TO_TICKS(150));

    // 12. Disable TVG string
    ESP_LOGI(TAG, "Disable GSV string...");
    send_ubx_cmd(UBX_CFG_MSG_TVG_OFF, sizeof(UBX_CFG_MSG_TVG_OFF));
    vTaskDelay(pdMS_TO_TICKS(150));

    ESP_LOGI(TAG, "U-blox configuration successful. Starting GPS Task loop...");
    
    xTaskCreate(uart_reader_task, "NMEA UART Reader Task", NMEA_TASK_STACK_SIZE,
                NULL, NMEA_TASK_PRIO, NULL);

    ESP_LOGI(TAG, "Initialised NMEA UART Reader Task");
}
