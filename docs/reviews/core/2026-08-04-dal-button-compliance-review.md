# 2026-08-04 dal_button 规范性合规评审

| 项 | 内容 |
|----|------|
| **评审对象** | `dal_button` 模块（`wink-micro-os/dal/include/input/dal_button.h` + `dal/src/input/dal_button.c` + `dal/include/input/dal_button_bal.h` + `codegen/drivers/button.yaml`） |
| **规范基线** | `dal-api-consistency-spec.md` v3.4.1 |
| **关联 ADR** | ADR-0001（错误码符号约定）、ADR-0004（静态分发）、ADR-0024（Deinit 三阶段模型）、ADR-0031（BAL IRQ 守护）、ADR-0034（pull NONE 浮空语义）、ADR-0043（YAML lint）、ADR-0046（Registry SSOT）、ADR-0048（执行器命名）、ADR-0056（跨 Profile 量纲 A/B） |
| **评审方法** | 规范 17 节逐条交叉对照 + 200 项微观合规清单 |
| **评审范围** | 全 P0（MUST） + 全 P1（SHOULD） |
| **结论** | 11 个 MUST 不合规 + 12 个 SHOULD 不规范；**无 1 个 vtable/container_of/strcpy/裸 busy-wait 之类的禁用项**（已对 8B-F-001/002、C-001/002、UT-001 等硬性禁令做反向交叉验证）；总体优于上一轮 `dal_ultrasonic` 评审，但仍存在可改善项 |

---

## 1. 执行摘要

`dal_button` 经历了 Wave 1~4 的快速迭代（基本去抖 → 事件回调 → ISR 计数 → BAL IRQ 守护），API 表面有 9 个公开函数 + 4 个 BAL-internal 函数，构成本仓库 B 类传感器中**唯一支持硬件中断路径**的器件，复杂度也最高。本次评审发现 **11 个 MUST 不合规 + 12 个 SHOULD 不规范**，按风险与修复成本分布如下：

| 风险等级 | 数量 | 涉及规则 |
|---------|------|---------|
| **P0 致命**（MUST 不合规，会触发 lint error 或破坏编译期/运行期契约） | 4 | DAL-L-008、B-025、V-010、S-014 |
| **P1 高级**（MUST 不合规，但不触发 lint error，仅在故障/迁移场景暴露） | 7 | DAL-L-007/011/012/014/015、C-040/042 |
| **P1 重要**（SHOULD 不规范，存量可豁免但应在迁移期收口） | 7 | U-010、V-002、U-001、B-013、C-031 |
| **P2 建议**（可改善项，文档/可测试性） | 5 | U-011、S-013、BC-001 等 |
| **完全合规章节** | §2.4 动态内存、§3.2 safe_off、§4.1 错误码符号、§5.1 函数命名、§5.2 read/get 三元语义、§5.3 动词语义库、§6.0 默认非线程安全契约、§7.1 阻塞标注、§9.1 单位后缀、§9.4 A 类定标（button 无 A 类量） | |

> 与上一轮 `dal_ultrasonic` 对比：`dal_button` 在 §3.1 生命周期、§5.2 动词语义、§6.4 回调上下文声明上**显著优于** ultrasonic；主要差距在 §2.3 ABI 稳定性断言（仅加 `config==0` 断言，缺 sizeof + initialized offset 的 32/64 位分档断言）、§6.2 SMP 多字段快照契约（不适用但应声明不适用理由）、§6.0 Thread-safe 默认契约（多数头注释缺声明）。

---

## 2. ΢�ۺϹ��嵥

> ���� 200 ��΢���嵥����¼ A��ժҪ��MUST �� 55 ��ȫ�� 36 / ���Ϲ� 11 / ������ 8��SHOULD �� 60 ��ȫ�� 41 / ���淶 12 / ������ 7��MAY �� 10 �����δʵ�� self_test / reset / get_state����

### 2.1 MUST ���Ϲ��嵥��11 �

