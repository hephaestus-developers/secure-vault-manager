<div align="center">

<img src="ui/resources/icons/brand/app_logo.svg" alt="Secure Vault Manager Logo" width="112" height="112" />

# Secure Vault Manager (SVM)

**Local-First, Offline-First Password Manager · Passkey (FIDO2/WebAuthn) · TOTP 2FA · OpenSSH Agent**

<p>
  <img src="https://img.shields.io/badge/Rust-14_Crates-000000?style=for-the-badge&logo=rust&logoColor=white" alt="Rust" />
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" alt="C++17" />
  <img src="https://img.shields.io/badge/Qt-6_Widgets-41CD52?style=for-the-badge&logo=qt&logoColor=white" alt="Qt6" />
  <img src="https://img.shields.io/badge/TypeScript-MV3_Extension-3178C6?style=for-the-badge&logo=typescript&logoColor=white" alt="TypeScript" />
  <img src="https://img.shields.io/badge/CMake-Ninja-064F8C?style=for-the-badge&logo=cmake&logoColor=white" alt="CMake" />
</p>

<p>
  <img src="https://img.shields.io/badge/Crypto-ChaCha20--Poly1305-1E1E2E?style=flat-square" alt="ChaCha20-Poly1305" />
  <img src="https://img.shields.io/badge/Passkey-WebAuthn_ES256-1E66F5?style=flat-square" alt="WebAuthn ES256" />
  <img src="https://img.shields.io/badge/SSH_Agent-Ed25519-45475A?style=flat-square" alt="OpenSSH Agent" />
  <img src="https://img.shields.io/badge/Platform-Linux_%7C_Windows_%7C_macOS-313244?style=flat-square" alt="Platforms" />
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-40A02B?style=flat-square" alt="MIT License" /></a>
</p>

</div>

---

**Secure Vault Manager (SVM)** là hệ sinh thái quản lý mật khẩu, định danh số, mã xác thực 2FA (TOTP), khóa xác thực **Passkey (FIDO2/WebAuthn)** và **OpenSSH Agent** hoạt động cục bộ theo triết lý **Local-First, Offline-First**.

Dự án được xây dựng theo kiến trúc phân lớp bản địa (Native) tối ưu cho hiệu năng máy trạm và an toàn bộ nhớ:

- **Lõi mật mã & Dịch vụ nền (`core/`):** Viết bằng **Rust** (Cargo Workspace **14 crates**), biên dịch thành thư viện động chuẩn C-ABI (`libsvm_core.so` / `libsvm_core.dylib` / `svm_core.dll`) kèm binary relay `svm-native-host` và công cụ CLI `svm-cli`.
- **Ứng dụng Desktop Native (`ui/`):** Viết bằng **C++17 & Qt6 Widgets** (`svm`), tích hợp giao diện không viền (`UnifiedHeaderBar`) với Inline Menubar (`Vault`, `Item`, `View`, `Help`), Context Menu chuột phải không đổi focus, hai chủ đề **Catppuccin Mocha / Latte** và song ngữ **EN / VI**.
- **Tiện ích mở rộng trình duyệt (`extension/`):** WebExtension **Manifest V3** bằng TypeScript cho Chrome/Chromium, Brave, Edge và Mozilla Firefox, kết nối với Desktop App qua **Native Messaging Host** (`io.hephaestus.secure_vault_manager`).

---

## Tính Năng Cốt Lõi

1. **Quản Lý Đa Kho & 9 Danh Mục Chuyên Biệt:**
   - Hỗ trợ tạo, tùy chỉnh và chuyển đổi nhiều Vault (`VaultProfile`), di chuyển bản ghi nhanh giữa các Vault (**Move to Vault**).
   - 9 danh mục dữ liệu: `Login` (kèm TOTP 2FA), `Passkey`, `Card`, `Identity`, `SecureNote`, `SshKey`, `Database`, `Server`, `Wifi` và hệ thống **Trường Tùy Biến (Custom Fields)** với 4 kiểu (`text`, `concealed`, `boolean`, `date`).
