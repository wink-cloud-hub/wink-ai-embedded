# SPDX-License-Identifier: Apache-2.0
"""
Headless Agent Synthesizer
==========================
Spawns an isolated Agent CLI (Claude-Code, OpenCode, Antigravity) to read
upstream C code and author authentic *.scenario.json files complying with
governance-sop-esp rules.
"""
from __future__ import annotations

import json
import os
import shutil
import subprocess
from pathlib import Path
from typing import Dict, List, Optional, Tuple


class AgentSynthesizer:
    """Dispatches one-shot scenario authoring tasks to headless Agent CLIs."""

    def __init__(self, workspace_root: Path, custom_agent_cmd: Optional[str] = None):
        self.ws_root = workspace_root
        self.custom_agent_cmd = custom_agent_cmd

    def detect_agent_executable(self) -> Optional[List[str]]:
        """Find the best available Agent CLI executable on system."""
        if self.custom_agent_cmd:
            return self.custom_agent_cmd.split()

        # Check in order of priority: qoderclicn -> agy -> claude -> opencode
        candidate_binaries = ["qoderclicn", "agy", "claude", "opencode"]
        for binary in candidate_binaries:
            found = shutil.which(binary) or shutil.which(f"{binary}.cmd") or shutil.which(f"{binary}.exe")
            # Also check known custom install paths if not in PATH
            if not found and binary == "qoderclicn":
                known_qoder_paths = [
                    Path(r"D:\software\ai-tools\qwenwork\QwenWorkCN\1.2.1-26092107\resources\bin\qoderclicn.exe"),
                    Path(os.path.expanduser(r"~\AppData\Roaming\Qoder\qodercli\bin\qoderclicn.exe")),
                ]
                for kp in known_qoder_paths:
                    if kp.is_file():
                        found = str(kp)
                        break

            if found:
                if binary == "agy":
                    return [found, "--dangerously-skip-permissions", "--effort", "medium", "-p"]
                if binary == "qoderclicn":
                    return [found, "--dangerously-skip-permissions", "-p"]
                if binary == "claude":
                    return [found, "--dangerously-skip-permissions", "-p"]
                if binary == "opencode":
                    return [found, "run"]
                return [found]
        return None

    def build_prompt(self, app_entry: Dict[str, Any], app_dir: Path) -> str:
        """Construct a rigorous, bounded prompt enforcing governance-sop-esp."""
        app_id = app_entry.get("id", "app")
        target_dir = app_entry.get("target_app_dir", "")
        app_name = Path(target_dir).name

        # Read C source files content
        c_files = list(app_dir.glob("*.c")) + list(app_dir.glob("main/*.c"))
        c_code_snippets = []
        for cf in c_files[:3]:
            try:
                code_text = cf.read_text(encoding="utf-8", errors="replace")
                if len(code_text) > 4000:
                    code_text = code_text[:4000] + "\n...[truncated]"
                c_code_snippets.append(f"// File: {cf.name}\n{code_text}")
            except Exception:
                pass
        c_code_combined = "\n\n".join(c_code_snippets)

        # Read wink-app.json if present
        wink_app_file = app_dir / "wink-app.json"
        wink_app_summary = ""
        if wink_app_file.is_file():
            try:
                wdata = json.loads(wink_app_file.read_text(encoding="utf-8"))
                devs = wdata.get("devices", {})
                wink_app_summary = f"Declared devices: {json.dumps(devs)}"
            except Exception:
                pass

        target_scenario_rel = f"wink-micro-app/vendor/esp_idfv61/{target_dir}/unisim-scenarios/{app_name}.scenario.json"

        prompt = f"""
[TASK] ESP-IDF 官方示例仿真治理流水线 - 场景断言自主编写

目标条目: {app_id}
工程路径: wink-micro-app/vendor/esp_idfv61/{target_dir}
{wink_app_summary}

【C 源码参考】:
{c_code_combined}

【核心指令】:
必须遵循项目技能: .agents/skills/governance-sop-esp/SKILL.md。
基于上述 C 源码的业务逻辑（分析其初始化、时钟、读写、外设操作或网络状态），直接编写合法的 UniSim 仿真场景 JSON 文件，并使用 write_to_file 工具写入到:
{target_scenario_rel}

【场景 JSON 规范要求】:
1. 顶级结构包含 "header" (version="1.0.0", name, templateId="esp_idfv61_{app_name}", accuracyMode="behavioral", timeoutUs="3000000", failurePolicy="fail-fast", determinism={{"prngSeed": 42}}) 与 "steps" 数组。
2. steps 中包含真实业务域断言（ASSERT_POINT 等）。
3. 【反模式红线禁令】:
   - 绝对禁止仅断言 power:* 电源轨（P-1 反模式，会被硬拦截拦截）！
   - 必须断言该工程所属领域的真实业务信号（如 i2c:*、uart:*、timer:*、http:*、wifi:*、gpio:*）；
   - 断言步骤数 >= 2，必须有时序因果推进（不同时间点，避免常数恒真）；
   - 范围匹配器使用合法 schema（如精确数值、{{"$between": [min, max]}}、{{"$near": {{...}}}}、{{"$gte": ...}}）。

请立即使用工具将完整的 JSON 内容写入 {target_scenario_rel}。
"""
        return prompt.strip()

    def synthesize(self, app_entry: Dict[str, Any], app_dir: Path, timeout_sec: int = 240) -> Tuple[bool, str]:
        """Spawn headless agent to author scenario.json for the given app.

        Returns (success: bool, message: str).
        """
        agent_cmd = self.detect_agent_executable()
        if not agent_cmd:
            return False, "No headless agent CLI (claude/opencode/agy) found in PATH. Use --agent-cmd or install claude CLI."

        prompt = self.build_prompt(app_entry, app_dir)
        full_cmd = agent_cmd + [prompt]

        try:
            res = subprocess.run(
                full_cmd,
                cwd=str(self.ws_root),
                capture_output=True,
                text=True,
                timeout=timeout_sec,
                encoding="utf-8",
                errors="replace",
                stdin=subprocess.DEVNULL,
            )
            if res.returncode != 0:
                return False, f"Agent CLI exited with code {res.returncode}: {res.stderr[:300]}"

            # Post-generation schema validation
            app_name = Path(app_entry.get("target_app_dir", "")).name
            scen_file = app_dir / "unisim-scenarios" / f"{app_name}.scenario.json"
            if not scen_file.is_file():
                return False, f"Agent completed successfully but target scenario file was not created: {scen_file}"

            # Validate JSON parseable
            try:
                data = json.loads(scen_file.read_text(encoding="utf-8"))
            except Exception as e:
                return False, f"Agent wrote invalid JSON in {scen_file}: {e}"

            steps = data.get("steps", [])
            if len(steps) < 2:
                return False, f"Agent wrote scenario with fewer than 2 steps: {len(steps)}"

            return True, f"Agent successfully synthesized valid scenario with {len(steps)} steps."

        except subprocess.TimeoutExpired:
            return False, f"Agent CLI timed out after {timeout_sec}s."
        except Exception as e:
            return False, f"Agent synthesis invocation failed: {e}"
