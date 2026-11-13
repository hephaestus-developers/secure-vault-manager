/**
 * @file extension/src/content/ui/crop-overlay.ts
 * @brief Lớp phủ chọn vùng (Crop Overlay) và Toast thông báo kết quả quét QR 2FA trên trang web.
 */

import { autofillCredentialsOnPage } from "../autofill";

const CROP_HOST_ID = "svm-qr-crop-host";
const QR_TOAST_HOST_ID = "svm-qr-toast-host";

export interface QrCropResultToastParams {
  ok: boolean;
  isInfo?: boolean;
  title: string;
  message: string;
  code?: string;
}

let activeCropCleanup: (() => void) | null = null;

/**
 * Gỡ bỏ Crop Overlay nếu đang mở.
 */
export function dismissQrCropOverlay(): void {
  if (activeCropCleanup) {
    activeCropCleanup();
    activeCropCleanup = null;
  }
  const existing = document.getElementById(CROP_HOST_ID);
  if (existing) {
    existing.remove();
  }
}

/**
 * Hiển thị lớp phủ kéo-thả vùng chọn để cắt mã QR trên trang hiện tại.
 */
export function startQrCropOverlay(itemTitle?: string): void {
  dismissQrCropOverlay();
  if (!document.documentElement) {
    return;
  }

  const host = document.createElement("div");
  host.id = CROP_HOST_ID;
  host.style.cssText =
    "position: fixed !important; inset: 0 !important; width: 100vw !important; height: 100vh !important; " +
    "z-index: 2147483647 !important; pointer-events: auto !important; margin: 0 !important; padding: 0 !important;";

  const shadow = host.attachShadow({ mode: "closed" });

  const style = document.createElement("style");
  style.textContent = `
    .backdrop {
      position: fixed;
      inset: 0;
      cursor: crosshair;
      user-select: none;
      -webkit-user-select: none;
      background: rgba(15, 23, 42, 0.48);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    }
    .backdrop.dragging {
      background: transparent;
    }
    .toolbar {
      position: fixed;
      top: 16px;
      left: 50%;
      transform: translateX(-50%);
      display: inline-flex;
      align-items: center;
      gap: 10px;
      padding: 9px 14px;
      border-radius: 999px;
      background: rgba(24, 24, 37, 0.96);
      color: #cdd6f4;
      border: 1px solid #45475a;
      box-shadow: 0 12px 30px rgba(0, 0, 0, 0.45);
      font-size: 12.5px;
      font-weight: 500;
      z-index: 2;
      cursor: default;
      max-width: 92vw;
    }
    .toolbarAccent {
      color: #89b4fa;
      font-weight: 700;
    }
    .toolbarHint {
      color: #f9e2af;
      font-size: 11.5px;
      margin-left: 4px;
    }
    .cancelBtn {
      border: 1px solid #45475a;
      background: #313244;
      color: #cdd6f4;
      border-radius: 999px;
      padding: 4px 10px;
      font-size: 11.5px;
      font-weight: 600;
      cursor: pointer;
    }
    .cancelBtn:hover {
      background: #45475a;
      color: #ffffff;
    }
    .selectionBox {
      position: fixed;
      display: none;
      border: 2px solid #89b4fa;
      border-radius: 6px;
      box-shadow: 0 0 0 99999px rgba(15, 23, 42, 0.52);
      pointer-events: none;
      box-sizing: border-box;
    }
  `;

  const backdrop = document.createElement("div");
  backdrop.className = "backdrop";

  const toolbar = document.createElement("div");
  toolbar.className = "toolbar";

  const label = document.createElement("span");
  label.textContent = "Drag selection box around 2FA QR code";
  toolbar.appendChild(label);

  if (itemTitle && itemTitle.trim()) {
    const targetSpan = document.createElement("span");
    targetSpan.className = "toolbarAccent";
    targetSpan.textContent = `• ${itemTitle.trim()}`;
    toolbar.appendChild(targetSpan);
  }

  const hintSpan = document.createElement("span");
  hintSpan.className = "toolbarHint";
  toolbar.appendChild(hintSpan);

  const cancelBtn = document.createElement("button");
  cancelBtn.type = "button";
  cancelBtn.className = "cancelBtn";
  cancelBtn.textContent = "Cancel (Esc)";
  toolbar.appendChild(cancelBtn);

  const selectionBox = document.createElement("div");
  selectionBox.className = "selectionBox";

  backdrop.appendChild(selectionBox);
  backdrop.appendChild(toolbar);
  shadow.appendChild(style);
  shadow.appendChild(backdrop);

  let isDragging = false;
  let startX = 0;
  let startY = 0;
  let curX = 0;
  let curY = 0;

  const sendCancelToBackground = () => {
    const extChrome = (globalThis as Record<string, any>).chrome;
    if (extChrome?.runtime?.sendMessage) {
      extChrome.runtime.sendMessage({ action: "qr_crop_cancelled", payload: {} });
    }
  };

  const onKeyDown = (e: KeyboardEvent) => {
    if (e.key === "Escape") {
      e.preventDefault();
      e.stopPropagation();
      dismissQrCropOverlay();
      sendCancelToBackground();
    }
  };

  const cleanupListeners = () => {
    window.removeEventListener("keydown", onKeyDown, true);
  };
  activeCropCleanup = cleanupListeners;

  window.addEventListener("keydown", onKeyDown, true);

  cancelBtn.addEventListener("pointerdown", (e) => {
    e.stopPropagation();
  });
  cancelBtn.addEventListener("click", (e) => {
    e.stopPropagation();
    dismissQrCropOverlay();
    sendCancelToBackground();
  });
  toolbar.addEventListener("pointerdown", (e) => {
    e.stopPropagation();
  });

  const updateBox = () => {
    const left = Math.min(startX, curX);
    const top = Math.min(startY, curY);
    const width = Math.abs(curX - startX);
    const height = Math.abs(curY - startY);
    selectionBox.style.left = `${left}px`;
    selectionBox.style.top = `${top}px`;
    selectionBox.style.width = `${width}px`;
    selectionBox.style.height = `${height}px`;
    return { left, top, width, height };
  };

  backdrop.addEventListener("pointerdown", (e) => {
    if (e.button !== 0) return;
    e.preventDefault();
    e.stopPropagation();
    isDragging = true;
    startX = e.clientX;
    startY = e.clientY;
    curX = e.clientX;
    curY = e.clientY;
    hintSpan.textContent = "";
    backdrop.classList.add("dragging");
    selectionBox.style.display = "block";
    updateBox();
    try {
      backdrop.setPointerCapture(e.pointerId);
    } catch {
      // ignore if unsupported
    }
  });

  backdrop.addEventListener("pointermove", (e) => {
    if (!isDragging) return;
    e.preventDefault();
    curX = Math.max(0, Math.min(window.innerWidth, e.clientX));
    curY = Math.max(0, Math.min(window.innerHeight, e.clientY));
    updateBox();
  });

  const finishDrag = (e: PointerEvent) => {
    if (!isDragging) return;
    isDragging = false;
    e.preventDefault();
    curX = Math.max(0, Math.min(window.innerWidth, e.clientX));
    curY = Math.max(0, Math.min(window.innerHeight, e.clientY));
    const rect = updateBox();

    try {
      backdrop.releasePointerCapture(e.pointerId);
    } catch {
      // ignore
    }

    if (rect.width < 16 || rect.height < 16) {
      backdrop.classList.remove("dragging");
      selectionBox.style.display = "none";
      hintSpan.textContent = "Selection too small, please drag a larger area";
      return;
    }

    // Ẩn toàn bộ overlay trước khi Background gọi captureVisibleTab
    host.style.display = "none";
    requestAnimationFrame(() => {
      requestAnimationFrame(() => {
        dismissQrCropOverlay();
        const extChrome = (globalThis as Record<string, any>).chrome;
        if (extChrome?.runtime?.sendMessage) {
          extChrome.runtime.sendMessage({
            action: "qr_crop_selected",
            payload: {
              rect: {
                x: rect.left,
                y: rect.top,
                width: rect.width,
                height: rect.height,
              },
              devicePixelRatio: window.devicePixelRatio || 1,
            },
          });
        }
      });
    });
  };

  backdrop.addEventListener("pointerup", finishDrag);
  backdrop.addEventListener("pointercancel", () => {
    isDragging = false;
    backdrop.classList.remove("dragging");
    selectionBox.style.display = "none";
  });

  document.documentElement.appendChild(host);
}

