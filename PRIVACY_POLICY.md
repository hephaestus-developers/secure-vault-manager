# Privacy Policy for Secure Vault Manager

**Last Updated:** November 13, 2026

## Overview

**Secure Vault Manager** (including the desktop application and the browser extensions for Chromium and Firefox) is designed from the ground up with a strict **Local Zero-Knowledge** security and privacy architecture.

Your privacy and the security of your data are our highest priorities.

---

## 1. Zero Data Collection

- **No Remote Telemetry**: The extension does not collect, log, track, or transmit any analytics, telemetry, or behavioral data.
- **No Third-Party Tracking**: We do not include any third-party trackers, advertisements, or profiling SDKs.
- **No Remote Storage**: None of your passwords, passkeys, verification codes, or user identifiers are ever sent to or stored on any external servers.

---

## 2. Local Architecture & Communication

- **Local Native Messaging**: The browser extension communicates exclusively with the local companion Secure Vault Manager application running on your own computer via secure inter-process communication (`dev.hephaestus.secure_vault_manager`).
- **End-to-End Encryption**: All vault contents remain encrypted using industry-standard cryptography (Argon2id and ChaCha20-Poly1305). Data is decrypted only locally on demand when authorized by your Master Password or system biometrics.

---

## 3. Browser Permissions

The extension requests only the minimum permissions necessary to perform its functions:

- **`nativeMessaging`**: To communicate securely with the local desktop application.
- **`activeTab` & `scripting`**: To identify login and registration form fields on the active tab and autofill credentials only upon user action.
- **`clipboardWrite`**: To allow you to copy passwords, usernames, or 2FA codes directly to your clipboard when you click the copy button.
- **`storage`**: To persist local extension UI preferences and transient session status inside your browser.
- **`host_permissions: <all_urls>`**: To enable the password manager to detect login forms on any website you choose to log into.

---

## 4. Contact Us

If you have any questions or concerns regarding this Privacy Policy or the security of Secure Vault Manager, please open an issue on our official repository:

- GitHub: <https://github.com/hephaestus-developers/secure-vault-manager/issues>
