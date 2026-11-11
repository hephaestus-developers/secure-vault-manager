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
