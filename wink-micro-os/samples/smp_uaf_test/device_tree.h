/**
 * @file device_tree.h
 * @brief SMP UAF 测试的设备树头文件。
 *
 * 本测试是纯 CPU/中断控制器的软件测试，几乎不需要外设。
 * 将测试使用的逻辑中断编号配置化到设备树中，屏蔽具体平台的物理中断号。
 */

#ifndef DEVICE_TREE_H
#define DEVICE_TREE_H

/* SMP UAF 测试所使用的逻辑中断源定义 */
#define TEST_IRQ_UAF          7    /* UAF 主测试使用中断号 */
#define TEST_IRQ_SLOW         8    /* 阻塞测试使用中断号 */

#endif /* DEVICE_TREE_H */
