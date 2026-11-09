/**
 * @file extension/src/popup/components/LockView.ts
 * @brief Màn hình khi Desktop App chưa chạy (Offline) hoặc Kho Mật khẩu đang khóa (Locked).
 */

export interface LockViewProps {
  desktopRunning?: boolean;
  biometricAvailable: boolean;
  onUnlockPassword: (password: string) => Promise<boolean>;
  onUnlockBiometric: () => Promise<boolean>;
  onOpenDesktop: () => void;
  onRefresh?: () => void;
}

export function createLockView(props: LockViewProps): HTMLElement {
  const wrap = document.createElement("div");
  wrap.className = "stateCard";
  wrap.style.cssText = "padding: 32px 18px; gap: 16px;";

  const isDesktopOffline = props.desktopRunning === false;

  // ==========================================
  // TRƯỜNG HỢP 1: DESKTOP APP ĐANG TẮT (OFFLINE)
  // Không hiển thị ô mật khẩu, chỉ hiển thị nút Khởi chạy ứng dụng Desktop ở giữa.
  // ==========================================
  if (isDesktopOffline) {
    const iconWrap = document.createElement("div");
    iconWrap.style.cssText =
      "width: 52px; height: 52px; border-radius: 50%; background-color: var(--bg-surface0); display: flex; align-items: center; justify-content: center; color: var(--accent-primary); margin-bottom: 2px;";
    iconWrap.innerHTML = `
      <svg width="26" height="26" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <rect x="2" y="3" width="20" height="14" rx="2" ry="2"></rect>
        <line x1="8" y1="21" x2="16" y2="21"></line>
        <line x1="12" y1="17" x2="12" y2="21"></line>
      </svg>
    `;

    const heading = document.createElement("div");
    heading.className = "stateTitle";
    heading.style.cssText = "font-size: 16px; font-weight: 700;";
    heading.textContent = "Ứng dụng Desktop chưa khởi chạy";

    const desc = document.createElement("div");
    desc.className = "stateDesc";
    desc.style.cssText = "max-width: 260px; line-height: 1.5;";
    desc.textContent =
      "Vui lòng khởi chạy ứng dụng Secure Vault Manager trên máy tính để kết nối và mở khóa kho mật khẩu.";

    const actions = document.createElement("div");
    actions.style.cssText =
      "display: flex; flex-direction: column; gap: 10px; width: 100%; max-width: 260px; margin-top: 6px;";

    const launchBtn = document.createElement("button");
    launchBtn.type = "button";
    launchBtn.className = "primaryBtn";
    launchBtn.style.cssText =
      "height: 38px; font-size: 13px; font-weight: 700; display: flex; align-items: center; justify-content: center; gap: 8px;";
    launchBtn.innerHTML = `
      <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <polygon points="5 3 19 12 5 21 5 3"></polygon>
      </svg>
      <span>Khởi chạy ứng dụng Desktop</span>
    `;
    launchBtn.addEventListener("click", () => {
      launchBtn.disabled = true;
      launchBtn.innerHTML = `
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="animation: spin 1s linear infinite;">
          <circle cx="12" cy="12" r="10" stroke-opacity="0.25"></circle>
          <path d="M12 2a10 10 0 0 1 10 10"></path>
        </svg>
        <span>Đang khởi chạy...</span>
      `;
      props.onOpenDesktop();

      // Tự động kiểm tra lại trạng thái sau 1.5s và 3s
      if (props.onRefresh) {
        setTimeout(props.onRefresh, 1500);
        setTimeout(props.onRefresh, 3200);
      }
    });

    const refreshBtn = document.createElement("button");
    refreshBtn.type = "button";
    refreshBtn.className = "iconBtn";
    refreshBtn.style.cssText =
      "width: 100%; height: 32px; font-size: 12px; justify-content: center; gap: 6px;";
    refreshBtn.innerHTML = `
      <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <path d="M23 4v6h-6"></path>
        <path d="M1 20v-6h6"></path>
        <path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"></path>
      </svg>
      <span>Kiểm tra lại kết nối</span>
    `;
    refreshBtn.addEventListener("click", () => {
      if (props.onRefresh) {
        props.onRefresh();
      }
    });

    actions.appendChild(launchBtn);
    actions.appendChild(refreshBtn);

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
    "width: 48px; height: 48px; border-radius: 50%; background-color: var(--bg-surface0); display: flex; align-items: center; justify-content: center; color: var(--accent-primary); margin-bottom: 2px;";
  iconWrap.innerHTML = `
    <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect>
      <path d="M7 11V7a5 5 0 0 1 10 0v4"></path>
    </svg>
  `;

  const heading = document.createElement("div");
  heading.className = "stateTitle";
  heading.textContent = "Kho mật khẩu đang khóa";

  const desc = document.createElement("div");
  desc.className = "stateDesc";
  desc.textContent =
    "Nhập Mật khẩu chính để mở khóa đồng thời cả tiện ích và ứng dụng Secure Vault Manager.";

  // Khung nhập Password
  const inputContainer = document.createElement("div");
  inputContainer.style.cssText =
    "position: relative; width: 100%; max-width: 280px; display: flex; align-items: center;";

  const input = document.createElement("input");
  input.type = "password";
  input.className = "searchInput";
  input.placeholder = "Nhập Mật khẩu chính...";
  input.style.cssText =
    "width: 100%; padding-right: 36px; padding-left: 12px; height: 36px; font-size: 13px;";
  input.autocomplete = "current-password";

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
    "display: flex; flex-direction: column; gap: 8px; width: 100%; max-width: 280px; margin-top: 4px;";

  const unlockBtn = document.createElement("button");
  unlockBtn.type = "button";
  unlockBtn.className = "primaryBtn";
  unlockBtn.style.cssText = "height: 36px; font-size: 13px; font-weight: 700;";
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
      "width: 100%; height: 34px; padding: 0 12px; gap: 8px; justify-content: center; font-size: 12px; font-weight: 600;";
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

  // Nút mở ứng dụng Desktop
  const desktopRow = document.createElement("div");
  desktopRow.style.cssText =
    "display: flex; justify-content: center; width: 100%; margin-top: 6px;";
  const desktopLink = document.createElement("button");
  desktopLink.type = "button";
  desktopLink.style.cssText =
    "background: none; border: none; color: var(--text-muted); font-size: 11.5px; cursor: pointer; text-decoration: underline;";
  desktopLink.textContent = "Mở ứng dụng Desktop Secure Vault Manager";
  desktopLink.addEventListener("click", props.onOpenDesktop);
  desktopRow.appendChild(desktopLink);

  wrap.appendChild(iconWrap);
  wrap.appendChild(heading);
  wrap.appendChild(desc);
  wrap.appendChild(inputContainer);
  wrap.appendChild(errorLabel);
  wrap.appendChild(actions);
  wrap.appendChild(desktopRow);

  setTimeout(() => input.focus(), 80);

  return wrap;
}
