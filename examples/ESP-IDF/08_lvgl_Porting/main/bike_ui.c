#include "bike_ui.h"
#include "trip_computer.h"
#include "lvgl.h"
#include "waveshare_rgb_lcd_port.h"
#include <stdio.h>

static lv_obj_t *label_speed;
static lv_obj_t *label_time;
static lv_obj_t *label_avg;
static lv_obj_t *label_total;
static lv_obj_t *label_gradient;
static lv_obj_t *label_ascent;
static lv_obj_t *dot_ble;
static lv_obj_t *dot_gps;
static lv_obj_t *label_ble_status;
static lv_obj_t *label_gps_status;

/* ------------------------------------------------------------------ */
/*  Reset button callback                                               */
/* ------------------------------------------------------------------ */

static void reset_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        trip_computer_reset();
    }
}

static void power_off_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        wavesahre_rgb_lcd_bl_off();
    }
}

/* ------------------------------------------------------------------ */
/*  LVGL timer — fires every second on the LVGL task                   */
/* ------------------------------------------------------------------ */

static void bike_ui_timer_cb(lv_timer_t *timer)
{
    ui_timer_data_t *d = (ui_timer_data_t *)timer->user_data;
    bike_ui_update(d->trip, d->status);
}

/* ------------------------------------------------------------------ */
/*  Init                                                                */
/* ------------------------------------------------------------------ */

