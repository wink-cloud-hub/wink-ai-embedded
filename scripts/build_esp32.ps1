<#
.SYNOPSIS
    Thin shim that delegates to wink-micro-os/tools/esp32/build.py.

.DESCRIPTION
    The build logic (strip MSYS/EMSDK contamination, activate ESP-IDF via
    EIM profile / export.ps1, run idf.py) moved to Python
    (wink-micro-os/tools/esp32/build.py). This script is kept for
    backwards compatibility with docs, skills, and any external callers
    that still invoke `pwsh -File scripts/build_esp32.ps1 <idf args...>`.
    It will be removed in a future release.

    All arguments after this script's own params are forwarded verbatim
    to the Python runner after a `--` separator, so any idf.py flags
    (build, flash, monitor, -DFOO=bar, ...) pass through unchanged.

.EXAMPLE
    pwsh -File scripts/build_esp32.ps1 build
    pwsh -File scripts/build_esp32.ps1 flash monitor
    pwsh -File scripts/build_esp32.ps1 -DWINK_APP_DIR=... build
#>
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$IdfArgs = @("build")
)

$ErrorActionPreference = "Stop"

# Set UTF-8 in the shim so any early failure messages render on cp936
# consoles. build.py also sets these in the child env for idf.py.
$env:PYTHONUTF8       = "1"
$env:PYTHONIOENCODING = "utf-8"

# scripts/ = $PSScriptRoot; repo root = its parent.
$RepoRoot = (Get-Item (Join-Path $PSScriptRoot "..")).FullName

# Resolve SDK dir: env override wins; otherwise assume in-tree layout.
if ($env:WINK_SDK_PATH -and (Test-Path $env:WINK_SDK_PATH)) {
    $SdkDir = (Get-Item $env:WINK_SDK_PATH).FullName
} else {
    $SdkDir = Join-Path $RepoRoot "wink-micro-os"
}

$BuildScript = Join-Path $SdkDir "tools\esp32\build.py"
if (-not (Test-Path $BuildScript)) {
    Write-Error "Python build runner not found: $BuildScript"
    exit 1
}

$Esp32Dir = Join-Path $RepoRoot "esp32_firmware"

# Pick Python: prefer the IDF-managed venv when the IDF shell is active,
# else use whatever `python` is on PATH. build.py's activation logic will
# also work with a plain `python` since it locates IDF itself.
if ($env:IDF_PYTHON_ENV_PATH -and (Test-Path (Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts\python.exe"))) {
    $Python = Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts\python.exe"
} else {
    $Python = "python"
}

$pyArgs = @("--esp32-firmware-dir", $Esp32Dir, "--") + $IdfArgs

& $Python $BuildScript @pyArgs
exit $LASTEXITCODE
