/**
 * @file extension/src/popup/components/CredentialList.ts
 * @brief Danh sách các mục đăng nhập khớp với trang hiện tại; hỗ trợ tự động điền và sao chép nhanh.
 */

import { CredentialSummary } from "../../services/protocol";

export interface CredentialListProps {
  items: CredentialSummary[];
  activeOrigin?: string;
  activeDomain?: string;
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
    emptyTitle.textContent = props.activeDomain
      ? "Không có thông tin đăng nhập"
      : "Chưa mở trang web nào";
    const emptyDesc = document.createElement("div");
    emptyDesc.className = "stateDesc";
    emptyDesc.textContent = props.activeDomain
      ? `Không tìm thấy thông tin đăng nhập nào cho trang web ${props.activeDomain}.`
      : "Hãy mở một trang web trên trình duyệt để xem các tài khoản tương ứng.";
    empty.appendChild(emptyTitle);
    empty.appendChild(emptyDesc);
    container.appendChild(empty);
    return container;
  }

  if (props.activeDomain) {
    const domainHeader = document.createElement("div");
    domainHeader.className = "activeDomainHeader";

    const domainIcon = document.createElement("span");
    domainIcon.className = "activeDomainIcon";
    domainIcon.appendChild(
      createSvgIcon("M12 2a10 10 0 100 20 10 10 0 000-20zm0 0c2.5 2.5 4 6.5 4 10s-1.5 7.5-4 10c-2.5-2.5-4-6.5-4-10s1.5-7.5 4-10zm-8 10h16")
    );

    const domainName = document.createElement("span");
    domainName.className = "activeDomainName";
    domainName.textContent = props.activeDomain;

    const domainCount = document.createElement("span");
    domainCount.className = "activeDomainCount";
    domainCount.textContent = `${props.items.length} tài khoản`;

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

    const itemUrl = item.websiteUrl || (item as any).url || "";
    const subtitle = document.createElement("div");
    subtitle.className = "credSubtitle";
    subtitle.textContent = item.username || itemUrl || "Tài khoản đã lưu";
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
    fillBtn.title = "Tự động điền trên trang web hiện tại";
    fillBtn.addEventListener("click", () => props.onFill(item));
    actions.appendChild(fillBtn);

    const copyUserBtn = document.createElement("button");
    copyUserBtn.type = "button";
    copyUserBtn.className = "iconBtn";
    copyUserBtn.title = "Sao chép tên đăng nhập";
    copyUserBtn.appendChild(
      createSvgIcon("M20 21v-2a4 4 0 00-4-4H8a4 4 0 00-4 4v2M12 11a4 4 0 100-8 4 4 0 000 8z")
    );
    copyUserBtn.appendChild(createBtnLabel("User"));
    copyUserBtn.addEventListener("click", () => props.onCopyUsername(item));
    actions.appendChild(copyUserBtn);

    const copyPassBtn = document.createElement("button");
    copyPassBtn.type = "button";
    copyPassBtn.className = "iconBtn";
    copyPassBtn.title = "Sao chép mật khẩu";
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
      copyTotpBtn.title = `Sao chép mã 2FA (${item.totpCode})`;
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
