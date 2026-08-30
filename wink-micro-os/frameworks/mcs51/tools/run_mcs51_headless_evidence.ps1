<#
.SYNOPSIS
  One-click MCS-51 headless evidence runner (Stage 0/2 live channels).

.DESCRIPTION
  Runs every mcs51 carrier app's headless scenario(s) through the CROSS-REPO
  unisim CLI (sister repo wink-ai -> packages/wink-tools/wink.py), the single
  sanctioned way to produce mcs51 headless evidence. Each app's scenarios live
  in <app>/unisim-scenarios/*.scenario.json (the workspace convention the
  embedded-frontend workspace-scanner discovers); the --scenarios argument is
  that DIRECTORY, so all *.scenario.json in it run in one invocation.

  This standardizes two things that were previously ad-hoc:
    1. Scenario location  : <app>/unisim-scenarios/  (NOT the app root)
    2. Invocation         : cross-repo `wink.py sim run --mode headless`,
                            passing the scenarios directory.

  The sister CLI auto-builds the production WASM, generates device-tree.json,
  and extracts assets before running the real PinArbiter + real plugins.

.PARAMETER App
  Optional. Run only one carrier app (by directory name under wink-micro-app/).
  Default: all known mcs51 carrier apps.

.PARAMETER Reporter
  Reporter passed through to the CLI (spec|json|junit). Default: spec.

.EXAMPLE
  # from anywhere; auto-locates the sister repo as a sibling directory
  powershell -File wink-micro-os/frameworks/mcs51/tools/run_mcs51_headless_evidence.ps1

.EXAMPLE
  $env:WINK_AI_ROOT = "D:\path\to\wink-ai"   # override sister repo location
  .\run_mcs51_headless_evidence.ps1 -App mcs51_uart_echo
#>
[CmdletBinding()]
param(
    [string]$App,
    [ValidateSet('spec', 'json', 'junit')]
    [string]$Reporter = 'spec'
)

$ErrorActionPreference = 'Stop'

# --- Locate repos -----------------------------------------------------------
# This script lives in <embedded>/wink-micro-os/frameworks/mcs51/tools/.
$embeddedRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')

# Sister repo: $env:WINK_AI_ROOT wins; else assume a sibling directory named
# "wink-ai" next to this "wink-ai-embedded" checkout.
$winkToolsDir = $null
if ($env:WINK_AI_ROOT) {
    $cand = Join-Path $env:WINK_AI_ROOT 'packages\wink-tools'
    if (Test-Path (Join-Path $cand 'wink.py')) { $winkToolsDir = $cand }
}
if (-not $winkToolsDir) {
    $sibling = Join-Path (Split-Path $embeddedRoot -Parent) 'wink-ai\packages\wink-tools'
    if (Test-Path (Join-Path $sibling 'wink.py')) { $winkToolsDir = $sibling }
}
if (-not $winkToolsDir) {
    Write-Error "Could not locate sister repo wink-ai/packages/wink-tools. `
Set `$env:WINK_AI_ROOT to the wink-ai checkout root."
    exit 2
}

$microAppDir = Join-Path $embeddedRoot 'wink-micro-app'

# Carrier apps in channel-proof order. All five are expected to PASS. The two
# digital-INPUT apps (button_led polled, button_led_int /INT0) exercise mcs51
# digital pin INPUT; they were broken by the sister multi-arch headless engine
# (behavioral-mode PluginContext.writePin gate) and are green again after the
# sister fix 8d06a4e8 (arbiter driven unconditionally; only the timing waveform
# edge queue is gated to timing mode).
$carriers = @(
    @{ Name = 'mcs51_uart_hello';       Channel = 'ch2 UART TX (T1)' },
    @{ Name = 'mcs51_uart_echo';        Channel = 'ch2 UART RX live (T2.3)' },
    @{ Name = 'mcs51_analog_threshold'; Channel = 'ch3 analog ADC (T4)' },
    @{ Name = 'mcs51_button_led_int';   Channel = 'ch1 INT0/1 (T3)' },
    @{ Name = 'mcs51_button_led';       Channel = 'ch1 digital read (Stage 0)' }
)
if ($App) { $carriers = $carriers | Where-Object { $_.Name -eq $App } }
if (-not $carriers) { Write-Error "No carrier app matched '$App'."; exit 2 }

# --- Run each carrier -------------------------------------------------------
$env:WINK_DEV = '1'   # make winkcli use the sister TS source directly (no build)
$results = @()

foreach ($c in $carriers) {
    $appDir  = Join-Path $microAppDir $c.Name
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

    Push-Location $winkToolsDir
    # Native stderr (cmake/build chatter) must NOT be redirected: in Windows
    # PowerShell 5.1 `2>&1` wraps each stderr line in an ErrorRecord and, with
    # $ErrorActionPreference='Stop', aborts the script. Let stdout/stderr flow
    # to the console and judge success solely by the process exit code.
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        # --scenarios is the DIRECTORY -> loadScenarioSpecs auto-runs every
        # *.scenario.json in it. CLI auto-builds WASM + device-tree first.
        & python wink.py sim run --app "$appDir" --mode headless `
            --scenarios "$scenDir" --reporter $Reporter
        $ok = ($LASTEXITCODE -eq 0)
    }
    finally {
        $ErrorActionPreference = $prevEap
        Pop-Location
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
    Write-Warning "$($failed.Count) carrier(s) failed. Inspect the scenario step output above; all five carriers are expected to pass (digital-INPUT fixed in sister 8d06a4e8)."
    exit 1
}
Write-Host ""
Write-Host "All mcs51 headless carriers PASSED." -ForegroundColor Green
exit 0