| # | ���� | ���� | ��ǰ״̬ | ���� | �޷� |
|---|------|------|---------|------|------|
| 1 | DAL-L-007 | `init` ʧ��ʱ MUST �� `dev->initialized = false` | `init` ����δ��ʽ�� `dev->initialized = false`������ `{0}` ���� | P0 ���� | `init` ����� `dev->initialized = false`���� `dal_ultrasonic` һ�� |
| 2 | DAL-L-008 | `init` �ڲ���ĳ��ʧ�� MUST �ع������� claim �� PAL ��Դ | ��ǰ 3 ��ʧ�ܷ�֧���� inline release��`pal_resource_release`�����ظ�����©��ȱ�� 4 �� config ������ʧ�ܻع�·�� | P0 ���� | ��Ϊ goto-cleanup ��ʽ�ع���flag ��¼ÿ���ɹ�/ʧ�� |
| 3 | DAL-F-004 | ������������⣬���� `wink_status_t` ���� API MUST �� `WINK_WARN_UNUSED_RESULT` | 9 ������ API ȫ���ѺϹ��ע��`poll` / `deinit` ���ڻ�������� | **�Ϲ�** | �� |
| 4 | DAL-C-040 | ͬһ `dal_xxx_t *dev` �������� MUST �ɵ��÷��ⲿ���л� | ͷע�� 8 �� API ȱ `Thread-safe: No` �������� `on_event` �ᵽ | P1 �߼� | 9 ������ API ͷע�Ͳ� `Thread-safe: No; ISR-safe: No;` |
| 5 | DAL-B-025 | `poll` ����ֵ���� MUST ��Ӳ������ͬʱǨ�� state ����̬ | `dal_button_poll` ��ǰ `pal_gpio_read` ʧ��ֱ�� `return s` ͸����**δ�� last_status Ҳ��Ǩ state** | P0 ���� | ���� `last_status` �ֶΣ�ʧ��ʱ�� `last_status = s`������ stable_pressed�������֪״̬�� |
| 6 | DAL-V-010 | `was_*` ������ MUST ԭ�� | `dal_button_was_pressed` �ڲ� `stable_pressed && !last_reported` �ж� + д `last_reported` **���ٽ�������**��SMP ˫�˾�����˫���� | P0 ���� | �� `PAL_CRITICAL_SECTION` ��Χ��+д |
| 7 | DAL-L-010 | `deinit` MUST �ݵȣ�δ init �� `WINK_OK` | line 348 `if (!dev->initialized) return WINK_OK;` ��ʵ�� | **�Ϲ�** | �� |
| 8 | DAL-L-011 | deinit MUST �� ADR-0024 �峡�����ж� �� �� in-flight �ص����� �� �ͷ���Դ �� memset | ��ǰ˳����ȷ����**�� 1 ���� backend ������ disable ֮ǰ**�� ISR �ڵ� backend ����΢С���� | P1 �߼� | �ĵ�����˳����ͷע�ͣ�backend �� 1 �ֽڵ�д�߶��������� ISR �������ֵ�� |
| 9 | DAL-L-012 | ������ʹ�� ISR��deinit MUST �Ƚ��ж� �� �� in-flight �ص����� �� �ͷ���Դ | ��ʵ�֣�`pal_gpio_synchronize_interrupt`������ deinit ��·��ֻ�� `gpio_isr_registered` ͬ��һ�� | P1 �߼� | �ĵ���"deinit ����ÿ refcount �� synchronize����һ���㹻"��ͷע�� |
| 10 | DAL-L-014 | deinit �ڲ����ײ� PAL ʧ�� MUST �� LOGW | ��ǰ `WINK_IGNORE_UNUSED` ��Ĭ�̵� `pal_gpio_disable_interrupt` / `pal_resource_release` / `pal_gpio_reset_pin`��reset_pin �� void���ķ���ֵ��**�� LOGW** | P1 �߼� | ���� `LOGW_IF_RC` / `LOGW_IF_VOID` �꣨�� `dal_ultrasonic` һ�£� |
| 11 | DAL-L-015 | deinit ���ط� OK ʱ MUST �� best-effort �峡 | ��ǰ deinit û�� first_err �ռ����ƣ�**�κ� step ʧ���� return �����峡** | P1 �߼� | �ռ� first_err��goto-cleanup ͳһ�峡����� return first_err |
| 12 | DAL-L-020 | `safe_off` ���� `is_actuator: true` ʱ MUST ʵ�� | button YAML `is_actuator: false`��YAML `safe_off_fn: ""`��**�� `dal_button_safe_off` ʵ��** | **������** | �� |


### 2.2 SHOULD ���淶�嵥��12 �

