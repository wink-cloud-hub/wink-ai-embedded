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

Target set for YAML migration: `led` / `button` / `ultrasonic` / `ssd1306` /
`rc_servo` (+ Phase 1 role types). Types below remain **tools builtin plugins**
until Owner signs off a migration. Do **not** add a competing OS YAML while
they stay registered in `wink-tools/tools/codegen/drivers/*.py`.

| type | Status | Rationale (why not OS YAML yet) |
|------|--------|----------------------------------|
| `button` | **Exception — pending Owner sign-off** | Cross-field (`event_drive`↔`auto_poll_ms`/`gpio_pin`) + nested `advanced.pull` enum→C + conditional BAL event wrappers + post-init; exceeds declarative constraints without advanced.* maps |
| `rc_servo` | **Exception — pending Owner sign-off** | Nested `advanced.resolution_bits` / `clock_requirement` + `max_angle` 0-sentinel warning; conditional designated initializers |
| `dc_motor` | Exception (out of T10 target migrate set) | Multi-mode actuator semantics |
| `encoder` | Exception (out of T10 target migrate set) | Decode-mode / coupling complexity |
| `gps` / `eeprom` | Exception | Experimental or niche stubs (review before migrate) |

**Migrated (OS YAML SSOT):** `led`, `ultrasonic`, `ssd1306`.

> **Owner action:** Confirm `button` / `rc_servo` remain on this exception list
> for P3 exit, or authorize a follow-up task to extend constraints for
> `advanced.*` and migrate them. This table is the provisional sign-off surface.

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


## Schema

Each driver YAML includes `codegen_schema: 1`. The engine supports schema
version `1`. Schema `0` loads with a warning (N−1 compatibility). Newer or
older unsupported versions fail closed.

See the tech design for the full field mapping:
[scannable-codegen-extension-roots-design.md](../../docs/design/tech-designs/2026-07-28-scannable-codegen-extension-roots-design.md).

## Adding a driver

1. Implement DAL C sources under `wink-micro-os/dal/`.
2. Add `codegen/drivers/<type>.yaml` with `config`, `constraints`, and optional
   `role_bindings` (cover `roles/<role>.yaml` verbs or declare subset).
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
