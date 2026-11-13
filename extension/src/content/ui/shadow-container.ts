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
 *
 * @param hostId Định danh duy nhất cho host element trong DOM (tránh hiển thị trùng lặp)
 * @param customCss CSS bổ sung dành riêng cho modal/overlay
 * @returns Đối tượng chứa host, shadow root và hàm dọn dẹp cleanup()
 */
export function createShadowContainer(hostId: string, customCss = ""): ShadowContainerResult {
  // Gỡ bỏ instance cũ nếu đã tồn tại trước đó
  const existing = document.getElementById(hostId);
  if (existing) {
    existing.remove();
  }

  const host = document.createElement("div");
  host.id = hostId;
  const shadow = host.attachShadow({ mode: "closed" });

  const styleEl = document.createElement("style");
  styleEl.textContent = `${COMMON_SHADOW_RESET_CSS}\n${COMMON_THEME_CSS}\n${customCss}`;
  shadow.appendChild(styleEl);

  const cleanup = () => {
    try {
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
