# Stage7：测试与契约收尾（关闭项）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S7-CLOSE` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
| **优先级** | 🟡 P1 |
| **关联 CPL** | CPL-22（测试分层）、CPL-23（ABI 定稿） |
| **前置依赖** | stage1（双读/双轨已开）、stage6（目标已拆） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ 删除合成通道双读兼容层、转发 shim、旧单体别名，无告警残留。
- ✅ `SimTraceSpecV2` 与前端视窗 DTO 定稿，ABI 版本记录归档（含发版顺序与回滚步骤）。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `chips/cms8s78xx/src/cms8s_adc.cpp` 等兼容层 | 🗑️ | 删双读重定向 |
| `include/` 转发 shim | 🗑️ | 删 stage3 遗留 shim |
| `CMakeLists.txt` 别名 | 🗑️ | 删旧单体别名 |
| 契约文档 | ✏️ | 版本号定稿归档 |

## 3. 任务拆分

### Task S7-1：删兼容层 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：删片上合成通道重定向（板级 32+ch key 不受影响），合成通道访问改为硬 fail（或返回哨兵 + 计数，取其一写死）。
- [ ] **Step 2**：删转发 shim 与别名目标；全仓 grep 零 `TODO(stage7)`/`#warning` 残留。
- [ ] **Step 3**：`core-tests`/`cms8s-tests` 各自全绿后合入主线。

### Task S7-2：契约定稿 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：归档 ABI 版本（语义/版本号/发版顺序/回滚命令）。
- [ ] **Step 2**：`SimTraceSpecV2` + DTO 一致性复核签字。

## 4. 验收（系列关闭门）

- L0-L2：双轨全绿，E-02 无复发，绝缘单测全绿。E-02 无复发以跨仓真链路为终证（需 sister 仓检出＋已构建 WASM，手动）：
  `powershell -File wink-micro-os/frameworks/mcs51/tools/run_mcs51_headless_evidence.ps1`（单跑 `-App <name>`；host/wasm 单测全绿是必要非充分条件）。
- L3：契约文档与实现一致；总纲状态置 ✅ 已完成。
- L4：零兼容残留 grep 通过。

## 5. 风险与回滚

- R：删兼容层后旧前端版本断裂 → 缓解：发版顺序 + 版本号卡死，不兼容旧大版本属预期；回滚：`git revert <S7-commit>` 恢复兼容层（单独 commit，可独立回退）。

## 6. 阶段自审自我检验清单（Self-Audit Checkpoint）
- [ ] **目录落位**：全仓物理目录结构 100% 严丝合缝匹配 `00-README.md §3.2` 终态目录树。
- [ ] **零残留门禁**：全仓 `grep` 零 `#warning` 转发 shim、零旧 target 别名、零片上合成通道误用（板级 32+ch key 合法保留，见 stage1 分区；`TODO(stage7)` 同查）。
- [ ] **契约归档**：ABI 文档与 `SimTraceSpecV2` 复核签字，版本号定稿。
- [ ] **双轨与 CI**：host 与 wasm 双平台全绿，基线脚本 `--compare` 集合相等（计数以基线文件为准，不锁死 40/40 之类的过期数字），总纲置为 ✅ 已完成。
