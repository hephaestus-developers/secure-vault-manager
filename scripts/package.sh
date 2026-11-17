#!/usr/bin/env bash
# ==============================================================================
# @file scripts/package.sh
# @brief Script đóng gói ứng dụng Secure Vault Manager phân phối cho Linux.
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

VERSION="0.1.0"
DIST_DIR="${ROOT_DIR}/dist"
PKG_NAME="secure-vault-manager-${VERSION}-linux-x86_64"
STAGE_DIR="${DIST_DIR}/${PKG_NAME}"

echo "==> [SVM Package] Bắt đầu đóng gói ${PKG_NAME}..."

cd "${ROOT_DIR}"

# 1. Biên dịch Release mode
"${SCRIPT_DIR}/build.sh" --release

BUILD_DIR="${ROOT_DIR}/build/release"

# 2. Tạo cấu trúc thư mục phân phối
rm -rf "${STAGE_DIR}" "${STAGE_DIR}.tar.gz"
mkdir -p "${STAGE_DIR}/bin" "${STAGE_DIR}/lib" "${STAGE_DIR}/share/applications" "${STAGE_DIR}/share/icons/hicolor/scalable/apps"

# 3. Sao chép binaries và thư viện
cp -f "${BUILD_DIR}/svm" "${STAGE_DIR}/bin/"
cp -f "${BUILD_DIR}/svm-cli" "${STAGE_DIR}/bin/"
cp -f "${BUILD_DIR}/svm-native-host" "${STAGE_DIR}/bin/"
cp -f "${BUILD_DIR}/libsvm_core.so" "${STAGE_DIR}/lib/"

# 4. Sao chép tài nguyên Desktop & Icon
if [[ -f "${ROOT_DIR}/packaging/linux/io.hephaestus.secure-vault-manager.desktop" ]]; then
    cp -f "${ROOT_DIR}/packaging/linux/io.hephaestus.secure-vault-manager.desktop" "${STAGE_DIR}/share/applications/"
fi

if [[ -f "${ROOT_DIR}/src/resources/icons/brand/app_logo.svg" ]]; then
    cp -f "${ROOT_DIR}/src/resources/icons/brand/app_logo.svg" "${STAGE_DIR}/share/icons/hicolor/scalable/apps/io.hephaestus.secure-vault-manager.svg"
fi

# 5. Đóng gói tar.gz
cd "${DIST_DIR}"
tar -czf "${PKG_NAME}.tar.gz" "${PKG_NAME}"
echo "==> [SVM Package] Đã tạo gói phân phối: ${DIST_DIR}/${PKG_NAME}.tar.gz"

echo "==> [SVM Package] Hoàn tất đóng gói thành công!"
