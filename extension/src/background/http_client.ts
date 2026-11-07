/**
 * @file extension/src/background/http_client.ts
 * @brief Kênh dự phòng HTTP Loopback (`127.0.0.1:12519`) khi Native Messaging bị hạn chế bởi môi trường đóng gói.
 */

import { BridgeRequest, BridgeResponse } from "../services/protocol";

const LOOPBACK_ENDPOINT = "http://127.0.0.1:12519/v1/bridge";

export class HttpLoopbackClient {
  public async send<TPayload = Record<string, unknown>>(
    request: BridgeRequest<any>,
    timeoutMs = 2000
  ): Promise<BridgeResponse<TPayload> | null> {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), timeoutMs);

    try {
      const resp = await fetch(LOOPBACK_ENDPOINT, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          ...(request.sessionToken ? { "X-SVM-Session": request.sessionToken } : {}),
        },
        body: JSON.stringify(request),
        signal: controller.signal,
      });
      clearTimeout(timer);
      if (!resp.ok) return null;
      return (await resp.json()) as BridgeResponse<TPayload>;
    } catch {
      clearTimeout(timer);
      return null;
    }
  }
}
