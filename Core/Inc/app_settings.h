/**
 * @file    app_settings.h
 * @brief   Device mode and configuration management for the MissMetal sensor node.
 *
 * Owns the AppMode_t and AppConfig_s types and the validated get/set API that
 * backs them with EEPROM persistence.  All callers should go through this API
 * rather than calling app_eeprom directly for mode/config fields.
 *
 *  Author: Lynkz Instruments, 2026
 */

#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_MODE_STORAGE = 0,
    APP_MODE_OPERATION,
    APP_MODE_BEACON,
    APP_MODE_MAX
} AppMode_t;

/* Packed so the 3-byte EEPROM layout matches the struct exactly. */
#pragma pack(1)
struct AppConfig_s {
    uint8_t  wake_thresh;         /* BMA400 wake threshold (1 LSB = 8 mg) */
    uint16_t sleep_time_minutes;  /* RTC wakeup period */
};
#pragma pack()

#define WAKE_THRESHOLD_MIN      (1)
#define WAKE_THRESHOLD_MAX      (100)
#define WAKE_THRESHOLD_DEFAULT  (10)

#define SLEEP_TIME_MIN          (1)
#define SLEEP_TIME_MAX          (1440)
#define SLEEP_TIME_DEFAULT      (5)

/**
 * @brief  Write a validated mode to EEPROM. No-op if mode is out of range or
 *         STORAGE (not yet implemented).
 */
void app_set_device_mode(AppMode_t mode);

/**
 * @brief  Read the current mode from EEPROM, clamping invalid values to
 *         APP_MODE_OPERATION.
 */
AppMode_t app_get_device_mode(void);

/**
 * @brief  Write a validated config to EEPROM. Out-of-range fields are replaced
 *         with their defaults before writing.
 */
void app_set_device_config(struct AppConfig_s config);

/**
 * @brief  Read the current config from EEPROM, replacing out-of-range fields
 *         with defaults and re-writing if any correction was needed.
 */
struct AppConfig_s app_get_device_config(void);

/* --------------------------------------------------------------------------
 * Frame counter
 * -------------------------------------------------------------------------- */

/**
 * @brief  Load FCntUp from EEPROM.  Must be called once after app_eeprom_init().
 *         On a blank EEPROM (first boot), resets the counter to 0 and stamps
 *         the provisioned flag so subsequent boots load normally.
 */
void app_fcntup_init(void);

/**
 * @brief  Clear FCntUp to 0 and persist to EEPROM.  Use with caution, as this
 *         will cause LoRaWAN uplink frame counter desynchronization.
 */
void app_fcntup_clear(void);

/**
 * @brief  Increment FCntUp and persist the new value to EEPROM.
 */
void app_fcntup_increment_and_save(void);

/**
 * @brief  Return the current FCntUp value.
 */
uint32_t app_get_fcntup(void);

#endif /* APP_SETTINGS_H */
