/**
 * @file extension/src/content/passkey_bridge.ts
 * @brief Cầu nối sự kiện giữa `passkey_injected.ts` (Main World) và Background Service Worker (Isolated World).
 */

import { sendExtensionBridgeMessage } from "../services/protocol";

export function setupPasskeyBridge(): void {
  const chromeApi = (globalThis as Record<string, any>).chrome;
  if (chromeApi?.runtime?.getURL) {
    try {
      const script = document.createElement("script");
      script.src = chromeApi.runtime.getURL("passkey_injected.js");
      script.async = false;
      (document.head || document.documentElement).appendChild(script);
      script.remove();
    } catch {
      // Bỏ qua nếu CSP của trang hạn chế chèn script trực tiếp
    }
  }

  window.addEventListener("message", async (event) => {
    if (event.source !== window || !event.data || event.data.source !== "svm-passkey-page") {
      return;
    }

    const { requestId, action, payload } = event.data;
    if (action !== "passkey_create" && action !== "passkey_get") {
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
