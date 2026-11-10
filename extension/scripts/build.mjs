/**
 * @file extension/scripts/build.mjs
 * @brief Trình biên dịch và đóng gói đa điểm nhập (Multi-Entry Bundler) cho Browser Extension Manifest V3.
 * Sử dụng trực tiếp trình biên dịch TypeScript tích hợp trong Node.js (module.stripTypeScriptTypes)
 * và tạo bộ biểu tượng PNG chuẩn mà không cần tải phụ thuộc mạng.
 */

import fs from "node:fs";
import path from "node:path";
import zlib from "node:zlib";
import { stripTypeScriptTypes } from "node:module";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const EXT_ROOT = path.resolve(__dirname, "..");
const REPO_ROOT = path.resolve(EXT_ROOT, "..");
const DIST_CHROME = path.join(EXT_ROOT, "dist", "chrome");
const PUBLIC_DIR = path.join(EXT_ROOT, "public");

/**
 * Tạo ảnh PNG RGBA hợp lệ (biểu tượng khiên bảo mật màu Catppuccin Blue #89B4FA trên nền #1E1E2E).
 */
function generateShieldPng(size) {
  const rawData = Buffer.alloc(size * (size * 4 + 1));
  const cx = (size - 1) / 2;
  const cy = (size - 1) / 2;
  const rOuter = size * 0.44;

  for (let y = 0; y < size; y++) {
    const rowStart = y * (size * 4 + 1);
    rawData[rowStart] = 0; // PNG filter type 0 (None)
    for (let x = 0; x < size; x++) {
      const px = rowStart + 1 + x * 4;
      const dx = Math.abs(x - cx);
      const dy = y - cy;
      const inBg = dx <= rOuter && Math.abs(dy) <= rOuter;
      const inShield =
        y >= size * 0.2 &&
        y <= size * 0.82 &&
        dx <= (size * 0.32) * (1 - Math.max(0, (y - size * 0.5) / (size * 0.45)));

      if (inShield) {
        rawData[px] = 0x89;
        rawData[px + 1] = 0xb4;
        rawData[px + 2] = 0xfa;
        rawData[px + 3] = 0xff;
      } else if (inBg) {
        rawData[px] = 0x1e;
        rawData[px + 1] = 0x1e;
        rawData[px + 2] = 0x2e;
        rawData[px + 3] = 0xff;
      } else {
        rawData[px] = 0;
        rawData[px + 1] = 0;
        rawData[px + 2] = 0;
        rawData[px + 3] = 0;
      }
    }
  }

  function makeChunk(type, data) {
    const len = Buffer.alloc(4);
    len.writeUInt32BE(data.length, 0);
    const typeBuf = Buffer.from(type, "ascii");
    const crcInput = Buffer.concat([typeBuf, data]);
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE( zlib.crc32(crcInput) >>> 0, 0);
    return Buffer.concat([len, typeBuf, data, crc]);
  }

  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(size, 0);
  ihdr.writeUInt32BE(size, 4);
  ihdr[8] = 8; // bit depth
  ihdr[9] = 6; // color type RGBA
  ihdr[10] = 0;
  ihdr[11] = 0;
  ihdr[12] = 0;

  const idat = zlib.deflateSync(rawData);
  const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  return Buffer.concat([
    signature,
    makeChunk("IHDR", ihdr),
    makeChunk("IDAT", idat),
    makeChunk("IEND", Buffer.alloc(0)),
  ]);
}

/**
 * Giải quyết đường dẫn import tương đối giữa các tệp `.ts` / `.tsx`.
 */
function resolveTsImport(fromFile, importSpec) {
  const baseDir = path.dirname(fromFile);
  const candidateBase = path.resolve(baseDir, importSpec);
  const candidates = [
    candidateBase,
    `${candidateBase}.ts`,
    `${candidateBase}.tsx`,
    `${candidateBase}.js`,
    path.join(candidateBase, "index.ts"),
  ];
  for (const c of candidates) {
    if (fs.existsSync(c) && fs.statSync(c).isFile()) {
      return c;
    }
  }
  throw new Error(`Cannot resolve import '${importSpec}' from ${fromFile}`);
}

