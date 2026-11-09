/**
 * @file extension/src/popup/components/HeaderBar.ts
 * @brief Thanh tiêu đề đồng nhất của Extension Popup chứa nút chuyển đổi giao diện Sáng / Tối và trạng thái kết nối.
 */

export interface HeaderBarProps {
  theme: "dark" | "light";
  paired: boolean;
  unlocked: boolean;
  desktopRunning?: boolean;
  onToggleTheme: () => void;
  onLaunchDesktop: () => void;
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

  const actions = document.createElement("div");
  actions.className = "headerActions";

  if (props.unlocked && props.onLockVault) {
    const lockBtn = document.createElement("button");
    lockBtn.type = "button";
    lockBtn.className = "iconBtn";
    lockBtn.title = "Khóa kho mật khẩu (Lock Vault)";
    lockBtn.appendChild(createLockIconSvg());
    lockBtn.addEventListener("click", props.onLockVault);
    actions.appendChild(lockBtn);
  }

  const desktopBtn = document.createElement("button");
  desktopBtn.type = "button";
  desktopBtn.className = "iconBtn";
  desktopBtn.title = "Open Desktop App";
  desktopBtn.appendChild(createDesktopIconSvg());
  desktopBtn.addEventListener("click", props.onLaunchDesktop);

  const themeBtn = document.createElement("button");
  themeBtn.type = "button";
  themeBtn.className = "iconBtn";
  themeBtn.title = props.theme === "dark" ? "Switch to Light Mode" : "Switch to Dark Mode";
  themeBtn.appendChild(createThemeIconSvg(props.theme));
  themeBtn.addEventListener("click", props.onToggleTheme);

  actions.appendChild(desktopBtn);
  actions.appendChild(themeBtn);

  header.appendChild(brand);
  header.appendChild(actions);
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

function createDesktopIconSvg(): SVGSVGElement {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("width", "14");
  svg.setAttribute("height", "14");
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", "2");
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  path.setAttribute("d", "M4 5h16a1 1 0 011 1v10a1 1 0 01-1 1H4a1 1 0 01-1-1V6a1 1 0 011-1zm4 15h8m-4-3v3");
  svg.appendChild(path);
  return svg;
}

function createThemeIconSvg(theme: "dark" | "light"): SVGSVGElement {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("width", "15");
  svg.setAttribute("height", "15");
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", "2");
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  if (theme === "dark") {
    path.setAttribute("d", "M12 3v2m0 14v2m9-9h-2M5 12H3m15.36-6.36l-1.42 1.42M7.05 16.95l-1.42 1.42m12.73 0l-1.42-1.42M7.05 7.05L5.63 5.63M12 8a4 4 0 100 8 4 4 0 000-8z");
  } else {
    path.setAttribute("d", "M21 12.79A9 9 0 1111.21 3 7 7 0 0021 12.79z");
  }
  svg.appendChild(path);
  return svg;
}

function createLinkIconSvg(): SVGSVGElement {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("width", "15");
  svg.setAttribute("height", "15");
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", "2");
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  path.setAttribute("d", "M10 13a5 5 0 007.54.54l3-3a5 5 0 00-7.07-7.07l-1.72 1.71M14 11a5 5 0 00-7.54-.54l-3 3a5 5 0 007.07 7.07l1.71-1.71");
  svg.appendChild(path);
  return svg;
}
