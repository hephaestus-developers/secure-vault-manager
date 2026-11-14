#!/usr/bin/env bash
# ==============================================================================
# bundle-qt.sh - Tự động thu thập và nhúng các thư viện Qt6, ICU và Plugins
# Dành cho đóng gói Standalone / Portable trên Linux (DEB, RPM, AppImage)
# ==============================================================================
set -euo pipefail

TARGET_DIR="${1:-}"
STRIP_SYMBOLS="${2:-false}"

if [[ -z "${TARGET_DIR}" ]]; then
    echo "Cách dùng: $0 <TARGET_DIR> [true|false (strip)]"
    exit 1
fi

echo "==> [Bundle Qt] Thu thập thư viện Qt6 & Plugins vào: ${TARGET_DIR}"

# 1. Xác định thư mục thư viện Qt6 và Plugins trên hệ thống
QT_LIB_DIR=""
QT_PLUGIN_DIR=""

if command -v qtpaths6 >/dev/null 2>&1; then
    QT_LIB_DIR="$(qtpaths6 --qt-query | grep '^QT_INSTALL_LIBS:' | cut -d':' -f2 || true)"
    QT_PLUGIN_DIR="$(qtpaths6 --qt-query | grep '^QT_INSTALL_PLUGINS:' | cut -d':' -f2 || true)"
fi

if [[ -z "${QT_LIB_DIR}" || ! -d "${QT_LIB_DIR}" ]]; then
    for candidate in "/usr/lib/x86_64-linux-gnu" "/lib/x86_64-linux-gnu" "/usr/lib64" "/usr/lib"; do
        if [[ -f "${candidate}/libQt6Core.so.6" ]]; then
            QT_LIB_DIR="${candidate}"
            break
        fi
    done
fi

if [[ -z "${QT_PLUGIN_DIR}" || ! -d "${QT_PLUGIN_DIR}" ]]; then
    for candidate in "/usr/lib/x86_64-linux-gnu/qt6/plugins" "/usr/lib64/qt6/plugins" "/usr/lib/qt6/plugins"; do
        if [[ -d "${candidate}/platforms" ]]; then
            QT_PLUGIN_DIR="${candidate}"
            break
        fi
    done
fi

if [[ -z "${QT_LIB_DIR}" ]]; then
    echo "LỖI: Không tìm thấy thư viện Qt6 trên hệ thống!" >&2
    exit 1
fi

DEST_LIB="${TARGET_DIR}/lib"
DEST_PLUGINS="${TARGET_DIR}/plugins"
DEST_BIN="${TARGET_DIR}/bin"

mkdir -p "${DEST_LIB}"
mkdir -p "${DEST_PLUGINS}"
mkdir -p "${DEST_BIN}"

# 2. Danh sách các thư viện Qt6 cốt lõi cần nhúng
QT_LIBS=(
    "libQt6Core.so.6"
    "libQt6Gui.so.6"
    "libQt6Widgets.so.6"
    "libQt6Network.so.6"
    "libQt6DBus.so.6"
    "libQt6XcbQpa.so.6"
    "libQt6WaylandClient.so.6"
    "libQt6WlShellIntegration.so.6"
    "libQt6Svg.so.6"
    "libQt6OpenGL.so.6"
    "libQt6OpenGLWidgets.so.6"
)

echo "==> [Bundle Qt] Đang sao chép các thư viện Qt6 cốt lõi..."
for lib in "${QT_LIBS[@]}"; do
    matched_files=$(find "${QT_LIB_DIR}" "/lib/x86_64-linux-gnu" -maxdepth 1 -name "${lib}*" 2>/dev/null || true)
    for src in ${matched_files}; do
        fname=$(basename "${src}")
        if [[ ! -e "${DEST_LIB}/${fname}" ]]; then
            cp -P "${src}" "${DEST_LIB}/"
        fi
    done
done