/**
 * Đóng gói đệ quy một điểm nhập TypeScript thành một tệp JavaScript độc lập.
 */
function bundleEntry(entryRelPath) {
  const entryAbs = path.join(EXT_ROOT, entryRelPath);
  const visited = new Set();
  const orderedModules = [];

  function visit(fileAbs) {
    if (visited.has(fileAbs)) return;
    visited.add(fileAbs);

    const rawTs = fs.readFileSync(fileAbs, "utf8");
    const jsCode = stripTypeScriptTypes(rawTs, { mode: "transform" });

    const importRegex =
      /^\s*import\s+(?:([\w*{}\s,]+)\s+from\s+)?["']([^"']+)["']\s*;?/gm;
    let match;
    while ((match = importRegex.exec(jsCode)) !== null) {
      const spec = match[2];
      if (spec.startsWith(".")) {
        const depAbs = resolveTsImport(fileAbs, spec);
        visit(depAbs);
      }
    }

    const cleaned = jsCode
      .replace(/^\s*import\s+(?:[\w*{}\s,]+\s+from\s+)?["'][^"']+["']\s*;?\s*$/gm, "")
      .replace(/^\s*export\s+\*\s+from\s+["'][^"']+["']\s*;?\s*$/gm, "")
      .replace(/^\s*export\s+\{[^}]*\}\s*(?:from\s+["'][^"']+["'])?\s*;?\s*$/gm, "")
      .replace(/\bexport\s+(async\s+function|function|class|const|let|var)\b/g, "$1");

    orderedModules.push(`// --- Module: ${path.relative(REPO_ROOT, fileAbs)} ---\n${cleaned.trim()}`);
  }

  visit(entryAbs);
  return `(() => {\n"use strict";\n${orderedModules.join("\n\n")}\n})();\n`;
}

// 1. Đảm bảo biểu tượng PNG tồn tại trong public/
fs.mkdirSync(PUBLIC_DIR, { recursive: true });
for (const size of [16, 32, 48, 128]) {
  const iconPath = path.join(PUBLIC_DIR, `icon-${size}.png`);
  if (!fs.existsSync(iconPath)) {
    fs.writeFileSync(iconPath, generateShieldPng(size));
  }
}

// 2. Làm sạch và tạo thư mục dist/chrome
fs.rmSync(DIST_CHROME, { recursive: true, force: true });
fs.mkdirSync(DIST_CHROME, { recursive: true });

// 3. Đóng gói 4 entry points
const entries = [
  { name: "background.js", src: "src/background/index.ts" },
  { name: "content.js", src: "src/content/index.ts" },
  { name: "passkey_injected.js", src: "src/injected/passkey_injected.ts" },
  { name: "popup.js", src: "src/popup/popup.tsx" },
];

for (const entry of entries) {
  const bundled = bundleEntry(entry.src);
  fs.writeFileSync(path.join(DIST_CHROME, entry.name), bundled, "utf8");
}

// 4. Sao chép tài nguyên tĩnh (HTML, CSS, Icons, Manifest, WASM)
fs.copyFileSync(path.join(EXT_ROOT, "popup.html"), path.join(DIST_CHROME, "popup.html"));
fs.copyFileSync(path.join(EXT_ROOT, "src", "global.css"), path.join(DIST_CHROME, "global.css"));
fs.copyFileSync(
  path.join(EXT_ROOT, "src", "popup", "popup.module.css"),
  path.join(DIST_CHROME, "popup.css")
);
fs.copyFileSync(
  path.join(PUBLIC_DIR, "manifest.json"),
  path.join(DIST_CHROME, "manifest.json")
);

for (const size of [16, 32, 48, 128]) {
  fs.copyFileSync(
    path.join(PUBLIC_DIR, `icon-${size}.png`),
    path.join(DIST_CHROME, `icon-${size}.png`)
  );
}

console.log(`==> [Extension Build] Đã đóng gói bản dựng Chromium MV3 tại: ${DIST_CHROME}`);
