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

// Quản lý dữ liệu đăng nhập đang chờ lưu: lưu thuần trong bộ nhớ RAM của Background Worker theo tabId
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

// Danh sách các itemId đã được truy vấn và cấp phép cho từng tab cụ thể
// Ngăn chặn việc một trang web gọi fill_credential với itemId của trang web khác
const authorizedTabItems = new Map<number, Set<string>>();

// Theo dõi điều hướng Tab để nhắc lưu mật khẩu khi tải trang mới và xóa cấp phép cũ
if (chromeApi?.tabs?.onUpdated) {
  chromeApi.tabs.onUpdated.addListener(
    (tabId: number, changeInfo: { status?: string; url?: string }) => {
      if (changeInfo.url) {
        // Tab điều hướng sang URL mới -> xóa danh sách item cũ đã cấp phép
        authorizedTabItems.delete(tabId);
      }
      if (changeInfo.status === "complete") {
        const entry = pendingLogins.get(tabId);
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

// Xóa pending login và cấp phép khi đóng tab
if (chromeApi?.tabs?.onRemoved) {
  chromeApi.tabs.onRemoved.addListener((tabId: number) => {
    pendingLogins.delete(tabId);
    authorizedTabItems.delete(tabId);
  });
}

if (chromeApi?.runtime?.onMessage) {
  chromeApi.runtime.onMessage.addListener(
    (message: BridgeRequest<any>, _sender: unknown, sendResponse: (response: unknown) => void) => {
      if (!message || typeof message.action !== "string") {
        return false;
      }

      void (async () => {
        const senderTab = (_sender as { tab?: { id?: number; url?: string }; url?: string })?.tab;
        const senderTabId = senderTab?.id;
        const senderTabUrl = senderTab?.url || (_sender as { url?: string })?.url || "";
        const isContentScript = Boolean(senderTabId);

        let senderHostname = "";
        let senderOrigin = "";
        if (senderTabUrl) {
          try {
            const parsed = new URL(senderTabUrl);
            senderHostname = parsed.hostname.toLowerCase();
            senderOrigin = parsed.origin;
          } catch {
            // URL không hợp lệ
          }
        }

        if (message.action === "record_pending_login") {
          const entry: PendingLoginEntry = {
            domain:
              isContentScript && senderHostname
                ? senderHostname
                : String(message.payload?.domain || ""),
            origin:
              isContentScript && senderOrigin
                ? senderOrigin
                : String(message.payload?.origin || ""),
            username: String(message.payload?.username || ""),
            password: String(message.payload?.password || ""),
            timestamp: Number(message.payload?.timestamp || Date.now()),
            submittedUrl:
              isContentScript && senderTabUrl
                ? senderTabUrl
                : String(message.payload?.submittedUrl || ""),
            tabId: senderTabId,
          };
          if (senderTabId) {
            pendingLogins.set(senderTabId, entry);
          }
          sendResponse({ ok: true, type: "pending_login_recorded" });
          return;
        }

        if (message.action === "get_pending_login") {
          const entry = senderTabId ? pendingLogins.get(senderTabId) : null;
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

        if (message.action === "get_credentials") {
          let queryDomain = String(message.payload?.domain || "");
          if (isContentScript) {
            queryDomain = senderHostname;
          }
          const req = createBridgeRequest("get_credentials", {
            ...(message.payload || {}),
            domain: queryDomain,
          });
          const resp = await transport.dispatch<{ credentials?: Array<{ id: string }> }>(req);
          if (resp.ok && Array.isArray(resp.payload?.credentials) && senderTabId) {
            let itemSet = authorizedTabItems.get(senderTabId);
            if (!itemSet) {
              itemSet = new Set<string>();
              authorizedTabItems.set(senderTabId, itemSet);
            }
            for (const it of resp.payload.credentials) {
              if (it?.id) itemSet.add(it.id);
            }
          }
          sendResponse(resp);
          return;
        }

        if (message.action === "fill_credential") {
          const reqItemId = String(message.payload?.itemId || message.payload?.id || "");
          if (isContentScript && senderTabId) {
            const allowed = authorizedTabItems.get(senderTabId);
            if (!allowed || !allowed.has(reqItemId)) {
              sendResponse({
                ok: false,
                type: "credential_secret",
                error: {
                  code: "UNAUTHORIZED_ITEM_ACCESS",
                  message:
                    "Yêu cầu truy cập mật khẩu bị từ chối do không thuộc phạm vi của trang web hiện tại.",
                },
              });
              return;
            }
          }
          const resp = await transport.dispatch(message);
          sendResponse(resp);
          return;
        }

        if (message.action === "save_credential") {
          if (isContentScript && senderTabUrl) {
            if (!message.payload?.websiteUrl) {
              message.payload = { ...(message.payload || {}), websiteUrl: senderTabUrl };
            }
          }
          const resp = await transport.dispatch(message);
          sendResponse(resp);
          return;
        }

        if (message.action === "passkey_query") {
          if (isContentScript && senderHostname) {
            message.payload = { ...(message.payload || {}), rpId: senderHostname };
          }
          const resp = await passkeyRouter.handleQuery(message.payload);
          sendResponse(resp);
          return;
        }

        if (message.action === "passkey_create") {
          if (isContentScript && senderHostname) {
            const requestedRpId = String(message.payload?.rpId || senderHostname)
              .trim()
              .toLowerCase();
            const isSuffix =
              requestedRpId === senderHostname ||
              senderHostname.endsWith("." + requestedRpId) ||
              senderHostname.replace(/^www\./, "") === requestedRpId.replace(/^www\./, "");
            if (!isSuffix || !requestedRpId.includes(".")) {
              sendResponse({
                ok: false,
                error: {
                  code: "SECURITY_ERROR",
                  message: `Relying party ID "${requestedRpId}" is not permitted for host "${senderHostname}"`,
                },
              });
              return;
            }
            message.payload.origin = senderOrigin || `https://${requestedRpId}`;
          }
          const resp = await passkeyRouter.handleCreate(message.payload);
          sendResponse(resp);
          return;
        }

        if (message.action === "passkey_get") {
          if (isContentScript && senderHostname) {
            const requestedRpId = String(message.payload?.rpId || senderHostname)
              .trim()
              .toLowerCase();
            const isSuffix =
              requestedRpId === senderHostname ||
              senderHostname.endsWith("." + requestedRpId) ||
              senderHostname.replace(/^www\./, "") === requestedRpId.replace(/^www\./, "");
            if (!isSuffix || !requestedRpId.includes(".")) {
              sendResponse({
                ok: false,
                error: {
                  code: "SECURITY_ERROR",
                  message: `Relying party ID "${requestedRpId}" is not permitted for host "${senderHostname}"`,
                },
              });
              return;
            }
            message.payload.origin = senderOrigin || `https://${requestedRpId}`;
          }
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
