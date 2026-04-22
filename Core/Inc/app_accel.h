/**
 * @file    app_accel.h
 * @brief   Application-level accelerometer interface for the MissMetal sensor node.
 *
 * Wraps the BMA400 driver (bma400_api) and owns the sensor init state.
 * All functions guard on initialization internally; callers do not need to
 * check whether the sensor was successfully initialised before calling.
 *
 *  Author: Lynkz Instruments, 2026
 */

#ifndef APP_ACCEL_H
#define APP_ACCEL_H

#include <stdint.h>
#include "stm32wlxx_hal.h"

/**
 * @brief  Initialise the BMA400 and bind it to the given I2C bus.
 * @param  hi2c      Pointer to the STM32 HAL I2C handle.
 * @param  wake_flag Output — raw interrupt-status word read during init
 *                   (non-zero if this boot was triggered by a BMA400 interrupt).
 * @return 0 on success, negative BMA400 status code on failure.
 */
int8_t app_accel_init(I2C_HandleTypeDef *hi2c, uint16_t *wake_flag);

/**
 * @brief  Read the on-chip temperature.
 * @param  temperature  Output — temperature in units of 0.5 °C.
 * @return 0 on success, negative BMA400 status code on failure.
 *         Returns 0 without writing *temperature if the sensor was not initialised.
 */
int8_t app_accel_get_temperature(int16_t *temperature);

/**
 * @brief  Configure BMA400 for beacon-mode shutdown (threshold interrupt enabled).
 *         No-op if the sensor was not successfully initialised.
 * @param  wake_thresh  Wake threshold in units of 8 mg (1–100).
 */
void app_accel_prepare_beacon_shutdown(uint8_t wake_thresh);

/**
 * @brief  Configure BMA400 for operation-mode shutdown (low power, no interrupt).
 *         No-op if the sensor was not successfully initialised.
 */
void app_accel_prepare_operation_shutdown(void);

#endif /* APP_ACCEL_H */
