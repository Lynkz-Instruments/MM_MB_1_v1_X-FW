#include "nPM2100.h"
#include "npm2100-bm/src/mfd_npm2100.h"

#include "stm32wlxx_hal.h"
#include <string.h>

// STM32 HAL I2C read helper
int nPM2100_I2C_Read(I2C_HandleTypeDef *hi2c, uint8_t dev_addr, uint8_t reg_addr, uint8_t *reg_data, uint16_t len) {
	if (reg_data == NULL || len == 0) {
		return -1;
	}
	HAL_StatusTypeDef status;
	/* Use finite timeouts to prevent indefinite blocking if I2C bus is stuck */
	status = HAL_I2C_Master_Transmit(hi2c, dev_addr, &reg_addr, 1, 500);
	if (status != HAL_OK)
		return -2;
	status = HAL_I2C_Master_Receive(hi2c, dev_addr, reg_data, len, 500);
	if (status != HAL_OK)
		return -3;
	return 0;
}

// STM32 HAL I2C write helper
int nPM2100_I2C_Write(I2C_HandleTypeDef *hi2c, uint8_t dev_addr, uint8_t reg_addr, const uint8_t *reg_data, uint16_t len) {
	if (reg_data == NULL || len == 0) {
		return -1;
	}
	uint8_t buf[32];
	if (len > sizeof(buf) - 1) return -2;
	buf[0] = reg_addr;
	memcpy(&buf[1], reg_data, len);
	/* Use finite timeout to prevent indefinite blocking */
	HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(hi2c, dev_addr, buf, len + 1, 500);
	if (status != HAL_OK)
		return -3;
	return 0;
}

int nPM2100_Init(struct i2c_dev *dev) {
	uint8_t val;
	int ret = nPM2100_GetRegister(dev, 0x00, &val); // Try reading reg 0
	return ret;
}

int nPM2100_SetRegister(struct i2c_dev *dev, uint8_t reg, uint8_t value) {
	return i2c_reg_write_byte(dev, reg, value);
}

int nPM2100_GetRegister(struct i2c_dev *dev, uint8_t reg, uint8_t *value) {
	return i2c_reg_read_byte(dev, reg, value);
}

int nPM2100_SetTimer(struct i2c_dev *dev, uint32_t time_ms) {
	return mfd_npm2100_set_timer(dev, time_ms, NPM2100_TIMER_MODE_GENERAL_PURPOSE);
}

int nPM2100_StartTimer(struct i2c_dev *dev) {
	return mfd_npm2100_start_timer(dev);
}

int nPM2100_StopTimer(struct i2c_dev *dev) {
	return mfd_npm2100_stop_timer(dev);
}

int nPM2100_Reset(struct i2c_dev *dev) {
	return mfd_npm2100_reset(dev);
}

int nPM2100_Hibernate(struct i2c_dev *dev, uint32_t time_ms, bool pass_through) {
	return mfd_npm2100_hibernate(dev, time_ms, pass_through);
}

int nPM2100_EnableEvents(struct i2c_dev *dev, uint32_t events) {
	return mfd_npm2100_enable_events(dev, events);
}

int nPM2100_DisableEvents(struct i2c_dev *dev, uint32_t events) {
	return mfd_npm2100_disable_events(dev, events);
}

int nPM2100_ProcessEvents(struct i2c_dev *dev, uint32_t *events) {
	return mfd_npm2100_process_events(dev, events);
}
