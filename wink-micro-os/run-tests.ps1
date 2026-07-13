<#
.SYNOPSIS
  wink-micro-os host tests (unit + end-to-end) one-click runner.
.DESCRIPTION
  Builds and runs all host tests with WinLibs MinGW (gcc) + cmake on your PC.
  No real hardware / browser needed.

  Default: single build in ../build/test/ (fast, daily iteration).

  With -Full (or -Sanitize), runs the extended test matrix that Phase 1.5 DoD
  requires as a regression net for PAL IRQ contracts:

    Pass 1  ../build/test/         default host build (no sanitizers)
    Pass 2  ../build/test-san/     -fsanitize=undefined + -Wcast-function-type
                                (UBSan trap-on-error, plus GCC's static approximation
                                 of clang -fsanitize=cfi-icall)

  Usage:
    pwsh ./run-tests.ps1                # fast daily iteration (default build only)
    pwsh ./run-tests.ps1 -Clean         # wipe & rebuild default build only
    pwsh ./run-tests.ps1 -Detailed      # print each test exe's full output
    pwsh ./run-tests.ps1 -Sanitize      # add sanitize matrix (adds Pass 2)
    pwsh ./run-tests.ps1 -Full          # sanitize matrix (CI / pre-PR gate)
    pwsh ./run-tests.ps1 -WithWasm      # run optional WASM build compilation check

  NOTE: The historical -Optin pass (WINK_HOST_ALLOW_REALTIME_FOR_TESTING) was
        removed by ADR-0018 (2026-07-02). The PAL_IRQ_PRIO_REALTIME enum no
        longer exists, so no code path consumes that macro.
#>
[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$Detailed,
    [switch]$Sanitize,
    [switch]$Full,
    [switch]$WithWasm
)

$ErrorActionPreference = 'Stop'

if ($Full) { $Sanitize = $true }

# ---- 1. Verify toolchain present (gcc/cmake must be on PATH) ----
# Hardcoded toolchain prepends have been removed; the caller is expected to
# have gcc/cmake on PATH already (e.g. WinLibs MinGW-w64 activated, or via
# `python tools/wink.py ensure-host` bootstrap). See tools/preinstall.md.
foreach ($t in 'gcc','cmake') {
    if (-not (Get-Command $t -ErrorAction SilentlyContinue)) {
        Write-Host "[FAIL] '$t' not found on PATH." -ForegroundColor Red
        Write-Host "       Run 'python tools/wink.py doctor' to diagnose missing toolchain," -ForegroundColor Red
        Write-Host "       or see tools/preinstall.md for setup instructions." -ForegroundColor Red
        exit 1
    }
}

# ---- 2. Verify gcc is a MinGW-w64 build on Windows (catch MSYS2/Strawberry contamination) ----
if ($IsWindows -or $env:OS -eq 'Windows_NT') {
    $dumpMachine = (& gcc -dumpmachine 2>$null)
    if ($LASTEXITCODE -eq 0 -and $dumpMachine -and ($dumpMachine -notmatch 'w64-mingw32')) {
        Write-Host "[FAIL] gcc on PATH is not a MinGW-w64 build (dumpmachine: $dumpMachine)." -ForegroundColor Red
        Write-Host "       Run 'python tools/wink.py doctor' to diagnose, or see tools/preinstall.md." -ForegroundColor Red
        exit 1
    }
}

# ---- 3. cd to this script's dir (i.e. wink-micro-os/) ----
Set-Location $PSScriptRoot

# ---- 4. Helper: build + test one variant ----
function Invoke-TestPass {
    param(
        [Parameter(Mandatory)][string]$Label,
        [Parameter(Mandatory)][string]$BuildDir,
        [string]$ExtraCFlags = ''
    )

    Write-Host "`n===== [$Label] cmake configure ($BuildDir) =====" -ForegroundColor Cyan

    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Host "-> Cleaning $BuildDir ..." -ForegroundColor Yellow
        Remove-Item -Recurse -Force $BuildDir
    }

    $cmakeArgs = @('-B', $BuildDir, '-DTARGET_PLATFORM=host')
    if ($ExtraCFlags) {
        # Pass sanitize / opt-in flags via CMAKE_C_FLAGS. -fno-sanitize-recover
        # keeps UBSan noisy (any UB → nonzero exit). Cast-function-type-strict is
        # GCC's static approximation of clang's -fsanitize=cfi-icall (mismatched
        # function pointer casts fail at build time, catching the exact G1 regression).
        $cmakeArgs += "-DCMAKE_C_FLAGS=$ExtraCFlags"
    }

    cmake @cmakeArgs *> $null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] [$Label] configure failed, re-running with full output:" -ForegroundColor Red
        cmake @cmakeArgs
        return $false
    }

    Write-Host "-> [$Label] Building ..." -ForegroundColor Cyan
    cmake --build $BuildDir 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] [$Label] build failed" -ForegroundColor Red; return $false }

    Write-Host "-> [$Label] Running tests ..." -ForegroundColor Cyan
    Push-Location $BuildDir
    try {
        if ($Detailed) { ctest --output-on-failure -V } else { ctest --output-on-failure }
        $rc = $LASTEXITCODE
    } finally { Pop-Location }

    if ($rc -eq 0) {
        Write-Host "[PASS] [$Label] all tests passed" -ForegroundColor Green
        return $true
    } else {
        Write-Host "[FAIL] [$Label] some tests failed" -ForegroundColor Red
        return $false
    }
}

