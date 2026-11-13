/**
 * @file extension/src/popup/views/settings-view.ts
 * @brief System status and quick settings view in Popup.
 */

import { detectCurrentBrowserName, sendExtensionBridgeMessage } from "../../services/protocol";
import { createCloseIcon, createLockIcon } from "../components/icons";
import { showToast } from "../components/toast";
import { popupStore } from "../state";

export function renderSettingsView(onClose: () => void): HTMLElement {
  const state = popupStore.getState();
  const card = document.createElement("div");
  card.className = "bannerCard";

  const title = document.createElement("div");
  title.className = "bannerTitle";
  title.textContent = "System Status";

  const desc = document.createElement("div");
  desc.className = "bannerDesc";
  desc.innerHTML = `
    Browser: <b>${detectCurrentBrowserName()}</b><br/>
    Desktop App: <b>${state.desktopRunning ? "Connected (Native IPC)" : "Not Connected"}</b><br/>
    Vault Status: <b>${state.unlocked ? "Unlocked" : "Locked"}</b><br/>
    Biometrics: <b>${state.biometricAvailable ? "Available" : "Not Supported"}</b>
  `;

  const actions = document.createElement("div");
  actions.className = "bannerActions";

  const closeBtn = document.createElement("button");
  closeBtn.type = "button";
  closeBtn.className = "secondaryBtn";
  closeBtn.style.display = "inline-flex";
  closeBtn.style.alignItems = "center";
  closeBtn.style.gap = "6px";
  closeBtn.appendChild(createCloseIcon(12));
  closeBtn.appendChild(document.createTextNode("Close"));
  closeBtn.addEventListener("click", onClose);

  const lockBtn = document.createElement("button");
  lockBtn.type = "button";
  lockBtn.className = "primaryBtn";
  lockBtn.style.display = "inline-flex";
  lockBtn.style.alignItems = "center";
  lockBtn.style.gap = "6px";
  lockBtn.appendChild(createLockIcon(13));
  lockBtn.appendChild(document.createTextNode("Lock Vault Now"));
  lockBtn.disabled = !state.unlocked;
  lockBtn.addEventListener("click", async () => {
    await sendExtensionBridgeMessage("lock_vault");
    popupStore.setState({ unlocked: false, showGenerator: false, searchQuery: "" });
    showToast("Vault has been locked");
    onClose();
  });

  actions.appendChild(closeBtn);
  actions.appendChild(lockBtn);
  card.appendChild(title);
  card.appendChild(desc);
  card.appendChild(actions);

  return card;
}
