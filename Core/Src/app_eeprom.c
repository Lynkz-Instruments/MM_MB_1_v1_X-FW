/**
 * @file    app_eeprom.c
 * @brief   Application-level EEPROM interface for the OreTag sensor node.
 *
 * This is the single translation unit that couples the platform-agnostic
 * CAT24C32 driver to the STM32 HAL.  The three static callbacks below
 * (eeprom_i2c_write, eeprom_i2c_read, eeprom_delay_ms) are the only
 * place in the firmware that references hi2c1 for EEPROM traffic.
 *
 *  Author: Lynkz Instruments, 2025–2026
 */
#include "app_eeprom.h"
#include "cat24c32.h"
#include "i2c.h"
#include <stdio.h>
#include <stdbool.h>

#include "app_errors.h"
#include "utils.h"

#define EEPROM_PROVISIONED_FLAG_ADDR            0x00                            // 1 byte — EEPROM_PROVISIONED_MAGIC once on this layout
#define EEPROM_DEVICE_MODE_ADDR                 0x05                            // 1 byte
#define EEPROM_DEVICE_CONFIG_ADDR               0x06                            // 9 bytes (0x06-0x0E)
#define EEPROM_RF_TEST_CONFIG_ADDR              0x0F                            // 11 bytes (0x0F-0x19)
#define EEPROM_LAST_DEVICE_TIME_SYNC_ADDR        0x1A                            // 4 bytes

/* 0xA6, not 0xA5: the old MissMetal firmware stamped 0xA5 here on its own
 * 3-byte-config layout, so a different value is what tells the two apart
 * (see app_settings_init). */
#define EEPROM_PROVISIONED_MAGIC                0xA6

/* --------------------------------------------------------------------------
 * HAL I/O callbacks — injected into the driver via CAT24C32_IO_t
 * -------------------------------------------------------------------------- */

static int8_t eeprom_i2c_write(uint8_t dev_addr, uint8_t *data, uint16_t len)
{
    return HAL_I2C_Master_Transmit(&hi2c1, dev_addr, data, len, HAL_MAX_DELAY) == HAL_OK
        ? CAT24C32_OK : CAT24C32_ERROR;
}

static int8_t eeprom_i2c_read(uint8_t dev_addr, uint8_t *data, uint16_t len)
{
    return HAL_I2C_Master_Receive(&hi2c1, dev_addr, data, len, HAL_MAX_DELAY) == HAL_OK
        ? CAT24C32_OK : CAT24C32_ERROR;
}

static void eeprom_delay_ms(uint32_t ms)
{
    LM_Delay(ms, 0);
}

static const CAT24C32_IO_t eeprom_io = {
    .i2c_write = eeprom_i2c_write,
    .i2c_read  = eeprom_i2c_read,
    .delay_ms  = eeprom_delay_ms,
};

/* --------------------------------------------------------------------------
 * Init
 * -------------------------------------------------------------------------- */

static bool s_initialized = false;

int8_t app_eeprom_init(void)
{
    int8_t rslt = CAT24C32_Init(&eeprom_io, 0);
    if (rslt == CAT24C32_OK) {
        s_initialized = true;
    }
    return rslt;
}

/* --------------------------------------------------------------------------
 * Provisioned flag
 * -------------------------------------------------------------------------- */

bool app_eeprom_is_provisioned(void)
{
    uint8_t flag = 0xFF;
    if (!s_initialized) return false;
    CAT24C32_Read(EEPROM_PROVISIONED_FLAG_ADDR, &flag, 1);
    return flag == EEPROM_PROVISIONED_MAGIC;
}

void app_eeprom_set_provisioned(void)
{
    if (!s_initialized) return;
    uint8_t magic = EEPROM_PROVISIONED_MAGIC;
    CAT24C32_Write(EEPROM_PROVISIONED_FLAG_ADDR, &magic, 1);
}

/* --------------------------------------------------------------------------
 * Mode
 * -------------------------------------------------------------------------- */

void app_eeprom_write_mode(uint8_t mode)
{
    int8_t rslt = CAT24C32_Write(EEPROM_DEVICE_MODE_ADDR, &mode, 1);
    if (rslt != CAT24C32_OK) {
        printf("EEPROM Write Error (Device Mode)!\r\n");
        Error_Handler(ERROR_EEPROM_WRITE);
    }
}

