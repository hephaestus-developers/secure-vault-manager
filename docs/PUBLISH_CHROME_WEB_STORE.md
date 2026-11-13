# Hướng dẫn chi tiết xuất bản Tiện ích lên Chrome Web Store

Tài liệu này hướng dẫn chi tiết từng bước để nộp, cấu hình thông tin cửa hàng và gửi kiểm duyệt tiện ích mở rộng **Secure Vault Manager** lên **Chrome Web Store**.

---

## 1. Chuẩn bị tệp cài đặt (Extension Package)

Google Chrome Web Store yêu cầu tệp nộp phải ở định dạng `.zip`, trong đó tệp `manifest.json` và các tệp script/tài nguyên phải nằm ngay tại **thư mục gốc** của tệp zip (không được lồng trong một thư mục cha).

### Lệnh đóng gói tự động

Mở terminal tại thư mục gốc của dự án và chạy:

```bash
# 1. Biên dịch tiện ích sang dist/chrome
pnpm --dir extension run build

# 2. Đóng gói thư mục dist/chrome thành file ZIP chuẩn
cd extension/dist/chrome
zip -FSr ../secure-vault-manager-chrome-0.1.0.zip .
cd ../../
```

> **Tệp ZIP đã đóng gói sẵn sàng nộp**:
>
> ```text
> /mnt/storage/hephaestus-studio/secure-vault-manager/extension/dist/secure-vault-manager-chrome-0.1.0.zip
> ```
>
> *(Kích thước ~60 KB, chứa manifest V3, background service worker, icons và scripts đã tối ưu).*

---

## 2. Các bước thực hiện trên Chrome Web Store Developer Dashboard

