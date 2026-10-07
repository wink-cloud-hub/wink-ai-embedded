/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include "sdkconfig_base.h"

#define CONFIG_EXAMPLE_MDNS_HOST_NAME "dashboard"
#define CONFIG_EXAMPLE_WEB_MOUNT_POINT "/www"
#define CONFIG_EXAMPLE_DEPLOY_WEB_PAGES 0

#define CONFIG_HTTPD_MAX_REQ_HDR_LEN 512
#define CONFIG_HTTPD_MAX_URI_LEN 512

#ifndef IDF_VER
#define IDF_VER "v6.1"
#endif
