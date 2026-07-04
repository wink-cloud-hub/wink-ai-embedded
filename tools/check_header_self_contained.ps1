# 头自包含检查（P1-B2）
# 对每个 PAL/DAL 公共头文件，尝试把它作为唯一 include 编译一个空 TU，
# 确保每个头不依赖 transitive include 顺序。
# 用法：powershell -ExecutionPolicy Bypass -File tools/check_header_self_contained.ps1 [-Verbose]
# 退出码：0 全部通过；1 有失败。

param(
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$palInc   = Join-Path $projectRoot "wink-micro-os\pal\include"
$dalInc   = Join-Path $projectRoot "wink-micro-os\dal\include"
$commonInc = Join-Path $projectRoot "wink-micro-os\targets\common\include"
$runtimeInc = Join-Path $projectRoot "wink-micro-os\runtime\include"
$traceInc = Join-Path $projectRoot "wink-micro-os\trace\include"
$genDir   = Join-Path $projectRoot "wink-micro-os\build-test\generated"

$headerDirs = @($palInc, $dalInc)

$includes = @(
    "-I$palInc",
    "-I$palInc\osal",
    "-I$palInc\hal",
    "-I$dalInc",
    "-I$dalInc\input",
    "-I$dalInc\output",
    "-I$dalInc\actuator",
    "-I$dalInc\display",
    "-I$dalInc\sensor",
    "-I$dalInc\communication",
    "-I$dalInc\storage",
    "-I$commonInc",
    "-I$runtimeInc",
    "-I$traceInc",
    "-I$genDir"
)

# 跳过 target-private / emscripten-only / host-only / 需宏门禁 的头
$skipPatterns = @(
    "pal_irq_advanced",
    "pal_wasm_internal",
    "host_test_ctrl",
    "wink_sim_physical",
    "wasm_bridge",
    "host_wall_clock",
    "sim_ctx"
)

$tempDir = Join-Path $env:TEMP ("wink_hdr_check_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tempDir | Out-Null

$failures = New-Object System.Collections.ArrayList
$checked = 0

foreach ($dir in $headerDirs) {
    $headers = Get-ChildItem -Path $dir -Recurse -Filter "*.h"
    foreach ($hdr in $headers) {
        $name = $hdr.Name
        $skip = $false
        foreach ($pat in $skipPatterns) {
            if ($name -match $pat) { $skip = $true; break }
        }
        if ($skip) { continue }

        $checked++
        $tuPath = Join-Path $tempDir ("tu_" + $checked + ".c")
        $relPath = $hdr.FullName.Substring($projectRoot.Path.Length + 1)
        $incPath = $hdr.FullName -replace '\\','/'

        $tuContent = "/* Self-containment test for $relPath */`n#include `"$incPath`"`nint main(void) { return 0; }`n"
        Set-Content -Path $tuPath -Value $tuContent -Encoding ascii

        $objPath = Join-Path $tempDir ("tu_" + $checked + ".o")
        $procInfo = New-Object System.Diagnostics.ProcessStartInfo
        $procInfo.FileName = "gcc"
        $procInfo.Arguments = "-std=gnu99 -Wall -Wextra -Werror $($includes -join ' ') -c -o `"$objPath`" `"$tuPath`""
        $procInfo.RedirectStandardError = $true
        $procInfo.RedirectStandardOutput = $true
        $procInfo.UseShellExecute = $false
        $procInfo.CreateNoWindow = $true
        $p = [System.Diagnostics.Process]::Start($procInfo)
        $stdout = $p.StandardOutput.ReadToEnd()
        $stderr = $p.StandardError.ReadToEnd()
        $p.WaitForExit()
        $exitCode = $p.ExitCode

        if ($exitCode -ne 0) {
            [void]$failures.Add([pscustomobject]@{ Header = $relPath; Output = $stderr.Trim() })
            if ($Verbose) { Write-Host "FAIL $relPath" -ForegroundColor Red }
        } else {
            if ($Verbose) { Write-Host "OK   $relPath" -ForegroundColor Green }
            Remove-Item $objPath -ErrorAction SilentlyContinue
        }
        Remove-Item $tuPath -ErrorAction SilentlyContinue
    }
}

Remove-Item -Recurse -Force $tempDir -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Self-contained header check: $checked headers, $($failures.Count) failures"
if ($failures.Count -gt 0) {
    Write-Host ""
    Write-Host "=== Failing headers ===" -ForegroundColor Red
    foreach ($f in $failures) {
        Write-Host "  $($f.Header)" -ForegroundColor Yellow
        Write-Host "  $($f.Output)" -ForegroundColor Gray
    }
    exit 1
}
Write-Host "All OK." -ForegroundColor Green
exit 0
