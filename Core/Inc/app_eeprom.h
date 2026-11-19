/**
 * @file    app_eeprom.h
 * @brief   Application-level EEPROM interface for the OreTag sensor node.
 *
 * Wraps the platform-agnostic CAT24C32 driver with STM32 HAL I/O and exposes
 * typed read/write helpers for each logical field stored in the EEPROM.
 * All functions call Error_Handler() on I2C failure; callers do not need to
 * check return values except for app_eeprom_init().
 *
 *  Author: Lynkz Instruments, 2025–2026
 */

#ifndef APP_EEPROM_H
#define APP_EEPROM_H

#include <stdbool.h>
#include <stdint.h>

#include "app_settings.h"

/**
 * @brief  Bind the CAT24C32 driver to the STM32 HAL and verify communication.
 * @return 0 on success, -1 if the EEPROM does not respond.
 */
int8_t app_eeprom_init(void);

/**
 * @brief  Persist the device operating mode (AppMode_t) to EEPROM.
 * @param  mode  Raw mode byte; caller is responsible for range validation.
 */
void app_eeprom_write_mode(uint8_t mode);

/**
 * @brief  Read the device operating mode from EEPROM.
 * @return Stored mode byte, or APP_MODE_OPERATION if the read fails.
 */
uint8_t app_eeprom_read_mode(void);

/**
 * @brief  Persist the application configuration struct to EEPROM.
 * @param  config  Configuration to store (packed, 9 bytes).
 */
void app_eeprom_write_config(struct AppConfig_s config);

/**
 * @brief  Read the application configuration struct from EEPROM.
 * @return Stored configuration, or zero-initialised struct if the read fails.
 */
struct AppConfig_s app_eeprom_read_config(void);

/**
 * @brief  Return true if the provisioned flag byte equals the magic value.
 *         False on a blank/erased EEPROM or if not initialized.
 */
bool app_eeprom_is_provisioned(void);

/**
 * @brief  Write the provisioned magic value to EEPROM.  Call once after
 *         first-boot initialisation is complete.
 */
void app_eeprom_set_provisioned(void);

/**
 * @brief  Persist the RF test configuration struct to EEPROM.
 * @param  config  RF test configuration to store (packed, 11 bytes).
 */
void app_eeprom_write_rf_test_config(struct AppRfTestConfig_s config);

/**
 * @brief  Read the RF test configuration struct from EEPROM.
 * @return Stored configuration, or zero-initialised struct if the read fails.
 */
struct AppRfTestConfig_s app_eeprom_read_rf_test_config(void);

/**
 * @brief  Persist the Unix timestamp of the last successful DeviceTimeAns
 *         sync to EEPROM.
 */
void app_eeprom_write_last_device_time_sync(uint32_t epoch_seconds);

/**
 * @brief  Read the Unix timestamp of the last successful DeviceTimeAns sync.
 * @return Stored timestamp, or 0 if the read fails.
 */
uint32_t app_eeprom_read_last_device_time_sync(void);

#endif // APP_EEPROM_H
