/**
 * @file    app_settings.h
 * @brief   Device mode and configuration management for the OreTag sensor node.
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
    APP_MODE_RF_TEST,
    APP_MODE_BEACON,
    APP_MODE_CALIBRATION,
    APP_MODE_MAX
} AppMode_t;

/* Packed so the 9-byte EEPROM layout matches the struct exactly. */
#pragma pack(1)
struct AppConfig_s {
    uint8_t  wake_thresh;             /* BMA400 motion threshold, both directions (1 LSB = 8 mg) */
    uint16_t no_motion_duration_s;    /* Required no-motion duration before waking, seconds */
    uint16_t sleep_time_minutes;      /* Scheduled action period while active */
    uint16_t idle_sleep_time_minutes; /* Scheduled action period while idle (no-motion confirmed) */
    uint16_t schedule_offset_minutes; /* Wall-clock alignment offset, minutes past midnight UTC */
};
#pragma pack()

#define WAKE_THRESHOLD_MIN      (1)
#define WAKE_THRESHOLD_MAX      (100)
#define WAKE_THRESHOLD_DEFAULT  (10)

/* gen_int_dur runs at BMA400 FILT1's rate, tied to the accel's own configured
 * ODR (25Hz, see app_accel_init) - giving a 65535-sample (~655s) hardware
 * ceiling at that rate, capped well under that here. */
#define NO_MOTION_DURATION_MIN_S     (1)
#define NO_MOTION_DURATION_MAX_S     (600)
#define NO_MOTION_DURATION_DEFAULT_S (5)

#define SLEEP_TIME_MIN          (1)
#define SLEEP_TIME_MAX          (10080) /* 7 days */
#define SLEEP_TIME_DEFAULT      (5)

#define IDLE_SLEEP_TIME_MIN_MINUTES     (1)
#define IDLE_SLEEP_TIME_MAX_MINUTES     (10080) /* 7 days */
#define IDLE_SLEEP_TIME_DEFAULT_MINUTES (60)

/* Wall-clock alignment offset for the scheduled action, in minutes past
 * midnight UTC. E.g. sleep_time_minutes=90 with offset=120 wakes at
 * 02:00/03:30/05:00/... instead of the default 00:00/01:30/03:00/... */
#define SCHEDULE_OFFSET_MIN_MINUTES     (0)
#define SCHEDULE_OFFSET_MAX_MINUTES     (1439) /* just under 24h */
#define SCHEDULE_OFFSET_DEFAULT_MINUTES (0)

/* RF test configuration — persisted independently of AppConfig_s.
 * Packed to 11 bytes; maps directly onto ModParameter_t fields used by
 * RadioSetDefaultConfig().  Stored at EEPROM 0x0B–0x15 (see app_eeprom.c). */
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
 * @brief  One-time EEPROM conversion from the old MissMetal layout (3-byte
 *         config, BEACON=2/RF_TEST=3). If the provisioned flag isn't set:
 *         translates the stored mode to the current numbering, erases the
 *         config and RF test config to 0xFF (same as a blank EEPROM, so the
 *         getters fall back to defaults) and stamps the flag. Call once after
 *         app_eeprom_init(), before reading mode or config.
 */
void app_settings_init(void);

/**
 * @brief  Write a validated mode to EEPROM. No-op if mode is out of range.
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
 * DeviceTimeReq/DeviceTimeAns resync
 * -------------------------------------------------------------------------- */

/**
 * @brief  Persist the Unix timestamp (seconds) of the last successful
 *         DeviceTimeAns sync to EEPROM.
 */
void app_set_last_device_time_sync(uint32_t epoch_seconds);

/**
 * @brief  Read the Unix timestamp of the last successful DeviceTimeAns sync.
 * @return Stored timestamp, or 0 if the device has never synced.
 */
uint32_t app_get_last_device_time_sync(void);

#endif /* APP_SETTINGS_H */
