#!/usr/bin/env bash
# ==============================================================================
# @file scripts/build.sh
# @brief Script biên dịch toàn bộ dự án Secure Vault Manager (SVM).
# @author Hephaestus Developers <engineering@hephaestus.io>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_TYPE="Debug"
CARGO_FLAG=""
TARGET="all"

usage() {
    cat <<EOF
Sử dụng: ./scripts/build.sh [TÙY CHỌN]

Tùy chọn:
  -r, --release      Biên dịch ở chế độ tối ưu (Release mode)
  -d, --debug        Biên dịch ở chế độ kiểm thử (Debug mode - Mặc định)
  --core-only        Chỉ biên dịch Rust Core workspace (libsvm_core.so)
  --cli-only         Chỉ biên dịch công cụ dòng lệnh svm-cli
  -h, --help         Hiển thị hướng dẫn này

Ví dụ:
  ./scripts/build.sh --release
  ./scripts/build.sh --core-only
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -r|--release)
            BUILD_TYPE="Release"
            CARGO_FLAG="--release"
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            CARGO_FLAG=""
            shift
            ;;
        --core-only)
            TARGET="core"
            shift
            ;;
        --cli-only)
            TARGET="cli"
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

if [[ "${TARGET}" == "core" ]]; then
    echo "==> [Cargo] Biên dịch Rust Core package (svm-core)..."
    cargo build --manifest-path core/Cargo.toml --package svm-core ${CARGO_FLAG}
elif [[ "${TARGET}" == "cli" ]]; then
    echo "==> [Cargo] Biên dịch CLI binary (svm-cli)..."
    cargo build --manifest-path core/Cargo.toml --package svm-cli ${CARGO_FLAG}
else
    echo "==> [Cargo] Biên dịch toàn bộ Rust Workspace..."
    cargo build --manifest-path core/Cargo.toml --workspace ${CARGO_FLAG}

    if command -v cmake >/dev/null 2>&1 && command -v ninja >/dev/null 2>&1; then
        BUILD_DIR="build/$(echo "${BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"
        echo "==> [CMake] Cấu hình và kiểm tra liên kết qua CMake (${BUILD_DIR})..."
        cmake -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
        cmake --build "${BUILD_DIR}"
    fi
fi

echo "==> [SVM Build] Hoàn tất thành công!"
