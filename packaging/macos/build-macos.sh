#!/usr/bin/env bash
# ==============================================================================
# @file packaging/macos/build-macos.sh
# @brief Wrapper delegating to package_macos.sh
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/package_macos.sh" "$@"
