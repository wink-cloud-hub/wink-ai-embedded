# Closed-source `wink-tools` release checklist

Owner sign-off gate before shipping a closed-source or restricted `wink-tools` build.
Official driver/role **descriptions** stay in this tree (`wink-micro-os/codegen/`); the
engine must not require users to edit tools source to add a DAL type.

**Sign-off:** ☐ Owner name / date: _______________

---

## 1. Engine security (build-time)

| # | Item | Verify |
|---|------|--------|
| 1.1 | **Jinja2 sandbox** | All extension-root template rendering uses `jinja2.sandbox.SandboxedEnvironment` + whitelisted context (`yaml_render.py`, `app_codegen.py`). |
| 1.2 | **Jinja2 pin** | Dependency documents `jinja2>=3.1.4` ([`wink-tools/preinstall.md`](../../wink-tools/preinstall.md)); CI/host env matches. |
| 1.3 | **Template path jail** | `*_template_file` paths are extension-root-relative only; reject `..` and absolute paths (lint + loader). |
| 1.4 | **Hooks default OFF** | No user hook import/load in release build; `ENABLE_USER_HOOKS = False` ([`HOOKS.md`](../../wink-tools/tools/codegen/HOOKS.md)). |
| 1.5 | **Sandbox tests** | `pytest wink-tools/tools/codegen/tests/test_jinja_sandbox.py` PASS. |

## 2. Build correctness

| # | Item | Verify |
|---|------|--------|
| 2.1 | **`CMAKE_CONFIGURE_DEPENDS`** | OS/env extension YAML + referenced templates registered (`wink_dal_drivers.cmake`, `dal/CMakeLists.txt`). |
| 2.2 | **Cache truth** | `WINK_CODEGEN_PATHS` is CMake **CACHE STRING**; process env is CLI convenience only. |
| 2.3 | **Scan order** | builtin → os → env → app; `--check` logs winning path per type. |

## 3. Description SSOT (not in tools)

| # | Item | Verify |
|---|------|--------|
| 3.1 | **Official YAML SSOT** | Migrated types defined only under `wink-micro-os/codegen/drivers/*.yaml` (see [README §Migrated](README.md#migrated-os-yaml-drivers)). |
| 3.2 | **Role contracts** | Standard roles under `codegen/roles/*.yaml`; drivers use `role_bindings`. |
| 3.3 | **No user-writable tools SSOT** | Adding a production type does **not** require PR to `wink-tools/tools/codegen/drivers/*.py`. |
| 3.4 | **Builtin exception list** | Types still on `.py` plugins documented in [README §Builtin exceptions](README.md#builtin-python-exceptions-p3-exit--owner-sign-off-list); no competing OS YAML for same `type`. |
| 3.5 | **`new-dal` target** | Scaffold writes OS codegen YAML (+ templates), not tools tree plugins. |

## 4. Tools tree inventory (may shrink)

| # | Item | Verify |
|---|------|--------|
| 4.1 | **`drivers/*.py`** | Empty, examples-only, or **exception-list** builtins only — not the primary SSOT for migrated types. |
| 4.2 | **Dual-read exit plan** | Migrated types use `register: False` on legacy plugins or remove after golden parity. |

## 5. Quality gates

| # | Item | Verify |
|---|------|--------|
| 5.1 | **Codegen tests** | `pytest wink-tools/tools/codegen/tests -q` PASS. |
| 5.2 | **Lint** | `python wink-tools/wink.py lint --pack drivers --pack user_surface` no new errors on official tree. |
| 5.3 | **Host sample** | At least one host sample configure + build succeeds with YAML-only official drivers. |

## 6. Documentation

| # | Item | Verify |
|---|------|--------|
| 6.1 | **User-facing README** | [`codegen/README.md`](README.md) scan order, schema, safety lint, exception list current. |
| 6.2 | **Hooks design** | [`HOOKS.md`](../../wink-tools/tools/codegen/HOOKS.md) present; default OFF stated. |
| 6.3 | **ADR-0051** | Accepted; implementation plan T13 complete pending this checklist. |

---

*Checklist version: 2026-07-29 (Task T13 / PLAN-20260729-CODEGEN-EXT-ROOTS).*
