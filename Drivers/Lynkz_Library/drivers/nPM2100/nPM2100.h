#ifndef NPM2100_H
#define NPM2100_H

#include <stdint.h>
#include <stdbool.h>
#include "npm2100-bm/src/mfd_npm2100.h"
#include "npm2100-bm/src/gpio_npm2100.h"
#include "npm2100-bm/src/regulator_npm2100.h"
#include "hal/i2c.h"

// STM32 HAL I2C helpers
int nPM2100_I2C_Read(I2C_HandleTypeDef *hi2c, uint8_t dev_addr, uint8_t reg_addr, uint8_t *reg_data, uint16_t len);
int nPM2100_I2C_Write(I2C_HandleTypeDef *hi2c, uint8_t dev_addr, uint8_t reg_addr, const uint8_t *reg_data, uint16_t len);

// Basic init function
int nPM2100_Init(struct i2c_dev *dev);

// Register access helpers
int nPM2100_SetRegister(struct i2c_dev *dev, uint8_t reg, uint8_t value);
int nPM2100_GetRegister(struct i2c_dev *dev, uint8_t reg, uint8_t *value);

// High-level helpers
int nPM2100_SetTimer(struct i2c_dev *dev, uint32_t time_ms);
int nPM2100_StartTimer(struct i2c_dev *dev);
int nPM2100_StopTimer(struct i2c_dev *dev);
int nPM2100_Reset(struct i2c_dev *dev);
int nPM2100_Hibernate(struct i2c_dev *dev, uint32_t time_ms, bool pass_through);
int nPM2100_EnableEvents(struct i2c_dev *dev, uint32_t events);
int nPM2100_DisableEvents(struct i2c_dev *dev, uint32_t events);
int nPM2100_ProcessEvents(struct i2c_dev *dev, uint32_t *events);

#endif // NPM2100_H
