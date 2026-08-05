/**
 * @file pal.h
 * @brief PAL 聚合头 —— 一次 include 拉全 PAL 契约面（HAL + OSAL + 系统服务 + 状态码）。
 *        内核内部组件（dal/runtime/trace/targets）可 #include "pal.h"；
 *        App/BAL 禁用本头（见 03-directory-architecture.md §6 App/BAL 禁入规则），
 *        它们只应 include wink_status.h（基础类型例外）。
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
