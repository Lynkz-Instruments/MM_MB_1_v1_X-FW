/* USER CODE BEGIN Header */
#pragma once
/**
  ******************************************************************************
  * @file    lora_app.h
  * @author  MCD Application Team
  * @brief   Header of application of the LRWAN Middleware
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
#ifndef __LORA_APP_H__
#define __LORA_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include "stm32wl_utils.h"
#include "app_settings.h"
#include "app_accel.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/

/* LoraWAN application configuration (Mw is configured by lorawan_conf.h) */
#define ACTIVE_REGION                               LORAMAC_REGION_US915
// #define ACTIVE_REGION                               LORAMAC_REGION_EU433

/* USER CODE BEGIN EC_CAYENNE_LPP */
/*!
 * CAYENNE_LPP is myDevices Application server.
 */
/*#define CAYENNE_LPP*/
/* USER CODE END EC_CAYENNE_LPP */

/*!
 * Defines the application data transmission duty cycle. 10s, value in [ms].
 */
#define APP_TX_DUTYCYCLE                            100

/*!
 * LoRaWAN Switch class application port
 * @note do not use 224. It is reserved for certification
 */
#define LORAWAN_SWITCH_CLASS_PORT                   3

/*!
 * LoRaWAN default class
 */
#define LORAWAN_DEFAULT_CLASS                       CLASS_A

/*!
 * LoRaWAN default confirm state
 */
#define LORAWAN_DEFAULT_CONFIRMED_MSG_STATE         LORAMAC_HANDLER_UNCONFIRMED_MSG

/*!
 * LoRaWAN Adaptive Data Rate
 * @note Please note that when ADR is enabled the end-device should be static
 */
#define LORAWAN_ADR_STATE                           LORAMAC_HANDLER_ADR_OFF

/*!
 * LoRaWAN Default Data Rate
 * @note Please note that LORAWAN_DEFAULT_DATA_RATE is used only when LORAWAN_ADR_STATE is disabled
 */
#define LORAWAN_DEFAULT_DATA_RATE                   DR_0

/*!
 * Highest data rate SendTxData() may step up to when a payload doesn't fit at
 * LORAWAN_DEFAULT_DATA_RATE (see DatarateForPayload in lora_app.c). DR_3 is the
 * fastest US915 DR on the 125 kHz channels of sub-band 1; DR_4 would only be
 * usable on the single 500 kHz channel 64.
 */
#define LORAWAN_MAX_UPLINK_DATA_RATE                DR_3

/*!
 * LoRaWAN Default Tx output power
 * @note LORAWAN_DEFAULT_TX_POWER must be defined in the [XXXX_MIN_TX_POWER - XXXX_MAX_TX_POWER] range,
         else the end-device uses the XXXX_DEFAULT_TX_POWER value
 */
#define LORAWAN_DEFAULT_TX_POWER                    TX_POWER_0

/*!
 * LoRaWAN default activation type
 */
#define LORAWAN_DEFAULT_ACTIVATION_TYPE             ACTIVATION_TYPE_ABP

/*!
 * LoRaWAN force rejoin even if the NVM context is restored
 * @note useful only when context management is enabled by CONTEXT_MANAGEMENT_ENABLED
 */
#define LORAWAN_FORCE_REJOIN_AT_BOOT                false

/*!
 * User application data buffer size
 */
#define LORAWAN_APP_DATA_BUFFER_MAX_SIZE            242

/*!
 * Default Unicast ping slots periodicity
 *
 * \remark periodicity is equal to 2^LORAWAN_DEFAULT_PING_SLOT_PERIODICITY seconds
 *         example: 2^4 = 16 seconds. The end-device will open an Rx slot every 16 seconds.
 */
#define LORAWAN_DEFAULT_PING_SLOT_PERIODICITY       4

/*!
 * Default response timeout for class b and class c confirmed
 * downlink frames in milli seconds.
 *
 * The value shall not be smaller than RETRANSMIT_TIMEOUT plus
 * the maximum time on air.
 */
#define LORAWAN_DEFAULT_CLASS_B_C_RESP_TIMEOUT      8000

