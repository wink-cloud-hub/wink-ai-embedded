/* SPDX-License-Identifier: CC0-1.0 */
#ifndef CORPUS_LEGACY_I2C_SDKCONFIG_H_
#define CORPUS_LEGACY_I2C_SDKCONFIG_H_

#include "sdkconfig_base.h"

/* CONFIG_IDF_TARGET_* 由 CMake 注入（esp_idf_target.cmake）；SOC_* 能力宏由
   chips/<target>/include/soc/soc_caps.h 提供（ADR-0085 / ADR-0087）。 */
#define SOC_I2C_SUPPORT_SLAVE 1

#endif /* CORPUS_LEGACY_I2C_SDKCONFIG_H_ */
