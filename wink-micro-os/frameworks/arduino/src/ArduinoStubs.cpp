// SPDX-License-Identifier: LGPL-3.0-only
extern "C" {
#if defined(_MSC_VER)
void __declspec(selectany) setup(void) {}
void __declspec(selectany) loop(void) {}
#else
__attribute__((weak)) void setup(void) {}
__attribute__((weak)) void loop(void) {}
#endif
}
