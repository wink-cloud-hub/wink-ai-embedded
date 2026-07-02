#ifndef DEVICE_TREE_H
#define DEVICE_TREE_H

#include "wink_status.h"
#include "dal_ultrasonic.h"
#include "dal_servo.h"

#define DEV_ID_NECK_SERVO    1u
#define DEV_ID_FRONT_RADAR   2u

extern dal_ultrasonic_t front_radar;
extern dal_servo_t      neck_servo;

wink_status_t device_tree_apply_flash_config(void);

#endif /* DEVICE_TREE_H */
