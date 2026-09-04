param(
    [switch]$Watch,
    [string]$Mode = ""
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition

function Ensure-PeripheralEnvironment {
    param()

    $NodeModulesDir = Join-Path $ScriptDir "node_modules"
    $WinkAiDir = Join-Path $NodeModulesDir "@wink-ai"

    # 1. Probe for local wink-ai source packages
    $LocalPackagesDir = $null
    $CandidateRoots = @(
        Join-Path $ScriptDir "..\..\wink-ai\packages",
        Join-Path $ScriptDir "..\wink-ai\packages",
        Join-Path $ScriptDir "..\packages"
    )
    if ($env:WINK_AI_ROOT) {
        $CandidateRoots = @(Join-Path $env:WINK_AI_ROOT "packages") + $CandidateRoots
    }

    foreach ($cand in $CandidateRoots) {
        if ($cand -and (Test-Path (Join-Path $cand "unisim-ui")) -and (Test-Path (Join-Path $cand "unisim"))) {
            $LocalPackagesDir = (Get-Item $cand).FullName
            break
        }
    }

    # 2. If node_modules is missing, attempt installation or minimal setup
    if (-not (Test-Path $NodeModulesDir)) {
        Write-Host "[SETUP] Missing node_modules in wink-plugin-peripherals. Setting up dependencies..." -ForegroundColor Cyan
        $PackageManager = $null
        if (Get-Command "bun" -ErrorAction SilentlyContinue) {
            $PackageManager = "bun"
        } elseif (Get-Command "npm" -ErrorAction SilentlyContinue) {
            $PackageManager = "npm"
        }

        if ($PackageManager) {
            Write-Host "[SETUP] Running '$PackageManager install' in $ScriptDir..." -ForegroundColor Cyan
            Push-Location $ScriptDir
            try {
                if ($PackageManager -eq "bun") {
                    & bun install
                } else {
                    & npm install --no-audit --no-fund
                }
            } catch {
                Write-Host "[WARN] Package install encountered warning: $_" -ForegroundColor Yellow
            } finally {
                Pop-Location
            }
        } else {
            New-Item -ItemType Directory -Path $NodeModulesDir -Force | Out-Null
        }
    }

    # 3. Fallback devDependencies linking from embedded-frontend/node_modules if needed
    if ($LocalPackagesDir) {
        $FeNodeModules = Join-Path $LocalPackagesDir "embedded-frontend\node_modules"
        if ((Test-Path $FeNodeModules) -and (-not (Test-Path (Join-Path $NodeModulesDir "vue")))) {
            Write-Host "[SETUP] Linking devDependencies from embedded-frontend/node_modules..." -ForegroundColor Cyan
            foreach ($pkg in @("vue", "@wokwi", "vite", "@vitejs", "@types", "typescript", "postcss-prefix-selector")) {
                $srcPkg = Join-Path $FeNodeModules $pkg
                $dstPkg = Join-Path $NodeModulesDir $pkg
                if ((Test-Path $srcPkg) -and (-not (Test-Path $dstPkg))) {
                    try {
                        New-Item -ItemType Junction -Path $dstPkg -Target $srcPkg -Force | Out-Null
                    } catch {
                        # Ignore link fallback warning
                    }
                }
            }
        }

        # 4. Link @wink-ai/unisim and @wink-ai/unisim-ui
        if (-not (Test-Path $WinkAiDir)) {
            New-Item -ItemType Directory -Path $WinkAiDir -Force | Out-Null
        }

        $SrcUnisim = Join-Path $LocalPackagesDir "unisim"
        $SrcUnisimUi = Join-Path $LocalPackagesDir "unisim-ui"

        $LinkUnisim = Join-Path $WinkAiDir "unisim"
        $LinkUnisimUi = Join-Path $WinkAiDir "unisim-ui"

        function Setup-ModuleJunction([string]$target, [string]$link) {
            if (Test-Path $link) {
                $item = Get-Item $link -Force
                if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
                    return
                }
                Write-Host "[SETUP] Replacing npm directory with junction: $link -> $target" -ForegroundColor Cyan
                Remove-Item -Path $link -Recurse -Force
            }
            try {
                New-Item -ItemType Junction -Path $link -Target $target -Force | Out-Null
                Write-Host "[SETUP] Linked $(Split-Path -Leaf $link) -> $target" -ForegroundColor Green
            } catch {
                Write-Host "[WARN] Could not create junction for $link : $_" -ForegroundColor Yellow
            }
        }

        Setup-ModuleJunction $SrcUnisim $LinkUnisim
        Setup-ModuleJunction $SrcUnisimUi $LinkUnisimUi
    }
}

Ensure-PeripheralEnvironment

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
