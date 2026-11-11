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

### 2.1. Dựng `.app` Bundle

```bash
bash packaging/macos/build-macos.sh
```

- Kết xuất thư mục `build/packages/Secure Vault Manager.app`.
- Tự động gọi `macdeployqt` để copy Qt6 frameworks và plugins vào trong bundle.

### 2.2. Tạo Disk Image `.dmg`

```bash
bash packaging/macos/build-dmg.sh
```

- Tạo file `build/packages/SecureVaultManager-0.1.0.dmg`.

### 2.3. Tạo Installer `.pkg`

```bash
bash packaging/macos/build-pkg.sh
```

- Tạo file cài đặt `build/packages/SecureVaultManager-0.1.0.pkg`.
