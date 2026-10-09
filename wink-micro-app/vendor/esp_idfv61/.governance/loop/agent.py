# SPDX-License-Identifier: Apache-2.0
"""
Headless Agent Synthesizer & Dual-Agent Orchestrator
====================================================
Spawns isolated Agent CLIs (Claude-Code, OpenCode, Antigravity, QoderCLI)
to author scenarios, perform double-blind adversarial reviews, and synthesize
remediation patches complying with governance-sop-esp and ADR-0092 rules.
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


class AgentSynthesizer:
    """Dispatches one-shot authoring and dual-agent adversarial review tasks."""

    QUOTA_EXHAUSTED_KEYWORDS: Tuple[str, ...] = (
        "quota",
        "exhausted",
        "rate limit",
        "429",
        "too many requests",
        "resource has been exhausted",
        "insufficient_quota",
        "insufficient balance",
        "credit limit",
        "credits",
        "token limit",
        "额度",
        "欠费",
        "余额不足",
        "超出限制",
        "超出配额",
        "访问频次超限",
        "并发超限",
    )

    def __init__(
        self,
        workspace_root: Path,
        custom_agent_cmd: Optional[str] = None,
        custom_agent_a_cmd: Optional[str] = None,
        custom_agent_b_cmd: Optional[str] = None,
        qoder_model: str = "Qwen3.8-Flash",
    ):
        self.ws_root = workspace_root
        self.custom_agent_cmd = custom_agent_cmd
        self.custom_agent_a_cmd = custom_agent_a_cmd or custom_agent_cmd
        self.custom_agent_b_cmd = custom_agent_b_cmd or custom_agent_cmd
        self.qoder_model = qoder_model

    @classmethod
    def is_quota_exhausted(cls, output: str) -> bool:
        """Check whether execution output indicates API quota or rate limit exhaustion."""
        if not output:
            return False
        out_lower = output.lower()
        return any(kw in out_lower for kw in cls.QUOTA_EXHAUSTED_KEYWORDS)

    def detect_all_candidate_executables(self, role: str = "A") -> List[Tuple[str, List[str]]]:
        """Find all available Agent CLI executables in role priority order.

        Role A: Primary proposal author (defaults to qoderclicn [-m Qwen3.8-Flash] -> agy -> claude).
        Role B: Adversarial red-team auditor (defaults to agy -> claude -> qoderclicn).

        Returns list of (binary_name, command_arguments) tuples.
        """
        if role == "A" and self.custom_agent_a_cmd:
            parts = self.custom_agent_a_cmd.split()
            return [(parts[0], parts)]
        if role == "B" and self.custom_agent_b_cmd:
            parts = self.custom_agent_b_cmd.split()
            return [(parts[0], parts)]
        if self.custom_agent_cmd:
            parts = self.custom_agent_cmd.split()
            return [(parts[0], parts)]

        # Prioritization based on role
        if role == "A":
            candidate_binaries = ["qoderclicn", "agy", "claude", "opencode"]
        else:
            candidate_binaries = ["agy", "claude", "qoderclicn", "opencode"]

        available: List[Tuple[str, List[str]]] = []
        for binary in candidate_binaries:
            found = shutil.which(binary) or shutil.which(f"{binary}.cmd") or shutil.which(f"{binary}.exe")
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
                    effort = "high" if role == "B" else "medium"
                    cmd = [found, "--dangerously-skip-permissions", "--effort", effort, "-p"]
                elif binary == "qoderclicn":
                    cmd = [found]
                    if self.qoder_model:
                        cmd.extend(["-m", self.qoder_model])
                    cmd.extend(["--dangerously-skip-permissions", "-p"])
                elif binary == "claude":
                    cmd = [found, "--dangerously-skip-permissions", "-p"]
                elif binary == "opencode":
                    cmd = [found, "run"]
                else:
                    cmd = [found]
                available.append((binary, cmd))

        return available

    def detect_agent_executable(self, role: str = "A") -> Optional[List[str]]:
        """Find the best available Agent CLI executable for the given role (primary candidate)."""
        candidates = self.detect_all_candidate_executables(role=role)
        return candidates[0][1] if candidates else None

    def invoke_agent(
        self,
        prompt: str,
        role: str = "A",
        timeout_sec: int = 300,
        enable_failover: bool = True,
    ) -> Tuple[int, str]:
        """Invoke an isolated headless agent CLI subagent with clean context and failover support."""
        candidate_cmds = self.detect_all_candidate_executables(role=role)
        if not candidate_cmds:
            return 1, f"No agent CLI available on system for Role {role}. Please specify --agent-cmd or install agy/claude/qoderclicn."

        if not enable_failover:
            candidate_cmds = candidate_cmds[:1]

        last_rc = 1
        last_output = ""
        diagnostic_chain: List[str] = []

        for idx, (binary_name, agent_cmd) in enumerate(candidate_cmds):
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
                stdout = (res.stdout or "").strip()
                stderr = (res.stderr or "").strip()
                if stdout and stderr:
                    output = f"{stdout}\n--- [stderr] ---\n{stderr}"
                else:
                    output = stdout or stderr

                last_rc = res.returncode
                last_output = output

                if res.returncode == 0:
                    if idx > 0:
                        print(f"  [agent:failover] Fallback candidate '{binary_name}' succeeded for Role {role}.", flush=True)
                    return 0, output

                is_quota = self.is_quota_exhausted(output)
                fail_tag = "QUOTA_EXHAUSTED" if is_quota else f"EXIT_{res.returncode}"
                diagnostic_chain.append(f"Candidate '{binary_name}' [{fail_tag}]: {output[:150]}")

                if is_quota:
                    print(f"  [agent:alert] Role {role} Agent '{binary_name}' quota exhausted or rate-limited: {output[:120]}", flush=True)
                else:
                    print(f"  [agent:warn] Role {role} Agent '{binary_name}' exited with code {res.returncode}.", flush=True)

            except subprocess.TimeoutExpired:
                last_rc = 124
                last_output = f"Agent Role {role} ({binary_name}) timed out after {timeout_sec}s."
                diagnostic_chain.append(f"Candidate '{binary_name}' timed out after {timeout_sec}s")
                print(f"  [agent:warn] Role {role} Agent '{binary_name}' timed out after {timeout_sec}s.", flush=True)
            except Exception as e:
                last_rc = 1
                last_output = f"Agent invocation failed for {binary_name}: {e}"
                diagnostic_chain.append(f"Candidate '{binary_name}' error: {e}")
                print(f"  [agent:error] Failed invoking '{binary_name}': {e}", flush=True)

            if idx + 1 < len(candidate_cmds):
                next_binary = candidate_cmds[idx + 1][0]
                print(f"  [agent:failover] Failing over from '{binary_name}' to next candidate '{next_binary}' for Role {role}...", flush=True)

        # All available candidates failed
        if self.is_quota_exhausted(last_output):
            final_msg = f"[QUOTA_EXHAUSTED] All candidate agents failed. Last output: {last_output}"
        else:
            summary = " -> ".join(diagnostic_chain) if diagnostic_chain else last_output
            final_msg = f"[AGENT_EXECUTION_FAILED] All candidate agents failed: {summary}"

        return last_rc if last_rc != 0 else 1, final_msg

    # -------------------------------------------------------------------------
    # Prompt Construction
    # -------------------------------------------------------------------------

    def build_prompt(self, app_entry: Dict[str, Any], app_dir: Path) -> str:
        """Construct standard authoring prompt for Phase 1 scenario authoring."""
        app_id = app_entry.get("id", "app")
        target_dir = app_entry.get("target_app_dir", "")
        app_name = Path(target_dir).name

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

        wink_app_file = app_dir / "wink-app.json"
        wink_app_summary = ""
        if wink_app_file.is_file():
            try:
                wdata = json.loads(wink_app_file.read_text(encoding="utf-8"))
                devs = wdata.get("devices", {})
                wink_app_summary = f"Declared devices: {json.dumps(devs)}"
            except Exception:
                pass

        # L1-T2: Target isolated candidate snapshot directory, never formal repo
        try:
            target_scenario_rel = (app_dir / "unisim-scenarios" / f"{app_name}.scenario.json").resolve().relative_to(self.ws_root.resolve()).as_posix()
            candidate_app_rel = app_dir.resolve().relative_to(self.ws_root.resolve()).as_posix()
        except Exception:
            target_scenario_rel = str(app_dir / "unisim-scenarios" / f"{app_name}.scenario.json").replace("\\", "/")
            candidate_app_rel = str(app_dir).replace("\\", "/")

        prompt = f"""
