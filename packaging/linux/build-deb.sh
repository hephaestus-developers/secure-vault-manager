#!/usr/bin/env bash
# ==============================================================================
# @file packaging/linux/build-deb.sh
# @brief Build Debian / Ubuntu package (.deb) for Secure Vault Manager
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

PKG_BASE_NAME="secure-vault-manager"
PKG_VERSION="0.1.0"
PKG_ARCH="amd64"
BUILD_MODE="release"
BUNDLE_QT=true

usage() {
    cat << EOF
Sử dụng: $(basename "$0") [TÙY CHỌN]

Tùy chọn đóng gói:
  -r, --release      Đóng gói bản xuất xưởng Release (Mặc định: tối ưu kích thước, tự động strip symbols)
  -d, --debug        Đóng gói bản kiểm thử Debug (giữ nguyên debug symbols cho gdb/valgrind)
  --bundle-qt        Nhúng sẵn thư viện Qt6, ICU và Plugins (Mặc định: độc lập, chạy mọi bản phân phối Linux)
  --system-qt        Không nhúng Qt6, sử dụng Qt6 của hệ điều hành (gói .deb siêu nhẹ ~3MB)
  -h, --help         Hiển thị hướng dẫn này

Ví dụ:
  ./packaging/linux/build-deb.sh              # Đóng gói Standalone Release (nhúng Qt, tương thích mọi distro)
  ./packaging/linux/build-deb.sh --system-qt  # Đóng gói Release siêu nhẹ sử dụng Qt hệ thống
  ./packaging/linux/build-deb.sh --debug      # Đóng gói bản Debug
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -r|--release)
            BUILD_MODE="release"
            shift
            ;;
        -d|--debug)
            BUILD_MODE="debug"
            shift
            ;;
        --bundle-qt)
            BUNDLE_QT=true
            shift
            ;;
        --system-qt|--no-bundle-qt)
            BUNDLE_QT=false
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Lỗi: Tham số không hợp lệ '$1'" >&2
            usage
            ;;
    esac
done

if [[ "${BUILD_MODE}" == "release" ]]; then
    CMAKE_BUILD_TYPE="Release"
    BUILD_DIR="${ROOT_DIR}/build/release"
    PKG_NAME="${PKG_BASE_NAME}"
    DEB_NAME="${PKG_NAME}_${PKG_VERSION}_${PKG_ARCH}.deb"
    PKG_DESC_SUFFIX=""
else
    CMAKE_BUILD_TYPE="Debug"
    BUILD_DIR="${ROOT_DIR}/build/debug"
    PKG_NAME="${PKG_BASE_NAME}-debug"
    DEB_NAME="${PKG_NAME}_${PKG_VERSION}_${PKG_ARCH}.deb"
    PKG_DESC_SUFFIX=" (Debug Build with DWARF Symbols)"
fi

echo "======================================================================"
echo "==> [Build DEB] Chế độ đóng gói: [${CMAKE_BUILD_TYPE^^}]"
echo "==> [Build DEB] Tên gói: ${DEB_NAME}"
echo "======================================================================"

# Kiểm tra xem binary đã được biên dịch chưa, nếu chưa thì tự động biên dịch
if [[ ! -f "${BUILD_DIR}/ui/svm" || ! -f "${BUILD_DIR}/core/libsvm_core.so" ]]; then
    echo "==> [Build DEB] Chưa tìm thấy binaries tại: ${BUILD_DIR}"
    echo "==> [Build DEB] Đang tiến hành biên dịch [${CMAKE_BUILD_TYPE}]..."
    cmake -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" "${ROOT_DIR}"
    ninja -C "${BUILD_DIR}"
fi

STAGING_DIR="${ROOT_DIR}/build/packages/deb-staging"
OUTPUT_DIR="${ROOT_DIR}/build/packages"
OPT_DIR="${STAGING_DIR}/opt/secure-vault-manager"

