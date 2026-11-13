/**
 * @file extension/src/content/form-capture.ts
 * @brief Bắt thông minh tương tác đăng nhập (Click / Enter / Submit / SPA Navigation) và đề xuất lưu tài khoản khi đăng nhập thành công.
 */

import {
  cleanActionWords,
  extractBrandOrServiceName,
  generateSmartCredentialTitle,
} from "../services/brand-extractor";
import {
  CredentialSecretPayload,
  CredentialsListPayload,
  sendExtensionBridgeMessage,
} from "../services/protocol";
import { isRecentlyAutofilled } from "./autofill";
import {
  classifyDocumentFields,
  isElementVisible,
  isSearchOrFilterInput,
  isSubmitElement,
} from "./classifier";
import { showSaveCredentialPrompt } from "./ui/save-prompt";

export { cleanActionWords, extractBrandOrServiceName, generateSmartCredentialTitle };

const PENDING_LOGIN_TTL_MS = 60000; // 60 giây
const LAST_TYPED_TTL_MS = 60000; // 60 giây

export interface PendingLoginData {
  domain: string;
  origin: string;
  username: string;
  password: string;
  timestamp: number;
  submittedUrl: string;
  title?: string;
}

// Bộ nhớ cache tạm thời trong biến JavaScript
let memoryPendingLogin: PendingLoginData | null = null;

// Ghi nhớ giá trị username / password gần nhất khi người dùng gõ kèm mốc thời gian (TTL)
let lastTypedUsername = "";
let lastTypedPassword = "";
let lastTypedUsernameAt = 0;
let lastTypedPasswordAt = 0;

/**
 * Xóa sạch dữ liệu gõ phím tạm thời khỏi bộ nhớ phiên.
 */
function resetTypedCredentials(): void {
  lastTypedUsername = "";
  lastTypedPassword = "";
  lastTypedUsernameAt = 0;
  lastTypedPasswordAt = 0;
}

// Cờ khóa chống hiển thị Save Prompt trùng lặp gây nháy màn hình
let isPromptProcessing = false;
let lastPromptTimestamp = 0;
let activeSpaObserver: MutationObserver | null = null;
let activeSpaTimer: ReturnType<typeof setTimeout> | null = null;
let activeFallbackTimer: ReturnType<typeof setTimeout> | null = null;

/**
 * Lưu dữ liệu đăng nhập tạm thời.
 * Lưu ý bảo mật: Tuyệt đối không dùng sessionStorage (chia sẻ với Main World/trang web)
 * hoặc storage.session chia sẻ mọi context; chỉ gửi một chiều lên RAM của Background Service Worker.
 */
async function storePendingLogin(data: PendingLoginData): Promise<void> {
  memoryPendingLogin = data;

  // Gửi đồng bộ sang RAM của Background Service Worker theo tabId
  try {
    await sendExtensionBridgeMessage(
      "record_pending_login",
      data as unknown as Record<string, unknown>,
    );
  } catch {
    // Bỏ qua
  }
}

/**
 * Lấy dữ liệu đăng nhập tạm thời từ RAM / Background Service Worker.
 */
async function retrievePendingLogin(): Promise<PendingLoginData | null> {
  if (memoryPendingLogin && Date.now() - memoryPendingLogin.timestamp < PENDING_LOGIN_TTL_MS) {
    return memoryPendingLogin;
  }

  // Hỏi từ Background Service Worker gắn với Tab hiện tại
  try {
    const bgResp = await sendExtensionBridgeMessage<PendingLoginData>("get_pending_login", {
      domain: window.location.hostname,
    });
    if (bgResp.ok && bgResp.payload?.username && bgResp.payload?.password) {
      if (Date.now() - bgResp.payload.timestamp < PENDING_LOGIN_TTL_MS) {
        memoryPendingLogin = bgResp.payload;
        return bgResp.payload;
      }
    }
  } catch {
    // Bỏ qua lỗi
  }

  return null;
}

/**
 * Xóa sạch dữ liệu đăng nhập tạm thời khỏi RAM và Background Worker.
 */
async function clearPendingLogin(): Promise<void> {
  memoryPendingLogin = null;
  resetTypedCredentials();

  try {
    await sendExtensionBridgeMessage("clear_pending_login");
  } catch {
    // Bỏ qua
  }
}

/**
 * Kiểm tra xem trang hiện tại có hiển thị thông báo lỗi đăng nhập rõ ràng hay không.
 */
