/**
    * @file    bma400_api.c
    * @brief   API for BMA400 accelerometer sensor
    * 
    *
    *  high-level functions to interface with the BMA400 accelerometer sensor.
    *
    *  Created on: Mar 20 , 2025
    *  Author: Charles Marseille
    *  Copyright Lynkz Instruments Inc, Amos 2025
 */

#include "bma400_api.h"
#include <stdio.h>
#include <math.h>

/* Private variables */
static struct bma400_dev bma;
static struct bma400_dev bma1;
static struct bma400_dev bma2;
static I2C_HandleTypeDef *bma_i2c;
static uint8_t bma_address1 = 0x15 << 1; // 7-bit address shifted for HAL
static uint8_t bma_address2 = 0x14 << 1; // 7-bit address shifted for HAL

static float scale_factor = SCALE_FACTOR; // Default scale factor, will be set based on range

/* Private function prototypes */
static BMA400_INTF_RET_TYPE bma400_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
static BMA400_INTF_RET_TYPE bma400_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);
static void bma400_delay_us(uint32_t period, void *intf_ptr);
static float get_scale_factor_from_range(uint8_t range);


int8_t BMA400_Init(I2C_HandleTypeDef *hi2c, uint16_t * int_status)
{
    if (hi2c == NULL) {
        printf("Error: hi2c pointer is NULL\r\n");
        return BMA400_E_NULL_PTR;
    }

    int8_t rslt = 1;
    uint8_t power_mode = BMA400_MODE_NORMAL;
    struct bma400_sensor_conf accel_setting;
    struct bma400_device_conf fifo_conf;

    bma_i2c = hi2c;
    bma.intf_ptr = &bma_address1;
    bma.intf = BMA400_I2C_INTF;
    bma.read = bma400_i2c_read;
    bma.write = bma400_i2c_write;
    bma.delay_us = bma400_delay_us;

    bma_i2c = hi2c;
    bma1.intf_ptr = &bma_address1;
    bma1.intf = BMA400_I2C_INTF;
    bma1.read = bma400_i2c_read;
    bma1.write = bma400_i2c_write;
    bma1.delay_us = bma400_delay_us;

    bma2.intf_ptr = &bma_address2;
    bma2.intf = BMA400_I2C_INTF;
    bma2.read = bma400_i2c_read;
    bma2.write = bma400_i2c_write;
    bma2.delay_us = bma400_delay_us;

    // Wait for I2C bus to stabilize after power-on/wakeup
    HAL_Delay(15);

    // Retry initialization up to 6 times to handle transient I2C issues
    uint8_t retry_count = 0;
    const uint8_t max_retries = 6;
    
    do {
        if (bma400_init(&bma1) == BMA400_OK){
            printf("BMA address used: 15\r\n");
            rslt = BMA400_OK;
            break;
        }
        else if (bma400_init(&bma2) == BMA400_OK){
            printf("BMA address used: 14\r\n");
            rslt = BMA400_OK;
            break;
        }
        else {
            printf("BMA init failed\r\n");
            rslt = -1;
        }
        
        retry_count++;
        if (retry_count < max_retries) {
            printf("BMA400 init attempt %d failed (err=%d), retrying...\r\n", retry_count, rslt);
            HAL_Delay(10);
        }
    } while (retry_count < max_retries);
    
    if (rslt != BMA400_OK) {
        printf("BMA400 init failed after %d attempts (err=%d)\r\n", max_retries, rslt);
        return rslt;
    }

    rslt = (bma400_get_interrupt_status(int_status, &bma1) == BMA400_OK) ? BMA400_OK : bma400_get_interrupt_status(int_status, &bma2);
    if (rslt != BMA400_OK) {
        return rslt;
    }

    accel_setting.type = BMA400_ACCEL;

    // Get present settings
    rslt = (bma400_get_sensor_conf(&accel_setting, 1, &bma1) == BMA400_OK) ? BMA400_OK : bma400_get_sensor_conf(&accel_setting, 1, &bma2);
    if (rslt != BMA400_OK) {
        return rslt;
    }
    printf("Current Accel ODR: %d, Range: %d, Data Source: %d\r\n",
           accel_setting.param.accel.odr, accel_setting.param.accel.range, accel_setting.param.accel.data_src);

    accel_setting.param.accel.odr = BMA400_ODR_12_5HZ;
    accel_setting.param.accel.range = BMA400_RANGE_16G;
    accel_setting.param.accel.data_src = BMA400_DATA_SRC_ACCEL_FILT_LP;

    rslt = (bma400_set_sensor_conf(&accel_setting, 1, &bma1) == BMA400_OK) ? BMA400_OK : bma400_set_sensor_conf(&accel_setting, 1, &bma2);
    if (rslt != BMA400_OK) {
        return rslt;
    }

    // Set scale factor based on range
    scale_factor = get_scale_factor_from_range(accel_setting.param.accel.range);
    if (scale_factor < 0.0f) {
        return BMA400_E_INVALID_CONFIG;
    }
    printf("BMA400 initialized with range: %d, scale factor: %d\r\n", accel_setting.param.accel.range, (int)scale_factor);

    rslt = (bma400_get_power_mode(&power_mode, &bma1) == BMA400_OK) ? BMA400_OK : bma400_get_power_mode(&power_mode, &bma2);
    if (rslt != BMA400_OK) {
        return rslt;
    }
    printf("Current power mode: %s\r\n",(power_mode == BMA400_MODE_NORMAL) ? "NORMAL" :(power_mode == BMA400_MODE_SLEEP) ? "SLEEP" : "LOW POWER");

    rslt = (bma400_set_power_mode(BMA400_MODE_NORMAL, &bma1) == BMA400_OK) ? BMA400_OK : bma400_set_power_mode(BMA400_MODE_NORMAL, &bma2);
    if (rslt != BMA400_OK) {
        printf("Error setting power mode to normal\r\n");
    }

    return rslt;
}


