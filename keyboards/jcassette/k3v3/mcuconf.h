// Copyright 2025 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include_next <mcuconf.h>

#undef WB32_I2C_USE_I2C1
#define WB32_I2C_USE_I2C1 TRUE

#undef WB32_SPI_USE_QSPI
#define WB32_SPI_USE_QSPI TRUE
