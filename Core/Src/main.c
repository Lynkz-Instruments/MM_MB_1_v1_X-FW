/*
    ******************************************************************************
    * @brief          : MissMetal LoRaWAN Sensor Node Main File
    ******************************************************************************
    * @attention
    * Charles Marseille
    * Lynkz Instruments, 2026
    *
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_errors.h"
#include "app_settings.h"
#include "app_accel.h"
#include "app_eeprom.h"
#include "stm32_uart.h"
#include "dma.h"
#include "i2c.h"
#include "app_lorawan.h"
#include "rtc.h"
#include "usart.h"
#include "gpio.h"
#include "LmHandler.h"
#include "lora_app.h"
#include "radio_board_if.h"
#include "sys_app.h"
#include "stm32wl_utils.h"
#include "utils.h"
#include "stm32_systime.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C1_Init();
    MX_LoRaWAN_Init();                      //calls MX_RTC_Init()
    MX_USART2_UART_Init();

    int rslt = 0;
    uint32_t reset_flags = RCC->CSR;
    ResetReason reset_reason = RESET_REASON_NONE;
    int16_t temperature = 0;

    getResetReason(reset_flags, &reset_reason);
    RCC->CSR |= RCC_CSR_RMVF; // Clear reset flags after reading

    printf("MissMetal %s Started!\r\n", TOSTRING(FW_VERSION_MAJOR) "." TOSTRING(FW_VERSION_MINOR) "." TOSTRING(FW_VERSION_BUGFIX));
    blink_led(2, 50);

    rslt = app_eeprom_init();
    if (rslt != 0) {
        Error_Handler(ERROR_EEPROM_INIT);
    } else {
        printf("EEPROM initialized successfully.\r\n");
        app_fcntup_init();
    }

    AppMode_t current_mode = app_get_device_mode();
    printf("Current device mode: %s\r\n", 
        current_mode == APP_MODE_STORAGE ? "STORAGE" :
        current_mode == APP_MODE_OPERATION ? "OPERATION" : 
        current_mode == APP_MODE_BEACON ? "BEACON": "NONE");

    struct AppConfig_s current_config = app_get_device_config();
    printf("Current device config:\r\n");
    printf("Accel. Threshold (%d * 8mg) = %d mg\r\n", current_config.wake_thresh, current_config.wake_thresh * 8);
    printf("Sleep time: %d min\r\n", current_config.sleep_time_minutes);
    
    uint16_t int_status;
    rslt = app_accel_init(&hi2c1, &int_status);
    if (rslt != 0) {
        printf("Error initializing BMA400: %d\r\n", rslt);
        Error_Handler(ERROR_BMA_INIT);
    }
    printf("BMA400 initialized successfully.\r\n");
    printf("BMA400 wake flag: %d\r\n", int_status);

    rslt = app_accel_get_temperature(&temperature);
    if (rslt != 0) {
        printf("Error getting temperature: %d\r\n", rslt);
        Error_Handler(ERROR_BMA_GET_TEMPERATURE);
    }
    printf("Temperature: %d dC\r\n", temperature);

    // Sending heartbeat payload.
    if (current_mode == APP_MODE_OPERATION || current_mode == APP_MODE_BEACON) {
        printf("Sending heart beat payload\r\n");
        SendHeartBeatPayload(current_mode, current_config, temperature);
        LM_Delay(500, 0);
    }

    // if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) {
    //     print("Debugger attached: skipping SHUTDOWN.\r\n");
    //     while (1) { __NOP(); }
    // }

    // Re-read mode and config in case a downlink changed them.
    current_mode = app_get_device_mode();
    current_config = app_get_device_config();
    if (current_mode == APP_MODE_OPERATION) {
        EnterShutdownNoBMA(current_config);
    } else {
        EnterShutdownWithBMA(current_config);
    }

    while (1) {
        print("This should never print...\r\n");
        HAL_Delay(500);
    }
}
