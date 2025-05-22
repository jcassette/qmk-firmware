// Copyright 2025 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(EEPROM_ENABLE)
#define HAL_USE_I2C TRUE
#endif

#if defined(RGB_MATRIX_ENABLE)
#define HAL_USE_SPI TRUE
#define SPI_USE_WAIT TRUE
#define SPI_SELECT_MODE SPI_SELECT_MODE_PAD
#endif

#include_next <halconf.h>
