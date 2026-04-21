/*
-----------
Utils from Charles Marseille, Lynkz Instruments, 2026
-----------
*/
#include "utils.h"
#include "main.h"

extern int8_t g_message_was_sent;
extern uint32_t g_fcntup;
extern int8_t g_eeprom_initialized;
extern int8_t g_bma400_initialized;
extern RTC_HandleTypeDef hrtc;
extern I2C_HandleTypeDef hi2c1;

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler(ERROR_RCC_OSC_CONFIG);
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK3|RCC_CLOCKTYPE_HCLK
                              |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK3Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler(ERROR_RCC_CLOCK_CONFIG);
  }
}

void EnterShutdownWithBMA(struct AppConfig_s config)
{
    LoRaWAN_DeInit();
    if (g_bma400_initialized) {
        // BMA400_GenericInterruptsByActivity();
        // BMA400_ActivityAutoWakeupAutoLP(config.wake_thresh);
        BMA400_configureLPThresholdInterrupt(config.wake_thresh);
    }
    // blink(3, 100, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_HIGH);
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    print("Entering shutdown mode (With BMA).\r\n");
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
    HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, (config.sleep_time_minutes * 60), RTC_WAKEUPCLOCK_CK_SPRE_16BITS, 0);

    HAL_SuspendTick();
    HAL_PWREx_EnterSHUTDOWNMode();
}

void EnterShutdownNoBMA(struct AppConfig_s config)
{
    LoRaWAN_DeInit();
    if (g_bma400_initialized) {
        BMA400_SetLowPowerMode();
    }
    // blink(3, 100, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_HIGH);
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    print("Entering shutdown mode (No BMA).\r\n");
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
    HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, (config.sleep_time_minutes * 60), RTC_WAKEUPCLOCK_CK_SPRE_16BITS, 0);

    HAL_SuspendTick();
    HAL_PWREx_EnterSHUTDOWNMode();
}

void Error_Handler(ErrorCode_t error_code)
{
    print("Error occurred: %d\r\n", error_code);
    print("Sending error code on port 66.\r\n");
    SendErrorCode(error_code);
    g_message_was_sent = 1;
    // blink(2,100,0);
    
    AppMode_t current_mode = app_get_device_mode();
    struct AppConfig_s current_config = app_get_device_config();
    if (current_mode == APP_MODE_OPERATION){
        EnterShutdownNoBMA(current_config);
    }
    else{
        EnterShutdownWithBMA(current_config);
    }
}

/* Robust print function supporting %d and %f (single or multiple) using va_list
*/
void print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const char *p = fmt;
    while (*p) {
        if (*p == '%') {
            p++;
            if (*p == 'd') {
                int ival = va_arg(args, int);
                printf("%d", ival);
                p++;
            } else if (*p == 'f') {
                double dval = va_arg(args, double);
                int32_t val_int = (int32_t)(dval * 1000.0f);
                printf("%s%d.%03d", (val_int < 0) ? "-" : "", abs(val_int)/1000, abs(val_int)%1000);
                p++;
            } else if (*p == '%') {
                putchar('%');
                p++;
            } else {
                // Unknown format, print as is
                putchar('%');
            }
        } else {
            putchar(*p);
            p++;
        }
    }
    va_end(args);
}

void LM_Delay(uint32_t delay_ms, uint32_t start_time_ms){
    uint32_t start = start_time_ms ? start_time_ms : HAL_GetTick();
    while ((HAL_GetTick() - start) < delay_ms) {
        LmHandlerProcess();
    }
}

void blink(int times, int delay_ms, uint8_t end_state) {
    // for (int i = 0; i < times; i++) {
    //     HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
    //     HAL_Delay(100);
    //     HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
    //     HAL_Delay(delay_ms);
    // }
    // if (end_state == 0) {
    //     HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);
    // } else {
    //     HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_SET);
    // }
}