# SMP UAF 验证测试

## 测试目的

验证 `pal_irq_synchronize()` 真正解决了 **SMP 多核系统中的经典并发问题**：

> **Core 0 调用 `pal_irq_disable()` 返回后，Core 1 可能仍在执行该 ISR。**
>
> 如果此时释放 ISR 使用的内存，会导致 **UAF (Use-After-Free)** 崩溃。

这个问题在单线程 host 环境下永远测不出来，**必须在真实双核硬件上验证**。

---

## 测试原理

```
Step 1: 分配资源 + 注册中断
  └─ test_resource_t { magic=0xDEADBEEF, ... } → 传给 ISR

Step 2: 持续触发中断（让 ISR 一直在飞）
  └─ pal_irq_set_pending() × 200 次

Step 3: 关键路径
  ├─ pal_gpio_disable_interrupt(PAL_GPIO_0)  ← 阻止新中断进入
  ├─ pal_irq_synchronize(PAL_IRQ_GPIO)       ← ✅ 等待所有 ISR 退出
  └─ free(res)                               ← 现在安全了！

Step 4: ISR 中检查 magic 值
  └─ 如果 magic != 0xDEADBEEF → UAF 已发生！
```

---

## 真机运行步骤

### 第一步：切换到测试 App

```powershell
cd esp32_firmware
.\switch_to_smp_uaf_test.ps1
```

### 第二步：编译烧录（有 synchronize）

```powershell
idf.py build flash monitor
```

**预期结果：**
```
✅ synchronize() IS blocking correctly
✅ TEST PASSED! All 1000 rounds completed without UAF
```

### 第三步：注释掉 synchronize，验证必要性

编辑 `app_callbacks.c`：
```c
#define TEST_ENABLE_SYNCHRONIZE  false   // 改成 false
```

重新编译烧录：
```powershell
idf.py build flash monitor
```

**预期结果（几十轮内就会触发）：**
```
!!! UAF DETECTED at round 17 !!! magic=0xCCCCCCCC
❌ TEST FAILED
```

---

## 为什么 ISR 中要加 50us 延时？

```c
pal_delay_us(50);  // test_smp_uaf.c:66
```

真实的 ISR 执行很快，race window 只有几十纳秒，很难触发。
我们故意放慢 ISR，把 race window 放大到 **50 微秒**，让测试在几秒钟内就能测出问题。

这是 **并发测试的标准技巧** — 放大竞争窗口，把 "可能几天才出现一次的 bug" 变成 "几秒必现的 bug"。

---

## 代码架构说明

| 文件 | 说明 | 平台相关？ |
|------|------|-----------|
| `test_smp_uaf.h` | 测试 API 头文件 | ❌ 100% 通用 |
| `test_smp_uaf.c` | 核心测试逻辑 | ❌ 100% PAL API |
| `app_callbacks.c` | Wink App 回调接口 | ❌ 通用 |
| `device_tree.c` | 最小化设备树 | ❌ 通用 |
| `test_smp_uaf_e2e.c` | host 端验证 | ❌ 通用 |
| `CMakeLists.txt` | 构建脚本 | ✅ CMake 标准 |

> **关键架构成就：核心测试逻辑 100% 跨平台！**
>
> 移植到其他 SMP 多核 MCU（如 RP2350 Pico 2）时，
> 不需要修改任何一行测试代码，只需要实现 PAL 层即可。

---

## 预期实验结果对比表

| 场景 | 平台 | 有无 synchronize | 预期结果 |
|------|------|-----------------|---------|
| ESP32 双核 SMP | 真机 | ✅ 有 | 1000 轮全部通过 |
| ESP32 双核 SMP | 真机 | ❌ 无 | 几十轮内触发 UAF |
| host 单线程 | 模拟器 | 有无都一样 | 永远不触发 UAF |

> **单线程 host 环境下 synchronize 看似多余，但它是 SMP 系统安全的必要保障！**

---

## 参考资料

- Linux 内核 `synchronize_irq()` 设计原理
- [ADR-IRQ-007] SMP ISR 执行同步原语设计文档
- ESP32 Technical Reference Manual → 中断控制器章节
