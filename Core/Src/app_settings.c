/**
 * @file    app_settings.c
 * @brief   Device mode and configuration management for the OreTag sensor node.
 *
 *  Author: Lynkz Instruments, 2026
 */

#include "app_settings.h"
#include "app_eeprom.h"
#include <stdio.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * EEPROM layout conversion
 * -------------------------------------------------------------------------- */

/* Mode numbering used by the old MissMetal firmware. */
#define OLD_MM_MODE_STORAGE     (0)
#define OLD_MM_MODE_OPERATION   (1)
#define OLD_MM_MODE_BEACON      (2)
#define OLD_MM_MODE_RF_TEST     (3)

void app_settings_init(void)
{
    if (app_eeprom_is_provisioned()) {
        return;
    }

    uint8_t old_mode = app_eeprom_read_mode();
    AppMode_t mode;
    switch (old_mode) {
    case OLD_MM_MODE_STORAGE: mode = APP_MODE_STORAGE; break;
    case OLD_MM_MODE_BEACON:  mode = APP_MODE_BEACON;  break;
    case OLD_MM_MODE_RF_TEST: mode = APP_MODE_RF_TEST; break;
    case OLD_MM_MODE_OPERATION:
    default:                  mode = APP_MODE_OPERATION; break;  /* incl. blank 0xFF */
    }
    printf("EEPROM not on the current layout: converting (old mode %d -> %d), config reset to defaults\r\n",
           old_mode, mode);
    app_eeprom_write_mode(mode);

    struct AppConfig_s blank_config;
    memset(&blank_config, 0xFF, sizeof(blank_config));
    app_eeprom_write_config(blank_config);

    struct AppRfTestConfig_s blank_rf;
    memset(&blank_rf, 0xFF, sizeof(blank_rf));
    app_eeprom_write_rf_test_config(blank_rf);

    app_eeprom_set_provisioned();
}

/* --------------------------------------------------------------------------
 * DeviceTimeReq/DeviceTimeAns resync
 * -------------------------------------------------------------------------- */

void app_set_last_device_time_sync(uint32_t epoch_seconds)
{
    app_eeprom_write_last_device_time_sync(epoch_seconds);
}

uint32_t app_get_last_device_time_sync(void)
{
    uint32_t epoch = app_eeprom_read_last_device_time_sync();

    /* Blank/erased EEPROM reads back as 0xFFFFFFFF; treat that the same as
     * 0 (never synced) rather than as a bogus 2106-ish timestamp. */
    if (epoch == 0xFFFFFFFFUL) {
        epoch = 0;
        app_eeprom_write_last_device_time_sync(0);
    }

    return epoch;
}

bool app_mode_changed(AppMode_t prev, AppMode_t current)
{
    return prev != current;
}

bool app_config_changed(struct AppConfig_s prev, struct AppConfig_s current)
{
    return prev.wake_thresh != current.wake_thresh ||
           prev.no_motion_duration_s != current.no_motion_duration_s ||
           prev.sleep_time_minutes != current.sleep_time_minutes ||
           prev.idle_sleep_time_minutes != current.idle_sleep_time_minutes ||
           prev.schedule_offset_minutes != current.schedule_offset_minutes;
}

bool app_rf_test_config_changed(struct AppRfTestConfig_s prev, struct AppRfTestConfig_s current)
{
    return prev.frequency_hz != current.frequency_hz ||
           prev.power_dbm    != current.power_dbm    ||
           prev.bandwidth    != current.bandwidth     ||
           prev.sf           != current.sf            ||
           prev.coding_rate  != current.coding_rate   ||
           prev.lna          != current.lna           ||
           prev.pa_boost     != current.pa_boost      ||
           prev.low_dr_opt   != current.low_dr_opt;
}

void app_set_device_mode(AppMode_t mode)
{
    if (mode >= APP_MODE_MAX) return;

    AppMode_t current_mode = app_get_device_mode();
    if (mode != current_mode) {
        app_eeprom_write_mode(mode);
        printf("New device mode: %s\r\n",
            mode == APP_MODE_OPERATION   ? "OPERATION"   :
            mode == APP_MODE_RF_TEST     ? "RF_TEST"     :
            mode == APP_MODE_BEACON      ? "BEACON"      :
            mode == APP_MODE_CALIBRATION ? "CALIBRATION" : "STORAGE");
    }
}

AppMode_t app_get_device_mode(void)
{
    uint8_t mode = app_eeprom_read_mode();

    /* Clamp uninitialised or out-of-range EEPROM values to OPERATION. */
    if (mode >= APP_MODE_MAX) {
        app_eeprom_write_mode(APP_MODE_OPERATION);
        mode = APP_MODE_OPERATION;
    }

    return (AppMode_t)mode;
}

