/**
 * @file extension/src/popup/views/generator-view.ts
 * @brief Password generator card UI in Popup.
 */

import { GeneratedPasswordPayload, sendExtensionBridgeMessage } from "../../services/protocol";
import { updateHeaderGeneratorActive } from "../components/HeaderBar";
import {
  createAutofillArrowIcon,
  createCloseIcon,
  createCopyIcon,
  createRefreshIcon,
} from "../components/icons";
import { copyText, showToast } from "../components/toast";
import { popupStore } from "../state";

export async function generateNewPassword(): Promise<void> {
  const state = popupStore.getState();
  const resp = await sendExtensionBridgeMessage<GeneratedPasswordPayload>("generate_password", {
    length: state.genLength,
    uppercase: state.genUpper,
    lowercase: state.genLower,
    digits: state.genDigits,
    symbols: state.genSymbols,
  });

  if (resp.ok && resp.payload?.password) {
    popupStore.setState({
      generatedPassword: resp.payload.password,
      generatedPercent: resp.payload.percent || 0,
      generatedFeedback: resp.payload.feedback || "",
    });
    updateGeneratorCardDom();
  } else {
    showToast(resp.error?.message || "Failed to generate password");
  }
}

let activeGeneratorCloseHandler: (() => void) | null = null;

export function buildGeneratorCardElement(onClose: () => void): HTMLElement {
  activeGeneratorCloseHandler = onClose;
  const state = popupStore.getState();
  const card = document.createElement("div");
  card.className = "genCard";

  const header = document.createElement("div");
  header.className = "genHeader";
  const titleSpan = document.createElement("span");
  titleSpan.textContent = "Password Generator";
  const closeBtn = document.createElement("button");
  closeBtn.type = "button";
  closeBtn.className = "iconBtn";
  closeBtn.title = "Close";
  closeBtn.appendChild(createCloseIcon(12));
  closeBtn.addEventListener("click", () => {
    if (activeGeneratorCloseHandler) {
      activeGeneratorCloseHandler();
    } else {
      onClose();
    }
  });
  header.appendChild(titleSpan);
  header.appendChild(closeBtn);
  card.appendChild(header);

  const pwdBox = document.createElement("div");
  pwdBox.className = "genPwdBox";
  pwdBox.textContent = state.generatedPassword || "Generating password...";
  card.appendChild(pwdBox);

  const metaRow = document.createElement("div");
  metaRow.className = "genMetaRow";
  const bar = document.createElement("div");
  bar.className = "genStrengthBar";
  const fill = document.createElement("div");
  fill.className = "genStrengthFill";
  fill.style.width = `${Math.min(100, Math.max(0, state.generatedPercent))}%`;
  bar.appendChild(fill);
  const feedback = document.createElement("span");
  feedback.textContent = state.generatedFeedback || `${state.genLength} characters`;
  metaRow.appendChild(bar);
  metaRow.appendChild(feedback);
  card.appendChild(metaRow);

  const lenRow = document.createElement("div");
  lenRow.className = "genControlsRow";
  const lenLabel = document.createElement("span");
  lenLabel.textContent = `Length: ${state.genLength}`;
  const lenSlider = document.createElement("input");
  lenSlider.type = "range";
  lenSlider.min = "8";
  lenSlider.max = "64";
  lenSlider.value = String(state.genLength);
  lenSlider.addEventListener("input", () => {
    const val = Number.parseInt(lenSlider.value, 10) || 16;
    popupStore.setState({ genLength: val });
    lenLabel.textContent = `Length: ${val}`;
  });
  lenSlider.addEventListener("change", () => {
    void generateNewPassword();
  });
  lenRow.appendChild(lenLabel);
  lenRow.appendChild(lenSlider);
  card.appendChild(lenRow);

  const checksRow = document.createElement("div");
  checksRow.className = "genCheckboxGrid";
  const opts: Array<{ key: "genUpper" | "genLower" | "genDigits" | "genSymbols"; label: string }> =
    [
      { key: "genUpper", label: "A-Z" },
      { key: "genLower", label: "a-z" },
      { key: "genDigits", label: "0-9" },
      { key: "genSymbols", label: "!@#$" },
    ];
  for (const opt of opts) {
    const lbl = document.createElement("label");
    lbl.className = "genCheckLabel";
    const cb = document.createElement("input");
    cb.type = "checkbox";
    cb.checked = state[opt.key];
    cb.addEventListener("change", () => {
      popupStore.setState({ [opt.key]: cb.checked });
      void generateNewPassword();
    });
    lbl.appendChild(cb);
    lbl.appendChild(document.createTextNode(opt.label));
    checksRow.appendChild(lbl);
  }
  card.appendChild(checksRow);

  const actions = document.createElement("div");
  actions.className = "bannerActions";

  const regenBtn = document.createElement("button");
  regenBtn.type = "button";
  regenBtn.className = "secondaryBtn";
  regenBtn.style.display = "inline-flex";
  regenBtn.style.alignItems = "center";
  regenBtn.style.gap = "6px";
  regenBtn.appendChild(createRefreshIcon(13));
  regenBtn.appendChild(document.createTextNode("Regenerate"));
  regenBtn.addEventListener("click", () => void generateNewPassword());

  const copyBtn = document.createElement("button");
  copyBtn.type = "button";
  copyBtn.className = "secondaryBtn";
  copyBtn.style.display = "inline-flex";
  copyBtn.style.alignItems = "center";
  copyBtn.style.gap = "6px";
  copyBtn.appendChild(createCopyIcon(13));
  copyBtn.appendChild(document.createTextNode("Copy"));
  copyBtn.addEventListener("click", () => {
    void copyText(popupStore.getState().generatedPassword, "Generated password");
  });

  const fillBtn = document.createElement("button");
  fillBtn.type = "button";
  fillBtn.className = "primaryBtn";
  fillBtn.style.display = "inline-flex";
  fillBtn.style.alignItems = "center";
  fillBtn.style.gap = "6px";
  fillBtn.appendChild(createAutofillArrowIcon(13));
  fillBtn.appendChild(document.createTextNode("Fill into Page"));
  fillBtn.addEventListener("click", () => {
    const currentPwd = popupStore.getState().generatedPassword;
    if (!currentPwd) return;
    const chromeApi = (globalThis as Record<string, any>).chrome;
    if (chromeApi?.tabs?.query && chromeApi?.tabs?.sendMessage) {
      chromeApi.tabs.query(
        { active: true, currentWindow: true },
        (tabs: Array<{ id?: number }>) => {
          const tabId = tabs?.[0]?.id;
          if (tabId !== undefined) {
            chromeApi.tabs.sendMessage(tabId, {
              type: "SVM_FILL_ACTIVE_TAB",
              password: currentPwd,
            });
          }
        },
      );
    }
    showToast("Filled new password into page");
  });

  actions.appendChild(regenBtn);
  actions.appendChild(copyBtn);
  actions.appendChild(fillBtn);
  card.appendChild(actions);

  return card;
}

export function updateGeneratorCardDom(onClose?: () => void): void {
  if (onClose) {
    activeGeneratorCloseHandler = onClose;
  }
  const mount = document.getElementById("svm-generator-mount");
  if (!mount) return;
  const state = popupStore.getState();
  if (!state.showGenerator) {
    mount.replaceChildren();
    updateHeaderGeneratorActive(false);
    return;
  }
  mount.replaceChildren(
    buildGeneratorCardElement(
      activeGeneratorCloseHandler ||
        (() => {
          popupStore.setState({ showGenerator: false });
          updateHeaderGeneratorActive(false);
          mount.replaceChildren();
        }),
    ),
  );
}
