#ifndef __LYNKZ_KEY_GEN_H__
#define __LYNKZ_KEY_GEN_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Lynkz ABP key derivation
 *
 * DevAddr  = LL_FLASH_GetUDN() (last 32 bits of the STM32WL unique device number)
 * prefix   = PREFIXES[devaddr & 0x03]   — deterministic, device-specific
 * DevEUI   = prefix (4 bytes) || DevAddr (4 bytes, big-endian)
 * AppSKey  = HKDF-SHA256(IKM=DevEUI, salt=0x00, length=16)  [RFC 5869]
 * NwkSKey  = HKDF-SHA256(IKM=DevEUI, salt=0x01, length=16)
 *
 * The server confirms the prefix by trying all four and verifying the MIC.
 */

/*
 * Derive DevEUI, AppSKey and NwkSKey from the MCU unique device number,
 * then push them into the LoRaWAN stack via LmHandlerSetDevEUI /
 * LmHandlerSetKey.  Call once, after LmHandlerConfigure().
 */
void LynkzKeyGen_Apply(void);

#ifdef __cplusplus
}
#endif

#endif /* __LYNKZ_KEY_GEN_H__ */
