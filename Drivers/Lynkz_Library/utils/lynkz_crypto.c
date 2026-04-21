/**
 * @file lynkz_crypto.h
 * @author Alexandre Desgagné (alexd@lynkz.ca)
 * @brief 
 * @version 0.1
 * @date 2023-03-06
 * 
 * @copyright Copyright (c) 2022
 * For more informations: 
 * - https://infocenter.nordicsemi.com/index.jsp?topic=%2Fsdk_nrf5_v16.0.0%2Flib_crypto.html&cp=9_5_1_3_11_10&anchor=lib_crypto_intro_family_hkdf
 * - https://www.rfc-editor.org/rfc/rfc5869
 */

#include "lynkz_crypto.h"

#include "nrf_crypto.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "lynkz_utils.h"

#define DEVADDR_LENGTH_BYTES    4 // Bytes
#define KEY_LENGTH_BYTES       16 // Bytes
#define DEVEUI_LENGTH           8 // Bytes

ret_code_t generate_lora_keys(char * deveui, char * devaddr,  char * appskey, char * netwskey)
{
  ret_code_t ret;

  uint8_t deui[DEVEUI_LENGTH] = {0};
  uint8_t dev[DEVADDR_LENGTH_BYTES] = {0};
  uint8_t key1[KEY_LENGTH_BYTES] = {0};
  uint8_t key2[KEY_LENGTH_BYTES] = {0};
  uint8_t salt1[] = {0x00};
  uint8_t salt2[] = {0x01};
  uint8_t salt3[] = {0x02};
  size_t dev_len = sizeof(dev);
  size_t key1_len = sizeof(key1);
  size_t key2_len = sizeof(key2);

  nrf_crypto_hmac_context_t context;

  // Get deveui bytes
  for(int i = 0; i < DEVEUI_LENGTH; i++){
    sscanf(deveui + 2 * i, "%02X", &deui[i]);
  }

  // Initialize crypto library before using any crypto functionality.
  ret = nrf_crypto_init();

  // Dev address
  ret = nrf_crypto_hkdf_calculate(
    &context, // Optional context structure.
    &g_nrf_crypto_hmac_sha256_info, // Pointer to constant info structure.
    dev, // Pointer to output key material buffer.
    &dev_len, // Updated during the call.
    deui, // Pointer to input key material.
    DEVEUI_LENGTH, // Length of input key material.
    salt3, // Pointer to salt buffer.
    sizeof(salt3), // Length of salt buffer in bytes.
    NULL, // Pointer to additional info buffer.
    0, // Length of additional info buffer.
    NRF_CRYPTO_HKDF_EXTRACT_AND_EXPAND); // Use both extract and expand stages.
  
  // Key1
  ret = nrf_crypto_hkdf_calculate(
    &context, // Optional context structure.
    &g_nrf_crypto_hmac_sha256_info, // Pointer to constant info structure.
    key1, // Pointer to output key material buffer.
    &key1_len, // Updated during the call.
    deui, // Pointer to input key material.
    DEVEUI_LENGTH, // Length of input key material.
    salt1, // Pointer to salt buffer.
    sizeof(salt1), // Length of salt buffer in bytes.
    NULL, // Pointer to additional info buffer.
    0, // Length of additional info buffer.
    NRF_CRYPTO_HKDF_EXTRACT_AND_EXPAND); // Use both extract and expand stages.
  
  // Key2
  ret = nrf_crypto_hkdf_calculate(
    &context, // Optional context structure.
    &g_nrf_crypto_hmac_sha256_info, // Pointer to constant info structure.
    key2, // Pointer to output key material buffer.
    &key2_len, // Updated during the call.
    deui, // Pointer to input key material.
    DEVEUI_LENGTH, // Length of input key material.
    salt2, // Pointer to salt buffer.
    sizeof(salt2), // Length of salt buffer in bytes.
    NULL, // Pointer to additional info buffer.
    0, // Length of additional info buffer.
    NRF_CRYPTO_HKDF_EXTRACT_AND_EXPAND); // Use both extract and expand stages.

  ret = nrf_crypto_uninit();

  if (ret != NRF_SUCCESS)
  {
      // Handle error
      return ret;
  }
  
  // Convert bytes to strings
  char buffer[(2 * KEY_LENGTH_BYTES) + 1] = {0};
  bytesToHexString(key1, KEY_LENGTH_BYTES, buffer, (2 * KEY_LENGTH_BYTES) + 1);
  add_char_between_n_chars(buffer, appskey, ':', 2);

  bytesToHexString(key2, KEY_LENGTH_BYTES, buffer, (2 * KEY_LENGTH_BYTES) + 1);
  add_char_between_n_chars(buffer, netwskey, ':', 2);

  bytesToHexString(dev, KEY_LENGTH_BYTES, buffer, (2 * DEVADDR_LENGTH_BYTES) + 1);
  add_char_between_n_chars(buffer, devaddr, ':', 2);
  
  return 0;
}