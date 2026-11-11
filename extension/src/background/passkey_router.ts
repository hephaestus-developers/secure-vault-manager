/**
 * @file extension/src/background/passkey_router.ts
 * @brief Điều phối các yêu cầu tạo và ký Passkey / WebAuthn từ Content Script sang Native Host.
 */

import {
  BridgeResponse,
  createBridgeRequest,
} from "../services/protocol";
import {
  PasskeyCreateRequest,
  PasskeyCreateResponse,
  PasskeyGetRequest,
  PasskeyGetResponse,
} from "../services/webauthn";
import { BridgeTransport } from "./transport";

export class PasskeyRouter {
  private transport: BridgeTransport;

  constructor(transport: BridgeTransport) {
    this.transport = transport;
  }

  public async handleCreate(
    params: PasskeyCreateRequest
  ): Promise<BridgeResponse<PasskeyCreateResponse>> {
    const req = createBridgeRequest("passkey_create", params as unknown as Record<string, unknown>);
    return this.transport.dispatch<PasskeyCreateResponse>(req);
  }

  public async handleQuery(
    params: { rpId: string; allowCredentials?: string[] }
  ): Promise<BridgeResponse<{ unlocked: boolean; passkeys: Array<{ id: string; title: string; username: string; credentialId: string }> }>> {
    const req = createBridgeRequest("passkey_query", params as unknown as Record<string, unknown>);
    return this.transport.dispatch(req);
  }

  public async handleGet(
    params: PasskeyGetRequest
  ): Promise<BridgeResponse<PasskeyGetResponse>> {
    const req = createBridgeRequest("passkey_get", params as unknown as Record<string, unknown>);
    return this.transport.dispatch<PasskeyGetResponse>(req);
  }
}
