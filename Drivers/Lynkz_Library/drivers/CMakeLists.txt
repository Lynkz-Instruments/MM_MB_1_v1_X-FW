# Copyright (c) 2020 Circuit Dojo LLC
#
# SPDX-License-Identifier: Apache-2.0
#

add_subdirectory_ifdef(CONFIG_LYNKZ_BMI090L bmi090l)
add_subdirectory_ifdef(CONFIG_LYNKZ_MX25R mx25r)
add_subdirectory_ifdef(CONFIG_LYNKZ_S76S S76S)
add_subdirectory_ifdef(CONFIG_LYNKZ_DLHR dlhr)
add_subdirectory_ifdef(CONFIG_LYNKZ_ELVH elvh)
add_subdirectory_ifdef(CONFIG_LYNKZ_BATTERY battery)
add_subdirectory_ifdef(CONFIG_LYNKZ_ST50H ST50H)

zephyr_include_directories_ifdef(CONFIG_LYNKZ_BMI090L bmi090l/)
zephyr_include_directories_ifdef(CONFIG_LYNKZ_MX25R mx25r/)
zephyr_include_directories_ifdef(CONFIG_LYNKZ_S76S S76S/)
zephyr_include_directories_ifdef(CONFIG_LYNKZ_DLHR dlhr/)
zephyr_include_directories_ifdef(CONFIG_LYNKZ_ELVH elvh/)
zephyr_include_directories_ifdef(CONFIG_LYNKZ_BATTERY battery/)
zephyr_include_directories_ifdef(CONFIG_LYNKZ_ST50H ST50H/)
