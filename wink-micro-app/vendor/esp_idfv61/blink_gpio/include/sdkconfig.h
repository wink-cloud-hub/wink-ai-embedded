/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CORPUS_BLINK_SDKCONFIG_H_
#define CORPUS_BLINK_SDKCONFIG_H_

#include "sdkconfig_base.h"

/* Kconfig.projbuild 选择：GPIO 直驱分支（避开 LED_STRIP/RMT 后端，RMT 真门面递延 M2/M4） */
#define CONFIG_BLINK_LED_GPIO 1
#define CONFIG_BLINK_GPIO 2
#define CONFIG_BLINK_PERIOD 1000

#endif /* CORPUS_BLINK_SDKCONFIG_H_ */