echo "==> [Build DEB] Chuẩn bị staging directory tại: ${STAGING_DIR}"
rm -rf "${STAGING_DIR}"
mkdir -p "${STAGING_DIR}/DEBIAN"
mkdir -p "${OPT_DIR}/bin"
mkdir -p "${OPT_DIR}/lib"
mkdir -p "${STAGING_DIR}/usr/bin"
mkdir -p "${STAGING_DIR}/usr/share/applications"
mkdir -p "${STAGING_DIR}/usr/share/metainfo"
mkdir -p "${STAGING_DIR}/usr/share/doc/${PKG_NAME}"
mkdir -p "${STAGING_DIR}/etc/chromium/native-messaging-hosts"
mkdir -p "${STAGING_DIR}/etc/opt/chrome/native-messaging-hosts"
mkdir -p "${STAGING_DIR}/usr/lib/mozilla/native-messaging-hosts"
mkdir -p "${OUTPUT_DIR}"

# 1. Cài đặt các binary và thư viện qua CMake install để tự động cập nhật INSTALL_RPATH ($ORIGIN/../lib)
echo "==> [Build DEB] Cài đặt binaries và libraries qua CMake install vào ${OPT_DIR}..."
cmake --install "${BUILD_DIR}" --prefix "${OPT_DIR}"

# Đảm bảo các thư mục tiêu chuẩn luôn hiện diện
if [[ -d "${OPT_DIR}/lib64" && ! -d "${OPT_DIR}/lib" ]]; then
    mv "${OPT_DIR}/lib64" "${OPT_DIR}/lib"
fi

# Fallback bổ sung nếu cmake install bỏ sót file do cấu hình tùy biến
if [[ ! -f "${OPT_DIR}/bin/svm-cli" ]]; then
    if [[ -f "${BUILD_DIR}/core/svm-cli" ]]; then
        install -m 755 "${BUILD_DIR}/core/svm-cli" "${OPT_DIR}/bin/svm-cli"
    elif [[ -f "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-cli" ]]; then
        install -m 755 "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-cli" "${OPT_DIR}/bin/svm-cli"
    fi
fi

if [[ ! -f "${OPT_DIR}/bin/svm-native-host" ]]; then
    if [[ -f "${BUILD_DIR}/core/svm-native-host" ]]; then
        install -m 755 "${BUILD_DIR}/core/svm-native-host" "${OPT_DIR}/bin/svm-native-host"
    elif [[ -f "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-native-host" ]]; then
        install -m 755 "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-native-host" "${OPT_DIR}/bin/svm-native-host"
    fi
fi

if [[ ! -f "${OPT_DIR}/lib/libsvm_core.so" ]]; then
    mkdir -p "${OPT_DIR}/lib"
    if [[ -f "${BUILD_DIR}/core/libsvm_core.so" ]]; then
        install -m 755 "${BUILD_DIR}/core/libsvm_core.so" "${OPT_DIR}/lib/libsvm_core.so"
    elif [[ -f "${ROOT_DIR}/core/target/${BUILD_MODE}/libsvm_core.so" ]]; then
        install -m 755 "${ROOT_DIR}/core/target/${BUILD_MODE}/libsvm_core.so" "${OPT_DIR}/lib/libsvm_core.so"
    fi
fi

# 2. Tùy chọn nhúng Qt6 & Plugins (Standalone)
if [[ "${BUNDLE_QT}" == "true" ]]; then
    STRIP_QT="false"
    if [[ "${BUILD_MODE}" == "release" ]]; then
        STRIP_QT="true"
    fi
    "${ROOT_DIR}/packaging/linux/bundle-qt.sh" "${OPT_DIR}" "${STRIP_QT}"
fi

# 3. Tối ưu hóa kích thước (Strip Symbols) nếu là bản RELEASE
if [[ "${BUILD_MODE}" == "release" ]]; then
    echo "==> [Build DEB] Đang tối ưu hóa kích thước: loại bỏ debug symbols (strip)..."
    strip --strip-unneeded "${OPT_DIR}/bin/svm" 2>/dev/null || true
    strip --strip-unneeded "${OPT_DIR}/bin/svm-cli" 2>/dev/null || true
    strip --strip-unneeded "${OPT_DIR}/bin/svm-native-host" 2>/dev/null || true
    strip --strip-unneeded "${OPT_DIR}/lib/libsvm_core.so" 2>/dev/null || true
else
    echo "==> [Build DEB] Chế độ DEBUG: Giữ nguyên debug symbols cho gdb/valgrind."
fi

