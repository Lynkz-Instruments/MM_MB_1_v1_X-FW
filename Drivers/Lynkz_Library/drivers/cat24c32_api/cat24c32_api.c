/**
* @file    cat24c32_eeprom_api.c
* @brief   API for CAT24C32HU4I-GT3 EEPROM
*
*          Functions to interface with the CAT24C32HU4I-GT3 EEPROM.
*
*  Created on: May 9, 2025
*  Author: Charles Marseille
*  Copyright Lynkz Instruments Inc, Amos 2025
*/
#include "cat24c32_api.h"
#include <stdio.h>
#include <string.h> // For memcpy
#include <stdlib.h> // For malloc

/* Private defines */
#define CAT24C32_ADDR_BASE (0x50 << 1)    // Base 7-bit I2C address (0x50) shifted left for HAL
#define CAT24C32_PAGE_SIZE 32             // Page size in bytes
#define CAT24C32_MEM_SIZE 4096            // Total memory size in bytes

/* Private variables */
static I2C_HandleTypeDef *cat24c32_i2c;   // I2C handle
static uint8_t cat24c32_address;          // Device address (including A0-A2 pins)

/* Private function prototypes */
static int8_t CAT24C32_I2C_Read(uint16_t mem_addr, uint8_t *data, uint16_t size);
static int8_t CAT24C32_I2C_Write(uint16_t mem_addr, uint8_t *data, uint16_t size);

int8_t CAT24C32_Init(I2C_HandleTypeDef *hi2c, uint8_t address_config) {
	if (hi2c == NULL) {
		printf("Error: hi2c pointer is NULL\r\n");
		return CAT24C32_ERROR;
	}

	if (address_config > 7) {
		return CAT24C32_ERROR;
	}

	cat24c32_i2c = hi2c;
	cat24c32_address = CAT24C32_ADDR_BASE | (address_config << 1);

	uint8_t test_byte;
	if (CAT24C32_I2C_Read(0, &test_byte, 1) != HAL_OK) {
		return CAT24C32_ERROR;
	}

	return CAT24C32_OK;
}

int8_t CAT24C32_Read(uint16_t mem_addr, uint8_t *data, uint16_t size) {

		if (data == NULL) {
				printf("Error: data pointer is NULL\r\n");
				return CAT24C32_ERROR;
		}
	if (mem_addr + size > CAT24C32_MEM_SIZE) {
		return CAT24C32_ERROR;
	}

	if (CAT24C32_I2C_Read(mem_addr, data, size) != HAL_OK) {
		return CAT24C32_ERROR;
	}

	return CAT24C32_OK;
}

int8_t CAT24C32_Write(uint16_t mem_addr, uint8_t *data, uint16_t size) {
    if (data == NULL) {
        printf("Error: data pointer is NULL\r\n");
        return CAT24C32_ERROR;
    }

	if (mem_addr + size > CAT24C32_MEM_SIZE) {
		return CAT24C32_ERROR;
	}

	uint16_t bytes_written = 0;
	while (bytes_written < size) {
		uint16_t current_addr = mem_addr + bytes_written;
		uint16_t remaining_in_page = CAT24C32_PAGE_SIZE - (current_addr % CAT24C32_PAGE_SIZE);
		uint16_t write_size = (size - bytes_written < remaining_in_page) ? (size - bytes_written) : remaining_in_page;

		if (CAT24C32_I2C_Write(current_addr, data + bytes_written, write_size) != HAL_OK) {
			return CAT24C32_ERROR;
		}

		bytes_written += write_size;

		HAL_Delay(5);
	}

	return CAT24C32_OK;
}

static int8_t CAT24C32_I2C_Read(uint16_t mem_addr, uint8_t *data, uint16_t size) {
	uint8_t mem_addr_bytes[2];
	mem_addr_bytes[0] = (mem_addr >> 8) & 0xFF; // MSB
	mem_addr_bytes[1] = mem_addr & 0xFF;        // LSB

	HAL_StatusTypeDef status;

	status = HAL_I2C_Master_Transmit(cat24c32_i2c, cat24c32_address, mem_addr_bytes, 2, HAL_MAX_DELAY);
	if (status != HAL_OK) {
		return status;
	}

	status = HAL_I2C_Master_Receive(cat24c32_i2c, cat24c32_address, data, size, HAL_MAX_DELAY);
	return status;
}

static int8_t CAT24C32_I2C_Write(uint16_t mem_addr, uint8_t *data, uint16_t size) {
	uint8_t mem_addr_bytes[2];
	mem_addr_bytes[0] = (mem_addr >> 8) & 0xFF; // MSB
	mem_addr_bytes[1] = mem_addr & 0xFF;        // LSB

	uint8_t *buf;
	HAL_StatusTypeDef status;
	buf = malloc(size + 2);
	if (buf == NULL) {
		return HAL_ERROR;
	}

	buf[0] = mem_addr_bytes[0]; // MSB
	buf[1] = mem_addr_bytes[1]; // LSB
	memcpy(buf + 2, data, size);

	status = HAL_I2C_Master_Transmit(cat24c32_i2c, cat24c32_address, buf, size + 2, HAL_MAX_DELAY);

	free(buf);
	return status;
}