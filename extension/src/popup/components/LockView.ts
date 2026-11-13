/**
 * @file extension/src/popup/components/LockView.ts
 * @brief Màn hình khi Desktop App chưa chạy (Offline) hoặc Kho Mật khẩu đang khóa (Locked).
 */

import { sendExtensionBridgeMessage } from "../../services/protocol";

export interface LockViewProps {
  desktopRunning?: boolean;
  configured?: boolean;
  biometricAvailable: boolean;
  onUnlockPassword: (password: string) => Promise<boolean>;
  onUnlockBiometric: () => Promise<boolean>;
  onRefresh?: () => void;
}

function createSvg(
  viewBox: string,
  width: number,
  height: number,
  strokeWidth: number,
  children: SVGElement[],
  extraAttrs: Record<string, string> = {},
): SVGSVGElement {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", viewBox);
  svg.setAttribute("width", String(width));
  svg.setAttribute("height", String(height));
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", String(strokeWidth));
  svg.setAttribute("stroke-linecap", "round");
  svg.setAttribute("stroke-linejoin", "round");
  for (const [k, v] of Object.entries(extraAttrs)) {
    svg.setAttribute(k, v);
  }
  for (const child of children) {
    svg.appendChild(child);
  }
  return svg;
}

function createSvgPath(d: string, extraAttrs: Record<string, string> = {}): SVGPathElement {
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  path.setAttribute("d", d);
  for (const [k, v] of Object.entries(extraAttrs)) {
    path.setAttribute(k, v);
  }
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

function createSvgCircle(
  cx: number,
  cy: number,
  r: number,
  extraAttrs: Record<string, string> = {},
): SVGCircleElement {
  const circle = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  circle.setAttribute("cx", String(cx));
  circle.setAttribute("cy", String(cy));
  circle.setAttribute("r", String(r));
  for (const [k, v] of Object.entries(extraAttrs)) {
    circle.setAttribute(k, v);
  }
  return circle;
}

function createDesktopSvg(size = 28): SVGSVGElement {
  return createSvg("0 0 24 24", size, size, 1.8, [
    createSvgRect(2, 3, 20, 14, 2, 2),
    createSvgLine(8, 21, 16, 21),
    createSvgLine(12, 17, 12, 21),
  ]);
}

function createRefreshSvg(size = 13): SVGSVGElement {
  return createSvg("0 0 24 24", size, size, 2, [
    createSvgPath("M23 4v6h-6"),
    createSvgPath("M1 20v-6h6"),
    createSvgPath("M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"),
  ]);
}

function createShieldAlertSvg(size = 28): SVGSVGElement {
  return createSvg("0 0 24 24", size, size, 1.8, [
    createSvgPath("M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"),
    createSvgPath("M12 8v4"),
    createSvgPath("M12 16h.01"),
  ]);
}

function createLockSvg(size = 26): SVGSVGElement {
  return createSvg("0 0 24 24", size, size, 2, [
    createSvgRect(3, 11, 18, 11, 2, 2),
    createSvgPath("M7 11V7a5 5 0 0 1 10 0v4"),
  ]);
}

function createEyeSvg(size = 16): SVGSVGElement {
  return createSvg("0 0 24 24", size, size, 2, [
    createSvgPath("M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"),
    createSvgCircle(12, 12, 3),
  ]);
}

function createFingerprintSvg(size = 16): SVGSVGElement {
  return createSvg("0 0 24 24", size, size, 2, [
    createSvgPath("M12 11c0 2-1 3-2 4s-1 2-1 4"),
    createSvgPath("M14 18a8.3 8.3 0 0 0 1-4c0-2-1-3-2-4"),
    createSvgPath("M16 11c0-3.3-2.7-6-6-6s-6 2.7-6 6a10 10 0 0 0 1 4.5"),
    createSvgPath("M19 11a9 9 0 0 0-1-4"),
    createSvgPath("M8 11a4 4 0 0 1 4-4 4 4 0 0 1 4 4"),
  ]);
}

function createSpinnerSvg(size = 14): SVGSVGElement {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("width", String(size));
  svg.setAttribute("height", String(size));
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", "2");
  svg.style.animation = "spin 0.8s linear infinite";

  const circle = createSvgCircle(12, 12, 10, { "stroke-opacity": "0.25" });
  const path = createSvgPath("M12 2a10 10 0 0 1 10 10");

  svg.appendChild(circle);
  svg.appendChild(path);
  return svg;
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
    iconWrap.appendChild(createDesktopSvg(28));

    const heading = document.createElement("div");
    heading.className = "stateTitle";
    heading.style.cssText =
      "font-size: 16px; font-weight: 700; color: var(--text-primary); letter-spacing: -0.2px;";
    heading.textContent = "Ứng dụng Desktop chưa khởi chạy";

    const desc = document.createElement("div");
    desc.className = "stateDesc";
    desc.style.cssText =
      "max-width: 270px; line-height: 1.55; font-size: 12.5px; color: var(--text-secondary);";
    desc.textContent =
      "Vui lòng khởi chạy ứng dụng Secure Vault Manager trên máy tính để kết nối và mở khóa kho mật khẩu.";

    const actions = document.createElement("div");
    actions.style.cssText =
      "display: flex; flex-direction: column; align-items: center; gap: 8px; width: 100%; max-width: 260px; margin-top: 4px;";

    const launchBtn = document.createElement("button");
    launchBtn.type = "button";
    launchBtn.className = "primaryBtn";
    launchBtn.style.cssText =
      "width: 100%; height: 38px; font-size: 13px; font-weight: 700; border-radius: 8px; display: flex; align-items: center; justify-content: center; gap: 8px; cursor: pointer;";
    setButtonLabel(launchBtn, createDesktopSvg(15), "Mở Secure Vault Manager");

    launchBtn.addEventListener("click", async () => {
      launchBtn.disabled = true;
      setButtonLabel(launchBtn, createSpinnerSvg(14), "Đang khởi chạy...");
      try {
        await sendExtensionBridgeMessage("launch_desktop");
      } catch {
        // bỏ qua
      }
      setTimeout(() => {
        if (props.onRefresh) {
          props.onRefresh();
        }
        launchBtn.disabled = false;
        setButtonLabel(launchBtn, createDesktopSvg(15), "Mở Secure Vault Manager");
      }, 1500);
    });

    const refreshBtn = document.createElement("button");
    refreshBtn.type = "button";
    refreshBtn.className = "secondaryBtn";
    refreshBtn.style.cssText =
      "width: 100%; height: 34px; font-size: 12px; font-weight: 600; border-radius: 8px; display: flex; align-items: center; justify-content: center; gap: 6px; cursor: pointer; background: var(--bg-surface1, rgba(255, 255, 255, 0.06)); border: 1px solid var(--border-subtle, rgba(255, 255, 255, 0.12)); color: var(--text-primary);";
    setButtonLabel(refreshBtn, createRefreshSvg(13), "Kiểm tra lại kết nối");

    refreshBtn.addEventListener("click", () => {
      refreshBtn.disabled = true;
      setButtonLabel(refreshBtn, createSpinnerSvg(13), "Đang kiểm tra...");
      if (props.onRefresh) {
        props.onRefresh();
      }
      setTimeout(() => {
        refreshBtn.disabled = false;
        setButtonLabel(refreshBtn, createRefreshSvg(13), "Kiểm tra lại kết nối");
      }, 900);
    });

    const hint = document.createElement("div");
    hint.style.cssText =
      "font-size: 11px; color: var(--text-muted); text-align: center; margin-top: 2px;";
    hint.textContent = "Tiện ích sẽ tự động đồng bộ khi ứng dụng khởi động";

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
    iconWrap.appendChild(createShieldAlertSvg(28));

    const heading = document.createElement("div");
    heading.className = "stateTitle";
    heading.style.cssText =
      "font-size: 16px; font-weight: 700; color: var(--text-primary); letter-spacing: -0.2px;";
    heading.textContent = "Chưa thiết lập kho mật khẩu";

    const desc = document.createElement("div");
    desc.className = "stateDesc";
    desc.style.cssText =
      "max-width: 270px; line-height: 1.55; font-size: 12.5px; color: var(--text-secondary);";
    desc.textContent =
      "Vui lòng hoàn tất tạo Mật khẩu chính trên ứng dụng Secure Vault Manager ở máy tính để bắt đầu sử dụng tiện ích.";

    const actions = document.createElement("div");
    actions.style.cssText =
      "display: flex; flex-direction: column; align-items: center; gap: 10px; width: 100%; max-width: 260px; margin-top: 4px;";

    const refreshBtn = document.createElement("button");
    refreshBtn.type = "button";
    refreshBtn.className = "primaryBtn";
    refreshBtn.style.cssText =
      "width: 100%; height: 40px; font-size: 13px; font-weight: 700; border-radius: 8px; display: flex; align-items: center; justify-content: center; gap: 8px; cursor: pointer;";
    setButtonLabel(refreshBtn, createRefreshSvg(15), "Kiểm tra lại trạng thái");

    refreshBtn.addEventListener("click", () => {
      if (props.onRefresh) {
        props.onRefresh();
      }
    });

    const hint = document.createElement("div");
    hint.style.cssText =
      "font-size: 11px; color: var(--text-muted); text-align: center; margin-top: 2px;";
    hint.textContent = "Tiện ích sẽ tự động cập nhật ngay sau khi bạn tạo kho xong";

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
  iconWrap.appendChild(createLockSvg(26));

  const heading = document.createElement("div");
  heading.className = "stateTitle";
  heading.style.cssText =
    "font-size: 16px; font-weight: 700; color: var(--text-primary); letter-spacing: -0.2px;";
  heading.textContent = "Kho mật khẩu đang khóa";

  const desc = document.createElement("div");
  desc.className = "stateDesc";
  desc.style.cssText =
    "max-width: 270px; line-height: 1.55; font-size: 12.5px; color: var(--text-secondary);";
  desc.textContent =
    "Nhập Mật khẩu chính để mở khóa đồng thời cả tiện ích và ứng dụng Secure Vault Manager.";

  // Khung nhập Password
  const inputContainer = document.createElement("div");
  inputContainer.style.cssText =
    "position: relative; width: 100%; max-width: 270px; display: flex; align-items: center;";

  const input = document.createElement("input");
  input.type = "password";
  input.className = "inputField";
  input.placeholder = "Nhập Mật khẩu chính...";
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
  toggleBtn.appendChild(createEyeSvg(16));

  toggleBtn.addEventListener("click", () => {
    if (input.type === "password") {
      input.type = "text";
      toggleBtn.style.color = "var(--accent-primary)";
    } else {
      input.type = "password";
      toggleBtn.style.color = "var(--text-muted)";
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
  unlockBtn.textContent = "Mở khóa";

  const handleUnlock = async () => {
    const pw = input.value;
    if (!pw) {
      errorLabel.textContent = "Vui lòng nhập Mật khẩu chính";
      errorLabel.style.display = "block";
      input.focus();
      return;
    }

    unlockBtn.disabled = true;
    unlockBtn.textContent = "Đang mở khóa...";
    errorLabel.style.display = "none";

    const ok = await props.onUnlockPassword(pw);
    if (!ok) {
      unlockBtn.disabled = false;
      unlockBtn.textContent = "Mở khóa";
      errorLabel.textContent = "Mật khẩu chính không chính xác";
      errorLabel.style.display = "block";
      input.select();
      input.focus();
    }
  };

  unlockBtn.addEventListener("click", () => void handleUnlock());
  input.addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
      void handleUnlock();
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
    setButtonLabel(bioBtn, createFingerprintSvg(16), "Mở khóa bằng Sinh trắc học");

    bioBtn.addEventListener("click", async () => {
      bioBtn.disabled = true;
      const ok = await props.onUnlockBiometric();
      bioBtn.disabled = false;
      if (!ok) {
        errorLabel.textContent = "Xác thực sinh trắc học thất bại hoặc bị hủy";
        errorLabel.style.display = "block";
      }
    });
    actions.appendChild(bioBtn);
  }

  const forgotHint = document.createElement("div");
  forgotHint.style.cssText =
    "font-size: 11.5px; color: var(--text-muted); text-align: center; margin-top: 6px; cursor: pointer; text-decoration: underline;";
  forgotHint.textContent = "Quên mật khẩu hoặc mất Khóa khôi phục?";
  forgotHint.addEventListener("click", () => {
    errorLabel.textContent =
      "Vui lòng mở ứng dụng Secure Vault Manager trên Desktop để khôi phục hoặc đặt lại kho an toàn.";
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
