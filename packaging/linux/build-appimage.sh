#!/usr/bin/env bash
# ==============================================================================
# @file packaging/linux/build-appimage.sh
# @brief Build portable AppImage package for Secure Vault Manager
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

PKG_NAME="SecureVaultManager"
PKG_VERSION="0.1.0"
APP_DIR="${ROOT_DIR}/build/packages/AppDir"
OUTPUT_DIR="${ROOT_DIR}/build/packages"
APPIMAGE_NAME="${PKG_NAME}-${PKG_VERSION}-x86_64.AppImage"

BUILD_DIR="${ROOT_DIR}/build/release"
if [[ ! -f "${BUILD_DIR}/ui/svm" ]]; then
    BUILD_DIR="${ROOT_DIR}/build/debug"
fi

if [[ ! -f "${BUILD_DIR}/ui/svm" ]]; then
    echo "==> [AppImage] Biên dịch release..."
    cmake -B "${ROOT_DIR}/build/release" -G Ninja -DCMAKE_BUILD_TYPE=Release "${ROOT_DIR}"
    ninja -C "${ROOT_DIR}/build/release"
    BUILD_DIR="${ROOT_DIR}/build/release"
fi

echo "==> [AppImage] Chuẩn bị AppDir tại: ${APP_DIR}"
rm -rf "${APP_DIR}"
mkdir -p "${APP_DIR}/usr/bin"
mkdir -p "${APP_DIR}/usr/lib"
mkdir -p "${APP_DIR}/usr/share/applications"
mkdir -p "${APP_DIR}/usr/share/icons/hicolor"
mkdir -p "${OUTPUT_DIR}"

# 1. Sao chép binaries
install -m 755 "${BUILD_DIR}/ui/svm" "${APP_DIR}/usr/bin/svm"

if [[ -f "${BUILD_DIR}/core/svm-cli" ]]; then
    install -m 755 "${BUILD_DIR}/core/svm-cli" "${APP_DIR}/usr/bin/svm-cli"
elif [[ -f "${ROOT_DIR}/core/target/release/svm-cli" ]]; then
    install -m 755 "${ROOT_DIR}/core/target/release/svm-cli" "${APP_DIR}/usr/bin/svm-cli"
fi

if [[ -f "${BUILD_DIR}/core/svm-native-host" ]]; then
    install -m 755 "${BUILD_DIR}/core/svm-native-host" "${APP_DIR}/usr/bin/svm-native-host"
elif [[ -f "${ROOT_DIR}/core/target/release/svm-native-host" ]]; then
    install -m 755 "${ROOT_DIR}/core/target/release/svm-native-host" "${APP_DIR}/usr/bin/svm-native-host"
fi

# 2. Sao chép thư viện lõi C-ABI
if [[ -f "${BUILD_DIR}/core/libsvm_core.so" ]]; then
    install -m 755 "${BUILD_DIR}/core/libsvm_core.so" "${APP_DIR}/usr/lib/libsvm_core.so"
elif [[ -f "${ROOT_DIR}/core/target/release/libsvm_core.so" ]]; then
    install -m 755 "${ROOT_DIR}/core/target/release/libsvm_core.so" "${APP_DIR}/usr/lib/libsvm_core.so"
fi

# 3. Desktop Entry & Icons
install -m 644 "${ROOT_DIR}/packaging/linux/io.hephaestus.svm.desktop" "${APP_DIR}/usr/share/applications/io.hephaestus.svm.desktop"
cp "${ROOT_DIR}/packaging/linux/io.hephaestus.svm.desktop" "${APP_DIR}/io.hephaestus.svm.desktop"

if [[ ! -d "${ROOT_DIR}/packaging/linux/icons/hicolor" ]]; then
    "${ROOT_DIR}/scripts/generate-icons.py"
fi
mkdir -p "${APP_DIR}/usr/share/icons/hicolor"
cp -r "${ROOT_DIR}/packaging/linux/icons/hicolor/"* "${APP_DIR}/usr/share/icons/hicolor/"
cp "${ROOT_DIR}/packaging/linux/icons/hicolor/256x256/apps/io.hephaestus.svm.png" "${APP_DIR}/io.hephaestus.svm.png"
cp "${ROOT_DIR}/packaging/linux/icons/hicolor/scalable/apps/io.hephaestus.svm.svg" "${APP_DIR}/.DirIcon"

# 4. Tạo file thực thi AppRun tiêu chuẩn
cat << 'EOF' > "${APP_DIR}/AppRun"
#!/usr/bin/env bash
HERE="$(dirname "$(readlink -f "${0}")")"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH:-}"
export XDG_DATA_DIRS="${HERE}/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins:${QT_PLUGIN_PATH:-}"
exec "${HERE}/usr/bin/svm" "$@"
EOF
chmod +x "${APP_DIR}/AppRun"

# 5. Đóng gói thành file .AppImage nếu có appimagetool
if command -v appimagetool >/dev/null 2>&1; then
    echo "==> [AppImage] Chạy appimagetool để tạo file..."
    ARCH=x86_64 appimagetool "${APP_DIR}" "${OUTPUT_DIR}/${APPIMAGE_NAME}"
    echo "  ✓ AppImage đã được tạo tại: ${OUTPUT_DIR}/${APPIMAGE_NAME}"
else
    echo "==> [AppImage] Hoàn tất chuẩn bị AppDir tại: ${APP_DIR}"
    echo "    (Mẹo: Cài đặt 'appimagetool' để tự động đóng gói ra file .AppImage đơn lẻ)"
fi
