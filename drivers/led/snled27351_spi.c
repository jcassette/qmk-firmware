/* Copyright 2021 @ Keychron (https://www.keychron.com)
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

#include "snled27351_spi.h"
#include "spi_master.h"
#include "gpio.h"

#ifndef SNLED27351_SPI_MODE
#    define SNLED27351_SPI_MODE 0
#endif

#ifndef SNLED27351_SPI_DIVISOR
#    define SNLED27351_SPI_DIVISOR 16
#endif

#ifndef SNLED27351_PHASE_CHANNEL
#    define SNLED27351_PHASE_CHANNEL SNLED27351_SCAN_PHASE_12_CHANNEL
#endif

#ifndef SNLED27351_CURRENT_TUNE
#    define SNLED27351_CURRENT_TUNE \
        { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
#endif

const pin_t cs_pins[SNLED27351_DRIVER_COUNT] = {
#if defined(SNLED27351_CS_PIN_4)
    SNLED27351_CS_PIN_1, SNLED27351_CS_PIN_2, SNLED27351_CS_PIN_3, SNLED27351_CS_PIN_4
#elif defined(SNLED27351_CS_PIN_3)
    SNLED27351_CS_PIN_1, SNLED27351_CS_PIN_2, SNLED27351_CS_PIN_3
#elif defined(SNLED27351_CS_PIN_2)
    SNLED27351_CS_PIN_1, SNLED27351_CS_PIN_2
#elif defined(SNLED27351_CS_PIN_1)
    SNLED27351_CS_PIN_1
#endif
};

// These buffers match the SNLED27351 PWM registers.
// The control buffers match the PG0 LED On/Off registers.
// Storing them like this is optimal for SPI transfers to the registers.
// We could optimize this and take out the unused registers from these
// buffers and the transfers in snled27351_write_pwm_buffer() but it's
// probably not worth the extra complexity.
typedef struct snled27351_driver_t {
    uint8_t pwm_buffer[SNLED27351_PWM_DUTY_LENGTH];
    bool    pwm_buffer_dirty;
    uint8_t led_control_buffer[SNLED27351_LED_CONTROL_ON_OFF_LENGTH];
    bool    led_control_buffer_dirty;
} PACKED snled27351_driver_t;

snled27351_driver_t driver_buffers[SNLED27351_DRIVER_COUNT];

bool snled27351_transmit(uint8_t index, uint8_t *buf, uint8_t len) {
    if (!spi_start(cs_pins[index], false, SNLED27351_SPI_MODE, SNLED23751_SPI_DIVISOR)) {
        return false;
    }

    spi_status_t status = spi_transmit(buf, len);

    spi_stop();

    return (status == SPI_STATUS_SUCCESS);
}

bool snled27351_write_register(uint8_t index, uint16_t addr, uint8_t data) {
    uint8_t buffer[3];

    buffer[0] = SNLED27351_WRITE | SNLED27351_PATTERN | (addr >> 8 & 0x0F);
    buffer[1] = addr & 0xFF;
    buffer[2] = data;

    return snled27351_transmit(index, buffer, sizeof(buffer));
}

bool snled27351_write_registers(uint8_t index, uint16_t addr, uint8_t *data, uint8_t len) {
    uint8_t buffer[len + 2];

    buffer[0] = SNLED27351_WRITE | SNLED27351_PATTERN | (addr >> 8 & 0x0F);
    buffer[1] = addr & 0xFF;
    memcpy(&buffer[2], data, len);

    return snled27351_transmit(index, buffer, sizeof(buffer));
}

void snled27351_init_drivers(void) {
    spi_init();

#if defined(SNLED27351_SDB_PIN)
    gpio_set_pin_output(SNLED27351_SDB_PIN);
    gpio_write_pin_high(SNLED27351_SDB_PIN);
#endif

    for (uint8_t i = 0; i < SNLED27351_DRIVER_COUNT; i++) {
        snled27351_init(i);
    }

    for (int i = 0; i < SNLED27351_LED_COUNT; i++) {
        snled27351_set_led_control_register(i, true, true, true);
    }

    for (uint8_t i = 0; i < SNLED27351_DRIVER_COUNT; i++) {
        snled27351_update_led_control_registers(i);
    }
}

void snled27351_init(uint8_t index) {
    // Setting LED driver to shutdown mode
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SOFTWARE_SHUTDOWN, SNLED27351_SOFTWARE_SHUTDOWN_SSD_SHUTDOWN);
    // Setting internal channel pulldown/pullup
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_PULLDOWNUP, SNLED27351_PULLDOWNUP_ALL_ENABLED);
    // Select number of scan phase
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SCAN_PHASE, SNLED27351_PHASE_CHANNEL);
    // Setting PWM Delay Phase
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SLEW_RATE_CONTROL_MODE_1, SNLED27351_SLEW_RATE_CONTROL_MODE_1_PDP_ENABLE);
    // Setting Driving/Sinking Channel Slew Rate
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SLEW_RATE_CONTROL_MODE_2, SNLED27351_SLEW_RATE_CONTROL_MODE_2_DSL_ENABLE | SNLED27351_SLEW_RATE_CONTROL_MODE_2_SSL_ENABLE);
    // Setting Iref
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SOFTWARE_SLEEP, 0);

    snled27351_driver_t *driver = &driver_buffers[index];

    // Set LEDs off
    memset(driver->led_control_buffer, 0, SNLED27351_LED_CONTROL_ON_OFF_LENGTH);
    snled27351_write_registers(index, SNLED27351_LED_CONTROL_ON_OFF_FIRST_ADDR, driver->led_control_buffer, SNLED27351_LED_CONTROL_ON_OFF_LENGTH);

    driver->led_control_buffer_dirty = false;

    // Set PWM duty
    memset(driver->pwm_buffer, 0, SNLED27351_LED_CONTROL_ON_OFF_LENGTH);
    snled27351_write_registers(index, SNLED27351_PWM_DUTY_FIRST_ADDR, driver->pwm_buffer, SNLED27351_PWM_DUTY_LENGTH);

    driver->pwm_buffer_dirty = false;

    // Set current control
    uint8_t current_tune[SNLED27351_CURRENT_TUNE_CCS_LENGTH] = SNLED27351_CURRENT_TUNE;
    snled27351_write_registers(index, SNLED27351_CURRENT_TUNE_CCS_FIRST_ADDR, current_tune, SNLED27351_CURRENT_TUNE_CCS_LENGTH);

    // Setting LED driver to normal mode
    snled27351_write_register(index, SNLED27351_FUNCTION_REG_SOFTWARE_SHUTDOWN, SNLED27351_SOFTWARE_SHUTDOWN_SSD_NORMAL);
}

void snled27351_set_color(int index, uint8_t red, uint8_t green, uint8_t blue) {
    snled27351_led_t led;
    if (index >= 0 && index < SNLED27351_LED_COUNT) {
        memcpy_P(&led, (&g_snled27351_leds[index]), sizeof(led));

        if (driver_buffers[led.driver].pwm_buffer[led.r] == red && driver_buffers[led.driver].pwm_buffer[led.g] == green && driver_buffers[led.driver].pwm_buffer[led.b] == blue) {
            return;
        }

        driver_buffers[led.driver].pwm_buffer[led.r] = red;
        driver_buffers[led.driver].pwm_buffer[led.g] = green;
        driver_buffers[led.driver].pwm_buffer[led.b] = blue;
        driver_buffers[led.driver].pwm_buffer_dirty  = true;
    }
}

void snled27351_set_color_all(uint8_t red, uint8_t green, uint8_t blue) {
    for (int i = 0; i < SNLED27351_LED_COUNT; i++) {
        snled27351_set_color(i, red, green, blue);
    }
}

void snled27351_set_led_control_register(uint8_t index, bool red, bool green, bool blue) {
    snled27351_led_t led;
    memcpy_P(&led, (&g_snled27351_leds[index]), sizeof(led));

    uint8_t control_register_r = led.r / 8;
    uint8_t control_register_g = led.g / 8;
    uint8_t control_register_b = led.b / 8;
    uint8_t bit_r              = led.r % 8;
    uint8_t bit_g              = led.g % 8;
    uint8_t bit_b              = led.b % 8;

    if (red) {
        driver_buffers[led.driver].led_control_buffer[control_register_r] |= (1 << bit_r);
    } else {
        driver_buffers[led.driver].led_control_buffer[control_register_r] &= ~(1 << bit_r);
    }
    if (green) {
        driver_buffers[led.driver].led_control_buffer[control_register_g] |= (1 << bit_g);
    } else {
        driver_buffers[led.driver].led_control_buffer[control_register_g] &= ~(1 << bit_g);
    }
    if (blue) {
        driver_buffers[led.driver].led_control_buffer[control_register_b] |= (1 << bit_b);
    } else {
        driver_buffers[led.driver].led_control_buffer[control_register_b] &= ~(1 << bit_b);
    }

    driver_buffers[led.driver].led_control_buffer_dirty = true;
}

void snled27351_update_pwm_buffers(uint8_t index) {
    if (driver_buffers[index].pwm_buffer_dirty) {
        snled27351_write_registers(index, SNLED27351_PWM_DUTY_FIRST_ADDR, driver_buffers[index].pwm_buffer, SNLED27351_PWM_DUTY_LENGTH);
        driver_buffers[index].pwm_buffer_dirty = false;
    }
}

void snled27351_update_led_control_registers(uint8_t index) {
    if (driver_buffers[index].led_control_buffer_dirty) {
        snled27351_write_registers(index, SNLED27351_LED_CONTROL_ON_OFF_FIRST_ADDR, driver_buffers[index].led_control_buffer, SNLED27351_LED_CONTROL_ON_OFF_LENGTH);
        driver_buffers[index].led_control_buffer_dirty = false;
    }
}

void snled27351_flush(void) {
    for (uint8_t i = 0; i < SNLED27351_DRIVER_COUNT; i++) {
        snled27351_update_pwm_buffers(i);
    }
}

void snled27351_sleep(void) {
    for (uint8_t i = 0; i < SNLED27351_DRIVER_COUNT; i++) {
        snled27351_write_register(i, SNLED27351_FUNCTION_REG_SOFTWARE_SHUTDOWN, SNLED27351_SOFTWARE_SHUTDOWN_SSD_SHUTDOWN);
    }
#if defined(SNLED27351_SDB_PIN)
    gpio_write_pin_low(SNLED27351_SDB_PIN);
#else
    for (uint8_t i = 0; i < SNLED27351_DRIVER_COUNT; i++) {
        snled27351_write_register(i, SNLED27351_FUNCTION_REG_SOFTWARE_SLEEP, SNLED27351_SOFTWARE_SLEEP_ENABLE);
    }
#endif
}

void snled27351_wakeup(void) {
#if defined(SNLED27351_SDB_PIN)
    gpio_write_pin_high(SNLED27351_SDB_PIN);
#endif
    for (uint8_t i = 0; i < SNLED27351_DRIVER_COUNT; i++) {
        snled27351_write_register(i, SNLED27351_FUNCTION_REG_SOFTWARE_SHUTDOWN, SNLED27351_SOFTWARE_SHUTDOWN_SSD_NORMAL);
    }
}