int8_t BMA400_SetLowPowerMode()
{
    int8_t rslt;
    uint8_t power_mode;
    rslt = (bma400_get_power_mode(&power_mode, &bma1) == BMA400_OK) ? BMA400_OK : bma400_get_power_mode(&power_mode, &bma2);
    if (rslt != BMA400_OK) {
        return rslt;
    }
    printf("Current power mode: %s\r\n", (power_mode == BMA400_MODE_NORMAL) ? "NORMAL" :(power_mode == BMA400_MODE_SLEEP) ? "SLEEP" : "LOW POWER");
    
    if (power_mode != BMA400_MODE_LOW_POWER) {
        printf("Setting power mode to Low Power\r\n");

        if (bma400_set_power_mode(BMA400_MODE_LOW_POWER, &bma1) == BMA400_OK){
            printf("BMA address used: 15\r\n");
            rslt = BMA400_OK;
        }
        else{
            rslt = -1;
        }
        if (bma400_set_power_mode(BMA400_MODE_LOW_POWER, &bma2) == BMA400_OK){
            printf("BMA address used: 14\r\n");
            rslt = BMA400_OK;
        }
        else{
            rslt = -1;
        }
    }
    
    if (rslt != BMA400_OK) {
        printf("Error setting power mode to LOW POWER\r\n");
    }

    return rslt;
}


int8_t BMA400_GetAcceleration_g(BMA400_AccelData_g *accel_g, uint8_t print_values)
{
    int8_t rslt;
    struct bma400_sensor_data accel_data;
    
    rslt = bma400_get_accel_data(BMA400_DATA_ONLY, &accel_data, &bma);
    if (rslt == BMA400_OK) {
        int16_t x = accel_data.x & 0x0FFF;
        int16_t y = accel_data.y & 0x0FFF;
        int16_t z = accel_data.z & 0x0FFF;

        if (x > 2047) {
            x -= 4096;
        }
        if (y > 2047) {
            y -= 4096;
        }
        if (z > 2047) {
            z -= 4096;
        }

        accel_g->x = x / scale_factor;
        accel_g->y = y / scale_factor;
        accel_g->z = z / scale_factor;
    }

    if (print_values) {
        int32_t x_int = (int32_t)(accel_g->x * 1000.0f);
        int32_t y_int = (int32_t)(accel_g->y * 1000.0f);
        int32_t z_int = (int32_t)(accel_g->z * 1000.0f);

        // Print in format X: -0.123
        printf("X: %s%d.%03d, Y: %s%d.%03d, Z: %s%d.%03d\r\r\n", 
        (x_int < 0) ? "-" : "", abs(x_int)/1000, abs(x_int)%1000,
        (y_int < 0) ? "-" : "", abs(y_int)/1000, abs(y_int)%1000,
        (z_int < 0) ? "-" : "", abs(z_int)/1000, abs(z_int)%1000);
    }
    
    return rslt;
}


