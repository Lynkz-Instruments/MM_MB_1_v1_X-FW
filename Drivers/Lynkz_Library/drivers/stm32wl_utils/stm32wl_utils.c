#include "stm32wl_utils.h"

void getResetReason(uint32_t reset_flags, ResetReason *reset_reason) {
    
    if (reset_reason == NULL) {
        printf("Error: reset_reason pointer is NULL\r\n");
        return;
    }
    
    *reset_reason = RESET_REASON_NONE;
    const char *reset_reason_str = "NONE";

    if (reset_flags & RCC_CSR_LPWRRSTF) {
        *reset_reason = RESET_REASON_LPWR;
        reset_reason_str = "LPWR";
    } else if (reset_flags & RCC_CSR_WWDGRSTF) {
        *reset_reason = RESET_REASON_WWDG;
        reset_reason_str = "WWDG";
    } else if (reset_flags & RCC_CSR_IWDGRSTF) {
        *reset_reason = RESET_REASON_IWDG;
        reset_reason_str = "IWDG";
    } else if (reset_flags & RCC_CSR_SFTRSTF) {
        *reset_reason = RESET_REASON_SFTRST;
        reset_reason_str = "SFTRST";
    } else if (reset_flags & RCC_CSR_BORRSTF) {
        *reset_reason = RESET_REASON_BOR;
        reset_reason_str = "BOR";
    } else if (reset_flags & RCC_CSR_PINRSTF) {
        *reset_reason = RESET_REASON_PIN;
        reset_reason_str = "PIN";
    }
    printf("Reset Reason: %s\r\n", reset_reason_str);
    return;
}