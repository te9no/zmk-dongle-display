/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <lvgl.h>

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/usb.h>

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#define SOURCE_OFFSET 1
#else
#define SOURCE_OFFSET 0
#endif

#ifndef ZMK_SPLIT_BLE_PERIPHERAL_COUNT
#define ZMK_SPLIT_BLE_PERIPHERAL_COUNT 0
#endif

#define RAW_BATTERY_SOURCE_COUNT (ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET)
#if RAW_BATTERY_SOURCE_COUNT > 0
#define BATTERY_SOURCE_COUNT RAW_BATTERY_SOURCE_COUNT
#else
#define BATTERY_SOURCE_COUNT 1
#endif

struct battery_only_widget {
    sys_snode_t node;
    lv_obj_t *obj;
};

struct battery_state {
    uint8_t source;
    uint8_t level;
    bool usb_present;
};

struct battery_object {
    lv_obj_t *label;
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static struct battery_object battery_objects[BATTERY_SOURCE_COUNT];
static lv_style_t global_style;

static const char *source_label(uint8_t source) {
#if SOURCE_OFFSET == 1
    if (source == 0) {
        return "C";
    }
    return "P";
#else
    return "P";
#endif
}

static void place_battery_label(lv_obj_t *label, uint8_t source) {
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_VERTICAL_BATTERY)
    int32_t x = CONFIG_ZMK_DONGLE_DISPLAY_VERTICAL_BATTERY_LEFT_X;
    if (source > 0) {
        x = CONFIG_ZMK_DONGLE_DISPLAY_VERTICAL_BATTERY_RIGHT_X;
    }

    lv_obj_set_size(label, 56, 8);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(label, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(label, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_width(label, 64, LV_PART_MAIN);
    lv_obj_set_style_transform_height(label, 64, LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(label, CONFIG_ZMK_DONGLE_DISPLAY_VERTICAL_BATTERY_ROTATION,
                                        LV_PART_MAIN);
    lv_obj_set_pos(label, x, 31);
#else
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, source * 12);
#endif
}

static void set_battery_text(lv_obj_t *label, struct battery_state state) {
    if (state.usb_present) {
        lv_label_set_text_fmt(label, "%s USB", source_label(state.source));
    } else {
        lv_label_set_text_fmt(label, "%s %3u%%", source_label(state.source), state.level);
    }
}

static void set_battery_label(struct battery_state state) {
    if (state.source >= BATTERY_SOURCE_COUNT || battery_objects[state.source].label == NULL) {
        return;
    }

    lv_obj_t *label = battery_objects[state.source].label;
    set_battery_text(label, state);

    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(label);
}

static void battery_status_update_cb(struct battery_state state) {
    struct battery_only_widget *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_label(state); }
}

static struct battery_state peripheral_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);

    if (ev == NULL) {
        return (struct battery_state){.source = SOURCE_OFFSET, .level = 0};
    }

    return (struct battery_state){
        .source = ev->source + SOURCE_OFFSET,
        .level = ev->state_of_charge,
    };
}

static struct battery_state central_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

    return (struct battery_state){
        .source = 0,
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}

static struct battery_state battery_status_get_state(const zmk_event_t *eh) {
    if (as_zmk_peripheral_battery_state_changed(eh) != NULL) {
        return peripheral_battery_status_get_state(eh);
    }

    return central_battery_status_get_state(eh);
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_only_status, struct battery_state, battery_status_update_cb,
                            battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_only_status, zmk_peripheral_battery_state_changed);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
ZMK_SUBSCRIPTION(widget_battery_only_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_only_status, zmk_usb_conn_state_changed);
#endif
#endif
#endif

static void battery_only_widget_init(struct battery_only_widget *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);

    for (uint8_t i = 0; i < BATTERY_SOURCE_COUNT; i++) {
        lv_obj_t *label = lv_label_create(widget->obj);
        lv_label_set_text_fmt(label, "%s --%%", source_label(i));
        lv_obj_set_style_text_font(label, &lv_font_unscii_8, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_letter_space(label, 0, LV_PART_MAIN);
        lv_obj_set_style_text_line_space(label, 0, LV_PART_MAIN);
        place_battery_label(label, i);
        battery_objects[i].label = label;
    }

    sys_slist_append(&widgets, &widget->node);
    widget_battery_only_status_init();
}

lv_obj_t *zmk_display_status_screen(void) {
    static struct battery_only_widget battery_widget;

    lv_obj_t *screen = lv_obj_create(NULL);

    lv_style_init(&global_style);
    lv_style_set_bg_color(&global_style, lv_color_white());
    lv_style_set_bg_opa(&global_style, LV_OPA_COVER);
    lv_style_set_text_color(&global_style, lv_color_black());
    lv_style_set_text_font(&global_style, &lv_font_unscii_8);
    lv_style_set_text_letter_space(&global_style, 1);
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);

    battery_only_widget_init(&battery_widget, screen);

    return screen;
}
