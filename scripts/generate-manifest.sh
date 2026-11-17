#!/usr/bin/env bash
# ==============================================================================
# @file scripts/generate-manifest.sh
# @brief Generates SHA256 checksums and release-manifest.json for all dist/ artifacts
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DIST_DIR="${PROJECT_ROOT}/dist"

if [[ ! -d "${DIST_DIR}" ]]; then
    exit 0
fi

cd "${DIST_DIR}"

APP_NAME="secure-vault-manager"
VERSION="0.1.0"
GIT_COMMIT="$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")"
BUILD_TIME="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

# 1. Sinh bảng mã băm SHA256SUMS cho toàn bộ file phân phối
echo "==> [Checksums] Generating dist/SHA256SUMS..."
rm -f SHA256SUMS release-manifest.json

# Tìm tất cả file xuất xưởng hợp lệ (loại trừ thư mục, file ẩn, file staging)
ARTIFACTS=()
while IFS= read -r f; do
    if [[ -f "$f" ]]; then
        ARTIFACTS+=("$f")
    fi
done < <(find . -maxdepth 1 -type f ! -name ".*" ! -name "SHA256SUMS" ! -name "release-manifest.json" | sort)

if [[ ${#ARTIFACTS[@]} -eq 0 ]]; then
    echo "    [i] No artifacts found in dist/ to hash."
    exit 0
fi

sha256sum "${ARTIFACTS[@]}" | sed 's|\./||' > SHA256SUMS
echo "    ✓ Generated SHA256SUMS for ${#ARTIFACTS[@]} files."

# 2. Sinh release-manifest.json có cấu trúc
echo "==> [Manifest] Generating dist/release-manifest.json..."

{
    echo "{"
    echo "  \"appName\": \"${APP_NAME}\","
    echo "  \"version\": \"${VERSION}\","
    echo "  \"gitCommit\": \"${GIT_COMMIT}\","
    echo "  \"buildTimestamp\": \"${BUILD_TIME}\","
    echo "  \"artifacts\": ["

    first=true
    for f in "${ARTIFACTS[@]}"; do
        clean_name="${f#./}"
        size_bytes=$(stat -c%s "$clean_name" 2>/dev/null || stat -f%z "$clean_name" 2>/dev/null || echo 0)
        sha=$(sha256sum "$clean_name" | cut -d' ' -f1)

        if [[ "$first" == true ]]; then
            first=false
        else
            echo ","
        fi

        printf '    {\n      "filename": "%s",\n      "sizeBytes": %s,\n      "sha256": "%s"\n    }' \
            "$clean_name" "$size_bytes" "$sha"
    done

    echo ""
    echo "  ]"
    echo "}"
} > release-manifest.json

echo "    ✓ Generated release-manifest.json successfully."