function hasVisibleLoginError(): boolean {
  const errorPatterns =
    /invalid[-_ ]?(credentials|password|username)|sai[-_ ]?mật[-_ ]?khẩu|tài[-_ ]?khoản[-_ ]?không[-_ ]?đúng|incorrect password|authentication failed|đăng[-_ ]?nhập[-_ ]?thất[-_ ]?bại/i;

  const errorElements = Array.from(
    document.querySelectorAll<HTMLElement>(
      "[role='alert'], .error, .alert-danger, .error-message, .text-danger, .error-text",
    ),
  );

  for (const el of errorElements) {
    if (el instanceof HTMLElement && el.offsetParent !== null) {
      const text = el.textContent || "";
      if (errorPatterns.test(text)) {
        return true;
      }
    }
  }

  return false;
}

/**
 * Thu thập username và password khả dĩ nhất trên trang hiện tại.
 */
function extractCurrentCredentials(searchRoot: Document | Element = document): {
  username: string;
  password: string;
  passwordInput: HTMLInputElement | null;
} {
  let { usernameInput, passwordInput, newPasswordInput } = classifyDocumentFields(searchRoot);
  if (newPasswordInput && newPasswordInput.value) {
    passwordInput = newPasswordInput;
  }

  let username = usernameInput?.value?.trim() || "";
  let password = passwordInput?.value || "";

  // Dự phòng: Quét trực tiếp ô input type="password" đang hiển thị
  if (!passwordInput) {
    const anyPwd = Array.from(
      searchRoot.querySelectorAll<HTMLInputElement>("input[type='password']"),
    ).find((el) => !el.disabled && !el.readOnly && isElementVisible(el));
    if (anyPwd) {
      passwordInput = anyPwd;
      if (!password && anyPwd.value) {
        password = anyPwd.value;
      }
    }
  }

  const now = Date.now();

  // Nếu không thấy qua classifyDocumentFields, dùng giá trị đã ghi nhớ lúc user gõ (còn hiệu lực TTL)
  if (!username && lastTypedUsername && now - lastTypedUsernameAt < LAST_TYPED_TTL_MS) {
    username = lastTypedUsername;
  }
  if (!password && lastTypedPassword && now - lastTypedPasswordAt < LAST_TYPED_TTL_MS) {
    password = lastTypedPassword;
  }

  // Dự phòng cuối cho username: ô text đang hiển thị (không phải ô tìm kiếm/bộ lọc)
  if (!username) {
    const textInputs = Array.from(
      searchRoot.querySelectorAll<HTMLInputElement>("input[type='text'], input[type='email']"),
    );
    for (const ti of textInputs) {
      if (ti.disabled || ti.readOnly || !isElementVisible(ti) || isSearchOrFilterInput(ti)) {
        continue;
      }
      if (ti.value?.trim()) {
        username = ti.value.trim();
        break;
      }
    }
  }

  return { username, password, passwordInput };
}

/**
 * Xử lý khi bắt được thao tác submit/click đăng nhập từ phía người dùng.
 */
async function recordPotentialLoginAttempt(targetElement?: Element | null): Promise<void> {
  if (targetElement instanceof HTMLInputElement && isSearchOrFilterInput(targetElement)) {
    return;
  }
  if (targetElement instanceof HTMLFormElement && targetElement.getAttribute("role") === "search") {
    return;
  }

  const searchRoot = targetElement?.closest("form") || document;
  const { username, password, passwordInput } = extractCurrentCredentials(searchRoot);

  if (!passwordInput || !username || !password) {
    return;
  }

  // Bỏ qua nếu vừa được autofill bởi chính extension
  if (isRecentlyAutofilled(username, password)) {
    return;
  }

  const pending: PendingLoginData = {
    domain: window.location.hostname,
    origin: window.location.origin,
    username,
    password,
    timestamp: Date.now(),
    submittedUrl: window.location.href,
    title: generateSmartCredentialTitle(username, searchRoot),
  };

  await storePendingLogin(pending);

  // Hủy các quan sát/hẹn giờ cũ nếu có trước khi tạo mới
  if (activeSpaObserver) {
    activeSpaObserver.disconnect();
    activeSpaObserver = null;
  }
  if (activeSpaTimer) {
    clearTimeout(activeSpaTimer);
    activeSpaTimer = null;
  }
  if (activeFallbackTimer) {
    clearTimeout(activeFallbackTimer);
    activeFallbackTimer = null;
  }

  // Quan sát DOM hiện tại nếu form đăng nhập bị biến mất (SPA)
  observeSpaFormRemoval(passwordInput, pending);

  // Đặt thêm hẹn giờ dự phòng: nếu sau 1.5 giây URL thay đổi hoặc ô password bị gỡ/ẩn mà không có lỗi
  activeFallbackTimer = setTimeout(() => {
    activeFallbackTimer = null;
    if (
      window.location.href !== pending.submittedUrl ||
      !document.contains(passwordInput) ||
      !isElementVisible(passwordInput)
    ) {
      void verifyAndPrompt(pending);
    }
  }, 1500);
}

