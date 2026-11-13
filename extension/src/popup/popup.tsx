/**
 * @file extension/src/popup/popup.tsx
 * @brief Điểm nhập điều khiển giao diện Popup của Browser Extension (Logins theo domain).
 */

import { isDomainMatch } from "../services/domain";
import {
  CredentialSecretPayload,
  CredentialSummary,
  CredentialsListPayload,
  detectCurrentBrowserName,
  GeneratedPasswordPayload,
  PasskeyQueryResponsePayload,
  PasskeySummaryItem,
  PENDING_TOTP_STORAGE_KEY,
  PENDING_TOTP_TTL_MS,
  PendingTotpSetup,
  sendExtensionBridgeMessage,
  UpdateTotpResponsePayload,
  VaultStatusPayload,
} from "../services/protocol";
import { createCredentialList } from "./components/CredentialList";
import { createHeaderBar } from "./components/HeaderBar";
import { createSvgPathIcon } from "./components/icons";
import { createLockView } from "./components/LockView";

function getSystemTheme(): "dark" | "light" {
  if (typeof window !== "undefined" && window.matchMedia) {
    return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
  }
  return "dark";
}

interface PopupState {
  theme: "dark" | "light";
  unlocked: boolean;
  configured: boolean;
  desktopRunning: boolean;
  biometricAvailable: boolean;
  activeTabId?: number;
  activeWindowId?: number;
  activeOrigin: string;
  activeDomain: string;
  searchQuery: string;
  showGenerator: boolean;
  genLength: number;
  genUpper: boolean;
  genLower: boolean;
  genDigits: boolean;
  genSymbols: boolean;
  generatedPassword: string;
  generatedPercent: number;
  generatedFeedback: string;
  credentials: CredentialSummary[];
  passkeys: PasskeySummaryItem[];
  pendingTotp: PendingTotpSetup | null;
  confirmOverwrite: { item: CredentialSummary; pendingUri?: string } | null;
  toastMessage: string;
}

const state: PopupState = {
  theme: getSystemTheme(),
  unlocked: false,
  configured: false,
  desktopRunning: false,
  biometricAvailable: false,
  activeTabId: undefined,
  activeWindowId: undefined,
  activeOrigin: "",
  activeDomain: "",
  searchQuery: "",
  showGenerator: false,
  genLength: 16,
  genUpper: true,
  genLower: true,
  genDigits: true,
  genSymbols: true,
  generatedPassword: "",
  generatedPercent: 0,
  generatedFeedback: "",
  credentials: [],
  passkeys: [],
  pendingTotp: null,
  confirmOverwrite: null,
  toastMessage: "",
};

let toastClearTimer: ReturnType<typeof setTimeout> | null = null;
let searchDebounceTimer: ReturnType<typeof setTimeout> | null = null;
let lastTotpEpochStep = Math.floor(Date.now() / 1000 / 30);

function updateToastDom(): void {
  const shell = document.getElementById("svm-popup-shell");
  if (!shell) return;
  let toast = document.getElementById("svm-popup-toast");
  if (!state.toastMessage) {
    toast?.remove();
    return;
  }
  if (!toast) {
    toast = document.createElement("div");
    toast.id = "svm-popup-toast";
    toast.className = "toastBanner";
    shell.appendChild(toast);
  }
  toast.textContent = state.toastMessage;
}

function showToast(msg: string): void {
  state.toastMessage = msg;
  updateToastDom();
  if (toastClearTimer) {
    clearTimeout(toastClearTimer);
  }
  toastClearTimer = setTimeout(() => {
    if (state.toastMessage === msg) {
      state.toastMessage = "";
      updateToastDom();
    }
  }, 2200);
}

async function copyText(text: string, label: string): Promise<void> {
  if (!text) return;
  try {
    await navigator.clipboard.writeText(text);
    showToast(`Đã sao chép ${label.toLowerCase()}`);
  } catch {
    showToast(`Không thể sao chép ${label.toLowerCase()}`);
  }
}

