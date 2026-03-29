/* Copyright 2023 ~ 2025 @ Keychron (https://www.keychron.com)
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
#include "spi_master.h"

#ifndef LKBT51_INT_INPUT_PIN
#    error "LKBT51_INT_INPUT_PIN is not defined"
#endif

#ifndef LKBT51_INT_OUTPUT_PIN
#    error "LKBT51_INT_OUTPUT_PIN is not defined"
#endif

#define LKBT51_SPI_MODE 0
#define LKBT51_SPI_DIVISOR 32

#define LKBT51_RX_BUFFER_SIZE 64

#define LKBT51_NKRO_SIZE 20

#define LKBT51_PROFILE_MIN 1
#define LKBT51_PROFILE_MAX 3

// clang-format off
enum lkbt51_cmd {
    /* HID Report  */
    LKBT51_CMD_SEND_KB       = 0x11,
    LKBT51_CMD_SEND_NKRO     = 0x12,
    LKBT51_CMD_SEND_CONSUMER = 0x13,
    LKBT51_CMD_SEND_SYSTEM   = 0x14,
    LKBT51_CMD_SEND_MOUSE    = 0x16,
    /* Bluetooth connections */
    LKBT51_CMD_PAIRING        = 0x21,
    LKBT51_CMD_CONNECT        = 0x22,
    LKBT51_CMD_DISCONNECT     = 0x23,
    LKBT51_CMD_SWITCH_HOST    = 0x24,
    LKBT51_CMD_READ_STATE_REG = 0x25,
    /* Battery */
    LKBT51_CMD_BATTERY_MANAGE = 0x31,
    LKBT51_CMD_UPDATE_BAT_LVL = 0x32,
    LKBT51_CMD_UPDATE_BAT_STATE = 0x33,
    /* Set/get parameters */
    LKBT51_CMD_SET_CONFIG      = 0x41,
    LKBT51_CMD_SET_NAME        = 0x45,
    /* Event */
    LKBT51_CMD_ACK_CONNECTION  = 0xA4,
};

enum lkbt51_evt {
    LKBT51_EVT_ACK              = 0xA1,
    LKBT51_EVT_RESET            = 0xB0,
    LKBT51_EVT_LE_CONNECTION    = 0xB1,
    LKBT51_EVT_HOST_TYPE        = 0xB2,
    LKBT51_EVT_CONNECTION       = 0xB3,
    LKBT51_EVT_HID_EVENT        = 0xB4,
    LKBT51_EVT_BATTERY          = 0xB5,
};

enum lkbt51_conn {
    LKBT51_CONN_CONNECTED       = 0x20,
    LKBT51_CONN_PAIRING         = 0x21,
    LKBT51_CONN_RECONNECTING    = 0x22,
    LKBT51_CONN_DISCONNECTED    = 0x23,
    LKBT51_CONN_PINCODE_ENTER   = 0x24,
    LKBT51_CONN_PINCODE_EXIT    = 0x25,
    LKBT51_CONN_SLEEPING        = 0x26
};

enum lkbt51_ack {
    LKBT51_ACK_SUCCESS = 0x00,
    LKBT51_ACK_CHECKSUM_ERROR,
    LKBT51_ACK_FIFO_HALF_WARNING,
    LKBT51_ACK_FIFO_FULL_ERROR,
};

#define LKBT51_MSK_CONNECTION   0x01
#define LKBT51_MSK_LED          0x02
#define LKBT51_MSK_BATT         0x04
#define LKBT51_MSK_RESET        0x08
#define LKBT51_MSK_RPT_INTERVAL 0x10
#define LKBT51_MSK_MD           0x80
// clang-format on

// INTERNAL TYPES

typedef struct __attribute__((packed)) {
    uint8_t  event_mode; /* Must be 0x02 */
    uint16_t connected_idle_timeout;
    uint16_t pairing_timeout;   /* Range: 30 ~ 3600 second, 0 for default */
    uint8_t  pairing_mode;      /* 0: default, 1: Just Works, 2: Passkey Entry */
    uint16_t reconnect_timeout; /* 0: default, 0xFF: Unlimited time, 2 ~ 254 seconds */
    uint8_t  report_rate;       /* 90 or 133 */
    uint8_t  rsvd1;
    uint8_t  rsvd2;
    uint8_t  vendor_id_source; /* 0: From Bluetooth SIG, 1: From USB-IF */
    uint16_t vendor_id;
    uint16_t product_id;
    /* Below parametes are only available for BLE module  */
    uint16_t le_connection_interval_min;
    uint16_t le_connection_interval_max;
    uint16_t le_connection_interval_timeout;
} lkbt51_config_t;

// INTERNAL DATA

static enum lkbt51_conn lkbt51_conn = 0;

