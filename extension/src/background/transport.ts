/**
 * @file extension/src/background/transport.ts
 * @brief Bộ điều phối giao vận hợp nhất: ưu tiên Native Messaging, chuyển dự phòng sang HTTP Loopback và bộ đệm ngoại tuyến.
 */

import {
  BridgeRequest,
  BridgeResponse,
  CredentialSummary,
} from "../services/protocol";
import { generatePasswordLocal, generateTotpLocal } from "../services/wasm-crypto";
import { HttpLoopbackClient } from "./http_client";
import { NativeMessagingClient } from "./native_client";

export class BridgeTransport {
  private nativeClient = new NativeMessagingClient();
  private httpClient = new HttpLoopbackClient();
  private cachedItems: CredentialSummary[] = [];
  private unlocked = false;

  public async dispatch<TPayload = Record<string, unknown>>(
    request: BridgeRequest<any>
  ): Promise<BridgeResponse<TPayload>> {
    const nativeResp = await this.nativeClient.send<TPayload>(request);
    if (nativeResp) {
      if (request.action === "get_vault_status" && nativeResp.ok && nativeResp.payload) {
        this.unlocked = Boolean((nativeResp.payload as any).unlocked);
      }
      return nativeResp;
    }

    const httpResp = await this.httpClient.send<TPayload>(request);
    if (httpResp) {
      if (request.action === "get_vault_status" && httpResp.ok && httpResp.payload) {
        this.unlocked = Boolean((httpResp.payload as any).unlocked);
      }
      return httpResp;
    }

    return this.handleOfflineFallback(request) as Promise<BridgeResponse<TPayload>>;
  }

  private async handleOfflineFallback(request: BridgeRequest<any>): Promise<BridgeResponse<any>> {
    this.unlocked = false;
    this.cachedItems = [];
    const payload = request.payload || {};

    switch (request.action) {
      case "ping":
      case "get_vault_status":
        return {
          requestId: request.requestId,
          type: "vault_status",
          ok: true,
          payload: {
            unlocked: false,
            configured: true,
            version: "0.1.0",
            bridgeListening: false,
            paired: false,
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
            message: "Kho dữ liệu đang bị khóa hoặc chưa kết nối ứng dụng Desktop.",
          },
        };

      case "generate_password": {
        const generated = await generatePasswordLocal({
          length: Number(payload.length || 20),
          uppercase: payload.uppercase !== false,
          lowercase: payload.lowercase !== false,
          digits: payload.digits !== false,
          symbols: payload.symbols !== false,
        });
        return {
          requestId: request.requestId,
          type: "generated_password",
          ok: true,
          payload: {
            password: generated.password,
            score: generated.strength.score,
            percent: generated.strength.percent,
            feedback: generated.strength.feedback,
          },
        };
      }

      default:
        return {
          requestId: request.requestId,
          type: "error",
          ok: false,
          error: {
            code: "DESKTOP_DISCONNECTED",
            message: "Ứng dụng Desktop đang tắt hoặc không phản hồi.",
          },
        };
    }
  }
}
