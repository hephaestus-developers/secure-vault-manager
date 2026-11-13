/**
 * @file extension/src/popup/index.ts
 * @brief Điểm nhập điều khiển giao diện Popup của Browser Extension (MV3).
 */

import {
  detectCurrentBrowserName,
  sendExtensionBridgeMessage,
  VaultStatusPayload,
} from "../services/protocol";
import { createHeaderBar, updateHeaderGeneratorActive } from "./components/HeaderBar";
import { createLockView } from "./components/LockView";
import { showToast, updateToastDom } from "./components/toast";
import {
  fetchCredentialsForState,
  getActiveTabInfo,
  getSystemTheme,
  loadInitialData,
  popupStore,
} from "./state";
import {
  launchQrCropForTab,
  renderCredentialList,
  renderOverwriteBanner,
  renderPendingTotpBanner,
  renderSearchBar,
  updateCredentialListMount,
} from "./views/credential-view";
import { buildGeneratorCardElement, generateNewPassword } from "./views/generator-view";

let lastTotpEpochStep = Math.floor(Date.now() / 1000 / 30);

export function renderPopup(): void {
  const root = document.getElementById("svm-popup-root");
  if (!root) return;

  const state = popupStore.getState();
  const theme = getSystemTheme();
  document.documentElement.setAttribute("data-theme", theme);
  root.replaceChildren();

  const shell = document.createElement("div");
  shell.id = "svm-popup-shell";
  shell.className = "popupShell";

  shell.appendChild(
    createHeaderBar({
      unlocked: state.unlocked,
      configured: state.configured,
      desktopRunning: state.desktopRunning,
      showGenerator: state.showGenerator,
      onToggleGenerator: () => {
        const nextShow = !state.showGenerator;
        popupStore.setState({ showGenerator: nextShow });
        renderPopup();
        if (nextShow && !state.generatedPassword) {
          void generateNewPassword();
        }
      },
      onScanQr: () => void launchQrCropForTab(),
      onLockVault: async () => {
        await sendExtensionBridgeMessage("lock_vault");
        popupStore.setState({ unlocked: false, showGenerator: false, searchQuery: "" });
        showToast("Vault has been locked");
        renderPopup();
      },
    }),
  );

  const content = document.createElement("main");
  content.className = "contentArea";

  // When Vault is not configured or is locked: Display LockView
  if (!state.configured || !state.unlocked) {
    content.appendChild(
      createLockView({
        desktopRunning: state.desktopRunning,
        configured: state.configured,
        biometricAvailable: state.biometricAvailable,
        onUnlockPassword: async (password: string) => {
          const resp = await sendExtensionBridgeMessage<{ unlocked?: boolean }>("unlock_vault", {
            password,
            browser: detectCurrentBrowserName(),
            version: "0.1.0",
          });
          if (resp.ok && resp.payload?.unlocked) {
            popupStore.setState({ unlocked: true });
            showToast("Vault unlocked successfully");
            await loadInitialData();
            renderPopup();
            return true;
          }
          return false;
        },
        onUnlockBiometric: async () => {
          const resp = await sendExtensionBridgeMessage<{ unlocked?: boolean }>(
            "unlock_vault_biometric",
            {
              browser: detectCurrentBrowserName(),
              version: "0.1.0",
            },
          );
          if (resp.ok && resp.payload?.unlocked) {
            popupStore.setState({ unlocked: true });
            showToast("Unlocked with biometrics");
            await loadInitialData();
            renderPopup();
            return true;
          }
          return false;
        },
        onRefresh: () => {
          void loadInitialData().then(() => renderPopup());
        },
      }),
    );
    shell.appendChild(content);
    root.appendChild(shell);
    updateToastDom();
    return;
  }

  // Khi Vault ĐÃ MỞ KHÓA
  const genMount = document.createElement("div");
  genMount.id = "svm-generator-mount";
  if (state.showGenerator) {
    genMount.appendChild(
      buildGeneratorCardElement(() => {
        popupStore.setState({ showGenerator: false });
        updateHeaderGeneratorActive(false);
        renderPopup();
      }),
    );
  }
  content.appendChild(genMount);

  content.appendChild(renderSearchBar(() => updateCredentialListMount(renderPopup)));

  const overwriteBanner = renderOverwriteBanner(renderPopup);
  if (overwriteBanner) {
    content.appendChild(overwriteBanner);
  }

  const pendingBanner = renderPendingTotpBanner(renderPopup);
  if (pendingBanner) {
    content.appendChild(pendingBanner);
  }

  const credMount = document.createElement("div");
  credMount.id = "svm-cred-list-mount";
  credMount.appendChild(renderCredentialList(renderPopup));
  content.appendChild(credMount);

  shell.appendChild(content);
  root.appendChild(shell);
  updateToastDom();
}

async function syncStatusSilently(): Promise<void> {
  try {
    const { origin, domain } = await getActiveTabInfo();
    const state = popupStore.getState();
    const tabChanged = origin !== state.activeOrigin || domain !== state.activeDomain;

    const statusResp = await sendExtensionBridgeMessage<VaultStatusPayload>("get_vault_status");
    if (statusResp.ok && statusResp.payload) {
      const newConfigured = Boolean(statusResp.payload.configured);
      const newUnlocked = newConfigured && Boolean(statusResp.payload.unlocked);
      const newDesktop = Boolean(statusResp.payload.desktopRunning ?? false);
      const newBio = newConfigured && Boolean(statusResp.payload.biometricAvailable);
      if (
        newConfigured !== state.configured ||
        newUnlocked !== state.unlocked ||
        newDesktop !== state.desktopRunning ||
        newBio !== state.biometricAvailable ||
        tabChanged
      ) {
        await loadInitialData();
        renderPopup();
      }
    } else if (state.desktopRunning || state.unlocked || state.configured) {
      popupStore.setState({
        desktopRunning: false,
        configured: false,
        unlocked: false,
      });
      renderPopup();
    }
  } catch {
    // Bỏ qua lỗi kết nối ngầm
  }
}

function initPopup(): void {
  const theme = getSystemTheme();
  popupStore.setState({ theme }, true);
  document.documentElement.setAttribute("data-theme", theme);

  if (window.matchMedia) {
    window.matchMedia("(prefers-color-scheme: dark)").addEventListener("change", (e) => {
      const nextTheme = e.matches ? "dark" : "light";
      popupStore.setState({ theme: nextTheme }, true);
      document.documentElement.setAttribute("data-theme", nextTheme);
      renderPopup();
    });
  }

  renderPopup();
  void loadInitialData().then(() => {
    renderPopup();
  });

  setInterval(() => {
    void syncStatusSilently();
  }, 1500);

  setInterval(() => {
    const currentStep = Math.floor(Date.now() / 1000 / 30);
    if (currentStep !== lastTotpEpochStep) {
      lastTotpEpochStep = currentStep;
      const currentState = popupStore.getState();
      if (currentState.unlocked && currentState.credentials.some((c) => c.hasTotp)) {
        void fetchCredentialsForState().then(() => updateCredentialListMount(renderPopup));
      }
    }
  }, 1000);
}

if (typeof document !== "undefined") {
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initPopup);
  } else {
    initPopup();
  }
}
