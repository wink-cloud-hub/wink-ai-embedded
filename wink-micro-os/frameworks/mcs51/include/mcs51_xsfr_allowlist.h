// SPDX-License-Identifier: Apache-2.0
// Stage3 forwarding shim (PLAN-20260911-MCS51-S3, S3-1 Step 1): the real
// GAP-23 tripwire allowlist moved to
// chips/cms8s78xx/include/cms8s_xsfr_allowlist.h (consumed by
// mcs51_xdata.cpp until stage5 parametrizes the XSFR window). Survives one
// stage for the header (deleted in stage7); the allowlist CONTENT moves to
// chip ownership in stage5. See S3-D1 for `#pragma message` vs `#warning`.
// TODO(stage7): delete this forwarding shim with the stage7 close-out.
#pragma once
#pragma message("mcs51 stage3: <mcs51_xsfr_allowlist.h> moved to chips/cms8s78xx/include/cms8s_xsfr_allowlist.h")
#include "../chips/cms8s78xx/include/cms8s_xsfr_allowlist.h"
