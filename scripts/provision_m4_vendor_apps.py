#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import shutil
import hashlib
import json
from pathlib import Path

def normalize(text: str) -> str:
    text = text.replace('\r\n', '\n').replace('\r', '\n')
    return '\n'.join(line.rstrip() for line in text.split('\n')).rstrip('\n') + '\n'

def digest(path: Path) -> str:
    return hashlib.sha256(normalize(path.read_text(encoding='utf-8-sig')).encode('utf-8')).hexdigest()

idf_root = Path(r"D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf")
vendor_root = Path(r"wink-micro-app/vendor/esp_idfv61")

apps = [
    {
        "dir": "wifi_sta",
        "display_name": "Espressif ESP-IDF Wi-Fi station (STA connection)",
        "source_dir": "examples/wifi/getting_started/station/main",
        "files": ["station_example_main.c"],
        "sdkconfig": """#pragma once
#include "sdkconfig_base.h"
#define CONFIG_ESP_WIFI_SSID                     "myssid"
#define CONFIG_ESP_WIFI_PASSWORD                 "mypassword"
#define CONFIG_ESP_MAXIMUM_RETRY                 5
#define CONFIG_ESP_WIFI_AUTH_WPA2_PSK            1
#define CONFIG_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD 3
#define CONFIG_ESP_WPA3_SAE_PWE_BOTH             1
#define CONFIG_ESP_WIFI_PW_ID                    ""
""",
        "devices": {}
    },
    {
        "dir": "http_client",
        "display_name": "Espressif ESP-IDF HTTP client (REST GET/POST)",
        "source_dir": "examples/protocols/esp_http_client/main",
        "files": ["esp_http_client_example.c"],
        "sdkconfig": """#pragma once
#include "sdkconfig_base.h"
#define CONFIG_EXAMPLE_HTTP_ENDPOINT "http://httpbin.org/get"
""",
        "devices": {}
    },
    {
        "dir": "mqtt_tcp",
        "display_name": "Espressif ESP-IDF MQTT TCP client (Pub/Sub)",
        "source_dir": "examples/protocols/mqtt/main",
        "files": ["app_main.c"],
        "sdkconfig": """#pragma once
#include "sdkconfig_base.h"
#define CONFIG_BROKER_URL "mqtt://mqtt.eclipseprojects.io"
""",
        "devices": {}
    },
    {
        "dir": "bleprph",
        "display_name": "Espressif ESP-IDF NimBLE peripheral (GATT server)",
        "source_dir": "examples/bluetooth/nimble/bleprph/main",
        "files": ["main.c", "gatt_svr.c"],
        "extra_copy": ["bleprph.h"],
        "sdkconfig": """#pragma once
#include "sdkconfig_base.h"
#define CONFIG_BT_NIMBLE_ENABLED 1
""",
        "devices": {}
    }
]

for app in apps:
    app_dir = vendor_root / app["dir"]
    app_dir.mkdir(parents=True, exist_ok=True)
    (app_dir / "include").mkdir(parents=True, exist_ok=True)
    
    (app_dir / "include" / "sdkconfig.h").write_text(app["sdkconfig"], encoding="utf-8")
    
    file_hashes = {}
    src_base = idf_root / app["source_dir"]
    for fn in app["files"]:
        src_file = src_base / fn
        dst_file = app_dir / fn
        shutil.copy2(src_file, dst_file)
        file_hashes[fn] = digest(dst_file)
    
    for fn in app.get("extra_copy", []):
        src_file = src_base / fn
        dst_file = app_dir / fn
        shutil.copy2(src_file, dst_file)
    
    manifest = {
        "app_name": f"esp_idfv61_{app['dir']}",
        "display_name": app["display_name"],
        "board": "esp32_devkitc_v4",
        "category": "vendor_example",
        "upstream": {
            "vendor": "Espressif",
            "version": "v6.1",
            "source_dir": app["source_dir"],
            "files": file_hashes
        },
        "devices": app["devices"]
    }
    
    (app_dir / "wink-app.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Provisioned vendor app: {app['dir']}")
