/**
 * @file extension/src/content/save-prompt.ts
 * @brief Thanh thông báo gợi ý lưu thông tin đăng nhập với luồng mở khóa tại chỗ (Inline Unlock-to-Save).
 */

import { sendExtensionBridgeMessage, VaultStatusPayload } from "../services/protocol";

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
  // Chỉ hiển thị Save Prompt ở cửa sổ cấp cao nhất (window.top) để tránh vỡ giao diện trong iFrame nhỏ/quảng cáo
  if (typeof window !== "undefined" && window.self !== window.top) {
    return;
  }

  const existing = document.getElementById("svm-save-prompt-host");
  if (existing) {
    // Đã hiển thị popup trên màn hình, không gỡ bỏ để tránh nhấp nháy
    return;
  }

  // Truy vấn trạng thái Vault và danh sách kho
  let isUnlocked = false;
  let biometricAvailable = false;
  let availableVaults: Array<{ id: string; name: string; isDefault?: boolean }> = [];

  try {
    const [statusResp, vaultsResp] = await Promise.all([
      sendExtensionBridgeMessage<VaultStatusPayload>("get_vault_status"),
      sendExtensionBridgeMessage<any>("get_vaults"),
    ]);

    if (statusResp.ok && statusResp.payload) {
      const configured = Boolean(statusResp.payload.configured);
      isUnlocked = configured && Boolean(statusResp.payload.unlocked);
      biometricAvailable = Boolean(statusResp.payload.biometricAvailable);
    }
    if (vaultsResp.ok && Array.isArray(vaultsResp.payload?.vaults)) {
      availableVaults = vaultsResp.payload.vaults;
    }
  } catch {
    // Dự phòng
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
      box-sizing: border-box;
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
    .svm-status-badge {
      font-size: 10.5px;
      font-weight: 600;
      padding: 2px 7px;
      border-radius: 999px;
      display: inline-flex;
      align-items: center;
      gap: 3px;
      letter-spacing: 0.2px;
    }
    .svm-badge-locked {
      background: rgba(243, 139, 168, 0.15);
      color: #f38ba8;
      border: 1px solid rgba(243, 139, 168, 0.3);
    }
    .svm-badge-unlocked {
      background: rgba(166, 227, 161, 0.15);
      color: #a6e3a1;
      border: 1px solid rgba(166, 227, 161, 0.3);
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
      margin-bottom: 12px;
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
    .svm-unlock-box {
      background: rgba(30, 30, 46, 0.9);
      border: 1px solid #45475a;
      border-radius: 9px;
      padding: 10px 12px;
      margin-top: 4px;
      margin-bottom: 12px;
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .svm-unlock-header {
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 12px;
      font-weight: 600;
      color: #f9e2af;
    }
    .svm-unlock-desc {
      font-size: 11.5px;
      color: #a6adc8;
      line-height: 1.4;
    }
    .svm-toggle-pwd-link {
      background: transparent;
      border: none;
      color: #89b4fa;
      font-size: 11.5px;
      cursor: pointer;
      padding: 0;
      text-align: left;
      text-decoration: underline;
    }
    .svm-toggle-pwd-link:hover {
      color: #b4befe;
    }
    .svm-error-text {
      color: #f38ba8;
      font-size: 11.5px;
      margin-top: 2px;
      display: none;
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
      display: inline-flex;
      align-items: center;
      gap: 6px;
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
    .svm-btn-bio {
      background: #a6e3a1;
      color: #11111b;
    }
    .svm-btn-bio:hover {
      background: #94e2d5;
    }
    .svm-btn:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .svm-success-view {
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      padding: 16px 8px;
      text-align: center;
      gap: 8px;
    }
    .svm-success-icon {
      width: 42px;
      height: 42px;
      border-radius: 50%;
      background: rgba(166, 227, 161, 0.18);
      color: #a6e3a1;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .svm-success-title {
      font-size: 14.5px;
      font-weight: 700;
      color: #cdd6f4;
    }
    .svm-success-desc {
      font-size: 12px;
      color: #a6adc8;
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
      .svm-unlock-box {
        background: #f4f5f8;
        border-color: #ccd0da;
      }
      .svm-unlock-header {
        color: #df8e1d;
      }
      .svm-unlock-desc {
        color: #5c5f77;
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
      .svm-btn-bio {
        background: #40a02b;
        color: #ffffff;
      }
      .svm-btn-bio:hover {
        background: #288214;
      }
      .svm-success-title {
        color: #1e1e2e;
      }
      .svm-success-desc {
        color: #5c5f77;
      }
    }
  `;
  shadow.appendChild(style);

  const container = document.createElement("div");
  container.className = "svm-prompt";

  let autoDismissTimer: ReturnType<typeof setTimeout> | null = null;
  let lockPollTimer: ReturnType<typeof setInterval> | null = null;

  const cleanupPrompt = () => {
    if (autoDismissTimer) {
      clearTimeout(autoDismissTimer);
      autoDismissTimer = null;
    }
    if (lockPollTimer) {
      clearInterval(lockPollTimer);
      lockPollTimer = null;
    }
    window.removeEventListener("keydown", onPromptKeyDown, true);
    host.remove();
  };

  const scheduleAutoDismiss = (ms = isUnlocked ? 25000 : 60000) => {
    if (autoDismissTimer) {
      clearTimeout(autoDismissTimer);
    }
    autoDismissTimer = setTimeout(() => {
      cleanupPrompt();
    }, ms);
  };

  function onPromptKeyDown(e: KeyboardEvent) {
    if (e.key === "Escape") {
      cleanupPrompt();
    }
  }

  // Khởi động lắng nghe phím ESC
  window.addEventListener("keydown", onPromptKeyDown, true);

  // Cấu trúc giao diện
  const header = document.createElement("div");
  header.className = "svm-header";

  const brand = document.createElement("div");
  brand.className = "svm-header-brand";

  const shieldSvg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  shieldSvg.setAttribute("width", "18");
  shieldSvg.setAttribute("height", "18");
  shieldSvg.setAttribute("viewBox", "0 0 24 24");
  shieldSvg.setAttribute("fill", "none");
  shieldSvg.setAttribute("stroke", "currentColor");
  shieldSvg.setAttribute("stroke-width", "2.5");
  const shieldPath = document.createElementNS("http://www.w3.org/2000/svg", "path");
  shieldPath.setAttribute("d", "M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z");
  shieldSvg.appendChild(shieldPath);

  const titleSpan = document.createElement("span");
  titleSpan.textContent = data.isUpdate ? "Cập nhật Mật khẩu" : "Lưu vào Secure Vault?";

  const statusBadge = document.createElement("span");
  statusBadge.className = `svm-status-badge ${isUnlocked ? "svm-badge-unlocked" : "svm-badge-locked"}`;
  statusBadge.textContent = isUnlocked ? "● Đã mở" : "🔒 Đang khóa";

  brand.appendChild(shieldSvg);
  brand.appendChild(titleSpan);
  brand.appendChild(statusBadge);

  const closeBtn = document.createElement("button");
  closeBtn.type = "button";
  closeBtn.className = "svm-close-btn";
  closeBtn.title = "Đóng";
  closeBtn.textContent = "✕";
  closeBtn.addEventListener("click", cleanupPrompt);

  header.appendChild(brand);
  header.appendChild(closeBtn);

  const formRows = document.createElement("div");
  formRows.className = "svm-form-rows";

  // Dòng Tiêu đề
  const titleRow = document.createElement("div");
  titleRow.className = "svm-row";
  const titleLabel = document.createElement("label");
  titleLabel.className = "svm-label";
  titleLabel.textContent = "Tiêu đề";
  const titleWrapper = document.createElement("div");
  titleWrapper.className = "svm-input-wrapper";
  const titleInput = document.createElement("input");
  titleInput.type = "text";
  titleInput.className = "svm-input";
  titleInput.id = "svm-save-title";
  titleInput.value = data.title;
  titleWrapper.appendChild(titleInput);
  titleRow.appendChild(titleLabel);
  titleRow.appendChild(titleWrapper);
  formRows.appendChild(titleRow);

  // Dòng Tên đăng nhập
  const userRow = document.createElement("div");
  userRow.className = "svm-row";
  const userLabel = document.createElement("label");
  userLabel.className = "svm-label";
  userLabel.textContent = "Tên đăng nhập / Email";
  const userWrapper = document.createElement("div");
  userWrapper.className = "svm-input-wrapper";
  const userInput = document.createElement("input");
  userInput.type = "text";
  userInput.className = "svm-input";
  userInput.id = "svm-save-username";
  userInput.value = data.username;
  userWrapper.appendChild(userInput);
  userRow.appendChild(userLabel);
  userRow.appendChild(userWrapper);
  formRows.appendChild(userRow);

  // Dòng Mật khẩu (có nút con mắt toggle)
  const pwdRow = document.createElement("div");
  pwdRow.className = "svm-row";
  const pwdLabel = document.createElement("label");
  pwdLabel.className = "svm-label";
  pwdLabel.textContent = "Mật khẩu";
  const pwdWrapper = document.createElement("div");
  pwdWrapper.className = "svm-input-wrapper";
  const pwdInput = document.createElement("input");
  pwdInput.type = "password";
  pwdInput.className = "svm-input svm-pwd-input";
  pwdInput.id = "svm-save-password";
  pwdInput.value = data.password;

  const toggleEyeBtn = document.createElement("button");
  toggleEyeBtn.type = "button";
  toggleEyeBtn.className = "svm-toggle-eye";
  toggleEyeBtn.title = "Xem mật khẩu";
  const eyeSvg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  eyeSvg.setAttribute("width", "16");
  eyeSvg.setAttribute("height", "16");
  eyeSvg.setAttribute("viewBox", "0 0 24 24");
  eyeSvg.setAttribute("fill", "none");
  eyeSvg.setAttribute("stroke", "currentColor");
  eyeSvg.setAttribute("stroke-width", "2");
  const eyePath = document.createElementNS("http://www.w3.org/2000/svg", "path");
  eyePath.setAttribute("d", "M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z");
  const eyeCircle = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  eyeCircle.setAttribute("cx", "12");
  eyeCircle.setAttribute("cy", "12");
  eyeCircle.setAttribute("r", "3");
  eyeSvg.appendChild(eyePath);
  eyeSvg.appendChild(eyeCircle);
  toggleEyeBtn.appendChild(eyeSvg);

  pwdWrapper.appendChild(pwdInput);
  pwdWrapper.appendChild(toggleEyeBtn);

  const subInfo = document.createElement("div");
  subInfo.className = "svm-sub";
  subInfo.title = data.websiteUrl;
  subInfo.textContent = data.title;

  pwdRow.appendChild(pwdLabel);
  pwdRow.appendChild(pwdWrapper);
  pwdRow.appendChild(subInfo);
  formRows.appendChild(pwdRow);

  // Toggle con mắt mật khẩu tài khoản
  toggleEyeBtn.addEventListener("click", () => {
    pwdInput.type = pwdInput.type === "password" ? "text" : "password";
  });

  // Dòng Chọn Kho nếu có từ 2 Vault trở lên
  if (availableVaults.length >= 2) {
    const vaultRow = document.createElement("div");
    vaultRow.className = "svm-row";
    const vaultLabel = document.createElement("label");
    vaultLabel.className = "svm-label";
    vaultLabel.textContent = "Lưu vào kho";
    const vaultWrapper = document.createElement("div");
    vaultWrapper.className = "svm-input-wrapper";
    const vaultSelect = document.createElement("select");
    vaultSelect.className = "svm-input";
    vaultSelect.id = "svm-save-vault";
    vaultSelect.style.cursor = "pointer";

    for (const v of availableVaults) {
      const option = document.createElement("option");
      option.value = v.id;
      option.textContent = v.name;
      if (v.isDefault) {
        option.selected = true;
      }
      vaultSelect.appendChild(option);
    }

    vaultWrapper.appendChild(vaultSelect);
    vaultRow.appendChild(vaultLabel);
    vaultRow.appendChild(vaultWrapper);
    formRows.appendChild(vaultRow);
  }

  // Khung Mở khóa Inline (hiển thị khi kho đang khóa)
  const unlockBox = document.createElement("div");
  unlockBox.className = "svm-unlock-box";
  unlockBox.style.display = isUnlocked ? "none" : "flex";

  const unlockHeader = document.createElement("div");
  unlockHeader.className = "svm-unlock-header";
  unlockHeader.innerHTML = `<span>🔒</span><span>Kho dữ liệu đang khóa</span>`;

  const unlockDesc = document.createElement("div");
  unlockDesc.className = "svm-unlock-desc";
  unlockDesc.textContent = biometricAvailable
    ? "Xác thực sinh trắc học hoặc nhập Master Password để lưu an toàn vào kho."
    : "Nhập Master Password để mở khóa kho và lưu thông tin đăng nhập.";

  const masterPwdRow = document.createElement("div");
  masterPwdRow.className = "svm-row";
  masterPwdRow.style.display = biometricAvailable ? "none" : "flex";

  const masterPwdWrapper = document.createElement("div");
  masterPwdWrapper.className = "svm-input-wrapper";
  const masterPwdInput = document.createElement("input");
  masterPwdInput.type = "password";
  masterPwdInput.className = "svm-input svm-pwd-input";
  masterPwdInput.id = "svm-master-password";
  masterPwdInput.placeholder = "Nhập Master Password...";

  const toggleMasterEye = document.createElement("button");
  toggleMasterEye.type = "button";
  toggleMasterEye.className = "svm-toggle-eye";
  toggleMasterEye.title = "Xem mật khẩu chính";
  const masterEyeSvg = eyeSvg.cloneNode(true);
  toggleMasterEye.appendChild(masterEyeSvg);
  toggleMasterEye.addEventListener("click", () => {
    masterPwdInput.type = masterPwdInput.type === "password" ? "text" : "password";
  });

  masterPwdWrapper.appendChild(masterPwdInput);
  masterPwdWrapper.appendChild(toggleMasterEye);
  masterPwdRow.appendChild(masterPwdWrapper);

  const errorText = document.createElement("div");
  errorText.className = "svm-error-text";
  errorText.id = "svm-unlock-error";

  let togglePwdLink: HTMLButtonElement | null = null;
  if (biometricAvailable) {
    togglePwdLink = document.createElement("button");
    togglePwdLink.type = "button";
    togglePwdLink.className = "svm-toggle-pwd-link";
    togglePwdLink.textContent = "Nhập Master Password thay thế";
    togglePwdLink.addEventListener("click", () => {
      const isVisible = masterPwdRow.style.display !== "none";
      masterPwdRow.style.display = isVisible ? "none" : "flex";
      togglePwdLink!.textContent = isVisible
        ? "Nhập Master Password thay thế"
        : "Ẩn ô nhập Master Password";
      if (!isVisible) {
        masterPwdInput.focus();
      }
    });
  }

  unlockBox.appendChild(unlockHeader);
  unlockBox.appendChild(unlockDesc);
  unlockBox.appendChild(masterPwdRow);
  if (togglePwdLink) {
    unlockBox.appendChild(togglePwdLink);
  }
  unlockBox.appendChild(errorText);

  // Thanh nút hành động
  const actions = document.createElement("div");
  actions.className = "svm-actions";

  const dismissBtn = document.createElement("button");
  dismissBtn.type = "button";
  dismissBtn.className = "svm-btn svm-dismiss";
  dismissBtn.textContent = "Để sau";
  dismissBtn.addEventListener("click", cleanupPrompt);

  // Nút Sinh trắc học (nếu có hỗ trợ)
  let bioBtn: HTMLButtonElement | null = null;
  if (biometricAvailable) {
    bioBtn = document.createElement("button");
    bioBtn.type = "button";
    bioBtn.className = "svm-btn svm-btn-bio";
    bioBtn.textContent = "👆 Vân tay & Lưu";
    bioBtn.style.display = isUnlocked ? "none" : "inline-flex";
  }

  // Nút Lưu chính (Save / Unlock & Save)
  const saveBtn = document.createElement("button");
  saveBtn.type = "button";
  saveBtn.className = "svm-btn svm-save";
  saveBtn.textContent = isUnlocked
    ? data.isUpdate
      ? "Cập nhật"
      : "Lưu mật khẩu"
    : "Mở khóa & Lưu";

  // Hàm chuyển trạng thái giao diện khi kho đã mở
  function updateUiToUnlocked(): void {
    isUnlocked = true;
    statusBadge.className = "svm-status-badge svm-badge-unlocked";
    statusBadge.textContent = "● Đã mở";
    unlockBox.style.display = "none";
    if (bioBtn) {
      bioBtn.style.display = "none";
    }
    saveBtn.textContent = data.isUpdate ? "Cập nhật" : "Lưu mật khẩu";
    saveBtn.disabled = false;
    errorText.style.display = "none";
  }

  // Hàm hiển thị màn hình lưu thành công và tự đóng
  function showSuccessView(): void {
    if (lockPollTimer) {
      clearInterval(lockPollTimer);
      lockPollTimer = null;
    }
    container.innerHTML = "";

    const successWrap = document.createElement("div");
    successWrap.className = "svm-success-view";

    const successIcon = document.createElement("div");
    successIcon.className = "svm-success-icon";
    const checkSvg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    checkSvg.setAttribute("width", "24");
    checkSvg.setAttribute("height", "24");
    checkSvg.setAttribute("viewBox", "0 0 24 24");
    checkSvg.setAttribute("fill", "none");
    checkSvg.setAttribute("stroke", "currentColor");
    checkSvg.setAttribute("stroke-width", "3");
    const checkPolyline = document.createElementNS("http://www.w3.org/2000/svg", "polyline");
    checkPolyline.setAttribute("points", "20 6 9 17 4 12");
    checkSvg.appendChild(checkPolyline);
    successIcon.appendChild(checkSvg);

    const successTitle = document.createElement("div");
    successTitle.className = "svm-success-title";
    successTitle.textContent = data.isUpdate
      ? "Đã cập nhật mật khẩu an toàn!"
      : "Đã lưu vào kho an toàn!";

    const successDesc = document.createElement("div");
    successDesc.className = "svm-success-desc";
    const finalUser =
      formRows.querySelector<HTMLInputElement>("#svm-save-username")?.value.trim() || data.username;
    successDesc.textContent = `${finalUser} • ${data.websiteUrl || data.title}`;

    successWrap.appendChild(successIcon);
    successWrap.appendChild(successTitle);
    successWrap.appendChild(successDesc);
    container.appendChild(successWrap);

    setTimeout(() => {
      cleanupPrompt();
    }, 1400);
  }

  // Thực hiện lưu mật khẩu xuống Core
  async function executeSave(): Promise<boolean> {
    const finalTitle =
      formRows.querySelector<HTMLInputElement>("#svm-save-title")?.value.trim() || data.title;
    const finalUsername =
      formRows.querySelector<HTMLInputElement>("#svm-save-username")?.value.trim() || data.username;
    const finalPassword =
      formRows.querySelector<HTMLInputElement>("#svm-save-password")?.value || data.password;
    const chosenVaultId =
      formRows.querySelector<HTMLSelectElement>("#svm-save-vault")?.value || data.vaultId;

    const resp = await sendExtensionBridgeMessage<any>("save_credential", {
      ...data,
      title: finalTitle,
      username: finalUsername,
      password: finalPassword,
      vaultId: chosenVaultId,
    });

    if (resp.ok) {
      showSuccessView();
      return true;
    }

    if (resp.error?.code === "VAULT_LOCKED") {
      isUnlocked = false;
      statusBadge.className = "svm-status-badge svm-badge-locked";
      statusBadge.textContent = "🔒 Đang khóa";
      unlockBox.style.display = "flex";
      if (bioBtn) {
        bioBtn.style.display = "inline-flex";
      }
      saveBtn.textContent = "Mở khóa & Lưu";
      errorText.textContent = "Kho đang bị khóa. Hãy xác thực để lưu mật khẩu.";
      errorText.style.display = "block";
      startLockPolling();
      return false;
    }

    errorText.textContent = resp.error?.message || "Lỗi lưu mật khẩu. Vui lòng thử lại.";
    errorText.style.display = "block";
    return false;
  }

  // Xử lý mở khóa bằng Sinh trắc học & Lưu
  if (bioBtn) {
    bioBtn.addEventListener("click", async () => {
      bioBtn!.disabled = true;
      bioBtn!.textContent = "Đang xác thực...";
      saveBtn.disabled = true;
      errorText.style.display = "none";

      const bioResp = await sendExtensionBridgeMessage<any>("unlock_vault_biometric");
      if (bioResp.ok && bioResp.payload?.unlocked) {
        updateUiToUnlocked();
        bioBtn!.textContent = "Đang lưu...";
        await executeSave();
      } else {
        bioBtn!.disabled = false;
        bioBtn!.textContent = "👆 Vân tay & Lưu";
        saveBtn.disabled = false;
        errorText.textContent =
          bioResp.error?.message ||
          "Xác thực sinh trắc học không thành công. Hãy thử Master Password.";
        errorText.style.display = "block";
        masterPwdRow.style.display = "flex";
        if (togglePwdLink) {
          togglePwdLink.textContent = "Ẩn ô nhập Master Password";
        }
        masterPwdInput.focus();
      }
    });
  }

  // Xử lý nút chính (Save / Unlock & Save)
  saveBtn.addEventListener("click", async () => {
    errorText.style.display = "none";

    if (!isUnlocked) {
      const masterPwd = masterPwdInput.value;
      if (!masterPwd) {
        masterPwdRow.style.display = "flex";
        if (togglePwdLink) {
          togglePwdLink.textContent = "Ẩn ô nhập Master Password";
        }
        errorText.textContent = "Vui lòng nhập Master Password để mở khóa kho.";
        errorText.style.display = "block";
        masterPwdInput.focus();
        return;
      }

      saveBtn.disabled = true;
      if (bioBtn) {
        bioBtn.disabled = true;
      }
      saveBtn.textContent = "Đang mở khóa...";

      const unlockResp = await sendExtensionBridgeMessage<any>("unlock_vault", {
        password: masterPwd,
      });

      if (unlockResp.ok && unlockResp.payload?.unlocked) {
        updateUiToUnlocked();
        saveBtn.textContent = data.isUpdate ? "Đang cập nhật..." : "Đang lưu...";
        await executeSave();
      } else {
        saveBtn.disabled = false;
        if (bioBtn) {
          bioBtn.disabled = false;
        }
        saveBtn.textContent = "Mở khóa & Lưu";
        errorText.textContent =
          unlockResp.error?.message || "Mật khẩu chính không chính xác. Vui lòng thử lại.";
        errorText.style.display = "block";
        masterPwdInput.focus();
        masterPwdInput.select();
      }
      return;
    }

    saveBtn.disabled = true;
    saveBtn.textContent = data.isUpdate ? "Đang cập nhật..." : "Đang lưu...";
    const success = await executeSave();
    if (!success) {
      saveBtn.disabled = false;
      saveBtn.textContent = data.isUpdate ? "Cập nhật" : "Lưu mật khẩu";
    }
  });

  // Hỗ trợ phím Enter trên ô nhập Master Password để mở khóa nhanh
  masterPwdInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
      e.preventDefault();
      saveBtn.click();
    }
  });

  // Polling phát hiện kho được mở khóa từ ứng dụng Desktop hoặc Popup
  function startLockPolling(): void {
    if (lockPollTimer || isUnlocked) return;
    lockPollTimer = setInterval(async () => {
      try {
        const resp = await sendExtensionBridgeMessage<VaultStatusPayload>("get_vault_status");
        if (resp.ok && resp.payload?.unlocked) {
          if (lockPollTimer) {
            clearInterval(lockPollTimer);
            lockPollTimer = null;
          }
          updateUiToUnlocked();
        }
      } catch {
        // ignore
      }
    }, 1500);
  }

  if (!isUnlocked) {
    startLockPolling();
  }

  actions.appendChild(dismissBtn);
  if (bioBtn) {
    actions.appendChild(bioBtn);
  }
  actions.appendChild(saveBtn);

  container.appendChild(header);
  container.appendChild(formRows);
  container.appendChild(unlockBox);
  container.appendChild(actions);
  shadow.appendChild(container);
  document.documentElement.appendChild(host);

  // Tự động đóng sau khoảng thời gian nếu người dùng không tương tác
  container.addEventListener("mouseenter", () => {
    if (autoDismissTimer) {
      clearTimeout(autoDismissTimer);
      autoDismissTimer = null;
    }
  });
  container.addEventListener("focusin", () => {
    if (autoDismissTimer) {
      clearTimeout(autoDismissTimer);
      autoDismissTimer = null;
    }
  });
  container.addEventListener("mouseleave", () => {
    scheduleAutoDismiss(isUnlocked ? 20000 : 45000);
  });

  scheduleAutoDismiss();
}
