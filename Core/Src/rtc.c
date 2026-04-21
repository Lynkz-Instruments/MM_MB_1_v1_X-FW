/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rtc.c
  * @brief   This file provides code for the configuration
  *          of the RTC instances.
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
/* Includes ------------------------------------------------------------------*/
#include "rtc.h"
#include "utils.h"

/* USER CODE BEGIN 0 */

// Globals for backup register state (captured before UART ready)
uint32_t g_rtc_bkup_dr0 = 0;
uint32_t g_rtc_bkup_dr1 = 0;
uint32_t g_rtc_inits = 0;
uint8_t g_rtc_wut_restored = 0;

/* USER CODE END 0 */

RTC_HandleTypeDef hrtc;

/* RTC init function, non modified from cubeMX*/
void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_AlarmTypeDef sAlarm = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = RTC_PREDIV_A;
  hrtc.Init.SynchPrediv = RTC_PREDIV_S;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
  hrtc.Init.BinMode = RTC_BINARY_MIX;
  // Match BCD update to 10-bit synch prediv (0..1023) for 1 Hz calendar tick
  hrtc.Init.BinMixBcdU = RTC_BINARY_MIX_BCDU_2;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler(ERROR_RTC_INIT);
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  // Binary mix mode doesn't need SSRU for subsecond underflow
  // if (HAL_RTCEx_SetSSRU_IT(&hrtc) != HAL_OK)
  // {
  //   Error_Handler(ERROR_RTC_SET_ALARM);
  // }

  /** Enable the Alarm A
  */
  sAlarm.BinaryAutoClr = RTC_ALARMSUBSECONDBIN_AUTOCLR_NO;
  sAlarm.AlarmTime.SubSeconds = 0x0;
  sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDBINMASK_NONE;
  sAlarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, 0) != HAL_OK)
  {
    Error_Handler(ERROR_RTC_SET_ALARM);
  }
  /* USER CODE BEGIN RTC_Init 2 */
  // Capture backup register state for debug (UART not ready yet)
  g_rtc_bkup_dr0 = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0);
  g_rtc_bkup_dr1 = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1);
  g_rtc_inits = (READ_BIT(RTC->ICSR, RTC_ICSR_INITS) != 0U) ? 1UL : 0UL;
  g_rtc_wut_restored = 0;
  
  // DON'T restore WUT here - it starts counting immediately!
  // WUT should only be set right before entering shutdown mode
  // If sentinel matches and interval is valid, restore WUT
  // if (g_rtc_bkup_dr0 == 0xDEADBEEF && g_rtc_bkup_dr1 > 0 && g_rtc_bkup_dr1 <= 65535)
  // {
  //   if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, g_rtc_bkup_dr1, RTC_WAKEUPCLOCK_CK_SPRE_16BITS, 0) == HAL_OK)
  //   {
  //     g_rtc_wut_restored = 1;
  //   }
  // }
  /* USER CODE END RTC_Init 2 */

}

// void MX_RTC_Init(void)
// {

//   /* USER CODE BEGIN RTC_Init 0 */

//   /* USER CODE END RTC_Init 0 */

//   RTC_AlarmTypeDef sAlarm = {0};

//   /* USER CODE BEGIN RTC_Init 1 */
//   hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
//   hrtc.Init.SynchPrediv = 255;
//   /* USER CODE END RTC_Init 1 */

//   /** Initialize RTC Only
//   */
//   hrtc.Instance = RTC;
//   hrtc.Init.AsynchPrediv = RTC_PREDIV_A;
//   hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
//   hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
//   hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
//   hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
//   hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
//   hrtc.Init.BinMode = RTC_BINARY_ONLY;
//   if (HAL_RTC_Init(&hrtc) != HAL_OK)
//   {
//     Error_Handler(ERROR_RTC_INIT);
//   }

//   /* USER CODE BEGIN Check_RTC_BKUP */

//   /* USER CODE END Check_RTC_BKUP */

//   /** Initialize RTC and set the Time and Date
//   */
//   if (HAL_RTCEx_SetSSRU_IT(&hrtc) != HAL_OK)
//   {
//     Error_Handler(ERROR_RTC_SET_ALARM);
//   }