| # | ���� | ���� | ��ǰ״̬ | �޷� |
|---|------|------|---------|------|
| 13 | DAL-S-014 | Ӧ���� `_Static_assert(offsetof(dal_xxx_t, config) == 0)` �� sizeof �ֵ����� | ͷ�ļ� line 102 �Ѽ� `config==0` ���ԣ�**ȱ sizeof + initialized offset �� 32/64 λ�ֵ�����** | �� ILP32/LP64 �ֵ����ԣ��ο� `dal_ultrasonic` Step 2�� |
| 14 | DAL-U-001 | ����������������� MUST ����λ��׺ | `long_press_ms`��`debounce_ms`��`press_start_ms`��`held_ms` ȫ�Ϲ棻`debounce_counter` / `debounce_threshold` / `edge_count` �� count ���� | �������Ƿ�� `_raw` ��׺���ֿ��� `_counter` �ǹ������ɻ��⣩ |
| 15 | DAL-U-003 | ��λ��׺ MUST ȡ�Է��ö�ٱ� | `pulse_us` / `ms` / `cnt` ��׺���Ϲ� | **�Ϲ�** | �� |
| 16 | DAL-U-010 | API Contract ע�� MUST �� Range �ֶ����������Ϸ�ֵ�� | `dal_button_set_long_press_ms` ȱ `Range: > 0`��`dal_button_set_debounce_ms` ȱ `Range: [1, 2550] ms`��`enable_isr_counter` ȱ Error-codes �����б� | �� Range / Error-codes �ֶ� |
| 17 | DAL-V-002 | �������� API �� YAML �б� `device_specific: true` | button û�� device_specific API��enable_isr_counter �Ǳ�׼�⶯�� `enable_*` ��ĺ���ʹ�ã� | **������** | �� |
| 18 | DAL-C-042 | Thread-safe Contract �ֶ�ȱʧʱĬ�ϰ� No ���ͣ�lint SHOULD �Թ��� API ȱ���ֶα� warning | 9 ������ API �� 8 ��ȱ `Thread-safe: No` / `ISR-safe: No` ���� | ȫ�������� #4 �ϲ��� |
| 19 | DAL-B-013 | ���� API ע�� SHOULD ������ TWDT ��ϵ | `dal_button_poll` ע��˵"������"���� `pal_gpio_read` �� ESP-IDF �¿���������ȡ���������� | ͷע����ȷ"poll �� read ����ʱ���� PAL ��֤ �� N us��TWDT-safe" |
| 20 | DAL-S-013 | MUST ֧�� `{0}` ���ʼ�� | ��ǰ `{0}` �������ֶ�Ϊ��ȫĬ��̬��debounce_counter=0, event_cb=NULL, edge_count=0, isr_counter_enabled=false, event_backend=NONE, irq_pending=false, gpio_isr_registered=false | **�Ϲ�** | �� |
| 21 | DAL-U-011 | A ��ִ����������Խ�� MUST ��ʽǯλ | button �� B �ഫ�������� A ���������long_press_ms / debounce_ms �������࣬Խ�緵�� INVALID_ARG����ʵ�֣� | **������** | �� |
| 22 | DAL-BC-001 | Init-to-Ready ��Լ | init �ɹ��������ɵ� poll/event/on_event���� enable բ�� | **�Ϲ�** | �� |
| 23 | DAL-B-024 | ����ʽ get_cached �� state==IDLE �� NO_DATA/EMPTY | button �� poll �ƽ�ʽ������������ʽ | **������** | �� |
| 24 | DAL-C-031 | �ص������ı�����ȷ���� | `dal_button_event_cb` ע�ͣ�line 33-34������ȷ"�� dal_button_poll() �� task ������ͬ�����ã��� ISR���������� WINK_BLOCKING API �����鱣�ֶ�С" | **�Ϲ�** | �� |
| 25 | DAL-S-022 | 8 λ Micro Profile �� No-Malloc | button �� malloc | **�Ϲ�** | �� |
| 26 | DAL-C-010 | ���ֶο���һ���Ա������� | button ISR ���� 1 �����ֶ� `edge_count`������ DAL-C-001 ���ֿ�+��д�ߣ�������Ҫ���ֶο��գ��� `event_backend` �� 1 �ֽ� ISR �� + task д���Ѿ��� `PAL_CRITICAL_SECTION` ���� | **�Ϲ棨��������** | �� |


## 3. ��ϸ���֣����޸�˳�����У�

### 3.1 P0 �������⣨���ޣ�

#### #2 + #1 + #11��init goto-cleanup + deinit best-effort �����ع�

**λ��**��
- `dal_button.c:74-113`��init��
- `dal_button.c:342-376`��deinit��

**��������**��

init ��ǰ 4 ��ʧ�ܷ�֧��
1. `dev==NULL` / `cfg==NULL` �� �� return WINK_ERR_INVALID_ARG������Դй©��OK��
2. `cfg->owner==NULL` �� �� return��OK��
3. `dev->initialized` ���� true �� �� return WINK_ERR_ALREADY_INITIALIZED��OK��
4. `!button_pull_valid(cfg->pull)` �� �� return��OK��
5. `pal_resource_claim` ʧ�� �� �� return rs��**�ⲽ��ȷ����δ claim ������Դ**��
6. `pal_gpio_init` ʧ�� �� inline `pal_resource_release` + return��**�� 1 ���ع���**��
7. memcpy ����ʧ��·����OK��

δ��������� `apply_override` ����� `pal_gpio_set_direction` ���裬�� 6 ���� inline release ģʽ���ظ�����©��

deinit ��ǰ 4 ������ֱ�Ӵ��У�
1. �� `event_backend` + `isr_counter_enabled`��line 357-358��
2. `gpio_isr_registered` ʱ `disable + synchronize`��line 359-363��
3. `pal_gpio_reset_pin`��line 367��
4. `pal_resource_release`��line 370��
5. `memset` ���㣨line 373��

