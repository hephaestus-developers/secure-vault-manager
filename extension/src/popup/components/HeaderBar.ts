/**
 * @file extension/src/popup/components/HeaderBar.ts
 * @brief Thanh tiêu đề của Extension Popup chứa logo, trạng thái mở khóa và nút khóa nhanh.
 */

import { createQrCodeIconSvg, createSvgPathIcon } from "./icons";

export interface HeaderBarProps {
  unlocked: boolean;
  configured?: boolean;
  desktopRunning?: boolean;
  showGenerator?: boolean;
  onToggleGenerator?: () => void;
  onScanQr?: () => void;
  onLockVault?: () => void;
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
      genBtn.type = "button";
      genBtn.className = props.showGenerator ? "iconBtn iconBtnActive" : "iconBtn";
      genBtn.title = "Trình tạo mật khẩu (Password Generator)";
      genBtn.appendChild(
        createSvgPathIcon(
          "M21 2l-2 2m-7.61 7.61a5.5 5.5 0 11-7.778 7.778 5.5 5.5 0 017.777-7.777zm0 0L15.5 7.5m0 0l3 3L22 7l-3-3m-3.5 3.5L19 4",
          14,
        ),
      );
      genBtn.addEventListener("click", props.onToggleGenerator);
      actions.appendChild(genBtn);
    }

    if (props.onScanQr) {
      const qrBtn = document.createElement("button");
      qrBtn.type = "button";
      qrBtn.className = "iconBtn";
      qrBtn.title = "Quét / Crop mã QR 2FA trên trang";
      qrBtn.appendChild(createQrCodeIconSvg(14));
      qrBtn.addEventListener("click", props.onScanQr);
      actions.appendChild(qrBtn);
    }

    if (props.onLockVault) {
      const lockBtn = document.createElement("button");
      lockBtn.type = "button";
      lockBtn.className = "iconBtn";
      lockBtn.title = "Khóa kho mật khẩu (Lock Vault)";
      lockBtn.appendChild(createLockIconSvg());
      lockBtn.addEventListener("click", props.onLockVault);
      actions.appendChild(lockBtn);
    }

    header.appendChild(actions);
  }

  return header;
}

function createLockIconSvg(): SVGSVGElement {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("width", "14");
  svg.setAttribute("height", "14");
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", "2");
  const rect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
  rect.setAttribute("x", "3");
  rect.setAttribute("y", "11");
  rect.setAttribute("width", "18");
  rect.setAttribute("height", "11");
  rect.setAttribute("rx", "2");
  rect.setAttribute("ry", "2");
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  path.setAttribute("d", "M7 11V7a5 5 0 0110 0v4");
  svg.appendChild(rect);
  svg.appendChild(path);
  return svg;
}
