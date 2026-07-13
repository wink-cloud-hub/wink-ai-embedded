<#
.SYNOPSIS
    REMOVED: PowerShell app-source generation shim replaced by Python.

.DESCRIPTION
    The source-scanning logic that produced main/app_sources.cmake moved
    to Python during the SDK Python migration. This script is a fail-fast
    stub retained only to surface a clear error for workflows, muscle
    memory, or docs that still invoke the old PowerShell path. It does
    not generate any CMake files.

    Use one of these replacements instead:

      # The CMake configure step invokes the generator automatically;
      # no manual step is needed before idf.py build.

      # To regenerate manually:
      python wink-micro-os/tools/esp32/generate_app_sources.py `
          --app-dir <path-to-app> --esp32-firmware-dir esp32_firmware

      # Or via wink (which runs generate then build):
      python wink-micro-os/tools/wink.py esp32 --app <path-to-app> build

    The PowerShell shim existed only during the Python migration window
    on the feat/sdk-phase2-binary branch and was never part of a tagged
    release. It will be removed entirely in a future cleanup.
#>

Write-Host ""
Write-Host "ERROR: esp32_firmware/generate_app_sources.ps1 has been removed." -ForegroundColor Red
Write-Host ""
Write-Host "App source scanning is now Python. Use:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  python wink-micro-os/tools/esp32/generate_app_sources.py ``" -ForegroundColor White
Write-Host "      --app-dir <path-to-app> --esp32-firmware-dir esp32_firmware" -ForegroundColor White
Write-Host ""
Write-Host "or just run idf.py build - CMake regenerates app_sources.cmake" -ForegroundColor Yellow
Write-Host "automatically at configure time." -ForegroundColor Yellow
Write-Host ""
exit 1