static uint8_t lkbt51_profile = 1;

static uint8_t lkbt51_leds = 0;

// INTERNAL FUNCTIONS

static void ddump(void const *ptr, uint8_t len) {
#ifdef NO_DEBUG
    (void)ptr;
    (void)len;
#else
    static const char digits[] = "0123456789ABCDEF";

    uint8_t const *buf = ptr;

    if (!debug_config.enable) {
        return;
    }

    for (uint8_t i = 0; i < len; i++) {
        uint8_t b = buf[i];
        sendchar(digits[b >> 4]);
        sendchar(digits[b & 0xF]);
        sendchar(' ');
    }
    sendchar('\n');
#endif
}

static void lkbt51_transmit(enum lkbt51_cmd command, void const *data, uint8_t len, bool request_ack) {
    static uint8_t sequence_count = 1;

    dprintf("%s\n", __func__);

    uint8_t *payload = (uint8_t *)(data);

    // clang-format off
    uint8_t header[] = {
        0x84, 0x7E, 0x00, 0x00, 0xAA,
        request_ack ? 0x56 : 0x55,
        len + 3,
        ~(len + 3),
        sequence_count,
        command
    };
    // clang-format on

    uint16_t checksum = command;
    for (uint8_t i = 0; i < len; i++) {
        checksum += payload[i];
    }

    uint8_t trailer[] = {checksum & 0xFF, checksum >> 8};

    ddump(header, sizeof(header));
    if (payload && len) {
        ddump(payload, len);
    }
    ddump(trailer, sizeof(trailer));

    gpio_write_pin_low(LKBT51_INT_OUTPUT_PIN);
    wait_ms(1);
    gpio_write_pin_high(LKBT51_INT_OUTPUT_PIN);
    wait_ms(1);

    spi_start(LKBT51_INT_OUTPUT_PIN, false, LKBT51_SPI_MODE, LKBT51_SPI_DIVISOR);
    spi_transmit(header, sizeof(header));
    if (payload && len) {
        spi_transmit(payload, len);
    }
    spi_transmit(trailer, sizeof(trailer));
    spi_stop();

    if (sequence_count < 255) {
        sequence_count++;
    } else {
        sequence_count = 1;
    }
}

static void lkbt51_receive(uint8_t *buf, uint8_t len) {
    dprintf("%s\n", __func__);

    uint8_t header[] = {0x84, 0x7F, 0x00, 0x80};

    spi_start(LKBT51_INT_OUTPUT_PIN, false, LKBT51_SPI_MODE, LKBT51_SPI_DIVISOR);
    spi_transmit(header, sizeof(header));
    spi_receive(buf, len);
    spi_stop();

    ddump(buf, len);
}

static void lkbt51_configure(void) {
    dprintf("%s\n", __func__);

    char const *name = PRODUCT;

    // clang-format off
    lkbt51_config_t config = {
        .event_mode             = 0x02,
        .connected_idle_timeout = 7200,
        .pairing_timeout        = 180,
        .pairing_mode           = 0,
        .reconnect_timeout      = 5,
        .report_rate            = 90,
        .vendor_id_source       = 1,
        .vendor_id              = VENDOR_ID,
        .product_id             = PRODUCT_ID
    };
    // clang-format on

    lkbt51_transmit(LKBT51_CMD_SET_NAME, name, strlen(name), false);
    wait_ms(3);
    lkbt51_transmit(LKBT51_CMD_SET_CONFIG, &config, sizeof(config), false);
}

static void lkbt51_process_status(uint8_t const *buf) {
    dprintf("%s\n", __func__);

    if (buf[0] != 0xAA) {
        dprintf("%s: bad header 0x%02X\n", __func__, buf[0]);
        return;
    }

    uint8_t status_bits = buf[1];

    if (status_bits & LKBT51_MSK_CONNECTION) {
        lkbt51_conn = buf[2];
        dprintf("%s: conn = 0x%02X\n", __func__, lkbt51_conn);
        lkbt51_transmit(LKBT51_CMD_ACK_CONNECTION, NULL, 0, false);
    }

    if (status_bits & LKBT51_MSK_LED) {
        lkbt51_leds = buf[4];
        dprintf("%s: leds = 0x%02X\n", __func__, lkbt51_leds);
    }

    if (status_bits & LKBT51_MSK_BATT) {
        dprintf("%s: batt\n", __func__);
    }

    if (status_bits & LKBT51_MSK_RESET) {
        dprintf("%s: reset\n", __func__);
        lkbt51_configure();
    }

    if (status_bits & LKBT51_MSK_RPT_INTERVAL) {
        dprintf("%s: interval\n", __func__);
    }

    if (status_bits & LKBT51_MSK_MD) {
        // dprintf("%s: md\n", __func__);
    }
}

