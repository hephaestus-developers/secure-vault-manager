# Hướng dẫn chi tiết nộp Add-on lên Mozilla (Firefox Add-on Developer Hub)

Tài liệu này hướng dẫn chi tiết từng bước để nộp và phát hành tiện ích **Secure Vault Manager** lên kho tiện ích chính thức của Firefox (**addons.mozilla.org - AMO**).

---

## 1. Chuẩn bị tệp cài đặt (Add-on Package)

Mozilla yêu cầu tệp nộp phải ở định dạng `.zip` (hoặc `.xpi`), trong đó các tệp của extension (`manifest.json`, `background.js`, `content.js`,...) phải nằm ngay tại **thư mục gốc** của tệp zip (không được bọc trong một thư mục cha).

### Lệnh đóng gói tự động

Mở terminal tại thư mục gốc của dự án và chạy script đóng gói chuẩn:

```bash
# Đóng gói tự động cả bản Chromium, Firefox và Source Code ZIP
./scripts/build-extension.sh
```

> **Các tệp đầu ra tại `extension/dist/` sẵn sàng tải lên:**
>
> 1. Tệp cài đặt Firefox: `extension/dist/secure-vault-manager-firefox.zip` (~128 KB).
> 2. Tệp mã nguồn đối chiếu: `extension/dist/extension-source-code.zip` (~165 KB).

---

## 2. Quy trình từng bước trên Mozilla Developer Hub

### Bước 1: Chọn hình thức phân phối (How to Distribute this Version)

1. **Chọn**: `On this site. Your submission is publicly listed on addons.mozilla.org.`
   - *Lựa chọn này giúp tiện ích xuất hiện trên kho công khai của Firefox, người dùng có thể tìm kiếm và cài đặt với 1 click, và Firefox sẽ tự động cập nhật khi bạn phát hành bản mới.*
2. Nhấn nút **Continue**.

---

### Bước 2: Tải tệp bản dựng lên (Upload Version)

1. Nhấn nút **Select a file...**
2. Chọn tệp:

   ```text
   extension/dist/secure-vault-manager-firefox.zip
   ```

3. Hệ thống kiểm tra tự động của Mozilla sẽ phân tích tệp (chạy bộ xác thực linter).
   - Bản dựng đã được cấu hình chuẩn ID `secure-vault-manager-extension@hephaestus.dev`, manifest V3, `strict_min_version: 140.0` (Android `142.0`), `data_collection_permissions: { required: ["none"] }` và loại bỏ hoàn toàn `innerHTML`.
   - Bài kiểm tra sẽ báo xanh hoàn toàn (**0 errors, 0 warnings**).
4. Nhấn **Continue**.

---

### Bước 3: Xác nhận và nộp mã nguồn gốc (Do You Need to Submit Source Code?)

Mozilla đặt câu hỏi để kiểm tra tính minh bạch của mã nguồn:
> *"Do you use any of the following in your extension?*
>
> - *Code generators or minifiers*
> - *Tools that combine multiple files into a single file, such as webpack*
> - *Web template engines, both for HTML and CSS*
> - *Any other tool that takes code or files, applies processing, and generates code or file(s) to include in the extension"*

#### 1. Lựa chọn bắt buộc

- ✅ **Chọn: `Yes`**
- > [!IMPORTANT]
  > **Tại sao phải chọn `Yes`?**
  > Extension của chúng ta được viết bằng **TypeScript** và sử dụng trình đóng gói bundler Node.js tích hợp để bundle các module thành `popup.js`, `content.js`, `background.js`.
  > Nếu chọn `No`, đội ngũ reviewer của Mozilla khi mở tệp kiểm tra thấy mã đã qua xử lý đóng gói mà không có mã nguồn đối chiếu sẽ **từ chối duyệt (Reject)** tiện ích ngay lập tức theo chính sách Add-on Policy của Mozilla.

#### 2. Tải tệp mã nguồn gốc (Upload Source Code)

Sau khi bạn tích chọn **`Yes`**, giao diện sẽ mở rộng ra thêm mục đính kèm tệp:

- Nhấn nút tải tệp và chọn tệp nén mã nguồn đã được tạo sẵn tại:

  ```text
  extension/dist/extension-source-code.zip
  ```

  *(Tệp này chứa toàn bộ mã nguồn TypeScript, cấu hình `package.json`, scripts build mà không kèm thư mục build `dist/` hay `node_modules/`).*

#### 3. Nhập hướng dẫn biên dịch (Build Instructions)