# ---- 5. Execute matrix ----
$passes = @()
$passes += @{ Label='default'; Dir='../build/test';       Flags=''; Enabled=$true }
# NOTE: ADR-0018 (2026-07-02) removed PAL_IRQ_PRIO_REALTIME. The historical
#       -Optin pass with -DWINK_HOST_ALLOW_REALTIME_FOR_TESTING=1 is retired;
#       no code path consumes that macro anymore.
# Sanitize pass:
#   -fsanitize=undefined              : UBSan checks (invalid casts, int overflow,
#                                        misaligned access, etc.)
#   -fsanitize-undefined-trap-on-error: on UB, call __builtin_trap() instead of
#                                        libubsan diagnostics — WinLibs MinGW does not
#                                        ship libubsan.a, so this variant is required.
#                                        Tradeoff: on failure ctest reports "abnormal
#                                        program termination"; re-run the single test
#                                        under gdb to locate the exact UB.
#   -Wcast-function-type              : GCC's static approximation of clang
#                                        -fsanitize=cfi-icall (mismatched function
#                                        pointer casts, e.g. the (pal_isr_t)handler
#                                        cast that G1 removed, become build warnings).
#   -Werror=cast-function-type        : promote it to a hard error → permanent regression
#                                        guard against reintroducing G1-class casts.
# NOTE: clang -fsanitize=cfi-icall provides stronger, link-time guarantees but is
#       unavailable in the current WinLibs MinGW GCC 16.1 toolchain. Switch to clang
#       (or add a second sanitize matrix pass) if that check is required.
# NOTE: -Wcast-function-type-strict does NOT exist in GCC; that spelling is clang-only.
$passes += @{ Label='sanitize'; Dir='../build/test-san';  Flags='-fsanitize=undefined -fsanitize-undefined-trap-on-error -Wcast-function-type -Werror=cast-function-type'; Enabled=$Sanitize }

$overallRc = 0
foreach ($p in $passes) {
    if (-not $p.Enabled) { continue }
    $ok = Invoke-TestPass -Label $p.Label -BuildDir $p.Dir -ExtraCFlags $p.Flags
    if (-not $ok) { $overallRc = 1 }
}

