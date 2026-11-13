/**
 * @file extension/src/popup/state.ts
 * @brief Quản lý trạng thái Popup bằng mô hình Observer Pattern (Type-Safe Store & Pub/Sub).
 */

import { isDomainMatch } from "../services/domain";
import {
  CredentialSummary,
  CredentialsListPayload,
  PasskeyQueryResponsePayload,
  PasskeySummaryItem,
  PENDING_TOTP_STORAGE_KEY,
  PENDING_TOTP_TTL_MS,
  PendingTotpSetup,
  sendExtensionBridgeMessage,
  VaultStatusPayload,
} from "../services/protocol";

export interface PopupState {
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
}

export type StateListener = (state: PopupState) => void;

export function getSystemTheme(): "dark" | "light" {
  if (typeof window !== "undefined" && window.matchMedia) {
    return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
  }
  return "dark";
}

export class PopupStore {
  private state: PopupState;
  private listeners: Set<StateListener> = new Set();
  private isNotifying = false;

  constructor() {
    this.state = {
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
    };
  }

  public getState(): PopupState {
    return this.state;
  }

  public setState(partial: Partial<PopupState>, silent = false): void {
    let hasChanged = false;
    for (const key of Object.keys(partial) as Array<keyof PopupState>) {
      if (this.state[key] !== partial[key]) {
        hasChanged = true;
        break;
      }
    }
    if (!hasChanged) {
      return;
    }
    this.state = { ...this.state, ...partial };
    if (!silent && !this.isNotifying) {
      this.isNotifying = true;
      try {
        this.notify();
      } finally {
        this.isNotifying = false;
      }
    }
  }

  public subscribe(listener: StateListener): () => void {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  }

  private notify(): void {
    for (const listener of this.listeners) {
      listener(this.state);
    }
  }
}

export const popupStore = new PopupStore();

export async function getActiveTabInfo(): Promise<{
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

export async function loadPendingTotpSetup(): Promise<PendingTotpSetup | null> {
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

export async function clearPendingTotpSetup(): Promise<void> {
  popupStore.setState({ pendingTotp: null });
  const chromeApi = (globalThis as Record<string, any>).chrome;
  try {
    if (chromeApi?.storage?.session?.remove) {
      await chromeApi.storage.session.remove([PENDING_TOTP_STORAGE_KEY]);
    }
    if (chromeApi?.storage?.local?.remove) {
      await chromeApi.storage.local.remove([PENDING_TOTP_STORAGE_KEY]);
    }
  } catch {
    // Dự phòng
  }
}

export function matchesDomain(item: CredentialSummary, domain: string): boolean {
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

export async function fetchCredentialsForState(): Promise<void> {
  const state = popupStore.getState();
  if (!state.unlocked || !state.configured) {
    popupStore.setState({ credentials: [], passkeys: [] });
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

  let updatedCredentials: CredentialSummary[] = [];
  if (credsResp.ok && credsResp.payload?.credentials) {
    let items = credsResp.payload.credentials;
    if (!trimmedQuery) {
      items = state.activeDomain
        ? items.filter((item) => matchesDomain(item, state.activeDomain))
        : [];
    }
    updatedCredentials = items;
  }

  let updatedPasskeys: PasskeySummaryItem[] = [];
  if (passkeysResp?.ok && passkeysResp.payload?.passkeys) {
    updatedPasskeys = passkeysResp.payload.passkeys;
  }

  popupStore.setState({
    credentials: updatedCredentials,
    passkeys: updatedPasskeys,
  });
}

export async function loadInitialData(): Promise<void> {
  const { tabId, windowId, origin, domain } = await getActiveTabInfo();
  const pendingTotp = await loadPendingTotpSetup();

  let configured = false;
  let unlocked = false;
  let desktopRunning = false;
  let biometricAvailable = false;

  const statusResp = await sendExtensionBridgeMessage<VaultStatusPayload>("get_vault_status");
  if (statusResp.ok && statusResp.payload) {
    configured = Boolean(statusResp.payload.configured);
    unlocked = configured && Boolean(statusResp.payload.unlocked);
    desktopRunning = Boolean(statusResp.payload.desktopRunning ?? false);
    biometricAvailable = configured && Boolean(statusResp.payload.biometricAvailable);
  }

  popupStore.setState({
    activeTabId: tabId,
    activeWindowId: windowId,
    activeOrigin: origin,
    activeDomain: domain,
    pendingTotp,
    configured,
    unlocked,
    desktopRunning,
    biometricAvailable,
  });

  await fetchCredentialsForState();
}
