/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rtc.h
  * @brief   This file contains all the function prototypes for
  *          the rtc.c file
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
#ifndef __RTC_H__
#define __RTC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wlxx_hal.h"

#define RTC_PREDIV_A                            ((1<<(15-RTC_N_PREDIV_S))-1)
#define RTC_N_PREDIV_S                          10
#define RTC_PREDIV_S                            ((1<<RTC_N_PREDIV_S)-1)

extern RTC_HandleTypeDef hrtc;

void MX_RTC_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __RTC_H__ */