# ---- 5.5 WASM build check (optional compilation verification) ----
if ($WithWasm) {
    Write-Host "`n===== [wasm build check] =====" -ForegroundColor Cyan

    # Resolve emcc / emcmake without any hardcoded fallback path:
    #   1) If emcc + emcmake are already on PATH (activated shell), use them directly.
    #   2) Else if $env:EMSDK is set and points at a valid emsdk tree,
    #      dot-source emsdk_env.ps1 to activate, then re-probe.
    #   3) Else FAIL this pass and instruct the user to activate emsdk themselves.
    $emccReady = ($null -ne (Get-Command emcc -ErrorAction SilentlyContinue)) -and `
                 ($null -ne (Get-Command emcmake -ErrorAction SilentlyContinue))

    if (-not $emccReady -and $env:EMSDK -and (Test-Path $env:EMSDK)) {
        $envScript = Join-Path $env:EMSDK 'emsdk_env.ps1'
        if (Test-Path $envScript) {
            Write-Host "-> Activating EMSDK environment from $env:EMSDK ..." -ForegroundColor Cyan
            $env:EMSDK_QUIET = 1
            . $envScript
            $emccReady = ($null -ne (Get-Command emcc -ErrorAction SilentlyContinue)) -and `
                         ($null -ne (Get-Command emcmake -ErrorAction SilentlyContinue))
        }
    }

    if (-not $emccReady) {
        Write-Host "[FAIL] emcc not found on PATH and EMSDK env is not set." -ForegroundColor Red
        Write-Host "       Activate emsdk in your shell first (emsdk_env.ps1), or run" -ForegroundColor Red
        Write-Host "       'python tools/wink.py doctor' to diagnose." -ForegroundColor Red
        $overallRc = 1
    } else {
        $oldPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            # Default app for smoke = avoidance_car → build/wasm/{projectCode}/
            $wasmBuildDir = "../build/wasm/avoidance_car"
            if ($Clean -and (Test-Path $wasmBuildDir)) {
                Write-Host "-> Cleaning $wasmBuildDir ..." -ForegroundColor Yellow
                Remove-Item -Recurse -Force $wasmBuildDir
            }

            Write-Host "-> Configuring WASM build..." -ForegroundColor Cyan
            $LASTEXITCODE = 0
            emcmake cmake -B $wasmBuildDir -DTARGET_PLATFORM=wasm `
                "-DWINK_APP_DIR=$((Resolve-Path ../wink-micro-app/avoidance_car).Path)"
            if ($LASTEXITCODE -ne 0) {
                Write-Host "[FAIL] WASM configure failed" -ForegroundColor Red
                $overallRc = 1
            } else {
                Write-Host "-> Building WASM target..." -ForegroundColor Cyan
                cmake --build $wasmBuildDir
                if ($LASTEXITCODE -ne 0) {
                    Write-Host "[FAIL] WASM build failed" -ForegroundColor Red
                    $overallRc = 1
                } else {
                    Write-Host "[PASS] WASM build check succeeded" -ForegroundColor Green
                }
            }
        } finally {
            $ErrorActionPreference = $oldPreference
        }
    }
}


Write-Host ""
if ($overallRc -eq 0) {
    Write-Host "[PASS] All enabled test passes succeeded" -ForegroundColor Green
} else {
    Write-Host "[FAIL] One or more test passes failed (see output above)" -ForegroundColor Red
}

# ---- 6. L0 static lint: targets/esp32/*.c ESP_PLATFORM guard density ---------
# Task 3 (PLAN-20260701-PAL-TARGET-P1-MAINT): TUs under targets/esp32/ are
# compiled only when TARGET_PLATFORM=esp32; internal ESP_PLATFORM guards are
# dead code. R-4 red line: the OUTERMOST guard wrapping IDF-private headers
# (e.g. driver/gpio.h) MUST remain so IDE non-IDF opens don't fail parsing.
# Threshold: at most 1 #if defined(ESP_PLATFORM) per file.
# Runs AFTER host tests so a lint regression does not gate the test signal.
# ---- P1 保护：targets/esp32/*.c 内 ESP_PLATFORM 出现次数 ≤ 1 -----------------
# NOTE: cwd is $PSScriptRoot (wink-micro-os/), so path is relative to that.
# Regex anchor rationale: only count REAL preprocessor directives — lines that
# START with optional whitespace + `#` + optional space + `if`. This mirrors
# how the C preprocessor sees directives, so R-4 documentation comments like
# `* ✅ R-4：全文件仅 1 处最外层 `#if defined(ESP_PLATFORM)` ...` (leading `*`
# from a block comment) or `// #if defined(ESP_PLATFORM)` are correctly
# ignored. `#\s*if` tolerates the rare `# if defined(...)` spelling.
# Scope is intentionally narrow: `#ifdef ESP_PLATFORM` / `#if ESP_PLATFORM`
# are not currently used in-tree (per Task 3 audit) and are out of scope.
$violations = @()
Get-ChildItem targets/esp32/*.c | ForEach-Object {
    $count = (Select-String -Path $_.FullName -Pattern '^\s*#\s*if\s+defined\(ESP_PLATFORM\)').Count
    if ($count -gt 1) {
        $violations += "$($_.Name): $count occurrences (limit: 1)"
    }
}
if ($violations.Count -gt 0) {
    Write-Error "ESP_PLATFORM guard limit exceeded:`n$($violations -join `"`n`")"
    exit 1
}
Write-Host "[lint] ESP_PLATFORM guard density OK"

# ---- 7. L1 static lint: ADR-0017 WINK_STRICT_NONBLOCKING symbol elision -----
# Compile dal_ultrasonic.c standalone under -DWINK_STRICT_NONBLOCKING=1 and
# assert `nm` reports NO T-defined `dal_ultrasonic_read` symbol. This is the
# link-time layer of the three-layer hard isolation defined in ADR-0017:
# the header wraps the declaration in `#ifndef WINK_STRICT_NONBLOCKING` and
# the implementation follows suit, so under the strict flag the symbol MUST
# disappear from the translation unit. Cooperative-scheduler builds (T5)
# will link against libraries built with this flag; if the symbol survives
# here it would silently reappear there.
# Runs standalone (no full CMake re-configure) so E-3 stays cheap.
$dalC   = Join-Path $PSScriptRoot 'dal/src/sensor/dal_ultrasonic.c'
$strictObj = Join-Path $env:TEMP  'wink_strict_nonblocking_check.o'
$includes = @(
    '-I', (Join-Path $PSScriptRoot 'pal/include'),
    '-I', (Join-Path $PSScriptRoot 'pal/include/hal'),
    '-I', (Join-Path $PSScriptRoot 'pal/include/osal'),
    '-I', (Join-Path $PSScriptRoot 'dal/include'),
    '-I', (Join-Path $PSScriptRoot 'dal/include/sensor'),
    '-I', (Join-Path $PSScriptRoot 'trace/include'),
    '-I', (Join-Path $PSScriptRoot 'targets/host/include'),
    # 2026-07-04 P1-P2: wink_pt_debug.h(WINK_ASSERT_NONBLOCKING) 已迁至 runtime/include/。
    # dal_ultrasonic.c 现在 #include "wink_pt_debug.h"，独立 lint 编译需要能找到它。
    '-I', (Join-Path $PSScriptRoot 'runtime/include')
)
# -c: compile only (no link). -DWINK_STRICT_NONBLOCKING=1: the flag whose
# semantics we are verifying. Any include-path miss here means the assertion
# is meaningless — surface it loudly instead of silently passing.
$gccArgs = @('-c', '-DWINK_STRICT_NONBLOCKING=1') + $includes + @($dalC, '-o', $strictObj)
& gcc @gccArgs 2>&1 | Out-Host
if ($LASTEXITCODE -ne 0) {
    Write-Error "[lint] ADR-0017 L1: strict-mode compile of dal_ultrasonic.c failed (expected to succeed with the symbol elided)"
    exit 1
}
# `nm -g --defined-only` lists only externally-visible defined symbols.
# Match the exact symbol name at word boundary so a hypothetical
# `dal_ultrasonic_read_something` wouldn't false-positive.
$nmOut = & nm -g --defined-only $strictObj 2>&1
if ($nmOut -match '\bdal_ultrasonic_read\b') {
    Remove-Item -Force $strictObj -ErrorAction SilentlyContinue
    Write-Error @"
[lint] ADR-0017 L1 FAILED: dal_ultrasonic_read is still defined under
-DWINK_STRICT_NONBLOCKING=1. The #ifndef guard in dal_ultrasonic.c must
wrap the FULL function body. Offending symbol table:
$nmOut
"@
    exit 1
}
Remove-Item -Force $strictObj -ErrorAction SilentlyContinue
Write-Host "[lint] ADR-0017 L1: dal_ultrasonic_read absent under strict mode OK"

# ---- 8. L2 static lint: P1-B2 header self-containment ------------------------
# Every public header under pal/include/ and dal/include/ must compile as the
# first-and-only #include in an empty TU (both C and C++). Catches missing
# prerequisite includes (<stdint.h>, <stdbool.h>, "wink_status.h", ...) that
# would otherwise silently rely on transitive inclusion at downstream callers.
# Runs after other lints so a header regression does not gate the test signal.
# Soft-skips when python is unavailable (dev machine without Python installed).
Write-Host "[lint] Header self-containment (P1-B2)..." -ForegroundColor Cyan
$pythonCmd = Get-Command python -ErrorAction SilentlyContinue
if ($pythonCmd) {
    & python (Join-Path $PSScriptRoot 'tools/lint/check_headers_self_contained.py')
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[lint] P1-B2 header self-containment check failed (see output above)"
        exit 1
    }
} else {
    Write-Host "[SKIP] python not found on PATH — header self-containment check skipped" -ForegroundColor Yellow
}

# ---- 9. L3 static lint: log format-string literal gate (P1-L1) ----------------
# PLAN-20260705-LOGGING-HARDENING: all LOG_E/W/I/D and pal_log_e/w/i/d call sites
# must pass a compile-time string literal as their fmt argument. This is a
# prerequisite for future Tokenized/Dictionary logging (Pigweed-style hash
# compression) and catches format-string-injection footguns.
Write-Host "[lint] Log format-string literal gate (P1-L1)..." -ForegroundColor Cyan
if ($pythonCmd) {
    & python (Join-Path $PSScriptRoot 'tools/lint/check_log_format_literals.py') --root $PSScriptRoot
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[lint] P1-L1 log format-literal check failed (see output above)"
        exit 1
    }
} else {
    Write-Host "[SKIP] python not found on PATH — log format-literal check skipped" -ForegroundColor Yellow
}

exit $overallRc
