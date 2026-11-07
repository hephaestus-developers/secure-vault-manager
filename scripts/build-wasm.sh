#!/usr/bin/env bash
# ==============================================================================
# @file scripts/build-wasm.sh
# @brief Script biên dịch crate svm-wasm thành gói WebAssembly cho Browser Extension.
# @author Hai Pham Ngoc <ngochai285nd@gmail.com>
# @copyright (c) 2026 Hephaestus Developers. All rights reserved.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

OUT_DIR="${ROOT_DIR}/core/crates/svm-wasm/pkg"
mkdir -p "${OUT_DIR}"

cd "${ROOT_DIR}/core/crates/svm-wasm"

echo "==> [SVM WASM] Bắt đầu biên dịch WebAssembly cho Browser Extension..."

cargo build --manifest-path "${ROOT_DIR}/core/Cargo.toml" -p svm-wasm --target wasm32-unknown-unknown --release

WASM_BIN="${ROOT_DIR}/core/target/wasm32-unknown-unknown/release/svm_wasm.wasm"
cp "${WASM_BIN}" "${OUT_DIR}/svm_wasm.wasm"

cat > "${OUT_DIR}/svm_wasm.js" << 'EOF'
let wasmInstance = null;
const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder();

export default async function init(wasmInput) {
  if (wasmInstance) return wasmInstance;
  const source = wasmInput || new URL("./svm_wasm.wasm", import.meta.url);
  let bytes;
  if (source instanceof ArrayBuffer || ArrayBuffer.isView(source)) {
    bytes = source;
  } else {
    const resp = await fetch(source);
    bytes = await resp.arrayBuffer();
  }
  const { instance } = await WebAssembly.instantiate(bytes, {});
  wasmInstance = instance.exports;
  if (typeof crypto !== "undefined" && crypto.getRandomValues) {
    const seedArr = new BigUint64Array(1);
    crypto.getRandomValues(seedArr);
    wasmInstance.svm_wasm_seed(seedArr[0]);
  }
  return wasmInstance;
}

export function dispatchWasm(action, payload = {}) {
  if (!wasmInstance) {
    throw new Error("SVM WASM module has not been initialized. Call init() first.");
  }
  const reqStr = JSON.stringify({ action, ...payload });
  const reqBytes = textEncoder.encode(reqStr);
  const reqPtr = wasmInstance.svm_wasm_alloc(reqBytes.length);
  new Uint8Array(wasmInstance.memory.buffer, reqPtr, reqBytes.length).set(reqBytes);

  const outLenPtr = wasmInstance.svm_wasm_alloc(4);
  const outPtr = wasmInstance.svm_wasm_invoke(reqPtr, reqBytes.length, outLenPtr);
  wasmInstance.svm_wasm_dealloc(reqPtr, reqBytes.length);

  const outLen = new DataView(wasmInstance.memory.buffer).getUint32(outLenPtr, true);
  wasmInstance.svm_wasm_dealloc(outLenPtr, 4);

  const outBytes = new Uint8Array(wasmInstance.memory.buffer, outPtr, outLen);
  const outStr = textDecoder.decode(outBytes);
  wasmInstance.svm_wasm_dealloc(outPtr, outLen);

  return JSON.parse(outStr);
}
EOF

cat > "${OUT_DIR}/svm_wasm.d.ts" << 'EOF'
export default function init(wasmInput?: string | URL | ArrayBuffer | ArrayBufferView): Promise<WebAssembly.Exports>;
export function dispatchWasm<T = unknown>(action: string, payload?: Record<string, unknown>): {
  ok: boolean;
  data?: T;
  error?: string;
};
EOF

cat > "${OUT_DIR}/package.json" << 'EOF'
{
  "name": "@svm/wasm-pkg",
  "version": "0.1.0",
  "type": "module",
  "main": "svm_wasm.js",
  "types": "svm_wasm.d.ts",
  "files": [
    "svm_wasm.wasm",
    "svm_wasm.js",
    "svm_wasm.d.ts"
  ]
}
EOF

echo "==> [SVM WASM] Gói WebAssembly đã được xuất ra: ${OUT_DIR}"
