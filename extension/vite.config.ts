/**
 * @file extension/vite.config.ts
 * @brief Cấu hình đóng gói đa điểm nhập (Multi-Entry) cho Browser Extension Manifest V3.
 */

export default {
  root: ".",
  publicDir: "public",
  build: {
    outDir: "dist/chrome",
    emptyOutDir: true,
    rollupOptions: {
      input: {
        popup: "popup.html",
        background: "src/background/index.ts",
        content: "src/content/index.ts",
        passkey_injected: "src/injected/passkey_injected.ts",
      },
      output: {
        entryFileNames: "[name].js",
        chunkFileNames: "chunks/[name].js",
        assetFileNames: "assets/[name][extname]",
      },
    },
  },
};
