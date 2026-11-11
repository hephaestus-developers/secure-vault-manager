/**
 * @file extension/src/services/qr-scanner.ts
 * @brief Dịch vụ trích xuất URI `otpauth://totp/...` từ trang web hiện tại để thiết lập mã xác thực 2 bước nhanh.
 */

export interface ScannedOtpAuth {
  uri: string;
  issuer: string;
  account: string;
}

/**
 * Quét DOM của trang hiện tại để tìm các liên kết hoặc thuộc tính chứa `otpauth://totp/`.
 */
export function scanDocumentForOtpAuth(root: Document | Element = document): ScannedOtpAuth | null {
  const links = root.querySelectorAll(
    "a[href^='otpauth://'], [data-otpauth], [title*='otpauth://']",
  );
  for (const el of Array.from(links)) {
    const raw =
      el.getAttribute("href") || el.getAttribute("data-otpauth") || el.getAttribute("title") || "";
    if (raw.startsWith("otpauth://")) {
      return parseOtpAuthSummary(raw);
    }
  }
  return null;
}

/**
 * Quét DOM (bao gồm liên kết `otpauth://` và ảnh/canvas chứa QR code nếu trình duyệt hỗ trợ BarcodeDetector).
 */
export async function scanDocumentImagesForOtpAuth(
  root: Document | Element = document,
): Promise<ScannedOtpAuth | null> {
  const inline = scanDocumentForOtpAuth(root);
  if (inline) {
    return inline;
  }
  type BarcodeDetectorCtor = new (opts?: { formats?: string[] }) => {
    detect: (source: ImageBitmapSource) => Promise<Array<{ rawValue?: string }>>;
  };
  const detectorCtor = (globalThis as { BarcodeDetector?: BarcodeDetectorCtor }).BarcodeDetector;
  if (!detectorCtor) {
    return null;
  }
  try {
    const detector = new detectorCtor({ formats: ["qr_code"] });
    const candidates = Array.from(root.querySelectorAll("img, canvas")) as Array<
      HTMLImageElement | HTMLCanvasElement
    >;
    for (const el of candidates) {
      if (el instanceof HTMLImageElement && (!el.complete || el.naturalWidth === 0)) {
        continue;
      }
      if (el.width < 32 || el.height < 32) {
        continue;
      }
      const barcodes = await detector.detect(el);
      for (const code of barcodes) {
        const raw = code.rawValue?.trim() ?? "";
        if (raw.startsWith("otpauth://")) {
          return parseOtpAuthSummary(raw);
        }
      }
    }
  } catch {
    // Trình duyệt hoặc phần tử canvas bị taint (cross-origin)
  }
  return null;
}

/**
 * Phân tích nhanh thông tin hiển thị từ chuỗi `otpauth://`.
 */
export function parseOtpAuthSummary(uri: string): ScannedOtpAuth | null {
  if (!uri.startsWith("otpauth://")) {
    return null;
  }
  try {
    const url = new URL(uri);
    const label = decodeURIComponent(url.pathname.replace(/^\/+/, ""));
    const parts = label.split(":");
    const issuer =
      url.searchParams.get("issuer") || (parts.length > 1 ? parts[0].trim() : "Account");
    const account = parts.length > 1 ? parts.slice(1).join(":").trim() : label;
    return { uri, issuer, account };
  } catch {
    return null;
  }
}
