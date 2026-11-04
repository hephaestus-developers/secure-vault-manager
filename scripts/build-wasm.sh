#!/usr/bin/env bash
# ==============================================================================
# @file scripts/build-wasm.sh
# @brief Script biên dịch crate svm-wasm thành gói WebAssembly cho Browser Extension.
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

OUT_DIR="${ROOT_DIR}/core/crates/svm-wasm/pkg"

cd "${ROOT_DIR}/core/crates/svm-wasm"

echo "==> [SVM WASM] Bắt đầu biên dịch WebAssembly cho Browser Extension..."

if command -v wasm-pack >/dev/null 2>&1; then
    wasm-pack build --target web --out-dir "${OUT_DIR}"
    echo "==> [SVM WASM] Gói WebAssembly đã được xuất ra: ${OUT_DIR}"
else
    echo "==> [Cargo] wasm-pack chưa được cài đặt. Biên dịch qua cargo target wasm32..."
    rustup target add wasm32-unknown-unknown 2>/dev/null || true
    cargo build --target wasm32-unknown-unknown --release
    echo "==> [SVM WASM] Đã build thành công binary WASM tại core/target/wasm32-unknown-unknown/release!"
fi
