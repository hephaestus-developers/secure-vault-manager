/**
 * @file extension/src/services/protocol.ts
 * @brief Giao thức cầu nối IPC giữa Browser Extension (Popup, Content Script, Background Service Worker) và Native Messaging Host / Desktop App.
 */

export type BridgeAction =
  | "ping"
  | "get_vault_status"
  | "unlock_vault"
  | "unlock_vault_biometric"
  | "lock_vault"
  | "pairing_begin"
  | "pairing_confirm"
  | "get_credentials"
  | "fill_credential"
  | "save_credential"
  | "get_totp"
  | "update_totp"
  | "generate_password"
  | "passkey_create"
  | "passkey_get";

export interface BridgeRequest<TPayload = Record<string, unknown>> {
  requestId: string;
  action: BridgeAction;
  sessionToken?: string;
  payload?: TPayload;
}

export interface BridgeErrorDetail {
  code: string;
  message: string;
}

export interface BridgeResponse<TPayload = Record<string, unknown>> {
  requestId: string;
  type: string;
  ok: boolean;
  payload?: TPayload;
  error?: BridgeErrorDetail;
}

export interface VaultStatusPayload {
  unlocked: boolean;
  configured: boolean;
  version: string;
  bridgeListening: boolean;
  paired?: boolean;
  desktopRunning?: boolean;
  biometricAvailable?: boolean;
}

export interface UnlockVaultPayload {
  password?: string;
  browser?: string;
  version?: string;
}

export interface UnlockVaultResponsePayload {
  unlocked: boolean;
  paired?: boolean;
  clientId?: string;
  sessionToken?: string;
}

export interface PairingChallengePayload {
  pairing_code: string;
  expires_in_secs: number;
}

export interface PairingConfirmedPayload {
  paired: boolean;
  clientId: string;
  sessionToken: string;
}

export interface CredentialSummary {
  id: string;
  title: string;
  username: string;
  password?: string;
  websiteUrl: string;
  hasTotp: boolean;
  totpCode?: string;
  totpRemaining?: number;
}

export interface CredentialsListPayload {
  credentials: CredentialSummary[];
}

export interface CredentialSecretPayload {
  id: string;
  title: string;
  username: string;
  password: string;
  totpCode?: string;
  totpRemaining?: number;
}

export interface SaveCredentialRequestPayload {
  title: string;
  username: string;
  password: string;
  websiteUrl: string;
  totpSecret?: string;
}

export interface SaveCredentialResponsePayload {
  id: string;
  saved: boolean;
}

export interface GeneratedPasswordPayload {
  password: string;
  score: number;
  percent: number;
  feedback: string;
}

let requestSequence = 0;

/**
 * Tự động phát hiện tên trình duyệt đang chạy WebExtension từ navigator.
 */
export function detectCurrentBrowserName(): string {
  if (typeof navigator === "undefined" || !navigator.userAgent) {
    return "Google Chrome";
  }
  const ua = navigator.userAgent;
  if (ua.includes("Firefox/") || ua.includes("FxiOS/")) {
    return "Mozilla Firefox";
  }
  if (ua.includes("Edg/")) {
    return "Microsoft Edge";
  }
  if (ua.includes("Brave") || (navigator as any).brave) {
    return "Brave Browser";
  }
  if (ua.includes("OPR/") || ua.includes("Opera/")) {
    return "Opera";
  }
  if (ua.includes("Chrome/")) {
    return "Google Chrome";
  }
  return "Google Chrome";
}

/**
 * Tạo một phong bì yêu cầu Bridge chuẩn kèm định danh `requestId` duy nhất.
 */
export function createBridgeRequest<TPayload = Record<string, unknown>>(
  action: BridgeAction,
  payload?: TPayload,
  sessionToken?: string
): BridgeRequest<TPayload> {
  requestSequence = (requestSequence + 1) % 1_000_000;
  const currentBrowser = detectCurrentBrowserName();
  const basePayload =
    typeof payload === "object" && payload !== null
      ? { browser: currentBrowser, version: "1.0.0", ...payload }
      : ({ browser: currentBrowser, version: "1.0.0" } as unknown as TPayload);

  return {
    requestId: `req-${Date.now()}-${requestSequence}`,
    action,
    sessionToken,
    payload: basePayload,
  };
}

/**
 * Kiểm tra xem phản hồi Bridge có thành công và chứa `payload` hợp lệ hay không.
 */
export function isBridgeSuccess<TPayload>(
  resp: BridgeResponse<TPayload> | null | undefined
): resp is BridgeResponse<TPayload> & { ok: true; payload: TPayload } {
  return Boolean(resp && resp.ok && resp.payload !== undefined);
}

/**
 * Gửi yêu cầu từ Popup hoặc Content Script tới Background Service Worker.
 */
export async function sendExtensionBridgeMessage<TPayload = Record<string, unknown>>(
  action: BridgeAction,
  payload?: Record<string, unknown>
): Promise<BridgeResponse<TPayload>> {
  const req = createBridgeRequest(action, payload);
  const chromeApi = (globalThis as Record<string, any>).chrome;
  if (chromeApi?.runtime?.sendMessage) {
    return new Promise((resolve) => {
      chromeApi.runtime.sendMessage(req, (resp: BridgeResponse<TPayload>) => {
        if (chromeApi.runtime.lastError || !resp) {
          resolve({
            requestId: req.requestId,
            type: "error",
            ok: false,
            error: {
              code: "RUNTIME_ERROR",
              message: chromeApi.runtime.lastError?.message || "No response from background worker",
            },
          });
        } else {
          resolve(resp);
        }
      });
    });
  }
  return {
    requestId: req.requestId,
    type: "error",
    ok: false,
    error: { code: "UNAVAILABLE", message: "Extension runtime API is not available" },
  };
}
