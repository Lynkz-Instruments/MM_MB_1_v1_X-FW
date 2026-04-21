#ifndef APP_EEPROM_H
#define APP_EEPROM_H

#include <stdint.h>
#include "main.h"

int8_t app_eeprom_init(void);

void app_eeprom_write_mode(uint8_t mode);
uint8_t app_eeprom_read_mode(void);

void app_eeprom_write_config(struct AppConfig_s config);
struct AppConfig_s app_eeprom_read_config(void);

void app_eeprom_write_fcntup(uint32_t fcntup);
uint32_t app_eeprom_read_fcntup(void);

#endif // APP_EEPROM_H
