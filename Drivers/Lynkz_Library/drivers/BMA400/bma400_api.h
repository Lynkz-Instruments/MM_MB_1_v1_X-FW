/**
    * @file    bma400_api.h
    * @brief   API for BMA400 accelerometer sensor
    * 
    *
    *  high-level functions to interface with the BMA400 accelerometer sensor.
    *
    *  Created on: Mar 20 , 2025
    *  Author: Charles Marseille
    *  Copyright Lynkz Instruments Inc, Amos 2025
 */


#ifndef BMA400_API_H_
#define BMA400_API_H_


#define BMA400_SENSORTIME_0  0x0A
#define BMA400_SENSORTIME_1  0x0B
#define BMA400_SENSORTIME_2  0x0C

#define BMA400_MAX_BUFFER_WRITE_SIZE  32

#define SENSORTIME_RES_SECONDS  (1/26552)                                   // Each tick is 312.5 us

// #define ACCEL_WAKE_THRESHOLD    UINT8_C(20)                                 //  (1 LSB = 8mg)
#define ACCEL_WAKE_THRESHOLD    0x03                                 //  (1 LSB = 8mg)
#define ACCEL_WAKE_REF          UINT8_C(0)
#define ACCEL_WAKE_REF_X        ACCEL_WAKE_REF
#define ACCEL_WAKE_REF_Y        ACCEL_WAKE_REF
#define ACCEL_WAKE_REF_Z        ACCEL_WAKE_REF

#define SCALE_FACTOR_2G         (1024.0f)
#define SCALE_FACTOR_4G         (512.0f)
#define SCALE_FACTOR_8G         (256.0f)
#define SCALE_FACTOR_16G        (128.0f)
#define SCALE_FACTOR            SCALE_FACTOR_2G

#define SENSOR_TICK_TO_S        (0.0000390625f)
#define FIFO_SIZE               UINT16_C(1024)
#define FIFO_SIZE_FULL          (FIFO_SIZE + BMA400_FIFO_BYTES_OVERREAD)
#define FIFO_ACCEL_FRAME_COUNT  UINT8_C(200)


#include "bma400.h"
//specify the target HAL i2c header file for your board
#include "stm32wlxx_hal.h"
#include "stm32wlxx_hal_i2c.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


typedef struct {
    uint16_t frame_count;
    uint16_t max_accel;
    int16_t temperature;
    float sensor_time_s;
    float last_sensor_time_s;
    struct bma400_fifo_sensor_data accel_data;
    struct bma400_fifo_sensor_data accel_data_temp;
} BMA400_SensorContext;

/**
* @brief  Initialize the BMA400 accelerometer
* @param  hi2c Pointer to I2C handle
* @retval BMA400 status code (0 for success)
*/
int8_t BMA400_Init(I2C_HandleTypeDef *hi2c, uint16_t * int_status);

/**
* @brief  Get accelerometer data in g units
* @param  accel_g Pointer to structure that will receive acceleration values in g
* @param  print_values Flag to print values to console (1 to print, 0 to not print)
* @retval BMA400 status code (0 for success)
*/
typedef struct {
    float x;
    float y;
    float z;
} BMA400_AccelData_g;

int8_t BMA400_GetAcceleration_g(BMA400_AccelData_g *accel_g, uint8_t print_values);

/**
 * @brief Set to low power
 * @param None
 * @retval None
 */
int8_t BMA400_SetLowPowerMode();

/**
* @brief  Get raw accelerometer data
* @param  accel_raw Pointer to structure that will receive raw acceleration values
* @retval BMA400 status code (0 for success)
*/
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} BMA400_AccelData_raw;

int8_t BMA400_GetRawAcceleration(BMA400_AccelData_raw *accel_raw);

/**
* @brief  Get sensor range setting
* @retval Current range setting (2, 4, 8, or 16 for ±2g, ±4g, ±8g, or ±16g)
*/
uint8_t BMA400_GetRange(void);

/**
* @brief  Set sensor range
* @param  range Range value (BMA400_RANGE_2G, BMA400_RANGE_4G, etc.)
* @retval BMA400 status code (0 for success)
*/
int8_t BMA400_SetRange(uint8_t range);

/**
* @brief  Get sensor chip ID
* @param  chip_id Pointer to variable that will receive the chip ID
* @retval BMA400 status code (0 for success)
*/
int8_t BMA400_GetChipID(uint8_t *chip_id);

/**
* @brief  configure the threshold interrupt
* @retval BMA400 status code (0 for success)
*/
int8_t BMA400_configureLPThresholdInterrupt(uint8_t accel_thresh);

/**
 * @brief Configure auto wakeup and auto low power
 * @retval BMA400 status code (0 for success)
 */
int8_t BMA400_ActivityAutoWakeupAutoLP(uint8_t wkup_threshold);

/**
 * @brief Configure generic interrupts for activity/inactivity
 * @retval BMA400 status code (0 for success)
 */
int8_t BMA400_GenericInterruptsByActivity(void);

/**
 * @brief get sensor time in milliseconds
 * @param sensor_time Pointer to variable that will receive the sensor time
 * @retval BMA400 status code (0 for success)
 */
int8_t BMA400_get_sensor_time(struct bma400_dev *dev, uint32_t *sensor_time);

uint32_t BMA400_ReadSensorTime(void);

float BMA400_GetSensorTimeSeconds(void);

int8_t BMA400_ReadFifoDataAndTime(struct bma400_fifo_sensor_data *accel_data, uint16_t *frame_count, float *sensor_time_s);

int8_t BMA400_GetTemperature(int16_t *temperature);

uint16_t BMA400_GetMaxAccel(struct bma400_fifo_sensor_data *accel_data, uint16_t frame_count);

#endif /* BMA400_API_H_ */