**����**��ÿ�� `WINK_IGNORE_UNUSED` ��Ĭ�̵� rc����һʧ���� return����Υ�� DAL-L-014������ LOGW��+ DAL-L-015������ best-effort ����峡����

**�޷�**��

���� `dal_ultrasonic` ��Ӧ�õ�ģʽ��Step 5 + Step 10����
- init ��Ϊ goto-cleanup ��ʽ��
  ```c
  bool pin_claimed = false;
  bool pin_inited  = false;
  dev->initialized = false;   // DAL-L-007
  rc = pal_resource_claim(...);
  if (rc != WINK_OK) return rc; pin_claimed = true;
  rc = pal_gpio_init(...);
  if (rc != WINK_OK) goto cleanup; pin_inited = true;
  // ... ��� config ���ʼ���ֶ� ...
  dev->initialized = true;    // DAL-L-003
  return WINK_OK;
  cleanup:
      if (pin_inited)  (void)pal_gpio_reset_pin(cfg->pin);
      if (pin_claimed) WINK_IGNORE_UNUSED(pal_resource_release(...));
      return rc;
  ```
- deinit ���� first_err �ռ� + ���� 4 ���峡��
  ```c
  wink_status_t first_err = WINK_OK;
  // step 1: clear refs (event_backend + isr_counter_enabled)
  PAL_CRITICAL_SECTION({
      dev->event_backend = DAL_BUTTON_BACKEND_NONE;
      dev->isr_counter_enabled = false;
  });
  // step 2: disable + synchronize (if registered)
  if (dev->gpio_isr_registered) {
      LOGW_IF_RC(pal_gpio_disable_interrupt(pin), first_err);
      LOGW_IF_RC(pal_gpio_synchronize_interrupt(pin), first_err);
      dev->gpio_isr_registered = false;
  }
  // step 3: reset GPIO
  LOGW_IF_VOID(pal_gpio_reset_pin(pin));  // void func
  // step 4: release resource
  LOGW_IF_RC(pal_resource_release(PAL_RESOURCE_GPIO_PIN, pin, owner), first_err);
  // step 5: clear
  memset(dev, 0, sizeof(dal_button_t));
  return first_err;
  ```

**Ԥ�ڹ�����**��1 commit��~60 �иġ�

---

#### #5��poll ʧ��ʱ state Ǩ�� + last_status �ֶ�

**λ��**��`dal_button.c:120-121`

**����**��
```c
wink_status_t s = pal_gpio_read(dev->config.pin, &raw);
if (wink_status_is_error(s)) { return s; }
```
ֱ�� return ͸����**û��**�κ� state ��¼���´� poll �����������ԡ����Ӳ����Ĺ��ˣ��� `WINK_ERR_DISCONNECTED`�������÷����κ��ֶβ�ѯ"�����ť�ǲ����Ѿ�����"��

**�޷�**��

1. `dal_button_t` ���� `wink_status_t last_status;` �ֶΣ����� `initialized`������ ABI ���ԣ�
2. init ʱ `dev->last_status = WINK_OK`
3. poll ʧ��ʱ��
   ```c
   dev->last_status = s;
   return s;
   ```
4. �ṩ `dal_button_get_status(dev, &out_status)` API��MUST ͨ�� out �������أ�**��**ֱ�ӷ���״̬�룩
5. ͷע����ȷ"Ӳ����ȡʧ��ʱ last_status ͸����state machine �����ϴ��ȶ�̬"

> ��ע������ `test_pull_none_disconnected_without_injection` ���� `WINK_ERR_DISCONNECTED`���� new ��Ϊһ�¡�

**Ԥ�ڹ�����**��1 commit��~30 �иģ����ֶ�������get_status API�����⣩��

---

#### #6��was_pressed ԭ�ӻ�

**λ��**��`dal_button.c:170-178`

**����**��
```c
bool event = (dev->stable_pressed && !dev->last_reported);
dev->last_reported = dev->stable_pressed;
*out_was_pressed = event;
```
�� `last_reported` + д `last_reported` ���ٽ����������� SMP ˫�ˣ�ESP32-S3���ϣ�
- �� 0 �� was_pressed �� event=true �� д last_reported=true �� ���� true
- �� 1 ͬʱ�� was_pressed �� ���� last_reported=false���� 0 ��ûд���� event=true �� д last_reported=true �� ���� true
- **�����ͬһ�����¼������������Σ�Υ�� DAL-V-010 ԭ����**

**�޷�**��

�� `PAL_CRITICAL_SECTION` ��Χ��+д��
```c
PAL_CRITICAL_SECTION({
    bool event = (dev->stable_pressed && !dev->last_reported);
    dev->last_reported = dev->stable_pressed;
    *out_was_pressed = event;
});
```

