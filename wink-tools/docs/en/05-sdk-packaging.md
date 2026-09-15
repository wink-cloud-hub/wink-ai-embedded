<!--
visibility: public
winkcli-version: ">=0.1.0"
i18n-meta
source: wink-tools/docs/zh/05-sdk-packaging.md
translated: 2026-09-15
translator: AI-assisted
sync-status: up-to-date
-->
# 05 · SDK Packaging Guide (`winkcli pack`)

`winkcli pack` ships the SDK in two forms: full-source **Source SDK** and header-only **Binary SDK** with precompiled static libraries.

---

## 1. Mode comparison

| Command | Artifact | Use case | Traits |
|---|---|---|---|
| `winkcli pack source` | Source SDK | Open development / full in-house builds | Complete C sources included |
| `winkcli pack binary` | Binary SDK | Closed-source distribution / fast third-party builds | Headers + precompiled `.a` only |

---

## 2. Pack Ceilings (Binary SDK only)

To keep ABI compatibility between the precompiled library and application code, the Binary SDK enforces configuration ceilings:

| Config | Ceiling |
|---|---|
| Soft timers (`max_soft_timers`) | ≤ 32 |
| PWM channels (`pwm_channels`) | ≤ 16 |

Exceeding a ceiling aborts the build with a pointer to Source SDK:

```text
[config_h] Binary SDK config ceiling violation:
  - max_soft_timers=64 exceeds Binary SDK ceiling (32); use Source SDK or reduce the value.
```

> Consuming a Binary SDK requires a C99 compiler (GCC / Clang / MSVC). Windows MinGW and MSVC chains are both validated.

---

## 3. Packaging commands

```bash
# Binary SDK (per target)
winkcli pack binary --target host --out dist/wink-sdk-host
winkcli pack binary --target wasm --out dist/wink-sdk-wasm

# Reuse existing build outputs
winkcli pack binary --target host --out dist/wink-sdk-host --skip-build

# Source SDK
winkcli pack source --out dist/wink-sdk-source
```

---

## 4. Binary SDK layout

```text
wink-sdk-binary/
├── cmake/
│   └── wink_binary_import.cmake   # CMake import script for the prebuilt library
├── include/                       # Trimmed public API headers
│   ├── wink_status.h
│   ├── dal/
│   └── bal/
├── lib/
│   └── libwink_os.a               # Precompiled static library
├── sdk_manifest.json              # SDK version & config metadata
└── README.md                      # Integration guide
```
