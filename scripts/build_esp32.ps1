# ESP-IDF build helper (env/EIM-driven, clean env for git-bash/cmd invocations).
# Usage from PowerShell:    pwsh -File scripts/build_esp32.ps1 [<idf.py args>...]
# Usage from git-bash/MINGW: /c/WINDOWS/System32/WindowsPowerShell/v1.0/powershell.exe \
#                              -NoProfile -File scripts/build_esp32.ps1 build
#
# Rationale: calling idf.py from within MSYS/MINGW triggers the
# "MSys/Mingw is no longer supported" error in ESP-IDF v6. This script strips
# MSYS/EMSDK contamination and then either (a) trusts env vars already set by
# `wink.py esp32` (Task 9 ensure_for("esp32")), or (b) auto-discovers the EIM
# PowerShell profile, or (c) falls back to `$env:IDF_PATH\export.ps1`.
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$IdfArgs = @("build")
)

$ErrorActionPreference = "Stop"
$env:PYTHONUTF8        = "1"
$env:PYTHONIOENCODING  = "utf-8"
$env:MSYSTEM           = ""
foreach ($v in @("MSYS","MINGW_PREFIX","MSYSTEM_PREFIX","EMSDK","EMSDK_NODE","EMSDK_PYTHON")) {
    Remove-Item Env:$v -ErrorAction SilentlyContinue
}

function Test-IdfPathValid {
    param([string]$IdfPath)
    if ([string]::IsNullOrEmpty($IdfPath)) { return $false }
    if (-not (Test-Path $IdfPath)) { return $false }
    if (-not (Test-Path (Join-Path $IdfPath "tools\idf.py"))) { return $false }
    return $true
}

$activated = $false

# (1) Trust env vars already set by wink.py ensure_for("esp32").
if (Test-IdfPathValid $env:IDF_PATH) {
    $activated = $true
}

# (2) Auto-discover EIM PowerShell profile (highest v6.x wins).
if (-not $activated) {
    $eimProfiles = Get-ChildItem -Path "C:\Espressif\tools" -Filter "Microsoft.v6*.PowerShell_profile.ps1" -ErrorAction SilentlyContinue |
                   Sort-Object -Property Name
    if ($eimProfiles -and $eimProfiles.Count -gt 0) {
        $profilePath = $eimProfiles[-1].FullName
        Write-Host "Activating ESP-IDF via EIM profile: $profilePath" -ForegroundColor Cyan
        . $profilePath
        if (Test-IdfPathValid $env:IDF_PATH) {
            $activated = $true
        }
    }
}

# (3) Fallback: IDF_PATH set by user but not exported — dot-source export.ps1.
if (-not $activated -and -not [string]::IsNullOrEmpty($env:IDF_PATH)) {
    $exportScript = Join-Path $env:IDF_PATH "export.ps1"
    if (Test-Path $exportScript) {
        Write-Host "Activating ESP-IDF via $exportScript" -ForegroundColor Cyan
        try {
            . $exportScript
            if (Test-IdfPathValid $env:IDF_PATH) {
                $activated = $true
            }
        } catch {
            Write-Host "WARNING: failed to source $exportScript : $_" -ForegroundColor Yellow
        }
    }
}

# (4) Give up with a helpful message.
if (-not $activated) {
    Write-Host "ERROR: ESP-IDF not found." -ForegroundColor Red
    Write-Host "  Run 'python tools/wink.py doctor' to diagnose," -ForegroundColor Red
    Write-Host "  or see tools/preinstall.md " -NoNewline -ForegroundColor Red
    Write-Host ([char]0xA7 + "3 for ESP-IDF install instructions.") -ForegroundColor Red
    exit 1
}

# (5) Verify idf.py is available on PATH and IDF_PATH points to a real directory.
if (-not (Test-IdfPathValid $env:IDF_PATH)) {
    Write-Host "ERROR: IDF_PATH ('$env:IDF_PATH') is not a valid ESP-IDF installation." -ForegroundColor Red
    Write-Host "  Run 'python tools/wink.py doctor' to diagnose." -ForegroundColor Red
    exit 1
}
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: 'idf.py' not on PATH after ESP-IDF activation." -ForegroundColor Red
    Write-Host "  IDF_PATH = $env:IDF_PATH" -ForegroundColor Red
    Write-Host "  Run 'python tools/wink.py doctor' to diagnose." -ForegroundColor Red
    exit 1
}

# (6) Re-assert UTF-8 (profile/export may have reset these).
$env:PYTHONUTF8       = "1"
$env:PYTHONIOENCODING = "utf-8"

Set-Location (Split-Path -Parent $PSScriptRoot)
& idf.py -C esp32_firmware @IdfArgs
exit $LASTEXITCODE
