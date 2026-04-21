#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "stm32wl5mxx.h"

typedef enum {
    RESET_REASON_NONE,
    RESET_REASON_LPWR,
    RESET_REASON_WWDG,
    RESET_REASON_IWDG,
    RESET_REASON_SFTRST,
    RESET_REASON_BOR,
    RESET_REASON_PIN
} ResetReason;

void getResetReason(uint32_t reset_flags, ResetReason *reset_reason);
