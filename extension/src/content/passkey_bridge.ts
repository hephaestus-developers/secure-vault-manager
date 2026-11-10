/**
 * @file extension/src/content/passkey_bridge.ts
 * @brief Cầu nối sự kiện giữa `passkey_injected.ts` (Main World) và Background Service Worker (Isolated World).
 */

import { sendExtensionBridgeMessage } from "../services/protocol";
import { dismissPasskeyConsent, promptPasskeyConsent } from "./passkey_modal";

export function setupPasskeyBridge(): void {
  window.addEventListener("message", async (event) => {
    if (event.source !== window || !event.data || event.data.source !== "svm-passkey-page") {
      return;
    }

    const { requestId, action, payload } = event.data;
    if (action === "passkey_cancel") {
      dismissPasskeyConsent();
      return;
    }

    if (action !== "passkey_create" && action !== "passkey_get") {
      return;
    }

    const rpId = String(payload?.rpId || window.location.hostname);
    const rpName = String(payload?.rpName || rpId);
    const userName = String(payload?.userName || "");

    const consented = await promptPasskeyConsent({
      action,
      rpId,
      rpName,
      userName,
    });

    if (!consented) {
      window.postMessage(
        {
          source: "svm-passkey-content",
          requestId,
          response: {
            ok: false,
            error: { code: "USER_CANCELLED", message: "User cancelled passkey prompt" },
          },
        },
        window.location.origin
      );
      return;
    }

    const response = await sendExtensionBridgeMessage(action, payload);
    window.postMessage(
      {
        source: "svm-passkey-content",
        requestId,
        response,
      },
      window.location.origin
    );
  });
}
