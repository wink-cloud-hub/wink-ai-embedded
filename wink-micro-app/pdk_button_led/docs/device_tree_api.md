# PDK_BUTTON_LED Device Tree & API Specification

This document defines the hardware mapping and component bindings for `pdk_button_led`.

- **Board:** `pdk_pfs154_devboard`
- **Source Configuration:** `wink-micro-app/pdk_button_led/wink-app.json`

---

## 1. Device Summary

| Device Name | Driver Type | Pin Mapping | Active State | Description |
| :--- | :--- | :--- | :--- | :--- |
| `btn` | Button | PA.5 (Pin 5) | Active Low (0) | User push button with internal pull-up |
| `led` | LED | PA.4 (Pin 4) | Active High (1) | Output indicator LED |

---

## 2. Pin Routing & Peripheral Characteristics

- **PA.5 (Input)**: Configured via `PADIER |= (1 << 5)` (digital input buffer enable), `PAPH |= (1 << 5)` (internal pull-up enable), and `PAC &= ~(1 << 5)` (input direction).
- **PA.4 (Output)**: Configured via `PAC |= (1 << 4)` (output direction). Driven HIGH when button is pressed, and LOW when released.
