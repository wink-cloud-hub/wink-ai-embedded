/**
 * @file esp32_entry.c
 * @brief (DEPRECATED) 预留 ESP32 入口骨架——实际入口位于 esp32_firmware/main/app_main.c
 *
 * 历史（2026-07 全量评审 P0-E0）：
 *   评审时误判此文件为 "app_main 空 stub"，但真正的 app_main 已落在
 *   esp32_firmware/main/app_main.c，承担：
 *     1. nvs_flash_init()（含 erase-on-mismatch retry，ADR-0008 依赖）
 *     2. 创建 wink_runtime_task（8192B 栈, prio=5）→ 调 wink_runtime_run(cb, 0) 永久运行
 *     3. 通过 extern wink_app_get_callbacks() 按 DWINK_APP 选择 sample 的回调表
 *        （samples/<app>/app_callbacks.c 导出，由 esp32_firmware/main/app_sources.cmake
 *         自动扫描，由 tools/esp32/generate_app_sources.py 在 CMake configure 阶段驱动）
 *     4. 栈高水位 + Heap delta 每秒串口遥测（栈 <1KB / heap 泄漏 >512B 报警）
 *
 * 此文件仅保留作 "target 内可放置 early-boot hook（如 early UART bw, pin mux,
 * app-dependent brownout handler）" 的占位。当前无任何 hook 需求，不参与编译。
 */