int8_t BMA400_GetRawAcceleration(BMA400_AccelData_raw *accel_raw)
{
    int8_t rslt;
    struct bma400_sensor_data accel_data;
    
    rslt = bma400_get_accel_data(BMA400_DATA_ONLY, &accel_data, &bma);
    if (rslt == BMA400_OK) {
        accel_raw->x = accel_data.x;
        accel_raw->y = accel_data.y;
        accel_raw->z = accel_data.z;
    }
    
    return rslt;
}



uint8_t BMA400_GetRange(void)
{
    int8_t rslt;
    struct bma400_sensor_conf accel_setting;
    uint8_t range_val = 0;

    accel_setting.type = BMA400_ACCEL;
    
    rslt = bma400_get_sensor_conf(&accel_setting, 1, &bma);
    if (rslt == BMA400_OK) {
        scale_factor = get_scale_factor_from_range(accel_setting.param.accel.range);
        if (scale_factor < 0.0f) {
            return BMA400_E_INVALID_CONFIG;
        }
        return accel_setting.param.accel.range;
    }

    return range_val;
}


int8_t BMA400_SetRange(uint8_t range)
{
    int8_t rslt;
    struct bma400_sensor_conf accel_setting;
    
    accel_setting.type = BMA400_ACCEL;
    
    rslt = bma400_get_sensor_conf(&accel_setting, 1, &bma);
    if (rslt != BMA400_OK) {
        return rslt;
    }

    accel_setting.param.accel.range = range;
    
    scale_factor = get_scale_factor_from_range(range);
    if (scale_factor < 0.0f) {
        return BMA400_E_INVALID_CONFIG;
    }
    
    rslt = bma400_set_sensor_conf(&accel_setting, 1, &bma);
    
    return rslt;
}


int8_t BMA400_GetChipID(uint8_t *chip_id)
{
    return bma400_get_regs(BMA400_REG_CHIP_ID, chip_id, 1, &bma);
}

int8_t BMA400_GetAccelData(void)
{
    int8_t rslt;
    struct bma400_sensor_data accel_data;
    rslt = bma400_get_accel_data(BMA400_DATA_ONLY, &accel_data, &bma);
    if (rslt == BMA400_OK) {
        int16_t x = accel_data.x;
        int16_t y = accel_data.y;
        int16_t z = accel_data.z;
    
        printf("X: %d, Y: %d, Z: %d\r\r\n", x, y, z); 
    }

    return rslt;
}

