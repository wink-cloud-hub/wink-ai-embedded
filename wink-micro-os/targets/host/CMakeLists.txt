# host 一等 target（OBJECT 库）—— 提供完整 PAL 符号，供 test 与 host 样例链接。
add_library(pal_host OBJECT
    pal_hal_host.c
    pal_osal_host.c
    pal_resource_host.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/src/pal_pwm_router.c)

target_link_libraries(pal_host PUBLIC pal)

target_include_directories(pal_host PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/stubs)    # host_test_ctrl.h
