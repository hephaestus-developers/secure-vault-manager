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
  PairingChallengePayload,
  sendExtensionBridgeMessage,
  VaultStatusPayload,
} from "../services/protocol";
import { createCredentialList } from "./components/CredentialList";
import { createGeneratorTab } from "./components/GeneratorTab";
import { createHeaderBar } from "./components/HeaderBar";
import { createPairingView } from "./components/PairingView";

interface PopupState {
  theme: "dark" | "light";
  activeTab: "logins" | "generator" | "pairing";
  paired: boolean;
  unlocked: boolean;
  activeOrigin: string;
  searchQuery: string;
  credentials: CredentialSummary[];
  generatedPassword: string;
  generatorOptions: PasswordGeneratorOptions;
  suggestedPin: string;
  toastMessage: string;
}

const state: PopupState = {
  theme: "dark",
  activeTab: "logins",
  paired: true,
  unlocked: true,
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
  suggestedPin: "",
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
    state.unlocked = statusResp.payload.unlocked;
    state.paired = statusResp.payload.paired ?? true;
  }

  const credsResp = await sendExtensionBridgeMessage<CredentialsListPayload>("get_credentials", {
    domain: "",
  });
  if (credsResp.ok && credsResp.payload?.credentials) {
    state.credentials = credsResp.payload.credentials;
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
    onCopyTotp: (item) => void copyText(item.totpCode || "", "Verification code"),
  });
}

function createLockedVaultView(): HTMLElement {
  const card = document.createElement("div");
  card.className = "stateCard";

  const title = document.createElement("div");
  title.className = "stateTitle";
  title.textContent = "Vault is Locked";

  const desc = document.createElement("div");
  desc.className = "stateDesc";
  desc.textContent =
    "Unlock Secure Vault Manager on your desktop to view and autofill your saved logins.";

  const actions = document.createElement("div");
  actions.style.cssText = "display: flex; gap: 8px; justify-content: center; margin-top: 4px;";

  const unlockBtn = document.createElement("button");
  unlockBtn.type = "button";
  unlockBtn.className = "primaryBtn";
  unlockBtn.textContent = "Open Desktop App";
  unlockBtn.addEventListener("click", async () => {
    await sendExtensionBridgeMessage("launch_desktop", {});
    showToast("Opening Desktop App...");
  });

  const refreshBtn = document.createElement("button");
  refreshBtn.type = "button";
  refreshBtn.className = "iconBtn";
  refreshBtn.style.cssText = "width: auto; padding: 6px 12px;";
  refreshBtn.textContent = "Refresh";
  refreshBtn.addEventListener("click", () => {
    void loadInitialData();
  });

  actions.appendChild(unlockBtn);
  actions.appendChild(refreshBtn);

  card.appendChild(title);
  card.appendChild(desc);
  card.appendChild(actions);
  return card;
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
      onToggleTheme: () => {
        state.theme = state.theme === "dark" ? "light" : "dark";
        renderPopup();
      },
      onLaunchDesktop: async () => {
        await sendExtensionBridgeMessage("launch_desktop", {});
        showToast("Opening Desktop App...");
      },
      onOpenPairing: async () => {
        const resp = await sendExtensionBridgeMessage<PairingChallengePayload>("pairing_begin");
        state.suggestedPin = resp.payload?.pairing_code || "482915";
        state.activeTab = "pairing";
        renderPopup();
      },
    })
  );

  const listHost = document.createElement("div");

  if (state.activeTab === "logins" && state.unlocked) {
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

  const content = document.createElement("main");
  content.className = "contentArea";

  if (state.activeTab === "logins") {
    if (!state.unlocked) {
      content.appendChild(createLockedVaultView());
    } else {
      listHost.appendChild(buildFilteredCredentialList());
      content.appendChild(listHost);
    }
  } else if (state.activeTab === "generator") {
    content.appendChild(
      createGeneratorTab({
        password: state.generatedPassword,
        options: state.generatorOptions,
        onRegenerate: (opts) => void refreshGeneratedPassword(opts),
        onCopy: (pw) => void copyText(pw, "Password"),
      })
    );
  } else {
    content.appendChild(
      createPairingView({
        suggestedPin: state.suggestedPin,
        onConfirm: async (pin) => {
          const resp = await sendExtensionBridgeMessage("pairing_confirm", { pairingCode: pin });
          if (resp.ok) {
            state.activeTab = "logins";
            showToast("Connected to Desktop App");
          } else {
            showToast("Invalid 6-digit code");
          }
        },
        onCancel: () => {
          state.activeTab = "logins";
          renderPopup();
        },
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

if (typeof document !== "undefined") {
  document.addEventListener("DOMContentLoaded", () => {
    void loadInitialData();
  });
}