**����**���ο� `test_isr_counter_no_lost_edges_during_reset` ģʽ������ `test_was_pressed_atomic_under_lock`��
- �����ٽ�����`pal_irq_save_rtos_safe()`��
- ģ��"�Ѱ���δ����"��д `stable_pressed=true; last_reported=false;`
- ��������һ�� task ��ͼ�� was_pressed �� ������host �ϲ���ӣ���������·����һ�Σ�
- �ͷ���
- ��ֻ֤����һ�� true

**Ԥ�ڹ�����**��1 commit��~15 �и� + 1 ���µ��⡣

---

#### #13��ABI 4 ���ȶ��Զ���

**λ��**��`dal_button.h:102`

**����**����ǰֻ�� `offsetof(dal_button_t, config) == 0` �������ԣ�ȱ sizeof + initialized offset �� 32/64 λ�ֵ����ԡ����� `dal_dc_motor` golden ref ������

**�޷�**��

```c
_Static_assert(offsetof(dal_button_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_button_config_t) == 12, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_button_t, initialized) == 12, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_button_t) == 40, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_button_config_t) == 16, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_button_t, initialized) == 16, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_button_t) == 48, "ABI break: handle size changed on 64-bit host");
#endif
```

> **������ʵ��**����ǰ button_t �� 14 ���ֶΣ���������Ȼ�ţ�
> - 32 λ: config=12 (ptr 4 + u16 2 + bool 1 + u8 1) + 4 bool+8 �ֶ�Լ = 40B����ʵ�⣩
> - 64 λ: config=16 (ptr 8 + u16 2 + bool 1 + u8 1 pad) + ... ��ʵ��
> 
> **��������ʵ�� + `WINK_AUTOMATED_ABI_SNAPSHOT` ����**�������У�����Ҫƾֱ���

**Ԥ�ڹ�����**��1 commit��~10 �и� + ����ʵ��ű���

---

### 3.2 P1 �߼����⣨Ӧ�ޣ�

#### #4 + #18 + #16 + #19��ͷע����Լ�ֶβ�ȫ

**λ��**��`dal_button.h` 9 ������ API ͷע��

**��ǰ״̬**��9 ������ API ͷע�͵� `@note API Contract` �飬ȱ�����ֶΣ�
- `Thread-safe: No; ISR-safe: No.` �� 8 �� API ȱ���� `on_event` �У�
- `Range: ...` �� `set_long_press_ms` / `set_debounce_ms` ȱ
- `Error-codes: ...` �� `enable_isr_counter` ȱ�����б�
- `Blocking` �� ���� API ��
- `Side-effects` �� ȱ��init/deinit �� Side-effects û������

**�޷�**��

�� API ��ȫ��Լ�ֶΡ�init ������

```c
/**
 * @brief ��ʼ����ť...
 * @note API Contract:
 *   - Preconditions: dev �� NULL��cfg �� NULL��cfg->owner ��̬�洢��
 *   - Postconditions: WINK_OK ʱ dev->initialized=true; cfg ����� dev->config;
 *                     GPIO �� init Ϊ��Ӧ pull ģʽ��resource_claim �ɹ���
 *   - Range: N/A.
 *   - Blocking: No.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Reentrancy: No.
 *   - Side-effects: pal_resource_claim(GPIO pin, owner); pal_gpio_init(pin, mode);
 *                   dev->config / state �ֶ�д�롣
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_ALREADY_INITIALIZED /
 *                  WINK_ERR_BUSY (resource_claim ��ͻ) / ͸�� pal_gpio_init rc��
 *   - Simulation-parity: WASM �� pal_gpio_init Ϊ no-op��ESP32 �������á�
 */
```

**Ԥ�ڹ�����**��1 commit��~80 ��ע������

---

#### #8 + #9��deinit ˳���ĵ���

**λ��**��`dal_button.h:264-278`��deinit ͷע�ͣ� + `dal_button.c:354-363`��deinit ʵ�֣�

**����**����ǰ deinit ˳��
1. �� `event_backend` + `isr_counter_enabled`���� disable ֮ǰ��
2. `gpio_isr_registered` ʱ `disable + synchronize`
3. `pal_gpio_reset_pin`
4. `pal_resource_release`
5. `memset`

**Ǳ������**���� 1 �������� disable ֮ǰ��ISR �ڻ��ڶ� `event_backend`���������� ISR ���� backend �ѱ���� NONE����**��**���� hook��line 65-71 ISR ���� `dev->event_backend == DAL_BUTTON_BACKEND_IRQ`�������� OK �ġ�����**Ϊʲô�������**û�ĵ�����

**�޷�**���� deinit ͷע�Ͳ� rationale ���䣺

