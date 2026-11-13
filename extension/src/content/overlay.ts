/**
 * @file extension/src/content/overlay.ts
 * @brief Hộp chọn tài khoản nhanh (Inline Autofill Popover) hiển thị trong Shadow DOM khi người dùng chạm vào ô nhập liệu.
 */

import {
  CredentialSecretPayload,
  CredentialSummary,
  CredentialsListPayload,
  sendExtensionBridgeMessage,
} from "../services/protocol";
import { autofillCredentialsOnPage, setNativeInputValue } from "./autofill";
import { classifyDocumentFields } from "./classifier";

let suppressedInput: HTMLInputElement | null = null;

export function setupInlineAutofillOverlay(): void {
  const handleInteraction = async (event: Event) => {
    const target = event.target;
    if (!(target instanceof HTMLInputElement)) return;
    if (event.type === "click" && target === suppressedInput) {
      return;
    }

    const fields = classifyDocumentFields(document);
    const isTotp = target === fields.totpInput;
    let isUserOrPass = target === fields.usernameInput || target === fields.passwordInput;

    // Dự phòng bổ sung: nếu target là ô text/email hiển thị đứng trước passwordInput trong form
    if (!isUserOrPass && !isTotp && fields.passwordInput) {
      const p = target.compareDocumentPosition(fields.passwordInput);
      const isBeforePassword = (p & Node.DOCUMENT_POSITION_FOLLOWING) !== 0;
      const targetType = (target.type || "text").toLowerCase();
      if (
        isBeforePassword &&
        (targetType === "text" || targetType === "email" || targetType === "") &&
        !target.disabled &&
        !target.readOnly
      ) {
        isUserOrPass = true;
      }
    }

    if (!isUserOrPass && !isTotp) {
      return;
    }

    const resp = await sendExtensionBridgeMessage<CredentialsListPayload>("get_credentials", {
      domain: window.location.hostname,
    });
    if (!resp.ok || !resp.payload?.credentials?.length) {
      return;
    }

    if (document.activeElement !== target) {
      return;
    }

    renderInlinePopover(target, resp.payload.credentials, isTotp);
  };

  document.addEventListener("focusin", handleInteraction, true);
  document.addEventListener("click", handleInteraction, true);
  document.addEventListener(
    "focusout",
    (event) => {
      if (event.target === suppressedInput) {
        suppressedInput = null;
      }
    },
    true,
  );
}

