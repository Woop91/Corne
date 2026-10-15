/*
 *
 * Copyright (c) 2025 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include "status.h"
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool profiles_connected[NICEVIEW_PROFILE_COUNT];
    bool profiles_bonded[NICEVIEW_PROFILE_COUNT];
    char profile_name[32];
};

struct layer_status_state {
    zmk_keymap_layer_index_t index;
    const char *label;
};

// Battery rows occupy the space formerly used by the WPM graph.
static void draw_battery_row(lv_obj_t *canvas, int y, const char *side,
                             uint8_t level, bool known) {
    lv_draw_label_dsc_t label;
    init_label_dsc(&label, LVGL_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_LEFT);
    lv_draw_rect_dsc_t foreground, background;
    init_rect_dsc(&foreground, LVGL_FOREGROUND);
    init_rect_dsc(&background, LVGL_BACKGROUND);

    lv_canvas_draw_text(canvas, 0, y + 2, 8, &label, side);
    lv_canvas_draw_rect(canvas, 10, y, 20, 12, &foreground);
    lv_canvas_draw_rect(canvas, 11, y + 1, 18, 10, &background);
    lv_canvas_draw_rect(canvas, 30, y + 3, 2, 6, &foreground);
    if (known && level > 0) {
        lv_canvas_draw_rect(canvas, 12, y + 2, MAX(1, MIN(level, 100) * 16 / 100),
                            8, &foreground);
    }
    char percentage[5];
    if (known) {
        snprintf(percentage, sizeof(percentage), "%u%%", (unsigned)MIN(level, 100));
    } else {
        strcpy(percentage, "--");
    }
    lv_canvas_draw_text(canvas, 36, y + 2, 32, &label, percentage);
}

static void draw_top(struct zmk_widget_status *widget) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 0);
    const struct status_state *state = &widget->state;
    lv_draw_rect_dsc_t background;
    init_rect_dsc(&background, LVGL_BACKGROUND);
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &background);

    draw_battery_row(canvas, 2, "L", state->battery, true);
    draw_battery_row(canvas, 24, "R", widget->right_battery, widget->right_battery_known);

    // Keep the selected USB/Bluetooth endpoint below both battery rows.
    lv_draw_label_dsc_t label;
    init_label_dsc(&label, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
    const char *output = "";
    switch (state->selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        output = LV_SYMBOL_USB;
        break;
    case ZMK_TRANSPORT_BLE:
        output = state->active_profile_bonded
                     ? (state->active_profile_connected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE)
                     : LV_SYMBOL_SETTINGS;
        break;
    }
    lv_canvas_draw_text(canvas, 0, 46, CANVAS_SIZE, &label, output);
    rotate_canvas(canvas, widget->cbuf);
}

static void draw_middle(struct zmk_widget_status *widget) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 1);
    const struct status_state *state = &widget->state;

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
    lv_draw_label_dsc_t small_label;
    init_label_dsc(&small_label, LVGL_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_CENTER);

    // Fill background
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    char profile[12];
    snprintf(profile, sizeof(profile), "BT %u", (unsigned)state->active_profile_index + 1);
    lv_canvas_draw_text(canvas, 0, 0, 68, &label_dsc, profile);

    // At eight pixels per character, two explicit lines fit the 68-pixel width.
    char name_line[9];
    for (int line = 0; line < 2; ++line) {
        size_t offset = line * 8;
        size_t length = strlen(widget->profile_name);
        snprintf(name_line, sizeof(name_line), "%.8s",
                 offset < length ? widget->profile_name + offset : "");
        lv_canvas_draw_text(canvas, 2, 22 + line * 10, 64, &small_label, name_line);
    }
    const char *connection = !state->active_profile_bonded
                                 ? "UNPAIRED"
                                 : (state->active_profile_connected ? "ONLINE" : "OFFLINE");
    lv_canvas_draw_text(canvas, 2, 50, 64, &small_label, connection);

    // Rotate canvas
    rotate_canvas(canvas, widget->cbuf2);
}

static void draw_bottom(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 2);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);

    // Fill background
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    // Draw layer
    if (state->layer_label == NULL || strlen(state->layer_label) == 0) {
        char text[10] = {};

        sprintf(text, "LAYER %i", state->layer_index);

        lv_canvas_draw_text(canvas, 0, 5, 68, &label_dsc, text);
    } else {
        lv_canvas_draw_text(canvas, 0, 5, 68, &label_dsc, state->layer_label);
    }

    // Rotate canvas
    rotate_canvas(canvas, cbuf);
}

static void set_battery_status(struct zmk_widget_status *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

    widget->state.battery = state.level;

    draw_top(widget);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

static void set_output_status(struct zmk_widget_status *widget,
                              const struct output_status_state *state) {
    widget->state.selected_endpoint = state->selected_endpoint;
    widget->state.active_profile_index = state->active_profile_index;
    widget->state.active_profile_connected = state->active_profile_connected;
    widget->state.active_profile_bonded = state->active_profile_bonded;
    memcpy(widget->profile_name, state->profile_name, sizeof(widget->profile_name));
    for (int i = 0; i < NICEVIEW_PROFILE_COUNT; ++i) {
        widget->state.profiles_connected[i] = state->profiles_connected[i];
        widget->state.profiles_bonded[i] = state->profiles_bonded[i];
    }

    draw_top(widget);
    draw_middle(widget);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_output_status(widget, &state); }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    struct output_status_state state = {
        .selected_endpoint = zmk_endpoints_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
    for (int i = 0; i < MIN(NICEVIEW_PROFILE_COUNT, ZMK_BLE_PROFILE_COUNT); ++i) {
        state.profiles_connected[i] = zmk_ble_profile_is_connected(i);
        state.profiles_bonded[i] = !zmk_ble_profile_is_open(i);
    }
    static const char *const labels[NICEVIEW_PROFILE_COUNT] = {
        CONFIG_CORNE_BT_PROFILE_1_LABEL, CONFIG_CORNE_BT_PROFILE_2_LABEL,
        CONFIG_CORNE_BT_PROFILE_3_LABEL, CONFIG_CORNE_BT_PROFILE_4_LABEL,
        CONFIG_CORNE_BT_PROFILE_5_LABEL,
    };
    const char *name = zmk_ble_active_profile_name();
    if (name == NULL || name[0] == '\0') {
        name = state.active_profile_index >= 0 && state.active_profile_index < NICEVIEW_PROFILE_COUNT
                   ? labels[state.active_profile_index] : "";
    }
    // Do not confuse the keyboard's advertised name with the paired host's name.
    snprintf(state.profile_name, sizeof(state.profile_name), "%.15s",
             name[0] != '\0' ? name : "Unnamed");
    return state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

static void set_layer_status(struct zmk_widget_status *widget, struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;

    draw_bottom(widget->obj, widget->cbuf3, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_status(widget, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){
        .index = index, .label = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index))};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

// Cache reports even before display initialization. All LVGL work is queued
// onto the display thread; 0% remains distinct from not having a report yet.
static struct {
    uint8_t level;
    bool known;
} right_battery;
K_MUTEX_DEFINE(right_battery_mutex);

static void right_battery_update(struct k_work *work) {
    ARG_UNUSED(work);
    k_mutex_lock(&right_battery_mutex, K_FOREVER);
    uint8_t level = right_battery.level;
    bool known = right_battery.known;
    k_mutex_unlock(&right_battery_mutex);

    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->right_battery = level;
        widget->right_battery_known = known;
        draw_top(widget);
    }
}
K_WORK_DEFINE(right_battery_work, right_battery_update);

static int right_battery_listener(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    // This Corne has one peripheral: the right half occupies source 0.
    if (ev == NULL || ev->source != 0) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    k_mutex_lock(&right_battery_mutex, K_FOREVER);
    right_battery.level = ev->state_of_charge;
    right_battery.known = true;
    k_mutex_unlock(&right_battery_mutex);
    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &right_battery_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(corne_right_battery, right_battery_listener);
ZMK_SUBSCRIPTION(corne_right_battery, zmk_peripheral_battery_state_changed);

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);
    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, widget->cbuf, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);
    lv_obj_t *middle = lv_canvas_create(widget->obj);
    lv_obj_align(middle, LV_ALIGN_TOP_LEFT, 24, 0);
    lv_canvas_set_buffer(middle, widget->cbuf2, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);
    lv_obj_t *bottom = lv_canvas_create(widget->obj);
    lv_obj_align(bottom, LV_ALIGN_TOP_LEFT, -44, 0);
    lv_canvas_set_buffer(bottom, widget->cbuf3, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);

    sys_slist_append(&widgets, &widget->node);
    widget_battery_status_init();
    widget_output_status_init();
    widget_layer_status_init();
    right_battery_update(NULL);

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
