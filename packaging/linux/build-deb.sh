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

PKG_NAME="secure-vault-manager"
PKG_VERSION="0.1.0"
PKG_ARCH="amd64"
DEB_NAME="${PKG_NAME}_${PKG_VERSION}_${PKG_ARCH}.deb"

BUILD_DIR="${ROOT_DIR}/build/release"
if [[ ! -f "${BUILD_DIR}/ui/svm" ]]; then
    BUILD_DIR="${ROOT_DIR}/build/debug"
fi

if [[ ! -f "${BUILD_DIR}/ui/svm" ]]; then
    echo "==> [Build DEB] Chưa tìm thấy binary build/release/ui/svm hoặc build/debug/ui/svm."
    echo "==> Tiến hành biên dịch release qua ninja..."
    cmake -B "${ROOT_DIR}/build/release" -G Ninja -DCMAKE_BUILD_TYPE=Release "${ROOT_DIR}"
    ninja -C "${ROOT_DIR}/build/release"
    BUILD_DIR="${ROOT_DIR}/build/release"
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

# 1. Cài đặt các binary vào /opt/secure-vault-manager/bin/
echo "==> [Build DEB] Sao chép binaries vào /opt/secure-vault-manager/bin/..."
install -m 755 "${BUILD_DIR}/ui/svm" "${OPT_DIR}/bin/svm"

if [[ -f "${BUILD_DIR}/core/svm-cli" ]]; then
    install -m 755 "${BUILD_DIR}/core/svm-cli" "${OPT_DIR}/bin/svm-cli"
elif [[ -f "${ROOT_DIR}/core/target/release/svm-cli" ]]; then
    install -m 755 "${ROOT_DIR}/core/target/release/svm-cli" "${OPT_DIR}/bin/svm-cli"
elif [[ -f "${ROOT_DIR}/core/target/debug/svm-cli" ]]; then
    install -m 755 "${ROOT_DIR}/core/target/debug/svm-cli" "${OPT_DIR}/bin/svm-cli"
fi

if [[ -f "${BUILD_DIR}/core/svm-native-host" ]]; then
    install -m 755 "${BUILD_DIR}/core/svm-native-host" "${OPT_DIR}/bin/svm-native-host"
elif [[ -f "${ROOT_DIR}/core/target/release/svm-native-host" ]]; then
    install -m 755 "${ROOT_DIR}/core/target/release/svm-native-host" "${OPT_DIR}/bin/svm-native-host"
elif [[ -f "${ROOT_DIR}/core/target/debug/svm-native-host" ]]; then
    install -m 755 "${ROOT_DIR}/core/target/debug/svm-native-host" "${OPT_DIR}/bin/svm-native-host"
fi

# 2. Cài đặt thư viện C-ABI vào /opt/secure-vault-manager/lib/
echo "==> [Build DEB] Sao chép libsvm_core.so vào /opt/secure-vault-manager/lib/..."
if [[ -f "${BUILD_DIR}/core/libsvm_core.so" ]]; then
    install -m 755 "${BUILD_DIR}/core/libsvm_core.so" "${OPT_DIR}/lib/libsvm_core.so"
elif [[ -f "${ROOT_DIR}/core/target/release/libsvm_core.so" ]]; then
    install -m 755 "${ROOT_DIR}/core/target/release/libsvm_core.so" "${OPT_DIR}/lib/libsvm_core.so"
elif [[ -f "${ROOT_DIR}/core/target/debug/libsvm_core.so" ]]; then
    install -m 755 "${ROOT_DIR}/core/target/debug/libsvm_core.so" "${OPT_DIR}/lib/libsvm_core.so"
fi

# 3. Tạo symlinks trong /usr/bin/ trỏ vào /opt/secure-vault-manager/bin/
echo "==> [Build DEB] Tạo symlinks trong /usr/bin/..."
ln -sf "/opt/secure-vault-manager/bin/svm" "${STAGING_DIR}/usr/bin/svm"
ln -sf "/opt/secure-vault-manager/bin/svm-cli" "${STAGING_DIR}/usr/bin/svm-cli"
ln -sf "/opt/secure-vault-manager/bin/svm-native-host" "${STAGING_DIR}/usr/bin/svm-native-host"

