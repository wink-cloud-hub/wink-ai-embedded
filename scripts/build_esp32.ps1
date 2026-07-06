# ESP-IDF v6.0.1 build helper (EIM install path, clean env for git-bash/cmd invocations).
# Usage from PowerShell:    pwsh -File scripts/build_esp32.ps1 [<idf.py args>...]
# Usage from git-bash/MINGW: /c/WINDOWS/System32/WindowsPowerShell/v1.0/powershell.exe \
#                              -NoProfile -File scripts/build_esp32.ps1 build
#
# Rationale: calling idf.py from within MSYS/MINGW triggers the
# "MSys/Mingw is no longer supported" error in ESP-IDF v6. This script sets up
# the PATH from scratch (no MSYS entries) before invoking idf.py.
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$IdfArgs = @("build")
)

$ErrorActionPreference = "Stop"
$env:PYTHONUTF8        = "1"
$env:MSYSTEM           = ""
foreach ($v in @("MSYS","MINGW_PREFIX","MSYSTEM_PREFIX","EMSDK","EMSDK_NODE","EMSDK_PYTHON")) {
    Remove-Item Env:$v -ErrorAction SilentlyContinue
}

$env:IDF_TOOLS_PATH         = "C:\Espressif\tools"
$env:IDF_PATH               = "D:\software\embedded\esp\v6.0.1\esp-idf"
$env:IDF_PYTHON_ENV_PATH    = "C:\Espressif\tools\python\v6.0.1\venv"
$env:ESP_IDF_VERSION        = "6.0.1"
$env:IDF_CCACHE_ENABLE      = "1"
$env:ESP_ROM_ELF_DIR        = "C:\Espressif\tools\esp-rom-elfs\20241011/"
$env:OPENOCD_SCRIPTS        = "C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20260304\openocd-esp32/share/openocd/scripts"
$env:ESP_CLANG_LIBS_PATH    = "C:\Espressif\tools\esp-clang-libs\esp-20.1.1_20250829/esp-clang/lib"

$env:PATH = "C:\Espressif\tools\idf-exe\1.0.3;" + `
            "C:\Espressif\tools\ccache\4.12.1\ccache-4.12.1-windows-x86_64;" + `
            "C:\Espressif\tools\cmake\4.0.3\bin;" + `
            "C:\Espressif\tools\ninja\1.12.1;" + `
            "C:\Espressif\tools\python\v6.0.1\venv\Scripts;" + `
            "C:\Espressif\tools\python;" + `
            "C:\Espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin;" + `
            "C:\Espressif\tools\riscv32-esp-elf\esp-15.2.0_20251204\riscv32-esp-elf\bin;" + `
            "C:\Espressif\tools\esp32ulp-elf\2.38_20240113\esp32ulp-elf-binutils\bin;" + `
            "C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20260304\openocd-esp32\bin;" + `
            "C:\Espressif\tools\dfu-util\0.11\dfu-util-0.11-win64;" + `
            "C:\WINDOWS\system32;C:\WINDOWS;C:\WINDOWS\System32\Wbem;" + `
            "C:\WINDOWS\System32\WindowsPowerShell\v1.0\;" + `
            "C:\Program Files\CMake\bin"

Set-Location (Split-Path -Parent $PSScriptRoot)
& idf.py -C esp32_firmware @IdfArgs
exit $LASTEXITCODE
