/**
 * @file extension/src/content/autofill.ts
 * @brief Bộ điền tự động thông tin đăng nhập tương thích với React, Vue, Angular (Native Value Setter + InputEvent).
 */

import { classifyDocumentFields } from "./classifier";

let lastAutofilled: {
  username: string;
  password: string;
  domain: string;
  timestamp: number;
} | null = null;

export function recordAutofilledCredentials(username: string, password: string): void {
  lastAutofilled = {
    username: username.trim(),
    password,
    domain: window.location.hostname.toLowerCase(),
    timestamp: Date.now(),
  };
}

export function isRecentlyAutofilled(username: string, password: string): boolean {
  if (!lastAutofilled) return false;
  const sameUser = lastAutofilled.username.toLowerCase() === username.trim().toLowerCase();
  const samePass = lastAutofilled.password === password;
  const sameDomain = lastAutofilled.domain === window.location.hostname.toLowerCase();
  const isRecent = Date.now() - lastAutofilled.timestamp < 1000 * 60 * 5; // Trong vòng 5 phút
  return sameUser && samePass && sameDomain && isRecent;
}

/**
 * Đặt giá trị cho một phần tử `<input>` và phát chuỗi sự kiện chuẩn để kích hoạt state của các framework SPA.
 */
export function setNativeInputValue(input: HTMLInputElement, value: string): void {
  const prototype = Object.getPrototypeOf(input);
  const descriptor =
    Object.getOwnPropertyDescriptor(prototype, "value") ||
    Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, "value");

  if (descriptor?.set) {
    descriptor.set.call(input, value);
  } else {
    input.value = value;
  }

  input.dispatchEvent(new Event("input", { bubbles: true }));
  input.dispatchEvent(new Event("change", { bubbles: true }));
}

/**
 * Tìm kiếm phần tử `<input>` khả dụng cho mã xác thực 2 bước (TOTP) ở các màn hình 2FA độc lập.
 */
function findStandaloneTotpInput(doc: Document): HTMLInputElement | null {
  const active = doc.activeElement;
  if (
    active instanceof HTMLInputElement &&
    !active.disabled &&
    !active.readOnly &&
    active.type !== "hidden" &&
    active.type !== "submit" &&
    active.type !== "button"
  ) {
    return active;
  }

  const inputs = Array.from(doc.querySelectorAll("input")).filter(
    (inp) =>
      !inp.disabled &&
      !inp.readOnly &&
      inp.type !== "hidden" &&
      inp.type !== "submit" &&
      inp.type !== "button",
  ) as HTMLInputElement[];

  return inputs.length === 1 ? inputs[0] : null;
}

/**
 * Điền mã xác thực 2FA/TOTP vào 1 ô đơn hoặc tự động tách từng ký tự vào nhóm ô OTP (4-8 ô).
 */
export function fillTotpCode(
  targets: HTMLInputElement | HTMLInputElement[],
  code: string,
): boolean {
  const inputs = Array.isArray(targets) ? targets : [targets];
  const cleanCode = code.replace(/\s+/g, "").trim();
  if (inputs.length === 0 || !cleanCode) {
    return false;
  }

  if (inputs.length >= 4 && inputs.length <= 8) {
    const count = Math.min(inputs.length, cleanCode.length);
    for (let i = 0; i < count; i++) {
      const box = inputs[i];
      const ch = cleanCode[i];
      box.focus();
      setNativeInputValue(box, ch);
      box.dispatchEvent(new KeyboardEvent("keyup", { key: ch, bubbles: true }));
    }
    return count > 0;
  }

  setNativeInputValue(inputs[0], cleanCode);
  return true;
}

/**
 * Điền thông tin tài khoản, mật khẩu và mã xác thực 2 bước (nếu có) vào biểu mẫu hiện tại.
 */
export function autofillCredentialsOnPage(data: {
  username?: string;
  password?: string;
  totpCode?: string;
}): boolean {
  const fields = classifyDocumentFields(document);
  let filled = false;

  if (fields.usernameInput && data.username) {
    setNativeInputValue(fields.usernameInput, data.username);
    filled = true;
  }
  if (fields.passwordInput && data.password) {
    setNativeInputValue(fields.passwordInput, data.password);
    filled = true;
  }
  if (data.totpCode) {
    if (fields.totpInputs.length > 0) {
      filled = fillTotpCode(fields.totpInputs, data.totpCode) || filled;
    } else if (!fields.passwordInput) {
      // Fallback khi ở màn hình 2FA chuyên biệt
      const standaloneInput = findStandaloneTotpInput(document);
      if (standaloneInput) {
        filled = fillTotpCode(standaloneInput, data.totpCode) || filled;
      }
    }
  }

  if (data.username && data.password) {
    recordAutofilledCredentials(data.username, data.password);
  }

  return filled;
}
