/** @file
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "i2c_stm32.h"
#include "stm32wlxx_hal.h"


int i2c_init(struct i2c_dev *dev, I2C_HandleTypeDef *hi2c)
{
	if (!dev || !hi2c) return -1;
	struct i2c_ctx *ctx = (struct i2c_ctx *)dev->context;
	if (!ctx) return -2;
	ctx->hi2c = hi2c;
	return 0;
}


int i2c_write(struct i2c_dev *dev, uint8_t *buf, size_t len)
{
	if (!dev || !buf || len == 0) return -1;
	struct i2c_ctx *ctx = (struct i2c_ctx *)dev->context;
	if (!ctx || !ctx->hi2c) return -2;
	if (HAL_I2C_Master_Transmit(ctx->hi2c, dev->addr << 1, buf, len, 100) != HAL_OK) {
		return -3;
	}
	return 0;
}


int i2c_read(struct i2c_dev *dev, uint8_t reg, uint8_t *buf, size_t len)
{
	if (!dev || !buf || len == 0) return -1;
	struct i2c_ctx *ctx = (struct i2c_ctx *)dev->context;
	if (!ctx || !ctx->hi2c) return -2;
	// Write register address, then read data
	if (HAL_I2C_Master_Transmit(ctx->hi2c, dev->addr << 1, &reg, 1, 100) != HAL_OK) {
		return -3;
	}
	if (HAL_I2C_Master_Receive(ctx->hi2c, dev->addr << 1, buf, len, 100) != HAL_OK) {
		return -4;
	}
	return 0;
}


int i2c_reg_write_byte(struct i2c_dev *dev, uint8_t reg, uint8_t data)
{
	return i2c_write(dev, (uint8_t[]){reg, data}, 2U);
}

int i2c_reg_read_byte(struct i2c_dev *dev, uint8_t reg, uint8_t *data)
{
	return i2c_read(dev, reg, data, 1U);
}

int i2c_reg_update_byte(struct i2c_dev *dev, uint8_t reg, uint8_t mask, uint8_t data)
{
	uint8_t byte;
	int ret = i2c_reg_read_byte(dev, reg, &byte);

	if (ret < 0) {
		return ret;
	}

	return i2c_reg_write_byte(dev, reg, (byte & ~mask) | (data & mask));
}