uint8_t app_eeprom_read_mode(void)
{
    uint8_t mode = 0xFF;
    if(!s_initialized) {
        // If the EEPROM isn't initialized, return an invalid mode.
        return mode;
    }
    int8_t rslt = CAT24C32_Read(EEPROM_DEVICE_MODE_ADDR, &mode, sizeof(mode));
    if (rslt != CAT24C32_OK) {
        printf("EEPROM Read Error (Device Mode)!\r\n");
        Error_Handler(ERROR_EEPROM_READ);
        return APP_MODE_OPERATION;
    }
    return mode;
}

/* --------------------------------------------------------------------------
 * Config
 * -------------------------------------------------------------------------- */

void app_eeprom_write_config(struct AppConfig_s config)
{   
    if(!s_initialized) {
        return;
    }

    int8_t rslt = CAT24C32_Write(EEPROM_DEVICE_CONFIG_ADDR, (uint8_t *)&config, sizeof(struct AppConfig_s));
    if (rslt != CAT24C32_OK) {
        printf("EEPROM Write Error (Device Config)!\r\n");
        Error_Handler(ERROR_EEPROM_WRITE);
    }
}

struct AppConfig_s app_eeprom_read_config(void)
{
    struct AppConfig_s config = {0};

    if(!s_initialized) {
        return config;
    }

    int8_t rslt = CAT24C32_Read(EEPROM_DEVICE_CONFIG_ADDR, (uint8_t *)&config, sizeof(struct AppConfig_s));
    if (rslt != CAT24C32_OK) {
        printf("EEPROM Read Error (Device Config)!\r\n");
        Error_Handler(ERROR_EEPROM_READ);
    }
    return config;
}

/* --------------------------------------------------------------------------
 * RF test configuration
 * -------------------------------------------------------------------------- */

void app_eeprom_write_rf_test_config(struct AppRfTestConfig_s config)
{
    if (!s_initialized) return;
    int8_t rslt = CAT24C32_Write(EEPROM_RF_TEST_CONFIG_ADDR, (uint8_t *)&config, sizeof(struct AppRfTestConfig_s));
    if (rslt != CAT24C32_OK) {
        printf("EEPROM Write Error (RF Test Config)!\r\n");
        Error_Handler(ERROR_EEPROM_WRITE);
    }
}

struct AppRfTestConfig_s app_eeprom_read_rf_test_config(void)
{
    struct AppRfTestConfig_s config = {0};
    if (!s_initialized) return config;
    int8_t rslt = CAT24C32_Read(EEPROM_RF_TEST_CONFIG_ADDR, (uint8_t *)&config, sizeof(struct AppRfTestConfig_s));
    if (rslt != CAT24C32_OK) {
        printf("EEPROM Read Error (RF Test Config)!\r\n");
        Error_Handler(ERROR_EEPROM_READ);
    }
    return config;
}

/* --------------------------------------------------------------------------
 * DeviceTimeReq/DeviceTimeAns resync
 * -------------------------------------------------------------------------- */

void app_eeprom_write_last_device_time_sync(uint32_t epoch_seconds)
{
    if (!s_initialized) return;
    int8_t rslt = CAT24C32_Write(EEPROM_LAST_DEVICE_TIME_SYNC_ADDR, (uint8_t *)&epoch_seconds, sizeof(epoch_seconds));
    if (rslt != CAT24C32_OK) {
        printf("EEPROM Write Error (Last Device Time Sync)!\r\n");
        Error_Handler(ERROR_EEPROM_WRITE);
    }
}

uint32_t app_eeprom_read_last_device_time_sync(void)
{
    uint32_t epoch_seconds = 0;
    if (!s_initialized) return epoch_seconds;
    int8_t rslt = CAT24C32_Read(EEPROM_LAST_DEVICE_TIME_SYNC_ADDR, (uint8_t *)&epoch_seconds, sizeof(epoch_seconds));
    if (rslt != CAT24C32_OK) {
        printf("EEPROM Read Error (Last Device Time Sync)!\r\n");
        Error_Handler(ERROR_EEPROM_READ);
    }
    return epoch_seconds;
}

