#!/usr/bin/env bash
# ==============================================================================
# @file packaging/macos/package_macos.sh
# @brief Comprehensive macOS Packaging Script for Secure Vault Manager
# Generates:
#   1. Standalone Application Bundle (Secure Vault Manager.app) with bundled Qt 6
#   2. Apple Disk Image (.dmg) with drag-to-Applications shortcut
#   3. macOS Installer Package (.pkg) for system/enterprise deployment
#   4. Portable ZIP Archive (.zip)
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$PROJECT_ROOT"

APP_NAME="Secure Vault Manager"
APP_DIR_NAME="Secure Vault Manager.app"
PKG_BASE_NAME="SecureVaultManager"
VERSION="2026.11.0"
DIST_DIR="$PROJECT_ROOT/dist"
BUILD_MODE="release"

FORMAT_ALL=true
BUILD_DMG=false
BUILD_PKG=false
BUILD_ZIP=false

usage() {
    cat << EOF
Sử dụng: $(basename "$0") [TÙY CHỌN]

Định dạng xuất xưởng macOS:
  --all              Đóng gói tất cả định dạng (.app, .dmg, .pkg, .zip - Mặc định)
  --dmg              Chỉ tạo Apple Disk Image (.dmg)
  --pkg              Chỉ tạo Installer Package (.pkg)
  --zip              Chỉ tạo Portable ZIP Archive (.zip)

Tùy chọn cấu hình:
  -r, --release      Chế độ Release tối ưu hóa dung lượng (Mặc định)
  -d, --debug        Chế độ Debug kèm debug symbols
  -h, --help         Hiển thị hướng dẫn này

Ví dụ:
  ./packaging/macos/package_macos.sh            # Đóng gói toàn bộ các định dạng
  ./packaging/macos/package_macos.sh --dmg      # Chỉ tạo file .dmg
  ./packaging/macos/package_macos.sh --pkg      # Chỉ tạo file .pkg
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --all)
            FORMAT_ALL=true
            shift
            ;;
        --dmg)
            FORMAT_ALL=false
            BUILD_DMG=true
            shift
            ;;
        --pkg)
            FORMAT_ALL=false
            BUILD_PKG=true
            shift
            ;;
        --zip)
            FORMAT_ALL=false
            BUILD_ZIP=true
            shift
            ;;
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

if [[ "${FORMAT_ALL}" == true ]]; then
    BUILD_DMG=true
    BUILD_PKG=true
    BUILD_ZIP=true
fi

CPU_CORES=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
APP_BUNDLE="$DIST_DIR/$APP_DIR_NAME"
DMG_FILE="$DIST_DIR/${PKG_BASE_NAME}-${VERSION}.dmg"
PKG_FILE="$DIST_DIR/${PKG_BASE_NAME}-${VERSION}.pkg"
ZIP_FILE="$DIST_DIR/${PKG_BASE_NAME}-${VERSION}-macos.zip"

echo "===================================================="
echo "  Packaging $APP_NAME v$VERSION for macOS"
echo "  Mode: [${BUILD_MODE^^}]"
echo "  Targets: DMG=$BUILD_DMG, PKG=$BUILD_PKG, ZIP=$BUILD_ZIP"
echo "===================================================="

# -----------------------------------------------------------------------------
# 1. Build application via CMake Presets
# -----------------------------------------------------------------------------
echo "-> [1/5] Building application (${BUILD_MODE} mode)..."
cmake --preset "${BUILD_MODE}"
cmake --build --preset "build-${BUILD_MODE}" -j"$CPU_CORES"

RELEASE_BIN="$PROJECT_ROOT/build/${BUILD_MODE}/svm"
RELEASE_LIB="$PROJECT_ROOT/build/${BUILD_MODE}/libsvm_core.dylib"
if [[ ! -f "$RELEASE_LIB" ]]; then
    RELEASE_LIB="$PROJECT_ROOT/build/${BUILD_MODE}/libsvm_core.so"
