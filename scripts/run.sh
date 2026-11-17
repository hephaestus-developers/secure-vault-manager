#!/usr/bin/env bash
# ==============================================================================
# @file scripts/run.sh
# @brief Script khởi chạy ứng dụng Desktop hoặc công cụ CLI của SVM.
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

TARGET="app"
BUILD_MODE="debug"

usage() {
    cat <<EOF
Sử dụng: ./scripts/run.sh [TÙY CHỌN] [-- CÁC THAM SỐ TRUYỀN VÀO APP]

Tùy chọn:
  --app              Khởi chạy ứng dụng Desktop Qt6 UI (Mặc định)
  --cli              Khởi chạy công cụ dòng lệnh svm-cli
  --host             Khởi chạy tiến trình svm-native-host
  -r, --release      Chạy phiên bản Release
  -d, --debug        Chạy phiên bản Debug (Mặc định)
  -h, --help         Hiển thị hướng dẫn này

Ví dụ:
  ./scripts/run.sh
  ./scripts/run.sh --cli -- --help
  ./scripts/run.sh -r
EOF
    exit 0
}

EXTRA_ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --app)
            TARGET="app"
            shift
            ;;
        --cli)
            TARGET="cli"
            shift
            ;;
        --host)
            TARGET="host"
            shift
            ;;
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
        --)
            shift
            EXTRA_ARGS=("$@")
            break
            ;;
        *)
            EXTRA_ARGS+=("$1")
            shift
            ;;
    esac
done

cd "${ROOT_DIR}"
BUILD_DIR="build/${BUILD_MODE}"

if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "==> Chưa có build directory. Biên dịch dự án trước..."
    "${SCRIPT_DIR}/build.sh" "--${BUILD_MODE}"
fi

if [[ "${TARGET}" == "cli" ]]; then
    BIN_PATH="${BUILD_DIR}/svm-cli"
    if [[ ! -f "${BIN_PATH}" ]]; then
        cmake --build "${BUILD_DIR}" --target svm-cli
    fi
    echo "==> [SVM Run] Thực thi: ${BIN_PATH} ${EXTRA_ARGS[*]:-}"
    exec "${BIN_PATH}" ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
elif [[ "${TARGET}" == "host" ]]; then
    BIN_PATH="${BUILD_DIR}/svm-native-host"
    if [[ ! -f "${BIN_PATH}" ]]; then
        cmake --build "${BUILD_DIR}" --target svm-native-host
    fi
    echo "==> [SVM Run] Thực thi: ${BIN_PATH} ${EXTRA_ARGS[*]:-}"
    exec "${BIN_PATH}" ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
elif [[ "${TARGET}" == "app" ]]; then
    APP_PATH="${BUILD_DIR}/svm"
    if [[ ! -f "${APP_PATH}" ]]; then
        cmake --build "${BUILD_DIR}" --target svm
    fi
    echo "==> [SVM Run] Khởi chạy Desktop UI: ${APP_PATH}"
    exec "${APP_PATH}" ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
fi
