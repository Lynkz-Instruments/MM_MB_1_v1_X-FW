/*
*  stm32_uart.c
*  uart printf redirection
*  Built for STM32 series
*
*  Created on: March 19, 2025
*  Authors: Charles Marseille
*  Copyright Lynkz Instruments Inc, Amos 2025

Inspired by:
https://www.reddit.com/r/embedded/comments/1bby3qt/the_definitive_guide_to_enabling_printf_on_an/
*/

#include "stm32_uart.h"
#include <string.h>
#include <stdlib.h>

/* Redirect printf to UART */
extern UART_HandleTypeDef huart2;

int __io_putchar(int ch){
    (void) HAL_UART_Transmit(&huart2, (uint8_t *) &ch, 1, 0xFFFFU);
    return ch;
}

int _write(int file, char *ptr, int len) {
  int DataIdx;
  for (DataIdx = 0; DataIdx < len; DataIdx++) {
    __io_putchar(*ptr++);
  }
  return len;
}
