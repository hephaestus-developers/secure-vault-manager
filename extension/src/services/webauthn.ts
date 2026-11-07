/**
 * @file extension/src/services/webauthn.ts
 * @brief Tiện ích và cấu trúc dữ liệu WebAuthn / FIDO2 Passkey dùng cho trình duyệt và cầu nối.
 */

export interface PasskeyCreateRequest {
  rpId: string;
  rpName: string;
  userId: string;
  userName: string;
  userDisplayName: string;
  challenge: string;
  origin: string;
}

export interface PasskeyCreateResponse {
  itemId: string;
  credentialId: string;
  clientDataJSON: string;
  attestationObject: string;
}

export interface PasskeyGetRequest {
  rpId: string;
  challenge: string;
  origin: string;
  allowCredentialIds?: string[];
}

export interface PasskeyGetResponse {
  credentialId: string;
  rpId: string;
  authenticatorData: string;
  clientDataJSON: string;
  signature: string;
  userHandle?: string;
}

/**
 * Mã hóa mảng byte sang chuỗi Base64URL chuẩn WebAuthn (không có dấu `=`).
 */
export function bytesToBase64Url(bytes: Uint8Array): string {
  let binary = "";
  for (let i = 0; i < bytes.length; i++) {
    binary += String.fromCharCode(bytes[i]);
  }
  return btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/g, "");
}

/**
 * Giải mã chuỗi Base64 hoặc Base64URL thành `Uint8Array`.
 */
export function base64UrlToBytes(base64Url: string): Uint8Array {
  const normalized = base64Url.replace(/-/g, "+").replace(/_/g, "/");
  const padLen = (4 - (normalized.length % 4)) % 4;
  const padded = normalized + "=".repeat(padLen);
  const binary = atob(padded);
  const out = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) {
    out[i] = binary.charCodeAt(i);
  }
  return out;
}

/**
 * Trích xuất Relying Party ID (hostname) từ một URL đầy đủ.
 */
export function extractRpIdFromUrl(rawUrl: string): string {
  try {
    const parsed = new URL(rawUrl);
    return parsed.hostname.toLowerCase();
  } catch {
    return rawUrl.replace(/^https?:\/\//i, "").split("/")[0].toLowerCase();
  }
}
