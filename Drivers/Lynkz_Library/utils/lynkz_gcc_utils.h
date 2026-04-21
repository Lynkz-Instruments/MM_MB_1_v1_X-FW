/**
* @author Étienne Machabée (etienne@lynkz.ca)
* @brief GCC specific utilities
* @version 0.1
* @date 2025-05-26
* 
* @copyright Copyright (c) Lynkz Instruments Inc. Amos 2025
* 
*/

// Compile time definition of __FILENAME__, which returns the filename only
#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
