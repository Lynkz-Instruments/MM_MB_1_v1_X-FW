/**
 * @file    app_accel.c
 * @brief   Application-level accelerometer interface for the MissMetal sensor node.
 *
 * Wraps the Bosch BMA400 vendor driver directly.  The sensor is fixed at
 * I2C address 0x14.
 *
 *  Author: Lynkz Instruments, 2026
 */

#include "app_accel.h"
#include "bma400.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#define BMA400_ADDR             (0x14 << 1)
#define BMA400_MAX_WRITE_LEN    32

#define VIBRATION_SAMPLE_PERIOD_MS  10
#define VIBRATION_ODR_SETTLE_MS     20

/* 100Hz burst, mean removed per axis - a ~0.5s window, short enough to fit
 * inside CALIBRATION_INTERVAL_MS's periodic cadence. */
#define CALIB_VIBRATION_SAMPLE_COUNT 50  /* ~0.5s at 100Hz */

/* BMA400_DATA_SRC_ACC_FILT1 tracks the accel's own configured ODR (25Hz,
 * see app_accel_init), so gen_int_dur (in samples of that source) maps
 * directly to seconds via this rate - matches a known-working reference
 * config (nrf52832 project) that used FILT1 at 25Hz for this exact interrupt. */
#define NO_MOTION_INT_SAMPLE_RATE_HZ 25

/* Short and fixed (not downlink-configurable) - the activity interrupt exists
 * for fast response (resume normal cadence, or beacon-ping while searching
 * for a fallen tooth), not for tunable confirmation like the no-motion side.
 * Lowered from a full 1000ms to 200ms so it's easier to trigger, particularly
 * for beacon mode's "ping faster once moving" use case. */
#define ACTIVITY_INT_DURATION_MS 200

static struct bma400_dev     s_bma;
static I2C_HandleTypeDef    *s_hi2c;
static uint8_t               s_addr = BMA400_ADDR;
static int8_t                s_initialized = 0;

// Bosch driver callbacks

static BMA400_INTF_RET_TYPE bma400_i2c_read(uint8_t reg_addr, uint8_t *reg_data,
                                             uint32_t len, void *intf_ptr)
{
    if (len == 0)          return BMA400_OK;
    if (reg_data == NULL)  return BMA400_E_NULL_PTR;

    uint8_t dev_addr = *(uint8_t *)intf_ptr;

    if (HAL_I2C_Master_Transmit(s_hi2c, dev_addr, &reg_addr, 1, HAL_MAX_DELAY) != HAL_OK)
        return BMA400_E_COM_FAIL;
    if (HAL_I2C_Master_Receive(s_hi2c, dev_addr, reg_data, len, HAL_MAX_DELAY) != HAL_OK)
        return BMA400_E_COM_FAIL;

    return BMA400_OK;
}

static BMA400_INTF_RET_TYPE bma400_i2c_write(uint8_t reg_addr, const uint8_t *reg_data,
                                              uint32_t len, void *intf_ptr)
{
    if (reg_data == NULL || len == 0 || intf_ptr == NULL) return BMA400_E_NULL_PTR;
    if (len > BMA400_MAX_WRITE_LEN)                       return BMA400_E_INVALID_CONFIG;

    uint8_t buf[BMA400_MAX_WRITE_LEN + 1];
    buf[0] = reg_addr;
    memcpy(buf + 1, reg_data, len);

    if (HAL_I2C_Master_Transmit(s_hi2c, *(uint8_t *)intf_ptr, buf, len + 1, HAL_MAX_DELAY) != HAL_OK)
        return BMA400_E_COM_FAIL;

    return BMA400_OK;
}

static void bma400_delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    uint32_t ms = period / 1000;
    if (ms > 0) HAL_Delay(ms);
    /* Busy-wait remainder — crude but functional at low sysclock */
    volatile uint32_t ticks = (period % 1000) * (SystemCoreClock / 1000000) / 10;
    while (ticks--) __NOP();
}

// Public API

