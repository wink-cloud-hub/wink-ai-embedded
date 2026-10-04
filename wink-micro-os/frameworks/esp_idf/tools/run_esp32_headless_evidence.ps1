<#
.SYNOPSIS
  One-click ESP-IDF v6.1 headless evidence runner for ESP32.

.DESCRIPTION
  Runs every ESP32 ESP-IDF vendor app's headless scenario(s) through the CROSS-REPO
  unisim CLI (sister repo wink-ai -> packages/wink-tools/wink.py), the single
  sanctioned way to produce ESP-IDF headless evidence. Each app's scenarios live
  in <app>/unisim-scenarios/*.scenario.json; the --scenarios argument is that
  DIRECTORY or file path.

  The CLI auto-builds the production WASM, generates device-tree.json,
  and extracts assets before running the real PinArbiter + real plugins.

.PARAMETER App
  Optional. Run only one app (by directory name or relative path under wink-micro-app/).
  Default: all known ESP-IDF carrier apps.

.PARAMETER Reporter
  Reporter passed through to the CLI (spec|json|junit). Default: spec.

.EXAMPLE
  # Run all ESP-IDF carrier apps
  powershell -File wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1

.EXAMPLE
  # Run a specific app
  powershell -File wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1 -App blink_gpio
#>
[CmdletBinding()]
param(
    [string]$App,
    [string]$ConfigId,
    [string]$Scenario,
    [ValidateSet('spec', 'json', 'junit')]
    [string]$Reporter = 'spec',
    [switch]$WriteEvidence
)

$ErrorActionPreference = 'Stop'

# --- Locate repos -----------------------------------------------------------
# This script lives in <embedded>/wink-micro-os/frameworks/esp_idf/tools/.
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
    Write-Error "Could not locate sister repo wink-tools. Set `$env:WINK_AI_ROOT to the wink-ai checkout root."
    exit 2
}

$microAppDir = Join-Path $embeddedRoot 'wink-micro-app'
$espIdfBaseDir = Join-Path $microAppDir 'vendor\esp_idfv61'

# Known ESP32 carrier apps
$allCarriers = @(
    @{ Name = 'blink_gpio';    Rel = 'vendor/esp_idfv61/get-started/blink_gpio';    Channel = 'GPIO Output & FreeRTOS vTaskDelay' },
    @{ Name = 'hello_world';   Rel = 'vendor/esp_idfv61/get-started/hello_world';   Channel = 'Chip Info & System Lifecycle Reset' },
    @{ Name = 'ledc_basic';    Rel = 'vendor/esp_idfv61/peripherals/ledc_basic';    Channel = 'LEDC PWM & Hardware Timer' },
    @{ Name = 'i2c_basic';     Rel = 'vendor/esp_idfv61/peripherals/i2c_basic';     Channel = 'I2C Master Bus Communication' },
    @{ Name = 'uart_echo';     Rel = 'vendor/esp_idfv61/peripherals/uart_echo';     Channel = 'UART Loopback & Ring Buffer' },
    @{ Name = 'gptimer_alarm'; Rel = 'vendor/esp_idfv61/peripherals/gptimer_alarm'; Channel = 'General Purpose Timer & Alarms' },
    @{ Name = 'adc_oneshot_read'; Rel = 'vendor/esp_idfv61/peripherals/adc_oneshot_read'; Channel = 'ADC Oneshot Read & Calibration' },
    @{ Name = 'adc_continuous_read'; Rel = 'vendor/esp_idfv61/peripherals/adc_continuous_read'; Channel = 'ADC Continuous DMA Read & Event Callback' },
    @{ Name = 'dac_dac_oneshot'; Rel = 'vendor/esp_idfv61/peripherals/dac_dac_oneshot'; Channel = 'DAC Oneshot Output & Voltage Generation' },
    @{ Name = 'wifi_sta';      Rel = 'vendor/esp_idfv61/wifi/wifi_sta';              Channel = 'Wi-Fi Station Mode & Netif' },
    @{ Name = 'http_client';   Rel = 'vendor/esp_idfv61/protocols/http_client';   Channel = 'HTTP/REST Client & Events' },
    @{ Name = 'mqtt_tcp';      Rel = 'vendor/esp_idfv61/protocols/mqtt_tcp';      Channel = 'MQTT Protocol & Event Loop' },
    @{ Name = 'nvs_nvs_rw_value'; Rel = 'vendor/esp_idfv61/storage/nvs_nvs_rw_value'; Channel = 'NVS Read/Write Value & Key Iteration' },
    @{ Name = 'nvs_nvs_rw_blob';  Rel = 'vendor/esp_idfv61/storage/nvs_nvs_rw_blob';  Channel = 'NVS Read/Write Struct & Array Blobs' },
    @{ Name = 'spi_master_hd_eeprom'; Rel = 'vendor/esp_idfv61/peripherals/spi_master_hd_eeprom'; Channel = 'SPI Master Half-Duplex Bus Communication & EEPROM' },
    @{ Name = 'gpio_generic_gpio'; Rel = 'vendor/esp_idfv61/peripherals/gpio_generic_gpio'; Channel = 'Generic GPIO Input/Output & Edge Interrupt' },
    @{ Name = 'esp_event_default_event_loop'; Rel = 'vendor/esp_idfv61/system/esp_event_default_event_loop'; Channel = 'Default Event Loop & Periodic Timer/Task Events' },
    @{ Name = 'freertos_real_time_stats'; Rel = 'vendor/esp_idfv61/system/freertos_real_time_stats'; Channel = 'FreeRTOS Real Time Stats & Task Execution Measurement' },
    @{ Name = 'esp_timer'; Rel = 'vendor/esp_idfv61/system/esp_timer'; Channel = 'High Resolution Timer & Periodic/Oneshot Alarms' },
    @{ Name = 'dac_dac_cosine_wave'; Rel = 'vendor/esp_idfv61/peripherals/dac_dac_cosine_wave'; Channel = 'DAC Cosine Wave Generator & Voltage Output' },
    @{ Name = 'spiffs'; Rel = 'vendor/esp_idfv61/storage/spiffs'; Channel = 'SPIFFS Filesystem & VFS RAM Sandbox Operations' },
    @{ Name = 'i2c_i2c_eeprom'; Rel = 'vendor/esp_idfv61/peripherals/i2c_i2c_eeprom'; Channel = 'I2C Master Bus Communication & EEPROM Read/Write' },
    @{ Name = 'uart_uart_events'; Rel = 'vendor/esp_idfv61/peripherals/uart_uart_events'; Channel = 'UART FreeRTOS Event Queue & Pattern Detect' },
    @{ Name = 'ledc_ledc_fade'; Rel = 'vendor/esp_idfv61/peripherals/ledc_ledc_fade'; Channel = 'LEDC Hardware Smooth Fade & Semaphore Callback' },
    @{ Name = 'nvs_nvs_iteration'; Rel = 'vendor/esp_idfv61/storage/nvs_nvs_iteration'; Channel = 'NVS Key Iteration & Type Filter' },
    @{ Name = 'getting_started_softAP'; Rel = 'vendor/esp_idfv61/wifi/getting_started_softAP'; Channel = 'Wi-Fi SoftAP Hotspot & Broadcast Beacon' },
    @{ Name = 'fast_scan'; Rel = 'vendor/esp_idfv61/wifi/fast_scan'; Channel = 'Wi-Fi Fast Scan & Association Determinism' }
)

$carriers = @()
if ($App) {
    $carriers = $allCarriers | Where-Object { $_.Name -eq $App -or $_.Rel -eq $App -or (Split-Path $_.Rel -Leaf) -eq $App }
    if (-not $carriers) {
        # Custom app directory path
        $customRel = $App -replace '\\', '/'
        $customAppDir = Join-Path $microAppDir $customRel
        if (-not (Test-Path $customAppDir)) {
            $hit = Get-ChildItem $espIdfBaseDir -Directory -Filter $App | Select-Object -First 1
            if ($hit) { $customAppDir = $hit.FullName }
        }
        if (Test-Path $customAppDir) {
            $carriers = @( @{ Name = $App; Rel = $customAppDir; Channel = 'Custom ESP32 App' } )
        } else {
            Write-Error "No ESP-IDF app matched '$App'."
            exit 2
        }
    }
} else {
    # If no App specified, run all apps that actually have unisim-scenarios
    foreach ($c in $allCarriers) {
        $checkDir = Join-Path $microAppDir ($c.Rel -replace '/', '\')
        if (Test-Path (Join-Path $checkDir 'unisim-scenarios')) {
            $carriers += $c
        }
    }
}

# --- Run each carrier -------------------------------------------------------
$env:WINK_DEV = '1'
$results = @()

foreach ($c in $carriers) {
    $appDir  = if ([System.IO.Path]::IsPathRooted($c.Rel)) { $c.Rel } else { Join-Path $microAppDir ($c.Rel -replace '/', '\') }
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

    $targetScen = if ($Scenario) {
        if ([System.IO.Path]::IsPathRooted($Scenario)) { $Scenario }
        elseif (Test-Path (Join-Path $scenDir $Scenario)) { Join-Path $scenDir $Scenario }
        else { $Scenario }
    } else {
        $scenDir
    }

    $reportSrc = Join-Path $winkToolsDir 'artifacts\run-report.json'
    # Anti-False-Green: Remove any stale report from previous runs to guarantee freshness
    if (Test-Path $reportSrc) {
        Remove-Item -Force $reportSrc -ErrorAction SilentlyContinue
    }

    Push-Location $winkToolsDir
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $actualReporter = if ($WriteEvidence -and $Reporter -eq 'spec') { 'json' } else { $Reporter }
    try {
        & python wink.py sim run --app "$appDir" --mode headless `
            --scenarios "$targetScen" --reporter $actualReporter
        $ok = ($LASTEXITCODE -eq 0)
    }
    finally {
        $ErrorActionPreference = $prevEap
        Pop-Location
    }

    if ($ok -and $WriteEvidence) {
        if (-not (Test-Path $reportSrc)) {
            Write-Warning "Simulation reported success but no report artifact was created at $reportSrc"
            $ok = $false
        } else {
            $verifierScript = Join-Path $embeddedRoot 'wink-micro-app\vendor\esp_idfv61\.governance\gates\evidence_verifier.py'
            Write-Host "Recording evidence for $($c.Name)..." -ForegroundColor Magenta
            $vArgs = @('--write-app', $c.Name, '--report-src', $reportSrc, '--workspace-root', $embeddedRoot)
            if ($ConfigId) { $vArgs += @('--config-id', $ConfigId) }
            if ($Scenario) { $vArgs += @('--scenario', $targetScen) }
            & python "$verifierScript" @vArgs
            if ($LASTEXITCODE -ne 0) {
                Write-Warning "Failed to record evidence for $($c.Name)"
                $ok = $false
            }
        }
    }

    $results += [pscustomobject]@{ App = $c.Name; Channel = $c.Channel; Ok = $ok; Note = '' }
}

# --- Summary ----------------------------------------------------------------
Write-Host ""
Write-Host "================ ESP32 headless evidence summary ================" -ForegroundColor Yellow
foreach ($r in $results) {
    $tag = if ($r.Ok) { 'PASS' } else { 'FAIL' }
    $color = if ($r.Ok) { 'Green' } else { 'Red' }
    Write-Host ("  [{0}] {1,-20} {2,-38} {3}" -f $tag, $r.App, $r.Channel, $r.Note) -ForegroundColor $color
}

$failed = $results | Where-Object { -not $_.Ok }
if ($failed) {
    Write-Host ""
    Write-Warning "$($failed.Count) carrier(s) failed."
    exit 1
}
Write-Host ""
Write-Host "All ESP-IDF headless carriers PASSED." -ForegroundColor Green
exit 0
