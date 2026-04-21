#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "rtc.h"
#include "gpio.h"
#include "lora_app.h"
#include "bma400_api.h"
#include "LmHandler.h"

extern int8_t g_message_was_sent;
extern uint32_t g_fcntup;
extern int8_t g_eeprom_initialized;

void SystemClock_Config(void);
void EnterShutdownWithWakeUpPinEn(void);
void EnterShutdownWithBMA(struct AppConfig_s config);
void EnterShutdownNoBMA(struct AppConfig_s config);
void Error_Handler(ErrorCode_t error_code);
void SendErrorCode(uint8_t error_code);
void print(const char *fmt, ...);
void blink(int times, int delay_ms, uint8_t end_state);
