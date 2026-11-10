/**
 * @file extension/src/popup/components/LockView.ts
 * @brief Màn hình khi Desktop App chưa chạy (Offline) hoặc Kho Mật khẩu đang khóa (Locked).
 */

export interface LockViewProps {
  desktopRunning?: boolean;
  biometricAvailable: boolean;
  onUnlockPassword: (password: string) => Promise<boolean>;
  onUnlockBiometric: () => Promise<boolean>;
  onRefresh?: () => void;
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
    iconWrap.innerHTML = `
      <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
        <rect x="2" y="3" width="20" height="14" rx="2" ry="2"></rect>
        <line x1="8" y1="21" x2="16" y2="21"></line>
        <line x1="12" y1="17" x2="12" y2="21"></line>
      </svg>
    `;

    const heading = document.createElement("div");
    heading.className = "stateTitle";
    heading.style.cssText = "font-size: 16px; font-weight: 700; color: var(--text-primary); letter-spacing: -0.2px;";
    heading.textContent = "Ứng dụng Desktop chưa khởi chạy";

    const desc = document.createElement("div");
    desc.className = "stateDesc";
    desc.style.cssText = "max-width: 270px; line-height: 1.55; font-size: 12.5px; color: var(--text-secondary);";
    desc.textContent =
      "Vui lòng khởi chạy ứng dụng Secure Vault Manager trên máy tính để kết nối và mở khóa kho mật khẩu.";

    const actions = document.createElement("div");
    actions.style.cssText =
      "display: flex; flex-direction: column; align-items: center; gap: 10px; width: 100%; max-width: 260px; margin-top: 4px;";

    const refreshBtn = document.createElement("button");
    refreshBtn.type = "button";
    refreshBtn.className = "primaryBtn";
    refreshBtn.style.cssText =
      "width: 100%; height: 40px; font-size: 13px; font-weight: 700; border-radius: 8px; display: flex; align-items: center; justify-content: center; gap: 8px; cursor: pointer;";
    refreshBtn.innerHTML = `
      <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
        <path d="M23 4v6h-6"></path>
        <path d="M1 20v-6h6"></path>
        <path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"></path>
      </svg>
      <span>Kiểm tra lại kết nối</span>
    `;
    refreshBtn.addEventListener("click", () => {
      refreshBtn.disabled = true;
      const originalHtml = refreshBtn.innerHTML;
      refreshBtn.innerHTML = `
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="animation: spin 0.8s linear infinite;">
          <circle cx="12" cy="12" r="10" stroke-opacity="0.25"></circle>
          <path d="M12 2a10 10 0 0 1 10 10"></path>
        </svg>
        <span>Đang kiểm tra kết nối...</span>
      `;
      if (props.onRefresh) {
        props.onRefresh();
      }
      setTimeout(() => {
        refreshBtn.disabled = false;
        refreshBtn.innerHTML = originalHtml;
      }, 900);
    });

    const hint = document.createElement("div");
    hint.style.cssText = "font-size: 11px; color: var(--text-muted); text-align: center; margin-top: 2px;";
    hint.textContent = "Tiện ích sẽ tự động đồng bộ khi ứng dụng khởi động";

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
  iconWrap.innerHTML = `
    <svg width="26" height="26" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect>
      <path d="M7 11V7a5 5 0 0 1 10 0v4"></path>
    </svg>
  `;

  const heading = document.createElement("div");
  heading.className = "stateTitle";
  heading.style.cssText = "font-size: 16px; font-weight: 700; color: var(--text-primary); letter-spacing: -0.2px;";
  heading.textContent = "Kho mật khẩu đang khóa";

  const desc = document.createElement("div");
  desc.className = "stateDesc";
  desc.style.cssText = "max-width: 270px; line-height: 1.55; font-size: 12.5px; color: var(--text-secondary);";
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
  toggleBtn.innerHTML = `
    <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path>
      <circle cx="12" cy="12" r="3"></circle>
    </svg>
  `;
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
  unlockBtn.style.cssText = "height: 38px; font-size: 13px; font-weight: 700; border-radius: 8px; cursor: pointer;";
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
    bioBtn.innerHTML = `
      <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <path d="M12 11c0 2-1 3-2 4s-1 2-1 4"></path>
        <path d="M14 18a8.3 8.3 0 0 0 1-4c0-2-1-3-2-4"></path>
        <path d="M16 11c0-3.3-2.7-6-6-6s-6 2.7-6 6a10 10 0 0 0 1 4.5"></path>
        <path d="M19 11a9 9 0 0 0-1-4"></path>
        <path d="M8 11a4 4 0 0 1 4-4 4 4 0 0 1 4 4"></path>
      </svg>
      <span>Mở khóa bằng Sinh trắc học</span>
    `;
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

  wrap.appendChild(iconWrap);
  wrap.appendChild(heading);
  wrap.appendChild(desc);
  wrap.appendChild(inputContainer);
  wrap.appendChild(errorLabel);
  wrap.appendChild(actions);

  setTimeout(() => input.focus(), 80);

  return wrap;
}