//   /** Enable the Alarm A
//   */
//   sAlarm.BinaryAutoClr = RTC_ALARMSUBSECONDBIN_AUTOCLR_NO;
//   sAlarm.AlarmTime.SubSeconds = 0x0;
//   sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
//   sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDBINMASK_NONE;
//   sAlarm.Alarm = RTC_ALARM_A;
//   if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, 0) != HAL_OK)
//   {
//     Error_Handler(ERROR_RTC_SET_ALARM);
//   }
//   /* USER CODE BEGIN RTC_Init 2 */
//   if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0, RTC_WAKEUPCLOCK_RTCCLK_DIV16, 0) != HAL_OK)
//   {
//     Error_Handler(ERROR_RTC_SET_TIMER);
//   }
//   /* USER CODE END RTC_Init 2 */

// }

// void MX_RTC_Init(void)
// {

//   /* USER CODE BEGIN RTC_Init 0 */

//   /* USER CODE END RTC_Init 0 */

//   /* USER CODE BEGIN RTC_Init 1 */

//   /* USER CODE END RTC_Init 1 */
//   /** Initialize RTC Only
//   */
//   hrtc.Instance = RTC;
//   hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
//   hrtc.Init.AsynchPrediv = 127;
//   hrtc.Init.SynchPrediv = 255;
//   hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
//   hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
//   hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
//   hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
//   hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
//   hrtc.Init.BinMode = RTC_BINARY_ONLY;
//   if (HAL_RTC_Init(&hrtc) != HAL_OK)
//   {
//     Error_Handler(ERROR_RTC_INIT);
//   }
//   /** Enable the WakeUp
//   */
//   if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0, RTC_WAKEUPCLOCK_RTCCLK_DIV16, 0) != HAL_OK)
//   {
//     Error_Handler(ERROR_RTC_SET_TIMER);
//   }
//   /* USER CODE BEGIN RTC_Init 2 */

//   /* USER CODE END RTC_Init 2 */

// }

void HAL_RTC_MspInit(RTC_HandleTypeDef* rtcHandle)
{

  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspInit 0 */

  /* USER CODE END RTC_MspInit 0 */

  /** Initializes the peripherals clocks
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler(ERROR_RCC_CLOCK_CONFIG);
    }

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == 0U)
    {
      print("LSE not ready for RTC.\r\n");
      Error_Handler(ERROR_RCC_OSC_CONFIG);
    }

    if (__HAL_RCC_GET_RTC_SOURCE() != RCC_RTCCLKSOURCE_LSE)
    {
      print("RTC clock source is not LSE.\r\n");
      Error_Handler(ERROR_RCC_CLOCK_CONFIG);
    }

    /* RTC clock enable */
    __HAL_RCC_RTC_ENABLE();
    __HAL_RCC_RTCAPB_CLK_ENABLE();

    /* RTC interrupt Init */
    HAL_NVIC_SetPriority(TAMP_STAMP_LSECSS_SSRU_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TAMP_STAMP_LSECSS_SSRU_IRQn);
    HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);

  /* USER CODE BEGIN RTC_MspInit 1 */
    // HAL_NVIC_SetPriority(RTC_WKUP_IRQn, 0, 0);
    // HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);
  /* USER CODE END RTC_MspInit 1 */
  }
}

void HAL_RTC_MspDeInit(RTC_HandleTypeDef* rtcHandle)
{

  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspDeInit 0 */

  /* USER CODE END RTC_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_RTC_DISABLE();
    __HAL_RCC_RTCAPB_CLK_DISABLE();

    /* RTC interrupt Deinit */
    HAL_NVIC_DisableIRQ(TAMP_STAMP_LSECSS_SSRU_IRQn);
    HAL_NVIC_DisableIRQ(RTC_Alarm_IRQn);
  /* USER CODE BEGIN RTC_MspDeInit 1 */
    HAL_NVIC_DisableIRQ(RTC_WKUP_IRQn);
  /* USER CODE END RTC_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
