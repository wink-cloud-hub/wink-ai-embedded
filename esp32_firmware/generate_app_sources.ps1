<#
.SYNOPSIS
    Thin shim that delegates to tools/esp32/generate_app_sources.py.

.DESCRIPTION
    The generation logic moved to Python (wink-micro-os/tools/esp32/generate_app_sources.py).
    This script is kept for backwards compatibility with docs, skills, and any
    external callers that still invoke `.\generate_app_sources.ps1 -AppDir ... `
    or `-AppName ...`. It will be removed in a future release.

    PowerShell-style flags (-AppDir, -AppName) are translated to argparse
    (--app-dir, --app-name) before invoking Python.

.EXAMPLE
    .\generate_app_sources.ps1 -AppDir ..\wink-micro-app\devkitc_smoke
    .\generate_app_sources.ps1 -AppName devkitc_smoke
    .\generate_app_sources.ps1        # falls back to Python's default (devkitc_smoke)
#>

$ErrorActionPreference = "Stop"

# Ensure UTF-8 in the child so the ✅ glyph doesn't mojibake on cp936 consoles.
$env:PYTHONUTF8 = "1"
$env:PYTHONIOENCODING = "utf-8"

# esp32_firmware/ = $PSScriptRoot; repo root = its parent.
$ScriptDir = $PSScriptRoot
$RepoRoot  = (Get-Item (Join-Path $ScriptDir "..")).FullName

# Resolve SDK dir: env override wins; otherwise assume in-tree layout.
if ($env:WINK_SDK_PATH -and (Test-Path $env:WINK_SDK_PATH)) {
    $SdkDir = (Get-Item $env:WINK_SDK_PATH).FullName
} else {
    $SdkDir = Join-Path $RepoRoot "wink-micro-os"
}

$GenScript = Join-Path $SdkDir "tools\esp32\generate_app_sources.py"
if (-not (Test-Path $GenScript)) {
    Write-Error "Python generator not found: $GenScript"
    exit 1
}

# Pick Python: prefer the IDF-managed venv when the IDF shell is active,
# else use whatever `python` is on PATH.
if ($env:IDF_PYTHON_ENV_PATH -and (Test-Path (Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts\python.exe"))) {
    $Python = Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts\python.exe"
} else {
    $Python = "python"
}

# Translate PowerShell-style flags to argparse.
$pyArgs = @("--esp32-firmware-dir", $ScriptDir)
for ($i = 0; $i -lt $args.Count; $i++) {
    switch -regex ($args[$i]) {
        '^-AppDir$'  { $pyArgs += '--app-dir';  $pyArgs += $args[++$i] }
        '^-AppName$' { $pyArgs += '--app-name'; $pyArgs += $args[++$i] }
        default      { $pyArgs += $args[$i] }
    }
}

& $Python $GenScript @pyArgs
exit $LASTEXITCODE
