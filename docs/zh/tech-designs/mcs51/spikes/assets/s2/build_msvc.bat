@echo off
call "D:\software-install\VS\VisualStudio2022\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai-embedded
set A=docs\tech-designs\mcs51\spikes\assets\s2
cl /nologo /std:c++17 /EHsc /W3 /wd4101 /I %A%\shim %A%\build\user_blinky.cpp %A%\s2_harness.cpp /Fe:%A%\build\s2_msvc.exe /Fo:%A%\build\
if errorlevel 1 (echo MSVC_BUILD_FAIL & exit /b 1)
%A%\build\s2_msvc.exe
echo MSVC_EXIT=%errorlevel%
