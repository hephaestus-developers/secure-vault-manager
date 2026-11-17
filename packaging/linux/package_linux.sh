#!/usr/bin/env bash
# ==============================================================================
# Comprehensive packaging script for Linux:
# Generates Portable Directory, TAR.GZ, DEB, RPM, and AppImage formats
# with bundled Qt 6 libraries, ICU, and zero-knowledge Rust Core.
# ==============================================================================
set -euo pipefail
umask 022

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$PROJECT_ROOT"

APP_NAME="secure-vault-manager"
VERSION="0.1.0"
ARCH="x86_64"
DIST_DIR="$PROJECT_ROOT/dist"
BUILD_MODE="release"

FORMAT_ALL=true
BUILD_TAR=false
BUILD_DEB=false
BUILD_RPM=false
BUILD_APPIMAGE=false

usage() {
    cat << EOF
Sử dụng: $(basename "$0") [TÙY CHỌN]

Định dạng xuất xưởng:
  --all              Đóng gói tất cả định dạng (TAR.GZ, DEB, RPM, AppImage - Mặc định)
  --tar              Chỉ đóng gói thư mục Portable và file .tar.gz (kèm bundled Qt)
  --deb              Chỉ đóng gói gói Debian / Ubuntu (.deb - dùng Qt hệ thống)
  --rpm              Chỉ đóng gói gói Red Hat / Fedora (.rpm - dùng Qt hệ thống)
  --appimage         Chỉ đóng gói gói Portable AppImage (All-in-one tự chứa)

Tùy chọn cấu hình:
  -r, --release      Chế độ Release tối ưu hóa dung lượng (Mặc định)
  -d, --debug        Chế độ Debug kèm debug symbols
  -h, --help         Hiển thị hướng dẫn này

Ví dụ:
  ./packaging/linux/package_linux.sh              # Đóng gói toàn bộ các định dạng
  ./packaging/linux/package_linux.sh --appimage   # Chỉ tạo file AppImage
  ./packaging/linux/package_linux.sh --deb        # Chỉ tạo file .deb
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --all)
            FORMAT_ALL=true
            shift
            ;;
        --tar)
            FORMAT_ALL=false
            BUILD_TAR=true
            shift
            ;;
        --deb)
            FORMAT_ALL=false
            BUILD_DEB=true
            shift
            ;;
        --rpm)
            FORMAT_ALL=false
            BUILD_RPM=true
            shift
            ;;
        --appimage)
            FORMAT_ALL=false
            BUILD_APPIMAGE=true
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
    BUILD_TAR=true
    BUILD_DEB=true
    BUILD_RPM=true
    BUILD_APPIMAGE=true
fi

BUNDLE_DIR="$DIST_DIR/${APP_NAME}-${VERSION}-linux-${ARCH}"

echo "===================================================="
echo "  Packaging $APP_NAME v$VERSION for Linux ($ARCH)"
echo "  Mode: [${BUILD_MODE^^}]"
echo "  Targets: TAR=$BUILD_TAR, DEB=$BUILD_DEB, RPM=$BUILD_RPM, AppImage=$BUILD_APPIMAGE"
echo "===================================================="

# -----------------------------------------------------------------------------
# 1. Build application via CMake Presets
# -----------------------------------------------------------------------------
echo "-> [1/5] Building application (${BUILD_MODE} mode)..."
cmake --preset "${BUILD_MODE}"
cmake --build --preset "build-${BUILD_MODE}" -j"$(nproc 2>/dev/null || echo 4)"

RELEASE_BIN="$PROJECT_ROOT/build/${BUILD_MODE}/svm"
RELEASE_LIB="$PROJECT_ROOT/build/${BUILD_MODE}/libsvm_core.so"
CLI_BIN="$PROJECT_ROOT/build/${BUILD_MODE}/svm-cli"
HOST_BIN="$PROJECT_ROOT/build/${BUILD_MODE}/svm-native-host"

if [[ ! -f "$RELEASE_BIN" ]]; then
    echo "Lỗi: Không tìm thấy file thực thi tại: $RELEASE_BIN" >&2
    exit 1
fi

mkdir -p "$DIST_DIR"

