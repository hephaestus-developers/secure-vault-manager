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

echo "==> [SVM Build] Bắt đầu biên dịch dự án ở chế độ [${BUILD_TYPE}]..."

cd "${ROOT_DIR}"

BUILD_DIR="build/$(echo "${BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"

if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "==> [CMake] Cấu hình dự án tại ${BUILD_DIR}..."
    cmake -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
fi

echo "==> [CMake] Tiến hành build target [${TARGET}]..."
if [[ "${TARGET}" == "all" ]]; then
    cmake --build "${BUILD_DIR}"
else
    cmake --build "${BUILD_DIR}" --target "${TARGET}"
fi

echo "==> [SVM Build] Hoàn tất thành công!"
