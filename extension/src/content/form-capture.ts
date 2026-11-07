/**
 * @file extension/src/content/form-capture.ts
 * @brief Bắt sự kiện gửi biểu mẫu (Form Submit) để đề xuất lưu tài khoản mới vào Kho lưu trữ.
 */

import { classifyDocumentFields } from "./classifier";
import { showSaveCredentialPrompt } from "./save-prompt";

export function attachFormSubmitCapture(): void {
  document.addEventListener(
    "submit",
    (event) => {
      const form = event.target instanceof HTMLFormElement ? event.target : document;
      const { usernameInput, passwordInput } = classifyDocumentFields(form);
      const username = usernameInput?.value?.trim() || "";
      const password = passwordInput?.value || "";

      if (username && password) {
        showSaveCredentialPrompt({
          title: window.location.hostname || "Website Login",
          username,
          password,
          websiteUrl: window.location.origin,
        });
      }
    },
    true
  );
}
