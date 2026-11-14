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

BUILD_MODE="release"

usage() {
    cat << EOF
Sử dụng: $(basename "$0") [TÙY CHỌN]

Tùy chọn đóng gói macOS App Bundle:
  -r, --release      Đóng gói bản xuất xưởng Release (Mặc định: tối ưu dung lượng, strip symbols)
  -d, --debug        Đóng gói bản kiểm thử Debug (giữ nguyên debug symbols)
  -h, --help         Hiển thị hướng dẫn này

Ví dụ:
  ./packaging/macos/build-macos.sh            # Tạo Secure Vault Manager.app (Release)
  ./packaging/macos/build-macos.sh --debug    # Tạo Secure Vault Manager (Debug).app
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

if [[ "${BUILD_MODE}" == "release" ]]; then
    CMAKE_BUILD_TYPE="Release"
    BUILD_DIR="${ROOT_DIR}/build/release"
    APP_NAME="Secure Vault Manager"
else
    CMAKE_BUILD_TYPE="Debug"
    BUILD_DIR="${ROOT_DIR}/build/debug"
    APP_NAME="Secure Vault Manager (Debug)"
fi

APP_BUNDLE="${ROOT_DIR}/build/packages/${APP_NAME}.app"
CONTENTS="${APP_BUNDLE}/Contents"

echo "======================================================================"
echo "==> [macOS Build] Chế độ đóng gói: [${CMAKE_BUILD_TYPE^^}]"
echo "==> [macOS Build] Khởi tạo App Bundle: ${APP_BUNDLE}"
echo "======================================================================"

# Kiểm tra xem binary đã được biên dịch chưa, nếu chưa thì tự động biên dịch
if [[ ! -f "${BUILD_DIR}/ui/svm" ]]; then
    echo "==> [macOS Build] Chưa tìm thấy binaries tại: ${BUILD_DIR}"
    echo "==> [macOS Build] Đang tiến hành biên dịch [${CMAKE_BUILD_TYPE}]..."
    cmake -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" "${ROOT_DIR}"
    ninja -C "${BUILD_DIR}"
fi

rm -rf "${APP_BUNDLE}"
mkdir -p "${CONTENTS}/MacOS"
mkdir -p "${CONTENTS}/Frameworks"
mkdir -p "${CONTENTS}/Resources"

# 1. Sao chép binary thực thi chính
if [[ -f "${BUILD_DIR}/ui/svm" ]]; then
    install -m 755 "${BUILD_DIR}/ui/svm" "${CONTENTS}/MacOS/svm"
fi

if [[ -f "${BUILD_DIR}/core/svm-cli" ]]; then
    install -m 755 "${BUILD_DIR}/core/svm-cli" "${CONTENTS}/MacOS/svm-cli"
elif [[ -f "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-cli" ]]; then
    install -m 755 "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-cli" "${CONTENTS}/MacOS/svm-cli"
fi

if [[ -f "${BUILD_DIR}/core/svm-native-host" ]]; then
    install -m 755 "${BUILD_DIR}/core/svm-native-host" "${CONTENTS}/MacOS/svm-native-host"
elif [[ -f "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-native-host" ]]; then
    install -m 755 "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-native-host" "${CONTENTS}/MacOS/svm-native-host"
fi

# 2. Sao chép Rust Core dylib
if [[ -f "${ROOT_DIR}/core/target/${BUILD_MODE}/libsvm_core.dylib" ]]; then
    install -m 755 "${ROOT_DIR}/core/target/${BUILD_MODE}/libsvm_core.dylib" "${CONTENTS}/Frameworks/"
elif [[ -f "${BUILD_DIR}/core/libsvm_core.dylib" ]]; then
    install -m 755 "${BUILD_DIR}/core/libsvm_core.dylib" "${CONTENTS}/Frameworks/"
fi

# 3. Tối ưu hóa kích thước (Strip Symbols) nếu là bản RELEASE
if [[ "${BUILD_MODE}" == "release" ]]; then
    if command -v strip >/dev/null 2>&1; then
        echo "==> [macOS Build] Đang tối ưu hóa kích thước: loại bỏ debug symbols (strip)..."
        strip -u -r "${CONTENTS}/MacOS/svm" 2>/dev/null || strip -x "${CONTENTS}/MacOS/svm" 2>/dev/null || true
        if [[ -f "${CONTENTS}/MacOS/svm-cli" ]]; then
            strip -u -r "${CONTENTS}/MacOS/svm-cli" 2>/dev/null || strip -x "${CONTENTS}/MacOS/svm-cli" 2>/dev/null || true
        fi
        if [[ -f "${CONTENTS}/MacOS/svm-native-host" ]]; then
            strip -u -r "${CONTENTS}/MacOS/svm-native-host" 2>/dev/null || strip -x "${CONTENTS}/MacOS/svm-native-host" 2>/dev/null || true
        fi
        if [[ -f "${CONTENTS}/Frameworks/libsvm_core.dylib" ]]; then
            strip -x "${CONTENTS}/Frameworks/libsvm_core.dylib" 2>/dev/null || true
        fi
    fi
else
    echo "==> [macOS Build] Chế độ DEBUG: Giữ nguyên debug symbols."
fi

# 4. Đảm bảo RPATH trên macOS trỏ chuẩn vào @executable_path/../Frameworks
if command -v install_name_tool >/dev/null 2>&1; then
    install_name_tool -add_rpath "@executable_path/../Frameworks" "${CONTENTS}/MacOS/svm" 2>/dev/null || true
fi

# 4. Metadata & Icon
cp "${ROOT_DIR}/packaging/macos/Info.plist.in" "${CONTENTS}/Info.plist"
if [[ -f "${ROOT_DIR}/packaging/macos/svm.icns" ]]; then
    cp "${ROOT_DIR}/packaging/macos/svm.icns" "${CONTENTS}/Resources/svm.icns"
fi

# 5. Chạy macdeployqt nếu đang trên môi trường macOS
if command -v macdeployqt >/dev/null 2>&1; then
    echo "==> [macOS Build] Đang đóng gói Qt Frameworks qua macdeployqt..."
    macdeployqt "${APP_BUNDLE}" -always-overwrite
fi

echo "  ✓ Hoàn tất dựng App Bundle: ${APP_BUNDLE}"
