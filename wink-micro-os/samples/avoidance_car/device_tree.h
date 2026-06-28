/**
 * @file device_tree.h
 * @brief avoidance_car 示例 App 的设备树声明（codegen 产物占位；手动编写演示注入点）。
 */
#ifndef DEVICE_TREE_H
#define DEVICE_TREE_H

#include "wink_status.h"
#include "dal_ultrasonic.h"
#include "dal_servo.h"

/* ADR-0008 codegen 分配的稳定 device_id（注册表映射）。
 * 注：device_id ↔ params 布局是隐式契约；任一 DAL params 布局变更须 bump blob version。 */
#define DEV_ID_NECK_SERVO    1u
#define DEV_ID_FRONT_RADAR   2u

extern dal_ultrasonic_t front_radar;
extern dal_servo_t      neck_servo;

/**
 * @brief ADR-0008 Flash 覆写逃生通道：从 pal_storage 读 "dtcfg" blob 覆写静态实例字段。
 * @note 在 sample app_init 最顶部、dal_*_init 之前调用。读失败/损坏 → 静默降级到编译期默认，
 *       绝不 Panic。返回值仅供诊断/测试；app_init 设计上忽略结果（降级即默认行为），
 *       故未标 WINK_WARN_UNUSED_RESULT。
 * @return WINK_OK（已应用或 count==0）/ 降级错误码（EMPTY/UNSUPPORTED/CHECKSUM/CORRUPT）。
 */
wink_status_t device_tree_apply_flash_config(void);

#endif /* DEVICE_TREE_H */
