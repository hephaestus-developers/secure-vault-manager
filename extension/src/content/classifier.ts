/**
 * @file extension/src/content/classifier.ts
 * @brief Bộ phân loại trường nhập liệu DOM (Heuristic Form Classifier) nhận diện Username, Password và mã xác thực 2 bước.
 */

export interface ClassifiedFormFields {
  usernameInput: HTMLInputElement | null;
  passwordInput: HTMLInputElement | null;
  totpInput: HTMLInputElement | null;
}

const USERNAME_PATTERNS = /user|email|login|account|identifier|id/i;
const TOTP_PATTERNS = /totp|otp|2fa|mfa|verification|token|one-time-code/i;

/**
 * Tìm kiếm và phân loại các trường đăng nhập trên trang hiện tại.
 */
export function classifyDocumentFields(root: Document | Element = document): ClassifiedFormFields {
  const inputs = Array.from(root.querySelectorAll("input")) as HTMLInputElement[];
  let usernameInput: HTMLInputElement | null = null;
  let passwordInput: HTMLInputElement | null = null;
  let totpInput: HTMLInputElement | null = null;

  for (const input of inputs) {
    if (input.disabled || input.readOnly || input.type === "hidden") {
      continue;
    }

    const type = (input.type || "text").toLowerCase();
    const autocomplete = (input.getAttribute("autocomplete") || "").toLowerCase();
    const signature = `${input.name} ${input.id} ${input.placeholder} ${input.getAttribute("aria-label") || ""}`;

    if (type === "password" && !passwordInput) {
      passwordInput = input;
      continue;
    }

    if (
      autocomplete === "one-time-code" ||
      TOTP_PATTERNS.test(signature) ||
      (input.maxLength === 6 && type === "tel")
    ) {
      if (!totpInput) {
        totpInput = input;
        continue;
      }
    }

    if (
      type === "email" ||
      autocomplete === "username" ||
      autocomplete === "email" ||
      (type === "text" && USERNAME_PATTERNS.test(signature))
    ) {
      if (!usernameInput) {
        usernameInput = input;
      }
    }
  }

  return { usernameInput, passwordInput, totpInput };
}
