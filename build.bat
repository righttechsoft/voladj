@echo off
if defined VSCMD_VER goto build
set VC="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist %VC% (
    echo vcvars64.bat not found: %VC%
    exit /b 1
)
call %VC% >nul
:build
cd /d "%~dp0"
cl /nologo /O1 /GS- voladj.c /link /SUBSYSTEM:WINDOWS /NODEFAULTLIB /ENTRY:entry kernel32.lib user32.lib ole32.lib