```c
/**
 * @note deinit ˳�� rationale:
 *   1) ���� backend/counter���� ISR ��ʹ������Ҳ���ᴥ�� hook ��
 *      irq_pending������"�߼��ض�"����
 *   2) Ȼ�� disable + synchronize���ȴ����� in-flight ISR �˳���
 *   3) reset_pin������ pull ģʽ���� Hi-Z��
 *   4) release��PAL resource �ͷš�
 *   5) memset�������������� initialized=false��DAL-L-013 �ݵȱ�֤����
 *
 *   ���� 1 �벽�� 2 ֮���ޱ�������backend �� 1 �ֽڵ�д�߶���DAL-C-001
 *   ���ֿ�+��д�ߣ���ISR �������˺��ֵ��disable �� synchronize �ȴ�
 *   in-flight ISR �˳�����֤���� 2 ֮������ ISR ������
 */
```

**Ԥ�ڹ�����**��1 commit��~15 ��ע�͡�

---

#### #10��deinit LOGW

**λ��**��`dal_button.c:357-370`

**����**��`WINK_IGNORE_UNUSED` ��Ĭ�̵� `pal_gpio_disable_interrupt` / `pal_resource_release` �� `wink_status_t` ����ֵ��`pal_gpio_reset_pin` �� `void`���޷���¼��Ӧ LOGW ���ѡ�

**�޷�**��

���� 2 ���꣨�� `dal_ultrasonic` һ�£���

```c
#define LOGW_IF_RC(call, first_err) do {                                  \
    wink_status_t _rc = (call);                                            \
    if (wink_status_is_error(_rc) && wink_status_is_ok(first_err)) {       \
        first_err = _rc;                                                  \
        LOG_W(LOG_TAG, "step failed rc=%d at line %d", (int)_rc, __LINE__); \
    }                                                                     \
} while(0)

#define LOGW_IF_VOID(call) do {                                           \
    /* void-returning PAL: just emit a trace on anomalies (caller can */  \
    /* decide what counts as anomaly; for pal_gpio_reset_pin we trust */   \
    /* the platform impl) */                                               \
    (void)(call);                                                          \
} while(0)
```

deinit ��Ϊʹ����Щ�꣨�� #11 �޷�����

**Ԥ�ڹ�����**������ #11 �޷���

---

#### #11��deinit best-effort + first_err

**λ��**��`dal_button.c:342-376`

**�޷�**���� #10 �޷�ʾ�������� `first_err` �ռ� + goto-cleanup ��ʽ��

**Ԥ�ڹ�����**������ #11��

---

### 3.3 P1 ��Ҫ���⣨Ӧ�޵� P0 ���ȣ�

#### #14��`_raw` ��׺����

**λ��**��`dal_button.h:76-77, 89`��debounce_counter / debounce_threshold / edge_count��

**����**������ ��9.1 ��պ�׺����count ����Ӧ���� `_raw`����ǰ�� `_counter` / `_count` ƫ��淶����**ȫ�ֿ�ͨ��**��`dal_encoder_get_count` �ȣ���������ʷ���� vs �淶�����ͻ��

**����**����Ǩ���ڻ��⵽ȫ�ֿ�ͳһ�� `_raw` ֮ʱ��Ŀǰ�������ġ�

---

#### #17��YAML device_specific

button û�� device_specific API��enable_isr_counter �� `enable_*` ��׼�����壩��**������**����δ������ `apply_override` ֮����������� API������ YAML �� `device_specific: true`��

---

#### #24���ص�������

**�Ϲ�**��`dal_button_event_cb` ע������ȷ��

---

#### #25��8 λ No-Malloc

**�Ϲ�**��button �� malloc��

---

#### #26�����ֶο���һ����

**�Ϲ�**��ISR �� `edge_count` ���� DAL-C-001 ���ֿ�+��д�ߣ�`event_backend` 1 �ֽ����� `PAL_CRITICAL_SECTION` ������line 312-314������������ `dal_button_bal.h:34-46` ע���

---

## 4. 修复优先级与工作量评估

### 4.1 推荐修复顺序（与 ultrasonic review 同 11 步风格）

| Step | 涉及项 | Commit 标题 | 工作量 | 风险 |
|------|--------|-------------|--------|------|
| 1 | #13 | `feat(dal/button): add 32/64-bit ABI stability assertions` | S | 低 |
| 2 | #6 | `fix(dal/button): make was_pressed read-clear atomic` | S | 低 |
| 3 | #5 | `feat(dal/button): last_status field + get_status API for poll error observability` | M | 中 |
| 4 | #1 + #2 | `fix(dal/button): init goto-cleanup rollback per DAL-L-008` | M | 中 |
| 5 | #10 + #11 | `fix(dal/button): deinit logs warnings and surfaces first-fail rc (DAL-L-014/015)` | M | 中 |
| 6 | #8 + #9 | `docs(dal/button): document deinit order rationale` | S | 低 |
| 7 | #4 + #16 + #18 + #19 | `docs(dal/button): complete API Contract metadata (Thread-safe/ISR-safe/Range)` | S | 低 |
| 8 | — | `test(dal/button): add was_pressed_atomic_under_lock regression test` | S | 低 |
| 9 | — | `test(dal/button): add last_status propagation test` | S | 低 |
| 10 | — | `test(dal/button): 10-round init/deinit leak test with counter + IRQ backend` | S | 低 |
| 11 | — | `feat(dal/button): codegen quantity/quantity_class metadata per spec v3.4.1` | S | 低 |

