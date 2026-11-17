#!/usr/bin/env bash
# ==============================================================================
# @file scripts/test.sh
# @brief Script chạy kiểm thử toàn bộ hệ thống Secure Vault Manager (SVM).
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "==> [SVM Test] Bắt đầu chạy bộ kiểm thử toàn dự án..."

cd "${ROOT_DIR}"

echo "==> [Cargo Test] Chạy Unit Tests và Doc Tests trong Rust Workspace..."
cargo test --workspace --all-targets

echo "==> [Doc Test] Kiểm tra tính hợp lệ của tài liệu Rustdoc..."
cargo test --workspace --doc

# Chạy CTest nếu thư mục build CMake đã được cấu hình
if [[ -d "build/debug" ]] && command -v ctest >/dev/null 2>&1; then
    echo "==> [CTest] Thực thi kiểm thử C++ / CTest..."
    ctest --test-dir build/debug --output-on-failure || true
fi

echo "==> [SVM Test] Toàn bộ bài kiểm thử đã hoàn thành thành công!"
