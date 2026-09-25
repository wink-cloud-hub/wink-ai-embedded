#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# ESP-IDF facade line-coverage pipeline (M3 Task M3-2, T-011).
# Run from the repository root: bash wink-micro-os/frameworks/esp_idf/tools/coverage.sh
set -euo pipefail

BUILD_DIR="build_cov"
rm -rf "${BUILD_DIR}"

# TARGET_PLATFORM=host is mandatory: the default wasm platform does not register
# the test/ tree, so ctest would find 0 tests (exit 0) and coverage would be empty.
cmake -B "${BUILD_DIR}" -S wink-micro-os \
    -DENABLE_ESP_IDF_FRAMEWORK=ON \
    -DTARGET_PLATFORM=host \
    -DWINK_ENABLE_COVERAGE=ON \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build "${BUILD_DIR}" -j"$(nproc)"
ctest --test-dir "${BUILD_DIR}" -L esp_idf --output-on-failure

# Collect and keep only frameworks/esp_idf/src
lcov --capture --directory "${BUILD_DIR}" --output-file "${BUILD_DIR}/coverage_all.info"
lcov --extract "${BUILD_DIR}/coverage_all.info" "*/frameworks/esp_idf/src/*" \
    --output-file "${BUILD_DIR}/coverage_filtered.info"
genhtml "${BUILD_DIR}/coverage_filtered.info" --output-directory "${BUILD_DIR}/coverage_html"

echo "HTML report generated at: ${BUILD_DIR}/coverage_html/index.html"
