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

HOST_NAME="io.hephaestus.secure_vault_manager"
HOST_BIN=""

CANDIDATES=(
    "${ROOT_DIR}/build/debug/core/svm-native-host"
    "${ROOT_DIR}/build/release/core/svm-native-host"
    "${ROOT_DIR}/core/target/debug/svm-native-host"
    "${ROOT_DIR}/core/target/release/svm-native-host"
)

for c in "${CANDIDATES[@]}"; do
    if [[ -f "${c}" ]]; then
        HOST_BIN="$(cd "$(dirname "${c}")" && pwd)/$(basename "${c}")"
        break
    fi
done

if [[ -z "${HOST_BIN}" ]]; then
    echo "Lỗi: Binary svm-native-host chưa được biên dịch. Vui lòng chạy ./scripts/build.sh trước." >&2
    exit 1
fi

CUSTOM_CHROME_ID="${1:-}"

echo "==> [SVM Native Host] Cài đặt Native Messaging Host manifest cho [${HOST_NAME}]..."
echo "    Đường dẫn binary: ${HOST_BIN}"
if [[ -n "${CUSTOM_CHROME_ID}" ]]; then
    echo "    Bổ sung Custom Chrome Extension ID: ${CUSTOM_CHROME_ID}"
fi

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

# Danh sách origins cho Chrome/Chromium/Brave/Edge
ORIGINS=(
    "chrome-extension://nhkjlnifnoicniecninomjjckhnbkmem/"
    "chrome-extension://cblgnkccngfnmepkgkgljklmjgfkcfhd/"
    "chrome-extension://dkmcefgkjjfkhdlnodfkblffchgajhgn/"
    "chrome-extension://fkmjebflnmjmfppkcfcjjlkbbbfgnofn/"
)

if [[ -n "${CUSTOM_CHROME_ID}" ]]; then
    ORIGINS+=("chrome-extension://${CUSTOM_CHROME_ID}/")
fi

ORIGIN_JSON_ENTRIES=""
for orig in "${ORIGINS[@]}"; do
    if [[ -z "${ORIGIN_JSON_ENTRIES}" ]]; then
        ORIGIN_JSON_ENTRIES="    \"${orig}\""
    else
        ORIGIN_JSON_ENTRIES="${ORIGIN_JSON_ENTRIES},
    \"${orig}\""
    fi
done

# Nội dung manifest cho Chromium-based browsers
MANIFEST_CHROME=$(cat <<EOF
{
  "name": "${HOST_NAME}",
  "description": "Secure Vault Manager Native Messaging Host",
  "path": "${HOST_BIN}",
  "type": "stdio",
  "allowed_origins": [
${ORIGIN_JSON_ENTRIES}
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
    "extension@secure-vault.hephaestus.io",
    "secure-vault-manager@hephaestus.io"
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
