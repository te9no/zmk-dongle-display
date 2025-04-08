/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include "widgets/battery_status.h"
#include "widgets/modifiers.h"
#include "widgets/bongo_cat.h"
#include "widgets/layer_status.h"
#include "widgets/output_status.h"
#include "widgets/hid_indicators.h"
#include <lvgl.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_widget_output_status output_status_widget;
static struct zmk_widget_layer_stats layer_status_widget;
static struct zmk_widget_dongle_battery_status dongle_battery_status_widget;
static struct zmk_widget_modifiers modifiers_widget;
static struct zmk_widget_bongo_cat bongo_cat_widget;

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
static struct zmk_widget_hid_indicators hid_indicators_widget;
#endif

lv_style_t global_style;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;
    screen = lv_obj_create(NULL);

    lv_style_init(&global_style);
    lv_style_set_text_font(&global_style, &lv_font_unscii_8);
    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 1);
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);

    // Create a container for widgets
    lv_obj_t *container = lv_obj_create(screen);
    lv_obj_set_size(container, 32, 128);  // サイズを回転後の寸法に合わせる
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_pos(container, 0, 0);  // 位置を左上に固定

    zmk_widget_output_status_init(&output_status_widget, container);
    lv_obj_set_pos(zmk_widget_output_status_obj(&output_status_widget), 0, 0);
    
    zmk_widget_bongo_cat_init(&bongo_cat_widget, container);
    lv_obj_set_pos(zmk_widget_bongo_cat_obj(&bongo_cat_widget), 0, 48);

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, container);
    lv_obj_set_pos(zmk_widget_hid_indicators_obj(&hid_indicators_widget), 0, 96);
#endif

    zmk_widget_layer_stats_init(&layer_status_widget, container);
    lv_obj_set_pos(zmk_widget_layer_stats_obj(&layer_status_widget), 0, 106);

    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, container);
    lv_obj_set_pos(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget), 0, 116);

    // スクリーン全体を回転
    lv_obj_set_style_transform_angle(screen, 900, 0);

    return screen;
}