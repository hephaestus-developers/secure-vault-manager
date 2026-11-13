# Secure Vault Manager Extension - Source Code & Build Guide

This package contains the complete, unminified, and human-readable source code for the **Secure Vault Manager** Browser Extension (Manifest V3) for Firefox and Chromium browsers.

---

## 1. System Requirements & Prerequisites

To reproduce and build an exact copy of the extension from this source package:

- **Operating System**: Linux, macOS, or Windows
- **Node.js**: Version `>= 20.0.0` (LTS recommended)
- **Package Manager**: `npm` `>= 10.0.0` (bundled with Node.js) or `pnpm`

### Installing Node.js (if not already installed)

- **Official Download**: <https://nodejs.org/>
- **Via Package Manager (Ubuntu/Debian)**:

  ```bash
  sudo apt install nodejs npm
  ```

- **Via Package Manager (macOS)**:

  ```bash
  brew install node
  ```

---

## 2. Project Structure

```text
├── package.json          # Package manifest & build scripts
├── tsconfig.json         # TypeScript compiler configuration
├── popup.html            # Extension popup HTML entrypoint
├── public/               # Static assets, vector icons & browser manifests
│   ├── manifest.firefox.json   # Firefox MV3 manifest
│   ├── manifest.chrome.json    # Chromium MV3 manifest
│   ├── icons/                  # SVG vector icons
│   └── app_logo.svg            # Brand logo
├── scripts/
│   ├── build.mjs         # Multi-entry bundler using Node.js TypeScript transform
│   └── post-build.js     # Firefox distribution packager & manifest sync
└── src/
    ├── background/       # Background service worker & native IPC client
    ├── content/          # Content script, form detection & autofill
    │   └── ui/           # Scoped Shadow DOM overlays (Save Prompt, Crop, Passkey)
    ├── popup/            # Extension Popup MVC views & state management
    ├── injected/         # Injected WebAuthn/Passkey credential interceptor
    └── services/         # Protocol, domain matching & WebAuthn utilities
```

---

## 3. Step-by-Step Build Instructions

1. **Open a terminal** and navigate to this directory (where `package.json` is located).

2. **(Optional) Install development dependencies**:

   ```bash
   npm install
   ```

   *Note: The build process relies strictly on Node.js built-in modules (`node:module.stripTypeScriptTypes`, `node:fs`, `node:path`, `node:zlib`). No external bundlers or network downloads are required to produce the build output.*

3. **Execute the build script**:

   ```bash
   npm run build
   ```

   *(Alternatively, run the build scripts directly:)*

   ```bash
   node scripts/build.mjs && node scripts/post-build.js
   ```

4. **Verify Build Output**:
   The exact build output will be generated in:
   - **Firefox Add-on**: `dist/firefox/`
     - `dist/firefox/manifest.json` (Firefox MV3 manifest)
     - `dist/firefox/background.js` (Background service worker)
     - `dist/firefox/content.js` (Content script)
     - `dist/firefox/passkey_injected.js` (Injected passkey bridge)
     - `dist/firefox/popup.js`, `popup.html`, `popup.css`, `global.css`
     - `dist/firefox/icons/`
   - **Chromium Extension**: `dist/chrome/`