# 4. Tạo Launcher Wrapper để đảm bảo nạp đúng LD_LIBRARY_PATH khi nhúng Qt
if [[ "${BUNDLE_QT}" == "true" ]]; then
    echo "==> [Build DEB] Khởi tạo Launcher Wrapper /opt/secure-vault-manager/bin/svm..."
    mv "${OPT_DIR}/bin/svm" "${OPT_DIR}/bin/svm-bin"
    cat << 'EOF' > "${OPT_DIR}/bin/svm"
#!/usr/bin/env bash
# Secure Vault Manager Launcher Wrapper
# Tự động nạp toàn bộ thư viện Qt6, ICU và Core C-ABI nội bộ
set -e

SELF="$(readlink -f "$0" 2>/dev/null || realpath "$0" 2>/dev/null || echo "$0")"
BIN_DIR="$(dirname "$SELF")"
OPT_DIR="$(dirname "$BIN_DIR")"
LIB_DIR="${OPT_DIR}/lib"
PLUGINS_DIR="${OPT_DIR}/plugins"

if [[ -d "${LIB_DIR}" ]]; then
    export LD_LIBRARY_PATH="${LIB_DIR}:${LD_LIBRARY_PATH:-}"
fi

if [[ -d "${PLUGINS_DIR}" ]]; then
    export QT_PLUGIN_PATH="${PLUGINS_DIR}:${QT_PLUGIN_PATH:-}"
fi

exec "${BIN_DIR}/svm-bin" "$@"
EOF
    chmod 755 "${OPT_DIR}/bin/svm"
    chmod 755 "${OPT_DIR}/bin/svm-bin"
fi

# 4. Tạo symlinks trong /usr/bin/ trỏ vào /opt/secure-vault-manager/bin/
echo "==> [Build DEB] Tạo symlinks trong /usr/bin/..."
ln -sf "/opt/secure-vault-manager/bin/svm" "${STAGING_DIR}/usr/bin/svm"
ln -sf "/opt/secure-vault-manager/bin/svm-cli" "${STAGING_DIR}/usr/bin/svm-cli"
ln -sf "/opt/secure-vault-manager/bin/svm-native-host" "${STAGING_DIR}/usr/bin/svm-native-host"

# 5. Cài đặt Desktop Entry & AppStream Metadata
install -m 644 "${ROOT_DIR}/packaging/linux/io.hephaestus.svm.desktop" "${STAGING_DIR}/usr/share/applications/io.hephaestus.svm.desktop"
install -m 644 "${ROOT_DIR}/packaging/linux/io.hephaestus.svm.metainfo.xml" "${STAGING_DIR}/usr/share/metainfo/io.hephaestus.svm.metainfo.xml"

# 6. Cài đặt bộ Icons hicolor
echo "==> [Build DEB] Cài đặt hicolor icons..."
if [[ -d "${ROOT_DIR}/packaging/linux/icons/hicolor" ]]; then
    mkdir -p "${STAGING_DIR}/usr/share/icons/hicolor"
    cp -r "${ROOT_DIR}/packaging/linux/icons/hicolor/"* "${STAGING_DIR}/usr/share/icons/hicolor/"
fi

# 7. Cài đặt Native Messaging Manifests cấp hệ thống
sed 's|@SVM_NATIVE_HOST_PATH@|/opt/secure-vault-manager/bin/svm-native-host|g' \
    "${ROOT_DIR}/packaging/native-messaging/chrome.json.in" \
    > "${STAGING_DIR}/etc/chromium/native-messaging-hosts/io.hephaestus.svm.json"
cp "${STAGING_DIR}/etc/chromium/native-messaging-hosts/io.hephaestus.svm.json" \
   "${STAGING_DIR}/etc/opt/chrome/native-messaging-hosts/io.hephaestus.svm.json"

sed 's|@SVM_NATIVE_HOST_PATH@|/opt/secure-vault-manager/bin/svm-native-host|g' \
    "${ROOT_DIR}/packaging/native-messaging/firefox.json.in" \
    > "${STAGING_DIR}/usr/lib/mozilla/native-messaging-hosts/io.hephaestus.svm.json"