// PUBLIC FUNCTIONS

void lkbt51_pair(void) {
    dprintf("%s\n", __func__);

    // clang-format off
    uint8_t payload[] = {
        lkbt51_profile,
        0, 0, // default timeout
        3, // pairing mode LESC or SSP
        1, // bluetooth classic (not BLE)
        0 // default TX power
    };
    // clang-format on

    lkbt51_transmit(LKBT51_CMD_PAIRING, payload, sizeof(payload), true);
}

void lkbt51_connect(void) {
    dprintf("%s\n", __func__);

    // clang-format off
    uint8_t payload[] = {
        lkbt51_profile,
        0, 0 // default timeout
    };
    // clang-format on

    lkbt51_transmit(LKBT51_CMD_CONNECT, payload, sizeof(payload), true);
}

void lkbt51_disconnect(void) {
    dprintf("%s\n", __func__);

    uint8_t payload[] = {
        0 // sleep mode
    };
    lkbt51_transmit(LKBT51_CMD_DISCONNECT, payload, sizeof(payload), true);
}

void lkbt51_select_profile(uint8_t profile) {
    dprintf("%s\n", __func__);

    if (profile < LKBT51_PROFILE_MIN || profile > LKBT51_PROFILE_MAX) {
        dprintf("%s: invalid argument\n", __func__);
        return;
    }
    if (profile != lkbt51_profile) {
        lkbt51_profile = profile;
        if (lkbt51_conn == LKBT51_CONN_CONNECTED) {
            lkbt51_connect();
        }
    }
}

// BLUETOOTH DRIVER INTERFACE

void lkbt51_init(void) {
    spi_init();

    gpio_set_pin_output_push_pull(LKBT51_INT_OUTPUT_PIN);
    gpio_write_pin_high(LKBT51_INT_OUTPUT_PIN);

    gpio_set_pin_input_high(LKBT51_INT_INPUT_PIN);

    gpio_set_pin_output_push_pull(LKBT51_RESET_PIN);
    gpio_write_pin_low(LKBT51_RESET_PIN);
    wait_ms(1);
    gpio_write_pin_high(LKBT51_RESET_PIN);
}

void lkbt51_task(void) {
    if (gpio_read_pin(LKBT51_INT_INPUT_PIN) == 0) {
        uint8_t buf[LKBT51_RX_BUFFER_SIZE];
        memset(buf, 0, sizeof(buf));
        lkbt51_receive(buf, sizeof(buf));
        lkbt51_process_status(buf);
    }
}

bool lkbt51_is_connected(void) {
    return (lkbt51_conn == LKBT51_CONN_CONNECTED);
}

bool lkbt51_can_send_nkro(void) {
    return true;
}

uint8_t lkbt51_keyboard_leds(void) {
    return lkbt51_leds;
}

void lkbt51_send_keyboard(report_keyboard_t const *report) {
    dprintf("%s\n", __func__);

    lkbt51_transmit(LKBT51_CMD_SEND_KB, &report->mods, 2 + KEYBOARD_REPORT_KEYS, false);
}

void lkbt51_send_nkro(report_nkro_t const *report) {
    dprintf("%s\n", __func__);

    lkbt51_transmit(LKBT51_CMD_SEND_NKRO, &report->mods, LKBT51_NKRO_SIZE, false);
}

void lkbt51_send_mouse(report_mouse_t const *report) {
    dprintf("%s\n", __func__);

    // clang-format off
    uint8_t payload[] = {
        report->buttons,
        (int16_t)report->x & 0xFF,
        (int16_t)report->x >> 8,
        (int16_t)report->y & 0xFF,
        (int16_t)report->y >> 8,
        report->v,
        report->h
    };
    // clang-format on

    lkbt51_transmit(LKBT51_CMD_SEND_MOUSE, payload, sizeof(payload), false);
}

void lkbt51_send_consumer(uint16_t usage) {
    dprintf("%s\n", __func__);

    uint8_t payload[] = {usage & 0xFF, usage >> 8};
    lkbt51_transmit(LKBT51_CMD_SEND_CONSUMER, payload, sizeof(payload), false);
}

void lkbt51_send_system(uint16_t usage) {
    dprintf("%s\n", __func__);

    if (SYSTEM_POWER_DOWN <= usage && usage <= SYSTEM_WAKE_UP) {
        uint8_t bit     = usage - SYSTEM_POWER_DOWN;
        uint8_t payload = 1 << bit;
        lkbt51_transmit(LKBT51_CMD_SEND_SYSTEM, &payload, sizeof(payload), false);
    }
}

// void lkbt51_send_raw_hid(uint8_t *data, uint8_t length) {}
