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

Mở PowerShell với quyền Administrator hoặc Developer PowerShell for VS:

```powershell
.\packaging\windows\build-windows.ps1
```

Hoặc qua Command Prompt:

```cmd
packaging\windows\build-windows.bat
```

Bộ cài đặt sẽ được tạo tại `build\packages\`:

- `SecureVaultManager-0.1.0-Setup.exe` (Inno Setup)
- `SecureVaultManager-0.1.0-x64.msi` (Enterprise MSI)
