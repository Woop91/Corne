/* SPDX-License-Identifier: MIT */

#include <stdio.h>
#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/atomic.h>
#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>

#define OLED_WIDTH DT_PROP(DT_CHOSEN(zephyr_display), width)

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zephyr/bluetooth/conn.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#else
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#endif

static struct {
    uint8_t local_battery;
    bool usb_powered;
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    uint8_t right_battery;
    bool right_known;
    uint8_t profile;
    uint8_t layer;
    bool profile_bonded;
    bool profile_connected;
    bool usb_output;
    char profile_name[16];
#else
    bool linked;
#endif
} state;

K_MUTEX_DEFINE(state_mutex);
static atomic_t screen_ready;
static lv_obj_t *rows[3];

// Called with state_mutex held. Do not change the cached remote battery here.
static void refresh_common_state(void) {
    state.usb_powered = zmk_usb_is_powered();
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    static const char *const labels[] = {
        CONFIG_CORNE_BT_PROFILE_1_LABEL, CONFIG_CORNE_BT_PROFILE_2_LABEL,
        CONFIG_CORNE_BT_PROFILE_3_LABEL, CONFIG_CORNE_BT_PROFILE_4_LABEL,
        CONFIG_CORNE_BT_PROFILE_5_LABEL,
    };
    state.profile = zmk_ble_active_profile_index();
    state.layer = zmk_keymap_highest_layer_active();
    state.profile_bonded = !zmk_ble_active_profile_is_open();
    state.profile_connected = zmk_ble_active_profile_is_connected();
    state.usb_output = zmk_endpoints_selected().transport == ZMK_TRANSPORT_USB;
    const char *name = state.profile < ARRAY_SIZE(labels) ? labels[state.profile] : "";
    if (name[0] == '\0') {
        name = zmk_ble_active_profile_name();
    }
    snprintf(state.profile_name, sizeof(state.profile_name), "%.15s",
             name != NULL && name[0] != '\0' ? name : "Unnamed");
#else
    state.linked = zmk_split_bt_peripheral_is_connected();
#endif
}

static void render_status(struct k_work *work) {
    ARG_UNUSED(work);
    char top[17], middle[17], bottom[17];
    k_mutex_lock(&state_mutex, K_FOREVER);
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#if OLED_WIDTH < 128
    snprintf(top, sizeof(top), "L%u%%%s", (unsigned)state.local_battery,
             state.usb_powered ? "+" : "");
    if (state.right_known) {
        snprintf(middle, sizeof(middle), "R%u%%", (unsigned)state.right_battery);
    } else {
        snprintf(middle, sizeof(middle), "R--");
    }
    snprintf(bottom, sizeof(bottom), "BT%c %.4s", '1' + MIN(state.profile, 4),
             state.profile_name);
#else
    if (state.right_known) {
        snprintf(top, sizeof(top), "L%u%%%s R%u%%", (unsigned)state.local_battery,
                 state.usb_powered ? "+" : "", (unsigned)state.right_battery);
    } else {
        snprintf(top, sizeof(top), "L%u%%%s R--", (unsigned)state.local_battery,
                 state.usb_powered ? "+" : "");
    }
    snprintf(middle, sizeof(middle), "BT%u %.10s", (unsigned)state.profile + 1, state.profile_name);
    snprintf(bottom, sizeof(bottom), "%s %s L%u", state.usb_output ? "USB" : "BLE",
             !state.profile_bonded ? "NEW" : (state.profile_connected ? "ON" : "OFF"),
             (unsigned)state.layer);
#endif
#else
    snprintf(top, sizeof(top), "R%u%%%s", (unsigned)state.local_battery,
             state.usb_powered ? "+" : "");
    snprintf(middle, sizeof(middle), "%s", state.linked ? "LINKED" : "NO LINK");
    snprintf(bottom, sizeof(bottom), "%s",
             state.usb_powered ? (OLED_WIDTH < 128 ? "USB" : "USB POWER") : "BATTERY");
#endif
    k_mutex_unlock(&state_mutex);
    lv_label_set_text(rows[0], top);
    lv_label_set_text(rows[1], middle);
    lv_label_set_text(rows[2], bottom);
}
K_WORK_DEFINE(render_work, render_status);

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static void find_connected_right(struct bt_conn *conn, void *user_data) {
    struct bt_conn_info info;
    if (bt_conn_get_info(conn, &info) == 0 && info.role == BT_CONN_ROLE_CENTRAL &&
        info.state == BT_CONN_STATE_CONNECTED) {
        *(bool *)user_data = true;
    }
}

static bool right_connected(void) {
    bool connected = false;
    bt_conn_foreach(BT_CONN_TYPE_LE, find_connected_right, &connected);
    return connected;
}
#endif

static int oled_event_listener(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *local = as_zmk_battery_state_changed(eh);
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    const struct zmk_peripheral_battery_state_changed *remote =
        as_zmk_peripheral_battery_state_changed(eh);
    bool valid_remote = remote != NULL && remote->source == 0 &&
                        (remote->state_of_charge != 0 || right_connected());
#endif
    k_mutex_lock(&state_mutex, K_FOREVER);
    if (local != NULL) {
        state.local_battery = MIN(local->state_of_charge, 100);
    }
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    // Ignore ZMK v0.3.0's synthetic zero on disconnect, but accept a live 0%.
    if (valid_remote) {
        state.right_battery = MIN(remote->state_of_charge, 100);
        state.right_known = true;
    }
#endif
    refresh_common_state();
    k_mutex_unlock(&state_mutex);
    if (atomic_get(&screen_ready)) {
        k_work_submit_to_queue(zmk_display_work_q(), &render_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(corne_oled_status, oled_event_listener);
ZMK_SUBSCRIPTION(corne_oled_status, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(corne_oled_status, zmk_usb_conn_state_changed);
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
ZMK_SUBSCRIPTION(corne_oled_status, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(corne_oled_status, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(corne_oled_status, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(corne_oled_status, zmk_layer_state_changed);
#else
ZMK_SUBSCRIPTION(corne_oled_status, zmk_split_peripheral_status_changed);
#endif

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(screen, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(screen, &lv_font_unscii_8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    for (int i = 0; i < 3; ++i) {
        rows[i] = lv_label_create(screen);
        lv_obj_set_width(rows[i], OLED_WIDTH);
        lv_label_set_long_mode(rows[i], LV_LABEL_LONG_CLIP);
        lv_obj_set_pos(rows[i], 0, i * 11);
    }
    k_mutex_lock(&state_mutex, K_FOREVER);
    state.local_battery = MIN(zmk_battery_state_of_charge(), 100);
    refresh_common_state();
    k_mutex_unlock(&state_mutex);
    atomic_set(&screen_ready, 1);
    render_status(NULL);
    return screen;
}
