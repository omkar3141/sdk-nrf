#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bluetooth_mesh_light_ctrl)

# NORDIC SDK APP START
target_sources(app PRIVATE
  src/main.c
  src/model_handler.c
  src/lc_pwm_led.c
)

if(CONFIG_LIGHT_CTRL_ZMS_FOOTPRINT_DBG)
  target_sources(app PRIVATE src/zms_footprint_dbg.c)
endif()

target_include_directories(app PRIVATE include)
# NORDIC SDK APP END
