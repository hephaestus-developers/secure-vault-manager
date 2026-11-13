/**
 * @file extension/src/content/ui/passkey-modal.ts
 * @brief Hộp thoại xác nhận tương tác Passkey (Tạo / Xác thực / Mở khóa tại chỗ) trong Shadow DOM cô lập.
 */

import { sendExtensionBridgeMessage, VaultStatusPayload } from "../../services/protocol";
import {
  createCloseIcon,
  createDesktopIcon,
  createKeyIcon,
  createLockIcon,
  createShieldIcon,
} from "./icons";
import { createShadowContainer } from "./shadow-container";

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

const PASSKEY_MODAL_HOST_ID = "svm-passkey-modal-host";

const PASSKEY_MODAL_CSS = `
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
    overflow: hidden;
    animation: svmSlideIn 0.22s cubic-bezier(0.16, 1, 0.3, 1);
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
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    max-width: 220px;
  }
  .svm-error-text {
    color: #f38ba8;
    font-size: 12px;
    margin-bottom: 10px;
  }
  .svm-actions {
    display: flex;
    align-items: center;
    justify-content: flex-end;
    gap: 8px;
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
  .svm-vault-select {
    background: #11111b;
    color: #cdd6f4;
    border: 1px solid #45475a;
    border-radius: 6px;
    padding: 4px 8px;
    font-size: 12px;
    max-width: 180px;
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
    .svm-brand { color: #1e66f5; }
    .svm-icon-badge {
      background: #eff1f5;
      color: #1e66f5;
    }
    .svm-title { color: #1e1e2e; }
    .svm-rp-text { color: #6c6f85; }
    .svm-info-box {
      background: #f4f5f8;
      border-color: #ccd0da;
    }
    .svm-info-label { color: #8c8fa1; }
    .svm-info-val { color: #1e1e2e; }
    .svm-offline-desc,
    .svm-accounts-heading { color: #5c5f77; }
    .svm-account-option {
      background: #f4f5f8;
      border-color: #ccd0da;
    }
    .svm-account-user { color: #1e1e2e; }
    .svm-account-title { color: #7c7f93; }
    .svm-vault-select {
      background: #ffffff;
      border-color: #ccd0da;
      color: #1e1e2e;
    }
  }
`;

let activeModalCleanup: (() => void) | null = null;

export function dismissPasskeyConsent(): void {
  if (activeModalCleanup) {
    activeModalCleanup();
    activeModalCleanup = null;
  }
  const existing = document.getElementById(PASSKEY_MODAL_HOST_ID);
  if (existing) {
    existing.remove();
  }
}