/**
 * Theo dõi nếu trường password bị xóa khỏi DOM (dấu hiệu SPA đăng nhập thành công).
 */
function observeSpaFormRemoval(pwdInput: HTMLInputElement, pending: PendingLoginData): void {
  const observer = new MutationObserver(() => {
    const isDetached = !document.body.contains(pwdInput);
    const isHidden = pwdInput.offsetParent === null;

    if (isDetached || isHidden) {
      observer.disconnect();
      activeSpaObserver = null;
      if (activeSpaTimer) {
        clearTimeout(activeSpaTimer);
      }
      activeSpaTimer = setTimeout(() => {
        activeSpaTimer = null;
        void verifyAndPrompt(pending);
      }, 350);
    }
  });

  activeSpaObserver = observer;
  observer.observe(document.body, { childList: true, subtree: true, attributes: true });

  activeSpaTimer = setTimeout(() => {
    observer.disconnect();
    if (activeSpaObserver === observer) {
      activeSpaObserver = null;
    }
    activeSpaTimer = null;
  }, 10000);
}

/**
 * Xác minh và hiển thị Save Prompt nếu thông tin hợp lệ và chưa tồn tại trong Vault.
 */
async function verifyAndPrompt(pending: PendingLoginData): Promise<void> {
  // Chống kích hoạt trùng lặp làm nhấp nháy giao diện
  if (isPromptProcessing || Date.now() - lastPromptTimestamp < 3500) {
    return;
  }
  if (document.getElementById("svm-save-prompt-host")) {
    return;
  }

  isPromptProcessing = true;

  // Hủy ngay tất cả observer và timer đang chờ để không bị gọi lại lần thứ 2
  if (activeSpaObserver) {
    activeSpaObserver.disconnect();
    activeSpaObserver = null;
  }
  if (activeSpaTimer) {
    clearTimeout(activeSpaTimer);
    activeSpaTimer = null;
  }
  if (activeFallbackTimer) {
    clearTimeout(activeFallbackTimer);
    activeFallbackTimer = null;
  }

  // Xóa ngay bộ nhớ đệm phiên đăng nhập để tránh race condition từ các sự kiện khác
  await clearPendingLogin();

  try {
    // Kiểm tra lỗi hiển thị trên trang
    if (hasVisibleLoginError()) {
      return;
    }

    const resp = await sendExtensionBridgeMessage<CredentialsListPayload>("get_credentials", {
      domain: pending.domain,
    });

    if (resp.ok && resp.payload?.credentials?.length) {
      const match = resp.payload.credentials.find(
        (c) => c.username.trim().toLowerCase() === pending.username.toLowerCase(),
      );

      if (match) {
        // Mật khẩu trùng khớp -> đã lưu trước đó, bỏ qua
        if (match.password && match.password === pending.password) {
          return;
        }

        // Truy vấn mật khẩu giải mã để đối chiếu
        const secretResp = await sendExtensionBridgeMessage<CredentialSecretPayload>(
          "fill_credential",
          { itemId: match.id },
        );
        if (secretResp.ok && secretResp.payload?.password === pending.password) {
          return;
        }

        // Người dùng đổi mật khẩu mới -> Hỏi cập nhật
        await showSaveCredentialPrompt({
          itemId: match.id,
          title: match.title || pending.title || generateSmartCredentialTitle(pending.username),
          username: pending.username,
          password: pending.password,
          websiteUrl: pending.origin,
          isUpdate: true,
        });
        return;
      }
    }

    // Tài khoản mới hoàn toàn -> Hỏi lưu mới
    await showSaveCredentialPrompt({
      title: pending.title || generateSmartCredentialTitle(pending.username),
      username: pending.username,
      password: pending.password,
      websiteUrl: pending.origin,
      isUpdate: false,
    });
  } catch {
    // Nếu có lỗi kết nối backend, vẫn hiển thị Save Prompt để người dùng không bị lỡ
    await showSaveCredentialPrompt({
      title: pending.title || generateSmartCredentialTitle(pending.username),
      username: pending.username,
      password: pending.password,
      websiteUrl: pending.origin,
      isUpdate: false,
    });
  } finally {
    lastPromptTimestamp = Date.now();
    isPromptProcessing = false;
  }
}

