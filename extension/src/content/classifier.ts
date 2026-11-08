/**
 * @file extension/src/content/classifier.ts
 * @brief Bộ phân loại trường nhập liệu DOM (Heuristic Form Classifier) nhận diện Username, Password và mã xác thực 2 bước.
 */

export interface ClassifiedFormFields {
  usernameInput: HTMLInputElement | null;
  passwordInput: HTMLInputElement | null;
  totpInput: HTMLInputElement | null;
}

const USERNAME_PATTERNS = /user|email|login|account|identifier|^id$|\bid\b/i;
const TOTP_PATTERNS =
  /totp|otp|2fa|mfa|otc|pin|passcode|one[-_ ]?time|two[-_ ]?factor|2[-_ ]?factor|verification|verify|security[-_ ]?code|auth[-_ ]?code|authenticator|challenge|^x{6,8}$/i;

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
    const inputmode = (input.getAttribute("inputmode") || "").toLowerCase();
    const placeholder = (input.placeholder || "").trim();
    const signature = `${input.name} ${input.id} ${placeholder} ${input.className} ${input.getAttribute("aria-label") || ""}`;

    if (type === "password" && !passwordInput) {
      passwordInput = input;
      continue;
    }

    const isTotpCandidate =
      autocomplete === "one-time-code" ||
      TOTP_PATTERNS.test(signature) ||
      /^x{6,8}$/i.test(placeholder) ||
      /^[•·*]{6,8}$/.test(placeholder) ||
      (input.maxLength === 6 && (type === "tel" || type === "number" || type === "text")) ||
      (inputmode === "numeric" && input.maxLength >= 6 && input.maxLength <= 8);

    if (isTotpCandidate) {
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

  // Heuristic dự phòng: Nếu không tìm thấy totpInput nhưng trang là màn hình Two-Factor Authentication
  // (ví dụ GitHub "Two-factor authentication", có tiêu đề/URL chứa 2fa và chỉ có 1 ô input văn bản duy nhất)
  if (!totpInput && !passwordInput) {
    const pageText = `${document.title} ${window.location.href} ${document.body?.innerText?.slice(0, 800) || ""}`;
    const is2FaPage = /two-factor|2fa|mfa|verification|verify|authenticator|checkpoint/i.test(pageText);
    const visibleTextInputs = inputs.filter(
      (inp) =>
        !inp.disabled &&
        !inp.readOnly &&
        inp.type !== "hidden" &&
        inp.type !== "submit" &&
        inp.type !== "button"
    );
    if (is2FaPage && visibleTextInputs.length === 1) {
      totpInput = visibleTextInputs[0];
      if (usernameInput === totpInput) {
        usernameInput = null;
      }
    }
  }

  return { usernameInput, passwordInput, totpInput };
}
