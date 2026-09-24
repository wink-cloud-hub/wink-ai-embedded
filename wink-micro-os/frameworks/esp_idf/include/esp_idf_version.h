/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_IDF_VERSION_H
#define ESP_IDF_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_IDF_VERSION_MAJOR 6
#define ESP_IDF_VERSION_MINOR 1
#define ESP_IDF_VERSION_PATCH 0

#define ESP_IDF_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))

#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(ESP_IDF_VERSION_MAJOR, \
                                            ESP_IDF_VERSION_MINOR, \
                                            ESP_IDF_VERSION_PATCH)

const char *esp_get_idf_version(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP_IDF_VERSION_H */
