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
#include <stdbool.h>

#define TB_DETECT_PIN                           GPIO_PIN_10
#define TB_DETECT_GPIO_PORT                     GPIOB
#define LED_PIN                                 GPIO_PIN_1
#define LED_GPIO_PORT                           GPIOB
/* MissMetal board (MissMetal_Main.SchDoc): INT1_BMI on PA0 (not TA's PB14).
 * INT2_BMI (PC13) is wired but unused - every BMA400 interrupt is mapped to
 * INT1 - so PC13 stays in analog mode. */
#define BMA400_INT_PIN                          GPIO_PIN_0
#define BMA400_INT_GPIO_PORT                    GPIOA
#define BMA400_INT_EXTI_IRQn                    EXTI0_IRQn

void MX_GPIO_Init(void);
void gpio_blink_led(uint8_t times, uint16_t delay_ms);
void gpio_release_led(void);

/**
  * @brief  True if this device is sitting on the test board - TB_DETECT_PIN
  *         is pulled HIGH by a 100kOhm resistor normally, and pulled LOW
  *         deliberately on the test board, so a LOW reading means "on the
  *         test board".
  */
bool gpio_is_test_board(void);

/**
  * @brief  Disconnects TB_DETECT_PIN's digital input buffer (back to
  *         analog) once gpio_is_test_board() has been read for the last
  *         time this boot - mirrors gpio_release_led(). Negligible power
  *         difference either way (the pin's never floating - driven to a
  *         clean rail by the pull-up or the test-board ground either way),
  *         but free to do and matches this file's existing convention for
  *         pins that are read once and then don't need to be revisited.
  */
void gpio_release_test_board_detect(void);

#ifdef __cplusplus
}
#endif
#endif /*__ GPIO_H__ */

