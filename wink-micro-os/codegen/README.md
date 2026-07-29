# WinkMicroOS Codegen Extension Root

User-editable driver and role descriptions for `wink-tools` codegen. The tools
engine scans these YAML files read-only; you do not need to modify closed-source
tools to add or override a DAL type.

## Layout

```text
codegen/
  drivers/           # MVP: flat drivers/<type>.yaml only
  roles/             # Role contract YAML (verbs + error_class)
  README.md          # this file
```

Place one file per driver: `drivers/<type>.yaml`. The `type:` field in the file
**must** match the filename stem (`ultrasonic.yaml` → `type: ultrasonic`).

Optional templates live beside drivers, e.g.
`drivers/templates/ultrasonic_init.c.j2`, referenced via `init_template_file`.

Role contracts live under `roles/<role>.yaml`. The `id:` field **must** match
the filename stem. Driver `role_bindings` must cover contract verbs
(`covers_contract: full`, default) or declare `covers_contract: subset`.
**Do not** embed DAL type templates inside shared role contracts — put C
wrappers in driver `role_bindings` (required for multi-type roles).

## Safety lint (`wink lint --pack drivers`, Task T11)

These rules are **warning** on first landing (exit 0 unless `--strict`):

| Rule id | Check |
|---------|--------|
| `drivers.error_class_signature` | `normal`/`fatal` → `WINK_WARN_UNUSED_RESULT` + `wink_status_t`; `fire_and_forget` → `void`; `convenience` unrestricted |
| `drivers.init_status_propagation` | `init_template*` must `WINK_TRY(dal_*_init…)` (no silent discard) |
| `drivers.isr_safe_template` | `isr_safe: true` verb templates must not call sleep/mutex/I2C/blocking helpers (heuristic) |

**Promote warn → error when:** official OS `codegen/` stays clean for a full
migration window **and** Owner signs off at P3 exit. Until then keep severity
at warn so experimental App overrides can iterate without blocking CI.

## Migrated OS YAML drivers

These types are defined only under this extension root (builtin `.py` plugins
are unregistered / fallback-only for comparison tests):

| type | YAML |
|------|------|
| `led` | `drivers/led.yaml` |
| `ultrasonic` | `drivers/ultrasonic.yaml` |
| `ssd1306` | `drivers/ssd1306.yaml` |
| `rc_servo` | `drivers/rc_servo.yaml` |
| `button` | `drivers/button.yaml` |
| `dc_motor` | `drivers/dc_motor.yaml` |
| `encoder` | `drivers/encoder.yaml` |
| `gps` | `drivers/gps.yaml` |
| `eeprom` | `drivers/eeprom.yaml` |

## Role contracts

| role | YAML |
|------|------|
| `binary_indicator` | `roles/binary_indicator.yaml` |
| `binary_sensor` | `roles/binary_sensor.yaml` |
| `distance_sensor` | `roles/distance_sensor.yaml` |
| `text_display` | `roles/text_display.yaml` |
| `angular_actuator` | `roles/angular_actuator.yaml` |
| `open_loop_actuator` | `roles/open_loop_actuator.yaml` |
| `pulse_counter` | `roles/pulse_counter.yaml` |

## Builtin Python exceptions (P3 exit — Owner sign-off list)

All nine official driver types are OS YAML SSOT. Python plugins under
`wink-tools/tools/codegen/drivers/*.py` remain importable with `register = False`
for golden / comparison tests only.

**No types remain on the exception list** (see `EXCEPTIONS.md` if re-opened).

Previously listed before T11: `button`, `rc_servo`, `dc_motor`, `encoder`, `gps`, `eeprom`.

## Scan order (later wins)

When multiple extension roots are configured, merge order is:

1. **builtin** — tools built-in inventory (if any)
2. **os** — this tree (`wink-micro-os/codegen/`)
3. **env** — paths from CMake cache `WINK_CODEGEN_PATHS` (declaration order)
4. **app** — `wink-micro-app/<app>/codegen/` (highest priority)