async function getActiveTabInfo(): Promise<{
  tabId?: number;
  windowId?: number;
  origin: string;
  domain: string;
}> {
  const chromeApi = (globalThis as Record<string, any>).chrome;
  if (chromeApi?.tabs?.query) {
    try {
      const tabs = await new Promise<Array<{ id?: number; windowId?: number; url?: string }>>(
        (resolve) => {
          chromeApi.tabs.query(
            { active: true, currentWindow: true },
            (res: Array<{ id?: number; windowId?: number; url?: string }>) => {
              resolve(res || []);
            },
          );
        },
      );
      const active = tabs?.[0];
      const url = active?.url || "";
      let domain = "";
      if (url) {
        try {
          const parsed = new URL(url);
          if (parsed.protocol === "http:" || parsed.protocol === "https:") {
            domain = parsed.hostname.replace(/^www\./, "").toLowerCase();
          }
        } catch {
          domain = "";
        }
      }
      return {
        tabId: active?.id,
        windowId: active?.windowId,
        origin: url,
        domain,
      };
    } catch {
      return { origin: "", domain: "" };
    }
  }
  return { origin: "", domain: "" };
}

async function loadPendingTotpSetup(): Promise<PendingTotpSetup | null> {
  const chromeApi = (globalThis as Record<string, any>).chrome;
  try {
    let item: PendingTotpSetup | undefined;
    if (chromeApi?.storage?.session?.get) {
      const res = await chromeApi.storage.session.get([PENDING_TOTP_STORAGE_KEY]);
      item = res?.[PENDING_TOTP_STORAGE_KEY] as PendingTotpSetup | undefined;
    }
    if (!item && chromeApi?.storage?.local?.get) {
      const res = await chromeApi.storage.local.get([PENDING_TOTP_STORAGE_KEY]);
      item = res?.[PENDING_TOTP_STORAGE_KEY] as PendingTotpSetup | undefined;
    }
    if (!item || !item.uri) {
      return null;
    }
    if (Date.now() - Number(item.createdAt || 0) > PENDING_TOTP_TTL_MS) {
      await clearPendingTotpSetup();
      return null;
    }
    return item;
  } catch {
    return null;
  }
}

async function clearPendingTotpSetup(): Promise<void> {
  state.pendingTotp = null;
  const chromeApi = (globalThis as Record<string, any>).chrome;
  try {
    if (chromeApi?.storage?.session?.remove) {
      await chromeApi.storage.session.remove([PENDING_TOTP_STORAGE_KEY]);
    }
    if (chromeApi?.storage?.local?.remove) {
      await chromeApi.storage.local.remove([PENDING_TOTP_STORAGE_KEY]);
    }
  } catch {
    // ignore
  }
}

function matchesDomain(item: CredentialSummary, domain: string): boolean {
  if (!domain) return false;
  const rawUrl = (item.websiteUrl || (item as any).url || "").trim();
  if (rawUrl && isDomainMatch(rawUrl, domain)) {
    return true;
  }

  if (!rawUrl) {
    const titleLow = item.title.trim().toLowerCase();
    const normDomain = domain.replace(/^www\./, "").toLowerCase();
    return (
      normDomain.length >= 4 && (titleLow === normDomain || titleLow.endsWith(`.${normDomain}`))
    );
  }
  return false;
}

async function fetchCredentialsForState(): Promise<void> {
  if (!state.unlocked || !state.configured) {
    state.credentials = [];
    state.passkeys = [];
    return;
  }

  const trimmedQuery = state.searchQuery.trim();
  const [credsResp, passkeysResp] = await Promise.all([
    sendExtensionBridgeMessage<CredentialsListPayload>("get_credentials", {
      domain: state.activeDomain,
      query: trimmedQuery,
    }),
    state.activeDomain && !trimmedQuery
      ? sendExtensionBridgeMessage<PasskeyQueryResponsePayload>("passkey_query", {
          rpId: state.activeDomain,
          allowCredentials: [],
        })
      : Promise.resolve(null),
  ]);

  if (credsResp.ok && credsResp.payload?.credentials) {
    let items = credsResp.payload.credentials;
    if (!trimmedQuery) {
      items = state.activeDomain
        ? items.filter((item) => matchesDomain(item, state.activeDomain))
        : [];
    }
    state.credentials = items;
  } else {
    state.credentials = [];
  }

  if (passkeysResp?.ok && passkeysResp.payload?.passkeys) {
    state.passkeys = passkeysResp.payload.passkeys;
  } else {
    state.passkeys = [];
  }
}

