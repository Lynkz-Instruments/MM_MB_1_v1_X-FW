cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

# Set the project name
set(CMAKE_PROJECT_NAME MM_MB_1_v1_X-FW)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/Lynkz_Library/drivers/BMA400/bma400_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/driver-bma400/bma400.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/lib-stm32-uart/stm32_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/lib-stm32wl-utils/stm32wl_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/Lynkz_Library/drivers/cat24c32_api/cat24c32_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/LoRaWAN/App/lora_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/
    ${CMAKE_CURRENT_SOURCE_DIR}/Utilities/trace/adv_trace/stm32_adv_trace_if_template.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/utils.c
     ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_communication.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/Lynkz_Library/drivers/BMA400/
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/Lynkz_Library/drivers/BMA400/BMA400_SensorAPI/
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/Lynkz_Library/drivers/stm32_uart/
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/Lynkz_Library/drivers/cat24c32_api/
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/Lynkz_Library/drivers/stm32wl_utils/
    ${CMAKE_CURRENT_SOURCE_DIR}/LoRaWAN/App/
    ${CMAKE_CURRENT_SOURCE_DIR}/Utilities/trace/adv_trace/
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
