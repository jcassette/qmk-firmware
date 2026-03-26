#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "report.h"

void lkbt51_pair(void);
void lkbt51_connect(void);
void lkbt51_disconnect(void);
void lkbt51_select_profile(uint8_t profile);

void lkbt51_init(void);
void lkbt51_task(void);
bool lkbt51_is_connected(void);
bool lkbt51_can_send_nkro(void);
uint8_t lkbt51_keyboard_leds(void);
void lkbt51_send_keyboard(report_keyboard_t const *report);
void lkbt51_send_nkro(report_nkro_t const *report);
void lkbt51_send_mouse(report_mouse_t const *report);
void lkbt51_send_consumer(uint16_t usage);
void lkbt51_send_system(uint16_t usage);