/**
 * Kiểm tra xem có phiên đăng nhập pending nào từ trang trước chuyển hướng sang không.
 */
export async function checkPendingLoginAfterNavigation(): Promise<void> {
  if (isPromptProcessing || Date.now() - lastPromptTimestamp < 3500) {
    return;
  }
  if (document.getElementById("svm-save-prompt-host")) {
    return;
  }

  const pending = await retrievePendingLogin();
  if (!pending) {
    return;
  }

  if (Date.now() - pending.timestamp > PENDING_LOGIN_TTL_MS) {
    await clearPendingLogin();
    return;
  }

  if (pending.domain === window.location.hostname) {
    setTimeout(() => {
      void verifyAndPrompt(pending);
    }, 250);
  }
}

/**
 * Đính kèm bộ lắng nghe đa sự kiện (Input tracking, Submit, Click nút Sign-in, Phím Enter, SPA navigation).
 */
export function attachFormSubmitCapture(): void {
  // 1. Kiểm tra ngay phiên pending từ lần chuyển hướng trước
  void checkPendingLoginAfterNavigation();

  // 2. Theo dõi người dùng gõ phím vào ô username và password để luôn có dữ liệu mới nhất
  document.addEventListener(
    "input",
    (event) => {
      const target = event.target as HTMLElement | null;
      if (target instanceof HTMLInputElement) {
        if (target.type === "password") {
          lastTypedPassword = target.value;
          lastTypedPasswordAt = Date.now();
        } else if (target.type === "text" || target.type === "email") {
          if (isSearchOrFilterInput(target)) {
            return;
          }
          // Bỏ qua nếu ô này là mã xác thực 2FA/TOTP (tránh ghi đè username bằng mã số OTP 6 số)
          const signature =
            `${target.name} ${target.id} ${target.placeholder} ${target.className} ${target.getAttribute("aria-label") || ""}`.toLowerCase();
          const isOtpInput =
            target.getAttribute("autocomplete") === "one-time-code" ||
            target.maxLength === 6 ||
            /totp|otp|2fa|code|mfa|auth|token|checkpoint/i.test(signature);
          if (!isOtpInput) {
            const { usernameInput, passwordInput } = classifyDocumentFields(document);
            if (usernameInput === target || passwordInput !== null) {
              lastTypedUsername = target.value.trim();
              lastTypedUsernameAt = Date.now();
            }
          }
        }
      }
    },
    true,
  );

  // 3. Bắt sự kiện submit truyền thống
  document.addEventListener(
    "submit",
    (event) => {
      const form = event.target instanceof HTMLElement ? event.target : null;
      void recordPotentialLoginAttempt(form);
    },
    true,
  );

  // 4. Bắt sự kiện click trên các nút dạng button / SPA có nội dung đăng nhập
  document.addEventListener(
    "click",
    (event) => {
      const target = event.target as HTMLElement | null;
      if (!target) return;

      const clickable = target.closest(
        "button, [role='button'], input[type='submit'], input[type='button'], a.btn",
      );
      if (clickable && isSubmitElement(clickable)) {
        void recordPotentialLoginAttempt(clickable);
      }
    },
    true,
  );

  // 5. Bắt phím Enter trong ô input
  document.addEventListener(
    "keydown",
    (event) => {
      if (event.key === "Enter") {
        const target = event.target as HTMLElement | null;
        if (target instanceof HTMLInputElement) {
          void recordPotentialLoginAttempt(target);
        }
      }
    },
    true,
  );

  // 6. Theo dõi điều hướng SPA (popstate & hashchange & pushState intercept)
  window.addEventListener("popstate", () => {
    void checkPendingLoginAfterNavigation();
  });
  window.addEventListener("hashchange", () => {
    void checkPendingLoginAfterNavigation();
  });

  // Đánh chặn pushState / replaceState trong trang SPA
  try {
    const origPush = history.pushState.bind(history);
    const origReplace = history.replaceState.bind(history);
    history.pushState = function (...args) {
      const res = origPush(...args);
      setTimeout(() => void checkPendingLoginAfterNavigation(), 400);
      return res;
    };
    history.replaceState = function (...args) {
      const res = origReplace(...args);
      setTimeout(() => void checkPendingLoginAfterNavigation(), 400);
      return res;
    };
  } catch {
    // Bỏ qua nếu bị trang web bảo vệ đóng băng history
  }
}
