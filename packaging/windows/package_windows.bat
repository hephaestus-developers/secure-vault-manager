@echo off
REM ==============================================================================
REM @file packaging/windows/package_windows.bat
REM @brief Comprehensive Windows Packaging Script (Inno Setup EXE, WiX MSI, Portable ZIP)
REM @author Hai Pham Ngoc <ngochai285nd@gmail.com>
REM @copyright (c) 2026 Hephaestus Developers. All rights reserved.
REM ==============================================================================
setlocal enabledelayedexpansion

set "APP_NAME=secure-vault-manager"
set "PKG_BASE_NAME=SecureVaultManager"
set "VERSION=0.1.0"
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%..\.."
set "DIST_DIR=%PROJECT_ROOT%\dist"
set "BUNDLE_DIR=%DIST_DIR%\%APP_NAME%-%VERSION%-windows-x64"

set "BUILD_MODE=release"
set "TARGET_ALL=true"
set "TARGET_INNO=false"
set "TARGET_MSI=false"
set "TARGET_PORTABLE=false"

:parse_args
if "%~1"=="" goto after_args
if /I "%~1"=="/ALL" set "TARGET_ALL=true" & shift & goto parse_args
if /I "%~1"=="--all" set "TARGET_ALL=true" & shift & goto parse_args
if /I "%~1"=="/INNO" set "TARGET_ALL=false" & set "TARGET_INNO=true" & shift & goto parse_args
if /I "%~1"=="--inno" set "TARGET_ALL=false" & set "TARGET_INNO=true" & shift & goto parse_args
if /I "%~1"=="/MSI" set "TARGET_ALL=false" & set "TARGET_MSI=true" & shift & goto parse_args
if /I "%~1"=="--msi" set "TARGET_ALL=false" & set "TARGET_MSI=true" & shift & goto parse_args
if /I "%~1"=="/PORTABLE" set "TARGET_ALL=false" & set "TARGET_PORTABLE=true" & shift & goto parse_args
if /I "%~1"=="--portable" set "TARGET_ALL=false" & set "TARGET_PORTABLE=true" & shift & goto parse_args
if /I "%~1"=="/RELEASE" set "BUILD_MODE=release" & shift & goto parse_args
if /I "%~1"=="-r" set "BUILD_MODE=release" & shift & goto parse_args
if /I "%~1"=="--release" set "BUILD_MODE=release" & shift & goto parse_args
if /I "%~1"=="/DEBUG" set "BUILD_MODE=debug" & shift & goto parse_args
if /I "%~1"=="-d" set "BUILD_MODE=debug" & shift & goto parse_args
if /I "%~1"=="--debug" set "BUILD_MODE=debug" & shift & goto parse_args
if /I "%~1"=="/?" goto show_help
if /I "%~1"=="-h" goto show_help
if /I "%~1"=="--help" goto show_help

echo [ERROR] Unknown option: %~1
goto show_help

:show_help
echo Usage: package_windows.bat [OPTIONS]
echo.
echo Target Formats:
echo   /ALL           Build all formats (Setup.exe, .msi, .zip - Default)
echo   /INNO          Build Inno Setup executable installer (.exe)
echo   /MSI           Build WiX Toolset Windows Installer (.msi)
echo   /PORTABLE      Build standalone portable ZIP archive (.zip)
echo.
echo Build Configurations:
echo   /RELEASE       Build in Release mode (Default)
echo   /DEBUG         Build in Debug mode
echo   /?, -h, --help Show this help
exit /b 0

:after_args
if "%TARGET_ALL%"=="true" (
    set "TARGET_INNO=true"
    set "TARGET_MSI=true"
    set "TARGET_PORTABLE=true"
)

echo ====================================================
echo   Packaging %APP_NAME% v%VERSION% for Windows (x64)
echo   Mode: [%BUILD_MODE%]
echo   Targets: INNO=%TARGET_INNO%, MSI=%TARGET_MSI%, PORTABLE=%TARGET_PORTABLE%
echo ====================================================

