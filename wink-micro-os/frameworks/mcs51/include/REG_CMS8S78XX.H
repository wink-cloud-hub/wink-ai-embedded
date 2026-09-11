// SPDX-License-Identifier: Apache-2.0
// Stage3 forwarding shim (PLAN-20260911-MCS51-S3, S3-1 Step 1): the real
// CMS8S78xx sandbox device header moved to
// chips/cms8s78xx/include/REG_CMS8S78XX.H. This shim survives one stage
// (deleted in stage7) so samples/apps/tests keep compiling unmodified.
// S3-D1: `#pragma message` instead of `#warning` — MSVC has no `#warning`
// and GCC turns it into a hard error under this repo's -Werror test builds.
// TODO(stage7): delete this forwarding shim with the stage7 close-out.
#pragma once
#pragma message("mcs51 stage3: <REG_CMS8S78XX.H> moved to chips/cms8s78xx/include/; include the new path")
#include "../chips/cms8s78xx/include/REG_CMS8S78XX.H"
