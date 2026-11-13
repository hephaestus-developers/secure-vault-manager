/**
 * @file extension/src/background/qr_crop_handler.ts
 * @brief Điều phối chụp tab, cắt vùng ảnh QR bằng OffscreenCanvas, giải mã và lưu TOTP vào Vault.
 */

import {
  BridgeRequest,
  BridgeResponse,
  createBridgeRequest,
  DecodeQrResponsePayload,
  PENDING_TOTP_STORAGE_KEY,
  PendingTotpSetup,
  UpdateTotpResponsePayload,
} from "../services/protocol";
import { parseOtpAuthSummary } from "../services/domain";
import { BridgeTransport } from "./transport";

export interface QrCropRect {
  x: number;
  y: number;
  width: number;
  height: number;
}

interface QrCropSession {
  tabId: number;
  windowId: number;
  itemId?: string;
  itemTitle?: string;
  origin: string;
  domain: string;
  startedAt: number;
}

const CROP_SESSION_PREFIX = "svm_qr_crop_session_";
const inMemoryCropSessions = new Map<number, QrCropSession>();

function getBgChrome(): Record<string, any> | undefined {
  return (globalThis as Record<string, any>).chrome;
}

async function saveCropSession(session: QrCropSession): Promise<void> {
  inMemoryCropSessions.set(session.tabId, session);
  const extChrome = getBgChrome();
  if (extChrome?.storage?.session?.set) {
    try {
      await extChrome.storage.session.set({
        [`${CROP_SESSION_PREFIX}${session.tabId}`]: session,
      });
    } catch {
      // Fallback sang memory map
    }
  }
}

async function loadCropSession(tabId: number): Promise<QrCropSession | null> {
  const mem = inMemoryCropSessions.get(tabId);
  if (mem) {
    return mem;
  }
  const extChrome = getBgChrome();
  const key = `${CROP_SESSION_PREFIX}${tabId}`;
  if (extChrome?.storage?.session?.get) {
    try {
      const res = await extChrome.storage.session.get([key]);
      if (res && res[key]) {
        return res[key] as QrCropSession;
      }
    } catch {
      // ignore
    }
  }
  return null;
}

export async function clearCropSession(tabId?: number): Promise<void> {
  if (typeof tabId !== "number") return;
  inMemoryCropSessions.delete(tabId);
  const extChrome = getBgChrome();
  if (extChrome?.storage?.session?.remove) {
    try {
      await extChrome.storage.session.remove([`${CROP_SESSION_PREFIX}${tabId}`]);
    } catch {
      // ignore
    }
  }
}

async function sendTabMessage(tabId: number, message: Record<string, unknown>): Promise<boolean> {
  const extChrome = getBgChrome();
  if (!extChrome?.tabs?.sendMessage) {
    return false;
  }
  return new Promise((resolve) => {
    extChrome.tabs.sendMessage(tabId, message, () => {
      resolve(!extChrome.runtime?.lastError);
    });
  });
}

async function showTabQrToast(
  tabId: number,
  toast: { ok: boolean; isInfo?: boolean; title: string; message: string; code?: string },
): Promise<void> {
  await sendTabMessage(tabId, { type: "SVM_QR_RESULT_TOAST", toast });
}

function dataUrlToBlob(dataUrl: string): Blob {
  const commaIdx = dataUrl.indexOf(",");
  if (commaIdx < 0) {
    throw new Error("Invalid capture DataURL");
  }
  const header = dataUrl.slice(0, commaIdx);
  const b64 = dataUrl.slice(commaIdx + 1);
  const mimeMatch = /^data:([^;]+)/.exec(header);
  const mime = mimeMatch ? mimeMatch[1] : "image/png";
  const binStr = atob(b64);
  const bytes = new Uint8Array(binStr.length);
  for (let i = 0; i < binStr.length; i++) {
    bytes[i] = binStr.charCodeAt(i);
  }
  return new Blob([bytes], { type: mime });
}

function uint8ToBase64(bytes: Uint8ClampedArray | Uint8Array): string {
  const chunkSize = 0x8000;
  let binary = "";
  for (let i = 0; i < bytes.length; i += chunkSize) {
    const sub = bytes.subarray(i, Math.min(i + chunkSize, bytes.length));
    binary += String.fromCharCode.apply(null, Array.from(sub));
  }
  return btoa(binary);
}

