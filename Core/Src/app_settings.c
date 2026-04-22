/**
 * @file    app_settings.c
 * @brief   Device mode and configuration management for the MissMetal sensor node.
 *
 *  Author: Lynkz Instruments, 2026
 */

#include "app_settings.h"
#include "app_eeprom.h"
#include <stdio.h>

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
            mode == APP_MODE_BEACON    ? "BEACON"    : "STORAGE");
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