async function generateNewPassword(): Promise<void> {
  const resp = await sendExtensionBridgeMessage<GeneratedPasswordPayload>("generate_password", {
    length: state.genLength,
    uppercase: state.genUpper,
    lowercase: state.genLower,
    digits: state.genDigits,
    symbols: state.genSymbols,
  });
  if (resp.ok && resp.payload?.password) {
    state.generatedPassword = resp.payload.password;
    state.generatedPercent = resp.payload.percent || 0;
    state.generatedFeedback = resp.payload.feedback || "";
    updateGeneratorCardDom();
  } else {
    showToast(resp.error?.message || "Không thể tạo mật khẩu");
  }
}

async function loadInitialData(): Promise<void> {
  const { tabId, windowId, origin, domain } = await getActiveTabInfo();
  state.activeTabId = tabId;
  state.activeWindowId = windowId;
  state.activeOrigin = origin;
  state.activeDomain = domain;
  state.pendingTotp = await loadPendingTotpSetup();

  const statusResp = await sendExtensionBridgeMessage<VaultStatusPayload>("get_vault_status");
  if (statusResp.ok && statusResp.payload) {
    state.configured = Boolean(statusResp.payload.configured);
    state.unlocked = state.configured && Boolean(statusResp.payload.unlocked);
    state.desktopRunning = Boolean(statusResp.payload.desktopRunning ?? false);
    state.biometricAvailable = state.configured && Boolean(statusResp.payload.biometricAvailable);
  } else {
    state.configured = false;
    state.unlocked = false;
    state.desktopRunning = false;
    state.biometricAvailable = false;
  }

  await fetchCredentialsForState();
  renderPopup();
}

async function handleFillCredential(item: CredentialSummary): Promise<void> {
  const secretResp = await sendExtensionBridgeMessage<CredentialSecretPayload>("fill_credential", {
    itemId: item.id,
  });
  const payload = secretResp.payload || {
    username: item.username,
    password: item.password || "",
    totpCode: item.totpCode,
  };

  const chromeApi = (globalThis as Record<string, any>).chrome;
  if (chromeApi?.tabs?.query && chromeApi?.tabs?.sendMessage) {
    chromeApi.tabs.query({ active: true, currentWindow: true }, (tabs: Array<{ id?: number }>) => {
      const tabId = tabs?.[0]?.id;
      if (tabId !== undefined) {
        chromeApi.tabs.sendMessage(tabId, {
          type: "SVM_FILL_ACTIVE_TAB",
          username: payload.username,
          password: payload.password,
          totpCode: payload.totpCode,
        });
      }
    });
  }
  showToast(`Đã điền ${item.title}`);
}

async function launchQrCropForTab(item?: CredentialSummary): Promise<void> {
  if (!state.activeOrigin || !/^https?:\/\//i.test(state.activeOrigin)) {
    showToast("Chỉ hỗ trợ quét QR trên trang http:// hoặc https://");
    return;
  }
  const resp = await sendExtensionBridgeMessage<{ started?: boolean }>("start_qr_crop", {
    tabId: state.activeTabId,
    windowId: state.activeWindowId,
    origin: state.activeOrigin,
    domain: state.activeDomain,
    itemId: item?.id,
    itemTitle: item?.title,
  });
  if (resp.ok) {
    if (typeof window !== "undefined") {
      window.close();
    }
  } else {
    showToast(resp.error?.message || "Không thể khởi chạy khung quét QR");
  }
}

