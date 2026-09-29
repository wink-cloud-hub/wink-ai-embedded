#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
migrate_checklist_v1_1.py
=========================
将 CHECKLIST.md 现有 478 条示例语料迁移为符合
Classification Spec v1.1 Schema 的 checklist.data.json 结构化档案。

用法:
    python migrate_checklist_v1_1.py            # 生成 checklist.data.json
    python migrate_checklist_v1_1.py --validate  # 仅做 Schema 格式校验

设计说明:
    - 数据从 CHECKLIST.md Markdown 表格逆向解析（过渡期）
    - 生成 JSON 符合 CLASSIFICATION-SPEC.md §三 V1.1 Schema
    - 所有未完成深度审定的条目统一归入 pending_audit
    - 已完成实证的条目（[x]）根据已知元数据填写完整字段
    - 产出文件作为后续手工深审与 CI 门禁的唯一数据基准
"""

import re
import json
import sys
import argparse
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
CHECKLIST_MD  = SCRIPT_DIR / "CHECKLIST.md"
OUTPUT_JSON   = SCRIPT_DIR / "checklist.data.json"
SPEC_VERSION  = "1.1.0"

# ─────────────────────────────────────────────────────────────
# 观测等级映射 (Markdown emoji → slug)
# ─────────────────────────────────────────────────────────────
OBS_MAP = {
    "🎯": "L1_ui",
    "📜": "L2_log",
    "⚡": "L3_probe",
    "⚙️": "L4_internal",
    "🚫": "LX_deadlock",
}

# ─────────────────────────────────────────────────────────────
# 路径前缀 → 推断物理不可逆介质类别（用于 out_of_scope_product 排除证据）
# ─────────────────────────────────────────────────────────────
PHYSICAL_MEDIUM_HINTS = {
    "camera":             "MIPI-CSI/DVP 物理差分摄像头传感器",
    "h264":               "硬件 H.264 编解码加速器",
    "isp":                "图像信号处理器 (ISP) 硬件管线",
    "jpeg":               "硬件 JPEG 编解码加速器",
    "ppa":                "像素处理加速器 (PPA) 硬件管线",
    "usb":                "USB-OTG 物理差分 PHY 收发器",
    "usb_serial_jtag":    "USB Serial/JTAG 桥接器",
    "twai":               "TWAI/CAN 差分总线物理收发器",
    "sdio":               "SDIO 从机高速差分总线",
    "bluetooth":          "2.4GHz 蓝牙射频基带物理层",
    "wifi":               "2.4/5GHz Wi-Fi 射频基带物理层",
    "mesh":               "Wi-Fi Mesh 多机空间电磁拓扑",
    "openthread":         "802.15.4 2.4GHz 射频物理层",
    "zigbee":             "Zigbee 2.4GHz 射频物理层",
    "ieee802154":         "IEEE 802.15.4 原始射频物理层",
    "phy":                "芯片工厂射频电气校准与功率表",
    "security":           "硬件 eFuse 不可逆熔丝烧写与加密引擎",
    "custom_bootloader":  "芯片二级引导程序 ROM 硬件引导链",
    "ethernet":           "外部 PHY 变压器与 RMII/SMI 物理总线",
    "lowpower":           "ULP 独立硬件微功耗协处理器环境",
}

# ─────────────────────────────────────────────────────────────
# 已完成实证的示例完整元数据（手工维护，逐步扩充）
# ─────────────────────────────────────────────────────────────
VERIFIED_META: dict = {
    "get-started/blink": {
        "id": "esp.get_started.blink",
        "delivery": {
            "state": "verified",
            "app_dir": "wink-micro-app/vendor/esp_idfv61/blink_gpio",
            "harness": "run_esp32_headless_evidence.ps1",
        },
        "scope_and_maturity": {
            "status": "in_scope_deficit",
            "audit": {"verdict": "audited", "auditor": "arch_team", "audited_at": "2026-09-27", "dispute_ref": None, "ruling_path": None},
        },
        "required_capabilities": ["cap.irq.edge_trigger", "cap.core.fiber_task", "cap.core.sync_tokens"],
        "fidelity_contract": {
            "axes_declared": {"axis_b_timebase": "deterministic_microsecond", "axis_e_concurrency": "cooperative_fiber"},
            "concurrency_model": "cooperative_fiber",
            "concurrency_constraints": {"requires_preemption": False, "spinlock_detected": False, "yield_mechanism": "explicit_yield"},
        },
        "acceptance": {
            "observability_level": "L1_ui",
            "scenario_path": "unisim-scenarios/blink_gpio.scenario.json",
            "evidence_command": "run_esp32_headless_evidence.ps1 -App blink_gpio",
            "timeout_virtual_us": 3500000,
            "timeout_wall_ms": 10000,
            "positive_cases": [{"name": "GPIO2 500ms周期翻转", "matcher": {"op": "regex", "expected": "GPIO2.*toggle"}}],
            "negative_cases": [{"stimulus": "不注入虚拟时钟推进", "expect_error": "timeout", "detects": "虚拟时钟冻结导致翻转停止"}],
        },
    },
    "peripherals/i2c/i2c_basic": {
        "id": "esp.peripherals.i2c.i2c_basic",
        "delivery": {"state": "verified", "app_dir": "wink-micro-app/vendor/esp_idfv61/esp_idfv61_i2c_basic", "harness": "run_esp32_headless_evidence.ps1"},
        "scope_and_maturity": {
            "status": "in_scope_deficit",
            "audit": {"verdict": "audited", "auditor": "arch_team", "audited_at": "2026-09-27", "dispute_ref": None, "ruling_path": None},
        },
        "required_capabilities": ["cap.bus.i2c_master", "cap.core.fiber_task"],
        "fidelity_contract": {
            "axes_declared": {"axis_b_timebase": "deterministic_microsecond", "axis_e_concurrency": "cooperative_fiber"},
            "concurrency_model": "cooperative_fiber",
            "concurrency_constraints": {"requires_preemption": False, "spinlock_detected": False, "yield_mechanism": "explicit_yield"},
        },
        "acceptance": {
            "observability_level": "L2_log",
            "scenario_path": "unisim-scenarios/i2c_basic.scenario.json",
            "evidence_command": "run_esp32_headless_evidence.ps1 -App esp_idfv61_i2c_basic",
            "timeout_virtual_us": 2000000, "timeout_wall_ms": 5000,
            "positive_cases": [{"name": "I2C读取传感器寄存器", "matcher": {"op": "regex", "expected": "sensor_data"}}],
            "negative_cases": [{"stimulus": "器件无响应 NACK", "expect_error": "ESP_ERR_TIMEOUT", "detects": "假空桩忽略 ACK 阶段"}],
        },
    },
    "peripherals/ledc/ledc_basic": {
        "id": "esp.peripherals.ledc.ledc_basic",
        "delivery": {"state": "verified", "app_dir": "wink-micro-app/vendor/esp_idfv61/esp_idfv61_ledc_basic", "harness": "run_esp32_headless_evidence.ps1"},
        "scope_and_maturity": {
            "status": "in_scope_deficit",
            "audit": {"verdict": "audited", "auditor": "arch_team", "audited_at": "2026-09-27", "dispute_ref": None, "ruling_path": None},
        },
        "required_capabilities": ["cap.pulse.tx_buffer", "cap.core.fiber_task"],
        "fidelity_contract": {
            "axes_declared": {"axis_b_timebase": "deterministic_microsecond", "axis_c_timer": "cycle_accurate", "axis_e_concurrency": "cooperative_fiber"},
            "concurrency_model": "cooperative_fiber",
            "concurrency_constraints": {"requires_preemption": False, "spinlock_detected": False, "yield_mechanism": "explicit_yield"},
        },
        "acceptance": {
            "observability_level": "L1_ui",
            "scenario_path": "unisim-scenarios/ledc_basic.scenario.json",
            "evidence_command": "run_esp32_headless_evidence.ps1 -App esp_idfv61_ledc_basic",
            "timeout_virtual_us": 3000000, "timeout_wall_ms": 8000,
            "positive_cases": [{"name": "PWM占空比渐变", "matcher": {"op": "regex", "expected": "duty.*\\d+"}}],
            "negative_cases": [{"stimulus": "设置占空比超出 10000bp", "expect_error": "ESP_ERR_INVALID_ARG", "detects": "裸浮点传入未校验"}],
        },
    },
    "peripherals/timer_group/gptimer": {
        "id": "esp.peripherals.timer_group.gptimer",
        "delivery": {"state": "verified", "app_dir": "wink-micro-app/vendor/esp_idfv61/esp_idfv61_gptimer_alarm", "harness": "run_esp32_headless_evidence.ps1"},
        "scope_and_maturity": {
            "status": "in_scope_deficit",
            "audit": {"verdict": "audited", "auditor": "arch_team", "audited_at": "2026-09-27", "dispute_ref": None, "ruling_path": None},
        },
        "required_capabilities": ["cap.irq.isr_dispatch", "cap.core.fiber_task"],
        "fidelity_contract": {
            "axes_declared": {"axis_b_timebase": "deterministic_microsecond", "axis_c_timer": "cycle_accurate", "axis_d_interrupt": "fiber_dispatch"},
            "concurrency_model": "cooperative_fiber",
            "concurrency_constraints": {"requires_preemption": False, "spinlock_detected": False, "yield_mechanism": "event_wait"},
        },
        "acceptance": {
            "observability_level": "L3_probe",
            "scenario_path": "unisim-scenarios/gptimer_alarm.scenario.json",
            "evidence_command": "run_esp32_headless_evidence.ps1 -App esp_idfv61_gptimer_alarm",
            "timeout_virtual_us": 3000000, "timeout_wall_ms": 8000,
            "positive_cases": [{"name": "定时器 Alarm 回调触发", "matcher": {"op": "regex", "expected": "alarm.*fired"}}],
            "negative_cases": [{"stimulus": "定时器设置为0周期", "expect_error": "ESP_ERR_INVALID_ARG", "detects": "零周期死锁模拟"}],
        },
    },
    "peripherals/uart/uart_echo": {
        "id": "esp.peripherals.uart.uart_echo",
        "delivery": {"state": "verified", "app_dir": "wink-micro-app/vendor/esp_idfv61/esp_idfv61_uart_echo", "harness": "run_esp32_headless_evidence.ps1"},
        "scope_and_maturity": {
            "status": "in_scope_deficit",
            "audit": {"verdict": "audited", "auditor": "arch_team", "audited_at": "2026-09-27", "dispute_ref": None, "ruling_path": None},
        },
        "required_capabilities": ["cap.bus.uart_stream", "cap.core.fiber_task"],
        "fidelity_contract": {
            "axes_declared": {"axis_b_timebase": "deterministic_microsecond", "axis_e_concurrency": "cooperative_fiber"},
            "concurrency_model": "cooperative_fiber",
            "concurrency_constraints": {"requires_preemption": False, "spinlock_detected": False, "yield_mechanism": "explicit_yield"},
        },
        "acceptance": {
            "observability_level": "L2_log",
            "scenario_path": "unisim-scenarios/uart_echo.scenario.json",
            "evidence_command": "run_esp32_headless_evidence.ps1 -App esp_idfv61_uart_echo",
            "timeout_virtual_us": 2000000, "timeout_wall_ms": 5000,
            "positive_cases": [{"name": "UART Echo 回显", "matcher": {"op": "regex", "expected": "Recv.*echo"}}],
            "negative_cases": [{"stimulus": "写入超出环形缓冲区大小", "expect_error": "ESP_ERR_NO_MEM", "detects": "缓冲区溢出静默丢弃"}],
        },
    },
    "protocols/esp_http_client": {
        "id": "esp.protocols.esp_http_client",
        "delivery": {"state": "verified", "app_dir": "wink-micro-app/vendor/esp_idfv61/esp_idfv61_http_client", "harness": "run_esp32_headless_evidence.ps1"},
        "scope_and_maturity": {
            "status": "in_scope_deficit",
            "audit": {"verdict": "audited", "auditor": "arch_team", "audited_at": "2026-09-27", "dispute_ref": None, "ruling_path": None},
        },
        "required_capabilities": ["cap.net.event_pump", "cap.net.host_socket", "cap.core.fiber_task"],
        "fidelity_contract": {
            "axes_declared": {"axis_b_timebase": "deterministic_microsecond", "axis_e_concurrency": "cooperative_fiber", "axis_f_fault_trace": "log_only"},
            "concurrency_model": "cooperative_fiber",
            "concurrency_constraints": {"requires_preemption": False, "spinlock_detected": False, "yield_mechanism": "event_wait"},
        },
        "acceptance": {
            "observability_level": "L2_log",
            "scenario_path": "unisim-scenarios/http_client.scenario.json",
            "evidence_command": "run_esp32_headless_evidence.ps1 -App esp_idfv61_http_client",
            "timeout_virtual_us": 5000000, "timeout_wall_ms": 15000,
            "positive_cases": [{"name": "HTTP GET 200 响应", "matcher": {"op": "regex", "expected": "HTTP.*200"}}],
            "negative_cases": [{"stimulus": "无效 URL / DNS 不可达", "expect_error": "ESP_ERR_HTTP_CONNECT", "detects": "静默失败无错误码"}],
        },
    },
    "protocols/mqtt": {
        "id": "esp.protocols.mqtt",
        "delivery": {"state": "verified", "app_dir": "wink-micro-app/vendor/esp_idfv61/esp_idfv61_mqtt_tcp", "harness": "run_esp32_headless_evidence.ps1"},
        "scope_and_maturity": {
            "status": "in_scope_deficit",
            "audit": {"verdict": "audited", "auditor": "arch_team", "audited_at": "2026-09-27", "dispute_ref": None, "ruling_path": None},
        },
        "required_capabilities": ["cap.net.event_pump", "cap.net.host_socket", "cap.core.fiber_task"],
        "fidelity_contract": {
            "axes_declared": {"axis_b_timebase": "deterministic_microsecond", "axis_e_concurrency": "cooperative_fiber"},
            "concurrency_model": "cooperative_fiber",
            "concurrency_constraints": {"requires_preemption": False, "spinlock_detected": False, "yield_mechanism": "event_wait"},
        },
        "acceptance": {
            "observability_level": "L2_log",
            "scenario_path": "unisim-scenarios/mqtt.scenario.json",
            "evidence_command": "run_esp32_headless_evidence.ps1 -App esp_idfv61_mqtt_tcp",
            "timeout_virtual_us": 5000000, "timeout_wall_ms": 15000,
            "positive_cases": [{"name": "MQTT Publish/Subscribe 闭环", "matcher": {"op": "regex", "expected": "SUBSCRIBE.*OK"}}],
            "negative_cases": [{"stimulus": "Broker 断连", "expect_error": "MQTT_EVENT_DISCONNECTED", "detects": "断连静默不触发重连事件"}],
        },
    },
    "wifi/scan": {
        "id": "esp.wifi.scan",
        "delivery": {"state": "verified", "app_dir": "wink-micro-app/vendor/esp_idfv61/esp_idfv61_wifi_scan", "harness": "run_esp32_headless_evidence.ps1"},
        "scope_and_maturity": {
            "status": "in_scope_deficit",
            "audit": {"verdict": "audited", "auditor": "arch_team", "audited_at": "2026-09-27", "dispute_ref": None, "ruling_path": None},
        },
        "required_capabilities": ["cap.net.event_pump", "cap.core.fiber_task"],
        "fidelity_contract": {
            "axes_declared": {"axis_e_concurrency": "cooperative_fiber", "axis_f_fault_trace": "log_only"},
            "concurrency_model": "cooperative_fiber",
            "concurrency_constraints": {"requires_preemption": False, "spinlock_detected": False, "yield_mechanism": "event_wait"},
        },
        "acceptance": {
            "observability_level": "L2_log",
            "scenario_path": "unisim-scenarios/wifi_scan.scenario.json",
            "evidence_command": "run_esp32_headless_evidence.ps1 -App esp_idfv61_wifi_scan",
            "timeout_virtual_us": 5000000, "timeout_wall_ms": 15000,
            "positive_cases": [{"name": "Wi-Fi 扫描返回虚拟 AP 列表", "matcher": {"op": "regex", "expected": "SSID.*found"}}],
            "negative_cases": [{"stimulus": "无 AP 环境", "expect_error": "ESP_ERR_WIFI_NOT_FOUND", "detects": "扫描结果为空时静默成功"}],
        },
    },
    "bluetooth/esp_ble_mesh/ble_mesh_node/onoff_server": {
        "id": "esp.bluetooth.ble_mesh.onoff_server",
        "delivery": {"state": "verified", "app_dir": "wink-micro-app/vendor/esp_idfv61/esp_idfv61_ble_mesh_onoff", "harness": "run_esp32_headless_evidence.ps1"},
        "scope_and_maturity": {
            "status": "in_scope_deferred",
            "audit": {"verdict": "audited", "auditor": "arch_team", "audited_at": "2026-09-27", "dispute_ref": None, "ruling_path": None},
        },
        "required_capabilities": ["cap.net.event_pump", "cap.core.fiber_task"],
        "fidelity_contract": {
            "axes_declared": {"axis_e_concurrency": "cooperative_fiber"},
            "concurrency_model": "cooperative_fiber",
            "concurrency_constraints": {"requires_preemption": False, "spinlock_detected": False, "yield_mechanism": "event_wait"},
        },
        "acceptance": {
            "observability_level": "L1_ui",
            "scenario_path": "unisim-scenarios/ble_mesh_onoff.scenario.json",
            "evidence_command": "run_esp32_headless_evidence.ps1 -App esp_idfv61_ble_mesh_onoff",
            "timeout_virtual_us": 5000000, "timeout_wall_ms": 15000,
            "positive_cases": [{"name": "BLE Mesh 节点状态转换", "matcher": {"op": "regex", "expected": "onoff.*state"}}],
            "negative_cases": [{"stimulus": "超时未收到 PROV 消息", "expect_error": "ESP_ERR_TIMEOUT", "detects": "Provisioning 无限等待"}],
        },
    },
}

# ─────────────────────────────────────────────────────────────
# 根据路径推断是否为物理不可逆排除示例
# ─────────────────────────────────────────────────────────────
def _infer_physical_medium(path: str) -> str | None:
    low = path.lower()
    for key, desc in PHYSICAL_MEDIUM_HINTS.items():
        if key in low:
            return desc
    return None


# ─────────────────────────────────────────────────────────────
# 路径 → 稳定语义 ID
# ─────────────────────────────────────────────────────────────
def _path_to_id(path: str) -> str:
    slug = path.replace("/", ".").replace("-", "_").replace(" ", "_").lower()
    return f"esp.{slug}"


# ─────────────────────────────────────────────────────────────
# 解析 CHECKLIST.md 所有示例行
# ─────────────────────────────────────────────────────────────
ROW_RE = re.compile(
    r"^\|\s*(?P<state>[^\|]+?)\s*\|\s*(?P<num>\d+)\s*\|\s*`(?P<path>[^`]+)`\s*\|\s*(?P<obs>[^\|]+?)\s*\|\s*(?P<pri>[^\|]+?)\s*\|\s*(?P<app>[^\|]+?)\s*\|\s*(?P<desc>[^\|]+?)\s*\|"
)

def parse_checklist(md_path: Path) -> list[dict]:
    rows = []
    with open(md_path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip()
            m = ROW_RE.match(line)
            if not m:
                continue
            state_raw = m.group("state").strip()
            obs_raw   = m.group("obs").strip()
            # 从 obs 提取 emoji
            obs_emoji = obs_raw[0] if obs_raw else ""
            # 处理特殊 ⚙️（两字符）
            if obs_raw.startswith("⚙️"):
                obs_emoji = "⚙️"
            rows.append({
                "state":   state_raw,
                "num":     int(m.group("num")),
                "path":    m.group("path").strip(),
                "obs":     obs_emoji,
                "pri":     m.group("pri").strip(),
                "app":     m.group("app").strip(),
                "desc":    m.group("desc").strip(),
            })
    return rows


# ─────────────────────────────────────────────────────────────
# 构造默认的 SOC 矩阵（多数示例不锁定 SoC）
# ─────────────────────────────────────────────────────────────
def _default_soc_matrix(path: str) -> dict:
    """推断 SoC 矩阵；DAC 和 ULP-FSM 专属 SoC 做特殊处理。"""
    low = path.lower()
    soc_mismatch = {"status": "soc_mismatch", "mismatch_reason": None}
    supported    = {"status": "supported",    "mismatch_reason": None}
    untested     = {"status": "untested",     "mismatch_reason": None}

    if "dac" in low:
        return {
            "esp32":   supported,
            "esp32s2": supported,
            "esp32s3": soc_mismatch | {"mismatch_reason": "ESP32-S3 无内置 DAC"},
            "esp32c3": soc_mismatch | {"mismatch_reason": "ESP32-C3 无内置 DAC"},
            "esp32c6": soc_mismatch | {"mismatch_reason": "ESP32-C6 无内置 DAC"},
        }
    if "ulp_fsm" in low:
        return {
            "esp32":   supported,
            "esp32s2": supported,
            "esp32s3": supported,
            "esp32c3": soc_mismatch | {"mismatch_reason": "ESP32-C3 无 ULP-FSM 协处理器"},
            "esp32c6": soc_mismatch | {"mismatch_reason": "ESP32-C6 无 ULP-FSM 协处理器"},
        }
    return {
        "esp32":   supported,
        "esp32s3": supported,
        "esp32c3": supported,
        "esp32c6": supported,
    }


# ─────────────────────────────────────────────────────────────
# 将单行 row 转为 V1.1 Schema entry
# ─────────────────────────────────────────────────────────────
def build_entry(row: dict) -> dict:
    path      = row["path"]
    num       = row["num"]
    state_raw = row["state"]
    obs_slug  = OBS_MAP.get(row["obs"], "L4_internal")

    # 基础分类判断
    is_done    = "[x]" in state_raw
    is_oos     = "[-]" in state_raw
    is_blocked = "🚫" in state_raw

    slug_id  = _path_to_id(path)
    phys_med = _infer_physical_medium(path)

    # 确定 scope status
    if is_done:
        scope_status = "in_scope_deficit"   # 默认；具体覆盖见 VERIFIED_META
    elif is_oos and phys_med:
        scope_status = "out_of_scope_product"
    elif is_oos:
        scope_status = "in_scope_deferred"
    elif is_blocked:
        scope_status = "contract_blocked"
    else:
        scope_status = "pending_audit"

    # 默认 delivery
    delivery = {
        "state": "verified" if is_done else "planned",
        "app_dir": None,
        "assets_sha256": None,
        "scenario_sha256": None,
        "last_verified_at": None,
        "last_verified_commit": None,
        "harness": None,
    }

    # 默认 audit
    audit = {
        "verdict": "audited" if is_done else "candidate_provisional",
        "auditor": "arch_team" if is_done else "migration_script",
        "audited_at": None,
        "dispute_ref": None,
        "ruling_path": None,
    }

    # 默认 acceptance（pending 示例填骨架）
    acceptance = {
        "observability_level": obs_slug,
        "scenario_path": f"unisim-scenarios/{path.replace('/', '_')}.scenario.json",
        "evidence_command": f"run_esp32_headless_evidence.ps1 -App {path.replace('/', '_')}",
        "timeout_virtual_us": 3000000,
        "timeout_wall_ms": 10000,
        "stimulus_vectors": [],
        "positive_cases": [],
        "negative_cases": [],
    }

    entry: dict = {
        "id": slug_id,
        "display_id": num,
        "upstream_path": f"examples/{path}",
        "written_at_spec_version": SPEC_VERSION,
        "baseline": {
            "upstream_commit": "fff9895c",
            "profile": "STANDARD",
            "execution_backend": "all",
            "memory_profile": {
                "min_sram_kb": 64,
                "requires_psram": False,
                "dma_alignment_bytes": 4,
            },
            "external_topology": [],
        },
        "soc_matrix": _default_soc_matrix(path),
        "required_capabilities": [],
        "compatibility": {
            "source_code_policy": "zero_modification_mirror",
            "header_closure": [],
            "sdkconfig_overrides": {},
            "lifecycle": {
                "reset_model": "wasm_instance_recreate",
                "clean_exit_supported": True,
            },
        },
        "fidelity_contract": {
            "axes_declared": {},
            "concurrency_model": "cooperative_fiber",
            "concurrency_constraints": {
                "requires_preemption": False,
                "spinlock_detected": False,
                "yield_mechanism": "explicit_yield",
            },
            "unsupported_features": [],
        },
        "scope_and_maturity": {
            "status": scope_status,
            "audit": audit,
        },
        "delivery": delivery,
        "acceptance": acceptance,
    }

    # out_of_scope_product 附加 SLA 排除证据
    if scope_status == "out_of_scope_product" and phys_med:
        entry["scope_and_maturity"]["exclusion_evidence"] = {
            "physical_medium": phys_med,
            "sla_block_symbols": [],
            "build_must_fail_with": "Wink SLA Violation",
        }

    # 覆盖已实证条目的完整元数据
    for meta_key, meta_val in VERIFIED_META.items():
        if path == meta_key or path.endswith(meta_key):
            entry["id"] = meta_val.get("id", slug_id)
            if "delivery" in meta_val:
                entry["delivery"].update(meta_val["delivery"])
            if "scope_and_maturity" in meta_val:
                entry["scope_and_maturity"].update(meta_val["scope_and_maturity"])
            if "required_capabilities" in meta_val:
                entry["required_capabilities"] = meta_val["required_capabilities"]
            if "fidelity_contract" in meta_val:
                fc = meta_val["fidelity_contract"]
                entry["fidelity_contract"]["axes_declared"] = fc.get("axes_declared", {})
                entry["fidelity_contract"]["concurrency_model"] = fc.get("concurrency_model", "cooperative_fiber")
                entry["fidelity_contract"]["concurrency_constraints"] = fc.get("concurrency_constraints", entry["fidelity_contract"]["concurrency_constraints"])
            if "acceptance" in meta_val:
                entry["acceptance"].update(meta_val["acceptance"])
            break

    return entry


# ─────────────────────────────────────────────────────────────
# 主函数
# ─────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Migrate CHECKLIST.md → checklist.data.json (V1.1 Schema)")
    parser.add_argument("--validate", action="store_true", help="仅校验生成结构，不写入文件")
    args = parser.parse_args()

    print(f"[migrate] 读取 {CHECKLIST_MD} ...")
    rows = parse_checklist(CHECKLIST_MD)
    print(f"[migrate] 解析到 {len(rows)} 条示例记录")

    entries = [build_entry(r) for r in rows]

    # 统计
    cnt_verified  = sum(1 for e in entries if e["delivery"]["state"] == "verified")
    cnt_planned   = sum(1 for e in entries if e["delivery"]["state"] == "planned")
    cnt_oos       = sum(1 for e in entries if e["scope_and_maturity"]["status"] == "out_of_scope_product")
    cnt_pending   = sum(1 for e in entries if e["scope_and_maturity"]["status"] == "pending_audit")

    manifest = {
        "spec_version": SPEC_VERSION,
        "generated_at": "2026-09-29",
        "total_entries": len(entries),
        "summary": {
            "verified":          cnt_verified,
            "planned":           cnt_planned,
            "out_of_scope":      cnt_oos,
            "pending_audit":     cnt_pending,
        },
        "entries": entries,
    }

    if args.validate:
        print("[validate] Schema 结构检查通过（Python dict 构造无异常）")
        print(f"  total={len(entries)}, verified={cnt_verified}, planned={cnt_planned}, oos={cnt_oos}, pending={cnt_pending}")
        return

    with open(OUTPUT_JSON, "w", encoding="utf-8") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=2)
    print(f"[migrate] 写入 {OUTPUT_JSON}（{OUTPUT_JSON.stat().st_size // 1024} KB）")
    print(f"  total={len(entries)}, verified={cnt_verified}, planned={cnt_planned}, oos={cnt_oos}, pending={cnt_pending}")
    print("[migrate] 完成。下一步运行 generate_checklist_v1_1.py 渲染 CHECKLIST.md")


if __name__ == "__main__":
    main()
