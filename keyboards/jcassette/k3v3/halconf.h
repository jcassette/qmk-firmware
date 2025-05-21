// Copyright 2025 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(EEPROM_ENABLE)
#define HAL_USE_I2C TRUE
#endif

#if defined(RGB_MATRIX_ENABLE)
#define HAL_USE_SPI TRUE
#endif

#include_next <halconf.h>
