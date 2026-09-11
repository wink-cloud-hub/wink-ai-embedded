// SPDX-License-Identifier: Apache-2.0
// Stage3 forwarding shim (PLAN-20260911-MCS51-S3, S3-2 Step 3): the ADC0832
// board-device package moved to devices/adc0832/include/adc0832.h
// (lowercase canonical name; Linux CI is case-sensitive). Survives one
// stage (deleted in stage7). See S3-D1 for `#pragma message` vs `#warning`.
// TODO(stage7): delete this forwarding shim with the stage7 close-out.
#pragma once
#pragma message("mcs51 stage3: <ADC0832.H> moved to devices/adc0832/include/adc0832.h; include the new path")
#include "../devices/adc0832/include/adc0832.h"
