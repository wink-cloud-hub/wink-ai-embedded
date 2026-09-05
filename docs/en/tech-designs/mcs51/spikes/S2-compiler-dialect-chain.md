# Spike-S2 结论报告：三编译器方言链（GCC / MSVC / emcc）

| 项 | 内容 |
|---|---|
| Spike 编号 | S2（M0-3） |
| 日期 | 2026-08-27 |
| 状态 | ✅ 结论已出（三端实证） |
| 验证环境 | GCC：MinGW g++ 14.2.0；MSVC：VS 2022 Enterprise `cl` 14.40.33807（/std:c++17）；emcc：4.0.5 + Node v22.19.0 |
| PoC 资产 | `docs/tech-designs/mcs51/spikes/assets/s2/`（`user_blinky.c` 未改 Keil 源、`s2_cleanup.py` 清洗 Pass、`shim/` 方言头、`s2_harness.cpp`、`build_msvc.bat`；build 产物不入库） |
| 消费里程碑 | M1（`frameworks/mcs51/CMakeLists.txt`、REGX52.H、清洗 Pass 接入） |

---

## 1. 问题与裁决

**问题**：用户 Keil C 源（C89 + `sfr/sbit/code/xdata/interrupt N/using N` 方言）须在 GCC、MSVC、emcc 三端以 C++17 编译链接，源码零改动。需裁决：(1) 用户 `.c` 如何激活 C++ 模式（MSVC 不认 `-x c++`）；(2) 分编译器参数链；(3) 正则清洗 Pass 落点与严格性；(4) 是否需 `-fpermissive`。

**裁决一句话**：**清洗 Pass 把用户源输出为 `.cpp` 副本到 build dir，三端均按原生 C++17 编译——无需 `-x c++`、无需 MSVC `LANGUAGE CXX` 属性**；方言全部由 REGX52.H 宏 + C++ 代理承接；**禁 `-fpermissive`**（三端均不需要）。

## 2. 关键证据（三端运行输出一致）

未修改的 Keil 源（含 `sbit LED = P1^0;`、`unsigned char code seg_table[]`、`void Timer0_ISR(void) interrupt 1 using 1`、`P1|=0x01` RMW），经 `s2_cleanup.py` 清洗（1 处 ISR 重写）后三端编译链接运行：

```
GCC   : [s2] PASS: WINK_ISR auto-reg + C linkage + sbit toggle OK (P1=0x01)   EXIT=0
emcc  : [s2] PASS: WINK_ISR auto-reg + C linkage + sbit toggle OK (P1=0x01)   EXIT=0
MSVC  : [s2] PASS: WINK_ISR auto-reg + C linkage + sbit toggle OK (P1=0x01)   MSVC_EXIT=0
```

验证点：WINK_ISR 静态结构体自注册（vector 1 在 main 前填好）、ISR/主函数 C 链接未修饰、`sbit` 位翻转经代理写入 SFR 影子。

**复现命令**（仓库根目录）：

```bash
A=docs/tech-designs/mcs51/spikes/assets/s2
mkdir -p $A/build
python $A/s2_cleanup.py $A/user_blinky.c $A/build/user_blinky.cpp
# GCC
g++ -std=c++17 -Wall -Wextra -Wno-write-strings -I $A/shim \
    $A/build/user_blinky.cpp $A/s2_harness.cpp -o $A/build/s2_gcc.exe && $A/build/s2_gcc.exe
# emcc (wasm/node)
emcc -std=c++17 -O1 -Wno-write-strings -s ENVIRONMENT=node -s ALLOW_MEMORY_GROWTH=1 \
     -I $A/shim $A/build/user_blinky.cpp $A/s2_harness.cpp -o $A/build/s2_wasm.js && node $A/build/s2_wasm.js
# MSVC (需 vcvars64 环境)
cmd //c "$A/build_msvc.bat"
```

## 3. 三编译器参数表（M1 CMakeLists 直接消费）

| 用途 | GCC / Clang / emcc | MSVC (`cl`) |
|---|---|---|
| C++ 标准 | `-std=c++17` | `/std:c++17` |
| 语言激活 | **不需要 `-x c++`**（清洗输出已是 `.cpp`） | **不需要 `LANGUAGE CXX`**（`.cpp` 原生 C++） |
| 字符串字面量→`char*` | `-Wno-write-strings` | `/Zc:strictStrings-`（或默认宽松，实测 `/W3` 不报错） |
| 指针符号差异 | ⚠️ **不要加 `-Wno-pointer-sign`**——该标志仅 C/ObjC 有效，C++ 下告警"valid for C but not C++" | 无对应项（C++ 不报） |
| 异常（沙箱 .cpp 自身） | `-fno-exceptions -fno-rtti -fno-threadsafe-statics` | `/EHsc-` 风格按 ADR-0036 裁剪（用户 app 侧保留 `/EHsc`） |
| 宽松降级 | **禁 `-fpermissive`**（emcc/clang 忽略/弱支持，致 host 假通过） | 无对应项 |
| 编码 | 不硬编码 `-finput-charset`；wink-tools 前置探测转 UTF-8（GBK 见 §5） | `/utf-8`（源已转 UTF-8 后加） |

> CMake 分支仿 `frameworks/arduino/CMakeLists.txt:54-60`：`if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|Emscripten") ... elseif(MSVC) ...`。

## 4. 关键机制发现

1. **`.cpp` 副本消歧语言标志**：原方案"用户 `.c` + `-x c++`（GCC）/ `set_source_files_properties(LANGUAGE CXX)`（MSVC）"双轨麻烦且易错。改为清洗 Pass 直接输出 `user_blinky.cpp`，三端 CMake 都按 CXX 原生编译，**语言标志链彻底统一**。用户源仍以 `.c` 存档（零侵入证明），编译的是 build dir 里的 `.cpp` 副本。

