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

#define BMA400_ADDR             (0x14 << 1)
#define BMA400_MAX_WRITE_LEN    32
#define ACCEL_WAKE_REF          UINT8_C(0)

static struct bma400_dev     s_bma;
static I2C_HandleTypeDef    *s_hi2c;
static uint8_t               s_addr = BMA400_ADDR;
static int8_t                s_initialized = 0;

/* ── Bosch driver callbacks ─────────────────────────────────────────────── */

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

/* ── Public API ─────────────────────────────────────────────────────────── */

int8_t app_accel_init(I2C_HandleTypeDef *hi2c, uint16_t *wake_flag)
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

    rslt = bma400_get_interrupt_status(wake_flag, &s_bma);
    if (rslt != BMA400_OK) return rslt;

    struct bma400_sensor_conf accel_conf = { .type = BMA400_ACCEL };
    rslt = bma400_get_sensor_conf(&accel_conf, 1, &s_bma);
    if (rslt != BMA400_OK) return rslt;

    accel_conf.param.accel.odr      = BMA400_ODR_12_5HZ;
    accel_conf.param.accel.range    = BMA400_RANGE_16G;
    accel_conf.param.accel.data_src = BMA400_DATA_SRC_ACCEL_FILT_LP;
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

void app_accel_prepare_beacon_shutdown(uint8_t wake_thresh)
{
    if (!s_initialized) return;

    struct bma400_sensor_conf int_conf = { .type = BMA400_GEN1_INT };
    if (bma400_get_sensor_conf(&int_conf, 1, &s_bma) != BMA400_OK) return;

    int_conf.param.gen_int.gen_int_thres   = wake_thresh;
    int_conf.param.gen_int.gen_int_dur     = 5;
    int_conf.param.gen_int.axes_sel        = BMA400_AXIS_XYZ_EN;
    int_conf.param.gen_int.data_src        = BMA400_DATA_SRC_ACC_FILT2;
    int_conf.param.gen_int.criterion_sel   = BMA400_ACTIVITY_INT;
    int_conf.param.gen_int.ref_update      = BMA400_UPDATE_EVERY_TIME;
    int_conf.param.gen_int.hysteresis      = BMA400_HYST_0_MG;
    int_conf.param.gen_int.int_thres_ref_x = ACCEL_WAKE_REF;
    int_conf.param.gen_int.int_thres_ref_y = ACCEL_WAKE_REF;
    int_conf.param.gen_int.int_thres_ref_z = ACCEL_WAKE_REF;
    int_conf.param.gen_int.int_chan        = BMA400_INT_CHANNEL_1;

    if (bma400_set_sensor_conf(&int_conf, 1, &s_bma) != BMA400_OK) return;

    struct bma400_int_enable int_en = { .type = BMA400_GEN1_INT_EN, .conf = BMA400_ENABLE };
    bma400_enable_interrupt(&int_en, 1, &s_bma);
}

void app_accel_prepare_operation_shutdown(void)
{
    if (!s_initialized) return;
    bma400_set_power_mode(BMA400_MODE_LOW_POWER, &s_bma);
}
