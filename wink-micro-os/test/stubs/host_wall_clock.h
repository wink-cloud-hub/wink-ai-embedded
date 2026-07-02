/**
 * @file host_wall_clock.h
 * @brief 物理墙钟微秒 helper —— 供 pal_sim_scheduler_run WCET 量测与
 *        单测 cpu_hog 精准忙等共享使用（fixup 计划 R6 提取）。
 *
 * 契约：
 *   - 单调递增，微秒精度（host 走 QueryPerformanceCounter，POSIX 待补 TODO）。
 *   - 与 pal_os_get_us()（虚拟时钟）严格区分：虚拟时钟服务业务语义，物理墙钟
 *     只服务 WCET 兜底判定与"物理时长精准忙等" 测试助手。
 *   - 首次调用会初始化 static freq 缓存；后续调用零系统调用（除 QPC 本身）。
 */
#ifndef HOST_WALL_CLOCK_H
#define HOST_WALL_CLOCK_H

#include <stdint.h>

#ifdef _WIN32
#include <windows.h>

static inline uint64_t host_wall_clock_us(void) {
    static LARGE_INTEGER freq = { .QuadPart = 0 };
    LARGE_INTEGER c;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&c);
    /* 先乘再除避免频率整除精度损失（QPC freq 通常 10MHz） */
    return (uint64_t)(c.QuadPart * 1000000ULL / (uint64_t)freq.QuadPart);
}

#else
#  error "host_wall_clock_us: only Windows QPC supported this wave; POSIX TODO"
#endif

#endif /* HOST_WALL_CLOCK_H */
