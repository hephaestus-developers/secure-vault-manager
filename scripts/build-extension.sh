#!/usr/bin/env bash
# ==============================================================================
# @file scripts/build-extension.sh
# @brief Script đóng gói WebExtension (MV3) cho Chrome/Firefox.
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

EXT_DIR="${ROOT_DIR}/extension"

echo "==> [SVM Extension] Bắt đầu đóng gói Browser Extension (MV3)..."

if [[ ! -d "${EXT_DIR}" ]]; then
    echo "Thông báo: Thư mục extension/ chưa được khởi tạo."
    exit 0
fi

cd "${EXT_DIR}"

if command -v node >/dev/null 2>&1; then
    node --no-warnings scripts/build.mjs
    node --no-warnings scripts/post-build.js
    echo "==> [SVM Extension] Đóng gói extension thành công (dist/chrome & dist/firefox)!"
else
    echo "Lỗi: Node.js chưa được cài đặt. Vui lòng cài Node.js để build extension." >&2
    exit 1
fi
