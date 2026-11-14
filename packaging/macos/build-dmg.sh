#!/usr/bin/env bash
# ==============================================================================
# @file packaging/macos/build-dmg.sh
# @brief Create macOS Disk Image (.dmg) for Secure Vault Manager
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

Tùy chọn đóng gói macOS DMG:
  -r, --release      Đóng gói bản xuất xưởng Release (Mặc định)
  -d, --debug        Đóng gói bản kiểm thử Debug
  -h, --help         Hiển thị hướng dẫn này

Ví dụ:
  ./packaging/macos/build-dmg.sh            # Tạo SecureVaultManager-0.1.0.dmg (Release)
  ./packaging/macos/build-dmg.sh --debug    # Tạo SecureVaultManager-0.1.0-debug.dmg
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
    DMG_NAME="${PKG_NAME}-${PKG_VERSION}.dmg"
else
    APP_BUNDLE_NAME="Secure Vault Manager (Debug).app"
    DMG_NAME="${PKG_NAME}-${PKG_VERSION}-debug.dmg"
fi

APP_BUNDLE="${OUTPUT_DIR}/${APP_BUNDLE_NAME}"

echo "======================================================================"
echo "==> [macOS DMG] Chế độ đóng gói: [${BUILD_MODE^^}]"
echo "==> [macOS DMG] Tệp mục tiêu: ${DMG_NAME}"
echo "======================================================================"

if [[ ! -d "${APP_BUNDLE}" ]]; then
    echo "==> Chưa tìm thấy App Bundle. Đang khởi chạy build-macos.sh --${BUILD_MODE}..."
    bash "${SCRIPT_DIR}/build-macos.sh" "--${BUILD_MODE}"
fi

DMG_STAGING="${OUTPUT_DIR}/dmg-staging-${BUILD_MODE}"
rm -rf "${DMG_STAGING}"
mkdir -p "${DMG_STAGING}"

echo "==> [macOS DMG] Chuẩn bị tệp ảnh đĩa tại: ${DMG_STAGING}..."
cp -R "${APP_BUNDLE}" "${DMG_STAGING}/"
ln -sf /Applications "${DMG_STAGING}/Applications"

if command -v hdiutil >/dev/null 2>&1; then
    echo "==> [macOS DMG] Tạo file .dmg bằng hdiutil..."
    hdiutil create -volname "${PKG_NAME}" \
                   -srcfolder "${DMG_STAGING}" \
                   -ov -format UDZO \
                   "${OUTPUT_DIR}/${DMG_NAME}"
    echo "  ✓ DMG đã được tạo thành công tại: ${OUTPUT_DIR}/${DMG_NAME}"
else
    echo "==> [macOS DMG] Đã chuẩn bị sẵn staging tại: ${DMG_STAGING}"
    echo "    (Mẹo: Lệnh 'hdiutil' chỉ có sẵn trên macOS để tạo file .dmg cuối cùng)"
fi
