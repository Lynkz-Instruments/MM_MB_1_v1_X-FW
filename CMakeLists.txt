cmake_minimum_required(VERSION 3.22)

# =========================================================
# Firmware Version Defaults (if not provided by user)
# =========================================================
if(NOT DEFINED FW_VERSION_MAJOR)
    set(FW_VERSION_MAJOR 255)
endif()

if(NOT DEFINED FW_VERSION_MINOR)
    set(FW_VERSION_MINOR 255)
endif()

if(NOT DEFINED FW_VERSION_BUGFIX)
    set(FW_VERSION_BUGFIX 255)
endif()

message(STATUS "FW Version: ${FW_VERSION_MAJOR}.${FW_VERSION_MINOR}.${FW_VERSION_BUGFIX}")

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
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_accel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_communication.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_eeprom.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_errors.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_settings.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/driver-bma400/bma400.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/driver-cat24c32/cat24c32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/lib-stm32-uart/stm32_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/lib-stm32wl-utils/stm32wl_utils.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/driver-bma400/
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/driver-cat24c32/
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/lib-stm32-uart/
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/lib-stm32wl-utils/
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

# Add firmware version
if(DEFINED FW_VERSION_MAJOR)
    add_compile_definitions(FW_VERSION_MAJOR=${FW_VERSION_MAJOR})
endif()
if(DEFINED FW_VERSION_MINOR)
    add_compile_definitions(FW_VERSION_MINOR=${FW_VERSION_MINOR})
endif()
if(DEFINED FW_VERSION_BUGFIX)
    add_compile_definitions(FW_VERSION_BUGFIX=${FW_VERSION_BUGFIX})
endif()
