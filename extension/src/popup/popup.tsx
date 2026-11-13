/**
 * @file extension/src/popup/popup.tsx
 * @brief Điểm nhập điều khiển giao diện Popup của Browser Extension (Logins theo domain).
 */

import {
  CredentialSecretPayload,
  CredentialSummary,
  CredentialsListPayload,
  detectCurrentBrowserName,
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
  credentials: [],
  passkeys: [],
  pendingTotp: null,
  confirmOverwrite: null,
  toastMessage: "",
};

function showToast(msg: string): void {
  state.toastMessage = msg;
  renderPopup();
  setTimeout(() => {
    if (state.toastMessage === msg) {
      state.toastMessage = "";
      renderPopup();
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
  const rawUrl = (item.websiteUrl || (item as any).url || "").trim().toLowerCase();
  if (rawUrl) {
    try {
      const parsed = new URL(rawUrl.includes("://") ? rawUrl : `https://${rawUrl}`);
      const itemHost = parsed.hostname.replace(/^www\./, "").toLowerCase();
      if (
        itemHost === domain ||
        domain.endsWith(`.${itemHost}`) ||
        itemHost.endsWith(`.${domain}`)
      ) {
        return true;
      }
    } catch {
      if (rawUrl.includes(domain) || domain.includes(rawUrl)) {
        return true;
      }
    }
  }

  const titleLow = item.title.trim().toLowerCase();
  const domainParts = domain.split(".");
  const domainBase = domainParts[0];
  if (domainBase && domainBase.length >= 3 && titleLow.includes(domainBase)) {
    return true;
  }
  return false;
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

  if (state.unlocked && state.configured) {
    const [credsResp, passkeysResp] = await Promise.all([
      sendExtensionBridgeMessage<CredentialsListPayload>("get_credentials", {
        domain: state.activeDomain,
      }),
      state.activeDomain
        ? sendExtensionBridgeMessage<PasskeyQueryResponsePayload>("passkey_query", {
            rpId: state.activeDomain,
            allowCredentials: [],
          })
        : Promise.resolve(null),
    ]);
    if (credsResp.ok && credsResp.payload?.credentials) {
      let items = credsResp.payload.credentials;
      if (state.activeDomain) {
        items = items.filter((item) => matchesDomain(item, state.activeDomain));
      } else {
        items = [];
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
  } else {
    state.credentials = [];
    state.passkeys = [];
  }

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
  return createCredentialList({
    items: state.credentials,
    passkeys: state.passkeys,
    activeOrigin: state.activeOrigin,
    activeDomain: state.activeDomain,
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

export function renderPopup(): void {
  const root = document.getElementById("svm-popup-root");
  if (!root) return;

  state.theme = getSystemTheme();
  document.documentElement.setAttribute("data-theme", state.theme);
  root.replaceChildren();

  const shell = document.createElement("div");
  shell.className = "popupShell";

  shell.appendChild(
    createHeaderBar({
      unlocked: state.unlocked,
      configured: state.configured,
      desktopRunning: state.desktopRunning,
      onScanQr: () => void launchQrCropForTab(),
      onLockVault: async () => {
        await sendExtensionBridgeMessage("lock_vault");
        state.unlocked = false;
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

    if (state.toastMessage) {
      const toast = document.createElement("div");
      toast.className = "toastBanner";
      toast.textContent = state.toastMessage;
      shell.appendChild(toast);
    }

    root.appendChild(shell);
    return;
  }

  // Khi Vault ĐÃ MỞ KHÓA: Hiển thị danh sách Logins trực tiếp theo domain đang mở
  const overwriteBanner = renderOverwriteBanner();
  if (overwriteBanner) {
    content.appendChild(overwriteBanner);
  }
  const pendingBanner = renderPendingTotpBanner();
  if (pendingBanner) {
    content.appendChild(pendingBanner);
  }
  content.appendChild(renderCredentialList());
  shell.appendChild(content);

  if (state.toastMessage) {
    const toast = document.createElement("div");
    toast.className = "toastBanner";
    toast.textContent = state.toastMessage;
    shell.appendChild(toast);
  }

  root.appendChild(shell);
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
  });
}
