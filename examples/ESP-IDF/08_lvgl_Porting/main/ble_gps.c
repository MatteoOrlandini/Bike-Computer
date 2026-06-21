/*
 * ble_gps.c  —  BLE GATT server, NMEA sentence receiver
 *
 * The smartphone acts as GATT client and writes plain NMEA 0183 sentences
 * (e.g. "$GPRMC,...\r\n", "$GPGGA,...\r\n") to the characteristic below.
 * Each write carries exactly one sentence. The shared nmea_parser processes
 * it and updates the shared nmea_data_t, which any task can read via
 * ble_gps_get_data().
 *
 * Service UUID      : 12345678-1234-1234-1234-123456789abc
 * Characteristic UUID: 12345678-1234-1234-1234-123456789abd  (Write / Write-no-rsp)
 *
 * Connection state is exposed via ble_gps_is_connected() so the priority
 * logic (BLE > UART) can be implemented at a higher layer.
 */

#include "ble_gps.h"
#include "nmea_parser.h"

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* NimBLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define TAG "BLE_GPS"

/* ------------------------------------------------------------------ */
/*  UUIDs  (unchanged)                                                  */
/* ------------------------------------------------------------------ */

/* 128-bit service UUID: 12345678-1234-1234-1234-123456789abc */
static const ble_uuid128_t gps_svc_uuid =
    BLE_UUID128_INIT(0xbc,0x9a,0x78,0x56,0x34,0x12,
                     0x34,0x12,0x34,0x12,
                     0x34,0x12,
                     0x78,0x56,0x34,0x12);

/* 128-bit characteristic UUID: 12345678-1234-1234-1234-123456789abd */
static const ble_uuid128_t gps_chr_uuid =
    BLE_UUID128_INIT(0xbd,0x9a,0x78,0x56,0x34,0x12,
                     0x34,0x12,0x34,0x12,
                     0x34,0x12,
                     0x78,0x56,0x34,0x12);

/* ------------------------------------------------------------------ */
/*  Shared state                                                        */
/* ------------------------------------------------------------------ */

static nmea_data_t       s_nmea_data;
static SemaphoreHandle_t s_mutex;
static bool              s_connected = false;

/* ------------------------------------------------------------------ */
/*  GATT characteristic access callback                                 */
/* ------------------------------------------------------------------ */

static int gps_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }

    /* Copy the incoming mbuf into a null-terminated string */
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len == 0 || len > 127) {
        ESP_LOGW(TAG, "Unexpected write length: %u", len);
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    /*
    else{
        ESP_LOGW(TAG, "Write length: %u", len);
    }
    */

    char sentence[128];
    ble_hs_mbuf_to_flat(ctxt->om, sentence, len, NULL);
    sentence[len] = '\0';

    /* Feed into the shared NMEA parser */
    nmea_data_t tmp;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    tmp = s_nmea_data;                          /* start from current state */
    xSemaphoreGive(s_mutex);

    if (nmea_parse_sentence(sentence, &tmp)) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_nmea_data = tmp;
        xSemaphoreGive(s_mutex);
        ESP_LOGI(TAG, "Parsed: lat=%.6f lon=%.6f spd=%.1f alt=%.1f",
                 tmp.latitude, tmp.longitude, tmp.speed_kmh, tmp.altitude_m);
        /*
        ESP_LOGI(TAG, "Parsed: lat=%.6f lon=%.6f spd=%.1f alt=%.1f",
                 tmp.latitude, tmp.longitude, tmp.speed_kmh, tmp.altitude_m);
        */
    } else {
        ESP_LOGW(TAG, "Failed to parse: %s", sentence);
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  GATT service table  (unchanged)                                     */
/* ------------------------------------------------------------------ */

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gps_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid      = &gps_chr_uuid.u,
                .access_cb = gps_chr_access,
                .flags     = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 }
        },
    },
    { 0 }
};

/* ------------------------------------------------------------------ */
/*  GAP event handler                                                   */
/* ------------------------------------------------------------------ */

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_connected = true;
            ESP_LOGI(TAG, "Phone connected");
            /* Request larger MTU so full NMEA sentences fit in one write */
            ble_gattc_exchange_mtu(event->connect.conn_handle, NULL, NULL);
        } else {
            s_connected = false;
            /* connection failed — restart advertising */
            extern void ble_gps_start_advertising(void);
            ble_gps_start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        s_connected = false;
        ESP_LOGI(TAG, "Phone disconnected (reason %d)",
                 event->disconnect.reason);
        /* restart advertising so the phone can reconnect */
        extern void ble_gps_start_advertising(void);
        ble_gps_start_advertising();
        break;

    default:
        break;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Advertising                                                         */
/* ------------------------------------------------------------------ */

void ble_gps_start_advertising(void)
{
    struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };

    struct ble_hs_adv_fields fields = {
        .flags            = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
        .name             = (uint8_t *)"BikeGPS",
        .name_len         = 7,
        .name_is_complete = 1,
    };

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) { ESP_LOGE(TAG, "adv_set_fields: %d", rc); return; }

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start: %d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising as \"BikeGPS\"");
    }
}

/* ------------------------------------------------------------------ */
/*  NimBLE host callbacks                                               */
/* ------------------------------------------------------------------ */

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    ble_gps_start_advertising();
}

static void on_reset(int reason)
{
    s_connected = false;
    ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
}

/* ------------------------------------------------------------------ */
/*  NimBLE host task                                                    */
/* ------------------------------------------------------------------ */

static void nimble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void ble_gps_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    assert(s_mutex);
    memset(&s_nmea_data, 0, sizeof(s_nmea_data));
    s_connected = false;

    /* NVS required by NimBLE */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(nimble_port_init());

    ble_hs_cfg.sync_cb  = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    int rc = ble_gatts_count_cfg(s_gatt_svcs);
    assert(rc == 0);
    rc = ble_gatts_add_svcs(s_gatt_svcs);
    assert(rc == 0);

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("BikeGPS");

    ble_att_set_preferred_mtu(185);  /* enough for the longest NMEA sentence */

    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "BLE GPS service initialised");
}

bool ble_gps_get_data(nmea_data_t *out)
{
    if (!out) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_nmea_data;
    xSemaphoreGive(s_mutex);
    return out->valid;
}

bool ble_gps_is_connected(void)
{
    return s_connected;
}