#!/usr/bin/env bash
# ==============================================================================
# @file scripts/clean.sh
# @brief Script dọn dẹp các tệp build, cache và artifact sinh ra trong quá trình phát triển.
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

echo "==> [SVM Clean] Dọn dẹp các artifact biên dịch..."

# Dọn dẹp thư mục target của Cargo
if [[ -d "core/target" ]]; then
    echo "  - Xóa core/target/..."
    cargo clean --manifest-path core/Cargo.toml
fi

# Dọn dẹp thư mục build của CMake
if [[ -d "build" ]]; then
    echo "  - Xóa build/..."
    rm -rf build/
fi

# Xóa các tệp thư viện động rải rác nếu có
find . -maxdepth 3 -type f \( -name "*.so" -o -name "*.dylib" -o -name "*.dll" \) -not -path "./.git/*" -delete 2>/dev/null || true

echo "==> [SVM Clean] Dọn dẹp hoàn tất!"
