@echo off
REM ==============================================================================
REM @file packaging/windows/build-windows.bat
REM @brief Compatibility wrapper delegating to package_windows.bat
REM @author Hai Pham Ngoc <ngochai285nd@gmail.com>
REM @copyright (c) 2026 Hephaestus Developers. All rights reserved.
REM ==============================================================================
setlocal
set SCRIPT_DIR=%~dp0
call "%SCRIPT_DIR%package_windows.bat" %*
endlocal
