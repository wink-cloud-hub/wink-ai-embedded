# ADR-0011：无栈协程局部变量"足枪"的纵深防御体系

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-06-28（提议）/ 2026-06-28（通过） |
| 触发 | 静态分析工具代码审计发现 `check_pt_variables.py` 存在高危漏检漏洞 |
| 影响范围 | App 代码生成器 / `wink_app.h` 协程宏 / 静态检查工具 `check_pt_variables.py` / 全部协程代码 |
| 决策者 | 架构委员会（2026-06-28 技术评审通过） |
| 关联设计规范 | `03-app-codegen/01-app-business-logic.md`（已更新至 State Struct 模式） |

---

## 背景（Context）

WinkMicroOS 采用 **Duff's Device 变种实现的无栈协程**（Protothread）作为 App 业务逻辑的核心执行模型。该模型的头号安全漏洞是：

> **协程 yield 后，函数栈帧被销毁，所有自动（栈）变量的值变为未定义。**

这是业界公认的 Protothread "Footgun"——代码看起来完全正常，编译也无警告，但运行时会出现随机、难以复现的 Heisenbug，且故障现场无法追溯。

### 当前防御现状

项目目前已有三层防御：

| 防线 | 实现位置 | 现状评估 |
|------|----------|----------|
| **文档警告** | `wink_app.h` 头文件注释 | ✅ 已实现，但开发者不一定会读 |
| **调试期毒化** | `WINK_PT_POISON_STACK()` 宏 | ✅ 已实现，Debug 模式下 yield 后立即用 `0xDEADBEEF` 覆盖栈 |
| **静态分析** | `wink-micro-os/tools/lint/check_pt_variables.py` | ⚠️ **存在高危漏检**——见下文 |

### 已确认的静态分析漏洞（P0 级）

通过构造 PoC 测试用例复现，当前 `check_pt_variables.py` 第 116-118 行存在严重的逻辑缺陷：

```python
# 漏洞代码：只要是 i/j/k 变量且后面紧跟 for( 就盲目放行
if var_name in ('i', 'j', 'k') and 'for(' in remainder:
    continue  # ❌ 未检查 for 循环作用域内是否包含 yield 点！
```

**PoC 漏检样例（真实危险代码，但当前检出 0 错误）：**
```c
wink_status_t dangerous_coroutine(wink_pt_t *pt) {
    WINK_PT_BEGIN(pt);
    int j;  // 变量名是 'j'
    for (j = 0; j < 5; j++) {  // 后面紧跟 for(
        WINK_PT_DELAY_MS(pt, 100);  // ⚠️ for 体内有 yield！
        printf("j = %d\n", j);  // j 第二次进入时是垃圾值！
    }
    WINK_PT_END(pt);
}
```

该漏洞的风险是：**开发者以为有静态分析保护，实际上写出了危险代码却完全不知情。**

### 额外发现的隐患

当前 `WINK_PT_STATE_*` 系列宏提供了"协程实例自带状态"的正确模式，但并未强制执行：
- `static` 局部变量在协程内虽然值不会丢失，但**不支持多实例重入**——同一个协程函数被两个 task 运行时，状态互相覆盖
- 手写 App 代码的开发者可能图省事用 `static`，为后续平台化演进埋下架构债

---

## 方案比选（Options）

### 方案 A：现状维持

什么都不做，只在文档里加一行"注意 for 循环"。

- **优点**：零成本。
- **缺点**：P0 级漏洞持续存在；开发者对静态分析的信任反而放大风险；多实例问题永久无解。
- **判定**：**不推荐**——这是把技术债务留给未来。

---

### 方案 B：升级为 LLVM/Clang AST 分析器

弃用正则匹配，改用 `clang.cindex` Python binding 直接解析 C 代码 AST，精确识别变量存储类和作用域。

