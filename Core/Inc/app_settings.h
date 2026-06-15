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
    APP_MODE_RF_TEST,
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

/* RF test configuration — persisted independently of AppConfig_s.
 * Packed to 11 bytes; maps directly onto ModParameter_t fields used by
 * RadioSetDefaultConfig().  Stored at EEPROM 0x09–0x13. */
#pragma pack(1)
struct AppRfTestConfig_s {
    uint32_t frequency_hz;  /* carrier frequency in Hz */
    int8_t   power_dbm;     /* TX power (-9 to 22 dBm) */
    uint8_t  bandwidth;     /* BW_xxx: 0=7k 1=12k 2=31k 3=62k 4=125k 5=250k 6=500kHz */
    uint8_t  sf;            /* LoRa spreading factor (5–12) */
    uint8_t  coding_rate;   /* 1=4/5, 2=4/6, 3=4/7, 4=4/8 */
    uint8_t  lna;           /* LNA enable (0/1) */
    uint8_t  pa_boost;      /* PA boost enable (0/1) */
    uint8_t  low_dr_opt;    /* low data-rate optimisation (0/1) */
};
#pragma pack()

#define RF_TEST_FREQ_MIN_HZ       150000000UL
#define RF_TEST_FREQ_MAX_HZ       960000000UL
#define RF_TEST_FREQ_DEFAULT_HZ   433000000UL
#define RF_TEST_POWER_MIN_DBM     (-9)
#define RF_TEST_POWER_MAX_DBM     (22)
#define RF_TEST_POWER_DEFAULT_DBM (22)
#define RF_TEST_BW_MAX            (6)    /* BW_500kHz */
#define RF_TEST_BW_DEFAULT        (4)    /* BW_125kHz */
#define RF_TEST_SF_MIN            (5)
#define RF_TEST_SF_MAX            (12)
#define RF_TEST_SF_DEFAULT        (10)
#define RF_TEST_CR_MIN            (1)
#define RF_TEST_CR_MAX            (4)
#define RF_TEST_CR_DEFAULT        (1)    /* 4/5 */

/**
 * @brief  Return true if two modes differ.
 */
bool app_mode_changed(AppMode_t prev, AppMode_t current);

/**
 * @brief  Return true if any config field differs between prev and current.
 */
bool app_config_changed(struct AppConfig_s prev, struct AppConfig_s current);

/**
 * @brief  Return true if any RF test config field differs between prev and current.
 */
bool app_rf_test_config_changed(struct AppRfTestConfig_s prev, struct AppRfTestConfig_s current);

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

/**
 * @brief  Write a validated RF test config to EEPROM. Out-of-range frequency
 *         is replaced with RF_TEST_FREQ_DEFAULT_HZ before writing.
 */
void app_set_rf_test_config(struct AppRfTestConfig_s config);

/**
 * @brief  Read the RF test config from EEPROM, falling back to defaults if the
 *         stored value is out of range.
 */
struct AppRfTestConfig_s app_get_rf_test_config(void);

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