2. **Mật Mã Học & Bảo Vệ Bộ Nhớ:**
   - Dẫn xuất khóa `PBKDF2-HMAC-SHA256` (`100,000` vòng lặp) kết hợp mô hình **Dual-Wrapped Envelope** (`Master Password` và `Recovery Key` bọc độc lập khóa `DEK` 256-bit bằng `ChaCha20-Poly1305` trong header `SVM1` 169 byte).
   - Bảo vệ khóa `DEK` đang hoạt động bằng `mlock` / `VirtualLock`, zeroize khi thu hồi, vô hiệu hóa core dump (`prctl(PR_SET_DUMPABLE, 0)` / `ptrace`), tự động khóa khi nhàn rỗi/ngủ và tự động xóa clipboard nhạy cảm.
   - Hỗ trợ mở khóa sinh trắc học qua OS Keyring kèm tệp đồng hành `biometric.env` (`SVMB`).
3. **Passkey (FIDO2 / WebAuthn Authenticator):**
   - Triển khai Virtual Software Authenticator hỗ trợ **ES256 (ECDSA P-256 / SHA-256, COSE `-7`)** tại `svm-webauthn`, lưu trữ đầy đủ siêu dữ liệu (`rpId`, `credentialId`, `algorithm`, `userHandle`, `signCount`).
   - Can thiệp `navigator.credentials.create()` / `get()` qua MAIN world + modal xác nhận cô lập trong closed Shadow DOM, hỗ trợ mở khóa inline và fallback tự động về WebAuthn gốc của hệ điều hành.
4. **TOTP 2FA & Quét Mã QR Từ Màn Hình:**
   - Sinh mã OTP chuẩn RFC 6238; hỗ trợ kéo chọn vùng màn hình chứa mã QR trên trình duyệt (`crop-overlay.ts`), giải mã qua `BarcodeDetector` hoặc lõi Rust `rqrr` (`decode_qr` -> `update_totp`).
5. **OpenSSH Agent Tích Hợp (Unix):**
   - Cung cấp OpenSSH Agent Server (`svm-ssh-agent`) qua Unix Domain Socket (`$XDG_RUNTIME_DIR/svm/ssh-agent.sock`), hỗ trợ sinh và ký khóa `ssh-ed25519` từ RAM kèm hộp thoại phê duyệt ký số (`NotificationDialog`).
6. **Tệp Đính Kèm Mã Hóa, Lịch Sử Sửa Đổi & Chuyển Đổi Dữ Liệu:**
   - Tệp đính kèm mã hóa streaming theo từng chunk **64 KiB** (`SVMA` tại `attachments/*.bin`).
   - Tự động lưu tới **20 bản snapshot lịch sử** (`ItemRevision`) cho mỗi bản ghi và xem trước/khôi phục qua `RevisionHistoryDialog`.
   - Hỗ trợ Import/Export tương thích `.svm`, Bitwarden JSON, 1Password `.1pux`, KeePass XML và CSV trình duyệt.

---

## Bắt Đầu Nhanh (Quick Start)

### Yêu Cầu Môi Trường

- **Rust:** Toolchain `cargo` / `rustc` (Edition 2021) + `rustfmt`, `clippy` (tùy chọn `cargo-deny`).
- **C++ & Qt6:** Trình biên dịch C++17 (`g++`/`clang++`), `cmake` (>= 3.20), `ninja`, và Qt6 (`Core`, `Gui`, `Network`, `Widgets`), `clang-format`.
- **Extension:** Node.js (hỗ trợ `stripTypeScriptTypes`) và `pnpm` (tùy chọn `rsvg-convert` để render icon PNG từ SVG).

### Biên Dịch, Khởi Chạy & Kiểm Định

```bash
# 1. Biên dịch toàn bộ Rust Core + Desktop Qt6 UI (Debug hoặc --release)
./scripts/build.sh
./scripts/build.sh --release

# 2. Khởi chạy ứng dụng Desktop, Native Host hoặc CLI
./scripts/run.sh --app
./scripts/run.sh --cli -- --help

# 3. Biên dịch Browser Extension (Chrome & Firefox -> extension/dist/{chrome,firefox})
./scripts/build-extension.sh

# 4. Chạy toàn bộ kiểm thử (Cargo Workspace tests, Doc tests, Python FFI smoke test, CTest)
./scripts/test.sh

# 5. Kiểm tra chất lượng mã & định dạng (Clippy, Rustdoc, cargo-deny, oxlint, rustfmt, clang-format, prettier)
./scripts/lint.sh
./scripts/format.sh --check
```

---

## Cấu Trúc Dự Án (Repository Layout)

