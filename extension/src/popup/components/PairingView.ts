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
  wrap.style.cssText =
    "flex: 1; margin: 0; padding: 32px 20px; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 16px;";

  const heading = document.createElement("div");
  heading.className = "stateTitle";
  heading.style.cssText = "font-size: 16px; font-weight: 700; color: var(--text-primary);";
  heading.textContent = "Connect to Desktop App";

  const desc = document.createElement("div");
  desc.className = "stateDesc";
  desc.style.cssText = "max-width: 270px; line-height: 1.55; font-size: 12.5px; color: var(--text-secondary);";
  desc.textContent =
    "Enter the 6-digit verification code from Secure Vault Manager on your computer to autofill logins and two-factor codes.";

  const input = document.createElement("input");
  input.type = "text";
  input.maxLength = 6;
  input.className = "inputField";
  input.placeholder = "000000";
  input.value = props.suggestedPin;
  input.style.cssText =
    "text-align: center; font-size: 20px; letter-spacing: 6px; font-weight: 700; font-family: monospace; max-width: 200px; height: 42px; border-radius: 8px; background-color: var(--bg-mantle); color: var(--text-primary); border: 1px solid var(--border-subtle); outline: none;";

  input.addEventListener("focus", () => {
    input.style.borderColor = "var(--accent-primary)";
    input.style.boxShadow = "0 0 0 2px rgba(137, 180, 250, 0.25)";
  });
  input.addEventListener("blur", () => {
    input.style.borderColor = "var(--border-subtle)";
    input.style.boxShadow = "none";
  });

  const actions = document.createElement("div");
  actions.style.cssText = "display: flex; gap: 10px; justify-content: center; width: 100%; margin-top: 6px;";

  const cancelBtn = document.createElement("button");
  cancelBtn.type = "button";
  cancelBtn.className = "iconBtn";
  cancelBtn.style.cssText = "width: auto; padding: 8px 16px; height: 38px; border-radius: 8px; font-weight: 600;";
  cancelBtn.textContent = "Cancel";
  cancelBtn.addEventListener("click", props.onCancel);

  const confirmBtn = document.createElement("button");
  confirmBtn.type = "button";
  confirmBtn.className = "primaryBtn";
  confirmBtn.style.cssText = "padding: 8px 18px; height: 38px; border-radius: 8px; font-weight: 700;";
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
