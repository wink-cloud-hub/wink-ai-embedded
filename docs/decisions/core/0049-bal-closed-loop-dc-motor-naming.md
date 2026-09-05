# ADR-0049：BAL 闭环能力正名为 `wink_closed_loop_dc_motor`

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-28 |
| 触发 | ADR-0048 将 DAL `dal_motor` 正名为 `dal_dc_motor` 后，BAL `wink_closed_loop_motor` 仍为泛称 `motor`，与实际仅绑定 `dal_dc_motor` + `dal_encoder` 的实现不一致，易误导 AI/codegen |
| 影响范围 | `bal/include|src/control/wink_closed_loop_*.{h,c}`；`wink_chassis` 配置类型；对应 host 单测 / CMake；[06-bal-layer.md](../../zh/design/02-wink-micro-os/06-bal-layer.md) |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0037](0037-bal-domain-partition-and-closed-loop-motor.md)（闭环安全仍有效）；[ADR-0038](0038-bal-naming-hard-cut-and-layer-ssot.md)（命名 SSOT；本 ADR **部分更新**其映射表中「保留 `closed_loop_motor`」行）；[ADR-0048](0048-actuator-control-semantic-naming.md)（DAL 控制语义） |
| 关联活规范（SSOT） | [06-bal-layer.md](../../zh/design/02-wink-micro-os/06-bal-layer.md) |

---

## 背景（Context）

1. `wink_closed_loop_motor` 的公共 API 句柄为 `dal_dc_motor_t*`，输出为占空比、关断走 `dal_dc_motor_safe_off`→brake——**不是**通用电机闭环门面。
2. ADR-0048 已禁止泛称 `motor` 作为具体 DAL 前缀；Capability 别名层可继续用业务名（如 `left_wheel_set_speed`）。
3. 若保留 BAL 泛称，会复现当年 `dal_motor` 名不副实问题，只是上移到 control 层。

## 方案比选（Options）

| 方案 | 结论 |
|------|------|
| A. 保留 `wink_closed_loop_motor`，仅文档注明 DC | ❌ 符号面仍误导 AI |
| B. 一套通用 BAL control + 内层多 DAL | ❌ 与 ADR-0004 静态分发、ADR-0048 控制语义正交冲突 |
| **C. 硬切割改名为 `wink_closed_loop_dc_motor`** | ✅ **采纳** |

## 决策结论（Decision）

1. **硬切割改名**（不保留 deprecated 双名）：

| 旧 | 新 |
|---|---|
| `wink_closed_loop_motor.h/.c` | `wink_closed_loop_dc_motor.h/.c` |
| `wink_closed_loop_motor_*` | `wink_closed_loop_dc_motor_*` |
| `wink_closed_loop_motor_config_t` | `wink_closed_loop_dc_motor_config_t` |
| `WINK_CLOSED_LOOP_MOTOR_MAX` | `WINK_CLOSED_LOOP_DC_MOTOR_MAX` |
| `test_bal_closed_loop_motor` | `test_bal_closed_loop_dc_motor` |

2. **`wink_chassis` 保留领域名**；其配置内嵌类型改为 `wink_closed_loop_dc_motor_config_t`，调用改为 `wink_closed_loop_dc_motor_*`。头注释须标明「当前后端仅为 DC + encoder」。
3. **不引入**跨 DAL 的万能 `wink_closed_loop_motor` 门面；步进 / FOC / 总线伺服另开 control 组件。
4. **词表**：活规范器件词与 DAL 对齐时，用 `dc_motor` 替代泛称 `motor`（Capability 别名层仍可用业务名）。

## 后果与约束（Consequences）

| 正面 | 代价 |
|------|------|
| BAL 符号与 DAL 控制语义、句柄类型一致 | 破坏性 rename（BAL + 单测 + 活规范） |
| 降低 AI 误选「任意电机闭环」 | 历史计划/评审文档中的旧名保留为快照 |

## 遵循与后续（Compliance & Follow-up）

Accepted 后必须：

- [x] 回写 [06-bal-layer.md](../../zh/design/02-wink-micro-os/06-bal-layer.md) 示例与目标态清单
- [x] 更新 ADR-0038 映射表「`closed_loop_motor` 保留」行 → 指向本 ADR
- [x] 代码 / 测试 / CMake 硬切割改名
- [x] `python wink-tools/wink.py test`：`test_bal_closed_loop_dc_motor` / `test_bal_chassis` 全绿（2026-07-28；整仓 62/62 host PASS）

---

*状态变更记录：*
- 2026-07-28：Accepted（Owner 确认与 ADR-0048 对齐正名）

