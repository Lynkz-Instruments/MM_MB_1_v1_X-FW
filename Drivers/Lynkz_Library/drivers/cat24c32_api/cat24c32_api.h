/**
 * @file    cat24c32_eeprom_api.h
 * @brief   Header file for CAT24C32HU4I-GT3 EEPROM API
 *
 *  Created on: May 9, 2025
 *  Author: Charles Marseille
 *  Copyright Lynkz Instruments Inc, Amos 2025
 */

#ifndef CAT24C32_EEPROM_API_H
#define CAT24C32_EEPROM_API_H

#include "stm32wlxx_hal.h" // Or your specific STM32 HAL header

#ifdef __cplusplus
extern "C" {
#endif

/* Exported defines */
#define CAT24C32_OK    INT8_C(0)
#define CAT24C32_ERROR   INT8_C(-1)

/* Exported functions */
int8_t CAT24C32_Init(I2C_HandleTypeDef *hi2c, uint8_t address_config);
int8_t CAT24C32_Read(uint16_t mem_addr, uint8_t *data, uint16_t size);
int8_t CAT24C32_Write(uint16_t mem_addr, uint8_t *data, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif // CAT24C32_EEPROM_API_H