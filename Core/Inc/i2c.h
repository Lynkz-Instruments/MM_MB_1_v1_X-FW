/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2c.h
  * @brief   This file contains all the function prototypes for
  *          the i2c.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __I2C_H__
#define __I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32wlxx_hal.h"
#include <stdbool.h>

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_I2C1_Init(void);

/* USER CODE BEGIN Prototypes */
/**
  * @brief  Re-run I2C1 peripheral init after a STOP2 wake, which does not
  *         retain I2Cx peripheral configuration. Mirrors vcom_Resume().
  */
void i2c_Resume(void);

/**
  * @brief  I2C bus recovery - call before MX_I2C1_Init(). If a slave (BMA400
  *         or EEPROM) is holding SDA low because the MCU was reset (IWDG,
  *         debugger restart) in the middle of a transfer, clocks SCL by hand
  *         until it lets go (max 9 pulses), then generates a STOP. Also resets
  *         the I2C1 peripheral, which a core-only reset leaves configured.
  * @return true if the bus was stuck and recovery was needed.
  */
bool i2c_BusRecover(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __I2C_H__ */

