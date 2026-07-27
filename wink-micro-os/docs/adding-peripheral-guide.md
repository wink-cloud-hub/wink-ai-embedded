# 嵌入式端新增外设指南（ADR-0046）

与仿真侧 [ADDING_PERIPHERAL.md](file:///d:/workspaces/ai-coding/wink-ai/wink-ai/packages/unisim/docs/ADDING_PERIPHERAL.md) 配套。设备 `type` 字符串**两侧必须完全一致**。

设计依据：[tech-design 2026-07-27](../../docs/design/tech-designs/2026-07-27-peripheral-onboarding-optimization-design.md)、[ADR-0046](../../docs/design/decisions/0046-dal-driver-registry-ssot.md)。

---

## 1. 架构与 SSOT

```text
wink.py new-dal → dal_*.{h,c} + drivers/<type>.py
                         ↓
              DriverBase registry（自动扫描）
                         ↓
         list_drivers.py --cmake --mode=source|defs
                         ↓
         CMake 三入口 foreach（零手改驱动表）
```

**SSOT** = `wink-micro-os/tools/codegen/drivers/*.py`。不要再改 `dal/CMakeLists.txt` / `wink_dal_drivers.cmake` / Binary SDK / `ALL_WINK_USE_OPTIONS` 中的驱动枚举。

---

## 2. `wink.py new-dal`

```text
python wink-micro-os/tools/wink.py new-dal <type> \
  --category <input|output|actuator|sensor|display|communication|storage> \
  [--actuator] [--role <name>] [--pin-field <name>]... [--force]
```

生成：

- `dal/include/<cat>/dal_<type>.h`
- `dal/src/<cat>/dal_<type>.c`
- `tools/codegen/drivers/<type>.py`

`--category` 必须是既有 `DriverCategory`；不能发明新目录。

---

## 3. DAL 实现约定

- POD + named API（ADR-0004）；`wink_status_t` 负数错误码（ADR-0001）
- 双 target 同源（ADR-0002）
- 引脚字段优先 `*_pin` 后缀
- `pal_resource_claim` 失败走 `goto err_release` 释放链
- 非阻塞：`WINK_ASSERT_NONBLOCKING`（ADR-0017）
- 头文件底部保留 `WINK_UNAVAILABLE_MSG` stub（裁剪友好错误）

---

## 4. Codegen 插件

必填：`type`、`category`、`required_fields`、`get_headers` / `get_device_type` / `render_config_init` / `render_deinit`。

执行器：`is_actuator=True`，覆盖 `get_safe_off_fn()`（默认 `dal_<type>_off`）。

可选 Role：`default_role` / `role_verbs` / `render_role_wrapper`。

多 TU：手写 `extra_cmake_defs` + `extra_cmake_sources`（见设计 §3.2.1）；`new-dal` 不生成这两项。

---

## 5. `advanced.*`（ADR-0034）

高级配置只放 `advanced.*`；禁止 top-level alias 双写。用 `drivers/advanced.py` 的 `parse_advanced()`。

---

## 6. I2C / board / 引脚

Codegen 会做引脚冲突、I2C 总线分组、`$board.` / `use_onboard` 解析。新增 I2C 或板载外设时在 `wink-app.json` 的 `board` 中说明联动。

---

## 7. 多 TU 与 `--mode`

| 字段 | Host / ESP32（`--mode=source`） | Binary SDK（`--mode=defs`） |
|------|----------------------------------|------------------------------|
| `extra_cmake_defs` | 是 | 是 |
| `extra_cmake_sources` | 是 | **否** |

调用方在 `set(WINK_DAL_TARGET …)` 后执行 `wink_dal_apply_extra_cmake()`。

---

## 8. 门禁

```text
python wink-micro-os/tools/wink.py lint --pack drivers --pack layering --pack api
python wink-micro-os/tools/codegen/list_drivers.py --check
python wink-micro-os/tools/codegen/list_drivers.py --json   # 可选跨仓对照
```

---

## 9. unisim 对齐

1. 按 ADDING_PERIPHERAL.md 加 Manifest/模型  
2. `devices[].type` 与嵌入式 `type` 字节级一致  
3. 同源 `wink-app.json` 两侧可识别  
4. （可选）`--json` 与 unisim 已知 type 集交叉校验  

---

## 10. 边界（本指南不做）

- BAL stub / 算法联调 → BAL / ADR-0037 等  
- 扩展 `DriverCategory` → 改枚举 + Host `target_include_directories`  
- 仿真观测面 / 新 Wasm import → 仿真专题  

---

## 验收清单

- [ ] `new-dal` 三文件路径正确  
- [ ] 有 JSON：仅声明驱动 ON；无 JSON：全开 + ADR-0039 WARNING  
- [ ] 执行器 safe-off 已注册（若 `--actuator`）  
- [ ] `type` 与 unisim 一致  
- [ ] 多 TU：`extra_cmake_*` + Binary 仅 defs  
- [ ] `lint --pack drivers` 绿  
