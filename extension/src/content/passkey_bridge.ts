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
    let userName = String(payload?.userName || "");
    let matchedPasskeys: Array<{
      id: string;
      title: string;
      username: string;
      credentialId: string;
    }> = [];

    // Pre-flight check cho passkey_get: Kiểm tra xem kho có passkey phù hợp không
    if (action === "passkey_get") {
      const queryResp = await sendExtensionBridgeMessage<{
        unlocked: boolean;
        passkeys: Array<{ id: string; title: string; username: string; credentialId: string }>;
      }>("passkey_query", {
        rpId,
        allowCredentials: payload?.allowCredentials || [],
      });

      // Nếu kho đã mở nhưng KHÔNG có passkey nào phù hợp -> SILENT FALLBACK về trình duyệt!
      if (
        queryResp.ok &&
        queryResp.payload?.unlocked &&
        (!queryResp.payload.passkeys || queryResp.payload.passkeys.length === 0)
      ) {
        window.postMessage(
          {
            source: "svm-passkey-content",
            requestId,
            response: {
              ok: false,
              error: { code: "NOT_FOUND", message: "No matching passkey found" },
            },
          },
          window.location.origin,
        );
        return;
      }

      if (queryResp.ok && queryResp.payload?.passkeys) {
        matchedPasskeys = queryResp.payload.passkeys;
        if (matchedPasskeys.length > 0 && !userName) {
          userName = matchedPasskeys[0].username || matchedPasskeys[0].title;
        }
      }
    }

    const consentResult = await promptPasskeyConsent({
      action,
      rpId,
      rpName,
      userName,
      matchedPasskeys,
    });

    if (!consentResult.consented) {
      const errorCode = consentResult.useFallbackNative ? "USE_NATIVE_FALLBACK" : "USER_CANCELLED";
      const errorMsg = consentResult.useFallbackNative
        ? "User chose native authenticator method"
        : "User cancelled passkey prompt";

      window.postMessage(
        {
          source: "svm-passkey-content",
          requestId,
          response: {
            ok: false,
            error: { code: errorCode, message: errorMsg },
          },
        },
        window.location.origin,
      );
      return;
    }

    // Nếu người dùng chọn một passkey cụ thể từ danh sách tài khoản
    const getPayload = { ...payload };
    if (consentResult.selectedCredentialId) {
      getPayload.allowCredentials = [consentResult.selectedCredentialId];
    }
    if (consentResult.selectedVaultId) {
      getPayload.vaultId = consentResult.selectedVaultId;
    }

    const response = await sendExtensionBridgeMessage(action, getPayload);
    window.postMessage(
      {
        source: "svm-passkey-content",
        requestId,
        response,
      },
      window.location.origin,
    );
  });
}
