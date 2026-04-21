#include "lynkz_utils.h"
#include "ble.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#define MAX_VERSION_DIGITS 3

void bytesToHexString(unsigned char* inBytes, int numBytes, char* strBuffer, int outStringSize){
    unsigned char * pin = inBytes;
    const char * hex = "0123456789ABCDEF";
    char * pout = strBuffer;
    for(; pin < inBytes+numBytes; pout +=2, pin++){
        pout[0] = hex[(*pin>>4) & 0xF];
        pout[1] = hex[ *pin     & 0xF];
        if (pout + 2 - strBuffer > outStringSize){
            /* Better to truncate output string than overflow buffer */
            /* it would be still better to either return a status */
            /* or ensure the target buffer is large enough and it never happen */
            break;
        }
    }
    pout[0] = 0;
}

uint8_t crc8_encode(const uint8_t* const bytes, uint8_t size, uint8_t polynomial, uint8_t initialization)
{
    uint16_t i, j;
    uint8_t crc = initialization;

    for ( i = 0; i < size; i++ ) {
        crc ^= bytes[i];
        for ( j = 0; j < 8; j++ ) {
            if ( crc & 0x80 )
                crc = (uint8_t)(( crc << 1 ) ^ polynomial );
            else
                crc <<= 1;
        }
    }
    return crc;
}

void findMMMSTD(int16_t * buf, int16_t * res, uint16_t buf_size){
    
    int16_t max = buf[0];
    int16_t min = buf[0];
    int16_t mean = 0;
    int16_t std_deviation;

    int32_t total = 0;

    for(int i=0; i<buf_size; i++){
        total += buf[i];
        if(buf[i] > max){
            max = buf[i];
        }
        if(buf[i] < min){
            min = buf[i];
        }
    }

    mean = total / buf_size;
    total = 0;

    // Remove Gravity effect (easy way...)
    for(int i=0; i<buf_size; i++){
        buf[i] = abs(buf[i] - mean);
        total += buf[i];
    }

    std_deviation = total / buf_size;

    res[0] = min;
    res[1] = max;
    res[2] = mean;
    res[3] = std_deviation;

}

void findMMMSTD_signed(const int16_t * const buf, int16_t * res, const uint16_t buf_size){
    
    int16_t max = buf[0];
    int16_t min = buf[0];

    int32_t total = 0;

    for(int i=0; i<buf_size; i++){
        total += buf[i];

        if(buf[i] > max){
            max = buf[i];
        }

        if(buf[i] < min){
            min = buf[i];
        }
    }

    const int16_t mean = total / buf_size;
    total = 0;

    // Remove Gravity effect (easy way...)
    for(int i=0; i<buf_size; i++){
        total += abs(buf[i] - mean);
    }

    const int16_t std_deviation = total / buf_size;

    res[0] = min;
    res[1] = max;
    res[2] = mean;
    res[3] = std_deviation;

}


void findMMM(int16_t * buf, int16_t * res, uint8_t buf_size){
    
    int16_t max = buf[0];
    int16_t min = buf[0];
    int16_t mean = 0;

    int32_t total = 0;

    for(int i=0; i<buf_size; i++){
        total += buf[i];
        if(buf[i] > max){
            max = buf[i];
        }
        if(buf[i] < min){
            min = buf[i];
        }
    }

    mean = total / buf_size;
    total = 0;

    // Remove Gravity effect (easy way...)
    for(int i=0; i<buf_size; i++){
        buf[i] = abs(buf[i] - mean);
        total += buf[i];
    }

    mean = total / buf_size;

    res[0] = min;
    res[1] = max;
    res[2] = mean;
}


void hex2str(uint8_t* data, char* str, uint16_t size){
    unsigned char * pin = data;
    const char * hex = "0123456789ABCDEF";
    char * pout = str;
    int i = 0;
    for(; i < size-1; ++i){
        *pout++ = hex[(*pin>>4)&0xF];
        *pout++ = hex[(*pin++)&0xF];
        *pout++ = ':';
    }
    *pout++ = hex[(*pin>>4)&0xF];
    *pout++ = hex[(*pin)&0xF];
    *pout = 0;
    return;
}

/**
 * hex2int
 * take a hex string and convert it to a 32bit number (max 8 hex digits)
 */
uint32_t hex2int(char *hex){
    uint32_t val = 0;
    while (*hex) {
        // get current character then increment
        uint8_t byte = *hex++; 
        // transform hex character to the 4bit equivalent number, using the ascii table indexes
        if (byte >= '0' && byte <= '9') byte = byte - '0';
        else if (byte >= 'a' && byte <='f') byte = byte - 'a' + 10;
        else if (byte >= 'A' && byte <='F') byte = byte - 'A' + 10;    
        // shift 4 to make space for new digit, and add the 4 bits of the new digit 
        val = (val << 4) | (byte & 0xF);
    }
    return val;
}

/**
 * int2hex
 * take a 32 bits int and spits a 6 char long str
 */
void int2hex(uint32_t addr, char * hex){

    uint8_t num[6] = {0};

    num[0] = (addr & 0x00F00000) >> 20;
    num[1] = (addr & 0x000F0000) >> 16;
    num[2] = (addr & 0x0000F000) >> 12;
    num[3] = (addr & 0x00000F00) >> 8;
    num[4] = (addr & 0x000000F0) >> 4;
    num[5] = (addr & 0x0000000F);

    for(int i=0;i<6;i++){
        if((num[i] >= 0) && (num[i] <= 9)){
            hex[i] = num[i] + 0x30;
        }
        else if((num[i] >= 10) && (num[i] <= 16)){
            hex[i] = num[i] + 0x37;
        }
    }
}

