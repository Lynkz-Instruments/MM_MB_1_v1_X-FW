/**
 * @file nrf5_utils.h
 * @author Etienne Machabee (etienne@lynkz.ca), Alexandre Desgagné(alexd@lynkz.ca)
 * @brief Utilities specific for nrf5 devices
 * @version 0.1
 * @date 2022-01-20
 * 
 * @copyright Copyright (c) Lynkz Instruments Inc 2022
 * 
 */

#ifndef NRF5_LYNKZ_UTILS_H
#define NRF5_LYNKZ_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// nRF52 reset reasons from the NRF_POWER->RESETREAS register
typedef enum{
  NRF5_UTILS_RESETREASON_ONCHIP = 0,       // Reset from the on-chip reset generator
  NRF5_UTILS_RESETREASON_RESETPIN = 1,     // Reset from reset pin
  NRF5_UTILS_RESETREASON_DOG = 2,          // Reset from watchdog
  NRF5_UTILS_RESETREASON_SREQ = 3,         // Reset from soft reset
  NRF5_UTILS_RESETREASON_LOCKUP = 4,       // Reset from CPU lock-up
  NRF5_UTILS_RESETREASON_OFF = 5,          // Reset from System OFF by DETECT signal from GPIO
  NRF5_UTILS_RESETREASON_LPCOMP = 6,       // Reset from System OFF by ANADETECT from LPCOMP
  NRF5_UTILS_RESETREASON_DIF = 7,          // Reset from System OFF by debug interface mode
  NRF5_UTILS_RESETREASON_NFC = 8,          // Reset from System OFF by NFC field detect
  NRF5_UTILS_RESETREASON_MAX = 9,
} NRF5_UTILS_resetReason_t;

/**
 * @brief      Obtain the reason of the last reset
 *
 * @param[in]  softdevice Set to true if the softdevice is initialized at the time of calling the function
 *
 * @return     Reset reason as per enum
 */
NRF5_UTILS_resetReason_t NRF5_UTILS_GetResetReasons(bool softdevice);

/**
 * @brief      Set the APPROTECT configuration
 *
 * @param[in]  enable  Enable
 */
void NRF5_UTILS_SetApProtect(bool enable);

/**
 * @brief Function to set the nvmc in erasing mode.
 */
void nvmc_erase_mode(void);

/**
 * @brief Function to set the nvmc in read only mode.
 */
void nvmc_read_only_mode(void);

/**
 * @brief Function to set the nvmc in write mode.
 */
void nvmc_write_mode(void);

/**
 * @brief Function to wait for the nvmc to be ready.
 */
void wait_for_nvmc_ready(void);

/**
 * @brief Function to set a customer register to a specific value.
 */
void app_uicr_set(uint8_t id, const uint32_t value);

/**
 * @brief Function to get a customer register value.
 */
uint32_t app_uicr_get(uint8_t id);

/**
 * @brief Function to set the NFCPINS register to GPIO mode.
 */
void app_uicr_set_gpio_mode(void);

/**
 * @brief Function to set the NFCPINS register to NFC mode.
 */
void app_uicr_set_nfc_mode(void);

#ifdef __cplusplus
}
#endif

#endif // NRF5_LYNKZ_UTILS_H
