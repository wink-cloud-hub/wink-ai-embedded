@echo off
:: Set code page to UTF-8 to support Chinese characters display in CMD
chcp 65001 > nul

echo ============================================================
echo  Implementation Plans Status Viewer
echo ============================================================

set PYTHON_EXE=

:: Check if python is available in PATH
call python --version >nul 2>&1
if %errorlevel% equ 0 (
    set PYTHON_EXE=python
    goto run
)

:: Check if py (Python Launcher) is available
call py --version >nul 2>&1
if %errorlevel% equ 0 (
    set PYTHON_EXE=py
    goto run
)

echo [ERROR] Python was not found in your PATH.
echo Please make sure Python is installed and added to your System PATH variables.
goto end

:run
call "%PYTHON_EXE%" "%~dp0list_plans.py" %*

:end
echo ============================================================
echo.
pause