void bike_ui_init(trip_data_t *data, ui_status_t *status)
{
    static ui_timer_data_t s_timer_data;   /* static lifetime — safe for timer */
    s_timer_data.trip   = data;
    s_timer_data.status = status;
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // --- BLE status indicator (top-left) ---
    dot_ble = lv_obj_create(scr);
    lv_obj_set_size(dot_ble, 16, 16);
    lv_obj_set_style_radius(dot_ble, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot_ble, 0, 0);
    lv_obj_set_style_bg_color(dot_ble, lv_color_hex(0xFF0000), 0);
    lv_obj_align(dot_ble, LV_ALIGN_TOP_LEFT, 20, 14);

    label_ble_status = lv_label_create(scr);
    lv_obj_set_style_text_font(label_ble_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_ble_status, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(label_ble_status, "BLE OFF");
    lv_obj_align(label_ble_status, LV_ALIGN_TOP_LEFT, 44, 14);

    // --- GPS UART status indicator (top-left) ---
    dot_gps = lv_obj_create(scr);
    lv_obj_set_size(dot_gps, 16, 16);
    lv_obj_set_style_radius(dot_gps, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot_gps, 0, 0);
    lv_obj_set_style_bg_color(dot_gps, lv_color_hex(0xFF0000), 0);
    lv_obj_align(dot_gps, LV_ALIGN_TOP_LEFT, 20, 34);

    label_gps_status = lv_label_create(scr);
    lv_obj_set_style_text_font(label_gps_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_gps_status, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(label_gps_status, "GPS OFF");
    lv_obj_align(label_gps_status, LV_ALIGN_TOP_LEFT, 44, 34);

    // --- Current speed (large, top center) ---
    lv_obj_t *lbl_spd_title = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_spd_title, &lv_font_montserrat_36, 0);
    lv_label_set_text(lbl_spd_title, "Velocita' [km/h]");
    lv_obj_set_style_text_color(lbl_spd_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_spd_title, LV_ALIGN_TOP_MID, 0, 10);

    label_speed = lv_label_create(scr);
    lv_obj_set_style_text_font(label_speed, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(label_speed, lv_color_white(), 0);
    lv_label_set_text(label_speed, "--.-");
    lv_obj_align(label_speed, LV_ALIGN_TOP_MID, 0, 50);

    // --- Elapsed time ---
    lv_obj_t *lbl_time_title = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_time_title, &lv_font_montserrat_36, 0);
    lv_label_set_text(lbl_time_title, "Tempo");
    lv_obj_set_style_text_color(lbl_time_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_time_title, LV_ALIGN_TOP_LEFT, 20, 140);

    label_time = lv_label_create(scr);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(label_time, lv_color_white(), 0);
    lv_label_set_text(label_time, "00:00:00");
    lv_obj_align(label_time, LV_ALIGN_TOP_LEFT, 20, 180);

    // --- Avg speed ---
    lv_obj_t *lbl_avg_title = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_avg_title, &lv_font_montserrat_36, 0);
    lv_label_set_text(lbl_avg_title, "Velocita' media [km/h]");
    lv_obj_set_style_text_color(lbl_avg_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_avg_title, LV_ALIGN_TOP_RIGHT, -20, 140);

    label_avg = lv_label_create(scr);
    lv_obj_set_style_text_font(label_avg, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(label_avg, lv_color_white(), 0);
    lv_label_set_text(label_avg, "--.-");
    lv_obj_align(label_avg, LV_ALIGN_TOP_RIGHT, -20, 180);

    // --- Total km ---
    lv_obj_t *lbl_total_title = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_total_title, &lv_font_montserrat_36, 0);
    lv_label_set_text(lbl_total_title, "Distanza [km]");
    lv_obj_set_style_text_color(lbl_total_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_total_title, LV_ALIGN_TOP_LEFT, 20, 260);

    label_total = lv_label_create(scr);
    lv_obj_set_style_text_font(label_total, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(label_total, lv_color_white(), 0);
    lv_label_set_text(label_total, "0.000");
    lv_obj_align(label_total, LV_ALIGN_TOP_LEFT, 20, 300);

    // --- Gradient ---
    lv_obj_t *lbl_grad_title = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_grad_title, &lv_font_montserrat_36, 0);
    lv_label_set_text(lbl_grad_title, "Pendenza %");
    lv_obj_set_style_text_color(lbl_grad_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_grad_title, LV_ALIGN_TOP_MID, 0, 260);

    label_gradient = lv_label_create(scr);
    lv_obj_set_style_text_font(label_gradient, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(label_gradient, lv_color_white(), 0);
    lv_label_set_text(label_gradient, "0.0");
    lv_obj_align(label_gradient, LV_ALIGN_TOP_MID, 0, 300);

    // --- Ascent ---
    lv_obj_t *lbl_ascent_title = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_ascent_title, &lv_font_montserrat_36, 0);
    lv_label_set_text(lbl_ascent_title, "Ascesa [m]");
    lv_obj_set_style_text_color(lbl_ascent_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_ascent_title, LV_ALIGN_TOP_RIGHT, -20, 260);

    label_ascent = lv_label_create(scr);
    lv_obj_set_style_text_font(label_ascent, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(label_ascent, lv_color_white(), 0);
    lv_label_set_text(label_ascent, "0");
    lv_obj_align(label_ascent, LV_ALIGN_TOP_RIGHT, -20, 300);

    // --- Reset button (bottom center) ---
    lv_obj_t *btn_reset = lv_btn_create(scr);
    lv_obj_set_size(btn_reset, 160, 60);
    lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(btn_reset, reset_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn_reset);
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_36, 0);
    lv_label_set_text(btn_label, "Reset");
    lv_obj_center(btn_label);

    // --- Power off button (bottom right) ---
    lv_obj_t *btn_power_off = lv_btn_create(scr);
    lv_obj_set_size(btn_power_off, 160, 60);
    lv_obj_align(btn_power_off, LV_ALIGN_BOTTOM_RIGHT, 0, -10);
    lv_obj_add_event_cb(btn_power_off, power_off_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_power_off_label = lv_label_create(btn_power_off);
    lv_obj_set_style_text_font(btn_power_off_label, &lv_font_montserrat_32, 0);
    lv_label_set_text(btn_power_off_label, "Stand by");
    lv_obj_center(btn_power_off_label);

    // Start the 1-second refresh timer
    lv_timer_create(bike_ui_timer_cb, 1000, &s_timer_data);
}

/* ------------------------------------------------------------------ */
/*  Update                                                              */
/* ------------------------------------------------------------------ */

void bike_ui_update(const trip_data_t *data, const ui_status_t *status)
{
    char buf[32];
    
    /* --- BLE indicator --- */
    if (status) {
        switch (status->ble) {
            case BLE_STATUS_OFF:
                lv_obj_set_style_bg_color(dot_ble, lv_color_hex(0xFF0000), 0);
                lv_label_set_text(label_ble_status, "BLE OFF");
                break;
            case BLE_STATUS_ON:
                lv_obj_set_style_bg_color(dot_ble, lv_color_hex(0xFF8800), 0);
                lv_label_set_text(label_ble_status, "BLE ON");
                break;
            case BLE_STATUS_CONNECTED:
                lv_obj_set_style_bg_color(dot_ble, lv_color_hex(0x00FF00), 0);
                lv_label_set_text(label_ble_status, "BLE CONN");
                break;
        }

        switch (status->gps_uart) {
            case GPS_UART_STATUS_OFF:
                lv_obj_set_style_bg_color(dot_gps, lv_color_hex(0xFF0000), 0);
                lv_label_set_text(label_gps_status, "GPS OFF");
                break;
            case GPS_UART_STATUS_INVALID:
                lv_obj_set_style_bg_color(dot_gps, lv_color_hex(0xFF8800), 0);
                lv_label_set_text(label_gps_status, "GPS NO FIX");
                break;
            case GPS_UART_STATUS_VALID:
                lv_obj_set_style_bg_color(dot_gps, lv_color_hex(0x00FF00), 0);
                lv_label_set_text(label_gps_status, "GPS OK");
                break;
        }
    }

    if (!data || !data->valid) {
        lv_label_set_text(label_speed,    "--.-");
        lv_label_set_text(label_time,     "--:--:--");
        lv_label_set_text(label_avg,      "--.-");
        lv_label_set_text(label_total,    "--.---");
        lv_label_set_text(label_gradient, "--.-");
        lv_label_set_text(label_ascent,   "---");
        return;
    }

    // Speed
    snprintf(buf, sizeof(buf), "%.1f", data->speed_kmh);
    lv_label_set_text(label_speed, buf);

    // Elapsed time from trip computer (seconds since reset)
    uint32_t s = data->elapsed_sec;
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
             (unsigned long)(s / 3600),
             (unsigned long)((s % 3600) / 60),
             (unsigned long)(s % 60));
    lv_label_set_text(label_time, buf);

    // Average speed
    snprintf(buf, sizeof(buf), "%.1f", data->avg_speed_kmh);
    lv_label_set_text(label_avg, buf);

    // Total distance
    snprintf(buf, sizeof(buf), "%.3f", data->distance_km);
    lv_label_set_text(label_total, buf);

    // Gradient
    snprintf(buf, sizeof(buf), "%.1f", data->gradient_pct);
    lv_label_set_text(label_gradient, buf);

    // Ascent
    snprintf(buf, sizeof(buf), "%.0f", data->ascent_m);
    lv_label_set_text(label_ascent, buf);
}