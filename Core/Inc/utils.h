#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "rtc.h"
#include "gpio.h"
#include "lora_app.h"
#include "app_errors.h"
#include "app_accel.h"
#include "LmHandler.h"

extern uint32_t g_fcntup;

void SystemClock_Config(void);
void EnterShutdownWithWakeUpPinEn(void);
void EnterShutdownWithBMA(struct AppConfig_s config);
void EnterShutdownNoBMA(struct AppConfig_s config);
void print(const char *fmt, ...);
void blink(int times, int delay_ms, uint8_t end_state);
