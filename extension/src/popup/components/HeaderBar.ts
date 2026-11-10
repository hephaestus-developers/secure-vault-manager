/**
 * @file extension/src/popup/components/HeaderBar.ts
 * @brief Thanh tiêu đề của Extension Popup chứa logo, trạng thái mở khóa và nút khóa nhanh.
 */

export interface HeaderBarProps {
  paired: boolean;
  unlocked: boolean;
  desktopRunning?: boolean;
  onLockVault?: () => void;
}

export function createHeaderBar(props: HeaderBarProps): HTMLElement {
  const header = document.createElement("header");
  header.className = "headerBar";

  const brand = document.createElement("div");
  brand.className = "brandGroup";

  const shieldSvg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  shieldSvg.setAttribute("width", "20");
  shieldSvg.setAttribute("height", "20");
  shieldSvg.setAttribute("viewBox", "0 0 24 24");
  shieldSvg.setAttribute("fill", "none");

  const shieldPath = document.createElementNS("http://www.w3.org/2000/svg", "path");
  shieldPath.setAttribute(
    "d",
    "M12 2L3 6V11.5C3 17 6.8 21.6 12 22.8C17.2 21.6 21 17 21 11.5V6L12 2Z"
  );
  shieldPath.setAttribute("fill", "#0F766E");
  shieldPath.setAttribute("stroke", "#14B8A6");
  shieldPath.setAttribute("stroke-width", "1.8");
  shieldPath.setAttribute("stroke-linejoin", "round");

  const keyStem = document.createElementNS("http://www.w3.org/2000/svg", "path");
  keyStem.setAttribute("d", "M12 8V12");
  keyStem.setAttribute("stroke", "#F0FDFA");
  keyStem.setAttribute("stroke-width", "2");
  keyStem.setAttribute("stroke-linecap", "round");

  const keyDot = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  keyDot.setAttribute("cx", "12");
  keyDot.setAttribute("cy", "15");
  keyDot.setAttribute("r", "1.2");
  keyDot.setAttribute("fill", "#F0FDFA");

  shieldSvg.appendChild(shieldPath);
  shieldSvg.appendChild(keyStem);
  shieldSvg.appendChild(keyDot);

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
  } else if (props.unlocked) {
    dot.style.backgroundColor = "var(--status-success)";
    statusPill.appendChild(dot);
    statusPill.appendChild(document.createTextNode("Unlocked"));
  } else {
    dot.style.backgroundColor = "var(--status-danger)";
    statusPill.appendChild(dot);
    statusPill.appendChild(document.createTextNode("Locked"));
  }

  brand.appendChild(shieldSvg);
  brand.appendChild(title);
  brand.appendChild(statusPill);

  header.appendChild(brand);

  if (props.unlocked && props.onLockVault) {
    const actions = document.createElement("div");
    actions.className = "headerActions";

    const lockBtn = document.createElement("button");
    lockBtn.type = "button";
    lockBtn.className = "iconBtn";
    lockBtn.title = "Khóa kho mật khẩu (Lock Vault)";
    lockBtn.appendChild(createLockIconSvg());
    lockBtn.addEventListener("click", props.onLockVault);
    actions.appendChild(lockBtn);

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
