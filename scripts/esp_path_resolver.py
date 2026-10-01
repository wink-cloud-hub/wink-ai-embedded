#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""
scripts/esp_path_resolver.py — Bridge to .governance/tools/esp_path_resolver.py
"""
import importlib.util
import sys
from pathlib import Path

_GOV_RESOLVER = (
    Path(__file__).resolve().parent.parent
    / "wink-micro-app"
    / "vendor"
    / "esp_idfv61"
    / ".governance"
    / "tools"
    / "esp_path_resolver.py"
)

if not _GOV_RESOLVER.is_file():
    raise FileNotFoundError(f"Cannot find canonical esp_path_resolver at {_GOV_RESOLVER}")

_spec = importlib.util.spec_from_file_location("_esp_path_resolver_canonical", _GOV_RESOLVER)
if _spec is None or _spec.loader is None:
    raise ImportError(f"Cannot create spec for {_GOV_RESOLVER}")

_mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_mod)

# Re-export canonical functions
get_idf_root = _mod.get_idf_root
get_idf_examples_dir = _mod.get_idf_examples_dir
get_idf_components_dir = _mod.get_idf_components_dir
get_idf_version = _mod.get_idf_version
find_example = _mod.find_example
find_component = _mod.find_component
add_idf_cli_arguments = _mod.add_idf_cli_arguments
save_local_config = _mod.save_local_config
load_local_config = _mod.load_local_config
main = _mod.main

if __name__ == "__main__":
    sys.exit(main())
