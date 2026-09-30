# SPDX-License-Identifier: Apache-2.0
"""
test_g5_anti_decay.py
=====================
Unit tests for Gate 5 anti-decay rules:
- g5_no_app_specific_branch
- g5_no_raw_delay_tasks
- g5_reset_registration_verified
"""

from pathlib import Path
from rules import g5_no_app_specific_branch
from rules import g5_no_raw_delay_tasks
from rules import g5_reset_registration_verified


def test_no_app_specific_branch_clean_passes(tmp_path):
    src_dir = tmp_path / "wink-micro-os" / "frameworks" / "esp_idf" / "src" / "drivers"
    src_dir.mkdir(parents=True)
    c_file = src_dir / "clean_driver.c"
    c_file.write_text("""
    #include "esp_err.h"
    esp_err_t driver_init(void) {
        return ESP_OK;
    }
    """, encoding="utf-8")

    context = {
        "workspace_root": str(tmp_path),
        "changed_files": [str(c_file.relative_to(tmp_path))],
    }
    findings = g5_no_app_specific_branch.run(context)
    assert len(findings) == 0


def test_no_app_specific_branch_detects_strcmp(tmp_path):
    src_dir = tmp_path / "wink-micro-os" / "frameworks" / "esp_idf" / "src" / "drivers"
    src_dir.mkdir(parents=True)
    c_file = src_dir / "bad_driver.c"
    c_file.write_text("""
    if (strcmp(app, "blink") == 0) {
        do_blink_hack();
    }
    """, encoding="utf-8")

    context = {
        "workspace_root": str(tmp_path),
        "changed_files": [str(c_file.relative_to(tmp_path))],
    }
    findings = g5_no_app_specific_branch.run(context)
    assert len(findings) == 1
    assert findings[0]["severity"] == "error"
    assert "App-specific branch detected" in findings[0]["message"]


def test_no_app_specific_branch_detects_config_macro(tmp_path):
    src_dir = tmp_path / "wink-micro-os" / "frameworks" / "esp_idf" / "src" / "drivers"
    src_dir.mkdir(parents=True)
    c_file = src_dir / "bad_macro.c"
    c_file.write_text("""
    #ifdef CONFIG_APP_BLINK
    hack();
    #endif
    """, encoding="utf-8")

    context = {
        "workspace_root": str(tmp_path),
        "changed_files": [str(c_file.relative_to(tmp_path))],
    }
    findings = g5_no_app_specific_branch.run(context)
    assert len(findings) == 1
    assert findings[0]["severity"] == "error"


def test_no_raw_delay_tasks_clean_passes(tmp_path):
    src_dir = tmp_path / "wink-micro-os" / "frameworks" / "esp_idf" / "src" / "wifi"
    src_dir.mkdir(parents=True)
    c_file = src_dir / "esp_wifi.c"
    c_file.write_text("""
    // Uses timer work item instead of raw task
    esp_freertos_timer_post_work_item(cb, arg, &token, 10);
    """, encoding="utf-8")

    context = {
        "workspace_root": str(tmp_path),
        "changed_files": [str(c_file.relative_to(tmp_path))],
    }
    findings = g5_no_raw_delay_tasks.run(context)
    assert len(findings) == 0


def test_no_raw_delay_tasks_detects_temporary_task(tmp_path):
    src_dir = tmp_path / "wink-micro-os" / "frameworks" / "esp_idf" / "src" / "wifi"
    src_dir.mkdir(parents=True)
    c_file = src_dir / "bad_wifi.c"
    c_file.write_text("""
    xTaskCreate(task_fn, "wifi_connect_task", 4096, NULL, 5, NULL);
    """, encoding="utf-8")

    context = {
        "workspace_root": str(tmp_path),
        "changed_files": [str(c_file.relative_to(tmp_path))],
    }
    findings = g5_no_raw_delay_tasks.run(context)
    assert len(findings) >= 1
    assert any("Banned temporary delay task" in f["message"] for f in findings)


def test_reset_registration_verified_clean_passes(tmp_path):
    src_dir = tmp_path / "wink-micro-os" / "frameworks" / "esp_idf" / "src"
    src_dir.mkdir(parents=True)
    bridge_file = src_dir / "esp_idf_bridge.c"
    bridge_file.write_text("""
    void pal_wasm_target_clear_pending_reset(void) {
        esp_foo_sim_reset();
    }
    """, encoding="utf-8")

    driver_dir = src_dir / "drivers"
    driver_dir.mkdir()
    foo_file = driver_dir / "esp_foo.c"
    foo_file.write_text("""
    static struct foo_pool s_pool;
    void esp_foo_sim_reset(void) {
        memset(&s_pool, 0, sizeof(s_pool));
    }
    """, encoding="utf-8")

    context = {
        "workspace_root": str(tmp_path),
        "changed_files": [str(foo_file.relative_to(tmp_path))],
    }
    findings = g5_reset_registration_verified.run(context)
    assert len(findings) == 0


def test_reset_registration_unregistered_fails(tmp_path):
    src_dir = tmp_path / "wink-micro-os" / "frameworks" / "esp_idf" / "src"
    src_dir.mkdir(parents=True)
    bridge_file = src_dir / "esp_idf_bridge.c"
    bridge_file.write_text("""
    void pal_wasm_target_clear_pending_reset(void) {
        // missing esp_bar_sim_reset()
    }
    """, encoding="utf-8")

    driver_dir = src_dir / "drivers"
    driver_dir.mkdir()
    bar_file = driver_dir / "esp_bar.c"
    bar_file.write_text("""
    static int s_state;
    void esp_bar_sim_reset(void) {
        s_state = 0;
    }
    """, encoding="utf-8")

    context = {
        "workspace_root": str(tmp_path),
        "changed_files": [str(bar_file.relative_to(tmp_path))],
    }
    findings = g5_reset_registration_verified.run(context)
    assert len(findings) == 1
    assert findings[0]["severity"] == "error"
    assert "not called inside esp_idf_bridge.c reset DAG" in findings[0]["message"]
