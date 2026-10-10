<#
.SYNOPSIS
  One-click MCS-51 headless evidence runner (Stage 0/2 live channels).

.DESCRIPTION
  Runs every mcs51 carrier app's headless scenario(s) through `winkcli` (the
  unified build & simulation toolchain CLI), the single sanctioned way to
  produce mcs51 headless evidence. Each app's scenarios live in
  <app>/unisim-scenarios/*.scenario.json; the --scenarios argument is that
  DIRECTORY, so all *.scenario.json in it run in one invocation.

  This standardizes two things that were previously ad-hoc:
    1. Scenario location  : <app>/unisim-scenarios/  (NOT the app root)
    2. Invocation         : `winkcli sim run --mode headless`, passing the
                            scenarios directory.

  The CLI auto-builds the production WASM, generates device-tree.json,
  and extracts assets before running the real PinArbiter + real plugins.

.PARAMETER App
  Optional. Run only one carrier app (by directory name under wink-micro-app/).
  Default: all known mcs51 carrier apps.

.PARAMETER Reporter
  Reporter passed through to the CLI (spec|json|junit). Default: spec.

.EXAMPLE
  powershell -File wink-micro-os/frameworks/mcs51/tools/run_mcs51_headless_evidence.ps1

.EXAMPLE
  .\run_mcs51_headless_evidence.ps1 -App mcs51_uart_echo
#>
[CmdletBinding()]
param(
    [string]$App,
    [ValidateSet('spec', 'json', 'junit')]
    [string]$Reporter = 'spec'
)

$ErrorActionPreference = 'Stop'

# --- Verify prerequisites ---------------------------------------------------
if (-not (Get-Command winkcli -ErrorAction SilentlyContinue)) {
    Write-Error "'winkcli' command not found on PATH.`n`
Please install winkcli globally (e.g. 'pip install wink-tools') or ensure winkcli is added to your system PATH."
    exit 2
}

# This script lives in <embedded>/wink-micro-os/frameworks/mcs51/tools/.
$embeddedRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')
$microAppDir  = Join-Path $embeddedRoot 'wink-micro-app'

# Carrier apps in channel-proof order. All five are expected to PASS. The two
# digital-INPUT apps (button_led polled, button_led_int /INT0) exercise mcs51
# digital pin INPUT (arbiter driven unconditionally; only the timing waveform
# edge queue is gated to timing mode).
$carriers = @(
    @{ Name = 'mcs51_uart_hello';       Rel = 'mcs51/uart_hello';       Channel = 'ch2 UART TX (T1)' },
    @{ Name = 'mcs51_uart_echo';        Rel = 'mcs51/uart_echo';        Channel = 'ch2 UART RX live (T2.3)' },
    @{ Name = 'mcs51_analog_threshold'; Rel = 'mcs51/analog_threshold'; Channel = 'ch3 analog ADC (T4)' },
    @{ Name = 'mcs51_button_led_int';   Rel = 'mcs51/button_led_int';   Channel = 'ch1 INT0/1 (T3)' },
    @{ Name = 'mcs51_button_led';       Rel = 'mcs51/button_led';       Channel = 'ch1 digital read (Stage 0)' }
)
if ($App) { $carriers = $carriers | Where-Object { $_.Name -eq $App -or $_.Rel -eq $App } }
if (-not $carriers) {
    # Custom app: accept a nested relative path (e.g. vendor/cms8s78xx/wdt) or
    # a bare directory name resolved by recursive search.
    $customRel = $App -replace '\\', '/'
    $customAppDir = Join-Path $microAppDir $customRel
    if (-not (Test-Path $customAppDir)) {
        $hit = Get-ChildItem $microAppDir -Recurse -Directory -Filter (Split-Path $customRel -Leaf) |
            Where-Object { Test-Path (Join-Path $_.FullName 'unisim-scenarios') } |
            Select-Object -First 1
        if ($hit) { $customAppDir = $hit.FullName }
    }
    if (Test-Path $customAppDir) {
        $carriers = @( @{ Name = $App; Rel = $customAppDir; Channel = 'Custom / Vendor app' } )
    } else {
        Write-Error "No carrier app matched '$App'."
        exit 2
    }
}

# --- Run each carrier -------------------------------------------------------
$results = @()

foreach ($c in $carriers) {
    $appDir  = if ([System.IO.Path]::IsPathRooted($c.Rel)) { $c.Rel } else { Join-Path $microAppDir $c.Rel }
    $scenDir = Join-Path $appDir 'unisim-scenarios'

    Write-Host ""
    Write-Host "================================================================" -ForegroundColor Cyan
    Write-Host " $($c.Name)  —  $($c.Channel)" -ForegroundColor Cyan
    Write-Host "================================================================" -ForegroundColor Cyan

    if (-not (Test-Path $scenDir)) {
        Write-Warning "skip: $scenDir not found"
        $results += [pscustomobject]@{ App = $c.Name; Channel = $c.Channel; Ok = $false; Note = 'no unisim-scenarios/' }
        continue
    }

    # Native stderr (cmake/build chatter) must NOT be redirected: in Windows
    # PowerShell 5.1 `2>&1` wraps each stderr line in an ErrorRecord and, with
    # $ErrorActionPreference='Stop', aborts the script. Let stdout/stderr flow
    # to the console and judge success solely by the process exit code.
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        # --scenarios is the DIRECTORY -> loadScenarioSpecs auto-runs every
        # *.scenario.json in it. CLI auto-builds WASM + device-tree first.
        winkcli sim run --app "$appDir" --mode headless `
            --scenarios "$scenDir" --reporter $Reporter
        $ok = ($LASTEXITCODE -eq 0)
    }
    finally {
        $ErrorActionPreference = $prevEap
    }

    $results += [pscustomobject]@{ App = $c.Name; Channel = $c.Channel; Ok = $ok; Note = '' }
}

# --- Summary ----------------------------------------------------------------
Write-Host ""
Write-Host "================ MCS-51 headless evidence summary ================" -ForegroundColor Yellow
foreach ($r in $results) {
    $tag = if ($r.Ok) { 'PASS' } else { 'FAIL' }
    $color = if ($r.Ok) { 'Green' } else { 'Red' }
    Write-Host ("  [{0}] {1,-24} {2} {3}" -f $tag, $r.App, $r.Channel, $r.Note) -ForegroundColor $color
}

$failed = $results | Where-Object { -not $_.Ok }
if ($failed) {
    Write-Host ""
    Write-Warning "$($failed.Count) carrier(s) failed. Inspect the scenario step output above; all five carriers are expected to pass."
    exit 1
}
Write-Host ""
Write-Host "All mcs51 headless carriers PASSED." -ForegroundColor Green
exit 0
