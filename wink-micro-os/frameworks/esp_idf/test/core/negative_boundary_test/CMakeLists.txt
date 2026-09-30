# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.15)
project(negative_boundary_test C)

add_executable(negative_boundary_consumer negative_boundary_consumer.c)
target_link_libraries(negative_boundary_consumer PRIVATE wink_framework_esp_idf)
