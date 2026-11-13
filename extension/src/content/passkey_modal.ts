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
  selectedItemId?: string;
  useFallbackNative?: boolean;
  selectedVaultId?: string;
}

let activeModalCleanup: (() => void) | null = null;

export function dismissPasskeyConsent(): void {
  if (activeModalCleanup) {
    activeModalCleanup();
    activeModalCleanup = null;
  }
  const existing = document.getElementById("svm-passkey-modal-host");
  if (existing) {
    existing.remove();
  }
}

function createSvg(
  viewBox: string,
  width: number,
  height: number,
  strokeWidth: number,
  children: SVGElement[],
): SVGSVGElement {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", viewBox);
  svg.setAttribute("width", String(width));
  svg.setAttribute("height", String(height));
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", String(strokeWidth));
  for (const child of children) {
    svg.appendChild(child);
  }
  return svg;
}

function createSvgPath(d: string): SVGPathElement {
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  path.setAttribute("d", d);
  return path;
}

function createSvgRect(
  x: number,
  y: number,
  w: number,
  h: number,
  rx: number,
  ry: number,
): SVGRectElement {
  const rect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
  rect.setAttribute("x", String(x));
  rect.setAttribute("y", String(y));
  rect.setAttribute("width", String(w));
  rect.setAttribute("height", String(h));
  rect.setAttribute("rx", String(rx));
  rect.setAttribute("ry", String(ry));
  return rect;
}

function createSvgLine(x1: number, y1: number, x2: number, y2: number): SVGLineElement {
  const line = document.createElementNS("http://www.w3.org/2000/svg", "line");
  line.setAttribute("x1", String(x1));
  line.setAttribute("y1", String(y1));
  line.setAttribute("x2", String(x2));
  line.setAttribute("y2", String(y2));
  return line;
}

function createSvgCircle(cx: number, cy: number, r: number): SVGCircleElement {
  const circle = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  circle.setAttribute("cx", String(cx));
  circle.setAttribute("cy", String(cy));
  circle.setAttribute("r", String(r));
  return circle;
}

