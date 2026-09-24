/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_LOG_FORMAT_H_
#define ESP_LOG_FORMAT_H_

#define ESP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " (%u) %s: " format LOG_RESET_COLOR "\n"

#endif /* ESP_LOG_FORMAT_H_ */
