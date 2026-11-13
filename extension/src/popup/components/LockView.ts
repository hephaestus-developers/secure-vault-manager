/**
 * @file extension/src/popup/components/LockView.ts
 * @brief Màn hình khi Desktop App chưa chạy (Offline) hoặc Kho Mật khẩu đang khóa (Locked).
 */

import { sendExtensionBridgeMessage } from "../../services/protocol";
import {
  createDesktopIcon,
  createEyeIcon,
  createEyeOffIcon,
  createFingerprintIcon,
  createLockIcon,
  createRefreshIcon,
  createShieldAlertIcon,
  createSpinnerIcon,
} from "./icons";

export interface LockViewProps {
  desktopRunning?: boolean;
  configured?: boolean;
  biometricAvailable: boolean;
  onUnlockPassword: (password: string) => Promise<boolean>;
  onUnlockBiometric: () => Promise<boolean>;
  onRefresh?: () => void;
}

function setButtonLabel(btn: HTMLButtonElement, icon: SVGElement, text: string): void {
  btn.replaceChildren();
  btn.appendChild(icon);
  const span = document.createElement("span");
  span.textContent = text;
  btn.appendChild(span);
}

export function createLockView(props: LockViewProps): HTMLElement {
  const wrap = document.createElement("div");
  wrap.className = "stateCard";
  wrap.style.cssText =
    "flex: 1; margin: 0; padding: 32px 20px; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 16px;";

  const isDesktopOffline = props.desktopRunning === false;

  // ==========================================
  // TRƯỜNG HỢP 1: DESKTOP APP ĐANG TẮT (OFFLINE)
  // Không hiển thị ô mật khẩu, hiển thị nút Kiểm tra lại kết nối ở giữa.
  // ==========================================
  if (isDesktopOffline) {
    const iconWrap = document.createElement("div");
    iconWrap.style.cssText =
      "width: 60px; height: 60px; border-radius: 50%; background-color: var(--bg-surface0); display: flex; align-items: center; justify-content: center; color: var(--accent-primary); border: 1px solid var(--border-subtle); margin-bottom: 4px; box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);";
    iconWrap.appendChild(createDesktopIcon(28));

    const heading = document.createElement("div");
    heading.className = "stateTitle";
    heading.style.cssText =
      "font-size: 16px; font-weight: 700; color: var(--text-primary); letter-spacing: -0.2px;";
    heading.textContent = "Desktop App is not running";

    const desc = document.createElement("div");
    desc.className = "stateDesc";
    desc.style.cssText =
      "max-width: 270px; line-height: 1.55; font-size: 12.5px; color: var(--text-secondary);";
    desc.textContent =
      "Please launch the Secure Vault Manager desktop application to connect and unlock your vault.";

    const actions = document.createElement("div");
    actions.style.cssText =
      "display: flex; flex-direction: column; align-items: center; gap: 8px; width: 100%; max-width: 260px; margin-top: 4px;";

    const launchBtn = document.createElement("button");
    launchBtn.type = "button";
    launchBtn.className = "primaryBtn";
    launchBtn.style.cssText =
      "width: 100%; height: 38px; font-size: 13px; font-weight: 700; border-radius: 8px; display: flex; align-items: center; justify-content: center; gap: 8px; cursor: pointer;";
    setButtonLabel(launchBtn, createDesktopIcon(15), "Open Secure Vault Manager");

    launchBtn.addEventListener("click", async () => {
      launchBtn.disabled = true;
      setButtonLabel(launchBtn, createSpinnerIcon(14), "Launching...");
      try {
        await sendExtensionBridgeMessage("launch_desktop");
      } catch {
        // ignore
      }
      setTimeout(() => {
        if (props.onRefresh) {
          props.onRefresh();
        }
        launchBtn.disabled = false;
        setButtonLabel(launchBtn, createDesktopIcon(15), "Open Secure Vault Manager");
      }, 1500);
    });

    const refreshBtn = document.createElement("button");
    refreshBtn.type = "button";
    refreshBtn.className = "secondaryBtn";
    refreshBtn.style.cssText =
      "width: 100%; height: 34px; font-size: 12px; font-weight: 600; border-radius: 8px; display: flex; align-items: center; justify-content: center; gap: 6px; cursor: pointer; background: var(--bg-surface1, rgba(255, 255, 255, 0.06)); border: 1px solid var(--border-subtle, rgba(255, 255, 255, 0.12)); color: var(--text-primary);";
    setButtonLabel(refreshBtn, createRefreshIcon(13), "Check Connection");

    refreshBtn.addEventListener("click", () => {
      refreshBtn.disabled = true;
      setButtonLabel(refreshBtn, createSpinnerIcon(13), "Checking...");
      if (props.onRefresh) {
        props.onRefresh();
      }
      setTimeout(() => {
        refreshBtn.disabled = false;
        setButtonLabel(refreshBtn, createRefreshIcon(13), "Check Connection");
      }, 900);
    });

    const hint = document.createElement("div");
    hint.style.cssText =
      "font-size: 11px; color: var(--text-muted); text-align: center; margin-top: 2px;";
    hint.textContent = "The extension will automatically sync when the app starts";

    actions.appendChild(launchBtn);
    actions.appendChild(refreshBtn);
    actions.appendChild(hint);

    wrap.appendChild(iconWrap);
    wrap.appendChild(heading);
    wrap.appendChild(desc);
    wrap.appendChild(actions);
    return wrap;
  }

  // ==========================================
  // TRƯỜNG HỢP 1.5: DESKTOP APP ĐANG Ở MÀN ONBOARDING (CHƯA TẠO KHO)
  // ==========================================
  if (props.configured === false) {
    const iconWrap = document.createElement("div");
    iconWrap.style.cssText =
      "width: 60px; height: 60px; border-radius: 50%; background-color: var(--bg-surface0); display: flex; align-items: center; justify-content: center; color: var(--accent-primary); border: 1px solid var(--border-subtle); margin-bottom: 4px; box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);";
    iconWrap.appendChild(createShieldAlertIcon(28));

    const heading = document.createElement("div");
    heading.className = "stateTitle";
    heading.style.cssText =
      "font-size: 16px; font-weight: 700; color: var(--text-primary); letter-spacing: -0.2px;";
    heading.textContent = "Vault Not Set Up";

    const desc = document.createElement("div");
    desc.className = "stateDesc";
    desc.style.cssText =
      "max-width: 270px; line-height: 1.55; font-size: 12.5px; color: var(--text-secondary);";
    desc.textContent =
      "Please complete vault setup and create your Master Password in the Desktop App to use the extension.";

    const actions = document.createElement("div");
    actions.style.cssText =
      "display: flex; flex-direction: column; align-items: center; gap: 10px; width: 100%; max-width: 260px; margin-top: 4px;";

    const refreshBtn = document.createElement("button");
    refreshBtn.type = "button";
    refreshBtn.className = "primaryBtn";
    refreshBtn.style.cssText =
      "width: 100%; height: 40px; font-size: 13px; font-weight: 700; border-radius: 8px; display: flex; align-items: center; justify-content: center; gap: 8px; cursor: pointer;";
    setButtonLabel(refreshBtn, createRefreshIcon(15), "Check Status");

    refreshBtn.addEventListener("click", () => {
      if (props.onRefresh) {
        props.onRefresh();
      }
    });

    const hint = document.createElement("div");
    hint.style.cssText =
      "font-size: 11px; color: var(--text-muted); text-align: center; margin-top: 2px;";
    hint.textContent = "The extension will automatically update once setup is complete";

    actions.appendChild(refreshBtn);
    actions.appendChild(hint);

    wrap.appendChild(iconWrap);
    wrap.appendChild(heading);
    wrap.appendChild(desc);
    wrap.appendChild(actions);
    return wrap;
  }

  // ==========================================
  // TRƯỜNG HỢP 2: DESKTOP APP ĐANG CHẠY NHƯNG KHO BỊ KHÓA
  // Hiển thị ô nhập Mật khẩu chính và Sinh trắc học
  // ==========================================
  const iconWrap = document.createElement("div");
  iconWrap.style.cssText =
    "width: 56px; height: 56px; border-radius: 50%; background-color: var(--bg-surface0); display: flex; align-items: center; justify-content: center; color: var(--accent-primary); border: 1px solid var(--border-subtle); margin-bottom: 2px; box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);";
  iconWrap.appendChild(createLockIcon(26));

  const heading = document.createElement("div");
  heading.className = "stateTitle";
  heading.style.cssText =
    "font-size: 16px; font-weight: 700; color: var(--text-primary); letter-spacing: -0.2px;";
  heading.textContent = "Vault is Locked";

  const desc = document.createElement("div");
  desc.className = "stateDesc";
  desc.style.cssText =
    "max-width: 270px; line-height: 1.55; font-size: 12.5px; color: var(--text-secondary);";
  desc.textContent = "Enter your Master Password to unlock your credentials.";

  // Khung nhập Password
  const inputContainer = document.createElement("div");
  inputContainer.style.cssText =
    "position: relative; width: 100%; max-width: 270px; display: flex; align-items: center;";

  const input = document.createElement("input");
  input.type = "password";
  input.className = "inputField";
  input.placeholder = "Enter Master Password...";
  input.style.cssText =
    "width: 100%; padding-right: 36px; padding-left: 12px; height: 38px; font-size: 13px; border-radius: 8px; background-color: var(--bg-mantle); color: var(--text-primary); border: 1px solid var(--border-subtle); outline: none;";
  input.autocomplete = "current-password";

  input.addEventListener("focus", () => {
    input.style.borderColor = "var(--accent-primary)";
    input.style.boxShadow = "0 0 0 2px rgba(137, 180, 250, 0.25)";
  });
  input.addEventListener("blur", () => {
    input.style.borderColor = "var(--border-subtle)";
    input.style.boxShadow = "none";
  });

  // Nút Show/Hide password
  const toggleBtn = document.createElement("button");
  toggleBtn.type = "button";
  toggleBtn.style.cssText =
    "position: absolute; right: 8px; background: none; border: none; cursor: pointer; color: var(--text-muted); display: flex; align-items: center; justify-content: center; padding: 4px;";
  toggleBtn.appendChild(createEyeIcon(16));

  toggleBtn.addEventListener("click", () => {
    if (input.type === "password") {
      input.type = "text";
      toggleBtn.style.color = "var(--accent-primary)";
      toggleBtn.replaceChildren(createEyeOffIcon(16));
    } else {
      input.type = "password";
      toggleBtn.style.color = "var(--text-muted)";
      toggleBtn.replaceChildren(createEyeIcon(16));
    }
  });

  inputContainer.appendChild(input);
  inputContainer.appendChild(toggleBtn);

  // Thông báo lỗi
  const errorLabel = document.createElement("div");
  errorLabel.style.cssText =
    "color: var(--status-danger); font-size: 11.5px; font-weight: 600; min-height: 16px; margin-top: -6px; display: none;";

  // Nút Thao tác
  const actions = document.createElement("div");
  actions.style.cssText =
    "display: flex; flex-direction: column; gap: 8px; width: 100%; max-width: 270px; margin-top: 4px;";

  const unlockBtn = document.createElement("button");
  unlockBtn.type = "button";
  unlockBtn.className = "primaryBtn";
  unlockBtn.style.cssText =
    "height: 38px; font-size: 13px; font-weight: 700; border-radius: 8px; cursor: pointer;";
  unlockBtn.textContent = "Unlock";
  unlockBtn.disabled = !input.value.trim();

  input.addEventListener("input", () => {
    unlockBtn.disabled = !input.value.trim();
    if (errorLabel.style.display !== "none") {
      errorLabel.style.display = "none";
    }
  });

  const handleUnlock = async () => {
    const pw = input.value;
    if (!pw) {
      errorLabel.textContent = "Please enter your Master Password";
      errorLabel.style.display = "block";
      input.focus();
      return;
    }

    unlockBtn.disabled = true;
    unlockBtn.textContent = "Unlocking...";
    errorLabel.style.display = "none";

    const ok = await props.onUnlockPassword(pw);
    if (!ok) {
      unlockBtn.disabled = !input.value.trim();
      unlockBtn.textContent = "Unlock";
      errorLabel.textContent = "Incorrect Master Password";
      errorLabel.style.display = "block";
      input.select();
      input.focus();
    }
  };

  unlockBtn.addEventListener("click", () => void handleUnlock());
  input.addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
      if (!unlockBtn.disabled) {
        void handleUnlock();
      }
    }
  });

  actions.appendChild(unlockBtn);

  // Nút Sinh trắc học (nếu khả dụng)
  if (props.biometricAvailable) {
    const bioBtn = document.createElement("button");
    bioBtn.type = "button";
    bioBtn.className = "iconBtn";
    bioBtn.style.cssText =
      "width: 100%; height: 36px; padding: 0 12px; gap: 8px; justify-content: center; font-size: 12px; font-weight: 600; border-radius: 8px; cursor: pointer;";
    setButtonLabel(bioBtn, createFingerprintIcon(16), "Unlock with Biometrics");

    bioBtn.addEventListener("click", async () => {
      bioBtn.disabled = true;
      const ok = await props.onUnlockBiometric();
      bioBtn.disabled = false;
      if (!ok) {
        errorLabel.textContent = "Biometric authentication failed or was cancelled";
        errorLabel.style.display = "block";
      }
    });
    actions.appendChild(bioBtn);
  }

  const forgotHint = document.createElement("div");
  forgotHint.style.cssText =
    "font-size: 11.5px; color: var(--text-muted); text-align: center; margin-top: 6px; cursor: pointer; text-decoration: underline;";
  forgotHint.textContent = "Forgot password or lost recovery key?";
  forgotHint.addEventListener("click", () => {
    errorLabel.textContent =
      "Please open Secure Vault Manager on your desktop to recover or reset your vault.";
    errorLabel.style.display = "block";
    errorLabel.style.color = "var(--accent-primary)";
  });
  actions.appendChild(forgotHint);

  wrap.appendChild(iconWrap);
  wrap.appendChild(heading);
  wrap.appendChild(desc);
  wrap.appendChild(inputContainer);
  wrap.appendChild(errorLabel);
  wrap.appendChild(actions);

  setTimeout(() => input.focus(), 80);

  return wrap;
}
