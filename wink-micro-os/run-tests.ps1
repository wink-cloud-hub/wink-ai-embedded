<#
.SYNOPSIS
  wink-micro-os host tests (unit + end-to-end) one-click runner.
.DESCRIPTION
  Builds and runs all host tests with WinLibs MinGW (gcc) + cmake on your PC.
  No real hardware / browser needed.

  Default: single build in build-test/ (fast, daily iteration).

  With -Full (or -Sanitize + -Optin combined), runs the full test matrix that
  Phase 1.5 DoD requires as a regression net for PAL IRQ contracts:

    Pass 1  build-test/         default host build (no sanitizers, no opt-in)
    Pass 2  build-test-optin/   -DWINK_HOST_ALLOW_REALTIME_FOR_TESTING=1
                                (exercises test_irq_realtime_accepted_when_opt_in)
    Pass 3  build-test-san/     -fsanitize=undefined + -Wcast-function-type
                                (UBSan trap-on-error, plus GCC's static approximation
                                 of clang -fsanitize=cfi-icall)

  Usage:
    pwsh ./run-tests.ps1                # fast daily iteration (default build only)
    pwsh ./run-tests.ps1 -Clean         # wipe & rebuild default build only
    pwsh ./run-tests.ps1 -Detailed      # print each test exe's full output
    pwsh ./run-tests.ps1 -Optin         # add opt-in matrix (adds Pass 2)
    pwsh ./run-tests.ps1 -Sanitize      # add sanitize matrix (adds Pass 3)
    pwsh ./run-tests.ps1 -Full          # all three passes (CI / pre-PR gate)
#>
[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$Detailed,
    [switch]$Optin,
    [switch]$Sanitize,
    [switch]$Full
)

$ErrorActionPreference = 'Stop'

if ($Full) { $Optin = $true; $Sanitize = $true }

# ---- 1. Locate toolchain (WinLibs MinGW: gcc 16.1.0 + cmake 4.3.2) ----
$toolchain = "C:\Users\77174\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
if (Test-Path $toolchain) {
    $env:PATH = "$toolchain;$env:PATH"
}

# ---- 2. Verify toolchain present ----
foreach ($t in 'gcc','cmake') {
    if (-not (Get-Command $t -ErrorAction SilentlyContinue)) {
        Write-Host "[FAIL] '$t' not found. Check WinLibs install, or open a NEW PowerShell window so PATH refreshes." -ForegroundColor Red
        Write-Host "       Expected path: $toolchain"
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
$passes += @{ Label='default'; Dir='build-test';       Flags=''; Enabled=$true }
$passes += @{ Label='opt-in';  Dir='build-test-optin'; Flags='-DWINK_HOST_ALLOW_REALTIME_FOR_TESTING=1'; Enabled=$Optin }
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
$passes += @{ Label='sanitize'; Dir='build-test-san';  Flags='-fsanitize=undefined -fsanitize-undefined-trap-on-error -Wcast-function-type -Werror=cast-function-type'; Enabled=$Sanitize }

$overallRc = 0
foreach ($p in $passes) {
    if (-not $p.Enabled) { continue }
    $ok = Invoke-TestPass -Label $p.Label -BuildDir $p.Dir -ExtraCFlags $p.Flags
    if (-not $ok) { $overallRc = 1 }
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

exit $overallRc
