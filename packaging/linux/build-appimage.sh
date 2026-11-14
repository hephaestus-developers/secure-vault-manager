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
BUILD_MODE="release"

usage() {
    cat << EOF
Sử dụng: $(basename "$0") [TÙY CHỌN]

Tùy chọn đóng gói AppImage:
  -r, --release      Đóng gói bản xuất xưởng Release (Mặc định: tối ưu kích thước, strip symbols)
  -d, --debug        Đóng gói bản kiểm thử Debug (giữ nguyên debug symbols)
  -h, --help         Hiển thị hướng dẫn này

Ví dụ:
  ./packaging/linux/build-appimage.sh            # Đóng gói bản Release chuẩn (.AppImage)
  ./packaging/linux/build-appimage.sh --debug    # Đóng gói bản Debug (.AppImage)
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -r|--release)
            BUILD_MODE="release"
            shift
            ;;
        -d|--debug)
            BUILD_MODE="debug"
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Lỗi: Tham số không hợp lệ '$1'" >&2
            usage
            ;;
    esac
done

if [[ "${BUILD_MODE}" == "release" ]]; then
    CMAKE_BUILD_TYPE="Release"
    BUILD_DIR="${ROOT_DIR}/build/release"
    APPIMAGE_NAME="${PKG_NAME}-${PKG_VERSION}-x86_64.AppImage"
else
    CMAKE_BUILD_TYPE="Debug"
    BUILD_DIR="${ROOT_DIR}/build/debug"
    APPIMAGE_NAME="${PKG_NAME}-${PKG_VERSION}-debug-x86_64.AppImage"
fi

APP_DIR="${ROOT_DIR}/build/packages/AppDir-${BUILD_MODE}"
OUTPUT_DIR="${ROOT_DIR}/build/packages"

echo "======================================================================"
echo "==> [AppImage] Chế độ đóng gói: [${CMAKE_BUILD_TYPE^^}]"
echo "==> [AppImage] File mục tiêu: ${APPIMAGE_NAME}"
echo "======================================================================"

# Kiểm tra xem binary đã được biên dịch chưa, nếu chưa thì tự động biên dịch
if [[ ! -f "${BUILD_DIR}/ui/svm" || ! -f "${BUILD_DIR}/core/libsvm_core.so" ]]; then
    echo "==> [AppImage] Chưa tìm thấy binaries tại: ${BUILD_DIR}"
    echo "==> [AppImage] Đang tiến hành biên dịch [${CMAKE_BUILD_TYPE}]..."
    cmake -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" "${ROOT_DIR}"
    ninja -C "${BUILD_DIR}"
fi

echo "==> [AppImage] Chuẩn bị AppDir tại: ${APP_DIR}"
rm -rf "${APP_DIR}"
mkdir -p "${APP_DIR}/usr/bin"
mkdir -p "${APP_DIR}/usr/lib"
mkdir -p "${APP_DIR}/usr/share/applications"
mkdir -p "${APP_DIR}/usr/share/icons/hicolor"
mkdir -p "${OUTPUT_DIR}"

# 1. Cài đặt binaries và libraries qua CMake install vào AppDir
echo "==> [AppImage] Cài đặt binaries và libraries qua CMake install vào AppDir..."
cmake --install "${BUILD_DIR}" --prefix "${APP_DIR}/usr"

if [[ -d "${APP_DIR}/usr/lib64" && ! -d "${APP_DIR}/usr/lib" ]]; then
    mv "${APP_DIR}/usr/lib64" "${APP_DIR}/usr/lib"
fi

if [[ ! -f "${APP_DIR}/usr/bin/svm-cli" ]]; then
    if [[ -f "${BUILD_DIR}/core/svm-cli" ]]; then
        install -m 755 "${BUILD_DIR}/core/svm-cli" "${APP_DIR}/usr/bin/svm-cli"
    elif [[ -f "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-cli" ]]; then
        install -m 755 "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-cli" "${APP_DIR}/usr/bin/svm-cli"
    fi
fi

if [[ ! -f "${APP_DIR}/usr/bin/svm-native-host" ]]; then
    if [[ -f "${BUILD_DIR}/core/svm-native-host" ]]; then
        install -m 755 "${BUILD_DIR}/core/svm-native-host" "${APP_DIR}/usr/bin/svm-native-host"
    elif [[ -f "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-native-host" ]]; then
        install -m 755 "${ROOT_DIR}/core/target/${BUILD_MODE}/svm-native-host" "${APP_DIR}/usr/bin/svm-native-host"
    fi
