/**
 * @file extension/src/content/save-prompt.ts
 * @brief Thanh thông báo hỏi người dùng lưu thông tin đăng nhập mới / cập nhật (kết xuất trong Shadow DOM cô lập).
 */

import { sendExtensionBridgeMessage } from "../services/protocol";

export interface SavePromptOptions {
  itemId?: string;
  title: string;
  username: string;
  password: string;
  websiteUrl: string;
  isUpdate?: boolean;
  vaultId?: string;
}

export async function showSaveCredentialPrompt(data: SavePromptOptions): Promise<void> {
  const existing = document.getElementById("svm-save-prompt-host");
  if (existing) {
    existing.remove();
  }

  // Lấy danh sách Vaults nếu có nhiều hơn 1 kho
  let availableVaults: Array<{ id: string; name: string; isDefault?: boolean }> = [];
  try {
    const vaultsResp = await sendExtensionBridgeMessage<any>("get_vaults");
    if (vaultsResp.ok && Array.isArray(vaultsResp.payload?.vaults)) {
      availableVaults = vaultsResp.payload.vaults;
    }
  } catch {
    // Bỏ qua
  }

  const host = document.createElement("div");
  host.id = "svm-save-prompt-host";
  const shadow = host.attachShadow({ mode: "closed" });

  const style = document.createElement("style");
  style.textContent = `
    :host {
      all: initial;
    }
    .svm-prompt {
      position: fixed;
      top: 16px;
      right: 16px;
      z-index: 2147483647;
      width: 360px;
      background: #181825;
      color: #cdd6f4;
      border: 1px solid #313244;
      border-radius: 14px;
      padding: 16px 18px;
      box-shadow: 0 16px 40px rgba(0, 0, 0, 0.55), 0 0 0 1px rgba(255, 255, 255, 0.05);
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
      font-size: 13px;
      line-height: 1.5;
      animation: svmSlideIn 0.22s cubic-bezier(0.16, 1, 0.3, 1);
    }
    @keyframes svmSlideIn {
      from {
        opacity: 0;
        transform: translateY(-16px) scale(0.96);
      }
      to {
        opacity: 1;
        transform: translateY(0) scale(1);
      }
    }
    .svm-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
      font-weight: 700;
      font-size: 14px;
      margin-bottom: 12px;
      color: #89b4fa;
    }
    .svm-header-brand {
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .svm-close-btn {
      background: transparent;
      border: none;
      color: #6c7086;
      cursor: pointer;
      font-size: 16px;
      line-height: 1;
      padding: 4px;
      border-radius: 4px;
    }
    .svm-close-btn:hover {
      color: #cdd6f4;
      background: #313244;
    }
    .svm-form-rows {
      display: flex;
      flex-direction: column;
      gap: 10px;
      margin-bottom: 14px;
    }
    .svm-row {
      display: flex;
      flex-direction: column;
      gap: 4px;
    }
    .svm-label {
      font-size: 11.5px;
      font-weight: 600;
      color: #a6adc8;
    }
    .svm-input-wrapper {
      position: relative;
      display: flex;
      align-items: center;
    }
    .svm-input {
      width: 100%;
      box-sizing: border-box;
      background: #11111b;
      border: 1px solid #313244;
      border-radius: 8px;
      padding: 8px 10px;
      color: #cdd6f4;
      font-size: 13px;
      outline: none;
      font-family: inherit;
    }
    .svm-input:focus {
      border-color: #89b4fa;
      box-shadow: 0 0 0 2px rgba(137, 180, 250, 0.2);
    }
    .svm-pwd-input {
      padding-right: 36px;
    }
    .svm-toggle-eye {
      position: absolute;
      right: 8px;
      background: transparent;
      border: none;
      color: #6c7086;
      cursor: pointer;
      padding: 4px;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .svm-toggle-eye:hover {
      color: #cdd6f4;
    }
    .svm-sub {
      color: #6c7086;
      font-size: 11px;
      margin-top: 2px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .svm-actions {
      display: flex;
      align-items: center;
      justify-content: flex-end;
      gap: 8px;
      margin-top: 6px;
    }
    .svm-btn {
      padding: 7px 14px;
      border-radius: 6px;
      cursor: pointer;
      font-size: 12.5px;
      font-weight: 600;
      border: none;
      transition: background 0.15s ease;
    }
    .svm-dismiss {
      background: #313244;
      color: #cdd6f4;
      border: 1px solid #45475a;
    }
    .svm-dismiss:hover {
      background: #45475a;
    }
    .svm-save {
      background: #89b4fa;
      color: #11111b;
    }
    .svm-save:hover {
      background: #b4befe;
    }
    .svm-save:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    @media (prefers-color-scheme: light) {
      .svm-prompt {
        background: #ffffff;
        color: #1e1e2e;
        border-color: #ccd0da;
        box-shadow: 0 16px 40px rgba(30, 30, 46, 0.18), 0 0 0 1px rgba(0, 0, 0, 0.05);
      }
      .svm-header {
        color: #1e66f5;
      }
      .svm-label {
        color: #5c5f77;
      }
      .svm-input {
        background: #f4f5f8;
        border-color: #ccd0da;
        color: #1e1e2e;
      }
      .svm-dismiss {
        background: #e6e9ef;
        color: #1e1e2e;
        border-color: #ccd0da;
      }
      .svm-dismiss:hover {
        background: #dce0e8;
      }
      .svm-save {
        background: #1e66f5;
        color: #ffffff;
      }
      .svm-save:hover {
        background: #044dc8;
      }
    }
  `;
  shadow.appendChild(style);

  const container = document.createElement("div");
  container.className = "svm-prompt";

  const header = document.createElement("div");
  header.className = "svm-header";
  header.innerHTML = `
    <div class="svm-header-brand">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
        <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>
      </svg>
      <span>${data.isUpdate ? "Cập nhật Mật khẩu" : "Lưu vào Secure Vault?"}</span>
    </div>
    <button class="svm-close-btn" title="Đóng">✕</button>
  `;
  header.querySelector(".svm-close-btn")?.addEventListener("click", () => host.remove());

  const formRows = document.createElement("div");
  formRows.className = "svm-form-rows";

  // Dòng Tên đăng nhập (cho phép sửa)
  const userRow = document.createElement("div");
  userRow.className = "svm-row";
  userRow.innerHTML = `
    <label class="svm-label">Tên đăng nhập / Email</label>
    <div class="svm-input-wrapper">
      <input type="text" class="svm-input" id="svm-save-username" value="${escapeHtml(data.username)}" />
    </div>
  `;
  formRows.appendChild(userRow);

  // Dòng Mật khẩu (có nút con mắt toggle)
  const pwdRow = document.createElement("div");
  pwdRow.className = "svm-row";
  pwdRow.innerHTML = `
    <label class="svm-label">Mật khẩu</label>
    <div class="svm-input-wrapper">
      <input type="password" class="svm-input svm-pwd-input" id="svm-save-password" value="${escapeHtml(data.password)}" />
      <button type="button" class="svm-toggle-eye" id="svm-toggle-eye" title="Xem mật khẩu">
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
          <circle cx="12" cy="12" r="3"/>
        </svg>
      </button>
    </div>
    <div class="svm-sub" title="${escapeHtml(data.websiteUrl)}">${escapeHtml(data.title)}</div>
  `;
  formRows.appendChild(pwdRow);

  // Dòng Chọn Kho nếu có từ 2 Vault trở lên
  if (availableVaults.length >= 2) {
    const vaultRow = document.createElement("div");
    vaultRow.className = "svm-row";
    vaultRow.innerHTML = `
      <label class="svm-label">Lưu vào kho</label>
      <div class="svm-input-wrapper">
        <select class="svm-input" id="svm-save-vault" style="cursor: pointer;">
          ${availableVaults
            .map(
              (v) =>
                `<option value="${escapeHtml(v.id)}" ${v.isDefault ? "selected" : ""}>${escapeHtml(v.name)}</option>`,
            )
            .join("")}
        </select>
      </div>
    `;
    formRows.appendChild(vaultRow);
  }

  const actions = document.createElement("div");
  actions.className = "svm-actions";

  const dismissBtn = document.createElement("button");
  dismissBtn.type = "button";
  dismissBtn.className = "svm-btn svm-dismiss";
  dismissBtn.textContent = "Để sau";
  dismissBtn.addEventListener("click", () => host.remove());

  const saveBtn = document.createElement("button");
  saveBtn.type = "button";
  saveBtn.className = "svm-btn svm-save";
  saveBtn.textContent = data.isUpdate ? "Cập nhật" : "Lưu mật khẩu";
  saveBtn.addEventListener("click", async () => {
    saveBtn.disabled = true;
    saveBtn.textContent = data.isUpdate ? "Đang cập nhật..." : "Đang lưu...";

    const finalUsername =
      formRows.querySelector<HTMLInputElement>("#svm-save-username")?.value.trim() || data.username;
    const finalPassword =
      formRows.querySelector<HTMLInputElement>("#svm-save-password")?.value || data.password;
    const chosenVaultId =
      formRows.querySelector<HTMLSelectElement>("#svm-save-vault")?.value || data.vaultId;

    await sendExtensionBridgeMessage("save_credential", {
      ...data,
      username: finalUsername,
      password: finalPassword,
      vaultId: chosenVaultId,
    });
    host.remove();
  });

  // Toggle xem mật khẩu
  pwdRow.querySelector("#svm-toggle-eye")?.addEventListener("click", () => {
    const pwdInput = formRows.querySelector<HTMLInputElement>("#svm-save-password");
    if (pwdInput) {
      pwdInput.type = pwdInput.type === "password" ? "text" : "password";
    }
  });

  actions.appendChild(dismissBtn);
  actions.appendChild(saveBtn);

  container.appendChild(header);
  container.appendChild(formRows);
  container.appendChild(actions);
  shadow.appendChild(container);
  document.documentElement.appendChild(host);
}

function escapeHtml(raw: string): string {
  return raw
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}
