/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "waveshare_rgb_lcd_port.h"
#include "nvs_flash.h"
/* BLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "bike_ui.h" 
#include "freertos/task.h"


// static const char *TAG = "bike_computer";
static const char *BLE_DEVICE_NAME = "BikeComputer";

static void ble_advertise(void);

static void ble_on_sync(void)
{
    ble_addr_t addr;
    ble_hs_id_gen_rnd(1, &addr);
    ble_hs_id_set_rnd(addr.val);
    ble_advertise();
}

static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE reset, reason=%d", reason);
}

static void ble_advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)BLE_DEVICE_NAME;
    fields.name_len = strlen(BLE_DEVICE_NAME);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    int rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER,
                               &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "BLE adv start failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "BLE advertising as \"%s\"", BLE_DEVICE_NAME);
    }
}

static void nimble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble: %d", ret);
        return;
    }
    ble_hs_cfg.sync_cb  = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_svc_gap_device_name_set(BLE_DEVICE_NAME);
    nimble_port_freertos_init(nimble_host_task);

    waveshare_esp32_s3_rgb_lcd_init();

    /*
    ESP_LOGI(TAG, "Display LVGL demos");
    if (lvgl_port_lock(-1)) {
#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911
        lv_demo_widgets();
#else
        lv_demo_music();
#endif
        lvgl_port_unlock();
    }
    */
    
    ESP_LOGI(TAG, "Starting bike UI");
    if (lvgl_port_lock(-1)) {
        bike_ui_init();
        lvgl_port_unlock();
    }

    // Update display with random values every second
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (lvgl_port_lock(-1)) {
            bike_ui_update_random();
            lvgl_port_unlock();
        }
    }
}