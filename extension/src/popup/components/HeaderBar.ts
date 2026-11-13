/**
 * @file extension/src/popup/components/HeaderBar.ts
 * @brief Thanh tiêu đề của Extension Popup chứa logo, trạng thái mở khóa và nút khóa nhanh.
 */

import { createGeneratorIcon, createLockIcon, createQrCodeIconSvg } from "./icons";

export interface HeaderBarProps {
  unlocked: boolean;
  configured?: boolean;
  desktopRunning?: boolean;
  showGenerator?: boolean;
  onToggleGenerator?: () => void;
  onScanQr?: () => void;
  onLockVault?: () => void;
}

export function updateHeaderGeneratorActive(active: boolean): void {
  const genBtn = document.getElementById("svm-header-gen-btn");
  if (genBtn) {
    genBtn.className = active ? "iconBtn iconBtnActive" : "iconBtn";
  }
}

export function createHeaderBar(props: HeaderBarProps): HTMLElement {
  const header = document.createElement("header");
  header.className = "headerBar";

  const brand = document.createElement("div");
  brand.className = "brandGroup";

  const logoImg = document.createElement("img");
  logoImg.src = "./app_logo.svg";
  logoImg.width = 20;
  logoImg.height = 20;
  logoImg.alt = "Secure Vault";
  logoImg.style.cssText = "display: block; flex-shrink: 0;";

  const title = document.createElement("span");
  title.className = "brandTitle";
  title.textContent = "Secure Vault";

  const statusPill = document.createElement("span");
  statusPill.className = "statusPill";
  const dot = document.createElement("span");
  dot.className = "statusDot";
  if (props.desktopRunning === false) {
    dot.style.backgroundColor = "var(--text-muted)";
    statusPill.appendChild(dot);
    statusPill.appendChild(document.createTextNode("Offline"));
  } else if (props.configured === false) {
    dot.style.backgroundColor = "var(--status-warning)";
    statusPill.appendChild(dot);
    statusPill.appendChild(document.createTextNode("Setup"));
  } else if (props.unlocked) {
    dot.style.backgroundColor = "var(--status-success)";
    statusPill.appendChild(dot);
    statusPill.appendChild(document.createTextNode("Unlocked"));
  } else {
    dot.style.backgroundColor = "var(--status-danger)";
    statusPill.appendChild(dot);
    statusPill.appendChild(document.createTextNode("Locked"));
  }

  brand.appendChild(logoImg);
  brand.appendChild(title);
  brand.appendChild(statusPill);

  header.appendChild(brand);

  if (props.unlocked && (props.onToggleGenerator || props.onLockVault || props.onScanQr)) {
    const actions = document.createElement("div");
    actions.className = "headerActions";

    if (props.onToggleGenerator) {
      const genBtn = document.createElement("button");
      genBtn.id = "svm-header-gen-btn";
      genBtn.type = "button";
      genBtn.className = props.showGenerator ? "iconBtn iconBtnActive" : "iconBtn";
      genBtn.title = "Password Generator";
      genBtn.appendChild(createGeneratorIcon(14));
      genBtn.addEventListener("click", props.onToggleGenerator);
      actions.appendChild(genBtn);
    }

    if (props.onScanQr) {
      const qrBtn = document.createElement("button");
      qrBtn.type = "button";
      qrBtn.className = "iconBtn";
      qrBtn.title = "Scan / Crop 2FA QR Code";
      qrBtn.appendChild(createQrCodeIconSvg(14));
      qrBtn.addEventListener("click", props.onScanQr);
      actions.appendChild(qrBtn);
    }

    if (props.onLockVault) {
      const lockBtn = document.createElement("button");
      lockBtn.type = "button";
      lockBtn.className = "iconBtn";
      lockBtn.title = "Lock Vault";
      lockBtn.appendChild(createLockIcon(14));
      lockBtn.addEventListener("click", props.onLockVault);
      actions.appendChild(lockBtn);
    }

    header.appendChild(actions);
  }

  return header;
}
