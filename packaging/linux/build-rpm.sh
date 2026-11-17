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

PKG_BASE_NAME="secure-vault-manager"
PKG_VERSION="0.1.0"
BUILD_MODE="release"

usage() {
    cat << EOF
Sử dụng: $(basename "$0") [TÙY CHỌN]

Tùy chọn đóng gói RPM:
  -r, --release      Đóng gói bản xuất xưởng Release (Mặc định: tối ưu dung lượng, strip symbols)
  -d, --debug        Đóng gói bản kiểm thử Debug (giữ nguyên debug symbols)
  -h, --help         Hiển thị hướng dẫn này

Ví dụ:
  ./packaging/linux/build-rpm.sh              # Đóng gói Native Release (dùng Qt hệ thống)
  ./packaging/linux/build-rpm.sh --debug      # Đóng gói bản Debug (.rpm)
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
        -h|--help)
            usage
            ;;
        *)
            echo "Lỗi: Tham số không hợp lệ '$1'" >&2
            usage
            ;;
    esac
done

EXTRA_SPEC_HEADERS=""
if [[ "${BUILD_MODE}" == "release" ]]; then
    CMAKE_BUILD_TYPE="Release"
    BUILD_DIR="${ROOT_DIR}/build/release"
    PKG_NAME="${PKG_BASE_NAME}"
else
    CMAKE_BUILD_TYPE="Debug"
    BUILD_DIR="${ROOT_DIR}/build/debug"
    PKG_NAME="${PKG_BASE_NAME}-debug"
    EXTRA_SPEC_HEADERS="Provides: ${PKG_BASE_NAME} = %{version}-%{release}
Conflicts: ${PKG_BASE_NAME}
%define __strip /bin/true
%global debug_package %{nil}"
fi

RPM_ROOT="${ROOT_DIR}/build/packages/rpmbuild-${BUILD_MODE}"
OUTPUT_DIR="${ROOT_DIR}/build/packages"

echo "======================================================================"
echo "==> [Build RPM] Chế độ đóng gói: [${CMAKE_BUILD_TYPE^^}]"
echo "==> [Build RPM] Gói mục tiêu: ${PKG_NAME}-${PKG_VERSION}"
echo "======================================================================"

# Kiểm tra xem binary đã được biên dịch chưa, nếu chưa thì tự động biên dịch
if [[ ! -f "${BUILD_DIR}/svm" && ! -f "${BUILD_DIR}/ui/svm" ]]; then
    echo "==> [Build RPM] Chưa tìm thấy binaries tại: ${BUILD_DIR}"
    echo "==> [Build RPM] Đang tiến hành biên dịch [${CMAKE_BUILD_TYPE}] bằng CMake presets..."
    cmake --preset "${BUILD_MODE}"
    cmake --build --preset "build-${BUILD_MODE}" -j"$(nproc 2>/dev/null || echo 4)"
fi

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
${EXTRA_SPEC_HEADERS}

%description
Secure Vault Manager is a high-performance, private, and secure credential vault
powered by Qt 6 and a zero-knowledge Rust Core cryptographic engine.

%install
rm -rf \$RPM_BUILD_ROOT
mkdir -p \$RPM_BUILD_ROOT/opt/secure-vault-manager
mkdir -p \$RPM_BUILD_ROOT/usr/bin
mkdir -p \$RPM_BUILD_ROOT/usr/share/applications
mkdir -p \$RPM_BUILD_ROOT/usr/share/metainfo
mkdir -p \$RPM_BUILD_ROOT/usr/share/icons/hicolor
mkdir -p \$RPM_BUILD_ROOT/etc/chromium/native-messaging-hosts
mkdir -p \$RPM_BUILD_ROOT/etc/opt/chrome/native-messaging-hosts
mkdir -p \$RPM_BUILD_ROOT/usr/lib/mozilla/native-messaging-hosts

cmake --install "${BUILD_DIR}" --prefix "\$RPM_BUILD_ROOT/opt/secure-vault-manager"

if [ -d "\$RPM_BUILD_ROOT/opt/secure-vault-manager/lib64" ] && [ ! -d "\$RPM_BUILD_ROOT/opt/secure-vault-manager/lib" ]; then
    mv "\$RPM_BUILD_ROOT/opt/secure-vault-manager/lib64" "\$RPM_BUILD_ROOT/opt/secure-vault-manager/lib"
fi

if [ ! -f "\$RPM_BUILD_ROOT/opt/secure-vault-manager/bin/svm-cli" ]; then
    if [ -f "${BUILD_DIR}/core/svm-cli" ]; then
        install -m 755 "${BUILD_DIR}/core/svm-cli" \$RPM_BUILD_ROOT/opt/secure-vault-manager/bin/svm-cli
    elif [ -f "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-cli" ]; then
        install -m 755 "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-cli" \$RPM_BUILD_ROOT/opt/secure-vault-manager/bin/svm-cli
    fi
fi

if [ ! -f "\$RPM_BUILD_ROOT/opt/secure-vault-manager/bin/svm-native-host" ]; then
    if [ -f "${BUILD_DIR}/core/svm-native-host" ]; then
        install -m 755 "${BUILD_DIR}/core/svm-native-host" \$RPM_BUILD_ROOT/opt/secure-vault-manager/bin/svm-native-host
    elif [ -f "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-native-host" ]; then
        install -m 755 "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-native-host" \$RPM_BUILD_ROOT/opt/secure-vault-manager/bin/svm-native-host
    fi
