# SPDX-License-Identifier: GPL-3.0-only
param(
    [string]$UartSource,
    [string]$OutDir
)
$ErrorActionPreference = 'Stop'
$uartTestRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (-not $UartSource) { $UartSource = Join-Path $uartTestRoot 'targets/wasm/pal_wasm_ch2_uart.c' }
$UartSource = (Resolve-Path -LiteralPath $UartSource).Path
if (-not $OutDir) { $OutDir = Join-Path $uartTestRoot '../../build/wasm/uart-rx-semantics' }
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if (-not (Get-Command emcc -ErrorAction SilentlyContinue)) { throw 'Activate emsdk: emcc not found.' }
if (-not (Get-Command node -ErrorAction SilentlyContinue)) { throw 'Node.js not found.' }
$uartEmccArgs = @(
    "-I$uartTestRoot/test/unity",
    "-I$uartTestRoot/pal/include",
    "-I$uartTestRoot/pal/include/osal",
    "-I$uartTestRoot/pal/include/internal",
    "-I$uartTestRoot/targets/wasm",
    "-I$uartTestRoot/targets/common/include",
    "-I$uartTestRoot/trace/include",
    "$uartTestRoot/test/unit/dal/test_dal_uart_rx_sim.c",
    "$uartTestRoot/test/wasm/uart_rx_link_stubs.c",
    "$uartTestRoot/test/unity/unity.c",
    "$uartTestRoot/osal/common/pal_osal_ringbuf.c",
    $UartSource,
    '-sWASM=1', '-sEXIT_RUNTIME=1', '-sALLOW_MEMORY_GROWTH=0',
    '-sERROR_ON_UNDEFINED_SYMBOLS=1', '-Wall', '-Wextra', '-Werror',
    '-O0', '-o', (Join-Path $OutDir 'test_uart_rx.js')
)
& emcc @uartEmccArgs
if ($LASTEXITCODE -ne 0) { throw "UART emcc build failed: $LASTEXITCODE" }
& node (Join-Path $OutDir 'test_uart_rx.js')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