fi

if [[ ! -f "${APP_DIR}/usr/lib/libsvm_core.so" ]]; then
    mkdir -p "${APP_DIR}/usr/lib"
    if [[ -f "${BUILD_DIR}/core/libsvm_core.so" ]]; then
        install -m 755 "${BUILD_DIR}/core/libsvm_core.so" "${APP_DIR}/usr/lib/libsvm_core.so"
    elif [[ -f "${ROOT_DIR}/core/target/${BUILD_MODE}/libsvm_core.so" ]]; then
        install -m 755 "${ROOT_DIR}/core/target/${BUILD_MODE}/libsvm_core.so" "${APP_DIR}/usr/lib/libsvm_core.so"
    fi
fi

# 2. Nhúng Qt6, ICU và Plugins (AppImage luôn là Standalone tự chứa)
STRIP_QT="false"
if [[ "${BUILD_MODE}" == "release" ]]; then
    STRIP_QT="true"
fi
"${ROOT_DIR}/packaging/linux/bundle-qt.sh" "${APP_DIR}/usr" "${STRIP_QT}"

# 3. Tối ưu hóa kích thước (Strip Symbols) nếu là bản RELEASE
if [[ "${BUILD_MODE}" == "release" ]]; then
    echo "==> [AppImage] Đang tối ưu hóa kích thước: loại bỏ debug symbols (strip)..."
    strip --strip-unneeded "${APP_DIR}/usr/bin/svm" 2>/dev/null || true
    strip --strip-unneeded "${APP_DIR}/usr/bin/svm-cli" 2>/dev/null || true
    strip --strip-unneeded "${APP_DIR}/usr/bin/svm-native-host" 2>/dev/null || true
    strip --strip-unneeded "${APP_DIR}/usr/lib/libsvm_core.so" 2>/dev/null || true
else
    echo "==> [AppImage] Chế độ DEBUG: Giữ nguyên debug symbols."
fi

# 4. Desktop Entry & Icons
install -m 644 "${ROOT_DIR}/packaging/linux/io.hephaestus.svm.desktop" "${APP_DIR}/usr/share/applications/io.hephaestus.svm.desktop"
cp "${ROOT_DIR}/packaging/linux/io.hephaestus.svm.desktop" "${APP_DIR}/io.hephaestus.svm.desktop"

if [[ -d "${ROOT_DIR}/packaging/linux/icons/hicolor" ]]; then
    mkdir -p "${APP_DIR}/usr/share/icons/hicolor"
    cp -r "${ROOT_DIR}/packaging/linux/icons/hicolor/"* "${APP_DIR}/usr/share/icons/hicolor/"
    if [[ -f "${ROOT_DIR}/packaging/linux/icons/hicolor/256x256/apps/io.hephaestus.svm.png" ]]; then
        cp "${ROOT_DIR}/packaging/linux/icons/hicolor/256x256/apps/io.hephaestus.svm.png" "${APP_DIR}/io.hephaestus.svm.png"
    fi
    if [[ -f "${ROOT_DIR}/packaging/linux/icons/hicolor/scalable/apps/io.hephaestus.svm.svg" ]]; then
        cp "${ROOT_DIR}/packaging/linux/icons/hicolor/scalable/apps/io.hephaestus.svm.svg" "${APP_DIR}/.DirIcon"
    fi
fi

# 5. Tạo file thực thi AppRun tiêu chuẩn
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

# 6. Đóng gói thành file .AppImage nếu có appimagetool
if command -v appimagetool >/dev/null 2>&1; then
    echo "==> [AppImage] Chạy appimagetool để tạo file..."
    ARCH=x86_64 appimagetool "${APP_DIR}" "${OUTPUT_DIR}/${APPIMAGE_NAME}"
    echo "  ✓ AppImage đã được tạo tại: ${OUTPUT_DIR}/${APPIMAGE_NAME}"
else
    echo "==> [AppImage] Hoàn tất chuẩn bị AppDir tại: ${APP_DIR}"
    echo "    (Mẹo: Cài đặt 'appimagetool' để tự động đóng gói ra file .AppImage đơn lẻ)"
fi
