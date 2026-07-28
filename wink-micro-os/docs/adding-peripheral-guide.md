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

**SSOT** = `wink-tools/tools/codegen/drivers/*.py`。`list_drivers` / `boards/` / 生成横幅路径均相对 codegen 包目录锚定，不依赖 cwd。不要再改 `dal/CMakeLists.txt` / `wink_dal_drivers.cmake` / Binary SDK / `ALL_WINK_USE_OPTIONS` 中的驱动枚举。

---

## 2. `wink.py new-dal`

```text
python wink-tools/wink.py new-dal <type> \
  --category <input|output|actuator|sensor|display|communication|storage> \
  [--actuator] [--role <name>] [--pin-field <name>]... [--force]
```

产出：

| 文件 | 位置 |
|------|------|
| DAL 头 | `wink-micro-os/dal/include/<category>/dal_<type>.h` |
| DAL 源 | `wink-micro-os/dal/src/<category>/dal_<type>.c` |
| Codegen 插件 | `wink-tools/tools/codegen/drivers/<type>.py` |

填完实现后重新 configure CMake，`WINK_KNOWN_DRIVERS` 会自动包含新驱动。

---

## 3. 验收清单

```text
python wink-tools/wink.py lint --pack drivers --pack layering --pack api
python wink-tools/tools/codegen/list_drivers.py --check
python wink-tools/tools/codegen/list_drivers.py --json   # 可选跨仓对照
python wink-tools/wink.py build host --app <your_app>
```

Host 构建至少一次带 `wink-app.json` 裁剪、一次不带 JSON（全量驱动）以覆盖 stub 路径。

---

## 4. Unisim 对齐（文档级）

仿真侧 Manifest 的 `type` 字符串必须与 codegen `DriverBase.type` **逐字一致**。不强制跨仓 CI；嵌入式侧以 `list_drivers.py --json` 作为对照源。

---

## 5. 路径锚点（拆仓后）

| 变量 / 路径 | 含义 |
|-------------|------|
| `WINK_TOOLS_ROOT` | `…/wink-tools`（CLI / codegen / lint） |
| `WINK_MICRO_OS_ROOT` | `…/wink-micro-os`（C 运行时） |
| 入口 | `python wink-tools/wink.py …` |