fi
CLI_BIN="$PROJECT_ROOT/build/${BUILD_MODE}/svm-cli"
HOST_BIN="$PROJECT_ROOT/build/${BUILD_MODE}/svm-native-host"

mkdir -p "$DIST_DIR"

# -----------------------------------------------------------------------------
# 2. Assemble Application Bundle (.app)
# -----------------------------------------------------------------------------
echo "-> [2/5] Assembling macOS Application Bundle: $APP_BUNDLE"
rm -rf "$APP_BUNDLE"
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Frameworks"
mkdir -p "$APP_BUNDLE/Contents/Resources/native-messaging"

cp -p "$RELEASE_BIN" "$APP_BUNDLE/Contents/MacOS/svm"
if [[ -f "$CLI_BIN" ]]; then cp -p "$CLI_BIN" "$APP_BUNDLE/Contents/MacOS/"; fi
if [[ -f "$HOST_BIN" ]]; then cp -p "$HOST_BIN" "$APP_BUNDLE/Contents/MacOS/"; fi

if [[ -f "$RELEASE_LIB" ]]; then
    cp -p "$RELEASE_LIB" "$APP_BUNDLE/Contents/Frameworks/libsvm_core.dylib" 2>/dev/null || \
    cp -p "$RELEASE_LIB" "$APP_BUNDLE/Contents/Frameworks/"
fi

# Plist & Icons
if [[ -f "$SCRIPT_DIR/Info.plist" ]]; then
    cp -p "$SCRIPT_DIR/Info.plist" "$APP_BUNDLE/Contents/"
fi
if [[ -f "$SCRIPT_DIR/svm.icns" ]]; then
    cp -p "$SCRIPT_DIR/svm.icns" "$APP_BUNDLE/Contents/Resources/"
fi

# Native messaging templates
if [[ -f "$PROJECT_ROOT/packaging/native-messaging/chrome.json.in" ]]; then
    sed "s|@SVM_NATIVE_HOST_PATH@|/Applications/${APP_DIR_NAME}/Contents/MacOS/svm-native-host|g" \
        "$PROJECT_ROOT/packaging/native-messaging/chrome.json.in" \
        > "$APP_BUNDLE/Contents/Resources/native-messaging/io.hephaestus.secure_vault_manager.json"
fi

