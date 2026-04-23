/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.h
  * @brief   This file contains all the function prototypes for
  *          the gpio.c file
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
#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wlxx_hal.h"

#define TB_DETECT_PIN                           GPIO_PIN_10
#define TB_DETECT_GPIO_PORT                     GPIOB
#define LED_PIN                                 GPIO_PIN_1
#define LED_GPIO_PORT                           GPIOB

void MX_GPIO_Init(void);
void blink_led(uint8_t times, uint16_t delay_ms);

#ifdef __cplusplus
}
#endif
#endif /*__ GPIO_H__ */

