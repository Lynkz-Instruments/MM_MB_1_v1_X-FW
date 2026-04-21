/**
 * @file    app_eeprom.h
 * @brief   Application-level EEPROM interface for the MissMetal sensor node.
 *
 * Wraps the platform-agnostic CAT24C32 driver with STM32 HAL I/O and exposes
 * typed read/write helpers for each logical field stored in the EEPROM.
 * All functions call Error_Handler() on I2C failure; callers do not need to
 * check return values except for app_eeprom_init().
 *
 * EEPROM memory map (CAT24C32, 4 kB):
 *   0x00        1 B   BMA400 wake flag
 *   0x01–0x04   4 B   FCntUp (LoRaWAN uplink frame counter, little-endian)
 *   0x05–0x08   4 B   BMA400 sensor time
 *   0x09        1 B   Device mode (AppMode_t)
 *   0x10–0x12   3 B   AppConfig (wake_thresh + sleep_time_minutes)
 *
 *  Author: Lynkz Instruments, 2025–2026
 */

#ifndef APP_EEPROM_H
#define APP_EEPROM_H

#include <stdint.h>
#include "main.h"

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
 * @return Stored mode byte, or APP_MODE_BEACON if the read fails.
 */
uint8_t app_eeprom_read_mode(void);

/**
 * @brief  Persist the application configuration struct to EEPROM.
 * @param  config  Configuration to store (packed, 3 bytes).
 */
void app_eeprom_write_config(struct AppConfig_s config);

/**
 * @brief  Read the application configuration struct from EEPROM.
 * @return Stored configuration, or zero-initialised struct if the read fails.
 */
struct AppConfig_s app_eeprom_read_config(void);

/**
 * @brief  Persist the LoRaWAN uplink frame counter to EEPROM (little-endian).
 * @param  fcntup  Current FCntUp value.
 */
void app_eeprom_write_fcntup(uint32_t fcntup);

/**
 * @brief  Read the LoRaWAN uplink frame counter from EEPROM.
 * @return Stored FCntUp value, or 0 for any byte whose read fails.
 */
uint32_t app_eeprom_read_fcntup(void);

#endif // APP_EEPROM_H
