@echo off
rem ==============================================================================
rem @file packaging/windows/build-windows.bat
rem @brief Windows Batch wrapper for build-windows.ps1 (Supports Debug & Release)
rem @author Hai Pham Ngoc <ngochai285nd@gmail.com>
rem @copyright (c) 2026 Hephaestus Developers. All rights reserved.
rem ==============================================================================

setlocal
set SCRIPT_DIR=%~dp0
set MODE=Release

if /I "%~1"=="-d" set MODE=Debug
if /I "%~1"=="--debug" set MODE=Debug
if /I "%~1"=="debug" set MODE=Debug
if /I "%~1"=="-r" set MODE=Release
if /I "%~1"=="--release" set MODE=Release
if /I "%~1"=="release" set MODE=Release

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build-windows.ps1" -Configuration %MODE%
endlocal