async function captureVisibleTabPng(windowId: number): Promise<string> {
  const extChrome = getBgChrome();
  if (!extChrome?.tabs?.captureVisibleTab) {
    throw new Error("Trình duyệt không hỗ trợ captureVisibleTab");
  }
  return new Promise((resolve, reject) => {
    extChrome.tabs.captureVisibleTab(windowId, { format: "png" }, (dataUrl: string) => {
      if (extChrome.runtime?.lastError || !dataUrl) {
        reject(new Error(extChrome.runtime?.lastError?.message || "Không thể chụp màn hình tab"));
      } else {
        resolve(dataUrl);
      }
    });
  });
}

async function cropPngToCanvas(
  dataUrl: string,
  rect: QrCropRect,
  dpr: number,
): Promise<OffscreenCanvas> {
  if (typeof OffscreenCanvas === "undefined" || typeof createImageBitmap !== "function") {
    throw new Error("Trình duyệt không hỗ trợ OffscreenCanvas/createImageBitmap");
  }
  const blob = dataUrlToBlob(dataUrl);
  const bmp = await createImageBitmap(blob);
  try {
    const scale = dpr > 0 ? dpr : 1;
    const sx = Math.max(0, Math.min(bmp.width - 1, Math.floor(rect.x * scale)));
    const sy = Math.max(0, Math.min(bmp.height - 1, Math.floor(rect.y * scale)));
    const sw = Math.max(1, Math.min(bmp.width - sx, Math.ceil(rect.width * scale)));
    const sh = Math.max(1, Math.min(bmp.height - sy, Math.ceil(rect.height * scale)));

    const maxSide = 560;
    const shrink = Math.max(sw, sh) > maxSide ? maxSide / Math.max(sw, sh) : 1;
    const dstW = Math.max(1, Math.round(sw * shrink));
    const dstH = Math.max(1, Math.round(sh * shrink));
    const quietPad = 16;

    const canvas = new OffscreenCanvas(dstW + quietPad * 2, dstH + quietPad * 2);
    const ctx = canvas.getContext("2d");
    if (!ctx) {
      throw new Error("Không thể khởi tạo 2D context");
    }
    ctx.fillStyle = "#ffffff";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.drawImage(bmp, sx, sy, sw, sh, quietPad, quietPad, dstW, dstH);
    return canvas;
  } finally {
    bmp.close();
  }
}

async function tryLocalBarcodeDetector(canvas: OffscreenCanvas): Promise<string | null> {
  type BgDetectorCtor = new (opts?: { formats?: string[] }) => {
    detect: (source: ImageBitmapSource) => Promise<Array<{ rawValue?: string }>>;
  };
  const ctor = (globalThis as { BarcodeDetector?: BgDetectorCtor }).BarcodeDetector;
  if (!ctor) {
    return null;
  }
  try {
    const detector = new ctor({ formats: ["qr_code"] });
    const results = await detector.detect(canvas as unknown as ImageBitmapSource);
    for (const r of results) {
      const val = r.rawValue?.trim();
      if (val) {
        return val;
      }
    }
  } catch {
    // Fallback sang Rust rqrr
  }
  return null;
}

