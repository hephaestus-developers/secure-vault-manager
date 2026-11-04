#!/usr/bin/env bash
# ==============================================================================
# @file scripts/gen-header.sh
# @brief Script sinh và kiểm tra tệp C-ABI header (svm_core.h) bằng cbindgen.
# @author Hephaestus Developers <engineering@hephaestus.io>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

CHECK_MODE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --check)
            CHECK_MODE=true
            shift
            ;;
        *)
            echo "Tham số không hợp lệ: $1" >&2
            exit 1
            ;;
    esac
done

cd "${ROOT_DIR}/core"

HEADER_PATH="include/svm_core.h"

if ! command -v cbindgen >/dev/null 2>&1; then
    echo "Thông báo: cbindgen chưa được cài đặt (cài bằng: cargo install cbindgen)."
    echo "Tệp header C-ABI hiện tại '${HEADER_PATH}' được duy trì chuẩn hóa thủ công."
    exit 0
fi

if [[ "${CHECK_MODE}" == true ]]; then
    echo "==> [SVM Header] Kiểm tra độ lệch (drift) của tệp header svm_core.h..."
    TMP_HEADER=$(mktemp)
    cbindgen --config cbindgen.toml --crate svm-core --output "${TMP_HEADER}"
    if ! diff -u "${HEADER_PATH}" "${TMP_HEADER}"; then
        echo "Lỗi: Phát hiện độ lệch giữa mã nguồn Rust và tệp header ${HEADER_PATH}!" >&2
        rm -f "${TMP_HEADER}"
        exit 1
    fi
    rm -f "${TMP_HEADER}"
    echo "  ✓ Header hoàn toàn khớp với Rust ABI!"
else
    echo "==> [SVM Header] Tự động sinh tệp header ${HEADER_PATH} qua cbindgen..."
    cbindgen --config cbindgen.toml --crate svm-core --output "${HEADER_PATH}"
    echo "==> [SVM Header] Hoàn tất!"
fi
