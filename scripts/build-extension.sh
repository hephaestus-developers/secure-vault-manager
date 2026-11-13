#!/usr/bin/env bash
# ==============================================================================
# @file scripts/build-extension.sh
# @brief Script đóng gói WebExtension (MV3) cho Chrome/Firefox và nén file ZIP.
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

EXT_DIR="${ROOT_DIR}/extension"
DIST_DIR="${EXT_DIR}/dist"

echo "==> [SVM Extension] Bắt đầu đóng gói Browser Extension (MV3)..."

if [[ ! -d "${EXT_DIR}" ]]; then
    echo "Thông báo: Thư mục extension/ chưa được khởi tạo."
    exit 0
fi

cd "${EXT_DIR}"

if command -v node >/dev/null 2>&1; then
    node --no-warnings scripts/build.mjs
    node --no-warnings scripts/post-build.js
    echo "==> [SVM Extension] Biên dịch extension thành công (dist/chrome & dist/firefox)!"
else
    echo "Lỗi: Node.js chưa được cài đặt. Vui lòng cài Node.js để build extension." >&2
    exit 1
fi

echo "==> [SVM Extension] Bắt đầu nén các gói phân phối (.zip) vào extension/dist/..."

# Nén zip bằng python3 (chuẩn đa nền tảng, không phụ thuộc gói zip bên ngoài) hoặc lệnh zip
if command -v python3 >/dev/null 2>&1; then
    python3 -c '
import os, sys, zipfile

def zip_dir(src_dir, zip_path):
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for root, _, files in os.walk(src_dir):
            for file in files:
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, src_dir)
                zf.write(full_path, rel_path)

def zip_source(ext_dir, zip_path):
    excludes = {"dist", "node_modules", ".git", "__pycache__"}
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for root, dirs, files in os.walk(ext_dir):
            dirs[:] = [d for d in dirs if d not in excludes]
            for file in files:
                if file.endswith(".zip") or file.endswith(".tar.gz"):
                    continue
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, ext_dir)
                zf.write(full_path, rel_path)

ext_dir = sys.argv[1]
dist_dir = os.path.join(ext_dir, "dist")
chrome_dir = os.path.join(dist_dir, "chrome")
firefox_dir = os.path.join(dist_dir, "firefox")

# 1. secure-vault-manager-chrome.zip
if os.path.isdir(chrome_dir):
    zip_path = os.path.join(dist_dir, "secure-vault-manager-chrome.zip")
    zip_dir(chrome_dir, zip_path)
    sz = os.path.getsize(zip_path) // 1024
    print(f"    + Đã tạo: dist/secure-vault-manager-chrome.zip ({sz} KB)")

# 2. secure-vault-manager-firefox.zip
if os.path.isdir(firefox_dir):
    zip_path = os.path.join(dist_dir, "secure-vault-manager-firefox.zip")
    zip_dir(firefox_dir, zip_path)
    sz = os.path.getsize(zip_path) // 1024
    print(f"    + Đã tạo: dist/secure-vault-manager-firefox.zip ({sz} KB)")

# 3. extension-source-code.zip
src_zip_path = os.path.join(dist_dir, "extension-source-code.zip")
zip_source(ext_dir, src_zip_path)
sz = os.path.getsize(src_zip_path) // 1024
print(f"    + Đã tạo: dist/extension-source-code.zip ({sz} KB)")
' "${EXT_DIR}"
elif command -v zip >/dev/null 2>&1; then
    (cd "${DIST_DIR}/chrome" && zip -r -q "${DIST_DIR}/secure-vault-manager-chrome.zip" .)
    echo "    + Đã tạo: dist/secure-vault-manager-chrome.zip"

    (cd "${DIST_DIR}/firefox" && zip -r -q "${DIST_DIR}/secure-vault-manager-firefox.zip" .)
    echo "    + Đã tạo: dist/secure-vault-manager-firefox.zip"

    (cd "${EXT_DIR}" && zip -r -q "${DIST_DIR}/extension-source-code.zip" . -x "dist/*" -x "node_modules/*" -x "*.zip")
    echo "    + Đã tạo: dist/extension-source-code.zip"
else
    echo "Cảnh báo: Không tìm thấy python3 hoặc zip để nén file. Chỉ giữ lại thư mục dist/chrome và dist/firefox." >&2
fi

echo "==> [SVM Extension] Hoàn tất toàn bộ quy trình đóng gói Extension!"
