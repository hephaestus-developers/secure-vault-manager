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
  // Chỉ hiển thị Save Prompt ở cửa sổ cấp cao nhất (window.top) để tránh vỡ giao diện trong iFrame nhỏ/quảng cáo
  if (typeof window !== "undefined" && window.self !== window.top) {
    return;
  }

  const existing = document.getElementById("svm-save-prompt-host");
  if (existing) {
    // Đã hiển thị popup trên màn hình, không gỡ bỏ để tránh nhấp nháy / lặp lại animation
    return;
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

  brand.appendChild(shieldSvg);
  brand.appendChild(titleSpan);

  const closeBtn = document.createElement("button");
  closeBtn.type = "button";
  closeBtn.className = "svm-close-btn";
  closeBtn.title = "Đóng";
  closeBtn.textContent = "✕";

  header.appendChild(brand);
  header.appendChild(closeBtn);

  const formRows = document.createElement("div");
  formRows.className = "svm-form-rows";

  // Dòng Tiêu đề (cho phép sửa)
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

  // Dòng Tên đăng nhập (cho phép sửa)
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
  toggleEyeBtn.id = "svm-toggle-eye";
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

  let autoDismissTimer: ReturnType<typeof setTimeout> | null = null;
  const cleanupPrompt = () => {
    if (autoDismissTimer) {
      clearTimeout(autoDismissTimer);
      autoDismissTimer = null;
    }
    window.removeEventListener("keydown", onPromptKeyDown, true);
    host.remove();
  };

  const scheduleAutoDismiss = () => {
    if (autoDismissTimer) {
      clearTimeout(autoDismissTimer);
    }
    autoDismissTimer = setTimeout(() => {
      cleanupPrompt();
    }, 25000);
  };

  function onPromptKeyDown(e: KeyboardEvent) {
    if (e.key === "Escape") {
      cleanupPrompt();
    }
  }

  header.querySelector(".svm-close-btn")?.addEventListener("click", cleanupPrompt);

  const actions = document.createElement("div");
  actions.className = "svm-actions";

  const dismissBtn = document.createElement("button");
  dismissBtn.type = "button";
  dismissBtn.className = "svm-btn svm-dismiss";
  dismissBtn.textContent = "Để sau";
  dismissBtn.addEventListener("click", cleanupPrompt);

  const saveBtn = document.createElement("button");
  saveBtn.type = "button";
  saveBtn.className = "svm-btn svm-save";
  saveBtn.textContent = data.isUpdate ? "Cập nhật" : "Lưu mật khẩu";
  saveBtn.addEventListener("click", async () => {
    saveBtn.disabled = true;
    saveBtn.textContent = data.isUpdate ? "Đang cập nhật..." : "Đang lưu...";

    const finalTitle =
      formRows.querySelector<HTMLInputElement>("#svm-save-title")?.value.trim() || data.title;
    const finalUsername =
      formRows.querySelector<HTMLInputElement>("#svm-save-username")?.value.trim() || data.username;
    const finalPassword =
      formRows.querySelector<HTMLInputElement>("#svm-save-password")?.value || data.password;
    const chosenVaultId =
      formRows.querySelector<HTMLSelectElement>("#svm-save-vault")?.value || data.vaultId;

    await sendExtensionBridgeMessage("save_credential", {
      ...data,
      title: finalTitle,
      username: finalUsername,
      password: finalPassword,
      vaultId: chosenVaultId,
    });
    cleanupPrompt();
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

  window.addEventListener("keydown", onPromptKeyDown, true);
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
  container.addEventListener("mouseleave", scheduleAutoDismiss);
  scheduleAutoDismiss();
}