# 4. Cài đặt Desktop Entry & AppStream Metadata
install -m 644 "${ROOT_DIR}/packaging/linux/io.hephaestus.svm.desktop" "${STAGING_DIR}/usr/share/applications/io.hephaestus.svm.desktop"
install -m 644 "${ROOT_DIR}/packaging/linux/io.hephaestus.svm.metainfo.xml" "${STAGING_DIR}/usr/share/metainfo/io.hephaestus.svm.metainfo.xml"

# 5. Cài đặt bộ Icons hicolor
echo "==> [Build DEB] Cài đặt hicolor icons..."
if [[ ! -d "${ROOT_DIR}/packaging/linux/icons/hicolor" ]]; then
    "${ROOT_DIR}/scripts/generate-icons.py"
fi
mkdir -p "${STAGING_DIR}/usr/share/icons/hicolor"
cp -r "${ROOT_DIR}/packaging/linux/icons/hicolor/"* "${STAGING_DIR}/usr/share/icons/hicolor/"

# 6. Cài đặt Native Messaging Manifests cấp hệ thống trỏ vào /opt/secure-vault-manager/bin/
sed 's|@SVM_NATIVE_HOST_PATH@|/opt/secure-vault-manager/bin/svm-native-host|g' \
    "${ROOT_DIR}/packaging/native-messaging/chrome.json.in" \
    > "${STAGING_DIR}/etc/chromium/native-messaging-hosts/io.hephaestus.svm.json"
cp "${STAGING_DIR}/etc/chromium/native-messaging-hosts/io.hephaestus.svm.json" \
   "${STAGING_DIR}/etc/opt/chrome/native-messaging-hosts/io.hephaestus.svm.json"

sed 's|@SVM_NATIVE_HOST_PATH@|/opt/secure-vault-manager/bin/svm-native-host|g' \
    "${ROOT_DIR}/packaging/native-messaging/firefox.json.in" \
    > "${STAGING_DIR}/usr/lib/mozilla/native-messaging-hosts/io.hephaestus.svm.json"

# 7. Cài đặt Copyright & License documentation
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

# 7. Tạo DEBIAN/control
INSTALLED_SIZE=$(du -sk "${STAGING_DIR}" | cut -f1)

cat << EOF > "${STAGING_DIR}/DEBIAN/control"
Package: ${PKG_NAME}
Version: ${PKG_VERSION}
Section: utils
Priority: optional
Architecture: ${PKG_ARCH}
Installed-Size: ${INSTALLED_SIZE}
Maintainer: Hai Pham Ngoc <ngochai285nd@gmail.com>
Depends: libc6 (>= 2.31), libqt6core6 (>= 6.2.0), libqt6gui6 (>= 6.2.0), libqt6widgets6 (>= 6.2.0), libqt6network6 (>= 6.2.0)
Recommends: pinentry-curses | pinentry-gnome3 | pinentry-qt
Homepage: https://github.com/hephaestus-developers/secure-vault-manager
Description: Enterprise-grade zero-knowledge local password and credential manager
 Secure Vault Manager (SVM) is a high-performance offline password and secret
 vault. It features end-to-end encryption using ChaCha20-Poly1305 and Argon2id,
 WebAuthn passkey management, TOTP authenticator tokens, and SSH agent socket
 integration.
EOF

# 8. Chuẩn hóa phân quyền và thực hiện build gói .deb
echo "==> [Build DEB] Đang đóng gói .deb bằng dpkg-deb..."
find "${STAGING_DIR}" -type d -exec chmod 755 {} +
find "${STAGING_DIR}/opt/secure-vault-manager/bin" -type f -exec chmod 755 {} +
find "${STAGING_DIR}/opt/secure-vault-manager/lib" -type f -name "*.so*" -exec chmod 755 {} +
find "${STAGING_DIR}/usr/share" -type f -exec chmod 644 {} +
find "${STAGING_DIR}/etc" -type f -exec chmod 644 {} +
chmod 755 "${STAGING_DIR}/DEBIAN"
chmod 644 "${STAGING_DIR}/DEBIAN/control"
dpkg-deb --build --root-owner-group "${STAGING_DIR}" "${OUTPUT_DIR}/${DEB_NAME}"

echo "======================================================================"
echo "  ✓ Gói Debian đã được tạo thành công:"
echo "    ${OUTPUT_DIR}/${DEB_NAME}"
echo "======================================================================"