REM -----------------------------------------------------------------------------
REM 1. Build application via CMake Presets
REM -----------------------------------------------------------------------------
echo.
echo -> [1/6] Building application (%BUILD_MODE% mode)...
cd /d "%PROJECT_ROOT%"
cmake --preset "%BUILD_MODE%"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

cmake --build --preset "build-%BUILD_MODE%"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Compilation failed.
    exit /b 1
)

set "RELEASE_BIN=%PROJECT_ROOT%\build\%BUILD_MODE%\svm.exe"
set "CLI_BIN=%PROJECT_ROOT%\build\%BUILD_MODE%\svm-cli.exe"
set "HOST_BIN=%PROJECT_ROOT%\build\%BUILD_MODE%\svm-native-host.exe"
set "RELEASE_LIB=%PROJECT_ROOT%\build\%BUILD_MODE%\svm_core.dll"
if not exist "%RELEASE_LIB%" (
    set "RELEASE_LIB=%PROJECT_ROOT%\build\%BUILD_MODE%\libsvm_core.dll"
)

if not exist "%RELEASE_BIN%" (
    echo [ERROR] Application binary not found: %RELEASE_BIN%
    exit /b 1
)

if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

REM -----------------------------------------------------------------------------
REM 2. Prepare staging bundle directory
REM -----------------------------------------------------------------------------
echo.
echo -> [2/6] Preparing staging directory: %BUNDLE_DIR%
if exist "%BUNDLE_DIR%" rmdir /s /q "%BUNDLE_DIR%"
mkdir "%BUNDLE_DIR%"
mkdir "%BUNDLE_DIR%\native-messaging"

REM -----------------------------------------------------------------------------
REM 3. Copy application binaries, configs, and assets
REM -----------------------------------------------------------------------------
echo.
echo -> [3/6] Copying executable, Rust Core DLL, and assets...
copy /y "%RELEASE_BIN%" "%BUNDLE_DIR%\" >nul
if exist "%CLI_BIN%" copy /y "%CLI_BIN%" "%BUNDLE_DIR%\" >nul
if exist "%HOST_BIN%" copy /y "%HOST_BIN%" "%BUNDLE_DIR%\" >nul
if exist "%RELEASE_LIB%" copy /y "%RELEASE_LIB%" "%BUNDLE_DIR%\svm_core.dll" >nul

copy /y "%SCRIPT_DIR%svm.ico" "%BUNDLE_DIR%\" >nul 2>&1
copy /y "%PROJECT_ROOT%\README.md" "%BUNDLE_DIR%\" >nul 2>&1
if exist "%PROJECT_ROOT%\LICENSE" copy /y "%PROJECT_ROOT%\LICENSE" "%BUNDLE_DIR%\" >nul

REM Generate Native Messaging Manifests for Windows
(
    echo {
    echo   "name": "io.hephaestus.secure_vault_manager",
    echo   "description": "Secure Vault Manager Native Messaging Host",
    echo   "path": "svm-native-host.exe",
    echo   "type": "stdio",
    echo   "allowed_origins": [
    echo     "chrome-extension://jfgmpgpijiloaglplchmjomeboglioec/"
    echo   ]
    echo }
) > "%BUNDLE_DIR%\native-messaging\chrome.json"

(
    echo {
    echo   "name": "io.hephaestus.secure_vault_manager",
    echo   "description": "Secure Vault Manager Native Messaging Host",
    echo   "path": "svm-native-host.exe",
    echo   "type": "stdio",
    echo   "allowed_extensions": [
    echo     "svm@hephaestus.io"
    echo   ]
    echo }
) > "%BUNDLE_DIR%\native-messaging\firefox.json"

REM -----------------------------------------------------------------------------
REM 4. Deploy Qt libraries using windeployqt
REM -----------------------------------------------------------------------------
echo.
echo -> [4/6] Deploying Qt 6 runtime dependencies with windeployqt...
where windeployqt >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set "DEPLOY_FLAG=--release"
    if /I "%BUILD_MODE%"=="debug" set "DEPLOY_FLAG=--debug"
    windeployqt !DEPLOY_FLAG! --no-translations --compiler-runtime "%BUNDLE_DIR%\svm.exe"
    echo        ✓ Qt 6 dependencies successfully bundled.
) else (
    echo    [!] Warning: windeployqt not found in PATH.
)

