# Windows Packaging Guide

Tài liệu hướng dẫn biên dịch và đóng gói Secure Vault Manager trên Windows 10/11 (hỗ trợ cả installer `.exe` qua Inno Setup và Enterprise MSI installer qua WiX Toolset).

---

## 1. Yêu cầu Tiền đề (Prerequisites)

- Visual Studio 2022 C++ Build Tools (MSVC 19.3x+).
- Rust toolchain (`stable-x86_64-pc-windows-msvc`).
- Qt 6.6+ (MSVC 64-bit).
- Inno Setup 6 (để build file `Setup.exe` qua lệnh `iscc`).
- WiX Toolset v3.11+ (để build file Enterprise `.msi` qua `candle` & `light`).

---

## 2. Kịch bản Đóng gói Tự Động

Tất cả kịch bản đều hỗ trợ cấu hình `Release` (mặc định) và `Debug`.

### 2.1. Sử dụng PowerShell

Mở PowerShell với quyền Administrator hoặc Developer PowerShell for VS:

```powershell
# Đóng gói bản Release chuẩn (nhỏ gọn nhất, khuyến nghị):
.\packaging\windows\build-windows.ps1 -Configuration Release

# Đóng gói bản Debug (kèm PDB debug info cho WinDbg/Visual Studio):
.\packaging\windows\build-windows.ps1 -Configuration Debug
```

---

### 2.2. Sử dụng Command Prompt (CMD)

```cmd
:: Đóng gói bản Release:
packaging\windows\build-windows.bat
rem Hoặc: packaging\windows\build-windows.bat --release

:: Đóng gói bản Debug:
packaging\windows\build-windows.bat --debug
```

---

## 3. Tệp cài đặt đầu ra

Các bộ cài đặt sẽ được tạo tại thư mục `build\packages\`:

- **Bản Release**:
  - `SecureVaultManager-0.1.0-Setup.exe` (Inno Setup)
  - `SecureVaultManager-0.1.0-x64.msi` (Enterprise MSI)
- **Bản Debug**:
  - `SecureVaultManager-0.1.0-debug-Setup.exe` (Inno Setup)
  - `SecureVaultManager-0.1.0-debug-x64.msi` (Enterprise MSI)