async function applyTotpUriToVault(
  itemId: string | null,
  uri: string,
  createNew: boolean,
): Promise<void> {
  const pending = state.pendingTotp;
  const payload: Record<string, unknown> = itemId
    ? { itemId, uri }
    : {
        createNew,
        uri,
        title: pending?.issuer || state.activeDomain || "2FA Account",
        username: pending?.account || "",
        websiteUrl: pending?.origin || state.activeOrigin || "",
      };
  const resp = await sendExtensionBridgeMessage<UpdateTotpResponsePayload>("update_totp", payload);
  if (!resp.ok || !resp.payload) {
    showToast(resp.error?.message || "Không thể lưu mã TOTP");
    return;
  }
  await clearPendingTotpSetup();
  state.confirmOverwrite = null;
  if (resp.payload.code) {
    try {
      await navigator.clipboard.writeText(resp.payload.code);
    } catch {
      // ignore
    }
  }
  await loadInitialData();
  showToast(`Đã lưu mã 2FA (${resp.payload.code})`);
}

function handleAttachTotpButtonClick(item: CredentialSummary): void {
  if (state.pendingTotp) {
    if (item.hasTotp) {
      state.confirmOverwrite = { item, pendingUri: state.pendingTotp.uri };
      renderPopup();
      return;
    }
    void applyTotpUriToVault(item.id, state.pendingTotp.uri, false);
    return;
  }
  if (item.hasTotp) {
    state.confirmOverwrite = { item };
    renderPopup();
    return;
  }
  void launchQrCropForTab(item);
}

function renderOverwriteBanner(): HTMLElement | null {
  const pendingConfirm = state.confirmOverwrite;
  if (!pendingConfirm) {
    return null;
  }
  const card = document.createElement("div");
  card.className = "bannerCard bannerWarn";

  const title = document.createElement("div");
  title.className = "bannerTitle";
  title.textContent = "Xác nhận ghi đè mã 2FA";

  const desc = document.createElement("div");
  desc.className = "bannerDesc";
  desc.textContent = pendingConfirm.pendingUri
    ? `Tài khoản "${pendingConfirm.item.title}" đã có mã 2FA. Bạn có muốn thay thế bằng mã QR vừa quét không?`
    : `Tài khoản "${pendingConfirm.item.title}" đã có mã 2FA. Quét QR mới để thay thế?`;

  const actions = document.createElement("div");
  actions.className = "bannerActions";

  const cancelBtn = document.createElement("button");
  cancelBtn.type = "button";
  cancelBtn.className = "secondaryBtn";
  cancelBtn.textContent = "Hủy";
  cancelBtn.addEventListener("click", () => {
    state.confirmOverwrite = null;
    renderPopup();
  });

  const confirmBtn = document.createElement("button");
  confirmBtn.type = "button";
  confirmBtn.className = "primaryBtn";
  confirmBtn.textContent = pendingConfirm.pendingUri ? "Thay thế 2FA" : "Tiếp tục quét QR";
  confirmBtn.addEventListener("click", () => {
    const targetItem = pendingConfirm.item;
    const uri = pendingConfirm.pendingUri;
    state.confirmOverwrite = null;
    if (uri) {
      void applyTotpUriToVault(targetItem.id, uri, false);
    } else {
      void launchQrCropForTab(targetItem);
    }
  });

  actions.appendChild(cancelBtn);
  actions.appendChild(confirmBtn);
  card.appendChild(title);
  card.appendChild(desc);
  card.appendChild(actions);
  return card;
}

function renderPendingTotpBanner(): HTMLElement | null {
  const pending = state.pendingTotp;
  if (!pending) {
    return null;
  }
  const card = document.createElement("div");
  card.className = "bannerCard";

  const title = document.createElement("div");
  title.className = "bannerTitle";
  title.textContent = "Mã QR 2FA đang chờ lưu";

  const labelText =
    [pending.issuer, pending.account].filter(Boolean).join(" • ") || "otpauth://totp";
  const desc = document.createElement("div");
  desc.className = "bannerDesc";
  desc.textContent =
    state.credentials.length > 0
      ? `Đã quét ${labelText}. Bấm "Gắn 2FA" trên tài khoản bên dưới hoặc tạo tài khoản mới.`
      : `Đã quét ${labelText}. Tạo bản ghi Login mới để lưu mã 2FA này.`;

  const actions = document.createElement("div");
  actions.className = "bannerActions";

  const dismissBtn = document.createElement("button");
  dismissBtn.type = "button";
  dismissBtn.className = "secondaryBtn";
  dismissBtn.textContent = "Bỏ qua";
  dismissBtn.addEventListener("click", () => {
    void clearPendingTotpSetup().then(() => renderPopup());
  });

  const createBtn = document.createElement("button");
  createBtn.type = "button";
  createBtn.className = "primaryBtn";
  createBtn.textContent = "Tạo Login mới với 2FA";
  createBtn.addEventListener("click", () => {
    void applyTotpUriToVault(null, pending.uri, true);
  });

  actions.appendChild(dismissBtn);
  actions.appendChild(createBtn);
  card.appendChild(title);
  card.appendChild(desc);
  card.appendChild(actions);
  return card;
}

