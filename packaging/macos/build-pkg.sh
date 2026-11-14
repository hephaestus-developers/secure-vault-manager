#!/usr/bin/env bash
# ==============================================================================
# @file packaging/macos/build-pkg.sh
# @brief Create macOS Installer Package (.pkg) for Secure Vault Manager
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

PKG_NAME="SecureVaultManager"
PKG_VERSION="0.1.0"
BUILD_MODE="release"

usage() {
    cat << EOF
Sử dụng: $(basename "$0") [TÙY CHỌN]

Tùy chọn đóng gói macOS PKG:
  -r, --release      Đóng gói bản xuất xưởng Release (Mặc định)
  -d, --debug        Đóng gói bản kiểm thử Debug
  -h, --help         Hiển thị hướng dẫn này

Ví dụ:
  ./packaging/macos/build-pkg.sh            # Tạo SecureVaultManager-0.1.0.pkg (Release)
  ./packaging/macos/build-pkg.sh --debug    # Tạo SecureVaultManager-0.1.0-debug.pkg
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

OUTPUT_DIR="${ROOT_DIR}/build/packages"
if [[ "${BUILD_MODE}" == "release" ]]; then
    APP_BUNDLE_NAME="Secure Vault Manager.app"
    PKG_OUTPUT="${OUTPUT_DIR}/${PKG_NAME}-${PKG_VERSION}.pkg"
else
    APP_BUNDLE_NAME="Secure Vault Manager (Debug).app"
    PKG_OUTPUT="${OUTPUT_DIR}/${PKG_NAME}-${PKG_VERSION}-debug.pkg"
fi

APP_BUNDLE="${OUTPUT_DIR}/${APP_BUNDLE_NAME}"

echo "======================================================================"
echo "==> [macOS PKG] Chế độ đóng gói: [${BUILD_MODE^^}]"
echo "==> [macOS PKG] Tệp mục tiêu: ${PKG_OUTPUT}"
echo "======================================================================"

if [[ ! -d "${APP_BUNDLE}" ]]; then
    echo "==> Chưa tìm thấy App Bundle. Đang khởi chạy build-macos.sh --${BUILD_MODE}..."
    bash "${SCRIPT_DIR}/build-macos.sh" "--${BUILD_MODE}"
fi

if command -v pkgbuild >/dev/null 2>&1; then
    echo "==> [macOS PKG] Đang đóng gói installer .pkg..."
    pkgbuild --install-location "/Applications/${APP_BUNDLE_NAME}" \
             --component "${APP_BUNDLE}" \
             --identifier "io.hephaestus.svm" \
             --version "${PKG_VERSION}" \
             "${PKG_OUTPUT}"
    echo "  ✓ PKG đã được tạo thành công tại: ${PKG_OUTPUT}"
else
    echo "==> [macOS PKG] Cần công cụ 'pkgbuild' (chỉ có sẵn trên macOS Xcode Command Line Tools)."
fi
