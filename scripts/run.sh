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

TARGET="cli"
BUILD_MODE="debug"

usage() {
    cat <<EOF
Sử dụng: ./scripts/run.sh [TÙY CHỌN] [-- CÁC THAM SỐ TRUYỀN VÀO APP]

Tùy chọn:
  --cli              Khởi chạy công cụ dòng lệnh svm-cli (Mặc định)
  --host             Khởi chạy tiến trình svm-native-host
  --app              Khởi chạy ứng dụng Desktop Qt6 (khi đã biên dịch UI)
  -r, --release      Chạy phiên bản Release
  -d, --debug        Chạy phiên bản Debug (Mặc định)
  -h, --help         Hiển thị hướng dẫn này

Ví dụ:
  ./scripts/run.sh --cli
  ./scripts/run.sh --cli -- --help
EOF
    exit 0
}

EXTRA_ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --cli)
            TARGET="cli"
            shift
            ;;
        --host)
            TARGET="host"
            shift
            ;;
        --app)
            TARGET="app"
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

if [[ "${TARGET}" == "cli" ]]; then
    BIN_PATH="core/target/${BUILD_MODE}/svm-cli"
    if [[ ! -f "${BIN_PATH}" ]]; then
        echo "==> Binary chưa được biên dịch. Tiến hành biên dịch svm-cli..."
        CARGO_FLAG=""
        [[ "${BUILD_MODE}" == "release" ]] && CARGO_FLAG="--release"
        cargo build --manifest-path core/Cargo.toml --package svm-cli ${CARGO_FLAG}
    fi
    echo "==> [SVM Run] Thực thi: ${BIN_PATH} ${EXTRA_ARGS[*]:-}"
    exec "${BIN_PATH}" ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
elif [[ "${TARGET}" == "host" ]]; then
    BIN_PATH="core/target/${BUILD_MODE}/svm-native-host"
    if [[ ! -f "${BIN_PATH}" ]]; then
        echo "==> Binary chưa được biên dịch. Tiến hành biên dịch svm-native-host..."
        CARGO_FLAG=""
        [[ "${BUILD_MODE}" == "release" ]] && CARGO_FLAG="--release"
        cargo build --manifest-path core/Cargo.toml --package svm-native-host ${CARGO_FLAG}
    fi
    echo "==> [SVM Run] Thực thi: ${BIN_PATH} ${EXTRA_ARGS[*]:-}"
    exec "${BIN_PATH}" ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
elif [[ "${TARGET}" == "app" ]]; then
    APP_PATH="build/${BUILD_MODE}/ui/svm_ui"
    if [[ ! -f "${APP_PATH}" ]]; then
        echo "Lỗi: Ứng dụng Desktop UI chưa được biên dịch tại '${APP_PATH}'." >&2
        echo "Tầng UI Qt6 sẽ được liên kết khi phát triển phân hệ ui/." >&2
        exit 1
    fi
    echo "==> [SVM Run] Khởi chạy Desktop UI: ${APP_PATH}"
    exec "${APP_PATH}" ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
fi
