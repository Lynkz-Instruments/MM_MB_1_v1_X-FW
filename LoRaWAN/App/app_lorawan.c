/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_lorawan.c
  * @author  MCD Application Team
  * @brief   Application of the LRWAN Middleware
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

#include "app_lorawan.h"
#include "lora_app.h"
#include "sys_app.h"
#include "stm32_seq.h"

#include "stm32_seq.h"

void MX_LoRaWAN_Init(void)
{
  SystemApp_Init();
  LoRaWAN_Init();
}

void MX_LoRaWAN_Process(void)
{
  UTIL_SEQ_Run(UTIL_SEQ_DEFAULT);
}