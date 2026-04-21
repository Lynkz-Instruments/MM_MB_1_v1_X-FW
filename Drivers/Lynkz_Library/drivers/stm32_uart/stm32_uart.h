/*
*  stm32_uart.h
*  uart printf redirection
*  Built for STM32 series
*
*  Created on: March 19, 2025
*  Authors: Charles Marseille
*  Copyright Lynkz Instruments Inc, Amos 2025
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef STM32_UART_H
#define STM32_UART_H

#include "stm32wlxx_hal.h"

void UART_Redirect_Init(UART_HandleTypeDef *huart);

int _write(int file, char *data, int len);

void print_float_as_fixed_point(float value);

#endif // STM32_UART_H