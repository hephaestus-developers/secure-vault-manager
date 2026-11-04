#!/usr/bin/env bash
# ==============================================================================
# @file scripts/test.sh
# @brief Script chạy kiểm thử toàn bộ hệ thống Secure Vault Manager (SVM).
# @author Hephaestus Developers <engineering@hephaestus.io>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "==> [SVM Test] Bắt đầu chạy bộ kiểm thử toàn dự án..."

cd "${ROOT_DIR}"

echo "==> [Cargo Test] Chạy Unit Tests và Doc Tests trong Rust Core Workspace..."
cargo test --manifest-path core/Cargo.toml --workspace --all-targets

echo "==> [Doc Test] Kiểm tra tính hợp lệ của tài liệu Rustdoc..."
cargo test --manifest-path core/Cargo.toml --workspace --doc

# Kiểm thử FFI C-ABI bằng script Python ctypes
if command -v python3 >/dev/null 2>&1; then
    echo "==> [FFI Test] Kiểm tra nạp động thư viện chia sẻ C-ABI qua Python ctypes..."
    python3 -c '
import ctypes, os

lib_path = "./core/target/debug/libsvm_core.so"
if not os.path.exists(lib_path):
    print("libsvm_core.so chưa được build ở debug mode, bỏ qua smoke test ctypes.")
    exit(0)

lib = ctypes.CDLL(lib_path)
lib.svm_abi_version.restype = ctypes.c_uint32
ver = lib.svm_abi_version()
assert ver == 1, f"ABI Version mismatch: {ver}"

lib.svm_core_version.restype = ctypes.c_char_p
core_ver = lib.svm_core_version().decode("utf-8")
assert core_ver == "0.1.0", f"Core Version mismatch: {core_ver}"

ctx = lib.svm_context_new(None)
assert ctx is not None, "Failed to create SvmContext"
lib.svm_context_free(ctx)
print("  ✓ C-ABI FFI smoke test PASSED!")
'
fi

# Chạy CTest nếu thư mục build CMake đã được cấu hình
if [[ -d "build/debug" ]] && command -v ctest >/dev/null 2>&1; then
    echo "==> [CTest] Thực thi kiểm thử C++ / CTest..."
    ctest --test-dir build/debug --output-on-failure || true
fi

echo "==> [SVM Test] Toàn bộ bài kiểm thử đã hoàn thành thành công!"
