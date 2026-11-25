<#
.SYNOPSIS
    Automated packaging script for Secure Vault Manager on Windows (x64).
    Produces Portable Folder, Inno Setup Installer (.exe), WiX Installer (.msi), and Portable ZIP.

.DESCRIPTION
    1. Compiles the build using CMake Presets (Release or Debug).
    2. Gathers binaries, DLLs, assets, and native messaging manifests into dist/secure-vault-manager-2026.11.0-windows-x64.
    3. Runs windeployqt to bundle all required Qt 6 runtime libraries.
    4. Compiles Setup Executable (.exe) using Inno Setup.
    5. Compiles Windows Installer (.msi) using WiX Toolset.
    6. Compiles Portable ZIP archive.

.EXAMPLE
    .\package_windows.ps1
    .\package_windows.ps1 -Configuration Debug -Target Inno
#>

[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug", "release", "debug")]
    [string]$Configuration = "Release",

    [ValidateSet("All", "Inno", "Msi", "Portable")]
    [string]$Target = "All",

    [switch]$SkipBuild = $false
)

$ErrorActionPreference = "Stop"

$AppName = "secure-vault-manager"
$PkgBaseName = "SecureVaultManager"
$Version = "2026.11.0"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = (Resolve-Path "$ScriptDir\..\..").Path
$DistDir = Join-Path $ProjectRoot "dist"
$BundleDir = Join-Path $DistDir "$AppName-$Version-windows-x64"

$ConfigName = if ($Configuration.ToLower() -eq "debug") { "debug" } else { "release" }

Write-Host "====================================================" -ForegroundColor Cyan
Write-Host "  Packaging $AppName v$Version for Windows (x64)" -ForegroundColor Cyan
Write-Host "  Mode: [$($ConfigName.ToUpper())] | Target: [$Target]" -ForegroundColor Cyan
Write-Host "====================================================" -ForegroundColor Cyan

# -----------------------------------------------------------------------------
# 1. Build application via CMake Presets
# -----------------------------------------------------------------------------
if (-not $SkipBuild) {
    Write-Host "`n-> [1/6] Building application ($ConfigName mode)..." -ForegroundColor Yellow
    Set-Location $ProjectRoot
    cmake --preset $ConfigName
    cmake --build --preset "build-$ConfigName"
}

$ReleaseBin = Join-Path $ProjectRoot "build\$ConfigName\svm.exe"
$CliBin = Join-Path $ProjectRoot "build\$ConfigName\svm-cli.exe"
$HostBin = Join-Path $ProjectRoot "build\$ConfigName\svm-native-host.exe"
$ReleaseLib = Join-Path $ProjectRoot "build\$ConfigName\svm_core.dll"
if (-not (Test-Path $ReleaseLib)) {
    $ReleaseLib = Join-Path $ProjectRoot "build\$ConfigName\libsvm_core.dll"
}

if (-not (Test-Path $ReleaseBin)) {
    throw "Application binary not found: $ReleaseBin"
}

# -----------------------------------------------------------------------------
# 2. Prepare staging bundle directory
# -----------------------------------------------------------------------------
Write-Host "`n-> [2/6] Preparing staging directory: $BundleDir" -ForegroundColor Yellow
if (Test-Path $BundleDir) {
    Remove-Item -Path $BundleDir -Recurse -Force
}
New-Item -ItemType Directory -Path $BundleDir -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $BundleDir "native-messaging") -Force | Out-Null
New-Item -ItemType Directory -Path $DistDir -Force | Out-Null

# -----------------------------------------------------------------------------
# 3. Copy application binaries, configs, and assets
# -----------------------------------------------------------------------------
Write-Host "`n-> [3/6] Copying executable, Rust Core DLL, and assets..." -ForegroundColor Yellow
Copy-Item $ReleaseBin -Destination $BundleDir
if (Test-Path $CliBin) { Copy-Item $CliBin -Destination $BundleDir }
if (Test-Path $HostBin) { Copy-Item $HostBin -Destination $BundleDir }
if (Test-Path $ReleaseLib) { Copy-Item $ReleaseLib -Destination (Join-Path $BundleDir "svm_core.dll") }

Copy-Item (Join-Path $ScriptDir "svm.ico") -Destination $BundleDir -ErrorAction SilentlyContinue
Copy-Item (Join-Path $ProjectRoot "README.md") -Destination $BundleDir -ErrorAction SilentlyContinue

# Generate Native Messaging manifests
$ChromeJson = @{
    name = "io.hephaestus.secure_vault_manager"
    description = "Secure Vault Manager Native Messaging Host"
    path = "svm-native-host.exe"
    type = "stdio"
    allowed_origins = @("chrome-extension://jfgmpgpijiloaglplchmjomeboglioec/")
} | ConvertTo-Json -Depth 4
Set-Content -Path (Join-Path $BundleDir "native-messaging\chrome.json") -Value $ChromeJson

