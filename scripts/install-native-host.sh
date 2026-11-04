#!/usr/bin/env bash
# ==============================================================================
# @file scripts/install-native-host.sh
# @brief Script cài đặt Native Messaging Host manifest cho trình duyệt trên Linux/macOS.
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

HOST_NAME="io.hephaestus.svm"
HOST_BIN="${ROOT_DIR}/core/target/release/svm-native-host"

if [[ ! -f "${HOST_BIN}" ]]; then
    HOST_BIN="${ROOT_DIR}/core/target/debug/svm-native-host"
fi

if [[ ! -f "${HOST_BIN}" ]]; then
    echo "Lỗi: Binary svm-native-host chưa được biên dịch. Chạy ./scripts/build.sh trước." >&2
    exit 1
fi

echo "==> [SVM Native Host] Cài đặt Native Messaging Host manifest cho [${HOST_NAME}]..."
echo "    Đường dẫn binary: ${HOST_BIN}"

# Xác định thư mục cấu hình trình duyệt theo hệ điều hành
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    CHROME_DIR="${HOME}/.config/google-chrome/NativeMessagingHosts"
    CHROMIUM_DIR="${HOME}/.config/chromium/NativeMessagingHosts"
    BRAVE_DIR="${HOME}/.config/BraveSoftware/Brave-Browser/NativeMessagingHosts"
    EDGE_DIR="${HOME}/.config/microsoft-edge/NativeMessagingHosts"
    FIREFOX_DIR="${HOME}/.mozilla/native-messaging-hosts"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    CHROME_DIR="${HOME}/Library/Application Support/Google/Chrome/NativeMessagingHosts"
    CHROMIUM_DIR="${HOME}/Library/Application Support/Chromium/NativeMessagingHosts"
    BRAVE_DIR="${HOME}/Library/Application Support/BraveSoftware/Brave-Browser/NativeMessagingHosts"
    EDGE_DIR="${HOME}/Library/Application Support/Microsoft Edge/NativeMessagingHosts"
    FIREFOX_DIR="${HOME}/Library/Application Support/Mozilla/NativeMessagingHosts"
else
    echo "Hệ điều hành chưa được hỗ trợ tự động: $OSTYPE"
    exit 1
fi

# Nội dung manifest cho Chromium-based browsers
MANIFEST_CHROME=$(cat <<EOF
{
  "name": "${HOST_NAME}",
  "description": "Secure Vault Manager Native Messaging Host",
  "path": "${HOST_BIN}",
  "type": "stdio",
  "allowed_origins": [
    "chrome-extension://fkmjebflnmjmfppkcfcjjlkbbbfgnofn/"
  ]
}
EOF
)

# Nội dung manifest cho Firefox
MANIFEST_FIREFOX=$(cat <<EOF
{
  "name": "${HOST_NAME}",
  "description": "Secure Vault Manager Native Messaging Host",
  "path": "${HOST_BIN}",
  "type": "stdio",
  "allowed_extensions": [
    "svm@hephaestus.io"
  ]
}
EOF
)

install_manifest() {
    local target_dir="$1"
    local content="$2"
    if [[ -d "$(dirname "${target_dir}")" ]] || [[ -d "${target_dir}" ]]; then
        mkdir -p "${target_dir}"
        echo "${content}" > "${target_dir}/${HOST_NAME}.json"
        echo "  ✓ Đã cài đặt manifest tại: ${target_dir}/${HOST_NAME}.json"
    fi
}

install_manifest "${CHROME_DIR}" "${MANIFEST_CHROME}"
install_manifest "${CHROMIUM_DIR}" "${MANIFEST_CHROME}"
install_manifest "${BRAVE_DIR}" "${MANIFEST_CHROME}"
install_manifest "${EDGE_DIR}" "${MANIFEST_CHROME}"
install_manifest "${FIREFOX_DIR}" "${MANIFEST_FIREFOX}"

echo "==> [SVM Native Host] Hoàn tất cài đặt Native Host!"
