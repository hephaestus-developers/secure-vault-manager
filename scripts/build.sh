#!/usr/bin/env bash
# ==============================================================================
# @file scripts/build.sh
# @brief Script biên dịch toàn bộ dự án Secure Vault Manager (SVM).
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_TYPE="Debug"
TARGET="all"

usage() {
    cat <<EOF
Sử dụng: ./scripts/build.sh [TÙY CHỌN]

Tùy chọn:
  -r, --release      Biên dịch ở chế độ tối ưu (Release mode)
  -d, --debug        Biên dịch ở chế độ kiểm thử (Debug mode - Mặc định)
  --app-only         Chỉ biên dịch ứng dụng Desktop UI (svm)
  --core-only        Chỉ biên dịch Rust Core workspace (svm_core)
  --cli-only         Chỉ biên dịch công cụ dòng lệnh (svm-cli)
  --host-only        Chỉ biên dịch Native Messaging Host (svm-native-host)
  -h, --help         Hiển thị hướng dẫn này

Ví dụ:
  ./scripts/build.sh --release
  ./scripts/build.sh --app-only
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --app-only)
            TARGET="svm"
            shift
            ;;
        --core-only)
            TARGET="svm_core"
            shift
            ;;
        --cli-only)
            TARGET="svm-cli"
            shift
            ;;
        --host-only)
            TARGET="svm-native-host"
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

PRESET_NAME="$(echo "${BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"
BUILD_DIR="build/${PRESET_NAME}"

echo "==> [SVM Build] Bắt đầu biên dịch dự án ở chế độ [${BUILD_TYPE}]..."

cd "${ROOT_DIR}"

# 1. Cấu hình dự án bằng CMake Presets
if [[ ! -f "${BUILD_DIR}/build.ninja" ]]; then
    echo "==> [CMake] Cấu hình dự án bằng preset [${PRESET_NAME}]..."
    cmake --preset "${PRESET_NAME}"
fi

# 2. Biên dịch bằng CMake Presets
echo "==> [CMake] Tiến hành build target [${TARGET}] với $(nproc 2>/dev/null || echo 4) luồng..."
if [[ "${TARGET}" == "all" ]]; then
    cmake --build --preset "build-${PRESET_NAME}" -j"$(nproc 2>/dev/null || echo 4)"
else
    cmake --build "${BUILD_DIR}" --target "${TARGET}" -j"$(nproc 2>/dev/null || echo 4)"
fi

# 3. Tự động cập nhật symlink compile_commands.json cho IDE/clangd
if [[ -f "${BUILD_DIR}/compile_commands.json" ]]; then
    ln -sf "${BUILD_DIR}/compile_commands.json" "${ROOT_DIR}/compile_commands.json"
fi

echo "==> [SVM Build] Hoàn tất thành công! Binaries nằm tại: ${BUILD_DIR}/"
