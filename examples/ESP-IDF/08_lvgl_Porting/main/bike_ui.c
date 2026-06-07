#include "bike_ui.h"
#include "lvgl.h"
#include "ble_gps.h"
#include <stdlib.h>
#include <stdio.h>

static lv_obj_t *label_speed;
static lv_obj_t *label_time;
static lv_obj_t *label_avg;
static lv_obj_t *label_total;
static lv_obj_t *label_gradient;
static lv_obj_t *label_ascent;

static void bike_ui_timer_cb(lv_timer_t *timer)
{
    bike_ui_update();
}

void bike_ui_init(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // --- Current speed (large, top center) ---
    lv_obj_t *lbl_spd_title = lv_label_create(scr);
    lv_label_set_text(lbl_spd_title, "km/h");
    lv_obj_set_style_text_color(lbl_spd_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_spd_title, LV_ALIGN_TOP_MID, 0, 10);

    label_speed = lv_label_create(scr);
    lv_obj_set_style_text_font(label_speed, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_speed, lv_color_hex(0x00FF00), 0);
    lv_label_set_text(label_speed, "0.0");
    lv_obj_align(label_speed, LV_ALIGN_TOP_MID, 0, 30);

    // --- Elapsed time ---
    lv_obj_t *lbl_time_title = lv_label_create(scr);
    lv_label_set_text(lbl_time_title, "Time");
    lv_obj_set_style_text_color(lbl_time_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_time_title, LV_ALIGN_TOP_LEFT, 20, 140);

    label_time = lv_label_create(scr);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_time, lv_color_white(), 0);
    lv_label_set_text(label_time, "00:00:00");
    lv_obj_align(label_time, LV_ALIGN_TOP_LEFT, 20, 160);

    // --- Avg speed ---
    lv_obj_t *lbl_avg_title = lv_label_create(scr);
    lv_label_set_text(lbl_avg_title, "Avg km/h");
    lv_obj_set_style_text_color(lbl_avg_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_avg_title, LV_ALIGN_TOP_RIGHT, -20, 140);

    label_avg = lv_label_create(scr);
    lv_obj_set_style_text_font(label_avg, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_avg, lv_color_white(), 0);
    lv_label_set_text(label_avg, "0.0");
    lv_obj_align(label_avg, LV_ALIGN_TOP_RIGHT, -20, 160);

    // --- Total km ---
    lv_obj_t *lbl_total_title = lv_label_create(scr);
    lv_label_set_text(lbl_total_title, "Total km");
    lv_obj_set_style_text_color(lbl_total_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_total_title, LV_ALIGN_TOP_LEFT, 20, 260);

    label_total = lv_label_create(scr);
    lv_obj_set_style_text_font(label_total, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_total, lv_color_white(), 0);
    lv_label_set_text(label_total, "0.0");
    lv_obj_align(label_total, LV_ALIGN_TOP_LEFT, 20, 280);

    // --- Gradient ---
    lv_obj_t *lbl_grad_title = lv_label_create(scr);
    lv_label_set_text(lbl_grad_title, "Gradient %");
    lv_obj_set_style_text_color(lbl_grad_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_grad_title, LV_ALIGN_TOP_MID, 0, 260);

    label_gradient = lv_label_create(scr);
    lv_obj_set_style_text_font(label_gradient, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_gradient, lv_color_white(), 0);
    lv_label_set_text(label_gradient, "0.0");
    lv_obj_align(label_gradient, LV_ALIGN_TOP_MID, 0, 280);

    // --- Ascent ---
    lv_obj_t *lbl_ascent_title = lv_label_create(scr);
    lv_label_set_text(lbl_ascent_title, "Ascent m");
    lv_obj_set_style_text_color(lbl_ascent_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_ascent_title, LV_ALIGN_TOP_RIGHT, -20, 260);

    label_ascent = lv_label_create(scr);
    lv_obj_set_style_text_font(label_ascent, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_ascent, lv_color_white(), 0);
    lv_label_set_text(label_ascent, "0");
    lv_obj_align(label_ascent, LV_ALIGN_TOP_RIGHT, -20, 280);

    lv_timer_create(bike_ui_timer_cb, 1000, NULL);
}

void bike_ui_update(void)
{
    ble_gps_data_t gps = {0};
    char buf[32] = {0};

    if (!ble_gps_get_data(&gps)) {
        // No fix yet — show dashes
        lv_label_set_text(label_speed, "--.-");
        lv_label_set_text(label_gradient, "--.-");
        lv_label_set_text(label_ascent, "--.-");
        return;
    }

    snprintf(buf, sizeof(buf), "%.1f", gps.speed_kmh);
    lv_label_set_text(label_speed, buf);

    // Gradient and ascent need altitude history — placeholder for now
    snprintf(buf, sizeof(buf), "%.1f", gps.altitude_m);
    lv_label_set_text(label_gradient, buf);
    
    snprintf(buf, sizeof(buf), "%.1f", gps.longitude);
    lv_label_set_text(label_ascent, buf);
}