export function promptPasskeyConsent(params: PasskeyModalParams): Promise<PasskeyConsentResult> {
  return new Promise((resolve) => {
    dismissPasskeyConsent();

    const {
      host,
      shadow,
      cleanup: removeHost,
    } = createShadowContainer(PASSKEY_MODAL_HOST_ID, PASSKEY_MODAL_CSS);

    const backdrop = document.createElement("div");
    backdrop.className = "svm-backdrop";

    const container = document.createElement("div");
    container.className = "svm-overlay";
    backdrop.appendChild(container);

    let isResolved = false;
    let lockPollTimer: ReturnType<typeof setInterval> | null = null;
    let isUnlocked = false;

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
      if (isResolved) return;
      isResolved = true;
      stopLockPolling();
      window.removeEventListener("keydown", onModalKeyDown, true);
      removeHost();
      activeModalCleanup = null;
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
      brand.appendChild(createShieldIcon(16));
      const brandText = document.createTextNode(" Secure Vault");
      brand.appendChild(brandText);

      const closeBtn = document.createElement("button");
      closeBtn.type = "button";
      closeBtn.className = "svm-close-btn";
      closeBtn.title = "Close";
      closeBtn.appendChild(createCloseIcon(12));
      closeBtn.addEventListener("click", () => cleanup(false, undefined, false));

      header.appendChild(brand);
      header.appendChild(closeBtn);
      container.appendChild(header);

      const body = document.createElement("div");
      body.className = "svm-body";

      // Query status from background
      const statusResp = await sendExtensionBridgeMessage<VaultStatusPayload>("get_vault_status");
      const isDesktopRunning = Boolean(statusResp.ok && statusResp.payload?.desktopRunning);
      isUnlocked = Boolean(statusResp.ok && statusResp.payload?.unlocked);

      // CASE 1: Desktop App is not running (Offline)
      if (!isDesktopRunning) {
        const titleRow = document.createElement("div");
        titleRow.className = "svm-title-row";
        const iconBadge = document.createElement("div");
        iconBadge.className = "svm-icon-badge";
        iconBadge.appendChild(createDesktopIcon(20));

        const titleTextWrap = document.createElement("div");
        const titleEl = document.createElement("div");
        titleEl.className = "svm-title";
        titleEl.textContent = "Desktop App is not running";
        const rpEl = document.createElement("div");
        rpEl.className = "svm-rp-text";
        rpEl.textContent = "Open the desktop application to use Passkeys";
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
          "Secure Vault Manager requires the desktop app to be running to securely sign and manage Passkeys.";
        infoBox.appendChild(offlineDesc);

        const actions = document.createElement("div");
        actions.className = "svm-actions";
        const cancelBtn = document.createElement("button");
        cancelBtn.type = "button";
        cancelBtn.className = "svm-btn svm-btn-secondary";
        cancelBtn.id = "svm-btn-cancel";
        cancelBtn.textContent = "Other Method";
        cancelBtn.addEventListener("click", () => cleanup(false, undefined, true));

        const retryBtn = document.createElement("button");
        retryBtn.type = "button";
        retryBtn.className = "svm-btn svm-btn-primary";
        retryBtn.id = "svm-btn-retry";
        retryBtn.textContent = "Check Again";
        retryBtn.addEventListener("click", async () => {
          retryBtn.disabled = true;
          retryBtn.textContent = "Checking...";
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

      // CASE 2: Desktop is running but Vault is Locked
      if (!isUnlocked) {
        const bioAvailable = Boolean(statusResp.ok && statusResp.payload?.biometricAvailable);

        const titleRow = document.createElement("div");
        titleRow.className = "svm-title-row";
        const iconBadge = document.createElement("div");
        iconBadge.className = "svm-icon-badge";
        iconBadge.appendChild(createLockIcon(20));

        const titleTextWrap = document.createElement("div");
        const titleEl = document.createElement("div");
        titleEl.className = "svm-title";
        titleEl.textContent = "Vault is Locked";
        const rpEl = document.createElement("div");
        rpEl.className = "svm-rp-text";
        rpEl.textContent = "Unlock securely to continue with Passkey";
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
          "For security reasons, do not enter your Master Password directly on web pages. Please unlock using Biometrics or via the extension icon / Desktop App.";
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
        btnCancel.textContent = "Other Method";
        btnCancel.addEventListener("click", () => cleanup(false, undefined, true));

        actions.appendChild(btnCancel);

        if (bioAvailable) {
          const btnBio = document.createElement("button");
          btnBio.type = "button";
          btnBio.className = "svm-btn svm-btn-primary";
          btnBio.id = "svm-btn-bio";
          btnBio.textContent = "Unlock with Biometrics";
          btnBio.addEventListener("click", async () => {
            btnBio.disabled = true;
            btnBio.textContent = "Authenticating...";
            errorMsg.textContent = "";
            const bioResp = await sendExtensionBridgeMessage<{ unlocked?: boolean }>(
              "unlock_vault_biometric",
            );
            if (bioResp.ok && bioResp.payload?.unlocked) {
              stopLockPolling();
              isUnlocked = true;
              void checkStatusAndRender();
            } else {
              btnBio.disabled = false;
              btnBio.textContent = "Unlock with Biometrics";
              errorMsg.textContent = bioResp.error?.message || "Biometric authentication failed.";
            }
          });
          actions.appendChild(btnBio);
        } else {
          const btnRetry = document.createElement("button");
          btnRetry.type = "button";
          btnRetry.className = "svm-btn svm-btn-primary";
          btnRetry.id = "svm-btn-retry";
          btnRetry.textContent = "Check Again";
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
              const pollResp =
                await sendExtensionBridgeMessage<VaultStatusPayload>("get_vault_status");
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

      // Vault is Unlocked: Show Passkey Create / Sign In confirmation
      const isCreate = params.action === "passkey_create";
      const title = isCreate ? "Save New Passkey" : "Sign in with Passkey";
      const rpId = params.rpId || "";
      const matched = params.matchedPasskeys || [];
      const primaryPasskey = matched[0];
      const userName = params.userName || primaryPasskey?.username || primaryPasskey?.title || "";
      const subtitle = isCreate
        ? "Save passkey securely into your vault"
        : "Fast authentication using passkey in Secure Vault";

      // Title & Key Icon
      const titleRow = document.createElement("div");
      titleRow.className = "svm-title-row";
      const iconBadge = document.createElement("div");
      iconBadge.className = "svm-icon-badge";
      iconBadge.appendChild(createKeyIcon(20));

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
      rpLabel.textContent = "Website:";
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
        userLabel.textContent = "Account:";
        const userVal = document.createElement("span");
        userVal.className = "svm-info-val";
        userVal.title = userName;
        userVal.textContent = userName;
        userRow.appendChild(userLabel);
        userRow.appendChild(userVal);
        infoBox.appendChild(userRow);
      }

      // Query available vaults
      let availableVaults: Array<{ id: string; name: string; isDefault?: boolean }> = [];
      const vaultsResp = await sendExtensionBridgeMessage<{
        vaults?: Array<{ id: string; name: string; isDefault?: boolean }>;
      }>("get_vaults");
      if (vaultsResp.ok && Array.isArray(vaultsResp.payload?.vaults)) {
        availableVaults = vaultsResp.payload.vaults;
      }

      let vaultSelectEl: HTMLSelectElement | null = null;
      if (isCreate && availableVaults.length >= 2) {
        const vRow = document.createElement("div");
        vRow.className = "svm-info-row";
        vRow.style.alignItems = "center";
        const vLabel = document.createElement("span");
        vLabel.className = "svm-info-label";
        vLabel.textContent = "Save to vault:";
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
        vLabel.textContent = "Vault:";
        const vVal = document.createElement("span");
        vVal.className = "svm-info-val";
        vVal.textContent = availableVaults[0].name;
        vRow.appendChild(vLabel);
        vRow.appendChild(vVal);
        infoBox.appendChild(vRow);
      }

      body.appendChild(infoBox);

      // Accounts list if multiple passkeys match
      if (!isCreate && matched.length > 1) {
        const accList = document.createElement("div");
        accList.className = "svm-accounts-list";
        const accHeading = document.createElement("div");
        accHeading.className = "svm-accounts-heading";
        accHeading.textContent = "Choose an account to sign in:";
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
          userDiv.textContent = pk.username || pk.title || "Passkey Account";
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
      btnCancel.textContent = "Other Method";
      btnCancel.addEventListener("click", () => cleanup(false, undefined, true));

      const btnConfirm = document.createElement("button");
      btnConfirm.type = "button";
      btnConfirm.className = "svm-btn svm-btn-primary";
      btnConfirm.id = "svm-btn-confirm";
      btnConfirm.textContent = isCreate ? "Save Passkey" : "Sign In";

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

    shadow.appendChild(backdrop);
    document.documentElement.appendChild(host);

    void checkStatusAndRender();
  });
}
