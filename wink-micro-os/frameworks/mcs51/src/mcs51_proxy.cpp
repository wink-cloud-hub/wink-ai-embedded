// SPDX-License-Identifier: Apache-2.0
// MCS-51 SFR shadow storage (C linkage, boundary ③). Definition lives in one
// TU; the proxy header declares it extern "C".
#include "mcs51_proxy.hpp"

// Zero-initialised 256-byte SFR address space (BSS, no static-init ordering
// hazard — ADR-0070 static-init safety).
extern "C" uint8_t wink_mcs51_sfr_shadow[256] = {0};