[TASK] ESP-IDF 官方示例仿真治理流水线 - 场景断言自主编写 (L1 隔离快照模式)

目标条目: {app_id}
隔离候选路径: {candidate_app_rel}
【安全边界红线】: 严禁写入正式目录 wink-micro-app/vendor/esp_idfv61/{target_dir}，所有操作仅限在上述隔离快照内执行！
{wink_app_summary}

【C 源码参考】:
{c_code_combined}

【核心指令】:
必须遵循项目技能: .agents/skills/governance-sop-esp/SKILL.md。
基于上述 C 源码的业务逻辑（分析其初始化、时钟、读写、外设操作或网络状态），直接编写合法的 UniSim 仿真场景 JSON 文件，并使用 write_to_file 工具写入到隔离路径:
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

    def build_root_cause_prompt(
        self,
        app_entry: Dict[str, Any],
        app_dir: Path,
        failure_log: str,
        investigation_dir: Path,
    ) -> str:
        """Construct Root Cause Analysis & Remediation Proposal prompt for Agent A."""
        app_id = app_entry.get("id", "app")
        target_dir = app_entry.get("target_app_dir", "")
        app_name = Path(target_dir).name

        prompt = f"""
[TASK] ESP-IDF 官方示例仿真基线失败 - 根因调查 (RCA) 与自愈方案出案 (Role A: Proposer)

目标条目: {app_id}
工程目录: wink-micro-app/vendor/esp_idfv61/{target_dir}
调查资产目录: {investigation_dir.as_posix()}

【原始仿真报错日志】:
```text
{failure_log[-4000:]}
```

【核心要求与职责】:
请基于以上报错日志和源码进行深入排查，并在调查资产目录下产出两份不可变资产与补丁草案：
1. 写入 `{investigation_dir.as_posix()}/01-ROOT-CAUSE-ANALYSIS.md`:
   - 描述失败现象、调用栈追踪、因果链断点；
   - 明确进行【归因判定 (五级归因矩阵)】:
     * Layer C (App配置): 引脚缺失、Kconfig 未开、超时不足
     * Layer A (C 框架外设/协议层): frameworks/esp_idf 缺少协议状态机桩 (如 MQTT Broker 响应)
     * Layer Core-B (PAL 通用外设增量): 缺少片上通用外设 (如 DAC/Touch/SDMMC/WDT)，须严格纯增量
     * Layer D (外置插件外设层): I2C/SPI 从机无应答、读 WHO_AM_I 为 0/FF、缺少传感器/显示/执行器等外部芯片
     * Layer B1 (UniSim TS内核) / Layer Core-A (OSAL 内核调度): 若属此类，必须明确标注并建议人类审批熔断。

【Layer D: 外置插件外设判定与实施军规 (SSOT)】:
若排查确认仿真失败原因为缺少外部芯片/传感器：
1. 必须将缺陷明确归类为: 【Layer D (Peripheral Plugin)】；
2. 必须先行读取权威文档:
   - D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/implementation-plans/wokwi-dal-type-coverage-type/00-master-execution-plan.md
   - docs/implementation-plans/wokwi-dal-type-coverage-type/00.1-category-type-variant-wokwi-ssot.md
3. 查表确定标准命名:
   * 严禁裸芯片型号作为 type (如严禁 type: mpu9250，必须为 type: imu, variant: mpu9250_i2c)
   * 确认对应的 Wokwi 原生组件标识 (如 <wokwi-mpu6050>) 与物理引脚拓扑
4. 区分已有插件 vs 全新插件:
   * 若 wink-plugin-peripherals/builtin/<type>/dist/manifest.json 已就绪，直接编排 wink-app.json 引脚拓扑 (分支 D1.A)
   * 若为全新器件类型，在 docs/implementation-plans/wokwi-dal-type-coverage-type/ 下编撰标准子计划 (分支 D1.B)。
2. 写入 `{investigation_dir.as_posix()}/02-REMEDIATION-PLAN.md`:
   - 技术修复策略、修改文件列表、验收标准；
   - 架构防腐红线自查 (杜绝空桩、杜绝 app_name 特判、杜绝私降编译参数、保持 PAL 跨平台纯度、遵循外设军规 P-1~P-8)；
   - 在方案正文中必须包含标准的 Unified Diff 代码补丁（使用 ```diff 格式），或者直接将补丁写入 `{investigation_dir.as_posix()}/patch.diff`。

【patch.diff 格式硬性约束】:
- 必须是标准的 Unified Diff 格式（以 `--- a/path` 和 `+++ b/path` 开头，上下文行数=3）；
- 涉及的文件路径必须严格遵守 SAFE_WRITE_WHITELIST，严禁修改 OSAL 核心或 UniSim TS 核心调度；
- 若涉及 PAL 外设扩展（wink-micro-os/pal/include/hal/），必须满足纯增量原则并提供 targets/wasm 与 targets/esp32 同源适配，严禁引入 esp_*.h 头文件；
- 若涉及外设插件，必须包含子计划与规范的 builtin/<type>/ 插件代码，严禁裸芯片型号作为 type。

请立即开始分析并创建上述文档。
"""
        return prompt.strip()

    def build_adversarial_review_prompt(
        self,
        app_entry: Dict[str, Any],
        raw_failure_log: str,
        plan_content: str,
        patch_diff: str,
    ) -> str:
        """Construct Double-Blind Adversarial Red-Team Review prompt for Agent B.

        Notice: Agent A's internal chain-of-thought is strictly stripped to prevent anchoring.
        """
        app_id = app_entry.get("id", "app")
        target_dir = app_entry.get("target_app_dir", "")

        prompt = f"""
[ROLE] 独立对抗审查裁判 (Role B: Adversarial Red-Team Auditor)
你是一名严苛的嵌入式系统架构师。你的唯一 KPI 是发现技术方案和代码补丁中的隐蔽缺陷、反模式合谋、假桩作弊及破坏性跨平台污染。
你必须对方案进行无情的挑刺与审查！

【待审工程】: {app_id} ({target_dir})

【原始失败日志】:
```text
{raw_failure_log[-2000:]}
```

【待审方案正文 (02-REMEDIATION-PLAN.md)】:
{plan_content}

【待审物理补丁 (patch.diff)】:
```diff
{patch_diff[:6000]}
```

【审判准则与反模式红线 (8 大硬检 + 外设 4 大军规)】:
1. H-1 空桩审查：严禁任何只 return ESP_OK / 0 的虚假空桩；
2. H-2 特判审查：严禁在 C 框架中针对本工程名称进行 strstr / 硬编码分支；
3. H-3 变异审查：严禁试图绕过 Canary 变异击杀机制；
4. H-4 告警降级：严禁注释掉 -Werror 或关闭编译器告警；
5. H-5 PAL 纯洁度审查：若修改了 pal/，必须为严格纯增量，严禁引入任何 esp_*.h 或厂商专有头文件，必须三位一体（头文件 + wasm桩 + esp32驱动）；
6. H-6 浮点 PWM：严禁使用裸 float 占空比 API，必须使用 pal_pwm_set_duty_bp；
7. H-7 许可证合规：新增文件必须包含正确 SPDX-License-Identifier；
8. H-8 安全白名单：改动必须严格限制在应用自身与 frameworks/esp_idf, pal/hal, builtin 插件或 doc plans。

【外设插件专项找茬红线 (Role B 重点对抗项)】:
- P-5 (SSOT 命名合规性): 方案是否违反了 00-master-execution-plan.md？是否存在自创裸芯片 type 行为？
- P-6 (引脚物理 1:1 对齐): 仿真模型引脚是否与 Wokwi 物理规格 1:1 对齐？是否有遗漏中断引脚或辅助总线引脚？
- P-7 (时序与寄存器诚实度): 是否实现了真实的寄存器映射与时序响应？是否存在无状态伪造假桩？
- P-8 (多用例状态隔离): 插件是否提供了清理与重置机制？是否会泄漏状态给下一个测试用例？
凡违反上述任一红线，必须判定为 REVISE_REQUIRED 或 REJECTED！

【输出要求】:
必须以 YAML Frontmatter 开头，随后给出详细审查理由：
```markdown
---
verdict: APPROVED | REVISE_REQUIRED | REJECTED
he_checks_passed: true | false
blocking_issues_count: 0
---

## 对抗审查详细评估

### 1. 架构防腐红线审查
...
### 2. 缺陷归因与最小爆炸半径
...
### 3. PAL 跨平台纯洁度与通用性审计 (Vendor-Neutrality Audit)
...
### 4. 阻断性缺陷清单 (Blocking Issues)
...
### 5. 改进吸收建议 (Actionable Recommendations)
...
```
请开始你的挑刺审查！
"""
        return prompt.strip()

    def build_synthesis_prompt(
        self,
        app_entry: Dict[str, Any],
        original_plan: str,
        review_content: str,
        investigation_dir: Path,
    ) -> str:
        """Construct Deep Synthesis prompt for Agent A to reconstruct plan body."""
        app_id = app_entry.get("id", "app")

        prompt = f"""
[TASK] 自愈方案深度融合与正文重构 (Role A: Synthesis)

目标条目: {app_id}
调查目录: {investigation_dir.as_posix()}

【评审裁判出具的对抗找茬记录 (03-ADVERSARIAL-REVIEW.md)】:
{review_content}

【原方案正文 (02-REMEDIATION-PLAN.md 初稿)】:
{original_plan}

【核心指令 - 深度重构协议 (Deep Synthesis Protocol)】:
1. 绝对禁止仅在文档尾部追加文本！你必须【重构修改正文中的具体实现方案、架构图或补丁草案】以响应裁判的 Blocking Issues 与 Recommendations；
2. 在重构完正文后，在文末增加章节: `## 4. 评审建议融合记录（Synthesis Log）`，逐条对照裁判的清单写明采纳修改详情；
3. 将更新后的方案写回: `{investigation_dir.as_posix()}/02-REMEDIATION-PLAN.md`；
4. 将更新后的完整标准 Unified Diff 补丁写回: `{investigation_dir.as_posix()}/patch.diff`。

请立即执行深度重构并保存文件。
"""
        return prompt.strip()

    def build_post_exec_audit_prompt(
        self,
        app_entry: Dict[str, Any],
        plan_content: str,
        patch_diff: str,
    ) -> str:
        """Construct Post-Execution Completeness Audit prompt for Agent A based on DoD checklist."""
        app_id = app_entry.get("id", "app")

        prompt = f"""
[TASK] 自愈实施后完整性自查与 DoD 对照审计 (Role A: Post-Execution Audit)

目标条目: {app_id}

你刚刚已经完成了 02-REMEDIATION-PLAN.md 的物理代码初版实施与 patch.diff 导出。
现在请对照【原方案承诺的验收标准 (DoD)】与【实际生成的 patch.diff】进行严密的客观对比自查：

【原方案正文 (02-REMEDIATION-PLAN.md)】:
{plan_content}

【已生成的物理补丁 (patch.diff)】:
```diff
{patch_diff[:6000]}
```

【强制自查四项清单】:
1. 方案覆盖度对照：02-REMEDIATION-PLAN.md 中承诺的每一个技术要点，在 patch.diff 中是否均有物理代码落地？是否存在遗漏？
2. 三位一体与接口完整性：新增/修改的 C 接口是否在对应头文件中正确导出？Wasm 仿真端与 ESP32 物理端是否双向闭环？
3. 边界与防御完整性：超时、缓冲区溢出、空指针及异常返回路径是否均有正确处理，是否存在未完成的 TODO？
4. 机器合规洁癖：是否存在任何未删掉的测试硬编码、浮点 PWM 或临时日志？

【二值化判决输出规范 (严格防过度设计与反向画蛇添足)】:
你必须输出以下两者之一，严禁无病呻吟发散：
- 情况 A (确实完全交付，无任何遗漏):
  输出:
  VERDICT: FULLY_COMPLETE
  理由简述（列出各项 DoD 已 100% 满足的事实证据）。
  （此时严禁添加任何新特性或重构！）

- 情况 B (发现确凿遗漏):
  输出:
  VERDICT: GAPS_FOUND
  明确列出缺失的要点（例如：漏掉了头文件导出或超时重试分支）。
  并直接给出增量补遗补丁：
  ```diff
  ... 增量修复 diff ...
  ```
"""
        return prompt.strip()

    @staticmethod
    def parse_post_exec_audit_verdict(audit_text: str) -> Tuple[str, Optional[str]]:
        """Parse post-execution audit verdict and optional supplementary diff.

        Returns (verdict, optional_supplementary_diff).
        verdict is 'FULLY_COMPLETE' or 'GAPS_FOUND'.
        """
        verdict = "FULLY_COMPLETE"
        if re.search(r"\bVERDICT:\s*GAPS_FOUND\b", audit_text, re.IGNORECASE):
            verdict = "GAPS_FOUND"
        elif re.search(r"\bVERDICT:\s*FULLY_COMPLETE\b", audit_text, re.IGNORECASE):
            verdict = "FULLY_COMPLETE"

        supplementary_diff = AgentSynthesizer.extract_patch_diff(audit_text) if verdict == "GAPS_FOUND" else None
        return verdict, supplementary_diff

    # -------------------------------------------------------------------------
    # Parsing Helpers
    # -------------------------------------------------------------------------

    @staticmethod
    def extract_patch_diff(text: str) -> Optional[str]:
        """Extract standard unified diff block from markdown code blocks or raw text."""
        # Check ```diff blocks
        diff_match = re.search(r"```(?:diff|patch)\s*\n(.*?)```", text, re.DOTALL)
        if diff_match:
            candidate = diff_match.group(1).strip()
            if "--- " in candidate and "+++ " in candidate:
                return candidate

        # Check raw text for diff headers
        if "--- a/" in text and "+++ b/" in text:
            start_idx = text.find("--- a/")
            return text[start_idx:].strip()

        return None

    @staticmethod
    def parse_review_verdict(review_text: str) -> Tuple[str, bool, int, List[str]]:
        """Parse YAML frontmatter from review text to extract verdict and issues.

        Returns (verdict, he_checks_passed, blocking_issues_count, recommendations).
        """
        verdict = "REVISE_REQUIRED"
        he_checks = True
        blocking_count = 0
        recommendations: List[str] = []

        frontmatter_match = re.search(r"^---\s*\n(.*?)\n---", review_text, re.DOTALL)
        if frontmatter_match:
            fm_text = frontmatter_match.group(1)
            for line in fm_text.splitlines():
                if line.startswith("verdict:"):
                    v_str = line.split(":", 1)[1].strip().upper()
                    if v_str in ("APPROVED", "REVISE_REQUIRED", "REJECTED"):
                        verdict = v_str
                elif line.startswith("he_checks_passed:"):
                    he_str = line.split(":", 1)[1].strip().lower()
                    he_checks = (he_str == "true")
                elif line.startswith("blocking_issues_count:"):
                    try:
                        blocking_count = int(line.split(":", 1)[1].strip())
                    except ValueError:
                        blocking_count = 0

        # Extract recommendations
        rec_match = re.search(r"### 5\.\s*改进吸收建议.*?\n(.*?)(?=\n##|\Z)", review_text, re.DOTALL)
        if rec_match:
            for rline in rec_match.group(1).splitlines():
                s = rline.strip()
                if s and (s.startswith("-") or re.match(r"^\d+\.", s)):
                    recommendations.append(s)

        return verdict, he_checks, blocking_count, recommendations

    # -------------------------------------------------------------------------
    # Legacy / Compatibility Synthesis method
    # -------------------------------------------------------------------------

    def synthesize(self, app_entry: Dict[str, Any], app_dir: Path, timeout_sec: int = 240) -> Tuple[bool, str]:
        """Spawn headless agent to author scenario.json for the given app (Phase 1)."""
        prompt = self.build_prompt(app_entry, app_dir)
        rc, out = self.invoke_agent(prompt, role="A", timeout_sec=timeout_sec)
        if rc != 0:
            if self.is_quota_exhausted(out):
                return False, f"[QUOTA_EXHAUSTED] Scenario authoring blocked by model quota: {out[:500]}"
            return False, f"Agent CLI exited with code {rc}: {out[:500]}"

        app_name = Path(app_entry.get("target_app_dir", "")).name
        scen_file = app_dir / "unisim-scenarios" / f"{app_name}.scenario.json"
        if not scen_file.is_file():
            if self.is_quota_exhausted(out):
                return False, f"[QUOTA_EXHAUSTED] Scenario file was not created due to model quota exhaustion: {out[:500]}"
            return False, f"Agent completed with returncode 0 but target scenario file was not created: {scen_file} (Output: {out[:300]})"

        try:
            data = json.loads(scen_file.read_text(encoding="utf-8"))
        except Exception as e:
            return False, f"Agent wrote invalid JSON in {scen_file}: {e}"

        steps = data.get("steps", [])
        if len(steps) < 2:
            return False, f"Agent wrote scenario with fewer than 2 steps: {len(steps)}"

        return True, f"Agent successfully synthesized valid scenario with {len(steps)} steps."

    @staticmethod
    def detect_peripheral_type_from_failure(failure_log: str) -> Optional[str]:
        """Detect peripheral type keyword from failure log (e.g. imu, motion, rtc)."""
        log_lower = failure_log.lower()
        if "mpu" in log_lower or "imu" in log_lower or "gyro" in log_lower or "accel" in log_lower:
            return "imu"
        if "pir" in log_lower or "motion" in log_lower:
            return "motion"
        if "rtc" in log_lower or "ds1307" in log_lower or "ds3231" in log_lower:
            return "rtc"
        if "hx711" in log_lower or "load_cell" in log_lower or "scale" in log_lower:
            return "load_cell"
        if "tft" in log_lower or "ili9341" in log_lower:
            return "tft"
        return None

    def is_existing_peripheral_plugin(self, peripheral_type: str) -> bool:
        """Check if peripheral plugin already has a built manifest (Branch D1.A)."""
        manifest_p = (
            self.ws_root
            / "wink-plugin-peripherals"
            / "builtin"
            / peripheral_type
            / "1.0.0"
            / "dist"
            / "manifest.json"
        )
        return manifest_p.is_file()
