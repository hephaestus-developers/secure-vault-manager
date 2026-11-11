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
DMG_NAME="${PKG_NAME}-${PKG_VERSION}.dmg"
OUTPUT_DIR="${ROOT_DIR}/build/packages"
APP_BUNDLE="${OUTPUT_DIR}/Secure Vault Manager.app"

if [[ ! -d "${APP_BUNDLE}" ]]; then
    echo "==> Chưa tìm thấy App Bundle. Chạy build-macos.sh trước..."
    bash "${SCRIPT_DIR}/build-macos.sh"
fi

DMG_STAGING="${OUTPUT_DIR}/dmg-staging"
rm -rf "${DMG_STAGING}"
mkdir -p "${DMG_STAGING}"

echo "==> [macOS DMG] Chuẩn bị tệp ảnh đĩa..."
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
