/**
 * @file    app_settings.c
 * @brief   Device mode and configuration management for the MissMetal sensor node.
 *
 *  Author: Lynkz Instruments, 2026
 */

#include "app_settings.h"
#include "app_eeprom.h"
#include <stdio.h>

/* --------------------------------------------------------------------------
 * Frame counter
 * -------------------------------------------------------------------------- */

static uint32_t s_fcntup = 0;

void app_fcntup_init(void)
{
    if (!app_eeprom_is_provisioned()) {
        s_fcntup = 0;
        app_eeprom_write_fcntup(0);
        app_eeprom_set_provisioned();
        printf("First boot: FCntUp reset to 0\r\n");
    } else {
        s_fcntup = app_eeprom_read_fcntup();
        printf("FCntUp: %lu\r\n", s_fcntup);
    }
}

void app_fcntup_clear(void)
{
    s_fcntup = 0;
    app_eeprom_write_fcntup(s_fcntup);
    printf("FCntUp cleared to 0\r\n");
}

void app_fcntup_increment_and_save(void)
{
    s_fcntup++;
    app_eeprom_write_fcntup(s_fcntup);
}

uint32_t app_get_fcntup(void)
{
    return s_fcntup;
}

bool app_mode_changed(AppMode_t prev, AppMode_t current)
{
    return prev != current;
}

bool app_config_changed(struct AppConfig_s prev, struct AppConfig_s current)
{
    return prev.wake_thresh != current.wake_thresh ||
           prev.sleep_time_minutes != current.sleep_time_minutes;
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
    if (mode == APP_MODE_STORAGE) {
        printf("STORAGE mode not implemented\r\n");
        return;
    }

    AppMode_t current_mode = app_get_device_mode();
    if (mode != current_mode) {
        app_eeprom_write_mode(mode);
        printf("New device mode: %s\r\n",
            mode == APP_MODE_OPERATION ? "OPERATION" :
            mode == APP_MODE_BEACON    ? "BEACON"    :
            mode == APP_MODE_RF_TEST   ? "RF_TEST"   : "STORAGE");
    }
}

AppMode_t app_get_device_mode(void)
{
    uint8_t mode = app_eeprom_read_mode();

    /* Clamp uninitialised or out-of-range EEPROM values to OPERATION. */
    if (mode >= APP_MODE_MAX || mode == APP_MODE_STORAGE) {
        app_eeprom_write_mode(APP_MODE_OPERATION);
        mode = APP_MODE_OPERATION;
    }

    return (AppMode_t)mode;
}

void app_set_device_config(struct AppConfig_s config)
{
    if (config.wake_thresh < WAKE_THRESHOLD_MIN || config.wake_thresh > WAKE_THRESHOLD_MAX)
        config.wake_thresh = WAKE_THRESHOLD_DEFAULT;
    if (config.sleep_time_minutes < SLEEP_TIME_MIN || config.sleep_time_minutes > SLEEP_TIME_MAX)
        config.sleep_time_minutes = SLEEP_TIME_DEFAULT;

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
    if (config.sleep_time_minutes < SLEEP_TIME_MIN || config.sleep_time_minutes > SLEEP_TIME_MAX) {
        config.sleep_time_minutes = SLEEP_TIME_DEFAULT;
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
    rf_test_config_apply_defaults(&config);
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