export function promptPasskeyConsent(params: PasskeyModalParams): Promise<PasskeyConsentResult> {
  return new Promise((resolve) => {
    dismissPasskeyConsent();

    const host = document.createElement("div");
    host.id = "svm-passkey-modal-host";
    const shadow = host.attachShadow({ mode: "closed" });

    const style = document.createElement("style");
    style.textContent = `
      :host {
        all: initial;
      }
      .svm-backdrop {
        position: fixed;
        top: 0;
        left: 0;
        right: 0;
        bottom: 0;
        background: rgba(0, 0, 0, 0.5);
        backdrop-filter: blur(2px);
        z-index: 2147483646;
        display: flex;
        align-items: center;
        justify-content: center;
      }
      .svm-overlay {
        position: relative;
        z-index: 2147483647;
        width: 400px;
        max-width: 92vw;
        background: #181825;
        color: #cdd6f4;
        border: 1px solid #313244;
        border-radius: 16px;
        box-shadow: 0 20px 50px rgba(0, 0, 0, 0.65), 0 0 0 1px rgba(255, 255, 255, 0.08);
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
        .svm-vault-select {
          background: #ffffff;
          border-color: #ccd0da;
          color: #1e1e2e;
        }
      }
      .svm-vault-select {
        background: #11111b;
        color: #cdd6f4;
        border: 1px solid #45475a;
        border-radius: 6px;
        padding: 4px 8px;
        font-size: 12px;
        font-family: inherit;
        outline: none;
        cursor: pointer;
        max-width: 180px;
      }
      .svm-vault-select:focus {
        border-color: #89b4fa;
      }
    `;

    const backdrop = document.createElement("div");
    backdrop.className = "svm-backdrop";

    const container = document.createElement("div");
    container.className = "svm-overlay";
    backdrop.appendChild(container);

    let isUnlocked = false;
    let lockPollTimer: ReturnType<typeof setInterval> | null = null;

    function stopLockPolling() {
      if (lockPollTimer) {
        clearInterval(lockPollTimer);
        lockPollTimer = null;
      }
    }

    function onModalKeyDown(e: KeyboardEvent) {
      if (e.key === "Escape") {
        cleanup(false, undefined, false);
      }
    }

    function cleanup(
      consented: boolean,
      selectedCredentialId?: string,
      useFallbackNative = false,
      selectedVaultId?: string,
      selectedItemId?: string,
    ) {
      stopLockPolling();
      window.removeEventListener("keydown", onModalKeyDown, true);
      activeModalCleanup = null;
      host.remove();
      resolve({
        consented,
        selectedCredentialId,
        selectedItemId,
        useFallbackNative,
        selectedVaultId,
      });
    }

    activeModalCleanup = () => cleanup(false, undefined, false);
    window.addEventListener("keydown", onModalKeyDown, true);

    backdrop.addEventListener("click", (e) => {
      if (e.target === backdrop) {
        cleanup(false, undefined, false);
      }
    });

    async function checkStatusAndRender() {
      container.replaceChildren();

      // Header
      const header = document.createElement("div");
      header.className = "svm-header";

      const brand = document.createElement("div");
      brand.className = "svm-brand";
      brand.appendChild(
        createSvg("0 0 24 24", 16, 16, 2.5, [
          createSvgPath("M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"),
        ]),
      );
      const brandText = document.createTextNode(" Secure Vault");
      brand.appendChild(brandText);

      const closeBtn = document.createElement("button");
      closeBtn.type = "button";
      closeBtn.className = "svm-close-btn";
      closeBtn.title = "Đóng";
      closeBtn.textContent = "✕";
      closeBtn.addEventListener("click", () => cleanup(false, undefined, false));

      header.appendChild(brand);
      header.appendChild(closeBtn);
      container.appendChild(header);

      const body = document.createElement("div");
      body.className = "svm-body";

      // Kiểm tra trạng thái mở khóa từ background
      const statusResp = await sendExtensionBridgeMessage<any>("get_vault_status");
      const isDesktopRunning = Boolean(statusResp.ok && statusResp.payload?.desktopRunning);
      isUnlocked = Boolean(statusResp.ok && statusResp.payload?.unlocked);

      // TRƯỜNG HỢP 1: Desktop App chưa chạy (Offline)
      if (!isDesktopRunning) {
        const titleRow = document.createElement("div");
        titleRow.className = "svm-title-row";
        const iconBadge = document.createElement("div");
        iconBadge.className = "svm-icon-badge";
        iconBadge.appendChild(
          createSvg("0 0 24 24", 20, 20, 2, [
            createSvgRect(2, 3, 20, 14, 2, 2),
            createSvgLine(8, 21, 16, 21),
            createSvgLine(12, 17, 12, 21),
          ]),
        );
        const titleTextWrap = document.createElement("div");
        const titleEl = document.createElement("div");
        titleEl.className = "svm-title";
        titleEl.textContent = "Ứng dụng Desktop chưa khởi chạy";
        const rpEl = document.createElement("div");
        rpEl.className = "svm-rp-text";
        rpEl.textContent = "Mở ứng dụng trên máy tính để sử dụng Passkey";
        titleTextWrap.appendChild(titleEl);
        titleTextWrap.appendChild(rpEl);
        titleRow.appendChild(iconBadge);
        titleRow.appendChild(titleTextWrap);

        const infoBox = document.createElement("div");
        infoBox.className = "svm-info-box";
        infoBox.style.marginBottom = "14px";
        const offlineDesc = document.createElement("div");
        offlineDesc.className = "svm-offline-desc";
        offlineDesc.textContent =
          "Secure Vault Manager cần ứng dụng Desktop đang chạy để bảo vệ và ký xác thực Passkey an toàn.";
        infoBox.appendChild(offlineDesc);

        const actions = document.createElement("div");
        actions.className = "svm-actions";
        const cancelBtn = document.createElement("button");
        cancelBtn.type = "button";
        cancelBtn.className = "svm-btn svm-btn-secondary";
        cancelBtn.id = "svm-btn-cancel";
        cancelBtn.textContent = "Phương thức khác";
        cancelBtn.addEventListener("click", () => cleanup(false, undefined, true));

        const retryBtn = document.createElement("button");
        retryBtn.type = "button";
        retryBtn.className = "svm-btn svm-btn-primary";
        retryBtn.id = "svm-btn-retry";
        retryBtn.textContent = "Kiểm tra lại";
        retryBtn.addEventListener("click", async () => {
          retryBtn.disabled = true;
          retryBtn.textContent = "Đang kiểm tra...";
          await checkStatusAndRender();
        });

        actions.appendChild(cancelBtn);
        actions.appendChild(retryBtn);

        body.appendChild(titleRow);
        body.appendChild(infoBox);
        body.appendChild(actions);
        container.appendChild(body);
        return;
      }

      // TRƯỜNG HỢP 2: Desktop đang chạy nhưng Kho bị khóa
      if (!isUnlocked) {
        const bioAvailable = Boolean(statusResp.ok && statusResp.payload?.biometricAvailable);

        const titleRow = document.createElement("div");
        titleRow.className = "svm-title-row";
        const iconBadge = document.createElement("div");
        iconBadge.className = "svm-icon-badge";
        iconBadge.appendChild(
          createSvg("0 0 24 24", 20, 20, 2, [
            createSvgRect(3, 11, 18, 11, 2, 2),
            createSvgPath("M7 11V7a5 5 0 0 1 10 0v4"),
          ]),
        );
        const titleTextWrap = document.createElement("div");
        const titleEl = document.createElement("div");
        titleEl.className = "svm-title";
        titleEl.textContent = "Kho mật khẩu đang khóa";
        const rpEl = document.createElement("div");
        rpEl.className = "svm-rp-text";
        rpEl.textContent = "Mở khóa an toàn để tiếp tục với Passkey";
        titleTextWrap.appendChild(titleEl);
        titleTextWrap.appendChild(rpEl);
        titleRow.appendChild(iconBadge);
        titleRow.appendChild(titleTextWrap);

        const infoBox = document.createElement("div");
        infoBox.className = "svm-info-box";
        infoBox.style.marginBottom = "10px";
        const offlineDesc = document.createElement("div");
        offlineDesc.className = "svm-offline-desc";
        offlineDesc.textContent =
          "Vì lý do bảo mật, không nhập Mật khẩu chính trực tiếp trên trang web. Vui lòng mở khóa bằng Sinh trắc học hoặc qua biểu tượng Secure Vault trên thanh công cụ trình duyệt / ứng dụng Desktop.";
        infoBox.appendChild(offlineDesc);

        const errorMsg = document.createElement("div");
        errorMsg.className = "svm-error-text";
        errorMsg.id = "svm-error-msg";

        const actions = document.createElement("div");
        actions.className = "svm-actions";
        const btnCancel = document.createElement("button");
        btnCancel.type = "button";
        btnCancel.className = "svm-btn svm-btn-secondary";
        btnCancel.id = "svm-btn-cancel";
        btnCancel.textContent = "Phương thức khác";
        btnCancel.addEventListener("click", () => cleanup(false, undefined, true));

        actions.appendChild(btnCancel);

        if (bioAvailable) {
          const btnBio = document.createElement("button");
          btnBio.type = "button";
          btnBio.className = "svm-btn svm-btn-primary";
          btnBio.id = "svm-btn-bio";
          btnBio.textContent = "Mở khóa Sinh trắc học";
          btnBio.addEventListener("click", async () => {
            btnBio.disabled = true;
            btnBio.textContent = "Đang xác thực...";
            errorMsg.textContent = "";
            const bioResp = await sendExtensionBridgeMessage<any>("unlock_vault_biometric");
            if (bioResp.ok && bioResp.payload?.unlocked) {
              stopLockPolling();
              isUnlocked = true;
              void checkStatusAndRender();
            } else {
              btnBio.disabled = false;
              btnBio.textContent = "Mở khóa Sinh trắc học";
              errorMsg.textContent =
                bioResp.error?.message || "Xác thực sinh trắc học không thành công.";
            }
          });
          actions.appendChild(btnBio);
        } else {
          const btnRetry = document.createElement("button");
          btnRetry.type = "button";
          btnRetry.className = "svm-btn svm-btn-primary";
          btnRetry.id = "svm-btn-retry";
          btnRetry.textContent = "Kiểm tra lại";
          btnRetry.addEventListener("click", () => {
            void checkStatusAndRender();
          });
          actions.appendChild(btnRetry);
        }

        body.appendChild(titleRow);
        body.appendChild(infoBox);
        body.appendChild(errorMsg);
        body.appendChild(actions);
        container.appendChild(body);

        if (!lockPollTimer) {
          lockPollTimer = setInterval(() => {
            void (async () => {
              const pollResp = await sendExtensionBridgeMessage<any>("get_vault_status");
              if (pollResp.ok && pollResp.payload?.unlocked) {
                stopLockPolling();
                void checkStatusAndRender();
              }
            })();
          }, 1500);
        }
        return;
      }

      stopLockPolling();

      // Khi kho đã mở: Hiển thị xác nhận Passkey Tạo / Đăng nhập
      const isCreate = params.action === "passkey_create";
      const title = isCreate ? "Lưu Passkey mới" : "Đăng nhập bằng Passkey";
      const rpId = params.rpId || "";
      const matched = params.matchedPasskeys || [];
      const primaryPasskey = matched[0];
      const userName = params.userName || primaryPasskey?.username || primaryPasskey?.title || "";
      const subtitle = isCreate
        ? "Lưu khóa Passkey an toàn vào kho"
        : "Xác thực nhanh bằng khóa lưu trong Secure Vault";

      // Tiêu đề & Icon Key
      const titleRow = document.createElement("div");
      titleRow.className = "svm-title-row";
      const iconBadge = document.createElement("div");
      iconBadge.className = "svm-icon-badge";
      iconBadge.appendChild(
        createSvg("0 0 24 24", 20, 20, 2, [
          createSvgCircle(8.5, 8.5, 4.5),
          createSvgLine(12, 12, 21, 21),
          createSvgLine(16, 16, 16, 18),
          createSvgLine(19, 19, 19, 21),
        ]),
      );
      const titleTextWrap = document.createElement("div");
      const titleEl = document.createElement("div");
      titleEl.className = "svm-title";
      titleEl.textContent = title;
      const subtitleEl = document.createElement("div");
      subtitleEl.className = "svm-rp-text";
      subtitleEl.textContent = subtitle;
      titleTextWrap.appendChild(titleEl);
      titleTextWrap.appendChild(subtitleEl);
      titleRow.appendChild(iconBadge);
      titleRow.appendChild(titleTextWrap);
      body.appendChild(titleRow);

      // Info box
      const infoBox = document.createElement("div");
      infoBox.className = "svm-info-box";

      const rpRow = document.createElement("div");
      rpRow.className = "svm-info-row";
      const rpLabel = document.createElement("span");
      rpLabel.className = "svm-info-label";
      rpLabel.textContent = "Trang web:";
      const rpVal = document.createElement("span");
      rpVal.className = "svm-info-val";
      rpVal.title = rpId;
      rpVal.textContent = rpId;
      rpRow.appendChild(rpLabel);
      rpRow.appendChild(rpVal);
      infoBox.appendChild(rpRow);

      if (userName && matched.length <= 1) {
        const userRow = document.createElement("div");
        userRow.className = "svm-info-row";
        const userLabel = document.createElement("span");
        userLabel.className = "svm-info-label";
        userLabel.textContent = "Tài khoản:";
        const userVal = document.createElement("span");
        userVal.className = "svm-info-val";
        userVal.title = userName;
        userVal.textContent = userName;
        userRow.appendChild(userLabel);
        userRow.appendChild(userVal);
        infoBox.appendChild(userRow);
      }

      // Lấy danh sách Vaults khả dụng
      let availableVaults: Array<{ id: string; name: string; isDefault?: boolean }> = [];
      if (statusResp.ok && Array.isArray(statusResp.payload?.vaults)) {
        availableVaults = statusResp.payload.vaults;
      } else {
        const vaultsResp = await sendExtensionBridgeMessage<any>("get_vaults");
        if (vaultsResp.ok && Array.isArray(vaultsResp.payload?.vaults)) {
          availableVaults = vaultsResp.payload.vaults;
        }
      }

      let vaultSelectEl: HTMLSelectElement | null = null;
      if (isCreate && availableVaults.length >= 2) {
        const vRow = document.createElement("div");
        vRow.className = "svm-info-row";
        vRow.style.alignItems = "center";
        const vLabel = document.createElement("span");
        vLabel.className = "svm-info-label";
        vLabel.textContent = "Lưu vào kho:";
        vaultSelectEl = document.createElement("select");
        vaultSelectEl.className = "svm-vault-select";
        vaultSelectEl.id = "svm-vault-select";
        for (const v of availableVaults) {
          const opt = document.createElement("option");
          opt.value = v.id;
          opt.textContent = v.name;
          if (v.isDefault) opt.selected = true;
          vaultSelectEl.appendChild(opt);
        }
        vRow.appendChild(vLabel);
        vRow.appendChild(vaultSelectEl);
        infoBox.appendChild(vRow);
      } else if (isCreate && availableVaults.length === 1) {
        const vRow = document.createElement("div");
        vRow.className = "svm-info-row";
        const vLabel = document.createElement("span");
        vLabel.className = "svm-info-label";
        vLabel.textContent = "Kho lưu trữ:";
        const vVal = document.createElement("span");
        vVal.className = "svm-info-val";
        vVal.textContent = availableVaults[0].name;
        vRow.appendChild(vLabel);
        vRow.appendChild(vVal);
        infoBox.appendChild(vRow);
      }

      body.appendChild(infoBox);

      // Danh sách tài khoản nếu có nhiều passkey
      if (!isCreate && matched.length > 1) {
        const accList = document.createElement("div");
        accList.className = "svm-accounts-list";
        const accHeading = document.createElement("div");
        accHeading.className = "svm-accounts-heading";
        accHeading.textContent = "Chọn tài khoản để đăng nhập:";
        accList.appendChild(accHeading);

        matched.forEach((pk, idx) => {
          const label = document.createElement("label");
          label.className = "svm-account-option";

          const radio = document.createElement("input");
          radio.type = "radio";
          radio.name = "svm-selected-pk";
          radio.value = pk.credentialId;
          if (idx === 0) radio.checked = true;

          const textWrap = document.createElement("div");
          const userDiv = document.createElement("div");
          userDiv.className = "svm-account-user";
          userDiv.textContent = pk.username || pk.title || "Tài khoản Passkey";
          const titleDiv = document.createElement("div");
          titleDiv.className = "svm-account-title";
          titleDiv.textContent = pk.title || rpId;

          textWrap.appendChild(userDiv);
          textWrap.appendChild(titleDiv);
          label.appendChild(radio);
          label.appendChild(textWrap);
          accList.appendChild(label);
        });

        body.appendChild(accList);
      }

      // Actions
      const actions = document.createElement("div");
      actions.className = "svm-actions";
      const btnCancel = document.createElement("button");
      btnCancel.type = "button";
      btnCancel.className = "svm-btn svm-btn-secondary";
      btnCancel.id = "svm-btn-cancel";
      btnCancel.textContent = "Phương thức khác";
      btnCancel.addEventListener("click", () => cleanup(false, undefined, true));

      const btnConfirm = document.createElement("button");
      btnConfirm.type = "button";
      btnConfirm.className = "svm-btn svm-btn-primary";
      btnConfirm.id = "svm-btn-confirm";
      btnConfirm.textContent = isCreate ? "Lưu Passkey" : "Đăng nhập";

      btnConfirm.addEventListener("click", () => {
        let selectedCredId = primaryPasskey?.credentialId;
        let selectedItemId = primaryPasskey?.id;
        if (!isCreate && matched.length > 1) {
          const checked = body.querySelector<HTMLInputElement>(
            "input[name='svm-selected-pk']:checked",
          );
          if (checked?.value) {
            selectedCredId = checked.value;
            const found = matched.find((pk) => pk.credentialId === selectedCredId);
            if (found) {
              selectedItemId = found.id;
            }
          }
        }
        let selectedVaultId: string | undefined;
        if (isCreate && vaultSelectEl) {
          selectedVaultId = vaultSelectEl.value;
        }
        cleanup(true, selectedCredId, false, selectedVaultId, selectedItemId);
      });

      actions.appendChild(btnCancel);
      actions.appendChild(btnConfirm);
      body.appendChild(actions);
      container.appendChild(body);
    }

    shadow.appendChild(style);
    shadow.appendChild(backdrop);
    document.documentElement.appendChild(host);

    void checkStatusAndRender();
  });
}
