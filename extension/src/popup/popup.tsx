/**
 * @file extension/src/popup/popup.tsx
 * @brief Điểm nhập điều khiển giao diện Popup của Browser Extension (Logins, Password Generator, Pairing).
 */

import { PasswordGeneratorOptions } from "../services/domain";
import {
  CredentialSecretPayload,
  CredentialSummary,
  CredentialsListPayload,
  GeneratedPasswordPayload,
  sendExtensionBridgeMessage,
  VaultStatusPayload,
} from "../services/protocol";
import { createCredentialList } from "./components/CredentialList";
import { createGeneratorTab } from "./components/GeneratorTab";
import { createHeaderBar } from "./components/HeaderBar";
import { createLockView } from "./components/LockView";

interface PopupState {
  theme: "dark" | "light";
  activeTab: "logins" | "generator";
  paired: boolean;
  unlocked: boolean;
  desktopRunning: boolean;
  biometricAvailable: boolean;
  activeOrigin: string;
  searchQuery: string;
  credentials: CredentialSummary[];
  generatedPassword: string;
  generatorOptions: PasswordGeneratorOptions;
  toastMessage: string;
}

const state: PopupState = {
  theme: "dark",
  activeTab: "logins",
  paired: true,
  unlocked: false,
  desktopRunning: false,
  biometricAvailable: false,
  activeOrigin: "",
  searchQuery: "",
  credentials: [],
  generatedPassword: "",
  generatorOptions: {
    length: 20,
    uppercase: true,
    lowercase: true,
    digits: true,
    symbols: true,
  },
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
    showToast(`${label} copied`);
  } catch {
    showToast(`Failed to copy ${label.toLowerCase()}`);
  }
}

async function loadInitialData(): Promise<void> {
  const chromeApi = (globalThis as Record<string, any>).chrome;
  if (chromeApi?.tabs?.query) {
    chromeApi.tabs.query({ active: true, currentWindow: true }, (tabs: Array<{ url?: string }>) => {
      if (tabs?.[0]?.url) {
        state.activeOrigin = tabs[0].url;
        renderPopup();
      }
    });
  }

  const statusResp = await sendExtensionBridgeMessage<VaultStatusPayload>("get_vault_status");
  if (statusResp.ok && statusResp.payload) {
    state.unlocked = Boolean(statusResp.payload.unlocked);
    state.paired = Boolean(statusResp.payload.paired ?? true);
    state.desktopRunning = Boolean(statusResp.payload.desktopRunning ?? false);
    state.biometricAvailable = Boolean(statusResp.payload.biometricAvailable);
  } else {
    state.unlocked = false;
    state.paired = false;
    state.desktopRunning = false;
    state.biometricAvailable = false;
  }

  if (state.unlocked && state.paired) {
    const credsResp = await sendExtensionBridgeMessage<CredentialsListPayload>("get_credentials", {
      domain: "",
    });
    if (credsResp.ok && credsResp.payload?.credentials) {
      state.credentials = credsResp.payload.credentials;
    } else {
      state.credentials = [];
      if (credsResp.error?.code === "NOT_PAIRED") {
        state.paired = false;
      }
    }
  } else {
    state.credentials = [];
  }

  await refreshGeneratedPassword(state.generatorOptions);
  renderPopup();
}