int8_t app_accel_init(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == NULL) return BMA400_E_NULL_PTR;

    s_hi2c         = hi2c;
    s_bma.intf_ptr = &s_addr;
    s_bma.intf     = BMA400_I2C_INTF;
    s_bma.read     = bma400_i2c_read;
    s_bma.write    = bma400_i2c_write;
    s_bma.delay_us = bma400_delay_us;

    HAL_Delay(15); /* allow I2C bus to stabilise after wakeup */

    int8_t rslt = BMA400_E_COM_FAIL;
    for (uint8_t attempt = 0; attempt < 6 && rslt != BMA400_OK; attempt++) {
        rslt = bma400_init(&s_bma);
        if (rslt != BMA400_OK && attempt < 5) {
            printf("BMA400 init attempt %d failed, retrying...\r\n", attempt + 1);
            HAL_Delay(10);
        }
    }
    if (rslt != BMA400_OK) {
        printf("BMA400 init failed\r\n");
        return rslt;
    }

    rslt = bma400_soft_reset(&s_bma);
    if (rslt != BMA400_OK) {
        printf("BMA400 soft reset failed\r\n");
        return rslt;
    }

    struct bma400_sensor_conf accel_conf = { .type = BMA400_ACCEL };
    rslt = bma400_get_sensor_conf(&accel_conf, 1, &s_bma);
    if (rslt != BMA400_OK) return rslt;

    accel_conf.param.accel.odr      = BMA400_ODR_25HZ;
    accel_conf.param.accel.range    = BMA400_RANGE_16G;
    accel_conf.param.accel.data_src = BMA400_DATA_SRC_ACCEL_FILT_1;
    rslt = bma400_set_sensor_conf(&accel_conf, 1, &s_bma);
    if (rslt != BMA400_OK) return rslt;

    rslt = bma400_set_power_mode(BMA400_MODE_NORMAL, &s_bma);
    if (rslt != BMA400_OK) {
        printf("BMA400 set normal power mode failed\r\n");
        return rslt;
    }

    s_initialized = 1;
    return BMA400_OK;
}

int8_t app_accel_get_temperature(int16_t *temperature)
{
    if (!s_initialized) return BMA400_OK;

    int8_t rslt = bma400_get_temperature_data(temperature, &s_bma);
    if (rslt != BMA400_OK)
        printf("BMA400 temperature read failed\r\n");
    return rslt;
}

void app_accel_enter_low_power(void)
{
    if (!s_initialized) return;
    bma400_set_power_mode(BMA400_MODE_LOW_POWER, &s_bma);
}

int8_t app_accel_configure_no_motion_interrupt(uint8_t threshold_8mg, uint16_t duration_s)
{
    if (!s_initialized) return BMA400_OK;

    struct bma400_sensor_conf int_conf = { .type = BMA400_GEN1_INT };
    int8_t rslt = bma400_get_sensor_conf(&int_conf, 1, &s_bma);
    if (rslt != BMA400_OK) return rslt;

    int_conf.param.gen_int.gen_int_thres = threshold_8mg;
    int_conf.param.gen_int.gen_int_dur   = duration_s * NO_MOTION_INT_SAMPLE_RATE_HZ;
    int_conf.param.gen_int.axes_sel      = BMA400_AXIS_XYZ_EN;
    int_conf.param.gen_int.data_src      = BMA400_DATA_SRC_ACC_FILT1;
    int_conf.param.gen_int.criterion_sel = BMA400_INACTIVITY_INT;
    int_conf.param.gen_int.evaluate_axes = BMA400_ALL_AXES_INT;
    int_conf.param.gen_int.ref_update    = BMA400_UPDATE_EVERY_TIME;
    int_conf.param.gen_int.hysteresis    = BMA400_HYST_24_MG;
    int_conf.param.gen_int.int_chan      = BMA400_INT_CHANNEL_1;

    rslt = bma400_set_sensor_conf(&int_conf, 1, &s_bma);
    if (rslt != BMA400_OK) return rslt;

    /* Force the physical pin's electrical behaviour explicitly rather than
     * relying on its power-on-reset default, so it's guaranteed to match the
     * GPIO_MODE_IT_RISING/GPIO_NOPULL EXTI config in gpio.c. */
    struct bma400_device_conf pin_conf = { .type = BMA400_INT_PIN_CONF };
    pin_conf.param.int_conf.int_chan = BMA400_INT_CHANNEL_1;
    pin_conf.param.int_conf.pin_conf = BMA400_INT_PUSH_PULL_ACTIVE_1;
    rslt = bma400_set_device_conf(&pin_conf, 1, &s_bma);
    if (rslt != BMA400_OK) return rslt;

    struct bma400_int_enable int_en = { .type = BMA400_GEN1_INT_EN, .conf = BMA400_ENABLE };
    return bma400_enable_interrupt(&int_en, 1, &s_bma);
}