int8_t BMA400_configureLPThresholdInterrupt(uint8_t accel_thresh)
{
    int8_t rslt;
    struct bma400_int_enable int_en;
    struct bma400_sensor_conf int_conf;

    int_conf.type = BMA400_GEN1_INT;

    /* Configure the generic interrupt */
    rslt = (bma400_get_sensor_conf(&int_conf, 1, &bma1) == BMA400_OK) ? BMA400_OK : bma400_get_sensor_conf(&int_conf, 1, &bma2);
    if (rslt != BMA400_OK) {
        return rslt;
    }

    /* Configure the generic interrupt settings */
    int_conf.param.gen_int.gen_int_thres = accel_thresh;      /*  */
    int_conf.param.gen_int.gen_int_dur = 5;                           /* Duration of 1 sample */
    int_conf.param.gen_int.axes_sel = BMA400_AXIS_XYZ_EN;             /* Enable all axes */
    int_conf.param.gen_int.data_src = BMA400_DATA_SRC_ACC_FILT2;      /* Use filtered data */
    int_conf.param.gen_int.criterion_sel = BMA400_ACTIVITY_INT;       /* Activity interrupt */
    int_conf.param.gen_int.ref_update = BMA400_UPDATE_EVERY_TIME;     /* Manual reference update */
    int_conf.param.gen_int.hysteresis = BMA400_HYST_0_MG;            /* Hysteresis of 62.5mg */
    int_conf.param.gen_int.int_thres_ref_x = ACCEL_WAKE_REF_X;        /* Reference value for X axis */
    int_conf.param.gen_int.int_thres_ref_y = ACCEL_WAKE_REF_Y;        /* Reference value for Y axis */
    int_conf.param.gen_int.int_thres_ref_z = ACCEL_WAKE_REF_Z;        /* Reference value for Z axis */
    int_conf.param.gen_int.int_chan = BMA400_INT_CHANNEL_1;           /* Use INT1 pin */
    
    
    /* Configure the generic interrupt */
    rslt = (bma400_set_sensor_conf(&int_conf, 1, &bma1) == BMA400_OK) ? BMA400_OK : bma400_set_sensor_conf(&int_conf, 1, &bma2);
    if (rslt != BMA400_OK) {
        return rslt;
    }
    
    /* Enable the generic interrupt */
    int_en.type = BMA400_GEN1_INT_EN;
    int_en.conf = BMA400_ENABLE;
    // rslt = (bma400_enable_interrupt(&int_en, 1, &bma1) == BMA400_OK) ? BMA400_OK : bma400_enable_interrupt(&int_en, 1, &bma2);
    if (bma400_enable_interrupt(&int_en, 1, &bma1) == BMA400_OK){
        printf("BMA address used: 15\r\n");
        rslt = BMA400_OK;
    }
    else{
        rslt = -1;
    }

    if (bma400_enable_interrupt(&int_en, 1, &bma2) == BMA400_OK){
        printf("BMA address used: 14\r\n");
        rslt = BMA400_OK;
    }
    else{
        rslt = -1;
    }


    /* Set BMA400 to low power mode */
    // rslt = bma400_set_power_mode(BMA400_MODE_LOW_POWER, &bma);

    return rslt;
}

int8_t BMA400_ActivityAutoWakeupAutoLP(uint8_t wkup_threshold)
{
    int8_t rslt;
    struct bma400_device_conf dev_conf[2];
    struct bma400_int_enable int_en;

    dev_conf[0].type = BMA400_AUTOWAKEUP_INT;
    dev_conf[1].type = BMA400_AUTO_LOW_POWER;

    rslt = bma400_get_device_conf(dev_conf, 2, &bma);
    if (rslt != BMA400_OK) {
        return rslt;
    }

    dev_conf[0].param.wakeup.wakeup_axes_en = BMA400_AXIS_XYZ_EN;
    dev_conf[0].param.wakeup.wakeup_ref_update = BMA400_UPDATE_EVERY_TIME;
    dev_conf[0].param.wakeup.sample_count = BMA400_SAMPLE_COUNT_4;
    dev_conf[0].param.wakeup.int_wkup_threshold = 3;
    dev_conf[0].param.wakeup.int_chan = BMA400_INT_CHANNEL_1;

    dev_conf[1].param.auto_lp.auto_low_power_trigger = BMA400_AUTO_LP_GEN1_TRIGGER;

    rslt = bma400_set_device_conf(dev_conf, 2, &bma);
    if (rslt != BMA400_OK) {
        return rslt;
    }

    int_en.type = BMA400_AUTO_WAKEUP_EN;
    int_en.conf = BMA400_ENABLE;

    return bma400_enable_interrupt(&int_en, 1, &bma);
}

