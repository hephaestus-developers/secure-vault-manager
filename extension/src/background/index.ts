/**
 * @file extension/src/background/index.ts
 * @brief Điểm nhập Background Service Worker (Manifest V3) điều phối Autofill, TOTP và Passkey.
 */

import { BridgeRequest, createBridgeRequest, detectCurrentBrowserName } from "../services/protocol";
import { PasskeyRouter } from "./passkey_router";
import { clearCropSession, handleQrCropSelected, handleStartQrCrop } from "./qr_crop_handler";
import { BridgeTransport } from "./transport";

const transport = new BridgeTransport();
const passkeyRouter = new PasskeyRouter(transport);

const chromeApi = (globalThis as Record<string, any>).chrome;

if (chromeApi?.runtime?.onMessage) {
  chromeApi.runtime.onMessage.addListener(
    (message: BridgeRequest<any>, _sender: unknown, sendResponse: (response: unknown) => void) => {
      if (!message || typeof message.action !== "string") {
        return false;
      }

      void (async () => {
        if (message.action === "unlock_vault") {
          const password = String(message.payload?.password || "");
          const req = createBridgeRequest("unlock_vault", {
            password,
            browser: detectCurrentBrowserName(),
            version: "0.1.0",
          });
          const resp = await transport.dispatch<any>(req);
          const unlocked = Boolean(resp.ok && resp.payload?.unlocked);
          sendResponse({
            requestId: message.requestId,
            type: "vault_unlocked",
            ok: unlocked,
            payload: { unlocked },
            error: unlocked
              ? undefined
              : resp.error || {
                  code: "DESKTOP_NOT_RUNNING",
                  message: "Ứng dụng Desktop chưa khởi động hoặc mật khẩu không chính xác.",
                },
          });
          return;
        }

        if (message.action === "unlock_vault_biometric") {
          const req = createBridgeRequest("unlock_vault_biometric", {
            browser: detectCurrentBrowserName(),
            version: "0.1.0",
          });
          const resp = await transport.dispatch<any>(req);
          const unlocked = Boolean(resp.ok && resp.payload?.unlocked);
          sendResponse({
            requestId: message.requestId,
            type: "vault_unlocked",
            ok: unlocked,
            payload: { unlocked },
            error: unlocked
              ? undefined
              : resp.error || {
                  code: "BIOMETRIC_FAILED",
                  message: "Xác thực sinh trắc học không thành công hoặc Desktop chưa khởi động.",
                },
          });
          return;
        }

        if (message.action === "lock_vault") {
          const req = createBridgeRequest("lock_vault");
          const resp = await transport.dispatch<any>(req);
          const ok = Boolean(resp.ok);
          sendResponse({
            requestId: message.requestId,
            type: "vault_locked",
            ok,
            payload: { unlocked: !ok },
          });
          return;
        }

        if (message.action === "passkey_query") {
          const resp = await passkeyRouter.handleQuery(message.payload);
          sendResponse(resp);
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

        if (message.action === "start_qr_crop") {
          const resp = await handleStartQrCrop(message);
          sendResponse(resp);
          return;
        }

        if (message.action === "qr_crop_selected") {
          const senderTab = (
            _sender as {
              tab?: { id?: number; windowId?: number; url?: string };
            }
          )?.tab;
          const resp = await handleQrCropSelected(senderTab, message, transport);
          sendResponse(resp);
          return;
        }

        if (message.action === "qr_crop_cancelled") {
          const senderTab = (_sender as { tab?: { id?: number } })?.tab;
          await clearCropSession(senderTab?.id);
          sendResponse({
            ok: true,
            type: "qr_crop_cancelled",
            payload: { cancelled: true },
          });
          return;
        }

        const resp = await transport.dispatch(message);
        sendResponse(resp);
      })();

      return true;
    },
  );
}