int8_t app_accel_configure_activity_interrupt(uint8_t threshold_8mg)
{
    if (!s_initialized) return BMA400_OK;

    struct bma400_sensor_conf int_conf = { .type = BMA400_GEN2_INT };
    int8_t rslt = bma400_get_sensor_conf(&int_conf, 1, &s_bma);
    if (rslt != BMA400_OK) return rslt;

    int_conf.param.gen_int.gen_int_thres = threshold_8mg;
    int_conf.param.gen_int.gen_int_dur   = (ACTIVITY_INT_DURATION_MS * NO_MOTION_INT_SAMPLE_RATE_HZ) / 1000;
    int_conf.param.gen_int.axes_sel      = BMA400_AXIS_XYZ_EN;
    int_conf.param.gen_int.data_src      = BMA400_DATA_SRC_ACC_FILT1;
    int_conf.param.gen_int.criterion_sel = BMA400_ACTIVITY_INT;
    int_conf.param.gen_int.evaluate_axes = BMA400_ANY_AXES_INT;
    int_conf.param.gen_int.ref_update    = BMA400_UPDATE_EVERY_TIME;
    int_conf.param.gen_int.hysteresis    = BMA400_HYST_24_MG;
    int_conf.param.gen_int.int_chan      = BMA400_INT_CHANNEL_1;

    rslt = bma400_set_sensor_conf(&int_conf, 1, &s_bma);
    if (rslt != BMA400_OK) return rslt;

    /* Same physical pin/electrical config as the no-motion interrupt - see
     * app_accel_configure_no_motion_interrupt. Redundant if that already ran,
     * but harmless/idempotent, and keeps this function self-contained. */
    struct bma400_device_conf pin_conf = { .type = BMA400_INT_PIN_CONF };
    pin_conf.param.int_conf.int_chan = BMA400_INT_CHANNEL_1;
    pin_conf.param.int_conf.pin_conf = BMA400_INT_PUSH_PULL_ACTIVE_1;
    rslt = bma400_set_device_conf(&pin_conf, 1, &s_bma);
    if (rslt != BMA400_OK) return rslt;

    struct bma400_int_enable int_en = { .type = BMA400_GEN2_INT_EN, .conf = BMA400_ENABLE };
    return bma400_enable_interrupt(&int_en, 1, &s_bma);
}

int8_t app_accel_set_activity_interrupt_enabled(bool enable)
{
    if (!s_initialized) return BMA400_OK;

    struct bma400_int_enable int_en = { .type = BMA400_GEN2_INT_EN,
                                         .conf = enable ? BMA400_ENABLE : BMA400_DISABLE };
    return bma400_enable_interrupt(&int_en, 1, &s_bma);
}

int8_t app_accel_set_no_motion_interrupt_enabled(bool enable)
{
    if (!s_initialized) return BMA400_OK;

    struct bma400_int_enable int_en = { .type = BMA400_GEN1_INT_EN,
                                         .conf = enable ? BMA400_ENABLE : BMA400_DISABLE };
    return bma400_enable_interrupt(&int_en, 1, &s_bma);
}

void app_accel_get_motion_status(bool *no_motion, bool *activity)
{
    if (no_motion) *no_motion = false;
    if (activity)  *activity  = false;
    if (!s_initialized) return;

    /* Single read: GEN1 and GEN2 both clear on read from the same status
     * register - reading them via separate calls would let whichever one
     * runs second silently miss a bit the first one already consumed. */
    uint16_t int_status = 0;
    if (bma400_get_interrupt_status(&int_status, &s_bma) != BMA400_OK) return;

    if (no_motion) *no_motion = (int_status & BMA400_ASSERTED_GEN1_INT) != 0;
    if (activity)  *activity  = (int_status & BMA400_ASSERTED_GEN2_INT) != 0;
}

static int8_t accel_set_odr(uint8_t odr)
{
    struct bma400_sensor_conf accel_conf = { .type = BMA400_ACCEL };
    int8_t rslt = bma400_get_sensor_conf(&accel_conf, 1, &s_bma);
    if (rslt != BMA400_OK) return rslt;

    accel_conf.param.accel.odr = odr;
    return bma400_set_sensor_conf(&accel_conf, 1, &s_bma);
}