/**
 * Hiển thị toast kết quả quét/cập nhật mã QR 2FA ở góc trên-phải màn hình.
 */
export function showQrResultToast(params: QrCropResultToastParams): void {
  const existing = document.getElementById(QR_TOAST_HOST_ID);
  if (existing) {
    existing.remove();
  }
  if (!document.body && !document.documentElement) {
    return;
  }

  const host = document.createElement("div");
  host.id = QR_TOAST_HOST_ID;
  host.style.cssText =
    "position: fixed !important; top: 16px !important; right: 16px !important; z-index: 2147483647 !important;";

  const shadow = host.attachShadow({ mode: "closed" });
  const style = document.createElement("style");
  style.textContent = `
    .card {
      width: 340px;
      background: #1e1e2e;
      color: #cdd6f4;
      border: 1px solid #313244;
      border-radius: 12px;
      padding: 14px 16px;
      box-shadow: 0 14px 36px rgba(0, 0, 0, 0.52);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      font-size: 12.5px;
    }
    .header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
      margin-bottom: 6px;
    }
    .title {
      font-weight: 700;
      font-size: 13px;
      color: ${params.ok ? "#a6e3a1" : params.isInfo ? "#89b4fa" : "#f38ba8"};
    }
    .closeBtn {
      background: transparent;
      border: none;
      color: #9399b2;
      font-size: 16px;
      line-height: 1;
      cursor: pointer;
      padding: 0 4px;
    }
    .closeBtn:hover {
      color: #ffffff;
    }
    .msg {
      color: #bac2de;
      line-height: 1.45;
      margin-bottom: ${params.code ? "10px" : "0"};
    }
    .codeRow {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
      background: #181825;
      border: 1px solid #313244;
      border-radius: 8px;
      padding: 8px 10px;
    }
    .codeVal {
      font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
      font-size: 18px;
      font-weight: 700;
      letter-spacing: 2px;
      color: #a6e3a1;
    }
    .btnRow {
      display: flex;
      gap: 6px;
    }
    .btn {
      border: 1px solid #45475a;
      background: #313244;
      color: #cdd6f4;
      border-radius: 6px;
      padding: 5px 9px;
      font-size: 11.5px;
      font-weight: 600;
      cursor: pointer;
    }
    .btn:hover {
      background: #45475a;
    }
    .btnPrimary {
      border: none;
      background: #89b4fa;
      color: #11111b;
    }
    .btnPrimary:hover {
      background: #b4befe;
    }
  `;

  const card = document.createElement("div");
  card.className = "card";

  const header = document.createElement("div");
  header.className = "header";

  const titleEl = document.createElement("span");
  titleEl.className = "title";
  titleEl.textContent = params.title;

  const closeBtn = document.createElement("button");
  closeBtn.type = "button";
  closeBtn.className = "closeBtn";
  closeBtn.textContent = "×";
  closeBtn.addEventListener("click", () => host.remove());

  header.appendChild(titleEl);
  header.appendChild(closeBtn);
  card.appendChild(header);

  const msgEl = document.createElement("div");
  msgEl.className = "msg";
  msgEl.textContent = params.message;
  card.appendChild(msgEl);

  const cleanCode = (params.code || "").trim();
  if (cleanCode) {
    if (navigator.clipboard?.writeText) {
      void navigator.clipboard.writeText(cleanCode).catch(() => {});
    }

    const codeRow = document.createElement("div");
    codeRow.className = "codeRow";

    const codeSpan = document.createElement("span");
    codeSpan.className = "codeVal";
    codeSpan.textContent =
      cleanCode.length === 6 ? `${cleanCode.slice(0, 3)} ${cleanCode.slice(3)}` : cleanCode;

    const btnRow = document.createElement("div");
    btnRow.className = "btnRow";

    const copyBtn = document.createElement("button");
    copyBtn.type = "button";
    copyBtn.className = "btn";
    copyBtn.textContent = "Copy";
    copyBtn.addEventListener("click", () => {
      if (navigator.clipboard?.writeText) {
        void navigator.clipboard.writeText(cleanCode).then(
          () => {
            copyBtn.textContent = "Copied";
          },
          () => {},
        );
      }
    });

    const fillBtn = document.createElement("button");
    fillBtn.type = "button";
    fillBtn.className = "btn btnPrimary";
    fillBtn.textContent = "Fill Code";
    fillBtn.addEventListener("click", () => {
      const filled = autofillCredentialsOnPage({ totpCode: cleanCode });
      msgEl.textContent = filled
        ? "Filled 2FA code into verification field on page."
        : "No 2FA input field detected on page; code is ready in Clipboard.";
    });

    btnRow.appendChild(copyBtn);
    btnRow.appendChild(fillBtn);
    codeRow.appendChild(codeSpan);
    codeRow.appendChild(btnRow);
    card.appendChild(codeRow);
  }

  shadow.appendChild(style);
  shadow.appendChild(card);
  (document.body || document.documentElement).appendChild(host);

  const ttlMs = cleanCode ? 14000 : 7500;
  setTimeout(() => {
    if (host.isConnected) {
      host.remove();
    }
  }, ttlMs);
}