# Fix RPATH for macOS dynamic loading
if command -v install_name_tool &>/dev/null; then
    for dylib in "$APP_BUNDLE/Contents/Frameworks"/*; do
        if [[ -f "$dylib" ]]; then
            dylib_name=$(basename "$dylib")
            install_name_tool -id "@rpath/$dylib_name" "$dylib" 2>/dev/null || true
        fi
    done
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP_BUNDLE/Contents/MacOS/svm" 2>/dev/null || true
fi

# -----------------------------------------------------------------------------
# 3. Deploy Qt Dependencies via macdeployqt
# -----------------------------------------------------------------------------
echo "-> [3/5] Bundling Qt dependencies with macdeployqt..."
if command -v macdeployqt &>/dev/null; then
    macdeployqt "$APP_BUNDLE" -always-overwrite
    echo "       ✓ Qt frameworks successfully bundled into $APP_BUNDLE"
else
    echo "   [!] macdeployqt not found in PATH (sử dụng Qt hệ thống hoặc chạy trên macOS host)."
fi

# -----------------------------------------------------------------------------
# 4. Build Apple Disk Image (.dmg)
# -----------------------------------------------------------------------------
if [[ "${BUILD_DMG}" == true ]]; then
    echo "-> [4/5] Building Apple Disk Image (.dmg)..."
    if command -v hdiutil &>/dev/null; then
        DMG_STAGING="$DIST_DIR/.dmg_staging"
        rm -rf "$DMG_STAGING"
        mkdir -p "$DMG_STAGING"

        cp -R "$APP_BUNDLE" "$DMG_STAGING/"
        ln -sf /Applications "$DMG_STAGING/Applications"
        cp -p "$PROJECT_ROOT/README.md" "$DMG_STAGING/" 2>/dev/null || true

        rm -f "$DMG_FILE"
        hdiutil create -volname "$APP_NAME" \
                       -srcfolder "$DMG_STAGING" \
                       -ov \
                       -format UDZO \
                       "$DMG_FILE" >/dev/null

        rm -rf "$DMG_STAGING"
        if [[ -f "$DMG_FILE" ]]; then
            echo "       ✓ Generated: $(basename "$DMG_FILE") ($(du -h "$DMG_FILE" | cut -f1))"
        fi
    else
        echo "   [!] hdiutil chỉ khả dụng trên macOS. Bỏ qua tạo file .dmg."
    fi
fi

# -----------------------------------------------------------------------------
# 5. Build macOS Installer Package (.pkg)
# -----------------------------------------------------------------------------
if [[ "${BUILD_PKG}" == true ]]; then
    echo "-> [5/5] Building macOS Installer Package (.pkg)..."
    if command -v pkgbuild &>/dev/null && command -v productbuild &>/dev/null; then
        COMPONENT_PKG="$DIST_DIR/${PKG_BASE_NAME}-component.pkg"
        rm -f "$COMPONENT_PKG" "$PKG_FILE"

        pkgbuild --component "$APP_BUNDLE" \
                 --install-location "/Applications" \
                 --identifier "io.github.hephaestus.svm.pkg" \
                 --version "$VERSION" \
                 "$COMPONENT_PKG" >/dev/null

        if [[ -f "$SCRIPT_DIR/distribution.xml" ]]; then
            (cd "$DIST_DIR" && productbuild --distribution "$SCRIPT_DIR/distribution.xml" \
                         --package-path "$DIST_DIR" \
                         "$PKG_FILE" >/dev/null 2>&1) || \
            productbuild --package "$COMPONENT_PKG" "$PKG_FILE" >/dev/null
        else
            productbuild --package "$COMPONENT_PKG" "$PKG_FILE" >/dev/null
        fi

        rm -f "$COMPONENT_PKG"
        if [[ -f "$PKG_FILE" ]]; then
            echo "       ✓ Generated: $(basename "$PKG_FILE") ($(du -h "$PKG_FILE" | cut -f1))"
        fi
    else
        echo "   [!] pkgbuild hoặc productbuild không tìm thấy. Bỏ qua tạo file .pkg."
    fi
fi

# -----------------------------------------------------------------------------
# 6. Build Portable ZIP Archive (.zip)
# -----------------------------------------------------------------------------
if [[ "${BUILD_ZIP}" == true ]]; then
    if command -v ditto &>/dev/null; then
        ditto -c -k --keepParent "$APP_BUNDLE" "$ZIP_FILE"
        if [[ -f "$ZIP_FILE" ]]; then
            echo "       ✓ Generated: $(basename "$ZIP_FILE") ($(du -h "$ZIP_FILE" | cut -f1))"
        fi
    elif command -v zip &>/dev/null; then
        (cd "$DIST_DIR" && zip -r -q -y "$(basename "$ZIP_FILE")" "$APP_DIR_NAME")
        if [[ -f "$ZIP_FILE" ]]; then
            echo "       ✓ Generated: $(basename "$ZIP_FILE") ($(du -h "$ZIP_FILE" | cut -f1))"
        fi
    fi
fi

echo ""
echo "===================================================="
echo "✓ macOS Packaging Processed Successfully!"
echo "  Artifacts located in: $DIST_DIR"
echo "===================================================="
ls -lh "$DIST_DIR"/*.dmg "$DIST_DIR"/*.pkg "$DIST_DIR"/*.zip 2>/dev/null || true

# Tự động sinh mã băm SHA256SUMS và release-manifest.json
if [[ -x "$PROJECT_ROOT/scripts/generate-manifest.sh" ]]; then
    "$PROJECT_ROOT/scripts/generate-manifest.sh"
fi
