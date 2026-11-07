/**
 * @file extension/src/popup/components/GeneratorTab.ts
 * @brief Thẻ tạo mật khẩu mạnh ngẫu nhiên trong giao diện Extension Popup.
 */

import { PasswordGeneratorOptions } from "../../services/domain";

export interface GeneratorTabProps {
  password: string;
  options: PasswordGeneratorOptions;
  onRegenerate: (options: PasswordGeneratorOptions) => void;
  onCopy: (password: string) => void;
}

export function createGeneratorTab(props: GeneratorTabProps): HTMLElement {
  const wrap = document.createElement("div");
  wrap.className = "generatorBox";

  const preview = document.createElement("div");
  preview.className = "passwordPreview";

  const pwText = document.createElement("span");
  pwText.textContent = props.password;
  pwText.style.cssText = "flex: 1; min-width: 0; overflow: hidden; text-overflow: ellipsis;";

  const copyBtn = document.createElement("button");
  copyBtn.type = "button";
  copyBtn.className = "iconBtn";
  copyBtn.title = "Copy generated password";
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("width", "14");
  svg.setAttribute("height", "14");
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", "2");
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  path.setAttribute("d", "M9 9h10v10H9zM5 15H4a1 1 0 01-1-1V4a1 1 0 011-1h10a1 1 0 011 1v1");
  svg.appendChild(path);
  copyBtn.appendChild(svg);
  const copyTxt = document.createElement("span");
  copyTxt.style.marginLeft = "4px";
  copyTxt.textContent = "Copy";
  copyBtn.appendChild(copyTxt);
  copyBtn.addEventListener("click", () => props.onCopy(props.password));

  preview.appendChild(pwText);
  preview.appendChild(copyBtn);
  wrap.appendChild(preview);

  // 4-segment Password Strength Meter (matching Desktop PasswordEntropyMeter)
  const strengthWrap = document.createElement("div");
  strengthWrap.className = "strengthBarWrap";
  const segGrid = document.createElement("div");
  segGrid.className = "strengthSegments";
  const { level, color, label } = evaluateStrength(props.password);
  for (let i = 0; i < 4; i++) {
    const seg = document.createElement("div");
    seg.className = "strengthSegment";
    if (i < level) {
      seg.style.backgroundColor = color;
    }
    segGrid.appendChild(seg);
  }
  const strengthLabel = document.createElement("div");
  strengthLabel.className = "strengthLabel";
  strengthLabel.style.color = color;
  strengthLabel.textContent = label;
  strengthWrap.appendChild(segGrid);
  strengthWrap.appendChild(strengthLabel);
  wrap.appendChild(strengthWrap);

  const optionCard = document.createElement("div");
  optionCard.className = "optionCard";

  const lengthRow = document.createElement("div");
  lengthRow.className = "optionRow";
  const lengthLabel = document.createElement("span");
  lengthLabel.textContent = `Password Length: ${props.options.length}`;
  const lengthSlider = document.createElement("input");
  lengthSlider.type = "range";
  lengthSlider.min = "8";
  lengthSlider.max = "64";
  lengthSlider.value = String(props.options.length);
  lengthSlider.addEventListener("input", () => {
    props.options.length = Number(lengthSlider.value);
    lengthLabel.textContent = `Password Length: ${props.options.length}`;
    props.onRegenerate({ ...props.options });
  });
  lengthRow.appendChild(lengthLabel);
  lengthRow.appendChild(lengthSlider);
  optionCard.appendChild(lengthRow);

  const toggles: Array<{ key: keyof Omit<PasswordGeneratorOptions, "length">; label: string }> = [
    { key: "uppercase", label: "Uppercase Letters (A-Z)" },
    { key: "lowercase", label: "Lowercase Letters (a-z)" },
    { key: "digits", label: "Numbers (0-9)" },
    { key: "symbols", label: "Special Characters (!@#$)" },
  ];

  for (const t of toggles) {
    const row = document.createElement("label");
    row.className = "optionRow";
    const span = document.createElement("span");
    span.textContent = t.label;
    const checkbox = document.createElement("input");
    checkbox.type = "checkbox";
    checkbox.checked = Boolean(props.options[t.key]);
    checkbox.addEventListener("change", () => {
      props.options[t.key] = checkbox.checked;
      props.onRegenerate({ ...props.options });
    });
    row.appendChild(span);
    row.appendChild(checkbox);
    optionCard.appendChild(row);
  }

  wrap.appendChild(optionCard);

  const regenBtn = document.createElement("button");
  regenBtn.type = "button";
  regenBtn.className = "primaryBtn";
  regenBtn.textContent = "Generate New Password";
  regenBtn.addEventListener("click", () => props.onRegenerate({ ...props.options }));
  wrap.appendChild(regenBtn);

  return wrap;
}

function evaluateStrength(pw: string): { level: number; color: string; label: string } {
  let classes = 0;
  if (/[a-z]/.test(pw)) classes++;
  if (/[A-Z]/.test(pw)) classes++;
  if (/[0-9]/.test(pw)) classes++;
  if (/[^a-zA-Z0-9]/.test(pw)) classes++;

  if (pw.length >= 16 && classes >= 3) {
    return { level: 4, color: "var(--status-success)", label: "Excellent — Highly secure password" };
  }
  if (pw.length >= 12 && classes >= 2) {
    return { level: 3, color: "var(--accent-primary)", label: "Strong — Safe for all accounts" };
  }
  if (pw.length >= 8) {
    return { level: 2, color: "var(--status-warning)", label: "Fair — Increase length for better security" };
  }
  return { level: 1, color: "var(--status-danger)", label: "Weak — Too short" };
}
