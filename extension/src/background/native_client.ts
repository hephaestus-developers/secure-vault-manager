/**
 * @file extension/src/background/native_client.ts
 * @brief Kết nối Native Messaging (`chrome.runtime.connectNative`) tới tiến trình `svm-native-host`.
 */

import { BridgeRequest, BridgeResponse } from "../services/protocol";

const NATIVE_HOST_NAME = "io.hephaestus.secure_vault_manager";
const INTERACTIVE_NATIVE_ACTIONS = new Set([
  "unlock_vault",
  "unlock_vault_biometric",
  "passkey_create",
  "passkey_get",
]);

export class NativeMessagingClient {
  private port: any = null;
  private pending = new Map<
    string,
    { resolve: (resp: BridgeResponse<any>) => void; timer: ReturnType<typeof setTimeout> }
  >();

  public isConnected(): boolean {
    return this.port !== null;
  }

  public connect(): boolean {
    if (this.port) return true;
    const chromeApi = (globalThis as Record<string, any>).chrome;
    if (!chromeApi?.runtime?.connectNative) {
      return false;
    }

    try {
      this.port = chromeApi.runtime.connectNative(NATIVE_HOST_NAME);
      this.port.onMessage.addListener((msg: BridgeResponse<any>) => {
        if (msg && msg.requestId && this.pending.has(msg.requestId)) {
          const entry = this.pending.get(msg.requestId)!;
          clearTimeout(entry.timer);
          this.pending.delete(msg.requestId);
          entry.resolve(msg);
        }
      });
      this.port.onDisconnect.addListener(() => {
        this.port = null;
        for (const [reqId, entry] of this.pending.entries()) {
          clearTimeout(entry.timer);
          entry.resolve({
            requestId: reqId,
            type: "error",
            ok: false,
            error: {
              code: "NATIVE_DISCONNECTED",
              message: "Native messaging host disconnected",
            },
          });
        }
        this.pending.clear();
      });
      return true;
    } catch {
      this.port = null;
      return false;
    }
  }

  public async send<TPayload = Record<string, unknown>>(
    request: BridgeRequest<any>,
    timeoutMs?: number,
  ): Promise<BridgeResponse<TPayload> | null> {
    if (!this.connect() || !this.port) {
      return null;
    }
    const effectiveTimeout =
      timeoutMs ?? (INTERACTIVE_NATIVE_ACTIONS.has(request.action) ? 60000 : 8000);

    return new Promise((resolve) => {
      const timer = setTimeout(() => {
        this.pending.delete(request.requestId);
        resolve(null);
      }, effectiveTimeout);

      this.pending.set(request.requestId, { resolve, timer });
      try {
        this.port.postMessage(request);
      } catch {
        clearTimeout(timer);
        this.pending.delete(request.requestId);
        this.port = null;
        resolve(null);
      }
    });
  }
}
