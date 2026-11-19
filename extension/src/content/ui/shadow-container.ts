/**
 * @file extension/src/content/ui/shadow-container.ts
 * @brief Bộ khung tạo và quản lý Shadow DOM cô lập cho các UI Modal/Overlay của Extension.
 */

import { COMMON_SHADOW_RESET_CSS, COMMON_THEME_CSS } from "./theme.css";

export interface ShadowContainerResult {
  host: HTMLElement;
  shadow: ShadowRoot;
  cleanup: () => void;
}

/**
 * Tạo một container Shadow DOM đóng (`mode: "closed"`) và tiêm toàn bộ reset style + theme.
 * Khi `useTopLayerDialog = true`, bọc Shadow Host bên trong thẻ `<dialog>` để sử dụng Browser Top Layer (`showModal()`).
 *
 * @param hostId Định danh duy nhất cho host element trong DOM (tránh hiển thị trùng lặp)
 * @param customCss CSS bổ sung dành riêng cho modal/overlay
 * @param useTopLayerDialog Bật chế độ `<dialog>` Top Layer để nổi trên mọi modal của trang web
 * @returns Đối tượng chứa host, shadow root và hàm dọn dẹp cleanup()
 */
export function createShadowContainer(
  hostId: string,
  customCss = "",
  useTopLayerDialog = false,
): ShadowContainerResult {
  // Gỡ bỏ instance cũ nếu đã tồn tại trước đó
  const existing = document.getElementById(hostId);
  if (existing) {
    existing.remove();
  }

  let host: HTMLElement;
  let shadowHost: HTMLElement;

  if (useTopLayerDialog) {
    host = document.createElement("dialog");
    host.id = hostId;

    // Reset ::backdrop mặc định của <dialog> để dùng .svm-backdrop bên trong Shadow DOM
    const dialogStyle = document.createElement("style");
    dialogStyle.textContent = `#${hostId}::backdrop { background: transparent !important; }`;
    host.appendChild(dialogStyle);

    // Thẻ <div> bên trong chịu trách nhiệm gắn Shadow DOM (vì HTMLDialogElement không hỗ trợ attachShadow)
    shadowHost = document.createElement("div");
    shadowHost.style.cssText =
      "display: block !important; width: 100% !important; height: 100% !important; margin: 0 !important; padding: 0 !important; border: none !important; background: transparent !important;";
    host.appendChild(shadowHost);
  } else {
    host = document.createElement("div");
    host.id = hostId;
    shadowHost = host;
  }

  const shadow = shadowHost.attachShadow({ mode: "closed" });

  const styleEl = document.createElement("style");
  styleEl.textContent = `${COMMON_SHADOW_RESET_CSS}\n${COMMON_THEME_CSS}\n${customCss}`;
  shadow.appendChild(styleEl);

  const cleanup = () => {
    try {
      if (
        typeof HTMLDialogElement !== "undefined" &&
        host instanceof HTMLDialogElement &&
        host.open
      ) {
        host.close();
      }
      if (host.parentNode) {
        host.parentNode.removeChild(host);
      } else {
        host.remove();
      }
    } catch {
      // Bỏ qua lỗi DOM nếu node đã bị gỡ trước đó
    }
  };

  return { host, shadow, cleanup };
}
