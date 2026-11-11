#!/usr/bin/env bash
# ==============================================================================
# @file packaging/linux/build-rpm.sh
# @brief Build Red Hat / Fedora package (.rpm) for Secure Vault Manager
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

PKG_NAME="secure-vault-manager"
PKG_VERSION="0.1.0"
RPM_ROOT="${ROOT_DIR}/build/packages/rpmbuild"
OUTPUT_DIR="${ROOT_DIR}/build/packages"

echo "==> [Build RPM] Chuẩn bị môi trường rpmbuild tại: ${RPM_ROOT}"
rm -rf "${RPM_ROOT}"
mkdir -p "${RPM_ROOT}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
mkdir -p "${OUTPUT_DIR}"

SPEC_FILE="${RPM_ROOT}/SPECS/${PKG_NAME}.spec"

cat << EOF > "${SPEC_FILE}"
Name:           ${PKG_NAME}
Version:        ${PKG_VERSION}
Release:        1%{?dist}
Summary:        Enterprise-grade zero-knowledge local password and credential manager
License:        MIT
URL:            https://github.com/hephaestus-developers/secure-vault-manager
Requires:       qt6-qtbase-gui >= 6.2.0

%description
Secure Vault Manager is a high-performance, private, and secure credential vault
powered by Qt 6 and a zero-knowledge Rust Core cryptographic engine.

%install
rm -rf \$RPM_BUILD_ROOT
mkdir -p \$RPM_BUILD_ROOT/opt/secure-vault-manager/bin
mkdir -p \$RPM_BUILD_ROOT/opt/secure-vault-manager/lib
mkdir -p \$RPM_BUILD_ROOT/usr/bin
mkdir -p \$RPM_BUILD_ROOT/usr/share/applications
mkdir -p \$RPM_BUILD_ROOT/usr/share/metainfo
mkdir -p \$RPM_BUILD_ROOT/usr/share/icons/hicolor
mkdir -p \$RPM_BUILD_ROOT/etc/chromium/native-messaging-hosts
mkdir -p \$RPM_BUILD_ROOT/etc/opt/chrome/native-messaging-hosts
mkdir -p \$RPM_BUILD_ROOT/usr/lib/mozilla/native-messaging-hosts

cp -a ${ROOT_DIR}/build/debug/ui/svm \$RPM_BUILD_ROOT/opt/secure-vault-manager/bin/svm
ln -sf /opt/secure-vault-manager/bin/svm \$RPM_BUILD_ROOT/usr/bin/svm

if [ -f "${ROOT_DIR}/build/debug/core/svm-cli" ]; then
    cp -a ${ROOT_DIR}/build/debug/core/svm-cli \$RPM_BUILD_ROOT/opt/secure-vault-manager/bin/svm-cli
    ln -sf /opt/secure-vault-manager/bin/svm-cli \$RPM_BUILD_ROOT/usr/bin/svm-cli
fi

if [ -f "${ROOT_DIR}/build/debug/core/svm-native-host" ]; then
    cp -a ${ROOT_DIR}/build/debug/core/svm-native-host \$RPM_BUILD_ROOT/opt/secure-vault-manager/bin/svm-native-host
    ln -sf /opt/secure-vault-manager/bin/svm-native-host \$RPM_BUILD_ROOT/usr/bin/svm-native-host
fi

if [ -f "${ROOT_DIR}/core/target/debug/libsvm_core.so" ]; then
    cp -a ${ROOT_DIR}/core/target/debug/libsvm_core.so \$RPM_BUILD_ROOT/opt/secure-vault-manager/lib/libsvm_core.so
fi

cp ${ROOT_DIR}/packaging/linux/io.hephaestus.svm.desktop \$RPM_BUILD_ROOT/usr/share/applications/
cp ${ROOT_DIR}/packaging/linux/io.hephaestus.svm.metainfo.xml \$RPM_BUILD_ROOT/usr/share/metainfo/
cp -r ${ROOT_DIR}/packaging/linux/icons/hicolor/* \$RPM_BUILD_ROOT/usr/share/icons/hicolor/

sed 's|@SVM_NATIVE_HOST_PATH@|/opt/secure-vault-manager/bin/svm-native-host|g' \
    ${ROOT_DIR}/packaging/native-messaging/chrome.json.in \
    > \$RPM_BUILD_ROOT/etc/chromium/native-messaging-hosts/io.hephaestus.svm.json
cp \$RPM_BUILD_ROOT/etc/chromium/native-messaging-hosts/io.hephaestus.svm.json \
   \$RPM_BUILD_ROOT/etc/opt/chrome/native-messaging-hosts/io.hephaestus.svm.json

sed 's|@SVM_NATIVE_HOST_PATH@|/opt/secure-vault-manager/bin/svm-native-host|g' \
    ${ROOT_DIR}/packaging/native-messaging/firefox.json.in \
    > \$RPM_BUILD_ROOT/usr/lib/mozilla/native-messaging-hosts/io.hephaestus.svm.json

%files
/opt/secure-vault-manager/
/usr/bin/svm
/usr/bin/svm-cli
/usr/bin/svm-native-host
/usr/share/applications/io.hephaestus.svm.desktop
/usr/share/metainfo/io.hephaestus.svm.metainfo.xml
/usr/share/icons/hicolor/*
/etc/chromium/native-messaging-hosts/io.hephaestus.svm.json
/etc/opt/chrome/native-messaging-hosts/io.hephaestus.svm.json
/usr/lib/mozilla/native-messaging-hosts/io.hephaestus.svm.json
EOF

if command -v rpmbuild >/dev/null 2>&1; then
    echo "==> [Build RPM] Thực thi rpmbuild..."
    rpmbuild --define "_topdir ${RPM_ROOT}" -bb "${SPEC_FILE}"
    find "${RPM_ROOT}/RPMS" -name "*.rpm" -exec cp {} "${OUTPUT_DIR}/" \;
    echo "  ✓ Gói RPM đã được tạo thành công trong ${OUTPUT_DIR}/"
else
    echo "==> [Build RPM] Đã tạo SPEC file tại: ${SPEC_FILE}"
    echo "    (Mẹo: Cài đặt 'rpm-build' trên Fedora/CentOS/openSUSE để build trực tiếp file .rpm)"
fi
