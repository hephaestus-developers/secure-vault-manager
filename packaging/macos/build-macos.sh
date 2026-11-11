#!/usr/bin/env bash
# ==============================================================================
# @file packaging/macos/build-macos.sh
# @brief Assemble macOS App Bundle (.app) for Secure Vault Manager
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

APP_NAME="Secure Vault Manager"
APP_BUNDLE="${ROOT_DIR}/build/packages/${APP_NAME}.app"
CONTENTS="${APP_BUNDLE}/Contents"

echo "==> [macOS Build] Khởi tạo cấu trúc .app bundle tại: ${APP_BUNDLE}"
rm -rf "${APP_BUNDLE}"
mkdir -p "${CONTENTS}/MacOS"
mkdir -p "${CONTENTS}/Frameworks"
mkdir -p "${CONTENTS}/Resources"

BUILD_DIR="${ROOT_DIR}/build/release"
if [[ ! -f "${BUILD_DIR}/ui/svm" ]]; then
    BUILD_DIR="${ROOT_DIR}/build/debug"
fi

# 1. Sao chép binary thực thi chính
if [[ -f "${BUILD_DIR}/ui/svm" ]]; then
    cp -a "${BUILD_DIR}/ui/svm" "${CONTENTS}/MacOS/svm"
fi

if [[ -f "${BUILD_DIR}/core/svm-cli" ]]; then
    cp -a "${BUILD_DIR}/core/svm-cli" "${CONTENTS}/MacOS/svm-cli"
elif [[ -f "${ROOT_DIR}/core/target/release/svm-cli" ]]; then
    cp -a "${ROOT_DIR}/core/target/release/svm-cli" "${CONTENTS}/MacOS/svm-cli"
fi

if [[ -f "${BUILD_DIR}/core/svm-native-host" ]]; then
    cp -a "${BUILD_DIR}/core/svm-native-host" "${CONTENTS}/MacOS/svm-native-host"
elif [[ -f "${ROOT_DIR}/core/target/release/svm-native-host" ]]; then
    cp -a "${ROOT_DIR}/core/target/release/svm-native-host" "${CONTENTS}/MacOS/svm-native-host"
fi

# 2. Sao chép Rust Core dylib
if [[ -f "${ROOT_DIR}/core/target/release/libsvm_core.dylib" ]]; then
    cp -a "${ROOT_DIR}/core/target/release/libsvm_core.dylib" "${CONTENTS}/Frameworks/"
elif [[ -f "${ROOT_DIR}/core/target/debug/libsvm_core.dylib" ]]; then
    cp -a "${ROOT_DIR}/core/target/debug/libsvm_core.dylib" "${CONTENTS}/Frameworks/"
fi

# 3. Metadata & Icon
cp "${ROOT_DIR}/packaging/macos/Info.plist.in" "${CONTENTS}/Info.plist"
if [[ -f "${ROOT_DIR}/packaging/macos/svm.icns" ]]; then
    cp "${ROOT_DIR}/packaging/macos/svm.icns" "${CONTENTS}/Resources/svm.icns"
fi

# 4. Chạy macdeployqt nếu đang trên môi trường macOS
if command -v macdeployqt >/dev/null 2>&1; then
    echo "==> [macOS Build] Đang đóng gói Qt Frameworks qua macdeployqt..."
    macdeployqt "${APP_BUNDLE}" -always-overwrite
fi

echo "  ✓ Hoàn tất dựng App Bundle: ${APP_BUNDLE}"
