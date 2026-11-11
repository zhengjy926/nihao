# SPDX-License-Identifier: Apache-2.0
#
# Host-side application used only to generate the devicetree/Kconfig header
# snapshot. It is never compiled into the firmware.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(config_gen)

target_sources(app PRIVATE src/main.c)
