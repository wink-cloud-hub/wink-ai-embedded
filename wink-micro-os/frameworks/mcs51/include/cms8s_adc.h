// SPDX-License-Identifier: Apache-2.0
// Stage3 forwarding shim (PLAN-20260911-MCS51-S3, S3-1 Step 1): the real
// on-chip ADC model header moved to chips/cms8s78xx/include/cms8s_adc.h.
// Survives one stage (deleted in stage7). See S3-D1 for why this uses
// `#pragma message` instead of `#warning`.
// TODO(stage7): delete this forwarding shim with the stage7 close-out.
#pragma once
#pragma message("mcs51 stage3: <cms8s_adc.h> moved to chips/cms8s78xx/include/; include the new path")
#include "../chips/cms8s78xx/include/cms8s_adc.h"
