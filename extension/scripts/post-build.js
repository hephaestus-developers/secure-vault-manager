/**
 * @file extension/scripts/post-build.js
 * @brief Kịch bản hậu xử lý nhân bản gói mở rộng sang định dạng Firefox MV3 (`dist/firefox`).
 */

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const EXT_ROOT = path.resolve(__dirname, "..");
const DIST_CHROME = path.join(EXT_ROOT, "dist", "chrome");
const DIST_FIREFOX = path.join(EXT_ROOT, "dist", "firefox");
const FIREFOX_MANIFEST = path.join(EXT_ROOT, "public", "manifest.firefox.json");

fs.rmSync(DIST_FIREFOX, { recursive: true, force: true });
fs.cpSync(DIST_CHROME, DIST_FIREFOX, { recursive: true });

if (fs.existsSync(FIREFOX_MANIFEST)) {
  fs.copyFileSync(FIREFOX_MANIFEST, path.join(DIST_FIREFOX, "manifest.json"));
}

console.log(`==> [Extension Post-Build] Đã đồng bộ bản dựng Firefox MV3 tại: ${DIST_FIREFOX}`);
