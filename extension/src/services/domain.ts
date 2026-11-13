/**
 * @file extension/src/services/domain.ts
 * @brief Định nghĩa kiểu dữ liệu miền (Domain Schemas) cho Browser Extension.
 */

export type Category =
  | "login"
  | "card"
  | "note"
  | "identity"
  | "ssh_key"
  | "api_credential"
  | "database"
  | "wifi"
  | "software_license"
  | "passkey";

export type FieldKind = "text" | "concealed" | "url" | "totp" | "date" | "boolean";

export interface CustomField {
  id: string;
  label: string;
  value: string;
  kind: FieldKind;
}

export interface VaultProfile {
  id: string;
  name: string;
  iconName: string;
  description: string;
  itemCount: number;
  isDefault: boolean;
  createdAt: number;
}

export interface VaultItem {
  id: string;
  vaultId: string;
  title: string;
  category: Category;
  username: string;
  password: string;
  websiteUrl: string;
  notes: string;
  favorite: boolean;
  trashed: boolean;
  hasTotp: boolean;
  totpSecret: string;
  passkeyCredentialId?: string;
  passkeyRpId?: string;
  passkeyAlgorithm?: number;
  passkeyUserHandle?: string;
  passkeySignCount?: number;
  cardNumber?: string;
  cardExpiry?: string;
  cardCvv?: string;
  customFields: CustomField[];
  tags: string[];
  passwordStrengthScore: number;
  createdAt: number;
  updatedAt: number;
}

export interface ItemRevision {
  revision: number;
  timestamp: number;
  snapshot: VaultItem;
}

export interface TrashItem {
  itemId: string;
  trashedAt: number;
}

export interface AttachmentMeta {
  id: string;
  itemId: string;
  fileName: string;
  sizeBytes: number;
  createdAt: number;
}

export interface PasswordGeneratorOptions {
  length: number;
  uppercase: boolean;
  lowercase: boolean;
  digits: boolean;
  symbols: boolean;
}

export interface PasswordStrengthResult {
  score: number;
  percent: number;
  guesses: number;
  feedback: string;
}

export interface TotpSnapshot {
  code: string;
  remaining: number;
  issuer?: string;
  account?: string;
}

export interface ScannedOtpAuth {
  uri: string;
  issuer: string;
  account: string;
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

const MULTI_LABEL_PUBLIC_SUFFIXES = new Set([
  "co.uk",
  "org.uk",
  "ac.uk",
  "gov.uk",
  "com.vn",
  "net.vn",
  "org.vn",
  "edu.vn",
  "gov.vn",
  "com.au",
  "net.au",
  "org.au",
  "co.jp",
  "ne.jp",
  "or.jp",
  "com.br",
  "com.cn",
  "com.sg",
  "com.tw",
  "com.hk",
  "co.kr",
  "co.in",
  "co.nz",
  "co.za",
  "github.io",
  "gitlab.io",
  "pages.dev",
  "vercel.app",
  "netlify.app",
  "web.app",
  "firebaseapp.com",
]);

/**
 * Trích xuất hostname đã chuẩn hóa (chữ thường, bỏ cổng/đường dẫn và dấu chấm cuối) từ URL hoặc host.
 */
export function extractNormalizedHostname(urlOrHost: string): string {
  const raw = urlOrHost.trim().toLowerCase();
  if (!raw) return "";
  try {
    const withScheme = raw.includes("://") ? raw : `https://${raw}`;
    return new URL(withScheme).hostname.toLowerCase().replace(/\.+$/, "");
  } catch {
    const noScheme = raw.split("://").pop() || raw;
    const hostPort = noScheme.split(/[/?#]/)[0] || "";
    return (hostPort.split(":")[0] || "").replace(/\.+$/, "");
  }
}

/**
 * Kiểm tra xem URL/host lưu trong Vault có khớp an toàn với hostname hiện tại hay không.
 */
export function isDomainMatch(savedUrlOrHost: string, currentHost: string): boolean {
  const sh = extractNormalizedHostname(savedUrlOrHost).replace(/^www\./, "");
  const th = extractNormalizedHostname(currentHost).replace(/^www\./, "");
  if (!sh || !th) return false;
  if (sh === th) return true;
  if (sh.includes(".") && !sh.startsWith(".") && th.endsWith(`.${sh}`)) {
    return true;
  }
  if (th.includes(".") && !th.startsWith(".") && sh.endsWith(`.${th}`)) {
    return true;
  }
  return false;
}

/**
 * Xác thực Relying Party ID WebAuthn hợp lệ cho hostname hiện tại (hỗ trợ localhost và chặn Public Suffix).
 */
export function isValidWebAuthnRpId(currentHostname: string, requestedRpId: string): boolean {
  const host = extractNormalizedHostname(currentHostname);
  const rp = extractNormalizedHostname(requestedRpId);
  if (!host || !rp) return false;
  if (rp === "localhost") {
    return host === "localhost";
  }
  if (!rp.includes(".") || rp.startsWith(".")) {
    return false;
  }
  if (host !== rp && MULTI_LABEL_PUBLIC_SUFFIXES.has(rp)) {
    return false;
  }
  return host === rp || host.endsWith(`.${rp}`);
}