2. **MSVC 抓出真跨编译器缺陷（C/C++ 链接不匹配）**：`s_sfr_shadow` 在 `mcs51_proxy.hpp` 中初版写成 C++ 链接 `extern uint8_t [...]`，harness 按 `extern "C"` 定义——GCC/emcc 宽松链接通过，**MSVC 严格报 LNK2019（修饰名 `?s_sfr_shadow@@3PAEA` 不匹配）**。裁决：**SFR 影子 / trap 表 / ISR 表等跨 TU C-ABI 边界一律 `extern "C"` 声明**（与边界③④一致）。此问题只在 MSVC 暴露，印证三编译器矩阵的必要性。

3. **`P1^0` 重载歧义**：`operator^(uint8_t)` 与内建 `operator^(int,int)`（经 `operator uint8_t()` 转换）对字面量 `0`（int）歧义，GCC 警告。修：**位代理访问算子参数用 `int`**（`WinkSfrBitProxy operator^(int b)`），成员函数精确匹配优先于内建，歧义消除。M1 写 `WinkSfr::operator^` 时按此签名。

4. **WINK_ISR 静态注册三端一致**：标准 C++ 匿名命名空间 + 静态结构体构造器（不用 `__attribute__((constructor))`），MSVC/GCC/emcc 均在 main 前完成 vector 注册，行为一致。

5. **正则清洗严格性（R-006）**：模式 `void\s+(\w+)\s*\(\s*(?:void)?\s*\)\s*interrupt\s+(\d+)(?:\s+using\s+\d+)?` 实测：
   - ✅ 匹配 `void f(void) interrupt 1`、`void f() interrupt 4 using 2`；
   - ✅ 不匹配非 void 参数 `void bad(int x) interrupt 2`（非法 Keil，留给编译器报错）；
   - ✅ 不匹配普通函数、不匹配注释里的 `interrupt` 单词；
   - ⚠️ **残留**：注释中若出现**完整模式文本**（如 `// void f(void) interrupt 1`）仍会被替换。缓解：清洗 Pass 前先做注释/字符串剥离（或接受——真实工程注释里极少出现完整合法 ISR 签名；M1 实现时加注释预剥离更稳）。不匹配时 CMake `FATAL_ERROR` 提示用户。

## 5. GBK 编码探针结论

- 硬编码 `-finput-charset=GBK` 会误杀 UTF-8 源（SSOT 已禁）。
- 裁决：编码探测→转 UTF-8 作为 **wink-tools 构建前置步骤**（与 codegen 同级的 build pre-step），在正则清洗之前运行；清洗 Pass 与编译器只面对 UTF-8。MSVC 侧加 `/utf-8`。
- 真实 GBK 工程（含中文注释）的转码无损验证留待 S3 采集真实工程 fixture 后回归（E-003）。

## 6. 可复用产物（M1 直接消费）

- **清洗 Pass**：`s2_cleanup.py`（Python，严格正则 + `.cpp` 输出 + 计数日志）→ M1 用 CMake `add_custom_command` 调用，输出到 `${CMAKE_CURRENT_BINARY_DIR}/mcs51_generated/`，**严禁原地改源**。
- **方言头**：`shim/REGX52.H`（系统头预引入 + main 重映射 + 方言宏 + `sfr/sbit` inline 降级）、`shim/wink_mcs51_isr.h`（WINK_ISR 静态注册宏）→ M1 生产版补全 SFR 全集与 `extern "C"` 边界。
- **参数表**：§3 表格直接落入 `frameworks/mcs51/CMakeLists.txt` 分编译器分支。
- **CMake 片段要点**：
  ```cmake
  # 用户 app：清洗 .c -> build dir .cpp，三端原生 C++17
  add_custom_command(OUTPUT ${gen}/<app>.cpp
      COMMAND ${Python3_EXECUTABLE} <mcs51>/tools/s2_cleanup.py <src>.c ${gen}/<app>.cpp
      DEPENDS <src>.c)
  add_library(wink_mcs51_user_app STATIC ${gen}/<app>.cpp)
  target_include_directories(wink_mcs51_user_app PRIVATE <mcs51>/include)
  if(MSVC)
      target_compile_options(wink_mcs51_user_app PRIVATE /std:c++17 /utf-8)
  else()
      target_compile_options(wink_mcs51_user_app PRIVATE -std=c++17 -Wno-write-strings)
  endif()
  ```

## 7. 回写点（影响计划任务行）

| 任务 | 回写内容 |
|---|---|
| M1 CMakeLists | 用户源经清洗 Pass 输出 `.cpp`，**放弃 `-x c++`/`LANGUAGE CXX` 双轨**；按 §3 参数表分支；清洗脚本 `add_custom_command` 落 build dir |
| M1 REGX52.H / mcs51_proxy.hpp | SFR 影子等 C-ABI 边界一律 `extern "C"`；`WinkSfr::operator^` 参数用 `int` 消歧 |
| M1 WINK_ISR | 沿用静态结构体注册（三端实证一致） |
| R-001（MSVC 编译断裂） | **退役**——三端实证通过；残留仅注释剥离加固（低危） |
| R-006（正则误伤） | 主体退役（严格模式实测）；注释预剥离列为 M1 加固项 |
| M3 GBK | 编码前转 UTF-8 归 wink-tools pre-step；真实 GBK 工程回归随 S3 fixture |

---

*Spike 结论在 M1 落地后，可将参数表与清洗片段并入总纲 SSOT §2.2 并归档。*