- **优点**：100% 语法分析精确性。
- **缺点**：
  - 引入 `libclang` 重量级二进制依赖，Windows 下配置极其繁琐
  - 破坏"开箱即用"的开发者体验——新手需要配置 `LIBCLANG_PATH` 才能编译
  - 维护成本高——需要懂 Clang API 的专家才能调试
- **判定**：**不推荐**——典型的"过度工程化"反模式，为 5% 的边缘场景引入 95% 的复杂度。

---

### 方案 C（推荐）：纵深防御三层体系

在现有架构基础上，构建三层递进的防御体系，**成本最低、收益最大、无任何外部依赖**。

#### 第一层：修补静态分析漏洞（P0，立即修复）

修复 `check_pt_variables.py` 的 for 循环漏检逻辑：

```python
# ✅ 修复后逻辑
if var_name in ('i', 'j', 'k') and 'for(' in remainder:
    # 1. 定位 for 循环体的 { 开始位置
    for_body_start = find_for_opening_brace(content, var_end)
    # 2. 括号计数找到 for 作用域的结束位置 }
    for_body_end = find_matching_brace(content, for_body_start)
    # 3. 在 for 体内搜索 yield 标记
    has_yield = contains_any(content[for_body_start:for_body_end], [
        'WINK_PT_YIELD', 'WINK_PT_DELAY_MS', 'WINK_PT_WAIT_'
    ])
    if has_yield:
        # ⚠️ for 体内有 yield → 必须报警，不能放行
        errors.append(dangerous_for_loop_error)
    # else: for 体内无 yield → 安全，可以放行
```

同时增加检测规则：**检测协程函数内的 `static` 局部变量并报警**。

#### 第二层：强制 State Struct 模式（P1，代码生成器端）

在 Codegen 端，AI / 低代码生成的 App 代码**必须**采用以下模式，从源头消灭问题：

```c
// ✅ Codegen 强制生成的标准模式
WINK_PT_STATE_BEGIN(task_001_avoidance)
    // 所有状态变量必须在 struct 内声明
    int    task_001_counter;
    float  task_001_last_distance;
    uint8_t task_001_sm_state;
WINK_PT_STATE_END()

wink_status_t task_001_avoidance(wink_pt_t *pt) {
    WINK_PT_STATE_USE(task_001_avoidance);  // 注入 state 指针
    WINK_PT_BEGIN(pt);

    state->counter++;  // ✅ 永远安全，多实例隔离
    WINK_PT_DELAY_MS(pt, 100);

    WINK_PT_END(pt);
}
```

**命名空间保证**：所有 struct 成员必须加 `task_{NNN}_` 前缀（NNN 为节点唯一 ID），彻底避免跨协程符号冲突。

#### 第三层：编译期断言强化（可选，内核层）

在 `WINK_PT_STATE_USE` 宏中植入编译期"陷阱"，尽可能在编译期拦截 `static` 局部变量（此为锦上添花，依赖编译器实现特性，不作为硬性要求）。

### 方案 C 的额外架构红利

强制 State Struct 模式不仅解决了安全问题，还为平台长期演进打下确定性基础：

| 高级功能 | 如何实现 |
|----------|----------|
| **WebSim 状态树监控** | 前端可通过 `wink_pt_t*` 指针直接读取每个协程的完整状态，在浏览器中实时展示状态树 |
| **时间旅行调试** | 状态可序列化保存/恢复，支持"倒带"重放故障场景 |
| **深度睡眠唤醒** | 休眠前 dump 所有协程状态到 Flash，唤醒后原地恢复，协程完全无感继续执行 |
| **多实例安全性** | 同一个协程函数可同时运行多个实例，状态完全隔离 |
| **故障黑匣子** | Crash 时自动 dump 所有协程状态，事后分析精准定位 |

---

## 决策结论（Decision）

**推荐采纳方案 C：纵深防御三层体系**。

