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

uint32_t g_fcntup = 0;
int8_t g_eeprom_initialized = 0;


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
    float time_diff = 0.0f;
    volatile uint32_t reset_flags = RCC->CSR;
    ResetReason reset_reason = RESET_REASON_NONE;
    int16_t temperature = 0;
    uint8_t clear_flag = 0x00;
    uint8_t eeprom_flag_value = 0;
    (void)time_diff;
    (void)clear_flag;
    (void)eeprom_flag_value;

    getResetReason(reset_flags, &reset_reason);
    RCC->CSR |= RCC_CSR_RMVF; // Clear reset flags after reading

    rslt = app_eeprom_init();
    if (rslt != 0) {
        Error_Handler(ERROR_EEPROM_INIT);
    } else {
        g_eeprom_initialized = 1;
        printf("EEPROM initialized successfully.\r\n");
    }

    // Show mode and configs.
    AppMode_t current_mode = app_get_device_mode();
    printf("Current device mode: %s\r\n", 
        current_mode == APP_MODE_STORAGE ? "STORAGE" :
        current_mode == APP_MODE_OPERATION ? "OPERATION" : 
        current_mode == APP_MODE_BEACON ? "BEACON": "NONE");

    struct AppConfig_s current_config = app_get_device_config();
    printf("Current device config:\r\n");
    printf("Accel. Threshold (%d * 8mg) = %d mg\r\n", current_config.wake_thresh, current_config.wake_thresh * 8);
    printf("Sleep time: %d min\r\n", current_config.sleep_time_minutes);
    
    /* Initialize BMA400 sensor */
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

    // Sending messages based on mode.
    // Mode OPERATION or BEACON
    // TODO: Using BMA400 flag to make sure the device wakes up from BMA400 interrupt pin.
    if (current_mode == APP_MODE_OPERATION || current_mode == APP_MODE_BEACON){
        print("Sending heart beat payload\r\n");
        SendHeartBeatPayload(current_mode, current_config, temperature);
        // blink(2, 100, 0); // Blink twice to indicate that the payload was sent
        HAL_Delay(500);
    }

    // The following logic will be used if flag from BMA400 is get.
    // else if (current_mode == APP_MODE_BEACON /* and no flag from BMA400 */){
    //     print("Sending heart beat payload (BEACON)\r\n");
    //     uint16_t max_accel = 0;
    //     SendHeartBeatPayload(current_mode, current_config, temperature);
    //     blink(2, 100, 0); // Blink twice to indicate that the payload was sent
    //     HAL_Delay(500);
    // }
    // else if (current_mode == APP_MODE_BEACON /* and flag from BMA400 */){
    //     print("Sending data payload (BEACON)\r\n");
    //     uint16_t max_accel = 0;
    //     SendBeaconPayload(&max_accel, &temperature);
    //     blink(2, 100, 0); // Blink twice to indicate that the payload was sent
    //     HAL_Delay(500);
    // }

    // Entering shutdown mode based on current mode.
    // Get the device mode and config in case it changed by a dowmlink.
    current_mode = app_get_device_mode();
    current_config = app_get_device_config();
    if (current_mode == APP_MODE_OPERATION){
        EnterShutdownNoBMA(current_config);
    }
    else{
        EnterShutdownWithBMA(current_config);
    }

    while (1)
    {
        print("This should never print...\r\n");
        HAL_Delay(500);
    }
}