Tại ô văn bản hướng dẫn build dành cho reviewer, hãy sao chép và dán đoạn sau:

```text
Build Environment:
- Operating System: Linux / macOS / Windows
- Node.js: >= 20.x
- Package Manager: npm (or pnpm)

Build Steps:
1. Extract the source code and navigate into the project root:
   cd extension
2. Install dependencies (optional, build uses Node.js standard modules):
   npm install
3. Run the production build and packaging:
   npm run build
   (or execute: node scripts/build.mjs && node scripts/post-build.js)

Output Verification:
The exact compiled files for Firefox will be produced in:
extension/dist/firefox/
```

#### 4. Nhấn nút **Continue** để tiếp tục sang Bước 4

---

### Bước 4: Hướng dẫn chi tiết điền biểu mẫu "Describe Add-on"

Dưới đây là hướng dẫn cụ thể từng trường trong trang **Describe Add-on**:

---

#### 1. Name (Tên tiện ích)

- **Giá trị**: `Secure Vault Manager`
- *Giải thích*: Giữ nguyên tên mặc định được trích xuất tự động từ `manifest.json`.

---

#### 2. Add-on URL (Đường dẫn tiện ích trên AMO)

- **Giá trị**: `https://addons.mozilla.org/.../secure-vault-manager`
- *Giải thích*: Mặc định hệ thống tự sinh slug `secure-vault-manager`, bạn giữ nguyên không cần bấm Edit.

---

#### 3. Summary (Tóm tắt ngắn - Bắt buộc \*)

- **Giá trị**:

  ```text
  Autofill logins, verification codes, and passkeys securely from Secure Vault Manager.
  ```

- *Giải thích*: Tóm tắt ngắn gọn dưới 250 ký tự (chuỗi trên có 84 ký tự), giải thích rõ công năng: tự động điền tài khoản, mã 2FA và Passkeys an toàn.

---

#### 4. Description (Mô tả chi tiết tính năng)

Hỗ trợ định dạng Markdown. Hãy sao chép toàn bộ đoạn văn bản mẫu chuẩn sau và dán vào ô **Description**:

```markdown
**Secure Vault Manager** is the official Firefox browser companion for the open-source Secure Vault Manager desktop application. It enables seamless, zero-knowledge autofill for logins, one-time verification codes (TOTP/2FA), and modern FIDO2/WebAuthn Passkeys.

### ✨ Key Features

- 🔐 **Zero-Knowledge Security**: Your credentials and keys are encrypted end-to-end using state-of-the-art cryptography (PBKDF2-HMAC-SHA256, ChaCha20-Poly1305). Nothing is ever sent to third-party cloud servers.
- ⚡ **Smart Autofill**: Seamlessly detects login and registration forms, filling usernames and passwords with a single click.
- 🔑 **FIDO2 / WebAuthn Passkeys**: Native passkey integration allows you to create and authenticate passkeys directly within your secure vault.
- ⏱️ **Integrated 2FA / TOTP**: Generates and autofills two-factor authentication codes in real time without needing your phone.
- 🖥️ **Companion Desktop Architecture**: Communicates locally with the Secure Vault Manager desktop app via secure Native Messaging (`io.hephaestus.secure_vault_manager`).
- 🛡️ **Biometric & Master Key Unlock**: Unlock your vault securely using system biometrics or your Master Password.

### 📋 Prerequisites

This extension requires the **Secure Vault Manager Desktop Application** to be installed and running on your computer. When the desktop app is closed, the extension remains safely in a protected offline state.

- Source Code & Desktop App: https://github.com/hephaestus-developers/secure-vault-manager
```

---

#### 5. Checkboxes (Hộp kiểm trạng thái)

| Hộp kiểm | Lựa chọn | Giải thích |
| :--- | :---: | :--- |
| **This add-on is experimental** | ❌ **Không tích** | Tiện ích đã hoàn thiện và sẵn sàng cho người dùng cuối sử dụng ổn định. |
| **This add-on requires payment, non-free services or software, or additional hardware** | ❌ **Không tích** | Tiện ích và ứng dụng Desktop đi kèm hoàn toàn **miễn phí và là mã nguồn mở 100%**, không yêu cầu dịch vụ tính phí hay phần cứng độc quyền. |

---

#### 6. Categories (Chọn tối đa 3 danh mục)

- ✅ **Tích chọn**: **`Privacy & Security`**
- *(Tùy chọn bổ sung)*: Bạn có thể chọn thêm **`Web Development`** nếu muốn, nhưng khuyến nghị chỉ cần chọn **`Privacy & Security`** để tối ưu hóa vị trí hiển thị trên Store.

