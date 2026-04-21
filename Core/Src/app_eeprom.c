#include "app_eeprom.h"
#include "cat24c32.h"
#include "i2c.h"
#include <stdio.h>

/* HAL-specific I2C callbacks — the only place in the app that touches the HAL/hi2c1 for EEPROM */

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
    HAL_Delay(ms);
}

static const CAT24C32_IO_t eeprom_io = {
    .i2c_write = eeprom_i2c_write,
    .i2c_read  = eeprom_i2c_read,
    .delay_ms  = eeprom_delay_ms,
};

int8_t app_eeprom_init(void)
{
    return CAT24C32_Init(&eeprom_io, 0);
}

/* Mode -------------------------------------------------------------------- */

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
    int8_t rslt = CAT24C32_Read(EEPROM_DEVICE_MODE_ADDR, &mode, sizeof(mode));
    if (rslt != CAT24C32_OK) {
        printf("EEPROM Read Error (Device Mode)!\r\n");
        Error_Handler(ERROR_EEPROM_READ);
        return APP_MODE_BEACON;
    }
    return mode;
}

/* Config ------------------------------------------------------------------ */

void app_eeprom_write_config(struct AppConfig_s config)
{
    int8_t rslt = CAT24C32_Write(EEPROM_DEVICE_CONFIG_ADDR, (uint8_t *)&config, sizeof(struct AppConfig_s));
    if (rslt != CAT24C32_OK) {
        printf("EEPROM Write Error (Device Config)!\r\n");
        Error_Handler(ERROR_EEPROM_WRITE);
    }
}

struct AppConfig_s app_eeprom_read_config(void)
{
    struct AppConfig_s config = {0};
    int8_t rslt = CAT24C32_Read(EEPROM_DEVICE_CONFIG_ADDR, (uint8_t *)&config, sizeof(struct AppConfig_s));
    if (rslt != CAT24C32_OK) {
        printf("EEPROM Read Error (Device Config)!\r\n");
        Error_Handler(ERROR_EEPROM_READ);
    }
    return config;
}

/* Frame counter ----------------------------------------------------------- */

void app_eeprom_write_fcntup(uint32_t fcntup)
{
    for (size_t i = 0; i < sizeof(fcntup); i++) {
        uint8_t buf = (fcntup >> (8 * i)) & 0xFF;
        CAT24C32_Write(EEPROM_FCNTUP_ADDR + i, &buf, 1);
    }
}

uint32_t app_eeprom_read_fcntup(void)
{
    uint32_t fcntup = 0;
    for (size_t i = 0; i < sizeof(fcntup); i++) {
        uint8_t buf = 0;
        if (CAT24C32_Read(EEPROM_FCNTUP_ADDR + i, &buf, 1) == CAT24C32_OK) {
            fcntup |= ((uint32_t)buf << (8 * i));
        }
    }
    return fcntup;
}
