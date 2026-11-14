# ==============================================================================
# @file packaging/windows/build-windows.ps1
# @brief Automated Build & Packaging script for Windows (Inno Setup & WiX MSI)
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

param(
    [ValidateSet("Release", "Debug", "release", "debug")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent (Split-Path -Parent $ScriptDir)

# Normalize Configuration string
if ($Configuration -eq "debug" -or $Configuration -eq "Debug") {
    $ConfigName = "Debug"
    $IsDebug = $true
    $PkgSuffix = "-debug"
} else {
    $ConfigName = "Release"
    $IsDebug = $false
    $PkgSuffix = ""
}

$BuildDir = Join-Path $RootDir "build\windows-$($ConfigName.ToLower())"
$PackageDir = Join-Path $RootDir "build\packages"

Write-Host "======================================================================"
Write-Host "==> [Windows Build] Chế độ đóng gói: [$($ConfigName.ToUpper())]"
Write-Host "==> [Windows Build] Thư mục biên dịch: $BuildDir"
Write-Host "======================================================================"

# 1. Tạo thư mục build
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null

# 2. Cấu hình CMake & Biên dịch Ninja
Set-Location $RootDir
cmake -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=$ConfigName
ninja -C $BuildDir

# 3. Chạy windeployqt để bundle Qt6 libraries
$SvmExe = Join-Path $BuildDir "ui\svm.exe"
if (Get-Command windeployqt -ErrorAction SilentlyContinue) {
    Write-Host "==> [Windows Build] Đang bundle Qt6 libraries qua windeployqt..."
    $DeployFlag = if ($IsDebug) { "--debug" } else { "--release" }
    windeployqt $DeployFlag --no-translations --compiler-runtime $SvmExe
}

# 4. Đóng gói Inno Setup nếu có ISCC
if (Get-Command iscc -ErrorAction SilentlyContinue) {
    Write-Host "==> [Windows Build] Đang tạo file cài đặt Setup.exe qua Inno Setup..."
    $IssFile = Join-Path $ScriptDir "svm.iss"
    $SetupOutName = "SecureVaultManager-0.1.0$PkgSuffix-Setup"
    & iscc "/DAppMode=$ConfigName" "/DOutputBaseFilename=$SetupOutName" $IssFile
    Write-Host "  ✓ Đã tạo thành công bộ cài Setup.exe trong: $PackageDir"
}

# 5. Đóng gói WiX MSI nếu có candle / light
if (Get-Command candle -ErrorAction SilentlyContinue) {
    Write-Host "==> [Windows Build] Đang tạo Enterprise MSI qua WiX Toolset..."
    $WxsFile = Join-Path $ScriptDir "svm.wxs"
    $WixObj = Join-Path $BuildDir "svm.wixobj"
    $MsiOut = Join-Path $PackageDir "SecureVaultManager-0.1.0$PkgSuffix-x64.msi"
    & candle -nologo -out $WixObj $WxsFile
    & light -nologo -ext WixUIExtension -out $MsiOut $WixObj
    Write-Host "  ✓ Đã tạo thành công Enterprise MSI trong: $MsiOut"
}

Write-Host "======================================================================"
Write-Host "==> [Windows Build] Hoàn tất tiến trình đóng gói Windows [$ConfigName]!"
Write-Host "======================================================================"
