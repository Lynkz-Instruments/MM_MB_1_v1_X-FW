/**
 * @file nrf5_utils.c
 * @author Etienne Machabee (etienne@lynkz.ca), Alexandre Desgagné(alexd@lynkz.ca)
 * @brief Utilities specific for nrf5 devices
 * @version 0.1
 * @date 2022-01-20
 * 
 * @copyright Copyright (c) Lynkz Instruments Inc 2022
 * 
 */

#include "nrf5_utils.h"

#include <stdbool.h>

#include "nrf52.h"
#include "nrf52_bitfields.h"
#include "nrf_assert.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#define UICR_ADDRESS (0x10001000)
#define UICR_LENGTH  (0x210)
#define UICR_CUSTOM_OFFSET (0x080)
#define UICR_NBR_CUSTOM (32)
#define UICR_NFC_OFFSET (0x20C)

// UICR utils Buffer
uint8_t buffer[UICR_LENGTH] __attribute__ ((aligned (4)));

// PRIVATE FUNCTIONS PROTOTYPES //

/**
 * @brief      Decode the reset reason register
 *
 * @param[in]  reason  Value of the reset reason
 *
 * @return     Reset reason as per enum
 */
static NRF5_UTILS_resetReason_t decodeResetReason(uint32_t reason);

// PUBLIC FUNTIONS

NRF5_UTILS_resetReason_t NRF5_UTILS_GetResetReasons(bool softdevice)
{
  NRF5_UTILS_resetReason_t resetreason;

  uint32_t reset_reason = NRF_POWER->RESETREAS;
  
  if (softdevice == true) {
  // Clear the last reset reason
  sd_power_reset_reason_clr(0xffffffff);
  } else {
  // Clear all bits in the RESETREAS register
  NRF_POWER->RESETREAS = 0xFFFFFFFF; // This clears all the bits in the register
  }
  resetreason = decodeResetReason(reset_reason);

  NRF_LOG_INFO("REASON OF SYSTEM RESET: %d", resetreason);
  NRF_LOG_PROCESS();

  return resetreason;
}

void NRF5_UTILS_SetApProtect(bool enable)
{
  #ifndef UICR_APPROTECT_PALL_HwDisabled
  #define UICR_APPROTECT_PALL_HwDisabled (UICR_APPROTECT_PALL_Disabled)
  #endif
  if (enable) {
    if ((NRF_UICR->APPROTECT & UICR_APPROTECT_PALL_Msk) !=
      (UICR_APPROTECT_PALL_Enabled << UICR_APPROTECT_PALL_Pos)) {

          NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
          while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}

          NRF_UICR->APPROTECT = ((NRF_UICR->APPROTECT & ~((uint32_t)UICR_APPROTECT_PALL_Msk)) |
          (UICR_APPROTECT_PALL_Enabled << UICR_APPROTECT_PALL_Pos));

          NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
          while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}
          NVIC_SystemReset();
      }
  } else {
    if ((NRF_UICR->APPROTECT & UICR_APPROTECT_PALL_Msk) 
       == (UICR_APPROTECT_PALL_Msk))
    {
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
        NRF_UICR->APPROTECT = (UICR_APPROTECT_PALL_HwDisabled << UICR_APPROTECT_PALL_Pos);
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
        NVIC_SystemReset();
    }  
  }
}

void nvmc_erase_mode(void)
{
  NRF_NVMC->CONFIG = (int32_t)NVMC_CONFIG_WEN_Een;
}

void nvmc_read_only_mode(void)
{
  NRF_NVMC->CONFIG = (int32_t)NVMC_CONFIG_WEN_Ren;
}

void nvmc_write_mode(void)
{
  NRF_NVMC->CONFIG = (int32_t)NVMC_CONFIG_WEN_Wen;
}

void wait_for_nvmc_ready(void)
{
  while( !(bool)(NRF_NVMC->READY & NVMC_READY_READY_Msk) );
}

void nvmc_erase_uicr(void)
{
  nvmc_erase_mode();
  NRF_NVMC->ERASEUICR = 1;
  wait_for_nvmc_ready();
  nvmc_read_only_mode();
}

void nvmc_write_words(uint32_t* address, uint32_t* words, uint32_t size)
{
  nvmc_write_mode();
  for (uint32_t i = 0; i < size; i++){
    wait_for_nvmc_ready();
    *(volatile uint32_t *)(address + i) = ((uint32_t const *)words)[i];
    __DMB();
  }
  nvmc_read_only_mode();
}

void nvmc_write_word(uint32_t* const address, const uint32_t word)
{
  nvmc_write_mode();

  wait_for_nvmc_ready();
  *(volatile uint32_t *)(address) = word;
  __DMB();

  nvmc_read_only_mode();
}

