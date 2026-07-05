# tools/codegen — wink-app.json → C/CMake glue generator

Reads a `wink-app.json` device tree spec and emits four files into an output
directory, ready to be built alongside a sample's hand-written
`app_callbacks.c`:

| File | Purpose |
|------|---------|
| `device_tree.h` | `extern` handles + init/deinit prototypes |
| `device_tree.c` | Static device instances, safe-off thunks, init/deinit sequences |
| `app_support.c` | Service-start glue (button polling, telemetry, …) |
| `app_options.cmake` | `WINK_USE_XXX` flags forced ON per driver used |

## Run

```bash
python tools/codegen/app_codegen.py \
    --config wink-micro-os/samples/devkitc_smoke/wink-app.json \
    --out-dir wink-micro-os/samples/devkitc_smoke/build/generated
```

Exit codes: `0` success, `2` schema/validation error, `1` other errors.

## Add a device type

Drop a plugin at `tools/codegen/drivers/<type>.py` that subclasses
`DriverBase` (see `drivers/base.py`). At minimum override `type`,
`is_actuator`, `required_fields`, `get_headers`, `get_device_type`,
`render_config_init`, and `render_deinit`. Optional hooks let a driver
inject post-init calls, service-start lines, and extra headers. Discovery
is automatic — subclassing registers the plugin.

## Golden tests

```bash
python -m tools.codegen.tests.test_golden
```

The test renders `tests/golden_devkitc.json` to a temp dir and diffs every
generated file against `tests/golden_expected/`. Missing expected files
fail the test — regenerate them explicitly via the hidden `--regen-golden`
flag (populated in P1-2 once driver plugins land).

## Constraints (per tech-design)

- Stdlib + Jinja2 only (no pydantic, no schema libs).
- `app_codegen.py` stays under 300 lines; `drivers/base.py` under 80.
- Actuator safe-off registration happens **after** every device init
  succeeds, in **reverse** device order (safe-off pop = LIFO in
  fault-flow). Unregister runs in forward init order — the exact inverse.
- Depends-on cycles and missing refs are hard errors (exit 2).