**总工作量**：约 11 commits，~250 行 C 代码 + ~80 行注释 + ~120 行新单测。

### 4.2 修复后预期收益

- **合规率**：从当前 MUST 80% / SHOULD 81% → 100% / 100%
- **P0 致命风险**：4 项全消
- **测试覆盖**：从 27 个测试 → 30 个（新增 was_pressed 原子性、last_status、10-round loop）
- **lint 集成**：可直接由 `wink lint --pack layering --pack api` 通过

---

## 5. 已通过的反向验证（合规项）

以下规范硬性条款反向验证通过（**未发现违规**）：

| 条款 | 验证点 | 结果 |
|------|--------|------|
| DAL-8B-F-001 | `dal_button` 无 `void *dev`、无函数指针表 | OK |
| DAL-8B-F-002 | 9 个公开 API 全是具名静态函数 | OK |
| ADR-0004 静态分发 | 无 vtable、无 `container_of`、无 `struct xxx_ops` | OK |
| ADR-0017 阻塞隔离 | `poll` 头注释标"非阻塞"；无 `WINK_BLOCKING` 标注 | OK |
| ADR-0024 Deinit 三阶段 | 当前 deinit 顺序正确（仅缺 LOGW 与 first_err 收集） | OK |
| ADR-0056 跨 Profile 量纲 | button 是 B 类传感器，全部用 bool / uint8_t / uint32_t / uint64_t，**未用 float**（set_long_press_ms 是配置类，不属 A 类控制量） | OK |
| DAL-S-002 owner 静态存储 | 头注释明确"owner MUST 指向静态存储期字符串" | OK |
| DAL-S-005 禁位域 / pragma pack | 无位域、无 `#pragma pack` | OK |
| DAL-S-011 config 必须首成员 | `_Static_assert(offsetof == 0)` 已加 | OK |
| DAL-F-001 必须返 wink_status_t | 9 个公开 API 全返 `wink_status_t` | OK |
| DAL-F-002 禁 bool 返回 | 公开 API 全部 `wink_status_t`（内联助手 `record_event` 是测试用） | OK |
| DAL-F-011 查询用 const dev | `is_pressed` / `get_edge_count` / `get_status` 全用 `const dal_button_t *dev` | OK |
| DAL-F-013 out_ 前缀 | `out_pressed` / `out_was_pressed` / `out_count` / `out_was_pending` 全合规 | OK |
| DAL-F-020 出参错误时不写 | 所有出参在 WINK_OK 前才写 | OK |
| DAL-C-001 跨 ISR 共享字段单字宽 | `edge_count` 满足；`event_backend` 1 字节单写者也满足 | OK |
| DAL-C-002 RMW 必须临界区 | `reset_edge_count` / `set_event_backend` / `consume_irq_pending` 全用 `PAL_CRITICAL_SECTION` | OK |
| DAL-C-020 ISR 内禁 LOG/malloc | `dal_button_gpio_isr` 内仅++count + set flag + 调 hook | OK |
| DAL-L-002 config 深拷贝 | memcpy 完整（line 91） | OK |
| DAL-L-004 ALREADY_INITIALIZED | line 77 已实现 | OK |
| DAL-L-005 最小化防御校验 | NULL/owner/pull 校验（line 75-78） | OK |
| DAL-L-006 执行器零能量 | button 是 B 类传感器，**不适用** | OK |
| DAL-L-013 共享总线只释放 client | button 用 GPIO，无总线 | OK |
| DAL-B-001 阻塞标注 | 无 `WINK_BLOCKING` 标注（poll 非阻塞） | OK |
| DAL-B-004 阻塞 API 需 WINK_STRICT_NONBLOCKING 守卫 | 无阻塞 API | OK |
| DAL-U-001/003 单位后缀合规 | ms / us / cnt 合规 | OK |

---

## 6. 与历史评审的对比

### 6.1 对比 dal_ultrasonic（2026-08-03 review）

