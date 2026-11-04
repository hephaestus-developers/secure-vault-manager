#!/usr/bin/env bash
# ==============================================================================
# @file scripts/format.sh
# @brief Script tự động định dạng mã nguồn Rust, C/C++ và Web/TS.
# @author Hephaestus Developers <engineering@hephaestus.io>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

CHECK_ONLY=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --check)
            CHECK_ONLY=true
            shift
            ;;
        *)
            echo "Tham số không hợp lệ: $1" >&2
            exit 1
            ;;
    esac
done

cd "${ROOT_DIR}"

echo "==> [SVM Format] Định dạng mã nguồn Rust (rustfmt)..."
if [[ "${CHECK_ONLY}" == true ]]; then
    cargo fmt --manifest-path core/Cargo.toml --all -- --check
else
    cargo fmt --manifest-path core/Cargo.toml --all
fi

echo "==> [SVM Format] Định dạng mã nguồn C/C++ (clang-format)..."
if command -v clang-format >/dev/null 2>&1; then
    CLANG_FILES=$(find core/include ui -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.c" -o -name "*.cpp" \) 2>/dev/null || true)
    if [[ -n "${CLANG_FILES}" ]]; then
        if [[ "${CHECK_ONLY}" == true ]]; then
            clang-format --dry-run --Werror ${CLANG_FILES}
        else
            clang-format -i ${CLANG_FILES}
        fi
    fi
else
    echo "  [Bỏ qua] clang-format chưa được cài đặt trên hệ thống."
fi

echo "==> [SVM Format] Định dạng hoàn tất!"