function renderInlinePopover(
  anchor: HTMLInputElement,
  credentials: CredentialSummary[],
  isTotpField: boolean = false,
): void {
  const existing = document.getElementById("svm-inline-overlay-host");
  if (existing) {
    existing.remove();
  }

  const rect = anchor.getBoundingClientRect();
  const host = document.createElement("div");
  host.id = "svm-inline-overlay-host";
  const shadow = host.attachShadow({ mode: "closed" });

  const style = document.createElement("style");
  style.textContent = `
    .svm-menu {
      position: fixed;
      top: ${Math.round(rect.bottom + 6)}px;
      left: ${Math.round(rect.left)}px;
      width: ${Math.max(260, Math.round(rect.width))}px;
      z-index: 2147483646;
      background: #181825;
      color: #cdd6f4;
      border: 1px solid #313244;
      border-radius: 10px;
      padding: 6px;
      box-shadow: 0 10px 28px rgba(0, 0, 0, 0.45);
      font-family: -apple-system, BlinkMacSystemFont, 'Inter', 'Segoe UI', sans-serif;
      font-size: 12.5px;
    }
    .svm-header {
      display: flex;
      align-items: center;
      gap: 6px;
      padding: 4px 8px 6px;
      margin-bottom: 4px;
      border-bottom: 1px solid #313244;
      font-size: 11px;
      font-weight: 600;
      color: #89b4fa;
    }
    .svm-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
      width: 100%;
      padding: 7px 10px;
      background: transparent;
      border: none;
      border-radius: 6px;
      color: inherit;
      cursor: pointer;
      text-align: left;
    }
    .svm-row:hover, .svm-row[data-active="true"] {
      background: #313244;
    }
    .svm-info {
      display: flex;
      flex-direction: column;
      min-width: 0;
      flex: 1;
    }
    .svm-title {
      font-weight: 600;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .svm-user {
      color: #a6adc8;
      font-size: 11px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .svm-badge {
      padding: 2px 6px;
      border-radius: 4px;
      background: rgba(166, 227, 161, 0.14);
      color: #a6e3a1;
      font-size: 10px;
      font-weight: 700;
      font-family: monospace;
    }
    .svm-vault-badge {
      display: inline-block;
      margin-left: 6px;
      padding: 1px 5px;
      border-radius: 4px;
      background: rgba(137, 180, 250, 0.15);
      color: #89b4fa;
      font-size: 9.5px;
      font-weight: 600;
      vertical-align: middle;
    }
    @media (prefers-color-scheme: light) {
      .svm-menu {
        background: #ffffff;
        color: #1e1e2e;
        border-color: #ccd0da;
        box-shadow: 0 8px 24px rgba(30, 30, 46, 0.14);
      }
      .svm-header {
        border-bottom-color: #e6e9ef;
        color: #1e66f5;
      }
      .svm-row:hover, .svm-row[data-active="true"] {
        background: #e6e9ef;
      }
      .svm-user {
        color: #5c5f77;
      }
      .svm-badge {
        background: rgba(64, 160, 43, 0.12);
        color: #40a02b;
      }
    }
  `;
  shadow.appendChild(style);

  const menu = document.createElement("div");
  menu.className = "svm-menu";

  const header = document.createElement("div");
  header.className = "svm-header";
  header.textContent = isTotpField ? "Secure Vault 2FA" : "Secure Vault Autofill";
  menu.appendChild(header);

  const visibleItems = credentials.slice(0, 4);
  const rows: HTMLButtonElement[] = [];
  let activeIndex = -1;

  const triggerFill = (cred: CredentialSummary) => {
    cleanup();
    void (async () => {
      // Nếu đang ở ô 2FA và đã có sẵn totpCode trong summary, điền ngay tức thì
      if (isTotpField && cred.totpCode) {
        setNativeInputValue(anchor, cred.totpCode);
      }
      const secretResp = await sendExtensionBridgeMessage<CredentialSecretPayload>(
        "fill_credential",
        { itemId: cred.id },
      );
      const code = secretResp.payload?.totpCode || cred.totpCode;
      if (isTotpField && code) {
        setNativeInputValue(anchor, code);
      } else {
        autofillCredentialsOnPage({
          username: secretResp.payload?.username || cred.username,
          password: secretResp.payload?.password || "",
          totpCode: code,
        });
      }
    })();
  };

  const updateActiveRow = () => {
    rows.forEach((r, idx) => {
      r.setAttribute("data-active", activeIndex >= 0 && idx === activeIndex ? "true" : "false");
    });
  };

  for (let i = 0; i < visibleItems.length; i++) {
    const cred = visibleItems[i];
    const row = document.createElement("button");
    row.type = "button";
    row.className = "svm-row";
    row.setAttribute("data-active", "false");

    const info = document.createElement("div");
    info.className = "svm-info";

    const titleSpan = document.createElement("span");
    titleSpan.className = "svm-title";
    titleSpan.textContent = cred.title;

    const vName = cred.vaultName?.trim();
    if (vName && !vName.toLowerCase().startsWith("vault_")) {
      const vBadge = document.createElement("span");
      vBadge.className = "svm-vault-badge";
      vBadge.textContent = vName;
      titleSpan.appendChild(vBadge);
    }

    const userSpan = document.createElement("span");
    userSpan.className = "svm-user";
    userSpan.textContent = cred.username;

    info.appendChild(titleSpan);
    info.appendChild(userSpan);
    row.appendChild(info);

    if (cred.hasTotp && cred.totpCode) {
      const badge = document.createElement("span");
      badge.className = "svm-badge";
      badge.textContent = isTotpField ? `Điền: ${cred.totpCode}` : "2FA";
      row.appendChild(badge);
    }

    row.addEventListener("mousedown", (e) => {
      e.preventDefault();
      triggerFill(cred);
    });

    rows.push(row);
    menu.appendChild(row);
  }

  shadow.appendChild(menu);
  document.documentElement.appendChild(host);

  const updatePosition = () => {
    if (!anchor.isConnected) {
      cleanup();
      return;
    }
    const r = anchor.getBoundingClientRect();
    const menuWidth = Math.max(260, Math.round(r.width));
    const menuHeight = menu.offsetHeight || 180;
    const clampedLeft = Math.min(
      Math.max(8, Math.round(r.left)),
      Math.max(8, window.innerWidth - menuWidth - 8),
    );
    const shouldFlipUp =
      r.bottom + menuHeight + 8 > window.innerHeight && r.top - menuHeight - 6 >= 8;
    const topPos = shouldFlipUp ? Math.round(r.top - menuHeight - 6) : Math.round(r.bottom + 6);
    menu.style.top = `${topPos}px`;
    menu.style.left = `${clampedLeft}px`;
    menu.style.width = `${menuWidth}px`;
  };

  updatePosition();

  const onInput = () => {
    suppressedInput = anchor;
    cleanup();
  };

  const onKeyDown = (e: KeyboardEvent) => {
    if (e.key === "ArrowDown") {
      e.preventDefault();
      activeIndex = activeIndex < 0 ? 0 : (activeIndex + 1) % visibleItems.length;
      updateActiveRow();
    } else if (e.key === "ArrowUp") {
      e.preventDefault();
      activeIndex = activeIndex <= 0 ? visibleItems.length - 1 : activeIndex - 1;
      updateActiveRow();
    } else if (e.key === "Enter") {
      if (activeIndex >= 0 && visibleItems[activeIndex]) {
        e.preventDefault();
        triggerFill(visibleItems[activeIndex]);
      } else {
        cleanup();
      }
    } else if (e.key === "Escape") {
      suppressedInput = anchor;
      cleanup();
    }
  };

  function cleanup() {
    host.remove();
    anchor.removeEventListener("blur", cleanup);
    anchor.removeEventListener("keydown", onKeyDown);
    anchor.removeEventListener("input", onInput);
    window.removeEventListener("scroll", updatePosition, true);
    window.removeEventListener("resize", updatePosition);
  }
  anchor.addEventListener("blur", cleanup);
  anchor.addEventListener("keydown", onKeyDown);
  anchor.addEventListener("input", onInput);
  window.addEventListener("scroll", updatePosition, true);
  window.addEventListener("resize", updatePosition);
}
