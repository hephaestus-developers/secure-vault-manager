/**
 * @file extension/src/background/transport.ts
 * @brief Bộ điều phối giao vận hợp nhất qua Native Messaging và bộ đệm trạng thái ngoại tuyến.
 */

import { BridgeRequest, BridgeResponse, CredentialSummary } from "../services/protocol";
import { NativeMessagingClient } from "./native_client";

export class BridgeTransport {
  private nativeClient = new NativeMessagingClient();
  private cachedItems: CredentialSummary[] = [];
  private unlocked = false;

  public async dispatch<TPayload = Record<string, unknown>>(
    request: BridgeRequest<any>,
  ): Promise<BridgeResponse<TPayload>> {
    const nativeResp = await this.nativeClient.send<TPayload>(request);
    if (nativeResp) {
      if (request.action === "get_vault_status" && nativeResp.ok && nativeResp.payload) {
        this.unlocked = Boolean((nativeResp.payload as any).unlocked);
      }
      return nativeResp;
    }

    return this.handleOfflineFallback(request) as Promise<BridgeResponse<TPayload>>;
  }

  private async handleOfflineFallback(request: BridgeRequest<any>): Promise<BridgeResponse<any>> {
    this.unlocked = false;
    this.cachedItems = [];

    switch (request.action) {
      case "ping":
      case "get_vault_status":
        return {
          requestId: request.requestId,
          type: "vault_status",
          ok: true,
          payload: {
            unlocked: false,
            desktopRunning: false,
            configured: false,
            version: "0.1.0",
            bridgeListening: false,
          },
        };

      case "get_credentials":
      case "fill_credential":
      case "save_credential":
      case "get_totp":
        return {
          requestId: request.requestId,
          type: "error",
          ok: false,
          error: {
            code: "VAULT_LOCKED",
            message: "Vault is locked or Desktop App is not connected.",
          },
        };

      default:
        return {
          requestId: request.requestId,
          type: "error",
          ok: false,
          error: {
            code: "DESKTOP_DISCONNECTED",
            message: "Desktop App is not running or not responding.",
          },
        };
    }
  }
}
