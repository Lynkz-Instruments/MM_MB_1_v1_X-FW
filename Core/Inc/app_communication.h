/**
 * @file app_communication.h
 * @author Alexandre Desgagné (alexd@lynkz.ca)
 * @brief 
 * @version 0.1
 * @date 2026-02-16
 * @copyright Copyright (c) Lynkz Instruments Inc, Amos 2026
 * 
 */

#ifndef APP_COMMUNICATION_H
#define APP_COMMUNICATION_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Function to process the downlinks received from LoRa
 * 
 * @param data pointer
 * @param len
 */
void app_comm_lora_process(uint8_t port, uint8_t * data, uint8_t len);

# endif // APP_COMMUNICATION_H