# 8. Cài đặt Copyright & License documentation
cat << 'EOF' > "${STAGING_DIR}/usr/share/doc/${PKG_NAME}/copyright"
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: Secure Vault Manager
Upstream-Contact: Hai Pham Ngoc <ngochai285nd@gmail.com>
Source: https://github.com/hephaestus-developers/secure-vault-manager

Files: *
Copyright: 2026 Hephaestus Developers
License: MIT

Files: debian/*
Copyright: 2026 Hephaestus Developers
License: MIT

License: MIT
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 .
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
EOF

# 9. Tạo DEBIAN/control
INSTALLED_SIZE=$(du -sk "${STAGING_DIR}" | cut -f1)

CONFLICTS_LINE=""
PROVIDES_LINE=""
if [[ "${BUILD_MODE}" == "debug" ]]; then
    CONFLICTS_LINE="Conflicts: secure-vault-manager"
    PROVIDES_LINE="Provides: secure-vault-manager"
fi

DEPENDS_LIST="libc6 (>= 2.31)"
if [[ "${BUNDLE_QT}" == "false" ]]; then
    DEPENDS_LIST="libc6 (>= 2.31), libqt6core6 (>= 6.2.0), libqt6gui6 (>= 6.2.0), libqt6widgets6 (>= 6.2.0), libqt6network6 (>= 6.2.0)"
fi

cat << EOF > "${STAGING_DIR}/DEBIAN/control"
Package: ${PKG_NAME}
Version: ${PKG_VERSION}
Section: utils
Priority: optional
Architecture: ${PKG_ARCH}
Installed-Size: ${INSTALLED_SIZE}
Maintainer: Hai Pham Ngoc <ngochai285nd@gmail.com>
Depends: ${DEPENDS_LIST}
Recommends: pinentry-curses | pinentry-gnome3 | pinentry-qt
Homepage: https://github.com/hephaestus-developers/secure-vault-manager
Description: Enterprise-grade zero-knowledge local password and credential manager${PKG_DESC_SUFFIX}
 Secure Vault Manager (SVM) is a high-performance offline password and secret
 vault. It features end-to-end encryption using ChaCha20-Poly1305 and Argon2id,
 WebAuthn passkey management, TOTP authenticator tokens, and SSH agent socket
 integration.
EOF

if [[ -n "${CONFLICTS_LINE}" ]]; then
    echo "${CONFLICTS_LINE}" >> "${STAGING_DIR}/DEBIAN/control"
fi
if [[ -n "${PROVIDES_LINE}" ]]; then
    echo "${PROVIDES_LINE}" >> "${STAGING_DIR}/DEBIAN/control"
fi

# 10. Chuẩn hóa phân quyền và thực hiện build gói .deb
echo "==> [Build DEB] Đang đóng gói .deb bằng dpkg-deb..."
find "${STAGING_DIR}" -type d -exec chmod 755 {} +
find "${STAGING_DIR}/opt/secure-vault-manager/bin" -type f -exec chmod 755 {} +
find "${STAGING_DIR}/opt/secure-vault-manager/lib" -type f -name "*.so*" -exec chmod 755 {} +
find "${STAGING_DIR}/usr/share" -type f -exec chmod 644 {} +
find "${STAGING_DIR}/etc" -type f -exec chmod 644 {} +
chmod 755 "${STAGING_DIR}/DEBIAN"
chmod 644 "${STAGING_DIR}/DEBIAN/control"

dpkg-deb --build --root-owner-group "${STAGING_DIR}" "${OUTPUT_DIR}/${DEB_NAME}"

DEB_SIZE=$(ls -lh "${OUTPUT_DIR}/${DEB_NAME}" | awk '{print $5}')
LIB_SIZE=$(ls -lh "${OPT_DIR}/lib/libsvm_core.so" | awk '{print $5}')
UI_SIZE=$(ls -lh "${OPT_DIR}/bin/svm" | awk '{print $5}')

echo "======================================================================"
echo "  ✓ Gói Debian đã được tạo thành công:"
echo "    - File: ${OUTPUT_DIR}/${DEB_NAME} (Dung lượng: ${DEB_SIZE})"
echo "    - Thư viện libsvm_core.so: ${LIB_SIZE}"
echo "    - Giao diện svm (UI): ${UI_SIZE}"
echo "======================================================================"
