/**
 * @file extension/src/popup/components/PairingView.ts
 * @brief Màn hình nhập mã xác nhận 6 chữ số để liên kết trình duyệt với ứng dụng Desktop.
 */

export interface PairingViewProps {
  suggestedPin: string;
  onConfirm: (pin: string) => void;
  onCancel: () => void;
}

export function createPairingView(props: PairingViewProps): HTMLElement {
  const wrap = document.createElement("div");
  wrap.className = "stateCard";

  const heading = document.createElement("div");
  heading.className = "stateTitle";
  heading.textContent = "Connect to Desktop App";

  const desc = document.createElement("div");
  desc.className = "stateDesc";
  desc.textContent =
    "Enter the 6-digit verification code from Secure Vault Manager on your computer to autofill logins and two-factor codes.";

  const input = document.createElement("input");
  input.type = "text";
  input.maxLength = 6;
  input.className = "searchInput";
  input.placeholder = "000000";
  input.value = props.suggestedPin;
  input.style.cssText =
    "text-align: center; font-size: 18px; letter-spacing: 5px; font-weight: 700; font-family: monospace; max-width: 190px;";

  const actions = document.createElement("div");
  actions.style.cssText = "display: flex; gap: 8px; justify-content: center; width: 100%; margin-top: 4px;";

  const cancelBtn = document.createElement("button");
  cancelBtn.type = "button";
  cancelBtn.className = "iconBtn";
  cancelBtn.style.cssText = "width: auto; padding: 6px 14px;";
  cancelBtn.textContent = "Cancel";
  cancelBtn.addEventListener("click", props.onCancel);

  const confirmBtn = document.createElement("button");
  confirmBtn.type = "button";
  confirmBtn.className = "primaryBtn";
  confirmBtn.textContent = "Connect Extension";
  confirmBtn.addEventListener("click", () => props.onConfirm(input.value.trim()));

  actions.appendChild(cancelBtn);
  actions.appendChild(confirmBtn);

  wrap.appendChild(heading);
  wrap.appendChild(desc);
  wrap.appendChild(input);
  wrap.appendChild(actions);
  return wrap;
}