function handleFillPasskeyUsername(pk: PasskeySummaryItem): void {
  if (!pk.username) return;
  const chromeApi = (globalThis as Record<string, any>).chrome;
  if (chromeApi?.tabs?.query && chromeApi?.tabs?.sendMessage) {
    chromeApi.tabs.query({ active: true, currentWindow: true }, (tabs: Array<{ id?: number }>) => {
      const tabId = tabs?.[0]?.id;
      if (tabId !== undefined) {
        chromeApi.tabs.sendMessage(tabId, {
          type: "SVM_FILL_ACTIVE_TAB",
          username: pk.username,
        });
      }
    });
  }
  showToast(`Đã điền ${pk.username}`);
}

function renderCredentialList(): HTMLElement {
  const trimmedQuery = state.searchQuery.trim();
  const displayScope = trimmedQuery ? `Tìm kiếm: "${trimmedQuery}"` : state.activeDomain;
  return createCredentialList({
    items: state.credentials,
    passkeys: state.passkeys,
    activeOrigin: state.activeOrigin,
    activeDomain: displayScope,
    hasPendingTotp: Boolean(state.pendingTotp),
    onFill: (item) => void handleFillCredential(item),
    onAttachTotpQr: (item) => handleAttachTotpButtonClick(item),
    onFillPasskeyUsername: (pk) => handleFillPasskeyUsername(pk),
    onCopyPasskeyUsername: (pk) => void copyText(pk.username, "Tên tài khoản Passkey"),
    onScanQrEmpty: () => void launchQrCropForTab(),
    onCopyUsername: (item) => void copyText(item.username, "Tên đăng nhập"),
    onCopyPassword: (item) =>
      void (async () => {
        const secretResp = await sendExtensionBridgeMessage<CredentialSecretPayload>(
          "fill_credential",
          { itemId: item.id },
        );
        await copyText(secretResp.payload?.password || "", "Mật khẩu");
      })(),
    onCopyTotp: (item) => {
      void (async () => {
        // Luôn yêu cầu mã TOTP thời gian thực mới nhất từ Desktop backend
        const secretResp = await sendExtensionBridgeMessage<CredentialSecretPayload>(
          "fill_credential",
          { itemId: item.id },
        );
        const code = secretResp.payload?.totpCode || item.totpCode || "";
        item.totpCode = code;
        await copyText(code, "Mã xác thực 2FA");
        const chromeApi = (globalThis as Record<string, any>).chrome;
        if (chromeApi?.tabs?.query && chromeApi?.tabs?.sendMessage && code) {
          chromeApi.tabs.query(
            { active: true, currentWindow: true },
            (tabs: Array<{ id?: number }>) => {
              const tabId = tabs?.[0]?.id;
              if (tabId !== undefined) {
                chromeApi.tabs.sendMessage(tabId, {
                  type: "SVM_FILL_ACTIVE_TAB",
                  username: item.username,
                  totpCode: code,
                });
              }
            },
          );
        }
      })();
    },
  });
}

function updateCredentialListMount(): void {
  const mount = document.getElementById("svm-cred-list-mount");
  if (!mount) return;
  mount.replaceChildren(renderCredentialList());
}

