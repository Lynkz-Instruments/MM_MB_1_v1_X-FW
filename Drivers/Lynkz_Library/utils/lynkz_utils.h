/**
 * @file lynkz_utils.h
 * @author Alexandre Desgagné (alexd@lynkz.ca)
 * @author Emile Laplante (emile@lynkz.ca)
 * @brief 
 * @version 0.1
 * @date 2022-01-20
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef LYNKZ_UTILS_H
#define LYNKZ_UTILS_H

#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "math.h"

#include "nrf52.h"
#include "nrf52_bitfields.h"

#include "nrf_assert.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

//get a bit from a variable
#define GETBIT(var, bit)   (((var) >> (bit)) & 1)

//set a bit to 1
#define SETBIT(var, bit)   var |= (1 << (bit))

//set a bit to 0
#define CLRBIT(var, bit)   var &= (~(1 << (bit)))

/**
 * @brief Function for converting a byte array into a ASCII hex string
 *
 * @param inBytes: Array to convert
 * @param numBytes: Byte array size
 * @param strBuffer: Converted string destination
 * @param outStringSize: Max size of destination string
 */
void bytesToHexString(unsigned char* inBytes, int numBytes, char* strBuffer, int outStringSize);

/**
 * @brief Function to perform a cyclic redundancy check. This is specific for the neblina service usage.
 * 
 * @param bytes Data
 * @param size Data length
 * @param polynomial 
 * @param initialization Initialization byte for the crc algorithm
 * @return uint8_t -> CRC
 */
uint8_t crc8_encode(const uint8_t* const bytes, uint8_t size, uint8_t polynomial, uint8_t initialization);


/**
 * @brief Function to find min, max and standard deviation in a buffer 
 * 
 * @param buf       buffer to find min max and standard deviation
 * @param res       return min max and standard deviation in an array by reference
 * @param buf_size  size of the buffer to find min, max and standard deviation
 */
void findMMM(int16_t * buf, int16_t * res, uint8_t buf_size);

/**
 * @brief Function to find min max, mean and standard deviation in a buffer 
 * 
 * @param buf       buffer to find min max, mean and standard deviation
 * @param res       return min max, mean and standard deviation in an array by reference
 * @param buf_size  size of the buffer to find min max, mean and standard deviation
 */
void findMMMSTD(int16_t * buf, int16_t * res, uint16_t buf_size);
void findMMMSTD_signed(const int16_t * const buf, int16_t * res, const uint16_t buf_size);

/**
 * @brief Convert hex value tu string
 * 
 * @param data   data to convert
 * @param str    string into data will be converted
 * @param size   size of the string
 */
void hex2str(uint8_t* data, char* str, uint16_t size);

/**
 * @brief take a hex string and convert it to a 32bit number (max 8 hex digits)
 * 
 * @param hex         hex string
 * @return uint32_t   int value
 */
uint32_t hex2int(char *hex);

/**
 * @brief take a 32 bits int and spits a 6 char long str
 * 
 * @param addr 32 bits number
 * @param hex  string of 6 char
 */
void int2hex(uint32_t addr, char * hex);

/**
 * @brief Function to check if an array is empty (only 0)
 * 
 * @param array     array to check
 * @param len       length of the array
 * @return true     array is empty
 * @return false    array is not empty
 */
bool array_empty_check(uint8_t * array, int len);

/**
 * @brief calculate crc16 on an array of bytes
 * 
 * @param buff        buffer to calculate crc16
 * @param size        size of the buffer
 * @return uint16_t   return crc16
 */
uint16_t crc16(const uint8_t* buff, size_t size);

/**
 * @brief give a pointer to the first occurence of subarray in array, if there are no occurence, return NULL
 * 
 * @param subarray        subarray to find
 * @param subarray_size   size of the Subarray
 * @param array           where the subarray is search
 * @param array_size      size of the array
 * @return uint8_t*, First occurence pointer
 */
uint8_t* find_subarray_in_array(uint8_t* subarray, const uint16_t subarray_size, uint8_t* array, const uint16_t array_size);

#define logInfo(...)                      NRF_LOG_INFO( __VA_ARGS__); NRF_LOG_FLUSH();

void get_versions(char * ver, uint8_t * major, uint8_t * minor, uint8_t * patch);

void remove_all_chars(char* str, char c);

void add_char_between_n_chars(char str[], char out_str[], char ch, int n);

#endif // LYNKZ_UTILS_H