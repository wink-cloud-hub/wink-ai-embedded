param(
    [switch]$Watch,
    [string]$Mode = ""
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition

# 1. Detect Dependency Mode (SOURCE_LINKED vs NPM_SEMVER)
$NodeModulesDir = Join-Path $ScriptDir "node_modules\@wink-ai"
$UnisimLink = Join-Path $NodeModulesDir "unisim"
$ModeStr = "NPM_SEMVER"

if (Test-Path $UnisimLink) {
    $item = Get-Item $UnisimLink -Force
    if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        $ModeStr = "SOURCE_LINKED"
    }
}
if ($Mode -eq "source" -or $Mode -eq "link") { $ModeStr = "SOURCE_LINKED" }
if ($Mode -eq "npm" -or $Mode -eq "unlink") { $ModeStr = "NPM_SEMVER" }

$ExeCmd = ""
$PrefixArgs = @()
$RunnerMode = ""
$WinkToolsPath = $null

# 2. Strategy A: In SOURCE_LINKED mode, prioritize local private wink-tools Python source
if ($ModeStr -eq "SOURCE_LINKED") {
    if ($env:WINK_TOOLS_PATH) {
        $WinkToolsPath = $env:WINK_TOOLS_PATH
    } else {
        $LocalCandidate = Join-Path $ScriptDir "..\..\wink-ai\packages\wink-tools"
        if (Test-Path $LocalCandidate) {
            $WinkToolsPath = $LocalCandidate
        }
    }

    if ($WinkToolsPath -and -not [System.IO.Path]::IsPathRooted($WinkToolsPath)) {
        $WinkToolsPath = Join-Path $ScriptDir $WinkToolsPath
    }

    if ($WinkToolsPath -and (Test-Path $WinkToolsPath)) {
        $WinkPy = Join-Path $WinkToolsPath "wink.py"
        if (-not (Test-Path $WinkPy)) {
            $WinkPyCandidate = Join-Path $WinkToolsPath "tools\wink.py"
            if (Test-Path $WinkPyCandidate) { $WinkPy = $WinkPyCandidate }
        }

        if (Test-Path $WinkPy) {
            $VenvPy = Join-Path (Split-Path -Parent (Get-Item $WinkToolsPath).FullName) "..\.venv\Scripts\python.exe"
            $PyCmd = if (Test-Path $VenvPy) { (Get-Item $VenvPy).FullName } else { "python" }
            $ExeCmd = $PyCmd
            $PrefixArgs = @((Get-Item $WinkPy).FullName)
            $RunnerMode = "Local Source Python ($PyCmd -> $WinkPy)"
        }
    }
}

# 3. Strategy B: In NPM_SEMVER mode or Fallback, use global CLI (winkcli / wink)
if (-not $ExeCmd) {
    if ($env:WINK_CLI_EXE -and (Test-Path $env:WINK_CLI_EXE)) {
        $ExeCmd = $env:WINK_CLI_EXE
        $PrefixArgs = @()
        $RunnerMode = "Environment WINK_CLI_EXE ($ExeCmd)"
    } elseif (Get-Command "winkcli" -ErrorAction SilentlyContinue) {
        $ExeCmd = "winkcli"
        $PrefixArgs = @()
        $RunnerMode = "Global CLI (winkcli)"
    } elseif (Get-Command "wink" -ErrorAction SilentlyContinue) {
        $ExeCmd = "wink"
        $PrefixArgs = @()
        $RunnerMode = "Global CLI (wink)"
    }
}

