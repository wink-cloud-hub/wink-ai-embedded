# ESP-IDF example project: avoidance_car
# Usage:
#   1. Install ESP-IDF v5.x (https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
#   2. Run `export.ps1` (Windows) or `. ./export.sh` (Linux/macOS)
#   3. Run `idf.py set-target esp32`
#   4. Run `idf.py build`
#   5. Run `idf.py -p COMx flash monitor` to flash and monitor

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_avoidance_car)
