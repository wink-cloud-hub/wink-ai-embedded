// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal.h
 * @brief PAL aggregate header - Includes the entire PAL contract surface (HAL + OSAL + system services + status codes).
 *        Kernel internal components (dal/runtime/trace/targets) may #include "pal.h".
 *        App/BAL components are prohibited from including this header (see 03-directory-architecture.md §6).
 *        App/BAL should only include wink_status.h for status types.
 */
#ifndef PAL_H
#define PAL_H

#include "wink_status.h"
#include "pal_hal.h"
#include "hal/pal_i2c.h"
#include "hal/pal_adc.h"
#include "pal_osal.h"
#include "pal_log.h"
#include "pal_resource.h"
#include "pal_storage.h"

#endif /* PAL_H */
