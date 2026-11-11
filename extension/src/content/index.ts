/**
 * @file extension/src/content/index.ts
 * @brief Điểm nhập Content Script của Browser Extension: khởi tạo Autofill, Form Capture và Passkey Bridge.
 */

import { autofillCredentialsOnPage } from "./autofill";
import { QrCropResultToastParams, showQrResultToast, startQrCropOverlay } from "./crop-overlay";
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
      message: {
        type?: string;
        username?: string;
        password?: string;
        totpCode?: string;
        itemTitle?: string;
        toast?: QrCropResultToastParams;
      },
      _sender: unknown,
      sendResponse: (resp: Record<string, unknown>) => void,
    ) => {
      if (message?.type === "SVM_FILL_ACTIVE_TAB") {
        const filled = autofillCredentialsOnPage({
          username: message.username,
          password: message.password,
          totpCode: message.totpCode,
        });
        sendResponse({ filled });
      } else if (message?.type === "SVM_START_QR_CROP") {
        startQrCropOverlay(message.itemTitle);
        sendResponse({ started: true });
      } else if (message?.type === "SVM_QR_RESULT_TOAST" && message.toast) {
        showQrResultToast(message.toast);
        sendResponse({ shown: true });
      }
      return false;
    },
  );
}