void app_set_device_config(struct AppConfig_s config)
{
    if (config.wake_thresh < WAKE_THRESHOLD_MIN || config.wake_thresh > WAKE_THRESHOLD_MAX)
        config.wake_thresh = WAKE_THRESHOLD_DEFAULT;
    if (config.no_motion_duration_s < NO_MOTION_DURATION_MIN_S || config.no_motion_duration_s > NO_MOTION_DURATION_MAX_S)
        config.no_motion_duration_s = NO_MOTION_DURATION_DEFAULT_S;
    if (config.sleep_time_minutes < SLEEP_TIME_MIN || config.sleep_time_minutes > SLEEP_TIME_MAX)
        config.sleep_time_minutes = SLEEP_TIME_DEFAULT;
    if (config.idle_sleep_time_minutes < IDLE_SLEEP_TIME_MIN_MINUTES || config.idle_sleep_time_minutes > IDLE_SLEEP_TIME_MAX_MINUTES)
        config.idle_sleep_time_minutes = IDLE_SLEEP_TIME_DEFAULT_MINUTES;
    /* SCHEDULE_OFFSET_MIN_MINUTES is 0, so no lower-bound check is needed
     * for this unsigned field - any uint16_t already satisfies it. */
    if (config.schedule_offset_minutes > SCHEDULE_OFFSET_MAX_MINUTES)
        config.schedule_offset_minutes = SCHEDULE_OFFSET_DEFAULT_MINUTES;

    app_eeprom_write_config(config);
    printf("New device config set.\r\n");
}

struct AppConfig_s app_get_device_config(void)
{
    struct AppConfig_s config = app_eeprom_read_config();
    bool rewrite = false;

    if (config.wake_thresh < WAKE_THRESHOLD_MIN || config.wake_thresh > WAKE_THRESHOLD_MAX) {
        config.wake_thresh = WAKE_THRESHOLD_DEFAULT;
        rewrite = true;
    }
    if (config.no_motion_duration_s < NO_MOTION_DURATION_MIN_S || config.no_motion_duration_s > NO_MOTION_DURATION_MAX_S) {
        config.no_motion_duration_s = NO_MOTION_DURATION_DEFAULT_S;
        rewrite = true;
    }
    if (config.sleep_time_minutes < SLEEP_TIME_MIN || config.sleep_time_minutes > SLEEP_TIME_MAX) {
        config.sleep_time_minutes = SLEEP_TIME_DEFAULT;
        rewrite = true;
    }
    if (config.idle_sleep_time_minutes < IDLE_SLEEP_TIME_MIN_MINUTES || config.idle_sleep_time_minutes > IDLE_SLEEP_TIME_MAX_MINUTES) {
        config.idle_sleep_time_minutes = IDLE_SLEEP_TIME_DEFAULT_MINUTES;
        rewrite = true;
    }
    if (config.schedule_offset_minutes > SCHEDULE_OFFSET_MAX_MINUTES) {
        config.schedule_offset_minutes = SCHEDULE_OFFSET_DEFAULT_MINUTES;
        rewrite = true;
    }

    if (rewrite)
        app_eeprom_write_config(config);

    return config;
}

static void rf_test_config_apply_defaults(struct AppRfTestConfig_s *c)
{
    if (c->frequency_hz < RF_TEST_FREQ_MIN_HZ || c->frequency_hz > RF_TEST_FREQ_MAX_HZ)
        c->frequency_hz = RF_TEST_FREQ_DEFAULT_HZ;
    if (c->power_dbm < RF_TEST_POWER_MIN_DBM || c->power_dbm > RF_TEST_POWER_MAX_DBM)
        c->power_dbm = RF_TEST_POWER_DEFAULT_DBM;
    if (c->bandwidth > RF_TEST_BW_MAX)
        c->bandwidth = RF_TEST_BW_DEFAULT;
    if (c->sf < RF_TEST_SF_MIN || c->sf > RF_TEST_SF_MAX)
        c->sf = RF_TEST_SF_DEFAULT;
    if (c->coding_rate < RF_TEST_CR_MIN || c->coding_rate > RF_TEST_CR_MAX)
        c->coding_rate = RF_TEST_CR_DEFAULT;
    if (c->lna > 1)      c->lna      = 1;
    if (c->pa_boost > 1) c->pa_boost = 1;
    if (c->low_dr_opt > 1) c->low_dr_opt = 0;
}

void app_set_rf_test_config(struct AppRfTestConfig_s config)
{
    rf_test_config_apply_defaults(&config);
    app_eeprom_write_rf_test_config(config);
    printf("New RF test config: freq=%lu Hz pwr=%d dBm bw=%u sf=%u cr=%u\r\n",
           config.frequency_hz, config.power_dbm, config.bandwidth,
           config.sf, config.coding_rate);
}

struct AppRfTestConfig_s app_get_rf_test_config(void)
{
    struct AppRfTestConfig_s config = app_eeprom_read_rf_test_config();
    struct AppRfTestConfig_s before = config;

    /* Blank (erased, all-0xFF) EEPROM: most fields are then out of range and
     * fall back to defaults below, but power_dbm 0xFF reads as -1 dBm, which
     * IS in range and would otherwise stick. */
    const uint8_t *raw = (const uint8_t *)&before;
    bool blank = true;
    for (size_t i = 0; i < sizeof(before); i++) {
        if (raw[i] != 0xFFU) { blank = false; break; }
    }

    rf_test_config_apply_defaults(&config);
    if (blank) {
        config.power_dbm = RF_TEST_POWER_DEFAULT_DBM;
    }
    if (config.frequency_hz != before.frequency_hz ||
        config.power_dbm    != before.power_dbm    ||
        config.bandwidth    != before.bandwidth     ||
        config.sf           != before.sf            ||
        config.coding_rate  != before.coding_rate   ||
        config.lna          != before.lna           ||
        config.pa_boost     != before.pa_boost      ||
        config.low_dr_opt   != before.low_dr_opt) {
        app_eeprom_write_rf_test_config(config);
    }
    return config;
}