/* USER CODE BEGIN EC */
// Device type according to the Lynkz device types.
// See https://lynkz.atlassian.net/wiki/spaces/LYNKZ/pages/2057568284/Lynkz+Device+Types
#define LYNKZ_DEVICE_TYPE                           (0x04) // MissMetal

#define LORAWAN_USER_ERROR_PORT                     66
#define LORAWAN_USER_BEACON_PORT                    68
#define LORAWAN_USER_STATUS_PORT                    69
#define LORAWAN_USER_RF_TEST_PORT                   70
#define LORAWAN_USER_CALIBRATION_PORT               73

/* USER CODE END EC */

/* Exported macros -----------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief  Init Lora Application
  */
void LoRaWAN_Init(void);
void LoRaWAN_DeInit(void);

/* USER CODE BEGIN EFP */
void SendTxData(void);
void SendBeaconPayload(int16_t x, int16_t y, int16_t z);
void SendCalibrationPayload(int16_t x_mg, int16_t y_mg, int16_t z_mg);
void SendStatusPayload(AppMode_t mode, struct AppConfig_s config);
void SendLoRaModTestPayload(void);
void SendRfTestConfigPayload(void);
void SendErrorCode(uint8_t error_code);
void RadioSetDefaultConfig(void);
void LogSysTime(const char *label);

/**
  * @brief  Arm (or re-arm) the next wake at the wall-clock boundary aligned
  *         to period_minutes, phase-shifted by offset_minutes past midnight
  *         UTC (e.g. period=5/offset=0 wakes at :00/:05/:10/...; period=90/
  *         offset=120 wakes at 02:00/03:30/05:00/...), then switches to a
  *         plain recurring period after that first fire. Safe to call again
  *         any time period_minutes or offset_minutes changes (e.g. via
  *         downlink) - always fully re-aligns rather than just re-periods.
  */
void ScheduleAction(uint16_t period_minutes, uint16_t offset_minutes);

/**
  * @brief  Starts the IWDG safety net: arms hiwdg and the periodic checker
  *         that conditionally feeds it based on recent evidence of a
  *         completed cycle (see IwdgMarkProgress/OnIwdgCheckTimerEvent in
  *         lora_app.c). Call once from main(), after MX_LoRaWAN_Init().
  */
void IwdgMonitor_Start(void);

/**
  * @brief  The periodic action itself: resync check, heartbeat (status
  *         payload, OPERATION and STORAGE) or beacon send, downlink-driven
  *         config re-check, RF test mode handling. Runs once at boot (called
  *         directly) and thereafter on the schedule armed by ScheduleAction()
  *         (invoked via the sequencer task).
  */
void RunScheduledAction(void);

/**
  * @brief  Runs when the BMA400 no-motion or activity interrupt wakes the
  *         device from STOP2 (independent of the scheduled cadence above).
  *         In APP_MODE_BEACON: activity and motion-stopped each send a beacon
  *         ping (used to locate a fallen device) with no schedule change and
  *         no rate limiting.
  */
void RunMotionStatusAction(void);

/**
  * @brief  Runs on a fixed short interval while in APP_MODE_CALIBRATION,
  *         sending per-axis vibration (gravity/orientation removed, see
  *         app_accel_measure_vibration_short_8mg) so wake_thresh and
  *         no_motion_duration_s can be picked from observed real-world
  *         values instead of guessed. Independent of the normal
  *         no-motion/activity interrupts and the scheduled-action cadence -
  *         armed/disarmed by CALIBRATION mode entry/exit alone. Each fire
  *         blocks for ~0.5s (the vibration capture window), well inside
  *         CALIBRATION_INTERVAL_MS.
  */
void RunCalibrationAction(void);

/**
  * @brief  Re-syncs every piece of mode-dependent device configuration
  *         (GEN1/GEN2 interrupt arming, the calibration timer) to match the
  *         given mode in one place, so a mode transition can never leave one
  *         of them stale regardless of which code path detects it -
  *         RefreshModeAndConfig calls this on every downlink-driven mode
  *         change. Exposed (rather than static in lora_app.c) so main() can
  *         also call it once for the initial sync at boot, which
  *         RefreshModeAndConfig can't do since it only reacts to a *change*
  *         of mode and a fresh boot never has one.
  */
void SyncModeConfiguration(AppMode_t mode);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*__LORA_APP_H__*/