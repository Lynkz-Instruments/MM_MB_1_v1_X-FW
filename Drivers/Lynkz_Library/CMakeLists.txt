# Copyright (c) 2020 Circuit Dojo LLC
#
# SPDX-License-Identifier: Apache-2.0
#

add_subdirectory(drivers)
add_subdirectory(peripherals)
add_subdirectory(utils)

zephyr_include_directories(drivers/)
zephyr_include_directories(peripherals/)
zephyr_include_directories(utils/)