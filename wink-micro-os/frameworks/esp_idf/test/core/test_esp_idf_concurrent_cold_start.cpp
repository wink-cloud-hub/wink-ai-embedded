/* SPDX-License-Identifier: GPL-3.0-only */
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

extern "C" void esp_idf_ensure_framework_ready(void);

namespace {
std::atomic<bool> g_start{false};
std::atomic<bool> g_reset_entered{false};
std::atomic<bool> g_release_reset{false};
std::atomic<unsigned> g_reset_calls{0};
std::atomic<unsigned> g_returned_callers{0};

bool wait_for(const std::atomic<bool>& value) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!value.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}
} // namespace

/* GNU ld --wrap holds the reset call open while competing callers arrive. */
extern "C" void __wrap_esp_freertos_pools_reset(void) {
    g_reset_calls.fetch_add(1, std::memory_order_relaxed);
    g_reset_entered.store(true, std::memory_order_release);
    while (!g_release_reset.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
}

int main() {
    constexpr unsigned kCallers = 8;
    std::vector<std::thread> callers;
    callers.reserve(kCallers);
    for (unsigned i = 0; i < kCallers; ++i) {
        callers.emplace_back([] {
            while (!g_start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            esp_idf_ensure_framework_ready();
            g_returned_callers.fetch_add(1, std::memory_order_release);
        });
    }
    g_start.store(true, std::memory_order_release);

    if (!wait_for(g_reset_entered)) {
        g_release_reset.store(true, std::memory_order_release);
        for (auto& caller : callers) caller.join();
        std::fprintf(stderr, "pool initialization did not start\n");
        return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    if (g_returned_callers.load(std::memory_order_acquire) != 0) {
        g_release_reset.store(true, std::memory_order_release);
        for (auto& caller : callers) caller.join();
        std::fprintf(stderr, "cold-start caller returned before pool initialization completed\n");
        return 2;
    }

    g_release_reset.store(true, std::memory_order_release);
    for (auto& caller : callers) caller.join();
    if (g_returned_callers.load(std::memory_order_acquire) != kCallers) {
        std::fprintf(stderr, "not all cold-start callers returned\n");
        return 3;
    }
    if (g_reset_calls.load(std::memory_order_relaxed) != 1) {
        std::fprintf(stderr, "pool reset ran more than once\n");
        return 4;
    }
    return 0;
}
