/**
 * @file extension/src/content/ui/save-prompt.ts
 * @brief Thanh thông báo gợi ý lưu thông tin đăng nhập với luồng mở khóa tại chỗ (Inline Unlock-to-Save).
 */

import { sendExtensionBridgeMessage, VaultStatusPayload } from "../../services/protocol";
import {
  createCheckIcon,
  createCloseIcon,
  createEyeIcon,
  createEyeOffIcon,
  createFingerprintIcon,
  createLockIcon,
  createShieldIcon,
} from "./icons";
import { createShadowContainer } from "./shadow-container";

export interface SavePromptOptions {
  itemId?: string;
  title: string;
  username: string;
  password: string;
  websiteUrl: string;
  isUpdate?: boolean;
  vaultId?: string;
}

const SAVE_PROMPT_HOST_ID = "svm-save-prompt-host";

const SAVE_PROMPT_CSS = `
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
    animation: svmSlideIn 0.22s cubic-bezier(0.16, 1, 0.3, 1);
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
    transition: opacity 0.15s ease, background 0.15s ease, filter 0.15s ease;
  }
  .svm-save:hover:not(:disabled) {
    background: #b4befe;
  }
  .svm-save:disabled {
    opacity: 0.45;
    filter: grayscale(0.5);
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
    .svm-header { color: #1e66f5; }
    .svm-label { color: #5c5f77; }
    .svm-unlock-box {
      background: #f4f5f8;
      border-color: #ccd0da;
    }
    .svm-unlock-header { color: #df8e1d; }
    .svm-unlock-desc { color: #5c5f77; }
    .svm-dismiss {
      background: #e6e9ef;
      color: #1e1e2e;
      border-color: #ccd0da;
    }
    .svm-success-title { color: #1e1e2e; }
    .svm-success-desc { color: #5c5f77; }
  }
`;

