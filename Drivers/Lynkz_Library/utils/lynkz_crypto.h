/**
 * @file lynkz_crypto.h
 * @author Alexandre Desgagné (alexd@lynkz.ca)
 * @brief 
 * @version 0.1
 * @date 2023-03-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef LYNKZ_KDF_H
#define LYNKZ_KDF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "sdk_errors.h"

ret_code_t generate_lora_keys(char * deveui, char * devaddr,  char * appskey, char * netwskey);

#endif // LYNKZ_KDF_H