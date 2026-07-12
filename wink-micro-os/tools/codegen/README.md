# tools/codegen — wink-app.json → C/CMake glue generator

Reads a `wink-app.json` device tree spec and emits four files into an output
directory, ready to be built alongside a sample's hand-written
`app_callbacks.c`:

| File | Purpose |
|------|---------|
| `device_tree.h` | `extern` handles + init/deinit prototypes |
| `device_tree.c` | Static device instances, safe-off thunks, init/deinit sequences |
| `device_tree_api.md` | Auto-generated Markdown API specification document |
| `app_options.cmake` | `WINK_USE_XXX` flags forced ON per driver used |

Sibling generators in this directory:

| File | Purpose |
|------|---------|
| `config_h.py` | `wink_app.json` → `wink_config.h` (tick / timers SSOT) |
| `pt_state.py` | Protothread `WINK_PT_STATE` business-logic helper |

## Run

```bash
python wink-micro-os/tools/wink.py gen --app devkitc_smoke
```

Or directly:

```bash
python wink-micro-os/tools/codegen/app_codegen.py \
    --config wink-micro-app/devkitc_smoke/wink-app.json \
    --out-dir build/generated
```

Exit codes: `0` success, `2` schema/validation error, `1` other errors.

## Add a device type

Drop a plugin at `wink-micro-os/tools/codegen/drivers/<type>.py` that subclasses
`DriverBase` (see `drivers/base.py`). Discovery is automatic — subclassing
registers the plugin.

## Golden tests

```bash
$env:PYTHONPATH = "wink-micro-os"
python wink-micro-os/tools/codegen/tests/test_golden.py
```

## Constraints (per tech-design)

- Stdlib + Jinja2 only (no pydantic, no schema libs).
- `app_codegen.py` stays under 300 lines; `drivers/base.py` under 80.
- Actuator safe-off registration happens **after** every device init
  succeeds, in **reverse** device order (safe-off pop = LIFO in
  fault-flow). Unregister runs in forward init order — the exact inverse.
- Depends-on cycles and missing refs are hard errors (exit 2).
