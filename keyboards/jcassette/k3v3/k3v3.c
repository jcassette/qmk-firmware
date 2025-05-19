// Copyright 2025 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quantum.h"

#define UNUSED_PIN_1 A13
#define UNUSED_PIN_2 B2

#define BATTERY_LOW_LED_PIN B12

void keyboard_post_init_kb(void) {
    gpio_set_pin_input_low(UNUSED_PIN_1);
    gpio_set_pin_input_low(UNUSED_PIN_2);

    gpio_set_pin_output_push_pull(BATTERY_LOW_LED_PIN);
    gpio_write_pin_low(BATTERY_LOW_LED_PIN);

    keyboard_post_init_user();
}