function buildGeneratorCardElement(): HTMLElement {
  const card = document.createElement("div");
  card.className = "genCard";

  const header = document.createElement("div");
  header.className = "genHeader";
  const titleSpan = document.createElement("span");
  titleSpan.textContent = "Trình tạo mật khẩu an toàn";
  const closeBtn = document.createElement("button");
  closeBtn.type = "button";
  closeBtn.className = "secondaryBtn";
  closeBtn.textContent = "Đóng";
  closeBtn.addEventListener("click", () => {
    state.showGenerator = false;
    renderPopup();
  });
  header.appendChild(titleSpan);
  header.appendChild(closeBtn);
  card.appendChild(header);

  const pwdBox = document.createElement("div");
  pwdBox.className = "genPwdBox";
  pwdBox.textContent = state.generatedPassword || "Đang khởi tạo mật khẩu...";
  card.appendChild(pwdBox);

  const metaRow = document.createElement("div");
  metaRow.className = "genMetaRow";
  const bar = document.createElement("div");
  bar.className = "genStrengthBar";
  const fill = document.createElement("div");
  fill.className = "genStrengthFill";
  fill.style.width = `${Math.min(100, Math.max(0, state.generatedPercent))}%`;
  bar.appendChild(fill);
  const feedback = document.createElement("span");
  feedback.textContent = state.generatedFeedback || `${state.genLength} ký tự`;
  metaRow.appendChild(bar);
  metaRow.appendChild(feedback);
  card.appendChild(metaRow);

  const lenRow = document.createElement("div");
  lenRow.className = "genControlsRow";
  const lenLabel = document.createElement("span");
  lenLabel.textContent = `Độ dài: ${state.genLength}`;
  const lenSlider = document.createElement("input");
  lenSlider.type = "range";
  lenSlider.min = "8";
  lenSlider.max = "64";
  lenSlider.value = String(state.genLength);
  lenSlider.addEventListener("input", () => {
    state.genLength = Number.parseInt(lenSlider.value, 10) || 16;
    lenLabel.textContent = `Độ dài: ${state.genLength}`;
  });
  lenSlider.addEventListener("change", () => {
    void generateNewPassword();
  });
  lenRow.appendChild(lenLabel);
  lenRow.appendChild(lenSlider);
  card.appendChild(lenRow);

  const checksRow = document.createElement("div");
  checksRow.className = "genCheckboxGrid";
  const opts: Array<{ key: "genUpper" | "genLower" | "genDigits" | "genSymbols"; label: string }> =
    [
      { key: "genUpper", label: "A-Z" },
      { key: "genLower", label: "a-z" },
      { key: "genDigits", label: "0-9" },
      { key: "genSymbols", label: "!@#$" },
    ];
  for (const opt of opts) {
    const lbl = document.createElement("label");
    lbl.className = "genCheckLabel";
    const cb = document.createElement("input");
    cb.type = "checkbox";
    cb.checked = state[opt.key];
    cb.addEventListener("change", () => {
      state[opt.key] = cb.checked;
      void generateNewPassword();
    });
    lbl.appendChild(cb);
    lbl.appendChild(document.createTextNode(opt.label));
    checksRow.appendChild(lbl);
  }
  card.appendChild(checksRow);

  const actions = document.createElement("div");
  actions.className = "bannerActions";

  const regenBtn = document.createElement("button");
  regenBtn.type = "button";
  regenBtn.className = "secondaryBtn";
  regenBtn.textContent = "Tạo lại";
  regenBtn.addEventListener("click", () => void generateNewPassword());

  const copyBtn = document.createElement("button");
  copyBtn.type = "button";
  copyBtn.className = "secondaryBtn";
  copyBtn.textContent = "Sao chép";
  copyBtn.addEventListener("click", () => {
    void copyText(state.generatedPassword, "Mật khẩu vừa tạo");
  });

  const fillBtn = document.createElement("button");
  fillBtn.type = "button";
  fillBtn.className = "primaryBtn";
  fillBtn.textContent = "Điền vào trang";
  fillBtn.addEventListener("click", () => {
    if (!state.generatedPassword) return;
    const chromeApi = (globalThis as Record<string, any>).chrome;
    if (chromeApi?.tabs?.query && chromeApi?.tabs?.sendMessage) {
      chromeApi.tabs.query(
        { active: true, currentWindow: true },
        (tabs: Array<{ id?: number }>) => {
          const tabId = tabs?.[0]?.id;
          if (tabId !== undefined) {
            chromeApi.tabs.sendMessage(tabId, {
              type: "SVM_FILL_ACTIVE_TAB",
              password: state.generatedPassword,
            });
          }
        },
      );
    }
    showToast("Đã điền mật khẩu mới lên trang");
  });

  actions.appendChild(regenBtn);
  actions.appendChild(copyBtn);
  actions.appendChild(fillBtn);
  card.appendChild(actions);

  return card;
}

