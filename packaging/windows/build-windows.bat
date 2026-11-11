@echo off
rem ==============================================================================
rem @file packaging/windows/build-windows.bat
rem @brief Windows Batch wrapper for build-windows.ps1
rem @author Hai Pham Ngoc <ngochai285nd@gmail.com>
rem @copyright (c) 2026 Hephaestus Developers. All rights reserved.
rem ==============================================================================

setlocal
set SCRIPT_DIR=%~dp0
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build-windows.ps1" %*
endlocal
