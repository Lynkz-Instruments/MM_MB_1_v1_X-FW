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

/* Backup-domain state captured at MX_RTC_Init(), before UART is up. See
 * LoRaWAN_Init()'s startup log for a printed dump of these once UART is ready. */
extern uint32_t g_rtc_bkup_dr0;
extern uint32_t g_rtc_bkup_dr1;
extern uint32_t g_rtc_inits;
extern uint8_t g_rtc_wut_restored;

/* Raw RCC->BDCR captured as the very first instruction of main(), before any
 * init code runs. Brackets whether RCC_BDCR_RTCSEL already reads wrong at the
 * earliest possible moment (backup domain lost during shutdown/wake itself)
 * or only after MX_RTC_Init() runs (a software-triggered reset via the RTC
 * clock-source mismatch path in HAL_RCCEx_PeriphCLKConfig()). */
extern uint32_t g_boot_bdcr;

/* RCC->BDCR captured at the end of MX_RTC_Init(), after HAL_RTC_MspInit()'s
 * LSE selection/readiness checks have already passed (that function calls
 * Error_Handler() itself if LSE isn't ready or isn't selected as the RTC
 * clock source, so reaching this point already implies success) - printed
 * alongside g_boot_bdcr in LoRaWAN_Init() as a live, post-init confirmation
 * rather than just the pre-init snapshot. */
extern uint32_t g_rtc_bdcr_after_init;

void MX_RTC_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __RTC_H__ */

