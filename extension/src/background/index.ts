/**
 * @file extension/src/background/index.ts
 * @brief Điểm nhập Background Service Worker (Manifest V3) điều phối Autofill, TOTP và Passkey.
 */

import {
  BridgeRequest,
  createBridgeRequest,
  detectCurrentBrowserName,
  getChromeApi,
} from "../services/protocol";
import { PasskeyRouter } from "./passkey_router";
import { clearCropSession, handleQrCropSelected, handleStartQrCrop } from "./qr_crop_handler";
import { BridgeTransport } from "./transport";

const transport = new BridgeTransport();
const passkeyRouter = new PasskeyRouter(transport);

const chromeApi = getChromeApi();

// Cấp phép cho Content Script truy cập chrome.storage.session trong Chromium MV3
if (chromeApi?.storage?.session?.setAccessLevel) {
  try {
    chromeApi.storage.session.setAccessLevel({ accessLevel: "TRUSTED_AND_UNTRUSTED_CONTEXTS" });
  } catch {
    // Bỏ qua nếu phiên bản Chromium cũ hơn 102
  }
}

// Lưu trữ các phiên đăng nhập đang chờ xác nhận lưu thông tin
interface PendingLoginEntry {
  domain: string;
  origin: string;
  username: string;
  password: string;
  timestamp: number;
  submittedUrl: string;
  tabId?: number;
}
const pendingLogins = new Map<number, PendingLoginEntry>();
let latestGlobalPendingLogin: PendingLoginEntry | null = null;

// Theo dõi điều hướng Tab để nhắc lưu mật khẩu khi tải trang mới
if (chromeApi?.tabs?.onUpdated) {
  chromeApi.tabs.onUpdated.addListener(
    (tabId: number, changeInfo: { status?: string; url?: string }) => {
      if (changeInfo.status === "complete") {
        const entry = pendingLogins.get(tabId) || latestGlobalPendingLogin;
        if (entry && Date.now() - entry.timestamp < 60000) {
          chromeApi.tabs.sendMessage(tabId, { type: "SVM_CHECK_PENDING_LOGIN" }, () => {
            if (chromeApi.runtime.lastError) {
              // Tab chưa sẵn sàng nhận message
            }
          });
        }
      }
    },
  );
}

if (chromeApi?.runtime?.onMessage) {
  chromeApi.runtime.onMessage.addListener(
    (message: BridgeRequest<any>, _sender: unknown, sendResponse: (response: unknown) => void) => {
      if (!message || typeof message.action !== "string") {
        return false;
      }

      void (async () => {
        const senderTabId = (_sender as { tab?: { id?: number } })?.tab?.id;

        if (message.action === "record_pending_login") {
          const entry: PendingLoginEntry = {
            domain: String(message.payload?.domain || ""),
            origin: String(message.payload?.origin || ""),
            username: String(message.payload?.username || ""),
            password: String(message.payload?.password || ""),
            timestamp: Number(message.payload?.timestamp || Date.now()),
            submittedUrl: String(message.payload?.submittedUrl || ""),
            tabId: senderTabId,
          };
          if (senderTabId) {
            pendingLogins.set(senderTabId, entry);
          }
          latestGlobalPendingLogin = entry;
          sendResponse({ ok: true, type: "pending_login_recorded" });
          return;
        }

        if (message.action === "get_pending_login") {
          const entry =
            (senderTabId ? pendingLogins.get(senderTabId) : null) || latestGlobalPendingLogin;
          if (entry && Date.now() - entry.timestamp < 60000) {
            sendResponse({ ok: true, type: "pending_login_data", payload: entry });
          } else {
            sendResponse({ ok: false, type: "pending_login_data", payload: null });
          }
          return;
        }

        if (message.action === "clear_pending_login") {
          if (senderTabId) {
            pendingLogins.delete(senderTabId);
          }
          latestGlobalPendingLogin = null;
          sendResponse({ ok: true, type: "pending_login_cleared" });
          return;
        }
        if (message.action === "unlock_vault") {
          const password = String(message.payload?.password || "");
          const req = createBridgeRequest("unlock_vault", {
            password,
            browser: detectCurrentBrowserName(),
            version: "0.1.0",
          });
          const resp = await transport.dispatch<any>(req);
          const unlocked = Boolean(resp.ok && resp.payload?.unlocked);
          sendResponse({
            requestId: message.requestId,
            type: "vault_unlocked",
            ok: unlocked,
            payload: { unlocked },
            error: unlocked
              ? undefined
              : resp.error || {
                  code: "DESKTOP_NOT_RUNNING",
                  message: "Ứng dụng Desktop chưa khởi động hoặc mật khẩu không chính xác.",
                },
          });
          return;
        }

        if (message.action === "unlock_vault_biometric") {
          const req = createBridgeRequest("unlock_vault_biometric", {
            browser: detectCurrentBrowserName(),
            version: "0.1.0",
          });
          const resp = await transport.dispatch<any>(req);
          const unlocked = Boolean(resp.ok && resp.payload?.unlocked);
          sendResponse({
            requestId: message.requestId,
            type: "vault_unlocked",
            ok: unlocked,
            payload: { unlocked },
            error: unlocked
              ? undefined
              : resp.error || {
                  code: "BIOMETRIC_FAILED",
                  message: "Xác thực sinh trắc học không thành công hoặc Desktop chưa khởi động.",
                },
          });
          return;
        }

        if (message.action === "lock_vault") {
          const req = createBridgeRequest("lock_vault");
          const resp = await transport.dispatch<any>(req);
          const ok = Boolean(resp.ok);
          sendResponse({
            requestId: message.requestId,
            type: "vault_locked",
            ok,
            payload: { unlocked: !ok },
          });
          return;
        }

        if (message.action === "passkey_query") {
          const resp = await passkeyRouter.handleQuery(message.payload);
          sendResponse(resp);
          return;
        }

        if (message.action === "passkey_create") {
          const resp = await passkeyRouter.handleCreate(message.payload);
          sendResponse(resp);
          return;
        }

        if (message.action === "passkey_get") {
          const resp = await passkeyRouter.handleGet(message.payload);
          sendResponse(resp);
          return;
        }

        if (message.action === "start_qr_crop") {
          const resp = await handleStartQrCrop(message);
          sendResponse(resp);
          return;
        }

        if (message.action === "qr_crop_selected") {
          const senderTab = (
            _sender as {
              tab?: { id?: number; windowId?: number; url?: string };
            }
          )?.tab;
          const resp = await handleQrCropSelected(senderTab, message, transport);
          sendResponse(resp);
          return;
        }

        if (message.action === "qr_crop_cancelled") {
          const senderTab = (_sender as { tab?: { id?: number } })?.tab;
          await clearCropSession(senderTab?.id);
          sendResponse({
            ok: true,
            type: "qr_crop_cancelled",
            payload: { cancelled: true },
          });
          return;
        }

        const resp = await transport.dispatch(message);
        sendResponse(resp);
      })();

      return true;
    },
  );
}