---

#### 7. Support Email

- **Giá trị**: Điền email hỗ trợ kỹ thuật của bạn / tổ chức (ví dụ: `support@hephaestus.dev` hoặc email cá nhân của bạn).

---

#### 8. Support Website

- **Giá trị**:

  ```text
  https://github.com/hephaestus-developers/secure-vault-manager/issues
  ```

- *Giải thích*: Trang tiếp nhận báo lỗi và hỗ trợ người dùng trên GitHub.

---

#### 9. License (Giấy phép - Bắt buộc \*)

- **Lựa chọn**: **`MIT License`** (hoặc **`Apache License 2.0`**)
- *Giải thích*: Dự án Secure Vault Manager được phân phối theo giấy phép kép `MIT OR Apache-2.0`. Lựa chọn `MIT License` là lựa chọn phổ biến và phù hợp nhất cho extension mã nguồn mở.

---

#### 10. Privacy Policy (Chính sách quyền riêng tư)

- ✅ **Tích chọn vào ô**: `This add-on has a Privacy Policy`
- Khi tích chọn, một ô văn bản sẽ hiện ra, hãy dán nội dung sau vào:

```markdown
Secure Vault Manager operates on a strict Local Zero-Knowledge model:

1. No Data Collection: The extension does not collect, record, track, or transmit any user browsing activity, form inputs, personal identifiers, or telemetry data.
2. Local Communication: All communication is strictly local via Native Messaging with the Secure Vault Manager desktop application running on your own machine.
3. Cryptographic Protection: Vault secrets and master credentials remain securely encrypted and never leave your local device.
```

---

#### 11. Notes to Reviewer (Ghi chú dành cho Đội ngũ Kiểm duyệt Mozilla)

> [!IMPORTANT]
> Đây là phần **rất quan trọng**. Do extension sử dụng quyền `nativeMessaging` để kết nối với Desktop App, nếu không giải thích rõ, reviewer có thể từ chối vì họ mở extension lên chỉ thấy trạng thái "Ứng dụng Desktop chưa khởi chạy" (Desktop App Offline).

Hãy sao chép và dán toàn bộ đoạn sau vào ô **Notes to Reviewer**:

```text
Dear AMO Reviewers,

This extension is an open-source companion extension for the desktop password manager "Secure Vault Manager".

1. Architecture & Native Messaging:
- The extension communicates exclusively with the local companion desktop host via Native Messaging under the host name "io.hephaestus.secure_vault_manager".
- All cryptographic keys, passwords, and passkeys are managed locally by the companion core.
- No network telemetry or analytics are collected or sent to external servers.

2. Testing the Extension:
- When the companion desktop app is not running, opening the popup displays the expected and safe "Desktop App Offline" state with "Open Secure Vault Manager" and "Check Connection" buttons.
- When on web pages with login forms, the content script identifies input fields to offer autofill capabilities once connected to the local host.
- The WebAuthn passkey bridge intercepts credential creation/assertion calls and falls back cleanly to browser native prompts when cancelled.

3. Companion App & Source Code:
- The complete source code for both the extension and companion desktop app is available at:
  https://github.com/hephaestus-developers/secure-vault-manager

Thank you for your time and review!
```

---

### Bước 5: Hoàn tất & Gửi duyệt (Submit)

1. Kiểm tra lại toàn bộ các trường đã điền ở Bước 4.
2. Nhấn nút **Submit Version** (hoặc nút **Save Changes / Submit** ở cuối trang).
3. Sau khi gửi, tiện ích sẽ được chuyển vào hàng đợi kiểm duyệt (**Awaiting Review**). Thời gian kiểm duyệt thông thường từ **vài giờ đến 24-48 giờ**. Khi được duyệt, extension sẽ có trạng thái **Approved** và xuất hiện công khai trên kho AMO.

---

## 3. Xử lý lỗi thường gặp (Troubleshooting)

### Lỗi: `Duplicate add-on ID found.`

- **Nguyên nhân**:
  1. Bạn đang nộp phiên bản mới (Update) nhưng lại chọn nhầm nút *"Submit a New Add-on"*: Hãy vào trang **My Add-ons** (`/developers/addons`), bấm vào tiện ích đã có và chọn **"Upload a New Version"**.
  2. ID tiện ích đã có người khác đăng ký hoặc từng bị xóa khỏi AMO trước đây: Đảm bảo sử dụng file zip mới với ID chuẩn đã được cập nhật `secure-vault-manager-extension@hephaestus.dev`.
