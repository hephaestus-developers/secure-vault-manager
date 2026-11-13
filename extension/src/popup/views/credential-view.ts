/**
 * @file extension/src/popup/views/credential-view.ts
 * @brief Giao diện hiển thị danh sách tài khoản theo domain, tìm kiếm, gắn TOTP và Passkey.
 */

import {
  CredentialSecretPayload,
  CredentialSummary,
  PasskeySummaryItem,
  sendExtensionBridgeMessage,
  UpdateTotpResponsePayload,
} from "../../services/protocol";
import { createCredentialList } from "../components/CredentialList";
import { createCloseIcon, createSearchIcon } from "../components/icons";
import { copyText, showToast } from "../components/toast";
import {
  clearPendingTotpSetup,
  fetchCredentialsForState,
  loadInitialData,
  popupStore,
} from "../state";

let searchDebounceTimer: ReturnType<typeof setTimeout> | null = null;

export async function handleFillCredential(item: CredentialSummary): Promise<void> {
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

export function handleFillPasskeyUsername(pk: PasskeySummaryItem): void {
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
  showToast(`Filled ${pk.username}`);
}

export async function launchQrCropForTab(item?: CredentialSummary): Promise<void> {
  const state = popupStore.getState();
  if (!state.activeOrigin || !/^https?:\/\//i.test(state.activeOrigin)) {
    showToast("QR scanning is only supported on http:// or https:// pages");
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
    showToast(resp.error?.message || "Failed to start QR code scanner");
  }
}

export async function applyTotpUriToVault(
  itemId: string | null,
  uri: string,
  createNew: boolean,
  onComplete?: () => void,
): Promise<void> {
  const state = popupStore.getState();
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
    showToast(resp.error?.message || "Failed to save TOTP code");
    return;
  }
  await clearPendingTotpSetup();
  popupStore.setState({ confirmOverwrite: null });
  if (resp.payload.code) {
    try {
      await navigator.clipboard.writeText(resp.payload.code);
    } catch {
      // ignore
    }
  }
  await loadInitialData();
  showToast(`Saved 2FA code (${resp.payload.code})`);
  onComplete?.();
}

export function handleAttachTotpButtonClick(item: CredentialSummary, onRerender: () => void): void {
  const state = popupStore.getState();
  if (state.pendingTotp) {
    if (item.hasTotp) {
      popupStore.setState({ confirmOverwrite: { item, pendingUri: state.pendingTotp.uri } });
      onRerender();
      return;
    }
    void applyTotpUriToVault(item.id, state.pendingTotp.uri, false, onRerender);
    return;
  }
  if (item.hasTotp) {
    popupStore.setState({ confirmOverwrite: { item } });
    onRerender();
    return;
  }
  void launchQrCropForTab(item);
}

