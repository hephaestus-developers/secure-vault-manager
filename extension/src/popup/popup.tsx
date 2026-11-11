/**
 * @file extension/src/popup/popup.tsx
 * @brief Điểm nhập điều khiển giao diện Popup của Browser Extension (Logins theo domain).
 */

import {
  CredentialSecretPayload,
  CredentialSummary,
  CredentialsListPayload,
  detectCurrentBrowserName,
  sendExtensionBridgeMessage,
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
  activeOrigin: string;
  activeDomain: string;
  credentials: CredentialSummary[];
  toastMessage: string;
}

const state: PopupState = {
  theme: getSystemTheme(),
  unlocked: false,
  configured: false,
  desktopRunning: false,
  biometricAvailable: false,
  activeOrigin: "",
  activeDomain: "",
  credentials: [],
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

async function getActiveTabInfo(): Promise<{ origin: string; domain: string }> {
  const chromeApi = (globalThis as Record<string, any>).chrome;
  if (chromeApi?.tabs?.query) {
    try {
      const tabs = await new Promise<Array<{ url?: string }>>((resolve) => {
        chromeApi.tabs.query({ active: true, currentWindow: true }, (res: Array<{ url?: string }>) => {
          resolve(res || []);
        });
      });
      const url = tabs?.[0]?.url || "";
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
      return { origin: url, domain };
    } catch {
      return { origin: "", domain: "" };
    }
  }
  return { origin: "", domain: "" };
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
  const { origin, domain } = await getActiveTabInfo();
  state.activeOrigin = origin;
  state.activeDomain = domain;

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
    const credsResp = await sendExtensionBridgeMessage<CredentialsListPayload>("get_credentials", {
      domain: state.activeDomain,
    });
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
  } else {
    state.credentials = [];
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

function renderCredentialList(): HTMLElement {
  return createCredentialList({
    items: state.credentials,
    activeOrigin: state.activeOrigin,
    activeDomain: state.activeDomain,
    onFill: (item) => void handleFillCredential(item),
    onCopyUsername: (item) => void copyText(item.username, "Tên đăng nhập"),
    onCopyPassword: (item) =>
      void (async () => {
        const secretResp = await sendExtensionBridgeMessage<CredentialSecretPayload>(
          "fill_credential",
          { itemId: item.id }
        );
        await copyText(secretResp.payload?.password || "", "Mật khẩu");
      })(),
    onCopyTotp: (item) => {
      void (async () => {
        // Luôn yêu cầu mã TOTP thời gian thực mới nhất từ Desktop backend
        const secretResp = await sendExtensionBridgeMessage<CredentialSecretPayload>(
          "fill_credential",
          { itemId: item.id }
        );
        const code = secretResp.payload?.totpCode || item.totpCode || "";
        item.totpCode = code;
        await copyText(code, "Mã xác thực 2FA");
        const chromeApi = (globalThis as Record<string, any>).chrome;
        if (chromeApi?.tabs?.query && chromeApi?.tabs?.sendMessage && code) {
          chromeApi.tabs.query({ active: true, currentWindow: true }, (tabs: Array<{ id?: number }>) => {
            const tabId = tabs?.[0]?.id;
            if (tabId !== undefined) {
              chromeApi.tabs.sendMessage(tabId, {
                type: "SVM_FILL_ACTIVE_TAB",
                username: item.username,
                totpCode: code,
              });
            }
          });
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
      onLockVault: async () => {
        await sendExtensionBridgeMessage("lock_vault");
        state.unlocked = false;
        showToast("Kho mật khẩu đã bị khóa");
        renderPopup();
      },
    })
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
      })
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
