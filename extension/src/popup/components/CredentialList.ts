/**
 * @file extension/src/popup/components/CredentialList.ts
 * @brief Danh sách các mục đăng nhập khớp với trang hiện tại; hỗ trợ tự động điền và sao chép nhanh.
 */

import { CredentialSummary, PasskeySummaryItem } from "../../services/protocol";
import {
  createCopyIcon,
  createPasskeyIcon,
  createQrCodeIconSvg,
  createSvgPathIcon,
  createTotpClockIcon,
  createUserIcon,
} from "./icons";

export interface CredentialListProps {
  items: CredentialSummary[];
  passkeys?: PasskeySummaryItem[];
  activeOrigin?: string;
  activeDomain?: string;
  hasPendingTotp?: boolean;
  onFill: (item: CredentialSummary) => void;
  onCopyUsername: (item: CredentialSummary) => void;
  onCopyPassword: (item: CredentialSummary) => void;
  onCopyTotp: (item: CredentialSummary) => void;
  onAttachTotpQr?: (item: CredentialSummary) => void;
  onFillPasskeyUsername?: (pk: PasskeySummaryItem) => void;
  onCopyPasskeyUsername?: (pk: PasskeySummaryItem) => void;
  onScanQrEmpty?: () => void;
}

export function createCredentialList(props: CredentialListProps): HTMLElement {
  const container = document.createElement("div");
  container.className = "credList";
  const passkeys = props.passkeys || [];

  if (props.items.length === 0 && passkeys.length === 0) {
    const empty = document.createElement("div");
    empty.className = "stateCard";
    const emptyTitle = document.createElement("div");
    emptyTitle.className = "stateTitle";
    emptyTitle.textContent = props.activeDomain ? "No logins found" : "No website open";
    const emptyDesc = document.createElement("div");
    emptyDesc.className = "stateDesc";
    emptyDesc.textContent = props.activeDomain
      ? `No logins found for ${props.activeDomain}.`
      : "Open a website in your browser to view matching credentials.";
    empty.appendChild(emptyTitle);
    empty.appendChild(emptyDesc);
    if (props.activeDomain && props.onScanQrEmpty) {
      const scanEmptyBtn = document.createElement("button");
      scanEmptyBtn.type = "button";
      scanEmptyBtn.className = "primaryBtn";
      scanEmptyBtn.textContent = "Scan 2FA QR Code";
      scanEmptyBtn.addEventListener("click", props.onScanQrEmpty);
      empty.appendChild(scanEmptyBtn);
    }
    container.appendChild(empty);
    return container;
  }

  if (props.activeDomain) {
    const domainHeader = document.createElement("div");
    domainHeader.className = "activeDomainHeader";

    const domainIcon = document.createElement("span");
    domainIcon.className = "activeDomainIcon";
    domainIcon.appendChild(
      createSvgPathIcon(
        "M12 2a10 10 0 100 20 10 10 0 000-20zm0 0c2.5 2.5 4 6.5 4 10s-1.5 7.5-4 10c-2.5-2.5-4-6.5-4-10s1.5-7.5 4-10zm-8 10h16",
      ),
    );

    const domainName = document.createElement("span");
    domainName.className = "activeDomainName";
    domainName.textContent = props.activeDomain;

    const domainCount = document.createElement("span");
    domainCount.className = "activeDomainCount";
    const countParts: string[] = [];
    if (props.items.length > 0) {
      countParts.push(`${props.items.length} login${props.items.length > 1 ? "s" : ""}`);
    }
    if (passkeys.length > 0) {
      countParts.push(`${passkeys.length} passkey${passkeys.length > 1 ? "s" : ""}`);
    }
    domainCount.textContent = countParts.join(" • ");

    domainHeader.appendChild(domainIcon);
    domainHeader.appendChild(domainName);
    domainHeader.appendChild(domainCount);
    container.appendChild(domainHeader);
  }

  for (const item of props.items) {
    const card = document.createElement("div");
    card.className = "credCard";

    const topRow = document.createElement("div");
    topRow.className = "credTopRow";

    const avatar = document.createElement("div");
    avatar.className = "credAvatar";
    const initial = (item.title.trim()[0] ?? "S").toUpperCase();
    avatar.textContent = initial;

    const info = document.createElement("div");
    info.className = "credInfo";

    const titleRow = document.createElement("div");
    titleRow.className = "credTitleRow";

    const title = document.createElement("div");
    title.className = "credTitle";
    title.textContent = item.title;
    title.title = item.title;
    titleRow.appendChild(title);

    const vName = item.vaultName?.trim();
    if (vName && !vName.toLowerCase().startsWith("vault_")) {
      const vBadge = document.createElement("span");
      vBadge.style.cssText =
        "margin-left: 6px; padding: 1px 6px; border-radius: 4px; background: rgba(137, 180, 250, 0.15); color: #89b4fa; font-size: 10px; font-weight: 600; flex-shrink: 0;";
      vBadge.textContent = vName;
      titleRow.appendChild(vBadge);
    }

    const itemUrl = item.websiteUrl || (item as any).url || "";
    const subtitle = document.createElement("div");
    subtitle.className = "credSubtitle";
    subtitle.textContent = item.username || itemUrl || "Saved login";
    subtitle.title = item.username;

    info.appendChild(titleRow);
    info.appendChild(subtitle);

    topRow.appendChild(avatar);
    topRow.appendChild(info);

    if (props.onAttachTotpQr) {
      const qrBtn = document.createElement("button");
      qrBtn.type = "button";
      qrBtn.className = props.hasPendingTotp ? "iconBtn credTopQrPending" : "iconBtn credTopQrBtn";
      qrBtn.appendChild(createQrCodeIconSvg(13));
      if (props.hasPendingTotp) {
        qrBtn.title = "Attach scanned 2FA QR code to this login";
        qrBtn.appendChild(createBtnLabel("Attach 2FA"));
      } else {
        qrBtn.title = item.hasTotp
          ? "Rescan QR code to replace current 2FA"
          : "Crop QR code on page to add 2FA (TOTP)";
      }
      qrBtn.addEventListener("click", () => props.onAttachTotpQr!(item));
      topRow.appendChild(qrBtn);
    }

    const actions = document.createElement("div");
    actions.className = "credActions";

    const fillBtn = document.createElement("button");
    fillBtn.type = "button";
    fillBtn.className = "primaryBtn credFillBtn";
    fillBtn.textContent = "Autofill";
    fillBtn.title = "Autofill credentials on this page";
    fillBtn.addEventListener("click", () => props.onFill(item));
    actions.appendChild(fillBtn);

    const quickGroup = document.createElement("div");
    quickGroup.className = "credQuickGroup";

    const copyUserBtn = document.createElement("button");
    copyUserBtn.type = "button";
    copyUserBtn.className = "iconBtn credCompactBtn";
    copyUserBtn.title = "Copy username";
    copyUserBtn.appendChild(createUserIcon(13));
    copyUserBtn.appendChild(createBtnLabel("User"));
    copyUserBtn.addEventListener("click", () => props.onCopyUsername(item));
    quickGroup.appendChild(copyUserBtn);

    const copyPassBtn = document.createElement("button");
    copyPassBtn.type = "button";
    copyPassBtn.className = "iconBtn credCompactBtn";
    copyPassBtn.title = "Copy password";
    copyPassBtn.appendChild(createCopyIcon(13));
    copyPassBtn.appendChild(createBtnLabel("Pass"));
    copyPassBtn.addEventListener("click", () => props.onCopyPassword(item));
    quickGroup.appendChild(copyPassBtn);

    if (item.hasTotp && item.totpCode) {
      const rawCode = item.totpCode.trim();
      const formattedCode =
        rawCode.length === 6 ? `${rawCode.slice(0, 3)} ${rawCode.slice(3)}` : rawCode;
      const copyTotpBtn = document.createElement("button");
      copyTotpBtn.type = "button";
      copyTotpBtn.className = "iconBtn credCompactBtn credTotpBtn";
      copyTotpBtn.title = `Copy 2FA code (${formattedCode})`;
      copyTotpBtn.appendChild(createTotpClockIcon(13));
      copyTotpBtn.appendChild(createBtnLabel(formattedCode));
      copyTotpBtn.addEventListener("click", () => props.onCopyTotp(item));
      quickGroup.appendChild(copyTotpBtn);
    }

    actions.appendChild(quickGroup);

    card.appendChild(topRow);
    card.appendChild(actions);
    container.appendChild(card);
  }

  for (const pk of passkeys) {
    const card = document.createElement("div");
    card.className = "credCard";

    const topRow = document.createElement("div");
    topRow.className = "credTopRow";

    const avatar = document.createElement("div");
    avatar.className = "credAvatar";
    avatar.appendChild(createPasskeyIcon(16));

    const info = document.createElement("div");
    info.className = "credInfo";

    const titleRow = document.createElement("div");
    titleRow.className = "credTitleRow";

    const title = document.createElement("div");
    title.className = "credTitle";
    title.textContent = pk.title || props.activeDomain || "Passkey";
    title.title = title.textContent;
    titleRow.appendChild(title);

    const badge = document.createElement("span");
    badge.className = "passkeyBadge";
    badge.textContent = "Passkey";
    titleRow.appendChild(badge);

    const subtitle = document.createElement("div");
    subtitle.className = "credSubtitle";
    subtitle.textContent = pk.username || props.activeDomain || "WebAuthn / FIDO2";
    subtitle.title = subtitle.textContent;

    info.appendChild(titleRow);
    info.appendChild(subtitle);
    topRow.appendChild(avatar);
    topRow.appendChild(info);

    const actions = document.createElement("div");
    actions.className = "credActions";

    const hint = document.createElement("span");
    hint.className = "passkeyHint";
    hint.textContent = "FIDO2 / Passkey";
    actions.appendChild(hint);

    if (pk.username) {
      const quickGroup = document.createElement("div");
      quickGroup.className = "credQuickGroup";

      if (props.onFillPasskeyUsername) {
        const fillUserBtn = document.createElement("button");
        fillUserBtn.type = "button";
        fillUserBtn.className = "primaryBtn credFillBtn";
        fillUserBtn.textContent = "Fill User";
        fillUserBtn.title = "Fill passkey username on this page";
        fillUserBtn.addEventListener("click", () => props.onFillPasskeyUsername!(pk));
        quickGroup.appendChild(fillUserBtn);
      }

      if (props.onCopyPasskeyUsername) {
        const copyUserBtn = document.createElement("button");
        copyUserBtn.type = "button";
        copyUserBtn.className = "iconBtn credCompactBtn";
        copyUserBtn.title = "Copy passkey username";
        copyUserBtn.appendChild(createUserIcon(13));
        copyUserBtn.appendChild(createBtnLabel("User"));
        copyUserBtn.addEventListener("click", () => props.onCopyPasskeyUsername!(pk));
        quickGroup.appendChild(copyUserBtn);
      }

      actions.appendChild(quickGroup);
    }

    card.appendChild(topRow);
    card.appendChild(actions);
    container.appendChild(card);
  }

  return container;
}

function createBtnLabel(text: string): HTMLSpanElement {
  const span = document.createElement("span");
  span.style.marginLeft = "4px";
  span.textContent = text;
  return span;
}