void app_uicr_set(uint8_t id, const uint32_t value)
{
  ASSERT(id < UICR_NBR_CUSTOM);

  uint8_t* uicr_register = (uint8_t*)UICR_ADDRESS;
  const uint32_t offset = UICR_CUSTOM_OFFSET + 4* id;

  const uint32_t actual_value = *(uint32_t*)(uicr_register + offset);

  if( actual_value == value){
    // Nothing to do, the register already have the good value
    return;
  }

  if((value & actual_value) == value){
    // We can write the value in register without erasing
    nvmc_write_word( (uint32_t*)(uicr_register + offset) ,value);
  }
  else{
    // We copy all the UICR
    memcpy(buffer, uicr_register, UICR_LENGTH);

    // We change the value of the register in the buffer
    *(uint32_t*)(buffer + offset) = value;

    // We Erase all the UICR
    nvmc_erase_uicr();

    // we rewrite all the UICR with the buffer
    nvmc_write_words((uint32_t*)UICR_ADDRESS, (uint32_t*)buffer, UICR_LENGTH/4);
  }
}

uint32_t app_uicr_get(uint8_t id)
{
  ASSERT(id < UICR_NBR_CUSTOM);

  uint8_t* uicr_register = (uint8_t*)UICR_ADDRESS;
  const uint32_t offset = UICR_CUSTOM_OFFSET + 4* id;

  return *(uint32_t*)(uicr_register + offset);
}

void app_uicr_set_gpio_mode(void)
{ 
  uint8_t* uicr_register = (uint8_t*)UICR_ADDRESS;
  const uint32_t offset = UICR_NFC_OFFSET;

  if (NRF_UICR->NFCPINS == 0xFFFFFFFE){
    // No need to change the value. return.
    return; 
  }
  else if (NRF_UICR->NFCPINS == 0xFFFFFFFF){
    // Can be written without erasing the UICR.
    nvmc_write_mode();
    wait_for_nvmc_ready();
    NRF_UICR->NFCPINS = 0xFFFFFFFE;
    nvmc_read_only_mode();
  }
  else{ 
    // This should not get there in anyway but just to be sure.
    // We copy all the UICR
    memcpy(buffer, uicr_register, UICR_LENGTH);

    // We change the value of the register in the buffer
    *(uint32_t*)(buffer + offset) = 0xFFFFFFFE;

    // We Erase all the UICR
    nvmc_erase_uicr();

    // we rewrite all the UICR with the buffer
    nvmc_write_words((uint32_t*)UICR_ADDRESS, (uint32_t*)buffer, UICR_LENGTH/4);
  }
  NRF_LOG_INFO("NFC pins set for GPIO");
  NRF_LOG_PROCESS();
  NVIC_SystemReset(); // Reseting for the changes to take effect.
}

void app_uicr_set_nfc_mode(void)
{
  uint8_t* uicr_register = (uint8_t*)UICR_ADDRESS;
  const uint32_t offset = UICR_NFC_OFFSET;

  if (NRF_UICR->NFCPINS == 0xFFFFFFFF){
    // No need to change the value. return.
    return; 
  }
  else{ 
    // This should not get there in anyway but just to be sure.
    // We copy all the UICR
    memcpy(buffer, uicr_register, UICR_LENGTH);

    // We change the value of the register in the buffer
    *(uint32_t*)(buffer + offset) = 0xFFFFFFFF;

    // We Erase all the UICR
    nvmc_erase_uicr();

    // we rewrite all the UICR with the buffer
    nvmc_write_words((uint32_t*)UICR_ADDRESS, (uint32_t*)buffer, UICR_LENGTH/4);
  }
  NRF_LOG_INFO("NFC pins set for NFC");
  NRF_LOG_PROCESS();
  NVIC_SystemReset(); // Reseting for the changes to take effect.
}

// PRIVATE FUNCTIONS //

static NRF5_UTILS_resetReason_t decodeResetReason(uint32_t reason)
{
  if (reason & 0x01u){ return NRF5_UTILS_RESETREASON_RESETPIN; }
  if ((reason >> 1u) & 0x01u){ return NRF5_UTILS_RESETREASON_DOG; }
  if ((reason >> 2u) & 0x01u){ return NRF5_UTILS_RESETREASON_SREQ; }
  if ((reason >> 3u) & 0x01u){ return NRF5_UTILS_RESETREASON_LOCKUP; }
  if ((reason >> 16u) & 0x01u){ return NRF5_UTILS_RESETREASON_OFF; }
  if ((reason >> 17u) & 0x01u){ return NRF5_UTILS_RESETREASON_LPCOMP; }
  if ((reason >> 18u) & 0x01u){ return NRF5_UTILS_RESETREASON_DIF; }
  if ((reason >> 19u) & 0x01u){ return NRF5_UTILS_RESETREASON_NFC; }

  // Return ONCHIP if no reset reason is decoded
  return NRF5_UTILS_RESETREASON_ONCHIP;
}
