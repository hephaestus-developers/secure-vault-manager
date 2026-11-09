/**
 * @file extension/src/content/form-capture.ts
 * @brief Bắt sự kiện gửi biểu mẫu (Form Submit) để đề xuất lưu tài khoản mới hoặc cập nhật mật khẩu vào Kho lưu trữ.
 */

import {
  CredentialSecretPayload,
  CredentialsListPayload,
  sendExtensionBridgeMessage,
} from "../services/protocol";
import { isRecentlyAutofilled } from "./autofill";
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

      if (!username || !password) {
        return;
      }

      // 1. Kiểm tra tức thì: Nếu vừa được chính extension Autofill điền thì bỏ qua hoàn toàn
      if (isRecentlyAutofilled(username, password)) {
        return;
      }

      // 2. Đối chiếu với dữ liệu hiện có trong Vault
      void (async () => {
        try {
          const resp = await sendExtensionBridgeMessage<CredentialsListPayload>("get_credentials", {
            domain: window.location.hostname,
          });

          if (resp.ok && resp.payload?.credentials?.length) {
            const match = resp.payload.credentials.find(
              (c) => c.username.trim().toLowerCase() === username.toLowerCase()
            );

            if (match) {
              // Nếu mật khẩu trùng khớp với bản ghi đã lưu -> Đã tồn tại, bỏ qua
              if (match.password && match.password === password) {
                return;
              }

              // Truy vấn chi tiết mật khẩu giải mã để đối chiếu chính xác
              const secretResp = await sendExtensionBridgeMessage<CredentialSecretPayload>(
                "fill_credential",
                { itemId: match.id }
              );
              if (secretResp.ok && secretResp.payload?.password === password) {
                return;
              }

              // Người dùng đã đổi mật khẩu mới cho tài khoản cũ -> Hỏi "Update Password"
              showSaveCredentialPrompt({
                title: window.location.hostname || "Website Login",
                username,
                password,
                websiteUrl: window.location.origin,
                isUpdate: true,
              });
              return;
            }
          }

          // Tài khoản mới chưa từng lưu trong Vault -> Hỏi "Save Login"
          showSaveCredentialPrompt({
            title: window.location.hostname || "Website Login",
            username,
            password,
            websiteUrl: window.location.origin,
            isUpdate: false,
          });
        } catch {
          // Bỏ qua nếu có lỗi kết nối
        }
      })();
    },
    true
  );
}
