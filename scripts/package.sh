#!/usr/bin/env bash
# ==============================================================================
# @file scripts/package.sh
# @brief Universal packaging dispatcher across Linux, macOS, and Windows.
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

OS="$(uname -s)"

case "$OS" in
    Linux*)
        "$PROJECT_ROOT/packaging/linux/package_linux.sh" "$@"
        ;;
    Darwin*)
        if [[ -x "$PROJECT_ROOT/packaging/macos/package_macos.sh" ]]; then
            "$PROJECT_ROOT/packaging/macos/package_macos.sh" "$@"
        else
            "$PROJECT_ROOT/packaging/macos/build-macos.sh" "$@"
        fi
        ;;
    CYGWIN*|MINGW*|MSYS*)
        if [[ -f "$PROJECT_ROOT/packaging/windows/package_windows.bat" ]]; then
            cmd.exe /c "$PROJECT_ROOT/packaging/windows/package_windows.bat" "$@"
        else
            cmd.exe /c "$PROJECT_ROOT/packaging/windows/build-windows.bat" "$@"
        fi
        ;;
    *)
        echo "Lỗi: Hệ điều hành không được hỗ trợ: $OS" >&2
        exit 1
        ;;
esac

# Tự động sinh mã băm SHA256SUMS và release-manifest.json
if [[ -x "$PROJECT_ROOT/scripts/generate-manifest.sh" ]]; then
    "$PROJECT_ROOT/scripts/generate-manifest.sh"
fi
