/**
 * @file extension/src/injected/passkey_injected.ts
 * @brief Kịch bản Main World đánh chặn `navigator.credentials.create` và `navigator.credentials.get` cho WebAuthn Passkey.
 */

import { base64UrlToBytes, bytesToBase64Url } from "../services/webauthn";

(function installPasskeyInterceptor() {
  if (typeof window === "undefined" || !navigator.credentials) {
    return;
  }

  const nativeCreate = navigator.credentials.create.bind(navigator.credentials);
  const nativeGet = navigator.credentials.get.bind(navigator.credentials);

  function requestPasskeyBridge(
    action: "passkey_create" | "passkey_get",
    payload: Record<string, unknown>,
    signal?: AbortSignal | null
  ): Promise<any> {
    if (signal?.aborted) {
      return Promise.resolve(null);
    }

    const requestId = `pk-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
    return new Promise((resolve) => {
      function cleanup() {
        clearTimeout(timeout);
        window.removeEventListener("message", onMessage);
        if (signal) {
          signal.removeEventListener("abort", onAbort);
        }
      }

      const timeout = setTimeout(() => {
        cleanup();
        resolve(null);
      }, 120000);

      function onAbort() {
        cleanup();
        window.postMessage(
          {
            source: "svm-passkey-page",
            requestId,
            action: "passkey_cancel",
          },
          window.location.origin
        );
        resolve(null);
      }

      function onMessage(event: MessageEvent) {
        if (
          event.source === window &&
          event.data?.source === "svm-passkey-content" &&
          event.data?.requestId === requestId
        ) {
          cleanup();
          resolve(event.data.response);
        }
      }

      if (signal) {
        signal.addEventListener("abort", onAbort);
      }

      window.addEventListener("message", onMessage);
      window.postMessage(
        {
          source: "svm-passkey-page",
          requestId,
          action,
          payload,
        },
        window.location.origin
      );
    });
  }

  navigator.credentials.create = async function (options?: CredentialCreationOptions): Promise<Credential | null> {
    if (options?.publicKey) {
      const pk = options.publicKey;
      const rpId = pk.rp?.id || window.location.hostname;
      const challengeBytes =
        pk.challenge instanceof ArrayBuffer
          ? new Uint8Array(pk.challenge)
          : ArrayBuffer.isView(pk.challenge)
            ? new Uint8Array(pk.challenge.buffer, pk.challenge.byteOffset, pk.challenge.byteLength)
            : new Uint8Array(16);

      let userHandle = "";
      if (pk.user?.id) {
        const uhBytes =
          pk.user.id instanceof ArrayBuffer
            ? new Uint8Array(pk.user.id)
            : ArrayBuffer.isView(pk.user.id)
              ? new Uint8Array(pk.user.id.buffer, pk.user.id.byteOffset, pk.user.id.byteLength)
              : new Uint8Array();
        if (uhBytes.length > 0) {
          userHandle = bytesToBase64Url(uhBytes);
        }
      }

      const bridgeResp = await requestPasskeyBridge(
        "passkey_create",
        {
          rpId,
          rpName: pk.rp?.name || rpId,
          userName: pk.user?.name || "user",
          userDisplayName: pk.user?.displayName || pk.user?.name || "User",
          userHandle,
          challenge: bytesToBase64Url(challengeBytes),
          origin: window.location.origin,
        },
        options?.signal
      );

      if (bridgeResp?.ok && bridgeResp.payload?.credentialId) {
        const rawIdBytes = base64UrlToBytes(bridgeResp.payload.credentialId);
        return {
          id: bridgeResp.payload.credentialId,
          rawId: rawIdBytes.buffer as ArrayBuffer,
          type: "public-key",
          authenticatorAttachment: "platform",
          getClientExtensionResults: () => ({}),
          response: {
            clientDataJSON: base64UrlToBytes(bridgeResp.payload.clientDataJSON || "").buffer as ArrayBuffer,
            attestationObject: base64UrlToBytes(bridgeResp.payload.attestationObject || "").buffer as ArrayBuffer,
          },
        } as unknown as Credential;
      }
    }
    return nativeCreate(options);
  };

  navigator.credentials.get = async function (options?: CredentialRequestOptions): Promise<Credential | null> {
    // Bỏ qua Conditional UI (yêu cầu Autofill ngầm lúc tải trang) để không hiển thị Modal làm phiền người dùng
    if (options?.mediation === "conditional") {
      return nativeGet(options);
    }

    if (options?.publicKey) {
      const pk = options.publicKey;
      const rpId = pk.rpId || window.location.hostname;
      const challengeBytes =
        pk.challenge instanceof ArrayBuffer
          ? new Uint8Array(pk.challenge)
          : ArrayBuffer.isView(pk.challenge)
            ? new Uint8Array(pk.challenge.buffer, pk.challenge.byteOffset, pk.challenge.byteLength)
            : new Uint8Array(16);

      const bridgeResp = await requestPasskeyBridge(
        "passkey_get",
        {
          rpId,
          challenge: bytesToBase64Url(challengeBytes),
          origin: window.location.origin,
        },
        options?.signal
      );

      if (bridgeResp?.ok && bridgeResp.payload?.credentialId) {
        const rawIdBytes = base64UrlToBytes(bridgeResp.payload.credentialId);
        const userHandleBytes = bridgeResp.payload.userHandle
          ? base64UrlToBytes(bridgeResp.payload.userHandle)
          : null;

        return {
          id: bridgeResp.payload.credentialId,
          rawId: rawIdBytes.buffer as ArrayBuffer,
          type: "public-key",
          authenticatorAttachment: "platform",
          getClientExtensionResults: () => ({}),
          response: {
            clientDataJSON: base64UrlToBytes(bridgeResp.payload.clientDataJSON || "").buffer as ArrayBuffer,
            authenticatorData: base64UrlToBytes(bridgeResp.payload.authenticatorData || "").buffer as ArrayBuffer,
            signature: base64UrlToBytes(bridgeResp.payload.signature || "").buffer as ArrayBuffer,
            userHandle: userHandleBytes ? (userHandleBytes.buffer as ArrayBuffer) : null,
          },
        } as unknown as Credential;
      }
    }
    return nativeGet(options);
  };
})();