Truy cập: [https://chrome.google.com/webstore/devconsole](https://chrome.google.com/webstore/devconsole)

> [!NOTE]
> Nếu đây là lần đầu tiên bạn đăng ký tài khoản Chrome Web Store Developer:
>
> - Google yêu cầu thanh toán khoản phí đăng ký nhà phát triển 1 lần là **$5 USD**.
> - Tài khoản Google phải bật tính năng **Xác minh 2 bước (2-Step Verification)**.

---

### Bước 1: Tải tệp bản dựng lên (Upload Package)

1. Trên giao diện bảng điều khiển, nhấn nút **Thêm mục mới** (hoặc **+ New Item**).
2. Hộp thoại **"Thêm mục mới"** hiện lên (như ảnh bạn chụp):
   - Nhấn nút **Chọn tệp** (hoặc kéo thả tệp vào khung nét đứt).
   - Chọn tệp:

     ```text
     /mnt/storage/hephaestus-studio/secure-vault-manager/extension/dist/secure-vault-manager-chrome-0.1.0.zip
     ```

3. Chờ Google tải lên và trích xuất dữ liệu từ `manifest.json`. Sau khi tải xong, bạn sẽ được tự động chuyển đến trang cấu hình chi tiết của tiện ích.

---

### Bước 2: Trang thông tin trên Cửa hàng (Store Listing)

Ở menu bên trái, chọn mục **Trang thông tin trên Cửa hàng** (Store Listing) và điền các thông tin sau:

#### 1. Chi tiết trang thông tin (Listing Details)

- **Tên mục (Item name)**: `Secure Vault Manager` *(Mặc định tự động điền từ manifest)*.
- **Mô tả ngắn (Short description)** *(Tối đa 132 ký tự)*:

  ```text
  Autofill logins, verification codes, and passkeys securely from Secure Vault Manager.
  ```

- **Mô tả chi tiết (Detailed description)**:
  Sao chép và dán nội dung sau vào ô mô tả:

  ```text
  Secure Vault Manager is the official Chromium browser companion for the open-source Secure Vault Manager desktop application. It delivers zero-knowledge autofill for passwords, two-factor authentication codes (TOTP), and next-generation FIDO2/WebAuthn Passkeys.

  KEY FEATURES:

  - Zero-Knowledge Security: Your master credentials and vault data are encrypted end-to-end using state-of-the-art cryptography (Argon2id and ChaCha20-Poly1305). Secrets never leave your device unencrypted.
  - Smart Autofill: Instantly identifies login forms to auto-fill usernames and passwords securely with a single click or keyboard shortcut (Ctrl+Shift+L).
  - Integrated 2FA / TOTP: Automatically generates and fills one-time verification codes in real time without needing an external authenticator app.
  - Native WebAuthn Passkeys: Create and authenticate passkeys directly inside your secure vault with biometric or master authorization.
  - Safe In-Page Save Prompts: Conveniently capture and update newly registered accounts or changed passwords directly to your desired vault.
  - Companion Desktop Integration: Seamlessly connects to your local desktop application via high-performance Native Messaging.

  PREREQUISITES:
  This extension is a browser companion and requires the Secure Vault Manager desktop application to be running on your machine. When the desktop app is offline, the extension remains locked in a safe state.

  - Official Project & Source Code: https://github.com/hephaestus-developers/secure-vault-manager
  ```

- **Danh mục (Category)**:
  - Chọn: **`Năng suất`** (Productivity) hoặc **`Quyền riêng tư và bảo mật`** (Privacy & Security).

---

#### 2. Tài sản đồ họa (Graphic Assets)

Google yêu cầu tối thiểu:

1. **Biểu tượng ứng dụng (Store Icon - 128x128 px)**:
   - Tải tệp biểu tượng có sẵn tại:

     ```text
     /mnt/storage/hephaestus-studio/secure-vault-manager/extension/public/icon-128.png
     ```

2. **Ảnh chụp màn hình (Screenshots)** *(Bắt buộc tối thiểu 1 ảnh, tối đa 5 ảnh)*:
   - Kích thước chuẩn: **1280x800 px** (PNG, 24-bit sắc nét).
   - Bộ 4 ảnh chụp màn hình chất lượng cao đã được tạo sẵn tại thư mục `docs/screenshots/`:
     1. `docs/screenshots/01_popup_autofill_2fa.png` — *Autofill Logins & Real-time 2FA / TOTP*.
     2. `docs/screenshots/02_inpage_save_prompt.png` — *Smart In-Page Credential & Multi-Vault Saving*.
     3. `docs/screenshots/03_passkey_webauthn_modal.png` — *Native FIDO2 & WebAuthn Passkeys Support*.
     4. `docs/screenshots/04_locked_biometric_screen.png` — *Zero-Knowledge Architecture & Biometric Unlock*.
   - Bạn chỉ cần chọn và tải 4 tệp ảnh này lên giao diện Chrome Web Store.

---

### Bước 3: Thực tiễn về quyền riêng tư (Privacy Practices)

> [!IMPORTANT]
> Đây là phần **quan trọng nhất** khi duyệt tiện ích trên Google Chrome Web Store. Google kiểm tra rất kỹ tính chính đáng của các quyền hạn (Permissions Justification).

Ở menu bên trái, nhấn vào tab **Thực tiễn về quyền riêng tư** (Privacy Practices):

#### 1. Mục đích duy nhất (Single Purpose)

Điền đoạn văn sau:

```text
Secure Vault Manager provides secure autofill of stored logins, one-time verification codes (TOTP), and WebAuthn passkeys by communicating directly with the user's local Secure Vault Manager desktop application.
```

#### 2. Lý do yêu cầu các quyền (Permission Justification)

Google sẽ liệt kê từng quyền khai báo trong `manifest.json`. Hãy sao chép văn bản giải trình tương ứng:

- **`nativeMessaging`**:

  ```text
  Required to establish secure local IPC communication with the companion Secure Vault Manager desktop application (dev.hephaestus.secure_vault_manager) to unlock vaults, retrieve encrypted credentials, and sign passkey assertions.
  ```

- **`activeTab`**:

  ```text
  Used to detect login forms on the currently focused tab and safely autofill credentials or one-time verification codes when triggered by the user.
  ```

- **`tabs`**:

  ```text
  Used to query the active tab's URL to match and display relevant saved credentials for the current domain in the popup, send autofill instructions to content scripts, and open item URLs in new tabs when requested by the user.
  ```

- **`scripting`**:

  ```text
  Required to inject the form autofill engine and passkey credential management bridge into active web pages upon user interaction.
  ```

- **`storage`**:

  ```text
  Used strictly to store local extension preferences, UI settings, and transient session metadata locally within the browser.
  ```

- **`clipboardWrite`**:

  ```text
  Allows users to copy passwords, usernames, or 2FA verification codes to the system clipboard when clicking the copy button in the extension popup.
  ```

- **Quyền truy cập máy chủ / Tất cả URL (`host_permissions: <all_urls>`)**:

  ```text
  Required so the password manager's content script can identify login forms and provide autofill capabilities on any website where the user possesses saved credentials.
  ```

#### 3. Sử dụng dữ liệu (Data Usage)

- Tại câu hỏi về thu thập dữ liệu người dùng:
  - **Tích chọn**: Khẳng định tiện ích **KHÔNG** thu thập hoặc truyền dữ liệu người dùng ra bất kỳ máy chủ bên thứ ba nào (Zero data collected/transmitted externally).
- **Chứng nhận của nhà phát triển (Developer Certifications)**:
  - ✅ Tích chọn: *"Tôi không bán hoặc chuyển giao dữ liệu người dùng cho bên thứ ba..."*
  - ✅ Tích chọn: *"Tôi không sử dụng hoặc chuyển giao dữ liệu người dùng cho các mục đích không liên quan..."*
  - ✅ Tích chọn: *"Tôi không sử dụng hoặc chuyển giao dữ liệu người dùng để xác định mức độ tin cậy tín dụng..."*

#### 4. Chính sách quyền riêng tư (Privacy Policy URL)

- Điền liên kết đến chính sách quyền riêng tư của dự án:

  ```text
  https://github.com/hephaestus-developers/secure-vault-manager/blob/main/docs/PRIVACY_POLICY.md
  ```

  *(Hoặc đường dẫn website chính thức của bạn).*

---

### Bước 4: Phân phối (Distribution)

Ở menu bên trái, nhấn vào tab **Phân phối** (Distribution):

1. **Khả năng hiển thị (Visibility)**:
   - Chọn **Công khai** (Public) để tiện ích hiển thị trên kết quả tìm kiếm Chrome Web Store.
   - *(Hoặc Unlisted nếu bạn chỉ muốn người có link mới cài đặt được).*
2. **Khu vực (Regions)**:
   - Chọn **Tất cả các khu vực** (All regions).
3. **Định giá (Pricing)**:
   - Chọn **Miễn phí** (Free).

---

### Bước 5: Gửi để xem xét (Submit for Review)

1. Kiểm tra lại góc trên bên phải màn hình dashboard: Đảm bảo không còn cảnh báo thiếu sót thông tin (các trường bắt buộc đều đã có dấu tích xanh hoặc hoàn thành).
2. Nhấn nút màu xanh **Gửi để xem xét** (Submit for Review).
3. Một hộp thoại xác nhận sẽ hiện ra, nhấn **Gửi** (Submit).

---

## 3. Thời gian và Lưu ý kiểm duyệt của Google

- **Thời gian xét duyệt**: Thông thường từ **24 giờ đến 3 ngày làm việc** (một số tiện ích dùng quyền `nativeMessaging` hoặc `<all_urls>` có thể mất thêm từ 1 - 2 ngày để đội ngũ bảo mật của Google kiểm tra thủ công).
- **Thông báo kết quả**: Google sẽ gửi email thông báo kết quả duyệt (Approved / Rejected) đến hòm thư tài khoản nhà phát triển của bạn.
- **Sau khi được duyệt**: Tiện ích sẽ chính thức có mặt trên Chrome Web Store với đường dẫn cố định và nhận cập nhật tự động mỗi khi bạn tải lên bản ZIP phiên bản mới.