int8_t BMA400_GenericInterruptsByActivity(void)
{
    int8_t rslt;
    struct bma400_sensor_conf accel_setting[2];
    struct bma400_int_enable int_en[2];

    accel_setting[0].type = BMA400_GEN1_INT;
    accel_setting[1].type = BMA400_GEN2_INT;

    rslt = bma400_get_sensor_conf(accel_setting, 2, &bma);
    if (rslt != BMA400_OK) {
        return rslt;
    }

    accel_setting[0].param.gen_int.int_chan = BMA400_INT_CHANNEL_2;
    accel_setting[0].param.gen_int.axes_sel = BMA400_AXIS_XYZ_EN;
    accel_setting[0].param.gen_int.criterion_sel = BMA400_INACTIVITY_INT;
    accel_setting[0].param.gen_int.evaluate_axes = BMA400_ANY_AXES_INT;
    accel_setting[0].param.gen_int.ref_update = BMA400_UPDATE_EVERY_TIME;
    accel_setting[0].param.gen_int.data_src = BMA400_DATA_SRC_ACC_FILT2;
    accel_setting[0].param.gen_int.gen_int_thres = 0x02;
    accel_setting[0].param.gen_int.gen_int_dur = 100;
    accel_setting[0].param.gen_int.hysteresis = BMA400_HYST_0_MG;

    accel_setting[1].param.gen_int.int_chan = BMA400_INT_CHANNEL_2;
    accel_setting[1].param.gen_int.axes_sel = BMA400_AXIS_XYZ_EN;
    accel_setting[1].param.gen_int.criterion_sel = BMA400_INACTIVITY_INT;
    accel_setting[1].param.gen_int.evaluate_axes = BMA400_ANY_AXES_INT;
    accel_setting[1].param.gen_int.ref_update = BMA400_UPDATE_ONE_TIME;
    accel_setting[1].param.gen_int.data_src = BMA400_DATA_SRC_ACC_FILT1;
    accel_setting[1].param.gen_int.gen_int_thres = 0x10;
    accel_setting[1].param.gen_int.gen_int_dur = 0x01;
    accel_setting[1].param.gen_int.hysteresis = BMA400_HYST_0_MG;

    rslt = bma400_set_sensor_conf(accel_setting, 2, &bma);
    if (rslt != BMA400_OK) {
        return rslt;
    }

    int_en[0].type = BMA400_GEN1_INT_EN;
    int_en[0].conf = BMA400_ENABLE;

    int_en[1].type = BMA400_GEN2_INT_EN;
    int_en[1].conf = BMA400_DISABLE;

    return bma400_enable_interrupt(int_en, 2, &bma);
}

int8_t BMA400_ReadFifoDataAndTime(struct bma400_fifo_sensor_data *accel_data, uint16_t *frame_count, float *sensor_time_s) {
    /* sensor_time rollsover after 654 seconds*/

    if (accel_data == NULL || frame_count == NULL || sensor_time_s == NULL) {
        printf("Error: pointer is NULL\r\n");
        return BMA400_E_NULL_PTR;
    }

    int8_t rslt;
    struct bma400_fifo_data fifo_frame;
    uint8_t fifo_buff[FIFO_SIZE_FULL] = { 0 };

    fifo_frame.data = fifo_buff;
    fifo_frame.length = FIFO_SIZE_FULL;

    rslt = bma400_get_fifo_data(&fifo_frame, &bma);
    if (rslt != BMA400_OK) {
        printf("Error in bma400_get_fifo_data\r\n");
        return rslt;
    }

    uint16_t frames = fifo_frame.length / 7;     
    
    // frame count (1 byte for header, 6 bytes per accel frame)
    *frame_count = frames;
    rslt = bma400_extract_accel(&fifo_frame, accel_data, frame_count, &bma);
    if (rslt != BMA400_OK) {
        printf("Error in bma400_extract_accel\r\n");
        return rslt;
    }

    *sensor_time_s = (float)(fifo_frame.fifo_sensor_time) * SENSOR_TICK_TO_S;
    return rslt;
}

uint16_t BMA400_GetMaxAccel(struct bma400_fifo_sensor_data *accel_data, uint16_t frame_count){
    if (accel_data == NULL || frame_count == 0) {
        printf("Error: pointer is NULL or frame_count is zero\r\n");
        return 0;           // Better way to handle this?
    }    
    uint16_t max_accel = 0;
    for (int i = 0; i < frame_count; i++) {
        if (accel_data[i].x == 0 && accel_data[i].y == 0 && accel_data[i].z == 0) {
            continue;
        } else if (abs(accel_data[i].x) > 10000 || abs(accel_data[i].y) > 10000 || abs(accel_data[i].z) > 10000) {
            printf(" -- Invalid data, skipping\r\n");
            continue;
        }

        float accel_magnitude = sqrtf(
            ((float)accel_data[i].x * (float)accel_data[i].x) +
            ((float)accel_data[i].y * (float)accel_data[i].y) +
            ((float)accel_data[i].z * (float)accel_data[i].z));
        if ((uint16_t)accel_magnitude > max_accel) {
            max_accel = accel_magnitude;
        }
    }
    max_accel = (uint16_t)(max_accel * 1000.0f / scale_factor);
    return max_accel;
}