| 层级 | 优先级 | 预计工时 |
|------|--------|----------|
| 第一层：修补静态分析漏洞 | **P0 立即** | 2~4 小时 |
| 第二层：Codegen 强制 State Struct | **P1 高** | 1~2 天 |
| 第三层：编译期断言强化 | P3 可选 | 4 小时 |

**核心理由：**
1. ✅ 完全解决已确认的 P0 级漏检漏洞
2. ✅ 零外部依赖，不破坏"开箱即用"体验
3. ✅ 成本仅为方案 B 的 10%，却覆盖 99% 的实际场景
4. ✅ 顺带解决了多实例重入安全问题
5. ✅ 为仿真调试、低功耗、故障诊断等高级功能打下架构基础

---

## 后果与约束（Consequences & Constraints）

### 正向后果

1. **安全性提升**：P0 级漏检漏洞修复，静态分析可信度恢复
2. **架构升级**：从"单实例假设"升级为"原生多实例安全"
3. **平台化基础**：State Struct 模式为 WebSim、低功耗等高级功能扫清障碍

### 负向后果与迁移成本

1. **既有代码适配**：项目中现有的协程代码如果使用 `static` 局部变量，需要迁移到 State Struct 模式
2. **代码生成器改造**：低代码 / AI 代码生成器需要更新模板，从"自由变量"模式切换到"状态 struct"模式

### 约束条件

1. 修补后的静态分析器**必须**通过所有 PoC 测试用例的验证
2. State Struct 的命名前缀规则**必须**写入代码生成规范，作为强制性要求
3. `static` 变量报警**不搞一刀切**——允许开发者在确认单实例场景下加 `// NOLINT` 豁免，但必须显式声明

---

## 遵循与后续（Compliance & Follow-up）

### 验收标准

- [ ] PoC 危险代码 `int j; for (j=0; ...) { WINK_PT_DELAY_MS }` 可被正确检出
- [ ] 安全代码 `for (int i=0; ...) { /* no yield */ }` 不产生误报
- [ ] 协程内的 `static` 局部变量可被检测并报警
- [ ] Codegen 生成的所有 App 协程均采用 State Struct 模式
- [ ] 生成的状态变量带有唯一的 `task_{NNN}_` 命名前缀

### 文档回写

本 ADR 已采纳，已同步更新以下设计规范：
- ✅ `03-app-codegen/01-app-business-logic.md`——已全部更新至 State Struct 模式，含代码示例、架构对比表、Codegen 命名规则
- ⏳ `02-wink-micro-os/04-runtime-and-trace.md`——待补充协程安全模型与多实例设计（低优先级）

### 实施完成情况

P0 高优先级（✓ 已完成）：
- ✅ 修复 `check_pt_variables.py` for 循环漏检漏洞
- ✅ 新增多变量声明支持
- ✅ 新增 `static` 局部变量检测（多实例安全）
- ✅ Windows GBK 编码兼容

P1 中优先级（✓ 已完成）：
- ✅ 设计文档 `01-app-business-logic.md` 更新为 State Struct 模式
- ✅ `app_codegen.py` 代码生成器骨架实现
- ✅ `wink_app.h` 编译期安全增强（static_assert、INIT/VALID 宏）

P3 低优先级（可选）：
- ⏳ LLVM/Clang AST 分析器（ROI 太低，暂缓）

### 合规检查

本决策的执行情况通过以下机制保证：
1. **编译门禁**：`check_pt_variables.py` 在 CMake 构建时自动运行，失败则构建终止（✓ 已存在）
2. **Code Review Checklist**：协程代码的 Review 必须检查是否采用 State Struct 模式
3. **定期审计**：每季度通过脚本扫描代码库，统计不符合规范的协程数量并追踪清零

---

*本 ADR 状态变更请在此记录：*
- 2026-06-28：Proposed（漏洞分析完成，提交评审）
- 2026-06-28：Accepted（P0/P1 全部实施完成，代码与文档均已更新）
