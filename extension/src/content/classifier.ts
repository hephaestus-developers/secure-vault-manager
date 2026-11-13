/**
 * @file extension/src/content/classifier.ts
 * @brief Bộ phân loại trường nhập liệu DOM (Heuristic Form Classifier) nhận diện Username, Password và mã xác thực 2 bước.
 */

export interface ClassifiedFormFields {
  usernameInput: HTMLInputElement | null;
  passwordInput: HTMLInputElement | null;
  newPasswordInput: HTMLInputElement | null;
  totpInput: HTMLInputElement | null;
}

const USERNAME_PATTERNS =
  /\b(user(name)?|e[-_]?mail|login([-_]?id)?|account([-_ ]?(name|id))?|identifier|user[-_ ]?id|member[-_ ]?id)\b/i;
const NEW_PASSWORD_PATTERNS =
  /\b(new[-_ ]?pass(word)?|create[-_ ]?pass(word)?)\b|mật[-_ ]?khẩu[-_ ]?mới/i;
const CONFIRM_PASSWORD_PATTERNS =
  /\b(confirm|repeat|retype|verify[-_ ]?pass(word)?|again)\b|xác[-_ ]?nhận|nhập[-_ ]?lại/i;
const LOGIN_CONTEXT_PATTERNS =
  /\b(sign[-_ ]?in|log[-_ ]?in|auth(enticate)?|sso|account)\b|đăng[-_ ]?nhập/i;
const TOTP_PATTERNS =
  /totp|otp|2fa|mfa|otc|pin|passcode|one[-_ ]?time|two[-_ ]?factor|2[-_ ]?factor|verification|verify|security[-_ ]?code|auth[-_ ]?code|authenticator|challenge|^x{6,8}$/i;

const SUBMIT_PATTERNS =
  /sign[-_ ]?in|log[-_ ]?in|đăng[-_ ]?nhập|submit|continue|tiếp[-_ ]?tục|authenticate|xác[-_ ]?thực/i;

const SEARCH_INPUT_PATTERNS =
  /\b(search|filter|query|find|lookup|keyword)\b|tìm[-_ ]?kiếm|bộ[-_ ]?lọc/i;

const NON_LOGIN_BUTTON_PATTERNS =
  /\b(search|filter|find|clear|reset|cancel|close|delete|remove|copy|edit|back)\b|tìm[-_ ]?kiếm|lọc|hủy|đóng|xóa|quay[-_ ]?lại/i;

/**
 * Kiểm tra xem ô nhập liệu có phải là ô tìm kiếm hoặc bộ lọc không (tránh nhầm lẫn với username/TOTP).
 */
export function isSearchOrFilterInput(input: HTMLInputElement): boolean {
  const type = (input.type || "text").toLowerCase();
  const role = (input.getAttribute("role") || "").toLowerCase();
  if (type === "search" || role === "searchbox") {
    return true;
  }
  if (input.closest("[role='search'], form[role='search']") !== null) {
    return true;
  }
  const placeholder = (input.placeholder || "").trim();
  const signature = `${input.name} ${input.id} ${placeholder} ${input.className} ${input.getAttribute("aria-label") || ""}`;
  return SEARCH_INPUT_PATTERNS.test(signature);
}

/**
 * Kiểm tra phần tử nhập liệu có đang hiển thị trên giao diện hay không (tránh điền vào honey-pot hoặc field ẩn).
 */
export function isElementVisible(el: HTMLElement): boolean {
  if (!el.isConnected) {
    return false;
  }
  const style = window.getComputedStyle(el);
  if (
    style.display === "none" ||
    style.visibility === "hidden" ||
    Number.parseFloat(style.opacity || "1") <= 0
  ) {
    return false;
  }
  const rect = el.getBoundingClientRect();
  return rect.width > 0 && rect.height > 0;
}

/**
 * Kiểm tra ngữ cảnh biểu mẫu đăng nhập khi trang chưa hiển thị ô mật khẩu (ví dụ bước 1 của đăng nhập 2 bước).
 */
function hasLoginContext(input: HTMLInputElement, root: Document | Element): boolean {
  const scope = input.closest("form") || root;
  if (findProbableSubmitButton(scope) !== null) {
    return true;
  }
  const formEl = input.closest("form");
  const formText = formEl
    ? `${formEl.id} ${formEl.className} ${formEl.getAttribute("action") || ""} ${formEl.textContent?.slice(0, 240) || ""}`
    : "";
  const pageContext = `${document.title} ${window.location.pathname} ${formText}`;
  return LOGIN_CONTEXT_PATTERNS.test(pageContext);
}

/**
 * Tìm kiếm và phân loại các trường đăng nhập trên trang hiện tại.
 */