$FirefoxJson = @{
    name = "io.hephaestus.secure_vault_manager"
    description = "Secure Vault Manager Native Messaging Host"
    path = "svm-native-host.exe"
    type = "stdio"
    allowed_extensions = @("svm@hephaestus.io")
} | ConvertTo-Json -Depth 4
Set-Content -Path (Join-Path $BundleDir "native-messaging\firefox.json") -Value $FirefoxJson

# -----------------------------------------------------------------------------
# 4. Deploy Qt runtime libraries with windeployqt
# -----------------------------------------------------------------------------
Write-Host "`n-> [4/6] Deploying Qt 6 runtime dependencies with windeployqt..." -ForegroundColor Yellow
if (Get-Command windeployqt -ErrorAction SilentlyContinue) {
    $DeployFlag = if ($ConfigName -eq "debug") { "--debug" } else { "--release" }
    & windeployqt $DeployFlag --no-translations --compiler-runtime (Join-Path $BundleDir "svm.exe")
    Write-Host "       ✓ Qt 6 dependencies successfully bundled." -ForegroundColor Green
} else {
    Write-Host "   [!] Warning: windeployqt not found in PATH." -ForegroundColor DarkYellow
}

# -----------------------------------------------------------------------------
# 5. Build Inno Setup Installer (.exe)
# -----------------------------------------------------------------------------
if ($Target -eq "All" -or $Target -eq "Inno") {
    Write-Host "`n-> [5/6] Building Inno Setup Installer (.exe)..." -ForegroundColor Yellow
    $IsccCmd = $null
    if (Get-Command iscc -ErrorAction SilentlyContinue) { $IsccCmd = "iscc" }
    elseif (Test-Path "C:\Program Files (x86)\Inno Setup 6\ISCC.exe") { $IsccCmd = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" }
    elseif (Test-Path "C:\Program Files\Inno Setup 6\ISCC.exe") { $IsccCmd = "C:\Program Files\Inno Setup 6\ISCC.exe" }

    if ($IsccCmd) {
        $IssFile = Join-Path $ScriptDir "svm.iss"
        & $IsccCmd "/Q" "/DSourceBundleDir=$BundleDir" $IssFile
        Write-Host "       ✓ Generated: $DistDir\$PkgBaseName-$Version-Setup.exe" -ForegroundColor Green
    } else {
        Write-Host "   [!] Inno Setup (ISCC.exe) not found. Skipping .exe installer." -ForegroundColor DarkYellow
    }
}

# -----------------------------------------------------------------------------
# 6. Build WiX Toolset Installer (.msi)
# -----------------------------------------------------------------------------
if ($Target -eq "All" -or $Target -eq "Msi") {
    Write-Host "`n-> [6/6] Building WiX Toolset Installer (.msi)..." -ForegroundColor Yellow
    $MsiOutput = Join-Path $DistDir "$PkgBaseName-$Version-windows-x64.msi"
    $WxsFile = Join-Path $ScriptDir "svm.wxs"

    if (Get-Command wix -ErrorAction SilentlyContinue) {
        & wix build -arch x64 $WxsFile -d SourceDir="$BundleDir" -o $MsiOutput
        Write-Host "       ✓ Generated: $MsiOutput" -ForegroundColor Green
    } elseif (Get-Command candle -ErrorAction SilentlyContinue) {
        & candle -arch x64 -dSourceDir="$BundleDir" $WxsFile -out "$ScriptDir\"
        & light -ext WixUIExtension "$ScriptDir\svm.wixobj" -out $MsiOutput
        Remove-Item "$ScriptDir\*.wixobj", "$ScriptDir\*.wixpdb" -Force -ErrorAction SilentlyContinue
        Write-Host "       ✓ Generated: $MsiOutput" -ForegroundColor Green
    } else {
        Write-Host "   [!] WiX Toolset not found. Skipping .msi installer." -ForegroundColor DarkYellow
    }
}

# -----------------------------------------------------------------------------
# 7. Build Portable ZIP Archive (.zip)
# -----------------------------------------------------------------------------
if ($Target -eq "All" -or $Target -eq "Portable") {
    Write-Host "`n-> Creating Portable ZIP Archive (.zip)..." -ForegroundColor Yellow
    $ZipOutput = Join-Path $DistDir "$PkgBaseName-$Version-windows-x64-portable.zip"
    Compress-Archive -Path "$BundleDir\*" -DestinationPath $ZipOutput -Force
    if (Test-Path $ZipOutput) {
        Write-Host "       ✓ Generated: $ZipOutput" -ForegroundColor Green
    }
}

Write-Host "`n====================================================" -ForegroundColor Cyan
Write-Host "✓ Windows Packaging Complete!" -ForegroundColor Cyan
Write-Host "  Artifacts located in: $DistDir" -ForegroundColor Cyan
Write-Host "====================================================" -ForegroundColor Cyan
Get-ChildItem -Path $DistDir -Filter "*windows*" | Select-Object Name, Length
Get-ChildItem -Path $DistDir -Filter "*Setup.exe" | Select-Object Name, Length
Get-ChildItem -Path $DistDir -Filter "*.msi" | Select-Object Name, Length