export async function showSaveCredentialPrompt(data: SavePromptOptions): Promise<void> {
  // Chỉ hiển thị Save Prompt ở cửa sổ cấp cao nhất (window.top)
  if (typeof window !== "undefined" && window.self !== window.top) {
    return;
  }

  const existing = document.getElementById(SAVE_PROMPT_HOST_ID);
  if (existing) {
    return;
  }

  // Truy vấn trạng thái Vault và danh sách kho
  let isUnlocked = false;
  let biometricAvailable = false;
  let availableVaults: Array<{ id: string; name: string; isDefault?: boolean }> = [];

  try {
    const [statusResp, vaultsResp] = await Promise.all([
      sendExtensionBridgeMessage<VaultStatusPayload>("get_vault_status"),
      sendExtensionBridgeMessage<{
        vaults?: Array<{ id: string; name: string; isDefault?: boolean }>;
      }>("get_vaults"),
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
    // Dự phòng khi background chưa sẵn sàng
  }

  const { host, shadow, cleanup } = createShadowContainer(SAVE_PROMPT_HOST_ID, SAVE_PROMPT_CSS);
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
    cleanup();
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

  window.addEventListener("keydown", onPromptKeyDown, true);

  // Header & Brand
  const header = document.createElement("div");
  header.className = "svm-header";

  const brand = document.createElement("div");
  brand.className = "svm-header-brand";

  const shieldIcon = createShieldIcon(18);

  const titleSpan = document.createElement("span");
  titleSpan.textContent = data.isUpdate ? "Update Password" : "Save to Secure Vault?";

  const statusBadge = document.createElement("span");
  statusBadge.className = `svm-status-badge ${isUnlocked ? "svm-badge-unlocked" : "svm-badge-locked"}`;
  statusBadge.textContent = isUnlocked ? "Unlocked" : "Locked";

  brand.appendChild(shieldIcon);
  brand.appendChild(titleSpan);
  brand.appendChild(statusBadge);

  const closeBtn = document.createElement("button");
  closeBtn.type = "button";
  closeBtn.className = "svm-close-btn";
  closeBtn.title = "Close";
  closeBtn.appendChild(createCloseIcon(12));
  closeBtn.addEventListener("click", cleanupPrompt);

  header.appendChild(brand);
  header.appendChild(closeBtn);

  // Form Fields
  const formRows = document.createElement("div");
  formRows.className = "svm-form-rows";

  // Title Row
  const titleRow = document.createElement("div");
  titleRow.className = "svm-row";
  const titleLabel = document.createElement("label");
  titleLabel.className = "svm-label";
  titleLabel.textContent = "Title";
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

  // Username Row
  const userRow = document.createElement("div");
  userRow.className = "svm-row";
  const userLabel = document.createElement("label");
  userLabel.className = "svm-label";
  userLabel.textContent = "Username / Email";
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

  // Password Row
  const pwdRow = document.createElement("div");
  pwdRow.className = "svm-row";
  const pwdLabel = document.createElement("label");
  pwdLabel.className = "svm-label";
  pwdLabel.textContent = "Password";
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
  toggleEyeBtn.title = "Show password";
  let isPwdRevealed = false;
  let eyeIcon = createEyeIcon(16);
  toggleEyeBtn.appendChild(eyeIcon);

  toggleEyeBtn.addEventListener("click", () => {
    isPwdRevealed = !isPwdRevealed;
    pwdInput.type = isPwdRevealed ? "text" : "password";
    toggleEyeBtn.innerHTML = "";
    eyeIcon = isPwdRevealed ? createEyeOffIcon(16) : createEyeIcon(16);
    toggleEyeBtn.appendChild(eyeIcon);
    toggleEyeBtn.title = isPwdRevealed ? "Hide password" : "Show password";
  });

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

  // Vault selector if more than 1 vault exists
  if (availableVaults.length > 1) {
    const vaultRow = document.createElement("div");
    vaultRow.className = "svm-row";
    const vaultLabel = document.createElement("label");
    vaultLabel.className = "svm-label";
    vaultLabel.textContent = "Save to vault";
    const vaultSelect = document.createElement("select");
    vaultSelect.className = "svm-input";
    vaultSelect.id = "svm-save-vault";
    for (const v of availableVaults) {
      const opt = document.createElement("option");
      opt.value = v.id;
      opt.textContent = v.name + (v.isDefault ? " (Default)" : "");
      if (v.id === data.vaultId || (!data.vaultId && v.isDefault)) {
        opt.selected = true;
      }
      vaultSelect.appendChild(opt);
    }
    vaultRow.appendChild(vaultLabel);
    vaultRow.appendChild(vaultSelect);
    formRows.appendChild(vaultRow);
  }

  // Inline Unlock-to-Save box
  const unlockBox = document.createElement("div");
  unlockBox.className = "svm-unlock-box";
  unlockBox.style.display = isUnlocked ? "none" : "flex";

  const unlockHeader = document.createElement("div");
  unlockHeader.className = "svm-unlock-header";
  const lockIcon = createLockIcon(14);
  const unlockHeaderText = document.createElement("span");
  unlockHeaderText.textContent = "Vault is Locked";
  unlockHeader.appendChild(lockIcon);
  unlockHeader.appendChild(unlockHeaderText);

  const unlockDesc = document.createElement("div");
  unlockDesc.className = "svm-unlock-desc";
  unlockDesc.textContent = biometricAvailable
    ? "Authenticate with biometrics below or enter Master Password to unlock and save:"
    : "Enter Master Password to unlock vault and save credentials:";

  const masterPwdRow = document.createElement("div");
  masterPwdRow.className = "svm-row";
  masterPwdRow.style.display = biometricAvailable ? "none" : "flex";

  const masterPwdInput = document.createElement("input");
  masterPwdInput.type = "password";
  masterPwdInput.className = "svm-input";
  masterPwdInput.placeholder = "Enter Master Password...";
  masterPwdRow.appendChild(masterPwdInput);

  let togglePwdLink: HTMLButtonElement | null = null;
  if (biometricAvailable) {
    togglePwdLink = document.createElement("button");
    togglePwdLink.type = "button";
    togglePwdLink.className = "svm-toggle-pwd-link";
    togglePwdLink.textContent = "Use Master Password";
    togglePwdLink.addEventListener("click", () => {
      const isHidden = masterPwdRow.style.display === "none";
      masterPwdRow.style.display = isHidden ? "flex" : "none";
      togglePwdLink!.textContent = isHidden ? "Hide Master Password" : "Use Master Password";
      if (isHidden) {
        masterPwdInput.focus();
      }
    });
  }

  const errorText = document.createElement("div");
  errorText.className = "svm-error-text";

  unlockBox.appendChild(unlockHeader);
  unlockBox.appendChild(unlockDesc);
  if (togglePwdLink) {
    unlockBox.appendChild(togglePwdLink);
  }
  unlockBox.appendChild(masterPwdRow);
  unlockBox.appendChild(errorText);

  // Actions
  const actions = document.createElement("div");
  actions.className = "svm-actions";

  const dismissBtn = document.createElement("button");
  dismissBtn.type = "button";
  dismissBtn.className = "svm-btn svm-dismiss";
  dismissBtn.textContent = "Not Now";
  dismissBtn.addEventListener("click", cleanupPrompt);

  let bioBtn: HTMLButtonElement | null = null;
  if (biometricAvailable) {
    bioBtn = document.createElement("button");
    bioBtn.type = "button";
    bioBtn.className = "svm-btn svm-btn-bio";
    bioBtn.appendChild(createFingerprintIcon(14));
    bioBtn.appendChild(document.createTextNode("Biometrics & Save"));
    bioBtn.style.display = isUnlocked ? "none" : "inline-flex";
  }

  const saveBtn = document.createElement("button");
  saveBtn.type = "button";
  saveBtn.className = "svm-btn svm-save";
  saveBtn.textContent = isUnlocked ? (data.isUpdate ? "Update" : "Save Password") : "Unlock & Save";
  if (!isUnlocked) {
    saveBtn.disabled = !masterPwdInput.value.trim();
  }

  masterPwdInput.addEventListener("input", () => {
    if (!isUnlocked) {
      saveBtn.disabled = !masterPwdInput.value.trim();
      errorText.style.display = "none";
    }
  });

  function updateUiToUnlocked(): void {
    isUnlocked = true;
    statusBadge.className = "svm-status-badge svm-badge-unlocked";
    statusBadge.textContent = "Unlocked";
    unlockBox.style.display = "none";
    if (bioBtn) {
      bioBtn.style.display = "none";
    }
    saveBtn.textContent = data.isUpdate ? "Update" : "Save Password";
    saveBtn.disabled = false;
    errorText.style.display = "none";
  }

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
    const checkSvg = createCheckIcon(24, 3);
    successIcon.appendChild(checkSvg);

    const successTitle = document.createElement("div");
    successTitle.className = "svm-success-title";
    successTitle.textContent = data.isUpdate
      ? "Password updated securely!"
      : "Saved to vault securely!";

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

  async function executeSave(): Promise<boolean> {
    const finalTitle =
      formRows.querySelector<HTMLInputElement>("#svm-save-title")?.value.trim() || data.title;
    const finalUsername =
      formRows.querySelector<HTMLInputElement>("#svm-save-username")?.value.trim() || data.username;
    const finalPassword =
      formRows.querySelector<HTMLInputElement>("#svm-save-password")?.value || data.password;
    const chosenVaultId =
      formRows.querySelector<HTMLSelectElement>("#svm-save-vault")?.value || data.vaultId;

    const resp = await sendExtensionBridgeMessage<Record<string, unknown>>("save_credential", {
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
      statusBadge.textContent = "Locked";
      unlockBox.style.display = "flex";
      if (bioBtn) {
        bioBtn.style.display = "inline-flex";
      }
      saveBtn.textContent = "Unlock & Save";
      saveBtn.disabled = !masterPwdInput.value.trim();
      errorText.textContent = "Vault is locked. Please authenticate to save.";
      errorText.style.display = "block";
      startLockPolling();
      return false;
    }

    errorText.textContent = resp.error?.message || "Failed to save password. Please try again.";
    errorText.style.display = "block";
    return false;
  }

  if (bioBtn) {
    bioBtn.addEventListener("click", async () => {
      bioBtn!.disabled = true;
      bioBtn!.textContent = "Authenticating...";
      saveBtn.disabled = true;
      errorText.style.display = "none";

      const bioResp = await sendExtensionBridgeMessage<{ unlocked?: boolean }>(
        "unlock_vault_biometric",
      );
      if (bioResp.ok && bioResp.payload?.unlocked) {
        updateUiToUnlocked();
        bioBtn!.textContent = "Saving...";
        await executeSave();
      } else {
        bioBtn!.disabled = false;
        bioBtn!.replaceChildren(
          createFingerprintIcon(14),
          document.createTextNode("Biometrics & Save"),
        );
        saveBtn.disabled = !masterPwdInput.value.trim();
        errorText.textContent =
          bioResp.error?.message || "Biometric authentication failed. Please try Master Password.";
        errorText.style.display = "block";
        masterPwdRow.style.display = "flex";
        if (togglePwdLink) {
          togglePwdLink.textContent = "Hide Master Password";
        }
        masterPwdInput.focus();
      }
    });
  }

  saveBtn.addEventListener("click", async () => {
    errorText.style.display = "none";

    if (!isUnlocked) {
      const masterPwd = masterPwdInput.value;
      if (!masterPwd) {
        masterPwdRow.style.display = "flex";
        if (togglePwdLink) {
          togglePwdLink.textContent = "Hide Master Password";
        }
        errorText.textContent = "Please enter Master Password to unlock vault.";
        errorText.style.display = "block";
        masterPwdInput.focus();
        return;
      }

      saveBtn.disabled = true;
      if (bioBtn) {
        bioBtn.disabled = true;
      }
      saveBtn.textContent = "Unlocking...";

      const unlockResp = await sendExtensionBridgeMessage<{ unlocked?: boolean }>("unlock_vault", {
        password: masterPwd,
      });

      if (unlockResp.ok && unlockResp.payload?.unlocked) {
        updateUiToUnlocked();
        saveBtn.textContent = data.isUpdate ? "Updating..." : "Saving...";
        await executeSave();
      } else {
        saveBtn.disabled = !masterPwdInput.value.trim();
        if (bioBtn) {
          bioBtn.disabled = false;
        }
        saveBtn.textContent = "Unlock & Save";
        errorText.textContent =
          unlockResp.error?.message || "Incorrect Master Password. Please try again.";
        errorText.style.display = "block";
        masterPwdInput.focus();
        masterPwdInput.select();
      }
      return;
    }

    saveBtn.disabled = true;
    saveBtn.textContent = data.isUpdate ? "Updating..." : "Saving...";
    const success = await executeSave();
    if (!success) {
      saveBtn.disabled = false;
      saveBtn.textContent = data.isUpdate ? "Update" : "Save Password";
    }
  });

  masterPwdInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
      e.preventDefault();
      if (!saveBtn.disabled) {
        saveBtn.click();
      }
    }
  });

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
        // Dự phòng
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
