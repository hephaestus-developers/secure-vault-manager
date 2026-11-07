/**
 * @file extension/src/services/wasm-crypto.ts
 * @brief Cầu nối gọi mô-đun WebAssembly `svm_wasm` phục vụ tính toán TOTP, sinh mật khẩu và giải mã ngoại tuyến.
 */

import { PasswordGeneratorOptions, PasswordStrengthResult, TotpSnapshot } from "./domain";

let wasmExports: any = null;
const encoder = new TextEncoder();
const decoder = new TextDecoder();

/**
 * Khởi tạo mô-đun WebAssembly `svm_wasm.wasm` nếu có sẵn trong môi trường trình duyệt.
 */
export async function initWasmCrypto(): Promise<boolean> {
  if (wasmExports) return true;
  try {
    const chromeApi = (globalThis as Record<string, any>).chrome;
    const wasmUrl = chromeApi?.runtime?.getURL
      ? chromeApi.runtime.getURL("svm_wasm.wasm")
      : "./svm_wasm.wasm";
    const response = await fetch(wasmUrl);
    const bytes = await response.arrayBuffer();
    const { instance } = await WebAssembly.instantiate(bytes, {});
    wasmExports = instance.exports;
    if (typeof crypto !== "undefined" && crypto.getRandomValues && wasmExports.svm_wasm_seed) {
      const seed = new BigUint64Array(1);
      crypto.getRandomValues(seed);
      wasmExports.svm_wasm_seed(seed[0]);
    }
    return true;
  } catch {
    return false;
  }
}

function invokeWasmJson(payload: Record<string, unknown>): any {
  if (!wasmExports) return null;
  const reqBytes = encoder.encode(JSON.stringify(payload));
  const reqPtr = wasmExports.svm_wasm_alloc(reqBytes.length);
  new Uint8Array(wasmExports.memory.buffer, reqPtr, reqBytes.length).set(reqBytes);

  const outLenPtr = wasmExports.svm_wasm_alloc(4);
  const outPtr = wasmExports.svm_wasm_invoke(reqPtr, reqBytes.length, outLenPtr);
  wasmExports.svm_wasm_dealloc(reqPtr, reqBytes.length);

  const outLen = new DataView(wasmExports.memory.buffer).getUint32(outLenPtr, true);
  wasmExports.svm_wasm_dealloc(outLenPtr, 4);

  const outBytes = new Uint8Array(wasmExports.memory.buffer, outPtr, outLen);
  const jsonStr = decoder.decode(outBytes);
  wasmExports.svm_wasm_dealloc(outPtr, outLen);
  return JSON.parse(jsonStr);
}

/**
 * Sinh mật khẩu mạnh cục bộ thông qua WASM (hoặc WebCrypto dự phòng).
 */
export async function generatePasswordLocal(
  options: PasswordGeneratorOptions
): Promise<{ password: string; strength: PasswordStrengthResult }> {
  await initWasmCrypto();
  const res = invokeWasmJson({
    action: "generate_password",
    length: options.length,
    uppercase: options.uppercase,
    lowercase: options.lowercase,
    digits: options.digits,
    symbols: options.symbols,
  });
  if (res?.ok && res.data) {
    return {
      password: res.data.password,
      strength: {
        score: res.data.score ?? 4,
        percent: res.data.percent ?? 95,
        guesses: 1e12,
        feedback: res.data.feedback ?? "",
      },
    };
  }

  let charset = "";
  if (options.lowercase) charset += "abcdefghijkmnopqrstuvwxyz";
  if (options.uppercase) charset += "ABCDEFGHJKLMNPQRSTUVWXYZ";
  if (options.digits) charset += "23456789";
  if (options.symbols) charset += "!@#$%^&*()-_=+[]{}?";
  if (!charset) charset = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";

  const rnd = new Uint8Array(options.length);
  crypto.getRandomValues(rnd);
  let password = "";
  for (let i = 0; i < rnd.length; i++) {
    password += charset[rnd[i] % charset.length];
  }
  return {
    password,
    strength: {
      score: password.length >= 16 ? 4 : 3,
      percent: Math.min(100, password.length * 5),
      guesses: 1e10,
      feedback: "",
    },
  };
}

/**
 * Sinh mã xác thực 2 bước (TOTP) cục bộ qua mô-đun WASM.
 */
export async function generateTotpLocal(secret: string): Promise<TotpSnapshot | null> {
  await initWasmCrypto();
  const nowSecs = Math.floor(Date.now() / 1000);
  const res = invokeWasmJson({
    action: "totp",
    secret,
    timestamp: nowSecs,
  });
  if (res?.ok && res.data) {
    return {
      code: res.data.code,
      remaining: res.data.remaining,
    };
  }
  return null;
}
