#!/usr/bin/env bash
# ==============================================================================
# @file scripts/format.sh
# @brief Script tự động định dạng mã nguồn Rust, C++ và Web/TS.
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
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
    cargo fmt --all -- --check
else
    cargo fmt --all
fi

echo "==> [SVM Format] Định dạng mã nguồn C++ (clang-format)..."
if command -v clang-format >/dev/null 2>&1; then
    CLANG_FILES=$(find src -type f \( -name "*.hpp" -o -name "*.cpp" \) 2>/dev/null || true)
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

echo "==> [SVM Format] Định dạng mã nguồn Web/TS (prettier)..."
PRETTIER_BIN="${ROOT_DIR}/extension/node_modules/.bin/prettier"
if [[ ! -x "${PRETTIER_BIN}" ]] && command -v prettier >/dev/null 2>&1; then
    PRETTIER_BIN="$(command -v prettier)"
fi

if [[ -x "${PRETTIER_BIN}" ]]; then
    PRETTIER_TARGETS=(
        "extension/src/**/*.{ts,tsx,js,mjs,css,html}"
        "extension/scripts/**/*.{js,mjs}"
        "extension/*.{json,md,html}"
    )
    if [[ "${CHECK_ONLY}" == true ]]; then
        "${PRETTIER_BIN}" --check "${PRETTIER_TARGETS[@]}" 2>/dev/null || true
    else
        "${PRETTIER_BIN}" --write "${PRETTIER_TARGETS[@]}" 2>/dev/null || true
    fi
else
    echo "  [Bỏ qua] prettier chưa được cài đặt trong extension."
fi

echo "==> [SVM Format] Hoàn tất định dạng mã nguồn!"
