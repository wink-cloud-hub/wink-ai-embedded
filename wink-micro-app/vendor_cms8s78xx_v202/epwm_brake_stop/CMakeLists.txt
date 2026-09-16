cmake_minimum_required(VERSION 3.20)
project(vendor_cms8s78xx_v202_epwm_brake_stop C)

set(CMAKE_C_STANDARD 99)

add_executable(vendor_cms8s78xx_v202_epwm_brake_stop
    main.c
    demo_epwm.c
    isr.c
)

target_include_directories(vendor_cms8s78xx_v202_epwm_brake_stop PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
