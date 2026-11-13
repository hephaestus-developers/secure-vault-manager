/**
 * @file extension/src/popup/components/toast.ts
 * @brief Thông báo nhanh (Toast notification banner) và tiện ích sao chép vào Clipboard trong Popup.
 */

let activeToastMessage = "";
let toastClearTimer: ReturnType<typeof setTimeout> | null = null;

export function updateToastDom(): void {
  const shell = document.getElementById("svm-popup-shell");
  if (!shell) return;

  let toast = document.getElementById("svm-popup-toast");
  if (!activeToastMessage) {
    toast?.remove();
    return;
  }

  if (!toast) {
    toast = document.createElement("div");
    toast.id = "svm-popup-toast";
    toast.className = "toastBanner";
    shell.appendChild(toast);
  }
  toast.textContent = activeToastMessage;
}

export function showToast(msg: string): void {
  activeToastMessage = msg;
  updateToastDom();

  if (toastClearTimer) {
    clearTimeout(toastClearTimer);
  }
  toastClearTimer = setTimeout(() => {
    if (activeToastMessage === msg) {
      activeToastMessage = "";
      updateToastDom();
    }
  }, 2200);
}

export async function copyText(text: string, label: string): Promise<void> {
  if (!text) return;
  try {
    await navigator.clipboard.writeText(text);
    showToast(`Copied ${label.toLowerCase()}`);
  } catch {
    showToast(`Failed to copy ${label.toLowerCase()}`);
  }
}
