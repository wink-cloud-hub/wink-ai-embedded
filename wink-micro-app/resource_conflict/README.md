# resource_conflict sample（反例样本）

## 目的

**验证 pal_resource 冲突治理在 DAL 层真正生效**——AI codegen 生成的错误
设备树（两个逻辑设备抢占同一物理资源）能在 `dal_*_init` 阶段就被
`pal_resource_claim` 拦截并返回 `WINK_ERR_BUSY`，而不是到真机上电后
才因电气冲突损坏硬件。

这是 Track A（M1）的**红线反例**：如果本 sample 有任一 case 断言失败
（第二个 init 返回 WINK_OK 而非 WINK_ERR_BUSY），则说明该 DAL 未正确
接线 `pal_resource_claim`，Track A 的验收出口未通过。

## 与其他 sample 的关系

| Sample | 立场 | 目的 |
|--------|------|------|
| `samples/devkitc_smoke` | 正例 | 正确设备树，硬件全链路 smoke |
| `samples/resource_conflict`（本样本） | **反例** | 故意错配，验证冲突治理生效 |

## 覆盖的冲突类型

样本一次跑完 4 种资源类型的冲突：

1. **GPIO 引脚冲突** — 两个 `dal_led` 抢同 pin
2. **PWM 通道冲突** — 两个 `dal_servo` 抢同 channel
3. **UART 端口冲突** — 两个 `dal_gps` 抢同 port
4. **I2C 地址冲突** — 两个 `dal_eeprom` 抢同 `(port, addr)`

对每一类：第一个 `dal_*_init` 应返 `WINK_OK`，第二个应返 `WINK_ERR_BUSY`。

## 运行

```powershell
# host 构建（由 python wink-tools/wink.py test 覆盖）
powershell python wink-tools/wink.py test
# 会构建并运行 sample_resource_conflict 目标；exit 0 = 反例通过
```

预期输出：

```
=== resource_conflict sample: verifying pal_resource conflict wiring ===
[resource_conflict] GPIO pin 2 conflict correctly rejected
[resource_conflict] PWM channel 0 conflict correctly rejected
[resource_conflict] UART port 1 conflict correctly rejected
[resource_conflict] I2C (port=0, addr=0x50) conflict correctly rejected
SAMPLE PASS: all 4 resource conflicts correctly rejected at dal_*_init
```

若任一 case 失败（例如某个 DAL 未接线 claim），样本会 `printf` 具体故障并
以 exit code 1 退出，CI 会红。

## 与 `test/test_pal_resource_wire.c` 的分工

`test_pal_resource_wire.c` 是 Unity 单元测试，覆盖 11 个精细 case（跨 DAL
冲突、rollback 正确性、owner=NULL 契约、release-then-reclaim 等）。

本 sample 是 developer/AI-facing 的**演示性反例**——用 `printf` 输出人类可读的
冲突场景，帮助 codegen 和用户直观理解"设备树错配时会发生什么"。二者互补，不重叠。

## 关联文档

- [Track A M1 实施计划](../../../docs/design/implementation-plans/2026-07-01-wmos-code-optimization-q3-plan.md) §3.1.1 / §6 Task A-3
- [PAL 平台抽象规范 §4.1](../../../docs/design/02-wink-micro-os/02-pal-platform-abstraction.md) —— DAL-SSOT 边界
