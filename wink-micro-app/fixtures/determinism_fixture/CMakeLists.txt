# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)
project(determinism_fixture C CXX)

set(WINK_APP_NAME "determinism_fixture")
set(WINK_APP_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/app_callbacks.c
    ${CMAKE_CURRENT_LIST_DIR}/device_tree.c
)
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake")
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../sample_common.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/../../sample_common.cmake")
endif()