int8_t BMA400_GetTemperature(int16_t *temperature){
    if (temperature == NULL) {
        printf("Error: temperature pointer is NULL\r\n");
        return BMA400_E_NULL_PTR;
    }
    int8_t rslt;
    rslt = (bma400_get_temperature_data(temperature, &bma1) == BMA400_OK) ? BMA400_OK : bma400_get_temperature_data(temperature, &bma2);
    if (rslt != BMA400_OK) {
        printf("Error in bma400_get_temperature_data\r\n");
        return rslt;
    }
    return rslt;
}

static BMA400_INTF_RET_TYPE bma400_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr){
    if (len == 0) {
        return BMA400_OK;
    }
    if (reg_data == NULL) {
        printf("Error: reg_data pointer is NULL\r\n");
        return BMA400_E_NULL_PTR;
    }
    uint8_t dev_addr = *(uint8_t*)intf_ptr;

    HAL_StatusTypeDef status;
    
    status = HAL_I2C_Master_Transmit(bma_i2c, dev_addr, &reg_addr, 1, HAL_MAX_DELAY);
    if (status != HAL_OK)
        return BMA400_E_COM_FAIL;
        
    status = HAL_I2C_Master_Receive(bma_i2c, dev_addr, reg_data, len, HAL_MAX_DELAY);
    if (status != HAL_OK)
        return BMA400_E_COM_FAIL;
        
    return BMA400_OK;
}


static BMA400_INTF_RET_TYPE bma400_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr){
    if (reg_data == NULL || len == 0) {
        printf("Error: reg_data pointer is NULL or length is zero\r\n");
        return BMA400_E_NULL_PTR;
    }
    if (len > BMA400_MAX_BUFFER_WRITE_SIZE) {
        printf("Error: length exceeds maximum buffer size\r\n");
        return BMA400_E_INVALID_CONFIG;
    }
    if (intf_ptr == NULL) {
        printf("Error: intf_ptr pointer is NULL\r\n");
        return BMA400_E_NULL_PTR;
    }
    uint8_t dev_addr = *(uint8_t*)intf_ptr;
    HAL_StatusTypeDef status;

    uint8_t buf[BMA400_MAX_BUFFER_WRITE_SIZE];

    buf[0] = reg_addr;
    memcpy(buf + 1, reg_data, len);

    status = HAL_I2C_Master_Transmit(bma_i2c, dev_addr, buf, len + 1, HAL_MAX_DELAY);

    if (status != HAL_OK)
        return BMA400_E_COM_FAIL;

    return BMA400_OK;
}


static void bma400_delay_us(uint32_t period, void *intf_ptr)
{
    if (intf_ptr == NULL) {
        printf("Error: intf_ptr pointer is NULL\r\n");
        return;
    }
    (void)intf_ptr;
    
    // Correctly handle microsecond delays
    // For delays >= 1000µs, use HAL_Delay (milliseconds)
    // For delays < 1000µs, use busy-wait loop
    uint32_t ms = period / 1000;
    uint32_t us_remainder = period % 1000;
    
    if (ms > 0) {
        HAL_Delay(ms);
    }
    
    // Busy-wait for remaining microseconds (crude but functional)
    // At 4MHz sysclock, each iteration ~= few cycles
    if (us_remainder > 0) {
        volatile uint32_t count = us_remainder * (SystemCoreClock / 1000000) / 10;
        while (count--) {
            __NOP();
        }
    }
}


static float get_scale_factor_from_range(uint8_t range)
{
    switch (range) {
        case BMA400_RANGE_2G:
                return 1024.0f;
        case BMA400_RANGE_4G:
                return 512.0f;
        case BMA400_RANGE_8G:
                return 256.0f;
        case BMA400_RANGE_16G:
                return 128.0f;
        default:
                return -1.0f; // Invalid range
    }
}
