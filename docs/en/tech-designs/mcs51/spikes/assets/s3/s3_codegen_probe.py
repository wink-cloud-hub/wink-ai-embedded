#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Spike-S3 PoC (throwaway): prove the EXISTING unisim runtime device-tree
# emitter (wink-tools/tools/frontend/runtime_device_tree.py) flattens 8051
# P0.0~P3.7 -> pin index 0..31 and accepts an adc0832 + thermal_heater_plate
# element with ZERO emitter changes. No wink-tools file is modified.
import json
import sys
from pathlib import Path

WTOOLS = Path(r"D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai\packages\wink-tools")
sys.path.insert(0, str(WTOOLS / "tools"))

from frontend.runtime_device_tree import build_runtime_device_tree  # noqa: E402

ASSETS = Path(__file__).resolve().parent

# Synthetic manifest index — same shape as a scanned dist/manifest.json:
# {"version":1,"by_type":{type:{"pins":[...],"properties":[...]}}}
MANIFEST_INDEX = {
    "version": 1,
    "by_type": {
        "adc0832": {
            "pins": [
                {"name": "CS",  "direction": "sink",   "signal": "digital", "role": "cs",  "aliases": ["cs",  "nss"],  "required": True},
                {"name": "CLK", "direction": "sink",   "signal": "digital", "role": "clk", "aliases": ["clk", "sclk"], "required": True},
                {"name": "DIO", "direction": "bidir",  "signal": "digital", "role": "dio", "aliases": ["dio", "mosi", "miso", "data"], "required": True},
                {"name": "VCC", "pinType": "vcc", "role": "power",  "aliases": ["vcc"], "required": False},
                {"name": "GND", "pinType": "gnd", "role": "ground", "aliases": ["gnd"], "required": False},
            ],
            # property keys are camelCase after snake->camel conversion
            "properties": ["channel", "vrefMv"],
        },
        "thermal_heater_plate": {
            "pins": [
                {"name": "DRIVE", "direction": "sink", "signal": "digital", "role": "drive",
                 "aliases": ["drive", "heat", "pwm", "gate"], "required": True},
                {"name": "VCC", "pinType": "vcc", "role": "power",  "aliases": ["vcc"], "required": False},
                {"name": "GND", "pinType": "gnd", "role": "ground", "aliases": ["gnd"], "required": False},
            ],
            # ntcChannel binds the heater to the ADC channel that reads its NTC;
            # thermal constants are simulation-side properties (no MCU pin).
            "properties": ["ntcChannel", "setpointC", "thermalTauS", "heaterWatts", "ntcBeta", "ntcR25Ohm"],
        },
        "led": {
            "pins": [
                {"name": "GPIO", "direction": "sink", "signal": "digital", "role": "gpio",
                 "aliases": ["gpio", "sig", "anode"], "required": True},
            ],
            "properties": ["activeHigh"],
        },
    },
    "i2c_role": {},
}


def main() -> int:
    app = json.loads((ASSETS / "wink_app_iron_ntc.json").read_text(encoding="utf-8"))
    tree = build_runtime_device_tree(
        app,
        boards_dir=ASSETS / "boards",
        manifest_index=MANIFEST_INDEX,
    )
    out = ASSETS / "device-tree.iron_ntc.json"
    out.write_text(json.dumps(tree, indent=2, ensure_ascii=False), encoding="utf-8")
    print("=== device-tree.json (emitted by UNMODIFIED emitter) ===")
    print(json.dumps(tree, indent=2, ensure_ascii=False))

    # Assertions: pin flattening 0..31 + thermal property passthrough.
    fails = []
    adc = tree["devices"]["temp_adc"]["pinMapping"]
    if adc != {"CS": 16, "CLK": 17, "DIO": 18}:
        fails.append(f"adc0832 pinMapping want CS/CLK/DIO=16/17/18 got {adc}")
    heat = tree["devices"]["heater"]
    if heat["pinMapping"].get("DRIVE") != 8:
        fails.append(f"heater DRIVE want 8 (P1.0) got {heat['pinMapping']}")
    props = heat["properties"]
    for k, want in (("ntcChannel", 0), ("setpointC", 180.0), ("thermalTauS", 8.0),
                    ("heaterWatts", 40.0), ("ntcBeta", 3950), ("ntcR25Ohm", 100000)):
        if props.get(k) != want:
            fails.append(f"heater property {k} want {want} got {props.get(k)}")
    # Every flattened pin must be in 0..31 (8051 4-port space).
    for d in tree["devices"].values():
        for pin, idx in d.get("pinMapping", {}).items():
            if not (0 <= idx <= 31):
                fails.append(f"pin {pin}={idx} outside 0..31")

    if fails:
        print("\n[s3] FAIL:")
        for f in fails:
            print("  -", f)
        return 1
    print("\n[s3] PASS: existing emitter flattens P0.0~P3.7 -> 0..31 and passes")
    print("        adc0832 + thermal_heater_plate schema with ZERO emitter change.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