fi

if [ ! -f "\$RPM_BUILD_ROOT/opt/secure-vault-manager/lib/libsvm_core.so" ]; then
    mkdir -p \$RPM_BUILD_ROOT/opt/secure-vault-manager/lib
    if [ -f "${BUILD_DIR}/core/libsvm_core.so" ]; then
        install -m 755 "${BUILD_DIR}/core/libsvm_core.so" \$RPM_BUILD_ROOT/opt/secure-vault-manager/lib/libsvm_core.so
    elif [ -f "${ROOT_DIR}/core/target/${BUILD_MODE}/libsvm_core.so" ]; then
        install -m 755 "${ROOT_DIR}/core/target/${BUILD_MODE}/libsvm_core.so" \$RPM_BUILD_ROOT/opt/secure-vault-manager/lib/libsvm_core.so
    fi
fi

if [ "${BUILD_MODE}" = "release" ]; then
    strip --strip-unneeded \$RPM_BUILD_ROOT/opt/secure-vault-manager/bin/svm || true
    strip --strip-unneeded \$RPM_BUILD_ROOT/opt/secure-vault-manager/bin/svm-cli || true
    strip --strip-unneeded \$RPM_BUILD_ROOT/opt/secure-vault-manager/bin/svm-native-host || true
    strip --strip-unneeded \$RPM_BUILD_ROOT/opt/secure-vault-manager/lib/libsvm_core.so || true
fi

cp -p "${ROOT_DIR}/packaging/linux/svm" \$RPM_BUILD_ROOT/opt/secure-vault-manager/bin/svm.sh 2>/dev/null || true

ln -sf /opt/secure-vault-manager/bin/svm \$RPM_BUILD_ROOT/usr/bin/svm
ln -sf /opt/secure-vault-manager/bin/svm-cli \$RPM_BUILD_ROOT/usr/bin/svm-cli
ln -sf /opt/secure-vault-manager/bin/svm-native-host \$RPM_BUILD_ROOT/usr/bin/svm-native-host

install -m 644 "${ROOT_DIR}/packaging/linux/io.github.hephaestus.svm.desktop" \$RPM_BUILD_ROOT/usr/share/applications/
install -m 644 "${ROOT_DIR}/packaging/linux/io.github.hephaestus.svm.metainfo.xml" \$RPM_BUILD_ROOT/usr/share/metainfo/
if [ -d "${ROOT_DIR}/packaging/linux/icons/hicolor" ]; then
    cp -r "${ROOT_DIR}/packaging/linux/icons/hicolor/"* \$RPM_BUILD_ROOT/usr/share/icons/hicolor/
fi

sed 's|@SVM_NATIVE_HOST_PATH@|/opt/secure-vault-manager/bin/svm-native-host|g' \
    "${ROOT_DIR}/packaging/native-messaging/chrome.json.in" \
    > \$RPM_BUILD_ROOT/etc/chromium/native-messaging-hosts/io.hephaestus.secure_vault_manager.json
cp \$RPM_BUILD_ROOT/etc/chromium/native-messaging-hosts/io.hephaestus.secure_vault_manager.json \
   \$RPM_BUILD_ROOT/etc/opt/chrome/native-messaging-hosts/io.hephaestus.secure_vault_manager.json

sed 's|@SVM_NATIVE_HOST_PATH@|/opt/secure-vault-manager/bin/svm-native-host|g' \
    "${ROOT_DIR}/packaging/native-messaging/firefox.json.in" \
    > \$RPM_BUILD_ROOT/usr/lib/mozilla/native-messaging-hosts/io.hephaestus.secure_vault_manager.json

%files
/opt/secure-vault-manager/
/usr/bin/svm
/usr/bin/svm-cli
/usr/bin/svm-native-host
/usr/share/applications/io.github.hephaestus.svm.desktop
/usr/share/metainfo/io.github.hephaestus.svm.metainfo.xml
/usr/share/icons/hicolor/*
/etc/chromium/native-messaging-hosts/io.hephaestus.secure_vault_manager.json
/etc/opt/chrome/native-messaging-hosts/io.hephaestus.secure_vault_manager.json
/usr/lib/mozilla/native-messaging-hosts/io.hephaestus.secure_vault_manager.json
EOF

if command -v rpmbuild >/dev/null 2>&1; then
    echo "==> [Build RPM] Thực thi rpmbuild..."
    rpmbuild --define "_topdir ${RPM_ROOT}" -bb "${SPEC_FILE}"
    find "${RPM_ROOT}/RPMS" -name "*.rpm" -exec cp {} "${OUTPUT_DIR}/" \;
    echo "  ✓ Gói RPM đã được tạo thành công trong ${OUTPUT_DIR}/"
else
    echo "==> [Build RPM] rpmbuild spec file đã được tạo tại: ${SPEC_FILE}"
    echo "    (Mẹo: Cài đặt gói 'rpm' hoặc 'rpm-build' để đóng gói tệp .rpm hoàn chỉnh)"
fi
