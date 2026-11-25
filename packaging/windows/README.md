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
.\packaging\windows\package_windows.ps1

# Đóng gói bản Debug:
.\packaging\windows\package_windows.ps1 -Configuration Debug

# Chỉ tạo bộ cài Inno Setup EXE:
.\packaging\windows\package_windows.ps1 -Target Inno
```

---

### 2.2. Sử dụng Command Prompt (CMD)

```cmd
:: Đóng gói toàn bộ các định dạng (Setup.exe, .msi, .zip):
packaging\windows\package_windows.bat

:: Chỉ tạo bộ cài Inno Setup EXE:
packaging\windows\package_windows.bat /INNO

:: Chỉ tạo bộ cài WiX MSI:
packaging\windows\package_windows.bat /MSI

:: Đóng gói bản Debug:
packaging\windows\package_windows.bat /DEBUG
```

---

## 3. Tệp cài đặt đầu ra

Toàn bộ sản phẩm phân phối xuất xưởng sẽ được tập trung tại thư mục `dist\`:

- `dist\SecureVaultManager-2026.11.0-Setup.exe` (Inno Setup Installer cho End User)
- `dist\SecureVaultManager-2026.11.0-windows-x64.msi` (WiX Toolset Installer cho Enterprise IT / GPO)
- `dist\SecureVaultManager-2026.11.0-windows-x64-portable.zip` (Portable Archive)
- `dist\SHA256SUMS` (Bảng mã băm tính toàn vẹn)
- `dist\release-manifest.json` (Metadata phiên bản phát hành)

- **Bản Release**:
  - `SecureVaultManager-2026.11.0-Setup.exe` (Inno Setup)
  - `SecureVaultManager-2026.11.0-x64.msi` (Enterprise MSI)
- **Bản Debug**:
  - `SecureVaultManager-2026.11.0-debug-Setup.exe` (Inno Setup)
  - `SecureVaultManager-2026.11.0-debug-x64.msi` (Enterprise MSI)
