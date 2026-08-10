# P1-2: build + run Arbiter-first GPIO semantics tests under emcc/Node.
$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$OutDir = Join-Path $Root "build\wasm-gpio-semantics"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$emcc = Get-Command emcc -ErrorAction SilentlyContinue
if (-not $emcc) {
    throw "emcc not found on PATH. Activate emsdk first."
}

$incs = @(
    "-I$Root/test/unity",
    "-I$Root/test",
    "-I$Root/pal/include",
    "-I$Root/pal/include/hal",
    "-I$Root/pal/include/osal",
    "-I$Root/pal/include/internal",
    "-I$Root/targets/wasm",
    "-I$Root/targets/common/include",
    "-I$Root/trace/include"
)

$srcs = @(
    "$Root/test/wasm/test_pal_gpio_read_wasm_semantics.c",
    "$Root/test/wasm/gpio_semantics_link_stubs.c",
    "$Root/test/unity/unity.c",
    "$Root/targets/wasm/pal_wasm_ch1_gpio.c",
    "$Root/targets/common/src/pal_resource.c"
)

$outJs = Join-Path $OutDir "test_pal_gpio_read_wasm_semantics.js"

Write-Host "== emcc build =="
$emccArgs = @()
$emccArgs += $incs
$emccArgs += $srcs
$emccArgs += @(
    "-D__EMSCRIPTEN__",
    "-sWASM=1",
    "-sEXIT_RUNTIME=1",
    "-sALLOW_MEMORY_GROWTH=0",
    "-sERROR_ON_UNDEFINED_SYMBOLS=1",
    "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString']",
    "-ffunction-sections",
    "-fdata-sections",
    "-Wl,--gc-sections",
    "-O0",
    "-o",
    $outJs
)
& emcc @emccArgs

if ($LASTEXITCODE -ne 0) { throw "emcc failed: $LASTEXITCODE" }

Write-Host "== node run =="
& node $outJs
if ($LASTEXITCODE -ne 0) { throw "tests failed: $LASTEXITCODE" }
Write-Host "ALL PASS"