bool array_empty_check(uint8_t * array, int len)
{
  int64_t sum = 0;
  for (int i = 0; i < len; ++i)
  {
    sum = array[i] + sum;
  }
  if (sum == 0) 
  {
    return true;
  }
  else
  {
    return false;
  }
}

 uint16_t crc16(const uint8_t* buff, size_t size){
     uint8_t* data = (uint8_t*)buff;
     uint16_t result = 0xFFFF;

     for (size_t i = 0; i < size; ++i){
         result ^= data[i];
         for (size_t j = 0; j < 8; ++j){
             if (result & 0x01) result = (result >> 1) ^ 0xA001;
             else result >>= 1;
         }
     }
     return result;
 }

uint8_t* find_subarray_in_array(uint8_t* subarray, const uint16_t subarray_size, uint8_t* array, const uint16_t array_size)
{
  if(array_size < subarray_size) return NULL;
  if(subarray_size == 0) return array;

  uint8_t* subarray_cursor = subarray;
  
  for(uint8_t* array_cursor = array; array_cursor < array+array_size; array_cursor++)
  {
    if(*array_cursor == *subarray_cursor)
    {
      subarray_cursor++;
      if(subarray_cursor == subarray+subarray_size) return array_cursor - subarray_size + 1;
    }
    else
    {
      subarray_cursor = subarray;
    }
  }


  return NULL;
}

void get_versions(char * ver, uint8_t * major, uint8_t * minor, uint8_t * patch)
{
    char * ptr1;
    char * ptr2;
    char * ptrv;
    uint8_t size = 0;

    char version[MAX_VERSION_DIGITS + 1] = {0};
    
    // Check for V characters
    ptr1 = strstr(ver, "v");
    ptr2 = strstr(ver, "V");

    // Start after the V character if present
    if(ptr1){
      ptrv = ptr1 + 1;
    }
    else if(ptr2){
      ptrv = ptr2 + 1;
    }
    else{
      ptrv = ver;
    }
    
    // Check for the first period
    ptr1 = strstr(ptrv, ".");
    size = ptr1-ptrv;
    if(ptr1){
        if(size > MAX_VERSION_DIGITS){
            size = MAX_VERSION_DIGITS;
        }
        memcpy(version, ptrv, size); // Check how many characters there is in front of the period (3 max)
        *major = atoi(version);
        ptrv = ptr1 + 1; // Skip the period
        memset(version, 0, MAX_VERSION_DIGITS + 1);
    }
    else{ // If there is no period at all
        size = strlen(ptrv);
        if(size){
            if(size > MAX_VERSION_DIGITS){
                size = MAX_VERSION_DIGITS;
            }
            memcpy(version, ptrv, size); // Check how many characters there is in front of the period (3 max)
            if(strstr(version, "D")){ // Check if it is vDEV version
                *major = 0x63;
                *minor = 0x63;
                *patch = 0x63;
                return;
            }
            else{
                *major = atoi(version);
            }
        }
        else{ // Fill with 0xFF as no version was provided
            *major = 0xFF;
            *minor = 0xFF;
            *patch = 0xFF;
            return;
        }
        *minor = 0x00;
        *patch = 0x00;
        return;
    }
    
    // Check for the second period
    ptr1 = strstr(ptrv, ".");
    if(ptr1){
        size = ptr1-ptrv;
        if(size > MAX_VERSION_DIGITS){
            size = MAX_VERSION_DIGITS;
        }
        memcpy(version, ptrv, size); // Check how many characters there is in front of the period (3 max)
        *minor = atoi(version);
        ptrv = ptr1 + 1; // Skip the period
        memset(version, 0, MAX_VERSION_DIGITS + 1);
    }
    else{ // If there is no second period
        size = strlen(ptrv);
        if(size){
            if(size > MAX_VERSION_DIGITS){
                size = MAX_VERSION_DIGITS;
            }
            memcpy(version, ptrv, size); // Check how many characters there is in front of the period (3 max)
            *minor = atoi(version);
        }
        else{ // Fill rest with zeroes
            *minor = 0x00;
            *patch = 0x00;
            return;
        }
        *patch = 0x00;
        return;
    }
    
    // Check after the 2nd period
    size = strlen(ptrv);
    if(size){
        if(size > MAX_VERSION_DIGITS){
            size = MAX_VERSION_DIGITS;
        }
        memcpy(version, ptrv, size); // Check how many characters there is in front of the period (3 max)
        *patch = atoi(version);
    }
    else{ // Fill rest with zeroes
        *patch = 0x00;
    }
}

// Function to remove a specific char in a string.
void remove_all_chars(char* str, char c) 
{
    char *pr = str, *pw = str;
    while (*pr) {
        *pw = *pr++;
        pw += (*pw != c);
    }
    *pw = '\0';
}

// Function to add characters in string.
void add_char_between_n_chars(char str[], char out_str[], char ch, int n) 
{
  int len = strlen(str);

  int i, j;
  for (i = 0, j = 0; i < len; i++) {
    out_str[j++] = str[i];
    if ((i+1) % n == 0 && i != len-1) {  // add the character after every n characters except for the last character
        out_str[j++] = ch;
    }
  }
  out_str[j] = '\0';  // terminate the new string with a null character

}