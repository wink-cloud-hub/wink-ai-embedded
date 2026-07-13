<#
.SYNOPSIS
    REMOVED: PowerShell build shim replaced by Python.

.DESCRIPTION
    The ESP32 build orchestration (strip MSYS/EMSDK, activate IDF via EIM
    profile / export script, invoke idf.py) has moved to Python. This
    script is a fail-fast stub retained only to surface a clear error
    for workflows, muscle memory, or docs that still invoke the old
    PowerShell path. It does not perform any build work.

    Use one of these replacements instead:

      # From a shell at the repo root (recommended):
      python wink-micro-os/tools/wink.py esp32 --app <path-to-app> build

      # Direct invocation of the runner (forwards all idf.py args):
      python wink-micro-os/tools/esp32/build.py `
          --esp32-firmware-dir esp32_firmware -- build flash monitor

    The PowerShell shim existed only during the Python migration window
    on the feat/sdk-phase2-binary branch and was never part of a tagged
    release. It will be removed entirely in a future cleanup.
#>

Write-Host ""
Write-Host "ERROR: scripts/build_esp32.ps1 has been removed." -ForegroundColor Red
Write-Host ""
Write-Host "ESP32 build orchestration is now Python. Use:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  python wink-micro-os/tools/wink.py esp32 --app <app-dir> build" -ForegroundColor White
Write-Host ""
Write-Host "or, for direct idf.py passthrough:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  python wink-micro-os/tools/esp32/build.py ``" -ForegroundColor White
Write-Host "      --esp32-firmware-dir esp32_firmware -- build flash monitor" -ForegroundColor White
Write-Host ""
exit 1
