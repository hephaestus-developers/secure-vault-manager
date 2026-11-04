#!/usr/bin/env bash
# ==============================================================================
# @file scripts/build-extension.sh
# @brief Script đóng gói WebExtension (MV3) cho Chrome/Firefox.
# @author Hephaestus Developers <engineering@hephaestus.io>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

EXT_DIR="${ROOT_DIR}/extension"

echo "==> [SVM Extension] Bắt đầu đóng gói Browser Extension (MV3)..."

if [[ ! -d "${EXT_DIR}" ]]; then
    echo "Thông báo: Thư mục extension/ chưa được khởi tạo."
    echo "Phân hệ extension sẽ được liên kết khi phát triển WebExtension."
    exit 0
fi

cd "${EXT_DIR}"

if command -v pnpm >/dev/null 2>&1; then
    pnpm install
    pnpm build
    echo "==> [SVM Extension] Đóng gói extension thành công!"
else
    echo "Lỗi: pnpm chưa được cài đặt. Vui lòng cài pnpm để build extension." >&2
    exit 1
fi