export function classifyDocumentFields(root: Document | Element = document): ClassifiedFormFields {
  const inputs = Array.from(root.querySelectorAll("input")) as HTMLInputElement[];
  let usernameInput: HTMLInputElement | null = null;
  let passwordInput: HTMLInputElement | null = null;
  let newPasswordInput: HTMLInputElement | null = null;
  let totpInput: HTMLInputElement | null = null;

  for (const input of inputs) {
    if (input.disabled || input.readOnly || input.type === "hidden" || !isElementVisible(input)) {
      continue;
    }

    const type = (input.type || "text").toLowerCase();
    const autocomplete = (input.getAttribute("autocomplete") || "").toLowerCase();
    const inputmode = (input.getAttribute("inputmode") || "").toLowerCase();
    const placeholder = (input.placeholder || "").trim();
    const signature = `${input.name} ${input.id} ${placeholder} ${input.className} ${input.getAttribute("aria-label") || ""}`;

    if (type === "password") {
      if (!passwordInput || autocomplete === "current-password") {
        passwordInput = input;
      }
      if (
        !newPasswordInput &&
        !CONFIRM_PASSWORD_PATTERNS.test(signature) &&
        (autocomplete === "new-password" || NEW_PASSWORD_PATTERNS.test(signature))
      ) {
        newPasswordInput = input;
      }
      continue;
    }

    if (isSearchOrFilterInput(input)) {
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

  // Nếu không có ô mật khẩu trên trang, chỉ giữ usernameInput khi có tín hiệu rõ ràng (email / autocomplete / form đăng nhập)
  if (!passwordInput && usernameInput) {
    const uType = (usernameInput.type || "text").toLowerCase();
    const uAuto = (usernameInput.getAttribute("autocomplete") || "").toLowerCase();
    if (
      uType !== "email" &&
      uAuto !== "username" &&
      uAuto !== "email" &&
      !hasLoginContext(usernameInput, root)
    ) {
      usernameInput = null;
    }
  }

  // Heuristic dự phòng: Nếu không tìm thấy totpInput nhưng trang là màn hình Two-Factor Authentication
  if (!totpInput && !passwordInput) {
    const pageText = `${document.title} ${window.location.href} ${document.body?.innerText?.slice(0, 800) || ""}`;
    const is2FaPage = /two-factor|2fa|mfa|verification|verify|authenticator|checkpoint/i.test(
      pageText,
    );
    const visibleTextInputs = inputs.filter(
      (inp) =>
        !inp.disabled &&
        !inp.readOnly &&
        inp.type !== "hidden" &&
        inp.type !== "submit" &&
        inp.type !== "button" &&
        !isSearchOrFilterInput(inp) &&
        isElementVisible(inp),
    );
    if (is2FaPage && visibleTextInputs.length === 1) {
      totpInput = visibleTextInputs[0];
      if (usernameInput === totpInput) {
        usernameInput = null;
      }
    }
  }

  return { usernameInput, passwordInput, newPasswordInput, totpInput };
}

/**
 * Kiểm tra xem một phần tử có phải là nút Submit / Đăng nhập khả dĩ không.
 */
export function isSubmitElement(el: Element | null): boolean {
  if (!el || !(el instanceof HTMLElement)) {
    return false;
  }

  if (el.closest("[role='search'], form[role='search']") !== null) {
    return false;
  }

  // Kiểm tra văn bản nội dung và các thuộc tính để loại trừ nút tìm kiếm / lọc / thao tác khác
  const text = (el.innerText || el.textContent || "").trim();
  const value = (el as HTMLInputElement).value || "";
  const ariaLabel = el.getAttribute("aria-label") || "";
  const title = el.getAttribute("title") || "";
  const idAndClass = `${el.id} ${el.className}`;
  const combined = `${text} ${value} ${ariaLabel} ${title} ${idAndClass}`;

  if (NON_LOGIN_BUTTON_PATTERNS.test(combined)) {
    return false;
  }

  // 1. Kiểm tra type="submit"
  const type = (el.getAttribute("type") || "").toLowerCase();
  if (type === "submit") {
    return true;
  }

  // 2. Thẻ button hoặc phần tử có role="button" hoặc class nút
  const tagName = el.tagName.toLowerCase();
  const role = (el.getAttribute("role") || "").toLowerCase();
  const isButtonLike =
    tagName === "button" ||
    role === "button" ||
    el.classList.contains("btn") ||
    el.classList.contains("button");

  if (!isButtonLike && tagName !== "input" && tagName !== "a") {
    return false;
  }

  // 3. Đối chiếu từ khóa đăng nhập
  return SUBMIT_PATTERNS.test(combined);
}

/**
 * Tìm kiếm nút submit khả dĩ nhất trên biểu mẫu hoặc tài liệu.
 */
export function findProbableSubmitButton(root: Document | Element = document): HTMLElement | null {
  const explicitSubmit = root.querySelector<HTMLElement>(
    'button[type="submit"], input[type="submit"]',
  );
  if (explicitSubmit && explicitSubmit.offsetParent !== null) {
    return explicitSubmit;
  }

  const candidates = Array.from(
    root.querySelectorAll<HTMLElement>(
      "button, [role='button'], input[type='button'], a.btn, a[role='button']",
    ),
  );
  for (const el of candidates) {
    if (el.offsetParent !== null && isSubmitElement(el)) {
      return el;
    }
  }

  return null;
}
