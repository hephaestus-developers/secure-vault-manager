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

### 2.1. Đóng gói Tổng hợp (Khuyến nghị)

```bash
# Đóng gói toàn bộ (.app, .dmg, .pkg, .zip):
./packaging/macos/package_macos.sh

# Chỉ tạo Apple Disk Image (.dmg):
./packaging/macos/package_macos.sh --dmg

# Chỉ tạo Installer Package (.pkg):
./packaging/macos/package_macos.sh --pkg

# Đóng gói bản Debug:
./packaging/macos/package_macos.sh --debug
```

- Tự động gọi `macdeployqt` để copy Qt6 frameworks và plugins vào trong bundle.
- Tự động sửa `@rpath` bằng `install_name_tool`.
- Toàn bộ sản phẩm xuất xưởng được đưa vào thư mục `dist/` kèm file `SHA256SUMS` và `release-manifest.json`.

---

### 2.2. Tạo Disk Image `.dmg`

```bash
# Tạo file DMG Release:
./packaging/macos/build-dmg.sh
# File đầu ra: build/packages/SecureVaultManager-2026.11.0.dmg

# Tạo file DMG Debug:
./packaging/macos/build-dmg.sh --debug
# File đầu ra: build/packages/SecureVaultManager-2026.11.0-debug.dmg
```

---

### 2.3. Tạo Installer `.pkg`

```bash
# Tạo file PKG Release:
./packaging/macos/build-pkg.sh
# File đầu ra: build/packages/SecureVaultManager-2026.11.0.pkg

# Tạo file PKG Debug:
./packaging/macos/build-pkg.sh --debug
# File đầu ra: build/packages/SecureVaultManager-2026.11.0-debug.pkg
```