function updateGeneratorCardDom(): void {
  const mount = document.getElementById("svm-generator-mount");
  if (!mount) return;
  if (!state.showGenerator) {
    mount.replaceChildren();
    return;
  }
  mount.replaceChildren(buildGeneratorCardElement());
}

export function renderPopup(): void {
  const root = document.getElementById("svm-popup-root");
  if (!root) return;

  state.theme = getSystemTheme();
  document.documentElement.setAttribute("data-theme", state.theme);
  root.replaceChildren();

  const shell = document.createElement("div");
  shell.id = "svm-popup-shell";
  shell.className = "popupShell";

  shell.appendChild(
    createHeaderBar({
      unlocked: state.unlocked,
      configured: state.configured,
      desktopRunning: state.desktopRunning,
      showGenerator: state.showGenerator,
      onToggleGenerator: () => {
        state.showGenerator = !state.showGenerator;
        renderPopup();
        if (state.showGenerator && !state.generatedPassword) {
          void generateNewPassword();
        }
      },
      onScanQr: () => void launchQrCropForTab(),
      onLockVault: async () => {
        await sendExtensionBridgeMessage("lock_vault");
        state.unlocked = false;
        state.showGenerator = false;
        state.searchQuery = "";
        showToast("Kho mật khẩu đã bị khóa");
        renderPopup();
      },
    }),
  );

  const content = document.createElement("main");
  content.className = "contentArea";

  // Khi Vault chưa thiết lập, đang bị khóa, hoặc Desktop App chưa chạy: Hiển thị màn trạng thái tương ứng
  if (!state.configured || !state.unlocked) {
    content.appendChild(
      createLockView({
        desktopRunning: state.desktopRunning,
        configured: state.configured,
        biometricAvailable: state.biometricAvailable,
        onUnlockPassword: async (password: string) => {
          const resp = await sendExtensionBridgeMessage<any>("unlock_vault", {
            password,
            browser: detectCurrentBrowserName(),
            version: "0.1.0",
          });
          if (resp.ok && resp.payload?.unlocked) {
            state.unlocked = true;
            showToast("Đã mở khóa kho mật khẩu");
            await loadInitialData();
            return true;
          }
          return false;
        },
        onUnlockBiometric: async () => {
          const resp = await sendExtensionBridgeMessage<any>("unlock_vault_biometric", {
            browser: detectCurrentBrowserName(),
            version: "0.1.0",
          });
          if (resp.ok && resp.payload?.unlocked) {
            state.unlocked = true;
            showToast("Đã mở khóa bằng sinh trắc học");
            await loadInitialData();
            return true;
          }
          return false;
        },
        onRefresh: () => {
          void loadInitialData();
        },
      }),
    );
    shell.appendChild(content);
    root.appendChild(shell);
    updateToastDom();
    return;
  }

  // Khi Vault ĐÃ MỞ KHÓA
  const genMount = document.createElement("div");
  genMount.id = "svm-generator-mount";
  if (state.showGenerator) {
    genMount.appendChild(buildGeneratorCardElement());
  }
  content.appendChild(genMount);

  const searchWrap = document.createElement("div");
  searchWrap.className = "searchBarWrap";
  const searchIcon = document.createElement("span");
  searchIcon.className = "searchBarIcon";
  searchIcon.appendChild(
    createSvgPathIcon("M21 21l-4.35-4.35M11 19a8 8 0 100-16 8 8 0 000 16z", 14),
  );
  const searchInput = document.createElement("input");
  searchInput.type = "text";
  searchInput.className = "searchBarInput";
  searchInput.placeholder = "Tìm tài khoản trong kho...";
  searchInput.value = state.searchQuery;

  const clearBtn = document.createElement("button");
  clearBtn.type = "button";
  clearBtn.className = "searchClearBtn";
  clearBtn.textContent = "✕";
  clearBtn.title = "Xóa tìm kiếm";
  clearBtn.style.display = state.searchQuery ? "inline-flex" : "none";

  const triggerSearchRefresh = () => {
    if (searchDebounceTimer) {
      clearTimeout(searchDebounceTimer);
    }
    searchDebounceTimer = setTimeout(() => {
      void fetchCredentialsForState().then(() => updateCredentialListMount());
    }, 150);
  };

  searchInput.addEventListener("input", () => {
    state.searchQuery = searchInput.value;
    clearBtn.style.display = state.searchQuery ? "inline-flex" : "none";
    triggerSearchRefresh();
  });
  clearBtn.addEventListener("click", () => {
    state.searchQuery = "";
    searchInput.value = "";
    clearBtn.style.display = "none";
    searchInput.focus();
    triggerSearchRefresh();
  });

  searchWrap.appendChild(searchIcon);
  searchWrap.appendChild(searchInput);
  searchWrap.appendChild(clearBtn);
  content.appendChild(searchWrap);

  const overwriteBanner = renderOverwriteBanner();
  if (overwriteBanner) {
    content.appendChild(overwriteBanner);
  }
  const pendingBanner = renderPendingTotpBanner();
  if (pendingBanner) {
    content.appendChild(pendingBanner);
  }

  const credMount = document.createElement("div");
  credMount.id = "svm-cred-list-mount";
  credMount.appendChild(renderCredentialList());
  content.appendChild(credMount);

  shell.appendChild(content);
  root.appendChild(shell);
  updateToastDom();
}

