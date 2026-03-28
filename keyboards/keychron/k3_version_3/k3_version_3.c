/* Copyright 2024 ~ 2025 @ Keychron (https://www.keychron.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "quantum.h"
#include "config.h"
#ifdef BLUETOOTH_ENABLE
#    include "connection.h"
#    include "lkbt51.h"
#endif
#ifdef LK_WIRELESS_ENABLE
#    include "keychron_task.h"
#endif

#define POWER_ON_LED_DURATION 3000
static uint32_t power_on_indicator_timer;

#ifdef BT_INDICATION_LED_PIN_LIST
pin_t bt_led_pins[] = BT_INDICATION_LED_PIN_LIST;
#endif

bool dip_switch_update_kb(uint8_t index, bool active) {
    if (!dip_switch_update_user(index, active)) {
        return false;
    }
    if (index == 0) {
        default_layer_set(1UL << (active ? 0 : 2));
    }
    return true;
}

void keyboard_post_init_kb(void) {
#ifdef BLUETOOTH_ENABLE
    gpio_set_pin_input(BT_MODE_SELECT_PIN);
    gpio_write_pin(BAT_LOW_LED_PIN, BAT_LOW_LED_PIN_ON_STATE);
#endif

    power_on_indicator_timer = timer_read32();

#ifdef ENCODER_ENABLE
    encoder_cb_init();
#endif

#if defined(MCU_UNUSED_PINS) && defined(MCU_UNSED_PINS_STATE)
    pin_t    unused_pins[]       = MCU_UNUSED_PINS;
    uint32_t unused_pins_state[] = MCU_UNSED_PINS_STATE;

    for (uint8_t i = 0; i < ARRAY_SIZE(unused_pins); i++) {
        palSetLineMode(unused_pins[i], unused_pins_state[i]);
    }
#endif

    keyboard_post_init_user();
}

bool keychron_task_kb(void) {
    if (power_on_indicator_timer) {
        if (timer_elapsed32(power_on_indicator_timer) > POWER_ON_LED_DURATION) {
            power_on_indicator_timer = 0;
            gpio_write_pin(BAT_LOW_LED_PIN, !BAT_LOW_LED_PIN_ON_STATE);
        } else {
            gpio_write_pin(BAT_LOW_LED_PIN, BAT_LOW_LED_PIN_ON_STATE);
        }
    }

#ifdef BLUETOOTH_ENABLE
    if (gpio_read_pin(BT_MODE_SELECT_PIN) == 0) {
        connection_set_host_noeeprom(CONNECTION_HOST_BLUETOOTH);
    } else {
        connection_set_host_noeeprom(CONNECTION_HOST_USB);
    }
#endif

    return true;
}

#if 1
void housekeeping_task_kb() {
    keychron_task_kb();
}
#endif

#ifdef LK_WIRELESS_ENABLE
bool lpm_is_kb_idle(void) {
    return power_on_indicator_timer == 0;
}
#endif

#ifdef BLUETOOTH_ENABLE
void connection_host_changed_kb(connection_host_t host) {
    switch (host) {
        case CONNECTION_HOST_USB:
            lkbt51_disconnect();
            break;
        case CONNECTION_HOST_BLUETOOTH:
            lkbt51_connect();
            break;
        default:
            // nothing
            break;
    }
}
#endif

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record)) { return false; }

    switch (keycode) {
#ifdef BLUETOOTH_ENABLE
        case BT_PRF1 ... BT_PRF3:
            if (record->event.pressed && connection_get_host() == CONNECTION_HOST_BLUETOOTH) {
                lkbt51_select_profile(keycode - BT_PRF1 + 1);
                lkbt51_connect();
            }
            return false;
#endif
    }

    return true;
}
