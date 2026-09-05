# DAL `variant` Wave B — HAS_* DCE / UniSim / SH1106（stub）

> **状态**：📝 Stub — Wave A 完成后跟踪  
> **关联**：`2026-07-30-dal-variant-unified-field-plan.md` §2.2；设计 `2026-07-30-dal-variant-unified-field-design.md`

## 范围（本计划不做 Wave A 已完成的 rename）

| 项 | 说明 |
|----|------|
| `WINK_<TYPE>_HAS_<VALUE>` 并集裁剪 + init `switch` 重构 | 设计 §4.2；现网仍是 `!= default → UNSUPPORTED` |
| UniSim / 前端消费 `affects_pins` / props.`variant` | 嵌仓 SSOT 已先发（设计 Q5） |
| `phase_enable` / `pwm_on_in` / encoder x2/x4 / SH1106 **真路径** | 仍 reserved → 实现时另开实施计划 |
| YAML schema 白名单 `affects_pins` | 防 typo 静默忽略 |

## 前置

Wave A exit gate 全绿；设计 Accepted（Wave A）。
