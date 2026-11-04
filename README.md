# Secure Vault Manager (SVM)

**Secure Vault Manager (SVM)** là hệ sinh thái quản lý mật khẩu, định danh số, khóa xác thực **Passkey (FIDO2/WebAuthn)** và **OpenSSH Agent** cục bộ theo triết lý **Local-First, Zero-Knowledge**.

Hệ thống được thiết kế tối ưu cho hiệu năng máy trạm và an toàn bộ nhớ mức phần cứng:

- **Lõi mật mã & Dịch vụ nền (`core/`):** Viết hoàn toàn bằng **Rust** (15 crates chuyên biệt), biên dịch thành thư viện chia sẻ chuẩn C-ABI (`libsvm_core.so` / `.dylib` / `.dll`) kèm binary relay `svm-native-host` và công cụ dòng lệnh `svm-cli`.
- **Giao diện người dùng máy trạm (`ui/`):** Viết bằng **C++17 & Qt6 Widgets Native**, khởi động tức thời (`< 150ms`), tiêu thụ cực ít tài nguyên (`< 40MB RAM` ở chế độ khay hệ thống).
- **Tiện ích mở rộng trình duyệt (`extension/`):** Hỗ trợ Chromium (Chrome, Brave, Edge) và Firefox theo chuẩn **Manifest V3**, giao tiếp qua kênh **Native Messaging Host** được mã hóa đầu cuối bằng `X25519 + HKDF-SHA256 + AES-256-GCM`.

---

## Tính Năng Cốt Lõi

1. **Quản Lý Đa Kho & 9 Danh Mục Dữ Liệu Chuyên Biệt:** Hỗ trợ đa kho lưu trữ độc lập (`Multi-Vault`) với 9 danh mục: `Login` (tích hợp Mật khẩu và mã TOTP 2FA), `Passkey` (khóa xác thực FIDO2/WebAuthn), `SshKey` (khóa SSH), `Card`, `SecureNote`, `Identity`, `Wifi`, `Database`, `Server`, cùng khả năng mở rộng Trường Tùy Biến (Custom Fields) không giới hạn trên toàn bộ các danh mục.
2. **Mật Mã Học & Bảo Vệ Bộ Nhớ Đa Tầng:**
   - Dẫn xuất khóa `Argon2id` (`m=64MiB, t=3, p=4`) kết hợp mô hình **Envelope Encryption** (`Master Password -> KEK -> DEK`) với thuật toán AEAD `ChaCha20-Poly1305`.
   - Khóa trang bộ nhớ vật lý (`mlock` / `VirtualLock`), tự động xóa sạch dữ liệu nhạy cảm (`ZeroizeOnDrop`), và vô hiệu hóa core dump toàn tiến trình (`PR_SET_DUMPABLE = 0`).
3. **Trình Xác Thực Passkey (FIDO2 / WebAuthn Authenticator):** Tích hợp bộ xử lý khóa ảo hỗ trợ `ES256 (P-256)` và `EdDSA (Ed25519)`, xác thực nghiêm ngặt `rpId` qua Public Suffix List (PSL) và phê duyệt trực tiếp trên giao diện Desktop.
4. **OpenSSH Agent Tích Hợp:** Cung cấp socket tương thích chuẩn `SSH_AUTH_SOCK`, nạp khóa trong RAM được bảo vệ và hiển thị hộp thoại phê duyệt (`SshApprovalDialog`) kèm thông tin tiến trình gọi (`PID`, đường dẫn binary).
5. **Tệp Đính Kèm Mã Hóa Streaming & Lịch Sử Phiên Bản:** Mã hóa tệp đính kèm lên tới `50MB` theo từng phân đoạn `64KiB` (`STREAM AEAD`) và lưu trữ tối đa 20 bản ghi lịch sử sửa đổi cho mỗi mục.

---

## Danh Mục Tài Liệu Thiết Kế Kỹ Thuật (`docs/`)

Toàn bộ đặc tả nghiệp vụ, kiến trúc phần mềm, giao thức nhị phân và mô hình bảo mật của dự án được quy định chi tiết trong thư mục [`docs/`](./docs):

