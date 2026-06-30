/**
 * @file device_tree.c
 * @brief SMP UAF 测试的设备树实现。
 *
 * 本测试是纯 CPU/中断控制器的软件测试，不依赖任何 DAL 外设。
 * 保持最小实现，仅满足编译链接要求。
 */

#include "device_tree.h"

/* 避免 MSVC C4206 警告（空翻译单元） */
typedef int _dummy_typedef_for_msvc;
