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

  if (descriptor && descriptor.set) {
    descriptor.set.call(input, value);
  } else {
    input.value = value;
  }

  input.dispatchEvent(new Event("input", { bubbles: true }));
  input.dispatchEvent(new Event("change", { bubbles: true }));
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
  if (fields.totpInput && data.totpCode) {
    setNativeInputValue(fields.totpInput, data.totpCode);
    filled = true;
  } else if (data.totpCode && !fields.passwordInput) {
    // Fallback khi ở màn hình 2FA chuyên biệt: nếu chưa map được totpInput qua classifier,
    // ưu tiên điền vào ô input đang được focus (activeElement) hoặc ô input hiển thị duy nhất.
    const active = document.activeElement;
    if (
      active instanceof HTMLInputElement &&
      active.type !== "hidden" &&
      active.type !== "submit" &&
      active.type !== "button"
    ) {
      setNativeInputValue(active, data.totpCode);
      filled = true;
    } else {
      const inputs = Array.from(document.querySelectorAll("input")).filter(
        (inp) =>
          !inp.disabled &&
          !inp.readOnly &&
          inp.type !== "hidden" &&
          inp.type !== "submit" &&
          inp.type !== "button"
      ) as HTMLInputElement[];
      if (inputs.length === 1) {
        setNativeInputValue(inputs[0], data.totpCode);
        filled = true;
      }
    }
  }

  if (data.username && data.password) {
    recordAutofilledCredentials(data.username, data.password);
  }

  return filled;
}
