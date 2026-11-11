# Linux Packaging Guide

Tài liệu hướng dẫn đóng gói Secure Vault Manager trên các bản phân phối Linux:
Debian / Ubuntu / Linux Mint (`.deb`), Red Hat / Fedora (`.rpm`), và portable binary (`AppImage`).

---

## 1. Yêu cầu Tiền đề (Prerequisites)

```bash
# Debian / Ubuntu / Mint
sudo apt-get install -y dpkg-dev build-essential librsvg2-bin

# Fedora / RHEL
sudo dnf install -y rpm-build librsvg2-tools
```

---

## 2. Kịch bản Đóng gói

### 2.1. Đóng gói Debian / Ubuntu (.deb)

```bash
bash packaging/linux/build-deb.sh
```

- Gói output: `build/packages/secure-vault-manager_0.1.0_amd64.deb`
- Cấu trúc cài đặt chuẩn Enterprise:
  - Cài đặt toàn bộ binaries & private shared libraries vào `/opt/secure-vault-manager/` (`bin/svm`, `bin/svm-cli`, `bin/svm-native-host`, `lib/libsvm_core.so`).
  - Tự động gắn symlink vào hệ thống: `/usr/bin/svm`, `/usr/bin/svm-cli`, `/usr/bin/svm-native-host`.
  - Tự động đăng ký Native Messaging Manifest cho Chromium, Chrome, Brave, Edge và Firefox.
- Lệnh cài đặt thử nghiệm:

  ```bash
  sudo apt install ./build/packages/secure-vault-manager_0.1.0_amd64.deb
  ```

### 2.2. Đóng gói Portable AppImage

```bash
bash packaging/linux/build-appimage.sh
```

- Tạo cấu trúc `build/packages/AppDir` và đóng gói thành file `.AppImage` (yêu cầu `appimagetool`).

### 2.3. Đóng gói Red Hat / Fedora (.rpm)

```bash
bash packaging/linux/build-rpm.sh
```

- Tạo cấu trúc rpmbuild và file `.spec` tự động tại `build/packages/rpmbuild/`.
