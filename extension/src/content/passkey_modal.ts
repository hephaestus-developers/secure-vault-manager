/**
 * @file extension/src/content/passkey_modal.ts
 * @brief Hộp thoại xác nhận tương tác Passkey (Tạo / Xác thực / Mở khóa tại chỗ) trong Shadow DOM cô lập.
 */

import { sendExtensionBridgeMessage } from "../services/protocol";

export interface MatchedPasskey {
  id: string;
  title: string;
  username: string;
  credentialId: string;
}

export interface PasskeyModalParams {
  action: "passkey_create" | "passkey_get";
  rpId: string;
  rpName?: string;
  userName?: string;
  matchedPasskeys?: MatchedPasskey[];
}

export interface PasskeyConsentResult {
  consented: boolean;
  selectedCredentialId?: string;
}

export function dismissPasskeyConsent(): void {
  const existing = document.getElementById("svm-passkey-modal-host");
  if (existing) {
    existing.remove();
  }
}

export function promptPasskeyConsent(params: PasskeyModalParams): Promise<PasskeyConsentResult> {
  return new Promise((resolve) => {
    const existing = document.getElementById("svm-passkey-modal-host");
    if (existing) {
      existing.remove();
    }

    const host = document.createElement("div");
    host.id = "svm-passkey-modal-host";
    const shadow = host.attachShadow({ mode: "closed" });

    const style = document.createElement("style");
    style.textContent = `
      :host {
        all: initial;
      }
      .svm-overlay {
        position: fixed;
        top: 20px;
        right: 20px;
        z-index: 2147483647;
        width: 380px;
        background: #181825;
        color: #cdd6f4;
        border: 1px solid #313244;
        border-radius: 14px;
        box-shadow: 0 16px 40px rgba(0, 0, 0, 0.55), 0 0 0 1px rgba(255, 255, 255, 0.05);
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
        font-size: 13px;
        line-height: 1.5;
        overflow: hidden;
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
        padding: 12px 16px;
        border-bottom: 1px solid #232433;
        background: rgba(24, 24, 37, 0.6);
      }
      .svm-brand {
        display: flex;
        align-items: center;
        gap: 8px;
        font-weight: 700;
        font-size: 13px;
        color: #89b4fa;
      }
      .svm-close-btn {
        background: transparent;
        border: none;
        color: #6c7086;
        cursor: pointer;
        font-size: 18px;
        line-height: 1;
        padding: 4px;
        border-radius: 4px;
        display: flex;
        align-items: center;
        justify-content: center;
      }
      .svm-close-btn:hover {
        color: #cdd6f4;
        background: #313244;
      }
      .svm-body {
        padding: 16px;
      }
      .svm-title-row {
        display: flex;
        align-items: center;
        gap: 10px;
        margin-bottom: 10px;
      }
      .svm-icon-badge {
        width: 36px;
        height: 36px;
        border-radius: 8px;
        background: #313244;
        display: flex;
        align-items: center;
        justify-content: center;
        color: #89b4fa;
        flex-shrink: 0;
      }
      .svm-title {
        font-weight: 700;
        font-size: 14px;
        color: #f5e0dc;
      }
      .svm-rp-text {
        color: #a6adc8;
        font-size: 12px;
      }
      .svm-info-box {
        background: #11111b;
        border: 1px solid #313244;
        border-radius: 8px;
        padding: 10px 12px;
        margin: 12px 0 16px 0;
      }
      .svm-info-row {
        display: flex;
        justify-content: space-between;
        margin-bottom: 4px;
      }
      .svm-info-row:last-child {
        margin-bottom: 0;
      }
      .svm-info-label {
        color: #6c7086;
        font-size: 12px;
      }
      .svm-info-val {
        color: #cdd6f4;
        font-weight: 600;
        font-size: 12px;
        max-width: 200px;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
      }
      .svm-unlock-pane {
        display: flex;
        flex-direction: column;
        gap: 10px;
        margin-top: 10px;
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
        padding: 9px 36px 9px 12px;
        color: #cdd6f4;
        font-size: 13px;
        outline: none;
      }
      .svm-input:focus {
        border-color: #89b4fa;
        box-shadow: 0 0 0 2px rgba(137, 180, 250, 0.2);
      }
      .svm-toggle-eye {
        position: absolute;
        right: 10px;
        background: transparent;
        border: none;
        color: #6c7086;
        cursor: pointer;
        padding: 2px;
      }
      .svm-error-text {
        color: #f38ba8;
        font-size: 12px;
        min-height: 16px;
      }
      .svm-actions {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 8px;
        margin-top: 8px;
      }
      .svm-btn {
        flex: 1;
        height: 36px;
        padding: 0 14px;
        border-radius: 8px;
        font-size: 12.5px;
        font-weight: 600;
        cursor: pointer;
        border: none;
        white-space: nowrap;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        gap: 6px;
        transition: background 0.15s ease;
      }
      .svm-offline-desc {
        font-size: 12px;
        color: #a6adc8;
        line-height: 1.5;
      }
      .svm-accounts-list {
        display: flex;
        flex-direction: column;
        gap: 6px;
        margin: 10px 0 14px 0;
      }
      .svm-accounts-heading {
        font-size: 11.5px;
        color: #a6adc8;
        font-weight: 600;
      }
      .svm-account-option {
        display: flex;
        align-items: center;
        gap: 8px;
        padding: 8px 10px;
        background: #11111b;
        border: 1px solid #313244;
        border-radius: 8px;
        cursor: pointer;
      }
      .svm-account-option:hover {
        border-color: #45475a;
      }
      .svm-account-user {
        font-weight: 600;
        color: #cdd6f4;
      }
      .svm-account-title {
        font-size: 11px;
        color: #6c7086;
      }
      .svm-btn-primary {
        background: #89b4fa;
        color: #11111b;
      }
      .svm-btn-primary:hover {
        background: #b4befe;
      }
      .svm-btn-secondary {
        background: #313244;
        color: #cdd6f4;
        border: 1px solid #45475a;
      }
      .svm-btn-secondary:hover {
        background: #45475a;
      }
      .svm-btn:disabled {
        opacity: 0.5;
        cursor: not-allowed;
      }
      @media (prefers-color-scheme: light) {
        .svm-overlay {
          background: #ffffff;
          color: #1e1e2e;
          border-color: #ccd0da;
          box-shadow: 0 16px 40px rgba(30, 30, 46, 0.18), 0 0 0 1px rgba(0, 0, 0, 0.05);
        }
        .svm-header {
          border-bottom-color: #e6e9ef;
          background: #f4f5f8;
        }
        .svm-brand {
          color: #1e66f5;
        }
        .svm-close-btn {
          color: #8c8fa1;
        }
        .svm-close-btn:hover {
          color: #4c4f69;
          background: #e6e9ef;
        }
        .svm-icon-badge {
          background: #eff1f5;
          color: #1e66f5;
        }
        .svm-title {
          color: #1e1e2e;
        }
        .svm-rp-text {
          color: #6c6f85;
        }
        .svm-info-box {
          background: #f4f5f8;
          border-color: #ccd0da;
        }
        .svm-info-label {
          color: #8c8fa1;
        }
        .svm-info-val {
          color: #1e1e2e;
        }
        .svm-input {
          background: #ffffff;
          border-color: #ccd0da;
          color: #1e1e2e;
        }
        .svm-btn-primary {
          background: #1e66f5;
          color: #ffffff;
        }
        .svm-btn-primary:hover {
          background: #04a5e5;
        }
        .svm-btn-secondary {
          background: #e6e9ef;
          color: #1e1e2e;
          border-color: #ccd0da;
        }
        .svm-btn-secondary:hover {
          background: #dce0e8;
        }
        .svm-offline-desc,
        .svm-accounts-heading {
          color: #5c5f77;
        }
        .svm-account-option {
          background: #f4f5f8;
          border-color: #ccd0da;
        }
        .svm-account-user {
          color: #1e1e2e;
        }
        .svm-account-title {
          color: #7c7f93;
        }
      }
    `;

    const container = document.createElement("div");
    container.className = "svm-overlay";

    let isUnlocked = false;

    function cleanup(consented: boolean, selectedCredentialId?: string) {
      host.remove();
      resolve({ consented, selectedCredentialId });
    }

    async function checkStatusAndRender() {
      container.innerHTML = "";

      // Header
      const header = document.createElement("div");
      header.className = "svm-header";
      header.innerHTML = `
        <div class="svm-brand">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
            <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>
          </svg>
          Secure Vault
        </div>
        <button class="svm-close-btn" title="Đóng">✕</button>
      `;
      header.querySelector(".svm-close-btn")?.addEventListener("click", () => cleanup(false));
      container.appendChild(header);

      const body = document.createElement("div");
      body.className = "svm-body";

      // Kiểm tra trạng thái mở khóa từ background
      const statusResp = await sendExtensionBridgeMessage<any>("get_vault_status");
      const isDesktopRunning = Boolean(statusResp.ok && statusResp.payload?.desktopRunning);
      isUnlocked = Boolean(statusResp.ok && statusResp.payload?.unlocked);

      // TRƯỜNG HỢP 1: Desktop App chưa chạy (Offline)
      if (!isDesktopRunning) {
        body.innerHTML = `
          <div class="svm-title-row">
            <div class="svm-icon-badge">
              <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <rect x="2" y="3" width="20" height="14" rx="2" ry="2"/>
                <line x1="8" y1="21" x2="16" y2="21"/>
                <line x1="12" y1="17" x2="12" y2="21"/>
              </svg>
            </div>
            <div>
              <div class="svm-title">Ứng dụng Desktop chưa khởi chạy</div>
              <div class="svm-rp-text">Mở ứng dụng trên máy tính để sử dụng Passkey</div>
            </div>
          </div>
          <div class="svm-info-box" style="margin-bottom: 14px;">
            <div class="svm-offline-desc">
              Secure Vault Manager cần ứng dụng Desktop đang chạy để bảo vệ và ký xác thực Passkey an toàn.
            </div>
          </div>
          <div class="svm-actions">
            <button class="svm-btn svm-btn-secondary" id="svm-btn-cancel">Phương thức khác</button>
            <button class="svm-btn svm-btn-primary" id="svm-btn-retry">
              Kiểm tra lại
            </button>
          </div>
        `;

        container.appendChild(body);

        body.querySelector("#svm-btn-cancel")?.addEventListener("click", () => cleanup(false));
        const retryBtn = body.querySelector<HTMLButtonElement>("#svm-btn-retry");
        retryBtn?.addEventListener("click", async () => {
          if (retryBtn) {
            retryBtn.disabled = true;
            retryBtn.textContent = "Đang kiểm tra...";
          }
          await checkStatusAndRender();
        });
        return;
      }

      // TRƯỜNG HỢP 2: Desktop đang chạy nhưng Kho bị khóa
      if (!isUnlocked) {
        // Giao diện Yêu cầu Mở khóa kho
        body.innerHTML = `
          <div class="svm-title-row">
            <div class="svm-icon-badge">
              <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <rect x="3" y="11" width="18" height="11" rx="2" ry="2"/>
                <path d="M7 11V7a5 5 0 0 1 10 0v4"/>
              </svg>
            </div>
            <div>
              <div class="svm-title">Kho mật khẩu đang khóa</div>
              <div class="svm-rp-text">Mở khóa để tiếp tục với Passkey</div>
            </div>
          </div>
          <div class="svm-unlock-pane">
            <div class="svm-input-wrapper">
              <input type="password" class="svm-input" id="svm-pwd-input" placeholder="Nhập Mật khẩu chính..." autofocus />
              <button class="svm-toggle-eye" id="svm-toggle-eye" type="button" title="Hiện mật khẩu">👁</button>
            </div>
            <div class="svm-error-text" id="svm-error-msg"></div>
            <div class="svm-actions">
              <button class="svm-btn svm-btn-secondary" id="svm-btn-cancel">Hủy</button>
              <button class="svm-btn svm-btn-primary" id="svm-btn-unlock">Mở khóa</button>
            </div>
          </div>
        `;

        container.appendChild(body);

        const pwdInput = body.querySelector<HTMLInputElement>("#svm-pwd-input");
        const toggleEye = body.querySelector<HTMLButtonElement>("#svm-toggle-eye");
        const errorMsg = body.querySelector<HTMLDivElement>("#svm-error-msg");
        const btnCancel = body.querySelector<HTMLButtonElement>("#svm-btn-cancel");
        const btnUnlock = body.querySelector<HTMLButtonElement>("#svm-btn-unlock");

        toggleEye?.addEventListener("click", () => {
          if (pwdInput) {
            pwdInput.type = pwdInput.type === "password" ? "text" : "password";
          }
        });

        btnCancel?.addEventListener("click", () => cleanup(false));

        async function doUnlock() {
          const pwd = pwdInput?.value || "";
          if (!pwd) {
            if (errorMsg) errorMsg.textContent = "Vui lòng nhập Mật khẩu chính";
            return;
          }
          if (btnUnlock) {
            btnUnlock.disabled = true;
            btnUnlock.textContent = "Đang mở...";
          }
          if (errorMsg) errorMsg.textContent = "";

          const unlockResp = await sendExtensionBridgeMessage<any>("unlock_vault", {
            password: pwd,
          });
          if (unlockResp.ok && unlockResp.payload?.unlocked) {
            isUnlocked = true;
            checkStatusAndRender();
          } else {
            if (btnUnlock) {
              btnUnlock.disabled = false;
              btnUnlock.textContent = "Mở khóa";
            }
            if (errorMsg) {
              if (unlockResp.error?.code === "DESKTOP_NOT_RUNNING") {
                errorMsg.textContent = "Ứng dụng Desktop chưa khởi động.";
              } else {
                errorMsg.textContent = unlockResp.error?.message || "Mật khẩu không đúng";
              }
            }
          }
        }

        btnUnlock?.addEventListener("click", () => void doUnlock());
        pwdInput?.addEventListener("keydown", (e) => {
          if (e.key === "Enter") {
            void doUnlock();
          }
        });
        pwdInput?.focus();
        return;
      }

      // Khi kho đã mở: Hiển thị xác nhận Passkey Tạo / Đăng nhập
      const escapeHtml = (raw: string): string =>
        raw
          .replace(/&/g, "&amp;")
          .replace(/</g, "&lt;")
          .replace(/>/g, "&gt;")
          .replace(/"/g, "&quot;")
          .replace(/'/g, "&#39;");

      const isCreate = params.action === "passkey_create";
      const title = isCreate ? "Lưu Passkey mới" : "Đăng nhập bằng Passkey";
      const safeRpId = escapeHtml(params.rpId || "");
      const matched = params.matchedPasskeys || [];
      const primaryPasskey = matched[0];
      const safeUser = escapeHtml(
        params.userName || primaryPasskey?.username || primaryPasskey?.title || "",
      );
      const subtitle = isCreate
        ? "Lưu khóa Passkey an toàn vào kho"
        : "Xác thực nhanh bằng khóa lưu trong Secure Vault";

      // Danh sách tài khoản nếu có nhiều passkey
      let accountsHtml = "";
      if (!isCreate && matched.length > 1) {
        accountsHtml = `
          <div class="svm-accounts-list">
            <div class="svm-accounts-heading">Chọn tài khoản để đăng nhập:</div>
            ${matched
              .map(
                (pk, idx) => `
                <label class="svm-account-option">
                  <input type="radio" name="svm-selected-pk" value="${escapeHtml(pk.credentialId)}" ${idx === 0 ? "checked" : ""} />
                  <div>
                    <div class="svm-account-user">${escapeHtml(pk.username || pk.title || "Tài khoản Passkey")}</div>
                    <div class="svm-account-title">${escapeHtml(pk.title || safeRpId)}</div>
                  </div>
                </label>
              `,
              )
              .join("")}
          </div>
        `;
      }

      body.innerHTML = `
        <div class="svm-title-row">
          <div class="svm-icon-badge">
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <circle cx="8.5" cy="8.5" r="4.5"/>
              <line x1="12" y1="12" x2="21" y2="21"/>
              <line x1="16" y1="16" x2="16" y2="18"/>
              <line x1="19" y1="19" x2="19" y2="21"/>
            </svg>
          </div>
          <div>
            <div class="svm-title">${title}</div>
            <div class="svm-rp-text">${subtitle}</div>
          </div>
        </div>
        <div class="svm-info-box">
          <div class="svm-info-row">
            <span class="svm-info-label">Trang web:</span>
            <span class="svm-info-val" title="${safeRpId}">${safeRpId}</span>
          </div>
          ${
            safeUser && matched.length <= 1
              ? `<div class="svm-info-row">
                  <span class="svm-info-label">Tài khoản:</span>
                  <span class="svm-info-val" title="${safeUser}">${safeUser}</span>
                </div>`
              : ""
          }
        </div>
        ${accountsHtml}
        <div class="svm-actions">
          <button class="svm-btn svm-btn-secondary" id="svm-btn-cancel">Phương thức khác</button>
          <button class="svm-btn svm-btn-primary" id="svm-btn-confirm">
            ${isCreate ? "Lưu Passkey" : "Đăng nhập"}
          </button>
        </div>
      `;

      container.appendChild(body);

      body.querySelector("#svm-btn-cancel")?.addEventListener("click", () => cleanup(false));
      body.querySelector("#svm-btn-confirm")?.addEventListener("click", () => {
        let selectedCredId = primaryPasskey?.credentialId;
        if (!isCreate && matched.length > 1) {
          const checked = body.querySelector<HTMLInputElement>(
            "input[name='svm-selected-pk']:checked",
          );
          if (checked?.value) {
            selectedCredId = checked.value;
          }
        }
        cleanup(true, selectedCredId);
      });
    }

    shadow.appendChild(style);
    shadow.appendChild(container);
    document.documentElement.appendChild(host);

    void checkStatusAndRender();
  });
}