REM -----------------------------------------------------------------------------
REM 5. Build Inno Setup Installer (.exe)
REM -----------------------------------------------------------------------------
if "%TARGET_INNO%"=="true" (
    echo.
    echo -> [5/6] Building Inno Setup Installer (.exe)...
    set "ISCC_CMD="
    where iscc >nul 2>&1
    if %ERRORLEVEL% equ 0 set "ISCC_CMD=iscc"
    if not defined ISCC_CMD if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" set "ISCC_CMD=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    if not defined ISCC_CMD if exist "C:\Program Files\Inno Setup 6\ISCC.exe" set "ISCC_CMD=C:\Program Files\Inno Setup 6\ISCC.exe"

    if defined ISCC_CMD (
        "!ISCC_CMD!" /Q "/DSourceBundleDir=%BUNDLE_DIR%" "%SCRIPT_DIR%svm.iss"
        if %ERRORLEVEL% equ 0 (
            echo        ✓ Generated: %DIST_DIR%\%PKG_BASE_NAME%-%VERSION%-Setup.exe
        ) else (
            echo    [!] Inno Setup failed.
        )
    ) else (
        echo    [!] Inno Setup (ISCC.exe) not found. Skipping .exe installer.
    )
)

REM -----------------------------------------------------------------------------
REM 6. Build WiX Toolset Installer (.msi)
REM -----------------------------------------------------------------------------
if "%TARGET_MSI%"=="true" (
    echo.
    echo -> [6/6] Building WiX Toolset Windows Installer (.msi)...
    set "MSI_OUTPUT=%DIST_DIR%\%PKG_BASE_NAME%-%VERSION%-windows-x64.msi"

    where wix >nul 2>&1
    if %ERRORLEVEL% equ 0 (
        wix build -arch x64 "%SCRIPT_DIR%svm.wxs" -d SourceDir="%BUNDLE_DIR%" -o "%MSI_OUTPUT%" >nul 2>&1
        if %ERRORLEVEL% equ 0 (
            echo        ✓ Generated: %MSI_OUTPUT%
            goto :after_msi
        )
    )

    where candle >nul 2>&1
    if %ERRORLEVEL% equ 0 (
        candle -arch x64 -dSourceDir="%BUNDLE_DIR%" "%SCRIPT_DIR%svm.wxs" -out "%SCRIPT_DIR%" >nul 2>&1
        light -ext WixUIExtension "%SCRIPT_DIR%svm.wixobj" -out "%MSI_OUTPUT%" >nul 2>&1
        del /f /q "%SCRIPT_DIR%*.wixobj" "%SCRIPT_DIR%*.wixpdb" >nul 2>&1
        if %ERRORLEVEL% equ 0 (
            echo        ✓ Generated: %MSI_OUTPUT%
        ) else (
            echo    [!] WiX v3 light.exe failed.
        )
    ) else (
        echo    [!] WiX Toolset not found. Skipping .msi installer.
    )
)
:after_msi

REM -----------------------------------------------------------------------------
REM 7. Build Portable ZIP Archive (.zip)
REM -----------------------------------------------------------------------------
if "%TARGET_PORTABLE%"=="true" (
    echo.
    echo -> Creating Portable ZIP Archive (.zip)...
    set "ZIP_OUTPUT=%DIST_DIR%\%PKG_BASE_NAME%-%VERSION%-windows-x64-portable.zip"
    powershell.exe -NoProfile -Command "Compress-Archive -Path '%BUNDLE_DIR%\*' -DestinationPath '%ZIP_OUTPUT%' -Force" 2>nul
    if exist "%ZIP_OUTPUT%" (
        echo        ✓ Generated: %ZIP_OUTPUT%
    )
)

echo.
echo ====================================================
echo ✓ Windows Packaging Pipeline Complete!
echo   Artifacts directory: %DIST_DIR%
echo ====================================================
dir "%DIST_DIR%\*windows*" "%DIST_DIR%\*.exe" "%DIST_DIR%\*.msi" "%DIST_DIR%\*.zip" 2>nul
