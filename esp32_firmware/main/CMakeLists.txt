# ============================================================
# 自动扫描 App 源文件：零改 esp32_firmware，换 App / 增删源文件全自动
# 方式一：CMake configure 阶段自动跑脚本（推荐，idf.py build 完全无感）
# 方式二：手动指定 WINK_APP 覆盖默认：idf.py build -DWINK_APP=avoidance_car
# ============================================================
if(NOT DEFINED WINK_APP)
    set(WINK_APP "devkitc_smoke" CACHE STRING "Samples app name to build")
endif()

# configure 阶段自动跑脚本，生成 app_sources.cmake
# 注意：用 COMPONENT_DIR 而非 CMAKE_CURRENT_SOURCE_DIR——ESP-IDF 在 build 目录处理组件依赖时后者会变
execute_process(
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
        -File "${COMPONENT_DIR}/../generate_app_sources.ps1"
        -AppName "${WINK_APP}"
    WORKING_DIRECTORY "${COMPONENT_DIR}/.."
    RESULT_VARIABLE GEN_SCRIPT_RESULT
)

if(NOT GEN_SCRIPT_RESULT EQUAL 0)
    message(FATAL_ERROR "generate_app_sources.ps1 failed with code ${GEN_SCRIPT_RESULT}")
endif()

# 引入自动生成的源文件列表
include(${COMPONENT_DIR}/app_sources.cmake)

idf_component_register(
    SRCS
        "app_main.c"
        ${WINK_APP_SOURCES}  # ← 自动扫描得到（app + samples/common）
    INCLUDE_DIRS
        "."
        ${WINK_APP_DIR}      # ← 自动得到 App 头文件目录
        ${WINK_APP_COMMON_INCLUDE_DIR}  # ← samples/common/include (default_telemetry / blink / sim_echo)
    REQUIRES driver esp32
)

# Windows GCC 默认按 GBK 解析源文件，中文 UTF-8 注释会乱码报错
# 强制指定源文件编码为 UTF-8，解决中文注释编译错误
target_compile_options(${COMPONENT_LIB} PRIVATE
    -finput-charset=UTF-8
    -fexec-charset=UTF-8
)
