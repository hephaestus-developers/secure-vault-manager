/**
 * @file extension/src/content/index.ts
 * @brief Điểm nhập Content Script của Browser Extension: khởi tạo Autofill, Form Capture và Passkey Bridge.
 */

import { autofillCredentialsOnPage } from "./autofill";
import { attachFormSubmitCapture } from "./form-capture";
import { setupInlineAutofillOverlay } from "./overlay";
import { setupPasskeyBridge } from "./passkey_bridge";

setupInlineAutofillOverlay();
attachFormSubmitCapture();
setupPasskeyBridge();

const chromeApi = (globalThis as Record<string, any>).chrome;
if (chromeApi?.runtime?.onMessage) {
  chromeApi.runtime.onMessage.addListener(
    (
      message: { type?: string; username?: string; password?: string; totpCode?: string },
      _sender: unknown,
      sendResponse: (resp: { filled: boolean }) => void
    ) => {
      if (message?.type === "SVM_FILL_ACTIVE_TAB") {
        const filled = autofillCredentialsOnPage({
          username: message.username,
          password: message.password,
          totpCode: message.totpCode,
        });
        sendResponse({ filled });
      }
      return false;
    }
  );
}
