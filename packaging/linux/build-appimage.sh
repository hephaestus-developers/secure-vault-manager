#!/usr/bin/env bash
# ==============================================================================
# @file packaging/linux/build-appimage.sh
# @brief Build portable AppImage package for Secure Vault Manager
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/package_linux.sh" --appimage "$@"
