@echo off
title Build All Peripheral Plugins
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-peripherals.ps1"
echo.
echo Press any key to exit...
pause >nul
