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
  const links = root.querySelectorAll("a[href^='otpauth://'], [data-otpauth], [title*='otpauth://']");
  for (const el of Array.from(links)) {
    const raw =
      el.getAttribute("href") ||
      el.getAttribute("data-otpauth") ||
      el.getAttribute("title") ||
      "";
    if (raw.startsWith("otpauth://")) {
      return parseOtpAuthSummary(raw);
    }
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
    const issuer = url.searchParams.get("issuer") || (parts.length > 1 ? parts[0].trim() : "Account");
    const account = parts.length > 1 ? parts.slice(1).join(":").trim() : label;
    return { uri, issuer, account };
  } catch {
    return null;
  }
}