export function renderOverwriteBanner(onRerender: () => void): HTMLElement | null {
  const state = popupStore.getState();
  const pendingConfirm = state.confirmOverwrite;
  if (!pendingConfirm) {
    return null;
  }
  const card = document.createElement("div");
  card.className = "bannerCard bannerWarn";

  const title = document.createElement("div");
  title.className = "bannerTitle";
  title.textContent = "Confirm Overwrite 2FA";

  const desc = document.createElement("div");
  desc.className = "bannerDesc";
  desc.textContent = pendingConfirm.pendingUri
    ? `Account "${pendingConfirm.item.title}" already has 2FA configured. Do you want to replace it with the newly scanned QR code?`
    : `Account "${pendingConfirm.item.title}" already has 2FA configured. Scan a new QR code to replace it?`;

  const actions = document.createElement("div");
  actions.className = "bannerActions";

  const cancelBtn = document.createElement("button");
  cancelBtn.type = "button";
  cancelBtn.className = "secondaryBtn";
  cancelBtn.textContent = "Cancel";
  cancelBtn.addEventListener("click", () => {
    popupStore.setState({ confirmOverwrite: null });
    onRerender();
  });

  const confirmBtn = document.createElement("button");
  confirmBtn.type = "button";
  confirmBtn.className = "primaryBtn";
  confirmBtn.textContent = pendingConfirm.pendingUri ? "Replace 2FA" : "Scan New QR";
  confirmBtn.addEventListener("click", () => {
    const targetItem = pendingConfirm.item;
    const uri = pendingConfirm.pendingUri;
    popupStore.setState({ confirmOverwrite: null });
    if (uri) {
      void applyTotpUriToVault(targetItem.id, uri, false, onRerender);
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

export function renderPendingTotpBanner(onRerender: () => void): HTMLElement | null {
  const state = popupStore.getState();
  const pending = state.pendingTotp;
  if (!pending) {
    return null;
  }
  const card = document.createElement("div");
  card.className = "bannerCard";

  const title = document.createElement("div");
  title.className = "bannerTitle";
  title.textContent = "Pending 2FA QR Code";

  const labelText =
    [pending.issuer, pending.account].filter(Boolean).join(" • ") || "otpauth://totp";
  const desc = document.createElement("div");
  desc.className = "bannerDesc";
  desc.textContent =
    state.credentials.length > 0
      ? `Scanned ${labelText}. Click "Attach 2FA" on an account below or create a new login.`
      : `Scanned ${labelText}. Create a new login to save this 2FA code.`;

  const actions = document.createElement("div");
  actions.className = "bannerActions";

  const dismissBtn = document.createElement("button");
  dismissBtn.type = "button";
  dismissBtn.className = "secondaryBtn";
  dismissBtn.textContent = "Dismiss";
  dismissBtn.addEventListener("click", () => {
    void clearPendingTotpSetup().then(() => onRerender());
  });

  const createBtn = document.createElement("button");
  createBtn.type = "button";
  createBtn.className = "primaryBtn";
  createBtn.textContent = "Create New Login with 2FA";
  createBtn.addEventListener("click", () => {
    void applyTotpUriToVault(null, pending.uri, true, onRerender);
  });

  actions.appendChild(dismissBtn);
  actions.appendChild(createBtn);
  card.appendChild(title);
  card.appendChild(desc);
  card.appendChild(actions);
  return card;
}

export function renderCredentialList(onRerender?: () => void): HTMLElement {
  const state = popupStore.getState();
  const trimmedQuery = state.searchQuery.trim();
  const displayScope = trimmedQuery ? `Search: "${trimmedQuery}"` : state.activeDomain;

  return createCredentialList({
    items: state.credentials,
    passkeys: state.passkeys,
    activeOrigin: state.activeOrigin,
    activeDomain: displayScope,
    hasPendingTotp: Boolean(state.pendingTotp),
    onFill: (item) => void handleFillCredential(item),
    onAttachTotpQr: (item) => handleAttachTotpButtonClick(item, onRerender || (() => {})),
    onFillPasskeyUsername: (pk) => handleFillPasskeyUsername(pk),
    onCopyPasskeyUsername: (pk) => void copyText(pk.username, "Passkey username"),
    onScanQrEmpty: () => void launchQrCropForTab(),
    onCopyUsername: (item) => void copyText(item.username, "Username"),
    onCopyPassword: (item) =>
      void (async () => {
        const secretResp = await sendExtensionBridgeMessage<CredentialSecretPayload>(
          "fill_credential",
          { itemId: item.id },
        );
        await copyText(secretResp.payload?.password || "", "Password");
      })(),
    onCopyTotp: (item) => {
      void (async () => {
        const secretResp = await sendExtensionBridgeMessage<CredentialSecretPayload>(
          "fill_credential",
          { itemId: item.id },
        );
        const code = secretResp.payload?.totpCode || item.totpCode || "";
        item.totpCode = code;
        await copyText(code, "2FA Code");
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

export function updateCredentialListMount(onRerender?: () => void): void {
  const mount = document.getElementById("svm-cred-list-mount");
  if (!mount) return;
  mount.replaceChildren(renderCredentialList(onRerender));
}

export function renderSearchBar(onSearchChange: () => void): HTMLElement {
  const state = popupStore.getState();
  const searchWrap = document.createElement("div");
  searchWrap.className = "searchBarWrap";

  const searchIcon = document.createElement("span");
  searchIcon.className = "searchBarIcon";
  searchIcon.appendChild(createSearchIcon(14));

  const searchInput = document.createElement("input");
  searchInput.type = "text";
  searchInput.className = "searchBarInput";
  searchInput.placeholder = "Search logins in vault...";
  searchInput.value = state.searchQuery;

  const clearBtn = document.createElement("button");
  clearBtn.type = "button";
  clearBtn.className = "searchClearBtn";
  clearBtn.title = "Clear search";
  clearBtn.appendChild(createCloseIcon(10));
  clearBtn.style.display = state.searchQuery ? "inline-flex" : "none";

  const triggerSearchRefresh = () => {
    if (searchDebounceTimer) {
      clearTimeout(searchDebounceTimer);
    }
    searchDebounceTimer = setTimeout(() => {
      void fetchCredentialsForState().then(() => onSearchChange());
    }, 150);
  };

  searchInput.addEventListener("input", () => {
    popupStore.setState({ searchQuery: searchInput.value });
    clearBtn.style.display = searchInput.value ? "inline-flex" : "none";
    triggerSearchRefresh();
  });

  clearBtn.addEventListener("click", () => {
    popupStore.setState({ searchQuery: "" });
    searchInput.value = "";
    clearBtn.style.display = "none";
    searchInput.focus();
    triggerSearchRefresh();
  });

  searchWrap.appendChild(searchIcon);
  searchWrap.appendChild(searchInput);
  searchWrap.appendChild(clearBtn);

  return searchWrap;
}
