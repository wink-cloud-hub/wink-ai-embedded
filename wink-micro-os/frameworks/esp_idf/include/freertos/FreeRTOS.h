/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef INC_FREERTOS_H
#define INC_FREERTOS_H

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOSConfig.h"
#include "freertos/projdefs.h"
#include "freertos/portable.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Unconditionally include idf_additions.h per M0-2 Step 6 */
#include "freertos/idf_additions.h"

#ifdef __cplusplus
}
#endif

#endif /* INC_FREERTOS_H */