async function refreshGeneratedPassword(options: PasswordGeneratorOptions): Promise<void> {
  state.generatorOptions = options;
  const genResp = await sendExtensionBridgeMessage<GeneratedPasswordPayload>(
    "generate_password",
    options as unknown as Record<string, unknown>
  );
  if (genResp.ok && genResp.payload?.password) {
    state.generatedPassword = genResp.payload.password;
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
  showToast(`Filled ${item.title}`);
}

function buildFilteredCredentialList(): HTMLElement {
  const q = state.searchQuery.trim().toLowerCase();
  const filtered = state.credentials.filter(
    (c) =>
      !q ||
      c.title.toLowerCase().includes(q) ||
      c.username.toLowerCase().includes(q) ||
      (c.url || c.websiteUrl || "").toLowerCase().includes(q)
  );
  return createCredentialList({
    items: filtered,
    activeOrigin: state.activeOrigin,
    onFill: (item) => void handleFillCredential(item),
    onCopyUsername: (item) => void copyText(item.username, "Username"),
    onCopyPassword: (item) =>
      void (async () => {
        const secretResp = await sendExtensionBridgeMessage<CredentialSecretPayload>(
          "fill_credential",
          { itemId: item.id }
        );
        await copyText(secretResp.payload?.password || "", "Password");
      })(),
    onCopyTotp: (item) => {
      void (async () => {
        const code = item.totpCode || "";
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

  document.documentElement.setAttribute("data-theme", state.theme);
  root.replaceChildren();

  const shell = document.createElement("div");
  shell.className = "popupShell";

  shell.appendChild(
    createHeaderBar({
      theme: state.theme,
      paired: state.paired,
      unlocked: state.unlocked,
      desktopRunning: state.desktopRunning,
      onToggleTheme: () => {
        state.theme = state.theme === "dark" ? "light" : "dark";
        renderPopup();
      },
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

  // Khi Vault đang BỊ KHÓA hoặc Desktop App chưa chạy: Hiển thị ngay màn hình mở khóa
  if (!state.unlocked) {
    content.appendChild(
      createLockView({
        desktopRunning: state.desktopRunning,
        biometricAvailable: state.biometricAvailable,
        onUnlockPassword: async (password: string) => {
          const resp = await sendExtensionBridgeMessage<any>("unlock_vault", {
            password,
            browser: "WebExtension",
            version: "1.0.0",
          });
          if (resp.ok && resp.payload?.unlocked) {
            state.unlocked = true;
            state.paired = true;
            showToast("Đã mở khóa kho mật khẩu");
            await loadInitialData();
            return true;
          }
          return false;
        },
        onUnlockBiometric: async () => {
          const resp = await sendExtensionBridgeMessage<any>("unlock_vault_biometric", {
            browser: "WebExtension",
            version: "1.0.0",
          });
          if (resp.ok && resp.payload?.unlocked) {
            state.unlocked = true;
            state.paired = true;
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

  // Khi Vault ĐÃ MỞ KHÓA: Hiển thị Search bar và Tabs
  const listHost = document.createElement("div");

  if (state.activeTab === "logins") {
    const searchWrap = document.createElement("div");
    searchWrap.className = "searchBarWrap";
    const searchInput = document.createElement("input");
    searchInput.type = "search";
    searchInput.className = "searchInput";
    searchInput.placeholder = "Search logins by name, username, or website...";
    searchInput.value = state.searchQuery;
    searchInput.addEventListener("input", () => {
      state.searchQuery = searchInput.value;
      listHost.replaceChildren(buildFilteredCredentialList());
    });
    searchWrap.appendChild(searchInput);
    shell.appendChild(searchWrap);
  }

  const tabs = document.createElement("nav");
  tabs.className = "navTabs";

  const loginsTabBtn = document.createElement("button");
  loginsTabBtn.type = "button";
  loginsTabBtn.className = `navTabBtn ${state.activeTab === "logins" ? "navTabBtnActive" : ""}`;
  loginsTabBtn.textContent = "Logins";
  loginsTabBtn.addEventListener("click", () => {
    state.activeTab = "logins";
    renderPopup();
  });

  const genTabBtn = document.createElement("button");
  genTabBtn.type = "button";
  genTabBtn.className = `navTabBtn ${state.activeTab === "generator" ? "navTabBtnActive" : ""}`;
  genTabBtn.textContent = "Password Generator";
  genTabBtn.addEventListener("click", () => {
    state.activeTab = "generator";
    renderPopup();
  });

  tabs.appendChild(loginsTabBtn);
  tabs.appendChild(genTabBtn);
  shell.appendChild(tabs);

  if (state.activeTab === "logins") {
    listHost.appendChild(buildFilteredCredentialList());
    content.appendChild(listHost);
  } else {
    content.appendChild(
      createGeneratorTab({
        password: state.generatedPassword,
        options: state.generatorOptions,
        onRegenerate: (opts) => void refreshGeneratedPassword(opts),
        onCopy: (pw) => void copyText(pw, "Password"),
      })
    );
  }

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
    const statusResp = await sendExtensionBridgeMessage<VaultStatusPayload>("get_vault_status");
    if (statusResp.ok && statusResp.payload) {
      const newUnlocked = Boolean(statusResp.payload.unlocked);
      const newDesktop = Boolean(statusResp.payload.desktopRunning ?? false);
      const newBio = Boolean(statusResp.payload.biometricAvailable);
      if (
        newUnlocked !== state.unlocked ||
        newDesktop !== state.desktopRunning ||
        newBio !== state.biometricAvailable
      ) {
        await loadInitialData();
      }
    } else if (state.desktopRunning || state.unlocked) {
      state.desktopRunning = false;
      state.unlocked = false;
      renderPopup();
    }
  } catch {
    // Ignore silent check errors
  }
}

if (typeof document !== "undefined") {
  document.addEventListener("DOMContentLoaded", () => {
    void loadInitialData();
    setInterval(() => {
      void syncStatusSilently();
    }, 1500);
  });
}
