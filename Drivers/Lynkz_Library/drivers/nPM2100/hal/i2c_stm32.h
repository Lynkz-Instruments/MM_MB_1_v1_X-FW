/** @file
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stdint.h>
#include "stm32wlxx_hal.h"
#include "i2c.h"

#ifndef I2C_STM32_H
#define I2C_STM32_H

// STM32 context for I2C
struct i2c_ctx {
	I2C_HandleTypeDef *hi2c;
};

// Initialize i2c_dev for STM32
int i2c_init(struct i2c_dev *dev, I2C_HandleTypeDef *hi2c);

#endif // I2C_STM32_H