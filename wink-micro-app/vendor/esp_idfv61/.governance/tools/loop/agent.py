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

        for binary in ["claude", "opencode", "agy"]:
            found = shutil.which(binary) or shutil.which(f"{binary}.cmd") or shutil.which(f"{binary}.exe")
            if found:
                if binary == "claude":
                    return [found, "-p"]
                if binary == "opencode":
                    return [found, "run"]
                if binary == "agy":
                    return [found, "exec"]
                return [found]
        return None

    def build_prompt(self, app_entry: Dict[str, Any], app_dir: Path) -> str:
        """Construct a rigorous, bounded prompt enforcing governance-sop-esp."""
        app_id = app_entry.get("id", "app")
        target_dir = app_entry.get("target_app_dir", "")
        app_name = Path(target_dir).name

        # List C source files
        c_files = list(app_dir.glob("*.c")) + list(app_dir.glob("main/*.c"))
        c_file_rel = [str(f.relative_to(self.ws_root)).replace("\\", "/") for f in c_files]

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
源代码列表: {c_file_rel}
{wink_app_summary}

【核心指令】:
1. 必须严格遵循项目技能: .agents/skills/governance-sop-esp/SKILL.md 与 references/domain-assertion-guide.md。
2. 阅读该示例的 C 源码，分析其业务输入、处理、输出因果链。
3. 重写或创建场景文件: {target_scenario_rel}。
4. 【反模式红线禁令】:
   - 绝对禁止仅断言 power:* 电源轨（P-1 反模式，Gate 1 门栓会硬拦截）！
   - 必须断言该工程所属领域的真实业务信号（如 i2c:*、uart:*、timer:*、http:*、wifi:*、gpio:*）；
   - 断言步骤数 >= 2，必须包含时序因果推进（避免常数恒真）；
   - 若断言 gpio:<pin>，引脚必须在 wink-app.json 的 devices 中存在；
   - 范围匹配器请使用合法 schema（例如 {{"$between": [min, max]}}、{{"$near": {{...}}}} 或精确标量）。

完成后直接写入 {target_scenario_rel} 并退出，无需输出无关废话。
"""
        return prompt.strip()

    def synthesize(self, app_entry: Dict[str, Any], app_dir: Path, timeout_sec: int = 180) -> Tuple[bool, str]:
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
