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
OUTPUT_DIR="${ROOT_DIR}/build/packages"
APP_BUNDLE="${OUTPUT_DIR}/Secure Vault Manager.app"
PKG_OUTPUT="${OUTPUT_DIR}/${PKG_NAME}-${PKG_VERSION}.pkg"

if [[ ! -d "${APP_BUNDLE}" ]]; then
    bash "${SCRIPT_DIR}/build-macos.sh"
fi

if command -v pkgbuild >/dev/null 2>&1; then
    echo "==> [macOS PKG] Đang đóng gói installer .pkg..."
    pkgbuild --install-location "/Applications/Secure Vault Manager.app" \
             --component "${APP_BUNDLE}" \
             --identifier "io.hephaestus.svm" \
             --version "${PKG_VERSION}" \
             "${PKG_OUTPUT}"
    echo "  ✓ PKG đã được tạo thành công tại: ${PKG_OUTPUT}"
else
    echo "==> [macOS PKG] Cần công cụ 'pkgbuild' (chỉ có sẵn trên macOS Xcode Command Line Tools)."
fi
