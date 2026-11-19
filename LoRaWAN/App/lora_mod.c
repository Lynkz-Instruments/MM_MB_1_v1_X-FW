/**
  ******************************************************************************
  * @file    lora_mod.c
  * @author  Alexandre Desgagné (alexd@lynkz.ca)
  * @brief   Control the radio modulation.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "sys_app.h"
#include "lora_mod.h"
#include "radio.h"
#include "stm32_seq.h"
#include "utilities_def.h"
#include "sys_conf.h"

/* Private define ------------------------------------------------------------*/
#define F_868MHz                      868000000
#define P_14dBm                       14
#define SF12                          12
#define CR4o5                         1
#define EMISSION_POWER                P_14dBm
#define CONTINUOUS_TIMEOUT            0xFFFF
#define TX_RADIO_TONE                 (1<<0)
#define DEFAULT_LDR_OPT               2
#define DEFAULT_FSK_DEVIATION         25000
#define DEFAULT_GAUSS_BT              3 /*Lora default in legacy*/

/* Private variables ---------------------------------------------------------*/
static uint8_t RadioState = 0;

static ModParameter_t modParam = { TEST_LORA, F_868MHz, EMISSION_POWER, BW_125kHz, SF12, CR4o5, 0, 0, DEFAULT_FSK_DEVIATION, DEFAULT_LDR_OPT, DEFAULT_GAUSS_BT};

int32_t radio_set_config(ModParameter_t *Param)
{
  UTIL_MEM_cpy_8(&modParam, Param, sizeof(ModParameter_t));

  return 0;
}

int32_t radio_get_config(ModParameter_t *Param)
{
  UTIL_MEM_cpy_8(Param, &modParam, sizeof(ModParameter_t));
  return 0;
}

int32_t radio_tone(void)
{
  if ((RadioState & TX_RADIO_TONE) != TX_RADIO_TONE)
  {
    RadioState |= TX_RADIO_TONE;
    Radio.SetTxContinuousWave(modParam.freq, modParam.power, CONTINUOUS_TIMEOUT);
    return 0;
  }
  return -1;
}

int32_t radio_stop(void)
{
  RadioState = 0;
  Radio.Sleep();
  return 0;
}