**Build truth:** `WINK_CODEGEN_PATHS` is a CMake **CACHE STRING**
(`-DWINK_CODEGEN_PATHS=...`). Configure always passes the cache into
`list_drivers --codegen-paths=...`. The process environment variable of the
same name is a **CLI convenience only** and must not be treated as an implicit
configure input.

Same `type` in a later root replaces the earlier definition entirely (no deep
merge in MVP).


## Schema 1.1

Each driver YAML declares `codegen_schema: "1.1"`. Schema `1` / `"1.0"` loads with a
warning (N−1 compatibility window). Unsupported versions fail closed.

Full field mapping:
[scannable-codegen-extension-roots-design.md §4](../../docs/design/tech-designs/2026-07-28-scannable-codegen-extension-roots-design.md).

### 必填 / 可省 / 何时手写

| 类别 | 内容 |
|------|------|
| **必填** | `codegen_schema`, `type`, `experimental`, `fields`（每项含 `tier` + `type`；有歧义时加 `c` / `emit` / `map`） |
| **能推则省** | `category`（唯一 `dal/include/<cat>/dal_<type>.h` 时可省略）、`is_actuator`、`config.*` 命名约定、1:1 `role_bindings`、标准 init 约定发射 |
| **何时手写** | stub 必填 `category`；非标 `safe_off_fn`；事件/解包类 role 动词；`build_variants` 或 escape `extra_cmake_*`；JSON 名 ≠ C 成员时 `fields.<n>.c`；既进 config 又发 macro/post_init 时用 `config_member: true` |

**禁止**在描述文件里写旧四表（`required_fields` / `stable_fields` /
`advanced_fields` / `constraints`）——引擎从 `fields:` **派生**这些视图供
`list_drivers` / `user_surface` / 约束求值使用。N−1 旧文件仍可加载并 **warn**；
下一 schema minor / 下一 tools release 起升 **error**（见 tech-design §3.3）。

**`config_member`：** 默认跟 `emit`——`emit: config` 进结构体，其它不进。若同一字段既要
`.member = …` 又要 `#define` / post_init（如 `use_rmt`、`i2c_addr`），写
`emit: macro`（或 `post_init`）并加 `config_member: true`。

### 渲染三态分发（T3a）

`render_strategy()` 取代旧二态「有模板 / 无模板→plugin」：

| 策略 | 条件 |
|------|------|
| `template_override` | 存在 `config.init_template` 或 `init_template_file` |
| `convention_emit` | 扩展根 YAML 获胜、无 init 模板、已物化 `config` |
| `plugin` | 仅 builtin Python 插件 |

**T3a 硬闸：** OS / env / app YAML 去掉 init 模板后**必须**走
`convention_emit`（或显式报错），**禁止**静默回退 `plugin`。否则已迁 YAML
驱动会在去模板时误触 `_require_plugin` 或破坏 SSOT。

## Adding a driver

1. Implement DAL C sources under `wink-micro-os/dal/`.
2. Add `codegen/drivers/<type>.yaml` with `codegen_schema: "1.1"`,
   `experimental`, `fields:`, and optional `role_bindings`. Stubs must set
   `category` explicitly.
3. Run `python wink-tools/wink.py list_drivers --check` to confirm discovery.

Detailed steps: [adding-peripheral.md](../docs/dal-development-guide/adding-peripheral.md).

## Python hooks (P4 — **default OFF**)

MVP and current releases are **YAML + Jinja templates only**. The engine does not
load user Python from extension roots (`ENABLE_USER_HOOKS = False`).

Future optional hooks (narrow API, `hooks_schema` version, root-relative
`module_file` only) are documented in
[`wink-tools/tools/codegen/HOOKS.md`](../../wink-tools/tools/codegen/HOOKS.md).
Hooks run as trusted build-time code — **not** sandboxed; Jinja templates remain
sandboxed regardless.

## Closed-source release

Before shipping a closed-source `wink-tools` build, Owner must sign off:
[`CLOSED_SOURCE_CHECKLIST.md`](CLOSED_SOURCE_CHECKLIST.md).