| STT | Tài Liệu | Nội Dung Trọng Tâm |
| :--- | :--- | :--- |
| **01** | [01. BRD - Business Requirements Document.md](docs/01.%20BRD%20-%20Business%20Requirements%20Document.md) | Yêu cầu nghiệp vụ, mục tiêu chiến lược, phạm vi dự án và các chỉ số hiệu năng phi chức năng. |
| **02** | [02. SRS - System Requirements Specification.md](docs/02.%20SRS%20-%20System%20Requirements%20Specification.md) | Đặc tả yêu cầu chức năng chi tiết (FR) và yêu cầu phi chức năng (NFR) của toàn hệ thống. |
| **03** | [03. SAD - Software Architecture Document.md](docs/03.%20SAD%20-%20Software%20Architecture%20Document.md) | Kiến trúc phân lớp, thiết kế 15 Rust crates, cơ chế Prompt Broker hai chiều và kiến trúc MVC Qt6. |
| **04** | [04. FFI Contract.md](docs/04.%20FFI%20Contract.md) | Hợp đồng giao tiếp C-ABI (`svm_core.h`), quy tắc sở hữu bộ nhớ, an toàn đa luồng và chống Panic. |
| **05** | [05. Vault File Format.md](docs/05.%20Vault%20File%20Format.md) | Đặc tả cấu trúc nhị phân tệp `vault.svm` (Header 112 bytes, AAD), `biometric.env` và `attachments/*.blob`. |
| **06** | [06. Extension Bridge Protocol.md](docs/06.%20Extension%20Bridge%20Protocol.md) | Giao thức Native Messaging, IPC Framing, bắt cặp X25519 + Pairing Code 6 số và các Action JSON. |
| **07** | [07. Passkey Implementation.md](docs/07.%20Passkey%20Implementation.md) | Luồng can thiệp trình duyệt (`create`/`get`), xác thực `rpId` qua PSL và mã hóa phong bì hai lớp. |
| **08** | [08. SSH Agent Protocol.md](docs/08.%20SSH%20Agent%20Protocol.md) | Giao thức nhị phân OpenSSH Agent, cơ chế kiểm tra `SO_PEERCRED` và quy trình phê duyệt ký số. |
| **09** | [09. Threat Model.md](docs/09.%20Threat%20Model.md) | Phân tích rủi ro theo mô hình STRIDE, các ranh giới tin cậy (Trust Boundaries) và giới hạn bảo mật. |
| **10** | [10. Domain Data Schemas.md](docs/10.%20Domain%20Data%20Schemas.md) | Đặc tả JSON Schema cho `VaultItem`, Trường tùy biến (Custom Fields), 9 danh mục chuyên biệt, `ItemFilter`, `Prompt` và `AppSettings`. |

---

## Cấu Trúc Dự Án (Project Layout)

Dự án được tổ chức theo mô hình Monorepo chuẩn hóa với các phân hệ độc lập:

```text
secure-vault-manager/
├── CMakeLists.txt                         # Root build script tích hợp core và ui
├── CMakePresets.json                      # Cấu hình preset Ninja/Clang/GCC/MSVC
├── docs/                                  # 10 tài liệu đặc tả kiến trúc & kỹ thuật chi tiết
├── core/                                  # Rust Core workspace (15 crates, C-ABI shared library)
├── ui/                                    # Ứng dụng Desktop Qt6 Widgets (C++17 MVC architecture)
├── extension/                             # Tiện ích mở rộng trình duyệt WebExtension Manifest V3
├── shared-ts/                             # Gói TypeScript dùng chung giữa extension & frontend
├── packaging/                             # Bộ script đóng gói phân phối Linux, macOS, Windows
├── scripts/                               # Tiện ích phát triển: build, test, format, lint, WASM
├── .agents/skills/                        # Bộ kỹ năng hướng dẫn phát triển agentic coding
└── .github/workflows/                     # CI/CD tự động hóa kiểm thử và phát hành
```

> Chi tiết toàn bộ cây thư mục tệp tin (scaffold layout) và vai trò của từng thành phần mã nguồn được đặc tả chi tiết tại [03. SAD - Software Architecture Document.md](docs/03.%20SAD%20-%20Software%20Architecture%20Document.md#3-cấu-trúc-thư-mục--phân-hệ-toàn-dự-án-repository-scaffold-architecture).
