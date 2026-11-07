/**
 * @file extension/src/popup/components/CredentialList.ts
 * @brief Danh sách các mục đăng nhập khớp với trang hiện tại; hỗ trợ tự động điền và sao chép nhanh.
 */

import { CredentialSummary } from "../../services/protocol";

export interface CredentialListProps {
  items: CredentialSummary[];
  activeOrigin?: string;
  onFill: (item: CredentialSummary) => void;
  onCopyUsername: (item: CredentialSummary) => void;
  onCopyPassword: (item: CredentialSummary) => void;
  onCopyTotp: (item: CredentialSummary) => void;
}

export function createCredentialList(props: CredentialListProps): HTMLElement {
  const container = document.createElement("div");
  container.className = "credList";

  if (props.items.length === 0) {
    const empty = document.createElement("div");
    empty.className = "stateCard";
    const emptyTitle = document.createElement("div");
    emptyTitle.className = "stateTitle";
    emptyTitle.textContent = "No Matching Logins";
    const emptyDesc = document.createElement("div");
    emptyDesc.className = "stateDesc";
    emptyDesc.textContent =
      "No saved credentials matched this website or search query.";
    empty.appendChild(emptyTitle);
    empty.appendChild(emptyDesc);
    container.appendChild(empty);
    return container;
  }

  let activeHost = "";
  if (props.activeOrigin) {
    try {
      activeHost = new URL(props.activeOrigin).hostname.replace(/^www\./, "").toLowerCase();
    } catch {
      activeHost = "";
    }
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

    const isDomainMatch =
      activeHost.length > 0 &&
      Boolean(item.url && item.url.toLowerCase().includes(activeHost));
    if (isDomainMatch) {
      const matchBadge = document.createElement("span");
      matchBadge.className = "matchBadge";
      matchBadge.textContent = "Match";
      titleRow.appendChild(matchBadge);
    }

    const subtitle = document.createElement("div");
    subtitle.className = "credSubtitle";
    subtitle.textContent = item.username || item.url || "Saved account";
    subtitle.title = item.username;

    info.appendChild(titleRow);
    info.appendChild(subtitle);

    topRow.appendChild(avatar);
    topRow.appendChild(info);

    const actions = document.createElement("div");
    actions.className = "credActions";

    const fillBtn = document.createElement("button");
    fillBtn.type = "button";
    fillBtn.className = "primaryBtn";
    fillBtn.textContent = "Autofill";
    fillBtn.title = "Autofill login on current page";
    fillBtn.addEventListener("click", () => props.onFill(item));
    actions.appendChild(fillBtn);

    const copyUserBtn = document.createElement("button");
    copyUserBtn.type = "button";
    copyUserBtn.className = "iconBtn";
    copyUserBtn.title = "Copy username";
    copyUserBtn.appendChild(
      createSvgIcon("M20 21v-2a4 4 0 00-4-4H8a4 4 0 00-4 4v2M12 11a4 4 0 100-8 4 4 0 000 8z")
    );
    copyUserBtn.appendChild( createBtnLabel("User"));
    copyUserBtn.addEventListener("click", () => props.onCopyUsername(item));
    actions.appendChild(copyUserBtn);

    const copyPassBtn = document.createElement("button");
    copyPassBtn.type = "button";
    copyPassBtn.className = "iconBtn";
    copyPassBtn.title = "Copy password";
    copyPassBtn.appendChild(
      createSvgIcon("M9 9h10v10H9zM5 15H4a1 1 0 01-1-1V4a1 1 0 011-1h10a1 1 0 011 1v1")
    );
    copyPassBtn.appendChild(createBtnLabel("Pass"));
    copyPassBtn.addEventListener("click", () => props.onCopyPassword(item));
    actions.appendChild(copyPassBtn);

    if (item.hasTotp && item.totpCode) {
      const copyTotpBtn = document.createElement("button");
      copyTotpBtn.type = "button";
      copyTotpBtn.className = "iconBtn";
      copyTotpBtn.style.color = "var(--status-success)";
      copyTotpBtn.title = `Copy 2FA verification code (${item.totpCode})`;
      copyTotpBtn.appendChild(
        createSvgIcon("M12 8v4l3 3m6-3a9 9 0 11-18 0 9 9 0 0118 0z")
      );
      copyTotpBtn.appendChild(createBtnLabel(item.totpCode));
      copyTotpBtn.addEventListener("click", () => props.onCopyTotp(item));
      actions.appendChild(copyTotpBtn);
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

function createSvgIcon(pathData: string): SVGSVGElement {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("width", "14");
  svg.setAttribute("height", "14");
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", "2");
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  path.setAttribute("d", pathData);
  svg.appendChild(path);
  return svg;
}
