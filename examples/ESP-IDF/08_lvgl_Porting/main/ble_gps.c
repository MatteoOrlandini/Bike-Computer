/*
 * BLE GPS receiver – NimBLE GATT server
 *
 * The smartphone acts as GATT client and writes GPS fixes to the
 * "GPS Data" characteristic defined below.
 *
 * Service UUID  : 12345678-1234-1234-1234-123456789abc
 * Characteristic: 12345678-1234-1234-1234-123456789abd  (Write / Write-no-response)
 *
 * Payload written by the phone (20 bytes, little-endian):
 *   [0..7]   double  latitude   (IEEE 754 64-bit)
 *   [8..15]  double  longitude  (IEEE 754 64-bit)
 *   [16..19] float   speed_kmh  (IEEE 754 32-bit)
 *
 * Optionally a 24-byte payload adds altitude:
 *   [20..23] float   altitude_m (IEEE 754 32-bit)
 */

#include "ble_gps.h"

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
/*  UUIDs                                                               */
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
/*  Shared GPS state                                                    */
/* ------------------------------------------------------------------ */

static ble_gps_data_t  s_gps_data;
static SemaphoreHandle_t s_mutex;

/* ------------------------------------------------------------------ */
/*  GATT characteristic access callback                                 */
/* ------------------------------------------------------------------ */

static int gps_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }

    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < 20) {
        ESP_LOGW(TAG, "GPS write too short (%u bytes)", len);
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint8_t buf[24] = {0};
    uint16_t copy_len = len < sizeof(buf) ? len : sizeof(buf);
    ble_hs_mbuf_to_flat(ctxt->om, buf, copy_len, NULL);

    ble_gps_data_t tmp;
    memcpy(&tmp.latitude,  buf + 0,  8);
    memcpy(&tmp.longitude, buf + 8,  8);
    memcpy(&tmp.speed_kmh, buf + 16, 4);
    tmp.altitude_m = 0.0f;
    if (len >= 24) {
        memcpy(&tmp.altitude_m, buf + 20, 4);
    }
    tmp.valid = true;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_gps_data = tmp;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "GPS fix: lat=%.6f lon=%.6f spd=%.1f km/h alt=%.1f m",
             tmp.latitude, tmp.longitude, tmp.speed_kmh, tmp.altitude_m);

    return 0;
}

/* ------------------------------------------------------------------ */
/*  GATT service table                                                  */
/* ------------------------------------------------------------------ */

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gps_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid       = &gps_chr_uuid.u,
                .access_cb  = gps_chr_access,
                .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 }   /* sentinel */
        },
    },
    { 0 }   /* sentinel */
};

/* ------------------------------------------------------------------ */
/*  GAP / advertising                                                   */
/* ------------------------------------------------------------------ */

static void start_advertising(void)
{
    struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,   /* undirected connectable */
        .disc_mode = BLE_GAP_DISC_MODE_GEN,   /* general discoverable  */
    };

    struct ble_hs_adv_fields fields = {
        .flags              = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
        .name               = (uint8_t *)"BikeGPS",
        .name_len           = 7,
        .name_is_complete   = 1,
    };

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields error: %d", rc);
        return;
    }

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start error: %d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising as \"BikeGPS\"");
    }
}

/* ------------------------------------------------------------------ */
/*  NimBLE host callbacks                                               */
/* ------------------------------------------------------------------ */

static void on_sync(void)
{
    /* Make sure we have a valid public address */
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    start_advertising();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
}

/* ------------------------------------------------------------------ */
/*  NimBLE host task                                                    */
/* ------------------------------------------------------------------ */

static void nimble_host_task(void *param)
{
    nimble_port_run();          /* blocks until nimble_port_stop() */
    nimble_port_freertos_deinit();
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void ble_gps_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    assert(s_mutex);
    memset(&s_gps_data, 0, sizeof(s_gps_data));

    /* NVS is required by NimBLE for bonding storage */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = nimble_port_init();
    ESP_ERROR_CHECK(err);

    /* Register host callbacks */
    ble_hs_cfg.sync_cb  = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    /* Register GATT services */
    int rc = ble_gatts_count_cfg(s_gatt_svcs);
    assert(rc == 0);
    rc = ble_gatts_add_svcs(s_gatt_svcs);
    assert(rc == 0);

    ble_svc_gap_init();
    ble_svc_gatt_init();

    /* Set device name visible in GAP */
    ble_svc_gap_device_name_set("BikeGPS");

    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "BLE GPS service initialised");
}

bool ble_gps_get_data(ble_gps_data_t *out)
{
    if (!out) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_gps_data;
    xSemaphoreGive(s_mutex);
    return out->valid;
}