```text
secure-vault-manager/
├── CMakeLists.txt           # Root CMake tích hợp core/ và ui/
├── CMakePresets.json        # Cấu hình preset Debug / Release / CI
├── STYLE_GUIDELINE.md       # Quy chuẩn giao diện UI/UX (Catppuccin Mocha/Latte & QSS)
├── LICENSE                  # Giấy phép MIT
├── core/                    # Rust Core Workspace (14 crates -> libsvm_core, svm-native-host, svm-cli)
├── ui/                      # Ứng dụng Desktop Qt6 Widgets C++17 (binary: svm)
├── extension/               # Browser Extension Manifest V3 (TypeScript, build.mjs, oxlint, prettier)
├── packaging/               # Bộ đóng gói phân phối cho Linux (.deb/.rpm/AppImage), macOS (.dmg/.pkg), Windows (.iss/.wxs)
├── scripts/                 # Script phát triển: build, run, test, lint, format, clean, gen-header, build-extension
└── docs/                    # Bộ 11 tài liệu đặc tả kiến trúc & giao thức kỹ thuật
```

---

## Danh Mục Tài Liệu Kỹ Thuật (`docs/` & `STYLE_GUIDELINE.md`)

| Tài Liệu | Nội Dung Trọng Tâm |
| :--- | :--- |
| [STYLE_GUIDELINE.md](STYLE_GUIDELINE.md) | Hệ phân cấp bề mặt, bảng màu Catppuccin Mocha/Latte, CTA nghịch đảo và quy chuẩn thành phần Qt6/QSS. |
| [01. BRD - Business Requirements Document.md](docs/01.%20BRD%20-%20Business%20Requirements%20Document.md) | Bối cảnh, mục tiêu chiến lược, phạm vi dự án và yêu cầu nghiệp vụ chi tiết. |
| [02. SRS - System Requirements Specification.md](docs/02.%20SRS%20-%20System%20Requirements%20Specification.md) | Đặc tả yêu cầu chức năng (FR) và phi chức năng (NFR) của Desktop, Core và Extension. |
| [03. SAD - Software Architecture Document.md](docs/03.%20SAD%20-%20Software%20Architecture%20Document.md) | Kiến trúc phân lớp, danh mục 14 crates, cây thư mục thực tế, Prompt/IPC và mô hình luồng. |
| [04. FFI Contract.md](docs/04.%20FFI%20Contract.md) | Đặc tả 67 hàm C-ABI (`svm_core.h`), quy tắc sở hữu bộ nhớ, `svm_free_secret` và `catch_unwind`. |
| [05. Vault File Format.md](docs/05.%20Vault%20File%20Format.md) | Đặc tả tệp `vault.svm` (Header `SVM1` 169B), `biometric.env` (`SVMB` 64B) và chunk đính kèm `SVMA`. |
| [06. Extension Bridge Protocol.md](docs/06.%20Extension%20Bridge%20Protocol.md) | Giao thức Native Messaging / IPC framing (4B LE + JSON) và chi tiết 15+ action JSON. |
| [07. Passkey Implementation.md](docs/07.%20Passkey%20Implementation.md) | Kiến trúc can thiệp WebAuthn, quy trình `passkey_create`/`get` ES256 và Shadow DOM consent modal. |
| [08. SSH Agent Protocol.md](docs/08.%20SSH%20Agent%20Protocol.md) | Giao thức OpenSSH Agent qua Unix Domain Socket, khóa `ssh-ed25519` và phê duyệt ký số. |
| [09. Threat Model.md](docs/09.%20Threat%20Model.md) | Phân tích rủi ro STRIDE, ranh giới tin cậy và các giới hạn bảo mật hiện tại. |
| [10. Domain Data Schemas.md](docs/10.%20Domain%20Data%20Schemas.md) | Schema JSON cho `VaultProfile`, `VaultItem` phẳng 32 trường, `CustomField`, 9 danh mục và cấu hình. |
| [11. External Extensions Installation Specification.md](docs/11.%20External%20Extensions%20Installation%20Specification.md) | Đặc tả cài đặt Native Messaging Host (`io.hephaestus.secure_vault_manager`) và lộ trình External Extensions. |
| [12. Browser Extension Setup & Testing Guide.md](docs/12.%20Browser%20Extension%20Setup%20%26%20Testing%20Guide.md) | Hướng dẫn chi tiết từng bước nạp unpacked extension trên Chrome, Firefox, Brave, Edge; kịch bản test Autofill, TOTP QR Crop, Passkey WebAuthn và khắc phục lỗi. |

---

## Giấy Phép (License)

Phát hành theo giấy phép **MIT** — xem chi tiết tại [LICENSE](LICENSE).  
Copyright (c) 2026 **Hephaestus Developers**.
