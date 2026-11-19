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
#include <stdbool.h>
#include "stm32wlxx_hal.h"

/**
 * @brief  Initialise the BMA400 and bind it to the given I2C bus.
 * @param  hi2c      Pointer to the STM32 HAL I2C handle.
 * @return 0 on success, negative BMA400 status code on failure.
 */
int8_t app_accel_init(I2C_HandleTypeDef *hi2c);

/**
 * @brief  Read the on-chip temperature.
 * @param  temperature  Output — temperature in units of 0.5 °C.
 * @return 0 on success, negative BMA400 status code on failure.
 *         Returns 0 without writing *temperature if the sensor was not initialised.
 */
int8_t app_accel_get_temperature(int16_t *temperature);

/**
 * @brief  Put the BMA400 into low power mode. Only call before full chip
 *         shutdown - the sensor must otherwise stay in BMA400_MODE_NORMAL
 *         continuously for the no-motion interrupt engine to keep running.
 *         No-op if the sensor was not successfully initialised.
 */
void app_accel_enter_low_power(void);

/**
 * @brief  Configure and enable the BMA400's generic interrupt 1 for
 *         inactivity (no-motion) detection. Requires the sensor to remain
 *         in BMA400_MODE_NORMAL continuously - Advanced Interrupts (which
 *         this is) are disabled in low power/sleep mode, so the interrupt
 *         engine serving it only runs while the sensor stays in NORMAL.
 *         No-op if the sensor was not successfully initialised.
 * @param  threshold_8mg  Motion threshold below which the sensor is
 *                        considered still, in units of 8 mg (1-100).
 * @param  duration_s     How long that stillness must persist before the
 *                        interrupt fires, in seconds (BMA400 hardware
 *                        ceiling is ~655s via its duration counter).
 * @return 0 on success, negative BMA400 status code on failure.
 */
int8_t app_accel_configure_no_motion_interrupt(uint8_t threshold_8mg, uint16_t duration_s);

/**
 * @brief  Configure and enable the BMA400's generic interrupt 2 for activity
 *         (motion) detection - the mirror of generic interrupt 1's inactivity
 *         check, sharing the same physical INT1 pin. Same BMA400_MODE_NORMAL
 *         requirement as the no-motion interrupt. Fires quickly (a short,
 *         fixed duration, not configurable) rather than requiring sustained
 *         confirmation like the no-motion side.
 *         No-op if the sensor was not successfully initialised.
 * @param  threshold_8mg  Motion threshold above which the sensor is
 *                        considered moving, in units of 8 mg (1-100).
 * @return 0 on success, negative BMA400 status code on failure.
 */
int8_t app_accel_configure_activity_interrupt(uint8_t threshold_8mg);

/**
 * @brief  Enable or disable generic interrupt 2 (activity) without touching
 *         its configuration. GEN2 re-asserts repeatedly for as long as real
 *         motion continues (it is not edge-triggered), so callers that only
 *         want to detect the idle->active *transition* once (rather than
 *         being spammed on every re-assertion while already active) should
 *         disable it right after handling that transition, and only
 *         re-enable it once they actually want to watch for the next one
 *         (e.g. after re-entering the idle state, or when a mode that wants
 *         continuous activity pings - like a locate/beacon mode - is active).
 *         No-op if the sensor was not successfully initialised.
 * @param  enable  true to arm GEN2, false to disable it.
 * @return 0 on success, negative BMA400 status code on failure.
 */
int8_t app_accel_set_activity_interrupt_enabled(bool enable);

/**
 * @brief  Enable or disable generic interrupt 1 (no-motion) without touching
 *         its configuration. Same re-asserts-repeatedly behaviour as GEN2
 *         (see app_accel_set_activity_interrupt_enabled) - once stillness is
 *         confirmed and reported, callers should disable it (there's nothing
 *         new to detect while already known-idle) and only re-enable it once
 *         activity resumes, so the next stillness transition can be caught.
 *         No-op if the sensor was not successfully initialised.
 * @param  enable  true to arm GEN1, false to disable it.
 * @return 0 on success, negative BMA400 status code on failure.
 */
int8_t app_accel_set_no_motion_interrupt_enabled(bool enable);

/**
 * @brief  Read and clear the BMA400 interrupt status in a single register
 *         read, reporting whether the no-motion (generic interrupt 1) and
 *         activity (generic interrupt 2) conditions are asserted. Both share
 *         one clear-on-read status register, so they're combined into one
 *         call deliberately - checking them via separate reads would let
 *         whichever one runs second silently miss a bit the first one
 *         already consumed.
 * @param  no_motion  Output - true if the no-motion interrupt is asserted.
 *                    Pass NULL if not needed.
 * @param  activity   Output - true if the activity interrupt is asserted.
 *                    Pass NULL if not needed.
 *                    Both outputs are false if the sensor was not
 *                    initialised or the read failed.
 */
void app_accel_get_motion_status(bool *no_motion, bool *activity);

/**
 * @brief  Per-axis vibration (gravity/orientation removed) over a short
 *         (~0.5s) 100Hz burst capture, converted to the same unit as
 *         wake_thresh (1 LSB = 8 mg) and reported per-axis (matching
 *         wake_thresh's own per-axis "any axis" criterion). Used for beacon
 *         and calibration payloads. Blocks for the capture duration (~0.5s)
 *         - reconfigures ODR to 100Hz and back to 25Hz.
 * @param  x_8mg, y_8mg, z_8mg  Output — per-axis vibration RMS in units of
 *                              8 mg. Pass NULL for any axis not needed.
 * @return 0 on success, negative BMA400 status code on failure.
 *         Writes zeros without error if the sensor was not initialised.
 */
int8_t app_accel_measure_vibration_short_8mg(int16_t *x_8mg, int16_t *y_8mg, int16_t *z_8mg);

#endif /* APP_ACCEL_H */
