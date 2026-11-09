/**
 * @file extension/src/content/save-prompt.ts
 * @brief Thanh thông báo hỏi người dùng lưu thông tin đăng nhập mới (kết xuất trong Shadow DOM cô lập).
 */

import { sendExtensionBridgeMessage } from "../services/protocol";

export function showSaveCredentialPrompt(data: {
  title: string;
  username: string;
  password: string;
  websiteUrl: string;
  isUpdate?: boolean;
}): void {
  const existing = document.getElementById("svm-save-prompt-host");
  if (existing) {
    existing.remove();
  }

  const host = document.createElement("div");
  host.id = "svm-save-prompt-host";
  const shadow = host.attachShadow({ mode: "closed" });

  const style = document.createElement("style");
  style.textContent = `
    .svm-prompt {
      position: fixed;
      top: 16px;
      right: 16px;
      z-index: 2147483647;
      width: 330px;
      background: #181825;
      color: #cdd6f4;
      border: 1px solid #313244;
      border-radius: 12px;
      padding: 14px 16px;
      box-shadow: 0 12px 32px rgba(0, 0, 0, 0.45);
      font-family: -apple-system, BlinkMacSystemFont, 'Inter', 'Segoe UI', sans-serif;
      font-size: 13px;
    }
    .svm-header {
      display: flex;
      align-items: center;
      gap: 8px;
      font-weight: 700;
      margin-bottom: 6px;
    }
    .svm-sub {
      color: #a6adc8;
      font-size: 12px;
      margin-bottom: 14px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .svm-actions {
      display: flex;
      justify-content: flex-end;
      gap: 8px;
    }
    .svm-dismiss {
      background: #313244;
      color: #cdd6f4;
      border: 1px solid #45475a;
      border-radius: 6px;
      padding: 6px 12px;
      cursor: pointer;
      font-size: 12px;
      font-weight: 500;
    }
    .svm-save {
      background: #89b4fa;
      color: #11111b;
      font-weight: 600;
      border: none;
      border-radius: 6px;
      padding: 6px 14px;
      cursor: pointer;
      font-size: 12px;
    }
    @media (prefers-color-scheme: light) {
      .svm-prompt {
        background: #ffffff;
        color: #1e1e2e;
        border-color: #ccd0da;
        box-shadow: 0 10px 28px rgba(30, 30, 46, 0.16);
      }
      .svm-sub {
        color: #5c5f77;
      }
      .svm-dismiss {
        background: #e6e9ef;
        color: #1e1e2e;
        border-color: #ccd0da;
      }
      .svm-save {
        background: #1e66f5;
        color: #ffffff;
      }
    }
  `;
  shadow.appendChild(style);

  const container = document.createElement("div");
  container.className = "svm-prompt";

  const header = document.createElement("div");
  header.className = "svm-header";
  header.textContent = data.isUpdate
    ? "Update Password in Secure Vault?"
    : "Save Login to Secure Vault?";

  const subtitle = document.createElement("div");
  subtitle.className = "svm-sub";
  subtitle.textContent = `${data.username} • ${data.title}`;

  const actions = document.createElement("div");
  actions.className = "svm-actions";

  const dismissBtn = document.createElement("button");
  dismissBtn.type = "button";
  dismissBtn.className = "svm-dismiss";
  dismissBtn.textContent = "Not Now";
  dismissBtn.addEventListener("click", () => host.remove());

  const saveBtn = document.createElement("button");
  saveBtn.type = "button";
  saveBtn.className = "svm-save";
  saveBtn.textContent = data.isUpdate ? "Update Password" : "Save Login";
  saveBtn.addEventListener("click", async () => {
    saveBtn.disabled = true;
    saveBtn.textContent = data.isUpdate ? "Updating..." : "Saving...";
    await sendExtensionBridgeMessage("save_credential", data);
    host.remove();
  });

  actions.appendChild(dismissBtn);
  actions.appendChild(saveBtn);
  container.appendChild(header);
  container.appendChild(subtitle);
  container.appendChild(actions);
  shadow.appendChild(container);
  document.documentElement.appendChild(host);
}
