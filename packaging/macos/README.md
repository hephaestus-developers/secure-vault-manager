# macOS Packaging Guide

Tài liệu hướng dẫn dựng ứng dụng `Secure Vault Manager.app`, tạo gói cài đặt đĩa `DMG` và gói cài đặt `PKG` trên macOS (hỗ trợ cả Apple Silicon ARM64 và Intel x86_64).

---

## 1. Yêu cầu Tiền đề (Prerequisites)

```bash
# Cài đặt Xcode Command Line Tools
xcode-select --install

# Cài đặt Qt 6 qua Homebrew (nếu chưa có)
brew install qt@6
```

---

## 2. Các Kịch bản Đóng gói

Tất cả các script đều hỗ trợ cờ `-r, --release` (mặc định) và `-d, --debug`.

### 2.1. Dựng `.app` Bundle

```bash
# Dựng bản Release (tối ưu hóa kích thước, strip symbols):
./packaging/macos/build-macos.sh
# File đầu ra: build/packages/Secure Vault Manager.app

# Dựng bản Debug:
./packaging/macos/build-macos.sh --debug
# File đầu ra: build/packages/Secure Vault Manager (Debug).app
```

- Tự động gọi `macdeployqt` để copy Qt6 frameworks và plugins vào trong bundle.

---

### 2.2. Tạo Disk Image `.dmg`

```bash
# Tạo file DMG Release:
./packaging/macos/build-dmg.sh
# File đầu ra: build/packages/SecureVaultManager-0.1.0.dmg

# Tạo file DMG Debug:
./packaging/macos/build-dmg.sh --debug
# File đầu ra: build/packages/SecureVaultManager-0.1.0-debug.dmg
```

---

### 2.3. Tạo Installer `.pkg`

```bash
# Tạo file PKG Release:
./packaging/macos/build-pkg.sh
# File đầu ra: build/packages/SecureVaultManager-0.1.0.pkg

# Tạo file PKG Debug:
./packaging/macos/build-pkg.sh --debug
# File đầu ra: build/packages/SecureVaultManager-0.1.0-debug.pkg
```