# 4. Handle toolchain missing with clean, domain-appropriate error messages
if (-not $ExeCmd) {
    Write-Host ""
    Write-Host "========================================================" -ForegroundColor Red
    Write-Host " [ERROR] Wink CLI toolchain not found" -ForegroundColor Red
    Write-Host "========================================================" -ForegroundColor Red
    if ($ModeStr -eq "SOURCE_LINKED") {
        Write-Host "You are in SOURCE_LINKED mode, but local wink-tools/wink.py was not found." -ForegroundColor Yellow
        Write-Host "Please verify that wink-ai/packages/wink-tools exists or install winkcli globally." -ForegroundColor Yellow
    } else {
        Write-Host "For Open-Source Users (wink-ai-embedded):" -ForegroundColor Cyan
        Write-Host "  Please install the Wink CLI toolchain globally:" -ForegroundColor White
        Write-Host "    pip install wink-tools" -ForegroundColor Green
        Write-Host "  Or download 'winkcli.exe' and add it to your system PATH." -ForegroundColor Green
    }
    Write-Host ""
    exit 1
}

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "     Wink AI - Peripheral Plugins Build Tool            " -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "Dependency Mode: $ModeStr" -ForegroundColor $(if ($ModeStr -eq "SOURCE_LINKED") { "Green" } else { "Yellow" })
if ($WinkToolsPath) {
    Write-Host "Local Tools Path: $WinkToolsPath"
}
Write-Host "Runner Mode:     $RunnerMode"
if ($Watch) {
    Write-Host "Watch Mode:      ENABLED" -ForegroundColor Yellow
}
Write-Host ""

$BuiltinDir = Join-Path $ScriptDir "builtin"
if (-not (Test-Path $BuiltinDir)) {
    Write-Host "[ERROR] Builtin directory not found: $BuiltinDir" -ForegroundColor Red
    exit 1
}

function Build-AllPeripherals {
    $Simulations = Get-ChildItem -Path $BuiltinDir -Filter "simulation.ts" -Recurse | Where-Object {
        $_.FullName -notmatch "[\\/](dist|node_modules)($|[\\/])" -and (Test-Path (Join-Path $_.Directory "definition.ts"))
    }

    $BuildCount = 0
    $script:FailCount = 0

    foreach ($sim in $Simulations) {
        $PluginDir = $sim.Directory.Parent.FullName
        $OutDir = Join-Path $PluginDir "dist"
        $BuildCount++

        Write-Host "--------------------------------------------------------" -ForegroundColor Yellow
        Write-Host "Building plugin in: $PluginDir" -ForegroundColor Yellow
        Write-Host "Output dir:        $OutDir"

        $BuildArgs = $PrefixArgs + @("--skip-toolchain-check", "build", "unisim-plugin", "--path", "$PluginDir", "--out", "$OutDir")
        & $ExeCmd @BuildArgs
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[SUCCESS] Built successfully." -ForegroundColor Green
        } else {
            Write-Host "[FAIL] Build failed with exit code $LASTEXITCODE" -ForegroundColor Red
            $script:FailCount++
        }
        Write-Host ""
    }

    Write-Host "========================================================" -ForegroundColor Cyan
    Write-Host "Build Summary: Total $BuildCount, Failed $script:FailCount" -ForegroundColor Cyan
    Write-Host "========================================================" -ForegroundColor Cyan
}

Build-AllPeripherals

if ($script:FailCount -gt 0) {
    exit 1
}

if ($Watch) {
    Write-Host "Watching $BuiltinDir for changes in src/... (Press Ctrl+C to stop)" -ForegroundColor Yellow
    $Watcher = New-Object System.IO.FileSystemWatcher
    $Watcher.Path = $BuiltinDir
    $Watcher.IncludeSubdirectories = $true
    $Watcher.EnableRaisingEvents = $true

    while ($true) {
        $result = $Watcher.WaitForChanged([System.IO.WatcherChangeTypes]::Changed -or [System.IO.WatcherChangeTypes]::Created, 1000)
        if ($result.TimedOut -eq $false) {
            if ($result.Name -notmatch "(dist|node_modules)") {
                Write-Host "[WATCH] File changed: $($result.Name), rebuilding..." -ForegroundColor Yellow
                Build-AllPeripherals
            }
        }
    }
}
