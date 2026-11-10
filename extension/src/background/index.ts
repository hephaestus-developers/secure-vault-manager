/**
 * @file extension/src/background/index.ts
 * @brief Điểm nhập Background Service Worker (Manifest V3) điều phối Autofill, TOTP, Pairing và Passkey.
 */

import { BridgeRequest } from "../services/protocol";
import { PairingManager } from "./pairing";
import { PasskeyRouter } from "./passkey_router";
import { BridgeTransport } from "./transport";

const transport = new BridgeTransport();
const pairingManager = new PairingManager(transport);
const passkeyRouter = new PasskeyRouter(transport);

const chromeApi = (globalThis as Record<string, any>).chrome;

if (chromeApi?.runtime?.onMessage) {
  chromeApi.runtime.onMessage.addListener(
    (
      message: BridgeRequest<any>,
      _sender: unknown,
      sendResponse: (response: unknown) => void
    ) => {
      if (!message || typeof message.action !== "string") {
        return false;
      }

      void (async () => {
        if (message.action === "unlock_vault") {
          const password = String(message.payload?.password || "");
          const ok = await pairingManager.unlockVault(password);
          sendResponse({
            requestId: message.requestId,
            type: "vault_unlocked",
            ok,
            payload: { unlocked: ok },
          });
          return;
        }

        if (message.action === "unlock_vault_biometric") {
          const ok = await pairingManager.unlockBiometric();
          sendResponse({
            requestId: message.requestId,
            type: "vault_unlocked",
            ok,
            payload: { unlocked: ok },
          });
          return;
        }

        if (message.action === "lock_vault") {
          const ok = await pairingManager.lockVault();
          sendResponse({
            requestId: message.requestId,
            type: "vault_locked",
            ok,
            payload: { unlocked: !ok },
          });
          return;
        }

        if (message.action === "pairing_begin") {
          const payload = await pairingManager.beginPairing();
          sendResponse({
            requestId: message.requestId,
            type: "pairing_challenge",
            ok: true,
            payload,
          });
          return;
        }

        if (message.action === "pairing_confirm") {
          const code = String(message.payload?.pairingCode || message.payload?.code || "");
          const ok = await pairingManager.confirmPairing(code);
          sendResponse({
            requestId: message.requestId,
            type: "pairing_confirmed",
            ok,
            payload: pairingManager.getState(),
          });
          return;
        }

        if (message.action === "passkey_create") {
          const resp = await passkeyRouter.handleCreate(message.payload);
          sendResponse(resp);
          return;
        }

        if (message.action === "passkey_get") {
          const resp = await passkeyRouter.handleGet(message.payload);
          sendResponse(resp);
          return;
        }

        const resp = await transport.dispatch(message);
        sendResponse(resp);
      })();

      return true;
    }
  );
}
