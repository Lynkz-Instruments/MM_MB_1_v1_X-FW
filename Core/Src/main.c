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

/* TESTING ONLY: uncomment to force a device mode at every boot. The mode is
 * written to EEPROM (the rest of the firmware reads it from there), so it
 * persists after this line is commented out again - switch back with a 0xA2
 * downlink or by forcing the original mode once. */
// #define FORCE_DEVICE_MODE   APP_MODE_STORAGE

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    bool i2c_was_stuck = i2c_BusRecover();  // before init: unstick SDA after a mid-transfer reset
    MX_I2C1_Init();
    MX_LoRaWAN_Init();                      //calls MX_RTC_Init()
    MX_USART2_UART_Init();

    // Needs the RTC/UTIL_TIMER already initialised (above) - starts hiwdg's
    // hardware countdown immediately, so keep this right after them and
    // before anything else below that could legitimately take a while
    // (BMA400 init, LoRaWAN join); IWDG_PROGRESS_BOOT_MARGIN_MINUTES in
    // lora_app.c covers that gap.
    IwdgMonitor_Start();

    int rslt = 0;
    uint32_t reset_flags = RCC->CSR;
    ResetReason reset_reason = RESET_REASON_NONE;
    int16_t temperature = 0;

    getResetReason(reset_flags, &reset_reason);
    RCC->CSR |= RCC_CSR_RMVF; // Clear reset flags after reading

    printf("MissMetal %s Started!\r\n", TOSTRING(FW_VERSION_MAJOR) "." TOSTRING(FW_VERSION_MINOR) "." TOSTRING(FW_VERSION_BUGFIX));
    printf("Reset reason: %s (RCC_CSR=0x%08lX)\r\n",
        reset_reason == RESET_REASON_LPWR   ? "LPWR"   :
        reset_reason == RESET_REASON_WWDG   ? "WWDG"   :
        reset_reason == RESET_REASON_IWDG   ? "IWDG"   :
        reset_reason == RESET_REASON_SFTRST ? "SFTRST" :
        reset_reason == RESET_REASON_BOR    ? "BOR (power-on / SHUTDOWN wake)" :
        reset_reason == RESET_REASON_PIN    ? "PIN"    : "NONE",
        (unsigned long)reset_flags);
    if (i2c_was_stuck) {
        printf("I2C bus was stuck (SDA held low) - recovered\r\n");
    }
    gpio_blink_led(2, 50);

    rslt = app_eeprom_init();
    if (rslt != 0) {
        Error_Handler(ERROR_EEPROM_INIT);
    } else {
        printf("EEPROM initialized successfully.\r\n");
        app_settings_init();
    }

#ifdef FORCE_DEVICE_MODE
    if (app_get_device_mode() != FORCE_DEVICE_MODE) {
        printf("FORCE_DEVICE_MODE: overriding EEPROM mode\r\n");
        app_set_device_mode(FORCE_DEVICE_MODE);
    }
#endif

    AppMode_t current_mode = app_get_device_mode();
    printf("Current device mode: %s\r\n",
        current_mode == APP_MODE_STORAGE     ? "STORAGE"     :
        current_mode == APP_MODE_OPERATION   ? "OPERATION"   :
        current_mode == APP_MODE_RF_TEST     ? "RF_TEST"     :
        current_mode == APP_MODE_BEACON      ? "BEACON"      :
        current_mode == APP_MODE_CALIBRATION ? "CALIBRATION" : "UNKNOWN");

    struct AppConfig_s current_config = app_get_device_config();
    printf("Current device config:\r\n");
    printf("Accel. Threshold (%d * 8mg) = %d mg\r\n", current_config.wake_thresh, current_config.wake_thresh * 8);
    printf("No-motion duration: %d s\r\n", current_config.no_motion_duration_s);
    printf("Sleep time: %d min\r\n", current_config.sleep_time_minutes);
    printf("Idle sleep time: %d min\r\n", current_config.idle_sleep_time_minutes);

    rslt = app_accel_init(&hi2c1);
    if (rslt != 0) {
        printf("Error initializing BMA400: %d\r\n", rslt);
        Error_Handler(ERROR_BMA_INIT);
    }
    printf("BMA400 initialized successfully.\r\n");

    rslt = app_accel_get_temperature(&temperature);
    if (rslt != 0) {
        printf("Error getting temperature: %d\r\n", rslt);
        Error_Handler(ERROR_BMA_GET_TEMPERATURE);
    }
    printf("Temperature: %d dC\r\n", temperature);

    rslt = app_accel_configure_no_motion_interrupt(current_config.wake_thresh, current_config.no_motion_duration_s);
    if (rslt != 0) {
        printf("Error configuring no-motion interrupt: %d\r\n", rslt);
    }

    rslt = app_accel_configure_activity_interrupt(current_config.wake_thresh);
    if (rslt != 0) {
        printf("Error configuring activity interrupt: %d\r\n", rslt);
    }

    // app_accel_configure_no_motion_interrupt()/app_accel_configure_activity_interrupt()
    // above enable GEN1/GEN2 unconditionally - both are actually only meaningful
    // in specific modes (see SyncModeConfiguration), so
    // explicitly (re-)sync everything mode-dependent (GEN1, GEN2, the
    // calibration timer) here the same way a downlink-driven mode change does
    // via RefreshModeAndConfig - a fresh boot never sees a "change" for that
    // path to react to, so it needs this explicit initial call instead.
    SyncModeConfiguration(current_mode);

    // GEN1_INT (the no-motion interrupt) is an Advanced Interrupt - the BMA400's
    // interrupt engine that serves it is disabled in low power/sleep mode, so
    // the sensor must stay in BMA400_MODE_NORMAL continuously for it to work.

    // Run the first cycle immediately (heartbeat, downlink re-check, RF_TEST
    // if applicable), then arm the recurring wall-clock-aligned schedule and hand
    // off to the sequencer, which sleeps in STOP2 between scheduled/radio events.
    RunScheduledAction();
    struct AppConfig_s scheduled_config = app_get_device_config();
    ScheduleAction(scheduled_config.sleep_time_minutes, scheduled_config.schedule_offset_minutes);

    // Short LED flash every GPIO_ALIVE_BLINK_PERIOD_MS while idling in STOP2.
    gpio_alive_blink_start();

    while (1) {
        MX_LoRaWAN_Process();
    }
}
