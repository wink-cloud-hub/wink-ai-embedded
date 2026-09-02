// SPDX-License-Identifier: Apache-2.0
// cms8s78xx.h — SANDBOX device header for the Cmsemicon CMS8S78xx part.
//
// The vendor SDK ships its own device header (Libary/Device/CMS8S78xx/Include/
// cms8s78xx.h) written for the Keil C51 toolchain: it re-typedefs the stdint
// types, re-declares every SFR with the Keil `sfr` keyword, and defines XSFRs
// as wild MOVX pointers (`#define ADCLDO *(volatile unsigned char xdata *)
// 0xF692`). That file cannot be compiled by the host/wasm C++ sandbox.
//
// Instead, the vendor StdDriver sources (e.g. StdDriver/src/adc.c) are compiled
// UNMODIFIED (after the Keil-dialect cleanup pass) with THIS header first on
// the include path, so their `#include "cms8s78xx.h"` resolves here rather than
// to the vendor Keil header. Everything the vendor driver needs — the SFR
// proxy instances, the XSFR proxies, the verbatim vendor bit masks, and the
// stdint/intrins shims — is provided by REG_CMS8S78XX.H / REGX52.H.
//
// The vendor header and sources remain reference-only fixtures under
// docs/vendors/ and are NEVER committed; this committed shim is the sandbox
// stand-in (M5 tier-b harvest, ADR-0073 D6).
#pragma once

#include "REG_CMS8S78XX.H"
