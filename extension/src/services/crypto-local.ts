/**
 * @file extension/src/services/crypto-local.ts
 * @brief Tiện ích mật mã học cục bộ sử dụng WebCrypto API (CSPRNG) tiêu chuẩn của trình duyệt.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

import { PasswordGeneratorOptions, PasswordStrengthResult } from "./domain";

/**
 * Sinh mật khẩu mạnh cục bộ sử dụng WebCrypto CSPRNG (crypto.getRandomValues).
 */
export function generatePasswordLocal(
  options: PasswordGeneratorOptions
): { password: string; strength: PasswordStrengthResult } {
  let charset = "";
  if (options.lowercase) charset += "abcdefghijkmnopqrstuvwxyz";
  if (options.uppercase) charset += "ABCDEFGHJKLMNPQRSTUVWXYZ";
  if (options.digits) charset += "23456789";
  if (options.symbols) charset += "!@#$%^&*()-_=+[]{}?";
  if (!charset) charset = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";

  const length = Math.max(8, Math.min(128, options.length || 20));
  const rnd = new Uint8Array(length);
  crypto.getRandomValues(rnd);

  let password = "";
  for (let i = 0; i < rnd.length; i++) {
    password += charset[rnd[i] % charset.length];
  }

  const score = length >= 20 ? 4 : length >= 14 ? 3 : 2;
  const percent = Math.min(100, Math.round((length / 24) * 100));

  return {
    password,
    strength: {
      score,
      percent,
      guesses: Math.pow(charset.length, length),
      feedback: length < 12 ? "Mật khẩu nên có tối thiểu 12 ký tự" : "",
    },
  };
}