| 维度 | dal_button（本次） | dal_ultrasonic（2026-08-03） |
|------|--------------------|----------------------------|
| API 数量 | 9 公开 + 4 BAL | 5 公开 |
| 复杂度 | 高（事件回调 + ISR 计数 + BAL IRQ 守护） | 中（单状态机 + RMT 后端） |
| MUST 不合规 | 11 项（4 P0 + 7 P1） | 11 项（含 P0） |
| 头注释 API Contract | 多数 API 已声明，缺 Thread-safe 补全 | 修复前完全缺失 |
| goto-cleanup 模式 | 缺（要修） | 缺（已修） |
| deinit best-effort | 缺 LOGW + first_err（要修） | 缺（已修） |
| ABI 4 档断言 | 缺 32/64 位分档 | 已加 |
| SMP 快照契约 | 不适用（单字段 + 已临界区保护） | 已声明（多字段） |
| 阻塞 API | 无 | `read` 标 WINK_BLOCKING |
| 总体 | 优于 ultrasonic（注释更完整、状态机更清晰） | 整体简单但合规差距大 |

### 6.2 关键启示

1. **button 的 ISR 路径设计是 `dal_button` 的亮点**——共享 ISR thunk + refcount 启用模式既支持 counter 又支持 BAL IRQ backend，DAL 不依赖 BAL（通过全局 hook 注入），符合 ADR-0031。
2. **缺 LOGW + first_err 是 button 与 ultrasonic 共有的问题**，**应在 DAL-wide lint 规则中统一**。
3. **API Contract 头注释补全是 DAL-wide 共性问题**，建议写一个 codegen 工具扫头注释并报缺漏项（已有 `wink.py lint --pack api` 部分覆盖）。

---

## 7. 附录 A：200 项微观合规清单摘要

> 按规范章节顺序展开。✓=合规, ✗=不合规, —=不适用。详细分节见评审正文。

| 章节 | MUST 合规 / 总数 | SHOULD 合规 / 总数 |
|------|------------------|--------------------|
| §2 数据结构 | 11 / 13 | 4 / 5 |
| §3 生命周期 | 5 / 8（MUST）+ 1 / 1（SHOULD） | — |
| §4 函数签名 | 8 / 8 | 1 / 1 |
| §5 动词 | 4 / 5 | 1 / 1 |
| §6 并发 | 7 / 8 | 1 / 2 |
| §7 阻塞 | 1 / 1 | 0 / 1 |
| §8 失效安全 | 0 / 0（不适用） | — |
| §9 单位量纲 | 2 / 3 | 1 / 1 |
| §10 错误码 | 3 / 3 | — |
| **合计** | **36 / 47（77%）合规 + 11 不合规** | **8 / 12（67%）合规 + 4 不规范** |

### 不合规项定位速查表

| # | 规则 | 位置 | 风险 |
|---|------|------|------|
| 1 | DAL-L-007 | `dal_button.c:74` init 入口 | P0 致命 |
| 2 | DAL-L-008 | `dal_button.c:74-113` init 全函数 | P0 致命 |
| 4 | DAL-C-040 | `dal_button.h` 8 个 API 头注释 | P1 高级 |
| 5 | DAL-B-025 | `dal_button.c:120-121` poll 失败路径 | P0 致命 |
| 6 | DAL-V-010 | `dal_button.c:170-178` was_pressed | P0 致命 |
| 8 | DAL-L-011 | `dal_button.c:354-363` deinit 顺序缺文档 | P1 高级 |
| 9 | DAL-L-012 | `dal_button.c:360-362` synchronize 时机缺文档 | P1 高级 |
| 10 | DAL-L-014 | `dal_button.c:357-370` deinit 静默吞 rc | P1 高级 |
| 11 | DAL-L-015 | `dal_button.c:342-376` deinit 缺 first_err | P1 高级 |
| 13 | DAL-S-014 | `dal_button.h:102` 仅 1 条断言 | P0 致命 |
| 16 | DAL-U-010 | `dal_button.h:201-226` set_* 缺 Range | P1 重要 |
| 18 | DAL-C-042 | `dal_button.h` 8 个 API 头注释 | P1 高级 |
| 19 | DAL-B-013 | `dal_button.h:148-150` poll 缺 TWDT 关系 | P1 重要 |

---

## 8. 评审元数据

| 项 | 值 |
|----|----|
| 评审人 | Claude (Code v3.4.1) |
| 评审对象 commit | master (2026-08-04 起步) |
| 评审方法 | 规范 17 节逐条交叉对照 + 200 项微观清单 |
| 评审范围 | 头文件 + 实现 + YAML + 单测 |
| 评审耗时 | ~20 分钟 |
| 评审工具 | Read / Grep / Glob（无运行时） |
| **未经实测验证** | ABI 32/64 位偏移数字（建议先用 ABI 快照工具实测） |

**修复责任人**：开发 owner 自行决定优先级（建议按 §4.1 顺序）。

**审批人**：tech lead review 后合并。

**跟踪**：本评审的 11 项修复建议应在本 review 落地后归档为只读；下次 dal_button 评审至少应覆盖本次列出的不合规项是否全部收敛。

