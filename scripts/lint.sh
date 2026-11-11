#!/usr/bin/env bash
# ==============================================================================
# @file scripts/lint.sh
# @brief Script kiểm tra tĩnh (linter & static analysis) cho toàn bộ dự án.
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

echo "==> [SVM Lint] Kiểm tra mã nguồn Rust bằng Clippy..."
cargo clippy --manifest-path core/Cargo.toml --workspace --all-targets -- -D warnings

echo "==> [SVM Lint] Kiểm tra tài liệu Rustdoc..."
cargo doc --manifest-path core/Cargo.toml --workspace --no-deps

echo "==> [SVM Lint] Kiểm tra phụ thuộc và bản quyền (cargo-deny nếu có)..."
if command -v cargo-deny >/dev/null 2>&1; then
    (cd core && (cargo-deny check 2>/dev/null || cargo-deny --offline check))
else
    echo "  [Bỏ qua] cargo-deny chưa được cài đặt."
fi

echo "==> [SVM Lint] Kiểm tra mã nguồn Extension Web/TS bằng oxlint..."
if [[ -x "${ROOT_DIR}/extension/node_modules/.bin/oxlint" ]] && command -v pnpm >/dev/null 2>&1; then
    pnpm --dir "${ROOT_DIR}/extension" lint
elif [[ -x "${ROOT_DIR}/extension/node_modules/.bin/oxlint" ]]; then
    (cd "${ROOT_DIR}/extension" && ./node_modules/.bin/oxlint --deny-warnings src scripts vite.config.ts)
elif command -v oxlint >/dev/null 2>&1; then
    (cd "${ROOT_DIR}/extension" && oxlint --deny-warnings src scripts vite.config.ts)
else
    echo "  [Lỗi] Chưa cài đặt oxlint cho extension (chạy: pnpm --dir extension install)." >&2
    exit 1
fi

echo "==> [SVM Lint] Kiểm tra tĩnh hoàn tất không phát hiện lỗi!"
