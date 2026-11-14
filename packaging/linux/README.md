# Linux Packaging Guide

Tài liệu hướng dẫn đóng gói Secure Vault Manager trên các bản phân phối Linux:
Debian / Ubuntu / Linux Mint (`.deb`), Red Hat / Fedora (`.rpm`), và portable binary (`AppImage`).

---

## 1. Yêu cầu Tiền đề (Prerequisites)

```bash
# Debian / Ubuntu / Mint
sudo apt-get install -y dpkg-dev build-essential librsvg2-bin binutils

# Fedora / RHEL
sudo dnf install -y rpm-build librsvg2-tools binutils
```

---

## 2. Kịch bản Đóng gói (Packaging Scripts)

Tất cả kịch bản đóng gói đều hỗ trợ độc lập hai chế độ:

- **`--release` (Mặc định)**: Tối ưu dung lượng tối đa, bật LTO toàn diện và tự động loại bỏ debug symbols (strip).
- **`--debug`**: Giữ nguyên toàn bộ debug symbols (DWARF info) dành cho việc gỡ lỗi (`gdb`, `lldb`, `valgrind`).

---

### 2.1. Đóng gói Debian / Ubuntu (.deb)

Script `build-deb.sh` hỗ trợ hai chế độ phân phối:

1. **Standalone Bundle (Mặc định)**: Tự động nhúng toàn bộ thư viện Qt6, ICU và Qt platform plugins (`platforms/`, `imageformats/`). Gói `.deb` hoạt động độc lập trên **bất kỳ bản phân phối Linux nào** (Ubuntu 20.04/22.04/24.04, Debian 11/12, Linux Mint, v.v.) mà không yêu cầu hệ điều hành phải có sẵn đúng phiên bản Qt.
2. **System Qt (Tùy chọn)**: Sử dụng các gói Qt có sẵn trong kho phần mềm của hệ điều hành. Gói siêu nhẹ (~3 MB), phù hợp khi phân phối qua kho PPA hoặc repository chính thức.

```bash
# 1. Đóng gói Release Standalone (Khuyến nghị, nhúng Qt6, tương thích mọi distro Linux):
./packaging/linux/build-deb.sh
# Hoặc: ./packaging/linux/build-deb.sh --release --bundle-qt
# File đầu ra: build/packages/secure-vault-manager_0.1.0_amd64.deb (~21 MB)

# 2. Đóng gói Release siêu nhẹ dùng Qt hệ thống:
./packaging/linux/build-deb.sh --system-qt
# File đầu ra: build/packages/secure-vault-manager_0.1.0_amd64.deb (~3 MB)

# 3. Đóng gói bản Debug (chứa debug info gỡ lỗi):
./packaging/linux/build-deb.sh --debug
# File đầu ra: build/packages/secure-vault-manager-debug_0.1.0_amd64.deb
```

- Cấu trúc cài đặt chuẩn Enterprise:
  - Cài đặt toàn bộ binaries & private shared libraries vào `/opt/secure-vault-manager/` (`bin/svm`, `bin/svm-cli`, `bin/svm-native-host`, `lib/libsvm_core.so`).
  - Tự động gắn symlink vào hệ thống: `/usr/bin/svm`, `/usr/bin/svm-cli`, `/usr/bin/svm-native-host`.
  - Tự động đăng ký Native Messaging Manifest cho Chromium, Chrome, Brave, Edge và Firefox.
- Lệnh cài đặt thử nghiệm:

  ```bash
  sudo apt install ./build/packages/secure-vault-manager_0.1.0_amd64.deb
  ```

---

### 2.2. Đóng gói Portable AppImage

AppImage được tạo ra với đầy đủ Qt6, ICU, Qt platform plugins và thư viện Rust Core. Chạy được ngay trên mọi Linux chỉ với 1 click:

```bash
# Đóng gói bản Release AppImage tự chứa:
./packaging/linux/build-appimage.sh
# File đầu ra: build/packages/SecureVaultManager-0.1.0-x86_64.AppImage
# (và thư mục thực thi sẵn: build/packages/AppDir-release)

# Đóng gói bản Debug (chứa debug symbols):
./packaging/linux/build-appimage.sh --debug
# File đầu ra: build/packages/SecureVaultManager-0.1.0-debug-x86_64.AppImage
```

---

### 2.3. Đóng gói Red Hat / Fedora (.rpm)

```bash
# Đóng gói bản Release Standalone (.rpm chứa sẵn Qt6):
./packaging/linux/build-rpm.sh
# Hoặc đóng gói siêu nhẹ dùng Qt hệ thống:
./packaging/linux/build-rpm.sh --system-qt
# File đầu ra: build/packages/secure-vault-manager-0.1.0-1.x86_64.rpm

# Đóng gói bản Debug:
./packaging/linux/build-rpm.sh --debug
# File đầu ra: build/packages/secure-vault-manager-debug-0.1.0-1.x86_64.rpm
```
