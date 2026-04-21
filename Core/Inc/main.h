/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32wlxx_hal.h"


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef enum {
    ERROR_BMA_INIT,
    ERROR_BMA_READ_FIFO,
    ERROR_BMA_GET_MAX_ACCEL,
    ERROR_BMA_GET_TEMPERATURE,
    ERROR_EEPROM_INIT,
    ERROR_EEPROM_READ,
    ERROR_EEPROM_WRITE,
    ERROR_RCC_OSC_CONFIG,
    ERROR_RCC_CLOCK_CONFIG,
    ERROR_RTC_INIT,
    ERROR_RTC_SET_ALARM,
    ERROR_RTC_SET_TIMER,
    ERROR_UART_INIT,
    ERROR_I2C_INIT,
    ERROR_DMA_INIT,
    ERROR_TIMER_START,
    ERROR_FLASH_INIT,
    ERROR_SUBGHZ_INIT,
    ERROR_MAX_ACCEL_TOO_HIGH,
    ERROR_MAX_ACCEL_TOO_LOW,
    ERROR_nPM2100_INIT,
    ERROR_nPM2100,
    ERROR_TEST
} ErrorCode_t;

// Enum for defining all the possible device states.
typedef enum {
    APP_MODE_STORAGE = 0,
    APP_MODE_OPERATION,
    APP_MODE_BEACON,
    APP_MODE_MAX
} AppMode_t;

#pragma pack(1)
struct AppConfig_s {
    uint8_t wake_thresh;   // Wake threshold.
    uint16_t sleep_time_minutes;  // Sleep time in minutes (between heart beats).
};
#pragma pack() // Resets alignment to default
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(ErrorCode_t error_code);
void EnterShutdownWithWakeUpPinEn(void);
void SystemClock_Config(void);
void print(const char *fmt, ...);
void LM_Delay(uint32_t delay_ms, uint32_t start_time_ms);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define RTC_PREDIV_A                            ((1<<(15-RTC_N_PREDIV_S))-1)
#define RTC_N_PREDIV_S                          10
#define RTC_PREDIV_S                            ((1<<RTC_N_PREDIV_S)-1)

/* USER CODE BEGIN Private defines */
#define WAKE_SEND_INTERVAL_SEC                  30                              // seconds, if time since last send is greater than this, send payload on next wake up
#define DOWNLINK_WAIT_INTERVAL                  2500                            // ms, time to wait for downlink after sending uplink

#define DEBUG_FLASH_TIME_MS                     500                            // used for debugging to have enough time to flash the board before entering shutdown mode

#define BMA400_ROLLOVER_SECS                    0x654                           //seconds

#define EEPROM_BMA_WAKE_FLAG_VALUE              0x01
#define PAYLOAD_SEND_INTERVAL_SEC               5                               // seconds
#define PAYLOAD_SIZE                            4                               // 2 bytes max acceleration, 2 bytes temperature

#define EEPROM_BMA_WAKE_FLAG_ADDR               0x00                            // 1 byte
#define EEPROM_FCNTUP_ADDR                      0x01                            // 2 bytes
#define EEPROM_MAX_ACCEL_ADDR                   0x03                            // 2 bytes
#define EEPROM_BMA_SENSOR_TIME_ADDR             0x05                            // 4 bytes
#define EEPROM_DEVICE_MODE_ADDR                 0x09                            // 1 bytes for storing the device mode.
#define EEPROM_DEVICE_CONFIG_ADDR               0x10                            // 3 bytes for the device config.

#define TB_DETECT_PIN                           GPIO_PIN_10
#define TB_DETECT_GPIO_PORT                     GPIOB
#define LED_PIN                                 GPIO_PIN_1
#define LED_GPIO_PORT                           GPIOB

#define WAKE_THRESHOLD_MIN                      (1)
#define WAKE_THRESHOLD_MAX                      (100)
#define WAKE_THRESHOLD_DEFAULT                  (10)

#define SLEEP_TIME_MIN                          (1)
#define SLEEP_TIME_MAX                          (120)
#define SLEEP_TIME_DEFAULT                      (5)

/*LoRaWAN send params are defined in lora_app.h and region params are defined in RegionUS915.h */

// #define FIRST_BOOT                                                              // set fcntup to 0 on first boot. Comment for second firmware upload.
/* USER CODE END Private defines */

void app_set_device_mode(AppMode_t mode);
AppMode_t app_get_device_mode(void);
void app_eeprom_write_mode(uint8_t mode);
uint8_t app_eeprom_read_mode(void);

void app_set_device_config(struct AppConfig_s config);
struct AppConfig_s app_get_device_config(void);
void app_eeprom_write_config(struct AppConfig_s config);
struct AppConfig_s app_eeprom_read_config(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