# -----------------------------------------------------------------------------
# 2. Helper functions for Bundling Qt 6 (Portable TAR.GZ & AppImage)
# -----------------------------------------------------------------------------
QT_PLUGIN_DIR=$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || echo "/usr/lib/x86_64-linux-gnu/qt6/plugins")
QT_LIB_DIR=$(qmake6 -query QT_INSTALL_LIBS 2>/dev/null || echo "/usr/lib/x86_64-linux-gnu")

copy_lib() {
    local pattern="$1"
    for f in $(find "$QT_LIB_DIR" /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu -maxdepth 1 -name "$pattern" 2>/dev/null); do
        if [ -f "$f" ] || [ -L "$f" ]; then
            cp -d "$f" "$BUNDLE_DIR/lib/" 2>/dev/null || true
            if [ -L "$f" ]; then
                local target
                target=$(readlink -f "$f")
                if [ -f "$target" ]; then
                    cp -p "$target" "$BUNDLE_DIR/lib/" 2>/dev/null || true
                fi
            fi
        fi
    done
}

copy_plugin_dir() {
    local dir_name="$1"
    if [ -d "$QT_PLUGIN_DIR/$dir_name" ]; then
        mkdir -p "$BUNDLE_DIR/plugins/$dir_name"
        cp -dpR "$QT_PLUGIN_DIR/$dir_name"/* "$BUNDLE_DIR/plugins/$dir_name/" 2>/dev/null || true
    fi
}

prepare_bundle_dir() {
    if [[ -d "$BUNDLE_DIR" && -f "$BUNDLE_DIR/svm" && -d "$BUNDLE_DIR/lib" ]]; then
        return 0
    fi

    echo "-> Preparing Portable Staging Directory: $BUNDLE_DIR"
    rm -rf "$BUNDLE_DIR"
    mkdir -p "$BUNDLE_DIR/bin"
    mkdir -p "$BUNDLE_DIR/lib"
    mkdir -p "$BUNDLE_DIR/plugins"
    mkdir -p "$BUNDLE_DIR/share/applications"
    mkdir -p "$BUNDLE_DIR/share/icons/hicolor/scalable/apps"

    cp -p "$RELEASE_BIN" "$BUNDLE_DIR/bin/svm-bin"
    if [[ -f "$CLI_BIN" ]]; then cp -p "$CLI_BIN" "$BUNDLE_DIR/bin/"; fi
    if [[ -f "$HOST_BIN" ]]; then cp -p "$HOST_BIN" "$BUNDLE_DIR/bin/"; fi
    if [[ -f "$RELEASE_LIB" ]]; then cp -p "$RELEASE_LIB" "$BUNDLE_DIR/lib/"; fi

    cp -p "$SCRIPT_DIR/svm" "$BUNDLE_DIR/svm"
    cp -p "$SCRIPT_DIR/svm" "$BUNDLE_DIR/bin/svm"
    cp -p "$SCRIPT_DIR/qt.conf" "$BUNDLE_DIR/bin/qt.conf"
    chmod +x "$BUNDLE_DIR/svm" "$BUNDLE_DIR/bin/svm"

    # Bundle Qt 6 core libraries
    copy_lib "libQt6Widgets.so.6*"
    copy_lib "libQt6Gui.so.6*"
    copy_lib "libQt6Core.so.6*"
    copy_lib "libQt6Network.so.6*"
    copy_lib "libQt6DBus.so.6*"
    copy_lib "libQt6XcbQpa.so.6*"
    copy_lib "libQt6WaylandClient.so.6*"
    copy_lib "libQt6WlShellIntegration.so.6*"
    copy_lib "libQt6Svg.so.6*"
    copy_lib "libQt6OpenGL.so.6*"
    copy_lib "libQt6WaylandEglClientHwIntegration.so.6*"

    # Bundle input dependencies & supporting runtimes
    copy_lib "libxkbcommon*.so.*"
    copy_lib "libicui18n.so.*"
    copy_lib "libicuuc.so.*"
    copy_lib "libicudata.so.*"
    copy_lib "libdouble-conversion.so.*"
    copy_lib "libmd4c.so.*"
    copy_lib "libpcre2-16.so.*"
    copy_lib "libb2.so.*"
    copy_lib "libharfbuzz.so.*"
    copy_lib "libfreetype.so.*"
    copy_lib "libpng16.so.*"
    copy_lib "libz.so.*"
    copy_lib "libzstd.so.*"

    # Bundle Qt plugins
    copy_plugin_dir "platforms"
    copy_plugin_dir "wayland-shell-integration"
    copy_plugin_dir "wayland-decoration-client"
    copy_plugin_dir "wayland-graphics-integration-client"
    copy_plugin_dir "xcbglintegrations"
    copy_plugin_dir "platformthemes"
    copy_plugin_dir "imageformats"
    copy_plugin_dir "iconengines"
    copy_plugin_dir "platforminputcontexts"

    # Strip symbols in release mode
    if [[ "${BUILD_MODE}" == "release" ]]; then
        find "$BUNDLE_DIR/lib" -type f -name "*.so*" -exec strip --strip-unneeded {} + 2>/dev/null || true
        find "$BUNDLE_DIR/plugins" -type f -name "*.so*" -exec strip --strip-unneeded {} + 2>/dev/null || true
        strip --strip-unneeded "$BUNDLE_DIR/bin/svm-bin" 2>/dev/null || true
        if [[ -f "$BUNDLE_DIR/bin/svm-cli" ]]; then strip --strip-unneeded "$BUNDLE_DIR/bin/svm-cli" 2>/dev/null || true; fi
        if [[ -f "$BUNDLE_DIR/bin/svm-native-host" ]]; then strip --strip-unneeded "$BUNDLE_DIR/bin/svm-native-host" 2>/dev/null || true; fi
    fi

    # Desktop & Assets
    if [[ -f "$SCRIPT_DIR/io.github.hephaestus.svm.desktop" ]]; then
        cp -p "$SCRIPT_DIR/io.github.hephaestus.svm.desktop" "$BUNDLE_DIR/share/applications/"
    fi
    if [[ -f "$SCRIPT_DIR/icons/hicolor/scalable/apps/io.github.hephaestus.svm.svg" ]]; then
        cp -p "$SCRIPT_DIR/icons/hicolor/scalable/apps/io.github.hephaestus.svm.svg" "$BUNDLE_DIR/share/icons/hicolor/scalable/apps/"
        cp -p "$SCRIPT_DIR/icons/hicolor/scalable/apps/io.github.hephaestus.svm.svg" "$BUNDLE_DIR/svm.svg"
    fi
    cp -p "$PROJECT_ROOT/README.md" "$BUNDLE_DIR/" 2>/dev/null || true
    if [[ -f "$PROJECT_ROOT/LICENSE" ]]; then cp -p "$PROJECT_ROOT/LICENSE" "$BUNDLE_DIR/"; fi
}

# -----------------------------------------------------------------------------
# 3. Build Portable TAR.GZ Package
# -----------------------------------------------------------------------------
if [[ "${BUILD_TAR}" == true ]]; then
    prepare_bundle_dir
    echo "-> Creating Portable TAR.GZ: $DIST_DIR/${APP_NAME}-${VERSION}-linux-${ARCH}.tar.gz"
    TAR_FILE="$DIST_DIR/${APP_NAME}-${VERSION}-linux-${ARCH}.tar.gz"
    (cd "$DIST_DIR" && tar -czf "$(basename "$TAR_FILE")" "$(basename "$BUNDLE_DIR")")
    echo "       ✓ Generated: $(basename "$TAR_FILE") ($(du -h "$TAR_FILE" | cut -f1))"
fi

# -----------------------------------------------------------------------------
# 4. Build Debian (.deb) Package (Native System Package)
# -----------------------------------------------------------------------------
if [[ "${BUILD_DEB}" == true ]]; then
    echo "-> Building Debian (.deb) package (native system dependencies)..."
    "$SCRIPT_DIR/build-deb.sh" "--${BUILD_MODE}"

    # Copy to dist/
    for deb in "$PROJECT_ROOT/build/packages"/*.deb; do
        if [[ -f "$deb" ]]; then
            cp -p "$deb" "$DIST_DIR/"
            echo "       ✓ Generated: $(basename "$deb") ($(du -h "$deb" | cut -f1))"
        fi
    done
fi

# -----------------------------------------------------------------------------
# 5. Build Red Hat (.rpm) Package (Native System Package)
# -----------------------------------------------------------------------------
if [[ "${BUILD_RPM}" == true ]]; then
    echo "-> Building RPM (.rpm) package (native system dependencies)..."
    "$SCRIPT_DIR/build-rpm.sh" "--${BUILD_MODE}" 2>/dev/null || true

    # Copy to dist/
    for rpm in "$PROJECT_ROOT/build/packages"/*.rpm; do
        if [[ -f "$rpm" ]]; then
            cp -p "$rpm" "$DIST_DIR/"
            echo "       ✓ Generated: $(basename "$rpm") ($(du -h "$rpm" | cut -f1))"
        fi
    done
fi

# -----------------------------------------------------------------------------
# 6. Build Portable AppImage (All-in-one Standalone Package)
# -----------------------------------------------------------------------------
if [[ "${BUILD_APPIMAGE}" == true ]]; then
    prepare_bundle_dir
    echo "-> Building Portable AppImage (all-in-one)..."

    APPIMAGE_TOOL=""
    RUNTIME_FILE="$SCRIPT_DIR/runtime-x86_64"

    if [[ -f "$SCRIPT_DIR/appimagetool" ]]; then
        APPIMAGE_TOOL="$SCRIPT_DIR/appimagetool"
    elif command -v appimagetool &>/dev/null; then
        APPIMAGE_TOOL="appimagetool"
    fi

    if [[ -n "$APPIMAGE_TOOL" ]]; then
        APPDIR="$DIST_DIR/.appdir"
        rm -rf "$APPDIR"
        mkdir -p "$APPDIR"
        cp -dpR "$BUNDLE_DIR"/* "$APPDIR/"

        # AppImage entry point setup
        cp -p "$APPDIR/svm" "$APPDIR/AppRun"
        chmod +x "$APPDIR/AppRun"
        cp -p "$SCRIPT_DIR/io.github.hephaestus.svm.desktop" "$APPDIR/"
        if [[ -f "$SCRIPT_DIR/icons/hicolor/scalable/apps/io.github.hephaestus.svm.svg" ]]; then
            cp -p "$SCRIPT_DIR/icons/hicolor/scalable/apps/io.github.hephaestus.svm.svg" "$APPDIR/"
            ln -sf io.github.hephaestus.svm.svg "$APPDIR/.DirIcon"
        fi

        APPIMAGE_PKG="$DIST_DIR/${APP_NAME}-${VERSION}-${ARCH}.AppImage"

        RUNTIME_ARG=""
        if [[ -f "$RUNTIME_FILE" ]]; then
            RUNTIME_ARG="--runtime-file $RUNTIME_FILE"
        fi

        ARCH=x86_64 "$APPIMAGE_TOOL" --appimage-extract-and-run $RUNTIME_ARG "$APPDIR" "$APPIMAGE_PKG" >/dev/null 2>&1 || \
        ARCH=x86_64 "$APPIMAGE_TOOL" $RUNTIME_ARG "$APPDIR" "$APPIMAGE_PKG" >/dev/null 2>&1 || true

        rm -rf "$APPDIR"
        if [[ -f "$APPIMAGE_PKG" ]]; then
            chmod +x "$APPIMAGE_PKG"
            echo "       ✓ Generated: $(basename "$APPIMAGE_PKG") ($(du -h "$APPIMAGE_PKG" | cut -f1))"
        else
            echo "   [!] Không thể tạo AppImage (yêu cầu FUSE hoặc runtime tương thích)."
        fi
    else
        echo "   [!] Không tìm thấy appimagetool, bỏ qua AppImage."
    fi
fi

echo ""
echo "===================================================="
echo "✓ All Linux Packages Processed Successfully!"
echo "  Artifacts located in: $DIST_DIR"
echo "===================================================="
ls -lh "$DIST_DIR"/*.tar.gz "$DIST_DIR"/*.deb "$DIST_DIR"/*.rpm "$DIST_DIR"/*.AppImage 2>/dev/null || true

# Tự động sinh mã băm SHA256SUMS và release-manifest.json
if [[ -x "$PROJECT_ROOT/scripts/generate-manifest.sh" ]]; then
    echo ""
    "$PROJECT_ROOT/scripts/generate-manifest.sh"
fi
