/**
 * @file extension/src/background/pairing.ts
 * @brief Quản lý trạng thái ghép nối (Pairing) và khóa phiên giữa Browser Extension và ứng dụng Desktop.
 */

import {
  createBridgeRequest,
  detectCurrentBrowserName,
  PairingChallengePayload,
  PairingConfirmedPayload,
} from "../services/protocol";
import { BridgeTransport } from "./transport";

export interface PairingState {
  paired: boolean;
  clientId: string;
  sessionToken: string;
}

export class PairingManager {
  private state: PairingState = {
    paired: false,
    clientId: "",
    sessionToken: "",
  };

  private transport: BridgeTransport;

  constructor(transport: BridgeTransport) {
    this.transport = transport;
    this.loadState();
  }

  private loadState(): void {
    const chromeApi = (globalThis as Record<string, any>).chrome;
    if (chromeApi?.storage?.local?.get) {
      chromeApi.storage.local.get(["svm_pairing_state"], (res: any) => {
        if (res?.svm_pairing_state) {
          this.state = res.svm_pairing_state;
        }
      });
    }
  }

  private persistState(): void {
    const chromeApi = (globalThis as Record<string, any>).chrome;
    if (chromeApi?.storage?.local?.set) {
      chromeApi.storage.local.set({ svm_pairing_state: this.state });
    }
  }

  public setUnpaired(): void {
    this.state = {
      paired: false,
      clientId: "",
      sessionToken: "",
    };
    this.persistState();
  }

  public setPaired(clientId: string, sessionToken: string): void {
    this.state = {
      paired: true,
      clientId,
      sessionToken,
    };
    this.persistState();
  }

  public getState(): PairingState {
    return this.state;
  }

  public async unlockVault(password: string): Promise<boolean> {
    const req = createBridgeRequest("unlock_vault", {
      password,
      browser: detectCurrentBrowserName(),
      version: "1.0.0",
    });
    const resp = await this.transport.dispatch<any>(req);
    if (resp.ok && resp.payload?.unlocked) {
      if (resp.payload.clientId && resp.payload.sessionToken) {
        this.setPaired(resp.payload.clientId, resp.payload.sessionToken);
      }
      return true;
    }
    return false;
  }

  public async unlockBiometric(): Promise<boolean> {
    const req = createBridgeRequest("unlock_vault_biometric", {
      browser: detectCurrentBrowserName(),
      version: "1.0.0",
    });
    const resp = await this.transport.dispatch<any>(req);
    if (resp.ok && resp.payload?.unlocked) {
      if (resp.payload.clientId && resp.payload.sessionToken) {
        this.setPaired(resp.payload.clientId, resp.payload.sessionToken);
      }
      return true;
    }
    return false;
  }

  public async lockVault(): Promise<boolean> {
    const req = createBridgeRequest("lock_vault");
    const resp = await this.transport.dispatch<any>(req);
    return Boolean(resp.ok);
  }

  public async beginPairing(): Promise<PairingChallengePayload> {
    const req = createBridgeRequest("pairing_begin");
    const resp = await this.transport.dispatch<PairingChallengePayload>(req);
    if (resp.ok && resp.payload?.pairing_code) {
      return resp.payload;
    }
    return {
      pairing_code: "",
      expires_in_secs: 60,
    };
  }

  public async confirmPairing(pairingCode: string): Promise<boolean> {
    const req = createBridgeRequest("pairing_confirm", { pairingCode });
    const resp = await this.transport.dispatch<PairingConfirmedPayload>(req);
    if (resp.ok && resp.payload?.paired) {
      this.setPaired(resp.payload.clientId, resp.payload.sessionToken);
      return true;
    }
    return false;
  }
}
