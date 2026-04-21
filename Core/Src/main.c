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
#include "main.h"
#include "bma400.h"
#include "bma400_api.h"
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

int8_t g_message_was_sent = 0;
uint32_t g_fcntup = 0;
int8_t g_eeprom_initialized = 0;
int8_t g_bma400_initialized = 0;


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
    BMA400_SensorContext bma_ctx;
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
    rslt = BMA400_Init(&hi2c1, &int_status);
    if (rslt != BMA400_OK) {
        printf("Error initializing BMA400: %d\r\n", rslt);
        Error_Handler(ERROR_BMA_INIT);
    }
    g_bma400_initialized = 1;
    printf("BMA400 initialized successfully.\r\n");
    printf("BMA400 wake flag: %d\r\n", int_status);

    // Getting temperature.
    rslt = BMA400_GetTemperature(&bma_ctx.temperature);
    if (rslt != BMA400_OK) {
        printf("Error getting temperature: %d\r\n", rslt);
        Error_Handler(ERROR_BMA_GET_TEMPERATURE);
    
    }
    printf("Temperature: %d dC\r\n", bma_ctx.temperature);

    // Sending messages based on mode.
    // Mode OPERATION or BEACON
    // TODO: Using BMA400 flag to make sure the device wakes up from BMA400 interrupt pin.
    if (current_mode == APP_MODE_OPERATION || current_mode == APP_MODE_BEACON){
        print("Sending heart beat payload\r\n");
        SendHeartBeatPayload(current_mode, current_config, bma_ctx.temperature);
        // blink(2, 100, 0); // Blink twice to indicate that the payload was sent
        HAL_Delay(500);
        g_message_was_sent = 1;
    }

    // The following logic will be used if flag from BMA400 is get.
    // else if (current_mode == APP_MODE_BEACON /* and no flag from BMA400 */){
    //     print("Sending heart beat payload (BEACON)\r\n");
    //     uint16_t max_accel = 0;
    //     SendHeartBeatPayload(current_mode, current_config, bma_ctx.temperature);
    //     blink(2, 100, 0); // Blink twice to indicate that the payload was sent
    //     HAL_Delay(500);
    //     g_message_was_sent = 1;
    // }
    // else if (current_mode == APP_MODE_BEACON /* and flag from BMA400 */){
    //     print("Sending data payload (BEACON)\r\n");
    //     uint16_t max_accel = 0;
    //     SendBeaconPayload(&max_accel, &bma_ctx.temperature);
    //     blink(2, 100, 0); // Blink twice to indicate that the payload was sent
    //     HAL_Delay(500);
    //     g_message_was_sent = 1;
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

void app_set_device_mode(AppMode_t mode)
{   
    // Validate mode.
    if (mode >= APP_MODE_MAX){ return; }
    // Storage is not implemented yet.
    if (mode == APP_MODE_STORAGE){ 
        print("STORAGE mode not implemented\r\n");
        return; 
    }
    
    // Getting current mode.
    AppMode_t current_mode = app_get_device_mode();

    // Write the new mode if different than current mode.
    if (mode != current_mode){
        app_eeprom_write_mode(mode);
        printf("New device mode: %s\r\n", 
        mode == APP_MODE_STORAGE ? "STORAGE" :
        mode == APP_MODE_OPERATION ? "OPERATION" : 
        mode == APP_MODE_BEACON ? "BEACON": "NONE");
    }
}

AppMode_t app_get_device_mode(void)
{
    uint8_t current_mode = 0xFF;

    current_mode = app_eeprom_read_mode();

    // Failsafe if invalid mode written on EEPROM (Empty EEPROM).
    if (current_mode >= APP_MODE_MAX){
        app_eeprom_write_mode(APP_MODE_OPERATION);
        current_mode = app_eeprom_read_mode();
    }

    // to remove once STORAGE mode is implemented.
    if (current_mode == APP_MODE_STORAGE){
        app_eeprom_write_mode(APP_MODE_OPERATION);
        current_mode = app_eeprom_read_mode();
    }

    return current_mode;
}

void app_set_device_config(struct AppConfig_s config)
{   
    // Validate config (clamping)
    if(config.wake_thresh < WAKE_THRESHOLD_MIN || config.wake_thresh > WAKE_THRESHOLD_MAX){
        config.wake_thresh = WAKE_THRESHOLD_DEFAULT;
    }
    if (config.sleep_time_minutes < SLEEP_TIME_MIN || config.sleep_time_minutes > SLEEP_TIME_MAX){
        config.sleep_time_minutes = SLEEP_TIME_DEFAULT;
    }

    app_eeprom_write_config(config);
    printf("New device config set.\r\n");
}

struct AppConfig_s app_get_device_config(void)
{
    struct AppConfig_s current_config = {0};

    current_config = app_eeprom_read_config();

    // Failsafe if invalid mode written on EEPROM (Empty EEPROM).
    bool rewrite = false;
    if(current_config.wake_thresh < WAKE_THRESHOLD_MIN || current_config.wake_thresh > WAKE_THRESHOLD_MAX){
        current_config.wake_thresh = WAKE_THRESHOLD_DEFAULT;
        rewrite = true;
    }
    if (current_config.sleep_time_minutes < SLEEP_TIME_MIN || current_config.sleep_time_minutes > SLEEP_TIME_MAX){
        current_config.sleep_time_minutes = SLEEP_TIME_DEFAULT;
        rewrite = true;
    }

    if (rewrite){
        app_eeprom_write_config(current_config);
        current_config = app_eeprom_read_config();
    }

    return current_config;
}