int8_t app_accel_measure_vibration_short_8mg(int16_t *x_8mg, int16_t *y_8mg, int16_t *z_8mg)
{
    if (x_8mg) *x_8mg = 0;
    if (y_8mg) *y_8mg = 0;
    if (z_8mg) *z_8mg = 0;
    if (!s_initialized) return BMA400_OK;

    /* Burst-capture at 100Hz, remove each axis's own mean over the window
     * (that mean *is* the gravity/orientation component here) to get
     * vibration alone, then report RMS of the per-axis deviation - per-axis
     * so it lines up with wake_thresh's own per-axis "any axis" criterion.
     *
     * (An earlier approach tried reading GEN2's hardware reference registers
     * back asynchronously instead of capturing here - that doesn't work:
     * BMA400_UPDATE_EVERY_TIME refreshes the reference from the same live
     * filter data at the end of every evaluation cycle, so an async I2C read
     * moments later sees ref ~= a very recent sample, not a slow baseline.) */
    int8_t rslt = bma400_set_power_mode(BMA400_MODE_NORMAL, &s_bma);
    if (rslt != BMA400_OK) return rslt;

    rslt = accel_set_odr(BMA400_ODR_100HZ);
    if (rslt != BMA400_OK) return rslt;

    HAL_Delay(VIBRATION_ODR_SETTLE_MS);

    static int16_t xs[CALIB_VIBRATION_SAMPLE_COUNT];
    static int16_t ys[CALIB_VIBRATION_SAMPLE_COUNT];
    static int16_t zs[CALIB_VIBRATION_SAMPLE_COUNT];
    int32_t sum_x = 0, sum_y = 0, sum_z = 0;

    uint32_t next_tick = HAL_GetTick();
    for (uint16_t i = 0; i < CALIB_VIBRATION_SAMPLE_COUNT; i++) {
        struct bma400_sensor_data accel = {0};
        bma400_get_accel_data(BMA400_DATA_ONLY, &accel, &s_bma);
        xs[i] = accel.x; ys[i] = accel.y; zs[i] = accel.z;
        sum_x += accel.x; sum_y += accel.y; sum_z += accel.z;

        next_tick += VIBRATION_SAMPLE_PERIOD_MS;
        while (HAL_GetTick() < next_tick) { __WFI(); }
    }

    int16_t mean_x = (int16_t)(sum_x / CALIB_VIBRATION_SAMPLE_COUNT);
    int16_t mean_y = (int16_t)(sum_y / CALIB_VIBRATION_SAMPLE_COUNT);
    int16_t mean_z = (int16_t)(sum_z / CALIB_VIBRATION_SAMPLE_COUNT);

    uint32_t sq_sum_x = 0, sq_sum_y = 0, sq_sum_z = 0;
    for (uint16_t i = 0; i < CALIB_VIBRATION_SAMPLE_COUNT; i++) {
        int32_t dx = xs[i] - mean_x;
        int32_t dy = ys[i] - mean_y;
        int32_t dz = zs[i] - mean_z;
        sq_sum_x += (uint32_t)(dx * dx);
        sq_sum_y += (uint32_t)(dy * dy);
        sq_sum_z += (uint32_t)(dz * dz);
    }

    uint16_t rms_x = (uint16_t)sqrtf((float)sq_sum_x / CALIB_VIBRATION_SAMPLE_COUNT);
    uint16_t rms_y = (uint16_t)sqrtf((float)sq_sum_y / CALIB_VIBRATION_SAMPLE_COUNT);
    uint16_t rms_z = (uint16_t)sqrtf((float)sq_sum_z / CALIB_VIBRATION_SAMPLE_COUNT);

    int8_t restore_rslt = accel_set_odr(BMA400_ODR_25HZ);
    if (restore_rslt != BMA400_OK) return restore_rslt;

    /* BMA400_RANGE_16G (see app_accel_init): +-16g full scale over the 12-bit
     * signed output = 16000 mg / 2048 = 125/16 mg per LSB (the *data
     * register* scale). Dividing that by 8 converts to the GEN1/GEN2
     * threshold register's own fixed 8 mg/LSB scale - the same unit as
     * wake_thresh - so a calibration reading can be plugged straight into
     * wake_thresh with no unit conversion needed. */
    if (x_8mg) *x_8mg = (int16_t)(((int32_t)rms_x * 125) / 128);
    if (y_8mg) *y_8mg = (int16_t)(((int32_t)rms_y * 125) / 128);
    if (z_8mg) *z_8mg = (int16_t)(((int32_t)rms_z * 125) / 128);

    return BMA400_OK;
}