async function syncStatusSilently(): Promise<void> {
  try {
    const { origin, domain } = await getActiveTabInfo();
    const tabChanged = origin !== state.activeOrigin || domain !== state.activeDomain;

    const statusResp = await sendExtensionBridgeMessage<VaultStatusPayload>("get_vault_status");
    if (statusResp.ok && statusResp.payload) {
      const newConfigured = Boolean(statusResp.payload.configured);
      const newUnlocked = newConfigured && Boolean(statusResp.payload.unlocked);
      const newDesktop = Boolean(statusResp.payload.desktopRunning ?? false);
      const newBio = newConfigured && Boolean(statusResp.payload.biometricAvailable);
      if (
        newConfigured !== state.configured ||
        newUnlocked !== state.unlocked ||
        newDesktop !== state.desktopRunning ||
        newBio !== state.biometricAvailable ||
        tabChanged
      ) {
        await loadInitialData();
      }
    } else if (state.desktopRunning || state.unlocked || state.configured) {
      state.desktopRunning = false;
      state.configured = false;
      state.unlocked = false;
      renderPopup();
    }
  } catch {
    // Ignore silent check errors
  }
}

if (typeof document !== "undefined") {
  document.addEventListener("DOMContentLoaded", () => {
    state.theme = getSystemTheme();
    document.documentElement.setAttribute("data-theme", state.theme);

    if (window.matchMedia) {
      window.matchMedia("(prefers-color-scheme: dark)").addEventListener("change", (e) => {
        state.theme = e.matches ? "dark" : "light";
        document.documentElement.setAttribute("data-theme", state.theme);
        renderPopup();
      });
    }

    void loadInitialData();
    setInterval(() => {
      void syncStatusSilently();
    }, 1500);
    setInterval(() => {
      const currentStep = Math.floor(Date.now() / 1000 / 30);
      if (currentStep !== lastTotpEpochStep) {
        lastTotpEpochStep = currentStep;
        if (state.unlocked && state.credentials.some((c) => c.hasTotp)) {
          void fetchCredentialsForState().then(() => updateCredentialListMount());
        }
      }
    }, 1000);
  });
}