export async function handleStartQrCrop(message: BridgeRequest<any>): Promise<BridgeResponse> {
  const extChrome = getBgChrome();
  const payload = message.payload || {};
  let tabId = typeof payload.tabId === "number" ? payload.tabId : undefined;
  let windowId = typeof payload.windowId === "number" ? payload.windowId : undefined;
  let origin = String(payload.origin || "");
  let domain = String(payload.domain || "");

  if (tabId === undefined && extChrome?.tabs?.query) {
    const tabs = await new Promise<Array<{ id?: number; windowId?: number; url?: string }>>(
      (resolve) => {
        extChrome.tabs.query({ active: true, currentWindow: true }, (res: any[]) =>
          resolve(res || []),
        );
      },
    );
    const active = tabs[0];
    tabId = active?.id;
    windowId = active?.windowId;
    if (!origin && active?.url) {
      origin = active.url;
    }
  }

  if (typeof tabId !== "number" || typeof windowId !== "number") {
    return {
      requestId: message.requestId,
      type: "error",
      ok: false,
      error: { code: "NO_ACTIVE_TAB", message: "Không xác định được tab trình duyệt hiện tại." },
    };
  }

  if (origin && !/^https?:\/\//i.test(origin)) {
    return {
      requestId: message.requestId,
      type: "error",
      ok: false,
      error: {
        code: "UNSUPPORTED_TAB",
        message: "Chỉ hỗ trợ quét mã QR trên các trang web http:// hoặc https://.",
      },
    };
  }

  const session: QrCropSession = {
    tabId,
    windowId,
    itemId: payload.itemId ? String(payload.itemId) : undefined,
    itemTitle: payload.itemTitle ? String(payload.itemTitle) : undefined,
    origin,
    domain,
    startedAt: Date.now(),
  };
  await saveCropSession(session);

  let sent = await sendTabMessage(tabId, {
    type: "SVM_START_QR_CROP",
    itemTitle: session.itemTitle || "",
  });

  if (!sent && extChrome?.scripting?.executeScript) {
    try {
      await extChrome.scripting.executeScript({
        target: { tabId },
        files: ["content.js"],
      });
      sent = await sendTabMessage(tabId, {
        type: "SVM_START_QR_CROP",
        itemTitle: session.itemTitle || "",
      });
    } catch {
      sent = false;
    }
  }

  if (!sent) {
    await clearCropSession(tabId);
    return {
      requestId: message.requestId,
      type: "error",
      ok: false,
      error: {
        code: "CONTENT_UNAVAILABLE",
        message: "Không thể bật khung quét trên trang này. Hãy tải lại trang rồi thử lại.",
      },
    };
  }

  return {
    requestId: message.requestId,
    type: "qr_crop_started",
    ok: true,
    payload: { started: true },
  };
}