# 3. Sao chép ICU libraries (tránh lỗi mismatch ICU giữa các bản Linux)
echo "==> [Bundle Qt] Đang sao chép thư viện ICU (i18n, uc, data)..."
ICU_PATTERNS=("libicui18n.so.*" "libicuuc.so.*" "libicudata.so.*")
for pat in "${ICU_PATTERNS[@]}"; do
    matched_files=$(find "${QT_LIB_DIR}" "/lib/x86_64-linux-gnu" -maxdepth 1 -name "${pat}" 2>/dev/null || true)
    for src in ${matched_files}; do
        fname=$(basename "${src}")
        if [[ ! -e "${DEST_LIB}/${fname}" ]]; then
            cp -P "${src}" "${DEST_LIB}/"
        fi
    done
done

# 4. Sao chép Qt Platform & Imageformat Plugins
if [[ -n "${QT_PLUGIN_DIR}" && -d "${QT_PLUGIN_DIR}" ]]; then
    echo "==> [Bundle Qt] Đang sao chép Qt Plugins từ: ${QT_PLUGIN_DIR}..."
    
    # 4.1 Platforms (bắt buộc cho GUI: XCB và Wayland)
    if [[ -d "${QT_PLUGIN_DIR}/platforms" ]]; then
        mkdir -p "${DEST_PLUGINS}/platforms"
        for p in libqxcb.so libqwayland-generic.so libqwayland-egl.so libqoffscreen.so libqminimal.so; do
            if [[ -f "${QT_PLUGIN_DIR}/platforms/${p}" ]]; then
                cp -P "${QT_PLUGIN_DIR}/platforms/${p}" "${DEST_PLUGINS}/platforms/"
            fi
        done
    fi

    # 4.2 Imageformats (đọc icons SVG, PNG, ICO, JPEG)
    if [[ -d "${QT_PLUGIN_DIR}/imageformats" ]]; then
        mkdir -p "${DEST_PLUGINS}/imageformats"
        for img in libqsvg.so libqico.so libqjpeg.so libqgif.so; do
            if [[ -f "${QT_PLUGIN_DIR}/imageformats/${img}" ]]; then
                cp -P "${QT_PLUGIN_DIR}/imageformats/${img}" "${DEST_PLUGINS}/imageformats/"
            fi
        done
    fi

    # 4.3 Wayland Shell Integration
    if [[ -d "${QT_PLUGIN_DIR}/wayland-shell-integration" ]]; then
        mkdir -p "${DEST_PLUGINS}/wayland-shell-integration"
        cp -P "${QT_PLUGIN_DIR}/wayland-shell-integration/"*.so "${DEST_PLUGINS}/wayland-shell-integration/" 2>/dev/null || true
    fi

    # 4.4 Platformthemes (tích hợp theme hệ thống GTK3 nếu có)
    if [[ -d "${QT_PLUGIN_DIR}/platformthemes" ]]; then
        mkdir -p "${DEST_PLUGINS}/platformthemes"
        if [[ -f "${QT_PLUGIN_DIR}/platformthemes/libqgtk3.so" ]]; then
            cp -P "${QT_PLUGIN_DIR}/platformthemes/libqgtk3.so" "${DEST_PLUGINS}/platformthemes/"
        fi
    fi
fi

# 5. Tạo file cấu hình qt.conf chỉ định đường dẫn nội bộ
echo "==> [Bundle Qt] Khởi tạo cấu hình ${DEST_BIN}/qt.conf..."
cat << 'EOF' > "${DEST_BIN}/qt.conf"
[Paths]
Prefix = ..
Libraries = lib
Plugins = plugins
EOF

# 6. Tối ưu kích thước nếu được yêu cầu
if [[ "${STRIP_SYMBOLS}" == "true" ]]; then
    echo "==> [Bundle Qt] Tối ưu dung lượng: strip symbols các thư viện Qt đã nhúng..."
    find "${DEST_LIB}" -maxdepth 1 -name "libQt6*.so*" -exec strip --strip-unneeded {} 2>/dev/null + || true
    find "${DEST_LIB}" -maxdepth 1 -name "libicu*.so*" -exec strip --strip-unneeded {} 2>/dev/null + || true
    find "${DEST_PLUGINS}" -type f -name "*.so*" -exec strip --strip-unneeded {} 2>/dev/null + || true
fi

echo "==> [Bundle Qt] Hoàn tất nhúng Qt vào: ${TARGET_DIR}"
