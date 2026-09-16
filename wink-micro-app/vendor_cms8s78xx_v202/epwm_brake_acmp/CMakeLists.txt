cmake_minimum_required(VERSION 3.20)
project(vendor_cms8s78xx_v202_epwm_brake_acmp C)

set(CMAKE_C_STANDARD 99)

add_executable(vendor_cms8s78xx_v202_epwm_brake_acmp
    main.c
    demo_epwm.c
    demo_acmp.c
    isr.c
)

target_include_directories(vendor_cms8s78xx_v202_epwm_brake_acmp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