export async function handleQrCropSelected(
  senderTab: { id?: number; windowId?: number; url?: string } | undefined,
  message: BridgeRequest<any>,
  transport: BridgeTransport,
): Promise<BridgeResponse> {
  const tabId = senderTab?.id;
  if (typeof tabId !== "number") {
    return {
      requestId: message.requestId,
      type: "error",
      ok: false,
      error: { code: "NO_TAB", message: "Missing sender tabId" },
    };
  }

  const session = await loadCropSession(tabId);
  await clearCropSession(tabId);

  const windowId = session?.windowId ?? senderTab?.windowId;
  const rect = message.payload?.rect as QrCropRect | undefined;
  const dpr = Number(message.payload?.devicePixelRatio || 1);

  if (typeof windowId !== "number" || !rect || rect.width < 12 || rect.height < 12) {
    await showTabQrToast(tabId, {
      ok: false,
      title: "Vùng chọn không hợp lệ",
      message: "Vui lòng kéo khung bao quanh toàn bộ mã QR.",
    });
    return {
      requestId: message.requestId,
      type: "error",
      ok: false,
      error: { code: "INVALID_RECT", message: "Invalid crop rectangle" },
    };
  }

  try {
    const dataUrl = await captureVisibleTabPng(windowId);
    const canvas = await cropPngToCanvas(dataUrl, rect, dpr);

    let qrText = await tryLocalBarcodeDetector(canvas);
    let issuer = "";
    let account = "";

    if (!qrText) {
      const ctx = canvas.getContext("2d");
      if (!ctx) {
        throw new Error("Không thể đọc pixel từ OffscreenCanvas");
      }
      const imgData = ctx.getImageData(0, 0, canvas.width, canvas.height);
      const rgba = imgData.data;
      const pxCount = canvas.width * canvas.height;
      const luma = new Uint8Array(pxCount);
      for (let i = 0; i < pxCount; i++) {
        const off = i * 4;
        const lum = (299 * rgba[off] + 587 * rgba[off + 1] + 114 * rgba[off + 2]) / 1000;
        const a = rgba[off + 3];
        luma[i] = Math.round((lum * a + 255 * (255 - a)) / 255);
      }
      const lumaBase64 = uint8ToBase64(luma);
      const decResp = await transport.dispatch<DecodeQrResponsePayload>(
        createBridgeRequest("decode_qr", {
          lumaBase64,
          width: canvas.width,
          height: canvas.height,
        }),
      );
      if (!decResp.ok || !decResp.payload?.text) {
        await showTabQrToast(tabId, {
          ok: false,
          title: "Không thấy mã QR",
          message:
            decResp.error?.message ||
            "Không nhận diện được mã QR. Hãy kéo rộng hơn một chút bao quanh viền trắng mã QR.",
        });
        return {
          requestId: message.requestId,
          type: "error",
          ok: false,
          error: decResp.error || { code: "QR_NOT_FOUND", message: "QR not found" },
        };
      }
      qrText = decResp.payload.text;
      issuer = decResp.payload.otpauth?.issuer || "";
      account = decResp.payload.otpauth?.account || "";
    }

    const trimmed = qrText.trim();
    const lower = trimmed.toLowerCase();
    if (lower.startsWith("fido:/")) {
      await showTabQrToast(tabId, {
        ok: false,
        title: "Không phải mã QR TOTP",
        message:
          "Đây là mã QR Passkey (FIDO) của trình duyệt, không phải mã thiết lập 2FA otpauth://.",
      });
      return {
        requestId: message.requestId,
        type: "error",
        ok: false,
        error: { code: "PASSKEY_QR", message: "FIDO Passkey QR is not TOTP" },
      };
    }

    if (!lower.startsWith("otpauth://")) {
      await showTabQrToast(tabId, {
        ok: false,
        title: "Định dạng QR không hỗ trợ",
        message: "Chỉ hỗ trợ mã QR thiết lập 2FA chuẩn otpauth://.",
      });
      return {
        requestId: message.requestId,
        type: "error",
        ok: false,
        error: { code: "INVALID_OTPAUTH", message: "Not an otpauth:// QR" },
      };
    }

    const summary = parseOtpAuthSummary(trimmed);
    issuer = issuer || summary?.issuer || "";
    account = account || summary?.account || "";

    if (session?.itemId) {
      const updResp = await transport.dispatch<UpdateTotpResponsePayload>(
        createBridgeRequest("update_totp", {
          itemId: session.itemId,
          uri: trimmed,
        }),
      );
      if (!updResp.ok || !updResp.payload) {
        await showTabQrToast(tabId, {
          ok: false,
          title: "Lưu mã 2FA thất bại",
          message: updResp.error?.message || "Không thể lưu bí mật TOTP vào Desktop Vault.",
        });
        return updResp;
      }

      const targetName = session.itemTitle || updResp.payload.issuer || "tài khoản";
      await showTabQrToast(tabId, {
        ok: true,
        title: `Đã thêm 2FA cho ${targetName}`,
        message: "Mã 6 số hiện tại đã được sao chép vào Clipboard.",
        code: updResp.payload.code,
      });
      return updResp;
    }

    // Chế độ quét chung (chưa chọn item): lưu tạm vào chrome.storage.local để Popup gắn sau
    const extChrome = getBgChrome();
    let domain = session?.domain || "";
    const origin = session?.origin || senderTab?.url || "";
    if (!domain && origin) {
      try {
        domain = new URL(origin).hostname.replace(/^www\./, "").toLowerCase();
      } catch {
        domain = "";
      }
    }
    const pending: PendingTotpSetup = {
      uri: trimmed,
      issuer,
      account,
      origin,
      domain,
      createdAt: Date.now(),
    };
    if (extChrome?.storage?.session?.set) {
      await extChrome.storage.session.set({ [PENDING_TOTP_STORAGE_KEY]: pending });
    } else if (extChrome?.storage?.local?.set) {
      await extChrome.storage.local.set({ [PENDING_TOTP_STORAGE_KEY]: pending });
    }

    const labelText = [issuer, account].filter(Boolean).join(" • ") || "otpauth://";
    await showTabQrToast(tabId, {
      ok: false,
      isInfo: true,
      title: "Đã quét mã QR 2FA",
      message: `Đã nhận diện ${labelText}. Hãy bấm mở lại tiện ích Secure Vault để chọn hoặc tạo tài khoản lưu vào.`,
    });

    return {
      requestId: message.requestId,
      type: "qr_pending_saved",
      ok: true,
      payload: { pending: true, issuer, account },
    };
  } catch (err) {
    const errMsg = err instanceof Error ? err.message : "Lỗi xử lý ảnh QR";
    await showTabQrToast(tabId, {
      ok: false,
      title: "Quét QR 2FA thất bại",
      message: errMsg,
    });
    return {
      requestId: message.requestId,
      type: "error",
      ok: false,
      error: { code: "CROP_ERROR", message: errMsg },
    };
  }
}
